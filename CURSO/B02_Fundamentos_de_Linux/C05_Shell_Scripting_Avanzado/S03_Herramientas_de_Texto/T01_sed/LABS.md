# Labs — sed

## Descripcion

Laboratorio practico para dominar sed: sustitucion (s///), flags
(g, i, p), delimitadores alternativos, direcciones (lineas, regex,
rangos), comandos (d, p, i, a, c, y), -i in-place, backreferences,
hold space, y recetas utiles.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Sustitucion y flags | s///, g, i, p, delimitadores, -E |
| 2 | Direcciones y comandos | Lineas, regex, rangos, d/p/i/a/c/y, -i |
| 3 | Backreferences y hold space | \1, &, -E, h/H/g/G/x, recetas |

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
