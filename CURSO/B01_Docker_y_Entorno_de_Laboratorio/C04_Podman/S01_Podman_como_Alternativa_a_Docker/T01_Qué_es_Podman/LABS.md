# Labs — Qué es Podman

## Descripción

Laboratorio práctico con 4 partes progresivas para entender qué es Podman
y en qué se diferencia de Docker: primer contenedor rootless, ausencia de
daemon, almacenamiento por usuario, y compatibilidad OCI con Docker.

## Prerequisitos

- Podman instalado (`podman --version`)
- Docker instalado (para comparación)
- Imagen base descargada:

```bash
podman pull docker.io/library/alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Primer contenedor rootless | Podman funciona sin root ni daemon |
| 2 | Sin daemon | No hay proceso central, cada contenedor es independiente |
| 3 | Storage por usuario | Almacenamiento separado rootless vs rootful |
| 4 | Compatibilidad OCI | Misma imagen y Dockerfile entre Docker y Podman |

## Archivos

```
labs/
├── README.md          ← Guía paso a paso (documento principal del lab)
└── Dockerfile.compat  ← Dockerfile para probar compatibilidad OCI
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
| `lab-compat` | Imagen (Podman) | 4 | Sí |
| `lab-compat` | Imagen (Docker) | 4 | Sí |
| Contenedores temporales | Contenedor | 1-4 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
