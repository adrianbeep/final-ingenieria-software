#include "test_framework.h"

#include "ui/Buscador.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Tests del BUSCADOR.
//
// El buscador es la pieza que reemplaza al prompt del id: se escribe un dato
// que el usuario CONOCE (el nombre, el apellido, el dni) y se elige una de
// las coincidencias. Lo que se verifica aca:
//
//   - los DOS focos (el texto y la lista) y como se pasa de uno al otro;
//   - que la busqueda corre al CONFIRMAR (no en cada tecla);
//   - que se elige con las flechas o escribiendo el numero;
//   - el error de una busqueda sin coincidencias;
//   - que el id elegido es el de la ENTIDAD y no el de la posicion;
//   - el hit-testing del raton y el texto plano.
//
// La busqueda es un FAKE: las reglas de verdad son las del repositorio y las
// prueba la pantalla (en `tests/test_ui.cpp`).
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

// Una busqueda de mentira: devuelve SIEMPRE las mismas coincidencias.
ui::Buscador::Busqueda busquedaFija(std::vector<ui::Buscador::Coincidencia> coincidencias) {
    return [coincidencias](const std::string&) { return coincidencias; };
}

// Una busqueda de mentira que NO encuentra nada.
ui::Buscador::Busqueda busquedaVacia() {
    return [](const std::string&) { return std::vector<ui::Buscador::Coincidencia>(); };
}

// Tres coincidencias de prueba.
//
// Los ids NO son 1, 2 y 3: son 7, 9 y 12. Asi se verifica que el id elegido
// es el de la ENTIDAD y no el de la posicion de la lista.
std::vector<ui::Buscador::Coincidencia> tresCoincidencias() {
    return {{7, {"7", "García", "Ana", "2020391"}},
            {9, {"9", "López", "Martín", "2020392"}},
            {12, {"12", "Sosa", "Camila", "2020393"}}};
}

// Escribe un texto en el buscador (un evento por caracter).
void escribir(ui::Buscador& buscador, const std::string& texto) {
    for (const char caracter : texto) {
        buscador.aplicarEvento(eventoCaracter(caracter));
    }
}

} // namespace

TEST_CASE("el buscador arranca con el foco en el texto y sin coincidencias",
           buscador_inicial) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI", busquedaVacia());

    CHECK(buscador.titulo() == "Buscar paciente");
    CHECK(buscador.filtro().empty() == true);
    // EL FOCO ARRANCA EN EL TEXTO: es lo primero que se usa.
    CHECK(buscador.focoEnElTexto() == true);
    CHECK(buscador.error().empty() == true);
    CHECK(buscador.elegido() == false);
    CHECK(buscador.idElegido() == -1);
    CHECK(buscador.coincidencias().empty() == true);
    CHECK(buscador.cantidadDeLineas() == 0);
}

TEST_CASE("el buscador abre con la lista de la busqueda vacia", buscador_lista_inicial) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    // EL USUARIO TIENE QUE VER QUE HAY ANTES DE ELEGIR
    //
    // El buscador se abre para elegir: si arrancara con la lista vacia, el
    // usuario veria una pantalla en blanco.
    CHECK(buscador.cantidadDeLineas() == 3);
    // PERO EL FOCO ESTA EN EL TEXTO: al abrir, el usuario viene a ESCRIBIR.
    CHECK(buscador.focoEnElTexto() == true);
    // Y NO HAY ERROR: el usuario todavia no intento nada.
    CHECK(buscador.error().empty() == true);
    CHECK(buscador.elegido() == false);
}

TEST_CASE("escribir y enter buscan: la lista se actualiza", buscador_busca_al_confirmar) {
    // La busqueda devuelve coincidencias SOLO si el texto no esta vacio: asi
    // se ve la diferencia entre la lista inicial y la de la busqueda.
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          [](const std::string& texto) {
                              if (texto.empty()) {
                                  return std::vector<ui::Buscador::Coincidencia>();
                              }
                              return tresCoincidencias();
                          });

    // LA LISTA INICIAL (la busqueda vacia) no tiene nada.
    CHECK(buscador.cantidadDeLineas() == 0);

    escribir(buscador, "Ana");
    CHECK(buscador.filtro() == "Ana");
    // AUN NO SE BUSCO: la lista sigue como estaba (se busca al confirmar).
    CHECK(buscador.cantidadDeLineas() == 0);

    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);

    // LA LISTA SE LLENO Y EL FOCO PASA A LA LISTA.
    CHECK(buscador.cantidadDeLineas() == 3);
    CHECK(buscador.coincidencias().size() == 3);
    CHECK(buscador.focoEnElTexto() == false);
    CHECK(buscador.error().empty() == true);
    // El texto NO se borra: el usuario puede corregirlo.
    CHECK(buscador.filtro() == "Ana");
}

TEST_CASE("la busqueda recibe el texto escrito", buscador_busca_con_el_filtro) {
    // La busqueda es la parte que la pantalla arma: aca se verifica que el
    // widget le pasa el texto que el usuario escribio.
    std::string recibido;
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          [&recibido](const std::string& texto) {
                              recibido = texto;
                              return std::vector<ui::Buscador::Coincidencia>();
                          });

    escribir(buscador, "Gar");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    CHECK(recibido == "Gar");
}

TEST_CASE("elegir con las flechas", buscador_elegir_con_flechas) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "a");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    // La flecha ABAJO mueve el resaltado (y borra el numero escrito).
    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaAbajo)) == true);
    CHECK(buscador.seleccion() == 1);

    // El Enter elige la coincidencia resaltada.
    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);
    CHECK(buscador.elegido() == true);
    // EL ID ES EL DE LA ENTIDAD (9), NO EL DE LA POSICION (2).
    CHECK(buscador.idElegido() == 9);
}

TEST_CASE("elegir escribiendo el numero", buscador_elegir_con_numero) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "a");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    // El numero se escribe en la LISTA (aca es el numero de una opcion).
    CHECK(buscador.aplicarEvento(eventoCaracter('3')) == true);
    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);

    CHECK(buscador.elegido() == true);
    // La TERCERA coincidencia: el id es el de la entidad (12).
    CHECK(buscador.idElegido() == 12);
}

TEST_CASE("el elegido es POR EVENTO: la tecla siguiente no vuelve a elegir",
           buscador_elegido_es_por_evento) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "a");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    // El Enter elige.
    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);
    CHECK(buscador.elegido() == true);
    CHECK(buscador.idElegido() == 7);

    // EL ELEGIDO SE REINICIA EN CADA EVENTO
    //
    // El flag dice "elegido en ESTE evento". Si quedara prendido, al volver a
    // la vista (por ejemplo, del "No" de una confirmacion) la siguiente tecla
    // volveria a disparar la accion.
    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::FlechaAbajo)) == true);
    CHECK(buscador.elegido() == false);
    CHECK(buscador.idElegido() == -1);
}

TEST_CASE("un numero que no existe deja el error y no elige", buscador_numero_invalido) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "a");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    CHECK(buscador.aplicarEvento(eventoCaracter('9')) == true);
    CHECK(buscador.aplicarEvento(eventoCaracter('9')) == true);
    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter)) == true);

    // EL ERROR SE MUESTRA Y NO SE ELIGE NADA.
    CHECK(buscador.error().find("opcion invalida") != std::string::npos);
    CHECK(buscador.elegido() == false);
    CHECK(buscador.idElegido() == -1);
}

TEST_CASE("el retroceso vuelve al texto para corregirlo", buscador_retroceso_al_texto) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "Ana");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));
    CHECK(buscador.focoEnElTexto() == false);

    // El Retroceso en la LISTA vuelve al TEXTO (no borra un numero).
    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Retroceso)) == true);
    CHECK(buscador.focoEnElTexto() == true);
    // El texto sigue ahi: lo que se corrige es el filtro.
    CHECK(buscador.filtro() == "Ana");
}

TEST_CASE("sin coincidencias el error es 'no hay coincidencias' y vuelve al texto",
           buscador_sin_coincidencias) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI", busquedaVacia());

    escribir(buscador, "Zzz");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    CHECK(buscador.error() == "no hay coincidencias");
    CHECK(buscador.cantidadDeLineas() == 0);
    // NO HAY NADA QUE ELEGIR: el foco vuelve al texto.
    CHECK(buscador.focoEnElTexto() == true);
    CHECK(buscador.elegido() == false);
}

TEST_CASE("una busqueda nueva invalida la eleccion anterior", buscador_resetea_la_eleccion) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "Ana");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));
    CHECK(buscador.elegido() == true);

    // Se busca otra vez: la entidad elegida era de la lista anterior.
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Retroceso));
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    CHECK(buscador.elegido() == false);
    CHECK(buscador.idElegido() == -1);
}

TEST_CASE("la linea de la coincidencia une las celdas", buscador_linea_de_la_coincidencia) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "Ana");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    // LAS CELDAS EN UNA SOLA LINEA, SEPARADAS POR UN PUNTO.
    CHECK(buscador.lineaDe(0) == "7 · García · Ana · 2020391");
    CHECK(buscador.lineaDe(2) == "12 · Sosa · Camila · 2020393");
    // Un indice fuera de rango no rompe nada.
    CHECK(buscador.lineaDe(9).empty() == true);
    CHECK(buscador.lineaDe(-1).empty() == true);
}

TEST_CASE("la linea del campo muestra el texto con el cursor", buscador_linea_del_campo) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI", busquedaVacia());

    escribir(buscador, "Ana");
    CHECK(buscador.lineaDelCampo() == "Nombre, apellido o DNI: Ana|");
}

TEST_CASE("el escape no lo consume el buscador", buscador_escape_no_consumido) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    // Con el foco en el TEXTO.
    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Escape)) == false);

    escribir(buscador, "Ana");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    // Y con el foco en la LISTA.
    CHECK(buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Escape)) == false);
}

TEST_CASE("el hit-testing del raton devuelve la coincidencia de la fila",
           buscador_hit_testing) {
    // Una busqueda que no encuentra nada: la lista esta vacia y no hay
    // ninguna fila que tocar.
    ui::Buscador buscadorVacio("Buscar paciente", "Nombre, apellido o DNI", busquedaVacia());
    CHECK(buscadorVacio.coincidenciaEnFila(4, 4) == -1);

    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "Ana");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    buscador.definirFilaInicial(4);

    CHECK(buscador.filaInicial() == 4);
    CHECK(buscador.coincidenciaEnFila(4, buscador.filaInicial()) == 0);
    CHECK(buscador.coincidenciaEnFila(6, buscador.filaInicial()) == 2);

    // Fuera de la lista no hay ninguna coincidencia.
    CHECK(buscador.coincidenciaEnFila(3, buscador.filaInicial()) == -1);
    CHECK(buscador.coincidenciaEnFila(7, buscador.filaInicial()) == -1);
}

TEST_CASE("un clic mueve el resaltado y no elige", buscador_raton_no_elige) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "Ana");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    // EL RATON NO LO MANEJA EL BUSCADOR
    //
    // El hit-testing lo hace la pantalla (que sabe donde dibujo cada
    // coincidencia): el widget no lo consume.
    ui::Evento clic;
    clic.boton = ui::BotonMouse::Izquierdo;
    clic.x = 4;
    clic.y = 4;
    CHECK(buscador.aplicarEvento(clic) == false);

    // PERO SI SE PUEDE MOVER EL RESALTADO (lo hace la pantalla).
    buscador.seleccionar(2);
    CHECK(buscador.seleccion() == 2);

    // Un indice fuera de rango se ignora.
    buscador.seleccionar(99);
    CHECK(buscador.seleccion() == 2);
}

TEST_CASE("el texto plano muestra el campo y las lineas", buscador_texto_plano) {
    ui::Buscador buscador("Buscar paciente", "Nombre, apellido o DNI",
                          busquedaFija(tresCoincidencias()));

    escribir(buscador, "Ana");
    buscador.aplicarEvento(eventoTecla(ui::TipoTecla::Enter));

    const std::string texto = buscador.textoPlano();
    CHECK(texto.find("Buscar paciente") == 0);
    CHECK(texto.find("Nombre, apellido o DNI: Ana|") != std::string::npos);
    CHECK(texto.find("1. 7 · García · Ana · 2020391") != std::string::npos);
    CHECK(texto.find("3. 12 · Sosa · Camila · 2020393") != std::string::npos);
    // La resaltada es la UNICA marcada (en texto plano no hay invertidas).
    CHECK(texto.find("(elegida)") != std::string::npos);
    CHECK(texto.find("(elegida)", texto.find("(elegida)") + 1) == std::string::npos);
}
