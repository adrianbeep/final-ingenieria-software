#pragma once

#include "entities/Persona.h"

#include <cstddef>
#include <ostream>
#include <string>

// ---------------------------------------------------------------------------
// Paciente: una Persona que atiende consultas medicas.
//
// `dni` es UNICA en el sistema: la unicidad la verifica
// `RepositorioPacientes`, que es quien tiene el conjunto de las entidades
// cargadas.
//
// FORMATO CANONICO (el que usa el archivo de datos de la Fase 4):
//   id|nombre|apellido|email|telefono|activo|dni
//
// Los 6 primeros campos son los COMUNES con `Medico`: por eso `Persona`
// los expone con `escribirComunes` / `leerComunes` y esta clase no repite
// el listado.
//
// POR QUE EL `friend`: igual que en `Medico`, el `operator<<` es una
// funcion libre que necesita leer el estado privado, y el `friend` da el
// acceso minimo sin agrandar la interfaz publica.
// ---------------------------------------------------------------------------
class Paciente : public Persona {
public:
    // --- Regla de Cinco ---
    Paciente() = default;
    Paciente(const Paciente&) = default;
    Paciente& operator=(const Paciente&) = default;
    Paciente(Paciente&&) noexcept = default;
    Paciente& operator=(Paciente&&) noexcept = default;
    ~Paciente() override = default;

    // Construye un paciente validando TODOS los datos a traves de los setters
    // (que lanzan `DatoInvalidoException`).
    Paciente(int id_,
             const std::string& nombre_,
             const std::string& apellido_,
             const std::string& email_,
             const std::string& telefono_,
             const std::string& dni_);

    std::string getTipo() const override;

    const std::string& getDni() const { return dni; }

    void setDni(const std::string& v);

    bool esValido() const override;

    // Escribe el registro canonico en el sumidero indicado.
    friend std::ostream& operator<<(std::ostream& os, const Paciente& paciente);

    // Parsea un registro canonico. Lanza `DatoInvalidoException` si la linea
    // esta mal formada o si tiene una cantidad de campos incorrecta.
    static Paciente fromRegistro(const std::string& linea);

    // Nombres de los campos del formato canonico, separados por '|'.
    static std::string esquema();

    // --- Limites de los campos, con nombre ---
    //
    // EL DNI SON 7 U 8 DIGITOS
    //
    // No es un texto libre: es un numero. Por eso la regla es "todos
    // digitos" y no "alfanumerico": un DNI con letras no es un DNI.
    static constexpr std::size_t DNI_LONGITUD_MINIMA = 7;
    static constexpr std::size_t DNI_LONGITUD_MAXIMA = 8;

private:
    static constexpr std::size_t CANTIDAD_CAMPOS = 7;

    static bool dniValido(const std::string& v);

    std::string dni;
};
