# Labs — Root en contenedor

## Descripcion

Laboratorio practico con 4 partes progresivas que demuestran por que correr como
root en un contenedor es un riesgo de seguridad y como mitigarlo usando la
instruccion USER en el Dockerfile y la opcion `--user` en `docker run`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas:

```bash
docker pull alpine:latest
docker pull debian:bookworm
docker pull nginx:latest
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Root por defecto | El usuario predeterminado es root (UID 0), puede instalar paquetes y modificar archivos del sistema |
| 2 | Riesgo de root con bind mounts | Root en el contenedor puede leer y modificar archivos del host montados con `-v` |
| 3 | `--user` en docker run | Sobrescribir el usuario desde la linea de comandos para correr como non-root |
| 4 | USER en Dockerfile | Crear un usuario non-root en la imagen y establecerlo como predeterminado, comparar con imagenes oficiales |

## Archivos

```
labs/
├── README.md              <- Guia paso a paso (documento principal del lab)
└── Dockerfile.nonroot     <- Imagen con usuario non-root (Parte 4)
```

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
| `lab-nonroot` | Imagen | 4 | Si |
| `root-check` | Contenedor | 1 | Si (en la parte) |
| `root-risk` | Contenedor | 2 | Si (en la parte) |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
