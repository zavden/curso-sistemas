# Labs — Directivas de logrotate

## Descripcion

Laboratorio practico para las directivas de logrotate: frecuencia
(daily, weekly, size, minsize, maxsize), retencion (rotate, maxage),
compresion (compress, delaycompress, algoritmos alternativos),
creacion (create, copytruncate, copy), nombrado (dateext,
dateformat), control (missingok, notifempty), scripts (prerotate,
postrotate, firstaction, lastaction, sharedscripts), directivas
avanzadas (su, tabooext), ejemplo de produccion, y diagnostico.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Frecuencia, retencion, compresion | daily, size, rotate, compress |
| 2 | Creacion, nombrado, control | create, copytruncate, dateext, missingok |
| 3 | Scripts y avanzadas | postrotate, sharedscripts, su, produccion |

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
