#pragma once

#include "repositories/IRepositorio.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "ui/Aplicacion.h"
#include "ui/Buscador.h"
#include "ui/Campo.h"
#include "ui/CargarEjemplo.h"
#include "ui/ColorFtxui.h"
#include "ui/Confirmacion.h"
#include "ui/CriterioBusqueda.h"
#include "ui/Menu.h"
#include "ui/Mensaje.h"
#include "ui/Pantalla.h"
#include "ui/RenderDeSelector.h"
#include "ui/Selector.h"
#include "ui/Tabla.h"
#include "ui/Theme.h"
#include "utils/Utils.h"

#include <ftxui.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// El motor CRUD de personas: la pantalla que sabe registrar, modificar,
// dar de baja, listar, buscar y mostrar la ficha de un PACIENTE o de un
// MEDICO.
//
// POR QUE UN TEMPLATE Y NO DOS PANTALLAS
//
// El flujo de un paciente y el de un medico es EL MISMO: las mismas vistas,
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
// El motor consume el `Escape` en sus vistas internas: desde la vista del
// buscador se vuelve a su menu, desde el formulario se vuelve a su menu, y
// desde la confirmacion se vuelve a la vista del buscador. En el menu CRUD el
// `Escape` NO se consume: ahi lo maneja la aplicacion, que saca la pantalla
// entera de la pila (y vuelve al menu principal).
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
// Los turnos de un paciente son `Turno`, no `Paciente`: el motor es generico
// sobre el tipo de la persona y no tiene por que saber como se ve un turno.
// Por eso el que decide es el que arma la CONFIGURACION: devuelve las
// filas ya formateadas, con el orden de `cabeceras`.
struct TurnosDePersona {
    std::vector<std::string> cabeceras;
    std::function<std::vector<std::vector<std::string>>(int)> vigentes;
    std::function<std::vector<std::vector<std::string>>(int)> historial;

    // LA NOTA DE LOS TURNOS
    //
    // Es un TEXTO que se dibuja DEBAJO de las tablas de turnos, y sirve para
    // explicar lo que hay marcado en ellas (por ejemplo, el "*" de los
    // turnos que ya no son de la especialidad del medico).
    //
    // ES DATO, NO UNA REGLA DEL MOTOR
    //
    // El motor NO sabe que significa la nota: la dibuja y listo. La que
    // sabe es la pantalla que arma la configuracion, y la pone ahi. Asi el
    // motor sigue siendo generico sobre el tipo de la persona.
    //
    // ES UNA FUNCION DEL ID
    //
    // La nota depende de la persona: si no hay nada que explicar, no hay
    // nota. Una leyenda que no explica nada es ruido.
    std::function<std::string(int)> nota;
};

// EL CRITERIO DE BUSQUEDA Y LA COINCIDENCIA VIVEN EN SU PROPIO HEADER
//
// El criterio (y la regla con la que el buscador corre los criterios) lo
// usan tambien las pantallas que buscan a una persona para otra cosa (la de
// turnos al agendar y la de reportes). Si vivieran aca, esas pantallas
// dependerian del motor CRUD entero para tener un criterio.

// El texto en mayusculas.
//
// SOLO ASCII: es para los titulos ("PACIENTES", "MEDICOS"), que son
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

template <class T>
class CrudPersonas : public Pantalla {
public:
    // La fila de una entidad: las celdas, en el orden de las cabeceras.
    using Fila = std::function<std::vector<std::string>(const T&)>;

    // TODO lo que cambia entre un paciente y un medico.
    struct Config {
        std::string singular;                        // "paciente"
        std::string plural;                          // "pacientes"
        std::string opcionFicha;                     // "Ver ficha + historial"
        std::vector<std::string> cabeceras;          // las columnas de la tabla
        Fila fila;                                   // la fila de una entidad
        std::vector<CampoFormulario> campos;         // el formulario
        std::function<T(const std::vector<std::string>&)> construir;  // arma la entidad
        std::vector<CriterioBusqueda> criterios;     // los criterios de busqueda
        std::function<bool(const T&)> esActivo;      // esta activa?
        TurnosDePersona turnos;                      // los turnos de la ficha

        // LAS COLUMNAS QUE MUESTRA EL BUSCADOR, por NOMBRE (no por
        // posicion). La del id tiene que estar: es la que el buscador
        // devuelve.
        //
        // Se buscan por nombre dentro de `cabeceras`, asi que el orden de las
        // columnas de la entidad puede cambiar sin que el buscador muestre
        // otra cosa.
        std::vector<std::string> columnasBuscador;

        // La pista del campo del buscador ("Nombre, apellido o DNI").
        std::string pistaBuscador;

        // EL SELECTOR DEL FORMULARIO
        //
        // Un dato que se ELIGE de una lista en vez de escribirse. Es
        // OPCIONAL: si no esta, el formulario es solo de texto y el motor
        // funciona exactamente como antes.
        //
        // El `valor` devuelve el VALOR de la opcion elegida (no su
        // etiqueta): para la especialidad es el CODIGO ("GAST"), que es lo
        // que viaja al archivo. La `etiqueta` es lo que ve el usuario.
        //
        // EL VALOR VA AL FINAL DEL VECTOR DE VALORES
        //
        // El motor arma el vector de `construir` con los campos de texto, en
        // el orden de `campos`, y el valor del selector se AGREGA al final.
        struct SelectorDeFormulario {
            std::string etiqueta;                                  // "especialidad"
            std::vector<std::string> opciones;                     // las 12 descripciones
            std::function<std::string(std::size_t)> valor;         // el valor de cada opcion
            std::function<int(const T&)> indiceInicial;            // la opcion que arranca elegida
        };
        std::optional<SelectorDeFormulario> selector;
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
        Selector,
        Busqueda,
        Confirmar,
        Listado,
        Criterio,
        Valor,
        Resultados,
        Ficha,
        ConfirmarEjemplo
    };

    // Que hay que hacer cuando se confirme el id.
    enum class Accion { Modificar, Baja, Ficha };

    // Si el formulario esta registrando o modificando.
    enum class ModoFormulario { Registro, Modificacion };

    // La fila de la pantalla donde se dibuja la PRIMERA opcion de la
    // confirmacion (la del "Si").
    //
    // La pantalla dibuja las dos opciones en dos filas conocidas y se lo
    // dice a la `ui::Confirmacion` (`definirFilaInicial`): sin esto, un clic
    // en el ENCABEZADO caeria en la primera opcion y confirmaria una baja
    // sin querer.
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
    bool manejarSelector(const Evento& evento);
    bool manejarBusqueda(const Evento& evento);
    bool manejarCampo(const Evento& evento, const std::unique_ptr<Campo>& campo);
    bool manejarConfirmacion(const Evento& evento);
    bool manejarConfirmacionEjemplo(const Evento& evento);
    bool manejarListado(const Evento& evento);
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

    // --- El selector ---
    //
    // Con selector, el formulario NO guarda: deja los valores esperando y
    // abre el selector. El Enter del selector es el que arma la entidad y
    // la guarda.
    void abrirSelector();
    void confirmarSeleccion();
    void guardar(std::vector<std::string> valores);

    // --- El id ---
    //
    // La entidad ya NO se pide por id: se BUSCA. El `ui::Buscador` corre los
    // criterios de la configuracion y devuelve el id de la entidad elegida.
    void abrirBuscador(Accion accion);
    void confirmarId();
    std::vector<Coincidencia> coincidenciasDelBuscador(const std::string& texto);
    std::vector<Coincidencia> activosDelBuscador() const;
    std::vector<std::string> celdasDelBuscador(const std::vector<std::string>& fila) const;

    // --- La baja y la reactivation ---
    std::string preguntaDeBaja(const T& entidad) const;
    void ejecutarBaja();

    // --- El estado vacio con ACCION ---
    //
    // Un listado sin datos no es un mensaje: es un menu con dos opciones
    // ("cargar ejemplo" y "volver"). La logica de la carga vive en el
    // helper compartido (`ui::cargarDatosDeEjemplo`); el motor solo la pide
    // y muestra el resultado.
    void pedirCargaDeEjemplo();
    void ejecutarCargaDeEjemplo();

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
    ftxui::Element lineaNumeroSelector() const;
    ftxui::Element lineaCampo(const Campo& campo, bool enfocado) const;
    ftxui::Element lineaBuscador() const;
    ftxui::Element lineaError(const std::string& texto) const;
    ftxui::Element lineaMensaje() const;
    ftxui::Element lineaFormulario() const;
    ftxui::Element lineaConfirmacion() const;
    ftxui::Element lineaConfirmacionEjemplo() const;
    ftxui::Element lineaListado() const;
    ftxui::Element lineaMenuVacio() const;
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

    // EL MENU DEL ESTADO VACIO
    //
    // Es el menu de DOS opciones que aparece cuando el listado no tiene
    // nada: "cargar ejemplo" y "volver". El atajo '1' es el que dispara la
    // carga y el '2' el que vuelve (el '0' tambien vuelve, como en todos los
    // niveles).
    Menu _menuVacio;

    Vista _vista = Vista::Menu;
    ModoFormulario _modoFormulario = ModoFormulario::Registro;
    Accion _accion = Accion::Modificar;

    // Los campos del formulario y el que tiene el foco.
    std::vector<std::unique_ptr<Campo>> _campos;
    std::size_t _indiceCampo = 0;

    // El selector del formulario y los valores que esperan su eleccion.
    //
    // Los valores quedan ESPERANDO porque el selector es un paso DEL
    // formulario: si el usuario vuelve atras (con el `Escape`), lo que
    // escribio sigue ahi.
    std::unique_ptr<Selector> _selector;
    std::vector<std::string> _valoresPendientes;

    // El campo del valor de busqueda (el de la pantalla "Buscar").
    std::unique_ptr<Campo> _campoValor;

    // El buscador de la entidad (el de "Modificar", "Baja" y "Ficha").
    std::unique_ptr<Buscador> _buscador;

    // La confirmacion de la baja.
    std::unique_ptr<Confirmacion> _confirmacion;

    // La confirmacion de la carga de datos de ejemplo.
    std::unique_ptr<Confirmacion> _confirmacionEjemplo;

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
      _menuCriterios("Buscar " + _config.plural),
      _menuVacio("No hay " + _config.plural) {
    // EL MENU CRUD
    //
    // El atajo es el numero que se escribe (decision D28) y el '0' es
    // "Volver" en todos los niveles.
    //
    // LAS DOS PRIMERAS OPCIONES LLEVAN EL NOMBRE DE LA ENTIDAD
    //
    // El motor es generico, asi que el texto sale de la configuracion:
    // "Registrar paciente" en una pantalla y "Registrar medico" en la otra.
    // La opcion de la ficha tambien: cada pantalla dice lo que su ficha
    // muestra ("Ver ficha + historial", "Ver ficha + turnos").
    _menu.agregar('1', "Registrar " + _config.singular, [this]() { iniciarRegistro(); });
    _menu.agregar('2', "Modificar " + _config.singular, [this]() { iniciarModificacion(); });
    _menu.agregar('3', "Baja / Reactivar", [this]() { iniciarBaja(); });
    _menu.agregar('4', "Listar todos", [this]() { irA(Vista::Listado); });
    _menu.agregar('5', "Buscar", [this]() { abrirCriterio(); });
    _menu.agregar('6', _config.opcionFicha, [this]() { iniciarFicha(); });
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

    // EL MENU DEL ESTADO VACIO
    //
    // Es el menu de DOS opciones que aparece cuando el listado no tiene
    // nada. El '1' carga el juego de ejemplo (pide confirmacion antes) y el
    // '2' vuelve al menu del motor.
    _menuVacio.agregar('1', "Cargar ejemplo", [this]() { pedirCargaDeEjemplo(); });
    _menuVacio.agregar('2', "Volver", [this]() { volverAtras(); });
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
    static const std::string selector = "Selector";
    static const std::string busqueda = "Busqueda";
    static const std::string confirmacion = "Confirmar";
    static const std::string listado = "Listado";
    static const std::string criterio = "Criterio";
    static const std::string valor = "Valor";
    static const std::string resultados = "Resultados";
    static const std::string ficha = "Ficha";
    static const std::string confirmarEjemplo = "ConfirmarEjemplo";

    switch (vista) {
        case Vista::Menu:
            return menu;
        case Vista::Formulario:
            return formulario;
        case Vista::Selector:
            return selector;
        case Vista::Busqueda:
            return busqueda;
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
        case Vista::ConfirmarEjemplo:
            return confirmarEjemplo;
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
// dni puede empezar con un cero) y se lo queda el campo.
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
    //   - desde la confirmacion, se vuelve a la vista ANTERIOR (la del
    //     buscador), que es lo que dice la especificacion;
    //   - desde cualquier otra vista, se vuelve al menu CRUD.
    switch (_vista) {
        case Vista::Menu:
            _aplicacion.volver();
            return;
        case Vista::Confirmar:
            irA(Vista::Busqueda);
            return;
        case Vista::ConfirmarEjemplo:
            irA(Vista::Listado);
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
        case Vista::Selector:
            return manejarSelector(evento);
        case Vista::Busqueda:
            return manejarBusqueda(evento);
        case Vista::Confirmar:
            return manejarConfirmacion(evento);
        case Vista::ConfirmarEjemplo:
            return manejarConfirmacionEjemplo(evento);
        case Vista::Criterio:
            return manejarMenuCriterios(evento);
        case Vista::Valor:
            return manejarCampo(evento, _campoValor);
        case Vista::Listado:
            return manejarListado(evento);
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

// ---------------------------------------------------------------------------
// El selector
// ---------------------------------------------------------------------------

template <class T>
bool CrudPersonas<T>::manejarSelector(const Evento& evento) {
    if (!_selector) {
        return false;
    }

    // EL `Escape` VUELVE AL FORMULARIO
    //
    // El selector es un paso DEL formulario (no un nivel del menu): el
    // `Escape` sube al formulario, con todo lo que el usuario escribio
    // intacto.
    if (esTeclaEscape(evento)) {
        irA(Vista::Formulario);
        return true;
    }

    // EL RATON MUEVE EL RESALTADO, NO CONFIRMA
    //
    // Un clic elige la fila, pero la confirmacion es el Enter: un clic
    // accidental no cambia la especialidad de un medico.
    if (evento.esMouse()) {
        if (!evento.esClic()) {
            return false;
        }
        const int indice = _selector->opcionEnFila(evento.y, _selector->filaInicial());
        if (indice < 0) {
            // El clic cae fuera de las opciones: no se consume.
            return false;
        }
        _selector->seleccionar(indice);
        return true;
    }

    // EL '0' NO ES "VOLVER" ACA
    //
    // En el selector el '0' es un DIGITO: es el numero de una opcion, y como
    // el catalogo tiene doce opciones (y no diez), el '0' no puede ser un
    // atajo de vuelta. Si no existe, el selector muestra el error.
    if (!_selector->aplicarEvento(evento)) {
        return false;
    }

    // EL ENTER CONFIRMA LA ELECCION
    //
    // Con un numero escrito que no existe, el selector deja el error y el
    // numero en la pantalla: el usuario lo corrige. Sin error, la eleccion
    // es valida y se guarda.
    if (evento.tipo == TipoTecla::Enter && _selector->error().empty()) {
        confirmarSeleccion();
    }
    return true;
}

// ---------------------------------------------------------------------------
// El buscador
//
// LA ENTIDAD SE BUSCA, NO SE ESCRIBE
//
// El `ui::Buscador` tiene DOS focos: el TEXTO (lo que se escribe) y la LISTA
// (las coincidencias). El `Escape` y el '0' los atiende la pantalla; el
// resto lo atiende el widget.
// ---------------------------------------------------------------------------

template <class T>
bool CrudPersonas<T>::manejarBusqueda(const Evento& evento) {
    if (!_buscador) {
        return false;
    }

    // EL `Escape` VUELVE AL MENU DEL MOTOR
    //
    // El buscador NO lo consume (el `Escape` es una tecla de navegacion, no
    // un dato): lo atiende la pantalla, que sube un nivel.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }

    // EL '0' ES "VOLVER" SOLO SI EL TEXTO ESTA VACIO
    //
    // Con algo escrito el '0' es un dato (un dni puede empezar con un cero) y
    // se lo queda el campo. Y solo cuando el foco esta en el TEXTO: en la
    // lista el '0' es el numero de una opcion.
    if (_buscador->focoEnElTexto() && esTeclaVolver(evento) && _buscador->filtro().empty()) {
        volverAtras();
        return true;
    }

    // EL RATON MUEVE EL RESALTADO, NO CONFIRMA
    //
    // Un clic elige la fila, pero la confirmacion es el Enter: un clic
    // accidental no da de baja a un paciente.
    if (evento.esMouse()) {
        if (!evento.esClic()) {
            return false;
        }
        const int indice = _buscador->coincidenciaEnFila(evento.y, _buscador->filaInicial());
        if (indice < 0) {
            // El clic cae fuera de las coincidencias: no se consume.
            return false;
        }
        _buscador->seleccionar(indice);
        return true;
    }

    if (!_buscador->aplicarEvento(evento)) {
        return false;
    }

    // EL ENTER ELIGE LA COINCIDENCIA
    //
    // Con un numero escrito que no existe, el buscador deja el error y el
    // numero en la pantalla: el usuario lo corrige. Sin error, la entidad
    // elegida es la que estaba resaltada.
    if (_buscador->elegido()) {
        confirmarId();
    }
    return true;
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
        confirmarValor();
        return true;
    }

    // EL `Escape` SUBE UN NIVEL
    //
    // El motor consume el `Escape` en sus vistas internas: desde la vista del
    // valor se vuelve al menu CRUD. El `Escape` es una tecla de navegacion, no
    // un dato: se atiende aunque el campo tenga texto escrito.
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
    // EL `Escape` VUELVE A LA VISTA DEL BUSCADOR
    //
    // `volverAtras()` desde la confirmacion vuelve a la vista anterior (la
    // del buscador), que es lo que dice la especificacion.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    if (!_confirmacion) {
        return false;
    }

    // EL RATON LO RESUELVE LA CONFIRMACION
    //
    // La `ui::Confirmacion` tiene su propio hit-testing: el motor le dijo en
    // que fila se dibujan sus opciones (`definirFilaInicial`) y ella sabe
    // que opcion se toco. Por eso el clic NO se traduce a mano aca.
    if (!_confirmacion->aplicarEvento(evento)) {
        return false;
    }

    if (_confirmacion->confirmo()) {
        ejecutarBaja();
    } else {
        // Si NO confirma, se vuelve a la vista anterior (la del buscador).
        irA(Vista::Busqueda);
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
// El estado vacio CON ACCION
//
// Un listado sin datos no es un mensaje: es un MENU con dos opciones. La
// primera carga el juego de ejemplo (pidiendo confirmacion antes) y la
// segunda vuelve al menu del motor.
//
// El '0' y el `Escape` tambien vuelven (la regla de UX del proyecto), asi
// que el usuario no queda atrapado en el estado vacio.
// ---------------------------------------------------------------------------

template <class T>
bool CrudPersonas<T>::manejarListado(const Evento& evento) {
    // EL ESTADO VACIO TIENE SU PROPIO MENU
    //
    // Solo cuando NO hay nada que mostrar: con datos, el listado es una
    // tabla de solo lectura (como antes).
    if (_repositorio.listar().empty()) {
        if (esTeclaVolver(evento)) {
            volverAtras();
            return true;
        }
        if (esTeclaEscape(evento)) {
            volverAtras();
            return true;
        }
        return _menuVacio.aplicarEvento(evento);
    }

    return manejarLectura(evento);
}

template <class T>
void CrudPersonas<T>::pedirCargaDeEjemplo() {
    // LA CONFIRMACION DE DOS PASOS
    //
    // La carga pisa los datos de trabajo: no se puede deshacer, asi que se
    // pregunta antes (decision D37: los atajos son '1' y '2', no '0').
    _confirmacionEjemplo = std::make_unique<Confirmacion>(
        "Cargar los datos de ejemplo? Se pisan los datos actuales.");
    // La confirmacion se dibuja en unas filas conocidas: se lo dice para que
    // su hit-testing del raton funcione.
    _confirmacionEjemplo->definirFilaInicial(FILA_INICIAL_CONFIRMACION);
    irA(Vista::ConfirmarEjemplo);
}

template <class T>
void CrudPersonas<T>::ejecutarCargaDeEjemplo() {
    // LA LOGICA DE LA COPIA Y LA RECARGA VIVE EN EL HELPER COMPARTIDO
    //
    // El motor no sabe de archivos: solo pide la carga y muestra el
    // resultado. Asi el motor CRUD, la pantalla de turnos y la de
    // configuracion hacen EXACTAMENTE lo mismo.
    std::string error;
    if (cargarDatosDeEjemplo(_aplicacion.contexto(), error)) {
        avisarExito("Datos de ejemplo cargados.");
    } else {
        avisar(error);
    }
    // Se vuelve al listado: ahora muestra los datos nuevos (el listado se
    // dibuja a partir del repositorio, que ya fue recargado).
    irA(Vista::Listado);
}

template <class T>
bool CrudPersonas<T>::manejarConfirmacionEjemplo(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE AL LISTADO
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    if (!_confirmacionEjemplo) {
        return false;
    }

    // EL RATON LO RESUELVE LA CONFIRMACION
    //
    // El motor le dijo en que fila se dibujan sus opciones
    // (`definirFilaInicial`), asi que el clic NO se traduce a mano aca.
    if (!_confirmacionEjemplo->aplicarEvento(evento)) {
        return false;
    }

    if (_confirmacionEjemplo->confirmo()) {
        ejecutarCargaDeEjemplo();
    } else {
        // Si NO confirma, se vuelve al listado sin hacer nada.
        irA(Vista::Listado);
    }
    return true;
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
    abrirBuscador(Accion::Modificar);
}

template <class T>
void CrudPersonas<T>::iniciarBaja() {
    abrirBuscador(Accion::Baja);
}

template <class T>
void CrudPersonas<T>::iniciarFicha() {
    abrirBuscador(Accion::Ficha);
}

// ---------------------------------------------------------------------------
// El buscador de la entidad
//
// LA ENTIDAD SE BUSCA, NO SE ESCRIBE
//
// Antes se pedia el id en un campo de texto: un dato que nadie tiene en la
// cabeza (es una clave de un archivo, no un dato del consultorio). Ahora se
// escribe un dato que el usuario CONOCE (el nombre, el apellido, el dni) y
// se elige una de las coincidencias.
//
// LAS REGLAS DE BUSQUEDA SON LAS DE LA CONFIGURACION
//
// El buscador NO escribe reglas propias: corre los mismos criterios que usa
// la pantalla "Buscar" (los de `config.criterios`), que a su vez preguntan al
// repositorio. Si el buscador buscara distinto, habria dos reglas que se
// pueden desincronizar.
// ---------------------------------------------------------------------------

template <class T>
void CrudPersonas<T>::abrirBuscador(Accion accion) {
    _accion = accion;
    _buscador = std::make_unique<Buscador>(
        "Buscar " + _config.singular,
        _config.pistaBuscador,
        [this](const std::string& texto) { return coincidenciasDelBuscador(texto); });
    irA(Vista::Busqueda);
}

template <class T>
std::vector<Coincidencia> CrudPersonas<T>::coincidenciasDelBuscador(const std::string& texto) {
    // LA REGLA DEL BUSCADOR ESTA EN UN SOLO LUGAR
    //
    // Con el texto vacio se listan los activos, y con texto se corren los
    // criterios de la configuracion sin repetir. Es la MISMA regla que usan
    // las pantallas que buscan a una persona para otra cosa (la de turnos al
    // agendar y la de reportes): ver `ui::CriterioBusqueda.h`.
    return coincidenciasDelBuscadorDePersonas(
        _config.criterios,
        [this]() { return activosDelBuscador(); },
        [this](const std::vector<std::string>& fila) { return celdasDelBuscador(fila); },
        texto);
}

template <class T>
std::vector<Coincidencia> CrudPersonas<T>::activosDelBuscador() const {
    // LAS ENTIDADES ACTIVAS, CON SU FILA
    //
    // Son las que se pueden usar: las dadas de baja no. (Se siguen BUSCANDO
    // por nombre, dni o legajo: los criterios las encuentran, asi que la
    // reactivacion se puede hacer escribiendo el nombre.)
    std::vector<Coincidencia> activos;
    for (const T& entidad : _repositorio.listar()) {
        const bool activa = _config.esActivo ? _config.esActivo(entidad) : true;
        if (!activa) {
            continue;
        }
        activos.push_back({entidad.getId(), _config.fila(entidad)});
    }
    return activos;
}

template <class T>
std::vector<std::string> CrudPersonas<T>::celdasDelBuscador(
    const std::vector<std::string>& fila) const {
    // LAS CELDAS QUE MUESTRA EL BUSCADOR
    //
    // Son las de las columnas que declara la configuracion, buscadas por
    // NOMBRE dentro de `cabeceras`: asi el buscador muestra lo que dice la
    // configuracion y no lo que quede en una posicion. Una columna que no
    // existe queda vacia (nunca se lee una celda de otro lado).
    return celdasDeLasColumnas(fila, _config.cabeceras, _config.columnasBuscador);
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

    // ARMAR LOS VALORES
    //
    // El vector que recibe `construir` es [id, campo1, campo2, ...]: el id
    // va adelante porque lo PROPONE el repositorio (o es el id de la entidad
    // que se esta modificando) y nunca lo escribe el usuario.
    const int id =
        _modoFormulario == ModoFormulario::Registro ? _repositorio.proximoId() : _id;

    std::vector<std::string> valores;
    valores.reserve(_campos.size() + 2);
    valores.push_back(std::to_string(id));
    for (const std::unique_ptr<Campo>& campo : _campos) {
        valores.push_back(campo->valor());
    }

    // CON SELECTOR: PRIMERO SE ELIGE, DESPUES SE GUARDA
    //
    // Un dato que se ELIGE no se escribe: el formulario deja los valores
    // ESPERANDO y abre el selector. El Enter del selector es el que arma la
    // entidad y la guarda.
    if (_config.selector) {
        _valoresPendientes = std::move(valores);
        abrirSelector();
        return;
    }

    guardar(std::move(valores));
}

template <class T>
void CrudPersonas<T>::abrirSelector() {
    const typename Config::SelectorDeFormulario& descriptor = *_config.selector;

    _selector = std::make_unique<Selector>(descriptor.etiqueta, descriptor.opciones);

    // LA OPCION QUE ARRANCA ELEGIDA
    //
    // Al MODIFICAR arranca con la especialidad que ya tiene la entidad: el
    // selector muestra el dato que se esta editando, no el primero de la
    // lista. Al REGISTRAR no hay entidad, asi que arranca con la primera.
    if (_entidad && descriptor.indiceInicial) {
        _selector->seleccionar(descriptor.indiceInicial(*_entidad));
    }

    irA(Vista::Selector);
}

template <class T>
void CrudPersonas<T>::confirmarSeleccion() {
    if (!_selector || !_config.selector) {
        irA(Vista::Menu);
        return;
    }
    const typename Config::SelectorDeFormulario& descriptor = *_config.selector;
    if (!descriptor.valor) {
        // No deberia pasar: es un error de programacion de la pantalla.
        avisar("el selector no tiene valor configurado");
        irA(Vista::Formulario);
        return;
    }
    const int indice = _selector->seleccion();
    if (indice < 0 || indice >= _selector->cantidad()) {
        // No deberia pasar: es un error de programacion de la pantalla (un
        // selector sin opciones no tiene nada que elegir).
        avisar("el selector no tiene opciones para elegir");
        irA(Vista::Formulario);
        return;
    }

    // EL VALOR DEL SELECTOR VA AL FINAL
    //
    // El vector de valores es [id, campo1, campo2, ...] y el valor elegido
    // se AGREGA al final: asi los campos de texto no cambian de lugar.
    std::vector<std::string> valores = _valoresPendientes;
    valores.push_back(descriptor.valor(static_cast<std::size_t>(indice)));

    guardar(std::move(valores));
}

template <class T>
void CrudPersonas<T>::guardar(std::vector<std::string> valores) {
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
        // dni ya la tiene otro). El error se muestra y el formulario
        // sigue abierto: no se pierde lo que el usuario escribio.
        //
        // Con selector, el error puede aparecer en la vista del selector:
        // en ese caso se vuelve al formulario, que es donde se corrige.
        avisar(error.motivo());
        irA(Vista::Formulario);
    } catch (const ExcepcionSistema& error) {
        avisar(error.what());
        irA(Vista::Formulario);
    }
}

// ---------------------------------------------------------------------------
// El id elegido
// ---------------------------------------------------------------------------

template <class T>
void CrudPersonas<T>::confirmarId() {
    // EL ID VIENE DEL BUSCADOR
    //
    // El buscador solo ofrece entidades que EXISTEN (las busca en el
    // repositorio), asi que el chequeo de abajo no deberia disparar nunca.
    // Se deja igual: es la red de contencion contra un dato raro.
    const int id = _buscador ? _buscador->idElegido() : -1;
    T entidad;
    if (id < 0 || !_repositorio.buscarPorId(id, entidad)) {
        avisar("no existe " + _config.singular + " #" + std::to_string(id));
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
            // La confirmacion se dibuja en unas filas conocidas: se lo dice
            // para que su hit-testing del raton funcione.
            _confirmacion->definirFilaInicial(FILA_INICIAL_CONFIRMACION);
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
    _filasResultado.clear();
    if (criterio.buscar) {
        // LA PANTALLA "BUSCAR" MUESTRA LAS CELDAS
        //
        // El criterio devuelve el id junto con la fila (el id lo usa el
        // buscador, para elegir la entidad). Aca el id no hace falta: lo que
        // se muestra es una TABLA de las coincidencias.
        for (const Coincidencia& coincidencia : criterio.buscar(valor)) {
            _filasResultado.push_back(coincidencia.second);
        }
    }
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
    // LA COLUMNA SE BUSCA POR NOMBRE
    //
    // La regla (la de `ui::CriterioBusqueda.h`) la usan el buscador y el
    // formulario de modificar: los dos buscan la columna por su NOMBRE.
    return ui::indiceDeColumna(_config.cabeceras, nombre);
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
        case Vista::Selector: {
            if (!_selector) {
                break;
            }
            lineas.push_back(ftxui::text("  " + _selector->titulo()));
            lineas.push_back(lineaNumeroSelector());
            // La opcion `i` del selector queda en la fila `filaInicial + i`:
            // el hit-testing del raton lo necesita.
            _selector->definirFilaInicial(static_cast<int>(lineas.size()));
            // EL DIBUJO DE LA LISTA ES EL COMPARTIDO
            //
            // Es el mismo que usa la pantalla de turnos (ver
            // `ui/RenderDeSelector.h`): asi el resaltado de la opcion
            // elegida es el mismo en toda la aplicacion.
            for (ftxui::Element& elemento : lineasDeSelector(*_selector)) {
                lineas.push_back(elemento);
            }
            if (!_selector->error().empty()) {
                lineas.push_back(lineaError(_selector->error()));
            }
            break;
        }
        case Vista::Busqueda: {
            if (!_buscador) {
                break;
            }
            lineas.push_back(ftxui::text("  " + _buscador->titulo()));
            lineas.push_back(lineaBuscador());
            // La coincidencia `i` queda en la fila `filaInicial + i`: el
            // hit-testing del raton lo necesita.
            _buscador->definirFilaInicial(static_cast<int>(lineas.size()));
            for (ftxui::Element& elemento : lineasDeBuscador(*_buscador)) {
                lineas.push_back(elemento);
            }
            if (!_buscador->error().empty()) {
                lineas.push_back(lineaError(_buscador->error()));
            }
            break;
        }
        case Vista::Valor:
            lineas.push_back(lineaCampo(*_campoValor, true));
            break;
        case Vista::Confirmar:
            lineas.push_back(lineaConfirmacion());
            break;
        case Vista::ConfirmarEjemplo:
            lineas.push_back(lineaConfirmacionEjemplo());
            break;
        case Vista::Listado:
            // EL ESTADO VACIO ES UN MENU
            //
            // Sin datos, el listado no es una tabla: es un mensaje ("no hay
            // ...") mas un menu de DOS opciones ("cargar ejemplo" y "volver").
            // Con datos, es la tabla de siempre.
            if (_repositorio.listar().empty()) {
                lineas.push_back(ftxui::text("  " + vacio(_config.plural)));
                // La opcion `i` del menu vacio queda en la fila
                // `filaInicial + i`: el hit-testing del raton lo necesita.
                _menuVacio.definirFilaInicial(static_cast<int>(lineas.size()));
                lineas.push_back(lineaMenuVacio());
            } else {
                lineas.push_back(lineaListado());
            }
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
ftxui::Element CrudPersonas<T>::lineaNumeroSelector() const {
    // EL NUMERO QUE SE ESTA ESCRIBIENDO
    //
    // Va a la vista porque es lo que el usuario esta TECLEANDO: si no se
    // mostrara, escribir "12" seria indistinguible de no escribir nada. La
    // barra vertical es el cursor, como en el `Campo`.
    return ftxui::text("  Numero: " + _selector->numeroDigitado() + "|");
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
ftxui::Element CrudPersonas<T>::lineaBuscador() const {
    // EL CAMPO DEL BUSCADOR
    //
    // Se invierte cuando el foco esta en el TEXTO: es la misma convencion
    // de los demas campos (el foco se ve invertido).
    ftxui::Element elemento = ftxui::text("  " + _buscador->lineaDelCampo());
    if (_buscador->focoEnElTexto()) {
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
ftxui::Element CrudPersonas<T>::lineaConfirmacionEjemplo() const {
    ftxui::Elements lineas;
    if (_confirmacionEjemplo) {
        lineas.push_back(ftxui::text("  " + _confirmacionEjemplo->pregunta()));
        // LAS DOS OPCIONES EN DOS FILAS (como en la confirmacion de la baja).
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
    return tablaDe(filas, _config.cabeceras);
}

template <class T>
ftxui::Element CrudPersonas<T>::lineaMenuVacio() const {
    // EL ESTADO VACIO ES UN MENU, NO UN MENSAJE
    //
    // Un listado sin datos tiene una salida (cargar el ejemplo) y una
    // vuelta (volver). El menu se dibuja con el MISMO formato que todos
    // los otros menus de la aplicacion ("  N. Etiqueta").
    ftxui::Elements lineas;
    for (int i = 0; i < _menuVacio.cantidad(); ++i) {
        lineas.push_back(lineaOpcion(_menuVacio, i));
    }
    return ftxui::vbox(std::move(lineas));
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

    // LA NOTA DE LOS TURNOS
    //
    // Va DEBAJO de las tablas: es la leyenda de lo que hay marcado. El motor
    // no sabe que dice: la dibuja y listo (la nota la pone la pantalla).
    if (_config.turnos.nota) {
        const std::string nota = _config.turnos.nota(_id);
        if (!nota.empty()) {
            lineas.push_back(ftxui::text("  " + nota));
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
        case Vista::Selector:
            return "Flechas: mover   Numero: elegir   Enter: confirmar   "
                   "Esc: volver al formulario";
        case Vista::Busqueda:
            // LA AYUDA DE UN BUSCADOR DICE LOS DOS PASOS
            //
            // El buscador tiene dos focos: se escribe el texto (Enter busca)
            // y se elige de la lista (flechas o numero). El '0' vuelve SOLO
            // con el texto vacio, como en cualquier otro campo de texto (con
            // algo escrito el '0' es un dato).
            return "Enter: buscar   Flechas: elegir   Retroceso: corregir   "
                   "0: volver   Esc: volver";
        case Vista::Valor:
            return "Enter: confirmar   0: volver (campo vacio)   Esc: volver";
        case Vista::Confirmar:
            return "1: Si   2: No   0: volver   Esc: volver";
        case Vista::ConfirmarEjemplo:
            return "1: Si   2: No   0: volver   Esc: volver";
        case Vista::Listado:
            // En el listado VACIO las opciones son las del menu del estado
            // vacio; con datos, el listado es de solo lectura.
            if (_repositorio.listar().empty()) {
                return "1: cargar ejemplo   2: volver   0: volver   Esc: volver";
            }
            return "0: volver   Esc: volver";
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
    //
    // La salida (cargar el ejemplo) y la vuelta (volver) son las dos
    // opciones del menu del estado vacio.
    return "No hay " + queFalta + " para mostrar.";
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
        case Vista::Selector: {
            if (_selector) {
                // La vista en texto plano tiene el MISMO orden que la
                // dibujada: el titulo, el numero que se esta escribiendo y
                // las opciones.
                salida += "  " + _selector->titulo() + "\n";
                salida += "  Numero: " + _selector->numeroDigitado() + "\n";
                for (int i = 0; i < _selector->cantidad(); ++i) {
                    salida += _selector->lineaDe(i, i == _selector->seleccion()) + "\n";
                }
                if (!_selector->error().empty()) {
                    salida += "      " + _selector->error() + "\n";
                }
            }
            return salida;
        }
        case Vista::Busqueda: {
            if (_buscador) {
                // La vista en texto plano tiene el MISMO orden que la
                // dibujada: el titulo, el campo y las lineas de las
                // coincidencias.
                salida += _buscador->textoPlano();
            }
            return salida;
        }
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
        case Vista::ConfirmarEjemplo:
            if (_confirmacionEjemplo) {
                salida += "  " + _confirmacionEjemplo->pregunta() + "\n";
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
                // EL ESTADO VACIO ES UN MENU
                //
                // El texto plano conserva el formato del menu ("  N.
                // Etiqueta"), asi que lo que se ve sin terminal es lo mismo
                // que lo que se ve en la terminal.
                salida += "  " + vacio(_config.plural) + "\n";
                for (int i = 0; i < _menuVacio.cantidad(); ++i) {
                    salida += "  " + std::string(1, _menuVacio.atajoDe(i)) + ". "
                        + _menuVacio.etiquetas()[static_cast<std::size_t>(i)] + "\n";
                }
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
            if (_config.turnos.nota) {
                const std::string nota = _config.turnos.nota(_id);
                if (!nota.empty()) {
                    salida += "  " + nota + "\n";
                }
            }
            return salida;
        }
    }
    return salida;
}

} // namespace ui
