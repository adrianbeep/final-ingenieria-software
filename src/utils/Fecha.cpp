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

bool Hora::operator<(const Hora& otra) const {
    if (hora != otra.hora) {
        return hora < otra.hora;
    }
    return minuto < otra.minuto;
}

bool Hora::operator==(const Hora& otra) const {
    return hora == otra.hora && minuto == otra.minuto;
}
