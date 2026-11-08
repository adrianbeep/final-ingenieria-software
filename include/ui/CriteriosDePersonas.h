#pragma once

#include "domain/Especialidad.h"
#include "entities/Medico.h"
#include "entities/Paciente.h"
#include "repositories/RepositorioMedicos.h"
#include "repositories/RepositorioPacientes.h"
#include "ui/CriterioBusqueda.h"
#include "ui/IdDesdeTexto.h"

#include <cstddef>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// Las reglas de las PERSONAS que usan MAS DE UNA pantalla.
//
// QUE HAY ACA
//
//   - la FILA de un paciente y de un medico (las celdas, en el orden de sus
//     columnas), con las CABECERAS que las nombran;
//   - las COLUMNAS y la PISTA del buscador de cada uno;
//   - los CRITERIOS de busqueda (por id, por nombre o apellido, y por dni o
//     legajo);
//   - el BUSCADOR de cada uno: las coincidencias que muestra.
//
// POR QUE VIVEN ACA Y NO EN CADA PANTALLA
//
// Las cuatro pantallas que buscan a una persona (las de pacientes, las de
// medicos, la de turnos al agendar y la de reportes) tienen que buscar SIEMPRE
// lo mismo: si cada una escribiera su propia regla, "Gomez" encontraria una
// persona en una pantalla y otra en otra, y el usuario tendria que aprender
// cuatro busquedas distintas.
//
// ES EL MISMO CRITERIO QUE EL `ui::NombresDePersonas.h`
//
// Ese header existe para que la resolucion de los nombres de las personas no
// se escriba dos veces. Este es el hermano: la resolucion de los nombres y la
// busqueda de las personas son las dos reglas que mas de una pantalla
// necesita.
//
// POR QUE SON `inline` Y NO UN .CPP
//
// Son funciones que dependen de las clases concretas de las personas (que son
// las que saben buscar por nombre, por dni y por legajo). Si vivieran en un
// .cpp, el header tendria que declarar esas clases igual, asi que no se gana
// nada: se dejan aca.
// ---------------------------------------------------------------------------
namespace ui {

// ---------------------------------------------------------------------------
// LOS PACIENTES
// ---------------------------------------------------------------------------

// Las columnas de la fila de un paciente.
//
// LA FILA Y SUS COLUMNAS VAN JUNTAS
//
// La fila son las celdas en el ORDEN de estas columnas: si una cambiara y la
// otra no, la proyeccion del buscador leeria una celda de otro lado.
inline const std::vector<std::string>& cabecerasDePacientes() {
    static const std::vector<std::string> cabeceras = {"id", "nombre", "apellido", "email",
                                                        "telefono", "dni", "estado"};
    return cabeceras;
}

// LA FILA DE UN PACIENTE
//
// Son las celdas, en el orden de las cabeceras: `id`, `nombre`, `apellido`,
// `email`, `telefono`, `dni` y `estado`.
inline std::vector<std::string> filaDePaciente(const Paciente& paciente) {
    return {std::to_string(paciente.getId()),
            paciente.getNombre(),
            paciente.getApellido(),
            paciente.getEmail(),
            paciente.getTelefono(),
            paciente.getDni(),
            paciente.esActivo() ? "activo" : "inactivo"};
}

// EL BUSCADOR DE PACIENTES
//
// Las columnas que muestra el buscador son las que AYUDAN a reconocer a la
// persona: el apellido y el nombre (que es como se la llama) y el dni (que es
// lo que la distingue de los homonimos). El id va primero: es lo que el
// buscador devuelve.
inline const std::vector<std::string>& columnasDelBuscadorDePacientes() {
    static const std::vector<std::string> columnas = {"id", "apellido", "nombre", "dni"};
    return columnas;
}

// La pista del campo del buscador de pacientes.
inline const std::string& pistaDelBuscadorDePacientes() {
    static const std::string pista = "Nombre, apellido o DNI";
    return pista;
}

// ---------------------------------------------------------------------------
// LOS MEDICOS
// ---------------------------------------------------------------------------

// Las columnas de la fila de un medico.
//
// LA FILA Y SUS COLUMNAS VAN JUNTAS
//
// La fila son las celdas en el ORDEN de estas columnas: si una cambiara y la
// otra no, la proyeccion del buscador leeria una celda de otro lado.
inline const std::vector<std::string>& cabecerasDeMedicos() {
    static const std::vector<std::string> cabeceras = {"id", "nombre", "apellido", "email",
                                                        "telefono", "legajo", "especialidad",
                                                        "estado"};
    return cabeceras;
}

// LA FILA DE UN MEDICO
//
// Son las celdas, en el orden de las cabeceras: `id`, `nombre`, `apellido`,
// `email`, `telefono`, `legajo`, `especialidad` y `estado`.
//
// La especialidad se muestra con su DESCRIPCION ("Cardiologia"), que es lo
// que el usuario lee: el CODIGO ("CARD") es el dato que viaja en el archivo.
inline std::vector<std::string> filaDeMedico(const Medico& medico) {
    return {std::to_string(medico.getId()),
            medico.getNombre(),
            medico.getApellido(),
            medico.getEmail(),
            medico.getTelefono(),
            medico.getLegajo(),
            descripcionDe(medico.getEspecialidad()),
            medico.esActivo() ? "activo" : "inactivo"};
}

// EL BUSCADOR DE MEDICOS
//
// Las columnas que muestra el buscador son las que AYUDAN a reconocer al
// medico: el apellido y el nombre, la especialidad (que es lo que distingue a
// dos medicos con el mismo nombre) y el legajo (que es lo que lo identifica).
// El id va primero: es lo que el buscador devuelve.
inline const std::vector<std::string>& columnasDelBuscadorDeMedicos() {
    static const std::vector<std::string> columnas = {"id", "apellido", "nombre", "especialidad",
                                                        "legajo"};
    return columnas;
}

// La pista del campo del buscador de medicos.
inline const std::string& pistaDelBuscadorDeMedicos() {
    static const std::string pista = "Nombre, apellido, legajo o especialidad";
    return pista;
}

// ---------------------------------------------------------------------------
// LOS CRITERIOS DE BUSQUEDA
//
// LOS CRITERIOS SON LOS DE LA CONFIGURACION DE LAS PANTALLAS
//
// Cada criterio devuelve las COINCIDENCIAS: el id de la entidad y sus
// celdas. La pantalla "Buscar" muestra las celdas (en una tabla) y el
// buscador usa el id (para elegir la entidad).
//
// LAS REGLAS SON LAS DEL REPOSITORIO
//
// La pantalla NO tiene reglas de busqueda propias: las pregunta al
// repositorio, que es quien tiene el conjunto de las personas. El "nombre o
// apellido" usa `buscarPorNombre` (coincidencia PARCIAL e insensible a
// mayusculas), el "dni" usa `existeDni` y el "legajo" usa `existeLegajo`
// (coincidencia EXACTA e insensible a mayusculas, la misma con la que se
// verifica su unicidad). Si la pantalla copiara la regla, habria DOS reglas
// que se pueden desincronizar.
// ---------------------------------------------------------------------------

// Los criterios de busqueda de los PACIENTES.
inline std::vector<CriterioBusqueda> criteriosDePacientes(RepositorioPacientes& repositorio) {
    return {
        {"id", [&repositorio](const std::string& valor) {
             std::vector<Coincidencia> coincidencias;
             const int id = idDesdeTexto(valor);
             if (id < 0) {
                 return coincidencias;
             }
             Paciente paciente;
             if (repositorio.buscarPorId(id, paciente)) {
                 coincidencias.push_back({paciente.getId(), filaDePaciente(paciente)});
             }
             return coincidencias;
         }},
        {"nombre o apellido", [&repositorio](const std::string& valor) {
             std::vector<Coincidencia> coincidencias;
             for (const Paciente& paciente : repositorio.buscarPorNombre(valor)) {
                 coincidencias.push_back({paciente.getId(), filaDePaciente(paciente)});
             }
             return coincidencias;
         }},
        {"dni", [&repositorio](const std::string& valor) {
             std::vector<Coincidencia> coincidencias;
             // El repositorio devuelve el id del paciente que tiene el
             // dni: con ese id se arma la fila.
             int id = -1;
             if (!repositorio.existeDni(valor, id)) {
                 return coincidencias;
             }
             Paciente paciente;
             if (repositorio.buscarPorId(id, paciente)) {
                 coincidencias.push_back({paciente.getId(), filaDePaciente(paciente)});
             }
             return coincidencias;
         }}};
}

// Los criterios de busqueda de los MEDICOS.
inline std::vector<CriterioBusqueda> criteriosDeMedicos(RepositorioMedicos& repositorio) {
    return {
        {"id", [&repositorio](const std::string& valor) {
             std::vector<Coincidencia> coincidencias;
             const int id = idDesdeTexto(valor);
             if (id < 0) {
                 return coincidencias;
             }
             Medico medico;
             if (repositorio.buscarPorId(id, medico)) {
                 coincidencias.push_back({medico.getId(), filaDeMedico(medico)});
             }
             return coincidencias;
         }},
        {"nombre o apellido", [&repositorio](const std::string& valor) {
             std::vector<Coincidencia> coincidencias;
             for (const Medico& medico : repositorio.buscarPorNombre(valor)) {
                 coincidencias.push_back({medico.getId(), filaDeMedico(medico)});
             }
             return coincidencias;
         }},
        {"legajo", [&repositorio](const std::string& valor) {
             std::vector<Coincidencia> coincidencias;
             // El repositorio devuelve el id del medico que tiene el legajo:
             // con ese id se arma la fila.
             int id = -1;
             if (!repositorio.existeLegajo(valor, id)) {
                 return coincidencias;
             }
             Medico medico;
             if (repositorio.buscarPorId(id, medico)) {
                 coincidencias.push_back({medico.getId(), filaDeMedico(medico)});
             }
             return coincidencias;
         }}};
}

// ---------------------------------------------------------------------------
// EL BUSCADOR DE PERSONAS
//
// LAS COINCIDENCIAS SON LAS DEL MOTOR CRUD
//
// Con el texto vacio se listan los ACTIVOS y con texto se corren los
// criterios: es la MISMA regla que usa el motor CRUD (ver
// `ui::CriterioBusqueda.h`), asi que buscar un paciente se comporta igual en
// las cuatro pantallas que lo buscan.
//
// LA PROYECCION LA HACE LA PANTALLA
//
// Las celdas de cada coincidencia son las de las COLUMNAS del buscador, y la
// proyeccion (de la fila completa a las columnas) se hace aca, con las
// columnas que declaran las pantallas de pacientes y de medicos: asi los
// cuatro buscadores muestran EXACTAMENTE las mismas celdas.
// ---------------------------------------------------------------------------

// Los pacientes ACTIVOS, con su fila (es lo que se muestra con el texto
// vacio del buscador).
inline std::vector<Coincidencia> activosDePacientes(RepositorioPacientes& repositorio) {
    std::vector<Coincidencia> activos;
    for (const Paciente& paciente : repositorio.listarActivos()) {
        activos.push_back({paciente.getId(), filaDePaciente(paciente)});
    }
    return activos;
}

// Los medicos ACTIVOS, con su fila (es lo que se muestra con el texto vacio
// del buscador).
inline std::vector<Coincidencia> activosDeMedicos(RepositorioMedicos& repositorio) {
    std::vector<Coincidencia> activos;
    for (const Medico& medico : repositorio.listarActivos()) {
        activos.push_back({medico.getId(), filaDeMedico(medico)});
    }
    return activos;
}

// Las coincidencias del buscador de pacientes.
inline std::vector<Coincidencia> coincidenciasDelBuscadorDePacientes(
    RepositorioPacientes& repositorio, const std::string& texto) {
    return coincidenciasDelBuscadorDePersonas(
        criteriosDePacientes(repositorio),
        [&repositorio]() { return activosDePacientes(repositorio); },
        [](const std::vector<std::string>& fila) {
            return celdasDeLasColumnas(fila, cabecerasDePacientes(),
                                       columnasDelBuscadorDePacientes());
        },
        texto);
}

// Las coincidencias del buscador de medicos.
inline std::vector<Coincidencia> coincidenciasDelBuscadorDeMedicos(
    RepositorioMedicos& repositorio, const std::string& texto) {
    return coincidenciasDelBuscadorDePersonas(
        criteriosDeMedicos(repositorio),
        [&repositorio]() { return activosDeMedicos(repositorio); },
        [](const std::vector<std::string>& fila) {
            return celdasDeLasColumnas(fila, cabecerasDeMedicos(),
                                       columnasDelBuscadorDeMedicos());
        },
        texto);
}

} // namespace ui
