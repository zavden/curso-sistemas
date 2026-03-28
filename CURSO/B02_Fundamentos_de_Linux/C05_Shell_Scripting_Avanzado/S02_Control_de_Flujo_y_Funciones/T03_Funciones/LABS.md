# Labs — Funciones

## Descripcion

Laboratorio practico para dominar funciones en bash: definicion,
argumentos ($1, $@, $#), local, return vs exit, nameref (local -n),
patrones de retorno, y funciones profesionales (die/log/require_cmd,
main wrapper).

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Definicion y argumentos | Sintaxis, $1 $@ $#, local, scope |
| 2 | Retorno y nameref | return vs exit, echo+$(), local -n |
| 3 | Patrones profesionales | die/log/require, main wrapper, composicion |

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
