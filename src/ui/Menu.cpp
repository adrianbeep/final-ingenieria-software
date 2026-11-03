#include "ui/Menu.h"

#include "exceptions/ExcepcionesSistema.h"

#include <cctype>
#include <cstddef>
#include <stdexcept>
#include <utility>

namespace ui {

Menu::Menu(std::string titulo) : _titulo(std::move(titulo)) {
}

void Menu::agregar(char atajo, const std::string& etiqueta,
                   std::function<void()> accion) {
    // Un atajo tiene que ser un DIGITO: es el numero que el usuario escribe.
    // Un atajo que no es un digito no se podria escribir nunca, asi que es
    // un error de programacion.
    if (!std::isdigit(static_cast<unsigned char>(atajo))) {
        throw DatoInvalidoException("atajo", std::string(1, atajo),
                                    "el atajo de una opcion tiene que ser un digito");
    }

    // Los atajos son UNICOS: son la forma de elegir una opcion escribiendo un
    // numero, asi que dos opciones con el mismo atajo serian ambiguas.
    if (indiceDeAtajo(atajo) >= 0) {
        throw DatoInvalidoException("atajo", std::string(1, atajo),
                                    "el atajo ya esta en uso por otra opcion");
    }

    _opciones.push_back(Opcion{atajo, std::move(accion)});
    _etiquetas.push_back(etiqueta);
}

bool Menu::moverArriba() {
    if (_opciones.empty()) {
        return false;
    }
    // La navegacion NO da la vuelta: en el extremo, la seleccion se queda.
    // El por que esta en `include/ui/Menu.h`.
    if (_seleccion <= 0) {
        return false;
    }
    --_seleccion;
    return true;
}

bool Menu::moverAbajo() {
    if (_opciones.empty()) {
        return false;
    }
    if (_seleccion >= static_cast<int>(_opciones.size()) - 1) {
        return false;
    }
    ++_seleccion;
    return true;
}

bool Menu::aplicarEvento(const Evento& evento) {
    // EL RATON
    //
    // Se atiende PRIMERO, antes que las teclas: un evento de raton no trae
    // tipo de tecla (el `Evento` guarda el boton y las coordenadas), asi que
    // el `switch` de abajo lo tomaria por un caracter vacio.
    //
    // Solo el clic izquierdo apretado elige una opcion. La rueda y el boton
    // derecho no eligen nada (el menu no tiene scroll que hacer), asi que
    // no los consume.
    if (evento.esMouse()) {
        if (!evento.esClic()) {
            return false;
        }
        const int indice = opcionEnFila(evento.y, _filaInicial);
        if (indice < 0) {
            // El clic cae fuera de las opciones: no hay nada que ejecutar.
            return false;
        }
        ejecutar(indice);
        return true;
    }

    switch (evento.tipo) {
        case TipoTecla::FlechaArriba:
            moverArriba();
            return true;

        case TipoTecla::FlechaAbajo:
            moverAbajo();
            return true;

        case TipoTecla::Enter:
            ejecutar(_seleccion);
            return true;

        case TipoTecla::Caracter: {
            // Un atajo es UN digito. Un caracter de mas de un byte (una letra
            // con tilde, por ejemplo) no puede ser un atajo.
            if (evento.caracter.size() != 1) {
                return false;
            }
            const char digito = evento.caracter[0];
            if (!std::isdigit(static_cast<unsigned char>(digito))) {
                return false;
            }
            const int indice = indiceDeAtajo(digito);
            if (indice < 0) {
                // Es un digito, pero no es atajo de ninguna opcion: el menu
                // no lo consume y lo puede atender la pantalla.
                return false;
            }
            ejecutar(indice);
            return true;
        }

        default:
            break;
    }

    return false;
}

int Menu::opcionEnFila(int y, int filaInicial) const {
    const int indice = y - filaInicial;
    if (indice < 0 || indice >= static_cast<int>(_opciones.size())) {
        return -1;
    }
    return indice;
}

void Menu::definirFilaInicial(int fila) {
    _filaInicial = fila;
}

int Menu::filaInicial() const {
    return _filaInicial;
}

const std::string& Menu::titulo() const {
    return _titulo;
}

const std::vector<std::string>& Menu::etiquetas() const {
    return _etiquetas;
}

char Menu::atajoDe(int indice) const {
    if (indice < 0 || indice >= static_cast<int>(_opciones.size())) {
        throw std::out_of_range("Menu::atajoDe: indice fuera de rango");
    }
    return _opciones[static_cast<std::size_t>(indice)].atajo;
}

int Menu::seleccion() const {
    return _seleccion;
}

void Menu::seleccionar(int indice) {
    if (indice < 0 || indice >= static_cast<int>(_opciones.size())) {
        throw std::out_of_range("Menu::seleccionar: indice fuera de rango");
    }
    _seleccion = indice;
}

int Menu::cantidad() const {
    return static_cast<int>(_opciones.size());
}

std::string Menu::textoPlano() const {
    std::string salida = _titulo;
    for (std::size_t i = 0; i < _opciones.size(); ++i) {
        salida += "\n  ";
        salida += _opciones[i].atajo;
        salida += ". ";
        salida += _etiquetas[i];
    }
    return salida;
}

int Menu::indiceDeAtajo(char atajo) const {
    for (std::size_t i = 0; i < _opciones.size(); ++i) {
        if (_opciones[i].atajo == atajo) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

void Menu::ejecutar(int indice) {
    if (indice < 0 || indice >= static_cast<int>(_opciones.size())) {
        return;
    }
    const std::function<void()>& accion = _opciones[static_cast<std::size_t>(indice)].accion;
    if (accion) {
        accion();
    }
}

} // namespace ui
