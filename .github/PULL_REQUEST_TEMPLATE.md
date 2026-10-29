## Linked Issue

<!-- REQUERIDO. Debe referenciar un issue que tenga la label `status:approved`.
     Keywords validos: Closes #N | Fixes #N | Resolves #N -->

Closes #

## Tipo de PR

<!-- Marcar EXACTAMENTE UNO. La label type:* correspondiente se agrega al PR. -->

- [ ] Bug fix (`type:bug`)
- [ ] Nueva funcionalidad (`type:feature`)
- [ ] Solo documentacion (`type:docs`)
- [ ] Refactoring (`type:refactor`)
- [ ] Mantenimiento / tooling (`type:chore`)
- [ ] Cambio incompatible (`type:breaking-change`)

## Resumen

<!-- 1 a 3 bullets: que cambia y por que. -->

## Cambios

| Archivo | Cambio |
|---|---|
| `path/al/archivo` | Que hace |

## Plan de pruebas

<!-- Marcar lo que corresponda. Si algo no aplica, marcarlo y explicar. -->

- [ ] Compila limpio (`-Wall -Wextra -Wpedantic`, sin warnings)
- [ ] Los tests pasan (`ctest` / `make test`)
- [ ] Se probó manualmente el flujo afectado
- [ ] Se verificó en una máquina limpia (clone + build desde cero)

## Checklist

- [ ] El issue referenciado tiene la label `status:approved`
- [ ] El PR tiene exactamente una label `type:*`
- [ ] Los commits siguen el formato conventional commits
- [ ] Los tests van en el mismo commit que el código que verifican
- [ ] La documentación se actualizo si cambio el comportamiento
- [ ] Sin trailers `Co-Authored-By` ni atribucion de IA
