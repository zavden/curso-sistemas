# Labs — Equivalencia de comandos

## Descripción

Laboratorio práctico con 4 partes progresivas para demostrar que Podman
es un drop-in replacement de Docker CLI: mismos comandos, alias transparente,
build con Buildah, y gestión de registries.

## Prerequisitos

- Podman y Docker instalados
- Imagen base descargada:

```bash
podman pull docker.io/library/alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Mismos comandos | run, ps, exec, logs, stop, rm idénticos |
| 2 | Alias docker=podman | Scripts Docker funcionan sin cambios |
| 3 | Build con Podman | Dockerfile funciona con Buildah |
| 4 | Registries | Nombres completos y configuración |

## Archivos

```
labs/
├── README.md          ← Guía paso a paso (documento principal del lab)
├── Dockerfile.build   ← Dockerfile para probar podman build
└── app.sh             ← Script simple para el build
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~20 minutos (las 4 partes completas).

## Recursos Docker/Podman que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-equiv` | Imagen (Podman) | 3 | Sí |
| Contenedores temporales | Contenedor | 1-4 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
