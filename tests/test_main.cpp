#include "test_framework.h"

#include <iostream>

// Test de humo: verifica que el arnes funciona. Si esto falla, el problema
// esta en el arnes y no en el codigo de la aplicacion.
TEST_CASE("el arnes de tests funciona", arnes_funciona) {
    CHECK(true);
    CHECK(1 + 1 == 2);
}

int main() {
    std::cout << "Ejecutando " << testing::registro().size() << " caso(s)...\n\n";

    int ejecutados = 0;
    for (const auto& caso : testing::registro()) {
        const int antes = testing::fallos();
        caso.fn();
        ++ejecutados;
        const bool ok = (testing::fallos() == antes);
        std::cout << (ok ? "[  OK  ] " : "[ FAIL ] ") << caso.nombre << "\n";
    }

    std::cout << "\n" << ejecutados << " caso(s), " << testing::fallos()
              << " fallo(s)\n";

    // Codigo de salida != 0 si algo fallo: es lo que leen `make test` y CTest.
    return testing::fallos() == 0 ? 0 : 1;
}
