# Labs — Exposicion de puertos

## Descripcion

Laboratorio practico con 4 partes progresivas que verifican de forma empirica
los conceptos de exposicion de puertos: la diferencia entre EXPOSE (metadata)
y `-p` (publicacion real), las variantes de port mapping, la publicacion
automatica con `-P`, y la comunicacion interna entre contenedores sin
necesidad de publicar puertos.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas:

```bash
docker pull nginx:alpine
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | EXPOSE vs -p | EXPOSE es solo metadata y no publica puertos; `-p` es lo que realmente crea el port mapping |
| 2 | Variantes de -p | Binding a todas las interfaces vs localhost, multiples puertos, `docker port` para ver mapeos |
| 3 | -P y puertos automaticos | Publicacion automatica de todos los puertos declarados con EXPOSE a puertos aleatorios del host |
| 4 | Comunicacion interna sin -p | Contenedores en la misma red custom se comunican directamente sin publicar puertos; el host solo llega con -p |

## Archivos

```
labs/
├── README.md              <- Guia paso a paso (documento principal del lab)
└── Dockerfile.expose-test <- Imagen custom con EXPOSE para demostrar que no publica puertos
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el analisis de
resultados.

## Tiempo estimado

~25 minutos (las 4 partes completas).

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-expose-test` | Imagen | 1 | Si |
| `lab-internal-net` | Red | 4 | Si |
| `expose-only` | Contenedor | 1 | Si |
| `published-web` | Contenedor | 1 | Si |
| `public-web` | Contenedor | 2 | Si |
| `local-web` | Contenedor | 2 | Si |
| `auto-web` | Contenedor | 3 | Si |
| `internal-server` | Contenedor | 4 | Si |
| `internal-client` | Contenedor | 4 | Si |
| `published-server` | Contenedor | 4 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
