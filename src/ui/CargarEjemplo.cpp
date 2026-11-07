#include "ui/CargarEjemplo.h"

#include "exceptions/ExcepcionesSistema.h"
#include "ui/Contexto.h"

#include <exception>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

// ---------------------------------------------------------------------------
// La carga de los datos de ejemplo.
//
// Lo que esta en el header es el POR QUE; lo que esta aca es el COMO.
//
// ---------------------------------------------------------------------------
namespace ui {
namespace {

// EL DIRECTORIO DE LA SEMILLA
//
// La semilla es el subdirectorio `ejemplo` de la MISMA carpeta que el
// archivo de datos: `data/pacientes.dat` se copia desde
// `data/ejemplo/pacientes.dat`.
//
// Se DERIVA del archivo de destino y no es una constante, por una razon
// sola: la copia tiene que ir al archivo que el repositorio tiene ABIERTO
// (el repositorio es el que sabe cual es), y la semilla es el archivo de la
// MISMA nombre que esta al lado, en la carpeta `ejemplo`.
//
// La semilla NO se toca: la copia va de la semilla al archivo vivo, nunca al
// reves.
constexpr const char* NOMBRE_DEL_DIRECTORIO_DE_SEMILLA = "ejemplo";

// La ruta de la semilla que corresponde a un archivo de datos.
std::filesystem::path semillaDe(const std::filesystem::path& destino) {
    return destino.parent_path() / NOMBRE_DEL_DIRECTORIO_DE_SEMILLA / destino.filename();
}

// EL TEMPORAL DE LA COPIA
//
// La copia NO va directo al destino: `copy_file` borra el destino y copia
// encima, asi que un corte a mitad deja el archivo de datos TRUNCADO.
//
// Es el MISMO patron que usa `ArchivoTexto::guardar()`: se escribe un
// `.tmp` en el MISMO directorio y despues se hace `rename` (que es atomico
// dentro del mismo volumen).
//
// El temporal vive en el mismo directorio a proposito: el `rename` solo es
// atomico dentro del mismo volumen, y ahi esta el destino.
std::filesystem::path temporalDe(const std::filesystem::path& destino) {
    return destino.parent_path() / (destino.filename().string() + ".tmp");
}

// Copia un archivo de la semilla sobre el archivo vivo, de forma ATOMICA.
//
// Devuelve false si no se pudo, y `error` dice por que.
bool copiarSobre(const std::filesystem::path& origen, const std::filesystem::path& destino,
                 std::string& error) {
    std::error_code code;

    if (!std::filesystem::exists(origen, code)) {
        error = "no se encontro la semilla '" + origen.string() + "'";
        return false;
    }

    const std::filesystem::path temporal = temporalDe(destino);

    // PASO 1: SE COPIA AL TEMPORAL
    //
    // `overwrite_existing`: el temporal puede haber quedado de una carga
    // anterior que se corto a mitad, y hay que poder pisarlo.
    std::filesystem::copy_file(origen, temporal,
                                std::filesystem::copy_options::overwrite_existing, code);
    if (code) {
        error = "no se pudo copiar '" + origen.string() + "' a '" + temporal.string()
            + "': " + code.message();
        return false;
    }

    // PASO 2: EL TEMPORAL PASA A SER EL ARCHIVO VIVO
    //
    // El `rename` REEMPLAZA el destino (a diferencia de `std::rename`, que
    // falla en Windows si el destino existe) y es atomico dentro del mismo
    // volumen: el archivo vivo esta viejo o nuevo, nunca a medias.
    std::filesystem::rename(temporal, destino, code);
    if (code) {
        // El temporal se borra: no queda tirado.
        std::error_code errorAlBorrar;
        std::filesystem::remove(temporal, errorAlBorrar);
        error = "no se pudo reemplazar '" + destino.string() + "': " + code.message();
        return false;
    }

    return true;
}

// Recarga UN repositorio.
//
// Devuelve false si fallo, y `error` dice por que. NUNCA lanza: un
// repositorio que no se puede recargar no puede tapar a los otros dos.
template <class R>
bool recargar(R& repositorio, std::string& error) {
    try {
        repositorio.cargar();
        return true;
    } catch (const ExcepcionSistema& excepcion) {
        error = excepcion.what();
    } catch (const std::exception& excepcion) {
        error = excepcion.what();
    } catch (...) {
        error = "no se pudo recargar el repositorio";
    }
    return false;
}

} // namespace

bool cargarDatosDeEjemplo(Contexto& contexto, std::string& error) {
    error.clear();

    // TODO EL CUERPO VA DENTRO DEL `try`
    //
    // El helper promete NUNCA LANZAR: el `catch` global de `main` solo
    // catchea `ExcepcionSistema`, asi que una `filesystem_error` o un
    // `bad_alloc` escaparian y el proceso moriria con SIGABRT en vez de con
    // codigo 0. Armar las rutas y copiar los archivos tambien pueden fallar
    // (una ruta invalida, un disco lleno), asi que nada de eso queda fuera.
    try {
        // LOS ARCHIVOS VIVOS
        //
        // Los destinos salen de los repositorios del contexto: son los
        // archivos que la aplicacion tiene abiertos, y son los que hay que
        // pisar.
        const std::vector<std::filesystem::path> destinos = {
            std::filesystem::path(contexto.pacientes().ruta()),
            std::filesystem::path(contexto.medicos().ruta()),
            std::filesystem::path(contexto.turnos().ruta())
        };

        // PASO 1: COPIAR
        //
        // Se copian los tres. Si uno falla, se siguen con los otros: el error
        // se reporta al final y la recarga se hace igual, asi la memoria y el
        // disco quedan de acuerdo (que es lo que el usuario ve).
        bool copiaSalioBien = true;
        std::string primerErrorDeCopia;
        for (const std::filesystem::path& destino : destinos) {
            std::string errorDeCopia;
            if (!copiarSobre(semillaDe(destino), destino, errorDeCopia)) {
                copiaSalioBien = false;
                if (primerErrorDeCopia.empty()) {
                    primerErrorDeCopia = errorDeCopia;
                }
            }
        }

        // PASO 2: RECARGAR
        //
        // La recarga va DESPUES de la copia, siempre: `cargar()` sobre un
        // archivo inexistente lo crea VACIO y borra el arbol en memoria.
        //
        // Se recarga EN EL LUGAR: el mismo objeto que las pantallas tienen por
        // referencia, asi que ven los datos nuevos sin enterarse.
        //
        // CADA REPOSITORIO SE RECARGA POR SEPARADO
        //
        // Si uno falla, los otros dos tienen que recargar IGUAL. Si no, el
        // que falla deja a los otros dos con los datos VIEJOS en memoria, y
        // la proxima `persistir()` de un alta pisaria el archivo nuevo con la
        // memoria vieja: la carga se revertiria en silencio.
        bool recargaSalioBien = true;
        std::string primerErrorDeRecarga;
        std::string errorDeRecarga;

        if (!recargar(contexto.pacientes(), errorDeRecarga)) {
            recargaSalioBien = false;
            primerErrorDeRecarga = errorDeRecarga;
        }
        errorDeRecarga.clear();
        if (!recargar(contexto.medicos(), errorDeRecarga)) {
            recargaSalioBien = false;
            if (primerErrorDeRecarga.empty()) {
                primerErrorDeRecarga = errorDeRecarga;
            }
        }
        errorDeRecarga.clear();
        if (!recargar(contexto.turnos(), errorDeRecarga)) {
            recargaSalioBien = false;
            if (primerErrorDeRecarga.empty()) {
                primerErrorDeRecarga = errorDeRecarga;
            }
        }

        // Si ALGO fallo, la carga NO se da por buena: se informa el error.
        if (!copiaSalioBien || !recargaSalioBien) {
            error = !primerErrorDeCopia.empty() ? primerErrorDeCopia : primerErrorDeRecarga;
            return false;
        }

        return true;
    } catch (const ExcepcionSistema& excepcion) {
        error = excepcion.what();
    } catch (const std::exception& excepcion) {
        error = excepcion.what();
    } catch (...) {
        error = "no se pudieron cargar los datos de ejemplo";
    }
    return false;
}

} // namespace ui
