#pragma once

#include "entities/Alumno.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "repositories/RepositorioCRUD.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// RepositorioAlumnos: el repositorio de ALUMNOS.
//
// Es `RepositorioCRUD<Alumno>` MAS la baja logica Y la unicidad de la
// matricula. El resto del CRUD (buscar, listar, contar, proximoId, cargar,
// persistir) NO se reimplementa: sale del template.
//
// POR QUE IMPLEMENTA `IRepositorioBajaLogica<Alumno>`
//
// Un alumno TIENE el flag `activo` (lo hereda de `Persona`), asi que se
// puede dar de baja logicamente y reactivar. Un turno no: por eso el
// repositorio de turnos NO implementa esta interfaz (decision R2 del issue
// #14).
//
// LA UNICIDAD DE LA MATRICULA: POR QUE VIVE ACA Y NO EN LA ENTIDAD
//
// `entities/Alumno.h` dice que la matricula es UNICA y que la unicidad la
// verifica el repositorio. Ese comentario existia desde la Fase 2 pero la
// verificacion NUNCA se implemento: hoy `alta()` solo detecta ids
// duplicados, asi que se podian dar de alta dos alumnos con la misma
// matricula y el sistema quedaba inconsistente.
//
// Y el repositorio es el UNICO lugar donde se puede verificar: la entidad
// sola no sabe que matriculas hay cargadas (su unicidad es una regla del
// CONJUNTO, no de la entidad), asi que la tiene que resolver quien tiene el
// conjunto: el repositorio.
//
// Se reescriben `alta` y `modificar` porque son los DOS UNICOS puntos por
// los que una matricula entra al repositorio. Los dos validan ANTES de
// tocar el arbol: si la excepcion saliera despues, el repositorio quedaria
// con la entidad insertada y el archivo con un registro de mas.
// ---------------------------------------------------------------------------
class RepositorioAlumnos : public RepositorioCRUD<Alumno>,
                           public IRepositorioBajaLogica<Alumno> {
public:
    explicit RepositorioAlumnos(std::string ruta);

    void bajaLogica(int id) override;
    void reactivar(int id) override;

    // Devuelve true si YA existe un alumno con esa matricula.
    // idExistente = id del alumno que la tiene, o -1 si no existe ninguna.
    //
    // La comparacion es CASE-INSENSITIVE: "MAT-20001" y "mat-20001" son la
    // MISMA matricula.
    //
    // NO lanza y NO modifica nada: es una CONSULTA. Que no haya una
    // matricula repetida es un dato de la consulta, no un error (es el
    // mismo criterio que usa `buscarPorId()` cuando no encuentra el id).
    bool existeMatricula(const std::string& matricula, int& idExistente) const;

    // Los alumnos ACTIVOS, en orden ascendente de id.
    //
    // Es la consulta de las pantallas que eligen a QUIEN agendar: un alumno
    // dado de baja no puede tener turnos (es una regla del servicio), asi
    // que no tiene sentido ofrecerlo.
    //
    // NO lanza y NO modifica nada: es una CONSULTA.
    std::vector<Alumno> listarActivos() const;

    // Los alumnos DADOS DE BAJA, en orden ascendente de id.
    //
    // Es la consulta de la pantalla de reactivation: un alumno dado de baja
    // sigue en el repositorio (la baja es logica), asi que hay que poder
    // listarlos para volver a activarlos.
    std::vector<Alumno> listarInactivos() const;

    // Las dos ESCRITURAS pasan por la unicidad de la matricula.
    void alta(const Alumno& alumno) override;
    void modificar(const Alumno& alumno) override;

private:
    // Busca la matricula IGNORANDO a la entidad con id `idExcluido`.
    // `idExcluido` = -1 significa "ignorar a nadie".
    //
    // POR QUE HACE FALTA LA EXCLUSION
    //
    // `modificar` REEMPLAZA a la entidad que ya esta guardada: si el alumno
    // #7 conserva su propia matricula no puede chocar consigo mismo. La
    // exclusion es por `getId()`, que es la clave con la que el repositorio
    // indexa las entidades.
    //
    // Y en `alta` la exclusion tambien tiene sentido: si el id ya esta
    // ocupado, el error que corresponde es el de id duplicado
    // (`IdDuplicadoException`, que lanza el CRUD), no el de matricula
    // repetida. Asi el alta de un id repetido sigue fallando por el id,
    // que es el error mas especifico para ese caso.
    bool buscarMatricula(const std::string& matricula, int idExcluido, int& idExistente) const;
};
