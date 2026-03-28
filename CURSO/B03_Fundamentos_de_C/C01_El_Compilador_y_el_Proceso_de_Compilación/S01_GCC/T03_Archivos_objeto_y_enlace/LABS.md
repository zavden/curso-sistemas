# Labs -- Archivos objeto y enlace

## Descripcion

Laboratorio para entender archivos objeto (.o), tablas de simbolos, visibilidad
de simbolos (extern vs static), y los dos modelos de enlace: bibliotecas
estaticas (.a) y bibliotecas dinamicas (.so). Se provocan errores clasicos de
enlace y se compara el comportamiento de cada modelo.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas binutils: `nm`, `ar`, `ldd`, `file` (incluidas con GCC)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Simbolos en archivos objeto | Visibilidad extern vs static con nm (T/t/D/d/U) |
| 2 | Compilacion multi-archivo y enlace | Compilar .o por separado, enlazar, errores clasicos |
| 3 | Biblioteca estatica | Crear y usar libcalc.a con ar, efecto del orden de enlace |
| 4 | Biblioteca dinamica | Crear y usar libcalc.so, LD_LIBRARY_PATH, comparar con estatica |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── calc.h           <- Header con prototipos: add, sub, mul
├── add.c            <- Implementacion de add
├── sub.c            <- Implementacion de sub
├── mul.c            <- Implementacion de mul
├── main_calc.c      <- Programa principal que usa las tres funciones
└── visibility.c     <- Programa con simbolos extern y static
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
| *.o | Archivos objeto | Si (limpieza por parte y final) |
| libcalc.a | Biblioteca estatica | Si (limpieza parte 3 y final) |
| libcalc.so | Biblioteca dinamica | Si (limpieza parte 4 y final) |
| Ejecutables compilados | Binarios | Si (limpieza por parte y final) |
