#pragma once

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <cstddef>
#include <ostream>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Persona: la base compartida de `Profesor` y `Alumno`.
//
// POR QUE ES HEADER-ONLY (sin .cpp): es la clase base de dos librerias
// distintas. Si tuviera su propio .cpp habria que meterla en un target
// comun o duplicarla en dos librerias, y duplicarla rompe el link por el
// vtable. Header-only evita el problema: el vtable se emite como simbolo
// debil y el linker lo unifica.
//
// POR QUE INCLUYE LA CABECERA DE EXCEPCIONES (a diferencia de `Fecha.h`):
// esta clase es header-only, asi que las implementaciones de los setters
// (que lanzan `DatoInvalidoException`) viven en el header y necesitan el
// tipo completo. Es la unica dependencia que arrastra el header.
//
// EL DOMINIO NO HACE E/S: la unica salida de datos es `std::ostream`, que es
// un "sumidero" al que se le puede escribir sin que exista ninguna pantalla
// ni ningun archivo. El `operator<<` de cada entidad escribe el registro
// canonico en ese sumidero.
//
// REGLA DE NOMBRES DE PARAMETROS: los constructores y setters usan sufijo
// `_` en los parametros (`Turno(int id_, ...)`). No es estilo: el build
// compila con `-Wshadow`, que avisa cuando un parametro se llama igual que
// un miembro, y el build tiene que dar cero warnings.
// ---------------------------------------------------------------------------
class Persona {
public:
    // --- Regla de Cinco, declarada explicitamente ---
    // Declarar un destructor suprime el constructor de movimiento implicito:
    // con `std::string` adentro, las clases copiarian en vez de mover.
    // Declarando los cinco con `= default` el comportamiento queda en el
    // compilador (que es el optimo) y desaparecen los warnings de
    // `-Wdeprecated-copy`.
    Persona() = default;
    Persona(const Persona&) = default;
    Persona& operator=(const Persona&) = default;
    Persona(Persona&&) noexcept = default;
    Persona& operator=(Persona&&) noexcept = default;

    // Destructor VIRTUAL: `Persona` es una base polimorfica (tiene
    // `getTipo()` virtual puro) y toda clase base con funciones virtuales
    // debe tener destructor virtual.
    virtual ~Persona() = default;

    // Tipo de la persona, en texto: "Profesor" / "Alumno".
    virtual std::string getTipo() const = 0;

    // --- Getters ---
    int getId() const { return id; }
    const std::string& getNombre() const { return nombre; }
    const std::string& getApellido() const { return apellido; }
    const std::string& getEmail() const { return email; }
    const std::string& getTelefono() const { return telefono; }
    bool esActivo() const { return activo; }

    // --- Setters con validacion: lanzan `DatoInvalidoException` ---
    // Con excepcion es IMPOSIBLE olvidarse de validar: el llamador no puede
    // ignorar el error como pasaria con un `bool setX(...)`.
    void setNombre(const std::string& v) {
        if (!nombreValido(v)) {
            throw DatoInvalidoException("nombre", v, "se esperaba un nombre alfabetico no vacio");
        }
        nombre = Utils::trim(v);
    }

    void setApellido(const std::string& v) {
        if (!apellidoValido(v)) {
            throw DatoInvalidoException("apellido", v, "se esperaba un apellido alfabetico no vacio");
        }
        apellido = Utils::trim(v);
    }

    void setEmail(const std::string& v) {
        if (!emailValido(v)) {
            throw DatoInvalidoException("email", v,
                                        "se esperaba un email de 5 a 60 caracteres, con arroba, "
                                        "sin espacios y con letras, digitos, '.', '-', '_'");
        }
        email = Utils::trim(v);
    }

    void setTelefono(const std::string& v) {
        if (!telefonoValido(v)) {
            throw DatoInvalidoException("telefono", v,
                                        "se esperaba un telefono de 7 a 20 caracteres, con digitos, "
                                        "'+', '-' y espacios");
        }
        telefono = Utils::trim(v);
    }

    void setActivo(bool v) { activo = v; }

    // `esValido()` solo tiene sentido para el objeto construido por defecto
    // (campos vacios -> false), porque los setters ya validan: sirve para
    // comprobar que un objeto construido a mano por defecto esta completo.
    virtual bool esValido() const {
        return nombreValido(nombre) && apellidoValido(apellido)
            && emailValido(email) && telefonoValido(telefono);
    }

    // --- Operadores ---
    // `operator==` compara el id Y el tipo: el id es unico POR ENTIDAD, no
    // global, asi que `Profesor(5) == Alumno(5)` tiene que ser false.
    // Comparar solo por el id romperia el "strict weak ordering" que necesita
    // el arbol de la Fase 3: si `a` no es menor que `b` ni al reves, `a`
    // tiene que ser igual a `b`.
    bool operator==(const Persona& otra) const {
        return id == otra.id && getTipo() == otra.getTipo();
    }

    // `operator<` ordena por id y usa el tipo como desempate, para que el
    // orden sea total y determinista.
    bool operator<(const Persona& otra) const {
        if (id != otra.id) {
            return id < otra.id;
        }
        return getTipo() < otra.getTipo();
    }

    // --- Limites de los campos, con nombre (no hay numeros magicos) ---
    static constexpr std::size_t EMAIL_LONGITUD_MINIMA = 5;
    static constexpr std::size_t EMAIL_LONGITUD_MAXIMA = 60;
    static constexpr std::size_t TELEFONO_LONGITUD_MINIMA = 7;
    static constexpr std::size_t TELEFONO_LONGITUD_MAXIMA = 20;

protected:
    // Cantidad de campos que comparten `Profesor` y `Alumno` en el formato
    // canonico: id|nombre|apellido|email|telefono|activo.
    static constexpr std::size_t CANTIDAD_CAMPOS_COMUNES = 6;

    // El id lo asigna el repositorio, no el usuario: por eso es `protected`
    // y no hay un `setId` publico.
    void setId(int id_) { id = id_; }

    // Escribe los 6 campos comunes del formato canonico. `protected` para que
    // el `operator<<` de cada derivada los use sin repetir el listado.
    void escribirComunes(std::ostream& os) const {
        os << id << '|' << nombre << '|' << apellido << '|' << email << '|'
           << telefono << '|' << (activo ? 1 : 0);
    }

    // Lee los 6 campos comunes del formato canonico. Valida cada campo a
    // traves de los setters, asi que un registro invalido lanza
    // `DatoInvalidoException` con el nombre del campo que fallo.
    void leerComunes(const std::vector<std::string>& campos) {
        if (campos.size() < CANTIDAD_CAMPOS_COMUNES) {
            throw DatoInvalidoException("registro", std::to_string(campos.size()),
                                        "faltan los campos comunes de la persona");
        }
        setId(enteroDesdeTexto(campos[0], "id"));
        setNombre(campos[1]);
        setApellido(campos[2]);
        setEmail(campos[3]);
        setTelefono(campos[4]);
        setActivo(booleanoDesdeTexto(campos[5], "activo"));
    }

private:
    // --- Reglas puras: viven en UN solo lugar y las usan los setters (para
    // --- lanzar) y `esValido()` (para comprobar). ---
    static bool nombreValido(const std::string& v) {
        const std::string t = Utils::trim(v);
        // `esAlfabetico` de la Fase 1 es consciente de UTF-8: "José" y
        // "Muñoz" son validos, un emoji o un ideograma no.
        return Utils::esNoVacio(t) && Utils::esAlfabetico(t);
    }

    // Misma regla que el nombre: no se reimplementa, se delega.
    static bool apellidoValido(const std::string& v) { return nombreValido(v); }

    static bool emailValido(const std::string& v) {
        const std::string t = Utils::trim(v);
        if (t.size() < EMAIL_LONGITUD_MINIMA || t.size() > EMAIL_LONGITUD_MAXIMA) {
            return false;
        }
        // Ademas del alfabeto cerrado, el correo tiene que tener una arroba:
        // un conjunto de caracteres permitidos no alcanza para decir "correo".
        if (t.find('@') == std::string::npos) {
            return false;
        }
        // ALFABETO CERRADO: el separador del formato canonico queda excluido
        // por construccion, sin meter una regla de persistencia en el
        // dominio.
        for (const char c : t) {
            if (!esCaracterAlfanumerico(c) && c != '.' && c != '@' && c != '-' && c != '_') {
                return false;
            }
        }
        return true;
    }

    static bool telefonoValido(const std::string& v) {
        const std::string t = Utils::trim(v);
        if (t.size() < TELEFONO_LONGITUD_MINIMA || t.size() > TELEFONO_LONGITUD_MAXIMA) {
            return false;
        }
        // Un telefono es un NUMERO: digitos, y los Signs que se usan para
        // agrupar ('+', '-', espacios). Las letras no se admiten.
        for (const char c : t) {
            if (!esCaracterDigito(c) && c != '+' && c != '-' && c != ' ') {
                return false;
            }
        }
        return true;
    }

    // Caracter alfanumerico ASCII (los correos y los telefonos no llevan
    // letras acentuadas).
    static bool esCaracterAlfanumerico(char c) {
        return esCaracterDigito(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
    }

    static bool esCaracterDigito(char c) { return c >= '0' && c <= '9'; }

    // Convierte un texto a entero, validando el formato antes de convertir.
    static int enteroDesdeTexto(const std::string& texto, const std::string& campo) {
        if (!Utils::esEntero(texto)) {
            throw DatoInvalidoException(campo, texto, "se esperaba un numero entero");
        }
        return std::stoi(texto);
    }

    // El campo `activo` viaja como "1" o "0" (nunca como "true"/"false").
    static bool booleanoDesdeTexto(const std::string& texto, const std::string& campo) {
        if (texto == "1") {
            return true;
        }
        if (texto == "0") {
            return false;
        }
        throw DatoInvalidoException(campo, texto, "se esperaba 1 (activo) o 0 (inactivo)");
    }

    int id = -1;  // -1 = sin asignar
    std::string nombre;
    std::string apellido;
    std::string email;
    std::string telefono;
    bool activo = true;
};
