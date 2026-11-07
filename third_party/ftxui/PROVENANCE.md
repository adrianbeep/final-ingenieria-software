# FTXUI vendorizada

Este directorio contiene el codigo fuente de **FTXUI** (Functional Terminal (X) User
interface), la libreria que va a implementar la TUI del sistema de turnos.

| Campo | Valor |
|---|---|
| Libreria | FTXUI (Functional Terminal (X) User interface) |
| Version | v7.0.3 |
| Fuente | https://github.com/ArthurSonzogni/FTXUI |
| Distribucion | amalgamada (`ftxui-amalgamated.zip`) |
| SHA256 del .zip | `63d13bb65747bdbf0edf04d83ce490708022371cf378f15fad8489249ea4fd9a` |
| Licencia | MIT (ver `LICENSE`) |
| Copyright | Copyright (c) 2019 Arthur Sonzogni |
| Fecha de descarga | 2026-11-03 |

## Por que se vendoriza y no se instala ni se descarga en el build

La consigna tiene un criterio de aceptacion que no admite matices: el proyecto tiene
que funcionar **en una maquina limpia**. Eso significa que `git clone` + `make` tiene
que andar sin internet, sin permisos de administrador y sin instalar nada.

Hay tres caminos posibles para obtener FTXUI, y solo uno cumple:

1. **Vendorizar el codigo en el repo** (lo que hace este directorio).
2. **Instalar FTXUI como paquete del sistema** (`apt install libftxui-dev`,
   `brew install ftxui`, `vcpkg install ftxui`). Rompe el criterio de aceptacion: la
   maquina del profesor no tiene el paquete, y el primer `make` falla. Ademas cada
   sistema tiene su version, asi que dos maquinas distintas compilan contra FTXUI
   distintas y los bugs dejan de ser reproducibles.
3. **Descargar el release en el build** (un `curl` o `gh release download` en el
   `Makefile`). Tambien rompe el criterio de aceptacion: sin internet el build
   muere, y ademas el build deja de ser una funcion pura del contenido del repo.
   Un `make` que depende de la red es un `make` que puede fallar por causas que no
   son del proyecto.

Vendorizar deja el estado del build **descrito por el repositorio**: la version esta
congelada, el hash esauditable y el build es reproducible en cualquier parte. Por eso
estos archivos estan en el repo y no en un `.gitignore`.

La alternativa descartada de la release amalgamada es la variante header-only
(`ftxui_all.hpp`): la variante split (`ftxui.hpp` + `ftxui.cpp`) es mas chica en
total y separa la declaracion de la implementacion, que es lo que necesitan tanto
CMake como el Makefile de fallback.

## Como se verifica la integridad

El zip se descarga del release y se verifica **antes** de descomprimirlo:

```bash
gh release download v7.0.3 --repo ArthurSonzogni/FTXUI -p ftxui-amalgamated.zip -D <dir>
Get-FileHash <dir>/ftxui-amalgamated.zip -Algorithm SHA256
```

El hash tiene que dar exactamente:

```
63D13BB65747BDBF0EDF04D83CE490708022371CF378F15FAD8489249EA4FD9A
```

Ese mismo valor es el `digest` que GitHub publica para el asset del release, asi que
la verificacion no depende de que este documento sea creible: se puede contrastar
contra la API de GitHub y contra el archivo descargado.

Despues de descomprimir, los tres archivos que se copian al repo tienen que seguir
con estos SHA256:

| Archivo | SHA256 |
|---|---|
| `ftxui.hpp` | `063a53c13e8ab89bbf6f533a4e28f91566279b3744d5b59110fb200f2b3a6049` |
| `ftxui.cpp` | `56cde550d3a38c5ce6763b4acafe9e7b548719ed48ec15d71830d3cc2ad3d165` |
| `LICENSE` | `0224d97639ad37047d719a5ed5c0acc98de1c58ae63be0c27183dcad1328fc61` |

Para recalcularlos:

```bash
Get-FileHash third_party/ftxui/ftxui.hpp -Algorithm SHA256
Get-FileHash third_party/ftxui/ftxui.cpp -Algorithm SHA256
Get-FileHash third_party/ftxui/LICENSE  -Algorithm SHA256
```

## Que NO se hace aqui

- **El codigo no se modifica.** Los tres archivos son el release **byte a byte**.
  Ningun `#include`, ninguna macro, ningun fix. Si FTXUI necesita un arreglo, el
  arreglo se hace en el PR que actualiza la version, no en el que la vendoriza.
  Por eso `.gitattributes` marca `third_party/ftxui/*` con `-text`: sin conversion
  de fin de linea, el archivo del repo es identico al del release.
- **La version no se actualiza sola.** No hay `curl` que baje la ultima version ni
  dependencia flotante. Cambiar de version es un PR propio, con su hash nuevo y su
  verificacion completa: si el release se actualizara por su cuenta, el build del
  proyecto dejaria de ser reproducible sin que nadie lo decide.
- **No se agregan parches locales.** Si hiciera falta un parche, el diff tiene que
  quedar escrito y justificado en el PR, con el hash del upstream al que se aplica.
  Parchear en silencio es como no auditar: el proximo que lea el codigo no va a
  poder saber que parte es de FTXUI y que parte es nuestra.

## Nota sobre la capa de dominio

FTXUI es un detalle de la **presentacion**. El dominio no la conoce y no la puede
conocer: es la condicion para que las reglas de negocio se puedan probar sin
terminal. El CI lo verifica con un `grep` que falla el build si `ftxui` aparece
dentro de `include/domain`, `include/entities`, `include/utils`,
`include/repositories`, `include/services`, `include/persistence`,
`include/structures` o `include/exceptions`.
