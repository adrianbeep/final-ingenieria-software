# final-ingenieria-software

Trabajo Practico Final de Ingenieria de Software, Segundo anio de la carrera Analisis de Sistemas.

**Sistema de turnos medicos** en C++17, con interfaz de consola TUI.

## Estado del proyecto

El plan completo por fases vive en el [issue #1](https://github.com/adrianbeep/final-ingenieria-software/issues/1).

| Fase | Contenido | Issue | Estado |
|---|---|---|---|
| 0 | Scaffold y build | [#5](https://github.com/adrianbeep/final-ingenieria-software/issues/5) | Completada |
| 1 | Cimientos: excepciones, validacion y fecha | [#2](https://github.com/adrianbeep/final-ingenieria-software/issues/2) | Completada |
| 2 | Entidades, operadores y catalogo de especialidades | [#3](https://github.com/adrianbeep/final-ingenieria-software/issues/3) | Completada |
| 3 | Estructuras de datos propias | [#6](https://github.com/adrianbeep/final-ingenieria-software/issues/6) | Completada |
| 4 | Persistencia | [#4](https://github.com/adrianbeep/final-ingenieria-software/issues/4) | Completada |
| 5 | Repositorios y reglas de negocio | [#14](https://github.com/adrianbeep/final-ingenieria-software/issues/14) | Completada |
| 6 | Interfaz de consola TUI: 38 pantallas | [#16](https://github.com/adrianbeep/final-ingenieria-software/issues/16) | En curso |
| 7 | Extras y cierre tecnico | — | Pendiente |
| 8 | Documentacion y defensa | — | Pendiente |

### El dominio

`Medico`, `Paciente` y `Turno`, con `Especialidad` como `enum class` de doce
valores.

El modelo es el de un consultorio: un medico tiene **una** especialidad, da
turnos a pacientes, y **no puede atender a dos pacientes a la vez**. El turno
tiene que ser **de la especialidad del medico que lo atiende**: esa regla es
la que hace que la especialidad deje de ser un dato decorativo.

La duracion de un turno es un multiplo de 15 minutos (hasta 240), el horario de
atencion va de 08:00 a 22:00 con granularidad de 30 minutos, y el consultorio
se identifica con un codigo de hasta 20 caracteres.

> Las decisiones de diseno, con su justificacion, estan en
> [docs/decisiones.md](docs/decisiones.md).

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
# 387 caso(s), 0 fallo(s)
```

El arnes de tests es propio (`tests/test_framework.h`): macros `TEST_CASE` y
`CHECK`, sin framework externo. Un `CHECK` fallido no corta el caso, acumula
el fallo y sigue, para ver todos los problemas de una vez.

## Estructura

```
include/    headers: domain, entities, structures, persistence,
                       repositories, exceptions, services, utils, ui
  ui/Pantallas/  las pantallas: MenuPrincipal, Pacientes, Medicos
src/        implementaciones .cpp, espejo de include/
  domain/         el catalogo de especialidades
  entities/       las entidades: Medico, Paciente, Persona y Turno
  repositories/   un repositorio por entidad
  services/       las reglas de negocio de los turnos
  ui/             la TUI: el motor, el menu, la tabla, el campo de entrada
  ui/Pantallas/   las pantallas: MenuPrincipal, Pacientes, Medicos
tests/      arnes de tests propio (sin framework externo)
data/       datos de trabajo: los archivos que usa la aplicacion
data/ejemplo/  la semilla: los datos de ejemplo, para empezar de cero
third_party/  FTXUI, vendorizada (ver la decision 18)
docs/       trazabilidad y decisiones de diseno
```

Los TADs genericos (`ArbolBST`, `ListaEnlazada`, `ArchivoTexto`, `RepositorioCRUD`)
son **templates header-only**: viven en `include/` y no tienen `.cpp`.

### Por que el arbol, y por que la busqueda es O(log n)

`ArbolBST<T>` es el backing de las tres colecciones, y la razon es una sola: las
busquedas son por clave y el conjunto crece. La busqueda por id es **O(log n)**, y
eso esta garantizado por construccion:

- El repositorio busca con `ArbolBST<T>::buscarPorClave()`, un **descenso** por
  el arbol que compara la clave contra el valor almacenado. No recorre el arbol.
- El arbol del repositorio esta **balanceado siempre**: se reconstruye con
  `construirDesdeOrdenados()` despues de cada mutacion, no solo al cargar.
  (`insertar()` NO balancea: es una decision de diseno del TAD.)

Medido con 1000 entidades: **altura 10 y 9 comparaciones** para encontrar un id
y 9 para un id inexistente. Los tests fijan la complejidad **contando
comparaciones**, no midiendo el reloj.

### La interfaz: una TUI de consola

La interfaz es una **TUI de consola** sobre [FTXUI](https://github.com/ArthurSonzonni/FTXUI),
vendorizada en `third_party/ftxui/`:

- **Teclado primero.** Las flechas mueven, `Enter` elige, `Esc` y `0` vuelven.
  El mouse es un plus: funciona en los terminales que lo soportan (Windows
  Terminal, VS Code, Linux) y su ausencia no rompe nada.
- **Sin terminal, no hay TUI.** El binario detecta que no hay terminal
  interactiva, imprime el menu en texto plano y sale con codigo 0. Es lo que
  permite que el CI lo ejecute sin colgarse.
- **`NO_COLOR` respetado:** sin ese variable, la salida no tiene codigos de
  color.

### Los archivos de datos de ejemplo

`data/pacientes.dat`, `data/medicos.dat` y `data/turnos.dat` estan escritos con
**LF** (no CRLF) y **UTF-8 sin BOM**. La capa de persistencia escribe en modo
**binario**, asi que los bytes que produce son **identicos en cualquier sistema
operativo**: el archivo generado en Windows es byte a byte el mismo que el
generado en Linux. Por eso `.gitattributes` los marca `-text` y se guardan tal
cual, sin conversion de fin de linea.

En `data/` estan los archivos de **trabajo**: los que la aplicacion abre y
modifica. En `data/ejemplo/` esta la **semilla**: el mismo juego de datos, en
la version 2 del formato y sin tocar. La idea es que la pantalla de "cargar
datos de ejemplo" copie la semilla sobre `data/` para empezar de cero (esa
pantalla todavia no existe: es la Fase 6 / PR15).

La semilla tiene las MISMAS garantias de bytes: **LF**, **UTF-8 sin BOM** y
`-text` en `.gitattributes`. Por eso la regla es explicita para los dos
directorios (`data/*.dat` y `data/ejemplo/*.dat`): en `.gitattributes` un
patron con barra esta anclado al directorio del archivo y el `*` no cruza la
barra, asi que `data/*.dat` NO alcanza a `data/ejemplo/*.dat`.

## Convenciones

- Codigo, comentarios e identificadores en **español** (regla de la catedra).
- Compilacion limpia con `-Wall -Wextra -Wpedantic` (mas `-Wshadow`,
  `-Wnon-virtual-dtor` y `-Woverloaded-virtual`).
- `main.cpp` sin logica de negocio: solo despacho.
- La capa de dominio **no** depende de la interfaz: lo verifica el CI.
