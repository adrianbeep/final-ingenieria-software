#include "test_framework.h"

#include "structures/ListaEnlazada.h"

#include <algorithm>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Tests de `ListaEnlazada<T>`: la lista simplemente enlazada.
//
// La lista NO ordena y NO necesita `operator<`: con un tipo que solo tiene
// `operator==` funciona igual. Eso la diferencia del arbol, que si necesita
// un orden.
//
// Los tests del ARBOL estan en test_arbol.cpp.
// ---------------------------------------------------------------------------

// Tipo minimo: un orden y una igualdad. Sirve para el recorrido normal.
struct Entero {
    int v;
    bool operator==(const Entero& otra) const { return v == otra.v; }
    bool operator<(const Entero& otra) const { return v < otra.v; }
};

// Tipo con SOLO igualdad: demuestra que la lista no exige `operator<`.
struct SoloIgualdad {
    int v;
    bool operator==(const SoloIgualdad& otra) const { return v == otra.v; }
};

TEST_CASE("una lista nueva esta vacia", estructuras_lista_vacia) {
    ListaEnlazada<Entero> lista;
    CHECK(lista.vacio());
    CHECK(lista.size() == 0);
    CHECK(lista.begin() == lista.end());
}

TEST_CASE("insertarFinal conserva el orden de insercion", estructuras_lista_orden_insercion) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{3});
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});

    CHECK(lista.size() == 3);
    CHECK(!lista.vacio());

    std::vector<int> vistos;
    for (const Entero& e : lista) {
        vistos.push_back(e.v);
    }
    CHECK(vistos.size() == 3);
    CHECK(vistos[0] == 3);
    CHECK(vistos[1] == 1);
    CHECK(vistos[2] == 2);
}

TEST_CASE("la lista se recorre con range-for", estructuras_lista_range_for) {
    ListaEnlazada<Entero> lista;
    for (int i = 1; i <= 5; ++i) {
        lista.insertarFinal(Entero{i});
    }

    int suma = 0;
    for (const Entero& e : lista) {
        suma += e.v;
    }
    CHECK(suma == 15);
}

TEST_CASE("con al menos un elemento begin es distinto de end",
          estructuras_lista_begin_distinto_de_end) {
    ListaEnlazada<Entero> lista;
    CHECK(lista.begin() == lista.end());
    lista.insertarFinal(Entero{1});
    CHECK(lista.begin() != lista.end());
    CHECK((*lista.begin()).v == 1);
}

TEST_CASE("eliminar un elemento del medio, de la cabeza y del final",
          estructuras_lista_eliminar) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});
    lista.insertarFinal(Entero{3});

    CHECK(lista.eliminar(Entero{2}));  // del medio
    CHECK(lista.size() == 2);

    CHECK(lista.eliminar(Entero{1}));  // de la cabeza
    CHECK(lista.size() == 1);

    CHECK(lista.eliminar(Entero{3}));  // del final
    CHECK(lista.size() == 0);
    CHECK(lista.vacio());
    CHECK(lista.begin() == lista.end());
}

TEST_CASE("eliminar la cabeza actualiza el puntero al ultimo",
          estructuras_lista_eliminar_cabeza_actualiza_ultimo) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});

    CHECK(lista.eliminar(Entero{1}));
    CHECK(lista.size() == 1);

    // Si el puntero al ultimo no se hubiera actualizado, esta insercion
    // escribiria sobre un nodo ya liberado.
    lista.insertarFinal(Entero{3});
    CHECK(lista.size() == 2);

    std::vector<int> vistos;
    for (const Entero& e : lista) {
        vistos.push_back(e.v);
    }
    CHECK(vistos.size() == 2);
    CHECK(vistos[0] == 2);
    CHECK(vistos[1] == 3);
}

TEST_CASE("eliminar el ultimo actualiza el puntero al ultimo",
          estructuras_lista_eliminar_final_actualiza_ultimo) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});

    CHECK(lista.eliminar(Entero{2}));  // era el ultimo
    CHECK(lista.size() == 1);

    lista.insertarFinal(Entero{3});
    CHECK(lista.size() == 2);

    std::vector<int> vistos;
    for (const Entero& e : lista) {
        vistos.push_back(e.v);
    }
    CHECK(vistos.size() == 2);
    CHECK(vistos[0] == 1);
    CHECK(vistos[1] == 3);
}

TEST_CASE("eliminar un valor inexistente devuelve false", estructuras_lista_eliminar_inexistente) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});

    CHECK(!lista.eliminar(Entero{9}));
    CHECK(lista.size() == 2);
}

TEST_CASE("el post-incremento del iterador devuelve la copia anterior",
          estructuras_lista_post_incremento) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});

    ListaEnlazada<Entero>::Iterador it = lista.begin();
    CHECK((*it).v == 1);

    const ListaEnlazada<Entero>::Iterador copia = it++;
    CHECK((*it).v == 2);      // el iterador avanzo
    CHECK((*copia).v == 1);   // la copia quedo en el nodo anterior
}

TEST_CASE("el iterador funciona con los algoritmos de la biblioteca",
          estructuras_lista_iterador_algoritmos) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});
    lista.insertarFinal(Entero{3});

    const auto encontrado = std::find(lista.begin(), lista.end(), Entero{2});
    CHECK(encontrado != lista.end());
    CHECK((*encontrado).v == 2);

    const int pares = std::count_if(lista.begin(), lista.end(),
                                     [](const Entero& e) { return e.v % 2 == 0; });
    CHECK(pares == 1);
}

TEST_CASE("la copia de la lista es independiente de la original",
          estructuras_lista_copia_profunda) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});
    lista.insertarFinal(Entero{3});

    {
        ListaEnlazada<Entero> copia = lista;
        CHECK(copia.size() == 3);
        CHECK(ListaEnlazada<Entero>::nodosVivos() == 6);

        CHECK(copia.eliminar(Entero{2}));
        CHECK(copia.size() == 2);
        CHECK(lista.size() == 3);  // la original no se toco
    }

    CHECK(ListaEnlazada<Entero>::nodosVivos() == 3);
}

TEST_CASE("la asignacion de copia de la lista no deja fugas",
          estructuras_lista_asignacion_sin_fuga) {
    {
        ListaEnlazada<Entero> origen;
        origen.insertarFinal(Entero{1});
        origen.insertarFinal(Entero{2});

        ListaEnlazada<Entero> destino;
        destino.insertarFinal(Entero{9});
        destino = origen;

        CHECK(ListaEnlazada<Entero>::nodosVivos() == 4);
        CHECK(destino.size() == 2);
    }
    CHECK(ListaEnlazada<Entero>::nodosVivos() == 0);
}

TEST_CASE("destruir la lista no deja nodos vivos", estructuras_lista_memoria) {
    {
        ListaEnlazada<Entero> lista;
        for (int i = 1; i <= 100; ++i) {
            lista.insertarFinal(Entero{i});
        }
        CHECK(ListaEnlazada<Entero>::nodosVivos() == 100);
    }
    CHECK(ListaEnlazada<Entero>::nodosVivos() == 0);
}

TEST_CASE("el movimiento de la lista roba la cadena", estructuras_lista_movimiento) {
    {
        ListaEnlazada<Entero> origen;
        origen.insertarFinal(Entero{1});
        origen.insertarFinal(Entero{2});

        ListaEnlazada<Entero> destino = std::move(origen);
        CHECK(destino.size() == 2);
        CHECK(origen.vacio());
        CHECK(origen.size() == 0);
        CHECK(ListaEnlazada<Entero>::nodosVivos() == 2);
    }
    CHECK(ListaEnlazada<Entero>::nodosVivos() == 0);
}

TEST_CASE("la lista solo necesita operator==: no exige operator<",
          estructuras_lista_solo_igualdad) {
    ListaEnlazada<SoloIgualdad> lista;
    lista.insertarFinal(SoloIgualdad{1});
    lista.insertarFinal(SoloIgualdad{2});
    CHECK(lista.size() == 2);
    CHECK(lista.eliminar(SoloIgualdad{1}));
    CHECK(lista.size() == 1);
    CHECK(!lista.eliminar(SoloIgualdad{1}));
}

// ---------------------------------------------------------------------------
// RECORRIDO EN UN CONTEXTO `const`
//
// La capa de persistencia recibe la lista por `const&`: sin iteradores
// constantes, la lista no se podria recorrer ahi.
// ---------------------------------------------------------------------------

// El iterador de una lista CONSTANTE tiene que devolver `const T&`: si
// devolviera `T&`, una lista constante se podria modificar a traves del
// recorrido. Es una comprobacion de COMPILACION: si el tipo cambia, el
// proyecto deja de compilar.
static_assert(std::is_same<decltype(*std::declval<const ListaEnlazada<Entero>&>().begin()),
                           const Entero&>::value,
              "el iterador de una lista constante tiene que dar 'const Entero&'");

// Y el de una lista que NO es constante sigue dando `T&`: el recorrido de
// lectura no le saca el poder de escribir.
static_assert(std::is_same<decltype(*std::declval<ListaEnlazada<Entero>&>().begin()),
                           Entero&>::value,
              "el iterador de una lista mutable tiene que dar 'Entero&'");

TEST_CASE("la lista se recorre en un contexto const", estructuras_lista_recorrido_const) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{3});
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});

    // La lista llega por `const&`: es como la recibe la capa de
    // persistencia.
    const ListaEnlazada<Entero>& constante = lista;

    std::vector<int> vistos;
    for (const Entero& e : constante) {
        vistos.push_back(e.v);
    }
    CHECK(vistos.size() == 3);
    CHECK(vistos[0] == 3);
    CHECK(vistos[1] == 1);
    CHECK(vistos[2] == 2);
}

TEST_CASE("el iterador constante no modifica la lista ni invalida nada",
          estructuras_lista_iterador_const_no_invalida) {
    ListaEnlazada<Entero> lista;
    for (int i = 1; i <= 5; ++i) {
        lista.insertarFinal(Entero{i});
    }

    const std::size_t tamanoAntes = lista.size();
    const int nodosAntes = ListaEnlazada<Entero>::nodosVivos();

    const ListaEnlazada<Entero>& constante = lista;
    for (auto it = constante.cbegin(); it != constante.cend(); ++it) {
        CHECK((*it).v >= 1);
        CHECK((*it).v <= 5);
    }

    // Recorrer no creo nodos, no borro nodos y no cambio el tamano: la
    // lista quedo EXACTAMENTE como estaba.
    CHECK(lista.size() == tamanoAntes);
    CHECK(ListaEnlazada<Entero>::nodosVivos() == nodosAntes);
}

TEST_CASE("el iterador constante funciona con los algoritmos de la biblioteca",
          estructuras_lista_iterador_const_algoritmos) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});
    lista.insertarFinal(Entero{3});

    const ListaEnlazada<Entero>& constante = lista;

    const auto encontrado = std::find(constante.begin(), constante.end(), Entero{2});
    CHECK(encontrado != constante.end());
    CHECK((*encontrado).v == 2);

    const int pares = std::count_if(constante.begin(), constante.end(),
                                     [](const Entero& e) { return e.v % 2 == 0; });
    CHECK(pares == 1);
}

TEST_CASE("aVector devuelve los mismos elementos en el mismo orden",
          estructuras_lista_a_vector) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{3});
    lista.insertarFinal(Entero{1});
    lista.insertarFinal(Entero{2});

    const std::vector<Entero> vector = lista.aVector();
    CHECK(vector.size() == 3);
    CHECK(vector[0].v == 3);
    CHECK(vector[1].v == 1);
    CHECK(vector[2].v == 2);

    // Y el vector tiene los MISMOS elementos que la lista, no solo el mismo
    // tamano: el recorrido es el de la lista.
    std::size_t i = 0;
    for (const Entero& e : lista) {
        CHECK(i < vector.size());
        if (i < vector.size()) {
            CHECK(e.v == vector[i].v);
        }
        ++i;
    }
    CHECK(i == vector.size());
}

TEST_CASE("aVector de una lista vacia es un vector vacio",
          estructuras_lista_a_vector_vacia) {
    const ListaEnlazada<Entero> lista;
    CHECK(lista.aVector().empty());
}

TEST_CASE("aVector de una lista constante tambien funciona",
          estructuras_lista_a_vector_const) {
    ListaEnlazada<Entero> lista;
    lista.insertarFinal(Entero{7});

    const ListaEnlazada<Entero>& constante = lista;
    const std::vector<Entero> vector = constante.aVector();
    CHECK(vector.size() == 1);
    CHECK(vector[0].v == 7);
}
