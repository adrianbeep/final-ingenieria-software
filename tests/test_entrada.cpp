#include "test_framework.h"

#include "exceptions/ExcepcionesSistema.h"
#include "ui/Campo.h"
#include "ui/Consola.h"

#include <string>

// ---------------------------------------------------------------------------
// Tests del CONTRATO DE ENTRADA: reintentar hasta `MAX_INTENTOS` y, si el
// usuario no llega a un valor valido, lanzar `DatoInvalidoException` con
// `campo() == "entrada"`.
//
// QUE CAMBIO Y POR QUE
//
// La Fase 1 probaba este contrato sobre `Consola::leerEntero`, que leia de
// `std::cin` (con un `std::istringstream` en el lugar del buffer). En la TUI
// no hay ninguna llamada que espere la linea: el texto se acumula en un
// `ui::Campo` y se valida al presionar Enter.
//
// Los tests de abajo manejan el campo con EVENTOS INYECTADOS (teclas y
// Enter) y prueban EXACTAMENTE lo mismo que probaba la Fase 1: que un dato
// invalido no termina el programa, que se reintenta, y que al agotar los
// intentos se propaga la excepcion del dominio.
// ---------------------------------------------------------------------------

namespace {

// Un evento de un caracter.
ui::Evento eventoCaracter(char caracter) {
    ui::Evento evento;
    evento.tipo = ui::TipoTecla::Caracter;
    evento.caracter = std::string(1, caracter);
    return evento;
}

// El evento Enter.
ui::Evento eventoEnter() {
    ui::Evento evento;
    evento.tipo = ui::TipoTecla::Enter;
    return evento;
}

// El evento Retroceso.
ui::Evento eventoRetroceso() {
    ui::Evento evento;
    evento.tipo = ui::TipoTecla::Retroceso;
    return evento;
}

// Escribe un texto en el campo.
//
// EL TEXTO NO SE BORRA SOLO
//
// Cuando un intento es invalido, el campo DEJA el texto: el usuario corrige
// lo que escribio. En el test, "corregir" es borrar con Retroceso y
// escribir de nuevo: es exactamente lo que hace el usuario con el teclado.
void escribir(ui::Campo& campo, const std::string& texto) {
    while (!campo.texto().empty()) {
        campo.aplicarEvento(eventoRetroceso());
    }
    for (const char caracter : texto) {
        campo.aplicarEvento(eventoCaracter(caracter));
    }
}

// Un campo que valida un entero en el rango [min, max].
//
// El validador es el mismo que usara la pantalla: `Consola::validarEntero`
// devuelve el dato o lanza; el campo necesita el MENSAJE, asi que el
// adaptador lo saca de la excepcion.
ui::Campo campoEntero(int min, int max) {
    return ui::Campo("Edad", "", [min, max](const std::string& texto) -> std::string {
        try {
            (void)ui::Consola::validarEntero(texto, min, max);
            return "";
        } catch (const DatoInvalidoException& e) {
            return e.motivo();
        }
    });
}

} // namespace

TEST_CASE("el campo descarta la entrada no numerica y reintenta", entrada_reintenta) {
    ui::Campo campo = campoEntero(0, 120);

    escribir(campo, "abc");
    campo.aplicarEvento(eventoEnter());

    // El intento fallo: el campo lo muestra y el usuario puede reintentar.
    CHECK(campo.intentos() == 1);
    CHECK(campo.error().empty() == false);
    CHECK(campo.confirmado() == false);
    // El texto NO se borro: el usuario corrige lo que escribio.
    CHECK(campo.texto() == "abc");

    escribir(campo, "42");
    campo.aplicarEvento(eventoEnter());

    CHECK(campo.confirmado() == true);
    CHECK(campo.error().empty() == true);
    CHECK(campo.valor() == "42");
    CHECK(ui::Consola::validarEntero(campo.valor(), 0, 120) == 42);
}

TEST_CASE("el campo reintenta si el valor esta fuera de rango", entrada_fuera_de_rango) {
    ui::Campo campo = campoEntero(0, 120);

    escribir(campo, "999");
    campo.aplicarEvento(eventoEnter());

    CHECK(campo.intentos() == 1);
    CHECK(campo.confirmado() == false);
    CHECK(campo.error().empty() == false);

    escribir(campo, "50");
    campo.aplicarEvento(eventoEnter());

    CHECK(campo.confirmado() == true);
    CHECK(campo.valor() == "50");
    CHECK(ui::Consola::validarEntero(campo.valor(), 0, 120) == 50);
}

TEST_CASE("el campo lanza DatoInvalidoException si nunca hay un valor valido",
           entrada_sin_valor_valido) {
    ui::Campo campo = campoEntero(0, 120);

    bool lanzoDatoInvalido = false;
    try {
        for (int intento = 0; intento < ui::Consola::MAX_INTENTOS; ++intento) {
            escribir(campo, "abc");
            campo.aplicarEvento(eventoEnter());
        }
    } catch (const DatoInvalidoException& e) {
        lanzoDatoInvalido = true;
        CHECK(e.campo() == "entrada");
        CHECK(std::string(e.what()).size() > 0);
    } catch (...) {
    }

    CHECK(lanzoDatoInvalido);
}

TEST_CASE("el campo lanza DatoInvalidoException si el valor nunca entra en rango",
           entrada_valor_fuera_de_rango) {
    ui::Campo campo = campoEntero(0, 120);

    bool lanzoDatoInvalido = false;
    try {
        for (int intento = 0; intento < ui::Consola::MAX_INTENTOS; ++intento) {
            escribir(campo, "999");
            campo.aplicarEvento(eventoEnter());
        }
    } catch (const DatoInvalidoException& e) {
        lanzoDatoInvalido = true;
        CHECK(e.campo() == "entrada");
    } catch (...) {
    }

    CHECK(lanzoDatoInvalido);
}
