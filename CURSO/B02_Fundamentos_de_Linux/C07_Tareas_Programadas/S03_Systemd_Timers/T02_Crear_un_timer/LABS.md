# Labs — Crear un timer

## Descripcion

Laboratorio practico para crear systemd timers: estructura minima
(service oneshot + timer), relacion timer-service, activacion,
Persistent=true, ejemplos completos (backup, limpieza, health check),
timers transient con systemd-run, user timers, gestion
(enable/disable/start/stop), y migracion desde cron.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Estructura y activacion | .service + .timer, enable, verify |
| 2 | Persistent y ejemplos | Tareas perdidas, backup, limpieza |
| 3 | Transient, user timers, migracion | systemd-run, --user, cron→timer |

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
