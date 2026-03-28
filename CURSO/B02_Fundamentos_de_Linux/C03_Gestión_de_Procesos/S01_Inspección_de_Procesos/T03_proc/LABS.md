# Labs — /proc/[pid]

## Descripcion

Laboratorio practico para explorar el filesystem /proc: inspeccionar
informacion de procesos a traves de archivos virtuales como status,
cmdline, fd, maps, limits, y ns.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Archivos basicos de /proc/[pid] | status, cmdline, comm, environ |
| 2 | File descriptors y memoria | fd/, maps, limits |
| 3 | Enlaces y metadatos | exe, cwd, root, ns/ |

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
