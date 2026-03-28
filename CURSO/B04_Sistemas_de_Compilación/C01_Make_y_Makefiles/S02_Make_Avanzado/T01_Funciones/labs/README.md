# Lab -- Funciones de Make

## Objetivo

Usar las funciones integradas de Make para descubrir archivos automaticamente,
transformar listas de paths, filtrar fuentes por modulo, iterar sobre
subdirectorios, obtener informacion del sistema, definir funciones
reutilizables, aplicar condicionales en linea, y validar la configuracion con
diagnosticos. Al finalizar, sabras construir un Makefile que gestione un
proyecto C multi-directorio sin listar archivos manualmente.

## Prerequisitos

- GCC instalado
- Make instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `main.c` | Programa principal (incluye headers de core y net) |
| `app.h` | Header de la aplicacion |
| `engine.h` | Header del modulo core/engine |
| `engine.c` | Implementacion core/engine |
| `parser.h` | Header del modulo core/parser |
| `parser.c` | Implementacion core/parser |
| `client.h` | Header del modulo net/client |
| `client.c` | Implementacion net/client |
| `server.h` | Header del modulo net/server |
| `server.c` | Implementacion net/server |
| `Makefile.explore` | Makefile exploratorio: imprime resultado de cada funcion |
| `Makefile.validate` | Makefile que demuestra error, warning e info |
| `Makefile.project` | Makefile completo que integra todas las funciones |

---

## Parte 1 -- Estructura del proyecto

**Objetivo**: Crear la organizacion de subdirectorios `src/core/`, `src/net/`
e `include/` que los Makefiles del laboratorio esperan.

### Paso 1.1 -- Examinar los archivos disponibles

```bash
ls *.c *.h
```

Salida esperada:

```
app.h  client.c  client.h  engine.c  engine.h  main.c  parser.c  parser.h  server.c  server.h
```

Todos los fuentes y headers estan en el directorio actual, planos. Los
Makefiles del laboratorio esperan una estructura con subdirectorios.

### Paso 1.2 -- Crear la estructura de directorios

```bash
mkdir -p src/core src/net include build
```

Verifica:

```bash
ls -d src/ src/core/ src/net/ include/ build/
```

Salida esperada:

```
build/  include/  src/  src/core/  src/net/
```

### Paso 1.3 -- Mover los archivos a su lugar

```bash
cp main.c src/
cp engine.c parser.c src/core/
cp client.c server.c src/net/
cp app.h engine.h parser.h client.h server.h include/
```

### Paso 1.4 -- Verificar la estructura completa

```bash
find src/ include/ -type f | sort
```

Salida esperada:

```
include/app.h
include/client.h
include/engine.h
include/parser.h
include/server.h
src/core/engine.c
src/core/parser.c
src/main.c
src/net/client.c
src/net/server.c
```

### Paso 1.5 -- Examinar un fuente que depende de la estructura

```bash
cat src/main.c
```

Salida esperada:

```c
#include <stdio.h>
#include "app.h"
#include "engine.h"
#include "client.h"

int main(void) {
    printf("=== Function Demo ===\n");
    app_run();
    engine_start();
    client_connect();
    printf("=== Done ===\n");
    return 0;
}
```

Observa que los `#include` usan rutas relativas (`"app.h"`, no
`"include/app.h"`). El Makefile debera pasar `-Iinclude` al compilador para que
gcc encuentre los headers en el directorio `include/`.

---

## Parte 2 -- wildcard

**Objetivo**: Usar `$(wildcard)` para descubrir archivos `.c` y `.h`
automaticamente. Observar que pasa con directorios inexistentes.

### Paso 2.1 -- Ejecutar Makefile.explore y observar la seccion wildcard

```bash
make -f Makefile.explore 2>&1 | head -8
```

Salida esperada:

```
=== wildcard ===
SRCS_CORE = src/core/engine.c src/core/parser.c
SRCS_NET  = src/net/client.c src/net/server.c
SRCS_TOP  = src/main.c
ALL_SRCS  = src/main.c src/core/engine.c src/core/parser.c src/net/client.c src/net/server.c
HEADERS   = include/app.h include/client.h include/engine.h include/parser.h include/server.h

```

Las lineas relevantes del Makefile son:

```makefile
SRCS_CORE := $(wildcard src/core/*.c)
SRCS_NET  := $(wildcard src/net/*.c)
SRCS_TOP  := $(wildcard src/*.c)
ALL_SRCS  := $(SRCS_TOP) $(SRCS_CORE) $(SRCS_NET)
HEADERS   := $(wildcard include/*.h)
```

Cada `$(wildcard)` expandio un patron glob y devolvio los archivos que
coinciden. No hubo que listar ningun archivo manualmente.

### Paso 2.2 -- Predecir que pasa con un directorio inexistente

Predice: si agregas `$(wildcard src/gui/*.c)` y el directorio `src/gui/` no
existe, que contendra la variable? Dara error Make? Dara un warning? Devolvera
una cadena vacia?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.3 -- Verificar con un directorio que no existe

Crea un Makefile temporal para comprobarlo:

```bash
cat > Makefile_test <<'EOF'
GHOST := $(wildcard src/gui/*.c)
$(info GHOST = [$(GHOST)])
$(info length = $(words $(GHOST)))
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
GHOST = []
length = 0
done
```

`$(wildcard)` devuelve una cadena vacia cuando ningun archivo coincide. No da
error ni advertencia. Esto es silencioso, lo cual puede ser un bug dificil de
detectar si el nombre del directorio tiene un typo. Por eso es buena practica
validar con `$(error)` despues de un wildcard critico.

### Paso 2.4 -- Limpiar

```bash
rm -f Makefile_test
```

---

## Parte 3 -- patsubst

**Objetivo**: Transformar paths de `src/*.c` a `build/*.o` preservando la
estructura de subdirectorios.

### Paso 3.1 -- Observar la salida de patsubst en Makefile.explore

```bash
make -f Makefile.explore 2>&1 | grep -A 3 "=== patsubst ==="
```

Salida esperada:

```
=== patsubst ===
ALL_SRCS -> OBJS:
  src/main.c src/core/engine.c src/core/parser.c src/net/client.c src/net/server.c
  build/main.o build/core/engine.o build/core/parser.o build/net/client.o build/net/server.o
```

La linea clave del Makefile es:

```makefile
OBJS := $(patsubst src/%.c,build/%.o,$(ALL_SRCS))
```

El `%` captura todo lo que hay entre `src/` y `.c`, incluyendo subdirectorios.
Asi `src/core/engine.c` se transforma en `build/core/engine.o` porque `%`
capturo `core/engine`.

### Paso 3.2 -- Predecir un caso con archivos que no matchean

Predice: si `ALL_SRCS` contuviera tambien `lib/extra.c`, que pasaria con ese
elemento al aplicar `$(patsubst src/%.c,build/%.o,...)`? Se transformaria, daria
error, o se dejaria intacto?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Verificar con un elemento que no matchea

```bash
cat > Makefile_test <<'EOF'
MIXED := src/main.c lib/extra.c src/core/engine.c
OBJS  := $(patsubst src/%.c,build/%.o,$(MIXED))
$(info OBJS = $(OBJS))
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
OBJS = build/main.o lib/extra.c build/core/engine.o
done
```

`lib/extra.c` no matchea el patron `src/%.c`, asi que se deja intacta en la
lista. `patsubst` nunca da error por un elemento que no matchea; simplemente lo
copia sin cambios. Esta es una diferencia clave con `subst`, que busca
subcadenas literales en todo el texto.

### Paso 3.4 -- Limpiar

```bash
rm -f Makefile_test
```

---

## Parte 4 -- filter y filter-out

**Objetivo**: Separar fuentes por modulo y excluir archivos especificos de
listas.

### Paso 4.1 -- Observar la salida de filter en Makefile.explore

```bash
make -f Makefile.explore 2>&1 | grep -A 4 "=== filter"
```

Salida esperada:

```
=== filter / filter-out ===
CORE_SRCS = src/core/engine.c src/core/parser.c
NET_SRCS  = src/net/client.c src/net/server.c
NO_MAIN   = src/core/engine.c src/core/parser.c src/net/client.c src/net/server.c

```

Las lineas relevantes del Makefile son:

```makefile
CORE_SRCS := $(filter src/core/%,$(ALL_SRCS))
NET_SRCS  := $(filter src/net/%,$(ALL_SRCS))
NO_MAIN   := $(filter-out src/main.c,$(ALL_SRCS))
```

`filter` retiene solo las palabras que matchean el patron. `filter-out` retiene
solo las que NO matchean.

### Paso 4.2 -- Predecir un caso practico

Supongamos que tienes esta lista:

```
src/main.c src/core/engine.c src/core/parser.c src/net/client.c src/net/server.c
```

Predice: que devolvera `$(filter-out src/core/%,$(ALL_SRCS))`? Cuantos
elementos tendra el resultado?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Verificar la prediccion

```bash
cat > Makefile_test <<'EOF'
ALL_SRCS := src/main.c src/core/engine.c src/core/parser.c src/net/client.c src/net/server.c
EXCLUDED := $(filter-out src/core/%,$(ALL_SRCS))
$(info EXCLUDED = $(EXCLUDED))
$(info count    = $(words $(EXCLUDED)))
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
EXCLUDED = src/main.c src/net/client.c src/net/server.c
count    = 3
done
```

Se eliminaron los dos archivos de `src/core/` y se conservaron los demas (3
elementos). `filter-out` es especialmente util para excluir `main.c` al enlazar
tests, evitando tener dos funciones `main()`.

### Paso 4.4 -- Verificar con multiples patrones

```bash
cat > Makefile_test <<'EOF'
FILES := src/main.c include/app.h Makefile README.md src/core/engine.c
CODE  := $(filter %.c %.h,$(FILES))
OTHER := $(filter-out %.c %.h,$(FILES))
$(info CODE  = $(CODE))
$(info OTHER = $(OTHER))
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
CODE  = src/main.c include/app.h src/core/engine.c
OTHER = Makefile README.md
done
```

Se pueden combinar multiples patrones separados por espacio. `filter` retiene
todo lo que coincide con **cualquiera** de los patrones.

### Paso 4.5 -- Limpiar

```bash
rm -f Makefile_test
```

---

## Parte 5 -- foreach

**Objetivo**: Iterar sobre subdirectorios para recolectar fuentes y generar
flags de compilacion.

### Paso 5.1 -- Observar la salida de foreach en Makefile.explore

```bash
make -f Makefile.explore 2>&1 | grep -A 3 "=== foreach ==="
```

Salida esperada:

```
=== foreach ===
MOD_SRCS  = src/core/engine.c src/core/parser.c src/net/client.c src/net/server.c
INC_FLAGS = -Iinclude -Iinclude

```

Las lineas relevantes del Makefile son:

```makefile
SUBDIRS   := core net
MOD_SRCS  := $(foreach dir,$(SUBDIRS),$(wildcard src/$(dir)/*.c))
INC_FLAGS := $(foreach dir,$(SUBDIRS),-Iinclude)
```

`foreach` itera sobre cada palabra de `SUBDIRS`. Para cada iteracion, asigna
la palabra a `dir` y expande la expresion. Los resultados se concatenan con
espacios.

### Paso 5.2 -- Predecir el efecto de agregar un subdirectorio

Predice: si cambias `SUBDIRS := core net` a `SUBDIRS := core net utils` pero
el directorio `src/utils/` no existe, que contendra `MOD_SRCS`? Se agregaran
archivos fantasma? Se vaciara la lista? Se mantendran los existentes?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.3 -- Verificar la prediccion

```bash
cat > Makefile_test <<'EOF'
SUBDIRS  := core net utils
MOD_SRCS := $(foreach dir,$(SUBDIRS),$(wildcard src/$(dir)/*.c))
$(info MOD_SRCS = $(MOD_SRCS))
$(info count    = $(words $(MOD_SRCS)))
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
MOD_SRCS = src/core/engine.c src/core/parser.c src/net/client.c src/net/server.c
count    = 4
done
```

La iteracion sobre `utils` ejecuto `$(wildcard src/utils/*.c)` que devolvio
cadena vacia (el directorio no existe). `foreach` simplemente concateno ese
resultado vacio con los demas. No hay error ni advertencia. Esto refuerza lo
aprendido sobre `wildcard` en la parte 2: el silencio ante la ausencia puede
ser un bug sutil.

### Paso 5.4 -- foreach para generar flags de compilacion

```bash
cat > Makefile_test <<'EOF'
MODULES   := core net io
INC_FLAGS := $(foreach mod,$(MODULES),-Imodules/$(mod)/include)
$(info INC_FLAGS = $(INC_FLAGS))
$(info count     = $(words $(INC_FLAGS)))
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
INC_FLAGS = -Imodules/core/include -Imodules/net/include -Imodules/io/include
count     = 3
done
```

`foreach` produce un resultado por cada elemento de la lista. Este patron es la
clave de la escalabilidad: para agregar un modulo nuevo, basta con agregarlo a
la lista.

### Paso 5.5 -- Limpiar

```bash
rm -f Makefile_test
```

---

## Parte 6 -- shell

**Objetivo**: Obtener informacion del sistema (uname, date, conteo de archivos)
con `$(shell)` y entender sus implicaciones de rendimiento.

### Paso 6.1 -- Observar la salida de shell en Makefile.explore

```bash
make -f Makefile.explore 2>&1 | grep -A 4 "=== shell ==="
```

Salida esperada (los valores de KERNEL y ARCH dependen de tu sistema):

```
=== shell ===
FILE_COUNT = 5
KERNEL     = Linux
ARCH       = x86_64

```

Las lineas relevantes del Makefile son:

```makefile
FILE_COUNT := $(shell ls src/core/*.c src/net/*.c src/*.c 2>/dev/null | wc -l)
KERNEL     := $(shell uname -s)
ARCH       := $(shell uname -m)
```

`$(shell)` ejecuta un comando del shell y captura su stdout. Los saltos de
linea se reemplazan por espacios.

### Paso 6.2 -- Predecir la diferencia entre := y =

Considera estas dos formas:

```makefile
DATE_LAZY  = $(shell date +%H:%M:%S)
DATE_EAGER := $(shell date +%H:%M:%S)
```

Predice: cual de las dos ejecuta el comando `date` cada vez que se referencia
la variable? Cual lo ejecuta solo una vez al leer el Makefile?

Intenta predecir antes de continuar al siguiente paso.

### Paso 6.3 -- Verificar la diferencia

```bash
cat > Makefile_test <<'EOF'
DATE_EAGER := $(shell date +%H:%M:%S.%N)
DATE_LAZY   = $(shell date +%H:%M:%S.%N)

.PHONY: test
test:
	@echo "EAGER ref 1: $(DATE_EAGER)"
	@sleep 0.1
	@echo "EAGER ref 2: $(DATE_EAGER)"
	@echo "---"
	@echo "LAZY  ref 1: $(DATE_LAZY)"
	@sleep 0.1
	@echo "LAZY  ref 2: $(DATE_LAZY)"
EOF
make -f Makefile_test
```

Salida esperada (los valores exactos varian):

```
EAGER ref 1: 14:30:25.123456789
EAGER ref 2: 14:30:25.123456789
---
LAZY  ref 1: 14:30:25.345678901
LAZY  ref 2: 14:30:25.456789012
```

Las dos referencias a `DATE_EAGER` muestran el mismo valor (se evaluo una sola
vez con `:=`). Las dos referencias a `DATE_LAZY` muestran valores distintos
(se evalua cada vez con `=`). Regla: **siempre usar `:=` con `$(shell)`**.

### Paso 6.4 -- Preferir funciones nativas sobre $(shell)

```bash
cat > Makefile_test <<'EOF'
SRCS_SHELL  := $(shell ls src/core/*.c 2>/dev/null)
SRCS_NATIVE := $(wildcard src/core/*.c)
$(info shell:  $(SRCS_SHELL))
$(info native: $(SRCS_NATIVE))
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
shell:  src/core/engine.c src/core/parser.c
native: src/core/engine.c src/core/parser.c
done
```

Ambos producen el mismo resultado, pero `$(wildcard)` es preferible porque no
invoca un subproceso, es portable entre sistemas, y se evalua durante la lectura
del Makefile. Usa `$(shell)` solo cuando no exista una funcion nativa
equivalente: obtener hashes de git, fechas, informacion del sistema.

### Paso 6.5 -- Limpiar

```bash
rm -f Makefile_test
```

---

## Parte 7 -- call

**Objetivo**: Definir funciones reutilizables con parametros `$(1)`, `$(2)` e
invocarlas con `$(call)`.

### Paso 7.1 -- Observar la salida de call en Makefile.explore

```bash
make -f Makefile.explore 2>&1 | grep -A 3 "=== call ==="
```

Salida esperada:

```
=== call ===
src_to_obj(src/core/engine.c) = build/core/engine.o
log(INFO,Build started) = [INFO] Build started

```

Las lineas relevantes del Makefile son:

```makefile
src_to_obj = $(patsubst src/%.c,build/%.o,$(1))
log = [$(1)] $(2)
```

Se definen como variables normales, pero usan `$(1)` y `$(2)` como
placeholders. Al invocar con `$(call nombre,arg1,arg2)`, los argumentos
sustituyen a `$(1)` y `$(2)`.

### Paso 7.2 -- Predecir la salida de una funcion personalizada

Considera esta definicion:

```makefile
reverse = $(2) $(1)
```

Predice: que devolvera `$(call reverse,hello,world)`? Y si solo pasas un
argumento: `$(call reverse,hello)`, que ocurrira con `$(2)`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 7.3 -- Verificar funciones con call

```bash
cat > Makefile_test <<'EOF'
reverse = $(2) $(1)
src_to_obj = $(patsubst src/%.c,build/%.o,$(1))
log = [$(1)] $(2)

$(info reverse(hello,world)  = [$(call reverse,hello,world)])
$(info reverse(hello)        = [$(call reverse,hello)])
$(info src_to_obj(engine.c)  = $(call src_to_obj,src/core/engine.c))
$(info src_to_obj(multi)     = $(call src_to_obj,src/main.c src/net/client.c))
$(info log(WARN,Missing)     = $(call log,WARN,Missing file))
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
reverse(hello,world)  = [world hello]
reverse(hello)        = [ hello]
src_to_obj(engine.c)  = build/core/engine.o
src_to_obj(multi)     = build/main.o build/net/client.o
log(WARN,Missing)     = [WARN] Missing file
done
```

Con un solo argumento en `reverse`, `$(2)` queda vacio, y el resultado es
` hello` (con un espacio al inicio donde estaba el `$(2)` vacio). Observa
tambien que `src_to_obj` funciona con multiples archivos a la vez porque
`patsubst` opera sobre cada palabra de la lista.

### Paso 7.4 -- Observar call en una recipe real

Examina como `Makefile.project` usa `call` dentro de recipes:

```bash
grep -n 'call log' Makefile.project
```

Salida esperada:

```
73:	$(call log,LD,$@)
79:	$(call log,CC,$<)
```

La funcion `log` esta definida como:

```makefile
log = @echo "[$(1)] $(2)"
```

Esto permite imprimir mensajes consistentes como `[CC] src/main.c` y
`[LD] funcapp` sin repetir el formato echo en cada recipe.

### Paso 7.5 -- Limpiar

```bash
rm -f Makefile_test
```

---

## Parte 8 -- if

**Objetivo**: Usar `$(if)` para seleccionar flags de compilacion segun el valor
de una variable.

### Paso 8.1 -- Observar la salida de if en Makefile.explore

```bash
make -f Makefile.explore 2>&1 | grep -A 2 "=== if ==="
```

Salida esperada:

```
=== if ===
DEBUG is empty -> MODE_FLAG = -O2

```

La linea relevante del Makefile es:

```makefile
DEBUG :=
MODE_FLAG := $(if $(DEBUG),-g -O0,-O2)
```

`DEBUG` esta vacio, asi que `$(if)` evalua a la rama "else" (`-O2`).

### Paso 8.2 -- Predecir el efecto de definir DEBUG

Predice: si ejecutas `make -f Makefile.explore DEBUG=1`, que valor tendra
`MODE_FLAG`?

Recuerda que `$(if)` evalua la condicion como: cadena vacia = false, cualquier
otra cadena = true.

Intenta predecir antes de continuar al siguiente paso.

### Paso 8.3 -- Verificar con DEBUG definido y otros valores

```bash
cat > Makefile_test <<'EOF'
DEBUG ?=
MODE_FLAG := $(if $(DEBUG),-g -O0,-O2)
$(info DEBUG     = [$(DEBUG)])
$(info MODE_FLAG = $(MODE_FLAG))
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
DEBUG     = []
MODE_FLAG = -O2
done
```

Ahora con `DEBUG=1`:

```bash
make -f Makefile_test DEBUG=1
```

Salida esperada:

```
DEBUG     = [1]
MODE_FLAG = -g -O0
done
```

Con `DEBUG=1`, la cadena `1` no es vacia, asi que `$(if)` evalua a la rama
"then" (`-g -O0`).

### Paso 8.4 -- Predecir un caso sutil

Predice: que devuelve `$(if 0,TRUE,FALSE)`? Recuerda que `$(if)` evalua texto,
no expresiones booleanas. La cadena `0` es vacia?

Intenta predecir antes de continuar al siguiente paso.

### Paso 8.5 -- Verificar valores de verdad en Make

```bash
cat > Makefile_test <<'EOF'
$(info $(if yes,TRUE,FALSE))
$(info $(if ,TRUE,FALSE))
$(info $(if 0,TRUE,FALSE))
$(info $(if  ,TRUE,FALSE))
$(info )
$(info "yes" -> no vacio -> TRUE)
$(info ""    -> vacio    -> FALSE)
$(info "0"   -> no vacio -> TRUE  (no es booleano))
$(info " "   -> strip lo reduce a vacio -> FALSE)
.PHONY: test
test:
	@echo "done"
EOF
make -f Makefile_test
```

Salida esperada:

```
TRUE
FALSE
TRUE
FALSE

"yes" -> no vacio -> TRUE
""    -> vacio    -> FALSE
"0"   -> no vacio -> TRUE  (no es booleano)
" "   -> strip lo reduce a vacio -> FALSE
done
```

Punto clave: `$(if)` evalua texto, no expresiones booleanas. Cualquier cadena
no vacia (incluyendo `"0"`, `"false"`, `"no"`) se considera verdadera.
`$(if)` aplica `strip` a la condicion antes de evaluarla, asi que `" "` (solo
espacios) es equivalente a vacio.

### Paso 8.6 -- Observar if en Makefile.project

```bash
grep -n 'if.*DEBUG' Makefile.project
```

Salida esperada:

```
45:CFLAGS += $(if $(DEBUG),-g -O0 -DDEBUG,-O2 -DNDEBUG)
```

Este patron permite al usuario elegir entre compilacion de depuracion y
compilacion optimizada desde la linea de comandos con `make DEBUG=1`.

### Paso 8.7 -- Limpiar

```bash
rm -f Makefile_test
```

---

## Parte 9 -- error, warning, info

**Objetivo**: Usar `$(error)`, `$(warning)` e `$(info)` para validar la
configuracion e imprimir diagnosticos durante la lectura del Makefile.

### Paso 9.1 -- Ejecutar Makefile.validate en modo normal

```bash
make -f Makefile.validate
```

Salida esperada:

```
Validating build configuration...
MODE   = release
CFLAGS = -O2
Validation passed. MODE=release, CFLAGS=-O2
```

`$(info)` imprime sin prefijo de archivo/linea. El modo `release` es valido,
asi que la ejecucion continua normalmente.

### Paso 9.2 -- Ejecutar con modo debug (provoca warning)

```bash
make -f Makefile.validate MODE=debug
```

Salida esperada:

```
Validating build configuration...
Makefile.validate:15: Debug mode enabled -- performance will be reduced
MODE   = debug
CFLAGS = -g -O0
Validation passed. MODE=debug, CFLAGS=-g -O0
```

Observa la diferencia entre las lineas:

- `Validating build configuration...` es `$(info)`: sin prefijo
- `Makefile.validate:15:` es `$(warning)`: con prefijo de archivo y linea, pero
  Make **continua** la ejecucion

### Paso 9.3 -- Predecir que pasa con un modo invalido

Examina esta seccion de `Makefile.validate`:

```makefile
VALID_MODES := debug release

ifeq ($(filter $(MODE),$(VALID_MODES)),)
  $(error MODE "$(MODE)" is not valid. Use one of: $(VALID_MODES))
endif
```

Predice: si pasas `MODE=invalid`, que devuelve
`$(filter invalid,debug release)`? Y si el resultado es vacio, que hace
`$(error)`? Llegara a ejecutar el target `validate`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 9.4 -- Verificar con un modo invalido

```bash
make -f Makefile.validate MODE=invalid
```

Salida esperada:

```
Validating build configuration...
Makefile.validate:20: *** MODE "invalid" is not valid. Use one of: debug release.  Stop.
```

`$(filter invalid,debug release)` devolvio cadena vacia (no hay match), lo
cual activo el bloque `ifeq`. `$(error)` imprimio el mensaje con prefijo de
archivo/linea y **aborto** Make inmediatamente. La linea `MODE = ...` nunca se
imprimio porque Make se detuvo antes.

### Paso 9.5 -- Comparar los tres niveles de diagnostico

Resumiendo lo observado en los pasos anteriores:

```
$(info ...)    -> sin prefijo,                  continua ejecucion
$(warning ...) -> con prefijo Archivo:Linea:,   continua ejecucion
$(error ...)   -> con prefijo Archivo:Linea:***, ABORTA Make
```

Las tres se expanden al **leer** el Makefile, no al ejecutar una recipe. Si
colocas un `$(error)` dentro de una recipe, se expande antes de que la recipe
se ejecute.

---

## Parte 10 -- Proyecto completo

**Objetivo**: Integrar todas las funciones en un Makefile funcional que compila
el proyecto multi-directorio. Ejecutar `Makefile.project` y observar como
wildcard, patsubst, foreach, shell, filter, call, if, info y error trabajan
juntos.

### Paso 10.1 -- Examinar la configuracion automatica

```bash
head -30 Makefile.project
```

Salida esperada:

```
# Makefile.project -- Proyecto completo usando funciones de Make
#
# Uso: make -f Makefile.project
#
# Estructura esperada:
#   src/main.c
#   src/core/engine.c  src/core/parser.c
#   src/net/client.c   src/net/server.c
#   include/*.h
#   build/              (generado)

# --- Herramientas ---
CC  := gcc
RM  := rm -f

# --- Directorios ---
SRC_DIR   := src
INC_DIR   := include
BUILD_DIR := build

# --- Subdirectorios de fuentes ---
SRC_SUBDIRS := core net

# --- Descubrimiento automatico (wildcard + foreach) ---
SRCS := $(wildcard $(SRC_DIR)/*.c) \
        $(foreach dir,$(SRC_SUBDIRS),$(wildcard $(SRC_DIR)/$(dir)/*.c))

# --- Objetos (patsubst) ---
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
```

Identifica las funciones de cada parte del laboratorio:

- `$(wildcard)` + `$(foreach)` para descubrir fuentes (partes 2 y 5)
- `$(patsubst)` para generar rutas de objetos (parte 3)
- `$(shell)` para obtener OS, ARCH, DATE (parte 6)
- `$(call log,...)` para mensajes de compilacion (parte 7)
- `$(if $(DEBUG),...)` para seleccionar flags (parte 8)
- `$(info)` y `$(error)` para validacion (parte 9)

### Paso 10.2 -- Compilar el proyecto

```bash
make -f Makefile.project
```

Salida esperada:

```
=== Build Configuration ===
  CC     = gcc
  CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -DNDEBUG
  OS     = Linux
  ARCH   = x86_64
  DATE   = 2026-03-18
  SRCS   = 5 files
============================

[CC] src/main.c
[CC] src/core/engine.c
[CC] src/core/parser.c
[CC] src/net/client.c
[CC] src/net/server.c
[LD] funcapp
```

Make descubrio los 5 archivos fuente automaticamente, creo los directorios de
build necesarios, compilo cada `.c` a `.o`, y enlazo el ejecutable `funcapp`.

### Paso 10.3 -- Verificar la estructura de build generada

```bash
find build -type f | sort
```

Salida esperada:

```
build/core/engine.o
build/core/parser.o
build/main.o
build/net/client.o
build/net/server.o
```

`$(patsubst src/%.c,build/%.o,...)` transformo las rutas de `src/` a `build/`
preservando los subdirectorios `core/` y `net/`.

### Paso 10.4 -- Ejecutar el programa

```bash
./funcapp
```

Salida esperada:

```
=== Function Demo ===
[core/engine] Engine started
[net/client] Client connected
=== Done ===
```

El proyecto compila y ejecuta correctamente usando un Makefile que descubre
archivos automaticamente.

### Paso 10.5 -- Predecir: recompilacion sin cambios

Predice: si ejecutas `make -f Makefile.project` de nuevo sin cambiar nada, que
ocurrira? Se recompilara algo? Se imprimira la configuracion?

Intenta predecir antes de continuar al siguiente paso.

### Paso 10.6 -- Verificar que Make no recompila innecesariamente

```bash
make -f Makefile.project
```

Salida esperada:

```
=== Build Configuration ===
  CC     = gcc
  CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -DNDEBUG
  OS     = Linux
  ARCH   = x86_64
  DATE   = 2026-03-18
  SRCS   = 5 files
============================

make: 'funcapp' is up to date.
```

Los mensajes de `$(info)` siempre se imprimen (se expanden al leer el
Makefile), pero ninguna recipe se ejecuto porque todo esta al dia.

### Paso 10.7 -- Compilar en modo debug

```bash
make -f Makefile.project clean
make -f Makefile.project DEBUG=1
```

Salida esperada (observa CFLAGS):

```
=== Build Configuration ===
  CC     = gcc
  CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -O0 -DDEBUG
  OS     = Linux
  ARCH   = x86_64
  DATE   = 2026-03-18
  SRCS   = 5 files
============================

[CC] src/main.c
[CC] src/core/engine.c
[CC] src/core/parser.c
[CC] src/net/client.c
[CC] src/net/server.c
[LD] funcapp
```

`$(if $(DEBUG),-g -O0 -DDEBUG,-O2 -DNDEBUG)` selecciono los flags de
depuracion. El mismo Makefile soporta ambos modos sin cambiar una sola linea.

### Paso 10.8 -- Usar el target info para listar variables

```bash
make -f Makefile.project info
```

Salida esperada:

```
=== Build Configuration ===
  CC     = gcc
  CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -O0 -DDEBUG
  OS     = Linux
  ARCH   = x86_64
  DATE   = 2026-03-18
  SRCS   = 5 files
============================

Sources:
  src/main.c
  src/core/engine.c
  src/core/parser.c
  src/net/client.c
  src/net/server.c
Objects:
  build/main.o
  build/core/engine.o
  build/core/parser.o
  build/net/client.o
  build/net/server.o
Build dirs:
  build/
  build/core/
  build/net/
```

Este target usa `$(foreach)` dentro de las recipes para iterar sobre las listas
y mostrar cada elemento. Observa como la estructura de directorios en `build/`
refleja la de `src/`.

### Paso 10.9 -- Predecir: que pasa si modificas un solo archivo

Predice: si ejecutas `touch src/core/engine.c` y luego `make`, cuantos
archivos se recompilaran? Solo `engine.c`? Todos?

Intenta predecir antes de continuar al siguiente paso.

### Paso 10.10 -- Verificar la recompilacion selectiva

```bash
touch src/core/engine.c
make -f Makefile.project
```

Salida esperada:

```
=== Build Configuration ===
  CC     = gcc
  CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -g -O0 -DDEBUG
  OS     = Linux
  ARCH   = x86_64
  DATE   = 2026-03-18
  SRCS   = 5 files
============================

[CC] src/core/engine.c
[LD] funcapp
```

Solo `engine.c` se recompilo y luego se re-enlazo el ejecutable. Los otros 4
archivos fuente no se tocaron. Esta es la ventaja de la compilacion separada
combinada con las funciones de Make para descubrimiento automatico.

### Paso 10.11 -- Predecir: que pasa si eliminamos los fuentes

Predice: si renombras `src/` a `src_backup/`, que hara el Makefile? Que funcion
detectara el problema? Dara error o simplemente no compilara nada?

Intenta predecir antes de continuar al siguiente paso.

### Paso 10.12 -- Verificar la validacion de fuentes

```bash
mv src src_backup
make -f Makefile.project 2>&1; echo "Exit: $?"
mv src_backup src
```

Salida esperada:

```
=== Build Configuration ===
  CC     = gcc
  CFLAGS = -std=c11 -Wall -Wextra -Wpedantic -O2 -DNDEBUG
  OS     = Linux
  ARCH   = x86_64
  DATE   = 2026-03-18
  SRCS   = 0 files
============================

Makefile.project:62: *** No source files found in src/.  Stop.
Exit: 2
```

`$(wildcard)` devolvio vacio, `$(words)` mostro 0 files, y el `$(error)` aborto
Make con un mensaje claro. Sin esa validacion, Make simplemente diria "Nothing
to be done" sin explicar por que.

---

## Limpieza final

```bash
make -f Makefile.project clean
rm -rf src/ include/ build/ funcapp
```

Verifica que solo quedan los archivos originales del laboratorio:

```bash
ls
```

Salida esperada:

```
app.h     client.h  engine.h  Makefile.explore   Makefile.validate  parser.c  README.md  server.h
client.c  engine.c  main.c    Makefile.project    parser.h           server.c
```

---

## Conceptos reforzados

1. `$(wildcard patron)` expande globs durante la lectura del Makefile. Devuelve
   cadena vacia si ningun archivo coincide, sin dar error ni advertencia. Es la
   forma correcta de obtener listas de archivos en variables (en vez de
   `$(shell ls)`).

2. `$(patsubst patron,reemplazo,texto)` transforma cada palabra que matchea el
   patron. El `%` captura cualquier secuencia de caracteres, incluyendo
   subdirectorios. Los elementos que no matchean se copian sin cambios.

3. `$(filter patron,texto)` retiene solo las palabras que matchean.
   `$(filter-out patron,texto)` retiene las que NO matchean. Son esenciales
   para separar fuentes por modulo o excluir archivos especificos. Ambas
   aceptan multiples patrones separados por espacio.

4. `$(foreach var,lista,expresion)` itera sobre una lista, expandiendo la
   expresion para cada elemento. No es un bucle imperativo; es expansion de
   texto. Combinado con `$(wildcard)`, permite recolectar fuentes de multiples
   subdirectorios con una sola linea.

5. `$(shell comando)` ejecuta un comando del shell y captura su stdout.
   Siempre usar `:=` con `$(shell)` para evitar ejecuciones repetidas. Preferir
   funciones nativas de Make (`wildcard`, `dir`, `notdir`) cuando existan
   equivalentes.

6. `$(call variable,arg1,arg2)` invoca una variable como funcion. `$(1)` y
   `$(2)` dentro de la variable se sustituyen por los argumentos. Permite
   definir funciones reutilizables para transformaciones de paths y mensajes de
   compilacion consistentes.

7. `$(if condicion,then,else)` evalua la condicion como texto: cadena vacia es
   false, cualquier otra cadena es true. No soporta comparadores (`==`, `<`).
   Para validar si un valor esta en una lista, se combina con `$(filter)`.

8. `$(info)` imprime sin prefijo y continua. `$(warning)` imprime con prefijo
   `Archivo:Linea:` y continua. `$(error)` imprime con prefijo y **aborta**
   Make. Las tres se expanden al **leer** el Makefile, no al ejecutar recipes.

9. La combinacion de `$(wildcard)` + `$(foreach)` + `$(patsubst)` +
   `$(if)` + `$(error)` es el patron fundamental para Makefiles escalables:
   descubrir fuentes automaticamente, transformarlas en objetos, seleccionar
   flags segun el entorno, y validar la configuracion antes de compilar.
   Agregar un nuevo modulo solo requiere modificar una variable.
