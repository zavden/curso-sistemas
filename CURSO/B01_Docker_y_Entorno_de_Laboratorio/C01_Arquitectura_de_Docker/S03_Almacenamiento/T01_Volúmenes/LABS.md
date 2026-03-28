# Labs -- Volumenes

## Descripcion

Laboratorio practico con 5 partes progresivas que demuestran los tres tipos de
montaje en Docker (named volumes, bind mounts, tmpfs), la diferencia entre las
sintaxis `-v` y `--mount`, volumenes anonimos, la copia inicial de datos, y el
uso compartido de volumenes entre contenedores.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Named volumes | Crear, usar, inspeccionar y eliminar named volumes con `docker volume`; persistencia de datos tras `docker rm` |
| 2 | Bind mounts | Montar un directorio del host en el contenedor; visibilidad bidireccional de cambios; modo solo lectura |
| 3 | tmpfs mounts | Montar filesystem en memoria; comprobar que nada se escribe a disco; los datos desaparecen al detener el contenedor |
| 4 | `-v` vs `--mount` y volumenes anonimos | Diferencia de comportamiento ante paths inexistentes; volumenes anonimos creados por la instruccion VOLUME; acumulacion de volumenes huerfanos |
| 5 | Copia inicial y volumenes compartidos | Comportamiento de copia inicial de named volumes sobre directorios con datos preexistentes en la imagen; compartir un volumen entre dos contenedores |

## Archivos

```
labs/
├── README.md              <- Guia paso a paso (documento principal del lab)
└── Dockerfile.anon-vol    <- Imagen con instruccion VOLUME (Parte 4)
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el analisis de
resultados.

## Tiempo estimado

~30 minutos (las 5 partes completas).

## Recursos Docker creados

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-data` | Volumen | 1 | Si |
| `lab-shared` | Volumen | 5 | Si |
| `lab-anon-vol` | Imagen | 4 | Si |
| `vol-writer` | Contenedor | 1 | Si |
| `vol-reader` | Contenedor | 1 | Si |
| `bind-test` | Contenedor | 2 | Si |
| `tmpfs-test` | Contenedor | 3 | Si |
| `anon-vol-test` | Contenedor | 4 | Si |
| `shared-writer` | Contenedor | 5 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
