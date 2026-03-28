# Labs — Variables de entorno

## Descripción

Laboratorio práctico con 4 partes progresivas para dominar las tres formas
de manejar variables de entorno en Docker Compose: `environment:` inline,
`env_file:` externo, interpolación con `.env`, y la precedencia entre fuentes.

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
| 1 | environment: inline | Variables definidas directamente en el Compose file |
| 2 | env_file: | Cargar variables desde archivo externo |
| 3 | Interpolación .env | Sustituir ${VAR} en el Compose file desde .env |
| 4 | Precedencia | Orden de prioridad entre fuentes de variables |

## Archivos

```
labs/
├── README.md                ← Guía paso a paso (documento principal del lab)
├── compose.env-inline.yml   ← Variables con environment: inline
├── compose.env-file.yml     ← Variables con env_file:
├── compose.env-interp.yml   ← Interpolación ${VAR} desde .env
├── compose.precedence.yml   ← Combinación de environment: y env_file:
└── app.env                  ← Archivo de variables para env_file:
```

El archivo `.env` (interpolación) se crea y elimina durante el lab.

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
| Contenedores temporales | Contenedor | 1-4 | Sí (se eliminan solos) |
| `labs-web-1` | Contenedor | 3 | Sí |

Ningún recurso del lab persiste después de completar la limpieza.
