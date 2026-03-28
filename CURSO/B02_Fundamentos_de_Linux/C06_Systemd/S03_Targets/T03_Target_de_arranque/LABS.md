# Labs — Target de arranque

## Descripcion

Laboratorio practico para entender como systemd decide que arrancar:
la cadena de decision (cmdline → default.target), el proceso de boot
paso a paso, systemd-analyze (blame, critical-chain, plot, verify),
optimizacion del boot, parametros del kernel, y debug-shell.service.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Cadena de decision | cmdline, default.target, proceso de boot |
| 2 | systemd-analyze | blame, critical-chain, verify, plot |
| 3 | Optimizacion y debug | Identificar cuellos, debug-shell, cmdline |

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
