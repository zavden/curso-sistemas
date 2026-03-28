# Labs — Zonas horarias

## Descripcion

Laboratorio practico para zonas horarias: como Linux maneja zonas
(/etc/localtime como symlink a /usr/share/zoneinfo/), ver la zona
actual (timedatectl, date +%Z/%z, /etc/timezone), cambiar zona
(timedatectl set-timezone, ln -sf), variable TZ (por proceso, por
sesion, en servicios), horario de verano (DST, zdump, implicaciones
en logs y cron), servidores en UTC, RTC en UTC vs hora local, y
conversion entre zonas en scripts.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Zona actual y archivos | /etc/localtime, zoneinfo, /etc/timezone |
| 2 | TZ y DST | Variable TZ, zdump, horario de verano |
| 3 | Servidores y scripts | UTC, RTC, conversion, Debian vs RHEL |

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
