#include "ui/Pantallas/MenuPrincipal.h"

#include "ui/Aplicacion.h"
#include "ui/ColorFtxui.h"
#include "ui/Contexto.h"
#include "ui/Mensaje.h"
#include "ui/Pantallas/AcercaDe.h"
#include "ui/Pantallas/Calendario.h"
#include "ui/Pantallas/Configuracion.h"
#include "ui/Pantallas/Pacientes.h"
#include "ui/Pantallas/Medicos.h"
#include "ui/Pantallas/Reportes.h"
#include "ui/Pantallas/Turnos.h"
#include "ui/Reloj.h"

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
    _menu.agregar('3', "Turnos", [this]() { abrirTurnos(); });
    _menu.agregar('4', "Reportes", [this]() { abrirReportes(); });
    _menu.agregar('5', "Calendario", [this]() { abrirCalendario(); });
    _menu.agregar('6', "Configuracion", [this]() { abrirConfiguracion(); });
    _menu.agregar('7', "Acerca de", [this]() { abrirAcercaDe(); });
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

void MenuPrincipal::abrirTurnos() {
    // LA PANTALLA DE TURNOS NECESITA LOS TRES REPOSITORIOS
    //
    // El de turnos para listar y para leer, el de pacientes y el de medicos
    // para resolver los NOMBRES de la tabla (el turno guarda los ids, no
    // los nombres) y el servicio, que es donde estan las reglas.
    _aplicacion.ir([](ui::Aplicacion& app) {
        return crearPantallaTurnos(app, app.contexto().turnos(), app.contexto().pacientes(),
                                   app.contexto().medicos(), app.contexto().servicioTurnos());
    });
}

void MenuPrincipal::abrirReportes() {
    // LA PANTALLA DE REPORTES NECESITA LOS DOS REPOSITORIOS DE PERSONAS
    //
    // Los de pacientes y de medicos para resolver los NOMBRES de las
    // tablas (el turno guarda los ids, no los nombres) y el servicio, que
    // es donde estan las consultas de los reportes.
    _aplicacion.ir([](ui::Aplicacion& app) {
        return crearPantallaReportes(app, app.contexto().pacientes(), app.contexto().medicos(),
                                      app.contexto().servicioTurnos());
    });
}

void MenuPrincipal::abrirCalendario() {
    // LA PANTALLA DE CALENDARIO NECESITA LOS DOS REPOSITORIOS DE PERSONAS
    //
    // Los de pacientes y de medicos para resolver los NOMBRES de la tabla
    // del detalle del dia (el turno guarda los ids, no los nombres) y el
    // servicio, que es donde estan las consultas de los turnos.
    //
    // Y LA FECHA DE HOY
    //
    // La pantalla NO la consulta: la RECIBE. Aca se le pasa la del reloj
    // del sistema (ver `ui/Reloj.h`), asi que la pantalla no tiene una regla
    // que no sea la suya y los tests pueden fijarla.
    _aplicacion.ir([](ui::Aplicacion& app) {
        return crearPantallaCalendario(app, app.contexto().pacientes(), app.contexto().medicos(),
                                       app.contexto().servicioTurnos(), ui::hoyDelSistema());
    });
}

void MenuPrincipal::abrirConfiguracion() {
    // LA PANTALLA DE CONFIGURACION
    //
    // No necesita nada del contexto: el toggle de los colores escribe el
    // estado COMPARTIDO de la paleta (ver `ui::Theme`) y la carga de datos de
    // ejemplo usa el contexto (que es donde estan los repositorios).
    _aplicacion.ir([](ui::Aplicacion& app) {
        return crearPantallaConfiguracion(app);
    });
}

void MenuPrincipal::abrirAcercaDe() {
    // LA PANTALLA DE "ACERCA DE"
    //
    // Es de SOLO LECTURA: no necesita ni el contexto ni ningun repositorio.
    _aplicacion.ir([](ui::Aplicacion& app) {
        return crearPantallaAcercaDe(app);
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
