#include "test_framework.h"

#include "domain/Especialidad.h"
#include "entities/Paciente.h"
#include "entities/Medico.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "repositories/RepositorioPacientes.h"
#include "repositories/RepositorioMedicos.h"
#include "repositories/RepositorioTurnos.h"
#include "services/ServicioTurnos.h"
#include "ui/Aplicacion.h"
#include "ui/Campo.h"
#include "ui/ColorFtxui.h"
#include "ui/Confirmacion.h"
#include "ui/Consola.h"
#include "ui/Contexto.h"
#include "ui/CrudPersonas.h"
#include "ui/Evento.h"
#include "ui/Menu.h"
#include "ui/Mensaje.h"
#include "ui/Pantallas/Pacientes.h"
#include "ui/Pantallas/Medicos.h"
#include "ui/Tabla.h"
#include "ui/Terminal.h"
#include "ui/Theme.h"
#include "utils/Fecha.h"

#include <ftxui.hpp>

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Tests de la capa de UI.
//
// Son tests de LOGICA PURA: no abren el ciclo de eventos de FTXUI ni
// necesitan una terminal. Lo que se testea son las piezas que se pueden
// testear sin terminal:
//
//   - la paleta (`ui::Theme`): dato y logica de `NO_COLOR`;
//   - el menu (`ui::Menu`): seleccion, navegacion, atajos y hit-testing;
//   - la traduccion de eventos (`ui::eventoDesde`): una funcion pura;
//   - las validaciones (`ui::Consola`): funciones puras;
//   - el campo de entrada (`ui::Campo`): acumula, valida y reintenta;
//   - la confirmacion (`ui::Confirmacion`) y el mensaje (`ui::Mensaje`);
//   - la tabla (`ui::Tabla`): el render se prueba sobre un `ftxui::Screen`.
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

// Un `ui::Evento` de una tecla que no es un caracter.
ui::Evento eventoTecla(ui::TipoTecla tipo) {
    ui::Evento evento;
    evento.tipo = tipo;
    return evento;
}

// Renderiza un elemento en un `ftxui::Screen` y devuelve las lineas.
//
// LOS NOMBRES EXACTOS DE FTXUI QUE SE USAN
//
//   - `ftxui::Screen::Create(ftxui::Dimension::Fixed(ancho),
//                             ftxui::Dimension::Fixed(alto))`: crea el screen
//     del tamano pedido (es lo que permite comparar el texto linea por
//     linea).
//   - `ftxui::Render(pantalla, elemento)`: dibuja el elemento en el screen.
//   - `pantalla.ToString()`: devuelve el texto del screen, con las lineas
//     separadas por "\r\n".
std::vector<std::string> renderizarLineas(const ftxui::Element& elemento, int ancho,
                                          int alto) {
    ftxui::Screen pantalla = ftxui::Screen::Create(ftxui::Dimension::Fixed(ancho),
                                                   ftxui::Dimension::Fixed(alto));
    ftxui::Render(pantalla, elemento);

    std::vector<std::string> lineas;
    std::string actual;
    for (const char caracter : pantalla.ToString()) {
        if (caracter == '\r') {
            continue;
        }
        if (caracter == '\n') {
            lineas.push_back(actual);
            actual.clear();
            continue;
        }
        actual += caracter;
    }
    lineas.push_back(actual);
    return lineas;
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

// ---------------------------------------------------------------------------
// Consola: las validaciones (funciones puras)
// ---------------------------------------------------------------------------

TEST_CASE("validarEntero devuelve el entero cuando esta en rango",
           consola_validar_entero_ok) {
    CHECK(ui::Consola::validarEntero("42", 0, 120) == 42);
    CHECK(ui::Consola::validarEntero("0", 0, 120) == 0);
    CHECK(ui::Consola::validarEntero("120", 0, 120) == 120);
    // Los espacios alrededor no son un error: el usuario los escribe.
    CHECK(ui::Consola::validarEntero("  7  ", 0, 120) == 7);
}

TEST_CASE("validarEntero distingue la entrada vacia", consola_validar_entero_vacio) {
    const ErrorCapturado error = capturarError([]() {
        (void)ui::Consola::validarEntero("   ", 0, 120);
    });

    CHECK(error.lanzo == true);
    CHECK(error.campo == "entrada");
    // El mensaje dice la verdad: no se escribio NADA.
    CHECK(error.mensaje.find("vacia") != std::string::npos);
}

TEST_CASE("validarEntero distingue la entrada no numerica",
           consola_validar_entero_no_numerico) {
    const ErrorCapturado error = capturarError([]() {
        (void)ui::Consola::validarEntero("abc", 0, 120);
    });

    CHECK(error.lanzo == true);
    CHECK(error.campo == "entrada");
    CHECK(error.mensaje.find("Entrada invalida") != std::string::npos);
    CHECK(error.mensaje.find("abc") != std::string::npos);
}

TEST_CASE("validarEntero distingue el valor demasiado grande",
           consola_validar_entero_demasiado_grande) {
    const ErrorCapturado error = capturarError([]() {
        (void)ui::Consola::validarEntero("99999999999999999999", 0, 120);
    });

    CHECK(error.lanzo == true);
    CHECK(error.campo == "entrada");
    // El texto ES un numero: decir "ingrese un numero entero" seria
    // mentiroso.
    CHECK(error.mensaje.find("demasiado grande") != std::string::npos);
}

TEST_CASE("validarEntero distingue el valor fuera de rango",
           consola_validar_entero_fuera_de_rango) {
    const ErrorCapturado error = capturarError([]() {
        (void)ui::Consola::validarEntero("999", 0, 120);
    });

    CHECK(error.lanzo == true);
    CHECK(error.campo == "entrada");
    CHECK(error.mensaje.find("entre 0 y 120") != std::string::npos);
}

TEST_CASE("validarFecha devuelve la fecha y falla con el campo fecha",
           consola_validar_fecha) {
    const Fecha fecha = ui::Consola::validarFecha("2026-04-01");
    CHECK(fecha.anio == 2026);
    CHECK(fecha.mes == 4);
    CHECK(fecha.dia == 1);

    // El 31 de febrero no existe en el calendario.
    const ErrorCapturado error = capturarError([]() {
        (void)ui::Consola::validarFecha("2026-02-31");
    });
    CHECK(error.lanzo == true);
    CHECK(error.campo == "fecha");

    // Y el formato tampoco vale.
    const ErrorCapturado errorFormato = capturarError([]() {
        (void)ui::Consola::validarFecha("01/04/2026");
    });
    CHECK(errorFormato.lanzo == true);
    CHECK(errorFormato.campo == "fecha");
}

TEST_CASE("validarHora devuelve la hora y falla con el campo hora",
           consola_validar_hora) {
    const Hora hora = ui::Consola::validarHora("08:30");
    CHECK(hora.hora == 8);
    CHECK(hora.minuto == 30);

    // Las 25:00 no son una hora.
    const ErrorCapturado error = capturarError([]() {
        (void)ui::Consola::validarHora("25:00");
    });
    CHECK(error.lanzo == true);
    CHECK(error.campo == "hora");

    // Y el formato tampoco vale.
    const ErrorCapturado errorFormato = capturarError([]() {
        (void)ui::Consola::validarHora("8h30");
    });
    CHECK(errorFormato.lanzo == true);
    CHECK(errorFormato.campo == "hora");
}

// ---------------------------------------------------------------------------
// Campo: el widget de entrada
// ---------------------------------------------------------------------------

TEST_CASE("el campo acumula los caracteres que se escriben", campo_acumula) {
    ui::Campo campo("Nombre", "", [](const std::string&) { return std::string(); });

    campo.aplicarEvento(eventoCaracter('A'));
    campo.aplicarEvento(eventoCaracter('n'));
    campo.aplicarEvento(eventoCaracter('a'));

    CHECK(campo.texto() == "Ana");
    CHECK(campo.etiqueta() == "Nombre");
}

TEST_CASE("el campo inserta en la posicion del cursor", campo_inserta_en_el_medio) {
    ui::Campo campo("Nombre", "", [](const std::string&) { return std::string(); });

    campo.aplicarEvento(eventoCaracter('a'));
    campo.aplicarEvento(eventoCaracter('c'));
    // El cursor va para atras (entre la 'a' y la 'c').
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    campo.aplicarEvento(eventoCaracter('b'));

    CHECK(campo.texto() == "abc");
}

TEST_CASE("las flechas mueven el cursor y no se pasan del texto",
           campo_cursor) {
    ui::Campo campo("Nombre", "", [](const std::string&) { return std::string(); });

    campo.aplicarEvento(eventoCaracter('a'));
    campo.aplicarEvento(eventoCaracter('b'));

    // A la derecha del todo: el cursor queda al final.
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaDerecha));
    CHECK(campo.textoEnPantalla() == "ab|");

    // Una flecha a la derecha: no se pasa del final.
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaDerecha));
    CHECK(campo.textoEnPantalla() == "ab|");

    // Una flecha a la izquierda: el cursor queda antes de la 'b'.
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    CHECK(campo.textoEnPantalla() == "a|b");

    // Y hasta el principio.
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    CHECK(campo.textoEnPantalla() == "|ab");
}

TEST_CASE("el retroceso borra el caracter que esta antes del cursor",
           campo_retroceso) {
    ui::Campo campo("Nombre", "", [](const std::string&) { return std::string(); });

    campo.aplicarEvento(eventoCaracter('a'));
    campo.aplicarEvento(eventoCaracter('b'));
    campo.aplicarEvento(eventoCaracter('c'));
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::Retroceso));

    CHECK(campo.texto() == "ab");

    // Con el cursor al principio, el retroceso no hace nada.
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::Retroceso));
    CHECK(campo.texto() == "ab");
}

TEST_CASE("el campo valida al presionar Enter y cuenta los intentos",
           campo_validacion) {
    ui::Campo campo("Edad", "", [](const std::string& texto) {
        return texto == "42" ? std::string() : std::string("no es 42");
    });

    campo.aplicarEvento(eventoCaracter('a'));
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    CHECK(campo.intentos() == 1);
    CHECK(campo.confirmado() == false);
    CHECK(campo.error() == "no es 42");

    // Un intento valido: el error se borra y el campo queda confirmado.
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::Retroceso));
    campo.aplicarEvento(eventoCaracter('4'));
    campo.aplicarEvento(eventoCaracter('2'));
    campo.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    CHECK(campo.intentos() == 1);
    CHECK(campo.confirmado() == true);
    CHECK(campo.error().empty() == true);
    CHECK(campo.valor() == "42");
}

TEST_CASE("el campo lanza al agotar los intentos", campo_agota_intentos) {
    ui::Campo campo("Edad", "", [](const std::string&) { return std::string("nope"); });

    bool lanzo = false;
    try {
        for (int intento = 0; intento < ui::Consola::MAX_INTENTOS; ++intento) {
            campo.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));
        }
    } catch (const DatoInvalidoException& e) {
        lanzo = true;
        CHECK(e.campo() == "entrada");
    }

    CHECK(lanzo == true);
    CHECK(campo.intentos() == ui::Consola::MAX_INTENTOS);
}

TEST_CASE("valor() antes de confirmar lanza", campo_valor_sin_confirmar) {
    ui::Campo campo("Edad", "", [](const std::string&) { return std::string(); });

    const ErrorCapturado error = capturarError([&campo]() {
        (void)campo.valor();
    });

    CHECK(error.lanzo == true);
    CHECK(error.campo == "entrada");
}

TEST_CASE("el campo no toma los clics del raton", campo_ignora_raton) {
    ui::Campo campo("Nombre", "", [](const std::string&) { return std::string(); });

    ui::Evento clic;
    clic.boton = ui::BotonMouse::Izquierdo;
    clic.x = 2;
    clic.y = 1;

    CHECK(campo.aplicarEvento(clic) == false);
    CHECK(campo.texto().empty() == true);
}

// ---------------------------------------------------------------------------
// Confirmacion
// ---------------------------------------------------------------------------

TEST_CASE("la confirmacion responde Si", confirmacion_si) {
    ui::Confirmacion confirmacion("Desea dar de baja al paciente?");

    CHECK(confirmacion.respondida() == false);
    CHECK(confirmacion.confirmo() == false);
    CHECK(confirmacion.pregunta() == "Desea dar de baja al paciente?");

    // El atajo '1' es "Si".
    CHECK(confirmacion.aplicarEvento(eventoCaracter('1')) == true);

    CHECK(confirmacion.respondida() == true);
    CHECK(confirmacion.confirmo() == true);
}

TEST_CASE("la confirmacion responde No", confirmacion_no) {
    ui::Confirmacion confirmacion("Desea dar de baja al paciente?");

    // El atajo '2' es "No".
    CHECK(confirmacion.aplicarEvento(eventoCaracter('2')) == true);

    CHECK(confirmacion.respondida() == true);
    CHECK(confirmacion.confirmo() == false);
}

TEST_CASE("el texto plano de la confirmacion tiene la pregunta y las opciones",
           confirmacion_texto_plano) {
    ui::Confirmacion confirmacion("Desea continuar?");

    const std::string texto = confirmacion.textoPlano();
    CHECK(texto.find("Desea continuar?") != std::string::npos);
    CHECK(texto.find("1. ") != std::string::npos);
    CHECK(texto.find("2. ") != std::string::npos);
}

// ---------------------------------------------------------------------------
// Mensaje
// ---------------------------------------------------------------------------

TEST_CASE("el mensaje de exito usa el formato OK", mensaje_exito) {
    const ui::Mensaje mensaje = ui::Mensaje::exito("Paciente #7 registrado.");

    CHECK(mensaje.texto() == "OK  Paciente #7 registrado.");
    CHECK(mensaje.esExito() == true);
    CHECK(mensaje.esError() == false);
    CHECK(mensaje.esVacio() == false);
}

TEST_CASE("el mensaje de error usa el formato ERR", mensaje_error) {
    const ui::Mensaje mensaje = ui::Mensaje::error("No existe la entidad #7");

    CHECK(mensaje.texto() == "ERR No existe la entidad #7");
    CHECK(mensaje.esError() == true);
    CHECK(mensaje.esExito() == false);
    CHECK(mensaje.esVacio() == false);
}

TEST_CASE("el mensaje de aviso tiene su propio formato", mensaje_aviso) {
    const ui::Mensaje mensaje = ui::Mensaje::aviso("La dni ya existe");

    CHECK(mensaje.texto() == "AVISO La dni ya existe");
    CHECK(mensaje.esExito() == false);
    CHECK(mensaje.esError() == false);
    CHECK(mensaje.esVacio() == false);
}

TEST_CASE("el mensaje vacio no es exito ni error", mensaje_vacio) {
    const ui::Mensaje mensaje = ui::Mensaje::vacio();

    CHECK(mensaje.esVacio() == true);
    CHECK(mensaje.esExito() == false);
    CHECK(mensaje.esError() == false);
    CHECK(mensaje.texto().empty() == true);
}

// ---------------------------------------------------------------------------
// Tabla
// ---------------------------------------------------------------------------

TEST_CASE("la tabla cuenta las filas de datos", tabla_cuenta_filas) {
    ui::Tabla tabla({"ID", "Nombre"});

    CHECK(tabla.filas() == 0);

    tabla.agregarFila({"1", "Ana"});
    tabla.agregarFila({"2", "Beto"});

    CHECK(tabla.filas() == 2);
}

TEST_CASE("el ancho de las columnas sale del contenido mas largo",
           tabla_ancho_de_columnas) {
    // Los colores se apagan para poder COMPARAR el texto: con `NO_COLOR` la
    // paleta no propone ningun color y el texto del screen sale limpio.
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    ui::Tabla tabla({"ID", "Nombre"});
    tabla.agregarFila({"1", "Ana"});
    tabla.agregarFila({"22", "Beto"});

    // El ancho sale del contenido mas largo de cada columna: "ID" (2) y
    // "Nombre" (6). El ancho TOTAL de la tabla sale de eso.
    const std::vector<std::string> lineas = renderizarLineas(tabla.renderizar(), 15, 6);

    CHECK(lineas.size() == 6);
    if (lineas.size() == 6) {
        CHECK(lineas[0] == "+----+--------+");
        CHECK(lineas[1] == "| ID | Nombre |");
        CHECK(lineas[2] == "+----+--------+");
        CHECK(lineas[3] == "| 1  | Ana    |");
        CHECK(lineas[4] == "| 22 | Beto   |");
        CHECK(lineas[5] == "+----+--------+");
    }

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

TEST_CASE("el render de la tabla tiene las cabeceras y los datos",
           tabla_render_contiene_los_datos) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    ui::Tabla tabla({"Especialidad", "Consultorio"});
    tabla.agregarFila({"Cardiologia", "204"});
    tabla.agregarFila({"Pediatria", "301"});

    const std::vector<std::string> lineas = renderizarLineas(tabla.renderizar(), 32, 5);

    CHECK(lineas.size() == 5);
    if (lineas.size() == 5) {
        CHECK(lineas[1].find("Especialidad") != std::string::npos);
        CHECK(lineas[1].find("Consultorio") != std::string::npos);
        CHECK(lineas[3].find("Cardiologia") != std::string::npos);
        CHECK(lineas[3].find("204") != std::string::npos);
        CHECK(lineas[4].find("Pediatria") != std::string::npos);
        CHECK(lineas[4].find("301") != std::string::npos);
    }

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

// ---------------------------------------------------------------------------
// El motor CRUD de personas
//
// QUE SE PRUEBA ACa
//
//   - la maquina de vistas del motor: que vista esta, que se ve y que
//     transicion se dispara;
//   - los siete pasos del flujo CRUD, con EVENTOS INYECTADOS (sin terminal);
//   - las reglas que son de la especificacion: el '0' es "volver", la baja
//     pide confirmacion de dos pasos, el error va debajo del campo y el
//     texto no se pierde, y el estado vacio dice como volver.
//
// LOS DATOS VAN A UN ARCHIVO REAL
//
// El repositorio de pacientes es el de verdad (escribiendo en un directorio
// temporal): asi se prueba tambien la persistencia y la unicidad de la
// dni, que son reglas del dominio y no de la interfaz.
// ---------------------------------------------------------------------------
namespace {

// Directorio temporal donde los tests escriben sus archivos.
std::filesystem::path directorioDePruebas() {
    static const std::filesystem::path directorio = [] {
        const std::filesystem::path base =
            std::filesystem::temp_directory_path() / "sistema_turnos_fase6";
        std::error_code error;
        std::filesystem::remove_all(base, error);
        std::filesystem::create_directories(base, error);
        return base;
    }();
    return directorio;
}

// La ruta de un archivo de prueba NUEVO: borra el anterior, asi cada test
// arranca con el repositorio vacio.
std::string rutaNuevaDePrueba(const std::string& nombre) {
    const std::filesystem::path ruta = directorioDePruebas() / nombre;
    std::error_code error;
    std::filesystem::remove(ruta, error);
    return ruta.string();
}

// LA PANTALLA QUE ESTA DEBAJO DEL MOTOR
//
// Es el "menu anterior": sirve para comprobar que el '0' del motor vuelve a
// la pantalla de abajo de la pila.
class PantallaDePrueba : public ui::Pantalla {
public:
    std::string titulo() const override { return "Pantalla de prueba"; }
    ftxui::Element renderizar() override { return ftxui::text("pantalla de prueba"); }
    bool manejar(const ui::Evento&) override { return false; }
    std::string textoPlano() const override { return "pantalla de prueba"; }
};

// EL ESCENARIO DE LAS PRUEBAS DEL MOTOR
//
// El motor va en la PILA de la aplicacion (como en la app real), con una
// pantalla de prueba DEBAJO.
//
// LOS TRES REPOSITORIOS SON LOS DE VERDAD
//
// El contexto expone los repositorios CONCRETOS (para que las pantallas
// tengan las reglas de busqueda del repositorio), asi que el escenario usa
// los mismos tres que usa la aplicacion. Los del motor CRUD siguen siendo
// los de verdad: asi se prueban tambien la persistencia y la unicidad de la
// dni, que son reglas del dominio y no de la interfaz.
struct Escenario {
    explicit Escenario(const std::string& nombre)
        : pacientes(rutaNuevaDePrueba(nombre)),
          medicos(rutaNuevaDePrueba(nombre + "-medicos")),
          turnos(rutaNuevaDePrueba(nombre + "-turnos")),
          servicio(pacientes, medicos, turnos),
          contexto(pacientes, medicos, turnos, servicio),
          aplicacion("Prueba", contexto) {
        pacientes.cargar();

        aplicacion.ir([](ui::Aplicacion&) { return std::make_unique<PantallaDePrueba>(); });

        auto motor = std::make_unique<ui::CrudPersonas<Paciente>>(
            aplicacion, pacientes, pacientes, ui::Pantallas::configDePacientes(pacientes, servicio));
        puntero = motor.get();
        aplicacion.ir([&motor](ui::Aplicacion&) { return std::move(motor); });
    }

    RepositorioPacientes pacientes;
    RepositorioMedicos medicos;
    RepositorioTurnos turnos;
    ServicioTurnos servicio;
    ui::Contexto contexto;
    ui::Aplicacion aplicacion;

    // El motor. OJO: si el motor sale de la pila (por ejemplo, con el '0'),
    // este puntero queda colgando y no se puede volver a usar.
    ui::CrudPersonas<Paciente>* puntero = nullptr;
};

// Escribe un texto en la pantalla (un evento por caracter).
//
// Es un TEMPLATE porque el motor es generico: sirve para el de pacientes y
// para el de medicos.
template <class T>
void escribir(ui::CrudPersonas<T>& pantalla, const std::string& texto) {
    for (const char caracter : texto) {
        pantalla.manejar(eventoCaracter(caracter));
    }
}

// Un paciente valido, para las pruebas que necesitan uno ya cargado.
Paciente pacienteDePrueba(int id, const std::string& nombre, const std::string& apellido,
                      const std::string& dni) {
    return Paciente(id, nombre, apellido, nombre + "." + apellido + "@unlp.edu.ar", "2214556677",
                  dni);
}

} // namespace

TEST_CASE("el menu del motor muestra las seis opciones y el cero de volver",
           crud_menu_inicial) {
    Escenario escenario("crud_menu_inicial");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    CHECK(motor.vista() == "Menu");
    // El titulo es el nombre de la entidad en plural.
    CHECK(motor.titulo() == "PACIENTES");

    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("1. Registrar paciente") != std::string::npos);
    CHECK(texto.find("2. Modificar paciente") != std::string::npos);
    CHECK(texto.find("3. Baja / Reactivar") != std::string::npos);
    CHECK(texto.find("4. Listar todos") != std::string::npos);
    CHECK(texto.find("5. Buscar") != std::string::npos);
    CHECK(texto.find("6. Ver ficha + historial") != std::string::npos);
    CHECK(texto.find("0. Volver") != std::string::npos);
}

TEST_CASE("el cero del menu del motor vuelve a la pantalla anterior",
           crud_menu_cero_vuelve) {
    Escenario escenario("crud_menu_cero_vuelve");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    // Todavia esta el motor arriba de la pila.
    CHECK(escenario.aplicacion.textoPlanoActual() != "pantalla de prueba");

    motor.manejar(eventoCaracter('0'));

    // El motor salio de la pila: la pantalla anterior quedo arriba.
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");
}

TEST_CASE("registrar un paciente lo deja en el repositorio", crud_registrar) {
    Escenario escenario("crud_registrar");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('1'));
    CHECK(motor.vista() == "Formulario");

    escribir(motor, "Ana");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "Gomez");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "ana.gomez@unlp.edu.ar");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "2214556677");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "2020391");

    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // Vuelve al menu y el paciente esta en el repositorio.
    CHECK(motor.vista() == "Menu");
    const std::vector<Paciente> pacientes = escenario.pacientes.listar();
    CHECK(pacientes.size() == 1);
    if (!pacientes.empty()) {
        CHECK(pacientes[0].getId() == 1);
        CHECK(pacientes[0].getNombre() == "Ana");
        CHECK(pacientes[0].getApellido() == "Gomez");
        CHECK(pacientes[0].getEmail() == "ana.gomez@unlp.edu.ar");
        CHECK(pacientes[0].getTelefono() == "2214556677");
        CHECK(pacientes[0].getDni() == "2020391");
    }

    // El mensaje de exito tiene el formato de la aplicacion.
    CHECK(escenario.contexto.mensaje().texto() == "OK  Paciente #1 registrado.");
}

TEST_CASE("un email invalido muestra el error y a la tercera vez lanza",
           crud_email_invalido) {
    Escenario escenario("crud_email_invalido");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('1'));
    escribir(motor, "Ana");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "Gomez");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "no-es-un-email");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "2214556677");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "2020391");

    // El primer intento: el error aparece DEBAJO del campo.
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Formulario");
    const std::string conError = motor.vistaTextoPlano();
    CHECK(conError.find("email") != std::string::npos);
    // El texto NO se pierde: el usuario corrige lo que escribio.
    CHECK(conError.find("no-es-un-email|") != std::string::npos);

    // El segundo intento: el campo cuenta los intentos y todavia no lanza.
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Formulario");

    // El tercero: se agotaron los intentos y se lanza la excepcion del
    // dominio (la que el `catch` global de la aplicacion atrapa).
    bool lanzo = false;
    try {
        motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    } catch (const DatoInvalidoException&) {
        lanzo = true;
    }
    CHECK(lanzo == true);

    // Y NO se registro nada.
    CHECK(escenario.pacientes.listar().empty() == true);
}

TEST_CASE("modificar cambia el dato y lo deja en el repositorio", crud_modificar) {
    Escenario escenario("crud_modificar");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Id");

    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // El formulario arranca con los datos de la entidad que se edita.
    CHECK(motor.vista() == "Formulario");
    const std::string formulario = motor.vistaTextoPlano();
    CHECK(formulario.find("nombre: Ana|") != std::string::npos);
    CHECK(formulario.find("apellido: Gomez|") != std::string::npos);
    CHECK(formulario.find("dni: 2020391|") != std::string::npos);

    // Se cambia el nombre: tres retrocesos y "Sol".
    motor.manejar(eventoTecla(ui::TipoTecla::Retroceso));
    motor.manejar(eventoTecla(ui::TipoTecla::Retroceso));
    motor.manejar(eventoTecla(ui::TipoTecla::Retroceso));
    escribir(motor, "Sol");

    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(motor.vista() == "Menu");
    Paciente paciente;
    CHECK(escenario.pacientes.buscarPorId(1, paciente) == true);
    CHECK(paciente.getNombre() == "Sol");
    // Los datos que NO se tocaron siguen como estaban.
    CHECK(paciente.getApellido() == "Gomez");
    CHECK(paciente.getDni() == "2020391");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Paciente #1 modificado.");
}

TEST_CASE("la baja pide confirmacion y sin confirmar no toca nada",
           crud_baja_sin_confirmar) {
    Escenario escenario("crud_baja_sin_confirmar");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('3'));
    CHECK(motor.vista() == "Id");
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // La vista de confirmacion pregunta QUE va a pasar.
    CHECK(motor.vista() == "Confirmar");
    CHECK(motor.vistaTextoPlano().find("Dar de baja al paciente #1?") != std::string::npos);

    // "No": vuelve a la vista anterior (la del id) y NO toca nada.
    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Id");

    Paciente paciente;
    CHECK(escenario.pacientes.buscarPorId(1, paciente) == true);
    CHECK(paciente.esActivo() == true);
}

TEST_CASE("la baja confirmada deja al paciente inactivo y la reactivation lo activa",
           crud_baja_y_reactivacion) {
    Escenario escenario("crud_baja_y_reactivacion");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;
    Paciente paciente;

    // LA BAJA
    motor.manejar(eventoCaracter('3'));
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    motor.manejar(eventoCaracter('1'));  // "Si"

    CHECK(motor.vista() == "Menu");
    CHECK(escenario.pacientes.buscarPorId(1, paciente) == true);
    CHECK(paciente.esActivo() == false);
    CHECK(escenario.contexto.mensaje().texto() == "OK  Paciente #1 dado de baja.");

    // LA REACTIVACION
    motor.manejar(eventoCaracter('3'));
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    // Ahora la accion es la contraria: la pregunta lo dice.
    CHECK(motor.vistaTextoPlano().find("Reactivar al paciente #1?") != std::string::npos);
    motor.manejar(eventoCaracter('1'));  // "Si"

    CHECK(motor.vista() == "Menu");
    CHECK(escenario.pacientes.buscarPorId(1, paciente) == true);
    CHECK(paciente.esActivo() == true);
    CHECK(escenario.contexto.mensaje().texto() == "OK  Paciente #1 reactivado.");
}

TEST_CASE("listar muestra una fila por paciente", crud_listar) {
    Escenario escenario("crud_listar");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.pacientes.alta(pacienteDePrueba(2, "Beto", "Diaz", "2020392"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('4'));
    CHECK(motor.vista() == "Listado");

    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("Ana") != std::string::npos);
    CHECK(texto.find("Gomez") != std::string::npos);
    CHECK(texto.find("Beto") != std::string::npos);
    CHECK(texto.find("activo") != std::string::npos);
}

TEST_CASE("un listado vacio dice que no hay y como volver", crud_listado_vacio) {
    Escenario escenario("crud_listado_vacio");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('4'));
    CHECK(motor.vista() == "Listado");

    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("No hay pacientes") != std::string::npos);
    CHECK(texto.find("0: volver al menu") != std::string::npos);
}

TEST_CASE("buscar por nombre devuelve las filas de las coincidencias", crud_buscar) {
    Escenario escenario("crud_buscar");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.pacientes.alta(pacienteDePrueba(2, "Beto", "Diaz", "2020392"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('5'));
    CHECK(motor.vista() == "Criterio");
    // El segundo criterio es el de nombre o apellido.
    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Valor");

    escribir(motor, "Gomez");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(motor.vista() == "Resultados");
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("Gomez") != std::string::npos);
    // La otra fila NO aparece: solo las coincidencias.
    CHECK(texto.find("Diaz") == std::string::npos);
}

TEST_CASE("una busqueda sin coincidencias dice que no hay", crud_sin_coincidencias) {
    Escenario escenario("crud_sin_coincidencias");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('5'));
    motor.manejar(eventoCaracter('2'));
    escribir(motor, "Zzz");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(motor.vista() == "Resultados");
    CHECK(motor.vistaTextoPlano().find("No hay coincidencias") != std::string::npos);
}

TEST_CASE("la ficha muestra los datos de la persona y sus turnos", crud_ficha) {
    Escenario escenario("crud_ficha");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    // Un turno vigente y uno cancelado: la ficha muestra los dos, pero en
    // tablas distintas.
    escenario.turnos.alta(
        Turno(1, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(2, 1, 1, Especialidad::PEDI, Fecha(2027, 3, 11), Hora(9, 0), 60, "A-2",
              EstadoTurno::Cancelado));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('6'));
    CHECK(motor.vista() == "Id");
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(motor.vista() == "Ficha");
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("Ana") != std::string::npos);
    CHECK(texto.find("Gomez") != std::string::npos);
    CHECK(texto.find("Turnos vigentes") != std::string::npos);
    CHECK(texto.find("Historial completo") != std::string::npos);
    CHECK(texto.find("2027-03-10") != std::string::npos);
    CHECK(texto.find("2027-03-11") != std::string::npos);
}

TEST_CASE("un id que no existe muestra el error debajo del campo", crud_id_inexistente) {
    Escenario escenario("crud_id_inexistente");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "9");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // La vista NO cambia: el usuario corrige el id.
    CHECK(motor.vista() == "Id");
    CHECK(motor.vistaTextoPlano().find("no existe paciente #9") != std::string::npos);
}

TEST_CASE("en un campo de texto el cero es un dato si ya hay texto escrito",
           crud_cero_es_dato) {
    Escenario escenario("crud_cero_es_dato");
    escenario.pacientes.alta(pacienteDePrueba(10, "Ana", "Gomez", "2020391"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "1");
    escribir(motor, "0");

    // El cero NO fue "volver": es parte del id.
    CHECK(motor.vista() == "Id");
    CHECK(motor.vistaTextoPlano().find("Id: 10|") != std::string::npos);
}

TEST_CASE("el cero de un campo vacio vuelve al menu del motor", crud_cero_vuelve) {
    Escenario escenario("crud_cero_vuelve");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Id");

    motor.manejar(eventoCaracter('0'));
    CHECK(motor.vista() == "Menu");
}

TEST_CASE("un clic en el encabezado de la confirmacion no confirma nada",
           crud_confirmacion_raton) {
    Escenario escenario("crud_confirmacion_raton");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('3'));
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Confirmar");

    // Un clic en el ENCABEZADO (la fila 0): cae fuera de las opciones y no
    // confirma nada. (La `ui::Confirmacion` no sabe en que fila se dibujan
    // sus opciones: el motor traduce el clic.)
    ui::Evento encabezado;
    encabezado.boton = ui::BotonMouse::Izquierdo;
    encabezado.x = 4;
    encabezado.y = 0;
    motor.manejar(encabezado);

    CHECK(motor.vista() == "Confirmar");
    Paciente paciente;
    CHECK(escenario.pacientes.buscarPorId(1, paciente) == true);
    CHECK(paciente.esActivo() == true);

    // Un clic en la fila del "Si" (la fila 3) SI confirma.
    ui::Evento si;
    si.boton = ui::BotonMouse::Izquierdo;
    si.x = 4;
    si.y = 3;
    motor.manejar(si);

    CHECK(motor.vista() == "Menu");
    CHECK(escenario.pacientes.buscarPorId(1, paciente) == true);
    CHECK(paciente.esActivo() == false);
}

TEST_CASE("el render del motor se puede dibujar en una pantalla de FTXUI",
           crud_render) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    Escenario escenario("crud_render");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    const std::vector<std::string> lineas = renderizarLineas(motor.renderizar(), 60, 12);
    CHECK(lineas.size() == 12);
    if (lineas.size() == 12) {
        CHECK(lineas[0].find("PACIENTES") != std::string::npos);
        CHECK(lineas[2].find("1. Registrar paciente") != std::string::npos);
        CHECK(lineas[8].find("0. Volver") != std::string::npos);
    }

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

// ---------------------------------------------------------------------------
// EL `Escape` DENTRO DEL MOTOR
//
// El motor consume el `Escape` en sus vistas internas (sube UN nivel) y no lo
// consume en su menu (ahi lo maneja la aplicacion, que saca la pantalla
// entera de la pila).
// ---------------------------------------------------------------------------
TEST_CASE("el escape del motor sube un nivel y no sale de la pantalla",
           crud_escape_sube_un_nivel) {
    Escenario escenario("crud_escape_sube_un_nivel");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    // La vista del id: el `Escape` vuelve al menu del motor.
    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Id");
    CHECK(motor.manejar(eventoTecla(ui::TipoTecla::Escape)) == true);
    CHECK(motor.vista() == "Menu");

    // El motor sigue en la pila: la pantalla de prueba NO quedo arriba.
    CHECK(escenario.aplicacion.textoPlanoActual() != "pantalla de prueba");
}

TEST_CASE("el escape del menu del motor no lo consume (lo maneja la aplicacion)",
           crud_escape_del_menu) {
    Escenario escenario("crud_escape_del_menu");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    CHECK(motor.vista() == "Menu");
    // El motor NO consume el `Escape`: lo deja pasar para que la aplicacion
    // saque la pantalla de la pila.
    CHECK(motor.manejar(eventoTecla(ui::TipoTecla::Escape)) == false);
    CHECK(motor.vista() == "Menu");
}

TEST_CASE("el escape de la confirmacion vuelve a la vista del id",
           crud_escape_de_la_confirmacion) {
    Escenario escenario("crud_escape_de_la_confirmacion");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('3'));
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Confirmar");

    CHECK(motor.manejar(eventoTecla(ui::TipoTecla::Escape)) == true);
    CHECK(motor.vista() == "Id");
}

TEST_CASE("el escape de los resultados vuelve al menu del motor",
           crud_escape_de_los_resultados) {
    Escenario escenario("crud_escape_de_los_resultados");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('5'));
    motor.manejar(eventoCaracter('2'));
    escribir(motor, "Gomez");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Resultados");

    CHECK(motor.manejar(eventoTecla(ui::TipoTecla::Escape)) == true);
    CHECK(motor.vista() == "Menu");
}

// ---------------------------------------------------------------------------
// LAS REGLAS DE BUSQUEDA SON LAS DEL REPOSITORIO
//
// La pantalla no reescribe la regla: la pide al repositorio. Estos tests
// fijan las reglas que la pantalla TIENE que respetar.
// ---------------------------------------------------------------------------
TEST_CASE("la busqueda por nombre es parcial e insensible a mayusculas",
           crud_buscar_nombre_regla) {
    Escenario escenario("crud_buscar_nombre_regla");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.pacientes.alta(pacienteDePrueba(2, "Beto", "Diaz", "2020392"));
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    // Un fragmento del apellido, en minusculas.
    motor.manejar(eventoCaracter('5'));
    motor.manejar(eventoCaracter('2'));
    escribir(motor, "go");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(motor.vista() == "Resultados");
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("Gomez") != std::string::npos);
    CHECK(texto.find("Diaz") == std::string::npos);
}

TEST_CASE("la busqueda por dni es exacta", crud_buscar_dni_regla) {
    Escenario escenario("crud_buscar_dni_regla");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    // El criterio de dni es el TERCERO.
    motor.manejar(eventoCaracter('5'));
    motor.manejar(eventoCaracter('3'));

    // El dni exacto: es la MISMA dni.
    escribir(motor, "2020391");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Resultados");
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("2020391") != std::string::npos);
    CHECK(texto.find("Ana") != std::string::npos);

    // Un fragmento NO alcanza: la coincidencia es EXACTA.
    motor.manejar(eventoCaracter('0'));
    motor.manejar(eventoCaracter('5'));
    motor.manejar(eventoCaracter('3'));
    escribir(motor, "202039");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Resultados");
    CHECK(motor.vistaTextoPlano().find("No hay coincidencias") != std::string::npos);
  }

// ---------------------------------------------------------------------------
// LA PANTALLA DE MEDICOS
//
// Es la hermana de la de pacientes: el motor es el mismo y lo unico que cambia
// son los datos. Lo que se verifica aca es que el criterio del legajo usa la
// MISMA regla del repositorio (coincidencia EXACTA e insensible a mayusculas).
// ---------------------------------------------------------------------------
namespace {

// El escenario de las pruebas de la pantalla de medicos.
struct EscenarioMedicos {
    explicit EscenarioMedicos(const std::string& nombre)
        : medicos(rutaNuevaDePrueba(nombre + "-medicos")),
          pacientes(rutaNuevaDePrueba(nombre + "-pacientes")),
          turnos(rutaNuevaDePrueba(nombre + "-turnos")),
          servicio(pacientes, medicos, turnos),
          contexto(pacientes, medicos, turnos, servicio),
          aplicacion("Prueba", contexto) {
        aplicacion.ir([](ui::Aplicacion&) { return std::make_unique<PantallaDePrueba>(); });

        auto motor = std::make_unique<ui::CrudPersonas<Medico>>(
            aplicacion, medicos, medicos,
            ui::Pantallas::configDeMedicos(medicos, servicio));
        puntero = motor.get();
        aplicacion.ir([&motor](ui::Aplicacion&) { return std::move(motor); });
    }

    RepositorioMedicos medicos;
    RepositorioPacientes pacientes;
    RepositorioTurnos turnos;
    ServicioTurnos servicio;
    ui::Contexto contexto;
    ui::Aplicacion aplicacion;
    ui::CrudPersonas<Medico>* puntero = nullptr;
};

// Un medico valido, para las pruebas que necesitan uno ya cargado.
Medico medicoDePrueba(int id, const std::string& nombre, const std::string& apellido,
                          const std::string& legajo) {
    return Medico(id, nombre, apellido, nombre + "." + apellido + "@unlp.edu.ar", "2214556677",
                    legajo, Especialidad::GAST);
}

// La linea del selector donde esta la opcion ELEGIDA (la que tiene la marca
// "(elegida)"), sin la marca.
//
// En texto plano no hay invertidas que digan cual es la elegida: por eso el
// selector la marca, y por eso el test la busca por la marca.
std::string lineaDeLaOpcionElegida(const std::string& texto) {
    const std::size_t marca = texto.find("(elegida)");
    if (marca == std::string::npos) {
        return "";
    }
    const std::size_t salto = texto.rfind('\n', marca);
    const std::size_t inicio = (salto == std::string::npos) ? 0 : salto + 1;
    return texto.substr(inicio, marca - inicio);
}

} // namespace

TEST_CASE("la busqueda por legajo es exacta e insensible a mayusculas",
           crud_buscar_legajo_regla) {
    EscenarioMedicos escenario("crud_buscar_legajo_regla");
    escenario.medicos.alta(medicoDePrueba(1, "Ana", "Gomez", "LEG-1001"));
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    // El criterio de legajo es el TERCERO.
    motor.manejar(eventoCaracter('5'));
    motor.manejar(eventoCaracter('3'));
    escribir(motor, "leg-1001");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(motor.vista() == "Resultados");
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("LEG-1001") != std::string::npos);
    CHECK(texto.find("Gomez") != std::string::npos);

    // Un fragmento NO alcanza: la coincidencia es EXACTA.
    motor.manejar(eventoCaracter('0'));
    motor.manejar(eventoCaracter('5'));
    motor.manejar(eventoCaracter('3'));
    escribir(motor, "LEG-100");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Resultados");
    CHECK(motor.vistaTextoPlano().find("No hay coincidencias") != std::string::npos);
}

TEST_CASE("el menu de medicos muestra las seis opciones y el cero de volver",
           crud_medicos_menu_inicial) {
    EscenarioMedicos escenario("crud_medicos_menu_inicial");
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    CHECK(motor.vista() == "Menu");
    CHECK(motor.titulo() == "MEDICOS");

    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("1. Registrar medico") != std::string::npos);
    CHECK(texto.find("2. Modificar medico") != std::string::npos);
    CHECK(texto.find("3. Baja / Reactivar") != std::string::npos);
    CHECK(texto.find("4. Listar todos") != std::string::npos);
    CHECK(texto.find("5. Buscar") != std::string::npos);
    // La ficha del medico muestra sus TURNOS, no su historial.
    CHECK(texto.find("6. Ver ficha + turnos") != std::string::npos);
    CHECK(texto.find("0. Volver") != std::string::npos);
}

// ---------------------------------------------------------------------------
// EL FORMULARIO DE MODIFICAR UN MEDICO
//
// QUE SE VERIFICA
//
// La especialidad NO es un campo de texto: se ELIGE de una lista de doce
// (decision 1). El caso verifica el camino COMPLETO:
//
//   1. el formulario de modificar se abre con los datos del medico y SIN un
//      campo de especialidad;
//   2. al confirmar el formulario se abre el SELECTOR, con las doce
//      opciones numeradas y con la especialidad del medico ya elegida;
//   3. al elegir otra opcion y confirmar, el medico se modifica.
//
// El paso 3 es el que estaba roto: la especialidad llegaba al formulario
// como texto (la DESCRIPCION de la fila del listado) y su propio validador
// la rechazaba, asi que el medico NO se podia modificar y, al tercer Enter,
// la excepcion mataba la aplicacion.
// ---------------------------------------------------------------------------
TEST_CASE("registrar un medico elige la especialidad de una lista",
           crud_medicos_registrar_formulario) {
    EscenarioMedicos escenario("crud_medicos_registrar_formulario");
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('1'));
    CHECK(motor.vista() == "Formulario");

    escribir(motor, "Ana");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "Gomez");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "ana.gomez@unlp.edu.ar");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "2214556677");
    motor.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(motor, "LEG-1001");

    // SE CONFIRMA EL FORMULARIO: SE ABRE EL SELECTOR.
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Selector");

    // AL REGISTRAR NO HAY ENTIDAD: EL SELECTOR ARRANCA CON LA PRIMERA.
    CHECK(lineaDeLaOpcionElegida(motor.vistaTextoPlano()).find("1. Cardiolog")
          != std::string::npos);

    // SE ELIGE LA TERCERA (Pediatria) ESCRIBIENDO SU NUMERO.
    motor.manejar(eventoCaracter('3'));
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(motor.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Medico #1 registrado.");

    Medico medico;
    CHECK(escenario.medicos.buscarPorId(1, medico) == true);
    CHECK(medico.getNombre() == "Ana");
    CHECK(medico.getApellido() == "Gomez");
    CHECK(medico.getLegajo() == "LEG-1001");
    CHECK(medico.getEspecialidad() == Especialidad::PEDI);
}

TEST_CASE("modificar un medico elige la especialidad de una lista",
           crud_medicos_modificar_formulario) {
    EscenarioMedicos escenario("crud_medicos_modificar_formulario");
    escenario.medicos.alta(medicoDePrueba(1, "Ana", "Gomez", "LEG-1001"));
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    // SE ABRE "MODIFICAR" Y SE ELIGE EL MEDICO #1.
    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Id");
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // EL FORMULARIO SE ABRE CON LOS DATOS DEL MEDICO.
    CHECK(motor.vista() == "Formulario");
    const std::string formulario = motor.vistaTextoPlano();
    CHECK(formulario.find("nombre: Ana|") != std::string::npos);
    CHECK(formulario.find("apellido: Gomez|") != std::string::npos);
    CHECK(formulario.find("email: Ana.Gomez@unlp.edu.ar|") != std::string::npos);
    CHECK(formulario.find("telefono: 2214556677|") != std::string::npos);
    CHECK(formulario.find("legajo: LEG-1001|") != std::string::npos);

    // LA ESPECIALIDAD YA NO ES UN CAMPO DE TEXTO: se elige de una lista.
    CHECK(formulario.find("especialidad") == std::string::npos);

    // SE CONFIRMA EL FORMULARIO: SE ABRE EL SELECTOR.
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Selector");

    // EL SELECTOR MUESTRA LAS DOCE OPCIONES, NUMERADAS.
    const std::string selector = motor.vistaTextoPlano();
    CHECK(selector.find("1. Cardiolog") != std::string::npos);
    CHECK(selector.find("2. Medicina General") != std::string::npos);
    CHECK(selector.find("10. Gastroenterolog") != std::string::npos);
    CHECK(selector.find("11. Endocrinol") != std::string::npos);
    CHECK(selector.find("12. Neumolog") != std::string::npos);

    // LA ESPECIALIDAD DEL MEDICO (GAST, la decima) ARRANCA ELEGIDA.
    CHECK(lineaDeLaOpcionElegida(selector).find("10. Gastroenterolog")
          != std::string::npos);

    // SE ELIGE OTRA ESPECIALIDAD CON LAS FLECHAS Y SE CONFIRMA.
    //
    // GAST es la decima: para llegar a CARD (la primera) hay que subir nueve
    // veces. La navegacion NO da la vuelta.
    for (int i = 0; i < 9; ++i) {
        motor.manejar(eventoTecla(ui::TipoTecla::FlechaArriba));
    }
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // EL MEDICO SE MODIFICO.
    CHECK(motor.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Medico #1 modificado.");

    Medico modificado;
    CHECK(escenario.medicos.buscarPorId(1, modificado) == true);
    CHECK(modificado.getNombre() == "Ana");
    CHECK(modificado.getApellido() == "Gomez");
    CHECK(modificado.getLegajo() == "LEG-1001");
    // LA ESPECIALIDAD CAMBIO: la que se eligio en el selector.
    CHECK(modificado.getEspecialidad() == Especialidad::CARD);
}

TEST_CASE("modificar un medico elige la especialidad escribiendo el numero",
           crud_medicos_modificar_por_numero) {
    EscenarioMedicos escenario("crud_medicos_modificar_por_numero");
    escenario.medicos.alta(medicoDePrueba(1, "Ana", "Gomez", "LEG-1001"));
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Formulario");

    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Selector");

    // EL NUMERO COMPLETO: "12" elige la doceava especialidad (Neumologia).
    //
    // Es lo que un `Menu` no puede hacer: sus atajos son de un digito.
    motor.manejar(eventoCaracter('1'));
    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vistaTextoPlano().find("Numero: 12") != std::string::npos);
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(motor.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Medico #1 modificado.");

    Medico modificado;
    CHECK(escenario.medicos.buscarPorId(1, modificado) == true);
    CHECK(modificado.getEspecialidad() == Especialidad::NEUM);
}

TEST_CASE("un numero de especialidad que no existe no guarda el medico",
           crud_medicos_numero_invalido) {
    EscenarioMedicos escenario("crud_medicos_numero_invalido");
    escenario.medicos.alta(medicoDePrueba(1, "Ana", "Gomez", "LEG-1001"));
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Selector");

    // "99" no es ninguna de las doce especialidades.
    motor.manejar(eventoCaracter('9'));
    motor.manejar(eventoCaracter('9'));
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // EL ERROR SE MUESTRA Y EL MEDICO NO SE MODIFICA.
    CHECK(motor.vista() == "Selector");
    CHECK(motor.vistaTextoPlano().find("opcion invalida") != std::string::npos);
    Medico medico;
    CHECK(escenario.medicos.buscarPorId(1, medico) == true);
    CHECK(medico.getNombre() == "Ana");
    CHECK(medico.getEspecialidad() == Especialidad::GAST);
}

TEST_CASE("el render del selector se puede dibujar en una pantalla de FTXUI",
           crud_medicos_render_selector) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioMedicos escenario("crud_medicos_render_selector");
    escenario.medicos.alta(medicoDePrueba(1, "Ana", "Gomez", "LEG-1001"));
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Selector");

    const std::vector<std::string> lineas = renderizarLineas(motor.renderizar(), 60, 20);
    CHECK(lineas.size() == 20);
    if (lineas.size() == 20) {
        // EL TITULO, EL NUMERO QUE SE ESTA ESCRIBIENDO Y LAS DOCE OPCIONES.
        CHECK(lineas[2].find("especialidad") != std::string::npos);
        CHECK(lineas[3].find("Numero:") != std::string::npos);
        CHECK(lineas[4].find("1. Cardiolog") != std::string::npos);
        CHECK(lineas[13].find("10. Gastroenterolog") != std::string::npos);
        CHECK(lineas[15].find("12. Neumolog") != std::string::npos);
    }

    // UN CLIC EN LA FILA DE UNA OPCION LA ELIGE
    //
    // El hit-testing del raton necesita que la pantalla se haya DIBUJADO: el
    // motor le avisa al selector en que fila arranca la lista cuando dibuja.
    ui::Evento clic;
    clic.boton = ui::BotonMouse::Izquierdo;
    clic.x = 4;
    clic.y = 4;
    motor.manejar(clic);

    CHECK(lineaDeLaOpcionElegida(motor.vistaTextoPlano()).find("1. Cardiolog")
          != std::string::npos);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

TEST_CASE("el escape del selector vuelve al formulario con lo escrito",
           crud_medicos_escape_vuelve_al_formulario) {
    EscenarioMedicos escenario("crud_medicos_escape_vuelve_al_formulario");
    escenario.medicos.alta(medicoDePrueba(1, "Ana", "Gomez", "LEG-1001"));
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Formulario");

    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Selector");

    motor.manejar(eventoTecla(ui::TipoTecla::Escape));

    // VUELVE AL FORMULARIO Y LO QUE HABIA ESCRITO SIGUE AHI.
    CHECK(motor.vista() == "Formulario");
    const std::string formulario = motor.vistaTextoPlano();
    CHECK(formulario.find("nombre: Ana|") != std::string::npos);
    CHECK(formulario.find("legajo: LEG-1001|") != std::string::npos);
}

// ---------------------------------------------------------------------------
// EL FORMULARIO DE MODIFICAR SE PRECARGA CON LOS DATOS DE LA ENTIDAD
//
// POR QUE ESTE CASO EXISTE
//
// El motor precarga el formulario de modificar con la FILA de la entidad: el
// campo se busca por su ETIQUETA, que es el nombre de la columna. Ese
// acoplamiento es correcto, pero es FRAGIL: si una fila muestra una cosa y
// el campo espera otra (la especialidad mostraba la DESCRIPCION y el
// validador esperaba el CODIGO), el formulario arranca con un valor que su
// propio validador rechaza y la pantalla se traba.
//
// Por eso se verifica CAMPO POR CAMPO, en las dos pantallas: cada campo de
// texto tiene que arrancar con el valor REAL de la entidad.
// ---------------------------------------------------------------------------
TEST_CASE("el formulario de modificar se precarga con los datos de la entidad",
           crud_formulario_modificar_precarga) {
    // LOS PACIENTES
    {
        Escenario escenario("crud_formulario_modificar_precarga");
        escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
        ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

        motor.manejar(eventoCaracter('2'));
        escribir(motor, "1");
        motor.manejar(eventoTecla(ui::TipoTecla::Enter));
        CHECK(motor.vista() == "Formulario");

        const std::string formulario = motor.vistaTextoPlano();
        CHECK(formulario.find("nombre: Ana|") != std::string::npos);
        CHECK(formulario.find("apellido: Gomez|") != std::string::npos);
        CHECK(formulario.find("email: Ana.Gomez@unlp.edu.ar|") != std::string::npos);
        CHECK(formulario.find("telefono: 2214556677|") != std::string::npos);
        CHECK(formulario.find("dni: 2020391|") != std::string::npos);
    }

    // LOS MEDICOS
    {
        EscenarioMedicos escenario("crud_formulario_modificar_precarga");
        escenario.medicos.alta(medicoDePrueba(1, "Ana", "Gomez", "LEG-1001"));
        ui::CrudPersonas<Medico>& motor = *escenario.puntero;

        motor.manejar(eventoCaracter('2'));
        escribir(motor, "1");
        motor.manejar(eventoTecla(ui::TipoTecla::Enter));
        CHECK(motor.vista() == "Formulario");

        const std::string formulario = motor.vistaTextoPlano();
        CHECK(formulario.find("nombre: Ana|") != std::string::npos);
        CHECK(formulario.find("apellido: Gomez|") != std::string::npos);
        CHECK(formulario.find("email: Ana.Gomez@unlp.edu.ar|") != std::string::npos);
        CHECK(formulario.find("telefono: 2214556677|") != std::string::npos);
        CHECK(formulario.find("legajo: LEG-1001|") != std::string::npos);
        // LA ESPECIALIDAD NO ES UN CAMPO DE TEXTO: no se precarga, se elige.
        CHECK(formulario.find("especialidad") == std::string::npos);
    }
}
