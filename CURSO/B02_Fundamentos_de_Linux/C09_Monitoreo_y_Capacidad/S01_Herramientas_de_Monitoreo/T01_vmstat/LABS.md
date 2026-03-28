# Labs — vmstat

## Descripcion

Laboratorio practico para vmstat: lectura en vivo (snapshot vs
intervalo), interpretacion de columnas (procs r/b, memory, swap
si/so, io bi/bo, system in/cs, cpu us/sy/id/wa/st), primera linea
como promedio, opciones utiles (-S, -w, -t, -a, -d), patrones de
diagnostico (CPU saturada, presion de memoria, thrashing, I/O wait),
y uso en scripts de monitoreo.

## Prerequisitos

- Entorno del curso levantado (debian-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Lectura basica | Snapshot, intervalo, primera linea, columnas |
| 2 | Interpretacion | procs, memory, swap, io, system, cpu |
| 3 | Opciones y scripts | -S, -w, -t, -a, -d, patrones, scripts |

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
