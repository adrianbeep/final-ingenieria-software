#include "ui/Consola.h"

#include <iostream>

// Punto de entrada. En la Fase 0 no tiene logica de negocio (requisito 9 de
// la consigna): solo despacha. El menu real llega en la Fase 6.
int main() {
    ui::Consola::limpiar();
    ui::Consola::titulo("Sistema de Turnos");
    std::cout << "Andamiaje listo. Fase 0 completada.\n";
    return 0;
}
