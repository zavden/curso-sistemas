# Labs — Ejecucion

## Descripcion

Laboratorio practico con 4 partes progresivas que verifican de forma empirica
los modos de ejecucion de contenedores: flags de `docker run` (-d, -it, --rm,
--name, -e, -w), ejecucion de comandos con `docker exec`, diferencias entre
`attach` y `exec`, y como sobrescribir CMD.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas:

```bash
docker pull debian:bookworm
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Modos interactivo y detached | Diferencias entre `-it`, `-d`, y sin flags; efecto de `-t` en la salida |
| 2 | Flags de configuracion | `--rm`, `--name`, `-e`, `-w` y sobrescribir CMD |
| 3 | `docker exec` | Ejecutar comandos en contenedores corriendo, flags `-it`, `-u`, `-w`, `-e` |
| 4 | `attach` vs `exec` | Diferencia fundamental: `attach` conecta a PID 1, `exec` crea proceso nuevo |

## Archivos

```
labs/
├── README.md                <- Guia paso a paso (documento principal del lab)
└── Dockerfile.exec-demo     <- Imagen con proceso de larga vida para practicar exec
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
| `lab-exec-demo` | imagen | 3 | Si |
| `run-default` | contenedor | 1 | Si |
| `run-tty` | contenedor | 1 | Si |
| `run-detach` | contenedor | 1 | Si |
| `run-interactive` | contenedor | 1 | Si |
| `name-demo` | contenedor | 2 | Si |
| `env-demo` | contenedor | 2 | Si |
| `exec-target` | contenedor | 3 | Si |
| `exec-custom` | contenedor | 3 | Si |
| `attach-demo` | contenedor | 4 | Si |
| `exec-compare` | contenedor | 4 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
