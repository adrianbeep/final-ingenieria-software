#include "ui/Consola.h"

#include <cstdlib>
#include <iostream>

namespace ui {

void Consola::limpiar() {
#if defined(_WIN32) || defined(_WIN64)
    std::system("cls");
#else
    std::system("clear");
#endif
}

void Consola::titulo(const std::string& texto) {
    std::cout << "\n=== " << texto << " ===\n";
}

} // namespace ui
