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
//   id|nombre|apellido|email|telefono|activo|dni|observaciones
//
// `observaciones` es OPCIONAL: es un texto libre donde se anotan cosas del
// paciente (alergias, tratamiento en curso, lo que haga falta). Se valida
// como todos los demas campos, en su setter.
//
// LOS 6 PRIMEROS CAMPOS SON LOS COMUNES con `Medico`: por eso `Persona`
// los expone con `escribirComunes` / `leerComunes` y esta clase no repite
// el listado.
//
// EL FORMATO ANTERIOR
//
// El paciente tambien se puede leer en el formato v2 (el que no tiene
// `observaciones`): por eso `esquemaAnterior()` declara el esquema de esa
// version. Un archivo v2 se lee con `observaciones` VACIA.
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
    //
    // `observaciones_` va AL FINAL y con VALOR POR DEFECTO: asi ningun
    // llamador de los que ya existian se rompe (el campo es opcional).
    Paciente(int id_,
             const std::string& nombre_,
             const std::string& apellido_,
             const std::string& email_,
             const std::string& telefono_,
             const std::string& dni_,
             const std::string& observaciones_ = "");

    std::string getTipo() const override;

    const std::string& getDni() const { return dni; }
    const std::string& getObservaciones() const { return observaciones; }

    void setDni(const std::string& v);
    void setObservaciones(const std::string& v);

    bool esValido() const override;

    // Escribe el registro canonico en el sumidero indicado.
    friend std::ostream& operator<<(std::ostream& os, const Paciente& paciente);

    // Parsea un registro canonico. Lanza `DatoInvalidoException` si la linea
    // esta mal formada o si tiene una cantidad de campos incorrecta.
    //
    // Acepta 7 u 8 campos: con 7 (el formato v2) el campo `observaciones`
    // queda vacio, y con 8 (el formato actual) es el septimo.
    static Paciente fromRegistro(const std::string& linea);

    // Nombres de los campos del formato canonico, separados por '|'.
    static std::string esquema();

    // Nombres de los campos del formato ANTERIOR (la v2), separados por '|'.
    //
    // Es el esquema que el paciente TENIA antes del campo `observaciones`:
    // el loader lo usa para leer los archivos v2 sin rechazarlos.
    static std::string esquemaAnterior();

    // --- Limites de los campos, con nombre ---
    //
    // EL DNI SON 7 U 8 DIGITOS
    //
    // No es un texto libre: es un numero. Por eso la regla es "todos
    // digitos" y no "alfanumerico": un DNI con letras no es un DNI.
    static constexpr std::size_t DNI_LONGITUD_MINIMA = 7;
    static constexpr std::size_t DNI_LONGITUD_MAXIMA = 8;

    // EL LARGO MAXIMO DE LAS OBSERVACIONES
    //
    // Es un texto libre, pero no infinito: 200 caracteres alcanzan para
    // anotar lo que haga falta y el archivo sigue siendo legible.
    static constexpr std::size_t OBSERVACIONES_LONGITUD_MAXIMA = 200;

private:
    // La cantidad de campos del formato ACTUAL (con `observaciones`).
    static constexpr std::size_t CANTIDAD_CAMPOS = 8;

    // La cantidad de campos del formato ANTERIOR (sin `observaciones`).
    static constexpr std::size_t CANTIDAD_CAMPOS_ANTERIOR = 7;

    static bool dniValido(const std::string& v);

    // El motivo por el que las observaciones NO sirven (vacio si sirven).
    //
    // Devuelve el texto y no un `bool` porque hay TRES motivos distintos y
    // cada uno merece su propio mensaje: el setter lo usa para lanzar y
    // `esValido()` para comprobar. Vive en UN solo lugar para que las dos
    // cosas no se puedan desincronizar.
    static std::string motivoDeObservacionesInvalidas(const std::string& v);

    std::string dni;
    std::string observaciones;
};
