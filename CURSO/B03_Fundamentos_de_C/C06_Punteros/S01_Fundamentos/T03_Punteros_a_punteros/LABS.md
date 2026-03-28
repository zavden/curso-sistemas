# Labs -- Punteros a punteros

## Descripcion

Laboratorio para practicar la doble indireccion (`int **`), entender por que
se necesita para modificar punteros desde funciones, explorar `char **argv`, y
construir un array 2D dinamico con filas de diferente tamano (jagged array).

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Doble indireccion | `int **pp`, desreferenciar dos veces, direcciones, sizeof |
| 2 | Modificar puntero desde funcion | `alloc_wrong` vs `alloc_correct` con `int **` |
| 3 | argv | `char **argv`, recorrer argumentos, terminador NULL |
| 4 | Array dinamico 2D | `malloc` con `int **`, filas de diferente tamano, liberacion |

## Archivos

```
labs/
├── README.md             <- Ejercicios paso a paso
├── double_indirection.c  <- Programa para la parte 1
├── alloc_wrong.c         <- Programa para la parte 2
├── argv_explore.c        <- Programa para la parte 3
└── jagged_array.c        <- Programa para la parte 4
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
