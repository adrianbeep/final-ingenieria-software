#pragma once

#include <cstddef>
#include <vector>

// ---------------------------------------------------------------------------
// ArbolBST<T>: arbol binario de busqueda, escrito a mano.
//
// ES EL BACKING DE LAS TRES COLECCIONES DEL SISTEMA (profesores, alumnos y
// turnos): el acceso es POR CLAVE (el id), que es unico y comparable.
//
// ---------------------------------------------------------------------------
// POR QUE PUNTEROS CRUDOS Y NO `std::unique_ptr` (decision S1)
//
// El requisito 7 de la consigna pide una estructura IMPLEMENTADA POR EL
// ALUMNO. Con `unique_ptr` el `operator=` de copia queda eliminado (el TAD no
// se podria copiar) y el contador de nodos vivos (el detector de fugas
// portable, decision S4) no se podria escribir sin instrumentar el `deleter`.
// Con punteros crudos hay que escribir a mano las CINCO operaciones de la
// Regla de Cinco, que es justamente lo que se aprende con este ejercicio.
//
// ---------------------------------------------------------------------------
// EL CONTRATO DEL TIPO `T`
//
// El TAD necesita SOLO dos cosas de `T`:
//   1. `bool operator<(const T&) const` -- define el ORDEN.
//   2. Que sea copiable (constructor de copia y de asignacion).
//
// NO necesita `operator==`: la busqueda y la deteccion de duplicados usan la
// EQUIVALENCIA que induce el orden, es decir, "ni `a < b` ni `b < a`".
// Esa es la misma nocion de "mismo elemento" que usa `std::less`, y por eso
// el orden tiene que ser un STRICT WEAK ORDERING (irreflexivo, asimetrico y
// transitivo). En el dominio lo cumple `Persona::operator<`, que ordena por
// `(id, tipo)`; para cualquier otro `T` alcanza con que su `operator<` sea un
// orden estricto.
//
// ---------------------------------------------------------------------------
// POR QUE NO HAY EXCEPCIONES DE DOMINIO EN ESTE TAD (decision S2)
//
// El TAD es generico sobre `T` y no puede conocer `IdDuplicadoException`:
// devuelve `bool` y deja la decision a quien llama (el repositorio, Fase 5).
//
// ---------------------------------------------------------------------------
// POR QUE EL ARBOL NO SE BALANCEA AL INSERTAR
//
// `insertar()` es un BST plano: la altura es O(h) y en el peor caso O(n).
// Con ids secuenciales (1, 2, 3...) insertados en orden ascendente el arbol
// se degenera en una cadena. Por eso existe la SEGUNDA PUERTA,
// `construirDesdeOrdenados()`, que construye el arbol balanceado en O(n) con
// divide y conqueras: es la que usa el repositorio al cargar el archivo de
// datos, que viene ordenado por id.
// ---------------------------------------------------------------------------
template <class T>
class ArbolBST {
public:
    // --- Regla de Cinco: las CINCO operaciones, todas explicitas ---
    //
    // Declarar el destructor suprime el constructor de movimiento implicito:
    // sin esto, un `ArbolBST b = std::move(a)` copiaria en vez de mover.
    ArbolBST() = default;
    ~ArbolBST() { liberar(); }
    ArbolBST(const ArbolBST& otro);
    ArbolBST& operator=(const ArbolBST& otro);
    ArbolBST(ArbolBST&& otro) noexcept;
    ArbolBST& operator=(ArbolBST&& otro) noexcept;

    // Inserta un valor. Devuelve FALSE si el valor es EQUIVALENTE a uno ya
    // insertado (mismo `id` y mismo `tipo` en el dominio), TRUE si se
    // inserted. NO lanza excepciones (decision S2).
    bool insertar(const T& valor_);

    // Busca un valor por equivalencia. Devuelve true si esta.
    bool buscar(const T& valor_) const;

    // Busca por una CLAVE, comparada contra el valor almacenado con el
    // COMPARADOR que da el llamador.
    //
    // EL CONTRATO DEL COMPARADOR: recibe (clave, valor) y devuelve
    //   < 0 si la clave es MENOR que el valor
    //   = 0 si la clave y el valor son EQUIVALENTES
    //   > 0 si la clave es MAYOR que el valor
    // O sea: el mismo criterio de tres vias de `std::less`, pero entre la
    // clave y el valor, no entre dos valores.
    //
    // POR QUE EXISTE
    //
    // El arbol indexa por el `operator<` del TIPO COMPLETO, asi que
    // `buscar()` solo puede preguntar por un `T` entero. Un repositorio,
    // en cambio, necesita preguntar por una PARTE del valor (el id): para
    // preguntar "existe el #7" con `buscar()` habria que construir una
    // entidad de prueba COMPLETA (con nombre, apellido, email, telefono y
    // matricula validos, porque los setters lanzan `DatoInvalidoException`),
    // solo para descartar ese dato en la comparacion.
    //
    // Este metodo separa la CLAVE del VALOR: el arbol sigue siendo
    // generico (no sabe que es un id ni que existe el dominio) y el
    // COMPARADOR es el que aporta el conocimiento de la parte.
    //
    // COMPLEJIDAD: O(altura). Es un DESCENSO por el arbol, igual que
    // `buscar()`: no recorre el arbol entero, asi que sobre un arbol
    // balanceado la busqueda es O(log n).
    //
    // `fuera` solo se escribe si la clave esta: si no esta, devuelve false
    // y NO lo toca. No modifica el arbol y no lanza excepciones.
    template <class K, class Comparador>
    bool buscarPorClave(const K& clave, Comparador comparador, T& fuera) const;

    // Elimina un valor por equivalencia. Devuelve false si no estaba.
    bool eliminar(const T& valor_);

    // --- Consultas ---
    std::size_t size() const { return tam; }  // O(1): es el contador, no un recorrido
    bool vacio() const { return tam == 0; }    // O(1)
    int altura() const;                        // O(n): solo para diagnostico

    // --- Recorridos ---
    //
    // `inOrder()` devuelve los valores ORDENADOS por clave: es la propiedad
    // que define a un BST y lo que va a producir el archivo de datos.
    // `recorrerInOrder()` es el caso general (callback) y `inOrder()` se
    // apoya en el: no hay dos formas de hacer lo mismo.
    std::vector<T> inOrder() const;
    template <class F> void recorrerInOrder(F&& visitante) const;

    // Construye el arbol BALANCEADO a partir de una secuencia ORDENADA por
    // `operator<` (divide y conqueras: el elemento del medio es la raiz).
    // REEMPLAZA el contenido actual del arbol. O(n) en nodos, altura
    // ~log2(n).
    void construirDesdeOrdenados(std::vector<T> ordenados);

    // Nodos vivos de esta instanciacion: el detector de fugas portable
    // (decision S4). Es `static` DENTRO de una plantilla, asi que hay un
    // contador por `T`: el de `ArbolBST<Profesor>` es independiente del de
    // `ArbolBST<Alumno>`.
    static int nodosVivos() { return contador(); }

private:
    // El nodo NO conoce al arbol: no tiene puntero al padre. Eso simplifica
    // el borrado y evita el clasico puntero al padre que queda colgando.
    struct Nodo {
        T valor;
        Nodo* izq;
        Nodo* der;
        explicit Nodo(const T& valor_) : valor(valor_), izq(nullptr), der(nullptr) {}
    };

    Nodo* raiz = nullptr;
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

    // Libra un subarbol COMPLETO (recursivo): lo usa el destructor.
    static void liberarTodo(Nodo* nodo) {
        if (!nodo) {
            return;
        }
        liberarTodo(nodo->izq);
        liberarTodo(nodo->der);
        delete nodo;
        --contador();
    }

    // Libra UN SOLO nodo, sin sus hijos: lo usa `eliminar`, que en los casos
    // de cero y de un hijo ya reengancho el subarbol superviviente.
    static void liberarNodo(Nodo* nodo) {
        delete nodo;
        --contador();
    }

    // Copia profunda: crea un arbol NUEVO con los mismos valores. Nada de
    // compartir nodos: compartir es lo que produce el double free.
    static Nodo* copiar(const Nodo* origen) {
        if (!origen) {
            return nullptr;
        }
        Nodo* nodo = nuevo(origen->valor);
        nodo->izq = copiar(origen->izq);
        nodo->der = copiar(origen->der);
        return nodo;
    }

    // Libra todo el contenido y deja el arbol vacio.
    void liberar() {
        liberarTodo(raiz);
        raiz = nullptr;
        tam = 0;
    }

    // Intercambia el estado de dos arboles. Se usa en el copy-and-swap: al
    // intercambiar la raiz y el tamano COMPLETOS, cada arbol se queda con
    // sus propios nodos (nunca con los de otro).
    void intercambiar(ArbolBST& otro) {
        Nodo* raizAux = raiz;
        raiz = otro.raiz;
        otro.raiz = raizAux;

        std::size_t tamAux = tam;
        tam = otro.tam;
        otro.tam = tamAux;
    }

    // --- Insercion recursiva ---
    //
    // El nodo NO conoce al padre, asi que la funcion devuelve la raiz (nueva
    // o no) del subarbol y el llamador la reengancha.
    //
    // La comparacion es la EQUIVALENCIA del orden: si el valor nuevo es
    // menor, se baja a la izquierda; si es mayor, a la derecha; y si NINGUNO
    // es menor que el otro, son equivalentes y el valor ya estaba.
    static Nodo* insertarEn(Nodo* nodo, const T& valor_, bool& duplicado) {
        if (!nodo) {
            return nuevo(valor_);
        }
        if (valor_ < nodo->valor) {
            nodo->izq = insertarEn(nodo->izq, valor_, duplicado);
        } else if (nodo->valor < valor_) {
            nodo->der = insertarEn(nodo->der, valor_, duplicado);
        } else {
            duplicado = true;  // equivalentes: el valor ya estaba
        }
        return nodo;
    }

    // --- Eliminacion recursiva: los tres casos ---
    //
    //   - sin hijos: se reemplaza el nodo por nullptr.
    //   - un hijo: el hijo ocupa el lugar del nodo.
    //   - dos hijos: el SUCESOR en orden (el minimo del subarbol derecho)
    //     copia su valor al nodo y se elimina de la derecha.
    static Nodo* eliminarEn(Nodo* nodo, const T& valor_, bool& eliminado) {
        if (!nodo) {
            return nullptr;
        }
        if (valor_ < nodo->valor) {
            nodo->izq = eliminarEn(nodo->izq, valor_, eliminado);
            return nodo;
        }
        if (nodo->valor < valor_) {
            nodo->der = eliminarEn(nodo->der, valor_, eliminado);
            return nodo;
        }

        // Equivalentes: este es el nodo a eliminar.
        eliminado = true;

        if (!nodo->izq) {
            Nodo* victima = nodo;
            nodo = nodo->der;
            liberarNodo(victima);
            return nodo;
        }
        if (!nodo->der) {
            Nodo* victima = nodo;
            nodo = nodo->izq;
            liberarNodo(victima);
            return nodo;
        }

        // Dos hijos: el sucesor en orden reemplaza al nodo.
        Nodo* sucesor = nodo->der;
        while (sucesor->izq) {
            sucesor = sucesor->izq;
        }
        nodo->valor = sucesor->valor;

        bool descartado = false;
        nodo->der = eliminarEn(nodo->der, sucesor->valor, descartado);
        return nodo;
    }

    // --- Recorridos recursivos ---
    template <class F>
    static void recorrerEn(const Nodo* nodo, F& visitante) {
        if (!nodo) {
            return;
        }
        recorrerEn(nodo->izq, visitante);
        visitante(nodo->valor);
        recorrerEn(nodo->der, visitante);
    }

    static int alturaDe(const Nodo* nodo) {
        if (!nodo) {
            return 0;
        }
        const int alturaIzq = alturaDe(nodo->izq);
        const int alturaDer = alturaDe(nodo->der);
        return 1 + (alturaIzq > alturaDer ? alturaIzq : alturaDer);
    }

    // --- Construccion balanceada: divide y conqueras ---
    //
    // El elemento del MEDIO es la raiz y el resto se reparte en dos mitades:
    // el arbol queda balanceado y se construye en O(n).
    static Nodo* construirDesde(const std::vector<T>& ordenados, std::size_t desde,
                                std::size_t hasta) {
        if (desde >= hasta) {
            return nullptr;
        }
        const std::size_t medio = desde + (hasta - desde) / 2;
        Nodo* nodo = nuevo(ordenados[medio]);
        nodo->izq = construirDesde(ordenados, desde, medio);
        nodo->der = construirDesde(ordenados, medio + 1, hasta);
        return nodo;
    }
};

// ---------------------------------------------------------------------------
// Definiciones fuera de la clase (tienen que estar en el .cpp de la
// instanciacion, asi que van en el header: es un template).
// ---------------------------------------------------------------------------

// COPIA PROFUNDA: el arbol nuevo tiene sus propios nodos. Si compartiera los
// punteros, al destruir uno de los dos se liberaria memoria que el otro
// todavia usa: double free.
template <class T>
ArbolBST<T>::ArbolBST(const ArbolBST& otro) : tam(otro.tam) {
    raiz = copiar(otro.raiz);
}

// COPY-AND-SWAP: se copia en un temporal y se intercambia el estado. Es corto
// y no tiene casos raros: el temporal se destruye al salir y libera el
// contenido viejo.
template <class T>
ArbolBST<T>& ArbolBST<T>::operator=(const ArbolBST& otro) {
    if (this != &otro) {
        ArbolBST copia(otro);
        intercambiar(copia);
    }
    return *this;
}

// ROBO DE LA RAIZ: el origen queda vacio.
template <class T>
ArbolBST<T>::ArbolBST(ArbolBST&& otro) noexcept : raiz(otro.raiz), tam(otro.tam) {
    otro.raiz = nullptr;
    otro.tam = 0;
}

// MOVIMIENTO POR ASIGNACION: primero se libera lo que habia (si no, seria
// una fuga) y despues se roba.
template <class T>
ArbolBST<T>& ArbolBST<T>::operator=(ArbolBST&& otro) noexcept {
    if (this != &otro) {
        liberar();
        raiz = otro.raiz;
        tam = otro.tam;
        otro.raiz = nullptr;
        otro.tam = 0;
    }
    return *this;
}

template <class T>
bool ArbolBST<T>::insertar(const T& valor_) {
    bool duplicado = false;
    raiz = insertarEn(raiz, valor_, duplicado);
    if (duplicado) {
        return false;
    }
    ++tam;
    return true;
}

template <class T>
bool ArbolBST<T>::buscar(const T& valor_) const {
    const Nodo* nodo = raiz;
    while (nodo) {
        if (valor_ < nodo->valor) {
            nodo = nodo->izq;
        } else if (nodo->valor < valor_) {
            nodo = nodo->der;
        } else {
            return true;  // equivalentes
        }
    }
    return false;
}

template <class T>
template <class K, class Comparador>
bool ArbolBST<T>::buscarPorClave(const K& clave, Comparador comparador, T& fuera) const {
    // El descenso es el MISMO que el de `buscar()`, pero en vez de comparar
    // el valor buscado contra el del nodo, se compara la CLAVE contra el
    // del nodo con el comparador del llamador. Por eso el metodo es
    // generico: el arbol no sabe que es la clave.
    const Nodo* nodo = raiz;
    while (nodo) {
        const int comparacion = comparador(clave, nodo->valor);
        if (comparacion < 0) {
            nodo = nodo->izq;
        } else if (comparacion > 0) {
            nodo = nodo->der;
        } else {
            // Equivalentes: la clave esta. Se copia el valor encontrado y
            // se devuelve true.
            fuera = nodo->valor;
            return true;
        }
    }
    // La clave NO esta: `fuera` no se toco (queda como estaba).
    return false;
}

template <class T>
bool ArbolBST<T>::eliminar(const T& valor_) {
    bool eliminado = false;
    raiz = eliminarEn(raiz, valor_, eliminado);
    if (eliminado) {
        --tam;
    }
    return eliminado;
}

template <class T>
int ArbolBST<T>::altura() const {
    return alturaDe(raiz);
}

template <class T>
std::vector<T> ArbolBST<T>::inOrder() const {
    std::vector<T> salida;
    recorrerInOrder([&salida](const T& valor_) { salida.push_back(valor_); });
    return salida;
}

template <class T>
template <class F>
void ArbolBST<T>::recorrerInOrder(F&& visitante) const {
    recorrerEn(raiz, visitante);
}

template <class T>
void ArbolBST<T>::construirDesdeOrdenados(std::vector<T> ordenados) {
    // El arbol se REEMPLAZA: primero se libera lo que hubiera.
    liberar();
    raiz = construirDesde(ordenados, 0, ordenados.size());
    tam = ordenados.size();
}
