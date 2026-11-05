#pragma once

#include "ui/Menu.h"
#include "ui/Pantalla.h"
#include "ui/Theme.h"

#include <string>

// ---------------------------------------------------------------------------
// El menu principal de la aplicacion.
//
// NACE CON LAS OPCIONES CUYO MODULO YA EXISTE (decision D33)
//
// El menu no promete pantallas que todavia no estan: en este PR son
// "1. Pacientes", "2. Medicos" y "0. Salir". Los PRs siguientes van
// agregar su opcion en su lugar, asi que la lista nunca promete nada que no
// funcione.
//
// EL MENSAJE DE LA ULTIMA OPERACION
//
// La pantalla guarda el mensaje de la ultima operacion y lo muestra, para que
// el usuario vea el "OK  Paciente #7 registrado." al volver del motor CRUD. El
// mensaje vive en el contexto (es lo unico que se comparte entre pantallas).
// ---------------------------------------------------------------------------
namespace ui {

class Aplicacion;

namespace Pantallas {

class MenuPrincipal : public Pantalla {
public:
    explicit MenuPrincipal(Aplicacion& aplicacion);

    std::string titulo() const override;
    ftxui::Element renderizar() override;
    bool manejar(const Evento& evento) override;
    std::string textoPlano() const override;

private:
    // La fila de la pantalla donde se dibuja la PRIMERA opcion del menu.
    //
    // El hit-testing del raton es una comparacion de la fila del clic con
    // `FILA_INICIAL + i`. Si el dibujo cambia (un encabezado mas, un
    // separador mas), este numero tiene que cambiar con el.
    static constexpr int FILA_INICIAL = 2;

    void abrirPacientes();
    void abrirMedicos();

    ftxui::Element encabezado() const;
    ftxui::Element lineaOpcion(int indice) const;
    ftxui::Element lineaMensaje() const;

    Aplicacion& _aplicacion;
    Menu _menu;
    Theme _tema;
};

} // namespace Pantallas
} // namespace ui
