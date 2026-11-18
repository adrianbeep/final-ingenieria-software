# Trazabilidad de los requisitos obligatorios

Este documento mapea **los 9 requisitos obligatorios de la consigna** al
**archivo y la linea** donde cada uno esta resuelto, y a la forma de
verificarlo.

Es el entregable que la consigna pide ("un README de una pagina con el
diagrama de clases y la descripcion de las decisiones de diseno") y el ultimo
punto pendiente de la Definicion de Done del
[roadmap](https://github.com/adrianbeep/final-ingenieria-software/issues/1).

Ademas es el **mapa de la defensa**: si cada requisito apunta a una linea
real, las ocho preguntas que seguro te hacen se responden con este mismo
documento.

## Como se lee

- El texto de cada requisito esta **copiado textual de la consigna**, para
  poder compararlo con el enunciado.
- Las lineas se **verificaron contra el codigo** del commit `9503ecd`
  (`main`, con las 39 pantallas). Si una linea queda desactualizada, el
  documento esta mal: se corrige el documento, no el codigo.
- Cada fila dice **como se verifica** el requisito: el test que lo cubre, o
  la comprobacion manual cuando no hay uno (y entonces se dice por que).
- La columna **Decision** apunta a la fila de
  [`docs/decisiones.md`](decisiones.md) que justifica el porque. El numero es
  el de esa tabla.

## Los nueve, de un vistazo

| # | Requisito | Donde esta resuelto | Como se verifica |
|---|---|---|---|
| 1 | POO: dos entidades, la clase del turno, encapsulamiento, ctor y dtor, sobrecarga de operador y una funcion friend justificada | `include/entities/` | `tests/test_entidades.cpp` |
| 2 | Persistencia en archivos: se guardan y se recuperan al iniciar | `include/persistence/ArchivoTexto.h` | `tests/test_persistencia.cpp` |
| 3 | Menu con ABML/CRUD completo para las dos entidades y para los turnos, con baja logica | `include/ui/CrudPersonas.h`, `src/ui/Pantallas/Turnos.cpp` | `tests/test_ui.cpp` |
| 4 | Reglas de negocio de los turnos: no duplicar, verificar que existan, cancelar y reprogramar | `src/services/ServicioTurnos.cpp` | `tests/test_servicio.cpp` |
| 5 | Templates: al menos una clase o funcion template reutilizada | `include/structures/`, `include/persistence/`, `include/repositories/`, `include/ui/` | los tests de cada TAD |
| 6 | Excepciones: try/catch y al menos una clase propia | `include/exceptions/ExcepcionesSistema.h` | `tests/test_excepciones.cpp` |
| 7 | Estructura de datos: al menos una, vista en clase, implementada por el alumno | `include/structures/ArbolBST.h`, `include/structures/ListaEnlazada.h` | `tests/test_arbol.cpp`, `tests/test_lista.cpp` |
| 8 | Validacion de entradas: el programa no se rompe con datos invalidos | los setters de las entidades | `tests/test_entidades.cpp`, `tests/test_fecha.cpp` |
| 9 | Organizacion y comentarios: `.h`/`.cpp` y comentarios que expliquen las decisiones | todo el repo | el CI + la lectura |

---

## 1. POO

> "POO: dos entidades principales más una clase para el turno (la relación), con
> encapsulamiento, constructores y destructor, al menos una sobrecarga de
> operador y al menos una función friend justificada."

| Que | Donde esta resuelto | Como se verifica | Decision |
|---|---|---|---|
| Las dos entidades principales y la clase de la relacion | `include/entities/Medico.h:40`, `include/entities/Paciente.h:37`, `include/entities/Turno.h:43`, con la base comun `include/entities/Persona.h:36` | `tests/test_entidades.cpp` | 1 |
| Encapsulamiento | los datos son privados y los setters validan y lanzan `DatoInvalidoException`: no se puede construir una entidad invalida | `tests/test_entidades.cpp` | 4 |
| Constructores | `Medico.h:53`, `Paciente.h:52`, `Turno.h:55` (validan **todos** los datos a traves de los setters) | `tests/test_entidades.cpp` | 1 |
| Destructores | `Persona.h:53` (virtual, es una base polimorfica), `Medico.h:48`, `Paciente.h:45`, `Turno.h:51` | compila con `-Wnon-virtual-dtor` (uno de los seis flags del proyecto) | 1 |
| Sobrecarga de operador | `operator<<` (friend) en `Medico.h:72`, `Paciente.h:71`, `Turno.h:102`; `operator==` y `operator<` en `Persona.h:117` y `Persona.h:123`, y en `Turno.h:98` y `Turno.h:99` | `tests/test_entidades.cpp`, `tests/test_formato.cpp` | 1 |
| La funcion friend, **justificada** | `Medico.h:72`; el porque esta en `Medico.h:33-38` (las dos alternativas descartadas: exponer getters publicos, o hacer `operator<<` miembro y perder el ADL). Los mismos argumentos en `Paciente.h:33` y `Turno.h:39` | la lectura del codigo | 1 |

**Nota sobre `operator==`:** compara el id **y** el tipo, porque el id es unico
*por entidad* y no global. Sin eso, el `ArbolBST` no tendria el *strict weak
ordering* que necesita (`Persona.h:112`).

---

## 2. Persistencia en archivos

> "Persistencia en archivos: los datos de ambas entidades y de los turnos se
> guardan en archivos y se recuperan al iniciar el programa."

| Que | Donde esta resuelto | Como se verifica | Decision |
|---|---|---|---|
| La capa de archivos | `include/persistence/ArchivoTexto.h:256` (`ArchivoTexto<T>`) | `tests/test_persistencia.cpp` | 11 |
| Guardar | `ArchivoTexto.h:290` (la secuencia), `:291` (la lista enlazada), `:302` (initializer_list) | `tests/test_persistencia.cpp` | 45 |
| Cargar | `ArchivoTexto.h:312` (la secuencia), `:313` (la lista enlazada) | `tests/test_persistencia.cpp` | 45 |
| El formato, versionado | `ArchivoTexto.h:272` (`VERSION_FORMATO = 3`) y `:279` (`VERSION_ANTERIOR = 2`), mas el header y la linea de esquema de `data/*.dat` | `tests/test_persistencia.cpp`: un archivo de la version anterior SE LEE (con el campo nuevo vacio) y uno de una version que no se entiende se rechaza con un error claro | 11, 3 |
| Se recupera al iniciar | `src/main.cpp:33-35` (los tres `cargar()`) | el arranque de la app, y el CI lo corre en cada build | 13 |
| Los bytes son iguales en cualquier sistema operativo | la escritura en modo **binario** de `ArchivoTexto.h` (por eso `.gitattributes` los marca `-text`) | `tests/test_persistencia.cpp` | 13 |

**Por que texto y no binario:** con `fwrite` de un `std::string` se mide el
**objeto** (puntero, tamanho y buffer), no los caracteres: el resultado es
basura. Ademas, agregar un campo invalidaria todos los archivos anteriores.

---

## 3. Menu con ABML/CRUD completo

> "Menú con ABML/CRUD completo para las dos entidades y para los turnos (alta,
> baja, modificación, listado y consulta/búsqueda). Debe incluir baja lógica en
> al menos una entidad."

| Que | Donde esta resuelto | Como se verifica | Decision |
|---|---|---|---|
| Las cinco acciones de las entidades | `include/ui/CrudPersonas.h:438` (alta), `:439` (modificacion), `:440` (baja / reactivar), `:441` (listado), `:442` (busqueda) | `tests/test_ui.cpp` | 5, 39 |
| Las de los turnos | `src/ui/Pantallas/Turnos.cpp:494` (agendar), `:495` (cancelar), `:496` (reprogramar), `:497` (modificar), `:498` (listado), `:499` (buscar), `:500` (ver detalle) | `tests/test_ui.cpp` | 41, 43 |
| La **baja logica** | el flag `activo` (`include/entities/Persona.h:261`) con su setter (`:101`), y la implementacion compartida en `include/repositories/BajaLogicaCompartida.h:38` (`darDeBaja` en `:51`, `reactivar` en `:59`) | `tests/test_repositorio.cpp` | 10 |
| El registro **sigue existiendo** | `include/persistence/ArchivoTexto.h:135` ("No filtra inactivos": el `activo` es un DATO, no una condicion de carga) | `tests/test_persistencia.cpp` | 10 |
| La baja logica de los turnos | `EstadoTurno` (`include/entities/Turno.h:12`) y `ServicioTurnos::cancelar` (`src/services/ServicioTurnos.cpp:121`), que cambia el estado y **no borra** el registro | `tests/test_servicio.cpp` | 10 |

**Ojo:** hay una baja logica en las **tres** entidades, no solo en una: el
requisito pide "al menos una".

---

## 4. Reglas de negocio de los turnos

> "Asignación de turnos con reglas de negocio: no permitir duplicar un turno
> para la misma entidad en la misma fecha/hora, verificar que las entidades
> existan, permitir cancelar y reprogramar."

| Que | Donde esta resuelto | Como se verifica | Decision |
|---|---|---|---|
| Agendar | `src/services/ServicioTurnos.cpp:109` | `tests/test_servicio.cpp` | 15 |
| El paciente existe y esta activo | `ServicioTurnos.cpp:382` (REGLA 1) | `tests/test_servicio.cpp` | 15 |
| El medico existe y esta activo | `ServicioTurnos.cpp:392` (REGLA 2) | `tests/test_servicio.cpp` | 15 |
| El turno es de la especialidad del medico | `ServicioTurnos.cpp:402` (REGLA 2b) | `tests/test_servicio.cpp` | 15 |
| La fecha existe y no esta en el pasado | `ServicioTurnos.cpp:413` (REGLA 3) | `tests/test_servicio.cpp` | 15 |
| La hora, la duracion y el resto de los datos son validos | `ServicioTurnos.cpp:422` (REGLA 4) | `tests/test_servicio.cpp` | 16 |
| El turno no se pisa con ningun turno vigente | `ServicioTurnos.cpp:425` (REGLA 5) y `verificarSolapamiento` (`:480`) | `tests/test_servicio.cpp` | 15 |
| **Como** se detecta el solapamiento | `seSolapan` (`ServicioTurnos.cpp:87`): se comparan **intervalos de minutos**, no la igualdad de fecha y hora (asi 10:00-10:45 y 10:30-11:15 se pisan, y dos turnos que se tocan justo a las 10:45 no) | `tests/test_servicio.cpp` | 15 |
| Cancelar | `ServicioTurnos.cpp:121` (exige un turno `Activo`: `Cancelado` y `Reprogramado` son finales) | `tests/test_servicio.cpp` | 10 |
| Reprogramar | `ServicioTurnos.cpp:149` (crea el turno nuevo con `proximoId()` y deja el viejo `Reprogramado` apuntando al nuevo) | `tests/test_servicio.cpp` | 8 |
| Modificar | `ServicioTurnos.cpp:328` (excluye al propio turno del control de solapamiento) | `tests/test_servicio.cpp` | 32 |

**Las reglas viven en el servicio, no en la pantalla.** La interfaz solo
muestra el mensaje de la excepcion (`src/ui/Pantallas/Turnos.cpp`): si la
regla estuviera en la pantalla, se podria saltar.

---

## 5. Templates

> "Templates: al menos una clase o función template reutilizada en el
> proyecto (ejemplo: manejador genérico de archivos o contenedor propio)."

Hay **cinco**, y todos se usan:

| Template | Donde esta resuelto | Que reutiliza |
|---|---|---|
| `ArbolBST<T>` | `include/structures/ArbolBST.h:56` | la coleccion ordenada por clave |
| `ListaEnlazada<T>` | `include/structures/ListaEnlazada.h:51` | la estructura de la capa de persistencia |
| `ArchivoTexto<T>` | `include/persistence/ArchivoTexto.h:256` | la lectura y la escritura de los archivos |
| `RepositorioCRUD<T>` | `include/repositories/RepositorioCRUD.h:95` | el CRUD de las tres entidades |
| `CrudPersonas<T>` | `include/ui/CrudPersonas.h:146` | el motor de las pantallas de personas |

Los cuatro ultimos son **header-only** (sin conocer `T` no se puede compilar, no
hay `.cpp` que compilar) ✓.

---

## 6. Excepciones

> "Excepciones: manejo de errores con try/catch y al menos una clase de
> excepción propia (ejemplo: archivo inexistente, ID duplicado, dato inválido,
> turno ocupado)."

| Que | Donde esta resuelto | Como se verifica | Decision |
|---|---|---|---|
| La clase propia | `include/exceptions/ExcepcionesSistema.h:24` (`ExcepcionSistema`, de `std::runtime_error`) y sus siete derivadas: `:30` (archivo no encontrado), `:36` (id duplicado), `:44` (dato invalido), `:65` (la entidad no existe), `:71` (turno ocupado), `:77` (el turno no existe), `:92` (formato desconocido) | `tests/test_excepciones.cpp` | 1 |
| Los `catch` | 22 en `src/`; el global en `src/main.cpp:53` | `tests/test_excepciones.cpp` | 27 |
| La red de seguridad | `include/persistence/ArchivoTexto.h:740`: cualquier excepcion cruda se traduce a una del proyecto, con el archivo y la linea | `tests/test_persistencia.cpp` | 11 |

**Por que alcanza un solo `catch` global:** todas las excepciones del
proyecto cuelgan de `ExcepcionSistema`, asi que el `catch` de `main.cpp:53`
las cubre a todas. Y cada excepcion lleva el **campo** que fallo y el
**valor**, para que la pantalla pueda decir *"Dato invalido en campo 'email'"*
sin reinterpretar el mensaje.

---

## 7. Estructura de datos

> "Estructura de datos: uso justificado de al menos una estructura vista en
> clase (lista, pila, cola o árbol), implementada por el alumno."

| Que | Donde esta resuelto | Como se verifica | Decision |
|---|---|---|---|
| El **arbol binario de busqueda**, que es el backing real de las tres colecciones | `include/structures/ArbolBST.h:56`, y el miembro `_arbol` en `include/repositories/RepositorioCRUD.h:193` | `tests/test_arbol.cpp`, `tests/test_repositorio.cpp` | 46 |
| La **lista enlazada**, que es la estructura de la capa de persistencia | `include/structures/ListaEnlazada.h:51`, usada por `include/persistence/ArchivoTexto.h:291` y `:313` | `tests/test_lista.cpp`, `tests/test_persistencia.cpp` | 45 |
| La justificacion | la seccion "Decisiones de los TADs y de la capa de persistencia" de `docs/decisiones.md` (filas 45 y 46) | la lectura | 45, 46 |

**Por que un arbol y no un vector o una lista:** las busquedas son por clave y
el conjunto crece, asi que la busqueda por id es **O(log n) garantizada por
construccion**: el arbol del repositorio esta balanceado siempre (se
reconstruye con `construirDesdeOrdenados()` despues de cada mutacion). Los
tests **fijan la complejidad contando comparaciones**, no midiendo el reloj.

**Por que la lista no ordena y no tiene `operator[]`:** la lista recibe la
secuencia **ya ordenada** del arbol (no necesita `operator<`), y el indice
aleatorio seria O(n), peor que un vector: el puente es `aVector()`.

---

## 8. Validacion de entradas

> "Validación de entradas: el programa no debe romperse ante datos inválidos
> (letras en lugar de números, fechas imposibles, campos vacíos)."

| Que | Donde esta resuelto | Como se verifica | Decision |
|---|---|---|---|
| Los validadores del dominio | `include/entities/Persona.h:171` (nombre), `:181` (email), `:201` (telefono); `src/entities/Paciente.cpp:53` (dni); `src/entities/Medico.cpp:54` (legajo); `src/entities/Turno.cpp:112` (duracion) y `:122` (consultorio) | `tests/test_entidades.cpp`, `tests/test_turno.cpp` | 16 |
| La fecha y la hora | `include/utils/Fecha.h:39` (la fecha existe en el calendario, con bisiestos) y `:128` (la hora esta en el horario de atencion) | `tests/test_fecha.cpp` | 15 |
| Los helpers | `include/utils/Utils.h:31` (`esEntero`), `:39` (`esAlfabetico`), `:96` (`esFormatoFecha`), `:101` (`esFormatoHora`) | `tests/test_utils.cpp` | 4 |
| La pantalla **no reimplementa** la regla | `src/ui/Pantallas/Pacientes.cpp:64`: el validador del campo **envuelve al setter** de la entidad y devuelve el motivo | `tests/test_ui.cpp` | 27 |
| La red | `src/main.cpp:53` y `include/persistence/ArchivoTexto.h:740` | `tests/test_persistencia.cpp` | 11 |

**El caso del enunciado:** `31/02`, `29/02/2025` y `"abc"` no rompen el
programa: el primer caso es una fecha que no existe, el segundo un anio que no
es bisiesto, y el tercero un numero que no es un numero. Los tres los rechaza
`Fecha::esValida` y `Utils::esEntero` **antes** de tocar nada ✓.

---

## 9. Organizacion y comentarios

> "Organización y comentarios: código dividido en archivos .h/.cpp, con
> comentarios que expliquen las decisiones principales."

| Que | Donde esta resuelto | Como se verifica | Decision |
|---|---|---|---|
| La division `.h`/`.cpp` | `include/` espeja `src/`: misma carpeta, mismo nombre | la estructura del repo | 20 |
| Los comentarios explican **el por que** | por ejemplo `include/structures/ArbolBST.h:15` (por que punteros crudos y no `unique_ptr`), `src/services/ServicioTurnos.cpp:452` (por que el turno tiene que terminar antes del cierre) y `include/entities/Medico.h:33` (por que `friend`) | la lectura del codigo | 46 |
| Que el CI lo verifica | `.github/workflows/ci.yml:46` (el dominio no toca I/O) y `:57` (el dominio no depende de FTXUI) | el CI corre en cada push y en cada PR | 20 |

**El criterio de los comentarios:** explican **por que** se hizo asi, con la
alternativa descartada. No repiten lo que hace el codigo.

---

## Lo que NO pide la consigna y esta hecho

El sistema tiene decisiones que van mas alla de los nueve requisitos, y estan
documentadas en [`docs/decisiones.md`](decisiones.md):

| Decision | Que aporta |
|---|---|
| 30 | La especialidad se **elige de una lista** de doce: el estado invalido es irrepresentable |
| 36, 37 | El color es un estado **compartido** y `NO_COLOR` es el estado inicial, no un veto |
| 38 | Cargar los datos de ejemplo es **un solo helper** |
| 39, 40, 41, 42 | La entidad se elige con un **buscador**, no escribiendo su id: **la app no le pide un id de base de datos a nadie** |
| 43, 44 | El flujo de agendar es **paciente -> especialidad -> medico -> formulario**, con los medicos filtrados por la especialidad |
| 45, 46 | La lista enlazada es el TAD propio del requisito 7 y es la estructura de la persistencia |
| 54 | El formato de archivo es **versionado** y el loader **entiende la version anterior**: agregar el campo opcional `observaciones` (la v3) no invalida los archivos v2 |

## Las ocho preguntas de la defensa

Las ocho estan contestadas, con su respuesta, su alternativa descartada y
su evidencia, en [`docs/defensa.md`](defensa.md). Este documento es la
evidencia cruda de seis de las ocho:

| Pregunta | Donde esta la evidencia |
|---|---|
| 1. ¿Por que separaste las capas? | `docs/decisiones.md` (20, 26, 27) y el arbol de `include/` |
| 2. ¿Por que un arbol y no un vector o una lista? | `include/structures/ArbolBST.h:15` (el porque) y el requisito 7 de este documento |
| 3. ¿Por que `friend` y no un getter? | `include/entities/Medico.h:33` y el requisito 1 de este documento |
| 4. ¿Como garantizas que no se dupliquen turnos? | `src/services/ServicioTurnos.cpp:480` y `:87` (el solapamiento por intervalo) |
| 5. ¿Por que `enum class` y no `string`? | `docs/decisiones.md` (1, 2, 3) |
| 6. ¿Que pasa si el programa muere escribiendo el archivo? | `include/persistence/ArchivoTexto.h` (la escritura atomica con `.tmp` y `rename`) y `src/main.cpp:53` |
| 7. ¿Por que un solo `catch` alcanza? | `include/exceptions/ExcepcionesSistema.h:24` y el requisito 6 de este documento |
| 8. ¿Como es el arbol? Mostralo en el papel | `include/structures/ArbolBST.h`: `construirDesdeOrdenados()` (divide y conqueras sobre la secuencia ordenada) |

## Lo que queda, y lo que no

Los nueve requisitos estan mapeados e implementados. De lo que el proyecto se
propuso **ademas** de la consigna (la Fase 7, "Extras y cierre tecnico"), queda
**una sola cosa**:

- El **export de reportes a archivo**: los seis reportes se muestran en
  pantalla pero no se pueden guardar en un archivo. **La consigna NO lo pide**
  (esta es la Fase 7, que es extras: los nueve requisitos van del POO a la
  validacion de entradas). Se decidio **no hacerlo**: cuesta 1-2 dias, es lo
  unico que agrega una funcion nueva, y deja abierto el formato del archivo
  de salida. Es una decision, no un forgot.

Lo que si se hizo de la Fase 7 y la Fase 8:

| Que | Donde |
|---|---|
| La semilla con las **doce** especialidades (un medico y sus turnos por especialidad) | `data/ejemplo/` |
| Los **tests de integracion** del dominio completo (el ciclo de vida entero, los borrados logicos y el error de persistencia) | `tests/test_integracion.cpp` |
| La verificacion de que `main.cpp` quedo **sin logica de negocio**, y que el CI la hace cumplir | `docs/decisiones.md` y `.github/workflows/ci.yml` |
| El **ensayo de la modificacion en vivo**: el campo opcional `observaciones` y el formato v3 que sigue leyendo la v2 | `docs/decisiones.md` (decision 54) |
| Las **ocho preguntas** de la defensa, contestadas | `docs/defensa.md` |
