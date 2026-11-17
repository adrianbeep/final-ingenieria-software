#include "test_framework.h"

#include "domain/Especialidad.h"
#include "entities/Medico.h"
#include "entities/Paciente.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "persistence/ArchivoTexto.h"
#include "repositories/RepositorioMedicos.h"
#include "repositories/RepositorioPacientes.h"
#include "repositories/RepositorioTurnos.h"
#include "services/ServicioTurnos.h"
#include "utils/Fecha.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <vector>

// ---------------------------------------------------------------------------
// Tests de integracion del dominio completo.
//
// Que prueban estos tests
//
// Los tests de las fases anteriores prueban una capa por vez: los
// repositorios contra archivos, el servicio contra fakes en memoria, la
// persistencia contra `ArchivoTexto`. Aqui se prueban juntas, con los
// repositorios reales (los que escriben en archivos) y el servicio real.
//
// El recorrido que se prueba es el del programa de verdad: se agenda un
// turno, se persiste, se cierra el programa, se vuelve a abrir (con
// repositorios nuevos, que es lo que simula el reinicio) y se comprueba que
// lo que estaba en el archivo sigue igual.
//
// Por que estos tests no tocan los archivos de `data/`
//
// Los tres casos escriben en un directorio temporal propio. Los archivos de
// ejemplo de `data/` son de la aplicacion, no de los tests: un test que los
// modificara dejaria el repositorio con datos cambiados, y el resultado
// dependeria del orden de ejecucion.
//
// Y el directorio es distinto del que usan los tests de la Fase 4 y los de
// la Fase 5: esos borran el suyo con `remove_all` en su primera llamada, asi
// que si compartieran la base cada grupo se borraria el archivo al otro.
//
// Los tres casos
//
//   1. el ciclo de vida completo de un turno (agendar, cancelar, reprogramar)
//      sobrevive al cierre y a la reapertura;
//   2. las bajas logicas del paciente y del medico tambien sobreviven;
//   3. un fallo de escritura no deja la memoria y el disco desincronizados.
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

// Captura la excepcion `E` de la funcion. `lanzo` dice si lanzo esa
// excepcion; `lanzoOtro` dice si lanzo otra cosa (o ninguna: en ese caso
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

// El directorio temporal donde estos tests escriben sus archivos.
//
// Es una base propia: los tests de la Fase 4 (`sistema_turnos_fase4`) y los
// de la Fase 5 (`sistema_turnos_fase5`) borran la suya con `remove_all` en su
// primera llamada, y si compartieran base cada grupo se borraria el archivo
// al otro.
std::filesystem::path directorioDePruebas() {
    static const std::filesystem::path directorio = [] {
        const std::filesystem::path base =
            std::filesystem::temp_directory_path() / "sistema_turnos_integracion";
        std::error_code error;
        std::filesystem::remove_all(base, error);
        std::filesystem::create_directories(base, error);
        return base;
    }();
    return directorio;
}

// El directorio de un caso concreto.
//
// Cada caso tiene el suyo: asi los archivos de uno no se mezclan con los de
// otro y el resultado no depende del orden de ejecucion.
std::filesystem::path directorioDelCaso(const std::string& nombre) {
    const std::filesystem::path directorio = directorioDePruebas() / nombre;
    std::error_code error;
    std::filesystem::remove_all(directorio, error);
    std::filesystem::create_directories(directorio, error);
    return directorio;
}

// La ruta de un archivo de prueba.
std::string rutaDePrueba(const std::filesystem::path& directorio, const std::string& nombre) {
    return (directorio / nombre).string();
}

bool existeArchivo(const std::string& ruta) {
    std::error_code error;
    return std::filesystem::exists(ruta, error);
}

// El contenido completo del archivo, en bytes.
//
// Es lo que se compara cuando la pregunta es "el archivo quedo igual": si se
// compararan lineas, un cambio en los finales de linea pasaria inadvertido.
std::string leerArchivo(const std::string& ruta) {
    std::ifstream archivo(ruta, std::ios::in | std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(archivo)),
                       std::istreambuf_iterator<char>());
}

// Los `.tmp` que quedaron tirados en el directorio.
//
// El nombre del temporal es unico por escritura (`<ruta>.<token>.tmp`), asi
// que no se puede preguntar por `ruta + ".tmp"`: hay que escanear el
// directorio.
std::vector<std::filesystem::path> temporalesEnElDirectorio(
    const std::filesystem::path& directorio) {
    std::vector<std::filesystem::path> temporales;
    std::error_code error;
    for (const auto& entrada : std::filesystem::directory_iterator(directorio, error)) {
        if (entrada.path().extension() == ".tmp") {
            temporales.push_back(entrada.path());
        }
    }
    return temporales;
}

// ---------------------------------------------------------------------------
// Datos de prueba: entidades validas de cada tipo.
//
// Los nombres no llevan digitos: el validador del dominio (`Utils::esAlfabetico`)
// acepta solo letras y separadores.
// ---------------------------------------------------------------------------

Paciente pacienteDePrueba(int id) {
    return Paciente(id, "Ana", "Gomez", "paciente" + std::to_string(id) + "@unlp.edu.ar",
                    "2214556677", "200000" + std::to_string(id));
}

Medico medicoDePrueba(int id) {
    return Medico(id, "Beto", "Perez", "medico" + std::to_string(id) + "@unlp.edu.ar",
                  "2214556677", "LEG-" + std::to_string(1000 + id), Especialidad::CARD);
}

// Un turno valido: el constructor valida todo a traves de los setters, asi que
// un turno armado asi ya es un turno que el servicio acepta.
Turno turnoDePrueba(int id, int idMedico, int idPaciente, const Fecha& fecha, const Hora& hora) {
    return Turno(id, idMedico, idPaciente, Especialidad::CARD, fecha, hora, 45, "A-1");
}

// ---------------------------------------------------------------------------
// Como se hace fallar una escritura
//
// El destino del guardado tiene que ser algo que no se pueda reemplazar. Hay
// dos caminos, y el que sirve depende de la plataforma:
//
//   1. el archivo destino de solo lectura. En Windows el atributo de solo
//      lectura impide que el `MoveFileEx` lo reemplace (el temporal se
//      escribe bien y lo que falla es el rename final). En POSIX el rename se
//      autoriza segun los permisos del directorio y no los del archivo, asi
//      que ademas hay que quitarle el permiso de escritura al directorio:
//      sin ese permiso el temporal ni siquiera se puede abrir.
//
//   2. el destino que es un directorio. El rename de un archivo sobre un
//      directorio falla siempre, en cualquier plataforma y con cualquier
//      usuario (incluso con permisos de superusuario).
//
// El camino 1 es el que deja el archivo destino intacto byte a byte, que es lo
// que el test quiere verificar. El camino 2 se usa solo cuando el proceso
// corre como superusuario, en cuyo caso las comprobaciones de permisos no se
// aplican y el camino 1 no serviria.
// ---------------------------------------------------------------------------

// El modo de bloqueo que se uso para impedir la escritura.
enum class BloqueoDeEscritura {
    ElDestinoEsUnArchivoDeSoloLectura,
    ElDestinoEsUnDirectorio
};

#if !defined(_WIN32) && !defined(_WIN64)
// Dice si el sistema de archivos hace cumplir los permisos de escritura del
// directorio.
//
// La sonda es un directorio sin permiso de escritura: si se puede crear un
// archivo adentro, es que los permisos no se estan aplicando (o sea, que el
// proceso corre como superusuario).
//
// Esta sonda solo hace falta en POSIX: en Windows el bloqueo lo hace el
// atributo de solo lectura del archivo destino, que el sistema de archivos
// respeta siempre.
bool seCumplenLosPermisosDeEscritura() {
    const std::filesystem::path sonda = directorioDePruebas() / "sonda_permisos";
    std::error_code error;
    std::filesystem::remove_all(sonda, error);
    std::filesystem::create_directories(sonda, error);

    // El directorio se queda sin permiso de escritura, pero con permiso de
    // lectura y de recorrido: sin recorrido no se podria ni listar.
    std::filesystem::permissions(sonda,
                                 std::filesystem::perms::owner_read
                                     | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace, error);

    std::ofstream archivo(sonda / "sonda.dat", std::ios::out | std::ios::binary);
    const bool sePudoCrear = archivo.is_open();
    archivo.close();

    // Se restauran los permisos para que la sonda se pueda borrar.
    std::filesystem::permissions(sonda, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, error);
    std::filesystem::remove_all(sonda, error);

    return !sePudoCrear;
}
#endif

// El bloqueo que se va a usar depende de la plataforma.
BloqueoDeEscritura modoDeBloqueo() {
#if defined(_WIN32) || defined(_WIN64)
    // En Windows el bloqueo lo hace el atributo de solo lectura del archivo
    // destino: el `MoveFileEx` no puede reemplazar un archivo de solo
    // lectura. El atributo de solo lectura de un directorio, en cambio, no
    // impide crear archivos adentro, asi que el directorio no es lo que
    // frena la escritura.
    return BloqueoDeEscritura::ElDestinoEsUnArchivoDeSoloLectura;
#else
    // En POSIX el rename se autoriza segun los permisos del directorio. Si
    // los permisos se aplican, el directorio sin permiso de escritura
    // alcanza. Si no se aplican (proceso con permisos de superusuario), el
    // unico bloqueo que funciona en todas las plataformas es un destino que
    // sea un directorio.
    if (seCumplenLosPermisosDeEscritura()) {
        return BloqueoDeEscritura::ElDestinoEsUnArchivoDeSoloLectura;
    }
    return BloqueoDeEscritura::ElDestinoEsUnDirectorio;
#endif
}

// Impide que se pueda escribir el archivo `ruta`, que esta en `directorio`.
//
// Devuelve el modo de bloqueo que se uso, para que el test sepa que tiene que
// verificar.
BloqueoDeEscritura bloquearLaEscritura(const std::filesystem::path& directorio,
                                         const std::string& ruta) {
    std::error_code error;

    if (modoDeBloqueo() == BloqueoDeEscritura::ElDestinoEsUnDirectorio) {
        // Con permisos de superusuario las comprobaciones de permisos no se
        // aplican: el unico bloqueo que funciona en todas las plataformas es
        // un destino que sea un directorio, porque el rename de un archivo
        // sobre un directorio falla siempre.
        std::filesystem::remove(ruta, error);
        std::filesystem::create_directories(ruta, error);
        return BloqueoDeEscritura::ElDestinoEsUnDirectorio;
    }

    // El archivo destino queda de solo lectura: en Windows es lo que impide
    // el reemplazo (el `MoveFileEx` no puede reemplazar un archivo de solo
    // lectura) y en POSIX es una red mas.
    std::filesystem::permissions(ruta, std::filesystem::perms::owner_read,
                                 std::filesystem::perm_options::replace, error);

    // Y el directorio queda sin permiso de escritura: en POSIX el rename se
    // autoriza segun los permisos del directorio (no los del archivo), asi
    // que sin ese permiso el temporal ni siquiera se puede crear y el
    // guardado falla al abrirlo, sin tocar el destino.
    //
    // En Windows este permiso no cambia nada (el atributo de solo lectura de
    // un directorio no impide crear archivos adentro), pero deja el estado
    // del caso igual en las dos plataformas.
    std::filesystem::permissions(directorio,
                                 std::filesystem::perms::owner_read
                                     | std::filesystem::perms::owner_exec,
                                 std::filesystem::perm_options::replace, error);

    return BloqueoDeEscritura::ElDestinoEsUnArchivoDeSoloLectura;
}

// Restaura los permisos que `bloquearLaEscritura` toco.
//
// Sin esto, el directorio del caso quedaria sin permiso de escritura y la
// limpieza no podria borrarlo.
void restaurarLaEscritura(const std::filesystem::path& directorio, const std::string& ruta) {
    std::error_code error;
    std::filesystem::permissions(directorio, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, error);
    std::filesystem::permissions(ruta, std::filesystem::perms::owner_all,
                                 std::filesystem::perm_options::replace, error);
}

} // namespace

// ---------------------------------------------------------------------------
// Caso 1: el ciclo de vida completo de un turno
//
// Agendar, verificar en memoria, persistir, recargar del archivo con
// repositorios nuevos, verificar que sigue igual, cancelar, reprogramar,
// recargar otra vez y verificar el historial.
// ---------------------------------------------------------------------------

TEST_CASE("el ciclo de vida completo de un turno sobrevive al cierre y a la reapertura",
          integracion_ciclo_de_vida_del_turno) {
    const std::filesystem::path directorio = directorioDelCaso("ciclo_de_vida");
    const std::string rutaPacientes = rutaDePrueba(directorio, "pacientes.dat");
    const std::string rutaMedicos = rutaDePrueba(directorio, "medicos.dat");
    const std::string rutaTurnos = rutaDePrueba(directorio, "turnos.dat");

    // Las fechas son futuras: el servicio rechaza agendar en una fecha que ya
    // paso (por eso las fechas salen del reloj y no son fijas).
    const Fecha fechaDelTurno = fechaDesplazada(30);
    const Fecha fechaDeLaReprogramacion = fechaDesplazada(45);

    // Primera sesion: se agenda el turno y se deja todo escrito en el disco.
    {
        RepositorioPacientes pacientes(rutaPacientes);
        RepositorioMedicos medicos(rutaMedicos);
        RepositorioTurnos turnos(rutaTurnos);
        ServicioTurnos servicio(pacientes, medicos, turnos);

        pacientes.alta(pacienteDePrueba(1));
        medicos.alta(medicoDePrueba(1));

        // El id del turno lo propone el repositorio, no el usuario.
        const int idTurno = turnos.proximoId();
        servicio.agendar(turnoDePrueba(idTurno, 1, 1, fechaDelTurno, Hora(10, 0)));

        // El turno quedo en memoria...
        Turno enMemoria;
        CHECK(turnos.buscarPorId(idTurno, enMemoria));
        CHECK(enMemoria.getEstado() == EstadoTurno::Activo);
        CHECK(enMemoria.getFecha() == fechaDelTurno);
        CHECK(enMemoria.getHora() == Hora(10, 0));
        CHECK(enMemoria.getIdPaciente() == 1);
        CHECK(enMemoria.getIdMedico() == 1);
        CHECK(enMemoria.getIdTurnoReemplazante() == -1);
        CHECK(servicio.turnosVigentesDe(1).size() == 1);

        // ...y en el disco.
        turnos.persistir();
        CHECK(existeArchivo(rutaTurnos));
    }
    // Aca termina la primera sesion: los tres repositorios se destruyen. Lo
    // que sigue es un programa que arranca de cero.

    // Segunda sesion: el programa arranca de nuevo.
    //
    // Los repositorios son otros objetos, no los mismos con la memoria puesta:
    // lo que se carga sale del archivo, que es justamente lo que hay que
    // verificar.
    {
        RepositorioPacientes pacientes(rutaPacientes);
        RepositorioMedicos medicos(rutaMedicos);
        RepositorioTurnos turnos(rutaTurnos);
        ServicioTurnos servicio(pacientes, medicos, turnos);

        pacientes.cargar();
        medicos.cargar();
        turnos.cargar();

        // Lo que se cargo es igual a lo que se habia agendado.
        Turno recargado;
        CHECK(turnos.buscarPorId(1, recargado));
        CHECK(recargado.getEstado() == EstadoTurno::Activo);
        CHECK(recargado.getFecha() == fechaDelTurno);
        CHECK(recargado.getHora() == Hora(10, 0));
        CHECK(recargado.getDuracionMin() == 45);
        CHECK(recargado.getConsultorio() == "A-1");
        CHECK(recargado.getEspecialidad() == Especialidad::CARD);
        CHECK(recargado.getIdTurnoReemplazante() == -1);
        CHECK(servicio.turnosVigentesDe(1).size() == 1);

        // Se cancela el turno: el estado pasa a `Cancelado` y el turno no se
        // borra (la baja es logica).
        servicio.cancelar(1);

        Turno cancelado;
        CHECK(turnos.buscarPorId(1, cancelado));
        CHECK(cancelado.getEstado() == EstadoTurno::Cancelado);

        // Se agenda un segundo turno, para el camino de la reprogramacion.
        //
        // Por que hace falta un segundo turno: un turno cancelado no se
        // puede reprogramar (los estados `Cancelado` y `Reprogramado` son
        // finales). El ciclo completo necesita un turno activo para
        // reprogramar, asi que se agenda otro.
        const int idSegundo = turnos.proximoId();
        servicio.agendar(turnoDePrueba(idSegundo, 1, 1, fechaDelTurno, Hora(11, 0)));

        // Se reprograma el segundo turno: el viejo queda `Reprogramado`
        // apuntando al nuevo, y el nuevo queda activo.
        const int idNuevo = servicio.reprogramar(idSegundo, fechaDeLaReprogramacion, Hora(14, 0));
        CHECK(idNuevo == idSegundo + 1);

        turnos.persistir();
    }

    // Tercera sesion: se vuelve a abrir el programa y se mira el historial.
    {
        RepositorioPacientes pacientes(rutaPacientes);
        RepositorioMedicos medicos(rutaMedicos);
        RepositorioTurnos turnos(rutaTurnos);
        ServicioTurnos servicio(pacientes, medicos, turnos);

        pacientes.cargar();
        medicos.cargar();
        turnos.cargar();

        // El turno cancelado sigue cancelado.
        Turno cancelado;
        CHECK(turnos.buscarPorId(1, cancelado));
        CHECK(cancelado.getEstado() == EstadoTurno::Cancelado);

        // El turno reprogramado quedo `Reprogramado` y apunta al nuevo.
        Turno reprogramado;
        CHECK(turnos.buscarPorId(2, reprogramado));
        CHECK(reprogramado.getEstado() == EstadoTurno::Reprogramado);
        CHECK(reprogramado.getIdTurnoReemplazante() == 3);

        // Y el turno nuevo quedo activo, con la fecha y la hora nuevas.
        Turno nuevo;
        CHECK(turnos.buscarPorId(3, nuevo));
        CHECK(nuevo.getEstado() == EstadoTurno::Activo);
        CHECK(nuevo.getIdTurnoReemplazante() == -1);
        CHECK(nuevo.getFecha() == fechaDeLaReprogramacion);
        CHECK(nuevo.getHora() == Hora(14, 0));
        CHECK(nuevo.getIdPaciente() == 1);
        CHECK(nuevo.getIdMedico() == 1);
        CHECK(nuevo.getDuracionMin() == 45);
        CHECK(nuevo.getConsultorio() == "A-1");

        // El historial completo: tres turnos en el archivo y uno solo vigente.
        CHECK(turnos.contar() == 3);
        const std::vector<Turno> vigentes = servicio.turnosVigentesDe(1);
        CHECK(vigentes.size() == 1);
        if (!vigentes.empty()) {
            CHECK(vigentes.front().getId() == 3);
        }
    }
}

// ---------------------------------------------------------------------------
// Caso 2: el mismo ciclo con bajas logicas
//
// La baja de un paciente y la de un medico tienen que sobrevivir al cierre y
// al reinicio: las dos personas siguen existiendo, pero con `activo == false`,
// y los turnos que ya estaban siguen en el archivo.
// ---------------------------------------------------------------------------

TEST_CASE("las bajas logicas del paciente y del medico sobreviven al cierre y a la reapertura",
          integracion_bajas_logicas) {
    const std::filesystem::path directorio = directorioDelCaso("bajas_logicas");
    const std::string rutaPacientes = rutaDePrueba(directorio, "pacientes.dat");
    const std::string rutaMedicos = rutaDePrueba(directorio, "medicos.dat");
    const std::string rutaTurnos = rutaDePrueba(directorio, "turnos.dat");

    const Fecha fechaDelTurno = fechaDesplazada(30);

    // Primera sesion: se agenda el turno y despues se dan de baja las dos
    // personas.
    {
        RepositorioPacientes pacientes(rutaPacientes);
        RepositorioMedicos medicos(rutaMedicos);
        RepositorioTurnos turnos(rutaTurnos);
        ServicioTurnos servicio(pacientes, medicos, turnos);

        pacientes.alta(pacienteDePrueba(1));
        medicos.alta(medicoDePrueba(1));

        const int idTurno = turnos.proximoId();
        servicio.agendar(turnoDePrueba(idTurno, 1, 1, fechaDelTurno, Hora(10, 0)));

        // Las dos bajas son logicas: las personas siguen existiendo, con el
        // flag `activo` en false.
        pacientes.bajaLogica(1);
        medicos.bajaLogica(1);

        pacientes.persistir();
        medicos.persistir();
        turnos.persistir();
    }

    // Segunda sesion: el programa arranca de nuevo.
    {
        RepositorioPacientes pacientes(rutaPacientes);
        RepositorioMedicos medicos(rutaMedicos);
        RepositorioTurnos turnos(rutaTurnos);
        ServicioTurnos servicio(pacientes, medicos, turnos);

        pacientes.cargar();
        medicos.cargar();
        turnos.cargar();

        // El paciente sigue existiendo, pero inactivo.
        Paciente paciente;
        CHECK(pacientes.buscarPorId(1, paciente));
        CHECK(!paciente.esActivo());
        CHECK(pacientes.contar() == 1);

        // Y el medico tambien: la baja es logica en los dos casos.
        Medico medico;
        CHECK(medicos.buscarPorId(1, medico));
        CHECK(!medico.esActivo());
        CHECK(medicos.contar() == 1);

        // Los turnos no se borran: el turno que ya estaba sigue en el
        // archivo, con su estado intacto.
        Turno turno;
        CHECK(turnos.buscarPorId(1, turno));
        CHECK(turno.getEstado() == EstadoTurno::Activo);
        CHECK(turno.getFecha() == fechaDelTurno);
        CHECK(turno.getHora() == Hora(10, 0));
        CHECK(turnos.contar() == 1);

        // Y las reglas de negocio siguen valiendo despues de recargar: un
        // paciente dado de baja no puede tener turnos nuevos.
        const int idNuevo = turnos.proximoId();
        const ErrorCapturado error = capturarError(
            [&] { servicio.agendar(turnoDePrueba(idNuevo, 1, 1, fechaDelTurno, Hora(11, 0))); });
        CHECK(error.lanzo);
        CHECK(!error.lanzoOtraExcepcion);
        CHECK(error.campo == "paciente");

        // Y el rechazo no dejo nada a medias en el repositorio.
        CHECK(turnos.contar() == 1);
    }
}

// ---------------------------------------------------------------------------
// Caso 3: el error de persistencia
//
// Si el archivo no se puede escribir, la aplicacion no puede quedar con la
// memoria y el disco desincronizados. Lo que se verifica es que:
//
//   - el fallo se propaga (no se traga la excepcion);
//   - el archivo de destino que ya existia queda intacto, byte a byte;
//   - no queda ningun `.tmp` tirado;
//   - la memoria del repositorio que fallo no quedo corrupta: las entidades
//     que tenia siguen ahi y `listar()` sigue funcionando.
// ---------------------------------------------------------------------------

TEST_CASE("si el archivo no se puede escribir el fallo se propaga y la memoria y el disco no quedan desincronizados",
          integracion_error_de_persistencia) {
    const std::filesystem::path directorio = directorioDelCaso("persistencia_fallo");
    const std::string rutaPacientes = rutaDePrueba(directorio, "pacientes.dat");
    const std::string rutaMedicos = rutaDePrueba(directorio, "medicos.dat");
    const std::string rutaTurnos = rutaDePrueba(directorio, "turnos.dat");

    const Fecha fechaDelTurno = fechaDesplazada(30);

    RepositorioPacientes pacientes(rutaPacientes);
    RepositorioMedicos medicos(rutaMedicos);
    RepositorioTurnos turnos(rutaTurnos);
    ServicioTurnos servicio(pacientes, medicos, turnos);

    // Se deja el mundo en marcha: las personas y un turno agendado y
    // escrito en el disco.
    pacientes.alta(pacienteDePrueba(1));
    medicos.alta(medicoDePrueba(1));
    servicio.agendar(turnoDePrueba(turnos.proximoId(), 1, 1, fechaDelTurno, Hora(10, 0)));

    const std::string contenidoOriginal = leerArchivo(rutaTurnos);
    CHECK(!contenidoOriginal.empty());

    // Se inyecta el fallo de escritura.
    const BloqueoDeEscritura bloqueo = bloquearLaEscritura(directorio, rutaTurnos);

    // El agendamiento falla y el fallo se propaga: no se traga la excepcion
    // en ningun punto de la cadena (ni el repositorio ni el servicio).
    const int idNuevo = turnos.proximoId();
    const Fallo fallo = capturarTipo<ExcepcionSistema>(
        [&] { servicio.agendar(turnoDePrueba(idNuevo, 1, 1, fechaDelTurno, Hora(11, 0))); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    // Y el mensaje dice que archivo fallo.
    CHECK(fallo.mensaje.find("turnos.dat") != std::string::npos);

    // El destino quedo intacto.
    if (bloqueo == BloqueoDeEscritura::ElDestinoEsUnDirectorio) {
        // Este camino solo se usa cuando el proceso corre como superusuario
        // (los permisos no se aplican): el destino es un directorio, asi que
        // lo que se verifica es que el rename no lo toco.
        std::error_code error;
        CHECK(std::filesystem::is_directory(rutaTurnos, error));
    } else {
        // El archivo que ya existia sigue con los mismos bytes: la
        // escritura que fallo no lo toco.
        CHECK(leerArchivo(rutaTurnos) == contenidoOriginal);
    }

    // No quedo ningun `.tmp` tirado: la escritura es atomica y el temporal
    // se borra cuando la publicacion falla.
    CHECK(temporalesEnElDirectorio(directorio).empty());

    // Y la MUTACION SE DESHIZO: el turno que no se pudo escribir tampoco
    // quedo en la memoria.
    //
    // Si se hubiera quedado, la memoria y el disco quedarian
    // DESINCRONIZADOS: el turno existiria en memoria pero no en el archivo,
    // y el proximo `persistir()` lo escribiria como si fuera nuevo. El
    // repositorio deshace la mutacion cuando la escritura falla, asi que los
    // dos quedan de acuerdo.
    CHECK(turnos.contar() == 1);
    CHECK(turnos.listar().size() == 1);

    Turno primero;
    CHECK(turnos.buscarPorId(1, primero));
    CHECK(primero.getEstado() == EstadoTurno::Activo);
    CHECK(primero.getFecha() == fechaDelTurno);

    // El turno que fallo no esta en la memoria.
    Turno segundo;
    CHECK(!turnos.buscarPorId(2, segundo));

    // EL INVARIANTE DEL CASO: la memoria y el disco dicen lo mismo.
    //
    // Se relee el archivo (como si el programa se cerrara y se abriera de
    // nuevo) y se compara con lo que quedo en memoria: los mismos ids, en el
    // mismo orden y con el mismo estado. Si el repositorio se hubiera
    // quedado con la entidad que no se pudo escribir, los dos contarian
    // distinto y este `CHECK` fallaria.
    const std::vector<Turno> enDisco =
        ArchivoTexto<Turno>(rutaTurnos, "TURNOS").cargar();
    CHECK(enDisco.size() == turnos.contar());
    const std::vector<Turno> enMemoria = turnos.listar();
    CHECK(enMemoria.size() == enDisco.size());
    for (std::size_t i = 0; i < enMemoria.size() && i < enDisco.size(); ++i) {
        CHECK(enMemoria[i].getId() == enDisco[i].getId());
        CHECK(enMemoria[i].getEstado() == enDisco[i].getEstado());
        CHECK(enMemoria[i].getFecha() == enDisco[i].getFecha());
        CHECK(enMemoria[i].getHora() == enDisco[i].getHora());
    }

    // Y el repositorio sigue siendo usable: se puede seguir consultando, y
    // el id que se consumio con el turno fallido se vuelve a ofrecer (la
    // mutacion se deshizo, asi que ese id sigue libre).
    CHECK(turnos.proximoId() == 2);

    // Se restauran los permisos para que la limpieza pueda borrar el
    // directorio del caso.
    restaurarLaEscritura(directorio, rutaTurnos);
}
