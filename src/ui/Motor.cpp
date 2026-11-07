#include "Motor.h"

#include <utility>

namespace ui {
namespace detalle {

Motor::Motor(std::function<ftxui::Element()> render,
             std::function<bool(ftxui::Event)> onEvento)
    : _render(std::move(render)), _onEvento(std::move(onEvento)) {
}

ftxui::Element Motor::OnRender() {
    if (!_render) {
        return ftxui::text("");
    }
    return _render();
}

bool Motor::OnEvent(ftxui::Event evento) {
    const bool consumido = _onEvento ? _onEvento(evento) : false;

    // EL REDIBUJADO
    //
    // FTXUI invalida el frame cada vez que procesa un evento (en
    // `App::Internal::HandleTask` el flag `frame_valid_` pasa a false), asi
    // que el menu se mueve solo con las flechas.
    //
    // Igual se pide el redibujado explicitamente, por dos razones:
    // (a) deja el mecanismo a la vista: el que lea esto sabe que el frame
    //     se repinta despues de un evento, y no por accidente;
    // (b) cubre el caso de un cambio de estado que NO viene de un evento
    //     (un temporizador, un hilo), donde FTXUI no invalida nada por su
    //     cuenta. `RequestAnimationFrame()` es el mecanismo de FTXUI v7
    //     para eso: programa un frame de animacion que invalida el frame
    //     pintado.
    if (ftxui::App* app = ftxui::App::Active()) {
        app->RequestAnimationFrame();
    }

    return consumido;
}

} // namespace detalle
} // namespace ui
