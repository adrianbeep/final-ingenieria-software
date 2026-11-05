#include "ui/Pantallas/Medicos.h"

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
// La configuracion del CRUD de MEDICOS.
//
// Es el espejo de la de pacientes (`src/ui/Pantallas/Pacientes.cpp`): el motor es
// el mismo, asi que lo que cambia son los DATOS (las columnas, los campos,
// el constructor de la entidad y los criterios de busqueda).
//
// EL VOCABULARIO VISIBLE TAMBIEN ES CONFIGURACION
//
// El `singular` y el `plural` son el TEXTO que ve el usuario, y por eso
// tambien viven aca. El motor los lee y arma con ellos el titulo de la
// pantalla ("MEDICOS"), el titulo del submenu de busqueda ("Buscar medicos"),
// los mensajes de exito, la pregunta de la baja y el estado vacio ("No hay
// medicos para mostrar.").
//
// ---------------------------------------------------------------------------
namespace ui {
namespace Pantallas {
namespace {

// LA FILA DE UN MEDICO
//
// Son las celdas, en el orden de las cabeceras: `id`, `nombre`, `apellido`,
// `email`, `telefono`, `legajo`, `especialidad` y `estado`.
//
// La especialidad se muestra con su DESCRIPCION ("Cardiología"), que es lo
// que el usuario lee: el CODIGO ("CARD") es el dato que viaja en el archivo.
std::vector<std::string> filaDe(const Medico& medico) {
    return {std::to_string(medico.getId()),
            medico.getNombre(),
            medico.getApellido(),
            medico.getEmail(),
            medico.getTelefono(),
            medico.getLegajo(),
            descripcionDe(medico.getEspecialidad()),
            medico.esActivo() ? "activo" : "inactivo"};
}

// LA FILA DE UN TURNO
//
// Es la fila de la ficha: los turnos del medico con su estado.
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
// que lanza `DatoInvalidoException` con el motivo del rechazo.
Campo::Validador validadorDelCampo(std::function<void(Medico&, const std::string&)> aplicar) {
    return [aplicar](const std::string& texto) -> std::string {
        Medico medico;
        try {
            aplicar(medico, texto);
        } catch (const DatoInvalidoException& error) {
            return error.motivo();
        }
        return "";
    };
}

} // namespace

CrudPersonas<Medico>::Config configDeMedicos(RepositorioMedicos& repositorio,
                                                   ServicioTurnos& servicioTurnos) {
    CrudPersonas<Medico>::Config config;

    config.singular = "medico";
    config.plural = "medicos";

    config.cabeceras = {"id",  "nombre",      "apellido", "email",
                        "telefono", "legajo", "especialidad", "estado"};
    config.fila = [](const Medico& medico) { return filaDe(medico); };

    // EL FORMULARIO
    //
    // El nombre de cada campo es el nombre de su columna: asi el formulario
    // de modificar se carga con los valores de la entidad que se edita.
    config.campos = {
        {"nombre", "", validadorDelCampo([](Medico& medico, const std::string& texto) {
             medico.setNombre(texto);
         })},
        {"apellido", "", validadorDelCampo([](Medico& medico, const std::string& texto) {
             medico.setApellido(texto);
         })},
        {"email", "", validadorDelCampo([](Medico& medico, const std::string& texto) {
             medico.setEmail(texto);
         })},
        {"telefono", "", validadorDelCampo([](Medico& medico, const std::string& texto) {
             medico.setTelefono(texto);
         })},
        {"legajo", "", validadorDelCampo([](Medico& medico, const std::string& texto) {
             medico.setLegajo(texto);
         })},
        // LA ESPECIALIDAD SE ESCRIBE CON SU CODIGO
        //
        // El campo es un TEXTO, asi que lo que el usuario escribe es el
        // codigo del catalogo ("CARD"). `especialidadDesdeCodigo` lo
        // traduce al `enum class` y, si el codigo no existe, lanza
        // `DatoInvalidoException`: el validador la captura y muestra el
        // motivo, sin dejar que la pantalla invente una regla propia.
        {"especialidad", "", validadorDelCampo([](Medico& medico, const std::string& texto) {
             medico.setEspecialidad(especialidadDesdeCodigo(texto));
         })}};

    // EL CONSTRUCTOR DE LA ENTIDAD
    //
    // El vector que llega es [id, nombre, apellido, email, telefono, legajo,
    // especialidad]: el id va adelante porque lo PROPONE el repositorio (o es
    // el id de la entidad que se esta modificando) y nunca lo escribe el
    // usuario. La especialidad llega como CODIGO y se traduce al catalogo.
    config.construir = [](const std::vector<std::string>& valores) {
        return Medico(std::stoi(valores[0]), valores[1], valores[2], valores[3], valores[4],
                        valores[5], especialidadDesdeCodigo(valores[6]));
    };

    config.esActivo = [](const Medico& medico) { return medico.esActivo(); };

    // LOS CRITERIOS DE BUSQUEDA
    //
    // LAS REGLAS SON LAS DEL REPOSITORIO
    //
    // La pantalla NO tiene reglas de busqueda propias: las pregunta al
    // repositorio, que es quien tiene el conjunto de los medicos. El
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
             Medico medico;
             if (repositorio.buscarPorId(id, medico)) {
                 filas.push_back(filaDe(medico));
             }
             return filas;
         }},
        {"nombre o apellido", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             for (const Medico& medico : repositorio.buscarPorNombre(valor)) {
                 filas.push_back(filaDe(medico));
             }
             return filas;
         }},
        {"legajo", [&repositorio](const std::string& valor) {
             std::vector<std::vector<std::string>> filas;
             // El repositorio devuelve el id del medico que tiene el legajo:
             // con ese id se arma la fila.
             int id = -1;
             if (!repositorio.existeLegajo(valor, id)) {
                 return filas;
             }
             Medico medico;
             if (repositorio.buscarPorId(id, medico)) {
                 filas.push_back(filaDe(medico));
             }
             return filas;
         }}};

    // EL ROTULO DE LA FICHA
    //
    // El motor arma la opcion 6 del menu con este texto: cada pantalla dice
    // lo que su ficha muestra.
    config.opcionFicha = "Ver ficha + turnos";

    // LOS TURNOS DE LA FICHA
    //
    // Los VIGENTES son los que el medico tiene por delante y el HISTORIAL
    // son todos (los cancelados y los reprogramados tambien son parte de su
    // historia).
    //
    // El servicio no tiene una consulta de "turnos vigentes de un medico"
    // (la que usa es la de los pacientes): los vigentes se sacan del historial
    // filtrando por estado.
    config.turnos.cabeceras = {"id", "fecha", "hora", "duracion", "consultorio", "especialidad", "estado"};
    config.turnos.vigentes = [&servicioTurnos](int id) {
        std::vector<std::vector<std::string>> filas;
        for (const Turno& turno : servicioTurnos.turnosDeMedico(id)) {
            if (turno.getEstado() == EstadoTurno::Activo) {
                filas.push_back(filaDeTurno(turno));
            }
        }
        return filas;
    };
    config.turnos.historial = [&servicioTurnos](int id) {
        std::vector<std::vector<std::string>> filas;
        for (const Turno& turno : servicioTurnos.turnosDeMedico(id)) {
            filas.push_back(filaDeTurno(turno));
        }
        return filas;
    };

    return config;
}

std::unique_ptr<Pantalla> crearPantallaMedicos(Aplicacion& aplicacion,
                                                    RepositorioMedicos& repositorio,
                                                    ServicioTurnos& servicioTurnos) {
    // EL MISMO OBJETO CUMPLE LOS DOS PAPELES DEL MOTOR
    //
    // El motor CRUD necesita el CRUD (`IRepositorio<Medico>`) y la baja
    // logica (`IRepositorioBajaLogica<Medico>`). El repositorio concreto
    // implementa las dos interfaces, asi que se pasa UNA sola vez.
    return std::make_unique<CrudPersonas<Medico>>(
        aplicacion, repositorio, repositorio, configDeMedicos(repositorio, servicioTurnos));
}

} // namespace Pantallas
} // namespace ui
