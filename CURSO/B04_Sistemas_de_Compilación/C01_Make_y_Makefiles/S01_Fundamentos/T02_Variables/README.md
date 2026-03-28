# T02 — Variables

## Sintaxis de variables

Las variables en Make almacenan texto. Se definen con
`NOMBRE = valor` y se referencian con `$(NOMBRE)` o `${NOMBRE}`.
Por convención, los nombres van en mayusculas con guiones bajos:

```makefile
CC = gcc
CFLAGS = -Wall -Wextra -std=c17
SRC_DIR = src
BUILD_DIR = build

# Referencia con $():
all:
	$(CC) $(CFLAGS) -o main $(SRC_DIR)/main.c

# ${} funciona igual — $() es más común en Makefiles:
clean:
	rm -rf ${BUILD_DIR}

# Para un solo carácter NO se necesitan paréntesis:
# $X equivale a $(X) — pero solo si X es un carácter.
# Esto solo se usa en variables automáticas: $@, $<, $^
# Para todo lo demás, siempre usar $() o ${}.
```

## Recursively expanded (=)

El operador `=` crea una variable de expansion recursiva. El valor
NO se evalua al definirla, sino cada vez que se referencia. Esto
permite referenciar variables que todavia no existen:

```makefile
# GCC_VERSION no existe todavía — no importa con =
CC = $(GCC_VERSION)

# Se define después:
GCC_VERSION = gcc-13

# Cuando Make expande $(CC), evalúa $(GCC_VERSION) en ese momento:
all:
	@echo $(CC)
# Imprime: gcc-13
```

```makefile
# Cada referencia re-evalúa la expresión completa:
TIMESTAMP = $(shell date +%T)

all:
	@echo "Primera: $(TIMESTAMP)"
	@sleep 1
	@echo "Segunda: $(TIMESTAMP)"
# Cada $(TIMESTAMP) ejecuta el shell command de nuevo.
# Las dos líneas muestran horas diferentes.
```

## Simply expanded (:=)

El operador `:=` crea una variable de expansion inmediata. El valor
se evalua una sola vez, en el momento de la definicion. Funciona
como una asignacion normal en cualquier lenguaje de programacion:

```makefile
TIMESTAMP := $(shell date +%T)

all:
	@echo "Primera: $(TIMESTAMP)"
	@sleep 1
	@echo "Segunda: $(TIMESTAMP)"
# Ambas líneas muestran la MISMA hora.
# $(shell date +%T) se ejecutó una sola vez al leer la línea.
```

```makefile
# Con := el orden importa — la variable debe existir ANTES:
CC := $(GCC_VERSION)
GCC_VERSION := gcc-13

all:
	@echo $(CC)
# Imprime una línea vacía.
# Al evaluar $(GCC_VERSION), todavía no estaba definida.
```

## Diferencia practica entre = y :=

```makefile
# --- Ejemplo donde el resultado cambia ---

X = hello
A = $(X) world
B := $(X) world

X = goodbye

all:
	@echo "A = $(A)"
	@echo "B = $(B)"
# A = goodbye world   (= re-evalúa: ve el X actual)
# B = hello world      (:= capturó el X del momento de definición)
```

```makefile
# --- Problema: recursión infinita con = ---

# ESTO NO FUNCIONA:
CFLAGS = $(CFLAGS) -Wall
# Make intenta expandir $(CFLAGS), que contiene $(CFLAGS),
# que contiene $(CFLAGS)... recursión infinita.
# Error: "Recursive variable 'CFLAGS' references itself"

# Solución 1: usar :=
CFLAGS := -O2
CFLAGS := $(CFLAGS) -Wall
# Con := se evalúa inmediatamente: CFLAGS ahora es "-O2 -Wall"

# Solución 2: usar +=
CFLAGS = -O2
CFLAGS += -Wall
# += está diseñado para esto. Resultado: "-O2 -Wall"
```

## Conditional assignment (?=)

El operador `?=` asigna el valor solo si la variable no esta
definida previamente. Es el mecanismo estandar para definir valores
por defecto que el usuario puede sobreescribir:

```makefile
# Valores por defecto — el usuario puede cambiarlos:
CC ?= gcc
CFLAGS ?= -O2 -Wall
PREFIX ?= /usr/local

# Si el usuario ejecuta:
#   make CC=clang
# CC será "clang" (ya estaba definida, ?= no la toca).
#
# Si ejecuta solo:
#   make
# CC será "gcc" (no estaba definida, ?= le asigna el default).
```

```makefile
# ?= también respeta variables de entorno:
# Si el shell tiene CC=clang exportada, Make la hereda,
# y ?= no la sobreescribe.

# Patrón común en proyectos: defaults configurables
DESTDIR ?=
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
LIBDIR ?= $(PREFIX)/lib
MANDIR ?= $(PREFIX)/share/man

install:
	install -d $(DESTDIR)$(BINDIR)
	install -m 755 myapp $(DESTDIR)$(BINDIR)/myapp
```

## Append (+=)

El operador `+=` agrega texto al valor existente de una variable,
separado por un espacio. Su comportamiento depende de si la
variable original fue definida con `=` o `:=`:

```makefile
# Si la variable original usó =, += mantiene expansión recursiva:
CFLAGS = -Wall
CFLAGS += -Wextra
CFLAGS += -std=c17
# Equivalente a: CFLAGS = -Wall -Wextra -std=c17
# Sigue siendo recursively expanded.

# Si la variable original usó :=, += hace expansión inmediata:
CFLAGS := -Wall
CFLAGS += -Wextra
CFLAGS += -std=c17
# Equivalente a: CFLAGS := -Wall -Wextra -std=c17
# Sigue siendo simply expanded.
```

```makefile
# Patrón típico: construir flags incrementalmente:
CFLAGS := -std=c17

# Warnings:
CFLAGS += -Wall -Wextra -Wpedantic

# Optimización según modo:
ifdef DEBUG
  CFLAGS += -g -O0 -DDEBUG
else
  CFLAGS += -O2 -DNDEBUG
endif

# Sanitizers opcionales:
ifdef SANITIZE
  CFLAGS += -fsanitize=address,undefined
  LDFLAGS += -fsanitize=address,undefined
endif

all:
	$(CC) $(CFLAGS) -o main main.c $(LDFLAGS)
```

## Variables convencionales

Make reconoce ciertas variables por convención. Las reglas
implicitas las usan para compilar, enlazar, etc. Definirlas en
tu Makefile configura el comportamiento automatico:

```makefile
# --- Programas ---
CC       = gcc          # Compilador de C
CXX      = g++          # Compilador de C++
AR       = ar           # Creador de archivos .a (bibliotecas estáticas)
RM       = rm -f        # Comando para borrar archivos

# --- Flags de compilación ---
CFLAGS   = -Wall -O2    # Flags para el compilador de C
CXXFLAGS = -Wall -O2    # Flags para el compilador de C++
CPPFLAGS = -Iinclude    # Flags para el preprocesador (-I, -D)
                         # CPPFLAGS = C PreProcessor Flags (no C++)

# --- Flags de enlazado ---
LDFLAGS  = -Llib        # Flags para el linker (directorios, opciones)
LDLIBS   = -lm -lpthread  # Bibliotecas a enlazar (-l)
```

```makefile
# Las reglas implícitas de Make usan estas variables.
# Por ejemplo, la regla implícita para .c → .o es:
#
#   $(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
#
# Y para enlazar:
#
#   $(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
#
# LDFLAGS va ANTES de los objetos (rutas de búsqueda),
# LDLIBS va DESPUÉS (bibliotecas a resolver).

# Si solo defines estas variables, Make ya sabe compilar:
CC = gcc
CFLAGS = -Wall -std=c17
LDLIBS = -lm

# Con eso, "make main" compila main.c → main automáticamente.
```

## Variables desde la linea de comandos

Las variables pasadas en la linea de comandos tienen prioridad
sobre las definidas en el Makefile. Esto permite al usuario
personalizar la compilacion sin editar el archivo:

```makefile
CC = gcc
CFLAGS = -O2 -Wall
PREFIX = /usr/local
```

```bash
# Sobreescribir variables al invocar make:
make CC=clang                    # compila con clang
make CFLAGS="-O3 -march=native"  # optimización agresiva
make PREFIX=/opt/myapp install    # instalar en otra ruta
make CC=clang CFLAGS="-O0 -g"    # múltiples overrides
```

```makefile
# La directiva override impide que la línea de comandos
# sobreescriba una variable:
override CFLAGS = -Wall -Werror

# Ahora, incluso con "make CFLAGS=-O3", CFLAGS será "-Wall -Werror".
# override también funciona con +=:
override CFLAGS += -std=c17
```

```makefile
# Patrón seguro: combinar override con += para garantizar flags mínimos:
override CFLAGS += -Wall

# El usuario puede agregar flags:
#   make CFLAGS="-O3"
# Resultado: CFLAGS = -O3 -Wall
# -Wall siempre estará presente.
```

## Variables de entorno

Make hereda automaticamente las variables de entorno del shell.
La prioridad de asignacion, de mayor a menor, es:

1. Linea de comandos (`make VAR=value`)
2. Makefile (`VAR = value`)
3. Entorno del shell (`export VAR=value`)

```bash
# El shell tiene CC definida:
export CC=clang
make
# Make usa CC=clang (heredada del entorno).
# PERO si el Makefile define CC = gcc, gana el Makefile.
```

```makefile
# Para que el entorno tenga prioridad sobre el Makefile,
# invocar make con -e (environment override):
# make -e
# Ahora las variables de entorno ganan sobre las del Makefile.
# Esto rara vez se usa en la práctica.
```

```makefile
# export dentro del Makefile pasa variables a sub-makes:
CC = gcc
CFLAGS = -Wall
export CC CFLAGS

# Cuando una receta ejecuta $(MAKE) en un subdirectorio,
# el sub-make hereda CC y CFLAGS:
libs:
	$(MAKE) -C libfoo
	$(MAKE) -C libbar
# libfoo/Makefile y libbar/Makefile ven CC=gcc y CFLAGS=-Wall.

# export sin argumentos exporta TODAS las variables:
export
# Generalmente no recomendado — puede causar efectos inesperados.
```

## Variables automaticas

Las variables automaticas se definen automaticamente dentro
de cada regla. Aqui se mencionan brevemente; se cubren en detalle
en T03:

```makefile
# $@  — el target de la regla
# $<  — el primer prerequisito
# $^  — todos los prerequisitos (sin duplicados)
# $*  — el stem del pattern match (en reglas con %)

# Ejemplo rápido:
main: main.o utils.o
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
# $@ = main
# $^ = main.o utils.o

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@
# $< = el .c correspondiente
# $@ = el .o que se genera
# $* = el stem (ej: "main" cuando compila main.o desde main.c)
```

## Sustitucion en variables

Make permite hacer sustituciones de patrones dentro de una
variable, sin necesidad de funciones externas:

```makefile
# Sintaxis: $(VAR:patrón=reemplazo)
# Reemplaza el sufijo "patrón" por "reemplazo" en cada palabra.

SRCS = main.c utils.c parser.c
OBJS = $(SRCS:.c=.o)
# OBJS = main.o utils.o parser.o

DEPS = $(SRCS:.c=.d)
# DEPS = main.d utils.d parser.d
```

```makefile
# La sustitución solo aplica al FINAL de cada palabra:
FILES = foo.c bar.c.bak baz.c
OBJS = $(FILES:.c=.o)
# OBJS = foo.o bar.c.bak baz.o
# bar.c.bak no cambió: .c no está al final.
```

```makefile
# También se puede usar la función patsubst para patrones más complejos:
SRCS = src/main.c src/utils.c src/parser.c
OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))
# OBJS = build/main.o build/utils.o build/parser.o

# patsubst usa % como wildcard — más flexible que la sustitución simple.
```

```makefile
# Ejemplo completo con sustitución:
SRCS = main.c utils.c parser.c
OBJS = $(SRCS:.c=.o)
DEPS = $(SRCS:.c=.d)

TARGET = myapp

$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -MMD -c $< -o $@

-include $(DEPS)

clean:
	$(RM) $(TARGET) $(OBJS) $(DEPS)
```

## Variables multilinea

La directiva `define` permite crear variables con contenido
multilinea. Es util para secuencias de comandos o texto extenso:

```makefile
# Sintaxis:
# define NOMBRE
# contenido
# línea 2
# ...
# endef

define COMPILE_MSG
@echo "================================"
@echo "  Compiling: $<"
@echo "================================"
endef

%.o: %.c
	$(COMPILE_MSG)
	$(CC) $(CFLAGS) -c $< -o $@
```

```makefile
# define usa = por defecto (recursively expanded).
# Se puede especificar el operador:
define HELP_TEXT :=
Usage: make [target]

Targets:
  all     - Build everything
  clean   - Remove build artifacts
  install - Install to PREFIX (default: /usr/local)
  help    - Show this message
endef

help:
	@echo "$(HELP_TEXT)"
```

```makefile
# define es esencial para recetas multi-comando que se reusan:
define RUN_TESTS
@echo "Running tests..."
@cd tests && ./run_all.sh
@echo "Tests complete."
endef

test: $(TARGET)
	$(RUN_TESTS)

test-verbose: $(TARGET)
	$(RUN_TESTS)
	@echo "Verbose output above."
```

## Buenas practicas

```makefile
# 1. Usar := por defecto — comportamiento predecible.
#    Solo usar = cuando se necesita expansión diferida a propósito.
CC      := gcc
CFLAGS  := -Wall -Wextra -std=c17

# 2. Usar ?= para defaults que el usuario puede sobreescribir.
PREFIX  ?= /usr/local
DESTDIR ?=

# 3. Agrupar todas las variables al inicio del Makefile.
#    Primero programas, luego flags, luego rutas:
CC       := gcc
AR       := ar
RM       := rm -f

CFLAGS   := -Wall -Wextra -std=c17
CPPFLAGS := -Iinclude
LDFLAGS  :=
LDLIBS   := -lm

SRC_DIR  := src
BUILD_DIR := build
TARGET   := myapp

# 4. Separar CPPFLAGS de CFLAGS:
#    CPPFLAGS = flags del preprocesador (-I, -D)
#    CFLAGS   = flags del compilador (-Wall, -O2, -std)
CPPFLAGS := -Iinclude -I/usr/local/include -DVERSION=\"1.0\"
CFLAGS   := -Wall -Wextra -O2

# 5. Separar LDFLAGS de LDLIBS:
#    LDFLAGS = opciones del linker (-L, -Wl,...)
#    LDLIBS  = bibliotecas (-l)
LDFLAGS := -Llib
LDLIBS  := -lm -lpthread

# 6. Construir flags incrementalmente con +=:
CFLAGS := -std=c17
CFLAGS += -Wall -Wextra
ifdef DEBUG
  CFLAGS += -g -O0
else
  CFLAGS += -O2
endif

# 7. Usar variables derivadas con sustitución:
SRCS := $(wildcard $(SRC_DIR)/*.c)
OBJS := $(SRCS:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)
DEPS := $(OBJS:.o=.d)
```

---

## Ejercicios

### Ejercicio 1 — Expansion recursiva vs inmediata

```makefile
# Crear un Makefile con las siguientes variables y predecir
# la salida antes de ejecutar:
#
# VERSION = 1.0
# A = "Version $(VERSION)"
# B := "Version $(VERSION)"
# VERSION = 2.0
#
# all:
# 	@echo A = $(A)
# 	@echo B = $(B)
#
# Luego cambiar el orden: definir A y B ANTES de VERSION = 1.0.
# Predecir y verificar qué cambia.
```

### Ejercicio 2 — Makefile configurable

```makefile
# Escribir un Makefile para un proyecto con src/main.c y src/utils.c
# que cumpla:
# - CC, CFLAGS, PREFIX deben ser sobreescribibles desde la línea de comandos
# - Si DEBUG=1, agregar -g -O0 -DDEBUG a CFLAGS
# - Si SANITIZE=1, agregar -fsanitize=address,undefined
# - Targets: all, clean, install (instala en $(PREFIX)/bin)
# - Usar := para flags, ?= para defaults, += para construcción incremental
# Probar con:
#   make
#   make CC=clang DEBUG=1
#   make SANITIZE=1 PREFIX=/tmp/test install
```

### Ejercicio 3 — Sustitucion y variables derivadas

```makefile
# Dado un proyecto con archivos .c en src/ y headers en include/:
#   src/main.c  src/parser.c  src/lexer.c  src/codegen.c
#
# Escribir un Makefile que:
# - Detecte todos los .c con $(wildcard)
# - Derive OBJS con sustitución $(SRCS:src/%.c=build/%.o)
# - Derive DEPS cambiando .o por .d
# - Compile cada .o en build/ (crear el directorio si no existe)
# - Incluya los .d para dependencias automáticas (-MMD)
# - Tenga un target "info" que imprima SRCS, OBJS y DEPS
# Verificar que agregar un nuevo .c no requiere editar el Makefile.
```
