# Labs — Capabilities y seccomp

## Descripcion

Laboratorio practico con 5 partes progresivas que demuestran como Docker
usa capabilities y seccomp para limitar lo que un contenedor puede hacer,
como manipular capabilities con `--cap-drop` y `--cap-add`, y la diferencia
entre un contenedor normal y uno con `--privileged`.

## Prerequisitos

- Docker instalado y funcionando (`docker info` sin errores)
- Imagen base descargada:

```bash
docker pull alpine:latest
```

- Opcional: `capsh` instalado en el host para decodificar bitmasks de
  capabilities (paquete `libcap` o `libcap2-bin` segun la distribucion)

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Capabilities por defecto | Listar capabilities del contenedor, decodificar el bitmask, identificar que se mantiene y que se elimina |
| 2 | `--cap-drop` individual | Eliminar capabilities una a una y observar que operaciones fallan |
| 3 | Drop ALL + add minimo | Patron de minimo privilegio: eliminar todo y anadir solo lo necesario |
| 4 | seccomp | Verificar que seccomp filtra syscalls, observar que syscalls estan bloqueadas por defecto |
| 5 | `--privileged` vs capabilities especificas | Comparar el modo privilegiado con el uso granular de capabilities |

## Archivos

```
labs/
├── README.md              <- Guia paso a paso (documento principal del lab)
└── Dockerfile.captest     <- Imagen con programa que necesita capabilities especificas (Parte 3)
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte. Cada parte
incluye los comandos a ejecutar, las salidas esperadas, y el analisis de
resultados.

## Tiempo estimado

~25 minutos (las 5 partes completas).

## Recursos Docker creados

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| `lab-captest` | Imagen | 3 | Si |
| `cap-default` | Contenedor | 1 | Si (en la parte) |
| `priv-test` | Contenedor | 5 | Si (en la parte) |

Ningun recurso persiste despues de completar el lab. La limpieza se hace
al final de cada parte y en la seccion de limpieza final.
