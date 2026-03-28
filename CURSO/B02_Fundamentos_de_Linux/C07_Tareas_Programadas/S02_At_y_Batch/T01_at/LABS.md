# Labs — at

## Descripcion

Laboratorio practico para dominar el comando at: ejecucion unica
programada, sintaxis de tiempo (hora, fecha, expresiones relativas),
crear tareas (interactivo, pipe, heredoc, archivo), atq/atrm,
ver contenido con at -c, colas, control de acceso (at.allow/deny),
captura de entorno, y casos de uso.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Sintaxis y creacion | Formatos de tiempo, pipe, heredoc, -f |
| 2 | Gestion de tareas | atq, at -c, atrm, colas |
| 3 | Entorno y control | Captura de entorno, at.allow/deny, casos de uso |

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

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
