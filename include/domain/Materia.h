#pragma once

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Catalogo de materias.
//
// `Materia` NO es una entidad: no tiene id, ni se persiste, ni tiene CRUD.
// Es un `enum class` con 12 valores, y este header expone las TRES funciones
// que lo pueden Interpretar:
//
//   descripcionDe(m)      -> "Algoritmos I"   (para MOSTRAR)
//   codigoDe(m)           -> "ALG1"           (para PERSISTIR)
//   materiaDesdeCodigo(c) -> Materia::ALG1    (reverse lookup)
//
// POR QUE `enum class` Y NO `enum` PELADO: sin conversion implicita a `int`,
// sin colision de nombres entre enumeradores, y el compilador obliga a ser
// explicito. No existe `Materia::ALG1 == 0` porque `0` no es un `Materia`.
//
// POR QUE CODIGO Y DESCRIPCION SON DOS COSAS: el codigo es corto y estable
// (va al archivo); la descripcion es larga y puede cambiar (va a pantalla).
// Si se persistiera la descripcion, renombrar una materia invalidaria todos
// los archivos de datos.
//
// NOTA DE CODIFICACION: este archivo esta en UTF-8 (sin BOM) porque las
// descripciones contienen tildes.
// ---------------------------------------------------------------------------

enum class Materia {
    ALG1,
    ALG2,
    BDD,
    ING1,
    ING2,
    ESTRUCT,
    ANALISIS,
    CALC1,
    FISICA2,
    REDES,
    SISTEMAS,
    ARQUITECTURA
};

// Descripcion legible de la materia, para mostrar en pantalla.
std::string descripcionDe(Materia materia);

// Codigo corto y estable de la materia, para persistir.
std::string codigoDe(Materia materia);

// Busca la materia a partir de su codigo.
// Lanza `DatoInvalidoException` si el codigo no existe en el catalogo.
Materia materiaDesdeCodigo(const std::string& codigo);

// Todas las materias del catalogo, en el orden del enum.
// Devuelve una REFERENCIA a un static local (singleton de Meyers): se
// construye una sola vez, es thread-safe desde C++11 y no devuelve copias.
const std::vector<Materia>& todasLasMaterias();
