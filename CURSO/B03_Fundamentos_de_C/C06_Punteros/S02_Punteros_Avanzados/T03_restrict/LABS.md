# Labs — restrict

## Descripcion

Laboratorio para entender en profundidad el calificador `restrict`: por que
existe (el problema de aliasing), como cambia el assembly generado, que pasa
cuando se viola la promesa, y la relacion entre `memcpy` y `memmove`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas: `objdump` (incluida con GCC/binutils)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | El problema del aliasing | Por que el compilador no puede optimizar sin restrict |
| 2 | restrict en accion | Comparar assembly con/sin restrict usando -O2 |
| 3 | Violar restrict | Que pasa cuando los punteros se solapan (UB con -O0 vs -O2) |
| 4 | restrict en la stdlib | memcpy vs memmove: por que memcpy tiene restrict |

## Archivos

```
labs/
├── README.md             ← Ejercicios paso a paso
├── aliasing.c            ← Dos funciones de suma: con y sin restrict
├── aliasing_problem.c    ← Por que el aliasing impide la optimizacion
├── restrict_violation.c  ← Violacion de restrict: UB observable
└── memcpy_vs_memmove.c   ← Comparar memcpy (restrict) vs memmove (sin restrict)
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~25 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| *.s | Assembly generado | Si (limpieza por parte y final) |
| Ejecutables compilados | Binarios | Si (limpieza por parte y final) |
