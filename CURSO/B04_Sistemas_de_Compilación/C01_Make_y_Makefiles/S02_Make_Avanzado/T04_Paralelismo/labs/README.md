# Lab -- Paralelismo en Make

## Objetivo

Medir el speedup real de `make -j` frente a la compilacion secuencial,
dominar order-only prerequisites para crear directorios de salida, controlar
la salida intercalada con `--output-sync`, proteger builds con
`.DELETE_ON_ERROR`, y detectar dependencias faltantes que solo se manifiestan
en compilacion paralela. Al finalizar, sabras escribir Makefiles seguros para
compilacion con `-j` y diagnosticar race conditions causadas por
dependencias no declaradas.

## Prerequisitos

- GCC instalado
- GNU Make 4.x
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `funcs.h` | Header comun con declaraciones de 10 funciones compute_*() |
| `main_parallel.c` | main() que suma los resultados de las 10 funciones |
| `alpha.c` .. `juliet.c` | 10 archivos con funciones que hacen trabajo computacional |
| `app_generated.c` | Programa que depende de un header generado (generated.h) |
| `generated.h.in` | Template para generar generated.h |
| `Makefile.speedup` | Makefile para las partes 1 y 4 |
| `Makefile.outputsync` | Makefile con echo marcadores para la parte 2 |
| `Makefile.no_orderonly` | Makefile sin order-only (mkdir en cada recipe) |
| `Makefile.orderonly` | Makefile con order-only prerequisites |
| `Makefile.race` | Makefile con dependencia faltante intencional |
| `Makefile.race_fixed` | Makefile con la dependencia corregida |

---

## Parte 1 -- Speedup con -j

**Objetivo**: Compilar un proyecto de 10 archivos .c con distintos niveles de
paralelismo (-j1, -j2, -j4, -j$(nproc)) y medir la diferencia de tiempo real.

### Paso 1.1 -- Examinar el proyecto

```bash
cat funcs.h
```

Observa que declara 10 funciones `compute_*()`, una por cada archivo .c
(alpha hasta juliet).

```bash
cat alpha.c
```

Cada funcion hace un bucle de 5 millones de iteraciones con una variable
`volatile` para evitar que el optimizador lo elimine. Esto garantiza que
cada compilacion tarde un tiempo medible.

```bash
cat main_parallel.c
```

El `main()` llama a las 10 funciones y suma sus resultados.

### Paso 1.2 -- Examinar el Makefile

```bash
cat Makefile.speedup
```

Observa:

- La lista de SRCS incluye los 11 archivos (.c del main + 10 funciones)
- Cada `.o` depende de `funcs.h` (prerequisite normal)
- `.DELETE_ON_ERROR` esta presente (lo exploraremos en la parte 4)
- Se compila con `-O0` para que gcc no optimice el tiempo de compilacion

### Paso 1.3 -- Predecir la diferencia

Antes de compilar, responde mentalmente:

- Hay 11 archivos .c independientes entre si. Cuantos se pueden compilar
  en paralelo antes del paso de linking?
- Si tu maquina tiene N nucleos, cual es el speedup teorico maximo con -jN?
- El paso de linking es secuencial. Limita mucho el speedup?

Intenta predecir antes de continuar al siguiente paso.

### Paso 1.4 -- Compilar con -j1 (secuencial)

```bash
make -f Makefile.speedup clean
time make -f Makefile.speedup -j1
```

Anota el tiempo `real` que reporta `time`. Este es tu baseline secuencial.

Salida esperada (los tiempos varian segun tu hardware):

```
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o main_parallel.o main_parallel.c
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o alpha.o alpha.c
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o bravo.o bravo.c
...
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o juliet.o juliet.c
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -o parallel_demo main_parallel.o alpha.o ...

real    ~0m2.5s
...
```

Las 11 compilaciones se ejecutan una tras otra. Make espera a que cada una
termine antes de lanzar la siguiente.

### Paso 1.5 -- Compilar con -j2

```bash
make -f Makefile.speedup clean
time make -f Makefile.speedup -j2
```

Salida esperada:

```
...
real    ~0m1.4s
...
```

Anota el tiempo `real`. Con 2 jobs, Make lanza 2 compilaciones
simultaneamente. El tiempo deberia ser aproximadamente la mitad del baseline.

### Paso 1.6 -- Compilar con -j4

```bash
make -f Makefile.speedup clean
time make -f Makefile.speedup -j4
```

Salida esperada:

```
...
real    ~0m0.8s
...
```

Anota el tiempo `real`. Con 4 jobs, el tiempo deberia ser aproximadamente
un cuarto del baseline.

### Paso 1.7 -- Compilar con -j$(nproc)

Primero verifica cuantos nucleos tienes:

```bash
nproc
```

```bash
make -f Makefile.speedup clean
time make -f Makefile.speedup -j$(nproc)
```

Anota el tiempo `real`. Con tantos jobs como nucleos, el speedup sera maximo
(limitado por los 11 archivos disponibles para compilar en paralelo).

### Paso 1.8 -- Construir la tabla de speedup

Con los tiempos anotados, calcula el speedup para cada nivel de paralelismo:

```
Speedup = tiempo_j1 / tiempo_jN
```

Tu tabla deberia verse similar a (los tiempos son aproximados):

```
Jobs       | Tiempo   | Speedup
-----------|----------|--------
-j1        | ~2.5s    | 1.0x
-j2        | ~1.4s    | ~1.8x
-j4        | ~0.8s    | ~3.1x
-j$(nproc) | ~0.5s    | ~5.0x
```

El speedup no sera exactamente N porque:

- El linking es secuencial y no se beneficia del paralelismo
- Hay overhead de coordinacion de procesos
- Solo hay 11 archivos: con mas de 11 nucleos, el speedup no crece

Esto refleja la ley de Amdahl: las partes secuenciales (linking) limitan
la ganancia maxima. Con 10 archivos independientes, el speedup teorico
maximo es cercano a 10, independientemente de cuantos nucleos tengas.

### Paso 1.9 -- Verificar el resultado

```bash
./parallel_demo
```

Salida esperada:

```
Total: <un numero>
```

El numero concreto depende de la aritmetica de los bucles. Lo importante es
que el programa compila y ejecuta correctamente con cualquier nivel de -j.

### Limpieza de la parte 1

```bash
make -f Makefile.speedup clean
```

---

## Parte 2 -- Salida intercalada y --output-sync

**Objetivo**: Observar como la salida de multiples compilaciones se mezcla
con `-j` y como `--output-sync` la organiza por target.

### Paso 2.1 -- Examinar el Makefile con marcadores

```bash
cat Makefile.outputsync
```

Este Makefile agrega `@echo ">>> Start: $< <<<"` y `@echo ">>> Done: $< <<<"`
alrededor de cada compilacion, y `@echo "=== Linking $@ ==="` /
`@echo "=== Link complete ==="` para el paso de enlace. Esto hara visible
cuando empieza y termina cada job.

### Paso 2.2 -- Compilar con -j$(nproc) SIN output-sync

```bash
make -f Makefile.outputsync clean
make -f Makefile.outputsync -j$(nproc)
```

Observa la salida. Los marcadores `>>> Start` y `>>> Done` de distintos
archivos aparecen intercalados:

Salida esperada (el orden exacto varia en cada ejecucion):

```
>>> Start: alpha.c <<<
>>> Start: bravo.c <<<
>>> Start: charlie.c <<<
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o delta.o delta.c
>>> Done:  alpha.c <<<
>>> Start: echo_func.c <<<
>>> Done:  charlie.c <<<
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o foxtrot.o foxtrot.c
>>> Done:  bravo.c <<<
>>> Start: golf.c <<<
...
```

Los mensajes de distintos targets se entremezclan. Si hubiera errores de
compilacion, seria dificil saber a que archivo pertenece cada error.

### Paso 2.3 -- Predecir el efecto de --output-sync

Antes de ejecutar, responde mentalmente:

- Con `--output-sync=target`, la salida de cada target se agrupa. Significa
  que veremos todos los mensajes de alpha.c juntos, luego todos los de
  bravo.c, etc.?
- Cambia el orden de ejecucion real? O solo el orden de la salida impresa?
- El tiempo total de compilacion sera mayor, menor, o igual?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.4 -- Compilar con --output-sync

```bash
make -f Makefile.outputsync clean
make -f Makefile.outputsync -j$(nproc) --output-sync=target
```

Salida esperada (el orden de targets varia, pero cada uno esta agrupado):

```
>>> Start: alpha.c <<<
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o alpha.o alpha.c
>>> Done:  alpha.c <<<
>>> Start: bravo.c <<<
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o bravo.o bravo.c
>>> Done:  bravo.c <<<
>>> Start: charlie.c <<<
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o charlie.o charlie.c
>>> Done:  charlie.c <<<
...
=== Linking parallel_demo ===
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -o parallel_demo ...
=== Link complete ===
```

Ahora cada target muestra su salida completa sin interrupciones de otros
targets. La compilacion sigue siendo paralela (el tiempo total es similar),
pero la **salida** se almacena en buffer y se imprime agrupada cuando cada
target termina.

### Paso 2.5 -- La forma corta: -O

```bash
make -f Makefile.outputsync clean
make -f Makefile.outputsync -j$(nproc) -O
```

`-O` es equivalente a `--output-sync=target`. Es la forma recomendada
para uso diario. La combinacion habitual es:

```bash
make -j$(nproc) -O
```

### Limpieza de la parte 2

```bash
make -f Makefile.outputsync clean
```

---

## Parte 3 -- Order-only prerequisites para directorios

**Objetivo**: Entender por que los directorios de salida deben ser
order-only prerequisites (`| build/`) y no prerequisites normales, y como
esto evita recompilaciones innecesarias.

### Paso 3.1 -- Examinar el Makefile sin order-only

```bash
cat Makefile.no_orderonly
```

Observa que cada regla `$(BUILDDIR)/%.o` ejecuta `mkdir -p $(BUILDDIR)` en
su recipe. Esto funciona, pero tiene problemas que veremos a continuacion.

Observa tambien el comentario en el Makefile: "con -j, todos los jobs
ejecutan mkdir simultaneamente".

### Paso 3.2 -- Compilar sin order-only

```bash
make -f Makefile.no_orderonly clean
make -f Makefile.no_orderonly -j$(nproc)
```

Salida esperada:

```
mkdir -p build
  [compiling main_parallel.c]
mkdir -p build
  [compiling alpha.c]
mkdir -p build
  [compiling bravo.c]
mkdir -p build
  [compiling charlie.c]
...
```

Observa que `mkdir -p build` se ejecuta **una vez por cada .o**. Con 11
archivos, se ejecuta 11 veces. `mkdir -p` no falla si el directorio ya
existe, asi que funciona, pero es ineficiente y llena la salida de ruido.

### Paso 3.3 -- Examinar el Makefile con order-only

```bash
cat Makefile.orderonly
```

Observa la diferencia clave en la regla de `.o`:

```
$(BUILDDIR)/%.o: %.c funcs.h | $(BUILDDIR)/
```

El `| $(BUILDDIR)/` es un order-only prerequisite. Make garantiza que
`$(BUILDDIR)/` exista antes de compilar cualquier .o, pero no recompila
si el timestamp del directorio cambia.

Observa tambien que hay una regla separada para crear el directorio:

```makefile
$(BUILDDIR)/:
	mkdir -p $@
```

### Paso 3.4 -- Compilar con order-only

```bash
make -f Makefile.orderonly clean
make -f Makefile.orderonly -j$(nproc)
```

Salida esperada:

```
mkdir -p build/
  [compiling main_parallel.c]
  [compiling alpha.c]
  [compiling bravo.c]
  [compiling charlie.c]
...
```

`mkdir -p build/` aparece **una sola vez** al principio. Make crea el
directorio una vez y luego lanza todas las compilaciones en paralelo.
Comparado con la version sin order-only, la salida es mas limpia y no hay
11 ejecuciones redundantes de mkdir.

### Paso 3.5 -- Predecir el comportamiento en segunda compilacion

Antes de ejecutar, responde mentalmente:

- Si ejecutas `make` de nuevo sin cambiar ningun archivo, se recompilara
  algo?
- El directorio `build/` cambio de timestamp cuando se crearon los .o
  dentro. Si build/ fuera un prerequisite normal, que pasaria?
- Y con order-only?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.6 -- Verificar que no recompila innecesariamente

```bash
make -f Makefile.orderonly -j$(nproc)
```

Salida esperada:

```
make: Nothing to be done for 'all'.
```

Nada se recompila. Aunque el timestamp de `build/` cambio al crear los .o,
como es un order-only prerequisite, Make lo ignora para decidir si
recompilar. Solo verifica que **exista**.

Si `build/` fuera un prerequisite normal (sin el `|`), Make compararia su
timestamp con cada .o. Como el timestamp de un directorio cambia cada vez
que se crea o borra un archivo dentro de el, esto provocaria recompilaciones
innecesarias en cascada: cada .o generado actualizaria el timestamp de
`build/`, haciendo que los demas .o parecieran desactualizados.

### Paso 3.7 -- Verificar con touch

```bash
touch build/
make -f Makefile.orderonly -j$(nproc)
```

Salida esperada:

```
make: Nothing to be done for 'all'.
```

Incluso al cambiar manualmente el timestamp de `build/`, Make no recompila
nada. Ese es el proposito de order-only: garantizar existencia, ignorar
timestamp.

### Limpieza de la parte 3

```bash
make -f Makefile.orderonly clean
make -f Makefile.no_orderonly clean
```

---

## Parte 4 -- .DELETE_ON_ERROR

**Objetivo**: Entender por que `.DELETE_ON_ERROR` es especialmente importante
en compilacion paralela, donde la probabilidad de archivos .o corruptos
es mayor.

### Paso 4.1 -- Revisar .DELETE_ON_ERROR en el Makefile

```bash
cat Makefile.speedup
```

Observa la linea `.DELETE_ON_ERROR:` cerca del inicio. Este target especial
le dice a Make: "si la recipe de un target falla, borra el archivo que se
estaba generando".

### Paso 4.2 -- Entender el problema sin .DELETE_ON_ERROR

Considera este escenario con `make -j8`:

1. Make lanza 8 compilaciones simultaneamente.
2. El sistema esta bajo carga pesada (8 procesos gcc + otros programas).
3. `gcc -c -o hotel.o hotel.c` empieza a escribir `hotel.o`.
4. El OOM killer del kernel mata ese proceso gcc, o el usuario pulsa Ctrl+C.
5. `hotel.o` queda en disco **parcialmente escrito**: tiene bytes pero no es
   un archivo objeto valido.
6. Sin `.DELETE_ON_ERROR`, Make deja ese archivo en disco.
7. En la siguiente ejecucion, Make ve que `hotel.o` existe y su timestamp
   es mas nuevo que `hotel.c`. Concluye: "no necesita recompilar".
8. El linking falla con un error confuso porque `hotel.o` esta corrupto.

Este escenario es particularmente problematico porque el error del linking
no apunta a la causa real (un .o corrupto) y puede ser muy dificil de
diagnosticar. Con `-j`, la probabilidad aumenta: mas procesos compiten por
CPU y memoria, y un Ctrl+C mata todos los procesos gcc en vuelo.

### Paso 4.3 -- Demostrar sin .DELETE_ON_ERROR

Introduce un error temporal en uno de los archivos fuente:

```bash
cp hotel.c hotel.c.bak
```

Agrega un error de sintaxis al final de hotel.c:

```bash
echo "SYNTAX ERROR HERE" >> hotel.c
```

Ahora compila sin `.DELETE_ON_ERROR`. Usa Makefile.no_orderonly que no
tiene esa directiva:

```bash
make -f Makefile.no_orderonly clean
make -f Makefile.no_orderonly -j$(nproc)
```

Salida esperada:

```
...
hotel.c:10:1: error: unknown type name 'SYNTAX'
...
make: *** [Makefile.no_orderonly:...] Error 1
```

La compilacion falla, como se espera. Verifica si quedo un `hotel.o`
parcial:

```bash
ls -la build/hotel.o 2>/dev/null && echo "hotel.o EXISTE (potencialmente corrupto)" || echo "hotel.o no existe"
```

El resultado puede variar: gcc normalmente borra el .o si falla, pero hay
situaciones donde no puede hacerlo (kill -9, OOM killer, disco lleno). El
punto es que **sin** `.DELETE_ON_ERROR`, Make no garantiza la limpieza.

### Paso 4.4 -- Comparar con .DELETE_ON_ERROR

Ahora repite con Makefile.orderonly, que si tiene `.DELETE_ON_ERROR`:

```bash
make -f Makefile.orderonly clean
make -f Makefile.orderonly -j$(nproc)
```

La compilacion falla igualmente, pero `.DELETE_ON_ERROR` garantiza que
Make borre cualquier `build/hotel.o` parcial. En la siguiente ejecucion,
Make sabe que debe recompilar porque el archivo no existe.

### Paso 4.5 -- Restaurar hotel.c

```bash
mv hotel.c.bak hotel.c
```

### Paso 4.6 -- La regla practica

`.DELETE_ON_ERROR` deberia estar en **todo** Makefile. La razon por la que
no es el comportamiento por defecto es compatibilidad historica con
Makefiles de los anos 70. En un Makefile moderno, siempre incluyelo:

```makefile
.DELETE_ON_ERROR:
```

Es critico con `-j` por dos razones:

- Mas procesos gcc en vuelo significa mayor probabilidad de que un kill,
  Ctrl+C, o falta de memoria interrumpa uno a mitad de escritura.
- Un .o corrupto que "pasa" el chequeo de timestamps produce errores de
  linking oscuros que no apuntan a la causa real.

### Limpieza de la parte 4

```bash
make -f Makefile.orderonly clean
make -f Makefile.no_orderonly clean
```

---

## Parte 5 -- Dependencias faltantes con -j

**Objetivo**: Provocar una race condition causada por una dependencia no
declarada en el Makefile, y corregirla. Entender por que `-j1` oculta bugs
que `-j` expone.

### Paso 5.1 -- Examinar el escenario

```bash
cat app_generated.c
```

Este programa hace `#include "generated.h"`. Ese header no existe todavia;
se genera a partir de `generated.h.in` mediante una regla del Makefile.

```bash
cat generated.h.in
```

Es un template simple que define `BUILD_TAG`.

```bash
cat Makefile.race
```

Observa el **bug intencional**: la regla de `app_generated.o` lista
`app_generated.c` y `funcs.h` como prerequisitos, pero **no** lista
`generated.h`. Sin embargo, `app_generated.c` hace `#include "generated.h"`.

La regla de `generated.h` tiene un `sleep 1` intencional para simular una
generacion lenta de header (como un generador de codigo real).

### Paso 5.2 -- Predecir el comportamiento con -j1

Antes de ejecutar, responde mentalmente:

- Make ejecuta las reglas una a una con -j1.
- El target `all` depende de `app_race`.
- `app_race` depende de `app_generated.o` y `alpha.o`.
- Make buscara las reglas para generar esos .o.
- En que orden procesara las reglas? Generara `generated.h` antes o despues
  de intentar compilar `app_generated.o`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.3 -- Compilar con -j1

```bash
make -f Makefile.race clean
make -f Makefile.race -j1
```

Salida esperada:

```
  [generating header...]
  [generated.h created]
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o app_generated.o app_generated.c
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o alpha.o alpha.c
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -o app_race app_generated.o alpha.o
```

Funciona. Con `-j1`, Make ejecuta las reglas en un orden que "por suerte"
genera `generated.h` antes de compilar `app_generated.o`. El Makefile tiene
un bug de dependencias, pero el modo secuencial lo oculta.

### Paso 5.4 -- Predecir con -j

Antes de ejecutar, responde mentalmente:

- `generated.h` tarda 1 segundo en generarse (por el sleep).
- `app_generated.o` necesita `generated.h` para compilar (por el #include).
- Pero esa dependencia **no** esta declarada en el Makefile.
- Con -j, Make puede lanzar ambos jobs simultaneamente?
- Que pasa si gcc intenta compilar `app_generated.c` antes de que
  `generated.h` exista?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.5 -- Compilar con -j y observar la race condition

```bash
make -f Makefile.race clean
make -f Makefile.race -j$(nproc)
```

Salida esperada (con alta probabilidad):

```
  [generating header...]
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o app_generated.o app_generated.c
app_generated.c:2:10: fatal error: generated.h: No such file or directory
    2 | #include "generated.h"
      |          ^~~~~~~~~~~~~
compilation terminated.
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o alpha.o alpha.c
  [generated.h created]
make: *** [Makefile.race:21: app_generated.o] Error 1
```

La compilacion de `app_generated.o` empezo **antes** de que `generated.h`
terminara de generarse. El sleep de 1 segundo hace que la race condition
sea casi segura: gcc intenta abrir `generated.h` inmediatamente, pero el
`cp` que lo crea no se ejecuta hasta despues del sleep.

Este es un bug del Makefile, no de Make. Make respeta las dependencias
declaradas, y `generated.h` no estaba declarada como prerequisito de
`app_generated.o`.

Si no falla en el primer intento (poco probable con el sleep de 1 segundo),
ejecuta `make -f Makefile.race clean && make -f Makefile.race -j$(nproc)`
varias veces.

### Paso 5.6 -- Examinar la correccion

```bash
cat Makefile.race_fixed
```

Compara con Makefile.race. La diferencia clave esta en la regla de
`app_generated.o`:

```
# Makefile.race (BUG):
app_generated.o: app_generated.c funcs.h

# Makefile.race_fixed (CORREGIDO):
app_generated.o: app_generated.c funcs.h generated.h
```

Ahora `generated.h` esta declarado como prerequisito. Make sabe que debe
generarlo **antes** de compilar `app_generated.o`, incluso con `-j`.

Observa tambien que Makefile.race_fixed incluye `.DELETE_ON_ERROR:` como
buena practica.

### Paso 5.7 -- Predecir la ejecucion corregida

Antes de ejecutar, responde mentalmente:

- Ahora `app_generated.o` depende de `generated.h`.
- `alpha.o` NO depende de `generated.h`.
- Con `-j`, cuales jobs pueden correr en paralelo?
- Make esperara a que `generated.h` este listo antes de compilar
  `app_generated.o`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.8 -- Verificar la correccion con -j

```bash
make -f Makefile.race_fixed clean
make -f Makefile.race_fixed -j$(nproc)
```

Salida esperada:

```
  [generating header...]
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o alpha.o alpha.c
  [generated.h created]
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -c -o app_generated.o app_generated.c
gcc -std=c11 -Wall -Wextra -Wpedantic -O0 -o app_race app_generated.o alpha.o
```

Observa el orden: `alpha.o` se compila en paralelo con la generacion de
`generated.h` (no depende de el). Pero `app_generated.o` espera a que
`generated.h` este listo. Esto es exactamente lo que queriamos: maximo
paralelismo posible respetando las dependencias reales.

### Paso 5.9 -- Verificar repetibilidad

```bash
make -f Makefile.race_fixed clean
make -f Makefile.race_fixed -j$(nproc)
```

Ejecutalo varias veces. Siempre funciona. Comparalo con Makefile.race,
que falla de forma no-determinista.

La regla practica:

- Si un build funciona con `-j1` pero falla con `-j`, el Makefile tiene
  dependencias faltantes.
- Siempre probar con `-j$(nproc)` para detectar estos bugs.
- Declarar **todas** las dependencias, incluyendo headers generados.

```bash
./app_race
```

Salida esperada:

```
Build tag: lab-parallel
compute_alpha() = <un numero>
```

Confirma que el programa funciona correctamente.

### Limpieza de la parte 5

```bash
make -f Makefile.race_fixed clean
```

---

## Limpieza final

```bash
rm -f *.o parallel_demo app_race generated.h
rm -rf build/
```

Verifica que solo quedan los archivos fuente originales del lab:

```bash
ls
```

Salida esperada:

```
Makefile.no_orderonly  Makefile.outputsync  Makefile.race  Makefile.race_fixed  Makefile.speedup
README.md  alpha.c  app_generated.c  bravo.c  charlie.c  delta.c  echo_func.c
foxtrot.c  funcs.h  generated.h.in  golf.c  hotel.c  india.c  juliet.c  main_parallel.c
```

---

## Conceptos reforzados

1. `make -j$(nproc)` compila hasta N archivos en paralelo, donde N es el
   numero de nucleos del procesador. El speedup real se acerca a N cuando la
   mayor parte del trabajo es compilacion independiente de .o.

2. El speedup tiene un limite: la ley de Amdahl dice que las partes
   secuenciales (linking, generacion de headers) limitan la ganancia.
   Con 10 archivos y 1 paso de linking, el speedup maximo es cercano a 10,
   independientemente de cuantos nucleos tengas.

3. `--output-sync=target` (o `-O`) agrupa la salida por target, evitando que
   los mensajes de distintas compilaciones se entremezclen. No cambia el
   orden de ejecucion ni el tiempo total; solo organiza la salida en buffer
   y la imprime agrupada cuando cada target completa.

4. Los directorios de salida deben ser **order-only prerequisites**
   (`| build/`), no prerequisites normales. El timestamp de un directorio
   cambia cada vez que se crea o borra un archivo dentro de el. Si fuera
   un prerequisite normal, esto provocaria recompilaciones innecesarias.

5. `mkdir -p` en la recipe de cada `.o` funciona pero es ineficiente: se
   ejecuta una vez por cada archivo. Con order-only, Make crea el directorio
   una sola vez antes de lanzar las compilaciones paralelas.

6. `.DELETE_ON_ERROR:` le dice a Make que borre un target si su recipe falla.
   Sin esto, un proceso gcc interrumpido (por kill, Ctrl+C, o OOM killer)
   puede dejar un archivo .o parcialmente escrito en disco. Make lo
   considerara "up to date" en la siguiente compilacion, produciendo errores
   de linking oscuros. Es especialmente critico con `-j` porque mas procesos
   en vuelo aumentan la probabilidad de fallos parciales.

7. Una dependencia faltante en el Makefile puede funcionar con `-j1` "por
   suerte" (el orden secuencial casualmente genera el prerequisito primero)
   pero fallar con `-j` (race condition). `-j1` oculta bugs de dependencias;
   `-j` los expone. La regla practica: si funciona con `-j1` pero falla con
   `-j`, el Makefile tiene un bug.

8. Toda dependencia real del codigo fuente (`#include`) debe estar reflejada
   en el Makefile. Las dependencias ocultas son la fuente principal de builds
   no-deterministas y race conditions con `-j`.
