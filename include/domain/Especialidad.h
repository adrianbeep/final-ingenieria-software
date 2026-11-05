#pragma once

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Catalogo de especialidades.
//
// `Especialidad` NO es una entidad: no tiene id, ni se persiste, ni tiene CRUD.
// Es un `enum class` con 12 valores, y este header expone las TRES funciones
// que lo pueden Interpretar:
//
//   descripcionDe(e)           -> "Cardiologia"   (para MOSTRAR)
//   codigoDe(e)                -> "CARD"          (para PERSISTIR)
//   especialidadDesdeCodigo(c) -> Especialidad::CARD    (reverse lookup)
//
// POR QUE `enum class` Y NO `enum` PELADO: sin conversion implicita a `int`,
// sin colision de nombres entre enumeradores, y el compilador obliga a ser
// explicito. No existe `Especialidad::CARD == 0` porque `0` no es una
// `Especialidad`.
//
// POR QUE CODIGO Y DESCRIPCION SON DOS COSAS: el codigo es corto y estable
// (va al archivo); la descripcion es larga y puede cambiar (va a pantalla).
// Si se persistiera la descripcion, renombrar una especialidad invalidaria
// todos los archivos de datos.
//
// NOTA DE CODIFICACION: este archivo esta en UTF-8 (sin BOM) porque las
// descripciones contienen tildes.
// ---------------------------------------------------------------------------

enum class Especialidad {
    CARD,
    MEDG,
    PEDI,
    TRAU,
    DERM,
    NEUR,
    OFTA,
    PSI,
    RADI,
    GAST,
    ENDO,
    NEUM
};

// Descripcion legible de la especialidad, para mostrar en pantalla.
std::string descripcionDe(Especialidad especialidad);

// Codigo corto y estable de la especialidad, para persistir.
std::string codigoDe(Especialidad especialidad);

// Busca la especialidad a partir de su codigo.
// Lanza `DatoInvalidoException` si el codigo no existe en el catalogo.
Especialidad especialidadDesdeCodigo(const std::string& codigo);

// Todas las especialidades del catalogo, en el orden del enum.
// Devuelve una REFERENCIA a un static local (singleton de Meyers): se
// construye una sola vez, es thread-safe desde C++11 y no devuelve copias.
const std::vector<Especialidad>& todasLasEspecialidades();
