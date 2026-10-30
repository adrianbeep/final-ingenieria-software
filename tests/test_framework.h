#pragma once

#include "exceptions/ExcepcionesSistema.h"

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

// ---------------------------------------------------------------------------
// Helpers compartidos para probar el lanzamiento de excepciones del dominio.
// Van en el ambito global (y no en `namespace testing`) porque las llamadas
// de los tests son `lanzaConCampo(...)` a secas, sin calificar.
// ---------------------------------------------------------------------------
// Lo que se aprendio de la excepcion: si lanzo la esperada, el campo, el
// valor y el mensaje quedan capturados.
struct ErrorCapturado {
    bool lanzo = false;
    bool lanzoOtraExcepcion = false;
    std::string campo;
    std::string valor;
    std::string mensaje;
};

template <typename F>
ErrorCapturado capturarError(F funcion) {
    ErrorCapturado error;
    try {
        funcion();
    } catch (const DatoInvalidoException& excepcion) {
        error.lanzo = true;
        error.campo = excepcion.campo();
        error.valor = excepcion.valor();
        error.mensaje = excepcion.what();
    } catch (...) {
        error.lanzoOtraExcepcion = true;
    }
    return error;
}

// Dice si la funcion lanzo `DatoInvalidoException` con el campo esperado.
template <typename F>
bool lanzaConCampo(F funcion, const std::string& campoEsperado) {
    const ErrorCapturado error = capturarError(funcion);
    return error.lanzo && error.campo == campoEsperado;
}

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
