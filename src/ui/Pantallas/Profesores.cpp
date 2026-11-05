#include "ui/Pantallas/Profesores.h"

#include "domain/Materia.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "services/ServicioTurnos.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// La configuracion del CRUD de PROFESORES.
//
// Es el espejo de la de alumnos (`src/ui/Pantallas/Alumnos.cpp`): el motor es
// el mismo, asi que lo que cambia son los DATOS (las columnas, los campos,
// el constructor de la entidad y los criterios de busqueda).
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {
namespace {

// LA FILA DE UN PROFESOR
//
// Son las celdas, en el orden de las cabeceras: `id`, `nombre`, `apellido`,
// `email`, `telefono`, `legajo`, `especialidad` y `estado`.
std::vector<std::string> filaDe(const Profesor& profesor) {
    return {std::to_string(profesor.getId()),
            profesor.getNombre(),
            profesor.getApellido(),
            profesor.getEmail(),
            profesor.getTelefono(),
            profesor.getLegajo(),
            profesor.getEspecialidad(),
            profesor.esActivo() ? "activo" : "inactivo"};
}

// LA FILA DE UN TURNO
//
// Es la fila de la ficha: los turnos del profesor con su estado.
std::vector<std::string> filaDeTurno(const Turno& turno) {
    return {std::to_string(turno.getId()),
            turno.getFecha().aTexto(),
            turno.getHora().aTexto(),
            std::to_string(turno.getDuracionMin()),
            turno.getAula(),
            descripcionDe(turno.getMateria()),
            estadoATexto(turno.getEstado())};
}

// EL VALIDADOR DE UN CAMPO DEL FORMULARIO
//
// EL VALIDADOR ES EL SETTER DE LA ENTIDAD
//
// La regla de cada campo vive en el setter (`setNombre`, `setEmail`, ...),
// que lanza `DatoInvalidoException` con el motivo del rechazo.
Campo::Validador validadorDelCampo(std::function<void(Profesor&, const std::string&)> aplicar) {
    return [aplicar](const std::string& texto) -> std::string {
        Profesor profesor;
        try {
            aplicar(profesor, texto);
        } catch (const DatoInvalidoException& error) {
            return error.motivo();
        }
        return "";
    };
}

} // namespace

CrudPersonas<Profesor>::Config configDeProfesores(RepositorioProfesores& repositorio,
                                                   ServicioTurnos& servicioTurnos) {
    CrudPersonas<Profesor>::Config config;

    config.singular = "profesor";
    config.plural = "profesores";

    config.cabeceras = {"id",  "nombre",      "apellido", "email",
                        "telefono", "legajo", "especialidad", "estado"};
    config.fila = [](const Profesor& profesor) { return filaDe(profesor); };

    // EL FORMULARIO
    //
    // El nombre de cada campo es el nombre de su columna: asi el formulario
    // de modificar se carga con los valores de la entidad que se edita.
    config.campos = {
        {"nombre", "", validadorDelCampo([](Profesor& profesor, const std::string& texto) {
             profesor.setNombre(texto);
         })},
        {"apellido", "", validadorDelCampo([](Profesor& profesor, const std::string& texto) {
             profesor.setApellido(texto);
         })},
        {"email", "", validadorDelCampo([](Profesor& profesor, const std::string& texto) {
             profesor.setEmail(texto);
         })},
        {"telefono", "", validadorDelCampo([](Profesor& profesor, const std::string& texto) {
             profesor.setTelefono(texto);
         })},
        {"legajo", "", validadorDelCampo([](Profesor& profesor, const std::string& texto) {
             profesor.setLegajo(texto);
         })},
        {"especialidad", "", validadorDelCampo([](Profesor& profesor, const std::string& texto) {
             profesor.setEspecialidad(texto);
         })}};

    // EL CONSTRUCTOR DE LA ENTIDAD
    //
    // El vector que llega es [id, nombre, apellido, email, telefono, legajo,
    // especialidad]: el id va adelante porque lo PROPONE el repositorio (o es
    // el id de la entidad que se esta modificando) y nunca lo escribe el
    // usuario.
    config.construir = [](const std::vector<std::string>& valores) {
        return Profesor(std::stoi(valores[0]), valores[1], valores[2], valores[3], valores[4],
                        valores[5], valores[6]);
    };

    config.esActivo = [](const Profesor& profesor) { return profesor.esActivo(); };

    // LOS CRITERIOS DE BUSQUEDA
    //
    // LAS REGLAS SON LAS DEL REPOSITORIO
    //
    // La pantalla NO tiene reglas de busqueda propias: las pregunta al
    // repositorio, que es quien tiene el conjunto de los profesores. El
    // "nombre o apellido" usa `buscarPorNombre` (coincidencia PARCIAL e
    // insensible a mayusculas) y el "legajo" usa `existeLegajo` (coincidencia
    // EXACTA e insensible a mayusculas, la misma con la que se verifica su
    // unicidad).
    config.criterios = {
        {"id", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             const int id = idDesdeTexto(valor);
             if (id < 0) {
                 return filas;
             }
             Profesor profesor;
             if (repositorio.buscarPorId(id, profesor)) {
                 filas.push_back(filaDe(profesor));
             }
             return filas;
         }},
        {"nombre o apellido", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             for (const Profesor& profesor : repositorio.buscarPorNombre(valor)) {
                 filas.push_back(filaDe(profesor));
             }
             return filas;
         }},
        {"legajo", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             // El repositorio devuelve el id del profesor que tiene el legajo:
             // con ese id se arma la fila.
             int id = -1;
             if (!repositorio.existeLegajo(valor, id)) {
                 return filas;
             }
             Profesor profesor;
             if (repositorio.buscarPorId(id, profesor)) {
                 filas.push_back(filaDe(profesor));
             }
             return filas;
         }}};

    // LOS TURNOS DE LA FICHA
    //
    // Los VIGENTES son los que el profesor tiene por delante y el HISTORIAL
    // son todos (los cancelados y los reprogramados tambien son parte de su
    // historia).
    //
    // El servicio no tiene una consulta de "turnos vigentes de un profesor"
    // (la que usa es la de los alumnos): los vigentes se sacan del historial
    // filtrando por estado.
    config.turnos.cabeceras = {"id", "fecha", "hora", "duracion", "aula", "materia", "estado"};
    config.turnos.vigentes = [&servicioTurnos](int id) {
        std::vector<std::vector<std::string>> filas;
        for (const Turno& turno : servicioTurnos.turnosDeProfesor(id)) {
            if (turno.getEstado() == EstadoTurno::Activo) {
                filas.push_back(filaDeTurno(turno));
            }
        }
        return filas;
    };
    config.turnos.historial = [&servicioTurnos](int id) {
        std::vector<std::vector<std::string>> filas;
        for (const Turno& turno : servicioTurnos.turnosDeProfesor(id)) {
            filas.push_back(filaDeTurno(turno));
        }
        return filas;
    };

    return config;
}

std::unique_ptr<Pantalla> crearPantallaProfesores(Aplicacion& aplicacion,
                                                    RepositorioProfesores& repositorio,
                                                    ServicioTurnos& servicioTurnos) {
    // EL MISMO OBJETO CUMPLE LOS DOS PAPELES DEL MOTOR
    //
    // El motor CRUD necesita el CRUD (`IRepositorio<Profesor>`) y la baja
    // logica (`IRepositorioBajaLogica<Profesor>`). El repositorio concreto
    // implementa las dos interfaces, asi que se pasa UNA sola vez.
    return std::make_unique<CrudPersonas<Profesor>>(
        aplicacion, repositorio, repositorio, configDeProfesores(repositorio, servicioTurnos));
}

} // namespace Pantallas
} // namespace ui
