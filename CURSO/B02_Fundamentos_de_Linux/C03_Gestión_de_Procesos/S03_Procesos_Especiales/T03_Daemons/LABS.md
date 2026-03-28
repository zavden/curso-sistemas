# Labs — Daemons

## Descripcion

Laboratorio practico para entender daemons: identificar procesos
sin terminal, inspeccionar sus caracteristicas (TTY=?, PPID=1,
cwd=/), y comparar el metodo clasico (doble fork) con el moderno
(systemd).

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Identificar daemons | TTY=?, PPID, session leader |
| 2 | Inspeccionar un daemon | /proc, fd, cwd, exe |
| 3 | Doble fork vs systemd | Metodo clasico vs moderno |

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
