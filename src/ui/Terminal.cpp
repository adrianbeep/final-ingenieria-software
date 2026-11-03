#include "ui/Terminal.h"

#include "Motor.h"

#include <memory>

#if defined(_WIN32)
  #include <io.h>
#else
  #include <unistd.h>
#endif

namespace ui {
namespace {

// La App de FTXUI vive mientras dura el ciclo de eventos.
//
// Es un `unique_ptr` y no una `App` en el stack porque `ftxui::App` es
// MOVIL y no copiable: se construye con `App::Fullscreen()` (el alternate
// screen) y se deja en el heap para poder sacarla y ponerla.
std::unique_ptr<ftxui::App>& appActiva() {
    static std::unique_ptr<ftxui::App> instancia;
    return instancia;
}

} // namespace

Evento eventoDesde(const ftxui::Event& evento) {
    Evento salida;

    // EL RATON
    //
    // FTXUI entrega las coordenadas ya en base 0: las del terminal son de
    // base 1 y FTXUI les resta el origen del dibujo (`App::Internal::HandleTask`
    // les resta la posicion del cursor, que por defecto es 1,1). Por eso las
    // coordenadas se copian tal cual: son las mismas filas y columnas que ve
    // la pantalla.
    //
    // El `const_cast` es por la API de FTXUI: `Event::mouse()` es el unico
    // acceso al raton y NO es `const` (los eventos de FTXUI se modifican en
    // el camino). Aca solo se lee.
    if (evento.is_mouse()) {
        const ftxui::Mouse& mouse = const_cast<ftxui::Event&>(evento).mouse();
        salida.x = mouse.x;
        salida.y = mouse.y;

        // Solo se traduce el boton APRETADO. Soltar el boton o mover el
        // raton no son acciones: llegan con `BotonMouse::Ninguno`, y asi
        // `Evento::esClic()` sigue significando "clic", y no "el raton esta
        // en algum lado".
        if (mouse.motion == ftxui::Mouse::Pressed) {
            switch (mouse.button) {
                case ftxui::Mouse::Left:
                    salida.boton = BotonMouse::Izquierdo;
                    break;
                case ftxui::Mouse::Right:
                    salida.boton = BotonMouse::Derecho;
                    break;
                case ftxui::Mouse::WheelUp:
                    salida.boton = BotonMouse::RuedaArriba;
                    break;
                case ftxui::Mouse::WheelDown:
                    salida.boton = BotonMouse::RuedaAbajo;
                    break;
                default:
                    salida.boton = BotonMouse::Ninguno;
                    break;
            }
        }
        return salida;
    }

    // UN CARACTER
    //
    // El ESPACIO es un caracter como cualquier otro: FTXUI no tiene un
    // `Event::Space` (se verifica en el header vendorizado), asi que llega
    // como `Event::Character(' ')`.
    if (evento.is_character()) {
        const std::string& caracter = evento.character();
        if (caracter == " ") {
            salida.tipo = TipoTecla::Espacio;
            return salida;
        }
        salida.tipo = TipoTecla::Caracter;
        salida.caracter = caracter;
        return salida;
    }

    // LAS TECLAS ESPECIALES
    //
    // FTXUI las expone como constantes estaticas y las compara por su
    // contenido. OJO: la tecla ENTER en FTXUI v7 se llama `Event::Return`
    // (no existe `Event::Enter`).
    if (evento == ftxui::Event::ArrowUp) {
        salida.tipo = TipoTecla::FlechaArriba;
    } else if (evento == ftxui::Event::ArrowDown) {
        salida.tipo = TipoTecla::FlechaAbajo;
    } else if (evento == ftxui::Event::ArrowLeft) {
        salida.tipo = TipoTecla::FlechaIzquierda;
    } else if (evento == ftxui::Event::ArrowRight) {
        salida.tipo = TipoTecla::FlechaDerecha;
    } else if (evento == ftxui::Event::Return) {
        salida.tipo = TipoTecla::Enter;
    } else if (evento == ftxui::Event::Escape) {
        salida.tipo = TipoTecla::Escape;
    } else if (evento == ftxui::Event::Tab) {
        salida.tipo = TipoTecla::Tabulador;
    } else if (evento == ftxui::Event::Backspace) {
        salida.tipo = TipoTecla::Retroceso;
    }

    // Lo que no se tradujo queda con el tipo por defecto (`Caracter`) y sin
    // caracter: un evento que no hace nada.
    return salida;
}

bool Terminal::hayTerminal() {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(STDOUT_FILENO) != 0;
#endif
}

void Terminal::iniciar() {
    // Idempotente: iniciar dos veces no crea dos Apps.
    if (appActiva()) {
        return;
    }
    appActiva() = std::make_unique<ftxui::App>(ftxui::App::Fullscreen());
    // El rastreo del raton viene activado por defecto en FTXUI v7, pero se
    // activa explicitamente: la aplicacion lo quiere, y que quede a la vista
    // es mejor que depender de un default.
    appActiva()->TrackMouse(true);
}

void Terminal::terminar() {
    // Idempotente: terminar dos veces no hace nada.
    if (!appActiva()) {
        return;
    }
    appActiva().reset();
}

int Terminal::ancho() {
    return ftxui::Terminal::Size().dimx;
}

int Terminal::alto() {
    return ftxui::Terminal::Size().dimy;
}

ftxui::App* Terminal::app() {
    return appActiva().get();
}

void Terminal::ejecutarCiclo(std::function<ftxui::Element()> render,
                             std::function<bool(ftxui::Event)> onEvento) {
    iniciar();
    if (!appActiva()) {
        return;
    }
    auto motor = std::make_shared<detalle::Motor>(std::move(render), std::move(onEvento));
    try {
        appActiva()->Loop(motor);
    } catch (...) {
        // El ciclo termino con una excepcion: el terminal se restaura igual
        // y la excepcion sigue de largo (la maneja el `try/catch` global de
        // `main.cpp`).
        terminar();
        throw;
    }
    terminar();
}

} // namespace ui
