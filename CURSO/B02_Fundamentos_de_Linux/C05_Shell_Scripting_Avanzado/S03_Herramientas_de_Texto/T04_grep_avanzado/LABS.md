# Labs — grep avanzado

## Descripcion

Laboratorio practico para dominar grep mas alla de lo basico:
BRE/ERE/PCRE, flags avanzados (-o, -w, -q, -c, -l, -L, -m),
contexto (-A, -B, -C), busqueda recursiva (--include, --exclude-dir),
PCRE con lookahead/lookbehind, y patrones comunes.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Modos y flags | BRE/ERE/PCRE, -o, -w, -q, -c, -l |
| 2 | Contexto y recursion | -A, -B, -C, -r, --include, --exclude-dir |
| 3 | PCRE y patrones | Lookahead/lookbehind, patrones comunes, rendimiento |

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
