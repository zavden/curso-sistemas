# Labs — Jobs

## Descripcion

Laboratorio practico para gestionar procesos en background y
foreground: fg, bg, Ctrl+Z, jobs, nohup, disown, y entender
cuando el proceso sobrevive al cierre de la terminal.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Control de jobs | &, Ctrl+Z, fg, bg, jobs |
| 2 | Sobrevivir al cierre de terminal | nohup, disown, SIGHUP |
| 3 | Comparacion de metodos | nohup vs disown vs tmux/screen |

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
