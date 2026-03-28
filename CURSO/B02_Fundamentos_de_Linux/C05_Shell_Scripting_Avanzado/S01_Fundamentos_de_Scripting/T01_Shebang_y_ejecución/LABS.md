# Labs — Shebang y ejecucion

## Descripcion

Laboratorio practico para entender el shebang (#!/bin/bash vs
#!/usr/bin/env bash), la diferencia entre sh y bash (dash en Debian),
chmod +x, las distintas formas de ejecutar un script (./script vs
source vs bash), y la relacion con PATH.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Shebang | #!/bin/bash vs #!/usr/bin/env bash, sh→dash |
| 2 | Formas de ejecucion | ./script vs source vs bash, chmod +x |
| 3 | PATH y seguridad | Resolucion de interpretes, PATH hardcoded vs env |

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

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
