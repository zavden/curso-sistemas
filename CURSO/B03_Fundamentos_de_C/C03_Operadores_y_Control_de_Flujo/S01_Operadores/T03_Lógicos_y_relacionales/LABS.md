# Labs — Logicos y relacionales

## Descripcion

Laboratorio para experimentar con operadores relacionales, logicos y sus
trampas clasicas. Se observa truthiness en C, short-circuit evaluation con
side effects, la diferencia entre operadores logicos y bitwise, y errores
comunes de precedencia y comparacion.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Operadores relacionales y truthiness | Resultado int de comparaciones, sizeof, valores truthy/falsy |
| 2 | Operadores logicos y short-circuit | &&, \|\|, !, evaluacion corta con side effects |
| 3 | Errores comunes y precedencia | = vs ==, floats con ==, signed vs unsigned, & vs &&, precedencia, De Morgan |

## Archivos

```
labs/
├── README.md            <- Ejercicios paso a paso
├── relational.c         <- Operadores relacionales basicos
├── truthiness.c         <- Valores truthy y falsy en C
├── short_circuit.c      <- Short-circuit evaluation y side effects
├── logical_vs_bitwise.c <- Diferencia entre & y && (y | vs ||)
├── traps.c              <- Trampas clasicas: = vs ==, floats, signed/unsigned
└── precedence.c         <- Precedencia de operadores y leyes de De Morgan
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~25 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Ejecutables compilados | Binarios | Si (limpieza final) |
