#include "repositories/RepositorioProfesores.h"

#include "repositories/BajaLogicaCompartida.h"

#include <string>

// ---------------------------------------------------------------------------
// El nombre de la entidad que viaja en el header del archivo de datos.
//
// NO es mecanico a partir de `getTipo()`: `getTipo()` devuelve "Profesor" y el
// plural no sale de ahi. Por eso el nombre va escrito aca, en MAYUSCULAS.
// ---------------------------------------------------------------------------
namespace {

const char* NOMBRE_ENTIDAD = "PROFESORES";

} // namespace

RepositorioProfesores::RepositorioProfesores(std::string ruta)
    : RepositorioCRUD<Profesor>(std::move(ruta), NOMBRE_ENTIDAD) {
}

void RepositorioProfesores::bajaLogica(int id) {
    // La logica es la MISMA que la del alumno: vive en
    // `BajaLogicaCompartida`, escrita UNA sola vez.
    BajaLogicaCompartida<Profesor>::darDeBaja(*this, id);
}

void RepositorioProfesores::reactivar(int id) {
    BajaLogicaCompartida<Profesor>::reactivar(*this, id);
}
