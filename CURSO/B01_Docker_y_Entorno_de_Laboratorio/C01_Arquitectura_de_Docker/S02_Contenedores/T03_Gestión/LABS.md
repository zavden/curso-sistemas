# Labs — Gestion

## Descripcion

Laboratorio practico con 4 partes progresivas que verifican de forma empirica
las herramientas de gestion de contenedores: `docker ps` con filtros y formato,
`docker inspect`, `docker logs`, `docker stats`, `docker cp`, `docker rename`,
y `docker top`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas:

```bash
docker pull debian:bookworm
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Listado y filtrado | `docker ps` con `-a`, `-q`, `--filter`, `--format`, `-s` |
| 2 | Inspeccion profunda | `docker inspect` para extraer configuracion, estado, red, y mounts |
| 3 | Logs y procesos | `docker logs` con --tail, --since, -f; `docker top` para ver procesos |
| 4 | Copia de archivos y monitoreo | `docker cp` entre host y contenedor, `docker rename`, `docker stats` |

## Archivos

```
labs/
├── README.md              <- Guia paso a paso (documento principal del lab)
└── Dockerfile.log-demo    <- Imagen que genera logs con timestamps para practicar docker logs
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el analisis de
resultados.

## Tiempo estimado

~25 minutos (las 4 partes completas).

## Recursos Docker creados

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-log-demo` | imagen | 3 | Si |
| `ps-run` | contenedor | 1 | Si |
| `ps-stop` | contenedor | 1 | Si |
| `ps-label` | contenedor | 1 | Si |
| `inspect-demo` | contenedor | 2 | Si |
| `log-source` | contenedor | 3 | Si |
| `top-demo` | contenedor | 3 | Si |
| `cp-demo` | contenedor | 4 | Si |
| `stats-busy` | contenedor | 4 | Si |
| `stats-idle` | contenedor | 4 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
