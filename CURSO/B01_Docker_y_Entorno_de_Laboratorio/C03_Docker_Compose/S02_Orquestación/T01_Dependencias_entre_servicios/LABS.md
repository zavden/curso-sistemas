# Labs — Dependencias entre servicios

## Descripción

Laboratorio práctico con 4 partes progresivas para entender el control de
orden de arranque en Docker Compose: arranque paralelo sin dependencias,
`depends_on` simple (insuficiente), `depends_on` con healthcheck (correcto),
y `service_completed_successfully` para tareas de inicialización.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Docker Compose v2 (`docker compose version`)
- Imágenes base descargadas:

```bash
docker pull alpine:latest
docker pull postgres:16-alpine
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | Sin depends_on | Arranque paralelo, sin orden garantizado |
| 2 | depends_on simple | Orden de arranque, pero sin esperar readiness |
| 3 | depends_on + healthcheck | Esperar a que el servicio esté realmente listo |
| 4 | service_completed_successfully | Esperar a que una tarea de init termine |

## Archivos

```
labs/
├── README.md                ← Guía paso a paso (documento principal del lab)
├── compose.parallel.yml     ← Tres servicios sin depends_on
├── compose.deps-simple.yml  ← API con depends_on simple hacia PostgreSQL
├── compose.deps-healthy.yml ← API con depends_on + healthcheck
└── compose.deps-init.yml    ← Tarea init + app con service_completed_successfully
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
| Contenedores api, db, cache | Contenedor | 1-3 | Sí |
| Contenedores init, app | Contenedor | 4 | Sí |
| Volúmenes de PostgreSQL | Volumen | 2-3 | Sí (con down -v) |

Ningún recurso del lab persiste después de completar la limpieza.
