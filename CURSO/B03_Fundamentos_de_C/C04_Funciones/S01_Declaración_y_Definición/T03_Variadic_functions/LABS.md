# Labs — Variadic functions

## Descripcion

Laboratorio para practicar funciones variadicas en C usando `<stdarg.h>`.
Se implementan los tres patrones clasicos para determinar cuantos argumentos
recibio la funcion: conteo explicito, sentinel, y format string.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | stdarg.h basico | va_list, va_start, va_arg, va_end con conteo explicito |
| 2 | Sentinel | Recorrer argumentos hasta un marcador (-1, NULL) |
| 3 | Format string | Mini-printf que interpreta %d, %s, %c y %% |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── sum_variadic.c   <- Funcion sum con conteo explicito (parte 1)
├── sentinel.c       <- Funciones con sentinel -1 y NULL (parte 2)
└── mini_printf.c    <- Mini-printf con format string (parte 3)
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
| sum_variadic | Ejecutable | Si (limpieza final) |
| sentinel | Ejecutable | Si (limpieza final) |
| mini_printf | Ejecutable | Si (limpieza final) |
