#include "ui/Pantallas/Turnos.h"

#include "domain/Especialidad.h"
#include "exceptions/ExcepcionesSistema.h"
#include "ui/CrudPersonas.h"
#include "utils/Fecha.h"
#include "utils/Utils.h"

#include <cstddef>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// La pantalla de TURNOS.
//
// Lo que esta en el header es el POR QUE; lo que esta aca es el COMO.
//
// EL FLUJO DE UNA ACCION
//
// Todas las acciones empiezan en el menu y pasan por el id (salvo
// "agendar", que no tiene id: el turno es nuevo). De ahi salen dos
// caminos:
//
//   - el camino CORTO (cancelar): id -> confirmacion -> la accion.
//
//   - el camino LARGO (agendar, reprogramar, modificar): id -> formulario
//     -> [selector] -> la accion.
//
// El paso del SELECTOR es opcional: solo lo tienen las acciones en las que
// la especialidad se ELIGE (agendar y modificar). Es el mismo criterio que
// usa el motor CRUD con los medicos.
//
// LA PANTALLA NO REIMPLEMENTA NINGUNA REGLA
//
// Los validadores de los campos le PREGUNTAN al dominio (a `Fecha`, a
// `Hora` y a los setters de `Turno`) si el dato sirve, y las reglas de
// negocio (el paciente existe, el medico esta activo, la especialidad
// corresponde, el horario no esta ocupado) las aplica el SERVICIO. La
// pantalla solo muestra lo que el servicio dice.
//
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {
namespace {

// LA ETIQUETA DEL CAMPO DE LA HORA
//
// Es la que se usa en el formulario de agendar y en el de reprogramar. El
// nombre importa: el '0' de un campo VACIO es "volver", pero el de la hora
// es un dato (ver `PantallaTurnos::esTeclaVolverConCampoVacio`).
const char* ETIQUETA_HORA = "hora";

// ---------------------------------------------------------------------------
// LOS VALIDADORES DE LOS CAMPOS
//
// LA REGLA ES LA DEL DOMINIO, NO LA DE LA PANTALLA
//
// Cada validador le PREGUNTA al dominio si el texto sirve y devuelve el
// motivo del rechazo. La pantalla no reescribe ninguna regla: si lo
// hiciera, habria dos reglas que se pueden desincronizar.
//
// LO QUE EL VALIDADOR DECIDE Y LO QUE NO
//
// El campo del formulario decide la FORMA del dato (que la fecha se vea
// como AAAA-MM-DD, que la hora se vea como HH:MM, que la duracion sea un
// numero). El RANGO (que la fecha exista, que la hora este en el horario
// de atencion, que la duracion sea multiplo de 15) NO lo decide el campo:
// lo decide el TURNO, y el error sale como MENSAJE (`ERR ...`), que es el
// canal de los errores de la aplicacion.
//
// POR QUE NO SE VALIDA EL RANGO EN EL CAMPO
//
// El mensaje del rango es el del turno ("fuera del horario de
// atencion..."), que es el mismo que usa el servicio. Si el campo lo
// rechazara, el usuario veria ese mismo texto DEBAJO del campo y el
// mensaje no seria uno solo en toda la aplicacion. Con el campo validando
// solo la forma, el error del rango sale SIEMPRE como `ERR <mensaje>`.
//
// LO QUE EL VALIDADOR NO DECIDE NUNCA
//
// Que la entidad EXISTE o que este ACTIVA es otra pregunta: la responden
// el repositorio y el servicio, y el error que muestran es el de ellos.
// ---------------------------------------------------------------------------

// Un id es un entero POSITIVO. Que exista es otra pregunta.
Campo::Validador validadorDeId() {
    return [](const std::string& texto) -> std::string {
        if (idDesdeTexto(texto) < 0) {
            return "el id tiene que ser un numero entero mayor que cero";
        }
        return "";
    };
}

// La fecha tiene que tener FORMA AAAA-MM-DD.
//
// Que la fecha EXISTA en el calendario no lo decide el campo: lo decide el
// turno (que dice "fecha inexistente").
Campo::Validador validadorDeFecha() {
    return [](const std::string& texto) -> std::string {
        if (!Utils::esFormatoFecha(texto)) {
            return "se esperaba AAAA-MM-DD";
        }
        return "";
    };
}

// La hora tiene que tener FORMA HH:MM.
//
// Que la hora este en el HORARIO DE ATENCION no lo decide el campo: lo
// decide el turno (que dice "fuera del horario de atencion...").
Campo::Validador validadorDeHora() {
    return [](const std::string& texto) -> std::string {
        if (!Utils::esFormatoHora(texto)) {
            return "se esperaba HH:MM";
        }
        return "";
    };
}

// La duracion tiene que ser un numero ENTERO de minutos.
//
// Que sea multiplo de 15 y de 240 minutos como maximo no lo decide el
// campo: lo decide el turno (que dice "se esperaba una duracion positiva,
// multiplo de 15...").
Campo::Validador validadorDeDuracion() {
    return [](const std::string& texto) -> std::string {
        if (!Utils::esEntero(texto)) {
            return "se esperaba un numero entero de minutos";
        }
        return "";
    };
}

// El consultorio no puede estar VACIO.
//
// Que tenga de 1 a 20 caracteres, con letras, digitos, espacios, '-' y
// '.', no lo decide el campo: lo decide el turno (que dice "se esperaba un
// consultorio de 1 a 20 caracteres...").
Campo::Validador validadorDeConsultorio() {
    return [](const std::string& texto) -> std::string {
        if (!Utils::esNoVacio(texto)) {
            return "el consultorio no puede estar vacio";
        }
        return "";
    };
}

// ---------------------------------------------------------------------------
// LA FECHA Y LA HORA QUE SE LE PIDEN AL TURNO
//
// EL TEXTO YA TIENE LA FORMA
//
// `Utils::esFormatoFecha` y `Utils::esFormatoHora` dicen si el texto tiene
// forma AAAA-MM-DD y HH:MM: es la validacion de FORMA, la que le toca al
// campo del formulario. El RANGO (si la fecha existe, si la hora esta en el
// horario de atencion) es otra pregunta, y la responde el TURNO.
//
// POR QUE SE ARMA EL VALOR CRUDO Y NO CON `desdeTexto`
//
// `Fecha::desdeTexto` y `Hora::desdeTexto` rechazan tambien los valores
// fuera de rango, con un mensaje PROPIO ("se esperaba HH:MM con una hora
// valida"). Lo que el usuario tiene que leer es el del TURNO ("fuera del
// horario de atencion..."), que es el mismo que usa el servicio: por eso
// aca se arma el valor CRUDO y lo deja que lo valide el turno. Asi el
// mensaje es UNO en toda la aplicacion, este o no el dato venga de la
// pantalla o de un archivo.
//
// El texto tiene la forma (el campo la valido), asi que el corte es fijo.
// ---------------------------------------------------------------------------
Fecha fechaCruda(const std::string& texto) {
    return Fecha(std::stoi(texto.substr(0, 4)), std::stoi(texto.substr(5, 2)),
                 std::stoi(texto.substr(8, 2)));
}

Hora horaCruda(const std::string& texto) {
    return Hora(std::stoi(texto.substr(0, 2)), std::stoi(texto.substr(3, 2)));
}

// ---------------------------------------------------------------------------
// EL INDICE DE UNA ESPECIALIDAD EN EL CATALOGO
//
// El numero del `enum class` NO es el indice del catalogo (el catalogo es
// una tabla y el enum su declaracion): se comparan las especialidades,
// que son la clave del catalogo.
// ---------------------------------------------------------------------------
int indiceDeEspecialidad(Especialidad especialidad) {
    const std::vector<Especialidad>& catalogo = todasLasEspecialidades();
    for (std::size_t i = 0; i < catalogo.size(); ++i) {
        if (catalogo[i] == especialidad) {
            return static_cast<int>(i);
        }
    }
    // No esta en el catalogo: se deja la primera (el `ui::Selector` ignora
    // un indice fuera de rango).
    return 0;
}

// ---------------------------------------------------------------------------
// LAS COLUMNAS Y LAS FILAS DE UN TURNO
//
// LA LISTA SE ENFOCA EN SEIS COLUMNAS
//
// El listado y los resultados de una busqueda muestran SEIS columnas: `id`,
// `fecha`, `hora`, `paciente`, `medico` y `estado`. Son las que hacen falta
// para RECONOCER un turno, y son las que entran en un terminal de 80
// columnas.
//
// QUE SE SACO DE LA LISTA Y POR QUE
//
// De las diez columnas de la especificacion (issue #16, seccion 5.4, T5):
//
//   - `dia` se DEDUCE de la fecha: la fecha sola ya dice si es un dia de
//     trabajo, y el nombre del dia es un dato DERIVADO;
//   - `especialidad` es, por la regla 2b, la del MEDICO que lo atiende:
//     repetirla en cada fila es repetir un dato que ya esta en otra columna;
//   - `duracion` tiene un rotulo de ocho letras para un dato de dos, y
//     `consultorio` tampoco ayuda a reconocer un turno.
//
// LAS CUATRO QUE SALEN NO SE PIERDEN
//
// La pantalla de DETALLE sigue mostrando las DIEZ columnas de la
// especificacion: el listado se enfoca, la ficha no.
//
// LA MEDIDA
//
// Con los datos de la semilla, la tabla de diez columnas mide 137 columnas:
// 106 de contenido, 20 de padding (dos por columna) y 11 de bordes (el
// separador que `ui::Tabla` dibuja DESPUES de cada columna). En un terminal
// de 80 quedaba cortada, y la columna `estado` no se veia.
//
// Con las seis columnas de la lista mide 79: 60 de contenido, 12 de padding
// y 7 de bordes. Entra.
//
// Y hay un test que lo verifica: el ancho natural de la vista "Listar todos"
// no puede pasar de 80 columnas.
// ---------------------------------------------------------------------------

// LAS COLUMNAS DE LA LISTA
const std::vector<std::string>& cabecerasDeTurno() {
    static const std::vector<std::string> cabeceras = {"id", "fecha", "hora", "paciente",
                                                        "medico", "estado"};
    return cabeceras;
}

// LAS COLUMNAS DE LA FICHA
//
// Son las DIEZ de la especificacion: la ficha muestra el turno entero.
const std::vector<std::string>& cabecerasCompletasDeTurno() {
    static const std::vector<std::string> cabeceras = {"id",      "paciente", "medico",
                                                        "especialidad", "fecha",   "dia",
                                                        "hora",   "duracion", "consultorio",
                                                        "estado"};
    return cabeceras;
}

// EL NOMBRE DEL PACIENTE Y DEL MEDICO
//
// El turno guarda los IDS del paciente y del medico, no sus nombres: la
// fila los resuelve con el repositorio. Es el mismo nombre que se ve en
// las tablas de las otras pantallas.
//
// CUANDO NO EXISTEN
//
// Se muestra el id con un "#" adelante: un dato que falta tiene que
// verse, no desaparecer.
std::string nombreDelPaciente(int id, RepositorioPacientes& pacientes) {
    Paciente paciente;
    if (!pacientes.buscarPorId(id, paciente)) {
        return "#" + std::to_string(id);
    }
    return paciente.getNombre() + " " + paciente.getApellido();
}

std::string nombreDelMedico(int id, RepositorioMedicos& medicos) {
    Medico medico;
    if (!medicos.buscarPorId(id, medico)) {
        return "#" + std::to_string(id);
    }
    return medico.getNombre() + " " + medico.getApellido();
}

// LA FILA DE UN TURNO EN LA LISTA
//
// Las celdas van en el MISMO orden que `cabecerasDeTurno()`: si no, la
// tabla queda desalineada.
std::vector<std::string> filaDe(const Turno& turno, RepositorioPacientes& pacientes,
                                RepositorioMedicos& medicos) {
    return {std::to_string(turno.getId()),
            turno.getFecha().aTexto(),
            turno.getHora().aTexto(),
            nombreDelPaciente(turno.getIdPaciente(), pacientes),
            nombreDelMedico(turno.getIdMedico(), medicos),
            estadoATexto(turno.getEstado())};
}

// LA FILA DE UN TURNO EN LA FICHA
//
// Son los DIEZ campos de la especificacion, en el orden de
// `cabecerasCompletasDeTurno()`.
//
// LA ESPECIALIDAD SE MUESTRA CON SU DESCRIPCION
//
// Es lo que el usuario lee ("Cardiologia"): el CODIGO ("CARD") es el dato
// que viaja en el archivo.
//
// EL DIA DE LA SEMANA
//
// Es el nombre del dia ("miercoles"): la fecha sola no dice si es un dia
// de trabajo.
std::vector<std::string> filaCompletaDe(const Turno& turno, RepositorioPacientes& pacientes,
                                         RepositorioMedicos& medicos) {
    return {std::to_string(turno.getId()),
            nombreDelPaciente(turno.getIdPaciente(), pacientes),
            nombreDelMedico(turno.getIdMedico(), medicos),
            descripcionDe(turno.getEspecialidad()),
            turno.getFecha().aTexto(),
            Fecha::nombreDiaSemana(turno.getFecha().diaSemana()),
            turno.getHora().aTexto(),
            std::to_string(turno.getDuracionMin()),
            turno.getConsultorio(),
            estadoATexto(turno.getEstado())};
}

// EL RESUMEN DE UN TURNO
//
// Una linea con lo que hace falta para RECONOCERLO: el id, la fecha, la
// hora, el estado y el consultorio. Es lo que se usa para nombrar al turno
// que reemplaza a otro (y al que lo reemplaza) sin repetir la tabla
// entera.
std::string resumenDe(const Turno& turno) {
    return "#" + std::to_string(turno.getId()) + " (" + turno.getFecha().aTexto() + " "
        + turno.getHora().aTexto() + ", " + estadoATexto(turno.getEstado()) + ", "
        + turno.getConsultorio() + ")";
}

} // namespace

// ---------------------------------------------------------------------------
// Construccion
// ---------------------------------------------------------------------------

PantallaTurnos::PantallaTurnos(Aplicacion& aplicacion,
                               RepositorioTurnos& turnos,
                               RepositorioPacientes& pacientes,
                               RepositorioMedicos& medicos,
                               ServicioTurnos& servicioTurnos)
    : _aplicacion(aplicacion),
      _turnos(turnos),
      _pacientes(pacientes),
      _medicos(medicos),
      _servicioTurnos(servicioTurnos),
      _titulo("TURNOS"),
      _menu("Turnos"),
      _menuCriterios("Buscar turnos") {
    // EL MENU DEL MODULO
    //
    // El atajo es el numero que se escribe (decision D28) y el '0' es
    // "Volver" en todos los niveles.
    _menu.agregar('1', "Agendar turno", [this]() { iniciarAgendar(); });
    _menu.agregar('2', "Cancelar turno", [this]() { iniciarConId(Accion::Cancelar); });
    _menu.agregar('3', "Reprogramar turno", [this]() { iniciarConId(Accion::Reprogramar); });
    _menu.agregar('4', "Modificar turno", [this]() { iniciarConId(Accion::Modificar); });
    _menu.agregar('5', "Listar todos", [this]() { irA(Vista::Listado); });
    _menu.agregar('6', "Buscar", [this]() { abrirCriterio(); });
    _menu.agregar('7', "Ver detalle", [this]() { iniciarConId(Accion::Detalle); });
    _menu.agregar('0', "Volver", [this]() { volverAtras(); });

    // EL MENU DE CRITERIOS
    //
    // La busqueda es de DOS PASOS (decision 5): primero se elige el
    // criterio y despues se escribe el valor. Los criterios salen en el
    // orden de la especificacion y su atajo es la posicion.
    static const Criterio CRITERIOS[] = {Criterio::Id,           Criterio::Paciente,
                                         Criterio::Medico,       Criterio::Especialidad,
                                         Criterio::Consultorio,  Criterio::Estado};
    for (std::size_t i = 0; i < std::size(CRITERIOS); ++i) {
        _menuCriterios.agregar(static_cast<char>('1' + static_cast<int>(i)),
                                etiquetaDeCriterio(CRITERIOS[i]),
                                [this, criterio = CRITERIOS[i]]() { iniciarBusqueda(criterio); });
    }
    _menuCriterios.agregar('0', "Volver", [this]() { volverAtras(); });
}

std::string PantallaTurnos::titulo() const {
    return _titulo;
}

// ---------------------------------------------------------------------------
// La navegacion
// ---------------------------------------------------------------------------

void PantallaTurnos::irA(Vista vista) {
    _vista = vista;
}

void PantallaTurnos::volverAtras() {
    // EL "VOLVER" SUBE UN NIVEL
    //
    //   - desde el menu de la pantalla, se sale de la pantalla (el menu
    //     principal esta en la pila de la aplicacion);
    //   - desde la confirmacion, se vuelve a la vista del id;
    //   - desde el selector, se vuelve al formulario (el selector es un
    //     paso del formulario, no un nivel del menu);
    //   - desde el valor de la busqueda, se vuelve al menu de criterios
    //     (la busqueda es de dos pasos: criterio y despues valor);
    //   - desde cualquier otra vista, se vuelve al menu.
    switch (_vista) {
        case Vista::Menu:
            _aplicacion.volver();
            return;
        case Vista::Confirmar:
            irA(Vista::Id);
            return;
        case Vista::Selector:
            irA(Vista::Formulario);
            return;
        case Vista::Valor:
            irA(Vista::Criterio);
            return;
        default:
            irA(Vista::Menu);
            return;
    }
}

bool PantallaTurnos::esTeclaVolver(const Evento& evento) const {
    return evento.tipo == TipoTecla::Caracter && evento.caracter == "0";
}

bool PantallaTurnos::esTeclaEscape(const Evento& evento) const {
    return evento.tipo == TipoTecla::Escape;
}

// EL '0' ES "VOLVER" SOLO SI EL CAMPO ESTA VACIO
//
// Con algo escrito el '0' es un DATO (un id puede ser 10) y se lo queda el
// campo.
//
// LA HORA ES LA EXCEPCION
//
// La hora SIEMPRE empieza con un cero ("08:00", "09:00"): si el '0' fuera
// "volver" con el campo vacio, esas horas NO SE PODRIAN ESCRIBIR (el
// usuario tendria que escribir el resto del horario para esquivarlo).
//
// Asi que, en el campo de la hora, el '0' es SIEMPRE un dato. El "volver"
// de ese campo es el `Escape`, que es la misma tecla que funciona en todos
// los demas niveles.
//
// Los demas campos (los ids, la fecha, la duracion y el consultorio) no
// empiezan con un cero: el '0' es "volver" cuando estan vacios, como en el
// resto de la aplicacion.
bool PantallaTurnos::esTeclaVolverConCampoVacio(const Evento& evento,
                                                 const Campo& campo) const {
    if (campo.etiqueta() == ETIQUETA_HORA) {
        return false;
    }
    return esTeclaVolver(evento) && campo.texto().empty();
}

// ---------------------------------------------------------------------------
// Los manejadores de cada vista
// ---------------------------------------------------------------------------

bool PantallaTurnos::manejar(const Evento& evento) {
    switch (_vista) {
        case Vista::Menu:
            return manejarMenu(evento);
        case Vista::Id:
            return manejarCampo(evento, _campoId, [this]() { confirmarId(); });
        case Vista::Formulario:
            return manejarFormulario(evento);
        case Vista::Selector:
            return manejarSelector(evento);
        case Vista::Confirmar:
            return manejarConfirmacion(evento);
        case Vista::Criterio:
            return manejarMenuCriterios(evento);
        case Vista::Valor:
            return manejarCampo(evento, _campoValor, [this]() { confirmarValor(); });
        case Vista::Listado:
        case Vista::Resultados:
        case Vista::Detalle:
            return manejarLectura(evento);
    }
    return false;
}

bool PantallaTurnos::manejarMenu(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` NO SE CONSUME EN EL MENU
    //
    // El menu de la pantalla es el NIVEL MAS ALTO de esta pantalla: desde
    // ahi el `Escape` tiene que sacar la pantalla ENTERA de la pila, y eso
    // lo hace la aplicacion. Si la pantalla lo consumiera, el efecto seria
    // el mismo pero quedaria repartido en dos lugares.
    return _menu.aplicarEvento(evento);
}

bool PantallaTurnos::manejarCampo(const Evento& evento,
                                   const std::unique_ptr<Campo>& campo,
                                   const std::function<void()>& alConfirmar) {
    if (!campo) {
        return false;
    }

    if (evento.tipo == TipoTecla::Enter) {
        // El campo valida y, si el valor sirve, se confirma la vista.
        campo->aplicarEvento(evento);
        if (!campo->confirmado()) {
            // El campo ya mostro el error debajo: no se sigue adelante.
            return true;
        }
        alConfirmar();
        return true;
    }

    // EL `Escape` SUBE UN NIVEL
    //
    // El `Escape` es una tecla de navegacion, no un dato: se atiende aunque
    // el campo tenga texto escrito.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }

    // EL '0' ES "VOLVER" SOLO SI EL CAMPO ESTA VACIO
    if (esTeclaVolverConCampoVacio(evento, *campo)) {
        volverAtras();
        return true;
    }

    return campo->aplicarEvento(evento);
}

bool PantallaTurnos::manejarFormulario(const Evento& evento) {
    if (_campos.empty()) {
        return false;
    }

    // EL FOCO SE MUEVE CON EL TAB (y con las flechas arriba y abajo)
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
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }

    // EL '0' ES "VOLVER" SOLO SI EL CAMPO ESTA VACIO
    if (esTeclaVolverConCampoVacio(evento, *_campos[_indiceCampo])) {
        volverAtras();
        return true;
    }

    return _campos[_indiceCampo]->aplicarEvento(evento);
}

bool PantallaTurnos::manejarSelector(const Evento& evento) {
    if (!_selector) {
        return false;
    }

    // EL `Escape` VUELVE AL FORMULARIO
    //
    // El selector es un paso DEL formulario: el `Escape` sube al formulario,
    // con todo lo que el usuario escribio intacto.
    if (esTeclaEscape(evento)) {
        irA(Vista::Formulario);
        return true;
    }

    // EL RATON MUEVE EL RESALTADO, NO CONFIRMA
    //
    // Un clic elige la fila, pero la confirmacion es el Enter: un clic
    // accidental no cambia la especialidad de un turno.
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
    // En el selector el '0' es un DIGITO: es el numero de una opcion, y
    // como el catalogo tiene doce opciones (y no diez), el '0' no puede ser
    // un atajo de vuelta. Si no existe, el selector muestra el error.
    if (!_selector->aplicarEvento(evento)) {
        return false;
    }

    // EL ENTER CONFIRMA LA ELECCION
    if (evento.tipo == TipoTecla::Enter && _selector->error().empty()) {
        confirmarSeleccion();
    }
    return true;
}

bool PantallaTurnos::manejarConfirmacion(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE A LA VISTA DEL ID
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    if (!_confirmacion) {
        return false;
    }

    // EL RATON LO TRADUCE LA PANTALLA
    //
    // La `ui::Confirmacion` no sabe en que fila se dibujan sus opciones (no
    // expone su menu), asi que un clic caeria siempre en la primera: un
    // clic en el encabezado confirmaria una cancelacion sin querer. La
    // pantalla sabe donde las dibuja, asi que el clic se traduce al atajo.
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
        ejecutarCancelacion();
    } else {
        // Si NO confirma, se vuelve a la vista anterior (la del id).
        irA(Vista::Id);
    }
    return true;
}

bool PantallaTurnos::manejarMenuCriterios(const Evento& evento) {
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

bool PantallaTurnos::manejarLectura(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE AL MENU DE LA PANTALLA
    //
    // Las vistas de solo lectura (el listado, los resultados y la ficha)
    // no tienen ningun campo: el `Escape` sube un nivel, al menu.
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    // Las vistas de solo lectura no consumen nada mas.
    return false;
}

// ---------------------------------------------------------------------------
// Las acciones
// ---------------------------------------------------------------------------

void PantallaTurnos::iniciarAgendar() {
    _accion = Accion::Agendar;

    // EL FORMULARIO DE AGENDAR
    //
    // El id del turno NO se escribe: lo propone el repositorio
    // (`proximoId()`), que es la unica forma de que dos turnos no tengan
    // el mismo id.
    std::vector<std::unique_ptr<Campo>> campos;
    campos.push_back(std::make_unique<Campo>("id paciente", "", validadorDeId()));
    campos.push_back(std::make_unique<Campo>("id medico", "", validadorDeId()));
    campos.push_back(std::make_unique<Campo>("fecha", "", validadorDeFecha()));
    campos.push_back(std::make_unique<Campo>(ETIQUETA_HORA, "", validadorDeHora()));
    campos.push_back(std::make_unique<Campo>("duracion", "", validadorDeDuracion()));
    campos.push_back(std::make_unique<Campo>("consultorio", "", validadorDeConsultorio()));
    abrirFormulario(std::move(campos));
}

void PantallaTurnos::iniciarConId(Accion accion) {
    _accion = accion;
    _campoId = std::make_unique<Campo>("Id", "", validadorDeId());
    irA(Vista::Id);
}

void PantallaTurnos::abrirFormulario(std::vector<std::unique_ptr<Campo>> campos) {
    _campos = std::move(campos);
    _indiceCampo = 0;
    irA(Vista::Formulario);
}

void PantallaTurnos::moverFoco(int delta) {
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

bool PantallaTurnos::laAccionTieneSelector() const {
    // LA ESPECIALIDAD SE ELIGE, NO SE ESCRIBE
    //
    // En "agendar" y en "modificar" la especialidad es una de las doce del
    // catalogo: se elige de la lista, con su numero. Es el mismo criterio
    // que usa la pantalla de medicos (decision 1: el estado invalido es
    // irrepresentable).
    return _accion == Accion::Agendar || _accion == Accion::Modificar;
}

PantallaTurnos::DatosFormulario PantallaTurnos::datosDelFormulario() const {
    // LOS DATOS SE LEEN POR ETIQUETA
    //
    // Cada campo sabe como se llama ("id paciente", "fecha", ...), asi que
    // el formulario se arma sin indices sueltos: agregar un campo a un
    // formulario no mueve los valores de los otros.
    DatosFormulario datos;
    for (const std::unique_ptr<Campo>& campo : _campos) {
        const std::string& etiqueta = campo->etiqueta();
        const std::string valor = campo->valor();
        if (etiqueta == "id paciente") {
            datos.idPaciente = valor;
        } else if (etiqueta == "id medico") {
            datos.idMedico = valor;
        } else if (etiqueta == "fecha") {
            datos.fecha = valor;
        } else if (etiqueta == ETIQUETA_HORA) {
            datos.hora = valor;
        } else if (etiqueta == "duracion") {
            datos.duracion = valor;
        } else if (etiqueta == "consultorio") {
            datos.consultorio = valor;
        }
    }
    return datos;
}

void PantallaTurnos::confirmarFormulario() {
    // VALIDAR TODOS LOS CAMPOS
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

    _pendientes = datosDelFormulario();

    // CON SELECTOR: PRIMERO SE ELIGE, DESPUES SE HACE LA OPERACION
    if (laAccionTieneSelector()) {
        abrirSelector();
        return;
    }

    ejecutarAccion(_pendientes);
}

void PantallaTurnos::abrirSelector() {
    std::vector<std::string> opciones;
    for (const Especialidad& especialidad : todasLasEspecialidades()) {
        opciones.push_back(descripcionDe(especialidad));
    }

    _selector = std::make_unique<Selector>("especialidad", std::move(opciones));

    // LA ESPECIALIDAD QUE ARRANCA ELEGIDA
    //
    // Al MODIFICAR es la que ya tiene el turno; al AGENDAR es la primera
    // de la lista (aun no hay entidad que editar).
    _selector->seleccionar(_indiceEspecialidad);

    irA(Vista::Selector);
}

void PantallaTurnos::confirmarSeleccion() {
    if (!_selector) {
        irA(Vista::Menu);
        return;
    }
    const int indice = _selector->seleccion();
    if (indice < 0 || indice >= _selector->cantidad()) {
        // No deberia pasar: es un error de programacion de la pantalla.
        irA(Vista::Formulario);
        return;
    }

    // LA ESPECIALIDAD ELEGIDA SE GUARDA PARA PODER VOLVER A ABRIR EL
    // SELECTOR
    //
    // Si el servicio rechaza la operacion, el usuario vuelve al formulario
    // y puede abrir el selector otra vez: la especialidad que habia
    // elegido sigue siendo la que arranca elegida.
    _indiceEspecialidad = indice;
    _pendientes.especialidad = codigoDe(todasLasEspecialidades()[static_cast<std::size_t>(indice)]);

    ejecutarAccion(_pendientes);
}

// ---------------------------------------------------------------------------
// El id
// ---------------------------------------------------------------------------

void PantallaTurnos::confirmarId() {
    const int id = idDesdeTexto(_campoId->valor());
    Turno turno;
    if (id < 0 || !_turnos.buscarPorId(id, turno)) {
        // EL TURNO NO EXISTE
        //
        // El mensaje es el de la excepcion del DOMINIO: la pantalla no
        // escribe su propia version de "no existe".
        avisar(EntidadNoExisteException(id, "Turno").what());
        irA(Vista::Menu);
        return;
    }

    _idTurno = id;
    _turno = turno;

    switch (_accion) {
        case Accion::Cancelar:
            // LA CANCELACION PIDE CONFIRMACION
            //
            // Son dos pasos: primero el id, despues la confirmacion. El
            // turno se cancela SOLO cuando el usuario dice "Si".
            _confirmacion = std::make_unique<Confirmacion>("Cancelar el turno #"
                                                           + std::to_string(id) + "?");
            irA(Vista::Confirmar);
            return;

        case Accion::Reprogramar: {
            // LA FECHA Y LA HORA NUEVAS
            //
            // Los dos campos arrancan VACIOS: lo que se reprograma es una
            // fecha y una hora nuevas, y escribirlas de cero evita que el
            // usuario reprograme a la hora que ya tiene sin querer.
            std::vector<std::unique_ptr<Campo>> campos;
            campos.push_back(std::make_unique<Campo>("fecha", "", validadorDeFecha()));
            campos.push_back(std::make_unique<Campo>(ETIQUETA_HORA, "", validadorDeHora()));
            abrirFormulario(std::move(campos));
            return;
        }

        case Accion::Modificar: {
            // EL FORMULARIO DE MODIFICAR ARRANCA CON LOS DATOS VIEJOS
            //
            // El consultorio y la duracion son los que tiene el turno: lo
            // que se modifica es SU valor. La especialidad tambien
            // arranca con la del turno (en el selector).
            std::vector<std::unique_ptr<Campo>> campos;
            campos.push_back(std::make_unique<Campo>("consultorio", turno.getConsultorio(),
                                                     validadorDeConsultorio()));
            campos.push_back(std::make_unique<Campo>("duracion",
                                                     std::to_string(turno.getDuracionMin()),
                                                     validadorDeDuracion()));
            _indiceEspecialidad = indiceDeEspecialidad(turno.getEspecialidad());
            abrirFormulario(std::move(campos));
            return;
        }

        case Accion::Detalle:
            // LA FICHA DEL TURNO
            //
            // El turno ya esta cargado: la ficha lo muestra entero, con el
            // turno que lo reemplazo y el turno al que reemplazo.
            irA(Vista::Detalle);
            return;

        case Accion::Agendar:
            // No deberia llegar aca: "agendar" no pasa por el id.
            irA(Vista::Menu);
            return;
    }
}

// ---------------------------------------------------------------------------
// Lo que hace cada accion
// ---------------------------------------------------------------------------

void PantallaTurnos::ejecutarAccion(const DatosFormulario& datos) {
    try {
        switch (_accion) {
            case Accion::Agendar:
                agendarTurno(datos);
                return;
            case Accion::Reprogramar:
                reprogramarTurno(datos);
                return;
            case Accion::Modificar:
                modificarTurno(datos);
                return;
            case Accion::Cancelar:
                // No deberia llegar aca: la cancelacion tiene su propio
                // camino (la confirmacion).
                irA(Vista::Menu);
                return;
            case Accion::Detalle:
                // No deberia llegar aca: el detalle no pasa por el
                // formulario.
                irA(Vista::Menu);
                return;
        }
    } catch (const ExcepcionSistema& error) {
        // EL ERROR ES EL DEL SERVICIO
        //
        // La pantalla no reimplementa ninguna regla: muestra EXACTAMENTE el
        // mensaje de la excepcion. Y vuelve al formulario, que es donde se
        // corrige lo que esta mal.
        avisar(error.what());
        irA(Vista::Formulario);
    }
}

void PantallaTurnos::agendarTurno(const DatosFormulario& datos) {
    // EL ID LO PROPONE EL REPOSITORIO
    //
    // El turno se construye con el id que dice `proximoId()`: es la misma
    // regla que usa el resto del sistema (decision R4) y la que garantiza
    // que dos turnos no tengan el mismo id.
    const Turno turno(_turnos.proximoId(),
                      std::stoi(datos.idMedico),
                      std::stoi(datos.idPaciente),
                      especialidadDesdeCodigo(datos.especialidad),
                      fechaCruda(datos.fecha),
                      horaCruda(datos.hora),
                      std::stoi(datos.duracion),
                      datos.consultorio);

    // TODAS LAS REGLAS SON DEL SERVICIO
    //
    // Si el paciente esta dado de baja, si el medico no atiende esa
    // especialidad, si la fecha ya paso o si el horario esta ocupado, el
    // servicio lo dice con su excepcion y la pantalla muestra ese mensaje.
    _servicioTurnos.agendar(turno);

    avisarExito("Turno #" + std::to_string(turno.getId()) + " agendado.");
    irA(Vista::Menu);
}

void PantallaTurnos::reprogramarTurno(const DatosFormulario& datos) {
    // LA REPROGRAMACION CREA UN TURNO NUEVO
    //
    // El id del turno nuevo lo devuelve el servicio (que es el que pide el
    // id al repositorio): sin el, el mensaje "Nuevo turno #32." no se
    // puede armar.
    const int idNuevo = _servicioTurnos.reprogramar(_idTurno, fechaCruda(datos.fecha),
                                                     horaCruda(datos.hora));

    avisarExito("Turno #" + std::to_string(_idTurno) + " reprogramado. Nuevo turno #"
                + std::to_string(idNuevo) + ".");
    irA(Vista::Menu);
}

void PantallaTurnos::modificarTurno(const DatosFormulario& datos) {
    // SOLO CAMBIAN EL CONSULTORIO, LA DURACION Y LA ESPECIALIDAD
    //
    // La fecha y la hora NO se tocan: moverlas es reprogramar, y
    // reprogramar crea un turno nuevo con su propio historial.
    _servicioTurnos.modificarTurno(_idTurno, datos.consultorio, std::stoi(datos.duracion),
                                   especialidadDesdeCodigo(datos.especialidad));

    avisarExito("Turno #" + std::to_string(_idTurno) + " modificado.");
    irA(Vista::Menu);
}

void PantallaTurnos::ejecutarCancelacion() {
    try {
        _servicioTurnos.cancelar(_idTurno);
        avisarExito("Turno #" + std::to_string(_idTurno) + " cancelado.");
        irA(Vista::Menu);
    } catch (const ExcepcionSistema& error) {
        // EL ERROR ES EL DEL SERVICIO
        //
        // Un turno que no esta activo no se puede cancelar: el mensaje es
        // el del servicio y la pantalla vuelve a la vista del id, que es
        // donde se elige otro turno.
        avisar(error.what());
        irA(Vista::Id);
    }
}

// ---------------------------------------------------------------------------
// La busqueda
//
// LA BUSQUEDA ES DE DOS PASOS
//
// Primero se elige el CRITERIO (el menu) y despues se escribe el VALOR (el
// campo). Es la decision 5 del proyecto: un buscador multi-campo obliga a
// decidir que campo se busca y es mas dificil de usar.
//
// LAS REGLAS SON LAS DEL DOMINIO
//
// La especialidad se busca por CODIGO ("CARD") y el estado por su texto
// ("Activo"): los dos los valida el dominio (`especialidadDesdeCodigo` y
// `estadoDesdeTexto`), asi que su error se muestra como el de cualquier
// otro dato.
//
// El paciente y el medico se buscan por NOMBRE: la coincidencia es
// PARCIAL e insensible a mayusculas, y es la del repositorio
// (`buscarPorNombre`), no una regla nueva de la pantalla.
// ---------------------------------------------------------------------------

void PantallaTurnos::abrirCriterio() {
    irA(Vista::Criterio);
}

std::string PantallaTurnos::etiquetaDeCriterio(Criterio criterio) const {
    switch (criterio) {
        case Criterio::Id:
            return "id";
        case Criterio::Paciente:
            return "paciente";
        case Criterio::Medico:
            return "medico";
        case Criterio::Especialidad:
            return "especialidad";
        case Criterio::Consultorio:
            return "consultorio";
        case Criterio::Estado:
            return "estado";
    }
    return "";
}

Campo::Validador PantallaTurnos::validadorDeCriterio(Criterio criterio) const {
    switch (criterio) {
        case Criterio::Id:
            return validadorDeId();

        case Criterio::Especialidad:
            // LA ESPECIALIDAD SE BUSCA POR CODIGO
            //
            // El codigo es el dato que viaja en el archivo, asi que es el
            // que se escribe. El dominio dice si existe.
            return [](const std::string& texto) -> std::string {
                try {
                    (void)especialidadDesdeCodigo(texto);
                } catch (const DatoInvalidoException& error) {
                    return error.motivo();
                }
                return "";
            };

        case Criterio::Estado:
            // EL ESTADO SE BUSCA POR SU TEXTO
            //
            // El texto es el que ve el usuario ("Activo", "Cancelado",
            // "Reprogramado"). El dominio dice si existe.
            return [](const std::string& texto) -> std::string {
                try {
                    (void)estadoDesdeTexto(texto);
                } catch (const DatoInvalidoException& error) {
                    return error.motivo();
                }
                return "";
            };

        case Criterio::Paciente:
        case Criterio::Medico:
        case Criterio::Consultorio:
        default:
            // Un texto libre: lo que importa es que no este vacio.
            return [](const std::string& texto) -> std::string {
                if (!Utils::esNoVacio(texto)) {
                    return "el valor a buscar no puede estar vacio";
                }
                return "";
            };
    }
}

void PantallaTurnos::iniciarBusqueda(Criterio criterio) {
    _criterioElegido = criterio;
    _campoValor = std::make_unique<Campo>("Valor a buscar", "", validadorDeCriterio(criterio));
    irA(Vista::Valor);
}

void PantallaTurnos::confirmarValor() {
    const std::string valor = _campoValor->valor();
    try {
        _filasResultado = buscarPorCriterio(_criterioElegido, valor);
    } catch (const ExcepcionSistema& error) {
        // EL VALOR NO ES VALIDO PARA EL CRITERIO
        //
        // El campo ya lo habia avisado (su validador pregunta al
        // dominio), asi que esto es la red de contencion: el mensaje se
        // muestra y el usuario vuelve a escribir el valor.
        avisar(error.what());
        irA(Vista::Valor);
        return;
    }
    irA(Vista::Resultados);
}

std::vector<std::vector<std::string>> PantallaTurnos::buscarPorCriterio(Criterio criterio,
                                                                         const std::string& valor) const {
    std::vector<std::vector<std::string>> filas;

    switch (criterio) {
        case Criterio::Id: {
            const int id = idDesdeTexto(valor);
            if (id < 0) {
                return filas;
            }
            Turno turno;
            if (_turnos.buscarPorId(id, turno)) {
                filas.push_back(filaDe(turno, _pacientes, _medicos));
            }
            return filas;
        }

        case Criterio::Paciente: {
            // POR NOMBRE, CON LA REGLA DEL REPOSITORIO
            //
            // La coincidencia es parcial e insensible a mayusculas: la
            // pregunta el repositorio, que es quien tiene el conjunto.
            for (const Paciente& paciente : _pacientes.buscarPorNombre(valor)) {
                for (const Turno& turno : _servicioTurnos.turnosDePaciente(paciente.getId())) {
                    filas.push_back(filaDe(turno, _pacientes, _medicos));
                }
            }
            return filas;
        }

        case Criterio::Medico: {
            for (const Medico& medico : _medicos.buscarPorNombre(valor)) {
                for (const Turno& turno : _servicioTurnos.turnosDeMedico(medico.getId())) {
                    filas.push_back(filaDe(turno, _pacientes, _medicos));
                }
            }
            return filas;
        }

        case Criterio::Especialidad: {
            const Especialidad especialidad = especialidadDesdeCodigo(valor);
            for (const Turno& turno : _servicioTurnos.turnosDeEspecialidad(especialidad)) {
                filas.push_back(filaDe(turno, _pacientes, _medicos));
            }
            return filas;
        }

        case Criterio::Consultorio: {
            // EL CONSULTORIO SE COMPARA EXACTO, IGNORANDO MAYUSCULAS
            //
            // El consultorio es un valor de campo ("A-1"), no un nombre: se
            // busca el que es, no los que lo contienen.
            const std::string buscado = Utils::aMinusculas(Utils::trim(valor));
            for (const Turno& turno : _turnos.listar()) {
                if (Utils::aMinusculas(turno.getConsultorio()) == buscado) {
                    filas.push_back(filaDe(turno, _pacientes, _medicos));
                }
            }
            return filas;
        }

        case Criterio::Estado: {
            const EstadoTurno estado = estadoDesdeTexto(valor);
            for (const Turno& turno : _turnos.listar()) {
                if (turno.getEstado() == estado) {
                    filas.push_back(filaDe(turno, _pacientes, _medicos));
                }
            }
            return filas;
        }
    }
    return filas;
}

// ---------------------------------------------------------------------------
// Los mensajes
// ---------------------------------------------------------------------------

void PantallaTurnos::avisar(const std::string& texto) {
    // El mensaje vive en el CONTEXTO: es lo que la pantalla anterior va a
    // mostrar cuando el usuario vuelva.
    _aplicacion.contexto().mostrarMensaje(Mensaje::error(texto));
}

void PantallaTurnos::avisarExito(const std::string& texto) {
    _aplicacion.contexto().mostrarMensaje(Mensaje::exito(texto));
}

const Mensaje& PantallaTurnos::mensajeActual() const {
    return _aplicacion.contexto().mensaje();
}

// ---------------------------------------------------------------------------
// El dibujo
// ---------------------------------------------------------------------------

ftxui::Element PantallaTurnos::encabezado() const {
    ftxui::Element elemento = ftxui::text(_aplicacion.titulo() + " - " + _titulo);
    if (const auto color = _tema.colorDe(Rol::Titulo)) {
        elemento = elemento | ftxui::color(aFtxui(*color)) | ftxui::bold;
    }
    return elemento;
}

ftxui::Element PantallaTurnos::lineaOpcion(const Menu& menu, int indice) const {
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

ftxui::Element PantallaTurnos::lineaOpcionSelector(int indice) const {
    ftxui::Element elemento = ftxui::text(_selector->lineaDe(indice));
    if (indice == _selector->seleccion()) {
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

ftxui::Element PantallaTurnos::lineaNumeroSelector() const {
    // EL NUMERO QUE SE ESTA ESCRIBIENDO
    //
    // Va a la vista porque es lo que el usuario esta TECLEANDO: si no se
    // mostrara, escribir "12" seria indistinguible de no escribir nada.
    return ftxui::text("  Numero: " + _selector->numeroDigitado() + "|");
}

ftxui::Element PantallaTurnos::lineaCampo(const Campo& campo, bool enfocado) const {
    ftxui::Element elemento =
        ftxui::text("  " + campo.etiqueta() + ": " + campo.textoEnPantalla());
    if (enfocado) {
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

ftxui::Element PantallaTurnos::lineaError(const std::string& texto) const {
    ftxui::Element elemento = ftxui::text("      " + texto);
    if (const auto color = _tema.colorDe(Rol::Error)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

ftxui::Element PantallaTurnos::lineaMensaje() const {
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

ftxui::Element PantallaTurnos::lineaFormulario() const {
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

ftxui::Element PantallaTurnos::lineaConfirmacion() const {
    ftxui::Elements lineas;
    if (_confirmacion) {
        lineas.push_back(ftxui::text("  " + _confirmacion->pregunta()));
        // LAS DOS OPCIONES EN DOS FILAS
        //
        // Una por fila, como en todos los otros menus: asi la pantalla sabe
        // en que fila esta cada una y puede traducir el clic del raton al
        // atajo que corresponde.
        lineas.push_back(ftxui::text("  1. Si"));
        lineas.push_back(ftxui::text("  2. No"));
    }
    return ftxui::vbox(std::move(lineas));
}

ftxui::Element PantallaTurnos::tablaDe(const std::vector<std::vector<std::string>>& filas,
                                       const std::vector<std::string>& cabeceras) const {
    Tabla tabla(cabeceras);
    for (const std::vector<std::string>& fila : filas) {
        tabla.agregarFila(fila);
    }
    return tabla.renderizar();
}

ftxui::Element PantallaTurnos::lineaListado() const {
    std::vector<std::vector<std::string>> filas;
    for (const Turno& turno : _turnos.listar()) {
        filas.push_back(filaDe(turno, _pacientes, _medicos));
    }
    if (filas.empty()) {
        return ftxui::text("  " + vacio("turnos"));
    }
    return tablaDe(filas, cabecerasDeTurno());
}

ftxui::Element PantallaTurnos::lineaResultados() const {
    if (_filasResultado.empty()) {
        return ftxui::text("  No hay coincidencias. 0: volver al menu.");
    }
    return tablaDe(_filasResultado, cabecerasDeTurno());
}

ftxui::Element PantallaTurnos::lineaDetalle() const {
    ftxui::Elements lineas;

    lineas.push_back(ftxui::text("  Ficha de turno #" + std::to_string(_turno.getId())));

    // LA FILA DEL TURNO
    //
    // Todos los campos del turno, en las diez columnas de la
    // especificacion.
    std::vector<std::vector<std::string>> filas;
    filas.push_back(filaCompletaDe(_turno, _pacientes, _medicos));
    lineas.push_back(tablaDe(filas, cabecerasCompletasDeTurno()));

    // EL TURNO REEMPLAZANTE
    //
    // Un turno reprogramado apunta al turno que lo reemplazo: ese es el
    // `idTurnoReemplazante`.
    if (_turno.getIdTurnoReemplazante() != -1) {
        lineas.push_back(
            ftxui::text("  idTurnoReemplazante: "
                        + std::to_string(_turno.getIdTurnoReemplazante())));
        Turno reemplazo;
        if (_turnos.buscarPorId(_turno.getIdTurnoReemplazante(), reemplazo)) {
            lineas.push_back(ftxui::text("  Reemplazante: " + resumenDe(reemplazo)));
        }
    }

    // EL TURNO QUE ESTE TURNO REEMPLAZO
    //
    // El turno NUEVO no guarda a quien reemplazo: el VIEJO es el que
    // apunta. Por eso se busca al turno que apunta a este.
    for (const Turno& otro : _turnos.listar()) {
        if (otro.getIdTurnoReemplazante() == _turno.getId()) {
            lineas.push_back(ftxui::text("  Reemplazado: " + resumenDe(otro)));
        }
    }

    return ftxui::vbox(std::move(lineas));
}

std::string PantallaTurnos::vacio(const std::string& queFalta) const {
    // EL ESTADO VACIO TIENE SALIDA
    //
    // Un mensaje que dice "no hay nada" y no dice como volver deja al
    // usuario atrapado: el mensaje dice las dos cosas.
    return "No hay " + queFalta + " para mostrar. 0: volver al menu.";
}

ftxui::Element PantallaTurnos::renderizar() {
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
        case Vista::Id:
            lineas.push_back(lineaCampo(*_campoId, true));
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
            for (int i = 0; i < _selector->cantidad(); ++i) {
                lineas.push_back(lineaOpcionSelector(i));
            }
            if (!_selector->error().empty()) {
                lineas.push_back(lineaError(_selector->error()));
            }
            break;
        }
        case Vista::Confirmar:
            lineas.push_back(lineaConfirmacion());
            break;
        case Vista::Listado:
            lineas.push_back(lineaListado());
            break;
        case Vista::Criterio:
            // La opcion `i` del menu de criterios queda en la fila
            // `filaInicial + i`: el hit-testing del raton lo necesita.
            _menuCriterios.definirFilaInicial(static_cast<int>(lineas.size()));
            for (int i = 0; i < _menuCriterios.cantidad(); ++i) {
                lineas.push_back(lineaOpcion(_menuCriterios, i));
            }
            lineas.push_back(ftxui::separator());
            break;
        case Vista::Valor:
            lineas.push_back(lineaCampo(*_campoValor, true));
            break;
        case Vista::Resultados:
            lineas.push_back(lineaResultados());
            break;
        case Vista::Detalle:
            lineas.push_back(lineaDetalle());
            break;
    }

    if (!mensajeActual().esVacio()) {
        lineas.push_back(lineaMensaje());
    }
    lineas.push_back(ftxui::text(ayuda()));

    return ftxui::vbox(std::move(lineas));
}

std::string PantallaTurnos::ayuda() const {
    // LA AYUDA DICE COMO VOLVER
    //
    // Un mensaje que no dice como se vuelve deja al usuario atrapado. En las
    // vistas con un campo de texto el '0' solo vuelve si el campo esta
    // vacio, asi que la ayuda lo dice: el '0' puede ser un dato.
    switch (_vista) {
        case Vista::Menu:
            return "Flechas: mover   Enter: elegir   0: volver   Esc: volver";
        case Vista::Criterio:
            return "Flechas: mover   Enter: elegir   0: volver   Esc: volver";
        case Vista::Formulario:
            return "Tab: siguiente campo   Enter: confirmar   0: volver (campo vacio)   "
                   "Esc: volver";
        case Vista::Selector:
            return "Flechas: mover   Numero: elegir   Enter: confirmar   "
                   "Esc: volver al formulario";
        case Vista::Id:
        case Vista::Valor:
            return "Enter: confirmar   0: volver (campo vacio)   Esc: volver";
        case Vista::Confirmar:
            return "1: Si   2: No   0: volver   Esc: volver";
        case Vista::Listado:
        case Vista::Resultados:
        case Vista::Detalle:
            return "0: volver   Esc: volver";
    }
    return "";
}

// ---------------------------------------------------------------------------
// El texto plano (lo que se ve sin terminal)
// ---------------------------------------------------------------------------

const std::string& PantallaTurnos::nombreDeVista(Vista vista) {
    // Los nombres son ESTATICOS: son los mismos para todas las
    // pantallas y la referencia que se devuelve tiene que vivir mas que la
    // llamada.
    static const std::string menu = "Menu";
    static const std::string id = "Id";
    static const std::string formulario = "Formulario";
    static const std::string selector = "Selector";
    static const std::string confirmar = "Confirmar";
    static const std::string listado = "Listado";
    static const std::string criterio = "Criterio";
    static const std::string valor = "Valor";
    static const std::string resultados = "Resultados";
    static const std::string detalle = "Detalle";

    switch (vista) {
        case Vista::Menu:
            return menu;
        case Vista::Id:
            return id;
        case Vista::Formulario:
            return formulario;
        case Vista::Selector:
            return selector;
        case Vista::Confirmar:
            return confirmar;
        case Vista::Listado:
            return listado;
        case Vista::Criterio:
            return criterio;
        case Vista::Valor:
            return valor;
        case Vista::Resultados:
            return resultados;
        case Vista::Detalle:
            return detalle;
    }
    return menu;
}

const std::string& PantallaTurnos::vista() const {
    return nombreDeVista(_vista);
}

std::string PantallaTurnos::textoDeLaVista() const {
    std::string salida;

    switch (_vista) {
        case Vista::Menu: {
            for (int i = 0; i < _menu.cantidad(); ++i) {
                salida += "  " + std::string(1, _menu.atajoDe(i)) + ". "
                    + _menu.etiquetas()[static_cast<std::size_t>(i)] + "\n";
            }
            return salida;
        }
        case Vista::Criterio: {
            for (int i = 0; i < _menuCriterios.cantidad(); ++i) {
                salida += "  " + std::string(1, _menuCriterios.atajoDe(i)) + ". "
                    + _menuCriterios.etiquetas()[static_cast<std::size_t>(i)] + "\n";
            }
            return salida;
        }
        case Vista::Id: {
            if (!_campoId) {
                return salida;
            }
            salida += "  " + _campoId->etiqueta() + ": " + _campoId->textoEnPantalla() + "\n";
            if (!_campoId->error().empty()) {
                salida += "      " + _campoId->error() + "\n";
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
        case Vista::Confirmar: {
            if (_confirmacion) {
                salida += "  " + _confirmacion->pregunta() + "\n";
                salida += "  1. Si\n";
                salida += "  2. No\n";
            }
            return salida;
        }
        case Vista::Valor: {
            if (!_campoValor) {
                return salida;
            }
            salida += "  " + _campoValor->etiqueta() + ": " + _campoValor->textoEnPantalla() + "\n";
            if (!_campoValor->error().empty()) {
                salida += "      " + _campoValor->error() + "\n";
            }
            return salida;
        }
        case Vista::Listado: {
            const std::vector<Turno> turnos = _turnos.listar();
            for (const Turno& turno : turnos) {
                const std::vector<std::string> fila = filaDe(turno, _pacientes, _medicos);
                for (std::size_t i = 0; i < fila.size(); ++i) {
                    if (i > 0) {
                        salida += " | ";
                    }
                    salida += fila[i];
                }
                salida += "\n";
            }
            if (turnos.empty()) {
                salida += "  " + vacio("turnos") + "\n";
            }
            return salida;
        }
        case Vista::Resultados: {
            for (const std::vector<std::string>& fila : _filasResultado) {
                for (std::size_t i = 0; i < fila.size(); ++i) {
                    if (i > 0) {
                        salida += " | ";
                    }
                    salida += fila[i];
                }
                salida += "\n";
            }
            if (_filasResultado.empty()) {
                salida += "  No hay coincidencias. 0: volver al menu.\n";
            }
            return salida;
        }
        case Vista::Detalle: {
            salida += "  Ficha de turno #" + std::to_string(_turno.getId()) + "\n";
            // La ficha muestra el turno ENTERO: la fila completa, con las
            // diez columnas de la especificacion.
            const std::vector<std::string> fila = filaCompletaDe(_turno, _pacientes, _medicos);
            for (std::size_t i = 0; i < fila.size(); ++i) {
                if (i > 0) {
                    salida += " | ";
                }
                salida += fila[i];
            }
            salida += "\n";
            if (_turno.getIdTurnoReemplazante() != -1) {
                salida += "  idTurnoReemplazante: "
                    + std::to_string(_turno.getIdTurnoReemplazante()) + "\n";
                Turno reemplazo;
                if (_turnos.buscarPorId(_turno.getIdTurnoReemplazante(), reemplazo)) {
                    salida += "  Reemplazante: " + resumenDe(reemplazo) + "\n";
                }
            }
            for (const Turno& otro : _turnos.listar()) {
                if (otro.getIdTurnoReemplazante() == _turno.getId()) {
                    salida += "  Reemplazado: " + resumenDe(otro) + "\n";
                }
            }
            return salida;
        }
    }
    return salida;
}

std::string PantallaTurnos::vistaTextoPlano() const {
    // SOLO LA VISTA
    //
    // Es lo que se ve ahora, sin el encabezado ni la ayuda: es lo que
    // necesita el test para comprobar que la vista muestra lo que tiene que
    // mostrar.
    return textoDeLaVista();
}

std::string PantallaTurnos::textoPlano() const {
    std::string salida = _aplicacion.titulo() + " - " + _titulo + "\n";
    salida += textoDeLaVista();
    if (!mensajeActual().esVacio()) {
        salida += mensajeActual().texto() + "\n";
    }
    salida += ayuda();
    return salida;
}

// ---------------------------------------------------------------------------
// La fabrica
// ---------------------------------------------------------------------------

std::unique_ptr<Pantalla> crearPantallaTurnos(Aplicacion& aplicacion,
                                               RepositorioTurnos& turnos,
                                               RepositorioPacientes& pacientes,
                                               RepositorioMedicos& medicos,
                                               ServicioTurnos& servicioTurnos) {
    return std::make_unique<PantallaTurnos>(aplicacion, turnos, pacientes, medicos,
                                             servicioTurnos);
}

} // namespace Pantallas
} // namespace ui
