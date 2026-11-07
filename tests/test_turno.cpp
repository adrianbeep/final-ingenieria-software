#include "test_framework.h"

#include "domain/Especialidad.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "utils/Fecha.h"
#include "utils/Utils.h"

#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// Tests de `Turno` y de `EstadoTurno`: la relacion entre un medico y un
// paciente, con sus reglas de validacion y sus operadores.
//
// `Turno` NO hereda de `Persona`: no tiene nombre, ni email, ni telefono, y
// heredar seria un "es-un" falso. Tiene-un un medico y un paciente, pero
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
    CHECK(turno.getConsultorio().empty());
    CHECK(!turno.esValido());
}

TEST_CASE("el constructor del turno guarda todos los datos", entidades_turno_constructor) {
    const Turno turno(14, 3, 7, Especialidad::TRAU, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    CHECK(turno.getId() == 14);
    CHECK(turno.getIdMedico() == 3);
    CHECK(turno.getIdPaciente() == 7);
    CHECK(turno.getEspecialidad() == Especialidad::TRAU);
    CHECK(turno.getFecha() == Fecha(2026, 10, 8));
    CHECK(turno.getHora() == Hora(9, 0));
    CHECK(turno.getDuracionMin() == 60);
    CHECK(turno.getConsultorio() == "A-3");
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

TEST_CASE("el setter de consultorio rechaza un dato invalido", entidades_set_consultorio) {
    Turno turno;
    CHECK(lanzaConCampo([&] { turno.setConsultorio(""); }, "consultorio"));
    CHECK(lanzaConCampo([&] { turno.setConsultorio("A-3|B"); }, "consultorio"));
    CHECK(lanzaConCampo([&] { turno.setConsultorio("123456789012345678901"); }, "consultorio"));
    CHECK(!lanzaConCampo([&] { turno.setConsultorio("A-3"); }, "consultorio"));
    CHECK(!lanzaConCampo([&] { turno.setConsultorio("Sala 3"); }, "consultorio"));
}

TEST_CASE("el consultorio acepta letras acentuadas y las guarda intactas", entidades_consultorio_acentuada) {
    Turno turno;
    // 'ñ' (U+00F1) son 0xC3 0xB1. El escape hexadecimal va en un literal
    // propio porque se extiende hasta el proximo caracter hexadecimal.
    const std::string consultorioAcentuada = "Sala A" "\xC3\xB1" "o";
    CHECK(!lanzaConCampo([&] { turno.setConsultorio(consultorioAcentuada); }, "consultorio"));
    // El valor vuelve IDENTICO: la tilde no se convierte ni se pierde.
    CHECK(turno.getConsultorio() == consultorioAcentuada);
    CHECK(turno.getConsultorio() == "Sala A" "\xC3\xB1" "o");
    // Con 'ó' (U+00F3, 0xC3 0xB3) tambien.
    CHECK(!lanzaConCampo([&] { turno.setConsultorio("Sala A" "\xC3\xB3" "o"); }, "consultorio"));
    CHECK(turno.getConsultorio() == "Sala A" "\xC3\xB3" "o");
}

TEST_CASE("el limite del consultorio se cuenta en caracteres y no en bytes", entidades_consultorio_longitud) {
    Turno turno;
    // Un consultorio de EXACTAMENTE 20 caracteres que incluye una tilde: son 21
    // bytes, pero 20 caracteres, y el limite es de 20 CARACTERES. Si se
    // contaran bytes, este valor seria rechazado sin motivo.
    const std::string consultorioConTilde = "Sala A" "\xC3\xB1" "o";
    std::string consultorioAlLimite = consultorioConTilde;
    consultorioAlLimite.append(20 - Utils::cantidadCaracteres(consultorioConTilde), 'X');
    CHECK(Utils::cantidadCaracteres(consultorioAlLimite) == 20);  // 20 caracteres
    CHECK(consultorioAlLimite.size() == 21);                      // 21 bytes
    CHECK(!lanzaConCampo([&] { turno.setConsultorio(consultorioAlLimite); }, "consultorio"));
    CHECK(turno.getConsultorio() == consultorioAlLimite);
    // Un caracter mas (21 caracteres) SI se rechaza: el limite se cuenta en
    // caracteres, con o sin tildes.
    const std::string consultorioAlLimiteMasUno = consultorioAlLimite + "X";
    CHECK(Utils::cantidadCaracteres(consultorioAlLimiteMasUno) == 21);
    CHECK(lanzaConCampo([&] { turno.setConsultorio(consultorioAlLimiteMasUno); }, "consultorio"));
    // Vacio.
    CHECK(lanzaConCampo([&] { turno.setConsultorio(""); }, "consultorio"));
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
    const Turno primero(14, 3, 7, Especialidad::TRAU, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    const Turno segundo(15, 3, 9, Especialidad::TRAU, Fecha(2026, 10, 9), Hora(10, 0), 60, "A-3",
                        EstadoTurno::Activo, 14);
    CHECK(primero < segundo);
    CHECK(!(segundo < primero));
    CHECK(!(primero == segundo));

    const Turno copia(14, 9, 9, Especialidad::CARD, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    CHECK(primero == copia);
}
TEST_CASE("el constructor del turno tambien valida los datos", entidades_turno_constructor_valida) {
    CHECK(lanzaConCampo([] { Turno(14, 3, 7, Especialidad::TRAU, Fecha(2026, 10, 8), Hora(9, 0), 20,
                                   "A-3"); },
                        "duracionMin"));
}

TEST_CASE("el turno se copia (Regla de Cinco)", entidades_turno_regla_de_cinco) {
    const Turno turno(14, 3, 7, Especialidad::TRAU, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    const Turno copia(turno);
    CHECK(copia.getId() == 14);
    CHECK(copia.getEspecialidad() == Especialidad::TRAU);
}
