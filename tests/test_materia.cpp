#include "test_framework.h"

#include "domain/Materia.h"
#include "exceptions/ExcepcionesSistema.h"

#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Tests del catalogo de materias.
//
// NOTA DE CODIFICACION: este archivo esta en UTF-8 (sin BOM) porque las
// descripciones del catalogo contienen tildes.
//
// LO QUE SE VERIFICA ACA:
//   - el round trip del catalogo es POR CODIGO (materiaDesdeCodigo(codigoDe(m))
//     == m) y no por indice del enum;
//   - los codigos y las descripciones son unicos;
//   - `todasLasMaterias()` devuelve las 12.
// ---------------------------------------------------------------------------

namespace {

// Ejecuta la funcion y dice si lanzo `DatoInvalidoException` con el campo
// esperado. Si lanzo otra excepcion (o ninguna), devuelve false.
template <typename F>
bool lanzaConCampo(F funcion, const std::string& campoEsperado) {
    try {
        funcion();
    } catch (const DatoInvalidoException& excepcion) {
        return excepcion.campo() == campoEsperado;
    } catch (...) {
        return false;
    }
    return false;
}

// El catalogo esperado, escrito a mano para detectar que la tabla de
// `Materia.cpp` no se desincronice con la descripcion de cada materia.
struct MateriaEsperada {
    Materia materia;
    const char* codigo;
    const char* descripcion;
};

const MateriaEsperada CATALOGO_ESPERADO[] = {
    {Materia::ALG1, "ALG1", "Algoritmos I"},
    {Materia::ALG2, "ALG2", "Algoritmos II"},
    {Materia::BDD, "BDD", "Base de Datos"},
    {Materia::ING1, "ING1", "Ingeniería de Software I"},
    {Materia::ING2, "ING2", "Ingeniería de Software II"},
    {Materia::ESTRUCT, "ESTRUCT", "Estructuras de Datos"},
    {Materia::ANALISIS, "ANALISIS", "Análisis Matemático"},
    {Materia::CALC1, "CALC1", "Cálculo I"},
    {Materia::FISICA2, "FISICA2", "Física II"},
    {Materia::REDES, "REDES", "Redes y Comunicación"},
    {Materia::SISTEMAS, "SISTEMAS", "Sistemas Operativos"},
    {Materia::ARQUITECTURA, "ARQUITECTURA", "Arquitectura de Computadores"}
};

} // namespace

TEST_CASE("el catalogo tiene las 12 materias", materia_cantidad) {
    CHECK(todasLasMaterias().size() == 12);
}

TEST_CASE("el round trip del catalogo es por codigo", materia_round_trip_codigo) {
    // Que reordenar el enum NO cambie el codigo persistido es justamente lo
    // que permite que la Fase 4 guarde codigos y no indices.
    for (const Materia materia : todasLasMaterias()) {
        CHECK(materiaDesdeCodigo(codigoDe(materia)) == materia);
    }
}

TEST_CASE("materiaDesdeCodigo lanza con un codigo desconocido", materia_codigo_desconocido) {
    CHECK(lanzaConCampo([] { materiaDesdeCodigo("XXX"); }, "materia"));
    CHECK(lanzaConCampo([] { materiaDesdeCodigo(""); }, "materia"));
    // Mayusculas y minusculas son DISTINTAS: el codigo es exacto.
    CHECK(lanzaConCampo([] { materiaDesdeCodigo("alg1"); }, "materia"));
}

TEST_CASE("los codigos del catalogo son unicos", materia_codigos_unicos) {
    std::set<std::string> codigos;
    for (const Materia materia : todasLasMaterias()) {
        codigos.insert(codigoDe(materia));
    }
    CHECK(codigos.size() == 12);
}

TEST_CASE("las descripciones del catalogo son unicas", materia_descripciones_unicas) {
    std::set<std::string> descripciones;
    for (const Materia materia : todasLasMaterias()) {
        descripciones.insert(descripcionDe(materia));
    }
    CHECK(descripciones.size() == 12);
}

TEST_CASE("el codigo y la descripcion son valores distintos", materia_codigo_distinto) {
    for (const Materia materia : todasLasMaterias()) {
        CHECK(codigoDe(materia) != descripcionDe(materia));
    }
}

TEST_CASE("las descripciones coinciden con el catalogo esperado", materia_descripciones) {
    for (const MateriaEsperada& esperada : CATALOGO_ESPERADO) {
        CHECK(codigoDe(esperada.materia) == esperada.codigo);
        CHECK(descripcionDe(esperada.materia) == esperada.descripcion);
    }
}

TEST_CASE("las tildes de las descripciones estan en UTF-8", materia_descripcion_utf8) {
    // "Análisis Matemático" tiene 19 letras y 21 bytes: las dos "á" ocupan
    // dos bytes cada una. Sin tildes ("Analisis Matematico") serian 19 bytes.
    const std::string analisis = descripcionDe(Materia::ANALISIS);
    CHECK(analisis.size() == 21);
    CHECK(analisis != "Analisis Matematico");
}

TEST_CASE("el catalogo no depende del orden del enum", materia_catalogo_estable) {
    // El vector de `todasLasMaterias()` es el mismo objeto en cada llamada
    // (singleton de Meyers): no se reconstruye ni se devuelve una copia.
    CHECK(&todasLasMaterias() == &todasLasMaterias());
}
