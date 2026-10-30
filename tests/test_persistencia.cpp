#include "test_framework.h"

#include "domain/Materia.h"
#include "entities/Alumno.h"
#include "entities/Profesor.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "persistence/ArchivoTexto.h"
#include "structures/ArbolBST.h"
#include "utils/Fecha.h"
#include "utils/Utils.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ostream>
#include <string>
#include <system_error>
#include <vector>

// ---------------------------------------------------------------------------
// Tests de la FASE 4: la persistencia en archivos.
//
// Lo que se prueba ACa:
//   - el round trip (guardar y recargar) comparado CAMPO POR CAMPO
//   - el header, la linea de esquema y el formato del archivo
//   - las tildes y los finales de linea de Windows
//   - los errores tipificados: ArchivoNoEncontrado, FormatoDesconocido y
//     DatoInvalido
//   - la escritura ATOMICA (el archivo original nunca queda a medias y no
//     queda ningun `.tmp` tirado)
//
// NOTA: el round trip NO se compara con `operator==` (que compara solo por
// id): se compara campo por campo, que es el criterio real.
//
// Los archivos que usan estos tests se escriben en un directorio temporal:
// los tests no tocan los archivos de ejemplo de `data/` (salvo el test que
// los LEE).
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
            std::filesystem::temp_directory_path() / "sistema_turnos_fase4";
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

// Escribe BYTES EXACTOS (sin pasar por el modo texto): asi el test puede
// simular un archivo editado a mano, con finales de linea CRLF o LF.
void escribirBytes(const std::string& ruta, const std::string& contenido) {
    std::ofstream archivo(ruta, std::ios::out | std::ios::binary | std::ios::trunc);
    archivo.write(contenido.data(), static_cast<std::streamsize>(contenido.size()));
}

// Contenido completo de un archivo.
std::string leerArchivo(const std::string& ruta) {
    std::ifstream archivo(ruta, std::ios::in | std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(archivo)),
                       std::istreambuf_iterator<char>());
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

bool existeArchivo(const std::string& ruta) {
    std::error_code error;
    return std::filesystem::exists(ruta, error);
}

// Busca un archivo de ejemplo (`data/...`) probando los directorios
// candidatos: el test corre desde la raiz del repo (Makefile) o desde
// `build/` (CTest).
std::string rutaDeEjemplo(const std::string& nombre) {
    static const char* DIRECTORIOS[] = {"data", "../data", "../../data", "../../../data"};
    for (const char* directorio : DIRECTORIOS) {
        const std::filesystem::path candidata = std::filesystem::path(directorio) / nombre;
        if (existeArchivo(candidata.string())) {
            return candidata.string();
        }
    }
    return std::string();
}

// ---------------------------------------------------------------------------
// Datos de prueba: 100 entidades validas de cada tipo.
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
    const std::vector<Materia>& materias = todasLasMaterias();
    const Materia materia = materias[static_cast<std::size_t>(id - 1) % materias.size()];
    return Turno(id, 1 + (id % 5), 1 + (id % 7), materia, Fecha(2026, 10, 8 + (id % 10)),
                 Hora(8 + (id % 10), (id % 2) * 30), 60, "A-" + std::to_string(1 + (id % 4)),
                 (id % 5 == 0) ? EstadoTurno::Cancelado : EstadoTurno::Activo,
                 (id % 7 == 0) ? 42 : -1);
}

// ---------------------------------------------------------------------------
// Un tipo de prueba que ROMPE a mitad de la serializacion.
//
// Sirve para verificar la ATOMICIDAD: si la serializacion falla, el archivo
// original tiene que quedar intacto y no tiene que quedar ningun `.tmp`.
//
// Tambien demuestra que el TAD es un template de verdad: funciona con
// cualquier tipo que cumpla el contrato, no solo con las entidades del
// dominio.
// ---------------------------------------------------------------------------
class RegistroQueFalla {
public:
    static int escrituras;
    static int fallarEn;

    static void reiniciar(int fallarEn_) {
        escrituras = 0;
        fallarEn = fallarEn_;
    }

    static std::string esquema() { return "id|valor"; }

    static RegistroQueFalla fromRegistro(const std::string& linea) {
        RegistroQueFalla registro;
        registro.id = 1;
        registro.valor = linea;
        return registro;
    }

    friend std::ostream& operator<<(std::ostream& os, const RegistroQueFalla& registro) {
        ++escrituras;
        if (escrituras == fallarEn) {
            throw DatoInvalidoException("valor", registro.valor, "fallo simulado de serializacion");
        }
        return os << registro.id << '|' << registro.valor;
    }

    int id = 0;
    std::string valor;
};

int RegistroQueFalla::escrituras = 0;
int RegistroQueFalla::fallarEn = -1;

} // namespace

// ---------------------------------------------------------------------------
// ROUND TRIP
// ---------------------------------------------------------------------------

TEST_CASE("el round trip del alumno conserva todos los campos", persistencia_round_trip_alumno) {
    std::vector<Alumno> originales;
    for (int id = 1; id <= 100; ++id) {
        originales.push_back(alumnoDePrueba(id));
    }

    const std::string ruta = rutaDePrueba("alumnos.dat");
    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar(originales);

    const std::vector<Alumno> cargados = archivo.cargar();
    CHECK(cargados.size() == originales.size());

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

TEST_CASE("el round trip del profesor conserva todos los campos", persistencia_round_trip_profesor) {
    std::vector<Profesor> originales;
    for (int id = 1; id <= 100; ++id) {
        originales.push_back(profesorDePrueba(id));
    }

    const std::string ruta = rutaDePrueba("profesores.dat");
    ArchivoTexto<Profesor> archivo(ruta, "PROFESORES");
    archivo.guardar(originales);

    const std::vector<Profesor> cargados = archivo.cargar();
    CHECK(cargados.size() == originales.size());

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

TEST_CASE("el round trip del turno conserva todos los campos", persistencia_round_trip_turno) {
    std::vector<Turno> originales;
    for (int id = 1; id <= 100; ++id) {
        originales.push_back(turnoDePrueba(id));
    }

    const std::string ruta = rutaDePrueba("turnos.dat");
    ArchivoTexto<Turno> archivo(ruta, "TURNOS");
    archivo.guardar(originales);

    const std::vector<Turno> cargados = archivo.cargar();
    CHECK(cargados.size() == originales.size());

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

TEST_CASE("el turno conserva el estado y el turno que reemplaza",
          persistencia_round_trip_estado_y_reemplazo) {
    const Turno cancelado(1, 3, 7, Materia::ALG1, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3",
                          EstadoTurno::Cancelado, -1);
    const Turno reprogramado(2, 3, 7, Materia::ALG1, Fecha(2026, 10, 9), Hora(10, 0), 60, "A-3",
                             EstadoTurno::Reprogramado, 42);

    const std::string ruta = rutaDePrueba("estados.dat");
    ArchivoTexto<Turno> archivo(ruta, "TURNOS");
    archivo.guardar({cancelado, reprogramado});

    const std::vector<Turno> cargados = archivo.cargar();
    CHECK(cargados.size() == 2);
    CHECK(cargados[0].getEstado() == EstadoTurno::Cancelado);
    CHECK(cargados[0].getIdTurnoReemplazante() == -1);
    CHECK(cargados[1].getEstado() == EstadoTurno::Reprogramado);
    CHECK(cargados[1].getIdTurnoReemplazante() == 42);
}

TEST_CASE("la baja logica sobrevive al round trip", persistencia_round_trip_baja_logica) {
    Alumno alumno = alumnoDePrueba(1);
    alumno.setActivo(false);

    const std::string ruta = rutaDePrueba("baja.dat");
    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar({alumno});

    const std::vector<Alumno> cargados = archivo.cargar();
    CHECK(cargados.size() == 1);
    CHECK(!cargados[0].esActivo());

    // Y en el archivo el campo `activo` vale "0", no "false".
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);
    CHECK(Utils::separar(lineas[2], '|')[5] == "0");
}

TEST_CASE("las 12 materias viajan por codigo y vuelven por codigo",
          persistencia_materias_por_codigo) {
    std::vector<Turno> originales;
    for (std::size_t i = 0; i < todasLasMaterias().size(); ++i) {
        const Materia materia = todasLasMaterias()[i];
        originales.push_back(Turno(static_cast<int>(i) + 1, 1, 1, materia, Fecha(2026, 10, 8),
                                   Hora(9, 0), 60, "A-3"));
    }

    const std::string ruta = rutaDePrueba("materias.dat");
    ArchivoTexto<Turno> archivo(ruta, "TURNOS");
    archivo.guardar(originales);

    const std::vector<Turno> cargados = archivo.cargar();
    CHECK(cargados.size() == 12);

    for (std::size_t i = 0; i < cargados.size() && i < originales.size(); ++i) {
        // El codigo del archivo es el del catalogo, no el indice del enum.
        CHECK(codigoDe(cargados[i].getMateria()) == codigoDe(originales[i].getMateria()));
        CHECK(materiaDesdeCodigo(codigoDe(cargados[i].getMateria())) == originales[i].getMateria());
    }

    // El archivo escribe el CODIGO de la materia.
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(Utils::separar(lineas[2], '|')[3] == "ALG1");
    CHECK(Utils::separar(lineas[3], '|')[3] == "ALG2");
    CHECK(Utils::separar(lineas[13], '|')[3] == "ARQUITECTURA");
}

// ---------------------------------------------------------------------------
// HEADER Y FORMATO
// ---------------------------------------------------------------------------

TEST_CASE("el archivo arranca con el header y la linea de esquema",
          persistencia_header_y_esquema) {
    const std::string ruta = rutaDePrueba("header.dat");
    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar({alumnoDePrueba(1), alumnoDePrueba(2), alumnoDePrueba(3)});

    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 5);
    CHECK(lineas[0] == "#ALUMNOS|1|3");
    CHECK(lineas[1] == Alumno::esquema());
    CHECK(lineas[2] == "1|Beto|Perez|alumno1@unlp.edu.ar|2214556677|1|MAT-20001");
}

TEST_CASE("el encabezado declara la cantidad de registros",
          persistencia_header_cantidad) {
    const std::string ruta = rutaDePrueba("cantidad.dat");
    ArchivoTexto<Profesor> archivo(ruta, "PROFESORES");

    archivo.guardar({});
    CHECK(leerLineas(ruta)[0] == "#PROFESORES|1|0");

    archivo.guardar({profesorDePrueba(1)});
    CHECK(leerLineas(ruta)[0] == "#PROFESORES|1|1");
}

TEST_CASE("los registros salen ordenados por id", persistencia_orden_por_id) {
    // El orden lo aporta el `inOrder()` del BST (que es lo que va a usar el
    // repositorio de la Fase 5), no la capa de archivo: esta escribe lo que
    // le dan, en el orden en que lo recibe.
    ArbolBST<Alumno> arbol;
    CHECK(arbol.insertar(alumnoDePrueba(5)));
    CHECK(arbol.insertar(alumnoDePrueba(1)));
    CHECK(arbol.insertar(alumnoDePrueba(9)));
    CHECK(arbol.insertar(alumnoDePrueba(3)));
    CHECK(arbol.insertar(alumnoDePrueba(7)));

    const std::string ruta = rutaDePrueba("orden.dat");
    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar(arbol.inOrder());

    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 7);

    int idAnterior = 0;
    for (std::size_t i = 2; i < lineas.size(); ++i) {
        const int id = std::stoi(Utils::separar(lineas[i], '|')[0]);
        CHECK(id > idAnterior);
        idAnterior = id;
    }
}

TEST_CASE("ningun campo del archivo contiene el separador", persistencia_sin_separador) {
    const std::string ruta = rutaDePrueba("separador.dat");
    ArchivoTexto<Turno> archivo(ruta, "TURNOS");
    archivo.guardar({turnoDePrueba(1), turnoDePrueba(2), turnoDePrueba(3)});

    const std::vector<std::string> lineas = leerLineas(ruta);
    for (std::size_t i = 0; i < lineas.size(); ++i) {
        // El header y el esquema tienen separadores (son delimitadores), pero
        // ningun DATO puede traerlo.
        if (i < 2) {
            continue;
        }
        const std::vector<std::string> campos = Utils::separar(lineas[i], '|');
        CHECK(campos.size() == 10);
        for (const std::string& campo : campos) {
            CHECK(campo.find('|') == std::string::npos);
        }
    }
}

TEST_CASE("el archivo esta en UTF-8 y no tiene BOM", persistencia_utf8_sin_bom) {
    const std::string ruta = rutaDePrueba("utf8.dat");
    // Un nombre con tilde: tiene que quedar en UTF-8 (dos bytes por letra
    // acentuada) y el archivo NO tiene que arrancar con el BOM.
    const Alumno alumno(1, "Jos\xc3\xa9", "Mu\xc3\xb1oz", "jose@unlp.edu.ar", "2214556677",
                        "MAT-20001");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar({alumno});

    const std::string contenido = leerArchivo(ruta);
    CHECK(contenido.size() > 3);

    // BOM: los bytes EF BB BF al principio del archivo.
    CHECK(!(contenido[0] == '\xEF' && contenido[1] == '\xBB' && contenido[2] == '\xBF'));

    // Y el texto acentuado se guarda como UTF-8 de verdad.
    CHECK(contenido.find("Jos\xc3\xa9") != std::string::npos);
    CHECK(contenido.find("Mu\xc3\xb1oz") != std::string::npos);
}

// ---------------------------------------------------------------------------
// TILDES Y ACENTOS
// ---------------------------------------------------------------------------

TEST_CASE("los nombres con tilde y enye sobreviven al round trip",
          persistencia_tildes) {
    const std::vector<std::string> NOMBRES_ESPECIALES = {"Jos\xc3\xa9",        // José
                                                          "Mu\xc3\xb1oz",       // Muñoz
                                                          "Gonz\xc3\xa1lez",    // González
                                                          "Mart\xc3\xadnez"};  // Martínez
    std::vector<Alumno> originales;
    for (std::size_t i = 0; i < NOMBRES_ESPECIALES.size(); ++i) {
        originales.push_back(Alumno(static_cast<int>(i) + 1, NOMBRES_ESPECIALES[i], "Perez",
                                    "acent" + std::to_string(i + 1) + "@unlp.edu.ar", "2214556677",
                                    "MAT-3000" + std::to_string(i + 1)));
    }

    const std::string ruta = rutaDePrueba("tildes.dat");
    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar(originales);

    const std::vector<Alumno> cargados = archivo.cargar();
    CHECK(cargados.size() == originales.size());
    for (std::size_t i = 0; i < cargados.size() && i < originales.size(); ++i) {
        CHECK(cargados[i].getNombre() == originales[i].getNombre());
    }

    // Y en el archivo estan los bytes UTF-8 correctos.
    const std::string contenido = leerArchivo(ruta);
    CHECK(contenido.find("Jos\xc3\xa9") != std::string::npos);
    CHECK(contenido.find("Mu\xc3\xb1oz") != std::string::npos);
    CHECK(contenido.find("Gonz\xc3\xa1lez") != std::string::npos);
    CHECK(contenido.find("Mart\xc3\xadnez") != std::string::npos);
}

TEST_CASE("un archivo con finales CRLF carga igual que uno con LF",
          persistencia_fin_de_linea) {
    const std::string esquema = Alumno::esquema();
    const std::string registro = "1|Jos\xc3\xa9|Perez|jose@unlp.edu.ar|2214556677|1|MAT-30001";
    const std::string cuerpo = "1|Ana|Gomez|ana@unlp.edu.ar|2214556677|1|MAT-20001";

    const std::string rutaCrlf = rutaDePrueba("crlf.dat");
    escribirBytes(rutaCrlf, "#ALUMNOS|1|2\r\n" + esquema + "\r\n" + registro + "\r\n" + cuerpo
                                   + "\r\n");

    const std::string rutaLf = rutaDePrueba("lf.dat");
    escribirBytes(rutaLf, "#ALUMNOS|1|2\n" + esquema + "\n" + registro + "\n" + cuerpo + "\n");

    ArchivoTexto<Alumno> archivoCrlf(rutaCrlf, "ALUMNOS");
    ArchivoTexto<Alumno> archivoLf(rutaLf, "ALUMNOS");

    const std::vector<Alumno> cargadosCrlf = archivoCrlf.cargar();
    const std::vector<Alumno> cargadosLf = archivoLf.cargar();

    CHECK(cargadosCrlf.size() == 2);
    CHECK(cargadosLf.size() == 2);
    CHECK(cargadosCrlf.size() == cargadosLf.size());

    for (std::size_t i = 0; i < cargadosCrlf.size() && i < cargadosLf.size(); ++i) {
        CHECK(cargadosCrlf[i].getId() == cargadosLf[i].getId());
        CHECK(cargadosCrlf[i].getNombre() == cargadosLf[i].getNombre());
        CHECK(cargadosCrlf[i].getApellido() == cargadosLf[i].getApellido());
        CHECK(cargadosCrlf[i].getEmail() == cargadosLf[i].getEmail());
        CHECK(cargadosCrlf[i].getMatricula() == cargadosLf[i].getMatricula());
    }

    // El nombre con tilde llego bien en los dos casos.
    CHECK(cargadosCrlf[0].getNombre() == "Jos\xc3\xa9");
}

// ---------------------------------------------------------------------------
// VALIDACION Y ERRORES
// ---------------------------------------------------------------------------

TEST_CASE("cargar un archivo inexistente lanza ArchivoNoEncontradoException",
          persistencia_error_inexistente) {
    const std::string ruta = rutaDePrueba("no_existe.dat");
    std::filesystem::remove(ruta);

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    const Fallo fallo = capturarTipo<ArchivoNoEncontradoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("no_existe.dat") != std::string::npos);
}

TEST_CASE("un archivo sin header lanza FormatoDesconocidoException",
          persistencia_error_sin_header) {
    const std::string ruta = rutaDePrueba("sin_header.dat");
    escribirBytes(ruta, "1|Ana|Gomez|ana@unlp.edu.ar|2214556677|1|MAT-20001\n");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("header") != std::string::npos);
}

TEST_CASE("un archivo de otra entidad lanza FormatoDesconocidoException",
          persistencia_error_otra_entidad) {
    const std::string ruta = rutaDePrueba("otra_entidad.dat");
    escribirBytes(ruta, "#TURNOS|1|0\n" + Turno::esquema() + "\n");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    // El mensaje dice las DOS entidades: la del archivo y la esperada.
    CHECK(fallo.mensaje.find("TURNOS") != std::string::npos);
    CHECK(fallo.mensaje.find("ALUMNOS") != std::string::npos);
}

TEST_CASE("una version distinta lanza FormatoDesconocidoException",
          persistencia_error_version) {
    const std::string ruta = rutaDePrueba("version.dat");
    escribirBytes(ruta, "#ALUMNOS|3|0\n" + Alumno::esquema() + "\n");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    // El mensaje menciona la version del archivo y la que lee el codigo.
    CHECK(fallo.mensaje.find("3") != std::string::npos);
    CHECK(fallo.mensaje.find("1") != std::string::npos);
}

TEST_CASE("un esquema distinto lanza FormatoDesconocidoException",
          persistencia_error_esquema) {
    const std::string ruta = rutaDePrueba("esquema.dat");
    escribirBytes(ruta, "#ALUMNOS|1|0\nid|nombre|apellido|email|telefono|activo|matricula|extra\n");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("extra") != std::string::npos);
}

TEST_CASE("una linea con pocos campos lanza DatoInvalidoException con el numero de linea",
          persistencia_error_cantidad_campos) {
    const std::string ruta = rutaDePrueba("pocos_campos.dat");
    escribirBytes(ruta,
                  "#TURNOS|1|1\n" + Turno::esquema()
                      + "\n1|3|7|ING1|2026-10-08|09:00|60|A-3|Activo\n");

    ArchivoTexto<Turno> archivo(ruta, "TURNOS");
    const Fallo fallo = capturarTipo<DatoInvalidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    // El error dice QUE LINEA es (la 3: header, esquema, registro) y cuantas
    // campos se esperaban.
    CHECK(fallo.mensaje.find("3") != std::string::npos);
    CHECK(fallo.mensaje.find("10") != std::string::npos);
}

TEST_CASE("una materia inexistente lanza DatoInvalidoException con archivo y linea",
          persistencia_error_materia) {
    const std::string ruta = rutaDePrueba("materia_mala.dat");
    escribirBytes(ruta,
                  "#TURNOS|1|1\n" + Turno::esquema()
                      + "\n1|3|7|XXX|2026-10-08|09:00|60|A-3|Activo|-1\n");

    ArchivoTexto<Turno> archivo(ruta, "TURNOS");

    // El campo que fallo se conserva: la capa de archivo agrega el archivo y
    // la linea, pero no borra el diagnostico del dominio.
    CHECK(lanzaConCampo([&] { archivo.cargar(); }, "materia"));

    const Fallo fallo = capturarTipo<DatoInvalidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("materia_mala.dat") != std::string::npos);
    CHECK(fallo.mensaje.find("3") != std::string::npos);
    // El motivo del dominio se conserva (y no se duplica el mensaje entero).
    CHECK(fallo.mensaje.find("codigo de materia desconocido") != std::string::npos);

    // "Dato invalido" aparece UNA sola vez: el re-lanzamiento no repite el
    // mensaje que ya venia armado.
    std::size_t apariciones = 0;
    std::size_t posicion = fallo.mensaje.find("Dato invalido");
    while (posicion != std::string::npos) {
        ++apariciones;
        posicion = fallo.mensaje.find("Dato invalido", posicion + 1);
    }
    CHECK(apariciones == 1);
}

TEST_CASE("un header que declara mas registros de los que hay lanza DatoInvalidoException",
          persistencia_error_truncado) {
    const std::string ruta = rutaDePrueba("truncado.dat");
    escribirBytes(ruta,
                  "#ALUMNOS|1|5\n" + Alumno::esquema()
                      + "\n1|Ana|Gomez|ana1@unlp.edu.ar|2214556677|1|MAT-20001\n"
                        "2|Beto|Perez|beto@unlp.edu.ar|2214556677|1|MAT-20002\n"
                        "3|Caro|Sosa|caro@unlp.edu.ar|2214556677|1|MAT-20003\n");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    const Fallo fallo = capturarTipo<DatoInvalidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("truncado") != std::string::npos);
}

TEST_CASE("una linea en blanco intercalada se ignora", persistencia_linea_en_blanco) {
    const std::string ruta = rutaDePrueba("linea_blanca.dat");
    escribirBytes(ruta,
                  "#ALUMNOS|1|2\n" + Alumno::esquema()
                      + "\n1|Ana|Gomez|ana1@unlp.edu.ar|2214556677|1|MAT-20001\n"
                        "\n"
                        "2|Beto|Perez|beto@unlp.edu.ar|2214556677|1|MAT-20002\n");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    const std::vector<Alumno> cargados = archivo.cargar();
    CHECK(cargados.size() == 2);
    CHECK(cargados[0].getId() == 1);
    CHECK(cargados[1].getId() == 2);
}

TEST_CASE("un archivo vacio lanza FormatoDesconocidoException", persistencia_archivo_vacio) {
    const std::string ruta = rutaDePrueba("vacio.dat");
    escribirBytes(ruta, "");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
}

// ---------------------------------------------------------------------------
// ATOMICIDAD
// ---------------------------------------------------------------------------

TEST_CASE("si la serializacion falla el archivo original queda intacto",
          persistencia_serializacion_falla) {
    const std::string ruta = rutaDePrueba("atomico.dat");

    // Primero se escribe un archivo valido.
    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar({alumnoDePrueba(1), alumnoDePrueba(2)});
    const std::string contenidoOriginal = leerArchivo(ruta);
    CHECK(!contenidoOriginal.empty());

    // Ahora se intenta guardar con un tipo que falla a mitad de camino.
    RegistroQueFalla::reiniciar(2);
    std::vector<RegistroQueFalla> registros;
    registros.push_back(RegistroQueFalla{1, "uno"});
    registros.push_back(RegistroQueFalla{2, "dos"});
    registros.push_back(RegistroQueFalla{3, "tres"});

    ArchivoTexto<RegistroQueFalla> archivoRoto(ruta, "ALUMNOS");
    const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivoRoto.guardar(registros); });
    CHECK(fallo.lanzo);

    // El archivo original quedo INTACTO (no a medias).
    CHECK(leerArchivo(ruta) == contenidoOriginal);

    // Y no quedo ningun `.tmp` tirado en el directorio.
    CHECK(!existeArchivo(ruta + ".tmp"));

    // El archivo sigue siendo valido: se puede cargar.
    const std::vector<Alumno> cargados = archivo.cargar();
    CHECK(cargados.size() == 2);
}

TEST_CASE("un guardado exitoso no deja ningun .tmp", persistencia_sin_tmp) {
    const std::string ruta = rutaDePrueba("sin_tmp.dat");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar({alumnoDePrueba(1)});
    CHECK(!existeArchivo(ruta + ".tmp"));

    archivo.guardar({alumnoDePrueba(1), alumnoDePrueba(2)});
    CHECK(!existeArchivo(ruta + ".tmp"));

    // Y en el directorio no hay ningun `.tmp` con otra extension.
    const std::filesystem::path directorio = directorioDePruebas();
    for (const auto& entrada : std::filesystem::directory_iterator(directorio)) {
        CHECK(entrada.path().extension() != ".tmp");
    }
}

TEST_CASE("guardar reemplaza el archivo y no lo agrega al final",
          persistencia_reemplaza_archivo) {
    const std::string ruta = rutaDePrueba("reemplaza.dat");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar({alumnoDePrueba(1), alumnoDePrueba(2), alumnoDePrueba(3)});
    CHECK(leerLineas(ruta).size() == 5);

    // Se guarda un solo registro: el archivo tiene que quedar con UN
    // registro, no con cuatro.
    archivo.guardar({alumnoDePrueba(9)});
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);
    CHECK(lineas[0] == "#ALUMNOS|1|1");
    CHECK(Utils::separar(lineas[2], '|')[0] == "9");
}

TEST_CASE("el contenido del archivo es exactamente el esperado",
          persistencia_contenido_exacto) {
    const std::string ruta = rutaDePrueba("exacto.dat");

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.guardar({alumnoDePrueba(1)});

    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);
    CHECK(lineas[0] == "#ALUMNOS|1|1");
    CHECK(lineas[1] == "id|nombre|apellido|email|telefono|activo|matricula");
    CHECK(lineas[2] == "1|Beto|Perez|alumno1@unlp.edu.ar|2214556677|1|MAT-20001");
}

TEST_CASE("crearVacio crea el archivo y no pisa uno existente",
          persistencia_crear_vacio) {
    const std::string ruta = rutaDePrueba("crear_vacio.dat");
    std::filesystem::remove(ruta);

    ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    archivo.crearVacio();

    CHECK(existeArchivo(ruta));
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 2);
    CHECK(lineas[0] == "#ALUMNOS|1|0");
    CHECK(lineas[1] == Alumno::esquema());

    // El archivo recien creado se puede cargar: da cero registros.
    const std::vector<Alumno> cargados = archivo.cargar();
    CHECK(cargados.empty());

    // `crearVacio` sobre un archivo que YA existe no lo destruye.
    archivo.guardar({alumnoDePrueba(1)});
    archivo.crearVacio();
    CHECK(archivo.cargar().size() == 1);
}

// ---------------------------------------------------------------------------
// INTEGRACION
// ---------------------------------------------------------------------------

TEST_CASE("el turno sigue siendo valido aunque se reordene el enum de materias",
          persistencia_enum_reordenado) {
    // El archivo guarda el CODIGO de la materia, asi que reordenar el enum no
    // lo invalida: el codigo es el mismo.
    const Turno turno(1, 3, 7, Materia::ARQUITECTURA, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");

    const std::string ruta = rutaDePrueba("reorden.dat");
    ArchivoTexto<Turno> archivo(ruta, "TURNOS");
    archivo.guardar({turno});

    const std::vector<Turno> cargados = archivo.cargar();
    CHECK(cargados.size() == 1);
    CHECK(cargados[0].getMateria() == Materia::ARQUITECTURA);
    CHECK(codigoDe(cargados[0].getMateria()) == "ARQUITECTURA");
}

TEST_CASE("los archivos de ejemplo se cargan y traen las 12 materias",
          persistencia_ejemplos) {
    const std::string rutaTurnos = rutaDeEjemplo("turnos.dat");
    CHECK(!rutaTurnos.empty());
    if (rutaTurnos.empty()) {
        return;
    }

    ArchivoTexto<Turno> archivoTurnos(rutaTurnos, "TURNOS");
    const std::vector<Turno> turnos = archivoTurnos.cargar();
    CHECK(!turnos.empty());

    // Las 12 materias del catalogo aparecen en los turnos de ejemplo.
    constexpr std::size_t CANTIDAD_MATERIAS = static_cast<std::size_t>(Materia::ARQUITECTURA) + 1;
    bool aparecen[CANTIDAD_MATERIAS] = {false};
    for (const Turno& turno : turnos) {
        aparecen[static_cast<std::size_t>(turno.getMateria())] = true;
    }
    for (std::size_t i = 0; i < todasLasMaterias().size(); ++i) {
        CHECK(aparecen[static_cast<std::size_t>(todasLasMaterias()[i])]);
    }

    // Los otros dos archivos de ejemplo tambien cargan.
    const std::string rutaAlumnos = rutaDeEjemplo("alumnos.dat");
    const std::string rutaProfesores = rutaDeEjemplo("profesores.dat");
    CHECK(!rutaAlumnos.empty());
    CHECK(!rutaProfesores.empty());
    if (rutaAlumnos.empty() || rutaProfesores.empty()) {
        return;
    }

    ArchivoTexto<Alumno> archivoAlumnos(rutaAlumnos, "ALUMNOS");
    ArchivoTexto<Profesor> archivoProfesores(rutaProfesores, "PROFESORES");
    CHECK(!archivoAlumnos.cargar().empty());
    CHECK(!archivoProfesores.cargar().empty());
}

TEST_CASE("el archivo de ejemplo de alumnos tiene el header esperado",
          persistencia_ejemplo_header) {
    const std::string rutaAlumnos = rutaDeEjemplo("alumnos.dat");
    CHECK(!rutaAlumnos.empty());
    if (rutaAlumnos.empty()) {
        return;
    }

    const std::vector<std::string> lineas = leerLineas(rutaAlumnos);
    CHECK(lineas.size() >= 2);
    CHECK(lineas[0].compare(0, 9, "#ALUMNOS|") == 0);
    CHECK(lineas[1] == Alumno::esquema());
}

TEST_CASE("la API expone la ruta y el nombre de la entidad", persistencia_api) {
    const std::string ruta = rutaDePrueba("api.dat");
    const ArchivoTexto<Alumno> archivo(ruta, "ALUMNOS");
    CHECK(archivo.ruta() == ruta);
    CHECK(archivo.nombreEntidad() == "ALUMNOS");
    CHECK(ArchivoTexto<Alumno>::VERSION_FORMATO == 1);
}
