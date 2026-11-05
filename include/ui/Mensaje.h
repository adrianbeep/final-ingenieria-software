#pragma once

#include <string>

// ---------------------------------------------------------------------------
// El mensaje de resultado de una operacion.
//
// ES UN VALOR, NO UNA IMPRESION (decision D38)
//
// En una TUI, imprimir a stdout ROMPE el frame: el motor dibuja la pantalla
// entera y un `std::cout` en el medio se cuela entre dos frames. El mensaje
// es DATO: lo guarda la pantalla y lo dibuja con el `Theme` (el rol `Exito`,
// `Error` o `Advertencia` de la paleta).
//
// EL FORMATO ES UNO SOLO
//
// "OK  " (con dos espacios) y "ERR " (con un espacio) son el formato de la
// especificacion. Vive ACA, y no en cada pantalla: si cada pantalla
// escribiera el prefijo a mano, el formato se desincronizaria en la primera
// pantalla nueva.
// ---------------------------------------------------------------------------
namespace ui {

class Mensaje {
public:
    // "OK  <texto>"
    static Mensaje exito(const std::string& texto);

    // "ERR <texto>"
    static Mensaje error(const std::string& texto);

    // "AVISO <texto>"
    static Mensaje aviso(const std::string& texto);

    // El mensaje que no dice nada: la pantalla todavia no mostro un
    // resultado.
    //
    // OJO: el predicado se llama `esVacio()` y no `vacio()` porque C++ no
    // permite que una funcion estatica y otra que no lo sea tengan el mismo
    // nombre y la misma lista de parametros.
    static Mensaje vacio();

    // El texto del mensaje, CON el prefijo.
    const std::string& texto() const;

    bool esExito() const;
    bool esError() const;

    // Si el mensaje no dice nada.
    bool esVacio() const;

private:
    enum class Rol { Ninguno, Exito, Error, Aviso };

    Mensaje(Rol rol, std::string texto);

    Rol _rol;
    std::string _texto;
};

} // namespace ui
