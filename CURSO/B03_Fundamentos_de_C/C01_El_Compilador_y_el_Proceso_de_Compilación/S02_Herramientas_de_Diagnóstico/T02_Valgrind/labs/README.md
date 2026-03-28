# Lab -- Valgrind (Memcheck)

## Objetivo

Usar Valgrind para detectar errores de memoria en programas C: memory leaks,
accesos invalidos, uso de memoria no inicializada, use-after-free y double
free. Al finalizar, sabras leer los reportes de Valgrind, interpretar cada
tipo de error y localizar la linea exacta del problema en el codigo fuente.

## Prerequisitos

- GCC instalado
- Valgrind instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `leak.c` | Programa que asigna memoria en funciones y nunca la libera |
| `invalid_access.c` | Programa con lecturas/escrituras fuera de limites y memoria no inicializada |
| `use_after_free.c` | Programa que usa memoria despues de liberarla y libera dos veces |
| `buggy.c` | Programa con todos los tipos de errores combinados |

---

## Parte 1 -- Memory leaks

**Objetivo**: Detectar fugas de memoria con Valgrind y aprender a leer el
HEAP SUMMARY y LEAK SUMMARY.

### Paso 1.1 -- Verificar el entorno

```bash
gcc --version
valgrind --version
```

Confirma que ambas herramientas estan instaladas. Valgrind requiere que los
programas se compilen con informacion de depuracion (`-g`) y sin optimizacion
(`-O0`) para mostrar numeros de linea precisos.

### Paso 1.2 -- Examinar el codigo fuente

```bash
cat leak.c
```

Observa:

- `create_greeting()` usa `malloc()` para crear una cadena y la retorna
- `create_scores()` usa `malloc()` para crear un array de enteros y lo retorna
- `main()` llama a estas funciones pero nunca llama a `free()`

Antes de compilar y ejecutar con Valgrind, predice:

- Cuantas llamadas a `malloc()` hay en total?
- Cuantos bytes se pierden en cada leak? (pista: mira los argumentos de `malloc`)
- Cuantos bytes se pierden en total?

### Paso 1.3 -- Compilar y ejecutar normalmente

```bash
gcc -g -O0 leak.c -o leak
./leak
```

El programa no produce ninguna salida y termina con exito. Sin herramientas
especiales, no hay ninguna indicacion de que algo este mal.

### Paso 1.4 -- Ejecutar con Valgrind

```bash
valgrind --leak-check=full ./leak
```

Salida esperada (los PID y direcciones varian):

```
~PID~ Memcheck, a memory error detector
~PID~ Copyright (C) ...
~PID~ Using Valgrind-3.x.x ...
~PID~ Command: ./leak
~PID~
~PID~
~PID~ HEAP SUMMARY:
~PID~     in use at exit: 46 bytes in 3 blocks
~PID~   total heap usage: 3 allocs, 0 frees, 46 bytes allocated
~PID~
~PID~ 12 bytes in 1 blocks are definitely lost in loss record 1 of 3
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: create_greeting (leak.c:6)
~PID~    by 0x...: main (leak.c:27)
~PID~
~PID~ 14 bytes in 1 blocks are definitely lost in loss record 2 of 3
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: create_greeting (leak.c:6)
~PID~    by 0x...: main (leak.c:23)
~PID~
~PID~ 20 bytes in 1 blocks are definitely lost in loss record 3 of 3
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: create_scores (leak.c:13)
~PID~    by 0x...: main (leak.c:30)
~PID~
~PID~ LEAK SUMMARY:
~PID~    definitely lost: 46 bytes in 3 blocks
~PID~    indirectly lost: 0 bytes in 0 blocks
~PID~      possibly lost: 0 bytes in 0 blocks
~PID~    still reachable: 0 bytes in 0 blocks
~PID~         suppressed: 0 bytes in 0 blocks
~PID~
~PID~ ERROR SUMMARY: 3 errors from 3 contexts
```

### Paso 1.5 -- Interpretar el reporte

Analiza la salida y verifica tu prediccion:

**HEAP SUMMARY**:

- `3 allocs, 0 frees` -- se llamo a `malloc()` 3 veces pero nunca a `free()`
- `in use at exit: 46 bytes in 3 blocks` -- al terminar, 46 bytes siguen
  ocupados en 3 bloques separados

**Cada bloque perdido**:

- Valgrind muestra la stack trace de donde fue asignada la memoria: la
  funcion y el numero de linea exacto
- `create_greeting (leak.c:6)` indica que el `malloc()` de la linea 6 de
  `leak.c` creo el bloque

**LEAK SUMMARY**:

- **definitely lost**: memoria a la que ya no apunta ningun puntero en el
  programa. Es un leak seguro. Esto es lo mas grave.
- **still reachable**: memoria que aun tiene un puntero apuntandola al
  terminar el programa (por ejemplo, un puntero global). Tecnicamente es un
  leak, pero podria ser intencional.
- **indirectly lost**: memoria apuntada solo desde bloques que ya estan
  perdidos (por ejemplo, nodos de una lista enlazada cuyo head se perdio).
- **possibly lost**: Valgrind no esta seguro (hay un puntero que apunta al
  interior del bloque, no al inicio).

### Limpieza de la parte 1

```bash
rm -f leak
```

---

## Parte 2 -- Accesos invalidos

**Objetivo**: Detectar lecturas y escrituras fuera de los limites del heap, y
uso de memoria no inicializada.

### Paso 2.1 -- Examinar el codigo fuente

```bash
cat invalid_access.c
```

Observa las tres funciones:

- `heap_overflow_read()`: lee `arr[5]` de un array de 5 elementos (indices 0-4)
- `heap_overflow_write()`: escribe `arr[3]` de un array de 3 elementos (indices 0-2)
- `uninitialized_read()`: asigna 4 enteros pero solo inicializa 2, luego
  lee los no inicializados en un `if`

### Paso 2.2 -- Compilar y ejecutar normalmente

```bash
gcc -g -O0 invalid_access.c -o invalid_access
./invalid_access
```

El programa probablemente ejecuta sin errores visibles. Puede imprimir un
valor basura para `arr[5]` y `data[2]`, pero no se cuelga. Este es el
peligro de los accesos invalidos: el programa "funciona" pero tiene
comportamiento indefinido.

### Paso 2.3 -- Ejecutar con Valgrind

```bash
valgrind ./invalid_access
```

Salida esperada (extracto):

```
~PID~ Invalid read of size 4
~PID~    at 0x...: heap_overflow_read (invalid_access.c:13)
~PID~    by 0x...: main (invalid_access.c:46)
~PID~  Address 0x... is 0 bytes after a block of size 20 alloc'd
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: heap_overflow_read (invalid_access.c:5)
~PID~    by 0x...: main (invalid_access.c:46)
```

```
~PID~ Invalid write of size 4
~PID~    at 0x...: heap_overflow_write (invalid_access.c:24)
~PID~    by 0x...: main (invalid_access.c:47)
~PID~  Address 0x... is 0 bytes after a block of size 12 alloc'd
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: heap_overflow_write (invalid_access.c:19)
~PID~    by 0x...: main (invalid_access.c:47)
```

```
~PID~ Conditional jump or move depends on uninitialised value(s)
~PID~    at 0x...: uninitialized_read (invalid_access.c:37)
~PID~    by 0x...: main (invalid_access.c:48)
```

### Paso 2.4 -- Interpretar los mensajes

**"Invalid read of size 4"**:

- Se leyo un `int` (4 bytes) fuera de los limites
- Linea 13 de `invalid_access.c`: es `arr[5]`

**"Address 0x... is 0 bytes after a block of size 20 alloc'd"**:

- El acceso fue a 0 bytes despues del final de un bloque de 20 bytes
- 20 bytes = 5 `int` de 4 bytes = el array que asignamos
- "0 bytes after" significa que leimos justo despues del ultimo byte valido

**"Invalid write of size 4"**:

- Se escribio un `int` fuera de los limites del array de 3 elementos (12 bytes)
- Linea 24: es `arr[3] = 4` (el `i <= 3` deberia ser `i < 3`)

**"Conditional jump depends on uninitialised value"**:

- El `if (data[2] > 0)` usa un valor que nunca fue inicializado
- Valgrind detecta que el programa toma decisiones basadas en basura

### Paso 2.5 -- Rastrear el origen de valores no inicializados

```bash
valgrind --track-origins=yes ./invalid_access
```

Salida adicional esperada:

```
~PID~ Conditional jump or move depends on uninitialised value(s)
~PID~    at 0x...: uninitialized_read (invalid_access.c:37)
~PID~    by 0x...: main (invalid_access.c:48)
~PID~  Uninitialised value was created by a heap allocation
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: uninitialized_read (invalid_access.c:32)
~PID~    by 0x...: main (invalid_access.c:48)
```

Con `--track-origins=yes`, Valgrind muestra donde se creo la memoria no
inicializada: el `malloc()` de la linea 32. Esto confirma que `data[2]` nunca
recibio un valor despues de la asignacion.

Sin esta flag, Valgrind solo dice que el valor no esta inicializado, pero no
dice donde se asigno la memoria. `--track-origins=yes` usa mas memoria y es
mas lento, pero cuando el origen del error no es obvio, es indispensable.

### Limpieza de la parte 2

```bash
rm -f invalid_access
```

---

## Parte 3 -- Use-after-free y double free

**Objetivo**: Detectar uso de memoria despues de liberarla y doble
liberacion. Entender las tres stack traces que muestra Valgrind para estos
errores.

### Paso 3.1 -- Examinar el codigo fuente

```bash
cat use_after_free.c
```

Observa:

- `use_after_free_example()`: llama a `free(name)` y luego usa `name` en
  un `printf()`
- `double_free_example()`: llama a `free(data)` dos veces

### Paso 3.2 -- Compilar y ejecutar normalmente

```bash
gcc -g -O0 use_after_free.c -o use_after_free
./use_after_free
```

Antes de ejecutar, predice:

- El use-after-free producira un segfault?
- El double free producira un crash?

Resultado probable: el use-after-free imprime la cadena correctamente (la
memoria aun no fue sobreescrita). El double free puede o no causar un crash
dependiendo de la implementacion de `malloc()` de tu sistema. Este es
exactamente el problema: estos errores son silenciosos e impredecibles.

### Paso 3.3 -- Ejecutar con Valgrind

```bash
valgrind ./use_after_free
```

Salida esperada para use-after-free (extracto):

```
~PID~ Invalid read of size 1
~PID~    at 0x...: strlen (...)
~PID~    by 0x...: printf (...)
~PID~    by 0x...: use_after_free_example (use_after_free.c:14)
~PID~    by 0x...: main (use_after_free.c:31)
~PID~  Address 0x... is 0 bytes inside a block of size 32 free'd
~PID~    at 0x...: free (...)
~PID~    by 0x...: use_after_free_example (use_after_free.c:12)
~PID~    by 0x...: main (use_after_free.c:31)
~PID~  Block was alloc'd at
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: use_after_free_example (use_after_free.c:6)
~PID~    by 0x...: main (use_after_free.c:31)
```

Salida esperada para double free:

```
~PID~ Invalid free() / delete / delete[] / realloc()
~PID~    at 0x...: free (...)
~PID~    by 0x...: double_free_example (use_after_free.c:27)
~PID~    by 0x...: main (use_after_free.c:32)
~PID~  Address 0x... is 0 bytes inside a block of size 40 free'd
~PID~    at 0x...: free (...)
~PID~    by 0x...: double_free_example (use_after_free.c:25)
~PID~    by 0x...: main (use_after_free.c:32)
~PID~  Block was alloc'd at
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: double_free_example (use_after_free.c:19)
~PID~    by 0x...: main (use_after_free.c:32)
```

### Paso 3.4 -- Las tres stack traces

Valgrind muestra tres stack traces para use-after-free y double free. Cada
una responde a una pregunta diferente:

| Stack trace | Pregunta que responde |
|-------------|----------------------|
| Primera | Donde ocurrio el error? (la lectura invalida o el free invalido) |
| Segunda ("free'd at") | Donde se libero la memoria la primera vez? |
| Tercera ("alloc'd at") | Donde se asigno la memoria originalmente? |

Con estas tres piezas de informacion puedes reconstruir la secuencia
completa: asignacion -> liberacion -> uso indebido.

**Por que "el programa parece funcionar sin Valgrind"**: despues de `free()`,
la memoria no se borra ni se devuelve inmediatamente al sistema operativo. El
bloque queda marcado como libre en el heap del proceso, pero los bytes siguen
ahi. Por eso, leer despues de free suele devolver el valor original -- hasta
que otro `malloc()` reutilice ese espacio y lo sobreescriba. Esto crea bugs
que aparecen y desaparecen de forma aparentemente aleatoria.

### Limpieza de la parte 3

```bash
rm -f use_after_free
```

---

## Parte 4 -- Encontrar todos los bugs

**Objetivo**: Analizar un programa con multiples errores combinados. Primero
manualmente, luego con Valgrind.

### Paso 4.1 -- Leer el codigo fuente

```bash
cat buggy.c
```

Lee el codigo completo con atencion. Hay cuatro funciones ademas de `main()`:
`register_user()`, `format_id()`, `sum_array()` y `process_data()`.

Antes de usar Valgrind, intenta encontrar todos los bugs manualmente. Para
cada funcion, preguntate:

- Se libera toda la memoria asignada?
- Los accesos a arrays respetan los limites?
- Se inicializan todos los valores antes de leerlos?
- Se libera algun bloque mas de una vez?

Anota cuantos bugs encontraste y en que lineas.

### Paso 4.2 -- Compilar

```bash
gcc -g -O0 buggy.c -o buggy
```

### Paso 4.3 -- Ejecutar con Valgrind

```bash
valgrind --leak-check=full --track-origins=yes ./buggy
```

Salida esperada (extracto de los errores principales):

```
~PID~ Invalid write of size 1
~PID~    at 0x...: sprintf (...)
~PID~    by 0x...: format_id (buggy.c:17)
~PID~    by 0x...: main (buggy.c:51)
~PID~  Address 0x... is 0 bytes after a block of size 7 alloc'd
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: format_id (buggy.c:15)
~PID~    by 0x...: main (buggy.c:51)
```

```
~PID~ Conditional jump or move depends on uninitialised value(s)
~PID~    at 0x...: sum_array (buggy.c:33)
~PID~    by 0x...: main (buggy.c:52)
~PID~  Uninitialised value was created by a heap allocation
~PID~    at 0x...: malloc (...)
~PID~    by 0x...: sum_array (buggy.c:26)
~PID~    by 0x...: main (buggy.c:52)
```

```
~PID~ Invalid free() / delete / delete[] / realloc()
~PID~    at 0x...: free (...)
~PID~    by 0x...: process_data (buggy.c:47)
~PID~    by 0x...: main (buggy.c:53)
~PID~  Address 0x... is 0 bytes inside a block of size 80 free'd
~PID~    at 0x...: free (...)
~PID~    by 0x...: process_data (buggy.c:45)
~PID~    by 0x...: main (buggy.c:53)
```

```
~PID~ LEAK SUMMARY:
~PID~    definitely lost: 128 bytes in 2 blocks
~PID~    ...
```

### Paso 4.4 -- Inventario de bugs

Compara tu analisis manual con lo que encontro Valgrind:

| # | Tipo | Funcion | Linea | Descripcion |
|---|------|---------|-------|-------------|
| 1 | Memory leak | `register_user()` | 8 | `malloc(64)` sin `free()`. Se llama 2 veces = 128 bytes perdidos |
| 2 | Invalid write | `format_id()` | 17 | `sprintf()` escribe 8 bytes ("ID-1000\0") en un buffer de 7 |
| 3 | Uninitialized read | `sum_array()` | 33 | Lee `arr[1]`, `arr[3]`, `arr[5]` que nunca fueron inicializados |
| 4 | Double free | `process_data()` | 47 | `free(buffer)` se llama dos veces |

Si encontraste los 4 en tu analisis manual, excelente. Si Valgrind encontro
alguno que no viste, revisa el codigo de esa funcion para entender por que se
te escapo.

### Paso 4.5 -- Como se corregiria cada bug

Para cada error, el fix es concreto:

1. **Leak en `register_user()`**: agregar `free(record)` antes de retornar,
   o retornar el puntero para que el llamador lo libere.

2. **Invalid write en `format_id()`**: aumentar el buffer a `malloc(8)` o
   mejor, calcular el tamano necesario. Usar `snprintf()` en vez de
   `sprintf()` para limitar la escritura.

3. **Uninitialized read en `sum_array()`**: inicializar todos los elementos
   del array, por ejemplo con `memset(arr, 0, count * sizeof(int))` despues
   del `malloc()`, o cambiar el loop a `i++` en vez de `i += 2`.

4. **Double free en `process_data()`**: eliminar el segundo `free(buffer)`,
   o asignar `buffer = NULL` despues del primer free (asi el segundo free es
   inofensivo: `free(NULL)` no hace nada).

### Limpieza de la parte 4

```bash
rm -f buggy
```

---

## Limpieza final

```bash
rm -f leak invalid_access use_after_free buggy
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  buggy.c  invalid_access.c  leak.c  use_after_free.c
```

---

## Conceptos reforzados

1. Compilar con `-g -O0` es necesario para que Valgrind muestre numeros de
   linea precisos. Sin `-g`, solo veras direcciones hexadecimales. Con
   optimizacion (`-O2`), el compilador puede reordenar o eliminar codigo,
   haciendo que los numeros de linea no correspondan al fuente.

2. `valgrind --leak-check=full` muestra cada bloque perdido con la stack
   trace de donde fue asignado. "definitely lost" significa que ningun
   puntero apunta al bloque al terminar el programa.

3. "Invalid read/write of size N" indica un acceso fuera de los limites de
   un bloque asignado. El mensaje "X bytes after a block of size Y alloc'd"
   dice exactamente cuanto te pasaste del limite y cual era el tamano
   original del bloque.

4. `--track-origins=yes` muestra donde se creo la memoria no inicializada
   (el `malloc()` original), no solo donde se uso. Es mas lento pero
   esencial cuando el origen no es obvio.

5. Para use-after-free y double free, Valgrind muestra tres stack traces:
   donde ocurrio el error, donde se libero la memoria, y donde se asigno.
   Estas tres piezas reconstruyen la secuencia completa del bug.

6. Los errores de memoria son silenciosos: un programa puede "funcionar"
   correctamente durante anos con un use-after-free o un buffer overflow,
   hasta que un cambio en el layout de memoria del heap lo haga fallar.
   Valgrind los encuentra de forma determinista independientemente del layout.
