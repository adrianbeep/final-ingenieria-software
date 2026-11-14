#pragma once

#include "ui/Aplicacion.h"
#include "ui/Dibujo.h"
#include "ui/Menu.h"
#include "ui/Mensaje.h"
#include "ui/Pantalla.h"
#include "ui/Theme.h"

#include <ftxui.hpp>

#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// La pantalla del TEMA DE COLOR.
//
// QUE ES
//
// Es la pantalla que deja elegir el tema de la paleta: "Apagados" (el color
// por defecto de la terminal), "Oscuro" (para una terminal de fondo oscuro) o
// "Claro" (para una terminal de fondo claro).
//
// POR QUE ES UNA PANTALLA Y NO UN INTERRUPTOR
//
// Un interruptor ("activar / desactivar colores") no puede expresar tres
// estados: con el tema hay tres, y el usuario tiene que ver CUAL esta activo
// antes de elegir. Ademas, el tema se elige desde dos lugares (el menu
// principal y la pantalla de configuracion), asi que es una pantalla propia y
// no una linea de un menu.
//
// EL TEMA ES UN ESTADO COMPARTIDO
//
// La pantalla escribe el estado COMPARTIDO de la paleta (ver
// `ui::Theme::establecerTema`) y no guarda un valor propio: por eso el
// cambio se ve AL INSTANTE en todas las pantallas que ya estan abiertas.
//
// EL TEMA ACTIVO SE MARCA
//
// La opcion del tema activo es la que queda resaltada del menu, y la linea
// de abajo dice cual es. Con el tema "Apagados" la pantalla no tiene color
// propio, asi que el nombre del tema es la unica forma de saber que tema
// esta elegido.
//
// EL '0' ES VOLVER, EN TODOS LOS NIVELES
//
// Es la regla de UX del proyecto. Y el `Escape` tambien vuelve: en el menu de
// la pantalla NO se consume, ahi lo maneja la aplicacion, que saca la
// pantalla entera de la pila.
// ---------------------------------------------------------------------------
namespace ui {

namespace Pantallas {

class PantallaTema : public Pantalla {
public:
    explicit PantallaTema(Aplicacion& aplicacion);

    // De Pantalla:
    std::string titulo() const override;
    ftxui::Element renderizar() override;
    bool manejar(const Evento& evento) override;
    std::string textoPlano() const override;

    // Para los tests: que se ve.
    std::string vistaTextoPlano() const;

private:
    // --- La navegacion ---
    bool esTeclaVolver(const Evento& evento) const;
    bool esTeclaEscape(const Evento& evento) const;

    // --- Las acciones ---
    void aplicarTema(Tema tema);

    // --- Los mensajes ---
    void avisarExito(const std::string& texto);
    const Mensaje& mensajeActual() const;

    // --- El dibujo ---
    ftxui::Element encabezado() const;
    ftxui::Element lineaOpcion(int indice) const;
    ftxui::Element lineaTemaActivo() const;
    ftxui::Element lineaMensaje() const;
    std::string ayuda() const;

    // --- El texto plano (lo que se ve sin terminal) ---
    std::string textoDeLaVista() const;

    // EL NOMBRE DE UN TEMA
    //
    // Es lo que se ve: "Apagados", "Oscuro" o "Claro".
    static std::string nombreDe(Tema tema);

    // EL INDICE DE UN TEMA EN EL MENU
    //
    // El menu tiene los tres temas en el mismo orden que el enum, asi que el
    // indice es el del tema. Devuelve -1 si el tema no esta en el menu (no
    // deberia pasar: son los mismos tres).
    static int indiceDe(Tema tema);

    Aplicacion& _aplicacion;
    Theme _tema;
    std::string _titulo;
    Menu _menu;
};

// La pantalla del tema, lista para apilarse en la aplicacion.
std::unique_ptr<Pantalla> crearPantallaTema(Aplicacion& aplicacion);

} // namespace Pantallas
} // namespace ui
