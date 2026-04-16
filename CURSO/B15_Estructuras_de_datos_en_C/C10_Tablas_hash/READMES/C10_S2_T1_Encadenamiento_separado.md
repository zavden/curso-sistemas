# Encadenamiento separado (chaining)

## Objetivo

Implementar una tabla hash con **encadenamiento separado** como
estrategia de resolución de colisiones: cada bucket contiene una
lista enlazada que almacena todos los pares (clave, valor) que
hashean a ese índice:

- Inserción, búsqueda, eliminación: $O(1 + \alpha)$ promedio.
- $\alpha$ puede superar 1.0 sin degradación catastrófica.
- Implementación genérica en C con `void *`.
- Trade-offs vs open addressing.
- Referencia: S01/T04 clasificó las estrategias de colisión.
  Aquí se implementa chaining completo.

---

## Estructura de la tabla

Cada bucket es la cabeza de una lista enlazada simple:

```
tabla[0]: NULL
tabla[1]: (k1,v1) → NULL
tabla[2]: (k5,v5) → (k2,v2) → NULL
tabla[3]: NULL
tabla[4]: (k3,v3) → NULL
tabla[5]: (k7,v7) → (k6,v6) → (k4,v4) → NULL
...
```

### Estructura en C

```c
typedef struct Entry {
    char *key;
    int value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry **buckets;   // array de punteros a listas
    int m;             // capacidad (numero de buckets)
    int n;             // elementos almacenados
} HashTable;
```

`buckets` es un array de $m$ punteros. Cada puntero apunta a la
cabeza de una lista enlazada (o `NULL` si el bucket está vacío).

---

## Operaciones

### Crear

```c
HashTable *ht_create(int capacity) {
    HashTable *ht = malloc(sizeof(HashTable));
    ht->m = capacity;
    ht->n = 0;
    ht->buckets = calloc(capacity, sizeof(Entry *));
    return ht;
}
```

`calloc` inicializa todos los punteros a `NULL` — todos los
buckets empiezan vacíos.

### Función hash

```c
static unsigned int hash(const char *key, int m) {
    unsigned int h = 5381;  // djb2
    while (*key)
        h = h * 33 + (unsigned char)*key++;
    return h % m;
}
```

### Insertar

```c
void ht_insert(HashTable *ht, const char *key, int value) {
    unsigned int i = hash(key, ht->m);

    // buscar si la clave ya existe
    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            e->value = value;  // actualizar
            return;
        }
    }

    // insertar al inicio de la lista (O(1))
    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->value = value;
    e->next = ht->buckets[i];
    ht->buckets[i] = e;
    ht->n++;
}
```

**Insertar al inicio** (prepend) es $O(1)$. Insertar al final
requeriría recorrer la lista ($O(k)$ donde $k$ es la longitud
del bucket). El orden dentro del bucket no importa para la
corrección.

La verificación de clave duplicada recorre la lista primero: si
la clave ya existe, se actualiza el valor en vez de crear un
duplicado. Esto es esencial — sin la verificación, una tabla con
duplicados da resultados inconsistentes (¿cuál valor retornar?).

### Buscar

```c
int ht_search(HashTable *ht, const char *key, int *value) {
    unsigned int i = hash(key, ht->m);

    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            if (value) *value = e->value;
            return 1;  // encontrado
        }
    }
    return 0;  // no encontrado
}
```

Pasos:
1. Calcular $h(k)$: $O(m_k)$ donde $m_k$ es la longitud de la
   clave.
2. Acceder a `buckets[i]`: $O(1)$.
3. Recorrer la lista comparando claves: $O(\text{longitud})$.

### Eliminar

```c
int ht_delete(HashTable *ht, const char *key) {
    unsigned int i = hash(key, ht->m);

    Entry *prev = NULL;
    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            if (prev)
                prev->next = e->next;
            else
                ht->buckets[i] = e->next;
            free(e->key);
            free(e);
            ht->n--;
            return 1;
        }
        prev = e;
    }
    return 0;  // no encontrado
}
```

La eliminación en chaining es **trivial** comparada con open
addressing: simplemente desenlazar el nodo de la lista y liberarlo.
No hay tombstones ni cadenas de sondeo que romper.

### Liberar

```c
void ht_free(HashTable *ht) {
    for (int i = 0; i < ht->m; i++) {
        Entry *e = ht->buckets[i];
        while (e) {
            Entry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(ht->buckets);
    free(ht);
}
```

---

## Traza completa

Tabla con $m = 7$, insertando `{"alice", "bob", "carol", "dave",
"eve", "frank", "grace"}`:

```
hash("alice") % 7 = 3
hash("bob")   % 7 = 0
hash("carol") % 7 = 3    ← colision con alice
hash("dave")  % 7 = 5
hash("eve")   % 7 = 6
hash("frank") % 7 = 0    ← colision con bob
hash("grace") % 7 = 2

Tabla final:
  [0]: frank → bob → NULL
  [1]: NULL
  [2]: grace → NULL
  [3]: carol → alice → NULL
  [4]: NULL
  [5]: dave → NULL
  [6]: eve → NULL

alpha = 7/7 = 1.0
Colisiones: 2 (carol con alice, frank con bob)
Max chain: 2
```

**Buscar "carol"**:

```
h("carol") % 7 = 3
bucket[3]: carol → alice → NULL
  comparar "carol" == "carol"? SI → encontrado
Comparaciones: 1
```

**Buscar "alice"**:

```
h("alice") % 7 = 3
bucket[3]: carol → alice → NULL
  comparar "carol" == "alice"? NO → siguiente
  comparar "alice" == "alice"? SI → encontrado
Comparaciones: 2 (carol fue insertada despues, quedo al inicio)
```

**Buscar "xyz"** (no existe):

```
h("xyz") % 7 = 1
bucket[1]: NULL
  lista vacia → no encontrado
Comparaciones: 0
```

---

## Análisis de complejidad

### Supuestos

- Función hash uniforme: cada clave tiene probabilidad $1/m$ de
  caer en cualquier bucket.
- $n$ elementos, $m$ buckets, $\alpha = n/m$.

### Búsqueda fallida

Se recorre toda la lista del bucket, que tiene longitud esperada
$\alpha$:

$$T_{\text{miss}} = O(1 + \alpha)$$

El $1$ es el costo de calcular el hash y acceder al bucket.

### Búsqueda exitosa

El elemento buscado está en alguna posición de la lista. En
promedio, está a mitad de camino:

$$T_{\text{hit}} = O\left(1 + \frac{\alpha}{2}\right)$$

### Inserción (con verificación de duplicados)

Requiere recorrer la lista para verificar que la clave no existe,
luego insertar al inicio:

$$T_{\text{insert}} = O(1 + \alpha)$$

Si se permite insertar sin verificar duplicados: $O(1)$ (prepend
directo), pero se sacrifica corrección.

### Eliminación

Buscar el elemento y desenlazarlo:

$$T_{\text{delete}} = O(1 + \alpha)$$

### Resumen

| Operación | Promedio | Peor caso |
|-----------|:--------:|:---------:|
| Búsqueda (hit) | $O(1 + \alpha/2)$ | $O(n)$ |
| Búsqueda (miss) | $O(1 + \alpha)$ | $O(n)$ |
| Inserción | $O(1 + \alpha)$ | $O(n)$ |
| Eliminación | $O(1 + \alpha)$ | $O(n)$ |

Si $\alpha = O(1)$ (se redimensiona al crecer), todo es $O(1)$
amortizado.

---

## Variantes del bucket

### Lista enlazada simple (estándar)

Lo implementado arriba. Cada nodo tiene un puntero `next`.

**Ventaja**: simple, inserción $O(1)$ al inicio.
**Desventaja**: mala localidad de caché (nodos dispersos en el heap).

### Lista con move-to-front

Al encontrar un elemento en la búsqueda, moverlo al inicio de la
lista. Los elementos buscados frecuentemente migran al inicio,
reduciendo las comparaciones futuras:

```c
int ht_search_mtf(HashTable *ht, const char *key, int *value) {
    unsigned int i = hash(key, ht->m);

    Entry *prev = NULL;
    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            if (value) *value = e->value;
            // move to front
            if (prev) {
                prev->next = e->next;
                e->next = ht->buckets[i];
                ht->buckets[i] = e;
            }
            return 1;
        }
        prev = e;
    }
    return 0;
}
```

Útil cuando la distribución de búsquedas sigue una ley de Zipf
(pocas claves se buscan muchas veces). Ver C09/S01/T01.

### Lista ordenada

Mantener la lista de cada bucket ordenada por clave. Permite
**early termination** en búsqueda fallida (si pasamos la posición
donde debería estar, no está).

```c
// busqueda en lista ordenada
int ht_search_sorted(HashTable *ht, const char *key, int *value) {
    unsigned int i = hash(key, ht->m);

    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        int cmp = strcmp(e->key, key);
        if (cmp == 0) {
            if (value) *value = e->value;
            return 1;
        }
        if (cmp > 0) return 0;  // early exit
    }
    return 0;
}
```

Reduce las comparaciones de búsqueda fallida de $\alpha$ a
$\alpha/2$ en promedio, a costa de inserción $O(\alpha)$ (hay que
encontrar la posición correcta).

### Array dinámico por bucket

En vez de lista enlazada, cada bucket es un `Vec` / array
dinámico. Mejor localidad de caché pero inserciones más costosas
(posible realloc).

Go usa esta estrategia: cada bucket es un array inline de 8 slots
con overflow a buckets adicionales.

### Árbol por bucket (Java 8+)

Java 8 convierte la lista en un **red-black tree** cuando la
longitud del bucket supera 8 (y revierte a lista cuando cae por
debajo de 6). Esto garantiza $O(\log k)$ peor caso por bucket
en vez de $O(k)$.

### Tabla comparativa de variantes

| Variante | Hit promedio | Miss promedio | Inserción | Memoria |
|----------|:--------:|:--------:|:--------:|:-------:|
| Lista simple | $\alpha/2$ | $\alpha$ | $O(1)$ prepend | 1 ptr/nodo |
| Move-to-front | < $\alpha/2$ (Zipf) | $\alpha$ | $O(1)$ prepend | 1 ptr/nodo |
| Lista ordenada | $\alpha/2$ | $\alpha/2$ | $O(\alpha)$ | 1 ptr/nodo |
| Array dinámico | $\alpha/2$ | $\alpha$ | amort $O(1)$ | cache-friendly |
| Árbol (Java 8) | $O(\log \alpha)$ | $O(\log \alpha)$ | $O(\log \alpha)$ | 3 ptrs/nodo |

---

## Implementación genérica con `void *`

Una tabla hash útil debe poder almacenar **cualquier tipo** de
clave y valor. En C, esto se logra con `void *`:

```c
typedef unsigned int (*HashFunc)(const void *key, int m);
typedef int (*CmpFunc)(const void *a, const void *b);

typedef struct GenEntry {
    void *key;
    void *value;
    struct GenEntry *next;
} GenEntry;

typedef struct {
    GenEntry **buckets;
    int m;
    int n;
    HashFunc hash_fn;
    CmpFunc cmp_fn;
} GenHashTable;
```

El usuario provee la función hash y la de comparación al crear
la tabla, igual que `qsort` recibe un comparador.

### Ejemplo: tabla de strings → enteros

```c
unsigned int hash_str(const void *key, int m) {
    const char *s = key;
    unsigned int h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h % m;
}

int cmp_str(const void *a, const void *b) {
    return strcmp(a, b);
}

// uso:
GenHashTable *ht = gen_ht_create(101, hash_str, cmp_str);
int val = 42;
gen_ht_insert(ht, "hello", &val);
```

### Ejemplo: tabla de enteros → strings

```c
unsigned int hash_int(const void *key, int m) {
    int k = *(const int *)key;
    return ((k % m) + m) % m;
}

int cmp_int(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}
```

---

## Chaining vs open addressing: resumen práctico

| Aspecto | Chaining | Open addressing |
|---------|:--------:|:---------------:|
| Implementación | simple | media-alta |
| Eliminación | trivial | tombstones |
| $\alpha > 1$ | sí | no |
| Cache locality | mala | buena |
| Memoria overhead | puntero por nodo | ninguna |
| Degradación | lineal ($O(\alpha)$) | exponencial ($O(1/(1-\alpha))$) |
| Uso real | Java, Go, Redis | Python, Rust, C++ (desde C++11) |

La mayoría de las implementaciones modernas de alto rendimiento
usan **open addressing** (Swiss Table en Rust, Robin Hood en
versiones anteriores). Chaining es más simple y sigue siendo la
elección pedagógica y para muchos sistemas en producción.

---

## Programa completo en C

```c
// chaining.c — hash table with separate chaining
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// --- hash table with chaining ---

typedef struct Entry {
    char *key;
    int value;
    struct Entry *next;
} Entry;

typedef struct {
    Entry **buckets;
    int m;
    int n;
} HashTable;

unsigned int djb2(const char *s, int m) {
    unsigned int h = 5381;
    while (*s) h = h * 33 + (unsigned char)*s++;
    return h % m;
}

HashTable *ht_create(int capacity) {
    HashTable *ht = malloc(sizeof(HashTable));
    ht->m = capacity;
    ht->n = 0;
    ht->buckets = calloc(capacity, sizeof(Entry *));
    return ht;
}

void ht_insert(HashTable *ht, const char *key, int value) {
    unsigned int i = djb2(key, ht->m);

    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            e->value = value;
            return;
        }
    }

    Entry *e = malloc(sizeof(Entry));
    e->key = strdup(key);
    e->value = value;
    e->next = ht->buckets[i];
    ht->buckets[i] = e;
    ht->n++;
}

int ht_search(HashTable *ht, const char *key, int *value) {
    unsigned int i = djb2(key, ht->m);
    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            if (value) *value = e->value;
            return 1;
        }
    }
    return 0;
}

// search with comparison count
int ht_search_counted(HashTable *ht, const char *key, int *comps) {
    unsigned int i = djb2(key, ht->m);
    *comps = 0;
    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        (*comps)++;
        if (strcmp(e->key, key) == 0) return 1;
    }
    return 0;
}

int ht_delete(HashTable *ht, const char *key) {
    unsigned int i = djb2(key, ht->m);

    Entry *prev = NULL;
    for (Entry *e = ht->buckets[i]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) {
            if (prev)
                prev->next = e->next;
            else
                ht->buckets[i] = e->next;
            free(e->key);
            free(e);
            ht->n--;
            return 1;
        }
        prev = e;
    }
    return 0;
}

void ht_print(HashTable *ht) {
    for (int i = 0; i < ht->m; i++) {
        printf("  [%2d]: ", i);
        int count = 0;
        for (Entry *e = ht->buckets[i]; e; e = e->next) {
            if (count > 0) printf(" -> ");
            printf("(%s,%d)", e->key, e->value);
            count++;
        }
        if (count == 0) printf("(empty)");
        printf("\n");
    }
}

void ht_stats(HashTable *ht) {
    int max_chain = 0, empty = 0;
    int total_len = 0;
    for (int i = 0; i < ht->m; i++) {
        int len = 0;
        for (Entry *e = ht->buckets[i]; e; e = e->next)
            len++;
        if (len > max_chain) max_chain = len;
        if (len == 0) empty++;
        total_len += len;
    }
    printf("  n=%d, m=%d, alpha=%.2f\n", ht->n, ht->m,
           (double)ht->n / ht->m);
    printf("  empty buckets: %d/%d (%.0f%%)\n",
           empty, ht->m, 100.0 * empty / ht->m);
    printf("  max chain: %d\n", max_chain);
    printf("  avg chain (non-empty): %.2f\n",
           ht->m - empty > 0 ? (double)total_len / (ht->m - empty) : 0);
}

void ht_free(HashTable *ht) {
    for (int i = 0; i < ht->m; i++) {
        Entry *e = ht->buckets[i];
        while (e) {
            Entry *next = e->next;
            free(e->key);
            free(e);
            e = next;
        }
    }
    free(ht->buckets);
    free(ht);
}

// --- generic hash table ---

typedef struct GenEntry {
    void *key;
    void *value;
    struct GenEntry *next;
} GenEntry;

typedef unsigned int (*HashFunc)(const void *, int);
typedef int (*CmpFunc)(const void *, const void *);
typedef void (*FreeFunc)(void *);

typedef struct {
    GenEntry **buckets;
    int m, n;
    HashFunc hash_fn;
    CmpFunc cmp_fn;
    size_t key_size, val_size;
} GenHashTable;

GenHashTable *gen_create(int m, HashFunc hf, CmpFunc cf,
                          size_t ks, size_t vs) {
    GenHashTable *ht = malloc(sizeof(GenHashTable));
    ht->m = m; ht->n = 0;
    ht->buckets = calloc(m, sizeof(GenEntry *));
    ht->hash_fn = hf; ht->cmp_fn = cf;
    ht->key_size = ks; ht->val_size = vs;
    return ht;
}

void gen_insert(GenHashTable *ht, const void *key, const void *value) {
    unsigned int i = ht->hash_fn(key, ht->m);

    for (GenEntry *e = ht->buckets[i]; e; e = e->next) {
        if (ht->cmp_fn(e->key, key) == 0) {
            memcpy(e->value, value, ht->val_size);
            return;
        }
    }

    GenEntry *e = malloc(sizeof(GenEntry));
    e->key = malloc(ht->key_size);
    e->value = malloc(ht->val_size);
    memcpy(e->key, key, ht->key_size);
    memcpy(e->value, value, ht->val_size);
    e->next = ht->buckets[i];
    ht->buckets[i] = e;
    ht->n++;
}

void *gen_search(GenHashTable *ht, const void *key) {
    unsigned int i = ht->hash_fn(key, ht->m);
    for (GenEntry *e = ht->buckets[i]; e; e = e->next) {
        if (ht->cmp_fn(e->key, key) == 0)
            return e->value;
    }
    return NULL;
}

void gen_free(GenHashTable *ht) {
    for (int i = 0; i < ht->m; i++) {
        GenEntry *e = ht->buckets[i];
        while (e) {
            GenEntry *next = e->next;
            free(e->key); free(e->value); free(e);
            e = next;
        }
    }
    free(ht->buckets);
    free(ht);
}

// --- hash/cmp for generic table ---

unsigned int hash_int_gen(const void *key, int m) {
    int k = *(const int *)key;
    return ((unsigned int)k) % m;
}

int cmp_int_gen(const void *a, const void *b) {
    return *(const int *)a - *(const int *)b;
}

// --- demos ---

int main(void) {
    // demo 1: basic operations
    printf("=== Demo 1: basic operations ===\n");
    HashTable *ht = ht_create(7);

    ht_insert(ht, "alice", 100);
    ht_insert(ht, "bob", 200);
    ht_insert(ht, "carol", 300);
    ht_insert(ht, "dave", 400);
    ht_insert(ht, "eve", 500);
    ht_insert(ht, "frank", 600);
    ht_insert(ht, "grace", 700);

    printf("Table (m=7, n=7):\n");
    ht_print(ht);
    ht_stats(ht);
    printf("\n");

    // demo 2: search
    printf("=== Demo 2: search with comparison count ===\n");
    const char *queries[] = {"alice", "carol", "frank", "grace", "xyz", "bob"};
    for (int i = 0; i < 6; i++) {
        int comps;
        int found = ht_search_counted(ht, queries[i], &comps);
        printf("  search(\"%s\"): %s, %d comparison(s)\n",
               queries[i], found ? "FOUND" : "NOT FOUND", comps);
    }
    printf("\n");

    // demo 3: update and delete
    printf("=== Demo 3: update and delete ===\n");
    ht_insert(ht, "alice", 999);  // update
    int val;
    ht_search(ht, "alice", &val);
    printf("  alice updated to %d\n", val);

    ht_delete(ht, "carol");
    printf("  carol deleted\n");
    printf("  search(carol): %s\n",
           ht_search(ht, "carol", NULL) ? "FOUND" : "NOT FOUND");
    printf("  n=%d (was 7)\n\n", ht->n);

    ht_free(ht);

    // demo 4: alpha effect
    printf("=== Demo 4: alpha effect on comparisons ===\n");
    printf("  %6s  %8s  %8s  %8s\n", "alpha", "n", "avg_hit", "avg_miss");

    int m_test = 997;  // prime
    for (int n = 100; n <= 5000; n += 100) {
        HashTable *t = ht_create(m_test);
        char key[32];

        // insert n random keys
        for (int i = 0; i < n; i++) {
            snprintf(key, sizeof(key), "key_%d", rand() % 1000000);
            ht_insert(t, key, i);
        }

        // measure hit
        double total_hit = 0;
        int hits = 0;
        for (int i = 0; i < t->m && hits < 500; i++) {
            for (Entry *e = t->buckets[i]; e && hits < 500; e = e->next) {
                int c;
                ht_search_counted(t, e->key, &c);
                total_hit += c;
                hits++;
            }
        }

        // measure miss
        double total_miss = 0;
        for (int i = 0; i < 500; i++) {
            snprintf(key, sizeof(key), "miss_%d", i);
            int c;
            ht_search_counted(t, key, &c);
            total_miss += c;
        }

        if (n % 500 == 0) {
            printf("  %6.2f  %8d  %8.2f  %8.2f\n",
                   (double)t->n / t->m, t->n,
                   total_hit / hits, total_miss / 500);
        }
        ht_free(t);
    }
    printf("\n");

    // demo 5: generic hash table (int -> int)
    printf("=== Demo 5: generic hash table (int -> int) ===\n");
    GenHashTable *gt = gen_create(101, hash_int_gen, cmp_int_gen,
                                   sizeof(int), sizeof(int));

    for (int i = 0; i < 10; i++) {
        int val2 = i * 100;
        gen_insert(gt, &i, &val2);
    }

    for (int i = 0; i < 12; i++) {
        int *result = gen_search(gt, &i);
        if (result)
            printf("  get(%d) = %d\n", i, *result);
        else
            printf("  get(%d) = NOT FOUND\n", i);
    }
    gen_free(gt);
    printf("\n");

    // demo 6: chain length distribution
    printf("=== Demo 6: chain length distribution ===\n");
    HashTable *big = ht_create(1000);
    char buf[32];
    for (int i = 0; i < 1000; i++) {
        snprintf(buf, sizeof(buf), "word_%d", i);
        ht_insert(big, buf, i);
    }

    int dist[20] = {0};
    for (int i = 0; i < big->m; i++) {
        int len = 0;
        for (Entry *e = big->buckets[i]; e; e = e->next) len++;
        if (len < 20) dist[len]++;
    }

    printf("  alpha = %.2f\n", (double)big->n / big->m);
    printf("  Chain length distribution:\n");
    for (int i = 0; i < 10; i++) {
        printf("    len %d: %4d buckets", i, dist[i]);
        printf("  ");
        for (int j = 0; j < dist[i] && j < 50; j++) printf("#");
        printf("\n");
    }

    ht_stats(big);
    ht_free(big);

    return 0;
}
```

Compilar y ejecutar:

```bash
gcc -std=c11 -Wall -Wextra -O2 -o chaining chaining.c
./chaining
```

---

## Programa completo en Rust

```rust
// chaining.rs — hash table with separate chaining (manual + HashMap)
use std::collections::HashMap;

// --- manual chaining hash table ---

struct ChainEntry {
    key: String,
    value: i32,
}

struct ChainTable {
    buckets: Vec<Vec<ChainEntry>>,  // Vec per bucket instead of linked list
    m: usize,
    n: usize,
}

impl ChainTable {
    fn new(capacity: usize) -> Self {
        ChainTable {
            buckets: (0..capacity).map(|_| Vec::new()).collect(),
            m: capacity,
            n: 0,
        }
    }

    fn hash(&self, key: &str) -> usize {
        let mut h: u32 = 5381;
        for b in key.bytes() {
            h = h.wrapping_mul(33).wrapping_add(b as u32);
        }
        h as usize % self.m
    }

    fn insert(&mut self, key: &str, value: i32) {
        let i = self.hash(key);
        // check for existing key
        for entry in &mut self.buckets[i] {
            if entry.key == key {
                entry.value = value;
                return;
            }
        }
        self.buckets[i].push(ChainEntry {
            key: key.to_string(),
            value,
        });
        self.n += 1;
    }

    fn search(&self, key: &str) -> Option<(i32, usize)> {
        let i = self.hash(key);
        let mut comps = 0;
        for entry in &self.buckets[i] {
            comps += 1;
            if entry.key == key {
                return Some((entry.value, comps));
            }
        }
        Some((-1, comps)).filter(|_| false)  // None with comps lost
            .or_else(|| { let _ = comps; None })
    }

    fn search_counted(&self, key: &str) -> (bool, usize) {
        let i = self.hash(key);
        let mut comps = 0;
        for entry in &self.buckets[i] {
            comps += 1;
            if entry.key == key {
                return (true, comps);
            }
        }
        (false, comps)
    }

    fn get(&self, key: &str) -> Option<i32> {
        let i = self.hash(key);
        self.buckets[i].iter()
            .find(|e| e.key == key)
            .map(|e| e.value)
    }

    fn delete(&mut self, key: &str) -> bool {
        let i = self.hash(key);
        if let Some(pos) = self.buckets[i].iter().position(|e| e.key == key) {
            self.buckets[i].swap_remove(pos);
            self.n -= 1;
            true
        } else {
            false
        }
    }

    fn alpha(&self) -> f64 {
        self.n as f64 / self.m as f64
    }

    fn stats(&self) {
        let max_chain = self.buckets.iter().map(|b| b.len()).max().unwrap_or(0);
        let empty = self.buckets.iter().filter(|b| b.is_empty()).count();
        let non_empty = self.m - empty;
        let avg = if non_empty > 0 { self.n as f64 / non_empty as f64 } else { 0.0 };

        println!("  n={}, m={}, alpha={:.2}", self.n, self.m, self.alpha());
        println!("  empty: {}/{} ({:.0}%)", empty, self.m,
                 100.0 * empty as f64 / self.m as f64);
        println!("  max chain: {}", max_chain);
        println!("  avg chain (non-empty): {:.2}", avg);
    }
}

fn main() {
    // demo 1: basic operations
    println!("=== Demo 1: manual chaining table ===");
    let mut table = ChainTable::new(7);

    for (name, val) in &[("alice",100), ("bob",200), ("carol",300),
                          ("dave",400), ("eve",500), ("frank",600), ("grace",700)] {
        table.insert(name, *val);
    }

    println!("Table (m=7):");
    for (i, bucket) in table.buckets.iter().enumerate() {
        let entries: Vec<String> = bucket.iter()
            .map(|e| format!("({},{})", e.key, e.value))
            .collect();
        println!("  [{}]: {}", i,
                 if entries.is_empty() { "(empty)".to_string() }
                 else { entries.join(" -> ") });
    }
    table.stats();
    println!();

    // demo 2: search with comparison count
    println!("=== Demo 2: search ===");
    for query in &["alice", "carol", "frank", "xyz", "bob"] {
        let (found, comps) = table.search_counted(query);
        println!("  search(\"{}\"): {}, {} comparison(s)",
                 query, if found { "FOUND" } else { "NOT FOUND" }, comps);
    }
    println!();

    // demo 3: delete
    println!("=== Demo 3: delete ===");
    table.insert("alice", 999);
    println!("  alice updated to {:?}", table.get("alice"));
    table.delete("carol");
    println!("  carol deleted: {:?}", table.get("carol"));
    println!("  n={}\n", table.n);

    // demo 4: alpha effect
    println!("=== Demo 4: alpha effect on comparisons ===");
    println!("  {:>6} {:>6} {:>8} {:>8} {:>8} {:>8}",
             "alpha", "n", "avg_hit", "avg_miss", "th_hit", "th_miss");

    let m = 997;
    for &n in &[100, 250, 500, 750, 1000, 1500, 2000, 3000, 5000] {
        let mut t = ChainTable::new(m);
        let mut rng: u64 = n as u64 * 31337;
        for _ in 0..n {
            rng = rng.wrapping_mul(6364136223846793005).wrapping_add(1);
            let key = format!("k{}", rng >> 16);
            t.insert(&key, 0);
        }

        // hit
        let mut total_hit = 0usize;
        let mut hits = 0;
        for bucket in &t.buckets {
            for entry in bucket {
                let (_, c) = t.search_counted(&entry.key);
                total_hit += c;
                hits += 1;
                if hits >= 500 { break; }
            }
            if hits >= 500 { break; }
        }

        // miss
        let mut total_miss = 0usize;
        for i in 0..500 {
            let key = format!("miss_{}", i);
            let (_, c) = t.search_counted(&key);
            total_miss += c;
        }

        let alpha = t.alpha();
        println!("  {:>6.2} {:>6} {:>8.2} {:>8.2} {:>8.2} {:>8.2}",
                 alpha, t.n,
                 total_hit as f64 / hits as f64,
                 total_miss as f64 / 500.0,
                 1.0 + alpha / 2.0,
                 alpha);
    }
    println!();

    // demo 5: chain length distribution
    println!("=== Demo 5: chain length distribution ===");
    let mut big = ChainTable::new(1000);
    for i in 0..1000 {
        big.insert(&format!("word_{}", i), i as i32);
    }

    let mut dist = [0u32; 10];
    for bucket in &big.buckets {
        let len = bucket.len().min(9);
        dist[len] += 1;
    }
    println!("  alpha = {:.2}", big.alpha());
    for i in 0..6 {
        let bar: String = (0..dist[i].min(50)).map(|_| '#').collect();
        println!("    len {}: {:>4} buckets  {}", i, dist[i], bar);
    }
    big.stats();
    println!();

    // demo 6: Rust HashMap comparison
    println!("=== Demo 6: Rust HashMap (uses Swiss Table internally) ===");
    let mut hm: HashMap<String, i32> = HashMap::new();
    for i in 0..1000 {
        hm.insert(format!("word_{}", i), i);
    }

    println!("  len={}, capacity={}", hm.len(), hm.capacity());
    println!("  alpha={:.3}", hm.len() as f64 / hm.capacity() as f64);
    println!("  get(word_500) = {:?}", hm.get("word_500"));
    println!("  get(word_9999) = {:?}", hm.get("word_9999"));

    // entry API (idiomatic Rust for insert-or-update)
    println!("\n  Entry API:");
    let mut counts: HashMap<&str, i32> = HashMap::new();
    let text = "the cat sat on the mat the cat";
    for word in text.split_whitespace() {
        *counts.entry(word).or_insert(0) += 1;
    }
    println!("  Word counts: {:?}", counts);

    // demo 7: Vec<Vec<T>> as manual chaining (idiomatic Rust pattern)
    println!("\n=== Demo 7: Vec<Vec<T>> chaining pattern ===");
    let m_small = 5;
    let mut chain_vec: Vec<Vec<(String, i32)>> = vec![vec![]; m_small];

    let items = [("a", 1), ("b", 2), ("c", 3), ("d", 4), ("e", 5),
                 ("f", 6), ("g", 7)];
    for &(k, v) in &items {
        let h = k.bytes().next().unwrap() as usize % m_small;
        chain_vec[h].push((k.to_string(), v));
    }

    for (i, bucket) in chain_vec.iter().enumerate() {
        let entries: Vec<String> = bucket.iter()
            .map(|(k, v)| format!("({},{})", k, v))
            .collect();
        println!("  [{}]: {}", i,
                 if entries.is_empty() { "(empty)".into() }
                 else { entries.join(" -> ") });
    }
}
```

Compilar y ejecutar:

```bash
rustc chaining.rs && ./chaining
```

---

## Errores frecuentes

1. **No verificar duplicados en inserción**: insertar sin buscar
   primero crea duplicados. Búsquedas posteriores pueden encontrar
   el "viejo" o el "nuevo" según la posición en la lista, dando
   resultados inconsistentes.
2. **Olvidar `strdup` para la clave**: almacenar el puntero
   original a la clave sin copiarla. Si el caller modifica o libera
   el string, la tabla se corrompe.
3. **Liberar la clave sin `strdup`**: si la clave fue copiada con
   `strdup`, hay que liberarla con `free`. Si no fue copiada, no
   liberar. Mezclar ambos casos causa double-free o use-after-free.
4. **Usar `==` para comparar strings en C**: `e->key == key`
   compara punteros, no contenidos. Siempre usar `strcmp`.
5. **No actualizar `n` al eliminar**: decrementar `n` es esencial
   para que $\alpha$ sea correcto y el redimensionamiento funcione.

---

## Ejercicios

### Ejercicio 1 — Traza de inserción

Inserta las claves `{"ant", "bat", "cat", "dog", "eel", "fox",
"gnu", "hen"}` en una tabla de chaining con $m = 5$ usando
`djb2`. Dibuja la tabla final, indica $\alpha$, el número de
colisiones, y la longitud máxima de cadena.

<details><summary>Predice antes de ejecutar</summary>

Con 8 elementos y 5 buckets ($\alpha = 1.6$), ¿cuántos buckets
tendrán cadena de longitud $\geq 2$? ¿Alguno tendrá longitud 3?

</details>

### Ejercicio 2 — Traza de búsqueda y eliminación

Partiendo de la tabla del ejercicio 1, traza paso a paso:
1. Buscar "cat" (éxito).
2. Buscar "pig" (fracaso).
3. Eliminar "bat".
4. Buscar "bat" de nuevo.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas comparaciones requiere cada búsqueda? ¿La eliminación
de "bat" afecta la búsqueda de alguna otra clave?

</details>

### Ejercicio 3 — Move-to-front

Implementa la variante move-to-front. Inserta 100 claves,
luego busca solo 5 de ellas repetidamente (1000 búsquedas
concentradas en esas 5). Compara el promedio de comparaciones
con la versión sin move-to-front.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas comparaciones ahorra move-to-front en promedio?
¿Merece la pena si las búsquedas están distribuidas
uniformemente (no concentradas)?

</details>

### Ejercicio 4 — Lista ordenada vs desordenada

Implementa la variante con lista ordenada por clave. Compara
las comparaciones de búsqueda fallida con la versión desordenada
para $\alpha = 1.0$ y 1000 búsquedas fallidas.

<details><summary>Predice antes de ejecutar</summary>

¿La lista ordenada reduce las comparaciones de miss a la mitad?
¿El costo extra de inserción ordenada compensa el ahorro en miss?

</details>

### Ejercicio 5 — Tabla genérica: `Point → String`

Usando la tabla genérica con `void *`, implementa una tabla que
mapee structs `Point {int x, y}` a strings. Implementa las
funciones `hash_point` y `cmp_point`.

<details><summary>Predice antes de ejecutar</summary>

¿Qué pasa si hasheas el struct como bloque de bytes y hay padding?
¿Cómo se comparan dos `Point` correctamente?

</details>

### Ejercicio 6 — Overhead de memoria

Para 10000 strings de longitud promedio 10, calcula el overhead de
memoria de chaining vs un array simple: (a) puntero `next` por
nodo, (b) puntero `key` duplicado, (c) array de buckets. ¿Cuántos
bytes extra por elemento?

<details><summary>Predice antes de ejecutar</summary>

En 64-bit: `next` = 8 bytes, `key` = 8 bytes (puntero) +
~11 bytes (strdup). ¿El overhead total es ~20%, ~50% o ~100%?

</details>

### Ejercicio 7 — Verificar distribución uniforme

Inserta $10^4$ claves en una tabla de $m = 997$ y calcula el
estadístico $\chi^2$ de la distribución de longitudes de cadena.
Compara con el valor esperado ($\chi^2 \approx m-1 = 996$).

<details><summary>Predice antes de ejecutar</summary>

Si djb2 es una buena función hash, ¿$\chi^2$ estará cerca de
996? ¿Un $\chi^2$ mucho mayor indica un problema?

</details>

### Ejercicio 8 — Redimensionamiento manual

Implementa `ht_resize(HashTable *ht, int new_m)` que crea una
tabla nueva, reinserta todos los elementos, y libera la vieja.
Llama a `ht_resize` cuando $\alpha > 0.75$ dentro de `ht_insert`.
Verifica que las búsquedas siguen funcionando después del resize.

<details><summary>Predice antes de ejecutar</summary>

Después de un resize de $m=100$ a $m=200$, ¿las claves cambian
de bucket? ¿Hay que recalcular todos los hashes?

</details>

### Ejercicio 9 — HashMap entry API en Rust

Implementa un contador de bigramas (pares de palabras
consecutivas) en un texto usando `HashMap` con la entry API.
Para el texto "the cat sat on the mat the cat", reporta los
bigramas más frecuentes.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántos bigramas únicos hay en ese texto? ¿Cuál es el más
frecuente?

</details>

### Ejercicio 10 — Benchmark: chaining vs HashMap

Para $10^5$ strings aleatorios, compara tu implementación manual
de chaining (en Rust con `Vec<Vec<>>`) contra `HashMap`. Mide
tiempo de inserción y tiempo de 10000 búsquedas.

<details><summary>Predice antes de ejecutar</summary>

¿Cuántas veces más rápido será `HashMap` (Swiss Table) que
el chaining manual? ¿1.5x, 3x o 10x?

</details>
