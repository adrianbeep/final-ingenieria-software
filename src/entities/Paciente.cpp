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
                const std::string& dni_,
                const std::string& observaciones_) {
    // Se valida a traves de los setters: el constructor no puede dejar una
    // entidad invalida en pie.
    setId(id_);
    setNombre(nombre_);
    setApellido(apellido_);
    setEmail(email_);
    setTelefono(telefono_);
    setDni(dni_);
    setObservaciones(observaciones_);
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

void Paciente::setObservaciones(const std::string& v) {
    const std::string motivo = motivoDeObservacionesInvalidas(v);
    if (!motivo.empty()) {
        throw DatoInvalidoException("observaciones", v, motivo);
    }
    observaciones = Utils::trim(v);
}

bool Paciente::esValido() const {
    return Persona::esValido() && dniValido(dni)
        && motivoDeObservacionesInvalidas(observaciones).empty();
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

std::string Paciente::motivoDeObservacionesInvalidas(const std::string& v) {
    // EL SEPARADOR NO PUEDE APARECER
    //
    // El archivo esta delimitado por '|': un separador dentro de un campo
    // desalinearia TODOS los registros que vienen despues.
    if (v.find('|') != std::string::npos) {
        return "las observaciones no pueden contener el separador '|'";
    }

    // NI SALTOS DE LINEA
    //
    // El archivo es POR LINEAS: un salto de linea partiria el registro en
    // dos y el que viene despues se leeria como un registro entero.
    //
    // Se mira el valor CRUDO (sin recortar): un "\n" al final tambien
    // rompe el archivo, y el recorte de los bordes lo esconderia.
    if (v.find('\n') != std::string::npos || v.find('\r') != std::string::npos) {
        return "las observaciones no pueden contener saltos de linea";
    }

    // EL LARGO MAXIMO
    //
    // El largo se mide en CARACTERES (codepoints), no en bytes: una
    // observacion con tildes ocupa los mismos caracteres.
    if (Utils::cantidadCaracteres(Utils::trim(v)) > OBSERVACIONES_LONGITUD_MAXIMA) {
        return "las observaciones no pueden pasar de " +
            std::to_string(OBSERVACIONES_LONGITUD_MAXIMA) + " caracteres";
    }

    // LA CADENA VACIA ES VALIDA: el campo es opcional.
    return std::string();
}

std::ostream& operator<<(std::ostream& os, const Paciente& paciente) {
    paciente.escribirComunes(os);
    // EL CAMPO OPCIONAL VIAJA SIEMPRE, AUNQUE ESTE VACIO
    //
    // El registro tiene la misma cantidad de campos que el esquema: si el
    // `|` final no se escribiera, el archivo tendria una linea con menos
    // campos y el loader la rechazaria.
    return os << '|' << paciente.dni << '|' << paciente.observaciones;
}

Paciente Paciente::fromRegistro(const std::string& linea) {
    const std::vector<std::string> campos = Utils::separar(linea, '|');
    // 7 u 8 campos: con 7 (el formato v2) no hay `observaciones` y queda
    // vacia; con 8 (el formato actual) es el septimo campo.
    if (campos.size() != CANTIDAD_CAMPOS && campos.size() != CANTIDAD_CAMPOS_ANTERIOR) {
        throw DatoInvalidoException("registro", linea,
                                    "se esperaban 7 u 8 campos para un paciente y llegaron "
                                        + std::to_string(campos.size()));
    }

    Paciente paciente;
    paciente.leerComunes(campos);
    paciente.setDni(campos[6]);
    if (campos.size() == CANTIDAD_CAMPOS) {
        paciente.setObservaciones(campos[7]);
    }
    return paciente;
}

std::string Paciente::esquema() {
    return "id|nombre|apellido|email|telefono|activo|dni|observaciones";
}

std::string Paciente::esquemaAnterior() {
    // EL ESQUEMA DE LA v2: EL MISMO QUE EL DE HOY, MENOS `observaciones`
    //
    // Es el esquema que el paciente tenia antes de que el campo existiera:
    // el loader lo usa para leer los archivos v2 (que se leen con las
    // `observaciones` vacias).
    return "id|nombre|apellido|email|telefono|activo|dni";
}
