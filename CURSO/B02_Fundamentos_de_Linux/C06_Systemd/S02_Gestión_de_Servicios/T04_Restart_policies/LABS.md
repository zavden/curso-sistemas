# Labs — Restart policies

## Descripcion

Laboratorio practico para dominar las politicas de reinicio de
systemd: Restart= (no, always, on-failure, on-abnormal, on-abort,
on-watchdog), RestartSec, StartLimitBurst/IntervalSec, timeouts
(start/stop), KillMode, WatchdogSec, y patrones profesionales.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Restart y RestartSec | Valores de Restart=, condiciones, RestartSec |
| 2 | Start limits y timeouts | StartLimitBurst, TimeoutSec, KillMode |
| 3 | Watchdog y patrones | WatchdogSec, servicio de produccion |

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

~20 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
