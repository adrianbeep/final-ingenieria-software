#pragma once

#include "entities/Paciente.h"
#include "repositories/RepositorioPacientes.h"
#include "services/ServicioTurnos.h"
#include "ui/CrudPersonas.h"

#include <memory>

// ---------------------------------------------------------------------------
// La pantalla de PACIENTES.
//
// TODO lo que es ESPECIFICO de los pacientes vive en `src/ui/Pantallas/Pacientes.cpp`:
// las columnas, el formulario, los validadores, el constructor de la entidad
// y los criterios de busqueda. El motor (`ui::CrudPersonas`) es generico y no
// sabe que existe una entidad que se llame "paciente".
//
// POR QUE LA CONFIGURACION ES UNA FUNCION Y NO UN CONSTRUCTOR
//
// La configuracion necesita el repositorio (para las busquedas) y el
// servicio (para los turnos de la ficha): son datos que la pantalla no
// puede inventar. Por eso la arma una funcion que los recibe.
//
// POR QUE EL REPOSITORIO ES EL CONCRETO
//
// La pantalla necesita las REGLAS DE BUSQUEDA del repositorio: el nombre o
// apellido es una coincidencia PARCIAL e insensible a mayusculas, y la
// dni es una coincidencia EXACTA (la misma con la que se verifica su
// unicidad). Si la pantalla recibiera el repositorio por su interfaz, no
// tendria esas reglas y tendria que reescribirlas: dos reglas que se
// pueden desincronizar.
//
// El motor CRUD, en cambio, sigue trabajando contra las interfaces: eso es
// lo que permite que sus tests injecten fakes.
// ---------------------------------------------------------------------------
namespace ui {

class Aplicacion;
class Pantalla;

namespace Pantallas {

// La configuracion del CRUD de pacientes.
CrudPersonas<Paciente>::Config configDePacientes(RepositorioPacientes& repositorio,
                                              ServicioTurnos& servicioTurnos);

// La pantalla de pacientes, lista para apilarse en la aplicacion.
std::unique_ptr<Pantalla> crearPantallaPacientes(Aplicacion& aplicacion,
                                                RepositorioPacientes& repositorio,
                                                ServicioTurnos& servicioTurnos);

} // namespace Pantallas
} // namespace ui
