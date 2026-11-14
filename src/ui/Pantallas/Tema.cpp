#include "ui/Pantallas/Tema.h"

#include "ui/ColorFtxui.h"
#include "ui/Contexto.h"

#include <ftxui.hpp>

#include <cstddef>
#include <string>
#include <utility>

// ---------------------------------------------------------------------------
// La pantalla del TEMA DE COLOR.
//
// Lo que esta en el header es el POR QUE; lo que esta aca es el COMO.
//
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {

PantallaTema::PantallaTema(Aplicacion& aplicacion)
    : _aplicacion(aplicacion), _titulo("TEMA"), _menu("Colores") {
    // EL MENU DE LOS TRES TEMAS
    //
    // El atajo es el numero que se escribe (decision D28) y el '0' es
    // "Volver" en todos los niveles.
    _menu.agregar('1', "Apagados", [this]() { aplicarTema(Tema::Apagados); });
    _menu.agregar('2', "Oscuro", [this]() { aplicarTema(Tema::Oscuro); });
    _menu.agregar('3', "Claro", [this]() { aplicarTema(Tema::Claro); });
    _menu.agregar('0', "Volver", [this]() { _aplicacion.volver(); });

    // El menu se dibuja a partir de la fila FILA_INICIAL: el hit-testing del
    // raton lo necesita.
    _menu.definirFilaInicial(3);
}

std::string PantallaTema::titulo() const {
    return _titulo;
}

// ---------------------------------------------------------------------------
// La navegacion
// ---------------------------------------------------------------------------

bool PantallaTema::esTeclaVolver(const Evento& evento) const {
    return evento.tipo == TipoTecla::Caracter && evento.caracter == "0";
}

bool PantallaTema::esTeclaEscape(const Evento& evento) const {
    return evento.tipo == TipoTecla::Escape;
}

// ---------------------------------------------------------------------------
// Los manejadores de cada vista
// ---------------------------------------------------------------------------

bool PantallaTema::manejar(const Evento& evento) {
    // EL '0' ES VOLVER
    //
    // Es la regla de UX del proyecto: en todos los niveles, sin excepcion.
    if (esTeclaVolver(evento)) {
        _aplicacion.volver();
        return true;
    }

    // EL `Escape` TAMBIEN SACA LA PANTALLA
    //
    // Esta pantalla no tiene vistas internas: no hay ningun nivel interno al
    // que el `Escape` pueda subir, asi que el unico nivel es el de la
    // aplicacion (sacar la pantalla de la pila).
    if (esTeclaEscape(evento)) {
        _aplicacion.volver();
        return true;
    }

    // EL MENU SE ENCARGA DEL RESTO
    //
    // Las flechas mueven, el Enter elige y el clic elige la opcion de la
    // fila.
    return _menu.aplicarEvento(evento);
}

// ---------------------------------------------------------------------------
// Las acciones
// ---------------------------------------------------------------------------

void PantallaTema::aplicarTema(Tema tema) {
    // EL TEMA ES DEL ESTADO COMPARTIDO
    //
    // No es un valor de esta pantalla: es el estado que comparten TODAS las
    // pantallas (ver `ui::Theme::establecerTema`). Por eso el efecto es
    // inmediato en las que ya estan abiertas.
    Theme::establecerTema(tema);

    // El resaltado del menu sigue al tema activo: asi se ve cual esta.
    const int indice = indiceDe(tema);
    if (indice >= 0) {
        _menu.seleccionar(indice);
    }

    avisarExito("Tema " + nombreDe(tema) + ".");
}

// ---------------------------------------------------------------------------
// Los mensajes
// ---------------------------------------------------------------------------

void PantallaTema::avisarExito(const std::string& texto) {
    // El mensaje vive en el CONTEXTO: es lo que la pantalla anterior va a
    // mostrar cuando el usuario vuelva.
    _aplicacion.contexto().mostrarMensaje(Mensaje::exito(texto));
}

const Mensaje& PantallaTema::mensajeActual() const {
    return _aplicacion.contexto().mensaje();
}

// ---------------------------------------------------------------------------
// El dibujo
// ---------------------------------------------------------------------------

ftxui::Element PantallaTema::encabezado() const {
    ftxui::Element elemento = ftxui::text(_aplicacion.titulo() + " - " + _titulo);
    if (const auto color = _tema.colorDe(Rol::Titulo)) {
        elemento = elemento | ftxui::color(aFtxui(*color)) | ftxui::bold;
    }
    return elemento;
}

ftxui::Element PantallaTema::lineaOpcion(int indice) const {
    return lineaDeOpcion(_tema, std::string(1, _menu.atajoDe(indice)),
                          _menu.etiquetas()[static_cast<std::size_t>(indice)],
                          indice == _menu.seleccion());
}

ftxui::Element PantallaTema::lineaTemaActivo() const {
    // EL TEMA ACTIVO
    //
    // Es lo que la pantalla trata: va con el color de realce.
    //
    // CON EL TEMA APAGADO NO HAY COLOR PROPIO
    //
    // Con el tema "Apagados" esta linea sale con el color por defecto de la
    // terminal: es el precio de apagar el color, y el nombre del tema sigue
    // diciendo cual esta elegido.
    return lineaDeRealce(_tema, "Tema activo", nombreDe(_tema.tema()));
}

ftxui::Element PantallaTema::lineaMensaje() const {
    const Mensaje& mensaje = mensajeActual();
    ftxui::Element elemento = ftxui::text("  " + mensaje.texto());

    Rol rol = Rol::Secundario;
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

std::string PantallaTema::ayuda() const {
    // LA AYUDA DICE COMO VOLVER
    //
    // Un mensaje que no dice como se vuelve deja al usuario atrapado.
    return "Flechas: mover   Enter: elegir   0: volver   Esc: volver";
}

ftxui::Element PantallaTema::renderizar() {
    ftxui::Elements lineas;

    lineas.push_back(encabezado());
    lineas.push_back(ftxui::separator());

    // La opcion `i` del menu queda en la fila `filaInicial + i`: el
    // hit-testing del raton lo necesita.
    _menu.definirFilaInicial(static_cast<int>(lineas.size()));
    for (int i = 0; i < _menu.cantidad(); ++i) {
        lineas.push_back(lineaOpcion(i));
    }
    lineas.push_back(ftxui::separator());

    lineas.push_back(lineaTemaActivo());

    if (!mensajeActual().esVacio()) {
        lineas.push_back(lineaMensaje());
    }
    lineas.push_back(lineaDeAyuda(_tema, ayuda()));

    return ftxui::vbox(std::move(lineas));
}

// ---------------------------------------------------------------------------
// El texto plano (lo que se ve sin terminal)
// ---------------------------------------------------------------------------

std::string PantallaTema::nombreDe(Tema tema) {
    switch (tema) {
        case Tema::Apagados:
            return "Apagados";
        case Tema::Oscuro:
            return "Oscuro";
        case Tema::Claro:
            return "Claro";
    }
    return "Oscuro";
}

int PantallaTema::indiceDe(Tema tema) {
    // EL MISMO ORDEN QUE EL ENUM
    //
    // El menu tiene los tres temas en el orden del enum, asi que el indice
    // del tema es su posicion en el menu.
    switch (tema) {
        case Tema::Apagados:
            return 0;
        case Tema::Oscuro:
            return 1;
        case Tema::Claro:
            return 2;
    }
    return -1;
}

std::string PantallaTema::textoDeLaVista() const {
    std::string salida;

    for (int i = 0; i < _menu.cantidad(); ++i) {
        salida += "  " + std::string(1, _menu.atajoDe(i)) + ". "
            + _menu.etiquetas()[static_cast<std::size_t>(i)] + "\n";
    }
    salida += "  Tema activo: " + nombreDe(_tema.tema()) + "\n";
    return salida;
}

std::string PantallaTema::vistaTextoPlano() const {
    // SOLO LA VISTA
    //
    // Es lo que se ve ahora, sin el encabezado ni la ayuda: es lo que
    // necesita el test para comprobar que la vista muestra lo que tiene que
    // mostrar.
    return textoDeLaVista();
}

std::string PantallaTema::textoPlano() const {
    std::string salida = _aplicacion.titulo() + " - " + _titulo + "\n";
    salida += textoDeLaVista();
    if (!mensajeActual().esVacio()) {
        salida += mensajeActual().texto() + "\n";
    }
    salida += ayuda();
    return salida;
}

std::unique_ptr<Pantalla> crearPantallaTema(Aplicacion& aplicacion) {
    return std::make_unique<PantallaTema>(aplicacion);
}

} // namespace Pantallas
} // namespace ui
