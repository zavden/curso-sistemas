# Labs — Clases de almacenamiento

## Descripcion

Laboratorio para observar el comportamiento de las storage class specifiers de C:
`auto`, `static`, `extern` y `register`. Se demuestra la persistencia de variables
estaticas, el enlace de variables externas entre archivos, y la ubicacion de cada
tipo de variable en los segmentos de memoria (.data, .bss, stack).

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas binutils: `nm`, `objdump`, `size` (incluidas con GCC)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | auto vs static | Variable local estatica que persiste entre llamadas vs variable auto que se reinicializa |
| 2 | extern | Compartir variables entre archivos, declaracion vs definicion, errores de enlace |
| 3 | register | Hint al compilador, comparar assembly con y sin optimizacion |
| 4 | Donde vive cada variable | Inspeccionar segmentos con nm, objdump y size (.data, .bss, stack) |

## Archivos

```
labs/
├── README.md          <- Ejercicios paso a paso
├── auto_vs_static.c   <- Parte 1: comparacion auto vs static local
├── static_init.c      <- Parte 1: valores por defecto de variables static
├── config.h           <- Parte 2: header con declaraciones extern
├── config.c           <- Parte 2: definiciones de variables globales
├── main_extern.c      <- Parte 2: programa que usa las variables externas
├── register_demo.c    <- Parte 3: funciones con y sin register
└── storage_map.c      <- Parte 4: programa con variables en distintos segmentos
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
| *.o | Archivos objeto | Si (limpieza final) |
| *.s | Archivos assembly | Si (limpieza final) |
| Ejecutables compilados | Binarios | Si (limpieza final) |
