#pragma once

#include "entities/Persona.h"

#include <cstddef>
#include <ostream>
#include <string>

// ---------------------------------------------------------------------------
// Alumno: una Persona que cursa materias.
//
// `matricula` es UNICA en el sistema: la unicidad la verifica
// `RepositorioAlumnos`, que es quien tiene el conjunto de las entidades
// cargadas.
//
// FORMATO CANONICO (el que usa el archivo de datos de la Fase 4):
//   id|nombre|apellido|email|telefono|activo|matricula
//
// Los 6 primeros campos son los COMUNES con `Profesor`: por eso `Persona`
// los expone con `escribirComunes` / `leerComunes` y esta clase no repite
// el listado.
//
// POR QUE EL `friend`: igual que en `Profesor`, el `operator<<` es una
// funcion libre que necesita leer el estado privado, y el `friend` da el
// acceso minimo sin agrandar la interfaz publica.
// ---------------------------------------------------------------------------
class Alumno : public Persona {
public:
    // --- Regla de Cinco ---
    Alumno() = default;
    Alumno(const Alumno&) = default;
    Alumno& operator=(const Alumno&) = default;
    Alumno(Alumno&&) noexcept = default;
    Alumno& operator=(Alumno&&) noexcept = default;
    ~Alumno() override = default;

    // Construye un alumno validando TODOS los datos a traves de los setters
    // (que lanzan `DatoInvalidoException`).
    Alumno(int id_,
           const std::string& nombre_,
           const std::string& apellido_,
           const std::string& email_,
           const std::string& telefono_,
           const std::string& matricula_);

    std::string getTipo() const override;

    const std::string& getMatricula() const { return matricula; }

    void setMatricula(const std::string& v);

    bool esValido() const override;

    // Escribe el registro canonico en el sumidero indicado.
    friend std::ostream& operator<<(std::ostream& os, const Alumno& alumno);

    // Parsea un registro canonico. Lanza `DatoInvalidoException` si la linea
    // esta mal formada o si tiene una cantidad de campos incorrecta.
    static Alumno fromRegistro(const std::string& linea);

    // Nombres de los campos del formato canonico, separados por '|'.
    static std::string esquema();

    // --- Limites de los campos, con nombre ---
    static constexpr std::size_t MATRICULA_LONGITUD_MINIMA = 4;
    static constexpr std::size_t MATRICULA_LONGITUD_MAXIMA = 12;

private:
    static constexpr std::size_t CANTIDAD_CAMPOS = 7;

    static bool matriculaValido(const std::string& v);

    std::string matricula;
};
