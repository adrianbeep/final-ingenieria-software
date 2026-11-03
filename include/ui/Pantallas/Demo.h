#pragma once

#include "ui/Menu.h"
#include "ui/Pantalla.h"
#include "ui/Theme.h"

#include <string>

// ---------------------------------------------------------------------------
// La pantalla de DEMOSTRACION del motor (decision D32).
//
// No es una pantalla del sistema: existe para PROBAR que el motor anda antes
// de que exista una sola pantalla real. Muestra la paleta de colores, el
// tamano del terminal y la posicion del raton, y sale con la opcion '0'.
//
// Las tres cosas que muestra son las tres que hay que ver para saber que el
// motor funciona:
//   - la paleta: prueba que los colores se pintan (y que `NO_COLOR` los
//     apaga);
//   - el tamano: prueba que la App de FTXUI se creo bien;
//   - la posicion del raton: prueba que el mouse llega a la pantalla.
// ---------------------------------------------------------------------------
namespace ui {

class Aplicacion;

namespace Pantallas {

class Demo : public Pantalla {
public:
    explicit Demo(Aplicacion& aplicacion);

    std::string titulo() const override;
    ftxui::Element renderizar() override;
    bool manejar(const Evento& evento) override;
    std::string textoPlano() const override;

private:
    // La fila de la pantalla donde se dibuja la PRIMERA opcion del menu.
    //
    // El hit-testing del raton es una comparacion de la fila del clic con
    // `FILA_INICIAL + i`. Si el dibujo cambia (un encabezado mas, un
    // separador mas), este numero tiene que cambiar con el: por eso es una
    // constante con nombre y no un numero suelto en el codigo del clic.
    static constexpr int FILA_INICIAL = 2;

    // Las tres cosas que la demo muestra.
    void mostrarPaleta();
    void mostrarTamano();
    void mostrarRaton();

    // El encabezado (fila 0), el separador (fila 1) y las opciones (filas 2
    // en adelante).
    ftxui::Element encabezado() const;
    ftxui::Element lineaOpcion(int indice) const;
    ftxui::Element lineaPaleta() const;

    Aplicacion& _aplicacion;
    Menu _menu;
    Theme _tema;

    // Lo que se muestra debajo del menu.
    std::string _informacion;

    // La ultima posicion del raton que se vio (-1 si todavia no se movio).
    int _ultimoX = -1;
    int _ultimoY = -1;
};

} // namespace Pantallas
} // namespace ui
