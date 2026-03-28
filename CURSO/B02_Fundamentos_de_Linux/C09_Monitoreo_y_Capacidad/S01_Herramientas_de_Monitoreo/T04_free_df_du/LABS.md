# Labs — free, df, du

## Descripcion

Laboratorio practico para free, df y du: free (columnas total/used/
free/shared/buff-cache/available, -h, -w, -s, scripts de alerta),
df (uso por filesystem, -h, -i inodos, -x exclusion, --output,
discrepancia df vs du), du (por directorio, -sh, --max-depth, -x,
top consumidores, ncdu), y flujo completo de diagnostico de disco
lleno.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | free — memoria | Columnas, available, buff/cache, swap, scripts |
| 2 | df — filesystems | Uso, inodos, exclusion, discrepancia |
| 3 | du — directorios | Tamaño, profundidad, top consumidores, flujo |

## Archivos

```
labs/
└── README.md    <- Guia paso a paso (documento principal del lab)
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
