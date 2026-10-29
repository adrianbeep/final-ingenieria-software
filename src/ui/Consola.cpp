#include "ui/Consola.h"

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

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

int Consola::leerEntero(const std::string& prompt, int min, int max) {
    std::string linea;
    std::string ultimoIntento;  // ultimo texto no vacio que se intento ingresar

    for (int intento = 1; intento <= MAX_INTENTOS; ++intento) {
        std::cout << prompt << std::flush;

        // `std::cin >> std::ws` se come el '\n' que dejo la lectura anterior:
        // es el bug clasico de `std::cin >> int`, que ademas deja basura en el
        // buffer si el usuario escribe algo que no es un numero.
        std::cin >> std::ws;
        // `std::getline` no vacia la cadena si el stream ya esta en error, asi
        // que se limpia antes de leer.
        linea.clear();
        std::getline(std::cin, linea);
        linea = Utils::trim(linea);

        if (!linea.empty()) {
            ultimoIntento = linea;
        }

        // Vacio o no numerico: se informa y se reintenta.
        if (linea.empty() || !Utils::esEntero(linea)) {
            std::cerr << "Entrada invalida: '" << linea << "'. Ingrese un numero entero.\n";
            continue;
        }

        int valor = 0;
        try {
            valor = std::stoi(linea);
        } catch (const std::invalid_argument&) {
            // Red de contencion: si el texto "parece" un entero pero `stoi` no
            // lo puede convertir, se trata como entrada invalida y se reintenta.
            std::cerr << "Entrada invalida: '" << linea << "'. Ingrese un numero entero.\n";
            continue;
        } catch (const std::out_of_range&) {
            // El texto ES un entero, pero no entra en un `int`. Es un problema
            // distinto del anterior y merece su propio mensaje: decir "ingrese
            // un numero entero" seria mentiroso.
            std::cerr << "El valor es demasiado grande para un numero entero.\n";
            continue;
        }

        if (valor < min || valor > max) {
            std::cerr << "El valor debe estar entre " << min << " y " << max << ".\n";
            continue;
        }

        return valor;
    }

    // Se agotaron los intentos. Se informa y se propaga el problema como
    // excepcion del dominio: la UI de arriba decide como seguir.
    std::cerr << "No se ingreso ningun valor valido.\n";
    throw DatoInvalidoException("entrada", ultimoIntento,
                                "se esperaba un entero entre min y max");
}

} // namespace ui
