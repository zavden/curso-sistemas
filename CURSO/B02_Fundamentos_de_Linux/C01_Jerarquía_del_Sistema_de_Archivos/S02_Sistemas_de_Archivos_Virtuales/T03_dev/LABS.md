# Labs — /dev

## Descripcion

Laboratorio practico para explorar /dev: clasificar dispositivos de
bloque y caracter, usar los dispositivos especiales (null, zero,
urandom, full), y entender el /dev minimo de un contenedor.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Tipos de dispositivos | Bloque vs caracter, major/minor numbers |
| 2 | Dispositivos especiales | /dev/null, /dev/zero, /dev/urandom, /dev/full |
| 3 | Terminales y /dev en contenedores | pts, tty, /dev minimo |

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
