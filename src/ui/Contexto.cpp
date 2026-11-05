#include "ui/Contexto.h"

namespace ui {

Contexto::Contexto(RepositorioAlumnos& alumnos,
                   RepositorioProfesores& profesores,
                   RepositorioTurnos& turnos,
                   ServicioTurnos& servicioTurnos)
    : _alumnos(alumnos),
      _profesores(profesores),
      _turnos(turnos),
      _servicioTurnos(servicioTurnos),
      _mensaje(Mensaje::vacio()) {
}

RepositorioAlumnos& Contexto::alumnos() const {
    return _alumnos;
}

RepositorioProfesores& Contexto::profesores() const {
    return _profesores;
}

RepositorioTurnos& Contexto::turnos() const {
    return _turnos;
}

ServicioTurnos& Contexto::servicioTurnos() const {
    return _servicioTurnos;
}

void Contexto::mostrarMensaje(const Mensaje& mensaje) {
    _mensaje = mensaje;
}

const Mensaje& Contexto::mensaje() const {
    return _mensaje;
}

} // namespace ui
