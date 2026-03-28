# Labs — Cambiar target

## Descripcion

Laboratorio practico para gestionar targets de systemd: get-default
y set-default para el target de arranque, isolate para cambio
inmediato, AllowIsolate, acciones del sistema (poweroff/reboot/
suspend), cambio desde GRUB, targets custom, y equivalencias
SysVinit.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Default target | get-default, set-default, symlink |
| 2 | Isolate y acciones | isolate, AllowIsolate, shutdown |
| 3 | GRUB y targets custom | kernel cmdline, targets personalizados |

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
