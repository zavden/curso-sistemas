# Labs -- Permisos en bind mounts

## Descripcion

Laboratorio practico con 4 partes progresivas que reproducen el problema
clasico de permisos en bind mounts (archivos creados como root), demuestran dos
soluciones (`--user` y Dockerfile con USER + ARG), y verifican el comportamiento
de `COPY --chown` en Dockerfiles.

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
| 1 | El problema de permisos | UID/GID compartido entre host y contenedor; archivos creados como root en bind mounts; `Permission denied` al intentar editar/borrar desde el host |
| 2 | Solucion con `--user` | Pasar `$(id -u):$(id -g)` al contenedor; archivos creados con el UID correcto; efecto secundario "I have no name!" |
| 3 | Solucion con Dockerfile | Crear un usuario con ARG para UID dinamico; instruccion USER; imagen reutilizable que produce archivos con la propiedad correcta |
| 4 | `COPY --chown` en Dockerfiles | Copiar archivos con propiedad especifica durante el build; comparar con COPY sin --chown |

## Archivos

```
labs/
├── README.md               <- Guia paso a paso (documento principal del lab)
├── Dockerfile.problem       <- Imagen que corre como root (Parte 1)
├── Dockerfile.user-arg      <- Imagen con USER + ARG para UID dinamico (Parte 3)
├── Dockerfile.copy-chown    <- Demuestra COPY --chown (Parte 4)
└── sample-config.txt        <- Archivo de ejemplo para COPY --chown (Parte 4)
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
| `lab-perm-problem` | Imagen | 1 | Si |
| `lab-perm-user-arg` | Imagen | 3 | Si |
| `lab-perm-copy-chown` | Imagen | 4 | Si |
| `perm-problem` | Contenedor | 1 | Si |
| `perm-user-fix` | Contenedor | 2 | Si |
| `perm-dockerfile-fix` | Contenedor | 3 | Si |
| `/tmp/lab-perms` | Directorio host | 1-3 | Si |
| `/tmp/lab-chown` | Directorio host | 4 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
