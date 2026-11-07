#pragma once

#include "utils/Fecha.h"

#include <ctime>

// ---------------------------------------------------------------------------
// El RELOJ del sistema: la fecha de hoy.
//
// POR QUE VIVE EN LA CAPA `ui`
//
// La fecha de HOY es la unica regla del sistema que depende del "ahora"
// (todo lo demas es aritmetica sobre el calendario). La usa la pantalla de
// calendario para saber en que mes y en que dia arranca el recorrido.
//
// LA PANTALLA NO LA CONSULTA: LA RECIBE
//
// La pantalla de calendario recibe la fecha de HOY por parametro. El que la
// llama es el que decide de donde sale: la aplicacion real le pasa la del
// reloj del sistema, y los tests le pasan una FIJA. Asi ningun test depende
// del dia en que se corre, y la pantalla no tiene una regla que no sea la
// suya.
//
// EL RELOJ DEL SISTEMA, NO EL DEL DOMINIO
//
// El dominio tiene su propia regla del "ahora" (la fecha en la que un turno
// ya paso), y esa es de negocio: vive en el servicio de turnos. Esta es de
// presentacion: es la fecha con la que arranca una grilla.
// ---------------------------------------------------------------------------
namespace ui {

// La fecha de HOY, segun el reloj del sistema.
inline Fecha hoyDelSistema() {
    const std::time_t ahora = std::time(nullptr);

    // `std::localtime` devuelve un puntero a un objeto estatico: es la forma
    // portable de obtener la fecha LOCAL (la del calendario de la
    // institucion), que es la que cuenta para el horario de atencion.
    const std::tm* local = std::localtime(&ahora);
    if (local == nullptr) {
        // No deberia pasar (solo si el reloj del sistema no se puede
        // convertir). Si pasa, se devuelve la PRIMERA fecha valida del
        // dominio: es la unica fecha que siempre existe.
        return Fecha();
    }

    // `tm_year` cuenta los anios desde 1900 y `tm_mon` los meses desde CERO.
    return Fecha(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
}

} // namespace ui
