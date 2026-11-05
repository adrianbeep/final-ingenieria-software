#include "domain/Especialidad.h"

#include "exceptions/ExcepcionesSistema.h"

#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// EL CATALOGO ES UNA SOLA TABLA.
//
// Si `descripcionDe`, `codigoDe` y `especialidadDesdeCodigo` tuvieran tres
// listas paralelas, al agregar una especialidad se olvidaria de una y
// `descripcionDe` devolveria la descripcion de otra. Con UNA tabla, las
// tres funciones derivan de la misma fuente de verdad y no se pueden
// desincronizar.
//
// `codigo` y `descripcion` son `const char*` (no `std::string`): la tabla es
// constante y las funciones publicas devuelven `std::string` por valor.
// ---------------------------------------------------------------------------
struct InfoEspecialidad {
    Especialidad especialidad;
    const char* codigo;
    const char* descripcion;
};

const InfoEspecialidad CATALOGO[] = {
    {Especialidad::CARD, "CARD", "Cardiología"},
    {Especialidad::MEDG, "MEDG", "Medicina General"},
    {Especialidad::PEDI, "PEDI", "Pediatría"},
    {Especialidad::TRAU, "TRAU", "Traumatología"},
    {Especialidad::DERM, "DERM", "Dermatología"},
    {Especialidad::NEUR, "NEUR", "Neurología"},
    {Especialidad::OFTA, "OFTA", "Oftalmología"},
    {Especialidad::PSI, "PSI", "Psiquiatría"},
    {Especialidad::RADI, "RADI", "Radiología"},
    {Especialidad::GAST, "GAST", "Gastroenterología"},
    {Especialidad::ENDO, "ENDO", "Endocrinología"},
    {Especialidad::NEUM, "NEUM", "Neumología"}
};

constexpr int CANTIDAD_ESPECIALIDADES = static_cast<int>(sizeof(CATALOGO) / sizeof(CATALOGO[0]));

// Devuelve la entrada del catalogo con ese codigo, o nullptr si no existe.
const InfoEspecialidad* buscarPorCodigo(const std::string& codigo) {
    for (const InfoEspecialidad& info : CATALOGO) {
        if (codigo == info.codigo) {
            return &info;
        }
    }
    return nullptr;
}

// Devuelve la entrada del catalogo con esa especialidad, o nullptr si el valor no
// pertenece al catalogo (un enum casteado desde fuera).
const InfoEspecialidad* buscarPorEspecialidad(Especialidad especialidad) {
    for (const InfoEspecialidad& info : CATALOGO) {
        if (info.especialidad == especialidad) {
            return &info;
        }
    }
    return nullptr;
}

// Texto para el mensaje de error cuando el valor no pertenece al catalogo.
std::string valorDeEspecialidad(Especialidad especialidad) {
    return std::to_string(static_cast<int>(especialidad));
}

} // namespace

std::string descripcionDe(Especialidad especialidad) {
    const InfoEspecialidad* info = buscarPorEspecialidad(especialidad);
    if (info == nullptr) {
        throw DatoInvalidoException("especialidad", valorDeEspecialidad(especialidad),
                                    "la especialidad no pertenece al catalogo");
    }
    return info->descripcion;
}

std::string codigoDe(Especialidad especialidad) {
    const InfoEspecialidad* info = buscarPorEspecialidad(especialidad);
    if (info == nullptr) {
        throw DatoInvalidoException("especialidad", valorDeEspecialidad(especialidad),
                                    "la especialidad no pertenece al catalogo");
    }
    return info->codigo;
}

Especialidad especialidadDesdeCodigo(const std::string& codigo) {
    const InfoEspecialidad* info = buscarPorCodigo(codigo);
    if (info == nullptr) {
        throw DatoInvalidoException("especialidad", codigo, "codigo de especialidad desconocido");
    }
    return info->especialidad;
}

const std::vector<Especialidad>& todasLasEspecialidades() {
    // Singleton de Meyers: el vector se construye UNA sola vez, en la primera
    // llamada, y despues se devuelve la referencia. Derivarlo de la misma
    // tabla del catalogo evita una segunda lista que se pueda desincronizar.
    static const std::vector<Especialidad> todas = []() {
        std::vector<Especialidad> listado;
        listado.reserve(CANTIDAD_ESPECIALIDADES);
        for (const InfoEspecialidad& info : CATALOGO) {
            listado.push_back(info.especialidad);
        }
        return listado;
    }();
    return todas;
}
