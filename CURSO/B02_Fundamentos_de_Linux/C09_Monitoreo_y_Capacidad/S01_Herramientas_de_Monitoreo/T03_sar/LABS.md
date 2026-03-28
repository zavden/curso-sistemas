# Labs — sar

## Descripcion

Laboratorio practico para sar (System Activity Reporter): paquete
sysstat (instalacion, habilitacion, timer/cron, datos en /var/log),
estadisticas en vivo (-u CPU, -r memoria, -S swap, -W actividad
swap, -b I/O, -d por dispositivo, -n DEV/EDEV/SOCK red, -q load,
-w context switches), analisis historico (rangos horarios, picos,
correlacion), sadf (JSON, CSV, SVG), y diferencias Debian vs RHEL.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Sysstat y config | Instalacion, habilitacion, datos, timer |
| 2 | Estadisticas en vivo | CPU, memoria, swap, disco, red, load |
| 3 | Historico y formatos | Rangos, picos, correlacion, sadf |

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
