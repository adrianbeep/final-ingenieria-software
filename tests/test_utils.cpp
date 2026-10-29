#include "test_framework.h"

#include "utils/Utils.h"

#include <string>

// ---------------------------------------------------------------------------
// Tests de las utilidades de texto.
//
// Los casos con caracteres NO ASCII se escriben con escapes hexadecimales
// explicitos (y no con el caracter literal) para que el test sea
// independiente de la codificacion con la que se lea el archivo fuente:
//   - 'É' (U+00C9) -> 0xC3 0x89      - 'é' (U+00E9) -> 0xC3 0xA9
//   - emoji (U+1F600) -> 0xF0 0x9F 0x98 0x80
//   - ideograma japones (U+65E5) -> 0xE6 0x97 0xA5
// Cada escape va en un literal propio porque un escape hexadecimal se
// extende hasta el proximo caracter hexadecimal de la cadena.
// ---------------------------------------------------------------------------

TEST_CASE("trim recorta los espacios de los extremos", utils_trim) {
    CHECK(Utils::trim("  hola  ") == "hola");
    CHECK(Utils::trim("hola") == "hola");
    CHECK(Utils::trim("   ") == "");
    CHECK(Utils::trim("a b") == "a b");
    CHECK(Utils::trim("\t hola \n") == "hola");
    CHECK(Utils::trim("") == "");
    CHECK(Utils::trim("\r\n hola \r\n") == "hola");
}

TEST_CASE("esEntero acepta solo digitos con signo opcional", utils_es_entero) {
    CHECK(Utils::esEntero("42"));
    CHECK(Utils::esEntero("-5"));
    CHECK(Utils::esEntero("+7"));
    CHECK(Utils::esEntero("0"));

    CHECK(!Utils::esEntero(""));
    CHECK(!Utils::esEntero("-"));
    CHECK(!Utils::esEntero("+"));
    CHECK(!Utils::esEntero("3.5"));
    CHECK(!Utils::esEntero("12a"));
    CHECK(!Utils::esEntero("a12"));
    // No tolera espacios: hay que aplicar `trim` antes.
    CHECK(!Utils::esEntero(" 42 "));
    CHECK(!Utils::esEntero("42 "));
    CHECK(!Utils::esEntero(" 42"));
}

TEST_CASE("esNoVacio ignora los espacios de los extremos", utils_es_no_vacio) {
    CHECK(!Utils::esNoVacio("   "));
    CHECK(!Utils::esNoVacio(""));
    CHECK(Utils::esNoVacio("a"));
    CHECK(Utils::esNoVacio(" a "));
    // Las tabulaciones y los saltos de linea tambien son espacios para
    // `trim`, asi que un texto solo con ellos esta vacio.
    CHECK(!Utils::esNoVacio("\t\n"));
}

TEST_CASE("esAlfabetico acepta nombres con letras, espacios, apostrofos y guiones", utils_alfabetico_ok) {
    CHECK(Utils::esAlfabetico("abc"));
    CHECK(Utils::esAlfabetico("Jose"));
    CHECK(Utils::esAlfabetico("Jose Perez"));
    CHECK(Utils::esAlfabetico("O'Brien"));
    CHECK(Utils::esAlfabetico("Garcia-Jones"));
    CHECK(Utils::esAlfabetico("Munoz"));
    CHECK(Utils::esAlfabetico("Z"));
    CHECK(Utils::esAlfabetico("Nino"));
    // Con tilde y con enye (UTF-8 de 2 bytes).
    CHECK(Utils::esAlfabetico("Jos\xC3\xA9"));
    CHECK(Utils::esAlfabetico("Mu\xC3\xB1oz"));
    CHECK(Utils::esAlfabetico("A\xC3\x91"));
}

TEST_CASE("esAlfabetico rechaza lo que no es un nombre", utils_alfabetico_falla) {
    CHECK(!Utils::esAlfabetico("123"));
    CHECK(!Utils::esAlfabetico("a1"));
    CHECK(!Utils::esAlfabetico(""));
    CHECK(!Utils::esAlfabetico("   "));
    CHECK(!Utils::esAlfabetico("---"));
    CHECK(!Utils::esAlfabetico("Jose2"));
    // Emoji (U+1F600): es UTF-8 valido, pero no es una letra.
    CHECK(!Utils::esAlfabetico("\xF0\x9F\x98\x80"));
    // Ideograma japones (U+65E5): tampoco es una letra.
    CHECK(!Utils::esAlfabetico("\xE6\x97\xA5"));
    // UTF-8 malformado: la cadena no es valida.
    CHECK(!Utils::esAlfabetico("\xC3"));
    // Signos de multiplicacion (U+00D7) y division (U+00F7): viven dentro del
    // rango Latin-1 pero NO son letras.
    CHECK(!Utils::esAlfabetico("3 \xC3\x97 4"));
    CHECK(!Utils::esAlfabetico("3 \xC3\xB7 4"));
}

TEST_CASE("aMinusculas pasa a minuscula el ASCII y el rango Latin-1", utils_a_minusculas) {
    CHECK(Utils::aMinusculas("HOLA") == "hola");
    CHECK(Utils::aMinusculas("MUNOZ") == "munoz");
    CHECK(Utils::aMinusculas("ANDRES") == "andres");
    CHECK(Utils::aMinusculas("Jose Perez") == "jose perez");
    CHECK(Utils::aMinusculas("Ya Cambie") == "ya cambie");
    CHECK(Utils::aMinusculas("") == "");
    // 'É' (0xC3 0x89) -> 'é' (0xC3 0xA9): hay que reescribir los 2 bytes.
    CHECK(Utils::aMinusculas("\xC3\x89") == "\xC3\xA9");
    CHECK(Utils::aMinusculas("\xC3\x89" "XITO") == "\xC3\xA9" "xito");
    // Lo que ya esta en minuscula no cambia.
    CHECK(Utils::aMinusculas("hola") == "hola");
    CHECK(Utils::aMinusculas("\xC3\xA9") == "\xC3\xA9");
}

TEST_CASE("esFormatoFecha acepta solo el formato AAAA-MM-DD", utils_formato_fecha) {
    CHECK(Utils::esFormatoFecha("2026-03-15"));
    CHECK(!Utils::esFormatoFecha("2026-3-15"));
    CHECK(!Utils::esFormatoFecha("2026/03/15"));
    CHECK(!Utils::esFormatoFecha("20260315"));
    CHECK(!Utils::esFormatoFecha("aaaa-mm-dd"));
    CHECK(!Utils::esFormatoFecha("2026-03-15 "));
    CHECK(!Utils::esFormatoFecha("2026-03-15\n"));
    CHECK(!Utils::esFormatoFecha(""));
    // "2026-04-31" TIENE formato valido (aunque no sea una fecha existente):
    // la existencia en el calendario la decide `Fecha::esValida`.
    CHECK(Utils::esFormatoFecha("2026-04-31"));
}

TEST_CASE("esFormatoHora acepta solo el formato HH:MM", utils_formato_hora) {
    CHECK(Utils::esFormatoHora("09:30"));
    CHECK(!Utils::esFormatoHora("9:30"));
    CHECK(!Utils::esFormatoHora("09-30"));
    CHECK(!Utils::esFormatoHora("09:3"));
    CHECK(!Utils::esFormatoHora("09:300"));
    CHECK(!Utils::esFormatoHora(""));
    CHECK(!Utils::esFormatoHora("aa:bb"));
    // "08:99" TIENE formato valido (aunque el minuto no exista):
    // el rango lo decide `Hora::esValida`.
    CHECK(Utils::esFormatoHora("08:99"));
}
