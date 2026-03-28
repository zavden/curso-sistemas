# T03 — Makefiles recursivos vs no-recursivos

## Que es recursive make

Un Makefile recursivo invoca `$(MAKE)` para compilar
subdirectorios. Cada subdirectorio tiene su propio Makefile
independiente, y el Makefile raiz los coordina:

```makefile
# Patron clasico: el Makefile raiz invoca sub-makes.
# $(MAKE) -C subdir entra al directorio y ejecuta make ahi.

SUBDIRS := lib app

.PHONY: all $(SUBDIRS)
all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

# Establecer orden: app depende de lib.
app: lib
```

```makefile
# Cada subdirectorio tiene su propio Makefile completo.
# lib/Makefile:
CC     := gcc
CFLAGS := -Wall -Wextra -fPIC

SRCS := mathutils.o strutils.o
TARGET := libutils.a

$(TARGET): $(SRCS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(SRCS) $(TARGET)
```

## $(MAKE) vs make

Dentro de una recipe, siempre usar `$(MAKE)` en lugar de
`make` directamente. `$(MAKE)` es una variable especial que
apunta al mismo ejecutable de Make que se esta usando:

```makefile
# $(MAKE) hereda flags importantes del make padre:
#   -j N  (paralelismo)
#   -k    (continuar tras errores)
#   -n    (dry-run: solo imprime, no ejecuta)
#   -s    (silencioso)

# CORRECTO:
libs:
	$(MAKE) -C lib

# INCORRECTO:
libs:
	make -C lib
# Si el usuario ejecuto "make -j8", el sub-make no hereda -j8.
# Si el usuario ejecuto "make -n" (dry-run), el sub-make
# SI ejecuta comandos reales — rompe el dry-run.
```

```makefile
# $(MAKE) tambien asegura que se use el mismo binario.
# Si el usuario invoco /usr/local/bin/gmake, $(MAKE) sera
# /usr/local/bin/gmake, no /usr/bin/make.

# Ademas, Make trata las lineas con $(MAKE) de forma especial:
# incluso con -n (dry-run), Make EJECUTA las lineas con $(MAKE)
# para que los sub-makes tambien reciban -n y hagan su propio
# dry-run. Si usas "make" literal, esto no funciona.
```

## Ejemplo de recursive make

Un proyecto con dos componentes: una biblioteca en `lib/`
y una aplicacion en `app/` que depende de ella:

```
proyecto/
  Makefile          # raiz: coordina sub-makes
  lib/
    Makefile        # compila libcalc.a
    calc.c
    calc.h
  app/
    Makefile        # compila programa principal
    main.c
```

```makefile
# proyecto/Makefile (raiz)
SUBDIRS := lib app

.PHONY: all clean $(SUBDIRS)

all: $(SUBDIRS)

# app necesita que lib se compile primero:
app: lib

$(SUBDIRS):
	$(MAKE) -C $@

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done
```

```makefile
# proyecto/lib/Makefile
CC     := gcc
CFLAGS := -Wall -Wextra -std=c17

TARGET := libcalc.a
SRCS   := calc.c
OBJS   := $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)
```

```makefile
# proyecto/app/Makefile
CC      := gcc
CFLAGS  := -Wall -Wextra -std=c17
LDFLAGS := -L../lib
LDLIBS  := -lcalc

TARGET := calculator
SRCS   := main.c
OBJS   := $(SRCS:.c=.o)

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -I../lib -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)
```

```bash
$ make
make -C lib
make[1]: Entering directory '/home/user/proyecto/lib'
gcc -Wall -Wextra -std=c17 -c calc.c -o calc.o
ar rcs libcalc.a calc.o
make[1]: Leaving directory '/home/user/proyecto/lib'
make -C app
make[1]: Entering directory '/home/user/proyecto/app'
gcc -Wall -Wextra -std=c17 -I../lib -c main.c -o main.o
gcc -L../lib -o calculator main.o -lcalc
make[1]: Leaving directory '/home/user/proyecto/app'
```

## export — pasar variables a sub-makes

La directiva `export` pasa variables del Makefile padre a los
sub-makes como variables de entorno. Esto permite centralizar
la configuracion en el Makefile raiz:

```makefile
# proyecto/Makefile (raiz)
CC     := gcc
CFLAGS := -Wall -Wextra -std=c17

# Exportar para que los sub-makes hereden estas variables:
export CC CFLAGS

SUBDIRS := lib app

.PHONY: all $(SUBDIRS)
all: $(SUBDIRS)

app: lib

$(SUBDIRS):
	$(MAKE) -C $@
```

```makefile
# Con export, los sub-makes ya no necesitan definir CC ni CFLAGS.
# proyecto/lib/Makefile simplificado:
TARGET := libcalc.a
SRCS   := calc.c
OBJS   := $(SRCS:.c=.o)

# CC y CFLAGS vienen del Makefile padre via export.

$(TARGET): $(OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)
```

```makefile
# Variantes de export:
export CC CFLAGS LDFLAGS   # exportar variables especificas

export                      # exportar TODAS las variables
                            # (no recomendado — efectos impredecibles)

unexport PRIVATE_VAR       # excluir una variable de la exportacion

# export tambien funciona en la definicion:
export CC := gcc
export CFLAGS := -Wall -Wextra

# Las variables pasadas por linea de comandos se exportan
# automaticamente:
#   make CC=clang
# CC=clang estara disponible en todos los sub-makes sin
# necesidad de export explicito.
```

## Problemas del recursive make

En 1998, Peter Miller publico "Recursive Make Considered Harmful",
documentando los problemas fundamentales del patron recursivo.
El problema central es que cada sub-make tiene una vision parcial
del proyecto:

```makefile
# Problema 1: no hay vision global del grafo de dependencias.
#
# El Makefile de lib/ no sabe que app/ existe.
# El Makefile de app/ no sabe que archivos tiene lib/.
# El Makefile raiz solo sabe que "app depende de lib" como
# directorios, no a nivel de archivos individuales.
#
# Resultado: Make no puede construir el grafo completo
# de dependencias y toma decisiones suboptimas.
```

```makefile
# Problema 2: paralelismo incorrecto entre subdirectorios.
#
# Con make -j8, cada sub-make recibe -j8 independientemente.
# Si hay 3 subdirectorios, se lanzan hasta 24 procesos (3 x 8).
# Esto sobrecarga la maquina.
#
# GNU Make tiene un "job server" para mitigar esto, pero
# no todos los sub-makes lo soportan correctamente.
# Ademas, Make no puede paralelizar ENTRE subdirectorios
# si hay dependencias implicitas que no estan declaradas.
```

```makefile
# Problema 3: recompilacion incorrecta.
#
# Escenario: lib/calc.h cambia. app/main.c incluye lib/calc.h.
#
# make raiz ejecuta: $(MAKE) -C lib
# lib/ detecta el cambio, recompila calc.o, regenera libcalc.a.
#
# make raiz ejecuta: $(MAKE) -C app
# app/ NO sabe que lib/calc.h cambio.
# app/ ve que main.c no cambio y main.o existe.
# app/ NO recompila main.o.
# El programa se enlaza con un main.o que usa la version
# ANTERIOR de calc.h. Bug silencioso.

# Para "arreglar" esto, muchos proyectos recursivos hacen:
.PHONY: app
app: lib
	$(MAKE) -C app
# Pero declarar app como phony fuerza recompilar SIEMPRE,
# incluso cuando nada cambio. Se pierde la ventaja de Make.
```

```makefile
# Problema 4: el orden de subdirectorios es manual.
#
# Si tests/ depende de lib/ y app/:
app: lib
tests: app lib
# Estas dependencias se gestionan a mano en el Makefile raiz.
# Si alguien agrega una nueva dependencia y olvida actualizarlo,
# el build falla intermitentemente (funciona en serial, falla
# en paralelo con -j).
```

## Que es non-recursive make

Un Makefile no-recursivo usa un unico Makefile que conoce
TODOS los archivos fuente del proyecto. No invoca sub-makes.
Tiene vision completa del grafo de dependencias:

```makefile
# Un solo Makefile en la raiz del proyecto.
# Conoce todos los .c, todos los .h, todas las dependencias.
# Make construye un grafo global y toma decisiones correctas.

CC     := gcc
CFLAGS := -Wall -Wextra -std=c17

# Todos los sources listados explicitamente:
LIB_SRCS := lib/calc.c
APP_SRCS := app/main.c

LIB_OBJS := $(LIB_SRCS:.c=.o)
APP_OBJS := $(APP_SRCS:.c=.o)

LIB_TARGET := lib/libcalc.a
APP_TARGET := app/calculator

.PHONY: all clean
all: $(APP_TARGET)

# app depende de lib — Make lo sabe a nivel de archivo:
$(APP_TARGET): $(APP_OBJS) $(LIB_TARGET)
	$(CC) -o $@ $(APP_OBJS) $(LIB_TARGET)

$(LIB_TARGET): $(LIB_OBJS)
	$(AR) rcs $@ $^

# Regla generica para compilar .c a .o:
%.o: %.c
	$(CC) $(CFLAGS) -Ilib -c $< -o $@

clean:
	$(RM) $(LIB_OBJS) $(APP_OBJS) $(LIB_TARGET) $(APP_TARGET)
```

```bash
# Ahora si lib/calc.h cambia y app/main.c lo incluye,
# Make recompila main.o automaticamente (con dependencias
# generadas por -MMD). El grafo es global y correcto.

$ make -j8
# Make paraleliza correctamente: puede compilar calc.o y
# main.o en paralelo si no hay dependencia entre ellos.
# Solo espera a que libcalc.a exista antes de enlazar.
```

## Ejemplo de non-recursive make

El mismo proyecto de antes, pero sin recursion:

```
proyecto/
  Makefile          # unico Makefile, conoce todo
  lib/
    calc.c
    calc.h
  app/
    main.c
```

```makefile
# proyecto/Makefile (non-recursive)
CC     := gcc
CFLAGS := -Wall -Wextra -std=c17

# === Library ===
LIB_SRCS := lib/calc.c
LIB_OBJS := $(LIB_SRCS:.c=.o)
LIB_DEPS := $(LIB_OBJS:.o=.d)
LIB      := lib/libcalc.a

# === Application ===
APP_SRCS := app/main.c
APP_OBJS := $(APP_SRCS:.c=.o)
APP_DEPS := $(APP_OBJS:.o=.d)
APP      := app/calculator

# === All sources for dependency tracking ===
ALL_OBJS := $(LIB_OBJS) $(APP_OBJS)
ALL_DEPS := $(LIB_DEPS) $(APP_DEPS)

# === Targets ===
.PHONY: all clean

all: $(APP)

$(APP): $(APP_OBJS) $(LIB)
	$(CC) -o $@ $(APP_OBJS) $(LIB)

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

# === Compilation rule ===
%.o: %.c
	$(CC) $(CFLAGS) -Ilib -MMD -MP -c $< -o $@

# === Auto-generated dependencies ===
-include $(ALL_DEPS)

clean:
	$(RM) $(ALL_OBJS) $(ALL_DEPS) $(LIB) $(APP)
```

```bash
# Ventaja visible: si lib/calc.h cambia, Make sabe que
# app/main.o depende de el (via los .d generados por -MMD).
$ touch lib/calc.h
$ make
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c app/main.c -o app/main.o
gcc -o app/calculator app/main.o lib/libcalc.a
# Recompilo main.o y reenlazó. Correcto.
# Con recursive make, esto no habria ocurrido.
```

## Patron de include con module.mk

En proyectos con muchos subdirectorios, listar todos los
archivos en un solo Makefile se vuelve dificil de mantener.
El patron de `include` resuelve esto: cada subdirectorio
define sus archivos en un fragmento que el Makefile raiz incluye:

```makefile
# lib/module.mk
LIB_SRCS := lib/calc.c lib/mathutils.c lib/strutils.c
LIB      := lib/libutils.a
```

```makefile
# app/module.mk
APP_SRCS := app/main.c app/cli.c app/config.c
APP      := app/myapp
```

```makefile
# proyecto/Makefile (raiz)
CC     := gcc
CFLAGS := -Wall -Wextra -std=c17

# Incluir fragmentos de cada modulo:
include lib/module.mk
include app/module.mk

# Derivar objetos y dependencias:
LIB_OBJS := $(LIB_SRCS:.c=.o)
APP_OBJS := $(APP_SRCS:.c=.o)
ALL_DEPS := $(LIB_OBJS:.o=.d) $(APP_OBJS:.o=.d)

.PHONY: all clean
all: $(APP)

$(APP): $(APP_OBJS) $(LIB)
	$(CC) -o $@ $(APP_OBJS) $(LIB)

$(LIB): $(LIB_OBJS)
	$(AR) rcs $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -Ilib -MMD -MP -c $< -o $@

-include $(ALL_DEPS)

clean:
	$(RM) $(LIB_OBJS) $(APP_OBJS) $(ALL_DEPS) $(LIB) $(APP)
```

```makefile
# Cada module.mk solo define variables — no tiene reglas.
# Las reglas estan centralizadas en el Makefile raiz.
# Esto mantiene los beneficios del non-recursive make
# (grafo global) con la organizacion del recursivo
# (cada subdirectorio gestiona su lista de archivos).

# Convenciones comunes para los fragmentos:
#   module.mk     — nombre mas usado
#   Makefile.inc   — alternativa comun
#   build.mk      — otra variante

# Agregar un nuevo modulo:
# 1. Crear tests/module.mk con TESTS_SRCS := ...
# 2. Agregar "include tests/module.mk" al Makefile raiz
# 3. Agregar reglas para el nuevo modulo
```

## Comparacion

| Aspecto | Recursivo | No-recursivo |
|---|---|---|
| Grafo de dependencias | Parcial (cada sub-make ve solo lo suyo) | Global (un solo Make ve todo) |
| Paralelismo con -j | Problematico (job server imperfecto, posible sobrecarga) | Correcto (Make gestiona un unico pool de jobs) |
| Recompilacion | Puede fallar (no detecta cambios entre subdirectorios) | Correcta (dependencias globales con -MMD) |
| Complejidad inicial | Baja (cada Makefile es independiente) | Media (requiere planificar la estructura) |
| Escalabilidad | Buena para equipos grandes (encapsulacion) | Requiere disciplina (module.mk ayuda) |
| Mantenimiento | Duplicacion de variables (CC, CFLAGS en cada Makefile) | Centralizado (variables en un solo lugar) |
| Orden de compilacion | Manual (declarar dependencias entre subdirectorios) | Automatico (Make lo deduce del grafo) |
| Velocidad de build | Mas lento (multiples procesos Make, overhead por invocacion) | Mas rapido (un solo proceso, sin overhead) |
| Builds incrementales | Poco fiables (dependencias entre modulos invisibles) | Fiables (grafo completo) |

```makefile
# Resumen en una frase:
# Recursivo = simple de escribir, dificil de mantener correcto.
# No-recursivo = mas trabajo inicial, builds confiables.
```

## Enfoque hibrido

En la practica, muchos proyectos grandes usan un enfoque
hibrido: estructura recursiva para la organizacion, pero con
mecanismos para compensar sus debilidades:

```makefile
# Estrategia 1: recursivo con dependencias explicitas.
# El Makefile raiz declara dependencias entre subdirectorios
# y fuerza reconstruccion cuando es necesario.

SUBDIRS := lib app tests

.PHONY: all $(SUBDIRS)
all: $(SUBDIRS)

# Dependencias entre subdirectorios explicitas:
app: lib
tests: lib app

$(SUBDIRS):
	$(MAKE) -C $@

# Limitacion: las dependencias siguen siendo a nivel de
# directorio, no de archivo. Mejor que nada, pero no ideal.
```

```makefile
# Estrategia 2: usar CMake, Meson o similar.
# Estas herramientas generan Makefiles non-recursive
# automaticamente a partir de una descripcion de alto nivel.

# CMakeLists.txt (raiz):
# cmake_minimum_required(VERSION 3.20)
# project(calculator C)
# add_subdirectory(lib)
# add_subdirectory(app)

# CMakeLists.txt (lib):
# add_library(calc calc.c)

# CMakeLists.txt (app):
# add_executable(calculator main.c)
# target_link_libraries(calculator calc)

# CMake genera un build system non-recursive internamente.
# El usuario escribe algo parecido a recursive make,
# pero obtiene los beneficios del non-recursive.
```

```makefile
# Estrategia 3: non-recursive con include (ya visto arriba).
# Es el enfoque hibrido mas comun en proyectos que quieren
# quedarse con Make puro:
# - Un Makefile raiz con todas las reglas.
# - Cada subdirectorio tiene un module.mk con sus variables.
# - include los combina en un grafo unico.
```

## Recomendacion practica

```makefile
# Proyecto pequeno/mediano (< 50 archivos fuente):
# Usar non-recursive make.
# Un solo Makefile o Makefile + module.mk por subdirectorio.
# Las dependencias son correctas, -j funciona bien,
# los builds incrementales son fiables.

# Proyecto grande (> 100 archivos, multiples equipos):
# Usar CMake, Meson, o Bazel.
# Estas herramientas generan builds non-recursive internamente,
# manejan dependencias entre modulos automaticamente,
# y escalan a miles de archivos sin problemas.

# Recursive make puro:
# Es el enfoque MENOS recomendado.
# Solo tiene sentido si:
# - El proyecto ya lo usa y no vale la pena migrarlo.
# - Los modulos son genuinamente independientes (no comparten
#   headers, no se enlazan entre si).
# - Se acepta que los builds incrementales pueden fallar.

# Resumen de decisiones:
#
#   Tamano del proyecto    Enfoque recomendado
#   ---------------------  ----------------------
#   Pequeno (1-20 .c)      Non-recursive puro
#   Mediano (20-100 .c)    Non-recursive + module.mk
#   Grande (100+ .c)       CMake / Meson / Bazel
#   Legado recursivo       Documentar dependencias, considerar migracion
```

---

## Ejercicios

### Ejercicio 1 — Recursive make y el problema de dependencias

```makefile
# Crear un proyecto con la estructura:
#
#   proyecto/
#     Makefile
#     lib/
#       Makefile
#       utils.c
#       utils.h
#     app/
#       Makefile
#       main.c     (incluye ../lib/utils.h)
#
# 1. Implementar con recursive make (Makefile raiz invoca
#    $(MAKE) -C lib y $(MAKE) -C app).
# 2. Compilar con make. Verificar que funciona.
# 3. Modificar lib/utils.h (agregar un comentario o cambiar
#    una firma de funcion).
# 4. Ejecutar make de nuevo. Observar si app/main.o se
#    recompila o no.
# 5. Explicar por que app/ no detecta el cambio en lib/utils.h.
# 6. Que tendrias que hacer para forzar la recompilacion?
```

### Ejercicio 2 — Convertir recursivo a non-recursive

```makefile
# Tomar el mismo proyecto del ejercicio 1 y convertirlo a
# non-recursive:
# 1. Eliminar lib/Makefile y app/Makefile.
# 2. Crear un unico Makefile en la raiz que:
#    - Liste todos los .c de lib/ y app/
#    - Compile con -MMD para generar dependencias automaticas
#    - Incluya los .d con -include
# 3. Repetir el test: modificar lib/utils.h y ejecutar make.
# 4. Verificar que ahora app/main.o SI se recompila.
# 5. Ejecutar make -j4 y verificar que funciona correctamente.
```

### Ejercicio 3 — Non-recursive con module.mk

```makefile
# Extender el proyecto del ejercicio 2 agregando un tercer
# componente tests/:
#
#   proyecto/
#     Makefile
#     lib/
#       module.mk
#       utils.c
#       utils.h
#     app/
#       module.mk
#       main.c
#     tests/
#       module.mk
#       test_utils.c
#
# 1. Crear un module.mk en cada subdirectorio que defina
#    las variables de sus fuentes (LIB_SRCS, APP_SRCS, TEST_SRCS).
# 2. El Makefile raiz debe hacer include de los tres module.mk.
# 3. Definir tres targets: app/calculator, lib/libutils.a,
#    tests/test_runner.
# 4. Verificar que make -j4 compila todo correctamente.
# 5. Agregar un nuevo archivo lib/math.c y su entrada en
#    lib/module.mk. Verificar que no hay que tocar el Makefile raiz.
```
