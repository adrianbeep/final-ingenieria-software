#pragma once

#include <stdexcept>
#include <string>

// ---------------------------------------------------------------------------
// Jerarquia de excepciones del sistema.
//
// Los nombres van globales (sin namespace) y con sufijo "Exception", tal como
// lo piden las firmas sugeridas en el issue #2.
//
// DECISION DE DISENO (ya tomada en el issue #2, no se revierte):
// `ExcepcionSistema` NO declara destructor virtual propio. No hace falta
// porque hereda de `std::runtime_error`, cuyo destructor YA es virtual: en
// realidad, borrar por puntero a `ExcepcionSistema*` es seguro. Lo que NO se
// hace es un destructor virtual explicito redundante.
//
// HERENCIA: `ExcepcionSistema` deriva de `std::runtime_error` (error de
// tiempo de ejecucion) y NO de `std::logic_error`: los datos invalidos que
// maneja el sistema son errores de ejecucion, no de programacion.
// ---------------------------------------------------------------------------

// Raiz de todas las excepciones propias del sistema.
class ExcepcionSistema : public std::runtime_error {
public:
    explicit ExcepcionSistema(const std::string& mensaje);
};

// No se pudo abrir el archivo de datos solicitado.
class ArchivoNoEncontradoException : public ExcepcionSistema {
public:
    explicit ArchivoNoEncontradoException(const std::string& ruta);
};

// Se intento registrar un identificador que ya existe.
class IdDuplicadoException : public ExcepcionSistema {
public:
    IdDuplicadoException(int id, const std::string& entidad);
};

// Un dato ingresado (o leido de un archivo) no cumple el formato esperado.
// Expone `campo()` y `valor()` para que la capa de UI pueda ubicar el dato
// que fallo y mostralo junto al error.
class DatoInvalidoException : public ExcepcionSistema {
public:
    DatoInvalidoException(const std::string& campo,
                          const std::string& valor,
                          const std::string& motivo);

    const std::string& campo() const;
    const std::string& valor() const;

    // El MOTIVO solo (sin el prefijo "Dato invalido en campo '...': '...'"):
    // la capa de persistencia lo usa para re-lanzar el error agregandole el
    // archivo y el numero de linea, sin duplicar el mensaje entero.
    const std::string& motivo() const;

private:
    std::string campo_;
    std::string valor_;
    std::string motivo_;
};

// SeBusco una entidad por identificador y no existe.
class EntidadNoExisteException : public ExcepcionSistema {
public:
    EntidadNoExisteException(int id, const std::string& entidad);
};

// El horario pedido ya esta ocupado por otro turno.
class TurnoOcupadoException : public ExcepcionSistema {
public:
    TurnoOcupadoException(int idTurnoExistente, const std::string& detalle);
};

// SeBusco un turno por identificador y no existe.
class TurnoNoExisteException : public ExcepcionSistema {
public:
    explicit TurnoNoExisteException(int id);
};

// El archivo de datos NO tiene el formato que el codigo espera.
//
// Cubre los cuatro casos de la Fase 4:
//   - falta el header (la primera linea no empieza con '#')
//   - el magic no coincide (es un archivo de otra entidad)
//   - la version del formato es otra
//   - la linea de esquema declara otros campos
//
// El mensaje lleva el detalle EXACTO de que no calza: sin el, el error
// obligaria a debuggear a ciegas.
class FormatoDesconocidoException : public ExcepcionSistema {
public:
    explicit FormatoDesconocidoException(const std::string& mensaje);
};
