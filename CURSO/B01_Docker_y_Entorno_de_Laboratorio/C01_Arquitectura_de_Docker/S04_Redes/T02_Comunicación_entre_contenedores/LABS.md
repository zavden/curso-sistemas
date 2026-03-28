# Labs — Comunicacion entre contenedores

## Descripcion

Laboratorio practico con 4 partes progresivas que verifican de forma empirica
los mecanismos de comunicacion entre contenedores: resolucion DNS por nombre,
aliases de red con round-robin DNS, contenedores multi-red que actuan como
puente, y la ausencia de DNS en la red por defecto.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | DNS en red custom | Resolucion de nombres entre contenedores en una red custom, el servidor DNS embebido en 127.0.0.11 |
| 2 | Aliases y round-robin | Multiples nombres para un contenedor con `--network-alias`, distribucion DNS round-robin con multiples contenedores compartiendo alias |
| 3 | Contenedores multi-red | Conectar un contenedor a dos redes para que actue como puente, verificar aislamiento entre redes |
| 4 | Red por defecto sin DNS | Demostrar que la red bridge por defecto no resuelve nombres, solo permite comunicacion por IP |

## Archivos

```
labs/
└── README.md   <- Guia paso a paso (documento principal del lab)
```

Este laboratorio no requiere archivos de soporte. Todos los ejercicios se
realizan directamente con comandos Docker e imagenes publicas.

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el analisis de
resultados.

## Tiempo estimado

~25 minutos (las 4 partes completas).

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-dns-net` | Red | 1 | Si |
| `lab-alias-net` | Red | 2 | Si |
| `lab-frontend` | Red | 3 | Si |
| `lab-backend` | Red | 3 | Si |
| `dns-server` | Contenedor | 1 | Si |
| `dns-client` | Contenedor | 1 | Si |
| `alias-main` | Contenedor | 2 | Si |
| `rr-worker-1`, `rr-worker-2`, `rr-worker-3` | Contenedor | 2 | Si |
| `rr-client` | Contenedor | 2 | Si |
| `multi-api` | Contenedor | 3 | Si |
| `multi-web` | Contenedor | 3 | Si |
| `multi-db` | Contenedor | 3 | Si |
| `default-a`, `default-b` | Contenedor | 4 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
