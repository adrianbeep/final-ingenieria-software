#pragma once

#include "ui/Aplicacion.h"
#include "ui/Pantalla.h"
#include "ui/Tabla.h"
#include "ui/Theme.h"

#include <ftxui.hpp>

#include <memory>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// La pantalla de ACERCA DE.
//
// QUE ES
//
// Es la pantalla que dice QUE es esto y COMO se usa. No tiene acciones: es de
// SOLO LECTURA.
//
// POR QUE NO TIENE ACCIONES
//
// "Acerca de" no configura nada: informa. La unica accion que tiene es
// VOLVER, que no es una accion del modulo sino la de todas las pantallas.
//
// QUE CONTIENE
//
//   - los datos del trabajo Practico (el sistema, la carrera, el autor);
//   - la tabla de ATAJOS de teclado y de mouse;
//   - los creditos de FTXUI (la version, la licencia y donde esta el codigo
//     de terceros).
//
// LA TABLA DE ATAJOS
//
// Es la unica parte que "miente" un poco: no todos los atajos valen en todas
// las pantallas (el `Tab` solo tiene sentido en un formulario). Por eso la
// tabla dice "que hace" y no "donde".
//
// ---------------------------------------------------------------------------
namespace ui {

namespace Pantallas {

class PantallaAcercaDe : public Pantalla {
public:
    explicit PantallaAcercaDe(Aplicacion& aplicacion);

    // De Pantalla:
    std::string titulo() const override;
    ftxui::Element renderizar() override;
    bool manejar(const Evento& evento) override;
    std::string textoPlano() const override;

    // Para los tests: que se ve.
    std::string vistaTextoPlano() const;

private:
    // --- La navegacion ---
    bool esTeclaVolver(const Evento& evento) const;
    bool esTeclaEscape(const Evento& evento) const;

    // --- El dibujo ---
    ftxui::Element encabezado() const;
    ftxui::Element lineaDato(const std::string& etiqueta, const std::string& valor) const;
    ftxui::Element tablaDeAtajos() const;
    std::string ayuda() const;

    // --- El texto plano (lo que se ve sin terminal) ---
    std::string textoDeLaVista() const;

    // LAS FILAS DE LA TABLA DE ATAJOS
    //
    // Son datos, no logica: viven en una funcion estatica para que el
    // render y el texto plano muestren EXACTAMENTE lo mismo.
    static const std::vector<std::vector<std::string>>& filasDeAtajos();
    static const std::vector<std::string>& cabecerasDeAtajos();

    // LOS DATOS DEL TRABAJO PRACTICO
    static const std::vector<std::pair<std::string, std::string>>& datosDelTrabajo();

    Aplicacion& _aplicacion;
    Theme _tema;
    std::string _titulo;
};

// La pantalla de "acerca de", lista para apilarse en la aplicacion.
std::unique_ptr<Pantalla> crearPantallaAcercaDe(Aplicacion& aplicacion);

} // namespace Pantallas
} // namespace ui
