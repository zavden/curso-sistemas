# T02 — Arena allocators

## Qué es un arena allocator

Un arena (también llamado bump allocator, linear allocator o
region-based allocator) es un patrón donde se aloca un gran
bloque de memoria y se **subaloca** desde él secuencialmente.
Toda la memoria se libera **de una vez**:

```c
// Con malloc/free individual:
int *a = malloc(sizeof(int));
int *b = malloc(sizeof(int));
int *c = malloc(sizeof(int));
// ... usar a, b, c ...
free(a);
free(b);
free(c);
// 3 mallocs + 3 frees + posible fragmentación

// Con arena:
Arena arena = arena_create(4096);    // un solo malloc de 4 KB
int *a = arena_alloc(&arena, sizeof(int));
int *b = arena_alloc(&arena, sizeof(int));
int *c = arena_alloc(&arena, sizeof(int));
// ... usar a, b, c ...
arena_destroy(&arena);    // un solo free — libera todo
```

```c
// Modelo mental:
//
// Arena (4096 bytes):
// ┌──────────────────────────────────────────────────────────┐
// │[a:4][b:4][c:4][                   libre                ]│
// └──────────────────────────────────────────────────────────┘
//  ↑                ↑                                        ↑
//  base             offset                                   cap
//
// arena_alloc simplemente avanza el offset (bump).
// No hay free individual — solo arena_destroy o arena_reset.
```

## Implementación simple

```c
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

struct Arena {
    char *base;       // inicio de la memoria
    size_t offset;    // posición actual (siguiente byte libre)
    size_t cap;       // capacidad total
};

int arena_init(struct Arena *a, size_t cap) {
    a->base = malloc(cap);
    if (!a->base) return -1;
    a->offset = 0;
    a->cap = cap;
    return 0;
}

void *arena_alloc(struct Arena *a, size_t size) {
    // Alinear a 8 bytes (para cualquier tipo):
    size_t aligned = (size + 7) & ~(size_t)7;

    if (a->offset + aligned > a->cap) {
        return NULL;    // sin espacio
    }

    void *ptr = a->base + a->offset;
    a->offset += aligned;
    return ptr;
}

void arena_reset(struct Arena *a) {
    a->offset = 0;    // "liberar" todo sin realocar
}

void arena_destroy(struct Arena *a) {
    free(a->base);
    a->base = NULL;
    a->offset = 0;
    a->cap = 0;
}
```

### Uso

```c
#include <stdio.h>

struct Point {
    double x, y;
};

int main(void) {
    struct Arena arena;
    arena_init(&arena, 4096);

    // Alocar sin preocuparse de free individual:
    int *nums = arena_alloc(&arena, 10 * sizeof(int));
    struct Point *p1 = arena_alloc(&arena, sizeof(struct Point));
    struct Point *p2 = arena_alloc(&arena, sizeof(struct Point));
    char *name = arena_alloc(&arena, 50);

    // Usar normalmente:
    for (int i = 0; i < 10; i++) nums[i] = i;
    *p1 = (struct Point){1.0, 2.0};
    *p2 = (struct Point){3.0, 4.0};
    snprintf(name, 50, "test arena");

    printf("p1 = (%f, %f)\n", p1->x, p1->y);
    printf("name = %s\n", name);
    printf("Used: %zu / %zu bytes\n", arena.offset, arena.cap);

    // Liberar TODO de una vez:
    arena_destroy(&arena);
    return 0;
}
```

## Arena con crecimiento

```c
// Una arena que crece cuando se llena:

struct ArenaBlock {
    char *data;
    size_t cap;
    struct ArenaBlock *next;
};

struct GrowableArena {
    struct ArenaBlock *current;
    size_t offset;
    size_t default_cap;
};

int garena_init(struct GrowableArena *a, size_t cap) {
    a->current = malloc(sizeof(struct ArenaBlock));
    if (!a->current) return -1;

    a->current->data = malloc(cap);
    if (!a->current->data) {
        free(a->current);
        return -1;
    }
    a->current->cap = cap;
    a->current->next = NULL;
    a->offset = 0;
    a->default_cap = cap;
    return 0;
}

void *garena_alloc(struct GrowableArena *a, size_t size) {
    size_t aligned = (size + 7) & ~(size_t)7;

    if (a->offset + aligned > a->current->cap) {
        // Alocar nuevo bloque:
        size_t new_cap = a->default_cap;
        if (aligned > new_cap) new_cap = aligned;

        struct ArenaBlock *block = malloc(sizeof(struct ArenaBlock));
        if (!block) return NULL;
        block->data = malloc(new_cap);
        if (!block->data) { free(block); return NULL; }
        block->cap = new_cap;
        block->next = a->current;
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

## Ventajas del arena

```c
// 1. Rendimiento:
// arena_alloc es O(1) — solo incrementa un offset.
// malloc es O(n) en el peor caso (busca en free lists).
// Sin fragmentación del heap.

// 2. Simplicidad:
// No hay free individual — imposible tener:
// - Use-after-free (dentro de la vida del arena)
// - Double free
// - Memory leaks (se libera todo con destroy)
// Solo un punto de falla: arena_destroy.

// 3. Locality:
// Todos los datos están contiguos → excelente cache performance.

// 4. Batch deallocation:
// Perfecto para procesamiento por fases donde todo se
// libera al terminar la fase.
```

## Cuándo usar arenas

```c
// IDEAL para:

// 1. Parsers y compiladores:
// Cada archivo se parsea en un arena.
// Al terminar el archivo, se destruye el arena.
struct Arena ast_arena;
arena_init(&ast_arena, 1024 * 1024);  // 1 MB para el AST
// ... parsear, crear nodos del AST en ast_arena ...
// ... procesar el AST ...
arena_destroy(&ast_arena);    // liberar todo el AST de una vez

// 2. Request handling en servidores:
// Cada request usa un arena temporal.
void handle_request(Request *req) {
    struct Arena temp;
    arena_init(&temp, 8192);

    char *header = arena_alloc(&temp, 256);
    char *body = arena_alloc(&temp, 4096);
    // ... procesar request ...

    arena_destroy(&temp);    // limpio al terminar el request
}

// 3. Frame allocator en juegos:
// Cada frame del juego usa un arena.
// Se resetea (no se destruye) al inicio del siguiente frame.
struct Arena frame_arena;
arena_init(&frame_arena, 1024 * 1024);

while (running) {
    arena_reset(&frame_arena);    // reusar memoria del frame anterior
    // ... alocar datos temporales del frame ...
    render();
}
arena_destroy(&frame_arena);

// 4. Tests:
// Cada test usa un arena → cleanup automático.
```

```c
// NO ideal para:

// 1. Datos con lifetimes muy diferentes:
// Si A vive 1 segundo y B vive 1 hora y ambos están
// en el mismo arena, el arena no se puede destruir
// hasta que B termine → A desperdicia memoria.

// 2. Cuando se necesita free individual:
// Arenas no soportan liberar un bloque específico.
// Si necesitas liberar piezas individuales, usar malloc/free.

// 3. Datos que deben sobrevivir al scope:
// Si los datos deben vivir más allá de la fase actual,
// usar malloc o moverlos a otro arena.
```

## Arena helpers

```c
// Funciones útiles construidas sobre el arena:

// Alocar e inicializar a cero (como calloc):
void *arena_calloc(struct Arena *a, size_t count, size_t size) {
    void *ptr = arena_alloc(a, count * size);
    if (ptr) memset(ptr, 0, count * size);
    return ptr;
}

// Duplicar un string en el arena:
char *arena_strdup(struct Arena *a, const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = arena_alloc(a, len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

// sprintf en el arena:
char *arena_sprintf(struct Arena *a, const char *fmt, ...) {
    va_list args, args2;
    va_start(args, fmt);
    va_copy(args2, args);

    int len = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    char *buf = arena_alloc(a, len + 1);
    if (buf) vsnprintf(buf, len + 1, fmt, args2);
    va_end(args2);

    return buf;
}
```

## Arena en proyectos reales

```c
// El patrón arena se usa extensamente en:
//
// - Compiladores (GCC, Clang): cada translation unit usa un arena
//   para el AST y las tablas de símbolos.
//
// - Servidores web (nginx): cada request usa un pool allocator
//   similar a un arena.
//
// - Bases de datos (SQLite): usa pools y arenas para queries.
//
// - Juegos (muchos engines): frame allocator = arena reseteado cada frame.
//
// - Zig y Odin: pasan un "allocator" como parámetro a las funciones,
//   permitiendo usar arenas de forma natural.
```

---

## Ejercicios

### Ejercicio 1 — Arena básico

```c
// Implementar el arena simple (init, alloc, reset, destroy).
// Alocar 100 structs Point { double x, y; } en el arena.
// Verificar que todos los datos son accesibles.
// Reset el arena, alocar otros 100 Points.
// Destroy al final. Verificar con valgrind.
```

### Ejercicio 2 — Arena string builder

```c
// Usar un arena para construir un string largo:
// 1. Crear un arena de 4096 bytes.
// 2. Implementar arena_strdup y arena_sprintf.
// 3. Construir un string concatenando 50 líneas "Line N: hello\n".
// 4. Imprimir el resultado.
// Sin un solo free individual — solo arena_destroy.
```

### Ejercicio 3 — Parser con arena

```c
// Implementar un parser CSV simple que use un arena:
// 1. Leer un archivo CSV.
// 2. Parsear cada línea en tokens (strings en el arena).
// 3. Almacenar las filas en un array de arrays (todo en el arena).
// 4. Imprimir la tabla.
// 5. arena_destroy libera todo (datos + estructura).
// Verificar con valgrind: un solo malloc, un solo free.
```
