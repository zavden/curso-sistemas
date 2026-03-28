# Labs — Variables y expansiones

## Descripcion

Laboratorio practico para dominar las expansiones de parametros de
bash: valores por defecto (${:-}, ${:=}, ${:+}, ${:?}), manipulacion
de strings (${#}, ${##}, ${%%}, ${//}, ${^^}, ${,,}), aritmetica
con $(()) y sustitucion de comandos con $().

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Defaults y asignacion | ${:-}, ${:=}, ${:+}, ${:?} |
| 2 | Manipulacion de strings | ${#}, ${##}, ${%%}, ${//}, ${^^}, ${,,} |
| 3 | Aritmetica y sustitucion | $(( )), $(), $@ vs $* |

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
