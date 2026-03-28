# Labs -- Makefiles recursivos vs no-recursivos

## Descripcion

Laboratorio para construir un proyecto multi-directorio con recursive make
(lib/ y app/ con Makefiles independientes), demostrar el problema de
dependencias cruzadas entre subdirectorios, convertir a non-recursive make
con dependencias automaticas (-MMD), y organizar el proyecto con el patron
module.mk.

## Prerequisitos

- GCC instalado (`gcc --version`)
- GNU Make instalado (`make --version`)
- Terminal con acceso al directorio `labs/`

## Contenido del laboratorio

| Parte | Concepto | Que se demuestra |
|-------|----------|------------------|
| 1 | Recursive make | Crear estructura lib/ y app/ con Makefiles independientes, compilar con $(MAKE) -C |
| 2 | Dependencias cruzadas rotas | Modificar lib/calc.h, observar que app/main.o NO se recompila |
| 3 | Non-recursive make | Convertir a un solo Makefile raiz con -MMD, verificar que las dependencias se resuelven |
| 4 | Patron module.mk | Reorganizar con fragmentos module.mk por subdirectorio, mantener los beneficios del non-recursive |

## Archivos

```
labs/
├── README.md              <- Ejercicios paso a paso
├── calc.h                 <- Header de la biblioteca (add, sub)
├── calc.c                 <- Implementacion de la biblioteca
├── main.c                 <- Programa principal que usa calc.h
├── Makefile.recursive.root <- Makefile raiz para la version recursiva
├── Makefile.recursive.lib  <- Makefile de lib/ para la version recursiva
├── Makefile.recursive.app  <- Makefile de app/ para la version recursiva
├── Makefile.nonrecursive   <- Makefile unico para la version non-recursive
├── Makefile.modulemk       <- Makefile raiz para la version con module.mk
├── module.mk.lib           <- Fragmento module.mk para lib/
└── module.mk.app           <- Fragmento module.mk para app/
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
| proyecto-rec/ | Directorio de trabajo (version recursiva) | Si (limpieza parte 2) |
| proyecto-nonrec/ | Directorio de trabajo (version non-recursive) | Si (limpieza final) |
| proyecto-modul/ | Directorio de trabajo (version module.mk) | Si (limpieza final) |
| libcalc.a | Biblioteca estatica | Si (dentro de cada directorio) |
| calculator | Ejecutable | Si (dentro de cada directorio) |
| *.o, *.d | Objetos y dependencias | Si (dentro de cada directorio) |
