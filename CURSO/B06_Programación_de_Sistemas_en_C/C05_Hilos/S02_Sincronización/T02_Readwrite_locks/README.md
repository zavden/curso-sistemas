# Read-write locks

## Índice
1. [Motivación: lectores vs escritores](#1-motivación-lectores-vs-escritores)
2. [pthread_rwlock básico](#2-pthread_rwlock-básico)
3. [Inicialización y atributos](#3-inicialización-y-atributos)
4. [rdlock, wrlock y unlock](#4-rdlock-wrlock-y-unlock)
5. [tryrdlock, trywrlock y timedlock](#5-tryrdlock-trywrlock-y-timedlock)
6. [Políticas de scheduling](#6-políticas-de-scheduling)
7. [Cuándo son ventajosos](#7-cuándo-son-ventajosos)
8. [rwlock entre procesos](#8-rwlock-entre-procesos)
9. [Patrones de uso](#9-patrones-de-uso)
10. [Alternativas](#10-alternativas)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Motivación: lectores vs escritores

Con un mutex ordinario, todas las operaciones se serializan — incluso
las lecturas. Pero múltiples lecturas simultáneas son seguras si nadie
escribe:

```
  Mutex (serializa todo):
  ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐ ┌────┐
  │ R1 │ │ R2 │ │ W1 │ │ R3 │ │ R4 │ │ W2 │
  └────┘ └────┘ └────┘ └────┘ └────┘ └────┘
  ──────────────────────────────────────────► tiempo

  rwlock (lecturas en paralelo):
  ┌────┐
  │ R1 │ ┌────┐
  │    │ │ R2 │ ┌────┐ ┌────┐
  └────┘ └────┘ │ W1 │ │ R3 │
                └────┘ │    │ ┌────┐
                       │ R4 │ │ W2 │
                       └────┘ └────┘
  ──────────────────────────────────────────► tiempo
```

Un **read-write lock** (rwlock) permite:
- Múltiples lectores **simultáneamente** (shared access)
- Solo **un escritor** a la vez, excluyendo a todos los lectores
  (exclusive access)

```
┌───────────────────┬──────────────────────────────────────┐
│ Estado del rwlock │ ¿Quién puede entrar?                 │
├───────────────────┼──────────────────────────────────────┤
│ Sin lock          │ Cualquiera (reader o writer)         │
├───────────────────┼──────────────────────────────────────┤
│ Read-locked       │ Más readers: sí                      │
│ (1+ readers)      │ Writers: NO (bloquean)               │
├───────────────────┼──────────────────────────────────────┤
│ Write-locked      │ Readers: NO (bloquean)               │
│ (1 writer)        │ Writers: NO (bloquean)               │
└───────────────────┴──────────────────────────────────────┘
```

---

## 2. pthread_rwlock básico

```c
#include <pthread.h>

pthread_rwlock_t rwl = PTHREAD_RWLOCK_INITIALIZER;
int shared_data = 0;

void *reader(void *arg)
{
    pthread_rwlock_rdlock(&rwl);
    // Múltiples readers pueden estar aquí simultáneamente
    printf("Read: %d\n", shared_data);
    pthread_rwlock_unlock(&rwl);
    return NULL;
}

void *writer(void *arg)
{
    pthread_rwlock_wrlock(&rwl);
    // Solo un writer, sin readers
    shared_data++;
    printf("Write: %d\n", shared_data);
    pthread_rwlock_unlock(&rwl);
    return NULL;
}
```

---

## 3. Inicialización y atributos

### Estática

```c
pthread_rwlock_t rwl = PTHREAD_RWLOCK_INITIALIZER;
```

### Dinámica

```c
pthread_rwlockattr_t attr;
pthread_rwlockattr_init(&attr);

// Configurar atributos (ver secciones 6 y 8)
pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NP);
pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

pthread_rwlock_t rwl;
pthread_rwlock_init(&rwl, &attr);
pthread_rwlockattr_destroy(&attr);
```

### Destrucción

```c
int pthread_rwlock_destroy(pthread_rwlock_t *rwlock);
// No destruir si hay readers o writers activos
// Retorna: EBUSY si está en uso
```

---

## 4. rdlock, wrlock y unlock

### pthread_rwlock_rdlock — adquirir para lectura

```c
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock);
// Si no hay writer activo: adquiere read lock, retorna 0
// Si hay writer activo: BLOQUEA hasta que el writer haga unlock
// Múltiples rdlocks simultáneos: OK
```

```c
pthread_rwlock_rdlock(&rwl);
// ... leer datos compartidos ...
// NO modificar datos aquí
pthread_rwlock_unlock(&rwl);
```

Un thread puede hacer `rdlock` múltiples veces sin deadlock (el
read lock es recursivo por naturaleza). Pero cada `rdlock` necesita
su correspondiente `unlock`.

### pthread_rwlock_wrlock — adquirir para escritura

```c
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock);
// Si no hay readers ni writers: adquiere write lock, retorna 0
// Si hay readers o writer: BLOQUEA hasta que todos hagan unlock
```

```c
pthread_rwlock_wrlock(&rwl);
// ... modificar datos compartidos ...
// Acceso exclusivo: nadie más lee ni escribe
pthread_rwlock_unlock(&rwl);
```

### pthread_rwlock_unlock — liberar

```c
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock);
// Libera el lock (tanto read como write usan el mismo unlock)
// Si era el último reader: despierta writer(s) esperando
// Si era writer: despierta readers y/o writers esperando
```

No se distingue entre reader unlock y writer unlock. El rwlock
internamente sabe qué tipo de lock tenía el thread.

### Diagrama de estados

```
                    rdlock()
              ┌──────────────┐
              │              ▼
  ┌────────┐  │  ┌────────────────┐
  │  FREE  │──┘  │  READ-LOCKED   │◄──┐
  │        │     │  (readers > 0)  │───┘ rdlock() (más readers)
  └────┬───┘     └───────┬────────┘
       │                 │ unlock() (último reader)
       │                 ▼
       │         ┌────────────────┐
       │         │     FREE       │
       │         └────────────────┘
       │
       │ wrlock()
       ▼
  ┌────────────────┐
  │  WRITE-LOCKED  │
  │  (1 writer)    │
  └───────┬────────┘
          │ unlock()
          ▼
  ┌────────────────┐
  │     FREE       │
  └────────────────┘
```

---

## 5. tryrdlock, trywrlock y timedlock

### try — no bloqueante

```c
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock);
// Retorna: 0 si obtuvo read lock, EBUSY si hay writer activo

int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock);
// Retorna: 0 si obtuvo write lock, EBUSY si hay readers o writer
```

```c
if (pthread_rwlock_tryrdlock(&rwl) == 0) {
    int val = shared_data;
    pthread_rwlock_unlock(&rwl);
    return val;
} else {
    return cached_value;  // usar caché si no se puede leer ahora
}
```

### timedlock — con timeout

```c
int pthread_rwlock_timedrdlock(pthread_rwlock_t *rwlock,
                               const struct timespec *abs_timeout);
int pthread_rwlock_timedwrlock(pthread_rwlock_t *rwlock,
                               const struct timespec *abs_timeout);
// Retorna: 0 si obtuvo el lock, ETIMEDOUT si expiró
```

```c
struct timespec ts;
clock_gettime(CLOCK_REALTIME, &ts);
ts.tv_sec += 2;

int err = pthread_rwlock_timedwrlock(&rwl, &ts);
if (err == ETIMEDOUT) {
    fprintf(stderr, "Could not get write lock in 2s\n");
    return -1;
}
// Obtuvo el lock
shared_data++;
pthread_rwlock_unlock(&rwl);
```

---

## 6. Políticas de scheduling

Cuando un writer libera el lock y hay tanto readers como writers
esperando, ¿quién va primero? POSIX no lo especifica. Linux ofrece
control con extensiones `_np` (non-portable):

```c
#include <pthread.h>

pthread_rwlockattr_t attr;
pthread_rwlockattr_init(&attr);
pthread_rwlockattr_setkind_np(&attr, kind);
```

### Políticas disponibles (Linux/glibc)

```
┌────────────────────────────────────┬─────────────────────────────┐
│ Política                           │ Comportamiento              │
├────────────────────────────────────┼─────────────────────────────┤
│ PTHREAD_RWLOCK_PREFER_READER_NP    │ Prioridad a lectores.      │
│ (default)                          │ Escritores pueden sufrir    │
│                                    │ starvation.                 │
├────────────────────────────────────┼─────────────────────────────┤
│ PTHREAD_RWLOCK_PREFER_WRITER_NP    │ Nombre engañoso: en glibc  │
│                                    │ se comporta igual que       │
│                                    │ PREFER_READER en la         │
│                                    │ práctica.                   │
├────────────────────────────────────┼─────────────────────────────┤
│ PTHREAD_RWLOCK_PREFER_WRITER_      │ Prioridad a escritores.    │
│ NONRECURSIVE_NP                    │ Nuevos rdlock se bloquean   │
│                                    │ si hay writer esperando.    │
│                                    │ ⚠️ rdlock recursivo puede   │
│                                    │ causar deadlock.            │
└────────────────────────────────────┴─────────────────────────────┘
```

### Prioridad a lectores (default)

```
  Writer W espera            Readers R1, R2 llegan
  ─────────────              ──────────────────────
  wrlock() → bloqueado       rdlock() → pasan (hay readers activos)
                             rdlock() → pasan
  [W sigue esperando...]     [R1, R2 leen...]
                             Más readers llegan → pasan
  [W podría esperar          [el flujo de readers no se interrumpe]
   indefinidamente]
```

**Problema**: writer starvation. Si los readers llegan continuamente,
el writer nunca obtiene el lock.

### Prioridad a escritores (PREFER_WRITER_NONRECURSIVE_NP)

```
  Readers R1, R2 activos     Writer W llega
  ────────────────────        ────────────────
                              wrlock() → bloqueado (R1, R2 activos)
  R3 llega:
  rdlock() → BLOQUEADO        (writer esperando tiene prioridad)
  R1 termina → unlock
  R2 termina → unlock
  W despierta → wrlock OK     [W escribe]
  W termina → unlock
  R3 despierta → rdlock OK    [R3 lee]
```

**Problema**: reader starvation con escrituras frecuentes. Y rdlock
recursivo causa deadlock:

```c
// DEADLOCK con PREFER_WRITER_NONRECURSIVE:
pthread_rwlock_rdlock(&rwl);   // ok, count=1
// Otro thread: wrlock() → esperando
pthread_rwlock_rdlock(&rwl);   // DEADLOCK: bloqueado porque
                                // hay writer esperando, pero el
                                // writer espera a que YO libere
```

### Recomendación

- **Lecturas muy frecuentes, escrituras raras**: default
  (PREFER_READER). El riesgo de writer starvation es bajo.
- **Escrituras frecuentes, frescura de datos importa**:
  PREFER_WRITER_NONRECURSIVE_NP, pero **nunca hacer rdlock recursivo**.
- En la mayoría de casos, el default funciona bien.

---

## 7. Cuándo son ventajosos

Los rwlocks **no siempre** son mejores que un mutex. El rwlock tiene
más overhead que un mutex (mantiene contador de readers, lógica de
scheduling). Solo es ventajoso cuando la ganancia del paralelismo de
lecturas supera ese overhead.

### Condiciones favorables

```
┌──────────────────────────────────┬──────────────────────────┐
│ Favorable para rwlock            │ Desfavorable para rwlock │
├──────────────────────────────────┼──────────────────────────┤
│ Ratio lecturas:escrituras > 10:1 │ Ratio cercano a 1:1     │
├──────────────────────────────────┼──────────────────────────┤
│ Sección crítica larga (>1µs)     │ Sección crítica corta    │
│ (lecturas costosas)              │ (<100ns)                 │
├──────────────────────────────────┼──────────────────────────┤
│ Muchos threads lectores (>4)     │ Pocos threads (2-3)      │
├──────────────────────────────────┼──────────────────────────┤
│ Datos grandes (caché, config,    │ Un int o puntero         │
│ tablas de routing, DNS cache)    │ (mejor usar _Atomic)     │
└──────────────────────────────────┴──────────────────────────┘
```

### Benchmark típico

```
  8 threads, sección crítica de ~500ns

  Ratio R:W    mutex (ops/s)    rwlock (ops/s)    Mejora
  ─────────    ─────────────    ──────────────    ──────
  100:0 (R)    2M               16M               8x
  99:1         2M               14M               7x
  90:10        2M               5M                2.5x
  50:50        2M               1.8M              0.9x ← peor
  0:100 (W)    2M               1.5M              0.75x ← peor
```

Con 50:50 o más escrituras, el mutex simple es **más rápido** porque
el rwlock tiene overhead adicional y los writers se serializan igual.

### Regla práctica

> Si no puedes demostrar con profiling que tienes contención de lectura,
> usa un mutex. El rwlock es una optimización, no un default.

---

## 8. rwlock entre procesos

Como los mutexes, los rwlocks pueden compartirse entre procesos si
están en memoria compartida:

```c
#include <pthread.h>
#include <sys/mman.h>

typedef struct {
    pthread_rwlock_t rwl;
    int data[1000];
} shared_t;

shared_t *shm = mmap(NULL, sizeof(shared_t),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);

pthread_rwlockattr_t attr;
pthread_rwlockattr_init(&attr);
pthread_rwlockattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
pthread_rwlock_init(&shm->rwl, &attr);
pthread_rwlockattr_destroy(&attr);

// Después de fork:
// Proceso hijo (lector):
pthread_rwlock_rdlock(&shm->rwl);
read_data(shm->data);
pthread_rwlock_unlock(&shm->rwl);

// Proceso padre (escritor):
pthread_rwlock_wrlock(&shm->rwl);
update_data(shm->data);
pthread_rwlock_unlock(&shm->rwl);
```

**Nota**: no hay equivalente de `PTHREAD_MUTEX_ROBUST` para rwlocks
en POSIX. Si un proceso muere con un rwlock adquirido, los demás se
quedan bloqueados. Usar timeouts (`timedrdlock`/`timedwrlock`) para
detectar y recuperar.

---

## 9. Patrones de uso

### 9.1 Caché thread-safe

El caso de uso clásico: una caché que se lee constantemente y se
actualiza raramente:

```c
#include <pthread.h>
#include <string.h>
#include <stdlib.h>

#define CACHE_SIZE 1024

typedef struct {
    char key[64];
    char value[256];
    int valid;
} cache_entry_t;

typedef struct {
    pthread_rwlock_t rwl;
    cache_entry_t entries[CACHE_SIZE];
} cache_t;

void cache_init(cache_t *c)
{
    pthread_rwlock_init(&c->rwl, NULL);
    memset(c->entries, 0, sizeof(c->entries));
}

// Lectura (múltiples threads simultáneamente)
int cache_get(cache_t *c, const char *key, char *value, size_t vsize)
{
    pthread_rwlock_rdlock(&c->rwl);

    unsigned h = hash(key) % CACHE_SIZE;
    int found = 0;
    if (c->entries[h].valid && strcmp(c->entries[h].key, key) == 0) {
        strncpy(value, c->entries[h].value, vsize - 1);
        value[vsize - 1] = '\0';
        found = 1;
    }

    pthread_rwlock_unlock(&c->rwl);
    return found;
}

// Escritura (exclusiva)
void cache_put(cache_t *c, const char *key, const char *value)
{
    pthread_rwlock_wrlock(&c->rwl);

    unsigned h = hash(key) % CACHE_SIZE;
    strncpy(c->entries[h].key, key, sizeof(c->entries[h].key) - 1);
    strncpy(c->entries[h].value, value, sizeof(c->entries[h].value) - 1);
    c->entries[h].valid = 1;

    pthread_rwlock_unlock(&c->rwl);
}

void cache_invalidate(cache_t *c, const char *key)
{
    pthread_rwlock_wrlock(&c->rwl);
    unsigned h = hash(key) % CACHE_SIZE;
    c->entries[h].valid = 0;
    pthread_rwlock_unlock(&c->rwl);
}
```

### 9.2 Tabla de configuración

```c
typedef struct {
    pthread_rwlock_t rwl;
    char db_host[256];
    int db_port;
    int max_connections;
    int log_level;
} config_t;

config_t g_config;

void config_init(config_t *cfg)
{
    pthread_rwlock_init(&cfg->rwl, NULL);
    // cargar valores iniciales
}

// Lectores (hot path — muy frecuente)
int config_get_port(config_t *cfg)
{
    pthread_rwlock_rdlock(&cfg->rwl);
    int port = cfg->db_port;
    pthread_rwlock_unlock(&cfg->rwl);
    return port;
}

// Copiar toda la config de una vez (evitar múltiples rdlock)
void config_snapshot(config_t *cfg, config_t *out)
{
    pthread_rwlock_rdlock(&cfg->rwl);
    *out = *cfg;  // copia la struct completa (no el rwlock)
    pthread_rwlock_unlock(&cfg->rwl);
    // El caller usa 'out' sin locks
}

// Escritor (raro — SIGHUP reload)
void config_reload(config_t *cfg, const char *path)
{
    // Parsear config nueva FUERA del lock
    config_t new_cfg;
    parse_config_file(path, &new_cfg);

    // Solo lockear para el swap
    pthread_rwlock_wrlock(&cfg->rwl);
    cfg->db_host[0] = '\0';
    strncpy(cfg->db_host, new_cfg.db_host, sizeof(cfg->db_host) - 1);
    cfg->db_port = new_cfg.db_port;
    cfg->max_connections = new_cfg.max_connections;
    cfg->log_level = new_cfg.log_level;
    pthread_rwlock_unlock(&cfg->rwl);
}
```

### 9.3 Patrón read-copy-update (RCU simplificado)

Para datos que se leen muy frecuentemente y se actualizan muy
raramente, un patrón sin rwlock:

```c
_Atomic(config_t *) current_config;

// Lector: sin lock alguno
config_t *cfg = atomic_load(&current_config);
// Usar cfg->... directamente (solo lectura)
// No hacer free(cfg) — puede haber otros lectores

// Escritor: crear copia nueva, swap atómico, free vieja después
void update_config(const char *key, const char *value)
{
    config_t *old = atomic_load(&current_config);
    config_t *new = malloc(sizeof(config_t));
    *new = *old;  // copiar

    apply_change(new, key, value);

    atomic_store(&current_config, new);

    // Problema: ¿cuándo es seguro hacer free(old)?
    // Necesitas saber que ningún lector aún referencia old
    // Solución real: RCU con grace periods (kernel Linux)
    // Simplificación: epoch-based reclamation o hazard pointers
    // O simplemente nunca hacer free (si las configs son pocas)
}
```

Este patrón tiene lecturas de **costo cero** (sin lock, sin atomic
RMW), pero la gestión de memoria del lado de escritura es compleja.
En la práctica, el rwlock es más simple y suficiente.

### 9.4 Upgrade de read a write

POSIX no proporciona una operación de "upgrade" (pasar de rdlock a
wrlock atómicamente). Hay que soltar y readquirir:

```c
// NO existe: pthread_rwlock_upgrade(&rwl)

// Patrón manual (con ventana de inconsistencia):
pthread_rwlock_rdlock(&rwl);
int val = shared_data;  // leer
if (val < 0) {
    // Necesitamos escribir
    pthread_rwlock_unlock(&rwl);      // soltar read
    // --- VENTANA: otro thread puede modificar aquí ---
    pthread_rwlock_wrlock(&rwl);      // adquirir write
    // Re-verificar la condición (pudo haber cambiado)
    if (shared_data < 0) {
        shared_data = 0;              // ahora sí escribir
    }
    pthread_rwlock_unlock(&rwl);
} else {
    pthread_rwlock_unlock(&rwl);
}
```

La re-verificación después del wrlock es obligatoria — es el mismo
patrón de double-check que vemos con condvars.

---

## 10. Alternativas

### Mutex simple

Más rápido que rwlock cuando las escrituras son frecuentes o la
sección crítica es corta:

```c
// Mutex: ~25ns lock+unlock sin contención
// rwlock rdlock: ~35-40ns sin contención (overhead del contador)
```

### C11 _Atomic

Para una sola variable (int, puntero), `_Atomic` es más rápido y
simple que cualquier lock:

```c
_Atomic int config_value = 42;

// Lectura: ~5ns
int val = atomic_load(&config_value);

// Escritura: ~5ns
atomic_store(&config_value, 99);
```

### Sequence lock (seqlock)

Un patrón del kernel Linux para datos que se leen mucho más que se
escriben. El lector no adquiere lock — lee y luego verifica si un
escritor intervino:

```c
typedef struct {
    _Atomic unsigned seq;  // par = consistente, impar = write en progreso
    pthread_mutex_t wr_mtx;
} seqlock_t;

typedef struct {
    int x, y, z;
} point_t;

seqlock_t sl = {.seq = 0, .wr_mtx = PTHREAD_MUTEX_INITIALIZER};
point_t shared_point;

// Lector: sin lock, retry si escritor intervino
point_t read_point(void)
{
    point_t p;
    unsigned s;
    do {
        s = atomic_load_explicit(&sl.seq, memory_order_acquire);
        if (s & 1) continue;  // escritura en progreso, reintentar

        p = shared_point;  // puede leer datos inconsistentes

        atomic_thread_fence(memory_order_acquire);
    } while (s != atomic_load_explicit(&sl.seq, memory_order_relaxed));
    // Si seq no cambió, los datos son consistentes

    return p;
}

// Escritor: adquiere mutex + incrementa seq
void write_point(point_t new_p)
{
    pthread_mutex_lock(&sl.wr_mtx);

    atomic_fetch_add_explicit(&sl.seq, 1, memory_order_release);  // impar
    shared_point = new_p;
    atomic_fetch_add_explicit(&sl.seq, 1, memory_order_release);  // par

    pthread_mutex_unlock(&sl.wr_mtx);
}
```

**Ventaja**: lectores nunca bloquean, ni entre sí ni a escritores.
**Desventaja**: lectores pueden reintentar si un escritor interviene.
Solo funciona para datos copiables (no para punteros que se siguen).

### Tabla comparativa

```
┌──────────────┬──────────┬──────────┬──────────┬──────────────┐
│ Mecanismo    │ Lectores │ Lectores │ Escritor │ Complejidad  │
│              │ paralelo │ bloquean │ bloquea  │              │
│              │          │ escritor │ lectores │              │
├──────────────┼──────────┼──────────┼──────────┼──────────────┤
│ mutex        │ No       │ N/A      │ N/A      │ Baja         │
├──────────────┼──────────┼──────────┼──────────┼──────────────┤
│ rwlock       │ Sí       │ Sí       │ Sí       │ Media        │
├──────────────┼──────────┼──────────┼──────────┼──────────────┤
│ seqlock      │ Sí       │ No       │ No       │ Alta         │
├──────────────┼──────────┼──────────┼──────────┼──────────────┤
│ RCU          │ Sí       │ No       │ No       │ Muy alta     │
├──────────────┼──────────┼──────────┼──────────┼──────────────┤
│ _Atomic      │ Sí       │ No       │ No       │ Baja (simple)│
│              │          │          │          │ Alta (CAS)   │
└──────────────┴──────────┴──────────┴──────────┴──────────────┘
```

---

## 11. Errores comunes

### Error 1: Modificar datos bajo rdlock

```c
// MAL: rdlock solo permite lectura
pthread_rwlock_rdlock(&rwl);
shared_data++;               // ← ¡data race! Otros readers leen simultáneamente
pthread_rwlock_unlock(&rwl);

// BIEN: wrlock para modificar
pthread_rwlock_wrlock(&rwl);
shared_data++;
pthread_rwlock_unlock(&rwl);
```

**Por qué importa**: múltiples threads pueden tener rdlock
simultáneamente. Si uno modifica datos, todos los demás leen datos
inconsistentes. El rdlock no impide la concurrencia de readers.

### Error 2: rdlock recursivo con PREFER_WRITER_NONRECURSIVE

```c
// MAL: deadlock con política prefer-writer
void outer(void) {
    pthread_rwlock_rdlock(&rwl);  // rdlock #1
    inner();
    pthread_rwlock_unlock(&rwl);
}

void inner(void) {
    pthread_rwlock_rdlock(&rwl);  // rdlock #2 — DEADLOCK si un writer espera
    // Con PREFER_WRITER_NONRECURSIVE: este rdlock se bloquea porque
    // hay un writer esperando, pero el writer espera a que rdlock #1
    // se libere, y rdlock #1 no se libera hasta que inner() retorne
    read_data();
    pthread_rwlock_unlock(&rwl);
}

// BIEN: reestructurar para evitar rdlock recursivo
void outer(void) {
    pthread_rwlock_rdlock(&rwl);
    read_data();  // inline lo que hacía inner()
    pthread_rwlock_unlock(&rwl);
}

// O usar PREFER_READER (default), que permite rdlock recursivo
```

### Error 3: Asumir que rwlock es más rápido sin medir

```c
// MAL: usar rwlock "por si acaso" para un contador simple
pthread_rwlock_t rwl;
int counter = 0;

void increment(void) {
    pthread_rwlock_wrlock(&rwl);  // siempre write
    counter++;
    pthread_rwlock_unlock(&rwl);
}
// rwlock tiene más overhead que mutex para escrituras puras
// Y _Atomic es aún mejor para un simple contador

// BIEN: analizar el ratio lectura:escritura
// Si > 90% lecturas y sección crítica > 1µs → rwlock
// Si escrituras frecuentes o sección corta → mutex
// Si es solo un int → _Atomic
```

### Error 4: Mantener wrlock durante I/O

```c
// MAL: bloquea a TODOS los lectores durante I/O
pthread_rwlock_wrlock(&rwl);
prepare_data(&config);
save_to_disk(&config);          // puede tardar 100ms
notify_subscribers(&config);    // puede tardar más
pthread_rwlock_unlock(&rwl);

// BIEN: minimizar tiempo con wrlock
new_config = prepare_offline();  // fuera del lock

pthread_rwlock_wrlock(&rwl);
config = new_config;             // swap rápido
pthread_rwlock_unlock(&rwl);

save_to_disk(&new_config);       // fuera del lock
notify_subscribers(&new_config);
```

### Error 5: Olvidar unlock en un camino de error

```c
// MAL: mismo problema que con mutex
int lookup(cache_t *c, const char *key, char *out)
{
    pthread_rwlock_rdlock(&c->rwl);
    entry_t *e = find(c, key);
    if (!e) {
        return -1;  // ← rwlock queda locked
    }
    strcpy(out, e->value);
    pthread_rwlock_unlock(&c->rwl);
    return 0;
}

// BIEN:
int lookup(cache_t *c, const char *key, char *out)
{
    pthread_rwlock_rdlock(&c->rwl);
    entry_t *e = find(c, key);
    int result = -1;
    if (e) {
        strcpy(out, e->value);
        result = 0;
    }
    pthread_rwlock_unlock(&c->rwl);
    return result;
}
```

---

## 12. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                      READ-WRITE LOCKS                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Inicialización:                                                    │
│    Estática: PTHREAD_RWLOCK_INITIALIZER                            │
│    Dinámica: pthread_rwlock_init(&rwl, &attr)                      │
│    Destruir: pthread_rwlock_destroy(&rwl)                          │
│                                                                     │
│  Operaciones:                                                       │
│    pthread_rwlock_rdlock(&rwl)     // shared (múltiples readers)   │
│    pthread_rwlock_wrlock(&rwl)     // exclusive (un solo writer)   │
│    pthread_rwlock_unlock(&rwl)     // liberar (ambos tipos)        │
│    pthread_rwlock_tryrdlock(&rwl)  // EBUSY si hay writer          │
│    pthread_rwlock_trywrlock(&rwl)  // EBUSY si hay readers/writer  │
│    pthread_rwlock_timedrdlock/timedwrlock  // con timeout          │
│                                                                     │
│  Compatibilidad de locks:                                           │
│    ┌──────────┬──────────┬──────────┐                              │
│    │          │ rdlock   │ wrlock   │                              │
│    ├──────────┼──────────┼──────────┤                              │
│    │ rdlock   │ ✅ ambos │ ❌ bloquea│                              │
│    │ wrlock   │ ❌ bloquea│ ❌ bloquea│                              │
│    └──────────┴──────────┴──────────┘                              │
│                                                                     │
│  Políticas (Linux, setkind_np):                                     │
│    PREFER_READER_NP (default) — writer starvation posible          │
│    PREFER_WRITER_NONRECURSIVE_NP — no rdlock recursivo             │
│                                                                     │
│  Cuándo usar rwlock:                                                │
│    ✅ Ratio lectura:escritura > 10:1                               │
│    ✅ Sección crítica de lectura > 1µs                             │
│    ✅ Muchos threads lectores (>4)                                  │
│    ❌ Escrituras frecuentes → mutex                                │
│    ❌ Sección crítica corta → mutex                                │
│    ❌ Una sola variable → _Atomic                                  │
│                                                                     │
│  Reglas de oro:                                                     │
│    1. NUNCA modificar datos bajo rdlock                            │
│    2. No asumir que rwlock > mutex sin benchmark                   │
│    3. Minimizar tiempo bajo wrlock (sacar I/O fuera)               │
│    4. Cuidado con rdlock recursivo + PREFER_WRITER                 │
│    5. No existe upgrade (rdlock→wrlock): unlock + wrlock           │
│                                                                     │
│  Compilar: gcc -o prog prog.c -pthread                             │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: DNS cache con rwlock

Implementa una caché DNS thread-safe donde múltiples threads
resuelven hostnames concurrentemente:

```c
#define CACHE_BUCKETS 256
#define MAX_HOST 256
#define MAX_IP   64

typedef struct dns_entry {
    char hostname[MAX_HOST];
    char ip[MAX_IP];
    time_t expires;
    struct dns_entry *next;
} dns_entry_t;

typedef struct {
    pthread_rwlock_t rwl;
    dns_entry_t *buckets[CACHE_BUCKETS];
    _Atomic int hits;
    _Atomic int misses;
} dns_cache_t;

// Lookup: rdlock (frecuente)
int dns_cache_lookup(dns_cache_t *c, const char *host, char *ip);

// Insert: wrlock (raro)
void dns_cache_insert(dns_cache_t *c, const char *host,
                      const char *ip, int ttl_seconds);

// Purge expired: wrlock (periódico)
int dns_cache_purge(dns_cache_t *c);
```

Benchmark con 16 threads: 95% lookups, 4% inserts, 1% purges.
Comparar rendimiento con un mutex global.

**Pregunta de reflexión**: Si un lookup encuentra una entrada expirada,
¿debería purgarla en ese momento? Eso requeriría upgrade de rdlock
a wrlock. ¿Cómo lo resolverías sin upgrade atómico?

### Ejercicio 2: Benchmark mutex vs rwlock vs atomic

Escribe un benchmark que mida el throughput (operaciones/segundo)
para tres estrategias protegiendo un array de 1000 ints:

```c
// Operación de lectura: sumar todos los elementos
// Operación de escritura: incrementar un elemento aleatorio

// Parámetros: N threads, ratio R:W (100:0, 99:1, 90:10, 50:50)
```

Para cada combinación, medir y generar una tabla. Implementar:
1. `pthread_mutex_t` global
2. `pthread_rwlock_t` global
3. `pthread_rwlock_t` per-element (1000 rwlocks)
4. `_Atomic int` per-element (sin lock)

**Pregunta de reflexión**: ¿La estrategia 3 (rwlock por elemento)
es mejor que la 2 (rwlock global) para lecturas? La lectura debe
sumar todo el array — ¿necesita lockear los 1000 rwlocks? ¿Qué
garantía de consistencia pierdes si lockeas uno a uno vs todos a la
vez?

### Ejercicio 3: Servicio de configuración hot-reload

Implementa un servicio que carga configuración desde un archivo y
la sirve a múltiples worker threads. La configuración se recarga
cuando recibe SIGUSR1:

```c
typedef struct {
    pthread_rwlock_t rwl;
    char db_host[256];
    int db_port;
    int max_workers;
    int log_level;
    char allowed_origins[10][256];
    int n_origins;
} app_config_t;

app_config_t g_config;

void config_load(app_config_t *cfg, const char *path);
// Parsea archivo INI simple

void handle_sigusr1(int sig);
// Recarga configuración (cuidado: signal handler + rwlock)
```

Requisitos:
- Workers leen config continuamente (rdlock)
- SIGUSR1 trigger reload (wrlock) — usar self-pipe trick porque
  rwlock no es async-signal-safe
- Medir latencia que ven los workers durante un reload
- Comparar con patrón RCU simplificado (puntero atómico a config)

**Pregunta de reflexión**: Durante un reload (wrlock), todos los
readers se bloquean. Si el parseo del archivo tarda 50ms, ¿los
workers se bloquean 50ms? ¿Cómo puedes reducir el tiempo de wrlock
al mínimo absoluto?
