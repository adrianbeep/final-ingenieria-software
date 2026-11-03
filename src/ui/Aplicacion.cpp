#include "ui/Aplicacion.h"

#include "ui/Terminal.h"

#include <iostream>
#include <utility>

namespace ui {

Aplicacion::Aplicacion(std::string titulo) : _titulo(std::move(titulo)) {
}

void Aplicacion::ir(Fabrica fabrica) {
    if (!fabrica) {
        return;
    }
    _pila.push_back(fabrica(*this));
}

void Aplicacion::volver() {
    if (_pila.size() <= 1) {
        // No hay a donde volver: se termina el ciclo. Es el caso de `Escape`
        // en la pantalla de arriba del todo.
        salir();
        return;
    }
    _pila.pop_back();
}

void Aplicacion::salir() {
    _salir = true;
    // `App::Active()` es la App que esta corriendo el ciclo de eventos. Si
    // todavia no arranco (o ya termino), no hay nada que cortar.
    if (ftxui::App* app = ftxui::App::Active()) {
        app->Exit();
    }
}

void Aplicacion::ejecutar(std::function<std::string()> respaldoSinTerminal) {
    // EL GUARD DE TERMINAL (decisiones D16 y D31)
    //
    // Sin terminal interactiva NO se entra al alternate screen, NO se corre
    // el ciclo de eventos y se sale con codigo 0.
    //
    // Es lo que evita que el CI se cuelgue: ahi la salida es un pipe, y
    // `App::Loop()` sin TTY no retorna nunca.
    if (!Terminal::hayTerminal()) {
        const std::string texto =
            respaldoSinTerminal ? respaldoSinTerminal() : textoPlanoActual();
        std::cout << texto << std::endl;
        return;
    }

    Terminal::ejecutarCiclo(
        [this]() { return renderizar(); },
        [this](ftxui::Event evento) { return manejarEvento(evento); });
}

std::string Aplicacion::textoPlanoActual() const {
    if (_pila.empty()) {
        return _titulo;
    }
    return _pila.back()->textoPlano();
}

const std::string& Aplicacion::titulo() const {
    return _titulo;
}

ftxui::Element Aplicacion::renderizar() const {
    if (_pila.empty()) {
        return ftxui::vbox({
            ftxui::text(_titulo),
            ftxui::separator(),
            ftxui::text("No hay ninguna pantalla en la pila."),
        });
    }
    return _pila.back()->renderizar();
}

bool Aplicacion::manejarEvento(ftxui::Event evento) {
    // Ya se pidio salir: no se atiende mas nada. El ciclo se esta cerrando
    // y las pantallas que queden en la pila no tienen que reaccionar.
    if (_salir) {
        return false;
    }

    const Evento propio = eventoDesde(evento);

    // `Escape` lo maneja la APLICACION: es la clave de "volver" en todos los
    // niveles, asi que se atiende antes que la pantalla. Si no hay una
    // pantalla anterior, `volver()` termina el ciclo.
    if (propio.tipo == TipoTecla::Escape) {
        volver();
        return true;
    }

    if (_pila.empty()) {
        return false;
    }
    return _pila.back()->manejar(propio);
}

} // namespace ui
