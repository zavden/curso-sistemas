# Labs — #include y guardas

## Descripcion

Laboratorio para practicar las dos formas de `#include` (`< >` vs `" "`), las
rutas de busqueda con `-I`, include guards (`#ifndef`/`#define`/`#endif`),
`#pragma once`, y la inspeccion de la expansion del preprocesador con `gcc -E`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Inclusion doble sin guardas | Error de redefinicion al incluir un header dos veces |
| 2 | Include guards | `#ifndef`/`#define`/`#endif` previenen la inclusion doble, incluso transitiva |
| 3 | `#pragma once` | Alternativa no estandar que logra lo mismo con una sola linea |
| 4 | `#include < >` vs `" "` y `-I` | Diferencia en rutas de busqueda, uso de `-I` para directorios custom |
| 5 | `gcc -E` — inspeccion del preprocesador | Ver la expansion real, contar lineas, entender que hace `#include` |

## Archivos

```
labs/
├── README.md              ← Ejercicios paso a paso
├── point.h                ← Header CON include guard
├── point_noguard.h        ← Header SIN include guard (demuestra el problema)
├── point.c                ← Implementacion de point.h
├── shape.h                ← Header que incluye point.h (inclusion transitiva)
├── shape.c                ← Implementacion de shape.h
├── guards_ok.c            ← Incluye point.h directa e indirectamente (funciona)
├── noguard_fail.c         ← Incluye point_noguard.h dos veces (falla)
├── pragma_once_demo.h     ← Header con #pragma once
├── pragma_once_demo.c     ← Incluye pragma_once_demo.h dos veces (funciona)
├── search_path.c          ← Usa #include <greeting.h> (requiere -I)
├── preprocess_inspect.c   ← Para inspeccionar con gcc -E
└── myincludes/
    └── greeting.h         ← Header en directorio separado
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~20 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Ejecutables compilados (guards_ok, pragma_once_demo, search_path, preprocess_inspect) | Binarios | Si (limpieza final) |
