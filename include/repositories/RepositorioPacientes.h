#pragma once

#include "entities/Paciente.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "repositories/RepositorioCRUD.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// RepositorioPacientes: el repositorio de PACIENTES.
//
// Es `RepositorioCRUD<Paciente>` MAS la baja logica Y la unicidad del
// dni. El resto del CRUD (buscar, listar, contar, proximoId, cargar,
// persistir) NO se reimplementa: sale del template.
//
// POR QUE IMPLEMENTA `IRepositorioBajaLogica<Paciente>`
//
// Un paciente TIENE el flag `activo` (lo hereda de `Persona`), asi que se
// puede dar de baja logicamente y reactivar. Un turno no: por eso el
// repositorio de turnos NO implementa esta interfaz (decision R2 del issue
// #14).
//
// LA UNICIDAD DEL DNI: POR QUE VIVE ACA Y NO EN LA ENTIDAD
//
// `entities/Paciente.h` dice que el dni es UNICO y que la unicidad la
// verifica el repositorio. Ese comentario existia desde la Fase 2 pero la
// verificacion NUNCA se implemento: hoy `alta()` solo detecta ids
// duplicados, asi que se podian dar de alta dos pacientes con el mismo
// dni y el sistema quedaba inconsistente.
//
// Y el repositorio es el UNICO lugar donde se puede verificar: la entidad
// sola no sabe que dnis hay cargados (su unicidad es una regla del
// CONJUNTO, no de la entidad), asi que la tiene que resolver quien tiene el
// conjunto: el repositorio.
//
// Se reescriben `alta` y `modificar` porque son los DOS UNICOS puntos por
// los que un dni entra al repositorio. Los dos validan ANTES de
// tocar el arbol: si la excepcion saliera despues, el repositorio quedaria
// con la entidad insertada y el archivo con un registro de mas.
// ---------------------------------------------------------------------------
class RepositorioPacientes : public RepositorioCRUD<Paciente>,
                           public IRepositorioBajaLogica<Paciente> {
public:
    explicit RepositorioPacientes(std::string ruta);

    void bajaLogica(int id) override;
    void reactivar(int id) override;

    // Devuelve true si YA existe un paciente con ese dni.
    // idExistente = id del paciente que lo tiene, o -1 si no existe ninguno.
    //
    // La comparacion es EXACTA: el dni es un numero, asi que dos dnis son el
    // mismo si tienen los mismos digitos.
    //
    // NO lanza y NO modifica nada: es una CONSULTA. Que no haya un
    // dni repetido es un dato de la consulta, no un error (es el
    // mismo criterio que usa `buscarPorId()` cuando no encuentra el id).
    bool existeDni(const std::string& dni, int& idExistente) const;

    // Los pacientes ACTIVOS, en orden ascendente de id.
    //
    // Es la consulta de las pantallas que eligen a QUIEN agendar: un paciente
    // dado de baja no puede tener turnos (es una regla del servicio), asi
    // que no tiene sentido ofrecerlo.
    //
    // NO lanza y NO modifica nada: es una CONSULTA.
    std::vector<Paciente> listarActivos() const;

    // Los pacientes DADOS DE BAJA, en orden ascendente de id.
    //
    // Es la consulta de la pantalla de reactivation: un paciente dado de baja
    // sigue en el repositorio (la baja es logica), asi que hay que poder
    // listarlos para volver a activarlos.
    std::vector<Paciente> listarInactivos() const;

    // Las dos ESCRITURAS pasan por la unicidad del dni.
    void alta(const Paciente& paciente) override;
    void modificar(const Paciente& paciente) override;

private:
    // Busca el dni IGNORANDO a la entidad con id `idExcluido`.
    // `idExcluido` = -1 significa "ignorar a nadie".
    //
    // POR QUE HACE FALTA LA EXCLUSION
    //
    // `modificar` REEMPLAZA a la entidad que ya esta guardada: si el paciente
    // #7 conserva su propia dni no puede chocar consigo mismo. La
    // exclusion es por `getId()`, que es la clave con la que el repositorio
    // indexa las entidades.
    //
    // Y en `alta` la exclusion tambien tiene sentido: si el id ya esta
    // ocupado, el error que corresponde es el de id duplicado
    // (`IdDuplicadoException`, que lanza el CRUD), no el de dni
    // repetida. Asi el alta de un id repetido sigue fallando por el id,
    // que es el error mas especifico para ese caso.
    bool buscarDni(const std::string& dni, int idExcluido, int& idExistente) const;
};
