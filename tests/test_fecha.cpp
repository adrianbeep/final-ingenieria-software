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
