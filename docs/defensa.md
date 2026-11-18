# Defensa: las ocho preguntas

Este documento es el guion de la defensa. Contiene las **ocho preguntas que
seguro te hacen**, en el orden del
[roadmap](https://github.com/adrianbeep/final-ingenieria-software/issues/1), y
para cada una:

- **La respuesta**: el texto para decir de memoria. Cada respuesta incluye el
  *por que* y la **alternativa descartada**, como es la convencion de
  [`docs/decisiones.md`](decisiones.md).
- **La evidencia**: una tabla con las referencias `archivo:linea` (relativas a
  la raiz del repo) y la frase que hay en ese lugar. Todas las lineas estan
  verificadas contra el codigo.

Las decisiones de diseno que se citan estan todas en
[`docs/decisiones.md`](decisiones.md) (54 decisiones, cada una con su
alternativa descartada y su por que) y el mapeo de los 9 requisitos
obligatorios a sus lineas esta en
[`docs/trazabilidad.md`](trazabilidad.md).

## Las ocho, de un vistazo

| # | Pregunta | La respuesta en una linea | Evidencia principal |
|---|---|---|---|
| 1 | ¿Por qué separaste las capas? | Cada capa tiene una responsabilidad y las reglas se escriben de un solo lado; el CI verifica dos de esas fronteras con `grep` | `src/main.cpp:14`, `include/services/ServicioTurnos.h:22`, `.github/workflows/ci.yml:46` |
| 2 | ¿Por qué un árbol y no un vector o una lista? | El acceso es por clave y el `contar()` es O(1); el `insertar()` es plano y por eso existe la segunda puerta, `construirDesdeOrdenados()` | `include/structures/ArbolBST.h:46`, `include/repositories/RepositorioCRUD.h:171` |
| 3 | ¿Por qué `friend` acá y no un getter? | `operator<<` es funcion libre y necesita el estado privado; el `friend` da el acceso minimo sin agrandar la interfaz publica ni romper el ADL | `include/entities/Medico.h:33`, `include/entities/Medico.h:72` |
| 4 | ¿Cómo garantizás que no se dupliquen turnos? | Por solapamiento de **intervalos** (no de horas) en los dos ejes, y solo los turnos vigentes ocupan horario | `src/services/ServicioTurnos.cpp:87`, `src/services/ServicioTurnos.cpp:480` |
| 5 | ¿Por qué `enum class` y no `string`? | El estado invalido es irrepresentable, la especialidad deja de ser decorativa y lo que se persiste es el codigo, no el indice | `include/domain/Especialidad.h:17`, `src/domain/Especialidad.cpp:28` |
| 6 | ¿Qué pasa si el programa muere escribiendo el archivo? | La escritura es atomica (temporal + rename) y, si la escritura falla, la mutacion en memoria se deshace | `include/persistence/ArchivoTexto.h:283`, `include/persistence/ArchivoTexto.h:527` |
| 7 | ¿Por que un solo `catch` alcanza? | Todas las excepciones del dominio cuelgan de `ExcepcionSistema`; los `catch` especificos estan donde hay algo que hacer con el error | `include/exceptions/ExcepcionesSistema.h:24`, `src/main.cpp:53` |
| 8 | ¿Cómo es el árbol? Mostralo en el papel. | Nodo con valor + dos punteros, sin puntero al padre; `insertar()` es plano y `construirDesdeOrdenados()` lo balancea en O(n) | `include/structures/ArbolBST.h:149`, `include/structures/ArbolBST.h:313` |

---

## 1. ¿Por qué separaste las capas?

### La respuesta

La respuesta corta: **cada capa tiene una sola responsabilidad, y las reglas
que las conectan son reglas que se pueden escribir de un solo lado**. Si una
regla aparece en dos capas, las dos copias se pueden desincronizar; si vive en
la capa de arriba, se puede saltar.

El camino que recorre el sistema es este:

1. **Dominio** (`include/domain/`, `include/entities/`): que es un medico, un
   paciente, un turno y una especialidad. No sabe de archivos, ni de pantallas,
   ni de terminal. Su unica salida es `std::ostream`, que es un *"sumidero"*: se
   puede escribir en el sin que exista ninguna pantalla ni ningun archivo. Por
   eso el dominio se testea sin levantar la TUI.
2. **Estructuras** (`include/structures/`): TADs genericos. `ArbolBST<T>` solo
   necesita que `T` tenga `operator<`; no sabe que es un id ni que es un
   medico. Se reusan con cualquier tipo.
3. **Persistencia** (`include/persistence/`): la unica capa que abre, escribe
   y renombra archivos. `ArchivoTexto<T>` habla con la lista enlazada, no con
   el arbol.
4. **Repositorio** (`include/repositories/`): la costura entre el arbol en
   memoria y el archivo. Guarda y devuelve entidades, y **no tiene reglas de
   negocio**: lo unico que dice es *"este id ya estaba"* o *"este id no
   existe"*.
5. **Servicio** (`include/services/`): la capa de reglas de negocio. Decide
   **que se puede hacer** y no **como se guarda nada**: no sabe de archivos, ni
   de arboles, ni de registros. Recibe los repositorios por interfaz, y por
   eso sus tests inyectan fakes en memoria y prueban las reglas sin tocar el
   disco.
6. **Interfaz** (`include/ui/`): solo presenta y captura. Si el servicio
   rechaza un turno, la pantalla muestra el mensaje del servicio; no
   reimplementa la regla.
7. **`main`** (`src/main.cpp`): solo **arma y despacha**. No tiene logica de
   negocio, y el `try/catch` global vive ahi y no en la capa `ui`: la capa de
   interfaz no decide como se le informa un error al usuario.

**La alternativa descartada** fue el *`main` grande*: meter la logica de
negocio en la pantalla o en el `main`, que es lo que sale primero cuando el
proyecto es chico. Se descarto por dos razones. La primera es que **las reglas
se podrian saltar**: si la validacion viviera en la pantalla, un `main` tocado
a mano, un test o la consola podrian guardar un turno imposible (un turno de
una especialidad que su medico no atiende, o un turno que pisa con otro). La
segunda es la **testabilidad**: la UI necesita terminal y el dominio no; si
todo estuviera en un solo archivo, no se podria probar una regla de negocio sin
levantar la interfaz.

Y esto no es solo una intencion: la separacion esta **verificada
automaticamente**. El CI corre en cada push y en cada PR, y dos de sus pasos
son `grep` sobre el arbol de archivos: uno falla si el dominio incluye
`fstream`, `iostream` o llama a `system()`, y el otro falla si alguna capa que
no sea la de interfaz menciona `ftxui`. Si alguien rompe la frontera, el build
se pone rojo antes de que llegue a la defensa.

Un detalle que suele confundir: **el contexto expone los repositorios
concretos, no las interfaces**. La razon es que las reglas de busqueda
(`buscarPorNombre`, `existeDni`, `existeLegajo`) viven en el repositorio
concreto; si el contexto entregara la interfaz, la pantalla tendria que
reescribirlas y quedaria una segunda fuente de verdad. El motor CRUD, en
cambio, si trabaja contra las interfaces: eso es lo que le permite a sus tests
inyectar fakes.

### La evidencia

| Referencia | Que hay en ese lugar |
|---|---|
| `src/main.cpp:14` | `// Punto de entrada. NO tiene logica de negocio (requisito 9 de la consigna):` |
| `src/main.cpp:15` | `// solo arma la aplicacion y la corre.` |
| `src/main.cpp:17` | `// El `try/catch` global vive ACA y no en la capa `ui`: la capa de interfaz` |
| `src/main.cpp:19` | `int main() {` — el cuerpo entero arma repositorios, servicio, contexto y aplicacion, y despacha. |
| `src/main.cpp:53` | `} catch (const ExcepcionSistema& e) {` — el `catch` global esta en el `main`, no en la `ui`. |
| `include/entities/Persona.h:26` | `// EL DOMINIO NO HACE E/S: la unica salida de datos es `std::ostream`, que es` |
| `include/entities/Persona.h:27` | `// un "sumidero" al que se le puede escribir sin que exista ninguna pantalla` |
| `include/structures/ArbolBST.h:27` | `// El TAD necesita SOLO dos cosas de `T`:` — el arbol es generico: no sabe que es un id. |
| `include/structures/ArbolBST.h:28` | `//   1. `bool operator<(const T&) const` -- define el ORDEN.` |
| `include/repositories/RepositorioCRUD.h:68` | `// ESTA CAPA NO TIENE REGLAS DE NEGOCIO` |
| `include/repositories/RepositorioCRUD.h:70` | `// El repositorio guarda y devuelve entidades. No sabe si dos turnos se` |
| `include/repositories/RepositorioCRUD.h:72` | `// inactivo puede ser agendado. Eso es del servicio. Lo unico que esta capa` |
| `include/services/ServicioTurnos.h:16` | `// Este servicio decide QUE SE PUEDE HACER: si un paciente inactivo puede` |
| `include/services/ServicioTurnos.h:18` | `// reprogramar. No decide COMO se guarda nada: no sabe de archivos, ni de` |
| `include/services/ServicioTurnos.h:22` | `// QUE HACE EL SERVICIO Y NO HACE EL REPOSITORIO` |
| `include/services/ServicioTurnos.h:26` | `// si un turno cancelado se puede reprogramar. Eso es TODO de este servicio,` |
| `include/services/ServicioTurnos.h:27` | `// y es la unica razon de que la capa exista.` |
| `src/ui/Pantallas/Turnos.cpp:1652` | `// servicio lo dice con su excepcion y la pantalla muestra ese mensaje.` — la UI no reimplementa la regla. |
| `src/ui/Pantallas/Turnos.cpp:1653` | `_servicioTurnos.agendar(turno);` — la pantalla llama al servicio, no al repositorio. |
| `include/ui/Contexto.h:30` | `// El contexto expone los repositorios CONCRETOS (`RepositorioPacientes`,` |
| `.github/workflows/ci.yml:46` | `- name: El dominio no toca I/O` |
| `.github/workflows/ci.yml:48` | `if grep -rnE '\b(fstream\|iostream\|system[[:space:]]*\()' include/domain include/entities; then` |
| `.github/workflows/ci.yml:57` | `- name: "El dominio no depende de FTXUI"` |
| `.github/workflows/ci.yml:59` | `if grep -rni 'ftxui' include/domain include/entities include/utils \` |

---

## 2. ¿Por qué un árbol y no un vector o una lista?

### La respuesta

Porque **el acceso del sistema es por clave** (el id, que es unico y
comparable) y porque **el conjunto crece**: se dan de alta medicos, pacientes y
turnos todo el tiempo.

Con un `std::vector` la busqueda por id es un recorrido lineal, O(n). Con el
arbol, `buscarPorClave()` es un **descenso**: compara la clave contra el nodo
y se baja al hijo que corresponde, asi que hace **O(altura)** comparaciones, que
con el arbol balanceado es **O(log n)**. Ademas, `size()` —y por lo tanto
`contar()` del repositorio— es **O(1) de verdad**: es un contador que el TAD
lleva, no un recorrido.

Y hay un segundo beneficio que suele pasarse por alto: **el arbol ya entrega
los datos ordenados**. `inOrder()` sale ordenado por clave (es la propiedad que
define a un BST), y ese mismo recorrido es lo que se escribe al archivo de
datos. Con un vector habria que ordenarlo entero en cada escritura
(O(n log n)).

**Ahora, la parte honesta, que juega a favor:** `insertar()` **no balancea**.
Es un BST plano: la altura es O(h) y en el peor caso O(n). Con ids secuenciales
(1, 2, 3...) insertados en orden ascendente, el arbol **degenera en una
cadena**. El proyecto no esconde esto: el propio header lo dice con todas las
letras y **hay un test que lo asserta** (1000 ids insertados en orden dan
altura 1000).

La respuesta completa es que hay **dos puertas**, y la segunda es la que se
usa:

1. `insertar()`: BST plano, para inserciones puntuales.
2. `construirDesdeOrdenados()`: construye el arbol **balanceado en O(n)** por
   *divide y conqueras*, tomando el **elemento del medio como raiz**.

Y el repositorio usa **siempre la segunda**: al cargar el archivo (que ya
viene ordenado por id, porque se escribio con el `inOrder()` del arbol) y
despues de **cada mutacion**, para que el arbol este balanceado *todo*
momento, no solo cuando se carga desde el disco. El rebalanceo no agrega un
recorrido extra: se apoya en el `inOrder()` que ya se estaba haciendo para
persistir.

**Y la lista enlazada, donde queda?** La lista **no es** la estructura de las
colecciones: es la estructura de la **capa de persistencia** (la
representacion en memoria de un archivo de datos: por ella pasan todas las
lecturas y todas las escrituras de `data/*.dat`). La respuesta honesta del
propio header es que, para el caso de uso concreto de la lista (append-only,
solo lectura y sin busquedas), un `std::vector` **seria mas eficiente**: la
lista esta porque es el TAD propio que pide el requisito 7 de la consigna y
porque es la estructura de la persistencia. **No es una decision de
rendimiento.**

**La alternativa descartada** fue `std::vector<T>` como backing de las tres
colecciones. Se descarto por dos cosas: agrega una estructura de STL en
exactamente la parte del sistema donde el requisito 7 pide una estructura
implementada por el alumno, y pierde las dos propiedades que hacen util al
arbol: la busqueda por clave O(log n) y el recorrido en orden que ya viene
gratis y es lo que se persiste. Y la lista como backing de las colecciones se
descarto por lo mismo: no ordena y su acceso aleatorio seria O(n).

### La evidencia

| Referencia | Que hay en ese lugar |
|---|---|
| `include/structures/ArbolBST.h:11` | `// ES EL BACKING DE LAS TRES COLECCIONES DEL SISTEMA (medicos, pacientes y` |
| `include/structures/ArbolBST.h:12` | `// turnos): el acceso es POR CLAVE (el id), que es unico y comparable.` |
| `include/structures/ArbolBST.h:46` | `// POR QUE EL ARBOL NO SE BALANCEA AL INSERTAR` |
| `include/structures/ArbolBST.h:48` | ``// `insertar()` es un BST plano: la altura es O(h) y en el peor caso O(n).`` |
| `include/structures/ArbolBST.h:49` | `// Con ids secuenciales (1, 2, 3...) insertados en orden ascendente el arbol` |
| `include/structures/ArbolBST.h:50` | `// se degenera en una cadena. Por eso existe la SEGUNDA PUERTA,` |
| `include/structures/ArbolBST.h:51` | ``// `construirDesdeOrdenados()`, que construye el arbol balanceado en O(n) con`` |
| `include/structures/ArbolBST.h:114` | `std::size_t size() const { return tam; }  // O(1): es el contador, no un recorrido` |
| `include/structures/ArbolBST.h:116` | `int altura() const;                        // O(n): solo para diagnostico` |
| `include/structures/ArbolBST.h:134` | `// Construye el arbol BALANCEADO a partir de una secuencia ORDENADA por` |
| `include/structures/ArbolBST.h:135` | ``// `operator<` (divide y conqueras: el elemento del medio es la raiz).`` |
| `include/structures/ArbolBST.h:138` | `void construirDesdeOrdenados(std::vector<T> ordenados);` |
| `include/repositories/RepositorioCRUD.h:175` | ``// `ArbolBST::insertar()` es un BST plano: con ids secuenciales`` |
| `include/repositories/RepositorioCRUD.h:178` | `// es O(altura), tambien es O(n): el arbol balanceado es la CONDICION` |
| `include/repositories/RepositorioCRUD.h:179` | `// para que la busqueda sea O(log n).` |
| `include/repositories/RepositorioCRUD.h:191` | `void rebalancear();` — "Reconstruye el arbol BALANCEADO a partir de su contenido actual." |
| `include/repositories/RepositorioCRUD.h:218` | `// Se rebalancea DESPUES de insertar: el arbol tiene que quedar` |
| `include/repositories/RepositorioCRUD.h:221` | `rebalancear();` — el rebalanceo va despues de cada insercion. |
| `include/repositories/RepositorioCRUD.h:333` | `_arbol.construirDesdeOrdenados(_arbol.inOrder());` — el rebalanceo se apoya en el `inOrder()` que ya se hacia para persistir. |
| `include/repositories/RepositorioCRUD.h:345` | `// O(1) de verdad: `size()` es el CONTADOR del TAD, no un recorrido.` |
| `include/repositories/RepositorioCRUD.h:346` | `return _arbol.size();` — `contar()` del repositorio. |
| `include/repositories/RepositorioCRUD.h:437` | `// DECISION R6: al cargar se usa SIEMPRE `construirDesdeOrdenados()`,` |
| `include/repositories/RepositorioCRUD.h:438` | `// NUNCA `insertar()` en un loop.` |
| `include/repositories/RepositorioCRUD.h:454` | `_arbol.construirDesdeOrdenados(registros.aVector());` |
| `include/structures/ListaEnlazada.h:10` | `// ES LA REPRESENTACION EN MEMORIA DE UN ARCHIVO DE DATOS: por ella pasan` |
| `include/structures/ListaEnlazada.h:11` | `// TODAS las lecturas y TODAS las escrituras de `data/*.dat`. Recorrido` |
| `include/structures/ListaEnlazada.h:29` | `// La respuesta honesta: para el caso de uso concreto del historial de turnos` |
| `include/structures/ListaEnlazada.h:32` | `// estructura de la capa de persistencia. No es una decision de rendimiento.` |
| `tests/test_arbol.cpp:487` | `CHECK(degenerado.altura() == cantidad);` — 1000 ids insertados en orden dan altura 1000: la degeneracion esta asertada. |
| `tests/test_arbol.cpp:492` | `CHECK(balanceado.altura() <= 11);` — el mismo conjunto, construido balanceado, da altura ≤ 11. |
| `tests/test_arbol.cpp:912` | `CHECK(comparaciones <= arbol.altura());` — la busqueda hace como maximo una comparacion por nivel. |
| `tests/test_arbol.cpp:914` | `CHECK(comparaciones < 20);` — con 1000 nodos, la busqueda no es un recorrido. |
| `tests/test_repositorio.cpp:947` | `CHECK(repo.arbol().size() == 100);` |
| `tests/test_repositorio.cpp:948` | `CHECK(repo.arbol().altura() <= 11);` — el arbol del repositorio queda balanceado al cargar. |

---

## 3. ¿Por qué `friend` acá y no un getter?

### La respuesta

Porque **`operator<<` es una funcion libre** (no es un miembro de la entidad) y
necesita leer el **estado privado** de la entidad para escribir el registro
canonico. Hay dos alternativas y las dos se descartaron:

**(a) Exponer getters publicos para cada campo.** Se descarta porque agranda
la interfaz publica *para siempre*: un getter que existe solo para el
serializador es un getter que nadie mas usa, y la interfaz publica deja de
ser "lo que la aplicacion necesita". El criterio del proyecto es el
inverso — un getter existe si tiene un **consumidor real** (la interfaz
grafica). Los getters que si estan (`getNombre()`, `getApellido()`,
`getLegajo()`, `getEspecialidad()`, `getId()`...) estan porque la UI los usa;
el `operator<<` no necesito agregar ninguno.

**(b) Hacerlo miembro** (`std::ostream& operator<<(std::ostream&) const;`
dentro de la clase). Se descarta porque **`operator<<` deja de ser una funcion
libre y el ADL deja de funcionar**. El ADL (argument-dependent lookup) es lo
que permite que la capa de persistencia escriba `out << registro` sin saber el
tipo: el mecanismo por el cual `ArchivoTexto<T>` es generico de verdad. Si el
`operator<<` fuera miembro, la capa de archivos tendria que conocer el tipo
concreto o el TAD perderia la propiedad que lo hace reutilizable.

El `friend` da el **acceso minimo**: exactamente la lectura del estado que el
serializador necesita, sin agrandar la interfaz publica y sin romper el ADL.

**Y hay un beneficio extra que vale la pena mencionar:** como el `friend` es de
la clase *derivada* (`Medico`, `Paciente`), tambien alcanza para llamar al
helper `protected` de la base, `Persona::escribirComunes()`. Por eso los 6
campos comunes se escriben **en un solo lugar** y cada entidad solo agrega
los suyos. Y el efecto de diseno es mas importante que el detalle de C++: **el
formato canonico vive en la entidad, que es quien lo conoce**; la capa de
persistencia no tiene una tabla de que campos tiene cada entidad.

Como regla general del proyecto: *cada sobrecarga tiene un consumidor real*.
El `operator<<` lo tiene, y es el que escribe el archivo de datos.

### La evidencia

| Referencia | Que hay en ese lugar |
|---|---|
| `include/entities/Medico.h:33` | `// POR QUE EL `friend`: `operator<<` es una funcion LIBRE (no un miembro de` |
| `include/entities/Medico.h:34` | `// la entidad) y necesita leer el estado PRIVADO. Las alternativas son (a)` |
| `include/entities/Medico.h:35` | `// exponer getters para cada campo, que ensancha la interfaz publica para` |
| `include/entities/Medico.h:36` | `// siempre, o (b) hacerlo miembro, con lo que `operator<<` deja de ser una` |
| `include/entities/Medico.h:37` | `// funcion libre y el ADL deja de funcionar. El `friend` da el acceso minimo` |
| `include/entities/Medico.h:38` | `// sin agrandar la interfaz publica.` |
| `include/entities/Medico.h:72` | `friend std::ostream& operator<<(std::ostream& os, const Medico& medico);` |
| `include/entities/Paciente.h:33` | `// POR QUE EL `friend`: igual que en `Medico`, el `operator<<` es una` |
| `include/entities/Paciente.h:71` | `friend std::ostream& operator<<(std::ostream& os, const Paciente& paciente);` |
| `include/entities/Turno.h:39` | `// POR QUE EL `friend`: igual que en las demas entidades, el `operator<<` es` |
| `include/entities/Turno.h:102` | `friend std::ostream& operator<<(std::ostream& os, const Turno& turno);` |
| `include/entities/Persona.h:145` | `// Escribe los 6 campos comunes del formato canonico. `protected` para que` |
| `include/entities/Persona.h:147` | `void escribirComunes(std::ostream& os) const {` — el helper `protected` de la base. |
| `src/entities/Medico.cpp:72` | `medico.escribirComunes(os);` — el `friend` de la derivada alcanza el `protected` de la base. |
| `src/entities/Medico.cpp:76` | `return os << '|' << medico.legajo << '|' << codigoDe(medico.especialidad);` — lee los miembros privados. |
| `src/entities/Turno.cpp:137` | `os << turno.id << '|' << turno.idMedico << '|' << turno.idPaciente << '|'` — lee los miembros privados del turno. |
| `include/persistence/ArchivoTexto.h:715` | `// El `operator<<` del tipo `T` escribe el registro canonico. Se resuelve` |
| `include/persistence/ArchivoTexto.h:716` | `// por ADL: es la funcion friend que la entidad declara.` |
| `include/persistence/ArchivoTexto.h:717` | `out << registro;` — el consumidor real del `operator<<`, y el que depende del ADL. |

---

## 4. ¿Cómo garantizás que no se dupliquen turnos?

### La respuesta

Con **tres mecanismos distintos, en tres capas distintas**, porque "duplicado"
significa tres cosas distintas.

**1. El choque de horarios (la regla de negocio, y la que de verdad importa).**
No se compara la hora de inicio: se comparan **intervalos**. Dos intervalos se
solapan si **cada uno empieza antes de que termine el otro**:
`(inicioA < finB) && (inicioB < finA)`.

- Por eso un turno de **10:00 a 10:45** y otro de **10:30 a 11:15 se pisan**,
  aunque sus horas de inicio sean distintas. Comparando solo los inicios con
  el `operator<` de la `Hora`, el primero es menor que el segundo y el segundo
  no es menor que el primero: la comparacion daria *"no se pisan"* y dejaria
  pasar los dos.
- Y el criterio de intervalos distingue tambien el **caso limite**: dos
  turnos que se tocan justo (10:00-10:45 y 10:45-11:30) **no** se pisan,
  porque el segundo empieza exactamente cuando termina el primero.

Los intervalos se miden en **minutos absolutos** desde el primer dia del
dominio, asi que un turno que cruza la medianoche no es un caso especial: es
un intervalo mas largo.

El choque se busca en los **dos ejes**: basta con que coincida el **paciente**
o el **medico** (no pueden ser la misma persona, asi que si coincide uno, esa
persona esta ocupada). Y **solo los turnos vigentes ocupan el horario**: un
turno cancelado o reprogramado libero su horario y ese horario se puede
volver a usar.

Cuando hay choque se lanza `TurnoOcupadoException` **y no se guarda nada**:
`agendar()` aplica *todas* las reglas primero y recien despues escribe, asi
que si una regla falla el repositorio queda como estaba. Y las mismas reglas
se aplican al reprogramar, asi que reprogramar no esquiva el control de
solapamiento.

**2. La identidad del turno.** `Turno::operator==` compara por id (un turno es
un unico tipo). En `Persona`, `operator==` compara por id **y** por tipo,
porque el id es unico **por entidad**, no global: `Medico(5) == Paciente(5)`
tiene que ser `false`. Sin el tipo, el arbol no tendria el *"strict weak
ordering"* que necesita para poder detectar duplicados.

**3. La unicidad de los identificadores naturales.** El **DNI** del paciente y
el **legajo** del medico son unicos en el sistema, y esa unicidad la verifica
el **repositorio** (que es quien tiene el conjunto de las entidades
cargadas), no la entidad (que no sabe que hay cargado: su unicidad es una
regla del *conjunto*, no de la entidad). Se verifica **antes** de tocar el
arbol, en los dos unicos puntos por los que un dni entra al repositorio:
`alta()` y `modificar()`. Y lo mismo para el **id**, que lanza
`IdDuplicadoException`.

**La alternativa descartada** fue comparar *"misma fecha + misma hora"* (o
guardar un `set` de tuplas fecha/hora). Se descarto porque el requisito real
no es *"dos turnos empiecen a la vez"*, sino *"el paciente (o el medico) no
este ocupado durante ese intervalo"*: con la comparacion por hora de inicio, un
turno de 10:00 a 11:30 y otro de 10:15 a 10:45 se solapan y el sistema los
aceptaria. La segunda alternativa descartada fue comprobar la unicidad del dni
dentro de la entidad: se descarto porque una entidad sola no sabe que dnis hay
cargados, asi que la verificacion tiene que vivir en quien tiene el conjunto.

### La evidencia

| Referencia | Que hay en ese lugar |
|---|---|
| `include/services/ServicioTurnos.h:80` | `//   5. el turno no se pisa con ningun turno vigente, NI por el paciente` |
| `include/services/ServicioTurnos.h:83` | `// El turno se guarda recien cuando TODAS las reglas pasaron: si una` |
| `include/services/ServicioTurnos.h:84` | `// falla, el repositorio queda como estaba.` |
| `src/services/ServicioTurnos.cpp:69` | `// DOS INTERVALOS SE SOLAPAN SI CADA UNO EMPIEZA ANTES DE QUE TERMINE EL` |
| `src/services/ServicioTurnos.cpp:70` | `// OTRO: `(inicioA < finB) && (inicioB < finA)`.` |
| `src/services/ServicioTurnos.cpp:74` | `// Un turno de 10:00 a 10:45 y otro de 10:30 a 11:15 SE PISAN, aunque sus` |
| `src/services/ServicioTurnos.cpp:83` | `// Y el criterio de intervalos tambien distingue el caso limite: dos turnos` |
| `src/services/ServicioTurnos.cpp:84` | `// que se tocan JUSTO (10:00-10:45 y 10:45-11:30) NO se pisan, porque el` |
| `src/services/ServicioTurnos.cpp:88` | `return a.inicio < b.fin && b.inicio < a.fin;` — el test de solapamiento, en una linea. |
| `src/services/ServicioTurnos.cpp:480` | `void ServicioTurnos::verificarSolapamiento(const Turno& turno_, int idTurnoExcluido_) {` |
| `src/services/ServicioTurnos.cpp:498` | `if (existente.getEstado() != EstadoTurno::Activo) {` — solo los turnos vigentes ocupan horario. |
| `src/services/ServicioTurnos.cpp:508` | `if (existente.getIdPaciente() != turno_.getIdPaciente()` |
| `src/services/ServicioTurnos.cpp:509` | `&& existente.getIdMedico() != turno_.getIdMedico()) {` — el choque se busca en los dos ejes. |
| `src/services/ServicioTurnos.cpp:513` | `if (!seSolapan(nueva, ventanaDe(existente))) {` |
| `src/services/ServicioTurnos.cpp:520` | `throw TurnoOcupadoException(` — el error del choque, con los ids de lo que ya estaba ocupado. |
| `src/services/ServicioTurnos.cpp:112` | `validarAgendamiento(turno_);` — primero todas las reglas. |
| `src/services/ServicioTurnos.cpp:114` | `_turnos.alta(turno_);` — recien despues se guarda. |
| `include/entities/Turno.h:98` | `bool operator==(const Turno& otra) const { return id == otra.id; }` |
| `include/entities/Persona.h:112` | `// `operator==` compara el id Y el tipo: el id es unico POR ENTIDAD, no` |
| `include/entities/Persona.h:117` | `bool operator==(const Persona& otra) const {` |
| `include/entities/Persona.h:118` | `return id == otra.id && getTipo() == otra.getTipo();` |
| `include/repositories/RepositorioPacientes.h:24` | `// LA UNICIDAD DEL DNI: POR QUE VIVE ACA Y NO EN LA ENTIDAD` |
| `include/repositories/RepositorioPacientes.h:32` | `// Y el repositorio es el UNICO lugar donde se puede verificar: la entidad` |
| `include/repositories/RepositorioPacientes.h:35` | `// conjunto: el repositorio.` |
| `src/repositories/RepositorioPacientes.cpp:139` | `if (buscarDni(paciente.getDni(), paciente.getId(), idExistente)) {` |
| `src/repositories/RepositorioPacientes.cpp:140` | `throw DatoInvalidoException("dni", paciente.getDni(),` |
| `include/repositories/RepositorioMedicos.h:23` | `// LA UNICIDAD DEL LEGAJO: POR QUE VIVE ACA Y NO EN LA ENTIDAD` |
| `include/repositories/RepositorioCRUD.h:212` | `if (buscarPorIdEnArbol(entidad.getId(), existente)) {` |
| `include/repositories/RepositorioCRUD.h:213` | `throw IdDuplicadoException(entidad.getId(), _archivo.nombreEntidad());` |

---

## 5. ¿Por qué `enum class` y no `string`?

### La respuesta

Por tres razones, de la mas fuerte a la mas chica.

**1. El estado invalido es irrepresentable.** Con un `string`, la especialidad
es texto libre: `"CARD"`, `"Cardiología"` y `"cardiologia"` son tres valores
distintos, y el error aparece **despues**, cuando el usuario ya escribio todo
lo demas. Con un `enum class` el compilador obliga: no hay conversion implicita
a `int`, no hay colision de nombres entre enumeradores, y **no existe
`Especialidad::CARD == 0`** porque `0` no es una `Especialidad`. Y como el
catalogo son doce valores fijos, el `Medico` no necesita ni un validador
propio: **un valor del enum es siempre valido**, y lo unico que hay que
validar es que el codigo que llega del archivo exista en el catalogo.

**2. La especialidad deja de ser decorativa: aparece la primera regla de
negocio real.** Antes de esto, `especialidad` era un dato que se guardaba y se
mostraba pero **no restringia nada**. Con el enum se puede **comparar**, y en
comparar aparece una regla: *un medico atiende una sola especialidad, asi que
el turno tiene que ser de la especialidad del medico que lo atiende*. Esa
comparacion es directamente imposible de escribir con dos `string` sin
inventar una funcion de comparacion, y con strings seria facil que se comparen
mal (con tildes, con mayusculas, con el espacio de mas). La decision del
proyecto llego a este punto justamente por esto: **la especialidad paso de ser
un dato decorativo a ser una regla de negocio**. Y el efecto se vio en la
semilla: antes de la regla, 11 de los 13 turnos de ejemplo estaban atendidos
por un medico de otra especialidad; con la regla, eso **no se puede crear**
(`docs/decisiones.md:33`).

**3. Lo que se persiste es un codigo, no el indice ni el texto.** En el
archivo viaja `"CARD"`. Nunca `(int)especialidad`, porque el indice depende
del **orden de la declaracion** del enum: insertar una especialidad en el
medio corromperia todos los archivos guardados. Nunca `"Cardiología"`, porque
es largo y puede cambiar: renombrarla invalidaria todos los archivos de
datos. El codigo es corto y estable (va al archivo); la descripcion es larga y
cambiable (va a pantalla). El *reverse lookup* (`especialidadDesdeCodigo`) es
una funcion mas del catalogo, y lanza `DatoInvalidoException` si el codigo no
existe.

**Y el catalogo es una sola tabla.** `descripcionDe()`, `codigoDe()` y
`especialidadDesdeCodigo()` derivan de la **misma** fuente de verdad. Si
fueran tres listas paralelas, al agregar una especialidad se olvidaria de
una y `descripcionDe` devolveria la descripcion de otra.

**La alternativa descartada** fue hacer `Especialidad` una **entidad CRUD**
(con id, tabla, repositorio y pantallas). Se descarto porque la especialidad
**no se escribe: se elige** de una lista de doce. Hacerla entidad agrega una
tabla, un repositorio y un juego de pantallas, y no aporta ninguna regla que
el enum no cubra. La otra alternativa descartada es el `string` libre, que es
el caso que se acaba de argumentar.

### La evidencia

| Referencia | Que hay en ese lugar |
|---|---|
| `include/domain/Especialidad.h:17` | ``// POR QUE `enum class` Y NO `enum` PELADO: sin conversion implicita a `int`,`` |
| `include/domain/Especialidad.h:18` | `// sin colision de nombres entre enumeradores, y el compilador obliga a ser` |
| `include/domain/Especialidad.h:19` | `// explicito. No existe `Especialidad::CARD == 0` porque `0` no es una` |
| `include/domain/Especialidad.h:20` | `// `Especialidad`.` |
| `include/domain/Especialidad.h:9` | ``// `Especialidad` NO es una entidad: no tiene id, ni se persiste, ni tiene CRUD.`` |
| `include/domain/Especialidad.h:22` | `// POR QUE CODIGO Y DESCRIPCION SON DOS COSAS: el codigo es corto y estable` |
| `include/domain/Especialidad.h:24` | `// Si se persistiera la descripcion, renombrar una especialidad invalidaria` |
| `include/domain/Especialidad.h:31` | `enum class Especialidad {` — los doce valores del catalogo. |
| `include/domain/Especialidad.h:47` | `std::string descripcionDe(Especialidad especialidad);` — para mostrar. |
| `include/domain/Especialidad.h:50` | `std::string codigoDe(Especialidad especialidad);` — para persistir. |
| `include/domain/Especialidad.h:54` | `Especialidad especialidadDesdeCodigo(const std::string& codigo);` — el reverse lookup. |
| `src/domain/Especialidad.cpp:11` | `// EL CATALOGO ES UNA SOLA TABLA.` |
| `src/domain/Especialidad.cpp:13` | `// Si `descripcionDe`, `codigoDe` y `especialidadDesdeCodigo` tuvieran tres` |
| `src/domain/Especialidad.cpp:28` | `const InfoEspecialidad CATALOGO[] = {` — la fuente de verdad: enum, codigo y descripcion. |
| `src/domain/Especialidad.cpp:29` | `{Especialidad::CARD, "CARD", "Cardiología"},` — la primera fila del catalogo. |
| `src/domain/Especialidad.cpp:91` | `Especialidad especialidadDesdeCodigo(const std::string& codigo) {` |
| `src/domain/Especialidad.cpp:94` | `throw DatoInvalidoException("especialidad", codigo, "codigo de especialidad desconocido");` |
| `include/entities/Medico.h:24` | `// LA ESPECIALIDAD ES UN `enum class`, NO UN TEXTO` |
| `include/entities/Medico.h:28` | `// no hay `especialidadValido` ni limites de longitud: un valor del enum es` |
| `include/entities/Medico.h:29` | `// SIEMPRE valido, y lo unico que hay que validar es que el CODIGO que` |
| `include/entities/Turno.h:36` | ``// `especialidad` viaja con su CODIGO ("CARD"), nunca con el indice del enum:`` |
| `include/entities/Turno.h:37` | `// reordenar el enum no tiene que invalidar los archivos de datos.` |
| `src/services/ServicioTurnos.cpp:402` | `// --- REGLA 2b: el turno es de la especialidad del medico ---` |
| `src/services/ServicioTurnos.cpp:408` | `if (medico.getEspecialidad() != turno_.getEspecialidad()) {` — la comparacion que solo es posible con el enum. |
| `src/services/ServicioTurnos.cpp:410` | `"el medico no atiende esa especialidad");` |
| `docs/decisiones.md:33` | "... el ejemplo de datos tenia 11 de 13 turnos con el medico de otra especialidad, algo que con esta regla **no se puede crear**" |

---

## 6. ¿Qué pasa si el programa muere escribiendo el archivo?

### La respuesta

La pregunta tiene dos partes: **que pasa con el archivo** y **que pasa con la
memoria**.

### (a) El archivo: la escritura es atomica

**El archivo de datos nunca se escribe en el lugar.** El flujo tiene cuatro
pasos:

1. **Se serializa todo en memoria** (un `ostringstream`). Si algo lanza (un
   dato invalido), **no se toco el disco**: el archivo original sigue
   intacto. Por eso el buffer va antes de abrir nada.
2. **Se escribe un temporal**, en el **mismo directorio** del destino (el
   rename solo es atomico dentro del mismo volumen), con un nombre **unico**
   por escritura, sufijo `.tmp` y en modo **binario** (en modo texto el
   runtime traduce el `\n` a CRLF en Windows y el archivo commiteado solo seria
   reproducible en la maquina donde se genero).
3. **Se cierra el temporal explicitamente y se chequea el resultado.** El
   `close()` es donde aparecen los errores diferidos (cuota, error de E/S del
   dispositivo): si se deja al destructor, ese error llega tarde y el rename
   publicaria un archivo a medio escribir. Si el `close()` falla, **el destino
   no se toca** (sigue con los datos viejos) y el `.tmp` se borra.
4. **Se hace el rename** (`std::filesystem::rename`), que es **atomico**: el
   destino pasa de la version vieja a la nueva de un solo tiron. Si el rename
   falla, se borra el `.tmp` y el archivo viejo sigue como estaba.

La consecuencia es la que importa: **el archivo de datos esta siempre en una
version completa**. Nunca queda truncado, nunca queda a medio escribir. Si el
proceso muere entre el paso 2 y el paso 4, lo que queda es un `.tmp` huerfano
y el archivo de datos **sigue siendo el de antes, integro**. Y si muere
despues del rename, el archivo nuevo ya esta entero: no hay nada a medias.

### (b) La memoria: la mutacion se deshace si la escritura falla

El repositorio muta la memoria y **despues** escribe el archivo. Como la
escritura es atomica, cuando la escritura falla el archivo destino conserva
la version anterior... pero la entidad ya habia quedado en la memoria. Si no
se deshiciera, el repositorio quedaria con la **memoria y el disco
desincronizados**: la entidad existiria en memoria pero no en el archivo, y el
proximo `persistir()` la escribiria como si fuera nueva. La correccion es
deshacer la mutacion cuando la escritura falla: en `alta()` se saca la entidad
del arbol, y en `modificar()` se restaura la entidad anterior; despues se
vuelve a lanzar la excepcion, asi que el llamador se entera igual.

> **El arreglo del punto (b) esta verificado por un test de integracion**
> (`tests/test_integracion.cpp`): sin el, el caso "si el archivo no se puede
> escribir" falla 6 aserciones. Es el mismo criterio que el resto del
> proyecto: una garantia de la que no hay prueba no es una garantia.

### (c) Y si la excepcion llega hasta arriba

Esta el `catch` global del `main`: cualquier `ExcepcionSistema` que llegue
hasta arriba se imprime por `stderr` y el programa sale con **codigo 0**, sin
dejar la pantalla a medias ni un core dump.

**La alternativa descartada** fue **escribir directamente sobre el archivo de
datos** (un `ofstream` con `trunc` sobre el destino). Se descarto porque un
corte de luz o un `kill` en mitad de la escritura deja el archivo truncado:
la version anterior se perdio y no hay de donde recuperarla. Con el
temporal + rename, **la version anterior sobrevive hasta que la nueva esta
completa**. La segunda alternativa descartada fue escribir un `.bak` o un
`.tmp` fijo antes de cada guardado: se descarto porque un temporal de nombre
fijo se pisa con dos escritores concurrentes, y porque el temporal tiene que
vivir en el mismo directorio para que el rename sea atomico.

### La evidencia

| Referencia | Que hay en ese lugar |
|---|---|
| `include/persistence/ArchivoTexto.h:283` | `// Escribe TODOS los registros, de forma ATOMICA. Reemplaza el archivo` |
| `include/persistence/ArchivoTexto.h:389` | `//   - vive en el MISMO DIRECTORIO que el destino (el rename atomico solo` |
| `include/persistence/ArchivoTexto.h:391` | `//   - termina en `.tmp` (asi lo cubre el `.gitignore`);` |
| `include/persistence/ArchivoTexto.h:433` | `// --- PASO 1: serializar TODO en memoria ---` |
| `include/persistence/ArchivoTexto.h:434` | `// Si algo lanza (un dato invalido), NO se toco el disco: el archivo` |
| `include/persistence/ArchivoTexto.h:435` | `// original sigue intacto. Por eso el buffer va antes de abrir nada.` |
| `include/persistence/ArchivoTexto.h:444` | `// --- PASO 2: escribir el `.tmp`, en el MISMO directorio del destino ---` |
| `include/persistence/ArchivoTexto.h:492` | `// `binary` explicito: en modo TEXTO el runtime traduce cada '\n' a` |
| `include/persistence/ArchivoTexto.h:495` | `// archivo generado es IDENTICO byte a byte en cualquier SO.` |
| `include/persistence/ArchivoTexto.h:496` | `std::ofstream salida(temporal, std::ios::out \| std::ios::binary \| std::ios::trunc);` |
| `include/persistence/ArchivoTexto.h:503` | `if (!salida) {` — error de escritura (disco lleno, permisos). |
| `include/persistence/ArchivoTexto.h:512` | `// --- PASO 2.1: el `close()` EXPLICITO y su resultado ---` |
| `include/persistence/ArchivoTexto.h:517` | `// se cierra explicitamente y se CHECKEA: si falla, el destino NO se` |
| `include/persistence/ArchivoTexto.h:518` | `// toca (sigue con los datos viejos) y el `.tmp` se borra.` |
| `include/persistence/ArchivoTexto.h:519` | `salida.close();` |
| `include/persistence/ArchivoTexto.h:520` | `if (salida.fail()) {` |
| `include/persistence/ArchivoTexto.h:527` | `// --- PASO 3: el rename ATOMICO ---` |
| `include/persistence/ArchivoTexto.h:531` | `std::filesystem::rename(temporal, destino, error);` |
| `include/persistence/ArchivoTexto.h:532` | `if (error) {` — el rename fallo: el archivo viejo sigue como estaba. |
| `include/repositories/RepositorioCRUD.h:237` | `persistir();` — la llamada que escribe el archivo; esta dentro del `try` que deshace la mutacion si falla. |
| `include/repositories/RepositorioCRUD.h:464` | `_archivo.guardar(_arbol.inOrderEnLista());` — lo que se persiste es la lista ordenada del arbol. |
| `src/main.cpp:53` | `} catch (const ExcepcionSistema& e) {` — la ultima red. |
| `src/main.cpp:54` | `std::cerr << "Error: " << e.what() << "\n";` |
| `include/repositories/RepositorioCRUD.h:226` | `// Y si la escritura FALLA, la mutacion se DESHACE.` — el comentario que explica el por que. |
| `include/repositories/RepositorioCRUD.h:236` | `try {` — el `try/catch` del `persistir()` en `alta()`. |
| `include/repositories/RepositorioCRUD.h:239` | `_arbol.eliminar(entidad);` — se deshace la insercion. |
| `include/repositories/RepositorioCRUD.h:275` | ``// Misma regla que en `alta()`: si la escritura falla, la memoria vuelve`` — la misma regla en `modificar()`. |
| `include/repositories/RepositorioCRUD.h:284` | `_arbol.insertar(anterior);` — se restaura la entidad anterior en `modificar()`. |
---

## 7. ¿Por qué un solo `catch` alcanza para todas tus excepciones?

### La respuesta

Porque **todas las excepciones del dominio cuelgan de una sola raiz**:
`ExcepcionSistema`, que deriva de `std::runtime_error`. Las siete derivadas
son `ArchivoNoEncontradoException`, `IdDuplicadoException`,
`DatoInvalidoException`, `EntidadNoExisteException`, `TurnoOcupadoException`,
`TurnoNoExisteException` y `FormatoDesconocidoException`. Un
`catch (const ExcepcionSistema& e)` las cubre a todas por polimorfismo de
excepciones, y `e.what()` trae el mensaje.

Pero la respuesta completa tiene **tres partes**:

**1. Son pocas y son de la misma familia.** No hay una jerarquia de veinte
tipos con errores que se solapen. Si la hubiera, el `catch` global se volveria
un `switch` disfrazado y la pregunta *"¿que hago con este error?"* quedaria
respondida en un solo lugar por tipo. Aca el `catch` global es una decision de
**politica**: *algo fallo, lo digo y el programa sale con codigo 0*.

**2. El `catch` especifico esta donde tiene sentido: donde se puede hacer algo
con el error.** Un `catch` global no reemplaza a los locales: los
**complementa**. Y hay tres situaciones distintas:

- **Donde la excepcion trae informacion que se puede mostrar.** `DatoInvalidoException`
  expone el **campo** que fallo y el **valor**, asi que la interfaz puede
  decir *"Dato invalido en campo 'email'"* sin reinterpretar el mensaje. El
  motor CRUD captura la excepcion, la muestra y **vuelve al formulario**: no
  se pierde lo que el usuario escribio. La pantalla de turnos hace lo mismo
  con los errores del servicio.
- **Donde una excepcion de la biblioteca estandar puede colarse.** `std::stoi`
  puede lanzar `std::out_of_range` o `std::invalid_argument`, que **no** son
  `ExcepcionSistema` y se escaparian del `catch` global. Por eso, en el
  dominio, `enteroDesdeTexto` las traduce al error del dominio (que si sabe
  decir que campo fallo y con que valor), y en la consola hay **dos** `catch`
  distintos con **dos** mensajes distintos: "no es un numero" y "es demasiado
  grande". Son mensajes distintos porque el problema es distinto; un solo
  `catch` con un solo mensaje habria mentido en uno de los dos casos.
- **Donde una excepcion cruda podria matar el proceso.** Como el `catch`
  global solo cubre `ExcepcionSistema`, una `filesystem_error` o un
  `bad_alloc` escaparian y el proceso moriria con SIGABRT en vez de con codigo
  0. Por eso el helper de "cargar datos de ejemplo" **promete nunca lanzar**
  y tiene su propio `try/catch` con `catch (...)`. En la capa de archivos,
  `leerRegistro` traduce cualquier excepcion a `DatoInvalidoException` con el
  archivo y la linea: el contrato de `cargar()` es que un registro mal formado
  **nunca** escapa como excepcion cruda.

**3. El `catch` global esta en el `main`, no en la capa `ui`.** La interfaz no
decide como se le informa un error al usuario: eso lo decide el `main`, que es
el que sabe que no hay a quien avisarle y que lo correcto es escribir por
`stderr` y salir con codigo 0.

**La alternativa descartada** fue un `catch` por tipo en el `main` (o un
`catch (...)` gigante que imprima el tipo de la excepcion). Se descarto porque,
con siete excepciones que son la misma familia, **no agrega informacion**: lo
que el `catch` global necesita es el *mensaje*, y el mensaje ya viene en
`what()`. Y el `catch (...)` gigante se descarto por lo contrario: esconde
bugs, porque una `std::logic_error` de programacion —un error de codigo, no de
datos— se reportaria como un "Error: ..." mas y nadie la encontraria.

### La evidencia

| Referencia | Que hay en ese lugar |
|---|---|
| `include/exceptions/ExcepcionesSistema.h:23` | `// Raiz de todas las excepciones propias del sistema.` |
| `include/exceptions/ExcepcionesSistema.h:24` | `class ExcepcionSistema : public std::runtime_error {` |
| `include/exceptions/ExcepcionesSistema.h:30` | `class ArchivoNoEncontradoException : public ExcepcionSistema {` |
| `include/exceptions/ExcepcionesSistema.h:36` | `class IdDuplicadoException : public ExcepcionSistema {` |
| `include/exceptions/ExcepcionesSistema.h:44` | `class DatoInvalidoException : public ExcepcionSistema {` |
| `include/exceptions/ExcepcionesSistema.h:50` | `const std::string& campo() const;` — el campo que fallo, para que la UI lo muestre. |
| `include/exceptions/ExcepcionesSistema.h:51` | `const std::string& valor() const;` |
| `include/exceptions/ExcepcionesSistema.h:65` | `class EntidadNoExisteException : public ExcepcionSistema {` |
| `include/exceptions/ExcepcionesSistema.h:71` | `class TurnoOcupadoException : public ExcepcionSistema {` |
| `include/exceptions/ExcepcionesSistema.h:77` | `class TurnoNoExisteException : public ExcepcionSistema {` |
| `include/exceptions/ExcepcionesSistema.h:92` | `class FormatoDesconocidoException : public ExcepcionSistema {` |
| `src/main.cpp:53` | `} catch (const ExcepcionSistema& e) {` — un solo `catch` para las siete. |
| `src/main.cpp:54` | `std::cerr << "Error: " << e.what() << "\n";` |
| `src/main.cpp:17` | `// El `try/catch` global vive ACA y no en la capa `ui`: la capa de interfaz` |
| `include/entities/Persona.h:234` | `} catch (const std::out_of_range&) {` — la excepcion de la biblioteca traducida al error del dominio. |
| `include/entities/Persona.h:237` | `} catch (const std::invalid_argument&) {` |
| `include/entities/Persona.h:238` | `// No deberia llegar aca (el formato ya se valido con` |
| `src/ui/Consola.cpp:35` | `} catch (const std::invalid_argument&) {` — "no es un numero". |
| `src/ui/Consola.cpp:41` | `} catch (const std::out_of_range&) {` — "es demasiado grande": dos mensajes distintos. |
| `include/ui/CrudPersonas.h:1260` | `} catch (const DatoInvalidoException& error) {` — la UI captura para mostrar y volver al formulario. |
| `include/ui/CrudPersonas.h:1267` | `avisar(error.motivo());` |
| `include/ui/CrudPersonas.h:1269` | `} catch (const ExcepcionSistema& error) {` |
| `src/ui/Pantallas/Turnos.cpp:1690` | `} catch (const ExcepcionSistema& error) {` — la pantalla de turnos muestra el error del servicio. |
| `src/ui/CargarEjemplo.cpp:112` | `} catch (...) {` — la red que evita que una excepcion cruda mate el proceso. |
| `src/ui/CargarEjemplo.cpp:125` | `// El helper promete NUNCA LANZAR: el `catch` global de `main` solo` |
| `src/ui/CargarEjemplo.cpp:126` | ``// catchea `ExcepcionSistema`, asi que una `filesystem_error` o un`` |
| `include/persistence/ArchivoTexto.h:740` | `} catch (const DatoInvalidoException& error) {` — la capa de archivo agrega archivo y linea. |
| `include/persistence/ArchivoTexto.h:752` | `} catch (const std::exception& error) {` — "RED DE SEGURIDAD": ninguna excepcion cruda escapa de `cargar()`. |

---

## 8. ¿Cómo es el árbol? Mostralo en el papel.

### La respuesta

`ArbolBST<T>` (`include/structures/ArbolBST.h:56`) es un **arbol binario de
busqueda escrito a mano**, con **punteros crudos** y la **Regla de Cinco**
explicita (constructor de copia, asignacion por copia, constructor de
movimiento, asignacion por movimiento y destructor).

El **nodo** es chiquito: un valor y dos punteros. **No tiene puntero al
padre**: eso simplifica el borrado y evita el puntero al padre que queda
colgando. El arbol, por su parte, guarda la **raiz** y un **contador de
tamano** (`tam`).

**Como se inserta:** se baja por el arbol comparando con `operator<`. Si el
valor nuevo es menor que el del nodo, se baja a la izquierda; si es mayor, a
la derecha; y si **ninguno** es menor que el otro, son equivalentes y el
valor ya estaba (y `insertar()` devuelve `false` sin tocar nada).

**Como se borra:** los tres casos. Sin hijos, el nodo se reemplaza por
`nullptr`. Con un hijo, el hijo ocupa el lugar. Con dos hijos, el **sucesor en
orden** (el minimo del subarbol derecho) copia su valor al nodo y se elimina
de la derecha.

**Y dos consultas de diagnostico:** `altura()` (cuantos niveles tiene el
arbol) y `nodosVivos()`, que es un **contador de fugas portable**: cada TAD
cuenta sus nodos vivos y los tests verifican que el contador vuelve a cero. Es
`static` dentro de una plantilla, asi que hay uno por tipo: el de
`ArbolBST<Medico>` es independiente del de `ArbolBST<Paciente>`.

**El bosquejo, en papel.** Las tres etapas que pide el profesor:

### 1. El arbol vacio

```
        (ningun nodo)

              |
           raiz = nullptr
           tam   = 0
           altura() = 0
           inOrder() = []
```

Un arbol recien construido esta vacio: la raiz es `nullptr`, el contador de
tamano es 0, la altura es 0 y el recorrido en orden no devuelve nada.

### 2. La insercion de cinco claves con `insertar()` (el BST plano)

Claves `1, 2, 3, 4, 5`, insertadas en orden ascendente:

```
              1
               \
                2
                 \
                  3
                   \
                    4
                     \
                      5
```

Cada clave nueva es **mayor** que la de la raiz, asi que siempre cae por la
derecha: el arbol **degenera en una cadena**. La altura es **5** (que es
`n`), y el recorrido en orden sigue dando `1 2 3 4 5` (el BST plano
ordenado tambien sale ordenado).

El coste de la degradacion, medido contando comparaciones (no el reloj):

```
   clave buscada   camino (cadena)        comparaciones
        1              1                        1
        2              1 2                      2
        3              1 2 3                    3
        4              1 2 3 4                  4
        5              1 2 3 4 5                5
```

Es decir: con ids secuenciales, la busqueda por clave vuelve a ser **O(n)**.
Este es el comportamiento que el propio header declara y que los tests
asertan (1000 ids insertados en orden dan altura 1000).

### 3. El mismo conjunto por `construirDesdeOrdenados()` (el arbol balanceado)

`construirDesdeOrdenados({1, 2, 3, 4, 5})` aplica **divide y conqueras**: el
elemento del medio es la raiz y el resto se reparte en dos mitades.

La cuenta, en papel, con `medio = desde + (hasta - desde) / 2` (division
entera):

```
   [1 2 3 4 5]
     desde=0 hasta=5   medio = 0 + 5/2 = 2   ->  3      (raiz)

   izquierda: [1 2]    desde=0 hasta=2   medio = 0 + 2/2 = 1   ->  2
                [1]    desde=0 hasta=1   medio = 0 + 1/2 = 0   ->  1

   derecha:   [4 5]    desde=3 hasta=5   medio = 3 + 2/2 = 4   ->  5
                [4]    desde=3 hasta=4   medio = 3 + 1/2 = 3   ->  4
```

Y el arbol que queda:

```
              3
            /   \
           2     5
          /     /
         1     4
```

- La raiz es **3** (el elemento del medio de los cinco).
- La rama izquierda es `[1, 2]`: su medio es **2**, y su unico hijo izquierdo
  es **1**.
- La rama derecha es `[4, 5]`: su medio es **5** (el medio *superior*, porque
  con dos elementos la division entera se va hacia arriba), y su unico hijo
  izquierdo es **4**.

La altura pasa de **5 a 3**, y el mismo conjunto que antes costaba 5
comparaciones para la clave 5 ahora cuesta **3**:

```
   clave buscada   camino (balanceado)      comparaciones
        1              3 2 1                     3
        2              3 2                       2
        3              3                         1
        4              3 5 4                     3
        5              3 5                       2
```

**El arbol que ve el profesor en pantalla es este ultimo**, no el degenerado:
el repositorio siempre rebalancea (despues de cada mutacion y al cargar el
archivo), asi que el arbol con el que trabaja la aplicacion esta siempre
balanceado, con altura O(log n).

### Como se verifico este bosquejo

Los tres dibujos **no** son un dibujo a mano: se compilaron y se ejecutaron
contra el `ArbolBST<int>` real del proyecto, con un programa temporal (fuera
del repo) que usa **solo la API publica** del TAD: `size()`, `altura()`,
`inOrder()` y `buscarPorClave()` con un comparador que registra el camino de
cada busqueda. Los caminos que se ven arriba son los que registro el
programa, y coinciden con lo que dice el algoritmo `construirDesde` de
`include/structures/ArbolBST.h:313`.

La salida del programa fue:

```
=== 1. ARBOL VACIO ===
   size=0 vacio=si altura=0
   inOrder:

=== 2. BST PLANO (insertar 1..5 en orden) ===
   insertar(1) -> true | size=1 altura=1
   insertar(2) -> true | size=2 altura=2
   insertar(3) -> true | size=3 altura=3
   insertar(4) -> true | size=4 altura=4
   insertar(5) -> true | size=5 altura=5
   inOrder: 1 2 3 4 5
   clave 1 -> camino 1  | comparaciones=1
   clave 2 -> camino 1 2  | comparaciones=2
   clave 3 -> camino 1 2 3  | comparaciones=3
   clave 4 -> camino 1 2 3 4  | comparaciones=4
   clave 5 -> camino 1 2 3 4 5  | comparaciones=5

=== 3. ARBOL BALANCEADO (construirDesdeOrdenados) ===
   size=5 altura=3
   inOrder: 1 2 3 4 5
   clave 1 -> camino 3 2 1  | comparaciones=3
   clave 2 -> camino 3 2  | comparaciones=2
   clave 3 -> camino 3  | comparaciones=1
   clave 4 -> camino 3 5 4  | comparaciones=3
   clave 5 -> camino 3 5  | comparaciones=2

=== 5. CONTADOR DE FUGAS ===
   nodosVivos()=0
```

### La evidencia

| Referencia | Que hay en ese lugar |
|---|---|
| `include/structures/ArbolBST.h:56` | `class ArbolBST {` — el TAD. |
| `include/structures/ArbolBST.h:15` | `// POR QUE PUNTEROS CRUDOS Y NO `std::unique_ptr` (decision S1)` |
| `include/structures/ArbolBST.h:21` | `// Con punteros crudos hay que escribir a mano las CINCO operaciones de la` |
| `include/structures/ArbolBST.h:147` | `// El nodo NO conoce al arbol: no tiene puntero al padre. Eso simplifica` |
| `include/structures/ArbolBST.h:149` | `struct Nodo {` |
| `include/structures/ArbolBST.h:150` | `T valor;` |
| `include/structures/ArbolBST.h:151` | `Nodo* izq;` |
| `include/structures/ArbolBST.h:152` | `Nodo* der;` |
| `include/structures/ArbolBST.h:153` | `explicit Nodo(const T& valor_) : valor(valor_), izq(nullptr), der(nullptr) {}` |
| `include/structures/ArbolBST.h:156` | `Nodo* raiz = nullptr;` |
| `include/structures/ArbolBST.h:157` | `std::size_t tam = 0;` |
| `include/structures/ArbolBST.h:232` | `if (valor_ < nodo->valor) {` — a la izquierda. |
| `include/structures/ArbolBST.h:234` | `} else if (nodo->valor < valor_) {` — a la derecha. |
| `include/structures/ArbolBST.h:237` | `duplicado = true;  // equivalentes: el valor ya estaba` |
| `include/structures/ArbolBST.h:248` | `static Nodo* eliminarEn(Nodo* nodo, const T& valor_, bool& eliminado) {` — los tres casos del borrado. |
| `include/structures/ArbolBST.h:300` | `static int alturaDe(const Nodo* nodo) {` |
| `include/structures/ArbolBST.h:306` | `return 1 + (alturaIzq > alturaDer ? alturaIzq : alturaDer);` |
| `include/structures/ArbolBST.h:311` | `// El elemento del MEDIO es la raiz y el resto se reparte en dos mitades:` |
| `include/structures/ArbolBST.h:313` | `static Nodo* construirDesde(const std::vector<T>& ordenados, std::size_t desde,` |
| `include/structures/ArbolBST.h:318` | `const std::size_t medio = desde + (hasta - desde) / 2;` |
| `include/structures/ArbolBST.h:319` | `Nodo* nodo = nuevo(ordenados[medio]);` |
| `include/structures/ArbolBST.h:464` | `raiz = construirDesde(ordenados, 0, ordenados.size());` |
| `include/structures/ArbolBST.h:144` | `static int nodosVivos() { return contador(); }` — el contador de fugas portable. |
| `include/structures/ArbolBST.h:114` | `std::size_t size() const { return tam; }  // O(1): es el contador, no un recorrido` |
| `tests/test_arbol.cpp:472` | `TEST_CASE("el arbol degenera al insertar en orden y se balancea al construir",` |
| `tests/test_arbol.cpp:487` | `CHECK(degenerado.altura() == cantidad);` |
| `tests/test_arbol.cpp:492` | `CHECK(balanceado.altura() <= 11);` |
| `tests/test_arbol.cpp:493` | `CHECK(balanceado.altura() * 100 <= degenerado.altura());` |

---

## Apendice: las decisiones del proyecto que sostienen estas respuestas

| Decision | Que aporta a esta defensa |
|---|---|
| 20 | El dominio no depende de FTXUI (verificado por el CI) |
| 26 | El contexto expone los repositorios concretos, no las interfaces |
| 27 | La logica de la UI es testeable sin FTXUI |
| 45 | La lista enlazada es el TAD propio del requisito 7 y la estructura de la persistencia |
| 46 | Punteros crudos en los dos TAD, con la Regla de Cinco a mano |
| 47 | El contador de fugas portable (`nodosVivos()`) |
| 49 | El TAD de estructuras no lanza excepciones de dominio |
| 1, 2, 3 | La especialidad es un `enum class` de doce valores, persistido por codigo |
| 15 | El turno tiene que ser de la especialidad del medico que lo atiende |
| 16 | El DNI del paciente y el legajo del medico son los identificadores naturales |
| 11, 13 | `ArchivoTexto<T>` en modo binario, con header versionado |
| 54 | El formato de archivo v3, y el loader entiende la v2 |

El detalle completo de cada una (decision, alternativa descartada y por que)
esta en [`docs/decisiones.md`](decisiones.md).
