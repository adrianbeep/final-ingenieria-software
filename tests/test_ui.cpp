#include "test_framework.h"

#include "exceptions/ExcepcionesSistema.h"
#include "ui/ColorFtxui.h"
#include "ui/Evento.h"
#include "ui/Menu.h"
#include "ui/Terminal.h"
#include "ui/Theme.h"

#include <ftxui.hpp>

#include <cstdlib>
#include <string>

// ---------------------------------------------------------------------------
// Tests de la capa de UI.
//
// Son tests de LOGICA PURA: no abren el ciclo de eventos de FTXUI ni
// necesitan una terminal. Lo que se testea son las tres piezas que se
// pueden testear sin terminal:
//
//   - la paleta (`ui::Theme`): dato y logica de `NO_COLOR`;
//   - el menu (`ui::Menu`): seleccion, navegacion, atajos y hit-testing;
//   - la traduccion de eventos (`ui::eventoDesde`): una funcion pura.
//
// El resto (el ciclo de eventos, el alternate screen, el guard de terminal)
// se verifica por ejecucion, no por test unitario.
// ---------------------------------------------------------------------------

namespace {

// Poner y sacar la variable de entorno `NO_COLOR`.
//
// En Windows `_putenv_s(nombre, "")` borra la variable; en POSIX eso seria
// "definida pero vacia", que segun la especificacion de NO_COLOR NO apaga
// los colores. En los dos casos el resultado es "no hay NO_COLOR".
void definirNoColor(const char* valor) {
#if defined(_WIN32)
    _putenv_s("NO_COLOR", valor);
#else
    if (valor == nullptr) {
        unsetenv("NO_COLOR");
    } else {
        setenv("NO_COLOR", valor, 1);
    }
#endif
}

void borrarNoColor() {
    definirNoColor(nullptr);
}

// Un evento de mouse de FTXUI.
ftxui::Event eventoMouse(ftxui::Mouse::Button boton, ftxui::Mouse::Motion motion,
                         int x, int y) {
    ftxui::Mouse mouse;
    mouse.button = boton;
    mouse.motion = motion;
    mouse.x = x;
    mouse.y = y;
    return ftxui::Event::Mouse("mouse", mouse);
}

// Un `ui::Evento` de un caracter.
ui::Evento eventoCaracter(char caracter) {
    ui::Evento evento;
    evento.tipo = ui::TipoTecla::Caracter;
    evento.caracter = std::string(1, caracter);
    return evento;
}

} // namespace

// ---------------------------------------------------------------------------
// Theme
// ---------------------------------------------------------------------------

TEST_CASE("sin NO_COLOR el tema propone colores", tema_sin_no_color_propone_colores) {
    borrarNoColor();
    ui::Theme::recargarEntorno();

    CHECK(ui::Theme::coloresActivosPorDefecto() == true);

    const ui::Theme tema;
    CHECK(tema.coloresActivos() == true);
    CHECK(tema.colorDe(ui::Rol::Titulo).has_value() == true);
    CHECK(tema.colorDe(ui::Rol::Exito).has_value() == true);
    CHECK(tema.colorDe(ui::Rol::Error).has_value() == true);
}

TEST_CASE("con NO_COLOR el tema no propone ningun color", tema_con_no_color_no_propone) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    CHECK(ui::Theme::coloresActivosPorDefecto() == false);

    const ui::Theme tema;
    CHECK(tema.coloresActivos() == false);
    // Ningun rol propone color: la pantalla dibuja el texto tal cual.
    CHECK(tema.colorDe(ui::Rol::Titulo).has_value() == false);
    CHECK(tema.colorDe(ui::Rol::Subtitulo).has_value() == false);
    CHECK(tema.colorDe(ui::Rol::Exito).has_value() == false);
    CHECK(tema.colorDe(ui::Rol::Error).has_value() == false);
    CHECK(tema.colorDe(ui::Rol::Advertencia).has_value() == false);
    CHECK(tema.colorDe(ui::Rol::TablaHeader).has_value() == false);
    CHECK(tema.colorDe(ui::Rol::TablaCelda).has_value() == false);
    CHECK(tema.colorDe(ui::Rol::TablaBorde).has_value() == false);

    // La paleta pura sigue existiendo: apagar los colores no la borra.
    CHECK(tema.titulo().r >= 0 && tema.titulo().r <= 255);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

TEST_CASE("el tema se puede forzar con o sin colores", tema_forzado) {
    const ui::Theme con( true);
    const ui::Theme sin(false);

    CHECK(con.coloresActivos() == true);
    CHECK(con.colorDe(ui::Rol::Titulo).has_value() == true);
    CHECK(sin.coloresActivos() == false);
    CHECK(sin.colorDe(ui::Rol::Titulo).has_value() == false);
}

TEST_CASE("los caracteres de borde son los esperados", tema_bordes) {
    const ui::Theme tema;
    const ui::Bordes& bordes = tema.bordes();

    CHECK(bordes.esquinaSuperiorIzquierda == '+');
    CHECK(bordes.esquinaSuperiorDerecha == '+');
    CHECK(bordes.esquinaInferiorIzquierda == '+');
    CHECK(bordes.esquinaInferiorDerecha == '+');
    CHECK(bordes.horizontal == '-');
    CHECK(bordes.vertical == '|');
}

TEST_CASE("el color propio se traduce al color de FTXUI sin perder nada",
           tema_color_a_ftxui) {
    const ui::Color propio{12, 200, 255};
    const ftxui::Color traducido = ui::aFtxui(propio);

    // `ftxui::Color` no expone sus componentes, asi que la comparacion es
    // contra el color RGB equivalente.
    CHECK(traducido == ftxui::Color::RGB(12, 200, 255));
    CHECK(traducido != ftxui::Color::RGB(12, 200, 254));
}

// ---------------------------------------------------------------------------
// Menu
// ---------------------------------------------------------------------------

TEST_CASE("el menu guarda las opciones agregadas", menu_agrega_opciones) {
    ui::Menu menu("Titulo");
    menu.agregar('1', "Primera", [] {});
    menu.agregar('0', "Volver", [] {});

    CHECK(menu.cantidad() == 2);
    CHECK(menu.titulo() == "Titulo");
    CHECK(menu.etiquetas().size() == 2);
    CHECK(menu.etiquetas()[0] == "Primera");
    CHECK(menu.etiquetas()[1] == "Volver");
    CHECK(menu.atajoDe(0) == '1');
    CHECK(menu.atajoDe(1) == '0');
    CHECK(menu.seleccion() == 0);
}

TEST_CASE("las flechas mueven la seleccion y se detienen en el extremo",
           menu_navegacion_con_flechas) {
    ui::Menu menu("Titulo");
    menu.agregar('1', "Primera", [] {});
    menu.agregar('2', "Segunda", [] {});
    menu.agregar('3', "Tercera", [] {});

    CHECK(menu.moverAbajo() == true);
    CHECK(menu.seleccion() == 1);
    CHECK(menu.moverAbajo() == true);
    CHECK(menu.seleccion() == 2);

    // Abajo en la ultima opcion: la seleccion NO da la vuelta.
    CHECK(menu.moverAbajo() == false);
    CHECK(menu.seleccion() == 2);

    CHECK(menu.moverArriba() == true);
    CHECK(menu.seleccion() == 1);
    CHECK(menu.moverArriba() == true);
    CHECK(menu.seleccion() == 0);

    // Arriba en la primera opcion: tampoco da la vuelta.
    CHECK(menu.moverArriba() == false);
    CHECK(menu.seleccion() == 0);
}

TEST_CASE("las flechas del menu se aplican con el evento", menu_eventos_de_flecha) {
    ui::Menu menu("Titulo");
    menu.agregar('1', "Primera", [] {});
    menu.agregar('2', "Segunda", [] {});

    ui::Evento abajo;
    abajo.tipo = ui::TipoTecla::FlechaAbajo;
    CHECK(menu.aplicarEvento(abajo) == true);
    CHECK(menu.seleccion() == 1);

    ui::Evento arriba;
    arriba.tipo = ui::TipoTecla::FlechaArriba;
    CHECK(menu.aplicarEvento(arriba) == true);
    CHECK(menu.seleccion() == 0);
}

TEST_CASE("el atajo numerico ejecuta la opcion correcta", menu_atajo_numerico) {
    int ejecutadas = 0;
    ui::Menu menu("Titulo");
    menu.agregar('1', "Primera", [&ejecutadas]() { ++ejecutadas; });
    menu.agregar('2', "Segunda", [&ejecutadas]() { ++ejecutadas; });
    menu.agregar('0', "Volver", [&ejecutadas]() { ++ejecutadas; });

    CHECK(menu.aplicarEvento(eventoCaracter('2')) == true);
    CHECK(ejecutadas == 1);

    // El atajo ejecuta la opcion que TIENE ese atajo, no la seleccionada.
    menu.seleccionar(0);
    CHECK(menu.aplicarEvento(eventoCaracter('0')) == true);
    CHECK(ejecutadas == 2);
}

TEST_CASE("un digito que no es atajo no lo consume el menu", menu_digito_sin_atajo) {
    ui::Menu menu("Titulo");
    menu.agregar('1', "Primera", [] {});

    CHECK(menu.aplicarEvento(eventoCaracter('7')) == false);
    // Una letra tampoco.
    CHECK(menu.aplicarEvento(eventoCaracter('a')) == false);
    // Un caracter de mas de un byte tampoco.
    ui::Evento conTilde = eventoCaracter('a');
    conTilde.caracter = "\xC3\xB1";
    CHECK(menu.aplicarEvento(conTilde) == false);
}

TEST_CASE("Enter ejecuta la opcion seleccionada", menu_enter) {
    int ejecutadas = 0;
    ui::Menu menu("Titulo");
    menu.agregar('1', "Primera", [&ejecutadas]() { ++ejecutadas; });
    menu.agregar('2', "Segunda", [&ejecutadas]() { ++ejecutadas; });

    menu.seleccionar(1);
    ui::Evento enter;
    enter.tipo = ui::TipoTecla::Enter;
    CHECK(menu.aplicarEvento(enter) == true);
    CHECK(ejecutadas == 1);
}

TEST_CASE("un atajo repetido es un error de programacion", menu_atajo_repetido) {
    ui::Menu menu("Titulo");
    menu.agregar('1', "Primera", [] {});

    bool lanzo = false;
    try {
        menu.agregar('1', "Otra", [] {});
    } catch (const DatoInvalidoException& e) {
        lanzo = true;
        CHECK(std::string(e.campo()) == "atajo");
        CHECK(std::string(e.valor()) == "1");
    }
    CHECK(lanzo == true);

    // El menu no quedo con la opcion repetida.
    CHECK(menu.cantidad() == 1);
}

TEST_CASE("un atajo que no es un digito es un error de programacion",
           menu_atajo_no_digito) {
    ui::Menu menu("Titulo");

    bool lanzo = false;
    try {
        menu.agregar('x', "Invalida", [] {});
    } catch (const DatoInvalidoException&) {
        lanzo = true;
    }
    CHECK(lanzo == true);
    CHECK(menu.cantidad() == 0);
}

TEST_CASE("el hit-testing del raton devuelve el indice de la fila",
           menu_hit_testing) {
    ui::Menu menu("Titulo");
    menu.agregar('1', "Primera", [] {});
    menu.agregar('2', "Segunda", [] {});
    menu.agregar('3', "Tercera", [] {});

    // La primera opcion esta en la fila `filaInicial`.
    CHECK(menu.opcionEnFila(2, 2) == 0);
    CHECK(menu.opcionEnFila(3, 2) == 1);
    CHECK(menu.opcionEnFila(4, 2) == 2);

    // Fuera de rango: -1.
    CHECK(menu.opcionEnFila(1, 2) == -1);
    CHECK(menu.opcionEnFila(5, 2) == -1);
    CHECK(menu.opcionEnFila(-1, 2) == -1);
}

TEST_CASE("el clic del raton ejecuta la opcion de esa fila", menu_clic) {
    int ejecutadas = 0;
    ui::Menu menu("Titulo");
    menu.agregar('1', "Primera", [&ejecutadas]() { ++ejecutadas; });
    menu.agregar('2', "Segunda", [&ejecutadas]() { ++ejecutadas; });
    menu.definirFilaInicial(2);

    ui::Evento clic;
    clic.boton = ui::BotonMouse::Izquierdo;
    clic.x = 4;
    clic.y = 3;  // la segunda opcion

    CHECK(menu.aplicarEvento(clic) == true);
    CHECK(ejecutadas == 1);

    // Un clic que cae fuera de las opciones no se consume.
    ui::Evento fuera;
    fuera.boton = ui::BotonMouse::Izquierdo;
    fuera.x = 0;
    fuera.y = 40;
    CHECK(menu.aplicarEvento(fuera) == false);
    CHECK(ejecutadas == 1);
}

TEST_CASE("el texto plano del menu tiene el titulo y las etiquetas",
           menu_texto_plano) {
    ui::Menu menu("Demo del motor");
    menu.agregar('1', "Paleta de colores", [] {});
    menu.agregar('0', "Salir", [] {});

    const std::string texto = menu.textoPlano();
    CHECK(texto.find("Demo del motor") != std::string::npos);
    CHECK(texto.find("Paleta de colores") != std::string::npos);
    CHECK(texto.find("Salir") != std::string::npos);
    // El atajo va adelante de la etiqueta.
    CHECK(texto.find("1. Paleta de colores") != std::string::npos);
    CHECK(texto.find("0. Salir") != std::string::npos);
}

// ---------------------------------------------------------------------------
// La traduccion de eventos
// ---------------------------------------------------------------------------

TEST_CASE("la flecha arriba de FTXUI se traduce a FlechaArriba",
           evento_flecha_arriba) {
    const ui::Evento propio = ui::eventoDesde(ftxui::Event::ArrowUp);
    CHECK(propio.tipo == ui::TipoTecla::FlechaArriba);
    CHECK(propio.caracter.empty() == true);
    CHECK(propio.esMouse() == false);
}

TEST_CASE("la flecha abajo de FTXUI se traduce a FlechaAbajo", evento_flecha_abajo) {
    const ui::Evento propio = ui::eventoDesde(ftxui::Event::ArrowDown);
    CHECK(propio.tipo == ui::TipoTecla::FlechaAbajo);
}

TEST_CASE("Enter de FTXUI se traduce a Enter", evento_enter) {
    // OJO: en FTXUI v7 la tecla Enter es `Event::Return` (no existe
    // `Event::Enter`).
    const ui::Evento propio = ui::eventoDesde(ftxui::Event::Return);
    CHECK(propio.tipo == ui::TipoTecla::Enter);
}

TEST_CASE("Escape de FTXUI se traduce a Escape", evento_escape) {
    const ui::Evento propio = ui::eventoDesde(ftxui::Event::Escape);
    CHECK(propio.tipo == ui::TipoTecla::Escape);
}

TEST_CASE("un caracter de FTXUI se traduce con su caracter", evento_caracter) {
    const ui::Evento propio = ui::eventoDesde(ftxui::Event::Character('7'));
    CHECK(propio.tipo == ui::TipoTecla::Caracter);
    CHECK(propio.caracter == "7");
}

TEST_CASE("el espacio de FTXUI se traduce a Espacio", evento_espacio) {
    // FTXUI no tiene `Event::Space`: el espacio llega como un caracter.
    const ui::Evento propio = ui::eventoDesde(ftxui::Event::Character(' '));
    CHECK(propio.tipo == ui::TipoTecla::Espacio);
}

TEST_CASE("el tabulador y el retroceso se traducen", evento_tab_y_retroceso) {
    CHECK(ui::eventoDesde(ftxui::Event::Tab).tipo == ui::TipoTecla::Tabulador);
    CHECK(ui::eventoDesde(ftxui::Event::Backspace).tipo == ui::TipoTecla::Retroceso);
}

TEST_CASE("un evento de mouse se traduce con el boton y las coordenadas",
           evento_mouse) {
    const ui::Evento propio =
        ui::eventoDesde(eventoMouse(ftxui::Mouse::Left, ftxui::Mouse::Pressed, 7, 3));

    CHECK(propio.boton == ui::BotonMouse::Izquierdo);
    CHECK(propio.x == 7);
    CHECK(propio.y == 3);
    CHECK(propio.esClic() == true);
    CHECK(propio.esMouse() == true);
}

TEST_CASE("el boton derecho y la rueda tambien se traducen", evento_mouse_buttons) {
    const ui::Evento derecho =
        ui::eventoDesde(eventoMouse(ftxui::Mouse::Right, ftxui::Mouse::Pressed, 1, 2));
    CHECK(derecho.boton == ui::BotonMouse::Derecho);
    CHECK(derecho.esClic() == false);

    const ui::Evento ruedaArriba =
        ui::eventoDesde(eventoMouse(ftxui::Mouse::WheelUp, ftxui::Mouse::Pressed, 0, 0));
    CHECK(ruedaArriba.boton == ui::BotonMouse::RuedaArriba);

    const ui::Evento ruedaAbajo =
        ui::eventoDesde(eventoMouse(ftxui::Mouse::WheelDown, ftxui::Mouse::Pressed, 0, 0));
    CHECK(ruedaAbajo.boton == ui::BotonMouse::RuedaAbajo);
}

TEST_CASE("soltar el boton no es un clic", evento_mouse_soltado) {
    const ui::Evento propio =
        ui::eventoDesde(eventoMouse(ftxui::Mouse::Left, ftxui::Mouse::Released, 7, 3));

    CHECK(propio.esClic() == false);
    CHECK(propio.boton == ui::BotonMouse::Ninguno);
    // Pero las coordenadas siguen llegando: el raton esta en ese lugar.
    CHECK(propio.x == 7);
    CHECK(propio.y == 3);
}

// ---------------------------------------------------------------------------
// Terminal
// ---------------------------------------------------------------------------

TEST_CASE("el chequeo de terminal no revienta", terminal_hay_terminal) {
    // El RESULTADO depende del entorno (no se puede testear), pero la
    // llamada no puede fallar: es la que decide si el programa entra al
    // alternate screen o imprime el menu en texto plano.
    const bool hay = ui::Terminal::hayTerminal();
    CHECK(hay == true || hay == false);
}
