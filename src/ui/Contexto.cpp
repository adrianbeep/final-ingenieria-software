#include "ui/Contexto.h"

namespace ui {

Contexto::Contexto(RepositorioPacientes& pacientes,
                   RepositorioMedicos& medicos,
                   RepositorioTurnos& turnos,
                   ServicioTurnos& servicioTurnos)
    : _pacientes(pacientes),
      _medicos(medicos),
      _turnos(turnos),
      _servicioTurnos(servicioTurnos),
      _mensaje(Mensaje::vacio()) {
}

RepositorioPacientes& Contexto::pacientes() const {
    return _pacientes;
}

RepositorioMedicos& Contexto::medicos() const {
    return _medicos;
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
