#pragma once

#include "entities/Turno.h"
#include "repositories/RepositorioMedicos.h"
#include "repositories/RepositorioPacientes.h"
#include "services/ServicioTurnos.h"
#include "ui/Aplicacion.h"
#include "ui/Campo.h"
#include "ui/ColorFtxui.h"
#include "ui/MapaCalor.h"
#include "ui/Menu.h"
#include "ui/Mensaje.h"
#include "ui/Mes.h"
#include "ui/Pantalla.h"
#include "ui/Tabla.h"
#include "ui/Theme.h"

#include <ftxui.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// La pantalla de CALENDARIO.
//
// QUE ES
//
// Es la SHOWPIECE de la interfaz: una grilla del mes con el mapa de calor de
// la ocupacion, el detalle de un dia y la agenda de un dia.
//
// POR QUE NO ES UNA PANTALLA CRUD MAS
//
// El calendario no registra ni modifica nada: es una CONSULTA con una forma
// distinta a la de una tabla. Por eso tiene su propia pantalla (y no una
// opcion mas del motor CRUD): el motor no sabe mostrar una grilla.
//
// EL MES SE RECORRE CON LAS FLECHAS
//
// La grilla no es una foto: se recorre. Las flechas izquierda y derecha
// cambian de mes (y el anio da la vuelta cuando el mes se pasa), y el mes
// que se esta viendo QUEDA: es el estado de la pantalla.
//
// EL DIA DE LA SEMANA LO DICE EL DOMINIO
//
// La columna de cada dia (lunes, martes, ...) sale de `Fecha::diaSemana()`,
// que es aritmetica sobre el calendario: es DETERMINISTA y testeable. El
// reloj del sistema no se consulta para esto (ver `ui/Reloj.h`).
//
// LA FECHA DE HOY SE RECIBE, NO SE CONSULTA
//
// La pantalla recibe la fecha de HOY por parametro: el que la llama decide
// de donde sale (la aplicacion real le pasa la del reloj del sistema, los
// tests una fija). Asi la pantalla no tiene una regla que no sea la suya, y
// ningun test depende del dia en que se corre.
//
// EL '0' ES VOLVER, EN TODOS LOS NIVELES
//
// Es la regla de UX del proyecto. En las vistas con un CAMPO de texto el '0'
// puede ser un DATO (un anio puede ser 2020), asi que ahi el '0' solo es
// "volver" cuando el campo esta VACIO: si hay algo escrito, el '0' es
// parte del dato.
//
// EL `Escape` SUBE UN NIVEL
//
// En las vistas internas (los campos) el `Escape` sube UN nivel. En el menu
// de la pantalla NO se consume: ahi lo maneja la aplicacion, que saca la
// pantalla entera de la pila.
//
// ---------------------------------------------------------------------------
namespace ui {

namespace Pantallas {

class PantallaCalendario : public Pantalla {
public:
    PantallaCalendario(Aplicacion& aplicacion,
                       RepositorioPacientes& pacientes,
                       RepositorioMedicos& medicos,
                       ServicioTurnos& servicioTurnos,
                       const Fecha& hoy);

    // De Pantalla:
    std::string titulo() const override;
    ftxui::Element renderizar() override;
    bool manejar(const Evento& evento) override;
    std::string textoPlano() const override;

    // Para los tests: en que vista esta y que se ve.
    const std::string& vista() const;
    std::string vistaTextoPlano() const;

    // Para los tests: el mes que se esta viendo.
    int anio() const;
    int mes() const;

    // Para los tests: el dia de la agenda.
    int diaDeLaAgenda() const;

private:
    // LAS VISTAS DE LA MAQUINA DE ESTADOS
    //
    // El nombre de cada vista es el que ve el test (`vista()`): por eso son
    // palabras y no numeros.
    enum class Vista { Menu, Anio, Mes, Grilla, Dia, Detalle, Agenda };

    static const std::string& nombreDeVista(Vista vista);

    // --- La navegacion ---
    void irA(Vista vista);
    void volverAtras();
    bool esTeclaVolver(const Evento& evento) const;
    bool esTeclaEscape(const Evento& evento) const;
    bool esTeclaVolverConCampoVacio(const Evento& evento, const Campo& campo) const;

    // --- Los manejadores de cada vista ---
    bool manejarMenu(const Evento& evento);
    bool manejarCampo(const Evento& evento, const std::unique_ptr<Campo>& campo,
                      const std::function<void()>& alConfirmar);
    bool manejarGrilla(const Evento& evento);
    bool manejarDetalle(const Evento& evento);
    bool manejarAgenda(const Evento& evento);

    // --- El arranque de cada vista ---
    void iniciarVerMes();
    void iniciarDetalleDelDia();
    void iniciarAgenda();

    // --- La confirmacion ---
    void confirmarAnio();
    void confirmarMes();
    void confirmarDia();

    // --- La grilla ---
    void armarGrilla();
    void moverMes(int delta);
    int totalDeTurnosDelMes() const;
    std::vector<SemanaDeGrilla> grilla() const;

    // --- El detalle del dia ---
    void armarDetalle();

    // --- La agenda ---
    void armarAgenda();
    void moverDia(int delta);

    // --- Los mensajes ---
    void avisar(const std::string& texto);
    void avisarExito(const std::string& texto);
    const Mensaje& mensajeActual() const;

    // --- El dibujo ---
    ftxui::Element encabezado() const;
    ftxui::Element lineaOpcion(int indice) const;
    ftxui::Element lineaCampo(const Campo& campo) const;
    ftxui::Element lineaError(const std::string& texto) const;
    ftxui::Element lineaMensaje() const;
    ftxui::Element lineaGrilla() const;
    ftxui::Element lineaLeyenda() const;
    ftxui::Element lineaBordeGrilla() const;
    ftxui::Element lineaCabeceraGrilla() const;
    ftxui::Element filaDeGrilla(const FilaDeGrilla& fila) const;
    ftxui::Element lineaDetalle() const;
    ftxui::Element lineaAgenda() const;
    ftxui::Element tablaDe(const std::vector<std::vector<std::string>>& filas,
                           const std::vector<std::string>& cabeceras) const;
    std::string ayuda() const;

    // --- El texto plano (lo que se ve sin terminal) ---
    std::string textoDeLaVista() const;
    std::string textoDeLaGrilla() const;
    std::string textoDeLaTabla() const;
    std::string filaDeGrillaEnTexto(const FilaDeGrilla& fila) const;

    Aplicacion& _aplicacion;
    RepositorioPacientes& _pacientes;
    RepositorioMedicos& _medicos;
    ServicioTurnos& _servicioTurnos;
    Theme _tema;
    std::string _titulo;
    Menu _menu;

    Vista _vista = Vista::Menu;

    // EL MES QUE SE ESTA VIENDO
    //
    // Es el estado de la pantalla: lo cambian el "ver mes" (con el anio y el
    // mes escritos) y las flechas.
    int _anio = 0;
    int _mes = 0;

    // EL MES QUE SE ESTA ARMANDO
    //
    // Se completa de a dos pasos (el anio y el mes). Hasta que el mes no
    // esta completo, el mes que se esta viendo NO cambia.
    Fecha _mesEnArma;

    // EL DIA QUE SE ESTA VIENDO EN EL DETALLE
    //
    // Es un dia del mes que se esta viendo: la tabla muestra los turnos de
    // ese dia.
    Fecha _fechaDelDetalle;

    // EL DIA DE LA AGENDA
    //
    // Arranca en HOY (la fecha que recibio la pantalla) y lo mueven las
    // flechas.
    Fecha _diaAgenda;

    // LA FECHA DE HOY
    //
    // La que recibio la pantalla. Es el mes con el que arranca el recorrido
    // y el dia con el que arranca la agenda.
    Fecha _hoy;

    // Los campos de las preguntas (el anio, el mes y el dia).
    std::unique_ptr<Campo> _campoAnio;
    std::unique_ptr<Campo> _campoMes;
    std::unique_ptr<Campo> _campoDia;

    // LA GRILLA DEL MES
    //
    // Los contadores van por dia del mes: el indice es el dia menos uno.
    std::vector<int> _turnosPorDia;

    // Las filas de la tabla (la del detalle o la de la agenda) y las
    // columnas con las que se muestran.
    std::vector<std::vector<std::string>> _filas;
    std::vector<std::string> _cabeceras;

    // Lo que se dice cuando el dia no tiene turnos.
    std::string _mensajeVacio;
};

// La pantalla de calendario, lista para apilarse en la aplicacion.
std::unique_ptr<Pantalla> crearPantallaCalendario(Aplicacion& aplicacion,
                                                   RepositorioPacientes& pacientes,
                                                   RepositorioMedicos& medicos,
                                                   ServicioTurnos& servicioTurnos,
                                                   const Fecha& hoy);

} // namespace Pantallas
} // namespace ui
