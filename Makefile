# ---------------------------------------------------------------------------
# Makefile de FALLBACK: compila con g++ directo, sin CMake.
# Es el camino que va a usar el profesor en una maquina sin CMake.
#
# En Windows con MinGW el comando es `mingw32-make` (no existe `make`).
# ---------------------------------------------------------------------------

CXX      := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Woverloaded-virtual
INCLUDES := -Iinclude
LDFLAGS  :=

BUILD_DIR := bin

# Por modulo, con wildcard: agregar un .cpp dentro de un modulo NO obliga a
# tocar este archivo.
CORE_SRCS := $(wildcard src/domain/*.cpp) $(wildcard src/utils/*.cpp) $(wildcard src/exceptions/*.cpp)
UI_SRCS   := $(wildcard src/ui/*.cpp)
MAIN_SRCS := $(wildcard src/*.cpp)
TEST_SRCS := $(wildcard tests/*.cpp)

ALL_SRCS := $(CORE_SRCS) $(UI_SRCS) $(MAIN_SRCS)

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
  DEL_BINS := cmd /c del /q $(subst /,\,$(TARGET)) $(subst /,\,$(TEST_TARGET)) 2>nul
  DEL_DIR  := cmd /c rmdir /s /q $(subst /,\,$(BUILD_DIR))
else
  MKDIR    := mkdir -p
  DEL_BINS := rm -f $(TARGET) $(TEST_TARGET)
  DEL_DIR  := rm -rf $(BUILD_DIR)
endif

.PHONY: all run test clean distclean rebuild

all: $(TARGET)

$(BUILD_DIR):
	-$(MKDIR) $(BUILD_DIR)

$(TARGET): $(ALL_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(ALL_SRCS) -o $@ $(LDFLAGS)

run: $(TARGET)
	$(TARGET)

test: $(TEST_TARGET)
	$(TEST_TARGET)

# El binario de tests NO linkea src/main.cpp: habria dos main().
$(TEST_TARGET): $(TEST_SRCS) $(CORE_SRCS) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $(INCLUDES) $(TEST_SRCS) $(CORE_SRCS) -o $@ $(LDFLAGS)

clean:
	$(DEL_BINS)

distclean: clean
	$(DEL_DIR)

rebuild: distclean all
