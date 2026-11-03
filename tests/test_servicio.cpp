#include "test_framework.h"

#include "entities/Alumno.h"
#include "entities/Profesor.h"
#include "entities/Turno.h"
#include "exceptions/ExcepcionesSistema.h"
#include "repositories/IRepositorio.h"
#include "services/ServicioTurnos.h"
#include "utils/Fecha.h"

#include <cstddef>
#include <ctime>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// ---------------------------------------------------------------------------
// Tests de la FASE 5 (bloque 2): la capa de SERVICIOS (reglas de negocio).
//
// Lo que se prueba ACa:
//   - las reglas de `agendar`: existencia y estado del alumno y del
//     profesor, fecha no vencida, datos validos y NO solapamiento
//   - el solapamiento como INTERVALO y no como instante de inicio (decision
//     R7 del issue #14)
//   - el solapamiento en LOS DOS EJES: el alumno y el profesor (decision R8)
//   - la cancelacion: cambia el estado y NO borra el turno
//   - la reprogramacion: crea un turno nuevo, deja el viejo como
//     `Reprogramado` y no esquiva las reglas de `agendar`
//   - que el FAKE en memoria cumpla el contrato completo de `IRepositorio`
//
// POR QUE ESTOS TESTS NO TOCAN EL DISCO
//
// El servicio escribe contra `IRepositorio<T>`, que es una INTERFAZ. Eso
// permite inyectarle un fake en memoria: las reglas se prueban sin
// archivos, sin directorios temporales y sin limpiar nada. El fake esta
// definido mas abajo, en este mismo archivo.
// ---------------------------------------------------------------------------

namespace {

// ---------------------------------------------------------------------------
// Utilidades de los tests
// ---------------------------------------------------------------------------

// Resultado de capturar una excepcion de un tipo concreto.
struct Fallo {
    bool lanzo = false;
    bool lanzoOtro = false;
    std::string mensaje;
};

// Captura la excepcion `E` de la funcion. `lanzo` dice si lanzo ESA
// exception; `lanzoOtro` dice si lanzo otra cosa (o ninguna: en ese caso
// `mensaje` esta vacio y el test falla solo).
template <typename E, typename F>
Fallo capturarTipo(F funcion) {
    Fallo fallo;
    try {
        funcion();
    } catch (const E& excepcion) {
        fallo.lanzo = true;
        fallo.mensaje = excepcion.what();
    } catch (...) {
        fallo.lanzoOtro = true;
    }
    return fallo;
}

// ---------------------------------------------------------------------------
// EL FAKE: un repositorio en memoria que cumple `IRepositorio<T>`.
//
// POR QUE ESTA EN EL ARCHIVO DE TEST Y NO EN EL CODIGO DE PRODUCCION
//
// El fake es material de prueba, no de la aplicacion: no lo usa nadie mas
// que los tests. Y el hecho de que se pueda escribir esto es la PRUEBA de
// que `IRepositorio<T>` es una interfaz de verdad y no una clase concreta
// disfrazada.
//
// El `alta` y el `modificar` reproducen las reglas del CRUD real (id
// duplicado y entidad inexistente), para que los tests del servicio no
// dependan de una excepcion que el repositorio de verdad lanza.
// ---------------------------------------------------------------------------
template <class T>
class RepositorioEnMemoria : public IRepositorio<T> {
public:
    void alta(const T& entidad) override {
        for (const T& existente : _entidades) {
            if (existente.getId() == entidad.getId()) {
                throw IdDuplicadoException(entidad.getId(), "Prueba");
            }
        }
        _entidades.push_back(entidad);
    }

    void modificar(const T& entidad) override {
        for (T& existente : _entidades) {
            if (existente.getId() == entidad.getId()) {
                existente = entidad;
                return;
            }
        }
        throw EntidadNoExisteException(entidad.getId(), "Prueba");
    }

    bool buscarPorId(int id, T& fuera) const override {
        for (const T& entidad : _entidades) {
            if (entidad.getId() == id) {
                fuera = entidad;
                return true;
            }
        }
        return false;
    }

    std::vector<T> listar() const override { return _entidades; }

    std::size_t contar() const override { return _entidades.size(); }

    int proximoId() const override {
        int maximo = 0;
        for (const T& entidad : _entidades) {
            if (entidad.getId() > maximo) {
                maximo = entidad.getId();
            }
        }
        return maximo + 1;
    }

    void cargar() override {}

    void persistir() override {}

    // Acceso DIRECTO a las entidades, solo para los tests: sirve para
    // comprobar que un turno NO fue borrado.
    const std::vector<T>& entidades() const { return _entidades; }

private:
    std::vector<T> _entidades;
};

// ---------------------------------------------------------------------------
// VERIFICACION EN TIEMPO DE COMPILACION: el fake cumple el contrato COMPLETO
// de `IRepositorio<T>`.
//
// POR QUE SE HACE CON `static_assert` Y NO CON UN TEST QUE CORRA
//
// Si `IRepositorio<T>` tuviera un metodo mas, el fake no compilaria (seria
// una clase abstracta). Este trait es el mismo patron que usa
// `tests/test_arbol.cpp` con `TieneOperadorMenor` y que usa
// `tests/test_repositorio.cpp` con `TieneBajaLogica`: SFINAE para preguntar
// "existe este miembro" sin romper la compilacion cuando la respuesta es
// NO.
//
// Ademas, el `static_assert` de `std::is_base_of` deja EXPLICITO que el
// fake se usa a traves de la interfaz, que es de lo que se trata.
// ---------------------------------------------------------------------------

// Detecta si el fake tiene los OCHO metodos del contrato, cada uno con la
// firma que espera `IRepositorio<T>`.
template <class F, class E, class = void>
struct CumpleContrato : std::false_type {};

template <class F, class E>
struct CumpleContrato<F, E, std::void_t<
                                 decltype(std::declval<F&>().alta(std::declval<const E&>())),
                                 decltype(std::declval<F&>().modificar(std::declval<const E&>())),
                                 decltype(std::declval<F&>().buscarPorId(0, std::declval<E&>())),
                                 decltype(std::declval<F&>().listar()),
                                 decltype(std::declval<F&>().contar()),
                                 decltype(std::declval<F&>().proximoId()),
                                 decltype(std::declval<F&>().cargar()),
                                 decltype(std::declval<F&>().persistir())>> : std::true_type {};

// El fake se usa SIEMPRE a traves de la interfaz.
static_assert(std::is_base_of<IRepositorio<Alumno>, RepositorioEnMemoria<Alumno>>::value,
              "el fake de alumnos implementa IRepositorio<Alumno>");
static_assert(std::is_base_of<IRepositorio<Profesor>, RepositorioEnMemoria<Profesor>>::value,
              "el fake de profesores implementa IRepositorio<Profesor>");
static_assert(std::is_base_of<IRepositorio<Turno>, RepositorioEnMemoria<Turno>>::value,
              "el fake de turnos implementa IRepositorio<Turno>");

// Y cumple el contrato COMPLETO: si le faltara un metodo, el fake no
// compilaria y este `static_assert` tampoco.
static_assert(CumpleContrato<RepositorioEnMemoria<Alumno>, Alumno>::value,
              "el fake de alumnos cumple el contrato completo de IRepositorio");
static_assert(CumpleContrato<RepositorioEnMemoria<Profesor>, Profesor>::value,
              "el fake de profesores cumple el contrato completo de IRepositorio");
static_assert(CumpleContrato<RepositorioEnMemoria<Turno>, Turno>::value,
              "el fake de turnos cumple el contrato completo de IRepositorio");

// ---------------------------------------------------------------------------
// Datos de prueba
// ---------------------------------------------------------------------------

Alumno alumnoDePrueba(int id) {
    return Alumno(id, "Ana", "Gomez", "alumno" + std::to_string(id) + "@unlp.edu.ar",
                  "2214556677", "MAT-2" + std::to_string(id));
}

Profesor profesorDePrueba(int id) {
    return Profesor(id, "Beto", "Perez", "profesor" + std::to_string(id) + "@unlp.edu.ar",
                    "2214556677", "LEG-1" + std::to_string(id), "Algoritmos I");
}

// La fecha de HOY, segun el reloj del sistema. Es la misma referencia que
// usa el servicio para la regla de la fecha vencida.
Fecha hoyDelSistema() {
    const std::time_t ahora = std::time(nullptr);
    const std::tm* local = std::localtime(&ahora);
    return Fecha(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday);
}

// Una fecha `dias` dias a partir de hoy (negativo = antes).
//
// POR QUE NO UNA FECHA FIJA
//
// El servicio RECHAZA las fechas que ya pasaron. Una fecha fija dejaria de
// servir el dia que se cumpliera, y el test pasaria a fallar solo. Derivar
// la fecha del reloj hace que el test siga teniendo sentido siempre.
Fecha fechaDesplazada(int dias) {
    const Fecha hoy = hoyDelSistema();
    int anio = hoy.anio;
    int mes = hoy.mes;
    int dia = hoy.dia;

    while (dias > 0) {
        ++dia;
        if (dia > Fecha::diasDelMes(anio, mes)) {
            dia = 1;
            ++mes;
            if (mes > 12) {
                mes = 1;
                ++anio;
            }
        }
        --dias;
    }

    while (dias < 0) {
        --dia;
        if (dia < 1) {
            --mes;
            if (mes < 1) {
                mes = 12;
                --anio;
            }
            dia = Fecha::diasDelMes(anio, mes);
        }
        ++dias;
    }

    return Fecha(anio, mes, dia);
}

// Un turno valido, con la duracion por defecto (45 minutos).
Turno turnoDePrueba(int id, int idProfesor, int idAlumno, int hora, int minuto) {
    return Turno(id, idProfesor, idAlumno, Materia::ALG1, fechaDesplazada(30),
                 Hora(hora, minuto), 45, "A-1");
}

// El mundo de pruebas: tres fakes y el servicio que los maneja.
struct Escenario {
    RepositorioEnMemoria<Alumno> alumnos;
    RepositorioEnMemoria<Profesor> profesores;
    RepositorioEnMemoria<Turno> turnos;
    ServicioTurnos servicio;

    Escenario() : servicio(alumnos, profesores, turnos) {}

    // Un alumno y un profesor validos, activos.
    void conPersonas(int idAlumno, int idProfesor) {
        alumnos.alta(alumnoDePrueba(idAlumno));
        profesores.alta(profesorDePrueba(idProfesor));
    }
};

} // namespace

// ---------------------------------------------------------------------------
// AGENDAR: EXISTENCIA Y ESTADO DE LAS PERSONAS
// ---------------------------------------------------------------------------

TEST_CASE("agendar con un alumno que no existe lanza EntidadNoExisteException",
          servicio_agendar_alumno_inexistente) {
    Escenario escenario;
    escenario.profesores.alta(profesorDePrueba(1));

    const Turno turno = turnoDePrueba(1, 1, 99, 10, 0);

    const Fallo fallo = capturarTipo<EntidadNoExisteException>(
        [&] { escenario.servicio.agendar(turno); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(fallo.mensaje.find("99") != std::string::npos);
    CHECK(fallo.mensaje.find("Alumno") != std::string::npos);

    // Y NO se guardo el turno.
    CHECK(escenario.turnos.contar() == 0);
}

TEST_CASE("agendar con un alumno INACTIVO lanza DatoInvalidoException",
          servicio_agendar_alumno_inactivo) {
    Escenario escenario;
    escenario.profesores.alta(profesorDePrueba(1));

    Alumno alumno = alumnoDePrueba(1);
    alumno.setActivo(false);
    escenario.alumnos.alta(alumno);

    const Turno turno = turnoDePrueba(1, 1, 1, 10, 0);

    const ErrorCapturado error = capturarError([&] { escenario.servicio.agendar(turno); });
    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "alumno");

    // Y NO se guardo el turno.
    CHECK(escenario.turnos.contar() == 0);
}

TEST_CASE("agendar con un profesor que no existe lanza EntidadNoExisteException",
          servicio_agendar_profesor_inexistente) {
    Escenario escenario;
    escenario.alumnos.alta(alumnoDePrueba(1));

    const Turno turno = turnoDePrueba(1, 99, 1, 10, 0);

    const Fallo fallo = capturarTipo<EntidadNoExisteException>(
        [&] { escenario.servicio.agendar(turno); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(fallo.mensaje.find("99") != std::string::npos);
    CHECK(fallo.mensaje.find("Profesor") != std::string::npos);

    // Y NO se guardo el turno.
    CHECK(escenario.turnos.contar() == 0);
}

TEST_CASE("agendar con un profesor INACTIVO lanza DatoInvalidoException",
          servicio_agendar_profesor_inactivo) {
    Escenario escenario;
    escenario.alumnos.alta(alumnoDePrueba(1));

    Profesor profesor = profesorDePrueba(1);
    profesor.setActivo(false);
    escenario.profesores.alta(profesor);

    const Turno turno = turnoDePrueba(1, 1, 1, 10, 0);

    const ErrorCapturado error = capturarError([&] { escenario.servicio.agendar(turno); });
    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "profesor");

    // Y NO se guardo el turno.
    CHECK(escenario.turnos.contar() == 0);
}

// ---------------------------------------------------------------------------
// AGENDAR: LA FECHA
// ---------------------------------------------------------------------------

TEST_CASE("agendar con una fecha en el pasado lanza DatoInvalidoException",
          servicio_agendar_fecha_pasada) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    const Turno turno(1, 1, 1, Materia::ALG1, fechaDesplazada(-30), Hora(10, 0), 45, "A-1");

    const ErrorCapturado error = capturarError([&] { escenario.servicio.agendar(turno); });
    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "fecha");

    // Y NO se guardo el turno.
    CHECK(escenario.turnos.contar() == 0);
}

TEST_CASE("agendar con la fecha de HOY si se permite", servicio_agendar_fecha_de_hoy) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    const Turno turno(1, 1, 1, Materia::ALG1, fechaDesplazada(0), Hora(10, 0), 45, "A-1");

    // El dia de hoy NO esta en el pasado: se agenda.
    const Fallo fallo = capturarTipo<ExcepcionSistema>(
        [&] { escenario.servicio.agendar(turno); });
    CHECK(!fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(escenario.turnos.contar() == 1);
}

// ---------------------------------------------------------------------------
// AGENDAR: LOS DATOS DEL TURNO
// ---------------------------------------------------------------------------

TEST_CASE("agendar un turno armado a mano sin datos validos lanza EntidadNoExisteException",
          servicio_agendar_turno_mal_armado) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    // El `Turno` por DEFECTO es un turno mal armado: no tiene id, ni
    // alumno, ni profesor, ni duracion, ni aula.
    //
    // El constructor con datos SI valida todo, asi que este es el caso real
    // de "el llamador no construyo el turno bien". El servicio lo rechaza
    // por la PRIMERA regla que no se cumple: el alumno que pide el turno no
    // existe. Que la primera regla que falla sea la del alumno y no la del
    // id es lo que exige el orden de reglas.
    const Turno turno;

    const Fallo fallo = capturarTipo<EntidadNoExisteException>(
        [&] { escenario.servicio.agendar(turno); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(fallo.mensaje.find("Alumno") != std::string::npos);

    // Y NO se guardo el turno.
    CHECK(escenario.turnos.contar() == 0);
}

TEST_CASE("agendar un turno con un id invalido lanza DatoInvalidoException",
          servicio_agendar_id_invalido) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    // Un turno con alumno y profesor VALIDOS pero sin id: el id lo propone
    // el repositorio (decision R4 del issue #14), asi que un turno sin id no
    // se puede guardar. Para armarlo asi hay que partir del `Turno` por
    // defecto (que es el unico constructor que no valida) y completar los
    // datos que el servicio mira antes que el id.
    Turno turno;
    turno.setIdProfesor(1);
    turno.setIdAlumno(1);
    turno.setMateria(Materia::ALG1);
    turno.setFecha(fechaDesplazada(30));
    turno.setHora(Hora(10, 0));
    turno.setAula("A-1");

    const ErrorCapturado error = capturarError([&] { escenario.servicio.agendar(turno); });
    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "id");

    // Y NO se guardo el turno.
    CHECK(escenario.turnos.contar() == 0);
}

TEST_CASE("agendar un turno que termina despues de las 22:00 lanza DatoInvalidoException",
          servicio_agendar_turno_fuera_del_horario) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    // Un turno de 21:30 que dura 90 minutos termina a las 23:00: la clase
    // se esta dictando cuando la institucion ya no atiende.
    const Turno turno(1, 1, 1, Materia::ALG1, fechaDesplazada(30), Hora(21, 30), 90, "A-1");

    const ErrorCapturado error = capturarError([&] { escenario.servicio.agendar(turno); });
    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "hora");

    // Y NO se guardo el turno.
    CHECK(escenario.turnos.contar() == 0);
}

// ---------------------------------------------------------------------------
// AGENDAR: EL SOLAPAMIENTO COMO INTERVALO (decision R7)
// ---------------------------------------------------------------------------

TEST_CASE("un turno que pisa PARCIALMENTE a otro lanza TurnoOcupadoException",
          servicio_solapamiento_parcial) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    // El primero ocupa de 10:00 a 10:45.
    const Turno primero = turnoDePrueba(1, 1, 1, 10, 0);
    escenario.servicio.agendar(primero);

    // El segundo arranca a las 10:30 y llega hasta las 11:15: pisa los
    // ultimos 15 minutos del primero.
    //
    // ESTE es el test que PRUEBA que el solapamiento se compara por
    // INTERVALO y no por instante de inicio (decision R7 del issue #14).
    //
    // Sus horas de inicio son DISTINTAS (10:00 y 10:30), asi que una
    // comparacion ingenua de los inicios NO los detecta: con el
    // `operator<` de la `Hora`, "10:00 < 10:30" es VERDADERO pero
    // "10:30 < 10:00" es FALSO, y una comparacion que exija las dos
    // concludediria que los dos turnos se pisan. O al reves, si se usara
    // "menor o igual", el criterio seria tan fuerte que dos turnos que se
    // tocan JUSTO (uno termina cuando el otro arranca) tambien se
    // pisarian.
    //
    // El criterio correcto es el de los INTERVALOS: `(inicioA < finB) &&
    // (inicioB < finA)`.
    const Turno segundo = turnoDePrueba(2, 1, 1, 10, 30);

    // Y esta es la asercion que hace el trabajo: con una comparacion
    // ingenua por inicio, este `fallo.lanzo` seria FALSO y el turno se
    // guardaria aunque pise al otro.
    const Fallo fallo = capturarTipo<TurnoOcupadoException>(
        [&] { escenario.servicio.agendar(segundo); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);

    // Y el turno que se respeta es el que YA ESTABA.
    CHECK(fallo.mensaje.find("#1") != std::string::npos);

    // Y NO se guardo el segundo turno.
    CHECK(escenario.turnos.contar() == 1);
}

TEST_CASE("un turno que arranca EXACTAMENTE en el mismo horario lanza TurnoOcupadoException",
          servicio_solapamiento_exacto) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    const Turno primero = turnoDePrueba(1, 1, 1, 10, 0);
    escenario.servicio.agendar(primero);

    const Turno segundo = turnoDePrueba(2, 1, 1, 10, 0);

    const Fallo fallo = capturarTipo<TurnoOcupadoException>(
        [&] { escenario.servicio.agendar(segundo); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(escenario.turnos.contar() == 1);
}

TEST_CASE("un turno que empieza JUSTO cuando termina el otro NO se pisa",
          servicio_solapamiento_por_empalme) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    // De 10:00 a 10:30.
    const Turno primero(1, 1, 1, Materia::ALG1, fechaDesplazada(30), Hora(10, 0), 30, "A-1");
    escenario.servicio.agendar(primero);

    // De 10:30 a 11:00: arranca EXACTAMENTE cuando termina el primero, asi
    // que NO hay choque. Este es el caso limite del criterio de intervalos:
    // se solapan si `inicioA < finB` Y `inicioB < finA`, y aqui
    // `10:30 < 10:30` es falso.
    const Turno pegado(2, 1, 1, Materia::ALG1, fechaDesplazada(30), Hora(10, 30), 30, "A-1");

    const Fallo fallo = capturarTipo<ExcepcionSistema>(
        [&] { escenario.servicio.agendar(pegado); });
    CHECK(!fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(escenario.turnos.contar() == 2);
}

TEST_CASE("dos turnos en horarios separados se agendan sin problema",
          servicio_sin_solapamiento) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    // De 10:00 a 10:45.
    const Turno primero = turnoDePrueba(1, 1, 1, 10, 0);
    escenario.servicio.agendar(primero);

    // De 11:00 a 11:45: hay un hueco de quince minutos.
    const Turno segundo = turnoDePrueba(2, 1, 1, 11, 0);

    const Fallo fallo = capturarTipo<ExcepcionSistema>(
        [&] { escenario.servicio.agendar(segundo); });
    CHECK(!fallo.lanzo);
    CHECK(!fallo.lanzoOtro);

    // Los dos turnos estan guardados.
    CHECK(escenario.turnos.contar() == 2);
}

// ---------------------------------------------------------------------------
// AGENDAR: EL SOLAPAMIENTO EN LOS DOS EJES (decision R8)
// ---------------------------------------------------------------------------

TEST_CASE("el solapamiento por el ALUMNO lanza TurnoOcupadoException",
          servicio_solapamiento_eje_alumno) {
    Escenario escenario;
    escenario.conPersonas(1, 1);
    escenario.conPersonas(2, 2);

    // El primero es del alumno 1 con el profesor 1.
    const Turno primero = turnoDePrueba(1, 1, 1, 10, 0);
    escenario.servicio.agendar(primero);

    // El segundo es del MISMO alumno pero de otro profesor: el alumno esta
    // ocupado aunque el profesor sea distinto.
    const Turno segundo = turnoDePrueba(2, 2, 1, 10, 0);

    const Fallo fallo = capturarTipo<TurnoOcupadoException>(
        [&] { escenario.servicio.agendar(segundo); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(escenario.turnos.contar() == 1);
}

TEST_CASE("el solapamiento por el PROFESOR lanza TurnoOcupadoException",
          servicio_solapamiento_eje_profesor) {
    Escenario escenario;
    escenario.conPersonas(1, 1);
    escenario.conPersonas(2, 2);

    // El primero es del alumno 1 con el profesor 1.
    const Turno primero = turnoDePrueba(1, 1, 1, 10, 0);
    escenario.servicio.agendar(primero);

    // El segundo es de OTRO alumno pero del MISMO profesor: el profesor esta
    // ocupado aunque el alumno sea distinto.
    const Turno segundo = turnoDePrueba(2, 1, 2, 10, 0);

    const Fallo fallo = capturarTipo<TurnoOcupadoException>(
        [&] { escenario.servicio.agendar(segundo); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(escenario.turnos.contar() == 1);
}

TEST_CASE("el detalle del turno ocupado dice que profesor y que alumno estan ocupados",
          servicio_detalle_del_turno_ocupado) {
    Escenario escenario;
    escenario.conPersonas(1, 1);
    escenario.conPersonas(2, 2);

    const Turno primero = turnoDePrueba(7, 1, 1, 10, 0);
    escenario.servicio.agendar(primero);

    const Turno segundo = turnoDePrueba(8, 2, 1, 10, 0);

    const Fallo fallo = capturarTipo<TurnoOcupadoException>(
        [&] { escenario.servicio.agendar(segundo); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);

    // El detalle tiene el formato exacto "idProfesor=N idAlumno=M": es el
    // mismo que usa el mensaje de la excepcion de la Fase 1.
    CHECK(fallo.mensaje.find("idProfesor=1") != std::string::npos);
    CHECK(fallo.mensaje.find("idAlumno=1") != std::string::npos);
}

TEST_CASE("un turno cancelado libera su horario", servicio_el_cancelado_libera_el_horario) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    const Turno primero = turnoDePrueba(1, 1, 1, 10, 0);
    escenario.servicio.agendar(primero);
    escenario.servicio.cancelar(1);

    // El mismo horario, con el mismo alumno y el mismo profesor, ahora se
    // puede volver a usar: un turno cancelado no ocupa nada.
    const Turno segundo = turnoDePrueba(2, 1, 1, 10, 0);

    const Fallo fallo = capturarTipo<ExcepcionSistema>(
        [&] { escenario.servicio.agendar(segundo); });
    CHECK(!fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(escenario.turnos.contar() == 2);
}

// ---------------------------------------------------------------------------
// CANCELAR
// ---------------------------------------------------------------------------

TEST_CASE("cancelar deja el turno en estado Cancelado y NO lo borra",
          servicio_cancelar_no_borra) {
    Escenario escenario;
    escenario.conPersonas(1, 1);
    escenario.servicio.agendar(turnoDePrueba(1, 1, 1, 10, 0));

    escenario.servicio.cancelar(1);

    // El turno SIGUE en el repositorio: la baja es LOGICA (decision #10 del
    // proyecto).
    CHECK(escenario.turnos.contar() == 1);
    CHECK(escenario.turnos.entidades().size() == 1);

    Turno encontrado;
    CHECK(escenario.turnos.buscarPorId(1, encontrado));
    CHECK(encontrado.getEstado() == EstadoTurno::Cancelado);

    // Los datos del turno NO se tocan al cancelar.
    CHECK(encontrado.getIdAlumno() == 1);
    CHECK(encontrado.getIdProfesor() == 1);
    CHECK(encontrado.getHora() == Hora(10, 0));
}

TEST_CASE("cancelar un turno que no existe lanza EntidadNoExisteException",
          servicio_cancelar_inexistente) {
    Escenario escenario;

    const Fallo fallo = capturarTipo<EntidadNoExisteException>(
        [&] { escenario.servicio.cancelar(99); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(fallo.mensaje.find("99") != std::string::npos);
    CHECK(fallo.mensaje.find("Turno") != std::string::npos);
}

TEST_CASE("cancelar un turno ya cancelado lanza DatoInvalidoException",
          servicio_cancelar_dos_veces) {
    Escenario escenario;
    escenario.conPersonas(1, 1);
    escenario.servicio.agendar(turnoDePrueba(1, 1, 1, 10, 0));

    escenario.servicio.cancelar(1);

    const ErrorCapturado error = capturarError([&] { escenario.servicio.cancelar(1); });
    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "estado");

    // Y el turno sigue cancelado (no se rompio nada).
    Turno encontrado;
    CHECK(escenario.turnos.buscarPorId(1, encontrado));
    CHECK(encontrado.getEstado() == EstadoTurno::Cancelado);
}

// ---------------------------------------------------------------------------
// REPROGRAMAR
// ---------------------------------------------------------------------------

TEST_CASE("reprogramar crea un turno NUEVO y deja el viejo como Reprogramado",
          servicio_reprogramar_crea_turno_nuevo) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    const Turno original = turnoDePrueba(1, 1, 1, 10, 0);
    escenario.servicio.agendar(original);

    const Fecha nuevaFecha = fechaDesplazada(45);
    escenario.servicio.reprogramar(1, nuevaFecha, Hora(11, 0));

    // Ahora hay DOS turnos: el viejo y el nuevo.
    CHECK(escenario.turnos.contar() == 2);

    // El viejo quedo `Reprogramado` y apunta al nuevo.
    Turno viejo;
    CHECK(escenario.turnos.buscarPorId(1, viejo));
    CHECK(viejo.getEstado() == EstadoTurno::Reprogramado);
    CHECK(viejo.getIdTurnoReemplazante() == 2);

    // Y el nuevo tiene id PROPIO, esta activo y took la fecha y la hora
    // nuevas, pero conservo alumno, profesor, materia, duracion y aula.
    Turno nuevo;
    CHECK(escenario.turnos.buscarPorId(2, nuevo));
    CHECK(nuevo.getEstado() == EstadoTurno::Activo);
    CHECK(nuevo.getIdTurnoReemplazante() == -1);
    CHECK(nuevo.getFecha() == nuevaFecha);
    CHECK(nuevo.getHora() == Hora(11, 0));
    CHECK(nuevo.getIdAlumno() == original.getIdAlumno());
    CHECK(nuevo.getIdProfesor() == original.getIdProfesor());
    CHECK(nuevo.getMateria() == original.getMateria());
    CHECK(nuevo.getDuracionMin() == original.getDuracionMin());
    CHECK(nuevo.getAula() == original.getAula());
}

TEST_CASE("reprogramar a un horario libre funciona aunque el turno viejo siga en el repositorio",
          servicio_reprogramar_a_horario_libre) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    // El turno original ocupa de 10:00 a 10:45.
    const Turno original = turnoDePrueba(1, 1, 1, 10, 0);
    escenario.servicio.agendar(original);

    // Se reprograma a las 11:00: un horario libre. El turno viejo SIGUE
    // en el repositorio y en estado `Activo` mientras se valida el nuevo,
    // asi que este caso comprueba que el turno viejo no bloquea el
    // destino.
    const Fecha nuevaFecha = fechaDesplazada(30);
    const Fallo fallo = capturarTipo<ExcepcionSistema>(
        [&] { escenario.servicio.reprogramar(1, nuevaFecha, Hora(11, 0)); });
    CHECK(!fallo.lanzo);
    CHECK(!fallo.lanzoOtro);

    // Los dos turnos quedan en el repositorio: el viejo `Reprogramado` y el
    // nuevo `Activo`.
    CHECK(escenario.turnos.contar() == 2);
    Turno viejo;
    CHECK(escenario.turnos.buscarPorId(1, viejo));
    CHECK(viejo.getEstado() == EstadoTurno::Reprogramado);
}

TEST_CASE("reprogramar un turno que no existe lanza EntidadNoExisteException",
          servicio_reprogramar_inexistente) {
    Escenario escenario;

    const Fallo fallo = capturarTipo<EntidadNoExisteException>(
        [&] { escenario.servicio.reprogramar(99, fechaDesplazada(45), Hora(11, 0)); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);
    CHECK(fallo.mensaje.find("99") != std::string::npos);
    CHECK(fallo.mensaje.find("Turno") != std::string::npos);
}

TEST_CASE("reprogramar un turno CANCELADO lanza DatoInvalidoException",
          servicio_reprogramar_cancelado) {
    Escenario escenario;
    escenario.conPersonas(1, 1);
    escenario.servicio.agendar(turnoDePrueba(1, 1, 1, 10, 0));
    escenario.servicio.cancelar(1);

    const ErrorCapturado error = capturarError(
        [&] { escenario.servicio.reprogramar(1, fechaDesplazada(45), Hora(11, 0)); });
    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "estado");

    // Y no se creo ningun turno nuevo.
    CHECK(escenario.turnos.contar() == 1);
}

TEST_CASE("reprogramar a un horario que se pisa lanza TurnoOcupadoException y no modifica nada",
          servicio_reprogramar_a_horario_ocupado) {
    Escenario escenario;
    escenario.conPersonas(1, 1);

    // El turno que se va a reprogramar.
    const Turno original = turnoDePrueba(1, 1, 1, 10, 0);
    escenario.servicio.agendar(original);

    // Y otro turno que ocupa el horario de destino.
    const Turno ocupado = turnoDePrueba(2, 1, 1, 15, 0);
    escenario.servicio.agendar(ocupado);

    const Fallo fallo = capturarTipo<TurnoOcupadoException>(
        [&] { escenario.servicio.reprogramar(1, fechaDesplazada(30), Hora(15, 0)); });
    CHECK(fallo.lanzo);
    CHECK(!fallo.lanzoOtro);

    // NINGUN turno se modifico: el viejo sigue ACTIVO y el que ocupaba el
    // horario sigue intacto.
    Turno viejo;
    CHECK(escenario.turnos.buscarPorId(1, viejo));
    CHECK(viejo.getEstado() == EstadoTurno::Activo);
    CHECK(viejo.getIdTurnoReemplazante() == -1);

    Turno otro;
    CHECK(escenario.turnos.buscarPorId(2, otro));
    CHECK(otro.getEstado() == EstadoTurno::Activo);

    // Y no se creo ningun turno nuevo.
    CHECK(escenario.turnos.contar() == 2);
}

TEST_CASE("reprogramar a una fecha que ya paso lanza DatoInvalidoException",
          servicio_reprogramar_fecha_pasada) {
    Escenario escenario;
    escenario.conPersonas(1, 1);
    escenario.servicio.agendar(turnoDePrueba(1, 1, 1, 10, 0));

    const ErrorCapturado error = capturarError(
        [&] { escenario.servicio.reprogramar(1, fechaDesplazada(-5), Hora(11, 0)); });
    CHECK(error.lanzo);
    CHECK(!error.lanzoOtraExcepcion);
    CHECK(error.campo == "fecha");

    // Y el turno viejo sigue ACTIVO.
    Turno viejo;
    CHECK(escenario.turnos.buscarPorId(1, viejo));
    CHECK(viejo.getEstado() == EstadoTurno::Activo);
    CHECK(escenario.turnos.contar() == 1);
}

// ---------------------------------------------------------------------------
// TURNOS VIGENTES DE UN ALUMNO
// ---------------------------------------------------------------------------

TEST_CASE("turnosVigentesDe devuelve solo los turnos activos", servicio_turnos_vigentes) {
    Escenario escenario;
    escenario.conPersonas(1, 1);
    escenario.conPersonas(2, 2);

    // Dos turnos del alumno 1.
    escenario.servicio.agendar(turnoDePrueba(1, 1, 1, 10, 0));
    escenario.servicio.agendar(turnoDePrueba(2, 2, 1, 11, 0));

    // Y uno del alumno 2.
    escenario.servicio.agendar(turnoDePrueba(3, 2, 2, 12, 0));

    // El del alumno 1 de las 12:00 se cancela.
    escenario.servicio.cancelar(2);

    const std::vector<Turno> vigentes = escenario.servicio.turnosVigentesDe(1);
    CHECK(vigentes.size() == 1);
    CHECK(vigentes.front().getId() == 1);

    // El alumno 2 conserva el suyo.
    const std::vector<Turno> delOtro = escenario.servicio.turnosVigentesDe(2);
    CHECK(delOtro.size() == 1);
    CHECK(delOtro.front().getId() == 3);

    // Y un alumno que no tiene turnos no falla: la ausencia es un dato de
    // la consulta, no un error.
    const std::vector<Turno> deNinguno = escenario.servicio.turnosVigentesDe(99);
    CHECK(deNinguno.empty());
}

TEST_CASE("un turno reprogramado no cuenta como vigente", servicio_reprogramado_no_vigente) {
    Escenario escenario;
    escenario.conPersonas(1, 1);
    escenario.servicio.agendar(turnoDePrueba(1, 1, 1, 10, 0));

    escenario.servicio.reprogramar(1, fechaDesplazada(45), Hora(11, 0));

    // El turno viejo quedo `Reprogramado`: no esta vigente. El nuevo si.
    const std::vector<Turno> vigentes = escenario.servicio.turnosVigentesDe(1);
    CHECK(vigentes.size() == 1);
    CHECK(vigentes.front().getId() == 2);
}

// ---------------------------------------------------------------------------
// EL FAKE CUMPLE EL CONTRATO
// ---------------------------------------------------------------------------

TEST_CASE("el fake en memoria cumple el contrato completo de IRepositorio",
          servicio_fake_cumple_contrato) {
    // La comprobacion FUERTE es el `static_assert` de arriba: si al fake le
    // faltara un metodo del contrato, el codigo no compilaria.
    //
    // Aca se mide lo que se puede medir en EJECUCION: que el fake se
    // comporte como un repositorio de verdad.
    RepositorioEnMemoria<Turno> fake;

    CHECK(fake.contar() == 0);
    CHECK(fake.proximoId() == 1);

    fake.alta(turnoDePrueba(1, 1, 1, 10, 0));
    fake.alta(turnoDePrueba(7, 1, 1, 11, 0));

    CHECK(fake.contar() == 2);
    CHECK(fake.proximoId() == 8);

    Turno encontrado;
    CHECK(fake.buscarPorId(7, encontrado));
    CHECK(encontrado.getId() == 7);
    CHECK(!fake.buscarPorId(99, encontrado));

    CHECK(fake.listar().size() == 2);

    // Un id duplicado se rechaza, como en el repositorio de verdad.
    const Fallo fallo = capturarTipo<IdDuplicadoException>(
        [&] { fake.alta(turnoDePrueba(7, 1, 1, 12, 0)); });
    CHECK(fallo.lanzo);
    CHECK(fake.contar() == 2);

    // Y `modificar` REEMPLAZA: no deja dos turnos con el mismo id.
    Turno modificado = encontrado;
    modificado.setEstado(EstadoTurno::Cancelado);
    fake.modificar(modificado);
    CHECK(fake.contar() == 2);
    CHECK(fake.buscarPorId(7, encontrado));
    CHECK(encontrado.getEstado() == EstadoTurno::Cancelado);
}
