# final-ingenieria-software

Trabajo Practico Final de Ingenieria de Software, Segundo anio de la carrera Analisis de Sistemas.

Sistema de gestion de turnos en C++17.

## Estado del proyecto

El plan completo por fases vive en el [issue #1](https://github.com/adrianbeep/final-ingenieria-software/issues/1).

| Fase | Contenido | Issue | Estado |
|---|---|---|---|
| 0 | Scaffold y build | [#5](https://github.com/adrianbeep/final-ingenieria-software/issues/5) | Completada |
| 1 | Cimientos: excepciones, validacion y fecha | [#2](https://github.com/adrianbeep/final-ingenieria-software/issues/2) | Completada |
| 2 | Entidades, operadores y catalogo de materias | [#3](https://github.com/adrianbeep/final-ingenieria-software/issues/3) | Pendiente |
| 3 | Estructuras de datos propias | [#6](https://github.com/adrianbeep/final-ingenieria-software/issues/6) | Pendiente |
| 4 | Persistencia | [#4](https://github.com/adrianbeep/final-ingenieria-software/issues/4) | Pendiente |

## Requisitos

- Compilador con soporte **C++17** (g++ 8+, clang++ 8+, MSVC 2019+)
- **CMake 3.16+** (build principal)
- **GNU Make** (build de fallback, opcional)

## Build

### Con CMake (recomendado)

```bash
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
./build/bin/sistema_turnos
```

### Con el Makefile (fallback, sin CMake)

```bash
make
make test
make run
make clean
```

> En Windows con MinGW el comando es `mingw32-make` (no existe `make`).

### Verificacion

```bash
ctest --test-dir build --output-on-failure
# 100% tests passed, 0 tests failed out of 1

mingw32-make test
# 38 caso(s), 0 fallo(s)
```

El arnes de tests es propio (`tests/test_framework.h`): macros `TEST_CASE` y
`CHECK`, sin framework externo. Un `CHECK` fallido no corta el caso, acumula
el fallo y sigue, para ver todos los problemas de una vez.

## Estructura

```
include/    headers: domain, entities, structures, persistence,
                       repositories, exceptions, services, utils, ui
src/        implementaciones .cpp, espejo de include/
tests/      arnes de tests propio (sin framework externo)
data/       archivos de datos de ejemplo
docs/       trazabilidad y decisiones de diseno
```

Los TADs genericos (`ArbolBST`, `ListaEnlazada`, `ArchivoTexto`, `RepositorioCRUD`)
son **templates header-only**: viven en `include/` y no tienen `.cpp`.

## Convenciones

- Codigo, comentarios e identificadores en **español** (regla de la catedra).
- Compilacion limpia con `-Wall -Wextra -Wpedantic` (mas `-Wshadow`,
  `-Wnon-virtual-dtor` y `-Woverloaded-virtual`).
- `main.cpp` sin logica de negocio: solo despacho.
