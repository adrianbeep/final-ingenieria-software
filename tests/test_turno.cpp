#include "test_framework.h"

#include "domain/Materia.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "utils/Fecha.h"

#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// Tests de `Turno` y de `EstadoTurno`: la relacion entre un profesor y un
// alumno, con sus reglas de validacion y sus operadores.
//
// `Turno` NO hereda de `Persona`: no tiene nombre, ni email, ni telefono, y
// heredar seria un "es-un" falso. Tiene-un un profesor y un alumno, pero
// guarda solo sus ids.
// ---------------------------------------------------------------------------

// --- Regla de Cinco (se comprueba en tiempo de compilacion) ---

static_assert(std::is_copy_constructible<Turno>::value, "Turno debe ser copiable");
static_assert(std::is_move_constructible<Turno>::value, "Turno debe ser movible");
static_assert(std::is_destructible<Turno>::value, "Turno debe ser destructible");

TEST_CASE("el turno por defecto tiene estado Activo y sin reemplazo", entidades_turno_defecto) {
    const Turno turno;
    CHECK(turno.getId() == -1);
    CHECK(turno.getEstado() == EstadoTurno::Activo);
    CHECK(turno.getIdTurnoReemplazante() == -1);
    CHECK(turno.getDuracionMin() == 0);
    CHECK(turno.getAula().empty());
    CHECK(!turno.esValido());
}

TEST_CASE("el constructor del turno guarda todos los datos", entidades_turno_constructor) {
    const Turno turno(14, 3, 7, Materia::ING1, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    CHECK(turno.getId() == 14);
    CHECK(turno.getIdProfesor() == 3);
    CHECK(turno.getIdAlumno() == 7);
    CHECK(turno.getMateria() == Materia::ING1);
    CHECK(turno.getFecha() == Fecha(2026, 10, 8));
    CHECK(turno.getHora() == Hora(9, 0));
    CHECK(turno.getDuracionMin() == 60);
    CHECK(turno.getAula() == "A-3");
    CHECK(turno.getEstado() == EstadoTurno::Activo);
    CHECK(turno.getIdTurnoReemplazante() == -1);
    CHECK(turno.esValido());
}

TEST_CASE("el setter de duracion rechaza un dato invalido", entidades_set_duracion) {
    Turno turno;
    CHECK(lanzaConCampo([&] { turno.setDuracionMin(0); }, "duracionMin"));
    CHECK(lanzaConCampo([&] { turno.setDuracionMin(-30); }, "duracionMin"));
    CHECK(lanzaConCampo([&] { turno.setDuracionMin(20); }, "duracionMin"));    // no multiplo de 15
    CHECK(lanzaConCampo([&] { turno.setDuracionMin(255); }, "duracionMin"));   // supera el maximo
    CHECK(!lanzaConCampo([&] { turno.setDuracionMin(60); }, "duracionMin"));
    CHECK(!lanzaConCampo([&] { turno.setDuracionMin(240); }, "duracionMin"));
}

TEST_CASE("el setter de aula rechaza un dato invalido", entidades_set_aula) {
    Turno turno;
    CHECK(lanzaConCampo([&] { turno.setAula(""); }, "aula"));
    CHECK(lanzaConCampo([&] { turno.setAula("A-3|B"); }, "aula"));
    CHECK(lanzaConCampo([&] { turno.setAula("123456789012345678901"); }, "aula"));
    CHECK(!lanzaConCampo([&] { turno.setAula("A-3"); }, "aula"));
    CHECK(!lanzaConCampo([&] { turno.setAula("Aula 3"); }, "aula"));
}

TEST_CASE("el setter de idTurnoReemplazante rechaza un dato invalido", entidades_set_reemplazante) {
    Turno turno;
    CHECK(lanzaConCampo([&] { turno.setIdTurnoReemplazante(0); }, "idTurnoReemplazante"));
    CHECK(lanzaConCampo([&] { turno.setIdTurnoReemplazante(-2); }, "idTurnoReemplazante"));
    CHECK(!lanzaConCampo([&] { turno.setIdTurnoReemplazante(14); }, "idTurnoReemplazante"));
    CHECK(!lanzaConCampo([&] { turno.setIdTurnoReemplazante(-1); }, "idTurnoReemplazante"));
}

TEST_CASE("los setters de fecha y hora validan el valor", entidades_set_fecha_hora) {
    Turno turno;
    // La `Fecha` y la `Hora` de la Fase 1 admiten valores invalidos: la
    // validacion vive en `Fecha::esValida` / `Hora::esValida`, asi que los
    // setters del turno tienen que validarlos.
    CHECK(lanzaConCampo([&] { turno.setFecha(Fecha(2026, 13, 45)); }, "fecha"));
    CHECK(lanzaConCampo([&] { turno.setFecha(Fecha(2026, 2, 30)); }, "fecha"));
    CHECK(lanzaConCampo([&] { turno.setHora(Hora(23, 0)); }, "hora"));
    CHECK(lanzaConCampo([&] { turno.setHora(Hora(9, 15)); }, "hora"));
    CHECK(!lanzaConCampo([&] { turno.setFecha(Fecha(2026, 10, 8)); }, "fecha"));
    CHECK(!lanzaConCampo([&] { turno.setHora(Hora(9, 30)); }, "hora"));
}

TEST_CASE("Turno ordena y compara por id", entidades_turno_operadores) {
    const Turno primero(14, 3, 7, Materia::ING1, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    const Turno segundo(15, 3, 9, Materia::ING1, Fecha(2026, 10, 9), Hora(10, 0), 60, "A-3",
                        EstadoTurno::Activo, 14);
    CHECK(primero < segundo);
    CHECK(!(segundo < primero));
    CHECK(!(primero == segundo));

    const Turno copia(14, 9, 9, Materia::ALG1, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    CHECK(primero == copia);
}
TEST_CASE("el constructor del turno tambien valida los datos", entidades_turno_constructor_valida) {
    CHECK(lanzaConCampo([] { Turno(14, 3, 7, Materia::ING1, Fecha(2026, 10, 8), Hora(9, 0), 20,
                                   "A-3"); },
                        "duracionMin"));
}

TEST_CASE("el turno se copia (Regla de Cinco)", entidades_turno_regla_de_cinco) {
    const Turno turno(14, 3, 7, Materia::ING1, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    const Turno copia(turno);
    CHECK(copia.getId() == 14);
    CHECK(copia.getMateria() == Materia::ING1);
}
