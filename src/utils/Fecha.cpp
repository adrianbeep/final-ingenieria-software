#include "utils/Fecha.h"

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Utils.h"

#include <string>

namespace {

// Texto de dos digitos con cero a la izquierda ("07", "15").
// `std::to_string` NO rellena con ceros, asi que el relleno se hace a mano.
std::string dosDigitos(int valor) {
    if (valor < 10) {
        return "0" + std::to_string(valor);
    }
    return std::to_string(valor);
}

// ---------------------------------------------------------------------------
// La tabla del ALGORITMO DE SAKAMOTO.
//
// El indice es el mes MENOS UNO: la entrada 0 es enero y la entrada 11 es
// diciembre.
//
// QUE SON ESTOS NUMEROS
//
// Sakamoto calcula el dia de la semana con una cuenta corta: el anio entra
// dividido entre cuatro (con los ajustes de los siglos) y el mes aporta un
// DESPLAZAMIENTO fijo. La tabla es esa lista de desplazamientos: los meses
// que empiezan en un dia de la semana mas temprano (febrero, con 28 dias)
// desplazan menos que los que empiezan mas tarde (agosto, con 31).
//
// POR QUE NO SE USA `time()` NI `localtime`
//
// El reloj daria la respuesta, pero la respuesta DEPENDERIA del momento y de
// la zona horaria de la maquina: un test no podria fijar el resultado y dos
// maquinas darian dos respuestas distintas para la misma fecha. Sakamoto es
// aritmetica pura sobre (anio, mes, dia): la misma fecha da SIEMPRE el mismo
// dia, en cualquier maquina y en cualquier momento.
// ---------------------------------------------------------------------------
constexpr int DESPLAZAMIENTO_SAKAMOTO[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};

// Cantidad de dias de la semana: la convencion del dominio es lunes = 0.
constexpr int DIAS_DE_LA_SEMANA = 7;

} // namespace

// ---------------------------------------------------------------------------
// Fecha
// ---------------------------------------------------------------------------

Fecha::Fecha() : anio(ANIO_MINIMO), mes(1), dia(1) {
}

Fecha::Fecha(int anio_, int mes_, int dia_) : anio(anio_), mes(mes_), dia(dia_) {
}

bool Fecha::esValida(int anio, int mes, int dia) {
    if (anio < ANIO_MINIMO || anio > ANIO_MAXIMO) {
        return false;
    }
    if (mes < 1 || mes > 12) {
        return false;
    }
    const int dias = diasDelMes(anio, mes);
    if (dias == 0) {
        return false;
    }
    return dia >= 1 && dia <= dias;
}

Fecha Fecha::desdeTexto(const std::string& texto) {
    if (!Utils::esFormatoFecha(texto)) {
        throw DatoInvalidoException("fecha", texto, "se esperaba AAAA-MM-DD con una fecha existente");
    }

    const int anio = std::stoi(texto.substr(0, 4));
    const int mes = std::stoi(texto.substr(5, 2));
    const int dia = std::stoi(texto.substr(8, 2));

    if (!esValida(anio, mes, dia)) {
        throw DatoInvalidoException("fecha", texto, "se esperaba AAAA-MM-DD con una fecha existente");
    }

    return Fecha(anio, mes, dia);
}

std::string Fecha::aTexto() const {
    return std::to_string(anio) + "-" + dosDigitos(mes) + "-" + dosDigitos(dia);
}

bool Fecha::esBisiesto(int anio) {
    if (anio % 400 == 0) {
        return true;  // divisible por 400: bisiesto (2000)
    }
    if (anio % 100 == 0) {
        return false;  // divisible por 100 pero no por 400: NO bisiesto (1900, 2100)
    }
    return anio % 4 == 0;
}

int Fecha::diasDelMes(int anio, int mes) {
    switch (mes) {
        case 1:
        case 3:
        case 5:
        case 7:
        case 8:
        case 10:
        case 12:
            return 31;
        case 4:
        case 6:
        case 9:
        case 11:
            return 30;
        case 2:
            return esBisiesto(anio) ? 29 : 28;
        default:
            return 0;  // mes inexistente
    }
}

int Fecha::numeroDeDia() const {
    // Los anos COMPLETOS van primero: son los dias de cada ano entre el
    // arranque del dominio y el anio de esta fecha.
    int dias = (anio - ANIO_MINIMO) * DIAS_POR_ANIO;

    // Los bisiestos se suman aparte: un ano bisiesto tiene un dia mas, y
    // ese dia es el que hace que dos fechas del mismo mes de años distintos
    // NO se confundan.
    for (int anioPrevio = ANIO_MINIMO; anioPrevio < anio; ++anioPrevio) {
        if (esBisiesto(anioPrevio)) {
            ++dias;
        }
    }

    // Despues, los meses COMPLETOS del anio en curso.
    for (int mesPrevio = 1; mesPrevio < mes; ++mesPrevio) {
        dias += diasDelMes(anio, mesPrevio);
    }

    // Y por ultimo, los dias que ya pasaron del mes: el dia 1 no suma
    // nada (el primer dia del mes es el mismo dia del mes anterior).
    return dias + (dia - 1);
}

int Fecha::diaSemana() const {
    // La fecha tiene que EXISTIR en el calendario: el dia de la semana de un
    // 31 de febrero no es un dato, es un error. Es el mismo criterio que
    // usa `Turno::setFecha` y el servicio de turnos.
    if (!esValida(anio, mes, dia)) {
        throw DatoInvalidoException("fecha", aTexto(), "fecha inexistente");
    }

    // EL ALGORITMO DE SAKAMOTO
    //
    // La cuenta es: el anio, ajustado por los siglos, mas un cuarto del anio,
    // menos un centesima, mas un cuatrocentesima, mas el desplazamiento del
    // mes, mas el dia. Todo modulo siete.
    //
    // OJO CON LOS PRIMERES DOS MESES: enero y febrero son los "meses 13 y 14"
    // del ano anterior. Por eso, cuando el mes es menor que 3, el anio que
    // entra en la cuenta es el ANTERIOR: el desplazamiento del mes esta
    // calculado para esa convencion.
    int anioAjustado = anio;
    if (mes < 3) {
        --anioAjustado;
    }

    const int sakamoto = (anioAjustado + anioAjustado / 4 - anioAjustado / 100
                          + anioAjustado / 400 + DESPLAZAMIENTO_SAKAMOTO[mes - 1] + dia)
        % DIAS_DE_LA_SEMANA;

    // Sakamoto numera de 0 = domingo a 6 = sabado. El dominio usa la
    // convencion INVERSA (lunes = 0 ... domingo = 6), asi que el resultado
    // se SHIFTEA una posicion: el domingo (0) pasa a ser el 6 y el lunes (1)
    // pasa a ser el 0.
    return (sakamoto + DIAS_DE_LA_SEMANA - 1) % DIAS_DE_LA_SEMANA;
}

std::string Fecha::nombreDiaSemana(int indice) {
    // Los nombres van CON TILDE: son los que se muestran en pantalla, asi que
    // tienen que estar bien escritos.
    static const char* const NOMBRES[] = {"lunes",   "martes", "miércoles",
                                           "jueves", "viernes", "sábado",
                                           "domingo"};

    if (indice < 0 || indice >= DIAS_DE_LA_SEMANA) {
        throw DatoInvalidoException("diaSemana", std::to_string(indice),
                                    "se esperaba un indice de dia de la semana entre 0 y 6");
    }

    return NOMBRES[indice];
}

bool Fecha::operator<(const Fecha& otra) const {
    if (anio != otra.anio) {
        return anio < otra.anio;
    }
    if (mes != otra.mes) {
        return mes < otra.mes;
    }
    return dia < otra.dia;
}

bool Fecha::operator==(const Fecha& otra) const {
    return anio == otra.anio && mes == otra.mes && dia == otra.dia;
}

// ---------------------------------------------------------------------------
// Hora
// ---------------------------------------------------------------------------

Hora::Hora() : hora(HORA_INICIO), minuto(0) {
}

Hora::Hora(int hora_, int minuto_) : hora(hora_), minuto(minuto_) {
}

bool Hora::esValida(int hora, int minuto) {
    if (minuto < 0 || minuto >= MINUTOS_POR_HORA) {
        return false;
    }
    if (minuto % MINUTO_GRANULARIDAD != 0) {
        return false;  // la granularidad es de 30 minutos
    }

    // POR QUE SE VALIDA EN MINUTOS DESDE MEDIANOCHE Y NO COMPARANDO HORA Y
    // MINUTO POR SEPARADO: el tope del horario (22:00) es un instante, no
    // una hora. Si solo miramos la hora, "22:30" pareceria valido porque su
    // hora (22) esta dentro del rango; al llevar todo a minutos, 22:30 son
    // 1350 minutos y supera el tope de 1320 (22:00), asi que se descarta.
    const int total = hora * MINUTOS_POR_HORA + minuto;
    const int inicio = HORA_INICIO * MINUTOS_POR_HORA;  // 480 = 08:00
    const int fin = HORA_FIN * MINUTOS_POR_HORA;         // 1320 = 22:00

    return total >= inicio && total <= fin;
}

Hora Hora::desdeTexto(const std::string& texto) {
    if (!Utils::esFormatoHora(texto)) {
        throw DatoInvalidoException("hora", texto, "se esperaba HH:MM con una hora valida");
    }

    const int hora = std::stoi(texto.substr(0, 2));
    const int minuto = std::stoi(texto.substr(3, 2));

    if (!esValida(hora, minuto)) {
        throw DatoInvalidoException("hora", texto, "se esperaba HH:MM con una hora valida");
    }

    return Hora(hora, minuto);
}

std::string Hora::aTexto() const {
    return dosDigitos(hora) + ":" + dosDigitos(minuto);
}

int Hora::minutosDesdeMedianoche() const {
    return hora * MINUTOS_POR_HORA + minuto;
}

bool Hora::operator<(const Hora& otra) const {
    if (hora != otra.hora) {
        return hora < otra.hora;
    }
    return minuto < otra.minuto;
}

bool Hora::operator==(const Hora& otra) const {
    return hora == otra.hora && minuto == otra.minuto;
}
