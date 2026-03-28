# Labs -- Compilacion condicional

## Descripcion

Laboratorio para practicar las directivas de compilacion condicional del
preprocesador de C: `#ifdef`, `#ifndef`, `#if`, `#elif`, `#else`, `#endif`,
`defined()`, `gcc -D`, feature macros, `NDEBUG`/`assert`, `#error`/`#warning`,
y `gcc -E` para inspeccionar el resultado del preprocesamiento.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | #ifdef, #ifndef, defined(), gcc -D | Existencia vs valor de macros, definir macros desde la linea de compilacion |
| 2 | #if, #elif, #else, #endif | Cadenas condicionales, expresiones aritmeticas, macros no definidos valen 0 |
| 3 | Feature macros | _POSIX_C_SOURCE, _GNU_SOURCE, deteccion de plataforma/compilador, funciones que se desbloquean |
| 4 | NDEBUG y assert | Desactivar asserts con -DNDEBUG, side effects en asserts |
| 5 | #error y #warning | Detener compilacion con #error, emitir advertencias con #warning |
| 6 | gcc -E | Inspeccionar la salida del preprocesador para ver que codigo se incluye |

## Archivos

```
labs/
├── README.md            <- Ejercicios paso a paso
├── ifdef_basics.c       <- #ifdef, #ifndef, defined(), existencia vs valor, gcc -D
├── conditional_chain.c  <- #if/#elif/#else cadenas, defined() combinado, macros no definidos
├── feature_macros.c     <- _POSIX_C_SOURCE, _GNU_SOURCE, deteccion de plataforma
├── ndebug_assert.c      <- NDEBUG, assert(), side effects en asserts
├── error_warning.c      <- #error para requerir macros, #warning para deprecaciones
└── preprocess_view.c    <- Archivo pequeno para examinar salida de gcc -E
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
| Ejecutables compilados (ifdef_basics, conditional_chain, feature_macros, ndebug_assert, error_warning, preprocess_view) | Binarios | Si (limpieza final) |
