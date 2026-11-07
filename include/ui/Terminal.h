#pragma once

#include "ui/Evento.h"

#include <ftxui.hpp>

#include <functional>

// ---------------------------------------------------------------------------
// El terminal: el ciclo de vida del modo interactivo.
//
// LA FRONTERA CON FTXUI
//
// Este es el ADAPTER: es el unico lugar de la capa `ui` (junto con
// `ui::Motor`) que sabe que existe FTXUI. El resto de la capa habla con
// `ui::Evento`, que es nuestro.
// ---------------------------------------------------------------------------
namespace ui {

// Traduce un evento de FTXUI a NUESTRO evento.
//
// Es una funcion PURA: no toca el terminal, no lee el estado de nada y
// siempre devuelve lo mismo para la misma entrada. Por eso se puede
// testear sin terminal.
//
// LO QUE NO NOS INTERESA
//
// Los tipos de evento que FTXUI entrega y la capa `ui` no usa (el cursor, la
// forma del cursor, el nombre del terminal) se traducen a un `Evento` que no
// hace nada: el `Evento` vuelve con su tipo por defecto y sin caracter, y
// `Menu::aplicarEvento` lo rechaza.
Evento eventoDesde(const ftxui::Event& evento);

class Terminal {
public:
    // Si hay una terminal INTERACTIVA.
    //
    // ES EL GUARD OBLIGATORIO (decisiones D16 y D31): `App::Loop()` sin TTY
    // NO RETORNA NUNCA (esta MEDIDO: 15 s sin salida, matado a mano). Sin
    // este chequeo, el programa se cuelga en el CI, donde la salida es un
    // pipe.
    static bool hayTerminal();

    // Prepara el terminal para el modo interactivo (idempotente).
    //
    // FTXUI entra al alternate screen y al modo raw DENTRO de `App::Loop()`
    // (`App::Install()` es privado), asi que `iniciar()` deja el terminal
    // listo: crea la App de FTXUI, que es la que usa el alternate screen.
    static void iniciar();

    // Restaura el terminal (idempotente).
    static void terminar();

    // El ancho y el alto del terminal, en caracteres.
    static int ancho();
    static int alto();

    // La App de FTXUI con la que corre el ciclo de eventos, o nullptr si el
    // terminal no esta iniciado.
    static ftxui::App* app();

    // Corre el ciclo de eventos: `render` produce el frame y `onEvento`
    // recibe cada evento. Al terminar, el terminal queda como estaba.
    static void ejecutarCiclo(std::function<ftxui::Element()> render,
                              std::function<bool(ftxui::Event)> onEvento);
};

} // namespace ui
