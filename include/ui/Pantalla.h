#pragma once

#include "ui/Evento.h"

#include <ftxui.hpp>

#include <string>

// ---------------------------------------------------------------------------
// La interfaz de una pantalla.
//
// UNA PANTALLA ES UNA MAQUINA DE ESTADO (decision D29)
//
// No imprime y lee (el modelo de la consola clasica), sino que sabe
// DIBUJARSE y sabe QUE HACER con un evento. La diferencia no es de estilo:
// FTXUI redibuja POR EVENTO, asi que una pantalla que imprimiera y esperara
// una linea no se puede expresar en este modelo. Y el modelo de eventos es
// lo que hace que la navegacion con flechas y la del raton sean LA MISMA
// interaccion.
//
// La pantalla NO conoce la estructura de navegacion: de eso se ocupa la
// `Aplicacion` (la pila de pantallas).
// ---------------------------------------------------------------------------
namespace ui {

class Pantalla {
public:
    virtual ~Pantalla() = default;

    // El nombre de la pantalla.
    virtual std::string titulo() const = 0;

    // El frame de la pantalla.
    virtual ftxui::Element renderizar() = 0;

    // Que hace la pantalla con este evento. Devuelve true si lo consumio.
    virtual bool manejar(const Evento& evento) = 0;

    // El texto plano de la pantalla: es lo que se imprime cuando NO hay
    // terminal interactiva (D31). Por defecto es el titulo; las pantallas
    // con menu lo sobreescriben.
    virtual std::string textoPlano() const { return titulo(); }
};

} // namespace ui
