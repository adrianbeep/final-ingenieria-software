#include "test_framework.h"

#include "domain/Especialidad.h"
#include "entities/Paciente.h"
#include "entities/Medico.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "utils/Fecha.h"
#include "utils/Utils.h"

#include <sstream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Tests del FORMATO CANONICO (el contrato con la Fase 4).
//
// EL ROUND-TRIP SE COMPARA CAMPO POR CAMPO, NO CON `operator==`:
// `operator==` compara por id (y tipo), asi que un test `fromRegistro(linea)
// == original` pasaria aunque se perdiera el nombre. Ese criterio era
// trivial; el real compara cada campo.
//
// Tambien se verifica que `esquema()` y lo que escribe `operator<<` tengan la
// MISMA cantidad de campos: ese test detecta el fallo en esta fase y no en la
// Fase 4.
//
// LA ESPECIALIDAD VIAJA CON SU CODIGO
//
// El medico escribe el CODIGO de la especialidad ("CARD"), no la descripcion:
// el codigo es corto y estable, y la descripcion puede cambiar sin invalidar
// los archivos de datos.
// ---------------------------------------------------------------------------

namespace {

// Serializa una entidad en su registro canonico.
template <typename T>
std::string serializar(const T& entidad) {
    std::ostringstream sumidero;
    sumidero << entidad;
    return sumidero.str();
}

// `lanzaConCampo` viene de test_framework.h: es el helper compartido para
// comprobar el lanzamiento de una excepcion del dominio con su campo.

} // namespace

TEST_CASE("el medico escribe el registro canonico", formato_medico_registro) {
    const Medico medico(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                         Especialidad::CARD);
    CHECK(serializar(medico)
          == "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|CARD");
}

TEST_CASE("el paciente escribe el registro canonico", formato_paciente_registro) {
    const Paciente paciente(7, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900",
                             "20391001");
    CHECK(serializar(paciente)
          == "7|Lopez|Martin|martin.lopez@unlp.edu.ar|2214889900|1|20391001");
}

TEST_CASE("el turno escribe el registro canonico", formato_turno_registro) {
    const Turno turno(14, 3, 7, Especialidad::TRAU, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    CHECK(serializar(turno) == "14|3|7|TRAU|2026-10-08|09:00|60|A-3|Activo|-1");

    const Turno reemplazo(15, 3, 9, Especialidad::TRAU, Fecha(2026, 10, 9), Hora(10, 0), 60, "A-3",
                          EstadoTurno::Activo, 14);
    CHECK(serializar(reemplazo) == "15|3|9|TRAU|2026-10-09|10:00|60|A-3|Activo|14");
}

TEST_CASE("el round trip del medico conserva todos los campos", formato_medico_round_trip) {
    const Medico original(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                           Especialidad::CARD);
    const std::string registro = serializar(original);

    // Campo por campo del registro escrito.
    const std::vector<std::string> campos = Utils::separar(registro, '|');
    CHECK(campos.size() == 8);
    CHECK(campos[0] == "1");
    CHECK(campos[1] == original.getNombre());
    CHECK(campos[2] == original.getApellido());
    CHECK(campos[3] == original.getEmail());
    CHECK(campos[4] == original.getTelefono());
    CHECK(campos[5] == "1");
    CHECK(campos[6] == original.getLegajo());
    CHECK(campos[7] == codigoDe(original.getEspecialidad()));

    // Campo por campo del objeto releido: sin usar `operator==`.
    const Medico releido = Medico::fromRegistro(registro);
    CHECK(releido.getId() == original.getId());
    CHECK(releido.getNombre() == original.getNombre());
    CHECK(releido.getApellido() == original.getApellido());
    CHECK(releido.getEmail() == original.getEmail());
    CHECK(releido.getTelefono() == original.getTelefono());
    CHECK(releido.esActivo() == original.esActivo());
    CHECK(releido.getLegajo() == original.getLegajo());
    CHECK(releido.getEspecialidad() == original.getEspecialidad());
}

TEST_CASE("el round trip del paciente conserva todos los campos", formato_paciente_round_trip) {
    const Paciente original(7, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900",
                             "20391001");
    const std::string registro = serializar(original);

    const std::vector<std::string> campos = Utils::separar(registro, '|');
    CHECK(campos.size() == 7);
    CHECK(campos[0] == "7");
    CHECK(campos[1] == original.getNombre());
    CHECK(campos[2] == original.getApellido());
    CHECK(campos[3] == original.getEmail());
    CHECK(campos[4] == original.getTelefono());
    CHECK(campos[5] == "1");
    CHECK(campos[6] == original.getDni());

    const Paciente releido = Paciente::fromRegistro(registro);
    CHECK(releido.getId() == original.getId());
    CHECK(releido.getNombre() == original.getNombre());
    CHECK(releido.getApellido() == original.getApellido());
    CHECK(releido.getEmail() == original.getEmail());
    CHECK(releido.getTelefono() == original.getTelefono());
    CHECK(releido.esActivo() == original.esActivo());
    CHECK(releido.getDni() == original.getDni());
}

TEST_CASE("el round trip del turno conserva todos los campos", formato_turno_round_trip) {
    const Turno original(15, 3, 9, Especialidad::TRAU, Fecha(2026, 10, 9), Hora(10, 0), 60, "A-3",
                         EstadoTurno::Reprogramado, 14);
    const std::string registro = serializar(original);

    const std::vector<std::string> campos = Utils::separar(registro, '|');
    CHECK(campos.size() == 10);
    CHECK(campos[0] == "15");
    CHECK(campos[1] == "3");
    CHECK(campos[2] == "9");
    CHECK(campos[3] == "TRAU");
    CHECK(campos[4] == "2026-10-09");
    CHECK(campos[5] == "10:00");
    CHECK(campos[6] == "60");
    CHECK(campos[7] == "A-3");
    CHECK(campos[8] == "Reprogramado");
    CHECK(campos[9] == "14");

    const Turno releido = Turno::fromRegistro(registro);
    CHECK(releido.getId() == original.getId());
    CHECK(releido.getIdMedico() == original.getIdMedico());
    CHECK(releido.getIdPaciente() == original.getIdPaciente());
    CHECK(releido.getEspecialidad() == original.getEspecialidad());
    CHECK(releido.getFecha() == original.getFecha());
    CHECK(releido.getHora() == original.getHora());
    CHECK(releido.getDuracionMin() == original.getDuracionMin());
    CHECK(releido.getConsultorio() == original.getConsultorio());
    CHECK(releido.getEstado() == original.getEstado());
    CHECK(releido.getIdTurnoReemplazante() == original.getIdTurnoReemplazante());
}

TEST_CASE("el esquema y el registro tienen la misma cantidad de campos", formato_esquema_consistente) {
    const Medico medico(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                         Especialidad::CARD);
    const Paciente paciente(7, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900",
                             "20391001");
    const Turno turno(14, 3, 7, Especialidad::TRAU, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");

    CHECK(Utils::separar(Medico::esquema(), '|').size()
          == Utils::separar(serializar(medico), '|').size());
    CHECK(Utils::separar(Paciente::esquema(), '|').size()
          == Utils::separar(serializar(paciente), '|').size());
    CHECK(Utils::separar(Turno::esquema(), '|').size()
          == Utils::separar(serializar(turno), '|').size());

    // Y que el esquema diga las mismas cosas que el registro.
    CHECK(Medico::esquema() == "id|nombre|apellido|email|telefono|activo|legajo|especialidad");
    CHECK(Paciente::esquema() == "id|nombre|apellido|email|telefono|activo|dni");
    CHECK(Turno::esquema()
          == "id|idMedico|idPaciente|codigoEspecialidad|fecha|hora|duracionMin|consultorio|estado|idTurnoReemplazante");
}

TEST_CASE("fromRegistro lanza con una cantidad de campos incorrecta", formato_cantidad_campos) {
    // Faltan campos.
    CHECK(lanzaConCampo([] { Medico::fromRegistro("1|Garcia|Ana|ana@mail.com|2214556677|1"); },
                        "registro"));
    CHECK(lanzaConCampo([] { Paciente::fromRegistro("7|Lopez|Martin|"); }, "registro"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|TRAU|2026-10-08|09:00|60|A-3|Activo"); },
                        "registro"));

    // Sobran campos.
    CHECK(lanzaConCampo([] {
                Medico::fromRegistro(
                    "1|Garcia|Ana|ana@mail.com|2214556677|1|LEG-1042|CARD|extra");
            },
            "registro"));
    CHECK(lanzaConCampo([] { Paciente::fromRegistro("7|Lopez|Martin|a@mail.com|2214889900|1|20391001|x"); },
                        "registro"));
    CHECK(lanzaConCampo([] {
                Turno::fromRegistro("14|3|7|TRAU|2026-10-08|09:00|60|A-3|Activo|-1|extra");
            },
            "registro"));

    // Linea vacia.
    CHECK(lanzaConCampo([] { Medico::fromRegistro(""); }, "registro"));
    CHECK(lanzaConCampo([] { Paciente::fromRegistro(""); }, "registro"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro(""); }, "registro"));
}

TEST_CASE("fromRegistro lanza con un campo con dato invalido", formato_campo_invalido) {
    CHECK(lanzaConCampo([] {
                Medico::fromRegistro("1|Garcia|Ana|sin arroba|2214556677|1|LEG-1042|CARD");
            },
            "email"));
    CHECK(lanzaConCampo([] { Paciente::fromRegistro("7|Lopez|Martin|a@mail.com|2214889900|2|20391001"); },
                        "activo"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|XXX|2026-10-08|09:00|60|A-3|Activo|-1"); },
                        "especialidad"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|TRAU|2026-13-45|09:00|60|A-3|Activo|-1"); },
                        "fecha"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|TRAU|2026-10-08|25:00|60|A-3|Activo|-1"); },
                        "hora"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|TRAU|2026-10-08|09:00|20|A-3|Activo|-1"); },
                        "duracionMin"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|TRAU|2026-10-08|09:00|60|A-3|Pausado|-1"); },
                        "estado"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|TRAU|2026-10-08|09:00|60|A-3|Activo|0"); },
                        "idTurnoReemplazante"));
    CHECK(lanzaConCampo([] { Medico::fromRegistro("1|Garcia|Ana|ana@mail.com|2214556677|1|LEG-1042|"); },
                        "especialidad"));
    // Y un dni que no es un numero de 7 u 8 digitos.
    CHECK(lanzaConCampo([] { Paciente::fromRegistro("7|Lopez|Martin|a@mail.com|2214889900|1|12345"); },
                        "dni"));
}

TEST_CASE("el separador no puede aparecer dentro de un campo", formato_separador_excluido) {
    // El alfabeto cerrado de los validadores excluye el '|' POR
    // CONSTRUCCION: un dato con separador ni siquiera se puede guardar, asi
    // que el archivo de datos no se puede desalinear.
    Medico medico;
    CHECK(lanzaConCampo([&] { medico.setEmail("a|b@mail.com"); }, "email"));
    // Y el codigo de la especialidad tampoco puede traer el separador.
    CHECK(lanzaConCampo([] { especialidadDesdeCodigo("con|pipe"); }, "especialidad"));

    Paciente paciente;
    CHECK(lanzaConCampo([&] { paciente.setDni("20|91001"); }, "dni"));

    Turno turno;
    CHECK(lanzaConCampo([&] { turno.setConsultorio("A-3|B"); }, "consultorio"));
}

TEST_CASE("los estados tienen texto y vuelven desde el texto", formato_estados) {
    CHECK(estadoATexto(EstadoTurno::Activo) == "Activo");
    CHECK(estadoATexto(EstadoTurno::Cancelado) == "Cancelado");
    CHECK(estadoATexto(EstadoTurno::Reprogramado) == "Reprogramado");

    CHECK(estadoDesdeTexto("Activo") == EstadoTurno::Activo);
    CHECK(estadoDesdeTexto("Cancelado") == EstadoTurno::Cancelado);
    CHECK(estadoDesdeTexto("Reprogramado") == EstadoTurno::Reprogramado);

    CHECK(lanzaConCampo([] { estadoDesdeTexto("Pausado"); }, "estado"));
    CHECK(lanzaConCampo([] { estadoDesdeTexto(""); }, "estado"));
}

TEST_CASE("el turno viaja con el codigo de la especialidad, no con el indice",
          formato_codigo_especialidad) {
    // El codigo es estable ante reordenamientos del enum: por eso viaja el
    // codigo y no el indice.
    const Turno turno(14, 3, 7, Especialidad::TRAU, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    CHECK(codigoDe(turno.getEspecialidad()) == "TRAU");

    const std::vector<std::string> campos = Utils::separar(serializar(turno), '|');
    CHECK(campos[3] == codigoDe(Especialidad::TRAU));
    CHECK(campos[3] != "3");  // 3 es el indice del enum, NO el codigo
}

TEST_CASE("los ejemplos del contrato de formato se parsean", formato_ejemplos) {
    const Medico medico = Medico::fromRegistro(
        "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|CARD");
    CHECK(medico.getId() == 1);
    CHECK(medico.getLegajo() == "LEG-1042");
    CHECK(medico.getEspecialidad() == Especialidad::CARD);
    CHECK(serializar(medico)
          == "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|CARD");

    const Paciente paciente = Paciente::fromRegistro(
        "7|Lopez|Martin|martin.lopez@unlp.edu.ar|2214889900|1|20391001");
    CHECK(paciente.getId() == 7);
    CHECK(paciente.getDni() == "20391001");
    CHECK(serializar(paciente)
          == "7|Lopez|Martin|martin.lopez@unlp.edu.ar|2214889900|1|20391001");

    const Turno turno = Turno::fromRegistro("14|3|7|TRAU|2026-10-08|09:00|60|A-3|Activo|-1");
    CHECK(turno.getId() == 14);
    CHECK(turno.getEspecialidad() == Especialidad::TRAU);
    CHECK(turno.getIdTurnoReemplazante() == -1);
    CHECK(serializar(turno) == "14|3|7|TRAU|2026-10-08|09:00|60|A-3|Activo|-1");
}

TEST_CASE("un registro con finales de linea de Windows se parsea igual", formato_fin_de_linea) {
    // En Windows el archivo se escribe con "\r\n": el "\r" pegado al ultimo
    // campo no puede romper el parseo (por eso `Utils::separar` recorta).
    const std::string registro =
        "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|CARD\r\n";
    const Medico medico = Medico::fromRegistro(registro);
    CHECK(medico.getEspecialidad() == Especialidad::CARD);
    CHECK(serializar(medico)
          == "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|CARD");
}
