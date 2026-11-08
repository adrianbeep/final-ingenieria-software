#include "test_framework.h"

#include "ui/Selector.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Tests del SELECTOR.
//
// El selector es la pieza que hace que un dato se ELIJE en vez de escribirse:
// el estado invalido es irrepresentable. Lo que se verifica aca:
//
//   - la seleccion inicial;
//   - la navegacion con flechas (que NO da la vuelta, como en el `Menu`);
//   - el numero escrito, que puede ser de DOS digitos ("12" elige la
//     doceava opcion): es lo que un `Menu` no puede hacer;
//   - el error de un numero que no existe;
//   - el Enter sin numero escrito;
//   - el retroceso;
//   - el hit-testing del raton;
//   - el texto plano.
//
// El widget es LOGICA PURA: no hace falta terminal para probarlo.
// ---------------------------------------------------------------------------

namespace {

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

// Las doce opciones del catalogo: el caso que hace falta el numero de dos
// digitos (un `Menu` no podria mostrarlas: sus atajos son de un digito).
std::vector<std::string> doceOpciones() {
    return {"Uno",   "Dos",  "Tres", "Cuatro", "Cinco", "Seis",
            "Siete", "Ocho", "Nueve", "Diez",  "Once",  "Doce"};
}

} // namespace

TEST_CASE("el selector arranca con la primera opcion elegida",
           selector_seleccion_inicial) {
    ui::Selector selector("Especialidad", {"Cardiologia", "Medicina General"});

    CHECK(selector.cantidad() == 2);
    CHECK(selector.titulo() == "Especialidad");
    CHECK(selector.opciones().size() == 2);
    CHECK(selector.opciones()[0] == "Cardiologia");
    CHECK(selector.seleccion() == 0);
    CHECK(selector.error().empty() == true);
    CHECK(selector.numeroDigitado().empty() == true);
}

TEST_CASE("las flechas mueven la eleccion y no dan la vuelta",
           selector_navegacion_con_flechas) {
    ui::Selector selector("Especialidad", {"Uno", "Dos", "Tres"});

    CHECK(selector.moverAbajo() == true);
    CHECK(selector.seleccion() == 1);
    CHECK(selector.moverAbajo() == true);
    CHECK(selector.seleccion() == 2);

    // Abajo en la ultima opcion: la seleccion NO da la vuelta.
    CHECK(selector.moverAbajo() == false);
    CHECK(selector.seleccion() == 2);

    CHECK(selector.moverArriba() == true);
    CHECK(selector.seleccion() == 1);

    // Arriba en la primera opcion: tampoco da la vuelta.
    CHECK(selector.moverArriba() == true);
    CHECK(selector.seleccion() == 0);
    CHECK(selector.moverArriba() == false);
    CHECK(selector.seleccion() == 0);
}

TEST_CASE("las flechas del selector se aplican con el evento",
           selector_eventos_de_flecha) {
    ui::Selector selector("Especialidad", {"Uno", "Dos"});

    ui::Evento abajo;
    abajo.tipo = ui::TipoTecla::FlechaAbajo;
    CHECK(selector.aplicarEvento(abajo) == true);
    CHECK(selector.seleccion() == 1);

    ui::Evento arriba;
    arriba.tipo = ui::TipoTecla::FlechaArriba;
    CHECK(selector.aplicarEvento(arriba) == true);
    CHECK(selector.seleccion() == 0);
}

TEST_CASE("escribir el numero completo elige esa opcion", selector_numero_completo) {
    ui::Selector selector("Especialidad", doceOpciones());

    // "12" elige la DOCEAVA opcion: el numero se escribe COMPLETO.
    CHECK(selector.aplicarEvento(eventoCaracter('1')) == true);
    CHECK(selector.aplicarEvento(eventoCaracter('2')) == true);
    CHECK(selector.numeroDigitado() == "12");
    // Escribir el numero NO mueve el resaltado: la eleccion pasa a ser del
    // numero recien escrito.
    CHECK(selector.seleccion() == 0);

    CHECK(selector.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);
    CHECK(selector.seleccion() == 11);
    CHECK(selector.numeroDigitado().empty() == true);
    CHECK(selector.error().empty() == true);
}

TEST_CASE("un numero de un digito elige esa opcion", selector_numero_de_un_digito) {
    ui::Selector selector("Especialidad", doceOpciones());

    CHECK(selector.aplicarEvento(eventoCaracter('3')) == true);
    CHECK(selector.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);
    CHECK(selector.seleccion() == 2);
    CHECK(selector.error().empty() == true);
}

TEST_CASE("un numero que no existe deja el error y no cambia la eleccion",
           selector_numero_invalido) {
    ui::Selector selector("Especialidad", doceOpciones());
    selector.seleccionar(4);

    // "99" no es ninguna de las doce opciones.
    CHECK(selector.aplicarEvento(eventoCaracter('9')) == true);
    CHECK(selector.aplicarEvento(eventoCaracter('9')) == true);
    CHECK(selector.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);

    // El error se muestra...
    CHECK(selector.error().empty() == false);
    CHECK(selector.error().find("opcion invalida") != std::string::npos);
    // ...la eleccion NO cambia...
    CHECK(selector.seleccion() == 4);
    // ...y el numero written SE QUEDA, para que el usuario lo corrija.
    CHECK(selector.numeroDigitado() == "99");
}

TEST_CASE("el cero es un numero mas y no una opcion", selector_cero_invalido) {
    ui::Selector selector("Especialidad", doceOpciones());

    CHECK(selector.aplicarEvento(eventoCaracter('0')) == true);
    CHECK(selector.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);

    CHECK(selector.error().empty() == false);
    CHECK(selector.seleccion() == 0);
}

TEST_CASE("el Enter sin numero escrito acepta la opcion resaltada",
           selector_enter_acepta_la_resaltada) {
    ui::Selector selector("Especialidad", doceOpciones());
    selector.seleccionar(7);

    CHECK(selector.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);

    // La eleccion no cambia: se acepta la que estaba resaltada.
    CHECK(selector.seleccion() == 7);
    CHECK(selector.numeroDigitado().empty() == true);
    CHECK(selector.error().empty() == true);
}

TEST_CASE("el retroceso borra el ultimo digito", selector_retroceso) {
    ui::Selector selector("Especialidad", doceOpciones());

    selector.aplicarEvento(eventoCaracter('1'));
    selector.aplicarEvento(eventoCaracter('2'));
    CHECK(selector.numeroDigitado() == "12");

    CHECK(selector.aplicarEvento(eventoTecla(ui::TipoTecla::Retroceso)) == true);
    CHECK(selector.numeroDigitado() == "1");

    // Con "1" escrito, el Enter elige la PRIMERA opcion.
    selector.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));
    CHECK(selector.seleccion() == 0);
    CHECK(selector.error().empty() == true);
}

TEST_CASE("un tercer digito se ignora", selector_tercer_digito) {
    ui::Selector selector("Especialidad", doceOpciones());

    selector.aplicarEvento(eventoCaracter('1'));
    selector.aplicarEvento(eventoCaracter('2'));
    selector.aplicarEvento(eventoCaracter('3'));

    // Con dos digitos ya se alcanzan las doce opciones: el tercero no.
    CHECK(selector.numeroDigitado() == "12");
}

TEST_CASE("las flechas borran el numero que se estaba escribiendo",
           selector_flecha_borra_el_numero) {
    ui::Selector selector("Especialidad", doceOpciones());

    selector.aplicarEvento(eventoCaracter('1'));
    CHECK(selector.numeroDigitado() == "1");

    selector.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaAbajo));
    CHECK(selector.numeroDigitado().empty() == true);
}

TEST_CASE("un indice fuera de rango se ignora", selector_indice_fuera_de_rango) {
    ui::Selector selector("Especialidad", doceOpciones());

    selector.seleccionar(3);
    CHECK(selector.seleccion() == 3);

    // Un indice que no existe NO rompe nada: la seleccion no cambia.
    selector.seleccionar(-1);
    CHECK(selector.seleccion() == 3);
    selector.seleccionar(12);
    CHECK(selector.seleccion() == 3);
    selector.seleccionar(999);
    CHECK(selector.seleccion() == 3);
}

TEST_CASE("setOpciones reemplaza la lista y vuelve al estado inicial",
           selector_set_opciones) {
    ui::Selector selector("Especialidad", doceOpciones());

    selector.seleccionar(5);
    CHECK(selector.seleccion() == 5);

    selector.setOpciones({"Uno", "Dos"});

    // LA LISTA ES LA NUEVA.
    CHECK(selector.cantidad() == 2);
    CHECK(selector.opciones()[0] == "Uno");
    CHECK(selector.opciones()[1] == "Dos");
    // LA SELECCION VUELVE A LA PRIMERA.
    CHECK(selector.seleccion() == 0);
    CHECK(selector.error().empty() == true);
}

TEST_CASE("setOpciones borra el numero digitado", selector_set_opciones_borra_el_numero) {
    ui::Selector selector("Especialidad", doceOpciones());

    // El usuario esta escribiendo el numero de una opcion.
    selector.aplicarEvento(eventoCaracter('1'));
    selector.aplicarEvento(eventoCaracter('2'));
    CHECK(selector.numeroDigitado() == "12");

    // La lista cambia: el numero "12" ya no apunta a la misma opcion.
    selector.setOpciones(doceOpciones());

    // EL NUMERO SE BORRA
    //
    // Es la misma regla que aplica mover la seleccion con las flechas: si no
    // se borrara, el "12" escrito llevaria a otra entidad.
    CHECK(selector.numeroDigitado().empty() == true);
}

TEST_CASE("setOpciones borra el error de la lista anterior",
           selector_set_opciones_borra_el_error) {
    ui::Selector selector("Especialidad", doceOpciones());

    // Un numero que no existe deja el error.
    selector.aplicarEvento(eventoCaracter('9'));
    selector.aplicarEvento(eventoCaracter('9'));
    selector.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));
    CHECK(selector.error().empty() == false);

    selector.setOpciones(doceOpciones());

    // El error describia la lista anterior: no aplica a la nueva.
    CHECK(selector.error().empty() == true);
}

TEST_CASE("setOpciones con una lista vacia deja la seleccion en cero",
           selector_set_opciones_vacia) {
    ui::Selector selector("Especialidad", doceOpciones());
    selector.seleccionar(4);

    selector.setOpciones({});

    CHECK(selector.cantidad() == 0);
    CHECK(selector.seleccion() == 0);
    CHECK(selector.numeroDigitado().empty() == true);
}

TEST_CASE("el Escape no lo consume el selector", selector_escape_no_consumido) {
    ui::Selector selector("Especialidad", doceOpciones());

    CHECK(selector.aplicarEvento(eventoTecla(ui::TipoTecla::Escape)) == false);
    // Una letra tampoco lo consume.
    CHECK(selector.aplicarEvento(eventoCaracter('a')) == false);
    // Ni un caracter de mas de un byte.
    ui::Evento conTilde = eventoCaracter('a');
    conTilde.caracter = "\xC3\xB1";
    CHECK(selector.aplicarEvento(conTilde) == false);
}

TEST_CASE("el hit-testing del raton devuelve la opcion de la fila",
           selector_hit_testing) {
    ui::Selector selector("Especialidad", doceOpciones());
    selector.definirFilaInicial(4);

    CHECK(selector.filaInicial() == 4);
    CHECK(selector.opcionEnFila(4, selector.filaInicial()) == 0);
    CHECK(selector.opcionEnFila(6, selector.filaInicial()) == 2);
    CHECK(selector.opcionEnFila(15, selector.filaInicial()) == 11);

    // Fuera de la lista no hay ninguna opcion.
    CHECK(selector.opcionEnFila(3, selector.filaInicial()) == -1);
    CHECK(selector.opcionEnFila(16, selector.filaInicial()) == -1);
}

TEST_CASE("el texto plano muestra la numeracion y la opcion elegida",
           selector_texto_plano) {
    ui::Selector selector("Especialidad", {"Uno", "Dos", "Tres"});
    selector.seleccionar(1);

    const std::string texto = selector.textoPlano();

    // El titulo arriba y las opciones una por linea, con la numeracion
    // POSICIONAL (1, 2, 3, ...).
    CHECK(texto.find("Especialidad") == 0);
    CHECK(texto.find("\n  1. Uno") != std::string::npos);
    CHECK(texto.find("\n  2. Dos (elegida)") != std::string::npos);
    CHECK(texto.find("\n  3. Tres") != std::string::npos);
    // La elegida es la UNICA marcada.
    CHECK(texto.find("(elegida)") != std::string::npos);
    CHECK(texto.find("(elegida)", texto.find("(elegida)") + 1) == std::string::npos);
}

TEST_CASE("el texto plano numera las doce opciones", selector_texto_plano_de_doce) {
    ui::Selector selector("Especialidad", doceOpciones());
    selector.seleccionar(11);

    const std::string texto = selector.textoPlano();
    CHECK(texto.find("\n  1. Uno") != std::string::npos);
    CHECK(texto.find("\n  9. Nueve") != std::string::npos);
    CHECK(texto.find("\n  10. Diez") != std::string::npos);
    CHECK(texto.find("\n  11. Once") != std::string::npos);
    CHECK(texto.find("\n  12. Doce (elegida)") != std::string::npos);
}
