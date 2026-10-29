#pragma once

#include <string>

// ---------------------------------------------------------------------------
// Utilidades de validacion y manipulacion de texto.
//
// TODOS los metodos son `static` y NINGUNO lanza excepciones: son funciones
// de "pregunta" ("¿esto es valido?"). Responden si o no, siempre. Quien
// necesite fallar ante un dato invalido arma la excepcioncorrespondinge.
//
// REGLA DE ORO: estas funciones NO son la validacion de negocio. Por ejemplo,
// `esFormatoFecha` dice si el texto TIENE forma AAAA-MM-DD; si esa fecha
// existe en el calendario es otra pregunta, y la responde `Fecha::esValida`.
// ---------------------------------------------------------------------------
class Utils {
public:
    // La clase es utilitaria: no tiene estado, no se instancia.
    Utils() = delete;

    // Recorta los espacios (de `std::isspace`: espacio, \t, \n, \r, \v, \f)
    // del principio y del final del texto.
    static std::string trim(const std::string& texto);

    // Indica si el texto es un entero ESTRICTO: signo opcional ('+' o '-')
    // seguido de uno o mas digitos, y nada mas.
    // NO tolera espacios: " 42 " es FALSO. Si se necesitan tolerar, hay que
    // aplicar `trim` antes de llamar a esta funcion.
    static bool esEntero(const std::string& texto);

    // Indica si el texto tiene contenido util (hace `trim` internamente).
    static bool esNoVacio(const std::string& texto);

    // Indica si el texto es un nombre alfabetico, con soporte UTF-8.
    // Acepta letras ASCII, espacio, apostrofo y guion, y las letras latinas
    // acentuadas/enye del rango U+00C0..U+024F. Exige al menos una letra.
    static bool esAlfabetico(const std::string& texto);

    // Convierte a minuscula (ASCII y rango Latin-1 Supplement).
    static std::string aMinusculas(const std::string& texto);

    // Indica si el texto TIENE FORMA AAAA-MM-DD.
    // NO valida que la fecha exista en el calendario: "2026-04-31" tiene
    // formato valido pero no es una fecha existente (eso lo decide
    // `Fecha::esValida`).
    static bool esFormatoFecha(const std::string& texto);

    // Indica si el texto TIENE FORMA HH:MM.
    // NO valida el rango ni la granularidad: "08:99" tiene formato valido
    // (eso lo decide `Hora::esValida`).
    static bool esFormatoHora(const std::string& texto);
};
