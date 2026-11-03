#include "test_framework.h"

#include "exceptions/ExcepcionesSistema.h"
#include "utils/Fecha.h"

#include <string>

// ---------------------------------------------------------------------------
// Tests de `Fecha` y `Hora`.
//
// `Fecha::desdeTexto` y `Hora::desdeTexto` lanzan `DatoInvalidoException`
// cuando el texto no tiene el formato esperado o cuando el valor no existe
// en el calendario / no cae en el horario de atencion.
// ---------------------------------------------------------------------------

TEST_CASE("esValida acepta solo las fechas que existen en el calendario", fecha_es_valida) {
    // Anio bisiesto real: 29 de febrero existe.
    CHECK(Fecha::esValida(2024, 2, 29));
    // Anio no bisiesto: 29 de febrero no existe.
    CHECK(!Fecha::esValida(2025, 2, 29));
    // Dia que no existe en un mes de 30 dias.
    CHECK(!Fecha::esValida(2026, 4, 31));
    // Mes fuera de rango.
    CHECK(!Fecha::esValida(2026, 13, 1));
    CHECK(!Fecha::esValida(2026, 0, 1));
    // Mes de 31 dias.
    CHECK(Fecha::esValida(2026, 1, 31));
    CHECK(Fecha::esValida(2026, 12, 31));
    // Ceros: no existen.
    CHECK(!Fecha::esValida(2026, 0, 0));
    CHECK(!Fecha::esValida(2026, 1, 0));
    CHECK(!Fecha::esValida(2026, 1, 32));
    // Anios en los extremos del rango permitido.
    CHECK(Fecha::esValida(2000, 1, 1));
    CHECK(Fecha::esValida(2100, 12, 31));
    CHECK(!Fecha::esValida(1999, 12, 31));
    CHECK(!Fecha::esValida(2101, 1, 1));
}

TEST_CASE("esBisiesto aplica la regla divisible por 4, 100 y 400", fecha_es_bisiesto) {
    CHECK(Fecha::esBisiesto(2024));
    CHECK(!Fecha::esBisiesto(2025));
    CHECK(!Fecha::esBisiesto(1900));  // divisible por 100 pero no por 400
    CHECK(Fecha::esBisiesto(2000));   // divisible por 400
    CHECK(!Fecha::esBisiesto(2100));
}

TEST_CASE("diasDelMes devuelve la cantidad de dias de cada mes", fecha_dias_del_mes) {
    CHECK(Fecha::diasDelMes(2026, 1) == 31);
    CHECK(Fecha::diasDelMes(2026, 4) == 30);
    CHECK(Fecha::diasDelMes(2024, 2) == 29);
    CHECK(Fecha::diasDelMes(2026, 2) == 28);
    CHECK(Fecha::diasDelMes(2026, 13) == 0);
    CHECK(Fecha::diasDelMes(2026, 0) == 0);
}

TEST_CASE("aTexto arma la fecha con ceros a la izquierda", fecha_a_texto) {
    CHECK(Fecha(2026, 3, 15).aTexto() == "2026-03-15");
    CHECK(Fecha(2026, 12, 5).aTexto() == "2026-12-05");
    CHECK(Fecha(2026, 1, 1).aTexto() == "2026-01-01");
}

TEST_CASE("desdeTexto hace el round-trip de una fecha valida", fecha_desde_texto) {
    const Fecha fecha = Fecha::desdeTexto("2026-03-15");
    CHECK(fecha.anio == 2026);
    CHECK(fecha.mes == 3);
    CHECK(fecha.dia == 15);
    CHECK(fecha.aTexto() == "2026-03-15");
}

TEST_CASE("desdeTexto lanza DatoInvalidoException con una fecha inexistente", fecha_desde_texto_invalida) {
    bool lanzo = false;
    try {
        (void)Fecha::desdeTexto("2026-04-31");
    } catch (const DatoInvalidoException& e) {
        lanzo = true;
        CHECK(e.campo() == "fecha");
        CHECK(e.valor() == "2026-04-31");
    } catch (...) {
    }
    CHECK(lanzo);
}

TEST_CASE("desdeTexto lanza DatoInvalidoException con un formato incorrecto", fecha_desde_texto_formato) {
    bool lanzoFormato = false;
    try {
        (void)Fecha::desdeTexto("31/02/2026");
    } catch (const DatoInvalidoException&) {
        lanzoFormato = true;
    } catch (...) {
    }
    CHECK(lanzoFormato);

    bool lanzoBasura = false;
    try {
        (void)Fecha::desdeTexto("basura");
    } catch (const DatoInvalidoException&) {
        lanzoBasura = true;
    } catch (...) {
    }
    CHECK(lanzoBasura);

    bool lanzoAnio = false;
    try {
        (void)Fecha::desdeTexto("1999-01-01");
    } catch (const DatoInvalidoException&) {
        lanzoAnio = true;
    } catch (...) {
    }
    CHECK(lanzoAnio);
}

TEST_CASE("operator< de Fecha compara anio, mes y dia en ese orden", fecha_operador_menor) {
    CHECK(Fecha(2026, 1, 1) < Fecha(2026, 1, 2));
    CHECK(Fecha(2026, 1, 1) < Fecha(2026, 2, 1));
    CHECK(Fecha(2025, 12, 31) < Fecha(2026, 1, 1));
    CHECK(!(Fecha(2026, 1, 1) < Fecha(2026, 1, 1)));
    CHECK(!(Fecha(2026, 2, 1) < Fecha(2026, 1, 1)));
}

TEST_CASE("operator== de Fecha compara los tres campos", fecha_operador_igual) {
    CHECK(Fecha(2026, 3, 15) == Fecha(2026, 3, 15));
    CHECK(!(Fecha(2026, 3, 15) == Fecha(2026, 3, 16)));
    CHECK(!(Fecha(2026, 3, 15) == Fecha(2026, 4, 15)));
    CHECK(!(Fecha(2026, 3, 15) == Fecha(2025, 3, 15)));
}

TEST_CASE("Hora::esValida respeta el horario y la granularidad de 30 minutos", hora_es_valida) {
    CHECK(Hora::esValida(8, 0));    // 08:00
    CHECK(Hora::esValida(8, 30));   // 08:30
    CHECK(!Hora::esValida(8, 15));  // 08:15 no es multiple de 30
    CHECK(!Hora::esValida(7, 59));  // antes del horario de atencion
    CHECK(Hora::esValida(22, 0));   // 22:00 es el tope
    CHECK(!Hora::esValida(22, 30)); // 22:30 ya pasa el tope
    CHECK(Hora::esValida(21, 30));
    CHECK(!Hora::esValida(23, 0));
    CHECK(!Hora::esValida(8, 60));
    CHECK(!Hora::esValida(8, -30));
}

TEST_CASE("Hora::aTexto arma la hora con ceros a la izquierda", hora_a_texto) {
    CHECK(Hora(9, 30).aTexto() == "09:30");
    CHECK(Hora(8, 0).aTexto() == "08:00");
    CHECK(Hora(22, 0).aTexto() == "22:00");
}

TEST_CASE("Hora::desdeTexto hace el round-trip y lanza si no es valida", hora_desde_texto) {
    const Hora hora = Hora::desdeTexto("09:30");
    CHECK(hora.hora == 9);
    CHECK(hora.minuto == 30);
    CHECK(hora.aTexto() == "09:30");

    bool lanzo = false;
    try {
        (void)Hora::desdeTexto("25:00");
    } catch (const DatoInvalidoException& e) {
        lanzo = true;
        CHECK(e.campo() == "hora");
        CHECK(e.valor() == "25:00");
    } catch (...) {
    }
    CHECK(lanzo);

    bool lanzoFormato = false;
    try {
        (void)Hora::desdeTexto("9:30");
    } catch (const DatoInvalidoException&) {
        lanzoFormato = true;
    } catch (...) {
    }
    CHECK(lanzoFormato);
}

TEST_CASE("operator< y operator== de Hora comparan hora y minuto", hora_operadores) {
    CHECK(Hora(8, 0) < Hora(8, 30));
    CHECK(Hora(8, 30) < Hora(9, 0));
    CHECK(!(Hora(9, 0) < Hora(8, 30)));
    CHECK(Hora(9, 30) == Hora(9, 30));
    CHECK(!(Hora(9, 30) == Hora(9, 0)));
}

// ---------------------------------------------------------------------------
// EL DIA DE LA SEMANA
//
// La convencion del dominio es LUNES = 0 ... DOMINGO = 6: es la que usan
// los calendarios (la primera columna de una grilla semanal es el lunes).
//
// Las fechas de referencia son ABSOLUTAS: no se derivan del reloj, asi que el
// test sigue significando exactamente lo mismo dentro de cien años. Y como
// `diaSemana()` es aritmetica pura (no consulta el reloj), el resultado es
// el mismo en cualquier maquina.
// ---------------------------------------------------------------------------

TEST_CASE("diaSemana devuelve el dia de la semana con la convencion lunes = 0",
          fecha_dia_semana) {
    // El 2000-01-01 (la primera fecha valida del dominio) fue un SABADO:
    // con la convencion lunes = 0, el sabado es el indice 5.
    CHECK(Fecha(2000, 1, 1).diaSemana() == 5);
    // Y al dia siguiente, un DOMINGO: el indice 6.
    CHECK(Fecha(2000, 1, 2).diaSemana() == 6);
    // El 2024-01-01 fue un LUNES: el indice 0.
    CHECK(Fecha(2024, 1, 1).diaSemana() == 0);
    // El 2024-02-29 fue un JUEVES: el indice 3.
    //
    // OJO: el dia de la semana de un 29 de febrero no se deduce "a ojo" del
    // 1 de enero: hay que sumarle los dias del mes. Por eso este caso esta
    // verificado con la comprobacion CRUZADA de abajo, que no depende de
    // ningun dia de referencia.
    CHECK(Fecha(2024, 2, 29).diaSemana() == 3);
    // El 2025-12-31 fue un MIERCOLES: el indice 2.
    CHECK(Fecha(2025, 12, 31).diaSemana() == 2);
    // El 2026-11-03 fue un MARTES: el indice 1.
    CHECK(Fecha(2026, 11, 3).diaSemana() == 1);
}

TEST_CASE("diaSemana es consistente con numeroDeDia en un conjunto de fechas",
          fecha_dia_semana_verificacion_cruzada) {
    // LA VERIFICACION CRUZADA
    //
    // `numeroDeDia()` dice cuantos dias pasaron desde el 2000-01-01, y el
    // 2000-01-01 fue un SABADO (el indice 5 con la convencion lunes = 0).
    // Asi que el dia de la semana de CUALQUIER fecha tiene que ser
    // `(5 + numeroDeDia()) % 7`.
    //
    // POR QUE ESTA COMPROBACION ES FUERTE
    //
    // Las dos cuentas son INDEPENDIENTES: `numeroDeDia()` cuenta los dias del
    // calendario (con los anos bisiestos) y `diaSemana()` hace la cuenta de
    // Sakamoto. Si una de las dos estuviera mal (un bisiesto mal contado, un
    // desplazamiento mal puesto), el modulo no cerraria.
    //
    // El conjunto incluye los dos extremos del rango valido (2000 y 2100) y
    // tres 29 de febrero, que son los dias que mas veces rompen un calendario.
    const Fecha fechas[] = {
        Fecha(2000, 1, 1),    // el primer dia del dominio
        Fecha(2000, 2, 29),   // bisiesto del primer ano
        Fecha(2000, 12, 31),
        Fecha(2001, 1, 1),
        Fecha(2024, 2, 29),   // bisiesto
        Fecha(2025, 12, 31),
        Fecha(2026, 11, 3),
        Fecha(2096, 2, 29),   // bisiesto
        Fecha(2100, 2, 28),   // 2100 NO es bisiesto: el 29 no existe
        Fecha(2100, 12, 31)   // el ultimo dia del dominio
    };

    for (const Fecha& fecha : fechas) {
        CHECK(fecha.diaSemana() == (5 + fecha.numeroDeDia()) % 7);
    }
}

TEST_CASE("nombreDiaSemana devuelve el nombre del dia con la convencion lunes = 0",
          fecha_nombre_dia_semana) {
    // Los nombres van CON TILDE: son los que se muestran en pantalla.
    CHECK(Fecha::nombreDiaSemana(0) == "lunes");
    CHECK(Fecha::nombreDiaSemana(1) == "martes");
    CHECK(Fecha::nombreDiaSemana(2) == "miércoles");
    CHECK(Fecha::nombreDiaSemana(3) == "jueves");
    CHECK(Fecha::nombreDiaSemana(4) == "viernes");
    CHECK(Fecha::nombreDiaSemana(5) == "sábado");
    CHECK(Fecha::nombreDiaSemana(6) == "domingo");
}

TEST_CASE("nombreDiaSemana con un indice invalido lanza DatoInvalidoException",
          fecha_nombre_dia_semana_invalido) {
    const int indicesInvalidos[] = {-1, 7, 100, -100};

    for (int indice : indicesInvalidos) {
        const ErrorCapturado error = capturarError(
            [&] { (void)Fecha::nombreDiaSemana(indice); });
        CHECK(error.lanzo);
        CHECK(!error.lanzoOtraExcepcion);
        CHECK(error.campo == "diaSemana");
        CHECK(error.valor == std::to_string(indice));
    }
}
