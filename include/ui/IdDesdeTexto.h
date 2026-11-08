#pragma once

#include "utils/Utils.h"

#include <string>

// ---------------------------------------------------------------------------
// La conversion de un TEXTO en un id.
//
// POR QUE VIVE ACA Y NO EN EL MOTOR CRUD
//
// La conversion la usan los CRITERIOS de busqueda de las personas (el
// criterio "id") y las pantallas que buscan un id (el criterio "id" de los
// turnos, el de los reportes). Si viviera en el motor CRUD, los criterios
// dependerian de el: y el motor es GENERICO, no tiene por que saber que
// existe una entidad que se llame "paciente".
//
// LA CONVERSION NO LANZA
//
// `std::stoi` lanza `std::out_of_range` con un numero enorme, y esa
// excepcion NO es del sistema: se escaparia sin archivo ni linea. Por eso
// el texto se valida ANTES de convertir, y la conversion va en un `try`.
// ---------------------------------------------------------------------------
namespace ui {

// Convierte un texto en un id. Devuelve -1 si el texto NO es un entero
// valido (o si esta fuera del rango de un id).
inline int idDesdeTexto(const std::string& texto) {
    if (!Utils::esEntero(texto)) {
        return -1;
    }
    try {
        const long long valor = std::stoll(texto);
        if (valor < 1 || valor > 1000000000LL) {
            return -1;
        }
        return static_cast<int>(valor);
    } catch (...) {
        return -1;
    }
}

} // namespace ui
