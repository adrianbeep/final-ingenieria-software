#pragma once

#include "entities/Persona.h"
#include "exceptions/ExcepcionesSistema.h"

#include <string>
#include <type_traits>

// ---------------------------------------------------------------------------
// La baja logica COMPARTIDA entre el repositorio de alumnos y el de
// profesores.
//
// POR QUE VIVE ACA Y NO DUPLICADA EN LOS DOS REPOSITORIOS
//
// La logica es IDENTICA: se busca la entidad, se le cambia el flag `activo`
// y se persiste. La unica diferencia es el TIPO de la entidad, y eso lo
// resuelve el template.
//
// Y NO HAY QUE TOCAR EL DOMINIO: `Persona` ya expone `setActivo(bool)` y
// `esActivo()` en su interfaz publica. El repositorio no agrega ningun
// metodo al dominio: usa los que ya hay.
//
// POR QUE NO SE PONE EN `Persona` COMO METODO DEL DOMINIO
//
// "Dar de baja" es una operacion del REPOSITORIO (cambia el estado
// PERSISTIDO y lo escribe), no del dominio. Si estuviera en `Persona`, el
// dominio sabria que existe un repositorio: eso rompe la direccion de las
// dependencias.
//
// EL CONTRATO DEL TIPO `T`
//
// `T` tiene que ser una `Persona` (o derivar de ella) para tener el flag
// `activo`. El invariante se EXIGE con un `static_assert`, no con un
// comentario: si alguien usa esta clase con un tipo que no tiene el flag,
// el codigo deja de compilar y el error dice por que.
// ---------------------------------------------------------------------------
template <class T>
class BajaLogicaCompartida {
    static_assert(std::is_base_of<Persona, T>::value,
                  "la baja logica necesita una entidad con el flag 'activo' (derivada de Persona)");

public:
    // La clase es utilitaria: no tiene estado, no se instancia.
    BajaLogicaCompartida() = delete;

    // Marca la entidad con el id dado como INACTIVA. NO la borra: la baja es
    // logica (decision 10 del roadmap).
    //
    // Lanza `EntidadNoExisteException` si el id no existe.
    template <class Repositorio>
    static void darDeBaja(Repositorio& repositorio, int id) {
        aplicar(repositorio, id, false);
    }

    // Vuelve a activar la entidad con el id dado.
    //
    // Lanza `EntidadNoExisteException` si el id no existe.
    template <class Repositorio>
    static void reactivar(Repositorio& repositorio, int id) {
        aplicar(repositorio, id, true);
    }

private:
    // El nucleo, escrito UNA vez: buscar, cambiar el flag y persistir.
    //
    // El `modificar()` del repositorio es lo que REEMPLAZA la entidad en el
    // arbol y escribe el archivo. Por eso el cambio se hace sobre una COPIA
    // de la entidad encontrada y se la pasa a `modificar()`: si se modificara
    // la entidad Original, el arbol seguiria teniendo la version vieja (el
    // `ArbolBST` guarda COPIAS de los valores).
    template <class Repositorio>
    static void aplicar(Repositorio& repositorio, int id, bool activo) {
        T entidad;
        if (!repositorio.buscarPorId(id, entidad)) {
            throw EntidadNoExisteException(id, repositorio.nombreEntidad());
        }

        entidad.setActivo(activo);

        // `modificar()` REEMPLAZA la entidad guardada (no agrega una segunda)
        // y persiste el cambio.
        repositorio.modificar(entidad);
    }
};
