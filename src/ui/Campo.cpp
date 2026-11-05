#include "ui/Campo.h"

#include "exceptions/ExcepcionesSistema.h"
#include "ui/Consola.h"

#include <cstddef>
#include <string>
#include <utility>

namespace ui {
namespace {

// Un byte de CONTINUACION de un codepoint UTF-8 tiene los dos bits altos en
// 1 (0b10xxxxxx). Un byte que no es de continuacion es el PRIMER byte de un
// codepoint (o un byte suelto, si el texto esta roto).
bool esByteDeContinuacion(char byte) {
    return (static_cast<unsigned char>(byte) & 0xC0) == 0x80;
}

// El indice del PRIMER byte del codepoint que esta antes del cursor.
//
// Si el cursor esta al principio del texto, devuelve 0 (no hay nada que
// borrar).
std::size_t inicioAnterior(const std::string& texto, std::size_t cursor) {
    if (cursor == 0) {
        return 0;
    }
    std::size_t inicio = cursor - 1;
    while (inicio > 0 && esByteDeContinuacion(texto[inicio])) {
        --inicio;
    }
    return inicio;
}

// El indice del byte DESPUES del codepoint que empieza en `inicio`.
std::size_t finSiguiente(const std::string& texto, std::size_t inicio) {
    std::size_t fin = inicio + 1;
    while (fin < texto.size() && esByteDeContinuacion(texto[fin])) {
        ++fin;
    }
    return fin;
}

} // namespace

Campo::Campo(std::string etiqueta, std::string valorInicial, Validador validador)
    : _etiqueta(std::move(etiqueta)),
      _texto(std::move(valorInicial)),
      _cursor(_texto.size()),
      _validador(std::move(validador)) {
}

bool Campo::aplicarEvento(const Evento& evento) {
    // EL RATON NO TOCA EL TEXTO
    //
    // Un clic no inserta ni borra nada. Se devuelve false para que lo
    // atienda la pantalla y el clic no se quede sin destino.
    if (evento.esMouse()) {
        return false;
    }

    switch (evento.tipo) {
        case TipoTecla::Caracter:
            if (evento.caracter.empty()) {
                return false;
            }
            insertar(evento.caracter);
            return true;

        case TipoTecla::Espacio:
            // El espacio llega como tecla propia (no como caracter): si no se
            // atendiera aca, no se podrian escribir espacios.
            insertar(" ");
            return true;

        case TipoTecla::Retroceso:
            borrarAnterior();
            return true;

        case TipoTecla::FlechaIzquierda:
            moverCursorIzquierda();
            return true;

        case TipoTecla::FlechaDerecha:
            moverCursorDerecha();
            return true;

        case TipoTecla::Enter:
            validar();
            return true;

        default:
            break;
    }

    return false;
}

const std::string& Campo::etiqueta() const {
    return _etiqueta;
}

const std::string& Campo::texto() const {
    return _texto;
}

const std::string& Campo::error() const {
    return _error;
}

int Campo::intentos() const {
    return _intentos;
}

bool Campo::confirmado() const {
    return _confirmado;
}

const std::string& Campo::valor() const {
    if (!_confirmado) {
        throw DatoInvalidoException("entrada", _texto, "el campo no fue confirmado");
    }

    // Se revalida antes de devolver: el texto que sale de aca es el que PASO
    // la validacion. Es una red de contencion para un validador que no sea
    // puro (o para un campo al que se le toco el texto por la puerta de
    // atras).
    const std::string mensaje = _validador ? _validador(_texto) : std::string();
    if (!mensaje.empty()) {
        throw DatoInvalidoException("entrada", _texto, mensaje);
    }

    return _texto;
}

std::string Campo::textoEnPantalla() const {
    std::string salida = _texto;
    salida.insert(_cursor, "|");
    return salida;
}

void Campo::insertar(const std::string& caracteres) {
    _texto.insert(_cursor, caracteres);
    _cursor += caracteres.size();

    // EDITAR INVALIDA LA CONFIRMACION
    //
    // El valor confirmado era el texto de antes: si el usuario lo cambia, el
    // valor confirmado ya no es el que esta en el campo. Por eso `valor()`
    // vuelve a pedir la confirmacion.
    _confirmado = false;
}

void Campo::borrarAnterior() {
    if (_cursor == 0) {
        return;
    }
    const std::size_t inicio = inicioAnterior(_texto, _cursor);
    _texto.erase(inicio, _cursor - inicio);
    _cursor = inicio;
    _confirmado = false;
}

void Campo::moverCursorIzquierda() {
    if (_cursor == 0) {
        return;
    }
    _cursor = inicioAnterior(_texto, _cursor);
}

void Campo::moverCursorDerecha() {
    if (_cursor >= _texto.size()) {
        return;
    }
    _cursor = finSiguiente(_texto, _cursor);
}

void Campo::validar() {
    const std::string mensaje = _validador ? _validador(_texto) : std::string();

    if (mensaje.empty()) {
        _error.clear();
        _confirmado = true;
        return;
    }

    ++_intentos;
    _error = mensaje;
    _confirmado = false;

    // SE AGOTARON LOS INTENTOS
    //
    // Es el final de la linea: la excepcion es del dominio y la pantalla la
    // atrapa para volver al menu (en la Fase 1 el `catch` global mataba el
    // programa).
    if (_intentos >= Consola::MAX_INTENTOS) {
        throw DatoInvalidoException("entrada", _texto, mensaje);
    }
}

} // namespace ui
