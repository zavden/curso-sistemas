# Labs — Patron clasico: configure, make, make install

## Descripcion

Laboratorio practico para entender el patron clasico de compilacion
desde fuente: ./configure (deteccion de sistema y generacion de
Makefiles), make (compilacion), make install (instalacion), --prefix,
y otros build systems (CMake, Meson).

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | El proceso completo | configure, make, make install paso a paso |
| 2 | --prefix y make -j | Controlar destino, compilar en paralelo |
| 3 | Otros build systems | CMake, Meson, Makefile directo, ldconfig |

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
