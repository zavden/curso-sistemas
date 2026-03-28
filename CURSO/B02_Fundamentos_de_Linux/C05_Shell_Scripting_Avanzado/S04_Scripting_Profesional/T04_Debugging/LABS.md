# Labs — Debugging

## Descripcion

Laboratorio practico para dominar debugging de scripts bash:
set -x (tracing), PS4 personalizado, BASH_XTRACEFD, bash -n
(verificacion de sintaxis), ShellCheck (analisis estatico),
debug condicional (DEBUG=1), trap ERR/DEBUG para diagnostico,
y variables de diagnostico (LINENO, FUNCNAME, BASH_SOURCE, caller).

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | set -x y PS4 | Tracing, PS4, BASH_XTRACEFD, selectivo |
| 2 | bash -n y ShellCheck | Sintaxis, linting, directivas inline |
| 3 | Debug condicional | DEBUG=1, trap DEBUG/ERR, script debuggeable |

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
