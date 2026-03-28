# Labs — Rootless containers

## Descripcion

Laboratorio practico con 4 partes que exploran la diferencia entre Docker
rootful y rootless, la propiedad de procesos en el host, user namespaces,
y la alternativa Podman rootless. Algunas partes son observacionales porque
la configuracion de Docker rootless depende del sistema.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

- Opcionalmente: Podman instalado (`podman --version`) para las partes
  que lo utilizan. Si no esta disponible, esas partes se documentan como
  observacionales.

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Modo rootful vs rootless | Detectar en que modo corre Docker, inspeccionar el socket y el directorio de datos |
| 2 | Propiedad de procesos en el host | Verificar que el proceso del contenedor pertenece a root en el host (rootful) |
| 3 | User namespaces | Inspeccionar el mapeo de UIDs entre contenedor y host con `docker inspect` |
| 4 | Podman rootless | Comparar el modelo de Podman (sin daemon, rootless nativo) con Docker rootful |

## Archivos

```
labs/
└── README.md   <- Guia paso a paso (documento principal del lab)
```

Este laboratorio no requiere archivos de soporte. Todos los ejercicios
se realizan con comandos directos.

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el analisis de
resultados.

**Nota**: Algunas partes dependen de la configuracion del sistema (Docker
rootless, Podman). Los pasos que requieren configuracion especifica estan
claramente documentados. Si un paso no aplica a tu sistema, el lab
proporciona las salidas esperadas para analisis.

## Tiempo estimado

~15 minutos (las 4 partes completas).

## Recursos Docker creados

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `rootless-check` | Contenedor | 1, 2 | Si |
| `ns-inspect` | Contenedor | 3 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
