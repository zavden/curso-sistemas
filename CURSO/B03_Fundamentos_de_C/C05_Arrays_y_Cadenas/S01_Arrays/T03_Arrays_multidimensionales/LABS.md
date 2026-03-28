# Labs — Arrays multidimensionales

## Descripcion

Laboratorio para explorar arrays 2D en C: declaracion, inicializacion, layout
en memoria (row-major order), paso a funciones con distintas firmas, y la
diferencia entre arrays de strings con `char *arr[]` vs `char arr[][]`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Arrays 2D | Declaracion, inicializacion completa y parcial, acceso con `matrix[i][j]` |
| 2 | Layout en memoria | Row-major order, direcciones contiguas, recorrido plano con puntero |
| 3 | Pasar array 2D a funciones | Cuatro firmas: `mat[][N]`, `(*mat)[N]`, VLA, puntero plano |
| 4 | Arrays de strings | `const char *arr[]` vs `char arr[][N]`: sizeof, mutabilidad, layout |

## Archivos

```
labs/
├── README.md          ← Ejercicios paso a paso
├── matrix_basics.c    ← Declaracion, inicializacion y acceso a arrays 2D
├── memory_layout.c    ← Direcciones en memoria y recorrido plano
├── pass_to_func.c     ← Cuatro formas de pasar un array 2D a funciones
└── string_arrays.c    ← Comparacion de char *arr[] vs char arr[][]
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
| Ejecutables compilados | Binarios | Si (limpieza final) |
