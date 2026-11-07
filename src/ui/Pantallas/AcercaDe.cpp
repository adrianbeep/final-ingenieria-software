#include "ui/Pantallas/AcercaDe.h"

#include "ui/ColorFtxui.h"

#include <ftxui.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// La pantalla de ACERCA DE.
//
// Lo que esta en el header es el POR QUE; lo que esta aca es el COMO.
//
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {
namespace {

// LA VERSION DE FTXUI
//
// La version vendorizada es la **v7.0.3** (ver
// `third_party/ftxui/PROVENANCE.md`, que es donde esta la fuente de verdad
// del dato: con su SHA256 y la fecha de descarga).
constexpr const char* VERSION_DE_FTXUI = "v7.0.3";

// LA LICENCIA DE FTXUI
//
// Es la **MIT** (ver `third_party/ftxui/LICENSE`, que es el texto de la
// licencia del release).
constexpr const char* LICENCIA_DE_FTXUI = "MIT";

} // namespace

PantallaAcercaDe::PantallaAcercaDe(Aplicacion& aplicacion)
    : _aplicacion(aplicacion), _titulo("ACERCA DE") {
}

std::string PantallaAcercaDe::titulo() const {
    return _titulo;
}

// ---------------------------------------------------------------------------
// Los datos
// ---------------------------------------------------------------------------

const std::vector<std::pair<std::string, std::string>>& PantallaAcercaDe::datosDelTrabajo() {
    // Son datos FIJOS: son los del trabajo Practico, no cambian con la
    // aplicacion. Por eso son estaticos (y la referencia que se devuelve
    // tiene que vivir mas que la llamada).
    static const std::vector<std::pair<std::string, std::string>> datos = {
        {"Sistema", "Sistema de Turnos Medicos"},
        {"Carrera", "Analisis de Sistemas, segundo anio"},
        {"Trabajo Practico", "Trabajo Practico Final de Ingenieria de Software"},
        {"Autor", "Adrian Joaquin Naum"}
    };
    return datos;
}

const std::vector<std::string>& PantallaAcercaDe::cabecerasDeAtajos() {
    static const std::vector<std::string> cabeceras = {"Atajo", "Que hace"};
    return cabeceras;
}

const std::vector<std::vector<std::string>>& PantallaAcercaDe::filasDeAtajos() {
    // LOS ATAJOS DE LA APLICACION
    //
    // Son los de TODO el programa (la tabla es de la aplicacion, no de una
    // pantalla): el teclado es la via principal y el mouse es un plus.
    static const std::vector<std::vector<std::string>> filas = {
        {"Flechas arriba / abajo", "Mueven la seleccion"},
        {"Enter", "Elige la opcion"},
        {"0", "Vuelve"},
        {"Esc", "Vuelve"},
        {"Tab", "Pasa al siguiente campo"},
        {"Retroceso", "Borra el caracter anterior"},
        {"1 a 9", "Elige la opcion cuyo atajo es ese numero"},
        {"Clic izquierdo", "Elige la opcion"},
        {"Rueda del raton", "Mueve la seleccion"}
    };
    return filas;
}

// ---------------------------------------------------------------------------
// La navegacion
// ---------------------------------------------------------------------------

bool PantallaAcercaDe::esTeclaVolver(const Evento& evento) const {
    return evento.tipo == TipoTecla::Caracter && evento.caracter == "0";
}

bool PantallaAcercaDe::esTeclaEscape(const Evento& evento) const {
    return evento.tipo == TipoTecla::Escape;
}

bool PantallaAcercaDe::manejar(const Evento& evento) {
    // EL '0' Y EL `Escape` SACAN LA PANTALLA DE LA PILA
    //
    // El `Escape` se consume aca (a diferencia de los menus de los modulos):
    // esta pantalla no tiene vistas internas, asi que no hay ningun nivel
    // interno al que el `Escape` pueda subir. Sin este consumo, el `Escape`
    // lo maneja la aplicacion (que saca la pantalla de la pila) y el
    // resultado seria el mismo, pero repartido en dos lugares.
    if (esTeclaVolver(evento) || esTeclaEscape(evento)) {
        _aplicacion.volver();
        return true;
    }

    // Es una pantalla de SOLO LECTURA: no consume nada mas.
    return false;
}

// ---------------------------------------------------------------------------
// El dibujo
// ---------------------------------------------------------------------------

ftxui::Element PantallaAcercaDe::encabezado() const {
    ftxui::Element elemento = ftxui::text(_aplicacion.titulo() + " - " + _titulo);
    if (const auto color = _tema.colorDe(Rol::Titulo)) {
        elemento = elemento | ftxui::color(aFtxui(*color)) | ftxui::bold;
    }
    return elemento;
}

ftxui::Element PantallaAcercaDe::lineaDato(const std::string& etiqueta,
                                            const std::string& valor) const {
    ftxui::Element elemento = ftxui::text("  " + etiqueta + ": " + valor);
    if (const auto color = _tema.colorDe(Rol::Subtitulo)) {
        // El TEXTO del dato va con el color de subtitulo: el encabezado de la
        // tabla es el que dice que es, asi que el valor no compite con el.
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

ftxui::Element PantallaAcercaDe::tablaDeAtajos() const {
    Tabla tabla(cabecerasDeAtajos());
    for (const std::vector<std::string>& fila : filasDeAtajos()) {
        tabla.agregarFila(fila);
    }
    return tabla.renderizar();
}

std::string PantallaAcercaDe::ayuda() const {
    // LA AYUDA DICE COMO VOLVER
    //
    // Un mensaje que no dice como se vuelve deja al usuario atrapado.
    return "0: volver   Esc: volver";
}

ftxui::Element PantallaAcercaDe::renderizar() {
    ftxui::Elements lineas;

    lineas.push_back(encabezado());
    lineas.push_back(ftxui::separator());

    // LOS DATOS DEL TRABAJO PRACTICO
    for (const std::pair<std::string, std::string>& dato : datosDelTrabajo()) {
        lineas.push_back(lineaDato(dato.first, dato.second));
    }

    lineas.push_back(ftxui::separator());
    lineas.push_back(ftxui::text("  Atajos de teclado y de mouse:"));
    lineas.push_back(tablaDeAtajos());

    lineas.push_back(ftxui::separator());
    lineas.push_back(ftxui::text("  FTXUI " + std::string(VERSION_DE_FTXUI)
                                 + " (licencia " + LICENCIA_DE_FTXUI + ")"));
    lineas.push_back(ftxui::text("  Codigo de terceros vendorizado en third_party/ftxui/"));
    lineas.push_back(ftxui::text("  Copyright (c) 2019 Arthur Sonzonni"));

    lineas.push_back(ftxui::separator());
    lineas.push_back(ftxui::text(ayuda()));

    return ftxui::vbox(std::move(lineas));
}

// ---------------------------------------------------------------------------
// El texto plano (lo que se ve sin terminal)
// ---------------------------------------------------------------------------

std::string PantallaAcercaDe::textoDeLaVista() const {
    std::string salida;

    for (const std::pair<std::string, std::string>& dato : datosDelTrabajo()) {
        salida += "  " + dato.first + ": " + dato.second + "\n";
    }

    salida += "\n  Atajos de teclado y de mouse:\n";
    salida += "  " + cabecerasDeAtajos()[0] + " | " + cabecerasDeAtajos()[1] + "\n";
    for (const std::vector<std::string>& fila : filasDeAtajos()) {
        salida += "  " + fila[0] + " | " + fila[1] + "\n";
    }

    salida += "\n  FTXUI " + std::string(VERSION_DE_FTXUI) + " (licencia "
        + LICENCIA_DE_FTXUI + ")\n";
    salida += "  Codigo de terceros vendorizado en third_party/ftxui/\n";
    salida += "  Copyright (c) 2019 Arthur Sonzonni\n";

    return salida;
}

std::string PantallaAcercaDe::vistaTextoPlano() const {
    return textoDeLaVista();
}

std::string PantallaAcercaDe::textoPlano() const {
    std::string salida = _aplicacion.titulo() + " - " + _titulo + "\n";
    salida += textoDeLaVista();
    salida += ayuda() + "\n";
    return salida;
}

std::unique_ptr<Pantalla> crearPantallaAcercaDe(Aplicacion& aplicacion) {
    return std::make_unique<PantallaAcercaDe>(aplicacion);
}

} // namespace Pantallas
} // namespace ui
