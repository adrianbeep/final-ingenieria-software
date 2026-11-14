#include "ui/Tabla.h"

#include "ui/ColorFtxui.h"

#include <ftxui.hpp>

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace ui {

Tabla::Tabla(std::vector<std::string> cabeceras) : _cabeceras(std::move(cabeceras)) {
}

void Tabla::agregarFila(std::vector<std::string> celdas) {
    Fila fila;
    fila.celdas = std::move(celdas);
    _filas.push_back(std::move(fila));
}

void Tabla::agregarFila(std::vector<std::string> celdas, const std::vector<Rol>& roles) {
    Fila fila;
    fila.celdas = std::move(celdas);
    fila.roles = roles;
    _filas.push_back(std::move(fila));
}

void Tabla::agregarFilaAtenuada(std::vector<std::string> celdas) {
    Fila fila;
    fila.celdas = std::move(celdas);
    fila.atenuada = true;
    _filas.push_back(std::move(fila));
}

void Tabla::agregarFila(const Fila& fila) {
    _filas.push_back(fila);
}

int Tabla::filas() const {
    return static_cast<int>(_filas.size());
}

ftxui::Element Tabla::renderizar() const {
    const std::vector<int> anchos = this->anchos();
    const Bordes& bordes = _tema.bordes();

    ftxui::Elements lineas;

    // El borde de arriba.
    lineas.push_back(lineaBorde(anchos, bordes.esquinaSuperiorIzquierda,
                                 bordes.esquinaSuperiorDerecha));

    // La fila de cabeceras.
    Fila cabecera;
    cabecera.celdas = _cabeceras;
    lineas.push_back(fila(cabecera, anchos, true));

    // El separador entre las cabeceras y los datos.
    lineas.push_back(lineaBorde(anchos, bordes.esquinaSuperiorIzquierda,
                                 bordes.esquinaSuperiorDerecha));

    // Las filas de datos.
    for (const Fila& datos : _filas) {
        lineas.push_back(fila(datos, anchos, false));
    }

    // El borde de abajo.
    lineas.push_back(lineaBorde(anchos, bordes.esquinaInferiorIzquierda,
                                 bordes.esquinaInferiorDerecha));

    return ftxui::vbox(std::move(lineas));
}

std::vector<int> Tabla::anchos() const {
    std::vector<int> anchos(_cabeceras.size(), 0);

    // La cabecera manda: es el piso del ancho de la columna.
    for (std::size_t i = 0; i < _cabeceras.size(); ++i) {
        anchos[i] = ftxui::string_width(_cabeceras[i]);
    }

    // Y despues el dato mas largo de la columna.
    for (const Fila& datos : _filas) {
        for (std::size_t i = 0; i < anchos.size() && i < datos.celdas.size(); ++i) {
            const int ancho = ftxui::string_width(datos.celdas[i]);
            if (ancho > anchos[i]) {
                anchos[i] = ancho;
            }
        }
    }

    return anchos;
}

ftxui::Element Tabla::lineaBorde(const std::vector<int>& anchos, char izquierda,
                                  char derecha) const {
    const Bordes& bordes = _tema.bordes();

    std::string texto(1, izquierda);
    for (const int ancho : anchos) {
        // Cada columna ocupa su ancho mas los dos espacios de padding.
        texto.append(static_cast<std::size_t>(ancho) + 2, bordes.horizontal);
        texto += derecha;
    }

    ftxui::Element elemento = ftxui::text(texto);
    if (const auto color = _tema.colorDe(Rol::TablaBorde)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

Rol Tabla::rolDeCelda(const Fila& fila, std::size_t columna) const {
    // EL ROL EXPLICITO GANA
    //
    // Es el que sabe la pantalla: la celda del estado va con el color de su
    // estado, aunque la fila entera este atenuada (el estado es lo que mas
    // dice de la fila).
    //
    // EL ROL DE CELDA NO ES UN ROL EXPLICITO
    //
    // Es el rol POR DEFECTO de una celda: si la fila esta atenuada, la
    // atenuacion gana (una fila de una entidad dada de baja se ve atenuada
    // entera).
    if (columna < fila.roles.size() && fila.roles[columna] != Rol::TablaCelda) {
        return fila.roles[columna];
    }

    // UNA FILA ATENUADA VA CON EL ROL SECUNDARIO
    //
    // La entidad esta dada de baja: se ve, pero no compite con las vivas.
    if (fila.atenuada) {
        return Rol::Secundario;
    }

    // UNA CELDA NORMAL VA CON EL COLOR POR DEFECTO
    //
    // El rol de celda no tiene color propio: la tabla se lee como texto.
    return Rol::TablaCelda;
}

ftxui::Element Tabla::fila(const Fila& fila, const std::vector<int>& anchos,
                            bool cabecera) const {
    const Bordes& bordes = _tema.bordes();

    ftxui::Elements partes;

    // El separador vertical del principio de la fila.
    ftxui::Element separador = ftxui::text(std::string(1, bordes.vertical));
    if (const auto color = _tema.colorDe(Rol::TablaBorde)) {
        separador = separador | ftxui::color(aFtxui(*color));
    }
    partes.push_back(separador);

    for (std::size_t i = 0; i < anchos.size(); ++i) {
        const std::string celda = i < fila.celdas.size() ? fila.celdas[i] : std::string();

        // EL ANCHO FIJO DE LA COLUMNA
        //
        // `size(WIDTH, EQUAL, n)` deja el elemento con EXACTAMENTE n
        // columnas: si el texto es mas corto, lo rellena con espacios; si es
        // mas largo, lo recorta. Es lo que hace que las columnas queden
        // alineadas sin tener que padear el texto a mano (que es lo que
        // cualquiera que sepa que un caracter UTF-8 puede pesar dos bytes
        // terminaria haciendo mal).
        ftxui::Element contenido = ftxui::text(celda) |
                                   ftxui::size(ftxui::WIDTH, ftxui::EQUAL, anchos[i]);

        const Rol rol = cabecera ? Rol::TablaHeader : rolDeCelda(fila, i);
        if (const auto color = _tema.colorDe(rol)) {
            contenido = contenido | ftxui::color(aFtxui(*color));
            if (cabecera) {
                // LA CABECERA VA EN NEGRITA
                //
                // Es el rotulo de la columna: tiene que destacar de las
                // celdas (que van con el color por defecto) sin dejar de
                // leerse como texto.
                //
                // LA NEGRITA VA CON EL COLOR
                //
                // Con el tema apagado la tabla sale como texto plano, sin
                // ningun codigo de escape: el `NO_COLOR` tiene que apagarlo
                // TODO, no solo el color.
                contenido = contenido | ftxui::bold;
            }
        }

        partes.push_back(ftxui::text(" "));
        partes.push_back(contenido);
        partes.push_back(ftxui::text(" "));

        ftxui::Element cierre = ftxui::text(std::string(1, bordes.vertical));
        if (const auto color = _tema.colorDe(Rol::TablaBorde)) {
            cierre = cierre | ftxui::color(aFtxui(*color));
        }
        partes.push_back(cierre);
    }

    return ftxui::hbox(std::move(partes));
}

} // namespace ui
