#include "ui/Consola.h"

#include "exceptions/ExcepcionesSistema.h"

#include <iostream>

// Punto de entrada. No tiene logica de negocio (requisito 9 de la consigna):
// solo despacha. La Fase 1 demuestra el manejo de entrada segura: si el
// usuario escribe algo que no es un numero, NO se rompe el programa.
int main() {
    ui::Consola::limpiar();
    ui::Consola::titulo("Sistema de Turnos");
    try {
        const int edad = ui::Consola::leerEntero("Ingrese la edad: ", 0, 120);
        std::cout << "Edad registrada: " << edad << "\n";
    } catch (const ExcepcionSistema& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
