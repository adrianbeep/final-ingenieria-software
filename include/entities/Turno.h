#pragma once

#include "domain/Especialidad.h"
#include "utils/Fecha.h"

#include <cstddef>
#include <ostream>
#include <string>

// ---------------------------------------------------------------------------
// Estado de un turno.
enum class EstadoTurno {
    Activo,
    Cancelado,
    Reprogramado
};

// Texto del estado, para persistir y para mostrar.
std::string estadoATexto(EstadoTurno estado);

// Busca el estado por su texto. Lanza `DatoInvalidoException` si el texto no
// corresponde a ningún estado.
EstadoTurno estadoDesdeTexto(const std::string& texto);

// ---------------------------------------------------------------------------
// Turno: la RELACION entre un medico, un paciente, una especialidad, una fecha y
// una hora.
//
// POR QUE NO HEREDA DE `Persona`: un turno NO es una persona: no tiene
// nombre, ni email, ni telefono. Heredar seria un "es-un" falso. Un turno
// TIENE-UN medico y un paciente, pero solo guarda sus ids.
//
// FORMATO CANONICO (el que usa el archivo de datos de la Fase 4):
//   id|idMedico|idPaciente|codigoEspecialidad|fecha|hora|duracionMin|consultorio|estado|idTurnoReemplazante
//
// `especialidad` viaja con su CODIGO ("CARD"), nunca con el indice del enum:
// reordenar el enum no tiene que invalidar los archivos de datos.
//
// POR QUE EL `friend`: igual que en las demas entidades, el `operator<<` es
// una funcion libre que necesita leer el estado privado, y el `friend` da el
// acceso minimo sin agrandar la interfaz publica.
// ---------------------------------------------------------------------------
class Turno {
public:
    // --- Regla de Cinco ---
    Turno() = default;
    Turno(const Turno&) = default;
    Turno& operator=(const Turno&) = default;
    Turno(Turno&&) noexcept = default;
    Turno& operator=(Turno&&) noexcept = default;
    ~Turno() = default;

    // Construye un turno validando TODOS los datos a traves de los setters
    // (que lanzan `DatoInvalidoException`).
    Turno(int id_,
          int idMedico_,
          int idPaciente_,
          Especialidad especialidad_,
          const Fecha& fecha_,
          const Hora& hora_,
          int duracionMin_,
          const std::string& consultorio_,
          EstadoTurno estado_ = EstadoTurno::Activo,
          int idTurnoReemplazante_ = -1);

    // --- Getters ---
    int getId() const { return id; }
    int getIdMedico() const { return idMedico; }
    int getIdPaciente() const { return idPaciente; }
    Especialidad getEspecialidad() const { return especialidad; }
    const Fecha& getFecha() const { return fecha; }
    const Hora& getHora() const { return hora; }
    int getDuracionMin() const { return duracionMin; }
    const std::string& getConsultorio() const { return consultorio; }
    EstadoTurno getEstado() const { return estado; }
    int getIdTurnoReemplazante() const { return idTurnoReemplazante; }

    // --- Setters con validacion ---
    void setIdMedico(int idMedico_) { idMedico = idMedico_; }
    void setIdPaciente(int idPaciente_) { idPaciente = idPaciente_; }
    void setEspecialidad(Especialidad especialidad_) { especialidad = especialidad_; }

    // La `Fecha` y la `Hora` de la Fase 1 se pueden construir con valores
    // invalidos (la validacion vive en `Fecha::esValida` / `Hora::esValida`),
    // asi que los setters del turno las validan y lanzan.
    void setFecha(const Fecha& fecha_);
    void setHora(const Hora& hora_);

    void setDuracionMin(int duracionMin_);
    void setConsultorio(const std::string& v);
    void setEstado(EstadoTurno estado_) { estado = estado_; }
    void setIdTurnoReemplazante(int idTurnoReemplazante_);

    bool esValido() const;

    // --- Operadores ---
    // Un turno es un unico tipo, asi que la igualdad es solo por id.
    bool operator==(const Turno& otra) const { return id == otra.id; }
    bool operator<(const Turno& otra) const { return id < otra.id; }

    // Escribe el registro canonico en el sumidero indicado.
    friend std::ostream& operator<<(std::ostream& os, const Turno& turno);

    // Parsea un registro canonico. Lanza `DatoInvalidoException` si la linea
    // esta mal formada o si tiene una cantidad de campos incorrecta.
    static Turno fromRegistro(const std::string& linea);

    // Nombres de los campos del formato canonico, separados por '|'.
    static std::string esquema();

    // --- Limites de los campos, con nombre ---
    static constexpr int DURACION_MULTIPLO_MINUTOS = 15;
    static constexpr int DURACION_MAXIMA_MINUTOS = 240;
    static constexpr std::size_t CONSULTORIO_LONGITUD_MINIMA = 1;
    static constexpr std::size_t CONSULTORIO_LONGITUD_MAXIMA = 20;

protected:
    // El id lo asigna el repositorio, no el usuario: por eso es `protected`
    // y no hay un `setId` publico. Es el mismo invariante que el de
    // `Persona` (ver include/entities/Persona.h), para que las TRES
    // entidades se comporten igual.
    void setId(int id_) { id = id_; }

private:
    static constexpr std::size_t CANTIDAD_CAMPOS = 10;

    static bool duracionMinValido(int valor);
    static bool consultorioValido(const std::string& v);

    int id = -1;  // -1 = sin asignar
    int idMedico = -1;
    int idPaciente = -1;
    Especialidad especialidad = Especialidad::CARD;
    Fecha fecha;
    Hora hora;
    int duracionMin = 0;
    std::string consultorio;
    EstadoTurno estado = EstadoTurno::Activo;
    int idTurnoReemplazante = -1;  // -1 = este turno no reemplaza a otro
};
