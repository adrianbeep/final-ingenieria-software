#pragma once

#include "ui/Theme.h"

#include <ftxui.hpp>

namespace ui {

// Traduce un color PROPIO al color de FTXUI.
//
// Es la UNICA pieza que conoce las dos representaciones, y por eso vive
// sola en su archivo: la paleta (`ui::Theme`) no depende de FTXUI, pero el
// dibujo si.
//
// La conversion no pierde nada: FTXUI tiene `Color::RGB(r, g, b)` con los
// tres componentes en `uint8_t`, que es exactamente el rango de nuestro
// color.
ftxui::Color aFtxui(const Color& color);

} // namespace ui
