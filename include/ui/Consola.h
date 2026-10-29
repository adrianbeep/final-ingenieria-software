#pragma once

#include <string>

namespace ui {

// Fachada de la consola. En la Fase 0 solo tiene lo indispensable:
// limpiar() y titulo(). El resto (tabla, pausa, exito, error) llega en la
// Fase 6.
class Consola {
public:
    static void limpiar();
    static void titulo(const std::string& texto);
};

} // namespace ui
