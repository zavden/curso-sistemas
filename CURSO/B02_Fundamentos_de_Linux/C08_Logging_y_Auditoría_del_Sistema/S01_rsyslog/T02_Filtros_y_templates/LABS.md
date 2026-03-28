# Labs — Filtros y templates

## Descripcion

Laboratorio practico para filtros avanzados y templates en rsyslog:
filtros basados en propiedades (:propiedad, operador, "valor"),
operadores (isequal, contains, regex), la accion stop, RainerScript
(if/then/else, operadores logicos), templates predefinidos y
personalizados, templates con archivo dinamico (dynaFile), templates
JSON, y el orden de procesamiento de reglas.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Filtros de propiedades | :propiedad, operadores, stop |
| 2 | RainerScript | if/then/else, operadores, variables |
| 3 | Templates | Predefinidos, custom, dynaFile, JSON, orden |

## Archivos

```
labs/
└── README.md    <- Guia paso a paso (documento principal del lab)
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~20 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
