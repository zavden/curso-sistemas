# Labs — HEALTHCHECK

## Descripción

Laboratorio práctico con 4 partes progresivas que demuestran la instrucción
HEALTHCHECK: la transición de estados (starting → healthy → unhealthy),
control manual del estado de salud, impacto de los parámetros (start-period,
interval, retries), y healthchecks definidos en runtime.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imágenes base descargadas:

```bash
docker pull nginx:latest
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | starting → healthy | Transición de estados en un servidor nginx con healthcheck |
| 2 | Provocar unhealthy | App con salud controlable, romper y recuperar, inspeccionar Health.Log |
| 3 | start-period y parámetros | Impacto de start-period en apps con arranque lento, comparar con y sin |
| 4 | Runtime y HEALTHCHECK NONE | Definir healthcheck en `docker run`, deshabilitar con `--no-healthcheck` |

## Archivos

```
labs/
├── README.md                ← Guía paso a paso (documento principal del lab)
├── app.sh                   ← App con arranque rápido y marcador de salud (Parte 2)
├── slow-app.sh              ← App con arranque lento — 12s de inicialización (Parte 3)
├── Dockerfile.health-basic  ← Nginx con healthcheck básico (Parte 1)
├── Dockerfile.health-app    ← App con salud controlable (Parte 2)
└── Dockerfile.health-slow   ← App lenta con start-period (Parte 3)
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
| `lab-health-basic` | 1 | Sí |
| `lab-health-app` | 2 | Sí |
| `lab-health-slow` | 3 | Sí |

Ninguna imagen persiste después de completar el lab. La limpieza se hace
al final de cada parte y en la sección de limpieza final.
