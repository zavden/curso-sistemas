# Labs — const y punteros

## Descripcion

Laboratorio para practicar las 4 combinaciones de `const` con punteros,
aplicar la regla de lectura derecha a izquierda en declaraciones, y disenar
APIs con const correctness. Se enfoca exclusivamente en `const` aplicado a
punteros, no en `const` para variables simples.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Las 4 combinaciones | `int *`, `const int *`, `int *const`, `const int *const` en accion |
| 2 | Regla de lectura derecha a izquierda | Interpretar declaraciones complejas y verificar con el compilador |
| 3 | const correctness en APIs | Parametros const, retorno const, cast away const, UB |

## Archivos

```
labs/
├── README.md            ← Ejercicios paso a paso
├── four_combos.c        ← Las 4 combinaciones en accion
├── const_errors.c       ← Errores de compilacion intencionales
├── read_right_to_left.c ← Regla de lectura derecha a izquierda
├── const_api.c          ← const correctness en funciones
└── cast_away.c          ← Cast away const y UB
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
| Ejecutables compilados | Binarios | Si (limpieza final) |
