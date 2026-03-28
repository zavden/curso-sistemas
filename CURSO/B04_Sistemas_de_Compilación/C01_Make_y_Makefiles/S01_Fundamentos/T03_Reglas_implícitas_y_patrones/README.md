# T03 — Reglas implícitas y patrones

## Reglas implícitas (built-in)

Make incluye un conjunto de reglas predefinidas que sabe aplicar sin
que el usuario las escriba. Si Make necesita construir un archivo y no
encuentra una regla explícita, busca en su base de datos interna una
regla implícita que coincida:

```makefile
# Este Makefile NO tiene reglas de compilación:
CC = gcc
CFLAGS = -Wall -g

program: main.o utils.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# Sin embargo, make sabe construir main.o y utils.o.
# Busca main.c y utils.c, y aplica la regla implícita:
#   $(CC) $(CPPFLAGS) $(CFLAGS) -c -o main.o main.c
#   $(CC) $(CPPFLAGS) $(CFLAGS) -c -o utils.o utils.c
```

```bash
# Ver TODAS las reglas implícitas y variables predefinidas:
make -p

# Filtrar solo las reglas implícitas (sin las de tu Makefile):
make -p -f /dev/null | less

# Buscar la regla de .c a .o:
make -p -f /dev/null 2>/dev/null | grep -A 3 '%.o: %.c'
```

```bash
# Salida típica:
# %.o: %.c
# #  recipe to execute (built-in):
# 	$(COMPILE.c) $(OUTPUT_OPTION) $<
#
# Donde COMPILE.c = $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
# y OUTPUT_OPTION = -o $@
```

Por esto redefinir `CC` y `CFLAGS` afecta la compilación sin
necesidad de escribir reglas explícitas: las reglas implícitas
usan esas variables.

## Reglas implícitas comunes

```makefile
# 1. Compilar C → objeto (.c → .o):
#    $(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
# Variables involucradas:
#   CC        → compilador (default: cc)
#   CPPFLAGS  → flags del preprocesador (-I, -D)
#   CFLAGS    → flags de compilación (-Wall, -g, -O2)

# 2. Compilar C++ → objeto (.cpp/.cc/.C → .o):
#    $(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
# Variables involucradas:
#   CXX       → compilador C++ (default: g++)
#   CXXFLAGS  → flags de C++

# 3. Linkar objetos → ejecutable (.o → ejecutable):
#    $(CC) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@
# Variables involucradas:
#   LDFLAGS   → flags del linker (-L)
#   LDLIBS    → librerías (-lm, -lpthread)

# 4. Compilar C → ejecutable directo (.c → ejecutable):
#    $(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $^ $(LOADLIBES) $(LDLIBS) -o $@
# Esto funciona solo para programas de un solo archivo:
```

```bash
# Ejemplo: compilar un archivo .c sin Makefile:
echo 'int main(void) { return 0; }' > hello.c
make hello
# cc     hello.c   -o hello
# Make encontró hello.c y aplicó la regla implícita .c → ejecutable.

# Con variables en la línea de comandos:
make hello CC=gcc CFLAGS="-Wall -g"
# gcc -Wall -g    hello.c   -o hello
```

## Variables automáticas

Las variables automáticas se definen dentro de cada recipe y contienen
información sobre el target y los prerequisites de la regla que se
está ejecutando. Solo tienen valor dentro de un recipe, no en la
parte de condiciones o assignments.

```makefile
# $@ — El target de la regla.
# $< — El primer prerequisite.
# $^ — Todos los prerequisites, sin duplicados.
# $+ — Todos los prerequisites, con duplicados.
# $* — El stem: la parte que coincide con % en una pattern rule.
# $(@D) — El directorio del target (sin el nombre).
# $(@F) — El nombre del target (sin el directorio).
# $(<D) — El directorio del primer prerequisite.
# $(<F) — El nombre del primer prerequisite.

# Ejemplo que muestra cada una:
build/app: src/main.o src/utils.o src/main.o
	@echo "Target:               $@"       # build/app
	@echo "Primer prerequisite:  $<"       # src/main.o
	@echo "Todos (sin dupes):    $^"       # src/main.o src/utils.o
	@echo "Todos (con dupes):    $+"       # src/main.o src/utils.o src/main.o
	@echo "Dir del target:       $(@D)"    # build
	@echo "Nombre del target:    $(@F)"    # app
	@echo "Dir del 1er prereq:   $(<D)"    # src
	@echo "Nombre del 1er prereq:$(<F)"    # main.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)
```

```makefile
# $* — El stem (solo en pattern rules).
# Si la regla es %.o: %.c y el target es parser.o, el stem es "parser".

%.o: %.c
	@echo "Stem: $*"                       # parser
	@echo "Target: $@"                     # parser.o
	@echo "Source: $<"                     # parser.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# $* también funciona con paths. Si la regla es build/%.o: src/%.c
# y el target es build/parser.o, el stem es "parser".
```

```makefile
# Uso real de $(@D): crear directorios antes de compilar.
build/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Si el target es build/net/socket.o:
#   $(@D) = build/net        → mkdir -p build/net
#   $(@F) = socket.o
#   $<    = src/net/socket.c
```

## Reglas patron (pattern rules)

Las reglas patron usan `%` como wildcard. El `%` coincide con
cualquier cadena no vacia. Son la forma moderna de definir reglas
genericas:

```makefile
# Sintaxis: target-pattern: prerequisite-pattern
#     recipe

# El % en el target debe coincidir con el % en el prerequisite.
# La cadena que coincide con % se llama "stem".

# Compilar cualquier .c en un .o:
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Cuando Make necesita foo.o:
#   % coincide con "foo" (el stem)
#   Busca foo.c como prerequisite
#   Ejecuta: cc -c -o foo.o foo.c
```

```makefile
# Diferencia clave:
# - Regla implícita: predefinida por Make, no aparece en el Makefile.
# - Pattern rule: escrita por el usuario en el Makefile.
# - Una pattern rule del usuario REEMPLAZA la regla implícita equivalente.

# Si escribes esta pattern rule en tu Makefile:
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Make usa TU regla en lugar de la implícita para .c → .o.
# Esto permite agregar pasos extras (mkdir, mensajes, etc).
```

## Ejemplos de pattern rules

```makefile
# 1. Compilacion con directorio de output separado:
build/%.o: src/%.c
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Estructura del proyecto:
#   src/main.c  → build/main.o
#   src/utils.c → build/utils.o
```

```makefile
# 2. Generacion automatica de dependencias:
# GCC puede generar archivos .d que listan los headers que un .c incluye.
# Esto permite recompilar cuando un header cambia.

build/%.d: src/%.c
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -MM -MT 'build/$*.o build/$*.d' -MF $@ $<

# -MM:  generar dependencias (sin headers del sistema)
# -MT:  el target de la regla generada
# -MF:  archivo de salida
# $*:   el stem (ej: "main" si el target es build/main.d)

# El archivo generado build/main.d contiene algo como:
#   build/main.o build/main.d: src/main.c src/utils.h src/config.h
```

```makefile
# 3. Compilar archivos assembly:
%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

# 4. Generar archivos de preprocesador (para depuracion):
%.i: %.c
	$(CC) $(CPPFLAGS) -E -o $@ $<

# 5. Multiples extensiones de fuente:
%.o: %.cpp
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<

%.o: %.cc
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
```

## Cancelar reglas implicitas

```makefile
# A veces las reglas implicitas interfieren.
# Para cancelar una regla implicita, definir la pattern rule sin recipe:

%.o: %.c
# Sin recipe — cancela la regla implicita de .c → .o.
# Make no sabrá compilar .c a .o a menos que haya otra regla.
```

```makefile
# Cancelar la regla de compilacion directa .c → ejecutable:
%: %.c
# Esto evita que make intente compilar foo.c directamente a foo
# cuando en realidad quieres compilar a .o primero y luego linkar.
```

```makefile
# .SUFFIXES sin argumentos limpia las suffix rules legacy.
# Esto desactiva todas las reglas implicitas basadas en sufijos:
.SUFFIXES:

# Se pueden agregar solo los sufijos que interesan:
.SUFFIXES: .c .o
# Pero las pattern rules (%.o: %.c) son independientes de .SUFFIXES.
# .SUFFIXES solo afecta a las suffix rules antiguas.
```

```makefile
# En la practica, si quieres desactivar TODAS las reglas implicitas,
# se usa el flag -r (o --no-builtin-rules):
```

```bash
make -r
# Desactiva todas las reglas implicitas. Solo se usan las del Makefile.
# Util para Makefiles que definen todas sus reglas explicitamente.
```

## Static pattern rules

Las static pattern rules aplican una pattern rule solo a una lista
especifica de targets, no a cualquier archivo que coincida:

```makefile
# Sintaxis:
# targets: target-pattern: prerequisite-pattern
#     recipe

SRCS = main.c utils.c parser.c
OBJS = main.o utils.o parser.o

# Aplicar la regla SOLO a los archivos listados en $(OBJS):
$(OBJS): %.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Esto es equivalente a escribir:
#   main.o: main.c
#       $(CC) ...
#   utils.o: utils.c
#       $(CC) ...
#   parser.o: parser.c
#       $(CC) ...
```

```makefile
# Ventaja: limitar el alcance de la regla.
# Una pattern rule normal (%.o: %.c) aplica a CUALQUIER .o.
# Una static pattern rule aplica solo a los targets listados.

# Ejemplo: compilar fuentes de src/ y tests/ con flags diferentes:
SRC_OBJS = main.o utils.o parser.o
TEST_OBJS = test_utils.o test_parser.o

$(SRC_OBJS): %.o: src/%.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(TEST_OBJS): %.o: tests/%.c
	$(CC) $(CFLAGS) -DTEST -c -o $@ $<

# main.o se compila desde src/main.c con CFLAGS normales.
# test_utils.o se compila desde tests/test_utils.c con -DTEST.
```

```makefile
# Otro uso: aplicar regla solo a ciertos targets en un directorio:
LIBS = libfoo.a libbar.a

$(LIBS): lib%.a: %.o
	$(AR) rcs $@ $<

# libfoo.a se construye desde foo.o.
# libbar.a se construye desde bar.o.
# Si existiera baz.o, NO se le aplica esta regla.
```

## Suffix rules (legacy)

Las suffix rules son la forma antigua (pre-GNU Make) de definir
reglas genericas. Fueron reemplazadas por las pattern rules pero
aparecen en Makefiles viejos:

```makefile
# Suffix rule (forma vieja):
.c.o:
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Equivale a esta pattern rule (forma moderna):
%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# ATENCION: el orden es INVERSO en suffix rules.
# .c.o significa "de .c a .o" — fuente primero, target despues.
# En pattern rules es target primero: %.o: %.c
```

```makefile
# Otros ejemplos de suffix rules:
.c:
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) -o $@ $< $(LDLIBS)
# Equivale a: %: %.c

.cpp.o:
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) -c -o $@ $<
# Equivale a: %.o: %.cpp
```

```makefile
# Las suffix rules tienen limitaciones frente a pattern rules:
# 1. Solo pueden tener un prerequisite (el source).
# 2. No pueden usar paths — .c.o no coincide con src/foo.c
# 3. El orden de lectura (fuente.target) es contraintuitivo.
# 4. No hay stem ($* no funciona de la misma forma).
#
# Recomendacion: usar pattern rules siempre.
# Conocer suffix rules solo para leer codigo legacy.
```

## Ejemplo completo

Un Makefile real con pattern rules, variables automaticas,
compilacion separada y generacion de dependencias:

```makefile
# Proyecto con estructura:
#   src/main.c
#   src/lexer.c
#   src/parser.c
#   src/codegen.c
#   include/lexer.h
#   include/parser.h
#   include/codegen.h
#   build/          (directorio de salida)

# --- Configuracion ---
CC       = gcc
CFLAGS   = -Wall -Wextra -Werror -std=c11 -g
CPPFLAGS = -Iinclude
LDFLAGS  =
LDLIBS   = -lm

# --- Archivos ---
SRCDIR   = src
BUILDDIR = build
TARGET   = $(BUILDDIR)/compiler

SRCS = $(wildcard $(SRCDIR)/*.c)
OBJS = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
DEPS = $(OBJS:.o=.d)

# --- Target principal ---
all: $(TARGET)

# Linkar: combinar todos los .o en el ejecutable.
# $@ = build/compiler
# $^ = build/main.o build/lexer.o build/parser.o build/codegen.o
$(TARGET): $(OBJS)
	mkdir -p $(@D)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# --- Pattern rule: compilar .c → .o ---
# $@ = build/lexer.o
# $< = src/lexer.c
# $(@D) = build
$(BUILDDIR)/%.o: $(SRCDIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# --- Generacion de dependencias ---
# Crea archivos .d que contienen las dependencias de headers.
# $* = el stem (ej: "lexer")
$(BUILDDIR)/%.d: $(SRCDIR)/%.c
	mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -MM -MT '$(BUILDDIR)/$*.o $(BUILDDIR)/$*.d' -MF $@ $<

# Incluir las dependencias generadas (si existen).
# El - al inicio evita error si los .d no existen aun.
-include $(DEPS)

# --- Targets auxiliares ---
.PHONY: all clean info

clean:
	rm -rf $(BUILDDIR)

info:
	@echo "Sources:      $(SRCS)"
	@echo "Objects:      $(OBJS)"
	@echo "Dependencies: $(DEPS)"
	@echo "Target:       $(TARGET)"
```

```bash
# Uso del Makefile:

# Compilar todo:
make

# Ver qué se compilaría (dry run):
make -n

# Compilar con optimización (sobreescribir CFLAGS):
make CFLAGS="-O2 -DNDEBUG"

# Ver las reglas implícitas que Make conoce:
make -p | head -100

# Compilar sin reglas implícitas (solo las del Makefile):
make -r

# Limpiar y recompilar:
make clean && make
```

```bash
# Flujo de compilacion de este Makefile:
#
# 1. make lee el Makefile y encuentra que "all" depende de "build/compiler".
# 2. "build/compiler" depende de build/main.o build/lexer.o ...
# 3. Para cada .o, busca una regla: la pattern rule build/%.o: src/%.c.
# 4. Ejecuta la compilacion de cada .c a .o con las variables automaticas:
#      gcc -Iinclude -Wall -Wextra -Werror -std=c11 -g -c -o build/main.o src/main.c
#      gcc -Iinclude -Wall -Wextra -Werror -std=c11 -g -c -o build/lexer.o src/lexer.c
#      ...
# 5. Linka todos los .o en el ejecutable:
#      gcc -o build/compiler build/main.o build/lexer.o ... -lm
# 6. Los .d se generan e incluyen para que cambios en headers
#    provoquen recompilacion automatica.
```

---

## Ejercicios

### Ejercicio 1 --- Variables automaticas

```makefile
# Crear un Makefile con esta regla:
#
# debug/%.o: src/%.c include/%.h
# 	@echo "Target:        $@"
# 	@echo "First prereq:  $<"
# 	@echo "All prereqs:   $^"
# 	@echo "Stem:          $*"
# 	@echo "Target dir:    $(@D)"
# 	@echo "Target file:   $(@F)"
#
# Crear los archivos src/demo.c e include/demo.h (pueden estar vacios).
# Ejecutar: make debug/demo.o
# Predecir la salida de cada echo ANTES de ejecutar.
# Verificar las predicciones.
```

### Ejercicio 2 --- Static pattern rules con flags diferenciados

```makefile
# Crear un proyecto con:
#   src/app.c      (el main)
#   src/math.c     (funciones matematicas)
#   tests/test_math.c (tests que incluyen math.c)
#
# Escribir un Makefile que:
# 1. Compile los fuentes de src/ con: -O2 -DNDEBUG
# 2. Compile los fuentes de tests/ con: -g -DTEST
# 3. Genere dos ejecutables: build/app y build/test_math
# 4. Use static pattern rules para diferenciar las flags.
# 5. Use $(@D) para crear los directorios automaticamente.
```

### Ejercicio 3 --- Eliminar reglas implicitas

```makefile
# 1. Crear un archivo hello.c con un main que imprima "hello".
# 2. Sin Makefile, ejecutar: make hello
#    Observar que Make compila usando la regla implicita .c → ejecutable.
# 3. Crear un Makefile que:
#    a. Cancele la regla implicita %: %.c (definirla sin recipe).
#    b. Cancele la regla implicita %.o: %.c.
#    c. Defina sus propias pattern rules para .c → .o y .o → ejecutable.
#    d. Use CFLAGS = -Wall -Wextra -g
# 4. Verificar con make -n que usa las reglas del Makefile, no las implicitas.
# 5. Comparar con make -r (que desactiva las implicitas por flag).
```
