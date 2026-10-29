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
#   repositorios INTERFACE  RepositorioCRUD<T> es template           [Fase 5]
#   servicios   STATIC    src/services/ServicioTurnos.cpp            [Fase 5]
# ---------------------------------------------------------------------------

# --- ui: consola, menu, tema ---
file(GLOB UI_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp")
add_library(ui STATIC ${UI_SRCS})
target_include_directories(ui PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(ui PUBLIC warnings)

# --- Ejecutable principal. Sin logica de negocio (requisito 9 de la consigna). ---
add_executable(sistema_turnos src/main.cpp)
target_include_directories(sistema_turnos PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(sistema_turnos PRIVATE ui warnings)

# ---------------------------------------------------------------------------
# Tests. El objetivo existe desde la Fase 0 para que la Fase 1 solo AGREGUE
# archivos de test, sin tocar el build.
# ---------------------------------------------------------------------------
enable_testing()
file(GLOB TEST_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
add_executable(tests ${TEST_SRCS})
target_include_directories(tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(tests PRIVATE warnings)
add_test(NAME tests COMMAND tests)
