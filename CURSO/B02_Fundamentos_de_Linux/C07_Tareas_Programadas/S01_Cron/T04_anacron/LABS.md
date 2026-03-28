# Labs — anacron

## Descripcion

Laboratorio practico para entender anacron: el problema que resuelve
(tareas perdidas por apagado), /etc/anacrontab (periodo, delay,
identificador, comando), timestamps en /var/spool/anacron/,
START_HOURS_RANGE, RANDOM_DELAY, relacion con cron, diferencias
Debian vs RHEL, limitaciones, y comparacion con systemd timers.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Funcionamiento de anacron | Problema, timestamps, /etc/anacrontab |
| 2 | Configuracion y relacion con cron | Variables, invocacion, cron vs anacron |
| 3 | Limitaciones y alternativas | Solo root, solo dias, systemd timers |

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
