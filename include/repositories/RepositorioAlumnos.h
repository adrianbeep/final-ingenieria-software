#pragma once

#include "entities/Alumno.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "repositories/RepositorioCRUD.h"

#include <string>

// ---------------------------------------------------------------------------
// RepositorioAlumnos: el repositorio de ALUMNOS.
//
// Es `RepositorioCRUD<Alumno>` MAS la baja logica. El resto del CRUD
// (alta, modificar, buscar, listar, contar, proximoId, cargar, persistir) NO
// se reimplementa: sale del template.
//
// POR QUE IMPLEMENTA `IRepositorioBajaLogica<Alumno>`
//
// Un alumno TIENE el flag `activo` (lo hereda de `Persona`), asi que se
// puede dar de baja logicamente y reactivar. Un turno no: por eso el
// repositorio de turnos NO implementa esta interfaz (decision R2 del issue
// #14).
// ---------------------------------------------------------------------------
class RepositorioAlumnos : public RepositorioCRUD<Alumno>,
                           public IRepositorioBajaLogica<Alumno> {
public:
    explicit RepositorioAlumnos(std::string ruta);

    void bajaLogica(int id) override;
    void reactivar(int id) override;
};
