#include "test_framework.h"

#include "domain/Materia.h"
#include "entities/Alumno.h"
#include "entities/Profesor.h"
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

TEST_CASE("el profesor escribe el registro canonico", formato_profesor_registro) {
    const Profesor profesor(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                            "Algoritmos");
    CHECK(serializar(profesor)
          == "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|Algoritmos");
}

TEST_CASE("el alumno escribe el registro canonico", formato_alumno_registro) {
    const Alumno alumno(7, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900", "MAT-20391");
    CHECK(serializar(alumno) == "7|Lopez|Martin|martin.lopez@unlp.edu.ar|2214889900|1|MAT-20391");
}

TEST_CASE("el turno escribe el registro canonico", formato_turno_registro) {
    const Turno turno(14, 3, 7, Materia::ING1, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    CHECK(serializar(turno) == "14|3|7|ING1|2026-10-08|09:00|60|A-3|Activo|-1");

    const Turno reemplazo(15, 3, 9, Materia::ING1, Fecha(2026, 10, 9), Hora(10, 0), 60, "A-3",
                          EstadoTurno::Activo, 14);
    CHECK(serializar(reemplazo) == "15|3|9|ING1|2026-10-09|10:00|60|A-3|Activo|14");
}

TEST_CASE("el round trip del profesor conserva todos los campos", formato_profesor_round_trip) {
    const Profesor original(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                            "Algoritmos");
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
    CHECK(campos[7] == original.getEspecialidad());

    // Campo por campo del objeto releido: sin usar `operator==`.
    const Profesor releido = Profesor::fromRegistro(registro);
    CHECK(releido.getId() == original.getId());
    CHECK(releido.getNombre() == original.getNombre());
    CHECK(releido.getApellido() == original.getApellido());
    CHECK(releido.getEmail() == original.getEmail());
    CHECK(releido.getTelefono() == original.getTelefono());
    CHECK(releido.esActivo() == original.esActivo());
    CHECK(releido.getLegajo() == original.getLegajo());
    CHECK(releido.getEspecialidad() == original.getEspecialidad());
}

TEST_CASE("el round trip del alumno conserva todos los campos", formato_alumno_round_trip) {
    const Alumno original(7, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900", "MAT-20391");
    const std::string registro = serializar(original);

    const std::vector<std::string> campos = Utils::separar(registro, '|');
    CHECK(campos.size() == 7);
    CHECK(campos[0] == "7");
    CHECK(campos[1] == original.getNombre());
    CHECK(campos[2] == original.getApellido());
    CHECK(campos[3] == original.getEmail());
    CHECK(campos[4] == original.getTelefono());
    CHECK(campos[5] == "1");
    CHECK(campos[6] == original.getMatricula());

    const Alumno releido = Alumno::fromRegistro(registro);
    CHECK(releido.getId() == original.getId());
    CHECK(releido.getNombre() == original.getNombre());
    CHECK(releido.getApellido() == original.getApellido());
    CHECK(releido.getEmail() == original.getEmail());
    CHECK(releido.getTelefono() == original.getTelefono());
    CHECK(releido.esActivo() == original.esActivo());
    CHECK(releido.getMatricula() == original.getMatricula());
}

TEST_CASE("el round trip del turno conserva todos los campos", formato_turno_round_trip) {
    const Turno original(15, 3, 9, Materia::ING1, Fecha(2026, 10, 9), Hora(10, 0), 60, "A-3",
                         EstadoTurno::Reprogramado, 14);
    const std::string registro = serializar(original);

    const std::vector<std::string> campos = Utils::separar(registro, '|');
    CHECK(campos.size() == 10);
    CHECK(campos[0] == "15");
    CHECK(campos[1] == "3");
    CHECK(campos[2] == "9");
    CHECK(campos[3] == "ING1");
    CHECK(campos[4] == "2026-10-09");
    CHECK(campos[5] == "10:00");
    CHECK(campos[6] == "60");
    CHECK(campos[7] == "A-3");
    CHECK(campos[8] == "Reprogramado");
    CHECK(campos[9] == "14");

    const Turno releido = Turno::fromRegistro(registro);
    CHECK(releido.getId() == original.getId());
    CHECK(releido.getIdProfesor() == original.getIdProfesor());
    CHECK(releido.getIdAlumno() == original.getIdAlumno());
    CHECK(releido.getMateria() == original.getMateria());
    CHECK(releido.getFecha() == original.getFecha());
    CHECK(releido.getHora() == original.getHora());
    CHECK(releido.getDuracionMin() == original.getDuracionMin());
    CHECK(releido.getAula() == original.getAula());
    CHECK(releido.getEstado() == original.getEstado());
    CHECK(releido.getIdTurnoReemplazante() == original.getIdTurnoReemplazante());
}

TEST_CASE("el esquema y el registro tienen la misma cantidad de campos", formato_esquema_consistente) {
    const Profesor profesor(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                            "Algoritmos");
    const Alumno alumno(7, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900", "MAT-20391");
    const Turno turno(14, 3, 7, Materia::ING1, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");

    CHECK(Utils::separar(Profesor::esquema(), '|').size()
          == Utils::separar(serializar(profesor), '|').size());
    CHECK(Utils::separar(Alumno::esquema(), '|').size()
          == Utils::separar(serializar(alumno), '|').size());
    CHECK(Utils::separar(Turno::esquema(), '|').size()
          == Utils::separar(serializar(turno), '|').size());

    // Y que el esquema diga las mismas cosas que el registro.
    CHECK(Profesor::esquema() == "id|nombre|apellido|email|telefono|activo|legajo|especialidad");
    CHECK(Alumno::esquema() == "id|nombre|apellido|email|telefono|activo|matricula");
    CHECK(Turno::esquema()
          == "id|idProfesor|idAlumno|codigoMateria|fecha|hora|duracionMin|aula|estado|idTurnoReemplazante");
}

TEST_CASE("fromRegistro lanza con una cantidad de campos incorrecta", formato_cantidad_campos) {
    // Faltan campos.
    CHECK(lanzaConCampo([] { Profesor::fromRegistro("1|Garcia|Ana|ana@mail.com|2214556677|1"); },
                        "registro"));
    CHECK(lanzaConCampo([] { Alumno::fromRegistro("7|Lopez|Martin|"); }, "registro"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|ING1|2026-10-08|09:00|60|A-3|Activo"); },
                        "registro"));

    // Sobran campos.
    CHECK(lanzaConCampo([] {
                Profesor::fromRegistro(
                    "1|Garcia|Ana|ana@mail.com|2214556677|1|LEG-1042|Algoritmos|extra");
            },
            "registro"));
    CHECK(lanzaConCampo([] { Alumno::fromRegistro("7|Lopez|Martin|a@mail.com|2214889900|1|MAT-1|x"); },
                        "registro"));
    CHECK(lanzaConCampo([] {
                Turno::fromRegistro("14|3|7|ING1|2026-10-08|09:00|60|A-3|Activo|-1|extra");
            },
            "registro"));

    // Linea vacia.
    CHECK(lanzaConCampo([] { Profesor::fromRegistro(""); }, "registro"));
    CHECK(lanzaConCampo([] { Alumno::fromRegistro(""); }, "registro"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro(""); }, "registro"));
}

TEST_CASE("fromRegistro lanza con un campo con dato invalido", formato_campo_invalido) {
    CHECK(lanzaConCampo([] {
                Profesor::fromRegistro("1|Garcia|Ana|sin arroba|2214556677|1|LEG-1042|Algoritmos");
            },
            "email"));
    CHECK(lanzaConCampo([] { Alumno::fromRegistro("7|Lopez|Martin|a@mail.com|2214889900|2|MAT-1"); },
                        "activo"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|XXX|2026-10-08|09:00|60|A-3|Activo|-1"); },
                        "materia"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|ING1|2026-13-45|09:00|60|A-3|Activo|-1"); },
                        "fecha"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|ING1|2026-10-08|25:00|60|A-3|Activo|-1"); },
                        "hora"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|ING1|2026-10-08|09:00|20|A-3|Activo|-1"); },
                        "duracionMin"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|ING1|2026-10-08|09:00|60|A-3|Pausado|-1"); },
                        "estado"));
    CHECK(lanzaConCampo([] { Turno::fromRegistro("14|3|7|ING1|2026-10-08|09:00|60|A-3|Activo|0"); },
                        "idTurnoReemplazante"));
    CHECK(lanzaConCampo([] { Profesor::fromRegistro("1|Garcia|Ana|ana@mail.com|2214556677|1|LEG-1042|"); },
                        "especialidad"));
}

TEST_CASE("el separador no puede aparecer dentro de un campo", formato_separador_excluido) {
    // El alfabeto cerrado de los validadores excluye el '|' POR
    // CONSTRUCCION: un dato con separador ni siquiera se puede guardar, asi
    // que el archivo de datos no se puede desalinear.
    Profesor profesor;
    CHECK(lanzaConCampo([&] { profesor.setEspecialidad("con|pipe"); }, "especialidad"));
    CHECK(lanzaConCampo([&] { profesor.setEmail("a|b@mail.com"); }, "email"));

    Alumno alumno;
    CHECK(lanzaConCampo([&] { alumno.setMatricula("MAT|1"); }, "matricula"));

    Turno turno;
    CHECK(lanzaConCampo([&] { turno.setAula("A-3|B"); }, "aula"));
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

TEST_CASE("el turno viaja con el codigo de la materia, no con el indice", formato_codigo_materia) {
    // El codigo es estable ante reordenamientos del enum: por eso viaja el
    // codigo y no el indice.
    const Turno turno(14, 3, 7, Materia::ING1, Fecha(2026, 10, 8), Hora(9, 0), 60, "A-3");
    CHECK(codigoDe(turno.getMateria()) == "ING1");

    const std::vector<std::string> campos = Utils::separar(serializar(turno), '|');
    CHECK(campos[3] == codigoDe(Materia::ING1));
    CHECK(campos[3] != "3");  // 3 es el indice del enum, NO el codigo
}

TEST_CASE("los ejemplos del contrato de formato se parsean", formato_ejemplos) {
    const Profesor profesor = Profesor::fromRegistro(
        "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|Algoritmos");
    CHECK(profesor.getId() == 1);
    CHECK(profesor.getLegajo() == "LEG-1042");
    CHECK(profesor.getEspecialidad() == "Algoritmos");
    CHECK(serializar(profesor)
          == "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|Algoritmos");

    const Alumno alumno =
        Alumno::fromRegistro("7|Lopez|Martin|martin.lopez@unlp.edu.ar|2214889900|1|MAT-20391");
    CHECK(alumno.getId() == 7);
    CHECK(alumno.getMatricula() == "MAT-20391");
    CHECK(serializar(alumno) == "7|Lopez|Martin|martin.lopez@unlp.edu.ar|2214889900|1|MAT-20391");

    const Turno turno = Turno::fromRegistro("14|3|7|ING1|2026-10-08|09:00|60|A-3|Activo|-1");
    CHECK(turno.getId() == 14);
    CHECK(turno.getMateria() == Materia::ING1);
    CHECK(turno.getIdTurnoReemplazante() == -1);
    CHECK(serializar(turno) == "14|3|7|ING1|2026-10-08|09:00|60|A-3|Activo|-1");
}

TEST_CASE("un registro con finales de linea de Windows se parsea igual", formato_fin_de_linea) {
    // En Windows el archivo se escribe con "\r\n": el "\r" pegado al ultimo
    // campo no puede romper el parseo (por eso `Utils::separar` recorta).
    const std::string registro = "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|Algoritmos\r\n";
    const Profesor profesor = Profesor::fromRegistro(registro);
    CHECK(profesor.getEspecialidad() == "Algoritmos");
    CHECK(serializar(profesor)
          == "1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|LEG-1042|Algoritmos");
}
