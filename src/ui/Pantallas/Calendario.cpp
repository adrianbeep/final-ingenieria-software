#include "ui/Pantallas/Calendario.h"

#include "domain/Especialidad.h"
#include "ui/MapaCalor.h"
#include "ui/NombresDePersonas.h"
#include "utils/Utils.h"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// La pantalla de CALENDARIO.
//
// Lo que esta en el header es el POR QUE; lo que esta aca es el COMO.
//
// EL FLUJO DE "VER MES"
//
//   menu -> (anio) -> (mes) -> grilla
//
// El mes se pide en DOS pasos (el anio y el mes) porque son DOS datos: el
// anio solo tiene sentido con un mes, y el mes solo con un anio.
//
// EL FLUJO DEL "DETALLE DEL DIA"
//
//   menu -> (dia) -> tabla
//
// El dia se pide dentro del mes que se esta viendo (el que quedo al
// recorrer la grilla con las flechas).
//
// EL FLUJO DE LA "AGENDA"
//
//   menu -> tabla
//
// La agenda no pregunta nada: arranca en HOY y las flechas cambian de dia.
//
// LA GRILLA SE DIBUJA CON LOS CARACTERES DEL TEMA
//
// Es la misma idea que `ui::Tabla`: la grilla no usa el `border` de FTXUI
// (que trae sus propios caracteres) sino los del `Theme`, asi que las dos
// tablas de la aplicacion se ven igual.
//
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {
namespace {

// ---------------------------------------------------------------------------
// LA FORMA DE LA GRILLA
//
// SON SIETE COLUMNAS (LOS SIETE DIAS DE LA SEMANA)
//
// La columna es el DIA DE LA SEMANA: el lunes es la primera y el domingo la
// ultima. Es la convencion del dominio (`Fecha::diaSemana`: 0 = lunes).
//
// EL ANCHO DE UNA CELDA
//
// Nueve columnas: entran los dos digitos del dia ("31"), el numero de
// turnos entre parentesis ("(12)") y queda aire. Con ese ancho, la grilla
// entera mide 71 columnas: entra en una terminal de 80.
// ---------------------------------------------------------------------------
constexpr int CANTIDAD_DE_COLUMNAS = 7;
constexpr int ANCHO_CELDA = 9;

// LA MUESTRA DE COLOR DE LA LEYENDA
//
// Son los bloques que pintan el color del nivel. Con `NO_COLOR` no se
// dibujan (la leyenda es de texto).
constexpr const char* MUESTRA = "\u2588\u2588\u2588";

// ---------------------------------------------------------------------------
// LOS TEXTO DE LA GRILLA
//
// EL TEXTO CENTRADO EN UN ANCHO
//
// El ancho se mide en COLUMNAS y no en bytes: los nombres de los dias
// tienen tilde, asi que contarlos con `strlen` los daria un numero
// distinto al que ocupan en la pantalla.
// ---------------------------------------------------------------------------
std::string centrado(const std::string& texto, int ancho) {
    const int largo = ftxui::string_width(texto);
    if (largo >= ancho) {
        return texto;
    }
    const int izquierda = (ancho - largo) / 2;
    const int derecha = ancho - largo - izquierda;
    return std::string(static_cast<std::size_t>(izquierda), ' ') + texto
        + std::string(static_cast<std::size_t>(derecha), ' ');
}

// LA CABECERA DE LA GRILLA: LOS SIETE DIAS DE LA SEMANA
//
// El nombre de cada dia lo da el DOMINIO (`Fecha::nombreDiaSemana`), con su
// convencion (0 = lunes). Son DATOS, asi que van con la ortografia correcta
// (con tilde).
//
// La columna de cada nombre tiene que caer ENCIMA de la celda: por eso el
// nombre va centrado en el ancho de la celda, con un espacio de separador
// (que es donde va el separador vertical de la grilla).
std::string cabeceraDeGrilla() {
    std::string texto;
    for (int i = 0; i < CANTIDAD_DE_COLUMNAS; ++i) {
        texto += " " + centrado(Fecha::nombreDiaSemana(i), ANCHO_CELDA);
    }
    return texto;
}

// EL BORDE DE LA GRILLA
//
// El borde horizontal de una fila de la grilla: una esquina, los guiones de
// la primera celda, la esquina que la separa de la siguiente, y asi.
std::string bordeDeGrilla(const Bordes& bordes) {
    std::string texto(1, bordes.esquinaSuperiorIzquierda);
    for (int i = 0; i < CANTIDAD_DE_COLUMNAS; ++i) {
        texto.append(static_cast<std::size_t>(ANCHO_CELDA), bordes.horizontal);
        texto += bordes.esquinaSuperiorIzquierda;
    }
    return texto;
}

// ---------------------------------------------------------------------------
// LA GRILLA DEL MES: SUS SEMANAS
//
// CADA SEMANA ES DOS FILAS
//
// La primera tiene los NUMEROS de los dias y la segunda la CANTIDAD de
// turnos de cada dia. Los huecos (los dias que no son del mes) van vacios.
//
// EL COLOR DE LA CELDA ES EL NIVEL DE CALOR DEL DIA
//
// El nivel sale de la cantidad de turnos (ver `ui/MapaCalor.h`), asi que el
// color de la celda y el numero que tiene al lado siempre dicen lo mismo.
// ---------------------------------------------------------------------------
std::vector<SemanaDeGrilla> semanasDeGrilla(int anio, int mes,
                                            const std::vector<int>& turnosPorDia) {
    std::vector<SemanaDeGrilla> semanas;

    const std::vector<CeldaDelMes> celdas = celdasDelMes(anio, mes);
    for (std::size_t i = 0; i < celdas.size(); i += CANTIDAD_DE_COLUMNAS) {
        SemanaDeGrilla semana;

        for (std::size_t c = 0; c < CANTIDAD_DE_COLUMNAS && i + c < celdas.size(); ++c) {
            const CeldaDelMes& celda = celdas[i + c];
            if (celda.esHueco) {
                // EL HUECO: LA CELDA EXISTE PERO NO DICE NADA
                semana.numeros.celdas.push_back(
                    CeldaDeGrilla{centrado("", ANCHO_CELDA), Rol::TablaCelda});
                semana.contadores.celdas.push_back(
                    CeldaDeGrilla{centrado("", ANCHO_CELDA), Rol::TablaCelda});
                continue;
            }

            int turnos = 0;
            if (celda.dia >= 1 && static_cast<std::size_t>(celda.dia) <= turnosPorDia.size()) {
                turnos = turnosPorDia[static_cast<std::size_t>(celda.dia - 1)];
            }
            const Rol rol = rolDeCalor(nivelDeCalor(turnos));

            semana.numeros.celdas.push_back(
                CeldaDeGrilla{centrado(std::to_string(celda.dia), ANCHO_CELDA), rol});
            semana.contadores.celdas.push_back(
                CeldaDeGrilla{centrado("(" + std::to_string(turnos) + ")", ANCHO_CELDA), rol});
        }

        semanas.push_back(semana);
    }

    return semanas;
}

// ---------------------------------------------------------------------------
// LAS COLUMNAS Y LAS FILAS DE LAS TABLAS
//
// EL DETALLE DEL DIA: LAS SEIS COLUMNAS DE LA LISTA
//
// Son las mismas que la lista de turnos de la pantalla de turnos (la
// decision 31): `id`, `fecha`, `hora`, `paciente`, `medico` y `estado`. Con
// esas seis la tabla entra en una terminal de 80 columnas.
//
// LA AGENDA: LA VISTA CRONOLOGICA DEL DIA
//
// Es la hora de INICIO, la de FIN, la duracion, la especialidad, el
// consultorio y el estado. La hora de fin no la guarda el dominio (la
// guarda la duracion): se deriva.
//
// LA ESPECIALIDAD SE MUESTRA CON SU DESCRIPCION
//
// Es lo que el usuario lee ("Cardiologia"): el CODIGO ("CARD") es el dato
// que viaja en el archivo.
// ---------------------------------------------------------------------------
const std::vector<std::string>& cabecerasDeDetalle() {
    static const std::vector<std::string> cabeceras = {"id", "fecha", "hora", "paciente",
                                                        "medico", "estado"};
    return cabeceras;
}

const std::vector<std::string>& cabecerasDeAgenda() {
    static const std::vector<std::string> cabeceras = {"inicio", "fin", "duracion", "especialidad",
                                                        "consultorio", "estado"};
    return cabeceras;
}

// LA FILA DE UN TURNO EN EL DETALLE DEL DIA
//
// Los nombres del paciente y del medico los resuelve el repositorio (el
// turno guarda solo los ids): es la misma regla que usan las otras
// pantallas (`ui/NombresDePersonas.h`).
std::vector<std::string> filaDeDetalle(const Turno& turno, RepositorioPacientes& pacientes,
                                        RepositorioMedicos& medicos) {
    return {std::to_string(turno.getId()),
            turno.getFecha().aTexto(),
            turno.getHora().aTexto(),
            nombreDelPaciente(turno.getIdPaciente(), pacientes),
            nombreDelMedico(turno.getIdMedico(), medicos),
            estadoATexto(turno.getEstado())};
}

// LA FILA DE UN TURNO EN LA AGENDA
std::vector<std::string> filaDeAgenda(const Turno& turno, const std::string& fin) {
    return {turno.getHora().aTexto(),
            fin,
            std::to_string(turno.getDuracionMin()) + " min",
            descripcionDe(turno.getEspecialidad()),
            turno.getConsultorio(),
            estadoATexto(turno.getEstado())};
}

// ---------------------------------------------------------------------------
// LA HORA DE FIN DE UN TURNO
//
// EL DOMINIO NO LA GUARDA: LA GUARDA LA DURACION
//
// Asi que se deriva: la hora de inicio mas la duracion. La cuenta se hace
// en MINUTOS DESDE LA MEDIANOCHE (que es la unica cuenta que no mezcla
// horas y minutos) y el resultado se vuelve a horas y minutos.
//
// EL SERVICIO GARANTIZA QUE EL TURNO TERMINA DENTRO DEL HORARIO
//
// Asi que la hora de fin siempre cae dentro de la ventana de atencion.
// ---------------------------------------------------------------------------
std::string horaDeFin(const Turno& turno) {
    const int fin = turno.getHora().minutosDesdeMedianoche() + turno.getDuracionMin();
    return Hora(fin / Hora::MINUTOS_POR_HORA, fin % Hora::MINUTOS_POR_HORA).aTexto();
}

// ---------------------------------------------------------------------------
// LOS VALIDADORES DE LOS CAMPOS
//
// EL RANGO LO DECIDE EL DOMINIO
//
// El campo decide la FORMA del dato (que el anio sea un numero) y el RANGO
// que el dominio soporta: los anos del calendario de `Fecha` (2000 a 2100)
// y los doce meses. El "0 turnos = sin color" no es un dato: es la AUSENCIA
// de ocupacion, asi que no tiene nivel de calor.
// ---------------------------------------------------------------------------

// EL NUMERO DE UN TEXTO
//
// Devuelve -1 si el texto no es un numero (o es un numero tan grande que
// no entra en un `int`). El `try/catch` es por el `std::out_of_range` de
// `std::stoll`: un numero enorme no es un dato invalido del usuario, es un
// dato que no entra en un `int`, y no puede dejar tirada una excepcion del
// sistema.
int numeroDeTexto(const std::string& texto) {
    if (!Utils::esEntero(texto)) {
        return -1;
    }
    try {
        const long long valor = std::stoll(texto);
        if (valor < -1000000000LL || valor > 1000000000LL) {
            return -1;
        }
        return static_cast<int>(valor);
    } catch (...) {
        return -1;
    }
}

// EL VALIDADOR DEL ANIO
//
// El rango es el del dominio: un anio fuera de el no tiene un mes que
// mostrar.
Campo::Validador validadorDeAnio() {
    return [](const std::string& texto) -> std::string {
        const int anio = numeroDeTexto(texto);
        if (anio < Fecha::ANIO_MINIMO || anio > Fecha::ANIO_MAXIMO) {
            return "el anio tiene que ser un numero entre " + std::to_string(Fecha::ANIO_MINIMO)
                + " y " + std::to_string(Fecha::ANIO_MAXIMO);
        }
        return "";
    };
}

// EL VALIDADOR DEL MES
Campo::Validador validadorDeMes() {
    return [](const std::string& texto) -> std::string {
        const int mes = numeroDeTexto(texto);
        if (mes < 1 || mes > 12) {
            return "el mes tiene que ser un numero entre 1 y 12";
        }
        return "";
    };
}

// EL VALIDADOR DEL DIA
//
// El rango es el del MES QUE SE ESTA VIENDO: no existe el 31 de febrero, y
// el dia que se escribe tiene que existir en el mes que se va a mostrar.
Campo::Validador validadorDeDia(int anio, int mes) {
    const int ultimo = Fecha::diasDelMes(anio, mes);
    return [ultimo](const std::string& texto) -> std::string {
        const int dia = numeroDeTexto(texto);
        if (dia < 1 || dia > ultimo) {
            return "el dia tiene que ser un numero entre 1 y " + std::to_string(ultimo);
        }
        return "";
    };
}

// ---------------------------------------------------------------------------
// LA LEYENDA DEL MAPA DE CALOR
//
// CON COLORES: UNA MUESTRA POR NIVEL
//
// La muestra es un bloque pintado con el color del nivel, y al lado va el
// rango de turnos que lo forma.
//
// SIN COLORES: TEXTO
//
// Un usuario que no ve colores tiene que poder leer el mapa igual: la
// leyenda dice los rangos Y lo que significan ("baja", "media", "alta",
// "maxima"). Es lo que pide el `NO_COLOR`: la misma informacion, sin
// depender del color.
//
// LAS DOS VERSIONES TIENEN LA MISMA ESTRUCTURA
//
// El caso sin turnos va primero y los cuatro niveles van despues, en las
// dos versiones: asi la leyenda se lee igual con y sin color, y las dos
// caben en una terminal de 80 columnas.
// ---------------------------------------------------------------------------

// UNA PARTE DE LA LEYENDA
//
// O es una MUESTRA (el bloque de color de un nivel) o es TEXTO.
struct ParteLeyenda {
    bool esMuestra = false;
    std::string texto;
    Rol rol = Rol::Subtitulo;
};

// LAS PARTES DE LA LEYENDA
std::vector<ParteLeyenda> leyendaDelMapaCalor(bool conColores) {
    std::vector<ParteLeyenda> partes;

    // EL CASO SIN TURNOS
    //
    // No tiene muestra: no tiene color (un dia sin turnos no es "el nivel
    // mas bajo", es la ausencia de ocupacion).
    partes.push_back(ParteLeyenda{false, "0 = sin turnos  ", Rol::Subtitulo});

    for (const NivelCalor nivel : nivelesConColor()) {
        if (conColores) {
            partes.push_back(ParteLeyenda{true, MUESTRA, rolDeCalor(nivel)});
            partes.push_back(
                ParteLeyenda{false, " " + rangoDeCalor(nivel) + "  ", Rol::Subtitulo});
            continue;
        }
        partes.push_back(ParteLeyenda{false,
                                       rangoDeCalor(nivel) + " = " + nombreDeCalor(nivel) + "  ",
                                       Rol::Subtitulo});
    }

    return partes;
}

// ---------------------------------------------------------------------------
// EL TEXTO DEL MES
//
// Es el "AAAA-MM" del dominio: el mes que se esta viendo.
// ---------------------------------------------------------------------------
std::string textoDelMes(int anio, int mes) {
    return Fecha(anio, mes, 1).aTexto().substr(0, 7);
}

} // namespace

// ---------------------------------------------------------------------------
// Construccion
// ---------------------------------------------------------------------------

PantallaCalendario::PantallaCalendario(Aplicacion& aplicacion,
                                       RepositorioPacientes& pacientes,
                                       RepositorioMedicos& medicos,
                                       ServicioTurnos& servicioTurnos,
                                       const Fecha& hoy)
    : _aplicacion(aplicacion),
      _pacientes(pacientes),
      _medicos(medicos),
      _servicioTurnos(servicioTurnos),
      _titulo("CALENDARIO"),
      _menu("Calendario"),
      _anio(hoy.anio),
      _mes(hoy.mes),
      _mesEnArma(hoy.anio, hoy.mes, 1),
      _hoy(hoy) {
    // EL MENU DEL MODULO
    //
    // El atajo es el numero que se escribe (decision D28) y el '0' es
    // "Volver" en todos los niveles.
    _menu.agregar('1', "Ver mes", [this]() { iniciarVerMes(); });
    _menu.agregar('2', "Ver detalle del dia", [this]() { iniciarDetalleDelDia(); });
    _menu.agregar('3', "Agenda", [this]() { iniciarAgenda(); });
    _menu.agregar('0', "Volver", [this]() { volverAtras(); });

    // El menu se dibuja a partir de la fila FILA_INICIAL: el hit-testing del
    // raton lo necesita.
    _menu.definirFilaInicial(2);
}

std::string PantallaCalendario::titulo() const {
    return _titulo;
}

// ---------------------------------------------------------------------------
// La navegacion
// ---------------------------------------------------------------------------

void PantallaCalendario::irA(Vista vista) {
    _vista = vista;
}

void PantallaCalendario::volverAtras() {
    // EL "VOLVER" SUBE UN NIVEL
    //
    //   - desde el menu de la pantalla, se sale de la pantalla (el menu
    //     principal esta en la pila de la aplicacion);
    //   - desde cualquier otra vista, se vuelve al menu del modulo.
    switch (_vista) {
        case Vista::Menu:
            _aplicacion.volver();
            return;
        default:
            irA(Vista::Menu);
            return;
    }
}

bool PantallaCalendario::esTeclaVolver(const Evento& evento) const {
    return evento.tipo == TipoTecla::Caracter && evento.caracter == "0";
}

bool PantallaCalendario::esTeclaEscape(const Evento& evento) const {
    return evento.tipo == TipoTecla::Escape;
}

// EL '0' ES "VOLVER" SOLO SI EL CAMPO ESTA VACIO
//
// Con algo escrito el '0' es un DATO (un anio puede ser 2020) y se lo queda
// el campo. Es la misma regla que usan el motor CRUD y las pantallas de
// turnos y de reportes.
bool PantallaCalendario::esTeclaVolverConCampoVacio(const Evento& evento,
                                                     const Campo& campo) const {
    return esTeclaVolver(evento) && campo.texto().empty();
}

// ---------------------------------------------------------------------------
// Los manejadores de cada vista
// ---------------------------------------------------------------------------

bool PantallaCalendario::manejar(const Evento& evento) {
    switch (_vista) {
        case Vista::Menu:
            return manejarMenu(evento);
        case Vista::Anio:
            return manejarCampo(evento, _campoAnio, [this]() { confirmarAnio(); });
        case Vista::Mes:
            return manejarCampo(evento, _campoMes, [this]() { confirmarMes(); });
        case Vista::Grilla:
            return manejarGrilla(evento);
        case Vista::Dia:
            return manejarCampo(evento, _campoDia, [this]() { confirmarDia(); });
        case Vista::Detalle:
            return manejarDetalle(evento);
        case Vista::Agenda:
            return manejarAgenda(evento);
    }
    return false;
}

bool PantallaCalendario::manejarMenu(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` NO SE CONSUME EN EL MENU
    //
    // El menu de la pantalla es el NIVEL MAS ALTO de esta pantalla: desde
    // ahi el `Escape` tiene que sacar la pantalla ENTERA de la pila, y eso
    // lo hace la aplicacion.
    return _menu.aplicarEvento(evento);
}

bool PantallaCalendario::manejarCampo(const Evento& evento, const std::unique_ptr<Campo>& campo,
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

// ---------------------------------------------------------------------------
// La grilla del mes
// ---------------------------------------------------------------------------

bool PantallaCalendario::manejarGrilla(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE AL MENU DE LA PANTALLA
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    // LAS FLECHAS CAMBIAN DE MES
    //
    // La grilla se recorre: la izquierda va al mes anterior y la derecha al
    // siguiente. El mes que se esta viendo QUEDA (es el estado de la
    // pantalla).
    if (evento.tipo == TipoTecla::FlechaIzquierda) {
        moverMes(-1);
        return true;
    }
    if (evento.tipo == TipoTecla::FlechaDerecha) {
        moverMes(1);
        return true;
    }
    return false;
}

void PantallaCalendario::iniciarVerMes() {
    // EL ANIO Y EL MES SON DOS PASOS
    //
    // El anio se escribe primero (es el dato mas grande) y el mes despues.
    _campoAnio = std::make_unique<Campo>("Anio", "", validadorDeAnio());
    irA(Vista::Anio);
}

void PantallaCalendario::confirmarAnio() {
    // EL ANIO QUEDA A LA ESPERA DEL MES
    //
    // El mes que se esta viendo NO cambia todavia: cambia cuando el mes
    // tambien esta escrito.
    _mesEnArma = Fecha(numeroDeTexto(_campoAnio->valor()), 1, 1);
    _campoMes = std::make_unique<Campo>("Mes", "", validadorDeMes());
    irA(Vista::Mes);
}

void PantallaCalendario::confirmarMes() {
    const int mes = numeroDeTexto(_campoMes->valor());
    _mesEnArma = Fecha(_mesEnArma.anio, mes, 1);

    // EL MES QUE SE ESTA VIENDO
    _anio = _mesEnArma.anio;
    _mes = _mesEnArma.mes;

    armarGrilla();
    avisarExito(std::to_string(totalDeTurnosDelMes()) + " turno(s) en " + textoDelMes(_anio, _mes)
                + ".");
    irA(Vista::Grilla);
}

void PantallaCalendario::armarGrilla() {
    const int cantidad = Fecha::diasDelMes(_anio, _mes);
    _turnosPorDia.assign(static_cast<std::size_t>(cantidad), 0);

    // LOS TURNOS DEL MES, DE UNA VEZ
    //
    // La consulta es la del RANGO (del primer dia al ultimo del mes): una
    // sola pregunta al servicio en vez de una por dia.
    const std::vector<Turno> turnos = _servicioTurnos.turnosDeRango(Fecha(_anio, _mes, 1),
                                                                    Fecha(_anio, _mes, cantidad));
    for (const Turno& turno : turnos) {
        const int dia = turno.getFecha().dia;
        if (dia >= 1 && dia <= cantidad) {
            ++_turnosPorDia[static_cast<std::size_t>(dia - 1)];
        }
    }
}

void PantallaCalendario::moverMes(int delta) {
    int anio = _anio;
    int mes = _mes + delta;

    // EL ANIO DA LA VUELTA
    if (mes > 12) {
        mes = 1;
        ++anio;
    }
    if (mes < 1) {
        mes = 12;
        --anio;
    }

    // LOS EXTREMOS DEL CALENDARIO DEL DOMINIO
    //
    // El calendario de `Fecha` va del 2000 al 2100: un mes fuera de ese
    // rango no tiene fecha (y `diaSemana()` fallaria), asi que el recorrido
    // se detiene en el borde.
    if (anio > Fecha::ANIO_MAXIMO) {
        anio = Fecha::ANIO_MAXIMO;
        mes = 12;
    }
    if (anio < Fecha::ANIO_MINIMO) {
        anio = Fecha::ANIO_MINIMO;
        mes = 1;
    }

    _anio = anio;
    _mes = mes;
    armarGrilla();
}

int PantallaCalendario::totalDeTurnosDelMes() const {
    int total = 0;
    for (const int turnos : _turnosPorDia) {
        total += turnos;
    }
    return total;
}

std::vector<SemanaDeGrilla> PantallaCalendario::grilla() const {
    return semanasDeGrilla(_anio, _mes, _turnosPorDia);
}

// ---------------------------------------------------------------------------
// El detalle del dia
// ---------------------------------------------------------------------------

bool PantallaCalendario::manejarDetalle(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE AL MENU DE LA PANTALLA
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    // Las vistas de solo lectura no consumen nada mas.
    return false;
}

void PantallaCalendario::iniciarDetalleDelDia() {
    // EL DIA SE PIDE EN EL MES QUE SE ESTA VIENDO
    //
    // El mes que se esta viendo es el estado de la pantalla: si el usuario
    // recorrio el mes con las flechas, el detalle se ve en ese mes. Si no
    // lo recorrio, es el mes de HOY (con el que arranca la pantalla).
    _campoDia = std::make_unique<Campo>("Dia del mes", "", validadorDeDia(_anio, _mes));
    irA(Vista::Dia);
}

void PantallaCalendario::confirmarDia() {
    const int dia = numeroDeTexto(_campoDia->valor());
    _fechaDelDetalle = Fecha(_anio, _mes, dia);

    armarDetalle();
    avisarExito(std::to_string(_filas.size()) + " turno(s) el " + _fechaDelDetalle.aTexto() + ".");
    irA(Vista::Detalle);
}

void PantallaCalendario::armarDetalle() {
    _filas.clear();
    for (const Turno& turno : _servicioTurnos.turnosDeFecha(_fechaDelDetalle)) {
        _filas.push_back(filaDeDetalle(turno, _pacientes, _medicos));
    }
    _cabeceras = cabecerasDeDetalle();
    _mensajeVacio = "No hay turnos ese dia. 0: volver al menu.";
}

// ---------------------------------------------------------------------------
// La agenda
// ---------------------------------------------------------------------------

bool PantallaCalendario::manejarAgenda(const Evento& evento) {
    if (esTeclaVolver(evento)) {
        volverAtras();
        return true;
    }
    // EL `Escape` VUELVE AL MENU DE LA PANTALLA
    if (esTeclaEscape(evento)) {
        volverAtras();
        return true;
    }
    // LAS FLECHAS CAMBIAN DE DIA
    //
    // La agenda es de UN dia: la izquierda va al dia anterior y la derecha
    // al siguiente.
    if (evento.tipo == TipoTecla::FlechaIzquierda) {
        moverDia(-1);
        return true;
    }
    if (evento.tipo == TipoTecla::FlechaDerecha) {
        moverDia(1);
        return true;
    }
    return false;
}

void PantallaCalendario::iniciarAgenda() {
    // LA AGENDA ARRANCA EN HOY
    //
    // El dia que se esta viendo es el que recibio la pantalla: el recorrido
    // con las flechas lo va cambiando.
    _diaAgenda = _hoy;

    armarAgenda();
    avisarExito(std::to_string(_filas.size()) + " turno(s) el " + _diaAgenda.aTexto() + ".");
    irA(Vista::Agenda);
}

void PantallaCalendario::armarAgenda() {
    std::vector<Turno> turnos = _servicioTurnos.turnosDeFecha(_diaAgenda);

    // LA AGENDA ES CRONOLOGICA
    //
    // Los turnos vienen ordenados por id (es como los da el repositorio), y
    // la agenda es por HORA DE INICIO. El `stable_sort` conserva el orden
    // de los turnos que arrancan a la misma hora.
    std::stable_sort(turnos.begin(), turnos.end(), [](const Turno& uno, const Turno& otro) {
        return uno.getHora() < otro.getHora();
    });

    _filas.clear();
    for (const Turno& turno : turnos) {
        _filas.push_back(filaDeAgenda(turno, horaDeFin(turno)));
    }
    _cabeceras = cabecerasDeAgenda();
    _mensajeVacio = "No hay turnos ese dia. 0: volver al menu.";
}

void PantallaCalendario::moverDia(int delta) {
    const int numero = _diaAgenda.numeroDeDia() + delta;

    // LOS EXTREMOS DEL CALENDARIO DEL DOMINIO
    //
    // El dia no da la vuelta: en el borde del calendario el recorrido se
    // detiene (es la misma regla que el menu, que no da la vuelta al llegar
    // al extremo).
    if (numero < 0 || numero > ultimoNumeroDeDia()) {
        return;
    }

    _diaAgenda = fechaDesdeNumeroDeDia(numero);
    armarAgenda();
    avisarExito(std::to_string(_filas.size()) + " turno(s) el " + _diaAgenda.aTexto() + ".");
}

// ---------------------------------------------------------------------------
// Los mensajes
// ---------------------------------------------------------------------------

void PantallaCalendario::avisar(const std::string& texto) {
    // El mensaje vive en el CONTEXTO: es lo que la pantalla anterior va a
    // mostrar cuando el usuario vuelva.
    _aplicacion.contexto().mostrarMensaje(Mensaje::error(texto));
}

void PantallaCalendario::avisarExito(const std::string& texto) {
    _aplicacion.contexto().mostrarMensaje(Mensaje::exito(texto));
}

const Mensaje& PantallaCalendario::mensajeActual() const {
    return _aplicacion.contexto().mensaje();
}

// ---------------------------------------------------------------------------
// El dibujo
// ---------------------------------------------------------------------------

ftxui::Element PantallaCalendario::encabezado() const {
    ftxui::Element elemento = ftxui::text(_aplicacion.titulo() + " - " + _titulo);
    if (const auto color = _tema.colorDe(Rol::Titulo)) {
        elemento = elemento | ftxui::color(aFtxui(*color)) | ftxui::bold;
    }
    return elemento;
}

ftxui::Element PantallaCalendario::lineaOpcion(int indice) const {
    const std::string atajo(1, _menu.atajoDe(indice));
    ftxui::Element elemento =
        ftxui::text("  " + atajo + ". " + _menu.etiquetas()[static_cast<std::size_t>(indice)]);
    if (indice == _menu.seleccion()) {
        // El resaltado de la opcion seleccionada. `inverted` funciona SIN
        // color, asi que el menu se ve igual con `NO_COLOR` definido.
        elemento = elemento | ftxui::inverted;
    }
    return elemento;
}

ftxui::Element PantallaCalendario::lineaCampo(const Campo& campo) const {
    ftxui::Element elemento =
        ftxui::text("  " + campo.etiqueta() + ": " + campo.textoEnPantalla());
    elemento = elemento | ftxui::inverted;
    return elemento;
}

ftxui::Element PantallaCalendario::lineaError(const std::string& texto) const {
    ftxui::Element elemento = ftxui::text("      " + texto);
    if (const auto color = _tema.colorDe(Rol::Error)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

ftxui::Element PantallaCalendario::lineaMensaje() const {
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

// LA CABECERA DE LA GRILLA
//
// Los siete dias de la semana, con el color de los rotulos de tabla.
ftxui::Element PantallaCalendario::lineaCabeceraGrilla() const {
    ftxui::Element elemento = ftxui::text("  " + cabeceraDeGrilla());
    if (const auto color = _tema.colorDe(Rol::TablaHeader)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

// EL BORDE DE LA GRILLA
ftxui::Element PantallaCalendario::lineaBordeGrilla() const {
    ftxui::Element elemento = ftxui::text("  " + bordeDeGrilla(_tema.bordes()));
    if (const auto color = _tema.colorDe(Rol::TablaBorde)) {
        elemento = elemento | ftxui::color(aFtxui(*color));
    }
    return elemento;
}

// UNA FILA DE LA GRILLA
//
// Cada celda se pinta con su nivel de calor, y los separadores verticales
// con el color del borde (como los de las tablas).
ftxui::Element PantallaCalendario::filaDeGrilla(const FilaDeGrilla& fila) const {
    const Bordes& bordes = _tema.bordes();
    ftxui::Elements partes;

    // La sangria de la grilla (la misma que la cabecera y los bordes).
    partes.push_back(ftxui::text("  "));

    for (const CeldaDeGrilla& celda : fila.celdas) {
        ftxui::Element separador = ftxui::text(std::string(1, bordes.vertical));
        if (const auto color = _tema.colorDe(Rol::TablaBorde)) {
            separador = separador | ftxui::color(aFtxui(*color));
        }
        partes.push_back(separador);

        ftxui::Element contenido = ftxui::text(celda.texto);
        if (const auto color = _tema.colorDe(celda.rol)) {
            contenido = contenido | ftxui::color(aFtxui(*color));
        }
        partes.push_back(contenido);
    }

    // El separador vertical del final de la fila.
    ftxui::Element cierre = ftxui::text(std::string(1, bordes.vertical));
    if (const auto color = _tema.colorDe(Rol::TablaBorde)) {
        cierre = cierre | ftxui::color(aFtxui(*color));
    }
    partes.push_back(cierre);

    return ftxui::hbox(std::move(partes));
}

ftxui::Element PantallaCalendario::lineaGrilla() const {
    ftxui::Elements lineas;

    lineas.push_back(lineaCabeceraGrilla());
    lineas.push_back(lineaBordeGrilla());

    // CADA SEMANA: LOS NUMEROS, LOS CONTADORES Y EL BORDE DE ABAJO
    //
    // El borde va DESPUES de las dos filas de la semana: asi el numero del
    // dia y su cantidad de turnos quedan juntos, y las semanas se separan
    // entre si.
    for (const SemanaDeGrilla& semana : grilla()) {
        lineas.push_back(filaDeGrilla(semana.numeros));
        lineas.push_back(filaDeGrilla(semana.contadores));
        lineas.push_back(lineaBordeGrilla());
    }

    return ftxui::vbox(std::move(lineas));
}

// LA LEYENDA DEL MAPA DE CALOR
//
// CON COLORES: UNA MUESTRA POR NIVEL
//
// La muestra es un bloque pintado con el color del nivel, y al lado va el
// rango de turnos que lo forma.
//
// SIN COLORES: TEXTO
//
// Un usuario que no ve colores tiene que poder leer el mapa igual: la
// leyenda dice los rangos Y lo que significan ("baja", "media", "alta",
// "maxima"). Es lo que pide el `NO_COLOR`: la misma informacion, sin
// depender del color.
ftxui::Element PantallaCalendario::lineaLeyenda() const {
    ftxui::Elements partes;
    partes.push_back(ftxui::text("  Leyenda:  "));

    for (const ParteLeyenda& parte : leyendaDelMapaCalor(_tema.coloresActivos())) {
        if (!parte.esMuestra) {
            partes.push_back(ftxui::text(parte.texto));
            continue;
        }
        ftxui::Element muestra = ftxui::text(parte.texto);
        if (const auto color = _tema.colorDe(parte.rol)) {
            muestra = muestra | ftxui::color(aFtxui(*color));
        }
        partes.push_back(muestra);
    }

    return ftxui::hbox(std::move(partes));
}

// LA TABLA DEL DETALLE O DE LA AGENDA
//
// Es la MISMA tabla de la aplicacion (`ui::Tabla`): las columnas se miden
// por su contenido, asi que la tabla se ajusta al dato.
ftxui::Element PantallaCalendario::tablaDe(const std::vector<std::vector<std::string>>& filas,
                                           const std::vector<std::string>& cabeceras) const {
    Tabla tabla(cabeceras);
    for (const std::vector<std::string>& fila : filas) {
        tabla.agregarFila(fila);
    }
    return tabla.renderizar();
}

// EL DETALLE DEL DIA
//
// EL ESTADO VACIO TIENE SALIDA
//
// Un mensaje que dice "no hay nada" y no dice como volver deja al usuario
// atrapado: el mensaje dice las dos cosas.
ftxui::Element PantallaCalendario::lineaDetalle() const {
    if (_filas.empty()) {
        return ftxui::text("  " + _mensajeVacio);
    }
    return tablaDe(_filas, _cabeceras);
}

// LA AGENDA DEL DIA
ftxui::Element PantallaCalendario::lineaAgenda() const {
    if (_filas.empty()) {
        return ftxui::text("  " + _mensajeVacio);
    }
    return tablaDe(_filas, _cabeceras);
}

std::string PantallaCalendario::ayuda() const {
    // LA AYUDA DICE COMO VOLVER
    //
    // Un mensaje que no dice como se vuelve deja al usuario atrapado. En las
    // vistas con un campo de texto el '0' solo vuelve si el campo esta
    // vacio, asi que la ayuda lo dice: el '0' puede ser un dato.
    switch (_vista) {
        case Vista::Menu:
            return "Flechas: mover   Enter: elegir   0: volver   Esc: volver";
        case Vista::Anio:
        case Vista::Mes:
        case Vista::Dia:
            return "Enter: confirmar   0: volver (campo vacio)   Esc: volver";
        case Vista::Grilla:
            return "Flechas: mes anterior / mes siguiente   0: volver   Esc: volver";
        case Vista::Detalle:
            return "0: volver   Esc: volver";
        case Vista::Agenda:
            return "Flechas: dia anterior / dia siguiente   0: volver   Esc: volver";
    }
    return "";
}

ftxui::Element PantallaCalendario::renderizar() {
    ftxui::Elements lineas;

    lineas.push_back(encabezado());
    lineas.push_back(ftxui::separator());

    switch (_vista) {
        case Vista::Menu:
            // La opcion `i` del menu queda en la fila `filaInicial + i`: el
            // hit-testing del raton lo necesita.
            _menu.definirFilaInicial(static_cast<int>(lineas.size()));
            for (int i = 0; i < _menu.cantidad(); ++i) {
                lineas.push_back(lineaOpcion(i));
            }
            lineas.push_back(ftxui::separator());
            break;
        case Vista::Anio:
            lineas.push_back(lineaCampo(*_campoAnio));
            // EL ERROR VA DEBAJO DEL CAMPO
            //
            // El error describe el ultimo intento y el texto NO se borra: el
            // usuario tiene que corregir lo que escribio, no volver a
            // escribirlo.
            if (!_campoAnio->error().empty()) {
                lineas.push_back(lineaError(_campoAnio->error()));
            }
            break;
        case Vista::Mes:
            lineas.push_back(lineaCampo(*_campoMes));
            if (!_campoMes->error().empty()) {
                lineas.push_back(lineaError(_campoMes->error()));
            }
            break;
        case Vista::Dia:
            lineas.push_back(lineaCampo(*_campoDia));
            if (!_campoDia->error().empty()) {
                lineas.push_back(lineaError(_campoDia->error()));
            }
            break;
        case Vista::Grilla:
            lineas.push_back(ftxui::text("  Mes " + textoDelMes(_anio, _mes)));
            lineas.push_back(lineaGrilla());
            lineas.push_back(ftxui::separator());
            lineas.push_back(lineaLeyenda());
            break;
        case Vista::Detalle:
            lineas.push_back(ftxui::text("  Turnos del " + _fechaDelDetalle.aTexto()));
            lineas.push_back(lineaDetalle());
            break;
        case Vista::Agenda:
            // EL DIA DE LA SEMANA LO DICE EL DOMINIO
            //
            // `Fecha::diaSemana()` es aritmetica sobre el calendario: es
            // determinista (no sale del reloj del sistema).
            lineas.push_back(ftxui::text("  Agenda del " + _diaAgenda.aTexto() + " ("
                                         + Fecha::nombreDiaSemana(_diaAgenda.diaSemana()) + ")"));
            lineas.push_back(lineaAgenda());
            break;
    }

    if (!mensajeActual().esVacio()) {
        lineas.push_back(lineaMensaje());
    }
    lineas.push_back(ftxui::text(ayuda()));

    return ftxui::vbox(std::move(lineas));
}

// ---------------------------------------------------------------------------
// El texto plano (lo que se ve sin terminal)
// ---------------------------------------------------------------------------

const std::string& PantallaCalendario::nombreDeVista(Vista vista) {
    // Los nombres son ESTATICOS: son los mismos para todas las pantallas y
    // la referencia que se devuelve tiene que vivir mas que la llamada.
    static const std::string menu = "Menu";
    static const std::string anio = "Anio";
    static const std::string mes = "Mes";
    static const std::string grilla = "Grilla";
    static const std::string dia = "Dia";
    static const std::string detalle = "Detalle";
    static const std::string agenda = "Agenda";

    switch (vista) {
        case Vista::Menu:
            return menu;
        case Vista::Anio:
            return anio;
        case Vista::Mes:
            return mes;
        case Vista::Grilla:
            return grilla;
        case Vista::Dia:
            return dia;
        case Vista::Detalle:
            return detalle;
        case Vista::Agenda:
            return agenda;
    }
    return menu;
}

const std::string& PantallaCalendario::vista() const {
    return nombreDeVista(_vista);
}

int PantallaCalendario::anio() const {
    return _anio;
}

int PantallaCalendario::mes() const {
    return _mes;
}

int PantallaCalendario::diaDeLaAgenda() const {
    return _diaAgenda.dia;
}

// LA GRILLA EN TEXTO PLANO
//
// Es la misma grilla que se dibuja en la terminal, sin los colores: los
// bordes, los numeros de los dias y la cantidad de turnos de cada uno.
std::string PantallaCalendario::textoDeLaGrilla() const {
    std::string salida;

    salida += "  " + cabeceraDeGrilla() + "\n";
    salida += "  " + bordeDeGrilla(_tema.bordes()) + "\n";

    for (const SemanaDeGrilla& semana : grilla()) {
        salida += "  " + filaDeGrillaEnTexto(semana.numeros) + "\n";
        salida += "  " + filaDeGrillaEnTexto(semana.contadores) + "\n";
        salida += "  " + bordeDeGrilla(_tema.bordes()) + "\n";
    }

    return salida;
}

// UNA FILA DE LA GRILLA EN TEXTO PLANO
//
// Es la misma fila que se dibuja en la terminal, pero como texto: las
// celdas van entre los separadores verticales.
std::string PantallaCalendario::filaDeGrillaEnTexto(const FilaDeGrilla& fila) const {
    std::string linea;
    for (const CeldaDeGrilla& celda : fila.celdas) {
        linea += "|" + celda.texto;
    }
    linea += "|";
    return linea;
}

std::string PantallaCalendario::textoDeLaVista() const {
    std::string salida;

    switch (_vista) {
        case Vista::Menu: {
            for (int i = 0; i < _menu.cantidad(); ++i) {
                salida += "  " + std::string(1, _menu.atajoDe(i)) + ". "
                    + _menu.etiquetas()[static_cast<std::size_t>(i)] + "\n";
            }
            return salida;
        }
        case Vista::Anio: {
            if (_campoAnio) {
                salida += "  " + _campoAnio->etiqueta() + ": " + _campoAnio->textoEnPantalla() + "\n";
                if (!_campoAnio->error().empty()) {
                    salida += "      " + _campoAnio->error() + "\n";
                }
            }
            return salida;
        }
        case Vista::Mes: {
            if (_campoMes) {
                salida += "  " + _campoMes->etiqueta() + ": " + _campoMes->textoEnPantalla() + "\n";
                if (!_campoMes->error().empty()) {
                    salida += "      " + _campoMes->error() + "\n";
                }
            }
            return salida;
        }
        case Vista::Grilla: {
            salida += "  Mes " + textoDelMes(_anio, _mes) + "\n";
            salida += textoDeLaGrilla();
            salida += "  Leyenda:  ";
            for (const ParteLeyenda& parte : leyendaDelMapaCalor(_tema.coloresActivos())) {
                salida += parte.texto;
            }
            salida += "\n";
            return salida;
        }
        case Vista::Dia: {
            if (_campoDia) {
                salida += "  " + _campoDia->etiqueta() + ": " + _campoDia->textoEnPantalla() + "\n";
                if (!_campoDia->error().empty()) {
                    salida += "      " + _campoDia->error() + "\n";
                }
            }
            return salida;
        }
        case Vista::Detalle: {
            salida += "  Turnos del " + _fechaDelDetalle.aTexto() + "\n";
            salida += textoDeLaTabla();
            return salida;
        }
        case Vista::Agenda: {
            salida += "  Agenda del " + _diaAgenda.aTexto() + " ("
                + Fecha::nombreDiaSemana(_diaAgenda.diaSemana()) + ")\n";
            salida += textoDeLaTabla();
            return salida;
        }
    }
    return salida;
}

// LA TABLA EN TEXTO PLANO
//
// Las filas con las celdas unidas por " | " (es lo que usan las otras
// pantallas). El estado vacio dice lo que dice y como volver.
std::string PantallaCalendario::textoDeLaTabla() const {
    std::string salida;

    if (_filas.empty()) {
        return "  " + _mensajeVacio + "\n";
    }

    for (const std::vector<std::string>& fila : _filas) {
        for (std::size_t i = 0; i < fila.size(); ++i) {
            if (i > 0) {
                salida += " | ";
            }
            salida += fila[i];
        }
        salida += "\n";
    }

    return salida;
}

std::string PantallaCalendario::vistaTextoPlano() const {
    // SOLO LA VISTA
    //
    // Es lo que se ve ahora, sin el encabezado ni la ayuda: es lo que
    // necesita el test para comprobar que la vista muestra lo que tiene que
    // mostrar.
    return textoDeLaVista();
}

std::string PantallaCalendario::textoPlano() const {
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

std::unique_ptr<Pantalla> crearPantallaCalendario(Aplicacion& aplicacion,
                                                   RepositorioPacientes& pacientes,
                                                   RepositorioMedicos& medicos,
                                                   ServicioTurnos& servicioTurnos,
                                                   const Fecha& hoy) {
    return std::make_unique<PantallaCalendario>(aplicacion, pacientes, medicos, servicioTurnos, hoy);
}

} // namespace Pantallas
} // namespace ui
