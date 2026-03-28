# Labs — Cuándo usar Docker vs Podman

## Descripción

Laboratorio práctico con 3 partes para comparar Docker y Podman en la
práctica: diferencias de entorno y seguridad, el mismo workflow completo
en ambas herramientas, y ejercicios de decisión sobre cuál usar según
el contexto.

## Prerequisitos

- Podman y Docker instalados
- Imágenes descargadas:

```bash
docker pull alpine:latest
podman pull docker.io/library/alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Comparar entornos | Storage, seguridad, daemon vs daemonless |
| 2 | Mismo workflow | Build, run, test — idéntico resultado |
| 3 | Decisión práctica | Elegir herramienta según escenario |

## Archivos

```
labs/
├── README.md          ← Guía paso a paso (documento principal del lab)
├── Dockerfile.test    ← Dockerfile para comparar build
└── app.sh             ← Script simple para el build
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~15 minutos (las 3 partes completas).

## Recursos Docker/Podman que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-compare` | Imagen (Docker + Podman) | 2 | Sí |
| Contenedores temporales | Contenedor | 1-2 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
