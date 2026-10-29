#pragma once

#include <iostream>
#include <string>
#include <vector>

// Arnes de tests propio. Sin framework externo: la consigna no pide uno y
// meter gtest agrega una dependencia externa que en la defensa se discute.
//
// Uso:
//   TEST_CASE("nombre del caso") {
//       CHECK(algo == true);
//   }
//
// Un CHECK fallido NO corta el caso: acumula el fallo y sigue, para que una
// ejecucion muestre TODOS los problemas de una vez.

namespace testing {

struct Caso {
    std::string nombre;
    void (*fn)();
};

inline std::vector<Caso>& registro() {
    static std::vector<Caso> casos;
    return casos;
}

inline int& fallos() {
    static int f = 0;
    return f;
}

struct Registrar {
    Registrar(const std::string& nombre, void (*fn)()) {
        registro().push_back(Caso{nombre, fn});
    }
};

} // namespace testing

// TEST_CASE(descripcion, funcion)
//   descripcion: texto que se muestra en la salida (puede tener espacios)
//   funcion:     identificador unico del caso (se usa para el registro)
#define TEST_CASE(descripcion, funcion)                                      \
    static void funcion();                                                    \
    static testing::Registrar registrar_##funcion(descripcion, &funcion);     \
    static void funcion()

#define CHECK(condicion)                                                     \
    do {                                                                      \
        if (!(condicion)) {                                                   \
            std::cerr << "    FALLO: " << #condicion                           \
                      << "  (" << __FILE__ << ":" << __LINE__ << ")\n";         \
            ++testing::fallos();                                              \
        }                                                                     \
    } while (false)
