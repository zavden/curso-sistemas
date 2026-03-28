# Labs — Instrucciones básicas

## Descripción

Laboratorio práctico con 5 partes progresivas que cubren las instrucciones
fundamentales de un Dockerfile: FROM y sus variantes, RUN en forma shell y exec,
COPY/ADD/WORKDIR, combinación de RUN para optimizar capas, y las instrucciones
de metadata (CMD, EXPOSE, LABEL).

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imágenes base descargadas:

```bash
docker pull debian:bookworm
docker pull debian:bookworm-slim
docker pull alpine:3.19
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | FROM: la imagen base | Compilar el mismo programa con `debian:bookworm`, `debian:bookworm-slim` y `alpine`, comparar tamaños |
| 2 | RUN: shell vs exec | Diferencia entre forma shell (con expansión de variables) y forma exec (literal), cuándo usar cada una |
| 3 | COPY, ADD y WORKDIR | Copiar archivos al filesystem de la imagen, persistencia de WORKDIR, auto-extracción de tar con ADD |
| 4 | Combinar RUN | Impacto de separar vs combinar instrucciones RUN en el tamaño de la imagen, patrón install+clean |
| 5 | CMD, EXPOSE y LABEL | Instrucciones de metadata, capas de 0 bytes, sobrescribir CMD con `docker run`, inspeccionar metadata |

## Archivos

```
labs/
├── README.md               ← Guía paso a paso (documento principal del lab)
├── hello.c                 ← Programa C simple para compilar en las imágenes
├── config.txt              ← Archivo de configuración para demos de COPY
├── Dockerfile.from-full    ← FROM debian:bookworm (Parte 1)
├── Dockerfile.from-slim    ← FROM debian:bookworm-slim (Parte 1)
├── Dockerfile.from-alpine  ← FROM alpine:3.19 (Parte 1)
├── Dockerfile.run-forms    ← Forma shell vs exec de RUN (Parte 2)
├── Dockerfile.copy-workdir ← COPY y WORKDIR en acción (Parte 3)
├── Dockerfile.copy-vs-add  ← COPY vs ADD con tarball (Parte 3)
├── Dockerfile.run-separate ← Patrón incorrecto: RUN separados (Parte 4)
├── Dockerfile.run-combined ← Patrón correcto: RUN combinado (Parte 4)
└── Dockerfile.metadata     ← CMD, EXPOSE, LABEL (Parte 5)
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el análisis de
resultados.

## Tiempo estimado

~35 minutos (las 5 partes completas).

## Imágenes Docker que se crean

| Imagen | Parte | Se elimina al final |
|---|---|---|
| `lab-from-full` | 1 | Sí |
| `lab-from-slim` | 1 | Sí |
| `lab-from-alpine` | 1 | Sí |
| `lab-run-forms` | 2 | Sí |
| `lab-copy-workdir` | 3 | Sí |
| `lab-copy-vs-add` | 3 | Sí |
| `lab-run-separate` | 4 | Sí |
| `lab-run-combined` | 4 | Sí |
| `lab-metadata` | 5 | Sí |

Ninguna imagen persiste después de completar el lab. La limpieza se hace
al final de cada parte y en la sección de limpieza final.
