#include "services/ServicioTurnos.h"

#include "domain/Especialidad.h"
#include "exceptions/ExcepcionesSistema.h"

#include <ctime>
#include <string>

namespace {

// ---------------------------------------------------------------------------
// Constantes del calculo de instantes
// ---------------------------------------------------------------------------

// Horas que tiene un dia. Se usa para pasar de un numero de dia a minutos.
constexpr int HORAS_POR_DIA = 24;

// Minutos que tiene un dia: es el factor para pasar de `numeroDeDia()` a
// minutos absolutos.
constexpr int MINUTOS_POR_DIA = HORAS_POR_DIA * Hora::MINUTOS_POR_HORA;

// El `struct tm` cuenta los anos desde 1900 y los meses desde 0.
constexpr int ANIO_BASE_TM = 1900;
constexpr int MES_BASE_TM = 1;

// ---------------------------------------------------------------------------
// La VENTANA de un turno: el intervalo de tiempo que ocupa.
//
// QUE ES UN INSTANTE ABSOLUTO
//
// Cada ventana se mide en MINUTOS desde la primera fecha valida del dominio
// (2000-01-01 00:00). El numero de dia viene de `Fecha::numeroDeDia()` y la
// hora del dia, de `Hora::minutosDesdeMedianoche()`.
//
// POR QUE UN SOLO NUMERO Y NO DOS (DIA Y HORA)
//
// Con dos numeros habria que comparar el dia, despues la hora, y acordarse
// del caso en que un turno cruza la medianoche: el 23:30 del dia 3 y las
// 00:15 del dia 4 son instantes CONSECUTIVOS, y con dos numeros
// comparados por separado el segundo pareceria anterior al primero. Con un
// unico numero absoluto ese caso no existe: la medianoche es simplemente el
// minuto en que el numero de dia avanza.
//
// Un turno que termina despues de la medianoche (empieza a las 22:00 y
// dura 240 minutos) tiene su `fin` en el dia siguiente, y asi queda
// escrito: `fin` puede ser mayor que el numero de dia multiplicado por los
// minutos del dia, y por eso el comparaison de ventanas es en minutos y
// no en horas.
// ---------------------------------------------------------------------------
struct Ventana {
    int inicio = 0;
    int fin = 0;
};

// La ventana que ocupa un turno.
Ventana ventanaDe(const Turno& turno) {
    Ventana ventana;
    ventana.inicio = turno.getFecha().numeroDeDia() * MINUTOS_POR_DIA
        + turno.getHora().minutosDesdeMedianoche();
    // El fin es el inicio mas la duracion: puede pasarse de la medianoche,
    // y eso ya esta resuelto porque los minutos son absolutos.
    ventana.fin = ventana.inicio + turno.getDuracionMin();
    return ventana;
}

// ---------------------------------------------------------------------------
// El test de SOLAPAMIENTO (decision R7 del issue #14).
//
// DOS INTERVALOS SE SOLAPAN SI CADA UNO EMPIEZA ANTES DE QUE TERMINE EL
// OTRO: `(inicioA < finB) && (inicioB < finA)`.
//
// POR QUE NO ALCANZA CON COMPARAR LOS INICIOS
//
// Un turno de 10:00 a 10:45 y otro de 10:30 a 11:15 SE PISAN, aunque sus
// horas de inicio sean distintas. Comparando solo los inicios con el
// `operator<` de la `Hora`, el primero es menor que el segundo (10:00 <
// 10:30) y el segundo NO es menor que el primero (10:30 < 10:00 es falso):
// la comparacion daria "no se pisan" y dejaria pasar los dos turnos.
//
// Con el criterio de intervalos, en cambio: 10:00 < 11:15 y 10:30 < 10:45,
// que son las DOS condiciones, asi que se detecta el choque.
//
// Y el criterio de intervalos tambien distingue el caso limite: dos turnos
// que se tocan JUSTO (10:00-10:45 y 10:45-11:30) NO se pisan, porque el
// segundo empieza exactamente cuando termina el primero.
// ---------------------------------------------------------------------------
bool seSolapan(const Ventana& a, const Ventana& b) {
    return a.inicio < b.fin && b.inicio < a.fin;
}

} // namespace

// ---------------------------------------------------------------------------
// Construccion
// ---------------------------------------------------------------------------

ServicioTurnos::ServicioTurnos(IRepositorio<Paciente>& pacientes_,
                               IRepositorio<Medico>& medicos_,
                               IRepositorio<Turno>& turnos_)
    : _pacientes(pacientes_),
      _medicos(medicos_),
      _turnos(turnos_) {
}

// ---------------------------------------------------------------------------
// AGENDAR
// ---------------------------------------------------------------------------

void ServicioTurnos::agendar(const Turno& turno_) {
    // Primero todas las reglas, despues el guardado. Si una regla falla,
    // el repositorio queda como estaba: no queda un turno a medio agendar.
    validarAgendamiento(turno_);

    _turnos.alta(turno_);
}

// ---------------------------------------------------------------------------
// CANCELAR
// ---------------------------------------------------------------------------

void ServicioTurnos::cancelar(int idTurno_) {
    Turno turno = buscarTurno(idTurno_);

    // Un turno que NO esta activo no se puede cancelar.
    //
    // POR QUE ES UN ERROR Y NO UNA OPERACION IDEMPOTENTE
    //
    // Cancelar dos veces el mismo turno casi siempre es una confusion del
    // que lo pide (un doble clic, una pantalla que se recargo), y ocultarla
    // con un "ok" silencioso esconde el problema. Ademas, `Reprogramado` es
    // un estado DISTINTO de `Cancelado`: un turno reprogramado ya fue
    // movido, y volverlo a cancelar seria mentir sobre su historial.
    if (turno.getEstado() != EstadoTurno::Activo) {
        throw DatoInvalidoException("estado", estadoATexto(turno.getEstado()),
                                    "el turno no esta activo: no se puede cancelar");
    }

    turno.setEstado(EstadoTurno::Cancelado);

    // El turno NO se borra: la baja es LOGICA (decision #10 del proyecto).
    // `modificar` reemplaza el turno con el mismo id y lo persiste.
    _turnos.modificar(turno);
}

// ---------------------------------------------------------------------------
// REPROGRAMAR
// ---------------------------------------------------------------------------

int ServicioTurnos::reprogramar(int idTurno_,
                                 const Fecha& nuevaFecha_,
                                 const Hora& nuevaHora_) {
    Turno original = buscarTurno(idTurno_);

    // Un turno cancelado NO se reprograma: `Cancelado` y `Reprogramado` son
    // estados finales. Un turno ya reprogramado tampoco: su reemplazo ya
    // existe y volver a reprogramarlo perderia el historial.
    if (original.getEstado() != EstadoTurno::Activo) {
        throw DatoInvalidoException("estado", estadoATexto(original.getEstado()),
                                    "solo un turno activo se puede reprogramar");
    }

    // La reprogramacion crea un turno NUEVO (decision #8 del proyecto): el
    // viejo queda `Reprogramado` y apunta al nuevo. El turno nuevo hereda
    // paciente, medico, especialidad, duracion y consultorio, y toma la fecha y la
    // hora nuevas.
    const int idNuevo = _turnos.proximoId();
    Turno nuevo(idNuevo,
                 original.getIdMedico(),
                 original.getIdPaciente(),
                 original.getEspecialidad(),
                 nuevaFecha_,
                 nuevaHora_,
                 original.getDuracionMin(),
                 original.getConsultorio());

    // Las reglas de `agendar` valen TAMBIEN para el turno nuevo: no se
    // puede esquivar el control de solapamiento reprogramando. Se aplican
    // ANTES de tocar el repositorio, asi que si una falla NINGUN turno
    // queda modificado (el viejo sigue `Activo`).
    //
    // OJO: el turno viejo todavia esta `Activo` cuando se hace esta
    // validacion, asi que el control de solapamiento lo ve. Es lo
    // correcto: reprogramar a un horario que el propio turno viejo ocupa
    // es un error (seria mover un turno a un lugar donde ya esta).
    validarAgendamiento(nuevo);

    // Se guarda el turno NUEVO primero. Si el alta fallara (por ejemplo,
    // un id duplicado), el viejo queda intacto.
    _turnos.alta(nuevo);

    // Y despues se marca el viejo, que ahora apunta al reemplazo.
    original.setEstado(EstadoTurno::Reprogramado);
    original.setIdTurnoReemplazante(idNuevo);
    _turnos.modificar(original);

    // EL ID DEL TURNO NUEVO
    //
    // Lo devuelve el servicio porque es el que pide el id al repositorio:
    // la pantalla que reprograma lo usa para avisar "Turno #31
    // reprogramado. Nuevo turno #32." y no puede sacarlo de otro lado.
    return idNuevo;
}

// ---------------------------------------------------------------------------
// TURNOS VIGENTES DE UN PACIENTE
// ---------------------------------------------------------------------------

std::vector<Turno> ServicioTurnos::turnosVigentesDe(int idPaciente_) const {
    std::vector<Turno> vigentes;

    // Solo los turnos `Activo` cuentan: un turno cancelado o reprogramado
    // sigue en el repositorio (su baja es logica) pero NO ocupa un horario
    // ni se muestra como pendiente.
    for (const Turno& turno : _turnos.listar()) {
        if (turno.getIdPaciente() == idPaciente_ && turno.getEstado() == EstadoTurno::Activo) {
            vigentes.push_back(turno);
        }
    }

    return vigentes;
}

// ---------------------------------------------------------------------------
// LAS CONSULTAS DE LA INTERFAZ
//
// Las cinco son consultas y las cinco se comportan igual:
//
//   - NO lanzan y NO validan los ids: un id que no existe devuelve un
//     vector VACIO. La ausencia es un dato de la consulta, no un error.
//
//   - Devuelven los turnos de CUALQUIER estado: un reporte tiene que poder
//     mostrar tambien los cancelados y los reprogramados. Por eso NO filtran
//     por estado (a diferencia de `turnosVigentesDe`).
//
//   - Devuelven los resultados ORDENADOS por id: `listar()` ya entrega los
//     turnos ordenados, asi que el filtrado conserva el orden y no hace
//     falta reordenar despues.
//
// QUE HAY DE UNO Y OTRO
//
// La diferencia entre las cinco consultas es SOLO el filtro: por paciente, por
// medico, por especialidad, por fecha o por rango de fechas. El resto del
// comportamiento es identico, asi que todas comparten la misma forma.
// ---------------------------------------------------------------------------

std::vector<Turno> ServicioTurnos::turnosDePaciente(int idPaciente_) const {
    std::vector<Turno> turnos;

    for (const Turno& turno : _turnos.listar()) {
        if (turno.getIdPaciente() == idPaciente_) {
            turnos.push_back(turno);
        }
    }

    return turnos;
}

std::vector<Turno> ServicioTurnos::turnosDeMedico(int idMedico_) const {
    std::vector<Turno> turnos;

    for (const Turno& turno : _turnos.listar()) {
        if (turno.getIdMedico() == idMedico_) {
            turnos.push_back(turno);
        }
    }

    return turnos;
}

std::vector<Turno> ServicioTurnos::turnosDeEspecialidad(Especialidad especialidad_) const {
    std::vector<Turno> turnos;

    for (const Turno& turno : _turnos.listar()) {
        if (turno.getEspecialidad() == especialidad_) {
            turnos.push_back(turno);
        }
    }

    return turnos;
}

std::vector<Turno> ServicioTurnos::turnosDeFecha(const Fecha& fecha_) const {
    std::vector<Turno> turnos;

    for (const Turno& turno : _turnos.listar()) {
        if (turno.getFecha() == fecha_) {
            turnos.push_back(turno);
        }
    }

    return turnos;
}

std::vector<Turno> ServicioTurnos::turnosDeRango(const Fecha& desde_, const Fecha& hasta_) const {
    // UN RANGO AL REVES NO ES UN RANGO VACIO: ES UN ERROR
    //
    // Si `desde` es posterior a `hasta`, el rango no esta "vacio": esta mal
    // pedido. Devolver un vector vacio en ese caso taparia el error de quien
    // pidio el rango (un formulario con las fechas cruzadas), asi que se dice
    // con un `DatoInvalidoException`, que es el formato de error de dato
    // invalido de toda la aplicacion.
    //
    // `Fecha` no tiene `operator>`: se usa la negacion de `operator<`, que es
    // la unica comparacion que el valor de calendario ofrece.
    if (hasta_ < desde_) {
        throw DatoInvalidoException("rango", desde_.aTexto() + " - " + hasta_.aTexto(),
                                    "la fecha inicial no puede ser posterior a la final");
    }

    std::vector<Turno> turnos;

    for (const Turno& turno : _turnos.listar()) {
        // INCLUSIVO EN LOS DOS EXTREMOS: un turno del dia `desde` y un turno
        // del dia `hasta` entran. Por eso la comparacion es la NEGACION de
        // `<` (que es `<=`) y no un `<` pelado.
        if (!(turno.getFecha() < desde_) && !(hasta_ < turno.getFecha())) {
            turnos.push_back(turno);
        }
    }

    return turnos;
}

// ---------------------------------------------------------------------------
// MODIFICAR UN TURNO
// ---------------------------------------------------------------------------

void ServicioTurnos::modificarTurno(int idTurno_,
                                    const std::string& nuevoConsultorio_,
                                    int nuevaDuracionMin_,
                                    Especialidad nuevaEspecialidad_) {
    Turno turno = buscarTurno(idTurno_);

    // Un turno que NO esta activo no se modifica: `Cancelado` y
    // `Reprogramado` son estados finales. Es el mismo criterio (y el mismo
    // texto) que usa `reprogramar`.
    if (turno.getEstado() != EstadoTurno::Activo) {
        throw DatoInvalidoException("estado", estadoATexto(turno.getEstado()),
                                    "solo un turno activo se puede modificar");
    }

    // Se modifican SOLO los tres datos que se pueden cambiar. La fecha y la
    // hora NO se tocan: moverlas es reprogramar, y reprogramar crea un turno
    // nuevo con su propio historial. El paciente, el medico y el estado
    // tampoco cambian.
    //
    // Los setters validan: la duracion tiene que ser un multiplo de 15 (y
    // de 240 minutos como maximo) y el consultorio tiene que tener entre 1 y 20
    // caracteres. Si alguno esta mal, la excepcion sale de aca.
    turno.setConsultorio(nuevoConsultorio_);
    turno.setDuracionMin(nuevaDuracionMin_);
    turno.setEspecialidad(nuevaEspecialidad_);

    // Se revalidan los DATOS del turno con los valores nuevos: en
    // particular, que con la duracion nueva el turno siga TERMINANDO dentro
    // del horario de atencion (una duracion mas larga puede empujar el fin
    // del turno mas alla de las 22:00).
    //
    // Se usa el MISMO helper que usa `agendar`: la regla del fin del
    // horario vive ahi y no se reimplementa. Lo que NO se usa es
    // `validarAgendamiento`: ese helper tambien exige que el paciente y el
    // medico existan y esten activos y que la fecha no haya pasado, y
    // esas comprobaciones no aplican a una modificacion (el turno ya existe,
    // y puede estar en el pasado).
    validarDatosDelTurno(turno);

    // Y el solapamiento se recalcula con la duracion nueva, EXCLUYENDO al
    // propio turno: el turno se pisa consigo mismo (su ventana nueva se
    // superpone con su ventana vieja) y ese choque no es un error.
    verificarSolapamiento(turno, idTurno_);

    // Solo se persiste si TODAS las reglas pasaron: si alguna falla, el
    // repositorio queda como estaba.
    _turnos.modificar(turno);
}

// ---------------------------------------------------------------------------
// Las reglas
// ---------------------------------------------------------------------------

void ServicioTurnos::validarAgendamiento(const Turno& turno_) {
    // --- REGLA 1: el paciente existe y esta activo ---
    Paciente paciente;
    if (!_pacientes.buscarPorId(turno_.getIdPaciente(), paciente)) {
        throw EntidadNoExisteException(turno_.getIdPaciente(), "Paciente");
    }
    if (!paciente.esActivo()) {
        throw DatoInvalidoException("paciente", std::to_string(paciente.getId()),
                                    "el paciente esta dado de baja y no puede tener turnos");
    }

    // --- REGLA 2: el medico existe y esta activo ---
    Medico medico;
    if (!_medicos.buscarPorId(turno_.getIdMedico(), medico)) {
        throw EntidadNoExisteException(turno_.getIdMedico(), "Medico");
    }
    if (!medico.esActivo()) {
        throw DatoInvalidoException("medico", std::to_string(medico.getId()),
                                    "el medico esta dado de baja y no puede tener turnos");
    }

    // --- REGLA 2b: el turno es de la especialidad del medico ---
    //
    // Un medico atiende UNA especialidad: no puede atender un turno de otra.
    // El valor de la excepcion es el CODIGO de la especialidad del turno
    // ("DERM"), no la descripcion: el codigo es el dato que viaja en el
    // archivo.
    if (medico.getEspecialidad() != turno_.getEspecialidad()) {
        throw DatoInvalidoException("especialidad", codigoDe(turno_.getEspecialidad()),
                                    "el medico no atiende esa especialidad");
    }

    // --- REGLA 3: la fecha existe y no esta en el pasado ---
    const Fecha& fecha = turno_.getFecha();
    if (!Fecha::esValida(fecha.anio, fecha.mes, fecha.dia)) {
        throw DatoInvalidoException("fecha", fecha.aTexto(), "fecha inexistente");
    }
    if (fecha < fechaDeHoy()) {
        throw DatoInvalidoException("fecha", fecha.aTexto(), "la fecha del turno ya paso");
    }

    // --- REGLA 4: la hora, la duracion y el resto de los datos son validos ---
    validarDatosDelTurno(turno_);

    // --- REGLA 5: el turno no se pisa con ningun turno vigente ---
    verificarSolapamiento(turno_);
}

void ServicioTurnos::validarDatosDelTurno(const Turno& turno_) {
    // El id lo propone el repositorio (decision R4 del issue #14), asi que
    // un turno sin id no se puede guardar.
    if (turno_.getId() <= 0) {
        throw DatoInvalidoException("id", std::to_string(turno_.getId()),
                                    "se esperaba un id de turno valido");
    }

    const Hora& hora = turno_.getHora();
    if (!Hora::esValida(hora.hora, hora.minuto)) {
        throw DatoInvalidoException("hora", hora.aTexto(),
                                    "fuera del horario de atencion o con minutos que no son "
                                    "multiplos de 30");
    }

    const int duracion = turno_.getDuracionMin();
    if (duracion <= 0 || duracion % Turno::DURACION_MULTIPLO_MINUTOS != 0
        || duracion > Turno::DURACION_MAXIMA_MINUTOS) {
        throw DatoInvalidoException("duracionMin", std::to_string(duracion),
                                    "se esperaba una duracion positiva, multiplo de 15 y de "
                                    "240 minutos como maximo");
    }

    // EL TURNO TIENE QUE TERMINAR DENTRO DEL HORARIO DE ATENCION
    //
    // El tope de las 22:00 marca el fin del horario en que la institucion
    // atiende. Un turno que EMPIEZA a las 21:30 y dura 90 minutos termina
    // a las 23:00: la consulta se esta dictando cuando ya no hay nadie que
    // atienda. Por eso se exige que el turno TERMINE antes del tope, y no
    // solo que empiece dentro del horario.
    //
    // La cuenta es en MINUTOS: el tope es un instante (1320 minutos), no una
    // hora, asi que se compara con `minutosDesdeMedianoche()`.
    const int fin = hora.minutosDesdeMedianoche() + duracion;
    const int finDelHorario = Hora::HORA_FIN * Hora::MINUTOS_POR_HORA;
    if (fin > finDelHorario) {
        throw DatoInvalidoException("hora", hora.aTexto(),
                                    "el turno termina despues de las "
                                        + Hora(Hora::HORA_FIN, 0).aTexto()
                                        + ", que es el fin del horario de atencion");
    }

    // Lo que SOBRA de las reglas anteriores (fecha, hora y duracion ya
    // quedaron chequeadas) es el consultorio: por eso el mensaje la nombra.
    if (!turno_.esValido()) {
        throw DatoInvalidoException("consultorio", turno_.getConsultorio(),
                                    "se esperaba un consultorio de 1 a 20 caracteres, con letras, "
                                    "digitos, espacios, '-' y '.'");
    }
}

void ServicioTurnos::verificarSolapamiento(const Turno& turno_, int idTurnoExcluido_) {
    const Ventana nueva = ventanaDe(turno_);

    for (const Turno& existente : _turnos.listar()) {
        // EL TURNO QUE SE ESTA MODIFICANDO NO SE MIRA CONTRA SI MISMO
        //
        // `modificarTurno` alarga (o acorta) un turno que YA esta guardado:
        // la ventana nueva se superpone con la ventana vieja del MISMO turno,
        // y ese choque no es un error: es el turno cambiando de largo. La
        // exclusion es por id, que es la clave con la que el repositorio
        // indexa los turnos.
        if (existente.getId() == idTurnoExcluido_) {
            continue;
        }

        // SOLO los turnos VIGENTES ocupan el horario: un turno cancelado o
        // reprogramado libero su horario y ese horario se puede volver a
        // usar.
        if (existente.getEstado() != EstadoTurno::Activo) {
            continue;
        }

        // EL CHOQUE SE BUSCA EN LOS DOS EJES (decision R8 del issue #14)
        //
        // No puede ser que el paciente Y el medico sean la misma persona,
        // asi que basta con que coincida UNO de los dos: si coincide el
        // paciente, el paciente esta ocupado; si coincide el medico, el
        // medico esta ocupado.
        if (existente.getIdPaciente() != turno_.getIdPaciente()
            && existente.getIdMedico() != turno_.getIdMedico()) {
            continue;
        }

        if (!seSolapan(nueva, ventanaDe(existente))) {
            continue;
        }

        // El detalle lleva los ids del turno que YA ESTABA: son los que
        // estaban ocupados. Es el mismo formato que usa el mensaje de la
        // excepcion de la Fase 1 ("idMedico=N idPaciente=M").
        throw TurnoOcupadoException(
            existente.getId(),
            "idMedico=" + std::to_string(existente.getIdMedico())
                + " idPaciente=" + std::to_string(existente.getIdPaciente()));
    }
}

// ---------------------------------------------------------------------------
// Utilidades internas
// ---------------------------------------------------------------------------

Turno ServicioTurnos::buscarTurno(int idTurno_) const {
    Turno turno;
    if (!_turnos.buscarPorId(idTurno_, turno)) {
        throw EntidadNoExisteException(idTurno_, "Turno");
    }
    return turno;
}

Fecha ServicioTurnos::fechaDeHoy() {
    const std::time_t ahora = std::time(nullptr);

    // `std::localtime` devuelve un puntero a un objeto estatico: es la forma
    // portable de obtener la fecha LOCAL (la del calendario de la
    // institucion), que es la que cuenta para el horario de atencion.
    const std::tm* local = std::localtime(&ahora);
    if (local == nullptr) {
        // No deberia pasar (solo si el reloj del sistema no se puede
        // convertir), pero si pasa NO se puede decidir la regla de la fecha
        // en el pasado, asi que se dice en vez de dejar pasar un turno
        // invalido.
        throw ExcepcionSistema("No se pudo obtener la fecha actual del sistema");
    }

    return Fecha(local->tm_year + ANIO_BASE_TM, local->tm_mon + MES_BASE_TM, local->tm_mday);
}
