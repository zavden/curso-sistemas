# T02 — Fragmentación de heap

## Qué es la fragmentación

Después de muchas operaciones de malloc/free, el heap puede
tener **huecos** entre bloques alocados. Hay memoria libre
total suficiente, pero no hay un bloque contiguo del tamaño
necesario:

```c
// Estado inicial del heap:
// [████████████████████████████████████]
//  todo libre — 1000 bytes

// Después de 5 mallocs de 200 bytes:
// [AAAA][BBBB][CCCC][DDDD][EEEE]
//  200   200   200   200   200

// Liberar B y D:
// [AAAA][    ][CCCC][    ][EEEE]
//  200   200   200   200   200

// Hay 400 bytes libres, pero en dos huecos de 200.
// malloc(300) FALLA — no hay un bloque contiguo de 300 bytes.
// Esto es fragmentación externa.
```

## Fragmentación externa vs interna

```c
// FRAGMENTACIÓN EXTERNA:
// Memoria libre fragmentada en bloques pequeños no contiguos.
// El total libre es suficiente, pero ningún bloque individual es grande.
//
// [AAAA][  ][BBBB][    ][CCCC][ ][DDDD][  ]
//        10        20         5        10
// 45 bytes libres, pero malloc(30) falla.

// FRAGMENTACIÓN INTERNA:
// El allocator da bloques más grandes de lo pedido
// (por alineación o tamaño mínimo):
//
// malloc(1);    → el allocator da 16 bytes (mínimo)
//                 15 bytes desperdiciados (internos al bloque)
//
// malloc(17);   → el allocator da 32 bytes (alineado a 16)
//                 15 bytes desperdiciados

// glibc malloc tiene un overhead de 8-16 bytes por bloque
// (metadata: tamaño, flags, punteros de free list).
// malloc(1) realmente usa ~32 bytes en el heap.
```

## Por qué importa

```c
// 1. Programas de larga duración:
// Un servidor que corre semanas/meses con muchos malloc/free
// puede acumular fragmentación hasta quedarse sin memoria,
// aunque el uso real sea bajo.

// 2. Sistemas embebidos:
// Con poca RAM, la fragmentación importa más.
// Un microcontrolador con 64 KB no puede permitirse fragmentación.

// 3. Rendimiento:
// El allocator debe buscar bloques libres en la free list.
// Con mucha fragmentación, la búsqueda es más lenta.
// También afecta locality del cache.

// 4. Consumo real de memoria:
// Un programa que aloca/libera muchos bloques pequeños
// puede consumir mucha más memoria de la esperada
// por overhead del allocator y fragmentación.
```

## Cómo funciona el allocator (glibc malloc)

```c
// glibc malloc (ptmalloc2) usa una estructura compleja:

// 1. Cada bloque alocado tiene un HEADER:
// ┌─────────────────┐
// │ prev_size (8B)   │  ← tamaño del bloque anterior (si está free)
// ├─────────────────┤
// │ size | flags (8B)│  ← tamaño de este bloque + flags de estado
// ├─────────────────┤
// │ datos del usuario│  ← lo que retorna malloc
// │ ...              │
// └─────────────────┘
// Overhead: 16 bytes por bloque (en 64-bit).
// Tamaño mínimo: 32 bytes (incluyendo header).

// 2. Bloques libres se organizan en "bins" por tamaño:
// - Fast bins: bloques pequeños (16-80 bytes), LIFO, sin coalescing
// - Small bins: bloques medianos, lista doblemente enlazada
// - Large bins: bloques grandes, ordenados por tamaño
// - Unsorted bin: recién liberados, antes de clasificar

// 3. Para bloques grandes (>128 KB por defecto):
// malloc usa mmap directamente (no el heap).
// free hace munmap → la memoria se devuelve al SO inmediatamente.
// Sin fragmentación, pero mayor overhead de syscall.
```

## Estrategias para reducir fragmentación

### 1. Alocar y liberar en orden (LIFO)

```c
// La fragmentación es mínima si los bloques se liberan
// en orden inverso al que se alocaron:
int *a = malloc(100);
int *b = malloc(200);
int *c = malloc(300);

// Liberar en orden inverso:
free(c);
free(b);
free(a);
// El allocator puede coalescer los bloques libres adyacentes
// en un solo bloque grande.
```

### 2. Usar pools de tamaño fijo

```c
// Si siempre se alocan bloques del mismo tamaño,
// no hay fragmentación externa:

#define POOL_SIZE 1024
#define BLOCK_SIZE 64

struct Pool {
    char memory[POOL_SIZE];
    int used[POOL_SIZE / BLOCK_SIZE];    // 1 = usado, 0 = libre
};

void *pool_alloc(struct Pool *pool) {
    for (int i = 0; i < POOL_SIZE / BLOCK_SIZE; i++) {
        if (!pool->used[i]) {
            pool->used[i] = 1;
            return &pool->memory[i * BLOCK_SIZE];
        }
    }
    return NULL;    // pool lleno
}

void pool_free(struct Pool *pool, void *ptr) {
    int idx = ((char *)ptr - pool->memory) / BLOCK_SIZE;
    pool->used[idx] = 0;
}

// Cada bloque es de exactamente BLOCK_SIZE bytes.
// No hay fragmentación externa.
// Útil para alocar muchos structs del mismo tipo.
```

### 3. Alocar pocos bloques grandes

```c
// MAL — muchas alocaciones pequeñas:
for (int i = 0; i < 1000; i++) {
    items[i] = malloc(sizeof(struct Item));
}
// 1000 bloques × ~48 bytes (32 datos + 16 header) = 48 KB
// Con fragmentación, puede usar mucho más.

// BIEN — una sola alocación:
struct Item *items = malloc(1000 * sizeof(struct Item));
// 1 bloque de ~32016 bytes (datos + 16 header)
// Sin fragmentación interna significativa.
// Acceso secuencial es cache-friendly.
```

### 4. Usar arena allocators

```c
// Un arena aloca un gran bloque y subaloca desde él.
// Se libera todo de una vez — sin fragmentación.
// Ver T02 de esta sección para detalles.
```

## Allocators alternativos

```c
// glibc malloc no es el único allocator disponible:

// jemalloc (FreeBSD, Firefox, Redis):
// - Mejor para programas multithreaded
// - Menos fragmentación con muchos hilos
// - Estadísticas de uso detalladas
// Usar: LD_PRELOAD=/usr/lib/libjemalloc.so ./program

// tcmalloc (Google):
// - Thread-caching malloc
// - Cada hilo tiene un cache local → menos contención
// Usar: LD_PRELOAD=/usr/lib/libtcmalloc.so ./program

// mimalloc (Microsoft):
// - Diseñado para rendimiento
// - Muy bueno en benchmarks
// Usar: LD_PRELOAD=/usr/lib/libmimalloc.so ./program

// Todos son drop-in replacements — reemplazan malloc/free
// sin cambiar el código fuente.
```

## Observar la fragmentación

```c
// malloc_info (glibc) — estadísticas del heap:
#include <malloc.h>

malloc_info(0, stdout);    // imprime XML con estadísticas del heap

// mallopt — configurar el allocator:
#include <malloc.h>

// Cambiar el umbral de mmap (default ~128KB):
mallopt(M_MMAP_THRESHOLD, 64 * 1024);
// Bloques > 64KB usan mmap en vez del heap.

// malloc_trim — devolver memoria al SO:
malloc_trim(0);
// Intenta reducir el heap devolviendo páginas no usadas al SO.
// Útil después de liberar muchos bloques.
```

```bash
# Herramientas para analizar el heap:

# Valgrind Massif — perfil de uso de heap:
valgrind --tool=massif ./program
ms_print massif.out.<pid>
# Muestra un gráfico ASCII del uso de heap en el tiempo.

# Valgrind DHAT — análisis de alocaciones:
valgrind --tool=dhat ./program
# Muestra qué alocaciones son ineficientes.
```

---

## Ejercicios

### Ejercicio 1 — Observar fragmentación

```c
// 1. Alocar 100 bloques de 100 bytes cada uno.
// 2. Liberar los bloques en posiciones pares (0, 2, 4, ...).
// 3. Intentar alocar un bloque de 200 bytes.
//    ¿Funciona? ¿Por qué sí/no?
// 4. Repetir liberando los bloques en orden inverso.
//    ¿Cambia el resultado?
```

### Ejercicio 2 — Comparar overhead

```c
// Comparar el consumo de memoria de:
// 1. Alocar 10000 bloques de 1 byte con malloc(1)
// 2. Alocar 1 bloque de 10000 bytes con malloc(10000)
// Usar /proc/self/status (VmRSS) o mallinfo2()
// para medir el consumo real.
```

### Ejercicio 3 — Pool allocator simple

```c
// Implementar un pool allocator para struct Node { int val; Node *next; }:
// - pool_init(size) — aloca un bloque grande
// - pool_alloc() — retorna el siguiente Node disponible
// - pool_reset() — marca todos como disponibles (sin free individual)
// - pool_destroy() — libera el bloque grande
// Comparar rendimiento vs malloc/free individual para 100000 nodos.
```
