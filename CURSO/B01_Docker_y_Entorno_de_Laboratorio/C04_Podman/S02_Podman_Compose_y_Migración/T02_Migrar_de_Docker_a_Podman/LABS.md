# Labs — Migrar de Docker a Podman

## Descripción

Laboratorio práctico con 4 partes progresivas para realizar una migración
práctica de Docker a Podman: migrar imágenes, adaptar Compose files, manejar
puertos privilegiados rootless, y reemplazar restart policies con systemd.

## Prerequisitos

- Podman y Docker instalados
- Imágenes base descargadas:

```bash
docker pull nginx:latest
docker pull alpine:latest
podman pull docker.io/library/alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Migrar imágenes | docker save → podman load |
| 2 | Adaptar Compose | Nombres completos, :z en bind mounts |
| 3 | Puertos privilegiados | Limitación rootless con puertos < 1024 |
| 4 | Restart → systemd | Reemplazar --restart con systemd units |

## Archivos

```
labs/
├── README.md            ← Guía paso a paso (documento principal del lab)
├── compose.docker.yml   ← Compose original (estilo Docker)
├── compose.podman.yml   ← Compose adaptado para Podman
└── html/
    └── index.html       ← Contenido HTML para bind mount
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~25 minutos (las 4 partes completas).

## Recursos Docker/Podman que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-migrated` | Imagen (Podman) | 1 | Sí |
| Contenedores web | Contenedor | 2-4 | Sí |
| Unit file systemd | Archivo | 4 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
