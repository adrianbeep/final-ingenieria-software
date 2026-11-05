#pragma once

#include "ui/Menu.h"

#include <string>

// ---------------------------------------------------------------------------
// La confirmacion de dos pasos.
//
// UNA CONFIRMACION NECESITA ENTRADA DEL USUARIO (decision D37)
//
// No puede ser una funcion pura (como las validaciones de `Consola`) ni una
// impresion: hace falta que el usuario elija. Con dos opciones, es un `Menu`
// con el atajo '1' para confirmar y el atajo '2' para cancelar.
//
// LOS ATAJOS SON '1' Y '2', NO '0'
//
// El '0' es "Volver" en todos los niveles del menu (la regla de UX del
// proyecto) y una confirmacion no es "volver": es una decision. Por eso los
// dos atajos son '1' y '2'.
// ---------------------------------------------------------------------------
namespace ui {

class Confirmacion {
public:
    explicit Confirmacion(std::string pregunta);

    // Aplica un evento al menu de la confirmacion. Devuelve true si el
    // evento fue consumido (es decir, si el usuario eligio una opcion).
    bool aplicarEvento(const Evento& evento);

    // Si el usuario ya eligio una opcion.
    bool respondida() const;

    // Si la respuesta fue "Si". Solo tiene sentido si `respondida()` es
    // true.
    bool confirmo() const;

    const std::string& pregunta() const;

    // La confirmacion en texto plano: la pregunta y las dos opciones. Es lo
    // que se imprime cuando NO hay terminal interactiva (D31).
    std::string textoPlano() const;

private:
    Menu _menu;
    bool _respondida = false;
    bool _confirmo = false;
};

} // namespace ui
