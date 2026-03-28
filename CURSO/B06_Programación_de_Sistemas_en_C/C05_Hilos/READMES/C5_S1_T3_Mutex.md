# Mutex

## Índice
1. [Concepto de exclusión mutua](#1-concepto-de-exclusión-mutua)
2. [pthread_mutex básico](#2-pthread_mutex-básico)
3. [Inicialización y destrucción](#3-inicialización-y-destrucción)
4. [Lock, unlock y trylock](#4-lock-unlock-y-trylock)
5. [Timedlock](#5-timedlock)
6. [Tipos de mutex](#6-tipos-de-mutex)
7. [Deadlocks](#7-deadlocks)
8. [Granularidad del lock](#8-granularidad-del-lock)
9. [Mutex entre procesos](#9-mutex-entre-procesos)
10. [Patrones de uso](#10-patrones-de-uso)
11. [Rendimiento](#11-rendimiento)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Concepto de exclusión mutua

Un mutex (mutual exclusion) garantiza que solo **un thread** a la vez
ejecute una **sección crítica** — un bloque de código que accede a
datos compartidos:

```
  Sin mutex:                          Con mutex:
  Thread A    Thread B                Thread A       Thread B
  ────────    ────────                ────────       ────────
  LOAD  0     LOAD  0  ← race        lock()
  ADD   1     ADD   1                 LOAD  0        lock()
  STORE 1     STORE 1  ← perdido     ADD   1        [bloqueado]
                                      STORE 1        [bloqueado]
  counter=1 ✗                         unlock()
                                                     LOAD  1
                                                     ADD   2
                                                     STORE 2
                                                     unlock()
                                      counter=2 ✓
```

El mutex actúa como un candado: un thread lo "cierra" (lock), ejecuta
la sección crítica, y lo "abre" (unlock). Mientras está cerrado, otros
threads que intenten lock se **bloquean** (duermen) hasta que se libere.

---

## 2. pthread_mutex básico

```c
#include <pthread.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;

void *increment(void *arg)
{
    for (int i = 0; i < 1000000; i++) {
        pthread_mutex_lock(&mutex);
        counter++;                     // sección crítica
        pthread_mutex_unlock(&mutex);
    }
    return NULL;
}

int main(void)
{
    pthread_t t1, t2;
    pthread_create(&t1, NULL, increment, NULL);
    pthread_create(&t2, NULL, increment, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Counter: %d\n", counter);  // siempre 2000000
    return 0;
}
```

Compilar: `gcc -o mutex mutex.c -pthread`

---

## 3. Inicialización y destrucción

### Inicialización estática

Para mutexes en variables globales o static, usar la macro:

```c
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
// Listo para usar. No necesita destroy.
```

Es la forma más simple y segura. El mutex se inicializa en compilación,
sin race conditions.

### Inicialización dinámica

Cuando necesitas atributos especiales o el mutex está en el heap:

```c
pthread_mutex_t *mtx = malloc(sizeof(pthread_mutex_t));

pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);

int err = pthread_mutex_init(mtx, &attr);
if (err != 0) {
    fprintf(stderr, "mutex_init: %s\n", strerror(err));
    exit(1);
}
pthread_mutexattr_destroy(&attr);

// ... usar mtx ...

pthread_mutex_destroy(mtx);
free(mtx);
```

### Destrucción

```c
int pthread_mutex_destroy(pthread_mutex_t *mutex);
// Retorna: 0 en éxito, EBUSY si el mutex está locked
```

Reglas:
- No destruir un mutex que está locked
- No destruir un mutex si algún thread está bloqueado en él
- Después de destroy, el mutex no se puede usar (ni lock ni unlock)
- Un mutex inicializado con `PTHREAD_MUTEX_INITIALIZER` técnicamente
  no necesita destroy, pero es buena práctica

```c
// Verificar que no está en uso antes de destruir
int err = pthread_mutex_destroy(&mtx);
if (err == EBUSY) {
    fprintf(stderr, "mutex still locked!\n");
}
```

---

## 4. Lock, unlock y trylock

### pthread_mutex_lock — bloquear hasta obtener

```c
int pthread_mutex_lock(pthread_mutex_t *mutex);
// Si el mutex está libre: lo adquiere y retorna 0
// Si está locked por otro thread: BLOQUEA hasta que se libere
// Retorna: 0, o código de error
```

```c
pthread_mutex_lock(&mtx);
// --- sección crítica ---
// Solo un thread a la vez puede estar aquí
shared_data++;
// --- fin sección crítica ---
pthread_mutex_unlock(&mtx);
```

### pthread_mutex_unlock — liberar

```c
int pthread_mutex_unlock(pthread_mutex_t *mutex);
// Libera el mutex. Si hay threads bloqueados esperando,
// despierta a uno de ellos.
// Retorna: 0, o código de error
```

**Regla**: el thread que hizo lock **debe** ser el que haga unlock.
Hacer unlock desde otro thread es comportamiento indefinido con mutex
tipo DEFAULT o NORMAL.

### pthread_mutex_trylock — intentar sin bloquear

```c
int pthread_mutex_trylock(pthread_mutex_t *mutex);
// Si el mutex está libre: lo adquiere, retorna 0
// Si está locked: retorna EBUSY inmediatamente (no bloquea)
```

```c
if (pthread_mutex_trylock(&mtx) == 0) {
    // Obtuvimos el lock
    process_data();
    pthread_mutex_unlock(&mtx);
} else {
    // Ocupado — hacer otra cosa
    do_alternative_work();
}
```

Usos típicos de trylock:
- Evitar bloquear un thread principal (UI, event loop)
- Implementar backoff en contención alta
- Detectar deadlocks (si trylock falla en un lock que deberías tener,
  algo está mal)

---

## 5. Timedlock

```c
#include <pthread.h>
#include <time.h>

int pthread_mutex_timedlock(pthread_mutex_t *mutex,
                            const struct timespec *abs_timeout);
// Bloquea hasta obtener el mutex O hasta que expire el timeout
// Retorna: 0 si obtuvo el lock, ETIMEDOUT si expiró
```

El timeout es **absoluto** (no relativo):

```c
struct timespec ts;
clock_gettime(CLOCK_REALTIME, &ts);
ts.tv_sec += 5;  // 5 segundos desde ahora

int err = pthread_mutex_timedlock(&mtx, &ts);
if (err == ETIMEDOUT) {
    fprintf(stderr, "Could not acquire lock in 5 seconds\n");
    // Posible deadlock? Log y abortar?
} else if (err == 0) {
    // Obtuvimos el lock
    process();
    pthread_mutex_unlock(&mtx);
}
```

Útil para:
- Detectar deadlocks en producción (si un lock tarda > N segundos,
  probablemente hay deadlock)
- Implementar timeouts en operaciones que requieren mutex

---

## 6. Tipos de mutex

Los tipos controlan el comportamiento cuando un thread intenta lock
un mutex que **ya tiene**:

```c
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_settype(&attr, TYPE);
pthread_mutex_init(&mtx, &attr);
pthread_mutexattr_destroy(&attr);
```

### PTHREAD_MUTEX_DEFAULT / NORMAL

```
  Comportamiento si haces lock siendo ya el owner:
  → Comportamiento indefinido (deadlock en Linux)

  Comportamiento si haces unlock sin ser owner:
  → Comportamiento indefinido
```

Es el tipo por defecto (y el de `PTHREAD_MUTEX_INITIALIZER`). El más
rápido, pero no detecta errores de programación.

### PTHREAD_MUTEX_ERRORCHECK

```
  Lock si ya eres owner → retorna EDEADLK (no bloquea)
  Unlock sin ser owner  → retorna EPERM
  Unlock sin estar locked → retorna EPERM
```

```c
pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
pthread_mutex_init(&mtx, &attr);

pthread_mutex_lock(&mtx);
int err = pthread_mutex_lock(&mtx);  // segundo lock
if (err == EDEADLK) {
    printf("Detected self-deadlock!\n");
}

// unlock desde otro thread:
err = pthread_mutex_unlock(&mtx);  // sin ser owner
if (err == EPERM) {
    printf("Not the owner!\n");
}
```

**Recomendación**: usar ERRORCHECK durante desarrollo y testing. El
overhead es mínimo (~5%) y detecta bugs antes de producción.

### PTHREAD_MUTEX_RECURSIVE

```
  Lock si ya eres owner → incrementa un contador interno, retorna 0
  Unlock → decrementa el contador. Solo se libera cuando llega a 0
```

```c
pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
pthread_mutex_init(&mtx, &attr);

pthread_mutex_lock(&mtx);    // count = 1
pthread_mutex_lock(&mtx);    // count = 2 (no bloquea)
pthread_mutex_lock(&mtx);    // count = 3

pthread_mutex_unlock(&mtx);  // count = 2
pthread_mutex_unlock(&mtx);  // count = 1
pthread_mutex_unlock(&mtx);  // count = 0 → realmente liberado
```

Útil cuando una función protegida por mutex llama a otra función que
también necesita el mismo mutex:

```c
pthread_mutex_t mtx;  // recursive

void update_balance(int amount)
{
    pthread_mutex_lock(&mtx);
    balance += amount;
    log_transaction(amount);  // también usa mtx
    pthread_mutex_unlock(&mtx);
}

void log_transaction(int amount)
{
    pthread_mutex_lock(&mtx);   // si mtx es NORMAL → deadlock
    // Con RECURSIVE → funciona (count incrementa)
    write_log(balance, amount);
    pthread_mutex_unlock(&mtx);
}
```

**Advertencia**: los mutexes recursivos a menudo enmascaran diseño
incorrecto. Si necesitas recursive, considera refactorizar para que
las funciones internas no necesiten el lock (pasar la responsabilidad
de locking al caller).

### PTHREAD_MUTEX_ROBUST

No es un tipo sino un atributo ortogonal. Maneja el caso donde el
thread que tiene el lock **muere** (con `pthread_cancel` o `exit` del
thread):

```c
pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);
```

```c
int err = pthread_mutex_lock(&mtx);
if (err == EOWNERDEAD) {
    // El owner anterior murió con el lock
    // Los datos protegidos pueden estar inconsistentes
    repair_data();
    pthread_mutex_consistent(&mtx);  // marcar como recuperado
}
// Ahora somos el owner
```

Ya vimos esto en T01_Memoria_compartida — es especialmente útil con
`PTHREAD_PROCESS_SHARED` para mutexes entre procesos.

### Tabla comparativa

```
┌──────────────────┬──────────┬──────────┬───────────┬──────────┐
│ Tipo             │ Re-lock  │ Unlock   │ Unlock    │ Overhead │
│                  │ propio   │ ajeno    │ sin lock  │          │
├──────────────────┼──────────┼──────────┼───────────┼──────────┤
│ DEFAULT/NORMAL   │ UB       │ UB       │ UB        │ Mínimo   │
│                  │(deadlock)│          │           │          │
├──────────────────┼──────────┼──────────┼───────────┼──────────┤
│ ERRORCHECK       │ EDEADLK  │ EPERM    │ EPERM     │ ~5%      │
├──────────────────┼──────────┼──────────┼───────────┼──────────┤
│ RECURSIVE        │ OK       │ UB       │ UB        │ ~10%     │
│                  │(count++) │          │           │          │
├──────────────────┼──────────┼──────────┼───────────┼──────────┤
│ + ROBUST         │ depende  │ EPERM    │ EPERM     │ ~15%     │
│                  │ del tipo │          │           │          │
└──────────────────┴──────────┴──────────┴───────────┴──────────┘
UB = Undefined Behavior
```

---

## 7. Deadlocks

Un deadlock ocurre cuando dos o más threads se bloquean mutuamente
esperando recursos que el otro tiene.

### Condiciones necesarias (Coffman, 1971)

Las cuatro condiciones deben cumplirse **simultáneamente**:

```
1. Exclusión mutua    — los recursos no se comparten
2. Hold and wait      — un thread tiene un recurso y espera otro
3. No preemption      — no se puede quitar un recurso a un thread
4. Espera circular    — A espera a B, B espera a A
```

### Ejemplo clásico

```c
pthread_mutex_t mtx_a = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx_b = PTHREAD_MUTEX_INITIALIZER;

void *thread1(void *arg)
{
    pthread_mutex_lock(&mtx_a);    // tiene A
    sleep(1);                       // forzar el interleaving
    pthread_mutex_lock(&mtx_b);    // espera B → DEADLOCK
    // ... nunca llega aquí ...
    pthread_mutex_unlock(&mtx_b);
    pthread_mutex_unlock(&mtx_a);
    return NULL;
}

void *thread2(void *arg)
{
    pthread_mutex_lock(&mtx_b);    // tiene B
    sleep(1);
    pthread_mutex_lock(&mtx_a);    // espera A → DEADLOCK
    // ... nunca llega aquí ...
    pthread_mutex_unlock(&mtx_a);
    pthread_mutex_unlock(&mtx_b);
    return NULL;
}
```

```
  Thread 1                Thread 2
  ────────                ────────
  lock(A) ✓               lock(B) ✓
  lock(B) → bloqueado     lock(A) → bloqueado
       ↑                       ↑
       └───── DEADLOCK ────────┘
       Ambos esperan un lock que el otro tiene
```

### Estrategias para evitar deadlocks

#### 1. Lock ordering (la más práctica)

Definir un **orden total** de los mutexes y siempre adquirirlos en
ese orden:

```c
// Regla: siempre adquirir A antes que B
// (por dirección de memoria, o por un ID asignado)

void safe_lock_both(pthread_mutex_t *m1, pthread_mutex_t *m2)
{
    // Ordenar por dirección de memoria
    if (m1 < m2) {
        pthread_mutex_lock(m1);
        pthread_mutex_lock(m2);
    } else {
        pthread_mutex_lock(m2);
        pthread_mutex_lock(m1);
    }
}

void safe_unlock_both(pthread_mutex_t *m1, pthread_mutex_t *m2)
{
    pthread_mutex_unlock(m1);
    pthread_mutex_unlock(m2);
}
```

#### 2. Try-and-backoff

```c
void lock_both_no_deadlock(pthread_mutex_t *a, pthread_mutex_t *b)
{
    while (1) {
        pthread_mutex_lock(a);
        if (pthread_mutex_trylock(b) == 0)
            return;  // obtuvimos ambos
        // No pudimos obtener b → soltar a y reintentar
        pthread_mutex_unlock(a);
        usleep(rand() % 1000);  // backoff aleatorio
    }
}
```

#### 3. Un solo lock (simplificación)

Si la contención no es crítica, usar un solo mutex global:

```c
pthread_mutex_t global_mtx = PTHREAD_MUTEX_INITIALIZER;

// Todas las operaciones usan el mismo lock
void transfer(Account *from, Account *to, int amount)
{
    pthread_mutex_lock(&global_mtx);
    from->balance -= amount;
    to->balance += amount;
    pthread_mutex_unlock(&global_mtx);
}
```

Más simple, pero serializa todo el acceso.

### Detectar deadlocks

```bash
# Helgrind detecta orden de lock inconsistente
$ valgrind --tool=helgrind ./prog
==1234== Thread #2: lock order "0x601040 before 0x601060"
         first observed at ...
==1234== Thread #3: lock order "0x601060 before 0x601040"
         first observed at ...
==1234== This is a potential deadlock
```

```c
// En producción: usar timedlock para detectar
int err = pthread_mutex_timedlock(&mtx, &timeout_5s);
if (err == ETIMEDOUT) {
    fprintf(stderr, "DEADLOCK? Lock held for > 5s at %s:%d\n",
            __FILE__, __LINE__);
    abort();
}
```

---

## 8. Granularidad del lock

### Coarse-grained (grano grueso): un lock para todo

```c
pthread_mutex_t db_lock;

void db_insert(DB *db, Record *r) {
    pthread_mutex_lock(&db_lock);
    // serializa TODAS las operaciones
    insert_into_table(db, r);
    update_index(db, r);
    write_log(db, r);
    pthread_mutex_unlock(&db_lock);
}

void db_query(DB *db, Query *q) {
    pthread_mutex_lock(&db_lock);
    // un query bloquea inserts y viceversa
    execute_query(db, q);
    pthread_mutex_unlock(&db_lock);
}
```

**Ventajas**: simple, imposible tener deadlocks (un solo lock).
**Desventajas**: serializa todo, no escala con más cores.

### Fine-grained (grano fino): un lock por recurso

```c
typedef struct {
    pthread_mutex_t lock;
    char data[256];
    int refcount;
} bucket_t;

bucket_t buckets[N_BUCKETS];

void bucket_update(int idx, const char *data) {
    pthread_mutex_lock(&buckets[idx].lock);
    // solo bloquea UN bucket, los demás son paralelos
    strncpy(buckets[idx].data, data, 255);
    pthread_mutex_unlock(&buckets[idx].lock);
}
```

**Ventajas**: más paralelismo, escala mejor.
**Desventajas**: más complejo, riesgo de deadlocks si se adquieren
múltiples locks, más memoria por lock.

### Diagrama de trade-off

```
  Grano grueso                              Grano fino
  ◄────────────────────────────────────────────►
  Simple                                    Complejo
  Seguro (sin deadlocks)                    Riesgo de deadlocks
  No escala                                 Escala bien
  Baja contención (pocos threads)           Alta contención posible

  Recomendación: empezar con grano grueso,
  refinar solo si el profiling muestra contención
```

---

## 9. Mutex entre procesos

Un mutex puede compartirse entre procesos si:
1. Está en memoria compartida (mmap/shm)
2. Tiene el atributo `PTHREAD_PROCESS_SHARED`

```c
#include <pthread.h>
#include <sys/mman.h>

typedef struct {
    pthread_mutex_t mtx;
    int counter;
} shared_t;

// En el proceso que inicializa:
shared_t *shm = mmap(NULL, sizeof(shared_t),
                      PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_ANONYMOUS, -1, 0);

pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);

pthread_mutex_init(&shm->mtx, &attr);
pthread_mutexattr_destroy(&attr);
shm->counter = 0;

// Después del fork, tanto padre como hijo usan:
pthread_mutex_lock(&shm->mtx);
shm->counter++;
pthread_mutex_unlock(&shm->mtx);
```

Siempre combinar con `PTHREAD_MUTEX_ROBUST` para manejar el caso
donde un proceso muere con el lock adquirido:

```c
int err = pthread_mutex_lock(&shm->mtx);
if (err == EOWNERDEAD) {
    // El proceso que tenía el lock murió
    // Reparar datos si necesario
    shm->counter = validate_counter(shm->counter);
    pthread_mutex_consistent(&shm->mtx);
}
```

---

## 10. Patrones de uso

### 10.1 Proteger una estructura de datos

```c
typedef struct {
    pthread_mutex_t mtx;
    int *items;
    size_t count;
    size_t capacity;
} safe_array_t;

void safe_array_init(safe_array_t *arr, size_t cap)
{
    pthread_mutex_init(&arr->mtx, NULL);
    arr->items = malloc(cap * sizeof(int));
    arr->count = 0;
    arr->capacity = cap;
}

int safe_array_push(safe_array_t *arr, int item)
{
    pthread_mutex_lock(&arr->mtx);

    if (arr->count >= arr->capacity) {
        pthread_mutex_unlock(&arr->mtx);
        return -1;  // lleno
    }
    arr->items[arr->count++] = item;

    pthread_mutex_unlock(&arr->mtx);
    return 0;
}

int safe_array_pop(safe_array_t *arr, int *out)
{
    pthread_mutex_lock(&arr->mtx);

    if (arr->count == 0) {
        pthread_mutex_unlock(&arr->mtx);
        return -1;  // vacío
    }
    *out = arr->items[--arr->count];

    pthread_mutex_unlock(&arr->mtx);
    return 0;
}
```

### 10.2 Patrón lock-guard (cleanup automático)

En C no hay destructores automáticos, pero podemos usar
`pthread_cleanup_push` para garantizar unlock incluso si el thread
es cancelado:

```c
void safe_operation(pthread_mutex_t *mtx)
{
    pthread_mutex_lock(mtx);
    pthread_cleanup_push((void (*)(void *))pthread_mutex_unlock, mtx);

    // Si el thread es cancelado aquí (e.g., durante read()),
    // el cleanup handler hace unlock automáticamente
    risky_operation_that_might_cancel();

    pthread_cleanup_pop(1);  // 1 = ejecutar unlock
}
```

Macro helper para simular RAII:

```c
#define LOCK_GUARD(mtx) \
    pthread_mutex_lock(mtx); \
    pthread_cleanup_push((void(*)(void*))pthread_mutex_unlock, mtx)

#define LOCK_GUARD_END \
    pthread_cleanup_pop(1)

// Uso:
LOCK_GUARD(&mtx);
// ... sección crítica ...
LOCK_GUARD_END;
```

### 10.3 Producción: mutex + datos encapsulados

Encapsular el mutex con los datos que protege para evitar usar el
mutex equivocado:

```c
typedef struct {
    pthread_mutex_t lock;  // siempre al lado de los datos
    // --- datos protegidos por lock ---
    int balance;
    char owner[64];
    time_t last_access;
    // --- fin datos protegidos ---
} account_t;

void account_deposit(account_t *acc, int amount)
{
    pthread_mutex_lock(&acc->lock);
    acc->balance += amount;
    acc->last_access = time(NULL);
    pthread_mutex_unlock(&acc->lock);
}

void account_transfer(account_t *from, account_t *to, int amount)
{
    // Lock ordering por dirección para evitar deadlock
    account_t *first = (from < to) ? from : to;
    account_t *second = (from < to) ? to : from;

    pthread_mutex_lock(&first->lock);
    pthread_mutex_lock(&second->lock);

    if (from->balance >= amount) {
        from->balance -= amount;
        to->balance += amount;
    }

    pthread_mutex_unlock(&second->lock);
    pthread_mutex_unlock(&first->lock);
}
```

---

## 11. Rendimiento

### Costo de un mutex

```
  Operación                          Tiempo aprox. (x86_64)
  ─────────                          ──────────────────────
  lock + unlock (sin contención)     ~25 ns  (futex fast path)
  lock + unlock (con contención)     ~1-10 µs (syscall + context switch)
  atomic_fetch_add (sin contención)  ~5-15 ns
  Sin protección (baseline)          ~1 ns
```

### Contención

Cuando múltiples threads compiten por el mismo lock, el rendimiento
degrada:

```
  Threads    Throughput (ops/sec con mutex)
  ───────    ─────────────────────────────
  1          ~40M    (sin contención)
  2          ~15M    (algo de contención)
  4          ~6M     (contención moderada)
  8          ~3M     (alta contención)
  16         ~1.5M   (serialización casi total)
```

El escalamiento negativo ocurre porque el mutex **serializa** el
acceso. Con N threads y un solo lock, el throughput es peor que con
1 thread debido al overhead de context switching.

### Implementación en Linux: futex

En Linux, `pthread_mutex` usa **futex** (Fast Userspace muTEX):

```
  lock():
    1. Intentar CAS en userspace (atomic)
       → Si tuvo éxito: adquirido, sin syscall ✓
       → Si no: otro thread lo tiene, hacer syscall futex(FUTEX_WAIT)
                → kernel bloquea el thread

  unlock():
    1. Establecer estado a "libre" (atomic)
    2. Si hay waiters: syscall futex(FUTEX_WAKE)
       → kernel despierta un thread
```

El fast path (sin contención) no hace syscall — solo una operación
atómica en userspace. Esto hace que mutex sin contención sea muy
barato (~25ns).

### Cuándo preocuparse por rendimiento del mutex

1. **Profiling primero**: no optimizar sin medir. `perf` puede
   mostrar tiempo en `futex_wait`
2. **Reducir la sección crítica**: menos tiempo dentro del lock =
   menos contención
3. **Granularidad fina**: un lock por recurso en vez de global
4. **rwlock**: si lecturas >> escrituras (ver T02_Readwrite_locks)
5. **Lock-free**: atomics para casos simples (contadores, flags)
6. **Redesign**: evitar compartir datos entre threads

---

## 12. Errores comunes

### Error 1: Olvidar unlock en un camino de error

```c
// MAL: si validate falla, el mutex queda locked
int process(data_t *d)
{
    pthread_mutex_lock(&mtx);
    if (!validate(d)) {
        return -1;  // ← ¡mutex queda locked!
    }
    transform(d);
    pthread_mutex_unlock(&mtx);
    return 0;
}

// BIEN: unlock en todos los caminos de salida
int process(data_t *d)
{
    pthread_mutex_lock(&mtx);
    int result;
    if (!validate(d)) {
        result = -1;
    } else {
        transform(d);
        result = 0;
    }
    pthread_mutex_unlock(&mtx);
    return result;
}

// O con goto (patrón C clásico):
int process(data_t *d)
{
    int result = -1;
    pthread_mutex_lock(&mtx);

    if (!validate(d))
        goto out;

    transform(d);
    result = 0;

out:
    pthread_mutex_unlock(&mtx);
    return result;
}
```

**Por qué importa**: un mutex que nunca se libera bloquea a todos
los threads que intenten lock. Es un deadlock silencioso que solo se
manifiesta cuando se ejecuta el camino de error.

### Error 2: Lock ordering inconsistente

```c
// MAL: thread 1 adquiere A luego B, thread 2 adquiere B luego A
void *thread1(void *arg) {
    pthread_mutex_lock(&mtx_a);
    pthread_mutex_lock(&mtx_b);  // → deadlock con thread2
    // ...
}
void *thread2(void *arg) {
    pthread_mutex_lock(&mtx_b);
    pthread_mutex_lock(&mtx_a);  // → deadlock con thread1
    // ...
}

// BIEN: mismo orden siempre (A antes que B)
void *thread1(void *arg) {
    pthread_mutex_lock(&mtx_a);
    pthread_mutex_lock(&mtx_b);
    // ...
}
void *thread2(void *arg) {
    pthread_mutex_lock(&mtx_a);  // mismo orden
    pthread_mutex_lock(&mtx_b);
    // ...
}
```

### Error 3: Sección crítica demasiado larga

```c
// MAL: I/O dentro de la sección crítica
pthread_mutex_lock(&mtx);
prepare_data(&data);
write_to_network(fd, &data);  // puede tardar segundos
log_to_file(&data);           // I/O de disco
pthread_mutex_unlock(&mtx);
// Todos los threads esperan mientras uno hace I/O

// BIEN: sacar I/O fuera del lock
pthread_mutex_lock(&mtx);
data_copy = data;  // copia rápida dentro del lock
pthread_mutex_unlock(&mtx);
// I/O fuera del lock — no bloquea a nadie
write_to_network(fd, &data_copy);
log_to_file(&data_copy);
```

**Regla**: la sección crítica debe ser lo más corta posible.
Solo las operaciones que realmente acceden a datos compartidos
deben estar dentro del lock.

### Error 4: No verificar errores de pthread_mutex

```c
// MAL: ignorar retorno
pthread_mutex_lock(&mtx);  // ¿y si falla?

// BIEN: verificar (especialmente con ERRORCHECK/ROBUST)
int err = pthread_mutex_lock(&mtx);
if (err != 0) {
    if (err == EOWNERDEAD) {
        // robusto: el owner murió
        repair_data();
        pthread_mutex_consistent(&mtx);
    } else {
        fprintf(stderr, "mutex_lock: %s\n", strerror(err));
        abort();
    }
}
```

Con mutex DEFAULT, `pthread_mutex_lock` prácticamente nunca falla
(salvo `EINVAL` por mutex no inicializado). Pero con ERRORCHECK o
ROBUST, los errores son parte del flujo normal.

### Error 5: Proteger datos con el mutex equivocado

```c
// MAL: dos estructuras, dos mutexes, pero se cruzan
pthread_mutex_t mtx_a, mtx_b;
struct data_a { int x; } da;
struct data_b { int y; } db;

void *thread(void *arg) {
    pthread_mutex_lock(&mtx_a);
    da.x = 1;
    db.y = 2;  // ← protegido por mtx_a, pero db "pertenece" a mtx_b
    pthread_mutex_unlock(&mtx_a);
    // Otro thread accede a db.y bajo mtx_b → race condition
}
```

**Solución**: encapsular mutex + datos en la misma estructura (patrón
de la sección 10.3). Documentar qué mutex protege qué datos.

---

## 13. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                           MUTEX                                    │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Inicialización:                                                    │
│    Estática: pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;        │
│    Dinámica: pthread_mutex_init(&m, &attr);                        │
│    Destruir: pthread_mutex_destroy(&m);                            │
│                                                                     │
│  Operaciones:                                                       │
│    pthread_mutex_lock(&m)        // bloquear hasta obtener         │
│    pthread_mutex_unlock(&m)      // liberar                        │
│    pthread_mutex_trylock(&m)     // EBUSY si no disponible         │
│    pthread_mutex_timedlock(&m,t) // ETIMEDOUT si timeout           │
│                                                                     │
│  Tipos (pthread_mutexattr_settype):                                 │
│    DEFAULT/NORMAL  — rápido, UB en re-lock o unlock ajeno          │
│    ERRORCHECK      — retorna EDEADLK/EPERM en errores              │
│    RECURSIVE       — permite re-lock (cuenta), misma regla unlock  │
│                                                                     │
│  Atributos:                                                         │
│    settype(attr, TYPE)                                              │
│    setpshared(attr, PTHREAD_PROCESS_SHARED)  // entre procesos     │
│    setrobust(attr, PTHREAD_MUTEX_ROBUST)     // recovery si muere  │
│    setprioceiling / setprotocol              // priority inversion │
│                                                                     │
│  Evitar deadlocks:                                                  │
│    1. Lock ordering: siempre adquirir en el mismo orden            │
│    2. Trylock + backoff: no bloquear indefinidamente               │
│    3. Un solo lock (sacrificar paralelismo por simplicidad)         │
│    4. timedlock para detectar en producción                        │
│    5. Helgrind / TSan para detectar en desarrollo                  │
│                                                                     │
│  Rendimiento:                                                       │
│    Sin contención: ~25ns (futex fast path, sin syscall)            │
│    Con contención: ~1-10µs (syscall + context switch)              │
│    Regla: sección crítica lo más corta posible                     │
│    Regla: sacar I/O fuera del lock                                 │
│                                                                     │
│  Patrones:                                                          │
│    - Encapsular mutex + datos en misma struct                      │
│    - goto out + unlock para error handling                         │
│    - cleanup_push(unlock) para cancelación                         │
│    - Lock ordering por dirección para transfer(a, b)               │
│                                                                     │
│  Compilar: gcc -o prog prog.c -pthread                             │
│  Debug: valgrind --tool=helgrind ./prog                            │
│         gcc -fsanitize=thread -g prog.c -pthread                   │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 14. Ejercicios

### Ejercicio 1: Cuenta bancaria thread-safe

Implementa un sistema de cuentas bancarias con operaciones deposit,
withdraw y transfer. Transfer debe adquirir dos locks sin deadlock:

```c
typedef struct {
    pthread_mutex_t lock;
    int id;
    int balance;
} account_t;

int account_deposit(account_t *acc, int amount);
int account_withdraw(account_t *acc, int amount);
int account_transfer(account_t *from, account_t *to, int amount);
// transfer retorna -1 si fondos insuficientes
```

Requisitos:
- Lock ordering por dirección de memoria (o por id)
- Lanzar 8 threads que hagan 100000 transfers aleatorios entre 4
  cuentas
- Al final, verificar que la suma total de todos los balances no
  cambió (invariante de conservación)
- Usar ERRORCHECK durante desarrollo para detectar bugs

**Pregunta de reflexión**: Si cambias el lock ordering de "por
dirección" a "por id", ¿funciona igual? ¿Qué pasa si dos cuentas
tienen el mismo id? ¿Hay alguna ventaja de usar id sobre dirección?

### Ejercicio 2: Linked list thread-safe con lock striping

Implementa una linked list ordenada con tres estrategias de locking
y compara rendimiento:

```c
// Estrategia 1: un mutex global
typedef struct {
    pthread_mutex_t lock;
    node_t *head;
} coarse_list_t;

// Estrategia 2: lock por nodo (hand-over-hand locking)
typedef struct node {
    pthread_mutex_t lock;
    int value;
    struct node *next;
} fine_node_t;

// Estrategia 3: optimistic (buscar sin lock, validar con lock)
```

Hand-over-hand: al recorrer la lista, lockeas el nodo actual y el
siguiente antes de soltar el anterior. Esto permite que múltiples
threads recorran la lista en paralelo.

Benchmark con N threads haciendo inserts y lookups aleatorios.

**Pregunta de reflexión**: El hand-over-hand locking adquiere y
libera un lock por cada nodo de la lista. Si la lista tiene 1000
nodos, ¿son 1000 lock/unlock para un lookup? ¿Cuándo es esto mejor
que un solo lock global?

### Ejercicio 3: Detector de deadlocks

Implementa un wrapper sobre `pthread_mutex_lock` que mantenga un
grafo de dependencias y detecte ciclos:

```c
// Registrar qué thread tiene qué mutex
// Al intentar lock, verificar que no se forma un ciclo

typedef struct {
    pthread_mutex_t real_mutex;
    int id;               // ID del mutex para el grafo
    pthread_t owner;      // thread que lo tiene
    int locked;
} tracked_mutex_t;

int tracked_mutex_lock(tracked_mutex_t *m);
int tracked_mutex_unlock(tracked_mutex_t *m);
// tracked_mutex_lock verifica el grafo antes de hacer lock real
// Si detecta ciclo → imprime warning con los mutexes involucrados
```

El grafo de espera: nodo = thread, arista A→B significa "thread A
espera un mutex que thread B tiene". Un **ciclo** en este grafo es
un deadlock.

**Pregunta de reflexión**: ¿Es posible detectar deadlocks **antes**
de que ocurran (predicción) o solo cuando ya están ocurriendo
(detección)? ¿Qué hace Helgrind — predice o detecta? ¿Cuál es el
overhead de mantener el grafo en producción?
