#include "test_framework.h"

#include "domain/Especialidad.h"
#include "entities/Medico.h"
#include "entities/Paciente.h"
#include "repositories/RepositorioMedicos.h"
#include "repositories/RepositorioPacientes.h"
#include "ui/CriteriosDePersonas.h"

#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

// ---------------------------------------------------------------------------
// Tests de los CRITERIOS de busqueda de las personas.
//
// LOS CRITERIOS SON COMPARTIDOS
//
// Los usan las cuatro pantallas que buscan a una persona: las de pacientes,
// las de medicos, la de turnos (al agendar) y la de reportes. Por eso viven
// en un header (`ui/CriteriosDePersonas.h`) y no en la configuracion de una
// pantalla: si cada una escribiera los suyos, buscar un paciente se
// comportaria de una manera en cada pantalla.
//
// Lo que se verifica aca:
//
//   - que los criterios son los de ANTES (los mismos tres, con las mismas
//     etiquetas) y que devuelven las MISMAS coincidencias: el id de la
//     entidad con su fila;
//   - que las reglas son las del REPOSITORIO: el nombre o apellido es una
//     coincidencia PARCIAL e insensible a mayusculas, y el dni y el legajo son
//     EXACTOS (la misma con la que se verifica su unicidad).
// ---------------------------------------------------------------------------
namespace {

// Directorio temporal donde los tests escriben sus archivos.
std::filesystem::path directorioDePruebas() {
    static const std::filesystem::path directorio = [] {
        const std::filesystem::path base =
            std::filesystem::temp_directory_path() / "sistema_turnos_criterios";
        std::error_code error;
        std::filesystem::remove_all(base, error);
        std::filesystem::create_directories(base, error);
        return base;
    }();
    return directorio;
}

// La ruta de un archivo de prueba NUEVO: borra el anterior, asi cada test
// arranca con el repositorio vacio.
std::string rutaNuevaDePrueba(const std::string& nombre) {
    const std::filesystem::path ruta = directorioDePruebas() / nombre;
    std::error_code error;
    std::filesystem::remove(ruta, error);
    return ruta.string();
}

// Un paciente valido, para las pruebas que necesitan uno ya cargado.
Paciente pacienteDePrueba(int id, const std::string& nombre, const std::string& apellido,
                          const std::string& dni) {
    return Paciente(id, nombre, apellido, nombre + "." + apellido + "@unlp.edu.ar", "2214556677",
                    dni);
}

// Un medico valido.
Medico medicoDePrueba(int id, const std::string& nombre, const std::string& apellido,
                      const std::string& legajo) {
    return Medico(id, nombre, apellido, nombre + "." + apellido + "@unlp.edu.ar", "2214556677",
                  legajo, Especialidad::CARD);
}

// La celda de una coincidencia (la fila de la entidad).
std::string celda(const ui::Coincidencia& coincidencia, std::size_t indice) {
    if (indice >= coincidencia.second.size()) {
        return "";
    }
    return coincidencia.second[indice];
}

} // namespace

TEST_CASE("los criterios de pacientes devuelven las mismas coincidencias que antes",
           criterios_los_de_pacientes) {
    RepositorioPacientes pacientes(rutaNuevaDePrueba("criterios_de_pacientes"));
    pacientes.cargar();
    pacientes.alta(pacienteDePrueba(1, "Ana", "Gomez", "2020391"));
    pacientes.alta(pacienteDePrueba(2, "Beto", "Diaz", "2020392"));
    pacientes.alta(pacienteDePrueba(3, "Caro", "Sosa", "2020393"));

    const std::vector<ui::CriterioBusqueda> criterios = ui::criteriosDePacientes(pacientes);

    // LOS MISMOS TRES CRITERIOS, CON LAS MISMAS ETIQUETAS
    CHECK(criterios.size() == 3);
    if (criterios.size() != 3) {
        return;
    }
    CHECK(criterios[0].etiqueta == "id");
    CHECK(criterios[1].etiqueta == "nombre o apellido");
    CHECK(criterios[2].etiqueta == "dni");

    // EL CRITERIO "ID": EL ID CON SU FILA
    const std::vector<ui::Coincidencia> porId = criterios[0].buscar("2");
    CHECK(porId.size() == 1);
    if (!porId.empty()) {
        CHECK(porId[0].first == 2);
        // LA FILA COMPLETA DE LA ENTIDAD (id, nombre, apellido, email,
        // telefono, dni y estado).
        CHECK(porId[0].second.size() == 7);
        CHECK(celda(porId[0], 0) == "2");
        CHECK(celda(porId[0], 1) == "Beto");
        CHECK(celda(porId[0], 2) == "Diaz");
        CHECK(celda(porId[0], 5) == "2020392");
        CHECK(celda(porId[0], 6) == "activo");
    }
    // Un id que no existe no devuelve nada.
    CHECK(criterios[0].buscar("9").empty() == true);

    // EL CRITERIO "NOMBRE O APELLIDO": PARCIAL E INSENSIBLE A MAYUSCULAS
    const std::vector<ui::Coincidencia> porNombre = criterios[1].buscar("go");
    CHECK(porNombre.size() == 1);
    if (!porNombre.empty()) {
        CHECK(porNombre[0].first == 1);
        CHECK(celda(porNombre[0], 2) == "Gomez");
    }
    // El apellido tambien cuenta (y el texto se recorta).
    CHECK(criterios[1].buscar("SOSA").size() == 1);
    CHECK(criterios[1].buscar("sosa").size() == 1);
    // Un texto vacio NO encuentra nada.
    CHECK(criterios[1].buscar("").empty() == true);

    // EL CRITERIO "DNI": EXACTO
    const std::vector<ui::Coincidencia> porDni = criterios[2].buscar("2020393");
    CHECK(porDni.size() == 1);
    if (!porDni.empty()) {
        CHECK(porDni[0].first == 3);
        CHECK(celda(porDni[0], 5) == "2020393");
    }
    // Un dni incompleto NO es el dni de nadie: la comparacion es EXACTA.
    CHECK(criterios[2].buscar("202039").empty() == true);
}

TEST_CASE("los criterios de medicos devuelven las mismas coincidencias que antes",
           criterios_los_de_medicos) {
    RepositorioMedicos medicos(rutaNuevaDePrueba("criterios_de_medicos"));
    medicos.cargar();
    medicos.alta(medicoDePrueba(1, "Beto", "Perez", "LEG-1001"));
    medicos.alta(medicoDePrueba(2, "Caro", "Ruiz", "LEG-1002"));

    const std::vector<ui::CriterioBusqueda> criterios = ui::criteriosDeMedicos(medicos);

    // LOS MISMOS TRES CRITERIOS, CON LAS MISMAS ETIQUETAS
    CHECK(criterios.size() == 3);
    if (criterios.size() != 3) {
        return;
    }
    CHECK(criterios[0].etiqueta == "id");
    CHECK(criterios[1].etiqueta == "nombre o apellido");
    CHECK(criterios[2].etiqueta == "legajo");

    // EL CRITERIO "ID": EL ID CON SU FILA
    const std::vector<ui::Coincidencia> porId = criterios[0].buscar("2");
    CHECK(porId.size() == 1);
    if (!porId.empty()) {
        CHECK(porId[0].first == 2);
        // LA FILA COMPLETA DE LA ENTIDAD (id, nombre, apellido, email,
        // telefono, legajo, especialidad y estado).
        CHECK(porId[0].second.size() == 8);
        CHECK(celda(porId[0], 0) == "2");
        CHECK(celda(porId[0], 1) == "Caro");
        CHECK(celda(porId[0], 2) == "Ruiz");
        CHECK(celda(porId[0], 5) == "LEG-1002");
        // La especialidad se muestra con su DESCRIPCION (con tilde): es lo
        // que el usuario lee.
        CHECK(celda(porId[0], 6) == descripcionDe(Especialidad::CARD));
        CHECK(celda(porId[0], 7) == "activo");
    }
    CHECK(criterios[0].buscar("9").empty() == true);

    // EL CRITERIO "NOMBRE O APELLIDO": PARCIAL E INSENSIBLE A MAYUSCULAS
    const std::vector<ui::Coincidencia> porNombre = criterios[1].buscar("pe");
    CHECK(porNombre.size() == 1);
    if (!porNombre.empty()) {
        CHECK(porNombre[0].first == 1);
        CHECK(celda(porNombre[0], 2) == "Perez");
    }
    CHECK(criterios[1].buscar("").empty() == true);

    // EL CRITERIO "LEGAJO": EXACTO E INSENSIBLE A MAYUSCULAS
    //
    // Es la misma comparacion con la que se verifica su unicidad.
    const std::vector<ui::Coincidencia> porLegajo = criterios[2].buscar("LEG-1002");
    CHECK(porLegajo.size() == 1);
    if (!porLegajo.empty()) {
        CHECK(porLegajo[0].first == 2);
        CHECK(celda(porLegajo[0], 5) == "LEG-1002");
    }
    CHECK(criterios[2].buscar("leg-1002").size() == 1);
    // Un legajo incompleto NO es el legajo de nadie.
    CHECK(criterios[2].buscar("LEG-100").empty() == true);
}
