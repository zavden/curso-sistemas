# Labs — Loops

## Descripcion

Laboratorio para practicar los tres tipos de loops en C (for, while, do-while),
las sentencias de control break y continue, y patrones comunes como recorrer
strings, busqueda con sentinel value y validacion de input.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | for — variaciones | Contar arriba/abajo, paso variable, multiples variables, partes opcionales |
| 2 | while y do-while | Diferencia semantica: while puede ejecutar 0 veces, do-while al menos 1 |
| 3 | break y continue | Salir de loops, saltar iteraciones, break en loops anidados |
| 4 | Patrones comunes | Recorrer strings, sentinel values, validacion de input con do-while |

## Archivos

```
labs/
├── README.md            <- Ejercicios paso a paso
├── for_variations.c     <- Variaciones del for: conteo, paso, dos variables
├── for_optional_parts.c <- Partes opcionales del for: sin init, sin update, loop infinito
├── while_vs_dowhile.c   <- Comparacion directa entre while y do-while
├── break_continue.c     <- break para buscar, continue para filtrar
├── nested_break.c       <- break en loops anidados y patron con flag
├── string_traverse.c    <- Recorrer strings con indice y con puntero
├── sentinel_search.c    <- Busqueda lineal con valor de retorno -1
└── input_validation.c   <- Validacion de input con do-while
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
