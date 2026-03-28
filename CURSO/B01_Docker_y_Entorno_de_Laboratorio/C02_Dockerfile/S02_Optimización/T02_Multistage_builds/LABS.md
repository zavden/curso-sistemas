# Labs — Multi-stage builds

## Descripción

Laboratorio práctico con 4 partes progresivas que demuestran multi-stage builds:
comparación single-stage vs multi-stage, uso de `FROM scratch` para imágenes
mínimas, `COPY --from` entre stages y desde imágenes externas, y `--target`
para crear variantes dev/prod desde un solo Dockerfile.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imágenes base descargadas:

```bash
docker pull debian:bookworm
docker pull debian:bookworm-slim
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Single vs multi-stage | Comparar tamaños: imagen con gcc vs imagen solo con binario |
| 2 | FROM scratch | Imagen mínima absoluta — solo el binario estático |
| 3 | COPY --from | Copiar entre stages y desde imágenes externas |
| 4 | --target | Crear variantes dev y prod desde el mismo Dockerfile |

## Archivos

```
labs/
├── README.md               ← Guía paso a paso (documento principal del lab)
├── hello.c                 ← Programa C para compilar (Partes 1-3)
├── app.py                  ← Script Python para demo --target (Parte 4)
├── Dockerfile.single       ← Single-stage: gcc + binario en la misma imagen (Parte 1)
├── Dockerfile.multi        ← Multi-stage: builder + runtime slim (Parte 1)
├── Dockerfile.multi-scratch ← Multi-stage con FROM scratch (Parte 2)
└── Dockerfile.target       ← Dev vs prod con --target (Parte 4)
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~25 minutos (las 4 partes completas).

## Imágenes Docker que se crean

| Imagen | Parte | Se elimina al final |
|---|---|---|
| `lab-single` | 1 | Sí |
| `lab-multi` | 1 | Sí |
| `lab-scratch` | 2 | Sí |
| `lab-target-dev` | 4 | Sí |
| `lab-target-prod` | 4 | Sí |

Ninguna imagen persiste después de completar el lab.
