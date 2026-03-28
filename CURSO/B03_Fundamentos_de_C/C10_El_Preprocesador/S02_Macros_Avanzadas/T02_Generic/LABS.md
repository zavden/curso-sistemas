# Labs -- _Generic

## Descripcion

Laboratorio para practicar `_Generic` de C11: seleccion de expresiones por tipo
en tiempo de compilacion, macros genericas para imprimir valores, obtener nombres
de tipo, calcular valor absoluto y maximo, y manejo de tipos no soportados.

## Prerequisitos

- GCC con soporte C11 instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Sintaxis basica de _Generic | Seleccion por tipo, print_val generico, formato correcto por tipo |
| 2 | type_name y PRINT_VAR | Obtener nombre del tipo como string, combinar stringify con _Generic |
| 3 | abs y max genericos | Despacho a funciones por tipo, abs_val y max type-safe |
| 4 | default y errores de tipo | Clausula default, error de compilacion sin default, tipos no soportados |
| 5 | Combinacion con variadic macros | PRINT_VALS variadic que usa _Generic internamente |

## Archivos

```
labs/
├── README.md           <- Ejercicios paso a paso
├── print_val.c         <- Macro print_val basica con _Generic
├── type_name.c         <- Macros type_name y PRINT_VAR con stringify
├── abs_max.c           <- abs_val y max genericos con despacho a funciones
├── default_error.c     <- Clausula default y error de tipo no soportado
├── no_default_error.c  <- Ejemplo que falla sin default (no compila)
└── variadic_generic.c  <- Combinacion de _Generic con variadic macros
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
| Ejecutables compilados (print_val, type_name, abs_max, default_error, variadic_generic) | Binarios | Si (limpieza final) |
