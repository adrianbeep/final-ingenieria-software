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
// LOS ROLES SON POR CELDA
//
// Una tabla no es toda del mismo color: la cabecera es un rotulo, el id es
// una referencia y el estado DICE como esta la fila. Por eso una fila puede
// llevar el rol de cada celda: el que la arma (la pantalla) es el que sabe
// que celda es el estado.
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
    // UNA FILA DE LA TABLA
    //
    // Son las celdas, el rol de cada una y si la fila va atenuada.
    //
    // ES PUBLICA PORQUE LA ARMA LA PANTALLA
    //
    // La pantalla es la que sabe que celda es el id y que celda es el
    // estado: la tabla solo aplica los roles que le pasan.
    struct Fila {
        std::vector<std::string> celdas;
        std::vector<Rol> roles;
        bool atenuada = false;
    };

    explicit Tabla(std::vector<std::string> cabeceras);

    // Agrega una fila. Si la fila tiene menos celdas que columnas, las que
    // faltan quedan vacias; si tiene mas, las sobrantes no se dibujan (la
    // tabla tiene la forma que le dieron las cabeceras).
    //
    // Las celdas van con el rol de celda (el color por defecto de la
    // terminal).
    void agregarFila(std::vector<std::string> celdas);

    // Agrega una fila con el ROL de cada celda.
    //
    // El vector de roles es PARALELO al de celdas: si le falta un rol (o el
    // vector esta vacio), esa celda va con el rol de celda.
    void agregarFila(std::vector<std::string> celdas, const std::vector<Rol>& roles);

    // Agrega una fila ATENUADA.
    //
    // La entidad esta dada de baja: la fila se dibuja con el rol secundario,
    // para que se vea que esta pero que no compita con las que estan vivas.
    void agregarFilaAtenuada(std::vector<std::string> celdas);

    // Agrega una fila armada (con sus roles y su atenuacion).
    void agregarFila(const Fila& fila);

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
    ftxui::Element fila(const Fila& fila, const std::vector<int>& anchos,
                        bool cabecera) const;

    // El rol con el que se pinta una celda.
    //
    // El rol explicito de la fila gana; si no hay, una fila atenuada va con
    // el rol secundario y una fila normal con el rol de celda.
    Rol rolDeCelda(const Fila& fila, std::size_t columna) const;

    std::vector<std::string> _cabeceras;
    std::vector<Fila> _filas;
    Theme _tema;
};

} // namespace ui
