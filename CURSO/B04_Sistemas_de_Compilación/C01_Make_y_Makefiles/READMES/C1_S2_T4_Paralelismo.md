# T04 — Paralelismo

## Compilacion en paralelo con make -j

Por defecto Make ejecuta un comando a la vez: compila main.o,
espera a que termine, compila utils.o, espera, enlaza. En proyectos
con cientos de archivos esto desperdicia los nucleos del procesador.
El flag `-j` activa la ejecucion en paralelo:

```bash
# -j N: ejecutar hasta N jobs simultaneos.
make -j4             # hasta 4 compilaciones en paralelo
make -j$(nproc)      # tantos jobs como nucleos de CPU (recomendado)
make -j8             # 8 jobs

# -j sin numero: sin limite de jobs simultaneos.
# PELIGROSO — puede lanzar cientos de procesos y congelar la maquina.
make -j               # NO recomendado
```

```bash
# Diferencia en un proyecto con 100 archivos .c independientes:
#
# make -j1   → compila uno tras otro → ~100 segundos (1s por archivo)
# make -j4   → 4 a la vez           → ~25 segundos
# make -j8   → 8 a la vez           → ~13 segundos
#
# El speedup es dramatico en proyectos reales.
# El kernel de Linux pasa de ~2 horas (-j1) a ~15 minutos (-j$(nproc)).
```

## Como funciona: el DAG de dependencias

Make construye un DAG (grafo dirigido aciclico) con todas las reglas
y sus dependencias. Dos targets que no dependen entre si pueden
ejecutarse en paralelo. Make analiza el grafo y lanza jobs para
todo lo que esta listo simultaneamente:

```makefile
# Ejemplo:
all: program

program: main.o utils.o parser.o
	$(CC) -o $@ $^

main.o: main.c defs.h
	$(CC) $(CFLAGS) -c -o $@ $<

utils.o: utils.c defs.h
	$(CC) $(CFLAGS) -c -o $@ $<

parser.o: parser.c parser.h
	$(CC) $(CFLAGS) -c -o $@ $<

# DAG de dependencias:
#
#   main.o ─────┐
#   utils.o ────┼──→ program
#   parser.o ───┘
#
# main.o, utils.o y parser.o NO dependen entre si.
# Con -j3: los tres se compilan simultaneamente.
# Cuando los tres terminan, se enlaza program.
```

## Requisito critico: dependencias correctas

La compilacion paralela expone dependencias faltantes en el Makefile.
Sin `-j`, Make ejecuta en orden de aparicion y un bug de dependencias
puede pasar desapercibido. Con `-j`, el orden no esta garantizado
y las dependencias faltantes causan race conditions:

```makefile
# MAL — dependencia faltante:
app.o: app.c
	$(CC) $(CFLAGS) -c -o $@ $<

# app.c hace #include "generated.h", pero la dependencia no esta declarada.
# generated.h se genera por otra regla:
generated.h: schema.json
	./generate_header.py schema.json > generated.h

# Sin -j: Make ejecuta las reglas en algun orden secuencial.
# Si por casualidad generated.h se genera antes de compilar app.o, funciona.
#
# Con -j: Make puede intentar compilar app.o ANTES de que generated.h exista.
# Error: fatal error: generated.h: No such file or directory
# Este es un bug del Makefile, no de Make.
```

```makefile
# BIEN — dependencia declarada:
app.o: app.c generated.h
	$(CC) $(CFLAGS) -c -o $@ $<

generated.h: schema.json
	./generate_header.py schema.json > generated.h

# Ahora Make sabe que app.o necesita generated.h.
# Con -j, esperara a que generated.h exista antes de compilar app.o.
```

```bash
# Regla practica: siempre probar con -j para detectar dependencias faltantes.
# Si funciona con -j1 pero falla con -j8, el Makefile tiene un bug.
make -j$(nproc)
```

## Salida intercalada y --output-sync

Con `-j`, la salida de multiples compilaciones se mezcla en la terminal.
Los mensajes de error de un archivo aparecen entrelazados con los de
otro, haciendo dificil leer:

```bash
# Sin -O, la salida se intercala:
make -j4
# gcc -c main.c
# gcc -c utils.c
# main.c:10: error: undeclared variable 'x'
# utils.c:5: warning: unused variable
# main.c:12: error: expected ';'
# (No queda claro que errores son de main.c y cuales de utils.c)
```

```bash
# -O (--output-sync) agrupa la salida por target:
make -j4 -O
# Equivale a:
make -j4 --output-sync=target

# Ahora la salida se agrupa:
# --- main.o ---
# gcc -c main.c
# main.c:10: error: undeclared variable 'x'
# main.c:12: error: expected ';'
# --- utils.o ---
# gcc -c utils.c
# utils.c:5: warning: unused variable
```

```bash
# Modos de --output-sync:
make -j4 --output-sync=none      # sin sincronizacion (default sin -O)
make -j4 --output-sync=line      # sincronizar linea a linea
make -j4 --output-sync=target    # agrupar por target (mas util, default con -O)
make -j4 --output-sync=recurse   # agrupar por invocacion recursiva de make
```

## Order-only prerequisites

Los prerequisites normales provocan recompilacion cuando cambian
(su timestamp es mas nuevo que el target). Los order-only prerequisites
solo garantizan que el prerequisite **exista** antes de ejecutar la
recipe, pero no provocan recompilacion si cambian:

```makefile
# Sintaxis:
# target: normal_prereqs | order_only_prereqs
#                        ↑
#                   pipe separa los dos tipos

build/app.o: src/app.c src/app.h | build/
	$(CC) $(CFLAGS) -c -o $@ $<

# A la izquierda del | → prerequisites normales.
#   Si src/app.c o src/app.h cambian → recompila.
#
# A la derecha del | → order-only prerequisites.
#   build/ debe EXISTIR antes de compilar.
#   Si el timestamp de build/ cambia → NO recompila.
```

```makefile
# Sin order-only — por que el timestamp del directorio importa:
#
# Cada vez que se crea un archivo DENTRO de build/, el timestamp
# del directorio build/ se actualiza. Esto significa que si build/
# fuera un prerequisite normal, TODOS los .o se recompilarian cada
# vez que cualquier .o se genera (porque build/ cambia).
#
# build/main.o: src/main.c build/    ← MAL (prerequisite normal)
#   1. Compila build/main.o → timestamp de build/ cambia.
#   2. Compila build/utils.o → timestamp de build/ cambia de nuevo.
#   3. Make ve que build/ es mas nuevo que build/main.o → recompila main.o.
#   4. Bucle infinito de recompilaciones.
#
# build/main.o: src/main.c | build/  ← BIEN (order-only)
#   build/ solo necesita existir. Sus cambios de timestamp se ignoran.
```

## Crear directorios como order-only prerequisites

El caso de uso principal de order-only prerequisites es la creacion
de directorios de salida:

```makefile
# --- Configuracion ---
SRCDIR   = src
BUILDDIR = build
SRCS     = $(wildcard $(SRCDIR)/*.c)
OBJS     = $(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)
TARGET   = $(BUILDDIR)/program

# --- Reglas ---
all: $(TARGET)

$(TARGET): $(OBJS) | $(BUILDDIR)/
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)/
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# Regla para crear el directorio:
$(BUILDDIR)/:
	mkdir -p $@

.PHONY: all clean
clean:
	rm -rf $(BUILDDIR)
```

```makefile
# Con subdirectorios (build/net/, build/io/, etc.):

OBJS = build/main.o build/net/socket.o build/io/reader.o

# Cada .o necesita que su directorio exista:
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $$(dir $$@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

# La regla para directorios — el % coincide con cualquier path:
%/:
	mkdir -p $@

# $$(dir $$@) usa secondary expansion (requiere .SECONDEXPANSION:).
# dir extrae el directorio del target: dir(build/net/socket.o) = build/net/

.SECONDEXPANSION:
```

## Race condition con mkdir

Sin order-only prerequisites, la creacion de directorios en paralelo
es problematica:

```makefile
# MAL — cada .o crea el directorio en su recipe:
build/%.o: src/%.c
	mkdir -p build/
	$(CC) $(CFLAGS) -c -o $@ $<

# Con -j4: cuatro jobs ejecutan mkdir build/ simultaneamente.
# mkdir -p no falla si el directorio ya existe, asi que FUNCIONA,
# pero es ineficiente — se ejecuta mkdir una vez por cada .o.
```

```makefile
# BIEN — order-only prerequisite:
build/%.o: src/%.c | build/
	$(CC) $(CFLAGS) -c -o $@ $<

build/:
	mkdir -p $@

# Make sabe que build/ es prerequisite de todos los .o.
# Crea el directorio UNA SOLA VEZ antes de lanzar las compilaciones.
# Los jobs de compilacion corren en paralelo DESPUES de que build/ exista.
```

## .NOTPARALLEL

El target especial `.NOTPARALLEL` fuerza ejecucion secuencial, incluso
si se invoca con `-j`:

```makefile
# Forzar ejecucion secuencial de todo el Makefile:
.NOTPARALLEL:

all: step1 step2 step3

step1:
	@echo "Paso 1"

step2:
	@echo "Paso 2"

step3:
	@echo "Paso 3"

# Aunque ejecutes make -j8, los targets se ejecutan uno a uno.
# Util cuando el paralelismo no es seguro y no puedes
# corregir las dependencias facilmente.
```

```makefile
# GNU Make 4.4+ permite .NOTPARALLEL selectivo:
.NOTPARALLEL: deploy setup

# Solo deploy y setup se ejecutan en modo secuencial.
# El resto del Makefile puede ejecutarse en paralelo.

deploy: build upload notify
	@echo "Deploy complete"

# build, upload y notify se ejecutan secuencialmente
# porque deploy esta en .NOTPARALLEL.
```

```makefile
# Alternativa: .WAIT (GNU Make 4.4+)
# Inserta un punto de sincronizacion entre prerequisites:

all: compile .WAIT link .WAIT install

# compile se ejecuta primero (posiblemente en paralelo consigo mismo).
# .WAIT espera a que compile termine.
# Luego link se ejecuta.
# .WAIT espera a que link termine.
# Luego install se ejecuta.
```

## Job server

Cuando un Makefile invoca sub-makes recursivamente (con `$(MAKE) -C subdir`),
Make coordina el numero total de jobs a traves de un mecanismo llamado
**job server**. Sin el, cada sub-make lanzaria sus propios `-j N` jobs
y el total se multiplicaria:

```makefile
# Proyecto con sub-makefiles:
all:
	$(MAKE) -C lib/
	$(MAKE) -C src/
	$(MAKE) -C tests/

# Si ejecutas: make -j8
# El make padre crea un job server (un pipe) con 7 tokens (8 - 1).
# Cada sub-make pide tokens del pipe antes de lanzar un job.
# Total de jobs en todo momento: maximo 8 (controlado por el pipe).
```

```makefile
# $(MAKE) vs make:
all:
	$(MAKE) -C lib/       # BIEN — hereda el job server
	make -C lib/          # MAL  — no hereda el job server

# $(MAKE) pasa automaticamente los flags del padre (incluyendo -j)
# y participa en el job server.
# Un make literal NO hereda nada — lanzaria su propia instancia
# sin coordinacion, potencialmente saturando la maquina.
```

```makefile
# El prefijo + fuerza ejecucion (como $(MAKE)) y participa en el jobserver:
generate:
	+./run_codegen.sh
# El + hace que Make trate esta linea como un sub-make:
# se ejecuta incluso con -n (dry run) y participa en el job server.
# Sin +, el script no participa en el job server y podria
# lanzar procesos sin coordinacion.
```

```bash
# Diagnosticar el job server — Make pasa el file descriptor en MAKEFLAGS:
make -j8
# MAKEFLAGS contiene algo como: --jobserver-auth=3,4
# 3 y 4 son los file descriptors del pipe del job server.
# Los sub-makes leen tokens de fd 3 y los devuelven a fd 4.
```

## Limitar por load average con -l

El flag `-l` (o `--load-average`) limita la creacion de nuevos jobs
cuando el load average del sistema supera un umbral. Esto evita
saturar la maquina cuando hay otros procesos corriendo:

```bash
# Permitir hasta 8 jobs, pero no lanzar nuevos si el load > 4.0:
make -j8 -l4

# Esto es util en servidores compartidos o maquinas de CI:
make -j$(nproc) -l$(nproc)
# Hasta nproc jobs, pero se frena si la carga ya es alta.
```

```bash
# Diferencia entre -j y -l:
#
# -j8:       siempre intenta tener 8 jobs corriendo.
# -j8 -l4:   intenta tener 8 jobs, pero si el load average
#            ya es >= 4.0, espera antes de lanzar uno nuevo.
#
# El load average refleja cuantos procesos compiten por CPU.
# Un load de 4.0 en una maquina de 4 cores significa que esta al 100%.
```

## Medir el speedup

```bash
# Compilar con 1 job (baseline):
make clean
time make -j1
# real    1m42s

# Compilar con todos los cores:
make clean
time make -j$(nproc)
# real    0m18s

# Speedup = tiempo_secuencial / tiempo_paralelo = 102s / 18s = 5.7x
```

```bash
# Ley de Amdahl aplicada a compilacion:
#
# Speedup maximo = 1 / (S + P/N)
#
# S = fraccion secuencial (linking, generacion de headers)
# P = fraccion paralelizable (compilacion de .o)
# N = numero de cores
#
# Si el 95% del tiempo es compilacion de .o (paralelizable)
# y el 5% es linking (secuencial), con 8 cores:
#
# Speedup = 1 / (0.05 + 0.95/8) = 1 / (0.05 + 0.119) = 5.9x
#
# No 8x, porque el linking es secuencial y limita el speedup.
# Mas cores dan rendimiento decreciente.
```

```bash
# Herramienta para visualizar la compilacion paralela:
# https://github.com/jgehring/compiledb puede generar un compile_commands.json
# que luego se puede analizar.

# Forma simple de ver cuantos jobs corren realmente:
watch -n 0.5 'ps aux | grep "[g]cc" | wc -l'
# Mientras make -j8 corre, muestra cuantos gcc hay activos.
```

## Buenas practicas

```makefile
# 1. Siempre probar con -j para detectar dependencias faltantes.
#    Si falla con -j pero funciona sin -j, el Makefile tiene un bug.

# 2. Usar order-only para directorios:
$(BUILDDIR)/%.o: $(SRCDIR)/%.c | $(BUILDDIR)/
	$(CC) $(CFLAGS) -c -o $@ $<

$(BUILDDIR)/:
	mkdir -p $@

# 3. make -j$(nproc) como default razonable.
#    Se puede poner en el shell profile:
#    export MAKEFLAGS="-j$(nproc)"

# 4. Usar -O (output-sync) con -j para salida legible:
#    make -j$(nproc) -O

# 5. Usar $(MAKE) en lugar de make para sub-makes:
subdir:
	$(MAKE) -C subdir/    # hereda job server
#	make -C subdir/       # MAL — no hereda job server

# 6. Declarar todas las dependencias — no confiar en el orden
#    secuencial. Usar -MMD -MP para auto-generar dependencias de headers.

# 7. Reservar .NOTPARALLEL para casos donde no se pueden
#    corregir las dependencias (Makefiles legacy, scripts externos).

# 8. Usar .DELETE_ON_ERROR para proteger builds paralelos:
.DELETE_ON_ERROR:
#    Si una recipe falla (gcc muere, OOM killer, Ctrl+C), Make borra
#    el target parcialmente creado. Sin esto, un .o corrupto queda en
#    disco y Make lo considera "up to date" en la siguiente build.
#    Es especialmente importante con -j porque la probabilidad de
#    fallos parciales aumenta con la carga del sistema.
```

```bash
# Detectar dependencias ocultas con --shuffle (GNU Make 4.4+):
make --shuffle -j$(nproc)
# Randomiza el orden de ejecucion de targets independientes.
# Si un build funciona con -j pero depende del orden "por suerte",
# --shuffle lo expone. Ejecutar varias veces para mayor cobertura.
# Si falla con --shuffle, hay una dependencia faltante.
```

```bash
# Configuracion recomendada en ~/.bashrc o ~/.zshrc:
export MAKEFLAGS="-j$(nproc) -O"

# Esto hace que toda invocacion de make use todos los cores
# con output sync. Se puede sobreescribir:
make -j1          # fuerza secuencial para depurar
make -j4          # sobreescribe el default
```

---

## Ejercicios

### Ejercicio 1 --- Detectar dependencias faltantes

```makefile
# Crear un proyecto con:
#   config.h    — define VERSION "1.0" (o cualquier macro)
#   generator.c — lee un archivo y genera generated.h con un timestamp
#   app.c       — incluye config.h y generated.h
#
# Escribir un Makefile con una regla para generated.h (generado por generator)
# y una regla para app.o que OMITA intencionalmente generated.h de sus prerequisites.
#
# 1. Ejecutar make -j1 varias veces. Probablemente funcione.
# 2. Ejecutar make clean && make -j$(nproc). Puede fallar porque
#    app.o se compila antes de que generated.h exista.
# 3. Corregir el Makefile agregando generated.h como prerequisite de app.o.
# 4. Verificar que make clean && make -j$(nproc) funciona de forma consistente.
```

### Ejercicio 2 --- Order-only para directorios de salida

```makefile
# Crear un proyecto con estructura:
#   src/main.c
#   src/utils.c
#   src/net/client.c
#
# Escribir un Makefile que compile los .o en build/ (con subdirectorios):
#   build/main.o
#   build/utils.o
#   build/net/client.o
#
# 1. Primero, usar mkdir -p $(@D) dentro de la recipe de cada .o.
#    Ejecutar make -j$(nproc). Observar que mkdir se ejecuta para cada archivo.
#
# 2. Refactorizar: usar order-only prerequisites con .SECONDEXPANSION
#    para que los directorios se creen una sola vez.
#    Verificar con make -j$(nproc) que los directorios se crean antes
#    de que inicien las compilaciones.
#
# 3. Ejecutar make dos veces. Verificar que la segunda vez NO recompila
#    nada (el timestamp de build/ no provoca recompilacion).
```

### Ejercicio 3 --- Medir speedup real

```bash
# 1. Crear un proyecto con al menos 20 archivos .c (pueden ser triviales:
#    cada uno define una funcion que retorna un int).
#    Usar un script bash o un Makefile auxiliar para generarlos.
#
# 2. Escribir un Makefile con compilacion separada y dependencias correctas.
#
# 3. Medir tiempos:
#    make clean && time make -j1
#    make clean && time make -j2
#    make clean && time make -j4
#    make clean && time make -j$(nproc)
#    make clean && time make -j       # sin limite (observar con cuidado)
#
# 4. Crear una tabla con los resultados:
#    Jobs | Tiempo | Speedup
#    1    | X.XXs  | 1.0x
#    2    | X.XXs  | ...
#    4    | X.XXs  | ...
#    N    | X.XXs  | ...
#
# 5. Comparar con la prediccion de la ley de Amdahl.
#    Estimar la fraccion secuencial a partir de los datos.
```
