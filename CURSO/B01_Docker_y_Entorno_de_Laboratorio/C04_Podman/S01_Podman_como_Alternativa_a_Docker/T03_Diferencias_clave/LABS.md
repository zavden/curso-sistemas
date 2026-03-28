# Labs — Diferencias clave

## Descripción

Laboratorio práctico con 4 partes progresivas para explorar las diferencias
clave entre Docker y Podman: comportamiento sin daemon, integración con
systemd, etiquetas SELinux en bind mounts, y el socket API compatible.

## Prerequisitos

- Podman y Docker instalados
- Imagen base descargada:

```bash
podman pull docker.io/library/nginx:latest
podman pull docker.io/library/alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Sin daemon | Podman sin punto único de fallo, conmon por contenedor |
| 2 | Integración systemd | Generar unit files, gestionar como servicio |
| 3 | SELinux | Bind mounts con :z y :Z en Fedora/RHEL |
| 4 | Socket API | Docker Compose hablando con Podman via socket |

## Archivos

```
labs/
└── README.md  ← Guía paso a paso (documento principal del lab)
```

No hay archivos de soporte adicionales. Los recursos se crean durante
el lab.

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
| `lab-systemd` | Contenedor Podman | 2 | Sí |
| `container-lab-systemd.service` | Unit file systemd | 2 | Sí |
| Contenedores temporales | Contenedor | 1, 3-4 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
