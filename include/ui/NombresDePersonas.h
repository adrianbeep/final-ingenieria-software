#pragma once

#include "entities/Medico.h"
#include "entities/Paciente.h"
#include "repositories/RepositorioMedicos.h"
#include "repositories/RepositorioPacientes.h"

#include <string>

// ---------------------------------------------------------------------------
// Los NOMBRES de las personas, resueltos por su id.
//
// POR QUE VIVEN ACA Y NO DENTRO DE UNA PANTALLA
//
// Un turno guarda los IDS del paciente y del medico, no sus nombres: la fila
// los resuelve con el repositorio. La REGLA de esa resolucion la usan mas de
// una pantalla (la de turnos y la de reportes), asi que vive en UN solo
// lugar: si cada pantalla la escribiera de nuevo, habria dos reglas que se
// pueden desincronizar (por ejemplo, una que pusiera "#3" y otra que
// dejara la celda vacia).
//
// QUE SE VE CUANDO EL DATO NO EXISTE
//
// Se muestra el id con un "#" adelante: un dato que falta tiene que verse,
// no desaparecer.
//
// POR QUE SON `inline` Y NO UN .CPP
//
// Son funciones chiquitas que dependen de la clase concreta del
// repositorio (que es la que sabe buscar por id). Si vivieran en un .cpp,
// el header tendria que declarar la clase del repositorio igual, asi que
// no se gana nada: se dejan aca, como los otros helpers de la capa `ui`
// (`ui::idDesdeTexto`, `ui::eventoEnter`).
// ---------------------------------------------------------------------------
namespace ui {

inline std::string nombreDelPaciente(int id, RepositorioPacientes& pacientes) {
    Paciente paciente;
    if (!pacientes.buscarPorId(id, paciente)) {
        return "#" + std::to_string(id);
    }
    return paciente.getNombre() + " " + paciente.getApellido();
}

inline std::string nombreDelMedico(int id, RepositorioMedicos& medicos) {
    Medico medico;
    if (!medicos.buscarPorId(id, medico)) {
        return "#" + std::to_string(id);
    }
    return medico.getNombre() + " " + medico.getApellido();
}

} // namespace ui
