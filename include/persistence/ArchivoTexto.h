#pragma once

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// ArchivoTexto<T>: la CAPA DE PERSISTENCIA en archivos de texto.
//
// Esta clase sabe de archivos y de formatos. NO sabe de reglas de negocio, ni
// de `ArbolBST`, ni de que es un alumno. Por eso el dominio (`Persona`,
// `Profesor`, `Alumno`, `Turno`) no tiene una sola linea de `fstream`.
//
// ---------------------------------------------------------------------------
// ANATOMIA DEL ARCHIVO
//
// Un archivo = 1 header + 1 linea de esquema + N lineas de registro:
//
//     #ALUMNOS|1|2
//     id|nombre|apellido|email|telefono|activo|matricula
//     1|Garcia|Ana|ana.garcia@unlp.edu.ar|2214556677|1|MAT-20391
//     7|Lopez|Martin|martin.lopez@unlp.edu.ar|2214889900|1|MAT-20398
//
// El header es POSICIONAL (la primera linea SIEMPRE es el header): es mas
// simple y no depende de que ningun dato pueda parecerse a un header.
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
// LO QUE ESTA CAPA NO HACE
//
//   - No filtra inactivos: `cargar()` trae todos los registros. El `activo`
//     es un DATO, no una condicion de carga (si no, las bajas logicas
//     reaparecerian al reiniciar).
//   - No valida reglas de negocio: que dos turnos se solapen es cosa del
//     servicio (Fase 5). Esta capa valida FORMA, no SEMANTICA.
//   - No deduce ni completa: un `idProfesor` que no exista en
//     `profesores.dat` se carga igual.
//
// ---------------------------------------------------------------------------
// CONVENCION DE NOMBRES
//
// Los parametros llevan sufijo `_`. Los miembros llevan PREFIJO `_` porque los
// accesores publicos se llaman `ruta()` y `nombreEntidad()`: un miembro no
// puede llamarse igual que una funcion de la misma clase. Es el mismo
// criterio que usa `DatoInvalidoException` con `campo_` y `valor_`.
//
// EL NOMBRE DE LA ENTIDAD SE PASA EN MAYUSCULAS ("ALUMNOS", "PROFESORES",
// "TURNOS"): es el magic del header. NO es mecanico a partir de `getTipo()`:
// `getTipo()` devuelve "Alumno" y "Profesor", y el plural no sale de ahi
// ("Profesor" -> "Profesores", no "PROFESORS").
// ---------------------------------------------------------------------------
template <class T>
class ArchivoTexto {
public:
    // Version del FORMATO DE ARCHIVO (no la del programa). Vive en el
    // header: por eso el loader puede decir con precision que paso cuando
    // el archivo no se puede leer.
    static constexpr int VERSION_FORMATO = 1;

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
    const std::filesystem::path destino(_ruta);
    const std::filesystem::path temporal(destino.string() + ".tmp");

    {
        // `trunc` explicito: guardar REEMPLAZA el archivo, no lo agrega al
        // final (el bug clasico del `ofstream` mal abierto).
        std::ofstream salida(temporal, std::ios::out | std::ios::trunc);
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
    }  // <- close(): flush al sistema de archivos

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
        throw ExcepcionSistema("No se pudo reemplazar el archivo '" + destino.string() + "': "
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
        // mensaje: "es un archivo de TURNOS y se esperaba uno de ALUMNOS".
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
    // Si no coincide, el archivo esta truncado (o le sobran lineas).
    if (static_cast<long long>(registros.size()) != cantidadDeclarada) {
        throw DatoInvalidoException("archivo", _ruta,
                                    "archivo truncado: el header declara "
                                        + std::to_string(cantidadDeclarada) + " registro(s) y se leyeron "
                                        + std::to_string(registros.size()));
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
