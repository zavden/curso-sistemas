# Labs — crontab vs /etc/cron.d

## Descripcion

Laboratorio practico para entender los dos scopes de cron: crontabs
de usuario (crontab -e, sin campo usuario) vs crontab del sistema
(/etc/crontab, /etc/cron.d/ con campo usuario), directorios
periodicos (/etc/cron.{hourly,daily,weekly,monthly}), run-parts,
la relacion con anacron, y como auditar todas las tareas cron.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Scopes de cron | crontab usuario vs /etc/crontab vs /etc/cron.d/ |
| 2 | Directorios periodicos | cron.daily/weekly/monthly, run-parts, reglas |
| 3 | Auditoria y diferencias | Listar todas las tareas, Debian vs RHEL |

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
