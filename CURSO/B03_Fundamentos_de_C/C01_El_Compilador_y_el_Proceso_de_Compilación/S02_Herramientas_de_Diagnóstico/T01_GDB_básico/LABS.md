# Labs -- GDB basico

## Descripcion

Laboratorio practico para usar GDB (GNU Debugger): poner breakpoints,
ejecutar paso a paso con next/step, inspeccionar variables y arrays,
depurar un segfault con backtrace, y usar watchpoints para rastrear
cambios en variables.

## Prerequisitos

- gcc instalado
- gdb instalado
- Estar en el directorio `labs/` de este topico

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|---|---|---|
| 1 | Breakpoints y ejecucion paso a paso | break, run, next, step, print, finish, continue |
| 2 | Inspeccionar datos | print con arrays, formato hex, set var, list |
| 3 | Depurar un segfault con backtrace | bt, frame, identificar NULL pointer |
| 4 | Watchpoints | watch, watchpoints condicionales, encontrar un bug |

## Archivos

```
labs/
├── README.md        <- Guia paso a paso (documento principal del lab)
├── debug_basic.c    <- Programa con factorial() y process_array()
├── debug_crash.c    <- Programa que crashea por NULL pointer
└── debug_watch.c    <- Programa con bug: counter se vuelve negativo
```

## Como ejecutar

```bash
cd labs/
```

Seguir las instrucciones de `labs/README.md` parte por parte.
Los comandos se ejecutan en la terminal local (no en contenedores).

## Tiempo estimado

~25 minutos.

## Recursos que se crean

| Recurso | Tipo | Parte | Se elimina al final |
|---|---|---|---|
| debug_basic | binario | 1, 2 | Si |
| debug_crash | binario | 3 | Si |
| debug_watch | binario | 4 | Si |
