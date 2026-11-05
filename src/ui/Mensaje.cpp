#include "ui/Mensaje.h"

#include <string>
#include <utility>

namespace ui {
namespace {

// Los prefijos del formato unico.
//
// "OK  " lleva DOS espacios y "ERR " UNO: los dos mensajes arrancan el
// texto en la misma columna (la quinta), que es lo que hace que el bloque de
// mensajes se vea prolijo cuando hay uno debajo de otro.
constexpr const char* PREFIJO_EXITO = "OK  ";
constexpr const char* PREFIJO_ERROR = "ERR ";
constexpr const char* PREFIJO_AVISO = "AVISO ";

} // namespace

Mensaje::Mensaje(Rol rol, std::string texto) : _rol(rol), _texto(std::move(texto)) {
}

Mensaje Mensaje::exito(const std::string& texto) {
    return Mensaje(Rol::Exito, std::string(PREFIJO_EXITO) + texto);
}

Mensaje Mensaje::error(const std::string& texto) {
    return Mensaje(Rol::Error, std::string(PREFIJO_ERROR) + texto);
}

Mensaje Mensaje::aviso(const std::string& texto) {
    return Mensaje(Rol::Aviso, std::string(PREFIJO_AVISO) + texto);
}

Mensaje Mensaje::vacio() {
    return Mensaje(Rol::Ninguno, std::string());
}

const std::string& Mensaje::texto() const {
    return _texto;
}

bool Mensaje::esExito() const {
    return _rol == Rol::Exito;
}

bool Mensaje::esError() const {
    return _rol == Rol::Error;
}

bool Mensaje::esVacio() const {
    return _rol == Rol::Ninguno;
}

} // namespace ui
