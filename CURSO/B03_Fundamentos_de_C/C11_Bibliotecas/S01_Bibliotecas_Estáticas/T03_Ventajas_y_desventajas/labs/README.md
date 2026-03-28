# Lab — Ventajas y desventajas del enlace estatico

## Objetivo

Comparar de forma practica los tradeoffs del enlace estatico frente al dinamico.
Al finalizar, habras medido la diferencia de tamano entre ambos tipos de
binario, inspeccionado sus dependencias, verificado la portabilidad del binario
estatico, evaluado el uso de disco cuando multiples programas comparten la misma
biblioteca, y comprobado que actualizar una biblioteca dinamica surte efecto sin
recompilar.

## Prerequisitos

- GCC instalado
- Paquete de libc estatica instalado (ver LABS.md)
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `mathutil.h` | API publica: factorial, fibonacci, is_prime, version |
| `mathutil.c` | Implementacion de la biblioteca (version 1.0.0) |
| `mathutil_v2.c` | Implementacion actualizada (version 2.0.0) |
| `app_calc.c` | Programa que calcula factorial, fibonacci y primalidad |
| `app_stats.c` | Programa que lista primos y numeros de Fibonacci |

---

## Parte 1 — Construir la biblioteca y compilar ambas versiones

**Objetivo**: Crear `libmathutil.a` (estatica) y `libmathutil.so` (dinamica),
y compilar el mismo programa enlazando contra cada una.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat mathutil.h
```

Observa la interfaz: cuatro funciones, incluyendo `mathutil_version()` que
retorna un string con la version. Esta funcion sera clave en la parte 6.

```bash
cat mathutil.c
```

```bash
cat app_calc.c
```

El programa `app_calc.c` usa las funciones de la biblioteca para un numero dado
como argumento (por defecto 10).

### Paso 1.2 — Compilar el archivo objeto de la biblioteca

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c mathutil.c -o mathutil.o
```

### Paso 1.3 — Crear la biblioteca estatica

```bash
ar rcs libmathutil.a mathutil.o
```

Verifica que se creo:

```bash
file libmathutil.a
```

Salida esperada:

```
libmathutil.a: current ar archive
```

### Paso 1.4 — Crear la biblioteca dinamica

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -shared -fPIC mathutil.c -o libmathutil.so
```

### Paso 1.5 — Compilar el programa con enlace estatico completo

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -static app_calc.c -L. -lmathutil -o calc_static
```

La flag `-static` indica que TODO el enlace es estatico, incluyendo libc.

### Paso 1.6 — Compilar el programa con enlace dinamico

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic app_calc.c -L. -lmathutil -o calc_dynamic
```

Sin `-static`, la biblioteca propia (`libmathutil.a`) se copia al binario porque
es un `.a`, pero libc se enlaza dinamicamente. Para que use la `.so`:

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic app_calc.c -L. -lmathutil -o calc_dynamic \
    -Wl,-rpath,'$ORIGIN'
```

Aqui `-Wl,-rpath,'$ORIGIN'` le dice al linker dinamico que busque `.so` en el
mismo directorio que el ejecutable.

Nota: cuando GCC encuentra tanto `libmathutil.a` como `libmathutil.so` en el
mismo directorio, prefiere la `.so` por defecto.

### Paso 1.7 — Verificar que ambos producen el mismo resultado

```bash
./calc_static 12
```

Salida esperada:

```
mathutil version: 1.0.0
factorial(12)  = 479001600
fibonacci(12)  = 144
is_prime(12)   = no
```

```bash
LD_LIBRARY_PATH=. ./calc_dynamic 12
```

Salida esperada (identica):

```
mathutil version: 1.0.0
factorial(12)  = 479001600
fibonacci(12)  = 144
is_prime(12)   = no
```

Ambos binarios producen exactamente el mismo resultado. Las diferencias estan
bajo la superficie.

---

## Parte 2 — Comparar tamanos

**Objetivo**: Medir la diferencia de tamano entre el binario estatico y el
dinamico usando `ls -lh` y `size`.

### Paso 2.1 — Predecir

Antes de medir, responde mentalmente:

- El binario estatico incluye una copia completa de libc. Cuanto mas grande
  crees que sera comparado con el dinamico?
- El binario dinamico solo contiene stubs (entradas PLT/GOT) para las funciones
  de libc. Cual sera su tamano aproximado?

Intenta predecir antes de continuar al siguiente paso.

### Paso 2.2 — Comparar con ls -lh

```bash
ls -lh calc_static calc_dynamic
```

Salida esperada:

```
-rwxr-xr-x 1 <user> <group>  ~16K <date> calc_dynamic
-rwxr-xr-x 1 <user> <group> ~1.8M <date> calc_static
```

El binario estatico es aproximadamente 100 veces mas grande. La mayor parte de
ese tamano es libc estatica.

### Paso 2.3 — Desglose con size

```bash
size calc_static calc_dynamic
```

Salida esperada:

```
   text    data     bss     dec     hex filename
 ~780000  ~24000  ~33000  ~837000  ...  calc_static
  ~2000    ~600    ~16      ~2600  ...  calc_dynamic
```

- `text`: codigo ejecutable -- el estatico tiene mucho mas porque incluye libc
- `data`: datos inicializados
- `bss`: datos sin inicializar

### Paso 2.4 — Reflexion

La diferencia de tamano solo afecta el disco y la transferencia de red. En
memoria, el impacto real depende de cuantos procesos compartan la libc dinamica
(ver parte 5).

---

## Parte 3 — Dependencias e inspeccion

**Objetivo**: Usar `ldd` y `file` para ver que dependencias tiene cada binario.

### Paso 3.1 — Predecir

Antes de ejecutar, responde mentalmente:

- El binario estatico necesita bibliotecas externas en runtime?
- Cuantas dependencias dinamicas tendra el binario dinamico?

Intenta predecir antes de continuar al siguiente paso.

### Paso 3.2 — Inspeccionar con ldd

```bash
ldd calc_static
```

Salida esperada:

```
	not a dynamic executable
```

El binario estatico no depende de ninguna biblioteca externa. Todo esta dentro
del ejecutable.

```bash
ldd calc_dynamic
```

Salida esperada:

```
	linux-vdso.so.1 (0x...)
	libmathutil.so => .../libmathutil.so (0x...)
	libc.so.6 => /lib64/libc.so.6 (0x...)
	/lib64/ld-linux-x86-64.so.2 (0x...)
```

El binario dinamico depende de:

- `libmathutil.so` -- nuestra biblioteca
- `libc.so.6` -- la biblioteca estandar de C
- `ld-linux-x86-64.so.2` -- el linker dinamico
- `linux-vdso.so.1` -- interfaz virtual del kernel

### Paso 3.3 — Inspeccionar con file

```bash
file calc_static calc_dynamic
```

Salida esperada:

```
calc_static:  ELF 64-bit LSB executable, ..., statically linked, ...
calc_dynamic: ELF 64-bit LSB executable, ..., dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2, ...
```

`file` confirma el tipo de enlace. El binario dinamico menciona al "interpreter"
(el linker dinamico), mientras el estatico dice "statically linked".

---

## Parte 4 — Portabilidad

**Objetivo**: Demostrar que un binario estatico funciona al copiarse a un
directorio aislado, mientras que el dinamico falla si no encuentra sus `.so`.

### Paso 4.1 — Crear un directorio de prueba aislado

```bash
mkdir -p portability_test
```

### Paso 4.2 — Copiar ambos binarios

```bash
cp calc_static calc_dynamic portability_test/
```

### Paso 4.3 — Predecir

Antes de ejecutar, responde mentalmente:

- El binario estatico funcionara en el nuevo directorio?
- El binario dinamico funcionara? Recuerda que `libmathutil.so` esta en el
  directorio original, no en `portability_test/`.

Intenta predecir antes de continuar al siguiente paso.

### Paso 4.4 — Probar el binario estatico

```bash
portability_test/calc_static 7
```

Salida esperada:

```
mathutil version: 1.0.0
factorial(7)  = 5040
fibonacci(7)  = 13
is_prime(7)   = yes
```

Funciona perfectamente. No necesita nada externo.

### Paso 4.5 — Probar el binario dinamico

```bash
portability_test/calc_dynamic 7
```

Salida esperada:

```
portability_test/calc_dynamic: error while loading shared libraries: libmathutil.so: cannot open shared object file: No such file or directory
```

El binario dinamico falla porque el linker dinamico no puede encontrar
`libmathutil.so`. El `-rpath '$ORIGIN'` apunta al directorio del ejecutable
(`portability_test/`), donde no existe la `.so`.

### Paso 4.6 — Verificar con ldd desde el directorio de prueba

```bash
ldd portability_test/calc_dynamic
```

Salida esperada:

```
	linux-vdso.so.1 (0x...)
	libmathutil.so => not found
	libc.so.6 => /lib64/libc.so.6 (0x...)
	/lib64/ld-linux-x86-64.so.2 (0x...)
```

`libmathutil.so => not found` confirma el problema. La libc del sistema se
encuentra porque esta en las rutas estandar, pero nuestra biblioteca no.

### Paso 4.7 — Corregir copiando la .so

```bash
cp libmathutil.so portability_test/
portability_test/calc_dynamic 7
```

Salida esperada:

```
mathutil version: 1.0.0
factorial(7)  = 5040
fibonacci(7)  = 13
is_prime(7)   = yes
```

Ahora funciona. El binario dinamico necesita que sus dependencias esten
disponibles en las rutas esperadas. El binario estatico no tiene esta
restriccion.

---

## Parte 5 — Uso de disco con multiples programas

**Objetivo**: Medir cuanto espacio en disco consume cada estrategia cuando dos
programas comparten la misma biblioteca.

### Paso 5.1 — Compilar el segundo programa

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -static app_stats.c -L. -lmathutil -o stats_static
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic app_stats.c -L. -lmathutil -o stats_dynamic \
    -Wl,-rpath,'$ORIGIN'
```

### Paso 5.2 — Predecir

Ahora hay dos programas (`calc` y `stats`) enlazados contra la misma
biblioteca. Antes de medir, responde mentalmente:

- En el caso estatico, cada binario tiene su propia copia de libc y de
  libmathutil. Cuanto disco total ocuparan los dos?
- En el caso dinamico, la `.so` se comparte. Cuanto disco total ocuparan los
  dos binarios mas la `.so`?

Intenta predecir antes de continuar al siguiente paso.

### Paso 5.3 — Medir tamanos individuales

```bash
ls -lh calc_static stats_static calc_dynamic stats_dynamic libmathutil.so
```

Salida esperada:

```
-rwxr-xr-x 1 <user> <group>  ~16K <date> calc_dynamic
-rwxr-xr-x 1 <user> <group> ~1.8M <date> calc_static
-rwxr-xr-x 1 <user> <group>  ~16K <date> stats_dynamic
-rwxr-xr-x 1 <user> <group> ~1.8M <date> stats_static
-rwxr-xr-x 1 <user> <group>  ~16K <date> libmathutil.so
```

### Paso 5.4 — Calcular el total

Espacio con enlace estatico: `~1.8M + ~1.8M = ~3.6M`

Espacio con enlace dinamico: `~16K + ~16K + ~16K (la .so) = ~48K`

Con solo 2 programas, el enlace estatico usa aproximadamente 75 veces mas disco.
En un sistema con decenas de programas que comparten libc, la diferencia es aun
mayor.

### Paso 5.5 — Verificar que stats funciona

```bash
./stats_static
```

Salida esperada:

```
mathutil version: 1.0.0

Prime numbers up to 50:
  2  3  5  7  11  13  17  19  23  29  31  37  41  43  47
  (15 primes found)

First 15 Fibonacci numbers:
  0 1 1 2 3 5 8 13 21 34 55 89 144 233 377
```

---

## Parte 6 — Actualizacion de biblioteca

**Objetivo**: Demostrar que una actualizacion de la biblioteca dinamica surte
efecto sin recompilar los programas, mientras que con enlace estatico hay que
recompilar.

### Paso 6.1 — Examinar la version actualizada

```bash
cat mathutil_v2.c
```

La unica diferencia con `mathutil.c` es que `VERSION` cambia de `"1.0.0"` a
`"2.0.0"`. En un escenario real, la v2 podria corregir un bug o mejorar el
rendimiento.

### Paso 6.2 — Estado actual: ambos programas usan v1.0.0

```bash
./calc_static 5
LD_LIBRARY_PATH=. ./calc_dynamic 5
```

Salida esperada (ambos):

```
mathutil version: 1.0.0
factorial(5)  = 120
fibonacci(5)  = 5
is_prime(5)   = yes
```

### Paso 6.3 — Actualizar la biblioteca dinamica

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -shared -fPIC mathutil_v2.c -o libmathutil.so
```

Esto reemplaza `libmathutil.so` con la version 2.0.0. No se recompila ningun
programa.

### Paso 6.4 — Predecir

Antes de ejecutar, responde mentalmente:

- El binario dinamico (`calc_dynamic`) mostrara version 1.0.0 o 2.0.0?
- El binario estatico (`calc_static`) mostrara version 1.0.0 o 2.0.0?

Intenta predecir antes de continuar al siguiente paso.

### Paso 6.5 — Verificar el binario dinamico

```bash
LD_LIBRARY_PATH=. ./calc_dynamic 5
```

Salida esperada:

```
mathutil version: 2.0.0
factorial(5)  = 120
fibonacci(5)  = 5
is_prime(5)   = yes
```

El binario dinamico usa la version 2.0.0 sin haber sido recompilado. El linker
dinamico carga la `.so` nueva en cada ejecucion.

### Paso 6.6 — Verificar el binario estatico

```bash
./calc_static 5
```

Salida esperada:

```
mathutil version: 1.0.0
factorial(5)  = 120
fibonacci(5)  = 5
is_prime(5)   = yes
```

El binario estatico sigue en version 1.0.0. La biblioteca esta embebida dentro
del ejecutable; para actualizarla hay que recompilar.

### Paso 6.7 — Recompilar el estatico para obtener la v2

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -c mathutil_v2.c -o mathutil.o
ar rcs libmathutil.a mathutil.o
gcc -std=c11 -Wall -Wextra -Wpedantic -static app_calc.c -L. -lmathutil -o calc_static
./calc_static 5
```

Salida esperada:

```
mathutil version: 2.0.0
factorial(5)  = 120
fibonacci(5)  = 5
is_prime(5)   = yes
```

Para actualizar un binario estatico, hay que:

1. Recompilar la biblioteca
2. Recrear el `.a`
3. Re-enlazar el programa

Si hay N programas que usan esa biblioteca, hay que repetir el paso 3 para
cada uno. Con enlace dinamico, el paso 3 no es necesario para ninguno.

---

## Limpieza final

```bash
rm -f mathutil.o app_calc.o app_stats.o
rm -f libmathutil.a libmathutil.so
rm -f calc_static calc_dynamic stats_static stats_dynamic
rm -rf portability_test
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  app_calc.c  app_stats.c  mathutil.c  mathutil.h  mathutil_v2.c
```

---

## Conceptos reforzados

1. Un binario con enlace estatico completo (`-static`) incluye toda libc y todas
   las bibliotecas dentro del ejecutable. `ldd` reporta "not a dynamic
   executable" porque no depende de nada externo en runtime.

2. El tamano del binario estatico es drasticamente mayor (tipicamente ~100x para
   programas pequenos). La mayor parte de ese tamano es libc estatica. El
   comando `size` permite ver el desglose en secciones `text`, `data` y `bss`.

3. Un binario estatico es completamente portable: se puede copiar a cualquier
   directorio o maquina (misma arquitectura) y funciona sin dependencias. Un
   binario dinamico falla si no encuentra sus `.so` en las rutas esperadas.

4. Cuando multiples programas comparten la misma biblioteca, el enlace dinamico
   ahorra espacio en disco significativamente: la `.so` existe una sola vez,
   mientras que con enlace estatico cada binario tiene su propia copia.

5. Actualizar una biblioteca dinamica (reemplazar la `.so`) surte efecto para
   todos los programas que la usan sin necesidad de recompilar. Con enlace
   estatico, hay que recompilar y re-enlazar cada programa individualmente.

6. El tradeoff fundamental es: el enlace estatico prioriza portabilidad,
   simplicidad de despliegue y determinismo; el enlace dinamico prioriza ahorro
   de disco/memoria, actualizaciones centralizadas y flexibilidad.
