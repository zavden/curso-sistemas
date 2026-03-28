# Labs — kill, killall, pkill

## Descripcion

Laboratorio practico para terminar procesos con kill (por PID),
killall (por nombre exacto), pkill (por patron), y verificar
con pgrep antes de actuar.

## Prerequisitos

- Entorno del curso levantado (debian-dev y alma-dev corriendo)
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | kill | Por PID, grupos de procesos, builtin vs externo |
| 2 | killall | Por nombre exacto, opciones -i, -w, -r |
| 3 | pkill y pgrep | Patrones, filtros -u/-t/-P, pgrep para verificar |

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
