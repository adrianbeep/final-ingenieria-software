#pragma once

#include "entities/Alumno.h"
#include "entities/Profesor.h"
#include "entities/Turno.h"
#include "repositories/IRepositorio.h"
#include "utils/Fecha.h"

#include <vector>

// ---------------------------------------------------------------------------
// ServicioTurnos: la CAPA DE REGLAS DE NEGOCIO de los turnos.
//
// QUE ES Y QUE NO ES
//
// Este servicio decide QUE SE PUEDE HACER: si un alumno inactivo puede
// tener un turno, si dos horarios se pisan, si un turno cancelado se puede
// reprogramar. No decide COMO se guarda nada: no sabe de archivos, ni de
// arboles, ni de registros. Por eso recibe repositorios por INTERFAZ y no
// implementaciones concretas.
//
// QUE HACE EL SERVICIO Y NO HACE EL REPOSITORIO
//
// El repositorio guarda y devuelve entidades. No sabe si dos turnos se
// solapan, no valida que un alumno inactivo pueda ser agendado y no decide
// si un turno cancelado se puede reprogramar. Eso es TODO de este servicio,
// y es la unica razon de que la capa exista.
//
// LA INYECCION POR CONSTRUCTOR (decision R3 del issue #14)
//
// El servicio recibe los TRES repositorios por el constructor y no los
// guarda como punteros: los guarda como REFERENCIAS a las interfaces. Por
// eso hay un unico punto de composicion (el `main`) y no un contenedor de
// inyeccion: con tres repositorios, un contenedor seria ruido.
//
// Y por eso los tests pueden inyectar un FAKE en memoria: el servicio
// escribe contra `IRepositorio<T>`, asi que un `std::vector<T>` que
// implemente la interfaz sirve igual que un archivo. Las reglas se prueban
// sin tocar el disco.
//
// LA SEPARACION CON EL DOMINIO
//
// El servicio NO hereda de la entidad ni la extiende: no sabe como se
// construye un turno, solo lo recibe ya construido y decide si ese turno
// tiene permiso de existir. La unica regla que el servicio reimplanta es
// la de los datos validos del turno (hora, duracion, aula), y lo hace
// porque el llamador puede haber construido el turno a mano: el servicio
// no puede confiar en que el otro hizo bien su trabajo.
//
// ---------------------------------------------------------------------------
// EL ID DE UN TURNO NUEVO
//
// `agendar` NO asigna el id: recibe el turno ya construido, con su id
// (decision R4 del issue #14: el id lo propone el repositorio). Quien
// agenda pide `proximoId()` al repositorio de turnos y construye el turno
// con ese id.
//
// En cambio `reprogramar` SI crea un turno y por eso SI pide el id: usa
// `proximoId()` del repositorio de turnos (decision #8 del proyecto: la
// reprogramacion crea un turno NUEVO y el viejo queda `Reprogramado`).
// ---------------------------------------------------------------------------
class ServicioTurnos {
public:
    // Recibe INTERFACES: los tests pueden inyectar fakes en memoria y
    // probar las reglas sin tocar el disco.
    explicit ServicioTurnos(IRepositorio<Alumno>& alumnos_,
                            IRepositorio<Profesor>& profesors_,
                            IRepositorio<Turno>& turnos_);

    // Agenda un turno nuevo.
    //
    // ORDEN DE LAS REGLAS (cada una lanza antes de pasar a la siguiente):
    //   1. el alumno existe y esta activo;
    //   2. el profesor existe y esta activo;
    //   3. la fecha no esta en el pasado;
    //   4. la hora, la duracion y el resto de los datos son validos;
    //   5. el turno no se pisa con ningun turno vigente, NI por el alumno
    //      NI por el profesor.
    //
    // El turno se guarda recien cuando TODAS las reglas pasaron: si una
    // falla, el repositorio queda como estaba.
    void agendar(const Turno& turno_);

    // Cancela un turno: le cambia el estado a `Cancelado`.
    //
    // NO lo borra: la baja es LOGICA (decision #10 del proyecto). Un turno
    // cancelado sigue en el repositorio, con su historial, y su horario
    // queda libre.
    void cancelar(int idTurno_);

    // Reprograma un turno: crea uno NUEVO con la fecha y la hora pedidas y
    // deja al viejo como `Reprogramado`, apuntando al nuevo.
    //
    // El turno viejo tiene que estar `Activo`: un turno cancelado no se
    // reprograma (cancelado y reprogramado son estados finales).
    //
    // Las reglas de `agendar` se aplican TAMBIEN al turno nuevo: no se
    // puede esquivar el control de solapamiento reprogramando. Si alguna
    // falla, NINGUNO de los dos turnos se modifica.
    void reprogramar(int idTurno_, const Fecha& nuevaFecha_, const Hora& nuevaHora_);

    // Los turnos VIGENTES (`Activo`) de un alumno.
    //
    // Toda pantalla filtra por aca: un turno cancelado o reprogramado no se
    // muestra como pendiente, aunque siga en el repositorio.
    std::vector<Turno> turnosVigentesDe(int idAlumno_) const;

private:
    // Aplica TODAS las reglas de `agendar` a un turno candidato. Es el
    // unico lugar donde estan: `agendar` y `reprogramar` las comparten, asi
    // que reprogramar no puede esquivar ninguna.
    void validarAgendamiento(const Turno& turno_);

    // Revisa los datos PROPIOS del turno: id, hora, duracion y aula.
    //
    // POR QUE SE REVISA LO QUE LA ENTIDAD YA VALIDO
    //
    // El constructor de `Turno` valida todo, asi que un turno bien
    // construido ya es valido. Pero el servicio no puede dar por hecho que
    // el llamador lo construyo bien: un turno armado a mano (o el que deja
    // el constructor por defecto) puede tener la hora fuera de horario o
    // la duracion en cero. Ahi el servicio tiene que decir QUE campo esta
    // mal, y para eso necesita los datos por separado.
    void validarDatosDelTurno(const Turno& turno_);

    // Lanza `TurnoOcupadoException` si el turno se pisa con algun turno
    // vigente, sea por el alumno o por el profesor.
    void verificarSolapamiento(const Turno& turno_);

    // Busca un turno por id. Lanza `EntidadNoExisteException` si no esta.
    Turno buscarTurno(int idTurno_) const;

    // La fecha de HOY, segun el reloj del sistema.
    //
    // Es el UNICO lugar del sistema donde se consulta el reloj: el resto de
    // la aplicacion es determinista, y esta regla es la unica que depende
    // del "ahora".
    static Fecha fechaDeHoy();

    IRepositorio<Alumno>& _alumnos;
    IRepositorio<Profesor>& _profesores;
    IRepositorio<Turno>& _turnos;
};
