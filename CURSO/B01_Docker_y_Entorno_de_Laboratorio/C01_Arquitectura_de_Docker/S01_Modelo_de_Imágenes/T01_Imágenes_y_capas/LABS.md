# Labs — Imágenes y capas

## Descripción

Laboratorio práctico con 5 partes progresivas que verifican de forma empírica
los conceptos cubiertos en este tópico: anatomía de capas, Copy-on-Write,
deduplicación, eliminación en capas separadas, y whiteouts.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull debian:bookworm
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Anatomía de capas | Construir una imagen, ver cada capa con `docker history`, distinguir capas con contenido de capas de metadata |
| 2 | Copy-on-Write | Inmutabilidad de la imagen, capas de escritura independientes por contenedor, costo de CoW con archivos grandes, `docker diff` |
| 3 | Deduplicación | Dos imágenes con base común comparten capas, comparar hashes SHA256, espacio real vs virtual con `docker system df` |
| 4 | Eliminación en capas separadas | Comparar imagen "mala" (rm en capa separada) vs "buena" (rm en misma capa), caso real con gcc |
| 5 | Whiteouts y capa efímera | Estructura OverlayFS de un contenedor, archivos whiteout con `docker diff`, `stop` preserva vs `rm` destruye |

## Archivos

```
labs/
├── README.md           ← Guía paso a paso (documento principal del lab)
├── hello.c             ← Programa C compilado dentro de una imagen
├── Dockerfile.layers   ← Imagen con múltiples capas (Parte 1)
├── Dockerfile.dedup-a  ← Imagen con curl (Parte 3)
├── Dockerfile.dedup-b  ← Imagen con wget (Parte 3)
├── Dockerfile.bad      ← Elimina archivo en capa separada (Parte 4)
├── Dockerfile.good     ← Crea y elimina en la misma capa (Parte 4)
└── Dockerfile.gcc      ← Compilar con gcc y desinstalar en un RUN (Parte 4)
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el análisis de
resultados.

## Tiempo estimado

~30 minutos (las 5 partes completas).

## Imágenes Docker que se crean

| Imagen | Parte | Se elimina al final |
|---|---|---|
| `lab-layers` | 1 | Sí |
| `lab-dedup-curl` | 3 | Sí |
| `lab-dedup-wget` | 3 | Sí |
| `lab-bad` | 4 | Sí |
| `lab-good` | 4 | Sí |
| `lab-gcc` | 4 | Sí |

Ninguna imagen persiste después de completar el lab. La limpieza se hace
al final de cada parte y en la sección de limpieza final.
