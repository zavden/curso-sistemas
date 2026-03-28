# Labs — batch

## Descripcion

Laboratorio practico para entender batch: ejecucion condicionada a
la carga del sistema, load average, umbral de atd (-l), batch interval
(-b), creacion de tareas, comportamiento de la cola (uno por uno),
prioridad nice, diferencias con at, limitaciones, y alternativas
modernas (nice + ionice, systemd-run).

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Load average y umbral | uptime, /proc/loadavg, atd -l |
| 2 | Crear y gestionar tareas | batch, atq, cola "b", uno por uno |
| 3 | Limitaciones y alternativas | timeout, nice, ionice, systemd-run |

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
