# Hash tables en C

## Objetivo

Estudiar las opciones disponibles para usar hash tables en C —
un lenguaje sin hash table en su biblioteca estándar (a diferencia
de Rust, Python, Java, Go):

- **POSIX `hsearch`**: la API mínima del estándar, sus
  limitaciones severas.
- **GLib `GHashTable`**: hash table genérica, completa, parte del
  ecosistema GNOME/GTK.
- **uthash**: macros header-only que convierten cualquier struct
  en un hash table sin alocaciones separadas.
- **khash**: otra librería header-only, minimalista, usada en
  samtools/htslib.
- **Implementación propia vs librería**: cuándo vale la pena cada
  opción, trade-offs de mantenimiento y rendimiento.
- Referencia: S02 implementó tablas hash propias (chaining y open
  addressing). Aquí evaluamos soluciones existentes para código de
  producción.

---

## POSIX hsearch: lo mínimo

La familia `hsearch` está definida en `<search.h>` y forma parte
de POSIX (no del estándar C puro). Incluye:

```c
#include <search.h>

// create global hash table with nel entries max
int hcreate(size_t nel);

// search or insert
ENTRY *hsearch(ENTRY item, ACTION action);
// ENTRY = { char *key; void *data; }
// ACTION = FIND or ENTER

// destroy global table
void hdestroy(void);
```

### Limitaciones

| Limitación | Impacto |
|------------|---------|
| **Una sola tabla global** | No se pueden tener dos tablas simultáneas |
| **Tamaño fijo** | No crece; se define en `hcreate` y no puede cambiar |
| **No se puede eliminar** | No hay `hdelete` — solo insertar y buscar |
| **Solo claves `char *`** | No soporta claves enteras o structs |
| **No es thread-safe** | Variable global compartida |
| **No es estándar C** | POSIX, no disponible en Windows nativo |

### Extensión GNU: hsearch_r

GNU libc (glibc) ofrece versiones reentrantes que solucionan la
tabla global:

```c
#include <search.h>

struct hsearch_data htab = {0};
hcreate_r(100, &htab);

ENTRY item = { .key = "hello", .data = (void *)42 };
ENTRY *result;
hsearch_r(item, ENTER, &result, &htab);

hsearch_r((ENTRY){ .key = "hello" }, FIND, &result, &htab);
printf("%s: %ld\n", result->key, (long)result->data);

hdestroy_r(&htab);
```

Esto permite múltiples tablas, pero sigue sin soportar
eliminación ni redimensionamiento. `hsearch_r` es extensión GNU,
no portable.

### Veredicto

`hsearch` es útil solo para prototipos rápidos o scripts en C
donde se necesita un mapeo temporal de strings. Para cualquier
cosa seria, se necesita una librería o implementación propia.

---

## GLib GHashTable

GLib es la librería de utilidades de bajo nivel del ecosistema
GNOME. `GHashTable` es su hash table genérica, madura y
extensamente probada.

### Características

- **Claves y valores genéricos** via `gpointer` (`void *`).
- **Funciones de hash y comparación pluggables** — el usuario
  las proporciona al crear la tabla.
- **Funciones de destrucción** para claves y valores.
- **Redimensionamiento automático** (grow y shrink).
- **Iteración** con `g_hash_table_iter_*` o `g_hash_table_foreach`.
- **API completa**: insert, lookup, remove, contains, size,
  steal (remove sin destruir), replace.
- Implementación interna: **open addressing** con Robin Hood
  hashing (desde GLib 2.40).

### API

```c
#include <glib.h>

// creation
GHashTable *table = g_hash_table_new(g_str_hash, g_str_equal);

// with destructors for key and value
GHashTable *table = g_hash_table_new_full(
    g_str_hash,     // hash function
    g_str_equal,    // key comparison
    g_free,         // key destructor
    g_free          // value destructor
);

// insert (copies pointer, not data — use g_strdup for strings)
g_hash_table_insert(table, g_strdup("key"), g_strdup("value"));

// lookup
gpointer val = g_hash_table_lookup(table, "key");
if (val) printf("found: %s\n", (char *)val);

// contains
gboolean exists = g_hash_table_contains(table, "key");

// remove (calls destructors)
g_hash_table_remove(table, "key");

// size
guint size = g_hash_table_size(table);

// iteration
GHashTableIter iter;
gpointer key, value;
g_hash_table_iter_init(&iter, table);
while (g_hash_table_iter_next(&iter, &key, &value)) {
    printf("%s: %s\n", (char *)key, (char *)value);
}

// foreach (callback-based)
void print_entry(gpointer key, gpointer value, gpointer user_data) {
    printf("  %s: %s\n", (char *)key, (char *)value);
}
g_hash_table_foreach(table, print_entry, NULL);

// destroy
g_hash_table_unref(table);  // or g_hash_table_destroy(table)
```

### Funciones hash incluidas

GLib provee funciones hash y comparación para tipos comunes:

| Tipo | Hash function | Compare function |
|------|--------------|-----------------|
| `char *` | `g_str_hash` | `g_str_equal` |
| `int` (como pointer) | `g_direct_hash` | `g_direct_equal` |
| `gint64` | `g_int64_hash` | `g_int64_equal` |
| `gdouble` | `g_double_hash` | `g_double_equal` |

Para enteros pequeños, se puede usar el truco de `GINT_TO_POINTER`
y `g_direct_hash`:

```c
// integer keys without allocation
g_hash_table_insert(table, GINT_TO_POINTER(42), GINT_TO_POINTER(100));
gint val = GPOINTER_TO_INT(g_hash_table_lookup(table, GINT_TO_POINTER(42)));
```

### Ventajas y desventajas

| Ventaja | Desventaja |
|---------|------------|
| API completa y probada | Dependencia externa (libglib-2.0) |
| Gestión automática de memoria | `void *` pierde type safety |
| Redimensionamiento automático | API verbosa (prefijo `g_hash_table_`) |
| Robin Hood hashing (buen rendimiento) | Overhead de la capa de abstracción |
| Documentación excelente | No es header-only (requiere linkear) |

Compilación:

```bash
gcc -o prog prog.c $(pkg-config --cflags --libs glib-2.0)
```

---

## uthash: macros intrusivas header-only

**uthash** es una librería header-only (`uthash.h`, un solo
archivo) que convierte cualquier struct de C en un hash table
mediante macros. Es la solución más popular para hash tables en C
por su simplicidad.

### El modelo intrusivo

A diferencia de GLib (donde la tabla es un contenedor separado),
uthash **modifica la estructura del usuario** añadiendo un campo
`UT_hash_handle`:

```c
#include "uthash.h"

typedef struct {
    int id;              // key
    char name[32];       // value
    UT_hash_handle hh;   // makes this struct hashable
} User;

User *users = NULL;  // the hash table IS a pointer
```

El "hash table" es simplemente un puntero al primer elemento. No
hay una estructura contenedora separada — la metadata está dentro
del `UT_hash_handle` embebido en cada struct.

### API (macros)

```c
// add (key must be unique — check first!)
User *user = malloc(sizeof(User));
user->id = 42;
strcpy(user->name, "Alice");
HASH_ADD_INT(users, id, user);

// find
User *found;
int key = 42;
HASH_FIND_INT(users, &key, found);
if (found) printf("found: %s\n", found->name);

// delete (does NOT free — you must free)
HASH_DEL(users, found);
free(found);

// count
int count = HASH_COUNT(users);

// iterate
User *current, *tmp;
HASH_ITER(hh, users, current, tmp) {
    printf("  id=%d, name=%s\n", current->id, current->name);
}

// sort (in-place)
int name_sort(User *a, User *b) {
    return strcmp(a->name, b->name);
}
HASH_SORT(users, name_sort);

// delete all
HASH_ITER(hh, users, current, tmp) {
    HASH_DEL(users, current);
    free(current);
}
```

### Variantes de HASH_ADD/FIND por tipo de clave

| Clave | Add | Find |
|-------|-----|------|
| `int` | `HASH_ADD_INT(head, field, ptr)` | `HASH_FIND_INT(head, keyptr, out)` |
| `char *` (ptr) | `HASH_ADD_KEYPTR(head, field, key, keylen, ptr)` | `HASH_FIND_STR(head, key, out)` |
| `char[]` (inline) | `HASH_ADD_STR(head, field, ptr)` | `HASH_FIND_STR(head, key, out)` |
| struct/blob | `HASH_ADD(head, field, keylen, ptr)` | `HASH_FIND(head, field, keylen, keyptr, out)` |

### Internos

uthash usa **chaining con listas** por defecto. Internamente
mantiene un array de buckets y cada entrada del usuario se enlaza
con punteros dentro de `UT_hash_handle`. Soporta
redimensionamiento automático (potencia de 2 más cercana, umbral
configurable).

La función hash por defecto es una variante de Jenkins one-at-a-time,
pero se puede cambiar:

```c
#define HASH_FUNCTION HASH_BER   // Bernstein (djb2)
#define HASH_FUNCTION HASH_FNV   // FNV-1a
#define HASH_FUNCTION HASH_JEN   // Jenkins (default)
#define HASH_FUNCTION HASH_SFH   // SuperFastHash
#include "uthash.h"
```

### Ventajas y desventajas

| Ventaja | Desventaja |
|---------|------------|
| Header-only (cero dependencias) | Macros complejas (debugging difícil) |
| Sin alocaciones extras (intrusivo) | `UT_hash_handle` añade ~56 bytes/entry |
| API simple para casos básicos | No type-safe (macros operan sobre tipos) |
| Sort integrado | Claves deben ser únicas (HASH_ADD falla silenciosamente con dups) |
| Comunidad grande, bien probado | Sin variante thread-safe |

---

## khash: header-only minimalista

**khash** (de Attractive Chaos, usada en samtools/htslib) es una
librería header-only que genera código tipado mediante macros
genéricas. A diferencia de uthash, usa **open addressing** y genera
funciones específicas por tipo.

### Modelo

```c
#include "khash.h"

// declare a hash map type: name, key_type, value_type
KHASH_MAP_INIT_INT(m32, int)  // name=m32, key=int, value=int

// declare a hash set type
KHASH_SET_INIT_INT(s32)  // name=s32, key=int (no value)

// for string keys
KHASH_MAP_INIT_STR(mstr, int)  // name=mstr, key=char*, value=int
```

### API

```c
khash_t(m32) *h = kh_init(m32);

// insert
int ret;
khint_t k = kh_put(m32, h, 42, &ret);
// ret: 0 = key existed, 1 = new bucket, 2 = deleted bucket reused
kh_value(h, k) = 100;  // set value

// lookup
k = kh_get(m32, h, 42);
if (k != kh_end(h)) {
    printf("value: %d\n", kh_value(h, k));
}

// delete
kh_del(m32, h, k);

// iterate
for (k = kh_begin(h); k != kh_end(h); ++k) {
    if (kh_exist(h, k)) {
        printf("  %d: %d\n", kh_key(h, k), kh_value(h, k));
    }
}

// size
khint_t size = kh_size(h);

// destroy
kh_destroy(m32, h);
```

### Ventajas y desventajas

| Ventaja | Desventaja |
|---------|------------|
| Header-only | API poco intuitiva (`kh_put` + `kh_value` = dos pasos) |
| Open addressing (mejor cache) | Macros generan código inline (binary bloat) |
| Tipado generado (mejor rendimiento) | Documentación escasa |
| Muy rápido (usado en bioinformática) | Menos features que GLib (sin foreach) |
| Soporta sets y maps | Gestión de memoria manual |

---

## Comparación de librerías

| Criterio | hsearch | GLib | uthash | khash | Propia |
|----------|---------|------|--------|-------|--------|
| Estándar | POSIX | No | No | No | — |
| Dependencia | libc | libglib-2.0 | header | header | — |
| Tipo de resolución | impl. defined | open (Robin Hood) | chaining | open | cualquiera |
| Type safety | Baja (`char*` only) | Baja (`void*`) | Baja (macros) | Media (macros tipadas) | Alta (tú decides) |
| Redimensiona | No | Sí | Sí | Sí | Tú decides |
| Eliminación | No | Sí | Sí | Sí | Tú decides |
| Tamaño de código | 0 (libc) | Grande (linkear) | ~1500 líneas | ~600 líneas | Variable |
| Rendimiento | Bajo | Bueno | Medio | Muy bueno | Variable |
| Uso en producción | Legacy | GNOME, GTK | Muy extendido | Bioinformática | Común |
| Thread-safe | No | Con locks | No | No | Tú decides |

---

## Implementación propia vs librería

### Cuándo usar una librería

- **Siempre que sea posible**. Una librería madura como GLib o
  uthash tiene años de testing, bug fixes y optimizaciones. Tu
  implementación propia tendrá bugs que tardarás semanas en
  descubrir.
- El proyecto ya depende de GLib → usar `GHashTable`.
- Necesitas una hash table rápido para un programa corto → uthash.
- Rendimiento crítico con datos numéricos → khash.

### Cuándo implementar propia

- **Restricciones de dependencias**: embedded, kernel, o
  política del proyecto que prohíbe librerías externas.
- **Requisitos especializados**: tabla lock-free, persistente en
  disco, con semántica de caducidad (TTL), estructura de datos
  híbrida.
- **Aprendizaje**: estás tomando este curso.
- **Tipos específicos**: tabla con claves compuestas y
  comparación personalizada donde el overhead de `void *` es
  inaceptable.

### Tabla de decisión

```
¿Puedes añadir dependencias?
  ├── No → ¿Header-only es aceptable?
  │         ├── Sí → uthash (simple) o khash (rápido)
  │         └── No → Implementación propia
  └── Sí → ¿Ya usas GLib?
            ├── Sí → GHashTable
            └── No → ¿Vale la pena la dependencia?
                      ├── Proyecto grande → Sí, GLib
                      └── Script/herramienta → uthash
```

---

## Programa completo en C

Como uthash y GLib requieren librerías externas (aunque uthash es
solo un header), el programa implementa una **hash table genérica
con API similar a GLib** usando solo la biblioteca estándar. Luego
se muestra el contraste con la API de uthash y hsearch.

```c
// hash_tables_c.c
// Hash table options in C: custom implementation, hsearch, and
// uthash-style API
// Compile: gcc -O2 -o hash_tables_c hash_tables_c.c

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <search.h>  // POSIX hsearch

// ============================================================
// Custom generic hash table (GLib-like API)
// ============================================================

typedef unsigned int (*HashFunc)(const void *key);
typedef bool (*EqualFunc)(const void *a, const void *b);
typedef void (*DestroyFunc)(void *data);
typedef void (*ForeachFunc)(const void *key, void *value, void *user_data);

typedef struct HEntry {
    void *key;
    void *value;
    struct HEntry *next;
} HEntry;

typedef struct {
    HEntry **buckets;
    int capacity;
    int count;
    HashFunc hash_fn;
    EqualFunc equal_fn;
    DestroyFunc key_destroy;
    DestroyFunc value_destroy;
} GenericMap;

// hash functions
unsigned int str_hash(const void *key) {
    unsigned int h = 5381;
    const char *s = (const char *)key;
    int c;
    while ((c = (unsigned char)*s++)) {
        h = h * 33 + c;
    }
    return h;
}

unsigned int int_hash(const void *key) {
    unsigned int h = *(const int *)key;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h;
}

// comparison functions
bool str_equal(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b) == 0;
}

bool int_equal(const void *a, const void *b) {
    return *(const int *)a == *(const int *)b;
}

// constructors
GenericMap *gm_new(HashFunc hf, EqualFunc ef) {
    GenericMap *m = malloc(sizeof(GenericMap));
    m->capacity = 16;
    m->buckets = calloc(m->capacity, sizeof(HEntry *));
    m->count = 0;
    m->hash_fn = hf;
    m->equal_fn = ef;
    m->key_destroy = NULL;
    m->value_destroy = NULL;
    return m;
}

GenericMap *gm_new_full(HashFunc hf, EqualFunc ef,
                        DestroyFunc kd, DestroyFunc vd) {
    GenericMap *m = gm_new(hf, ef);
    m->key_destroy = kd;
    m->value_destroy = vd;
    return m;
}

static void gm_resize(GenericMap *m, int new_cap) {
    HEntry **old = m->buckets;
    int old_cap = m->capacity;

    m->buckets = calloc(new_cap, sizeof(HEntry *));
    m->capacity = new_cap;
    m->count = 0;

    for (int i = 0; i < old_cap; i++) {
        HEntry *e = old[i];
        while (e) {
            HEntry *next = e->next;
            unsigned int idx = m->hash_fn(e->key) % new_cap;
            e->next = m->buckets[idx];
            m->buckets[idx] = e;
            m->count++;
            e = next;
        }
    }
    free(old);
}

// insert or replace
bool gm_insert(GenericMap *m, void *key, void *value) {
    if ((double)(m->count + 1) / m->capacity > 0.75) {
        gm_resize(m, m->capacity * 2);
    }

    unsigned int idx = m->hash_fn(key) % m->capacity;
    for (HEntry *e = m->buckets[idx]; e; e = e->next) {
        if (m->equal_fn(e->key, key)) {
            // replace
            if (m->key_destroy) m->key_destroy(e->key);
            if (m->value_destroy) m->value_destroy(e->value);
            e->key = key;
            e->value = value;
            return false;  // replaced
        }
    }

    HEntry *e = malloc(sizeof(HEntry));
    e->key = key;
    e->value = value;
    e->next = m->buckets[idx];
    m->buckets[idx] = e;
    m->count++;
    return true;  // new entry
}

// lookup
void *gm_lookup(GenericMap *m, const void *key) {
    unsigned int idx = m->hash_fn(key) % m->capacity;
    for (HEntry *e = m->buckets[idx]; e; e = e->next) {
        if (m->equal_fn(e->key, key)) {
            return e->value;
        }
    }
    return NULL;
}

bool gm_contains(GenericMap *m, const void *key) {
    return gm_lookup(m, key) != NULL;
}

// remove
bool gm_remove(GenericMap *m, const void *key) {
    unsigned int idx = m->hash_fn(key) % m->capacity;
    HEntry **ptr = &m->buckets[idx];
    while (*ptr) {
        if (m->equal_fn((*ptr)->key, key)) {
            HEntry *found = *ptr;
            *ptr = found->next;
            if (m->key_destroy) m->key_destroy(found->key);
            if (m->value_destroy) m->value_destroy(found->value);
            free(found);
            m->count--;
            return true;
        }
        ptr = &(*ptr)->next;
    }
    return false;
}

// foreach
void gm_foreach(GenericMap *m, ForeachFunc fn, void *user_data) {
    for (int i = 0; i < m->capacity; i++) {
        for (HEntry *e = m->buckets[i]; e; e = e->next) {
            fn(e->key, e->value, user_data);
        }
    }
}

int gm_size(GenericMap *m) { return m->count; }

void gm_destroy(GenericMap *m) {
    for (int i = 0; i < m->capacity; i++) {
        HEntry *e = m->buckets[i];
        while (e) {
            HEntry *next = e->next;
            if (m->key_destroy) m->key_destroy(e->key);
            if (m->value_destroy) m->value_destroy(e->value);
            free(e);
            e = next;
        }
    }
    free(m->buckets);
    free(m);
}

// ============================================================
// Demos
// ============================================================

void print_str_str(const void *key, void *value, void *ctx) {
    (void)ctx;
    printf("    %s: %s\n", (const char *)key, (char *)value);
}

void demo1_custom_generic(void) {
    printf("=== Demo 1: custom generic hash table (GLib-like) ===\n");

    // string -> string map with automatic memory management
    GenericMap *dict = gm_new_full(str_hash, str_equal, free, free);

    gm_insert(dict, strdup("hello"), strdup("hola"));
    gm_insert(dict, strdup("world"), strdup("mundo"));
    gm_insert(dict, strdup("cat"), strdup("gato"));
    gm_insert(dict, strdup("dog"), strdup("perro"));

    printf("  size: %d\n", gm_size(dict));

    char *val = gm_lookup(dict, "cat");
    printf("  lookup(\"cat\"): %s\n", val ? val : "(null)");

    // replace
    gm_insert(dict, strdup("cat"), strdup("minino"));
    val = gm_lookup(dict, "cat");
    printf("  after replace, lookup(\"cat\"): %s\n", val);

    // remove
    gm_remove(dict, "dog");
    printf("  after remove(\"dog\"), size: %d\n", gm_size(dict));
    printf("  contains(\"dog\"): %s\n",
           gm_contains(dict, "dog") ? "yes" : "no");

    printf("  all entries:\n");
    gm_foreach(dict, print_str_str, NULL);
    printf("\n");

    gm_destroy(dict);
}

void demo2_int_keys(void) {
    printf("=== Demo 2: integer keys (no allocation) ===\n");

    // for integer keys, store directly in pointer (if sizeof(int) <= sizeof(void*))
    GenericMap *m = gm_new(int_hash, int_equal);

    // allocate int keys on heap (or use a pool)
    for (int i = 0; i < 10; i++) {
        int *key = malloc(sizeof(int));
        *key = i * 10;
        int *val = malloc(sizeof(int));
        *val = i * i;
        gm_insert(m, key, val);
    }

    printf("  size: %d\n", gm_size(m));

    int search_key = 50;
    int *result = gm_lookup(m, &search_key);
    printf("  lookup(50): %d\n", result ? *result : -1);

    // cleanup (manual since no destroyers set)
    for (int i = 0; i < m->capacity; i++) {
        HEntry *e = m->buckets[i];
        while (e) {
            HEntry *next = e->next;
            free(e->key);
            free(e->value);
            free(e);
            e = next;
        }
    }
    free(m->buckets);
    free(m);
    printf("\n");
}

void demo3_hsearch(void) {
    printf("=== Demo 3: POSIX hsearch ===\n");

    hcreate(100);  // global table, max 100 entries

    // insert
    ENTRY item, *result;

    item.key = "apple";  item.data = (void *)(long)5;
    hsearch(item, ENTER);

    item.key = "banana"; item.data = (void *)(long)3;
    hsearch(item, ENTER);

    item.key = "cherry"; item.data = (void *)(long)8;
    hsearch(item, ENTER);

    // find
    item.key = "banana";
    result = hsearch(item, FIND);
    if (result) {
        printf("  hsearch(\"banana\"): %ld\n", (long)result->data);
    }

    item.key = "grape";
    result = hsearch(item, FIND);
    printf("  hsearch(\"grape\"): %s\n",
           result ? "found" : "not found");

    // no delete available!
    printf("  (hsearch has no delete operation)\n");

    hdestroy();
    printf("\n");
}

void demo4_word_frequency(void) {
    printf("=== Demo 4: word frequency counter ===\n");

    GenericMap *freq = gm_new_full(str_hash, str_equal, free, free);

    const char *words[] = {
        "the", "cat", "sat", "on", "the", "mat",
        "the", "cat", "and", "the", "dog", "sat",
        "on", "the", "mat", "and", "the", "cat",
        NULL
    };

    for (int i = 0; words[i]; i++) {
        int *count = gm_lookup(freq, words[i]);
        if (count) {
            (*count)++;
        } else {
            int *c = malloc(sizeof(int));
            *c = 1;
            gm_insert(freq, strdup(words[i]), c);
        }
    }

    printf("  word frequencies:\n");
    for (int i = 0; i < freq->capacity; i++) {
        for (HEntry *e = freq->buckets[i]; e; e = e->next) {
            printf("    %s: %d\n", (char *)e->key, *(int *)e->value);
        }
    }
    printf("\n");

    gm_destroy(freq);
}

void demo5_api_comparison(void) {
    printf("=== Demo 5: API comparison ===\n\n");

    printf("  --- Custom (this demo) ---\n");
    printf("  GenericMap *m = gm_new_full(str_hash, str_equal, free, free);\n");
    printf("  gm_insert(m, strdup(\"key\"), strdup(\"val\"));\n");
    printf("  char *v = gm_lookup(m, \"key\");\n");
    printf("  gm_remove(m, \"key\");\n");
    printf("  gm_destroy(m);\n\n");

    printf("  --- GLib GHashTable ---\n");
    printf("  GHashTable *t = g_hash_table_new_full(g_str_hash,\n");
    printf("      g_str_equal, g_free, g_free);\n");
    printf("  g_hash_table_insert(t, g_strdup(\"key\"), g_strdup(\"val\"));\n");
    printf("  char *v = g_hash_table_lookup(t, \"key\");\n");
    printf("  g_hash_table_remove(t, \"key\");\n");
    printf("  g_hash_table_destroy(t);\n\n");

    printf("  --- uthash ---\n");
    printf("  typedef struct { char key[32]; int val; UT_hash_handle hh; } Item;\n");
    printf("  Item *table = NULL;\n");
    printf("  Item *i = malloc(sizeof(Item));\n");
    printf("  strcpy(i->key, \"key\"); i->val = 42;\n");
    printf("  HASH_ADD_STR(table, key, i);\n");
    printf("  Item *found; HASH_FIND_STR(table, \"key\", found);\n");
    printf("  HASH_DEL(table, found); free(found);\n\n");

    printf("  --- khash ---\n");
    printf("  KHASH_MAP_INIT_STR(m, int)\n");
    printf("  khash_t(m) *h = kh_init(m);\n");
    printf("  int ret; khint_t k = kh_put(m, h, \"key\", &ret);\n");
    printf("  kh_value(h, k) = 42;\n");
    printf("  k = kh_get(m, h, \"key\");\n");
    printf("  kh_del(m, h, k);\n");
    printf("  kh_destroy(m, h);\n\n");
}

void demo6_resize_behavior(void) {
    printf("=== Demo 6: automatic resize ===\n");

    GenericMap *m = gm_new_full(str_hash, str_equal, free, free);

    printf("  initial capacity: %d\n", m->capacity);

    char key[32], val[32];
    for (int i = 0; i < 100; i++) {
        int old_cap = m->capacity;
        snprintf(key, sizeof(key), "key_%03d", i);
        snprintf(val, sizeof(val), "val_%03d", i);
        gm_insert(m, strdup(key), strdup(val));
        if (m->capacity != old_cap) {
            printf("  insert #%d: resize %d -> %d (n=%d, alpha=%.3f)\n",
                   i + 1, old_cap, m->capacity, m->count,
                   (double)m->count / m->capacity);
        }
    }

    printf("  final: n=%d, capacity=%d, alpha=%.3f\n",
           m->count, m->capacity,
           (double)m->count / m->capacity);

    // verify all entries
    int found = 0;
    for (int i = 0; i < 100; i++) {
        snprintf(key, sizeof(key), "key_%03d", i);
        if (gm_lookup(m, key)) found++;
    }
    printf("  verified: %d/100 entries found\n\n", found);

    gm_destroy(m);
}

int main(void) {
    demo1_custom_generic();
    demo2_int_keys();
    demo3_hsearch();
    demo4_word_frequency();
    demo5_api_comparison();
    demo6_resize_behavior();
    return 0;
}
```

---

## Programa completo en Rust

```rust
// hash_tables_c_comparison.rs
// Demonstrates Rust's HashMap ergonomics vs C patterns
// Run: rustc hash_tables_c_comparison.rs && ./hash_tables_c_comparison

use std::collections::HashMap;

fn demo1_basic_ergonomics() {
    println!("=== Demo 1: Rust HashMap vs C hash tables ===");
    println!("  What takes ~20 lines in C takes 3 in Rust:\n");

    // this replaces: gm_new_full + strdup + gm_insert + gm_lookup + gm_destroy
    let mut dict: HashMap<&str, &str> = HashMap::new();
    dict.insert("hello", "hola");
    dict.insert("world", "mundo");
    dict.insert("cat", "gato");

    println!("  dict[\"cat\"] = {:?}", dict.get("cat"));
    println!("  No manual memory management needed\n");
}

fn demo2_word_frequency() {
    println!("=== Demo 2: word frequency (C needs ~30 lines) ===");
    let text = "the cat sat on the mat the cat and the dog sat on the mat and the cat";

    let mut freq: HashMap<&str, usize> = HashMap::new();
    for word in text.split_whitespace() {
        *freq.entry(word).or_insert(0) += 1;
    }

    // sort by frequency
    let mut sorted: Vec<_> = freq.iter().collect();
    sorted.sort_by(|a, b| b.1.cmp(a.1));
    for (word, count) in &sorted {
        println!("  {:>6}: {}", word, count);
    }
    println!();
}

fn demo3_type_safety() {
    println!("=== Demo 3: type safety (impossible in C void*) ===");

    // Rust catches type errors at compile time
    let mut scores: HashMap<String, Vec<i32>> = HashMap::new();
    scores.entry("Alice".to_string()).or_default().push(95);
    scores.entry("Alice".to_string()).or_default().push(88);
    scores.entry("Bob".to_string()).or_default().push(92);

    // compiler guarantees: keys are String, values are Vec<i32>
    // in C with void*, you could accidentally insert a char* as value
    for (name, grades) in &scores {
        let avg: f64 = grades.iter().sum::<i32>() as f64 / grades.len() as f64;
        println!("  {}: grades={:?}, avg={:.1}", name, grades, avg);
    }
    println!();
}

fn demo4_automatic_memory() {
    println!("=== Demo 4: automatic memory management ===");

    {
        let mut map = HashMap::new();
        for i in 0..1000 {
            map.insert(format!("key_{}", i), vec![i; 100]);
        }
        println!("  map with 1000 entries, each value is Vec<i32> of 100");
        println!("  total memory: ~400KB of values + keys + overhead");
        // map drops here — all memory freed automatically
    }
    println!("  map dropped — all memory freed (no leaks possible)");
    println!("  In C: must iterate and free each key, value, and entry\n");
}

fn demo5_generic_function() {
    println!("=== Demo 5: generic functions (vs C void* callbacks) ===");

    // Rust generics: zero-cost, type-safe
    fn count_occurrences<T: std::hash::Hash + Eq>(items: &[T]) -> HashMap<&T, usize> {
        let mut counts = HashMap::new();
        for item in items {
            *counts.entry(item).or_insert(0) += 1;
        }
        counts
    }

    // works with any type
    let ints = [1, 2, 3, 2, 1, 3, 3];
    let int_counts = count_occurrences(&ints);
    println!("  int counts: {:?}", int_counts);

    let strs = ["a", "b", "a", "c", "b", "a"];
    let str_counts = count_occurrences(&strs);
    println!("  str counts: {:?}", str_counts);

    let chars: Vec<char> = "abracadabra".chars().collect();
    let char_counts = count_occurrences(&chars);
    println!("  char counts: {:?}", char_counts);
    println!("  In C: would need void*, function pointers, casts\n");
}

fn demo6_no_manual_resize() {
    println!("=== Demo 6: automatic resize (transparent) ===");

    let mut map: HashMap<i32, i32> = HashMap::new();
    println!("  empty: cap={}", map.capacity());

    for i in 0..100 {
        let old_cap = map.capacity();
        map.insert(i, i * i);
        if map.capacity() != old_cap {
            println!("  insert #{}: cap {} -> {}", i + 1, old_cap, map.capacity());
        }
    }
    println!("  final: len={}, cap={}", map.len(), map.capacity());
    println!("  In C: must implement resize yourself (or use a library)\n");
}

fn demo7_c_patterns_in_rust() {
    println!("=== Demo 7: C patterns expressed in Rust ===\n");

    // Pattern: uthash-style "add only if not exists"
    let mut map = HashMap::new();
    map.entry("key").or_insert("first");   // inserted
    map.entry("key").or_insert("second");  // ignored (key exists)
    println!("  or_insert pattern: {:?}", map.get("key"));

    // Pattern: GLib foreach
    let scores = HashMap::from([("Alice", 95), ("Bob", 87), ("Carol", 92)]);
    print!("  foreach: ");
    scores.iter().for_each(|(k, v)| print!("{}={} ", k, v));
    println!();

    // Pattern: khash "put then set value"
    // In khash: k = kh_put(...); kh_value(h, k) = val;
    // In Rust, just:
    map.insert("key2", "value2");
    println!("  insert: no two-step put+set needed");

    // Pattern: hsearch FIND/ENTER
    // hsearch can find-or-insert in one call
    // Rust equivalent: entry API
    let val = map.entry("key3").or_insert("default");
    println!("  entry or_insert: {:?}\n", val);
}

fn main() {
    demo1_basic_ergonomics();
    demo2_word_frequency();
    demo3_type_safety();
    demo4_automatic_memory();
    demo5_generic_function();
    demo6_no_manual_resize();
    demo7_c_patterns_in_rust();
}
```

---

## Ejercicios

### Ejercicio 1 — hsearch: limitaciones en la práctica

Intenta usar `hsearch` para crear un diccionario de 10 palabras,
buscar 5, y luego intentar eliminar 2. ¿Qué pasa al intentar
eliminar?

<details><summary>¿Qué sucede al intentar eliminar?</summary>

No compila (o no existe la función). `hsearch` no tiene operación
de eliminación. Una vez insertado un elemento, permanece hasta
`hdestroy` (que destruye toda la tabla).

Si necesitas eliminación, debes usar otra solución. Esto hace que
`hsearch` sea inviable para la mayoría de aplicaciones reales.

</details>

<details><summary>¿Qué pasa si insertas más de nel elementos?</summary>

`hcreate(nel)` reserva internamente una tabla que puede no ser
exactamente `nel` (POSIX dice que puede ser mayor). Si insertas
más elementos de los que caben, `hsearch` con `ENTER` devuelve
`NULL` — inserción silenciosamente fallida. No hay
redimensionamiento automático.

</details>

---

### Ejercicio 2 — GLib vs uthash: ¿cuál elegir?

Tu proyecto es un daemon de red en C que mantiene una tabla de
conexiones activas (clave: socket FD, valor: struct con estado).
Las conexiones se abren y cierran constantemente. ¿GLib o uthash?

<details><summary>¿Cuál es mejor para este caso?</summary>

**uthash** es probablemente mejor aquí:
- **Sin dependencia extra**: uthash es header-only, no necesitas
  linkear libglib.
- **Intrusivo**: el struct de conexión ya contiene la metadata
  del hash — sin alocaciones separadas por entrada.
- **Eliminación frecuente**: uthash soporta `HASH_DEL`.
- **Rendimiento**: sin indirección extra por `void *`.

GLib sería mejor si el proyecto ya depende de GLib o si necesitas
otras utilidades de GLib (strings, arrays, event loop).

</details>

<details><summary>¿Cómo sería con uthash?</summary>

```c
typedef struct {
    int fd;                  // key
    char addr[64];
    int state;
    time_t connected_at;
    UT_hash_handle hh;
} Connection;

Connection *connections = NULL;

// add connection
Connection *conn = malloc(sizeof(Connection));
conn->fd = new_fd;
HASH_ADD_INT(connections, fd, conn);

// find by fd
Connection *found;
HASH_FIND_INT(connections, &client_fd, found);

// remove on disconnect
HASH_DEL(connections, found);
free(found);
```

Simple, directo, sin overhead.

</details>

---

### Ejercicio 3 — Implementar or_insert para tabla genérica

Añade un método `gm_or_insert` a la implementación `GenericMap`
que inserte un valor por defecto solo si la clave no existe, y
devuelva un puntero al valor (existente o nuevo).

<details><summary>¿Cuál es la firma de la función?</summary>

```c
void *gm_or_insert(GenericMap *m, void *key, void *default_val);
```

Devuelve `void *` al valor. Si la clave existe, devuelve puntero
al valor existente y **no toma ownership** de `key` ni
`default_val` (el caller debe liberarlos). Si no existe, inserta
y devuelve puntero al nuevo valor.

</details>

<details><summary>Implementación</summary>

```c
void *gm_or_insert(GenericMap *m, void *key, void *default_val) {
    void *existing = gm_lookup(m, key);
    if (existing) {
        // key exists — don't take ownership of key/default_val
        return existing;
    }
    gm_insert(m, key, default_val);
    return gm_lookup(m, key);
}
```

Cuidado con ownership: si la clave ya existe, el caller debe
liberar `key` y `default_val` manualmente. Esto es exactamente el
tipo de problema que Rust resuelve con el sistema de tipos y la
Entry API.

</details>

---

### Ejercicio 4 — uthash: claves string

Con uthash, ¿cuál es la diferencia entre `HASH_ADD_STR` y
`HASH_ADD_KEYPTR`? ¿Cuándo usar cada uno?

<details><summary>¿Cuándo usar HASH_ADD_STR?</summary>

`HASH_ADD_STR(head, field, ptr)` se usa cuando la clave es un
`char[]` (array de tamaño fijo) **dentro** del struct:

```c
typedef struct { char name[32]; int val; UT_hash_handle hh; } Item;
```

uthash calcula la longitud con `strlen` y usa la dirección del
campo inline como clave. La clave vive dentro del struct.

</details>

<details><summary>¿Cuándo usar HASH_ADD_KEYPTR?</summary>

`HASH_ADD_KEYPTR(head, field, keyptr, keylen, ptr)` se usa cuando
la clave es un `char *` (puntero) que apunta a memoria externa:

```c
typedef struct { char *name; int val; UT_hash_handle hh; } Item;
item->name = strdup("hello");  // allocated separately
HASH_ADD_KEYPTR(items, name, item->name, strlen(item->name), item);
```

La clave no está inline — es un puntero a otra parte de memoria.
Debes proporcionar la longitud explícitamente.

</details>

---

### Ejercicio 5 — Memory leak con uthash

El siguiente código tiene un memory leak. Encuéntralo:

```c
User *users = NULL;
for (int i = 0; i < 100; i++) {
    User *u = malloc(sizeof(User));
    u->id = i % 10;  // only 10 unique IDs
    HASH_ADD_INT(users, id, u);
}
```

<details><summary>¿Dónde está el leak?</summary>

`HASH_ADD_INT` no verifica si la clave ya existe. Si `i % 10`
produce IDs duplicados (0-9 repetidos 10 veces), se añaden
múltiples entradas con la misma clave. Esto:
1. Viola el invariante de uthash (claves únicas).
2. Pierde las entradas anteriores (no se liberan).

Los primeros 10 inserts crean entradas válidas. Los 90 restantes
crean entradas con claves duplicadas — uthash las añade pero el
comportamiento es indefinido para búsquedas.

Los 90 mallocs de User nunca se liberan → **leak de 90 structs**.

</details>

<details><summary>¿Cómo corregirlo?</summary>

```c
for (int i = 0; i < 100; i++) {
    int key = i % 10;
    User *existing;
    HASH_FIND_INT(users, &key, existing);
    if (existing) {
        // update or skip
        continue;
    }
    User *u = malloc(sizeof(User));
    u->id = key;
    HASH_ADD_INT(users, id, u);
}
```

Siempre verificar con `HASH_FIND` antes de `HASH_ADD`.

</details>

---

### Ejercicio 6 — khash: API de dos pasos

¿Por qué khash separa `kh_put` (insertar bucket) de
`kh_value` (asignar valor)? ¿No sería mejor una sola función?

<details><summary>¿Por qué dos pasos?</summary>

`kh_put` devuelve un **iterador** (índice del bucket) y un código
de retorno (`ret`). El caller necesita el iterador para:
- Asignar el valor (`kh_value(h, k) = val`).
- Saber si la clave ya existía (`ret == 0` → existente).

Esto permite al caller **decidir qué hacer** según si la clave es
nueva o existente — equivalente a la Entry API de Rust, pero manual.

Si fuera una sola función `kh_insert(h, key, value)`, no habría
forma de devolver tanto el éxito como un puntero al valor para
modificación posterior.

</details>

<details><summary>Patrón equivalente en Rust</summary>

```c
// khash (C):
int ret;
khint_t k = kh_put(m, h, key, &ret);
if (ret == 0) {
    kh_value(h, k) += 1;  // existed: increment
} else {
    kh_value(h, k) = 1;   // new: initialize
}

// Rust equivalent:
map.entry(key)
    .and_modify(|v| *v += 1)
    .or_insert(1);
```

La Entry API de Rust expresa la misma lógica de forma más
declarativa y sin posibilidad de error (no se puede olvidar
asignar el valor).

</details>

---

### Ejercicio 7 — Implementación propia vs librería: benchmark

Implementa un benchmark que inserte 100000 strings aleatorios y
busque 100000 strings (50% existentes, 50% no) en:
a) Tu implementación GenericMap de esta lección.
b) POSIX `hsearch`.

<details><summary>¿Cuál es más rápida?</summary>

`hsearch` suele ser más rápida para inserciones porque está
optimizada en glibc (usa open addressing con tabla estática).
GenericMap usa chaining con malloc por entrada.

Para búsquedas, depende: `hsearch` tiene mejor localidad (open
addressing), pero GenericMap permite eliminación y resize.

Típicamente: hsearch 1.5-2× más rápida en inserciones, similar en
búsquedas, pero **inútil** para aplicaciones reales por sus
limitaciones.

</details>

<details><summary>¿Cómo mejorar GenericMap?</summary>

1. **Pool allocator** en vez de `malloc` por entrada — elimina
   la fragmentación y reduce el overhead de alocación.
2. **Open addressing** en vez de chaining — mejor localidad.
3. **Función hash más rápida** (FNV-1a o xxHash en vez de djb2).
4. **Inline keys pequeñas** — almacenar strings cortos dentro del
   bucket sin puntero extra.

Cada mejora acerca el rendimiento al de librerías especializadas.

</details>

---

### Ejercicio 8 — Destructor doble: el peligro de void *

El siguiente código usa `GenericMap` con destructores. ¿Hay un
bug?

```c
char *shared_key = strdup("config");
gm_insert(m, shared_key, strdup("value1"));
gm_insert(m, shared_key, strdup("value2"));  // replace
```

<details><summary>¿Cuál es el bug?</summary>

En el `gm_insert` con replace, la implementación llama
`key_destroy(e->key)` para liberar la clave vieja. Pero
`e->key` y el nuevo `key` son el **mismo puntero** (`shared_key`).
Después de `free(e->key)`, `shared_key` es un dangling pointer,
y se almacena en la entrada como nueva clave.

Resultado: **use-after-free**. La siguiente búsqueda con
`shared_key` accede memoria liberada.

</details>

<details><summary>¿Cómo corregirlo?</summary>

Siempre pasar claves **independientes** al insertar:

```c
gm_insert(m, strdup("config"), strdup("value1"));
gm_insert(m, strdup("config"), strdup("value2"));
```

O verificar en la implementación si la clave nueva es la misma
que la vieja antes de destruir:

```c
if (e->key != key && m->key_destroy) {
    m->key_destroy(e->key);
}
```

Rust previene esto completamente con el sistema de ownership: no
se puede tener dos referencias mutables al mismo dato.

</details>

---

### Ejercicio 9 — uthash: iterar y eliminar

¿Es seguro eliminar elementos durante la iteración con
`HASH_ITER`? ¿Qué hace el parámetro `tmp`?

<details><summary>¿Es seguro?</summary>

**Sí**, y esta es una de las ventajas de uthash. `HASH_ITER` usa
dos punteros: `current` y `tmp`. Antes de procesar `current`, el
macro ya guardó `tmp = current->hh.next`. Así, aunque elimines
`current`, la iteración continúa desde `tmp`.

```c
User *current, *tmp;
HASH_ITER(hh, users, current, tmp) {
    if (current->id > 50) {
        HASH_DEL(users, current);  // safe!
        free(current);
    }
}
```

Sin `tmp`, eliminar `current` destruiría el puntero `next` y la
iteración se rompería. Este patrón es equivalente a `retain` en
Rust.

</details>

<details><summary>¿Qué pasa si insertas durante la iteración?</summary>

**No es seguro**. Insertar puede disparar un resize (rehash), lo
que invalida todos los punteros internos. El iterador se
corrompería. La regla: durante `HASH_ITER`, solo puedes
**eliminar** el elemento actual, no insertar nuevos.

Misma regla que en Rust: no se puede mutar una colección mientras
se itera (el borrow checker lo previene).

</details>

---

### Ejercicio 10 — Diseñar API de hash table en C

Diseña la **API** (solo firmas de funciones, sin implementación)
para una hash table en C que:
- Soporte claves y valores de cualquier tipo.
- Tenga ownership semántico (destructores).
- Soporte iteración segura con eliminación.
- Sea thread-safe con read-write lock.

<details><summary>¿Cómo sería la API?</summary>

```c
// types
typedef struct HTable HTable;
typedef struct HIterator HIterator;

typedef unsigned int (*HHashFn)(const void *key, size_t key_size);
typedef bool (*HEqualFn)(const void *a, const void *b, size_t size);
typedef void (*HFreeFn)(void *ptr);

// lifecycle
HTable *ht_create(size_t key_size, size_t val_size,
                  HHashFn hash, HEqualFn eq);
void ht_set_destructors(HTable *t, HFreeFn key_free, HFreeFn val_free);
void ht_destroy(HTable *t);

// operations
bool ht_insert(HTable *t, const void *key, const void *value);
bool ht_get(HTable *t, const void *key, void *out_value);
bool ht_remove(HTable *t, const void *key);
bool ht_contains(HTable *t, const void *key);
size_t ht_size(HTable *t);

// iteration (safe delete during iteration)
HIterator *ht_iter_create(HTable *t);
bool ht_iter_next(HIterator *it, void **key, void **value);
void ht_iter_remove(HIterator *it);  // remove current
void ht_iter_destroy(HIterator *it);

// thread safety
void ht_read_lock(HTable *t);
void ht_read_unlock(HTable *t);
void ht_write_lock(HTable *t);
void ht_write_unlock(HTable *t);
```

</details>

<details><summary>¿Qué complejidad añade el thread safety?</summary>

Mucha. El read-write lock permite múltiples lectores simultáneos
pero un solo escritor. Cada `ht_insert`, `ht_remove`, e
`ht_iter_remove` debe tomar el write lock. Cada `ht_get`,
`ht_contains`, e `ht_iter_next` debe tomar el read lock.

El iterador es problemático: ¿debe mantener un lock durante toda
la iteración? Si toma un read lock, no se puede eliminar. Si toma
un write lock, bloquea a todos los lectores durante la iteración
completa.

Alternativas: iterador que toma snapshots, o lock-free hash tables
(mucho más complejas). Esto ilustra por qué la mayoría de
librerías de C no son thread-safe — la complejidad es enorme.

</details>
