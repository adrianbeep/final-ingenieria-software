#pragma once

#include "ui/Theme.h"

#include <ftxui.hpp>

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// La tabla por columnas.
//
// EL ANCHO LO SALE DEL CONTENIDO
//
// Cada columna mide lo que mide su contenido mas largo (la cabecera
// incluida). Asi la tabla se ajusta al dato y no al reves: no hay que
// inventar anchos, ni pasar parametros de ancho, ni dejar columnas de un
// ancho fijo que quedan desproporcionadas en una pantalla angosta.
//
// SE DIBUJA A MANO, CON LOS CARACTERES DEL TEMA
//
// FTXUI tiene su propio `border`, pero usa caracteres propios (y fijos). El
// `Theme` define los caracteres de borde del proyecto, asi que la tabla los
// usa: son UN caracter cada uno, y la tabla se arma con `hbox` y `vbox`.
//
// EL RENDER ES UN `ftxui::Element`
//
// La tabla devuelve el elemento, no un texto: el texto plano se pierde (los
// colores, el ancho de las columnas, la alineacion). Para probarla se
// renderiza en un `ftxui::Screen` y se lee el `ToString()` del screen.
// ---------------------------------------------------------------------------
namespace ui {

class Tabla {
public:
    explicit Tabla(std::vector<std::string> cabeceras);

    // Agrega una fila. Si la fila tiene menos celdas que columnas, las que
    // faltan quedan vacias; si tiene mas, las sobrantes no se dibujan (la
    // tabla tiene la forma que le dieron las cabeceras).
    void agregarFila(std::vector<std::string> celdas);

    // La cantidad de filas de datos (las cabeceras no son filas).
    int filas() const;

    // El render de la tabla.
    ftxui::Element renderizar() const;

private:
    // El ancho de cada columna: el del contenido mas largo, cabecera
    // incluida.
    std::vector<int> anchos() const;

    // La linea de borde horizontal, con la esquina izquierda y la derecha
    // que se le pidan.
    ftxui::Element lineaBorde(const std::vector<int>& anchos, char izquierda,
                               char derecha) const;

    // Una fila de la tabla. `cabecera` decide el rol con el que se pinta.
    ftxui::Element fila(const std::vector<std::string>& celdas,
                        const std::vector<int>& anchos, bool cabecera) const;

    std::vector<std::string> _cabeceras;
    std::vector<std::vector<std::string>> _filas;
    Theme _tema;
};

} // namespace ui
