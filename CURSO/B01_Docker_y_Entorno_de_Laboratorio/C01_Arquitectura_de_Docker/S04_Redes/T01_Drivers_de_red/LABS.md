# Labs — Drivers de red

## Descripcion

Laboratorio practico con 4 partes progresivas que verifican de forma empirica
los conceptos de drivers de red en Docker: las redes predeterminadas, la
diferencia entre el bridge por defecto y un bridge custom, la red host, la red
none, y la gestion dinamica de redes con `docker network connect/disconnect`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Redes predeterminadas | Listar las tres redes que Docker crea al instalarse, inspeccionar subred y gateway de bridge, identificar los drivers |
| 2 | Bridge por defecto vs custom | Crear una red custom, demostrar que el bridge por defecto no tiene DNS entre contenedores mientras que el custom si |
| 3 | Host y none | Ejecutar un contenedor con red host que comparte el network stack del host, ejecutar un contenedor sin red con none |
| 4 | Gestion dinamica de redes | Crear redes custom, conectar y desconectar un contenedor en caliente, inspeccionar las redes de un contenedor |

## Archivos

```
labs/
└── README.md   <- Guia paso a paso (documento principal del lab)
```

Este laboratorio no requiere archivos de soporte. Todos los ejercicios se
realizan directamente con comandos Docker e imagenes publicas.

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
| `lab-custom-bridge` | Red | 2 | Si |
| `lab-dynamic-net` | Red | 4 | Si |
| `lab-second-net` | Red | 4 | Si |
| `bridge-a`, `bridge-b` | Contenedor | 2 | Si |
| `custom-a`, `custom-b` | Contenedor | 2 | Si |
| `host-check` | Contenedor | 3 | Si |
| `none-check` | Contenedor | 3 | Si |
| `dynamic-target` | Contenedor | 4 | Si |
| `dynamic-peer` | Contenedor | 4 | Si |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
