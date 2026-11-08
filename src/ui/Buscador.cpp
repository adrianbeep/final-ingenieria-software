#include "ui/Buscador.h"

#include <utility>

namespace ui {
namespace {

// EL SEPARADOR DE LAS CELDAS DE UNA COINCIDENCIA
//
// Las celdas de una coincidencia (el id, el apellido, el nombre...) se
// muestran en UNA sola linea, separadas por este punto. El separador hace
// que la linea se lea como una frase ("7 · García · Ana · 2020391") y no
// como una fila de tabla.
const char* SEPARADOR = " · ";

// EL ERROR DE UNA BUSQUEDA SIN COINCIDENCIAS
//
// Es el mensaje que el usuario necesita: la lista esta vacia y no hay nada
// que elegir. Sin el, el buscador pareceria roto.
const char* SIN_COINCIDENCIAS = "no hay coincidencias";

} // namespace

Buscador::Buscador(std::string titulo, std::string pista, Busqueda buscar)
    : _titulo(std::move(titulo)),
      _pista(std::move(pista)),
      _buscar(std::move(buscar)),
      // EL CAMPO NO VALIDA NADA
      //
      // Un texto vacio es una busqueda VALIDA: muestra la lista entera. Si
      // el campo lo rechazara, el buscador arrancaria con un error en vez de
      // con la lista.
      _campo(_pista, "", [](const std::string&) { return std::string(); }),
      _selector(_titulo, std::vector<std::string>()) {
    // LA LISTA ARRANCA CON EL CONTENIDO DE LA BUSQUEDA VACIA
    //
    // El buscador se abre para ELEGIR entre las entidades: si arrancara con
    // la lista vacia, el usuario veria una pantalla en blanco y tendria que
    // apretar Enter para saber que hay. La busqueda inicial (la del texto
    // vacio) muestra las entidades activas: es lo que el usuario espera ver
    // al abrir el buscador.
    //
    // EL FOCO QUEDA EN EL TEXTO
    //
    // El foco pasa a la LISTA cuando el usuario CONFIRMA una busqueda (el
    // Enter del texto). Al abrir, el usuario viene a ESCRIBIR: por eso el
    // foco no se mueve todavia. Tampoco hay error: el usuario todavia no
    // intento nada.
    llenarLaLista(std::string());
}

bool Buscador::aplicarEvento(const Evento& evento) {
    // EL RATON NO LO MANEJA EL BUSCADOR
    //
    // El hit-testing lo hace la pantalla, que es la que sabe en que fila
    // dibujo cada coincidencia (con `coincidenciaEnFila`).
    if (evento.esMouse()) {
        return false;
    }

    if (_focoEnElTexto) {
        return aplicarEventoEnElTexto(evento);
    }
    return aplicarEventoEnLaLista(evento);
}

const std::string& Buscador::filtro() const {
    return _campo.texto();
}

const std::string& Buscador::titulo() const {
    return _titulo;
}

bool Buscador::focoEnElTexto() const {
    return _focoEnElTexto;
}

const std::string& Buscador::error() const {
    // EL ERROR DEL SELECTOR TIENE PRIORIDAD
    //
    // El error del `Selector` ("opcion invalida") es del ULTIMO intento: el
    // usuario escribio un numero que no existe. El "no hay coincidencias" es
    // del intento ANTERIOR (la busqueda). El que se muestra es el del
    // ultimo intento.
    if (!_selector.error().empty()) {
        return _selector.error();
    }
    return _error;
}

bool Buscador::elegido() const {
    return _elegido;
}

int Buscador::idElegido() const {
    return _idElegido;
}

const std::vector<Buscador::Coincidencia>& Buscador::coincidencias() const {
    return _coincidencias;
}

int Buscador::seleccion() const {
    return _selector.seleccion();
}

void Buscador::seleccionar(int indice) {
    // Un indice fuera de rango SE IGNORA (el por que esta en el header).
    _selector.seleccionar(indice);
}

std::string Buscador::lineaDelCampo() const {
    return _pista + ": " + _campo.textoEnPantalla();
}

std::string Buscador::lineaDe(int indice) const {
    if (indice < 0 || indice >= static_cast<int>(_coincidencias.size())) {
        return "";
    }
    return unirCeldas(_coincidencias[static_cast<std::size_t>(indice)].second);
}

int Buscador::cantidadDeLineas() const {
    return static_cast<int>(_coincidencias.size());
}

std::string Buscador::textoPlano() const {
    // EL MISMO ORDEN QUE LA VISTA DIBUJADA
    //
    // El titulo, el campo con su cursor y las lineas de las coincidencias
    // (numeradas, como las de la lista dibujada).
    std::string salida = _titulo + "\n";
    salida += "  " + lineaDelCampo() + "\n";
    for (int i = 0; i < cantidadDeLineas(); ++i) {
        salida += "  " + std::to_string(i + 1) + ". " + lineaDe(i);
        if (i == seleccion()) {
            salida += " (elegida)";
        }
        salida += "\n";
    }
    if (!error().empty()) {
        salida += "      " + error() + "\n";
    }
    return salida;
}

int Buscador::coincidenciaEnFila(int y, int filaInicial) const {
    const int indice = y - filaInicial;
    if (indice < 0 || indice >= cantidadDeLineas()) {
        return -1;
    }
    return indice;
}

void Buscador::definirFilaInicial(int fila) {
    _filaInicial = fila;
}

int Buscador::filaInicial() const {
    return _filaInicial;
}

// ---------------------------------------------------------------------------
// Los eventos, segun donde esta el foco
// ---------------------------------------------------------------------------

bool Buscador::aplicarEventoEnElTexto(const Evento& evento) {
    // EL ENTER BUSCA Y PASA EL FOCO A LA LISTA
    //
    // La busqueda corre AL CONFIRMAR, no en cada tecla: mientras se escribe
    // la lista no cambia, y la numeracion posicional del `Selector` queda
    // estable (el por que esta en el header del widget).
    if (evento.tipo == TipoTecla::Enter) {
        _campo.aplicarEvento(evento);
        buscar();
        return true;
    }

    // EL RESTO LO MANEJA EL CAMPO
    //
    // Los caracteres, el espacio, el Retroceso y las flechas del cursor son
    // del TEXTO: el `Campo` los consume. El `Escape` no lo consume (es de la
    // pantalla).
    return _campo.aplicarEvento(evento);
}

bool Buscador::aplicarEventoEnLaLista(const Evento& evento) {
    // EL RETROCESO VUELVE AL TEXTO
    //
    // El `Selector` usa el Retroceso para borrar el numero que se esta
    // escribiendo, pero en el buscador lo que el usuario quiere corregir es
    // el TEXTO de la busqueda (el numero se corrige escribiendo otro).
    if (evento.tipo == TipoTecla::Retroceso) {
        _focoEnElTexto = true;
        return true;
    }

    if (!_selector.aplicarEvento(evento)) {
        return false;
    }

    // EL ENTER ELIGE
    //
    // Con un numero escrito que no existe, el `Selector` deja el error y el
    // numero en la pantalla: el usuario lo corrige. Sin error, la eleccion
    // es valida.
    if (evento.tipo == TipoTecla::Enter && _selector.error().empty()) {
        elegir();
    }
    return true;
}

// ---------------------------------------------------------------------------
// La busqueda y la eleccion
// ---------------------------------------------------------------------------

void Buscador::buscar() {
    llenarLaLista(_campo.texto());

    if (_coincidencias.empty()) {
        // NO HAY NADA QUE ELEGIR
        //
        // La lista queda vacia y el foco VUELVE al texto: el usuario tiene
        // que corregir lo que escribio, no elegir de una lista vacia.
        _error = SIN_COINCIDENCIAS;
        _focoEnElTexto = true;
        return;
    }

    _error.clear();
    _focoEnElTexto = false;
}

void Buscador::llenarLaLista(const std::string& texto) {
    _coincidencias = _buscar ? _buscar(texto) : std::vector<Buscador::Coincidencia>();

    // CADA COINCIDENCIA ES UNA OPCION DEL SELECTOR
    //
    // La linea de la opcion son las celdas de la coincidencia: el `Selector`
    // le pone el numero posicional adelante.
    std::vector<std::string> opciones;
    opciones.reserve(_coincidencias.size());
    for (const Buscador::Coincidencia& coincidencia : _coincidencias) {
        opciones.push_back(unirCeldas(coincidencia.second));
    }
    _selector.setOpciones(std::move(opciones));

    // LA ELECCION SE RESETEA
    //
    // La busqueda nueva invalida la eleccion anterior: el id que estaba
    // elegido era de una lista que ya no esta.
    _elegido = false;
    _idElegido = -1;
}

void Buscador::elegir() {
    const int indice = _selector.seleccion();
    if (indice < 0 || indice >= static_cast<int>(_coincidencias.size())) {
        // No deberia pasar: la lista tiene las mismas coincidencias que el
        // `Selector`.
        return;
    }

    // EL ID ES EL DE LA COINCIDENCIA
    //
    // No el de la posicion: el id viaja con la fila (el por que esta en
    // `include/ui/Buscador.h`).
    _idElegido = _coincidencias[static_cast<std::size_t>(indice)].first;
    _elegido = true;
}

std::string Buscador::unirCeldas(const std::vector<std::string>& celdas) {
    std::string linea;
    for (std::size_t i = 0; i < celdas.size(); ++i) {
        if (i > 0) {
            linea += SEPARADOR;
        }
        linea += celdas[i];
    }
    return linea;
}

} // namespace ui
