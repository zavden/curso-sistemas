# Labs — podman-compose

## Descripción

Laboratorio práctico con 3 partes progresivas para trabajar con archivos
Compose en Podman: usar podman-compose directamente, usar Docker Compose
via socket API, y comparar ambas opciones.

## Prerequisitos

- Podman instalado (`podman --version`)
- podman-compose instalado (`podman-compose --version`)
- Docker Compose instalado (para comparación via socket)
- Imágenes descargadas:

```bash
podman pull docker.io/library/nginx:latest
podman pull docker.io/library/alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | podman-compose | Levantar servicios con podman-compose |
| 2 | Docker Compose via socket | Usar Docker Compose con Podman como backend |
| 3 | Comparación | Diferencias prácticas entre ambas opciones |

## Archivos

```
labs/
├── README.md     ← Guía paso a paso (documento principal del lab)
└── compose.yml   ← Archivo Compose con web + app
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~20 minutos (las 3 partes completas).

## Recursos Podman que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Contenedores web, app | Contenedor | 1-3 | Sí |
| Red del proyecto | Red | 1-3 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
