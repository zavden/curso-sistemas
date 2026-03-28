# Labs -- Phony targets

## Descripcion

Laboratorio para entender por que `.PHONY` es necesario, experimentar el
problema cuando un archivo colisiona con el nombre de un target, y construir
un Makefile profesional con targets convencionales (all, clean, install, test,
help) incluyendo instalacion local con PREFIX y auto-documentacion con grep.

## Prerequisitos

- GCC instalado (`gcc --version`)
- GNU Make instalado (`make --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | El problema sin .PHONY | Crear un archivo "clean", ver que `make clean` dice "up to date", y solucionarlo con .PHONY |
| 2 | Targets convencionales | Usar un Makefile con all, clean, install, test, help; entender cada target |
| 3 | install con PREFIX | Instalar en un directorio local con PREFIX, verificar la estructura creada |
| 4 | Auto-documentacion con help | Entender el patron `## comment` + grep para generar ayuda automatica |

## Archivos

```
labs/
├── README.md           <- Ejercicios paso a paso
├── main.c              <- Programa principal (usa greeting.h)
├── greeting.h          <- Header de la biblioteca greeting
├── greeting.c          <- Implementacion de greeting (greet, greet_loud)
├── Makefile.broken     <- Makefile SIN .PHONY (para demostrar el problema)
└── Makefile.complete   <- Makefile completo con todos los targets convencionales
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
| greeter | Ejecutable | Si (limpieza final) |
| main.o, greeting.o | Archivos objeto | Si (limpieza final) |
| install/ | Directorio de instalacion local | Si (limpieza final) |
| clean, all | Archivos de prueba (touch) | Si (limpieza parte 1) |
