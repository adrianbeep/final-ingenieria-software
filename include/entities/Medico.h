#pragma once

#include "domain/Especialidad.h"
#include "entities/Persona.h"

#include <cstddef>
#include <ostream>
#include <string>

// ---------------------------------------------------------------------------
// Medico: una Persona que attends consultas de una especialidad.
//
// `legajo` es UNICO en el sistema: la unicidad la verifica
// `RepositorioMedicos`, que es quien tiene el conjunto de las entidades
// cargadas.
//
// FORMATO CANONICO (el que usa el archivo de datos de la Fase 4):
//   id|nombre|apellido|email|telefono|activo|legajo|especialidad
//
// EL FORMATO VIVE EN LA ENTIDAD: `operator<<` lo escribe, `fromRegistro` lo
// parsea y `esquema()` devuelve los nombres de los campos. Es el contrato
// con la Fase 4, y la entidad es quien lo conoce.
//
// LA ESPECIALIDAD ES UN `enum class`, NO UN TEXTO
//
// La especialidad del medico sale del CATALOGO (`domain/Especialidad.h`),
// asi que no es un texto libre: son los doce valores del catalogo. Por eso
// no hay `especialidadValido` ni limites de longitud: un valor del enum es
// SIEMPRE valido, y lo unico que hay que validar es que el CODIGO que
// llega del archivo exista en el catalogo (eso lo hace
// `especialidadDesdeCodigo`).
//
// POR QUE EL `friend`: `operator<<` es una funcion LIBRE (no un miembro de
// la entidad) y necesita leer el estado PRIVADO. Las alternativas son (a)
// exponer getters para cada campo, que ensancha la interfaz publica para
// siempre, o (b) hacerlo miembro, con lo que `operator<<` deja de ser una
// funcion libre y el ADL deja de funcionar. El `friend` da el acceso minimo
// sin agrandar la interfaz publica.
// ---------------------------------------------------------------------------
class Medico : public Persona {
public:
    // --- Regla de Cinco ---
    Medico() = default;
    Medico(const Medico&) = default;
    Medico& operator=(const Medico&) = default;
    Medico(Medico&&) noexcept = default;
    Medico& operator=(Medico&&) noexcept = default;
    ~Medico() override = default;

    // Construye un medico validando TODOS los datos a traves de los
    // setters (que lanzan `DatoInvalidoException`): no se puede construir
    // un medico invalido.
    Medico(int id_,
           const std::string& nombre_,
           const std::string& apellido_,
           const std::string& email_,
           const std::string& telefono_,
           const std::string& legajo_,
           const Especialidad& especialidad_);

    std::string getTipo() const override;

    const std::string& getLegajo() const { return legajo; }
    Especialidad getEspecialidad() const { return especialidad; }

    void setLegajo(const std::string& v);
    void setEspecialidad(const Especialidad& v);

    bool esValido() const override;

    // Escribe el registro canonico en el sumidero indicado.
    friend std::ostream& operator<<(std::ostream& os, const Medico& medico);

    // Parsea un registro canonico. Lanza `DatoInvalidoException` si la linea
    // esta mal formada o si tiene una cantidad de campos incorrecta.
    static Medico fromRegistro(const std::string& linea);

    // Nombres de los campos del formato canonico, separados por '|'.
    static std::string esquema();

    // --- Limites de los campos, con nombre ---
    static constexpr std::size_t LEGAJO_LONGITUD_MINIMA = 4;
    static constexpr std::size_t LEGAJO_LONGITUD_MAXIMA = 12;

private:
    static constexpr std::size_t CANTIDAD_CAMPOS = 8;

    static bool legajoValido(const std::string& v);

    std::string legajo;
    Especialidad especialidad = Especialidad::CARD;
};
