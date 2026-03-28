# Labs -- Persistencia

## Descripcion

Laboratorio practico con 4 partes progresivas que demuestran de forma empirica
que datos sobreviven a `docker stop`, `docker rm` y `docker rm -v`, la perdida
de datos cuando no se usan volumenes, la persistencia de named volumes a traves
de la recreacion de contenedores, la trampa de la instruccion VOLUME en el
Dockerfile, y el uso de `docker commit` para capturar el estado de un
contenedor.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas:

```bash
docker pull alpine:latest
docker pull debian:bookworm
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Tabla de supervivencia | Que sobrevive a `stop`+`start`, que sobrevive a `rm`, que sobrevive a `rm -v`; perdida de datos en la capa de escritura |
| 2 | Persistencia con named volumes | Los datos en named volumes sobreviven a la destruccion y recreacion del contenedor; patron de uso con bases de datos simuladas |
| 3 | La trampa de VOLUME | No se pueden escribir datos despues de VOLUME en el Dockerfile; el orden de instrucciones importa |
| 4 | `docker commit` | Capturar el estado de un contenedor como imagen; limitaciones vs Dockerfile |

## Archivos

```
labs/
├── README.md               <- Guia paso a paso (documento principal del lab)
├── Dockerfile.vol-trap-bad  <- VOLUME antes de escribir datos (Parte 3)
└── Dockerfile.vol-trap-good <- Escribir datos antes de VOLUME (Parte 3)
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
| `lab-persist` | Volumen | 2 | Si |
| `ephemeral-test` | Contenedor | 1 | Si |
| `survive-stop` | Contenedor | 1 | Si |
| `survive-rm` | Contenedor | 1 | Si |
| `anon-vol-rm` | Contenedor | 1 | Si |
| `persist-writer` | Contenedor | 2 | Si |
| `persist-reader` | Contenedor | 2 | Si |
| `lab-vol-trap-bad` | Imagen | 3 | Si |
| `lab-vol-trap-good` | Imagen | 3 | Si |
| `lab-committed` | Imagen | 4 | Si |
| `commit-base` | Contenedor | 4 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
