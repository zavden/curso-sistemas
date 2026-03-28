# Labs — ps

## Descripcion

Laboratorio practico para inspeccionar procesos con ps: formatos
BSD (aux) vs System V (-ef), codigos de estado STAT, formato
personalizado (-o), selectores de procesos, y pstree.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | ps aux vs ps -ef | Diferencias de formato, campos, filtros |
| 2 | Codigos STAT | R, S, D, Z, T y modificadores s, l, +, < |
| 3 | Formato personalizado y selectores | -o, -p, -u, -C, --sort |

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
