#include "ui/Consola.h"

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <stdexcept>
#include <string>

namespace ui {

int Consola::validarEntero(const std::string& texto, int min, int max) {
    // El texto se recorta antes de validarlo: " 42 " es un numero que el
    // usuario quiere decir, no un error.
    const std::string limpio = Utils::trim(texto);

    // VACIO
    //
    // Es un caso distinto de "no es un numero": el mensaje dice que no se
    // escribio nada, no que se escribio algo que no es un numero.
    if (limpio.empty()) {
        throw DatoInvalidoException(
            "entrada", texto, "La entrada esta vacia. Ingrese un numero entero.");
    }

    // NO NUMERICO
    if (!Utils::esEntero(limpio)) {
        throw DatoInvalidoException(
            "entrada", texto,
            "Entrada invalida: '" + limpio + "'. Ingrese un numero entero.");
    }

    int valor = 0;
    try {
        valor = std::stoi(limpio);
    } catch (const std::invalid_argument&) {
        // Red de contencion: si el texto "parece" un entero pero `stoi` no lo
        // puede convertir, se trata como entrada invalida.
        throw DatoInvalidoException(
            "entrada", texto,
            "Entrada invalida: '" + limpio + "'. Ingrese un numero entero.");
    } catch (const std::out_of_range&) {
        // El texto ES un entero, pero no entra en un `int`. Es un problema
        // distinto del anterior y merece su propio mensaje: decir "ingrese
        // un numero entero" seria mentiroso.
        throw DatoInvalidoException(
            "entrada", texto,
            "El valor es demasiado grande para un numero entero.");
    }

    // FUERA DE RANGO
    if (valor < min || valor > max) {
        throw DatoInvalidoException(
            "entrada", texto,
            "El valor debe estar entre " + std::to_string(min) + " y " +
                std::to_string(max) + ".");
    }

    return valor;
}

Fecha Consola::validarFecha(const std::string& texto) {
    // `Fecha::desdeTexto` ya valida el FORMATO y la EXISTENCIA de la fecha en
    // el calendario, y ya lanza `DatoInvalidoException` con el campo
    // "fecha". Delegar evita tener dos verdades sobre que es una fecha.
    return Fecha::desdeTexto(texto);
}

Hora Consola::validarHora(const std::string& texto) {
    // Mismo criterio que con la fecha: `Hora::desdeTexto` ya valida el
    // formato, el rango de atencion y la granularidad, y ya lanza con el
    // campo "hora".
    return Hora::desdeTexto(texto);
}

} // namespace ui
