#pragma once

#include "ui/Buscador.h"
#include "ui/ColorFtxui.h"
#include "ui/Selector.h"
#include "ui/Theme.h"

#include <ftxui.hpp>

#include <cstddef>
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
// invertida. Las dos formas las usan tres pantallas (turnos, reportes y el
// motor CRUD en sus dos listas). Si cada una escribiera su propio bucle, el
// resaltado de una podria quedar distinto del de la otra, y con el tema
// apagado la diferencia se veria.
//
// EL NUMERO VA CON EL COLOR DE REALCE
//
// El numero es la POSICION de la opcion, y es lo que el usuario tiene que
// escribir para elegirla: tiene que saltar a la vista. Por eso el numero va
// con el color de realce y el resto de la linea con el color por defecto.
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

// La linea de una opcion: su numero, su texto y el resaltado.
//
// EL RESALTADO ES `inverted`
//
// `inverted` funciona SIN color, asi que la lista se ve igual con el tema
// apagado. Y el invertido no borra el color: el numero sigue siendo el
// numero que hay que escribir.
inline ftxui::Element lineaDeOpcion(const Theme& tema, const std::string& numero,
                                     const std::string& texto, bool elegida) {
    ftxui::Elements partes;

    partes.push_back(ftxui::text("  "));

    ftxui::Element digito = ftxui::text(numero);
    if (const auto color = tema.colorDe(Rol::Realce)) {
        digito = digito | ftxui::color(aFtxui(*color));
    }
    partes.push_back(digito);

    partes.push_back(ftxui::text(". " + texto));

    ftxui::Element elemento = ftxui::hbox(std::move(partes));
    if (elegida) {
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

} // namespace detalle

// Las lineas de un SELECTOR: una por opcion, con la elegida invertida.
inline std::vector<ftxui::Element> lineasDeSelector(const Theme& tema,
                                                      const Selector& selector) {
    std::vector<ftxui::Element> lineas;
    lineas.reserve(static_cast<std::size_t>(selector.cantidad()));
    for (int i = 0; i < selector.cantidad(); ++i) {
        const std::size_t indice = static_cast<std::size_t>(i);
        lineas.push_back(detalle::lineaDeOpcion(tema, std::to_string(i + 1),
                                                 selector.opciones()[indice],
                                                 i == selector.seleccion()));
    }
    return lineas;
}

// Las lineas de un BUSCADOR: una por coincidencia, con la elegida invertida.
inline std::vector<ftxui::Element> lineasDeBuscador(const Theme& tema,
                                                     const Buscador& buscador) {
    std::vector<ftxui::Element> lineas;
    lineas.reserve(static_cast<std::size_t>(buscador.cantidadDeLineas()));
    for (int i = 0; i < buscador.cantidadDeLineas(); ++i) {
        lineas.push_back(detalle::lineaDeOpcion(tema, std::to_string(i + 1),
                                                 buscador.lineaDe(i),
                                                 i == buscador.seleccion()));
    }
    return lineas;
}

} // namespace ui
