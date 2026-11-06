#pragma once

#include "ui/Evento.h"

#include <cstddef>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// El selector: una lista de la que se elige UNA.
//
// POR QUE EXISTE
//
// Un `Menu` obliga a ELEGIR con un atajo de un digito, y eso lo limita a DIEZ
// opciones. El catalogo de especialidades tiene DOCE. Ademas, escribir el
// codigo ("CARD") a mano es escribir un dato que el sistema ya conoce: el
// usuario podria escribir "card", "Card" o "CARD", y el error aparece
// DESPUES, cuando ya se escribio todo lo demas.
//
// EL ESTADO INVALIDO ES IRREPRESENTABLE
//
// Un selector siempre tiene una opcion elegida: no hay forma de "no elegir".
// Por eso el dato que sale de aca SIEMPRE es uno del catalogo, y la
// validacion del dominio no se puede saltar por un error de escritura.
//
// POR QUE NO ES UN `Menu`
//
// Por el atajo. El `Menu` exige un atajo de UN digito y que sea unico
// (`ui::Menu::agregar` lanza `DatoInvalidoException` si no lo es): son los
// diez digitos, y no entran doce opciones. Agrandar el `Menu` para atajos de
// dos digitos obliga a decidir si "1" es la opcion 1 o el primer digito de
// "12": una ambiguedad en un widget que usan tres pantallas mas.
//
// LA NUMERACION ES POSICIONAL
//
// Las opciones se numeran por su POSICION (1, 2, 3, ...), no con un atajo
// elegido por el programador. El numero se puede escribir COMPLETO: "12"
// elige la doceava opcion. Asi el numero que se ve y el numero que se
// escribe son el MISMO, y no hay dos listas que puedan desincronizarse.
//
// NO ES UN MENU DEL MOTOR (decision D22)
//
// Como el `Menu` y el `Campo`, este widget es LOGICA PURA: no sabe nada del
// motor de terminal, no dibuja y no abre el ciclo de eventos. La pantalla
// dibuja las lineas y hace el hit-testing del raton (el widget solo le dice
// que opcion cae en cada fila).
// ---------------------------------------------------------------------------
namespace ui {

class Selector {
public:
    // El selector con sus opciones, en el orden en que se muestran.
    //
    // La primera opcion arranca elegida: un selector recien abierto no
    // tiene "nada elegido" (eso seria un estado invalido).
    Selector(std::string titulo, std::vector<std::string> opciones);

    // Mueve la opcion resaltada una linea.
    //
    // LA NAVEGACION NO DA LA VUELTA
    //
    // Es la misma regla del `Menu`: al llegar al extremo, la seleccion se
    // QUEDA. Con la lista a la vista, un salto del extremo al otro hace que
    // el ojo tenga que volver a encontrar la fila.
    //
    // Ademas, mover la seleccion BORRA el numero que se estaba escribiendo:
    // el usuario cambio de forma de elegir (con las flechas) y el numero a
    // medias ya no dice nada.
    //
    // Devuelve true si la seleccion cambio.
    bool moverArriba();
    bool moverAbajo();

    // Aplica un evento al selector.
    //
    // - las flechas mueven la opcion resaltada;
    // - un digito se escribe (el numero de la opcion que se quiere elegir);
    // - el Retroceso borra el ultimo digito escrito;
    // - el Enter elige: si hay un numero escrito, elige ESA opcion; si no
    //   hay ninguno, elige la que esta resaltada.
    //
    // Devuelve true si el evento fue CONSUMIDO. Un evento que el selector no
    // sabe que hacer (una tecla que no es un digito, el `Escape`) devuelve
    // false, para que lo pueda atender la pantalla.
    //
    // EL RATON NO LO MANEJA EL SELECTOR
    //
    // El widget es logica: la pantalla es la que sabe en que fila dibujo
    // cada opcion, asi que el hit-testing lo hace ella (con
    // `opcionEnFila`).
    bool aplicarEvento(const Evento& evento);

    // El indice de la opcion elegida.
    int seleccion() const;

    // Elige la opcion de ese indice.
    //
    // UN INDICE FUERA DE RANGO SE IGNORA
    //
    // A diferencia de `Menu::seleccionar` (que lanza `std::out_of_range`),
    // aca un indice invalido no rompe nada: la seleccion no cambia. El
    // indice lo calcula la CONFIGURACION de la pantalla (que puede no
    // encontrar el dato en el catalogo) y un dato raro no puede romper la
    // pantalla.
    void seleccionar(int indice);

    // Cuantas opciones hay.
    int cantidad() const;

    const std::string& titulo() const;
    const std::vector<std::string>& opciones() const;

    // El ultimo error. Queda vacio si el ultimo intento fue valido.
    //
    // EL ERROR NO SE BORRA AL SEGUIR ESCRIBIENDO
    //
    // Es la misma regla del `Campo`: el error describe el ultimo intento y
    // el usuario tiene que ver por que le reboto. Lo borra el Enter que si
    // elige una opcion valida.
    const std::string& error() const;

    // El numero que el usuario esta escribiendo.
    //
    // Va aparte del texto de las opciones porque es lo que el usuario esta
    // TECLEANDO: si no se muestra, escribir "12" seria indistinguible de no
    // escribir nada.
    const std::string& numeroDigitado() const;

    // HIT-TESTING DEL RATON
    //
    // El selector dibuja una opcion por linea, asi que la opcion `i` queda en
    // la fila `filaInicial + i`. La funcion devuelve el indice de la opcion
    // que hay en esa fila, o -1 si la fila no cae sobre ninguna.
    //
    // La columna (`x`) no se mira: la opcion ocupa la linea entera.
    int opcionEnFila(int y, int filaInicial) const;

    // La fila de la pantalla donde se dibuja la PRIMERA opcion.
    //
    // La pantalla que dibuja el selector tiene que avisarle en que fila
    // arranca: sin ese numero, el clic del raton no podria saber que opcion
    // se toco.
    void definirFilaInicial(int fila);
    int filaInicial() const;

    // La linea de una opcion: "  3. Pediatria".
    //
    // Con `elegida` en true, la linea de la opcion que esta elegida termina
    // con " (elegida)": es la marca del TEXTO PLANO, donde no hay
    // invertidas que digan cual es la opcion resaltada. La pantalla (que
    // dibuja con color) no la usa.
    std::string lineaDe(int indice, bool elegida = false) const;

    // El selector en texto plano: el titulo y las opciones, una por linea,
    // con el numero posicional adelante. Es lo que se imprime cuando NO hay
    // terminal interactiva (D31).
    //
    // LA OPCION ELEGIDA ESTA MARCADA
    //
    // La linea de la opcion elegida termina con " (elegida)": en texto plano
    // no hay colores ni invertidas, asi que sin la marca no se sabria cual
    // es la que esta resaltada.
    std::string textoPlano() const;

private:
    // Agrega un digito al numero que se esta escribiendo.
    void digitar(char digito);

    // Borra el ultimo digito del numero que se esta escribiendo.
    void borrarDigito();

    // El Enter: elige la opcion del numero escrito, o la resaltada si no
    // hay numero escrito.
    void confirmar();

    // El numero del texto escrito, o -1 si el texto no es un numero.
    int numeroDe(const std::string& texto) const;

    std::string _titulo;
    std::vector<std::string> _opciones;

    int _seleccion = 0;
    int _filaInicial = 0;

    // El numero que el usuario esta escribiendo (vacio si no escribio nada).
    std::string _numeroDigitado;

    std::string _error;
};

} // namespace ui
