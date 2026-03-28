# Labs — Punteros a arrays vs arrays de punteros

## Descripcion

Laboratorio para distinguir `int *p[5]` de `int (*p)[5]`, observar las
diferencias de sizeof y aritmetica de punteros, usar arrays de punteros para
strings y punteros a arrays para matrices 2D, y simplificar declaraciones
complejas con typedef.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | `int *p[5]` vs `int (*p)[5]` | sizeof, precedencia de `[]` sobre `*`, aritmetica de punteros |
| 2 | Array de punteros | `char *names[]`, ragged arrays, strings de diferente longitud |
| 3 | Puntero a array | Recorrer matriz 2D con `int (*row)[COLS]`, paso de fila completa |
| 4 | Declaraciones complejas | Regla espiral, typedef para simplificar, punteros a funcion |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── sizeof_compare.c   ← Comparacion de sizeof entre ambas formas
├── array_of_ptrs.c    ← Array de punteros a int y a char (ragged array)
├── ptr_to_array_2d.c  ← Puntero a array para matrices 2D
└── complex_decl.c     ← Declaraciones complejas con typedef
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
