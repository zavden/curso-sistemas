# Labs — Locale

## Descripcion

Laboratorio practico para locale: formato de nombre (idioma_TERRITORIO
.CODIFICACION), variables LC_* (CTYPE, NUMERIC, TIME, COLLATE,
MONETARY, MESSAGES), precedencia (LC_ALL > LC_* > LANG), locale
C/POSIX (predecible, ASCII), configuracion del sistema (Debian:
/etc/default/locale, locale-gen; RHEL: /etc/locale.conf, localectl),
implicaciones en scripts (sort, grep, separador decimal), y UTF-8.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Variables LC_* y precedencia | LANG, LC_*, LC_ALL, C/POSIX |
| 2 | Configuracion del sistema | Debian vs RHEL, locale-gen, localectl |
| 3 | Implicaciones en scripts | sort, grep, decimal, UTF-8 |

## Archivos

```
labs/
└── README.md    <- Guia paso a paso (documento principal del lab)
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
