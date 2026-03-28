# Labs — cut, sort, uniq, tr, tee, xargs

## Descripcion

Laboratorio practico para dominar las herramientas de texto de
linea de comandos: cut (-d, -f, -c), sort (-n, -r, -k, -h, -V, -u),
uniq (-c, -d, -u), tr (translate, -d, -s, -c), tee (-a, sudo),
xargs (-I, -0, -n, -P), y composicion con pipelines.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | cut, sort, uniq | Campos, ordenamiento, frecuencias |
| 2 | tr y tee | Transliteracion, -d, -s, tee -a, sudo |
| 3 | xargs y pipelines | -I {}, -0, -P paralelo, composicion |

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

~20 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
