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
#include "ui/CargarEjemplo.h"
#include "ui/ColorFtxui.h"
#include "ui/Confirmacion.h"
#include "ui/Consola.h"
#include "ui/Contexto.h"
#include "ui/CrudPersonas.h"
#include "ui/Evento.h"
#include "ui/MapaCalor.h"
#include "ui/Menu.h"
#include "ui/Mensaje.h"
#include "ui/Mes.h"
#include "ui/Pantallas/AcercaDe.h"
#include "ui/Pantallas/Calendario.h"
#include "ui/Pantallas/Configuracion.h"
#include "ui/Pantallas/MenuPrincipal.h"
#include "ui/Pantallas/Pacientes.h"
#include "ui/Pantallas/Medicos.h"
#include "ui/Pantallas/Reportes.h"
#include "ui/Pantallas/Turnos.h"
#include "ui/Semana.h"
#include "ui/Tabla.h"
#include "ui/Terminal.h"
#include "ui/Theme.h"
#include "utils/Fecha.h"

#include <ftxui.hpp>

#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
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
//
// OJO CON EL CASO DE BORRAR EN WINDOWS
//
// `_putenv_s(nombre, nullptr)` NO borra la variable en todas las
// implementaciones de la CRT (con MinGW la deja como estaba). La forma que
// si la borra en todas es la cadena vacia: `getenv` devuelve NULL y la
// variable deja de existir.
void definirNoColor(const char* valor) {
#if defined(_WIN32)
    _putenv_s("NO_COLOR", valor == nullptr ? "" : valor);
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

// Elige una entidad con el BUSCADOR.
//
// EL FLUJO SON DOS PASOS
//
// Se escribe el texto (el Enter BUSCA) y despues se elige la coincidencia
// (el Enter ELIGE la que esta resaltada). Es el flujo que hace la pantalla
// con el `ui::Buscador`.
template <class T>
void elegirEnElBuscador(ui::CrudPersonas<T>& pantalla, const std::string& texto) {
    escribir(pantalla, texto);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // busca
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige
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
    CHECK(motor.vista() == "Busqueda");

    // Se busca por el id (el criterio "id") y se elige la coincidencia.
    elegirEnElBuscador(motor, "1");

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
    CHECK(motor.vista() == "Busqueda");
    elegirEnElBuscador(motor, "1");

    // La vista de confirmacion pregunta QUE va a pasar.
    CHECK(motor.vista() == "Confirmar");
    CHECK(motor.vistaTextoPlano().find("Dar de baja al paciente #1?") != std::string::npos);

    // "No": vuelve a la vista anterior (la del buscador) y NO toca nada.
    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Busqueda");

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
    elegirEnElBuscador(motor, "1");
    motor.manejar(eventoCaracter('1'));  // "Si"

    CHECK(motor.vista() == "Menu");
    CHECK(escenario.pacientes.buscarPorId(1, paciente) == true);
    CHECK(paciente.esActivo() == false);
    CHECK(escenario.contexto.mensaje().texto() == "OK  Paciente #1 dado de baja.");

    // LA REACTIVACION
    motor.manejar(eventoCaracter('3'));
    elegirEnElBuscador(motor, "1");
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

TEST_CASE("un listado vacio dice que no hay y ofrece cargar el ejemplo",
           crud_listado_vacio) {
    Escenario escenario("crud_listado_vacio");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('4'));
    CHECK(motor.vista() == "Listado");

    // EL ESTADO VACIO ES UN MENU DE DOS OPCIONES
    //
    // El texto plano conserva el formato del menu ("  N. Etiqueta").
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("No hay pacientes") != std::string::npos);
    CHECK(texto.find("1. Cargar ejemplo") != std::string::npos);
    CHECK(texto.find("2. Volver") != std::string::npos);
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
    CHECK(motor.vista() == "Busqueda");
    elegirEnElBuscador(motor, "1");

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

TEST_CASE("una busqueda sin coincidencias dice que no hay y vuelve al texto",
           crud_busqueda_sin_coincidencias) {
    Escenario escenario("crud_busqueda_sin_coincidencias");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "9");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // La vista NO cambia: el usuario corrige el texto.
    CHECK(motor.vista() == "Busqueda");
    CHECK(motor.vistaTextoPlano().find("no hay coincidencias") != std::string::npos);
    // El texto NO se borra: el usuario lo corrige, no lo vuelve a escribir.
    CHECK(motor.vistaTextoPlano().find("9|") != std::string::npos);
}

TEST_CASE("en un campo de texto el cero es un dato si ya hay texto escrito",
           crud_cero_es_dato) {
    Escenario escenario("crud_cero_es_dato");
    escenario.pacientes.alta(pacienteDePrueba(10, "Ana", "Gomez", "2020391"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "1");
    escribir(motor, "0");

    // El cero NO fue "volver": es parte del texto de la busqueda.
    CHECK(motor.vista() == "Busqueda");
    CHECK(motor.vistaTextoPlano().find("Nombre, apellido o DNI: 10|") != std::string::npos);
}

TEST_CASE("el cero de un campo vacio vuelve al menu del motor", crud_cero_vuelve) {
    Escenario escenario("crud_cero_vuelve");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Busqueda");

    motor.manejar(eventoCaracter('0'));
    CHECK(motor.vista() == "Menu");
}

TEST_CASE("un clic en el encabezado de la confirmacion no confirma nada",
           crud_confirmacion_raton) {
    Escenario escenario("crud_confirmacion_raton");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));

    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('3'));
    elegirEnElBuscador(motor, "1");
    CHECK(motor.vista() == "Confirmar");

    // Un clic en el ENCABEZADO (la fila 0): cae fuera de las opciones y no
    // confirma nada. (La `ui::Confirmacion` sabe en que fila se dibujan sus
    // opciones: el motor se lo dice al crearla.)
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

TEST_CASE("el render del buscador se puede dibujar en una pantalla de FTXUI",
           crud_buscador_render) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    Escenario escenario("crud_buscador_render");
    escenario.pacientes.alta(pacienteDePrueba(7, "Ana", "Garcia", "2020391"));
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "Garcia");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    const std::vector<std::string> lineas = renderizarLineas(motor.renderizar(), 60, 12);
    CHECK(lineas.size() == 12);
    if (lineas.size() == 12) {
        // EL TITULO, EL CAMPO Y LA LINEA DE LA COINCIDENCIA.
        CHECK(lineas[2].find("Buscar paciente") != std::string::npos);
        CHECK(lineas[3].find("Nombre, apellido o DNI: Garcia|") != std::string::npos);
        CHECK(lineas[4].find("1. 7 · Garcia · Ana · 2020391") != std::string::npos);
    }

    // UN CLIC EN LA FILA DE UNA COINCIDENCIA LA ELIGE
    //
    // El hit-testing del raton necesita que la pantalla se haya DIBUJADO: el
    // motor le avisa al buscador en que fila arranca la lista cuando dibuja.
    ui::Evento clic;
    clic.boton = ui::BotonMouse::Izquierdo;
    clic.x = 4;
    clic.y = 4;
    motor.manejar(clic);

    // El clic NO elige: la confirmacion es el Enter.
    CHECK(motor.vista() == "Busqueda");

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

    // La vista del buscador: el `Escape` vuelve al menu del motor.
    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Busqueda");
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

TEST_CASE("el escape de la confirmacion vuelve a la vista del buscador",
           crud_escape_de_la_confirmacion) {
    Escenario escenario("crud_escape_de_la_confirmacion");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('3'));
    elegirEnElBuscador(motor, "1");
    CHECK(motor.vista() == "Confirmar");

    CHECK(motor.manejar(eventoTecla(ui::TipoTecla::Escape)) == true);
    CHECK(motor.vista() == "Busqueda");
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
// EL BUSCADOR DE ENTIDADES
//
// LA ENTIDAD SE BUSCA, NO SE ESCRIBE
//
// Las tres acciones que asktaban por un id (Modificar, Baja/Reactivar y Ver
// ficha) ahora pasan por el `ui::Buscador`: se escribe un dato que el
// usuario CONOCE y se elige una de las coincidencias.
//
// Lo que se verifica aca:
//
//   - que el prompt del id DESAPARECIO de la pantalla;
//   - que la lista muestra las columnas que declara la configuracion;
//   - que con el texto vacio se ven los activos;
//   - que las tres acciones terminan donde terminaban antes.
// ---------------------------------------------------------------------------

TEST_CASE("la pantalla del buscador ya no pide el id", crud_buscador_sin_prompt_de_id) {
    Escenario escenario("crud_buscador_sin_prompt_de_id");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Busqueda");

    const std::string texto = motor.vistaTextoPlano();
    // EL CAMPO ES EL DE LA BUSQUEDA, NO EL DEL ID.
    CHECK(texto.find("Nombre, apellido o DNI:") != std::string::npos);
    CHECK(texto.find("Id:") == std::string::npos);
    CHECK(texto.find("Buscar paciente") != std::string::npos);
}

TEST_CASE("el buscador muestra las columnas que declara la configuracion",
           crud_buscador_muestra_las_columnas) {
    Escenario escenario("crud_buscador_muestra_las_columnas");
    escenario.pacientes.alta(pacienteDePrueba(7, "Ana", "García", "2020391"));
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "García");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // LA LINEA DE LA COINCIDENCIA
    //
    // Son las celdas de las columnas del buscador (id, apellido, nombre y
    // dni), en ese orden y unidas por un punto.
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("7 · García · Ana · 2020391") != std::string::npos);
}

TEST_CASE("con el texto vacio el buscador muestra los activos",
           crud_buscador_muestra_los_activos) {
    Escenario escenario("crud_buscador_muestra_los_activos");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.pacientes.alta(pacienteDePrueba(2, "Beto", "Diaz", "2020392"));
    // Uno dado de baja: no es un paciente que se pueda usar.
    escenario.pacientes.alta(pacienteDePrueba(3, "Caro", "Sosa", "2020393"));
    escenario.pacientes.bajaLogica(3);
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    // El Enter con el texto vacio busca: se ven los activos.
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("Ana") != std::string::npos);
    CHECK(texto.find("Beto") != std::string::npos);
    CHECK(texto.find("Caro") == std::string::npos);
}

TEST_CASE("una entidad que no existe no se puede elegir con el buscador",
           crud_buscador_no_hay_entidades) {
    Escenario escenario("crud_buscador_no_hay_entidades");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "9");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // NO HAY COINCIDENCIAS: no se elige nada y el foco vuelve al texto.
    CHECK(motor.vista() == "Busqueda");
    CHECK(motor.vistaTextoPlano().find("no hay coincidencias") != std::string::npos);

    // Y el siguiente Enter tampoco elige: la lista sigue vacia.
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(motor.vista() == "Busqueda");
}

TEST_CASE("la busqueda del buscador no repite la misma entidad",
           crud_buscador_sin_repetidos) {
    // DOS CRITERIOS QUE DEVUELVEN LA MISMA ENTIDAD
    //
    // El buscador corre TODOS los criterios de la configuracion y une los
    // resultados: si dos criterios devuelven la misma entidad, en la lista
    // tiene que aparecer UNA vez (el primero que aparece gana).
    Escenario escenario("crud_buscador_sin_repetidos");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));

    ui::CrudPersonas<Paciente>::Config config;
    config.singular = "paciente";
    config.plural = "pacientes";
    config.cabeceras = {"id", "nombre", "apellido", "dni"};
    config.fila = [](const Paciente& paciente) {
        return std::vector<std::string>{std::to_string(paciente.getId()), paciente.getNombre(),
                                        paciente.getApellido(), paciente.getDni()};
    };
    config.esActivo = [](const Paciente&) { return true; };
    config.columnasBuscador = {"id", "apellido", "nombre", "dni"};
    config.pistaBuscador = "Nombre, apellido o DNI";

    // LOS DOS CRITERIOS DEVUELVEN EL MISMO PACIENTE.
    const auto mismo = [&escenario](const std::string&) {
        std::vector<ui::Coincidencia> coincidencias;
        Paciente paciente;
        if (escenario.pacientes.buscarPorId(1, paciente)) {
            coincidencias.push_back({paciente.getId(),
                                      {std::to_string(paciente.getId()), paciente.getNombre(),
                                       paciente.getApellido(), paciente.getDni()}});
        }
        return coincidencias;
    };
    config.criterios = {{"uno", mismo}, {"dos", mismo}};

    ui::CrudPersonas<Paciente> motor(escenario.aplicacion, escenario.pacientes,
                                     escenario.pacientes, config);

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "Gomez");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // UNA SOLA LINEA: la entidad no puede aparecer dos veces.
    const std::string texto = motor.vistaTextoPlano();
    const std::size_t primera = texto.find("1 · Gomez · Ana · 2020391");
    CHECK(primera != std::string::npos);
    CHECK(texto.find("1 · Gomez · Ana · 2020391", primera + 1) == std::string::npos);
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
    CHECK(motor.vista() == "Busqueda");
    elegirEnElBuscador(motor, "1");

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
    elegirEnElBuscador(motor, "1");
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
    elegirEnElBuscador(motor, "1");
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
    elegirEnElBuscador(motor, "1");
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
    elegirEnElBuscador(motor, "1");
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
// EL BUSCADOR DE MEDICOS
//
// Las tres acciones de medicos pasan por el mismo `ui::Buscador` que las de
// pacientes: el motor es el mismo y lo unico que cambia son los datos.
// ---------------------------------------------------------------------------

TEST_CASE("el buscador de medicos muestra las columnas que declara la configuracion",
           crud_buscador_medicos_muestra_las_columnas) {
    EscenarioMedicos escenario("crud_buscador_medicos_muestra_las_columnas");
    // El medico #3 de la semilla: Camila Sosa, de traumatologia.
    escenario.medicos.alta(Medico(3, "Camila", "Sosa", "camila.sosa@unlp.edu.ar", "2214556699",
                                   "LEG-1003", Especialidad::TRAU));
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('2'));
    escribir(motor, "Sosa");
    motor.manejar(eventoTecla(ui::TipoTecla::Enter));

    // ID, APELLIDO, NOMBRE, ESPECIALIDAD Y LEGAJO.
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("3 · Sosa · Camila · Traumatología · LEG-1003") != std::string::npos);
}

TEST_CASE("la baja de un medico pasa por el buscador y termina en la confirmacion",
           crud_buscador_medicos_baja) {
    EscenarioMedicos escenario("crud_buscador_medicos_baja");
    escenario.medicos.alta(medicoDePrueba(1, "Ana", "Gomez", "LEG-1001"));
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('3'));
    CHECK(motor.vista() == "Busqueda");

    elegirEnElBuscador(motor, "Gomez");

    CHECK(motor.vista() == "Confirmar");
    CHECK(motor.vistaTextoPlano().find("Dar de baja al medico #1?") != std::string::npos);

    motor.manejar(eventoCaracter('1'));  // "Si"
    CHECK(motor.vista() == "Menu");
    Medico medico;
    CHECK(escenario.medicos.buscarPorId(1, medico) == true);
    CHECK(medico.esActivo() == false);
}

TEST_CASE("volver de la confirmacion al buscador NO vuelve a disparar la accion",
           crud_buscador_volver_no_re_dispara) {
    Escenario escenario("crud_buscador_volver_no_re_dispara");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('3'));  // Baja / Reactivar
    CHECK(motor.vista() == "Busqueda");
    elegirEnElBuscador(motor, "Gomez");
    CHECK(motor.vista() == "Confirmar");

    // El "No" de la confirmacion vuelve al buscador.
    motor.manejar(eventoCaracter('2'));
    CHECK(motor.vista() == "Busqueda");

    // LA FLECHA NO VUELVE A ABRIR LA CONFIRMACION
    //
    // El "elegido" del buscador es POR EVENTO: la eleccion que cuenta es la
    // de la ultima tecla, no la de una anterior. Sin eso, el "No" de la
    // confirmacion y despues una flecha volvian a disparar la baja.
    motor.manejar(eventoTecla(ui::TipoTecla::FlechaAbajo));
    CHECK(motor.vista() == "Busqueda");
}

TEST_CASE("la ficha de un medico pasa por el buscador", crud_buscador_medicos_ficha) {
    EscenarioMedicos escenario("crud_buscador_medicos_ficha");
    escenario.medicos.alta(medicoDePrueba(1, "Ana", "Gomez", "LEG-1001"));
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('6'));
    CHECK(motor.vista() == "Busqueda");

    elegirEnElBuscador(motor, "Gomez");

    CHECK(motor.vista() == "Ficha");
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("Ficha de medico #1") != std::string::npos);
    CHECK(texto.find("Turnos vigentes") != std::string::npos);
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
        elegirEnElBuscador(motor, "1");
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
        elegirEnElBuscador(motor, "1");
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
    CHECK(motor.vista() == "Busqueda");
    elegirEnElBuscador(motor, "1");
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
    elegirEnElBuscador(motor, "1");
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

// Elige un turno en el BUSCADOR de la pantalla de turnos.
//
// EL FLUJO SON DOS PASOS
//
// Se escribe el texto (el Enter BUSCA) y despues se elige la coincidencia
// (el Enter ELIGE la que esta resaltada). Es el flujo que hace la pantalla
// con el `ui::Buscador`.
void elegirTurnoEnElBuscador(ui::Pantallas::PantallaTurnos& pantalla, const std::string& texto) {
    escribir(pantalla, texto);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // busca
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige
}

// Elige una PERSONA en el buscador de "agendar".
//
// EL FLUJO SON DOS PASOS
//
// Se escribe el texto (el Enter BUSCA) y despues se elige la coincidencia
// (el Enter ELIGE la que esta resaltada). El texto es un dato que el usuario
// CONOCE (el nombre, el apellido, el dni, el legajo): el id no se escribe.
void elegirPersonaEnElBuscador(ui::Pantallas::PantallaTurnos& pantalla, const std::string& texto) {
    escribir(pantalla, texto);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // busca
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige
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

// AGENDA UN TURNO: ELIGE LAS PERSONAS, LA ESPECIALIDAD Y LLENA EL FORMULARIO
//
// "AGENDAR" TIENE CUATRO PASOS
//
//   1. se elige el PACIENTE con el buscador;
//   2. se elige la ESPECIALIDAD con el selector;
//   3. se elige el MEDICO con el buscador (que sale filtrado por la
//      especialidad);
//   4. se llena el formulario (la fecha, la hora, la duracion y el
//      consultorio).
//
// Los dos ids NO se escriben: se buscan. Los textos son los que escribiria
// el usuario (el nombre o el apellido).
//
// `numeroDeEspecialidad` es el numero que se escribe en el selector: la
// POSICION en el catalogo (1 es Cardiologia, 3 es Pediatria).
//
// Devuelve sin hacer nada: el llamador confirma el formulario.
void agendarConLasPersonasElegidas(ui::Pantallas::PantallaTurnos& pantalla,
                                    const std::string& textoPaciente,
                                    const std::string& textoMedico, const std::string& fecha,
                                    const std::string& hora, const std::string& duracion = "45",
                                    int numeroDeEspecialidad = 1) {
    elegirPersonaEnElBuscador(pantalla, textoPaciente);
    escribir(pantalla, std::to_string(numeroDeEspecialidad));
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige la especialidad
    elegirPersonaEnElBuscador(pantalla, textoMedico);
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
    // EL PRIMER PASO ES EL BUSCADOR DEL PACIENTE
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("Buscar paciente") != std::string::npos);

    // EL PACIENTE, LA ESPECIALIDAD Y EL MEDICO SE ELIGEN CON EL BUSCADOR
    agendarConLasPersonasElegidas(pantalla, "Ana", "Beto", fechaDesplazada(30).aTexto(), "10:00");
    CHECK(pantalla.vista() == "Formulario");

    // EL FORMULARIO SE CONFIRMA: NO HAY SELECTOR DESPUES.
    //
    // En "agendar" la especialidad ya se eligio antes del medico, asi que el
    // turno sale con la del medico por construccion (la regla 2b no se puede
    // violar desde la interfaz).
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

TEST_CASE("el flujo de agendar es paciente -> especialidad -> medico -> formulario",
           turnos_agendar_el_flujo) {
    EscenarioTurnos escenario("turnos_agendar_el_flujo");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    // Un medico de Cardiologia y otro de Pediatria: el buscador de medicos
    // sale filtrado por la especialidad elegida.
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.medicos.alta(
        medicoConEspecialidad(2, "Dani", "Ruiz", "LEG-1002", Especialidad::PEDI));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // 1. EL PACIENTE
    pantalla.manejar(eventoCaracter('1'));
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("Buscar paciente") != std::string::npos);
    elegirPersonaEnElBuscador(pantalla, "Ana");

    // 2. LA ESPECIALIDAD (el selector, antes del medico)
    CHECK(pantalla.vista() == "Selector");
    CHECK(pantalla.vistaTextoPlano().find("1. Cardiolog") != std::string::npos);
    // Se elige la tercera del catalogo: Pediatria.
    pantalla.manejar(eventoCaracter('3'));
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // 3. EL MEDICO (el buscador, ya filtrado)
    CHECK(pantalla.vista() == "Busqueda");
    const std::string buscador = pantalla.vistaTextoPlano();
    CHECK(buscador.find("Buscar medico de ") != std::string::npos);
    CHECK(buscador.find("Pediatr") != std::string::npos);
    // EL MEDICO DE CARDIOLOGIA NO ESTA: la lista sale filtrada.
    CHECK(buscador.find("Beto") == std::string::npos);
    CHECK(buscador.find("Dani") != std::string::npos);
    elegirPersonaEnElBuscador(pantalla, "Dani");

    // 4. EL FORMULARIO
    CHECK(pantalla.vista() == "Formulario");
    escribir(pantalla, fechaDesplazada(30).aTexto());
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "10:00");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "45");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "A-2");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // 5. AGENDAR
    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Turno #1 agendado.");

    // EL TURNO TIENE LA ESPECIALIDAD ELEGIDA Y EL MEDICO DE ESA ESPECIALIDAD
    Turno agendado;
    CHECK(escenario.turnos.buscarPorId(1, agendado) == true);
    CHECK(agendado.getEspecialidad() == Especialidad::PEDI);
    CHECK(agendado.getIdMedico() == 2);
    CHECK(agendado.getIdPaciente() == 1);
}

TEST_CASE("el buscador de medicos muestra solo los de la especialidad elegida",
           turnos_agendar_el_buscador_filtra) {
    EscenarioTurnos escenario("turnos_agendar_el_buscador_filtra");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.medicos.alta(
        medicoConEspecialidad(2, "Caro", "Sosa", "LEG-1002", Especialidad::CARD));
    escenario.medicos.alta(
        medicoConEspecialidad(3, "Dani", "Ruiz", "LEG-1003", Especialidad::PEDI));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    elegirPersonaEnElBuscador(pantalla, "Ana");
    // Se elige Cardiologia (la primera del catalogo).
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // CON EL TEXTO VACIO: SOLO LOS DE CARDIOLOGIA
    std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Beto") != std::string::npos);
    CHECK(texto.find("Caro") != std::string::npos);
    CHECK(texto.find("Dani") == std::string::npos);

    // CON TEXTO: EL CRITERIO COMPARTIDO CORRE Y EL FILTRO SIGUE PUESTO
    escribir(pantalla, "Ruiz");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // busca
    texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("no hay coincidencias") != std::string::npos);
    CHECK(texto.find("Dani") == std::string::npos);

    // Y EL TEXTO QUE SI ES DE LA ESPECIALIDAD ELEGIDA ENCUENTRA AL MEDICO
    //
    // La busqueda sin resultado NO borra el texto (el usuario lo corrige):
    // se borra a mano antes de escribir el otro nombre.
    for (int i = 0; i < 4; ++i) {
        pantalla.manejar(eventoTecla(ui::TipoTecla::Retroceso));
    }
    escribir(pantalla, "Beto");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // busca
    texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Beto") != std::string::npos);
    CHECK(texto.find("no hay coincidencias") == std::string::npos);
}

TEST_CASE("una especialidad sin medicos avisa y vuelve al selector",
           turnos_agendar_especialidad_sin_medicos) {
    EscenarioTurnos escenario("turnos_agendar_especialidad_sin_medicos");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    // El unico medico es de Cardiologia: las otras once especialidades no
    // tienen ninguno (como en la semilla).
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    elegirPersonaEnElBuscador(pantalla, "Ana");
    CHECK(pantalla.vista() == "Selector");

    // Se elige Neurologia (la sexta del catalogo): no hay medicos de ella.
    pantalla.manejar(eventoCaracter('6'));
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // LA PANTALLA LO DICE CLARO...
    CHECK(escenario.contexto.mensaje().texto()
          == "ERR no hay medicos de la especialidad Neurología");
    // ...Y VUELVE AL SELECTOR (no al menu), para que elija otra.
    CHECK(pantalla.vista() == "Selector");

    // EL USUARIO ELIGE UNA QUE SI TIENE MEDICOS: EL FLUJO SIGUE
    pantalla.manejar(eventoCaracter('1'));
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("Beto") != std::string::npos);
}

TEST_CASE("una especialidad sin medicos activos avisa y vuelve al selector",
           turnos_agendar_especialidad_sin_medicos_activos) {
    EscenarioTurnos escenario("turnos_agendar_especialidad_sin_medicos_activos");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    // El unico medico de Pediatria esta DADO DE BAJA: no puede atender.
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::PEDI));
    escenario.medicos.bajaLogica(1);

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    elegirPersonaEnElBuscador(pantalla, "Ana");
    // Se elige Pediatria (la tercera del catalogo).
    pantalla.manejar(eventoCaracter('3'));
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(escenario.contexto.mensaje().texto()
          == "ERR no hay medicos de la especialidad Pediatría");
    CHECK(pantalla.vista() == "Selector");
}

TEST_CASE("la pantalla no puede agendar un turno con la especialidad de otro medico",
           turnos_agendar_no_se_puede_contradecir) {
    EscenarioTurnos escenario("turnos_agendar_no_se_puede_contradecir");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    // El medico atiende Gastroenterologia, no Cardiologia.
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::GAST));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    elegirPersonaEnElBuscador(pantalla, "Ana");

    // Se elige la PRIMERA especialidad (Cardiologia): el medico de
    // Gastroenterologia no sale en el buscador, asi que la pantalla NO puede
    // escribir la contradiccion.
    //
    // La regla 2b la sigue aplicando el SERVICIO (que es la que protege a
    // las otras rutas): su test esta en `tests/test_servicio.cpp` ("agendar
    // un turno de OTRA especialidad lanza DatoInvalidoException").
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(escenario.contexto.mensaje().texto()
          == "ERR no hay medicos de la especialidad Cardiología");
    CHECK(pantalla.vista() == "Selector");

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
    agendarConLasPersonasElegidas(pantalla, "Ana", "Beto", fechaDesplazada(30).aTexto(), "10:00");
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
    agendarConLasPersonasElegidas(pantalla, "Ana", "Beto", fechaDesplazada(-5).aTexto(), "10:00");
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
    agendarConLasPersonasElegidas(pantalla, "Ana", "Beto", fechaDesplazada(30).aTexto(), "07:00");
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
    agendarConLasPersonasElegidas(pantalla, "Ana", "Beto", fechaDesplazada(30).aTexto(), "10:00",
                                  "7");
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
    agendarConLasPersonasElegidas(pantalla, "Ana", "Beto", fechaDesplazada(30).aTexto(), "8:00");
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

    // EL PACIENTE, LA ESPECIALIDAD Y EL MEDICO SE ELIGEN PRIMERO
    elegirPersonaEnElBuscador(pantalla, "Ana");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige la especialidad
    elegirPersonaEnElBuscador(pantalla, "Beto");
    CHECK(pantalla.vista() == "Formulario");

    // Se llega al campo de la hora y se escribe "09:00".
    escribir(pantalla, fechaDesplazada(30).aTexto());
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "09:00");

    // El primer cero NO fue "volver": la hora SIEMPRE empieza con un cero.
    CHECK(pantalla.vista() == "Formulario");
    CHECK(pantalla.vistaTextoPlano().find("hora: 09:00|") != std::string::npos);
}

// ---------------------------------------------------------------------------
// EL BUSCADOR DE PERSONAS DE "AGENDAR"
//
// EL PACIENTE Y EL MEDICO SE BUSCAN, NO SE ESCRIBEN
//
// Lo que se verifica aca:
//
//   - que la pantalla de agendar ya NO pide los ids (ni el del paciente ni
//     el del medico);
//   - que el turno creado tiene el paciente y el medico ELEGIDOS;
//   - que las columnas del buscador son las mismas que usan las pantallas de
//     pacientes y de medicos;
//   - que con el texto vacio se listan los ACTIVOS (un medico dado de baja
//     no se puede atender);
//   - que el '0' y el `Escape` en cualquiera de los pasos vuelven al menu del
//     modulo.
//
// EL ORDEN DE LOS PASOS
//
// El del paciente, el de la especialidad (el selector) y el del medico. El
// buscador del medico sale FILTRADO por la especialidad elegida.
// ---------------------------------------------------------------------------

TEST_CASE("la pantalla de agendar ya no pide el id del paciente ni del medico",
           turnos_agendar_sin_prompt_de_id) {
    EscenarioTurnos escenario("turnos_agendar_sin_prompt_de_id");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));

    // EL PRIMER PASO ES EL BUSCADOR DEL PACIENTE
    CHECK(pantalla.vista() == "Busqueda");
    std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Buscar paciente") != std::string::npos);
    CHECK(texto.find("Nombre, apellido o DNI:") != std::string::npos);
    CHECK(texto.find("id paciente") == std::string::npos);
    CHECK(texto.find("id medico") == std::string::npos);

    // EL SEGUNDO PASO ES EL SELECTOR DE LA ESPECIALIDAD
    elegirPersonaEnElBuscador(pantalla, "Ana");
    CHECK(pantalla.vista() == "Selector");
    texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("1. Cardiolog") != std::string::npos);
    CHECK(texto.find("id paciente") == std::string::npos);
    CHECK(texto.find("id medico") == std::string::npos);

    // EL TERCER PASO ES EL BUSCADOR DEL MEDICO
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige la especialidad
    CHECK(pantalla.vista() == "Busqueda");
    texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Buscar medico") != std::string::npos);
    CHECK(texto.find("Nombre, apellido, legajo o especialidad:") != std::string::npos);
    CHECK(texto.find("id paciente") == std::string::npos);
    CHECK(texto.find("id medico") == std::string::npos);

    // Y EL FORMULARIO TIENE SOLO LOS DATOS DEL TURNO
    elegirPersonaEnElBuscador(pantalla, "Beto");
    CHECK(pantalla.vista() == "Formulario");
    texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("fecha:") != std::string::npos);
    CHECK(texto.find("hora:") != std::string::npos);
    CHECK(texto.find("duracion:") != std::string::npos);
    CHECK(texto.find("consultorio:") != std::string::npos);
    CHECK(texto.find("id paciente") == std::string::npos);
    CHECK(texto.find("id medico") == std::string::npos);
}

TEST_CASE("el turno agendado tiene el paciente y el medico ELEGIDOS",
           turnos_agendar_las_personas_elegidas) {
    EscenarioTurnos escenario("turnos_agendar_las_personas_elegidas");
    // DOS PACIENTES Y DOS MEDICOS: el que se elige es el que queda en el turno.
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.pacientes.alta(pacienteDePrueba(2, "Caro", "Sosa", "2020392"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.medicos.alta(
        medicoConEspecialidad(2, "Dani", "Ruiz", "LEG-1002", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    // EL SEGUNDO PACIENTE, LA ESPECIALIDAD Y EL SEGUNDO MEDICO
    elegirPersonaEnElBuscador(pantalla, "Sosa");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige la especialidad
    elegirPersonaEnElBuscador(pantalla, "Ruiz");

    escribir(pantalla, fechaDesplazada(30).aTexto());
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "10:00");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "45");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Tabulador));
    escribir(pantalla, "A-2");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // confirma el formulario
    CHECK(pantalla.vista() == "Menu");

    // EL TURNO TIENE A LOS ELEGIDOS
    Turno agendado;
    CHECK(escenario.turnos.buscarPorId(1, agendado) == true);
    CHECK(agendado.getIdPaciente() == 2);
    CHECK(agendado.getIdMedico() == 2);
}

TEST_CASE("el buscador de agendar muestra las mismas columnas que las pantallas de personas",
           turnos_agendar_las_columnas_del_buscador) {
    EscenarioTurnos escenario("turnos_agendar_las_columnas_del_buscador");
    escenario.pacientes.alta(pacienteDePrueba(7, "Ana", "García", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(3, "Beto", "Pérez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // EL PACIENTE: id, apellido, nombre y dni
    pantalla.manejar(eventoCaracter('1'));
    escribir(pantalla, "García");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // busca
    CHECK(pantalla.vistaTextoPlano().find("7 · García · Ana · 2020391") != std::string::npos);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige

    // LA ESPECIALIDAD: el selector va ENTRE los dos buscadores
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige Cardiologia
    CHECK(pantalla.vista() == "Busqueda");

    // EL MEDICO: id, apellido, nombre, especialidad y legajo
    escribir(pantalla, "Pérez");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // busca
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("3 · Pérez · Beto · ") != std::string::npos);
    CHECK(texto.find("LEG-1001") != std::string::npos);
}

TEST_CASE("con el texto vacio el buscador de agendar muestra los activos",
           turnos_agendar_muestra_los_activos) {
    EscenarioTurnos escenario("turnos_agendar_muestra_los_activos");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    // UN PACIENTE Y UN MEDICO DADOS DE BAJA: no se pueden atender.
    escenario.pacientes.alta(pacienteDePrueba(2, "Caro", "Sosa", "2020392"));
    escenario.pacientes.bajaLogica(2);
    escenario.medicos.alta(
        medicoConEspecialidad(2, "Dani", "Ruiz", "LEG-1002", Especialidad::CARD));
    escenario.medicos.bajaLogica(2);

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    // LA LISTA INICIAL ES LA BUSQUEDA DEL TEXTO VACIO: los ACTIVOS.
    std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Ana") != std::string::npos);
    // EL PACIENTE DADO DE BAJA NO APARECE: no se puede atender.
    CHECK(texto.find("Caro") == std::string::npos);

    // Y EL MEDICO DADO DE BAJA TAMPOCO.
    elegirPersonaEnElBuscador(pantalla, "Ana");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige la especialidad
    texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Beto") != std::string::npos);
    CHECK(texto.find("Dani") == std::string::npos);
}

TEST_CASE("el cero y el escape en cualquiera de los tres pasos vuelven al menu",
           turnos_agendar_cancelar_vuelve_al_menu) {
    EscenarioTurnos escenario("turnos_agendar_cancelar_vuelve_al_menu");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // EL CERO EN EL PRIMER PASO (el del paciente)
    pantalla.manejar(eventoCaracter('1'));
    CHECK(pantalla.vista() == "Busqueda");
    pantalla.manejar(eventoCaracter('0'));
    CHECK(pantalla.vista() == "Menu");

    // EL ESCAPE EN EL SEGUNDO PASO (el de la especialidad)
    //
    // El selector va ANTES del formulario: no hay formulario atras, asi que
    // el `Escape` cancela el flujo y vuelve al menu del modulo.
    pantalla.manejar(eventoCaracter('1'));
    elegirPersonaEnElBuscador(pantalla, "Ana");
    CHECK(pantalla.vista() == "Selector");
    CHECK(pantalla.manejar(eventoTecla(ui::TipoTecla::Escape)) == true);
    CHECK(pantalla.vista() == "Menu");

    // EL ESCAPE EN EL TERCER PASO (el del medico)
    pantalla.manejar(eventoCaracter('1'));
    elegirPersonaEnElBuscador(pantalla, "Ana");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige la especialidad
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.manejar(eventoTecla(ui::TipoTecla::Escape)) == true);
    CHECK(pantalla.vista() == "Menu");

    // Y LA PANTALLA SIGUE EN LA PILA: el menu del modulo, no la pantalla
    // anterior.
    CHECK(escenario.aplicacion.textoPlanoActual() != "pantalla de prueba");
}

TEST_CASE("cancelar un turno pide confirmacion y sin confirmar no toca nada",
           turnos_cancelar_sin_confirmar) {    EscenarioTurnos escenario("turnos_cancelar_sin_confirmar");
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Busqueda");
    elegirTurnoEnElBuscador(pantalla, "31");

    // LA CONFIRMACION DE DOS PASOS
    CHECK(pantalla.vista() == "Confirmar");
    CHECK(pantalla.vistaTextoPlano().find("Cancelar el turno #31?") != std::string::npos);

    // "No": vuelve a la vista del buscador y NO toca nada.
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Busqueda");

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
    elegirTurnoEnElBuscador(pantalla, "31");
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
    CHECK(pantalla.vista() == "Busqueda");
    elegirTurnoEnElBuscador(pantalla, "31");

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
    CHECK(pantalla.vista() == "Busqueda");
    elegirTurnoEnElBuscador(pantalla, "31");

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

TEST_CASE("modificar un turno sigue preguntando la especialidad al final del formulario",
           turnos_modificar_el_flujo) {
    EscenarioTurnos escenario("turnos_modificar_el_flujo");
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // EL FLUJO DE MODIFICAR NO CAMBIA
    //
    // Ahi la especialidad se corrige al FINAL: es el dato HISTORICO del
    // turno (decision D21), no el que se deduce del medico.
    pantalla.manejar(eventoCaracter('4'));
    elegirTurnoEnElBuscador(pantalla, "31");
    CHECK(pantalla.vista() == "Formulario");

    // EL FORMULARIO NO TIENE LA ESPECIALIDAD: se pregunta despues.
    CHECK(pantalla.vistaTextoPlano().find("especialidad") == std::string::npos);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Selector");

    // Y LA ESPECIALIDAD ELEGIDA ES LA QUE SE PONE AL TURNO.
    pantalla.manejar(eventoCaracter('2'));
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Menu");
    Turno modificado;
    CHECK(escenario.turnos.buscarPorId(31, modificado) == true);
    CHECK(modificado.getEspecialidad() == Especialidad::MEDG);
}

TEST_CASE("un turno que no existe no se puede elegir", turnos_id_inexistente) {
    EscenarioTurnos escenario("turnos_id_inexistente");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "99");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // NO HAY NADA QUE ELEGIR: el buscador lo dice y la vista no cambia.
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("no hay coincidencias") != std::string::npos);
    // Y NO se elige ningun turno: la pantalla sigue en el buscador.
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Busqueda");
}

TEST_CASE("en el texto del buscador de turnos el cero es un dato si ya hay texto",
           turnos_cero_es_dato) {
    EscenarioTurnos escenario("turnos_cero_es_dato");
    escenario.turnos.alta(
        Turno(310, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(10, 0), 45, "A-1"));

    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "31");
    escribir(pantalla, "0");

    // El cero NO fue "volver": es parte del texto.
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("estado o fecha: 310|") != std::string::npos);
}

TEST_CASE("el cero del texto vacio del buscador de turnos vuelve al menu",
           turnos_cero_vuelve) {
    EscenarioTurnos escenario("turnos_cero_vuelve");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Busqueda");

    pantalla.manejar(eventoCaracter('0'));
    CHECK(pantalla.vista() == "Menu");
}

TEST_CASE("el escape de los turnos sube un nivel y no sale de la pantalla",
           turnos_escape_sube_un_nivel) {
    EscenarioTurnos escenario("turnos_escape_sube_un_nivel");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Busqueda");
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
// EL BUSCADOR DE TURNOS
//
// Las cuatro acciones que operan sobre un turno existente (cancelar,
// reprogramar, modificar y ver detalle) eligen el turno con un `ui::Buscador`
// y no escribiendo su id.
//
// QUE SE VERIFICA ACa
//
//   - que con el texto VACIO se listen los turnos que la ACCION puede
//     operar (los `Activo` para las tres acciones que cambian el turno, TODOS
//     para "ver detalle": un cancelado se puede ver, es su historia);
//   - que la busqueda sea POR LO QUE EL TURNO MUESTRA: el texto tiene que
//     aparecer en la FILA COMPLETA del turno (los diez datos de la ficha);
//   - que la comparacion sea insensible a mayusculas y a tildes;
//   - que un texto que no coincide diga "no hay coincidencias";
//   - que la linea de la coincidencia sea la de las SEIS columnas de la
//     lista;
//   - que el raton mueva el resaltado y NO confirme;
//   - que las cuatro acciones lleguen a donde llegaban antes, y que el prompt
//     "Id:" no aparezca mas.
//
// LA REGLA DE LA COMPARACION
//
// No distingue MAYUSCULAS ni TILDES, como el resto de la aplicacion (los
// repositorios comparan los nombres con `Utils::aMinusculas`) y como hace
// falta con los datos de la semilla: "García", "Gómez" y "Cardiología" las
// tienen, y escribir una tilde en una terminal es incomodo.
// ---------------------------------------------------------------------------

// El escenario del BUSCADOR de turnos: una paciente y un medico (con
// nombres CON tilde, como los de la semilla) y dos turnos: uno ACTIVO y uno
// CANCELADO.
namespace {

struct EscenarioBuscadorTurnos {
    explicit EscenarioBuscadorTurnos(const std::string& nombre)
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

        pacientes.alta(pacienteDePrueba(1, "Ana", "García", "2020391"));
        pacientes.alta(pacienteDePrueba(2, "Bruno", "Soto", "2020392"));
        medicos.alta(
            medicoConEspecialidad(1, "Laura", "Gómez", "LEG-1001", Especialidad::CARD));
        medicos.alta(
            medicoConEspecialidad(2, "Carla", "Ruiz", "LEG-1002", Especialidad::PEDI));

        // El turno ACTIVO: el unico que se puede cancelar, reprogramar o
        // modificar.
        turnos.alta(
            Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(9, 0), 45,
                  "Consultorio 2"));
        // Y el CANCELADO: se puede VER (es su historia), pero no se puede
        // operar.
        turnos.alta(
            Turno(32, 2, 2, Especialidad::PEDI, Fecha(2027, 3, 11), Hora(10, 30), 30,
                  "Consultorio 3", EstadoTurno::Cancelado));
    }

    RepositorioPacientes pacientes;
    RepositorioMedicos medicos;
    RepositorioTurnos turnos;
    ServicioTurnos servicio;
    ui::Contexto contexto;
    ui::Aplicacion aplicacion;
    ui::Pantallas::PantallaTurnos* puntero = nullptr;
};

// Busca un texto en el BUSCADOR de turnos y devuelve lo que se ve.
//
// El buscador se abre de nuevo en cada busqueda (el `Escape` vuelve al
// menu), asi que cada busqueda arranca con el texto vacio.
//
// La accion es "ver detalle" (la '7'): con el texto escrito la busqueda no
// filtra por estado, asi que da el mismo resultado en las cuatro acciones.
std::string buscarEnElBuscadorDeTurnos(ui::Pantallas::PantallaTurnos& pantalla,
                                        const std::string& texto) {
    pantalla.manejar(eventoTecla(ui::TipoTecla::Escape));  // vuelve al menu
    pantalla.manejar(eventoCaracter('7'));                 // "Ver detalle"
    escribir(pantalla, texto);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));   // busca
    return pantalla.vistaTextoPlano();
}

} // namespace

TEST_CASE("con el texto vacio el buscador de turnos lista los turnos que la accion puede operar",
           turnos_buscador_texto_vacio) {
    EscenarioBuscadorTurnos escenario("turnos_buscador_texto_vacio");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // CANCELAR: solo los ACTIVOS (un cancelado no se puede cancelar).
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Busqueda");
    const std::string paraCancelar = pantalla.vistaTextoPlano();
    CHECK(paraCancelar.find("31 · ") != std::string::npos);
    CHECK(paraCancelar.find("32 · ") == std::string::npos);

    // REPROGRAMAR Y MODIFICAR: la misma regla (son las dos acciones mas).
    pantalla.manejar(eventoTecla(ui::TipoTecla::Escape));
    pantalla.manejar(eventoCaracter('3'));
    CHECK(pantalla.vistaTextoPlano().find("32 · ") == std::string::npos);

    pantalla.manejar(eventoTecla(ui::TipoTecla::Escape));
    pantalla.manejar(eventoCaracter('4'));
    CHECK(pantalla.vistaTextoPlano().find("32 · ") == std::string::npos);

    // VER DETALLE: TODOS (un cancelado se puede ver: es su historia).
    pantalla.manejar(eventoTecla(ui::TipoTecla::Escape));
    pantalla.manejar(eventoCaracter('7'));
    const std::string paraVer = pantalla.vistaTextoPlano();
    CHECK(paraVer.find("31 · ") != std::string::npos);
    CHECK(paraVer.find("32 · ") != std::string::npos);
}

TEST_CASE("el buscador de turnos busca en la fila completa del turno",
           turnos_buscador_busca_en_la_fila) {
    EscenarioBuscadorTurnos escenario("turnos_buscador_busca_en_la_fila");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // POR EL NOMBRE DEL PACIENTE
    std::string texto = buscarEnElBuscadorDeTurnos(pantalla, "Ana");
    CHECK(texto.find("31 · ") != std::string::npos);
    CHECK(texto.find("32 · ") == std::string::npos);

    // POR EL APELLIDO DEL PACIENTE (con tilde)
    texto = buscarEnElBuscadorDeTurnos(pantalla, "García");
    CHECK(texto.find("31 · ") != std::string::npos);
    CHECK(texto.find("32 · ") == std::string::npos);

    // POR EL APELLIDO DEL PACIENTE (SIN tilde: la comparacion las ignora)
    texto = buscarEnElBuscadorDeTurnos(pantalla, "Garcia");
    CHECK(texto.find("31 · ") != std::string::npos);
    CHECK(texto.find("32 · ") == std::string::npos);

    // POR EL NOMBRE DEL MEDICO (con tilde)
    texto = buscarEnElBuscadorDeTurnos(pantalla, "Gómez");
    CHECK(texto.find("31 · ") != std::string::npos);
    CHECK(texto.find("32 · ") == std::string::npos);

    // POR LA ESPECIALIDAD
    //
    // El catalogo escribe "Cardiología" y el buscador no distingue tildes:
    // "Cardiologia" (sin tilde) lo encuentra igual.
    texto = buscarEnElBuscadorDeTurnos(pantalla, "Cardiologia");
    CHECK(texto.find("31 · ") != std::string::npos);
    CHECK(texto.find("32 · ") == std::string::npos);

    // POR EL CONSULTORIO
    texto = buscarEnElBuscadorDeTurnos(pantalla, "Consultorio 3");
    CHECK(texto.find("32 · ") != std::string::npos);
    CHECK(texto.find("31 · ") == std::string::npos);

    // POR EL ESTADO
    texto = buscarEnElBuscadorDeTurnos(pantalla, "Cancelado");
    CHECK(texto.find("32 · ") != std::string::npos);
    CHECK(texto.find("31 · ") == std::string::npos);

    // POR LA FECHA
    texto = buscarEnElBuscadorDeTurnos(pantalla, "2027-03-11");
    CHECK(texto.find("32 · ") != std::string::npos);
    CHECK(texto.find("31 · ") == std::string::npos);

    // POR EL ID
    texto = buscarEnElBuscadorDeTurnos(pantalla, "31");
    CHECK(texto.find("31 · ") != std::string::npos);
    CHECK(texto.find("32 · ") == std::string::npos);
}

TEST_CASE("el buscador de turnos no distingue mayusculas", turnos_buscador_mayusculas) {
    EscenarioBuscadorTurnos escenario("turnos_buscador_mayusculas");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // "ana" encuentra a "Ana García".
    const std::string texto = buscarEnElBuscadorDeTurnos(pantalla, "ana");
    CHECK(texto.find("31 · ") != std::string::npos);
    CHECK(texto.find("32 · ") == std::string::npos);

    // Y "ANA" tambien (las mayusculas no cuentan).
    const std::string mayusculas = buscarEnElBuscadorDeTurnos(pantalla, "ANA");
    CHECK(mayusculas.find("31 · ") != std::string::npos);
    CHECK(mayusculas.find("32 · ") == std::string::npos);
}

TEST_CASE("un texto que no coincide dice que no hay coincidencias",
           turnos_buscador_sin_coincidencias) {
    EscenarioBuscadorTurnos escenario("turnos_buscador_sin_coincidencias");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('7'));
    escribir(pantalla, "zzzz");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("no hay coincidencias") != std::string::npos);
    // Y NO se elige ningun turno: la vista no cambia.
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    CHECK(pantalla.vista() == "Busqueda");
}

TEST_CASE("la linea de la coincidencia del buscador de turnos tiene las seis columnas de la lista",
           turnos_buscador_linea_de_la_coincidencia) {
    EscenarioBuscadorTurnos escenario("turnos_buscador_linea_de_la_coincidencia");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('7'));
    escribir(pantalla, "Ana");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // ID · FECHA · HORA · PACIENTE · MEDICO · ESTADO
    //
    // Son las seis columnas de la lista (`filaDe`), no las diez de la ficha.
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("31 · 2027-03-10 · 09:00 · Ana García · Laura Gómez · Activo")
          != std::string::npos);
}

TEST_CASE("las cuatro acciones de turnos llegan a su destino con el buscador",
           turnos_buscador_las_cuatro_acciones) {
    EscenarioBuscadorTurnos escenario("turnos_buscador_las_cuatro_acciones");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // CANCELAR: la confirmacion.
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Busqueda");
    // EL PROMPT DEL ID DESAPARECIO: ahora hay un buscador.
    CHECK(pantalla.vistaTextoPlano().find("Id:") == std::string::npos);
    elegirTurnoEnElBuscador(pantalla, "31");
    CHECK(pantalla.vista() == "Confirmar");

    // VOLVER AL MENU (la confirmacion vuelve al buscador, y el buscador al
    // menu).
    pantalla.manejar(eventoTecla(ui::TipoTecla::Escape));
    pantalla.manejar(eventoTecla(ui::TipoTecla::Escape));
    CHECK(pantalla.vista() == "Menu");

    // REPROGRAMAR: el formulario de la fecha y la hora nuevas.
    pantalla.manejar(eventoCaracter('3'));
    elegirTurnoEnElBuscador(pantalla, "31");
    CHECK(pantalla.vista() == "Formulario");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Escape));

    // MODIFICAR: el formulario del consultorio y la duracion.
    pantalla.manejar(eventoCaracter('4'));
    elegirTurnoEnElBuscador(pantalla, "31");
    CHECK(pantalla.vista() == "Formulario");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Escape));

    // VER DETALLE: la ficha del turno.
    pantalla.manejar(eventoCaracter('7'));
    elegirTurnoEnElBuscador(pantalla, "31");
    CHECK(pantalla.vista() == "Detalle");
    CHECK(pantalla.vistaTextoPlano().find("Ficha de turno #31") != std::string::npos);
}

TEST_CASE("volver al buscador despues de no confirmar deja elegir otro turno",
           turnos_buscador_vuelve_al_buscador) {
    EscenarioBuscadorTurnos escenario("turnos_buscador_vuelve_al_buscador");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // Se elige el turno ACTIVO y se cancela la confirmacion con el "No".
    pantalla.manejar(eventoCaracter('2'));
    elegirTurnoEnElBuscador(pantalla, "31");
    CHECK(pantalla.vista() == "Confirmar");
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Busqueda");

    // EL RESALTADO SE MUEVE SIN VOLVER A LA CONFIRMACION
    //
    // El "No" deja el buscador como estaba: el usuario tiene que poder
    // seguir eligiendo un turno sin que la flecha le vuelva a abrir la
    // confirmacion del que ya habia elegido.
    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaAbajo));
    CHECK(pantalla.vista() == "Busqueda");
}

TEST_CASE("el raton mueve el resaltado del buscador de turnos y no confirma",
           turnos_buscador_raton) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioBuscadorTurnos escenario("turnos_buscador_raton");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    // "Ver detalle": la lista tiene los DOS turnos (el activo y el cancelado).
    pantalla.manejar(eventoCaracter('7'));

    // EL DIBUJO LE DICE AL BUSCADOR EN QUE FILA ARRANCA LA LISTA
    //
    // Sin esto, el hit-testing del raton no sabria que coincidencia se toco.
    const std::vector<std::string> lineas = renderizarLineas(pantalla.renderizar(), 120, 20);
    CHECK(lineas.size() == 20);
    if (lineas.size() == 20) {
        // EL TITULO, EL CAMPO Y LAS DOS COINCIDENCIAS.
        CHECK(lineas[2].find("Buscar turno") != std::string::npos);
        CHECK(lineas[3].find("Paciente, medico, especialidad") != std::string::npos);
        CHECK(lineas[4].find("1. 31 · ") != std::string::npos);
        CHECK(lineas[5].find("2. 32 · ") != std::string::npos);
    }

    // UN CLIC EN LA SEGUNDA COINCIDENCIA
    ui::Evento clic;
    clic.boton = ui::BotonMouse::Izquierdo;
    clic.x = 4;
    clic.y = 5;
    pantalla.manejar(clic);

    // EL CLIC NO ELIGE: la confirmacion es el Enter.
    CHECK(pantalla.vista() == "Busqueda");
    // Pero el resaltado se movio a la segunda coincidencia.
    CHECK(lineaDeLaOpcionElegida(pantalla.vistaTextoPlano()).find("32 · ")
          != std::string::npos);

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

TEST_CASE("un listado de turnos vacio dice que no hay y ofrece cargar el ejemplo",
           turnos_listar_vacio) {
    EscenarioTurnos escenario("turnos_listar_vacio");
    ui::Pantallas::PantallaTurnos& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('5'));
    CHECK(pantalla.vista() == "Listado");

    // EL ESTADO VACIO ES UN MENU DE DOS OPCIONES
    //
    // El texto plano conserva el formato del menu ("  N. Etiqueta").
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("No hay turnos") != std::string::npos);
    CHECK(texto.find("1. Cargar ejemplo") != std::string::npos);
    CHECK(texto.find("2. Volver") != std::string::npos);
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
    CHECK(pantalla.vista() == "Busqueda");
    elegirTurnoEnElBuscador(pantalla, "31");
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
    elegirTurnoEnElBuscador(pantalla, "31");
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
    elegirTurnoEnElBuscador(pantalla, "32");
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
    elegirTurnoEnElBuscador(pantalla, "31");
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
    elegirTurnoEnElBuscador(pantalla, "31");
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

// Elige una PERSONA en el buscador de los reportes.
//
// EL FLUJO SON DOS PASOS
//
// Se escribe el texto (el Enter BUSCA) y despues se elige la coincidencia
// (el Enter ELIGE la que esta resaltada). El texto es un dato que el usuario
// CONOCE (el nombre, el apellido, el dni, el legajo): el id no se escribe.
void elegirPersonaEnElBuscador(ui::Pantallas::PantallaReportes& pantalla,
                                const std::string& texto) {
    escribir(pantalla, texto);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // busca
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // elige
}

// La fila esperada de la tabla de ocupacion.
//
// El nombre del dia lo pone el DOMINIO (`Fecha::nombreDiaSemana`), asi que
// el test lo pide ahi tambien: el texto plano de la vista une las celdas con
// " | ".
std::string filaDeOcupacion(const std::string& fecha, int diaSemana, const std::string& turnos,
                             const std::string& horas, const std::string& porcentaje) {
    return fecha + " | " + Fecha::nombreDiaSemana(diaSemana) + " | " + turnos + " | " + horas
        + " | " + porcentaje;
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
    CHECK(texto.find("5. Ocupacion por semana") != std::string::npos);
    CHECK(texto.find("6. Pacientes y medicos dados de baja") != std::string::npos);
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

    // EL PACIENTE SE ELIGE CON EL BUSCADOR
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("Buscar paciente") != std::string::npos);
    elegirPersonaEnElBuscador(pantalla, "Ana");

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

    // EL MEDICO SE ELIGE CON EL BUSCADOR
    pantalla.manejar(eventoCaracter('3'));
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("Buscar medico") != std::string::npos);
    elegirPersonaEnElBuscador(pantalla, "Beto");

    CHECK(pantalla.vista() == "Resultado");
    CHECK(escenario.contexto.mensaje().texto() == "OK  2 turno(s).");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("2027-03-10") != std::string::npos);
    CHECK(texto.find("Ana Gomez") != std::string::npos);
    // EL MEDICO NO ES UNA COLUMNA: es la dimension por la que se filtra.
    CHECK(texto.find("Beto Perez") == std::string::npos);
}

TEST_CASE("el buscador de medicos de reportes no se filtra por especialidad",
           reportes_el_buscador_no_se_filtra) {
    EscenarioReportes escenario("reportes_el_buscador_no_se_filtra");
    // MEDICOS DE TRES ESPECIALIDADES: el buscador los muestra a TODOS.
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.medicos.alta(
        medicoConEspecialidad(2, "Caro", "Sosa", "LEG-1002", Especialidad::PEDI));
    escenario.medicos.alta(
        medicoConEspecialidad(3, "Dani", "Ruiz", "LEG-1003", Especialidad::GAST));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    // EL REPORTE DE TURNOS POR MEDICO
    pantalla.manejar(eventoCaracter('3'));
    CHECK(pantalla.vista() == "Busqueda");

    // CON EL TEXTO VACIO: SALEN LOS MEDICOS DE TODAS LAS ESPECIALIDADES
    //
    // El filtro por especialidad es del flujo de "agendar" (el de la
    // pantalla de turnos): los buscadores de las otras pantallas no se
    // filtran.
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Beto") != std::string::npos);
    CHECK(texto.find("Caro") != std::string::npos);
    CHECK(texto.find("Dani") != std::string::npos);

    // Y CON TEXTO TAMBIEN: la regla compartida no tiene filtro.
    escribir(pantalla, "Ruiz");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));  // busca
    CHECK(pantalla.vistaTextoPlano().find("Dani") != std::string::npos);
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

TEST_CASE("un texto que no coincide dice que no hay coincidencias",
           reportes_buscador_sin_coincidencias) {
    EscenarioReportes escenario("reportes_buscador_sin_coincidencias");
    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "Zzz");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // NO HAY COINCIDENCIAS: no se elige nada y el foco vuelve al texto.
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("no hay coincidencias") != std::string::npos);
}

TEST_CASE("la pantalla de reportes ya no pide el id de la persona",
           reportes_buscador_sin_prompt_de_id) {
    EscenarioReportes escenario("reportes_buscador_sin_prompt_de_id");
    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    // EL REPORTE POR PACIENTE
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Busqueda");
    std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Buscar paciente") != std::string::npos);
    CHECK(texto.find("Nombre, apellido o DNI:") != std::string::npos);
    CHECK(texto.find("Id del paciente") == std::string::npos);

    // EL REPORTE POR MEDICO
    pantalla.manejar(eventoTecla(ui::TipoTecla::Escape));
    pantalla.manejar(eventoCaracter('3'));
    CHECK(pantalla.vista() == "Busqueda");
    texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Buscar medico") != std::string::npos);
    CHECK(texto.find("Nombre, apellido, legajo o especialidad:") != std::string::npos);
    CHECK(texto.find("Id del medico") == std::string::npos);
}

TEST_CASE("en un campo de un reporte el cero es un dato si ya hay texto",
           reportes_cero_es_dato) {
    EscenarioReportes escenario("reportes_cero_es_dato");
    escenario.pacientes.alta(pacienteDePrueba(10, "Ana", "Gomez", "2020391"));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, "1");
    escribir(pantalla, "0");

    // El cero NO fue "volver": es parte del texto.
    CHECK(pantalla.vista() == "Busqueda");
    CHECK(pantalla.vistaTextoPlano().find("Nombre, apellido o DNI: 10|") != std::string::npos);
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
    elegirPersonaEnElBuscador(pantalla, "Valentina");
    CHECK(pantalla.vista() == "Resultado");
    CHECK(anchoNatural(pantalla.renderizar()) <= 80);

    // POR MEDICO (61 columnas con estos datos)
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('3'));
    elegirPersonaEnElBuscador(pantalla, "Diego");
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

// ---------------------------------------------------------------------------
// LA OCUPACION POR SEMANA
//
// QUE SE VERIFICA ACa
//
//   - que la semana sea la del LUNES al DOMINGO que contiene a la fecha;
//   - que el total de la semana sea la suma de los siete dias;
//   - que la ocupacion sea la UNION de los intervalos reservados (dos turnos
//     en paralelo ocupan una vez, no dos);
//   - que SOLO los turnos `Activo` ocupen la agenda;
//   - que el porcentaje sea sobre la ventana de atencion (08:00 a 22:00).
//
// LA ARITMETICA DE LAS FECHAS
//
// La semana se arma con el numero de dia (`Fecha::numeroDeDia()`) y su
// inversa, que tienen que usar la MISMA convencion de bisiestos. Los tests
// del final del archivo fijan esa ida y vuelta.
// ---------------------------------------------------------------------------
TEST_CASE("el reporte de ocupacion muestra los siete dias de la semana y el total",
           reportes_ocupacion_por_semana) {
    EscenarioReportes escenario("reportes_ocupacion_por_semana");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    // Un turno el LUNES y otro el MIERCOLES de la misma semana.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 8), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "A-2"));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('5'));
    CHECK(pantalla.vista() == "Campo");
    // Se escribe un dia de la semana: el MIERCOLES.
    escribir(pantalla, "2027-03-10");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Ocupacion");

    // LA SEMANA ES DEL LUNES AL DOMINGO
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find(filaDeOcupacion("2027-03-08", 0, "1", "1.0", "7.1%")) != std::string::npos);
    CHECK(texto.find(filaDeOcupacion("2027-03-09", 1, "0", "0.0", "0.0%")) != std::string::npos);
    CHECK(texto.find(filaDeOcupacion("2027-03-10", 2, "1", "1.0", "7.1%")) != std::string::npos);
    CHECK(texto.find(filaDeOcupacion("2027-03-11", 3, "0", "0.0", "0.0%")) != std::string::npos);
    CHECK(texto.find(filaDeOcupacion("2027-03-12", 4, "0", "0.0", "0.0%")) != std::string::npos);
    CHECK(texto.find(filaDeOcupacion("2027-03-13", 5, "0", "0.0", "0.0%")) != std::string::npos);
    CHECK(texto.find(filaDeOcupacion("2027-03-14", 6, "0", "0.0", "0.0%")) != std::string::npos);

    // Y LA FILA DEL TOTAL DE LA SEMANA
    //
    // Dos turnos, dos horas (60 + 60) y el 2.0% de la ventana de la semana
    // (120 minutos sobre 7 x 840).
    CHECK(texto.find("TOTAL |  | 2 | 2.0 | 2.0%") != std::string::npos);

    // EL MENSAJE DICE DE QUE SEMANA ES EL REPORTE
    CHECK(escenario.contexto.mensaje().texto()
          == "OK  Semana del 2027-03-08: 2 turno(s), 2.0 horas ocupadas (2.0%).");
}

TEST_CASE("la ocupacion cuenta una vez los turnos que se pisan",
           reportes_ocupacion_es_la_union) {
    EscenarioReportes escenario("reportes_ocupacion_es_la_union");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.medicos.alta(
        medicoConEspecialidad(2, "Caro", "Ruiz", "LEG-1002", Especialidad::CARD));
    // DOS MEDICOS A LA VEZ: de 08:00 a 09:00 y de 08:30 a 09:30.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 8), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(32, 2, 1, Especialidad::CARD, Fecha(2027, 3, 8), Hora(8, 30), 60, "A-2"));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('5'));
    escribir(pantalla, "2027-03-08");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Ocupacion");
    const std::string texto = pantalla.vistaTextoPlano();

    // LA UNION DE LOS DOS INTERVALOS ES DE 08:00 A 09:30
    //
    // Son 90 minutos ("1.5" horas), no 120 ("2.0"): la agenda se cuenta una
    // vez. Los DOS turnos se cuentan, pero el tiempo reservado se cuenta una
    // sola vez.
    CHECK(texto.find(filaDeOcupacion("2027-03-08", 0, "2", "1.5", "10.7%")) != std::string::npos);
}

TEST_CASE("un turno cancelado no ocupa la agenda", reportes_ocupacion_solo_activos) {
    EscenarioReportes escenario("reportes_ocupacion_solo_activos");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    // Un turno ACTIVO y uno CANCELADO (que libero su horario).
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 8), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 8), Hora(10, 0), 60, "A-2",
              EstadoTurno::Cancelado));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('5'));
    escribir(pantalla, "2027-03-08");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Ocupacion");
    const std::string texto = pantalla.vistaTextoPlano();

    // SOLO EL TURNO ACTIVO OCUPA: una hora, no dos.
    CHECK(texto.find(filaDeOcupacion("2027-03-08", 0, "1", "1.0", "7.1%")) != std::string::npos);
}

TEST_CASE("un turno reprogramado tampoco ocupa la agenda",
           reportes_ocupacion_solo_activos_reprogramado) {
    EscenarioReportes escenario("reportes_ocupacion_solo_activos_reprogramado");
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 8), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 8), Hora(10, 0), 60, "A-2",
              EstadoTurno::Reprogramado));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('5'));
    escribir(pantalla, "2027-03-08");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Ocupacion");
    const std::string texto = pantalla.vistaTextoPlano();

    CHECK(texto.find(filaDeOcupacion("2027-03-08", 0, "1", "1.0", "7.1%")) != std::string::npos);
}

// ---------------------------------------------------------------------------
// LA ARITMETICA DE LAS FECHAS
//
// LA SEMANA SE ARMA CON EL NUMERO DE DIA Y SU INVERSA
//
// `Fecha::numeroDeDia()` es el numero de dias desde el 2000-01-01, contando
// los anos bisiestos. Para volver de un numero de dia a una fecha hay que
// hacer el camino de vuelta CON LA MISMA CONVENCION: si las dos contaran
// distinto, un dia seria de un largo y del otro.
//
// Estos tests fijan las dos puntas: la ida y vuelta, y el lunes que se
// espera de una semana conocida.
// ---------------------------------------------------------------------------
TEST_CASE("el numero de dia y su inversa dan la misma fecha", semana_ida_y_vuelta) {
    // Fechas queibo los bisiestos: el 2000 (divisible por 400) y el 2024
    // (divisible por 4), y una que NO es bisiesto (el 2100, divisible por
    // 100 pero no por 400).
    const Fecha fechas[] = {Fecha(2000, 1, 1),  Fecha(2000, 2, 29), Fecha(2000, 3, 1),
                            Fecha(2024, 2, 29), Fecha(2027, 3, 10), Fecha(2100, 12, 31)};
    for (const Fecha& fecha : fechas) {
        CHECK(ui::fechaDesdeNumeroDeDia(fecha.numeroDeDia()) == fecha);
    }

    // Y AL REVES: el numero de dia de una fecha que se conoce.
    CHECK(Fecha(2000, 1, 1).numeroDeDia() == 0);
    CHECK(Fecha(2000, 1, 2).numeroDeDia() == 1);
    // El 2000 ES bisiesto: febrero tiene 29 dias.
    CHECK(Fecha(2000, 3, 1).numeroDeDia() == 60);
    CHECK(Fecha(2000, 12, 31).numeroDeDia() == 365);
    // El 2001 NO es bisiesto: el 2000 sumo un dia al pasar el ano.
    CHECK(Fecha(2001, 1, 1).numeroDeDia() == 366);
    CHECK(Fecha(2001, 3, 1).numeroDeDia() == 425);
}

TEST_CASE("la semana de una fecha arranca en el lunes que se espera",
           semana_arranca_en_el_lunes) {    // EL 2027-03-10 ES UN MIERCOLES
    //
    // La convencion del dominio es 0 = lunes, 1 = martes, 2 = miercoles.
    CHECK(Fecha(2027, 3, 10).diaSemana() == 2);
    CHECK(Fecha(2027, 3, 8).diaSemana() == 0);

    const std::vector<Fecha> dias = ui::diasDeLaSemana(Fecha(2027, 3, 10));
    CHECK(dias.size() == 7);
    if (dias.size() == 7) {
        CHECK(dias[0] == Fecha(2027, 3, 8));   // el lunes
        CHECK(dias[1] == Fecha(2027, 3, 9));   // el martes
        CHECK(dias[2] == Fecha(2027, 3, 10));  // el miercoles
        CHECK(dias[6] == Fecha(2027, 3, 14));  // el domingo
    }

    // Y LA SEMANA DE UN LUNES ES ESA MISMA SEMANA
    const std::vector<Fecha> propia = ui::diasDeLaSemana(Fecha(2027, 3, 8));
    CHECK(propia.size() == 7);
    if (propia.size() == 7) {
        CHECK(propia.front() == Fecha(2027, 3, 8));
        CHECK(propia.back() == Fecha(2027, 3, 14));
    }

    // Y LA DE UN DOMINGO TAMBIEN
    const std::vector<Fecha> domingo = ui::diasDeLaSemana(Fecha(2027, 3, 14));
    CHECK(domingo.size() == 7);
    if (domingo.size() == 7) {
        CHECK(domingo.front() == Fecha(2027, 3, 8));
        CHECK(domingo.back() == Fecha(2027, 3, 14));
    }
}

// ---------------------------------------------------------------------------
// LOS PACIENTES Y MEDICOS DADOS DE BAJA
//
// QUE SE VERIFICA ACa
//
//   - que el reporte muestre las DOS tablas, con las columnas de las
//     pantallas de entidades;
//   - que salgan SOLO los dados de baja (los activos no);
//   - que el mensaje diga los DOS totales;
//   - el estado vacio, con su salida.
//
// LAS COLUMNAS SON LAS DE LAS PANTALLAS
//
// El reporte usa la misma configuracion que ellas (`configDePacientes` y
// `configDeMedicos`), asi que no hay dos listas de columnas que se puedan
// desincronizar.
// ---------------------------------------------------------------------------
TEST_CASE("el reporte de dados de baja muestra las dos tablas y los dos totales",
           reportes_dados_de_baja) {
    EscenarioReportes escenario("reportes_dados_de_baja");

    // Un paciente activo y uno DADO DE BAJA.
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    Paciente inactivo = pacienteDePrueba(2, "Valentina", "Ibarra", "2020392");
    inactivo.setActivo(false);
    escenario.pacientes.alta(inactivo);

    // Un medico activo y uno DADO DE BAJA.
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    Medico medicoInactivo = Medico(2, "Diego", "Fernandez", "diego.fernandez@unlp.edu.ar",
                                    "2214556688", "LEG-1002", Especialidad::PEDI);
    medicoInactivo.setActivo(false);
    escenario.medicos.alta(medicoInactivo);

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('6'));
    CHECK(pantalla.vista() == "Bajas");

    // EL MENSAJE DICE LOS DOS TOTALES
    CHECK(escenario.contexto.mensaje().texto()
          == "OK  1 paciente(s) inactivos. 1 medico(s) inactivos.");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Pacientes dados de baja") != std::string::npos);
    CHECK(texto.find("Medicos dados de baja") != std::string::npos);

    // LOS DADOS DE BAJA SALEN
    //
    // Las tablas de las entidades tienen el nombre y el apellido en columnas
    // separadas (son las de las pantallas de pacientes y de medicos).
    CHECK(texto.find("Valentina | Ibarra") != std::string::npos);
    CHECK(texto.find("Diego | Fernandez") != std::string::npos);
    CHECK(texto.find("inactivo") != std::string::npos);
    // Y LOS ACTIVOS NO
    CHECK(texto.find("Ana | Gomez") == std::string::npos);
    CHECK(texto.find("Beto | Perez") == std::string::npos);
}

TEST_CASE("un reporte de dados de baja sin dados de baja dice que no hay",
           reportes_dados_de_baja_vacio) {
    EscenarioReportes escenario("reportes_dados_de_baja_vacio");
    // SOLO ENTIDADES ACTIVAS: no hay ningun dado de baja.
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('6'));
    CHECK(pantalla.vista() == "Bajas");

    CHECK(escenario.contexto.mensaje().texto()
          == "OK  0 paciente(s) inactivos. 0 medico(s) inactivos.");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("No hay pacientes inactivos") != std::string::npos);
    CHECK(texto.find("No hay medicos inactivos") != std::string::npos);
    // Y CADA ESTADO VACIO DICE COMO VOLVER
    CHECK(texto.find("0: volver al menu") != std::string::npos);
}

// ---------------------------------------------------------------------------
// EL RENDER DE LA PANTALLA DE REPORTES
//
// QUE SE VERIFICA ACa
//
// Que cada vista de la pantalla se pueda DIBUJAR en una pantalla de FTXUI.
//
// Son seis vistas y el caso las recorre todas: el menu, el campo, el
// selector, el resultado de un reporte, la ocupacion y los dados de baja.
// (La tabla de los dados de baja es mas ancha que 80 columnas: es la de las
// pantallas de entidades, y por eso se dibuja en una pantalla mas ancha.)
// ---------------------------------------------------------------------------
TEST_CASE("el render de la pantalla de reportes se puede dibujar en FTXUI",
           reportes_render) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioReportes escenario("reportes_render");
    escenario.pacientes.alta(pacienteDePrueba(1, "Valentina", "Ibarra", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Diego", "Fernandez", "LEG-1001", Especialidad::CARD));
    Medico inactivo = Medico(2, "Caro", "Ruiz", "caro.ruiz@unlp.edu.ar", "2214556677", "LEG-1002",
                              Especialidad::PEDI);
    inactivo.setActivo(false);
    escenario.medicos.alta(inactivo);
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 8), Hora(8, 0), 60, "A-1"));

    ui::Pantallas::PantallaReportes& pantalla = *escenario.puntero;

    // EL MENU DEL MODULO
    std::vector<std::string> lineas = renderizarLineas(pantalla.renderizar(), 80, 14);
    CHECK(lineas.size() == 14);
    if (lineas.size() == 14) {
        CHECK(lineas[0].find("REPORTES") != std::string::npos);
        CHECK(lineas[2].find("1. Turnos por fecha") != std::string::npos);
        CHECK(lineas[7].find("6. Pacientes y medicos dados de baja") != std::string::npos);
        CHECK(lineas[8].find("0. Volver") != std::string::npos);
    }

    // EL CAMPO DE LA FECHA
    pantalla.manejar(eventoCaracter('1'));
    lineas = renderizarLineas(pantalla.renderizar(), 80, 8);
    CHECK(lineas.size() == 8);
    if (lineas.size() == 8) {
        CHECK(lineas[2].find("Fecha:") != std::string::npos);
    }

    // EL SELECTOR DE LA ESPECIALIDAD
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('4'));
    lineas = renderizarLineas(pantalla.renderizar(), 80, 20);
    CHECK(lineas.size() == 20);
    if (lineas.size() == 20) {
        CHECK(lineas[2].find("especialidad") != std::string::npos);
        CHECK(lineas[4].find("1. Cardiolog") != std::string::npos);
        CHECK(lineas[15].find("12. Neumolog") != std::string::npos);
    }

    // EL RESULTADO DE UN REPORTE
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    lineas = renderizarLineas(pantalla.renderizar(), 80, 12);
    CHECK(lineas.size() == 12);
    if (lineas.size() == 12) {
        CHECK(lineas[3].find("paciente") != std::string::npos);
        CHECK(lineas[3].find("medico") != std::string::npos);
    }

    // LA OCUPACION DE LA SEMANA
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('5'));
    escribir(pantalla, "2027-03-08");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    lineas = renderizarLineas(pantalla.renderizar(), 80, 16);
    CHECK(lineas.size() == 16);
    if (lineas.size() == 16) {
        CHECK(lineas[3].find("horas ocupadas") != std::string::npos);
        CHECK(lineas[3].find("% ocupacion") != std::string::npos);
    }

    // LOS DADOS DE BAJA
    //
    // La tabla de los medicos es mas ancha que 80 columnas (son las
    // columnas de la pantalla de medicos): se dibuja en una pantalla ancha.
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('6'));
    lineas = renderizarLineas(pantalla.renderizar(), 200, 16);
    CHECK(lineas.size() == 16);
    if (lineas.size() == 16) {
        CHECK(lineas[2].find("Pacientes dados de baja") != std::string::npos);
        CHECK(lineas[4].find("Medicos dados de baja") != std::string::npos);
        CHECK(lineas[8].find("Caro") != std::string::npos);
    }

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

// ---------------------------------------------------------------------------
// LA PANTALLA DE CALENDARIO
//
// QUE SE VERIFICA ACa
//
//   - el menu del modulo y su atajo de volver;
//   - la grilla del mes: los dias de la semana, los dias del mes, los huecos
//     y la cantidad de turnos de cada dia;
//   - el MAPA DE CALOR: los umbrales, el color de cada celda y la leyenda
//     (con colores y con `NO_COLOR`);
//   - el recorrido con las flechas (el mes anterior y el siguiente, con el
//     anio dando la vuelta);
//   - la RED DE LAS 80 COLUMNAS de la grilla.
//
// LA FECHA DE HOY SE INYECTA
//
// La pantalla recibe la fecha de HOY por parametro: los tests le pasan una
// FIJA, asi que ningun test depende del dia en que se corre.
//
// LOS NOMBRES DE LOS DIAS LOS TIENE EL DOMINIO
//
// El test no escribe "miercoles" (con tilde): pide el nombre al dominio
// (`Fecha::nombreDiaSemana`), que es de donde sale.
// ---------------------------------------------------------------------------
namespace {

// El escenario de las pruebas de la pantalla de calendario.
//
// Es el mismo esquema que el de reportes: los repositorios de verdad (en un
// directorio temporal), el servicio de verdad y la pantalla en la pila de la
// aplicacion, con una pantalla de prueba DEBAJO.
struct EscenarioCalendario {
    EscenarioCalendario(const std::string& nombre, const Fecha& hoy)
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

        auto pantalla = std::make_unique<ui::Pantallas::PantallaCalendario>(
            aplicacion, pacientes, medicos, servicio, hoy);
        puntero = pantalla.get();
        aplicacion.ir([&pantalla](ui::Aplicacion&) { return std::move(pantalla); });
    }

    RepositorioPacientes pacientes;
    RepositorioMedicos medicos;
    RepositorioTurnos turnos;
    ServicioTurnos servicio;
    ui::Contexto contexto;
    ui::Aplicacion aplicacion;
    ui::Pantallas::PantallaCalendario* puntero = nullptr;
};

// Escribe un texto en la pantalla de calendario (un evento por caracter).
void escribir(ui::Pantallas::PantallaCalendario& pantalla, const std::string& texto) {
    for (const char caracter : texto) {
        pantalla.manejar(eventoCaracter(caracter));
    }
}

// ABRE LA GRILLA DEL MES
//
// El mes se pide en dos pasos: el anio y el mes. La funcion hace los dos.
void abrirMes(ui::Pantallas::PantallaCalendario& pantalla, const std::string& anio,
               const std::string& mes) {
    pantalla.manejar(eventoCaracter('1'));
    escribir(pantalla, anio);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
    escribir(pantalla, mes);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
}

// ABRE EL DETALLE DE UN DIA
void abrirDetalle(ui::Pantallas::PantallaCalendario& pantalla, const std::string& dia) {
    pantalla.manejar(eventoCaracter('2'));
    escribir(pantalla, dia);
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));
}

// LAS CELDAS DE UNA FILA DE LA GRILLA
//
// La fila del texto plano de la grilla es "|    1    |    2    |...": se
// parte por el separador vertical y se saca el texto de cada celda (con los
// espacios que la centran).
std::vector<std::string> celdasDeLaFila(const std::string& fila) {
    std::vector<std::string> celdas;

    std::size_t inicio = fila.find('|');
    while (inicio != std::string::npos) {
        const std::size_t fin = fila.find('|', inicio + 1);
        if (fin == std::string::npos) {
            break;
        }
        celdas.push_back(fila.substr(inicio + 1, fin - inicio - 1));
        inicio = fin;
    }

    return celdas;
}

// LA CELDA SIN LOS ESPACIOS QUE LA CENTRAN
std::string celdaSinEspacios(const std::string& celda) {
    const std::size_t inicio = celda.find_first_not_of(' ');
    if (inicio == std::string::npos) {
        return "";
    }
    const std::size_t fin = celda.find_last_not_of(' ');
    return celda.substr(inicio, fin - inicio + 1);
}

} // namespace

TEST_CASE("el menu de calendario muestra las tres acciones y el cero de volver",
           calendario_menu_inicial) {
    EscenarioCalendario escenario("calendario_menu_inicial", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    CHECK(pantalla.vista() == "Menu");
    CHECK(pantalla.titulo() == "CALENDARIO");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("1. Ver mes") != std::string::npos);
    CHECK(texto.find("2. Ver detalle del dia") != std::string::npos);
    CHECK(texto.find("3. Agenda") != std::string::npos);
    CHECK(texto.find("0. Volver") != std::string::npos);
}

TEST_CASE("el cero del menu de calendario vuelve a la pantalla anterior",
           calendario_menu_cero_vuelve) {
    EscenarioCalendario escenario("calendario_menu_cero_vuelve", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    CHECK(escenario.aplicacion.textoPlanoActual() != "pantalla de prueba");

    pantalla.manejar(eventoCaracter('0'));

    // La pantalla salio de la pila: la pantalla anterior quedo arriba.
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");
}

TEST_CASE("la grilla del mes muestra los dias de la semana y los turnos de cada dia",
           calendario_grilla_muestra_el_mes) {
    EscenarioCalendario escenario("calendario_grilla_muestra_el_mes", Fecha(2027, 3, 10));
    // DOS TURNOS EL DIEZ DE MARZO Y UNO EL VEINTE.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 30, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(9, 0), 30, "A-1"));
    escenario.turnos.alta(
        Turno(33, 1, 1, Especialidad::CARD, Fecha(2027, 3, 20), Hora(8, 0), 30, "A-1"));

    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;
    abrirMes(pantalla, "2027", "3");

    CHECK(pantalla.vista() == "Grilla");
    CHECK(pantalla.anio() == 2027);
    CHECK(pantalla.mes() == 3);

    const std::string texto = pantalla.vistaTextoPlano();

    // LA CABECERA: LOS SIETE DIAS DE LA SEMANA
    //
    // Los nombres los tiene el dominio: el test los pide ahi.
    CHECK(texto.find(Fecha::nombreDiaSemana(0)) != std::string::npos);
    CHECK(texto.find(Fecha::nombreDiaSemana(2)) != std::string::npos);
    CHECK(texto.find(Fecha::nombreDiaSemana(6)) != std::string::npos);

    // EL MES QUE SE ESTA VIENDO
    CHECK(texto.find("2027-03") != std::string::npos);

    // LA CANTIDAD DE TURNOS DE CADA DIA
    //
    // El dia 10 tiene dos turnos y el dia 20 tiene uno: los numeros van
    // entre parentesis, debajo del numero del dia.
    CHECK(texto.find("(2)") != std::string::npos);
    CHECK(texto.find("(1)") != std::string::npos);
    CHECK(texto.find("(0)") != std::string::npos);
}

TEST_CASE("la grilla pone los dias en la columna de su dia de la semana",
           calendario_grilla_las_columnas) {
    EscenarioCalendario escenario("calendario_grilla_las_columnas", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;
    abrirMes(pantalla, "2027", "3");

    // LA PRIMERA SEMANA: DEL LUNES 1 AL DOMINGO 7
    //
    // El 1 de marzo del 2027 es un LUNES: la primera celda de la grilla es
    // el dia 1 y no hay huecos al principio.
    const std::string primera = lineaQueEmpiezaCon(pantalla.vistaTextoPlano(), "  |");
    const std::vector<std::string> celdas = celdasDeLaFila(primera);
    CHECK(celdas.size() == 7);
    if (celdas.size() == 7) {
        CHECK(celdaSinEspacios(celdas[0]) == "1");
        CHECK(celdaSinEspacios(celdas[6]) == "7");
    }

    // Y LA ULTIMA SEMANA: DEL LUNES 29 AL MIERCOLES 31, CON CUATRO HUECOS
    //
    // Los huecos son las celdas de los dias que NO son del mes: existen (la
    // grilla es un rectangulo) pero no dicen nada.
    const std::string ultima = lineaQueEmpiezaCon(pantalla.vistaTextoPlano(), "  |   29");
    const std::vector<std::string> celdasUltima = celdasDeLaFila(ultima);
    CHECK(celdasUltima.size() == 7);
    if (celdasUltima.size() == 7) {
        CHECK(celdaSinEspacios(celdasUltima[0]) == "29");
        CHECK(celdaSinEspacios(celdasUltima[2]) == "31");
        CHECK(celdaSinEspacios(celdasUltima[3]) == "");
        CHECK(celdaSinEspacios(celdasUltima[6]) == "");
    }
}

TEST_CASE("la grilla del mes se recorre con las flechas", calendario_grilla_las_flechas) {
    EscenarioCalendario escenario("calendario_grilla_las_flechas", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;
    abrirMes(pantalla, "2027", "3");

    CHECK(pantalla.anio() == 2027);
    CHECK(pantalla.mes() == 3);

    // LA FLECHA DERECHA: EL MES SIGUIENTE
    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaDerecha));
    CHECK(pantalla.anio() == 2027);
    CHECK(pantalla.mes() == 4);
    CHECK(pantalla.vistaTextoPlano().find("2027-04") != std::string::npos);

    // LA FLECHA IZQUIERDA: EL MES ANTERIOR
    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    CHECK(pantalla.anio() == 2027);
    CHECK(pantalla.mes() == 2);

    // EL ANIO DA LA VUELTA
    //
    // De diciembre a enero del ano siguiente, y de enero a diciembre del
    // anterior.
    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    CHECK(pantalla.anio() == 2026);
    CHECK(pantalla.mes() == 12);

    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaDerecha));
    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaDerecha));
    CHECK(pantalla.anio() == 2027);
    CHECK(pantalla.mes() == 2);
}

TEST_CASE("el cero y el escape de la grilla vuelven al menu del modulo",
           calendario_grilla_vuelve) {
    EscenarioCalendario escenario("calendario_grilla_vuelve", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;
    abrirMes(pantalla, "2027", "3");

    pantalla.manejar(eventoCaracter('0'));
    CHECK(pantalla.vista() == "Menu");

    abrirMes(pantalla, "2027", "3");
    CHECK(pantalla.manejar(eventoTecla(ui::TipoTecla::Escape)) == true);
    CHECK(pantalla.vista() == "Menu");

    // Y LA PANTALLA SIGUE EN LA PILA (el `Escape` del menu NO lo consume).
    CHECK(escenario.aplicacion.textoPlanoActual() != "pantalla de prueba");
    CHECK(pantalla.manejar(eventoTecla(ui::TipoTecla::Escape)) == false);
}

TEST_CASE("un anio que no existe muestra el error debajo del campo",
           calendario_anio_invalido) {
    EscenarioCalendario escenario("calendario_anio_invalido", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    CHECK(pantalla.vista() == "Anio");

    escribir(pantalla, "1800");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // LA VISTA NO CAMBIA: el usuario corrige el anio.
    CHECK(pantalla.vista() == "Anio");
    CHECK(pantalla.vistaTextoPlano().find("el anio tiene que ser un numero entre 2000 y 2100")
          != std::string::npos);
}

TEST_CASE("en el campo del anio el cero es un dato si ya hay texto",
           calendario_cero_es_dato) {
    EscenarioCalendario escenario("calendario_cero_es_dato", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    escribir(pantalla, "2");
    escribir(pantalla, "0");

    // El cero NO fue "volver": es parte del anio.
    CHECK(pantalla.vista() == "Anio");
    CHECK(pantalla.vistaTextoPlano().find("Anio: 20|") != std::string::npos);
}

TEST_CASE("el cero de un campo vacio del calendario vuelve al menu", calendario_cero_vuelve) {
    EscenarioCalendario escenario("calendario_cero_vuelve", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('1'));
    CHECK(pantalla.vista() == "Anio");

    pantalla.manejar(eventoCaracter('0'));
    CHECK(pantalla.vista() == "Menu");
}

// ---------------------------------------------------------------------------
// EL MAPA DE CALOR
//
// QUE SE VERIFICA ACa
//
//   - los UMBRALES: la cantidad de turnos decide el nivel, y los niveles
//     tienen umbrales ABSOLUTOS (1-2, 3-4, 5-6, 7 o mas);
//   - el COLOR de la celda: es el del nivel del dia;
//   - la LEYENDA: con colores muestra las muestras, y sin colores es de
//     texto (los rangos y lo que significan).
// ---------------------------------------------------------------------------
TEST_CASE("los umbrales del mapa de calor son absolutos", calendario_mapa_calor_umbrales) {
    // EL "0 TURNOS" NO TIENE COLOR DE CALOR
    CHECK(ui::nivelDeCalor(0) == ui::NivelCalor::Ninguno);
    CHECK(ui::nivelDeCalor(-1) == ui::NivelCalor::Ninguno);

    // 1-2: BAJO
    CHECK(ui::nivelDeCalor(1) == ui::NivelCalor::Bajo);
    CHECK(ui::nivelDeCalor(2) == ui::NivelCalor::Bajo);

    // 3-4: MEDIO
    CHECK(ui::nivelDeCalor(3) == ui::NivelCalor::Medio);
    CHECK(ui::nivelDeCalor(4) == ui::NivelCalor::Medio);

    // 5-6: ALTO
    CHECK(ui::nivelDeCalor(5) == ui::NivelCalor::Alto);
    CHECK(ui::nivelDeCalor(6) == ui::NivelCalor::Alto);

    // 7 O MAS: MAXIMO
    CHECK(ui::nivelDeCalor(7) == ui::NivelCalor::Maximo);
    CHECK(ui::nivelDeCalor(20) == ui::NivelCalor::Maximo);

    // Y LOS RANGOS DE LA LEYENDA
    CHECK(ui::rangoDeCalor(ui::NivelCalor::Bajo) == "1-2");
    CHECK(ui::rangoDeCalor(ui::NivelCalor::Medio) == "3-4");
    CHECK(ui::rangoDeCalor(ui::NivelCalor::Alto) == "5-6");
    CHECK(ui::rangoDeCalor(ui::NivelCalor::Maximo) == "7+");
}

TEST_CASE("la celda de un dia se pinta con el color de su nivel de calor",
           calendario_mapa_calor_color) {
    borrarNoColor();
    ui::Theme::recargarEntorno();

    EscenarioCalendario escenario("calendario_mapa_calor_color", Fecha(2027, 3, 10));

    // SIETE TURNOS EL PRIMERO DEL MES: EL NIVEL MAXIMO.
    //
    // El 1 de marzo del 2027 es un LUNES, asi que el dia 1 es la primera
    // celda de la grilla (columna 0, primera semana).
    for (int i = 0; i < 7; ++i) {
        escenario.turnos.alta(Turno(30 + i, 1, 1, Especialidad::CARD, Fecha(2027, 3, 1),
                                    Hora(8, 0), 30, "A-1"));
    }

    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;
    abrirMes(pantalla, "2027", "3");

    ftxui::Screen lienzo =
        ftxui::Screen::Create(ftxui::Dimension::Fixed(80), ftxui::Dimension::Fixed(30));
    ftxui::Render(lienzo, pantalla.renderizar());

    // LA POSICION DEL NUMERO DEL DIA
    //
    // La grilla arranca en la linea 5 (despues del encabezado, del
    // separador, del titulo del mes, de la cabecera de los dias y del borde
    // de arriba) y la columna 0 arranca en la posicion 3 (la sangria y el
    // separador vertical). El numero esta CENTRADO en la celda.
    const ui::Theme tema;
    CHECK(lienzo.PixelAt(7, 5).foreground_color == ui::aFtxui(tema.calorMaximo()));

    // Y UN DIA SIN TURNOS NO TIENE COLOR DE CALOR
    //
    // El dia 2 (la segunda celda de la primera semana) no tiene turnos: se
    // pinta con el color normal de una celda.
    CHECK(lienzo.PixelAt(17, 5).foreground_color == ui::aFtxui(tema.tablaCelda()));
}

TEST_CASE("la leyenda del mapa de calor muestra las muestras cuando hay colores",
           calendario_leyenda_con_color) {
    borrarNoColor();
    ui::Theme::recargarEntorno();

    EscenarioCalendario escenario("calendario_leyenda_con_color", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;
    abrirMes(pantalla, "2027", "3");

    const std::string texto = pantalla.vistaTextoPlano();

    // LA MUESTRA DE CADA NIVEL
    CHECK(texto.find("\u2588\u2588\u2588 1-2") != std::string::npos);
    CHECK(texto.find("\u2588\u2588\u2588 3-4") != std::string::npos);
    CHECK(texto.find("\u2588\u2588\u2588 5-6") != std::string::npos);
    CHECK(texto.find("\u2588\u2588\u2588 7+") != std::string::npos);
    // Y EL CASO SIN TURNOS
    CHECK(texto.find("0 = sin turnos") != std::string::npos);
}

TEST_CASE("sin NO_COLOR la leyenda del mapa de calor es de texto",
           calendario_leyenda_sin_color) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioCalendario escenario("calendario_leyenda_sin_color", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;
    abrirMes(pantalla, "2027", "3");

    const std::string texto = pantalla.vistaTextoPlano();

    // LOS RANGOS Y LO QUE SIGNIFICAN
    CHECK(texto.find("1-2 = baja") != std::string::npos);
    CHECK(texto.find("3-4 = media") != std::string::npos);
    CHECK(texto.find("5-6 = alta") != std::string::npos);
    CHECK(texto.find("7+ = maxima") != std::string::npos);
    CHECK(texto.find("0 = sin turnos") != std::string::npos);

    // Y NO HAY MUESTRAS DE COLOR
    CHECK(texto.find("\u2588\u2588\u2588") == std::string::npos);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

// ---------------------------------------------------------------------------
// LA RED DE LAS 80 COLUMNAS
//
// LA GRILLA TIENE QUE ENTRAR EN UNA TERMINAL DE 80 COLUMNAS
//
// La grilla no scrollea ni trunca: si es mas ancha que la terminal, la
// parte de la derecha no se ve. Eso hay que VERIFICARLO, no suponerlo.
// ---------------------------------------------------------------------------
TEST_CASE("la grilla del mes entra en 80 columnas", calendario_grilla_entra_en_80) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioCalendario escenario("calendario_grilla_entra_en_80", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;
    abrirMes(pantalla, "2027", "3");

    CHECK(anchoNatural(pantalla.renderizar()) <= 80);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

TEST_CASE("el menu principal ofrece la opcion de calendario", menu_principal_calendario) {
    EscenarioCalendario escenario("menu_principal_calendario", Fecha(2027, 3, 10));
    ui::Pantallas::MenuPrincipal menu(escenario.aplicacion);

    const std::string texto = menu.textoPlano();
    CHECK(texto.find("1. Pacientes") != std::string::npos);
    CHECK(texto.find("2. Medicos") != std::string::npos);
    CHECK(texto.find("3. Turnos") != std::string::npos);
    CHECK(texto.find("4. Reportes") != std::string::npos);
    CHECK(texto.find("5. Calendario") != std::string::npos);
    CHECK(texto.find("0. Salir") != std::string::npos);

    // Y EL ATAJO ABRE LA PANTALLA DE CALENDARIO.
    menu.manejar(eventoCaracter('5'));
    CHECK(escenario.aplicacion.textoPlanoActual().find("CALENDARIO") != std::string::npos);
}

// ---------------------------------------------------------------------------
// EL DETALLE DEL DIA Y LA AGENDA
//
// QUE SE VERIFICA ACa
//
//   - la tabla del detalle del dia (las seis columnas de la lista de turnos);
//   - que el detalle se vea en el MES QUE SE ESTA VIENDO;
//   - la agenda: la hora de inicio, la de fin (DERIVADA de la duracion), la
//     duracion, la especialidad, el consultorio y el estado, en orden
//     CRONOLOGICO;
//   - que la agenda arranque en el dia de HOY que recibe la pantalla y que
//     las flechas lo cambien;
//   - el estado vacio de las dos, con su salida;
//   - la RED DE LAS 80 COLUMNAS de las dos tablas.
// ---------------------------------------------------------------------------
TEST_CASE("el detalle del dia muestra la tabla de los turnos de ese dia",
           calendario_detalle_del_dia) {
    EscenarioCalendario escenario("calendario_detalle_del_dia", Fecha(2027, 3, 10));
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    // UN TURNO EL DIEZ Y OTRO EL ONCE
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(8, 0), 60, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::CARD, Fecha(2027, 3, 11), Hora(9, 0), 60, "A-2"));

    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Dia");
    escribir(pantalla, "10");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    CHECK(pantalla.vista() == "Detalle");
    CHECK(escenario.contexto.mensaje().texto() == "OK  1 turno(s) el 2027-03-10.");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("31") != std::string::npos);
    CHECK(texto.find("2027-03-10") != std::string::npos);
    CHECK(texto.find("08:00") != std::string::npos);
    CHECK(texto.find("Ana Gomez") != std::string::npos);
    CHECK(texto.find("Beto Perez") != std::string::npos);
    CHECK(texto.find("Activo") != std::string::npos);

    // EL TURNO DEL OTRO DIA NO ESTA: el detalle es de ESE dia.
    CHECK(texto.find("32") == std::string::npos);
}

TEST_CASE("el detalle del dia se ve en el mes que se esta viendo",
           calendario_detalle_en_el_mes_que_se_esta_viendo) {
    EscenarioCalendario escenario("calendario_detalle_en_el_mes_que_se_esta_viendo",
                                  Fecha(2027, 3, 10));
    // UN TURNO EN ABRIL
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 4, 5), Hora(8, 0), 60, "A-1"));

    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    // SE RECORRE EL MES HASTA ABRIL
    abrirMes(pantalla, "2027", "3");
    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaDerecha));
    CHECK(pantalla.mes() == 4);

    // Y EL DETALLE MUESTRA EL DIA DE ABRIL (no el de marzo)
    pantalla.manejar(eventoCaracter('0'));
    CHECK(pantalla.vista() == "Menu");
    abrirDetalle(pantalla, "5");

    CHECK(pantalla.vista() == "Detalle");
    CHECK(escenario.contexto.mensaje().texto() == "OK  1 turno(s) el 2027-04-05.");
}

TEST_CASE("un dia sin turnos dice que no hay y como volver", calendario_detalle_sin_turnos) {
    EscenarioCalendario escenario("calendario_detalle_sin_turnos", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    abrirDetalle(pantalla, "10");

    CHECK(pantalla.vista() == "Detalle");
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("No hay turnos ese dia") != std::string::npos);
    CHECK(texto.find("0: volver al menu") != std::string::npos);
}

TEST_CASE("un dia que no existe en el mes muestra el error", calendario_dia_invalido) {
    EscenarioCalendario escenario("calendario_dia_invalido", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    // EL MES DE FEBRERO DEL 2027 TIENE 28 DIAS
    abrirMes(pantalla, "2027", "2");
    // (desde la grilla hay que volver al menu del modulo)
    pantalla.manejar(eventoCaracter('0'));
    CHECK(pantalla.vista() == "Menu");
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Dia");
    escribir(pantalla, "31");
    pantalla.manejar(eventoTecla(ui::TipoTecla::Enter));

    // LA VISTA NO CAMBIA: el usuario corrige el dia.
    CHECK(pantalla.vista() == "Dia");
    CHECK(pantalla.vistaTextoPlano().find("el dia tiene que ser un numero entre 1 y 28")
          != std::string::npos);
}

TEST_CASE("la agenda muestra el dia en orden cronologico", calendario_agenda) {
    EscenarioCalendario escenario("calendario_agenda", Fecha(2027, 3, 10));
    escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));
    // DOS TURNOS DEL DIA: UNO A LAS DIEZ Y OTRO A LAS OCHO
    //
    // El de las diez tiene el id MENOR: si la agenda no los ordenara por
    // hora, el orden seria el del id.
    escenario.turnos.alta(
        Turno(31, 1, 1, Especialidad::CARD, Fecha(2027, 3, 10), Hora(10, 0), 30, "A-1"));
    escenario.turnos.alta(
        Turno(32, 1, 1, Especialidad::PEDI, Fecha(2027, 3, 10), Hora(8, 0), 45, "Consultorio 2"));

    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('3'));
    CHECK(pantalla.vista() == "Agenda");
    CHECK(escenario.contexto.mensaje().texto() == "OK  2 turno(s) el 2027-03-10.");

    const std::string texto = pantalla.vistaTextoPlano();

    // LAS COLUMNAS DE LA AGENDA
    //
    // La hora de INICIO, la de FIN (derivada de la duracion), la duracion,
    // la especialidad, el consultorio y el estado.
    CHECK(texto.find("08:00 | 08:45 | 45 min | " + descripcionDe(Especialidad::PEDI)
                     + " | Consultorio 2 | Activo")
          != std::string::npos);
    CHECK(texto.find("10:00 | 10:30 | 30 min | " + descripcionDe(Especialidad::CARD)
                     + " | A-1 | Activo")
          != std::string::npos);

    // Y EL ORDEN ES CRONOLOGICO
    CHECK(texto.find("08:00") < texto.find("10:00"));
}

TEST_CASE("la agenda arranca en el dia de hoy que recibe la pantalla",
           calendario_agenda_hoy) {
    EscenarioCalendario escenario("calendario_agenda_hoy", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('3'));

    CHECK(pantalla.vista() == "Agenda");
    CHECK(pantalla.diaDeLaAgenda() == 10);

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("2027-03-10") != std::string::npos);

    // EL DIA DE LA SEMANA LO DICE EL DOMINIO
    //
    // `Fecha::diaSemana()` es determinista: el nombre sale de ahi, no del
    // reloj del sistema.
    CHECK(texto.find(Fecha::nombreDiaSemana(Fecha(2027, 3, 10).diaSemana())) != std::string::npos);
}

TEST_CASE("las flechas de la agenda cambian de dia", calendario_agenda_las_flechas) {
    EscenarioCalendario escenario("calendario_agenda_las_flechas", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('3'));
    CHECK(pantalla.diaDeLaAgenda() == 10);

    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaDerecha));
    CHECK(pantalla.diaDeLaAgenda() == 11);
    CHECK(pantalla.vistaTextoPlano().find("2027-03-11") != std::string::npos);

    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    pantalla.manejar(eventoTecla(ui::TipoTecla::FlechaIzquierda));
    CHECK(pantalla.diaDeLaAgenda() == 9);
    CHECK(pantalla.vistaTextoPlano().find("2027-03-09") != std::string::npos);
}

TEST_CASE("una agenda sin turnos dice que no hay y como volver", calendario_agenda_sin_turnos) {
    EscenarioCalendario escenario("calendario_agenda_sin_turnos", Fecha(2027, 3, 10));
    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    pantalla.manejar(eventoCaracter('3'));

    CHECK(pantalla.vista() == "Agenda");
    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("No hay turnos ese dia") != std::string::npos);
    CHECK(texto.find("0: volver al menu") != std::string::npos);
}

// ---------------------------------------------------------------------------
// LA RED DE LAS 80 COLUMNAS
//
// LAS DOS TABLAS DEL CALENDARIO TIENEN QUE ENTRAR EN UNA TERMINAL DE 80
//
// `ui::Tabla` no scrollea ni trunca: si la tabla es mas ancha que la
// terminal, la parte de la derecha no se ve. Eso hay que VERIFICARLO, no
// suponerlo.
//
// LOS DATOS SON LOS MAS LARGOS
//
// Los nombres mas largos de la semilla ("Valentina Ibarra" y "Diego
// Fernandez"), la especialidad mas larga del catalogo (Gastroenterologia) y
// el estado mas largo ("Reprogramado").
// ---------------------------------------------------------------------------
TEST_CASE("las tablas del calendario entran en 80 columnas",
           calendario_las_tablas_entran_en_80) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    EscenarioCalendario escenario("calendario_las_tablas_entran_en_80", Fecha(2027, 3, 10));
    escenario.pacientes.alta(pacienteDePrueba(1, "Valentina", "Ibarra", "2020391"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Diego", "Fernandez", "LEG-1001", Especialidad::CARD));
    escenario.turnos.alta(Turno(31, 1, 1, Especialidad::GAST, Fecha(2027, 3, 10), Hora(8, 0), 60,
                                "Consultorio 1", EstadoTurno::Reprogramado));

    ui::Pantallas::PantallaCalendario& pantalla = *escenario.puntero;

    // EL DETALLE DEL DIA
    abrirDetalle(pantalla, "10");
    CHECK(pantalla.vista() == "Detalle");
    CHECK(anchoNatural(pantalla.renderizar()) <= 80);

    // LA AGENDA
    pantalla.manejar(eventoCaracter('0'));
    pantalla.manejar(eventoCaracter('3'));
    CHECK(pantalla.vista() == "Agenda");
    CHECK(anchoNatural(pantalla.renderizar()) <= 80);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}





// ---------------------------------------------------------------------------
// EL CIERRE DE LA FASE 6 (PR15)
//
// QUE SE VERIFICA ACa
//
//   - el flag de color COMPARTIDO: el toggle cambia el render de una pantalla
//     VIVA, `NO_COLOR` es el estado INICIAL (no un veto) y recargar el
//     entorno vuelve a lo que dice el entorno;
//   - el menu principal COMPLETO (las ocho opciones) y sus atajos;
//   - la pila de la `ui::Aplicacion` (ir, volver, salir);
//   - el estado que comparte el `ui::Contexto`;
//   - las dos pantallas nuevas (configuracion y "acerca de");
//   - los estados vacios CON ACCION (los listados de pacientes, medicos y
//     turnos) y la carga de datos de ejemplo;
//   - el conteo de 38 pantallas del epico;
//   - la Definition of Done: el '0' y el `Escape` vuelven en todos los
//     niveles, toda baja o cancelacion pide confirmacion de dos pasos, y hay
//     UN SOLO formato de error en toda la aplicacion.
// ---------------------------------------------------------------------------
namespace {

// EL COLOR DE FRENTE DE UNA FILA
//
// El texto de una pantalla es el mismo con y sin color: lo que cambia es el
// color de la celda. Por eso el test del toggle mira la celda y no el texto.
//
// Devuelve `ftxui::Color::Default` si la fila no tiene texto.
ftxui::Color colorDeLaFila(const ftxui::Element& elemento, int fila, int ancho = 120,
                            int alto = 30) {
    ftxui::Screen pantalla = ftxui::Screen::Create(ftxui::Dimension::Fixed(ancho),
                                                   ftxui::Dimension::Fixed(alto));
    ftxui::Render(pantalla, elemento);

    for (int x = 0; x < pantalla.dimx(); ++x) {
        const ftxui::Cell& celda = pantalla.PixelAt(x, fila);
        if (!celda.character.empty() && celda.character != " ") {
            return celda.foreground_color;
        }
    }
    return ftxui::Color::Default;
}

// UN DIRECTORIO DE PRUEBAS NUEVO
//
// La carga de datos de ejemplo necesita una CARPETA: la semilla es el
// subdirectorio `ejemplo` de la carpeta del archivo de datos. Por eso este
// helper crea la carpeta (y borra la anterior, asi cada test arranca limpio).
std::filesystem::path directorioNuevoDePrueba(const std::string& nombre) {
    const std::filesystem::path base = directorioDePruebas() / nombre;
    std::error_code error;
    std::filesystem::remove_all(base, error);
    std::filesystem::create_directories(base, error);
    return base;
}

// Escribe un archivo de texto (en binario, con LF: los bytes de los
// archivos de datos no dependen del sistema operativo).
void escribirArchivo(const std::filesystem::path& ruta, const std::string& contenido) {
    std::ofstream salida(ruta, std::ios::out | std::ios::binary | std::ios::trunc);
    salida << contenido;
}

// Lee un archivo de texto.
std::string leerArchivo(const std::filesystem::path& ruta) {
    std::ifstream entrada(ruta, std::ios::in | std::ios::binary);
    std::ostringstream buffer;
    buffer << entrada.rdbuf();
    return buffer.str();
}

// LA SEMILLA DE PRUEBA
//
// Los tres archivos de la semilla, con UN registro cada uno (alcanza para
// probar que la carga funciona). Son archivos VALIDOS del formato v2: lo que
// se copia se recarga despues, asi que tiene que poder leerse.
void escribirSemilla(const std::filesystem::path& base) {
    const std::filesystem::path semilla = base / "ejemplo";
    std::error_code error;
    std::filesystem::create_directories(semilla, error);

    escribirArchivo(semilla / "pacientes.dat",
                    "#PACIENTES|2|1\n"
                    "id|nombre|apellido|email|telefono|activo|dni\n"
                    "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|2020391\n");
    escribirArchivo(semilla / "medicos.dat",
                    "#MEDICOS|2|1\n"
                    "id|nombre|apellido|email|telefono|activo|legajo|especialidad\n"
                    "1|Gomez|Laura|laura.gomez@unlp.edu.ar|2214556677|1|LEG-1001|CARD\n");
    escribirArchivo(semilla / "turnos.dat",
                    "#TURNOS|2|1\n"
                    "id|idMedico|idPaciente|codigoEspecialidad|fecha|hora|duracionMin|consultorio|"
                    "estado|idTurnoReemplazante\n"
                    "1|1|1|CARD|2027-03-10|10:00|45|A-1|Activo|-1\n");
}

// EL ESCENARIO DEL CIERRE
//
// Los repositorios de verdad, en un directorio NUEVO (con su carpeta
// `ejemplo` al lado), y la aplicacion con una pantalla de prueba DEBAJO.
struct EscenarioCierre {
    explicit EscenarioCierre(const std::string& nombre)
        : base(directorioNuevoDePrueba(nombre)),
          pacientes((base / "pacientes.dat").string()),
          medicos((base / "medicos.dat").string()),
          turnos((base / "turnos.dat").string()),
          servicio(pacientes, medicos, turnos),
          contexto(pacientes, medicos, turnos, servicio),
          aplicacion("Prueba", contexto) {
        pacientes.cargar();
        medicos.cargar();
        turnos.cargar();

        aplicacion.ir([](ui::Aplicacion&) { return std::make_unique<PantallaDePrueba>(); });
    }

    std::filesystem::path base;
    RepositorioPacientes pacientes;
    RepositorioMedicos medicos;
    RepositorioTurnos turnos;
    ServicioTurnos servicio;
    ui::Contexto contexto;
    ui::Aplicacion aplicacion;
};

// EL ESCENARIO DEL MOTOR CRUD CON CARPETA
//
// Es el `Escenario` del motor, pero con los repositorios en una CARPETA (con
// su carpeta `ejemplo` al lado): la carga de datos de ejemplo necesita la
// semilla al lado de los archivos vivos.
struct EscenarioCrudConCarpeta {
    explicit EscenarioCrudConCarpeta(const std::string& nombre)
        : base(directorioNuevoDePrueba(nombre)),
          pacientes((base / "pacientes.dat").string()),
          medicos((base / "medicos.dat").string()),
          turnos((base / "turnos.dat").string()),
          servicio(pacientes, medicos, turnos),
          contexto(pacientes, medicos, turnos, servicio),
          aplicacion("Prueba", contexto) {
        pacientes.cargar();
        medicos.cargar();
        turnos.cargar();

        aplicacion.ir([](ui::Aplicacion&) { return std::make_unique<PantallaDePrueba>(); });

        auto motor = std::make_unique<ui::CrudPersonas<Paciente>>(
            aplicacion, pacientes, pacientes,
            ui::Pantallas::configDePacientes(pacientes, servicio));
        puntero = motor.get();
        aplicacion.ir([&motor](ui::Aplicacion&) { return std::move(motor); });
    }

    std::filesystem::path base;
    RepositorioPacientes pacientes;
    RepositorioMedicos medicos;
    RepositorioTurnos turnos;
    ServicioTurnos servicio;
    ui::Contexto contexto;
    ui::Aplicacion aplicacion;
    ui::CrudPersonas<Paciente>* puntero = nullptr;
};

// EL ESCENARIO DEL MOTOR CRUD DE MEDICOS CON CARPETA
//
// Igual que el anterior, pero con el motor de medicos.
struct EscenarioCrudMedicosConCarpeta {
    explicit EscenarioCrudMedicosConCarpeta(const std::string& nombre)
        : base(directorioNuevoDePrueba(nombre)),
          medicos((base / "medicos.dat").string()),
          pacientes((base / "pacientes.dat").string()),
          turnos((base / "turnos.dat").string()),
          servicio(pacientes, medicos, turnos),
          contexto(pacientes, medicos, turnos, servicio),
          aplicacion("Prueba", contexto) {
        medicos.cargar();
        pacientes.cargar();
        turnos.cargar();

        aplicacion.ir([](ui::Aplicacion&) { return std::make_unique<PantallaDePrueba>(); });

        auto motor = std::make_unique<ui::CrudPersonas<Medico>>(
            aplicacion, medicos, medicos,
            ui::Pantallas::configDeMedicos(medicos, servicio));
        puntero = motor.get();
        aplicacion.ir([&motor](ui::Aplicacion&) { return std::move(motor); });
    }

    std::filesystem::path base;
    RepositorioMedicos medicos;
    RepositorioPacientes pacientes;
    RepositorioTurnos turnos;
    ServicioTurnos servicio;
    ui::Contexto contexto;
    ui::Aplicacion aplicacion;
    ui::CrudPersonas<Medico>* puntero = nullptr;
};

// UNA PANTALLA MARCADA
//
// Es como `PantallaDePrueba`, pero con un texto propio: asi dos pantallas
// apiladas se pueden distinguir.
class PantallaMarcada : public ui::Pantalla {
public:
    explicit PantallaMarcada(std::string texto) : _texto(std::move(texto)) {
    }

    std::string titulo() const override { return _texto; }
    ftxui::Element renderizar() override { return ftxui::text(_texto); }
    bool manejar(const ui::Evento&) override { return false; }
    std::string textoPlano() const override { return _texto; }

private:
    std::string _texto;
};

// LA CANTIDAD DE OPCIONES DE UN MENU EN TEXTO PLANO
//
// Cada opcion es una linea que empieza con "  N. " (dos espacios, el atajo
// y el punto). Es el MISMO formato que dibuja `ui::Menu`, asi que el test
// cuenta lo que el usuario ve.
int cantidadDeOpciones(const std::string& textoDelMenu) {
    int cantidad = 0;
    std::size_t inicio = 0;
    while (inicio <= textoDelMenu.size()) {
        const std::size_t fin = textoDelMenu.find('\n', inicio);
        const std::string linea = textoDelMenu.substr(
            inicio, fin == std::string::npos ? std::string::npos : fin - inicio);
        if (linea.size() >= 4 && linea.compare(0, 2, "  ") == 0
            && std::isdigit(static_cast<unsigned char>(linea[2])) && linea[3] == '.') {
            ++cantidad;
        }
        if (fin == std::string::npos) {
            break;
        }
        inicio = fin + 1;
    }
    return cantidad;
}

// LA RAIZ DEL REPOSITORIO
//
// Se busca subiendo desde el directorio actual hasta encontrar el
// `CMakeLists.txt` con los directorios `include` y `src` al lado: eso es el
// repositorio. Si no se encuentra (por ejemplo, si los tests se corren
// desde un binario copiado a otro lado), devuelve una ruta vacia.
std::filesystem::path raizDelRepositorio() {
    std::filesystem::path actual = std::filesystem::current_path();
    for (int i = 0; i < 8; ++i) {
        if (std::filesystem::exists(actual / "CMakeLists.txt")
            && std::filesystem::is_directory(actual / "include")
            && std::filesystem::is_directory(actual / "src")) {
            return actual;
        }
        const std::filesystem::path padre = actual.parent_path();
        if (padre == actual) {
            break;
        }
        actual = padre;
    }
    return std::filesystem::path();
}

// TODOS LOS ARCHIVOS DE LA CAPA `ui`
std::vector<std::filesystem::path> archivosDeLaCapaUi(const std::filesystem::path& raiz) {
    std::vector<std::filesystem::path> archivos;
    const std::filesystem::path carpetas[] = {raiz / "include" / "ui", raiz / "src" / "ui"};
    for (const std::filesystem::path& carpeta : carpetas) {
        if (!std::filesystem::is_directory(carpeta)) {
            continue;
        }
        for (const auto& entrada : std::filesystem::recursive_directory_iterator(carpeta)) {
            if (entrada.is_regular_file()) {
                archivos.push_back(entrada.path());
            }
        }
    }
    return archivos;
}

} // namespace

// ---------------------------------------------------------------------------
// EL FLAG DE COLOR COMPARTIDO
// ---------------------------------------------------------------------------

TEST_CASE("el toggle cambia el render de una pantalla VIVA", tema_el_toggle_cambia_el_render) {
    // SIN COLORES
    ui::Theme::establecerColoresActivos(false);

    EscenarioCierre escenario("tema_el_toggle_cambia_el_render");
    ui::Pantallas::MenuPrincipal menu(escenario.aplicacion);

    // El titulo de la pantalla se dibuja con el color del titulo... si los
    // colores estan encendidos.
    CHECK(colorDeLaFila(menu.renderizar(), 0) == ftxui::Color::Default);

    // EL TOGGLE: el estado es COMPARTIDO, asi que el cambio se ve en la
    // pantalla que YA esta abierta (no en una nueva).
    ui::Theme::establecerColoresActivos(true);

    const ui::Theme tema;
    CHECK(colorDeLaFila(menu.renderizar(), 0) == ui::aFtxui(tema.titulo()));

    ui::Theme::establecerColoresActivos(false);
}

TEST_CASE("NO_COLOR definido y no vacio arranca SIN color", tema_no_color_no_vacio) {
    definirNoColor("1");
    ui::Theme::recargarEntorno();

    // EL ENTORNO MANDA AL ARRANCAR
    CHECK(ui::Theme::coloresActivosPorDefecto() == false);
    const ui::Theme tema;
    CHECK(tema.coloresActivos() == false);
    CHECK(tema.colorDe(ui::Rol::Titulo).has_value() == false);

    // PERO NO ES UN VETO: el usuario puede ENCENDERLOS desde la
    // configuracion.
    ui::Theme::establecerColoresActivos(true);
    CHECK(ui::Theme().coloresActivos() == true);
    CHECK(ui::Theme().colorDe(ui::Rol::Titulo).has_value() == true);

    ui::Theme::establecerColoresActivos(false);
    borrarNoColor();
    ui::Theme::recargarEntorno();
}

TEST_CASE("NO_COLOR definido y vacio arranca CON color", tema_no_color_vacio) {
    // Un `NO_COLOR` DEFINIDO pero VACIO no apaga los colores: la
    // especificacion dice que la variable tiene que tener un valor.
    //
    // OJO CON WINDOWS
    //
    // En Windows una variable de entorno no puede existir VACIA (la CRT la
    // borra: `_putenv_s` con una cadena vacia la elimina), asi que ahi el
    // caso se verifica con el estado que el sistema operativo puede
    // representar. En POSIX (donde si se puede) el caso se verifica entero.
    // El resultado esperado es el MISMO en los dos: los colores ENCENDIDOS.
    definirNoColor("");
    ui::Theme::recargarEntorno();

    CHECK(ui::Theme::coloresActivosPorDefecto() == true);
    const ui::Theme tema;
    CHECK(tema.coloresActivos() == true);
    CHECK(tema.colorDe(ui::Rol::Titulo).has_value() == true);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

TEST_CASE("establecer los colores y recargar el entorno vuelve al valor del entorno",
           tema_establecer_y_recargar) {
    borrarNoColor();
    ui::Theme::recargarEntorno();
    CHECK(ui::Theme().coloresActivos() == true);

    // EL TOGGLE MANUAL
    ui::Theme::establecerColoresActivos(false);
    CHECK(ui::Theme().coloresActivos() == false);

    // RECARGAR EL ENTORNO VUELVE A LO QUE DICE EL ENTORNO (no a lo que el
    // usuario habia apagado a mano).
    ui::Theme::recargarEntorno();
    CHECK(ui::Theme().coloresActivos() == true);

    // Y CON `NO_COLOR` DEFINIDO, RECARGAR LOS APAGA.
    definirNoColor("1");
    ui::Theme::recargarEntorno();
    CHECK(ui::Theme().coloresActivos() == false);

    borrarNoColor();
    ui::Theme::recargarEntorno();
}

TEST_CASE("un tema forzado no toca el estado compartido", tema_forzado_no_toca_el_compartido) {
    // El estado compartido arranca con los colores ENCENDIDOS: el test
    // comprueba que un tema forzado NO lo toca.
    ui::Theme::establecerColoresActivos(true);

    // EL TEMA FORZADO (lo usan los tests)
    const ui::Theme sin(false);
    CHECK(sin.coloresActivos() == false);
    CHECK(sin.colorDe(ui::Rol::Titulo).has_value() == false);

    // EL ESTADO COMPARTIDO NO SE TOCO: otra instancia sigue con los colores.
    CHECK(ui::Theme().coloresActivos() == true);
    CHECK(ui::Theme().colorDe(ui::Rol::Titulo).has_value() == true);

    // Se restaura el valor del ENTORNO para el test que sigue: si `NO_COLOR`
    // esta definida, el valor del entorno es "sin colores".
    ui::Theme::recargarEntorno();
}

// ---------------------------------------------------------------------------
// EL MENU PRINCIPAL
// ---------------------------------------------------------------------------

TEST_CASE("el menu principal muestra las ocho opciones",
           menu_principal_las_ocho_opciones) {
    EscenarioCierre escenario("menu_principal_las_ocho_opciones");
    ui::Pantallas::MenuPrincipal menu(escenario.aplicacion);

    const std::string texto = menu.textoPlano();
    CHECK(texto.find("1. Pacientes") != std::string::npos);
    CHECK(texto.find("2. Medicos") != std::string::npos);
    CHECK(texto.find("3. Turnos") != std::string::npos);
    CHECK(texto.find("4. Reportes") != std::string::npos);
    CHECK(texto.find("5. Calendario") != std::string::npos);
    CHECK(texto.find("6. Configuracion") != std::string::npos);
    CHECK(texto.find("7. Acerca de") != std::string::npos);
    CHECK(texto.find("0. Salir") != std::string::npos);

    // Y son ocho: ni una mas ni una menos.
    CHECK(cantidadDeOpciones(menu.textoPlano()) == 8);
}

TEST_CASE("los atajos del menu principal abren los modulos y el cero sale",
           menu_principal_los_atajos) {
    EscenarioCierre escenario("menu_principal_los_atajos");
    ui::Pantallas::MenuPrincipal menu(escenario.aplicacion);

    // EL '1' ABRE PACIENTES
    menu.manejar(eventoCaracter('1'));
    CHECK(escenario.aplicacion.textoPlanoActual().find("PACIENTES") != std::string::npos);
    CHECK(escenario.aplicacion.saliendo() == false);

    // EL '2' ABRE MEDICOS (desde el menu principal, que sigue abajo en la pila)
    ui::Pantallas::MenuPrincipal menu2(escenario.aplicacion);
    menu2.manejar(eventoCaracter('2'));
    CHECK(escenario.aplicacion.textoPlanoActual().find("MEDICOS") != std::string::npos);

    // EL '0' DEL MENU PRINCIPAL SALE DE LA APLICACION
    ui::Pantallas::MenuPrincipal menu3(escenario.aplicacion);
    menu3.manejar(eventoCaracter('0'));
    CHECK(escenario.aplicacion.saliendo() == true);
}

TEST_CASE("los atajos seis y siete abren la configuracion y el acerca de",
           menu_principal_configuracion_y_acerca_de) {
    EscenarioCierre escenario("menu_principal_configuracion_y_acerca_de");
    ui::Pantallas::MenuPrincipal menu(escenario.aplicacion);

    menu.manejar(eventoCaracter('6'));
    CHECK(escenario.aplicacion.textoPlanoActual().find("CONFIGURACION") != std::string::npos);

    EscenarioCierre otro("menu_principal_acerca_de");
    ui::Pantallas::MenuPrincipal menu2(otro.aplicacion);
    menu2.manejar(eventoCaracter('7'));
    CHECK(otro.aplicacion.textoPlanoActual().find("ACERCA DE") != std::string::npos);
}

// ---------------------------------------------------------------------------
// LA PILA DE LA APLICACION
// ---------------------------------------------------------------------------

TEST_CASE("la pila de la aplicacion apila, vuelve y sale", aplicacion_la_pila) {
    EscenarioCierre escenario("aplicacion_la_pila");

    // LA PILA ARRANCA CON LA PANTALLA DE PRUEBA
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");

    // `ir` APILA: la de arriba es la que se ve.
    escenario.aplicacion.ir([](ui::Aplicacion&) {
        return std::make_unique<PantallaMarcada>("arriba");
    });
    CHECK(escenario.aplicacion.textoPlanoActual() == "arriba");

    // `volver` SACA LA DE ARRIBA: la de abajo queda arriba.
    escenario.aplicacion.volver();
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");
    CHECK(escenario.aplicacion.saliendo() == false);

    // `volver` CON UNA SOLA PANTALLA TERMINA EL CICLO.
    escenario.aplicacion.volver();
    CHECK(escenario.aplicacion.saliendo() == true);
}

TEST_CASE("una fabrica vacia no apila nada y salir termina el ciclo",
           aplicacion_fabrica_vacia_y_salir) {
    EscenarioCierre escenario("aplicacion_fabrica_vacia_y_salir");

    // UNA FABRICA NULA NO APILA NADA
    escenario.aplicacion.ir(ui::Aplicacion::Fabrica());
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");

    // `salir` TERMINA EL CICLO
    CHECK(escenario.aplicacion.saliendo() == false);
    escenario.aplicacion.salir();
    CHECK(escenario.aplicacion.saliendo() == true);
}

// ---------------------------------------------------------------------------
// EL CONTEXTO: EL ESTADO COMPARTIDO
// ---------------------------------------------------------------------------

TEST_CASE("el contexto comparte el estado entre las pantallas",
           contexto_el_estado_compartido) {
    EscenarioCierre escenario("contexto_el_estado_compartido");

    // EL MENSAJE DE LA ULTIMA OPERACION
    //
    // Lo escribe una pantalla y lo lee la que esta arriba: es el estado que
    // se comparte ENTRE pantallas.
    CHECK(escenario.contexto.mensaje().esVacio());
    escenario.contexto.mostrarMensaje(ui::Mensaje::exito("Paciente #7 registrado."));
    CHECK(escenario.contexto.mensaje().esExito());
    CHECK(escenario.contexto.mensaje().texto() == "OK  Paciente #7 registrado.");

    // Y EL COLOR ES UN ESTADO COMPARTIDO DEL PROGRAMA
    //
    // No vive en el contexto, pero se comparte igual: lo que cambia en una
    // pantalla se ve en todas las demas.
    ui::Theme::establecerColoresActivos(false);
    CHECK(ui::Theme().coloresActivos() == false);
    // Se restaura el valor del ENTORNO (no un true a secas): si `NO_COLOR`
    // esta definida, el valor del entorno es "sin colores".
    ui::Theme::recargarEntorno();
    CHECK(ui::Theme().coloresActivos() == ui::Theme::coloresActivosPorDefecto());
}

// ---------------------------------------------------------------------------
// LA PANTALLA DE CONFIGURACION
// ---------------------------------------------------------------------------

TEST_CASE("el menu de configuracion muestra las dos acciones y el cero de volver",
           configuracion_menu_inicial) {
    EscenarioCierre escenario("configuracion_menu_inicial");
    ui::Pantallas::PantallaConfiguracion pantalla(escenario.aplicacion);

    CHECK(pantalla.vista() == "Menu");
    CHECK(pantalla.titulo() == "CONFIGURACION");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("1. Activar / desactivar colores") != std::string::npos);
    CHECK(texto.find("2. Cargar datos de ejemplo") != std::string::npos);
    CHECK(texto.find("0. Volver") != std::string::npos);
    CHECK(cantidadDeOpciones(texto) == 3);
}

TEST_CASE("el toggle de los colores cambia el estado compartido y avisa",
           configuracion_el_toggle) {
    EscenarioCierre escenario("configuracion_el_toggle");
    ui::Pantallas::PantallaConfiguracion pantalla(escenario.aplicacion);

    ui::Theme::establecerColoresActivos(false);

    pantalla.manejar(eventoCaracter('1'));
    CHECK(ui::Theme().coloresActivos() == true);
    CHECK(escenario.contexto.mensaje().esExito());
    CHECK(escenario.contexto.mensaje().texto() == "OK  Colores activados.");

    pantalla.manejar(eventoCaracter('1'));
    CHECK(ui::Theme().coloresActivos() == false);
    CHECK(escenario.contexto.mensaje().texto() == "OK  Colores desactivados.");

    // Se restaura el valor del ENTORNO (no un true a secas): si `NO_COLOR`
    // esta definida, el valor del entorno es "sin colores".
    ui::Theme::recargarEntorno();
}

TEST_CASE("cargar los datos de ejemplo pide confirmacion de dos pasos",
           configuracion_la_carga_pide_confirmacion) {
    EscenarioCierre escenario("configuracion_la_carga_pide_confirmacion");
    escribirSemilla(escenario.base);
    ui::Pantallas::PantallaConfiguracion pantalla(escenario.aplicacion);

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Confirmar");

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Cargar los datos de ejemplo?") != std::string::npos);
    CHECK(texto.find("1. Si") != std::string::npos);
    CHECK(texto.find("2. No") != std::string::npos);

    // TODAVIA NO SE COPIO NADA: la pantalla esta esperando la respuesta.
    CHECK(escenario.pacientes.contar() == 0);

    // EL '2' ES "NO": no se carga nada.
    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.pacientes.contar() == 0);
    CHECK(escenario.medicos.contar() == 0);
    CHECK(escenario.turnos.contar() == 0);
}

TEST_CASE("cargar los datos de ejemplo copia la semilla y recarga los repositorios",
           configuracion_la_carga_copia_y_recarga) {
    EscenarioCierre escenario("configuracion_la_carga_copia_y_recarga");
    escribirSemilla(escenario.base);
    ui::Pantallas::PantallaConfiguracion pantalla(escenario.aplicacion);

    const std::string semillaAntes =
        leerArchivo(escenario.base / "ejemplo" / "pacientes.dat");

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Confirmar");

    // EL '1' ES "SI": se copia la semilla y se recarga.
    pantalla.manejar(eventoCaracter('1'));

    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().esExito());
    CHECK(escenario.contexto.mensaje().texto() == "OK  Datos de ejemplo cargados.");

    // LOS REPOSITORIOS TIENEN LOS DATOS NUEVOS
    //
    // Se recargaron EN EL LUGAR: son los mismos objetos que la pantalla
    // tiene por referencia, asi que ven los datos sin enterarse.
    CHECK(escenario.pacientes.contar() == 1);
    CHECK(escenario.medicos.contar() == 1);
    CHECK(escenario.turnos.contar() == 1);

    // Y LA SEMILLA NO SE TOCO: la copia va de la semilla al archivo vivo,
    // nunca al reves.
    CHECK(leerArchivo(escenario.base / "ejemplo" / "pacientes.dat") == semillaAntes);
}

TEST_CASE("una carga que falla avisa el error y no lanza", configuracion_la_carga_falla) {
    EscenarioCierre escenario("configuracion_la_carga_falla");
    // NO hay semilla: la copia no se puede hacer.
    ui::Pantallas::PantallaConfiguracion pantalla(escenario.aplicacion);

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Confirmar");

    pantalla.manejar(eventoCaracter('1'));

    // EL ERROR SE MUESTRA EN PANTALLA (con el formato unico) y la pantalla
    // sigue viva.
    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().esError());
    CHECK(escenario.contexto.mensaje().texto().compare(0, 4, "ERR ") == 0);
}

TEST_CASE("el cero y el escape de la configuracion vuelven",
           configuracion_el_cero_y_el_escape_vuelven) {
    EscenarioCierre escenario("configuracion_el_cero_y_el_escape_vuelven");
    ui::Pantallas::PantallaConfiguracion pantalla(escenario.aplicacion);

    // EL '0' SACA LA PANTALLA DE LA PILA
    pantalla.manejar(eventoCaracter('0'));
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");

    // EL `Escape` TAMBIEN (la pantalla lo consume: no tiene vistas internas).
    ui::Pantallas::PantallaConfiguracion otra(escenario.aplicacion);
    otra.manejar(eventoTecla(ui::TipoTecla::Escape));
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");
}

TEST_CASE("el raton tambien confirma la carga de ejemplo de la configuracion",
           configuracion_el_raton_confirma) {    EscenarioCierre escenario("configuracion_el_raton_confirma");
    escribirSemilla(escenario.base);
    ui::Pantallas::PantallaConfiguracion pantalla(escenario.aplicacion);

    pantalla.manejar(eventoCaracter('2'));
    CHECK(pantalla.vista() == "Confirmar");

    // Un clic en el ENCABEZADO (la fila 0) cae fuera de las opciones: no
    // confirma nada.
    ui::Evento encabezado;
    encabezado.boton = ui::BotonMouse::Izquierdo;
    encabezado.x = 4;
    encabezado.y = 0;
    pantalla.manejar(encabezado);
    CHECK(pantalla.vista() == "Confirmar");
    CHECK(escenario.pacientes.contar() == 0);

    // Un clic en la fila del "Si" (la fila 3) SI confirma: la
    // `ui::Confirmacion` hace su propio hit-testing (la pantalla le dice en
    // que fila se dibuja).
    ui::Evento si;
    si.boton = ui::BotonMouse::Izquierdo;
    si.x = 4;
    si.y = 3;
    pantalla.manejar(si);

    CHECK(pantalla.vista() == "Menu");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Datos de ejemplo cargados.");
    CHECK(escenario.pacientes.contar() == 1);
}

// ---------------------------------------------------------------------------
// EL HELPER DE LA CARGA: LAS GARANTIAS DEL ARCHIVO
// ---------------------------------------------------------------------------

TEST_CASE("la copia deja el archivo vivo completo y sin temporales",
           carga_la_copia_es_atomica) {
    EscenarioCierre escenario("carga_la_copia_es_atomica");
    escribirSemilla(escenario.base);

    // EL ARCHIVO VIVO TIENE OTRO CONTENIDO
    escenario.pacientes.alta(pacienteDePrueba(1, "Viejo", "Paciente", "2020399"));

    std::string error;
    CHECK(ui::cargarDatosDeEjemplo(escenario.contexto, error) == true);
    CHECK(error.empty());

    // EL ARCHIVO VIVO ES EXACTAMENTE LA SEMILLA
    //
    // Si la copia se hiciera en dos pasos sin `rename`, el archivo podria
    // quedar truncado a mitad de copia.
    CHECK(leerArchivo(escenario.base / "pacientes.dat")
          == leerArchivo(escenario.base / "ejemplo" / "pacientes.dat"));
    CHECK(leerArchivo(escenario.base / "turnos.dat")
          == leerArchivo(escenario.base / "ejemplo" / "turnos.dat"));

    // Y NO QUEDA NINGUN TEMPORAL TIRADO
    CHECK(std::filesystem::exists(escenario.base / "pacientes.dat.tmp") == false);
    CHECK(std::filesystem::exists(escenario.base / "medicos.dat.tmp") == false);
    CHECK(std::filesystem::exists(escenario.base / "turnos.dat.tmp") == false);

    // Y LA SEMILLA NO SE TOCO
    CHECK(leerArchivo(escenario.base / "ejemplo" / "pacientes.dat").find("Garcia")
          != std::string::npos);
}

TEST_CASE("una semilla invalida no lanza y los otros repositorios se recargan",
           carga_la_semilla_invalida_no_lanza) {
    EscenarioCierre escenario("carga_la_semilla_invalida_no_lanza");
    escribirSemilla(escenario.base);

    // LA SEMILLA DE PACIENTES ES INVALIDA
    //
    // Un archivo de la version 1: el repositorio lo RECHAZA con un error
    // claro (no lo adivina). Es la forma de hacer que la recarga de UN
    // repositorio falle sin tocar el resto.
    //
    // OJO: es el PRIMERO que se recarga a proposito: si la recarga no
    // estuviera aislada, la excepcion cortaria el proceso y los otros dos
    // repositorios se quedarian con los datos VIEJOS.
    escribirArchivo(escenario.base / "ejemplo" / "pacientes.dat",
                    "#PACIENTES|1|1\n"
                    "id|nombre|apellido|email|telefono|activo|dni\n"
                    "1|Viejo|Paciente|viejo@unlp.edu.ar|2214556677|1|2020399\n");

    // HAY DATOS VIEJOS: la carga tiene que cambiar los de medicos y turnos.
    escenario.pacientes.alta(pacienteDePrueba(1, "Viejo", "Paciente", "2020399"));
    escenario.medicos.alta(
        medicoConEspecialidad(1, "Viejo", "Medico", "LEG-9999", Especialidad::CARD));
    escenario.turnos.alta(
        Turno(1, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(8, 0), 45, "A-1"));

    std::string error;
    const bool salio = ui::cargarDatosDeEjemplo(escenario.contexto, error);

    // NO SE LANZO NADA: el helper devuelve false y el error dice por que.
    CHECK(salio == false);
    CHECK(error.find("pacientes.dat") != std::string::npos);

    // Y LOS OTROS DOS SE RECARGARON
    //
    // Si la falla de uno los hubiera dejado con los datos viejos, la
    // proxima `persistir()` de un alta pisaria el archivo nuevo con la
    // memoria vieja (y la carga se revertiria en silencio).
    CHECK(escenario.medicos.contar() == 1);
    Medico medico;
    CHECK(escenario.medicos.buscarPorId(1, medico) == true);
    CHECK(medico.getNombre() == "Gomez");
    CHECK(escenario.turnos.contar() == 1);
    Turno turno;
    CHECK(escenario.turnos.buscarPorId(1, turno) == true);
    CHECK(turno.getFecha() == Fecha(2027, 3, 10));

    // EL QUE FALLO NO PERDIO SUS DATOS VIEJOS: la recarga se aborta antes de
    // tocar el arbol (el repositorio rechaza el archivo y no adivina).
    CHECK(escenario.pacientes.contar() == 1);
    Paciente paciente;
    CHECK(escenario.pacientes.buscarPorId(1, paciente) == true);
    CHECK(paciente.getNombre() == "Viejo");
}

// ---------------------------------------------------------------------------
// LA PANTALLA DE ACERCA DE
// ---------------------------------------------------------------------------

TEST_CASE("acerca de dice los datos del trabajo practico", acerca_de_los_datos) {
    EscenarioCierre escenario("acerca_de_los_datos");
    ui::Pantallas::PantallaAcercaDe pantalla(escenario.aplicacion);

    const std::string texto = pantalla.vistaTextoPlano();
    CHECK(texto.find("Sistema de Turnos Medicos") != std::string::npos);
    CHECK(texto.find("Analisis de Sistemas, segundo anio") != std::string::npos);
    CHECK(texto.find("Trabajo Practico Final de Ingenieria de Software")
          != std::string::npos);
    CHECK(texto.find("Adrian Joaquin Naum") != std::string::npos);
}

TEST_CASE("acerca de muestra la tabla de atajos", acerca_de_los_atajos) {
    EscenarioCierre escenario("acerca_de_los_atajos");
    ui::Pantallas::PantallaAcercaDe pantalla(escenario.aplicacion);

    const std::string texto = pantalla.vistaTextoPlano();

    // EL TECLADO
    CHECK(texto.find("Flechas arriba / abajo") != std::string::npos);
    CHECK(texto.find("Mueven la seleccion") != std::string::npos);
    CHECK(texto.find("Enter") != std::string::npos);
    CHECK(texto.find("Elige la opcion") != std::string::npos);
    CHECK(texto.find("Vuelve") != std::string::npos);
    CHECK(texto.find("Pasa al siguiente campo") != std::string::npos);
    CHECK(texto.find("Borra el caracter anterior") != std::string::npos);
    CHECK(texto.find("Elige la opcion cuyo atajo es ese numero") != std::string::npos);

    // EL RATON
    CHECK(texto.find("Clic izquierdo") != std::string::npos);
    CHECK(texto.find("Rueda del raton") != std::string::npos);
}

TEST_CASE("acerca de da los creditos de FTXUI", acerca_de_los_creditos) {
    EscenarioCierre escenario("acerca_de_los_creditos");
    ui::Pantallas::PantallaAcercaDe pantalla(escenario.aplicacion);

    const std::string texto = pantalla.vistaTextoPlano();

    // La version y la licencia son las de la copia VENDORIZADA (ver
    // `third_party/ftxui/PROVENANCE.md` y `third_party/ftxui/LICENSE`).
    CHECK(texto.find("FTXUI v7.0.3") != std::string::npos);
    CHECK(texto.find("MIT") != std::string::npos);
    CHECK(texto.find("third_party/ftxui/") != std::string::npos);
}

TEST_CASE("el render de acerca de se puede dibujar en FTXUI", acerca_de_el_render) {
    EscenarioCierre escenario("acerca_de_el_render");
    ui::Pantallas::PantallaAcercaDe pantalla(escenario.aplicacion);

    const std::vector<std::string> lineas = renderizarLineas(pantalla.renderizar(), 120, 40);
    CHECK(lineas.size() > 15);

    bool encontroElSistema = false;
    bool encontroLosCreditos = false;
    for (const std::string& linea : lineas) {
        if (linea.find("Sistema de Turnos Medicos") != std::string::npos) {
            encontroElSistema = true;
        }
        if (linea.find("FTXUI v7.0.3") != std::string::npos) {
            encontroLosCreditos = true;
        }
    }
    CHECK(encontroElSistema);
    CHECK(encontroLosCreditos);

    // LA RED DE LAS 80 COLUMNAS
    //
    // La tabla de atajos tiene que entrar en una terminal de 80: si no
    // entra, la columna de la derecha queda cortada.
    CHECK(anchoNatural(pantalla.renderizar()) <= 80);
}

TEST_CASE("el cero y el escape de acerca de vuelven", acerca_de_el_cero_y_el_escape_vuelven) {
    EscenarioCierre escenario("acerca_de_el_cero_y_el_escape_vuelven");
    ui::Pantallas::PantallaAcercaDe pantalla(escenario.aplicacion);

    pantalla.manejar(eventoCaracter('0'));
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");

    ui::Pantallas::PantallaAcercaDe otra(escenario.aplicacion);
    otra.manejar(eventoTecla(ui::TipoTecla::Escape));
    CHECK(escenario.aplicacion.textoPlanoActual() == "pantalla de prueba");
}

// ---------------------------------------------------------------------------
// LOS ESTADOS VACIOS CON ACCION
// ---------------------------------------------------------------------------

TEST_CASE("el listado vacio de pacientes ofrece cargar el ejemplo",
           crud_el_vacio_ofrece_cargar_el_ejemplo) {
    EscenarioCrudConCarpeta escenario("crud_el_vacio_ofrece_cargar_el_ejemplo");
    escribirSemilla(escenario.base);
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('4'));
    CHECK(motor.vista() == "Listado");

    // EL ESTADO VACIO ES UN MENU DE DOS OPCIONES
    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("No hay pacientes") != std::string::npos);
    CHECK(texto.find("1. Cargar ejemplo") != std::string::npos);
    CHECK(texto.find("2. Volver") != std::string::npos);

    // EL '1' PIDE CONFIRMACION DE DOS PASOS
    motor.manejar(eventoCaracter('1'));
    CHECK(motor.vista() == "ConfirmarEjemplo");
    CHECK(motor.vistaTextoPlano().find("Cargar los datos de ejemplo?")
          != std::string::npos);

    // EL '1' CONFIRMA: se copia la semilla y el listado se refresca.
    motor.manejar(eventoCaracter('1'));
    CHECK(motor.vista() == "Listado");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Datos de ejemplo cargados.");

    // EL LISTADO MUESTRA LOS DATOS NUEVOS
    const std::string despues = motor.vistaTextoPlano();
    CHECK(despues.find("Garcia") != std::string::npos);
    CHECK(despues.find("Cargar ejemplo") == std::string::npos);
}

TEST_CASE("el raton tambien confirma la carga del ejemplo", crud_el_vacio_el_raton) {
    EscenarioCrudConCarpeta escenario("crud_el_vacio_el_raton");
    escribirSemilla(escenario.base);
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('4'));
    motor.manejar(eventoCaracter('1'));
    CHECK(motor.vista() == "ConfirmarEjemplo");

    // Un clic en el ENCABEZADO (la fila 0) cae fuera de las opciones: no
    // confirma nada.
    ui::Evento encabezado;
    encabezado.boton = ui::BotonMouse::Izquierdo;
    encabezado.x = 4;
    encabezado.y = 0;
    motor.manejar(encabezado);
    CHECK(motor.vista() == "ConfirmarEjemplo");
    CHECK(escenario.pacientes.contar() == 0);

    // Un clic en la fila del "Si" (la fila 3) SI confirma: la `ui::Confirmacion`
    // hace su propio hit-testing (el motor le dice en que fila se dibuja).
    ui::Evento si;
    si.boton = ui::BotonMouse::Izquierdo;
    si.x = 4;
    si.y = 3;
    motor.manejar(si);

    CHECK(motor.vista() == "Listado");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Datos de ejemplo cargados.");
    CHECK(escenario.pacientes.contar() == 1);
}

TEST_CASE("el listado vacio de medicos ofrece cargar el ejemplo",
           crud_el_vacio_de_medicos_ofrece_cargar_el_ejemplo) {
    EscenarioCrudMedicosConCarpeta escenario(
        "crud_el_vacio_de_medicos_ofrece_cargar_el_ejemplo");
    escribirSemilla(escenario.base);
    ui::CrudPersonas<Medico>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('4'));
    CHECK(motor.vista() == "Listado");

    const std::string texto = motor.vistaTextoPlano();
    CHECK(texto.find("No hay medicos") != std::string::npos);
    CHECK(texto.find("1. Cargar ejemplo") != std::string::npos);
    CHECK(texto.find("2. Volver") != std::string::npos);

    motor.manejar(eventoCaracter('1'));
    CHECK(motor.vista() == "ConfirmarEjemplo");

    motor.manejar(eventoCaracter('1'));
    CHECK(motor.vista() == "Listado");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Datos de ejemplo cargados.");
    CHECK(motor.vistaTextoPlano().find("Gomez") != std::string::npos);
}

TEST_CASE("el listado vacio de turnos ofrece cargar el ejemplo",
           turnos_el_vacio_ofrece_cargar_el_ejemplo) {
    EscenarioCierre escenario("turnos_el_vacio_ofrece_cargar_el_ejemplo");
    escribirSemilla(escenario.base);

    auto pantalla = std::make_unique<ui::Pantallas::PantallaTurnos>(
        escenario.aplicacion, escenario.turnos, escenario.pacientes, escenario.medicos,
        escenario.servicio);
    ui::Pantallas::PantallaTurnos& turnos = *pantalla;
    escenario.aplicacion.ir(
        [&pantalla](ui::Aplicacion&) { return std::move(pantalla); });

    turnos.manejar(eventoCaracter('5'));
    CHECK(turnos.vista() == "Listado");

    const std::string texto = turnos.vistaTextoPlano();
    CHECK(texto.find("No hay turnos") != std::string::npos);
    CHECK(texto.find("1. Cargar ejemplo") != std::string::npos);
    CHECK(texto.find("2. Volver") != std::string::npos);

    turnos.manejar(eventoCaracter('1'));
    CHECK(turnos.vista() == "ConfirmarEjemplo");

    turnos.manejar(eventoCaracter('1'));
    CHECK(turnos.vista() == "Listado");
    CHECK(escenario.contexto.mensaje().texto() == "OK  Datos de ejemplo cargados.");
    CHECK(turnos.vistaTextoPlano().find("2027-03-10") != std::string::npos);
}

TEST_CASE("el cero y el escape del estado vacio vuelven",
           cierre_el_cero_y_el_escape_del_estado_vacio_vuelven) {
    EscenarioCrudConCarpeta escenario("cierre_el_cero_y_el_escape_del_estado_vacio_vuelven");
    ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

    motor.manejar(eventoCaracter('4'));
    CHECK(motor.vista() == "Listado");

    // EL '0' DEL ESTADO VACIO VUELVE AL MENU DEL MOTOR
    motor.manejar(eventoCaracter('0'));
    CHECK(motor.vista() == "Menu");

    // EL 'Escape' TAMBIEN
    motor.manejar(eventoCaracter('4'));
    motor.manejar(eventoTecla(ui::TipoTecla::Escape));
    CHECK(motor.vista() == "Menu");
}

// ---------------------------------------------------------------------------
// EL CONTEO DE 38 PANTALLAS DEL EPICO
// ---------------------------------------------------------------------------

TEST_CASE("el epico cuenta 38 pantallas y cada menu lista sus opciones",
           cierre_las_38_pantallas) {
    // CADA OPCION DE UN MENU ES UNA PANTALLA DEL EPICO
    //
    // El menu principal y la pantalla de "acerca de" son UNA cada una (no
    // son menus de acciones).
    int total = 0;

    // 1. EL MENU PRINCIPAL (una pantalla)
    {
        EscenarioCierre escenario("cierre_38_menu_principal");
        ui::Pantallas::MenuPrincipal menu(escenario.aplicacion);
        const std::string texto = menu.textoPlano();
        CHECK(texto.find("1. Pacientes") != std::string::npos);
        CHECK(texto.find("2. Medicos") != std::string::npos);
        CHECK(texto.find("3. Turnos") != std::string::npos);
        CHECK(texto.find("4. Reportes") != std::string::npos);
        CHECK(texto.find("5. Calendario") != std::string::npos);
        CHECK(texto.find("6. Configuracion") != std::string::npos);
        CHECK(texto.find("7. Acerca de") != std::string::npos);
        CHECK(texto.find("0. Salir") != std::string::npos);
        total += 1;
    }

    // 2. PACIENTES (siete pantallas)
    {
        EscenarioCrudConCarpeta escenario("cierre_38_pacientes");
        const std::string texto = escenario.puntero->vistaTextoPlano();
        CHECK(texto.find("1. Registrar paciente") != std::string::npos);
        CHECK(texto.find("2. Modificar paciente") != std::string::npos);
        CHECK(texto.find("3. Baja / Reactivar") != std::string::npos);
        CHECK(texto.find("4. Listar todos") != std::string::npos);
        CHECK(texto.find("5. Buscar") != std::string::npos);
        CHECK(texto.find("6. Ver ficha + historial") != std::string::npos);
        CHECK(texto.find("0. Volver") != std::string::npos);
        CHECK(cantidadDeOpciones(texto) == 7);
        total += 7;
    }

    // 3. MEDICOS (siete pantallas)
    {
        EscenarioCrudMedicosConCarpeta escenario("cierre_38_medicos");
        const std::string texto = escenario.puntero->vistaTextoPlano();
        CHECK(texto.find("1. Registrar medico") != std::string::npos);
        CHECK(texto.find("2. Modificar medico") != std::string::npos);
        CHECK(texto.find("3. Baja / Reactivar") != std::string::npos);
        CHECK(texto.find("4. Listar todos") != std::string::npos);
        CHECK(texto.find("5. Buscar") != std::string::npos);
        CHECK(texto.find("6. Ver ficha + turnos") != std::string::npos);
        CHECK(texto.find("0. Volver") != std::string::npos);
        CHECK(cantidadDeOpciones(texto) == 7);
        total += 7;
    }

    // 4. TURNOS (ocho pantallas)
    {
        EscenarioCierre escenario("cierre_38_turnos");
        auto pantalla = std::make_unique<ui::Pantallas::PantallaTurnos>(
            escenario.aplicacion, escenario.turnos, escenario.pacientes, escenario.medicos,
            escenario.servicio);
        const std::string texto = pantalla->vistaTextoPlano();
        CHECK(texto.find("1. Agendar turno") != std::string::npos);
        CHECK(texto.find("2. Cancelar turno") != std::string::npos);
        CHECK(texto.find("3. Reprogramar turno") != std::string::npos);
        CHECK(texto.find("4. Modificar turno") != std::string::npos);
        CHECK(texto.find("5. Listar todos") != std::string::npos);
        CHECK(texto.find("6. Buscar") != std::string::npos);
        CHECK(texto.find("7. Ver detalle") != std::string::npos);
        CHECK(texto.find("0. Volver") != std::string::npos);
        CHECK(cantidadDeOpciones(texto) == 8);
        total += 8;
    }

    // 5. REPORTES (siete pantallas)
    {
        EscenarioCierre escenario("cierre_38_reportes");
        auto pantalla = std::make_unique<ui::Pantallas::PantallaReportes>(
            escenario.aplicacion, escenario.pacientes, escenario.medicos, escenario.servicio);
        const std::string texto = pantalla->vistaTextoPlano();
        CHECK(texto.find("1. Turnos por fecha") != std::string::npos);
        CHECK(texto.find("2. Turnos por paciente") != std::string::npos);
        CHECK(texto.find("3. Turnos por medico") != std::string::npos);
        CHECK(texto.find("4. Turnos por especialidad") != std::string::npos);
        CHECK(texto.find("5. Ocupacion por semana") != std::string::npos);
        CHECK(texto.find("6. Pacientes y medicos dados de baja") != std::string::npos);
        CHECK(texto.find("0. Volver") != std::string::npos);
        CHECK(cantidadDeOpciones(texto) == 7);
        total += 7;
    }

    // 6. CALENDARIO (cuatro pantallas)
    {
        EscenarioCierre escenario("cierre_38_calendario");
        auto pantalla = std::make_unique<ui::Pantallas::PantallaCalendario>(
            escenario.aplicacion, escenario.pacientes, escenario.medicos, escenario.servicio,
            Fecha(2027, 3, 10));
        const std::string texto = pantalla->vistaTextoPlano();
        CHECK(texto.find("1. Ver mes") != std::string::npos);
        CHECK(texto.find("2. Ver detalle del dia") != std::string::npos);
        CHECK(texto.find("3. Agenda") != std::string::npos);
        CHECK(texto.find("0. Volver") != std::string::npos);
        CHECK(cantidadDeOpciones(texto) == 4);
        total += 4;
    }

    // 7. CONFIGURACION (tres pantallas)
    {
        EscenarioCierre escenario("cierre_38_configuracion");
        ui::Pantallas::PantallaConfiguracion pantalla(escenario.aplicacion);
        const std::string texto = pantalla.vistaTextoPlano();
        CHECK(texto.find("1. Activar / desactivar colores") != std::string::npos);
        CHECK(texto.find("2. Cargar datos de ejemplo") != std::string::npos);
        CHECK(texto.find("0. Volver") != std::string::npos);
        CHECK(cantidadDeOpciones(texto) == 3);
        total += 3;
    }

    // 8. ACERCA DE (una pantalla)
    {
        EscenarioCierre escenario("cierre_38_acerca_de");
        ui::Pantallas::PantallaAcercaDe pantalla(escenario.aplicacion);
        CHECK(pantalla.titulo() == "ACERCA DE");
        total += 1;
    }

    // EL CONTEO DEL EPICO
    CHECK(total == 38);
}

// ---------------------------------------------------------------------------
// LA DEFINITION OF DONE DEL EPICO
// ---------------------------------------------------------------------------

TEST_CASE("toda baja y toda cancelacion pide confirmacion de dos pasos",
           cierre_las_confirmaciones_de_dos_pasos) {
    // LA BAJA DE UN PACIENTE
    {
        EscenarioCrudConCarpeta escenario("cierre_confirmacion_de_dos_pasos_baja");
        escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
        ui::CrudPersonas<Paciente>& motor = *escenario.puntero;

        motor.manejar(eventoCaracter('3'));
        // El buscador: se escribe el texto, el Enter busca y el Enter elige.
        motor.manejar(eventoCaracter('1'));
        motor.manejar(eventoTecla(ui::TipoTecla::Enter));
        motor.manejar(eventoTecla(ui::TipoTecla::Enter));

        // La vista de confirmacion: los DOS pasos (el '1' y el '2').
        CHECK(motor.vista() == "Confirmar");
        const std::string texto = motor.vistaTextoPlano();
        CHECK(texto.find("1. Si") != std::string::npos);
        CHECK(texto.find("2. No") != std::string::npos);

        // Y sin confirmar NO se toca nada.
        motor.manejar(eventoCaracter('2'));
        Paciente paciente;
        CHECK(escenario.pacientes.buscarPorId(1, paciente) == true);
        CHECK(paciente.esActivo() == true);
    }

    // LA CANCELACION DE UN TURNO
    {
        EscenarioCierre escenario("cierre_confirmacion_de_dos_pasos_cancelacion");
        auto pantalla = std::make_unique<ui::Pantallas::PantallaTurnos>(
            escenario.aplicacion, escenario.turnos, escenario.pacientes, escenario.medicos,
            escenario.servicio);
        ui::Pantallas::PantallaTurnos& turnos = *pantalla;
        escenario.aplicacion.ir(
            [&pantalla](ui::Aplicacion&) { return std::move(pantalla); });

        escenario.turnos.alta(
            Turno(1, 1, 1, Especialidad::CARD, fechaDesplazada(30), Hora(8, 0), 45, "A-1"));

        turnos.manejar(eventoCaracter('2'));
        turnos.manejar(eventoCaracter('1'));
        turnos.manejar(eventoTecla(ui::TipoTecla::Enter));
        turnos.manejar(eventoTecla(ui::TipoTecla::Enter));

        CHECK(turnos.vista() == "Confirmar");
        const std::string texto = turnos.vistaTextoPlano();
        CHECK(texto.find("1. Si") != std::string::npos);
        CHECK(texto.find("2. No") != std::string::npos);
    }
}

TEST_CASE("hay un solo formato de error en toda la aplicacion",
           cierre_un_solo_formato_de_error) {
    // EL FORMATO VIVE EN UN SOLO LUGAR
    //
    // Los prefijos ("OK  ", "ERR ", "AVISO ") los escribe SOLO
    // `ui::Mensaje`. Si otra pantalla los escribiera a mano, el formato se
    // desincronizaria en la primera pantalla nueva.
    const std::filesystem::path raiz = raizDelRepositorio();
    CHECK(!raiz.empty());
    if (raiz.empty()) {
        return;
    }

    for (const std::filesystem::path& archivo : archivosDeLaCapaUi(raiz)) {
        // El archivo del formato queda fuera: es el que lo define.
        if (archivo.filename() == "Mensaje.cpp" || archivo.filename() == "Mensaje.h") {
            continue;
        }
        const std::string contenido = leerArchivo(archivo);
        CHECK(contenido.find("\"ERR \"") == std::string::npos);
        CHECK(contenido.find("\"OK  \"") == std::string::npos);
        CHECK(contenido.find("\"AVISO \"") == std::string::npos);
    }

    // Y EL FORMATO ES EL QUE DICE SER
    CHECK(ui::Mensaje::error("algo").texto() == "ERR algo");
    CHECK(ui::Mensaje::exito("algo").texto() == "OK  algo");
    CHECK(ui::Mensaje::aviso("algo").texto() == "AVISO algo");
}

TEST_CASE("los errores que la aplicacion produce usan el formato unico",
           cierre_los_errores_de_la_aplicacion) {
    // UN ERROR DEL DOMINIO (el servicio rechaza un turno)
    {
        EscenarioTurnos escenario("cierre_los_errores_de_la_aplicacion");
        ui::Pantallas::PantallaTurnos& turnos = *escenario.puntero;

        // Se agenda un turno con una fecha que ya paso: el servicio lo
        // rechaza y la pantalla muestra el error DEL DOMINIO.
        escenario.pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
        escenario.medicos.alta(
            medicoConEspecialidad(1, "Beto", "Perez", "LEG-1001", Especialidad::CARD));

        turnos.manejar(eventoCaracter('1'));
        agendarConLasPersonasElegidas(turnos, "Ana", "Beto", fechaDesplazada(-5).aTexto(), "10:00");
        turnos.manejar(eventoTecla(ui::TipoTecla::Enter));

        const std::string mensaje = escenario.contexto.mensaje().texto();
        CHECK(mensaje.compare(0, 4, "ERR ") == 0);
    }

    // UN ERROR DE LA CARGA DE DATOS DE EJEMPLO
    {
        EscenarioCierre escenario("cierre_los_errores_de_la_aplicacion_carga");
        ui::Pantallas::PantallaConfiguracion pantalla(escenario.aplicacion);
        pantalla.manejar(eventoCaracter('2'));
        pantalla.manejar(eventoCaracter('1'));

        const std::string mensaje = escenario.contexto.mensaje().texto();
        CHECK(mensaje.compare(0, 4, "ERR ") == 0);
    }
}
