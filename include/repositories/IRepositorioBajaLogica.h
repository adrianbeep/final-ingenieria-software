#pragma once

// ---------------------------------------------------------------------------
// IRepositorioBajaLogica<T>: el CONTRATO de la baja logica.
//
// ES UNA INTERFAZ SEPARADA, Y NO UN METODO MAS DE `IRepositorio<T>`, POR UNA
// RAZON CONCRETA: no todas las entidades admiten la baja logica.
//
// `Turno` NO tiene el flag `activo`: su estado es
// `EstadoTurno { Activo, Cancelado, Reprogramado }`, y un turno no se "da de
// baja", se CANCELA. Esa operacion tiene reglas propias (un turno cancelado
// no se puede reprogramar) y por eso la maneja el servicio de turnos, no el
// repositorio.
//
// Si `bajaLogica` estuviera en la interfaz generica, el repositorio de
// turnos TENDRA que implementarla: o con una operacion sin sentido, o
// lanzando una excepcion en tiempo de ejecucion por un error que el
// COMPILADOR deberia impedir. Con dos interfaces, el compilador lo
// impide: el repositorio de turnos sencillamente no implementa esta.
//
// SOLO la implementan los repositorios de PACIENTE y MEDICO, que si tienen
// el flag `activo` (en `Persona`).
// ---------------------------------------------------------------------------
template <class T>
class IRepositorioBajaLogica {
public:
    virtual ~IRepositorioBajaLogica() = default;

    // Marca la entidad como inactiva. NO la borra: la baja es logica.
    // Lanza `EntidadNoExisteException` si no existe.
    virtual void bajaLogica(int id) = 0;

    // Vuelve a activarla. Lanza `EntidadNoExisteException` si no existe.
    virtual void reactivar(int id) = 0;
};
