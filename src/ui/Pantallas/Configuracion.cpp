#include "ui/Pantallas/Configuracion.h"

#include "ui/CargarEjemplo.h"
#include "ui/ColorFtxui.h"
#include "ui/Contexto.h"

#include <ftxui.hpp>

#include <cstddef>
#include <string>
#include <utility>

// ---------------------------------------------------------------------------
// La pantalla de CONFIGURACION.
//
// Lo que esta en el header es el POR QUE; lo que esta aca es el COMO.
//
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {

PantallaConfiguracion::PantallaConfiguracion(Aplicacion& aplicacion)
    : _aplicacion(aplicacion), _titulo("CONFIGURACION"), _menu("Configuracion") {
    // EL MENU DE LA PANTALLA
    //
    // El atajo es el numero que se escribe (decision D28) y el '0' es
    // "Volver" en todos los niveles.
    _menu.agregar('1', "Activar / desactivar colores", [this]() { alternarColores(); });
    _menu.agregar('2', "Cargar datos de ejemplo", [this]() { pedirCargaDeEjemplo(); });
    _menu.agregar('0', "Volver", [this]() { volverAtras(); });

    // El menu se dibuja a partir de la fila FILA_INICIAL: el hit-testing del
    // raton lo necesita.
    _menu.definirFilaInicial(2);
}

std::string PantallaConfiguracion::titulo() const {
    return _titulo;
}

// ---------------------------------------------------------------------------
// La navegacion
// ---------------------------------------------------------------------------

void PantallaConfiguracion::volverAtras() {
    // EL "VOLVER" SUBE UN NIVEL
    //
    //   - desde el menu de la pantalla, se sale de la pantalla (el menu
    //     principal esta en la pila de la aplicacion);
    //   - desde la confirmacion, se vuelve al menu de la pantalla.
    switch (_vista) {
        case Vista::Menu:
            _aplicacion.volver();
            return;
        case Vista::Confirmar:
            irAMenu();
            return;
    }
}

void PantallaConfiguracion::irAMenu() {
    _vista = Vista::Menu;
    _confirmacion.reset();
}

bool PantallaConfiguracion::esTeclaVolver(const Evento& evento) const {
    return evento.tipo == TipoTecla::Caracter && evento.caracter == "0";
}

bool PantallaConfiguracion::esTeclaEscape(const Evento& evento) const {
    return evento.tipo == TipoTecla::Escape;
}

// ---------------------------------------------------------------------------
// Los manejadores de cada vista
// ---------------------------------------------------------------------------

bool PantallaConfiguracion::manejar(const Evento& evento) {
    switch (_vista) {
        case Vista::Menu:
            return manejarMenu(evento);
        case Vista::Confirmar:
            return manejarConfirmacion(evento);
    }
    return false;
}

bool PantallaConfiguracion::manejarMenu(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` NO SE CONSUME EN EL MENU
    //
    // El menu de la pantalla es el NIVEL MAS ALTO de esta pantalla: desde
    // ahi el `Escape` tiene que sacar la pantalla ENTERA de la pila, y eso
    // lo hace la aplicacion.
    return _menu.aplicarEvento(evento);
}

bool PantallaConfiguracion::manejarConfirmacion(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE AL MENU DE LA PANTALLA
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    if (!_confirmacion) {
        return false;
    }

    // EL RATON LO RESUELVE LA CONFIRMACION
    //
    // La `ui::Confirmacion` tiene su propio hit-testing: la pantalla le dijo
    // en que fila se dibujan sus opciones (`definirFilaInicial`) y ella sabe
    // que opcion se toco. Por eso el clic NO se traduce a mano aca.
    if (!_confirmacion->aplicarEvento(evento)) {
        return false;
    }

    if (_confirmacion->confirmo()) {
        cargarEjemplo();
    } else {
        // Si NO confirma, se vuelve al menu sin hacer nada.
        irAMenu();
    }
    return true;
}

// ---------------------------------------------------------------------------
// Las acciones
// ---------------------------------------------------------------------------

void PantallaConfiguracion::alternarColores() {
    // EL TOGGLE ES DEL ESTADO COMPARTIDO
    //
    // No es un bool de esta pantalla: es el estado que comparten TODAS las
    // pantallas (ver `ui::Theme::establecerColoresActivos`). Por eso el
    // efecto es inmediato en las que ya estan abiertas.
    const bool nuevoValor = !_tema.coloresActivos();
    Theme::establecerColoresActivos(nuevoValor);

    if (nuevoValor) {
        avisarExito("Colores activados.");
    } else {
        avisarExito("Colores desactivados.");
    }
}

void PantallaConfiguracion::pedirCargaDeEjemplo() {
    // LA CONFIRMACION DE DOS PASOS
    //
    // La carga pisa los datos de trabajo: no se puede deshacer, asi que se
    // pregunta antes. Los atajos son '1' (si) y '2' (no), no '0': una
    // confirmacion no es "volver", es una decision (decision D37).
    _confirmacion = std::make_unique<Confirmacion>(
        "Cargar los datos de ejemplo? Se pisan los datos actuales.");
    // La confirmacion se dibuja en unas filas conocidas: se lo dice para que
    // su hit-testing del raton funcione.
    _confirmacion->definirFilaInicial(FILA_INICIAL_CONFIRMACION);
    _vista = Vista::Confirmar;
}

void PantallaConfiguracion::cargarEjemplo() {
    std::string error;
    if (cargarDatosDeEjemplo(_aplicacion.contexto(), error)) {
        avisarExito("Datos de ejemplo cargados.");
    } else {
        avisar(error);
    }
    irAMenu();
}

// ---------------------------------------------------------------------------
// Los mensajes
// ---------------------------------------------------------------------------

void PantallaConfiguracion::avisar(const std::string& texto) {
    // El mensaje vive en el CONTEXTO: es lo que la pantalla anterior va a
    // mostrar cuando el usuario vuelva.
    _aplicacion.contexto().mostrarMensaje(Mensaje::error(texto));
}

void PantallaConfiguracion::avisarExito(const std::string& texto) {
    _aplicacion.contexto().mostrarMensaje(Mensaje::exito(texto));
}

const Mensaje& PantallaConfiguracion::mensajeActual() const {
    return _aplicacion.contexto().mensaje();
}

// ---------------------------------------------------------------------------
// El dibujo
// ---------------------------------------------------------------------------

ftxui::Element PantallaConfiguracion::encabezado() const {
    ftxui::Element elemento = ftxui::text(_aplicacion.titulo() + " - " + _titulo);
    if (const auto color = _tema.colorDe(Rol::Titulo)) {
        elemento = elemento | ftxui::color(aFtxui(*color)) | ftxui::bold;
    }
    return elemento;
}

ftxui::Element PantallaConfiguracion::lineaOpcion(int indice) const {
    const std::string atajo(1, _menu.atajoDe(indice));
    ftxui::Element elemento =
        ftxui::text("  " + atajo + ". " + _menu.etiquetas()[static_cast<std::size_t>(indice)]);
    if (indice == _menu.seleccion()) {
        // El resaltado de la opcion seleccionada. `inverted` funciona SIN
        // color, asi que el menu se ve igual con `NO_COLOR` definido.
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

ftxui::Element PantallaConfiguracion::lineaConfirmacion() const {
    ftxui::Elements lineas;
    if (_confirmacion) {
        lineas.push_back(ftxui::text("  " + _confirmacion->pregunta()));
        // LAS DOS OPCIONES EN DOS FILAS
        //
        // Una por fila, como en todos los otros menus: asi la pantalla sabe
        // en que fila esta cada una y puede traducir el clic del raton al
        // atajo que corresponde.
        lineas.push_back(ftxui::text("  1. Si"));
        lineas.push_back(ftxui::text("  2. No"));
    }
    return ftxui::vbox(std::move(lineas));
}

ftxui::Element PantallaConfiguracion::lineaMensaje() const {
    const Mensaje& mensaje = mensajeActual();
    ftxui::Element elemento = ftxui::text("  " + mensaje.texto());

    Rol rol = Rol::Subtitulo;
    if (mensaje.esExito()) {
        rol = Rol::Exito;
    } else if (mensaje.esError()) {
        rol = Rol::Error;
    }
    if (const auto color = _tema.colorDe(rol)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

std::string PantallaConfiguracion::ayuda() const {
    // LA AYUDA DICE COMO VOLVER
    //
    // Un mensaje que no dice como se vuelve deja al usuario atrapado.
    switch (_vista) {
        case Vista::Menu:
            return "Flechas: mover   Enter: elegir   0: volver   Esc: volver";
        case Vista::Confirmar:
            return "1: Si   2: No   0: volver   Esc: volver";
    }
    return "";
}

ftxui::Element PantallaConfiguracion::renderizar() {
    ftxui::Elements lineas;

    lineas.push_back(encabezado());
    lineas.push_back(ftxui::separator());

    switch (_vista) {
        case Vista::Menu:
            // La opcion `i` del menu queda en la fila `filaInicial + i`: el
            // hit-testing del raton lo necesita.
            _menu.definirFilaInicial(static_cast<int>(lineas.size()));
            for (int i = 0; i < _menu.cantidad(); ++i) {
                lineas.push_back(lineaOpcion(i));
            }
            lineas.push_back(ftxui::separator());
            break;
        case Vista::Confirmar:
            lineas.push_back(lineaConfirmacion());
            break;
    }

    if (!mensajeActual().esVacio()) {
        lineas.push_back(lineaMensaje());
    }
    lineas.push_back(ftxui::text(ayuda()));

    return ftxui::vbox(std::move(lineas));
}

// ---------------------------------------------------------------------------
// El texto plano (lo que se ve sin terminal)
// ---------------------------------------------------------------------------

const std::string& PantallaConfiguracion::nombreDeVista(Vista vista) {
    // Los nombres son ESTATICOS: son los mismos para todas las pantallas y
    // la referencia que se devuelve tiene que vivir mas que la llamada.
    static const std::string menu = "Menu";
    static const std::string confirmar = "Confirmar";

    switch (vista) {
        case Vista::Menu:
            return menu;
        case Vista::Confirmar:
            return confirmar;
    }
    return menu;
}

const std::string& PantallaConfiguracion::vista() const {
    return nombreDeVista(_vista);
}

std::string PantallaConfiguracion::textoDeLaVista() const {
    std::string salida;

    switch (_vista) {
        case Vista::Menu: {
            for (int i = 0; i < _menu.cantidad(); ++i) {
                salida += "  " + std::string(1, _menu.atajoDe(i)) + ". "
                    + _menu.etiquetas()[static_cast<std::size_t>(i)] + "\n";
            }
            return salida;
        }
        case Vista::Confirmar: {
            if (_confirmacion) {
                salida += "  " + _confirmacion->pregunta() + "\n";
                salida += "  1. Si\n";
                salida += "  2. No\n";
            }
            return salida;
        }
    }
    return salida;
}

std::string PantallaConfiguracion::vistaTextoPlano() const {
    // SOLO LA VISTA
    //
    // Es lo que se ve ahora, sin el encabezado ni la ayuda: es lo que
    // necesita el test para comprobar que la vista muestra lo que tiene que
    // mostrar.
    return textoDeLaVista();
}

std::string PantallaConfiguracion::textoPlano() const {
    std::string salida = _aplicacion.titulo() + " - " + _titulo + "\n";
    salida += textoDeLaVista();
    if (!mensajeActual().esVacio()) {
        salida += mensajeActual().texto() + "\n";
    }
    salida += ayuda();
    return salida;
}

std::unique_ptr<Pantalla> crearPantallaConfiguracion(Aplicacion& aplicacion) {
    return std::make_unique<PantallaConfiguracion>(aplicacion);
}

} // namespace Pantallas
} // namespace ui
