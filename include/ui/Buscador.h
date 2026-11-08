#pragma once

#include "ui/Campo.h"
#include "ui/Evento.h"
#include "ui/Selector.h"

#include <cstddef>
#include <functional>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// El buscador de entidades: se escribe un texto, se busca, y se elige UNA de
// las coincidencias.
//
// SON DOS COSAS EN UNA: el texto y la lista.
//
// El `Campo` (el texto) y el `Selector` (la lista) se pelean por el teclado:
// los DIGITOS son texto para el campo y numero de opcion para el selector.
// Por eso el buscador tiene DOS FOCOS y el usuario va de uno al otro con el
// Enter:
//
//   1. el usuario ESCRIBE (el Campo tiene el foco) y aprieta Enter;
//   2. la busqueda corre, la lista se actualiza y el foco pasa a la LISTA;
//   3. en la lista se elige con las flechas o escribiendo el numero, y el
//      Enter elige; el Retroceso vuelve al texto para corregirlo.
//
// SIEMPRE SE BUSCA AL CONFIRMAR, NO EN CADA TECLA
//
// Filtrar en vivo mientras se escribe tiene un problema con la numeracion
// posicional del `Selector` (el numero que se ve y el numero que se escribe
// tienen que ser el MISMO). Buscando al confirmar, la lista no cambia
// mientras se elige, y la numeracion es estable.
//
// POR QUE NO SABE NADA DE FTXUI
//
// Es LOGICA PURA, como el `ui::Selector` y el `ui::Campo`: no dibuja y no
// abre el ciclo de eventos. La pantalla dibuja las lineas y hace el
// hit-testing del raton (el widget solo le dice que coincidencia cae en cada
// fila).
// ---------------------------------------------------------------------------
namespace ui {

class Buscador {
public:
    // Una coincidencia de una busqueda: el id de la entidad y sus celdas.
    //
    // EL ID VIAJA CON LA FILA
    //
    // El id NO se deduce de la posicion de una columna: la columna del id
    // puede cambiar de lugar y el buscador devolveria la entidad
    // equivocada. El que busca ya tiene la entidad en la mano, asi que el id
    // lo devuelve con la fila.
    using Coincidencia = std::pair<int, std::vector<std::string>>;

    // La busqueda: recibe el texto escrito y devuelve las coincidencias.
    using Busqueda = std::function<std::vector<Coincidencia>(const std::string&)>;

    // `titulo` es el titulo de la pantalla ("Buscar paciente"), `pista` el
    // texto del campo ("Nombre, apellido o DNI").
    Buscador(std::string titulo, std::string pista, Busqueda buscar);

    // Aplica un evento. Devuelve true si lo consumio.
    //
    // EL `Escape` NO LO CONSUME
    //
    // El `Escape` es una tecla de navegacion y la decide la pantalla (vuelve
    // al menu). El `Campo` y el `Selector` tampoco lo consumen, asi que el
    // buscador devuelve false y el evento sigue de largo.
    bool aplicarEvento(const Evento& evento);

    // El texto que se esta escribiendo.
    const std::string& filtro() const;

    // El titulo de la pantalla.
    const std::string& titulo() const;

    // Si el foco esta en el texto (true) o en la lista (false).
    bool focoEnElTexto() const;

    // El error del ultimo intento (por ejemplo "no hay coincidencias").
    const std::string& error() const;

    // Si ya se eligio una entidad.
    bool elegido() const;

    // El id de la entidad elegida (-1 si todavia no se eligio ninguna).
    int idElegido() const;

    // Las coincidencias que muestra la lista.
    const std::vector<Coincidencia>& coincidencias() const;

    // El indice de la coincidencia RESALTADA (la que se elige con el Enter
    // si no se escribe un numero).
    int seleccion() const;

    // Elige la coincidencia de ese indice (la mueve el raton).
    //
    // UN INDICE FUERA DE RANGO SE IGNORA
    //
    // Es la misma regla del `ui::Selector`: un dato raro no puede romper la
    // pantalla.
    void seleccionar(int indice);

    // La linea del campo, con el cursor.
    std::string lineaDelCampo() const;

    // La linea de una coincidencia (sin el numero: el numero lo pone el que
    // dibuja, que es el que sabe la fila).
    std::string lineaDe(int indice) const;

    // Cuantas lineas ocupa la lista.
    int cantidadDeLineas() const;

    // El texto plano (para el modo no interactivo, D31).
    std::string textoPlano() const;

    // HIT-TESTING DEL RATON
    //
    // La coincidencia `i` queda en la fila `filaInicial + i`. La funcion
    // devuelve el indice de la coincidencia que hay en esa fila, o -1 si la
    // fila no cae sobre ninguna.
    int coincidenciaEnFila(int y, int filaInicial) const;

    // La fila de la pantalla donde se dibuja la PRIMERA coincidencia.
    void definirFilaInicial(int fila);
    int filaInicial() const;

private:
    // El evento, con el foco en el TEXTO.
    bool aplicarEventoEnElTexto(const Evento& evento);

    // El evento, con el foco en la LISTA.
    bool aplicarEventoEnLaLista(const Evento& evento);

    // Corre la busqueda con el texto escrito y llena la lista.
    void buscar();

    // Llena la lista con las coincidencias de un texto.
    //
    // LA BUSQUEDA INICIAL USA EL TEXTO VACIO
    //
    // El buscador se abre con la lista de las entidades (la del texto
    // vacio): el usuario tiene que ver que hay antes de elegir. El foco NO
    // se mueve: al abrir, el usuario viene a escribir.
    void llenarLaLista(const std::string& texto);

    // Toma la coincidencia resaltada: la entidad elegida.
    void elegir();

    // Las celdas de una coincidencia, en una sola linea.
    static std::string unirCeldas(const std::vector<std::string>& celdas);

    std::string _titulo;
    std::string _pista;
    Busqueda _buscar;

    // EL TEXTO
    //
    // El `Campo` NO valida nada: un texto vacio es una busqueda VALIDA (mira
    // la lista entera). Por eso su validador devuelve siempre cadena vacia.
    Campo _campo;

    // LA LISTA
    //
    // Una opcion por coincidencia: el `Selector` numera por posicion, asi
    // que el numero que se ve y el numero que se escribe son el MISMO.
    Selector _selector;

    // Las coincidencias de la ultima busqueda (con su id).
    std::vector<Coincidencia> _coincidencias;

    // Si el foco esta en el texto (true) o en la lista (false).
    bool _focoEnElTexto = true;

    // Si ya se eligio una entidad, y cual.
    bool _elegido = false;
    int _idElegido = -1;

    // El error de la ultima busqueda sin resultados.
    std::string _error;

    // La fila de la pantalla donde se dibuja la primera coincidencia.
    int _filaInicial = 0;
};

} // namespace ui
