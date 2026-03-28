# Labs -- Punto flotante

## Descripcion

Laboratorio para explorar los tipos de punto flotante en C: tamanos,
precision, errores de representacion, comparacion correcta y valores
especiales de IEEE 754. Se compila y ejecuta codigo que revela el
comportamiento real de float, double y long double.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Tamanos y precision | sizeof, float.h, digitos significativos, representacion de 0.1 |
| 2 | Errores de representacion | 0.1 + 0.2, acumulacion de errores, perdida por magnitud |
| 3 | Comparacion de flotantes | epsilon absoluto, epsilon relativo, FLT_EPSILON |
| 4 | Valores especiales | Infinity, NaN, subnormales, funciones de clasificacion |

## Archivos

```
labs/
├── README.md           <- Ejercicios paso a paso
├── sizes_precision.c   <- Tamanos, rangos y precision de los tres tipos
├── rounding_errors.c   <- Errores de representacion y acumulacion
├── compare_floats.c    <- Comparacion correcta con epsilon
└── special_values.c    <- Infinity, NaN, subnormales, clasificacion
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
