#include "repositories/RepositorioAlumnos.h"

#include "repositories/BajaLogicaCompartida.h"

#include <string>

// ---------------------------------------------------------------------------
// El nombre de la entidad que viaja en el header del archivo de datos.
//
// NO es mecanico a partir de `getTipo()`: `getTipo()` devuelve "Alumno" y el
// plural no sale de ahi. Por eso el nombre va escrito aca, en MAYUSCULAS.
// ---------------------------------------------------------------------------
namespace {

const char* NOMBRE_ENTIDAD = "ALUMNOS";

} // namespace

RepositorioAlumnos::RepositorioAlumnos(std::string ruta)
    : RepositorioCRUD<Alumno>(std::move(ruta), NOMBRE_ENTIDAD) {
}

void RepositorioAlumnos::bajaLogica(int id) {
    // La logica es la MISMA que la del profesor: vive en
    // `BajaLogicaCompartida`, escrita UNA sola vez.
    BajaLogicaCompartida<Alumno>::darDeBaja(*this, id);
}

void RepositorioAlumnos::reactivar(int id) {
    BajaLogicaCompartida<Alumno>::reactivar(*this, id);
}
