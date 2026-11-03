cmake_minimum_required(VERSION 3.16)
project(sistema_turnos LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Estandar: C++17 estricto. EXTENSIONS OFF a proposito (-std=c++17, no gnu++17).
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Debug CACHE STRING "Tipo de build" FORCE)
endif()

# Los binarios caen en build/bin y build/lib (por eso el .gitignore ignora build/)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# ---------------------------------------------------------------------------
# Warnings: "compila sin warnings" es un REQUISITO de la consigna, no una
# promesa. Estos flags lo convierten en una propiedad del build.
#   -Wnon-virtual-dtor    -> avisa si una base con metodos virtuales no tiene
#                             destructor virtual (el error que exige el issue #3)
#   -Woverloaded-virtual  -> cubre el friend operator<< + fromRegistro
#   -Wshadow              -> variables que se pisan entre si
# ---------------------------------------------------------------------------
add_library(warnings INTERFACE)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(warnings INTERFACE
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wnon-virtual-dtor
    -Woverloaded-virtual
  )
endif()

# ---------------------------------------------------------------------------
# FTXUI: la libreria de TUI, vendorizada en third_party/ftxui.
#
# El codigo de terceros se compila como un target PROPIO, con `-w`:
#   -w         -> los warnings del codigo de terceros NO son nuestros. El
#                 requisito "compila limpio sin warnings" es del codigo del
#                 proyecto, y sigue intacto para el nuestro.
#   SYSTEM     -> el include del vendor se pasa como `-isystem`, asi que los
#                 headers de terceros no ensucian el build de nuestros .cpp
#                 con los flags estrictos del proyecto.
#
# UNICODE/_UNICODE/NOMINMAX son obligatorias en Windows: ftxui.hpp tiene dos
# `#error` duros sin ellas. En Linux son inofensivas (FTXUI solo las exige bajo
# _WIN32), asi que aca se limitan a WIN32.
# ---------------------------------------------------------------------------
find_package(Threads REQUIRED)

add_library(ftxui STATIC third_party/ftxui/ftxui.cpp)
target_include_directories(ftxui SYSTEM PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/third_party/ftxui")
target_compile_options(ftxui PRIVATE -w)
if(WIN32)
  target_compile_definitions(ftxui PUBLIC UNICODE _UNICODE NOMINMAX)
endif()
target_link_libraries(ftxui PUBLIC Threads::Threads)

# ---------------------------------------------------------------------------
# Modulos.
# file(GLOB ... CONFIGURE_DEPENDS) para que agregar una clase NUEVA dentro de
# un modulo NO obligue a tocar este archivo.
#
# Topologia COMPLETA (cada fase agrega su target aca):
#   core        STATIC    src/domain + src/utils + src/exceptions   [Fase 1]
#   ui          STATIC    src/ui                                     [Fase 0/1/6]
#   profesores  STATIC    src/entities/Profesor.cpp                  [Fase 2]
#   alumnos     STATIC    src/entities/Alumno.cpp                    [Fase 2]
#   turnos      STATIC    src/entities/Turno.cpp                     [Fase 2]
#   structures  INTERFACE  templates header-only (ArbolBST, ListaEnlazada) [Fase 3]
#   persistencia INTERFACE  ArchivoTexto<T> es template              [Fase 4]
#   repositorios INTERFACE  IRepositorio<T>, IRepositorioBajaLogica<T> y
#                           RepositorioCRUD<T> son templates           [Fase 5]
#   servicios   STATIC    src/services/ServicioTurnos.cpp            [Fase 5]
# ---------------------------------------------------------------------------

# --- core: dominio + utilidades + excepciones ---
# Es un target STATIC, no INTERFACE: por eso puede usar PUBLIC para propagar
# las opciones de `warnings` (un target INTERFACE solo puede ser DESTINO de
# target_link_libraries, nunca origen).
file(GLOB CORE_SRCS CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/domain/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/utils/*.cpp"
     "${CMAKE_CURRENT_SOURCE_DIR}/src/exceptions/*.cpp")
add_library(core STATIC ${CORE_SRCS})
target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(core PUBLIC warnings)

# --- ui: consola, menu, tema ---
file(GLOB UI_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp")
add_library(ui STATIC ${UI_SRCS})
target_include_directories(ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ui PUBLIC core warnings)

# --- Entidades: profesor, alumno y turno ---
# Son tres targets y no uno solo porque son modulos independientes: cada uno
# depende de `core` (donde estan `Materia`, `Fecha`, `Hora`, `Utils` y las
# excepciones) y ninguno depende de los otros dos.
#
# `Persona` NO tiene target propio: es header-only (ver include/entities/
# Persona.h), asi que su codigo entra en cada uno de los tres.
add_library(profesores STATIC src/entities/Profesor.cpp)
target_include_directories(profesores PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(profesores PUBLIC core warnings)

add_library(alumnos STATIC src/entities/Alumno.cpp)
target_include_directories(alumnos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(alumnos PUBLIC core warnings)

add_library(turnos STATIC src/entities/Turno.cpp)
target_include_directories(turnos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(turnos PUBLIC core warnings)

# --- structures: templates header-only (ArbolBST, ListaEnlazada) ---
# Es un target INTERFACE porque los dos TAD son TEMPLATES: un template no se
# puede compilar sin conocer `T`, asi que viven en headers y no hay .cpp que
# compilar.
#
# OJO: un target INTERFACE solo puede ser DESTINO de target_link_libraries,
# nunca origen. Por eso aca no hay `target_link_libraries(structures ...)`.
add_library(structures INTERFACE)
target_include_directories(structures INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")

# --- persistencia: ArchivoTexto<T>, template header-only ---
# Mismo caso que `structures`: `ArchivoTexto<T>` es un TEMPLATE, asi que vive
# entero en include/persistence/ArchivoTexto.h y no hay .cpp que compilar.
#
# A diferencia de `structures`, este SI propaga una dependencia: usa
# `Utils::separar` y `DatoInvalidoException`, que estan en `core`. Se declara
# con la palabra clave INTERFACE, que es la unica valida para un target
# INTERFACE: lo que se lista aca lo reciben TODOS los que enlacen con
# `persistencia` (incluidos sus directorios de include y la libreria `core`).
add_library(persistencia INTERFACE)
target_include_directories(persistencia INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(persistencia INTERFACE core)

# --- repositorios: las interfaces y el template CRUD ---
# `IRepositorio<T>`, `IRepositorioBajaLogica<T>` y `RepositorioCRUD<T>` son
# TEMPLATES: sin conocer `T` no se pueden compilar, asi que viven enteros en
# include/repositories/ y no hay .cpp que compilar. Es el mismo caso que
# `structures` y `persistencia`, asi que el target es INTERFACE.
#
# Este target es SOLO la parte header-only. Los tres repositorios concretos
# (alumnos, profesores y turnos) SI tienen .cpp, asi que van en un target
# STATIC aparte, `repositorios_concretos`, que es el que enlazan los tests y
# el ejecutable.
#
# A diferencia de `structures`, este SI propaga dependencias: el template
# CRUD usa `ArbolBST` (structures) y `ArchivoTexto` (persistencia), y las
# excepciones del dominio (core).
add_library(repositorios INTERFACE)
target_include_directories(repositorios INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(repositorios INTERFACE core structures persistencia warnings)

# --- repositorios concretos: alumnos, profesores y turnos ---
# Es un target STATIC, no INTERFACE, porque los tres repositorios tienen
# metodos que NO son de template (`bajaLogica`, `reactivar`): hay codigo que
# compilar y que empaquetar en una libreria.
#
# Los tres son modulos del mismo tipo (un repositorio por entidad), asi que
# van juntos en UNA sola libreria: comparten la implementacion de la baja
# logica (`BajaLogicaCompartida`), que es la misma para alumnos y profesores.
file(GLOB REPOSITORIOS_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/repositories/*.cpp")
add_library(repositorios_concretos STATIC ${REPOSITORIOS_SRCS})
target_include_directories(repositorios_concretos PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(repositorios_concretos PUBLIC core structures persistencia repositorios warnings)

# --- servicios: las reglas de negocio ---
# A diferencia de los modulos anteriores, este target es STATIC y NO
# INTERFACE: `ServicioTurnos` no es un template, asi que hay codigo que
# compilar y que empaquetar en una libreria.
#
# Enlaza con `core` (de ahi vienen `Fecha`, `Hora`, `Utils` y las
# excepciones), con `repositorios` (las interfaces `IRepositorio<T>`, que es
# contra lo que el servicio escribe) y con las tres entidades, porque el
# servicio construye turnos y consulta alumnos y profesores.
file(GLOB SERVICES_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/services/*.cpp")
add_library(servicios STATIC ${SERVICES_SRCS})
target_include_directories(servicios PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(servicios PUBLIC core alumnos profesores turnos repositorios warnings)

# --- Ejecutable principal. Sin logica de negocio (requisito 9 de la consigna). ---
add_executable(sistema_turnos src/main.cpp)
target_include_directories(sistema_turnos PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(sistema_turnos PRIVATE ui warnings ftxui Threads::Threads)

# ---------------------------------------------------------------------------
# Tests. El objetivo existe desde la Fase 0 para que la Fase 1 solo AGREGUE
# archivos de test, sin tocar el build.
# ---------------------------------------------------------------------------
enable_testing()
file(GLOB TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(tests ${TEST_SRCS})
target_include_directories(tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(tests PRIVATE core ui profesores alumnos turnos structures persistencia repositorios repositorios_concretos servicios warnings)
add_test(NAME tests COMMAND tests)
