#pragma once

#include "entities/Turno.h"
#include "repositories/RepositorioMedicos.h"
#include "repositories/RepositorioPacientes.h"
#include "repositories/RepositorioTurnos.h"
#include "services/ServicioTurnos.h"
#include "ui/Aplicacion.h"
#include "ui/Buscador.h"
#include "ui/Campo.h"
#include "ui/ColorFtxui.h"
#include "ui/Confirmacion.h"
#include "ui/Menu.h"
#include "ui/Mensaje.h"
#include "ui/Pantalla.h"
#include "ui/RenderDeSelector.h"
#include "ui/Selector.h"
#include "ui/Tabla.h"
#include "ui/Theme.h"

#include <ftxui.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// La pantalla de TURNOS.
//
// POR QUE NO ES EL MOTOR CRUD
//
// Un turno NO es una persona: no se registra, no se da de baja y no se
// busca por nombre. Sus acciones tienen reglas PROPIAS, y el motor CRUD no
// las tiene:
//
//   - "Agendar turno" tiene que pasar por `ServicioTurnos::agendar`, que
//     aplica TODAS las reglas de negocio (el paciente existe y esta
//     activo, el medico tambien, el turno es de la especialidad del
//     medico, la fecha no paso, la hora esta dentro del horario de
//     atencion y el horario no esta ocupado). El `alta` del motor CRUD se
//     saltaria TODAS esas reglas.
//
//   - "Cancelar turno" no es una baja: es un CAMBIO DE ESTADO, con su
//     propia regla (un turno que no esta activo no se puede cancelar).
//
//   - "Reprogramar turno" crea un turno NUEVO y deja el viejo como
//     `Reprogramado`: el motor CRUD no tiene ninguna accion que haga eso.
//
//   - "Modificar turno" cambia el consultorio, la duracion y la
//     especialidad, pero NO la fecha ni la hora (moverlas es reprogramar).
//
// EL FLUJO DE "AGENDAR": LA ESPECIALIDAD ANTES DEL MEDICO
//
// Agendar es: paciente -> especialidad -> medico -> formulario. La
// especialidad se elige ANTES del medico, asi que el buscador de medicos
// sale FILTRADO por ella y el turno sale con la especialidad del medico POR
// CONSTRUCCION: la regla 2b no se puede violar desde la interfaz.
//
// "Modificar turno" NO cambia: ahi la especialidad se sigue preguntando al
// final del formulario, porque ahi se corrige el dato HISTORICO del turno
// (decision D21).
//
// LA PANTALLA NO REIMPLEMENTA NINGUNA REGLA
//
// La pantalla arma los datos y se los pasa al servicio. Si el servicio
// rechaza la operacion, la pantalla muestra EXACTAMENTE el mensaje de la
// excepcion (`ERR <mensaje>`). Las reglas viven en el servicio, que es
// donde estan: una pantalla que las copiara tendria DOS reglas que se
// pueden desincronizar.
//
// EL '0' ES VOLVER, EN TODOS LOS NIVELES
//
// Es la regla de UX del proyecto. En las vistas con un CAMPO de texto el
// '0' puede ser un DATO (un id puede ser 10), asi que ahi el '0' solo es
// "volver" cuando el campo esta VACIO: si hay algo escrito, el '0' es
// parte del dato.
//
// EL `Escape` SUBE UN NIVEL
//
// En las vistas internas (el id, el formulario, el selector y la
// confirmacion) el `Escape` sube UN nivel. En el menu de la pantalla NO
// se consume: ahi lo maneja la aplicacion, que saca la pantalla entera de
// la pila.
//
// ---------------------------------------------------------------------------
namespace ui {

namespace Pantallas {

class PantallaTurnos : public Pantalla {
public:
    PantallaTurnos(Aplicacion& aplicacion,
                   RepositorioTurnos& turnos,
                   RepositorioPacientes& pacientes,
                   RepositorioMedicos& medicos,
                   ServicioTurnos& servicioTurnos);

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
    // palabras y no numeros.
    enum class Vista {
        Menu,
        Busqueda,
        Formulario,
        Selector,
        Confirmar,
        Listado,
        Criterio,
        Valor,
        Resultados,
        Detalle,
        ConfirmarEjemplo
    };

    // Que se hace con el turno que se elige
    //
    // El TURNO es el PRIMER paso de cuatro de las acciones: el mismo
    // buscador sirve para cancelar, reprogramar, modificar y ver el detalle,
    // y lo que cambia despues es lo que se hace con el turno encontrado.
    enum class Accion { Agendar, Cancelar, Reprogramar, Modificar, Detalle };

    // EL PASO DEL BUSCADOR
    //
    // El buscador de TURNOS elige un turno (cancelar, reprogramar, modificar
    // y ver el detalle). El de AGENDAR elige a una PERSONA, y lo hace en DOS
    // pasos: primero el PACIENTE y despues el MEDICO.
    //
    // UN SOLO BUSCADOR PARA LOS TRES
    //
    // Los pasos son consecutivos: cuando se elige el paciente, el buscador se
    // vuelve a abrir para el medico. Un solo widget alcanza: lo que cambia
    // es DE QUIEN se esta eligiendo.
    //
    // ENTRE LOS DOS PASOS ESTA EL SELECTOR DE LA ESPECIALIDAD
    //
    // El orden es paciente -> especialidad -> medico: el buscador del
    // medico se abre DESPUES de elegir la especialidad, y sale filtrado por
    // ella.
    enum class PasoDelBuscador { Turno, Paciente, Medico };

    // LOS CRITERIOS DE BUSQUEDA
    //
    // Son seis: el id, el paciente, el medico, la especialidad, el
    // consultorio y el estado. La busqueda es de DOS PASOS (decision 5):
    // primero se elige el criterio y despues se escribe el valor.
    enum class Criterio { Id, Paciente, Medico, Especialidad, Consultorio, Estado };

    // LOS DATOS QUE ESCRIBE EL FORMULARIO
    //
    // Es una estructura con campos CON NOMBRE (y no un vector de textos)
    // porque cada accion tiene su propio formulario: con los indices
    // sueltos, agregar un campo a un formulario movia los valores de otro
    // sin que nadie lo notara.
    struct DatosFormulario {
        // EL PACIENTE Y EL MEDICO ELEGIDOS CON EL BUSCADOR
        //
        // No son campos del formulario: los eligio el usuario en los dos
        // buscadores de "agendar" (-1 si no se eligio ninguno).
        int idPaciente = -1;
        int idMedico = -1;
        std::string fecha;
        std::string hora;
        std::string duracion;
        std::string consultorio;
        // El CODIGO de la especialidad ("CARD"): lo elige el selector.
        //
        // En "modificar" lo elige el selector que va DESPUES del formulario;
        // en "agendar" lo pone la especialidad que ya se habia elegido antes
        // del medico.
        std::string especialidad;
    };

    // La fila de la pantalla donde se dibuja la PRIMERA opcion de la
    // confirmacion (la del "Si").
    //
    // La pantalla dibuja las dos opciones en dos filas conocidas y se lo
    // dice a la `ui::Confirmacion` (`definirFilaInicial`): sin esto, un clic
    // en el ENCABEZADO caeria en la primera opcion y confirmaria una
    // cancelacion sin querer.
    static constexpr int FILA_INICIAL_CONFIRMACION = 3;

    static const std::string& nombreDeVista(Vista vista);

    // --- La navegacion ---
    void irA(Vista vista);
    void volverAtras();
    bool esTeclaVolver(const Evento& evento) const;
    bool esTeclaEscape(const Evento& evento) const;
    bool esTeclaVolverConCampoVacio(const Evento& evento, const Campo& campo) const;

    // --- Los manejadores de cada vista ---
    bool manejarMenu(const Evento& evento);
    bool manejarFormulario(const Evento& evento);
    bool manejarSelector(const Evento& evento);
    bool manejarBusqueda(const Evento& evento);
    bool manejarCampo(const Evento& evento, const std::unique_ptr<Campo>& campo,
                      const std::function<void()>& alConfirmar);
    bool manejarConfirmacion(const Evento& evento);
    bool manejarConfirmacionEjemplo(const Evento& evento);
    bool manejarListado(const Evento& evento);
    bool manejarMenuCriterios(const Evento& evento);
    bool manejarLectura(const Evento& evento);

    // --- Las acciones ---
    void iniciarAgendar();
    void abrirFormulario(std::vector<std::unique_ptr<Campo>> campos);
    void confirmarFormulario();
    void moverFoco(int delta);
    DatosFormulario datosDelFormulario() const;
    bool laAccionTieneSelector() const;

    // --- El buscador de personas ---
    //
    // EL PACIENTE Y EL MEDICO SE BUSCAN, NO SE ESCRIBEN
    //
    // Los dos ids que pedia "agendar" son claves de un archivo: el usuario
    // no las tiene en la cabeza. Se escriben los datos que el usuario CONOCE
    // (el nombre, el apellido, el dni, el legajo) y se elige UNA de las
    // coincidencias.
    //
    // LAS MISMAS REGLAS QUE LAS PANTALLAS DE PACIENTES Y DE MEDICOS
    //
    // Los criterios son los compartidos (ver `ui::CriteriosDePersonas.h`):
    // buscar un paciente tiene que comportarse igual en todas las pantallas.
    //
    // EL BUSCADOR DE MEDICOS DE ESTA PANTALLA FILTRA POR ESPECIALIDAD
    //
    // Es el filtro del flujo de "agendar" (la especialidad se elige antes
    // del medico). Los buscadores de medicos de las OTRAS pantallas no se
    // filtran: la regla compartida no se toca.
    void abrirBuscadorDePacientes();
    void abrirBuscadorDeMedicos();
    void confirmarPersona();
    void abrirFormularioDeAgendar();
    std::vector<Buscador::Coincidencia> coincidenciasDePacientes(const std::string& texto);
    std::vector<Buscador::Coincidencia> coincidenciasDeMedicos(const std::string& texto);

    // --- El buscador de turnos ---
    //
    // EL TURNO SE BUSCA, NO SE ESCRIBE
    //
    // El turno ya NO se pide por id: se BUSCA. El `ui::Buscador` corre la
    // regla de la pantalla (el texto se busca en la fila completa del turno)
    // y devuelve el id del turno elegido.
    void abrirBuscador(Accion accion);
    void confirmarId();
    std::vector<Buscador::Coincidencia> coincidenciasDelBuscador(const std::string& texto);
    bool laAccionPuedeOperar(const Turno& turno) const;
    bool laFilaContiene(const Turno& turno, const std::string& buscado) const;

    // --- El selector ---
    //
    // EL MISMO SELECTOR PARA LOS DOS FLUJOS
    //
    // En "agendar" elige la especialidad que filtra el buscador de medicos;
    // en "modificar" elige la especialidad que se le pone al turno. Es el
    // mismo widget y la misma lista: lo que cambia es lo que se hace con
    // lo elegido.
    void abrirSelector();
    void confirmarSeleccion();

    // --- Lo que hace cada accion ---
    void ejecutarAccion(const DatosFormulario& datos);
    void agendarTurno(const DatosFormulario& datos);
    void reprogramarTurno(const DatosFormulario& datos);
    void modificarTurno(const DatosFormulario& datos);
    void ejecutarCancelacion();

    // --- La busqueda ---
    void abrirCriterio();
    void iniciarBusqueda(Criterio criterio);
    void confirmarValor();
    std::vector<std::vector<std::string>> buscarPorCriterio(Criterio criterio,
                                                              const std::string& valor) const;
    std::string etiquetaDeCriterio(Criterio criterio) const;
    Campo::Validador validadorDeCriterio(Criterio criterio) const;

    // --- El estado vacio con ACCION ---
    //
    // Un listado sin turnos no es un mensaje: es un menu con dos opciones
    // ("cargar ejemplo" y "volver"). La logica de la carga vive en el
    // helper compartido (`ui::cargarDatosDeEjemplo`); la pantalla solo la
    // pide y muestra el resultado.
    void pedirCargaDeEjemplo();
    void ejecutarCargaDeEjemplo();

    // --- Los mensajes ---
    void avisar(const std::string& texto);
    void avisarExito(const std::string& texto);
    const Mensaje& mensajeActual() const;

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
    ftxui::Element lineaDetalle() const;
    ftxui::Element tablaDe(const std::vector<std::vector<std::string>>& filas,
                           const std::vector<std::string>& cabeceras) const;
    std::string vacio(const std::string& queFalta) const;
    std::string ayuda() const;

    // --- El texto plano (lo que se ve sin terminal) ---
    std::string textoDeLaVista() const;

    Aplicacion& _aplicacion;
    RepositorioTurnos& _turnos;
    RepositorioPacientes& _pacientes;
    RepositorioMedicos& _medicos;
    ServicioTurnos& _servicioTurnos;
    Theme _tema;
    std::string _titulo;
    Menu _menu;
    Menu _menuCriterios;

    // EL MENU DEL ESTADO VACIO
    //
    // Es el menu de DOS opciones que aparece cuando el listado no tiene
    // turnos: "cargar ejemplo" y "volver".
    Menu _menuVacio;

    Vista _vista = Vista::Menu;
    Accion _accion = Accion::Agendar;

    // El criterio de busqueda elegido y las filas del resultado.
    Criterio _criterioElegido = Criterio::Id;
    std::vector<std::vector<std::string>> _filasResultado;

    // Los campos del formulario y el que tiene el foco.
    std::vector<std::unique_ptr<Campo>> _campos;
    std::size_t _indiceCampo = 0;

    // El selector de la especialidad y los datos que esperan su eleccion.
    //
    // Los datos quedan ESPERANDO porque el selector es un paso DEL
    // formulario: si el usuario vuelve atras (con el `Escape`), lo que
    // escribio sigue ahi.
    std::unique_ptr<Selector> _selector;
    DatosFormulario _pendientes;

    // El indice de la especialidad elegida en el catalogo.
    //
    // Se recuerda aparte del selector para que, si una operacion es
    // rechazada por el servicio y el usuario vuelve al formulario, la
    // especialidad que habia elegido siga siendo la que arranca elegida.
    int _indiceEspecialidad = 0;

    // LA ESPECIALIDAD QUE FILTRA EL BUSCADOR DE MEDICOS
    //
    // Es la que se elige en el paso del selector del flujo de "agendar": el
    // buscador de medicos muestra SOLO los de esa especialidad, asi que el
    // turno sale con la del medico POR CONSTRUCCION (la regla 2b no se
    // puede violar desde la interfaz).
    //
    // Se guarda como `Especialidad` (y no como el indice del selector): es
    // el dato que viaja al turno, y el indice es solo la forma de pedirlo.
    //
    // VA SIN VALOR HASTA QUE SE ELIGE UNA: antes de ese paso no hay filtro
    // que aplicar (el buscador de medicos todavia no se abrio).
    std::optional<Especialidad> _especialidadElegida;

    // El buscador de turnos (el de "Cancelar", "Reprogramar", "Modificar" y
    // "Ver detalle"), y el de las personas (el de "Agendar turno").
    std::unique_ptr<Buscador> _buscador;

    // De que se esta eligiendo la entidad en el buscador.
    PasoDelBuscador _pasoDelBuscador = PasoDelBuscador::Turno;

    // EL PACIENTE Y EL MEDICO ELEGIDOS CON EL BUSCADOR
    //
    // Son los ids que van al turno agendado. Los elige el usuario en los dos
    // buscadores de "agendar": son claves de un archivo, no un dato que
    // alguien pueda escribir.
    int _idPacienteElegido = -1;
    int _idMedicoElegido = -1;

    // El campo del valor que se busca.
    std::unique_ptr<Campo> _campoValor;

    // La confirmacion de la cancelacion.
    std::unique_ptr<Confirmacion> _confirmacion;

    // La confirmacion de la carga de datos de ejemplo.
    std::unique_ptr<Confirmacion> _confirmacionEjemplo;

    // El turno de la operacion en curso.
    Turno _turno;
    int _idTurno = -1;
};

// La pantalla de turnos, lista para apilarse en la aplicacion.
std::unique_ptr<Pantalla> crearPantallaTurnos(Aplicacion& aplicacion,
                                               RepositorioTurnos& turnos,
                                               RepositorioPacientes& pacientes,
                                               RepositorioMedicos& medicos,
                                               ServicioTurnos& servicioTurnos);

} // namespace Pantallas
} // namespace ui
