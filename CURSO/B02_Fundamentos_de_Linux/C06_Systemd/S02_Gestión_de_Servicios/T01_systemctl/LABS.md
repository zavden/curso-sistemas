# Labs — systemctl

## Descripcion

Laboratorio practico para dominar systemctl: operaciones basicas
(start/stop/restart/reload), status e inspeccion detallada,
enable/disable y arranque al boot, mask/unmask, listar y filtrar
unidades, show para propiedades, cat/edit para archivos,
daemon-reload, y operaciones en scripts.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Inspeccion de servicios | status, show, cat, propiedades |
| 2 | Listar y filtrar | list-units, list-unit-files, --type, --state |
| 3 | Operaciones y scripts | enable/disable, mask, is-active en scripts |

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
