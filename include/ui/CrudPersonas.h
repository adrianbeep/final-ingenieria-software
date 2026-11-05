#pragma once

#include "repositories/IRepositorio.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "ui/Aplicacion.h"
#include "ui/Campo.h"
#include "ui/ColorFtxui.h"
#include "ui/Confirmacion.h"
#include "ui/Menu.h"
#include "ui/Mensaje.h"
#include "ui/Pantalla.h"
#include "ui/Tabla.h"
#include "ui/Theme.h"
#include "utils/Utils.h"

#include <ftxui.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// El motor CRUD de personas: la pantalla que sabe registrar, modificar,
// dar de baja, listar, buscar y mostrar la ficha de un ALUMNO o de un
// PROFESOR.
//
// POR QUE UN TEMPLATE Y NO DOS PANTALLAS
//
// El flujo de un alumno y el de un profesor es EL MISMO: las mismas vistas,
// las mismas reglas de navegacion, los mismos mensajes. Lo UNICO que cambia
// son los datos (las columnas, los campos, el constructor y los criterios de
// busqueda). Es el mismo criterio que usa el proyecto en la capa de dominio
// (`RepositorioCRUD<T>`, `ArchivoTexto<T>`): si el flujo es el mismo, la
// logica se escribe UNA vez.
//
// LA LOGICA Y EL DIBUJO ESTAN SEPARADOS
//
// La maquina de estados (que vista esta, que se muestra, que transicion se
// dispara) es LOGICA PURA: se testea sin terminal, inyectando eventos. El
// `renderizar()` solo traduce la vista actual a un `ftxui::Element`: no
// decide nada.
//
// EL '0' ES VOLVER, EN TODOS LOS NIVELES
//
// Es la regla de UX del proyecto. En las vistas con un CAMPO de texto el '0'
// puede ser un DATO (un id puede ser 10), asi que ahi el '0' solo es
// "volver" cuando el campo esta VACIO: si hay algo escrito, el '0' es parte
// del dato.
//
// EL 'Escape' SUBE UN NIVEL DENTRO DEL MOTOR
//
// El motor consume el `Escape` en sus vistas internas: desde la vista del id
// se vuelve a su menu, desde el formulario se vuelve a su menu, y desde la
// confirmacion se vuelve a la vista del id. En el menu CRUD el `Escape` NO
// se consume: ahi lo maneja la aplicacion, que saca la pantalla entera de la
// pila (y vuelve al menu principal).
//
// O sea: los dos caminos de vuelta tienen alcances DISTINTOS y CONOCIDOS. El
// '0' sube un nivel del motor; el `Escape` sube un nivel del motor y, en su
// menu, sale de la pantalla.
// ---------------------------------------------------------------------------
namespace ui {

// Un campo del formulario: etiqueta, valor inicial y como se valida.
struct CampoFormulario {
    std::string etiqueta;
    std::string valorInicial;
    Campo::Validador validador;
};

// Los turnos de la persona, para la ficha.
//
// SON FILAS Y NO ENTIDADES
//
// Los turnos de un alumno son `Turno`, no `Alumno`: el motor es generico
// sobre el tipo de la persona y no tiene por que saber como se ve un turno.
// Por eso el que decide es el que arma la CONFIGURACION: devuelve las
// filas ya formateadas, con el orden de `cabeceras`.
struct TurnosDePersona {
    std::vector<std::string> cabeceras;
    std::function<std::vector<std::vector<std::string>>(int)> vigentes;
    std::function<std::vector<std::vector<std::string>>(int)> historial;
};

// Un criterio de busqueda: como se llama y que devuelve.
//
// DEVUELVE LAS FILAS
//
// El resultado de una busqueda son VARIAS filas (una por coincidencia), y
// cada fila son las celdas de las columnas. Por eso el tipo es
// `vector<vector<string>>`: el motor las mete en una `ui::Tabla` tal cual.
struct CriterioBusqueda {
    std::string etiqueta;
    std::function<std::vector<std::vector<std::string>>(const std::string&)> buscar;
};

// El texto en mayusculas.
//
// SOLO ASCII: es para los titulos ("ALUMNOS", "PROFESORES"), que son
// nombres de entidad y no texto de usuario. Por eso no hace falta
// decodificar UTF-8.
inline std::string aMayusculas(const std::string& texto) {
    std::string salida = texto;
    for (char& caracter : salida) {
        if (caracter >= 'a' && caracter <= 'z') {
            caracter = static_cast<char>(caracter - 'a' + 'A');
        }
    }
    return salida;
}

// El evento de la tecla Enter.
//
// Lo arma el motor para que un `Campo` valide sin que el usuario tenga que
// apretar una tecla de verdad: el `Campo` valida al recibir Enter.
inline Evento eventoEnter() {
    Evento evento;
    evento.tipo = TipoTecla::Enter;
    return evento;
}

// Convierte un texto en un id. Devuelve -1 si el texto NO es un entero
// valido (o si esta fuera del rango de un id).
//
// LA CONVERSION NO LANZA
//
// `std::stoi` lanza `std::out_of_range` con un numero enorme, y esa
// excepcion NO es del sistema: se escaparia sin archivo ni linea. Por eso
// el texto se valida ANTES de convertir, y la conversion va en un `try`.
inline int idDesdeTexto(const std::string& texto) {
    if (!Utils::esEntero(texto)) {
        return -1;
    }
    try {
        const long long valor = std::stoll(texto);
        if (valor < 1 || valor > 1000000000LL) {
            return -1;
        }
        return static_cast<int>(valor);
    } catch (...) {
        return -1;
    }
}

template <class T>
class CrudPersonas : public Pantalla {
public:
    // La fila de una entidad: las celdas, en el orden de las cabeceras.
    using Fila = std::function<std::vector<std::string>(const T&)>;

    // TODO lo que cambia entre un alumno y un profesor.
    struct Config {
        std::string singular;                        // "alumno"
        std::string plural;                          // "alumnos"
        std::vector<std::string> cabeceras;          // las columnas de la tabla
        Fila fila;                                   // la fila de una entidad
        std::vector<CampoFormulario> campos;         // el formulario
        std::function<T(const std::vector<std::string>&)> construir;  // arma la entidad
        std::vector<CriterioBusqueda> criterios;     // los criterios de busqueda
        std::function<bool(const T&)> esActivo;      // esta activa?
        TurnosDePersona turnos;                      // los turnos de la ficha
    };

    CrudPersonas(Aplicacion& app, IRepositorio<T>& repositorio,
                 IRepositorioBajaLogica<T>& bajaLogica, Config config);

    // De Pantalla:
    std::string titulo() const override;
    ftxui::Element renderizar() override;
    bool manejar(const Evento& evento) override;
    std::string textoPlano() const override;

    // Para los tests: en que vista esta y que se ve.
    const std::string& vista() const;
    std::string vistaTextoPlano() const;

private:
    // LAS VISTAS DE LA MAQUINA DE ESTADOS
    //
    // El nombre de cada vista es el que ve el test (`vista()`): por eso son
    // palabras y no numeros, y por eso hay una funcion que las traduce.
    enum class Vista {
        Menu,
        Formulario,
        Id,
        Confirmar,
        Listado,
        Criterio,
        Valor,
        Resultados,
        Ficha
    };

    // Que hay que hacer cuando se confirme el id.
    enum class Accion { Modificar, Baja, Ficha };

    // Si el formulario esta registrando o modificando.
    enum class ModoFormulario { Registro, Modificacion };

    // La fila de la pantalla donde se dibuja la PRIMERA opcion de la
    // confirmacion (la del "Si").
    //
    // La `ui::Confirmacion` NO expone la fila en la que se dibujan sus
    // opciones, asi que su hit-testing no sirve: el motor dibuja las dos
    // opciones en dos filas conocidas y traduce el clic al atajo
    // correspondiente. Sin esto, un clic en el ENCABEZADO caeria en la
    // primera opcion y confirmaria una baja sin querer.
    static constexpr int FILA_INICIAL_CONFIRMACION = 3;

    static const std::string& nombreDeVista(Vista vista);

    // --- La navegacion ---
    void volverAtras();
    void irA(Vista vista);
    bool esTeclaVolver(const Evento& evento) const;
    bool esTeclaEscape(const Evento& evento) const;
    bool esTeclaVolverConCampoVacio(const Evento& evento, const Campo& campo) const;

    // --- Los manejadores de cada vista ---
    bool manejarMenu(const Evento& evento);
    bool manejarFormulario(const Evento& evento);
    bool manejarCampo(const Evento& evento, const std::unique_ptr<Campo>& campo);
    bool manejarConfirmacion(const Evento& evento);
    bool manejarMenuCriterios(const Evento& evento);
    bool manejarLectura(const Evento& evento);

    // --- El menu CRUD ---
    void iniciarRegistro();
    void iniciarModificacion();
    void iniciarBaja();
    void iniciarFicha();

    // --- Los formularios ---
    void abrirFormulario(std::vector<std::unique_ptr<Campo>> campos);
    void confirmarFormulario();
    void moverFoco(int delta);

    // --- El id ---
    void abrirCampoId(Accion accion);
    Campo::Validador validadorDeId();
    void confirmarId();

    // --- La baja y la reactivation ---
    std::string preguntaDeBaja(const T& entidad) const;
    void ejecutarBaja();

    // --- La busqueda ---
    void abrirCriterio();
    void iniciarBusqueda(std::size_t indice);
    void confirmarValor();

    // --- Los mensajes ---
    void avisar(const std::string& texto);
    void avisarExito(const std::string& texto);
    const Mensaje& mensajeActual() const;
    std::string singularConMayuscula() const;

    // --- El dibujo ---
    ftxui::Element encabezado() const;
    ftxui::Element lineaOpcion(const Menu& menu, int indice) const;
    ftxui::Element lineaCampo(const Campo& campo, bool enfocado) const;
    ftxui::Element lineaError(const std::string& texto) const;
    ftxui::Element lineaMensaje() const;
    ftxui::Element lineaFormulario() const;
    ftxui::Element lineaConfirmacion() const;
    ftxui::Element lineaListado() const;
    ftxui::Element lineaResultados() const;
    ftxui::Element lineaFicha() const;
    ftxui::Element tablaDe(const std::vector<std::vector<std::string>>& filas,
                           const std::vector<std::string>& cabeceras) const;
    std::string ayuda() const;
    std::string vacio(const std::string& queFalta) const;

    // --- El texto plano (lo que se ve sin terminal) ---
    std::string textoDeLaVista() const;
    static std::string unirFila(const std::vector<std::string>& fila);

    // El indice de la columna con ese nombre, o -1 si no existe.
    //
    // LA MODIFICACION SE CARGA POR NOMBRE DE COLUMNA
    //
    // El formulario de modificar arranca con los valores de la entidad que
    // se esta editando. Esos valores salen de la FILA de la entidad, y la
    // fila y las cabeceras tienen las columnas en el mismo orden: el campo
    // del formulario se busca por su ETIQUETA, que es el nombre de la
    // columna. Si el nombre no existe, se deja el valor inicial de la
    // configuracion (nunca se borra un dato por un nombre que no calza).
    int indiceDeColumna(const std::string& nombre) const;

    Aplicacion& _aplicacion;
    IRepositorio<T>& _repositorio;
    IRepositorioBajaLogica<T>& _bajaLogica;
    Config _config;
    Theme _tema;
    std::string _titulo;
    Menu _menu;
    Menu _menuCriterios;

    Vista _vista = Vista::Menu;
    ModoFormulario _modoFormulario = ModoFormulario::Registro;
    Accion _accion = Accion::Modificar;

    // Los campos del formulario y el que tiene el foco.
    std::vector<std::unique_ptr<Campo>> _campos;
    std::size_t _indiceCampo = 0;

    // El campo del id y el del valor de busqueda.
    std::unique_ptr<Campo> _campoId;
    std::unique_ptr<Campo> _campoValor;

    // La confirmacion de la baja.
    std::unique_ptr<Confirmacion> _confirmacion;

    // La entidad de la vista actual (la que se modifica, la que se da de
    // baja o la de la ficha).
    std::unique_ptr<T> _entidad;
    int _id = -1;

    // El criterio de busqueda elegido y las filas del resultado.
    std::size_t _criterioElegido = 0;
    std::vector<std::vector<std::string>> _filasResultado;
};

// ---------------------------------------------------------------------------
// Implementaciones (van en el header: es un template).
// ---------------------------------------------------------------------------

template <class T>
CrudPersonas<T>::CrudPersonas(Aplicacion& app, IRepositorio<T>& repositorio,
                               IRepositorioBajaLogica<T>& bajaLogica, Config config)
    : _aplicacion(app),
      _repositorio(repositorio),
      _bajaLogica(bajaLogica),
      _config(std::move(config)),
      _titulo(aMayusculas(_config.plural)),
      _menu(_titulo),
      _menuCriterios("Buscar " + _config.plural) {
    // EL MENU CRUD
    //
    // El atajo es el numero que se escribe (decision D28) y el '0' es
    // "Volver" en todos los niveles.
    _menu.agregar('1', "Registrar", [this]() { iniciarRegistro(); });
    _menu.agregar('2', "Modificar", [this]() { iniciarModificacion(); });
    _menu.agregar('3', "Baja / Reactivar", [this]() { iniciarBaja(); });
    _menu.agregar('4', "Listar todos", [this]() { irA(Vista::Listado); });
    _menu.agregar('5', "Buscar", [this]() { abrirCriterio(); });
    _menu.agregar('6', "Ver ficha + historial", [this]() { iniciarFicha(); });
    _menu.agregar('0', "Volver", [this]() { volverAtras(); });

    // EL MENU DE CRITERIOS
    //
    // Los criterios salen de la configuracion, asi que sus atajos se arman
    // aca. El '9' es el tope: un menu con mas atajos que digitos no tendria
    // forma de elegir sus opciones.
    for (std::size_t i = 0; i < _config.criterios.size() && i < 9; ++i) {
        _menuCriterios.agregar(static_cast<char>('1' + static_cast<int>(i)),
                                _config.criterios[i].etiqueta,
                                [this, i]() { iniciarBusqueda(i); });
    }
    _menuCriterios.agregar('0', "Volver", [this]() { volverAtras(); });
}

template <class T>
std::string CrudPersonas<T>::titulo() const {
    return _titulo;
}

template <class T>
const std::string& CrudPersonas<T>::vista() const {
    return nombreDeVista(_vista);
}

template <class T>
const std::string& CrudPersonas<T>::nombreDeVista(Vista vista) {
    // Los nombres son ESTATICOS: son los mismos para todas las
    // instanciaciones del template y la referencia que se devuelve tiene que
    // vivir mas que la llamada.
    static const std::string menu = "Menu";
    static const std::string formulario = "Formulario";
    static const std::string id = "Id";
    static const std::string confirmacion = "Confirmar";
    static const std::string listado = "Listado";
    static const std::string criterio = "Criterio";
    static const std::string valor = "Valor";
    static const std::string resultados = "Resultados";
    static const std::string ficha = "Ficha";

    switch (vista) {
        case Vista::Menu:
            return menu;
        case Vista::Formulario:
            return formulario;
        case Vista::Id:
            return id;
        case Vista::Confirmar:
            return confirmacion;
        case Vista::Listado:
            return listado;
        case Vista::Criterio:
            return criterio;
        case Vista::Valor:
            return valor;
        case Vista::Resultados:
            return resultados;
        case Vista::Ficha:
            return ficha;
    }
    return menu;
}

// ---------------------------------------------------------------------------
// La navegacion
// ---------------------------------------------------------------------------

template <class T>
bool CrudPersonas<T>::esTeclaVolver(const Evento& evento) const {
    return evento.tipo == TipoTecla::Caracter && evento.caracter == "0";
}

// LA TECLA ESCAPE
//
// Es la que la aplicacion usa como "volver" en el nivel de la pila. El motor
// la CONSUME en sus vistas internas (para subir un nivel) y la deja pasar en
// su menu (para que la aplicacion saque la pantalla entera).
template <class T>
bool CrudPersonas<T>::esTeclaEscape(const Evento& evento) const {
    return evento.tipo == TipoTecla::Escape;
}

// EL '0' ES "VOLVER" SOLO SI EL CAMPO ESTA VACIO
//
// Con algo escrito el '0' es un DATO (un telefono puede tener un cero, una
// matricula puede empezar con un cero) y se lo queda el campo.
//
// LA MISMA REGLA EN TODAS LAS VISTAS CON CAMPO
//
// La regla vive ACA y no copiada en cada manejador: asi la vista del `Id`,
// la del `Valor` y la del `Formulario` la aplican EXACTAMENTE igual, y no
// pueden desincronizarse entre si.
template <class T>
bool CrudPersonas<T>::esTeclaVolverConCampoVacio(const Evento& evento,
                                                   const Campo& campo) const {
    return esTeclaVolver(evento) && campo.texto().empty();
}

template <class T>
void CrudPersonas<T>::irA(Vista vista) {
    _vista = vista;
}

template <class T>
void CrudPersonas<T>::volverAtras() {
    // EL "VOLVER" TIENE TRES DESTINOS
    //
    //   - desde el menu CRUD, se sale de la pantalla (el menu anterior es el
    //     menu principal, que esta en la pila de la aplicacion);
    //   - desde la confirmacion, se vuelve a la vista ANTERIOR (la del id),
    //     que es lo que dice la especificacion;
    //   - desde cualquier otra vista, se vuelve al menu CRUD.
    switch (_vista) {
        case Vista::Menu:
            _aplicacion.volver();
            return;
        case Vista::Confirmar:
            irA(Vista::Id);
            return;
        default:
            irA(Vista::Menu);
            return;
    }
}

// ---------------------------------------------------------------------------
// Los manejadores de cada vista
// ---------------------------------------------------------------------------

template <class T>
bool CrudPersonas<T>::manejar(const Evento& evento) {
    switch (_vista) {
        case Vista::Menu:
            return manejarMenu(evento);
        case Vista::Formulario:
            return manejarFormulario(evento);
        case Vista::Id:
            return manejarCampo(evento, _campoId);
        case Vista::Confirmar:
            return manejarConfirmacion(evento);
        case Vista::Criterio:
            return manejarMenuCriterios(evento);
        case Vista::Valor:
            return manejarCampo(evento, _campoValor);
        case Vista::Listado:
        case Vista::Resultados:
        case Vista::Ficha:
            return manejarLectura(evento);
    }
    return false;
}

template <class T>
bool CrudPersonas<T>::manejarMenu(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` NO SE CONSUME EN EL MENU CRUD
    //
    // El menu CRUD es el NIVEL MAS ALTO de esta pantalla: desde ahi el
    // `Escape` tiene que sacar la pantalla ENTERA de la pila, y eso lo hace
    // la aplicacion. Si el motor lo consumiera, el efecto seria el mismo
    // pero quedaria repartido en dos lugares.
    return _menu.aplicarEvento(evento);
}

template <class T>
bool CrudPersonas<T>::manejarMenuCriterios(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    return _menuCriterios.aplicarEvento(evento);
}

template <class T>
bool CrudPersonas<T>::manejarFormulario(const Evento& evento) {
    if (_campos.empty()) {
        return false;
    }

    // EL FOCO SE MUEVE CON EL TAB (y con las flechas arriba y abajo)
    //
    // En un formulario con varios campos hay que poder pasar de uno a otro:
    // el Tabulador es la tecla que se usa para eso, y las flechas verticales
    // hacen lo mismo para quien no las usa.
    if (evento.tipo == TipoTecla::Tabulador) {
        moverFoco(1);
        return true;
    }
    if (evento.tipo == TipoTecla::FlechaArriba) {
        moverFoco(-1);
        return true;
    }
    if (evento.tipo == TipoTecla::FlechaAbajo) {
        moverFoco(1);
        return true;
    }

    // ENTER CONFIRMA EL FORMULARIO ENTERO
    //
    // No solo el campo que tiene el foco: un formulario con un campo
    // invalido no se puede confirmar.
    if (evento.tipo == TipoTecla::Enter) {
        confirmarFormulario();
        return true;
    }

    // EL `Escape` SUBE UN NIVEL
    //
    // El motor consume el `Escape` en sus vistas internas: desde el
    // formulario se vuelve al menu CRUD. El `Escape` es una tecla de
    // navegacion, no un dato: se atiende aunque el campo tenga texto
    // escrito.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }

    // EL '0' ES "VOLVER" SOLO SI EL CAMPO ESTA VACIO
    //
    // Con algo escrito el '0' es un dato (un telefono puede tener un cero) y
    // se lo queda el campo.
    if (esTeclaVolverConCampoVacio(evento, *_campos[_indiceCampo])) {
        volverAtras();
        return true;
    }

    return _campos[_indiceCampo]->aplicarEvento(evento);
}

template <class T>
bool CrudPersonas<T>::manejarCampo(const Evento& evento, const std::unique_ptr<Campo>& campo) {
    if (!campo) {
        return false;
    }

    if (evento.tipo == TipoTecla::Enter) {
        // El campo valido y, si el valor sirve, se confirma la vista.
        campo->aplicarEvento(evento);
        if (!campo->confirmado()) {
            // El campo ya mostro el error debajo: no se sigue adelante.
            return true;
        }
        if (_vista == Vista::Id) {
            confirmarId();
        } else {
            confirmarValor();
        }
        return true;
    }

    // EL `Escape` SUBE UN NIVEL
    //
    // El motor consume el `Escape` en sus vistas internas: desde la vista del
    // id o del valor se vuelve al menu CRUD. El `Escape` es una tecla de
    // navegacion, no un dato: se atiende aunque el campo tenga texto
    // escrito.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }

    // EL '0' ES "VOLVER" SOLO SI EL CAMPO ESTA VACIO
    //
    // Con algo escrito el '0' es un dato (un id puede ser 10) y se lo queda
    // el campo.
    if (esTeclaVolverConCampoVacio(evento, *campo)) {
        volverAtras();
        return true;
    }

    return campo->aplicarEvento(evento);
}

template <class T>
bool CrudPersonas<T>::manejarConfirmacion(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE A LA VISTA DEL ID
    //
    // `volverAtras()` desde la confirmacion vuelve a la vista anterior (la
    // del id), que es lo que dice la especificacion.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    if (!_confirmacion) {
        return false;
    }

    // EL RATON LO TRADUCE EL MOTOR
    //
    // La `ui::Confirmacion` no sabe en que fila se dibujan sus opciones (no
    // expone su menu), asi que un clic caeria siempre en la primera: un
    // clic en el encabezado confirmaria una baja sin querer. El motor sabe
    // donde las dibuja, asi que el clic se traduce al atajo.
    Evento efectivo = evento;
    if (evento.esMouse()) {
        if (!evento.esClic()) {
            return false;
        }
        const int indice = evento.y - FILA_INICIAL_CONFIRMACION;
        if (indice < 0 || indice > 1) {
            // El clic cae fuera de las opciones: no se consume.
            return false;
        }
        Evento atajo;
        atajo.tipo = TipoTecla::Caracter;
        atajo.caracter = indice == 0 ? "1" : "2";
        efectivo = atajo;
    }

    if (!_confirmacion->aplicarEvento(efectivo)) {
        return false;
    }

    if (_confirmacion->confirmo()) {
        ejecutarBaja();
    } else {
        // Si NO confirma, se vuelve a la vista anterior (la del id).
        irA(Vista::Id);
    }
    return true;
}

template <class T>
bool CrudPersonas<T>::manejarLectura(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE AL MENU DEL MOTOR
    //
    // Las vistas de solo lectura (el listado, los resultados y la ficha) no
    // tienen ningun campo: el `Escape` sube un nivel, al menu CRUD.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    // Las vistas de solo lectura no consumen nada mas.
    return false;
}

// ---------------------------------------------------------------------------
// El menu CRUD
// ---------------------------------------------------------------------------

template <class T>
void CrudPersonas<T>::iniciarRegistro() {
    _modoFormulario = ModoFormulario::Registro;
    _entidad.reset();

    std::vector<std::unique_ptr<Campo>> campos;
    for (const CampoFormulario& campo : _config.campos) {
        campos.push_back(
            std::make_unique<Campo>(campo.etiqueta, campo.valorInicial, campo.validador));
    }
    abrirFormulario(std::move(campos));
}

template <class T>
void CrudPersonas<T>::iniciarModificacion() {
    abrirCampoId(Accion::Modificar);
}

template <class T>
void CrudPersonas<T>::iniciarBaja() {
    abrirCampoId(Accion::Baja);
}

template <class T>
void CrudPersonas<T>::iniciarFicha() {
    abrirCampoId(Accion::Ficha);
}

template <class T>
void CrudPersonas<T>::abrirCampoId(Accion accion) {
    _accion = accion;
    _campoId = std::make_unique<Campo>("Id", "", validadorDeId());
    irA(Vista::Id);
}

// ---------------------------------------------------------------------------
// El formulario
// ---------------------------------------------------------------------------

template <class T>
void CrudPersonas<T>::abrirFormulario(std::vector<std::unique_ptr<Campo>> campos) {
    _campos = std::move(campos);
    _indiceCampo = 0;
    irA(Vista::Formulario);
}

template <class T>
void CrudPersonas<T>::moverFoco(int delta) {
    if (_campos.empty()) {
        return;
    }
    const int cantidad = static_cast<int>(_campos.size());
    int indice = static_cast<int>(_indiceCampo) + delta;
    // El foco DA LA VUELTA: con un formulario largo, volver al principio es
    // tan natural como avanzar.
    while (indice < 0) {
        indice += cantidad;
    }
    _indiceCampo = static_cast<std::size_t>(indice % cantidad);
}

template <class T>
void CrudPersonas<T>::confirmarFormulario() {
    // VALIDAR TODOS LOS CAMPOS
    //
    // El `Campo` valida al recibir Enter: guarda el error (que se muestra
    // DEBAJO del campo, sin borrar lo que el usuario escribio) y cuenta el
    // intento. Si el error se repite tres veces, el campo lanza
    // `DatoInvalidoException` y la excepcion sube hasta el `catch` global.
    //
    // Se validan TODOS y no solo el que tiene el foco: un formulario con un
    // campo invalido no se puede confirmar.
    for (std::size_t i = 0; i < _campos.size(); ++i) {
        _campos[i]->aplicarEvento(eventoEnter());
        if (!_campos[i]->error().empty()) {
            // El foco va al campo que fallo: el usuario ve donde esta el
            // problema sin tener que buscarlo.
            _indiceCampo = i;
            return;
        }
    }

    // ARMAR LA ENTIDAD
    //
    // El vector que recibe `construir` es [id, campo1, campo2, ...]: el id
    // va adelante porque lo PROPONE el repositorio (o es el id de la entidad
    // que se esta modificando) y nunca lo escribe el usuario.
    const int id =
        _modoFormulario == ModoFormulario::Registro ? _repositorio.proximoId() : _id;

    std::vector<std::string> valores;
    valores.reserve(_campos.size() + 1);
    valores.push_back(std::to_string(id));
    for (const std::unique_ptr<Campo>& campo : _campos) {
        valores.push_back(campo->valor());
    }

    try {
        const T entidad = _config.construir(valores);
        if (_modoFormulario == ModoFormulario::Registro) {
            _repositorio.alta(entidad);
            avisarExito(singularConMayuscula() + " #" + std::to_string(entidad.getId())
                        + " registrado.");
        } else {
            _repositorio.modificar(entidad);
            avisarExito(singularConMayuscula() + " #" + std::to_string(entidad.getId())
                        + " modificado.");
        }
        irA(Vista::Menu);
    } catch (const DatoInvalidoException& error) {
        // El repositorio puede rechazar la entidad (por ejemplo, la
        // matricula ya la tiene otro). El error se muestra y el formulario
        // sigue abierto: no se pierde lo que el usuario escribio.
        avisar(error.motivo());
    } catch (const ExcepcionSistema& error) {
        avisar(error.what());
    }
}

// ---------------------------------------------------------------------------
// El id
// ---------------------------------------------------------------------------

template <class T>
Campo::Validador CrudPersonas<T>::validadorDeId() {
    return [this](const std::string& texto) -> std::string {
        const int id = idDesdeTexto(texto);
        if (id < 0) {
            return "el id tiene que ser un numero entero mayor que cero";
        }
        T entidad;
        if (!_repositorio.buscarPorId(id, entidad)) {
            return "no existe " + _config.singular + " #" + texto;
        }
        return "";
    };
}

template <class T>
void CrudPersonas<T>::confirmarId() {
    const int id = idDesdeTexto(_campoId->valor());
    T entidad;
    if (id < 0 || !_repositorio.buscarPorId(id, entidad)) {
        // No deberia pasar: el validador del campo ya lo comprobo.
        avisar("no existe " + _config.singular + " #" + _campoId->texto());
        irA(Vista::Menu);
        return;
    }

    _id = id;
    _entidad = std::make_unique<T>(entidad);

    switch (_accion) {
        case Accion::Modificar: {
            // EL FORMULARIO DE MODIFICAR ARRANCA CON LOS DATOS VIEJOS
            //
            // Los valores salen de la fila de la entidad: el campo del
            // formulario se busca por su etiqueta (que es el nombre de la
            // columna).
            const std::vector<std::string> fila = _config.fila(entidad);
            std::vector<std::unique_ptr<Campo>> campos;
            for (const CampoFormulario& campo : _config.campos) {
                std::string valor = campo.valorInicial;
                const int columna = indiceDeColumna(campo.etiqueta);
                if (columna >= 0 && static_cast<std::size_t>(columna) < fila.size()) {
                    valor = fila[static_cast<std::size_t>(columna)];
                }
                campos.push_back(
                    std::make_unique<Campo>(campo.etiqueta, valor, campo.validador));
            }
            _modoFormulario = ModoFormulario::Modificacion;
            abrirFormulario(std::move(campos));
            return;
        }
        case Accion::Baja:
            _confirmacion = std::make_unique<Confirmacion>(preguntaDeBaja(entidad));
            irA(Vista::Confirmar);
            return;
        case Accion::Ficha:
            irA(Vista::Ficha);
            return;
    }
}

// ---------------------------------------------------------------------------
// La baja y la reactivation
// ---------------------------------------------------------------------------

template <class T>
std::string CrudPersonas<T>::preguntaDeBaja(const T& entidad) const {
    const bool activa = _config.esActivo ? _config.esActivo(entidad) : true;
    if (activa) {
        return "Dar de baja al " + _config.singular + " #"
            + std::to_string(entidad.getId()) + "?";
    }
    return "Reactivar al " + _config.singular + " #" + std::to_string(entidad.getId()) + "?";
}

template <class T>
void CrudPersonas<T>::ejecutarBaja() {
    if (!_entidad) {
        irA(Vista::Menu);
        return;
    }

    const bool activa = _config.esActivo ? _config.esActivo(*_entidad) : true;
    try {
        if (activa) {
            _bajaLogica.bajaLogica(_id);
            avisarExito(singularConMayuscula() + " #" + std::to_string(_id)
                        + " dado de baja.");
        } else {
            _bajaLogica.reactivar(_id);
            avisarExito(singularConMayuscula() + " #" + std::to_string(_id) + " reactivado.");
        }
    } catch (const ExcepcionSistema& error) {
        avisar(error.what());
    }
    irA(Vista::Menu);
}

// ---------------------------------------------------------------------------
// La busqueda
// ---------------------------------------------------------------------------

template <class T>
void CrudPersonas<T>::abrirCriterio() {
    irA(Vista::Criterio);
}

template <class T>
void CrudPersonas<T>::iniciarBusqueda(std::size_t indice) {
    _criterioElegido = indice;
    _campoValor = std::make_unique<Campo>(
        "Valor a buscar", "",
        [](const std::string& texto) -> std::string {
            if (!Utils::esNoVacio(texto)) {
                return "el valor a buscar no puede estar vacio";
            }
            return "";
        });
    irA(Vista::Valor);
}

template <class T>
void CrudPersonas<T>::confirmarValor() {
    const std::string valor = _campoValor->valor();
    const CriterioBusqueda& criterio = _config.criterios[_criterioElegido];
    _filasResultado =
        criterio.buscar ? criterio.buscar(valor) : std::vector<std::vector<std::string>>();
    irA(Vista::Resultados);
}

// ---------------------------------------------------------------------------
// Los mensajes
// ---------------------------------------------------------------------------

template <class T>
void CrudPersonas<T>::avisar(const std::string& texto) {
    // El mensaje vive en el CONTEXTO: es lo que la pantalla anterior va a
    // mostrar cuando el usuario vuelva.
    _aplicacion.contexto().mostrarMensaje(Mensaje::error(texto));
}

template <class T>
void CrudPersonas<T>::avisarExito(const std::string& texto) {
    _aplicacion.contexto().mostrarMensaje(Mensaje::exito(texto));
}

template <class T>
const Mensaje& CrudPersonas<T>::mensajeActual() const {
    return _aplicacion.contexto().mensaje();
}

template <class T>
std::string CrudPersonas<T>::singularConMayuscula() const {
    std::string singular = _config.singular;
    if (!singular.empty()) {
        singular[0] = static_cast<char>(singular[0] - 'a' + 'A');
    }
    return singular;
}

template <class T>
int CrudPersonas<T>::indiceDeColumna(const std::string& nombre) const {
    for (std::size_t i = 0; i < _config.cabeceras.size(); ++i) {
        if (_config.cabeceras[i] == nombre) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

// ---------------------------------------------------------------------------
// El dibujo
// ---------------------------------------------------------------------------

template <class T>
ftxui::Element CrudPersonas<T>::renderizar() {
    ftxui::Elements lineas;

    lineas.push_back(encabezado());
    lineas.push_back(ftxui::separator());

    switch (_vista) {
        case Vista::Menu:
            // La opcion `i` del menu queda en la fila `filaInicial + i`: el
            // hit-testing del raton lo necesita.
            _menu.definirFilaInicial(static_cast<int>(lineas.size()));
            for (int i = 0; i < _menu.cantidad(); ++i) {
                lineas.push_back(lineaOpcion(_menu, i));
            }
            lineas.push_back(ftxui::separator());
            break;
        case Vista::Criterio:
            _menuCriterios.definirFilaInicial(static_cast<int>(lineas.size()));
            for (int i = 0; i < _menuCriterios.cantidad(); ++i) {
                lineas.push_back(lineaOpcion(_menuCriterios, i));
            }
            lineas.push_back(ftxui::separator());
            break;
        case Vista::Formulario:
            lineas.push_back(lineaFormulario());
            break;
        case Vista::Id:
            lineas.push_back(lineaCampo(*_campoId, true));
            break;
        case Vista::Valor:
            lineas.push_back(lineaCampo(*_campoValor, true));
            break;
        case Vista::Confirmar:
            lineas.push_back(lineaConfirmacion());
            break;
        case Vista::Listado:
            lineas.push_back(lineaListado());
            break;
        case Vista::Resultados:
            lineas.push_back(lineaResultados());
            break;
        case Vista::Ficha:
            lineas.push_back(lineaFicha());
            break;
    }

    if (!mensajeActual().esVacio()) {
        lineas.push_back(lineaMensaje());
    }
    lineas.push_back(ftxui::text(ayuda()));

    return ftxui::vbox(std::move(lineas));
}

template <class T>
ftxui::Element CrudPersonas<T>::encabezado() const {
    ftxui::Element elemento = ftxui::text(_aplicacion.titulo() + " - " + _titulo);
    if (const auto color = _tema.colorDe(Rol::Titulo)) {
        elemento = elemento | ftxui::color(aFtxui(*color)) | ftxui::bold;
    }
    return elemento;
}

template <class T>
ftxui::Element CrudPersonas<T>::lineaOpcion(const Menu& menu, int indice) const {
    const std::string atajo(1, menu.atajoDe(indice));
    ftxui::Element elemento =
        ftxui::text("  " + atajo + ". " + menu.etiquetas()[static_cast<std::size_t>(indice)]);
    if (indice == menu.seleccion()) {
        // El resaltado de la opcion seleccionada. `inverted` funciona SIN
        // color, asi que el menu se ve igual con `NO_COLOR` definido.
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

template <class T>
ftxui::Element CrudPersonas<T>::lineaCampo(const Campo& campo, bool enfocado) const {
    ftxui::Element elemento =
        ftxui::text("  " + campo.etiqueta() + ": " + campo.textoEnPantalla());
    if (enfocado) {
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

template <class T>
ftxui::Element CrudPersonas<T>::lineaError(const std::string& texto) const {
    ftxui::Element elemento = ftxui::text("      " + texto);
    if (const auto color = _tema.colorDe(Rol::Error)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

template <class T>
ftxui::Element CrudPersonas<T>::lineaMensaje() const {
    const Mensaje& mensaje = mensajeActual();
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

template <class T>
ftxui::Element CrudPersonas<T>::lineaFormulario() const {
    ftxui::Elements lineas;
    for (std::size_t i = 0; i < _campos.size(); ++i) {
        lineas.push_back(lineaCampo(*_campos[i], i == _indiceCampo));
        // EL ERROR VA DEBAJO DEL CAMPO
        //
        // El error describe el ultimo intento y el texto NO se borra: el
        // usuario tiene que corregir lo que escribio, no volver a escribirlo.
        if (!_campos[i]->error().empty()) {
            lineas.push_back(lineaError(_campos[i]->error()));
        }
    }
    return ftxui::vbox(std::move(lineas));
}

template <class T>
ftxui::Element CrudPersonas<T>::lineaConfirmacion() const {
    ftxui::Elements lineas;
    if (_confirmacion) {
        lineas.push_back(ftxui::text("  " + _confirmacion->pregunta()));
        // LAS DOS OPCIONES EN DOS FILAS
        //
        // Una por fila, como en todos los otros menus: asi el motor sabe en
        // que fila esta cada una y puede traducir el clic del raton al
        // atajo que corresponde.
        lineas.push_back(ftxui::text("  1. Si"));
        lineas.push_back(ftxui::text("  2. No"));
    }
    return ftxui::vbox(std::move(lineas));
}

template <class T>
ftxui::Element CrudPersonas<T>::lineaListado() const {
    std::vector<std::vector<std::string>> filas;
    for (const T& entidad : _repositorio.listar()) {
        filas.push_back(_config.fila(entidad));
    }
    if (filas.empty()) {
        return ftxui::text("  " + vacio(_config.plural));
    }
    return tablaDe(filas, _config.cabeceras);
}

template <class T>
ftxui::Element CrudPersonas<T>::lineaResultados() const {
    if (_filasResultado.empty()) {
        return ftxui::text("  No hay coincidencias. 0: volver al menu.");
    }
    return tablaDe(_filasResultado, _config.cabeceras);
}

template <class T>
ftxui::Element CrudPersonas<T>::lineaFicha() const {
    ftxui::Elements lineas;

    if (_entidad) {
        lineas.push_back(ftxui::text("  Ficha de " + _config.singular + " #"
                                     + std::to_string(_entidad->getId())));
        std::vector<std::vector<std::string>> filas;
        filas.push_back(_config.fila(*_entidad));
        lineas.push_back(tablaDe(filas, _config.cabeceras));
        lineas.push_back(ftxui::separator());
    }

    // LOS TURNOS VIGENTES Y EL HISTORIAL COMPLETO
    //
    // Son DOS tablas y no una: un turno vigente es el que espera la persona
    // y un turno cancelado es parte de su historia. Mostrar solo uno de los
    // dos seria mostrar a medias.
    if (_config.turnos.vigentes) {
        lineas.push_back(ftxui::text("  Turnos vigentes:"));
        const std::vector<std::vector<std::string>> filas = _config.turnos.vigentes(_id);
        if (filas.empty()) {
            lineas.push_back(ftxui::text("    No tiene turnos vigentes."));
        } else {
            lineas.push_back(tablaDe(filas, _config.turnos.cabeceras));
        }
    }

    if (_config.turnos.historial) {
        lineas.push_back(ftxui::text("  Historial completo:"));
        const std::vector<std::vector<std::string>> filas = _config.turnos.historial(_id);
        if (filas.empty()) {
            lineas.push_back(ftxui::text("    No tiene turnos."));
        } else {
            lineas.push_back(tablaDe(filas, _config.turnos.cabeceras));
        }
    }

    return ftxui::vbox(std::move(lineas));
}

template <class T>
ftxui::Element CrudPersonas<T>::tablaDe(const std::vector<std::vector<std::string>>& filas,
                                         const std::vector<std::string>& cabeceras) const {
    Tabla tabla(cabeceras);
    for (const std::vector<std::string>& fila : filas) {
        tabla.agregarFila(fila);
    }
    return tabla.renderizar();
}

template <class T>
std::string CrudPersonas<T>::ayuda() const {
    // LA AYUDA DICE COMO VOLVER
    //
    // Un mensaje que no dice como se vuelve deja al usuario atrapado. En las
    // vistas con un campo de texto el '0' solo vuelve si el campo esta
    // vacio, asi que la ayuda lo dice: el '0' puede ser un dato.
    switch (_vista) {
        case Vista::Menu:
        case Vista::Criterio:
            return "Flechas: mover   Enter: elegir   0: volver   Esc: volver";
        case Vista::Formulario:
            return "Tab: siguiente campo   Enter: confirmar   0: volver (campo vacio)   "
                   "Esc: volver";
        case Vista::Id:
        case Vista::Valor:
            return "Enter: confirmar   0: volver (campo vacio)   Esc: volver";
        case Vista::Confirmar:
            return "1: Si   2: No   0: volver   Esc: volver";
        case Vista::Listado:
        case Vista::Resultados:
        case Vista::Ficha:
            return "0: volver   Esc: volver";
    }
    return "";
}

template <class T>
std::string CrudPersonas<T>::vacio(const std::string& queFalta) const {
    // EL ESTADO VACIO TIENE SALIDA
    //
    // Un mensaje que dice "no hay nada" y no dice como volver deja al
    // usuario atrapado: el mensaje dice las dos cosas.
    return "No hay " + queFalta + " para mostrar. 0: volver al menu.";
}

// ---------------------------------------------------------------------------
// El texto plano
// ---------------------------------------------------------------------------

template <class T>
std::string CrudPersonas<T>::textoPlano() const {
    std::string salida = _aplicacion.titulo() + " - " + _titulo + "\n";
    salida += textoDeLaVista();
    if (!mensajeActual().esVacio()) {
        salida += mensajeActual().texto() + "\n";
    }
    salida += ayuda();
    return salida;
}

template <class T>
std::string CrudPersonas<T>::vistaTextoPlano() const {
    // SOLO LA VISTA
    //
    // Es lo que se ve ahora, sin el encabezado ni la ayuda: es lo que
    // necesita el test para comprobar que la vista muestra lo que tiene que
    // mostrar.
    return textoDeLaVista();
}

template <class T>
std::string CrudPersonas<T>::unirFila(const std::vector<std::string>& fila) {
    std::string salida;
    for (std::size_t i = 0; i < fila.size(); ++i) {
        if (i > 0) {
            salida += " | ";
        }
        salida += fila[i];
    }
    return salida;
}

template <class T>
std::string CrudPersonas<T>::textoDeLaVista() const {
    std::string salida;

    switch (_vista) {
        case Vista::Menu:
        case Vista::Criterio: {
            const Menu& menu = _vista == Vista::Menu ? _menu : _menuCriterios;
            for (int i = 0; i < menu.cantidad(); ++i) {
                salida += "  " + std::string(1, menu.atajoDe(i)) + ". "
                    + menu.etiquetas()[static_cast<std::size_t>(i)] + "\n";
            }
            return salida;
        }
        case Vista::Formulario: {
            for (std::size_t i = 0; i < _campos.size(); ++i) {
                salida +=
                    "  " + _campos[i]->etiqueta() + ": " + _campos[i]->textoEnPantalla() + "\n";
                if (!_campos[i]->error().empty()) {
                    salida += "      " + _campos[i]->error() + "\n";
                }
            }
            return salida;
        }
        case Vista::Id:
            salida += "  " + _campoId->etiqueta() + ": " + _campoId->textoEnPantalla() + "\n";
            if (!_campoId->error().empty()) {
                salida += "      " + _campoId->error() + "\n";
            }
            return salida;
        case Vista::Valor:
            salida +=
                "  " + _campoValor->etiqueta() + ": " + _campoValor->textoEnPantalla() + "\n";
            if (!_campoValor->error().empty()) {
                salida += "      " + _campoValor->error() + "\n";
            }
            return salida;
        case Vista::Confirmar:
            if (_confirmacion) {
                salida += "  " + _confirmacion->pregunta() + "\n";
                salida += "  1. Si\n";
                salida += "  2. No\n";
            }
            return salida;
        case Vista::Listado: {
            const std::vector<T> entidades = _repositorio.listar();
            for (const T& entidad : entidades) {
                salida += "  " + unirFila(_config.fila(entidad)) + "\n";
            }
            if (entidades.empty()) {
                salida += "  " + vacio(_config.plural) + "\n";
            }
            return salida;
        }
        case Vista::Resultados: {
            for (const std::vector<std::string>& fila : _filasResultado) {
                salida += "  " + unirFila(fila) + "\n";
            }
            if (_filasResultado.empty()) {
                salida += "  No hay coincidencias. 0: volver al menu.\n";
            }
            return salida;
        }
        case Vista::Ficha: {
            if (_entidad) {
                salida += "  Ficha de " + _config.singular + " #"
                    + std::to_string(_entidad->getId()) + "\n";
                salida += "  " + unirFila(_config.fila(*_entidad)) + "\n";
            }
            if (_config.turnos.vigentes) {
                salida += "  Turnos vigentes:\n";
                for (const std::vector<std::string>& fila : _config.turnos.vigentes(_id)) {
                    salida += "    " + unirFila(fila) + "\n";
                }
            }
            if (_config.turnos.historial) {
                salida += "  Historial completo:\n";
                for (const std::vector<std::string>& fila : _config.turnos.historial(_id)) {
                    salida += "    " + unirFila(fila) + "\n";
                }
            }
            return salida;
        }
    }
    return salida;
}

} // namespace ui
