# Lab — Arena allocators

## Objetivo

Implementar un arena allocator desde cero, usarlo para alocar distintos tipos
de datos, practicar el patron reset para reusar memoria sin realocar, y medir
la diferencia de rendimiento frente a malloc/free individual. Al finalizar,
entenderas por que los arenas son el patron preferido en compiladores,
servidores y game engines.

## Prerequisitos

- GCC instalado
- Estar en el directorio `labs/`

## Archivos del laboratorio

| Archivo | Descripcion |
|---------|-------------|
| `arena.h` | Implementacion del arena (header-only: init, alloc, reset, destroy) |
| `arena_basic.c` | Crea una arena, aloca ints/structs/strings, verifica datos |
| `arena_types.c` | Aloca tipos mixtos y observa como avanza el offset |
| `arena_reset.c` | Simula un frame allocator con arena_reset |
| `arena_vs_malloc.c` | Benchmark: arena vs malloc/free en loop |

---

## Parte 1 — Implementar y usar una arena basica

**Objetivo**: Entender la estructura de un arena, como funciona el bump pointer,
y como un solo `free` libera toda la memoria.

### Paso 1.1 — Examinar la implementacion del arena

```bash
cat arena.h
```

Observa la estructura `Arena` con tres campos:

- `base` — puntero al inicio de la memoria (devuelto por `malloc`)
- `offset` — posicion actual del siguiente byte libre (el "bump pointer")
- `cap` — capacidad total de la arena

Observa `arena_alloc`: solo alinea el tamano a 8 bytes, verifica que hay
espacio, y avanza el offset. No hay free lists, no hay busqueda de bloques
libres. Es O(1).

Observa `arena_destroy`: un solo `free(a->base)`. Toda la memoria se libera
de una vez.

### Paso 1.2 — Examinar el programa

```bash
cat arena_basic.c
```

El programa crea una arena de 4096 bytes y aloca tres tipos de datos desde
ella: un array de ints, dos structs Point, y un string. Imprime el offset
despues de cada alocacion para ver como avanza el bump pointer.

Antes de compilar, predice:

- `sizeof(int)` es 4 bytes. Si alocamos 10 ints (40 bytes), y la arena
  alinea a 8 bytes, cual sera el offset? (40 ya es multiplo de 8.)
- `sizeof(struct Point)` con dos `double` sera 16 bytes. Despues de alocar
  2 Points (32 bytes) sobre offset 40, cual sera el offset?
- Si alocamos 64 bytes para el string y la arena alinea a 8, cual sera el
  offset final?

### Paso 1.3 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic arena_basic.c -o arena_basic
./arena_basic
```

Salida esperada:

```
Arena created: cap = 4096 bytes, offset = 0
After allocating 10 ints: offset = 40
After allocating 2 Points: offset = 72
After allocating string: offset = 136

nums: 0 10 20 30 40 50 60 70 80 90
p1 = (1.5, 2.5)
p2 = (3.0, 4.0)
name = arena test string

Total used: 136 / 4096 bytes
Arena destroyed.
```

Verifica tus predicciones:

- 10 ints = 40 bytes, alineado a 8 = 40. Correcto.
- 2 Points = 2 x 16 = 32 bytes. 40 + 32 = 72. Correcto.
- String de 64 bytes, alineado a 8 = 64. 72 + 64 = 136. Correcto.

Observa que todos los datos (ints, structs, string) viven en la misma region
contigua de memoria. Un solo `malloc` al inicio, un solo `free` al final.

### Paso 1.4 — Verificar con valgrind (opcional)

```bash
valgrind --leak-check=full ./arena_basic
```

Salida esperada (fragmento relevante):

```
All heap blocks were freed -- no leaks are possible
```

Un solo `malloc`, un solo `free`. Cero leaks. Esto es la fortaleza del arena:
es practicamente imposible tener memory leaks si siempre llamas
`arena_destroy`.

### Limpieza de la parte 1

```bash
rm -f arena_basic
```

---

## Parte 2 — Tipos mixtos en la arena

**Objetivo**: Observar como el arena aloca tipos de distintos tamanos (ints,
doubles, structs grandes, strings) de forma contigua, y como el alineamiento
a 8 bytes afecta el offset.

### Paso 2.1 — Examinar el programa

```bash
cat arena_types.c
```

Observa la struct `Player` con campos mixtos: un array de chars, dos ints,
y un struct Point anidado. El programa aloca cinco tipos diferentes desde la
misma arena e imprime el offset despues de cada alocacion.

Antes de compilar, predice:

- 5 ints = 20 bytes. Alineado a 8 = 24. Offset despues: 24.
- 3 doubles = 24 bytes. Alineado a 8 = 24. Offset despues: 48.
- `sizeof(struct Player)` con `char[32]` + 2 `int` + `struct Point`(2 doubles).
  Cuanto sera? (Pista: el compilador puede agregar padding.)

### Paso 2.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic arena_types.c -o arena_types
./arena_types
```

Salida esperada:

```
=== Allocating different types from one arena ===

After 5 ints:      offset =  24  (each int = 4 bytes)
After 3 doubles:    offset =  48  (each double = 8 bytes)
After 1 Player:     offset = 104  (Player = 56 bytes)
After string (22):  offset = 128
After 4 Points:    offset = 192

=== Verifying data ===
scores: 100 200 300 400 500
temps: 36.5 37.2 38.1
player: Alice, hp=100, score=0, pos=(10,20)
msg: Hello from the arena!
points: (0,0) (1,2) (2,4) (3,6)

Total used: 192 / 4096 bytes (4.7%)
```

Analisis:

- 5 ints = 20 bytes, pero alineado a 8 ocupa 24. Los 4 bytes extra son
  padding de alineamiento.
- `struct Player` ocupa 56 bytes (el compilador anade padding interno para
  alinear los doubles). Alineado a 8 = 56. Offset: 48 + 56 = 104.
- El string "Hello from the arena!" tiene 22 bytes (21 chars + null), pero
  alineado a 8 ocupa 24. Offset: 104 + 24 = 128.
- Solo usamos 192 de 4096 bytes (4.7%). La arena tiene espacio de sobra.

Punto clave: todos estos tipos diferentes conviven contiguamente en un solo
bloque de memoria. Con malloc individual, cada uno seria una alocacion
separada, potencialmente en regiones distintas del heap.

### Limpieza de la parte 2

```bash
rm -f arena_types
```

---

## Parte 3 — Arena con reset

**Objetivo**: Usar `arena_reset` para reusar la memoria del arena sin liberar
ni realocar. Este es el patron "frame allocator" usado en game engines.

### Paso 3.1 — Examinar el programa

```bash
cat arena_reset.c
```

El programa simula 3 "frames" de un game loop. En cada frame:

1. Llama a `arena_reset` (el offset vuelve a 0).
2. Aloca datos temporales (puntos, un label, ids).
3. Usa los datos.
4. Al terminar el frame, los datos ya no importan — el siguiente reset los
   "borra".

Antes de ejecutar, predice:

- Despues de `arena_reset`, el offset vuelve a 0. Pero, se llama a `free`?
- Los tres frames usan la misma cantidad de datos. El offset al final de
  cada frame, sera el mismo o diferente?
- Al final, cuantas llamadas a `malloc` y `free` hubo en total?

### Paso 3.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic arena_reset.c -o arena_reset
./arena_reset
```

Salida esperada:

```
=== Arena with reset (simulating frame allocator) ===

--- Frame 0 (offset after reset: 0) ---
  label: frame_0_data
  points: (0,0) (1,1) (2,2) (3,3) (4,4)
  ids: 0 1 2 3 4
  offset at end of frame: 136 / 1024 bytes
--- Frame 1 (offset after reset: 0) ---
  label: frame_1_data
  points: (10,100) (11,101) (12,102) (13,103) (14,104)
  ids: 1000 1001 1002 1003 1004
  offset at end of frame: 136 / 1024 bytes
--- Frame 2 (offset after reset: 0) ---
  label: frame_2_data
  points: (20,200) (21,201) (22,202) (23,203) (24,204)
  ids: 2000 2001 2002 2003 2004
  offset at end of frame: 136 / 1024 bytes

=== Summary ===
Processed 3 frames, each reusing the same 1024-byte arena.
Without reset: would need 3 x 132 = 396 bytes total.
With reset: only 1024 bytes needed.
Arena destroyed. Single free for all frames.
```

Verifica tus predicciones:

- `arena_reset` no llama a `free`. Solo pone `offset = 0`. La memoria sigue
  alocada — simplemente se reutiliza.
- Los tres frames terminan con offset 136. Cada frame usa exactamente la
  misma cantidad de memoria porque aloca los mismos tipos y cantidades.
- Total de llamadas al sistema: 1 `malloc` (en `arena_init`) y 1 `free`
  (en `arena_destroy`). Sin importar cuantos frames se procesen.

### Paso 3.3 — Verificar con valgrind (opcional)

```bash
valgrind --leak-check=full ./arena_reset
```

Un solo bloque alocado, un solo bloque liberado. Cero leaks. Si en vez de
arena hubieramos usado malloc/free por cada dato de cada frame, tendriamos
3 x 3 = 9 mallocs y 9 frees (o mas), con riesgo de olvidar algun `free`.

### Limpieza de la parte 3

```bash
rm -f arena_reset
```

---

## Parte 4 — Arena vs malloc/free: comparacion de rendimiento

**Objetivo**: Medir la diferencia de rendimiento entre alocar con arena (un
solo malloc + resets) y alocar con malloc/free individual en un loop.

### Paso 4.1 — Examinar el programa

```bash
cat arena_vs_malloc.c
```

El programa ejecuta dos benchmarks identicos:

1. **malloc/free**: en cada iteracion, aloca 1000 structs individualmente con
   `malloc`, las usa, y las libera con 1000 llamadas a `free`.
2. **Arena**: en cada iteracion, hace `arena_reset` y aloca un bloque contiguo
   para 1000 structs. Al final, un solo `arena_destroy`.

Ambos hacen 100 iteraciones.

Antes de ejecutar, predice:

- malloc/free hara 100 x 1000 = 100,000 mallocs y 100,000 frees.
  Arena hara 1 malloc total. Cual sera mas rapido?
- La diferencia sera 2x? 5x? 10x?

### Paso 4.2 — Compilar y ejecutar

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic arena_vs_malloc.c -o arena_vs_malloc
./arena_vs_malloc
```

Salida esperada (los tiempos varian segun el hardware):

```
=== Arena vs malloc/free performance ===
Iterations: 100, allocations per iteration: 1000
Struct size: 32 bytes

malloc/free:  ~4-8 ms  (1000 mallocs + 1000 frees per iteration)
arena:        ~2-4 ms  (1 alloc + reset per iteration)

malloc/free is ~1.5-3x slower than arena

Total malloc calls:   100000
Total free calls:     100000
Total arena mallocs:  1 (initial) + 0 (resets)
Total arena frees:    1 (destroy)
```

Analisis:

- Arena es mas rapido porque `arena_alloc` solo incrementa un offset (una
  suma y una comparacion). `malloc` debe buscar en free lists, actualizar
  metadata, posiblemente pedir memoria al sistema.
- `arena_reset` es O(1) — pone offset a 0. Liberar 1000 objetos con `free`
  es O(n) — cada free actualiza la free list del allocator.
- La diferencia exacta depende del hardware y del allocator de libc. En
  sistemas con allocators modernos (glibc, jemalloc), malloc es eficiente,
  asi que la diferencia puede ser modesta (~2x). En allocators mas simples
  o con fragmentacion, puede ser mucho mayor.

### Paso 4.3 — Ejecutar varias veces

```bash
./arena_vs_malloc
./arena_vs_malloc
./arena_vs_malloc
```

Observa que los tiempos varian entre ejecuciones. Esto es normal — el
sistema operativo, la cache, y otros procesos afectan las mediciones.
Lo importante es la tendencia: arena consistentemente es mas rapido.

### Limpieza de la parte 4

```bash
rm -f arena_vs_malloc
```

---

## Limpieza final

```bash
rm -f arena_basic arena_types arena_reset arena_vs_malloc
```

Verifica que solo quedan los archivos fuente:

```bash
ls
```

Salida esperada:

```
README.md  arena.h  arena_basic.c  arena_reset.c  arena_types.c  arena_vs_malloc.c
```

---

## Conceptos reforzados

1. Un arena allocator reserva un gran bloque de memoria con un solo `malloc`
   y subaloca desde el secuencialmente avanzando un offset ("bump pointer").
   `arena_alloc` es O(1) — solo una suma y una comparacion.

2. El alineamiento a 8 bytes garantiza que cualquier tipo (int, double,
   puntero, struct) se aloque en una direccion valida. Esto puede agregar
   bytes de padding entre alocaciones.

3. `arena_destroy` libera toda la memoria con un solo `free`. No hay
   posibilidad de memory leaks, use-after-free entre alocaciones del arena,
   ni double free — siempre que se llame a destroy al final.

4. `arena_reset` pone el offset a 0 sin llamar a `free`. La memoria se
   reutiliza en la siguiente ronda de alocaciones. Este es el patron
   "frame allocator" de los game engines.

5. Todos los datos alocados desde un arena son contiguos en memoria. Esto
   mejora la cache locality comparado con malloc individual, donde las
   alocaciones pueden quedar dispersas en el heap.

6. Arena es mas rapido que malloc/free individual en escenarios de muchas
   alocaciones temporales porque elimina el overhead de buscar bloques
   libres, actualizar free lists, y llamar a free por cada objeto.

7. Los arenas son ideales cuando todas las alocaciones tienen el mismo
   lifetime (parsers, request handlers, frames de juegos). No son ideales
   cuando se necesita liberar objetos individuales en momentos diferentes.
