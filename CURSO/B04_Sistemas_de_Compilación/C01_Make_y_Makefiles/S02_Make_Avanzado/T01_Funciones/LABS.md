# Labs -- Funciones de Make

## Descripcion

Laboratorio para dominar las funciones integradas de Make: descubrimiento de
archivos con $(wildcard), transformacion de paths con $(patsubst), filtrado de
listas con $(filter)/$(filter-out), iteracion con $(foreach), ejecucion de
comandos del sistema con $(shell), funciones reutilizables con $(call),
condicionales con $(if), y diagnostico con $(info)/$(warning)/$(error). Se
trabaja sobre un proyecto con multiples subdirectorios (src/core/, src/net/)
y se verifica con $(info) que produce cada funcion.

## Prerequisitos

- GCC instalado (`gcc --version`)
- GNU Make instalado (`make --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Estructura del proyecto | Crear la organizacion de subdirectorios src/core/, src/net/, include/ |
| 2 | wildcard | Descubrir archivos .c y .h automaticamente; observar que pasa con directorios inexistentes |
| 3 | patsubst | Transformar paths de src/*.c a build/*.o preservando subdirectorios |
| 4 | filter y filter-out | Separar fuentes por modulo, excluir archivos especificos de listas |
| 5 | foreach | Iterar sobre subdirectorios para recolectar fuentes y generar flags |
| 6 | shell | Obtener informacion del sistema (uname, date, conteo de archivos) |
| 7 | call | Definir funciones reutilizables con parametros $(1), $(2) |
| 8 | if | Seleccionar flags de compilacion segun variables |
| 9 | error, warning, info | Validar configuracion e imprimir diagnosticos |
| 10 | Proyecto completo | Integrar todas las funciones en un Makefile funcional que compila el proyecto |

## Archivos

```
labs/
├── README.md            <- Ejercicios paso a paso
├── main.c               <- Programa principal (incluye headers de core y net)
├── app.h                <- Header general de la aplicacion
├── engine.h             <- Header del modulo core/engine
├── engine.c             <- Implementacion core/engine
├── parser.h             <- Header del modulo core/parser
├── parser.c             <- Implementacion core/parser
├── client.h             <- Header del modulo net/client
├── client.c             <- Implementacion net/client
├── server.h             <- Header del modulo net/server
├── server.c             <- Implementacion net/server
├── Makefile.explore     <- Makefile exploratorio: imprime resultado de cada funcion
├── Makefile.validate    <- Makefile que demuestra error, warning e info
└── Makefile.project     <- Makefile completo que integra todas las funciones
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~40 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| src/ | Directorio de fuentes (organizado) | Si (limpieza final) |
| include/ | Directorio de headers (organizado) | Si (limpieza final) |
| build/ | Directorio de objetos compilados | Si (limpieza final) |
| funcapp | Ejecutable | Si (limpieza final) |
