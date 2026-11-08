#pragma once

#include <cstddef>
#include <string>
#include <vector>

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

    // Indica si el texto solo tiene letras, digitos y los signos admitidos.
    //
    // Las letras son las del DOMINIO (ASCII mas el rango Latin-1 Supplement /
    // Latin Extended), las mismas que acepta `esAlfabetico`: por eso las
    // tildes y la enye se admiten igual que en los nombres. Los digitos SI
    // se admiten (a diferencia de `esAlfabetico`), porque hay campos del
    // dominio que son una mezcla de letras y digitos.
    //
    // `signosAdmitidos` es el conjunto de signos EXTRA que se permiten,
    // escrito como un texto con los caracteres ASCII admitidos (por ejemplo
    // ".@-" para un correo). La comparacion se hace contra el CODEPOINT
    // DECODIFICADO, asi que un caracter de mas de un byte nunca puede
    // confundirse con un signo ASCII: el separador '|' del formato canonico
    // queda excluido por construccion si no esta en el conjunto.
    //
    // El texto se recorre decodificando UTF-8: una secuencia malformada hace
    // que la funcion devuelva false. Un texto vacio tambien es invalido.
    static bool esAlfanumericoConSignos(const std::string& texto, const std::string& signosAdmitidos);

    // Cantidad de CARACTERES del texto (codepoints UTF-8), no de bytes.
    // Sirve para medir el largo de un valor que puede traer tildes: "Sala
    // Anio" con tilde son 8 caracteres, aunque sean 9 bytes.
    static std::size_t cantidadCaracteres(const std::string& texto);

    // Convierte a minuscula (ASCII y rango Latin-1 Supplement).
    static std::string aMinusculas(const std::string& texto);

    // Quita las TILDES: cada letra acentuada se reemplaza por su letra base
    // ("García" -> "Garcia", "Cardiología" -> "Cardiologia").
    //
    // PARA QUE SIRVE
    //
    // Para COMPARAR dos textos que se diferencian SOLO en las tildes. Los
    // datos de la clinica las tienen ("García", "Gómez", "Muñoz") y el
    // usuario escribe sin tilde casi siempre: escribir una tilde en una
    // terminal es incomodo. Sin esta funcion, buscar "garcia" no encuentra
    // a "García".
    //
    // EL RANGO QUE CONVIERTE
    //
    // El Latin-1 Supplement (U+00C0..U+00FF), que es el que cubre la
    // ortografia castellana (a con tilde, e con tilde, i con tilde, o con
    // tilde, u con tilde, enye y cedilla). Las letras del Latin Extended
    // ("cz", "sz", "lz") se dejan como estan: no aparecen en los datos.
    //
    // COMO SE USA
    //
    // Con `aMinusculas` da la forma de comparar sin tildes ni mayusculas:
    // `sinTildes(aMinusculas(texto))`.
    static std::string sinTildes(const std::string& texto);

    // Indica si el texto TIENE FORMA AAAA-MM-DD.
    // NO valida que la fecha exista en el calendario: "2026-04-31" tiene
    // formato valido pero no es una fecha existente (eso lo decide
    // `Fecha::esValida`).
    static bool esFormatoFecha(const std::string& texto);

    // Indica si el texto TIENE FORMA HH:MM.
    // NO valida el rango ni la granularidad: "08:99" tiene formato valido
    // (eso lo decide `Hora::esValida`).
    static bool esFormatoHora(const std::string& texto);

    // Parte el texto en campos usando el delimitador indicado y RECORTA
    // (trim) cada campo.
    //
    // POR QUE RECORTA: el archivo de la Fase 4 se escribe en modo texto, asi
    // que en Windows los finales de linea son "\r\n" y un "\r" pegado al
    // ultimo campo haria fallar el parseo de una fecha. Los campos del
    // formato canonico no tienen espacios significantes en los extremos.
    //
    // Un texto vacio devuelve UN campo vacio (no cero campos): la cantidad
    // de campos es la cantidad de delimitadores + 1.
    static std::vector<std::string> separar(const std::string& texto, char delimitador);
};
