# Labs — Redes multi-servicio

## Descripción

Laboratorio práctico con 4 partes progresivas para entender el networking
de Docker Compose: red default con DNS automático, redes custom para
aislamiento, redes internas sin acceso a internet, y redes externas para
comunicar dos proyectos.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Docker Compose v2 (`docker compose version`)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Red default | Todos los servicios se ven por DNS de servicio |
| 2 | Redes custom | Aislamiento frontend/backend |
| 3 | Red interna | Contenedor sin acceso a internet |
| 4 | Red externa | Comunicar dos proyectos Compose |

## Archivos

```
labs/
├── README.md               ← Guía paso a paso (documento principal del lab)
├── compose.default-net.yml ← Tres servicios en la red default
├── compose.custom-nets.yml ← Servicios en redes frontend/backend
├── compose.internal.yml    ← Servicio en red interna (sin internet)
├── compose.project-a.yml  ← Proyecto A (crea red compartida)
└── compose.project-b.yml  ← Proyecto B (usa red externa)
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~25 minutos (las 4 partes completas).

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Contenedores web, api, db | Contenedor | 1-2 | Sí |
| Contenedores isolated, connected | Contenedor | 3 | Sí |
| Contenedores service_a, service_b | Contenedor | 4 | Sí |
| Redes frontend, backend | Red | 2 | Sí |
| Red no_internet | Red | 3 | Sí |
| Red lab-shared-network | Red | 4 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
