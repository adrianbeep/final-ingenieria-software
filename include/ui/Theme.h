#pragma once

#include <optional>
#include <string>

// ---------------------------------------------------------------------------
// La paleta de la aplicacion.
//
// ESTA CLASE ES DATO, NO PRESENTACION
//
// El `Theme` no sabe dibujar: solo responde dos preguntas. Que color tiene
// cada rol, y si ese color se PINTA o no. La pantalla es la que dibuja, y
// la que decide si aplica el color.
//
// POR QUE EL COLOR ES PROPIO Y NO `ftxui::Color` (decision D30)
//
// Si la paleta guardara tipos de FTXUI, su logica (los colores activos, el
// `NO_COLOR`) dejaria de ser testeable sin FTXUI. Con un color propio
// (`r`, `g`, `b`), la paleta es dato puro, y la conversion al color de FTXUI
// es un helper aparte (`ui::aFtxui`, en `ui/ColorFtxui.h`).
// ---------------------------------------------------------------------------
namespace ui {

// Un color en RGB. Cada componente va de 0 a 255.
struct Color {
    int r = 0;
    int g = 0;
    int b = 0;
};

// Los roles de la paleta. Son los que nombra el roadmap.
enum class Rol {
    Titulo,
    Subtitulo,
    Exito,
    Error,
    Advertencia,
    TablaHeader,
    TablaCelda,
    TablaBorde
};

// Los caracteres con los que se dibujan tablas y recuadros.
//
// Son UN caracter cada uno (el motor dibuja celda por celda), asi que son
// `char` y no `std::string`.
struct Bordes {
    char esquinaSuperiorIzquierda = '+';
    char esquinaSuperiorDerecha = '+';
    char esquinaInferiorIzquierda = '+';
    char esquinaInferiorDerecha = '+';
    char horizontal = '-';
    char vertical = '|';
};

class Theme {
public:
    // Usa el valor por defecto de los colores activos (ver
    // `coloresActivosPorDefecto()`).
    Theme();

    // Fuerza el valor. Lo usan los tests y el menu de configuracion.
    explicit Theme(bool coloresActivos);

    // Si la paleta propone colores o no.
    bool coloresActivos() const;

    // El color a pintar para un rol.
    //
    // Devuelve `std::nullopt` cuando los colores estan apagados: la
    // pantalla dibuja el texto tal cual, sin ningun codigo de color. Asi la
    // decision de pintar la toma quien dibuja, que es el unico que sabe
    // hacerlo.
    std::optional<Color> colorDe(Rol rol) const;

    // La paleta pura: el color del rol, exista o no el modo sin color.
    // Sirve para saber QUE color se usaria si los colores estuvieran
    // encendidos.
    const Color& titulo() const;
    const Color& subtitulo() const;
    const Color& exito() const;
    const Color& error() const;
    const Color& advertencia() const;
    const Color& tablaHeader() const;
    const Color& tablaCelda() const;
    const Color& tablaBorde() const;

    // Los caracteres de borde de las tablas y los recuadros.
    const Bordes& bordes() const;

    // Si los colores estan activos por defecto en ESTE proceso.
    //
    // `NO_COLOR` (https://no-color.org) apaga los colores: si la variable de
    // entorno esta DEFINIDA y NO esta vacia, la paleta no propone ningun
    // color.
    //
    // SE LEE UNA SOLA VEZ: el valor se cachea en el primer uso. Releer el
    // entorno en cada dibujo seria un `getenv` por frame, y el entorno no
    // cambia mientras corre el programa.
    static bool coloresActivosPorDefecto();

    // Descarta el valor cacheado: la proxima llamada a
    // `coloresActivosPorDefecto()` vuelve a leer el entorno.
    //
    // Lo usan los tests (que cambian `NO_COLOR` en caliente) y el menu de
    // configuracion (que puede cambiar la paleta en caliente).
    static void recargarEntorno();

private:
    bool _coloresActivos;
};

} // namespace ui
