#include "test_framework.h"

#include "entities/Alumno.h"
#include "entities/Profesor.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "repositories/IRepositorio.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "repositories/RepositorioAlumnos.h"
#include "repositories/RepositorioCRUD.h"
#include "repositories/RepositorioProfesores.h"
#include "repositories/RepositorioTurnos.h"
#include "structures/ArbolBST.h"
#include "utils/Fecha.h"
#include "utils/Utils.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <type_traits>
#include <vector>

// ---------------------------------------------------------------------------
// Tests de la FASE 5 (bloque 1): la capa de REPOSITORIOS.
//
// Lo que se prueba ACa:
//   - el CRUD completo contra archivos REALES, en un directorio temporal
//   - los errores tipificados: `IdDuplicadoException` y
//     `EntidadNoExisteException`
//   - la busqueda por id de un id que NO existe (devuelve false, NO lanza)
//   - `contar()` exacto y `proximoId()` (vacio y con ids {1, 7, 12})
//   - el round trip comparado CAMPO POR CAMPO
//   - la baja logica: sobrevive al round trip y NO borra la entidad
//   - la UNICIDAD de la matricula y del legajo (comparacion case-insensitive)
//   - el arbol cargado queda BALANCEADO (la decision R6 del issue #14)
//   - el formato del archivo que deja `persistir()`
//   - que el repositorio crea el archivo cuando no existe, en vez de fallar
//   - que el repositorio de TURNOS NO tenga baja logica (verificacion en
//     tiempo de COMPILACION, con `static_assert`)
//
// Los archivos que usan estos tests se escriben en un directorio temporal:
// los tests no tocan los archivos de ejemplo de `data/`.
// ---------------------------------------------------------------------------

namespace {

// ---------------------------------------------------------------------------
// Utilidades de los tests
// ---------------------------------------------------------------------------

// Resultado de capturar una excepcion de un tipo concreto.
struct Fallo {
    bool lanzo = false;
    bool lanzoOtro = false;
    std::string mensaje;
};

// Captura la excepcion `E` de la funcion. `lanzo` dice si lanzo ESA
// exception; `lanzoOtro` dice si lanzo otra cosa (o ninguna: en ese caso
// `mensaje` esta vacio y el test falla solo).
template <typename E, typename F>
Fallo capturarTipo(F funcion) {
    Fallo fallo;
    try {
        funcion();
    } catch (const E& excepcion) {
        fallo.lanzo = true;
        fallo.mensaje = excepcion.what();
    } catch (...) {
        fallo.lanzoOtro = true;
    }
    return fallo;
}

// Directorio temporal donde los tests escriben sus archivos.
std::filesystem::path directorioDePruebas() {
    static const std::filesystem::path directorio = [] {
        const std::filesystem::path base =
            std::filesystem::temp_directory_path() / "sistema_turnos_fase5";
        std::error_code error;
        std::filesystem::remove_all(base, error);
        std::filesystem::create_directories(base, error);
        return base;
    }();
    return directorio;
}

// Ruta de un archivo de prueba.
std::string rutaDePrueba(const std::string& nombre) {
    return (directorioDePruebas() / nombre).string();
}

bool existeArchivo(const std::string& ruta) {
    std::error_code error;
    return std::filesystem::exists(ruta, error);
}

// Lineas del archivo, sin el terminador y sin el '\r' que deja el modo texto
// de Windows (asi la comparacion es igual en Windows y en Linux).
std::vector<std::string> leerLineas(const std::string& ruta) {
    std::ifstream archivo(ruta, std::ios::in);
    std::vector<std::string> lineas;
    std::string linea;
    while (std::getline(archivo, linea)) {
        if (!linea.empty() && linea.back() == '\r') {
            linea.pop_back();
        }
        lineas.push_back(linea);
    }
    return lineas;
}

// ---------------------------------------------------------------------------
// Datos de prueba: entidades validas de cada tipo.
//
// Los NOMBRES no llevan digitos: `Utils::esAlfabetico` (el validador del
// dominio) acepta solo letras y separadores.
// ---------------------------------------------------------------------------

const char* NOMBRES[] = {"Ana",   "Beto",  "Caro",  "Dani",  "Elsa",  "Fede",  "Gina",  "Hugo",
                          "Iva",   "Juli",  "Luna",  "Marcos", "Nina", "Omar",  "Paula", "Quin",
                          "Rita",  "Santi", "Tina",  "Uma"};
const char* APELLIDOS[] = {"Gomez",  "Perez",   "Sosa",   "Vega",   "Zamora", "Alvarez", "Bianchi",
                           "Costa",  "Duarte",  "Esteban", "Ferrer", "Gimenez", "Herrera", "Ibarra",
                           "Juarez", "Ledesma", "Molina", "Nunez",  "Ortiz",   "Paredes"};

std::string nombreDePrueba(int indice) {
    return std::string(NOMBRES[static_cast<std::size_t>(indice) % 20]);
}

std::string apellidoDePrueba(int indice) {
    return std::string(APELLIDOS[static_cast<std::size_t>(indice) % 20]);
}

Alumno alumnoDePrueba(int id) {
    return Alumno(id, nombreDePrueba(id), apellidoDePrueba(id),
                  "alumno" + std::to_string(id) + "@unlp.edu.ar", "2214556677",
                  "MAT-" + std::to_string(20000 + id));
}

Profesor profesorDePrueba(int id) {
    return Profesor(id, nombreDePrueba(id + 3), apellidoDePrueba(id + 7),
                    "profesor" + std::to_string(id) + "@unlp.edu.ar", "2214556677",
                    "LEG-" + std::to_string(1000 + id), "Algoritmos I");
}

Turno turnoDePrueba(int id) {
    return Turno(id, 1 + (id % 5), 1 + (id % 7), Materia::ALG1, Fecha(2026, 10, 8 + (id % 10)),
                 Hora(8 + (id % 10), (id % 2) * 30), 60, "A-" + std::to_string(1 + (id % 4)));
}

// ---------------------------------------------------------------------------
// VERIFICACION EN TIEMPO DE COMPILACION: el repositorio de TURNOS NO tiene
// baja logica.
//
// POR QUE SE HACE CON `static_assert` Y NO CON UN TEST QUE CORRA
//
// La decision R2 del issue #14 dice que un turno NO se "da de baja", se
// cancela, y que la cancelacion la maneja el servicio. La forma de que eso
// SEA VERDAD no es una convencion escrita en un comentario: es que el
// COMPILADOR lo impida.
//
// Este trait es el mismo patron que usa `tests/test_arbol.cpp` con
// `TieneOperadorMenor`: SFINAE para preguntar "existe este miembro" sin
// romper la compilacion cuando la respuesta es NO.
//
// Si alguien le agregara `bajaLogica` al repositorio de turnos, el
// `static_assert` de abajo dejaria de compilar y el error lo diria en el
// momento de compilar, no en el momento de ejecutarlo.
// ---------------------------------------------------------------------------

// Detecta si `T` tiene un metodo `bajaLogica` que se pueda llamar con un
// `int` y devolver `void`.
template <class T, class = void>
struct TieneBajaLogica : std::false_type {};

template <class T>
struct TieneBajaLogica<T, std::void_t<decltype(std::declval<T&>().bajaLogica(0))>>
    : std::true_type {};

// El repositorio de alumnos y el de profesores TIENEN baja logica: los dos
// son personas y las personas tienen el flag `activo`.
static_assert(TieneBajaLogica<RepositorioAlumnos>::value,
              "el repositorio de alumnos tiene que implementar la baja logica");
static_assert(TieneBajaLogica<RepositorioProfesores>::value,
              "el repositorio de profesores tiene que implementar la baja logica");

// Y el de turnos NO: un turno se cancela, no se da de baja. Si este
// `static_assert` falla, se le agrego `bajaLogica` al repositorio de turnos
// y eso viola la decision R2 del issue #14.
static_assert(!TieneBajaLogica<RepositorioTurnos>::value,
              "el repositorio de turnos NO debe tener baja logica: un turno se cancela, "
              "no se da de baja (decision R2 del issue #14)");

// La misma verificacion, pero por el camino de las INTERFACES: el
// repositorio de turnos NO implementa `IRepositorioBajaLogica<Turno>`, y los
// otros dos SI.
static_assert(std::is_base_of<IRepositorioBajaLogica<Alumno>, RepositorioAlumnos>::value,
              "el repositorio de alumnos implementa IRepositorioBajaLogica<Alumno>");
static_assert(std::is_base_of<IRepositorioBajaLogica<Profesor>, RepositorioProfesores>::value,
              "el repositorio de profesores implementa IRepositorioBajaLogica<Profesor>");
static_assert(!std::is_base_of<IRepositorioBajaLogica<Turno>, RepositorioTurnos>::value,
              "el repositorio de turnos NO implementa IRepositorioBajaLogica<Turno>");

// Los tres repositorios implementan el CRUD generico: es el contrato UNICO
// para las tres entidades (decision R1 del issue #14).
static_assert(std::is_base_of<IRepositorio<Alumno>, RepositorioAlumnos>::value,
              "el repositorio de alumnos implementa IRepositorio<Alumno>");
static_assert(std::is_base_of<IRepositorio<Profesor>, RepositorioProfesores>::value,
              "el repositorio de profesores implementa IRepositorio<Profesor>");
static_assert(std::is_base_of<IRepositorio<Turno>, RepositorioTurnos>::value,
              "el repositorio de turnos implementa IRepositorio<Turno>");

} // namespace

// ---------------------------------------------------------------------------
// CRUD: ALTA Y MODIFICAR
// ---------------------------------------------------------------------------

TEST_CASE("el alta de un id que ya existe lanza IdDuplicadoException",
          repositorio_alta_duplicada) {
    const std::string ruta = rutaDePrueba("alta_duplicada.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));

    // La excepcion correcta: `IdDuplicadoException` y no una de otro tipo.
    const Fallo fallo = capturarTipo<IdDuplicadoException>(
        [&] { repo.alta(alumnoDePrueba(1)); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    // El mensaje dice que id se duplico y de que entidad.
    CHECK(fallo.mensaje.find("1") != std::string::npos);
    CHECK(fallo.mensaje.find("ALUMNOS") != std::string::npos);

    // Y el repositorio NO se corrupto: sigue teniendo UNA sola entidad.
    CHECK(repo.contar() == 1);
}

TEST_CASE("el alta de un id libre deja la entidad en el repositorio",
          repositorio_alta_exitosa) {
    const std::string ruta = rutaDePrueba("alta_ok.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(7));

    CHECK(repo.contar() == 1);

    Alumno encontrado;
    CHECK(repo.buscarPorId(7, encontrado));
    CHECK(encontrado.getId() == 7);
    CHECK(encontrado.getMatricula() == "MAT-20007");
}

TEST_CASE("el alta persiste en el archivo", repositorio_alta_persiste) {
    const std::string ruta = rutaDePrueba("alta_persiste.dat");
    std::filesystem::remove(ruta);

    {
        RepositorioAlumnos repo(ruta);
        repo.alta(alumnoDePrueba(1));
    }

    // El archivo existe apenas se hizo el alta: `persistir()` se llama
    // DESPUES de cada mutacion, no solo al salir (decision P6 de la Fase 4).
    CHECK(existeArchivo(ruta));
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);
    CHECK(lineas[0] == "#ALUMNOS|1|1");
}

TEST_CASE("modificar un id que no existe lanza EntidadNoExisteException",
          repositorio_modificar_inexistente) {
    const std::string ruta = rutaDePrueba("modificar_inexistente.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));

    const Fallo fallo = capturarTipo<EntidadNoExisteException>(
        [&] { repo.modificar(alumnoDePrueba(99)); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(fallo.mensaje.find("99") != std::string::npos);

    // Y el repositorio quedo como estaba.
    CHECK(repo.contar() == 1);
}

TEST_CASE("modificar REEMPLAZA la entidad y no agrega una segunda",
          repositorio_modificar_reemplaza) {
    const std::string ruta = rutaDePrueba("modificar_reemplaza.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));

    // Se modifica el MISMO id, cambiando los datos.
    repo.modificar(Alumno(1, "Nuevo", "Nombre", "nuevo@unlp.edu.ar", "2214000000", "MAT-99999"));

    // No hay DOS entidades con el id 1: hay UNA, con los datos nuevos.
    CHECK(repo.contar() == 1);

    Alumno encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(encontrado.getNombre() == "Nuevo");
    CHECK(encontrado.getApellido() == "Nombre");
    CHECK(encontrado.getEmail() == "nuevo@unlp.edu.ar");
    CHECK(encontrado.getMatricula() == "MAT-99999");

    // Y el archivo tambien tiene UN solo registro (no dos).
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);
    CHECK(Utils::separar(lineas[2], '|')[0] == "1");
}

// ---------------------------------------------------------------------------
// LA UNICIDAD DE LA MATRICULA Y DEL LEGAJO
// ---------------------------------------------------------------------------
//
// ESTE es el defecto que cierra este bloque: `entities/Alumno.h` y
// `entities/Profesor.h` dicen que la matricula (y el legajo) son UNICOS y
// que la unicidad la verifica el repositorio, pero esa verificacion nunca
// se implemento. Con esto, el repositorio la verifica.
//
// La comparacion es CASE-INSENSITIVE: "MAT-20001" y "mat-20001" son la
// misma matricula.
//
// Y el error es `DatoInvalidoException`: el UNICO formato de error de dato
// invalido de toda la aplicacion (asi lo exige la capa de UI, que ya sabe
// mostrar el `campo()` y el `valor()` del error).

TEST_CASE("el alta de un alumno con una matricula ya usada lanza DatoInvalidoException",
          repositorio_alta_matricula_duplicada) {
    const std::string ruta = rutaDePrueba("matricula_duplicada.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));  // este tiene "MAT-20001"

    // El segundo alumno quiere la matricula del primero.
    const ErrorCapturado error = capturarError([&] {
        repo.alta(Alumno(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "MAT-20001"));
    });

    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    // El `campo()` es el que la UI usa para ubicar el dato que fallo.
    CHECK(error.campo == "matricula");
    CHECK(error.valor == "MAT-20001");
    // Y el mensaje tiene el formato UNICO de la aplicacion.
    CHECK(error.mensaje == "Dato invalido en campo 'matricula': 'MAT-20001' "
                           "(ya la tiene el alumno #1)");
}

TEST_CASE("el alta con la MISMA matricula en otra capitalizacion tambien lanza",
          repositorio_alta_matricula_duplicada_otro_caso) {
    const std::string ruta = rutaDePrueba("matricula_duplicada_otro_caso.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));  // este tiene "MAT-20001"

    // La misma matricula, en minusculas: si la comparacion no fuera
    // case-insensitive, las dos coexistirian y la unicidad seria un espejismo.
    const ErrorCapturado error = capturarError([&] {
        repo.alta(Alumno(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "mat-20001"));
    });

    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "matricula");
    CHECK(error.valor == "mat-20001");
}

TEST_CASE("el alta con una matricula LIBRE no lanza y deja al alumno en el repositorio",
          repositorio_alta_matricula_libre) {
    const std::string ruta = rutaDePrueba("matricula_libre.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));

    const ErrorCapturado error = capturarError([&] {
        repo.alta(Alumno(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "MAT-99999"));
    });

    CHECK(!error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(repo.contar() == 2);

    Alumno encontrado;
    CHECK(repo.buscarPorId(2, encontrado));
    CHECK(encontrado.getMatricula() == "MAT-99999");
}

TEST_CASE("tras el rechazo por matricula repetida el repositorio queda INTACTO",
          repositorio_alta_matricula_rechazada_no_rompe) {
    const std::string ruta = rutaDePrueba("matricula_rechazo_intacto.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));
    const std::size_t antes = repo.contar();

    const ErrorCapturado error = capturarError([&] {
        repo.alta(Alumno(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "MAT-20001"));
    });
    CHECK(error.lanzo);

    // La entidad rechazada NO quedo insertada: la validacion es ANTES de
    // tocar el arbol.
    CHECK(repo.contar() == antes);
    CHECK(repo.contar() == 1);

    // Y no aparece en la lista.
    bool aparece = false;
    for (const Alumno& alumno : repo.listar()) {
        if (alumno.getId() == 2) {
            aparece = true;
        }
    }
    CHECK(!aparece);

    // El archivo tampoco tiene el registro rechazado (header, esquema y UN
    // registro).
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);

    // Y el alumno que ya estaba sigue intacto.
    Alumno encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(encontrado.getMatricula() == "MAT-20001");
}

TEST_CASE("modificar con la matricula de OTRO alumno lanza DatoInvalidoException",
          repositorio_modificar_matricula_de_otro) {
    const std::string ruta = rutaDePrueba("matricula_modificar_de_otro.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));  // "MAT-20001"
    repo.alta(alumnoDePrueba(2));  // "MAT-20002"

    // El alumno #2 quiere la matricula del #1.
    const ErrorCapturado error = capturarError([&] {
        repo.modificar(Alumno(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "MAT-20001"));
    });

    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "matricula");
    CHECK(error.mensaje == "Dato invalido en campo 'matricula': 'MAT-20001' "
                           "(ya la tiene el alumno #1)");

    // Y el repositorio quedo como estaba: el #2 conserva SU matricula.
    CHECK(repo.contar() == 2);
    Alumno encontrado;
    CHECK(repo.buscarPorId(2, encontrado));
    CHECK(encontrado.getMatricula() == "MAT-20002");
}

TEST_CASE("modificar conservando la matricula PROPIA no lanza",
          repositorio_modificar_matricula_propia) {
    const std::string ruta = rutaDePrueba("matricula_modificar_propia.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(7));  // "MAT-20007"

    // El alumno #7 cambia sus datos pero conserva su matricula: no puede
    // chocar consigo mismo (la exclusion es por `getId()`).
    const ErrorCapturado error = capturarError([&] {
        repo.modificar(Alumno(7, "Nuevo", "Nombre", "nuevo@unlp.edu.ar", "2214000000", "MAT-20007"));
    });

    CHECK(!error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(repo.contar() == 1);

    Alumno encontrado;
    CHECK(repo.buscarPorId(7, encontrado));
    CHECK(encontrado.getMatricula() == "MAT-20007");
    CHECK(encontrado.getNombre() == "Nuevo");
}

TEST_CASE("existeMatricula devuelve true con el id que la tiene, y false con -1",
          repositorio_existe_matricula) {
    const std::string ruta = rutaDePrueba("existe_matricula.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));
    repo.alta(alumnoDePrueba(7));

    // La matricula existe: devuelve true y el id del alumno que la tiene.
    int idExistente = -1;
    CHECK(repo.existeMatricula("MAT-20007", idExistente));
    CHECK(idExistente == 7);

    // Y la encuentra tambien sin distinguir mayusculas.
    idExistente = -1;
    CHECK(repo.existeMatricula("mat-20007", idExistente));
    CHECK(idExistente == 7);

    // Una matricula que no existe: false, y la salida queda en -1 (no en un
    // id de una consulta anterior).
    idExistente = 999;
    CHECK(!repo.existeMatricula("MAT-99999", idExistente));
    CHECK(idExistente == -1);

    // Y la consulta NO modifica nada: es una consulta.
    CHECK(repo.contar() == 2);
}

TEST_CASE("el alta de un profesor con un legajo ya usado lanza DatoInvalidoException",
          repositorio_alta_legajo_duplicado) {
    const std::string ruta = rutaDePrueba("legajo_duplicado.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(profesorDePrueba(1));  // este tiene "LEG-1001"

    // El segundo profesor quiere el legajo del primero.
    const ErrorCapturado error = capturarError([&] {
        repo.alta(Profesor(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "LEG-1001",
                            "Algoritmos I"));
    });

    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "legajo");
    CHECK(error.valor == "LEG-1001");
    CHECK(error.mensaje == "Dato invalido en campo 'legajo': 'LEG-1001' "
                           "(ya lo tiene el profesor #1)");
}

TEST_CASE("el alta con el MISMO legajo en otra capitalizacion tambien lanza",
          repositorio_alta_legajo_duplicado_otro_caso) {
    const std::string ruta = rutaDePrueba("legajo_duplicado_otro_caso.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(profesorDePrueba(1));  // este tiene "LEG-1001"

    const ErrorCapturado error = capturarError([&] {
        repo.alta(Profesor(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "leg-1001",
                            "Algoritmos I"));
    });

    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "legajo");
    CHECK(error.valor == "leg-1001");
}

TEST_CASE("el alta con un legajo LIBRE no lanza y deja al profesor en el repositorio",
          repositorio_alta_legajo_libre) {
    const std::string ruta = rutaDePrueba("legajo_libre.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(profesorDePrueba(1));

    const ErrorCapturado error = capturarError([&] {
        repo.alta(Profesor(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "LEG-9999",
                            "Algoritmos I"));
    });

    CHECK(!error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(repo.contar() == 2);

    Profesor encontrado;
    CHECK(repo.buscarPorId(2, encontrado));
    CHECK(encontrado.getLegajo() == "LEG-9999");
}

TEST_CASE("tras el rechazo por legajo repetido el repositorio queda INTACTO",
          repositorio_alta_legajo_rechazado_no_rompe) {
    const std::string ruta = rutaDePrueba("legajo_rechazo_intacto.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(profesorDePrueba(1));
    const std::size_t antes = repo.contar();

    const ErrorCapturado error = capturarError([&] {
        repo.alta(Profesor(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "LEG-1001",
                            "Algoritmos I"));
    });
    CHECK(error.lanzo);

    CHECK(repo.contar() == antes);
    CHECK(repo.contar() == 1);

    bool aparece = false;
    for (const Profesor& profesor : repo.listar()) {
        if (profesor.getId() == 2) {
            aparece = true;
        }
    }
    CHECK(!aparece);

    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);

    Profesor encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(encontrado.getLegajo() == "LEG-1001");
}

TEST_CASE("modificar con el legajo de OTRO profesor lanza DatoInvalidoException",
          repositorio_modificar_legajo_de_otro) {
    const std::string ruta = rutaDePrueba("legajo_modificar_de_otro.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(profesorDePrueba(1));  // "LEG-1001"
    repo.alta(profesorDePrueba(2));  // "LEG-1002"

    const ErrorCapturado error = capturarError([&] {
        repo.modificar(Profesor(2, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "LEG-1001",
                                "Algoritmos I"));
    });

    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "legajo");
    CHECK(error.mensaje == "Dato invalido en campo 'legajo': 'LEG-1001' "
                           "(ya lo tiene el profesor #1)");

    CHECK(repo.contar() == 2);
    Profesor encontrado;
    CHECK(repo.buscarPorId(2, encontrado));
    CHECK(encontrado.getLegajo() == "LEG-1002");
}

TEST_CASE("modificar conservando el legajo PROPIO no lanza",
          repositorio_modificar_legajo_propio) {
    const std::string ruta = rutaDePrueba("legajo_modificar_propio.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(profesorDePrueba(7));  // "LEG-1007"

    const ErrorCapturado error = capturarError([&] {
        repo.modificar(Profesor(7, "Nuevo", "Nombre", "nuevo@unlp.edu.ar", "2214000000", "LEG-1007",
                                "Base de Datos"));
    });

    CHECK(!error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(repo.contar() == 1);

    Profesor encontrado;
    CHECK(repo.buscarPorId(7, encontrado));
    CHECK(encontrado.getLegajo() == "LEG-1007");
    CHECK(encontrado.getNombre() == "Nuevo");
}

TEST_CASE("existeLegajo devuelve true con el id que lo tiene, y false con -1",
          repositorio_existe_legajo) {
    const std::string ruta = rutaDePrueba("existe_legajo.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(profesorDePrueba(1));
    repo.alta(profesorDePrueba(7));

    int idExistente = -1;
    CHECK(repo.existeLegajo("LEG-1007", idExistente));
    CHECK(idExistente == 7);

    // Y la encuentra tambien sin distinguir mayusculas.
    idExistente = -1;
    CHECK(repo.existeLegajo("leg-1007", idExistente));
    CHECK(idExistente == 7);

    // Un legajo que no existe: false, y la salida queda en -1.
    idExistente = 999;
    CHECK(!repo.existeLegajo("LEG-9999", idExistente));
    CHECK(idExistente == -1);

    // Y la consulta NO modifica nada.
    CHECK(repo.contar() == 2);
}

// ---------------------------------------------------------------------------
// BUSQUEDA POR ID
// ---------------------------------------------------------------------------

TEST_CASE("buscarPorId de un id que no existe devuelve false y no lanza",
          repositorio_busqueda_inexistente) {
    const std::string ruta = rutaDePrueba("busqueda_inexistente.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));

    Alumno encontrado;
    // La ausencia es un DATO de la consulta, no un error: no lanza nada.
    const Fallo fallo = capturarTipo<ExcepcionSistema>(
        [&] { CHECK(!repo.buscarPorId(99, encontrado)); });
    CHECK(!fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
}

TEST_CASE("buscarPorId de un id que no existe en un repositorio vacio devuelve false",
          repositorio_busqueda_vacia) {
    const std::string ruta = rutaDePrueba("busqueda_vacia.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.cargar();

    Alumno encontrado;
    CHECK(!repo.buscarPorId(1, encontrado));
    CHECK(repo.contar() == 0);
}

TEST_CASE("buscarPorId encuentra el id en un repositorio con muchos ids",
          repositorio_busqueda_exitosa) {
    const std::string ruta = rutaDePrueba("busqueda_ok.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    for (int id = 1; id <= 50; ++id) {
        repo.alta(alumnoDePrueba(id));
    }

    // El id buscado esta en el MEDIO del arbol: si la busqueda por id
    // estuviera rota (por ejemplo, si usara `ArbolBST::buscar()` con una
    // sonda mal construida), este seria el caso que falla.
    Alumno encontrado;
    CHECK(repo.buscarPorId(25, encontrado));
    CHECK(encontrado.getId() == 25);
    CHECK(encontrado.getMatricula() == "MAT-20025");

    // Y el ultimo, que esta en una hoja del arbol balanceado.
    CHECK(repo.buscarPorId(50, encontrado));
    CHECK(encontrado.getId() == 50);
}

// ---------------------------------------------------------------------------
// CONTAR Y PROXIMO ID
// ---------------------------------------------------------------------------

TEST_CASE("contar devuelve la cantidad exacta", repositorio_contar_exacto) {
    const std::string ruta = rutaDePrueba("contar.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    CHECK(repo.contar() == 0);

    for (int id = 1; id <= 7; ++id) {
        repo.alta(alumnoDePrueba(id));
        CHECK(repo.contar() == static_cast<std::size_t>(id));
    }

    // Y despues de cargar, la cuenta sigue siendo la misma.
    repo.cargar();
    CHECK(repo.contar() == 7);
}

TEST_CASE("proximoId sobre un repositorio vacio devuelve 1", repositorio_proximo_id_vacio) {
    const std::string ruta = rutaDePrueba("proximo_vacio.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.cargar();
    CHECK(repo.proximoId() == 1);
}

TEST_CASE("proximoId sobre los ids {1, 7, 12} devuelve 13", repositorio_proximo_id_con_ids) {
    const std::string ruta = rutaDePrueba("proximo_ids.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));
    repo.alta(alumnoDePrueba(7));
    repo.alta(alumnoDePrueba(12));

    // El id propuesto es el MAXIMO + 1, no la cantidad de entidades: con los
    // ids {1, 7, 12} hay 3 entidades, pero el siguiente libre es el 13.
    CHECK(repo.proximoId() == 13);
}

// ---------------------------------------------------------------------------
// ROUND TRIP
// ---------------------------------------------------------------------------

TEST_CASE("el round trip del alumno conserva todos los campos",
          repositorio_round_trip_alumno) {
    const std::string ruta = rutaDePrueba("round_trip_alumno.dat");
    std::filesystem::remove(ruta);

    std::vector<Alumno> originales;
    for (int id = 1; id <= 100; ++id) {
        originales.push_back(alumnoDePrueba(id));
    }

    {
        RepositorioAlumnos repo(ruta);
        for (const Alumno& alumno : originales) {
            repo.alta(alumno);
        }
    }

    RepositorioAlumnos repo(ruta);
    repo.cargar();
    CHECK(repo.contar() == 100);

    const std::vector<Alumno> cargados = repo.listar();
    CHECK(cargados.size() == 100);

    // Campo por campo: `operator==` compararia solo el id y el test pasaria
    // aunque se perdiera el nombre.
    for (std::size_t i = 0; i < cargados.size() && i < originales.size(); ++i) {
        CHECK(cargados[i].getId() == originales[i].getId());
        CHECK(cargados[i].getNombre() == originales[i].getNombre());
        CHECK(cargados[i].getApellido() == originales[i].getApellido());
        CHECK(cargados[i].getEmail() == originales[i].getEmail());
        CHECK(cargados[i].getTelefono() == originales[i].getTelefono());
        CHECK(cargados[i].esActivo() == originales[i].esActivo());
        CHECK(cargados[i].getMatricula() == originales[i].getMatricula());
    }
}

TEST_CASE("el round trip del profesor conserva todos los campos",
          repositorio_round_trip_profesor) {
    const std::string ruta = rutaDePrueba("round_trip_profesor.dat");
    std::filesystem::remove(ruta);

    std::vector<Profesor> originales;
    for (int id = 1; id <= 100; ++id) {
        originales.push_back(profesorDePrueba(id));
    }

    {
        RepositorioProfesores repo(ruta);
        for (const Profesor& profesor : originales) {
            repo.alta(profesor);
        }
    }

    RepositorioProfesores repo(ruta);
    repo.cargar();
    CHECK(repo.contar() == 100);

    const std::vector<Profesor> cargados = repo.listar();
    CHECK(cargados.size() == 100);

    for (std::size_t i = 0; i < cargados.size() && i < originales.size(); ++i) {
        CHECK(cargados[i].getId() == originales[i].getId());
        CHECK(cargados[i].getNombre() == originales[i].getNombre());
        CHECK(cargados[i].getApellido() == originales[i].getApellido());
        CHECK(cargados[i].getEmail() == originales[i].getEmail());
        CHECK(cargados[i].getTelefono() == originales[i].getTelefono());
        CHECK(cargados[i].esActivo() == originales[i].esActivo());
        CHECK(cargados[i].getLegajo() == originales[i].getLegajo());
        CHECK(cargados[i].getEspecialidad() == originales[i].getEspecialidad());
    }
}

TEST_CASE("el round trip del turno conserva todos los campos",
          repositorio_round_trip_turno) {
    const std::string ruta = rutaDePrueba("round_trip_turno.dat");
    std::filesystem::remove(ruta);

    std::vector<Turno> originales;
    for (int id = 1; id <= 100; ++id) {
        originales.push_back(turnoDePrueba(id));
    }

    {
        RepositorioTurnos repo(ruta);
        for (const Turno& turno : originales) {
            repo.alta(turno);
        }
    }

    RepositorioTurnos repo(ruta);
    repo.cargar();
    CHECK(repo.contar() == 100);

    const std::vector<Turno> cargados = repo.listar();
    CHECK(cargados.size() == 100);

    // Campo por campo: el `operator==` del turno compara SOLO el id, asi que
    // con `==` el test pasaria aunque se perdiera la fecha.
    for (std::size_t i = 0; i < cargados.size() && i < originales.size(); ++i) {
        CHECK(cargados[i].getId() == originales[i].getId());
        CHECK(cargados[i].getIdProfesor() == originales[i].getIdProfesor());
        CHECK(cargados[i].getIdAlumno() == originales[i].getIdAlumno());
        CHECK(cargados[i].getMateria() == originales[i].getMateria());
        CHECK(cargados[i].getFecha() == originales[i].getFecha());
        CHECK(cargados[i].getHora() == originales[i].getHora());
        CHECK(cargados[i].getDuracionMin() == originales[i].getDuracionMin());
        CHECK(cargados[i].getAula() == originales[i].getAula());
        CHECK(cargados[i].getEstado() == originales[i].getEstado());
        CHECK(cargados[i].getIdTurnoReemplazante() == originales[i].getIdTurnoReemplazante());
    }
}

// ---------------------------------------------------------------------------
// EL ARBOL CARGADO QUEDA BALANCEADO (decision R6 del issue #14)
// ---------------------------------------------------------------------------

TEST_CASE("el arbol cargado queda balanceado", repositorio_arbol_balanceado) {
    const std::string ruta = rutaDePrueba("balanceado.dat");
    std::filesystem::remove(ruta);

    {
        RepositorioAlumnos repo(ruta);
        for (int id = 1; id <= 100; ++id) {
            repo.alta(alumnoDePrueba(id));
        }
    }

    RepositorioAlumnos repo(ruta);
    repo.cargar();

    // ESTE es el test que verifica la decision R6: al cargar se usa
    // `construirDesdeOrdenados()` y NO `insertar()` en un loop.
    //
    // Con `insertar()` en un loop y ids secuenciales, el arbol degenera en
    // una cadena y la altura seria 100. Con `construirDesdeOrdenados()`
    // (O(n), divide y conqueras) la altura es ~log2(100) = 7, y el margen
    // de 11 deja lugar para el desbalanceo natural de un arbol construido
    // por mitades.
    CHECK(repo.arbol().size() == 100);
    CHECK(repo.arbol().altura() <= 11);
}

TEST_CASE("el arbol cargado con ids espaciados queda balanceado",
          repositorio_arbol_balanceado_espaciado) {
    const std::string ruta = rutaDePrueba("balanceado_espaciado.dat");
    std::filesystem::remove(ruta);

    {
        RepositorioAlumnos repo(ruta);
        for (int id = 1; id <= 200; id += 2) {
            repo.alta(alumnoDePrueba(id));
        }
    }

    RepositorioAlumnos repo(ruta);
    repo.cargar();
    CHECK(repo.arbol().size() == 100);
    CHECK(repo.arbol().altura() <= 11);
}

// ---------------------------------------------------------------------------
// EL ARCHIVO QUE DEJA `persistir()`
// ---------------------------------------------------------------------------

TEST_CASE("persistir deja el archivo con el header, el esquema y los registros ordenados por id",
          repositorio_formato_del_archivo) {
    const std::string ruta = rutaDePrueba("formato.dat");
    std::filesystem::remove(ruta);

    {
        RepositorioAlumnos repo(ruta);
        // Se dan de alta en ORDEN DESCENDENTE: el archivo tiene que salir
        // ordenado por id igual, porque lo que se escribe es el `inOrder()`
        // del arbol, no el orden en que se dieron de alta.
        repo.alta(alumnoDePrueba(5));
        repo.alta(alumnoDePrueba(1));
        repo.alta(alumnoDePrueba(9));
        repo.alta(alumnoDePrueba(3));
    }

    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 6);
    // La linea 1 es el header y la 2 el esquema.
    CHECK(lineas[0] == "#ALUMNOS|1|4");
    CHECK(lineas[1] == Alumno::esquema());

    // Y los registros salen ordenados por id.
    int idAnterior = 0;
    for (std::size_t i = 2; i < lineas.size(); ++i) {
        const int id = std::stoi(Utils::separar(lineas[i], '|')[0]);
        CHECK(id > idAnterior);
        idAnterior = id;
    }
    CHECK(Utils::separar(lineas[2], '|')[0] == "1");
    CHECK(Utils::separar(lineas[5], '|')[0] == "9");
}

TEST_CASE("persistir deja el archivo del turno con su propio nombre de entidad",
          repositorio_formato_del_archivo_turnos) {
    const std::string ruta = rutaDePrueba("formato_turnos.dat");
    std::filesystem::remove(ruta);

    {
        RepositorioTurnos repo(ruta);
        repo.alta(turnoDePrueba(1));
        repo.alta(turnoDePrueba(2));
    }

    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 4);
    CHECK(lineas[0] == "#TURNOS|1|2");
    CHECK(lineas[1] == Turno::esquema());
}

// ---------------------------------------------------------------------------
// EL REPOSITORIO CREA EL ARCHIVO QUE NO EXISTE
// ---------------------------------------------------------------------------

TEST_CASE("cargar un archivo que no existe lo crea vacio y no falla",
          repositorio_crea_archivo) {
    const std::string ruta = rutaDePrueba("no_existe.dat");
    std::filesystem::remove(ruta);
    CHECK(!existeArchivo(ruta));

    RepositorioAlumnos repo(ruta);

    // NO lanza: la aplicacion no puede romperse al arrancar por un archivo
    // que todavia no esta (es el mismo criterio que el caso "abc" de la
    // Fase 1).
    const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { repo.cargar(); });
    CHECK(!fallo.lanzo);
    CHECK(!fallo.lanzoOtro);

    // El archivo se creo, con el header y el esquema y CERO registros.
    CHECK(existeArchivo(ruta));
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 2);
    CHECK(lineas[0] == "#ALUMNOS|1|0");
    CHECK(lineas[1] == Alumno::esquema());

    // Y el repositorio quedo vacio, listo para usarse.
    CHECK(repo.contar() == 0);
    CHECK(repo.proximoId() == 1);
}

TEST_CASE("cargar un archivo que no existe no borra uno que ya existe",
          repositorio_crea_archivo_no_pisa) {
    const std::string ruta = rutaDePrueba("no_pisa.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));

    // Se carga de nuevo: el archivo existe, asi que se leen sus datos.
    repo.cargar();
    CHECK(repo.contar() == 1);
}

// ---------------------------------------------------------------------------
// LA BAJA LOGICA
// ---------------------------------------------------------------------------

TEST_CASE("la baja logica del alumno no lo borra y deja activo = false",
          repositorio_baja_logica_alumno) {
    const std::string ruta = rutaDePrueba("baja_alumno.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));
    repo.alta(alumnoDePrueba(2));

    repo.bajaLogica(1);

    // La entidad SIGUE en el repositorio: la baja es LOGICA, no fisica.
    CHECK(repo.contar() == 2);

    Alumno encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(!encontrado.esActivo());

    // Y la otra no se toco.
    CHECK(repo.buscarPorId(2, encontrado));
    CHECK(encontrado.esActivo());
}

TEST_CASE("la baja logica del alumno sobrevive al round trip",
          repositorio_baja_logica_round_trip) {
    const std::string ruta = rutaDePrueba("baja_alumno_round_trip.dat");
    std::filesystem::remove(ruta);

    {
        RepositorioAlumnos repo(ruta);
        repo.alta(alumnoDePrueba(1));
        repo.bajaLogica(1);
    }

    RepositorioAlumnos repo(ruta);
    repo.cargar();

    // La entidad vuelve, y vuelve INACTIVA: el `activo = 0` del archivo se
    // lee como `activo = false`.
    CHECK(repo.contar() == 1);
    Alumno encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(!encontrado.esActivo());

    // Y en el archivo el campo `activo` vale "0", no "false".
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);
    CHECK(Utils::separar(lineas[2], '|')[5] == "0");
}

TEST_CASE("reactivar el alumno lo vuelve a activar", repositorio_reactivar_alumno) {
    const std::string ruta = rutaDePrueba("reactivar_alumno.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));
    repo.bajaLogica(1);

    repo.reactivar(1);

    // La entidad sigue en el repositorio (la reactivacion tampoco borra).
    CHECK(repo.contar() == 1);
    Alumno encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(encontrado.esActivo());
}

TEST_CASE("la baja logica del profesor no lo borra y deja activo = false",
          repositorio_baja_logica_profesor) {
    const std::string ruta = rutaDePrueba("baja_profesor.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(profesorDePrueba(1));

    repo.bajaLogica(1);

    CHECK(repo.contar() == 1);

    Profesor encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(!encontrado.esActivo());
}

TEST_CASE("la baja logica del profesor sobrevive al round trip y se puede reactivar",
          repositorio_baja_logica_profesor_round_trip) {
    const std::string ruta = rutaDePrueba("baja_profesor_round_trip.dat");
    std::filesystem::remove(ruta);

    {
        RepositorioProfesores repo(ruta);
        repo.alta(profesorDePrueba(1));
        repo.bajaLogica(1);
    }

    RepositorioProfesores repo(ruta);
    repo.cargar();

    CHECK(repo.contar() == 1);
    Profesor encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(!encontrado.esActivo());

    // Y se puede reactivar: el flag vuelve a `true` y se persiste.
    repo.reactivar(1);
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(encontrado.esActivo());

    RepositorioProfesores repo2(ruta);
    repo2.cargar();
    CHECK(repo2.buscarPorId(1, encontrado));
    CHECK(encontrado.esActivo());
}

TEST_CASE("la baja logica de un id que no existe lanza EntidadNoExisteException",
          repositorio_baja_logica_inexistente) {
    const std::string ruta = rutaDePrueba("baja_inexistente.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));

    const Fallo fallo = capturarTipo<EntidadNoExisteException>(
        [&] { repo.bajaLogica(99); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(fallo.mensaje.find("99") != std::string::npos);

    // Lo mismo con `reactivar`.
    const Fallo otro = capturarTipo<EntidadNoExisteException>(
        [&] { repo.reactivar(99); });
    CHECK(otro.lanzo);
    CHECK(!otro.lanzoOtro);

    // Y el repositorio quedo como estaba.
    CHECK(repo.contar() == 1);
}

// ---------------------------------------------------------------------------
// EL REPOSITORIO DE TURNOS NO TIENE BAJA LOGICA
// ---------------------------------------------------------------------------

TEST_CASE("el repositorio de turnos no tiene baja logica",
          repositorio_turnos_sin_baja_logica) {
    // La comprobacion FUERTE es el `static_assert` de arriba (que dice que
    // `RepositorioTurnos` NO tiene el metodo `bajaLogica`): si alguien se lo
    // agrega, el codigo deja de compilar.
    //
    // Aca se mide lo que se puede medir en EJECUCION: el repositorio de
    // turnos funciona como un CRUD comun y su estado es el de `Turno`
    // (`EstadoTurno`), no un flag `activo`.
    const std::string ruta = rutaDePrueba("turnos_sin_baja.dat");
    std::filesystem::remove(ruta);

    RepositorioTurnos repo(ruta);
    repo.alta(turnoDePrueba(1));

    CHECK(repo.contar() == 1);

    Turno encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    // El estado del turno es `Activo`, no "activo = true".
    CHECK(encontrado.getEstado() == EstadoTurno::Activo);

    // Y el repositorio de turnos NO es un `IRepositorioBajaLogica<Turno>`:
    // no se lo puede usar como tal.
    //
    // El tipo se declara afuera del `CHECK` porque la coma de la lista de
    // argumentos de la plantilla se interpretaria como un separador de
    // argumentos de la macro.
    using BaseDeBajaDeTurnos = std::is_base_of<IRepositorioBajaLogica<Turno>, RepositorioTurnos>;
    CHECK(!BaseDeBajaDeTurnos::value);
}

// ---------------------------------------------------------------------------
// LA BUSQUEDA POR ID ES O(log n) (la promesa del roadmap de la Fase 3)
// ---------------------------------------------------------------------------
//
// POR QUE ESTOS TESTS MIDEN EL NUMERO DE COMPARACIONES
//
// Un test de TIEMPO seria fragil (depende de la maquina) y no probaria
// nada. Un test que solo dice "encuentra la entidad" tampoco: pasaria
// igual con una implementacion O(n).
//
// Lo que se mide aqui es el NUMERO DE COMPARACIONES que hace la busqueda:
// es la MEDIDA DIRECTA de la complejidad. Un recorrido del arbol entero
// hace 1000 comparaciones con 1000 entidades; un descenso por un arbol
// balanceado hace ~11.
//
// Los tests de abajo fallarian CONTRA la implementacion anterior (que
// recorria el arbol en orden y que ademas dejaba el arbol en memoria
// degenerado en una cadena).

TEST_CASE("el arbol en memoria queda balanceado despues de 1000 altas",
          repositorio_arbol_en_memoria_balanceado) {
    // ESTE es el test que fija el segundo defecto: el arbol del
    // repositorio tiene que estar balanceado NO SOLO despues de cargar
    // desde el disco, sino tambien despues de las altas.
    //
    // Con `insertar()` en un loop y ids secuenciales (1, 2, 3...) el arbol
    // degenera en una CADENA: la altura era 1000. Con el arbol balanceado
    // la altura es ~log2(1000) = 10.
    const std::string ruta = rutaDePrueba("balanceado_en_memoria.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    for (int id = 1; id <= 1000; ++id) {
        repo.alta(alumnoDePrueba(id));
    }

    CHECK(repo.contar() == 1000);
    // La altura balanceada de 1000 nodos es 10; el margen de 12 deja lugar
    // para el desbalanceo natural de un arbol construido por mitades.
    CHECK(repo.arbol().altura() <= 12);
    // Y el margen inferior acota por el otro lado: un arbol REALMENTE
    // balanceado no puede ser mas bajo que log2(1000) - 1 = 9.
    CHECK(repo.arbol().altura() >= 9);
    // El contraste explicito: la altura NO puede ser la de una cadena.
    // Con 1000 nodos, una cadena daria 1000; el arbol balanceado da 10.
    CHECK(repo.arbol().altura() * 100 <= 1000);
}

TEST_CASE("el arbol en memoria sigue balanceado despues de 1000 modificaciones",
          repositorio_arbol_tras_modificar_balanceado) {
    // `modificar()` borra y reinserta: en un BST plano eso puede dejar el
    // arbol todavia mas desbalanceado. El repositorio lo rebalancea, asi que
    // la altura se mantiene en la del arbol balanceado.
    const std::string ruta = rutaDePrueba("balanceado_modificar.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    for (int id = 1; id <= 1000; ++id) {
        repo.alta(alumnoDePrueba(id));
    }

    // Se modifica el primero: es el que cambia la forma del arbol (en un
    // BST plano, sacar la raiz de una cadena la deja desbalanceada).
    repo.modificar(Alumno(1, "Nuevo", "Nombre", "nuevo@unlp.edu.ar", "2214000000", "MAT-99999"));

    CHECK(repo.contar() == 1000);
    CHECK(repo.arbol().altura() <= 12);

    // Y se modifica uno del medio, que es la raiz del arbol balanceado.
    repo.modificar(Alumno(500, "Otro", "Nombre", "otro@unlp.edu.ar", "2214000000", "MAT-88888"));

    CHECK(repo.contar() == 1000);
    CHECK(repo.arbol().altura() <= 12);

    // La entidad modificada sigue siendo la correcta y no se duplico.
    Alumno encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(encontrado.getNombre() == "Nuevo");
    CHECK(repo.buscarPorId(500, encontrado));
    CHECK(encontrado.getNombre() == "Otro");
}

TEST_CASE("la busqueda por id hace O(log n) comparaciones: se cuentan, no se miden en el reloj",
          repositorio_busqueda_por_id_es_log_n) {
    // ESTE es el test que fija el PRIMER defecto.
    //
    // El comparador de `buscarPorClave()` CUENTA sus invocaciones. La
    // busqueda del repositorio es un DESCENSO por el arbol: visita como
    // maximo un nodo por nivel, asi que hace como maximo `altura()`
    // comparaciones. Un recorrido del arbol entero haria `n` comparaciones.
    //
    // La cuenta se hace sobre el ARBOL DEL REPOSITORIO, construido igual que
    // lo construye el repositorio (mismo `construirDesdeOrdenados()`), y
    // con el MISMO comparador por id que usa el repositorio. Asi el test
    // mide la complejidad REAL de la busqueda del repositorio.
    const int cantidad = 1000;

    std::vector<Alumno> ordenados;
    for (int id = 1; id <= cantidad; ++id) {
        ordenados.push_back(alumnoDePrueba(id));
    }

    // El arbol del repositorio: balanceado, con la misma altura que el que
    // el repositorio mantiene en memoria.
    const std::string ruta = rutaDePrueba("log_n.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    for (int id = 1; id <= cantidad; ++id) {
        repo.alta(alumnoDePrueba(id));
    }

    ArbolBST<Alumno> arbol;
    arbol.construirDesdeOrdenados(ordenados);

    // El comparador por id, IGUAL al del repositorio, pero contando.
    struct ComparadorContador {
        int* contador;
        int operator()(int clave, const Alumno& valor) const {
            ++(*contador);
            if (clave < valor.getId()) {
                return -1;
            }
            if (clave > valor.getId()) {
                return 1;
            }
            return 0;
        }
    };

    Alumno fuera;
    int comparaciones = 0;
    CHECK(arbol.buscarPorClave(777, ComparadorContador{&comparaciones}, fuera));
    CHECK(fuera.getId() == 777);

    // Un descenso por el arbol: como maximo un nodo por nivel.
    CHECK(comparaciones <= arbol.altura());
    // Y la cuenta NO crece linealmente con la cantidad de entidades.
    CHECK(comparaciones < 20);
    CHECK(comparaciones < cantidad / 10);

    // La altura del arbol del REPOSITORIO es la misma: por eso la busqueda
    // del repositorio tambien es O(log n).
    CHECK(repo.arbol().altura() == arbol.altura());
}

TEST_CASE("la cuenta de comparaciones no crece con la cantidad de entidades",
          repositorio_busqueda_comparaciones_no_crecen) {
    // El mismo tree con 100 entidades y con 1000: la busqueda hace
    // COMPARACIONES DE LA MISMA MAGNITUD. Si la busqueda fuera O(n), la
    // cuenta creeria con el tamano.
    //
    // Se mide sobre el arbol del repositorio: el que se usa es el mismo
    // para las dos medidas.
    const std::string ruta = rutaDePrueba("comparaciones_no_crecen.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    for (int id = 1; id <= 100; ++id) {
        repo.alta(alumnoDePrueba(id));
    }

    struct ComparadorContador {
        int* contador;
        int operator()(int clave, const Alumno& valor) const {
            ++(*contador);
            if (clave < valor.getId()) {
                return -1;
            }
            if (clave > valor.getId()) {
                return 1;
            }
            return 0;
        }
    };

    // Con 100 entidades.
    std::vector<Alumno> cien;
    for (int id = 1; id <= 100; ++id) {
        cien.push_back(alumnoDePrueba(id));
    }
    ArbolBST<Alumno> arbolCien;
    arbolCien.construirDesdeOrdenados(cien);

    Alumno fuera;
    int comparacionesCien = 0;
    CHECK(arbolCien.buscarPorClave(77, ComparadorContador{&comparacionesCien}, fuera));

    // Con 1000 entidades.
    std::vector<Alumno> mil;
    for (int id = 1; id <= 1000; ++id) {
        mil.push_back(alumnoDePrueba(id));
    }
    ArbolBST<Alumno> arbolMil;
    arbolMil.construirDesdeOrdenados(mil);

    int comparacionesMil = 0;
    CHECK(arbolMil.buscarPorClave(777, ComparadorContador{&comparacionesMil}, fuera));

    // Diez veces mas entidades NO pueden significar diez veces mas
    // comparaciones: las dos cuentas son de la misma magnitud (log2).
    CHECK(comparacionesMil <= comparacionesCien + 4);
    CHECK(comparacionesMil < 20);
    CHECK(comparacionesCien < 20);
}

TEST_CASE("la busqueda por id del repositorio no recorre el arbol: cuenta comparaciones",
          repositorio_busqueda_no_recorre) {
    // El repositorio expone su arbol (para los tests de balanceo), asi que
    // el test puede buscar por id sobre ESE arbol con el comparador por id
    // y contar las comparaciones. Es la MISMA busqueda que hace
    // `buscarPorId()`: mismo arbol, mismo comparador, mismo descenso.
    //
    // Contra la implementacion anterior (un recorrido en orden con corte
    // temprano) el recuento habria sido O(n), no O(log n).
    const std::string ruta = rutaDePrueba("no_recorre.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    for (int id = 1; id <= 1000; ++id) {
        repo.alta(alumnoDePrueba(id));
    }

    struct ComparadorContador {
        int* contador;
        int operator()(int clave, const Alumno& valor) const {
            ++(*contador);
            if (clave < valor.getId()) {
                return -1;
            }
            if (clave > valor.getId()) {
                return 1;
            }
            return 0;
        }
    };

    // La busqueda de un id que NO existe tambien tiene que ser O(log n):
    // el descenso termina en una hoja, sin recorrer el arbol.
    Alumno fuera;
    int comparaciones = 0;
    CHECK(!repo.arbol().buscarPorClave(5000, ComparadorContador{&comparaciones}, fuera));
    CHECK(comparaciones <= repo.arbol().altura());
    CHECK(comparaciones < 20);
}

TEST_CASE("la busqueda por id sigue funcionando en los tres repositorios",
          repositorio_busqueda_por_id_los_tres) {
    // El comparador del repositorio usa `getId()`, que existe en las TRES
    // entidades. En particular `Turno` NO tiene `getTipo()` (no es una
    // `Persona`): si el comparador dependiera del tipo, el repositorio de
    // turnos no compilaria.
    const std::string rutaAlumnos = rutaDePrueba("tres_alumnos.dat");
    const std::string rutaProfesores = rutaDePrueba("tres_profesores.dat");
    const std::string rutaTurnos = rutaDePrueba("tres_turnos.dat");
    std::filesystem::remove(rutaAlumnos);
    std::filesystem::remove(rutaProfesores);
    std::filesystem::remove(rutaTurnos);

    RepositorioAlumnos repoAlumnos(rutaAlumnos);
    RepositorioProfesores repoProfesores(rutaProfesores);
    RepositorioTurnos repoTurnos(rutaTurnos);

    for (int id = 1; id <= 200; ++id) {
        repoAlumnos.alta(alumnoDePrueba(id));
        repoProfesores.alta(profesorDePrueba(id));
        repoTurnos.alta(turnoDePrueba(id));
    }

    // Los tres arboles quedan balanceados.
    CHECK(repoAlumnos.arbol().altura() <= 12);
    CHECK(repoProfesores.arbol().altura() <= 12);
    CHECK(repoTurnos.arbol().altura() <= 12);

    // Y los tres encuentran por id, tanto un id presente como uno ausente.
    Alumno alumno;
    CHECK(repoAlumnos.buscarPorId(150, alumno));
    CHECK(alumno.getId() == 150);
    CHECK(!repoAlumnos.buscarPorId(500, alumno));

    Profesor profesor;
    CHECK(repoProfesores.buscarPorId(150, profesor));
    CHECK(profesor.getId() == 150);
    CHECK(!repoProfesores.buscarPorId(500, profesor));

    Turno turno;
    CHECK(repoTurnos.buscarPorId(150, turno));
    CHECK(turno.getId() == 150);
    CHECK(!repoTurnos.buscarPorId(500, turno));
}

TEST_CASE("la busqueda por id devuelve la entidad COMPLETA, no solo el id",
          repositorio_busqueda_devuelve_la_entidad) {
    // La busca por clave no puede devolver un "id": devuelve el VALOR
    // entero que esta en el nodo. El test comprueba que el valor es la
    // entidad completa (con todos sus campos), no solo una coincidencia.
    const std::string ruta = rutaDePrueba("entidad_completa.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(7));

    Alumno encontrado;
    CHECK(repo.buscarPorId(7, encontrado));
    CHECK(encontrado.getId() == 7);
    CHECK(encontrado.getNombre() == nombreDePrueba(7));
    CHECK(encontrado.getApellido() == apellidoDePrueba(7));
    CHECK(encontrado.getEmail() == "alumno7@unlp.edu.ar");
    CHECK(encontrado.getTelefono() == "2214556677");
    CHECK(encontrado.getMatricula() == "MAT-20007");
    CHECK(encontrado.esActivo());
}

TEST_CASE("la busqueda por id de un id ausente NO modifica la variable de salida",
          repositorio_busqueda_no_toca_la_salida) {
    // La ausencia es un dato de la consulta: devuelve false y deja `fuera`
    // como estaba. El `buscarPorClave()` del arbol tiene esa misma regla.
    const std::string ruta = rutaDePrueba("no_toca_salida.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));

    // Se busca primero una entidad QUE EXISTE, para dejar `fuera` con un
    // valor reconocible.
    Alumno encontrado;
    CHECK(repo.buscarPorId(1, encontrado));
    CHECK(encontrado.getId() == 1);

    // Ahora se busca una que NO existe: `fuera` tiene que seguir con la
    // entidad anterior, no vacia ni modificada.
    CHECK(!repo.buscarPorId(999, encontrado));
    CHECK(encontrado.getId() == 1);
    CHECK(encontrado.getMatricula() == "MAT-20001");
}

TEST_CASE("despues de las altas y las modificaciones el archivo sigue ordenado por id",
          repositorio_archivo_ordenado_despues_de_mutar) {
    // El rebalanceo NO puede romper el orden del archivo: `persistir()`
    // escribe el `inOrder()` del arbol, y ese recorrido tiene que salir
    // ordenado por id.
    const std::string ruta = rutaDePrueba("ordenado_tras_mutar.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    // Altas en orden DESCENDENTE: el archivo tiene que salir ordenado igual.
    for (int id = 20; id >= 1; --id) {
        repo.alta(alumnoDePrueba(id));
    }
    // Y una modificacion en el medio.
    repo.modificar(Alumno(10, "Cambio", "Nombre", "cambio@unlp.edu.ar", "2214000000", "MAT-77777"));

    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 22);
    int idAnterior = 0;
    for (std::size_t i = 2; i < lineas.size(); ++i) {
        const int id = std::stoi(Utils::separar(lineas[i], '|')[0]);
        CHECK(id > idAnterior);
        idAnterior = id;
    }
    CHECK(Utils::separar(lineas[2], '|')[0] == "1");
    CHECK(Utils::separar(lineas[21], '|')[0] == "20");

    // Y `listar()` tambien sale ordenado por id.
    const std::vector<Alumno> listados = repo.listar();
    CHECK(listados.size() == 20);
    for (std::size_t i = 1; i < listados.size(); ++i) {
        CHECK(listados[i - 1].getId() < listados[i].getId());
    }
}

// ---------------------------------------------------------------------------
// LA BUSQUEDA POR NOMBRE
//
// La consulta que usan las pantallas de Consultas para encontrar a una
// persona por su nombre o por su apellido.
//
// LAS TRES REGLAS
//   - la coincidencia es PARCIAL: el texto buscado aparece DENTRO del nombre
//     o del apellido;
//   - es INSENSIBLE A MAYUSCULAS;
//   - un texto VACIO no encuentra nada.
// ---------------------------------------------------------------------------

TEST_CASE("buscarPorNombre encuentra por coincidencia parcial, sin distinguir mayusculas",
          repositorio_buscar_por_nombre) {
    const std::string ruta = rutaDePrueba("buscar_por_nombre.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(Alumno(1, "Ana", "Gomez", "ana1@unlp.edu.ar", "2214556677", "MAT-20001"));
    repo.alta(Alumno(2, "Beto", "Perez", "beto2@unlp.edu.ar", "2214556677", "MAT-20002"));
    repo.alta(Alumno(3, "Caro", "Sosa", "caro3@unlp.edu.ar", "2214556677", "MAT-20003"));
    repo.alta(Alumno(4, "Daniela", "Vega", "daniela4@unlp.edu.ar", "2214556677", "MAT-20004"));

    // POR EL NOMBRE, de forma PARCIAL: "an" esta DENTRO de "Ana" y de
    // "Daniela", y en ningun otro nombre ni apellido.
    const std::vector<Alumno> porNombre = repo.buscarPorNombre("an");
    CHECK(porNombre.size() == 2);
    CHECK(porNombre[0].getId() == 1);
    CHECK(porNombre[1].getId() == 4);

    // POR EL APELLIDO: "per" esta dentro de "Perez" y de nada mas.
    const std::vector<Alumno> porApellido = repo.buscarPorNombre("per");
    CHECK(porApellido.size() == 1);
    CHECK(porApellido[0].getId() == 2);

    // Y un texto que aparece en VARIOS apellidos: "ez" esta dentro de
    // "Gomez" y de "Perez".
    const std::vector<Alumno> porVarios = repo.buscarPorNombre("ez");
    CHECK(porVarios.size() == 2);
    CHECK(porVarios[0].getId() == 1);
    CHECK(porVarios[1].getId() == 2);

    // SIN DISTINGUIR MAYUSCULAS: el texto en mayusculas encuentra lo mismo.
    CHECK(repo.buscarPorNombre("PER").size() == 1);
    CHECK(repo.buscarPorNombre("per").size() == 1);
    CHECK(repo.buscarPorNombre("Gom").size() == 1);
    CHECK(repo.buscarPorNombre("gom").front().getId() == 1);
    CHECK(repo.buscarPorNombre("PEREZ").size() == 1);

    // UNA BUSQUEDA SIN TEXTO NO ENCUENTRA NADA: devolver la lista entera
    // seria inventar resultados.
    CHECK(repo.buscarPorNombre("").empty());
    CHECK(repo.buscarPorNombre("   ").empty());

    // Y una busqueda que no coincide con nadie: vacio, sin lanzar.
    const Fallo fallo = capturarTipo<ExcepcionSistema>(
        [&] { CHECK(repo.buscarPorNombre("zzz").empty()); });
    CHECK(!fallo.lanzo);
    CHECK(!fallo.lanzoOtro);

    // La consulta NO modifica nada: es una consulta.
    CHECK(repo.contar() == 4);
}

TEST_CASE("buscarPorNombre devuelve los resultados ordenados por id",
          repositorio_buscar_por_nombre_ordenado) {
    const std::string ruta = rutaDePrueba("buscar_por_nombre_ordenado.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    // Se dan de alta en ORDEN DESCENDENTE: el resultado tiene que salir
    // ordenado por id igual, porque lo que se filtra es el `listar()` del
    // arbol, que ya viene ordenado.
    repo.alta(Alumno(30, "Ana", "Gomez", "ana30@unlp.edu.ar", "2214556677", "MAT-20030"));
    repo.alta(Alumno(20, "Ana", "Perez", "ana20@unlp.edu.ar", "2214556677", "MAT-20020"));
    repo.alta(Alumno(10, "Ana", "Sosa", "ana10@unlp.edu.ar", "2214556677", "MAT-20010"));

    const std::vector<Alumno> encontrados = repo.buscarPorNombre("ana");
    CHECK(encontrados.size() == 3);
    CHECK(encontrados[0].getId() == 10);
    CHECK(encontrados[1].getId() == 20);
    CHECK(encontrados[2].getId() == 30);
}

TEST_CASE("buscarPorNombre funciona tambien en el repositorio de profesores",
          repositorio_buscar_por_nombre_profesores) {
    const std::string ruta = rutaDePrueba("buscar_por_nombre_profesores.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(Profesor(1, "Ana", "Gomez", "ana1@unlp.edu.ar", "2214556677", "LEG-1001",
                       "Algoritmos I"));
    repo.alta(Profesor(2, "Beto", "Perez", "beto2@unlp.edu.ar", "2214556677", "LEG-1002",
                       "Base de Datos"));

    CHECK(repo.buscarPorNombre("beto").size() == 1);
    CHECK(repo.buscarPorNombre("BET").size() == 1);
    CHECK(repo.buscarPorNombre("legajo").empty());
    CHECK(repo.buscarPorNombre("").empty());
}

// ---------------------------------------------------------------------------
// LOS ACTIVOS Y LOS INACTIVOS
//
// La baja es LOGICA: un alumno o un profesor dado de baja sigue en el
// repositorio. Estas dos consultas son las que permiten separar los dos
// grupos: la de los que se pueden agendar y la de los que hay que reactivar.
// ---------------------------------------------------------------------------

TEST_CASE("listarActivos y listarInactivos separan los alumnos por su estado",
          repositorio_alumnos_activos_e_inactivos) {
    const std::string ruta = rutaDePrueba("alumnos_activos.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos repo(ruta);
    repo.alta(alumnoDePrueba(1));
    repo.alta(alumnoDePrueba(2));
    repo.alta(alumnoDePrueba(3));

    repo.bajaLogica(2);

    const std::vector<Alumno> activos = repo.listarActivos();
    CHECK(activos.size() == 2);
    CHECK(activos[0].getId() == 1);
    CHECK(activos[1].getId() == 3);

    const std::vector<Alumno> inactivos = repo.listarInactivos();
    CHECK(inactivos.size() == 1);
    CHECK(inactivos[0].getId() == 2);

    // NINGUN ALUMNO ESTA EN LAS DOS LISTAS.
    for (const Alumno& activo : activos) {
        for (const Alumno& inactivo : inactivos) {
            CHECK(activo.getId() != inactivo.getId());
        }
    }

    // Y el repositorio sigue teniendo a LOS TRES: la baja es logica.
    CHECK(repo.contar() == 3);
}

TEST_CASE("listarActivos y listarInactivos separan los profesores por su estado",
          repositorio_profesores_activos_e_inactivos) {
    const std::string ruta = rutaDePrueba("profesores_activos.dat");
    std::filesystem::remove(ruta);

    RepositorioProfesores repo(ruta);
    repo.alta(profesorDePrueba(1));
    repo.alta(profesorDePrueba(2));
    repo.alta(profesorDePrueba(3));

    repo.bajaLogica(3);

    const std::vector<Profesor> activos = repo.listarActivos();
    CHECK(activos.size() == 2);
    CHECK(activos[0].getId() == 1);
    CHECK(activos[1].getId() == 2);

    const std::vector<Profesor> inactivos = repo.listarInactivos();
    CHECK(inactivos.size() == 1);
    CHECK(inactivos[0].getId() == 3);

    // NINGUN PROFESOR ESTA EN LAS DOS LISTAS.
    for (const Profesor& activo : activos) {
        for (const Profesor& inactivo : inactivos) {
            CHECK(activo.getId() != inactivo.getId());
        }
    }

    CHECK(repo.contar() == 3);
}

// ---------------------------------------------------------------------------
// INTERFACES: EL CONTRATO UNICO Y LOS FAKES
// ---------------------------------------------------------------------------

TEST_CASE("un fake en memoria cumple el contrato de IRepositorio",
          repositorio_fake_cumple_contrato) {
    // El FAKE es la justificacion de que la interfaz sea una interfaz y no
    // una clase concreta: el servicio (Fase 5, bloque 2) va a recibir
    // `IRepositorio<T>&` y los tests del servicio van a inyectar un fake
    // en memoria, sin tocar el disco.
    //
    // Acá se verifica que el contrato es COMPLETO: si `IRepositorio<T>`
    // tuviera un metodo mas, este fake no compilaria.
    class RepositorioEnMemoria : public IRepositorio<Alumno> {
    public:
        void alta(const Alumno& entidad) override { _alumnos.push_back(entidad); }
        void modificar(const Alumno& entidad) override {
            for (Alumno& alumno : _alumnos) {
                if (alumno.getId() == entidad.getId()) {
                    alumno = entidad;
                    return;
                }
            }
        }
        bool buscarPorId(int id, Alumno& fuera) const override {
            for (const Alumno& alumno : _alumnos) {
                if (alumno.getId() == id) {
                    fuera = alumno;
                    return true;
                }
            }
            return false;
        }
        std::vector<Alumno> listar() const override { return _alumnos; }
        std::size_t contar() const override { return _alumnos.size(); }
        int proximoId() const override {
            int maximo = 0;
            for (const Alumno& alumno : _alumnos) {
                maximo = alumno.getId() > maximo ? alumno.getId() : maximo;
            }
            return maximo + 1;
        }
        void cargar() override {}
        void persistir() override {}

    private:
        std::vector<Alumno> _alumnos;
    };

    RepositorioEnMemoria fake;
    fake.alta(alumnoDePrueba(1));
    fake.alta(alumnoDePrueba(2));

    CHECK(fake.contar() == 2);
    CHECK(fake.proximoId() == 3);

    Alumno encontrado;
    CHECK(fake.buscarPorId(2, encontrado));
    CHECK(encontrado.getId() == 2);
    CHECK(!fake.buscarPorId(9, encontrado));

    fake.modificar(alumnoDePrueba(1));
    CHECK(fake.buscarPorId(1, encontrado));
    CHECK(encontrado.getId() == 1);
}

// ---------------------------------------------------------------------------
// EL REPOSITORIO TRABAJA CON INTERFACAS, NO CON IMPLEMENTACIONES
// ---------------------------------------------------------------------------

TEST_CASE("el repositorio se puede usar a traves de la interfaz", repositorio_uso_via_interfaz) {
    const std::string ruta = rutaDePrueba("via_interfaz.dat");
    std::filesystem::remove(ruta);

    RepositorioAlumnos concreto(ruta);

    // Se escribe CONTRA LA INTERFAZ: asi se ve que la implementacion
    // concreta es reemplazable (por un fake, en los tests del servicio).
    IRepositorio<Alumno>& repo = concreto;

    repo.alta(alumnoDePrueba(3));

    Alumno encontrado;
    CHECK(repo.buscarPorId(3, encontrado));
    CHECK(repo.contar() == 1);
    CHECK(repo.proximoId() == 4);

    // Y el repositorio de alumnos tambien se puede ver por la interfaz de
    // la baja logica: es la misma objeto.
    IRepositorioBajaLogica<Alumno>& conBaja = concreto;
    conBaja.bajaLogica(3);
    CHECK(repo.buscarPorId(3, encontrado));
    CHECK(!encontrado.esActivo());
}
