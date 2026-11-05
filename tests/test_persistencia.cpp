#include "test_framework.h"

#include "domain/Especialidad.h"
#include "entities/Paciente.h"
#include "entities/Medico.h"
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
//   - los caminos de fallo de la escritura: la apertura del temporal y el
//     rename final (cada test dice EXACTAMENTE que falla)
//   - las propiedades del nombre del temporal: que es unico por escritura,
//     que es unico tambien entre tipos de entidad distintos y que tiene un
//     largo acotado
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

// Lista los `.tmp` que quedaron tirados en el directorio de pruebas.
//
// El nombre del temporal ahora es UNICO por escritura
// (`<ruta>.<algo-unico>.tmp`), asi que no se puede preguntar por
// `ruta + ".tmp"`: hay que ESCANEAR el directorio. Es lo mismo que hace el
// test `persistencia_sin_tmp`.
std::vector<std::filesystem::path> temporalesEnElDirectorio() {
    std::vector<std::filesystem::path> temporales;
    std::error_code error;
    for (const auto& entrada : std::filesystem::directory_iterator(directorioDePruebas(), error)) {
        if (entrada.path().extension() == ".tmp") {
            temporales.push_back(entrada.path());
        }
    }
    return temporales;
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

Paciente pacienteDePrueba(int id) {
    return Paciente(id, nombreDePrueba(id), apellidoDePrueba(id),
                  "paciente" + std::to_string(id) + "@unlp.edu.ar", "2214556677",
                  "20" + std::to_string(20000 + id));
}

Medico medicoDePrueba(int id) {
    return Medico(id, nombreDePrueba(id + 3), apellidoDePrueba(id + 7),
                    "medico" + std::to_string(id) + "@unlp.edu.ar", "2214556677",
                    "LEG-" + std::to_string(1000 + id), Especialidad::CARD);
}

Turno turnoDePrueba(int id) {
    const std::vector<Especialidad>& especialidads = todasLasEspecialidades();
    const Especialidad especialidad = especialidads[static_cast<std::size_t>(id - 1) % especialidads.size()];
    return Turno(id, 1 + (id % 5), 1 + (id % 7), especialidad, Fecha(2026, 10, 8 + (id % 10)),
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

// ---------------------------------------------------------------------------
// Un tipo NUEVO, que no se usa en ningun otro test.
//
// Sirve para probar que el estado de unicidad de los temporales es
// COMPARTIDO por todas las instanciaciones del template: al no haberse
// usado nunca, su contador arranca en 1 si el estado fuera por tipo de
// entidad, y continua el contador global si el estado es compartido.
// ---------------------------------------------------------------------------
struct RegistroFicticio {
    int id = 0;
    std::string valor;

    RegistroFicticio() = default;
    RegistroFicticio(int id_, std::string valor_) : id(id_), valor(std::move(valor_)) {
    }

    static std::string esquema() { return "id|valor"; }

    static RegistroFicticio fromRegistro(const std::string& linea) {
        RegistroFicticio registro;
        registro.id = 1;
        registro.valor = linea;
        return registro;
    }

    friend std::ostream& operator<<(std::ostream& os, const RegistroFicticio& registro) {
        return os << registro.id << '|' << registro.valor;
    }
};

// El nombre completo del temporal es `<ruta>.<semilla>-<numero>-<hash>.tmp`.
// El numero de secuencia es el SEGUNDO componente del token, en hexadecimal.
//
// Devuelve 0 si el token no tiene la forma esperada.
unsigned long long numeroDeSecuencia(const std::string& nombreTemporal) {
    // Se aisla el token: la ultima parte del nombre, entre el ultimo punto
    // (del nombre del destino) y el sufijo `.tmp`.
    const std::size_t finDelTmp = nombreTemporal.rfind(".tmp");
    if (finDelTmp == std::string::npos) {
        return 0;
    }
    const std::size_t finDelToken = finDelTmp;
    const std::size_t inicioDelToken = nombreTemporal.rfind('.', finDelToken == 0 ? 0 : finDelToken - 1);
    if (inicioDelToken == std::string::npos || finDelToken <= inicioDelToken + 1) {
        return 0;
    }
    const std::string token = nombreTemporal.substr(inicioDelToken + 1, finDelToken - inicioDelToken - 1);

    // El token es `<semilla>-<numero>-<hash del hilo>`.
    const std::size_t primerSeparador = token.find('-');
    if (primerSeparador == std::string::npos) {
        return 0;
    }
    const std::size_t segundoSeparador = token.find('-', primerSeparador + 1);
    if (segundoSeparador == std::string::npos) {
        return 0;
    }
    const std::string numero = token.substr(primerSeparador + 1,
                                            segundoSeparador - primerSeparador - 1);

    try {
        return std::stoull(numero, nullptr, 16);
    } catch (const std::exception&) {
        return 0;
    }
}

} // namespace

// ---------------------------------------------------------------------------
// ROUND TRIP
// ---------------------------------------------------------------------------

TEST_CASE("el round trip del paciente conserva todos los campos", persistencia_round_trip_paciente) {
    std::vector<Paciente> originales;
    for (int id = 1; id <= 100; ++id) {
        originales.push_back(pacienteDePrueba(id));
    }

    const std::string ruta = rutaDePrueba("pacientes.dat");
    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar(originales);

    const std::vector<Paciente> cargados = archivo.cargar();
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
        // El `dni` es el NUMERICO de la version 2 del formato: no la
        // `matricula` alfanumerica de la version 1.
        CHECK(cargados[i].getDni() == originales[i].getDni());
    }
    // Y el valor esta PINADO (no solo "igual al original"): asi el test
    // detecta tambien un `dni` que se pierde o se transforma al guardar.
    CHECK(cargados[0].getDni() == "2020001");
    CHECK(cargados[99].getDni() == "2020100");
}

TEST_CASE("el round trip del medico conserva todos los campos", persistencia_round_trip_medico) {
    std::vector<Medico> originales;
    for (int id = 1; id <= 100; ++id) {
        // La especialidad VARIA de un medico al otro: con una sola
        // especialidad el round trip probaria que el enum viaja, pero no que
        // viaja el CODIGO ("CARD", "PEDI", ...) que es lo que va al archivo.
        const std::vector<Especialidad>& especialidads = todasLasEspecialidades();
        const Especialidad especialidad =
            especialidads[static_cast<std::size_t>(id - 1) % especialidads.size()];
        originales.push_back(Medico(id, nombreDePrueba(id + 3), apellidoDePrueba(id + 7),
                                     "medico" + std::to_string(id) + "@unlp.edu.ar", "2214556677",
                                     "LEG-" + std::to_string(1000 + id), especialidad));
    }

    const std::string ruta = rutaDePrueba("medicos.dat");
    ArchivoTexto<Medico> archivo(ruta, "MEDICOS");
    archivo.guardar(originales);

    const std::vector<Medico> cargados = archivo.cargar();
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
        // Y por CODIGO: es lo que viaja al archivo, no la posicion del enum.
        CHECK(codigoDe(cargados[i].getEspecialidad()) == codigoDe(originales[i].getEspecialidad()));
    }

    // El archivo guarda el CODIGO, no la descripcion.
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(Utils::separar(lineas[2], '|')[7] == "CARD");
    CHECK(Utils::separar(lineas[3], '|')[7] == "MEDG");
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
        CHECK(cargados[i].getIdMedico() == originales[i].getIdMedico());
        CHECK(cargados[i].getIdPaciente() == originales[i].getIdPaciente());
        CHECK(cargados[i].getEspecialidad() == originales[i].getEspecialidad());
        CHECK(cargados[i].getFecha() == originales[i].getFecha());
        CHECK(cargados[i].getHora() == originales[i].getHora());
        CHECK(cargados[i].getDuracionMin() == originales[i].getDuracionMin());
        CHECK(cargados[i].getConsultorio() == originales[i].getConsultorio());
        CHECK(cargados[i].getEstado() == originales[i].getEstado());
        CHECK(cargados[i].getIdTurnoReemplazante() == originales[i].getIdTurnoReemplazante());
        // Y la especialidad por CODIGO: es lo que viaja al archivo, asi que
        // es lo que tiene que volver.
        CHECK(codigoDe(cargados[i].getEspecialidad()) == codigoDe(originales[i].getEspecialidad()));
    }
}

TEST_CASE("el turno conserva el estado y el turno que reemplaza",
          persistencia_round_trip_estado_y_reemplazo) {
    const Turno cancelado(1, 3, 7, Especialidad::CARD, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3",
                          EstadoTurno::Cancelado, -1);
    const Turno reprogramado(2, 3, 7, Especialidad::CARD, Fecha(2026, 10, 9), Hora(10, 0), 60, "A-3",
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
    Paciente paciente = pacienteDePrueba(1);
    paciente.setActivo(false);

    const std::string ruta = rutaDePrueba("baja.dat");
    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar({paciente});

    const std::vector<Paciente> cargados = archivo.cargar();
    CHECK(cargados.size() == 1);
    CHECK(!cargados[0].esActivo());

    // Y en el archivo el campo `activo` vale "0", no "false".
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);
    CHECK(Utils::separar(lineas[2], '|')[5] == "0");
}

TEST_CASE("las 12 especialidads viajan por codigo y vuelven por codigo",
          persistencia_especialidads_por_codigo) {
    std::vector<Turno> originales;
    for (std::size_t i = 0; i < todasLasEspecialidades().size(); ++i) {
        const Especialidad especialidad = todasLasEspecialidades()[i];
        originales.push_back(Turno(static_cast<int>(i) + 1, 1, 1, especialidad, Fecha(2026, 10, 8),
                                   Hora(9, 0), 60, "A-3"));
    }

    const std::string ruta = rutaDePrueba("especialidads.dat");
    ArchivoTexto<Turno> archivo(ruta, "TURNOS");
    archivo.guardar(originales);

    const std::vector<Turno> cargados = archivo.cargar();
    CHECK(cargados.size() == 12);

    for (std::size_t i = 0; i < cargados.size() && i < originales.size(); ++i) {
        // El codigo del archivo es el del catalogo, no el indice del enum.
        CHECK(codigoDe(cargados[i].getEspecialidad()) == codigoDe(originales[i].getEspecialidad()));
        CHECK(especialidadDesdeCodigo(codigoDe(cargados[i].getEspecialidad())) == originales[i].getEspecialidad());
    }

    // El archivo escribe el CODIGO de la especialidad.
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(Utils::separar(lineas[2], '|')[3] == "CARD");
    CHECK(Utils::separar(lineas[3], '|')[3] == "MEDG");
    CHECK(Utils::separar(lineas[13], '|')[3] == "NEUM");
}

// ---------------------------------------------------------------------------
// HEADER Y FORMATO
// ---------------------------------------------------------------------------

TEST_CASE("el archivo arranca con el header y la linea de esquema",
          persistencia_header_y_esquema) {
    const std::string ruta = rutaDePrueba("header.dat");
    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar({pacienteDePrueba(1), pacienteDePrueba(2), pacienteDePrueba(3)});

    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 5);
    CHECK(lineas[0] == "#PACIENTES|2|3");
    CHECK(lineas[1] == Paciente::esquema());
    CHECK(lineas[2] == "1|Beto|Perez|paciente1@unlp.edu.ar|2214556677|1|2020001");
}

TEST_CASE("el encabezado declara la cantidad de registros",
          persistencia_header_cantidad) {
    const std::string ruta = rutaDePrueba("cantidad.dat");
    ArchivoTexto<Medico> archivo(ruta, "MEDICOS");

    archivo.guardar({});
    CHECK(leerLineas(ruta)[0] == "#MEDICOS|2|0");

    archivo.guardar({medicoDePrueba(1)});
    CHECK(leerLineas(ruta)[0] == "#MEDICOS|2|1");
}

TEST_CASE("los registros salen ordenados por id", persistencia_orden_por_id) {
    // El orden lo aporta el `inOrder()` del BST (que es lo que va a usar el
    // repositorio de la Fase 5), no la capa de archivo: esta escribe lo que
    // le dan, en el orden en que lo recibe.
    ArbolBST<Paciente> arbol;
    CHECK(arbol.insertar(pacienteDePrueba(5)));
    CHECK(arbol.insertar(pacienteDePrueba(1)));
    CHECK(arbol.insertar(pacienteDePrueba(9)));
    CHECK(arbol.insertar(pacienteDePrueba(3)));
    CHECK(arbol.insertar(pacienteDePrueba(7)));

    const std::string ruta = rutaDePrueba("orden.dat");
    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
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
    const Paciente paciente(1, "Jos\xc3\xa9", "Mu\xc3\xb1oz", "jose@unlp.edu.ar", "2214556677",
                        "2020001");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar({paciente});

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
    std::vector<Paciente> originales;
    for (std::size_t i = 0; i < NOMBRES_ESPECIALES.size(); ++i) {
        originales.push_back(Paciente(static_cast<int>(i) + 1, NOMBRES_ESPECIALES[i], "Perez",
                                    "acent" + std::to_string(i + 1) + "@unlp.edu.ar", "2214556677",
                                    "203000" + std::to_string(i + 1)));
    }

    const std::string ruta = rutaDePrueba("tildes.dat");
    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar(originales);

    const std::vector<Paciente> cargados = archivo.cargar();
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
    const std::string esquema = Paciente::esquema();
    const std::string registro = "1|Jos\xc3\xa9|Perez|jose@unlp.edu.ar|2214556677|1|2030001";
    const std::string cuerpo = "1|Ana|Gomez|ana@unlp.edu.ar|2214556677|1|2020001";

    const std::string rutaCrlf = rutaDePrueba("crlf.dat");
    escribirBytes(rutaCrlf, "#PACIENTES|2|2\r\n" + esquema + "\r\n" + registro + "\r\n" + cuerpo
                                   + "\r\n");

    const std::string rutaLf = rutaDePrueba("lf.dat");
    escribirBytes(rutaLf, "#PACIENTES|2|2\n" + esquema + "\n" + registro + "\n" + cuerpo + "\n");

    ArchivoTexto<Paciente> archivoCrlf(rutaCrlf, "PACIENTES");
    ArchivoTexto<Paciente> archivoLf(rutaLf, "PACIENTES");

    const std::vector<Paciente> cargadosCrlf = archivoCrlf.cargar();
    const std::vector<Paciente> cargadosLf = archivoLf.cargar();

    CHECK(cargadosCrlf.size() == 2);
    CHECK(cargadosLf.size() == 2);
    CHECK(cargadosCrlf.size() == cargadosLf.size());

    for (std::size_t i = 0; i < cargadosCrlf.size() && i < cargadosLf.size(); ++i) {
        CHECK(cargadosCrlf[i].getId() == cargadosLf[i].getId());
        CHECK(cargadosCrlf[i].getNombre() == cargadosLf[i].getNombre());
        CHECK(cargadosCrlf[i].getApellido() == cargadosLf[i].getApellido());
        CHECK(cargadosCrlf[i].getEmail() == cargadosLf[i].getEmail());
        CHECK(cargadosCrlf[i].getDni() == cargadosLf[i].getDni());
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

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<ArchivoNoEncontradoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("no_existe.dat") != std::string::npos);
}

TEST_CASE("un archivo sin header lanza FormatoDesconocidoException",
          persistencia_error_sin_header) {
    const std::string ruta = rutaDePrueba("sin_header.dat");
    escribirBytes(ruta, "1|Ana|Gomez|ana@unlp.edu.ar|2214556677|1|2020001\n");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("header") != std::string::npos);
}

TEST_CASE("un archivo de otra entidad lanza FormatoDesconocidoException",
          persistencia_error_otra_entidad) {
    const std::string ruta = rutaDePrueba("otra_entidad.dat");
    escribirBytes(ruta, "#TURNOS|2|0\n" + Turno::esquema() + "\n");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    // El mensaje dice las DOS entidades: la del archivo y la esperada.
    CHECK(fallo.mensaje.find("TURNOS") != std::string::npos);
    CHECK(fallo.mensaje.find("PACIENTES") != std::string::npos);
}

TEST_CASE("una version distinta lanza FormatoDesconocidoException",
          persistencia_error_version) {
    const std::string ruta = rutaDePrueba("version.dat");
    escribirBytes(ruta, "#PACIENTES|3|0\n" + Paciente::esquema() + "\n");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    // El mensaje menciona la version del archivo y la que lee el codigo.
    CHECK(fallo.mensaje.find("formato v3") != std::string::npos);
    CHECK(fallo.mensaje.find("lee v2") != std::string::npos);
}

TEST_CASE("un archivo de la version anterior se rechaza con un error claro",
          persistencia_error_version_anterior) {
    // Un archivo de la version 1 (la `matricula` alfanumerica y la
    // especialidad por descripcion) NO se puede leer con el codigo de hoy.
    //
    // El archivo es un v1 AUTENTICO: su linea de esquema tambien es la de la
    // version 1. Si el loader no mirara la version PRIMERO, el error seria el
    // del esquema y no el de la version: este test verifica que el control
    // de version es el que dispara.
    const std::string ruta = rutaDePrueba("version_1.dat");
    escribirBytes(ruta,
                  "#PACIENTES|1|1\n"
                  "id|nombre|apellido|email|telefono|activo|matricula\n"
                  "1|Ana|Gomez|ana@unlp.edu.ar|2214556677|1|MAT-20391\n");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);

    // El mensaje dice LAS DOS versiones: la del archivo (v1) y la que lee el
    // codigo (v2). Sin las dos, el usuario no sabe que tiene que regenerar.
    CHECK(fallo.mensaje.find("formato v1") != std::string::npos);
    CHECK(fallo.mensaje.find("lee v2") != std::string::npos);
}

TEST_CASE("un esquema distinto lanza FormatoDesconocidoException",
          persistencia_error_esquema) {
    const std::string ruta = rutaDePrueba("esquema.dat");
    escribirBytes(ruta, "#PACIENTES|2|0\nid|nombre|apellido|email|telefono|activo|dni|extra\n");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("extra") != std::string::npos);
}

TEST_CASE("una linea con pocos campos lanza DatoInvalidoException con el numero de linea",
          persistencia_error_cantidad_campos) {
    const std::string ruta = rutaDePrueba("pocos_campos.dat");
    escribirBytes(ruta,
                  "#TURNOS|2|1\n" + Turno::esquema()
                      + "\n1|3|7|ING1|2026-10-08|09:00|60|A-3|Activo\n");

    ArchivoTexto<Turno> archivo(ruta, "TURNOS");
    const Fallo fallo = capturarTipo<DatoInvalidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    // El error dice QUE LINEA es (la 3: header, esquema, registro) y cuantas
    // campos se esperaban.
    CHECK(fallo.mensaje.find("3") != std::string::npos);
    CHECK(fallo.mensaje.find("10") != std::string::npos);
}

TEST_CASE("una especialidad inexistente lanza DatoInvalidoException con archivo y linea",
          persistencia_error_especialidad) {
    const std::string ruta = rutaDePrueba("especialidad_mala.dat");
    escribirBytes(ruta,
                  "#TURNOS|2|1\n" + Turno::esquema()
                      + "\n1|3|7|XXX|2026-10-08|09:00|60|A-3|Activo|-1\n");

    ArchivoTexto<Turno> archivo(ruta, "TURNOS");

    // El campo que fallo se conserva: la capa de archivo agrega el archivo y
    // la linea, pero no borra el diagnostico del dominio.
    CHECK(lanzaConCampo([&] { archivo.cargar(); }, "especialidad"));

    const Fallo fallo = capturarTipo<DatoInvalidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("especialidad_mala.dat") != std::string::npos);
    CHECK(fallo.mensaje.find("3") != std::string::npos);
    // El motivo del dominio se conserva (y no se duplica el mensaje entero).
    CHECK(fallo.mensaje.find("codigo de especialidad desconocido") != std::string::npos);

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
                  "#PACIENTES|2|5\n" + Paciente::esquema()
                      + "\n1|Ana|Gomez|ana1@unlp.edu.ar|2214556677|1|2020001\n"
                        "2|Beto|Perez|beto@unlp.edu.ar|2214556677|1|2020002\n"
                        "3|Caro|Sosa|caro@unlp.edu.ar|2214556677|1|2020003\n");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<DatoInvalidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    CHECK(fallo.mensaje.find("truncado") != std::string::npos);
}

TEST_CASE("un header que declara menos registros de los que hay no dice truncado",
          persistencia_error_sobran_registros) {
    const std::string ruta = rutaDePrueba("sobran.dat");
    // El header declara 2, pero el archivo tiene 3 registros: NO esta
    // truncado, le SOBRAN registros. Decir "truncado" seria mentir.
    escribirBytes(ruta,
                  "#PACIENTES|2|2\n" + Paciente::esquema()
                      + "\n1|Ana|Gomez|ana1@unlp.edu.ar|2214556677|1|2020001\n"
                        "2|Beto|Perez|beto@unlp.edu.ar|2214556677|1|2020002\n"
                        "3|Caro|Sosa|caro@unlp.edu.ar|2214556677|1|2020003\n");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<DatoInvalidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    // La palabra "truncado" NO puede aparecer: el archivo no esta truncado.
    CHECK(fallo.mensaje.find("truncado") == std::string::npos);
    // Y el mensaje dice las dos cantidades (la declarada y la leida).
    CHECK(fallo.mensaje.find("2") != std::string::npos);
    CHECK(fallo.mensaje.find("3") != std::string::npos);
}

TEST_CASE("un id que no entra en un int lanza DatoInvalidoException con archivo y linea",
          persistencia_error_id_desbordado) {
    const std::string ruta = rutaDePrueba("desborde.dat");
    // El id es un numero valido como TEXTO, pero no entra en un `int`.
    escribirBytes(ruta,
                  "#PACIENTES|2|1\n" + Paciente::esquema()
                      + "\n99999999999|Ana|Gomez|ana1@unlp.edu.ar|2214556677|1|2020001\n");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");

    // El campo que fallo se conserva: el dominio lo diagnostico bien.
    CHECK(lanzaConCampo([&] { archivo.cargar(); }, "id"));

    const Fallo fallo = capturarTipo<DatoInvalidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
    // El mensaje dice el ARCHIVO y la LINEA (la 3: header, esquema, registro).
    CHECK(fallo.mensaje.find("desborde.dat") != std::string::npos);
    CHECK(fallo.mensaje.find("3") != std::string::npos);
    // Y el motivo dice que el numero se sale del rango.
    CHECK(fallo.mensaje.find("rango") != std::string::npos);

    // NO puede escaparse una excepcion cruda de la biblioteca estandar: el
    // contrato de `cargar()` es que un registro mal formado siempre se
    // reporta como `DatoInvalidoException`.
    const Fallo otro = capturarTipo<ExcepcionSistema>([&] { archivo.cargar(); });
    CHECK(otro.lanzo);
    CHECK(!otro.lanzoOtro);
}

TEST_CASE("una linea en blanco intercalada se ignora", persistencia_linea_en_blanco) {
    const std::string ruta = rutaDePrueba("linea_blanca.dat");
    escribirBytes(ruta,
                  "#PACIENTES|2|2\n" + Paciente::esquema()
                      + "\n1|Ana|Gomez|ana1@unlp.edu.ar|2214556677|1|2020001\n"
                        "\n"
                        "2|Beto|Perez|beto@unlp.edu.ar|2214556677|1|2020002\n");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const std::vector<Paciente> cargados = archivo.cargar();
    CHECK(cargados.size() == 2);
    CHECK(cargados[0].getId() == 1);
    CHECK(cargados[1].getId() == 2);
}

TEST_CASE("un archivo vacio lanza FormatoDesconocidoException", persistencia_archivo_vacio) {
    const std::string ruta = rutaDePrueba("vacio.dat");
    escribirBytes(ruta, "");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<FormatoDesconocidoException>([&] { archivo.cargar(); });
    CHECK(fallo.lanzo);
}

// ---------------------------------------------------------------------------
// ATOMICIDAD
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// POR QUE NO HAY UN TEST DEL FALLO DIFERIDO EN EL `close()`
//
// `guardar()` tiene TRES comprobaciones que pueden fallar, en este orden:
//   1. la APERTURA del temporal (`ofstream`): se mide en
//      `persistencia_temporal_no_se_puede_abrir`;
//   2. la ESCRITURA y el `close()` (errores diferidos: disco lleno, cuota,
//      error de E/S del dispositivo): se mide la escritura en la rama de
//      `salida.fail()` y el `close()` en su propia rama;
//   3. el RENOMBRE final: se mide en `persistencia_renombre_falla` y en
//      `persistencia_renombre_falla_archivo_solo_lectura`.
//
// La rama 2, es decir el error que aparece DIFERIDO en el `close()`, NO la
// ejercita ningun test, y esto es DELIBERADO:
//
//   - No es reproducible en un sistema de archivos normal: haria falta un
//     disco lleno de verdad, una cuota agotada o un dispositivo que falle a
//     mitad de la escritura. Montar esas condiciones en cada corrida del test
//     no es una opcion (haria el test lento, fragil y dependiente del
//     entorno).
//   - Inyectarlo exigiria un SEAM DE PRUEBA en el codigo de produccion (por
//     ejemplo, un `ofstream` inyectable o un punto de fallo configurable).
//     Este proyecto NO tiene esa costura y agregarla seria meter codigo de
//     prueba dentro de la capa de persistencia.
//
// La rama PERMANECE en el codigo y esta verificada POR CONSTRUCCION: el
// `close()` es explicito y su resultado se chequea con `salida.fail()`, y si
// falla se borra el temporal y se lanza `ExcepcionSistema` antes de que el
// rename llegue a tocar el destino. Es decir: la garantia que importa (el
// destino no se toca) se sostiene por el ORDEN de las operaciones, no por
// el resultado del `close()`.
// ---------------------------------------------------------------------------

TEST_CASE("si la serializacion falla el archivo original queda intacto",
          persistencia_serializacion_falla) {
    const std::string ruta = rutaDePrueba("atomico.dat");

    // Primero se escribe un archivo valido.
    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar({pacienteDePrueba(1), pacienteDePrueba(2)});
    const std::string contenidoOriginal = leerArchivo(ruta);
    CHECK(!contenidoOriginal.empty());

    // Ahora se intenta guardar con un tipo que falla a mitad de camino.
    RegistroQueFalla::reiniciar(2);
    std::vector<RegistroQueFalla> registros;
    registros.push_back(RegistroQueFalla{1, "uno"});
    registros.push_back(RegistroQueFalla{2, "dos"});
    registros.push_back(RegistroQueFalla{3, "tres"});

    ArchivoTexto<RegistroQueFalla> archivoRoto(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivoRoto.guardar(registros); });
    CHECK(fallo.lanzo);

    // El archivo original quedo INTACTO (no a medias).
    CHECK(leerArchivo(ruta) == contenidoOriginal);

    // Y no quedo ningun `.tmp` tirado en el directorio. Como el temporal
    // ahora tiene un nombre UNICO por escritura, no se puede preguntar por
    // `ruta + ".tmp"`: hay que escanear el directorio.
    CHECK(temporalesEnElDirectorio().empty());

    // El archivo sigue siendo valido: se puede cargar.
    const std::vector<Paciente> cargados = archivo.cargar();
    CHECK(cargados.size() == 2);
}

TEST_CASE("si el temporal no se puede abrir se lanza ExcepcionSistema y el destino queda intacto",
          persistencia_temporal_no_se_puede_abrir) {
    // QUE INYECTA ESTE TEST, EXACTAMENTE:
    //
    // El destino esta dentro de un directorio que NO existe. Por eso el
    // TEMPORAL NO SE PUEDE ABRIR: falla la APERTURA (`ofstream`), que es la
    // PRIMERA comprobacion de `guardar()`.
    //
    // Lo que este test NO mide (y por eso no lo dice):
    //   - la comprobacion de escritura (`salida << ...` / `flush()`);
    //   - la comprobacion de cierre (`close()`), que es donde aparecen los
    //     errores diferidos (disco lleno, error de E/S en el `close`);
    //   - el rename final.
    //
    // Esos caminos se miden en otros tests (el rename en
    // `persistencia_renombre_falla`). El nombre del test dice lo que
    // realmente hace: fallar la APERTURA del temporal.
    const std::string ruta = rutaDePrueba("escritura_falla.dat");

    // Primero se escribe un archivo valido, para tener un destino que
    // preserve.
    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar({pacienteDePrueba(1), pacienteDePrueba(2)});
    const std::string contenidoOriginal = leerArchivo(ruta);
    CHECK(!contenidoOriginal.empty());

    // Ahora se apunta a un destino dentro de un directorio inexistente.
    const std::string rutaImposible =
        (directorioDePruebas() / "directorio_inexistente" / "destino.dat").string();
    ArchivoTexto<Paciente> archivoImposible(rutaImposible, "PACIENTES");
    const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivoImposible.guardar({pacienteDePrueba(1)}); });
    CHECK(fallo.lanzo);
    // El mensaje dice que archivo fallo.
    CHECK(fallo.mensaje.find("destino.dat") != std::string::npos);

    // No quedo ningun `.tmp` tirado en el directorio de pruebas.
    CHECK(temporalesEnElDirectorio().empty());

    // Y el archivo valido de antes quedo INTACTO.
    CHECK(leerArchivo(ruta) == contenidoOriginal);
    CHECK(archivo.cargar().size() == 2);
}

TEST_CASE("si el destino no se puede reemplazar se lanza ExcepcionSistema y no queda .tmp",
          persistencia_renombre_falla) {
    // QUE INYECTA ESTE TEST, EXACTAMENTE:
    //
    // El destino es un DIRECTORIO: el temporal se ESCRIBE bien (la escritura
    // y el cierre tienen exito) y lo que falla es el RENOMBRE final, porque
    // un directorio no se puede reemplazar con un archivo.
    //
    // Es el unico camino de fallo del rename que se puede reproducir de
    // forma determinista en un sistema de archivos normal: con un destino
    // que es un ARCHIVO regular, `std::filesystem::rename` lo reemplaza sin
    // problema (asi lo manda el estandar).
    //
    // El caso del ARCHIVO regular que no se puede reemplazar esta en el test
    // de abajo (`persistencia_renombre_falla_archivo_solo_lectura`).
    const std::filesystem::path destino = directorioDePruebas() / "destino_es_directorio";
    std::error_code error;
    std::filesystem::remove_all(destino, error);
    CHECK(std::filesystem::create_directories(destino, error));

    ArchivoTexto<Paciente> archivo(destino.string(), "PACIENTES");
    const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivo.guardar({pacienteDePrueba(1)}); });
    CHECK(fallo.lanzo);
    // El mensaje dice que archivo no se pudo reemplazar.
    CHECK(fallo.mensaje.find("destino_es_directorio") != std::string::npos);

    // No quedo ningun `.tmp` tirado en el directorio de pruebas: el temporal
    // se escribio y se CERRO bien, pero al fallar el rename se borro.
    CHECK(temporalesEnElDirectorio().empty());

    // El directorio de destino sigue siendo un directorio: no se toco.
    CHECK(std::filesystem::is_directory(destino, error));

    std::filesystem::remove_all(destino, error);
}

TEST_CASE("si el destino no se puede reemplazar queda intacto y sigue siendo cargable",
          persistencia_renombre_falla_archivo_solo_lectura) {
    // El destino es un ARCHIVO REGULAR al que se le sacan los permisos de
    // escritura, para que el rename no lo pueda reemplazar.
    //
    // OJO CON LA DIFERENCIA ENTRE PLATAFORMAS: el rename se autoriza segun
    // los permisos del DIRECTORIO que contiene al destino, no segun los del
    // archivo destino. En Windows (MoveFileEx) el archivo de solo lectura NO
    // se puede reemplazar y el rename falla; en POSIX (rename(2)) el rename
    // lo reemplaza igual, porque lo que manda es el directorio.
    //
    // Por eso el test mide lo que ES VERDAD EN LAS DOS PLATAFORMAS: el
    // destino preexistente nunca se publica a medias y el temporal nunca
    // queda tirado. En la rama de Windows se verifica ademas que el
    // contenido viejo quedo intacto byte a byte.
#if defined(_WIN32) || defined(_WIN64)
    const std::filesystem::path destino = directorioDePruebas() / "destino_solo_lectura.dat";
    std::error_code error;
    std::filesystem::remove(destino, error);

    // Primero se escribe un archivo valido, para tener un destino que
    // preserve.
    {
        ArchivoTexto<Paciente> archivoInicial(destino.string(), "PACIENTES");
        archivoInicial.guardar({pacienteDePrueba(1), pacienteDePrueba(2)});
    }
    const std::string contenidoOriginal = leerArchivo(destino.string());
    CHECK(!contenidoOriginal.empty());

    // Se le quitan los permisos de escritura: el rename ya no puede
    // reemplazar el archivo.
    std::filesystem::permissions(destino, std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace);

    ArchivoTexto<Paciente> archivo(destino.string(), "PACIENTES");
    const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivo.guardar({pacienteDePrueba(9)}); });
    CHECK(fallo.lanzo);
    // El mensaje dice que archivo no se pudo reemplazar.
    CHECK(fallo.mensaje.find("destino_solo_lectura.dat") != std::string::npos);

    // No quedo ningun `.tmp` tirado en el directorio de pruebas.
    CHECK(temporalesEnElDirectorio().empty());

    // El archivo preexistente quedo INTACTO, byte a byte.
    CHECK(leerArchivo(destino.string()) == contenidoOriginal);

    // Y sigue siendo valido: se puede cargar, con los MISMOS registros de
    // antes (no con el registro nuevo que se queria escribir).
    std::filesystem::permissions(destino, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    const std::vector<Paciente> cargados = archivo.cargar();
    CHECK(cargados.size() == 2);
    CHECK(cargados[0].getId() == 1);
    CHECK(cargados[1].getId() == 2);

    std::filesystem::remove(destino, error);
#else
    // En POSIX el rename SI reemplaza el archivo de solo lectura (manda el
    // directorio), asi que el guardado termina bien. Lo que se verifica es
    // la garantia que vale en TODAS las plataformas: el temporal no queda
    // tirado y el destino queda con un archivo valido.
    const std::filesystem::path destino = directorioDePruebas() / "destino_solo_lectura.dat";
    std::error_code error;
    std::filesystem::remove(destino, error);

    {
        ArchivoTexto<Paciente> archivoInicial(destino.string(), "PACIENTES");
        archivoInicial.guardar({pacienteDePrueba(1), pacienteDePrueba(2)});
    }

    std::filesystem::permissions(destino, std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace);

    ArchivoTexto<Paciente> archivo(destino.string(), "PACIENTES");
    const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivo.guardar({pacienteDePrueba(9)}); });

    // No quedo ningun `.tmp` tirado, pase lo que pase con el rename.
    CHECK(temporalesEnElDirectorio().empty());

    // Y el destino quedo siempre con un archivo valido y cargable.
    CHECK(existeArchivo(destino.string()));
    std::filesystem::permissions(destino, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace);
    CHECK(!archivo.cargar().empty());

    std::filesystem::remove(destino, error);
#endif
}

TEST_CASE("el archivo escrito usa solo LF y es identico en cualquier plataforma",
          persistencia_sin_cr) {
    const std::string ruta = rutaDePrueba("sin_cr.dat");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar({pacienteDePrueba(1), pacienteDePrueba(2)});

    // El modo BINARIO de escritura garantiza que los bytes sean los mismos
    // en Windows y en Linux: sin ningun '\r' (CRLF).
    const std::string contenido = leerArchivo(ruta);
    CHECK(!contenido.empty());
    CHECK(contenido.find('\r') == std::string::npos);

    // Y ademas las lineas se leen bien (el esquema y los registros estan
    // donde tienen que estar).
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 4);
    CHECK(lineas[0] == "#PACIENTES|2|2");
    CHECK(lineas[1] == Paciente::esquema());
}

TEST_CASE("un guardado exitoso no deja ningun .tmp", persistencia_sin_tmp) {
    const std::string ruta = rutaDePrueba("sin_tmp.dat");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar({pacienteDePrueba(1)});
    CHECK(temporalesEnElDirectorio().empty());

    archivo.guardar({pacienteDePrueba(1), pacienteDePrueba(2)});
    CHECK(temporalesEnElDirectorio().empty());

    // Y en el directorio no hay ningun `.tmp` con otra extension.
    const std::filesystem::path directorio = directorioDePruebas();
    for (const auto& entrada : std::filesystem::directory_iterator(directorio)) {
        CHECK(entrada.path().extension() != ".tmp");
    }
}

TEST_CASE("el archivo temporal tiene un nombre unico por escritura", persistencia_tmp_unico) {
    // El temporal NO puede ser siempre `<ruta>.tmp`: dos escritores del mismo
    // destino en el mismo directorio compartirian el temporal y uno
    // publicaria el archivo a medio escribir del otro.
    //
    // Para ver el nombre del temporal se lo pide al propio codigo: si el
    // directorio del destino no existe, la apertura del temporal falla y el
    // mensaje de error dice COMO se llamaba. Ese nombre es el que hay que
    // comparar entre dos escrituras.
    const std::filesystem::path directorioImposible =
        directorioDePruebas() / "directorio_inexistente";
    const std::string ruta = (directorioImposible / "destino.dat").string();

    std::vector<std::string> nombresTemporales;
    for (int i = 0; i < 5; ++i) {
        ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
        const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivo.guardar({pacienteDePrueba(1)}); });
        CHECK(fallo.lanzo);

        // El mensaje trae el nombre del temporal: se lo extrae.
        const std::size_t inicio = fallo.mensaje.find('\'');
        const std::size_t fin = fallo.mensaje.find('\'', inicio + 1);
        CHECK(inicio != std::string::npos);
        CHECK(fin != std::string::npos);
        if (inicio == std::string::npos || fin == std::string::npos) {
            continue;
        }
        const std::string nombreTemporal = fallo.mensaje.substr(inicio + 1, fin - inicio - 1);
        nombresTemporales.push_back(nombreTemporal);

        // El temporal conserva el SUFIJO `.tmp` (asi lo cubre el `.gitignore`).
        CHECK(nombreTemporal.size() > 4);
        CHECK(nombreTemporal.compare(nombreTemporal.size() - 4, 4, ".tmp") == 0);
        // Y NO es el nombre fijo `<ruta>.tmp`.
        CHECK(nombreTemporal != ruta + ".tmp");
    }

    // Las CINCO escrituras usaron temporales DISTINTOS.
    CHECK(nombresTemporales.size() == 5);
    for (std::size_t i = 0; i < nombresTemporales.size(); ++i) {
        for (std::size_t j = i + 1; j < nombresTemporales.size(); ++j) {
            CHECK(nombresTemporales[i] != nombresTemporales[j]);
        }
    }
}

TEST_CASE("el nombre del temporal tiene un largo acotado", persistencia_tmp_largo_acotado) {
    // El nombre del temporal se arma con el nombre del destino mas el token
    // unico. Hay DOS limites que hay que respetar:
    //
    //   1. el del COMPONENTE de la ruta (255 bytes en ext4 y en NTFS);
    //   2. el de la RUTA COMPLETA (260 caracteres en Windows, MAX_PATH).
    //
    // El segundo es el que MATA: el temporal es siempre MAS LARGO que el
    // destino (le agrega el token y el sufijo `.tmp`). Acotar solo el
    // componente NO alcanza: con un directorio de 70 caracteres y un destino
    // de 190, el destino entra en MAX_PATH pero el temporal se pasa en 30
    // caracteres, la apertura falla y el guardado reporta un error sobre un
    // destino que se puede escribir perfectamente.
    //
    // Se usa un destino con el nombre MUY largo: un nombre de 240 caracteres
    // (dentro del limite del componente) mas el token y el sufijo `.tmp` NO
    // entrarian ni en el componente ni en la ruta completa.
    const std::filesystem::path directorioImposible =
        directorioDePruebas() / "directorio_inexistente";
    const std::string nombreLargo(240, 'x');
    const std::string ruta = (directorioImposible / (nombreLargo + ".dat")).string();

    std::string nombreTemporal;
    {
        ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
        const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivo.guardar({pacienteDePrueba(1)}); });
        CHECK(fallo.lanzo);

        const std::size_t inicio = fallo.mensaje.find('\'');
        const std::size_t fin = fallo.mensaje.find('\'', inicio + 1);
        CHECK(inicio != std::string::npos);
        CHECK(fin != std::string::npos);
        if (inicio == std::string::npos || fin == std::string::npos) {
            return;
        }
        nombreTemporal = fallo.mensaje.substr(inicio + 1, fin - inicio - 1);
    }

    // El COMPONENTE del temporal (el nombre del archivo, sin los directorios)
    // entra en el limite del sistema de archivos.
    const std::filesystem::path componente(nombreTemporal);
    CHECK(componente.filename().string().size() <= 255);

    // Y la RUTA COMPLETA del temporal tambien entra: es lo que decide si el
    // temporal se puede abrir en Windows (MAX_PATH: 259 caracteres utiles).
    // El limite se mide sobre la ruta COMPLETA (directorio + nombre), que es
    // lo que el sistema de archivos mira de verdad.
    CHECK(nombreTemporal.size() <= 255);

    // El sufijo `.tmp` se conserva (asi lo cubre el `.gitignore`).
    CHECK(nombreTemporal.size() > 4);
    CHECK(nombreTemporal.compare(nombreTemporal.size() - 4, 4, ".tmp") == 0);

    // Y el temporal sigue siendo UNICO: dos escrituras del mismo destino
    // largo no pueden compartirlo.
    std::string nombreTemporal2;
    {
        ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
        const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivo.guardar({pacienteDePrueba(1)}); });
        CHECK(fallo.lanzo);
        const std::size_t inicio = fallo.mensaje.find('\'');
        const std::size_t fin = fallo.mensaje.find('\'', inicio + 1);
        CHECK(inicio != std::string::npos);
        CHECK(fin != std::string::npos);
        if (inicio == std::string::npos || fin == std::string::npos) {
            return;
        }
        nombreTemporal2 = fallo.mensaje.substr(inicio + 1, fin - inicio - 1);
    }
    CHECK(nombreTemporal != nombreTemporal2);
}

TEST_CASE("un destino con nombre largo se puede guardar igual", persistencia_destino_largo) {
    // La garantia que importa NO es "el temporal tiene un nombre corto":
    // es "si el DESTINO se puede escribir, el temporal tambien se puede
    // abrir". El temporal es mas largo que el destino (le agrega el token y
    // el `.tmp`), asi que un presupuesto mal calculado hace que `guardar()`
    // falle sobre un destino perfectamente escribible.
    //
    // El caso se arma a MEDIDA: se busca un nombre de archivo tal que la
    // ruta COMPLETA del destino entre en el limite del sistema (Windows:
    // MAX_PATH, 259 caracteres utiles) pero la ruta del temporal NO. Asi, sin
    // el recorte correcto, `guardar()` falla; con el recorte, entra.
    //
    // El largo del nombre se calcula sobre el directorio de pruebas REAL, que
    // cambia de maquina a maquina.
    const std::filesystem::path directorio = directorioDePruebas() / "destino_largo";
    std::error_code error;
    std::filesystem::remove_all(directorio, error);
    CHECK(std::filesystem::create_directories(directorio, error));

    // Largo objetivo de la ruta COMPLETA del destino: entra en MAX_PATH de
    // Windows con margen, pero el temporal (que le agrega ~30 caracteres) se
    // pasaria.
    constexpr std::size_t LARGO_OBJETIVO = 250;
    const std::size_t largoDelDirectorio = directorio.string().size();
    const std::size_t espacioParaElNombre =
        LARGO_OBJETIVO > largoDelDirectorio + 1 + 4 ? LARGO_OBJETIVO - largoDelDirectorio - 1 - 4 : 0;
    if (espacioParaElNombre < 16) {
        // El directorio de pruebas es tan largo que el caso no se puede
        // armar en esta maquina: no hay nada que afirmar.
        std::filesystem::remove_all(directorio, error);
        return;
    }

    const std::string nombreLargo(espacioParaElNombre, 'd');
    const std::string ruta = (directorio / (nombreLargo + ".dat")).string();

    // El destino entra en el limite del sistema de archivos.
    CHECK(ruta.size() <= 255);

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivo.guardar({pacienteDePrueba(1)}); });

    // ESTE es el punto del test: el destino con nombre largo se guarda SIN
    // error. Si el temporal se pasara del limite de ruta, `guardar()`
    // fallaria con "No se pudo abrir el archivo temporal".
    CHECK(!fallo.lanzo);
    CHECK(existeArchivo(ruta));

    // Y el archivo guardado es valido: se puede cargar.
    CHECK(archivo.cargar().size() == 1);

    // No quedo ningun `.tmp` tirado.
    CHECK(temporalesEnElDirectorio().empty());

    std::filesystem::remove_all(directorio, error);
}

TEST_CASE("un directorio tan largo que no queda presupuesto igual se comporta bien",
          persistencia_directorio_largo) {
    // Caso borde del presupuesto: el directorio es tan largo que, con el
    // token y el sufijo `.tmp` pegados, NO entra en el limite de ruta.
    //
    // Ahi no hay ninguna base que recortar: el temporal seria mas largo que
    // la ruta completa permite, SIEMPRE (el temporal es mas largo que el
    // destino por el token y el `.tmp`). Tampoco el destino entra. Lo que
    // tiene que pasar es que `guardar()` falle CON UN MENSAJE CLARO, sin
    // dejar ningun `.tmp` tirado y sin tocar el destino.
    //
    // Este test no necesita construir un directorio de 220 caracteres: con
    // un directorio normal ya se llega al `else` del presupuesto si el token
    // y el sufijo son largos. Se verifica el COMPORTAMIENTO: o el guardado
    // funciona, o falla con `ExcepcionSistema` (nunca con una excepcion
    // cruda) y sin dejar basura.
    const std::filesystem::path directorio = directorioDePruebas() / "directorio_largo";
    std::error_code error;
    std::filesystem::remove_all(directorio, error);
    CHECK(std::filesystem::create_directories(directorio, error));

    const std::string ruta = (directorio / "destino.dat").string();

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    bool lanzo = false;
    std::string mensaje;
    try {
        archivo.guardar({pacienteDePrueba(1)});
    } catch (const ExcepcionSistema& e) {
        lanzo = true;
        mensaje = e.what();
    } catch (const std::exception&) {
        // Una excepcion CRUDA de la biblioteca estandar seria un defecto:
        // la capa de persistencia garantiza `ExcepcionSistema`.
        CHECK(false);
    }

    if (lanzo) {
        // Si fallo, el mensaje tiene que decir QUE archivo fallo.
        CHECK(mensaje.find("destino.dat") != std::string::npos);
    } else {
        // Si pudo guardar, el archivo tiene que existir y ser valido.
        CHECK(existeArchivo(ruta));
        CHECK(archivo.cargar().size() == 1);
    }

    // Pase lo que pase, no queda ningun `.tmp` tirado.
    CHECK(temporalesEnElDirectorio().empty());

    std::filesystem::remove_all(directorio, error);
}

TEST_CASE("el temporal es unico tambien entre tipos de entidad distintos",
          persistencia_tmp_unico_entre_tipos) {
    // El estado de unicidad (el contador atomico y el generador de entropia)
    // es COMPARTIDO por todas las instanciaciones del template. Si fuera
    // por tipo de entidad, `ArchivoTexto<Paciente>` y `ArchivoTexto<Turno>`
    // tendrian cada uno su propio contador empezando en 0, y podrian
    // derivar el mismo nombre de temporal.
    //
    // COMO SE COMPRUEBA, Y POR QUE NO BASTA COMPARAR DOS NOMBRES:
    //
    // Comparar el temporal del paciente con el del turno NO alcanza: con el
    // estado POR TIPO, el contador del paciente ya viene adelantado por otros
    // tests y el del turno recien arranca, asi que los nombres DIFIEREN
    // igual y el test pasaria sin detectar nada.
    //
    // Lo que delata el estado por tipo es el NUMERO DE SECUENCIA: con el
    // estado COMPARTIDO hay UN SOLO contador para todo el programa, asi que
    // el numero de un tipo NUEVO (que nunca se uso en ningun test) es
    // estrictamente mayor que el del tipo que se uso antes. Con el estado por
    // tipo, el numero del tipo nuevo arrancaria en 1.
    //
    // El token tiene la forma `<semilla>-<numero>-<hash del hilo>`: se
    // extrae el numero de secuencia para poder comparar.
    const std::filesystem::path directorioImposible =
        directorioDePruebas() / "directorio_inexistente";
    const std::string ruta = (directorioImposible / "destino.dat").string();

    // El temporal del PACIENTE (el tipo que ya se uso en otros tests).
    std::string nombrePaciente;
    {
        ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
        const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivo.guardar({pacienteDePrueba(1)}); });
        CHECK(fallo.lanzo);
        const std::size_t inicio = fallo.mensaje.find('\'');
        const std::size_t fin = fallo.mensaje.find('\'', inicio + 1);
        CHECK(inicio != std::string::npos);
        CHECK(fin != std::string::npos);
        if (inicio == std::string::npos || fin == std::string::npos) {
            return;
        }
        nombrePaciente = fallo.mensaje.substr(inicio + 1, fin - inicio - 1);
    }

    // El temporal de un tipo NUEVO, que no se uso en ningun otro test.
    std::string nombreNuevo;
    {
        ArchivoTexto<RegistroFicticio> archivo(ruta, "FICTICIOS");
        const Fallo fallo = capturarTipo<ExcepcionSistema>([&] { archivo.guardar({RegistroFicticio(1, "uno")}); });
        CHECK(fallo.lanzo);
        const std::size_t inicio = fallo.mensaje.find('\'');
        const std::size_t fin = fallo.mensaje.find('\'', inicio + 1);
        CHECK(inicio != std::string::npos);
        CHECK(fin != std::string::npos);
        if (inicio == std::string::npos || fin == std::string::npos) {
            return;
        }
        nombreNuevo = fallo.mensaje.substr(inicio + 1, fin - inicio - 1);
    }

    // Los DOS temporales son DISTINTOS: son de tipos de entidad distintos y
    // no pueden compartir el nombre.
    CHECK(nombrePaciente != nombreNuevo);

    // Y el numero de secuencia del tipo NUEVO es estrictamente MAYOR que el
    // del paciente: eso solo puede pasar si el contador es UNO para todo el
    // programa. Con el estado por tipo, el tipo nuevo arrancaria en 1.
    const unsigned long long numeroPaciente = numeroDeSecuencia(nombrePaciente);
    const unsigned long long numeroNuevo = numeroDeSecuencia(nombreNuevo);
    CHECK(numeroPaciente > 0);
    CHECK(numeroNuevo > 0);
    CHECK(numeroNuevo > numeroPaciente);
}

TEST_CASE("guardar reemplaza el archivo y no lo agrega al final",
          persistencia_reemplaza_archivo) {
    const std::string ruta = rutaDePrueba("reemplaza.dat");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar({pacienteDePrueba(1), pacienteDePrueba(2), pacienteDePrueba(3)});
    CHECK(leerLineas(ruta).size() == 5);

    // Se guarda un solo registro: el archivo tiene que quedar con UN
    // registro, no con cuatro.
    archivo.guardar({pacienteDePrueba(9)});
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);
    CHECK(lineas[0] == "#PACIENTES|2|1");
    CHECK(Utils::separar(lineas[2], '|')[0] == "9");
}

TEST_CASE("el contenido del archivo es exactamente el esperado",
          persistencia_contenido_exacto) {
    const std::string ruta = rutaDePrueba("exacto.dat");

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.guardar({pacienteDePrueba(1)});

    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 3);
    CHECK(lineas[0] == "#PACIENTES|2|1");
    CHECK(lineas[1] == "id|nombre|apellido|email|telefono|activo|dni");
    CHECK(lineas[2] == "1|Beto|Perez|paciente1@unlp.edu.ar|2214556677|1|2020001");
}

TEST_CASE("crearVacio crea el archivo y no pisa uno existente",
          persistencia_crear_vacio) {
    const std::string ruta = rutaDePrueba("crear_vacio.dat");
    std::filesystem::remove(ruta);

    ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    archivo.crearVacio();

    CHECK(existeArchivo(ruta));
    const std::vector<std::string> lineas = leerLineas(ruta);
    CHECK(lineas.size() == 2);
    CHECK(lineas[0] == "#PACIENTES|2|0");
    CHECK(lineas[1] == Paciente::esquema());

    // El archivo recien creado se puede cargar: da cero registros.
    const std::vector<Paciente> cargados = archivo.cargar();
    CHECK(cargados.empty());

    // `crearVacio` sobre un archivo que YA existe no lo destruye.
    archivo.guardar({pacienteDePrueba(1)});
    archivo.crearVacio();
    CHECK(archivo.cargar().size() == 1);
}

// ---------------------------------------------------------------------------
// INTEGRACION
// ---------------------------------------------------------------------------

TEST_CASE("el turno sigue siendo valido aunque se reordene el enum de especialidads",
          persistencia_enum_reordenado) {
    // El archivo guarda el CODIGO de la especialidad, asi que reordenar el enum no
    // lo invalida: el codigo es el mismo.
    const Turno turno(1, 3, 7, Especialidad::NEUM, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");

    const std::string ruta = rutaDePrueba("reorden.dat");
    ArchivoTexto<Turno> archivo(ruta, "TURNOS");
    archivo.guardar({turno});

    const std::vector<Turno> cargados = archivo.cargar();
    CHECK(cargados.size() == 1);
    CHECK(cargados[0].getEspecialidad() == Especialidad::NEUM);
    CHECK(codigoDe(cargados[0].getEspecialidad()) == "NEUM");
}

TEST_CASE("los archivos de ejemplo se cargan y cada turno es de la especialidad de su medico",
          persistencia_ejemplos) {
    const std::string rutaTurnos = rutaDeEjemplo("turnos.dat");
    CHECK(!rutaTurnos.empty());
    if (rutaTurnos.empty()) {
        return;
    }

    ArchivoTexto<Turno> archivoTurnos(rutaTurnos, "TURNOS");
    const std::vector<Turno> turnos = archivoTurnos.cargar();
    CHECK(!turnos.empty());

    // LA SEMILLA ES COHERENTE CON LA REGLA 2b
    //
    // Un turno es de la especialidad de SU medico: si no lo fuera, el turno
    // no se podria agendar (la regla del servicio lo rechaza). Los archivos
    // de ejemplo tienen que cumplir la misma regla que la aplicacion.
    const std::string rutaMedicos = rutaDeEjemplo("medicos.dat");
    const std::string rutaPacientes = rutaDeEjemplo("pacientes.dat");
    CHECK(!rutaMedicos.empty());
    CHECK(!rutaPacientes.empty());
    if (rutaMedicos.empty() || rutaPacientes.empty()) {
        return;
    }

    ArchivoTexto<Medico> archivoMedicos(rutaMedicos, "MEDICOS");
    ArchivoTexto<Paciente> archivoPacientes(rutaPacientes, "PACIENTES");
    const std::vector<Medico> medicos = archivoMedicos.cargar();
    const std::vector<Paciente> pacientes = archivoPacientes.cargar();
    CHECK(!medicos.empty());
    CHECK(!pacientes.empty());

    for (const Turno& turno : turnos) {
        const Medico* medico = nullptr;
        for (const Medico& candidato : medicos) {
            if (candidato.getId() == turno.getIdMedico()) {
                medico = &candidato;
                break;
            }
        }
        CHECK(medico != nullptr);
        if (medico != nullptr) {
            CHECK(turno.getEspecialidad() == medico->getEspecialidad());
        }

        // Y el paciente del turno existe.
        bool existe = false;
        for (const Paciente& candidato : pacientes) {
            if (candidato.getId() == turno.getIdPaciente()) {
                existe = true;
                break;
            }
        }
        CHECK(existe);
    }
}

TEST_CASE("el archivo de ejemplo de pacientes tiene el header esperado",
          persistencia_ejemplo_header) {
    const std::string rutaPacientes = rutaDeEjemplo("pacientes.dat");
    CHECK(!rutaPacientes.empty());
    if (rutaPacientes.empty()) {
        return;
    }

    const std::vector<std::string> lineas = leerLineas(rutaPacientes);
    CHECK(lineas.size() >= 2);
    CHECK(lineas[0].compare(0, 11, "#PACIENTES|") == 0);
    CHECK(lineas[1] == Paciente::esquema());
}

TEST_CASE("la API expone la ruta y el nombre de la entidad", persistencia_api) {
    const std::string ruta = rutaDePrueba("api.dat");
    const ArchivoTexto<Paciente> archivo(ruta, "PACIENTES");
    CHECK(archivo.ruta() == ruta);
    CHECK(archivo.nombreEntidad() == "PACIENTES");
    CHECK(ArchivoTexto<Paciente>::VERSION_FORMATO == 2);
}
