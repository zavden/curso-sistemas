# Labs — Diferencias entre stop, kill y rm

## Descripcion

Laboratorio practico con 4 partes progresivas que verifican de forma empirica
las diferencias entre `docker stop`, `docker kill` y `docker rm`: senales
enviadas, grace period, manejo de SIGTERM con un programa en C, y que datos
se preservan o destruyen con cada operacion.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull debian:bookworm
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | `stop` vs `kill` | SIGTERM con grace period vs SIGKILL inmediato, medir tiempos |
| 2 | Manejo de senales | Programa en C que atrapa SIGTERM, demuestra graceful shutdown vs terminacion forzada |
| 3 | `rm` vs `rm -f` | `rm` requiere contenedor detenido, `rm -f` mata y elimina; efecto en la capa de escritura |
| 4 | `--stop-timeout` y `--init` | Configurar el grace period, tini como PID 1 para propagacion correcta de senales |

## Archivos

```
labs/
├── README.md                      <- Guia paso a paso (documento principal del lab)
├── signal-handler.c               <- Programa en C que atrapa SIGTERM y demuestra graceful shutdown
└── Dockerfile.signal-handler      <- Imagen que compila y ejecuta signal-handler.c
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
| `lab-signal-handler` | imagen | 2 | Si |
| `stop-test` | contenedor | 1 | Si |
| `kill-test` | contenedor | 1 | Si |
| `stop-quick` | contenedor | 1 | Si |
| `signal-graceful` | contenedor | 2 | Si |
| `signal-forced` | contenedor | 2 | Si |
| `rm-stopped` | contenedor | 3 | Si |
| `rm-running` | contenedor | 3 | Si |
| `rm-data` | contenedor | 3 | Si |
| `init-demo` | contenedor | 4 | Si |
| `no-init-demo` | contenedor | 4 | Si |
| `timeout-demo` | contenedor | 4 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
