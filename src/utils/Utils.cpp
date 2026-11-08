#include "utils/Utils.h"

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// Utilidades internas de UTF-8.
//
// POR QUE UTF-8 PROPIO Y NO `std::isalpha` CON EL LOCALE POR DEFECTO:
// `std::isalpha` (usando el locale "C") solo conoce el juego de caracteres
// ASCII, de modo que los bytes UTF-8 de una tilde o de una enye NO los
// clasifica como letras y un nombre como "Jose Perez" seria rechazado. Cambiar
// el locale global para validar texto tiene efectos secundarios (afecta a todo
// el proceso y no es thread-safe). Decodificar el codepoint y compararlo
// contra rangos Unicode es exacto y predecible.
// ---------------------------------------------------------------------------

constexpr std::uint32_t LETRA_ASCII_MAYUSCULA_INICIO = 'A';
constexpr std::uint32_t LETRA_ASCII_MAYUSCULA_FIN = 'Z';
constexpr std::uint32_t LETRA_ASCII_MINUSCULA_INICIO = 'a';
constexpr std::uint32_t LETRA_ASCII_MINUSCULA_FIN = 'z';

// Latin-1 Supplement (A con tilde, enye, u con diaeresis, ...) y
// Latin Extended-A/B. Es el rango que cubre la ortografia castellana.
constexpr std::uint32_t LATIN1_INICIO = 0x00C0;
constexpr std::uint32_t LATIN1_FIN = 0x024F;

// Dentro del rango Latin-1 Supplement hay dos simbolos que NO son letras: el
// signo de multiplicacion (U+00D7) y el de division (U+00F7).
constexpr std::uint32_t MULTIPLICACION_LATIN1 = 0x00D7u;
constexpr std::uint32_t DIVISION_LATIN1 = 0x00F7u;

// Tabla de correspondencia a minuscula del rango Latin-1 Supplement
// (U+00C0..U+00DE). El indice es (codepoint - 0x00C0).
// La posicion 23 corresponde a U+00D7 (signo de multiplicacion) y vale 0
// porque NO tiene minuscula: mapearla daria el signo de division (U+00F7).
// La tabla tiene 31 entradas: U+00C0..U+00DE.
constexpr std::uint32_t TABLA_MINUSCULA_LATIN1[] = {
    0x00E0, 0x00E1, 0x00E2, 0x00E3, 0x00E4, 0x00E5, 0x00E6, 0x00E7,  // A con...
    0x00E8, 0x00E9, 0x00EA, 0x00EB, 0x00EC, 0x00ED, 0x00EE, 0x00EF,  // E con...
    0x00F0, 0x00F1, 0x00F2, 0x00F3, 0x00F4, 0x00F5, 0x00F6,         // I con...
    0x0000,                                                       // x (no es letra)
    0x00F8, 0x00F9, 0x00FA, 0x00FB, 0x00FC, 0x00FD, 0x00FE          // O con... y U+00DE
};
constexpr std::uint32_t LATIN1_MAYUSCULA_INICIO = 0x00C0;
constexpr std::uint32_t LATIN1_MAYUSCULA_FIN = 0x00DE;

// Tabla de la LETRA BASE de cada letra acentuada del rango Latin-1
// Supplement (U+00C0..U+00FF). El indice es (codepoint - 0x00C0).
//
// EL VALOR 0 SIGNIFICA "SIN BASE"
//
// Son los dos simbolos que viven dentro del rango y no son letras (el signo
// de multiplicacion y el de division): se copian tal cual.
//
// LA MITAD DE ARRIBA ES MAYUSCULA Y LA DE ABAJO MINUSCULA
//
// La funcion NO cambia la caja de la letra: solo le saca el signo
// ("Á" -> "A", "á" -> "a"). El paso a minuscula es de `aMinusculas`.
constexpr char TABLA_SIN_TILDES[] = {
    'A', 'A', 'A', 'A', 'A', 'A', 'A', 'C',  // A con... y C con cedilla
    'E', 'E', 'E', 'E', 'I', 'I', 'I', 'I',  // E con... e I con...
    'D', 'N', 'O', 'O', 'O', 'O', 'O', '\0', // D con... y el signo de multiplicacion
    'O', 'U', 'U', 'U', 'U', 'Y', 'P', 'S',  // O con... y U con...
    'a', 'a', 'a', 'a', 'a', 'a', 'a', 'c',  // a con... y c con cedilla
    'e', 'e', 'e', 'e', 'i', 'i', 'i', 'i',  // e con... e i con...
    'd', 'n', 'o', 'o', 'o', 'o', 'o', '\0', // d con... y el signo de division
    'o', 'u', 'u', 'u', 'u', 'y', 'p', 'y'   // o con... e u con...
};
constexpr std::uint32_t LATIN1_TILDES_INICIO = 0x00C0;
constexpr std::uint32_t LATIN1_TILDES_FIN = 0x00FF;

// Decodifica el codepoint UTF-8 que arranca en la posicion `inicio`.
// Devuelve true si la secuencia es valida, escribiendo el codepoint y la
// cantidad de bytes que ocupa. Devuelve false ante cualquier secuencia
// malformada (byte de continuacion suelto, secuencia truncada, sobregijo
// (overlong) o codepoint fuera del rango Unicode).
bool decodificarUtf8(const std::string& texto, std::size_t inicio,
                     std::uint32_t& codepoint, std::size_t& largo) {
    const std::size_t total = texto.size();
    if (inicio >= total) {
        return false;
    }

    const unsigned char primer = static_cast<unsigned char>(texto[inicio]);

    if (primer <= 0x7Fu) {  // ASCII
        codepoint = primer;
        largo = 1;
        return true;
    }

    std::size_t cantidadBytes = 0;
    std::uint32_t valor = 0;
    unsigned char minimoSegundo = 0x80u;
    unsigned char maximoSegundo = 0xBFu;

    if (primer >= 0xC2u && primer <= 0xDFu) {  // 2 bytes
        // 0xC0 y 0xC1 quedan fuera: siempre serian sobregijos.
        cantidadBytes = 2;
        valor = primer & 0x1Fu;
    } else if (primer >= 0xE0u && primer <= 0xEFu) {  // 3 bytes
        cantidadBytes = 3;
        valor = primer & 0x0Fu;
        if (primer == 0xE0u) {
            minimoSegundo = 0xA0u;  // evita el sobregijo de 3 bytes
        }
        if (primer == 0xEDu) {
            maximoSegundo = 0x9Fu;  // evita los surrogados U+D800..U+DFFF
        }
    } else if (primer >= 0xF0u && primer <= 0xF4u) {  // 4 bytes
        cantidadBytes = 4;
        valor = primer & 0x07u;
        if (primer == 0xF0u) {
            minimoSegundo = 0x90u;  // evita el sobregijo de 4 bytes
        }
        if (primer == 0xF4u) {
            maximoSegundo = 0x8Fu;  // evita pasar de U+10FFFF
        }
    } else {
        // Byte de continuacion suelto (0x80..0xBF) o lead byte invalido
        // (0xC0, 0xC1, 0xF5..0xFF).
        return false;
    }

    if (inicio + cantidadBytes > total) {
        return false;  // secuencia truncada
    }

    for (std::size_t i = 1; i < cantidadBytes; ++i) {
        const unsigned char byte = static_cast<unsigned char>(texto[inicio + i]);
        const unsigned char minimo = (i == 1) ? minimoSegundo : 0x80u;
        const unsigned char maximo = (i == 1) ? maximoSegundo : 0xBFu;
        if (byte < minimo || byte > maximo) {
            return false;  // byte de continuacion fuera de rango
        }
        valor = (valor << 6) | (byte & 0x3Fu);
    }

    codepoint = valor;
    largo = cantidadBytes;
    return true;
}

// Codepoint -> bytes UTF-8.
std::string codepointAUtf8(std::uint32_t codepoint) {
    std::string salida;
    if (codepoint <= 0x7Fu) {
        salida.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7FFu) {
        salida.push_back(static_cast<char>(0xC0u | (codepoint >> 6)));
        salida.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else if (codepoint <= 0xFFFFu) {
        salida.push_back(static_cast<char>(0xE0u | (codepoint >> 12)));
        salida.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
        salida.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    } else {
        salida.push_back(static_cast<char>(0xF0u | (codepoint >> 18)));
        salida.push_back(static_cast<char>(0x80u | ((codepoint >> 12) & 0x3Fu)));
        salida.push_back(static_cast<char>(0x80u | ((codepoint >> 6) & 0x3Fu)));
        salida.push_back(static_cast<char>(0x80u | (codepoint & 0x3Fu)));
    }
    return salida;
}

// Indica si el codepoint es una letra segun las reglas del dominio.
bool esLetra(std::uint32_t codepoint) {
    if (codepoint >= LETRA_ASCII_MAYUSCULA_INICIO && codepoint <= LETRA_ASCII_MAYUSCULA_FIN) {
        return true;
    }
    if (codepoint >= LETRA_ASCII_MINUSCULA_INICIO && codepoint <= LETRA_ASCII_MINUSCULA_FIN) {
        return true;
    }
    // Los signos de multiplicacion y division viven dentro del rango Latin-1
    // pero no son letras: sin esta exclusion, "3 x 4" pasaria como un nombre.
    if (codepoint == MULTIPLICACION_LATIN1 || codepoint == DIVISION_LATIN1) {
        return false;
    }
    // Rango Latin-1 Supplement + Latin Extended: tildes, enye, dieresis.
    return codepoint >= LATIN1_INICIO && codepoint <= LATIN1_FIN;
}

// Indica si el codepoint es un separador admitido en los nombres.
bool esSeparador(std::uint32_t codepoint) {
    return codepoint == ' ' || codepoint == '\'' || codepoint == '-';
}

// Indica si el codepoint es un digito decimal ASCII.
bool esDigito(std::uint32_t codepoint) {
    return codepoint >= '0' && codepoint <= '9';
}

// Indica si el tramo [inicio, inicio+cantidad) del texto son solo digitos.
bool sonDigitos(const std::string& texto, std::size_t inicio, std::size_t cantidad) {
    if (inicio + cantidad > texto.size()) {
        return false;
    }
    for (std::size_t i = inicio; i < inicio + cantidad; ++i) {
        if (texto[i] < '0' || texto[i] > '9') {
            return false;
        }
    }
    return true;
}

} // namespace

std::string Utils::trim(const std::string& texto) {
    std::size_t inicio = 0;
    while (inicio < texto.size() && std::isspace(static_cast<unsigned char>(texto[inicio]))) {
        ++inicio;
    }

    std::size_t fin = texto.size();
    while (fin > inicio && std::isspace(static_cast<unsigned char>(texto[fin - 1]))) {
        --fin;
    }

    return texto.substr(inicio, fin - inicio);
}

bool Utils::esEntero(const std::string& texto) {
    if (texto.empty()) {
        return false;
    }

    std::size_t i = 0;
    if (texto[0] == '+' || texto[0] == '-') {
        i = 1;
    }
    if (i >= texto.size()) {
        return false;  // era solo un signo
    }

    for (; i < texto.size(); ++i) {
        if (texto[i] < '0' || texto[i] > '9') {
            return false;  // "3.5", "12a", etc.
        }
    }
    return true;
}

bool Utils::esNoVacio(const std::string& texto) {
    return !trim(texto).empty();
}

bool Utils::esAlfabetico(const std::string& texto) {
    bool tieneLetra = false;
    std::size_t i = 0;

    while (i < texto.size()) {
        std::uint32_t codepoint = 0;
        std::size_t largo = 0;
        if (!decodificarUtf8(texto, i, codepoint, largo)) {
            return false;  // UTF-8 malformado: la cadena no es valida
        }
        i += largo;

        if (esLetra(codepoint)) {
            tieneLetra = true;
            continue;
        }
        if (esSeparador(codepoint)) {
            continue;
        }
        return false;  // digito, emoji, ideograma, etc.
    }

    // Exige al menos una letra: "   " y "---" no son nombres.
    return tieneLetra;
}

bool Utils::esAlfanumericoConSignos(const std::string& texto, const std::string& signosAdmitidos) {
    // Un texto vacio no es un valor valido de ningun campo: no aporta nada.
    if (texto.empty()) {
        return false;
    }

    std::size_t i = 0;

    while (i < texto.size()) {
        std::uint32_t codepoint = 0;
        std::size_t largo = 0;
        if (!decodificarUtf8(texto, i, codepoint, largo)) {
            return false;  // UTF-8 malformado: la cadena no es valida
        }
        i += largo;

        // El codepoint DECODIFICADO se compara contra los signos admitidos.
        // Por eso un caracter multibyte (que vale >= 0x80) nunca puede
        // coincidir con un signo ASCII: la comparacion es exacta.
        if (codepoint < 0x80u) {
            const char caracter = static_cast<char>(codepoint);
            if (signosAdmitidos.find(caracter) != std::string::npos) {
                continue;
            }
        }

        if (esLetra(codepoint) || esDigito(codepoint)) {
            continue;
        }
        return false;  // emoji, ideograma, o un signo que no esta admitido.
    }

    return true;
}

std::size_t Utils::cantidadCaracteres(const std::string& texto) {
    std::size_t cantidad = 0;
    std::size_t i = 0;

    while (i < texto.size()) {
        std::uint32_t codepoint = 0;
        std::size_t largo = 0;
        if (!decodificarUtf8(texto, i, codepoint, largo)) {
            // UTF-8 malformado: se cuenta el byte roto como un caracter, para
            // que la longitud nunca sea MENOR que la cantidad de bytes.
            ++cantidad;
            ++i;
            continue;
        }
        i += largo;
        ++cantidad;
    }

    return cantidad;
}

std::string Utils::aMinusculas(const std::string& texto) {
    std::string resultado;
    resultado.reserve(texto.size());

    std::size_t i = 0;
    while (i < texto.size()) {
        std::uint32_t codepoint = 0;
        std::size_t largo = 0;
        if (!decodificarUtf8(texto, i, codepoint, largo)) {
            // Secuencia malformada: se copia tal cual para no perder datos.
            resultado.push_back(texto[i]);
            ++i;
            continue;
        }

        std::uint32_t destino = codepoint;
        if (codepoint >= LETRA_ASCII_MAYUSCULA_INICIO && codepoint <= LETRA_ASCII_MAYUSCULA_FIN) {
            destino = codepoint + ('a' - 'A');
        } else if (codepoint >= LATIN1_MAYUSCULA_INICIO && codepoint <= LATIN1_MAYUSCULA_FIN) {
            // Rango de 2 bytes: hay que reescribir los DOS bytes, por eso se
            // reconstruye la secuencia completa desde el codepoint.
            const std::uint32_t minuscula =
                TABLA_MINUSCULA_LATIN1[codepoint - LATIN1_MAYUSCULA_INICIO];
            if (minuscula != 0) {
                destino = minuscula;
            }
        }

        if (destino == codepoint) {
            resultado.append(texto, i, largo);
        } else {
            resultado.append(codepointAUtf8(destino));
        }
        i += largo;
    }

    return resultado;
}

std::string Utils::sinTildes(const std::string& texto) {
    std::string resultado;
    resultado.reserve(texto.size());

    std::size_t i = 0;
    while (i < texto.size()) {
        std::uint32_t codepoint = 0;
        std::size_t largo = 0;
        if (!decodificarUtf8(texto, i, codepoint, largo)) {
            // Secuencia malformada: se copia tal cual para no perder datos.
            resultado.push_back(texto[i]);
            ++i;
            continue;
        }

        // LA LETRA BASE, SI EL RANGO LA TIENE
        //
        // La tabla tiene UNA entrada por codepoint del Latin-1 Supplement, y
        // el valor 0 dice "esta letra no tiene base" (los dos signos).
        if (codepoint >= LATIN1_TILDES_INICIO && codepoint <= LATIN1_TILDES_FIN) {
            const char base = TABLA_SIN_TILDES[codepoint - LATIN1_TILDES_INICIO];
            if (base != '\0') {
                resultado.push_back(base);
                i += largo;
                continue;
            }
        }

        // CUALQUIER OTRO CARACTER SE COPIA TAL CUAL
        //
        // El texto no se decodifica y se vuelve a codificar: se copia el
        // tramo original, asi que un caracter de tres o cuatro bytes no
        // cambia de forma.
        resultado.append(texto, i, largo);
        i += largo;
    }

    return resultado;
}

bool Utils::esFormatoFecha(const std::string& texto) {
    // AAAA-MM-DD: 4 digitos, '-', 2 digitos, '-', 2 digitos.
    if (texto.size() != 10) {
        return false;
    }
    if (texto[4] != '-' || texto[7] != '-') {
        return false;
    }
    return sonDigitos(texto, 0, 4) && sonDigitos(texto, 5, 2) && sonDigitos(texto, 8, 2);
}

bool Utils::esFormatoHora(const std::string& texto) {
    // HH:MM: 2 digitos, ':', 2 digitos.
    if (texto.size() != 5) {
        return false;
    }
    if (texto[2] != ':') {
        return false;
    }
    return sonDigitos(texto, 0, 2) && sonDigitos(texto, 3, 2);
}

std::vector<std::string> Utils::separar(const std::string& texto, char delimitador) {
    std::vector<std::string> campos;
    std::string actual;

    for (const char caracter : texto) {
        if (caracter == delimitador) {
            campos.push_back(trim(actual));
            actual.clear();
        } else {
            actual.push_back(caracter);
        }
    }
    campos.push_back(trim(actual));

    return campos;
}
