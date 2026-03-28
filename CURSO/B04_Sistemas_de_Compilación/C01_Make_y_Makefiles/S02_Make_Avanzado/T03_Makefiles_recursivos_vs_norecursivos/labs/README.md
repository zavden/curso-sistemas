# Lab -- Makefiles recursivos vs no-recursivos

## Objetivo

Construir un proyecto con dos componentes (lib/ y app/) usando recursive make,
demostrar que las dependencias cruzadas entre subdirectorios no se detectan,
convertir el proyecto a non-recursive make con dependencias automaticas
(-MMD/-MP), y finalmente organizar el codigo con el patron module.mk. Al
finalizar, entenderas por que recursive make produce builds incrementales poco
fiables y como el enfoque non-recursive resuelve ese problema.

## Prerequisitos

- GCC y GNU Make instalados
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `calc.h` | Header de la biblioteca: declaraciones de add() y sub() |
| `calc.c` | Implementacion de la biblioteca |
| `main.c` | Programa que incluye calc.h y llama a add() y sub() |
| `Makefile.recursive.root` | Makefile raiz para la version recursiva |
| `Makefile.recursive.lib` | Makefile de lib/ para la version recursiva |
| `Makefile.recursive.app` | Makefile de app/ para la version recursiva |
| `Makefile.nonrecursive` | Makefile unico para la version non-recursive |
| `Makefile.modulemk` | Makefile raiz para la version con module.mk |
| `module.mk.lib` | Fragmento module.mk para lib/ |
| `module.mk.app` | Fragmento module.mk para app/ |

---

## Parte 1 -- Recursive make: estructura y compilacion

**Objetivo**: Crear un proyecto multi-directorio con recursive make. Cada
subdirectorio tiene su propio Makefile y el Makefile raiz los coordina con
`$(MAKE) -C`.

### Paso 1.1 -- Crear la estructura del proyecto recursivo

```bash
mkdir -p proyecto-rec/lib proyecto-rec/app
```

Copia los archivos fuente y Makefiles a su lugar:

```bash
cp calc.h calc.c proyecto-rec/lib/
cp main.c proyecto-rec/app/
cp Makefile.recursive.root proyecto-rec/Makefile
cp Makefile.recursive.lib proyecto-rec/lib/Makefile
cp Makefile.recursive.app proyecto-rec/app/Makefile
```

Verifica la estructura:

```bash
find proyecto-rec -type f | sort
```

Salida esperada:

```
proyecto-rec/Makefile
proyecto-rec/app/Makefile
proyecto-rec/app/main.c
proyecto-rec/lib/Makefile
proyecto-rec/lib/calc.c
proyecto-rec/lib/calc.h
```

### Paso 1.2 -- Examinar el Makefile raiz

```bash
cat proyecto-rec/Makefile
```

Observa los puntos clave:

- `SUBDIRS := lib app` define los subdirectorios
- `$(MAKE) -C $@` invoca make dentro de cada subdirectorio
- `app: lib` establece que lib debe compilarse antes que app
- Los targets `$(SUBDIRS)` son `.PHONY`: Make siempre entra a cada subdirectorio

### Paso 1.3 -- Examinar los Makefiles de subdirectorios

```bash
cat proyecto-rec/lib/Makefile
```

El Makefile de lib/ es un Makefile completo e independiente: define su propio
CC, CFLAGS, compila calc.c a calc.o, y genera libcalc.a.

```bash
cat proyecto-rec/app/Makefile
```

El Makefile de app/ tambien es independiente. Observa:

- `-I../lib` para encontrar calc.h al compilar
- `-L../lib` y `-lcalc` para enlazar contra libcalc.a
- CC y CFLAGS se definen de nuevo (duplicacion con lib/)

### Paso 1.4 -- Predecir la secuencia de compilacion

Antes de compilar, responde mentalmente:

- El Makefile raiz tiene `app: lib`. Que subdirectorio se compilara primero?
- Dentro de lib/, que archivos se generaran?
- Dentro de app/, que flags necesitara gcc para encontrar calc.h y libcalc.a?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.5 -- Compilar el proyecto

```bash
cd proyecto-rec && make
```

Salida esperada:

```
make -C lib
make[1]: Entering directory '.../proyecto-rec/lib'
gcc -Wall -Wextra -std=c17 -c calc.c -o calc.o
ar rcs libcalc.a calc.o
make[1]: Leaving directory '.../proyecto-rec/lib'
make -C app
make[1]: Entering directory '.../proyecto-rec/app'
gcc -Wall -Wextra -std=c17 -I../lib -c main.c -o main.o
gcc -L../lib -o calculator main.o -lcalc
make[1]: Leaving directory '.../proyecto-rec/app'
```

Observa:

- `make[1]` indica que es un sub-make (nivel 1 de recursion)
- Primero compila lib/ (por la dependencia `app: lib`), luego app/
- Cada sub-make entra y sale de su directorio

### Paso 1.6 -- Ejecutar el programa

```bash
./app/calculator
```

Salida esperada:

```
add(3, 5) = 8
sub(10, 4) = 6
```

### Paso 1.7 -- Verificar que el build incremental funciona dentro de un subdirectorio

```bash
make
```

Salida esperada:

```
make -C lib
make[1]: Entering directory '.../proyecto-rec/lib'
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '.../proyecto-rec/lib'
make -C app
make[1]: Entering directory '.../proyecto-rec/app'
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '.../proyecto-rec/app'
```

Nada se recompila porque ningun archivo cambio. Observa que Make igual **entra**
a cada subdirectorio -- solo para descubrir que no hay nada que hacer. Este
overhead de invocar sub-makes es una desventaja del enfoque recursivo.

```bash
cd ..
```

---

## Parte 2 -- El problema de las dependencias cruzadas

**Objetivo**: Demostrar que recursive make no detecta cambios en headers de
un subdirectorio cuando otro subdirectorio depende de ellos. Este es el
problema central documentado en "Recursive Make Considered Harmful".

### Paso 2.1 -- Predecir el resultado

Antes de continuar, responde mentalmente:

- `app/main.c` incluye `calc.h` de lib/. Si modificas `lib/calc.h`, Make
  recompilara `app/main.o`?
- El Makefile de app/ tiene la regla `%.o: %.c`. Sabe que main.o depende de
  `../lib/calc.h`?
- Que informacion le falta al sub-make de app/ para tomar la decision
  correcta?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2 -- Simular un cambio en lib/calc.h

Usa `touch` para actualizar el timestamp de calc.h sin cambiar su contenido.
Lo que importa es que Make vea un timestamp mas reciente:

```bash
cd proyecto-rec
touch lib/calc.h
```

Verifica que el timestamp cambio:

```bash
stat --format="%n: modify %y" lib/calc.h
```

### Paso 2.3 -- Recompilar y observar

```bash
make
```

Salida esperada:

```
make -C lib
make[1]: Entering directory '.../proyecto-rec/lib'
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '.../proyecto-rec/lib'
make -C app
make[1]: Entering directory '.../proyecto-rec/app'
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '.../proyecto-rec/app'
```

Nada se recompilo. Analicemos por que:

- El Makefile de lib/ tiene la regla `%.o: %.c`, que solo lista el .c como
  dependencia. No menciona calc.h, asi que Make no sabe que calc.o depende
  de calc.h. Por eso lib/ tampoco recompila.
- El Makefile de app/ no tiene **ninguna** informacion sobre archivos en
  lib/. Su regla `%.o: %.c` solo sabe que main.o depende de main.c. No
  menciona `../lib/calc.h`.
- Cada sub-make ve unicamente sus propios archivos. No tiene vision del
  proyecto completo.

### Paso 2.4 -- Confirmar el problema con touch en main.c

Para demostrar que app/ si reacciona a cambios en sus propios archivos:

```bash
touch app/main.c
make
```

Salida esperada:

```
make -C lib
make[1]: Entering directory '.../proyecto-rec/lib'
make[1]: Nothing to be done for 'all'.
make[1]: Leaving directory '.../proyecto-rec/lib'
make -C app
make[1]: Entering directory '.../proyecto-rec/app'
gcc -Wall -Wextra -std=c17 -I../lib -c main.c -o main.o
gcc -L../lib -o calculator main.o -lcalc
make[1]: Leaving directory '.../proyecto-rec/app'
```

app/ recompila main.o porque main.c cambio. Pero no detecto el cambio en
`lib/calc.h` del paso anterior. El sub-make de app/ solo vigila archivos
dentro de su propio directorio.

### Paso 2.5 -- Forzar recompilacion para verificar

Para confirmar que la falta de recompilacion es un bug (no un caso donde
realmente no habia nada que hacer), fuerza una recompilacion completa:

```bash
make clean && make
```

Salida esperada:

```
...
gcc -Wall -Wextra -std=c17 -c calc.c -o calc.o
ar rcs libcalc.a calc.o
...
gcc -Wall -Wextra -std=c17 -I../lib -c main.c -o main.o
gcc -L../lib -o calculator main.o -lcalc
...
```

Ahora todo se recompilo. Pero no deberia ser necesario hacer `make clean` --
un build incremental correcto deberia detectar que calc.h cambio y recompilar
los archivos que dependen de el.

### Paso 2.6 -- Por que esto es peligroso

En este caso solo hicimos `touch` en el header. Pero imagina que cambias la
**firma** de una funcion en calc.h (por ejemplo, `add()` pasa a recibir
tres argumentos). Con recursive make:

1. `lib/calc.c` no se recompila (el header no esta listado como dependencia)
2. `app/main.o` **no** se recompila
3. El programa se enlaza con un main.o que llama a `add(a, b)` y un libcalc.a
   donde `add` podria esperar tres argumentos
4. Resultado: comportamiento indefinido, crashes silenciosos

Este es el bug clasico del recursive make: builds incrementales que producen
binarios incorrectos sin ningun error ni warning. La "solucion" de hacer
`make clean && make` anula la ventaja principal de Make: compilar solo lo
necesario.

### Limpieza de la parte 2

```bash
cd ..
rm -rf proyecto-rec
```

---

## Parte 3 -- Non-recursive make: dependencias correctas

**Objetivo**: Convertir el proyecto a un unico Makefile en la raiz que conoce
todos los archivos fuente, usa -MMD/-MP para generar dependencias automaticas,
y detecta correctamente los cambios en headers entre subdirectorios.

### Paso 3.1 -- Crear la estructura non-recursive

```bash
mkdir -p proyecto-nonrec/lib proyecto-nonrec/app
```

Copia los archivos fuente (sin Makefiles individuales):

```bash
cp calc.h calc.c proyecto-nonrec/lib/
cp main.c proyecto-nonrec/app/
cp Makefile.nonrecursive proyecto-nonrec/Makefile
```

Verifica la estructura:

```bash
find proyecto-nonrec -type f | sort
```

Salida esperada:

```
proyecto-nonrec/Makefile
proyecto-nonrec/app/main.c
proyecto-nonrec/lib/calc.c
proyecto-nonrec/lib/calc.h
```

Observa: no hay Makefiles en lib/ ni app/. Solo un Makefile en la raiz.

### Paso 3.2 -- Examinar el Makefile non-recursive

```bash
cat proyecto-nonrec/Makefile
```

Observa las diferencias con el recursivo:

- **Todos** los archivos fuente estan listados: `LIB_SRCS := lib/calc.c`,
  `APP_SRCS := app/main.c`
- Una sola regla `%.o: %.c` con `-MMD -MP` para generar archivos .d
- `-include $(ALL_DEPS)` incorpora las dependencias de headers automaticamente
- Las dependencias entre componentes son a nivel de **archivo**, no de
  directorio: `$(APP): $(APP_OBJS) $(LIB)` dice que el ejecutable depende
  de los objetos de app Y de libcalc.a

### Paso 3.3 -- Predecir la salida de compilacion

Antes de compilar, responde mentalmente:

- Habra mensajes de `make[1]: Entering directory`?
- Cuantas invocaciones de gcc esperas ver?
- Que flags nuevos aparecen respecto al recursive make?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.4 -- Compilar por primera vez

```bash
cd proyecto-nonrec && make
```

Salida esperada:

```
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c lib/calc.c -o lib/calc.o
ar rcs lib/libcalc.a lib/calc.o
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c app/main.c -o app/main.o
gcc -o app/calculator app/main.o lib/libcalc.a
```

Observa:

- No hay `make[1]` -- no hay sub-makes, todo se ejecuta en un solo proceso
- Cada compilacion incluye `-MMD -MP`, que genera archivos .d con las
  dependencias de headers
- La salida es mas limpia: sin mensajes de "Entering/Leaving directory"

### Paso 3.5 -- Examinar las dependencias generadas

```bash
cat app/main.d
```

Salida esperada:

```
app/main.o: app/main.c lib/calc.h
```

El compilador detecto que `app/main.c` incluye `lib/calc.h` y lo registro en
el archivo .d. En la proxima ejecucion de make, `-include $(ALL_DEPS)` lee
este archivo y Make sabe que `app/main.o` depende de `lib/calc.h`.

```bash
cat lib/calc.d
```

Salida esperada:

```
lib/calc.o: lib/calc.c lib/calc.h
```

Ambos archivos .d muestran las dependencias reales detectadas por el
compilador via `#include`. Make usa esta informacion para tomar decisiones
de recompilacion correctas.

### Paso 3.6 -- Verificar el programa

```bash
./app/calculator
```

Salida esperada:

```
add(3, 5) = 8
sub(10, 4) = 6
```

### Paso 3.7 -- Predecir: que pasa si modificamos lib/calc.h

Antes de continuar, responde mentalmente:

- `app/main.d` dice que `app/main.o` depende de `lib/calc.h`. Si cambias
  `lib/calc.h`, Make recompilara `app/main.o`?
- Se recompilara tambien `lib/calc.o`?
- Se regenerara `lib/libcalc.a`?
- Se reenlazara `app/calculator`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.8 -- Modificar lib/calc.h y recompilar

```bash
touch lib/calc.h
make
```

Salida esperada:

```
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c lib/calc.c -o lib/calc.o
ar rcs lib/libcalc.a lib/calc.o
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c app/main.c -o app/main.o
gcc -o app/calculator app/main.o lib/libcalc.a
```

Make detecto el cambio en `lib/calc.h` y recompilo **todo lo necesario**:

1. `lib/calc.o` -- porque depende de `lib/calc.h` (via lib/calc.d)
2. `lib/libcalc.a` -- porque depende de `lib/calc.o`
3. `app/main.o` -- porque depende de `lib/calc.h` (via app/main.d)
4. `app/calculator` -- porque depende de `app/main.o` y `lib/libcalc.a`

Este es el comportamiento correcto que el recursive make **no podia lograr**.
La misma operacion (`touch lib/calc.h`) que en la parte 2 no provoco ninguna
recompilacion, aqui recompila todo lo que depende del header.

### Paso 3.9 -- Verificar build incremental sin cambios

```bash
make
```

Salida esperada:

```
make: Nothing to be done for 'all'.
```

Sin cambios, nada se recompila. Y la decision es correcta, no una suposicion.
Observa que la salida dice "Nothing to be done" directamente, sin entrar a
ningun subdirectorio.

### Paso 3.10 -- Predecir: que pasa si solo cambia un fuente

Si haces `touch app/main.c`, que archivos se recompilaran? Piensa en el
grafo de dependencias:

- `app/main.o` depende de `app/main.c` -- se recompilara?
- `lib/calc.o` depende de `lib/calc.c` -- se recompilara?
- `lib/libcalc.a` depende de `lib/calc.o` -- se regenerara?
- `app/calculator` depende de `app/main.o` y `lib/libcalc.a` -- se reenlazara?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.11 -- Verificar recompilacion selectiva

```bash
touch app/main.c
make
```

Salida esperada:

```
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c app/main.c -o app/main.o
gcc -o app/calculator app/main.o lib/libcalc.a
```

Solo se recompilo `app/main.o` y se reenlazo `app/calculator`. La biblioteca
`lib/libcalc.a` no se toco porque no depende de `app/main.c`. El grafo de
dependencias es preciso: recompila exactamente lo necesario, ni mas ni menos.

```bash
cd ..
```

---

## Parte 4 -- Patron module.mk

**Objetivo**: Reorganizar el Makefile non-recursive usando fragmentos
module.mk por subdirectorio. Cada modulo define sus archivos fuente en un
fragmento que el Makefile raiz incluye con `include`. Esto mantiene los
beneficios del non-recursive (grafo global) con la organizacion del
recursivo (cada subdirectorio gestiona su lista de archivos).

### Paso 4.1 -- Crear la estructura con module.mk

```bash
mkdir -p proyecto-modul/lib proyecto-modul/app
```

Copia los archivos fuente y Makefiles:

```bash
cp calc.h calc.c proyecto-modul/lib/
cp main.c proyecto-modul/app/
cp Makefile.modulemk proyecto-modul/Makefile
cp module.mk.lib proyecto-modul/lib/module.mk
cp module.mk.app proyecto-modul/app/module.mk
```

Verifica la estructura:

```bash
find proyecto-modul -type f | sort
```

Salida esperada:

```
proyecto-modul/Makefile
proyecto-modul/app/main.c
proyecto-modul/app/module.mk
proyecto-modul/lib/calc.c
proyecto-modul/lib/calc.h
proyecto-modul/lib/module.mk
```

Observa que hay un `module.mk` en cada subdirectorio, pero no hay Makefiles
independientes como en el recursivo.

### Paso 4.2 -- Examinar los fragmentos module.mk

```bash
cat proyecto-modul/lib/module.mk
```

Salida esperada:

```makefile
# lib/module.mk
# Fragment included by the root Makefile

LIB_SRCS := lib/calc.c
LIB      := lib/libcalc.a
```

```bash
cat proyecto-modul/app/module.mk
```

Salida esperada:

```makefile
# app/module.mk
# Fragment included by the root Makefile

APP_SRCS := app/main.c
APP      := app/calculator
```

Cada module.mk solo define variables -- no tiene reglas. Los paths son
relativos a la raiz del proyecto (no al subdirectorio), porque el Makefile
raiz los incluye y los usa directamente.

### Paso 4.3 -- Examinar el Makefile raiz

```bash
cat proyecto-modul/Makefile
```

Observa:

- `include lib/module.mk` e `include app/module.mk` importan las definiciones
- El Makefile raiz no lista archivos fuente directamente -- los obtiene de
  los fragmentos
- Las reglas de compilacion son identicas al Makefile non-recursive
- Si lib/ agrega un nuevo archivo fuente, solo hay que editar `lib/module.mk`
  -- el Makefile raiz no cambia

### Paso 4.4 -- Predecir la salida

Antes de compilar, responde mentalmente:

- La salida sera diferente a la del non-recursive puro (parte 3)?
- Habra mensajes de `make[1]`?
- La generacion de archivos .d funcionara igual?

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.5 -- Compilar y verificar

```bash
cd proyecto-modul && make
```

Salida esperada:

```
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c lib/calc.c -o lib/calc.o
ar rcs lib/libcalc.a lib/calc.o
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c app/main.c -o app/main.o
gcc -o app/calculator app/main.o lib/libcalc.a
```

La salida es identica a la del non-recursive puro. La directiva `include`
no cambia el comportamiento de Make -- solo la organizacion del codigo del
Makefile.

```bash
./app/calculator
```

Salida esperada:

```
add(3, 5) = 8
sub(10, 4) = 6
```

### Paso 4.6 -- Verificar dependencias cruzadas

```bash
touch lib/calc.h
make
```

Salida esperada:

```
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c lib/calc.c -o lib/calc.o
ar rcs lib/libcalc.a lib/calc.o
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c app/main.c -o app/main.o
gcc -o app/calculator app/main.o lib/libcalc.a
```

Las dependencias cruzadas funcionan correctamente, igual que en la parte 3.
El patron module.mk no pierde ninguna ventaja del non-recursive make: el
grafo de dependencias sigue siendo global y completo.

### Paso 4.7 -- Verificar recompilacion selectiva

```bash
touch lib/calc.c
make
```

Salida esperada:

```
gcc -Wall -Wextra -std=c17 -Ilib -MMD -MP -c lib/calc.c -o lib/calc.o
ar rcs lib/libcalc.a lib/calc.o
gcc -o app/calculator app/main.o lib/libcalc.a
```

Solo se recompilo `lib/calc.o` y se regenero `lib/libcalc.a`. Luego se
reenlazo `app/calculator` porque depende de `lib/libcalc.a`. Pero
`app/main.o` no se recompilo porque no depende de `lib/calc.c`. El grafo
es preciso.

### Paso 4.8 -- Ventaja del module.mk: agregar un archivo fuente

Imagina que necesitas agregar un nuevo archivo `lib/extra.c`. Con module.mk,
el proceso es:

1. Crear `lib/extra.c`
2. Agregar `lib/extra.c` a `LIB_SRCS` en `lib/module.mk`
3. No tocar el Makefile raiz

Con el Makefile non-recursive puro (parte 3), tendrias que editar el Makefile
raiz cada vez que agregas o eliminas un archivo fuente. El patron module.mk
escala mejor porque cada modulo gestiona su propia lista de archivos.

En proyectos grandes, esto es una diferencia significativa: un desarrollador
que trabaja en lib/ puede agregar archivos sin tocar el Makefile raiz, lo
cual reduce conflictos de merge en equipos.

```bash
cd ..
```

---

## Limpieza final

```bash
rm -rf proyecto-nonrec proyecto-modul
```

Verifica que solo quedan los archivos originales del lab:

```bash
ls
```

Salida esperada:

```
Makefile.modulemk       Makefile.recursive.lib  calc.c  main.c        module.mk.lib
Makefile.nonrecursive   Makefile.recursive.root calc.h  module.mk.app
Makefile.recursive.app  README.md
```

---

## Conceptos reforzados

1. En recursive make, cada sub-make tiene una **vision parcial** del proyecto.
   El Makefile de app/ no sabe que archivos tiene lib/, por lo que no puede
   detectar cambios en headers de lib/ que afectan a sus propios objetos.

2. El problema central del recursive make es que los **builds incrementales
   son poco fiables**: un cambio en `lib/calc.h` no provoca la recompilacion
   de `app/main.o`, produciendo binarios potencialmente incorrectos sin
   ningun error ni warning.

3. La solucion historica de hacer `make clean && make` anula la ventaja
   principal de Make: compilar solo lo necesario. Si hay que recompilar
   todo cada vez, no tiene sentido usar Make.

4. Un Makefile **non-recursive** tiene vision completa del grafo de
   dependencias. Con `-MMD -MP` y `-include $(ALL_DEPS)`, Make sabe
   exactamente que objetos dependen de que headers, incluso entre
   subdirectorios.

5. Los archivos `.d` generados por `-MMD` contienen las dependencias
   reales detectadas por el compilador (via `#include`). Make los lee
   con `-include` y los incorpora al grafo, haciendo los builds
   incrementales **correctos y precisos**.

6. El patron **module.mk** combina lo mejor de ambos enfoques: cada
   subdirectorio define sus archivos fuente en un fragmento (organizacion
   local), pero el Makefile raiz incluye todos los fragmentos y construye
   un grafo global (dependencias correctas).

7. El non-recursive make produce salida mas limpia (sin `make[1]: Entering
   directory`), ejecuta un solo proceso Make (sin overhead de invocaciones
   recursivas), y permite que `make -jN` paralelice correctamente con un
   unico pool de jobs.
