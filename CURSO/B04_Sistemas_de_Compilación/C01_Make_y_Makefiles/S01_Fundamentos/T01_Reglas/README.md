# T01 — Reglas

## Que es Make

Make es una herramienta de automatizacion de compilacion. Lee un
archivo llamado `Makefile` (o `makefile`) y ejecuta comandos para
construir programas. Su ventaja principal: **solo recompila lo
que cambio**, comparando las fechas de modificacion de archivos.

```bash
# Make busca Makefile en el directorio actual:
make

# Si el archivo se llama distinto:
make -f build.mk
make --makefile=otro_nombre
```

```makefile
# Un Makefile es una lista de reglas.
# Cada regla dice:
#   - que archivo construir (target)
#   - de que archivos depende (prerequisites)
#   - como construirlo (recipe)
```

## Anatomia de una regla

Una regla tiene tres partes: target, prerequisites y recipe:

```makefile
# Formato:
# target: prerequisites
# [TAB]recipe

program: main.c utils.c
	gcc -o program main.c utils.c

# target       → "program" — el archivo que se va a generar
# prerequisites → "main.c utils.c" — archivos de los que depende
# recipe       → "gcc -o program main.c utils.c" — comando a ejecutar
```

```makefile
# Anatomia completa:
#
#  target: prereq1 prereq2 prereq3
#  ↑       ↑
#  |       └── prerequisites (separados por espacios)
#  └── target (nombre del archivo a generar)
#
#  [TAB]command1           ← recipe (una o mas lineas)
#  [TAB]command2           ← cada linea DEBE empezar con TAB
#
# Los dos puntos (:) separan target de prerequisites.
# El salto de linea separa prerequisites de recipe.
# Cada linea de recipe DEBE empezar con un tabulador literal.
```

## El tab es obligatorio

Make usa un tabulador literal (el caracter ASCII 9) para
identificar las lineas de recipe. Espacios **no** funcionan:

```makefile
# CORRECTO — tab antes del comando:
program: main.c
	gcc -o program main.c

# INCORRECTO — espacios antes del comando:
program: main.c
    gcc -o program main.c
# Error: *** missing separator.  Stop.
```

```bash
# El error tipico cuando usas espacios en vez de tab:
# Makefile:2: *** missing separator.  Stop.

# Para ver si hay tabs o espacios:
cat -A Makefile
# Las lineas con tab muestran ^I al inicio:
#   program: main.c$
#   ^Igcc -o program main.c$
# Si ves espacios en vez de ^I, ese es el problema.
```

```bash
# Configurar editores para que inserten tabs en Makefiles:

# Vim — en .vimrc:
# autocmd FileType make setlocal noexpandtab

# VS Code — en settings.json:
# "[makefile]": {
#     "editor.insertSpaces": false,
#     "editor.tabSize": 4
# }

# Nano:
# nano -T 4 Makefile
# (nano usa tabs por defecto, no suele dar problemas)

# Si tu editor ya convirtio tabs a espacios:
# Reemplazar 4 espacios por tabs al inicio de lineas de recipe:
sed -i 's/^    /\t/' Makefile
```

## Regla simple

El caso mas basico: compilar un programa a partir de un archivo fuente:

```c
// main.c
#include <stdio.h>

int main(void) {
    printf("Hello from Make\n");
    return 0;
}
```

```makefile
# Makefile
program: main.c
	gcc -Wall -Wextra -o program main.c
```

```bash
# Ejecutar:
make
# gcc -Wall -Wextra -o program main.c

./program
# Hello from Make

# Si ejecutas make otra vez SIN modificar main.c:
make
# make: 'program' is up to date.
# Make ve que program es mas nuevo que main.c → no recompila.

# Si modificas main.c y ejecutas make de nuevo:
touch main.c    # simula una modificacion
make
# gcc -Wall -Wextra -o program main.c
# Ahora si recompila porque main.c es mas nuevo que program.
```

## Multiples prerequisites

Cuando un programa depende de varios archivos fuente y headers:

```c
// utils.h
#ifndef UTILS_H
#define UTILS_H
void greet(const char *name);
#endif
```

```c
// utils.c
#include <stdio.h>
#include "utils.h"

void greet(const char *name) {
    printf("Hello, %s!\n", name);
}
```

```c
// main.c
#include "utils.h"

int main(void) {
    greet("Make");
    return 0;
}
```

```makefile
# El programa depende de ambos .c y del header:
program: main.c utils.c utils.h
	gcc -Wall -Wextra -o program main.c utils.c

# Si cambias CUALQUIERA de los tres archivos, Make recompila.
# Si cambias utils.h → recompila (correcto, el header afecta ambos .c).
# Si cambias utils.c → recompila.
# Si cambias main.c → recompila.
#
# Problema: recompila TODO aunque solo cambio un archivo.
# La solucion es compilacion separada (siguiente seccion).
```

## Multiples reglas — compilacion separada

En proyectos reales se compila cada `.c` a un `.o` (objeto)
y luego se enlazan todos los `.o` en el ejecutable final.
Esto permite recompilar solo los archivos que cambiaron:

```makefile
# Compilacion separada: .c → .o → ejecutable

program: main.o utils.o
	gcc -o program main.o utils.o

main.o: main.c utils.h
	gcc -Wall -Wextra -c main.c

utils.o: utils.c utils.h
	gcc -Wall -Wextra -c utils.c

# -c compila sin enlazar → genera .o
# La ultima linea enlaza los .o en el ejecutable.
```

```makefile
# Como Make resuelve la cadena de dependencias:
#
# Ejecutas: make
# Make quiere construir "program" (primera regla).
#
# 1. Necesita main.o → busca la regla de main.o
#    a. main.o depende de main.c y utils.h
#    b. Si main.o no existe, o main.c/utils.h son mas nuevos → compila
#    c. gcc -Wall -Wextra -c main.c
#
# 2. Necesita utils.o → busca la regla de utils.o
#    a. utils.o depende de utils.c y utils.h
#    b. Si utils.o no existe, o utils.c/utils.h son mas nuevos → compila
#    c. gcc -Wall -Wextra -c utils.c
#
# 3. Ahora que tiene main.o y utils.o → enlaza
#    gcc -o program main.o utils.o
#
# Si ahora cambias SOLO utils.c:
# - main.o esta al dia → no se recompila
# - utils.o esta desactualizado → se recompila
# - program depende de utils.o que cambio → se re-enlaza
#
# Resultado: solo se ejecutan 2 comandos en vez de 3.
# En proyectos grandes (cientos de archivos) esto ahorra minutos.
```

## Como Make decide que recompilar

Make compara los **timestamps** (tiempo de modificacion, mtime)
del target contra los de sus prerequisites:

```makefile
# Algoritmo de decision de Make:
#
# Para cada target:
# 1. Si el target NO EXISTE → ejecutar recipe
# 2. Si ALGUN prerequisite es MAS NUEVO que el target → ejecutar recipe
# 3. Si el target es mas nuevo que TODOS sus prerequisites → no hacer nada
#
# "Mas nuevo" = tiene un mtime mayor (fue modificado mas recientemente).
```

```bash
# Ver los timestamps:
ls -l main.c main.o program
# -rw-r--r-- 1 user user  120 Mar 18 10:00 main.c
# -rw-r--r-- 1 user user 1520 Mar 18 10:01 main.o
# -rwxr-xr-x 1 user user 8400 Mar 18 10:01 program

# main.o (10:01) es mas nuevo que main.c (10:00) → no recompilar main.o
# program (10:01) es mas nuevo que main.o (10:01) → no re-enlazar

# Si editamos main.c:
touch main.c
ls -l main.c main.o
# main.c ahora tiene 10:05, main.o sigue en 10:01
# main.c (10:05) es mas nuevo que main.o (10:01) → recompilar

# Con stat para ver timestamps exactos:
stat --format="%n: %Y" main.c main.o
```

```makefile
# Consecuencias del sistema de timestamps:
#
# 1. Si copias un .c desde otro lugar, su mtime puede ser viejo.
#    Make podria NO recompilar aunque el contenido cambio.
#    Solucion: touch archivo.c, o make -B para forzar todo.
#
# 2. Si mueves el reloj del sistema hacia atras, Make se confunde.
#
# 3. Si un prerequisite se genera por otra herramienta y tarda,
#    la secuencia de timestamps importa.
#
# 4. Make NO compara contenido, solo timestamps.
#    Si haces touch main.c sin cambiar nada, Make recompila igual.
```

## Primera regla = default target

Make ejecuta la **primera regla** del Makefile por defecto
(cuando ejecutas `make` sin argumentos). Por convencion,
la primera regla se llama `all`:

```makefile
# Convencion: all como primera regla
all: program

program: main.o utils.o
	gcc -o program main.o utils.o

main.o: main.c utils.h
	gcc -Wall -Wextra -c main.c

utils.o: utils.c utils.h
	gcc -Wall -Wextra -c utils.c

# "make" ejecuta "all", que depende de "program".
# El orden de las OTRAS reglas no importa.
# Solo importa cual es la PRIMERA.
```

```makefile
# all puede tener multiples targets:
all: program tests docs

program: main.o
	gcc -o program main.o

tests: test_main.o
	gcc -o tests test_main.o

docs: manual.html
	# generar documentacion

# "make" construye program, tests y docs (en ese orden de dependencias).
```

```makefile
# Error comun: poner una regla auxiliar antes de all.
# MAL:
main.o: main.c
	gcc -c main.c

all: program        # esta NO es la primera regla

program: main.o
	gcc -o program main.o

# "make" solo construye main.o, no program.
# Solucion: siempre poner all como la primera regla.
```

## Reglas sin prerequisites

Un target puede no tener prerequisites. La recipe se ejecuta
cada vez que se invoca el target (si el target no es un archivo
que ya existe):

```makefile
# clean no tiene prerequisites:
clean:
	rm -f program main.o utils.o

# "make clean" siempre ejecuta rm.
# (Asumiendo que no existe un ARCHIVO llamado "clean"
#  en el directorio — ver nota abajo.)
```

```makefile
# Problema: si existe un archivo llamado "clean":
# $ touch clean
# $ make clean
# make: 'clean' is up to date.
#
# Make piensa que "clean" es un archivo y que esta al dia
# (no tiene prerequisites, asi que nunca esta desactualizado).
#
# Solucion: declarar clean como .PHONY (se vera en otro topico).
# .PHONY: clean
# clean:
# 	rm -f program *.o
```

## Reglas sin recipe

Una regla puede tener prerequisites pero no recipe. Sirve
para **agrupar dependencias**:

```makefile
# all agrupa todo lo que se debe construir:
all: program library tests

program: main.o utils.o
	gcc -o program main.o utils.o

library: libutils.a
	# ...

tests: test_main
	# ...

# "all" no tiene recipe propia — solo dice
# "para que all este completo, construye estos tres targets".
```

```makefile
# Otro uso: agregar prerequisites a una regla existente.
# Las dependencias se acumulan:

main.o: main.c
	gcc -Wall -Wextra -c main.c

# En otra parte del Makefile:
main.o: utils.h config.h

# Resultado: main.o depende de main.c, utils.h y config.h.
# La recipe es la de la primera definicion.
# Solo UNA regla puede tener recipe; las demas solo agregan prerequisites.
```

## Multiples targets en una regla

Cuando varios targets comparten los mismos prerequisites:

```makefile
# Esto:
prog1 prog2: common.h utils.h
	# recipe

# Equivale a:
prog1: common.h utils.h
	# recipe

prog2: common.h utils.h
	# recipe

# Es decir, se duplica la regla para cada target.
# La recipe se ejecuta UNA VEZ POR CADA target que lo necesite.
```

```makefile
# Uso tipico — varios .o dependen del mismo header:
main.o utils.o parser.o: config.h

# Equivale a:
# main.o: config.h
# utils.o: config.h
# parser.o: config.h

# Se combina con las reglas que ya tienen recipe:
main.o: main.c
	gcc -Wall -Wextra -c main.c

utils.o: utils.c
	gcc -Wall -Wextra -c utils.c

parser.o: parser.c
	gcc -Wall -Wextra -c parser.c

# Las dependencias se acumulan:
# main.o depende de main.c Y config.h.
```

## Recetas multilinea

Cada linea de una recipe se ejecuta en un **shell separado**
(un nuevo proceso `/bin/sh` por linea). Esto tiene consecuencias
importantes:

```makefile
# Cada linea es un shell independiente:
example:
	cd src
	gcc -c main.c
# PROBLEMA: el cd se ejecuta en un shell, y el gcc en OTRO.
# El gcc se ejecuta en el directorio original, no en src/.
# cd no tiene efecto porque el shell que lo ejecuto ya termino.
```

```makefile
# Solucion 1: combinar con ; en una sola linea
example:
	cd src; gcc -c main.c

# Solucion 2: combinar con && (se detiene si falla el cd)
example:
	cd src && gcc -c main.c

# Solucion 3: usar \ para continuar en la siguiente linea
# (Make las junta en una sola linea para el shell)
example:
	cd src && \
	gcc -c main.c && \
	gcc -c utils.c

# Con \ el backslash DEBE ser el ultimo caracter de la linea.
# No puede haber espacios despues del \.
```

```makefile
# Las variables de shell tambien se pierden entre lineas:

# MAL:
set_and_use:
	MY_VAR="hello"
	echo $$MY_VAR
# Imprime linea vacia — MY_VAR se definio en otro shell.
# ($$VAR es la sintaxis Make para pasar $VAR al shell.)

# BIEN:
set_and_use:
	MY_VAR="hello"; echo $$MY_VAR
# Imprime "hello" — todo en el mismo shell.

# BIEN con continuacion:
set_and_use:
	MY_VAR="hello" && \
	echo $$MY_VAR
```

```makefile
# Nota sobre $$ en recipes:
# Make interpreta $ como inicio de variable de Make.
# Para pasar un $ literal al shell, se escribe $$.
#
# Ejemplos:
test:
	echo $$HOME            # variable de shell HOME
	echo $$?               # exit code del ultimo comando
	for f in *.c; do \
		echo $$f; \
	done
```

## Prefijos de recipe

Las lineas de recipe pueden empezar con prefijos especiales
(despues del tab):

```makefile
# @ — Silencia el echo del comando.
# Por defecto, Make imprime cada comando antes de ejecutarlo.

normal:
	echo "Esto se muestra dos veces"
# Salida:
# echo "Esto se muestra dos veces"     ← Make imprime el comando
# Esto se muestra dos veces            ← el shell ejecuta el comando

silenced:
	@echo "Esto se muestra una vez"
# Salida:
# Esto se muestra una vez              ← solo la salida del comando
```

```makefile
# - — Ignora errores (Make continua aunque el comando falle).
# Sin -, Make se detiene en el primer error (exit code != 0).

clean:
	-rm program          # si program no existe, rm falla pero Make continua
	-rm *.o              # idem
	@echo "Clean done"

# Sin el -, si program no existe:
# rm program
# rm: cannot remove 'program': No such file or directory
# make: *** [Makefile:2: clean] Error 1
# (Make se detiene aqui)
```

```makefile
# Se pueden combinar:
clean:
	@-rm program *.o 2>/dev/null
	@echo "Clean complete"
# @ silencia el echo, - ignora errores.
# El orden no importa: -@ tambien funciona.

# Mas util: rm -f ya ignora archivos inexistentes:
clean:
	rm -f program *.o
# -f hace que rm no falle si el archivo no existe.
# No necesitas el prefijo - en este caso.
```

```makefile
# + — Ejecuta el comando incluso con make -n (dry run).
# Esto es raro y se usa en contextos especiales como
# builds recursivos.
subdir:
	+$(MAKE) -C subdir
```

## make con argumentos

```bash
# Ejecutar la primera regla (default target):
make

# Ejecutar una regla especifica:
make clean
make program
make main.o

# Ejecutar multiples targets:
make clean program
# Ejecuta clean primero, luego program.
```

```bash
# -n (--dry-run) — Muestra los comandos sin ejecutarlos.
# Util para verificar que hara Make:
make -n
# gcc -Wall -Wextra -c main.c
# gcc -Wall -Wextra -c utils.c
# gcc -o program main.o utils.o
# (Ningun comando se ejecuto realmente.)
```

```bash
# -B (--always-make) — Fuerza la reconstruccion de todo,
# ignorando timestamps:
make -B
# Recompila todo aunque nada haya cambiado.
# Util cuando algo salio mal con los timestamps.
```

```bash
# Otros argumentos utiles:

# -j N — compilacion paralela (N procesos simultaneos):
make -j4          # 4 procesos en paralelo
make -j$(nproc)   # tantos procesos como nucleos de CPU

# -s (--silent) — no imprimir comandos (como poner @ en cada linea):
make -s

# -k (--keep-going) — continuar despues de errores:
make -k
# Si main.o falla, igual intenta compilar utils.o.

# -C dir — ejecutar make en otro directorio:
make -C src/
# Equivale a: cd src && make

# -f archivo — usar un Makefile con otro nombre:
make -f build.mk

# Combinaciones:
make -j4 -B        # forzar todo, con 4 procesos en paralelo
make -n -B         # mostrar que haria si reconstruyera todo
```

## Ejemplo completo

```c
// calc.h
#ifndef CALC_H
#define CALC_H

int add(int a, int b);
int multiply(int a, int b);

#endif // CALC_H
```

```c
// calc.c
#include "calc.h"

int add(int a, int b) {
    return a + b;
}

int multiply(int a, int b) {
    return a * b;
}
```

```c
// main.c
#include <stdio.h>
#include "calc.h"

int main(void) {
    printf("3 + 4 = %d\n", add(3, 4));
    printf("3 * 4 = %d\n", multiply(3, 4));
    return 0;
}
```

```makefile
# Makefile

all: calculator

calculator: main.o calc.o
	gcc -o calculator main.o calc.o

main.o: main.c calc.h
	gcc -Wall -Wextra -c main.c

calc.o: calc.c calc.h
	gcc -Wall -Wextra -c calc.c

clean:
	rm -f calculator *.o
```

```bash
# Sesion tipica de trabajo:

make                  # compila todo
# gcc -Wall -Wextra -c main.c
# gcc -Wall -Wextra -c calc.c
# gcc -o calculator main.o calc.o

./calculator
# 3 + 4 = 7
# 3 * 4 = 12

# Editar calc.c para agregar subtract()...

make                  # solo recompila calc.o y re-enlaza
# gcc -Wall -Wextra -c calc.c
# gcc -o calculator main.o calc.o
# (main.o no se recompilo — no cambio)

make clean            # limpia todo
# rm -f calculator *.o

make -n               # ver que haria sin ejecutar
# gcc -Wall -Wextra -c main.c
# gcc -Wall -Wextra -c calc.c
# gcc -o calculator main.o calc.o
```

---

## Ejercicios

### Ejercicio 1 — Makefile basico

```makefile
# Crear un proyecto con:
#   greeter.h  — declaracion: void greet(const char *name);
#   greeter.c  — implementacion que imprime "Hello, <name>!"
#   main.c     — llama greet con argv[1] (o "World" si no hay argumento)
#
# Escribir un Makefile con:
#   - Regla all como default target
#   - Compilacion separada (greeter.o y main.o como intermedios)
#   - Cada .o debe listar sus prerequisites correctos (incluir .h)
#   - Regla clean que elimine ejecutable y .o
#
# Verificar:
#   1. make         → compila todo
#   2. make         → dice "up to date"
#   3. touch greeter.h && make → recompila ambos .o (dependen del header)
#   4. touch greeter.c && make → recompila solo greeter.o y re-enlaza
#   5. make clean   → elimina todo
```

### Ejercicio 2 — Cadena de dependencias

```makefile
# Crear un proyecto con 4 modulos: main.c, parser.c, lexer.c, utils.c
# con sus headers parser.h, lexer.h, utils.h.
# Dependencias:
#   main.c   incluye parser.h
#   parser.c incluye parser.h, lexer.h
#   lexer.c  incluye lexer.h, utils.h
#   utils.c  incluye utils.h
#
# Escribir un Makefile con compilacion separada.
# Verificar que al tocar SOLO utils.h se recompilan:
#   utils.o y lexer.o (dependen de utils.h), pero NO parser.o ni main.o.
# Usar make -n para verificar sin ejecutar.
```

### Ejercicio 3 — Diagnostico de errores

```makefile
# Crear un Makefile con errores INTENCIONALES y corregirlos:
#
# 1. Reemplazar los tabs por espacios en las recipes
#    → ejecutar make, observar el error "missing separator"
#    → corregir con tabs reales
#
# 2. Poner clean como primera regla en vez de all
#    → ejecutar make sin argumentos, observar que borra archivos
#    → mover all al inicio
#
# 3. Omitir un header de los prerequisites de un .o:
#    → modificar el header omitido, ejecutar make
#    → observar que NO recompila (bug silencioso)
#    → agregar el header a los prerequisites, verificar con touch + make
#
# Documentar cada error y su correccion como comentarios en el Makefile.
```
