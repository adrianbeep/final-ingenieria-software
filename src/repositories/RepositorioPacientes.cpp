#include "repositories/RepositorioPacientes.h"

#include "exceptions/ExcepcionesSistema.h"
#include "repositories/BajaLogicaCompartida.h"
#include "utils/Utils.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// El nombre de la entidad que viaja en el header del archivo de datos.
//
// NO es mecanico a partir de `getTipo()`: `getTipo()` devuelve "Paciente" y el
// plural no sale de ahi. Por eso el nombre va escrito aca, en MAYUSCULAS.
// ---------------------------------------------------------------------------
namespace {

const char* NOMBRE_ENTIDAD = "PACIENTES";

} // namespace

RepositorioPacientes::RepositorioPacientes(std::string ruta)
    : RepositorioCRUD<Paciente>(std::move(ruta), NOMBRE_ENTIDAD) {
}

void RepositorioPacientes::bajaLogica(int id) {
    // La logica es la MISMA que la del medico: vive en
    // `BajaLogicaCompartida`, escrita UNA sola vez.
    BajaLogicaCompartida<Paciente>::darDeBaja(*this, id);
}

void RepositorioPacientes::reactivar(int id) {
    BajaLogicaCompartida<Paciente>::reactivar(*this, id);
}

bool RepositorioPacientes::existeDni(const std::string& dni, int& idExistente) const {
    // No excluye a nadie: la pregunta es "existe ESTE dni en el
    // repositorio", sin excepciones.
    return buscarDni(dni, -1, idExistente);
}

std::vector<Paciente> RepositorioPacientes::listarActivos() const {
    std::vector<Paciente> activos;

    // `listar()` sale ordenado por id, asi que el resultado tambien.
    //
    // POR QUE NO UN INDICE SECUNDARIO POR EL FLAG `activo`
    //
    // El repositorio esta indexado por ID. Un indice por "activo" seria un
    // estado mas que hay que mantener en cada alta, modificacion, baja y
    // recarga, con el riesgo de que se desincronice del arbol. El volumen del
    // sistema lo hace irrelevante: un recorrido lineal sobre cientos de
    // pacientes es despreciable al lado de escribir el archivo. Es el mismo
    // criterio que usa la busqueda por dni.
    for (const Paciente& paciente : listar()) {
        if (paciente.esActivo()) {
            activos.push_back(paciente);
        }
    }

    return activos;
}

std::vector<Paciente> RepositorioPacientes::listarInactivos() const {
    std::vector<Paciente> inactivos;

    // Los INACTIVOS tambien se listan: la baja es LOGICA, asi que el paciente
    // sigue en el repositorio y hay que poder mostrarlo (y reactivarlo).
    for (const Paciente& paciente : listar()) {
        if (!paciente.esActivo()) {
            inactivos.push_back(paciente);
        }
    }

    return inactivos;
}

bool RepositorioPacientes::buscarDni(const std::string& dni,
                                          int idExcluido,
                                          int& idExistente) const {
    // La salida arranca en -1 (la convencion del contrato): si no se
    // encuentra nada, quien llama recibe -1 y no un id de una consulta
    // anterior.
    idExistente = -1;

    // La comparacion es EXACTA: dos dnis son el mismo si tienen
    // en las mayusculas son la MISMO dni. Sin esto la unicidad seria
    // un espejismo: "MAT-20001" y "mat-20001" podrian convivir.
    //
    // `Utils::aMinusculas` es la UNICA forma de comparar textos en este
    // proyecto: decodifica UTF-8 y baja tanto el ASCII como el rango Latin-1
    // Supplement. Los dnis del dominio son DIGITOS ASCII (ver
    // `Paciente::dniValido`), asi que para ellas alcanza (y sobra) la
    // parte ASCII de `aMinusculas`.
    const std::string buscada = Utils::aMinusculas(Utils::trim(dni));
    if (buscada.empty()) {
        return false;
    }

    // POR QUE UN RECORRIDO LINEAL Y NO UN INDICE SECUNDARIO POR MATRICULA
    //
    // La matriz del repositorio esta indexada por ID, no por dni: el
    // `ArbolBST` ordena por `getId()`. Agregar un indice secundario por
    // dni seria sobre-ingenieria para este dominio: el volumen del
    // sistema lo hace irrelevante (un recorrido lineal sobre cientos de
    // entidades es despreciable al lado de escribir el archivo), y el indice
    // tendria que mantenerce en cada alta, modificacion, baja y recarga, con
    // el riesgo de un estado mas que se desincroniza del arbol.
    const std::vector<Paciente> todos = listar();

    for (const Paciente& paciente : todos) {
        if (paciente.getId() == idExcluido) {
            continue;
        }
        // El dni guardado ya viene recortada (el setter la recorta),
        // asi que alcanza con bajarla a minusculas.
        if (Utils::aMinusculas(paciente.getDni()) == buscada) {
            idExistente = paciente.getId();
            return true;
        }
    }

    return false;
}

void RepositorioPacientes::alta(const Paciente& paciente) {
    // La unicidad del dni se verifica ANTES de tocar el arbol: si
    // la excepcion saliera despues, el repositorio quedaria con la entidad
    // insertada y el archivo con un registro de mas.
    //
    // El error es `DatoInvalidoException` y NO una excepcion nueva: es el
    // unico formato de error de dato invalido de toda la aplicacion, y la
    // capa de UI ya sabe mostrar su `campo()` y su `valor()`.
    //
    // La exclusion por el propio id deja pasar el alta de un id repetido
    // hasta el CRUD, que lanza `IdDuplicadoException`: ese error es mas
    // especifico que "dni repetido" para ese caso.
    int idExistente = -1;
    if (buscarDni(paciente.getDni(), paciente.getId(), idExistente)) {
        throw DatoInvalidoException("dni", paciente.getDni(),
                                    "ya la tiene el paciente #" + std::to_string(idExistente));
    }

    // A partir de aca, el CRUD hace su trabajo: si el id esta ocupado lanza
    // `IdDuplicadoException`, si no inserta, rebalancea y persiste.
    RepositorioCRUD<Paciente>::alta(paciente);
}

void RepositorioPacientes::modificar(const Paciente& paciente) {
    // Misma regla que en `alta`, con la EXCLUSION por el propio id: el
    // paciente que se esta modificando conserva su dni y no puede
    // chocar consigo mismo. Solo choca con el dni de OTRO paciente.
    //
    // La validacion va ANTES de tocar el arbol, por la misma razon que en
    // `alta`: si la excepcion saliera despues, el repositorio quedaria con
    // la entidad nueva y el archivo con un registro de mas.
    int idExistente = -1;
    if (buscarDni(paciente.getDni(), paciente.getId(), idExistente)) {
        throw DatoInvalidoException("dni", paciente.getDni(),
                                    "ya la tiene el paciente #" + std::to_string(idExistente));
    }

    RepositorioCRUD<Paciente>::modificar(paciente);
}
