#pragma once

#include <cstddef>
#include <vector>

// ---------------------------------------------------------------------------
// IRepositorio<T>: el CONTRATO de un repositorio.
//
// POR QUE ES UN TEMPLATE Y NO TRES INTERFACES (`IAlumnoRepositorio`,
// `IProfesorRepositorio`, `ITurnoRepositorio`)
//
// El CRUD de las tres entidades es IDENTICO: los mismos metodos, con las
// mismas reglas, cambiando solo el tipo de la entidad. Con tres interfaces
// el mismo contrato se escribiria tres veces y se mantendria tres veces: es
// el "CRUD triplicado" que la consigna quiere evitar, movido de las clases
// concretas a las interfaces. Con una sola interfaz generica el contrato
// esta escrito UNA vez, y quien lo usa trabaja con la INTERFAZ:
// `ServicioTurnos` recibe `IRepositorio<Alumno>&`, `IRepositorio<Profesor>&`
// y `IRepositorio<Turno>&`, que son tres tipos DISTINTOS.
//
// QUE HACE ESTA INTERFAZ Y QUE NO
//
// Solo declara COMO se guarda y como se recuperan las entidades. No dice
// NADA de archivos, de arboles ni de reglas de negocio: por eso es una
// interfaz y no una clase. En los tests del servicio se puede inyectar un
// FAKE en memoria que la implemente, y las reglas se prueban sin tocar el
// disco.
//
// POR QUE LOS METODOS SON VIRTUALES PUROS Y NO UN TEMPLATE DE METODOS
//
// Se busca poder inyectar un fake: si los metodos fueran de template, no
// habria vtable y no se podria sustituir la implementacion en caliente.
// ---------------------------------------------------------------------------
template <class T>
class IRepositorio {
public:
    virtual ~IRepositorio() = default;

    // Da de alta una entidad. Lanza `IdDuplicadoException` si el id ya
    // esta ocupado.
    virtual void alta(const T& entidad) = 0;

    // Reemplaza una entidad existente (misma clave). Lanza
    // `EntidadNoExisteException` si NO esta.
    virtual void modificar(const T& entidad) = 0;

    // Busca por id. Devuelve false (y NO lanza) si no existe: la ausencia
    // es un dato de la consulta, no un error.
    virtual bool buscarPorId(int id, T& fuera) const = 0;

    // Todas las entidades, ordenadas por clave.
    virtual std::vector<T> listar() const = 0;

    // Cuantas hay. O(1).
    virtual std::size_t contar() const = 0;

    // El proximo id libre. Lo propone el repositorio: el id lo asigna la
    // capa de repositorios, no el usuario.
    virtual int proximoId() const = 0;

    // Carga desde el archivo. Si el archivo no existe, lo crea vacio: la
    // aplicacion no puede romperse al arrancar por un archivo faltante.
    virtual void cargar() = 0;

    // Vuelca al archivo. La escritura es atomica (lo garantiza la capa de
    // persistencia).
    virtual void persistir() = 0;
};
