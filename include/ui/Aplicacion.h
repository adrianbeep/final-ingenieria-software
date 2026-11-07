#pragma once

#include "ui/Contexto.h"
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
// La pantalla tiene la PRIMERA palabra: el evento se le da primero a la
// pantalla de arriba de la pila, y solo si la pantalla NO lo consume la
// aplicacion atiende el `Escape` (que es la clave de "volver" en el nivel de
// la pila). Asi el motor CRUD puede usar el `Escape` para subir UN nivel
// dentro de sus vistas, y el `Escape` del menu CRUD saca la pantalla entera.
//
// El caracter '0' lo maneja cada pantalla (es el atajo de "Volver"), porque
// su significado depende de la pantalla.
// ---------------------------------------------------------------------------
namespace ui {

class Aplicacion {
public:
    // La fabrica recibe la aplicacion para poder navegar (ir, volver, salir).
    using Fabrica = std::function<std::unique_ptr<Pantalla>(Aplicacion&)>;

    // El CONTEXTO
    //
    // La aplicacion guarda el contexto (los repositorios y el servicio de
    // turnos) y lo pasa a las pantallas: es el unico lugar donde se juntan
    // los colaboradores, asi que las pantallas no los reciben por parametro
    // uno por uno.
    explicit Aplicacion(std::string titulo, Contexto& contexto);

    // El contexto de la aplicacion.
    Contexto& contexto() const;

    // Apila una pantalla nueva.
    void ir(Fabrica fabrica);

    // Saca la pantalla de arriba. Si era la unica, el ciclo termina.
    void volver();

    // Termina el ciclo de eventos.
    void salir();

    // Si el ciclo de eventos ya termino (o esta por terminar).
    //
    // Lo usan los tests: `salir()` no imprime nada ni corta el proceso, asi
    // que la unica forma de verificar que se disparo es preguntar.
    bool saliendo() const;

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

    // Traduce el evento y lo despacha: primero lo ve la pantalla de arriba y,
    // si no lo consumio, la aplicacion atiende el `Escape`.
    bool manejarEvento(ftxui::Event evento);

    std::string _titulo;
    Contexto& _contexto;
    std::vector<std::unique_ptr<Pantalla>> _pila;
    bool _salir = false;
};

} // namespace ui
