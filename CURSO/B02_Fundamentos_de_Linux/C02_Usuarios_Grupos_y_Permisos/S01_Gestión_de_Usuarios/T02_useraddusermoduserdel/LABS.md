# Labs — useradd, usermod, userdel

## Descripcion

Laboratorio practico para crear, modificar y eliminar usuarios con
las herramientas de bajo nivel. Explorar defaults, skeleton, el error
comun de -G sin -a, y la diferencia entre Debian y AlmaLinux.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | useradd | Crear usuarios, flags, defaults, skeleton |
| 2 | usermod | Modificar shell, grupos, bloqueo, el error -G sin -a |
| 3 | userdel y comparacion | Eliminar usuarios, archivos huerfanos, Debian vs RHEL |

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
