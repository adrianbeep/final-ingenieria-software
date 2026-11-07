#include "entities/Paciente.h"

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <ostream>
#include <string>
#include <vector>

Paciente::Paciente(int id_,
                const std::string& nombre_,
                const std::string& apellido_,
                const std::string& email_,
                const std::string& telefono_,
                const std::string& dni_) {
    // Se valida a traves de los setters: el constructor no puede dejar una
    // entidad invalida en pie.
    setId(id_);
    setNombre(nombre_);
    setApellido(apellido_);
    setEmail(email_);
    setTelefono(telefono_);
    setDni(dni_);
}

std::string Paciente::getTipo() const {
    return "Paciente";
}

void Paciente::setDni(const std::string& v) {
    if (!dniValido(v)) {
        throw DatoInvalidoException("dni", v,
                                    "se esperaba un dni de 7 u 8 digitos, todos numericos");
    }
    dni = Utils::trim(v);
}

bool Paciente::esValido() const {
    return Persona::esValido() && dniValido(dni);
}

bool Paciente::dniValido(const std::string& v) {
    const std::string t = Utils::trim(v);
    if (t.size() < DNI_LONGITUD_MINIMA || t.size() > DNI_LONGITUD_MAXIMA) {
        return false;
    }
    // SOLO DIGITOS: un DNI es un NUMERO, no un texto. No se admiten letras,
    // ni signos, ni espacios (los espacios de los bordes los recorta el
    // setter). El separador del formato canonico queda excluido por
    // construccion.
    for (const char c : t) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, const Paciente& paciente) {
    paciente.escribirComunes(os);
    return os << '|' << paciente.dni;
}

Paciente Paciente::fromRegistro(const std::string& linea) {
    const std::vector<std::string> campos = Utils::separar(linea, '|');
    if (campos.size() != CANTIDAD_CAMPOS) {
        throw DatoInvalidoException("registro", linea,
                                    "se esperaban 7 campos para un paciente y llegaron "
                                        + std::to_string(campos.size()));
    }

    Paciente paciente;
    paciente.leerComunes(campos);
    paciente.setDni(campos[6]);
    return paciente;
}

std::string Paciente::esquema() {
    return "id|nombre|apellido|email|telefono|activo|dni";
}
