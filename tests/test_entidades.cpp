#include "test_framework.h"

#include "domain/Especialidad.h"
#include "entities/Paciente.h"
#include "entities/Persona.h"
#include "entities/Medico.h"
#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <string>
#include <type_traits>
#include <utility>

// ---------------------------------------------------------------------------
// Tests de `Persona`, `Medico` y `Paciente`: constructores, setters con
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

Medico medicoDePrueba() {
    return Medico(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                    Especialidad::CARD);
}

Paciente pacienteDePrueba() {
    return Paciente(7, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900", "20391001");
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
static_assert(std::is_copy_constructible<Medico>::value, "Medico debe ser copiable");
static_assert(std::is_move_constructible<Medico>::value, "Medico debe ser movible");
static_assert(std::is_copy_assignable<Medico>::value, "Medico debe ser asignable por copia");
static_assert(std::is_copy_constructible<Paciente>::value, "Paciente debe ser copiable");
static_assert(std::is_move_constructible<Paciente>::value, "Paciente debe ser movible");
static_assert(std::is_destructible<Medico>::value, "Medico debe ser destructible");
static_assert(std::is_destructible<Paciente>::value, "Paciente debe ser destructible");

TEST_CASE("el medico por defecto esta sin asignar", entidades_medico_defecto) {
    const Medico medico;
    CHECK(medico.getId() == -1);
    CHECK(medico.getNombre().empty());
    CHECK(medico.getApellido().empty());
    CHECK(medico.getEmail().empty());
    CHECK(medico.getTelefono().empty());
    CHECK(medico.getLegajo().empty());
    // La especialidad es un valor del catalogo: la del medico por defecto es
    // la primera, no una cadena vacia.
    CHECK(medico.getEspecialidad() == Especialidad::CARD);
    CHECK(medico.esActivo());
    CHECK(!medico.esValido());
    CHECK(Medico::esquema().size() > 0);
}

TEST_CASE("el paciente por defecto esta sin asignar", entidades_paciente_defecto) {
    const Paciente paciente;
    CHECK(paciente.getId() == -1);
    CHECK(paciente.getNombre().empty());
    CHECK(paciente.getDni().empty());
    CHECK(paciente.esActivo());
    CHECK(!paciente.esValido());
}

TEST_CASE("el constructor del medico guarda todos los datos", entidades_medico_constructor) {
    const Medico medico = medicoDePrueba();
    CHECK(medico.getId() == 1);
    CHECK(medico.getNombre() == "Garcia");
    CHECK(medico.getApellido() == "Ana");
    CHECK(medico.getEmail() == "ana.garcia@unlp.edu.ar");
    CHECK(medico.getTelefono() == "2214556677");
    CHECK(medico.getLegajo() == "LEG-1042");
    CHECK(medico.getEspecialidad() == Especialidad::CARD);
    CHECK(medico.esActivo());
    CHECK(medico.esValido());
    CHECK(medico.getTipo() == "Medico");
}

TEST_CASE("el constructor del paciente guarda todos los datos", entidades_paciente_constructor) {
    const Paciente paciente = pacienteDePrueba();
    CHECK(paciente.getId() == 7);
    CHECK(paciente.getNombre() == "Lopez");
    CHECK(paciente.getApellido() == "Martin");
    CHECK(paciente.getEmail() == "martin.lopez@unlp.edu.ar");
    CHECK(paciente.getTelefono() == "2214889900");
    CHECK(paciente.getDni() == "20391001");
    CHECK(paciente.esActivo());
    CHECK(paciente.esValido());
    CHECK(paciente.getTipo() == "Paciente");
}

TEST_CASE("el setter de nombre rechaza un dato invalido", entidades_set_nombre) {
    Medico medico;
    CHECK(lanzaConCampo([&] { medico.setNombre("123"); }, "nombre"));
    CHECK(lanzaConCampo([&] { medico.setNombre(""); }, "nombre"));
    CHECK(lanzaConCampo([&] { medico.setNombre("   "); }, "nombre"));
    CHECK(lanzaConCampo([&] { medico.setNombre("Ana3"); }, "nombre"));
    // Con tilde y enye SI es valido: `Utils::esAlfabetico` es consciente de UTF-8.
    CHECK(!lanzaConCampo([&] { medico.setNombre("José"); }, "nombre"));
    CHECK(!lanzaConCampo([&] { medico.setNombre("Maria de los Angeles"); }, "nombre"));
}

TEST_CASE("el setter de apellido rechaza un dato invalido", entidades_set_apellido) {
    Medico medico;
    CHECK(lanzaConCampo([&] { medico.setApellido(""); }, "apellido"));
    CHECK(lanzaConCampo([&] { medico.setApellido("Gomez1"); }, "apellido"));
}

TEST_CASE("el setter de email rechaza un dato invalido", entidades_set_email) {
    Medico medico;
    CHECK(lanzaConCampo([&] { medico.setEmail(""); }, "email"));
    CHECK(lanzaConCampo([&] { medico.setEmail("a@b"); }, "email"));         // muy corto
    CHECK(lanzaConCampo([&] { medico.setEmail("con espacio@mail.com"); }, "email"));
    // Sin arroba y SIN espacios: si el valor tuviera un espacio, la prueba
    // pasaria por el espacio y no estaria probando la regla de la arroba.
    CHECK(lanzaConCampo([&] { medico.setEmail("sinarroba.com"); }, "email"));
    // El separador del formato canonico no puede pasar: rompe el archivo.
    CHECK(lanzaConCampo([&] { medico.setEmail("ana|garcia@mail.com"); }, "email"));
    CHECK(!lanzaConCampo([&] { medico.setEmail("ana.garcia-1_x@unlp.edu.ar"); }, "email"));
}

TEST_CASE("el email acepta letras acentuadas y las guarda intactas", entidades_email_acentuado) {
    Medico medico;
    // 'í' (U+00ED) son 0xC3 0xAD. El escape hexadecimal va en un literal
    // propio porque se extiende hasta el proximo caracter hexadecimal.
    const std::string emailAcentuado = "ana.garc" "\xC3\xAD" "a@unlp.edu.ar";
    CHECK(!lanzaConCampo([&] { medico.setEmail(emailAcentuado); }, "email"));
    // El valor vuelve IDENTICO: la tilde no se convierte ni se pierde.
    CHECK(medico.getEmail() == emailAcentuado);
    CHECK(medico.getEmail() == "ana.garc" "\xC3\xAD" "a@unlp.edu.ar");
    // Tambien con enye y con e acute.
    CHECK(!lanzaConCampo([&] { medico.setEmail("mu" "\xC3\xB1" "oz@unlp.edu.ar"); }, "email"));
    CHECK(medico.getEmail() == "mu" "\xC3\xB1" "oz@unlp.edu.ar");
    CHECK(!lanzaConCampo([&] { medico.setEmail("jose" "\xC3\xA9" "@unlp.edu.ar"); }, "email"));
    CHECK(medico.getEmail() == "jose" "\xC3\xA9" "@unlp.edu.ar");
}

TEST_CASE("el email sigue rechazando el separador y los valores malformados", entidades_email_malformado) {
    Medico medico;
    // El separador del formato canonico sigue excluido por construccion.
    CHECK(lanzaConCampo([&] { medico.setEmail("a|b@x.com"); }, "email"));
    CHECK(lanzaConCampo([&] { medico.setEmail("ana.garcia@unlp.edu.ar|x"); }, "email"));
    // Un espacio interno no se admite: el correo no lo lleva.
    CHECK(lanzaConCampo([&] { medico.setEmail("ana garcia@unlp.edu.ar"); }, "email"));
    // Vacio y demasiado corto.
    CHECK(lanzaConCampo([&] { medico.setEmail(""); }, "email"));
    CHECK(lanzaConCampo([&] { medico.setEmail("a@b"); }, "email"));
    // El limite es de 60 caracteres: uno mas se rechaza.
    const std::string correoAlLimite = std::string(54, 'a') + "@x.com";  // 60 caracteres
    CHECK(Utils::cantidadCaracteres(correoAlLimite) == 60);
    CHECK(!lanzaConCampo([&] { medico.setEmail(correoAlLimite); }, "email"));
    CHECK(lanzaConCampo([&] { medico.setEmail(correoAlLimite + "a"); }, "email"));  // 61
    // Demasiado largo.
    const std::string correoLargo = std::string(61, 'a') + "@x.com";
    CHECK(lanzaConCampo([&] { medico.setEmail(correoLargo); }, "email"));
    // Una arroba no alcanza: sin ella no es un correo.
    CHECK(lanzaConCampo([&] { medico.setEmail("sinarroba.com"); }, "email"));
}

TEST_CASE("el setter de telefono rechaza un dato invalido", entidades_set_telefono) {
    Medico medico;
    CHECK(lanzaConCampo([&] { medico.setTelefono(""); }, "telefono"));
    CHECK(lanzaConCampo([&] { medico.setTelefono("123456"); }, "telefono"));  // muy corto
    CHECK(lanzaConCampo([&] { medico.setTelefono("2214556677 letras"); }, "telefono"));
    CHECK(!lanzaConCampo([&] { medico.setTelefono("+54 221 455-6677"); }, "telefono"));
}

TEST_CASE("el setter de legajo rechaza un dato invalido", entidades_set_legajo) {
    Medico medico;
    CHECK(lanzaConCampo([&] { medico.setLegajo(""); }, "legajo"));
    CHECK(lanzaConCampo([&] { medico.setLegajo("ABC"); }, "legajo"));       // muy corto
    CHECK(lanzaConCampo([&] { medico.setLegajo("1234567890123"); }, "legajo"));  // muy largo
    CHECK(lanzaConCampo([&] { medico.setLegajo("LEG 1042"); }, "legajo"));
    CHECK(!lanzaConCampo([&] { medico.setLegajo("LEG-1042"); }, "legajo"));
}

TEST_CASE("el setter de especialidad acepta cualquier valor del catalogo",
          entidades_set_especialidad) {
    Medico medico;
    // LA ESPECIALIDAD ES UN `enum class`: NO HAY NADA QUE VALIDAR.
    //
    // Todo valor del catalogo es valido por construccion, asi que el setter
    // no lanza NUNCA. Lo que puede fallar es el CODIGO que llega del
    // archivo (que se traduce en `fromRegistro`), no el valor del enum.
    CHECK(!lanzaConCampo([&] { medico.setEspecialidad(Especialidad::CARD); }, "especialidad"));
    CHECK(medico.getEspecialidad() == Especialidad::CARD);
    CHECK(!lanzaConCampo([&] { medico.setEspecialidad(Especialidad::NEUM); }, "especialidad"));
    CHECK(medico.getEspecialidad() == Especialidad::NEUM);
    medico.setEspecialidad(Especialidad::DERM);
    CHECK(medico.getEspecialidad() == Especialidad::DERM);
}

TEST_CASE("la especialidad del medico se muestra con su descripcion del catalogo",
          entidades_especialidad_acentuada) {
    Medico medico;
    // 'ó' (U+00F3) son 0xC3 0xB3. La descripcion del catalogo esta en UTF-8
    // y la tilde se conserva intacta.
    medico.setEspecialidad(Especialidad::OFTA);
    CHECK(descripcionDe(medico.getEspecialidad()) == "Oftalmología");
    CHECK(descripcionDe(medico.getEspecialidad()) != "Oftalmologia");
    // Y el codigo es el que viaja al archivo.
    CHECK(codigoDe(medico.getEspecialidad()) == "OFTA");
}

TEST_CASE("la especialidad del medico rechaza los codigos que no estan en el catalogo",
          entidades_especialidad_malformada) {
    // El `enum class` no puede guardar un texto: lo que se rechaza es el
    // CODIGO, y lo rechaza el catalogo (`especialidadDesdeCodigo`).
    CHECK(lanzaConCampo([] { especialidadDesdeCodigo("con|pipe"); }, "especialidad"));
    CHECK(lanzaConCampo([] { especialidadDesdeCodigo(""); }, "especialidad"));
    CHECK(lanzaConCampo([] { especialidadDesdeCodigo("XXX"); }, "especialidad"));
    // Y el registro completo se rechaza tambien, con el mismo campo.
    CHECK(lanzaConCampo(
        [] {
            Medico::fromRegistro("1|Garcia|Ana|ana@mail.com|2214556677|1|LEG-1042|XXX");
        },
        "especialidad"));
}

TEST_CASE("el setter de dni rechaza un dato invalido", entidades_set_dni) {
    Paciente paciente;
    // EL DNI SON 7 U 8 DIGITOS, TODOS NUMERICOS
    CHECK(lanzaConCampo([&] { paciente.setDni(""); }, "dni"));
    CHECK(lanzaConCampo([&] { paciente.setDni("123456"); }, "dni"));      // 6 digitos
    CHECK(lanzaConCampo([&] { paciente.setDni("123456789"); }, "dni"));   // 9 digitos
    CHECK(lanzaConCampo([&] { paciente.setDni("ABC1234"); }, "dni"));    // con letras
    CHECK(lanzaConCampo([&] { paciente.setDni("2039100a"); }, "dni"));    // con letras
    CHECK(lanzaConCampo([&] { paciente.setDni("2039-100"); }, "dni"));   // con un signo
    CHECK(lanzaConCampo([&] { paciente.setDni("20|91001"); }, "dni"));    // con el separador
    // Y los dos casos validos: 7 y 8 digitos.
    CHECK(!lanzaConCampo([&] { paciente.setDni("2039100"); }, "dni"));
    CHECK(!lanzaConCampo([&] { paciente.setDni("20391001"); }, "dni"));
}

TEST_CASE("el dni acepta 7 y 8 digitos y rechaza todo lo demas", entidades_dni_regla) {
    Paciente paciente;

    // EL DNI VALIDO: 7 u 8 digitos, todos numericos.
    CHECK(!lanzaConCampo([&] { paciente.setDni("2039100"); }, "dni"));
    CHECK(!lanzaConCampo([&] { paciente.setDni("20391001"); }, "dni"));
    CHECK(paciente.getDni() == "20391001");

    // EL DNI INVALIDO, uno por uno.
    const std::string dnisInvalidos[] = {"", "123456", "123456789", "ABC1234", "2039 100",
                                         "-2039100", "2039100.5"};
    for (const std::string& invalido : dnisInvalidos) {
        const ErrorCapturado error = capturarError([&] { paciente.setDni(invalido); });
        CHECK(error.lanzo);
        CHECK(!error.lanzoOtraExcepcion);
        CHECK(error.campo == "dni");
    }
}

TEST_CASE("el dni con espacios alrededor se acepta porque el setter lo recorta",
          entidades_dni_con_espacios) {
    Paciente paciente;
    paciente.setDni("  20391001  ");
    CHECK(paciente.getDni() == "20391001");
    // Y el valor recortado es el que se guarda, no el original.
    CHECK(paciente.getDni().size() == 8);
}

TEST_CASE("el mensaje de error dice que campo y que valor fallo", entidades_mensaje_error) {
    Medico medico;
    const ErrorCapturado error = capturarError([&] { medico.setNombre("123"); });
    CHECK(error.lanzo);
    CHECK(error.campo == "nombre");
    CHECK(error.valor == "123");
    CHECK(error.mensaje.find("nombre") != std::string::npos);
    CHECK(error.mensaje.find("123") != std::string::npos);
}

TEST_CASE("operator== compara el id y el tipo", entidades_igualdad) {
    const Medico medico1 = medicoDePrueba();
    const Medico medico2 = medicoDePrueba();
    const Paciente paciente = pacienteDePrueba();
    CHECK(medico1 == medico2);
    CHECK(paciente.getId() == 7);

    // El id es unico POR ENTIDAD: un medico y un paciente pueden tener el
    // mismo id y NO son la misma persona.
    const Medico medico5(5, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                          Especialidad::CARD);
    const Paciente paciente5(5, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900",
                              "20391001");
    CHECK(!(medico5 == paciente5));
    CHECK(!(paciente5 == medico5));
}

TEST_CASE("operator< ordena por id", entidades_orden) {
    const Medico chico(1, "Garcia", "Ana", "ana.garcia@unlp.edu.ar", "2214556677", "LEG-1042",
                        Especialidad::CARD);
    const Medico grande(2, "Zeta", "Zulema", "zeta@mail.com", "2214556677", "LEG-9999",
                         Especialidad::GAST);
    CHECK(chico < grande);
    CHECK(!(grande < chico));

    // Ordena por id, NO por el nombre: "Zeta" va antes que "Garcia" si el id
    // es menor.
    const Medico zeta(1, "Zeta", "Zulema", "zeta@mail.com", "2214556677", "LEG-9999",
                      Especialidad::GAST);
    CHECK(!(chico < zeta));
    CHECK(!(zeta < chico));

    // Con el mismo id, el TIPO desempata: el orden es total y determinista.
    // ("Medico" va antes que "Paciente" alfabeticamente.)
    const Paciente pacienteMismoId(1, "Lopez", "Martin", "martin.lopez@unlp.edu.ar", "2214889900",
                                    "20391001");
    CHECK(chico < pacienteMismoId);
    CHECK(!(pacienteMismoId < chico));

    // Dos medicos con el mismo id son EQUIVALENTES: ninguno es menor que
    // el otro. Es lo que exige el "strict weak ordering" del arbol de la
    // Fase 3 (si `a` no es menor que `b` ni al reves, `a` es igual a `b`).
    const Medico otroMedico(1, "Otro", "Medico", "otro@mail.com", "2214556677", "LEG-7777",
                            Especialidad::GAST);
    CHECK(!(chico < otroMedico));
    CHECK(!(otroMedico < chico));
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
    Medico medico;
    medico.setNombre("  Ana  ");
    CHECK(medico.getNombre() == "Ana");
    medico.setEmail("  ana.garcia@unlp.edu.ar ");
    CHECK(medico.getEmail() == "ana.garcia@unlp.edu.ar");

    // EL DNI TAMBIEN SE RECORTA
    Paciente paciente;
    paciente.setDni("  20391001  ");
    CHECK(paciente.getDni() == "20391001");
}

TEST_CASE("el constructor tambien valida los datos", entidades_constructor_valida) {
    CHECK(lanzaConCampo([] { Medico(1, "", "Ana", "ana@mail.com", "2214556677", "LEG-1042",
                                     Especialidad::CARD); },
                         "nombre"));
    CHECK(lanzaConCampo([] { Paciente(7, "Lopez", "Martin", "sin arroba", "2214889900", "20000001"); },
                         "email"));
    // Y el dni invalido se rechaza en el constructor tambien.
    CHECK(lanzaConCampo([] { Paciente(7, "Lopez", "Martin", "lopez@mail.com", "2214889900", "123"); },
                         "dni"));
}

TEST_CASE("las entidades se copian y se mueven (Regla de Cinco)", entidades_regla_de_cinco) {
    const Medico original = medicoDePrueba();

    const Medico copia(original);
    CHECK(copia.getId() == original.getId());
    CHECK(copia.getNombre() == original.getNombre());
    CHECK(copia.getLegajo() == original.getLegajo());
    CHECK(copia.getEspecialidad() == original.getEspecialidad());

    Medico movido(std::move(copia));
    CHECK(movido.getLegajo() == "LEG-1042");
    CHECK(movido.getEspecialidad() == Especialidad::CARD);

    // El paciente tambien.
    const Paciente paciente = pacienteDePrueba();
    const Paciente pacienteCopia(paciente);
    CHECK(pacienteCopia.getDni() == "20391001");
}
