#pragma once

#include "ui/Evento.h"

#include <functional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// El menu de la aplicacion.
//
// POR QUE NO ES `ftxui::Menu` (decision D28)
//
// El menu de FTXUI no se puede testear con el arnes propio: no hay forma de
// preguntarle cual es la seleccion, ni de inyectarle un evento sin correr el
// ciclo de eventos, ni de ejecutarle una accion por opcion. Este menu es
// NUESTRO: la logica (la seleccion, la navegacion, los atajos y el
// hit-testing del raton) vive aca y se testea sin terminal.
//
// EL ATajo ES EL NUMERO QUE SE ESCRIBE
//
// Cada opcion tiene un atajo de un digito ('0' a '9'). El atajo es lo que se
// MUESTRA y lo que el usuario ESCRIBE, asi que no hay dos listas que puedan
// desincronizarse. '0' es "Volver" en todos los niveles (la regla de UX del
// proyecto).
// ---------------------------------------------------------------------------
namespace ui {

class Menu {
public:
    explicit Menu(std::string titulo);

    // Agrega una opcion.
    //
    // El atajo tiene que ser un DIGITO y no puede estar repetido: los dos
    // son errores de programacion (no datos invalidos del usuario), asi que
    // lanza `DatoInvalidoException`.
    void agregar(char atajo, const std::string& etiqueta, std::function<void()> accion);

    // Mueve la seleccion una linea.
    //
    // LA NAVEGACION NO DA LA VUELTA
    //
    // Al llegar al extremo, la seleccion se QUEDA: no salta de la primera
    // opcion a la ultima ni al reves.
    //
    // Por que: (a) el menu entero esta a la vista (son pocas opciones), asi
    // que no hay nada que"Justificar con scroll; (b) con el menu a la vista,
    // un salto del extremo al otro hace que el ojo tenga que volver a
    // encontrar la fila; (c) el raton y el teclado tienen que ser
    // EQUIVALENTES: un click en la primera fila selecciona la primera, y
    // la flecha arriba en la primera tambien deja la primera.
    //
    // Devuelve true si la seleccion cambio.
    bool moverArriba();
    bool moverAbajo();

    // Aplica un evento al menu.
    //
    // - las flechas mueven la seleccion;
    // - Enter ejecuta la opcion seleccionada;
    // - un digito que sea atajo de alguna opcion la ejecuta;
    // - un clic del raton ejecuta la opcion de esa fila.
    //
    // Devuelve true si el evento fue CONSUMIDO. Un evento que el menu no
    // sabe que hacer (una tecla que no es un atajo, un clic que cae fuera
    // de las opciones) devuelve false, para que lo pueda atender la
    // pantalla.
    bool aplicarEvento(const Evento& evento);

    // HIT-TESTING DEL RATON
    //
    // El menu dibuja una opcion por linea, asi que la opcion `i` queda en
    // la fila `filaInicial + i`. La funcion devuelve el indice de la opcion
    // que hay en esa fila, o -1 si la fila no cae sobre ninguna.
    //
    // La columna (`x`) no se mira: la opcion ocupa la linea entera.
    int opcionEnFila(int y, int filaInicial) const;

    // La fila de la pantalla donde se dibuja la PRIMERA opcion.
    //
    // La pantalla que dibuja el menu tiene que avisarle en que fila
    // arranca: sin ese numero, el clic del raton no podria saber que
    // opcion se toco.
    void definirFilaInicial(int fila);
    int filaInicial() const;

    const std::string& titulo() const;
    const std::vector<std::string>& etiquetas() const;
    char atajoDe(int indice) const;
    int seleccion() const;
    void seleccionar(int indice);
    int cantidad() const;

    // El menu en texto plano: el titulo y las opciones, una por linea, con
    // el atajo adelante. Es lo que se imprime cuando NO hay terminal
    // interactiva (D31).
    std::string textoPlano() const;

private:
    struct Opcion {
        char atajo = '0';
        std::function<void()> accion;
    };

    // El indice de la opcion con ese atajo, o -1 si no hay ninguna.
    int indiceDeAtajo(char atajo) const;

    // Ejecuta la accion de la opcion. Si la opcion no tiene accion, no
    // hace nada.
    void ejecutar(int indice);

    std::string _titulo;

    // Las opciones y las etiquetas van en DOS vectores paralelos, y no en
    // uno solo, porque `etiquetas()` tiene que devolver una REFERENCIA a un
    // vector de strings: si las etiquetas vivieran dentro de `Opcion`, la
    // funcion tendria que devolver una copia (y una copia por frame).
    std::vector<Opcion> _opciones;
    std::vector<std::string> _etiquetas;

    int _seleccion = 0;
    int _filaInicial = 0;
};

} // namespace ui
