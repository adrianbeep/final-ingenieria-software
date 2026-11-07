#pragma once

#include "entities/Medico.h"
#include "repositories/RepositorioMedicos.h"
#include "services/ServicioTurnos.h"
#include "ui/CrudPersonas.h"

#include <memory>

// ---------------------------------------------------------------------------
// La pantalla de MEDICOS.
//
// Es la HERMANA de la de pacientes: el motor (`ui::CrudPersonas`) es el mismo y
// lo unico que cambia son los datos (las columnas, los campos, el
// constructor y los criterios de busqueda).
//
// TODO lo que es ESPECIFICO de los medicos vive en
// `src/ui/Pantallas/Medicos.cpp`.
//
// POR QUE EL REPOSITORIO ES EL CONCRETO
//
// La pantalla necesita las REGLAS DE BUSQUEDA del repositorio: el nombre o
// apellido es una coincidencia PARCIAL e insensible a mayusculas, y el legajo
// es una coincidencia EXACTA (la misma con la que se verifica su unicidad).
// Si la pantalla recibiera el repositorio por su interfaz, no tendria esas
// reglas y tendria que reescribirlas: dos reglas que se pueden desincronizar.
// ---------------------------------------------------------------------------
namespace ui {

class Aplicacion;
class Pantalla;

namespace Pantallas {

// La configuracion del CRUD de medicos.
CrudPersonas<Medico>::Config configDeMedicos(RepositorioMedicos& repositorio,
                                                   ServicioTurnos& servicioTurnos);

// La pantalla de medicos, lista para apilarse en la aplicacion.
std::unique_ptr<Pantalla> crearPantallaMedicos(Aplicacion& aplicacion,
                                                    RepositorioMedicos& repositorio,
                                                    ServicioTurnos& servicioTurnos);

} // namespace Pantallas
} // namespace ui
