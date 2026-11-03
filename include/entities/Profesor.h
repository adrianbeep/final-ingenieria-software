#pragma once

#include "entities/Persona.h"

#include <cstddef>
#include <ostream>
#include <string>

// ---------------------------------------------------------------------------
// Profesor: una Persona que dicta clases.
//
// `legajo` es UNICO en el sistema: la unicidad la verifica
// `RepositorioProfesores`, que es quien tiene el conjunto de las entidades
// cargadas.
//
// FORMATO CANONICO (el que usa el archivo de datos de la Fase 4):
//   id|nombre|apellido|email|telefono|activo|legajo|especialidad
//
// EL FORMATO VIVE EN LA ENTIDAD: `operator<<` lo escribe, `fromRegistro` lo
// parsea y `esquema()` devuelve los nombres de los campos. Es el contrato
// con la Fase 4, y la entidad es quien lo conoce.
//
// POR QUE EL `friend`: `operator<<` es una funcion LIBRE (no un miembro de
// la entidad) y necesita leer el estado PRIVADO. Las alternativas son (a)
// exponer getters para cada campo, que ensancha la interfaz publica para
// siempre, o (b) hacerlo miembro, con lo que `operator<<` deja de ser una
// funcion libre y el ADL deja de funcionar. El `friend` da el acceso minimo
// sin agrandar la interfaz publica.
// ---------------------------------------------------------------------------
class Profesor : public Persona {
public:
    // --- Regla de Cinco ---
    Profesor() = default;
    Profesor(const Profesor&) = default;
    Profesor& operator=(const Profesor&) = default;
    Profesor(Profesor&&) noexcept = default;
    Profesor& operator=(Profesor&&) noexcept = default;
    ~Profesor() override = default;

    // Construye un profesor validando TODOS los datos a traves de los
    // setters (que lanzan `DatoInvalidoException`): no se puede construir
    // un profesor invalido.
    Profesor(int id_,
             const std::string& nombre_,
             const std::string& apellido_,
             const std::string& email_,
             const std::string& telefono_,
             const std::string& legajo_,
             const std::string& especialidad_);

    std::string getTipo() const override;

    const std::string& getLegajo() const { return legajo; }
    const std::string& getEspecialidad() const { return especialidad; }

    void setLegajo(const std::string& v);
    void setEspecialidad(const std::string& v);

    bool esValido() const override;

    // Escribe el registro canonico en el sumidero indicado.
    friend std::ostream& operator<<(std::ostream& os, const Profesor& profesor);

    // Parsea un registro canonico. Lanza `DatoInvalidoException` si la linea
    // esta mal formada o si tiene una cantidad de campos incorrecta.
    static Profesor fromRegistro(const std::string& linea);

    // Nombres de los campos del formato canonico, separados por '|'.
    static std::string esquema();

    // --- Limites de los campos, con nombre ---
    static constexpr std::size_t LEGAJO_LONGITUD_MINIMA = 4;
    static constexpr std::size_t LEGAJO_LONGITUD_MAXIMA = 12;
    static constexpr std::size_t ESPECIALIDAD_LONGITUD_MINIMA = 1;
    static constexpr std::size_t ESPECIALIDAD_LONGITUD_MAXIMA = 60;

private:
    static constexpr std::size_t CANTIDAD_CAMPOS = 8;

    static bool legajoValido(const std::string& v);
    static bool especialidadValido(const std::string& v);

    std::string legajo;
    std::string especialidad;
};
