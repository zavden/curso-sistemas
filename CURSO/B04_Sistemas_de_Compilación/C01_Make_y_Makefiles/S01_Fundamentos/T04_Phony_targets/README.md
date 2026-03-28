# T04 — Phony targets

## Que es un phony target

Un **phony target** es un target que no representa un archivo.
Su recipe ejecuta una accion, pero no produce un archivo con
el nombre del target:

```makefile
# "clean" no crea un archivo llamado "clean".
# Es una accion: eliminar archivos generados.
clean:
	rm -f *.o program

# "all" no crea un archivo llamado "all".
# Es una agrupacion: compilar todo.
all: program

# "test" no crea un archivo llamado "test".
# Es una accion: ejecutar las pruebas.
test:
	./run_tests.sh
```

```makefile
# Comparacion con un target real:
program: main.o utils.o
	gcc -o program main.o utils.o
# "program" SI crea un archivo llamado "program".
# Make verifica si el archivo existe y si esta actualizado.

# Un phony target no crea nada — solo ejecuta comandos.
clean:
	rm -f *.o program
# Make no deberia verificar si existe un archivo "clean".
# Deberia ejecutar la recipe siempre.
# Pero sin .PHONY, Make SI busca el archivo...
```

## El problema sin .PHONY

Si existe un archivo con el mismo nombre que el target, Make
cree que el target ya esta construido y no ejecuta la recipe:

```bash
# Crear un Makefile con un target clean:
$ cat Makefile
clean:
	rm -f *.o program

# Funciona normalmente:
$ make clean
rm -f *.o program

# Ahora crear un archivo llamado "clean":
$ touch clean

# Intentar de nuevo:
$ make clean
make: 'clean' is up to date.
# Make encontro el archivo "clean", no tiene prerequisites,
# entonces concluye que esta "actualizado" y no ejecuta nada.
```

```bash
# Lo mismo ocurre con cualquier target falso:
$ touch all
$ make all
make: 'all' is up to date.

$ touch test
$ make test
make: 'test' is up to date.

# Basta con que alguien cree un archivo con ese nombre
# (accidentalmente, o un programa que genere un archivo "test")
# para que make deje de funcionar.
```

```makefile
# Por que ocurre esto:
# Make trata TODOS los targets como archivos por defecto.
# Para el target "clean":
# 1. Make busca un archivo llamado "clean"
# 2. Si existe y no tiene prerequisites → "esta actualizado"
# 3. No ejecuta la recipe

# El mismo mecanismo que hace que Make sea eficiente
# (no recompilar si el .o ya esta actualizado) causa
# este problema con targets que no son archivos.
```

## .PHONY

La directiva `.PHONY` declara que un target no es un archivo.
Make siempre ejecuta su recipe, sin verificar la existencia
del archivo:

```makefile
.PHONY: clean
clean:
	rm -f *.o program
```

```bash
# Ahora funciona incluso si existe un archivo "clean":
$ touch clean
$ make clean
rm -f *.o program
# Make ignora el archivo "clean" porque sabe que es phony.
```

```makefile
# .PHONY es un target especial (special built-in target).
# Los prerequisites de .PHONY se tratan como phony targets.
# Make nunca busca archivos con esos nombres.
# Make siempre ejecuta sus recipes.

.PHONY: clean
clean:
	rm -f *.o program

# Sin .PHONY:
#   make busca archivo "clean" → lo encuentra → no ejecuta
# Con .PHONY:
#   make sabe que "clean" no es archivo → siempre ejecuta
```

## Multiples phony targets

Se pueden declarar varios targets phony en una sola linea
o en lineas separadas. Ambas formas son equivalentes:

```makefile
# Forma 1 — todos en una linea:
.PHONY: all clean install uninstall test help

# Forma 2 — en lineas separadas:
.PHONY: all
.PHONY: clean
.PHONY: install
.PHONY: test

# Forma 3 — junto a cada target (comun en proyectos grandes):
.PHONY: all
all: program

.PHONY: clean
clean:
	rm -f *.o program

.PHONY: install
install: program
	cp program /usr/local/bin/

.PHONY: test
test: program
	./run_tests.sh
```

```makefile
# La forma 3 es la mas legible en Makefiles grandes.
# Cada .PHONY queda junto al target que declara.
# Es facil ver que un target es phony sin buscar en otra parte.

# .PHONY es acumulativo:
.PHONY: all
.PHONY: clean
# Equivale a:
.PHONY: all clean
# Make acumula todos los prerequisites de .PHONY.
```

## Targets convencionales

Los proyectos Unix siguen convenciones para los nombres
de phony targets. Estos son los estandares:

```makefile
# all — compilar todo. Debe ser el PRIMER target (target por defecto).
.PHONY: all
all: program

# clean — eliminar archivos generados (.o, ejecutables, temporales).
.PHONY: clean
clean:
	rm -f *.o program

# install — copiar ejecutable y archivos al sistema.
.PHONY: install
install: program
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 program $(DESTDIR)$(PREFIX)/bin/

# uninstall — remover lo que install coloco.
.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/program

# test (o check) — ejecutar suite de tests.
.PHONY: test
test: program
	./run_tests.sh

# dist — crear tarball de distribucion.
.PHONY: dist
dist:
	tar czf program-$(VERSION).tar.gz *.c *.h Makefile README

# distclean — clean + eliminar archivos de configuracion.
.PHONY: distclean
distclean: clean
	rm -f config.h config.mk

# help — mostrar targets disponibles.
.PHONY: help
help:
	@echo "Targets disponibles:"
	@echo "  all       — compilar todo"
	@echo "  clean     — eliminar archivos generados"
	@echo "  install   — instalar en PREFIX"
	@echo "  test      — ejecutar tests"
	@echo "  help      — mostrar esta ayuda"
```

```makefile
# Convenciones de PREFIX y DESTDIR:
PREFIX  ?= /usr/local
DESTDIR ?=

# PREFIX es el directorio base de instalacion:
# /usr/local/bin, /usr/local/lib, /usr/local/include

# DESTDIR es un prefijo para empaquetado (RPM, DEB):
# make install DESTDIR=/tmp/staging
# Instala en /tmp/staging/usr/local/bin/program
# El paquete luego mueve los archivos a las rutas reales.

.PHONY: install
install: program
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 program $(DESTDIR)$(PREFIX)/bin/program

# install(1) es preferible a cp/mkdir:
# - Crea directorios si no existen (-d)
# - Establece permisos (-m 755)
# - Es atomico (copia a temporal + rename)
```

## Phony como prerequisite

Un phony target puede ser prerequisite de otro target.
Esto fuerza que la recipe del target dependiente se ejecute
siempre, porque Make considera que el prerequisite phony
"siempre cambia":

```makefile
.PHONY: force
force:

# "data.txt" depende de "force" (phony, siempre "cambia").
# Make siempre ejecuta la recipe de data.txt:
data.txt: force
	generate_data > data.txt

# Sin "force", Make solo regeneraria data.txt si no existe.
# Con "force", lo regenera siempre.
```

```makefile
# Patron FORCE — alternativa clasica a .PHONY para targets reales:
# (En Makefiles antiguos, antes de que existiera .PHONY)

FORCE:
.PHONY: FORCE

# Cualquier target que dependa de FORCE se reconstruye siempre:
config.h: FORCE
	./configure --generate-header

# Esto es util cuando el target SI es un archivo,
# pero queremos regenerarlo siempre.
# No se puede declarar config.h como .PHONY
# porque ES un archivo real.
```

```makefile
# Phony como prerequisite de phony — encadenar acciones:
.PHONY: deploy
deploy: test install
	@echo "Deployed successfully"

# "deploy" primero ejecuta "test", luego "install".
# Si test falla (exit code != 0), Make se detiene
# y no ejecuta install ni deploy.

.PHONY: test
test: program
	./run_tests.sh

.PHONY: install
install: program
	install -m 755 program $(DESTDIR)$(PREFIX)/bin/
```

## Phony recursivos

Un phony target puede depender de targets reales.
El phony se ejecuta siempre, pero sus prerequisites reales
solo se reconstruyen si estan desactualizados:

```makefile
.PHONY: all
all: program1 program2 libutils.a
# "all" es phony — Make siempre procesa esta regla.
# program1, program2, libutils.a son archivos reales.
# Make los reconstruye SOLO si sus fuentes cambiaron.

program1: program1.o utils.o
	gcc -o program1 program1.o utils.o

program2: program2.o utils.o
	gcc -o program2 program2.o utils.o

libutils.a: utils.o
	ar rcs libutils.a utils.o
```

```makefile
# Patron para subdirectorios (recursive make):
SUBDIRS = lib src tests

.PHONY: all $(SUBDIRS)
all: $(SUBDIRS)

$(SUBDIRS):
	$(MAKE) -C $@

# Declarar los subdirectorios como phony evita conflictos
# con los directorios reales del mismo nombre.
# Sin .PHONY, Make veria que "lib/" existe y diria
# "lib is up to date".

# Establecer orden de dependencia entre subdirectorios:
src: lib
tests: src
# "lib" se compila primero, luego "src", luego "tests".
```

## Agrupar phony targets — patron para make help

El target `help` es una convencion para documentar
los targets disponibles en un Makefile:

```makefile
# Forma basica — echo manual:
.PHONY: help
help:
	@echo "Usage: make [TARGET]"
	@echo ""
	@echo "Targets:"
	@echo "  all        Build all binaries (default)"
	@echo "  clean      Remove generated files"
	@echo "  install    Install to PREFIX (default: /usr/local)"
	@echo "  uninstall  Remove installed files"
	@echo "  test       Run test suite"
	@echo "  dist       Create distribution tarball"
	@echo "  help       Show this help message"
```

```makefile
# Forma avanzada — auto-documentar con comentarios:
# Cada target tiene un comentario con ## que sirve de descripcion.

.PHONY: help
help: ## Show this help message
	@grep -E '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | \
		awk -F ':.*## ' '{printf "  %-15s %s\n", $$1, $$2}'

.PHONY: all
all: program ## Build all binaries

.PHONY: clean
clean: ## Remove generated files
	$(RM) *.o program

.PHONY: install
install: program ## Install to PREFIX
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 program $(DESTDIR)$(PREFIX)/bin/

.PHONY: test
test: program ## Run test suite
	./run_tests.sh
```

```bash
# El resultado de make help con la forma avanzada:
$ make help
  help             Show this help message
  all              Build all binaries
  clean            Remove generated files
  install          Install to PREFIX
  test             Run test suite
```

## Convencion de clean

El target `clean` tiene convenciones especificas sobre
que eliminar y como hacerlo:

```makefile
# Que limpiar:
#   - Archivos objeto (.o)
#   - Ejecutables generados
#   - Archivos temporales
#   - Archivos generados por el build
#
# Que NO limpiar:
#   - Archivos fuente (.c, .h)
#   - Makefile
#   - README, documentacion
#   - Archivos de configuracion (eso es distclean)

CC     = gcc
CFLAGS = -Wall -Wextra
TARGET = program
OBJS   = main.o utils.o parser.o

.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)

# $(RM) es una variable predefinida de Make.
# Su valor por defecto es "rm -f".
# Usar $(RM) en lugar de rm -f es la convencion.
```

```makefile
# Por que $(RM) en lugar de rm -f:
#   - $(RM) es portátil (Make la define segun la plataforma)
#   - En GNU Make, $(RM) = "rm -f"
#   - El -f evita errores si el archivo no existe:
#       rm program    → error si "program" no existe
#       rm -f program → silencioso si no existe, exit 0

# Distintos niveles de limpieza:
.PHONY: clean
clean:
	$(RM) $(OBJS) $(TARGET)

.PHONY: distclean
distclean: clean
	$(RM) config.h config.mk
	$(RM) -r build/

# clean:     elimina archivos del build
# distclean: elimina TODO lo generado (incluyendo configure)
#            Deja el directorio como recien descargado.
```

```makefile
# No usar rm con wildcards agresivos:

# PELIGROSO:
clean:
	rm -rf *        # Borra TODO incluido codigo fuente

# PELIGROSO:
clean:
	rm -f *.c *.h   # Borra archivos fuente

# SEGURO — listar explicitamente lo que se elimina:
clean:
	$(RM) $(OBJS) $(TARGET)

# SEGURO — wildcards solo para extensiones de build:
clean:
	$(RM) *.o $(TARGET)
```

## .PHONY y rendimiento

En Makefiles grandes, declarar phony targets mejora el
rendimiento. Sin `.PHONY`, Make busca en disco si existe
un archivo con ese nombre para cada target:

```makefile
# Sin .PHONY, Make hace una stat() por cada target para ver
# si existe como archivo. En un Makefile con muchos phony
# targets, son llamadas al sistema innecesarias.

# Con .PHONY, Make sabe que no es un archivo y no lo busca.
# En Makefiles pequenos la diferencia es imperceptible.
# En proyectos con cientos de targets y builds en red (NFS),
# las stat() innecesarias pueden sumar latencia real.

# Regla: siempre declarar .PHONY por correccion, no solo
# por rendimiento. El beneficio principal es evitar el
# conflicto con archivos del mismo nombre.
```

```makefile
# Otra consecuencia de .PHONY en rendimiento:
# Un phony target SIEMPRE se ejecuta.
# Si un target real depende de un phony, tambien se
# "reconstruye" siempre:

.PHONY: version
version:
	echo "#define VERSION \"1.0\"" > version.h

# Si program.o depende de version.h, y version.h depende
# de "version" (phony), entonces program.o se recompila
# SIEMPRE, incluso si la fuente no cambio.
# Esto puede ser intencionado o un error de diseno.
# Usar con cuidado: no hacer que targets reales dependan
# de phony targets a menos que sea necesario.
```

## Ejemplo completo

```makefile
# === Project configuration ===
CC       = gcc
CFLAGS   = -Wall -Wextra -std=c11
LDFLAGS  =
LDLIBS   =

PREFIX   ?= /usr/local
DESTDIR  ?=
VERSION  ?= 1.0.0

# === Files ===
TARGET   = calculator
SRCS     = main.c parser.c eval.c utils.c
OBJS     = $(SRCS:.c=.o)

# === Default target ===
.PHONY: all
all: $(TARGET) ## Build the project

# === Build rules ===
$(TARGET): $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# === Installation ===
.PHONY: install
install: $(TARGET) ## Install to PREFIX (default: /usr/local)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

.PHONY: uninstall
uninstall: ## Remove installed files
	$(RM) $(DESTDIR)$(PREFIX)/bin/$(TARGET)

# === Testing ===
.PHONY: test
test: $(TARGET) ## Run tests
	@echo "Running tests..."
	@./$(TARGET) --test && echo "All tests passed."

.PHONY: check
check: test ## Alias for test

# === Cleaning ===
.PHONY: clean
clean: ## Remove build artifacts
	$(RM) $(OBJS) $(TARGET)

.PHONY: distclean
distclean: clean ## Remove all generated files
	$(RM) config.h
	$(RM) *.tar.gz

# === Distribution ===
.PHONY: dist
dist: ## Create distribution tarball
	tar czf $(TARGET)-$(VERSION).tar.gz \
		$(SRCS) *.h Makefile README.md LICENSE

# === Help ===
.PHONY: help
help: ## Show available targets
	@echo "$(TARGET) v$(VERSION) — Build system"
	@echo ""
	@echo "Usage: make [TARGET]"
	@echo ""
	@grep -E '^[a-zA-Z_-]+:.*##' $(MAKEFILE_LIST) | \
		awk -F ':.*## ' '{printf "  %-15s %s\n", $$1, $$2}'
	@echo ""
	@echo "Variables:"
	@echo "  CC=$(CC)  CFLAGS=$(CFLAGS)"
	@echo "  PREFIX=$(PREFIX)  DESTDIR=$(DESTDIR)"
```

```bash
# Uso del Makefile completo:

$ make              # compila todo (target all por defecto)
gcc -Wall -Wextra -std=c11 -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -c -o parser.o parser.c
gcc -Wall -Wextra -std=c11 -c -o eval.o eval.c
gcc -Wall -Wextra -std=c11 -c -o utils.o utils.c
gcc  -o calculator main.o parser.o eval.o utils.o

$ make test         # ejecuta los tests
Running tests...
All tests passed.

$ make clean        # limpia los archivos generados
rm -f main.o parser.o eval.o utils.o calculator

$ make install PREFIX=/opt/myapp
install -d /opt/myapp/bin
install -m 755 calculator /opt/myapp/bin/calculator

$ make help         # muestra la ayuda
calculator v1.0.0 — Build system

Usage: make [TARGET]

  all              Build the project
  install          Install to PREFIX (default: /usr/local)
  uninstall        Remove installed files
  test             Run tests
  check            Alias for test
  clean            Remove build artifacts
  distclean        Remove all generated files
  dist             Create distribution tarball
  help             Show available targets

Variables:
  CC=gcc  CFLAGS=-Wall -Wextra -std=c11
  PREFIX=/opt/myapp  DESTDIR=

$ make dist         # crea el tarball
tar czf calculator-1.0.0.tar.gz main.c parser.c eval.c utils.c *.h Makefile README.md LICENSE
```

---

## Ejercicios

### Ejercicio 1 — Demostrar el problema sin .PHONY

```makefile
# Crear un Makefile con un target "clean" SIN declararlo como .PHONY.
# 1. Ejecutar make clean — debe funcionar normalmente.
# 2. Crear un archivo llamado "clean" con touch.
# 3. Ejecutar make clean de nuevo — observar que no ejecuta.
# 4. Agregar .PHONY: clean y repetir — verificar que ahora si ejecuta.
# 5. Repetir el experimento con un target "all" que dependa de un
#    target real. Observar la diferencia con y sin .PHONY.
```

### Ejercicio 2 — Makefile con targets convencionales

```makefile
# Crear un Makefile para un proyecto con 3 archivos fuente
# (main.c, mathops.c, stringops.c) que implemente TODOS
# los targets convencionales:
# - all (por defecto), clean, install, uninstall, test, dist, help
# - Usar variables para CC, CFLAGS, PREFIX, DESTDIR, VERSION
# - El target help debe auto-documentarse con grep de comentarios ##
# - distclean debe limpiar mas que clean
# - install debe usar el comando install(1), no cp
# - Probar: make, make clean, make help, make install DESTDIR=/tmp/staging
```

### Ejercicio 3 — Phony como prerequisite

```makefile
# Crear un Makefile donde:
# 1. Un target "timestamp.h" se genera siempre con la fecha actual:
#      #define BUILD_TIME "2026-03-18 14:30:00"
#    Usar un phony prerequisite "force" para que siempre se regenere.
# 2. Un programa main.c incluye timestamp.h y lo imprime.
# 3. Compilar dos veces seguidas con make — verificar que el timestamp
#    cambia aunque main.c no se haya modificado.
# 4. Reflexionar: si se declara timestamp.h como .PHONY directamente,
#    que pasa diferente? (Pista: Make cree que nunca existe)
```
