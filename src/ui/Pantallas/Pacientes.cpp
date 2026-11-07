#include "ui/Pantallas/Pacientes.h"

#include "domain/Especialidad.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "services/ServicioTurnos.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// La configuracion del CRUD de PACIENTES.
//
// ACA VIVE TODO LO QUE ES ESPECIFICO DE LOS PACIENTES
//
// Las columnas, los campos del formulario, los validadores, el constructor
// de la entidad y los criterios de busqueda. El motor (`ui::CrudPersonas`)
// es generico: no sabe que existe una entidad que se llame "paciente".
//
// EL VOCABULARIO VISIBLE TAMBIEN ES CONFIGURACION
//
// El `singular`, el `plural` y las cabeceras son el TEXTO que ve el usuario,
// y por eso tambien viven aca. El motor los lee y arma con ellos el titulo
// de la pantalla ("PACIENTES"), el titulo del submenu de busqueda ("Buscar
// pacientes"), los mensajes de exito, la pregunta de la baja y el estado
// vacio ("No hay pacientes para mostrar.").
//
// Por eso el rotulo de un campo y el nombre de su columna viajan JUNTOS:
// el motor busca el valor inicial del formulario por el nombre de la
// columna. Si el rotulo no calzara con el nombre de la columna, el dni
// apareceria VACIO al modificar un paciente.
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {
namespace {

// LA FILA DE UN PACIENTE
//
// Son las celdas, en el orden de las cabeceras: `id`, `nombre`, `apellido`,
// `email`, `telefono`, `dni` y `estado`.
std::vector<std::string> filaDe(const Paciente& paciente) {
    return {std::to_string(paciente.getId()),
            paciente.getNombre(),
            paciente.getApellido(),
            paciente.getEmail(),
            paciente.getTelefono(),
            paciente.getDni(),
            paciente.esActivo() ? "activo" : "inactivo"};
}

// LA FILA DE UN TURNO
//
// Es la fila de la ficha: los turnos del paciente con su estado, para que se
// vea cuales estan vigentes y cuales son parte del historial.
std::vector<std::string> filaDeTurno(const Turno& turno) {
    return {std::to_string(turno.getId()),
            turno.getFecha().aTexto(),
            turno.getHora().aTexto(),
            std::to_string(turno.getDuracionMin()),
            turno.getConsultorio(),
            descripcionDe(turno.getEspecialidad()),
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
Campo::Validador validadorDelCampo(std::function<void(Paciente&, const std::string&)> aplicar) {
    return [aplicar](const std::string& texto) -> std::string {
        // Se usa un paciente de prueba: lo que se valida es el DATO, no el
        // paciente (aun asi, el setter recorta el texto, que es lo que se
        // guarda).
        Paciente paciente;
        try {
            aplicar(paciente, texto);
        } catch (const DatoInvalidoException& error) {
            return error.motivo();
        }
        return "";
    };
}

} // namespace

CrudPersonas<Paciente>::Config configDePacientes(RepositorioPacientes& repositorio,
                                              ServicioTurnos& servicioTurnos) {
    CrudPersonas<Paciente>::Config config;

    config.singular = "paciente";
    config.plural = "pacientes";

    config.cabeceras = {"id", "nombre", "apellido", "email", "telefono", "dni", "estado"};
    config.fila = [](const Paciente& paciente) { return filaDe(paciente); };

    // EL FORMULARIO
    //
    // El nombre de cada campo es el nombre de su columna: asi el formulario
    // de modificar se carga con los valores de la entidad que se edita.
    config.campos = {
        {"nombre", "", validadorDelCampo([](Paciente& paciente, const std::string& texto) {
             paciente.setNombre(texto);
         })},
        {"apellido", "", validadorDelCampo([](Paciente& paciente, const std::string& texto) {
             paciente.setApellido(texto);
         })},
        {"email", "", validadorDelCampo([](Paciente& paciente, const std::string& texto) {
             paciente.setEmail(texto);
         })},
        {"telefono", "", validadorDelCampo([](Paciente& paciente, const std::string& texto) {
             paciente.setTelefono(texto);
         })},
        {"dni", "", validadorDelCampo([](Paciente& paciente, const std::string& texto) {
             paciente.setDni(texto);
         })}};

    // EL CONSTRUCTOR DE LA ENTIDAD
    //
    // El vector que llega es [id, nombre, apellido, email, telefono,
    // dni]: el id va adelante porque lo PROPONE el repositorio (o es el
    // id de la entidad que se esta modificando) y nunca lo escribe el
    // usuario.
    config.construir = [](const std::vector<std::string>& valores) {
        return Paciente(std::stoi(valores[0]), valores[1], valores[2], valores[3], valores[4],
                      valores[5]);
    };

    config.esActivo = [](const Paciente& paciente) { return paciente.esActivo(); };

    // LOS CRITERIOS DE BUSQUEDA
    //
    // Cada criterio devuelve las FILAS de las coincidencias: las mete en la
    // tabla de resultados sin volver a preguntar nada.
    //
    // LAS REGLAS SON LAS DEL REPOSITORIO
    //
    // La pantalla NO tiene reglas de busqueda propias: las pregunta al
    // repositorio, que es quien tiene el conjunto de los pacientes. El "nombre
    // o apellido" usa `buscarPorNombre` (coincidencia PARCIAL e insensible a
    // mayusculas) y el "dni" usa `existeDni` (coincidencia EXACTA
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
             Paciente paciente;
             if (repositorio.buscarPorId(id, paciente)) {
                 filas.push_back(filaDe(paciente));
             }
             return filas;
         }},
        {"nombre o apellido", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             for (const Paciente& paciente : repositorio.buscarPorNombre(valor)) {
                 filas.push_back(filaDe(paciente));
             }
             return filas;
         }},
        {"dni", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             // El repositorio devuelve el id del paciente que tiene el
             // dni: con ese id se arma la fila.
             int id = -1;
             if (!repositorio.existeDni(valor, id)) {
                 return filas;
             }
             Paciente paciente;
             if (repositorio.buscarPorId(id, paciente)) {
                 filas.push_back(filaDe(paciente));
             }
             return filas;
         }}};

    // EL ROTULO DE LA FICHA
    //
    // El motor arma la opcion 6 del menu con este texto: cada pantalla dice
    // lo que su ficha muestra.
    config.opcionFicha = "Ver ficha + historial";

    // LOS TURNOS DE LA FICHA
    //
    // Los VIGENTES son los que espera la persona y el HISTORIAL son todos
    // (los cancelados y los reprogramados tambien son parte de su historia).
    config.turnos.cabeceras = {"id", "fecha", "hora", "duracion", "consultorio", "especialidad", "estado"};
    config.turnos.vigentes = [&servicioTurnos](int id) {
        std::vector<std::vector<std::string>> filas;
        for (const Turno& turno : servicioTurnos.turnosVigentesDe(id)) {
            filas.push_back(filaDeTurno(turno));
        }
        return filas;
    };
    config.turnos.historial = [&servicioTurnos](int id) {
        std::vector<std::vector<std::string>> filas;
        for (const Turno& turno : servicioTurnos.turnosDePaciente(id)) {
            filas.push_back(filaDeTurno(turno));
        }
        return filas;
    };

    return config;
}

std::unique_ptr<Pantalla> crearPantallaPacientes(Aplicacion& aplicacion,
                                                RepositorioPacientes& repositorio,
                                                ServicioTurnos& servicioTurnos) {
    // EL MISMO OBJETO CUMPLE LOS DOS PAPELES DEL MOTOR
    //
    // El motor CRUD necesita el CRUD (`IRepositorio<Paciente>`) y la baja logica
    // (`IRepositorioBajaLogica<Paciente>`). El repositorio concreto implementa
    // las dos interfaces, asi que se pasa UNA sola vez: el motor las recibe
    // por parametro y las verifica con la referencia al objeto concreto.
    return std::make_unique<CrudPersonas<Paciente>>(
        aplicacion, repositorio, repositorio, configDePacientes(repositorio, servicioTurnos));
}

} // namespace Pantallas
} // namespace ui
