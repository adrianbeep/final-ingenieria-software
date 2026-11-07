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

    // Dias transcurridos desde la primera fecha valida del dominio
    // (2000-01-01), contando los anos bisiestos.
    //
    // PARA QUE ESTA EXISTE
    //
    // Medir el tiempo en MINUTOS de forma continua necesita un numero de
    // dia comparable. El `operator<` alcanza para ORDENAR dos fechas, pero
    // no para RESTARLAS: con dos fechas no hay forma de saber cuantas
    // noches hay entre una y otra. Este numero es unico y monotono (a mas
    // fecha, mas numero), asi que multiplicado por los minutos del dia da
    // un instante absoluto, comparable con cualquier otro.
    //
    // Es un dato GENERICO de calendario: no sabe nada de turnos ni de
    // horarios, asi que vive aqui y no en la capa de reglas de negocio.
    int numeroDeDia() const;

    // El dia de la semana: 0 = lunes, 1 = martes, ... 6 = domingo.
    //
    // POR QUE LA CONVENCION ES "LUNES = 0"
    //
    // Es la convencion que usan los calendarios (la primera columna de una
    // grilla semanal es el lunes) y la que hace que el indice sea
    // `(numeroDeDia() + 5) % 7`: el 2000-01-01 (la primera fecha valida del
    // dominio) fue un SABADO, que con esta convencion es el indice 5.
    //
    // Es DETERMINISTA: el resultado sale de aritmetica sobre (anio, mes,
    // dia), no del reloj del sistema. Un test puede fijar el resultado y
    // la misma fecha da SIEMPRE el mismo dia, en cualquier maquina.
    //
    // Lanza `DatoInvalidoException` si la fecha no existe en el calendario:
    // el dia de la semana de un 31 de febrero no es un dato, es un error.
    int diaSemana() const;

    // El nombre del dia de la semana, con la convencion de `diaSemana()`:
    // el indice 0 es el lunes y el 6 el domingo.
    //
    // Los nombres van en minusculas y CON TILDE ("miercoles" con e acentuada,
    // "sabado" con o acentuada): son DATOS, no comentarios. Los comentarios
    // del proyecto son ASCII, pero un string que se muestra en pantalla
    // lleva la ortografia correcta.
    //
    // Lanza `DatoInvalidoException` si el indice no esta entre 0 y 6.
    static std::string nombreDiaSemana(int indice);

    bool operator<(const Fecha& otra) const;
    bool operator==(const Fecha& otra) const;

    // Dias de un ano que NO son bisiestos. Los bisiestos se cuentan
    // aparte, asi que este es el piso del calculo de `numeroDeDia`.
    static constexpr int DIAS_POR_ANIO = 365;
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

    // Minutos transcurridos desde la medianoche del mismo dia (0 .. 1439).
    //
    // PARA QUE ESTA EXISTE
    //
    // El horario de atencion y la duracion se miden en la MISMA unidad: el
    // tope de las 22:00 es un INSTANTE (1320 minutos), no una hora, asi que
    // compararlo con la hora suelta daria resultados erroneos ("22:30"
    // pareceria valido). Con esta conversion, la hora y la duracion se
    // suman sin ambiguedad.
    //
    // Es un dato GENERICO de reloj: no sabe nada de turnos ni de
    // agendamiento, asi que vive aqui y no en la capa de reglas de negocio.
    int minutosDesdeMedianoche() const;

    bool operator<(const Hora& otra) const;
    bool operator==(const Hora& otra) const;
};
