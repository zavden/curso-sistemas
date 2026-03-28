# Labs — Operaciones

## Descripcion

Laboratorio practico para dominar las operaciones de APT: el ciclo
de vida de un paquete (install, remove, purge, autoremove), la
diferencia entre upgrade y full-upgrade, y operaciones no interactivas
para scripts y Dockerfiles.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Ciclo de vida | install, remove vs purge, autoremove |
| 2 | Upgrades | upgrade vs full-upgrade, simulate |
| 3 | No interactivo | DEBIAN_FRONTEND, -y, -q, --no-install-recommends |

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
