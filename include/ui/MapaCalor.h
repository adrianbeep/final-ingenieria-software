#pragma once

#include "ui/Theme.h"

#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// El MAPA DE CALOR de la ocupacion del calendario.
//
// QUE ES
//
// La intensidad de un dia de la grilla es la cantidad de turnos que tiene.
// Esa cantidad se traduce a un NIVEL de calor, y el nivel tiene un COLOR (el
// de un rol de la paleta) y un RANGO (los numeros de turnos que lo forman).
//
// LA INTENSIDAD ES EL NUMERO DE TURNOS DEL DIA
//
// El nivel se decide con la cantidad de turnos y NADA mas: no depende de
// cuantos turnos tiene el resto del mes, ni de cuantos hay en el mes que
// se esta viendo. Asi un dia tiene SIEMPRE el mismo color, se mire el mes
// que se mire.
//
// LOS UMBRALES SON ABSOLUTOS
//
// Los rangos son fijos:
//
//   - 0 turnos: sin turnos (no tiene color de calor);
//   - 1 a 2: bajo;
//   - 3 a 4: medio;
//   - 5 a 6: alto;
//   - 7 o mas: maximo.
//
// Un mes con muchos turnos y otro con pocos usan la MISMA escala: asi el
// color de un dia se puede comparar entre meses (y entre machines).
//
// POR QUE NO ES UNA ESCALA RELATIVA AL MES
//
// Con una escala relativa (el dia mas lleno del mes es el maximo), el mismo
// dia cambiaria de color al cambiar de mes, y un mes tranquilo seria todo
// rojo. Ademas, el calor de un dia es un dato de la CLINICA, no un dato de
// la pantalla: la escala tiene que significar lo mismo siempre.
//
// LA LEYENDA
//
// La leyenda dice los rangos. Con colores, cada rango aparece con una
// MUESTRA (un bloque pintado con el color del nivel); sin colores (`NO_COLOR`),
// la leyenda es de TEXTO: los rangos y lo que significan ("baja", "media",
// "alta", "muy alta"), porque un usuario que no ve colores tiene que poder
// leer el mapa igual.
// ---------------------------------------------------------------------------
namespace ui {

// El nivel de calor de un dia.
enum class NivelCalor { Ninguno, Bajo, Medio, Alto, Maximo };

// EL NIVEL DE CALOR DE UN DIA
//
// La cantidad de turnos decide el nivel. Los umbrales son los del comentario
// de arriba: son ABSOLUTOS, no dependen del mes.
inline NivelCalor nivelDeCalor(int turnos) {
    if (turnos <= 0) {
        return NivelCalor::Ninguno;
    }
    if (turnos <= 2) {
        return NivelCalor::Bajo;
    }
    if (turnos <= 4) {
        return NivelCalor::Medio;
    }
    if (turnos <= 6) {
        return NivelCalor::Alto;
    }
    return NivelCalor::Maximo;
}

// EL ROL DE LA PALETA DEL NIVEL
//
// El nivel "sin turnos" no tiene rol de calor: se pinta con el color normal
// de una celda (no hay ocupacion que pintar).
inline Rol rolDeCalor(NivelCalor nivel) {
    switch (nivel) {
        case NivelCalor::Bajo:
            return Rol::CalorBajo;
        case NivelCalor::Medio:
            return Rol::CalorMedio;
        case NivelCalor::Alto:
            return Rol::CalorAlto;
        case NivelCalor::Maximo:
            return Rol::CalorMaximo;
        case NivelCalor::Ninguno:
            break;
    }
    return Rol::TablaCelda;
}

// EL RANGO DE TURNOS DEL NIVEL
//
// Es el texto que va en la leyenda: "1-2", "3-4", "5-6", "7+".
inline std::string rangoDeCalor(NivelCalor nivel) {
    switch (nivel) {
        case NivelCalor::Bajo:
            return "1-2";
        case NivelCalor::Medio:
            return "3-4";
        case NivelCalor::Alto:
            return "5-6";
        case NivelCalor::Maximo:
            return "7+";
        case NivelCalor::Ninguno:
            break;
    }
    return "0";
}

// LO QUE SIGNIFICA EL NIVEL
//
// Es la palabra de la leyenda SIN colores: un usuario que no ve colores
// tiene que poder saber que tan lleno esta el dia.
inline std::string nombreDeCalor(NivelCalor nivel) {
    switch (nivel) {
        case NivelCalor::Bajo:
            return "baja";
        case NivelCalor::Medio:
            return "media";
        case NivelCalor::Alto:
            return "alta";
        case NivelCalor::Maximo:
            return "maxima";
        case NivelCalor::Ninguno:
            break;
    }
    return "sin turnos";
}

// LOS NIVELES QUE TIENEN COLOR
//
// Son los cuatro de la leyenda (el "sin turnos" no tiene muestra: no tiene
// color).
inline const std::vector<NivelCalor>& nivelesConColor() {
    static const std::vector<NivelCalor> niveles = {NivelCalor::Bajo, NivelCalor::Medio,
                                                     NivelCalor::Alto, NivelCalor::Maximo};
    return niveles;
}

} // namespace ui
