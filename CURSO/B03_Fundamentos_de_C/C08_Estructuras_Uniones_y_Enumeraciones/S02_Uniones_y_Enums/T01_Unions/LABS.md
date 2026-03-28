# Labs -- Unions

## Descripcion

Laboratorio para explorar el funcionamiento de unions en C: memoria compartida,
tagged unions como patron seguro, type punning para reinterpretar bytes, y la
comparacion practica entre union y struct en ahorro de memoria.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Memoria compartida | Todos los miembros en offset 0, sizeof de union vs struct |
| 2 | Tagged union | Struct con enum tag + union, despacho seguro con switch |
| 3 | Type punning | Reinterpretar bytes de float como int, deteccion de endianness |
| 4 | Union vs struct | Cuando usar cada uno, ahorro de memoria con arrays grandes |

## Archivos

```
labs/
├── README.md          <- Ejercicios paso a paso
├── shared_memory.c    <- Programa para la parte 1: offsets y sizeof
├── tagged_union.c     <- Programa para la parte 2: tagged union con switch
├── type_punning.c     <- Programa para la parte 3: float bits y endianness
└── union_vs_struct.c  <- Programa para la parte 4: comparacion de tamanos
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
