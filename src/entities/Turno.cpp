#include "entities/Turno.h"

#include "domain/Especialidad.h"
#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// Convierte un texto a entero, validando el formato ANTES de convertir.
//
// El desborde de rango tambien se valida: `std::stoi` lanza
// `std::out_of_range` (una excepcion de la biblioteca estandar, NO una
// `ExcepcionSistema`) cuando el numero no entra en un `int`, y esa excepcion
// se escapa sin archivo ni linea. Aqui se la traduce al error del dominio,
// que si sabe decir QUE campo fallo y con QUE valor.
int enteroDesdeTexto(const std::string& texto, const std::string& campo) {
    if (!Utils::esEntero(texto)) {
        throw DatoInvalidoException(campo, texto, "se esperaba un numero entero");
    }
    try {
        return std::stoi(texto);
    } catch (const std::out_of_range&) {
        throw DatoInvalidoException(campo, texto,
                                    "el numero esta fuera del rango de un entero de 32 bits");
    } catch (const std::invalid_argument&) {
        // No deberia llegar aca (el formato ya se valido con
        // `Utils::esEntero`), pero se traduce igual para no dejar escapar una
        // excepcion de la biblioteca estandar.
        throw DatoInvalidoException(campo, texto, "se esperaba un numero entero");
    }
}

} // namespace

Turno::Turno(int id_,
             int idMedico_,
             int idPaciente_,
             Especialidad especialidad_,
             const Fecha& fecha_,
             const Hora& hora_,
             int duracionMin_,
             const std::string& consultorio_,
             EstadoTurno estado_,
             int idTurnoReemplazante_) {
    // Se valida a traves de los setters: el constructor no puede dejar un
    // turno invalido en pie.
    setId(id_);
    setIdMedico(idMedico_);
    setIdPaciente(idPaciente_);
    setEspecialidad(especialidad_);
    setFecha(fecha_);
    setHora(hora_);
    setDuracionMin(duracionMin_);
    setConsultorio(consultorio_);
    setEstado(estado_);
    setIdTurnoReemplazante(idTurnoReemplazante_);
}

void Turno::setFecha(const Fecha& fecha_) {
    if (!Fecha::esValida(fecha_.anio, fecha_.mes, fecha_.dia)) {
        throw DatoInvalidoException("fecha", fecha_.aTexto(), "fecha inexistente");
    }
    fecha = fecha_;
}

void Turno::setHora(const Hora& hora_) {
    if (!Hora::esValida(hora_.hora, hora_.minuto)) {
        throw DatoInvalidoException("hora", hora_.aTexto(),
                                    "fuera del horario de atencion o con minutos que no son "
                                    "multiplos de 30");
    }
    hora = hora_;
}

void Turno::setDuracionMin(int duracionMin_) {
    if (!duracionMinValido(duracionMin_)) {
        throw DatoInvalidoException("duracionMin", std::to_string(duracionMin_),
                                    "se esperaba una duracion positiva, multiplo de 15 y de "
                                    "240 minutos como maximo");
    }
    duracionMin = duracionMin_;
}

void Turno::setConsultorio(const std::string& v) {
    if (!consultorioValido(v)) {
        throw DatoInvalidoException("consultorio", v,
                                    "se esperaba un consultorio de 1 a 20 caracteres, con letras, "
                                    "digitos, espacios, '-' y '.'");
    }
    consultorio = Utils::trim(v);
}

void Turno::setIdTurnoReemplazante(int idTurnoReemplazante_) {
    if (idTurnoReemplazante_ != -1 && idTurnoReemplazante_ <= 0) {
        throw DatoInvalidoException("idTurnoReemplazante", std::to_string(idTurnoReemplazante_),
                                    "se esperaba un id de turno valido o -1 (sin reemplazo)");
    }
    idTurnoReemplazante = idTurnoReemplazante_;
}

bool Turno::esValido() const {
    return duracionMinValido(duracionMin) && consultorioValido(consultorio)
        && Fecha::esValida(fecha.anio, fecha.mes, fecha.dia)
        && Hora::esValida(hora.hora, hora.minuto);
}

bool Turno::duracionMinValido(int valor) {
    if (valor <= 0) {
        return false;
    }
    if (valor % DURACION_MULTIPLO_MINUTOS != 0) {
        return false;
    }
    return valor <= DURACION_MAXIMA_MINUTOS;
}

bool Turno::consultorioValido(const std::string& v) {
    const std::string t = Utils::trim(v);
    // El largo se mide en CARACTERES (codepoints), no en bytes: un consultorio
    // con tilde es mas largo en bytes, pero ocupa los mismos caracteres.
    const std::size_t largo = Utils::cantidadCaracteres(t);
    if (largo < CONSULTORIO_LONGITUD_MINIMA || largo > CONSULTORIO_LONGITUD_MAXIMA) {
        return false;
    }
    // ALFABETO CERRADO: el separador del formato canonico ('|') queda excluido
    // por construccion, porque no esta entre los signos admitidos. Las
    // tildes y la enye SI se admiten: son letras del dominio.
    return Utils::esAlfanumericoConSignos(t, " -.");
}

std::ostream& operator<<(std::ostream& os, const Turno& turno) {
    os << turno.id << '|' << turno.idMedico << '|' << turno.idPaciente << '|'
       << codigoDe(turno.especialidad) << '|' << turno.fecha.aTexto() << '|' << turno.hora.aTexto()
       << '|' << turno.duracionMin << '|' << turno.consultorio << '|' << estadoATexto(turno.estado)
       << '|' << turno.idTurnoReemplazante;
    return os;
}

Turno Turno::fromRegistro(const std::string& linea) {
    const std::vector<std::string> campos = Utils::separar(linea, '|');
    if (campos.size() != CANTIDAD_CAMPOS) {
        throw DatoInvalidoException("registro", linea,
                                    "se esperaban 10 campos para un turno y llegaron "
                                        + std::to_string(campos.size()));
    }

    Turno turno;
    turno.setId(enteroDesdeTexto(campos[0], "id"));
    turno.setIdMedico(enteroDesdeTexto(campos[1], "idMedico"));
    turno.setIdPaciente(enteroDesdeTexto(campos[2], "idPaciente"));
    turno.setEspecialidad(especialidadDesdeCodigo(campos[3]));
    turno.setFecha(Fecha::desdeTexto(campos[4]));
    turno.setHora(Hora::desdeTexto(campos[5]));
    turno.setDuracionMin(enteroDesdeTexto(campos[6], "duracionMin"));
    turno.setConsultorio(campos[7]);
    turno.setEstado(estadoDesdeTexto(campos[8]));
    turno.setIdTurnoReemplazante(enteroDesdeTexto(campos[9], "idTurnoReemplazante"));
    return turno;
}

std::string Turno::esquema() {
    // El 4º campo se llama `codigoEspecialidad` y no `especialidad` a proposito: lo que
    // viaja es el CODIGO de la especialidad ("CARD"), nunca el indice del enum. El
    // nombre del campo lo deja escrito en el header del archivo.
    return "id|idMedico|idPaciente|codigoEspecialidad|fecha|hora|duracionMin|consultorio|estado|idTurnoReemplazante";
}

std::string estadoATexto(EstadoTurno estado) {
    switch (estado) {
        case EstadoTurno::Activo:
            return "Activo";
        case EstadoTurno::Cancelado:
            return "Cancelado";
        case EstadoTurno::Reprogramado:
            return "Reprogramado";
    }
    throw DatoInvalidoException("estado", std::to_string(static_cast<int>(estado)),
                                "estado de turno desconocido");
}

EstadoTurno estadoDesdeTexto(const std::string& texto) {
    if (texto == "Activo") {
        return EstadoTurno::Activo;
    }
    if (texto == "Cancelado") {
        return EstadoTurno::Cancelado;
    }
    if (texto == "Reprogramado") {
        return EstadoTurno::Reprogramado;
    }
    throw DatoInvalidoException("estado", texto, "estado de turno desconocido");
}
