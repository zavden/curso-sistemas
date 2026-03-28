# Labs — Arrays y punteros

## Descripcion

Laboratorio para explorar la relacion entre arrays y punteros en C: el decay
automatico, las diferencias fundamentales entre ambos, la iteracion con
punteros, y la distincion entre `arr` y `&arr`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Decay | `arr` decae a `&arr[0]`, `arr[i]` es `*(arr+i)`, aritmetica de punteros |
| 2 | Diferencias array vs puntero | `sizeof(arr)` vs `sizeof(ptr)`, `&arr` vs `&arr[0]`, sizeof en funciones |
| 3 | Iteracion con punteros | Recorrer un array con indice, puntero incremental, aritmetica y sentinel |
| 4 | `arr` vs `&arr` | Tipo `int *` vs `int (*)[5]`, diferencia de tamano de paso al sumar 1 |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── decay.c          <- Programa para la parte 1 (decay y aritmetica)
├── differences.c    <- Programa para la parte 2 (sizeof y direcciones)
├── iteration.c      <- Programa para la parte 3 (4 formas de recorrer)
└── arr_vs_addr.c    <- Programa para la parte 4 (puntero a int vs puntero a array)
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
