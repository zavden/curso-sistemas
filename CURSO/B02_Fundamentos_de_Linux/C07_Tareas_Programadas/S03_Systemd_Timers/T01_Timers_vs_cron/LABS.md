# Labs — Timers vs cron

## Descripcion

Laboratorio practico para comparar systemd timers con cron: ventajas
(journal, dependencias, recursos, Persistent, precision, seguridad,
estado), desventajas (verbosidad, curva), sintaxis OnCalendar,
systemd-analyze calendar, AccuracySec, RandomizedDelaySec, y tabla
comparativa completa con cron y anacron.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Arquitectura y ventajas | timer+service, journal, dependencias, estado |
| 2 | OnCalendar y herramientas | Sintaxis, systemd-analyze calendar, precision |
| 3 | Comparacion completa | cron vs anacron vs timers, timers del sistema |

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
