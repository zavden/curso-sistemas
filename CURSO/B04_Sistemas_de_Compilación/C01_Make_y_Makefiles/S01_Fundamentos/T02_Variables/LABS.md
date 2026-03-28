# Labs -- Variables en Make

## Descripcion

Laboratorio para experimentar con los distintos tipos de asignacion de variables
en Make (`=`, `:=`, `?=`, `+=`), observar la diferencia entre expansion recursiva
e inmediata con `$(info)`, usar variables convencionales (CC, CFLAGS, LDFLAGS),
sobreescribir variables desde la linea de comandos, aplicar sustitucion de
patrones en variables, y conocer la directiva `override`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- GNU Make instalado (`make --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Expansion recursiva vs inmediata | Diferencia entre `=` y `:=` usando `$(info)`, efecto de redefinir variables |
| 2 | Asignacion condicional (?=) | Valores por defecto, sobreescritura desde linea de comandos y entorno, trampa con defaults integrados de Make |
| 3 | Append (+=) | Construccion incremental de flags, interaccion con ifdef DEBUG |
| 4 | Variables convencionales y compilacion real | CC, CFLAGS, LDFLAGS, LDLIBS en un Makefile que compila un proyecto de dos archivos |
| 5 | Sobreescritura desde la linea de comandos y override | Prioridad de variables, efecto de `make CC=clang`, directiva `override` |
| 6 | Sustitucion en variables | `$(SRCS:.c=.o)`, sustitucion solo al final de cada palabra, variables automaticas (mencion) |

## Archivos

```
labs/
├── README.md              <- Ejercicios paso a paso
├── hello.c                <- Programa simple para pruebas de compilacion
├── main.c                 <- Programa principal (usa utils)
├── utils.c                <- Modulo auxiliar con greet() y add()
├── utils.h                <- Header de utils
├── Makefile.lazy           <- Demuestra expansion recursiva (=)
├── Makefile.immediate      <- Demuestra expansion inmediata (:=)
├── Makefile.compare        <- Comparacion directa = vs :=
├── Makefile.conditional    <- Demuestra ?= (asignacion condicional)
├── Makefile.append         <- Demuestra += (append) con ifdef
├── Makefile.conventional   <- Variables convencionales, compila proyecto real
├── Makefile.override       <- Demuestra override y prioridad de variables
└── Makefile.substitution   <- Demuestra sustitucion $(VAR:.c=.o)
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
| hello | Ejecutable | Si (limpieza final) |
| myapp | Ejecutable | Si (limpieza final) |
| main.o, utils.o | Archivos objeto | Si (limpieza final) |
