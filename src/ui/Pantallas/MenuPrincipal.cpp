#include "ui/Pantallas/MenuPrincipal.h"

#include "ui/Aplicacion.h"
#include "ui/ColorFtxui.h"
#include "ui/Contexto.h"
#include "ui/Mensaje.h"
#include "ui/Pantallas/Pacientes.h"
#include "ui/Pantallas/Medicos.h"

#include <ftxui.hpp>

#include <cstddef>
#include <string>

namespace ui {
namespace Pantallas {

MenuPrincipal::MenuPrincipal(Aplicacion& aplicacion)
    : _aplicacion(aplicacion), _menu("Sistema de Turnos") {
    // El atajo es el numero que se escribe (decision D28) y el '0' es
    // "Volver" en todos los niveles (aca, "Salir").
    _menu.agregar('1', "Pacientes", [this]() { abrirPacientes(); });
    _menu.agregar('2', "Medicos", [this]() { abrirMedicos(); });
    _menu.agregar('0', "Salir", [this]() { _aplicacion.salir(); });

    // El menu se dibuja a partir de la fila FILA_INICIAL: el hit-testing del
    // raton lo necesita para saber que opcion se toco.
    _menu.definirFilaInicial(FILA_INICIAL);
}

std::string MenuPrincipal::titulo() const {
    return "Menu principal";
}

void MenuPrincipal::abrirPacientes() {
    // Los repositorios y el servicio salen del CONTEXTO: la pantalla no
    // recibe collaborators por parametro, los pide al contexto de la
    // aplicacion.
    _aplicacion.ir([](ui::Aplicacion& app) {
        return crearPantallaPacientes(app, app.contexto().pacientes(),
                                    app.contexto().servicioTurnos());
    });
}

void MenuPrincipal::abrirMedicos() {
    _aplicacion.ir([](ui::Aplicacion& app) {
        return crearPantallaMedicos(app, app.contexto().medicos(),
                                       app.contexto().servicioTurnos());
    });
}

bool MenuPrincipal::manejar(const Evento& evento) {
    // El menu se encarga de todo: las flechas, Enter, los atajos y el clic.
    return _menu.aplicarEvento(evento);
}

std::string MenuPrincipal::textoPlano() const {
    std::string salida = _menu.textoPlano();
    const Mensaje& mensaje = _aplicacion.contexto().mensaje();
    if (!mensaje.esVacio()) {
        salida += "\n" + mensaje.texto();
    }
    return salida;
}

ftxui::Element MenuPrincipal::renderizar() {
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

    // EL MENSAJE DE LA ULTIMA OPERACION
    //
    // Va debajo del menu: es el resultado de lo que se hizo en la pantalla
    // anterior (por ejemplo, el alta de un paciente).
    const Mensaje& mensaje = _aplicacion.contexto().mensaje();
    if (!mensaje.esVacio()) {
        lineas.push_back(lineaMensaje());
    }

    lineas.push_back(ftxui::text("Flechas: mover   Enter: elegir   0: salir   Esc: salir"));

    return ftxui::vbox(std::move(lineas));
}

ftxui::Element MenuPrincipal::encabezado() const {
    ftxui::Element elemento = ftxui::text(_aplicacion.titulo());
    if (const auto color = _tema.colorDe(Rol::Titulo)) {
        elemento = elemento | ftxui::color(aFtxui(*color)) | ftxui::bold;
    }
    return elemento;
}

ftxui::Element MenuPrincipal::lineaOpcion(int indice) const {
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

ftxui::Element MenuPrincipal::lineaMensaje() const {
    const Mensaje& mensaje = _aplicacion.contexto().mensaje();
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

} // namespace Pantallas
} // namespace ui
