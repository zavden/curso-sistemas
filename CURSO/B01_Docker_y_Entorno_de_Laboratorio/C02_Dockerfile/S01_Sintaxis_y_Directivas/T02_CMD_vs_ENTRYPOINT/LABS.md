# Labs — CMD vs ENTRYPOINT

## Descripción

Laboratorio práctico con 5 partes progresivas que demuestran las diferencias
entre CMD y ENTRYPOINT: CMD como comando reemplazable, ENTRYPOINT como
ejecutable fijo, la combinación de ambos, el impacto de la forma shell vs exec
en PID 1 y señales, y el patrón de wrapper script con `exec "$@"`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Qué se demuestra |
|---|---|---|
| 1 | CMD solo | CMD como comando por defecto, reemplazo total con argumentos de `docker run` |
| 2 | ENTRYPOINT solo | Ejecutable fijo, argumentos se añaden, `--entrypoint` para sobrescribir |
| 3 | ENTRYPOINT + CMD | Patrón combinado, CMD como args por defecto, inspeccionar metadata |
| 4 | Shell vs exec y señales | PID 1, propagación de SIGTERM, impacto en `docker stop` |
| 5 | Wrapper script | Patrón `exec "$@"`, entrypoint con setup, variables de entorno |

## Archivos

```
labs/
├── README.md               ← Guía paso a paso (documento principal del lab)
├── entrypoint.sh           ← Wrapper script con exec "$@" (Parte 5)
├── Dockerfile.cmd-only     ← CMD solo (Parte 1)
├── Dockerfile.entrypoint-only ← ENTRYPOINT solo (Parte 2)
├── Dockerfile.ep-cmd       ← ENTRYPOINT + CMD combinados (Parte 3)
├── Dockerfile.exec-form    ← ENTRYPOINT forma exec (Parte 4)
├── Dockerfile.shell-form   ← ENTRYPOINT forma shell (Parte 4)
└── Dockerfile.wrapper      ← Imagen con wrapper script (Parte 5)
```

## Cómo ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el análisis de
resultados.

## Tiempo estimado

~25 minutos (las 5 partes completas).

## Imágenes Docker que se crean

| Imagen | Parte | Se elimina al final |
|---|---|---|
| `lab-cmd-only` | 1 | Sí |
| `lab-ep-only` | 2 | Sí |
| `lab-ep-cmd` | 3 | Sí |
| `lab-exec-form` | 4 | Sí |
| `lab-shell-form` | 4 | Sí |
| `lab-wrapper` | 5 | Sí |

Ninguna imagen persiste después de completar el lab. La limpieza se hace
al final de cada parte y en la sección de limpieza final.
