# Labs -- #define

## Descripcion

Laboratorio para practicar las directivas `#define` y `#undef` del preprocesador
de C: macros sin parametros (object-like), macros con parametros (function-like),
operadores de stringizing (`#`) y token pasting (`##`), macros multilinea,
visualizacion de la expansion con `gcc -E`, y definicion desde linea de comandos
con `-D`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Object-like macros | Sustitucion de texto, trampa de parentesis, `gcc -E` para ver expansion |
| 2 | Function-like macros | Parametros, regla de parentesis, diferencia con funciones |
| 3 | #undef | Eliminar macros, redefinir sin warning |
| 4 | Stringizing (#) y token pasting (##) | Convertir tokens a strings, concatenar tokens, XSTRINGIFY |
| 5 | Macros multilinea y -D | Macros con do-while(0), definir macros desde linea de comandos |

## Archivos

```
labs/
├── README.md             <- Ejercicios paso a paso
├── object_macros.c       <- Macros sin parametros, trampa de precedencia
├── function_macros.c     <- Macros con parametros, regla de parentesis
├── undef_demo.c          <- #undef y redefinicion de macros
├── stringify_paste.c     <- Operadores # y ##, XSTRINGIFY
└── multiline_cmdline.c   <- Macros multilinea, uso con -D
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
| Ejecutables compilados (object_macros, function_macros, undef_demo, stringify_paste, multiline_cmdline) | Binarios | Si (limpieza final) |
| Archivos de salida del preprocesador (*.i) | Texto | Si (limpieza final) |
