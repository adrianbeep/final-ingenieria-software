#pragma once

#include "ui/Buscador.h"
#include "ui/Selector.h"

#include <ftxui.hpp>

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// El dibujo de una lista de la que se elige UNA.
//
// POR QUE VIVE ACA Y NO EN CADA PANTALLA
//
// La lista de la especialidad (un `ui::Selector`) y la lista de las
// coincidencias del buscador (un `ui::Buscador`) se dibujan IGUAL: una
// opcion por linea, con el numero posicional adelante y la ELEGIDA
// invertida. Las dos formas las usan tres pantallas (turnos, y el motor CRUD
// en sus dos listas). Si cada una escribiera su propio bucle, el resaltado
// de una podria quedar distinto del de la otra, y con `NO_COLOR` la
// diferencia se veria.
//
// EL NUMERO LO PONE EL SELECTOR
//
// El `Selector` ya numera sus lineas (`Selector::lineaDe`): su linea sale
// tal cual. El `Buscador` NO numera las suyas (su linea es el dato de la
// entidad, y se usa tambien en el texto plano), asi que el numero lo
// agrega el que dibuja.
//
// POR QUE SON `inline`
//
// Son funciones chiquitas que dependen de clases concretas (las del `ui`):
// si vivieran en un .cpp, el header tendria que declarar esas clases
// igual, asi que no se gana nada. Es el mismo criterio que usan los otros
// helpers de la capa `ui` (`ui::NombresDePersonas.h`).
// ---------------------------------------------------------------------------
namespace ui {
namespace detalle {

// La linea de una opcion: su texto y el resaltado.
//
// EL RESALTADO ES `inverted`
//
// `inverted` funciona SIN color, asi que la lista se ve igual con `NO_COLOR`
// definido.
inline ftxui::Element lineaDeOpcion(const std::string& texto, bool elegida) {
    ftxui::Element elemento = ftxui::text(texto);
    if (elegida) {
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

} // namespace detalle

// Las lineas de un SELECTOR: una por opcion, con la elegida invertida.
inline std::vector<ftxui::Element> lineasDeSelector(const Selector& selector) {
    std::vector<ftxui::Element> lineas;
    lineas.reserve(static_cast<std::size_t>(selector.cantidad()));
    for (int i = 0; i < selector.cantidad(); ++i) {
        lineas.push_back(detalle::lineaDeOpcion(selector.lineaDe(i), i == selector.seleccion()));
    }
    return lineas;
}

// Las lineas de un BUSCADOR: una por coincidencia, con la elegida invertida.
inline std::vector<ftxui::Element> lineasDeBuscador(const Buscador& buscador) {
    std::vector<ftxui::Element> lineas;
    lineas.reserve(static_cast<std::size_t>(buscador.cantidadDeLineas()));
    for (int i = 0; i < buscador.cantidadDeLineas(); ++i) {
        const std::string linea = "  " + std::to_string(i + 1) + ". " + buscador.lineaDe(i);
        lineas.push_back(detalle::lineaDeOpcion(linea, i == buscador.seleccion()));
    }
    return lineas;
}

} // namespace ui
