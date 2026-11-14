#pragma once

#include "ui/ColorFtxui.h"
#include "ui/Tabla.h"
#include "ui/Theme.h"

#include <ftxui.hpp>

#include <cstddef>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// El DIBUJO de las lineas de la aplicacion.
//
// POR QUE VIVE ACA Y NO EN CADA PANTALLA
//
// Las mismas lineas se dibujan en seis pantallas: la de una opcion de menu,
// la de un campo del formulario, la de atajos del pie y la de un dato que la
// pantalla trata. Si cada pantalla escribiera la suya, el color de una linea
// podria quedar distinto del de la misma linea en otra pantalla, y con
// `NO_COLOR` la diferencia se veria.
//
// LA REGLA DE LOS ROLES
//
// Cada linea se pinta con los roles que dicen PARA QUE SIRVE, no con los que
// dicen que elemento es:
//
//   - el DIGITO de una opcion va con el color de realce: es la tecla que hay
//     que apretar, y tiene que saltar a la vista;
//   - la ETIQUETA de un campo va con el color de etiqueta de campo y el
//     VALOR con el color por defecto: el valor es el dato, y el dato se lee
//     con el color de siempre;
//   - la linea de atajos del pie va con el color de ayuda: es una
//     referencia, no un dato;
//   - el DATO que la pantalla trata (el paciente elegido, el total de un
//     reporte) va con el color de realce, y su id con el color secundario.
//
// POR QUE SON `inline`
//
// Son funciones chiquitas que dependen de clases concretas (las del `ui`): si
// vivieran en un .cpp, el header tendria que declarar esas clases igual, asi
// que no se gana nada. Es el mismo criterio que usan los otros helpers de la
// capa `ui` (`ui::NombresDePersonas.h`, `ui::RenderDeSelector.h`).
// ---------------------------------------------------------------------------
namespace ui {

// ---------------------------------------------------------------------------
// EL ROL DE LA CELDA DEL ESTADO
//
// EL TEXTO DE LA CELDA ES EL ESTADO
//
// La columna del estado de una tabla dice COMO esta la fila, y el color
// tiene que decirlo tambien: es lo que hace que una lista de turnos se lea
// de un vistazo.
//
// SON DOS VOCABULARIOS
//
//   - el del TURNO es el `EstadoTurno`: "Activo", "Cancelado",
//     "Reprogramado";
//   - el de una PERSONA es el flag `activo`: "activo" e "inactivo".
//
// UN "INACTIVO" ES UN "CANCELADO"
//
// Una persona dada de baja no se puede usar: es el mismo meaning que un turno
// cancelado (no se va a usar), asi que va con el mismo rol. La fila entera,
// ademas, va atenuada: se ve que esta, pero no compite con las vivas.
// ---------------------------------------------------------------------------
inline Rol rolDeEstado(const std::string& texto) {
    if (texto == "Activo" || texto == "activo") {
        return Rol::EstadoActivo;
    }
    if (texto == "Cancelado" || texto == "inactivo") {
        return Rol::EstadoCancelado;
    }
    if (texto == "Reprogramado") {
        return Rol::EstadoReprogramado;
    }
    // Un texto que no es un estado conocido va con el rol de celda: la tabla
    // no tiene que inventar el significado de un dato.
    return Rol::TablaCelda;
}

// ---------------------------------------------------------------------------
// LOS ROLES DE UNA FILA DE TABLA
//
// LAS COLUMNAS CON SIGNIFICADO SON DOS
//
// La tabla de turnos, la de pacientes, la de medicos y las de los reportes
// tienen las mismas dos columnas con significado propio:
//
//   - la del ID va con el rol secundario: es una referencia, no el dato que
//     la pantalla trata;
//   - la del ESTADO va con el color de su estado.
//
// Las demas celdas van con el rol de celda (el color por defecto).
//
// UNA COLUMNA QUE NO EXISTE NO ROMPE NADA
//
// Si la tabla no tiene columna de estado (la de ocupacion, por ejemplo), no
// hay celda de estado: la fila se dibuja entera con el rol de celda.
// ---------------------------------------------------------------------------
inline std::vector<Rol> rolesDeFila(const std::vector<std::string>& cabeceras,
                                     const std::vector<std::string>& celdas) {
    std::vector<Rol> roles(celdas.size(), Rol::TablaCelda);

    for (std::size_t i = 0; i < cabeceras.size() && i < celdas.size(); ++i) {
        if (cabeceras[i] == "id") {
            roles[i] = Rol::Secundario;
        } else if (cabeceras[i] == "estado") {
            roles[i] = rolDeEstado(celdas[i]);
        }
    }

    return roles;
}

// ---------------------------------------------------------------------------
// LA FILA DE UNA TABLA, CON SUS ROLES
//
// La celda del id va con el rol secundario y la del estado con el color de su
// estado. Las demas van con el rol de celda (el color por defecto).
//
// UNA FILA ATENUADA
//
// La entidad esta dada de baja: la fila va atenuada, para que se vea que esta
// pero que no compita con las que estan vivas.
// ---------------------------------------------------------------------------
inline Tabla::Fila filaDeTabla(const std::vector<std::string>& cabeceras,
                                const std::vector<std::string>& celdas,
                                bool atenuada = false) {
    Tabla::Fila fila;
    fila.celdas = celdas;
    fila.roles = rolesDeFila(cabeceras, celdas);
    fila.atenuada = atenuada;
    return fila;
}

// ---------------------------------------------------------------------------
// LAS FILAS DE UNA TABLA, CON SUS ROLES
//
// Es el `filaDeTabla` de toda una tabla: lo usan las tablas que llegan
// armadas desde otro lado (los turnos de la ficha, las filas de un reporte).
// ---------------------------------------------------------------------------
inline std::vector<Tabla::Fila> filasDeTabla(
    const std::vector<std::string>& cabeceras,
    const std::vector<std::vector<std::string>>& filas) {
    std::vector<Tabla::Fila> resultado;
    resultado.reserve(filas.size());
    for (const std::vector<std::string>& fila : filas) {
        resultado.push_back(filaDeTabla(cabeceras, fila));
    }
    return resultado;
}

// ---------------------------------------------------------------------------
// LA LINEA DE UNA OPCION DE MENU
//
// EL DIGITO VA CON EL COLOR DE REALCE
//
// Es la tecla que hay que apretar: el atajo es lo que el usuario tiene que
// ver para elegir rapido. La etiqueta se queda con el color por defecto,
// asi que el ojo va al numero y no a una lista de textos iguales.
//
// EL RESALTADO DE LA ELEGIDA
//
// `inverted` funciona SIN color, asi que el menu se ve igual con el tema
// apagado. Y el invertido no borra el color: el digito sigue siendo el
// numero que hay que apretar.
// ---------------------------------------------------------------------------
inline ftxui::Element lineaDeOpcion(const Theme& tema, const std::string& atajo,
                                     const std::string& etiqueta, bool elegida) {
    ftxui::Elements partes;

    partes.push_back(ftxui::text("  "));

    ftxui::Element digito = ftxui::text(atajo);
    if (const auto color = tema.colorDe(Rol::Realce)) {
        digito = digito | ftxui::color(aFtxui(*color));
    }
    partes.push_back(digito);

    partes.push_back(ftxui::text(". " + etiqueta));

    ftxui::Element elemento = ftxui::hbox(std::move(partes));
    if (elegida) {
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

// ---------------------------------------------------------------------------
// LA LINEA DE UN CAMPO DEL FORMULARIO
//
// LA ETIQUETA Y EL VALOR NO SE CONFUNDEN
//
// La etiqueta dice que campo es ("Fecha:") y el valor es lo que el usuario
// escribio. Por eso van con roles distintos: se lee de un vistazo que parte
// de la linea es el rotulo y que parte es el dato.
//
// EL VALOR VA CON EL COLOR POR DEFECTO
//
// El rol `ValorDeCampo` no tiene color propio: el valor es el dato, y el
// dato se lee con el color de siempre. El texto que el usuario escribio no
// compite con nada.
// ---------------------------------------------------------------------------
inline ftxui::Element lineaDeCampo(const Theme& tema, const std::string& etiqueta,
                                    const std::string& valor) {
    ftxui::Elements partes;

    ftxui::Element rotulo = ftxui::text("  " + etiqueta + ":");
    if (const auto color = tema.colorDe(Rol::EtiquetaDeCampo)) {
        rotulo = rotulo | ftxui::color(aFtxui(*color));
    }
    partes.push_back(rotulo);
    partes.push_back(ftxui::text(" "));
    partes.push_back(ftxui::text(valor));

    return ftxui::hbox(std::move(partes));
}

// ---------------------------------------------------------------------------
// LA LINEA DEL DATO QUE LA PANTALLA TRATA
//
// EL VALOR VA CON EL COLOR DE REALCE
//
// Es lo que la pantalla trata: el paciente que se eligio, la especialidad,
// el medico, el total de un reporte. Tiene que saltar a la vista.
//
// EL ID VA CON EL COLOR SECUNDARIO
//
// El id es una referencia: sirve para elegir, no para leer. Va atenuado para
// que no compita con el nombre.
// ---------------------------------------------------------------------------
inline ftxui::Element lineaDeRealce(const Theme& tema, const std::string& etiqueta,
                                     const std::string& valor, const std::string& id = "") {
    ftxui::Elements partes;

    ftxui::Element rotulo = ftxui::text("  " + etiqueta + ":");
    if (const auto color = tema.colorDe(Rol::EtiquetaDeCampo)) {
        rotulo = rotulo | ftxui::color(aFtxui(*color));
    }
    partes.push_back(rotulo);
    partes.push_back(ftxui::text(" "));

    ftxui::Element dato = ftxui::text(valor);
    if (const auto color = tema.colorDe(Rol::Realce)) {
        dato = dato | ftxui::color(aFtxui(*color));
    }
    partes.push_back(dato);

    if (!id.empty()) {
        partes.push_back(ftxui::text(" "));
        ftxui::Element referencia = ftxui::text("(#" + id + ")");
        if (const auto color = tema.colorDe(Rol::Secundario)) {
            referencia = referencia | ftxui::color(aFtxui(*color));
        }
        partes.push_back(referencia);
    }

    return ftxui::hbox(std::move(partes));
}

// ---------------------------------------------------------------------------
// LA LINEA DE ATAJOS DEL PIE
//
// Es una REFERENCIA, no un dato: por eso va con el color de ayuda, que es
// el que no compite con el contenido de la pantalla.
// ---------------------------------------------------------------------------
inline ftxui::Element lineaDeAyuda(const Theme& tema, const std::string& texto) {
    ftxui::Element elemento = ftxui::text(texto);
    if (const auto color = tema.colorDe(Rol::Ayuda)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

} // namespace ui
