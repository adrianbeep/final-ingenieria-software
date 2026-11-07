#include "ui/Pantallas/Reportes.h"

#include "domain/Especialidad.h"
#include "exceptions/ExcepcionesSistema.h"
#include "ui/CrudPersonas.h"
#include "ui/NombresDePersonas.h"
#include "ui/Semana.h"
#include "utils/Fecha.h"
#include "utils/Utils.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// La pantalla de REPORTES.
//
// Lo que esta en el header es el POR QUE; lo que esta aca es el COMO.
//
// EL FLUJO DE UN REPORTE
//
//   menu -> (campo | selector) -> tabla con el total
//
// La mayoria de los reportes piden UN DATO (una fecha, un id) y uno elige
// una ESPECIALIDAD de una lista. En los dos casos el camino es el mismo: se
// responde la pregunta y se ve el resultado.
//
// LA PANTALLA NO REIMPLEMENTA NINGUNA REGLA
//
// Las consultas son del SERVICIO (que devuelve los turnos de cualquier
// estado) y los nombres salen del REPOSITORIO. La pantalla solo arma la
// tabla, cuenta las filas y avisa el resultado.
//
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {
namespace {

// ---------------------------------------------------------------------------
// LAS COLUMNAS DE LOS REPORTES DE TURNOS
//
// SON LAS DE LA LISTA, MENOS LA DIMENSION POR LA QUE SE FILTRA
//
// La pantalla de turnos enfoca su lista en seis columnas: `id`, `fecha`,
// `hora`, `paciente`, `medico` y `estado`. Un reporte es la MISMA lista con
// UNA dimension menos: la que el reporte filtra.
//
// Un reporte "por paciente" no repite el nombre del paciente en cada fila:
// es el dato que el usuario acaba de elegir, y repetirlo en veinte filas
// empuja la tabla y no agrega nada. Lo mismo con el medico y con la fecha.
//
// POR QUE NO SE USA LA LISTA DE LA PANTALLA DE TURNOS
//
// La de turnos vive en un namespace anonimo de su .cpp: no se puede leer
// desde aca. Y no haria falta: las columnas de un reporte NO son las de la
// lista (le falta una), asi que estan escritas aqui de nuevo. Lo que SI se
// comparte es la resolucion de los NOMBRES (`ui::nombreDelPaciente` y
// `ui::nombreDelMedico`), que viven en un header propio para que no haya dos
// reglas que se puedan desincronizar.
// ---------------------------------------------------------------------------

// LAS COLUMNAS DEL REPORTE POR FECHA
//
// La fecha es la dimension que se filtra: no se repite en cada fila.
const std::vector<std::string>& cabecerasPorFecha() {
    static const std::vector<std::string> cabeceras = {"id", "hora", "paciente", "medico",
                                                        "estado"};
    return cabeceras;
}

// LAS COLUMNAS DEL REPORTE POR PACIENTE
//
// El paciente es la dimension que se filtra.
const std::vector<std::string>& cabecerasPorPaciente() {
    static const std::vector<std::string> cabeceras = {"id", "fecha", "hora", "medico", "estado"};
    return cabeceras;
}

// LAS COLUMNAS DEL REPORTE POR MEDICO
//
// El medico es la dimension que se filtra.
const std::vector<std::string>& cabecerasPorMedico() {
    static const std::vector<std::string> cabeceras = {"id", "fecha", "hora", "paciente", "estado"};
    return cabeceras;
}

// LAS COLUMNAS DEL REPORTE POR ESPECIALIDAD
//
// La especialidad NO es una de las seis columnas de la lista (es la del
// medico, por la regla 2b), asi que este reporte las tiene a las seis.
const std::vector<std::string>& cabecerasPorEspecialidad() {
    static const std::vector<std::string> cabeceras = {"id",      "fecha", "hora",
                                                        "paciente", "medico", "estado"};
    return cabeceras;
}

// LAS COLUMNAS DEL REPORTE DE OCUPACION
//
// No son las de un turno: son las de un DIA de la agenda. La ultima es el
// porcentaje, asi que lleva el signo adelante ("% ocupacion").
const std::vector<std::string>& cabecerasDeOcupacion() {
    static const std::vector<std::string> cabeceras = {"fecha", "dia", "turnos",
                                                        "horas ocupadas", "% ocupacion"};
    return cabeceras;
}

// ---------------------------------------------------------------------------
// LA FILA DE UN TURNO EN CADA REPORTE
//
// Las celdas van en el MISMO orden que las cabeceras del reporte: si no, la
// tabla queda desalineada.
//
// EL NOMBRE COMPLETO
//
// La celda del paciente (o del medico) dice el nombre y el apellido: es lo
// que el usuario reconoce. El turno guarda solo el id, asi que el nombre lo
// resuelve el repositorio (ver `ui/NombresDePersonas.h`).
// ---------------------------------------------------------------------------
std::vector<std::string> filaPorFecha(const Turno& turno, RepositorioPacientes& pacientes,
                                      RepositorioMedicos& medicos) {
    return {std::to_string(turno.getId()),
            turno.getHora().aTexto(),
            nombreDelPaciente(turno.getIdPaciente(), pacientes),
            nombreDelMedico(turno.getIdMedico(), medicos),
            estadoATexto(turno.getEstado())};
}

std::vector<std::string> filaPorPaciente(const Turno& turno, RepositorioMedicos& medicos) {
    return {std::to_string(turno.getId()),
            turno.getFecha().aTexto(),
            turno.getHora().aTexto(),
            nombreDelMedico(turno.getIdMedico(), medicos),
            estadoATexto(turno.getEstado())};
}

std::vector<std::string> filaPorMedico(const Turno& turno, RepositorioPacientes& pacientes) {
    return {std::to_string(turno.getId()),
            turno.getFecha().aTexto(),
            turno.getHora().aTexto(),
            nombreDelPaciente(turno.getIdPaciente(), pacientes),
            estadoATexto(turno.getEstado())};
}

std::vector<std::string> filaPorEspecialidad(const Turno& turno,
                                              RepositorioPacientes& pacientes,
                                              RepositorioMedicos& medicos) {
    return {std::to_string(turno.getId()),
            turno.getFecha().aTexto(),
            turno.getHora().aTexto(),
            nombreDelPaciente(turno.getIdPaciente(), pacientes),
            nombreDelMedico(turno.getIdMedico(), medicos),
            estadoATexto(turno.getEstado())};
}

// ---------------------------------------------------------------------------
// LOS VALIDADORES DE LOS CAMPOS
//
// LA REGLA ES LA DEL DOMINIO, NO LA DE LA PANTALLA
//
// El campo del formulario decide la FORMA del dato (que la fecha se vea como
// AAAA-MM-DD, que el id sea un numero). El RANGO (que la fecha exista en el
// calendario, que el paciente exista) NO lo decide el campo: lo decide el
// dominio, y el error sale como MENSAJE (`ERR <mensaje>`), que es el canal
// de los errores de la aplicacion.
//
// POR QUE EL ID SE VALIDA CONTRA EL REPOSITORIO
//
// La consulta del servicio devuelve un vector VACIO si el id no existe, y
// eso taparia el error de quien escribio el id. La pantalla lo valida antes
// de preguntar: es lo que hace el motor CRUD con sus ids, y es lo que el
// propio servicio deja escrito ("la UI valida el id antes de preguntar").
// ---------------------------------------------------------------------------

// La fecha tiene que tener FORMA AAAA-MM-DD.
//
// Que la fecha EXISTA en el calendario no lo decide el campo: lo decide
// `Fecha::desdeTexto`, que es del dominio.
Campo::Validador validadorDeFecha() {
    return [](const std::string& texto) -> std::string {
        if (!Utils::esFormatoFecha(texto)) {
            return "se esperaba AAAA-MM-DD";
        }
        return "";
    };
}

// El id de un PACIENTE tiene que existir.
//
// Un id es un entero POSITIVO. Que exista es otra pregunta.
Campo::Validador validadorDeIdPaciente(RepositorioPacientes& pacientes) {
    return [&pacientes](const std::string& texto) -> std::string {
        const int id = idDesdeTexto(texto);
        if (id < 0) {
            return "el id tiene que ser un numero entero mayor que cero";
        }
        Paciente paciente;
        if (!pacientes.buscarPorId(id, paciente)) {
            return "no existe paciente #" + texto;
        }
        return "";
    };
}

// El id de un MEDICO tiene que existir.
Campo::Validador validadorDeIdMedico(RepositorioMedicos& medicos) {
    return [&medicos](const std::string& texto) -> std::string {
        const int id = idDesdeTexto(texto);
        if (id < 0) {
            return "el id tiene que ser un numero entero mayor que cero";
        }
        Medico medico;
        if (!medicos.buscarPorId(id, medico)) {
            return "no existe medico #" + texto;
        }
        return "";
    };
}

// ---------------------------------------------------------------------------
// LA OCUPACION DE UN DIA
//
// QUE SE MIDE
//
// La ocupacion es el tiempo que la agenda del dia tiene RESERVADO: los
// minutos que los turnos activos ocupan.
//
// LAS TRES DECISIONES DEL METRICO
//
// 1. ES LA UNION DE LOS INTERVALOS, NO LA SUMA DE LAS DURACIONES
//
//    Dos medicos que atienden a la vez ocupan el calendario UNA vez, no
//    dos: la agenda es el conjunto de los intervalos reservados, y un
//    intervalo contado dos veces es un intervalo libre contado como
//    ocupado. Por eso los intervalos se MERGEAN antes de medir.
//
// 2. SOLO OCUPAN LOS TURNOS ACTIVOS
//
//    Un turno cancelado o reprogramado NO tiene hora reservada: libero el
//    horario. Por eso la ocupacion se mide sobre los turnos `Activo`. (Los
//    cuatro reportes de listado muestran TODOS los estados: un reporte
//    tiene que poder mostrar la historia de la agenda. Esta es una
//    MEDIDA de la agenda, no un listado.)
//
// 3. EL PORCENTAJE ES SOBRE LA VENTANA DE ATENCION
//
//    El denominador es el horario de atencion del dominio (de las 08:00 a
//    las 22:00): 14 horas, 840 minutos. Un dia sin nada reservado es 0%.
// ---------------------------------------------------------------------------

// Un intervalo reservado: de tal minuto al tal minuto (desde la medianoche).
struct Intervalo {
    int inicio;
    int fin;
};

// Lo que la agenda de un dia tiene reservado.
struct Ocupacion {
    int turnos = 0;
    int minutos = 0;
};

// Los minutos de la VENTANA DE ATENCION (08:00 a 22:00).
int minutosDeLaVentana() {
    return (Hora::HORA_FIN - Hora::HORA_INICIO) * Hora::MINUTOS_POR_HORA;
}

// LA OCUPACION DE UN DIA
//
// Son los turnos que reservan la agenda y los minutos que la union de sus
// intervalos ocupa.
Ocupacion ocupacionDe(const std::vector<Turno>& turnos) {
    Ocupacion resultado;

    std::vector<Intervalo> intervalos;
    intervalos.reserve(turnos.size());

    for (const Turno& turno : turnos) {
        // SOLO OCUPAN LOS TURNOS ACTIVOS
        if (turno.getEstado() != EstadoTurno::Activo) {
            continue;
        }
        ++resultado.turnos;
        const int inicio = turno.getHora().minutosDesdeMedianoche();
        intervalos.push_back(Intervalo{inicio, inicio + turno.getDuracionMin()});
    }

    // LA UNION DE LOS INTERVALOS
    //
    // Se ordenan por arranque y se van Fusionando: dos intervalos que se
    // tocan (o que se pisan) son uno solo.
    std::sort(intervalos.begin(), intervalos.end(),
              [](const Intervalo& uno, const Intervalo& otro) {
                  if (uno.inicio != otro.inicio) {
                      return uno.inicio < otro.inicio;
                  }
                  return uno.fin < otro.fin;
              });

    int inicioActual = -1;
    int finActual = -1;
    for (const Intervalo& intervalo : intervalos) {
        if (inicioActual < 0) {
            inicioActual = intervalo.inicio;
            finActual = intervalo.fin;
            continue;
        }
        if (intervalo.inicio <= finActual) {
            // SE PISAN (o se tocan): el intervalo sigue siendo uno solo.
            if (intervalo.fin > finActual) {
                finActual = intervalo.fin;
            }
            continue;
        }
        // hay un hueco: el intervalo anterior ya termino.
        resultado.minutos += finActual - inicioActual;
        inicioActual = intervalo.inicio;
        finActual = intervalo.fin;
    }
    if (inicioActual >= 0) {
        resultado.minutos += finActual - inicioActual;
    }

    return resultado;
}

// LAS HORAS OCUPADAS, CON UN DECIMAL
//
// Se hace con ENTEROS: el numero de decimas de hora es `minutos * 10 / 60`,
// redondeado al mas cercano. (Un turno de 45 minutos ocupa "0.8" horas.)
std::string horasOcupadas(int minutos) {
    const int decimas = (minutos * 10 + 30) / 60;
    return std::to_string(decimas / 10) + "." + std::to_string(decimas % 10);
}

// EL PORCENTAJE DE OCUPACION, CON UN DECIMAL
//
// `ventana` es la cantidad de minutos que se toman como el cien por ciento:
// la ventana de atencion de un dia (840) o la de la semana (5880).
std::string porcentajeDe(int minutos, int ventana) {
    if (ventana <= 0) {
        return "0.0%";
    }
    const int decimas = (minutos * 1000 + ventana / 2) / ventana;
    return std::to_string(decimas / 10) + "." + std::to_string(decimas % 10) + "%";
}

} // namespace

// ---------------------------------------------------------------------------
// Construccion
// ---------------------------------------------------------------------------

PantallaReportes::PantallaReportes(Aplicacion& aplicacion,
                                   RepositorioPacientes& pacientes,
                                   RepositorioMedicos& medicos,
                                   ServicioTurnos& servicioTurnos)
    : _aplicacion(aplicacion),
      _pacientes(pacientes),
      _medicos(medicos),
      _servicioTurnos(servicioTurnos),
      _titulo("REPORTES"),
      _menu("Reportes") {
    // EL MENU DEL MODULO
    //
    // El atajo es el numero que se escribe (decision D28) y el '0' es
    // "Volver" en todos los niveles.
    _menu.agregar('1', "Turnos por fecha", [this]() { iniciarTurnosPorFecha(); });
    _menu.agregar('2', "Turnos por paciente", [this]() { iniciarTurnosPorPaciente(); });
    _menu.agregar('3', "Turnos por medico", [this]() { iniciarTurnosPorMedico(); });
    _menu.agregar('4', "Turnos por especialidad", [this]() { abrirSelector(); });
    _menu.agregar('5', "Ocupacion por semana", [this]() { iniciarOcupacion(); });
    _menu.agregar('0', "Volver", [this]() { volverAtras(); });
}

std::string PantallaReportes::titulo() const {
    return _titulo;
}

// ---------------------------------------------------------------------------
// La navegacion
// ---------------------------------------------------------------------------

void PantallaReportes::irA(Vista vista) {
    _vista = vista;
}

void PantallaReportes::volverAtras() {
    // EL "VOLVER" SUBE UN NIVEL
    //
    //   - desde el menu de la pantalla, se sale de la pantalla (el menu
    //     principal esta en la pila de la aplicacion);
    //   - desde cualquier otra vista, se vuelve al menu del modulo.
    //
    // El selector NO es un nivel aparte: es un PASO de la pregunta del
    // reporte, asi que el `Escape` lo deja en el menu (no hay formulario
    // al que volver: la especialidad es la unica pregunta).
    switch (_vista) {
        case Vista::Menu:
            _aplicacion.volver();
            return;
        default:
            irA(Vista::Menu);
            return;
    }
}

bool PantallaReportes::esTeclaVolver(const Evento& evento) const {
    return evento.tipo == TipoTecla::Caracter && evento.caracter == "0";
}

bool PantallaReportes::esTeclaEscape(const Evento& evento) const {
    return evento.tipo == TipoTecla::Escape;
}

// EL '0' ES "VOLVER" SOLO SI EL CAMPO ESTA VACIO
//
// Con algo escrito el '0' es un DATO (un id puede ser 10) y se lo queda el
// campo. Es la misma regla que usan el motor CRUD y la pantalla de turnos.
bool PantallaReportes::esTeclaVolverConCampoVacio(const Evento& evento,
                                                   const Campo& campo) const {
    return esTeclaVolver(evento) && campo.texto().empty();
}

// ---------------------------------------------------------------------------
// Los manejadores de cada vista
// ---------------------------------------------------------------------------

bool PantallaReportes::manejar(const Evento& evento) {
    switch (_vista) {
        case Vista::Menu:
            return manejarMenu(evento);
        case Vista::Campo:
            return manejarCampo(evento);
        case Vista::Selector:
            return manejarSelector(evento);
        case Vista::Resultado:
            return manejarLectura(evento);
        case Vista::Ocupacion:
            return manejarLectura(evento);
    }
    return false;
}

bool PantallaReportes::manejarMenu(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` NO SE CONSUME EN EL MENU
    //
    // El menu de la pantalla es el NIVEL MAS ALTO de esta pantalla: desde
    // ahi el `Escape` tiene que sacar la pantalla ENTERA de la pila, y eso
    // lo hace la aplicacion.
    return _menu.aplicarEvento(evento);
}

bool PantallaReportes::manejarCampo(const Evento& evento) {
    if (!_campo) {
        return false;
    }

    if (evento.tipo == TipoTecla::Enter) {
        // El campo valida y, si el valor sirve, se confirma la vista.
        _campo->aplicarEvento(evento);
        if (!_campo->confirmado()) {
            // El campo ya mostro el error debajo: no se sigue adelante.
            return true;
        }
        confirmarCampo();
        return true;
    }

    // EL `Escape` SUBE UN NIVEL
    //
    // El `Escape` es una tecla de navegacion, no un dato: se atiende aunque
    // el campo tenga texto escrito.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }

    // EL '0' ES "VOLVER" SOLO SI EL CAMPO ESTA VACIO
    if (esTeclaVolverConCampoVacio(evento, *_campo)) {
        volverAtras();
        return true;
    }

    return _campo->aplicarEvento(evento);
}

bool PantallaReportes::manejarSelector(const Evento& evento) {
    if (!_selector) {
        return false;
    }

    // EL `Escape` VUELVE AL MENU
    //
    // El selector es un paso DE LA PREGUNTA del reporte, no un nivel del
    // menu: el `Escape` lo deja en el menu del modulo.
    if (esTeclaEscape(evento)) {
        irA(Vista::Menu);
        return true;
    }

    // EL RATON MUEVE EL RESALTADO, NO CONFIRMA
    //
    // Un clic elige la fila, pero la confirmacion es el Enter: un clic
    // accidental no cambia el reporte que se esta viendo.
    if (evento.esMouse()) {
        if (!evento.esClic()) {
            return false;
        }
        const int indice = _selector->opcionEnFila(evento.y, _selector->filaInicial());
        if (indice < 0) {
            // El clic cae fuera de las opciones: no se consume.
            return false;
        }
        _selector->seleccionar(indice);
        return true;
    }

    // EL '0' NO ES "VOLVER" ACA
    //
    // En el selector el '0' es un DIGITO: es el numero de una opcion, y
    // como el catalogo tiene doce opciones (y no diez), el '0' no puede ser
    // un atajo de vuelta. Si no existe, el selector muestra el error.
    if (!_selector->aplicarEvento(evento)) {
        return false;
    }

    // EL ENTER CONFIRMA LA ELECCION
    if (evento.tipo == TipoTecla::Enter && _selector->error().empty()) {
        confirmarSeleccion();
    }
    return true;
}

bool PantallaReportes::manejarLectura(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE AL MENU DE LA PANTALLA
    //
    // La vista del resultado no tiene ningun campo: el `Escape` sube un
    // nivel, al menu.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    // Las vistas de solo lectura no consumen nada mas.
    return false;
}

// ---------------------------------------------------------------------------
// El arranque de cada reporte
// ---------------------------------------------------------------------------

void PantallaReportes::iniciarTurnosPorFecha() {
    _reporte = Reporte::TurnosPorFecha;
    _campo = std::make_unique<Campo>("Fecha", "", validadorDeFecha());
    irA(Vista::Campo);
}

void PantallaReportes::iniciarTurnosPorPaciente() {
    _reporte = Reporte::TurnosPorPaciente;
    _campo = std::make_unique<Campo>("Id del paciente", "", validadorDeIdPaciente(_pacientes));
    irA(Vista::Campo);
}

void PantallaReportes::iniciarTurnosPorMedico() {
    _reporte = Reporte::TurnosPorMedico;
    _campo = std::make_unique<Campo>("Id del medico", "", validadorDeIdMedico(_medicos));
    irA(Vista::Campo);
}

void PantallaReportes::iniciarOcupacion() {
    // LA SEMANA SE PIDE CON UNA FECHA
    //
    // La fecha que se escribe es un dia cualquiera de la semana: el reporte
    // arma la semana de LUNES a DOMINGO que lo contiene.
    _reporte = Reporte::OcupacionPorSemana;
    _campo = std::make_unique<Campo>("Fecha", "", validadorDeFecha());
    irA(Vista::Campo);
}

void PantallaReportes::abrirSelector() {
    // LA ESPECIALIDAD SE ELIGE DE LAS DOCE DEL CATALOGO
    //
    // Es el mismo criterio que usa la pantalla de turnos: la especialidad no
    // se escribe, se elige (decision 1).
    std::vector<std::string> opciones;
    for (const Especialidad& especialidad : todasLasEspecialidades()) {
        opciones.push_back(descripcionDe(especialidad));
    }

    _selector = std::make_unique<Selector>("especialidad", std::move(opciones));
    _reporte = Reporte::TurnosPorEspecialidad;
    irA(Vista::Selector);
}

// ---------------------------------------------------------------------------
// La confirmacion
// ---------------------------------------------------------------------------

void PantallaReportes::confirmarCampo() {
    _filas.clear();

    switch (_reporte) {
        case Reporte::TurnosPorFecha: {
            // LA FECHA TIENE QUE EXISTIR EN EL CALENDARIO
            //
            // El campo solo valido la FORMA ("AAAA-MM-DD"): que la fecha
            // exista es otra pregunta, y la responde el DOMINIO. Su mensaje
            // sale con el formato de error de la aplicacion (`ERR ...`) y el
            // usuario vuelve a escribir la fecha.
            Fecha fecha;
            try {
                fecha = Fecha::desdeTexto(_campo->valor());
            } catch (const DatoInvalidoException& error) {
                avisar(error.what());
                return;
            }
            for (const Turno& turno : _servicioTurnos.turnosDeFecha(fecha)) {
                _filas.push_back(filaPorFecha(turno, _pacientes, _medicos));
            }
            _cabeceras = cabecerasPorFecha();
            _mensajeVacio = "No hay turnos para esa fecha. 0: volver al menu.";
            break;
        }

        case Reporte::TurnosPorPaciente: {
            const int id = idDesdeTexto(_campo->valor());
            for (const Turno& turno : _servicioTurnos.turnosDePaciente(id)) {
                _filas.push_back(filaPorPaciente(turno, _medicos));
            }
            _cabeceras = cabecerasPorPaciente();
            _mensajeVacio = "No hay turnos para ese paciente. 0: volver al menu.";
            break;
        }

        case Reporte::TurnosPorMedico: {
            const int id = idDesdeTexto(_campo->valor());
            for (const Turno& turno : _servicioTurnos.turnosDeMedico(id)) {
                _filas.push_back(filaPorMedico(turno, _pacientes));
            }
            _cabeceras = cabecerasPorMedico();
            _mensajeVacio = "No hay turnos para ese medico. 0: volver al menu.";
            break;
        }

        case Reporte::TurnosPorEspecialidad:
            // No deberia llegar aca: la especialidad no se escribe, se elige.
            irA(Vista::Menu);
            return;

        case Reporte::OcupacionPorSemana: {
            // LA FECHA TIENE QUE EXISTIR EN EL CALENDARIO
            //
            // El campo solo valido la FORMA ("AAAA-MM-DD"): que la fecha
            // exista es otra pregunta, y la responde el DOMINIO.
            Fecha fecha;
            try {
                fecha = Fecha::desdeTexto(_campo->valor());
            } catch (const DatoInvalidoException& error) {
                avisar(error.what());
                return;
            }
            armarOcupacion(fecha);
            avisarExito(mensajeDeOcupacion());
            irA(Vista::Ocupacion);
            return;
        }
    }

    avisarExito(std::to_string(_filas.size()) + " turno(s).");
    irA(Vista::Resultado);
}

// ---------------------------------------------------------------------------
// La ocupacion de la semana
// ---------------------------------------------------------------------------

void PantallaReportes::armarOcupacion(const Fecha& fecha) {
    // LA SEMANA ES LA DEL LUNES AL DOMINGO QUE CONTIENE A LA FECHA
    //
    // La fecha escrita es un dia cualquiera de la semana: el reporte arma la
    // semana entera que lo contiene (ver `ui/Semana.h`).
    const std::vector<Fecha> dias = diasDeLaSemana(fecha);

    const int ventanaDelDia = minutosDeLaVentana();
    const int ventanaDeLaSemana = ventanaDelDia * 7;

    _filas.clear();
    int totalTurnos = 0;
    int totalMinutos = 0;

    for (const Fecha& dia : dias) {
        const Ocupacion ocupacion = ocupacionDe(_servicioTurnos.turnosDeFecha(dia));
        totalTurnos += ocupacion.turnos;
        totalMinutos += ocupacion.minutos;

        _filas.push_back({dia.aTexto(),
                          Fecha::nombreDiaSemana(dia.diaSemana()),
                          std::to_string(ocupacion.turnos),
                          horasOcupadas(ocupacion.minutos),
                          porcentajeDe(ocupacion.minutos, ventanaDelDia)});
    }

    // LA FILA DEL TOTAL DE LA SEMANA
    //
    // Es la suma de los siete dias: los turnos, los minutos y el porcentaje
    // sobre la ventana de la semana entera (siete ventanas de atencion).
    _filas.push_back({"TOTAL",
                      "",
                      std::to_string(totalTurnos),
                      horasOcupadas(totalMinutos),
                      porcentajeDe(totalMinutos, ventanaDeLaSemana)});

    _cabeceras = cabecerasDeOcupacion();
    _mensajeVacio = "No hay turnos en esa semana. 0: volver al menu.";
}

std::string PantallaReportes::mensajeDeOcupacion() const {
    // EL MENSAJE DICE QUE SEMANA SE REPORTO
    //
    // La ultima fila de la tabla es el total de los siete dias: el mensaje
    // lo resume y dice de que semana es, para que el usuario sepa que el
    // reporte hablo de la semana que queria.
    if (_filas.empty()) {
        return "0 turno(s).";
    }
    const std::vector<std::string>& total = _filas.back();
    if (total.size() < 5) {
        return "0 turno(s).";
    }
    return "Semana del " + _filas.front().front() + ": " + total[2] + " turno(s), " + total[3]
        + " horas ocupadas (" + total[4] + ").";
}

void PantallaReportes::confirmarSeleccion() {
    if (!_selector) {
        irA(Vista::Menu);
        return;
    }
    const int indice = _selector->seleccion();
    if (indice < 0 || indice >= _selector->cantidad()) {
        // No deberia pasar: es un error de programacion de la pantalla.
        irA(Vista::Menu);
        return;
    }

    const Especialidad especialidad = todasLasEspecialidades()[static_cast<std::size_t>(indice)];

    _filas.clear();
    for (const Turno& turno : _servicioTurnos.turnosDeEspecialidad(especialidad)) {
        _filas.push_back(filaPorEspecialidad(turno, _pacientes, _medicos));
    }
    _cabeceras = cabecerasPorEspecialidad();
    _mensajeVacio = "No hay turnos para esa especialidad. 0: volver al menu.";

    avisarExito(std::to_string(_filas.size()) + " turno(s).");
    irA(Vista::Resultado);
}

// ---------------------------------------------------------------------------
// Los mensajes
// ---------------------------------------------------------------------------

void PantallaReportes::avisar(const std::string& texto) {
    // El mensaje vive en el CONTEXTO: es lo que la pantalla anterior va a
    // mostrar cuando el usuario vuelva.
    _aplicacion.contexto().mostrarMensaje(Mensaje::error(texto));
}

void PantallaReportes::avisarExito(const std::string& texto) {
    _aplicacion.contexto().mostrarMensaje(Mensaje::exito(texto));
}

const Mensaje& PantallaReportes::mensajeActual() const {
    return _aplicacion.contexto().mensaje();
}

// ---------------------------------------------------------------------------
// El dibujo
// ---------------------------------------------------------------------------

ftxui::Element PantallaReportes::encabezado() const {
    ftxui::Element elemento = ftxui::text(_aplicacion.titulo() + " - " + _titulo);
    if (const auto color = _tema.colorDe(Rol::Titulo)) {
        elemento = elemento | ftxui::color(aFtxui(*color)) | ftxui::bold;
    }
    return elemento;
}

ftxui::Element PantallaReportes::lineaOpcion(int indice) const {
    const std::string atajo(1, _menu.atajoDe(indice));
    ftxui::Element elemento =
        ftxui::text("  " + atajo + ". " + _menu.etiquetas()[static_cast<std::size_t>(indice)]);
    if (indice == _menu.seleccion()) {
        // El resaltado de la opcion seleccionada. `inverted` funciona SIN
        // color, asi que el menu se ve igual con `NO_COLOR` definido.
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

ftxui::Element PantallaReportes::lineaOpcionSelector(int indice) const {
    ftxui::Element elemento = ftxui::text(_selector->lineaDe(indice));
    if (indice == _selector->seleccion()) {
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

ftxui::Element PantallaReportes::lineaNumeroSelector() const {
    // EL NUMERO QUE SE ESTA ESCRIBIENDO
    //
    // Va a la vista porque es lo que el usuario esta TECLEANDO: si no se
    // mostrara, escribir "12" seria indistinguible de no escribir nada.
    return ftxui::text("  Numero: " + _selector->numeroDigitado() + "|");
}

ftxui::Element PantallaReportes::lineaCampo(const Campo& campo) const {
    ftxui::Element elemento =
        ftxui::text("  " + campo.etiqueta() + ": " + campo.textoEnPantalla());
    elemento = elemento | ftxui::inverted;
    return elemento;
}

ftxui::Element PantallaReportes::lineaError(const std::string& texto) const {
    ftxui::Element elemento = ftxui::text("      " + texto);
    if (const auto color = _tema.colorDe(Rol::Error)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

ftxui::Element PantallaReportes::lineaMensaje() const {
    const Mensaje& mensaje = mensajeActual();
    ftxui::Element elemento = ftxui::text("  " + mensaje.texto());

    Rol rol = Rol::Subtitulo;
    if (mensaje.esExito()) {
        rol = Rol::Exito;
    } else if (mensaje.esError()) {
        rol = Rol::Error;
    }
    if (const auto color = _tema.colorDe(rol)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

ftxui::Element PantallaReportes::lineaResultado() const {
    // EL ESTADO VACIO TIENE SALIDA
    //
    // Un mensaje que dice "no hay nada" y no dice como volver deja al
    // usuario atrapado: el mensaje dice las dos cosas.
    if (_filas.empty()) {
        return ftxui::text("  " + _mensajeVacio);
    }
    return tablaDe(_filas, _cabeceras);
}

ftxui::Element PantallaReportes::lineaOcupacion() const {
    // LA TABLA DE LA OCUPACION SIEMPRE TIENE FILAS
    //
    // Son los siete dias de la semana mas el total: no hay un estado
    // vacio que mostrar (una semana sin turnos es una semana con siete
    // dias en cero).
    return tablaDe(_filas, _cabeceras);
}

ftxui::Element PantallaReportes::tablaDe(const std::vector<std::vector<std::string>>& filas,
                                          const std::vector<std::string>& cabeceras) const {
    Tabla tabla(cabeceras);
    for (const std::vector<std::string>& fila : filas) {
        tabla.agregarFila(fila);
    }
    return tabla.renderizar();
}

ftxui::Element PantallaReportes::renderizar() {
    ftxui::Elements lineas;

    lineas.push_back(encabezado());
    lineas.push_back(ftxui::separator());

    switch (_vista) {
        case Vista::Menu:
            // La opcion `i` del menu queda en la fila `filaInicial + i`: el
            // hit-testing del raton lo necesita.
            _menu.definirFilaInicial(static_cast<int>(lineas.size()));
            for (int i = 0; i < _menu.cantidad(); ++i) {
                lineas.push_back(lineaOpcion(i));
            }
            lineas.push_back(ftxui::separator());
            break;
        case Vista::Campo:
            lineas.push_back(lineaCampo(*_campo));
            // EL ERROR VA DEBAJO DEL CAMPO
            //
            // El error describe el ultimo intento y el texto NO se borra: el
            // usuario tiene que corregir lo que escribio, no volver a
            // escribirlo.
            if (!_campo->error().empty()) {
                lineas.push_back(lineaError(_campo->error()));
            }
            break;
        case Vista::Selector: {
            if (!_selector) {
                break;
            }
            lineas.push_back(ftxui::text("  " + _selector->titulo()));
            lineas.push_back(lineaNumeroSelector());
            // La opcion `i` del selector queda en la fila `filaInicial + i`:
            // el hit-testing del raton lo necesita.
            _selector->definirFilaInicial(static_cast<int>(lineas.size()));
            for (int i = 0; i < _selector->cantidad(); ++i) {
                lineas.push_back(lineaOpcionSelector(i));
            }
            if (!_selector->error().empty()) {
                lineas.push_back(lineaError(_selector->error()));
            }
            break;
        }
        case Vista::Resultado:
            lineas.push_back(lineaResultado());
            break;
        case Vista::Ocupacion:
            lineas.push_back(lineaOcupacion());
            break;
    }

    if (!mensajeActual().esVacio()) {
        lineas.push_back(lineaMensaje());
    }
    lineas.push_back(ftxui::text(ayuda()));

    return ftxui::vbox(std::move(lineas));
}

std::string PantallaReportes::ayuda() const {
    // LA AYUDA DICE COMO VOLVER
    //
    // Un mensaje que no dice como se vuelve deja al usuario atrapado. En las
    // vistas con un campo de texto el '0' solo vuelve si el campo esta
    // vacio, asi que la ayuda lo dice: el '0' puede ser un dato.
    switch (_vista) {
        case Vista::Menu:
            return "Flechas: mover   Enter: elegir   0: volver   Esc: volver";
        case Vista::Campo:
            return "Enter: confirmar   0: volver (campo vacio)   Esc: volver";
        case Vista::Selector:
            return "Flechas: mover   Numero: elegir   Enter: confirmar   Esc: volver";
        case Vista::Resultado:
            return "0: volver   Esc: volver";
        case Vista::Ocupacion:
            return "0: volver   Esc: volver";
    }
    return "";
}

// ---------------------------------------------------------------------------
// El texto plano (lo que se ve sin terminal)
// ---------------------------------------------------------------------------

const std::string& PantallaReportes::nombreDeVista(Vista vista) {
    // Los nombres son ESTATICOS: son los mismos para todas las pantallas y
    // la referencia que se devuelve tiene que vivir mas que la llamada.
    static const std::string menu = "Menu";
    static const std::string campo = "Campo";
    static const std::string selector = "Selector";
    static const std::string resultado = "Resultado";
    static const std::string ocupacion = "Ocupacion";

    switch (vista) {
        case Vista::Menu:
            return menu;
        case Vista::Campo:
            return campo;
        case Vista::Selector:
            return selector;
        case Vista::Resultado:
            return resultado;
        case Vista::Ocupacion:
            return ocupacion;
    }
    return menu;
}

const std::string& PantallaReportes::vista() const {
    return nombreDeVista(_vista);
}

std::string PantallaReportes::textoDeLaVista() const {
    std::string salida;

    switch (_vista) {
        case Vista::Menu: {
            for (int i = 0; i < _menu.cantidad(); ++i) {
                salida += "  " + std::string(1, _menu.atajoDe(i)) + ". "
                    + _menu.etiquetas()[static_cast<std::size_t>(i)] + "\n";
            }
            return salida;
        }
        case Vista::Campo: {
            if (!_campo) {
                return salida;
            }
            salida += "  " + _campo->etiqueta() + ": " + _campo->textoEnPantalla() + "\n";
            if (!_campo->error().empty()) {
                salida += "      " + _campo->error() + "\n";
            }
            return salida;
        }
        case Vista::Selector: {
            if (_selector) {
                // La vista en texto plano tiene el MISMO orden que la
                // dibujada: el titulo, el numero que se esta escribiendo y
                // las opciones.
                salida += "  " + _selector->titulo() + "\n";
                salida += "  Numero: " + _selector->numeroDigitado() + "\n";
                for (int i = 0; i < _selector->cantidad(); ++i) {
                    salida += _selector->lineaDe(i, i == _selector->seleccion()) + "\n";
                }
                if (!_selector->error().empty()) {
                    salida += "      " + _selector->error() + "\n";
                }
            }
            return salida;
        }
        case Vista::Resultado: {
            for (const std::vector<std::string>& fila : _filas) {
                for (std::size_t i = 0; i < fila.size(); ++i) {
                    if (i > 0) {
                        salida += " | ";
                    }
                    salida += fila[i];
                }
                salida += "\n";
            }
            if (_filas.empty()) {
                salida += "  " + _mensajeVacio + "\n";
            }
            return salida;
        }
        case Vista::Ocupacion: {
            for (const std::vector<std::string>& fila : _filas) {
                for (std::size_t i = 0; i < fila.size(); ++i) {
                    if (i > 0) {
                        salida += " | ";
                    }
                    salida += fila[i];
                }
                salida += "\n";
            }
            return salida;
        }
    }
    return salida;
}

std::string PantallaReportes::vistaTextoPlano() const {
    // SOLO LA VISTA
    //
    // Es lo que se ve ahora, sin el encabezado ni la ayuda: es lo que
    // necesita el test para comprobar que la vista muestra lo que tiene que
    // mostrar.
    return textoDeLaVista();
}

std::string PantallaReportes::textoPlano() const {
    std::string salida = _aplicacion.titulo() + " - " + _titulo + "\n";
    salida += textoDeLaVista();
    if (!mensajeActual().esVacio()) {
        salida += mensajeActual().texto() + "\n";
    }
    salida += ayuda();
    return salida;
}

// ---------------------------------------------------------------------------
// La fabrica
// ---------------------------------------------------------------------------

std::unique_ptr<Pantalla> crearPantallaReportes(Aplicacion& aplicacion,
                                                 RepositorioPacientes& pacientes,
                                                 RepositorioMedicos& medicos,
                                                 ServicioTurnos& servicioTurnos) {
    return std::make_unique<PantallaReportes>(aplicacion, pacientes, medicos, servicioTurnos);
}

} // namespace Pantallas
} // namespace ui
