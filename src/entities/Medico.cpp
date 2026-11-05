#include "entities/Medico.h"

#include "domain/Especialidad.h"
#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <ostream>
#include <string>
#include <vector>

Medico::Medico(int id_,
               const std::string& nombre_,
               const std::string& apellido_,
               const std::string& email_,
               const std::string& telefono_,
               const std::string& legajo_,
               const Especialidad& especialidad_) {
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

std::string Medico::getTipo() const {
    return "Medico";
}

void Medico::setLegajo(const std::string& v) {
    if (!legajoValido(v)) {
        throw DatoInvalidoException("legajo", v,
                                    "se esperaba un legajo alfanumerico de 4 a 12 caracteres");
    }
    legajo = Utils::trim(v);
}

void Medico::setEspecialidad(const Especialidad& v) {
    // NO HAY NADA QUE VALIDAR: la especialidad es un valor del catalogo
    // (`enum class`), no un texto libre. Todo valor del enum es valido por
    // construccion: la unica forma de que sea invalido es que el enum no
    // tenga ese valor (un `static_cast` desde fuera), y eso lo detecta el
    // catalogo cuando se le pide el codigo o la descripcion.
    especialidad = v;
}

bool Medico::esValido() const {
    return Persona::esValido() && legajoValido(legajo);
}

bool Medico::legajoValido(const std::string& v) {
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

std::ostream& operator<<(std::ostream& os, const Medico& medico) {
    medico.escribirComunes(os);
    // La especialidad viaja con su CODIGO ("CARD"), no con su descripcion:
    // el archivo guarda el codigo corto y estable, y el texto largo puede
    // cambiar sin invalidar los datos.
    return os << '|' << medico.legajo << '|' << codigoDe(medico.especialidad);
}

Medico Medico::fromRegistro(const std::string& linea) {
    const std::vector<std::string> campos = Utils::separar(linea, '|');
    if (campos.size() != CANTIDAD_CAMPOS) {
        throw DatoInvalidoException("registro", linea,
                                    "se esperaban 8 campos para un medico y llegaron "
                                        + std::to_string(campos.size()));
    }

    Medico medico;
    medico.leerComunes(campos);
    medico.setLegajo(campos[6]);
    // El CODIGO de la especialidad se traduce al valor del catalogo: si el
    // codigo no existe, `especialidadDesdeCodigo` lanza `DatoInvalidoException`
    // con el campo "especialidad".
    medico.setEspecialidad(especialidadDesdeCodigo(campos[7]));
    return medico;
}

std::string Medico::esquema() {
    return "id|nombre|apellido|email|telefono|activo|legajo|especialidad";
}
