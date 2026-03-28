# Lab — malloc, calloc, realloc, free

## Objetivo

Practicar la alocacion y liberacion de memoria dinamica en C usando las cuatro
funciones fundamentales de `<stdlib.h>`. Al finalizar, sabras alocar memoria con
`malloc` y `calloc`, redimensionar con `realloc` usando el patron seguro
(puntero temporal), liberar con `free`, y verificar en `/proc/self/maps` donde
vive el heap en el mapa de memoria del proceso.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `malloc_basic.c` | Alocar un int, un array, verificar NULL, free |
| `calloc_vs_malloc.c` | Comparar malloc (basura) vs calloc (zero-initialized) |
| `realloc_grow.c` | Crecer un array dinamico con realloc, patron seguro |
| `heap_map.c` | Direcciones de heap, stack, globals y /proc/self/maps |

---

## Parte 1 — malloc y free

**Objetivo**: Alocar memoria con `malloc`, verificar el retorno, usar la
memoria, y liberarla con `free`.

### Paso 1.1 — Examinar el codigo fuente

```bash
cat malloc_basic.c
```

Observa:

- Se aloca un `int` con `malloc(sizeof(*p))` — el patron recomendado usa
  `sizeof(*variable)` en vez de `sizeof(int)`
- Se verifica que el retorno no sea `NULL`
- No se castea el retorno de `malloc` — en C, `void *` se convierte
  implicitamente al tipo destino
- Se libera con `free(p)` y se asigna `p = NULL` despues

Antes de compilar, predice:

- Que direccion de memoria tendra `p`? Sera una direccion en el stack o en otra
  region?
- El array de 5 ints, que valores contendra?

### Paso 1.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic malloc_basic.c -o malloc_basic
./malloc_basic
```

Salida esperada:

```
Value: 42
Address: 0x<direccion_en_heap>
Array: 10 20 30 40 50
```

La direccion de `p` es una direccion en el heap (tipicamente en la zona baja del
espacio de memoria virtual, muy diferente a las direcciones del stack). El array
contiene los valores que asignamos en el ciclo `for`.

### Paso 1.3 — Que pasa si no verificas NULL

Si `malloc` falla (por ejemplo, si pides mas memoria de la disponible), retorna
`NULL`. Sin verificar, harias `*NULL = 42` — lo cual es un segfault.

Para este lab no podemos provocar una falla de `malloc` facilmente, pero el
patron de verificacion es obligatorio en codigo profesional:

```c
int *p = malloc(sizeof(*p));
if (p == NULL) {
    perror("malloc");
    return 1;
}
```

### Paso 1.4 — Por que no castear malloc en C

En C, `void *` se convierte implicitamente a cualquier puntero a objeto. Si
olvidas `#include <stdlib.h>`, el compilador asume que `malloc` retorna `int`
(declaracion implicita). Sin cast, obtienes un warning. Con cast, el warning
desaparece pero tienes comportamiento indefinido.

Regla: en C, **nunca** castees el retorno de `malloc`. El compilador te
advertira si olvidaste el `#include`.

(En C++, el cast es obligatorio porque C++ no permite conversion implicita de
`void *`. Pero en C++ se usa `new` en vez de `malloc`.)

### Limpieza de la parte 1

```bash
rm -f malloc_basic
```

---

## Parte 2 — calloc vs malloc

**Objetivo**: Observar la diferencia entre `malloc` (memoria sin inicializar) y
`calloc` (memoria inicializada a cero).

### Paso 2.1 — Examinar el codigo fuente

```bash
cat calloc_vs_malloc.c
```

Observa:

- `malloc` aloca `n * sizeof(*m)` bytes sin inicializar
- `calloc` aloca `n` elementos de `sizeof(*c)` bytes, todos a cero
- `calloc` toma dos argumentos (cantidad, tamano) — esto le permite verificar
  internamente si la multiplicacion haria overflow

Antes de compilar, predice:

- Los valores de `m[0]..m[4]` (malloc) seran cero o basura?
- Los valores de `c[0]..c[4]` (calloc) seran cero o basura?

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic calloc_vs_malloc.c -o calloc_vs_malloc
./calloc_vs_malloc
```

Salida esperada:

```
=== malloc (uninitialized) ===
m[0] = <valor_indeterminado>
m[1] = <valor_indeterminado>
m[2] = <valor_indeterminado>
m[3] = <valor_indeterminado>
m[4] = <valor_indeterminado>

=== calloc (zero-initialized) ===
c[0] = 0
c[1] = 0
c[2] = 0
c[3] = 0
c[4] = 0
```

**Nota importante**: En la practica, es posible que `malloc` tambien muestre
ceros. Esto ocurre porque el sistema operativo entrega paginas de memoria ya
limpias (por seguridad, para no exponer datos de otros procesos). Pero esto no
esta garantizado por el estandar de C — en un programa mas complejo que aloca
y libera repetidamente, `malloc` retornaria memoria con residuos de usos
anteriores. La unica garantia de inicializacion a cero es `calloc`.

### Paso 2.3 — Cuando usar cada una

Resumen:

| Funcion | Inicializa | Overflow check | Usar cuando... |
|---------|-----------|----------------|----------------|
| `malloc` | No | No | Vas a llenar la memoria inmediatamente |
| `calloc` | Si (a cero) | Si (interno) | Necesitas ceros o la multiplicacion podria desbordar |

### Limpieza de la parte 2

```bash
rm -f calloc_vs_malloc
```

---

## Parte 3 — realloc y crecimiento dinamico

**Objetivo**: Crecer un array dinamico con `realloc` usando el patron seguro
(puntero temporal), y observar cuando el bloque se mueve de direccion.

### Paso 3.1 — Examinar el codigo fuente

```bash
cat realloc_grow.c
```

Observa:

- Se aloca un array con capacidad inicial de 4 elementos
- Cuando `len == cap`, se duplica la capacidad con `realloc`
- Se usa un puntero temporal `tmp` para capturar el retorno de `realloc`
- Si `realloc` falla, `arr` sigue siendo valido y se libera explicitamente
- Se guarda la direccion original para detectar si el bloque fue movido

Antes de compilar, predice:

- Cuantas veces se hara `realloc` para insertar 12 elementos con capacidad
  inicial 4?
- Cuando la capacidad crece de 4 a 8, el bloque se movera a otra direccion o
  se extendera in-place?

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic realloc_grow.c -o realloc_grow
./realloc_grow
```

Salida esperada (las direcciones varian):

```
Initial: cap=4, len=0, addr=0x<addr1>
  Growing: cap 4 -> 8
  Block MOVED: 0x<addr1> -> 0x<addr2>
  Growing: cap 8 -> 16
  Block extended in place: 0x<addr2>

Final: cap=16, len=12
Contents: 100 200 300 400 500 600 700 800 900 1000 1100 1200
```

Se hizo `realloc` 2 veces: de 4 a 8, y de 8 a 16. Con 16 slots de capacidad,
los 12 elementos caben sin mas alocaciones.

Observa que la primera vez el bloque se movio (no habia espacio contiguo
despues del bloque original) y la segunda vez se extendio in-place (habia
espacio suficiente). Esto depende del estado del heap — en tu sistema podria
ser diferente.

### Paso 3.3 — El error clasico de realloc

El error mas comun con `realloc` es:

```c
arr = realloc(arr, new_size);    // MAL
```

Si `realloc` falla, retorna `NULL` y `arr` se sobreescribe con `NULL`. La
memoria original se perdio — **memory leak**. El patron correcto es siempre:

```c
int *tmp = realloc(arr, new_size);
if (tmp == NULL) {
    perror("realloc");
    free(arr);       // liberar la memoria original
    return 1;
}
arr = tmp;           // solo actualizar si tuvo exito
```

### Paso 3.4 — Casos especiales de realloc

Tres casos que debes conocer:

1. `realloc(NULL, size)` — equivale a `malloc(size)`
2. `realloc(ptr, 0)` — comportamiento definido por la implementacion. En C17,
   puede retornar `NULL` o un puntero. NO depender de este caso — usar
   `free()` explicitamente.
3. Si `realloc` tiene exito, el puntero original ya no es valido (incluso si
   la direccion no cambio conceptualmente, solo `tmp` es valido).

### Limpieza de la parte 3

```bash
rm -f realloc_grow
```

---

## Parte 4 — Mapa de memoria del proceso

**Objetivo**: Ver donde vive el heap en el mapa de memoria del proceso, y
comparar las direcciones del heap, stack, .data y .bss.

### Paso 4.1 — Examinar el codigo fuente

```bash
cat heap_map.c
```

Observa:

- `global_var` (inicializado) va en `.data`
- `global_zero` (sin inicializar) va en `.bss`
- `literal` (string constante) va en `.rodata`
- `heap_var` (malloc) va en el heap
- `stack_var` (variable local) va en el stack

Antes de compilar, predice:

- Las direcciones del heap seran mayores o menores que las del stack?
- `.data` y `.bss` estaran cerca o lejos entre si?
- `.rodata` estara cerca de `.data` o de `.text` (codigo)?

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic heap_map.c -o heap_map
./heap_map
```

Salida esperada (las direcciones varian):

```
=== Memory addresses ===
.rodata (literal):   0x<addr_baja>
.data  (global_var): 0x<addr_baja>
.bss   (global_zero):0x<addr_baja>
Heap   (heap_var):   0x<addr_media>
Stack  (stack_var):  0x7f<addr_alta>
```

Observa:

- Las direcciones del stack estan en la zona alta (tipicamente `0x7f...`)
- Las direcciones del heap estan mucho mas abajo
- `.rodata`, `.data` y `.bss` estan en la zona baja, cerca entre si
- Hay un gran espacio vacio entre el heap y el stack — ahi crecen ambos

### Paso 4.3 — Verificar con /proc/self/maps

Cuando el programa te pida que presiones Enter, abre otra terminal y ejecuta:

```bash
cat /proc/<PID>/maps
```

(Reemplaza `<PID>` con el PID que muestra el programa.)

O simplemente presiona Enter y el programa mostrara las lineas de `[heap]` y
`[stack]` del mapa.

Salida esperada:

```
<addr_inicio>-<addr_fin> rw-p ... [heap]
<addr_inicio>-<addr_fin> rw-p ... [stack]
```

Las columnas son: rango de direcciones, permisos (rw-p = read/write/private),
offset, dispositivo, inodo, y nombre de la region.

### Paso 4.4 — Comparar con pmap

Tambien puedes usar `pmap` para ver el mapa de memoria con un formato mas
legible. Con el programa todavia ejecutandose (antes de presionar Enter),
desde otra terminal:

```bash
pmap <PID>
```

Esto muestra el tamano de cada region. Busca la region `[heap]` — su tamano
crece a medida que haces mas `malloc`.

### Paso 4.5 — El modelo de memoria

El mapa confirma el diagrama de la teoria:

```
Direcciones altas:   Stack    (crece hacia abajo)
                     ...
                     ...
Direcciones medias:  Heap     (crece hacia arriba)
Direcciones bajas:   .bss, .data, .rodata, .text
```

El heap y el stack crecen en direcciones opuestas. Si alguno crece demasiado
(por ejemplo, recursion infinita en el stack o malloc sin free en el heap),
eventualmente se agotan los recursos.

### Limpieza de la parte 4

```bash
rm -f heap_map
```

---

## Limpieza final

```bash
rm -f malloc_basic calloc_vs_malloc realloc_grow heap_map
```

Verifica que solo quedan los archivos fuente originales:

```bash
ls
```

Salida esperada:

```
README.md  calloc_vs_malloc.c  heap_map.c  malloc_basic.c  realloc_grow.c
```

---

## Conceptos reforzados

1. `malloc` aloca memoria sin inicializar en el heap. Siempre verificar que el
   retorno no sea `NULL`. En C, no castear el retorno — `void *` se convierte
   implicitamente.

2. El patron recomendado es `malloc(n * sizeof(*ptr))` — si cambias el tipo del
   puntero, `sizeof` se actualiza automaticamente.

3. `calloc` aloca memoria inicializada a cero y verifica overflow en la
   multiplicacion internamente. Usar `calloc` cuando necesitas ceros o cuando
   la multiplicacion de cantidad por tamano podria desbordar.

4. `realloc` puede mover el bloque a otra direccion. Siempre usar un puntero
   temporal para capturar el retorno — asignar directamente a la variable
   original causa memory leak si `realloc` falla.

5. `free` libera la memoria alocada. Despues de `free(p)`, asignar `p = NULL`
   previene use-after-free. `free(NULL)` es seguro (no-op).

6. El heap vive en una region de memoria virtual entre las secciones de datos
   (`.bss`, `.data`) y el stack. Se puede verificar con `/proc/self/maps` o
   `pmap`. Las direcciones del stack estan en la zona alta y las del heap mucho
   mas abajo.

7. Crecimiento geometrico (duplicar capacidad) en arrays dinamicos minimiza la
   cantidad de llamadas a `realloc`. Con capacidad inicial 4, solo se necesitan
   2 reallocs para almacenar 12 elementos (4 -> 8 -> 16).
