# Variables de condición

## Índice
1. [El problema: esperar una condición](#1-el-problema-esperar-una-condición)
2. [pthread_cond_wait](#2-pthread_cond_wait)
3. [pthread_cond_signal y broadcast](#3-pthread_cond_signal-y-broadcast)
4. [Spurious wakeups](#4-spurious-wakeups)
5. [El patrón canónico](#5-el-patrón-canónico)
6. [pthread_cond_timedwait](#6-pthread_cond_timedwait)
7. [Inicialización y destrucción](#7-inicialización-y-destrucción)
8. [Patrones clásicos](#8-patrones-clásicos)
9. [Signal vs broadcast: cuándo usar cada uno](#9-signal-vs-broadcast-cuándo-usar-cada-uno)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El problema: esperar una condición

Un mutex protege datos compartidos, pero no resuelve el problema de
**esperar** a que una condición se cumpla. Ejemplo: un consumidor
necesita esperar a que haya datos en una cola.

### Solución incorrecta 1: busy wait

```c
// MAL: quema CPU esperando
pthread_mutex_lock(&mtx);
while (queue_empty(&q)) {
    pthread_mutex_unlock(&mtx);
    // Otro thread podría insertar aquí, pero...
    usleep(1000);  // ...dormimos de todas formas
    pthread_mutex_lock(&mtx);
}
item = queue_pop(&q);
pthread_mutex_unlock(&mtx);
```

Problemas: desperdicio de CPU, latencia variable (1ms mínimo de
delay), y el mutex se suelta y retoma constantemente.

### Solución incorrecta 2: sleep sin lock

```c
// MAL: race condition
while (queue_empty(&q)) {
    sleep(1);  // ¿y si alguien inserta justo ahora?
}
pthread_mutex_lock(&mtx);
item = queue_pop(&q);  // ¿sigue habiendo datos?
pthread_mutex_unlock(&mtx);
```

La verificación y la acción no son atómicas. Otro thread podría
consumir el dato entre el `while` y el `lock`.

### La solución: condvar

Una **variable de condición** (condvar) permite que un thread:
1. Libere el mutex
2. Se duerma
3. Sea despertado cuando la condición pueda haber cambiado
4. Re-adquiera el mutex

Todo atómicamente (pasos 1 y 2 son indivisibles):

```
  Consumidor                  Productor
  ──────────                  ─────────
  lock(mtx)
  while (empty) {
    cond_wait(cond, mtx)───── (duerme, libera mtx)
                              lock(mtx)
                              insert(data)
                              cond_signal(cond)
                              unlock(mtx)
    (despierta, re-lock mtx)
  }
  item = pop()
  unlock(mtx)
```

La atomicidad de "liberar mutex + dormir" es crucial. Si no fueran
atómicas:

```
  Race condition sin atomicidad:
  ──────────────────────────────
  Consumidor: unlock(mtx)
  Productor:  lock(mtx), insert, signal  ← señal perdida
  Consumidor: sleep(cond)  ← duerme DESPUÉS de la señal → no despierta
```

---

## 2. pthread_cond_wait

```c
#include <pthread.h>

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex);
// Precondición: mutex DEBE estar locked por el thread que llama
// 1. Libera mutex (atómicamente con paso 2)
// 2. Bloquea el thread en cond
// --- thread duerme ---
// 3. Cuando es despertado, re-adquiere mutex
// 4. Retorna (mutex está locked)
// Retorna: 0 en éxito
```

```c
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
int data_ready = 0;

// Consumidor (espera)
pthread_mutex_lock(&mtx);
while (!data_ready) {              // loop por spurious wakeups
    pthread_cond_wait(&cond, &mtx);
    // Al retornar, mtx está locked de nuevo
}
// data_ready es true, mtx está locked
process(data);
pthread_mutex_unlock(&mtx);
```

### Diagrama de estados del thread

```
  ┌─────────┐    lock(mtx)    ┌─────────┐
  │ Running │ ───────────────► │ Has mtx │
  └─────────┘                  └────┬────┘
                                    │
                               cond_wait()
                                    │
                    ┌───────────────┼───────────────┐
                    │ atómicamente: │               │
                    │ 1. unlock mtx │               │
                    │ 2. sleep      │               │
                    ▼               │               │
              ┌──────────┐         │               │
              │ Sleeping │         │               │
              │ on cond  │         │               │
              └────┬─────┘         │               │
                   │               │               │
              signal/broadcast     │               │
                   │               │               │
              ┌────▼─────┐         │               │
              │ Waiting  │         │               │
              │ for mtx  │ ◄──────┘               │
              └────┬─────┘                         │
                   │ re-lock mtx                   │
                   │                               │
              ┌────▼─────┐                         │
              │ Has mtx  │ (cond_wait retorna)     │
              │ (de      │                         │
              │  nuevo)  │                         │
              └──────────┘                         │
```

---

## 3. pthread_cond_signal y broadcast

### signal — despertar UN thread

```c
int pthread_cond_signal(pthread_cond_t *cond);
// Despierta AL MENOS un thread que está en cond_wait
// Si nadie espera, no hace nada (la señal se pierde)
```

### broadcast — despertar TODOS los threads

```c
int pthread_cond_broadcast(pthread_cond_t *cond);
// Despierta TODOS los threads que están en cond_wait
// Cada uno re-adquiere el mutex uno a uno
```

### ¿Señalizar dentro o fuera del lock?

Ambas opciones son correctas, pero tienen diferentes trade-offs:

```c
// Opción A: signal DENTRO del lock
pthread_mutex_lock(&mtx);
data_ready = 1;
pthread_cond_signal(&cond);  // señalizar
pthread_mutex_unlock(&mtx);
// El thread despertado intenta lock(mtx) pero nosotros todavía
// lo tenemos → se bloquea brevemente hasta nuestro unlock

// Opción B: signal FUERA del lock
pthread_mutex_lock(&mtx);
data_ready = 1;
pthread_mutex_unlock(&mtx);
pthread_cond_signal(&cond);  // señalizar después de unlock
// El thread despertado puede adquirir mtx inmediatamente
```

**Opción A** (dentro) es más segura y predecible. Evita una race
condition sutil donde entre el unlock y el signal, otro thread podría
modificar el estado. POSIX permite ambas.

**Opción B** (fuera) puede evitar un context switch innecesario
(hurry-up-and-wait: despertar un thread que inmediatamente se bloquea
en el mutex). Algunos sistemas optimizan esto (wait morphing).

**Recomendación**: señalizar dentro del lock a menos que tengas una
razón medida para no hacerlo.

---

## 4. Spurious wakeups

Un thread puede despertar de `cond_wait` **sin que nadie haya llamado
signal o broadcast**. Esto se llama **spurious wakeup** (despertar
espurio).

¿Por qué ocurre?
- Implementación del kernel (futex puede despertar por señales,
  fork, o rebalanceo de waitqueues)
- Interceptación por signal handlers
- Optimización de rendimiento en la implementación

POSIX lo permite explícitamente:

> "Spurious wakeups from the pthread_cond_timedwait() or
> pthread_cond_wait() functions may occur."

### Por qué el loop es obligatorio

```c
// MAL: if en vez de while
pthread_mutex_lock(&mtx);
if (!data_ready) {                 // ← if, no while
    pthread_cond_wait(&cond, &mtx);
}
// Spurious wakeup → data_ready sigue siendo false
// Pero el código continúa como si fuera true → BUG
process(data);  // ¡data no está lista!
pthread_mutex_unlock(&mtx);

// BIEN: while — siempre re-verificar la condición
pthread_mutex_lock(&mtx);
while (!data_ready) {              // ← while
    pthread_cond_wait(&cond, &mtx);
    // Si spurious wakeup, el while re-verifica
}
// Aquí data_ready es VERDADERAMENTE true
process(data);
pthread_mutex_unlock(&mtx);
```

El `while` también maneja otro caso: si hay múltiples consumidores y
solo se produjo un dato, `broadcast` despierta a todos pero solo uno
debería consumir. Los demás vuelven a dormir en el `while`.

---

## 5. El patrón canónico

Todo uso correcto de condvars sigue este patrón:

### Lado del que espera (consumidor)

```c
pthread_mutex_lock(&mtx);

while (!condición) {                    // 1. Verificar condición
    pthread_cond_wait(&cond, &mtx);     // 2. Dormir si no se cumple
}
// 3. La condición se cumple, mtx está locked
actuar_sobre_datos();

pthread_mutex_unlock(&mtx);
```

### Lado del que señaliza (productor)

```c
pthread_mutex_lock(&mtx);

modificar_datos();                      // 1. Modificar estado
condición = true;                       //    (hacer la condición verdadera)
pthread_cond_signal(&cond);             // 2. Despertar un waiter

pthread_mutex_unlock(&mtx);
```

### Los tres componentes

```
  ┌─────────────────────────────────────────────────┐
  │  1. pthread_mutex_t mtx    — protege los datos  │
  │  2. pthread_cond_t cond    — mecanismo de espera│
  │  3. Predicado (condición)  — cuándo actuar      │
  │     (e.g., !queue_empty, data_ready, count > 0) │
  └─────────────────────────────────────────────────┘

  Los tres están acoplados:
  - El mutex protege el predicado
  - La condvar señaliza que el predicado PUEDE haber cambiado
  - El while loop RE-VERIFICA el predicado
```

Errores si falta alguno:
- Sin mutex: race condition al verificar/modificar el predicado
- Sin condvar: busy wait (CPU quemado)
- Sin predicado/while: spurious wakeups causan bugs

---

## 6. pthread_cond_timedwait

```c
int pthread_cond_timedwait(pthread_cond_t *cond,
                           pthread_mutex_t *mutex,
                           const struct timespec *abstime);
// Como cond_wait pero con timeout absoluto
// Retorna: 0 si fue señalizado, ETIMEDOUT si expiró el timeout
```

```c
struct timespec ts;
clock_gettime(CLOCK_REALTIME, &ts);
ts.tv_sec += 5;  // 5 segundos desde ahora

pthread_mutex_lock(&mtx);
while (!data_ready) {
    int err = pthread_cond_timedwait(&cond, &mtx, &ts);
    if (err == ETIMEDOUT) {
        // Timeout: data no llegó en 5 segundos
        pthread_mutex_unlock(&mtx);
        return -1;
    }
}
process(data);
pthread_mutex_unlock(&mtx);
```

### Usar CLOCK_MONOTONIC (Linux)

Por defecto, `timedwait` usa `CLOCK_REALTIME`, que puede saltar
(NTP, ajuste manual). Para timeouts robustos, usar `CLOCK_MONOTONIC`:

```c
pthread_condattr_t cattr;
pthread_condattr_init(&cattr);
pthread_condattr_setclock(&cattr, CLOCK_MONOTONIC);

pthread_cond_t cond;
pthread_cond_init(&cond, &cattr);
pthread_condattr_destroy(&cattr);

// Ahora usar clock_gettime(CLOCK_MONOTONIC, &ts) para el timeout
struct timespec ts;
clock_gettime(CLOCK_MONOTONIC, &ts);
ts.tv_sec += 5;
pthread_cond_timedwait(&cond, &mtx, &ts);
```

### Helper para timeout relativo

```c
int cond_wait_timeout_ms(pthread_cond_t *cond, pthread_mutex_t *mtx,
                         int timeout_ms)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);  // o REALTIME si la cond no es MONOTONIC

    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec++;
        ts.tv_nsec -= 1000000000L;
    }

    return pthread_cond_timedwait(cond, mtx, &ts);
}
```

---

## 7. Inicialización y destrucción

### Estática

```c
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
// Usa CLOCK_REALTIME por defecto
```

### Dinámica

```c
pthread_condattr_t attr;
pthread_condattr_init(&attr);
pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
pthread_condattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);  // entre procesos

pthread_cond_t cond;
pthread_cond_init(&cond, &attr);
pthread_condattr_destroy(&attr);
```

### Destrucción

```c
int pthread_cond_destroy(pthread_cond_t *cond);
// No destruir si hay threads en cond_wait
// Retorna: EBUSY si hay waiters
```

### Atributos disponibles

```
┌───────────────────────┬──────────────────────────────────────┐
│ Atributo              │ Descripción                          │
├───────────────────────┼──────────────────────────────────────┤
│ clock (setclock)      │ CLOCK_REALTIME (default) o           │
│                       │ CLOCK_MONOTONIC para timedwait       │
├───────────────────────┼──────────────────────────────────────┤
│ pshared (setpshared)  │ PTHREAD_PROCESS_PRIVATE (default) o  │
│                       │ PTHREAD_PROCESS_SHARED               │
└───────────────────────┴──────────────────────────────────────┘
```

---

## 8. Patrones clásicos

### 8.1 Cola productor-consumidor (bounded buffer)

El patrón más común. Dos condiciones: "no vacía" (para consumidores)
y "no llena" (para productores):

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define QUEUE_SIZE 10

typedef struct {
    int items[QUEUE_SIZE];
    int head, tail, count;
    pthread_mutex_t mtx;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} bounded_queue_t;

void bq_init(bounded_queue_t *q)
{
    q->head = q->tail = q->count = 0;
    pthread_mutex_init(&q->mtx, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void bq_push(bounded_queue_t *q, int item)
{
    pthread_mutex_lock(&q->mtx);

    while (q->count == QUEUE_SIZE)           // cola llena
        pthread_cond_wait(&q->not_full, &q->mtx);

    q->items[q->tail] = item;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;

    pthread_cond_signal(&q->not_empty);      // despertar consumidor
    pthread_mutex_unlock(&q->mtx);
}

int bq_pop(bounded_queue_t *q)
{
    pthread_mutex_lock(&q->mtx);

    while (q->count == 0)                    // cola vacía
        pthread_cond_wait(&q->not_empty, &q->mtx);

    int item = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;

    pthread_cond_signal(&q->not_full);       // despertar productor
    pthread_mutex_unlock(&q->mtx);
    return item;
}

void bq_destroy(bounded_queue_t *q)
{
    pthread_mutex_destroy(&q->mtx);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}
```

Uso:

```c
bounded_queue_t q;
bq_init(&q);

void *producer(void *arg)
{
    for (int i = 0; i < 100; i++)
        bq_push(&q, i);
    return NULL;
}

void *consumer(void *arg)
{
    for (int i = 0; i < 100; i++)
        printf("Got: %d\n", bq_pop(&q));
    return NULL;
}
```

### 8.2 Barrera (todos esperan a todos)

N threads esperan a que todos lleguen a un punto antes de continuar:

```c
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t cv;
    int count;
    int target;
    int generation;  // evitar race en barreras reutilizables
} barrier_t;

void barrier_init(barrier_t *b, int n)
{
    pthread_mutex_init(&b->mtx, NULL);
    pthread_cond_init(&b->cv, NULL);
    b->count = 0;
    b->target = n;
    b->generation = 0;
}

void barrier_wait(barrier_t *b)
{
    pthread_mutex_lock(&b->mtx);

    int gen = b->generation;
    b->count++;

    if (b->count == b->target) {
        // Último en llegar: resetear y despertar a todos
        b->count = 0;
        b->generation++;
        pthread_cond_broadcast(&b->cv);
    } else {
        // No el último: esperar
        while (gen == b->generation)
            pthread_cond_wait(&b->cv, &b->mtx);
    }

    pthread_mutex_unlock(&b->mtx);
}
```

El campo `generation` evita un bug sutil: sin él, un thread rápido
podría re-entrar a la barrera y ver `count < target` de la generación
anterior.

### 8.3 Thread pool con work queue

```c
typedef struct {
    void (*func)(void *arg);
    void *arg;
} task_t;

typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t work_available;
    task_t *tasks;
    int capacity;
    int head, tail, count;
    int shutdown;
    pthread_t *threads;
    int n_threads;
} thread_pool_t;

void *pool_worker(void *arg)
{
    thread_pool_t *pool = (thread_pool_t *)arg;

    while (1) {
        pthread_mutex_lock(&pool->mtx);

        while (pool->count == 0 && !pool->shutdown)
            pthread_cond_wait(&pool->work_available, &pool->mtx);

        if (pool->shutdown && pool->count == 0) {
            pthread_mutex_unlock(&pool->mtx);
            return NULL;
        }

        // Extraer tarea
        task_t task = pool->tasks[pool->head];
        pool->head = (pool->head + 1) % pool->capacity;
        pool->count--;

        pthread_mutex_unlock(&pool->mtx);

        // Ejecutar fuera del lock
        task.func(task.arg);
    }
}

void pool_submit(thread_pool_t *pool, void (*func)(void *), void *arg)
{
    pthread_mutex_lock(&pool->mtx);

    // Esperar si la cola está llena (o descartar, según política)
    while (pool->count == pool->capacity)
        pthread_cond_wait(&pool->work_available, &pool->mtx);
        // Nota: necesitarías otra condvar "not_full" para esto

    pool->tasks[pool->tail] = (task_t){.func = func, .arg = arg};
    pool->tail = (pool->tail + 1) % pool->capacity;
    pool->count++;

    pthread_cond_signal(&pool->work_available);
    pthread_mutex_unlock(&pool->mtx);
}

void pool_shutdown(thread_pool_t *pool)
{
    pthread_mutex_lock(&pool->mtx);
    pool->shutdown = 1;
    pthread_cond_broadcast(&pool->work_available);  // despertar a todos
    pthread_mutex_unlock(&pool->mtx);

    for (int i = 0; i < pool->n_threads; i++)
        pthread_join(pool->threads[i], NULL);
}
```

### 8.4 One-time initialization (alternativa a pthread_once)

```c
pthread_mutex_t init_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t init_cv = PTHREAD_COND_INITIALIZER;
int initialized = 0;
int initializing = 0;

void ensure_initialized(void)
{
    pthread_mutex_lock(&init_mtx);

    if (initialized) {
        pthread_mutex_unlock(&init_mtx);
        return;
    }

    if (initializing) {
        // Otro thread está inicializando — esperar
        while (!initialized)
            pthread_cond_wait(&init_cv, &init_mtx);
        pthread_mutex_unlock(&init_mtx);
        return;
    }

    initializing = 1;
    pthread_mutex_unlock(&init_mtx);

    // Inicialización lenta fuera del lock
    do_expensive_init();

    pthread_mutex_lock(&init_mtx);
    initialized = 1;
    pthread_cond_broadcast(&init_cv);
    pthread_mutex_unlock(&init_mtx);
}
```

En la práctica, `pthread_once` es más simple para este caso.

---

## 9. Signal vs broadcast: cuándo usar cada uno

```
┌──────────────────┬────────────────────────┬────────────────────────┐
│ Situación        │ signal                 │ broadcast              │
├──────────────────┼────────────────────────┼────────────────────────┤
│ Un waiter puede  │ ✅ Correcto            │ ✅ Correcto (pero      │
│ manejar la       │ (más eficiente)        │    overkill)           │
│ condición        │                        │                        │
├──────────────────┼────────────────────────┼────────────────────────┤
│ Múltiples waiters│ ❌ Solo despierta uno, │ ✅ Todos re-verifican  │
│ deben re-evaluar │ los demás siguen       │    y actúan si pueden  │
│                  │ durmiendo              │                        │
├──────────────────┼────────────────────────┼────────────────────────┤
│ Cambio de        │ ❌ Puede despertar uno │ ✅ Todos se enteran    │
│ predicado        │ que no puede actuar    │                        │
│ múltiple         │ (deadlock funcional)   │                        │
├──────────────────┼────────────────────────┼────────────────────────┤
│ Shutdown         │ ❌                     │ ✅ Todos deben         │
│                  │                        │    terminar            │
└──────────────────┴────────────────────────┴────────────────────────┘
```

### Reglas prácticas

1. **Productor-consumidor con un solo consumidor**: `signal` es
   suficiente y más eficiente.

2. **Productor-consumidor con múltiples consumidores y un dato por
   señal**: `signal` funciona — despierta a uno, que consume el dato.

3. **Cambio de estado global** (shutdown, config reload): usar
   `broadcast` — todos deben enterarse.

4. **Múltiples condiciones en una sola condvar**: usar `broadcast` —
   cada thread re-verifica SU condición.

5. **En duda**: usar `broadcast`. Es correcto en todos los casos,
   solo potencialmente menos eficiente (thundering herd).

### El problema del thundering herd

Con `broadcast` y N waiters, todos se despiertan y compiten por el
mutex. Solo uno actúa, los demás vuelven a dormir. Con muchos waiters,
esto desperdicia CPU:

```
  broadcast con 100 waiters y 1 dato:
  ┌──────────────────────────────────────────────┐
  │ Thread 1: wakes up → locks mtx → pops item  │
  │ Thread 2: wakes up → locks mtx → queue empty → sleep │
  │ Thread 3: wakes up → locks mtx → queue empty → sleep │
  │ ...                                          │
  │ Thread 100: wakes up → locks mtx → empty → sleep │
  └──────────────────────────────────────────────┘
  99 threads despertaron para nada
```

`signal` solo despierta uno → más eficiente para este caso.

---

## 10. Errores comunes

### Error 1: if en vez de while (no manejar spurious wakeup)

```c
// MAL
pthread_mutex_lock(&mtx);
if (queue_empty(&q)) {         // ← if
    pthread_cond_wait(&cond, &mtx);
}
item = queue_pop(&q);          // spurious wakeup → pop de cola vacía → crash
pthread_mutex_unlock(&mtx);

// BIEN
pthread_mutex_lock(&mtx);
while (queue_empty(&q)) {     // ← while
    pthread_cond_wait(&cond, &mtx);
}
item = queue_pop(&q);          // seguro: la cola tiene datos
pthread_mutex_unlock(&mtx);
```

**Por qué importa**: es el error más frecuente con condvars. Funciona
"casi siempre" en pruebas locales pero falla esporádicamente en
producción con carga alta — el peor tipo de bug.

### Error 2: cond_wait sin tener el mutex locked

```c
// MAL: condvar sin mutex
pthread_cond_wait(&cond, &mtx);  // mtx no está locked
// Comportamiento indefinido

// BIEN:
pthread_mutex_lock(&mtx);
while (!condition)
    pthread_cond_wait(&cond, &mtx);  // mtx está locked ✓
pthread_mutex_unlock(&mtx);
```

### Error 3: signal sin modificar el predicado

```c
// MAL: señalizar sin cambiar el estado
pthread_cond_signal(&cond);
// El thread despierta, re-verifica while(!data_ready)...
// data_ready sigue siendo false → vuelve a dormir
// Señal desperdiciada

// BIEN: modificar el estado ANTES de señalizar
pthread_mutex_lock(&mtx);
data_ready = 1;                  // modificar predicado
pthread_cond_signal(&cond);      // señalizar
pthread_mutex_unlock(&mtx);
```

### Error 4: usar signal cuando se necesita broadcast

```c
// MAL: múltiples consumidores, cambio de estado global
pthread_mutex_lock(&mtx);
shutdown = 1;
pthread_cond_signal(&cond);   // solo despierta UNO
pthread_mutex_unlock(&mtx);
// Los demás N-1 consumidores siguen durmiendo

// BIEN: broadcast para despertar a todos
pthread_mutex_lock(&mtx);
shutdown = 1;
pthread_cond_broadcast(&cond);  // despierta a TODOS
pthread_mutex_unlock(&mtx);
```

### Error 5: señal perdida (signal antes de wait)

```c
// MAL: signal antes de que alguien esté esperando
// Productor: cond_signal(&cond)  ← nadie espera → señal perdida
// ... tiempo pasa ...
// Consumidor: cond_wait(&cond, &mtx)  ← bloquea para siempre

// BIEN: el predicado resuelve esto
// Productor:
pthread_mutex_lock(&mtx);
data_ready = 1;               // establecer predicado
pthread_cond_signal(&cond);
pthread_mutex_unlock(&mtx);

// Consumidor:
pthread_mutex_lock(&mtx);
while (!data_ready)            // verifica ANTES de wait
    pthread_cond_wait(&cond, &mtx);
// Si data_ready ya es true, no entra al wait → no pierde señal
```

El predicado (la variable de estado) es lo que realmente transmite
la información. La condvar es solo el mecanismo de despertar. Si el
productor pone `data_ready=1` antes de que el consumidor llegue al
while, el consumidor ve que ya es true y no necesita dormir.

---

## 11. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                   VARIABLES DE CONDICIÓN                           │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Componentes (los 3 siempre juntos):                                │
│    pthread_mutex_t mtx    — protege el predicado                   │
│    pthread_cond_t  cond   — mecanismo de espera                    │
│    bool/int predicado     — condición real a verificar             │
│                                                                     │
│  Inicialización:                                                    │
│    Estática: PTHREAD_COND_INITIALIZER                              │
│    Dinámica: pthread_cond_init(&cv, &attr)                         │
│    Destruir: pthread_cond_destroy(&cv)                             │
│    Attr: setclock(CLOCK_MONOTONIC), setpshared(PROCESS_SHARED)     │
│                                                                     │
│  Patrón canónico (WAITER):                                          │
│    pthread_mutex_lock(&mtx);                                        │
│    while (!predicado)                    // ← WHILE, nunca if      │
│        pthread_cond_wait(&cv, &mtx);    // unlock+sleep atómico   │
│    actuar();                             // predicado es true      │
│    pthread_mutex_unlock(&mtx);                                      │
│                                                                     │
│  Patrón canónico (SIGNALER):                                        │
│    pthread_mutex_lock(&mtx);                                        │
│    modificar_datos();                                               │
│    predicado = true;                     // cambiar estado         │
│    pthread_cond_signal(&cv);             // o broadcast            │
│    pthread_mutex_unlock(&mtx);                                      │
│                                                                     │
│  signal vs broadcast:                                               │
│    signal    — despierta 1 waiter (eficiente para 1:1)             │
│    broadcast — despierta todos (necesario para shutdown,           │
│                cambios globales, múltiples predicados)              │
│    En duda → broadcast (siempre correcto)                          │
│                                                                     │
│  timedwait:                                                         │
│    pthread_cond_timedwait(&cv, &mtx, &abs_timespec)                │
│    Retorna ETIMEDOUT si expira                                      │
│    Usar CLOCK_MONOTONIC con pthread_condattr_setclock              │
│                                                                     │
│  Errores fatales:                                                   │
│    ✗ if en vez de while → spurious wakeup → crash                  │
│    ✗ cond_wait sin mutex locked → UB                               │
│    ✗ signal sin modificar predicado → waiter vuelve a dormir      │
│    ✗ signal cuando se necesita broadcast → threads colgados       │
│                                                                     │
│  Compilar: gcc -o prog prog.c -pthread                             │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Cola productor-consumidor con shutdown

Extiende la `bounded_queue_t` de la sección 8.1 con soporte para
shutdown limpio:

```c
void bq_shutdown(bounded_queue_t *q);
// Señala a todos los waiters que deben terminar

int bq_pop_or_shutdown(bounded_queue_t *q, int *item);
// Retorna 0 si obtuvo un item, -1 si shutdown
```

Requisitos:
- M productores generan datos y terminan naturalmente
- N consumidores corren indefinidamente hasta shutdown
- El main thread espera a que los productores terminen, luego
  llama `bq_shutdown`
- Los consumidores drenan todos los items restantes antes de terminar
- Verificar que no se pierde ningún item

**Pregunta de reflexión**: ¿Deberías usar `signal` o `broadcast` en
`bq_shutdown`? ¿Y en `bq_push` cuando hay múltiples consumidores?
¿Qué pasa si usas `signal` en shutdown con 4 consumidores dormidos?

### Ejercicio 2: Lectores-Escritores con condvar

Implementa un read-write lock usando solo mutex + condvars (no
`pthread_rwlock_t`):

```c
typedef struct {
    pthread_mutex_t mtx;
    pthread_cond_t readers_cv;
    pthread_cond_t writer_cv;
    int active_readers;
    int active_writers;
    int waiting_writers;
} my_rwlock_t;

void my_rwlock_rdlock(my_rwlock_t *rw);
void my_rwlock_wrlock(my_rwlock_t *rw);
void my_rwlock_unlock(my_rwlock_t *rw);
```

Implementar dos políticas:
1. **Prioridad a lectores**: escritores esperan a que no haya lectores
2. **Prioridad a escritores**: si hay escritores esperando, nuevos
   lectores esperan

Benchmark con 8 lectores y 2 escritores. Medir starvation en cada
política.

**Pregunta de reflexión**: En la política de prioridad a lectores,
¿puede un escritor esperar indefinidamente (starvation)? ¿Y en
prioridad a escritores, pueden los lectores sufrir starvation?
¿Existe una política "justa" que evite starvation para ambos?

### Ejercicio 3: Pipeline de procesamiento

Implementa un pipeline de 4 etapas donde cada etapa es un thread
con su propia cola de entrada. Los datos fluyen: etapa 1 → 2 → 3 → 4:

```c
typedef struct {
    bounded_queue_t input;
    void (*process)(int *item);  // función de transformación
    bounded_queue_t *output;     // cola de la siguiente etapa (NULL si última)
} pipeline_stage_t;

// Etapas:
// 1. Generar números (1..N)
// 2. Multiplicar por 2
// 3. Sumar 1
// 4. Imprimir resultado

// Pipeline: N → x2 → +1 → print
// Resultado esperado para N=5: 3, 5, 7, 9, 11
```

Requisitos:
- Cada etapa procesa items independientemente
- Las colas entre etapas son bounded (backpressure natural)
- Soporte para shutdown limpio (propagar un "poison pill" o shutdown
  flag por todas las etapas)
- Medir throughput total del pipeline

**Pregunta de reflexión**: Si la etapa 2 es más lenta que las demás,
¿qué pasa con el pipeline? ¿Se acumula backpressure? ¿Cómo podrías
hacer que la etapa lenta use múltiples threads en paralelo sin cambiar
el diseño del pipeline?
