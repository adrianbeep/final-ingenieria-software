#include "ui/Pantallas/Pacientes.h"

#include "domain/Especialidad.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "services/ServicioTurnos.h"
#include "ui/CriteriosDePersonas.h"

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

    config.cabeceras = cabecerasDePacientes();
    config.fila = [](const Paciente& paciente) { return filaDePaciente(paciente); };

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
    // Cada criterio devuelve las COINCIDENCIAS: el id de la entidad y sus
    // celdas. La pantalla "Buscar" muestra las celdas (en una tabla) y el
    // buscador usa el id (para elegir la entidad).
    //
    // LOS CRITERIOS SON COMPARTIDOS
    //
    // Viven en `ui::CriteriosDePersonas.h`, no aca: los usan tambien las
    // pantallas que buscan a un paciente para otra cosa (la de turnos al
    // agendar y la de reportes). Si cada una escribiera los suyos, buscar un
    // paciente se comportaria de una manera en cada pantalla.
    config.criterios = criteriosDePacientes(repositorio);

    // EL BUSCADOR DE PACIENTES
    //
    // Las columnas que muestra el buscador son las que AYUDAN a reconocer a
    // la persona: el apellido y el nombre (que es como se la llama) y el dni
    // (que es lo que la distingue de los homonimos). El id va primero: es lo
    // que el buscador devuelve.
    config.columnasBuscador = columnasDelBuscadorDePacientes();
    config.pistaBuscador = pistaDelBuscadorDePacientes();

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
