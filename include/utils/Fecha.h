#pragma once

#include <string>

// ---------------------------------------------------------------------------
// Fecha y Hora: los dos valores de tiempo del sistema de turnos.
//
// Son estructuras (no clases) porque son valores: se copian, se comparan y
// no tienen comportamiento propio mas alla de formatearse.
//
// NOTA SOBRE `operator<`: se declara como MIEMBRO NO ESTATICO a proposito.
// El issue #2 proponia `static bool operator<`, pero eso esta mal en C++:
// un operador de comparacion recibe el otro operando como argumento, asi
// que tiene que ser un miembro (no estatico) que tome `const Fecha&` por
// parametro. Con un `static`, la firma seria `operator<(Fecha, Fecha)` y no
// se podria escribir `a < b` de forma natural ni sobrecargarlo por tipo.
// ---------------------------------------------------------------------------

// Fecha calendario: anio, mes y dia.
struct Fecha {
    int anio;
    int mes;
    int dia;

    static constexpr int ANIO_MINIMO = 2000;
    static constexpr int ANIO_MAXIMO = 2100;

    // Por defecto, la primera fecha valida del dominio (2000-01-01).
    // Es determinista a proposito: un valor por defecto que dependa del reloj
    // del sistema no se puede testear ni reproducir.
    Fecha();

    // Los parametros llevan sufijo '_' porque `-Wshadow` (exigido por la
    // consigna) avisa cuando un parametro se llama igual que un miembro.
    Fecha(int anio_, int mes_, int dia_);

    // Indica si la fecha existe en el calendario y esta dentro del rango de
    // anios permitido por el dominio.
    static bool esValida(int anio, int mes, int dia);

    // Parsea un texto "AAAA-MM-DD". Lanza `DatoInvalidoException` si el
    // formato no es correcto o si la fecha no existe en el calendario.
    // NOTA: este header NO incluye la cabecera de excepciones a proposito
    // (para no propagar una dependencia de la UI hacia el dominio); la
    // excepcion se declara en el .cpp.
    static Fecha desdeTexto(const std::string& texto);

    // Texto con formato "AAAA-MM-DD", con ceros a la izquierda.
    std::string aTexto() const;

    // Indica si el anio es bisiesto: divisible por 4 y (no por 100 o por 400).
    static bool esBisiesto(int anio);

    // Cantidad de dias del mes (28 o 29 en febrero). Devuelve 0 si el mes
    // esta fuera del rango 1..12.
    static int diasDelMes(int anio, int mes);

    bool operator<(const Fecha& otra) const;
    bool operator==(const Fecha& otra) const;
};

// Hora del dia: hora y minuto.
struct Hora {
    int hora;
    int minuto;

    static constexpr int HORA_INICIO = 8;           // 08:00
    static constexpr int HORA_FIN = 22;              // 22:00
    static constexpr int MINUTO_GRANULARIDAD = 30;   // los turnos duran 30 minutos
    static constexpr int MINUTOS_POR_HORA = 60;

    // Por defecto, la primera hora valida del dominio (08:00).
    Hora();

    // Los parametros llevan sufijo '_' por la misma razon que en `Fecha`.
    Hora(int hora_, int minuto_);

    // Indica si la hora cae dentro del horario de atencion y respeta la
    // granularidad de 30 minutos.
    static bool esValida(int hora, int minuto);

    // Parsea un texto "HH:MM". Lanza `DatoInvalidoException` si el formato
    // o el rango no son validos.
    static Hora desdeTexto(const std::string& texto);

    // Texto con formato "HH:MM", con ceros a la izquierda.
    std::string aTexto() const;

    bool operator<(const Hora& otra) const;
    bool operator==(const Hora& otra) const;
};
