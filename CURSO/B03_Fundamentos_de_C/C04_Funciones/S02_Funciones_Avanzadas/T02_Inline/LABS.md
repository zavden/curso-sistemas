# Labs -- Inline

## Descripcion

Laboratorio para observar el efecto de `inline` en el assembly generado,
practicar `static inline` en headers para multiples translation units, y
verificar que el compilador decide automaticamente cuando hacer inline con
`-O2`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Herramientas binutils: `nm`, `objdump` (incluidas con GCC)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Inline basico | Comparar assembly con y sin `inline` -- presencia/ausencia de `call` y del simbolo global |
| 2 | Inline en headers | Definir `static inline` en un `.h` usado por multiples `.c` -- sin errores de enlace |
| 3 | El compilador decide | Con `-O2`, GCC inlinea funciones pequenas sin que se lo pidan y respeta las grandes |

## Archivos

```
labs/
├── README.md          <- Ejercicios paso a paso
├── square_regular.c   <- Funcion normal (sin inline) para la parte 1
├── square_inline.c    <- Funcion static inline para la parte 1
├── math_inline.h      <- Header con funciones static inline para la parte 2
├── caller_a.c         <- Translation unit A que usa math_inline.h
├── caller_b.c         <- Translation unit B que usa math_inline.h
├── main_multi.c       <- main() que enlaza caller_a y caller_b
└── auto_inline.c      <- Funcion pequena y funcion grande para la parte 3
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
| *.s | Archivos assembly intermedios | Si (limpieza final) |
| *.o | Archivos objeto | Si (limpieza final) |
| Ejecutables compilados | Binarios | Si (limpieza final) |
