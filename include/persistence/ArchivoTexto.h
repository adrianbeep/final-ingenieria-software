#pragma once

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <functional>
#include <memory>
#include <ostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// ArchivoTexto<T>: la CAPA DE PERSISTENCIA en archivos de texto.
//
// Esta clase sabe de archivos y de formatos. NO sabe de reglas de negocio, ni
// de `ArbolBST`, ni de que es un paciente. Por eso el dominio (`Persona`,
// `Medico`, `Paciente`, `Turno`) no tiene una sola linea de `fstream`.
//
// ---------------------------------------------------------------------------
// ANATOMIA DEL ARCHIVO
//
// Un archivo = 1 header + 1 linea de esquema + N lineas de registro:
//
//     #PACIENTES|2|2
//     id|nombre|apellido|email|telefono|activo|dni
//     1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|2020391
//     7|Lopez|Martin|martin.lopez@unlp.edu.ar|2214889900|1|2020398
//
// El header es POSICIONAL (la primera linea SIEMPRE es el header): es mas
// simple y no depende de que ningun dato pueda parecerse a un header.
//
// El `2` del header es la VERSION DEL FORMATO (no la del programa). La
// version 2 es la del esquema medico: el `dni` del paciente es NUMERICO
// (en la version 1 era una matricula alfanumerica) y la especialidad se
// guarda POR CODIGO ("CARD"), no por descripcion. Un archivo de version 1
// se RECHAZA: el loader no adivina como migrarlo, dice que version es y
// deja la decision al usuario.
//
// ---------------------------------------------------------------------------
// POR QUE TEXTO DELIMITADO POR '|' Y NO BINARIO
//
// `fwrite(&registro, sizeof(registro), 1, f)` esta mal si el registro tiene
// `std::string`: `sizeof(std::string)` mide el OBJETO (32 bytes en libstdc++:
// puntero + tamaño + buffer inline), no los caracteres. Escribis basura.
// Ademas las tildes desalinean los registros de longitud fija ("Jose" son 5
// bytes y 4 caracteres) y agregar un campo invalida TODOS los archivos
// anteriores. En texto, una linea con un campo mas se parsea sin drama.
//
// ---------------------------------------------------------------------------
// EL CONTRATO DEL TIPO `T`
//
// Para que `ArchivoTexto<T>` funcione, `T` tiene que ofrecer exactamente
// esto:
//
//     friend std::ostream& operator<<(std::ostream&, const T&);  // escribe el registro
//     static T fromRegistro(const std::string& linea);            // parsea el registro
//     static std::string esquema();                               // "id|nombre|..."
//
// Y nada mas. Si `T` necesita mas, el formato no esta bien disenado: esa
// es la prueba de que el dominio esta desacoplado del storage.
//
// ---------------------------------------------------------------------------
// POR QUE ES HEADER-ONLY
//
// `ArchivoTexto<T>` es un TEMPLATE: sin conocer `T` no se puede compilar, asi
// que no hay `.cpp` que compilar. Vive entero en el header, como
// `ArbolBST<T>` y `ListaEnlazada<T>`.
//
// ---------------------------------------------------------------------------
// POR QUE `std::filesystem::rename` Y NO `std::rename`
//
// En Windows, `std::rename` (el de `<cstdio>`) FALLA si el destino existe.
// `std::filesystem::rename` esta especificado para reemplazar el destino y en
// MSVC/libstdc++ lo hace con `MoveFileEx(MOVEFILE_REPLACE_EXISTING)`. Es un
// detalle que rompe en silencio en Windows y funciona en Linux.
//
// ---------------------------------------------------------------------------
// EL `.tmp` VA EN EL MISMO DIRECTORIO
//
// El rename solo es atomico dentro del mismo volumen: si el `.tmp` estuviera
// en otra particion, el "rename" seria un copiar+borrar y perderias la
// atomicidad.
//
// ---------------------------------------------------------------------------
// EL NOMBRE DEL TEMPORAL TIENE UN LARGO ACOTADO
//
// Los sistemas de archivos limitan el largo de un COMPONENTE de ruta (255
// bytes en ext4 y en NTFS). Como el temporal se llama `<destino>.<token>.tmp`,
// un destino con el nombre al limite de lo permitido dejaria el temporal
// fuera del limite: la apertura falla y `guardar()` reporta un error sobre un
// destino que, en realidad, se puede escribir perfecto.
//
// Por eso el temporal NO se arma pegando el token al final del nombre del
// destino. Se arma dentro del directorio del destino (mismo volumen, asi que
// el rename sigue siendo atomico) con un nombre ACOTADO: se recorta el
// nombre del destino lo que haga falta y se le agrega el token unico. El
// resultado siempre entra, y el temporal sigue siendo unico.
//
// ---------------------------------------------------------------------------
// LO QUE ESTA CAPA NO HACE
//
//   - No filtra inactivos: `cargar()` trae todos los registros. El `activo`
//     es un DATO, no una condicion de carga (si no, las bajas logicas
//     reaparecerian al reiniciar).
//   - No valida reglas de negocio: que dos turnos se solapen es cosa del
//     servicio (Fase 5). Esta capa valida FORMA, no SEMANTICA.
//   - No deduce ni completa: un `idMedico` que no exista en
//     `medicos.dat` se carga igual.
//
// ---------------------------------------------------------------------------
// CONVENCION DE NOMBRES
//
// Los parametros llevan sufijo `_`. Los miembros llevan PREFIJO `_` porque los
// accesores publicos se llaman `ruta()` y `nombreEntidad()`: un miembro no
// puede llamarse igual que una funcion de la misma clase. Es el mismo
// criterio que usa `DatoInvalidoException` con `campo_` y `valor_`.
//
// EL NOMBRE DE LA ENTIDAD SE PASA EN MAYUSCULAS ("PACIENTES", "MEDICOS",
// "TURNOS"): es el magic del header. NO es mecanico a partir de `getTipo()`:
// `getTipo()` devuelve "Paciente" y "Medico", y el plural no sale de ahi
// ("Medico" -> "Medicos", no "MEDICOS").
// ---------------------------------------------------------------------------
// ESTADO COMPARTIDO DE LOS NOMBRES TEMPORALES
//
// `ArchivoTexto<T>` es un TEMPLATE, asi que si el estado de unicidad viviera
// en un miembro del template, existiria UNA COPIA POR INSTANTIACION:
// `ArchivoTexto<Paciente>`, `ArchivoTexto<Medico>` y `ArchivoTexto<Turno>`
// tendrian cada una su propio contador (empezando en 0) y su propio
// `std::random_device`. Dos escrituras de DISTINTOS tipos de entidad
// podrian entonces derivar el mismo nombre de temporal.
//
// Por eso el estado vive ACA, en una clase que NO es template: hay UN solo
// contador y UN solo generador para todo el programa.
//
// Es HEADER-ONLY seguro: el estado son `static` locales de funcion dentro de
// una funcion `static` de una clase no template. En C++17 hay exactamente una
// copia de un `static` local de funcion, aunque la funcion se defina en
// varios archivos, asi que no hay riesgo de definicion multiple al linkear.
// ---------------------------------------------------------------------------
class GeneradorDeNombresTemporales {
public:
    // Largo MAXIMO del token unico (en caracteres). Es un techo, no un ancho
    // fijo: el token real puede ser mas corto.
    static constexpr std::size_t LARGO_MAXIMO_TOKEN = 32;

    // Devuelve el siguiente token unico del proceso.
    //
    // TRES fuentes de unicidad, todas del estandar (nada de POSIX):
    //   1. `random_device`: entropia del sistema (diferente por proceso).
    //   2. un contador atomico: Diferente que dos escrituras del MISMO
    //      proceso no puedan obtener el mismo numero.
    //   3. el hash del hilo: dos hilos del mismo proceso tampoco chocan.
    //
    // NO LANZA NUNCA: si la fuente de entropia no esta disponible (el
    // `std::random_device` esta especificado para lanzar `std::runtime_error`
    // cuando el sistema no puede darle entropia), se degrada con elegancia al
    // contador atomico mas el hash del hilo, que ya dan la unicidad dentro
    // del proceso.
    static std::string siguienteToken() {
        // Unica vez en todo el programa: los `static` locales de funcion
        // tienen UNA sola instancia, compartida por todos los tipos `T`.
        static std::atomic<unsigned long long> contador(0);

        const unsigned long long numero = ++contador;
        const std::hash<std::thread::id> hashHilo;
        const unsigned long long hashDelHilo =
            static_cast<unsigned long long>(hashHilo(std::this_thread::get_id()));

        // La semilla sale del generador de entropia. Se construye y se usa
        // DENTRO del `try`: tanto el constructor del `std::random_device`
        // como su `operator()` estan especificados para lanzar
        // `std::runtime_error` cuando la fuente de entropia no esta
        // disponible, y el constructor no puede quedar fuera del `try`.
        //
        // El generador vive en un `unique_ptr` para que la inicializacion
        // (que puede lanzar) quede dentro del `try` y no en la inicializacion
        // estatica de la funcion.
        static std::unique_ptr<std::random_device> generador = crearGenerador();

        std::ostringstream nombre;
        if (generador) {
            try {
                nombre << std::hex << static_cast<unsigned long long>((*generador)()) << '-';
            } catch (const std::exception&) {
                // Sin entropia: se sigue igual (ver la nota de abajo).
            } catch (...) {
            }
        }
        nombre << std::hex << numero << '-' << hashDelHilo;

        std::string token = nombre.str();
        if (token.size() > LARGO_MAXIMO_TOKEN) {
            // No deberia pasar (los tres componentes son cortos), pero el
            // techo se RESPETA igual: el nombre del temporal tiene un largo
            // acotado por construccion.
            token.resize(LARGO_MAXIMO_TOKEN);
        }
        return token;
    }

private:
    // Crea el generador de entropia. Si el sistema no puede darle entropia,
    // devuelve un puntero NULO y el llamador sigue con el contador atomico
    // mas el hash del hilo.
    //
    // Sin entropia NO se pierde ninguna garantia: el contador atomico mas el
    // hash del hilo dan unicidad dentro del proceso, que es lo que protege
    // el rename atomico. La entropia solo agrega que dos PROCESOS distintos
    // no se pisen entre si.
    static std::unique_ptr<std::random_device> crearGenerador() {
        try {
            return std::unique_ptr<std::random_device>(new std::random_device());
        } catch (const std::exception&) {
            return std::unique_ptr<std::random_device>();
        } catch (...) {
            return std::unique_ptr<std::random_device>();
        }
    }
};

// ---------------------------------------------------------------------------
template <class T>
class ArchivoTexto {
public:
    // Version del FORMATO DE ARCHIVO (no la del programa). Vive en el
    // header: por eso el loader puede decir con precision que paso cuando
    // el archivo no se puede leer.
    //
    // La version 2 es la del ESQUEMA MEDICO: el `dni` del `Paciente` es
    // NUMERICO (en la version 1 era una matricula alfanumerica) y la
    // especialidad se guarda POR CODIGO ("CARD"), no por descripcion.
    //
    // Un archivo de version 1 se RECHAZA con un error claro: el loader dice
    // las DOS versiones (la del archivo y la que lee) en vez de adivinar
    // como migrarlo.
    static constexpr int VERSION_FORMATO = 2;

    explicit ArchivoTexto(std::string ruta_, std::string nombreEntidad_);

    // Escribe TODOS los registros, de forma ATOMICA. Reemplaza el archivo
    // completo: no agrega ni modifica lo que hubiera.
    void guardar(const std::vector<T>& registros);

    // Lee y valida. Lanza `ArchivoNoEncontradoException` si el archivo no
    // existe, `FormatoDesconocidoException` si el formato no calza y
    // `DatoInvalidoException` si un registro esta mal.
    std::vector<T> cargar();

    // Crea el archivo con el header y la linea de esquema, SIN registros.
    // No hace nada si el archivo ya existe (no pisa datos).
    void crearVacio();

    // Ruta y entidad, por si el llamador quiere mostrarlos en un mensaje.
    const std::string& ruta() const;
    const std::string& nombreEntidad() const;

private:
    // Escribe UN registro canonico con el `operator<<` del tipo `T`.
    void escribirRegistro(std::ostream& out, const T& registro) const;

    // Parsea UN registro canonico. `numeroLinea` es el numero de linea REAL
    // del archivo (1 = header, 2 = esquema): va en el mensaje de error para
    // que el error se ubique sin debuggear a ciegas.
    T leerRegistro(const std::string& linea, int numeroLinea) const;

    // "#ENTIDAD|VERSION|CANTIDAD_REGISTROS".
    std::string construirHeader(std::size_t cantidadRegistros) const;

    // Convierte un texto a entero. Devuelve -1 si no es un entero valido
    // (incluye el desborde de rango: "99999999999999999999" NO es un header
    // valido, y `std::stoll` lanzaria `std::out_of_range`).
    static long long enteroEstricto(const std::string& texto);

    // Nombre UNICO del archivo temporal de una escritura.
    //
    // El temporal va en el MISMO directorio del destino (requisito del
    // rename atomico) y con el SUFIJO `.tmp` (asi lo cubre el `.gitignore`).
    // Lo que cambia es la parte de en medio: dos escritores del mismo
    // destino en el mismo directorio NO pueden compartir el temporal, o uno
    // publicaria el archivo a medio escribir del otro.
    //
    // La unicidad NO depende de APIs POSIX: se arma con un `std::random_device`
    // (entropia del sistema) mas un contador atomico (unicidad dentro del
    // proceso, incluso entre hilos) mas el hash del hilo actual. El estado es
    // COMPARTIDO por todos los tipos `T` (ver
    // `GeneradorDeNombresTemporales`).
    //
    // NO LANZA NUNCA.
    static std::string nombreTemporalUnico();

    // Ruta del archivo temporal de una escritura, lista para usar.
    //
    // Propiedades que garantiza:
    //   - vive en el MISMO DIRECTORIO que el destino (el rename atomico solo
    //     funciona dentro del mismo volumen);
    //   - termina en `.tmp` (asi lo cubre el `.gitignore`);
    //   - el nombre del componente tiene un largo ACOTADO (dentro del limite
    //     que imponen los sistemas de archivos), sea cual sea el largo del
    //     nombre del destino;
    //   - es UNICO por escritura: dos escrituras del mismo destino en el
    //     mismo directorio nunca comparten el temporal.
    static std::filesystem::path rutaTemporalEnMismoDirectorio(const std::filesystem::path& destino);

    // Cantidad de campos del header: ENTIDAD, VERSION y CANTIDAD.
    static constexpr std::size_t CANTIDAD_CAMPOS_HEADER = 3;

    std::string _ruta;
    std::string _nombreEntidad;
};

// ---------------------------------------------------------------------------
// Implementaciones (van en el header: es un template).
// ---------------------------------------------------------------------------

template <class T>
ArchivoTexto<T>::ArchivoTexto(std::string ruta_, std::string nombreEntidad_)
    : _ruta(std::move(ruta_)), _nombreEntidad(std::move(nombreEntidad_)) {
}

template <class T>
void ArchivoTexto<T>::guardar(const std::vector<T>& registros) {
    // --- PASO 1: serializar TODO en memoria ---
    // Si algo lanza (un dato invalido), NO se toco el disco: el archivo
    // original sigue intacto. Por eso el buffer va antes de abrir nada.
    std::ostringstream buffer;
    buffer << construirHeader(registros.size()) << '\n';
    buffer << T::esquema() << '\n';
    for (const T& registro : registros) {
        escribirRegistro(buffer, registro);
        buffer << '\n';
    }

    // --- PASO 2: escribir el `.tmp`, en el MISMO directorio del destino ---
    //
    // TODO lo que arma el temporal (la conversion de la ruta a
    // `std::filesystem::path` y el calculo del nombre) va DENTRO del `try`:
    // cualquiera de esas operaciones puede lanzar una excepcion de la
    // biblioteca estandar (por ejemplo, `std::filesystem::filesystem_error`
    // si la ruta no se puede convertir al formato del sistema de archivos) y
    // esa excepcion NO es una `ExcepcionSistema`.
    std::filesystem::path destino;
    std::filesystem::path temporal;
    try {
        destino = std::filesystem::path(_ruta);

        // --- PASO 2.0: armar el nombre del temporal ---
        //
        // El temporal es UNICO por escritura (ver `nombreTemporalUnico`), pero
        // mantiene el sufijo `.tmp` y vive en el mismo directorio: el rename
        // atomico solo funciona dentro del mismo volumen.
        //
        // El nombre se arma con el nombre del DESTINO (para que se entienda
        // cual es) mas el token unico, y su largo esta ACOTADO: los sistemas de
        // archivos limitan el largo de un componente de ruta (255 bytes en ext4 y
        // en NTFS) y, sin ese tope, un destino con el nombre largo dejaria al
        // temporal fuera del limite. La apertura fallaria y el guardado reportaria
        // un error sobre un destino que se puede escribir perfecto.
        //
        // Si el temporal no se puede armar, el error se reporta como
        // `ExcepcionSistema` (NO como una excepcion cruda de la biblioteca
        // estandar) y el destino NO se toca.
        temporal = rutaTemporalEnMismoDirectorio(destino);
    } catch (const ExcepcionSistema&) {
        // Ya es un error del sistema: se respeta tal cual.
        throw;
    } catch (const std::exception& error) {
        // RED DE SEGURIDAD: cualquier excepcion de la biblioteca estandar
        // que se escape del armado del temporal se traduce al error del
        // sistema, que si dice QUE archivo fallo.
        throw ExcepcionSistema("No se pudo preparar el archivo temporal para '"
                                + _ruta + "': " + error.what());
    } catch (...) {
        throw ExcepcionSistema("No se pudo preparar el archivo temporal para '"
                                + _ruta + "'");
    }

    {
        // `trunc` explicito: guardar REEMPLAZA el archivo, no lo agrega al
        // final (el bug clasico del `ofstream` mal abierto).
        //
        // `binary` explicito: en modo TEXTO el runtime traduce cada '\n' a
        // CRLF en Windows, asi que los bytes dependen de la plataforma. En
        // modo BINARIO el '\n' se escribe tal cual (LF) en todas partes y el
        // archivo generado es IDENTICO byte a byte en cualquier SO.
        std::ofstream salida(temporal, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!salida.is_open()) {
            throw ExcepcionSistema("No se pudo abrir el archivo temporal '" + temporal.string() + "'");
        }

        salida << buffer.str();
        salida.flush();
        if (!salida) {
            // Error de escritura (disco lleno, permisos): el `.tmp` no sirve
            // para nada, asi que se borra y se reporta.
            salida.close();
            std::error_code errorAlBorrar;
            std::filesystem::remove(temporal, errorAlBorrar);
            throw ExcepcionSistema("No se pudo escribir el archivo temporal '" + temporal.string() + "'");
        }

        // --- PASO 2.1: el `close()` EXPLICITO y su resultado ---
        // El `close()` es el que de verdad manda los bytes al sistema de
        // archivos, y es donde aparecen los errores diferidos (cuota, error de
        // E/S del dispositivo). Si se deja al destructor, ese error llega
        // tarde y el rename publicaria un archivo a medio escribir. Por eso
        // se cierra explicitamente y se CHECKEA: si falla, el destino NO se
        // toca (sigue con los datos viejos) y el `.tmp` se borra.
        salida.close();
        if (salida.fail()) {
            std::error_code errorAlBorrar;
            std::filesystem::remove(temporal, errorAlBorrar);
            throw ExcepcionSistema("No se pudo cerrar el archivo temporal '" + temporal.string() + "'");
        }
    }

    // --- PASO 3: el rename ATOMICO ---
    // `std::filesystem::rename` reemplaza el destino (a diferencia de
    // `std::rename`, que falla en Windows si el destino existe).
    std::error_code error;
    std::filesystem::rename(temporal, destino, error);
    if (error) {
        // El rename fallo (permisos, disco lleno, el destino esta abierto por
        // otro proceso): se borra el `.tmp` y el archivo viejo sigue como
        // estaba.
        std::error_code errorAlBorrar;
        std::filesystem::remove(temporal, errorAlBorrar);
        // El mensaje usa `_ruta` (la ruta tal como la dio el llamador) y no
        // `destino.string()`: la conversion de un `std::filesystem::path` a
        // texto puede fallar y no hace falta arriesgar eso dentro del
        // mensaje de un error.
        throw ExcepcionSistema("No se pudo reemplazar el archivo '" + _ruta + "': "
                                + error.message());
    }
}

template <class T>
std::vector<T> ArchivoTexto<T>::cargar() {
    std::error_code error;
    if (!std::filesystem::exists(_ruta, error)) {
        throw ArchivoNoEncontradoException(_ruta);
    }

    std::ifstream entrada(_ruta, std::ios::in);
    if (!entrada.is_open()) {
        throw ArchivoNoEncontradoException(_ruta);
    }

    std::string linea;

    // --- PASO 1: el header ---
    if (!std::getline(entrada, linea)) {
        throw FormatoDesconocidoException(_ruta + ": el archivo esta vacio (falta el header)");
    }
    if (linea.empty() || linea[0] != '#') {
        throw FormatoDesconocidoException(_ruta
                                          + ": falta el header (la primera linea deberia empezar con '#')");
    }

    const std::vector<std::string> partes = Utils::separar(linea, '|');
    if (partes.size() != CANTIDAD_CAMPOS_HEADER) {
        throw FormatoDesconocidoException(
            _ruta + ": el header deberia tener la forma '#ENTIDAD|VERSION|CANTIDAD'");
    }

    const std::string magicEsperado = "#" + _nombreEntidad;
    if (partes[0] != magicEsperado) {
        // El nombre de la entidad del archivo (sin el '#') se muestra en el
        // mensaje: "es un archivo de TURNOS y se esperaba uno de PACIENTES".
        const std::string entidadDelArchivo =
            partes[0].size() > 1 ? partes[0].substr(1) : partes[0];
        throw FormatoDesconocidoException(_ruta + ": es un archivo de " + entidadDelArchivo
                                          + " y se esperaba uno de " + _nombreEntidad);
    }

    const long long version = enteroEstricto(partes[1]);
    if (version < 0) {
        throw FormatoDesconocidoException(_ruta + ": la version del header no es un numero: '"
                                          + partes[1] + "'");
    }
    if (version != VERSION_FORMATO) {
        throw FormatoDesconocidoException(_ruta
                                          + ": el archivo fue creado por otra version del programa "
                                            "(formato v"
                                          + std::to_string(version) + ", esta version lee v"
                                          + std::to_string(VERSION_FORMATO) + ")");
    }

    const long long cantidadDeclarada = enteroEstricto(partes[2]);
    if (cantidadDeclarada < 0) {
        throw FormatoDesconocidoException(_ruta
                                          + ": la cantidad de registros del header no es un numero: '"
                                          + partes[2] + "'");
    }

    // --- PASO 2: la linea de esquema ---
    // Es el checksum de la FORMA del registro: si el archivo declara 8 campos
    // y el codigo espera 9, el error dice exactamente eso en vez de
    // desalinear los datos en silencio.
    if (!std::getline(entrada, linea)) {
        throw FormatoDesconocidoException(_ruta + ": falta la linea de esquema (la segunda linea)");
    }
    const std::string esquemaArchivo = Utils::trim(linea);
    const std::string esquemaCodigo = T::esquema();
    if (esquemaArchivo != esquemaCodigo) {
        throw FormatoDesconocidoException(_ruta + ": el archivo declara el esquema '" + esquemaArchivo
                                          + "' y el codigo espera '" + esquemaCodigo + "'");
    }

    // --- PASO 3: los registros ---
    std::vector<T> registros;
    int numeroLinea = 2;  // la linea 1 es el header y la 2 el esquema
    while (std::getline(entrada, linea)) {
        ++numeroLinea;
        // Linea vacia: se ignora (tolerancia a los archivos editados a mano).
        if (Utils::trim(linea).empty()) {
            continue;
        }
        registros.push_back(leerRegistro(linea, numeroLinea));
    }

    // --- PASO 4: la cantidad declarada tiene que coincidir ---
    // Si no coincide, hay DOS casos opuestos y el mensaje tiene que decir
    // CUAL de los dos es: decir siempre "truncado" cuando sobran registros
    // seria mentir sobre el archivo.
    if (static_cast<long long>(registros.size()) != cantidadDeclarada) {
        const std::string detalle = "el header declara " + std::to_string(cantidadDeclarada)
            + " registro(s) y se leyeron " + std::to_string(registros.size());
        if (static_cast<long long>(registros.size()) < cantidadDeclarada) {
            // Faltan registros: el archivo esta truncado.
            throw DatoInvalidoException("archivo", _ruta, "archivo truncado: " + detalle);
        }
        // Sobran registros: el archivo tiene mas de los que declara.
        throw DatoInvalidoException("archivo", _ruta,
                                    "el archivo tiene mas registros que los declarados: " + detalle);
    }

    return registros;
}

template <class T>
void ArchivoTexto<T>::crearVacio() {
    // No pisa un archivo que ya existe: `crearVacio` es el arranque en frio
    // (cuando el archivo NO deberia estar), no un reset.
    std::error_code error;
    if (std::filesystem::exists(_ruta, error)) {
        return;
    }
    guardar(std::vector<T>());
}

template <class T>
const std::string& ArchivoTexto<T>::ruta() const {
    return _ruta;
}

template <class T>
const std::string& ArchivoTexto<T>::nombreEntidad() const {
    return _nombreEntidad;
}

template <class T>
void ArchivoTexto<T>::escribirRegistro(std::ostream& out, const T& registro) const {
    // El `operator<<` del tipo `T` escribe el registro canonico. Se resuelve
    // por ADL: es la funcion friend que la entidad declara.
    out << registro;
}

template <class T>
T ArchivoTexto<T>::leerRegistro(const std::string& linea, int numeroLinea) const {
    // La cantidad de campos se cuenta sobre el ESQUEMA (los nombres de los
    // campos), no sobre su longitud.
    const std::size_t camposEsperados = Utils::separar(T::esquema(), '|').size();
    const std::vector<std::string> campos = Utils::separar(linea, '|');
    if (campos.size() != camposEsperados) {
        throw DatoInvalidoException("registro", linea,
                                    _ruta + " linea " + std::to_string(numeroLinea)
                                        + ": se esperaban " + std::to_string(camposEsperados)
                                        + " campos y hay " + std::to_string(campos.size()));
    }

    try {
        return T::fromRegistro(linea);
    } catch (const DatoInvalidoException& error) {
        // El dominio no sabe de lineas ni de archivos: la capa de archivo es
        // la que agrega ese contexto y re-lanza. Se conserva el campo y el
        // valor que fallaron (el dominio los diagnostico bien) y se agrega el
        // archivo y la linea (los que solo la capa de archivo sabe).
        throw DatoInvalidoException(error.campo(), error.valor(),
                                    _ruta + " linea " + std::to_string(numeroLinea) + ": "
                                        + error.motivo());
    } catch (const ExcepcionSistema& error) {
        throw DatoInvalidoException("registro", linea,
                                    _ruta + " linea " + std::to_string(numeroLinea) + ": "
                                        + error.what());
    } catch (const std::exception& error) {
        // RED DE SEGURIDAD: cualquier otra excepcion de la biblioteca
        // estandar (por ejemplo un `std::out_of_range` de un `std::stoi`
        // que el dominio no haya traduido) se traduce igual. El contrato de
        // `cargar()` es que un registro mal formado NUNCA escapa como
        // excepcion cruda: siempre dice que archivo es y en que linea esta.
        throw DatoInvalidoException("registro", linea,
                                    _ruta + " linea " + std::to_string(numeroLinea) + ": "
                                        + error.what());
    }
}

template <class T>
std::string ArchivoTexto<T>::construirHeader(std::size_t cantidadRegistros) const {
    return "#" + _nombreEntidad + "|" + std::to_string(VERSION_FORMATO) + "|"
        + std::to_string(cantidadRegistros);
}

template <class T>
long long ArchivoTexto<T>::enteroEstricto(const std::string& texto) {
    if (!Utils::esEntero(texto)) {
        return -1;
    }
    try {
        return std::stoll(texto);
    } catch (const std::out_of_range&) {
        return -1;  // desborde de rango: no es un header valido
    } catch (const std::invalid_argument&) {
        return -1;
    }
}

template <class T>
std::string ArchivoTexto<T>::nombreTemporalUnico() {
    // Delega en el generador COMPARTIDO (ver
    // `GeneradorDeNombresTemporales`): asi el contador y el
    // `std::random_device` son UNOS SOLOS para todo el programa y no uno por
    // cada instanciacion del template.
    //
    // Esta funcion NO LANZA NUNCA: `siguienteToken()` ya degrada con
    // elegancia si la fuente de entropia no esta disponible. El `catch` de
    // abajo es la ultima red: si alguna vez se escapara una excepcion de la
    // biblioteca estandar, se reintenta una vez (que es el camino que el
    // generador ya sabe recorrer sin fallar) y, si tampoco, se devuelve un
    // token de repuesto derivado del reloj. Nunca se propaga.
    try {
        return GeneradorDeNombresTemporales::siguienteToken();
    } catch (...) {
        try {
            return GeneradorDeNombresTemporales::siguienteToken();
        } catch (...) {
            // Token de repuesto: el reloj del sistema mas el hilo actual.
            // Es la ULTIMA red: en la practica no se llega aca, porque
            // `siguienteToken()` ya no puede lanzar.
            const std::hash<std::thread::id> hashHilo;
            const unsigned long long marcaDelReloj =
                static_cast<unsigned long long>(
                    std::chrono::steady_clock::now().time_since_epoch().count());
            const unsigned long long hashDelHilo =
                static_cast<unsigned long long>(hashHilo(std::this_thread::get_id()));

            std::ostringstream nombre;
            nombre << std::hex << marcaDelReloj << '-' << hashDelHilo;

            std::string token = nombre.str();
            if (token.size() > GeneradorDeNombresTemporales::LARGO_MAXIMO_TOKEN) {
                token.resize(GeneradorDeNombresTemporales::LARGO_MAXIMO_TOKEN);
            }
            return token;
        }
    }
}

template <class T>
std::filesystem::path ArchivoTexto<T>::rutaTemporalEnMismoDirectorio(
    const std::filesystem::path& destino) {
    // Hay DOS limites distintos y hay que respetar los DOS:
    //
    //   1. el del COMPONENTE de la ruta (el nombre del archivo): 255 bytes en
    //      ext4 y en NTFS;
    //   2. el de la RUTA COMPLETA (directorio + nombre): MAX_PATH en Windows
    //      (259 caracteres utiles, con la terminacion nula) y PATH_MAX en
    //      Linux (4096).
    //
    // El segundo es el que MATA: el temporal es siempre MAS LARGO que el
    // destino (le agrega el token y el sufijo `.tmp`), asi que acotar solo el
    // componente NO alcanza. Con un directorio de 50 caracteres y un destino
    // de 190, el destino entra en MAX_PATH pero el temporal se pasa en 20
    // caracteres, la apertura falla y `guardar()` reporta un error sobre un
    // destino que se puede escribir perfectamente.
    //
    // Por eso el presupuesto se calcula sobre la RUTA COMPLETA. El tope de
    // 255 deja margen bajo los 259 caracteres utiles de MAX_PATH.
    constexpr std::size_t LARGO_MAXIMO_COMPONENTE = 255;
    constexpr std::size_t LARGO_MAXIMO_RUTA = 255;

    const std::string token = nombreTemporalUnico();

    // El nombre del temporal es `<nombre-del-destino>.<token>.tmp`.
    //
    // El SUFIJO `.tmp` se conserva (asi lo cubre el `.gitignore`) y el token
    // ya viene acotado.
    const std::string sufijo = "." + token + ".tmp";

    // Se usa el NOMBRE COMPLETO del destino (con su extension) para que el
    // temporal se parezca a el y se entienda cual es.
    std::string base = destino.filename().string();
    if (base.empty()) {
        // Un destino sin nombre de archivo (por ejemplo, una ruta que termina
        // en un separador) usa una base neutra.
        base = "archivo";
    }

    // El presupuesto del nombre es el MINIMO de los dos limites:
    //   - lo que sobra del limite de componente;
    //   - lo que sobra del limite de RUTA COMPLETA, que es el que manda:
    //     se resta el directorio (y su separador) del total.
    std::size_t presupuesto = LARGO_MAXIMO_COMPONENTE > sufijo.size()
        ? LARGO_MAXIMO_COMPONENTE - sufijo.size()
        : 1;

    const std::string directorio = destino.parent_path().string();
    if (!directorio.empty()) {
        // `directorio` + separador + nombre <= LARGO_MAXIMO_RUTA.
        const std::size_t rutaParcial = directorio.size() + 1 + sufijo.size();
        if (rutaParcial < LARGO_MAXIMO_RUTA) {
            const std::size_t presupuestoPorRuta = LARGO_MAXIMO_RUTA - rutaParcial;
            if (presupuestoPorRuta < presupuesto) {
                presupuesto = presupuestoPorRuta;
            }
        } else {
            // El directorio, con el token y el sufijo pegados, ya se pasa del
            // limite de ruta. NO se puede llamar a `guardar()` con un destino
            // asi sin que el temporal sea mas largo todavia: el temporal es
            // SIEMPRE mas largo que el destino.
            //
            // Que se llegue aca significa que el DIRECTORIO es tan largo que
            // no entra ningun nombre de archivo con el sufijo `.tmp`. El
            // destino tampoco va a entrar. Se deja el presupuesto minimo (un
            // caracter) y se deja que la apertura falle con su mensaje
            // claro, en vez de acortar el token y romper la unicidad.
            presupuesto = 1;
        }
    }
    if (presupuesto < 1) {
        presupuesto = 1;
    }

    if (base.size() > presupuesto) {
        // El nombre del destino NO entra con el token pegado: se recorta.
        // Se conserva el COMIENZO del nombre (que es lo que identifica al
        // archivo) y se le agrega el token, que ya es unico: dos escrituras
        // del mismo destino largo siguen teniendo temporales distintos.
        base.resize(presupuesto);
    }

    // El DIRECTORIO es el del destino: el rename atomico solo funciona
    // dentro del mismo volumen.
    return destino.parent_path() / (base + sufijo);
}
