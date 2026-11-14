#include "ui/Theme.h"

#include <cstdlib>

namespace ui {
namespace {

// ---------------------------------------------------------------------------
// LA PALETA DE UN TEMA
//
// Son los valores de los roles CON color propio de UN tema. Los roles SIN
// color propio (`ValorDeCampo` y `TablaCelda`) NO estan: se dibujan con el
// color por defecto de la terminal, asi que no hay ningun numero que guardar.
//
// QUE SIGNIFICA CADA COLOR (el contrato)
//
// Los numeros de abajo estan comentados con el SIGNIFICADO del color, no con
// su nombre: el color es la forma, el significado es el contrato. Un tono
// que se cambia tiene que poder cambiarse sin perder el significado.
//
//   - los que ACOMPAÑAN (el titulo, el realce, la etiqueta de campo) son los
//     que tienen que distinguirse del texto de alrededor;
//   - los que ACOMPAÑAN SIN IMPORTAR (el secundario, la ayuda, el borde) son
//     los que no pueden competir con el dato;
//   - los de ESTADO (activo, cancelado, reprogramado) dicen COMO ESTA la
//     fila, asi que tienen que ser distinguibles entre si.
//
// POR QUE HAY DOS JUEGOS DE VALORES Y NO UNO
//
// La paleta esta afinada para un fondo: los tonos claros se lavan en un
// terminal de fondo blanco y los oscuros desaparecen en uno de fondo negro.
// Los roles siguen siendo los mismos: lo que cambia es el numero, asi que
// ajustar un tono despues es tocar un valor.
// ---------------------------------------------------------------------------
struct Paleta {
    Color titulo;
    Color realce;
    Color secundario;
    Color etiquetaDeCampo;
    Color ayuda;
    Color exito;
    Color error;
    Color advertencia;
    Color estadoActivo;
    Color estadoCancelado;
    Color estadoReprogramado;
    Color tablaHeader;
    Color tablaBorde;
    Color calorBajo;
    Color calorMedio;
    Color calorAlto;
    Color calorMaximo;
};

// EL TEMA OSCURO
//
// Esta afinado para una terminal de FONDO OSCURO: los tonos son claros sobre
// el fondo, y los roles de exito y de error son los mas saturados, porque son
// los que tienen que saltar a la vista.
const Paleta& paletaOscura() {
    static const Paleta paleta = {
        {94, 214, 255},   // Titulo: el encabezado, bien separado del texto
        {255, 214, 102},  // Realce: lo que la pantalla trata
        {140, 150, 163},  // Secundario: ids, contadores y filas atenuadas
        {126, 168, 216},  // EtiquetaDeCampo: el nombre del campo del formulario
        {110, 118, 129},  // Ayuda: la linea de atajos del pie
        {126, 231, 135},  // Exito: la operacion salio bien
        {255, 106, 106},  // Error: la operacion fallo
        {255, 214, 102},  // Advertencia: hay que mirar pero no fallo
        {126, 231, 135},  // EstadoActivo: el turno o la persona esta vigente
        {217, 119, 118},  // EstadoCancelado: no se va a usar
        {255, 214, 102},  // EstadoReprogramado: se movera a otra fecha
        {94, 214, 255},   // TablaHeader: el rotulo de la columna
        {70, 80, 92},     // TablaBorde: el marco, sin competir con el dato
        {70, 130, 220},   // CalorBajo: la rampa del mapa de calor
        {90, 200, 140},   // CalorMedio: la rampa del mapa de calor
        {255, 200, 90},   // CalorAlto: la rampa del mapa de calor
        {240, 90, 70},    // CalorMaximo: la rampa del mapa de calor
    };
    return paleta;
}

// EL TEMA CLARO
//
// Esta afinado para una terminal de FONDO CLARO: los tonos son oscuros, asi
// que se leen sobre el blanco. Los mismos roles, otros numeros.
//
// LA RAMPA DEL MAPA DE CALOR NO CAMBIA
//
// Los cuatro niveles del mapa de calor son los mismos en los dos temas: son
// una rampa de "mas es mas caliente", y los dos extremos (el azul y el rojo)
// se distinguen igual sobre los dos fondos.
const Paleta& paletaClara() {
    static const Paleta paleta = {
        {20, 90, 150},    // Titulo: el encabezado, bien separado del texto
        {138, 109, 31},   // Realce: lo que la pantalla trata
        {95, 105, 117},   // Secundario: ids, contadores y filas atenuadas
        {47, 92, 138},    // EtiquetaDeCampo: el nombre del campo del formulario
        {138, 148, 159},  // Ayuda: la linea de atajos del pie
        {26, 127, 55},    // Exito: la operacion salio bien
        {179, 38, 30},    // Error: la operacion fallo
        {170, 107, 0},    // Advertencia: hay que mirar pero no fallo
        {26, 127, 55},    // EstadoActivo: el turno o la persona esta vigente
        {179, 38, 30},    // EstadoCancelado: no se va a usar
        {170, 107, 0},    // EstadoReprogramado: se movera a otra fecha
        {20, 90, 150},    // TablaHeader: el rotulo de la columna
        {195, 202, 210},  // TablaBorde: el marco, sin competir con el dato
        {70, 130, 220},   // CalorBajo: la rampa del mapa de calor
        {90, 200, 140},   // CalorMedio: la rampa del mapa de calor
        {255, 200, 90},   // CalorAlto: la rampa del mapa de calor
        {240, 90, 70},    // CalorMaximo: la rampa del mapa de calor
    };
    return paleta;
}

// EL CACHE DEL ENTORNO
//
// Son dos variables (el valor y si ya se leyo) y no una sola porque el valor
// se tiene que poder RESETEAR: los tests cambian `NO_COLOR` en caliente, y
// el menu de configuracion puede cambiar el tema en caliente.
Tema& valorCacheado() {
    static Tema valor = Tema::Oscuro;
    return valor;
}

bool& entornoLeido() {
    static bool leido = false;
    return leido;
}

// ---------------------------------------------------------------------------
// EL ESTADO COMPARTIDO DEL TEMA
//
// Es UN SOLO tema para todo el programa: la pantalla de configuracion (y la
// de tema) lo escriben y TODAS las pantallas lo leen.
//
// SE INICIALIZA UNA SOLA VEZ
//
// El valor inicial sale del entorno (`temaPorDefecto()`). Despues de eso, el
// unico que lo cambia es `establecerTema()`.
//
// POR QUE NO ESTA EN EL CONSTRUCTOR DE `Theme`
//
// `Tabla` crea un `Theme` en cada render, y las pantallas tienen el suyo
// desde que se arman. Si el constructor reiniciara el estado compartido, el
// tema se perderia en el primer redibujado. Por eso el estado vive ACA (un
// `static` de funcion) y el constructor solo lo CONSULTA.
//
// OJO: el estado se inicializa al PRIMER USO, no al cargar el programa. Es
// lo que permite que los tests cambien `NO_COLOR` antes de la primera
// consulta.
// ---------------------------------------------------------------------------
Tema& estadoCompartido() {
    static Tema valor = Theme::temaPorDefecto();
    return valor;
}

// La paleta del tema activo.
//
// El tema "Apagados" NO tiene paleta: es la ausencia de color, asi que no
// hay ningun numero que devolver.
const Paleta* paletaDelTema(Tema tema) {
    if (tema == Tema::Claro) {
        return &paletaClara();
    }
    if (tema == Tema::Oscuro) {
        return &paletaOscura();
    }
    return nullptr;
}

} // namespace

Theme::Theme() {
    // La instancia NO fija ningun valor: sigue al estado compartido (que
    // se inicializa en el primer uso).
}

Theme::Theme(Tema tema) : _forzado(tema) {
}

Tema Theme::tema() const {
    // El valor EFECTIVO: el forzado de la instancia si lo hay, y el del
    // estado compartido si no.
    if (_forzado.has_value()) {
        return *_forzado;
    }
    return estadoCompartido();
}

bool Theme::coloresActivos() const {
    // Solo el tema "Apagados" deja de pintar color.
    return tema() != Tema::Apagados;
}

std::optional<Color> Theme::colorDe(Rol rol) const {
    const Paleta* paleta = paletaDelTema(tema());
    if (paleta == nullptr) {
        // El tema esta apagado (o el rol no tiene color propio): la pantalla
        // dibuja el texto tal cual, sin ningun codigo de color.
        return std::nullopt;
    }

    switch (rol) {
        case Rol::Titulo:
            return paleta->titulo;
        case Rol::Realce:
            return paleta->realce;
        case Rol::Secundario:
            return paleta->secundario;
        case Rol::EtiquetaDeCampo:
            return paleta->etiquetaDeCampo;
        case Rol::ValorDeCampo:
            // SIN COLOR PROPIO: el valor de un campo va con el color por
            // defecto de la terminal.
            return std::nullopt;
        case Rol::Ayuda:
            return paleta->ayuda;
        case Rol::Exito:
            return paleta->exito;
        case Rol::Error:
            return paleta->error;
        case Rol::Advertencia:
            return paleta->advertencia;
        case Rol::EstadoActivo:
            return paleta->estadoActivo;
        case Rol::EstadoCancelado:
            return paleta->estadoCancelado;
        case Rol::EstadoReprogramado:
            return paleta->estadoReprogramado;
        case Rol::TablaHeader:
            return paleta->tablaHeader;
        case Rol::TablaCelda:
            // SIN COLOR PROPIO: la celda va con el color por defecto.
            return std::nullopt;
        case Rol::TablaBorde:
            return paleta->tablaBorde;
        case Rol::CalorBajo:
            return paleta->calorBajo;
        case Rol::CalorMedio:
            return paleta->calorMedio;
        case Rol::CalorAlto:
            return paleta->calorAlto;
        case Rol::CalorMaximo:
            return paleta->calorMaximo;
    }
    return std::nullopt;
}

std::optional<Color> Theme::titulo() const { return colorDe(Rol::Titulo); }
std::optional<Color> Theme::realce() const { return colorDe(Rol::Realce); }
std::optional<Color> Theme::secundario() const { return colorDe(Rol::Secundario); }
std::optional<Color> Theme::etiquetaDeCampo() const { return colorDe(Rol::EtiquetaDeCampo); }
std::optional<Color> Theme::valorDeCampo() const { return colorDe(Rol::ValorDeCampo); }
std::optional<Color> Theme::ayuda() const { return colorDe(Rol::Ayuda); }
std::optional<Color> Theme::exito() const { return colorDe(Rol::Exito); }
std::optional<Color> Theme::error() const { return colorDe(Rol::Error); }
std::optional<Color> Theme::advertencia() const { return colorDe(Rol::Advertencia); }
std::optional<Color> Theme::estadoActivo() const { return colorDe(Rol::EstadoActivo); }
std::optional<Color> Theme::estadoCancelado() const { return colorDe(Rol::EstadoCancelado); }
std::optional<Color> Theme::estadoReprogramado() const { return colorDe(Rol::EstadoReprogramado); }
std::optional<Color> Theme::tablaHeader() const { return colorDe(Rol::TablaHeader); }
std::optional<Color> Theme::tablaCelda() const { return colorDe(Rol::TablaCelda); }
std::optional<Color> Theme::tablaBorde() const { return colorDe(Rol::TablaBorde); }

std::optional<Color> Theme::calorBajo() const { return colorDe(Rol::CalorBajo); }
std::optional<Color> Theme::calorMedio() const { return colorDe(Rol::CalorMedio); }
std::optional<Color> Theme::calorAlto() const { return colorDe(Rol::CalorAlto); }
std::optional<Color> Theme::calorMaximo() const { return colorDe(Rol::CalorMaximo); }

const Bordes& Theme::bordes() const {
    static const Bordes bordes;
    return bordes;
}

Tema Theme::temaPorDefecto() {
    if (!entornoLeido()) {
        const char* noColor = std::getenv("NO_COLOR");
        // La especificacion de NO_COLOR dice: si la variable esta DEFINIDA y
        // NO esta vacia, no se usan colores. Una variable definida pero
        // vacia NO apaga los colores.
        valorCacheado() = (noColor != nullptr && noColor[0] != '\0') ? Tema::Apagados : Tema::Oscuro;
        entornoLeido() = true;
    }
    return valorCacheado();
}

void Theme::establecerTema(Tema tema) {
    // Escribe el estado COMPARTIDO: el efecto es inmediato en todas las
    // pantallas que lo siguen (o sea, todas).
    estadoCompartido() = tema;
}

void Theme::recargarEntorno() {
    // Se descarta el valor cacheado del entorno...
    entornoLeido() = false;

    // ...y se vuelve a aplicar al estado COMPARTIDO lo que dice el entorno.
    //
    // "Recargar el entorno" es "volver a lo que dice el entorno": si el
    // usuario habia apagado los colores a mano, esta llamada los vuelve a
    // prender (o los apaga, si `NO_COLOR` esta definida).
    estadoCompartido() = temaPorDefecto();
}

} // namespace ui
