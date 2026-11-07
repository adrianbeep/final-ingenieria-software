#pragma once

#include "entities/Medico.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "repositories/RepositorioCRUD.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// RepositorioMedicos: el repositorio de MEDICOS.
//
// Es `RepositorioCRUD<Medico>` MAS la baja logica Y la unicidad del
// legajo. El resto del CRUD no se reimplementa: sale del template.
//
// POR QUE IMPLEMENTA `IRepositorioBajaLogica<Medico>`
//
// Un medico TIENE el flag `activo` (lo hereda de `Persona`), asi que se
// puede dar de baja logicamente y reactivar. Un turno no: por eso el
// repositorio de turnos NO implementa esta interfaz (decision R2 del issue
// #14).
//
// LA UNICIDAD DEL LEGAJO: POR QUE VIVE ACA Y NO EN LA ENTIDAD
//
// `entities/Medico.h` dice que el legajo es UNICO y que la unicidad la
// verifica el repositorio. Ese comentario existia desde la Fase 2 pero la
// verificacion NUNCA se implemento: hoy `alta()` solo detecta ids
// duplicados, asi que se podian dar de alta dos medicos con el mismo
// legajo y el sistema quedaba inconsistente.
//
// Y el repositorio es el UNICO lugar donde se puede verificar: la entidad
// sola no sabe que legajos hay cargados (su unicidad es una regla del
// CONJUNTO, no de la entidad), asi que la tiene que resolver quien tiene el
// conjunto: el repositorio.
//
// Se reescriben `alta` y `modificar` porque son los DOS UNICOS puntos por
// los que un legajo entra al repositorio. Los dos validan ANTES de tocar el
// arbol: si la excepcion saliera despues, el repositorio quedaria con la
// entidad insertada y el archivo con un registro de mas.
// ---------------------------------------------------------------------------
class RepositorioMedicos : public RepositorioCRUD<Medico>,
                              public IRepositorioBajaLogica<Medico> {
public:
    explicit RepositorioMedicos(std::string ruta);

    void bajaLogica(int id) override;
    void reactivar(int id) override;

    // Devuelve true si YA existe un medico con ese legajo.
    // idExistente = id del medico que lo tiene, o -1 si no existe ninguno.
    //
    // La comparacion es CASE-INSENSITIVE: "LEG-1001" y "leg-1001" son el
    // MISMO legajo.
    //
    // NO lanza y NO modifica nada: es una CONSULTA. Que no haya un legajo
    // repetido es un dato de la consulta, no un error (es el mismo criterio
    // que usa `buscarPorId()` cuando no encuentra el id).
    bool existeLegajo(const std::string& legajo, int& idExistente) const;

    // Los medicos ACTIVOS, en orden ascendente de id.
    //
    // Es la consulta de las pantallas que eligen a QUIEN agendar: un medico
    // dado de baja no puede tener turnos (es una regla del servicio), asi
    // que no tiene sentido ofrecerlo.
    //
    // NO lanza y NO modifica nada: es una CONSULTA.
    std::vector<Medico> listarActivos() const;

    // Los medicos DADOS DE BAJA, en orden ascendente de id.
    //
    // Es la consulta de la pantalla de reactivation: un medico dado de baja
    // sigue en el repositorio (la baja es logica), asi que hay que poder
    // listarlos para volver a activarlos.
    std::vector<Medico> listarInactivos() const;

    // Las dos ESCRITURAS pasan por la unicidad del legajo.
    void alta(const Medico& medico) override;
    void modificar(const Medico& medico) override;

private:
    // Busca el legajo IGNORANDO a la entidad con id `idExcluido`.
    // `idExcluido` = -1 significa "ignorar a nadie".
    //
    // POR QUE HACE FALTA LA EXCLUSION
    //
    // `modificar` REEMPLAZA a la entidad que ya esta guardada: si el
    // medico #7 conserva su propio legajo no puede chocar consigo mismo.
    // La exclusion es por `getId()`, que es la clave con la que el
    // repositorio indexa las entidades.
    //
    // Y en `alta` la exclusion tambien tiene sentido: si el id ya esta
    // ocupado, el error que corresponde es el de id duplicado
    // (`IdDuplicadoException`, que lanza el CRUD), no el de legajo
    // repetido. Asi el alta de un id repetido sigue fallando por el id, que
    // es el error mas especifico para ese caso.
    bool buscarLegajo(const std::string& legajo, int idExcluido, int& idExistente) const;
};
