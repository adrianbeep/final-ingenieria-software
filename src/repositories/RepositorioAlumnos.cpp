#include "repositories/RepositorioAlumnos.h"

#include "exceptions/ExcepcionesSistema.h"
#include "repositories/BajaLogicaCompartida.h"
#include "utils/Utils.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// El nombre de la entidad que viaja en el header del archivo de datos.
//
// NO es mecanico a partir de `getTipo()`: `getTipo()` devuelve "Alumno" y el
// plural no sale de ahi. Por eso el nombre va escrito aca, en MAYUSCULAS.
// ---------------------------------------------------------------------------
namespace {

const char* NOMBRE_ENTIDAD = "ALUMNOS";

} // namespace

RepositorioAlumnos::RepositorioAlumnos(std::string ruta)
    : RepositorioCRUD<Alumno>(std::move(ruta), NOMBRE_ENTIDAD) {
}

void RepositorioAlumnos::bajaLogica(int id) {
    // La logica es la MISMA que la del profesor: vive en
    // `BajaLogicaCompartida`, escrita UNA sola vez.
    BajaLogicaCompartida<Alumno>::darDeBaja(*this, id);
}

void RepositorioAlumnos::reactivar(int id) {
    BajaLogicaCompartida<Alumno>::reactivar(*this, id);
}

bool RepositorioAlumnos::existeMatricula(const std::string& matricula, int& idExistente) const {
    // No excluye a nadie: la pregunta es "existe ESTA matricula en el
    // repositorio", sin excepciones.
    return buscarMatricula(matricula, -1, idExistente);
}

bool RepositorioAlumnos::buscarMatricula(const std::string& matricula,
                                          int idExcluido,
                                          int& idExistente) const {
    // La salida arranca en -1 (la convencion del contrato): si no se
    // encuentra nada, quien llama recibe -1 y no un id de una consulta
    // anterior.
    idExistente = -1;

    // La comparacion es CASE-INSENSITIVE: dos matriculas que solo difieren
    // en las mayusculas son la MISMA matricula. Sin esto la unicidad seria
    // un espejismo: "MAT-20001" y "mat-20001" podrian convivir.
    //
    // `Utils::aMinusculas` es la UNICA forma de comparar textos en este
    // proyecto: decodifica UTF-8 y baja tanto el ASCII como el rango Latin-1
    // Supplement. Las matriculas del dominio son alfanumericas ASCII (ver
    // `Alumno::matriculaValido`), asi que para ellas alcanza (y sobra) la
    // parte ASCII de `aMinusculas`.
    const std::string buscada = Utils::aMinusculas(Utils::trim(matricula));
    if (buscada.empty()) {
        return false;
    }

    // POR QUE UN RECORRIDO LINEAL Y NO UN INDICE SECUNDARIO POR MATRICULA
    //
    // La matriz del repositorio esta indexada por ID, no por matricula: el
    // `ArbolBST` ordena por `getId()`. Agregar un indice secundario por
    // matricula seria sobre-ingenieria para este dominio: el volumen del
    // sistema lo hace irrelevante (un recorrido lineal sobre cientos de
    // entidades es despreciable al lado de escribir el archivo), y el indice
    // tendria que mantenerce en cada alta, modificacion, baja y recarga, con
    // el riesgo de un estado mas que se desincroniza del arbol.
    const std::vector<Alumno> todos = listar();

    for (const Alumno& alumno : todos) {
        if (alumno.getId() == idExcluido) {
            continue;
        }
        // La matricula guardada ya viene recortada (el setter la recorta),
        // asi que alcanza con bajarla a minusculas.
        if (Utils::aMinusculas(alumno.getMatricula()) == buscada) {
            idExistente = alumno.getId();
            return true;
        }
    }

    return false;
}

void RepositorioAlumnos::alta(const Alumno& alumno) {
    // La unicidad de la matricula se verifica ANTES de tocar el arbol: si
    // la excepcion saliera despues, el repositorio quedaria con la entidad
    // insertada y el archivo con un registro de mas.
    //
    // El error es `DatoInvalidoException` y NO una excepcion nueva: es el
    // unico formato de error de dato invalido de toda la aplicacion, y la
    // capa de UI ya sabe mostrar su `campo()` y su `valor()`.
    //
    // La exclusion por el propio id deja pasar el alta de un id repetido
    // hasta el CRUD, que lanza `IdDuplicadoException`: ese error es mas
    // especifico que "matricula repetida" para ese caso.
    int idExistente = -1;
    if (buscarMatricula(alumno.getMatricula(), alumno.getId(), idExistente)) {
        throw DatoInvalidoException("matricula", alumno.getMatricula(),
                                    "ya la tiene el alumno #" + std::to_string(idExistente));
    }

    // A partir de aca, el CRUD hace su trabajo: si el id esta ocupado lanza
    // `IdDuplicadoException`, si no inserta, rebalancea y persiste.
    RepositorioCRUD<Alumno>::alta(alumno);
}

void RepositorioAlumnos::modificar(const Alumno& alumno) {
    // Misma regla que en `alta`, con la EXCLUSION por el propio id: el
    // alumno que se esta modificando conserva su matricula y no puede
    // chocar consigo mismo. Solo choca con la matricula de OTRO alumno.
    //
    // La validacion va ANTES de tocar el arbol, por la misma razon que en
    // `alta`: si la excepcion saliera despues, el repositorio quedaria con
    // la entidad nueva y el archivo con un registro de mas.
    int idExistente = -1;
    if (buscarMatricula(alumno.getMatricula(), alumno.getId(), idExistente)) {
        throw DatoInvalidoException("matricula", alumno.getMatricula(),
                                    "ya la tiene el alumno #" + std::to_string(idExistente));
    }

    RepositorioCRUD<Alumno>::modificar(alumno);
}
