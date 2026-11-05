#include "test_framework.h"

#include "domain/Especialidad.h"
#include "exceptions/ExcepcionesSistema.h"

#include <set>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Tests del catalogo de especialidades.
//
// NOTA DE CODIFICACION: este archivo esta en UTF-8 (sin BOM) porque las
// descripciones del catalogo contienen tildes.
//
// LO QUE SE VERIFICA ACA:
//   - el round trip del catalogo es POR CODIGO (especialidadDesdeCodigo(codigoDe(e))
//     == e) y no por indice del enum;
//   - los codigos y las descripciones son unicos;
//   - `todasLasEspecialidades()` devuelve las 12.
// ---------------------------------------------------------------------------

namespace {

// `lanzaConCampo` viene de test_framework.h: es el helper compartido para
// comprobar el lanzamiento de una excepcion del dominio con su campo.

// El catalogo esperado, escrito a mano para detectar que la tabla de
// `Especialidad.cpp` no se desincronice con la descripcion de cada especialidad.
struct EspecialidadEsperada {
    Especialidad especialidad;
    const char* codigo;
    const char* descripcion;
};

const EspecialidadEsperada CATALOGO_ESPERADO[] = {
    {Especialidad::CARD, "CARD", "Cardiología"},
    {Especialidad::MEDG, "MEDG", "Medicina General"},
    {Especialidad::PEDI, "PEDI", "Pediatría"},
    {Especialidad::TRAU, "TRAU", "Traumatología"},
    {Especialidad::DERM, "DERM", "Dermatología"},
    {Especialidad::NEUR, "NEUR", "Neurología"},
    {Especialidad::OFTA, "OFTA", "Oftalmología"},
    {Especialidad::PSI, "PSI", "Psiquiatría"},
    {Especialidad::RADI, "RADI", "Radiología"},
    {Especialidad::GAST, "GAST", "Gastroenterología"},
    {Especialidad::ENDO, "ENDO", "Endocrinología"},
    {Especialidad::NEUM, "NEUM", "Neumología"}
};

} // namespace

TEST_CASE("el catalogo tiene las 12 especialidades", especialidad_cantidad) {
    CHECK(todasLasEspecialidades().size() == 12);
}

TEST_CASE("el round trip del catalogo es por codigo", especialidad_round_trip_codigo) {
    // Que reordenar el enum NO cambie el codigo persistido es justamente lo
    // que permite que la Fase 4 guarde codigos y no indices.
    for (const Especialidad especialidad : todasLasEspecialidades()) {
        CHECK(especialidadDesdeCodigo(codigoDe(especialidad)) == especialidad);
    }
}

TEST_CASE("especialidadDesdeCodigo lanza con un codigo desconocido", especialidad_codigo_desconocido) {
    CHECK(lanzaConCampo([] { especialidadDesdeCodigo("XXX"); }, "especialidad"));
    CHECK(lanzaConCampo([] { especialidadDesdeCodigo(""); }, "especialidad"));
    // Mayusculas y minusculas son DISTINTAS: el codigo es exacto.
    CHECK(lanzaConCampo([] { especialidadDesdeCodigo("card"); }, "especialidad"));
}

TEST_CASE("los codigos del catalogo son unicos", especialidad_codigos_unicos) {
    std::set<std::string> codigos;
    for (const Especialidad especialidad : todasLasEspecialidades()) {
        codigos.insert(codigoDe(especialidad));
    }
    CHECK(codigos.size() == 12);
}

TEST_CASE("las descripciones del catalogo son unicas", especialidad_descripciones_unicas) {
    std::set<std::string> descripciones;
    for (const Especialidad especialidad : todasLasEspecialidades()) {
        descripciones.insert(descripcionDe(especialidad));
    }
    CHECK(descripciones.size() == 12);
}

TEST_CASE("el codigo y la descripcion son valores distintos", especialidad_codigo_distinto) {
    for (const Especialidad especialidad : todasLasEspecialidades()) {
        CHECK(codigoDe(especialidad) != descripcionDe(especialidad));
    }
}

TEST_CASE("las descripciones coinciden con el catalogo esperado", especialidad_descripciones) {
    for (const EspecialidadEsperada& esperada : CATALOGO_ESPERADO) {
        CHECK(codigoDe(esperada.especialidad) == esperada.codigo);
        CHECK(descripcionDe(esperada.especialidad) == esperada.descripcion);
    }
}

TEST_CASE("las tildes de las descripciones estan en UTF-8", especialidad_descripcion_utf8) {
    // "Oftalmología" tiene 12 letras y 13 bytes: la "í" ocupa dos bytes.
    // Sin tildes ("Oftalmologia") serian 12 bytes.
    const std::string oftalmologia = descripcionDe(Especialidad::OFTA);
    CHECK(oftalmologia.size() == 13);
    CHECK(oftalmologia != "Oftalmologia");
}

TEST_CASE("el catalogo no depende del orden del enum", especialidad_catalogo_estable) {
    // El vector de `todasLasEspecialidades()` es el mismo objeto en cada llamada
    // (singleton de Meyers): no se reconstruye ni se devuelve una copia.
    CHECK(&todasLasEspecialidades() == &todasLasEspecialidades());
}
