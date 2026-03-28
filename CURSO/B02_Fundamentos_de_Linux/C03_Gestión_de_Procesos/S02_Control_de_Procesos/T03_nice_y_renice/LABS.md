# Labs — nice y renice

## Descripcion

Laboratorio practico para gestionar prioridades de procesos:
nice para lanzar con prioridad alterada, renice para cambiar
en caliente, y ionice para prioridad de I/O.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | nice | Valores -20 a +19, efecto en PR, restricciones |
| 2 | renice | Cambiar prioridad en caliente, restricciones de usuario |
| 3 | ionice | Clases de I/O, scheduler dependency |

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
