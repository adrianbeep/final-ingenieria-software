#include "exceptions/ExcepcionesSistema.h"

namespace {

// Arma el mensaje de DatoInvalidoException. Es una funcion auxiliar (y no un
// constructor delegante) porque el mensaje completo depende de los tres
// datos y la clase base se inicializa ANTES que los miembros.
std::string componerMensajeDatoInvalido(const std::string& campo,
                                        const std::string& valor,
                                        const std::string& motivo) {
    return "Dato invalido en campo '" + campo + "': '" + valor + "' (" + motivo + ")";
}

} // namespace

ExcepcionSistema::ExcepcionSistema(const std::string& mensaje)
    : std::runtime_error(mensaje) {
}

ArchivoNoEncontradoException::ArchivoNoEncontradoException(const std::string& ruta)
    : ExcepcionSistema("No se encontro el archivo '" + ruta + "'") {
}

IdDuplicadoException::IdDuplicadoException(int id, const std::string& entidad)
    : ExcepcionSistema("Id duplicado: " + std::to_string(id) + " (" + entidad + ")") {
}

DatoInvalidoException::DatoInvalidoException(const std::string& campo,
                                             const std::string& valor,
                                             const std::string& motivo)
    : ExcepcionSistema(componerMensajeDatoInvalido(campo, valor, motivo)),
      campo_(campo),
      valor_(valor) {
}

const std::string& DatoInvalidoException::campo() const {
    return campo_;
}

const std::string& DatoInvalidoException::valor() const {
    return valor_;
}

EntidadNoExisteException::EntidadNoExisteException(int id, const std::string& entidad)
    : ExcepcionSistema("No existe la entidad #" + std::to_string(id) + " (" + entidad + ")") {
}

TurnoOcupadoException::TurnoOcupadoException(int idTurnoExistente, const std::string& detalle)
    : ExcepcionSistema("El turno #" + std::to_string(idTurnoExistente) +
                        " ya ocupa ese horario (" + detalle + ")") {
}

TurnoNoExisteException::TurnoNoExisteException(int id)
    : ExcepcionSistema("No existe el turno #" + std::to_string(id)) {
}
