#include "ui/Pantallas/Demo.h"

#include "ui/Aplicacion.h"
#include "ui/ColorFtxui.h"
#include "ui/Terminal.h"

#include <ftxui.hpp>

#include <cstddef>
#include <string>
#include <utility>

namespace ui {
namespace Pantallas {
namespace {

// El nombre de cada rol de la paleta: es lo que se muestra al lado del
// bloque de color.
const char* nombreRol(Rol rol) {
    switch (rol) {
        case Rol::Titulo:
            return "titulo";
        case Rol::Subtitulo:
            return "subtitulo";
        case Rol::Exito:
            return "exito";
        case Rol::Error:
            return "error";
        case Rol::Advertencia:
            return "advertencia";
        case Rol::TablaHeader:
            return "tablaHeader";
        case Rol::TablaCelda:
            return "tablaCelda";
        case Rol::TablaBorde:
            return "tablaBorde";
    }
    return "";
}

// Los roles que se muestran en la linea de la paleta.
constexpr Rol ROLES[] = {
    Rol::Titulo,      Rol::Subtitulo, Rol::Exito,        Rol::Error,
    Rol::Advertencia, Rol::TablaHeader, Rol::TablaCelda, Rol::TablaBorde};

} // namespace

Demo::Demo(Aplicacion& aplicacion)
    : _aplicacion(aplicacion), _menu("Demo del motor TUI") {
    _menu.agregar('1', "Paleta de colores", [this]() { mostrarPaleta(); });
    _menu.agregar('2', "Tamano del terminal", [this]() { mostrarTamano(); });
    _menu.agregar('3', "Posicion del raton", [this]() { mostrarRaton(); });
    // El '0' es "Salir": el atajo de volver es el mismo en todos los niveles
    // (la regla de UX del proyecto).
    _menu.agregar('0', "Salir", [this]() { _aplicacion.salir(); });

    // El menu se dibuja a partir de la fila FILA_INICIAL: el hit-testing del
    // raton lo necesita para saber que opcion se toco.
    _menu.definirFilaInicial(FILA_INICIAL);
}

std::string Demo::titulo() const {
    return "Demo del motor";
}

ftxui::Element Demo::renderizar() {
    ftxui::Elements lineas;

    // Fila 0: el encabezado.
    lineas.push_back(encabezado());
    // Fila 1: el separador.
    lineas.push_back(ftxui::separator());

    // Filas 2 en adelante: las opciones del menu, UNA POR LINEA. El menu
    // dibuja una opcion por linea, asi que la opcion `i` queda en la fila
    // `FILA_INICIAL + i`, que es lo que usa el hit-testing del raton.
    for (int i = 0; i < _menu.cantidad(); ++i) {
        lineas.push_back(lineaOpcion(i));
    }

    lineas.push_back(ftxui::separator());
    lineas.push_back(lineaPaleta());
    lineas.push_back(ftxui::text(_informacion));
    lineas.push_back(ftxui::text("Flechas: mover   Enter: elegir   0: salir   Esc: salir"));

    return ftxui::vbox(std::move(lineas));
}

bool Demo::manejar(const Evento& evento) {
    // La posicion del raton se registra en CUALQUIER evento de mouse: el
    // boton y el movimiento llegan por separado, y lo que interesa es donde
    // esta el puntero.
    if (evento.esMouse()) {
        _ultimoX = evento.x;
        _ultimoY = evento.y;
    }

    // El menu se encarga del resto: las flechas, Enter, los atajos y el clic.
    return _menu.aplicarEvento(evento);
}

std::string Demo::textoPlano() const {
    return _menu.textoPlano();
}

void Demo::mostrarPaleta() {
    std::string nombres = "Paleta: ";
    for (const Rol rol : ROLES) {
        nombres += nombreRol(rol);
        nombres += " ";
    }
    _informacion = nombres;
}

void Demo::mostrarTamano() {
    _informacion = "Terminal: " + std::to_string(Terminal::ancho()) + " x " +
                    std::to_string(Terminal::alto()) + " caracteres";
}

void Demo::mostrarRaton() {
    if (_ultimoX < 0) {
        _informacion = "Raton: todavia no se movio (muevelo o hace clic)";
        return;
    }
    _informacion = "Raton: columna " + std::to_string(_ultimoX) + ", fila " +
                    std::to_string(_ultimoY);
}

ftxui::Element Demo::encabezado() const {
    ftxui::Element titulo = ftxui::text(_aplicacion.titulo() + " - " + this->titulo());
    if (const auto color = _tema.colorDe(Rol::Titulo)) {
        titulo = titulo | ftxui::color(aFtxui(*color)) | ftxui::bold;
    }
    return titulo;
}

ftxui::Element Demo::lineaOpcion(int indice) const {
    const std::string atajo(1, _menu.atajoDe(indice));
    std::string texto = "  " + atajo + ". " + _menu.etiquetas()[static_cast<std::size_t>(indice)];

    ftxui::Element elemento = ftxui::text(texto);
    if (indice == _menu.seleccion()) {
        // El resaltado de la opcion seleccionada. `inverted` (invertir los
        // colores de la celda) funciona SIN color, asi que el menu se ve
        // igual con `NO_COLOR` definido.
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

ftxui::Element Demo::lineaPaleta() const {
    ftxui::Elements bloques;
    for (const Rol rol : ROLES) {
        ftxui::Element bloque = ftxui::text(std::string(" ") + nombreRol(rol) + " ");
        // Con los colores apagados (`NO_COLOR`) el bloque sale en texto
        // plano: es exactamente lo que se quiere, porque el terminal no
        // tiene que recibir codigos de color si el usuario no los quiere.
        if (const auto color = _tema.colorDe(rol)) {
            bloque = bloque | ftxui::color(aFtxui(*color));
        }
        bloques.push_back(bloque);
    }
    return ftxui::hbox(std::move(bloques));
}

} // namespace Pantallas
} // namespace ui
