#pragma once

#include "utils/Fecha.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// La SEMANA de una fecha: los siete dias que la contienen.
//
// POR QUE VIVE EN LA CAPA `ui` Y NO EN EL DOMINIO
//
// La semana es el recorte que usa el reporte de ocupacion: no es una regla
// de negocio ni un dato de las entidades. Vive aca, junto al widget que la
// muestra, y el dominio (`Fecha`) queda como esta: un valor de calendario y
// su numero de dia.
//
// EL LUNES ES EL PRIMER DIA
//
// `Fecha::diaSemana()` usa la convencion del dominio (0 = lunes, ...,
// 6 = domingo), asi que el lunes de la semana que contiene a una fecha es la
// fecha menos su dia de la semana.
//
// EL NUMERO DE DIA HACE LA CUENTA
//
// `Fecha::numeroDeDia()` es la cantidad de dias desde la primera fecha valida
// del dominio (2000-01-01), y es MONOTONO: a mas fecha, mas numero. Por eso
// el lunes se calcula restando el dia de la semana al numero de dia, y los
// siete dias de la semana son los siete numeros consecutivos que arrancan en
// el del lunes.
//
// LA CONVERSION DE VUELTA
//
// `Fecha` no tiene un inverso de `numeroDeDia()`: hay que hacerlo, y con la
// MISMA convencion de anios bisiestos que usa aquel (divisible por 4, salvo
// los divisible por 100 que no lo son por 400). Si las dos contaran
// distinto, un dia seria de un largo y del otro: el mismo bug del turno que
// cae en dos dias.
//
// LOS EXTREMOS DEL CALENDARIO
//
// El calendario del dominio va del 2000-01-01 al 2100-12-31, asi que una
// semana puede no caber entera: la del 2000-01-01 (un SABADO) arranca antes
// del primer dia valido, y la del 2100-12-31 (un JUEVES) termina despues del
// ultimo. Los dias se LIMITAN al rango del dominio, asi que la semana
// siempre tiene siete filas y ninguna fecha queda fuera del calendario.
// ---------------------------------------------------------------------------
namespace ui {

// La fecha que corresponde a un numero de dia (el inverso de
// `Fecha::numeroDeDia()`).
//
// POR QUE NO USA `Fecha::desdeTexto`
//
// `desdeTexto` parsea un TEXTO y valida que la fecha exista: es la puerta de
// entrada del dato que escribe el usuario. Aca el numero ya es un dato
// interno, asi que la conversion es aritmetica pura.
inline Fecha fechaDesdeNumeroDeDia(int numeroDeDia) {
    int anio = Fecha::ANIO_MINIMO;

    // Los anos COMPLETOS: un ano tiene 365 dias, y 366 si es bisiesto. Es la
    // misma cuenta que hace `numeroDeDia()`, al reves.
    int diasDelAnio = Fecha::DIAS_POR_ANIO + (Fecha::esBisiesto(anio) ? 1 : 0);
    while (numeroDeDia >= diasDelAnio) {
        numeroDeDia -= diasDelAnio;
        ++anio;
        diasDelAnio = Fecha::DIAS_POR_ANIO + (Fecha::esBisiesto(anio) ? 1 : 0);
    }

    // Los meses COMPLETOS del ano en curso.
    int mes = 1;
    while (numeroDeDia >= Fecha::diasDelMes(anio, mes)) {
        numeroDeDia -= Fecha::diasDelMes(anio, mes);
        ++mes;
    }

    // Y por ultimo el dia del mes (el dia 1 no suma nada).
    return Fecha(anio, mes, numeroDeDia + 1);
}

// El numero de dia del ULTIMO dia del calendario del dominio.
inline int ultimoNumeroDeDia() {
    return Fecha(Fecha::ANIO_MAXIMO, 12, 31).numeroDeDia();
}

// Los SIETE dias de la semana que contiene a la fecha, del lunes al domingo.
//
// LA SEMANA SIEMPRE TIENE SIETE DIAS
//
// Los extremos del calendario se limitan, asi que una semana puede empezar
// despues del lunes o terminar antes del domingo: en ese caso los dias que
// se salen del rango quedan en el ultimo dia valido. Es un caso de BORDE del
// dominio, no del reporte.
inline std::vector<Fecha> diasDeLaSemana(const Fecha& fecha) {
    const int maximo = ultimoNumeroDeDia();

    int lunes = fecha.numeroDeDia() - fecha.diaSemana();
    if (lunes < 0) {
        lunes = 0;
    }

    std::vector<Fecha> dias;
    dias.reserve(7);
    for (int i = 0; i < 7; ++i) {
        const int numero = lunes + i;
        dias.push_back(fechaDesdeNumeroDeDia(numero > maximo ? maximo : numero));
    }
    return dias;
}

} // namespace ui
