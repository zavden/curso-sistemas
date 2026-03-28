# Labs — Senales

## Descripcion

Laboratorio practico para entender las senales Unix: SIGTERM,
SIGKILL, SIGINT, SIGHUP, SIGSTOP/SIGCONT, y su comportamiento
en procesos y contenedores.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Senales basicas | SIGTERM, SIGKILL, SIGINT, listar senales |
| 2 | SIGHUP, SIGSTOP, SIGCONT | Reload, pausar, resumir procesos |
| 3 | Senales en contenedores | PID 1 y proteccion de senales |

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
