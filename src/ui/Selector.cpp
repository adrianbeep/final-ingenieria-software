#include "ui/Selector.h"

#include <cctype>
#include <string>
#include <utility>

namespace ui {
namespace {

// EL NUMERO TIENE COMO MAXIMO DOS DIGITOS
//
// Con dos digitos se alcanzan las doce opciones del catalogo de
// especialidades, que es la lista mas larga del proyecto. Un tercer digito
// no podria ser una opcion valida de ninguna lista: se ignora.
const std::size_t MAXIMO_DIGITOS = 2;

// El texto que se pone al final de la linea de la opcion elegida, en el
// texto plano (donde no hay invertidas que la marquen).
const char* MARCA_DE_ELEGIDA = " (elegida)";

} // namespace

Selector::Selector(std::string titulo, std::vector<std::string> opciones)
    : _titulo(std::move(titulo)), _opciones(std::move(opciones)) {
}

bool Selector::moverArriba() {
    // La navegacion NO da la vuelta: en el extremo, la seleccion se queda.
    // El por que esta en `include/ui/Selector.h`.
    //
    // El numero escrito se borra SIEMPRE: el usuario eligio con las flechas,
    // y un numero a medias ya no dice nada.
    _numeroDigitado.clear();
    if (_seleccion <= 0) {
        return false;
    }
    --_seleccion;
    return true;
}

bool Selector::moverAbajo() {
    _numeroDigitado.clear();
    if (_seleccion >= static_cast<int>(_opciones.size()) - 1) {
        return false;
    }
    ++_seleccion;
    return true;
}

bool Selector::aplicarEvento(const Evento& evento) {
    // EL RATON NO LO MANEJA EL SELECTOR
    //
    // El hit-testing lo hace la pantalla, que es la que sabe en que fila
    // dibujo cada opcion (con `opcionEnFila`).
    if (evento.esMouse()) {
        return false;
    }

    switch (evento.tipo) {
        case TipoTecla::FlechaArriba:
            moverArriba();
            return true;

        case TipoTecla::FlechaAbajo:
            moverAbajo();
            return true;

        case TipoTecla::Caracter: {
            // Un DIGITO: el numero de la opcion que se quiere elegir.
            // Un caracter de mas de un byte (una letra con tilde, por
            // ejemplo) no puede ser un digito.
            if (evento.caracter.size() != 1) {
                return false;
            }
            const char digito = evento.caracter[0];
            if (!std::isdigit(static_cast<unsigned char>(digito))) {
                return false;
            }
            digitar(digito);
            return true;
        }

        case TipoTecla::Retroceso:
            borrarDigito();
            return true;

        case TipoTecla::Enter:
            confirmar();
            return true;

        default:
            // El `Escape` NO lo consume el selector: es una tecla de
            // navegacion y la decide la pantalla.
            break;
    }

    return false;
}

int Selector::seleccion() const {
    return _seleccion;
}

void Selector::seleccionar(int indice) {
    // Un indice fuera de rango SE IGNORA: la seleccion no cambia. El por que
    // esta en `include/ui/Selector.h`.
    if (indice < 0 || indice >= static_cast<int>(_opciones.size())) {
        return;
    }
    _seleccion = indice;
}

int Selector::cantidad() const {
    return static_cast<int>(_opciones.size());
}

const std::string& Selector::titulo() const {
    return _titulo;
}

const std::vector<std::string>& Selector::opciones() const {
    return _opciones;
}

const std::string& Selector::error() const {
    return _error;
}

const std::string& Selector::numeroDigitado() const {
    return _numeroDigitado;
}

int Selector::opcionEnFila(int y, int filaInicial) const {
    const int indice = y - filaInicial;
    if (indice < 0 || indice >= static_cast<int>(_opciones.size())) {
        return -1;
    }
    return indice;
}

void Selector::definirFilaInicial(int fila) {
    _filaInicial = fila;
}

int Selector::filaInicial() const {
    return _filaInicial;
}

std::string Selector::lineaDe(int indice, bool elegida) const {
    if (indice < 0 || indice >= static_cast<int>(_opciones.size())) {
        return "";
    }
    std::string linea = "  " + std::to_string(indice + 1) + ". "
        + _opciones[static_cast<std::size_t>(indice)];
    if (elegida) {
        linea += MARCA_DE_ELEGIDA;
    }
    return linea;
}

std::string Selector::textoPlano() const {
    std::string salida = _titulo;
    for (std::size_t i = 0; i < _opciones.size(); ++i) {
        salida += "\n";
        salida += lineaDe(static_cast<int>(i), static_cast<int>(i) == _seleccion);
    }
    return salida;
}

void Selector::digitar(char digito) {
    // Un tercer digito se IGNORA: con dos digitos ya se alcanzan todas las
    // opciones del catalogo.
    if (_numeroDigitado.size() >= MAXIMO_DIGITOS) {
        return;
    }
    _numeroDigitado.push_back(digito);
}

void Selector::borrarDigito() {
    if (_numeroDigitado.empty()) {
        return;
    }
    _numeroDigitado.pop_back();
}

void Selector::confirmar() {
    // SIN NUMERO ESCRITO: SE ACEPTA LA OPCION RESALTADA
    //
    // Es lo que pasa con las flechas: el usuario llega a la opcion y
    // aprieta Enter.
    if (_numeroDigitado.empty()) {
        _error.clear();
        return;
    }

    const int numero = numeroDe(_numeroDigitado);
    if (numero < 1 || numero > cantidad()) {
        // EL NUMERO NO EXISTE
        //
        // El error se muestra y el numero SE QUEDA: el usuario lo corrige
        // (con el Retroceso o escribiendo encima). No se borra lo que
        // escribio: seria tirar su trabajo por un error de un digito.
        _error = "opcion invalida: el numero tiene que estar entre 1 y "
            + std::to_string(cantidad());
        return;
    }

    _seleccion = numero - 1;
    _numeroDigitado.clear();
    _error.clear();
}

int Selector::numeroDe(const std::string& texto) const {
    // LA CONVERSION NO LANZA
    //
    // `std::stoi` lanza `std::out_of_range` con un numero enorme: el texto
    // se convierte a mano, digito por digito, y el error no es posible.
    int numero = 0;
    for (const char caracter : texto) {
        if (!std::isdigit(static_cast<unsigned char>(caracter))) {
            return -1;
        }
        numero = numero * 10 + (caracter - '0');
    }
    return numero;
}

} // namespace ui
