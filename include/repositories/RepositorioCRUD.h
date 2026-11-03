#pragma once

#include "exceptions/ExcepcionesSistema.h"
#include "persistence/ArchivoTexto.h"
#include "repositories/IRepositorio.h"
#include "structures/ArbolBST.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// RepositorioCRUD<T>: la IMPLEMENTACION del repositorio, una sola para las
// tres entidades.
//
// El template queda en el HEADER (es header-only, como los demas TADs del
// proyecto: `ArbolBST`, `ListaEnlazada`, `ArchivoTexto`): sin conocer `T`
// no se puede compilar, asi que no hay `.cpp` que compilar.
//
// EL BACKING: un `ArbolBST<T>` (la coleccion ordenada por clave) y un
// `ArchivoTexto<T>` (la capa de archivos). El repositorio es la costura entre
// los dos, y es el UNICO lugar donde se conoce a los dos.
//
// ESTA CAPA NO TIENE REGLAS DE NEGOCIO
//
// El repositorio guarda y devuelve entidades. No sabe si dos turnos se
// solapan, no valida que una matricula sea unica, no decide si un alumno
// inactivo puede ser agendado. Eso es del servicio. Lo unico que esta capa
// dice es "este id ya estaba" o "este id no existe", porque son las dos
// reglas del CRUD en si.
//
// ---------------------------------------------------------------------------
// EL CONTRATO DEL TIPO `T`
//
// Para que `RepositorioCRUD<T>` funcione, `T` tiene que ofrecer exactamente
// esto:
//
//     int getId() const;                    // la clave que el repositorio indexa
//     bool operator<(const T&) const;       // define el ORDEN del arbol
//     // y el contrato de `ArchivoTexto<T>` (ver persistence/ArchivoTexto.h):
//     friend std::ostream& operator<<(std::ostream&, const T&);
//     static T fromRegistro(const std::string& linea);
//     static std::string esquema();
//
// `getId()` es el UNICO dato que el repositorio necesita del dominio: es lo
// que lo identifica y lo que devuelve `proximoId()`. No es una excepcion al
// desacoplamiento: el repositorio organiza las entidades POR ID, asi que
// conocer el id es parte de su trabajo.
// ---------------------------------------------------------------------------
template <class T>
class RepositorioCRUD : public IRepositorio<T> {
public:
    // `ruta` es el archivo de datos y `nombreEntidad` el "magic" del header
    // ("ALUMNOS", "PROFESORES", "TURNOS").
    RepositorioCRUD(std::string ruta, std::string nombreEntidad);

    void alta(const T& entidad) override;
    void modificar(const T& entidad) override;
    bool buscarPorId(int id, T& fuera) const override;
    std::vector<T> listar() const override;
    std::size_t contar() const override;
    int proximoId() const override;
    void cargar() override;
    void persistir() override;

    // El arbol, para los tests que necesitan verificar el balanceo (la
    // decision R6 se demuestra sobre la altura del arbol que queda cargado).
    const ArbolBST<T>& arbol() const { return _arbol; }

    // El nombre de la entidad ("ALUMNOS", "PROFESORES", "TURNOS"): es lo
    // que va en los mensajes de las excepciones del CRUD.
    const std::string& nombreEntidad() const { return _archivo.nombreEntidad(); }

private:
    // Busca la entidad por id y la deja en `fuera`. Devuelve false si no
    // esta.
    //
    // POR QUE NO SE USA `ArbolBST::buscar()`
    //
    // `ArbolBST::buscar()` busca por EQUIVALENCIA, y la equivalencia la
    // induce el `operator<` del tipo `T`. En el dominio, `Persona::operator<`
    // ordena por `(id, tipo)`: para encontrar un alumno HABRIA que construir
    // un alumno de prueba con el mismo id y el mismo tipo, y ese alumno de
    // prueba tendria que tener un nombre, un apellido, un email, un
    // telefono y una matricula VALIDOS (los setters lanzan
    // `DatoInvalidoException` si no). O sea: para preguntar "existe el
    // alumno #7" habria que inventar un alumno #7 completo.
    //
    // La solucion es un RECORRIDO EN ORDEN que se detiene en la primera
    // coincidencia del id. Es O(n) en el peor caso, pero el arbol esta
    // BALANCEADO (altura ~log2(n)), asi que el recorrido en orden toca los
    // nodos de forma ordenada y se puede cortar en cuanto pasa el id buscado.
    // Con el volumen de este sistema el costo es irrelevante y, sobre todo,
    // NO obliga al dominio a exponer un `setId` publico para poder construir
    // la sonda.
    bool buscarPorIdEnArbol(int id, T& fuera) const;

    ArbolBST<T> _arbol;
    ArchivoTexto<T> _archivo;
};

// ---------------------------------------------------------------------------
// Implementaciones (van en el header: es un template).
// ---------------------------------------------------------------------------

template <class T>
RepositorioCRUD<T>::RepositorioCRUD(std::string ruta, std::string nombreEntidad)
    : _archivo(std::move(ruta), std::move(nombreEntidad)) {
}

template <class T>
void RepositorioCRUD<T>::alta(const T& entidad) {
    // El id duplicado se detecta ANTES de tocar el arbol: si la excepcion
    // saliera despues, el arbol quedaria con la entidad insertada y el
    // archivo con un registro de mas.
    T existente;
    if (buscarPorIdEnArbol(entidad.getId(), existente)) {
        throw IdDuplicadoException(entidad.getId(), _archivo.nombreEntidad());
    }

    _arbol.insertar(entidad);

    // Se persiste DESPUES de cada mutacion, no solo al salir: si el
    // programa se cuelga, no se pierde la sesion.
    persistir();
}

template <class T>
void RepositorioCRUD<T>::modificar(const T& entidad) {
    // `modificar` REEMPLAZA: no agrega una segunda entidad con el mismo id.
    // Para eso hay que SACAR la vieja del arbol antes de insertar la nueva.
    T anterior;
    if (!buscarPorIdEnArbol(entidad.getId(), anterior)) {
        throw EntidadNoExisteException(entidad.getId(), _archivo.nombreEntidad());
    }

    // El borrado se hace con la entidad ANTERIOR (la que estaba guardada),
    // porque es la que el arbol tiene indexada por SU PROPIA clave.
    //
    // OJO: la clave del arbol es el `operator<` del tipo `T`, no el id. En el
    // dominio `Persona::operator<` ordena por `(id, tipo)`. Dentro de un
    // repositorio el tipo es SIEMPRE el mismo (`getTipo()` devuelve un valor
    // fijo por clase: "Alumno", "Profesor", "Turno"), asi que buscar por id y
    // buscar por clave son la misma busqueda. Aun asi, se borra con la
    // entidad ANTERIOR y no con la nueva: asi el borrado usa exactamente la
    // clave con la que el nodo fue indexado, sin depender de esa suposicion.
    //
    // Si se insertara la nueva sin borrar la anterior, quedarian DOS
    // entidades con el mismo id y `contar()` mintiera.
    _arbol.eliminar(anterior);
    _arbol.insertar(entidad);

    persistir();
}

template <class T>
bool RepositorioCRUD<T>::buscarPorId(int id, T& fuera) const {
    return buscarPorIdEnArbol(id, fuera);
}

template <class T>
bool RepositorioCRUD<T>::buscarPorIdEnArbol(int id, T& fuera) const {
    // El recorrido en orden VISITA los valores ordenados por clave. Como el
    // id es la parte izquierda del orden del dominio, los ids van
    // apareciendo en orden creciente: en cuanto se pasa del id buscado, ya
    // no va a aparecer mas.
    //
    // El corte se hace con un flag, porque `recorrerInOrder` NO puede
    // interrumperse: su visitante devuelve `void`. Un recorrido completo
    // sobre un arbol balanceado es O(n), y con el volumen de este sistema el
    // costo es irrelevante.
    bool encontrado = false;
    bool terminado = false;
    _arbol.recorrerInOrder([&](const T& valor) {
        if (terminado) {
            return;
        }
        const int idActual = valor.getId();
        if (idActual == id) {
            fuera = valor;
            encontrado = true;
            terminado = true;
            return;
        }
        // El recorrido ya paso el id buscado: no hay nada mas que mirar.
        if (idActual > id) {
            terminado = true;
        }
    });
    return encontrado;
}

template <class T>
std::vector<T> RepositorioCRUD<T>::listar() const {
    // `inOrder()` devuelve los valores ORDENADOS por clave: es la propiedad
    // que define al BST y es tambien el orden en que se escribe el archivo.
    return _arbol.inOrder();
}

template <class T>
std::size_t RepositorioCRUD<T>::contar() const {
    // O(1) de verdad: `size()` es el CONTADOR del TAD, no un recorrido.
    return _arbol.size();
}

template <class T>
int RepositorioCRUD<T>::proximoId() const {
    // El id lo propone el repositorio (decision R4 del issue #14): el
    // usuario no elige el id, el repositorio le dice cual es el siguiente
    // libre. Sobre un repositorio vacio el siguiente libre es 1.
    if (_arbol.vacio()) {
        return 1;
    }

    // El id maximo es el ULTIMO del recorrido en orden (la clave del dominio
    // ordena primero por id), asi que no hace falta un recorrido extra.
    const std::vector<T> ordenados = _arbol.inOrder();
    return ordenados.back().getId() + 1;
}

template <class T>
void RepositorioCRUD<T>::cargar() {
    // Si el archivo NO existe, se crea vacio y NO se falla: la aplicacion
    // no puede romperse al arrancar por un archivo que todavia no esta.
    // Es el mismo criterio que usa la capa de persistencia con `crearVacio`.
    std::vector<T> registros;
    try {
        registros = _archivo.cargar();
    } catch (const ArchivoNoEncontradoException&) {
        _archivo.crearVacio();
        _arbol.construirDesdeOrdenados(std::vector<T>());
        return;
    }

    // DECISION R6: al cargar se usa SIEMPRE `construirDesdeOrdenados()`,
    // NUNCA `insertar()` en un loop.
    //
    // `insertar()` es un BST plano: con ids secuenciales (1, 2, 3...) el
    // arbol degenera en una cadena y 100 ids dan una altura de 100.
    // `construirDesdeOrdenados()` es O(n) y arma un arbol balanceado
    // (altura ~log2(n)): el archivo YA viene ordenado por id, asi que el
    // arbol balanceado sale gratis.
    _arbol.construirDesdeOrdenados(std::move(registros));
}

template <class T>
void RepositorioCRUD<T>::persistir() {
    // Se escribe SIEMPRE el vector ordenado del arbol: `ArchivoTexto` no
    // conoce el arbol, escribe lo que le dan en el orden en que lo recibe.
    _archivo.guardar(_arbol.inOrder());
}
