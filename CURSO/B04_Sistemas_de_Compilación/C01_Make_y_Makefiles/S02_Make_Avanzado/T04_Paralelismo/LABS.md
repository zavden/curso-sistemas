# Labs -- Paralelismo en Make

## Descripcion

Laboratorio para medir el speedup real de `make -j` frente a `-j1` con un
proyecto de 10 archivos .c, usar order-only prerequisites para crear
directorios de salida correctamente, observar como la salida se intercala con
`-j` y solucionarlo con `--output-sync`, proteger builds parciales con
`.DELETE_ON_ERROR`, y detectar dependencias faltantes que solo fallan con `-j`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- GNU Make 4.x (`make --version`)
- Comando `time` disponible (built-in del shell)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Speedup con -j | Comparar tiempos de compilacion entre -j1, -j2, -j4 y -j$(nproc) con 10 archivos .c |
| 2 | --output-sync | Observar salida intercalada con -j y resolverla con -O (--output-sync=target) |
| 3 | Order-only prerequisites | Diferencia entre mkdir en cada recipe vs order-only (\| build/); evitar recompilaciones por timestamp de directorio |
| 4 | .DELETE_ON_ERROR | Proteger contra archivos .o parcialmente generados tras un fallo de compilacion |
| 5 | Dependencias faltantes con -j | Provocar una race condition por dependencia no declarada; corregirla y verificar con -j |

## Archivos

```
labs/
├── README.md              <- Ejercicios paso a paso
├── funcs.h                <- Header comun: declaraciones de 10 funciones compute_*()
├── main_parallel.c        <- Programa principal que suma los 10 compute_*()
├── alpha.c                <- compute_alpha() con bucle de trabajo
├── bravo.c                <- compute_bravo()
├── charlie.c              <- compute_charlie()
├── delta.c                <- compute_delta()
├── echo_func.c            <- compute_echo()
├── foxtrot.c              <- compute_foxtrot()
├── golf.c                 <- compute_golf()
├── hotel.c                <- compute_hotel()
├── india.c                <- compute_india()
├── juliet.c               <- compute_juliet()
├── app_generated.c        <- Programa que depende de un header generado
├── generated.h.in         <- Template para generar generated.h
├── Makefile.speedup       <- Makefile para medir speedup (parte 1, 2)
├── Makefile.outputsync    <- Makefile con echo marcadores para observar intercalado (parte 2)
├── Makefile.no_orderonly   <- Makefile sin order-only: mkdir en cada recipe (parte 3)
├── Makefile.orderonly      <- Makefile con order-only prerequisites (parte 3)
├── Makefile.race           <- Makefile con dependencia faltante intencional (parte 5)
└── Makefile.race_fixed     <- Makefile con dependencia corregida (parte 5)
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~30 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| *.o (alpha.o, bravo.o, etc.) | Archivos objeto | Si (limpieza final) |
| parallel_demo | Ejecutable | Si (limpieza final) |
| build/ | Directorio de compilacion | Si (limpieza final) |
| generated.h | Header generado | Si (limpieza final) |
| app_generated.o, alpha.o | Archivos objeto (parte 5) | Si (limpieza final) |
| app_race | Ejecutable (parte 5) | Si (limpieza final) |
