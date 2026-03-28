# Labs — Exit codes

## Descripcion

Laboratorio practico para entender los codigos de salida en bash:
$?, convenciones (0/1/2/126/127/128+N), set -euo pipefail,
PIPESTATUS, y patrones de manejo de errores.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Exit codes y convenciones | $?, codigos estandar, && y \|\| |
| 2 | set -euo pipefail | Strict mode, excepciones, pipefail |
| 3 | PIPESTATUS y patrones | Estado de pipelines, patrones de error |

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
