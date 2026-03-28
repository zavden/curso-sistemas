# Labs — Variables de entorno en cron

## Descripcion

Laboratorio practico para entender el entorno reducido de cron:
PATH minimo, SHELL=/bin/sh, MAILTO, HOME, errores comunes por
entorno (comando no encontrado, bashisms, variables no expandidas,
locale), tecnicas de depuracion (capturar entorno, reproducir
manualmente), y buenas practicas.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Entorno de cron | PATH, SHELL, MAILTO, HOME, diferencias |
| 2 | Errores comunes | Comando no encontrado, bashisms, %, locale |
| 3 | Depuracion y buenas practicas | Capturar entorno, reproducir, wrapper |

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
