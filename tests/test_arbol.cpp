#include "test_framework.h"

#include "entities/Alumno.h"
#include "entities/Persona.h"
#include "structures/ArbolBST.h"

#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Tests de `ArbolBST<T>`: el arbol binario de busqueda.
//
// Los tests del ARBOL corren con tipos que NO son entidades del dominio
// (`Entero`, `PersonaDePrueba`): si el TAD solo anduviera con `Profesor`,
// `Alumno` y `Turno`, no seria un template.
//
// El TAD NO usa `operator==` en ningun punto: la busqueda y la deteccion de
// duplicados usan la EQUIVALENCIA que induce `operator<` (ni `a < b` ni
// `b < a`), que es lo que exige `std::less`.
//
// Los tests de la LISTA estan en test_lista.cpp.
// ---------------------------------------------------------------------------

struct Entero {
    int v;
    bool operator==(const Entero& otra) const { return v == otra.v; }
    bool operator<(const Entero& otra) const { return v < otra.v; }
};

// Tipo con SOLO igualdad: demuestra que la lista no exige `operator<`.

// Tipo SIN orden: el contrato del template dice que `T` tiene que traer
// `operator<`, asi que este tipo NO se puede usar en el arbol.
struct SinOrden {
    int v;
};

// Doble de `Persona`: permite meter en UN MISMO arbol un "profesor" y un
// "alumno" con el mismo id, para comprobar que el TIPO desempata el orden
// (el `operator<` de la Fase 2 ordena por `(id, tipo)`).
class PersonaDePrueba : public Persona {
public:
    PersonaDePrueba(int id_, const std::string& tipo_) {
        setId(id_);
        tipo = tipo_;
    }

    std::string getTipo() const override { return tipo; }

private:
    std::string tipo;
};

// Deteccion de `operator<` con SFINAE: sirve para documentar (y verificar)
// el contrato del template sin tener que fallar la compilacion.
template <class T, class = void>
struct TieneOperadorMenor : std::false_type {};

template <class T>
struct TieneOperadorMenor<T, std::void_t<decltype(std::declval<const T&>()
                                                  < std::declval<const T&>())>>
    : std::true_type {};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// El invariante de un BST es que sus valores, recorridos en orden, forman
// una secuencia ESTRICTAMENTE ascendente. Con la API publica alcanza con
// mirar `inOrder()`.
template <class T>
bool esAscendente(const std::vector<T>& valores) {
    for (std::size_t i = 1; i < valores.size(); ++i) {
        if (!(valores[i - 1] < valores[i])) {
            return false;
        }
    }
    return true;
}

// Permutacion determinista de 1..cantidad: el test tiene que dar el mismo
// resultado en cualquier maquina y en cualquier orden de ejecucion.
std::vector<int> permutacionDeterminista(int cantidad) {
    std::vector<int> valores(cantidad);
    for (int i = 0; i < cantidad; ++i) {
        valores[i] = i + 1;
    }
    unsigned int semilla = 20260930u;
    for (int i = cantidad - 1; i > 0; --i) {
        semilla = semilla * 1664525u + 1013904223u;
        const int j = static_cast<int>(semilla % static_cast<unsigned int>(i + 1));
        std::swap(valores[i], valores[j]);
    }
    return valores;
}

Alumno alumnoDePrueba(int id, const std::string& nombre, const std::string& matricula) {
    return Alumno(id, nombre, "Gomez", "alumno@example.com", "1122334455", matricula);
}

static_assert(TieneOperadorMenor<Entero>::value, "Entero tiene que traer operator<");
static_assert(TieneOperadorMenor<Alumno>::value, "Alumno tiene que traer operator<");
static_assert(!TieneOperadorMenor<SinOrden>::value,
              "un tipo sin operator< no se puede usar en el arbol: es lo correcto");

TEST_CASE("el arbol es generico y solo exige operator<", estructuras_contrato_operador_menor) {
    // El TAD no usa `operator==` para nada: con `Entero` (que si lo tiene) y
    // con `PersonaDePrueba` (que tambien) el arbol se comporta igual.
    ArbolBST<Entero> arbol;
    CHECK(arbol.insertar(Entero{5}));
    CHECK(arbol.insertar(Entero{3}));
    CHECK(arbol.size() == 2);
}

TEST_CASE("el arbol funciona con Alumno, que trae el operator< de la Fase 2",
          estructuras_arbol_con_alumno) {
    ArbolBST<Alumno> arbol;
    CHECK(arbol.insertar(alumnoDePrueba(3, "Ana", "A-0003")));
    CHECK(arbol.insertar(alumnoDePrueba(1, "Beto", "A-0001")));
    CHECK(arbol.insertar(alumnoDePrueba(2, "Caro", "A-0002")));
    CHECK(arbol.size() == 3);
    CHECK(arbol.buscar(alumnoDePrueba(2, "Caro", "A-0002")));

    const std::vector<Alumno> ordenados = arbol.inOrder();
    CHECK(ordenados.size() == 3);
    CHECK(ordenados[0].getId() == 1);
    CHECK(ordenados[1].getId() == 2);
    CHECK(ordenados[2].getId() == 3);

    // El id es la clave: dos alumnos con el mismo id son duplicados.
    CHECK(!arbol.insertar(alumnoDePrueba(2, "Caro", "A-0002")));
    CHECK(arbol.size() == 3);
}

// ---------------------------------------------------------------------------
// Arbol: comportamiento
// ---------------------------------------------------------------------------

TEST_CASE("un arbol nuevo esta vacio", estructuras_arbol_vacio) {
    ArbolBST<Entero> arbol;
    CHECK(arbol.vacio());
    CHECK(arbol.size() == 0);
    CHECK(arbol.altura() == 0);
    CHECK(arbol.inOrder().empty());
}

TEST_CASE("buscar en un arbol vacio devuelve false", estructuras_arbol_buscar_vacio) {
    ArbolBST<Entero> arbol;
    CHECK(!arbol.buscar(Entero{1}));
}

TEST_CASE("eliminar de un arbol vacio devuelve false", estructuras_arbol_eliminar_vacio) {
    ArbolBST<Entero> arbol;
    CHECK(!arbol.eliminar(Entero{1}));
    CHECK(arbol.size() == 0);
}

TEST_CASE("los recorridos de un arbol vacio devuelven un vector vacio",
          estructuras_arbol_recorridos_vacio) {
    ArbolBST<Entero> arbol;
    CHECK(arbol.inOrder().empty());

    int visitas = 0;
    arbol.recorrerInOrder([&visitas](const Entero&) { ++visitas; });
    CHECK(visitas == 0);
}

TEST_CASE("insertar un elemento lo deja en el arbol", estructuras_arbol_insertar_uno) {
    ArbolBST<Entero> arbol;
    CHECK(arbol.insertar(Entero{7}));
    CHECK(arbol.size() == 1);
    CHECK(!arbol.vacio());
    CHECK(arbol.altura() == 1);
    CHECK(arbol.buscar(Entero{7}));
    CHECK(!arbol.buscar(Entero{8}));
}

TEST_CASE("la insercion masiva deja todos los elementos en el arbol",
          estructuras_arbol_insercion_masiva) {
    ArbolBST<Entero> arbol;
    const int cantidad = 1000;
    for (int i = 1; i <= cantidad; ++i) {
        CHECK(arbol.insertar(Entero{i}));
    }
    CHECK(arbol.size() == static_cast<std::size_t>(cantidad));
    for (int i = 1; i <= cantidad; ++i) {
        CHECK(arbol.buscar(Entero{i}));
    }
    CHECK(!arbol.buscar(Entero{cantidad + 1}));
    CHECK(!arbol.buscar(Entero{0}));
}

TEST_CASE("insertar una clave duplicada devuelve false y no cambia el tamano",
          estructuras_arbol_duplicado) {
    ArbolBST<Entero> arbol;
    CHECK(arbol.insertar(Entero{4}));
    CHECK(arbol.insertar(Entero{9}));
    const std::size_t antes = arbol.size();

    CHECK(!arbol.insertar(Entero{4}));
    CHECK(arbol.size() == antes);
    CHECK(arbol.size() == 2);
    CHECK(arbol.inOrder().size() == 2);
}

TEST_CASE("buscar una clave inexistente devuelve false", estructuras_arbol_busqueda_inexistente) {
    ArbolBST<Entero> arbol;
    arbol.insertar(Entero{4});
    arbol.insertar(Entero{9});
    CHECK(!arbol.buscar(Entero{5}));
    CHECK(!arbol.buscar(Entero{0}));
    CHECK(!arbol.buscar(Entero{100}));
}

TEST_CASE("inOrder devuelve los valores estrictamente ascendentes",
          estructuras_arbol_in_order_ascendente) {
    ArbolBST<Entero> arbol;
    const std::vector<int> valores = permutacionDeterminista(50);
    for (const int v : valores) {
        CHECK(arbol.insertar(Entero{v}));
    }

    const std::vector<Entero> ordenados = arbol.inOrder();
    CHECK(ordenados.size() == 50);
    CHECK(esAscendente(ordenados));

    // El recorrido por callback tiene que dar lo mismo que `inOrder()`.
    std::vector<int> porCallback;
    arbol.recorrerInOrder([&porCallback](const Entero& e) { porCallback.push_back(e.v); });
    CHECK(esAscendente(porCallback));
    CHECK(std::equal(porCallback.begin(), porCallback.end(), ordenados.begin(),
                     [](const int& a, const Entero& b) { return a == b.v; }));
}

TEST_CASE("size es O(1): las consultas no recontan los nodos",
          estructuras_arbol_size_no_cambia) {
    ArbolBST<Entero> arbol;
    for (int i = 1; i <= 100; ++i) {
        arbol.insertar(Entero{i});
    }
    const std::size_t tam = arbol.size();
    for (int i = 1; i <= 100; ++i) {
        arbol.buscar(Entero{i});
        arbol.buscar(Entero{-i});
    }
    CHECK(arbol.size() == tam);
    CHECK(arbol.size() == 100);
}

// ---------------------------------------------------------------------------
// Arbol: el orden
// ---------------------------------------------------------------------------

TEST_CASE("operator< es irreflexivo", estructuras_orden_irreflexivo) {
    const Entero a{5};
    const Entero b{5};
    CHECK(!(a < a));
    CHECK(!(b < b));
    CHECK(!(a < b));
    CHECK(!(b < a));
}

TEST_CASE("operator< es asimetrico", estructuras_orden_asimetrico) {
    const Entero a{3};
    const Entero b{8};
    CHECK(a < b);
    CHECK(!(b < a));

    // La propiedad se prueba en los DOS sentidos: si `x < y`, entonces
    // `!(y < x)`.
    const Entero c{8};
    const Entero d{3};
    CHECK(d < c);
    CHECK(!(c < d));
}

TEST_CASE("operator< es transitivo", estructuras_orden_transitivo) {
    const Entero a{1};
    const Entero b{2};
    const Entero c{3};
    CHECK(a < b);
    CHECK(b < c);
    CHECK(a < c);
    CHECK(!(c < a));
}

TEST_CASE("insertar un valor equivalente a uno ya insertado devuelve false",
          estructuras_duplicado_equivalente) {
    ArbolBST<PersonaDePrueba> arbol;
    CHECK(arbol.insertar(PersonaDePrueba(7, "Alumno")));
    CHECK(!arbol.insertar(PersonaDePrueba(7, "Alumno")));
    CHECK(arbol.size() == 1);
}

TEST_CASE("el tipo desempata el orden: mismo id y distinto tipo NO es duplicado",
          estructuras_mismo_id_distinto_tipo) {
    ArbolBST<PersonaDePrueba> arbol;
    CHECK(arbol.insertar(PersonaDePrueba(7, "Alumno")));
    CHECK(arbol.insertar(PersonaDePrueba(7, "Profesor")));
    CHECK(arbol.size() == 2);
    CHECK(arbol.buscar(PersonaDePrueba(7, "Alumno")));
    CHECK(arbol.buscar(PersonaDePrueba(7, "Profesor")));

    // Y volver a insertar cualquiera de los dos SIGUE siendo duplicado.
    CHECK(!arbol.insertar(PersonaDePrueba(7, "Alumno")));
    CHECK(!arbol.insertar(PersonaDePrueba(7, "Profesor")));
    CHECK(arbol.size() == 2);
}

TEST_CASE("con un tipo sin id ni tipo el arbol tambien detecta duplicados",
          estructuras_duplicado_con_Entero) {
    ArbolBST<Entero> arbol;
    CHECK(arbol.insertar(Entero{42}));
    CHECK(!arbol.insertar(Entero{42}));
    CHECK(arbol.size() == 1);
    CHECK(arbol.buscar(Entero{42}));
}

// ---------------------------------------------------------------------------
// Arbol: eliminacion (los tres casos)
// ---------------------------------------------------------------------------

TEST_CASE("eliminar una hoja", estructuras_eliminar_hoja) {
    ArbolBST<Entero> arbol;
    arbol.insertar(Entero{5});
    arbol.insertar(Entero{3});
    arbol.insertar(Entero{7});

    CHECK(arbol.eliminar(Entero{3}));
    CHECK(arbol.size() == 2);
    CHECK(!arbol.buscar(Entero{3}));
    CHECK(arbol.buscar(Entero{5}));
    CHECK(arbol.buscar(Entero{7}));
    CHECK(esAscendente(arbol.inOrder()));
}

TEST_CASE("eliminar un nodo con un hijo", estructuras_eliminar_un_hijo) {
    ArbolBST<Entero> arbol;
    arbol.insertar(Entero{5});
    arbol.insertar(Entero{3});
    arbol.insertar(Entero{7});
    arbol.insertar(Entero{8});  // hijo derecho de 7

    CHECK(arbol.eliminar(Entero{7}));
    CHECK(arbol.size() == 3);
    CHECK(!arbol.buscar(Entero{7}));
    CHECK(arbol.buscar(Entero{8}));  // el hijo subio en lugar del padre
    CHECK(esAscendente(arbol.inOrder()));
}

TEST_CASE("eliminar un nodo con dos hijos: el sucesor lo reemplaza",
          estructuras_eliminar_dos_hijos) {
    ArbolBST<Entero> arbol;
    arbol.insertar(Entero{5});
    arbol.insertar(Entero{3});
    arbol.insertar(Entero{7});
    arbol.insertar(Entero{6});
    arbol.insertar(Entero{8});

    CHECK(arbol.eliminar(Entero{7}));
    CHECK(arbol.size() == 4);
    CHECK(!arbol.buscar(Entero{7}));
    CHECK(arbol.buscar(Entero{6}));
    CHECK(arbol.buscar(Entero{8}));
    CHECK(esAscendente(arbol.inOrder()));
}

TEST_CASE("eliminar la raiz", estructuras_eliminar_raiz) {
    ArbolBST<Entero> arbol;
    arbol.insertar(Entero{5});
    arbol.insertar(Entero{3});
    arbol.insertar(Entero{7});
    arbol.insertar(Entero{6});

    CHECK(arbol.eliminar(Entero{5}));
    CHECK(arbol.size() == 3);
    CHECK(!arbol.buscar(Entero{5}));
    CHECK(arbol.buscar(Entero{3}));
    CHECK(arbol.buscar(Entero{6}));
    CHECK(arbol.buscar(Entero{7}));
    CHECK(esAscendente(arbol.inOrder()));
}

TEST_CASE("eliminar una clave inexistente devuelve false y no toca el arbol",
          estructuras_eliminar_inexistente) {
    ArbolBST<Entero> arbol;
    arbol.insertar(Entero{5});
    arbol.insertar(Entero{7});
    const std::vector<Entero> antes = arbol.inOrder();

    CHECK(!arbol.eliminar(Entero{6}));
    CHECK(arbol.size() == 2);
    CHECK(arbol.inOrder() == antes);
}

TEST_CASE("tras cada eliminacion el tamano baja en 1 y el invariante se mantiene",
          estructuras_invariante_bst_tras_eliminar) {
    ArbolBST<Entero> arbol;
    const std::vector<int> valores = permutacionDeterminista(30);
    for (const int v : valores) {
        arbol.insertar(Entero{v});
    }
    CHECK(arbol.size() == 30);

    // Se eliminan de a uno, del mas chico al mas grande.
    for (int i = 1; i <= 30; ++i) {
        const std::size_t antes = arbol.size();
        CHECK(arbol.eliminar(Entero{i}));
        CHECK(arbol.size() == antes - 1);
        CHECK(esAscendente(arbol.inOrder()));
    }
    CHECK(arbol.vacio());
    CHECK(arbol.altura() == 0);
}

// ---------------------------------------------------------------------------
// Arbol: construccion balanceada (la respuesta a la degeneracion)
// ---------------------------------------------------------------------------

TEST_CASE("construir desde ordenados deja el tamano correcto",
          estructuras_construir_desde_ordenados_tamanio) {
    std::vector<Entero> ordenados;
    for (int i = 1; i <= 1000; ++i) {
        ordenados.push_back(Entero{i});
    }

    ArbolBST<Entero> arbol;
    arbol.construirDesdeOrdenados(ordenados);
    CHECK(arbol.size() == 1000);
    CHECK(!arbol.vacio());
    CHECK(arbol.altura() >= 1);
}

TEST_CASE("el arbol degenera al insertar en orden y se balancea al construir",
          estructuras_construir_desde_ordenados_altura) {
    const int cantidad = 1000;
    std::vector<Entero> ordenados;
    for (int i = 1; i <= cantidad; ++i) {
        ordenados.push_back(Entero{i});
    }

    // Camino 1: insercion incremental, que es lo que degenerationa el arbol
    // cuando los ids son secuenciales (1, 2, 3...).
    ArbolBST<Entero> degenerado;
    for (const Entero& e : ordenados) {
        degenerado.insertar(e);
    }
    CHECK(degenerado.size() == static_cast<std::size_t>(cantidad));
    CHECK(degenerado.altura() == cantidad);

    // Camino 2: construccion balanceada desde la misma secuencia.
    ArbolBST<Entero> balanceado;
    balanceado.construirDesdeOrdenados(ordenados);
    CHECK(balanceado.altura() <= 11);
    CHECK(balanceado.altura() * 100 <= degenerado.altura());
}

TEST_CASE("construir desde ordenados devuelve los valores en orden ascendente",
          estructuras_construir_desde_ordenados_in_order) {
    std::vector<Entero> ordenados;
    for (int i = 1; i <= 1000; ++i) {
        ordenados.push_back(Entero{i});
    }

    ArbolBST<Entero> arbol;
    arbol.construirDesdeOrdenados(ordenados);

    const std::vector<Entero> salida = arbol.inOrder();
    CHECK(salida.size() == 1000);
    CHECK(esAscendente(salida));
    for (std::size_t i = 0; i < salida.size(); ++i) {
        CHECK(salida[i].v == static_cast<int>(i) + 1);
    }
}

TEST_CASE("construir desde ordenados deja todos los elementos encontrables",
          estructuras_construir_desde_ordenados_busqueda) {
    std::vector<Entero> ordenados;
    for (int i = 1; i <= 1000; ++i) {
        ordenados.push_back(Entero{i});
    }

    ArbolBST<Entero> arbol;
    arbol.construirDesdeOrdenados(ordenados);
    for (int i = 1; i <= 1000; ++i) {
        CHECK(arbol.buscar(Entero{i}));
    }
    CHECK(!arbol.buscar(Entero{1001}));
}

TEST_CASE("construir desde un vector vacio deja el arbol vacio",
          estructuras_construir_desde_ordenados_vacio) {
    ArbolBST<Entero> arbol;
    arbol.insertar(Entero{5});  // el arbol tiene contenido previo
    arbol.construirDesdeOrdenados(std::vector<Entero>{});
    CHECK(arbol.vacio());
    CHECK(arbol.size() == 0);
    CHECK(arbol.altura() == 0);
    CHECK(arbol.inOrder().empty());
}

TEST_CASE("construir desde un vector con un solo elemento da altura 1",
          estructuras_construir_desde_ordenados_un_elemento) {
    ArbolBST<Entero> arbol;
    arbol.construirDesdeOrdenados(std::vector<Entero>{Entero{7}});
    CHECK(arbol.size() == 1);
    CHECK(arbol.altura() == 1);
    CHECK(arbol.buscar(Entero{7}));
}

TEST_CASE("la altura de un arbol balanceado es log2(n)", estructuras_altura_balanceada) {
    // Un BST plano NO se balancea al insertar: con claves en orden aleatorio
    // la altura es O(log n) en promedio, pero NO es log2(n) exacto. El
    // balanceo exacto lo garantiza `construirDesdeOrdenados()`.
    const int cantidad = 1000;
    std::vector<Entero> ordenados;
    for (int i = 1; i <= cantidad; ++i) {
        ordenados.push_back(Entero{i});
    }

    ArbolBST<Entero> balanceado;
    balanceado.construirDesdeOrdenados(ordenados);
    CHECK(balanceado.altura() <= 11);
    CHECK(balanceado.altura() >= 9);

    // La misma cantidad de elementos, insertados en orden aleatorio, da una
    // altura de la misma magnitud (O(log n)), pero MUCHOS mas que 1000.
    ArbolBST<Entero> aleatorio;
    const std::vector<int> valores = permutacionDeterminista(cantidad);
    for (const int v : valores) {
        aleatorio.insertar(Entero{v});
    }
    CHECK(aleatorio.size() == static_cast<std::size_t>(cantidad));
    CHECK(aleatorio.altura() < 100);
    CHECK(aleatorio.altura() > balanceado.altura());
}

// ---------------------------------------------------------------------------
// Arbol: memoria
// ---------------------------------------------------------------------------

TEST_CASE("destruir el arbol no deja nodos vivos", estructuras_arbol_memoria_sin_fugas) {
    {
        ArbolBST<Entero> arbol;
        for (int i = 1; i <= 1000; ++i) {
            arbol.insertar(Entero{i});
        }
        CHECK(ArbolBST<Entero>::nodosVivos() == 1000);
    }
    CHECK(ArbolBST<Entero>::nodosVivos() == 0);
}

TEST_CASE("copiar el arbol duplica los nodos vivos y al destruir ambos vuelve a cero",
          estructuras_arbol_memoria_copia) {
    {
        ArbolBST<Entero> arbol;
        for (int i = 1; i <= 5; ++i) {
            arbol.insertar(Entero{i});
        }
        CHECK(ArbolBST<Entero>::nodosVivos() == 5);

        ArbolBST<Entero> copia = arbol;
        CHECK(ArbolBST<Entero>::nodosVivos() == 10);
        CHECK(copia.size() == 5);
    }
    CHECK(ArbolBST<Entero>::nodosVivos() == 0);
}

TEST_CASE("la copia del arbol es independiente de la original",
          estructuras_arbol_copia_independiente) {
    ArbolBST<Entero> original;
    original.insertar(Entero{1});
    original.insertar(Entero{2});

    ArbolBST<Entero> copia = original;
    CHECK(copia.insertar(Entero{3}));
    CHECK(original.size() == 2);
    CHECK(copia.size() == 3);
    CHECK(!original.buscar(Entero{3}));
    CHECK(copia.buscar(Entero{3}));

    // Y al revés: modificar la original no toca la copia.
    CHECK(original.insertar(Entero{4}));
    CHECK(copia.size() == 3);
    CHECK(!copia.buscar(Entero{4}));
    CHECK(original.size() == 3);
}

TEST_CASE("la asignacion de copia no deja fugas", estructuras_arbol_asignacion_sin_fuga) {
    {
        ArbolBST<Entero> origen;
        for (int i = 1; i <= 4; ++i) {
            origen.insertar(Entero{i});
        }
        CHECK(ArbolBST<Entero>::nodosVivos() == 4);

        ArbolBST<Entero> destino;
        destino = origen;
        CHECK(ArbolBST<Entero>::nodosVivos() == 8);
        CHECK(destino.size() == 4);
    }
    CHECK(ArbolBST<Entero>::nodosVivos() == 0);
}

TEST_CASE("el movimiento roba la raiz y no cambia el numero de nodos vivos",
          estructuras_arbol_movimiento) {
    {
        ArbolBST<Entero> origen;
        for (int i = 1; i <= 3; ++i) {
            origen.insertar(Entero{i});
        }
        CHECK(ArbolBST<Entero>::nodosVivos() == 3);

        ArbolBST<Entero> destino = std::move(origen);
        CHECK(destino.size() == 3);
        CHECK(destino.buscar(Entero{2}));
        CHECK(origen.vacio());
        CHECK(origen.size() == 0);
        CHECK(ArbolBST<Entero>::nodosVivos() == 3);
    }
    CHECK(ArbolBST<Entero>::nodosVivos() == 0);
}

TEST_CASE("la asignacion de movimiento libera lo que habia", estructuras_arbol_movimiento_asignacion) {
    {
        ArbolBST<Entero> origen;
        origen.insertar(Entero{1});
        origen.insertar(Entero{2});

        ArbolBST<Entero> destino;
        destino.insertar(Entero{99});
        destino.insertar(Entero{98});
        destino.insertar(Entero{97});
        CHECK(ArbolBST<Entero>::nodosVivos() == 5);

        destino = std::move(origen);
        CHECK(ArbolBST<Entero>::nodosVivos() == 2);
        CHECK(destino.size() == 2);
        CHECK(origen.vacio());
    }
    CHECK(ArbolBST<Entero>::nodosVivos() == 0);
}

TEST_CASE("cada instanciacion del TAD tiene su propio contador de nodos",
          estructuras_arbol_contadores_independientes) {
    CHECK(ArbolBST<Entero>::nodosVivos() == 0);
    CHECK(ArbolBST<Alumno>::nodosVivos() == 0);

    {
        ArbolBST<Entero> arbolEntero;
        ArbolBST<Alumno> arbolAlumno;
        arbolEntero.insertar(Entero{1});
        arbolAlumno.insertar(alumnoDePrueba(1, "Ana", "A-0001"));

        CHECK(ArbolBST<Entero>::nodosVivos() == 1);
        CHECK(ArbolBST<Alumno>::nodosVivos() == 1);
    }

    CHECK(ArbolBST<Entero>::nodosVivos() == 0);
    CHECK(ArbolBST<Alumno>::nodosVivos() == 0);
}
