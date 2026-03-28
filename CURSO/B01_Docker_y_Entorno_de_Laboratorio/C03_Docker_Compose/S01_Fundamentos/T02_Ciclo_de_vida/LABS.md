# Labs — Ciclo de vida de servicios

## Descripción

Laboratorio práctico con 4 partes progresivas para dominar el ciclo de
vida de servicios en Docker Compose: up/down, detección de cambios en
configuración, stop/start vs down, y ejecución de comandos con exec y run.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Docker Compose v2 (`docker compose version`)
- Imágenes base descargadas:

```bash
docker pull nginx:alpine
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | up y down | Ciclo completo: crear, inspeccionar, destruir |
| 2 | Detección de cambios | up idempotente, recrear solo lo modificado |
| 3 | stop/start vs up/down | Diferencia entre detener y destruir |
| 4 | exec y run | Ejecutar comandos en contenedores existentes vs nuevos |

## Archivos

```
labs/
├── README.md            ← Guía paso a paso (documento principal del lab)
├── compose.yml          ← Web (nginx) + worker (alpine con loop)
└── compose.updated.yml  ← Versión modificada (puerto cambiado)
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~20 minutos (las 4 partes completas).

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `labs-web-1` | Contenedor | 1-4 | Sí |
| `labs-worker-1` | Contenedor | 1-4 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
