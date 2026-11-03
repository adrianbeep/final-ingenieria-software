#pragma once

#include "entities/Turno.h"
#include "repositories/RepositorioCRUD.h"

#include <string>

// ---------------------------------------------------------------------------
// RepositorioTurnos: el repositorio de TURNOS.
//
// Es `RepositorioCRUD<Turno>` y nada mas.
//
// POR QUE NO IMPLEMENTA `IRepositorioBajaLogica<Turno>`
//
// Un turno NO tiene el flag `activo`: su estado es
// `EstadoTurno { Activo, Cancelado, Reprogramado }` y un turno no se "da de
// baja", se CANCELA. La cancelacion tiene reglas propias (un turno
// cancelado no se puede reprogramar) y por eso la maneja el servicio de
// turnos, no el repositorio.
//
// Con dos interfaces separadas, el COMPILADOR impide que el repositorio de
// turnos se confunda: si alguien le agregara `bajaLogica`, el `static_assert`
// del test de la Fase 5 dejaria de compilar. Es el motivo de que la baja
// logica sea una interfaz aparte y no un metodo mas del CRUD generico.
// ---------------------------------------------------------------------------
class RepositorioTurnos : public RepositorioCRUD<Turno> {
public:
    explicit RepositorioTurnos(std::string ruta);
};
