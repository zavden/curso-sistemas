# Labs — NTP / Chrony

## Descripcion

Laboratorio practico para NTP y Chrony: dos relojes del sistema
(RTC hardware y system clock), protocolo NTP (stratum, pools),
tres implementaciones (chrony, ntpd, systemd-timesyncd),
timedatectl, configuracion de chrony (chrony.conf, iburst,
makestep, driftfile), chronyc (sources, sourcestats, tracking),
systemd-timesyncd (timesyncd.conf), drift y correccion (slew vs
step), NTS, y diferencias Debian vs RHEL.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Relojes y NTP | RTC, system clock, stratum, pools |
| 2 | Chrony y timesyncd | Config, sources, tracking, drift |
| 3 | Comparacion | chrony vs timesyncd, Debian vs RHEL, NTS |

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
