#pragma once

#include "entities/Turno.h"
#include "repositories/RepositorioMedicos.h"
#include "repositories/RepositorioPacientes.h"
#include "services/ServicioTurnos.h"
#include "ui/Aplicacion.h"
#include "ui/Campo.h"
#include "ui/ColorFtxui.h"
#include "ui/Menu.h"
#include "ui/Mensaje.h"
#include "ui/Pantalla.h"
#include "ui/Selector.h"
#include "ui/Tabla.h"
#include "ui/Theme.h"

#include <ftxui.hpp>

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// La pantalla de REPORTES.
//
// QUE ES Y QUE NO ES
//
// Un reporte NO es una pantalla CRUD: no registra nada, no modifica nada y
// no da de baja. Es una CONSULTA con una tabla y un total. Por eso tiene su
// propia pantalla (y no una opcion mas del motor CRUD): el motor no sabe
// mostrar un resultado que no sea una entidad.
//
// LOS REPORTES MUESTRAN LOS TURNOS DE CUALQUIER ESTADO
//
// Los cuatro reportes de turnos usan las consultas de `ServicioTurnos` que
// devuelven los turnos de TODOS los estados (`Activo`, `Cancelado` y
// `Reprogramado`), no `turnosVigentesDe`. Un reporte tiene que poder mostrar
// tambien los cancelados: son parte de la historia de la agenda.
//
// LA PANTALLA NO REIMPLEMENTA NINGUNA REGLA
//
// Las consultas son del SERVICIO y las filas son de las ENTIDADES (el turno
// trae su fecha, su hora y su estado; el nombre del paciente y del medico
// lo resuelve el repositorio). La pantalla solo arma la tabla y muestra lo
// que le dieron.
//
// EL '0' ES VOLVER, EN TODOS LOS NIVELES
//
// Es la regla de UX del proyecto. En las vistas con un CAMPO de texto el '0'
// puede ser un DATO (un id puede ser 10), asi que ahi el '0' solo es
// "volver" cuando el campo esta VACIO: si hay algo escrito, el '0' es
// parte del dato.
//
// EL `Escape` SUBE UN NIVEL
//
// En las vistas internas (el campo y el selector) el `Escape` sube UN
// nivel. En el menu de la pantalla NO se consume: ahi lo maneja la
// aplicacion, que saca la pantalla entera de la pila.
//
// ---------------------------------------------------------------------------
namespace ui {

namespace Pantallas {

class PantallaReportes : public Pantalla {
public:
    PantallaReportes(Aplicacion& aplicacion,
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
    enum class Vista { Menu, Campo, Selector, Resultado, Ocupacion };

    // QUE REPORTE SE ESTA ARMANDO
    //
    // El reporte decide QUE se pregunta (un campo o el selector) y QUE se
    // muestra (las columnas de la tabla). Las cuatro comparten el mismo
    // camino: se elige el reporte del menu, se responde su pregunta y se ve
    // la tabla con el total.
    enum class Reporte {
        TurnosPorFecha,
        TurnosPorPaciente,
        TurnosPorMedico,
        TurnosPorEspecialidad,
        OcupacionPorSemana
    };

    static const std::string& nombreDeVista(Vista vista);

    // --- La navegacion ---
    void irA(Vista vista);
    void volverAtras();
    bool esTeclaVolver(const Evento& evento) const;
    bool esTeclaEscape(const Evento& evento) const;
    bool esTeclaVolverConCampoVacio(const Evento& evento, const Campo& campo) const;

    // --- Los manejadores de cada vista ---
    bool manejarMenu(const Evento& evento);
    bool manejarCampo(const Evento& evento);
    bool manejarSelector(const Evento& evento);
    bool manejarLectura(const Evento& evento);

    // --- El arranque de cada reporte ---
    void iniciarTurnosPorFecha();
    void iniciarTurnosPorPaciente();
    void iniciarTurnosPorMedico();
    void iniciarOcupacion();
    void abrirSelector();

    // --- La confirmacion ---
    void confirmarCampo();
    void confirmarSeleccion();

    // --- La ocupacion de la semana ---
    void armarOcupacion(const Fecha& fecha);
    std::string mensajeDeOcupacion() const;

    // --- Los mensajes ---
    void avisar(const std::string& texto);
    void avisarExito(const std::string& texto);
    const Mensaje& mensajeActual() const;

    // --- El dibujo ---
    ftxui::Element encabezado() const;
    ftxui::Element lineaOpcion(int indice) const;
    ftxui::Element lineaOpcionSelector(int indice) const;
    ftxui::Element lineaNumeroSelector() const;
    ftxui::Element lineaCampo(const Campo& campo) const;
    ftxui::Element lineaError(const std::string& texto) const;
    ftxui::Element lineaMensaje() const;
    ftxui::Element lineaResultado() const;
    ftxui::Element lineaOcupacion() const;
    ftxui::Element tablaDe(const std::vector<std::vector<std::string>>& filas,
                           const std::vector<std::string>& cabeceras) const;
    std::string ayuda() const;

    // --- El texto plano (lo que se ve sin terminal) ---
    std::string textoDeLaVista() const;

    Aplicacion& _aplicacion;
    RepositorioPacientes& _pacientes;
    RepositorioMedicos& _medicos;
    ServicioTurnos& _servicioTurnos;
    Theme _tema;
    std::string _titulo;
    Menu _menu;

    Vista _vista = Vista::Menu;
    Reporte _reporte = Reporte::TurnosPorFecha;

    // El campo de la pregunta del reporte (la fecha o el id).
    std::unique_ptr<Campo> _campo;

    // El selector de la especialidad.
    std::unique_ptr<Selector> _selector;

    // Las filas del reporte y las columnas con las que se muestran.
    std::vector<std::vector<std::string>> _filas;
    std::vector<std::string> _cabeceras;

    // Lo que se dice cuando el reporte no tiene filas.
    std::string _mensajeVacio;
};

// La pantalla de reportes, lista para apilarse en la aplicacion.
std::unique_ptr<Pantalla> crearPantallaReportes(Aplicacion& aplicacion,
                                                 RepositorioPacientes& pacientes,
                                                 RepositorioMedicos& medicos,
                                                 ServicioTurnos& servicioTurnos);

} // namespace Pantallas
} // namespace ui
