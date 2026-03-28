# Labs — Ciclo de vida

## Descripcion

Laboratorio practico con 4 partes progresivas que verifican de forma empirica
los estados de un contenedor Docker: created, running, paused, exited, y las
transiciones entre ellos mediante `docker create`, `start`, `pause`, `stop`,
`inspect` y filtros de `docker ps`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull debian:bookworm
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Estados y transiciones | Recorrer created, running, paused, exited observando cada transicion con `docker inspect` |
| 2 | PID 1 y exit codes | El contenedor refleja el estado de PID 1, exit codes por terminacion normal, error y senales |
| 3 | Filtrado por estado | Usar `docker ps` con `--filter status=` para localizar contenedores en cada estado |
| 4 | Persistencia de la capa de escritura | Un contenedor detenido conserva sus datos, `docker rm` los destruye |

## Archivos

```
labs/
└── README.md           <- Guia paso a paso (documento principal del lab)
```

Este laboratorio no requiere archivos de soporte. Todos los ejercicios usan
la imagen `debian:bookworm` directamente.

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el analisis de
resultados.

## Tiempo estimado

~20 minutos (las 4 partes completas).

## Recursos Docker creados

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lifecycle` | contenedor | 1 | Si |
| `exit-ok` | contenedor | 2 | Si |
| `exit-err` | contenedor | 2 | Si |
| `exit-kill` | contenedor | 2 | Si |
| `exit-term` | contenedor | 2 | Si |
| `filter-run` | contenedor | 3 | Si |
| `filter-stop` | contenedor | 3 | Si |
| `filter-pause` | contenedor | 3 | Si |
| `persist-test` | contenedor | 4 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
