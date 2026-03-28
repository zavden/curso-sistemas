# Labs — Pods

## Descripción

Laboratorio práctico con 4 partes progresivas para entender los pods de
Podman: crear un pod con múltiples contenedores, comunicación por localhost,
puertos a nivel de pod, y generación/consumo de YAML de Kubernetes.

## Prerequisitos

- Podman instalado (`podman --version`)
- Imágenes descargadas:

```bash
podman pull docker.io/library/nginx:latest
podman pull docker.io/library/alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Crear un pod | Pod con contenedor infra, añadir contenedores |
| 2 | Comunicación localhost | Contenedores se ven por localhost, no DNS |
| 3 | Puertos y conflictos | Puertos a nivel de pod, no de contenedor |
| 4 | YAML de Kubernetes | Generar y consumir YAML desde/hacia pods |

## Archivos

```
labs/
├── README.md   ← Guía paso a paso (documento principal del lab)
└── pod.yaml    ← YAML de Kubernetes para crear pod
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~25 minutos (las 4 partes completas).

## Recursos Podman que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-pod` | Pod | 1-3 | Sí |
| `lab-kube-pod` | Pod | 4 | Sí |
| Contenedores web, client | Contenedor | 1-3 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
