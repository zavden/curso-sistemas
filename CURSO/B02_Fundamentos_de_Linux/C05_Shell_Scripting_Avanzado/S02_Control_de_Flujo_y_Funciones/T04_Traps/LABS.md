# Labs — Traps

## Descripcion

Laboratorio practico para dominar traps en bash: trap EXIT para
cleanup, trap INT/TERM para manejo de senales, trap ERR para
diagnostico con stack trace, trap DEBUG para tracing, herencia
en subshells, y el patron de lockfile.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | trap EXIT | Cleanup automatico, archivos temporales |
| 2 | trap INT/TERM/ERR | Senales, stack trace, diagnostico |
| 3 | trap DEBUG y avanzado | Tracing, herencia en subshells, lockfile |

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
