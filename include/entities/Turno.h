#pragma once

#include "domain/Materia.h"
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
// Turno: la RELACION entre un profesor, un alumno, una materia, una fecha y
// una hora.
//
// POR QUE NO HEREDA DE `Persona`: un turno NO es una persona: no tiene
// nombre, ni email, ni telefono. Heredar seria un "es-un" falso. Un turno
// TIENE-UN profesor y un alumno, pero solo guarda sus ids.
//
// FORMATO CANONICO (el que usa el archivo de datos de la Fase 4):
//   id|idProfesor|idAlumno|codigoMateria|fecha|hora|duracionMin|aula|estado|idTurnoReemplazante
//
// `materia` viaja con su CODIGO ("ALG1"), nunca con el indice del enum:
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
          int idProfesor_,
          int idAlumno_,
          Materia materia_,
          const Fecha& fecha_,
          const Hora& hora_,
          int duracionMin_,
          const std::string& aula_,
          EstadoTurno estado_ = EstadoTurno::Activo,
          int idTurnoReemplazante_ = -1);

    // --- Getters ---
    int getId() const { return id; }
    int getIdProfesor() const { return idProfesor; }
    int getIdAlumno() const { return idAlumno; }
    Materia getMateria() const { return materia; }
    const Fecha& getFecha() const { return fecha; }
    const Hora& getHora() const { return hora; }
    int getDuracionMin() const { return duracionMin; }
    const std::string& getAula() const { return aula; }
    EstadoTurno getEstado() const { return estado; }
    int getIdTurnoReemplazante() const { return idTurnoReemplazante; }

    // --- Setters con validacion ---
    void setId(int id_) { id = id_; }
    void setIdProfesor(int idProfesor_) { idProfesor = idProfesor_; }
    void setIdAlumno(int idAlumno_) { idAlumno = idAlumno_; }
    void setMateria(Materia materia_) { materia = materia_; }

    // La `Fecha` y la `Hora` de la Fase 1 se pueden construir con valores
    // invalidos (la validacion vive en `Fecha::esValida` / `Hora::esValida`),
    // asi que los setters del turno las validan y lanzan.
    void setFecha(const Fecha& fecha_);
    void setHora(const Hora& hora_);

    void setDuracionMin(int duracionMin_);
    void setAula(const std::string& v);
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
    static constexpr std::size_t AULA_LONGITUD_MINIMA = 1;
    static constexpr std::size_t AULA_LONGITUD_MAXIMA = 20;

private:
    static constexpr std::size_t CANTIDAD_CAMPOS = 10;

    static bool duracionMinValido(int valor);
    static bool aulaValida(const std::string& v);

    int id = -1;  // -1 = sin asignar
    int idProfesor = -1;
    int idAlumno = -1;
    Materia materia = Materia::ALG1;
    Fecha fecha;
    Hora hora;
    int duracionMin = 0;
    std::string aula;
    EstadoTurno estado = EstadoTurno::Activo;
    int idTurnoReemplazante = -1;  // -1 = este turno no reemplaza a otro
};
