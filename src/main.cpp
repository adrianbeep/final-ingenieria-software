#include "ui/Aplicacion.h"
#include "ui/Pantallas/Demo.h"

#include "exceptions/ExcepcionesSistema.h"

#include <iostream>
#include <memory>

// Punto de entrada. NO tiene logica de negocio (requisito 9 de la consigna):
// solo arma la aplicacion y la corre.
//
// El `try/catch` global vive ACA y no en la capa `ui`: la capa de interfaz
// no decide como se le informa un error al usuario.
//
// La pantalla que se muestra es la DEMO del motor (decision D32): el menu
// principal de verdad entra en el PR7.
int main() {
    try {
        ui::Aplicacion aplicacion("Sistema de Turnos");

        aplicacion.ir([](ui::Aplicacion& app) {
            return std::make_unique<ui::Pantallas::Demo>(app);
        });

        // Sin terminal interactiva se imprime el menu en texto plano y se
        // sale con codigo 0 (decision D31). Con terminal, arranca el ciclo
        // de eventos de FTXUI.
        aplicacion.ejecutar([&aplicacion]() { return aplicacion.textoPlanoActual(); });
    } catch (const ExcepcionSistema& e) {
        std::cerr << "Error: " << e.what() << "\n";
    }
    return 0;
}
