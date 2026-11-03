#pragma once

#include "entities/Profesor.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "repositories/RepositorioCRUD.h"

#include <string>

// ---------------------------------------------------------------------------
// RepositorioProfesores: el repositorio de PROFESORES.
//
// Es `RepositorioCRUD<Profesor>` MAS la baja logica. El resto del CRUD no se
// reimplementa: sale del template.
//
// POR QUE IMPLEMENTA `IRepositorioBajaLogica<Profesor>`
//
// Un profesor TIENE el flag `activo` (lo hereda de `Persona`), asi que se
// puede dar de baja logicamente y reactivar. Un turno no: por eso el
// repositorio de turnos NO implementa esta interfaz (decision R2 del issue
// #14).
// ---------------------------------------------------------------------------
class RepositorioProfesores : public RepositorioCRUD<Profesor>,
                              public IRepositorioBajaLogica<Profesor> {
public:
    explicit RepositorioProfesores(std::string ruta);

    void bajaLogica(int id) override;
    void reactivar(int id) override;
};
