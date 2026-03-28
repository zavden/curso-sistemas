# Labs — apt vs apt-get

## Descripcion

Laboratorio practico para entender la familia de herramientas APT:
comparar apt y apt-get, descubrir apt-cache y apt-mark, y saber
cuando usar cada uno.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | La familia APT | Jerarquia dpkg < apt-get/apt-cache < apt |
| 2 | apt vs apt-get | Mismos comandos, diferente output y UX |
| 3 | apt-cache, apt-mark y exclusivos | Consultas, holds, comandos unicos |

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
