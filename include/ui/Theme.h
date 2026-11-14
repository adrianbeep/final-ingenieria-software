#pragma once

#include <optional>
#include <string>

// ---------------------------------------------------------------------------
// La paleta de la aplicacion.
//
// ESTA CLASE ES DATO, NO PRESENTACION
//
// El `Theme` no sabe dibujar: solo responde tres preguntas. Que color tiene
// cada rol, que tema esta activo, y si ese color se PINTA o no. La pantalla
// es la que dibuja, y la que decide si aplica el color.
//
// POR QUE EL COLOR ES PROPIO Y NO `ftxui::Color` (decision D30)
//
// Si la paleta guardara tipos de FTXUI, su logica (los temas activos, el
// `NO_COLOR`) dejaria de ser testeable sin FTXUI. Con un color propio
// (`r`, `g`, `b`), la paleta es dato puro, y la conversion al color de FTXUI
// es un helper aparte (`ui::aFtxui`, en `ui/ColorFtxui.h`).
//
// LOS ROLES SON POR INTENCION, NO POR ELEMENTO
//
// Un rol no dice QUE COSA es ("esto es un titulo", "esto es una cabecera de
// tabla"): dice PARA QUE SIRVE ("esto es lo que la pantalla trata", "esto es
// secundario", "esto es el estado"). Por eso hay un rol `Realce` (lo
// importante) y uno `Secundario` (lo que acompana), y no un rol por cada
// elemento de la interfaz.
//
// LA PALETA TIENE VARIOS JUEGOS DE VALORES
//
// Los roles son los mismos en los dos temas: lo que cambia es el NUMERO. Por
// eso ajustar un tono despues es cambiar un valor, no reescribir un contrato.
// ---------------------------------------------------------------------------
namespace ui {

// Un color en RGB. Cada componente va de 0 a 255.
struct Color {
    int r = 0;
    int g = 0;
    int b = 0;
};

// LOS TRES TEMAS DE LA PALETA
//
// Un tema es un JUEGO DE VALORES de los roles: el oscuro esta afinado para
// una terminal de fondo oscuro y el claro para una de fondo claro.
//
// "APAGADOS" NO ES UN JUEGO DE VALORES: es la ausencia de color. Los roles no
// pintan NINGUN codigo de color, asi que el texto sale con el color por
// defecto de la terminal. Es lo que hace el `NO_COLOR`.
//
// EL TEMA ACTIVO ES UN ESTADO COMPARTIDO
//
// Es el mismo mecanismo que el flag de color de la decision 36: lo escribe
// la pantalla de configuracion y TODAS las pantallas lo leen.
enum class Tema {
    Apagados,
    Oscuro,
    Claro
};

// Los roles de la paleta.
//
// QUE SIGNIFICA CADA UNO (el contrato)
//
//   - `Titulo`: el encabezado de la pantalla. Es lo que identifica donde
//     estas.
//   - `Realce`: lo que la pantalla TRATA. El dato que eligio el usuario, el
//     total de un reporte, el atajo que hay que apretar. Es lo que tiene que
//     saltar a la vista.
//   - `Secundario`: lo que ACOMPAÑA al dato sin ser el dato. Los ids, los
//     contadores, la leyenda, las filas de las entidades dadas de baja.
//   - `EtiquetaDeCampo`: el nombre de un campo del formulario ("Fecha:").
//   - `ValorDeCampo`: lo que el usuario escribio en el campo. Va con el color
//     por defecto: el valor es el dato, y el dato se lee solo.
//   - `Ayuda`: la linea de atajos del pie. Es una referencia, no un dato.
//   - `Exito`, `Error`, `Advertencia`: los tres mensajes del resultado de una
//     operacion.
//   - `EstadoActivo`, `EstadoCancelado`, `EstadoReprogramado`: el estado de un
//     turno o de una persona. El color DICE como esta.
//   - `TablaHeader`: la cabecera de una tabla. Es el rotulo, no el dato.
//   - `TablaCelda`: la celda de una tabla. Va con el color por defecto: la
//     tabla tiene que leerse como texto normal, no como texto de color.
//   - `TablaBorde`: el marco de una tabla. Es estructura, no informacion.
//   - `CalorBajo` ... `CalorMaximo`: los cuatro niveles del mapa de calor del
//     calendario.
enum class Rol {
    Titulo,
    Realce,
    Secundario,
    EtiquetaDeCampo,
    ValorDeCampo,
    Ayuda,
    Exito,
    Error,
    Advertencia,
    EstadoActivo,
    EstadoCancelado,
    EstadoReprogramado,
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
    // Usa el estado COMPARTIDO del tema (ver `establecerTema()`).
    //
    // LA INSTANCIA SIGUE AL ESTADO COMPARTIDO
    //
    // La instancia NO copia el valor: lo consulta cada vez que se lo piden.
    // Por eso un cambio de tema (el de la pantalla de configuracion) se ve
    // AL INSTANTE en las pantallas que ya estan abiertas, sin tener que
    // recrearlas.
    //
    // Y el constructor NO reinicia el estado compartido: si lo reiniciara,
    // el `Theme` que `Tabla` crea en cada render perderia el tema en el
    // primer redibujado.
    Theme();

    // Fuerza el tema SOLO para esta instancia (lo usan los tests).
    //
    // No toca el estado compartido: el tema forzado es de la instancia y
    // muere con ella.
    explicit Theme(Tema tema);

    // El tema EFECTIVO de esta instancia: el forzado (si lo hay) o el del
    // estado compartido.
    Tema tema() const;

    // Si el tema activo PINTA color.
    //
    // Es `false` SOLO para el tema "Apagados": en los otros dos, los roles
    // pintan su color (salvo los que no tienen color propio).
    bool coloresActivos() const;

    // El color a pintar para un rol.
    //
    // Devuelve `std::nullopt` cuando (a) el tema esta apagado o (b) el rol no
    // tiene color propio: la pantalla dibuja el texto tal cual, sin ningun
    // codigo de color. Asi la decision de pintar la toma quien dibuja, que es
    // el unico que sabe hacerlo.
    //
    // LOS ROLES SIN COLOR PROPIO
    //
    // `ValorDeCampo` y `TablaCelda` NO tienen color propio: van con el color
    // por defecto de la terminal. El valor de un campo y la celda de una
    // tabla son el dato, y el dato se lee con el color de siempre.
    std::optional<Color> colorDe(Rol rol) const;

    // La paleta del tema activo: el color del rol.
    //
    // Son los mismos colores que propone `colorDe()`: el atajo para pedir el
    // color de UN rol sin escribir el `switch` (que lo usan los tests para
    // comparar contra lo dibujado).
    std::optional<Color> titulo() const;
    std::optional<Color> realce() const;
    std::optional<Color> secundario() const;
    std::optional<Color> etiquetaDeCampo() const;
    std::optional<Color> valorDeCampo() const;
    std::optional<Color> ayuda() const;
    std::optional<Color> exito() const;
    std::optional<Color> error() const;
    std::optional<Color> advertencia() const;
    std::optional<Color> estadoActivo() const;
    std::optional<Color> estadoCancelado() const;
    std::optional<Color> estadoReprogramado() const;
    std::optional<Color> tablaHeader() const;
    std::optional<Color> tablaCelda() const;
    std::optional<Color> tablaBorde() const;

    // Los colores del MAPA DE CALOR del calendario (cuatro niveles).
    std::optional<Color> calorBajo() const;
    std::optional<Color> calorMedio() const;
    std::optional<Color> calorAlto() const;
    std::optional<Color> calorMaximo() const;

    // Los caracteres de borde de las tablas y los recuadros.
    const Bordes& bordes() const;

    // El tema inicial de ESTE proceso.
    //
    // `NO_COLOR` (https://no-color.org) apaga los colores: si la variable de
    // entorno esta DEFINIDA y NO esta vacia, el tema inicial es "Apagados".
    // Sin `NO_COLOR` (o con la variable vacia), el tema inicial es el oscuro.
    //
    // SE LEE UNA SOLA VEZ: el valor se cachea en el primer uso. Releer el
    // entorno en cada dibujo seria un `getenv` por frame, y el entorno no
    // cambia mientras corre el programa.
    //
    // ES EL ESTADO INICIAL, NO UN VETO
    //
    // `NO_COLOR` dice como ARRANCA la aplicacion, no si puede haber color.
    // Con la variable definida la aplicacion arranca sin colores, pero el
    // usuario puede ELEGIR un tema desde la pantalla de configuracion (con
    // `establecerTema()`).
    static Tema temaPorDefecto();

    // Escribe el estado COMPARTIDO del tema.
    //
    // El efecto es INMEDIATO: todas las pantallas que siguen al estado
    // compartido (o sea, todas las que se armaron con `Theme()`) cambian en
    // el proximo redibujado.
    //
    // Es lo que usa la pantalla de configuracion.
    static void establecerTema(Tema tema);

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
    // El tema FORZADO de esta instancia.
    //
    // Es `std::nullopt` cuando la instancia SIGUE al estado compartido: en
    // ese caso `tema()` devuelve el valor compartido, que puede cambiar en
    // cualquier momento.
    std::optional<Tema> _forzado;
};

} // namespace ui
