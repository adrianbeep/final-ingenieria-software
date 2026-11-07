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
#include "ui/Pantallas/MenuPrincipal.h"
#include "ui/Pantallas/Pacientes.h"
#include "ui/Pantallas/Medicos.h"
#include "ui/Pantallas/Reportes.h"
#include "ui/Pantallas/Turnos.h"
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

// EL ANCHO NATURAL DE UN ELEMENTO
//
// Es el ancho que la vista NECESITA para dibujarse entera: el que tiene que
// entrar en la terminal.
//
// COMO SE MIDE
//
// `ftxui::Render` calcula el requerimiento de cada elemento (el ancho minimo
// con el que se puede dibujar) y lo deja en `requirement()`. Ese ancho es el
// NATURAL de la vista.
//
// POR QUE NO SE MIDE "LA LINEA MAS LARGA" DEL SCREEN
//
// Dos cosas lo arruinan: el `ftxui::separator()` dibuja una linea del ANCHO
// de la pantalla, y `Screen::ToString()` devuelve la pantalla COMPLETA (con
// el relleno de espacios de la derecha). Las dos cosas hacen que la linea mas
// larga mida el ancho de la pantalla y no el de la vista.
//
// EL ANCHO DE LA PANTALLA
//
// Es de 300 columnas a proposito: es mas ancha que cualquier tabla que
// dibuje la aplicacion, asi que NADA se envuelve y el ancho medido es el
// natural y no el de un recorte.
std::size_t anchoNatural(const ftxui::Element& elemento) {
    ftxui::Screen pantalla = ftxui::Screen::Create(ftxui::Dimension::Fixed(300),
                                                   ftxui::Dimension::Fixed(60));
    ftxui::Render(pantalla, elemento);

    const ftxui::Requirement requerimiento = elemento->requirement();
    return requerimiento.min_x > 0 ? static_cast<std::size_t>(requerimiento.min_x) : 0;
}

// La linea que empieza con un texto: la PRIMERA que lo tiene.
//
// Es lo que permite comprobar el CONTENIDO de una linea concreta (la del
// aviso de la ficha, la de un turno de la ficha de un medico) sin tener que
// compararla entera.
std::string lineaQueEmpiezaCon(const std::string& texto, const std::string& prefijo) {
    std::size_t inicio = 0;
    while (inicio <= texto.size()) {
        const std::size_t fin = texto.find('\n', inicio);
        const std::string linea = texto.substr(
            inicio, fin == std::string::npos ? std::string::npos : fin - inicio);
        if (linea.compare(0, prefijo.size(), prefijo) == 0) {
            return linea;
        }
        if (fin == std::string::npos) {
            break;
        }
        inicio = fin + 1;
    }
    return "";
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
    // LA FICHA DEL PACIENTE NO MARCA NADA
    //
    // No muestra al medico, asi que no tiene con que comparar: la marca de
    // los turnos que ya no son de la especialidad del medico es solo de la
    // ficha del medico.
    CHECK(texto.find("*") == std::string::npos);
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

// ---------------------------------------------------------------------------
// LA MARCA DE LOS TURNOS EN LA FICHA DEL MEDICO
//
// LA ESPECIALIDAD DEL TURNO ES UN DATO HISTORICO
//
// La especialidad del TURNO es la que se agendo (un hecho del pasado) y la
// del MEDICO es la que atiende hoy (gobierna las reservas nuevas). Cuando no
// son la misma, el turno es de una especialidad que el medico ya no
// atiende.
//
// LA FICHA DEL MEDICO MARCA ESOS TURNOS
//
// El sujeto de la ficha es el medico: la comparacion es natural y es
// barata. La ficha del PACIENTE no muestra al medico, asi que no tiene con
// que comparar y no lleva marca.
//
// LA MARCA ES UN "*" Y ABAJO ESTA SU LEYENDA
//
// La marca va dentro de una celda de tabla, asi que tiene que ser corta. La
// leyenda la dibuja el motor de la ficha, DEBAJO de las tablas: la pone la
// pantalla (es dato, no una regla del motor).
// ---------------------------------------------------------------------------
TEST_CASE("la ficha de un medico marca los turnos que ya no son de su especialidad",
           crud_medicos_ficha_marca_turnos) {
    EscenarioMedicos escenario("crud_medicos_ficha_marca_turnos");
    // El medico ATIENNE PEDIATRIA hoy.
    escenario.medicos.alta(Medico(1, "Beto", "Perez", "beto.perez@unlp.edu.ar", "2214556677",
                                   "LEG-1001", Especialidad::PEDI));
    // El #31 se agendo de CARDIOLOGIA: NO es la que atiende hoy.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "A-1"));
    // El #32 si es de la especialidad que atiende.
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::PEDI, Fecha(2027, 3, 11), Hora(9, 30), 45, "A-2"));

    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('6'));
    CHECK(motor.vista() == "Id");
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Ficha");

    const std::string texto = motor.vistaTextoPlano();

    // EL TURNO QUE NO ES DE SU ESPECIALIDAD LLEVA LA MARCA
    const std::string fila31 = lineaQueEmpiezaCon(texto, "    31 | ");
    CHECK(fila31.find("Cardiolog") != std::string::npos);
    CHECK(fila31.find(" *") != std::string::npos);

    // Y EL QUE SI ES DE SU ESPECIALIDAD, NO
    const std::string fila32 = lineaQueEmpiezaCon(texto, "    32 | ");
    CHECK(fila32.find("Pediatr") != std::string::npos);
    CHECK(fila32.find(" *") == std::string::npos);

    // Y LA LEYENDA DICE QUE SIGNIFICA LA MARCA
    CHECK(texto.find("El * marca") != std::string::npos);
}

TEST_CASE("la ficha de un medico sin turnos divergentes no muestra la leyenda",
           crud_medicos_ficha_sin_leyenda) {
    EscenarioMedicos escenario("crud_medicos_ficha_sin_leyenda");
    escenario.medicos.alta(Medico(1, "Beto", "Perez", "beto.perez@unlp.edu.ar", "2214556677",
                                   "LEG-1001", Especialidad::PEDI));
    // El unico turno es de la especialidad que atiende: no hay nada que
    // marcar y no hay nada que explicar.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::PEDI, Fecha(2027, 3, 10), Hora(8, 0), 60, "A-1"));

    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('6'));
    escribir(motor, "1");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Ficha");

    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("El * marca") == std::string::npos);
    CHECK(lineaQueEmpiezaCon(texto, "    31 | ").find(" *") == std::string::npos);
}

// ---------------------------------------------------------------------------
// LA PANTALLA DE TURNOS
//
// QUE SE VERIFICA ACa
//
//   - el menu del modulo y su atajo de volver;
//   - las cuatro ACCIONES (agendar, cancelar, reprogramar y modificar),
//     con el camino COMPLETO de cada una (formulario, selector,
//     confirmacion);
//   - que TODA regla de negocio venga del SERVICIO: la pantalla solo
//     muestra el mensaje de la excepcion.
//
// QUE NO SE VERIFICA ACa
//
// Las reglas de negocio del servicio tienen sus propios tests, en
// `tests/test_servicio.cpp`. Aca se verifica que la pantalla las USA.
//
// LOS ERRORES SON LOS DEL SERVICIO
//
// Cada caso de error compara el mensaje COMPLETO (el `what()` de la
// excepcion del dominio), no una palabra suelta: asi el test falla si la
// pantalla inventa su propio texto.
// ---------------------------------------------------------------------------
namespace {

// El escenario de las pruebas de la pantalla de turnos.
struct EscenarioTurnos {
    explicit EscenarioTurnos(const std::string& nombre)
        : pacientes(rutaNuevaDePrueba(nombre)),
          medicos(rutaNuevaDePrueba(nombre + "-medicos")),
          turnos(rutaNuevaDePrueba(nombre + "-turnos")),
          servicio(pacientes, medicos, turnos),
          contexto(pacientes, medicos, turnos, servicio),
          aplicacion("Prueba", contexto) {
        pacientes.cargar();
        medicos.cargar();
        turnos.cargar();

        aplicacion.ir([](ui::Aplicacion&) { return std::make_unique<PantallaDePrueba>(); });

        auto pantalla = std::make_unique<ui::Pantallas::PantallaTurnos>(
            aplicacion, turnos, pacientes, medicos, servicio);
        puntero = pantalla.get();
        aplicacion.ir([&pantalla](ui::Aplicacion&) { return std::move(pantalla); });
    }

    RepositorioPacientes pacientes;
    RepositorioMedicos medicos;
    RepositorioTurnos turnos;
    ServicioTurnos servicio;
    ui::Contexto contexto;
    ui::Aplicacion aplicacion;
    ui::Pantallas::PantallaTurnos* puntero = nullptr;
};

// Escribe un texto en la pantalla de turnos (un evento por caracter).
void escribir(ui::Pantallas::PantallaTurnos& pantalla, const std::string& texto) {
    for (const char caracter : texto) {
        pantalla.manejar(eventoCaracter(caracter));
    }
}

// Un medico valido con la especialidad que se le pide.
//
// El overload de cuatro argumentos (el de mas arriba) deja al medico con
// GASTROENTEROLOGIA; este deja elegir la especialidad, que es lo que
// necesita el test de la especialidad que no atiende el medico.
Medico medicoConEspecialidad(int id, const std::string& nombre, const std::string& apellido,
                              const std::string& legajo, Especialidad especialidad) {
    return Medico(id, nombre, apellido, nombre + "." + apellido + "@unlp.edu.ar", "2214556677",
                  legajo, especialidad);
}

// Un turno de RELLENO.
//
// No usa ningun paciente ni medico real (el repositorio no lo comprueba al
// guardar): lo que importa es el id, para que el test pueda pedir un id
// concreto.
Turno turnoDeRelleno(int id) {
    return Turno(id, 99, 99, Especialidad::CARD, fechaDesplazada(30), Hora(8, 0), 45, "A-1");
}

// Llena el formulario de AGENDAR con los datos de un turno valido.
//
// Los ids del paciente y del medico son parametros: hay tests en los que
// el horario esta ocupado por OTRO turno, y para que el control de
// solapamiento se dispare hay que apuntar al mismo paciente o al mismo
// medico.
//
// Devuelve sin hacer nada: el llamador confirma el formulario.
void llenarFormularioDeAgendar(ui::Pantallas::PantallaTurnos& pantalla, const std::string& idPaciente,
                                const std::string& idMedico, const std::string& fecha,
                                const std::string& hora, const std::string& duracion = "45") {
    escribir(pantalla, idPaciente);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, idMedico);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, fecha);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, hora);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, duracion);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "A-2");
}

} // namespace

TEST_CASE("el menu de turnos muestra las cuatro acciones y el cero de volver",
           turnos_menu_inicial) {
    EscenarioTurnos escenario("turnos_menu_inicial");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    CHECK(pantalla.vista() == "Menu");
    CHECK(pantalla.titulo() == "TURNOS");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("1. Agendar turno") != std::string::npos);
    CHECK(texto.find("2. Cancelar turno") != std::string::npos);
    CHECK(texto.find("3. Reprogramar turno") != std::string::npos);
    CHECK(texto.find("4. Modificar turno") != std::string::npos);
    CHECK(texto.find("0. Volver") != std::string::npos);
}

TEST_CASE("el cero del menu de turnos vuelve a la pantalla anterior",
           turnos_menu_cero_vuelve) {
    EscenarioTurnos escenario("turnos_menu_cero_vuelve");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    CHECK(escenario.aplicacion.textoPlanoActual() != "pantalla de prueba");

    pantalla.manejar(eventoCaracter('0'));

    // La pantalla salio de la pila: la pantalla anterior quedo arriba.
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");
}

TEST_CASE("agendar un turno lo deja en el repositorio", turnos_agendar) {
    EscenarioTurnos escenario("turnos_agendar");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    // Los treinta turnos que ya estan: el nuevo va a ser el #31.
    for (int i = 1; i <= 30; ++i) {
        escenario.turnos.alta(turnoDeRelleno(i));
    }

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    CHECK(pantalla.vista() == "Formulario");

    llenarFormularioDeAgendar(pantalla, "1", "1", fechaDesplazada(30).aTexto(), "10:00");

    // EL FORMULARIO SE CONFIRMA: SE ABRE EL SELECTOR DE LA ESPECIALIDAD.
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Selector");

    // LA ESPECIALIDAD SE ELIGE DE UNA LISTA NUMERADA
    //
    // La primera opcion es Cardiologia, que es la del medico.
    CHECK(pantalla.vistaTextoPlano().find("1. Cardiolog") != std::string::npos);
    CHECK(pantalla.vistaTextoPlano().find("12. Neumolog") != std::string::npos);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Turno #31 agendado.");

    // EL TURNO ESTA EN EL REPOSITORIO, CON TODOS LOS DATOS.
    Turno agendado;
    CHECK(escenario.turnos.buscarPorId(31, agendado) == true);
    CHECK(agendado.getIdMedico() == 1);
    CHECK(agendado.getIdPaciente() == 1);
    CHECK(agendado.getEspecialidad() == Especialidad::CARD);
    CHECK(agendado.getFecha() == fechaDesplazada(30));
    CHECK(agendado.getHora() == Hora(10, 0));
    CHECK(agendado.getDuracionMin() == 45);
    CHECK(agendado.getConsultorio() == "A-2");
    CHECK(agendado.getEstado() == EstadoTurno::Activo);
}

TEST_CASE("agendar con una especialidad que el medico no atiende muestra el error del servicio",
           turnos_agendar_especialidad_incorrecta) {
    EscenarioTurnos escenario("turnos_agendar_especialidad_incorrecta");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    // El medico atiende Gastroenterologia, no Cardiologia.
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::GAST));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    llenarFormularioDeAgendar(pantalla, "1", "1", fechaDesplazada(30).aTexto(), "10:00");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Selector");
    // Se elige la PRIMERA especialidad (Cardiologia), que no es la del medico.
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // EL ERROR ES EL DEL SERVICIO, CON SU FORMATO
    CHECK(escenario.contexto.mensaje().texto()
          == "ERR Dato invalido en campo 'especialidad': 'CARD' "
             "(el medico no atiende esa especialidad)");

    // Y NO SE AGENDO NADA.
    CHECK(escenario.turnos.listar().empty() == true);
}

TEST_CASE("agendar a un horario ocupado muestra el error del servicio",
           turnos_agendar_ocupado) {
    EscenarioTurnos escenario("turnos_agendar_ocupado");
    escenario.pacientes.alta(pacienteDePrueba(8, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(3, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    // El turno #12 ya ocupa ese horario.
    for (int i = 1; i <= 11; ++i) {
        escenario.turnos.alta(turnoDeRelleno(i));
    }
    escenario.turnos.alta(
        Turno(12, 3, 8, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    // El turno nuevo es del MISMO paciente y del MISMO medico que el #12:
    // es lo que hace que el horario este ocupado.
    llenarFormularioDeAgendar(pantalla, "8", "3", fechaDesplazada(30).aTexto(), "10:00");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Selector");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(escenario.contexto.mensaje().texto()
          == "ERR El turno #12 ya ocupa ese horario (idMedico=3 idPaciente=8)");

    // NO SE AGENDO NADA NUEVO.
    CHECK(escenario.turnos.contar() == 12);
}

TEST_CASE("agendar con una fecha que ya paso muestra el error del servicio",
           turnos_agendar_fecha_pasada) {
    EscenarioTurnos escenario("turnos_agendar_fecha_pasada");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    // La fecha EXISTE (la valida el campo), pero ya paso: esa regla la
    // aplica el servicio.
    llenarFormularioDeAgendar(pantalla, "1", "1", fechaDesplazada(-5).aTexto(), "10:00");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Selector");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(escenario.contexto.mensaje().texto()
          == "ERR Dato invalido en campo 'fecha': '" + fechaDesplazada(-5).aTexto()
              + "' (la fecha del turno ya paso)");

    CHECK(escenario.turnos.listar().empty() == true);
}

TEST_CASE("agendar con una hora fuera del horario de atencion muestra el error del dominio",
           turnos_agendar_hora_fuera_del_horario) {
    EscenarioTurnos escenario("turnos_agendar_hora_fuera_del_horario");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    // Las 07:00 estan FUERA del horario de atencion (08:00 a 22:00).
    llenarFormularioDeAgendar(pantalla, "1", "1", fechaDesplazada(30).aTexto(), "07:00");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Selector");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // EL ERROR ES EL DEL DOMINIO, CON SU FORMATO
    //
    // El campo del formulario solo valida la FORMA ("HH:MM"): el RANGO lo
    // decide el turno, y el error sale como mensaje.
    CHECK(escenario.contexto.mensaje().texto()
          == "ERR Dato invalido en campo 'hora': '07:00' (fuera del horario de atencion o con "
             "minutos que no son multiplos de 30)");

    CHECK(escenario.turnos.listar().empty() == true);
}

TEST_CASE("agendar con una duracion que no es multiplo de 15 muestra el error del dominio",
           turnos_agendar_duracion_invalida) {
    EscenarioTurnos escenario("turnos_agendar_duracion_invalida");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    llenarFormularioDeAgendar(pantalla, "1", "1", fechaDesplazada(30).aTexto(), "10:00", "7");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Selector");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(escenario.contexto.mensaje().texto()
          == "ERR Dato invalido en campo 'duracionMin': '7' (se esperaba una duracion positiva, "
             "multiplo de 15 y de 240 minutos como maximo)");

    CHECK(escenario.turnos.listar().empty() == true);
}

TEST_CASE("un campo de texto con la forma equivocada muestra el error debajo del campo",
           turnos_agendar_forma_mal) {
    EscenarioTurnos escenario("turnos_agendar_forma_mal");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    llenarFormularioDeAgendar(pantalla, "1", "1", fechaDesplazada(30).aTexto(), "8:00");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // LA FORMA ESTA MAL: el error va DEBAJO del campo y la vista NO cambia.
    CHECK(pantalla.vista() == "Formulario");
    const std::string formulario = pantalla.vistaTextoPlano();
    CHECK(formulario.find("se esperaba HH:MM") != std::string::npos);
    // Y lo escrito NO se pierde: el usuario corrige, no vuelve a escribir.
    CHECK(formulario.find("hora: 8:00|") != std::string::npos);
}

TEST_CASE("en el campo de la hora el cero es un dato y no un volver", turnos_cero_en_la_hora) {
    EscenarioTurnos escenario("turnos_cero_en_la_hora");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    CHECK(pantalla.vista() == "Formulario");

    // Se llega al campo de la hora y se escribe "09:00".
    escribir(pantalla, "1");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "1");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, fechaDesplazada(30).aTexto());
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "09:00");

    // El primer cero NO fue "volver": la hora SIEMPRE empieza con un cero.
    CHECK(pantalla.vista() == "Formulario");
    CHECK(pantalla.vistaTextoPlano().find("hora: 09:00|") != std::string::npos);
}

TEST_CASE("cancelar un turno pide confirmacion y sin confirmar no toca nada",
           turnos_cancelar_sin_confirmar) {    EscenarioTurnos escenario("turnos_cancelar_sin_confirmar");
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Id");
    escribir(pantalla, "31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // LA CONFIRMACION DE DOS PASOS
    CHECK(pantalla.vista() == "Confirmar");
    CHECK(pantalla.vistaTextoPlano().find("Cancelar el turno #31?") != std::string::npos);

    // "No": vuelve a la vista del id y NO toca nada.
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Id");

    Turno turno;
    CHECK(escenario.turnos.buscarPorId(31, turno) == true);
    CHECK(turno.getEstado() == EstadoTurno::Activo);
}

TEST_CASE("cancelar un turno confirmado lo deja cancelado", turnos_cancelar) {
    EscenarioTurnos escenario("turnos_cancelar");
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Confirmar");

    // "Si": ahora si se cancela.
    pantalla.manejar(eventoCaracter('1'));

    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Turno #31 cancelado.");

    Turno turno;
    CHECK(escenario.turnos.buscarPorId(31, turno) == true);
    CHECK(turno.getEstado() == EstadoTurno::Cancelado);
    // LA BAJA ES LOGICA: el turno NO se borra.
    CHECK(escenario.turnos.contar() == 1);
}

TEST_CASE("reprogramar crea el turno nuevo y deja el viejo como Reprogramado",
           turnos_reprogramar) {
    EscenarioTurnos escenario("turnos_reprogramar");
    // El paciente y el medico tienen que existir: la reprogramacion
    // aplica las MISMAS reglas que el agendamiento.
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('3'));
    CHECK(pantalla.vista() == "Id");
    escribir(pantalla, "31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // LA FECHA Y LA HORA NUEVAS
    CHECK(pantalla.vista() == "Formulario");
    escribir(pantalla, fechaDesplazada(45).aTexto());
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "11:00");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto()
          == "OK  Turno #31 reprogramado. Nuevo turno #32.");

    // EL VIEJO QUEDA REPROGRAMADO Y APUNTA AL NUEVO.
    Turno viejo;
    CHECK(escenario.turnos.buscarPorId(31, viejo) == true);
    CHECK(viejo.getEstado() == EstadoTurno::Reprogramado);
    CHECK(viejo.getIdTurnoReemplazante() == 32);

    // Y EL NUEVO TIENE SU PROPIA FECHA Y HORA.
    Turno nuevo;
    CHECK(escenario.turnos.buscarPorId(32, nuevo) == true);
    CHECK(nuevo.getEstado() == EstadoTurno::Activo);
    CHECK(nuevo.getIdTurnoReemplazante() == -1);
    CHECK(nuevo.getFecha() == fechaDesplazada(45));
    CHECK(nuevo.getHora() == Hora(11, 0));
    CHECK(nuevo.getIdMedico() == viejo.getIdMedico());
    CHECK(nuevo.getIdPaciente() == viejo.getIdPaciente());
    CHECK(nuevo.getEspecialidad() == viejo.getEspecialidad());
    CHECK(nuevo.getDuracionMin() == viejo.getDuracionMin());
    CHECK(nuevo.getConsultorio() == viejo.getConsultorio());
}

TEST_CASE("modificar un turno cambia el consultorio, la duracion y la especialidad, y no la fecha ni la hora",
           turnos_modificar) {
    EscenarioTurnos escenario("turnos_modificar");
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('4'));
    CHECK(pantalla.vista() == "Id");
    escribir(pantalla, "31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // EL FORMULARIO ARRANCA CON LOS DATOS VIEJOS
    CHECK(pantalla.vista() == "Formulario");
    const std::string formulario = pantalla.vistaTextoPlano();
    CHECK(formulario.find("consultorio: A-1|") != std::string::npos);
    CHECK(formulario.find("duracion: 45|") != std::string::npos);
    // LA FECHA Y LA HORA NO SON CAMPOS: moverlas es REPROGRAMAR.
    CHECK(formulario.find("fecha") == std::string::npos);
    CHECK(formulario.find("hora") == std::string::npos);

    // SE CAMBIA EL CONSULTORIO: tres retrocesos y "B-2".
    for (int i = 0; i < 3; ++i) {
        pantalla.manejar(eventoTecla(ui::TipoTecla::Retroceso));
    }
    escribir(pantalla, "B-2");

    // SE CAMBIA LA DURACION: dos retrocesos y "60".
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    for (int i = 0; i < 2; ++i) {
        pantalla.manejar(eventoTecla(ui::TipoTecla::Retroceso));
    }
    escribir(pantalla, "60");

    // SE CONFIRMA EL FORMULARIO: SE ABRE EL SELECTOR DE LA ESPECIALIDAD.
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Selector");

    // LA ESPECIALIDAD DEL TURNO (Cardiologia) ARRANCA ELEGIDA.
    CHECK(lineaDeLaOpcionElegida(pantalla.vistaTextoPlano()).find("1. Cardiolog")
          != std::string::npos);

    // SE ELIGE LA SEGUNDA (Medicina General).
    pantalla.manejar(eventoCaracter('2'));
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Turno #31 modificado.");

    Turno modificado;
    CHECK(escenario.turnos.buscarPorId(31, modificado) == true);
    CHECK(modificado.getConsultorio() == "B-2");
    CHECK(modificado.getDuracionMin() == 60);
    CHECK(modificado.getEspecialidad() == Especialidad::MEDG);
    // LA FECHA Y LA HORA NO CAMBIAN: eso es reprogramar.
    CHECK(modificado.getFecha() == fechaDesplazada(30));
    CHECK(modificado.getHora() == Hora(10, 0));
    CHECK(modificado.getIdMedico() == 1);
    CHECK(modificado.getIdPaciente() == 1);
}

TEST_CASE("un turno que no existe se rechaza con el error del dominio",
           turnos_id_inexistente) {
    EscenarioTurnos escenario("turnos_id_inexistente");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "99");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto() == "ERR No existe la entidad #99 (Turno)");
}

TEST_CASE("en un campo de texto de turnos el cero es un dato si ya hay texto",
           turnos_cero_es_dato) {
    EscenarioTurnos escenario("turnos_cero_es_dato");
    escenario.turnos.alta(
        Turno(310, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "31");
    escribir(pantalla, "0");

    // El cero NO fue "volver": es parte del id.
    CHECK(pantalla.vista() == "Id");
    CHECK(pantalla.vistaTextoPlano().find("Id: 310|") != std::string::npos);
}

TEST_CASE("el cero de un campo de texto vacio de turnos vuelve al menu",
           turnos_cero_vuelve) {
    EscenarioTurnos escenario("turnos_cero_vuelve");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Id");

    pantalla.manejar(eventoCaracter('0'));
    CHECK(pantalla.vista() == "Menu");
}

TEST_CASE("el escape de los turnos sube un nivel y no sale de la pantalla",
           turnos_escape_sube_un_nivel) {
    EscenarioTurnos escenario("turnos_escape_sube_un_nivel");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Id");
    CHECK(pantalla.manejar(eventoTecla(ui::TipoTecla::Escape)) == true);
    CHECK(pantalla.vista() == "Menu");

    // La pantalla SIGUE en la pila: la pantalla de prueba no quedo arriba.
    CHECK(escenario.aplicacion.textoPlanoActual() != "pantalla de prueba");

    // Y en el menu, el `Escape` NO lo consume: lo maneja la aplicacion.
    CHECK(pantalla.manejar(eventoTecla(ui::TipoTecla::Escape)) == false);
}

TEST_CASE("el render de la pantalla de turnos se puede dibujar en FTXUI",
           turnos_render) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioTurnos escenario("turnos_render");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    const std::vector<std::string> lineas = renderizarLineas(pantalla.renderizar(), 60, 14);
    CHECK(lineas.size() == 14);
    if (lineas.size() == 14) {
        CHECK(lineas[0].find("TURNOS") != std::string::npos);
        CHECK(lineas[2].find("1. Agendar turno") != std::string::npos);
        CHECK(lineas[6].find("5. Listar todos") != std::string::npos);
        CHECK(lineas[8].find("7. Ver detalle") != std::string::npos);
        // El "0. Volver" es la OCTAVA opcion del menu (la fila 9).
        CHECK(lineas[9].find("0. Volver") != std::string::npos);
    }

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

// ---------------------------------------------------------------------------
// LAS TRES CONSULTAS DE TURNOS
//
// Son las que el issue #16 pide en su seccion 5.4: listar todos, buscar y
// ver el detalle.
//
// LO QUE SE VERIFICA
//
//   - las DIEZ columnas del listado, con una fila por turno;
//   - la busqueda de DOS PASOS (criterio y despues valor);
//   - la ficha completa de un turno, con el `idTurnoReemplazante` y los
//     turnos que se reemplazaron (en las DOS direcciones).
// ---------------------------------------------------------------------------

TEST_CASE("listar todos muestra una fila por turno con las seis columnas",
           turnos_listar) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioTurnos escenario("turnos_listar");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, fechaDesplazada(31), Hora(11, 0), 30, "A-2",
              EstadoTurno::Cancelado));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('5'));
    CHECK(pantalla.vista() == "Listado");

    const std::vector<std::string> lineas = renderizarLineas(pantalla.renderizar(), 220, 12);
    CHECK(lineas.size() == 12);
    if (lineas.size() == 12) {
        // LA FILA 3 ES LA CABECERA: TIENE LAS SEIS COLUMNAS DE LA LISTA.
        //
        // La lista se enfoca en lo que hace falta para RECONOCER un turno y
        // para que la tabla entre en 80 columnas (ver el caso del ancho).
        const std::string cabecera = lineas[3];
        CHECK(cabecera.find("id") != std::string::npos);
        CHECK(cabecera.find("fecha") != std::string::npos);
        CHECK(cabecera.find("hora") != std::string::npos);
        CHECK(cabecera.find("paciente") != std::string::npos);
        CHECK(cabecera.find("medico") != std::string::npos);
        CHECK(cabecera.find("estado") != std::string::npos);

        // Y LAS CUATRO QUE QUEDARON AFUERA NO ESTAN.
        //
        // `dia` se deduce de la fecha, `especialidad` es la del medico (la
        // regla 2b), y `duracion` y `consultorio` estan en la ficha.
        CHECK(cabecera.find("especialidad") == std::string::npos);
        CHECK(cabecera.find("dia") == std::string::npos);
        CHECK(cabecera.find("duracion") == std::string::npos);
        CHECK(cabecera.find("consultorio") == std::string::npos);

        // UNA FILA POR TURNO, con los NOMBRES del paciente y del medico.
        CHECK(lineas[5].find("Ana Gomez") != std::string::npos);
        CHECK(lineas[5].find("Beto Perez") != std::string::npos);
        CHECK(lineas[5].find("Activo") != std::string::npos);
        CHECK(lineas[6].find("Cancelado") != std::string::npos);
    }

    // Y EN TEXTO PLANO SE VEN LOS MISMOS CAMPOS.
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Ana Gomez") != std::string::npos);
    CHECK(texto.find("Beto Perez") != std::string::npos);
    CHECK(texto.find("Activo") != std::string::npos);
    CHECK(texto.find("Cancelado") != std::string::npos);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

// ---------------------------------------------------------------------------
// LA RED DE LAS 80 COLUMNAS
//
// LA LISTA TIENE QUE ENTRAR EN UNA TERMINAL DE 80 COLUMNAS
//
// `ui::Tabla` no scrollea ni trunca: si la tabla es mas ancha que la
// terminal, la parte de la derecha (la del `estado`) no se ve. Con las diez
// columnas de la especificacion, la tabla de la semilla mide 137 columnas.
//
// Este caso mide el ANCHO NATURAL de la vista "Listar todos" y falla si pasa
// de 80. Es la red que hace que esto no se vuelva a romper en silencio.
//
// LOS NOMBRES SON LOS MAS LARGOS DE LA SEMILLA
//
// La semilla tiene "Ramirez Joaquin" y "Fernandez Diego" (quince letras cada
// uno). El caso usa nombres de esa longitud para que el ancho que se mide
// sea el de verdad, y no el de un camino feliz con nombres cortitos.
// ---------------------------------------------------------------------------
TEST_CASE("la lista de turnos entra en 80 columnas", turnos_lista_entra_en_80) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioTurnos escenario("turnos_lista_entra_en_80");

    // LOS NOMBRES MAS LARGOS DE LA SEMILLA
    escenario.pacientes.alta(pacienteDePrueba(1, "Valentina", "Ibarra", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Diego", "Fernandez", "LEG-1001", Especialidad::CARD));

    // Y LOS TURNOS QUE PONEN CADA COLUMNA EN SU ANCHO MAXIMO: la fecha mas
    // larga, la hora mas larga y el estado mas largo ("Reprogramado").
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "Consultorio 1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 11), Hora(9, 30), 45, "Consultorio 2",
              EstadoTurno::Reprogramado));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('5'));
    CHECK(pantalla.vista() == "Listado");

    // EL ANCHO NATURAL DE LA VISTA
    const std::size_t ancho = anchoNatural(pantalla.renderizar());

    // LA RED
    CHECK(ancho <= 80);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

TEST_CASE("un listado de turnos vacio dice que no hay y como volver",
           turnos_listar_vacio) {
    EscenarioTurnos escenario("turnos_listar_vacio");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('5'));
    CHECK(pantalla.vista() == "Listado");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("No hay turnos") != std::string::npos);
    CHECK(texto.find("0: volver al menu") != std::string::npos);
}

TEST_CASE("buscar es de dos pasos: primero el criterio y despues el valor",
           turnos_buscar) {
    EscenarioTurnos escenario("turnos_buscar");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // EL PRIMER PASO ES EL CRITERIO
    pantalla.manejar(eventoCaracter('6'));
    CHECK(pantalla.vista() == "Criterio");
    const std::string criterios = pantalla.vistaTextoPlano();
    CHECK(criterios.find("1. id") != std::string::npos);
    CHECK(criterios.find("2. paciente") != std::string::npos);
    CHECK(criterios.find("3. medico") != std::string::npos);
    CHECK(criterios.find("4. especialidad") != std::string::npos);
    CHECK(criterios.find("5. consultorio") != std::string::npos);
    CHECK(criterios.find("6. estado") != std::string::npos);
    CHECK(criterios.find("0. Volver") != std::string::npos);

    // EL SEGUNDO PASO ES EL VALOR
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Valor");
    escribir(pantalla, "Gomez");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Resultados");
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Ana Gomez") != std::string::npos);
    // El turno se reconoce por su id: el consultorio ya no es una columna de
    // la lista (esta en la ficha).
    CHECK(texto.find("31") != std::string::npos);
}

TEST_CASE("buscar por estado y por consultorio devuelve las coincidencias",
           turnos_buscar_criterios) {
    EscenarioTurnos escenario("turnos_buscar_criterios");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, fechaDesplazada(31), Hora(11, 0), 30, "A-2",
              EstadoTurno::Cancelado));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // POR ESTADO
    pantalla.manejar(eventoCaracter('6'));
    pantalla.manejar(eventoCaracter('6'));
    CHECK(pantalla.vista() == "Valor");
    escribir(pantalla, "Cancelado");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Resultados");
    const std::string porEstado = pantalla.vistaTextoPlano();
    // Los turnos se distinguen por su id: el consultorio no es una columna de
    // la lista.
    CHECK(porEstado.find("32") != std::string::npos);
    // El otro turno NO aparece: solo las coincidencias.
    CHECK(porEstado.find("31") == std::string::npos);

    // POR CONSULTORIO
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('6'));
    pantalla.manejar(eventoCaracter('5'));
    escribir(pantalla, "A-1");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Resultados");
    const std::string porConsultorio = pantalla.vistaTextoPlano();
    CHECK(porConsultorio.find("31") != std::string::npos);
    CHECK(porConsultorio.find("32") == std::string::npos);
}

TEST_CASE("buscar con un codigo de especialidad que no existe muestra el error del dominio",
           turnos_buscar_especialidad_invalida) {
    EscenarioTurnos escenario("turnos_buscar_especialidad_invalida");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('6'));
    pantalla.manejar(eventoCaracter('4'));
    CHECK(pantalla.vista() == "Valor");
    escribir(pantalla, "XXX");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // LA VISTA NO CAMBIA: el usuario corrige el valor.
    CHECK(pantalla.vista() == "Valor");
    CHECK(pantalla.vistaTextoPlano().find("codigo de especialidad desconocido")
          != std::string::npos);
}

TEST_CASE("una busqueda sin coincidencias dice que no hay", turnos_buscar_sin_coincidencias) {
    EscenarioTurnos escenario("turnos_buscar_sin_coincidencias");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('6'));
    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "Zzz");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Resultados");
    CHECK(pantalla.vistaTextoPlano().find("No hay coincidencias") != std::string::npos);
}

TEST_CASE("ver detalle muestra todos los campos del turno", turnos_detalle) {
    EscenarioTurnos escenario("turnos_detalle");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('7'));
    CHECK(pantalla.vista() == "Id");
    escribir(pantalla, "31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Detalle");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Ficha de turno #31") != std::string::npos);
    CHECK(texto.find("Ana Gomez") != std::string::npos);
    CHECK(texto.find("Beto Perez") != std::string::npos);
    CHECK(texto.find("Cardiolog") != std::string::npos);
    CHECK(texto.find(fechaDesplazada(30).aTexto()) != std::string::npos);
    // El dia de la semana esta en la FICHA (la lista se enfoca en seis
    // columnas y el dia se deduce de la fecha).
    CHECK(texto.find(Fecha::nombreDiaSemana(fechaDesplazada(30).diaSemana()))
          != std::string::npos);
    CHECK(texto.find("10:00") != std::string::npos);
    CHECK(texto.find("45") != std::string::npos);
    CHECK(texto.find("A-1") != std::string::npos);
    CHECK(texto.find("Activo") != std::string::npos);
}

TEST_CASE("la ficha de un turno reprogramado muestra el reemplazo en las dos direcciones",
           turnos_detalle_reemplazo) {
    EscenarioTurnos escenario("turnos_detalle_reemplazo");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    // Se reprograma: el #31 queda `Reprogramado` y apunta al #32.
    escenario.servicio.reprogramar(31, fechaDesplazada(45), Hora(11, 0));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // LA FICHA DEL VIEJO: el `idTurnoReemplazante` y el turno que lo
    // reemplazo.
    pantalla.manejar(eventoCaracter('7'));
    escribir(pantalla, "31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Detalle");

    const std::string delViejo = pantalla.vistaTextoPlano();
    CHECK(delViejo.find("idTurnoReemplazante: 32") != std::string::npos);
    CHECK(delViejo.find("Reemplazante: #32") != std::string::npos);
    CHECK(delViejo.find("Reprogramado") != std::string::npos);
    // Y se ve la fecha nueva del reemplazo.
    CHECK(delViejo.find(fechaDesplazada(45).aTexto()) != std::string::npos);

    // LA FICHA DEL NUEVO: el turno al que reemplazo.
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('7'));
    escribir(pantalla, "32");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Detalle");

    const std::string delNuevo = pantalla.vistaTextoPlano();
    CHECK(delNuevo.find("Reemplazado: #31") != std::string::npos);
    CHECK(delNuevo.find(fechaDesplazada(30).aTexto()) != std::string::npos);
}

// ---------------------------------------------------------------------------
// EL AVISO DE LA ESPECIALIDAD
//
// LA ESPECIALIDAD DEL TURNO ES UN DATO HISTORICO
//
// La especialidad del TURNO es la que se AGENDO (un hecho del pasado) y la
// del MEDICO es la que atiende HOY (gobierna las reservas nuevas). Cuando no
// son la misma, el turno es de una especialidad que el medico ya no atiende.
//
// No es un error: es un hecho. Pero si la pantalla no lo dice, el usuario lo
// lee como un bug. Por eso la ficha del turno avisa.
//
// Y al reves: si las dos especialidades son la misma, NO hay aviso. Un
// aviso que aparece siempre es ruido.
// ---------------------------------------------------------------------------
TEST_CASE("el detalle de un turno avisa cuando su especialidad no es la que atiende hoy el medico",
           turnos_detalle_especialidad_distinta) {
    EscenarioTurnos escenario("turnos_detalle_especialidad_distinta");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    // El medico ATIENNE PEDIATRIA hoy...
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::PEDI));
    // ...pero el turno se agendo de CARDIOLOGIA.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('7'));
    escribir(pantalla, "31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Detalle");

    // EN TEXTO PLANO
    const std::string aviso = lineaQueEmpiezaCon(pantalla.vistaTextoPlano(), "  Aviso: ");
    CHECK(!aviso.empty());
    // El aviso dice las DOS especialidades y que difieren.
    CHECK(aviso.find("Cardiolog") != std::string::npos);
    CHECK(aviso.find("Pediatr") != std::string::npos);
    CHECK(aviso.find("difieren") != std::string::npos);

    // Y EN EL RENDER DE FTXUI
    const std::vector<std::string> lineas = renderizarLineas(pantalla.renderizar(), 300, 20);
    bool hayAviso = false;
    for (const std::string& linea : lineas) {
        if (linea.find("difieren") != std::string::npos) {
            hayAviso = true;
        }
    }
    CHECK(hayAviso);
}

TEST_CASE("el detalle de un turno no avisa cuando su especialidad es la que atiende hoy el medico",
           turnos_detalle_especialidad_igual) {
    EscenarioTurnos escenario("turnos_detalle_especialidad_igual");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    // El medico y el turno son de la MISMA especialidad: no hay nada que avisar.
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('7'));
    escribir(pantalla, "31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Detalle");

    // SIN AVISO EN TEXTO PLANO
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Aviso") == std::string::npos);
    CHECK(texto.find("difieren") == std::string::npos);

    // Y SIN AVISO EN EL RENDER
    const std::vector<std::string> lineas = renderizarLineas(pantalla.renderizar(), 300, 20);
    for (const std::string& linea : lineas) {
        CHECK(linea.find("difieren") == std::string::npos);
    }
}

TEST_CASE("el menu principal ofrece la opcion de turnos", menu_principal_turnos) {
    EscenarioTurnos escenario("menu_principal_turnos");
    ui::Pantallas::MenuPrincipal menu(escenario.aplicacion);

    const std::string texto = menu.textoPlano();
    CHECK(texto.find("1. Pacientes") != std::string::npos);
    CHECK(texto.find("2. Medicos") != std::string::npos);
    CHECK(texto.find("3. Turnos") != std::string::npos);
    CHECK(texto.find("0. Salir") != std::string::npos);

    // Y EL ATAJO ABRE LA PANTALLA DE TURNOS.
    menu.manejar(eventoCaracter('3'));
    CHECK(escenario.aplicacion.textoPlanoActual().find("TURNOS") != std::string::npos);
}

// ---------------------------------------------------------------------------
// LA PANTALLA DE REPORTES
//
// QUE SE VERIFICA ACa
//
//   - el menu del modulo y su atajo de volver;
//   - los cuatro reportes de turnos (por fecha, por paciente, por medico y
//     por especialidad), con su tabla, su total y su mensaje;
//   - que los reportes muestren los turnos de CUALQUIER estado;
//   - que las COLUMNAS de cada reporte sean las de la lista de turnos MENOS
//     la dimension por la que se filtra;
//   - el estado vacio con su salida;
//   - un solo formato de error y el '0' que es dato en un campo con texto;
//   - la RED DE LAS 80 COLUMNAS de las cuatro tablas.
//
// LOS ERRORES SON LOS DEL DOMINIO
//
// Cada caso de error compara el mensaje COMPLETO (el `what()` de la
// excepcion del dominio), no una palabra suelta: asi el test falla si la
// pantalla inventa su propio texto.
// ---------------------------------------------------------------------------
namespace {

// El escenario de las pruebas de la pantalla de reportes.
//
// Es el mismo esquema que el de turnos: los repositorios de verdad (en un
// directorio temporal), el servicio de verdad y la pantalla en la pila de la
// aplicacion, con una pantalla de prueba DEBAJO.
struct EscenarioReportes {
    explicit EscenarioReportes(const std::string& nombre)
        : pacientes(rutaNuevaDePrueba(nombre)),
          medicos(rutaNuevaDePrueba(nombre + "-medicos")),
          turnos(rutaNuevaDePrueba(nombre + "-turnos")),
          servicio(pacientes, medicos, turnos),
          contexto(pacientes, medicos, turnos, servicio),
          aplicacion("Prueba", contexto) {
        pacientes.cargar();
        medicos.cargar();
        turnos.cargar();

        aplicacion.ir([](ui::Aplicacion&) { return std::make_unique<PantallaDePrueba>(); });

        auto pantalla = std::make_unique<ui::Pantallas::PantallaReportes>(
            aplicacion, pacientes, medicos, servicio);
        puntero = pantalla.get();
        aplicacion.ir([&pantalla](ui::Aplicacion&) { return std::move(pantalla); });
    }

    RepositorioPacientes pacientes;
    RepositorioMedicos medicos;
    RepositorioTurnos turnos;
    ServicioTurnos servicio;
    ui::Contexto contexto;
    ui::Aplicacion aplicacion;
    ui::Pantallas::PantallaReportes* puntero = nullptr;
};

// Escribe un texto en la pantalla de reportes (un evento por caracter).
void escribir(ui::Pantallas::PantallaReportes& pantalla, const std::string& texto) {
    for (const char caracter : texto) {
        pantalla.manejar(eventoCaracter(caracter));
    }
}

} // namespace

TEST_CASE("el menu de reportes muestra las cuatro acciones y el cero de volver",
           reportes_menu_inicial) {
    EscenarioReportes escenario("reportes_menu_inicial");
    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    CHECK(pantalla.vista() == "Menu");
    CHECK(pantalla.titulo() == "REPORTES");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("1. Turnos por fecha") != std::string::npos);
    CHECK(texto.find("2. Turnos por paciente") != std::string::npos);
    CHECK(texto.find("3. Turnos por medico") != std::string::npos);
    CHECK(texto.find("4. Turnos por especialidad") != std::string::npos);
    CHECK(texto.find("0. Volver") != std::string::npos);
}

TEST_CASE("el cero del menu de reportes vuelve a la pantalla anterior",
           reportes_menu_cero_vuelve) {
    EscenarioReportes escenario("reportes_menu_cero_vuelve");
    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    CHECK(escenario.aplicacion.textoPlanoActual() != "pantalla de prueba");

    pantalla.manejar(eventoCaracter('0'));

    // La pantalla salio de la pila: la pantalla anterior quedo arriba.
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");
}

TEST_CASE("el reporte de turnos por fecha muestra la tabla y el total",
           reportes_turnos_por_fecha) {
    EscenarioReportes escenario("reportes_turnos_por_fecha");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    // Los dos turnos del dia: uno activo y otro cancelado.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(9, 0), 60, "A-2",
              EstadoTurno::Cancelado));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    CHECK(pantalla.vista() == "Campo");
    escribir(pantalla, "2027-03-10");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Resultado");
    // EL MENSAJE DEL TOTAL
    CHECK(escenario.contexto.mensaje().texto() == "OK  2 turno(s).");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("31") != std::string::npos);
    CHECK(texto.find("08:00") != std::string::npos);
    CHECK(texto.find("Ana Gomez") != std::string::npos);
    CHECK(texto.find("Beto Perez") != std::string::npos);
    // EL REPORTE MUESTRA LOS TURNOS DE CUALQUIER ESTADO
    CHECK(texto.find("Activo") != std::string::npos);
    CHECK(texto.find("Cancelado") != std::string::npos);
    // LA FECHA NO ES UNA COLUMNA: es la dimension por la que se filtra.
    CHECK(texto.find("2027-03-10") == std::string::npos);
}

TEST_CASE("el reporte de turnos por paciente no repite el nombre del paciente",
           reportes_turnos_por_paciente) {
    EscenarioReportes escenario("reportes_turnos_por_paciente");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 11), Hora(9, 0), 60, "A-2"));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Campo");
    escribir(pantalla, "1");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Resultado");
    CHECK(escenario.contexto.mensaje().texto() == "OK  2 turno(s).");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("2027-03-10") != std::string::npos);
    CHECK(texto.find("08:00") != std::string::npos);
    CHECK(texto.find("Beto Perez") != std::string::npos);
    // EL PACIENTE NO ES UNA COLUMNA: es la dimension por la que se filtra.
    CHECK(texto.find("Ana Gomez") == std::string::npos);
}

TEST_CASE("el reporte de turnos por medico no repite el nombre del medico",
           reportes_turnos_por_medico) {
    EscenarioReportes escenario("reportes_turnos_por_medico");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 11), Hora(9, 0), 60, "A-2"));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('3'));
    CHECK(pantalla.vista() == "Campo");
    escribir(pantalla, "1");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Resultado");
    CHECK(escenario.contexto.mensaje().texto() == "OK  2 turno(s).");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("2027-03-10") != std::string::npos);
    CHECK(texto.find("Ana Gomez") != std::string::npos);
    // EL MEDICO NO ES UNA COLUMNA: es la dimension por la que se filtra.
    CHECK(texto.find("Beto Perez") == std::string::npos);
}

TEST_CASE("el reporte de turnos por especialidad elige la especialidad de una lista",
           reportes_turnos_por_especialidad) {
    EscenarioReportes escenario("reportes_turnos_por_especialidad");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.medicos.alta(
        medicoConEspecialidad(2, "Caro", "Ruiz", "LEG-1002", Especialidad::PEDI));
    // Un turno de cada especialidad: el reporte tiene que separar los dos.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(32, 2, 1, Especialidad::PEDI, Fecha(2027, 3, 10), Hora(9, 0), 60, "A-2"));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('4'));
    CHECK(pantalla.vista() == "Selector");

    // EL SELECTOR MUESTRA LAS DOCE OPCIONES, NUMERADAS.
    const std::string selector = pantalla.vistaTextoPlano();
    CHECK(selector.find("1. Cardiolog") != std::string::npos);
    CHECK(selector.find("12. Neumolog") != std::string::npos);

    // SE ELIGE LA PRIMERA (Cardiologia) Y SE CONFIRMA.
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Resultado");
    CHECK(escenario.contexto.mensaje().texto() == "OK  1 turno(s).");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("31") != std::string::npos);
    CHECK(texto.find("Beto Perez") != std::string::npos);
    // EL TURNO DE PEDIATRIA NO ESTA: el reporte es de Cardiologia.
    CHECK(texto.find("32") == std::string::npos);
    CHECK(texto.find("Caro Ruiz") == std::string::npos);
    // LA ESPECIALIDAD NO ES UNA COLUMNA (es la del medico, por la regla 2b):
    // el reporte tiene las seis columnas de la lista.
    CHECK(texto.find("Cardiolog") == std::string::npos);
}

TEST_CASE("un reporte sin turnos dice que no hay y como volver", reportes_sin_turnos) {
    EscenarioReportes escenario("reportes_sin_turnos");
    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    escribir(pantalla, "2027-03-10");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Resultado");
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("No hay turnos para esa fecha") != std::string::npos);
    CHECK(texto.find("0: volver al menu") != std::string::npos);
}

TEST_CASE("una fecha que no existe en el calendario muestra el error del dominio",
           reportes_fecha_inexistente) {
    EscenarioReportes escenario("reportes_fecha_inexistente");
    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    // El 31 de febrero no existe: el campo solo valida la FORMA.
    escribir(pantalla, "2027-02-31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // LA VISTA NO CAMBIA: el usuario corrige la fecha.
    CHECK(pantalla.vista() == "Campo");
    CHECK(escenario.contexto.mensaje().texto()
          == "ERR Dato invalido en campo 'fecha': '2027-02-31' (se esperaba AAAA-MM-DD con una "
             "fecha existente)");
}

TEST_CASE("un id que no existe muestra el error debajo del campo",
           reportes_id_inexistente) {
    EscenarioReportes escenario("reportes_id_inexistente");
    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "9");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Campo");
    CHECK(pantalla.vistaTextoPlano().find("no existe paciente #9") != std::string::npos);
}

TEST_CASE("en un campo de un reporte el cero es un dato si ya hay texto",
           reportes_cero_es_dato) {
    EscenarioReportes escenario("reportes_cero_es_dato");
    escenario.pacientes.alta(pacienteDePrueba(10, "Ana", "Gomez", "2020391"));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "1");
    escribir(pantalla, "0");

    // El cero NO fue "volver": es parte del id.
    CHECK(pantalla.vista() == "Campo");
    CHECK(pantalla.vistaTextoPlano().find("Id del paciente: 10|") != std::string::npos);
}

TEST_CASE("el cero de un campo vacio de reportes vuelve al menu", reportes_cero_vuelve) {
    EscenarioReportes escenario("reportes_cero_vuelve");
    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    CHECK(pantalla.vista() == "Campo");

    pantalla.manejar(eventoCaracter('0'));
    CHECK(pantalla.vista() == "Menu");
}

TEST_CASE("el escape de los reportes sube un nivel y no sale de la pantalla",
           reportes_escape_sube_un_nivel) {
    EscenarioReportes escenario("reportes_escape_sube_un_nivel");
    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    // La vista del campo: el `Escape` vuelve al menu del modulo.
    pantalla.manejar(eventoCaracter('1'));
    CHECK(pantalla.vista() == "Campo");
    CHECK(pantalla.manejar(eventoTecla(ui::TipoTecla::Escape)) == true);
    CHECK(pantalla.vista() == "Menu");

    // La pantalla SIGUE en la pila: la pantalla de prueba no quedo arriba.
    CHECK(escenario.aplicacion.textoPlanoActual() != "pantalla de prueba");

    // Y en el menu, el `Escape` NO lo consume: lo maneja la aplicacion.
    CHECK(pantalla.manejar(eventoTecla(ui::TipoTecla::Escape)) == false);
}

// ---------------------------------------------------------------------------
// LA RED DE LAS 80 COLUMNAS
//
// LAS TABLAS DE LOS REPORTES TIENEN QUE ENTRAR EN UNA TERMINAL DE 80 COLUMNAS
//
// `ui::Tabla` no scrollea ni trunca: si la tabla es mas ancha que la
// terminal, la parte de la derecha no se ve. Los reportes usan las columnas
// de la lista de turnos MENOS la dimension por la que se filtran, asi que
// entran: pero eso hay que VERIFICARLO, no suponerlo.
//
// Este caso mide el ANCHO NATURAL de la vista de cada uno de los cuatro
// reportes y falla si alguno pasa de 80.
//
// LOS NOMBRES SON LOS MAS LARGOS DE LA SEMILLA
//
// La semilla tiene "Ramirez Joaquin" y "Fernandez Diego" (quince letras cada
// uno). El caso usa nombres de esa longitud para que el ancho que se mide
// sea el de verdad, y no el de un camino feliz con nombres cortitos.
// ---------------------------------------------------------------------------
TEST_CASE("las tablas de los reportes de turnos entran en 80 columnas",
           reportes_las_tablas_entran_en_80) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioReportes escenario("reportes_las_tablas_entran_en_80");

    // LOS NOMBRES MAS LARGOS DE LA SEMILLA
    escenario.pacientes.alta(pacienteDePrueba(1, "Valentina", "Ibarra", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Diego", "Fernandez", "LEG-1001", Especialidad::CARD));

    // Y LOS TURNOS QUE PONEN CADA COLUMNA EN SU ANCHO MAXIMO: la fecha mas
    // larga, la hora mas larga y el estado mas largo ("Reprogramado").
    //
    // LOS DOS TURNOS SON DEL MISMO DIA
    //
    // Asi el reporte por fecha tambien ve el estado mas largo: si uno de los
    // dos estuviera en otro dia, la columna `estado` de ese reporte mediria
    // menos y la red no estaria midiendo lo que tiene que medir.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "Consultorio 1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(9, 30), 45, "Consultorio 2",
              EstadoTurno::Reprogramado));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    // POR FECHA (66 columnas con estos datos)
    pantalla.manejar(eventoCaracter('1'));
    escribir(pantalla, "2027-03-10");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Resultado");
    CHECK(anchoNatural(pantalla.renderizar()) <= 80);

    // POR PACIENTE (60 columnas con estos datos)
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "1");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Resultado");
    CHECK(anchoNatural(pantalla.renderizar()) <= 80);

    // POR MEDICO (61 columnas con estos datos)
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('3'));
    escribir(pantalla, "1");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Resultado");
    CHECK(anchoNatural(pantalla.renderizar()) <= 80);

    // POR ESPECIALIDAD (79 columnas con estos datos)
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('4'));
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Resultado");
    CHECK(anchoNatural(pantalla.renderizar()) <= 80);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

TEST_CASE("el menu principal ofrece la opcion de reportes", menu_principal_reportes) {
    EscenarioReportes escenario("menu_principal_reportes");
    ui::Pantallas::MenuPrincipal menu(escenario.aplicacion);

    const std::string texto = menu.textoPlano();
    CHECK(texto.find("1. Pacientes") != std::string::npos);
    CHECK(texto.find("2. Medicos") != std::string::npos);
    CHECK(texto.find("3. Turnos") != std::string::npos);
    CHECK(texto.find("4. Reportes") != std::string::npos);
    CHECK(texto.find("0. Salir") != std::string::npos);

    // Y EL ATAJO ABRE LA PANTALLA DE REPORTES.
    menu.manejar(eventoCaracter('4'));
    CHECK(escenario.aplicacion.textoPlanoActual().find("REPORTES") != std::string::npos);
}

