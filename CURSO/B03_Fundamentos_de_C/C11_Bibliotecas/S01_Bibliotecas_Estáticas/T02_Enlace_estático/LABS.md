# Labs -- Enlace estatico

## Descripcion

Laboratorio para practicar el enlace estatico: como el linker resuelve
simbolos, por que el orden de `-l` importa, uso de `-L` para rutas de busqueda,
resolucion selectiva, `gcc -static` para enlace completamente estatico,
verificacion con `ldd` y `nm`, y diagnostico de errores comunes del linker.

## Prerequisitos

- GCC instalado (`gcc --version`)
- `ar`, `nm` y `ldd` disponibles
- Bibliotecas estaticas de libc instaladas para la parte 4 (`glibc-static` en
  Fedora, `libc-dev` en Debian/Ubuntu)
- Haber completado T01 (creacion de bibliotecas estaticas)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Preparar las bibliotecas | Compilar `.o`, crear `.a`, verificar simbolos con `nm`, mover a subdirectorio |
| 2 | Enlace correcto con -L y -l | Orden correcto de dependencias, ruta directa al `.a` como alternativa |
| 3 | El orden importa | Orden invertido causa `undefined reference`, `--trace` muestra resolucion selectiva |
| 4 | Enlace completamente estatico | `gcc -static`, comparar con `ldd`, diferencia de tamanos |
| 5 | Diagnostico de errores | `undefined reference`, `cannot find -l`, `multiple definition` |

## Archivos

```
labs/
├── README.md              <- Ejercicios paso a paso
├── format.h               <- API de formateo (format_name)
├── format.c               <- Implementacion de format_name
├── greet.h                <- API de saludo (greet_person)
├── greet.c                <- Implementacion que depende de format_name
├── main.c                 <- Programa que usa greet_person
├── greet_duplicate.c      <- Definicion duplicada para provocar error
├── calc.h                 <- API de calculadora (calc_add, calc_sub)
├── calc.c                 <- Implementacion de calc_add, calc_sub
└── main_calc.c            <- Programa que usa libcalc (enlace estatico completo)
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~30 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| `format.o`, `greet.o`, `main.o`, `greet_duplicate.o`, `calc.o`, `main_calc.o` | Archivos objeto | Si (limpieza final) |
| `lib/libformat.a`, `lib/libgreet.a`, `libcalc.a` | Bibliotecas estaticas | Si (limpieza final) |
| `greet_demo`, `greet_demo_direct`, `calc_dynamic`, `calc_static` | Ejecutables | Si (limpieza final) |
