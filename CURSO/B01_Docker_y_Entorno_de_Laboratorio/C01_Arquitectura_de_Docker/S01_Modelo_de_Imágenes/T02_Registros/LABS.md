# Labs -- Registros

## Descripcion

Laboratorio practico con 5 partes progresivas que verifican de forma empirica
los conceptos cubiertos en este topico: nomenclatura de imagenes, pull por tag
vs digest, la trampa de `latest`, mutabilidad de tags, rate limits de Docker
Hub, y manifiestos multi-platform.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Acceso a Internet (para interactuar con Docker Hub)
- Imagen base descargada:

```bash
docker pull alpine:3.20
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Nomenclatura de Docker Hub | Resolver nombres cortos a paths completos, el namespace `library/`, pull con nombre explicito |
| 2 | Tags y la trampa de `latest` | `latest` es solo un tag convencional, multiples tags pueden apuntar a la misma imagen, tags son mutables |
| 3 | Digests e inmutabilidad | Pull por digest, verificar que digest es inmutable, comparar digest con tag |
| 4 | Rate limits de Docker Hub | Consultar limites restantes via API, diferencia entre anonimo y autenticado |
| 5 | Manifiestos multi-platform | Inspeccionar manifest lists, ver plataformas disponibles, pull forzando plataforma |

## Archivos

```
labs/
├── README.md              <- Guia paso a paso (documento principal del lab)
├── Dockerfile.tag-test    <- Imagen simple para demostrar mutabilidad de tags
└── show-platform.sh       <- Script que muestra la plataforma del contenedor
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el analisis de
resultados.

## Tiempo estimado

~25 minutos (las 5 partes completas).

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `alpine:3.20` | Imagen (pull) | 1 | Si |
| `alpine:latest` | Imagen (pull) | 2 | Si |
| `alpine:3.19` | Imagen (pull) | 2 | Si |
| `lab-tag-v1` | Imagen (build) | 2 | Si |
| `lab-tag-v2` | Imagen (build) | 2 | Si |
| `alpine@sha256:...` | Imagen (pull) | 3 | Si |

Ninguna imagen persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
