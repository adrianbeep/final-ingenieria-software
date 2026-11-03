# ---------------------------------------------------------------------------
# Makefile de FALLBACK: compila con g++ directo, sin CMake.
# Es el camino que va a usar el profesor en una maquina sin CMake.
#
# En Windows con MinGW el comando es `mingw32-make` (no existe `make`).
# ---------------------------------------------------------------------------

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual

# UNICODE/_UNICODE/NOMINMAX NO son opcionales en Windows: ftxui.hpp tiene dos
# `#error` duros sin ellas. En Linux son inofensivas (FTXUI solo las exige bajo
# _WIN32), asi que el Makefile las define SIEMPRE: el mismo Makefile tiene que
# funcionar en las dos plataformas.
DEFS := -DUNICODE -D_UNICODE -DNOMINMAX

# El include del vendor va como `-isystem`: los headers de terceros no tienen
# que ensuciar el build de nuestro codigo con los flags estrictos del proyecto.
INCLUDES := -Iinclude -isystem third_party/ftxui

# El vendor se compila APARTE, con `-w`. Si se compilara en el mismo comando
# que nuestro codigo, el `-w` taparia NUESTROS warnings y el requisito de
# "cero warnings" quedaria sin verificar.
VENDOR_CXXFLAGS := -std=c++17 -w $(DEFS)
VENDOR_INCLUDES := -Ithird_party/ftxui

# -lpthread es obligatorio en Linux (FTXUI usa std::thread). En MinGW es
# opcional, pero el Makefile tiene que funcionar en las dos plataformas.
LDFLAGS := -lpthread

BUILD_DIR := bin

# OJO: `VENDOR_OBJ` se define ACA y no arriba, porque `:=` expande en el
# momento de la asignacion: si se declarara antes de `BUILD_DIR`, el
# prefijo quedaria vacio y el objeto se llamaria `/ftxui.o`.
VENDOR_OBJ := $(BUILD_DIR)/ftxui.o

# Por modulo, con wildcard: agregar un .cpp dentro de un modulo NO obliga a
# tocar este archivo.
CORE_SRCS := $(wildcard src/domain/*.cpp) $(wildcard src/utils/*.cpp) $(wildcard src/exceptions/*.cpp)
# Los globs son PLANOS (no recursivos), asi que `src/ui/Pantallas/` va
# aparte: sin esta linea, las pantallas no se compilarian.
UI_SRCS   := $(wildcard src/ui/*.cpp) $(wildcard src/ui/Pantallas/*.cpp)
ENTITIES_SRCS := $(wildcard src/entities/*.cpp)
REPOSITORIES_SRCS := $(wildcard src/repositories/*.cpp)
SERVICES_SRCS := $(wildcard src/services/*.cpp)
MAIN_SRCS := $(wildcard src/*.cpp)
TEST_SRCS := $(wildcard tests/*.cpp)

ALL_SRCS := $(CORE_SRCS) $(UI_SRCS) $(ENTITIES_SRCS) $(MAIN_SRCS)

ifeq ($(OS),Windows_NT)
  EXE := .exe
else
  EXE :=
endif

TARGET      := $(BUILD_DIR)/sistema_turnos$(EXE)
TEST_TARGET := $(BUILD_DIR)/tests$(EXE)

# --- Comandos de limpieza, portables entre Linux y Windows ---
# En Windows, `del` NO acepta rutas con barra hacia adelante: las interpreta
# como switches (del /q bin/app.exe -> "formato del parametro incorrecto").
# Por eso las rutas se convierten a barra invertida antes de borrarse.
ifeq ($(OS),Windows_NT)
  MKDIR    := cmd /c mkdir
  DEL_BINS := cmd /c del /q $(subst /,\,$(TARGET)) $(subst /,\,$(TEST_TARGET)) $(subst /,\,$(VENDOR_OBJ)) 2>nul
  DEL_DIR  := cmd /c rmdir /s /q $(subst /,\,$(BUILD_DIR))
else
  MKDIR    := mkdir -p
  DEL_BINS := rm -f $(TARGET) $(TEST_TARGET) $(VENDOR_OBJ)
  DEL_DIR  := rm -rf $(BUILD_DIR)
endif

.PHONY: all run test clean distclean rebuild

all: $(TARGET)

$(BUILD_DIR):
	-$(MKDIR) $(BUILD_DIR)

# El objeto del vendor se compila SOLO, con `-w`: si compartiera comando con
# nuestro codigo, el `-w` taparia nuestros propios warnings.
$(VENDOR_OBJ): third_party/ftxui/ftxui.cpp | $(BUILD_DIR)
	$(CXX) $(VENDOR_CXXFLAGS) $(VENDOR_INCLUDES) -c $< -o $@

$(TARGET): $(ALL_SRCS) $(VENDOR_OBJ) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(DEFS) $(INCLUDES) $(ALL_SRCS) $(VENDOR_OBJ) -o $@ $(LDFLAGS)

run: $(TARGET)
	$(TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

# El binario de tests NO linkea src/main.cpp: habria dos main().
# Linkea UI_SRCS porque los tests de la Fase 1 ejercitan ui::Consola::leerEntero.
# Linkea ENTITIES_SRCS porque los tests de la Fase 2 ejercitan las entidades.
# Linkea REPOSITORIES_SRCS porque los tests de la Fase 5 ejercitan los
# repositorios (el template `RepositorioCRUD` es header-only, pero los tres
# repositorios concretos tienen .cpp).
# Linkea SERVICES_SRCS porque los tests de la Fase 5 ejercitan las reglas de
# negocio del servicio de turnos (que, a diferencia de los demas modulos, si
# tiene .cpp).
# Linkea `$(VENDOR_OBJ)` porque `UI_SRCS` ya usa simbolos de FTXUI (el motor
# y las pantallas), y `$(DEFS)` porque los headers de FTXUI tienen dos
# `#error` duros en Windows sin `UNICODE`/`_UNICODE`/`NOMINMAX`.
$(TEST_TARGET): $(TEST_SRCS) $(CORE_SRCS) $(UI_SRCS) $(ENTITIES_SRCS) $(REPOSITORIES_SRCS) $(SERVICES_SRCS) $(VENDOR_OBJ) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(DEFS) $(INCLUDES) $(TEST_SRCS) $(CORE_SRCS) $(UI_SRCS) $(ENTITIES_SRCS) $(REPOSITORIES_SRCS) $(SERVICES_SRCS) $(VENDOR_OBJ) -o $@ $(LDFLAGS)

# El prefijo `-` hace que make NO falle si el archivo no existe: `del` y
# `rmdir` devuelven error cuando el destino no esta, y un `make clean` sobre
# un arbol ya limpio tiene que terminar en 0 igual.
#
# `clean` borra tambien el objeto del vendor (`$(VENDOR_OBJ)`): si no lo
# borrara, un `make clean && make` podria reusar un `.o` viejo y el criterio
# "make clean && make && make run funciona en una maquina limpia" dejaria de
# cumplirse sin que nadie se entere.
clean:
	-$(DEL_BINS)

distclean: clean
	-$(DEL_DIR)

rebuild: distclean all
