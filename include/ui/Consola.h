#pragma once

#include <string>

namespace ui {

// Fachada de la consola. En la Fase 0 solo tiene lo indispensable:
// limpiar() y titulo(). El resto (tabla, pausa, exito, error) llega en la
// Fase 6.
class Consola {
public:
    // Cantidad maxima de intentos que se le dan al usuario para ingresar un
    // valor valido antes de_THROW: al agotarlos, `leerEntero` lanza
    // `DatoInvalidoException` para que la capa de arriba decida que hacer.
    static constexpr int MAX_INTENTOS = 3;

    static void limpiar();
    static void titulo(const std::string& texto);

    // Pide un entero por consola y lo devuelve solo si esta dentro del rango
    // [min, max]. Reintenta hasta MAX_INTENTOS veces y, si no obtiene nada
    // valido, lanza `DatoInvalidoException`.
    // NUNCA se usa `std::cin >> int`: deja basura en el buffer y, peor,
    // lanza excepcion si el usuario escribe letras. Se lee linea completa
    // con `std::getline` y se valida el texto.
    static int leerEntero(const std::string& prompt, int min, int max);
};

} // namespace ui
