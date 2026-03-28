# Labs -- Comportamiento indefinido (UB)

## Descripcion

Laboratorio para observar, provocar y corregir comportamiento indefinido en C.
Se compila el mismo codigo con distintas combinaciones de flags y niveles de
optimizacion para demostrar que el UB no es "resultado aleatorio" -- es una
licencia que el compilador usa activamente para transformar el programa.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Sanitizers disponibles (`-fsanitize=undefined`, `-fsanitize=address`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | UB en accion | Compilar con y sin warnings/sanitizers, ver como UB pasa desapercibido |
| 2 | El optimizador explota UB | Mismo codigo produce resultados distintos con -O0 vs -O2 |
| 3 | Corregir UB | Patrones seguros que reemplazan cada UB, verificados con sanitizers |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── ub_showcase.c    <- 4 tipos de UB clasicos (overflow, out-of-bounds, uninit, shift)
├── ub_optimizer.c   <- El optimizador elimina codigo basandose en UB
└── ub_fixed.c       <- Version corregida de ub_showcase.c
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~20 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Ejecutables compilados | Binarios | Si (limpieza por parte y final) |
| Archivos assembly (.s) | Intermedios | Si (limpieza parte 2 y final) |
