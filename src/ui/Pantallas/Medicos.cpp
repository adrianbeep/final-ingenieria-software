#include "ui/Pantallas/Medicos.h"

#include "domain/Especialidad.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "repositories/IRepositorioBajaLogica.h"
#include "services/ServicioTurnos.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>
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

// LA MARCA DE LOS TURNOS QUE YA NO SON DE LA ESPECIALIDAD DEL MEDICO
//
// Es un asterisco: va dentro de una celda de tabla, asi que tiene que ser
// CORTO. La leyenda que lo explica la dibuja el motor de la ficha, debajo
// de las tablas (la pone la pantalla, no el motor).
const char* MARCADOR_DE_ESPECIALIDAD = "*";

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
std::vector<std::string> filaDeTurno(const Turno& turno,
                                     const std::optional<Especialidad>& especialidadActual) {
    // LA MARCA DEL TURNO QUE YA NO ES DE LA ESPECIALIDAD DEL MEDICO
    //
    // La especialidad del TURNO es la que se agendo: es un hecho del pasado.
    // La del MEDICO es la que atiende hoy: gobierna las reservas nuevas.
    //
    // Cuando no son la misma, el turno es de una especialidad que el medico
    // ya no atiende. No es un error: es un dato historico (el medico cambio
    // de especialidad, o el turno se migro a mano). Pero tiene que verse, o
    // el usuario lo lee como un bug.
    //
    // LA MARCA ES CORTA
    //
    // Va dentro de una celda de una tabla: un asterisco y la leyenda que
    // esta debajo de las tablas.
    std::string especialidad = descripcionDe(turno.getEspecialidad());
    if (especialidadActual && turno.getEspecialidad() != *especialidadActual) {
        especialidad += " " + std::string(MARCADOR_DE_ESPECIALIDAD);
    }

    return {std::to_string(turno.getId()),
            turno.getFecha().aTexto(),
            turno.getHora().aTexto(),
            std::to_string(turno.getDuracionMin()),
            turno.getConsultorio(),
            especialidad,
            estadoATexto(turno.getEstado())};
}

// LA ESPECIALIDAD ACTUAL DEL MEDICO DE LA FICHA
//
// El sujeto de la ficha es el medico: la comparacion con la especialidad de
// sus turnos es natural y es barata (el medico ya esta cargado).
//
// SI EL MEDICO NO ESTA
//
// No hay especialidad actual con que comparar, asi que no se marca nada: la
// marca es una COMPARACION, y sin un lado no hay comparacion.
std::optional<Especialidad> especialidadActualDe(int id, RepositorioMedicos& medicos) {
    Medico medico;
    if (!medicos.buscarPorId(id, medico)) {
        return std::nullopt;
    }
    return medico.getEspecialidad();
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
    //
    // LA ESPECIALIDAD NO ESTA ACA
    //
    // La especialidad no es un campo de TEXTO: se ELIGE de una lista (la
    // decision 1). El `ui::Menu` no puede, porque exige un atajo de un
    // digito y el catalogo tiene doce especialidades: la elige el
    // `ui::Selector` de mas abajo.
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
         })}};

    // EL SELECTOR DE LA ESPECIALIDAD
    //
    // LO QUE SE VE Y LO QUE SE GUARDA SON DOS COSAS
    //
    // El usuario ve la DESCRIPCION ("Gastroenterologia") y lo que viaja al
    // archivo es el CODIGO ("GAST"): el `valor` devuelve el codigo y la
    // `opciones` guarda las descripciones.
    //
    // LA NUMERACION ES POSICIONAL
    //
    // Las opciones se numeran por su posicion (1, 2, 3, ...), asi que el
    // numero completo se puede escribir: "12" elige la doceava. Un `Menu`
    // no podria: sus atajos son de un digito.
    CrudPersonas<Medico>::Config::SelectorDeFormulario selector;
    selector.etiqueta = "especialidad";
    for (const Especialidad& especialidad : todasLasEspecialidades()) {
        selector.opciones.push_back(descripcionDe(especialidad));
    }
    selector.valor = [](std::size_t indice) -> std::string {
        return codigoDe(todasLasEspecialidades()[indice]);
    };
    // EL INDICE NO SE ADIVINA: SE BUSCA
    //
    // El numero del `enum class` NO es el indice del catalogo (el catalogo
    // es una tabla y el enum su declaracion): se comparan los CODIGOS, que
    // son la clave del catalogo.
    selector.indiceInicial = [](const Medico& medico) -> int {
        const std::string codigo = codigoDe(medico.getEspecialidad());
        const std::vector<Especialidad>& catalogo = todasLasEspecialidades();
        for (std::size_t i = 0; i < catalogo.size(); ++i) {
            if (codigoDe(catalogo[i]) == codigo) {
                return static_cast<int>(i);
            }
        }
        // No esta en el catalogo: se deja la primera opcion (el `ui::Selector`
        // ignora un indice fuera de rango).
        return 0;
    };
    config.selector = std::move(selector);

    // EL CONSTRUCTOR DE LA ENTIDAD
    //
    // El vector que llega es [id, nombre, apellido, email, telefono, legajo,
    // especialidad]: el id va adelante porque lo PROPONE el repositorio (o es
    // el id de la entidad que se esta modificando) y nunca lo escribe el
    // usuario. La especialidad llega como CODIGO (la elige el selector) y se
    // traduce al catalogo.
    config.construir = [](const std::vector<std::string>& valores) {
        return Medico(std::stoi(valores[0]), valores[1], valores[2], valores[3], valores[4],
                        valores[5], especialidadDesdeCodigo(valores[6]));
    };

    config.esActivo = [](const Medico& medico) { return medico.esActivo(); };

    // LOS CRITERIOS DE BUSQUEDA
    //
    // Cada criterio devuelve las COINCIDENCIAS: el id de la entidad y sus
    // celdas. La pantalla "Buscar" muestra las celdas (en una tabla) y el
    // buscador usa el id (para elegir la entidad).
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
             std::vector<Coincidencia> coincidencias;
             const int id = idDesdeTexto(valor);
             if (id < 0) {
                 return coincidencias;
             }
             Medico medico;
             if (repositorio.buscarPorId(id, medico)) {
                 coincidencias.push_back({medico.getId(), filaDe(medico)});
             }
             return coincidencias;
         }},
        {"nombre o apellido", [&repositorio](const std::string& valor) {
             std::vector<Coincidencia> coincidencias;
             for (const Medico& medico : repositorio.buscarPorNombre(valor)) {
                 coincidencias.push_back({medico.getId(), filaDe(medico)});
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
                 coincidencias.push_back({medico.getId(), filaDe(medico)});
             }
             return coincidencias;
         }}};

    // EL BUSCADOR DE MEDICOS
    //
    // Las columnas que muestra el buscador son las que AYUDAN a reconocer al
    // medico: el apellido y el nombre, la especialidad (que es lo que
    // distingue a dos medicos con el mismo nombre) y el legajo (que es lo que
    // lo identifica). El id va primero: es lo que el buscador devuelve.
    config.columnasBuscador = {"id", "apellido", "nombre", "especialidad", "legajo"};
    config.pistaBuscador = "Nombre, apellido, legajo o especialidad";

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
    //
    // LA MARCA DE LOS TURNOS QUE YA NO SON DE SU ESPECIALIDAD
    //
    // La ficha del MEDICO tiene al medico delante: por eso los turnos que
    // no son de la especialidad que atiende HOY se marcan. La ficha del
    // PACIENTE no muestra al medico, asi que no tiene con que comparar y no
    // lleva marca.
    config.turnos.cabeceras = {"id", "fecha", "hora", "duracion", "consultorio", "especialidad", "estado"};
    config.turnos.vigentes = [&servicioTurnos, &repositorio](int id) {
        std::vector<std::vector<std::string>> filas;
        const std::optional<Especialidad> actual = especialidadActualDe(id, repositorio);
        for (const Turno& turno : servicioTurnos.turnosDeMedico(id)) {
            if (turno.getEstado() == EstadoTurno::Activo) {
                filas.push_back(filaDeTurno(turno, actual));
            }
        }
        return filas;
    };
    config.turnos.historial = [&servicioTurnos, &repositorio](int id) {
        std::vector<std::vector<std::string>> filas;
        const std::optional<Especialidad> actual = especialidadActualDe(id, repositorio);
        for (const Turno& turno : servicioTurnos.turnosDeMedico(id)) {
            filas.push_back(filaDeTurno(turno, actual));
        }
        return filas;
    };

    // LA LEYENDA DE LA MARCA
    //
    // Solo aparece si el medico tiene AL MENOS UN turno marcado: una nota
    // que no explica nada es ruido.
    //
    // Es DATO que le pasa al motor de la ficha: el motor la dibuja sin saber
    // que significa (ver `TurnosDePersona::nota`).
    config.turnos.nota = [&servicioTurnos, &repositorio](int id) {
        const std::optional<Especialidad> actual = especialidadActualDe(id, repositorio);
        if (!actual) {
            return std::string();
        }
        for (const Turno& turno : servicioTurnos.turnosDeMedico(id)) {
            if (turno.getEspecialidad() != *actual) {
                return std::string("El * marca los turnos de una especialidad que el medico ya "
                                   "no atiende.");
            }
        }
        return std::string();
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
