#pragma once

#include "ui/Semana.h"
#include "ui/Theme.h"
#include "utils/Fecha.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// La GRILLA de un mes: las semanas que lo contienen.
//
// QUE ES UNA GRILLA DE MES
//
// Son las semanas COMPLETAS (de lunes a domingo) que contienen al mes: la
// primera empieza el lunes de la semana del dia 1, y la ultima termina el
// domingo de la semana del ultimo dia del mes. Los dias que NO pertenecen
// al mes (los del principio y los del final) son HUECOS: la celda existe
// (la grilla es un rectangulo) pero no muestra ningun dia.
//
// LA GRILLA SE ARMA CON EL NUMERO DE DIA
//
// La cuenta es la del DOMINIO: `Fecha::numeroDeDia()` es la cantidad de dias
// desde el 2000-01-01, y `ui::fechaDesdeNumeroDeDia` es su inversa (las dos
// usan la misma convencion de anos bisiestos). Con esos numeros, las
// semanas son bloques CONSECUTIVOS de siete: el primero arranca el lunes
// de la semana del dia 1, y cada semana siguiente arranca siete dias
// despues.
//
// POR QUE NO SE USA `ui::diasDeLaSemana`
//
// `diasDeLaSemana` LIMITA los dias al rango del dominio: en el enero del
// 2000 (la primera fecha valida) devuelve una semana que arranca el SABADO
// (el lunes anterior no existe en el calendario del dominio) y la coloca en
// la columna del lunes. Para un reporte de ocupacion eso no importa (el
// nombre del dia lo da `Fecha::diaSemana`), pero para una GRILLA si: la
// columna del lunes tiene que ser el lunes. Por eso el lunes de la primera
// semana se calcula con la cuenta del dominio, sin limitar.
//
// LOS EXTREMOS DEL CALENDARIO
//
// Alrededor del 2000-01-01 y del 2100-12-31 hay celdas que no tienen fecha
// (estan antes o despues del rango del dominio): son huecos, como las del
// principio y del final de cualquier mes.
// ---------------------------------------------------------------------------
namespace ui {

// UNA CELDA DE LA GRILLA
//
// O es un dia del mes (con su fecha y su numero) o es un hueco.
struct CeldaDelMes {
    bool esHueco = true;
    Fecha fecha;
    int dia = 0;
};

// LAS CELDAS DE LA GRILLA DE UN MES
//
// Vienen en orden: de izquierda a derecha y de arriba abajo, de a siete por
// fila (de lunes a domingo). La cantidad siempre es un multiplo de siete.
inline std::vector<CeldaDelMes> celdasDelMes(int anio, int mes) {
    std::vector<CeldaDelMes> celdas;

    const Fecha primerDia(anio, mes, 1);
    const int cantidadDeDias = Fecha::diasDelMes(anio, mes);
    if (cantidadDeDias <= 0) {
        return celdas;
    }

    // EL LUNES DE LA SEMANA DEL DIA 1
    //
    // Es el dia 1 menos su dia de la semana. Puede quedar antes del
    // 2000-01-01 (el enero del 2000 arranca un sabado): en ese caso las
    // celdas de esa semana son huecos, y la cuenta sigue sirviendo para
    // saber que columna es cada una.
    const int lunes = primerDia.numeroDeDia() - primerDia.diaSemana();

    // EL ULTIMO DIA DEL MES
    const int ultimoDia = primerDia.numeroDeDia() + cantidadDeDias - 1;

    // LAS SEMANAS: BLOQUES CONSECUTIVOS DE SIETE DIAS
    for (int inicio = lunes; inicio <= ultimoDia; inicio += 7) {
        for (int i = 0; i < 7; ++i) {
            const int numero = inicio + i;

            CeldaDelMes celda;
            // Los EXTREMOS DEL CALENDARIO: la celda esta antes del
            // 2000-01-01 o despues del 2100-12-31, asi que no tiene fecha.
            if (numero < 0 || numero > ultimoNumeroDeDia()) {
                celdas.push_back(celda);
                continue;
            }

            const Fecha fecha = fechaDesdeNumeroDeDia(numero);
            // Un dia que NO es de este mes (el que abre la semana o el que
            // la cierra) es un hueco.
            if (fecha.anio != anio || fecha.mes != mes) {
                celdas.push_back(celda);
                continue;
            }

            celda.esHueco = false;
            celda.fecha = fecha;
            celda.dia = fecha.dia;
            celdas.push_back(celda);
        }
    }

    return celdas;
}

// ---------------------------------------------------------------------------
// LA FILA DE LA GRILLA QUE SE DIBUJA
//
// La grilla se arma UNA vez y se dibuja de DOS maneras (con color en la
// terminal y en texto plano), asi que los datos de una fila van en una
// estructura: el texto de cada celda y el rol de la paleta con el que se
// pinta.
//
// EL ROL DE LA CELDA
//
// Es el nivel de calor del dia (ver `ui/MapaCalor.h`). El hueco y el dia sin
// turnos se pintan con el color normal de una celda.
// ---------------------------------------------------------------------------
struct CeldaDeGrilla {
    std::string texto;
    Rol rol = Rol::TablaCelda;
};

// UNA FILA DE LA GRILLA: LAS SIETE CELDAS DE UNA SEMANA
struct FilaDeGrilla {
    std::vector<CeldaDeGrilla> celdas;
};

// UNA SEMANA DE LA GRILLA
//
// Son DOS filas: la de los numeros de los dias y la de la cantidad de
// turnos de cada dia. La segunda va DEBAJO de la primera, y no en otra
// tabla: asi se ve de un vistazo cuantos turnos tiene cada dia.
struct SemanaDeGrilla {
    FilaDeGrilla numeros;
    FilaDeGrilla contadores;
};

} // namespace ui
