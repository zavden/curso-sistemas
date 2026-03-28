# Labs -- Prototipos

## Descripcion

Laboratorio para comprender la diferencia entre declaracion y definicion de
funciones en C, el rol de los prototipos en la verificacion de tipos, la
organizacion de prototipos en headers con include guards, y los errores
clasicos de firma y de `()` vs `(void)`.

## Prerequisitos

- GCC instalado (`gcc --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Declaracion vs definicion | Prototipo antes de main, llamar sin prototipo (implicit declaration), definir antes de usar |
| 2 | Headers y compilacion multi-archivo | Organizar prototipos en .h, include guards, compilar y enlazar archivos separados |
| 3 | Errores clasicos | Firma incorrecta (argumentos de mas/menos), tipo incorrecto, `()` vs `(void)` |

## Archivos

```
labs/
├── README.md            <- Ejercicios paso a paso
├── with_prototype.c     <- Programa con prototipos antes de main
├── no_prototype.c       <- Programa sin prototipos (provoca error)
├── def_before_use.c     <- Definicion antes del uso (sin prototipo)
├── math_ops.h           <- Header con prototipos e include guard
├── math_ops.c           <- Implementacion de funciones matematicas
├── main_multi.c         <- Programa principal que usa math_ops.h
├── no_header.c          <- main sin incluir el header (provoca error)
├── empty_vs_void.c      <- Diferencia entre () y (void) en prototipos
├── bar_wrong_args.c     <- Llamar con argumentos a funcion (void)
├── wrong_signature.c    <- Numero incorrecto de argumentos
└── wrong_type.c         <- Tipo incorrecto de argumento
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
| *.i | Archivos preprocesados | Si (limpieza final) |
