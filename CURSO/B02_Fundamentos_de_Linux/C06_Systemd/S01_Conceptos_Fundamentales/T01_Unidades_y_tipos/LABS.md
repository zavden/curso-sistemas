# Labs — Unidades y tipos

## Descripcion

Laboratorio practico para explorar las unidades de systemd: listar
y filtrar unidades por tipo (service, socket, timer, target, mount,
slice, path, swap), inspeccionar su estructura, y entender las
relaciones entre unidades (targets agrupan, sockets/timers activan,
slices controlan recursos).

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Explorar unidades | list-unit-files, filtrar por tipo, contar |
| 2 | Inspeccionar unidades | Leer archivos .service/.socket/.timer, secciones |
| 3 | Relaciones entre tipos | Targets como agrupadores, socket→service, timer→service |

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
