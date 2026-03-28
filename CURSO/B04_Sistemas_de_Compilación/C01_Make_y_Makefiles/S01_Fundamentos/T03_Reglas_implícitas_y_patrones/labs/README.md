# Lab -- Reglas implicitas y patrones

## Objetivo

Explorar la base de datos interna de Make, compilar sin reglas explicitas usando
las reglas implicitas built-in, dominar las variables automaticas ($@, $<, $^,
$+, $*, $(@D), $(@F)), definir pattern rules propias que reemplazan las
implicitas, aplicar static pattern rules a listas especificas de targets, y
organizar la compilacion con un directorio de build separado usando $(@D). Al
finalizar, sabras aprovechar las reglas implicitas cuando convienen, reemplazarlas
con pattern rules cuando necesitas control, y usar variables automaticas para
escribir recipes genericas y reutilizables.

## Prerequisitos

- GCC instalado
- Make instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `hello.c` | Programa simple que imprime "Hello from implicit rules!" |
| `Makefile.implicit` | Makefile sin reglas de compilacion (usa built-in) |
| `Makefile.autovars` | Makefile que imprime todas las variables automaticas |
| `Makefile.pattern` | Makefile con pattern rule del usuario |
| `Makefile.static` | Makefile con static pattern rules |
| `Makefile.builddir` | Makefile con directorio de build separado |
| `src/main.c` | Programa principal (usa utils.h, llama add y mul) |
| `src/utils.c` | Funciones auxiliares (add, mul) |
| `src/utils.h` | Header de utils |

---

## Parte 1 -- Reglas implicitas built-in

**Objetivo**: Explorar la base de datos interna de Make con `make -p`, descubrir
que reglas conoce Make sin que nadie las escriba, y compilar un programa sin
Makefile usando reglas implicitas.

### Paso 1.1 -- Examinar el archivo fuente

```bash
cat hello.c
```

Salida esperada:

```
#include <stdio.h>

int main(void) {
    printf("Hello from implicit rules!\n");
    return 0;
}
```

Es un programa minimo con un `main` que imprime un mensaje. No existe ningun
Makefile en el directorio todavia.

### Paso 1.2 -- Compilar sin Makefile

Predice: si ejecutas `make hello` sin ningun Makefile presente, que ocurrira?

- Make necesita construir el target `hello`
- No hay Makefile con reglas explicitas
- Existe un archivo `hello.c` en el directorio...

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.3 -- Verificar la compilacion implicita

Primero, asegurate de que no hay un Makefile en el directorio actual (los
Makefiles del lab tienen nombres con sufijo, como Makefile.implicit):

```bash
ls Makefile makefile GNUmakefile 2>/dev/null
```

No debe producir salida. Ahora compila:

```bash
make hello
```

Salida esperada:

```
cc     hello.c   -o hello
```

Make encontro `hello.c` y aplico su regla implicita para compilar un `.c`
directamente a ejecutable. Observa que uso `cc` (no `gcc`) porque ese es el
valor por defecto de la variable `CC`.

Verifica que funciona:

```bash
./hello
```

Salida esperada:

```
Hello from implicit rules!
```

### Paso 1.4 -- Sobreescribir variables en la linea de comandos

```bash
rm hello
make hello CC=gcc CFLAGS="-Wall -Wextra -std=c11"
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11    hello.c   -o hello
```

Ahora Make uso `gcc` en vez de `cc` y agrego los flags. Las reglas implicitas
usan las variables `CC`, `CFLAGS`, `CPPFLAGS`, etc. Al redefinirlas, cambias el
comportamiento de la regla sin escribirla.

### Paso 1.5 -- Explorar la base de datos interna

```bash
make -p -f /dev/null 2>/dev/null | grep -A 3 '%.o: %.c'
```

Salida esperada (puede variar segun la version de Make):

```
%.o: %.c
#  recipe to execute (built-in):
	$(COMPILE.c) $(OUTPUT_OPTION) $<

```

Esta es la regla que Make aplica cuando necesita un `.o` y encuentra un `.c`.

### Paso 1.6 -- Ver que contiene COMPILE.c

```bash
make -p -f /dev/null 2>/dev/null | grep '^COMPILE.c'
```

Salida esperada:

```
COMPILE.c = $(CC) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH) -c
```

La cadena completa: la regla implicita ejecuta
`cc $(CFLAGS) $(CPPFLAGS) -c -o $@ $<`. Por esto redefinir `CC` y `CFLAGS`
afecta la compilacion sin escribir reglas.

### Paso 1.7 -- Limpieza

```bash
rm -f hello
```

---

## Parte 2 -- Compilacion sin reglas explicitas

**Objetivo**: Usar un Makefile que define solo variables y una regla de enlace,
dejando que Make aplique automaticamente la regla implicita `%.o: %.c` para
compilar los archivos fuente.

### Paso 2.1 -- Examinar los archivos fuente del proyecto

```bash
cat src/main.c src/utils.c src/utils.h
```

Observa que `main.c` incluye `utils.h` y llama a `add()` y `mul()`, que estan
implementadas en `utils.c`. Es un proyecto modular de tres archivos.

### Paso 2.2 -- Examinar el Makefile sin reglas de compilacion

```bash
cat Makefile.implicit
```

Observa que:

- Define `CC` y `CFLAGS`
- La regla `program: main.o utils.o` solo tiene la linea de enlace
- No hay ninguna regla que diga como compilar `.c` a `.o`

### Paso 2.3 -- Predecir el comportamiento

Antes de ejecutar, responde mentalmente:

- El Makefile no dice como construir `main.o` ni `utils.o`. Make fallara?
- Donde buscara Make los archivos `.c`? En el directorio actual o en `src/`?
- Las variables `CC` y `CFLAGS` estan definidas. Afectaran la regla implicita?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Copiar fuentes al directorio actual

La regla implicita `%.o: %.c` busca el `.c` en el mismo directorio que el `.o`.
Como el Makefile pide `main.o` (sin path), Make buscara `main.c` en el
directorio actual:

```bash
cp src/main.c src/utils.c src/utils.h .
```

### Paso 2.5 -- Compilar con dry run primero

```bash
make -n -f Makefile.implicit
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -g -Isrc   -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -g -Isrc   -c -o utils.o utils.c
gcc  -o program main.o utils.o
```

Sin haber escrito ninguna regla de compilacion, Make sabe que para construir
`main.o` debe buscar `main.c` y aplicar `$(CC) $(CFLAGS) -c -o main.o main.c`.
Esto es la regla implicita en accion.

### Paso 2.6 -- Compilar de verdad

```bash
make -f Makefile.implicit
```

Salida esperada:

```
gcc -Wall -Wextra -std=c11 -g -Isrc   -c -o main.o main.c
gcc -Wall -Wextra -std=c11 -g -Isrc   -c -o utils.o utils.c
gcc  -o program main.o utils.o
```

Verifica:

```bash
./program
```

Salida esperada:

```
Result of add(3, 4): 7
Result of mul(5, 6): 30
```

### Paso 2.7 -- Desactivar reglas implicitas con -r

Predice: que pasara si desactivas las reglas implicitas con el flag `-r`?

- El Makefile no tiene regla para `.c -> .o`
- Sin reglas implicitas, Make no sabe como construir `main.o`...

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.8 -- Verificar el efecto de -r

```bash
make -f Makefile.implicit clean
make -r -f Makefile.implicit
```

Salida esperada:

```
make: *** No rule to make target 'main.o', needed by 'program'.  Stop.
```

El flag `-r` desactiva todas las reglas implicitas. Sin ellas, Make no sabe
como construir `main.o` porque el Makefile no define ninguna regla para
`.c -> .o`. Esto confirma que la compilacion de la Parte 2 dependia
completamente de las reglas implicitas.

### Paso 2.9 -- Limpieza

```bash
make -f Makefile.implicit clean 2>/dev/null
rm -f main.c utils.c utils.h program main.o utils.o
```

---

## Parte 3 -- Variables automaticas

**Objetivo**: Ver con echo los valores reales de todas las variables automaticas
(`$@`, `$<`, `$^`, `$+`, `$*`, `$(@D)`, `$(@F)`, `$(<D)`, `$(<F)`) tanto en
la pattern rule de compilacion como en la regla de enlace.

### Paso 3.1 -- Examinar el Makefile de variables automaticas

```bash
cat Makefile.autovars
```

Observa que:

- La regla de enlace lista `src/main.o` **dos veces** a proposito (para ver
  la diferencia entre `$^` y `$+`)
- Cada variable automatica se imprime con echo antes de la compilacion real
- Hay una pattern rule `%.o: %.c` que tambien imprime las variables

### Paso 3.2 -- Predecir los valores en la pattern rule

Cuando Make compile `src/main.o` desde `src/main.c` usando la pattern rule
`%.o: %.c`, llena mentalmente esta tabla:

| Variable | Tu prediccion |
|----------|---------------|
| `$@` | ? |
| `$<` | ? |
| `$*` | ? |
| `$(@D)` | ? |
| `$(@F)` | ? |

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.3 -- Ejecutar y verificar las variables de la pattern rule

```bash
make -f Makefile.autovars
```

Primero veras la salida de la pattern rule para `src/main.o`:

```
--- Pattern rule: %.o: %.c ---
  $@  (target):  src/main.o
  $<  (source):  src/main.c
  $*  (stem):    src/main
  $(@D) (dir):   src
  $(@F) (file):  main.o
--------------------------------
gcc  -Wall -Wextra -std=c11 -g -Isrc -c -o src/main.o src/main.c
```

Compara con tus predicciones:

- `$@` = `src/main.o` (el target completo con path)
- `$<` = `src/main.c` (el primer -- y unico -- prerequisite)
- `$*` = `src/main` (el stem: la parte que coincide con `%`, incluyendo el path)
- `$(@D)` = `src` (solo el directorio del target)
- `$(@F)` = `main.o` (solo el nombre del target)

Luego, la pattern rule para `src/utils.o` con valores analogos:

```
--- Pattern rule: %.o: %.c ---
  $@  (target):  src/utils.o
  $<  (source):  src/utils.c
  $*  (stem):    src/utils
  $(@D) (dir):   src
  $(@F) (file):  utils.o
--------------------------------
gcc  -Wall -Wextra -std=c11 -g -Isrc -c -o src/utils.o src/utils.c
```

### Paso 3.4 -- Predecir los valores en la regla de enlace

La regla de enlace es `build/app: src/main.o src/utils.o src/main.o`. Observa
que `src/main.o` aparece dos veces en los prerequisites. Llena esta tabla:

| Variable | Tu prediccion |
|----------|---------------|
| `$@` | ? |
| `$<` | ? |
| `$^` | ? |
| `$+` | ? |
| `$(@D)` | ? |
| `$(@F)` | ? |
| `$(<D)` | ? |
| `$(<F)` | ? |

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.5 -- Verificar los valores en la regla de enlace

La salida de la regla de enlace:

```
=== Automatic variables in the link rule ===
  $@   (target):            build/app
  $<   (first prereq):      src/main.o
  $^   (all prereqs, unique):src/main.o src/utils.o
  $+   (all prereqs, dupes): src/main.o src/utils.o src/main.o
  $(@D) (target dir):        build
  $(@F) (target file):       app
  $(<D) (first prereq dir):  src
  $(<F) (first prereq file): main.o
============================================
mkdir -p build
gcc  -o build/app src/main.o src/utils.o
```

Puntos clave:

- `$^` elimino el duplicado: solo lista `src/main.o src/utils.o`
- `$+` conservo el duplicado: lista `src/main.o src/utils.o src/main.o`
- En la practica, siempre se usa `$^` para enlazar (con `$+` el linker recibiria
  el mismo `.o` dos veces)
- `mkdir -p $(@D)` usa `$(@D)` = `build` para crear el directorio de salida

Verifica que el programa funciona:

```bash
./build/app
```

Salida esperada:

```
Result of add(3, 4): 7
Result of mul(5, 6): 30
```

### Paso 3.6 -- Limpieza

```bash
make -f Makefile.autovars clean
```

---

## Parte 4 -- Pattern rules del usuario

**Objetivo**: Definir una pattern rule propia `%.o: %.c` que reemplaza la regla
implicita built-in de Make, verificar con `make -n` que Make usa la del usuario,
y confirmar que `-r` no afecta a las reglas escritas en el Makefile.

### Paso 4.1 -- Examinar el Makefile con pattern rule

```bash
cat Makefile.pattern
```

Observa que define explicitamente:

```makefile
%.o: %.c
	@echo "[pattern rule] Compiling $< -> $@"
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<
```

Esta pattern rule del usuario **reemplaza** la regla implicita built-in de
Make para `.c -> .o`. El `@echo` extra permite distinguir visualmente que
se esta usando la regla del usuario, no la built-in.

### Paso 4.2 -- Predecir la diferencia

Antes de ejecutar, responde mentalmente:

- La regla del usuario agrega un echo extra. Se vera en la salida?
- Si Make tuviera tanto la regla implicita como la del usuario, cual usaria?
- Si ejecutas con `make -r`, la pattern rule del usuario seguira funcionando?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.3 -- Comparar con dry run

```bash
make -n -f Makefile.pattern
```

Salida esperada:

```
echo "[pattern rule] Compiling src/main.c -> src/main.o"
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o src/main.o src/main.c
echo "[pattern rule] Compiling src/utils.c -> src/utils.o"
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o src/utils.o src/utils.c
gcc  -o program src/main.o src/utils.o
```

Los echo confirman que Make usa **tu** regla, no la built-in. Nota que con `-n`
(dry run) el `@echo` se muestra como `echo` (sin `@`), porque `-n` imprime
todos los comandos que ejecutaria, incluyendo los silenciados.

### Paso 4.4 -- Compilar y verificar

```bash
make -f Makefile.pattern
```

Salida esperada:

```
[pattern rule] Compiling src/main.c -> src/main.o
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o src/main.o src/main.c
[pattern rule] Compiling src/utils.c -> src/utils.o
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o src/utils.o src/utils.c
gcc  -o program src/main.o src/utils.o
```

```bash
./program
```

Salida esperada:

```
Result of add(3, 4): 7
Result of mul(5, 6): 30
```

### Paso 4.5 -- Verificar que -r no afecta a reglas del usuario

```bash
make -f Makefile.pattern clean
make -r -f Makefile.pattern
```

Salida esperada:

```
[pattern rule] Compiling src/main.c -> src/main.o
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o src/main.o src/main.c
[pattern rule] Compiling src/utils.c -> src/utils.o
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o src/utils.o src/utils.c
gcc  -o program src/main.o src/utils.o
```

Con `-r` se desactivan las reglas implicitas built-in, pero las pattern rules
escritas en el Makefile **siguen funcionando**. `-r` solo afecta a las
predefinidas.

### Paso 4.6 -- Limpieza

```bash
make -f Makefile.pattern clean
```

---

## Parte 5 -- Static pattern rules

**Objetivo**: Aplicar una pattern rule solo a una lista especifica de targets
usando static pattern rules, y verificar que archivos fuera de la lista no son
afectados por la regla.

### Paso 5.1 -- Examinar el Makefile con static pattern rules

```bash
cat Makefile.static
```

Observa la diferencia de sintaxis:

- Pattern rule normal: `%.o: %.c`
- Static pattern rule: `$(SRC_OBJS): %.o: %.c`

La static pattern rule tiene un elemento extra al inicio: la lista de targets
a los que aplica. Solo `src/main.o` y `src/utils.o` (los archivos en
`$(SRC_OBJS)`) seran procesados por esta regla.

### Paso 5.2 -- Predecir el comportamiento

Antes de ejecutar, responde mentalmente:

- Si existiera un archivo `src/extra.c`, la static pattern rule compilaria
  `src/extra.o`?
- Que diferencia practica hay entre `%.o: %.c` (que aplica a todo) y
  `$(SRC_OBJS): %.o: %.c` (que aplica solo a los listados)?
- Que valor tendra `$*` (stem) para `src/main.o`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.3 -- Compilar con static pattern rules

```bash
make -f Makefile.static
```

Salida esperada:

```
[static pattern] Compiling src/main.c -> src/main.o (stem: src/main)
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o src/main.o src/main.c
[static pattern] Compiling src/utils.c -> src/utils.o (stem: src/utils)
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o src/utils.o src/utils.c
gcc  -o program src/main.o src/utils.o
```

```bash
./program
```

Salida esperada:

```
Result of add(3, 4): 7
Result of mul(5, 6): 30
```

El resultado funcional es identico a la pattern rule normal. La ventaja aparece
cuando necesitas compilar diferentes conjuntos de archivos con diferentes flags
(por ejemplo, fuentes de produccion con `-O2` y tests con `-g -DTEST`). Con una
pattern rule normal no podrias diferenciar.

### Paso 5.4 -- Probar que la regla no aplica a otros archivos

Crea un archivo temporal para verificar:

```bash
cp hello.c src/extra.c
```

Si la regla fuera una pattern rule normal, Make podria compilar `src/extra.o`
automaticamente. Pero con static pattern rules:

```bash
make -f Makefile.static src/extra.o
```

Salida esperada:

```
make: *** No rule to make target 'src/extra.o'.  Stop.
```

La static pattern rule solo aplica a los targets listados en `$(SRC_OBJS)`.
`src/extra.o` no esta en esa lista, asi que Make no sabe como construirlo.

```bash
rm src/extra.c
```

### Paso 5.5 -- Limpieza

```bash
make -f Makefile.static clean
```

---

## Parte 6 -- Build directory separado

**Objetivo**: Compilar con una pattern rule `build/%.o: src/%.c`, colocando los
archivos objeto en un directorio `build/` separado del codigo fuente, y usar
`$(@D)` para crear directorios automaticamente.

### Paso 6.1 -- Examinar el Makefile con build directory

```bash
cat Makefile.builddir
```

Observa:

- Los fuentes viven en `src/`, los objetos van a `build/`
- `$(wildcard $(SRCDIR)/*.c)` encuentra automaticamente todos los `.c` en `src/`
- La sustitucion `$(SRCS:$(SRCDIR)/%.c=$(BUILDDIR)/%.o)` transforma los paths:
  `src/main.c` se convierte en `build/main.o`
- Cada recipe empieza con `mkdir -p $(@D)` para crear el directorio de salida
- La regla `info` muestra como se expanden las variables

### Paso 6.2 -- Ver la expansion de variables

```bash
make -f Makefile.builddir info
```

Salida esperada:

```
SRCS: src/main.c src/utils.c
OBJS: build/main.o build/utils.o
TARGET: build/program
```

Confirma que `wildcard` encontro los dos archivos fuente y que la sustitucion
de paths funciona correctamente.

### Paso 6.3 -- Predecir la estructura de build

Antes de ejecutar, responde mentalmente:

- El directorio `build/` no existe todavia. Make fallara?
- Cual es el valor de `$(@D)` cuando el target es `build/main.o`?
- Cual es el valor de `$*` (stem) cuando la regla es `build/%.o: src/%.c`
  y el target es `build/main.o`?
- En que orden se ejecutaran los comandos?

Intenta predecir antes de continuar al siguiente paso.

### Paso 6.4 -- Compilar con build directory separado

```bash
make -f Makefile.builddir
```

Salida esperada:

```
[compile] src/main.c -> build/main.o  (stem: main, dir: build)
mkdir -p build
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o build/main.o src/main.c
[compile] src/utils.c -> build/utils.o  (stem: utils, dir: build)
mkdir -p build
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o build/utils.o src/utils.c
[link] build/main.o build/utils.o -> build/program
mkdir -p build
gcc  -o build/program build/main.o build/utils.o
```

Observa:

- `$*` = `main` (solo el stem, sin el path). En la pattern rule
  `build/%.o: src/%.c`, el `%` coincide con `main`, no con `build/main`
- `$(@D)` = `build` para todos los targets
- `mkdir -p build` se ejecuta antes de cada compilacion para asegurar que el
  directorio existe (es idempotente: no falla si ya existe)

### Paso 6.5 -- Verificar la estructura resultante

```bash
ls build/
```

Salida esperada:

```
main.o  program  utils.o
```

Los archivos objeto y el ejecutable estan en `build/`, mientras que los fuentes
permanecen intactos en `src/`.

```bash
./build/program
```

Salida esperada:

```
Result of add(3, 4): 7
Result of mul(5, 6): 30
```

### Paso 6.6 -- Recompilar despues de modificar un fuente

```bash
touch src/utils.c
```

Predice: que archivos se recompilaran?

- `src/utils.c` cambio. `build/utils.o` depende de `src/utils.c`...
- `src/main.c` no cambio. `build/main.o` depende de `src/main.c`...
- `build/program` depende de ambos `.o`...

Intenta predecir antes de continuar al siguiente paso.

### Paso 6.7 -- Verificar la recompilacion selectiva

```bash
make -f Makefile.builddir
```

Salida esperada:

```
[compile] src/utils.c -> build/utils.o  (stem: utils, dir: build)
mkdir -p build
gcc -Isrc -Wall -Wextra -std=c11 -g -c -o build/utils.o src/utils.c
[link] build/main.o build/utils.o -> build/program
mkdir -p build
gcc  -o build/program build/main.o build/utils.o
```

Solo `build/utils.o` se recompilo y luego se re-enlazo el programa.
`build/main.o` no se toco. La compilacion separada con build directory funciona
igual que sin el: Make compara timestamps y recompila solo lo necesario.

### Paso 6.8 -- La ventaja de clean con build directory

```bash
make -f Makefile.builddir clean
ls build/ 2>/dev/null || echo "build/ does not exist"
```

Salida esperada:

```
rm -rf build
build/ does not exist
```

Con un build directory separado, `clean` es un solo `rm -rf build/`. No hay
riesgo de borrar archivos fuente por accidente. Compara con los Makefiles
anteriores donde `clean` tenia que listar cada `.o` individualmente.

---

## Limpieza final

```bash
make -f Makefile.pattern clean 2>/dev/null
make -f Makefile.static clean 2>/dev/null
make -f Makefile.builddir clean 2>/dev/null
rm -f hello program main.o utils.o
rm -f main.c utils.c utils.h
rm -rf build/
```

Verifica que solo quedan los archivos originales:

```bash
ls
```

Salida esperada:

```
Makefile.autovars  Makefile.implicit  Makefile.static  hello.c  src
Makefile.builddir  Makefile.pattern   README.md
```

```bash
ls src/
```

Salida esperada:

```
main.c  utils.c  utils.h
```

---

## Conceptos reforzados

1. Make tiene una **base de datos interna** con decenas de reglas implicitas que
   se pueden inspeccionar con `make -p -f /dev/null`. La regla `%.o: %.c` usa
   `$(COMPILE.c)` que se expande a `$(CC) $(CFLAGS) $(CPPFLAGS) -c`.

2. Make puede compilar un archivo `.c` **sin Makefile**: `make hello` busca
   `hello.c` y aplica la regla implicita `%: %.c` para producir un ejecutable
   directamente.

3. Redefinir variables como `CC` y `CFLAGS` **afecta las reglas implicitas**
   sin modificarlas. Esto funciona porque las reglas usan esas variables
   internamente. Se pueden redefinir en el Makefile o en la linea de comandos.

4. Las **variables automaticas** solo tienen valor dentro de un recipe: `$@` es
   el target, `$<` el primer prerequisite, `$^` todos sin duplicados, `$+`
   todos con duplicados, y `$*` el stem (la parte que coincide con `%`).

5. `$(@D)` y `$(@F)` extraen el **directorio y el nombre** del target
   respectivamente. `$(<D)` y `$(<F)` hacen lo mismo con el primer prerequisite.
   `$(@D)` es esencial con `mkdir -p` para crear directorios de build.

6. Una pattern rule escrita en el Makefile (`%.o: %.c`) **reemplaza** la regla
   implicita built-in equivalente. Esto permite agregar pasos extras como
   mensajes o creacion de directorios.

7. El flag `-r` desactiva las reglas implicitas built-in pero **no afecta** las
   pattern rules definidas en el Makefile. Un Makefile con `-r` y sus propias
   pattern rules es completamente auto-contenido.

8. Las **static pattern rules** (`$(OBJS): %.o: %.c`) limitan el alcance de una
   pattern rule a una lista especifica de targets. Archivos que no estan en la
   lista no son afectados, incluso si su nombre coincide con el patron.

9. La pattern rule `$(BUILDDIR)/%.o: $(SRCDIR)/%.c` permite mantener los
   archivos fuente y los objetos en **directorios separados**. La limpieza se
   reduce a `rm -rf build/` sin riesgo de borrar fuentes.
