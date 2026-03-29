# T03 — Object Pool en C

---

## 1. El problema: malloc en hot path

Cuando un programa necesita crear y destruir objetos frecuentemente
(conexiones, buffers, particulas, mensajes), cada `malloc`/`free`
tiene un costo:

```
  Hot path sin pool:
  ──────────────────
  request llega → malloc(sizeof(Buffer)) → procesar → free(buffer)
  request llega → malloc(sizeof(Buffer)) → procesar → free(buffer)
  request llega → malloc(sizeof(Buffer)) → procesar → free(buffer)
  ... 10,000 requests/segundo ...

  Cada malloc:
  1. Busca bloque libre en el heap (~50-200ns)
  2. Posible llamada a sbrk/mmap (~1000ns)
  3. Fragmentacion del heap
  4. Cache miss al acceder a memoria nueva
```

Un object pool pre-asigna los objetos una vez y los recicla:

```
  Hot path con pool:
  ──────────────────
  init: malloc N buffers una vez

  request llega → pool_acquire() → procesar → pool_release()
  request llega → pool_acquire() → procesar → pool_release()
  request llega → pool_acquire() → procesar → pool_release()

  Cada acquire/release:
  1. Pop/push en free list (~5-10ns)
  2. Sin fragmentacion
  3. Misma memoria → caliente en cache
```

```
  ┌──────────────────────────────────────────────────┐
  │  Pool (N=4 buffers)                              │
  │                                                  │
  │  Estado inicial:                                 │
  │  [LIBRE] [LIBRE] [LIBRE] [LIBRE]                │
  │                                                  │
  │  acquire() → [EN USO] [LIBRE] [LIBRE] [LIBRE]   │
  │  acquire() → [EN USO] [EN USO] [LIBRE] [LIBRE]  │
  │  release(0) → [LIBRE] [EN USO] [LIBRE] [LIBRE]  │
  │  acquire() → [EN USO] [EN USO] [LIBRE] [LIBRE]  │
  │              reutiliza slot 0 ↑                   │
  └──────────────────────────────────────────────────┘
```

---

## 2. Pool basico: array + flag in_use

La implementacion mas simple:

```c
/* buffer_pool.h */
#ifndef BUFFER_POOL_H
#define BUFFER_POOL_H

#include <stddef.h>

#define BUFFER_SIZE 4096
#define POOL_CAPACITY 64

typedef struct {
    char   data[BUFFER_SIZE];
    size_t used;   /* Bytes actualmente escritos */
} Buffer;

typedef struct BufferPool BufferPool;

BufferPool *buffer_pool_create(void);
void        buffer_pool_destroy(BufferPool *pool);

Buffer     *buffer_pool_acquire(BufferPool *pool);
void        buffer_pool_release(BufferPool *pool, Buffer *buf);

size_t      buffer_pool_available(const BufferPool *pool);

#endif
```

```c
/* buffer_pool.c */
#include "buffer_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef struct {
    Buffer buf;
    int    in_use;
} PoolSlot;

struct BufferPool {
    PoolSlot slots[POOL_CAPACITY];
    size_t   available;
};

BufferPool *buffer_pool_create(void) {
    BufferPool *pool = calloc(1, sizeof(*pool));
    if (!pool) return NULL;

    pool->available = POOL_CAPACITY;
    /* calloc ya pone todo a 0: in_use = 0, data = 0 */
    return pool;
}

void buffer_pool_destroy(BufferPool *pool) {
    if (!pool) return;

    /* Verificar que todos fueron devueltos */
    for (size_t i = 0; i < POOL_CAPACITY; i++) {
        if (pool->slots[i].in_use) {
            fprintf(stderr, "WARNING: buffer_pool_destroy: "
                    "slot %zu still in use\n", i);
        }
    }
    free(pool);
}

Buffer *buffer_pool_acquire(BufferPool *pool) {
    for (size_t i = 0; i < POOL_CAPACITY; i++) {
        if (!pool->slots[i].in_use) {
            pool->slots[i].in_use = 1;
            pool->slots[i].buf.used = 0;  /* Reset del buffer */
            pool->available--;
            return &pool->slots[i].buf;
        }
    }
    return NULL;  /* Pool agotado */
}

void buffer_pool_release(BufferPool *pool, Buffer *buf) {
    /* Encontrar el slot correspondiente */
    for (size_t i = 0; i < POOL_CAPACITY; i++) {
        if (&pool->slots[i].buf == buf) {
            if (!pool->slots[i].in_use) {
                fprintf(stderr, "WARNING: double release of slot %zu\n", i);
                return;
            }
            pool->slots[i].in_use = 0;
            pool->available++;
            return;
        }
    }
    fprintf(stderr, "ERROR: buffer %p does not belong to this pool\n",
            (void *)buf);
}

size_t buffer_pool_available(const BufferPool *pool) {
    return pool->available;
}
```

```c
/* main.c */
#include "buffer_pool.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    BufferPool *pool = buffer_pool_create();

    /* Adquirir un buffer */
    Buffer *b1 = buffer_pool_acquire(pool);
    if (b1) {
        const char *msg = "Hello, pool!";
        memcpy(b1->data, msg, strlen(msg) + 1);
        b1->used = strlen(msg);
        printf("b1: %s (%zu bytes)\n", b1->data, b1->used);
    }

    /* Adquirir otro */
    Buffer *b2 = buffer_pool_acquire(pool);
    printf("Available: %zu\n", buffer_pool_available(pool));  /* 62 */

    /* Devolver b1 */
    buffer_pool_release(pool, b1);
    printf("Available: %zu\n", buffer_pool_available(pool));  /* 63 */

    /* Reutilizar: acquire retorna el mismo slot */
    Buffer *b3 = buffer_pool_acquire(pool);
    printf("b3 == b1? %s\n", b3 == b1 ? "yes" : "no");  /* yes */

    buffer_pool_release(pool, b2);
    buffer_pool_release(pool, b3);
    buffer_pool_destroy(pool);
    return 0;
}
```

### 2.1 Problema de la busqueda lineal

`acquire` recorre todos los slots buscando uno libre — O(N):

```
  Pool con 1000 slots, 999 en uso:
  acquire() recorre 999 slots antes de encontrar el libre.
  1000 acquires = O(N²) total.
```

---

## 3. Pool con free list: O(1) acquire y release

En vez de buscar linealmente, mantener una lista enlazada de
slots libres. Usamos los mismos slots como nodos de la lista
(cuando estan libres, su memoria no se usa para datos):

```
  Free list embebida:
  ───────────────────

  Slot libre: data[] no se usa → reutilizar los primeros
  bytes para almacenar el puntero "next" de la free list.

  ┌─────────┐    ┌─────────┐    ┌─────────┐
  │ next: ──┼───>│ next: ──┼───>│ next:NULL│
  │ (libre) │    │ (libre) │    │ (libre) │
  └─────────┘    └─────────┘    └─────────┘
       ↑
       free_head

  acquire():   pop del head → O(1)
  release():   push al head → O(1)
```

### 3.1 Free list con indice (mas seguro)

En vez de punteros embebidos, usar un array de indices:

```c
/* pool.h */
#ifndef POOL_H
#define POOL_H

#include <stddef.h>

#define POOL_INVALID ((size_t)-1)

typedef struct Pool Pool;

/* Crea pool para `count` objetos de `obj_size` bytes cada uno */
Pool   *pool_create(size_t obj_size, size_t count);
void    pool_destroy(Pool *pool);

/* O(1) acquire/release */
void   *pool_acquire(Pool *pool);
void    pool_release(Pool *pool, void *obj);

size_t  pool_available(const Pool *pool);
size_t  pool_capacity(const Pool *pool);

#endif
```

```c
/* pool.c */
#include "pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Pool {
    char    *data;       /* Array contiguo de objetos */
    size_t  *free_stack; /* Stack de indices libres */
    size_t   obj_size;   /* Tamano de cada objeto */
    size_t   capacity;   /* Total de slots */
    size_t   free_top;   /* Tope del stack (proximo a entregar) */
};

Pool *pool_create(size_t obj_size, size_t count) {
    Pool *pool = malloc(sizeof(*pool));
    if (!pool) return NULL;

    pool->data = calloc(count, obj_size);
    pool->free_stack = malloc(count * sizeof(size_t));
    if (!pool->data || !pool->free_stack) {
        free(pool->data);
        free(pool->free_stack);
        free(pool);
        return NULL;
    }

    pool->obj_size = obj_size;
    pool->capacity = count;
    pool->free_top = count;  /* Todos libres */

    /* Llenar el stack: [0, 1, 2, ..., count-1] */
    for (size_t i = 0; i < count; i++) {
        pool->free_stack[i] = i;
    }

    return pool;
}

void pool_destroy(Pool *pool) {
    if (!pool) return;
    if (pool->free_top != pool->capacity) {
        fprintf(stderr, "WARNING: pool_destroy: %zu objects still in use\n",
                pool->capacity - pool->free_top);
    }
    free(pool->data);
    free(pool->free_stack);
    free(pool);
}

void *pool_acquire(Pool *pool) {
    if (pool->free_top == 0) {
        return NULL;  /* Pool agotado */
    }
    pool->free_top--;
    size_t idx = pool->free_stack[pool->free_top];
    void *obj = pool->data + (idx * pool->obj_size);
    memset(obj, 0, pool->obj_size);  /* Reset del objeto */
    return obj;
}

void pool_release(Pool *pool, void *obj) {
    /* Calcular indice a partir del puntero */
    ptrdiff_t offset = (char *)obj - pool->data;
    if (offset < 0 || (size_t)offset >= pool->capacity * pool->obj_size) {
        fprintf(stderr, "ERROR: object %p not from this pool\n", obj);
        return;
    }

    size_t idx = (size_t)offset / pool->obj_size;
    if ((size_t)offset % pool->obj_size != 0) {
        fprintf(stderr, "ERROR: misaligned pointer %p\n", obj);
        return;
    }

    /* Push al stack */
    pool->free_stack[pool->free_top] = idx;
    pool->free_top++;
}

size_t pool_available(const Pool *pool) {
    return pool->free_top;
}

size_t pool_capacity(const Pool *pool) {
    return pool->capacity;
}
```

### 3.2 Anatomia del free stack

```
  Pool create(obj_size=100, count=4):

  data (400 bytes contiguos):
  ┌──────────┬──────────┬──────────┬──────────┐
  │  slot 0  │  slot 1  │  slot 2  │  slot 3  │
  │ 100 bytes│ 100 bytes│ 100 bytes│ 100 bytes│
  └──────────┴──────────┴──────────┴──────────┘

  free_stack:  [0, 1, 2, 3]   free_top = 4 (todos libres)
                         ↑

  acquire():
  free_stack:  [0, 1, 2, 3]   free_top = 3, retorna slot 3
                      ↑

  acquire():
  free_stack:  [0, 1, 2, 3]   free_top = 2, retorna slot 2
                   ↑

  release(slot 3):
  free_stack:  [0, 1, 3, 3]   free_top = 3, push 3
                      ↑

  acquire():
  free_stack:  [0, 1, 3, 3]   free_top = 2, retorna slot 3
                   ↑            (el mismo que acabamos de devolver)
```

**Ventaja del stack sobre busqueda lineal**:
- `acquire`: un decremento + un array access = O(1)
- `release`: un calculo + un array access = O(1)
- No importa cuantos slots hay, siempre es O(1)

---

## 4. Pool generico con macro

Para evitar casts manuales en cada uso:

```c
/* pool_typed.h */
#include "pool.h"

#define TYPED_POOL_DECLARE(Type, Name)                          \
    typedef struct {                                            \
        Pool *inner;                                            \
    } Name##Pool;                                               \
                                                                \
    static inline Name##Pool Name##_pool_create(size_t count) { \
        return (Name##Pool){ pool_create(sizeof(Type), count) };\
    }                                                           \
                                                                \
    static inline void Name##_pool_destroy(Name##Pool *p) {     \
        pool_destroy(p->inner);                                 \
    }                                                           \
                                                                \
    static inline Type *Name##_pool_acquire(Name##Pool *p) {    \
        return (Type *)pool_acquire(p->inner);                  \
    }                                                           \
                                                                \
    static inline void Name##_pool_release(Name##Pool *p,       \
                                           Type *obj) {         \
        pool_release(p->inner, obj);                            \
    }

/* Uso: */
typedef struct {
    int  id;
    char name[64];
    double balance;
} Account;

TYPED_POOL_DECLARE(Account, Account)
/* Genera: AccountPool, Account_pool_create, etc. */
```

```c
int main(void) {
    AccountPool pool = Account_pool_create(100);

    Account *a = Account_pool_acquire(&pool);  /* Retorna Account*, no void* */
    a->id = 42;
    snprintf(a->name, sizeof(a->name), "Alice");
    a->balance = 1000.0;

    Account_pool_release(&pool, a);
    Account_pool_destroy(&pool);
    return 0;
}
```

---

## 5. Pool thread-safe

Para pools compartidos entre threads:

```c
/* pool_mt.h */
#ifndef POOL_MT_H
#define POOL_MT_H

#include <stddef.h>
#include <pthread.h>

typedef struct PoolMT PoolMT;

PoolMT *pool_mt_create(size_t obj_size, size_t count);
void    pool_mt_destroy(PoolMT *pool);
void   *pool_mt_acquire(PoolMT *pool);       /* Bloquea si vacio */
void   *pool_mt_try_acquire(PoolMT *pool);   /* Retorna NULL si vacio */
void    pool_mt_release(PoolMT *pool, void *obj);

#endif
```

```c
/* pool_mt.c */
#include "pool_mt.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct PoolMT {
    char            *data;
    size_t          *free_stack;
    size_t           obj_size;
    size_t           capacity;
    size_t           free_top;
    pthread_mutex_t  mutex;
    pthread_cond_t   not_empty;  /* Senalada cuando hay objetos disponibles */
};

PoolMT *pool_mt_create(size_t obj_size, size_t count) {
    PoolMT *pool = malloc(sizeof(*pool));
    if (!pool) return NULL;

    pool->data = calloc(count, obj_size);
    pool->free_stack = malloc(count * sizeof(size_t));
    if (!pool->data || !pool->free_stack) {
        free(pool->data);
        free(pool->free_stack);
        free(pool);
        return NULL;
    }

    pool->obj_size = obj_size;
    pool->capacity = count;
    pool->free_top = count;

    for (size_t i = 0; i < count; i++) {
        pool->free_stack[i] = i;
    }

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
    return pool;
}

void pool_mt_destroy(PoolMT *pool) {
    if (!pool) return;
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->not_empty);
    free(pool->data);
    free(pool->free_stack);
    free(pool);
}

void *pool_mt_acquire(PoolMT *pool) {
    pthread_mutex_lock(&pool->mutex);

    /* Esperar hasta que haya un objeto disponible */
    while (pool->free_top == 0) {
        pthread_cond_wait(&pool->not_empty, &pool->mutex);
    }

    pool->free_top--;
    size_t idx = pool->free_stack[pool->free_top];
    void *obj = pool->data + (idx * pool->obj_size);

    pthread_mutex_unlock(&pool->mutex);
    memset(obj, 0, pool->obj_size);
    return obj;
}

void *pool_mt_try_acquire(PoolMT *pool) {
    pthread_mutex_lock(&pool->mutex);

    if (pool->free_top == 0) {
        pthread_mutex_unlock(&pool->mutex);
        return NULL;
    }

    pool->free_top--;
    size_t idx = pool->free_stack[pool->free_top];
    void *obj = pool->data + (idx * pool->obj_size);

    pthread_mutex_unlock(&pool->mutex);
    memset(obj, 0, pool->obj_size);
    return obj;
}

void pool_mt_release(PoolMT *pool, void *obj) {
    ptrdiff_t offset = (char *)obj - pool->data;
    size_t idx = (size_t)offset / pool->obj_size;

    pthread_mutex_lock(&pool->mutex);

    pool->free_stack[pool->free_top] = idx;
    pool->free_top++;

    /* Despertar un thread que espera en acquire */
    pthread_cond_signal(&pool->not_empty);

    pthread_mutex_unlock(&pool->mutex);
}
```

```
  Thread A: acquire()           Thread B: acquire()
  │                             │
  ├─ lock                       ├─ lock (espera)
  ├─ free_top > 0 → pop        │
  ├─ unlock                     ├─ lock
  │                             ├─ free_top == 0!
  │                             ├─ cond_wait (duerme)
  │                             │
  │  ... Thread A procesa ...   │  (dormido)
  │                             │
  ├─ release()                  │
  ├─ lock                       │
  ├─ push al stack              │
  ├─ cond_signal ──────────────>├─ despierta!
  ├─ unlock                     ├─ re-check: free_top > 0
  │                             ├─ pop
  │                             ├─ unlock
```

---

## 6. Pool con free list embebida (sin array extra)

Para eliminar el array `free_stack` separado, embeber la lista
enlazada dentro de los objetos libres:

```c
/* Cuando un slot esta libre, sus primeros bytes almacenan
   el indice del siguiente slot libre. */

typedef union {
    char   data[BUFFER_SIZE];  /* Cuando esta en uso */
    size_t next_free;          /* Cuando esta libre */
} PoolSlot;

struct Pool {
    PoolSlot *slots;
    size_t    capacity;
    size_t    free_head;   /* Indice del primer libre, o POOL_INVALID */
    size_t    available;
};

Pool *pool_create(size_t count) {
    Pool *pool = malloc(sizeof(*pool));
    pool->slots    = calloc(count, sizeof(PoolSlot));
    pool->capacity = count;
    pool->available = count;

    /* Encadenar la free list: 0→1→2→...→(n-1)→INVALID */
    for (size_t i = 0; i < count - 1; i++) {
        pool->slots[i].next_free = i + 1;
    }
    pool->slots[count - 1].next_free = POOL_INVALID;
    pool->free_head = 0;

    return pool;
}

void *pool_acquire(Pool *pool) {
    if (pool->free_head == POOL_INVALID) return NULL;

    size_t idx = pool->free_head;
    pool->free_head = pool->slots[idx].next_free;  /* Pop */
    pool->available--;
    memset(&pool->slots[idx], 0, sizeof(PoolSlot)); /* Reset */
    return &pool->slots[idx];
}

void pool_release(Pool *pool, void *obj) {
    PoolSlot *slot = (PoolSlot *)obj;
    size_t idx = slot - pool->slots;

    slot->next_free = pool->free_head;  /* Push */
    pool->free_head = idx;
    pool->available++;
}
```

```
  Memoria con free list embebida:

  slots:
  ┌───────────────┬───────────────┬───────────────┬───────────────┐
  │ next_free: 1  │ next_free: 2  │ next_free: 3  │ next_free: INV│
  │  (libre)      │  (libre)      │  (libre)      │  (libre)      │
  └───────────────┴───────────────┴───────────────┴───────────────┘
  free_head = 0

  acquire() → retorna slot 0, free_head = 1
  ┌───────────────┬───────────────┬───────────────┬───────────────┐
  │ [DATA EN USO] │ next_free: 2  │ next_free: 3  │ next_free: INV│
  └───────────────┴───────────────┴───────────────┴───────────────┘
  free_head = 1

  release(slot 0) → free_head = 0, slot 0.next = 1
  ┌───────────────┬───────────────┬───────────────┬───────────────┐
  │ next_free: 1  │ next_free: 2  │ next_free: 3  │ next_free: INV│
  │  (libre)      │  (libre)      │  (libre)      │  (libre)      │
  └───────────────┴───────────────┴───────────────┴───────────────┘
  free_head = 0
```

**Ventaja**: sin array auxiliar — la free list vive dentro de
los datos. Ahorra `count * sizeof(size_t)` bytes.

**Restriccion**: el objeto debe ser al menos `sizeof(size_t)` bytes
para que quepa el puntero `next_free`.

---

## 7. Pool con reset en vez de release individual

Para escenarios donde todos los objetos se liberan juntos
(frames de un juego, requests batch):

```c
typedef struct {
    char  *data;
    size_t obj_size;
    size_t capacity;
    size_t count;    /* Objetos actualmente adquiridos */
} ArenaPool;

ArenaPool *arena_pool_create(size_t obj_size, size_t capacity) {
    ArenaPool *a = malloc(sizeof(*a));
    a->data     = calloc(capacity, obj_size);
    a->obj_size = obj_size;
    a->capacity = capacity;
    a->count    = 0;
    return a;
}

void *arena_pool_acquire(ArenaPool *a) {
    if (a->count >= a->capacity) return NULL;
    void *obj = a->data + (a->count * a->obj_size);
    a->count++;
    return obj;
}

void arena_pool_reset(ArenaPool *a) {
    /* Libera TODOS los objetos de una vez — O(1) */
    a->count = 0;
    /* Opcional: memset(a->data, 0, a->capacity * a->obj_size); */
}

void arena_pool_destroy(ArenaPool *a) {
    free(a->data);
    free(a);
}
```

```
  Frame-based allocation:
  ──────────────────────

  Frame 1: acquire → acquire → acquire → render → reset()
  Frame 2: acquire → acquire → acquire → render → reset()

  reset() no libera objetos individualmente.
  Simplemente mueve el contador a 0.
  Los objetos "viejos" se sobreescriben en el siguiente frame.
```

**Uso tipico**: game loops, procesamiento de requests batch,
parsers que construyen un arbol temporal.

---

## 8. Ejemplo real: pool de conexiones

```c
/* conn_pool.h */
#ifndef CONN_POOL_H
#define CONN_POOL_H

#include <pthread.h>

typedef struct {
    int  fd;           /* File descriptor del socket */
    int  alive;        /* 1 si la conexion sigue activa */
    long last_used;    /* Timestamp del ultimo uso */
} Connection;

typedef struct ConnPool ConnPool;

ConnPool   *conn_pool_create(const char *host, int port, size_t size);
void        conn_pool_destroy(ConnPool *pool);

Connection *conn_pool_acquire(ConnPool *pool);
void        conn_pool_release(ConnPool *pool, Connection *conn);

/* Mantenimiento: cerrar conexiones inactivas */
void        conn_pool_evict_idle(ConnPool *pool, long max_idle_sec);

#endif
```

```c
/* conn_pool.c */
#include "conn_pool.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

struct ConnPool {
    Connection      *conns;
    int             *free_stack;
    size_t           capacity;
    size_t           free_top;
    char             host[256];
    int              port;
    pthread_mutex_t  mutex;
    pthread_cond_t   available;
};

/* Simula abrir una conexion TCP */
static int open_connection(const char *host, int port) {
    /* En produccion: socket() + connect() */
    (void)host; (void)port;
    static int fake_fd = 100;
    return fake_fd++;
}

static void close_connection(int fd) {
    /* En produccion: close(fd) */
    (void)fd;
}

ConnPool *conn_pool_create(const char *host, int port, size_t size) {
    ConnPool *pool = calloc(1, sizeof(*pool));
    if (!pool) return NULL;

    pool->conns      = calloc(size, sizeof(Connection));
    pool->free_stack = malloc(size * sizeof(int));
    pool->capacity   = size;
    pool->free_top   = size;
    pool->port       = port;
    strncpy(pool->host, host, sizeof(pool->host) - 1);

    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->available, NULL);

    /* Pre-abrir todas las conexiones */
    for (size_t i = 0; i < size; i++) {
        pool->conns[i].fd        = open_connection(host, port);
        pool->conns[i].alive     = 1;
        pool->conns[i].last_used = time(NULL);
        pool->free_stack[i]      = (int)i;
    }

    return pool;
}

void conn_pool_destroy(ConnPool *pool) {
    if (!pool) return;

    for (size_t i = 0; i < pool->capacity; i++) {
        if (pool->conns[i].fd >= 0) {
            close_connection(pool->conns[i].fd);
        }
    }

    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->available);
    free(pool->conns);
    free(pool->free_stack);
    free(pool);
}

Connection *conn_pool_acquire(ConnPool *pool) {
    pthread_mutex_lock(&pool->mutex);

    while (pool->free_top == 0) {
        pthread_cond_wait(&pool->available, &pool->mutex);
    }

    pool->free_top--;
    int idx = pool->free_stack[pool->free_top];
    Connection *conn = &pool->conns[idx];

    /* Re-conectar si la conexion murio */
    if (!conn->alive) {
        close_connection(conn->fd);
        conn->fd    = open_connection(pool->host, pool->port);
        conn->alive = 1;
    }
    conn->last_used = time(NULL);

    pthread_mutex_unlock(&pool->mutex);
    return conn;
}

void conn_pool_release(ConnPool *pool, Connection *conn) {
    int idx = (int)(conn - pool->conns);

    pthread_mutex_lock(&pool->mutex);

    conn->last_used = time(NULL);
    pool->free_stack[pool->free_top] = idx;
    pool->free_top++;

    pthread_cond_signal(&pool->available);
    pthread_mutex_unlock(&pool->mutex);
}

void conn_pool_evict_idle(ConnPool *pool, long max_idle_sec) {
    long now = time(NULL);
    pthread_mutex_lock(&pool->mutex);

    /* Solo revisar conexiones en la free list (no en uso) */
    for (size_t i = 0; i < pool->free_top; i++) {
        int idx = pool->free_stack[i];
        Connection *conn = &pool->conns[idx];
        if (conn->alive && (now - conn->last_used) > max_idle_sec) {
            close_connection(conn->fd);
            conn->alive = 0;
            conn->fd    = -1;
        }
    }

    pthread_mutex_unlock(&pool->mutex);
}
```

```c
/* main.c */
#include "conn_pool.h"
#include <stdio.h>

int main(void) {
    ConnPool *pool = conn_pool_create("db.example.com", 5432, 5);

    Connection *c1 = conn_pool_acquire(pool);
    printf("Got connection fd=%d\n", c1->fd);

    Connection *c2 = conn_pool_acquire(pool);
    printf("Got connection fd=%d\n", c2->fd);

    /* Devolver c1 — queda disponible para reutilizar */
    conn_pool_release(pool, c1);

    /* Adquirir otra — recibe c1 reciclada */
    Connection *c3 = conn_pool_acquire(pool);
    printf("c3 == c1? %s (fd=%d)\n", c3 == c1 ? "yes" : "no", c3->fd);

    conn_pool_release(pool, c2);
    conn_pool_release(pool, c3);
    conn_pool_destroy(pool);
    return 0;
}
```

---

## 9. Rendimiento: pool vs malloc

```
  Benchmark tipico (allocating 4KB buffers):
  ──────────────────────────────────────────

  Operacion                  Tiempo aprox.   Notas
  ─────────                  ─────────────   ─────
  malloc(4096)               50-200 ns       Depende de fragmentacion
  free(ptr)                  30-100 ns       Depende de implementacion
  pool_acquire (free stack)  5-10 ns         Pop de stack
  pool_release (free stack)  5-10 ns         Push a stack
  pool_acquire (linear scan) 10-500 ns       Depende de ocupacion

  Speedup del pool: 5x-20x para acquire/release

  Pero el beneficio REAL no es solo la velocidad:
  ─────────────────────────────────────────────────
  1. Predecibilidad: siempre O(1), sin picos
  2. Fragmentacion: cero (bloques del mismo tamano)
  3. Cache: misma memoria reutilizada → caliente en L1/L2
  4. Determinismo: util en sistemas real-time
```

### 9.1 Cuando NO usar pool

```
  Situacion                              Usar pool?
  ─────────                              ──────────
  Objetos creados/destruidos raramente   NO — malloc basta
  Objetos de tamanos variables           NO — pool es fixed-size
  Pocos objetos (< 10)                   NO — overhead del pool
  Hot path, miles/segundo                SI
  Real-time (jitter inaceptable)         SI
  Embedded sin heap (sin malloc)         SI
  Objetos con lifetime largo             NO — no se reciclan
```

---

## 10. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| Usar objeto despues de release | Objeto sobreescrito por siguiente acquire | Set pointer a NULL post-release |
| Double release | Corrompe la free list | Flag in_use o debug check |
| Release de puntero ajeno | Indice calculado es basura | Verificar rango del puntero |
| Pool agotado sin aviso | `acquire` retorna NULL silenciosamente | Log + metrica de pool exhaustion |
| Busqueda lineal en pool grande | O(N) acquire | Free stack o free list embebida |
| Olvidar reset del objeto | Datos del uso anterior persisten | `memset` en acquire |
| Pool para objetos de tamano variable | Desperdicio o overflow | Separar en pools por tamano |
| Pool sin mutex en multi-thread | Race en free_top | `PoolMT` con mutex + cond_var |

---

## 11. Ejercicios

### Ejercicio 1 — Pool basico con array

Implementa un pool de `Message` (id, text[256], priority) con
array + flag `in_use`. Capacidad 8.

**Prediccion**: ¿que complejidad tiene acquire en el peor caso?

<details>
<summary>Respuesta</summary>

```c
#define MSG_POOL_CAP 8

typedef struct {
    int  id;
    char text[256];
    int  priority;
} Message;

typedef struct {
    Message msg;
    int     in_use;
} MsgSlot;

typedef struct {
    MsgSlot slots[MSG_POOL_CAP];
    size_t  available;
} MsgPool;

void msg_pool_init(MsgPool *pool) {
    memset(pool, 0, sizeof(*pool));
    pool->available = MSG_POOL_CAP;
}

Message *msg_pool_acquire(MsgPool *pool) {
    for (size_t i = 0; i < MSG_POOL_CAP; i++) {
        if (!pool->slots[i].in_use) {
            pool->slots[i].in_use = 1;
            memset(&pool->slots[i].msg, 0, sizeof(Message));
            pool->available--;
            return &pool->slots[i].msg;
        }
    }
    return NULL;
}

void msg_pool_release(MsgPool *pool, Message *msg) {
    for (size_t i = 0; i < MSG_POOL_CAP; i++) {
        if (&pool->slots[i].msg == msg) {
            pool->slots[i].in_use = 0;
            pool->available++;
            return;
        }
    }
}
```

Peor caso de acquire: O(N) — todos los slots ocupados excepto
el ultimo. Con N=8 es trivial, pero con N=10000 seria un
problema. Solucion: free stack (seccion 3).

</details>

---

### Ejercicio 2 — Free stack

Convierte el ejercicio 1 a free stack con O(1) acquire/release.

**Prediccion**: ¿cuanta memoria extra necesita el free stack?

<details>
<summary>Respuesta</summary>

```c
#define MSG_POOL_CAP 8

typedef struct {
    Message  messages[MSG_POOL_CAP];
    size_t   free_stack[MSG_POOL_CAP];
    size_t   free_top;
} MsgPool;

void msg_pool_init(MsgPool *pool) {
    memset(pool->messages, 0, sizeof(pool->messages));
    pool->free_top = MSG_POOL_CAP;
    for (size_t i = 0; i < MSG_POOL_CAP; i++) {
        pool->free_stack[i] = i;
    }
}

Message *msg_pool_acquire(MsgPool *pool) {
    if (pool->free_top == 0) return NULL;
    pool->free_top--;
    size_t idx = pool->free_stack[pool->free_top];
    memset(&pool->messages[idx], 0, sizeof(Message));
    return &pool->messages[idx];
}

void msg_pool_release(MsgPool *pool, Message *msg) {
    size_t idx = (size_t)(msg - pool->messages);
    pool->free_stack[pool->free_top] = idx;
    pool->free_top++;
}
```

Memoria extra: `MSG_POOL_CAP * sizeof(size_t)` = 8 * 8 = 64 bytes
(en 64-bit). Cada Message es 268 bytes, asi que el overhead es
64 / (8 * 268) = 3%. Pago insignificante por O(1) acquire/release.

</details>

---

### Ejercicio 3 — Pool generico con void*

Implementa un pool generico que trabaje con cualquier tipo
usando `void *` y `obj_size`.

**Prediccion**: ¿como calculas el indice de un objeto a partir
de su puntero?

<details>
<summary>Respuesta</summary>

```c
typedef struct {
    char   *data;        /* Array contiguo */
    size_t *free_stack;
    size_t  obj_size;
    size_t  capacity;
    size_t  free_top;
} GenericPool;

GenericPool *gpool_create(size_t obj_size, size_t capacity) {
    GenericPool *p = malloc(sizeof(*p));
    p->data       = calloc(capacity, obj_size);
    p->free_stack = malloc(capacity * sizeof(size_t));
    p->obj_size   = obj_size;
    p->capacity   = capacity;
    p->free_top   = capacity;
    for (size_t i = 0; i < capacity; i++) {
        p->free_stack[i] = i;
    }
    return p;
}

void *gpool_acquire(GenericPool *p) {
    if (p->free_top == 0) return NULL;
    p->free_top--;
    size_t idx = p->free_stack[p->free_top];
    void *obj = p->data + (idx * p->obj_size);
    memset(obj, 0, p->obj_size);
    return obj;
}

void gpool_release(GenericPool *p, void *obj) {
    /* Calculo del indice: */
    ptrdiff_t offset = (char *)obj - p->data;
    size_t idx = (size_t)offset / p->obj_size;

    p->free_stack[p->free_top] = idx;
    p->free_top++;
}

void gpool_destroy(GenericPool *p) {
    free(p->data);
    free(p->free_stack);
    free(p);
}
```

El indice se calcula como:
`idx = ((char*)obj - pool->data) / pool->obj_size`

Funciona porque los objetos estan contiguos en `data`, separados
por exactamente `obj_size` bytes. El cast a `char*` hace que la
resta sea en bytes.

</details>

---

### Ejercicio 4 — Free list embebida

Implementa un pool donde la free list esta embebida en los objetos
libres (sin array auxiliar). Objeto: `Particle` (float x, y, z, vx, vy, vz).

**Prediccion**: ¿que restriccion tiene el tamano minimo del objeto?

<details>
<summary>Respuesta</summary>

```c
typedef struct {
    float x, y, z;
    float vx, vy, vz;
} Particle;

/* Union: libre → next index, en uso → Particle */
typedef union {
    Particle particle;
    size_t   next_free;
} ParticleSlot;

/* Verificar que el union cabe el indice */
_Static_assert(sizeof(Particle) >= sizeof(size_t),
    "Particle must be >= sizeof(size_t) for embedded free list");

#define PARTICLE_POOL_CAP 1000
#define INVALID ((size_t)-1)

typedef struct {
    ParticleSlot slots[PARTICLE_POOL_CAP];
    size_t       free_head;
    size_t       available;
} ParticlePool;

void ppool_init(ParticlePool *pool) {
    pool->available = PARTICLE_POOL_CAP;
    pool->free_head = 0;
    for (size_t i = 0; i < PARTICLE_POOL_CAP - 1; i++) {
        pool->slots[i].next_free = i + 1;
    }
    pool->slots[PARTICLE_POOL_CAP - 1].next_free = INVALID;
}

Particle *ppool_acquire(ParticlePool *pool) {
    if (pool->free_head == INVALID) return NULL;
    size_t idx = pool->free_head;
    pool->free_head = pool->slots[idx].next_free;
    pool->available--;
    memset(&pool->slots[idx].particle, 0, sizeof(Particle));
    return &pool->slots[idx].particle;
}

void ppool_release(ParticlePool *pool, Particle *p) {
    ParticleSlot *slot = (ParticleSlot *)p;
    size_t idx = (size_t)(slot - pool->slots);
    slot->next_free = pool->free_head;
    pool->free_head = idx;
    pool->available++;
}
```

Restriccion: `sizeof(Particle)` debe ser >= `sizeof(size_t)`.
`Particle` tiene 6 floats = 24 bytes, `size_t` = 8 bytes → cumple.

Si el objeto fuera solo 4 bytes (un `int`), el union
necesitaria 8 bytes — desperdicio del 50%. En ese caso,
usar el free stack separado es mas eficiente.

</details>

---

### Ejercicio 5 — Pool thread-safe

Agrega `pthread_mutex` y `pthread_cond` al pool del ejercicio 2.
`acquire` debe bloquear si no hay objetos disponibles.

**Prediccion**: ¿por que `while` en vez de `if` antes de `cond_wait`?

<details>
<summary>Respuesta</summary>

```c
typedef struct {
    Message          messages[MSG_POOL_CAP];
    size_t           free_stack[MSG_POOL_CAP];
    size_t           free_top;
    pthread_mutex_t  mutex;
    pthread_cond_t   not_empty;
} MsgPoolMT;

void msg_pool_mt_init(MsgPoolMT *pool) {
    memset(pool->messages, 0, sizeof(pool->messages));
    pool->free_top = MSG_POOL_CAP;
    for (size_t i = 0; i < MSG_POOL_CAP; i++) {
        pool->free_stack[i] = i;
    }
    pthread_mutex_init(&pool->mutex, NULL);
    pthread_cond_init(&pool->not_empty, NULL);
}

Message *msg_pool_mt_acquire(MsgPoolMT *pool) {
    pthread_mutex_lock(&pool->mutex);

    while (pool->free_top == 0) {  /* WHILE, no IF */
        pthread_cond_wait(&pool->not_empty, &pool->mutex);
    }

    pool->free_top--;
    size_t idx = pool->free_stack[pool->free_top];
    pthread_mutex_unlock(&pool->mutex);

    memset(&pool->messages[idx], 0, sizeof(Message));
    return &pool->messages[idx];
}

void msg_pool_mt_release(MsgPoolMT *pool, Message *msg) {
    size_t idx = (size_t)(msg - pool->messages);

    pthread_mutex_lock(&pool->mutex);
    pool->free_stack[pool->free_top] = idx;
    pool->free_top++;
    pthread_cond_signal(&pool->not_empty);
    pthread_mutex_unlock(&pool->mutex);
}
```

**¿Por que `while` y no `if`?** Spurious wakeups:

```
  Thread A y B esperan en cond_wait (pool vacio).
  Thread C hace release → cond_signal → despierta A.

  Pero en algunas implementaciones POSIX, B tambien
  puede despertar (spurious wakeup). Si B usa `if`:

  if (free_top == 0)     ← ya paso este check
      cond_wait(...)     ← despierta (spurious)
  // Continua sin re-verificar!
  free_stack[--free_top] ← free_top sigue siendo 0 → underflow!

  Con while:
  while (free_top == 0)  ← re-verifica cada vez
      cond_wait(...)     ← despierta (spurious)
  // Vuelve al while → free_top == 0 → cond_wait de nuevo
```

Regla: SIEMPRE `while` con `cond_wait`, nunca `if`.

</details>

---

### Ejercicio 6 — Arena pool con reset

Implementa un arena pool para `Event` structs que se crean
durante un frame y se liberan todos juntos con `reset()`.

**Prediccion**: ¿que operacion es O(1) en arena que no lo es
en un pool normal?

<details>
<summary>Respuesta</summary>

```c
typedef struct {
    int         type;
    int         source_id;
    long        timestamp;
    char        data[128];
} Event;

#define EVENT_ARENA_CAP 256

typedef struct {
    Event  events[EVENT_ARENA_CAP];
    size_t count;  /* Proximo slot a entregar */
} EventArena;

void event_arena_init(EventArena *arena) {
    arena->count = 0;
}

Event *event_arena_acquire(EventArena *arena) {
    if (arena->count >= EVENT_ARENA_CAP) return NULL;
    Event *e = &arena->events[arena->count];
    memset(e, 0, sizeof(Event));
    arena->count++;
    return e;
}

void event_arena_reset(EventArena *arena) {
    /* Libera TODOS — O(1) */
    arena->count = 0;
}

size_t event_arena_used(const EventArena *arena) {
    return arena->count;
}
```

`reset()` es O(1) — liberar N objetos cuesta lo mismo que liberar 1.
En un pool normal, liberar N objetos requiere N llamadas a release
(N push al free stack). Arena: un solo `count = 0`.

```
  Pool normal:  free(e1); free(e2); free(e3); ... free(eN);  → O(N)
  Arena:        arena_reset();                                → O(1)
```

Trade-off: no puedes liberar objetos individualmente en el arena.
Es todo-o-nada. Ideal para frames de juego, batch processing,
o cualquier escenario donde el lifetime de los objetos coincide.

</details>

---

### Ejercicio 7 — Deteccion de double-free

Agrega deteccion de double-free al pool generico.

**Prediccion**: ¿como sabes si un objeto ya fue devuelto al pool?

<details>
<summary>Respuesta</summary>

Opcion 1 — bitmap de uso:

```c
typedef struct {
    char   *data;
    size_t *free_stack;
    char   *in_use;      /* Bitmap: 1 = en uso, 0 = libre */
    size_t  obj_size;
    size_t  capacity;
    size_t  free_top;
} SafePool;

SafePool *safe_pool_create(size_t obj_size, size_t capacity) {
    SafePool *p = malloc(sizeof(*p));
    p->data       = calloc(capacity, obj_size);
    p->free_stack = malloc(capacity * sizeof(size_t));
    p->in_use     = calloc(capacity, 1);  /* Todo en 0 = libre */
    p->obj_size   = obj_size;
    p->capacity   = capacity;
    p->free_top   = capacity;
    for (size_t i = 0; i < capacity; i++) {
        p->free_stack[i] = i;
    }
    return p;
}

void *safe_pool_acquire(SafePool *p) {
    if (p->free_top == 0) return NULL;
    p->free_top--;
    size_t idx = p->free_stack[p->free_top];
    p->in_use[idx] = 1;
    void *obj = p->data + (idx * p->obj_size);
    memset(obj, 0, p->obj_size);
    return obj;
}

int safe_pool_release(SafePool *p, void *obj) {
    ptrdiff_t offset = (char *)obj - p->data;
    if (offset < 0 || (size_t)offset >= p->capacity * p->obj_size) {
        fprintf(stderr, "ERROR: pointer not from this pool\n");
        return -1;
    }
    size_t idx = (size_t)offset / p->obj_size;
    if ((size_t)offset % p->obj_size != 0) {
        fprintf(stderr, "ERROR: misaligned pointer\n");
        return -1;
    }

    if (!p->in_use[idx]) {
        fprintf(stderr, "ERROR: double free of slot %zu\n", idx);
        return -1;  /* Double free detectado! */
    }

    p->in_use[idx] = 0;
    p->free_stack[p->free_top] = idx;
    p->free_top++;
    return 0;
}
```

El bitmap `in_use` usa 1 byte extra por slot. Para un pool de
1000 objetos de 4KB, eso es 1000 bytes extra vs 4MB de datos —
despreciable. La alternativa (recorrer el free stack buscando
el indice) seria O(N).

</details>

---

### Ejercicio 8 — Pool que crece

Implementa un pool que duplica su capacidad cuando se agota
en vez de retornar NULL.

**Prediccion**: ¿que pasa con los punteros existentes cuando
el pool crece con realloc?

<details>
<summary>Respuesta</summary>

```c
typedef struct {
    char   *data;
    size_t *free_stack;
    size_t  obj_size;
    size_t  capacity;
    size_t  free_top;
} GrowablePool;

static int pool_grow(GrowablePool *p) {
    size_t new_cap = p->capacity * 2;

    char *new_data = realloc(p->data, new_cap * p->obj_size);
    if (!new_data) return -1;

    size_t *new_stack = realloc(p->free_stack, new_cap * sizeof(size_t));
    if (!new_stack) {
        /* data ya fue movido por realloc — no podemos deshacer */
        p->data = new_data;
        return -1;
    }

    /* Inicializar nuevos slots */
    memset(new_data + (p->capacity * p->obj_size), 0,
           (new_cap - p->capacity) * p->obj_size);

    /* Agregar nuevos indices al free stack */
    for (size_t i = p->capacity; i < new_cap; i++) {
        new_stack[p->free_top] = i;
        p->free_top++;
    }

    p->data       = new_data;
    p->free_stack = new_stack;
    p->capacity   = new_cap;
    return 0;
}

void *gpool_acquire(GrowablePool *p) {
    if (p->free_top == 0) {
        if (pool_grow(p) != 0) return NULL;
    }
    p->free_top--;
    size_t idx = p->free_stack[p->free_top];
    void *obj = p->data + (idx * p->obj_size);
    memset(obj, 0, p->obj_size);
    return obj;
}
```

**Problema critico**: `realloc` puede mover el bloque a otra
direccion. Todos los punteros que el caller tiene a objetos
del pool quedan **dangling**:

```
  Antes del grow:
  data = 0x1000
  caller tiene: obj_a = 0x1000, obj_b = 0x1100

  Despues del grow (realloc mueve):
  data = 0x5000
  obj_a y obj_b siguen apuntando a 0x1000 → DANGLING!
```

**Solucion**: no entregar punteros directos. Entregar **handles**
(indices):

```c
typedef size_t Handle;

Handle gpool_acquire(GrowablePool *p) {
    /* ... */
    return idx;  /* Indice, no puntero */
}

void *gpool_get(GrowablePool *p, Handle h) {
    return p->data + (h * p->obj_size);
    /* Recalcula puntero cada vez — safe despues de grow */
}
```

Esto anticipa T04 (Object Pool en Rust), donde el patron de
handles es idiomatico.

</details>

---

### Ejercicio 9 — Pool con estadisticas

Agrega al pool generico: peak usage, total acquires, total releases,
acquire failures (pool agotado).

**Prediccion**: ¿estas estadisticas necesitan mutex si el pool
ya es thread-safe?

<details>
<summary>Respuesta</summary>

```c
typedef struct {
    size_t total_acquires;
    size_t total_releases;
    size_t acquire_failures;
    size_t peak_usage;
    size_t current_usage;
} PoolStats;

typedef struct {
    char            *data;
    size_t          *free_stack;
    size_t           obj_size;
    size_t           capacity;
    size_t           free_top;
    PoolStats        stats;
    pthread_mutex_t  mutex;
} StatsPool;

void *stats_pool_acquire(StatsPool *p) {
    pthread_mutex_lock(&p->mutex);

    if (p->free_top == 0) {
        p->stats.acquire_failures++;
        pthread_mutex_unlock(&p->mutex);
        return NULL;
    }

    p->free_top--;
    size_t idx = p->free_stack[p->free_top];

    p->stats.total_acquires++;
    p->stats.current_usage++;
    if (p->stats.current_usage > p->stats.peak_usage) {
        p->stats.peak_usage = p->stats.current_usage;
    }

    pthread_mutex_unlock(&p->mutex);

    void *obj = p->data + (idx * p->obj_size);
    memset(obj, 0, p->obj_size);
    return obj;
}

void stats_pool_release(StatsPool *p, void *obj) {
    size_t idx = ((char *)obj - p->data) / p->obj_size;

    pthread_mutex_lock(&p->mutex);
    p->free_stack[p->free_top] = idx;
    p->free_top++;
    p->stats.total_releases++;
    p->stats.current_usage--;
    pthread_mutex_unlock(&p->mutex);
}

PoolStats stats_pool_get_stats(StatsPool *p) {
    pthread_mutex_lock(&p->mutex);
    PoolStats copy = p->stats;
    pthread_mutex_unlock(&p->mutex);
    return copy;
}
```

**¿Necesitan mutex?** Si el pool ya tiene mutex, las estadisticas
se actualizan DENTRO de la seccion critica existente — no necesitan
mutex propio. Es un "free ride" del lock que ya tomamos para
manipular el free stack.

Si las estadisticas fueran solo contadores (sin relacion entre
si), podrian ser `_Atomic` fuera del lock. Pero `peak_usage`
depende de `current_usage` → deben actualizarse atomicamente
juntas → el mutex del pool es suficiente.

</details>

---

### Ejercicio 10 — Pool por tamanos (slab allocator)

Implementa un allocator con 3 pools: small (64 bytes), medium
(512 bytes), large (4096 bytes). `slab_alloc(size)` elige el pool.

**Prediccion**: ¿que pasa si piden 100 bytes? ¿Y si piden 5000?

<details>
<summary>Respuesta</summary>

```c
#define SMALL_SIZE   64
#define MEDIUM_SIZE  512
#define LARGE_SIZE   4096

#define SMALL_COUNT  256
#define MEDIUM_COUNT 64
#define LARGE_COUNT  16

typedef struct {
    char   *data;
    size_t *free_stack;
    size_t  obj_size;
    size_t  capacity;
    size_t  free_top;
} Slab;

typedef struct {
    Slab small;
    Slab medium;
    Slab large;
} SlabAllocator;

static void slab_init(Slab *s, size_t obj_size, size_t count) {
    s->data       = calloc(count, obj_size);
    s->free_stack = malloc(count * sizeof(size_t));
    s->obj_size   = obj_size;
    s->capacity   = count;
    s->free_top   = count;
    for (size_t i = 0; i < count; i++) {
        s->free_stack[i] = i;
    }
}

void slab_allocator_init(SlabAllocator *sa) {
    slab_init(&sa->small,  SMALL_SIZE,  SMALL_COUNT);
    slab_init(&sa->medium, MEDIUM_SIZE, MEDIUM_COUNT);
    slab_init(&sa->large,  LARGE_SIZE,  LARGE_COUNT);
}

static void *slab_acquire(Slab *s) {
    if (s->free_top == 0) return NULL;
    s->free_top--;
    size_t idx = s->free_stack[s->free_top];
    void *obj = s->data + (idx * s->obj_size);
    memset(obj, 0, s->obj_size);
    return obj;
}

void *slab_alloc(SlabAllocator *sa, size_t size) {
    if (size <= SMALL_SIZE)       return slab_acquire(&sa->small);
    if (size <= MEDIUM_SIZE)      return slab_acquire(&sa->medium);
    if (size <= LARGE_SIZE)       return slab_acquire(&sa->large);
    /* Fallback a malloc para tamanos mayores */
    return malloc(size);
}

static void slab_release(Slab *s, void *obj) {
    size_t idx = ((char *)obj - s->data) / s->obj_size;
    s->free_stack[s->free_top] = idx;
    s->free_top++;
}

static int belongs_to(Slab *s, void *obj) {
    ptrdiff_t off = (char *)obj - s->data;
    return off >= 0 && (size_t)off < s->capacity * s->obj_size;
}

void slab_free(SlabAllocator *sa, void *obj) {
    if (belongs_to(&sa->small, obj))  { slab_release(&sa->small, obj);  return; }
    if (belongs_to(&sa->medium, obj)) { slab_release(&sa->medium, obj); return; }
    if (belongs_to(&sa->large, obj))  { slab_release(&sa->large, obj);  return; }
    /* No es de ninguna slab → fue malloc'd */
    free(obj);
}
```

**100 bytes**: va al pool medium (512 bytes). Desperdicia
512 - 100 = 412 bytes. Esto es el trade-off clasico: velocidad
vs fragmentacion interna.

**5000 bytes**: ninguna slab lo cubre → fallback a `malloc`.
Esto es comun: el slab cubre los tamanos frecuentes (hot path),
y las asignaciones raras/grandes usan malloc normal.

Este es el principio detras del slab allocator del kernel Linux
(creado por Jeff Bonwick para SunOS/Solaris). El kernel mantiene
caches por tipo de objeto (inode_cache, dentry_cache, etc.)
con exactamente este patron.

</details>
