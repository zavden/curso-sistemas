# Labs — Monotonic vs realtime timers

## Descripcion

Laboratorio practico para distinguir los dos tipos de timer en systemd:
realtime (OnCalendar, basado en reloj del sistema) y monotonic
(OnBootSec, OnStartupSec, OnActiveSec, OnUnitActiveSec,
OnUnitInactiveSec, basados en tiempo transcurrido), combinacion de
directivas (OR), unidades de tiempo, reloj de pared vs monotonico,
ejemplos practicos y criterios para elegir entre ambos.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Directivas y relojes | OnCalendar vs On*Sec, wall clock vs monotonic |
| 2 | Combinaciones y ejemplos | OR entre directivas, time units, patrones |
| 3 | Elegir y analizar | Criterios de eleccion, timers del sistema, calculos |

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
