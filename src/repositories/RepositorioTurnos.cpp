#include "repositories/RepositorioTurnos.h"

#include <string>

// ---------------------------------------------------------------------------
// El nombre de la entidad que viaja en el header del archivo de datos.
//
// NO es mecanico a partir de `getTipo()`: `getTipo()` devuelve "Turno" y el
// plural no sale de ahi. Por eso el nombre va escrito aca, en MAYUSCULAS.
// ---------------------------------------------------------------------------
namespace {

const char* NOMBRE_ENTIDAD = "TURNOS";

} // namespace

RepositorioTurnos::RepositorioTurnos(std::string ruta)
    : RepositorioCRUD<Turno>(std::move(ruta), NOMBRE_ENTIDAD) {
}
