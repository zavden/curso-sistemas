# T01 — Funciones

## Sintaxis de funciones

Las funciones en Make se invocan con la misma sintaxis que las
variables, pero reciben argumentos. El formato general es
`$(funcion argumentos)` o `${funcion argumentos}`. El nombre de la
funcion y el primer argumento se separan por un espacio. Los
argumentos entre si se separan por comas:

```makefile
# Formato general:
#   $(funcion arg1,arg2,arg3)
#   ${funcion arg1,arg2,arg3}
#
# El espacio entre el nombre de la funcion y el primer argumento
# es opcional pero convencional.
# Los espacios despues de las comas SON parte del argumento.

# CUIDADO con los espacios despues de las comas:
RESULT := $(patsubst %.c,%.o,main.c utils.c)
# RESULT = main.o utils.o

RESULT := $(patsubst %.c, %.o,main.c utils.c)
# RESULT =  main.o  utils.o   (con espacios extra al inicio de cada nombre)
# El patron %.c SI matchea, pero el reemplazo es " %.o" (con espacio),
# asi que cada .o generado tiene un espacio antes: " main.o", " utils.o".
# Esto corrompe silenciosamente la lista — un target " main.o" no es
# lo mismo que "main.o", causando errores dificiles de diagnosticar.

# Regla: NO poner espacios despues de las comas en funciones.
```

```makefile
# Las funciones se pueden anidar:
SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))
# Primero se expande $(wildcard src/*.c),
# luego el resultado se pasa como tercer argumento a patsubst.

# No hay limite de anidamiento (pero se vuelve ilegible):
NAMES := $(notdir $(basename $(wildcard src/*.c)))
# 1. wildcard  → src/main.c src/utils.c
# 2. basename  → src/main src/utils
# 3. notdir    → main utils
```

## $(wildcard pattern)

Expande un patron glob dentro del Makefile. A diferencia de los
globs en las recipes (que ejecuta el shell), `$(wildcard)` se
expande durante la lectura del Makefile. Es la forma correcta de
obtener listas de archivos en variables:

```makefile
# Obtener todos los .c del directorio actual:
SRCS := $(wildcard *.c)
# Si existen main.c, utils.c y parser.c:
# SRCS = main.c parser.c utils.c

# Obtener .c de un subdirectorio:
SRCS := $(wildcard src/*.c)
# SRCS = src/main.c src/parser.c src/utils.c
```

```makefile
# Multiples patrones separados por espacio:
SRCS := $(wildcard src/*.c lib/*.c)
# Combina los resultados de ambos patrones.

# Buscar en subdirectorios especificos:
SRCS := $(wildcard src/*.c src/net/*.c src/io/*.c)

# Buscar multiples extensiones:
HDRS := $(wildcard include/*.h include/*.hpp)
```

```makefile
# Si ningun archivo coincide, devuelve una cadena vacia.
# NO da error — esto es silencioso:
SRCS := $(wildcard noexiste/*.c)
# SRCS = (vacio)

# Esto puede ser un bug silencioso. Si esperas que haya archivos
# y no los hay (por ejemplo un directorio mal escrito), Make no
# te avisa. Para detectar este caso:
SRCS := $(wildcard src/*.c)
ifeq ($(SRCS),)
  $(error No se encontraron archivos fuente en src/)
endif
```

```makefile
# Diferencia clave con globs del shell:
# En una recipe, el glob lo expande el shell:
list_shell:
	ls src/*.c
# Si no hay .c, el shell da error: "No such file or directory"

# En una variable, el glob lo expande Make:
SRCS := $(wildcard src/*.c)
list_make:
	@echo "$(SRCS)"
# Si no hay .c, imprime una linea vacia. Sin error.
```

```makefile
# wildcard NO soporta ** (busqueda recursiva).
# Esto NO funciona como esperarias:
SRCS := $(wildcard src/**/*.c)
# Solo encuentra archivos en src/algo/*.c, no en niveles mas profundos.

# Para busqueda recursiva, usar $(shell find ...):
SRCS := $(shell find src -name '*.c')

# O listar cada subdirectorio manualmente:
SRCS := $(wildcard src/*.c src/*/*.c src/*/*/*.c)
```

## $(patsubst pattern,replacement,text)

Sustituye un patron en cada palabra del texto. El caracter `%`
actua como wildcard que captura cualquier secuencia de caracteres.
El `%` en el reemplazo se sustituye por lo que capturo el `%` del
patron:

```makefile
# Cambiar extensiones de .c a .o:
SRCS := main.c utils.c parser.c
OBJS := $(patsubst %.c,%.o,$(SRCS))
# OBJS = main.o utils.o parser.o

# El % captura "main", "utils", "parser" respectivamente,
# y lo coloca en la posicion del % en el reemplazo.
```

```makefile
# Cambiar directorios:
SRCS := src/main.c src/utils.c src/parser.c
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))
# OBJS = build/main.o build/utils.o build/parser.o

# El % captura "main", "utils", "parser".
# "src/" y ".c" son la parte literal del patron.
# "build/" y ".o" son la parte literal del reemplazo.
```

```makefile
# Si una palabra no matchea el patron, se deja intacta:
FILES := main.c utils.h parser.c config.h
OBJS := $(patsubst %.c,%.o,$(FILES))
# OBJS = main.o utils.h parser.o config.h
# Los .h no matchean %.c, asi que se copian sin cambio.
```

```makefile
# Solo el primer % en el patron es wildcard.
# Un segundo % se trata como literal:
# Esto rara vez se usa, pero es parte de la especificacion.

# Sin patron (sin %): sustitución literal exacta.
RESULT := $(patsubst main.c,main.o,main.c utils.c)
# RESULT = main.o utils.c
# Solo reemplaza la palabra exacta "main.c".
```

```makefile
# Atajo: sustitucion en referencia de variable.
# $(VAR:patron=reemplazo) equivale a $(patsubst %patron,%reemplazo,$(VAR))
SRCS := main.c utils.c parser.c
OBJS := $(SRCS:.c=.o)
# Equivalente a: $(patsubst %.c,%.o,$(SRCS))
# OBJS = main.o utils.o parser.o

# Tambien funciona con % explícito:
OBJS := $(SRCS:%.c=%.o)
# Equivalente al anterior.

# Para cambios de directorio, se necesita patsubst completo:
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))
# El atajo NO puede hacer esto — solo opera sobre sufijos.
```

## $(subst from,to,text)

Sustitucion literal (sin patrones `%`). Reemplaza toda
ocurrencia exacta de la cadena `from` por `to`:

```makefile
# Reemplazo literal:
RESULT := $(subst ee,EE,feet on the street)
# RESULT = fEEt on the strEEt

# A diferencia de patsubst, NO opera palabra por palabra.
# Busca la subcadena en todo el texto.
```

```makefile
# Uso tipico: cambiar separadores.
DIRS := src:lib:include
DIRS_SPACES := $(subst :, ,$(DIRS))
# DIRS_SPACES = src lib include
# Reemplaza cada : por un espacio.
```

```makefile
# subst vs patsubst:
#
# subst:    sustitucion literal en todo el texto
#           $(subst .c,.o,main.c.bak)  →  main.o.bak
#           (reemplaza la PRIMERA ocurrencia de .c en cada busqueda)
#           En realidad reemplaza TODAS las ocurrencias:
#           $(subst a,X,banana)  →  bXnXnX
#
# patsubst: sustitucion de patron por palabra
#           $(patsubst %.c,%.o,main.c.bak)  →  main.c.bak  (no matchea)
#           Solo matchea si el patron coincide con la palabra COMPLETA.
#
# Consecuencia: para cambiar extensiones, patsubst es mas seguro.
# subst podria reemplazar .c en medio de un nombre:
FILES := source.cfg main.c
BAD   := $(subst .c,.o,$(FILES))
# BAD = source.ofg main.o   ← source.cfg se corrompio
GOOD  := $(patsubst %.c,%.o,$(FILES))
# GOOD = source.cfg main.o  ← correcto
```

## $(filter pattern,text) y $(filter-out pattern,text)

`filter` retiene solo las palabras que coinciden con el patron.
`filter-out` retiene solo las que NO coinciden. Ambas soportan
`%` como wildcard y aceptan multiples patrones:

```makefile
# Filtrar por extension:
FILES := main.c utils.h parser.c config.h lexer.c
SRCS  := $(filter %.c,$(FILES))
HDRS  := $(filter %.h,$(FILES))
# SRCS = main.c parser.c lexer.c
# HDRS = utils.h config.h
```

```makefile
# filter-out — excluir elementos:
SRCS  := main.c utils.c test_main.c test_utils.c parser.c
PROD  := $(filter-out test_%,$(SRCS))
TESTS := $(filter test_%,$(SRCS))
# PROD  = main.c utils.c parser.c
# TESTS = test_main.c test_utils.c
```

```makefile
# Multiples patrones:
FILES := main.c utils.h parser.c Makefile README.md config.yml
CODE  := $(filter %.c %.h,$(FILES))
# CODE = main.c utils.h parser.c

OTHER := $(filter-out %.c %.h,$(FILES))
# OTHER = Makefile README.md config.yml
```

```makefile
# Uso practico: compilar objetos de produccion y test por separado:
SRCS      := $(wildcard src/*.c)
TEST_SRCS := $(wildcard tests/*.c)

ALL_OBJS  := $(patsubst %.c,%.o,$(SRCS) $(TEST_SRCS))
PROD_OBJS := $(filter src/%,$(ALL_OBJS))
TEST_OBJS := $(filter tests/%,$(ALL_OBJS))

app: $(PROD_OBJS)
	$(CC) $^ -o $@

test_runner: $(TEST_OBJS) $(filter-out src/main.o,$(PROD_OBJS))
	$(CC) $^ -o $@
```

```makefile
# filter tambien sirve para validar targets:
VALID_MODES := debug release profile
MODE ?= release

ifneq ($(filter $(MODE),$(VALID_MODES)),$(MODE))
  $(error MODE debe ser uno de: $(VALID_MODES))
endif
```

## $(sort list)

Ordena las palabras alfabeticamente y **elimina duplicados**:

```makefile
LIST := banana apple cherry apple banana date
SORTED := $(sort $(LIST))
# SORTED = apple banana cherry date
# Duplicados eliminados automaticamente.
```

```makefile
# El orden es lexico (basado en caracteres), no numerico:
NUMS := 10 2 30 1 20 3
SORTED := $(sort $(NUMS))
# SORTED = 1 10 2 20 3 30
# "10" va antes que "2" porque '1' < '2' en ASCII.
```

```makefile
# Uso practico: eliminar duplicados de listas de includes.
# Si multiples modulos agregan el mismo -I:
INCLUDES := -Iinclude -Ilib/foo -Iinclude -Ilib/bar -Ilib/foo
INCLUDES := $(sort $(INCLUDES))
# INCLUDES = -Iinclude -Ilib/bar -Ilib/foo
# Los duplicados se eliminaron.
#
# NOTA: sort cambia el orden. Si el orden de -I importa
# (prioridad de busqueda), no usar sort para deduplicar.
```

## $(word n,text), $(wordlist s,e,text) y $(words text)

Funciones para trabajar con palabras individuales de una lista:

```makefile
# $(word n,text) — extraer la palabra en la posicion n (base 1):
LIST := alpha beta gamma delta
FIRST  := $(word 1,$(LIST))
THIRD  := $(word 3,$(LIST))
# FIRST = alpha
# THIRD = gamma

# Si n es mayor que el numero de palabras, devuelve vacio:
NONE := $(word 99,$(LIST))
# NONE = (vacio)

# Si n es 0, Make da error.
```

```makefile
# $(wordlist s,e,text) — extraer palabras de la posicion s a e:
LIST := one two three four five six
MID  := $(wordlist 2,4,$(LIST))
# MID = two three four

# Si e es mayor que el numero de palabras, devuelve hasta el final:
REST := $(wordlist 3,100,$(LIST))
# REST = three four five six
```

```makefile
# $(words text) — contar el numero de palabras:
SRCS := main.c utils.c parser.c lexer.c
COUNT := $(words $(SRCS))
# COUNT = 4

# Truco: obtener la ultima palabra de una lista:
LAST := $(word $(words $(SRCS)),$(SRCS))
# $(words $(SRCS)) = 4, luego $(word 4,...) = lexer.c
# LAST = lexer.c
```

## $(dir names) y $(notdir names)

Extraen la parte del directorio y del nombre de archivo,
respectivamente. Operan sobre cada palabra de la lista:

```makefile
# $(dir names) — extraer la parte del directorio:
PATHS := src/main.c lib/utils.c include/config.h
DIRS  := $(dir $(PATHS))
# DIRS = src/ lib/ include/
# Nota: el resultado siempre termina en /

# Si no hay directorio, devuelve ./
DIRS := $(dir main.c)
# DIRS = ./
```

```makefile
# $(notdir names) — extraer el nombre del archivo (sin directorio):
PATHS := src/main.c lib/utils.c include/config.h
NAMES := $(notdir $(PATHS))
# NAMES = main.c utils.c config.h

# Si ya no tiene directorio, devuelve el nombre tal cual:
NAMES := $(notdir main.c)
# NAMES = main.c
```

```makefile
# Uso tipico: obtener nombres base para generar objetos:
SRCS  := src/main.c src/utils.c src/parser.c
NAMES := $(notdir $(SRCS))
# NAMES = main.c utils.c parser.c
OBJS  := $(addprefix build/,$(NAMES:.c=.o))
# OBJS = build/main.o build/utils.o build/parser.o
```

## $(basename names) y $(suffix names)

`basename` elimina la extension (desde el ultimo punto).
`suffix` extrae la extension (incluido el punto):

```makefile
# $(basename names):
FILES := main.c utils.h archive.tar.gz Makefile
BASES := $(basename $(FILES))
# BASES = main utils archive.tar Makefile
# archive.tar.gz → archive.tar (solo quita la ultima extension)
# Makefile → Makefile (sin punto, no cambia)
```

```makefile
# $(suffix names):
FILES := main.c utils.h archive.tar.gz Makefile
EXTS  := $(suffix $(FILES))
# EXTS = .c .h .gz
# archive.tar.gz → .gz (solo la ultima extension)
# Makefile → (vacio, sin extension)
```

```makefile
# basename conserva el directorio:
FILES := src/main.c lib/utils.so.3
BASES := $(basename $(FILES))
# BASES = src/main lib/utils.so
```

```makefile
# Uso comun: generar dependencias:
SRCS := src/main.c src/utils.c
DEPS := $(addsuffix .d,$(basename $(SRCS)))
# DEPS = src/main.d src/utils.d
```

## $(addprefix prefix,names) y $(addsuffix suffix,names)

Agregan un prefijo o sufijo a cada palabra de la lista:

```makefile
# $(addprefix prefix,names):
MODULES := main utils parser
SRCS    := $(addprefix src/,$(MODULES))
# SRCS = src/main src/utils src/parser

OBJS := $(addsuffix .o,$(SRCS))
# OBJS = src/main.o src/utils.o src/parser.o
```

```makefile
# $(addsuffix suffix,names):
DIRS := src lib tests
CLEAN_DIRS := $(addsuffix /clean,$(DIRS))
# CLEAN_DIRS = src/clean lib/clean tests/clean
```

```makefile
# Combinar ambas:
MODULES := auth db api
LIB_FLAGS := $(addprefix -l,$(MODULES))
# LIB_FLAGS = -lauth -ldb -lapi

INC_DIRS := core plugins
INC_FLAGS := $(addprefix -I,$(addsuffix /include,$(INC_DIRS)))
# INC_FLAGS = -Icore/include -Iplugins/include
```

```makefile
# Uso tipico: construir flags de inclusion:
INCLUDE_DIRS := include src/include vendor/include
CPPFLAGS := $(addprefix -I,$(INCLUDE_DIRS))
# CPPFLAGS = -Iinclude -Isrc/include -Ivendor/include
```

## $(strip string)

Elimina espacios en blanco al inicio y al final del string, y
reemplaza secuencias internas de espacios/tabs/newlines por un
solo espacio:

```makefile
MESSY :=    hello    world
CLEAN := $(strip $(MESSY))
# CLEAN = hello world
# Sin espacios extra al inicio, al final ni entre palabras.
```

```makefile
# strip es esencial al comparar strings.
# Los espacios sobrantes causan comparaciones falsas:
VAR = debug

# MAL — puede fallar si VAR tiene espacios sobrantes:
ifeq ($(VAR),debug)
  CFLAGS += -g
endif

# BIEN — strip limpia antes de comparar:
ifeq ($(strip $(VAR)),debug)
  CFLAGS += -g
endif
```

```makefile
# Donde aparecen espacios inesperados:

# 1. Despues de un valor de variable:
CC = gcc     # este comentario agrega espacios antes del #
# CC ahora es "gcc     " (con espacios al final).
# Solucion: no poner comentarios en la misma linea que la asignacion,
# o usar strip al referenciar: $(strip $(CC))

# 2. Con variables multilinea (define/endef):
define LIST
  foo
  bar
  baz
endef
CLEAN_LIST := $(strip $(LIST))
# CLEAN_LIST = foo bar baz
```

## $(foreach var,list,text)

Itera sobre una lista. Para cada palabra en `list`, asigna la
palabra a `var` y expande `text`. Los resultados se concatenan
separados por espacios:

```makefile
# Sintaxis: $(foreach variable,lista,expresion)
DIRS := src lib tests
RESULT := $(foreach d,$(DIRS),$(d)/Makefile)
# RESULT = src/Makefile lib/Makefile tests/Makefile

# Para cada d en {src, lib, tests}, expande "$(d)/Makefile".
```

```makefile
# Generar flags de inclusion para multiples directorios:
MODULES := core net io
INCLUDES := $(foreach mod,$(MODULES),-Imodules/$(mod)/include)
# INCLUDES = -Imodules/core/include -Imodules/net/include -Imodules/io/include
```

```makefile
# Recolectar fuentes de multiples subdirectorios:
MODULES := core net io
SRCS := $(foreach mod,$(MODULES),$(wildcard modules/$(mod)/src/*.c))
# Ejecuta wildcard para cada modulo y concatena los resultados.
```

```makefile
# Generar reglas con foreach + eval (avanzado):
MODULES := core net io

define MODULE_RULE
$(1)/build:
	@mkdir -p $$@
$(1)/build/%.o: $(1)/src/%.c | $(1)/build
	$$(CC) $$(CFLAGS) -c $$< -o $$@
endef

$(foreach mod,$(MODULES),$(eval $(call MODULE_RULE,$(mod))))
# Genera reglas de compilacion para cada modulo.
# Nota: $$ se usa para escapar $ en el contexto de eval.
```

```makefile
# foreach es expansion, no un bucle imperativo.
# No tiene side effects — solo produce texto.
# Para acciones imperativas, usar bucles de shell en una recipe:
process_all:
	@for dir in src lib tests; do \
		echo "Processing $$dir"; \
		$(MAKE) -C $$dir; \
	done
```

## $(shell command)

Ejecuta un comando del shell y captura su stdout. Los saltos
de linea en la salida se reemplazan por espacios:

```makefile
# Obtener el hash de git:
GIT_HASH := $(shell git rev-parse --short HEAD)
# GIT_HASH = a1b2c3d

# Obtener la fecha:
BUILD_DATE := $(shell date +%Y-%m-%d)
# BUILD_DATE = 2026-03-18

# Contar archivos:
NUM_SRCS := $(shell ls src/*.c 2>/dev/null | wc -l)
```

```makefile
# IMPORTANTE: usar := (no =) con $(shell).
# Con = (recursiva), el comando se ejecuta CADA VEZ
# que se referencia la variable:

# MAL — ejecuta git en cada uso de GIT_HASH:
GIT_HASH = $(shell git rev-parse --short HEAD)

# BIEN — ejecuta git una sola vez:
GIT_HASH := $(shell git rev-parse --short HEAD)

# Con =, si GIT_HASH aparece 10 veces, git se ejecuta 10 veces.
# Esto ralentiza la ejecucion de Make significativamente.
```

```makefile
# Los newlines se convierten en espacios:
FILE_LIST := $(shell find src -name '*.c')
# Si find devuelve:
#   src/main.c
#   src/utils.c
#   src/parser.c
# FILE_LIST = src/main.c src/utils.c src/parser.c
# (todo en una linea, separado por espacios)
```

```makefile
# Pasar flags al compilador con informacion del entorno:
CC      := gcc
VERSION := 1.2.3
GIT_HASH := $(shell git rev-parse --short HEAD 2>/dev/null || echo unknown)
BUILD_DATE := $(shell date -u +%Y-%m-%dT%H:%M:%SZ)
OS := $(shell uname -s)
ARCH := $(shell uname -m)

CPPFLAGS := -DVERSION=\"$(VERSION)\" \
            -DGIT_HASH=\"$(GIT_HASH)\" \
            -DBUILD_DATE=\"$(BUILD_DATE)\"
```

```makefile
# Rendimiento: cada $(shell ...) invoca un subproceso.
# En Makefiles grandes con muchos $(shell ...) evaluados con =,
# la ejecucion puede ser lenta.
#
# Reglas:
# 1. Siempre usar := con $(shell ...)
# 2. Preferir funciones nativas de Make cuando existan:
#    - $(wildcard *.c) en vez de $(shell ls *.c)
#    - $(dir path) en vez de $(shell dirname path)
#    - $(notdir path) en vez de $(shell basename path)
# 3. Agrupar multiples datos en un solo $(shell ...):
#    INFO := $(shell git rev-parse --short HEAD && date +%Y%m%d)
```

## $(call variable,param1,param2,...)

Invoca una variable como si fuera una funcion, pasandole
argumentos. Dentro de la variable, `$(1)`, `$(2)`, etc.
se sustituyen por los argumentos. `$(0)` es el nombre de la
variable:

```makefile
# Definir una "funcion" como variable:
reverse = $(2) $(1)

RESULT := $(call reverse,hello,world)
# RESULT = world hello
# $(1) = hello, $(2) = world
```

```makefile
# Funcion para generar la ruta de un objeto a partir de un fuente:
src_to_obj = $(patsubst src/%.c,build/%.o,$(1))

SRCS := src/main.c src/utils.c src/parser.c
OBJS := $(call src_to_obj,$(SRCS))
# OBJS = build/main.o build/utils.o build/parser.o
```

```makefile
# Funcion de logging reutilizable:
log = @echo "[$(1)] $(2)"

all: program
	$(call log,INFO,Build complete)

clean:
	$(call log,CLEAN,Removing artifacts)
	rm -f $(OBJS) $(TARGET)
```

```makefile
# Funciones mas complejas con define/endef:
define compile_module
  @echo "Compiling module: $(1)"
  $(CC) $(CFLAGS) -c $(1)/src/*.c -o $(1)/build/
endef

build_core:
	$(call compile_module,core)

build_net:
	$(call compile_module,net)
```

```makefile
# Funcion para verificar que un programa existe:
check_prog = $(if $(shell which $(1) 2>/dev/null),,\
  $(error $(1) no encontrado. Instalar con: $(2)))

$(call check_prog,gcc,sudo apt install gcc)
$(call check_prog,pkg-config,sudo apt install pkg-config)
# Si gcc no esta en PATH, Make da error con el mensaje indicado.
```

```makefile
# Patrones avanzados con call + eval + foreach:
# Generar reglas para multiples programas.
PROGRAMS := client server tool

define PROGRAM_template
$(1): $(1).o common.o
	$$(CC) $$(LDFLAGS) $$^ -o $$@
endef

$(foreach prog,$(PROGRAMS),$(eval $(call PROGRAM_template,$(prog))))
# Genera:
#   client: client.o common.o
#       $(CC) $(LDFLAGS) $^ -o $@
#   server: server.o common.o
#       $(CC) $(LDFLAGS) $^ -o $@
#   tool: tool.o common.o
#       $(CC) $(LDFLAGS) $^ -o $@

# NOTA: dentro de define+eval se usa $$ para escapar.
# eval expande una vez (convierte $$ en $),
# luego Make expande otra vez cuando ejecuta la regla.
# Si pusieras $ simple, se expandiria demasiado pronto.
```

## $(if condition,then-part,else-part)

Condicional en linea. Si `condition` (despues de strip) no es
vacia, expande `then-part`. Si es vacia, expande `else-part`.
El `else-part` es opcional:

```makefile
# La condicion es: vacio = false, no-vacio = true.
DEBUG := yes

CFLAGS := $(if $(DEBUG),-g -O0,-O2)
# DEBUG no esta vacio → CFLAGS = -g -O0

DEBUG :=
CFLAGS := $(if $(DEBUG),-g -O0,-O2)
# DEBUG esta vacio → CFLAGS = -O2
```

```makefile
# Sin else-part: si la condicion es falsa, el resultado es vacio.
VERBOSE := yes
QUIET := $(if $(VERBOSE),,-s)
# VERBOSE no vacio → QUIET = (vacio, no se agrega -s)

VERBOSE :=
QUIET := $(if $(VERBOSE),,-s)
# VERBOSE vacio → QUIET = -s
```

```makefile
# Uso tipico: asignar valor por defecto:
CC := $(if $(CC_OVERRIDE),$(CC_OVERRIDE),gcc)
# Si CC_OVERRIDE esta definido, lo usa. Si no, usa gcc.
# (Nota: ?= es mas idomático para defaults simples.)
```

```makefile
# if evalua TEXTO, no expresiones booleanas.
# No funciona: $(if 0,...) — "0" no es vacio, es true.
# No funciona: $(if $(A) == $(B),...) — no hay comparador.
# Para comparaciones, usar ifeq/ifneq o combinar con filter:

# Verificar si un valor esta en una lista:
MODE := release
VALID := $(filter $(MODE),debug release profile)
$(if $(VALID),,$(error MODE invalido: $(MODE)))
```

## $(or ...) y $(and ...)

Operadores logicos. Evaluan sus argumentos de izquierda a
derecha y cortocircuitan:

```makefile
# $(or val1,val2,...) — devuelve el primer valor no vacio:
CC := $(or $(CC_CUSTOM),$(CC_FROM_ENV),gcc)
# Si CC_CUSTOM no esta vacio, lo usa.
# Si no, prueba CC_FROM_ENV.
# Si ambos estan vacios, usa "gcc".
# Es como un "fallback chain".
```

```makefile
# $(and val1,val2,...) — si TODOS son no vacios, devuelve el ultimo.
# Si alguno es vacio, devuelve vacio:
HAS_GCC := $(shell which gcc 2>/dev/null)
HAS_MAKE := $(shell which make 2>/dev/null)

READY := $(and $(HAS_GCC),$(HAS_MAKE))
# Si ambos existen, READY = la ruta de make (ultimo argumento).
# Si alguno falta, READY = (vacio).
```

```makefile
# Combinar or/and con if:
CAN_BUILD := $(and $(shell which $(CC) 2>/dev/null),$(wildcard src/*.c))
$(if $(CAN_BUILD),,$(error Falta compilador o archivos fuente))
```

## $(error text), $(warning text) y $(info text)

Funciones de diagnostico. Se expanden durante la lectura del
Makefile (o al expandir la variable/receta que las contiene):

```makefile
# $(error text) — imprime el mensaje y ABORTA Make inmediatamente:
ifndef CC
  $(error CC no esta definido. Definir con: make CC=gcc)
endif
# Salida:
# Makefile:2: *** CC no esta definido. Definir con: make CC=gcc.  Stop.
```

```makefile
# $(warning text) — imprime el mensaje pero CONTINUA la ejecucion:
ifeq ($(CC),gcc)
  $(warning Usando gcc por defecto. Considerar clang para mejor diagnostico.)
endif
# Salida:
# Makefile:2: Usando gcc por defecto. Considerar clang para mejor diagnostico.
# (Make continua normalmente.)
```

```makefile
# $(info text) — imprime el mensaje sin prefijo de archivo/linea:
$(info Build configuration:)
$(info   CC      = $(CC))
$(info   CFLAGS  = $(CFLAGS))
$(info   TARGET  = $(TARGET))
$(info )
# Salida:
# Build configuration:
#   CC      = gcc
#   CFLAGS  = -Wall -O2
#   TARGET  = myapp
#
# (Sin el prefijo "Makefile:N:" que agrega warning.)
```

```makefile
# Diferencias:
#   $(info ...)    → stdout, sin prefijo, continua
#   $(warning ...) → stderr, con prefijo "Makefile:N:", continua
#   $(error ...)   → stderr, con prefijo "Makefile:N:", ABORTA

# Las tres se expanden en el momento en que Make lee la linea.
# Si estan dentro de una recipe, se expanden ANTES de ejecutarla:
all:
	$(info Esto se imprime antes que cualquier recipe)
	@echo "Recipe ejecutandose"
# Salida:
# Esto se imprime antes que cualquier recipe
# Recipe ejecutandose
```

```makefile
# Patron de validacion al inicio del Makefile:
REQUIRED_BINS := gcc ar pkg-config
$(foreach bin,$(REQUIRED_BINS),\
  $(if $(shell which $(bin) 2>/dev/null),,\
    $(error "$(bin)" no encontrado en PATH)))
```

## Ejemplo completo

Un Makefile real para un proyecto con multiples subdirectorios
que demuestra wildcard, patsubst, foreach, shell, filter, call
y funciones de diagnostico:

```
proyecto/
  Makefile
  include/
    app.h
    config.h
  src/
    main.c
    core/
      engine.c
      parser.c
    net/
      client.c
      server.c
  tests/
    test_engine.c
    test_parser.c
```

```makefile
# Makefile

# --- Herramientas ---
CC       := gcc
AR       := ar
RM       := rm -f

# --- Informacion del entorno (shell) ---
GIT_HASH   := $(shell git rev-parse --short HEAD 2>/dev/null || echo dev)
BUILD_DATE := $(shell date -u +%Y-%m-%dT%H:%M:%SZ)
NPROC      := $(shell nproc 2>/dev/null || echo 1)

# --- Directorios ---
SRC_DIR    := src
INC_DIR    := include
BUILD_DIR  := build
TEST_DIR   := tests

# --- Descubrimiento automatico de fuentes (wildcard + foreach) ---
SRC_SUBDIRS := core net
SRCS := $(wildcard $(SRC_DIR)/*.c) \
        $(foreach dir,$(SRC_SUBDIRS),$(wildcard $(SRC_DIR)/$(dir)/*.c))
# SRCS = src/main.c src/core/engine.c src/core/parser.c
#        src/net/client.c src/net/server.c

# --- Fuentes de tests ---
TEST_SRCS := $(wildcard $(TEST_DIR)/*.c)
# TEST_SRCS = tests/test_engine.c tests/test_parser.c

# --- Objetos derivados (patsubst) ---
OBJS      := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
TEST_OBJS := $(patsubst $(TEST_DIR)/%.c,$(BUILD_DIR)/test/%.o,$(TEST_SRCS))
DEPS      := $(OBJS:.o=.d) $(TEST_OBJS:.o=.d)

# --- Flags ---
CFLAGS   := -std=c17 -Wall -Wextra -Wpedantic
CPPFLAGS := -I$(INC_DIR) -DGIT_HASH=\"$(GIT_HASH)\" \
            -DBUILD_DATE=\"$(BUILD_DATE)\"
LDFLAGS  :=
LDLIBS   :=

ifdef DEBUG
  CFLAGS += -g -O0 -DDEBUG
else
  CFLAGS += -O2 -DNDEBUG
endif

# --- Target principal ---
TARGET      := myapp
TEST_TARGET := test_runner

# --- Directorios de build necesarios (sort elimina duplicados) ---
BUILD_DIRS := $(sort $(dir $(OBJS) $(TEST_OBJS)))

# --- Funcion reutilizable: compilar .c a .o (call) ---
define compile
  @echo "  CC    $(notdir $(1))"
  @mkdir -p $(dir $(2))
  $(CC) $(CPPFLAGS) $(CFLAGS) -MMD -MP -c $(1) -o $(2)
endef

# --- Validacion (error/warning/info) ---
$(info === Build Configuration ===)
$(info   CC       = $(CC))
$(info   CFLAGS   = $(CFLAGS))
$(info   GIT_HASH = $(GIT_HASH))
$(info   SRCS     = $(words $(SRCS)) archivos)
$(info   TESTS    = $(words $(TEST_SRCS)) archivos)
$(info ============================)
$(info )

ifeq ($(strip $(SRCS)),)
  $(error No se encontraron archivos fuente en $(SRC_DIR)/)
endif

# Advertir si no hay tests:
ifeq ($(strip $(TEST_SRCS)),)
  $(warning No se encontraron tests en $(TEST_DIR)/)
endif

# ============================================================
# Reglas
# ============================================================

.PHONY: all clean test info dirs

all: $(TARGET)

# Enlazar el ejecutable principal (filter-out excluye test objects):
$(TARGET): $(OBJS)
	@echo "  LD    $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Enlazar tests — excluir main.o para evitar doble main():
$(TEST_TARGET): $(TEST_OBJS) $(filter-out $(BUILD_DIR)/main.o,$(OBJS))
	@echo "  LD    $@"
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

# Compilar fuentes del proyecto:
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(call compile,$<,$@)

# Compilar fuentes de tests:
$(BUILD_DIR)/test/%.o: $(TEST_DIR)/%.c
	$(call compile,$<,$@)

# Incluir dependencias generadas por -MMD:
-include $(DEPS)

# Ejecutar tests:
test: $(TEST_TARGET)
	@echo "Running tests..."
	./$(TEST_TARGET)

# Mostrar variables (info + foreach):
info:
	@echo "Sources:"
	@$(foreach src,$(SRCS),echo "  $(src)";)
	@echo "Objects:"
	@$(foreach obj,$(OBJS),echo "  $(obj)";)
	@echo "Build dirs:"
	@$(foreach d,$(BUILD_DIRS),echo "  $(d)";)

# Limpieza:
clean:
	$(RM) -r $(BUILD_DIR) $(TARGET) $(TEST_TARGET)

# Compilacion paralela por defecto:
MAKEFLAGS += -j$(NPROC)
```

```bash
# Sesion de uso:

make
# === Build Configuration ===
#   CC       = gcc
#   CFLAGS   = -std=c17 -Wall -Wextra -Wpedantic -O2 -DNDEBUG
#   GIT_HASH = a1b2c3d
#   SRCS     = 5 archivos
#   TESTS    = 2 archivos
# ============================
#
#   CC    main.c
#   CC    engine.c
#   CC    parser.c
#   CC    client.c
#   CC    server.c
#   LD    myapp

make DEBUG=1
# Recompila con -g -O0 -DDEBUG

make test
# Compila y ejecuta los tests

make info
# Muestra fuentes, objetos y directorios

make clean
# Elimina build/ y los ejecutables
```

---

## Ejercicios

### Ejercicio 1 — Transformaciones de listas

```makefile
# Dada la siguiente lista de archivos:
#
# FILES := src/main.c src/utils.c lib/parser.c lib/lexer.c \
#          tests/test_main.c tests/test_parser.c include/app.h
#
# Escribir un Makefile con un target "info" que imprima (usando
# funciones de Make, NO comandos del shell):
#
#   1. Solo los .c                          → $(filter ...)
#   2. Solo los .c de src/                  → $(filter src/%.c,...)
#   3. Los .c excluyendo tests              → $(filter-out tests/%,...)
#   4. Los nombres sin directorio           → $(notdir ...)
#   5. Los nombres sin extension            → $(basename $(notdir ...))
#   6. Objetos en build/ para cada .c       → $(patsubst ...)
#   7. Numero total de archivos             → $(words ...)
#   8. El tercer archivo de la lista        → $(word 3,...)
#   9. La lista ordenada sin duplicados     → $(sort ...)
#  10. Flags -I para cada directorio unico  → combinacion de dir, sort, addprefix
#
# Verificar cada resultado con @echo.
```

### Ejercicio 2 — Funciones reutilizables con call

```makefile
# Crear un Makefile que defina las siguientes "funciones" usando
# variables con $(1), $(2) y se invoquen con $(call ...):
#
# 1. log — recibe nivel (INFO/WARN/ERROR) y mensaje.
#          Imprime: [NIVEL] mensaje
#          Ejemplo: $(call log,INFO,Compilacion iniciada)
#
# 2. check_dir — recibe un directorio.
#          Si no existe, lo crea con mkdir -p.
#          Ejemplo: $(call check_dir,build/obj)
#
# 3. compile_to — recibe archivo fuente y directorio destino.
#          Genera el comando gcc que compila el .c a .o en ese directorio.
#          Ejemplo: $(call compile_to,src/main.c,build)
#          Debe producir: gcc $(CFLAGS) -c src/main.c -o build/main.o
#
# Usar estas funciones en un Makefile funcional con targets
# all, clean, y al menos 3 archivos .c.
#
# PISTA para compile_to: combinar notdir y patsubst para extraer
# el nombre del .o a partir del .c.
```

### Ejercicio 3 — Proyecto multi-modulo automatico

```makefile
# Crear un proyecto con la siguiente estructura:
#
#   modules/math/src/add.c
#   modules/math/src/mul.c
#   modules/math/include/math_ops.h
#   modules/string/src/upper.c
#   modules/string/src/lower.c
#   modules/string/include/string_ops.h
#   src/main.c
#   include/app.h
#
# Escribir un Makefile que:
#
# 1. Defina MODULES := math string
#
# 2. Use $(foreach) y $(wildcard) para recolectar automaticamente
#    todos los .c de todos los modulos (modules/*/src/*.c) y de src/
#
# 3. Use $(foreach) y $(addprefix) para generar los -I flags
#    para cada modules/*/include/ automaticamente
#
# 4. Use $(patsubst) para generar objetos en build/ manteniendo
#    la estructura de subdirectorios
#
# 5. Use $(call) para definir una funcion que genere la regla de
#    compilacion de cada modulo (con eval + foreach)
#
# 6. Use $(shell git ...) para incrustar el hash del commit
#
# 7. Use $(info) para imprimir un resumen de la configuracion
#
# 8. Use $(error) si algun modulo no tiene archivos .c
#
# Verificar que agregar un nuevo modulo (por ejemplo "io")
# solo requiere agregarlo a la variable MODULES.
```
