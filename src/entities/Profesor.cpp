#include "entities/Profesor.h"

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <ostream>
#include <string>
#include <vector>

Profesor::Profesor(int id_,
                   const std::string& nombre_,
                   const std::string& apellido_,
                   const std::string& email_,
                   const std::string& telefono_,
                   const std::string& legajo_,
                   const std::string& especialidad_) {
    // Se valida a traves de los setters: el constructor no puede dejar una
    // entidad invalida en pie.
    setId(id_);
    setNombre(nombre_);
    setApellido(apellido_);
    setEmail(email_);
    setTelefono(telefono_);
    setLegajo(legajo_);
    setEspecialidad(especialidad_);
}

std::string Profesor::getTipo() const {
    return "Profesor";
}

void Profesor::setLegajo(const std::string& v) {
    if (!legajoValido(v)) {
        throw DatoInvalidoException("legajo", v,
                                    "se esperaba un legajo alfanumerico de 4 a 12 caracteres");
    }
    legajo = Utils::trim(v);
}

void Profesor::setEspecialidad(const std::string& v) {
    if (!especialidadValido(v)) {
        throw DatoInvalidoException("especialidad", v,
                                    "se esperaba una especialidad de 1 a 60 caracteres, con letras, "
                                    "digitos, espacios, ',', '-', '.' y '+'");
    }
    especialidad = Utils::trim(v);
}

bool Profesor::esValido() const {
    return Persona::esValido() && legajoValido(legajo) && especialidadValido(especialidad);
}

bool Profesor::legajoValido(const std::string& v) {
    const std::string t = Utils::trim(v);
    if (t.size() < LEGAJO_LONGITUD_MINIMA || t.size() > LEGAJO_LONGITUD_MAXIMA) {
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

bool Profesor::especialidadValido(const std::string& v) {
    const std::string t = Utils::trim(v);
    if (t.size() < ESPECIALIDAD_LONGITUD_MINIMA || t.size() > ESPECIALIDAD_LONGITUD_MAXIMA) {
        return false;
    }
    for (const char c : t) {
        const bool alfanumerico = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9');
        if (!alfanumerico && c != ' ' && c != ',' && c != '-' && c != '.' && c != '+') {
            return false;
        }
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, const Profesor& profesor) {
    profesor.escribirComunes(os);
    return os << '|' << profesor.legajo << '|' << profesor.especialidad;
}

Profesor Profesor::fromRegistro(const std::string& linea) {
    const std::vector<std::string> campos = Utils::separar(linea, '|');
    if (campos.size() != CANTIDAD_CAMPOS) {
        throw DatoInvalidoException("registro", linea,
                                    "se esperaban 8 campos para un profesor y llegaron "
                                        + std::to_string(campos.size()));
    }

    Profesor profesor;
    profesor.leerComunes(campos);
    profesor.setLegajo(campos[6]);
    profesor.setEspecialidad(campos[7]);
    return profesor;
}

std::string Profesor::esquema() {
    return "id|nombre|apellido|email|telefono|activo|legajo|especialidad";
}
