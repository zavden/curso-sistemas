# Labs — Repositorios

## Descripcion

Laboratorio practico para explorar la configuracion de repositorios
APT: formato one-line y DEB822, archivos en sources.list.d, claves
GPG (legacy vs Signed-By), apt-cache policy, y prioridades.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Configuracion de repos | sources.list, DEB822, sources.list.d |
| 2 | GPG y autenticacion | Keyrings, Signed-By, legacy vs moderno |
| 3 | Policy y prioridades | apt-cache policy, pinning, backports |

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
