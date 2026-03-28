# Labs — journalctl

## Descripcion

Laboratorio practico para dominar journalctl: filtros basicos
(por unidad, tiempo, prioridad, boot, PID/UID), combinacion de
filtros, opciones de salida (follow, formatos, campos), filtros
avanzados con campos del journal, uso en scripts, y gestion de
espacio en disco.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Filtros basicos | -u, --since/until, -p, -b, _PID |
| 2 | Salida y formatos | -f, -n, -o short/verbose/json/cat |
| 3 | Campos y scripts | -F, _COMM, -o json + jq, disk-usage |

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

~25 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
