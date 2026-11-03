#pragma once

#include <string>

// ---------------------------------------------------------------------------
// El evento de la capa `ui`.
//
// POR QUE NO ES UN `ftxui::Event`
//
// FTXUI ya tiene su evento (`ftxui::Event`), pero si las pantallas lo
// usaran, la frontera de la capa seria porosa: cualquier pantalla podria
// empezar a depender de detalles del motor (las secuencias de escape, los
// codigos de control) y el motor quedaria pegado a FTXUI.
//
// Este evento es NUESTRO. La traduccion desde `ftxui::Event` vive en una
// sola funcion (`ui::eventoDesde`), que es PURA: por eso se puede testear
// sin terminal y sin abrir el ciclo de eventos.
// ---------------------------------------------------------------------------
namespace ui {

// Que tecla se aperto.
enum class TipoTecla {
    Caracter,
    Enter,
    Escape,
    Espacio,
    FlechaArriba,
    FlechaAbajo,
    FlechaIzquierda,
    FlechaDerecha,
    Tabulador,
    Retroceso
};

// Que boton del raton se moverio.
enum class BotonMouse {
    Izquierdo,
    Derecho,
    RuedaArriba,
    RuedaAbajo,
    Ninguno
};

struct Evento {
    TipoTecla tipo = TipoTecla::Caracter;
    std::string caracter;      // valido si tipo == Caracter
    BotonMouse boton = BotonMouse::Ninguno;
    int x = -1;                // columna del mouse, 0-based
    int y = -1;                // fila del mouse, 0-based

    // Un clic es el boton IZQUIERDO APRETADO.
    //
    // La traduccion deja el boton en `Izquierdo` solo cuando el boton esta
    // apretado: soltar el boton o mover el raton con el boton apretado no
    // son un clic, y llegan con `BotonMouse::Ninguno`.
    bool esClic() const { return boton == BotonMouse::Izquierdo; }

    // Si el evento trae coordenadas de mouse.
    //
    // Las coordenadas validas son >= 0: un -1 significa "este evento no vino
    // del raton". No hace falta un tipo de evento aparte para el raton: lo
    // que lo distingue es que trae coordenadas.
    bool esMouse() const { return x >= 0 && y >= 0; }
};

} // namespace ui
