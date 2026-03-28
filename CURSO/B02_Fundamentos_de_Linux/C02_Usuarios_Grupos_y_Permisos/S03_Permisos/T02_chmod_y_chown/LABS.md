# Labs — chmod y chown

## Descripcion

Laboratorio practico para cambiar permisos con chmod (octal y
simbolica) y propietarios con chown/chgrp. Incluye permisos
recursivos correctos y peculiaridades de symlinks.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | chmod octal y simbolica | Ambas sintaxis, cuando usar cada una |
| 2 | chown y chgrp | Cambiar propietario, grupo, restricciones |
| 3 | Recursivo y edge cases | chmod -R, find + chmod, X mayuscula, symlinks |

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
