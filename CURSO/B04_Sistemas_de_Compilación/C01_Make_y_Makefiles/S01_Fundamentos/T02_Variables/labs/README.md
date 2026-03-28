# Lab -- Variables en Make

## Objetivo

Experimentar con los operadores de asignacion de variables en Make (`=`, `:=`,
`?=`, `+=`), comprender la diferencia entre expansion recursiva e inmediata,
usar variables convencionales para compilar un proyecto real, sobreescribir
variables desde la linea de comandos, y aplicar sustitucion de patrones. Al
finalizar, sabras elegir el operador correcto para cada situacion y entenderas
la prioridad entre las distintas fuentes de variables.

## Prerequisitos

- GCC instalado
- GNU Make instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `hello.c` | Programa simple de una sola unidad para compilacion basica |
| `main.c` | Programa principal que usa el modulo utils |
| `utils.c` | Modulo auxiliar con funciones greet() y add() |
| `utils.h` | Header del modulo utils |
| `Makefile.lazy` | Demuestra expansion recursiva con `=` |
| `Makefile.immediate` | Demuestra expansion inmediata con `:=` |
| `Makefile.compare` | Comparacion directa de `=` vs `:=` en un solo archivo |
| `Makefile.conditional` | Demuestra asignacion condicional con `?=` |
| `Makefile.append` | Demuestra append con `+=` y construccion incremental |
| `Makefile.conventional` | Variables convencionales (CC, CFLAGS) con compilacion real |
| `Makefile.override` | Demuestra override y prioridad de variables |
| `Makefile.substitution` | Demuestra sustitucion `$(VAR:.c=.o)` |

---

## Parte 1 -- Expansion recursiva vs inmediata

**Objetivo**: Comprender la diferencia fundamental entre `=` (recursively
expanded) y `:=` (simply expanded) observando como Make evalua las variables
en cada caso.

### Paso 1.1 -- Examinar Makefile.lazy

```bash
cat Makefile.lazy
```

Observa que `A` se define con `=` (expansion recursiva). El Makefile usa
`$(info)` para imprimir el valor de `A` en dos momentos: antes y despues de
redefinir `X`.

Antes de ejecutar, responde mentalmente:

- `A = $(X) world` se define cuando `X = hello`.
  Luego `X` cambia a `goodbye`. Que valor tendra `A` en cada `$(info)`?
- Con `=`, el valor se evalua al definirlo o al referenciarlo?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.2 -- Ejecutar Makefile.lazy

```bash
make -f Makefile.lazy
```

Salida esperada:

```
--- Before redefining X ---
A = hello world
--- After redefining X ---
A = goodbye world
Done
```

Sorpresa: las **dos** lineas de `$(info)` muestran valores distintos, a pesar
de que `A` se definio una sola vez. Con `=`, Make no almacena el valor
calculado, almacena la **expresion** `$(X) world`. Cada vez que se referencia
`A`, Make re-evalua la expresion con el valor actual de `X`.

Pero espera: la primera linea dice "hello world", no "goodbye world". Por que?
Porque `$(info)` en la linea 7 se ejecuta durante la **lectura** del Makefile,
cuando `X` todavia es "hello". La segunda `$(info)` en la linea 12 se ejecuta
despues de que `X` se redefinio a "goodbye".

Punto clave: `$(info)` se expande durante la fase de lectura del Makefile, en
el orden en que aparece. Con `=`, cada expansion evalua la expresion con las
variables tal como estan en **ese momento**.

### Paso 1.3 -- Examinar y ejecutar Makefile.immediate

```bash
cat Makefile.immediate
```

Ahora `B` se define con `:=` (expansion inmediata).

Antes de ejecutar, responde mentalmente:

- `B := $(X) world` se evalua en el momento de la definicion. `X` vale "hello"
  en ese punto. Que se almacena en `B`?
- Luego `X` cambia a "goodbye". Afecta eso a `B`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 -- Verificar la expansion inmediata

```bash
make -f Makefile.immediate
```

Salida esperada:

```
--- Before redefining X ---
B = hello world
--- After redefining X ---
B = hello world
Done
```

Las **dos** lineas muestran "hello world". Con `:=`, Make evalua `$(X) world`
en el instante de la definicion y almacena el resultado como texto plano. Ya no
hay referencia a `X`. Cambiar `X` despues no tiene ningun efecto sobre `B`.

### Paso 1.5 -- Comparacion directa en un solo archivo

```bash
cat Makefile.compare
```

```bash
make -f Makefile.compare
```

Salida esperada:

```
LAZY     = goodbye world
IMMEDIATE = hello world
Done
```

Este es el resultado clave del lab. Ambas variables se definieron cuando
`X = hello`, pero:

- `LAZY` (definida con `=`) muestra "goodbye" porque se re-evalua al
  referenciarla. Para cuando `$(info)` la expande, `X` ya es "goodbye".
- `IMMEDIATE` (definida con `:=`) muestra "hello" porque capturo el valor
  en el momento de la definicion.

La diferencia con Makefile.lazy es que aqui ambos `$(info)` aparecen **despues**
de `X = goodbye`, asi que `LAZY` ya ve el valor final.

---

## Parte 2 -- Asignacion condicional (?=)

**Objetivo**: Comprender como `?=` define valores por defecto que el usuario
puede sobreescribir desde la linea de comandos o desde variables de entorno.
Descubrir una trampa comun con las variables que tienen valores por defecto
en Make.

### Paso 2.1 -- Examinar Makefile.conditional

```bash
cat Makefile.conditional
```

Las tres variables (`CC`, `PREFIX`, `VERBOSE`) usan `?=`. Esto significa:
"solo asignar si la variable no esta definida."

Antes de ejecutar, responde mentalmente:

- `CC ?= gcc` deberia asignar `gcc` a `CC` si no esta definida.
  Pero Make tiene valores por defecto para ciertas variables
  (`CC = cc`, `AR = ar`, etc.). `?=` considera a `CC` como "ya definida"?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2 -- Ejecutar sin argumentos

```bash
make -f Makefile.conditional
```

Salida esperada:

```
CC      = cc
PREFIX  = /usr/local
VERBOSE = no
Done
```

Sorpresa: `CC` muestra `cc`, no `gcc`. Make tiene un valor por defecto
integrado para `CC` (`cc`). Como `CC` ya esta definida (por Make), `?= gcc`
no tiene efecto. `PREFIX` y `VERBOSE` no tienen valores por defecto en Make,
asi que `?=` les asigna correctamente.

Esta es una trampa real: `?=` no distingue entre una variable definida por
el usuario y una definida internamente por Make. Si quieres forzar un valor
para `CC`, usa `CC := gcc` o `CC = gcc`.

### Paso 2.3 -- Sobreescribir desde la linea de comandos

```bash
make -f Makefile.conditional CC=clang VERBOSE=yes
```

Salida esperada:

```
CC      = clang
PREFIX  = /usr/local
VERBOSE = yes
Done
```

Las variables de la linea de comandos tienen la **prioridad mas alta**. `CC`
vale "clang" porque la linea de comandos la definio, sobreescribiendo tanto
el default de Make como el `?=` del Makefile.

### Paso 2.4 -- Predecir el efecto de sobreescribir desde el entorno

Antes de ejecutar, responde mentalmente:

- `CC=clang` antes de `make` establece una variable de **entorno**.
  Las variables de entorno tienen menor prioridad que el Makefile.
  Pero con `?=`, el Makefile solo asigna si no esta definida.
  El valor del entorno "define" la variable para `?=`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.5 -- Sobreescribir desde el entorno

```bash
CC=clang make -f Makefile.conditional
```

Salida esperada:

```
CC      = clang
PREFIX  = /usr/local
VERBOSE = no
Done
```

`CC=clang` como variable de entorno sobreescribe el valor por defecto de Make
(`cc`). Como `CC` ya esta definida (ahora con valor "clang"), `?= gcc` no
tiene efecto. El resultado es "clang".

Comparacion con la linea de comandos:

- `make CC=clang` (linea de comandos): maxima prioridad, siempre gana
- `CC=clang make` (entorno): menor prioridad que el Makefile, pero `?=` la
  respeta porque ya esta definida

### Paso 2.6 -- Patron correcto para defaults sobreescribibles

Para variables como `PREFIX` que no tienen valor por defecto en Make, `?=` es
el patron ideal:

```bash
make -f Makefile.conditional PREFIX=/opt
```

Salida esperada:

```
CC      = cc
PREFIX  = /opt
VERBOSE = no
Done
```

La linea de comandos sobreescribe `PREFIX`. Este es el caso de uso principal
de `?=`: definir valores por defecto para rutas de instalacion, flags
opcionales, y opciones de configuracion.

---

## Parte 3 -- Append (+=)

**Objetivo**: Construir flags incrementalmente con `+=` y ver su interaccion
con compilacion condicional (`ifdef`).

### Paso 3.1 -- Examinar Makefile.append

```bash
cat Makefile.append
```

Observa como CFLAGS crece paso a paso con `+=`, y como el bloque
`ifdef DEBUG` agrega flags distintos segun el modo.

### Paso 3.2 -- Ejecutar sin DEBUG

```bash
make -f Makefile.append
```

Salida esperada:

```
[1] CFLAGS = -std=c17
[2] CFLAGS = -std=c17 -Wall -Wextra
[3] CFLAGS = -std=c17 -Wall -Wextra -Wpedantic
[4] RELEASE mode: CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -O2
Final CFLAGS: -std=c17 -Wall -Wextra -Wpedantic -O2
```

Cada `+=` agrega al valor existente, separado por un espacio. Sin `DEBUG`,
se agrega `-O2`.

### Paso 3.3 -- Predecir el efecto de DEBUG=1

Antes de ejecutar, responde mentalmente:

- Las lineas [1], [2] y [3] cambiaran?
- Que flags se agregaran en la linea [4]?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Ejecutar con DEBUG=1

```bash
make -f Makefile.append DEBUG=1
```

Salida esperada:

```
[1] CFLAGS = -std=c17
[2] CFLAGS = -std=c17 -Wall -Wextra
[3] CFLAGS = -std=c17 -Wall -Wextra -Wpedantic
[4] DEBUG mode: CFLAGS = -std=c17 -Wall -Wextra -Wpedantic -g -O0 -DDEBUG
Final CFLAGS: -std=c17 -Wall -Wextra -Wpedantic -g -O0 -DDEBUG
```

Las lineas [1]-[3] son identicas. La diferencia esta en [4]: `ifdef DEBUG`
toma la rama que agrega `-g -O0 -DDEBUG` en lugar de `-O2`. Este patron de
construccion incremental con `ifdef` es muy comun en proyectos reales.

---

## Parte 4 -- Variables convencionales y compilacion real

**Objetivo**: Usar las variables convencionales de Make (CC, CFLAGS, LDFLAGS,
LDLIBS) para compilar un proyecto de dos archivos fuente, y ver como la
sustitucion `$(SRCS:.c=.o)` genera la lista de objetos automaticamente.

### Paso 4.1 -- Examinar los archivos fuente

```bash
cat utils.h
```

```bash
cat utils.c
```

```bash
cat main.c
```

`main.c` incluye `utils.h` y llama a `greet()` y `add()` definidas en
`utils.c`.

### Paso 4.2 -- Examinar Makefile.conventional

```bash
cat Makefile.conventional
```

Observa:

- Las variables convencionales al inicio: `CC`, `CFLAGS`, `LDFLAGS`, `LDLIBS`
- `OBJS := $(SRCS:.c=.o)` deriva la lista de objetos a partir de los fuentes
- La regla de patron `%.o: %.c` usa `$<` (primer prerequisito) y `$@` (target)
  -- estas son **variables automaticas**, que se cubren en detalle en T03

### Paso 4.3 -- Compilar el proyecto

```bash
make -f Makefile.conventional
```

Salida esperada:

```
CC      = gcc
CFLAGS  = -Wall -Wextra -std=c17
LDFLAGS =
LDLIBS  =
SRCS    = main.c utils.c
OBJS    = main.o utils.o
TARGET  = myapp
gcc -Wall -Wextra -std=c17 -c main.c -o main.o
gcc -Wall -Wextra -std=c17 -c utils.c -o utils.o
gcc  main.o utils.o  -o myapp
```

Observa como Make muestra los comandos que ejecuta. El `$(info)` imprime las
variables durante la lectura del Makefile; luego Make ejecuta las recetas.

### Paso 4.4 -- Verificar el ejecutable

```bash
./myapp
```

Salida esperada:

```
Hello, Make!
3 + 4 = 7
```

### Paso 4.5 -- Compilar con un compilador diferente

Antes de ejecutar, responde mentalmente:

- Si pasas `CC=clang` en la linea de comandos, que compilador usara Make?
- El Makefile define `CC := gcc`. Gana el Makefile o la linea de comandos?

Intenta predecir antes de continuar al siguiente paso.

```bash
make -f Makefile.conventional clean
make -f Makefile.conventional CC=clang
```

Salida esperada (si clang esta instalado):

```
CC      = clang
CFLAGS  = -Wall -Wextra -std=c17
...
clang -Wall -Wextra -std=c17 -c main.c -o main.o
clang -Wall -Wextra -std=c17 -c utils.c -o utils.o
clang  main.o utils.o  -o myapp
```

La linea de comandos gana sobre `:=` en el Makefile. Si no tienes clang
instalado, el paso fallara con un error -- eso es esperado.

### Paso 4.6 -- Limpiar

```bash
make -f Makefile.conventional clean
```

Verifica:

```bash
ls *.o myapp 2>/dev/null
```

No debe producir salida (los archivos fueron eliminados).

---

## Parte 5 -- Sobreescritura desde la linea de comandos y override

**Objetivo**: Entender la jerarquia de prioridad de las variables en Make y
como la directiva `override` protege variables criticas.

### Paso 5.1 -- Examinar Makefile.override

```bash
cat Makefile.override
```

Observa:

- `CC := gcc` -- expansion inmediata (sobreescribible desde la linea de comandos)
- `CFLAGS := -O2` -- expansion inmediata
- `override CFLAGS += -Wall` -- override con append

### Paso 5.2 -- Ejecutar sin argumentos

```bash
make -f Makefile.override
```

Salida esperada:

```
CC     = gcc
CFLAGS = -O2 -Wall
gcc -O2 -Wall -o hello hello.c
Compiled with: gcc -O2 -Wall
```

`CFLAGS` es `-O2` (del `:=`) mas `-Wall` (del `override +=`).

### Paso 5.3 -- Predecir el efecto de pasar CFLAGS desde la linea de comandos

Antes de ejecutar, responde mentalmente:

- Si ejecutas `make -f Makefile.override CFLAGS="-O3"`, que valor tendra
  CFLAGS?
- La linea de comandos normalmente sobreescribe el Makefile. Pero hay un
  `override CFLAGS += -Wall`. Que efecto tiene?
- El resultado sera `-O3` solo, `-O3 -Wall`, o `-O2 -Wall`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.4 -- Ejecutar con CFLAGS desde la linea de comandos

```bash
make -f Makefile.override CFLAGS="-O3"
```

Salida esperada:

```
CC     = gcc
CFLAGS = -O3 -Wall
gcc -O3 -Wall -o hello hello.c
Compiled with: gcc -O3 -Wall
```

La linea de comandos reemplaza `CFLAGS := -O2` con `-O3`. Pero `override
CFLAGS += -Wall` agrega `-Wall` de todas formas. El `override` garantiza que
`-Wall` siempre esta presente, sin importar lo que el usuario pase.

### Paso 5.5 -- Combinar sobreescrituras

```bash
make -f Makefile.override CC=clang CFLAGS="-O0 -g"
```

Salida esperada (si clang esta instalado):

```
CC     = clang
CFLAGS = -O0 -g -Wall
clang -O0 -g -Wall -o hello hello.c
Compiled with: clang -O0 -g -Wall
```

`CC` se sobreescribe completamente (no tiene `override`). `CFLAGS` recibe el
valor de la linea de comandos mas `-Wall` del `override`.

### Paso 5.6 -- Resumen de prioridades

La jerarquia de prioridades de variables en Make, de mayor a menor:

1. `override` en el Makefile (no se puede sobreescribir)
2. Linea de comandos (`make VAR=value`)
3. Makefile (`VAR = value`, `VAR := value`)
4. Variables de entorno (`export VAR=value` en el shell)
5. Variables por defecto de Make (CC = cc, etc.)

### Limpieza de la parte 5

```bash
rm -f hello
```

---

## Parte 6 -- Sustitucion en variables

**Objetivo**: Usar la sintaxis `$(VAR:patron=reemplazo)` para derivar listas
de archivos automaticamente, y entender sus limitaciones.

### Paso 6.1 -- Examinar Makefile.substitution

```bash
cat Makefile.substitution
```

Observa las dos sustituciones:

- `$(SRCS:.c=.o)` -- cambia `.c` por `.o` en cada palabra
- `$(MIXED:.c=.o)` -- caso con un archivo que termina en `.c.bak`

### Paso 6.2 -- Predecir las sustituciones

Antes de ejecutar, responde mentalmente:

- `SRCS = main.c utils.c hello.c`. Que produce `$(SRCS:.c=.o)`?
- `MIXED = foo.c bar.c.bak baz.c`. Que produce `$(MIXED:.c=.o)`?
  Pista: la sustitucion solo aplica al **final** de cada palabra.

Intenta predecir antes de continuar al siguiente paso.

### Paso 6.3 -- Verificar las sustituciones

```bash
make -f Makefile.substitution
```

Salida esperada:

```
SRCS = main.c utils.c hello.c
OBJS = main.o utils.o hello.o
DEPS = main.d utils.d hello.d

MIXED      = foo.c bar.c.bak baz.c
MIXED_OBJS = foo.o bar.c.bak baz.o
Notice: bar.c.bak did NOT change -- .c is not at the end
Done
```

`bar.c.bak` no cambio porque `.c` no esta al final de la palabra. La
sustitucion de Make es estrictamente de **sufijo**: solo reemplaza si el patron
coincide con el final de cada palabra.

### Paso 6.4 -- Variables automaticas (adelanto)

En Makefile.conventional viste `$<` y `$@` en la regla de patron:

```makefile
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@
```

Estas son **variables automaticas**: Make las define automaticamente dentro de
cada regla.

- `$@` -- el target (el archivo que se esta generando)
- `$<` -- el primer prerequisito (el archivo fuente)
- `$^` -- todos los prerequisitos

Las variables automaticas se cubren en detalle en **T03 -- Reglas**. Por ahora,
basta saber que existen y que son distintas de las variables definidas por el
usuario.

---

## Limpieza final

```bash
rm -f hello myapp
rm -f main.o utils.o
```

Verifica que solo quedan los archivos fuente y los Makefiles:

```bash
ls
```

Salida esperada:

```
Makefile.append        Makefile.immediate     Makefile.substitution  main.c     utils.h
Makefile.compare       Makefile.lazy          README.md              utils.c
Makefile.conditional   Makefile.override      hello.c
Makefile.conventional
```

---

## Conceptos reforzados

1. Con `=` (recursively expanded), Make almacena la **expresion**, no el valor.
   Cada referencia re-evalua la expresion con las variables tal como estan en
   ese momento. Si una variable referenciada cambia despues, el resultado cambia.

2. Con `:=` (simply expanded), Make evalua la expresion **una sola vez** en el
   momento de la definicion y almacena el resultado como texto plano. Cambios
   posteriores en las variables referenciadas no tienen efecto.

3. `?=` (conditional) asigna solo si la variable no esta definida. Es el patron
   estandar para valores por defecto que el usuario puede sobreescribir desde
   la linea de comandos. Cuidado: Make tiene valores integrados para ciertas
   variables (como `CC = cc`), y `?=` las considera "ya definidas".

4. `+=` (append) agrega texto al valor existente, separado por un espacio.
   Respeta el tipo de la variable original: si se definio con `=`, sigue siendo
   recursiva; si se definio con `:=`, sigue siendo inmediata.

5. Las variables convencionales (CC, CFLAGS, CPPFLAGS, LDFLAGS, LDLIBS) tienen
   significado especifico en Make. Las reglas implicitas las usan para compilar
   y enlazar. Definirlas correctamente configura el comportamiento automatico.

6. Las variables de la linea de comandos (`make CC=clang`) tienen prioridad
   sobre las del Makefile. La directiva `override` es la unica forma de
   garantizar que un valor persista incluso frente a la linea de comandos.

7. La sustitucion `$(SRCS:.c=.o)` reemplaza el sufijo `.c` por `.o` en cada
   palabra de la variable. Solo aplica al **final** de cada palabra; si el
   patron no esta al final, la palabra no se modifica.

8. Las variables automaticas (`$@`, `$<`, `$^`) son definidas por Make dentro
   de cada regla y se cubren en detalle en T03. No son asignadas por el
   usuario.
