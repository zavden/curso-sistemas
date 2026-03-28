# Labs — Configuración de logrotate

## Descripcion

Laboratorio practico para logrotate: que es y como se invoca
(systemd timer o cron.daily), /etc/logrotate.conf (defaults
globales), /etc/logrotate.d/ (configuraciones por aplicacion),
anatomia de un bloque de configuracion, patrones de archivo,
crear una configuracion para tu app, dry run (-d), forzar rotacion
(-f), estado en /var/lib/logrotate/status, flujo de rotacion paso
a paso, errores comunes, y diferencias Debian vs RHEL.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Config y estructura | logrotate.conf, logrotate.d/, invocacion |
| 2 | Crear y probar | Config custom, dry run, forzar, estado |
| 3 | Flujo, errores, distros | Rotacion paso a paso, errores, Debian vs RHEL |

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
