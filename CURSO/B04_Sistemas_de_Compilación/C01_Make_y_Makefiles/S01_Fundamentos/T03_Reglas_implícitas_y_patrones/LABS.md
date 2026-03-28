# Labs -- Reglas implicitas y patrones

## Descripcion

Laboratorio para explorar las reglas implicitas (built-in) de Make, las
variables automaticas ($@, $<, $^, $+, $*, $(@D), $(@F)), las pattern rules
definidas por el usuario (%.o: %.c), las static pattern rules, y la
compilacion con directorio de build separado (build/%.o: src/%.c).

## Prerequisitos

- GCC instalado (`gcc --version`)
- GNU Make instalado (`make --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Reglas implicitas built-in | Explorar la base de datos interna de Make con `make -p`, compilar sin Makefile usando reglas implicitas |
| 2 | Compilacion sin reglas explicitas | Makefile con solo variables y regla de enlace; Make aplica la regla implicita %.o: %.c |
| 3 | Variables automaticas | Ver con echo los valores de $@, $<, $^, $+, $*, $(@D), $(@F), $(<D), $(<F) |
| 4 | Pattern rules del usuario | Definir %.o: %.c propia que reemplaza la implicita, comparar con `make -n` |
| 5 | Static pattern rules | Aplicar una pattern rule solo a una lista especifica de targets |
| 6 | Build directory separado | Compilar con build/%.o: src/%.c, usar $(@D) para crear directorios |

## Archivos

```
labs/
├── README.md            <- Ejercicios paso a paso
├── hello.c              <- Programa simple para probar reglas implicitas
├── Makefile.implicit     <- Makefile sin reglas de compilacion (usa built-in)
├── Makefile.autovars     <- Makefile que imprime todas las variables automaticas
├── Makefile.pattern      <- Makefile con pattern rule del usuario
├── Makefile.static       <- Makefile con static pattern rules
├── Makefile.builddir     <- Makefile con directorio de build separado
└── src/
    ├── main.c           <- Programa principal (usa utils.h)
    ├── utils.c          <- Funciones auxiliares (add, mul)
    └── utils.h          <- Header de utils
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
| hello | Ejecutable (regla implicita) | Si (limpieza final) |
| program | Ejecutable | Si (limpieza final) |
| *.o | Archivos objeto | Si (limpieza final) |
| build/ | Directorio de build | Si (limpieza final) |
