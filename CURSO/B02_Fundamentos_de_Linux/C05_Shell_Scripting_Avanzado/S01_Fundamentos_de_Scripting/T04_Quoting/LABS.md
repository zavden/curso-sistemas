# Labs — Quoting

## Descripcion

Laboratorio practico para dominar el quoting en bash: comillas dobles
vs simples, $'...' (ANSI-C quoting), word splitting, globbing
accidental, y las diferencias entre [[ ]] y [ ].

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Comillas dobles y simples | Expansion vs literal, $'...' ANSI-C |
| 2 | Word splitting y globbing | Bugs por falta de comillas, IFS |
| 3 | [[ ]] vs [ ] | Diferencias, pattern matching, regex |

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
