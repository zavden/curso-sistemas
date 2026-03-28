# Labs -- Comportamiento por compilador

## Descripcion

Laboratorio para demostrar como el estandar seleccionado con -std afecta la
compilacion: extensiones GNU que GCC acepta silenciosamente, features de C99
que no existen en C89, features de C11 que no existen en C99, y por que
-std=c11 -Wpedantic es la combinacion recomendada para el curso.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Extensiones GNU vs C estandar | GCC acepta extensiones no portables sin avisar; -Wpedantic las expone |
| 2 | Diferencias entre estandares | C89 vs C99 vs C11: que features se agregan en cada version |
| 3 | El estandar del curso: -std=c11 | Por que -std=c11 -Wpedantic es la base recomendada |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── extensions.c     <- Programa con 5 extensiones GNU: statement expression, nested function, binary literal, zero-length array, case range
├── c99_features.c   <- Programa con features de C99: // comments, mixed declarations, VLA, designated initializers, for-loop declarations, stdint.h
└── c11_features.c   <- Programa con features de C11: _Static_assert, _Generic, anonymous struct/union, _Alignof, _Noreturn
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~15 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Ejecutables compilados | Binarios | Si (limpieza por parte y final) |
