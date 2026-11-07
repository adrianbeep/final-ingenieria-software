#include "ui/Theme.h"

#include <cstdlib>

namespace ui {
namespace {

// LA PALETA
//
// Son valores RGB enteros. Los tonos son legibles sobre un fondo oscuro
// (que es el fondo que se elige por defecto en una terminal) y los roles de
// exito y de error son los mas saturados, porque son los que tienen que
// saltar a la vista.
constexpr Color COLOR_TITULO{94, 214, 255};
constexpr Color COLOR_SUBTITULO{150, 150, 150};
constexpr Color COLOR_EXITO{126, 231, 135};
constexpr Color COLOR_ERROR{255, 106, 106};
constexpr Color COLOR_ADVERTENCIA{255, 214, 102};
constexpr Color COLOR_TABLA_HEADER{235, 235, 235};
constexpr Color COLOR_TABLA_CELDA{200, 200, 200};
constexpr Color COLOR_TABLA_BORDE{110, 110, 110};

// ---------------------------------------------------------------------------
// LOS COLORES DEL MAPA DE CALOR
//
// SON UNA RAMPA DE "MAS ES MAS CALIENTE"
//
// El dia con mas turnos se tiene que ver al instante, asi que los cuatro
// niveles van de un color FRIO a uno CALIENTE: azul, verde, amarillo y rojo.
// Es la rampa clasica del mapa de calor, y funciona sobre un fondo oscuro
// (que es el fondo que se elige por defecto en una terminal).
//
// EL "0 TURNOS" NO TIENE COLOR
//
// Un dia sin turnos no es "el nivel mas bajo": es la AUSENCIA de ocupacion.
// Por eso no tiene un rol propio: la celda se pinta con el color normal de
// una celda (`COLOR_TABLA_CELDA`) y la leyenda lo dice con palabras.
//
// POR QUE EL VERDE ES EL SEGUNDO Y NO EL PRIMERO
//
// El verde es, en un consultorio, el color de "todo bien": si el primer
// nivel (1-2 turnos) fuera verde, un dia con dos turnos pareceria un dia
// libre. Empezando por el azul, la lectura es "de poco a mucho": el azul es
// el nivel bajo y el rojo el nivel alto.
// ---------------------------------------------------------------------------
constexpr Color COLOR_CALOR_BAJO{70, 130, 220};
constexpr Color COLOR_CALOR_MEDIO{90, 200, 140};
constexpr Color COLOR_CALOR_ALTO{255, 200, 90};
constexpr Color COLOR_CALOR_MAXIMO{240, 90, 70};

// EL CACHE DEL ENTORNO
//
// Son dos variables (el valor y si ya se leyo) y no una sola porque el valor
// se tiene que poder RESETEAR: los tests cambian `NO_COLOR` en caliente, y
// el menu de configuracion puede cambiar la paleta en caliente.
bool& valorCacheado() {
    static bool valor = true;
    return valor;
}

bool& entornoLeido() {
    static bool leido = false;
    return leido;
}

// ---------------------------------------------------------------------------
// EL ESTADO COMPARTIDO DE LOS COLORES ACTIVOS
//
// Es UN SOLO booleano para todo el programa: el toggle de la pantalla de
// configuracion lo escribe y TODAS las pantallas lo leen.
//
// SE INICIALIZA UNA SOLA VEZ
//
// El valor inicial sale del entorno (`coloresActivosPorDefecto()`). Despues
// de eso, el unico que lo cambia es `establecerColoresActivos()`.
//
// POR QUE NO ESTA EN EL CONSTRUCTOR DE `Theme`
//
// `Tabla` crea un `Theme` en cada render, y las pantallas tienen el suyo
// desde que se arman. Si el constructor reiniciara el estado compartido, el
// toggle se perderia en el primer redibujado. Por eso el estado vive ACA
// (un `static` de funcion) y el constructor solo lo CONSULTA.
//
// OJO: el estado se inicializa al PRIMER USO, no al cargar el programa. Es
// lo que permite que los tests cambien `NO_COLOR` antes de la primera
// consulta.
// ---------------------------------------------------------------------------
bool& estadoCompartido() {
    static bool valor = Theme::coloresActivosPorDefecto();
    return valor;
}

} // namespace

Theme::Theme() {
    // La instancia NO fija ningun valor: sigue al estado compartido (que
    // se inicializa en el primer uso).
}

Theme::Theme(bool coloresActivos) : _forzado(coloresActivos) {
}

bool Theme::coloresActivos() const {
    // El valor EFECTIVO: el forzado de la instancia si lo hay, y el del
    // estado compartido si no.
    if (_forzado.has_value()) {
        return *_forzado;
    }
    return estadoCompartido();
}

std::optional<Color> Theme::colorDe(Rol rol) const {
    if (!coloresActivos()) {
        return std::nullopt;
    }
    switch (rol) {
        case Rol::Titulo:
            return titulo();
        case Rol::Subtitulo:
            return subtitulo();
        case Rol::Exito:
            return exito();
        case Rol::Error:
            return error();
        case Rol::Advertencia:
            return advertencia();
        case Rol::TablaHeader:
            return tablaHeader();
        case Rol::TablaCelda:
            return tablaCelda();
        case Rol::TablaBorde:
            return tablaBorde();
        case Rol::CalorBajo:
            return calorBajo();
        case Rol::CalorMedio:
            return calorMedio();
        case Rol::CalorAlto:
            return calorAlto();
        case Rol::CalorMaximo:
            return calorMaximo();
    }
    return std::nullopt;
}

const Color& Theme::titulo() const { return COLOR_TITULO; }
const Color& Theme::subtitulo() const { return COLOR_SUBTITULO; }
const Color& Theme::exito() const { return COLOR_EXITO; }
const Color& Theme::error() const { return COLOR_ERROR; }
const Color& Theme::advertencia() const { return COLOR_ADVERTENCIA; }
const Color& Theme::tablaHeader() const { return COLOR_TABLA_HEADER; }
const Color& Theme::tablaCelda() const { return COLOR_TABLA_CELDA; }
const Color& Theme::tablaBorde() const { return COLOR_TABLA_BORDE; }

const Color& Theme::calorBajo() const { return COLOR_CALOR_BAJO; }
const Color& Theme::calorMedio() const { return COLOR_CALOR_MEDIO; }
const Color& Theme::calorAlto() const { return COLOR_CALOR_ALTO; }
const Color& Theme::calorMaximo() const { return COLOR_CALOR_MAXIMO; }

const Bordes& Theme::bordes() const {
    static const Bordes bordes;
    return bordes;
}

bool Theme::coloresActivosPorDefecto() {
    if (!entornoLeido()) {
        const char* noColor = std::getenv("NO_COLOR");
        // La especificacion de NO_COLOR dice: si la variable esta DEFINIDA y
        // NO esta vacia, no se usan colores. Una variable definida pero
        // vacia NO apaga los colores.
        valorCacheado() = !(noColor != nullptr && noColor[0] != '\0');
        entornoLeido() = true;
    }
    return valorCacheado();
}

void Theme::establecerColoresActivos(bool coloresActivos) {
    // Escribe el estado COMPARTIDO: el efecto es inmediato en todas las
    // pantallas que lo siguen (o sea, todas).
    estadoCompartido() = coloresActivos;
}

void Theme::recargarEntorno() {
    // Se descarta el valor cacheado del entorno...
    entornoLeido() = false;

    // ...y se vuelve a aplicar al estado COMPARTIDO lo que dice el entorno.
    //
    // "Recargar el entorno" es "volver a lo que dice el entorno": si el
    // usuario habia apagado los colores a mano, esta llamada los vuelve a
    // prender (o los apaga, si `NO_COLOR` esta definida).
    estadoCompartido() = coloresActivosPorDefecto();
}

} // namespace ui
