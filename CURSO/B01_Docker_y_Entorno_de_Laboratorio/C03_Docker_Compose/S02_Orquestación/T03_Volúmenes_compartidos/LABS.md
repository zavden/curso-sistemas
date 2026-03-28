# Labs — Volúmenes compartidos

## Descripción

Laboratorio práctico con 4 partes progresivas para trabajar con volúmenes
en Docker Compose: named volumes para persistencia, bind mounts para
desarrollo, volúmenes compartidos entre servicios, y la diferencia entre
`down` y `down -v` para la preservación de datos.

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
| 1 | Named volumes | Persistencia de datos entre ciclos up/down |
| 2 | Bind mounts | Montar directorio del host, cambios inmediatos |
| 3 | Volúmenes compartidos | Dos servicios leyendo/escribiendo el mismo volumen |
| 4 | down vs down -v | Preservar vs destruir datos al detener |

## Archivos

```
labs/
├── README.md            ← Guía paso a paso (documento principal del lab)
├── compose.named.yml    ← Servicio con named volume
├── compose.bind.yml     ← Servicio con bind mount (:ro)
├── compose.shared.yml   ← Writer + reader compartiendo volumen
├── compose.persist.yml  ← Servicio para demo de persistencia
└── html/
    └── index.html       ← Contenido HTML para bind mount
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
| Contenedores app, web, writer, reader, db | Contenedor | 1-4 | Sí |
| Volumen app-data | Volumen | 1 | Sí |
| Volumen shared | Volumen | 3 | Sí |
| Volumen db-data | Volumen | 4 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
