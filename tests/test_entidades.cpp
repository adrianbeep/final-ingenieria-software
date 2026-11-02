#include "test_framework.h"

#include "entities/Alumno.h"
#include "entities/Persona.h"
#include "entities/Profesor.h"
#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// Tests de `Persona`, `Profesor` y `Alumno`: constructores, setters con
// validacion, getters, operadores y la Regla de Cinco.
//
// Los tests de `Turno` estan en test_turno.cpp, y los del formato canonico en
// test_formato.cpp.
//
// NOTA: `operator==` compara el id Y el tipo (el id es unico por entidad, no
// global), asi que el round trip del formato NO se puede verificar con `==`.
// Eso se verifica en test_formato.cpp, campo por campo.
// ---------------------------------------------------------------------------

namespace {

Profesor profesorDePrueba() {
    return Profesor(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                    "Algoritmos");
}

Alumno alumnoDePrueba() {
    return Alumno(7, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900", "MAT-20391");
}

// `Persona` es abstracta: para probar la base hace falta una derivada
// concreta.
class PersonaDePrueba : public Persona {
public:
    std::string getTipo() const override { return "PersonaDePrueba"; }
};

} // namespace

// --- Regla de Cinco y destructor virtual (se comprueba en tiempo de compilacion) ---
//
// OJO: `Persona` es abstracta, asi que no se puede "construir": por eso la
// constructibilidad se comprueba sobre una derivada concreta. La
// ASIGNABILIDAD si se puede comprobar sobre la base abstracta.

static_assert(std::has_virtual_destructor<Persona>::value,
              "Persona es base polimorfica: necesita destructor virtual");
static_assert(std::is_copy_assignable<Persona>::value, "Persona debe ser asignable por copia");
static_assert(std::is_move_assignable<Persona>::value, "Persona debe ser asignable por movimiento");
static_assert(std::is_copy_constructible<PersonaDePrueba>::value, "Persona debe ser copiable");
static_assert(std::is_move_constructible<PersonaDePrueba>::value, "Persona debe ser movible");
static_assert(std::is_copy_constructible<Profesor>::value, "Profesor debe ser copiable");
static_assert(std::is_move_constructible<Profesor>::value, "Profesor debe ser movible");
static_assert(std::is_copy_assignable<Profesor>::value, "Profesor debe ser asignable por copia");
static_assert(std::is_copy_constructible<Alumno>::value, "Alumno debe ser copiable");
static_assert(std::is_move_constructible<Alumno>::value, "Alumno debe ser movible");
static_assert(std::is_destructible<Profesor>::value, "Profesor debe ser destructible");
static_assert(std::is_destructible<Alumno>::value, "Alumno debe ser destructible");

TEST_CASE("el profesor por defecto esta sin asignar", entidades_profesor_defecto) {
    const Profesor profesor;
    CHECK(profesor.getId() == -1);
    CHECK(profesor.getNombre().empty());
    CHECK(profesor.getApellido().empty());
    CHECK(profesor.getEmail().empty());
    CHECK(profesor.getTelefono().empty());
    CHECK(profesor.getLegajo().empty());
    CHECK(profesor.getEspecialidad().empty());
    CHECK(profesor.esActivo());
    CHECK(!profesor.esValido());
    CHECK(Profesor::esquema().size() > 0);
}

TEST_CASE("el alumno por defecto esta sin asignar", entidades_alumno_defecto) {
    const Alumno alumno;
    CHECK(alumno.getId() == -1);
    CHECK(alumno.getNombre().empty());
    CHECK(alumno.getMatricula().empty());
    CHECK(alumno.esActivo());
    CHECK(!alumno.esValido());
}

TEST_CASE("el constructor del profesor guarda todos los datos", entidades_profesor_constructor) {
    const Profesor profesor = profesorDePrueba();
    CHECK(profesor.getId() == 1);
    CHECK(profesor.getNombre() == "Garcia");
    CHECK(profesor.getApellido() == "Ana");
    CHECK(profesor.getEmail() == "ana.garcia@unlp.edu.ar");
    CHECK(profesor.getTelefono() == "2214556677");
    CHECK(profesor.getLegajo() == "LEG-1042");
    CHECK(profesor.getEspecialidad() == "Algoritmos");
    CHECK(profesor.esActivo());
    CHECK(profesor.esValido());
    CHECK(profesor.getTipo() == "Profesor");
}

TEST_CASE("el constructor del alumno guarda todos los datos", entidades_alumno_constructor) {
    const Alumno alumno = alumnoDePrueba();
    CHECK(alumno.getId() == 7);
    CHECK(alumno.getNombre() == "Lopez");
    CHECK(alumno.getApellido() == "Martin");
    CHECK(alumno.getEmail() == "martin.lopez@unlp.edu.ar");
    CHECK(alumno.getTelefono() == "2214889900");
    CHECK(alumno.getMatricula() == "MAT-20391");
    CHECK(alumno.esActivo());
    CHECK(alumno.esValido());
    CHECK(alumno.getTipo() == "Alumno");
}

TEST_CASE("el setter de nombre rechaza un dato invalido", entidades_set_nombre) {
    Profesor profesor;
    CHECK(lanzaConCampo([&] { profesor.setNombre("123"); }, "nombre"));
    CHECK(lanzaConCampo([&] { profesor.setNombre(""); }, "nombre"));
    CHECK(lanzaConCampo([&] { profesor.setNombre("   "); }, "nombre"));
    CHECK(lanzaConCampo([&] { profesor.setNombre("Ana3"); }, "nombre"));
    // Con tilde y enye SI es valido: `Utils::esAlfabetico` es consciente de UTF-8.
    CHECK(!lanzaConCampo([&] { profesor.setNombre("José"); }, "nombre"));
    CHECK(!lanzaConCampo([&] { profesor.setNombre("Maria de los Angeles"); }, "nombre"));
}

TEST_CASE("el setter de apellido rechaza un dato invalido", entidades_set_apellido) {
    Profesor profesor;
    CHECK(lanzaConCampo([&] { profesor.setApellido(""); }, "apellido"));
    CHECK(lanzaConCampo([&] { profesor.setApellido("Gomez1"); }, "apellido"));
}

TEST_CASE("el setter de email rechaza un dato invalido", entidades_set_email) {
    Profesor profesor;
    CHECK(lanzaConCampo([&] { profesor.setEmail(""); }, "email"));
    CHECK(lanzaConCampo([&] { profesor.setEmail("a@b"); }, "email"));         // muy corto
    CHECK(lanzaConCampo([&] { profesor.setEmail("con espacio@mail.com"); }, "email"));
    // Sin arroba y SIN espacios: si el valor tuviera un espacio, la prueba
    // pasaria por el espacio y no estaria probando la regla de la arroba.
    CHECK(lanzaConCampo([&] { profesor.setEmail("sinarroba.com"); }, "email"));
    // El separador del formato canonico no puede pasar: rompe el archivo.
    CHECK(lanzaConCampo([&] { profesor.setEmail("ana|garcia@mail.com"); }, "email"));
    CHECK(!lanzaConCampo([&] { profesor.setEmail("ana.garcia-1_x@unlp.edu.ar"); }, "email"));
}

TEST_CASE("el email acepta letras acentuadas y las guarda intactas", entidades_email_acentuado) {
    Profesor profesor;
    // 'í' (U+00ED) son 0xC3 0xAD. El escape hexadecimal va en un literal
    // propio porque se extiende hasta el proximo caracter hexadecimal.
    const std::string emailAcentuado = "ana.garc" "\xC3\xAD" "a@unlp.edu.ar";
    CHECK(!lanzaConCampo([&] { profesor.setEmail(emailAcentuado); }, "email"));
    // El valor vuelve IDENTICO: la tilde no se convierte ni se pierde.
    CHECK(profesor.getEmail() == emailAcentuado);
    CHECK(profesor.getEmail() == "ana.garc" "\xC3\xAD" "a@unlp.edu.ar");
    // Tambien con enye y con e acute.
    CHECK(!lanzaConCampo([&] { profesor.setEmail("mu" "\xC3\xB1" "oz@unlp.edu.ar"); }, "email"));
    CHECK(profesor.getEmail() == "mu" "\xC3\xB1" "oz@unlp.edu.ar");
    CHECK(!lanzaConCampo([&] { profesor.setEmail("jose" "\xC3\xA9" "@unlp.edu.ar"); }, "email"));
    CHECK(profesor.getEmail() == "jose" "\xC3\xA9" "@unlp.edu.ar");
}

TEST_CASE("el email sigue rechazando el separador y los valores malformados", entidades_email_malformado) {
    Profesor profesor;
    // El separador del formato canonico sigue excluido por construccion.
    CHECK(lanzaConCampo([&] { profesor.setEmail("a|b@x.com"); }, "email"));
    CHECK(lanzaConCampo([&] { profesor.setEmail("ana.garcia@unlp.edu.ar|x"); }, "email"));
    // Un espacio interno no se admite: el correo no lo lleva.
    CHECK(lanzaConCampo([&] { profesor.setEmail("ana garcia@unlp.edu.ar"); }, "email"));
    // Vacio y demasiado corto.
    CHECK(lanzaConCampo([&] { profesor.setEmail(""); }, "email"));
    CHECK(lanzaConCampo([&] { profesor.setEmail("a@b"); }, "email"));
    // El limite es de 60 caracteres: uno mas se rechaza.
    const std::string correoAlLimite = std::string(54, 'a') + "@x.com";  // 60 caracteres
    CHECK(Utils::cantidadCaracteres(correoAlLimite) == 60);
    CHECK(!lanzaConCampo([&] { profesor.setEmail(correoAlLimite); }, "email"));
    CHECK(lanzaConCampo([&] { profesor.setEmail(correoAlLimite + "a"); }, "email"));  // 61
    // Demasiado largo.
    const std::string correoLargo = std::string(61, 'a') + "@x.com";
    CHECK(lanzaConCampo([&] { profesor.setEmail(correoLargo); }, "email"));
    // Una arroba no alcanza: sin ella no es un correo.
    CHECK(lanzaConCampo([&] { profesor.setEmail("sinarroba.com"); }, "email"));
}

TEST_CASE("el setter de telefono rechaza un dato invalido", entidades_set_telefono) {
    Profesor profesor;
    CHECK(lanzaConCampo([&] { profesor.setTelefono(""); }, "telefono"));
    CHECK(lanzaConCampo([&] { profesor.setTelefono("123456"); }, "telefono"));  // muy corto
    CHECK(lanzaConCampo([&] { profesor.setTelefono("2214556677 letras"); }, "telefono"));
    CHECK(!lanzaConCampo([&] { profesor.setTelefono("+54 221 455-6677"); }, "telefono"));
}

TEST_CASE("el setter de legajo rechaza un dato invalido", entidades_set_legajo) {
    Profesor profesor;
    CHECK(lanzaConCampo([&] { profesor.setLegajo(""); }, "legajo"));
    CHECK(lanzaConCampo([&] { profesor.setLegajo("ABC"); }, "legajo"));       // muy corto
    CHECK(lanzaConCampo([&] { profesor.setLegajo("1234567890123"); }, "legajo"));  // muy largo
    CHECK(lanzaConCampo([&] { profesor.setLegajo("LEG 1042"); }, "legajo"));
    CHECK(!lanzaConCampo([&] { profesor.setLegajo("LEG-1042"); }, "legajo"));
}

TEST_CASE("el setter de especialidad rechaza un dato invalido", entidades_set_especialidad) {
    Profesor profesor;
    CHECK(lanzaConCampo([&] { profesor.setEspecialidad(""); }, "especialidad"));
    CHECK(lanzaConCampo([&] { profesor.setEspecialidad("con|pipe"); }, "especialidad"));
    CHECK(!lanzaConCampo([&] { profesor.setEspecialidad("Sistemas Operativos, primera parte"); },
                          "especialidad"));
}

TEST_CASE("la especialidad acepta letras acentuadas y las guarda intactas",
          entidades_especialidad_acentuada) {
    Profesor profesor;
    // 'ó' (U+00F3) son 0xC3 0xB3. El escape hexadecimal va en un literal
    // propio porque se extiende hasta el proximo caracter hexadecimal.
    const std::string especialidadAcentuada = "Redes y Comunicaci" "\xC3\xB3" "n";
    CHECK(!lanzaConCampo([&] { profesor.setEspecialidad(especialidadAcentuada); }, "especialidad"));
    // El valor vuelve IDENTICO: la tilde no se convierte ni se pierde.
    CHECK(profesor.getEspecialidad() == especialidadAcentuada);
    CHECK(profesor.getEspecialidad() == "Redes y Comunicaci" "\xC3\xB3" "n");
    // Con enye tambien.
    CHECK(!lanzaConCampo([&] { profesor.setEspecialidad("Dise" "\xC3\xB1" "o de Software"); },
                          "especialidad"));
    CHECK(profesor.getEspecialidad() == "Dise" "\xC3\xB1" "o de Software");
}

TEST_CASE("la especialidad sigue rechazando el separador y los valores malformados",
          entidades_especialidad_malformada) {
    Profesor profesor;
    // El separador del formato canonico sigue excluido por construccion.
    CHECK(lanzaConCampo([&] { profesor.setEspecialidad("con|pipe"); }, "especialidad"));
    CHECK(lanzaConCampo([&] { profesor.setEspecialidad("Redes | Comunicacion"); }, "especialidad"));
    // Vacio.
    CHECK(lanzaConCampo([&] { profesor.setEspecialidad(""); }, "especialidad"));
    // El limite es de 60 caracteres: una mas se rechaza.
    const std::string especialidadAlLimite = std::string(60, 'a');
    CHECK(Utils::cantidadCaracteres(especialidadAlLimite) == 60);
    CHECK(!lanzaConCampo([&] { profesor.setEspecialidad(especialidadAlLimite); }, "especialidad"));
    CHECK(lanzaConCampo([&] { profesor.setEspecialidad(especialidadAlLimite + "a"); }, "especialidad"));
    // Demasiado larga.
    const std::string especialidadLarga = std::string(61, 'a');
    CHECK(lanzaConCampo([&] { profesor.setEspecialidad(especialidadLarga); }, "especialidad"));
}

TEST_CASE("el setter de matricula rechaza un dato invalido", entidades_set_matricula) {
    Alumno alumno;
    CHECK(lanzaConCampo([&] { alumno.setMatricula(""); }, "matricula"));
    CHECK(lanzaConCampo([&] { alumno.setMatricula("ABC"); }, "matricula"));
    CHECK(lanzaConCampo([&] { alumno.setMatricula("MAT 20391"); }, "matricula"));
    CHECK(!lanzaConCampo([&] { alumno.setMatricula("MAT-20391"); }, "matricula"));
}

TEST_CASE("el mensaje de error dice que campo y que valor fallo", entidades_mensaje_error) {
    Profesor profesor;
    const ErrorCapturado error = capturarError([&] { profesor.setNombre("123"); });
    CHECK(error.lanzo);
    CHECK(error.campo == "nombre");
    CHECK(error.valor == "123");
    CHECK(error.mensaje.find("nombre") != std::string::npos);
    CHECK(error.mensaje.find("123") != std::string::npos);
}

TEST_CASE("operator== compara el id y el tipo", entidades_igualdad) {
    const Profesor profesor1 = profesorDePrueba();
    const Profesor profesor2 = profesorDePrueba();
    const Alumno alumno = alumnoDePrueba();
    CHECK(profesor1 == profesor2);
    CHECK(alumno.getId() == 7);

    // El id es unico POR ENTIDAD: un profesor y un alumno pueden tener el
    // mismo id y NO son la misma persona.
    const Profesor profesor5(5, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                             "Algoritmos");
    const Alumno alumno5(5, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900", "MAT-20391");
    CHECK(!(profesor5 == alumno5));
    CHECK(!(alumno5 == profesor5));
}

TEST_CASE("operator< ordena por id", entidades_orden) {
    const Profesor chico(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                         "Algoritmos");
    const Profesor grande(2, "Zeta", "Zulema", "zeta@mail.com", "2214556677", "LEG-9999", "Algebra");
    CHECK(chico < grande);
    CHECK(!(grande < chico));

    // Ordena por id, NO por el nombre: "Zeta" va antes que "Garcia" si el id
    // es menor.
    const Profesor zeta(1, "Zeta", "Zulema", "zeta@mail.com", "2214556677", "LEG-9999", "Algebra");
    CHECK(!(chico < zeta));
    CHECK(!(zeta < chico));

    // Con el mismo id, el TIPO desempata: el orden es total y determinista.
    // ("Alumno" va antes que "Profesor" alfabeticamente.)
    const Alumno alumnoMismoId(1, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900",
                               "MAT-20391");
    CHECK(alumnoMismoId < chico);
    CHECK(!(chico < alumnoMismoId));

    // Dos profesores con el mismo id son EQUIVALENTES: ninguno es menor que
    // el otro. Es lo que exige el "strict weak ordering" del arbol de la
    // Fase 3 (si `a` no es menor que `b` ni al reves, `a` es igual a `b`).
    const Profesor otroProfesor(1, "Otro", "Profesor", "otro@mail.com", "2214556677", "LEG-7777",
                                "Algebra");
    CHECK(!(chico < otroProfesor));
    CHECK(!(otroProfesor < chico));
}

TEST_CASE("Persona es abstracta y las derivadas agregan sus campos", entidades_persona_abstracta) {
    PersonaDePrueba persona;
    CHECK(persona.getTipo() == "PersonaDePrueba");
    CHECK(!persona.esValido());
    CHECK(persona.esActivo());

    // `esValido()` de la derivada mira tambien los campos propios.
    persona.setNombre("Ana");
    persona.setApellido("Garcia");
    persona.setEmail("ana.garcia@unlp.edu.ar");
    persona.setTelefono("2214556677");
    CHECK(persona.esValido());
}

TEST_CASE("los setters guardan el valor recortado", entidades_trim_en_setter) {
    Profesor profesor;
    profesor.setNombre("  Ana  ");
    CHECK(profesor.getNombre() == "Ana");
    profesor.setEmail("  ana.garcia@unlp.edu.ar ");
    CHECK(profesor.getEmail() == "ana.garcia@unlp.edu.ar");
}
TEST_CASE("el constructor tambien valida los datos", entidades_constructor_valida) {
    CHECK(lanzaConCampo([] { Profesor(1, "", "Ana", "ana@mail.com", "2214556677", "LEG-1042",
                                       "Algoritmos"); },
                        "nombre"));
    CHECK(lanzaConCampo([] { Alumno(7, "Lopez", "Martin", "sin arroba", "2214889900", "MAT-1"); },
                        "email"));
}

TEST_CASE("las entidades se copian y se mueven (Regla de Cinco)", entidades_regla_de_cinco) {
    const Profesor original = profesorDePrueba();

    const Profesor copia(original);
    CHECK(copia.getId() == original.getId());
    CHECK(copia.getNombre() == original.getNombre());
    CHECK(copia.getLegajo() == original.getLegajo());
    CHECK(copia.getEspecialidad() == original.getEspecialidad());

    Profesor movido(std::move(copia));
    CHECK(movido.getLegajo() == "LEG-1042");
    CHECK(movido.getEspecialidad() == "Algoritmos");

    // El alumno tambien.
    const Alumno alumno = alumnoDePrueba();
    const Alumno alumnoCopia(alumno);
    CHECK(alumnoCopia.getMatricula() == "MAT-20391");
}
