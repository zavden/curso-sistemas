# Labs — Contenedor a mano con unshare

## Descripcion

Laboratorio practico para entender como se construye un contenedor
desde cero: el comando unshare, que flag crea cada namespace,
la integracion de namespaces + rootfs + cgroups, y la comparacion
con Docker.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | unshare y sus flags | Cada flag, que namespace crea |
| 2 | Integracion | namespaces + rootfs = contenedor |
| 3 | Comparacion con Docker | Que falta vs Docker real, tabla |

## Archivos

```
labs/
└── README.md    ← Guia paso a paso (documento principal del lab)
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.

## Tiempo estimado

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
