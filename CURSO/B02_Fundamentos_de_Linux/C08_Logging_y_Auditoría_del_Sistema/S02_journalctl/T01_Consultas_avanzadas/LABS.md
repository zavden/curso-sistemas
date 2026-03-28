# Labs — Consultas avanzadas

## Descripcion

Laboratorio practico para consultas avanzadas de journalctl:
combinar filtros (AND implicito, OR entre unidades, operador +),
filtros de tiempo avanzados (rangos, cursores), formatos de salida
(json, verbose, cat, short-iso, short-precise), consultas de
analisis (contar errores, timelines, kernel, grep), uso en scripts
de monitoreo, journals de otros sistemas, y rendimiento.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Filtros combinados | AND, OR, operador +, patrones, tiempo |
| 2 | Formatos de salida | json, verbose, cat, short-*, export |
| 3 | Analisis y scripts | Contar errores, timelines, grep, monitoreo |

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

~20 minutos.

## Recursos Docker que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| Ninguno | — | — | — |

Este lab no crea recursos Docker.
