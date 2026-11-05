#include "ui/Pantallas/Alumnos.h"

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
// La configuracion del CRUD de ALUMNOS.
//
// ACA VIVE TODO LO QUE ES ESPECIFICO DE LOS ALUMNOS
//
// Las columnas, los campos del formulario, los validadores, el constructor
// de la entidad y los criterios de busqueda. El motor (`ui::CrudPersonas`)
// es generico: no sabe que existe una entidad que se llame "alumno".
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {
namespace {

// LA FILA DE UN ALUMNO
//
// Son las celdas, en el orden de las cabeceras: `id`, `nombre`, `apellido`,
// `email`, `telefono`, `matricula` y `estado`.
std::vector<std::string> filaDe(const Alumno& alumno) {
    return {std::to_string(alumno.getId()),
            alumno.getNombre(),
            alumno.getApellido(),
            alumno.getEmail(),
            alumno.getTelefono(),
            alumno.getMatricula(),
            alumno.esActivo() ? "activo" : "inactivo"};
}

// LA FILA DE UN TURNO
//
// Es la fila de la ficha: los turnos del alumno con su estado, para que se
// vea cuales estan vigentes y cuales son parte del historial.
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
// que lanza `DatoInvalidoException` con el motivo del rechazo. El validador
// del campo lo captura y devuelve ese motivo (o una cadena vacia si el valor
// sirve). Si la pantalla copiara la regla, habria DOS reglas que se pueden
// desincronizar: la del dominio y la de la interfaz.
Campo::Validador validadorDelCampo(std::function<void(Alumno&, const std::string&)> aplicar) {
    return [aplicar](const std::string& texto) -> std::string {
        // Se usa un alumno de prueba: lo que se valida es el DATO, no el
        // alumno (aun asi, el setter recorta el texto, que es lo que se
        // guarda).
        Alumno alumno;
        try {
            aplicar(alumno, texto);
        } catch (const DatoInvalidoException& error) {
            return error.motivo();
        }
        return "";
    };
}

} // namespace

CrudPersonas<Alumno>::Config configDeAlumnos(RepositorioAlumnos& repositorio,
                                              ServicioTurnos& servicioTurnos) {
    CrudPersonas<Alumno>::Config config;

    config.singular = "alumno";
    config.plural = "alumnos";

    config.cabeceras = {"id", "nombre", "apellido", "email", "telefono", "matricula", "estado"};
    config.fila = [](const Alumno& alumno) { return filaDe(alumno); };

    // EL FORMULARIO
    //
    // El nombre de cada campo es el nombre de su columna: asi el formulario
    // de modificar se carga con los valores de la entidad que se edita.
    config.campos = {
        {"nombre", "", validadorDelCampo([](Alumno& alumno, const std::string& texto) {
             alumno.setNombre(texto);
         })},
        {"apellido", "", validadorDelCampo([](Alumno& alumno, const std::string& texto) {
             alumno.setApellido(texto);
         })},
        {"email", "", validadorDelCampo([](Alumno& alumno, const std::string& texto) {
             alumno.setEmail(texto);
         })},
        {"telefono", "", validadorDelCampo([](Alumno& alumno, const std::string& texto) {
             alumno.setTelefono(texto);
         })},
        {"matricula", "", validadorDelCampo([](Alumno& alumno, const std::string& texto) {
             alumno.setMatricula(texto);
         })}};

    // EL CONSTRUCTOR DE LA ENTIDAD
    //
    // El vector que llega es [id, nombre, apellido, email, telefono,
    // matricula]: el id va adelante porque lo PROPONE el repositorio (o es el
    // id de la entidad que se esta modificando) y nunca lo escribe el
    // usuario.
    config.construir = [](const std::vector<std::string>& valores) {
        return Alumno(std::stoi(valores[0]), valores[1], valores[2], valores[3], valores[4],
                      valores[5]);
    };

    config.esActivo = [](const Alumno& alumno) { return alumno.esActivo(); };

    // LOS CRITERIOS DE BUSQUEDA
    //
    // Cada criterio devuelve las FILAS de las coincidencias: las mete en la
    // tabla de resultados sin volver a preguntar nada.
    //
    // LAS REGLAS SON LAS DEL REPOSITORIO
    //
    // La pantalla NO tiene reglas de busqueda propias: las pregunta al
    // repositorio, que es quien tiene el conjunto de los alumnos. El "nombre
    // o apellido" usa `buscarPorNombre` (coincidencia PARCIAL e insensible a
    // mayusculas) y la "matricula" usa `existeMatricula` (coincidencia EXACTA
    // e insensible a mayusculas, la misma con la que se verifica su
    // unicidad). Si la pantalla copiara la regla, habria DOS reglas que se
    // pueden desincronizar.
    config.criterios = {
        {"id", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             const int id = idDesdeTexto(valor);
             if (id < 0) {
                 return filas;
             }
             Alumno alumno;
             if (repositorio.buscarPorId(id, alumno)) {
                 filas.push_back(filaDe(alumno));
             }
             return filas;
         }},
        {"nombre o apellido", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             for (const Alumno& alumno : repositorio.buscarPorNombre(valor)) {
                 filas.push_back(filaDe(alumno));
             }
             return filas;
         }},
        {"matricula", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             // El repositorio devuelve el id del alumno que tiene la
             // matricula: con ese id se arma la fila.
             int id = -1;
             if (!repositorio.existeMatricula(valor, id)) {
                 return filas;
             }
             Alumno alumno;
             if (repositorio.buscarPorId(id, alumno)) {
                 filas.push_back(filaDe(alumno));
             }
             return filas;
         }}};

    // LOS TURNOS DE LA FICHA
    //
    // Los VIGENTES son los que espera la persona y el HISTORIAL son todos
    // (los cancelados y los reprogramados tambien son parte de su historia).
    config.turnos.cabeceras = {"id", "fecha", "hora", "duracion", "aula", "materia", "estado"};
    config.turnos.vigentes = [&servicioTurnos](int id) {
        std::vector<std::vector<std::string>> filas;
        for (const Turno& turno : servicioTurnos.turnosVigentesDe(id)) {
            filas.push_back(filaDeTurno(turno));
        }
        return filas;
    };
    config.turnos.historial = [&servicioTurnos](int id) {
        std::vector<std::vector<std::string>> filas;
        for (const Turno& turno : servicioTurnos.turnosDeAlumno(id)) {
            filas.push_back(filaDeTurno(turno));
        }
        return filas;
    };

    return config;
}

std::unique_ptr<Pantalla> crearPantallaAlumnos(Aplicacion& aplicacion,
                                                RepositorioAlumnos& repositorio,
                                                ServicioTurnos& servicioTurnos) {
    // EL MISMO OBJETO CUMPLE LOS DOS PAPELES DEL MOTOR
    //
    // El motor CRUD necesita el CRUD (`IRepositorio<Alumno>`) y la baja logica
    // (`IRepositorioBajaLogica<Alumno>`). El repositorio concreto implementa
    // las dos interfaces, asi que se pasa UNA sola vez: el motor las recibe
    // por parametro y las verifica con la referencia al objeto concreto.
    return std::make_unique<CrudPersonas<Alumno>>(
        aplicacion, repositorio, repositorio, configDeAlumnos(repositorio, servicioTurnos));
}

} // namespace Pantallas
} // namespace ui
