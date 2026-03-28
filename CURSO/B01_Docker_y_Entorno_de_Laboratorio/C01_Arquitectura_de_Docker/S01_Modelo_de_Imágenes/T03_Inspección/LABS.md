# Labs -- Inspeccion

## Descripcion

Laboratorio practico con 4 partes progresivas que verifican de forma empirica
los conceptos cubiertos en este topico: `docker image inspect` con consultas
`--format`, `docker history` con y sin `--no-trunc`, el significado de
`<missing>` en el historial, y comparacion de tamano real vs virtual con
`docker system df`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagenes base descargadas:

```bash
docker pull debian:bookworm
docker pull nginx:latest
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | `docker image inspect` | Extraer campos especificos con `--format`: Config.Cmd, Config.Entrypoint, Config.Env, Config.ExposedPorts, RootFS.Layers, Architecture, Size |
| 2 | `docker history` | Leer el historial de capas, usar `--no-trunc`, entender `<missing>` en capas descargadas vs construidas localmente |
| 3 | Imagen local vs imagen remota | Construir una imagen local, comparar su history con una imagen descargada, observar donde aparece `<missing>` |
| 4 | Tamano real vs virtual | Medir espacio con `docker system df -v`, entender SHARED SIZE vs UNIQUE SIZE, verificar que deduplicacion reduce el espacio real |

## Archivos

```
labs/
├── README.md              <- Guia paso a paso (documento principal del lab)
├── show-info.sh           <- Script que muestra variables de entorno y argumentos
├── Dockerfile.inspect     <- Imagen con multiples instrucciones de metadata
└── Dockerfile.local       <- Imagen local para comparar history con imagenes remotas
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
| `debian:bookworm` | Imagen (pull) | 1-4 | Si |
| `nginx:latest` | Imagen (pull) | 1 | Si |
| `lab-inspect` | Imagen (build) | 1-2 | Si |
| `lab-local` | Imagen (build) | 3 | Si |

Ninguna imagen persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
