#pragma once

#include "ui/Evento.h"

#include <cstddef>
#include <functional>
#include <string>

// ---------------------------------------------------------------------------
// El campo de entrada de texto.
//
// EL TEXTO SE ACUMULA, NO SE PIDE
//
// En la consola clasica, leer un dato era una LLAMADA que esperaba la linea:
// `Consola::leerEntero` imprimia el prompt, leia de `std::cin` y reintentaba
// en un bucle. En una TUI no hay ninguna llamada que espere: el usuario
// escribe, la pantalla redibuja, y el texto se acumula en este widget. El
// dato se VALIDA cuando el usuario aprieta Enter.
//
// EL CONTRATO DE LA FASE 1 SOBREVIVE ENTERO (decision D34 y D35)
//
// La Fase 1 prometo tres reintentos y despues una `DatoInvalidoException`
// con `campo() == "entrada"`. Ese contrato se conserva: el campo cuenta los
// intentos invalidos y, al agotar `Consola::MAX_INTENTOS`, lanza la misma
// excepcion. Lo que cambia es DONDE se ve el error: antes terminaba en el
// `catch` global (que mataba el programa) y ahora se muestra en la pantalla
// y el usuario reintenta.
//
// EL TEXTO NO SE BORRA AL FALLAR
//
// Cuando el intento es invalido, el texto queda como estaba: el usuario
// TIENE que corregir lo que escribio, no volver a escribirlo desde cero.
// Borrarlo seria tirar su trabajo (un nombre de veinte caracteres) por un
// error de una letra.
// ---------------------------------------------------------------------------
namespace ui {

class Campo {
public:
    // El validador recibe el texto y DEVELVE el mensaje de error, o una
    // cadena vacia si el texto sirve.
    //
    // POR QUE DEVUELVE EL MENSAJE Y NO LANZA
    //
    // El campo necesita MOSTRAR el error (en la pantalla) y recien despues,
    // si se agotan los intentos, propagarlo como excepcion. Un validador que
    // lanzaria obligaria al campo a distinguir "error mostrable" de "error
    // fatal" en cada llamada; con un mensaje, la excepcion se arma en un
    // solo lugar (el que agota los intentos).
    using Validador = std::function<std::string(const std::string&)>;

    // El valor inicial es solo el texto con el que arranca el campo (un
    // valor por defecto de la pantalla). NO esta confirmado: para obtener el
    // dato, el usuario tiene que apretar Enter.
    Campo(std::string etiqueta, std::string valorInicial, Validador validador);

    // Aplica un evento: caracteres, espacio, Retroceso, flechas izquierda y
    // derecha (el cursor) y Enter (que valida). Devuelve true si el campo
    // consumio el evento.
    bool aplicarEvento(const Evento& evento);

    const std::string& etiqueta() const;

    // El texto acumulado.
    const std::string& texto() const;

    // El ultimo error. Queda vacio si el ultimo intento fue valido.
    //
    // El error NO se borra al editar el texto: describe el ultimo intento, y
    // el usuario tiene que ver por que le reboto hasta que apriete Enter otra
    // vez.
    const std::string& error() const;

    // Cuantos intentos invalidos lleva.
    int intentos() const;

    // Si el ultimo Enter resulto valido.
    bool confirmado() const;

    // El texto validado.
    //
    // Es el texto TAL COMO esta en el campo (el mismo que paso la
    // validacion): decide el validador que hacer con los espacios, no el
    // campo.
    //
    // Lanza `DatoInvalidoException` con `campo() == "entrada"` si el texto no
    // pasa la validacion o si el campo nunca fue confirmado. Es la operacion
    // que la pantalla llama cuando el usuario confirma.
    const std::string& valor() const;

    // El texto tal como se ve en pantalla, con el cursor.
    //
    // El cursor se marca con una barra vertical en la posicion en la que se
    // esta escribiendo: el texto que se ve TIENE que mostrar donde va el
    // proximo caracter.
    std::string textoEnPantalla() const;

private:
    void insertar(const std::string& caracteres);
    void borrarAnterior();
    void moverCursorIzquierda();
    void moverCursorDerecha();
    void validar();

    std::string _etiqueta;
    std::string _texto;
    std::string _error;

    // La posicion del cursor: un indice de BYTES que siempre cae en un limite
    // de codepoint (nunca en el medio de un caracter UTF-8). Ver los helpers
    // de `src/ui/Campo.cpp`.
    std::size_t _cursor = 0;

    int _intentos = 0;
    bool _confirmado = false;
    Validador _validador;
};

} // namespace ui
