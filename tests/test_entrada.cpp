#include "test_framework.h"

#include "exceptions/ExcepcionesSistema.h"
#include "ui/Consola.h"

#include <iostream>
#include <sstream>
#include <string>

// ---------------------------------------------------------------------------
// Tests de la lectura segura de enteros desde la consola.
//
// Se redirige `std::cin` a un `std::istringstream` con el contenido que se
// quiere "escribir" y se restaura el buffer original al final, ASIEMPRE
// (tambien si el test falla o lanza): si no, los tests siguientes leen de
// un stream roto.
// ---------------------------------------------------------------------------

namespace {

// Reemplaza los buffers de la consola (entrada, salida y error) por buffers
// de memoria durante el tiempo de vida del objeto.
class ConsolaSimulada {
public:
    explicit ConsolaSimulada(const std::string& contenido)
        : entrada_(contenido),
          originalEntrada_(std::cin.rdbuf(entrada_.rdbuf())),
          originalSalida_(std::cout.rdbuf(salida_.rdbuf())),
          originalError_(std::cerr.rdbuf(error_.rdbuf())) {
        // El stream puede venir con flags de error de una lectura anterior:
        // se limpian para que este test parta de un estado conocido.
        std::cin.clear();
    }

    ~ConsolaSimulada() {
        std::cin.rdbuf(originalEntrada_);
        std::cout.rdbuf(originalSalida_);
        std::cerr.rdbuf(originalError_);
        std::cin.clear();
    }

    ConsolaSimulada(const ConsolaSimulada&) = delete;
    ConsolaSimulada& operator=(const ConsolaSimulada&) = delete;

private:
    std::istringstream entrada_;
    std::ostringstream salida_;
    std::ostringstream error_;
    std::streambuf* originalEntrada_;
    std::streambuf* originalSalida_;
    std::streambuf* originalError_;
};

} // namespace

TEST_CASE("leerEntero descarta la entrada no numerica y reintenta", entrada_reintenta) {
    ConsolaSimulada consola("abc\n42\n");

    int edad = -1;
    bool lanzo = false;
    try {
        edad = ui::Consola::leerEntero("Ingrese la edad: ", 0, 120);
    } catch (...) {
        lanzo = true;
    }

    CHECK(!lanzo);
    CHECK(edad == 42);
}

TEST_CASE("leerEntero reintenta si el valor esta fuera de rango", entrada_fuera_de_rango) {
    ConsolaSimulada consola("999\n50\n");

    int edad = -1;
    bool lanzo = false;
    try {
        edad = ui::Consola::leerEntero("Ingrese la edad: ", 0, 120);
    } catch (...) {
        lanzo = true;
    }

    CHECK(!lanzo);
    CHECK(edad == 50);
}

TEST_CASE("leerEntero lanza DatoInvalidoException si nunca hay un valor valido", entrada_sin_valor_valido) {
    ConsolaSimulada consola("abc\n");

    bool lanzoDatoInvalido = false;
    try {
        (void)ui::Consola::leerEntero("Ingrese la edad: ", 0, 120);
    } catch (const DatoInvalidoException& e) {
        lanzoDatoInvalido = true;
        CHECK(e.campo() == "entrada");
        CHECK(std::string(e.what()).size() > 0);
    } catch (...) {
    }

    CHECK(lanzoDatoInvalido);
}

TEST_CASE("leerEntero lanza DatoInvalidoException si el valor nunca entra en rango", entrada_valor_negativo) {
    ConsolaSimulada consola("-5\n");

    bool lanzoDatoInvalido = false;
    try {
        (void)ui::Consola::leerEntero("Ingrese la edad: ", 0, 120);
    } catch (const DatoInvalidoException& e) {
        lanzoDatoInvalido = true;
        CHECK(e.campo() == "entrada");
    } catch (...) {
    }

    CHECK(lanzoDatoInvalido);
}
