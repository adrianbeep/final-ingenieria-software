#include "domain/Materia.h"

#include "exceptions/ExcepcionesSistema.h"

#include <string>
#include <vector>

namespace {

// ---------------------------------------------------------------------------
// EL CATALOGO ES UNA SOLA TABLA.
//
// Si `descripcionDe`, `codigoDe` y `materiaDesdeCodigo` tuvieran tres
// listas paralelas, al agregar una materia se olvidaria de una y
// `descripcionDe` devolveria la descripcion de otra. Con UNA tabla, las
// tres funciones derivan de la misma fuente de verdad y no se pueden
// desincronizar.
//
// `codigo` y `descripcion` son `const char*` (no `std::string`): la tabla es
// constante y las funciones publicas devuelven `std::string` por valor.
// ---------------------------------------------------------------------------
struct InfoMateria {
    Materia materia;
    const char* codigo;
    const char* descripcion;
};

const InfoMateria CATALOGO[] = {
    {Materia::ALG1, "ALG1", "Algoritmos I"},
    {Materia::ALG2, "ALG2", "Algoritmos II"},
    {Materia::BDD, "BDD", "Base de Datos"},
    {Materia::ING1, "ING1", "Ingeniería de Software I"},
    {Materia::ING2, "ING2", "Ingeniería de Software II"},
    {Materia::ESTRUCT, "ESTRUCT", "Estructuras de Datos"},
    {Materia::ANALISIS, "ANALISIS", "Análisis Matemático"},
    {Materia::CALC1, "CALC1", "Cálculo I"},
    {Materia::FISICA2, "FISICA2", "Física II"},
    {Materia::REDES, "REDES", "Redes y Comunicación"},
    {Materia::SISTEMAS, "SISTEMAS", "Sistemas Operativos"},
    {Materia::ARQUITECTURA, "ARQUITECTURA", "Arquitectura de Computadores"}
};

constexpr int CANTIDAD_MATERIAS = static_cast<int>(sizeof(CATALOGO) / sizeof(CATALOGO[0]));

// Devuelve la entrada del catalogo con ese codigo, o nullptr si no existe.
const InfoMateria* buscarPorCodigo(const std::string& codigo) {
    for (const InfoMateria& info : CATALOGO) {
        if (codigo == info.codigo) {
            return &info;
        }
    }
    return nullptr;
}

// Devuelve la entrada del catalogo con esa materia, o nullptr si el valor no
// pertenece al catalogo (un enum casteado desde fuera).
const InfoMateria* buscarPorMateria(Materia materia) {
    for (const InfoMateria& info : CATALOGO) {
        if (info.materia == materia) {
            return &info;
        }
    }
    return nullptr;
}

// Texto para el mensaje de error cuando el valor no pertenece al catalogo.
std::string valorDeMateria(Materia materia) {
    return std::to_string(static_cast<int>(materia));
}

} // namespace

std::string descripcionDe(Materia materia) {
    const InfoMateria* info = buscarPorMateria(materia);
    if (info == nullptr) {
        throw DatoInvalidoException("materia", valorDeMateria(materia),
                                    "la materia no pertenece al catalogo");
    }
    return info->descripcion;
}

std::string codigoDe(Materia materia) {
    const InfoMateria* info = buscarPorMateria(materia);
    if (info == nullptr) {
        throw DatoInvalidoException("materia", valorDeMateria(materia),
                                    "la materia no pertenece al catalogo");
    }
    return info->codigo;
}

Materia materiaDesdeCodigo(const std::string& codigo) {
    const InfoMateria* info = buscarPorCodigo(codigo);
    if (info == nullptr) {
        throw DatoInvalidoException("materia", codigo, "codigo de materia desconocido");
    }
    return info->materia;
}

const std::vector<Materia>& todasLasMaterias() {
    // Singleton de Meyers: el vector se construye UNA sola vez, en la primera
    // llamada, y despues se devuelve la referencia. Derivarlo de la misma
    // tabla del catalogo evita una segunda lista que se pueda desincronizar.
    static const std::vector<Materia> todas = []() {
        std::vector<Materia> listado;
        listado.reserve(CANTIDAD_MATERIAS);
        for (const InfoMateria& info : CATALOGO) {
            listado.push_back(info.materia);
        }
        return listado;
    }();
    return todas;
}
