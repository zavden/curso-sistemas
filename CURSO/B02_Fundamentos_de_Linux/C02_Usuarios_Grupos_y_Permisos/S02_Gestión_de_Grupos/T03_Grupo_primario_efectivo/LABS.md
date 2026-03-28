# Labs — Grupo primario efectivo

## Descripcion

Laboratorio practico para entender el grupo primario efectivo:
como afecta la creacion de archivos, como cambiarlo con newgrp y
sg, y como interactua con el SGID en directorios.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Observar el grupo efectivo | id -gn, /proc/self/status, cuatro GIDs |
| 2 | newgrp y sg | Cambiar grupo temporal, subshell, sg para un comando |
| 3 | SGID vs grupo efectivo | Que tiene prioridad, directorio compartido |

## Archivos

```
labs/
└── README.md    ← Guia paso a paso (documento principal del lab)
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
