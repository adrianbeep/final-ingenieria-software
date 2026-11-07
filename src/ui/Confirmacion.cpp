#include "ui/Confirmacion.h"

#include <utility>

namespace ui {

Confirmacion::Confirmacion(std::string pregunta) : _menu(std::move(pregunta)) {
    // Las dos acciones SOLO se distinguen por el atajo que las dispara: el
    // menu ejecuta la de la opcion elegida.
    _menu.agregar('1', "Sí", [this]() {
        _respondida = true;
        _confirmo = true;
    });
    _menu.agregar('2', "No", [this]() {
        _respondida = true;
        _confirmo = false;
    });
}

bool Confirmacion::aplicarEvento(const Evento& evento) {
    // La confirmacion es el `Menu`: las flechas mueven la seleccion, Enter
    // elige la seleccionada y los atajos eligen directo.
    return _menu.aplicarEvento(evento);
}

bool Confirmacion::respondida() const {
    return _respondida;
}

bool Confirmacion::confirmo() const {
    return _confirmo;
}

void Confirmacion::definirFilaInicial(int fila) {
    // La fila la necesita el HIT-TESTING del raton: el menu interno sabe
    // en que fila de la pantalla esta cada opcion.
    _menu.definirFilaInicial(fila);
}

int Confirmacion::filaInicial() const {
    return _menu.filaInicial();
}

const std::string& Confirmacion::pregunta() const {
    return _menu.titulo();
}

std::string Confirmacion::textoPlano() const {
    return _menu.textoPlano();
}

} // namespace ui
