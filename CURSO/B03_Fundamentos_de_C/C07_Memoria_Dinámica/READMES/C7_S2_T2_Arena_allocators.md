# T02 — Arena allocators

## Erratas detectadas en el material base

| Ubicación | Error | Corrección |
|-----------|-------|------------|
| `labs/arena_reset.c` líneas 59-64 y `labs/README.md` línea 257 | El resumen calcula "3 × 132 = 396 bytes" como uso teórico sin reset, usando datos crudos (80 + 32 + 20 = 132). Pero el offset real al final de cada frame es **136** (por el padding de alineamiento a 8 bytes en los 20 bytes de ints → 24). El estudiante ve 136 en la salida pero lee 132 en el resumen | Debería decir "3 × 136 = 408 bytes" para ser consistente con el uso real de la arena, o aclarar que los 132 son sin padding de alineamiento |

---

## 1. Qué es un arena allocator

Un arena (también llamado bump allocator, linear allocator o region-based allocator) pre-aloca un gran bloque de memoria y **subaloca** desde él secuencialmente. Toda la memoria se libera **de una vez**:

```c
// Con malloc/free individual:
int *a = malloc(sizeof(int));     // malloc #1
int *b = malloc(sizeof(int));     // malloc #2
int *c = malloc(sizeof(int));     // malloc #3
// ... usar a, b, c ...
free(a);                           // free #1
free(b);                           // free #2
free(c);                           // free #3
// 3 mallocs + 3 frees + posible fragmentación

// Con arena:
struct Arena arena;
arena_init(&arena, 4096);          // UN solo malloc de 4 KB
int *a = arena_alloc(&arena, sizeof(int));   // solo avanza offset
int *b = arena_alloc(&arena, sizeof(int));   // solo avanza offset
int *c = arena_alloc(&arena, sizeof(int));   // solo avanza offset
// ... usar a, b, c ...
arena_destroy(&arena);             // UN solo free — libera todo
```

### Modelo mental

```
Arena (4096 bytes):
┌──────────────────────────────────────────────────────────┐
│[a:4pad4][b:4pad4][c:4pad4][          libre              ]│
└──────────────────────────────────────────────────────────┘
 ↑                          ↑                               ↑
 base                       offset                          cap

arena_alloc: avanza offset (bump). O(1).
arena_reset: pone offset a 0. O(1). Sin free.
arena_destroy: free(base). O(1). Un solo free.
```

No hay free individual. No hay free lists. No hay búsqueda de bloques. No hay fragmentación.

---

## 2. Implementación: bump pointer con alineamiento

```c
#include <stdlib.h>
#include <stdint.h>

struct Arena {
    char *base;       // Inicio de la memoria (retorno de malloc)
    size_t offset;    // Siguiente byte libre (el "bump pointer")
    size_t cap;       // Capacidad total
};

int arena_init(struct Arena *a, size_t cap) {
    a->base = malloc(cap);
    if (!a->base) return -1;
    a->offset = 0;
    a->cap = cap;
    return 0;
}

void *arena_alloc(struct Arena *a, size_t size) {
    // Alinear a 8 bytes — válido para cualquier tipo fundamental
    size_t aligned = (size + 7) & ~(size_t)7;
    //                ^^^^^^^^^^^^^^^^^^^^^^
    // Ejemplo: size=20 → (20+7)=27 → 27 & ~7 = 27 & 0xFFFFF8 = 24
    // Ejemplo: size=16 → (16+7)=23 → 23 & ~7 = 16 (ya alineado)

    if (a->offset + aligned > a->cap) {
        return NULL;    // Sin espacio
    }

    void *ptr = a->base + a->offset;
    a->offset += aligned;
    return ptr;
}

void arena_reset(struct Arena *a) {
    a->offset = 0;    // "Liberar" todo sin free — la memoria se reutiliza
}

void arena_destroy(struct Arena *a) {
    free(a->base);    // Un solo free para toda la arena
    a->base = NULL;
    a->offset = 0;
    a->cap = 0;
}
```

### Por qué alinear a 8

En arquitecturas de 64 bits, los accesos desalineados a `double` (8 bytes) o punteros (8 bytes) pueden causar penalizaciones de rendimiento o incluso traps en algunas CPUs (ARM, SPARC). Alinear a 8 garantiza que cualquier tipo fundamental se aloque en una dirección válida:

```c
// Sin alineamiento:
// offset=0: int[10] → 40 bytes. offset=40. OK (40 es múltiplo de 8).
// offset=40: double → 8 bytes. offset=48. OK.

// Caso problemático sin alineamiento:
// offset=0: char[5] → 5 bytes. offset=5.
// offset=5: double → dirección base+5 → DESALINEADO (no múltiplo de 8).

// Con alineamiento a 8:
// offset=0: char[5] → alineado a 8. offset=8.
// offset=8: double → dirección base+8 → OK.
```

---

## 3. Uso: alocar sin preocuparse de free individual

```c
struct Point { double x, y; };

int main(void) {
    struct Arena arena;
    arena_init(&arena, 4096);

    // Alocar tipos mixtos — todos conviven contiguamente:
    int *nums = arena_alloc(&arena, 10 * sizeof(int));         // 40 bytes
    struct Point *p1 = arena_alloc(&arena, sizeof(struct Point)); // 16 bytes
    struct Point *p2 = arena_alloc(&arena, sizeof(struct Point)); // 16 bytes
    char *name = arena_alloc(&arena, 64);                       // 64 bytes

    // Usar normalmente — los punteros son válidos hasta arena_destroy:
    for (int i = 0; i < 10; i++) nums[i] = i * 10;
    *p1 = (struct Point){1.5, 2.5};
    *p2 = (struct Point){3.0, 4.0};
    snprintf(name, 64, "arena test string");

    printf("Used: %zu / %zu bytes\n", arena.offset, arena.cap);
    // Used: 136 / 4096 bytes

    arena_destroy(&arena);    // Un solo free — imposible leak
    return 0;
}
```

Con Valgrind: 1 `malloc`, 1 `free`, 0 leaks. Comparado con malloc individual: serían 4 `malloc`, 4 `free`, y 4 oportunidades de olvidar un `free`.

---

## 4. arena_reset: reutilizar sin realocar

`arena_reset` pone el offset a 0 **sin llamar a `free`**. La memoria sigue alocada pero se reutiliza. Este es el patrón **frame allocator** de los game engines:

```c
struct Arena frame_arena;
arena_init(&frame_arena, 1024 * 1024);  // 1 MB

while (game_running) {
    arena_reset(&frame_arena);  // offset = 0, sin free ni malloc

    // Alocar datos temporales del frame actual:
    struct Point *particles = arena_alloc(&frame_arena, 1000 * sizeof(struct Point));
    char *debug_text = arena_alloc(&frame_arena, 256);
    int *ids = arena_alloc(&frame_arena, 100 * sizeof(int));

    // ... procesar frame, renderizar ...

    // Al terminar el frame, todo lo anterior se "descarta"
    // automáticamente en el siguiente arena_reset
}

arena_destroy(&frame_arena);  // Un solo free al cerrar el juego
```

### Costo total de syscalls

| Operación | malloc/free individual | Arena con reset |
|-----------|----------------------|-----------------|
| Creación | N mallocs por frame | 1 malloc total |
| Por frame | N mallocs + N frees | 1 operación (offset = 0) |
| Destrucción | 0 (ya freed) | 1 free total |
| 100 frames × 3 allocs | 300 mallocs + 300 frees | 1 malloc + 1 free |

---

## 5. Arena con crecimiento (growable arena)

La arena simple falla si se agota la capacidad. Una arena con crecimiento aloca **bloques adicionales** encadenados:

```c
struct ArenaBlock {
    char *data;
    size_t cap;
    struct ArenaBlock *next;   // Enlace al bloque anterior
};

struct GrowableArena {
    struct ArenaBlock *current;   // Bloque activo
    size_t offset;
    size_t default_cap;
};

int garena_init(struct GrowableArena *a, size_t cap) {
    a->current = malloc(sizeof(struct ArenaBlock));
    if (!a->current) return -1;
    a->current->data = malloc(cap);
    if (!a->current->data) { free(a->current); return -1; }
    a->current->cap = cap;
    a->current->next = NULL;
    a->offset = 0;
    a->default_cap = cap;
    return 0;
}

void *garena_alloc(struct GrowableArena *a, size_t size) {
    size_t aligned = (size + 7) & ~(size_t)7;

    if (a->offset + aligned > a->current->cap) {
        // Bloque actual lleno → crear uno nuevo
        size_t new_cap = a->default_cap;
        if (aligned > new_cap) new_cap = aligned;  // Petición gigante

        struct ArenaBlock *block = malloc(sizeof(struct ArenaBlock));
        if (!block) return NULL;
        block->data = malloc(new_cap);
        if (!block->data) { free(block); return NULL; }
        block->cap = new_cap;
        block->next = a->current;  // Enlazar al bloque anterior
        a->current = block;
        a->offset = 0;
    }

    void *ptr = a->current->data + a->offset;
    a->offset += aligned;
    return ptr;
}

void garena_destroy(struct GrowableArena *a) {
    struct ArenaBlock *block = a->current;
    while (block) {
        struct ArenaBlock *next = block->next;
        free(block->data);
        free(block);
        block = next;
    }
    a->current = NULL;
}
```

La arena crece automáticamente sin requerir que el usuario estime la capacidad perfecta. Cada bloque se libera al destruir la arena.

---

## 6. Arena helpers: calloc, strdup, sprintf

Funciones útiles construidas sobre `arena_alloc`:

```c
// arena_calloc: alocar e inicializar a cero
void *arena_calloc(struct Arena *a, size_t count, size_t size) {
    size_t total = count * size;
    // Nota: no hay protección contra overflow de count*size
    // (en producción, verificar que total/count == size)
    void *ptr = arena_alloc(a, total);
    if (ptr) memset(ptr, 0, total);
    return ptr;
}

// arena_strdup: duplicar string en la arena
char *arena_strdup(struct Arena *a, const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = arena_alloc(a, len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

// arena_sprintf: sprintf que aloca en la arena
#include <stdarg.h>

char *arena_sprintf(struct Arena *a, const char *fmt, ...) {
    va_list args, args_copy;
    va_start(args, fmt);
    va_copy(args_copy, args);

    // Paso 1: medir cuántos bytes se necesitan
    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    if (len < 0) { va_end(args_copy); return NULL; }

    // Paso 2: alocar en la arena y escribir
    char *buf = arena_alloc(a, (size_t)len + 1);
    if (buf) vsnprintf(buf, (size_t)len + 1, fmt, args_copy);
    va_end(args_copy);

    return buf;
}
```

Estos helpers eliminan la necesidad de `free` individual para cada string o buffer — todo se libera con `arena_destroy`.

---

## 7. Ventajas del arena

### Rendimiento

```c
// arena_alloc es O(1): una suma + una comparación
// malloc es amortizado O(1) para fast bins, pero O(n) en worst case

// Benchmark típico (100 iteraciones × 1000 alocaciones):
// malloc/free:  ~5 ms  (100,000 mallocs + 100,000 frees)
// arena:        ~2 ms  (1 malloc + 100 resets + 1 free)
// Arena ~2-3× más rápido
```

### Seguridad de memoria

```c
// Con arena, es IMPOSIBLE tener (dentro de la vida del arena):
// - Memory leaks       → arena_destroy libera todo
// - Double free        → no hay free individual
// - Free de lo incorrecto → no hay free individual
// - Fragmentación      → alocación contigua

// El ÚNICO riesgo: use-after-destroy (usar punteros del arena
// después de arena_destroy). Pero es un solo punto de control.
```

### Localidad de caché

```c
// malloc individual: cada alocación puede estar en regiones
// distintas del heap → cache misses al acceder secuencialmente.

// Arena: todos los datos son CONTIGUOS en memoria.
// Acceso secuencial → excelente cache performance.
// Los datos caben en las mismas líneas de caché.
```

---

## 8. Cuándo usar (y cuándo no) arenas

### Ideal para

```c
// 1. PARSERS Y COMPILADORES
// Cada archivo/translation unit se procesa en una arena.
// Al terminar, se destruye toda la arena con el AST.
struct Arena ast_arena;
arena_init(&ast_arena, 1024 * 1024);
// ... parsear, crear nodos del AST ...
// ... análisis semántico, codegen ...
arena_destroy(&ast_arena);

// 2. REQUEST HANDLERS EN SERVIDORES
// Cada request usa una arena temporal.
void handle_request(Request *req) {
    struct Arena temp;
    arena_init(&temp, 8192);
    char *header = arena_alloc(&temp, 256);
    char *body = arena_alloc(&temp, 4096);
    // ... procesar ...
    arena_destroy(&temp);  // Limpio al terminar
}

// 3. FRAME ALLOCATOR EN JUEGOS
// Datos temporales por frame, reset al inicio del siguiente.

// 4. TESTS
// Cada test usa una arena → cleanup automático garantizado.
```

### NO ideal para

```c
// 1. DATOS CON LIFETIMES MUY DIFERENTES
// Si A vive 1 segundo y B vive 1 hora en la misma arena,
// la arena no puede destruirse hasta que B termine.
// A desperdicia memoria durante 59 minutos.

// 2. CUANDO SE NECESITA FREE INDIVIDUAL
// Arenas NO soportan liberar un bloque específico.
// Si necesitas liberar piezas individualmente → malloc/free o pool.

// 3. DATOS QUE DEBEN SOBREVIVIR AL SCOPE
// Si los datos deben vivir más allá de la fase actual,
// copiarlos fuera de la arena antes de destruirla.
```

---

## 9. Arenas en proyectos reales

El patrón arena es ubicuo en software de alto rendimiento:

| Proyecto | Uso del arena |
|----------|---------------|
| **GCC / Clang** | Cada translation unit usa un arena para AST y tablas de símbolos |
| **nginx** | Cada request usa un `ngx_pool_t` (arena con cleanup callbacks) |
| **SQLite** | Pools y arenas para gestión de queries temporales |
| **Game engines** | Frame allocator: arena reseteado cada frame |
| **Redis** | zmalloc con pools para estructuras internas |

### Lenguajes que integran arenas nativamente

```c
// Zig: pasa un "Allocator" como parámetro a las funciones.
// Las funciones no saben si usan malloc, arena, o stack.
// fn parse(allocator: std.mem.Allocator, input: []u8) !AST

// Odin: similar a Zig, allocators son first-class.
// parse :: proc(input: string, allocator := context.allocator) -> AST

// Go: el garbage collector actúa como una "arena automática",
// pero con overhead de GC en vez de bump pointer.
```

---

## 10. Comparación con Rust

```rust
// Rust con bumpalo (crate de arena allocator):
use bumpalo::Bump;

fn process_request(data: &[u8]) {
    let arena = Bump::new();

    // Alocar en la arena — O(1), bump pointer:
    let header = arena.alloc_str("Content-Type: text/html");
    let numbers = arena.alloc_slice_copy(&[1, 2, 3, 4, 5]);
    let point = arena.alloc(Point { x: 1.0, y: 2.0 });

    // Usar normalmente:
    println!("{}", header);
    println!("{:?}", numbers);

    // Al salir del scope, arena se dropea automáticamente
    // → equivalente a arena_destroy, pero sin poder olvidarlo
}
// drop(arena) aquí — toda la memoria liberada

// Ventajas de bumpalo sobre arena en C:
// 1. Drop automático (RAII) — imposible olvidar destroy
// 2. Type-safe — arena.alloc::<Point>() retorna &Point, no void*
// 3. Lifetime checking — el compilador rechaza usar
//    punteros del arena después de que la arena se destruya
// 4. Borrow checker — impide modificar datos a través de
//    múltiples referencias mutables simultáneas

// Vec con arena (sin usar el allocator global):
let names: bumpalo::collections::Vec<&str> = bumpalo::vec![in &arena;
    "Alice", "Bob", "Charlie"
];

// Global allocator override:
// Se puede hacer que TODO el programa use un arena
// configurando #[global_allocator], pero no es común.
// Lo habitual es usar arenas locales para fases específicas.
```

La diferencia clave: en C, si usas un puntero del arena después de `arena_destroy`, es UB silencioso. En Rust, el compilador lo rechaza en tiempo de compilación gracias al sistema de lifetimes.

---

## Ejercicios

### Ejercicio 1 — Arena básica: alocar y verificar

```c
#include <stdio.h>
#include "arena.h"  // Usar la implementación del lab

int main(void) {
    struct Arena a;
    arena_init(&a, 256);

    int *x = arena_alloc(&a, sizeof(int));
    int *y = arena_alloc(&a, sizeof(int));
    *x = 42;
    *y = 99;

    printf("offset after 2 ints: %zu\n", a.offset);
    printf("x=%d y=%d\n", *x, *y);
    printf("x addr: %p, y addr: %p\n", (void *)x, (void *)y);
    printf("distance: %td bytes\n", (char *)y - (char *)x);

    arena_destroy(&a);
    return 0;
}
```

**Predicción**: ¿Cuál será el offset después de 2 ints? ¿Cuál será la distancia entre `x` e `y`?

<details><summary>Respuesta</summary>

Offset = 16. Cada `int` ocupa 4 bytes, pero alineado a 8 ocupa 8. Dos alocaciones: 8 + 8 = 16. La distancia entre `x` e `y` es 8 bytes (no 4), porque el alineamiento inserta 4 bytes de padding después de cada int.

</details>

### Ejercicio 2 — Alineamiento con tipos mixtos

```c
#include <stdio.h>
#include "arena.h"

int main(void) {
    struct Arena a;
    arena_init(&a, 512);

    char *s = arena_alloc(&a, 5);   // "hola" + '\0'
    printf("After char[5]: offset=%zu\n", a.offset);

    double *d = arena_alloc(&a, sizeof(double));
    printf("After double:  offset=%zu\n", a.offset);

    char *s2 = arena_alloc(&a, 1);
    printf("After char[1]: offset=%zu\n", a.offset);

    int *n = arena_alloc(&a, sizeof(int));
    printf("After int:     offset=%zu\n", a.offset);

    arena_destroy(&a);
    return 0;
}
```

**Predicción**: ¿Cuáles serán los 4 offsets?

<details><summary>Respuesta</summary>

- `char[5]`: 5 bytes, alineado a 8 → offset = **8**
- `double`: 8 bytes, alineado a 8 → 8 + 8 = offset = **16**
- `char[1]`: 1 byte, alineado a 8 → 16 + 8 = offset = **24**
- `int`: 4 bytes, alineado a 8 → 24 + 8 = offset = **32**

Cada alocación ocupa mínimo 8 bytes por el alineamiento. Los 5 bytes de `char[5]` desperdician 3 bytes de padding. El `char[1]` desperdicia 7 bytes. Este es el costo del alineamiento universal a 8.

</details>

### Ejercicio 3 — Arena reset reutiliza memoria

```c
#include <stdio.h>
#include "arena.h"

int main(void) {
    struct Arena a;
    arena_init(&a, 128);

    int *first = arena_alloc(&a, sizeof(int));
    *first = 111;
    printf("first=%p val=%d offset=%zu\n", (void *)first, *first, a.offset);

    arena_reset(&a);
    printf("After reset: offset=%zu\n", a.offset);

    int *second = arena_alloc(&a, sizeof(int));
    *second = 222;
    printf("second=%p val=%d offset=%zu\n", (void *)second, *second, a.offset);

    printf("Same address: %s\n", (first == second) ? "YES" : "NO");

    arena_destroy(&a);
    return 0;
}
```

**Predicción**: ¿`first` y `second` tendrán la misma dirección? ¿Qué valor tiene `*first` después de `*second = 222`?

<details><summary>Respuesta</summary>

Sí, misma dirección. Tras `arena_reset`, el offset vuelve a 0. La siguiente `arena_alloc` retorna la misma posición (base + 0). `first` y `second` apuntan al mismo lugar, así que `*first` ahora es 222. **Pero**: usar `first` después de `arena_reset` es conceptualmente incorrecto — los datos del "frame anterior" se consideran descartados. Es análogo a use-after-free, pero sin UB porque la memoria sigue alocada.

</details>

### Ejercicio 4 — Arena se llena

```c
#include <stdio.h>
#include "arena.h"

int main(void) {
    struct Arena a;
    arena_init(&a, 32);  // Arena muy pequeña

    int *x = arena_alloc(&a, sizeof(int));    // 8 bytes (alineado)
    int *y = arena_alloc(&a, sizeof(int));    // 8 bytes
    int *z = arena_alloc(&a, sizeof(int));    // 8 bytes
    int *w = arena_alloc(&a, sizeof(int));    // 8 bytes → total: 32
    int *v = arena_alloc(&a, sizeof(int));    // ¿Cabe?

    printf("x=%p y=%p z=%p w=%p\n",
           (void *)x, (void *)y, (void *)z, (void *)w);
    printf("v=%p\n", (void *)v);
    printf("offset=%zu cap=%zu\n", a.offset, a.cap);

    arena_destroy(&a);
    return 0;
}
```

**Predicción**: ¿`v` será NULL? ¿Cuál será el offset?

<details><summary>Respuesta</summary>

`v` será **NULL**. Capacidad: 32 bytes. Las primeras 4 alocaciones usan 4 × 8 = 32 bytes, llenando la arena. La quinta alocación falla: `offset(32) + aligned(8) = 40 > cap(32)` → retorna NULL. El offset se queda en 32. La arena no crece automáticamente (para eso se necesitaría `GrowableArena`).

</details>

### Ejercicio 5 — Comparar malloc vs arena: conteo de syscalls

```c
#include <stdio.h>
#include <stdlib.h>
#include "arena.h"

int main(void) {
    int n = 50;

    // Enfoque malloc/free:
    int *blocks[50];
    for (int i = 0; i < n; i++) {
        blocks[i] = malloc(sizeof(int));
        *blocks[i] = i;
    }
    for (int i = 0; i < n; i++) {
        free(blocks[i]);
    }

    // Enfoque arena:
    struct Arena a;
    arena_init(&a, n * 8);  // 50 ints alineados a 8
    for (int i = 0; i < n; i++) {
        int *p = arena_alloc(&a, sizeof(int));
        *p = i;
    }
    arena_destroy(&a);

    return 0;
}
```

Ejecuta con `valgrind --leak-check=full`.

**Predicción**: ¿Cuántos allocs/frees reportará Valgrind para cada enfoque?

<details><summary>Respuesta</summary>

Valgrind reportará: "total heap usage: **51 allocs, 51 frees**" (50 mallocs individuales + 1 malloc de la arena + el buffer interno de printf/stdout). Sin la arena, serían 50 allocs + 50 frees. Con la arena: 1 alloc + 1 free. La suma total es 51 porque el programa usa ambos enfoques secuencialmente. Lo importante: 50 mallocs reducidos a 1.

</details>

### Ejercicio 6 — Arena strdup

```c
#include <stdio.h>
#include <string.h>
#include "arena.h"

char *arena_strdup(struct Arena *a, const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = arena_alloc(a, len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

int main(void) {
    struct Arena a;
    arena_init(&a, 256);

    char *s1 = arena_strdup(&a, "hello");
    char *s2 = arena_strdup(&a, "world");
    char *s3 = arena_strdup(&a, "!");

    printf("%s %s%s\n", s1, s2, s3);
    printf("offset=%zu\n", a.offset);

    // ¿Necesitamos free(s1), free(s2), free(s3)?
    arena_destroy(&a);
    return 0;
}
```

**Predicción**: ¿Cuál será el offset? ¿Cuántos bytes de padding hay en total?

<details><summary>Respuesta</summary>

- `"hello"` → 6 bytes, alineado a 8 → 8 bytes
- `"world"` → 6 bytes, alineado a 8 → 8 bytes
- `"!"` → 2 bytes, alineado a 8 → 8 bytes
- Offset total: **24**
- Padding total: (8-6) + (8-6) + (8-2) = 2 + 2 + 6 = **10 bytes** de padding

No se necesita `free` individual — `arena_destroy` libera todo. Es imposible tener leak mientras se llame `arena_destroy`.

</details>

### Ejercicio 7 — Frame allocator simulado

```c
#include <stdio.h>
#include "arena.h"

struct Particle { double x, y, vx, vy; };

int main(void) {
    struct Arena frame;
    arena_init(&frame, 4096);

    for (int f = 0; f < 5; f++) {
        arena_reset(&frame);

        int n = 10;
        struct Particle *particles = arena_alloc(&frame,
            (size_t)n * sizeof(struct Particle));

        for (int i = 0; i < n; i++) {
            particles[i] = (struct Particle){
                .x = i * 1.0, .y = f * 10.0,
                .vx = 0.5, .vy = -0.5
            };
        }

        printf("Frame %d: offset=%zu, p[0]=(%.0f,%.0f)\n",
               f, frame.offset, particles[0].x, particles[0].y);
    }

    arena_destroy(&frame);
    return 0;
}
```

**Predicción**: ¿El offset será igual en todos los frames? ¿Cuántos malloc/free totales?

<details><summary>Respuesta</summary>

Sí, el offset es **320** en todos los frames (10 Particles × 32 bytes cada uno). `sizeof(struct Particle)` = 32 (4 doubles × 8), ya alineado a 8 → sin padding adicional. Total: **1 malloc** (arena_init) + **1 free** (arena_destroy) = 2 operaciones de heap para 5 frames × 10 partículas = 50 alocaciones lógicas.

</details>

### Ejercicio 8 — Arena vs pool: diferencia conceptual

```c
// Sin compilar — análisis mental.
// Pool allocator (de T02_Fragmentación_de_heap):
struct Pool {
    char memory[POOL_CAP * SLOT_SIZE];
    int used[POOL_CAP];
};
// pool_alloc: busca slot libre O(n), retorna slot fijo
// pool_free: marca slot como libre → PERMITE free individual

// Arena:
struct Arena {
    char *base;
    size_t offset, cap;
};
// arena_alloc: avanza offset O(1)
// NO hay arena_free individual
```

**Predicción**: ¿En qué escenario preferirías un pool sobre un arena? ¿Y un arena sobre un pool?

<details><summary>Respuesta</summary>

**Pool sobre arena**: cuando necesitas **free individual** de objetos con lifetimes diferentes. Ejemplo: entidades en un juego que se crean y destruyen en momentos impredecibles. El pool permite reutilizar slots individuales.

**Arena sobre pool**: cuando todos los objetos tienen el **mismo lifetime** (se crean en una fase y se destruyen todos juntos). Ejemplo: parsear un archivo, procesar un request. Arena es más rápido (O(1) vs O(n) para alloc) y soporta tipos de tamaños diferentes (el pool requiere slots de tamaño fijo).

</details>

### Ejercicio 9 — Growable arena

```c
#include <stdio.h>
#include <stdlib.h>

// Arena simple de 32 bytes
struct Arena {
    char *base;
    size_t offset, cap;
};

int arena_init(struct Arena *a, size_t cap) {
    a->base = malloc(cap);
    if (!a->base) return -1;
    a->offset = 0;
    a->cap = cap;
    return 0;
}

void *arena_alloc(struct Arena *a, size_t size) {
    size_t aligned = (size + 7) & ~(size_t)7;
    if (a->offset + aligned > a->cap) return NULL;
    void *ptr = a->base + a->offset;
    a->offset += aligned;
    return ptr;
}

void arena_destroy(struct Arena *a) { free(a->base); }

int main(void) {
    struct Arena a;
    arena_init(&a, 32);

    int count = 0;
    while (arena_alloc(&a, sizeof(double)) != NULL) {
        count++;
    }
    printf("Allocated %d doubles in 32 bytes\n", count);

    arena_destroy(&a);
    return 0;
}
```

**Predicción**: ¿Cuántos doubles caben en una arena de 32 bytes?

<details><summary>Respuesta</summary>

**4 doubles**. Cada `double` = 8 bytes, ya alineado a 8. 32 / 8 = 4. La quinta alocación falla: offset(32) + 8 = 40 > cap(32) → NULL. No hay overhead por bloque (a diferencia de malloc donde cada alocación tiene un header de ~8 bytes). En la arena, los 32 bytes son 100% usables para datos.

</details>

### Ejercicio 10 — Constructor de AST con arena

```c
#include <stdio.h>
#include <string.h>
#include "arena.h"

enum NodeType { NODE_NUM, NODE_ADD, NODE_MUL };

struct ASTNode {
    enum NodeType type;
    int value;             // Solo para NODE_NUM
    struct ASTNode *left;  // Solo para NODE_ADD/MUL
    struct ASTNode *right;
};

struct ASTNode *make_num(struct Arena *a, int val) {
    struct ASTNode *n = arena_alloc(a, sizeof(*n));
    if (!n) return NULL;
    *n = (struct ASTNode){ .type = NODE_NUM, .value = val };
    return n;
}

struct ASTNode *make_binop(struct Arena *a, enum NodeType op,
                           struct ASTNode *l, struct ASTNode *r) {
    struct ASTNode *n = arena_alloc(a, sizeof(*n));
    if (!n) return NULL;
    *n = (struct ASTNode){ .type = op, .left = l, .right = r };
    return n;
}

int eval(struct ASTNode *n) {
    if (!n) return 0;
    switch (n->type) {
        case NODE_NUM: return n->value;
        case NODE_ADD: return eval(n->left) + eval(n->right);
        case NODE_MUL: return eval(n->left) * eval(n->right);
    }
    return 0;
}

int main(void) {
    struct Arena a;
    arena_init(&a, 4096);

    // Construir: (2 + 3) * (4 + 5)
    struct ASTNode *expr = make_binop(&a, NODE_MUL,
        make_binop(&a, NODE_ADD, make_num(&a, 2), make_num(&a, 3)),
        make_binop(&a, NODE_ADD, make_num(&a, 4), make_num(&a, 5))
    );

    printf("(2 + 3) * (4 + 5) = %d\n", eval(expr));
    printf("Arena used: %zu bytes for %d nodes\n", a.offset, 7);

    // Sin arena, habría que implementar tree_free recursivo.
    // Con arena: un solo destroy.
    arena_destroy(&a);
    return 0;
}
```

**Predicción**: ¿Cuántos bytes usará la arena para los 7 nodos? ¿Cuántos malloc/free totales con Valgrind?

<details><summary>Respuesta</summary>

`sizeof(struct ASTNode)` en 64-bit: enum(4) + padding(4) + int(4) + padding(4) + ptr(8) + ptr(8) = 32 bytes (con padding para alinear los punteros a 8). Ya alineado a 8 → 32 bytes por nodo. 7 nodos × 32 = **224 bytes** de offset. Valgrind: **1 malloc** (arena_init) + **1 free** (arena_destroy) = 2 operaciones totales (más el buffer de printf). Sin arena, se necesitarían 7 mallocs + una función recursiva `tree_free` con 7 frees.

</details>
