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
    TablaBorde,
    // LOS NIVELES DEL MAPA DE CALOR DEL CALENDARIO
    //
    // Son cuatro, y van de mas frio a mas caliente: el dia con mas turnos se
    // pinta con el color mas CALIENTE. Son los unicos roles que no son de un
    // texto de la interfaz: son de una CELDA de la grilla del mes, asi que
    // el "0 turnos" no tiene color propio (se queda con el de una celda
    // normal).
    CalorBajo,
    CalorMedio,
    CalorAlto,
    CalorMaximo
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
    // Usa el estado COMPARTIDO de los colores activos (ver
    // `establecerColoresActivos()`).
    //
    // LA INSTANCIA SIGUE AL ESTADO COMPARTIDO
    //
    // La instancia NO copia el valor: lo consulta cada vez que se lo piden.
    // Por eso un cambio del flag (el toggle de la pantalla de configuracion)
    // se ve AL INSTANTE en las pantallas que ya estan abiertas, sin tener que
    // recrearlas.
    //
    // Y el constructor NO reinicia el estado compartido: si lo reiniciara,
    // el `Theme` que `Tabla` crea en cada render perderia el toggle en el
    // primer redibujado.
    Theme();

    // Fuerza el valor SOLO para esta instancia (lo usan los tests).
    //
    // No toca el estado compartido: el valor forzado es de la instancia y
    // muere con ella.
    explicit Theme(bool coloresActivos);

    // El valor EFECTIVO de esta instancia: el forzado (si lo hay) o el del
    // estado compartido.
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

    // Los colores del MAPA DE CALOR del calendario (cuatro niveles).
    const Color& calorBajo() const;
    const Color& calorMedio() const;
    const Color& calorAlto() const;
    const Color& calorMaximo() const;

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
    //
    // ES EL ESTADO INICIAL, NO UN VETO
    //
    // `NO_COLOR` dice como ARRANCA la aplicacion, no si puede haber colores.
    // Con la variable definida la aplicacion arranca sin colores, pero el
    // usuario puede ENCENDERLOS desde la pantalla de configuracion (con
    // `establecerColoresActivos()`).
    static bool coloresActivosPorDefecto();

    // Escribe el estado COMPARTIDO de los colores activos.
    //
    // El efecto es INMEDIATO: todas las pantallas que siguen al estado
    // compartido (o sea, todas las que se armaron con `Theme()`) cambian en
    // el proximo redibujado.
    //
    // Es lo que usa el toggle de la pantalla de configuracion.
    static void establecerColoresActivos(bool coloresActivos);

    // Descarta el valor cacheado del entorno y VUELVE A APLICARLO al estado
    // compartido.
    //
    // "Recargar el entorno" es "volver a lo que dice el entorno": si el
    // usuario habia apagado los colores a mano, recargar los vuelve a
    // prender (o los apaga, si `NO_COLOR` esta definida).
    //
    // Lo usan los tests (que cambian `NO_COLOR` en caliente).
    static void recargarEntorno();

private:
    // El valor FORZADO de esta instancia.
    //
    // Es `std::nullopt` cuando la instancia SIGUE al estado compartido: en
    // ese caso `coloresActivos()` devuelve el valor compartido, que puede
    // cambiar en cualquier momento.
    std::optional<bool> _forzado;
};

} // namespace ui
