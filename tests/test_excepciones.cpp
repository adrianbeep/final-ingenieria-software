#include "test_framework.h"

#include "exceptions/ExcepcionesSistema.h"

#include <stdexcept>
#include <string>
#include <type_traits>

// ---------------------------------------------------------------------------
// Tests de la jerarquia de excepciones.
//
// Se verifica:
//   - que toda excepcion derivada se pueda capturar como `ExcepcionSistema`
//     (esa es la forma en que la UI las maneja),
//   - que los mensajes sean los exactos (son la interfaz con el usuario),
//   - que `ExcepcionSistema` herede de `std::runtime_error` y NO de
//     `std::logic_error`.
// ---------------------------------------------------------------------------

TEST_CASE("todas las excepciones del sistema heredan de ExcepcionSistema", excepciones_herencia) {
    static_assert(std::is_base_of<ExcepcionSistema, ArchivoNoEncontradoException>::value,
                  "ArchivoNoEncontradoException debe derivar de ExcepcionSistema");
    static_assert(std::is_base_of<ExcepcionSistema, IdDuplicadoException>::value,
                  "IdDuplicadoException debe derivar de ExcepcionSistema");
    static_assert(std::is_base_of<ExcepcionSistema, DatoInvalidoException>::value,
                  "DatoInvalidoException debe derivar de ExcepcionSistema");
    static_assert(std::is_base_of<ExcepcionSistema, EntidadNoExisteException>::value,
                  "EntidadNoExisteException debe derivar de ExcepcionSistema");
    static_assert(std::is_base_of<ExcepcionSistema, TurnoOcupadoException>::value,
                  "TurnoOcupadoException debe derivar de ExcepcionSistema");
    static_assert(std::is_base_of<ExcepcionSistema, TurnoNoExisteException>::value,
                  "TurnoNoExisteException debe derivar de ExcepcionSistema");

    static_assert(std::is_base_of<std::runtime_error, ExcepcionSistema>::value,
                  "ExcepcionSistema debe derivar de std::runtime_error");
    static_assert(!std::is_base_of<std::logic_error, ExcepcionSistema>::value,
                  "ExcepcionSistema no debe derivar de std::logic_error");

    CHECK(true);
}

TEST_CASE("cada excepcion derivada se captura como ExcepcionSistema", excepciones_captura_base) {
    bool archivo = false;
    bool duplicado = false;
    bool dato = false;
    bool entidad = false;
    bool ocupado = false;
    bool turno = false;

    try {
        throw ArchivoNoEncontradoException("data/pacientes.dat");
    } catch (const ExcepcionSistema&) {
        archivo = true;
    } catch (...) {
    }

    try {
        throw IdDuplicadoException(7, "Paciente");
    } catch (const ExcepcionSistema&) {
        duplicado = true;
    } catch (...) {
    }

    try {
        throw DatoInvalidoException("edad", "abc", "se esperaba un entero positivo");
    } catch (const ExcepcionSistema&) {
        dato = true;
    } catch (...) {
    }

    try {
        throw EntidadNoExisteException(7, "Paciente");
    } catch (const ExcepcionSistema&) {
        entidad = true;
    } catch (...) {
    }

    try {
        throw TurnoOcupadoException(12, "idMedico=3 idPaciente=7");
    } catch (const ExcepcionSistema&) {
        ocupado = true;
    } catch (...) {
    }

    try {
        throw TurnoNoExisteException(42);
    } catch (const ExcepcionSistema&) {
        turno = true;
    } catch (...) {
    }

    CHECK(archivo);
    CHECK(duplicado);
    CHECK(dato);
    CHECK(entidad);
    CHECK(ocupado);
    CHECK(turno);
}

TEST_CASE("ninguna excepcion del sistema tiene el mensaje vacio", excepciones_mensaje_no_vacio) {
    const ExcepcionSistema excepciones[] = {
        ExcepcionSistema("prueba"),
        ArchivoNoEncontradoException("data/pacientes.dat"),
        IdDuplicadoException(7, "Paciente"),
        DatoInvalidoException("edad", "abc", "se esperaba un entero positivo"),
        EntidadNoExisteException(7, "Paciente"),
        TurnoOcupadoException(12, "idMedico=3 idPaciente=7"),
        TurnoNoExisteException(42),
    };

    for (const ExcepcionSistema& excepcion : excepciones) {
        CHECK(std::string(excepcion.what()).size() > 0);
    }
}

TEST_CASE("el mensaje de ArchivoNoEncontradoException incluye la ruta", excepciones_archivo) {
    bool capturada = false;
    try {
        throw ArchivoNoEncontradoException("data/pacientes.dat");
    } catch (const ExcepcionSistema& e) {
        capturada = true;
        CHECK(std::string(e.what()) == "No se encontro el archivo 'data/pacientes.dat'");
        CHECK(std::string(e.what()).find("data/pacientes.dat") != std::string::npos);
    } catch (...) {
    }
    CHECK(capturada);
}

TEST_CASE("el mensaje de IdDuplicadoException incluye el id y la entidad", excepciones_id_duplicado) {
    bool capturada = false;
    try {
        throw IdDuplicadoException(7, "Paciente");
    } catch (const ExcepcionSistema& e) {
        capturada = true;
        CHECK(std::string(e.what()) == "Id duplicado: 7 (Paciente)");
        CHECK(std::string(e.what()).find("7") != std::string::npos);
        CHECK(std::string(e.what()).find("Paciente") != std::string::npos);
    } catch (...) {
    }
    CHECK(capturada);
}

TEST_CASE("el mensaje de DatoInvalidoException incluye campo, valor y motivo", excepciones_dato_invalido) {
    bool capturada = false;
    try {
        throw DatoInvalidoException("edad", "abc", "se esperaba un entero positivo");
    } catch (const ExcepcionSistema& e) {
        capturada = true;
        CHECK(std::string(e.what()) ==
              "Dato invalido en campo 'edad': 'abc' (se esperaba un entero positivo)");
        CHECK(std::string(e.what()).find("edad") != std::string::npos);
        CHECK(std::string(e.what()).find("abc") != std::string::npos);
        CHECK(std::string(e.what()).find("se esperaba un entero positivo") != std::string::npos);
    } catch (...) {
    }
    CHECK(capturada);
}

TEST_CASE("DatoInvalidoException expone el campo y el valor que fallaron", excepciones_dato_campo_valor) {
    bool capturada = false;
    try {
        throw DatoInvalidoException("edad", "abc", "se esperaba un entero positivo");
    } catch (const DatoInvalidoException& e) {
        capturada = true;
        CHECK(e.campo() == "edad");
        CHECK(e.valor() == "abc");
    } catch (...) {
    }
    CHECK(capturada);
}

TEST_CASE("el mensaje de EntidadNoExisteException incluye el id y la entidad", excepciones_entidad) {
    bool capturada = false;
    try {
        throw EntidadNoExisteException(7, "Paciente");
    } catch (const ExcepcionSistema& e) {
        capturada = true;
        CHECK(std::string(e.what()) == "No existe la entidad #7 (Paciente)");
        CHECK(std::string(e.what()).find("7") != std::string::npos);
        CHECK(std::string(e.what()).find("Paciente") != std::string::npos);
    } catch (...) {
    }
    CHECK(capturada);
}

TEST_CASE("el mensaje de TurnoOcupadoException incluye el turno y el detalle", excepciones_turno_ocupado) {
    bool capturada = false;
    try {
        throw TurnoOcupadoException(12, "idMedico=3 idPaciente=7");
    } catch (const ExcepcionSistema& e) {
        capturada = true;
        CHECK(std::string(e.what()) ==
              "El turno #12 ya ocupa ese horario (idMedico=3 idPaciente=7)");
        CHECK(std::string(e.what()).find("12") != std::string::npos);
        CHECK(std::string(e.what()).find("idMedico=3 idPaciente=7") != std::string::npos);
    } catch (...) {
    }
    CHECK(capturada);
}

TEST_CASE("el mensaje de TurnoNoExisteException incluye el id", excepciones_turno_no_existe) {
    bool capturada = false;
    try {
        throw TurnoNoExisteException(42);
    } catch (const ExcepcionSistema& e) {
        capturada = true;
        CHECK(std::string(e.what()) == "No existe el turno #42");
        CHECK(std::string(e.what()).find("42") != std::string::npos);
    } catch (...) {
    }
    CHECK(capturada);
}

TEST_CASE("ExcepcionSistema se captura como std::runtime_error", excepciones_como_runtime_error) {
    bool comoRuntimeError = false;
    try {
        throw ExcepcionSistema("prueba");
    } catch (const std::runtime_error& e) {
        comoRuntimeError = true;
        CHECK(std::string(e.what()) == "prueba");
    } catch (...) {
    }
    CHECK(comoRuntimeError);
}

TEST_CASE("ExcepcionSistema NO se captura como std::logic_error", excepciones_no_logic_error) {
    bool comoLogicError = false;
    bool comoRuntimeError = false;
    try {
        throw ExcepcionSistema("prueba");
    } catch (const std::logic_error&) {
        comoLogicError = true;
    } catch (const std::runtime_error&) {
        comoRuntimeError = true;
    } catch (...) {
    }
    CHECK(comoRuntimeError);
    CHECK(!comoLogicError);
}
