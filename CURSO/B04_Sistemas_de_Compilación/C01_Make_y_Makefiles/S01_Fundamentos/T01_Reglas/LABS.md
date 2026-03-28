# Labs -- Reglas de Make

## Descripcion

Laboratorio para dominar la anatomia de una regla de Make (target, prerequisites,
recipe), experimentar el error "missing separator" por usar espacios en vez de
TAB, y practicar compilacion separada con cadenas de dependencias, timestamps,
recompilacion selectiva, prefijos de recipe (@ y -) y flags de make (-n, -B).

## Prerequisitos

- GCC instalado (`gcc --version`)
- Make instalado (`make --version`)
- Terminal con acceso al directorio `labs/`
- Un editor que permita insertar tabuladores literales en Makefiles

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Regla basica y TAB obligatorio | Anatomia target:prerequisites + recipe, error "missing separator" con espacios |
| 2 | Timestamps y recompilacion selectiva | Make compara mtime del target vs prerequisites; `make` dice "up to date" si nada cambio |
| 3 | Multiples reglas y compilacion separada | Cadena de dependencias .c -> .o -> ejecutable; solo se recompila lo que cambio |
| 4 | Primera regla como default target | Efecto de que regla sea la primera; convencion de `all` |
| 5 | Prefijos de recipe: @ y - | Silenciar echo con @, ignorar errores con -, combinaciones |
| 6 | make -n (dry run) y make -B (forzar) | Previsualizar comandos sin ejecutarlos; forzar reconstruccion completa |

## Archivos

```
labs/
├── README.md       <- Ejercicios paso a paso
├── hello.c         <- Programa simple para la regla basica
├── calc.h          <- Header de biblioteca de calculo (add, subtract)
├── calc.c          <- Implementacion de la biblioteca
└── main_calc.c     <- Programa principal que usa calc.h
```

## Como ejecutar

```bash
cd labs/
# Seguir las instrucciones del README.md
```

## Tiempo estimado

~40 minutos

## Recursos creados

| Recurso | Tipo | Se elimina al final |
|---------|------|---------------------|
| Makefile | Makefile creado por el estudiante | Si (limpieza final) |
| Makefile_broken | Makefile con error intencional | Si (limpieza final) |
| hello, calculator | Ejecutables | Si (limpieza final) |
| main_calc.o, calc.o | Archivos objeto | Si (limpieza final) |
