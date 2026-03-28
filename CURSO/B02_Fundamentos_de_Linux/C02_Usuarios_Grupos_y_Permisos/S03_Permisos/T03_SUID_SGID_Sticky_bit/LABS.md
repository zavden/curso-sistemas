# Labs — SUID, SGID, Sticky bit

## Descripcion

Laboratorio practico para explorar los tres bits especiales de
permisos: encontrar programas SUID, configurar directorios con
SGID para trabajo compartido, y verificar la proteccion del sticky
bit en /tmp.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | SUID | Encontrar programas SUID, como funciona passwd |
| 2 | SGID en directorios | Herencia de grupo, directorio compartido |
| 3 | Sticky bit | Proteccion en /tmp, combinar bits especiales |

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
