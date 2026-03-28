# T02 — Fragmentación de heap

## Erratas detectadas en el material base

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `README.md` línea 43 | Dice "el allocator da 16 bytes (mínimo)" para `malloc(1)` | El chunk mínimo en glibc 64-bit es 32 bytes (header de 8 + 24 usables). `malloc_usable_size` retorna 24, no 16. Los 16 bytes se refieren al espacio mínimo de datos internos del chunk libre (punteros `fd`/`bk`), no al tamaño usable por el usuario |
| `README.md` línea 182 | Dice "Ver T02 de esta sección para detalles" sobre arena allocators | Este **es** T02. Los arena allocators se cubren en S02_Patrones_de_Gestión, no aquí |

---

## 1. Qué es la fragmentación del heap

Tras muchos ciclos de `malloc`/`free`, el heap acumula **huecos** entre bloques alocados. Puede haber memoria libre total suficiente, pero no un bloque **contiguo** del tamaño necesario:

```c
// Estado inicial: todo libre, 1000 bytes contiguos
// [████████████████████████████████████]

// Después de 5 mallocs de 200 bytes:
// [AAAA][BBBB][CCCC][DDDD][EEEE]

// Liberar B y D:
// [AAAA][    ][CCCC][    ][EEEE]
//        200          200
// 400 bytes libres, pero en dos huecos de 200.
// malloc(300) no encuentra un bloque contiguo → falla
// (en un allocator simple sin sbrk/mmap).
```

La fragmentación es un problema **acumulativo**: un programa que corre minutos puede no notarla, pero un servidor que corre semanas/meses con constantes `malloc`/`free` puede degradarse hasta fallar.

---

## 2. Fragmentación externa

La memoria libre está dispersa en **huecos no contiguos**. Individualmente ninguno es suficientemente grande, aunque su suma sí:

```c
// 20 bloques de 128 bytes:
// [B0][B1][B2][B3][B4]...[B19]

// Liberar los impares (B1, B3, B5, ...):
// [B0][  ][B2][  ][B4][  ]...[B18][  ]
//      128     128     128          128
// 10 huecos × 128 = 1280 bytes libres

// malloc(1280) → NO cabe en ningún hueco individual
// glibc resuelve esto extendiendo el heap con sbrk o usando mmap,
// pero la dirección del bloque grande estará FUERA del rango
// de los bloques originales → los 10 huecos quedan desperdiciados.
```

En glibc, `malloc` casi nunca retorna `NULL` porque puede pedir más memoria al kernel. Pero los huecos siguen ahí, consumiendo espacio de direcciones y empeorando la localidad de caché. En allocators embebidos (sin `sbrk`/`mmap`), `malloc` sí falla.

### El coalescing del allocator

Cuando se liberan dos bloques **adyacentes**, el allocator puede **fusionarlos** (*coalesce*) en un solo bloque más grande:

```c
free(blocks[3]);  // Hueco de 128 bytes
free(blocks[4]);  // Adyacente al anterior → fusión
// Ahora hay un hueco de ~256 bytes (más overhead)
```

Los **fast bins** de glibc (bloques pequeños, ≤80 bytes por defecto) **no** se fusionan inmediatamente — se almacenan en una pila LIFO para reasignación rápida. Solo se consolidan cuando el allocator necesita más espacio o se invoca `malloc_trim()`.

---

## 3. Fragmentación interna

El allocator da bloques **más grandes** de lo pedido por dos razones:

1. **Tamaño mínimo**: en glibc 64-bit, el chunk mínimo es 32 bytes (8 de header + 24 usables). `malloc(1)` retorna un bloque con 24 bytes utilizables → 23 bytes desperdiciados.

2. **Alineación**: los tamaños se redondean al siguiente múltiplo de 8 (con mínimo de 24 usables).

```c
#include <malloc.h>

void *p = malloc(1);
size_t usable = malloc_usable_size(p);
// usable == 24 (en glibc 64-bit moderno)
// Desperdicio: (24 - 1) / 24 = 95.8%

void *q = malloc(256);
size_t usable_q = malloc_usable_size(q);
// usable_q == 264
// Desperdicio: (264 - 256) / 264 = 3.0%
```

Además del espacio usable, cada bloque tiene un **header** de ~8 bytes (el campo `size` con flags) que `malloc_usable_size` no cuenta. Así, `malloc(1)` realmente consume ~32 bytes totales del heap.

### El costo de muchas alocaciones pequeñas

```c
// 1000 × malloc(1):
//   Usable total:  24000 bytes  (24 × 1000)
//   Header total:  ~8000 bytes  (8 × 1000)
//   Real total:    ~32000 bytes
//   Para almacenar 1000 bytes de datos → overhead de 32×

// 1 × malloc(1000):
//   Usable total:  ~1000 bytes
//   Header total:  ~8 bytes
//   Real total:    ~1008 bytes
//   Overhead: ~1.008×
```

---

## 4. Por qué importa la fragmentación

```c
// 1. SERVIDORES DE LARGA DURACIÓN
// Un servidor web que corre meses: cada request aloca y libera
// buffers de distintos tamaños. Fragmentación acumulada →
// RSS crece constantemente aunque el uso lógico sea estable.

// 2. SISTEMAS EMBEBIDOS
// Microcontrolador con 64 KB de RAM. Si malloc(512) falla
// porque la memoria libre está en 50 huecos de 100 bytes,
// el dispositivo se cuelga. Muchos sistemas embebidos
// prohíben malloc/free por completo.

// 3. RENDIMIENTO
// El allocator busca bloques libres en listas enlazadas.
// Más fragmentación → listas más largas → malloc más lento.
// Bloques dispersos → peor localidad de caché → más cache misses.

// 4. CONSUMO REAL vs LÓGICO
// Tu programa "usa" 10 MB de datos, pero el proceso consume
// 40 MB de RSS. La diferencia: overhead de headers, fragmentación
// interna, y huecos de fragmentación externa.
```

---

## 5. Cómo funciona el allocator (glibc ptmalloc2)

glibc usa **ptmalloc2**, un allocator con estructura de chunks y bins:

### Estructura de un chunk alocado

```
┌──────────────────────────┐
│ prev_size (8 bytes)      │ ← Reutilizado por el chunk anterior
├──────────────────────────┤   (si ese chunk está en uso, prev_size
│ size | AMP flags (8 B)   │    lo usa como datos, no como metadata)
├──────────────────────────┤
│ datos del usuario        │ ← Lo que retorna malloc()
│ ...                      │
└──────────────────────────┘

Flags en el campo size (3 bits bajos):
  A = NON_MAIN_ARENA  (chunk de arena secundaria)
  M = IS_MMAPPED      (alocado con mmap, no sbrk)
  P = PREV_INUSE      (chunk anterior está en uso)
```

El overhead efectivo por chunk alocado es **8 bytes** (solo el campo `size`), porque el campo `prev_size` del siguiente chunk es reutilizado como datos del chunk actual.

### Sistema de bins

```c
// Fast bins (≤80 bytes en glibc 64-bit por defecto):
//   - Lista LIFO (pila) por tamaño exacto
//   - NO se fusionan al hacer free → reasignación O(1)
//   - Se consolidan cuando malloc no encuentra espacio

// Small bins (≤512 bytes):
//   - Lista doblemente enlazada por tamaño exacto
//   - Se fusionan con vecinos libres al hacer free

// Large bins (>512 bytes):
//   - Lista doblemente enlazada, ordenada por tamaño
//   - Best-fit: busca el bloque más pequeño que quepa

// Unsorted bin:
//   - Chunks recién liberados antes de clasificarse
//   - malloc los recorre primero por si alguno encaja

// Top chunk (wilderness):
//   - El bloque libre al final del heap
//   - Si ningún bin tiene espacio, se corta del top chunk
//   - Si el top chunk es insuficiente → sbrk() para crecer el heap

// mmap threshold (~128 KB por defecto):
//   - Bloques grandes se alocan con mmap() directamente
//   - free() los devuelve al SO con munmap() → sin fragmentación
//   - Pero cada mmap es una syscall costosa
```

---

## 6. Estrategias: liberación LIFO y tamaños uniformes

### Liberación en orden inverso (LIFO)

```c
int *a = malloc(100);
int *b = malloc(200);
int *c = malloc(300);

// Liberar en orden inverso → el allocator fusiona
// los bloques libres adyacentes con el top chunk:
free(c);  // c está junto al top chunk → se fusiona
free(b);  // b ahora está junto al top chunk → se fusiona
free(a);  // a ahora está junto al top chunk → se fusiona
// Resultado: el heap vuelve a su estado original
```

Cuando la liberación sigue el orden inverso de alocación, los bloques se fusionan progresivamente con el top chunk. No queda fragmentación.

### Tamaños uniformes

```c
// MAL: tamaños mixtos
void *blocks[100];
for (int i = 0; i < 100; i++) {
    // Tamaños entre 16 y 256 → huecos de distintos tamaños
    blocks[i] = malloc(16 + rand() % 240);
}
// Al liberar algunos, los huecos de 50 bytes
// no sirven para un malloc(200).

// BIEN: tamaño uniforme
for (int i = 0; i < 100; i++) {
    blocks[i] = malloc(64);  // Todos iguales
}
// Al liberar cualquiera, el hueco encaja perfectamente
// en cualquier nueva alocación de 64 bytes.
```

Si tu programa necesita objetos de distintos tamaños, puedes **redondear al alza** a un conjunto fijo de "clases de tamaño" (32, 64, 128, 256...). Esto es exactamente lo que hacen allocators como jemalloc internamente.

---

## 7. Estrategias: pool allocator

Un pool allocator pre-aloca un bloque grande y lo subdivide en slots de tamaño fijo:

```c
#define POOL_CAPACITY  1024
#define BLOCK_SIZE     64

struct Pool {
    char memory[POOL_CAPACITY * BLOCK_SIZE];  // Bloque contiguo
    int used[POOL_CAPACITY];                  // Bitmap: 1=usado, 0=libre
};

void pool_init(struct Pool *pool) {
    memset(pool->used, 0, sizeof(pool->used));
}

void *pool_alloc(struct Pool *pool) {
    for (int i = 0; i < POOL_CAPACITY; i++) {  // Búsqueda lineal O(n)
        if (!pool->used[i]) {
            pool->used[i] = 1;
            return &pool->memory[i * BLOCK_SIZE];
        }
    }
    return NULL;  // Pool lleno
}

void pool_free(struct Pool *pool, void *ptr) {
    int idx = (int)((char *)ptr - pool->memory) / BLOCK_SIZE;
    pool->used[idx] = 0;
}
```

### Ventajas

- **Cero fragmentación externa**: todos los slots son del mismo tamaño
- **Sin overhead por bloque**: no hay headers como en `malloc`
- **Localidad de caché**: la memoria es contigua
- **Liberación masiva**: `pool_init()` "libera" todo de una vez

### Limitaciones de la versión simple

- Búsqueda lineal O(n) — se puede mejorar con una **free list** (lista enlazada de slots libres) para O(1)
- Tamaño fijo de slot — no sirve para objetos de tamaño variable
- Capacidad fija — se puede hacer crecer con un array de pools

### Pool con free list (O(1))

```c
struct PoolFast {
    char memory[POOL_CAPACITY * BLOCK_SIZE];
    int next_free;  // Índice del primer slot libre
    // Cada slot libre almacena el índice del siguiente libre
    // en sus primeros sizeof(int) bytes
};

void pool_fast_init(struct PoolFast *pool) {
    for (int i = 0; i < POOL_CAPACITY - 1; i++) {
        *(int *)&pool->memory[i * BLOCK_SIZE] = i + 1;
    }
    *(int *)&pool->memory[(POOL_CAPACITY - 1) * BLOCK_SIZE] = -1;
    pool->next_free = 0;
}

void *pool_fast_alloc(struct PoolFast *pool) {
    if (pool->next_free == -1) return NULL;
    int idx = pool->next_free;
    pool->next_free = *(int *)&pool->memory[idx * BLOCK_SIZE];
    return &pool->memory[idx * BLOCK_SIZE];
}

void pool_fast_free(struct PoolFast *pool, void *ptr) {
    int idx = (int)((char *)ptr - pool->memory) / BLOCK_SIZE;
    *(int *)&pool->memory[idx * BLOCK_SIZE] = pool->next_free;
    pool->next_free = idx;
}
```

---

## 8. Estrategias: batch allocation y realloc

### Batch allocation

En vez de muchas alocaciones individuales, alocar un bloque grande:

```c
// MAL — 1000 alocaciones individuales:
struct Node *nodes[1000];
for (int i = 0; i < 1000; i++) {
    nodes[i] = malloc(sizeof(struct Node));
    // Cada malloc: ~32 bytes mínimo + 8 bytes header = 40 bytes
}
// Total: ~40000 bytes para almacenar ~24000 bytes de datos

// BIEN — una sola alocación:
struct Node *all_nodes = malloc(1000 * sizeof(struct Node));
// Total: ~24008 bytes (datos + 1 header)
// Cada nodo: all_nodes[i]
```

La batch allocation no solo reduce el overhead sino que mejora la **localidad de caché**: los nodos son contiguos en memoria, lo que minimiza cache misses al recorrerlos secuencialmente.

### realloc para crecimiento incremental

`realloc` puede crecer un bloque **in-place** si hay espacio contiguo disponible:

```c
size_t size = 16;
char *buf = malloc(size);

for (int i = 0; i < 4; i++) {
    size *= 2;
    char *tmp = realloc(buf, size);
    if (tmp == NULL) {
        // buf sigue válido con el tamaño anterior
        break;
    }
    if (tmp == buf) {
        // Creció in-place → sin fragmentación adicional
    } else {
        // Se movió → el bloque antiguo fue liberado internamente
    }
    buf = tmp;
}
```

Cuando `realloc` crece in-place, no genera ninguna fragmentación. Incluso cuando mueve, es más eficiente que `malloc` + `memcpy` + `free` manual, porque el allocator puede hacer optimizaciones internas (como fusionar con el bloque siguiente si está libre).

---

## 9. Allocators alternativos y herramientas de diagnóstico

### Allocators alternativos

glibc malloc (ptmalloc2) no es el único allocator. Todos son **drop-in replacements** via `LD_PRELOAD`:

```bash
# jemalloc (usado por FreeBSD, Firefox, Redis):
# - Arenas por hilo → menos contención en multithreaded
# - Clases de tamaño (size classes) → menos fragmentación
# - Estadísticas detalladas con MALLOC_CONF
LD_PRELOAD=/usr/lib/libjemalloc.so ./program

# tcmalloc (Google, usado en Chromium):
# - Thread-caching: cada hilo tiene un caché local
# - Transferencias entre hilos en lotes → menos locks
LD_PRELOAD=/usr/lib/libtcmalloc.so ./program

# mimalloc (Microsoft):
# - Diseñado para rendimiento en benchmarks
# - Páginas dedicadas por clase de tamaño → fragmentación mínima
LD_PRELOAD=/usr/lib/libmimalloc.so ./program
```

No requieren cambiar ni recompilar el código fuente.

### Herramientas de diagnóstico

```c
#include <malloc.h>

// malloc_usable_size: tamaño real utilizable del bloque
void *p = malloc(50);
printf("%zu\n", malloc_usable_size(p));  // e.g. 56

// malloc_info: estadísticas del heap en XML
malloc_info(0, stdout);

// mallopt: configurar el allocator
mallopt(M_MMAP_THRESHOLD, 64 * 1024);  // mmap para bloques >64KB
mallopt(M_TRIM_THRESHOLD, 128 * 1024); // Umbral para trim del heap

// malloc_trim: devolver memoria al SO
malloc_trim(0);
// Intenta reducir el heap devolviendo páginas libres.
// Útil después de liberar muchos bloques.
```

```bash
# Valgrind Massif — perfil de uso de heap a lo largo del tiempo:
valgrind --tool=massif ./program
ms_print massif.out.<pid>
# Genera un gráfico ASCII mostrando picos de uso del heap.

# Valgrind DHAT — análisis de eficiencia de alocaciones:
valgrind --tool=dhat ./program
# Identifica alocaciones ineficientes (corta vida, poco uso, etc.)

# /proc/self/status — consumo desde el kernel:
# VmRSS: memoria residente real del proceso
# VmSize: espacio de direcciones virtual total
```

---

## 10. Comparación con Rust

Rust usa el **mismo allocator** del sistema (por defecto, libc malloc) y sufre la misma fragmentación a nivel de heap. La diferencia está en cómo el lenguaje **estructura** el uso de memoria:

```rust
// Vec<T> usa batch allocation internamente:
let mut v: Vec<i32> = Vec::with_capacity(1000);
// Una sola alocación de 4000 bytes, no 1000 mallocs

// Crecimiento geométrico automático (×2):
for i in 0..2000 {
    v.push(i);  // realloc interno cuando cap se agota
}
// Solo ~11 reallocs (log₂(2000)), no 2000 mallocs

// Box<T> para alocaciones individuales en heap:
let b = Box::new(42);
// Equivalente a malloc(sizeof(int)) + free automático al salir del scope

// El ownership system garantiza:
// - No use-after-free → no dangling pointers
// - Exactamente un free por alocación → no double free
// - Free determinístico al salir del scope → no memory leaks

// Arena allocators en Rust (crate bumpalo):
use bumpalo::Bump;
let arena = Bump::new();
let x = arena.alloc(42);        // Alocación O(1), solo avanza un puntero
let y = arena.alloc([0u8; 100]);
// Todo se libera al dropear la arena → cero fragmentación
// drop(arena);

// Allocators personalizados (nightly o con GlobalAlloc):
// Rust permite cambiar el allocator global:
// #[global_allocator]
// static ALLOC: jemallocator::Jemalloc = jemallocator::Jemalloc;
```

La ventaja clave de Rust no es evitar la fragmentación (es un problema del allocator, no del lenguaje), sino que `Vec`, `Box` y el ownership system hacen difícil caer en los patrones que la causan (muchos malloc/free individuales, objetos de vida irregular).

---

## Ejercicios

### Ejercicio 1 — Reutilización LIFO de fast bins

```c
// Compila y ejecuta:
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    void *a = malloc(40);
    void *b = malloc(40);
    void *c = malloc(40);
    printf("a=%p  b=%p  c=%p\n", a, b, c);

    free(c);
    free(b);
    free(a);

    void *x = malloc(40);
    void *y = malloc(40);
    void *z = malloc(40);
    printf("x=%p  y=%p  z=%p\n", x, y, z);

    free(x); free(y); free(z);
    return 0;
}
```

**Predicción**: ¿Las direcciones de `x`, `y`, `z` coincidirán con las de `a`, `b`, `c`? ¿En qué orden?

<details><summary>Respuesta</summary>

`x == a`, `y == b`, `z == c`. Los bloques de 40 bytes caen en fast bins, que son LIFO (pila). Se liberaron en orden `c, b, a`, así que se reasignan en orden `a, b, c` (pop de la pila). Las direcciones serán idénticas.

</details>

### Ejercicio 2 — Fragmentación externa observable

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    void *blocks[10];
    for (int i = 0; i < 10; i++) {
        blocks[i] = malloc(100);
    }

    // Liberar pares: 0, 2, 4, 6, 8
    for (int i = 0; i < 10; i += 2) {
        free(blocks[i]);
        blocks[i] = NULL;
    }

    // Intentar alocar un bloque que ocupa todo lo liberado
    void *big = malloc(500);
    printf("big=%p\n", big);
    printf("blocks[1]=%p\n", blocks[1]);
    printf("blocks[9]=%p\n", blocks[9]);

    free(big);
    for (int i = 1; i < 10; i += 2) free(blocks[i]);
    return 0;
}
```

**Predicción**: ¿La dirección de `big` estará entre `blocks[1]` y `blocks[9]`, o fuera de ese rango?

<details><summary>Respuesta</summary>

Estará **fuera** del rango. Los 5 huecos de ~100 bytes no son contiguos (están intercalados con los bloques impares que siguen alocados). glibc no puede fusionarlos y debe buscar/crear espacio nuevo, ya sea cortando del top chunk o extendiendo el heap con `sbrk`. La dirección de `big` será mayor que `blocks[9]`.

</details>

### Ejercicio 3 — Medir fragmentación interna con malloc_usable_size

```c
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

int main(void) {
    size_t sizes[] = {1, 8, 24, 25, 100, 1000};
    int n = sizeof(sizes) / sizeof(sizes[0]);

    for (int i = 0; i < n; i++) {
        void *p = malloc(sizes[i]);
        size_t usable = malloc_usable_size(p);
        printf("malloc(%4zu) → usable=%zu  waste=%.1f%%\n",
               sizes[i], usable,
               (double)(usable - sizes[i]) / (double)usable * 100.0);
        free(p);
    }
    return 0;
}
```

**Predicción**: ¿Cuál será el valor de `malloc_usable_size` para `malloc(1)`? ¿Y para `malloc(24)`? ¿Y para `malloc(25)`?

<details><summary>Respuesta</summary>

- `malloc(1)` → usable = 24 (mínimo del allocator). Waste: 95.8%.
- `malloc(24)` → usable = 24 (encaja exactamente en el mínimo). Waste: 0%.
- `malloc(25)` → usable = 40 (siguiente clase de tamaño, múltiplo de 8 con overhead). Waste: 37.5%.

Los tamaños usables típicos en glibc 64-bit son: 24, 40, 56, 72, 88, 104, 120, 136... (24 + n×16 para fast bins). Los valores exactos pueden variar por versión de glibc.

</details>

### Ejercicio 4 — Costo total: muchos malloc(1) vs uno grande

```c
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

int main(void) {
    int n = 500;
    void *blocks[500];
    size_t total_usable = 0;

    for (int i = 0; i < n; i++) {
        blocks[i] = malloc(1);
        total_usable += malloc_usable_size(blocks[i]);
    }

    void *big = malloc(500);
    size_t big_usable = malloc_usable_size(big);

    printf("500 × malloc(1): usable total = %zu bytes\n", total_usable);
    printf("  1 × malloc(500): usable total = %zu bytes\n", big_usable);
    printf("Factor de overhead: %.1fx\n",
           (double)total_usable / (double)big_usable);

    for (int i = 0; i < n; i++) free(blocks[i]);
    free(big);
    return 0;
}
```

**Predicción**: ¿Cuál será el factor de overhead aproximado?

<details><summary>Respuesta</summary>

500 × `malloc(1)` → 500 × 24 = 12000 bytes usables. `malloc(500)` → ~504 bytes usables. Factor: ~23.8×. Las 500 alocaciones de 1 byte consumen ~24 veces más memoria que una sola alocación de 500 bytes, sin contar los 8 bytes de header por chunk (que sumarían otros 4000 bytes de metadata invisible).

</details>

### Ejercicio 5 — Pool allocator básico

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define POOL_CAP 8
#define SLOT_SIZE 32

struct Pool {
    char mem[POOL_CAP * SLOT_SIZE];
    int used[POOL_CAP];
};

void pool_init(struct Pool *p) { memset(p->used, 0, sizeof(p->used)); }

void *pool_alloc(struct Pool *p) {
    for (int i = 0; i < POOL_CAP; i++) {
        if (!p->used[i]) { p->used[i] = 1; return &p->mem[i * SLOT_SIZE]; }
    }
    return NULL;
}

void pool_free(struct Pool *p, void *ptr) {
    int idx = (int)((char *)ptr - p->mem) / SLOT_SIZE;
    p->used[idx] = 0;
}

int main(void) {
    struct Pool pool;
    pool_init(&pool);

    void *a = pool_alloc(&pool);
    void *b = pool_alloc(&pool);
    void *c = pool_alloc(&pool);

    printf("a=%p b=%p c=%p\n", a, b, c);
    printf("Spacing: %td bytes\n", (char *)b - (char *)a);

    pool_free(&pool, b);
    void *d = pool_alloc(&pool);
    printf("d=%p (should reuse b's slot)\n", d);

    pool_free(&pool, a);
    pool_free(&pool, c);
    pool_free(&pool, d);
    return 0;
}
```

**Predicción**: ¿El spacing entre `a` y `b` será exactamente 32 bytes? ¿`d` tendrá la misma dirección que `b`?

<details><summary>Respuesta</summary>

Sí a ambas. El spacing es exactamente `SLOT_SIZE` = 32 bytes, porque el pool es un array contiguo subdividido aritméticamente. `d` reutiliza el slot de `b` porque `pool_alloc` busca linealmente desde el índice 0 y el slot 1 (de `b`) es el primer libre. No hay overhead de headers ni alineación impuesta por el allocator — el pool gestiona la memoria directamente.

</details>

### Ejercicio 6 — Coalescing: probar fusión de bloques adyacentes

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Alocar 3 bloques contiguos de 200 bytes
    // (200 > 80, así que NO caen en fast bins → se fusionan al liberar)
    void *a = malloc(200);
    void *b = malloc(200);
    void *c = malloc(200);
    void *guard = malloc(200);  // Evitar fusión con top chunk

    printf("a=%p b=%p c=%p\n", a, b, c);

    // Liberar b y c (adyacentes)
    free(b);
    free(c);

    // Intentar alocar un bloque de 400 bytes
    void *big = malloc(400);
    printf("big=%p\n", big);

    free(a);
    free(guard);
    free(big);
    return 0;
}
```

**Predicción**: ¿La dirección de `big` coincidirá con la de `b` (reutilizando los bloques fusionados `b+c`)? ¿O estará en una dirección diferente?

<details><summary>Respuesta</summary>

`big` debería tener la misma dirección que `b`. Al liberar `b` y `c` (ambos >80 bytes, fuera de fast bins), el allocator los fusiona (*coalesce*) en un bloque de ~416+ bytes. Cuando se pide `malloc(400)`, ese bloque fusionado es suficiente y se reutiliza. Si `b` y `c` hubieran sido de 40 bytes (fast bins), no se fusionarían y `big` estaría en otra dirección.

</details>

### Ejercicio 7 — Fast bins NO se fusionan

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    // Bloques pequeños: caen en fast bins (≤80 bytes)
    void *a = malloc(40);
    void *b = malloc(40);
    void *c = malloc(40);
    void *guard = malloc(40);

    printf("a=%p b=%p c=%p\n", a, b, c);

    free(b);
    free(c);
    // b y c están en fast bins → NO se fusionan

    // Intentar alocar un bloque de 80 bytes (b+c juntos)
    void *big = malloc(80);
    printf("big=%p\n", big);

    // ¿big reutiliza el espacio de b+c?
    int reused = ((char *)big >= (char *)b && (char *)big < (char *)c + 40);
    printf("Reused b+c space: %s\n", reused ? "YES" : "NO");

    free(a);
    free(guard);
    free(big);
    return 0;
}
```

**Predicción**: ¿`big` reutilizará el espacio combinado de `b` y `c`?

<details><summary>Respuesta</summary>

**No**. Los bloques de 40 bytes caen en fast bins, que no se fusionan al hacer `free`. `b` y `c` quedan como dos huecos separados de 40 bytes cada uno. `malloc(80)` necesita un bloque contiguo de 80 bytes que no existe en los fast bins, así que el allocator busca en otros bins o extiende el heap. `big` tendrá una dirección fuera del rango de `b`/`c`. Este es un ejemplo directo de fragmentación externa causada por fast bins.

</details>

### Ejercicio 8 — malloc_usable_size y realloc in-place

```c
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>

int main(void) {
    char *buf = malloc(16);
    size_t usable = malloc_usable_size(buf);
    printf("malloc(16): usable=%zu, address=%p\n", usable, (void *)buf);

    // Intentar realloc a un tamaño dentro del espacio usable
    char *buf2 = realloc(buf, usable);
    printf("realloc(%zu): address=%p  %s\n",
           usable, (void *)buf2,
           (buf2 == buf) ? "[in-place]" : "[moved]");

    // Intentar realloc a usable + 1 (excede el espacio disponible)
    char *buf3 = realloc(buf2, usable + 1);
    printf("realloc(%zu): address=%p  %s\n",
           usable + 1, (void *)buf3,
           (buf3 == buf2) ? "[in-place]" : "[moved]");

    free(buf3);
    return 0;
}
```

**Predicción**: ¿El primer realloc será in-place? ¿Y el segundo?

<details><summary>Respuesta</summary>

El primer `realloc(buf, usable)` será **in-place** porque el bloque ya tiene `usable` bytes disponibles — no necesita crecer. El segundo `realloc(buf2, usable + 1)` probablemente se **mueva**, porque excede el espacio usable del chunk actual. Sin embargo, si el siguiente chunk está libre, `realloc` podría fusionarlos y crecer in-place. El resultado depende del estado del heap en ese momento.

</details>

### Ejercicio 9 — Visualizar el patrón ajedrez

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

int main(void) {
    void *blocks[8];
    for (int i = 0; i < 8; i++) {
        blocks[i] = malloc(100);
    }

    // Liberar pares: 0, 2, 4, 6
    for (int i = 0; i < 8; i += 2) {
        free(blocks[i]);
        blocks[i] = NULL;
    }

    // Mapa visual
    uintptr_t base = (uintptr_t)blocks[1];  // Primer bloque aún vivo
    printf("Heap map (relative addresses):\n");
    for (int i = 0; i < 8; i++) {
        if (blocks[i]) {
            int offset = (int)((uintptr_t)blocks[i] - base);
            printf("  [%+5d] Block %d: USED\n", offset, i);
        } else {
            printf("  [     ] Block %d: FREE\n", i);
        }
    }

    // ¿Cuántos malloc(200) caben en los huecos?
    int count = 0;
    for (int attempt = 0; attempt < 4; attempt++) {
        void *p = malloc(200);
        if (p && (uintptr_t)p > base &&
            (uintptr_t)p < (uintptr_t)blocks[7] + 100) {
            count++;
            printf("  malloc(200) at %p → INSIDE original range\n", p);
        } else if (p) {
            printf("  malloc(200) at %p → OUTSIDE original range\n", p);
        }
        free(p);
    }

    for (int i = 1; i < 8; i += 2) free(blocks[i]);
    return 0;
}
```

**Predicción**: ¿Los `malloc(200)` cabrán en los huecos de ~100 bytes?

<details><summary>Respuesta</summary>

**No**. Los huecos de ~100 bytes (más overhead) no son suficientes para un bloque de 200 bytes. Todos los `malloc(200)` se alocarán **fuera** del rango original (más allá de `blocks[7]`), porque ningún hueco individual es lo bastante grande. Los bloques impares (1, 3, 5, 7) siguen intercalados, impidiendo la fusión de huecos adyacentes.

</details>

### Ejercicio 10 — Batch allocation vs individual: impacto real

```c
#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>

struct Record {
    int id;
    char name[24];
    double value;
};

int main(void) {
    int n = 200;

    // Estrategia 1: malloc individual
    struct Record **individual = malloc((size_t)n * sizeof(struct Record *));
    size_t total_individual = 0;
    for (int i = 0; i < n; i++) {
        individual[i] = malloc(sizeof(struct Record));
        total_individual += malloc_usable_size(individual[i]);
    }

    // Estrategia 2: batch allocation
    struct Record *batch = malloc((size_t)n * sizeof(struct Record));
    size_t total_batch = malloc_usable_size(batch);

    size_t logical = (size_t)n * sizeof(struct Record);
    printf("sizeof(Record) = %zu\n", sizeof(struct Record));
    printf("Logical data:   %zu bytes (%d records)\n", logical, n);
    printf("\nIndividual malloc: usable total = %zu bytes (%.1fx logical)\n",
           total_individual, (double)total_individual / (double)logical);
    printf("Batch malloc:      usable total = %zu bytes (%.1fx logical)\n",
           total_batch, (double)total_batch / (double)logical);
    printf("Batch saves:       %zu bytes (%.0f%%)\n",
           total_individual - total_batch,
           (1.0 - (double)total_batch / (double)total_individual) * 100.0);

    for (int i = 0; i < n; i++) free(individual[i]);
    free(individual);
    free(batch);
    return 0;
}
```

**Predicción**: Si `sizeof(struct Record)` es ~40 bytes, ¿cuál será la diferencia de overhead entre las dos estrategias para 200 records?

<details><summary>Respuesta</summary>

`sizeof(struct Record)` = 40 bytes (4 + 24 + 8, con padding a 8). Datos lógicos: 200 × 40 = 8000 bytes.

- **Individual**: `malloc(40)` → usable ~40 bytes (encaja en clase de 40). Total usable: ~8000 bytes. Pero hay 200 headers de 8 bytes = 1600 bytes ocultos. Factor ≈ 1.0× en usable, pero ~1.2× en uso real del heap.
- **Batch**: `malloc(8000)` → usable ~8000 bytes, 1 header de 8 bytes. Factor ≈ 1.0×.

La diferencia real es más notoria con structs más pequeños. Si `sizeof(Record)` fuera 8, individual usaría 200 × 24 (mínimo) = 4800 usable vs batch 1 × ~1600 = 1600 usable → 3× de diferencia. Además, la batch es mucho mejor para el caché.

</details>
