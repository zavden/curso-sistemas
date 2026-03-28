# Labs — /proc

## Descripcion

Laboratorio practico para explorar el filesystem virtual /proc:
inspeccionar procesos, leer informacion del sistema, y modificar
parametros del kernel con sysctl.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Procesos en /proc | /proc/[pid]/, status, cmdline, fd, exe |
| 2 | /proc/self | Autoreferencia, inspeccion propia |
| 3 | Informacion del sistema | cpuinfo, meminfo, uptime, version |
| 4 | /proc/sys y sysctl | Parametros del kernel en tiempo real |

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

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
