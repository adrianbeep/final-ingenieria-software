#include "repositories/RepositorioMedicos.h"

#include "exceptions/ExcepcionesSistema.h"
#include "repositories/BajaLogicaCompartida.h"
#include "utils/Utils.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// El nombre de la entidad que viaja en el header del archivo de datos.
//
// NO es mecanico a partir de `getTipo()`: `getTipo()` devuelve "Medico" y el
// plural no sale de ahi. Por eso el nombre va escrito aca, en MAYUSCULAS.
// ---------------------------------------------------------------------------
namespace {

const char* NOMBRE_ENTIDAD = "MEDICOS";

} // namespace

RepositorioMedicos::RepositorioMedicos(std::string ruta)
    : RepositorioCRUD<Medico>(std::move(ruta), NOMBRE_ENTIDAD) {
}

void RepositorioMedicos::bajaLogica(int id) {
    // La logica es la MISMA que la del paciente: vive en
    // `BajaLogicaCompartida`, escrita UNA sola vez.
    BajaLogicaCompartida<Medico>::darDeBaja(*this, id);
}

void RepositorioMedicos::reactivar(int id) {
    BajaLogicaCompartida<Medico>::reactivar(*this, id);
}

bool RepositorioMedicos::existeLegajo(const std::string& legajo, int& idExistente) const {
    // No excluye a nadie: la pregunta es "existe ESTE legajo en el
    // repositorio", sin excepciones.
    return buscarLegajo(legajo, -1, idExistente);
}

std::vector<Medico> RepositorioMedicos::listarActivos() const {
    std::vector<Medico> activos;

    // `listar()` sale ordenado por id, asi que el resultado tambien.
    //
    // POR QUE NO UN INDICE SECUNDARIO POR EL FLAG `activo`
    //
    // El repositorio esta indexado por ID. Un indice por "activo" seria un
    // estado mas que hay que mantener en cada alta, modificacion, baja y
    // recarga, con el riesgo de que se desincronice del arbol. El volumen del
    // sistema lo hace irrelevante: un recorrido lineal sobre cientos de
    // medicos es despreciable al lado de escribir el archivo. Es el mismo
    // criterio que usa la busqueda por legajo.
    for (const Medico& medico : listar()) {
        if (medico.esActivo()) {
            activos.push_back(medico);
        }
    }

    return activos;
}

std::vector<Medico> RepositorioMedicos::listarInactivos() const {
    std::vector<Medico> inactivos;

    // Los INACTIVOS tambien se listan: la baja es LOGICA, asi que el medico
    // sigue en el repositorio y hay que poder mostrarlo (y reactivarlo).
    for (const Medico& medico : listar()) {
        if (!medico.esActivo()) {
            inactivos.push_back(medico);
        }
    }

    return inactivos;
}

bool RepositorioMedicos::buscarLegajo(const std::string& legajo,
                                          int idExcluido,
                                          int& idExistente) const {
    // La salida arranca en -1 (la convencion del contrato): si no se
    // encuentra nada, quien llama recibe -1 y no un id de una consulta
    // anterior.
    idExistente = -1;

    // La comparacion es CASE-INSENSITIVE: dos legajos que solo difieren en
    // las mayusculas son el MISMO legajo. Sin esto la unicidad seria un
    // espejismo: "LEG-1001" y "leg-1001" podrian convivir.
    //
    // `Utils::aMinusculas` es la UNICA forma de comparar textos en este
    // proyecto: decodifica UTF-8 y baja tanto el ASCII como el rango Latin-1
    // Supplement. Los legajos del dominio son alfanumericos ASCII (ver
    // `Medico::legajoValido`), asi que para ellos alcanza (y sobra) la
    // parte ASCII de `aMinusculas`.
    const std::string buscado = Utils::aMinusculas(Utils::trim(legajo));
    if (buscado.empty()) {
        return false;
    }

    // POR QUE UN RECORRIDO LINEAL Y NO UN INDICE SECUNDARIO POR LEGAJO
    //
    // La matriz del repositorio esta indexada por ID, no por legajo: el
    // `ArbolBST` ordena por `getId()`. Agregar un indice secundario por
    // legajo seria sobre-ingenieria para este dominio: el volumen del
    // sistema lo hace irrelevante (un recorrido lineal sobre cientos de
    // entidades es despreciable al lado de escribir el archivo), y el indice
    // tendria que mantenerce en cada alta, modificacion, baja y recarga, con
    // el riesgo de un estado mas que se desincroniza del arbol.
    const std::vector<Medico> todos = listar();

    for (const Medico& medico : todos) {
        if (medico.getId() == idExcluido) {
            continue;
        }
        // El legajo guardado ya viene recortado (el setter lo recorta), asi
        // que alcanza con bajarlo a minusculas.
        if (Utils::aMinusculas(medico.getLegajo()) == buscado) {
            idExistente = medico.getId();
            return true;
        }
    }

    return false;
}

void RepositorioMedicos::alta(const Medico& medico) {
    // La unicidad del legajo se verifica ANTES de tocar el arbol: si la
    // excepcion saliera despues, el repositorio quedaria con la entidad
    // insertada y el archivo con un registro de mas.
    //
    // El error es `DatoInvalidoException` y NO una excepcion nueva: es el
    // unico formato de error de dato invalido de toda la aplicacion, y la
    // capa de UI ya sabe mostrar su `campo()` y su `valor()`.
    //
    // La exclusion por el propio id deja pasar el alta de un id repetido
    // hasta el CRUD, que lanza `IdDuplicadoException`: ese error es mas
    // especifico que "legajo repetido" para ese caso.
    int idExistente = -1;
    if (buscarLegajo(medico.getLegajo(), medico.getId(), idExistente)) {
        throw DatoInvalidoException("legajo", medico.getLegajo(),
                                    "ya lo tiene el medico #" + std::to_string(idExistente));
    }

    // A partir de aca, el CRUD hace su trabajo: si el id esta ocupado lanza
    // `IdDuplicadoException`, si no inserta, rebalancea y persiste.
    RepositorioCRUD<Medico>::alta(medico);
}

void RepositorioMedicos::modificar(const Medico& medico) {
    // Misma regla que en `alta`, con la EXCLUSION por el propio id: el
    // medico que se esta modificando conserva su legajo y no puede chocar
    // consigo mismo. Solo choca con el legajo de OTRO medico.
    //
    // La validacion va ANTES de tocar el arbol, por la misma razon que en
    // `alta`: si la excepcion saliera despues, el repositorio quedaria con
    // la entidad nueva y el archivo con un registro de mas.
    int idExistente = -1;
    if (buscarLegajo(medico.getLegajo(), medico.getId(), idExistente)) {
        throw DatoInvalidoException("legajo", medico.getLegajo(),
                                    "ya lo tiene el medico #" + std::to_string(idExistente));
    }

    RepositorioCRUD<Medico>::modificar(medico);
}
