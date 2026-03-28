# Labs — awk

## Descripcion

Laboratorio practico para dominar awk: campos ($1, $NF, $0),
separadores (-F, OFS), patrones (regex, rangos), BEGIN/END,
NR/NF/FNR, variables, arrays asociativos, funciones de string,
y diferencias gawk vs mawk.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Campos y separadores | $1, $NF, -F, OFS, print vs printf |
| 2 | Patrones y bloques | BEGIN/END, NR/NF, regex, rangos |
| 3 | Arrays y funciones | Arrays asociativos, string functions, recetas |

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
