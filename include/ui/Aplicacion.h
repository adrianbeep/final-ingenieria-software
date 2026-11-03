#pragma once

#include "ui/Pantalla.h"

#include <ftxui.hpp>

#include <functional>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// La aplicacion: la pila de pantallas y el ciclo de eventos.
//
// LA PILA
//
// `ir()` apila una pantalla nueva y `volver()` saca la de arriba. Todas las
// pantallas se dibujan con el mismo ciclo de eventos de FTXUI: no hay una
// pantalla por loop.
//
// LAS TECLAS DE NAVEGACION
//
// `Escape` lo maneja la APLICACION, no la pantalla: es la clave de "volver"
// en todos los niveles, asi que tiene que funcionar igual en todas las
// pantallas. El caracter '0' lo maneja cada pantalla (es el atajo de
// "Volver"), porque su significado depende de la pantalla.
// ---------------------------------------------------------------------------
namespace ui {

class Aplicacion {
public:
    // La fabrica recibe la aplicacion para poder navegar (ir, volver, salir).
    using Fabrica = std::function<std::unique_ptr<Pantalla>(Aplicacion&)>;

    explicit Aplicacion(std::string titulo);

    // Apila una pantalla nueva.
    void ir(Fabrica fabrica);

    // Saca la pantalla de arriba. Si era la unica, el ciclo termina.
    void volver();

    // Termina el ciclo de eventos.
    void salir();

    // Corre la aplicacion.
    //
    // `respaldoSinTerminal` es el texto que se imprime cuando NO hay
    // terminal interactiva (D31): en ese caso no se entra al alternate
    // screen, no se corre el ciclo de eventos, y se sale con codigo 0.
    void ejecutar(std::function<std::string()> respaldoSinTerminal);

    // El texto plano de la pantalla actual: es lo que se imprime cuando no
    // hay terminal interactiva.
    std::string textoPlanoActual() const;

    const std::string& titulo() const;

private:
    // El frame de la pantalla de arriba de la pila.
    ftxui::Element renderizar() const;

    // Traduce el evento y lo despacha: `Escape` lo maneja la aplicacion,
    // el resto lo maneja la pantalla.
    bool manejarEvento(ftxui::Event evento);

    std::string _titulo;
    std::vector<std::unique_ptr<Pantalla>> _pila;
    bool _salir = false;
};

} // namespace ui
