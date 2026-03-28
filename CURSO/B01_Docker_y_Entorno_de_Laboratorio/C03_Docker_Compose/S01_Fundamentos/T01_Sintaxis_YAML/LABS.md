# Labs — Sintaxis YAML y estructura de Compose

## Descripcion

Laboratorio práctico con 4 partes progresivas para trabajar con la estructura
de un archivo Compose: archivo mínimo, opciones de servicio, construcción
local con `build:`, y múltiples servicios con nombre de proyecto.

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
| 1 | Compose file mínimo | Estructura YAML, validación con `config`, ciclo up/down |
| 2 | Opciones de servicio | ports, environment, restart |
| 3 | build local | Construir imagen desde Dockerfile en lugar de pull |
| 4 | Múltiples servicios | Dos servicios, nombre del proyecto, prefijos |

## Archivos

```
labs/
├── README.md              ← Guía paso a paso (documento principal del lab)
├── compose.minimal.yml    ← Un solo servicio (nginx)
├── compose.options.yml    ← Servicio con ports, environment, restart
├── compose.build.yml      ← Servicio con build local
├── compose.multi.yml      ← Dos servicios (web + api)
├── Dockerfile.app         ← Dockerfile para build local
└── app.sh                 ← Script simple (app simulada)
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
| Contenedores `labs-web-1`, `labs-api-1`, `labs-app-1` | Contenedor | 1-4 | Sí |
| Imagen build local | Imagen | 3 | Sí |

Las imágenes `nginx:alpine` y `alpine:latest` se descargan en prerequisitos
y no se eliminan.
