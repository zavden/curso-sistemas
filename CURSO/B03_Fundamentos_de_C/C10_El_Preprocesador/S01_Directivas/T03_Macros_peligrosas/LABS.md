# Labs -- Macros peligrosas

## Descripcion

Laboratorio para demostrar los problemas clasicos de las macros del
preprocesador: bugs de precedencia por falta de parentesis, evaluacion multiple
de argumentos con side effects, macros multi-statement sin do-while(0), ausencia
de verificacion de tipos, y como las statement expressions de GCC y las
funciones inline resuelven estos problemas.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Precedencia sin parentesis | Expansiones incorrectas con gcc -E, resultados erroneos visibles |
| 2 | Evaluacion multiple | Side effects (++, llamadas a funciones) se ejecutan mas de una vez |
| 3 | do-while(0) | Macros multi-statement que rompen if/else sin do-while(0) |
| 4 | Type safety | Macros aceptan tipos incompatibles sin error; funciones inline no |
| 5 | Statement expressions e inline | GCC statement expressions como alternativa, comparacion con inline |

## Archivos

```
labs/
├── README.md        <- Ejercicios paso a paso
├── precedence.c     <- Macros sin/con parentesis, precedencia de operadores
├── double_eval.c    <- Evaluacion multiple con ++, funciones con side effects
├── do_while.c       <- Macros multi-statement: naked, braces, do-while(0)
├── type_safety.c    <- Macros vs funciones inline en verificacion de tipos
└── stmt_expr.c      <- Statement expressions de GCC, comparacion con inline
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
| Ejecutables compilados (precedence, double_eval, do_while, type_safety, stmt_expr) | Binarios | Si (limpieza final) |
