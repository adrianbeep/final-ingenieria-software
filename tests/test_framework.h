#pragma once

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Fecha.h"

#include <ctime>
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

// ---------------------------------------------------------------------------
// LAS FECHAS DE PRUEBA
//
// La regla de la fecha vencida la aplica el SERVICIO de turnos: un turno
// cuya fecha ya paso se rechaza. Un test que agenda tiene que usar una
// fecha que NO haya pasado, y una fecha fija dejaria de servir el dia que
// se cumpliera. Por eso la fecha se deriva del reloj del sistema.
//
// Estas helpers viven en el arnes (y no en un archivo de test) porque las
// necesitan los tests del SERVICIO y los de la PANTALLA de turnos.
// ---------------------------------------------------------------------------

// La fecha de HOY, segun el reloj del sistema. Es la misma referencia que
// usa el servicio para la regla de la fecha vencida.
inline Fecha hoyDelSistema() {
    const std::time_t ahora = std::time(nullptr);
    const std::tm* local = std::localtime(&ahora);
    return Fecha(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
}

// Una fecha `dias` dias a partir de hoy (negativo = antes).
inline Fecha fechaDesplazada(int dias) {
    const Fecha hoy = hoyDelSistema();
    int anio = hoy.anio;
    int mes = hoy.mes;
    int dia = hoy.dia;

    while (dias > 0) {
        ++dia;
        if (dia > Fecha::diasDelMes(anio, mes)) {
            dia = 1;
            ++mes;
            if (mes > 12) {
                mes = 1;
                ++anio;
            }
        }
        --dias;
    }

    while (dias < 0) {
        --dia;
        if (dia < 1) {
            --mes;
            if (mes < 1) {
                mes = 12;
                --anio;
            }
            dia = Fecha::diasDelMes(anio, mes);
        }
        ++dias;
    }

    return Fecha(anio, mes, dia);
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
