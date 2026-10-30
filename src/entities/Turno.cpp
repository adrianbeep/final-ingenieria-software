#include "entities/Turno.h"

#include "domain/Materia.h"
#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <ostream>
#include <string>
#include <vector>

namespace {

// Convierte un texto a entero, validando el formato antes de convertir.
int enteroDesdeTexto(const std::string& texto, const std::string& campo) {
    if (!Utils::esEntero(texto)) {
        throw DatoInvalidoException(campo, texto, "se esperaba un numero entero");
    }
    return std::stoi(texto);
}

} // namespace

Turno::Turno(int id_,
             int idProfesor_,
             int idAlumno_,
             Materia materia_,
             const Fecha& fecha_,
             const Hora& hora_,
             int duracionMin_,
             const std::string& aula_,
             EstadoTurno estado_,
             int idTurnoReemplazante_) {
    // Se valida a traves de los setters: el constructor no puede dejar un
    // turno invalido en pie.
    setId(id_);
    setIdProfesor(idProfesor_);
    setIdAlumno(idAlumno_);
    setMateria(materia_);
    setFecha(fecha_);
    setHora(hora_);
    setDuracionMin(duracionMin_);
    setAula(aula_);
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

void Turno::setAula(const std::string& v) {
    if (!aulaValida(v)) {
        throw DatoInvalidoException("aula", v,
                                    "se esperaba un aula de 1 a 20 caracteres, con letras, digitos, "
                                    "espacios, '-' y '.'");
    }
    aula = Utils::trim(v);
}

void Turno::setIdTurnoReemplazante(int idTurnoReemplazante_) {
    if (idTurnoReemplazante_ != -1 && idTurnoReemplazante_ <= 0) {
        throw DatoInvalidoException("idTurnoReemplazante", std::to_string(idTurnoReemplazante_),
                                    "se esperaba un id de turno valido o -1 (sin reemplazo)");
    }
    idTurnoReemplazante = idTurnoReemplazante_;
}

bool Turno::esValido() const {
    return duracionMinValido(duracionMin) && aulaValida(aula)
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

bool Turno::aulaValida(const std::string& v) {
    const std::string t = Utils::trim(v);
    if (t.size() < AULA_LONGITUD_MINIMA || t.size() > AULA_LONGITUD_MAXIMA) {
        return false;
    }
    // ALFABETO CERRADO: el separador del formato canonico queda excluido
    // por construccion.
    for (const char c : t) {
        const bool alfanumerico = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')
            || (c >= '0' && c <= '9');
        if (!alfanumerico && c != ' ' && c != '-' && c != '.') {
            return false;
        }
    }
    return true;
}

std::ostream& operator<<(std::ostream& os, const Turno& turno) {
    os << turno.id << '|' << turno.idProfesor << '|' << turno.idAlumno << '|'
       << codigoDe(turno.materia) << '|' << turno.fecha.aTexto() << '|' << turno.hora.aTexto()
       << '|' << turno.duracionMin << '|' << turno.aula << '|' << estadoATexto(turno.estado)
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
    turno.setIdProfesor(enteroDesdeTexto(campos[1], "idProfesor"));
    turno.setIdAlumno(enteroDesdeTexto(campos[2], "idAlumno"));
    turno.setMateria(materiaDesdeCodigo(campos[3]));
    turno.setFecha(Fecha::desdeTexto(campos[4]));
    turno.setHora(Hora::desdeTexto(campos[5]));
    turno.setDuracionMin(enteroDesdeTexto(campos[6], "duracionMin"));
    turno.setAula(campos[7]);
    turno.setEstado(estadoDesdeTexto(campos[8]));
    turno.setIdTurnoReemplazante(enteroDesdeTexto(campos[9], "idTurnoReemplazante"));
    return turno;
}

std::string Turno::esquema() {
    // El 4º campo se llama `codigoMateria` y no `materia` a proposito: lo que
    // viaja es el CODIGO de la materia ("ALG1"), nunca el indice del enum. El
    // nombre del campo lo deja escrito en el header del archivo.
    return "id|idProfesor|idAlumno|codigoMateria|fecha|hora|duracionMin|aula|estado|idTurnoReemplazante";
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
