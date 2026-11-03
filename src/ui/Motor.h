#pragma once

#include <ftxui.hpp>

#include <functional>

// ---------------------------------------------------------------------------
// El componente de motor.
//
// Es INTERNO de la capa `ui`: vive en `src/ui/`, no en `include/ui/`, porque
// nadie fuera de la capa tiene por que construirlo.
//
// POR QUE ES PROPIO Y NO UN `ftxui::Renderer`
//
// FTXUI redibuja POR EVENTO: hace falta un componente que, en cada evento,
// se lo pase a la pantalla y despues devuelva el frame nuevo. Ese "pegado"
// entre el render y los eventos es lo que hace esta clase.
// ---------------------------------------------------------------------------
namespace ui {
namespace detalle {

class Motor : public ftxui::ComponentBase {
public:
    Motor(std::function<ftxui::Element()> render,
          std::function<bool(ftxui::Event)> onEvento);

    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event evento) override;

private:
    std::function<ftxui::Element()> _render;
    std::function<bool(ftxui::Event)> _onEvento;
};

} // namespace detalle
} // namespace ui
