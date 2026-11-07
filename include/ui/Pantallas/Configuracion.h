#pragma once

#include "ui/Aplicacion.h"
#include "ui/Confirmacion.h"
#include "ui/Menu.h"
#include "ui/Mensaje.h"
#include "ui/Pantalla.h"
#include "ui/Theme.h"

#include <ftxui.hpp>

#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// La pantalla de CONFIGURACION.
//
// QUE ES
//
// Es la pantalla de las PREFERENCIAS del usuario: el color (se enciende o se
// apaga) y los datos (se carga el juego de ejemplo).
//
// QUE HAY EN ELLA (decision 7)
//
// Son DOS cosas y nada mas: los colores y los datos de ejemplo. El usuario no
// configura el dominio: el dominio son reglas de negocio, no preferencias.
//
// EL COLOR ES UN ESTADO COMPARTIDO
//
// El toggle no apaga el color de esta pantalla: apaga el color de TODAS las
// pantallas, y el efecto es inmediato. Por eso la pantalla escribe el estado
// compartido (`ui::Theme::establecerColoresActivos`) y no guarda un bool
// propio.
//
// LA CONFIRMACION ES DE DOS PASOS
//
// "Cargar datos de ejemplo" PISA los datos de trabajo: es una operacion que
// no se puede deshacer, asi que pide confirmacion antes de hacer nada (con
// '1' para confirmar y '2' para cancelar).
//
// EL '0' ES VOLVER, EN TODOS LOS NIVELES
//
// Es la regla de UX del proyecto. Y el `Escape` tambien vuelve: en el menu de
// la pantalla NO se consume, ahi lo maneja la aplicacion, que saca la
// pantalla entera de la pila.
//
// ---------------------------------------------------------------------------
namespace ui {

namespace Pantallas {

class PantallaConfiguracion : public Pantalla {
public:
    explicit PantallaConfiguracion(Aplicacion& aplicacion);

    // De Pantalla:
    std::string titulo() const override;
    ftxui::Element renderizar() override;
    bool manejar(const Evento& evento) override;
    std::string textoPlano() const override;

    // Para los tests: en que vista esta y que se ve.
    const std::string& vista() const;
    std::string vistaTextoPlano() const;

private:
    // LAS VISTAS DE LA MAQUINA DE ESTADOS
    //
    // El nombre de cada vista es el que ve el test (`vista()`): por eso son
    // palabras y no numeros.
    enum class Vista { Menu, Confirmar };

    // La fila de la pantalla donde se dibuja la PRIMERA opcion de la
    // confirmacion (la del "Si").
    //
    // La pantalla dibuja las dos opciones en dos filas conocidas y se lo
    // dice a la `ui::Confirmacion` (`definirFilaInicial`): sin esto, un clic
    // en el ENCABEZADO caeria en la primera opcion y confirmaria la carga
    // sin querer.
    static constexpr int FILA_INICIAL_CONFIRMACION = 3;

    static const std::string& nombreDeVista(Vista vista);

    // --- La navegacion ---
    void irAMenu();
    void volverAtras();
    bool esTeclaVolver(const Evento& evento) const;
    bool esTeclaEscape(const Evento& evento) const;

    // --- Los manejadores de cada vista ---
    bool manejarMenu(const Evento& evento);
    bool manejarConfirmacion(const Evento& evento);

    // --- Las acciones ---
    void alternarColores();
    void pedirCargaDeEjemplo();
    void cargarEjemplo();

    // --- Los mensajes ---
    void avisar(const std::string& texto);
    void avisarExito(const std::string& texto);
    const Mensaje& mensajeActual() const;

    // --- El dibujo ---
    ftxui::Element encabezado() const;
    ftxui::Element lineaOpcion(int indice) const;
    ftxui::Element lineaConfirmacion() const;
    ftxui::Element lineaMensaje() const;
    std::string ayuda() const;

    // --- El texto plano (lo que se ve sin terminal) ---
    std::string textoDeLaVista() const;

    Aplicacion& _aplicacion;
    Theme _tema;
    std::string _titulo;
    Menu _menu;

    Vista _vista = Vista::Menu;

    // La confirmacion de la carga de datos de ejemplo.
    std::unique_ptr<Confirmacion> _confirmacion;
};

// La pantalla de configuracion, lista para apilarse en la aplicacion.
std::unique_ptr<Pantalla> crearPantallaConfiguracion(Aplicacion& aplicacion);

} // namespace Pantallas
} // namespace ui
