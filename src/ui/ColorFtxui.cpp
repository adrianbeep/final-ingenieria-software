#include "ui/ColorFtxui.h"

#include <cstdint>

namespace ui {
namespace {

// Recorta un componente de color al rango valido de FTXUI (0 a 255).
//
// `Color::RGB` toma `uint8_t`: un valor fuera de rango "daria la vuelta" y
// produciria OTRO color (por ejemplo, 256 -> 0). Recortar deja el color
// cerca del que se queria, en vez de convertirlo en otro.
uint8_t acota(int valor) {
    if (valor < 0) {
        return 0;
    }
    if (valor > 255) {
        return 255;
    }
    return static_cast<uint8_t>(valor);
}

} // namespace

ftxui::Color aFtxui(const Color& color) {
    return ftxui::Color::RGB(acota(color.r), acota(color.g), acota(color.b));
}

} // namespace ui
