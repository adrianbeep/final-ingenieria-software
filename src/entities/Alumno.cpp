#include "entities/Alumno.h"

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <ostream>
#include <string>
#include <vector>

Alumno::Alumno(int id_,
               const std::string& nombre_,
               const std::string& apellido_,
               const std::string& email_,
               const std::string& telefono_,
               const std::string& matricula_) {
    // Se valida a traves de los setters: el constructor no puede dejar una
    // entidad invalida en pie.
    setId(id_);
    setNombre(nombre_);
    setApellido(apellido_);
    setEmail(email_);
    setTelefono(telefono_);
    setMatricula(matricula_);
}

std::string Alumno::getTipo() const {
    return "Alumno";
}

void Alumno::setMatricula(const std::string& v) {
    if (!matriculaValido(v)) {
        throw DatoInvalidoException("matricula", v,
                                    "se esperaba una matricula alfanumerica de 4 a 12 caracteres");
    }
    matricula = Utils::trim(v);
}

bool Alumno::esValido() const {
    return Persona::esValido() && matriculaValido(matricula);
}

bool Alumno::matriculaValido(const std::string& v) {
    const std::string t = Utils::trim(v);
    if (t.size() < MATRICULA_LONGITUD_MINIMA || t.size() > MATRICULA_LONGITUD_MAXIMA) {
        return false;
    }
    // ALFABETO CERRADO: letras, digitos, '-' y '_'. El separador del formato
    // canonico queda excluido por construccion.
    for (const char c : t) {
        const bool alfanumerico = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9');
        if (!alfanumerico && c != '-' && c != '_') {
            return false;
        }
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, const Alumno& alumno) {
    alumno.escribirComunes(os);
    return os << '|' << alumno.matricula;
}

Alumno Alumno::fromRegistro(const std::string& linea) {
    const std::vector<std::string> campos = Utils::separar(linea, '|');
    if (campos.size() != CANTIDAD_CAMPOS) {
        throw DatoInvalidoException("registro", linea,
                                    "se esperaban 7 campos para un alumno y llegaron "
                                        + std::to_string(campos.size()));
    }

    Alumno alumno;
    alumno.leerComunes(campos);
    alumno.setMatricula(campos[6]);
    return alumno;
}

std::string Alumno::esquema() {
    return "id|nombre|apellido|email|telefono|activo|matricula";
}
