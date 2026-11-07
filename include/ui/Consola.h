#pragma once

#include "utils/Fecha.h"

#include <string>

// ---------------------------------------------------------------------------
// La fachada de VALIDACION de la interfaz.
//
// QUE SE RETIRA Y POR QUE (decision D36)
//
// La Fase 0/1 dejo `limpiar()`, `titulo()` y `leerEntero()`:
//
//   - `limpiar()`: la TUI REDIBUJA, no limpia. Y ademas usaba
//     `std::system("cls")`: un `system()` en la UI es mala practica (lanza
//     una shell, depende del PATH y no se puede testear). La TUI escribe en
//     el alternate screen y el motor se encarga de borrar.
//
//   - `titulo()`: el titulo lo DIBUJA la pantalla con el `Theme` (el rol
//     `Titulo` de la paleta). Imprimirlo a stdout era lo unico que se podia
//     hacer cuando no habia motor.
//
//   - `leerEntero()`: era BLOQUEANTE (leia de `std::cin` en un bucle). En un
//     modelo de eventos no existe una llamada que espere la linea: el texto
//     se acumula en un `ui::Campo` y se valida al presionar Enter. Su
//     contrato (reintentar hasta `MAX_INTENTOS` y lanzar
//     `DatoInvalidoException` con `campo() == "entrada"`) vive ahora en el
//     `Campo`.
//
//   - `pausa()`: la pausa es un ESTADO de pantalla (una pantalla que espera
//     un Enter), no una funcion.
//
// LO QUE QUEDA: FUNCIONES PURAS
//
// Las tres validaciones son funciones puras: reciben el texto y devuelven el
// dato, o lanzan `DatoInvalidoException` con el campo que fallo. Son lo que
// usa el `ui::Campo` como validador, y se testean sin terminal.
//
// LA ESPECIALIDAD NO TIENE VALIDADOR
//
// La especialidad se elige de una lista NUMERADA de doce (un `Menu`), asi que el
// estado invalido es irrepresentable: no hay nada que validar.
// ---------------------------------------------------------------------------
namespace ui {

class Consola {
public:
    // Cantidad maxima de intentos que se le dan al usuario para ingresar un
    // valor valido antes de lanzar `DatoInvalidoException`. La cuenta la
    // hace el `ui::Campo`, que es el que reintenta.
    static constexpr int MAX_INTENTOS = 3;

    // Valida un texto como entero en el rango [min, max].
    //
    // Distingue los casos que se distinguen de verdad (vacio, no numerico,
    // demasiado grande y fuera de rango) y cada mensaje dice la verdad:
    // decir "ingrese un numero entero" a un numero que no entra en un `int`
    // seria mentiroso.
    //
    // Lanza `DatoInvalidoException` con `campo() == "entrada"`.
    static int validarEntero(const std::string& texto, int min, int max);

    // Valida un texto "AAAA-MM-DD".
    //
    // Lanza `DatoInvalidoException` con `campo() == "fecha"`.
    static Fecha validarFecha(const std::string& texto);

    // Valida un texto "HH:MM".
    //
    // Lanza `DatoInvalidoException` con `campo() == "hora"`.
    static Hora validarHora(const std::string& texto);
};

} // namespace ui
