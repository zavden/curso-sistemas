# Labs — Directorios raiz

## Descripcion

Laboratorio practico para explorar la jerarquia de directorios raiz de
Linux, identificar el proposito de cada directorio, verificar symlinks
del /usr merge, y comparar la estructura entre Debian y AlmaLinux.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Estructura raiz | Directorios de primer nivel y su proposito |
| 2 | Localizar software | which, /usr/bin, /usr/local/bin, headers |
| 3 | /tmp y permisos especiales | tmpfs, sticky bit |
| 4 | Comparar distros | Diferencias Debian vs AlmaLinux |

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
Los comandos se ejecutan dentro de los contenedores del curso.

## Tiempo estimado

~15 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker. Usa los contenedores existentes.
