# Labs -- _Noreturn y _Static_assert

## Descripcion

Laboratorio para explorar los especificadores `_Noreturn` y `_Static_assert`
de C11: funciones que nunca retornan, validaciones en tiempo de compilacion,
warnings del compilador, y un caso practico que combina ambos conceptos en un
mini-framework de manejo de errores.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | _Noreturn | Funciones que no retornan, warning si retornan, eliminacion de warning en funciones que llaman a die() |
| 2 | _Static_assert | Validaciones en tiempo de compilacion: sizeof, alignment, enum-array sync |
| 3 | Caso practico | Mini-framework de error handling combinando noreturn y static_assert |

## Archivos

```
labs/
├── README.md              <- Ejercicios paso a paso
├── noreturn_basic.c       <- Funcion die() con noreturn, elimina warning
├── noreturn_without.c     <- Misma logica SIN noreturn (produce warning)
├── noreturn_bad.c         <- Funcion noreturn que retorna (UB)
├── static_assert_basic.c  <- Validaciones de plataforma, struct, enum, alignment
├── static_assert_fail.c   <- Assertion que falla intencionalmente
└── error_framework.c      <- Combinacion de noreturn + static_assert
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
| Ejecutables compilados | Binarios | Si (limpieza final) |
