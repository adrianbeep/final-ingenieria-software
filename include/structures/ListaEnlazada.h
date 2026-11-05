#pragma once

#include <cstddef>
#include <iterator>

// ---------------------------------------------------------------------------
// ListaEnlazada<T>: lista simplemente enlazada, escrita a mano.
//
// ES EL HISTORIAL DE TURNOS: recorrido secuencial, orden de insercion.
//
// ---------------------------------------------------------------------------
// POR QUE UNA LISTA Y NO UN `std::vector`
//
// Porque el requisito 7 de la consigna pide una estructura IMPLEMENTADA POR
// EL PACIENTE, y porque el caso de uso real calza: el historial es append-only
// (crece al final), de solo lectura y sin busquedas.
//
//   `std::vector<T>`                  `ListaEnlazada<T>`
//   -------------------------------   ---------------------------------
//   insertarFinal: O(1) amortizado,   insertarFinal: O(1) con puntero al
//   pero REALLOCA el buffer:          ultimo, sin reallocar nunca
//   invalida iteradores y punteros
//   recorrer: O(n) cache-friendly     recorrer: O(n) con salto de punteros
//
// La respuesta honesta: para el historial, un vector seria mas eficiente. La
// lista esta porque el requisito la pide y porque el historial es el caso
// canonico de una lista. No es una decision de rendimiento.
//
// ---------------------------------------------------------------------------
// EL CONTRATO DEL TIPO `T`
//
// El TAD necesita SOLO que `T` sea copiable y que tenga
// `bool operator==(const T&) const` (lo usa `eliminar`). NO necesita
// `operator<`: la lista no ordena, conserva el ORDEN DE INSERCION.
//
// ---------------------------------------------------------------------------
// EL PUNTERO AL ULTIMO NODO (decision S5)
//
// `insertarFinal()` es O(1) porque la lista sabe donde termina. El precio es
// que hay que mantener ese puntero coherente al eliminar y al copiar: un
// `_ultimo` que apunta a un nodo de otra lista es un use-after-free
// esperando. Por eso la copia profunda vuelve a calcular el ultimo y el
// copy-and-swap intercambia la cabeza y el ultimo JUNTOS.
// ---------------------------------------------------------------------------
template <class T>
class ListaEnlazada {
private:
    // Nodo de la lista: guarda el valor y el siguiente. No tiene puntero al
    // anterior: la lista es de un solo sentido.
    //
    // Va declarado antes que `Iterador` porque el iterador maneja punteros a
    // nodos, y un puntero a un tipo incompleto es valido, pero leer sus
    // campos no.
    struct Nodo {
        T valor;
        Nodo* sig;
        explicit Nodo(const T& valor_) : valor(valor_), sig(nullptr) {}
    };

public:
    // ------------------------------------------------------------------------
    // Iterador propio, compatible con `range-for` y con los algoritmos de la
    // biblioteca que trabajan con iteradores de advancement (`std::find`,
    // `std::count_if`, `std::for_each`...).
    //
    // `end()` es el centinela `nullptr`: no hay un nodo vacio al final.
    //
    // NOTA: el iterador es de categoria FORWARD, no de acceso aleatorio. Por
    // eso `std::sort` NO se puede usar sobre el rango: exige iteradores de
    // acceso aleatorio, y el recorrido de una lista es inherentemente
    // secuencial. `std::find`, `std::count_if` y `std::for_each` si
    // funcionan.
    // ------------------------------------------------------------------------
    class Iterador {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        Iterador() = default;
        explicit Iterador(Nodo* nodo_) : nodo(nodo_) {}

        reference operator*() const { return nodo->valor; }
        pointer operator->() const { return &nodo->valor; }

        Iterador& operator++() {
            nodo = nodo->sig;
            return *this;
        }

        // El post-incremento COPIA ANTES de avanzar. Si se avanzara primero y
        // despues se copiara, el iterador devuelto apuntaria al nodo
        // siguiente: un bug silencioso que solo aparece en los bucles.
        Iterador operator++(int) {
            Iterador copia = *this;
            ++(*this);
            return copia;
        }

        bool operator==(const Iterador& otro) const { return nodo == otro.nodo; }
        bool operator!=(const Iterador& otro) const { return nodo != otro.nodo; }

    private:
        Nodo* nodo = nullptr;
        friend class ListaEnlazada;
    };

    // --- Regla de Cinco: las CINCO operaciones, todas explicitas ---
    ListaEnlazada() = default;
    ~ListaEnlazada() { liberar(); }
    ListaEnlazada(const ListaEnlazada& otro);
    ListaEnlazada& operator=(const ListaEnlazada& otro);
    ListaEnlazada(ListaEnlazada&& otro) noexcept;
    ListaEnlazada& operator=(ListaEnlazada&& otro) noexcept;

    // Inserta al final. O(1) gracias al puntero al ultimo nodo.
    void insertarFinal(const T& valor_);

    // Elimina la PRIMERA aparicion del valor. Devuelve false si no estaba.
    bool eliminar(const T& valor_);

    // --- Consultas ---
    std::size_t size() const { return tam; }  // O(1)
    bool vacio() const { return tam == 0; }    // O(1)

    // --- Recorrido ---
    Iterador begin() { return Iterador(cabeza); }
    Iterador end() { return Iterador(nullptr); }

    // Nodos vivos de esta instanciacion: el detector de fugas portable
    // (decision S4). Es `static` DENTRO de una plantilla, asi que hay un
    // contador por `T`.
    static int nodosVivos() { return contador(); }

private:
    Nodo* cabeza = nullptr;
    Nodo* ultimo = nullptr;
    std::size_t tam = 0;

    // --- Contador de nodos vivos ---
    static int& contador() {
        static int c = 0;
        return c;
    }

    static Nodo* nuevo(const T& valor_) {
        ++contador();
        return new Nodo(valor_);
    }

    static void liberarNodo(Nodo* nodo) {
        delete nodo;
        --contador();
    }

    // Copia profunda de la cadena. Devuelve la cabeza copiada y deja en
    // `fin` el ULTIMO nodo copiado (nullptr si la lista estaba vacia): sin
    // esto, el puntero al ultimo quedaria apuntando a la lista original.
    static Nodo* copiar(const Nodo* origen, Nodo*& fin) {
        if (!origen) {
            fin = nullptr;
            return nullptr;
        }
        if (!origen->sig) {
            // Ultimo de la cadena: se crea y se ancla en `fin`.
            Nodo* nodo = nuevo(origen->valor);
            fin = nodo;
            return nodo;
        }
        Nodo* nodo = nuevo(origen->valor);
        nodo->sig = copiar(origen->sig, fin);
        return nodo;
    }

    // Libra toda la cadena y deja la lista vacia.
    void liberar() {
        Nodo* actual = cabeza;
        while (actual) {
            Nodo* siguiente = actual->sig;
            liberarNodo(actual);
            actual = siguiente;
        }
        cabeza = nullptr;
        ultimo = nullptr;
        tam = 0;
    }

    // Intercambia el estado de dos listas. Se usa en el copy-and-swap: la
    // cabeza y el ultimo se intercambian JUNTOS, asi que cada lista se
    // queda con su propia cadena y su propio puntero al ultimo.
    void intercambiar(ListaEnlazada& otro) {
        Nodo* cabezaAux = cabeza;
        cabeza = otro.cabeza;
        otro.cabeza = cabezaAux;

        Nodo* ultimoAux = ultimo;
        ultimo = otro.ultimo;
        otro.ultimo = ultimoAux;

        std::size_t tamAux = tam;
        tam = otro.tam;
        otro.tam = tamAux;
    }
};

// ---------------------------------------------------------------------------
// Definiciones fuera de la clase (un template no tiene .cpp propio).
// ---------------------------------------------------------------------------

// COPIA PROFUNDA: nodos nuevos, no compartidos.
template <class T>
ListaEnlazada<T>::ListaEnlazada(const ListaEnlazada& otro) : tam(otro.tam) {
    cabeza = copiar(otro.cabeza, ultimo);
}

// COPY-AND-SWAP.
template <class T>
ListaEnlazada<T>& ListaEnlazada<T>::operator=(const ListaEnlazada& otro) {
    if (this != &otro) {
        ListaEnlazada copia(otro);
        intercambiar(copia);
    }
    return *this;
}

// ROBO: la lista de origen queda vacia.
template <class T>
ListaEnlazada<T>::ListaEnlazada(ListaEnlazada&& otro) noexcept
    : cabeza(otro.cabeza), ultimo(otro.ultimo), tam(otro.tam) {
    otro.cabeza = nullptr;
    otro.ultimo = nullptr;
    otro.tam = 0;
}

// MOVIMIENTO POR ASIGNACION: primero se libera lo que habia (si no, fuga).
template <class T>
ListaEnlazada<T>& ListaEnlazada<T>::operator=(ListaEnlazada&& otro) noexcept {
    if (this != &otro) {
        liberar();
        cabeza = otro.cabeza;
        ultimo = otro.ultimo;
        tam = otro.tam;
        otro.cabeza = nullptr;
        otro.ultimo = nullptr;
        otro.tam = 0;
    }
    return *this;
}

template <class T>
void ListaEnlazada<T>::insertarFinal(const T& valor_) {
    Nodo* nodo = nuevo(valor_);
    if (!ultimo) {
        // Lista vacia: el nodo nuevo es la cabeza y el ultimo.
        cabeza = nodo;
    } else {
        ultimo->sig = nodo;
    }
    ultimo = nodo;
    ++tam;
}

template <class T>
bool ListaEnlazada<T>::eliminar(const T& valor_) {
    Nodo* anterior = nullptr;
    Nodo* actual = cabeza;
    while (actual) {
        if (actual->valor == valor_) {
            if (anterior) {
                anterior->sig = actual->sig;
            } else {
                // Se elimina la cabeza.
                cabeza = actual->sig;
            }
            if (ultimo == actual) {
                // Se elimino el ultimo: el nuevo ultimo es el anterior (o
                // nullptr si la lista quedo vacia). Si no se actualizara,
                // el puntero al ultimo quedaria colgando de un nodo
                // liberado: use-after-free.
                ultimo = anterior;
            }
            liberarNodo(actual);
            --tam;
            return true;
        }
        anterior = actual;
        actual = actual->sig;
    }
    return false;
}
