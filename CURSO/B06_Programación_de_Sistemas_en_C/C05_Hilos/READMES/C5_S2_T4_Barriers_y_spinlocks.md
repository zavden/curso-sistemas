# Barriers y spinlocks

## Índice
1. [Barriers: concepto](#1-barriers-concepto)
2. [API de pthread_barrier](#2-api-de-pthread_barrier)
3. [Patrones con barriers](#3-patrones-con-barriers)
4. [Spinlocks: concepto](#4-spinlocks-concepto)
5. [API de pthread_spin](#5-api-de-pthread_spin)
6. [Spinlock vs mutex: cuándo usar cada uno](#6-spinlock-vs-mutex-cuándo-usar-cada-uno)
7. [Spinlocks en el kernel](#7-spinlocks-en-el-kernel)
8. [Implementación interna](#8-implementación-interna)
9. [Patrones avanzados](#9-patrones-avanzados)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Barriers: concepto

Una barrera es un punto de sincronización donde **todos los threads
deben llegar antes de que cualquiera pueda continuar**. Es la
herramienta natural cuando tu algoritmo tiene fases que dependen de
que todos los participantes hayan completado la fase anterior:

```
Thread 1:    ──────────┤ BARRIER ├──────────
Thread 2:    ────┤      BARRIER ├──────────
Thread 3:    ──────┤    BARRIER ├──────────
Thread 4:    ───┤       BARRIER ├──────────
                        ↑
              Todos esperan aquí
              hasta que llegue el último
```

### Analogía

Piensa en un grupo de excursionistas que acuerdan esperar en cada
cruce de caminos hasta que todos lleguen. Nadie continúa hasta que
el último excursionista alcanza el punto de encuentro.

### Casos de uso típicos

| Caso | Descripción |
|------|-------------|
| Simulaciones por fases | Física, autómatas celulares: calcular → barrera → siguiente paso |
| Inicialización paralela | Cada thread prepara su parte → barrera → empezar procesamiento |
| Benchmarking | Asegurar que todos los threads arrancan simultáneamente |
| Algoritmos iterativos | Jacobi, Gauss-Seidel: todos computan → barrera → intercambiar bordes |

---

## 2. API de pthread_barrier

### Ciclo de vida

```
pthread_barrier_init()     ← crear con count de participantes
        │
        ▼
pthread_barrier_wait()     ← cada thread llama; bloquea hasta que
        │                     count threads hayan llamado
        ▼
pthread_barrier_destroy()  ← liberar recursos
```

### Inicialización

```c
#include <pthread.h>

pthread_barrier_t barrier;

// Inicialización estática: NO EXISTE
// A diferencia de mutex/cond, las barriers siempre se inicializan dinámicamente

int pthread_barrier_init(pthread_barrier_t *barrier,
                         const pthread_barrierattr_t *attr,
                         unsigned count);
// count: número de threads que deben llamar a wait() antes de liberarse
// count DEBE ser > 0, o se obtiene EINVAL
// attr: NULL para atributos por defecto
// Retorna: 0 en éxito, código de error en fallo
```

> **Importante**: `count` se fija al inicializar la barrera y no puede
> cambiar después. Si necesitas un número dinámico de participantes,
> debes destruir y reinicializar la barrera.

### Espera en la barrera

```c
int pthread_barrier_wait(pthread_barrier_t *barrier);
// Retorna:
//   PTHREAD_BARRIER_SERIAL_THREAD  para EXACTAMENTE un thread
//   0                              para todos los demás threads
//   código de error                en fallo
```

El valor `PTHREAD_BARRIER_SERIAL_THREAD` se entrega a **exactamente
un thread** (no está definido cuál). Esto es muy útil para ejecutar
una acción que solo debe ocurrir una vez por ciclo:

```c
void *worker(void *arg) {
    // ... computar fase ...

    int rc = pthread_barrier_wait(&barrier);
    if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
        // Solo un thread ejecuta esto
        printf("Phase complete, merging results...\n");
        merge_results();
    }
    // rc == 0 para los demás threads
    // Todos continúan desde aquí

    return NULL;
}
```

### Destrucción

```c
int pthread_barrier_destroy(pthread_barrier_t *barrier);
// Solo destruir cuando ningún thread está esperando en la barrera
// Retorna: EBUSY si hay threads esperando
```

### Ejemplo completo: simulación por fases

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define NUM_THREADS  4
#define NUM_PHASES   3

static pthread_barrier_t phase_barrier;
static double data[NUM_THREADS];

void *simulate(void *arg) {
    int id = (int)(intptr_t)arg;

    for (int phase = 0; phase < NUM_PHASES; phase++) {
        // --- Fase de cómputo (cada thread trabaja en su porción) ---
        data[id] = id * 10.0 + phase;
        printf("Thread %d: computed data[%d] = %.1f (phase %d)\n",
               id, id, data[id], phase);

        // --- Barrera: esperar a que todos terminen esta fase ---
        int rc = pthread_barrier_wait(&phase_barrier);

        if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
            // Un solo thread imprime el resumen
            printf("--- Phase %d complete. Data:", phase);
            for (int i = 0; i < NUM_THREADS; i++)
                printf(" %.1f", data[i]);
            printf(" ---\n\n");
        }

        // Segunda barrera para que nadie empiece la siguiente fase
        // antes de que el thread serial termine de imprimir
        pthread_barrier_wait(&phase_barrier);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];

    // count = NUM_THREADS (solo los workers, main no participa)
    pthread_barrier_init(&phase_barrier, NULL, NUM_THREADS);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, simulate, (void *)(intptr_t)i);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    pthread_barrier_destroy(&phase_barrier);
    return 0;
}
```

```bash
gcc -pthread -o sim sim.c && ./sim
```

### Atributos de barrera

```c
pthread_barrierattr_t battr;
pthread_barrierattr_init(&battr);

// Compartir entre procesos (con shared memory)
pthread_barrierattr_setpshared(&battr, PTHREAD_PROCESS_SHARED);

pthread_barrier_init(&barrier, &battr, count);
pthread_barrierattr_destroy(&battr);
```

A diferencia de mutex y rwlock, las barriers **no tienen atributo
ROBUST**. Si un proceso muere mientras espera en una barrera
compartida, los demás procesos quedarán bloqueados indefinidamente.

### La barrera es reutilizable

Una vez que todos los threads pasan la barrera, esta se **reinicia
automáticamente** con el mismo count. No necesitas destruir y
reinicializar:

```c
// Funciona correctamente sin reinicializar
for (int i = 0; i < 100; i++) {
    do_work(i);
    pthread_barrier_wait(&barrier);  // se reinicia en cada ciclo
}
```

---

## 3. Patrones con barriers

### Patrón 1: arranque sincronizado

Asegurar que todos los threads empiezan a trabajar al mismo tiempo
(útil para benchmarking justo):

```c
static pthread_barrier_t start_barrier;

void *benchmark_worker(void *arg) {
    int id = (int)(intptr_t)arg;

    // Preparación (puede tomar tiempo variable)
    setup_thread_resources(id);

    // Todos arrancan juntos
    pthread_barrier_wait(&start_barrier);

    // --- Zona medida ---
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);

    do_actual_work(id);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    // --- Fin zona medida ---

    double elapsed = (t1.tv_sec - t0.tv_sec)
                   + (t1.tv_nsec - t0.tv_nsec) / 1e9;
    printf("Thread %d: %.6f s\n", id, elapsed);
    return NULL;
}
```

### Patrón 2: doble barrera (fases con resumen)

Cuando un thread serial necesita procesar datos entre fases,
se usan dos barreras para evitar que los demás threads
corrompan los datos mientras el serial los lee:

```c
void *phased_worker(void *arg) {
    int id = (int)(intptr_t)arg;

    for (int phase = 0; phase < NUM_PHASES; phase++) {
        // Fase de cómputo
        compute_local(id, phase);

        // Barrera 1: todos terminaron de computar
        int rc = pthread_barrier_wait(&barrier);
        if (rc == PTHREAD_BARRIER_SERIAL_THREAD) {
            // Solo uno: consolidar, reducir, imprimir...
            reduce_global_data();
        }

        // Barrera 2: el serial terminó de consolidar
        pthread_barrier_wait(&barrier);
        // Ahora es seguro leer datos globales y empezar siguiente fase
    }
    return NULL;
}
```

```
Tiempo ──────────────────────────────────────────────►

T1: [compute] ─┤B1├─ [espera] ─┤B2├─ [compute] ─┤B1├─
T2: [compute] ─┤B1├─ [espera] ─┤B2├─ [compute] ─┤B1├─
T3: [compute] ─┤B1├─ [SERIAL] ─┤B2├─ [compute] ─┤B1├─
T4: [compute] ─┤B1├─ [espera] ─┤B2├─ [compute] ─┤B1├─
```

### Patrón 3: pipeline de etapas

Cada etapa de un pipeline espera a que su grupo esté listo:

```c
#define STAGES 3

static pthread_barrier_t stage_barrier[STAGES];

void *pipeline_worker(void *arg) {
    int id = (int)(intptr_t)arg;

    for (int s = 0; s < STAGES; s++) {
        process_stage(id, s);
        pthread_barrier_wait(&stage_barrier[s]);
    }
    return NULL;
}

// Inicializar cada barrera con el mismo count
for (int s = 0; s < STAGES; s++)
    pthread_barrier_init(&stage_barrier[s], NULL, NUM_THREADS);
```

### Patrón 4: barrera con condvar (count dinámico)

`pthread_barrier` exige un count fijo. Si necesitas un número variable
de participantes, implementa tu propia barrera con mutex+condvar:

```c
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    unsigned        count;      // participantes totales
    unsigned        arrived;    // cuántos han llegado
    unsigned        generation; // evita wakeups espurios entre rondas
} dynamic_barrier_t;

void dynamic_barrier_init(dynamic_barrier_t *b, unsigned count) {
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->cond, NULL);
    b->count      = count;
    b->arrived    = 0;
    b->generation = 0;
}

// Cambiar el número de participantes (llamar solo cuando nadie espera)
void dynamic_barrier_set_count(dynamic_barrier_t *b, unsigned new_count) {
    pthread_mutex_lock(&b->mutex);
    b->count = new_count;
    pthread_mutex_unlock(&b->mutex);
}

int dynamic_barrier_wait(dynamic_barrier_t *b) {
    pthread_mutex_lock(&b->mutex);
    unsigned gen = b->generation;
    b->arrived++;

    if (b->arrived == b->count) {
        // Último en llegar: reiniciar y despertar a todos
        b->arrived = 0;
        b->generation++;
        pthread_cond_broadcast(&b->cond);
        pthread_mutex_unlock(&b->mutex);
        return 1;  // "serial thread"
    }

    // Esperar hasta que cambie la generación
    while (gen == b->generation)
        pthread_cond_wait(&b->cond, &b->mutex);

    pthread_mutex_unlock(&b->mutex);
    return 0;
}

void dynamic_barrier_destroy(dynamic_barrier_t *b) {
    pthread_mutex_destroy(&b->mutex);
    pthread_cond_destroy(&b->cond);
}
```

El campo `generation` es clave: sin él, un thread rápido podría
pasar la barrera, llegar a la siguiente ronda, e inmediatamente ver
`arrived < count` y dormirse, pero ser despertado por el broadcast
de la ronda anterior.

---

## 4. Spinlocks: concepto

Un spinlock es un mecanismo de exclusión mutua donde el thread que
intenta adquirir un lock **ya tomado** no se duerme, sino que
**gira en un bucle activo** (busy-wait) comprobando repetidamente
si el lock se ha liberado:

```
Mutex (blocking):                    Spinlock (busy-wait):

Thread A: [lock]──────[unlock]       Thread A: [lock]──────[unlock]

Thread B:      ┌─ SLEEP ─┐              Thread B:  ↻ ↻ ↻ ↻ ↻ ↻
               └─────────┘                     spin spin spin...
          (kernel suspende          (CPU ejecuta un loop vacío
           y despierta)              quemando ciclos)
```

### ¿Por qué querría quemar CPU?

Porque dormir y despertar un thread tiene un costo:

```
Operación                          Costo aproximado (x86-64)
─────────────────────────────────────────────────────────────
Spinlock lock/unlock               ~10-20 ns (sin contención)
Mutex lock/unlock (no contendido)  ~25 ns (futex fast path)
Mutex lock (contendido, context    ~1-10 µs (syscall + scheduler)
  switch completo)
```

Si la sección crítica dura **menos** que el costo de un context
switch (~1-10 µs), el spinlock gana: los threads que esperan
pierden menos tiempo girando que durmiendo y despertando.

Si la sección crítica dura **más** que el costo de un context
switch, el spinlock pierde: estás quemando CPU que otros threads
podrían usar para trabajo productivo.

---

## 5. API de pthread_spin

### Ciclo de vida

```c
#include <pthread.h>

pthread_spinlock_t slock;

int pthread_spin_init(pthread_spinlock_t *lock, int pshared);
// pshared: PTHREAD_PROCESS_PRIVATE o PTHREAD_PROCESS_SHARED

int pthread_spin_lock(pthread_spinlock_t *lock);
// Bloquea girando hasta adquirir el lock

int pthread_spin_trylock(pthread_spinlock_t *lock);
// Intenta adquirir sin girar
// Retorna: 0 si tuvo éxito, EBUSY si está tomado

int pthread_spin_unlock(pthread_spinlock_t *lock);

int pthread_spin_destroy(pthread_spinlock_t *lock);
```

> **Nota**: No existe `pthread_spin_timedlock`. Si necesitas
> timeout, debes implementarlo manualmente con `trylock` + reloj.

### Ejemplo básico

```c
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>

#define NUM_THREADS 4
#define ITERATIONS  1000000

static pthread_spinlock_t counter_lock;
static long counter = 0;

void *increment(void *arg) {
    (void)arg;
    for (int i = 0; i < ITERATIONS; i++) {
        pthread_spin_lock(&counter_lock);
        counter++;  // sección crítica mínima
        pthread_spin_unlock(&counter_lock);
    }
    return NULL;
}

int main(void) {
    pthread_t threads[NUM_THREADS];

    pthread_spin_init(&counter_lock, PTHREAD_PROCESS_PRIVATE);

    for (int i = 0; i < NUM_THREADS; i++)
        pthread_create(&threads[i], NULL, increment, NULL);
    for (int i = 0; i < NUM_THREADS; i++)
        pthread_join(threads[i], NULL);

    printf("Counter: %ld (expected %ld)\n",
           counter, (long)NUM_THREADS * ITERATIONS);

    pthread_spin_destroy(&counter_lock);
    return 0;
}
```

### No existe inicialización estática

A diferencia de `pthread_mutex_t` con `PTHREAD_MUTEX_INITIALIZER`,
los spinlocks **no tienen macro de inicialización estática**.
Siempre debes usar `pthread_spin_init`.

### No hay tipos ni atributos

Los spinlocks POSIX no tienen:
- Tipos (ERRORCHECK, RECURSIVE, ROBUST)
- Atributos (más allá de pshared)
- Detección de deadlock

Si haces `pthread_spin_lock` dos veces desde el mismo thread,
el resultado es **comportamiento indefinido** (en la práctica,
deadlock inmediato porque girará para siempre).

---

## 6. Spinlock vs mutex: cuándo usar cada uno

### Tabla de decisión

```
                           ┌────────────────────┐
                           │ ¿Sección crítica    │
                           │  < 1 µs?            │
                           └──────┬─────────────┘
                              Sí  │    No
                              ▼   │    ▼
                    ┌─────────┐   │  ┌──────────────┐
                    │¿Solo    │   │  │ Usa MUTEX     │
                    │1 CPU?   │   │  │ (el thread    │
                    └────┬────┘   │  │  duerme, CPU  │
                     Sí  │  No   │  │  libre)       │
                     ▼   │  ▼    │  └──────────────┘
              ┌──────┐  ┌──────┐ │
              │MUTEX │  │¿Se   │ │
              │(spin │  │llama │ │
              │ no   │  │algo  │ │
              │tiene │  │que   │ │
              │senti-│  │pueda │ │
              │do en │  │block?│ │
              │1 CPU)│  └──┬───┘ │
              └──────┘  Sí │ No  │
                        ▼  │ ▼   │
                 ┌──────┐ ┌──────┐
                 │MUTEX │ │SPIN  │
                 │(si   │ │LOCK  │
                 │duerme│ │      │
                 │nunca │ │      │
                 │te    │ │      │
                 │ceden │ │      │
                 │el    │ │      │
                 │CPU)  │ │      │
                 └──────┘ └──────┘
```

### Reglas prácticas

| Criterio | Spinlock | Mutex |
|----------|----------|-------|
| Sección crítica | < 1 µs (unas pocas instrucciones) | > 1 µs o impredecible |
| CPUs disponibles | Múltiples cores | Un core o desconocido |
| Dentro de la sección | Solo operaciones en memoria | Puede llamar a I/O, malloc, etc. |
| Prioridades | Threads de igual prioridad | Mezcla de prioridades (priority inheritance) |
| Contención esperada | Baja y breve | Cualquiera |
| Sleep/yield dentro | **NUNCA** | Permitido con condvar |

### ¿Por qué no usar spinlock en un solo core?

Con un solo core, si Thread A tiene el spinlock y Thread B intenta
adquirirlo, B gira consumiendo su quantum. Pero A no puede ejecutar
para liberar el lock hasta que B agote su quantum (o sea desalojado).
Resultado: todo el quantum de B se desperdicia girando.

```
1 CPU, 2 threads:

Quantum 1: Thread A [lock] ────────── (preempted)
Quantum 2: Thread B ↻↻↻↻↻↻↻↻↻↻↻↻↻↻  (spin completo desperdiciado)
Quantum 3: Thread A ── [unlock]
Quantum 4: Thread B [lock] ──── [unlock]
```

Con mutex, Thread B dormiría inmediatamente y A continuaría ejecutando.

### Benchmark: spinlock vs mutex

```c
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#define ITERS 10000000

static pthread_mutex_t    mtx = PTHREAD_MUTEX_INITIALIZER;
static pthread_spinlock_t spin;
static volatile int shared = 0;

double bench_mutex(void) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) {
        pthread_mutex_lock(&mtx);
        shared++;
        pthread_mutex_unlock(&mtx);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
}

double bench_spin(void) {
    struct timespec t0, t1;
    clock_gettime(CLOCK_MONOTONIC, &t0);
    for (int i = 0; i < ITERS; i++) {
        pthread_spin_lock(&spin);
        shared++;
        pthread_spin_unlock(&spin);
    }
    clock_gettime(CLOCK_MONOTONIC, &t1);
    return (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
}

int main(void) {
    pthread_spin_init(&spin, PTHREAD_PROCESS_PRIVATE);

    printf("Mutex:    %.4f s (%d M ops)\n", bench_mutex(), ITERS/1000000);
    printf("Spinlock: %.4f s (%d M ops)\n", bench_spin(),  ITERS/1000000);

    pthread_spin_destroy(&spin);
    return 0;
}
```

Resultado típico en x86-64 **sin contención** (1 thread):
```
Mutex:    0.2800 s (10 M ops)     ~28 ns/op
Spinlock: 0.1200 s (10 M ops)     ~12 ns/op
```

Con contención alta (4+ threads), la ventaja del spinlock se reduce
o invierte porque el busy-wait desperdicia ciclos de CPU.

---

## 7. Spinlocks en el kernel

En userspace los spinlocks tienen uso limitado, pero en el **kernel
de Linux** son el mecanismo de locking principal. Esto se debe a que:

1. **No puedes dormir en contexto de interrupción**: los interrupt
   handlers no pueden llamar a `schedule()`, así que un mutex
   (que duerme) no es opción.

2. **Secciones críticas mínimas**: el kernel protege estructuras
   con operaciones de punteros que duran nanosegundos.

3. **SMP obligatorio**: el kernel moderno asume múltiples CPUs.

### Variantes del kernel

```
Variante                  Uso
──────────────────────────────────────────────────────
spin_lock()               Contexto de proceso
spin_lock_irqsave()       Desactiva IRQs + spin
spin_lock_bh()            Desactiva bottom halves + spin
raw_spin_lock()           No se convierte en mutex con PREEMPT_RT
```

En `PREEMPT_RT` (parches de tiempo real para Linux), los spinlocks
regulares del kernel se convierten **automáticamente en mutexes**
que pueden dormir, para reducir la latencia. Solo `raw_spin_lock`
mantiene el comportamiento de busy-wait real.

### Ticket spinlock y qspinlock

El kernel Linux no usa un spinlock naive de test-and-set. Ha
evolucionado por rendimiento y fairness:

```
Evolución del spinlock en Linux:

1. Test-and-set (original)
   └─ Problema: no es fair (starvation)

2. Ticket spinlock (2008, Linux 2.6.25)
   └─ Cada thread toma un "ticket" numerado
   └─ Se sirve en orden FIFO
   └─ Problema: todos los cores hacen cache bounce
      en la misma variable "now_serving"

3. qspinlock (2015, Linux 4.2)
   └─ MCS-based: cada thread gira en su propia
      variable local en caché
   └─ Mínimo cache bouncing
   └─ Compacto: cabe en 4 bytes
```

```
Ticket spinlock:

  ┌─────────────────────────┐
  │  next_ticket = 5        │ ← atómico, cada lock() toma uno
  │  now_serving = 3        │ ← todos los cores leen y giran aquí
  └─────────────────────────┘
  Core 0: mi ticket=3 → ¡ES MÍO!
  Core 1: mi ticket=4 → spin (now_serving==3, no es mi turno)
  Core 2: mi ticket=5 → spin

qspinlock (MCS):

  ┌──────┐     ┌──────┐     ┌──────┐
  │Core 0│────▶│Core 1│────▶│Core 2│
  │locked│     │ spin │     │ spin │
  │      │     │ aquí │     │ aquí │
  └──────┘     └──────┘     └──────┘
  Cada nodo gira en su propia variable → sin cache bouncing
```

Este detalle es relevante para entender reportes de rendimiento
en sistemas con muchos cores (NUMA).

---

## 8. Implementación interna

### Spinlock naive: test-and-set

La implementación más simple usa una operación atómica
`test_and_set` (en x86: `xchg` o `lock cmpxchg`):

```c
// Implementación conceptual (NO usar en producción)
#include <stdatomic.h>

typedef struct {
    atomic_int locked;  // 0 = libre, 1 = tomado
} naive_spinlock_t;

void naive_spin_lock(naive_spinlock_t *s) {
    while (atomic_exchange(&s->locked, 1) != 0) {
        // Girar hasta que logremos cambiar 0→1
    }
}

void naive_spin_unlock(naive_spinlock_t *s) {
    atomic_store(&s->locked, 0);
}
```

**Problema**: cada iteración del spin ejecuta un `xchg`, que invalida
la cache line en todos los otros cores (incluso si el lock sigue
tomado). Esto genera un **bombardeo de invalidaciones de caché**:

```
Core 0 (tiene el lock): escribiendo datos en sección crítica
Core 1: xchg → invalidate cache line → reload → aún locked → repeat
Core 2: xchg → invalidate cache line → reload → aún locked → repeat
Core 3: xchg → invalidate cache line → reload → aún locked → repeat
         ↑
         Cada xchg genera tráfico en el bus de coherencia
```

### Mejora: test-and-test-and-set (TTAS)

Primero leer (sin operación atómica costosa), y solo intentar
adquirir cuando se observa que el lock está libre:

```c
void ttas_spin_lock(naive_spinlock_t *s) {
    for (;;) {
        // Test: lectura simple (puede servirse del caché)
        while (atomic_load_explicit(&s->locked, memory_order_relaxed))
            ;  // Spin leyendo, sin invalidar cachés ajenas

        // Test-and-set: intento atómico
        if (atomic_exchange(&s->locked, 1) == 0)
            return;  // Adquirido
        // Otro thread ganó, volver a girar
    }
}
```

Esto reduce drásticamente el tráfico de bus porque las lecturas
del spin se sirven del caché local.

### x86: instrucción PAUSE

En x86/x86-64, Intel recomienda insertar la instrucción `PAUSE`
en el loop de spin. `PAUSE` le dice al procesador "estoy en un
spin-wait loop" y le permite:

1. Reducir el consumo de energía durante el spin
2. Evitar penalizaciones de pipeline por memory order violation
3. Ceder recursos de hyperthreading al otro thread lógico del core

```c
void improved_spin_lock(naive_spinlock_t *s) {
    for (;;) {
        while (atomic_load_explicit(&s->locked, memory_order_relaxed)) {
            #if defined(__x86_64__) || defined(__i386__)
            __builtin_ia32_pause();  // o asm volatile("pause")
            #elif defined(__aarch64__)
            asm volatile("yield");   // equivalente en ARM
            #endif
        }
        if (atomic_exchange(&s->locked, 1) == 0)
            return;
    }
}
```

### Barrier (pthread_barrier) internamente

La implementación de glibc usa un `futex` con un protocolo
en varias fases:

```
Estado interno de pthread_barrier:

  ┌─────────────────────────────────────┐
  │  count:    N (participantes)        │
  │  arrived:  actual (atómico)         │
  │  event:    generación (par/impar)   │
  │  futex:    word para sleep/wake     │
  └─────────────────────────────────────┘

pthread_barrier_wait():
1. arrived++ (atómico)
2. if arrived < count:
     futex_wait(&event, old_generation)   ← dormir
3. if arrived == count:
     arrived = 0
     event ^= 1     ← cambiar generación
     futex_wake(&event, INT_MAX)          ← despertar a todos
     return PTHREAD_BARRIER_SERIAL_THREAD
4. Al despertar: return 0
```

A diferencia del spinlock, la barrera **sí duerme** a los threads
que esperan, porque la espera puede ser larga (depende del thread
más lento).

---

## 9. Patrones avanzados

### Patrón 1: adaptive spinlock (spin then sleep)

Combina lo mejor de ambos mundos: girar brevemente y, si no se
adquiere el lock, dormir con un mutex:

```c
#include <pthread.h>
#include <stdatomic.h>
#include <sched.h>

typedef struct {
    atomic_int      locked;
    pthread_mutex_t fallback;
} adaptive_lock_t;

#define SPIN_LIMIT 100

void adaptive_lock_init(adaptive_lock_t *a) {
    atomic_store(&a->locked, 0);
    pthread_mutex_init(&a->fallback, NULL);
}

void adaptive_lock(adaptive_lock_t *a) {
    // Fase 1: spin breve
    for (int i = 0; i < SPIN_LIMIT; i++) {
        if (atomic_exchange(&a->locked, 1) == 0)
            return;  // adquirido durante spin
        #if defined(__x86_64__)
        __builtin_ia32_pause();
        #endif
    }

    // Fase 2: fallback a mutex (duerme)
    pthread_mutex_lock(&a->fallback);
    while (atomic_exchange(&a->locked, 1) != 0) {
        // Alguien más lo tiene; el mutex serializa los waiters
        pthread_mutex_unlock(&a->fallback);
        sched_yield();
        pthread_mutex_lock(&a->fallback);
    }
    pthread_mutex_unlock(&a->fallback);
}

void adaptive_unlock(adaptive_lock_t *a) {
    atomic_store(&a->locked, 0);
}

void adaptive_lock_destroy(adaptive_lock_t *a) {
    pthread_mutex_destroy(&a->fallback);
}
```

> **Nota**: el `futex` de Linux implementa exactamente esta idea
> dentro de `pthread_mutex_lock`: primero intenta un CAS en
> userspace (fast path) y solo cae al kernel si hay contención.

### Patrón 2: seqcount con spinlock

Para proteger datos que se leen con mucha más frecuencia que se
escriben, y donde la lectura debe ser consistente pero sin bloquear:

```c
#include <stdatomic.h>
#include <pthread.h>

typedef struct {
    atomic_uint         seq;     // contador de secuencia
    pthread_spinlock_t  wlock;   // protege escritores entre sí
} seqcount_t;

typedef struct {
    double x, y, z;  // coordenadas 3D (no atómico)
} point_t;

static seqcount_t  sc;
static point_t     shared_point;

void seqcount_init(seqcount_t *s) {
    atomic_store(&s->seq, 0);
    pthread_spin_init(&s->wlock, PTHREAD_PROCESS_PRIVATE);
}

// Escritor (exclusión mutua entre escritores)
void update_point(double x, double y, double z) {
    pthread_spin_lock(&sc.wlock);

    atomic_fetch_add(&sc.seq, 1);           // seq impar = escritura en curso
    atomic_thread_fence(memory_order_release);

    shared_point.x = x;
    shared_point.y = y;
    shared_point.z = z;

    atomic_thread_fence(memory_order_release);
    atomic_fetch_add(&sc.seq, 1);           // seq par = escritura completa

    pthread_spin_unlock(&sc.wlock);
}

// Lector (sin locks, retry si hay escritura concurrente)
point_t read_point(void) {
    point_t p;
    unsigned seq0, seq1;

    do {
        seq0 = atomic_load_explicit(&sc.seq, memory_order_acquire);
        if (seq0 & 1) continue;  // escritura en curso, reintentar

        p = shared_point;  // lectura (puede ser torn)

        atomic_thread_fence(memory_order_acquire);
        seq1 = atomic_load_explicit(&sc.seq, memory_order_relaxed);
    } while (seq0 != seq1);  // si cambió, hubo escritura → reintentar

    return p;  // lectura consistente garantizada
}
```

### Patrón 3: barrier con timeout manual

`pthread_barrier` no tiene `timedwait`. Podemos construirlo con
condvar:

```c
#include <pthread.h>
#include <errno.h>
#include <time.h>

typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
    unsigned        count;
    unsigned        arrived;
    unsigned        generation;
} timed_barrier_t;

void timed_barrier_init(timed_barrier_t *b, unsigned count) {
    pthread_mutex_init(&b->mutex, NULL);

    pthread_condattr_t attr;
    pthread_condattr_init(&attr);
    pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
    pthread_cond_init(&b->cond, &attr);
    pthread_condattr_destroy(&attr);

    b->count      = count;
    b->arrived    = 0;
    b->generation = 0;
}

// Retorna: 1 = serial, 0 = normal, -1 = timeout
int timed_barrier_wait(timed_barrier_t *b, unsigned timeout_ms) {
    struct timespec deadline;
    clock_gettime(CLOCK_MONOTONIC, &deadline);
    deadline.tv_sec  += timeout_ms / 1000;
    deadline.tv_nsec += (timeout_ms % 1000) * 1000000;
    if (deadline.tv_nsec >= 1000000000) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000;
    }

    pthread_mutex_lock(&b->mutex);
    unsigned gen = b->generation;
    b->arrived++;

    if (b->arrived == b->count) {
        b->arrived = 0;
        b->generation++;
        pthread_cond_broadcast(&b->cond);
        pthread_mutex_unlock(&b->mutex);
        return 1;  // serial thread
    }

    while (gen == b->generation) {
        int rc = pthread_cond_timedwait(&b->cond, &b->mutex, &deadline);
        if (rc == ETIMEDOUT) {
            // Decidir: ¿decrementar arrived? Depende de tu protocolo
            b->arrived--;
            pthread_mutex_unlock(&b->mutex);
            return -1;  // timeout
        }
    }

    pthread_mutex_unlock(&b->mutex);
    return 0;
}
```

> **Cuidado**: si un participante sale por timeout, la barrera ya
> no puede alcanzar su `count`. Debes diseñar un protocolo de
> recuperación (reducir count, cancelar la fase, etc.).

### Patrón 4: trylock con backoff exponencial

Para reducir contención en un spinlock bajo carga:

```c
#include <pthread.h>
#include <time.h>
#include <stdlib.h>

#define MAX_BACKOFF_NS 10000  // 10 µs

void spin_lock_with_backoff(pthread_spinlock_t *lock) {
    unsigned backoff = 1;

    while (pthread_spin_trylock(lock) != 0) {
        // Espera proporcional al backoff
        struct timespec ts = { 0, backoff + (rand() % backoff) };
        nanosleep(&ts, NULL);

        backoff *= 2;
        if (backoff > MAX_BACKOFF_NS)
            backoff = MAX_BACKOFF_NS;
    }
}
```

> **Paradoja**: si añades backoff con `nanosleep`, el spinlock
> empieza a dormir... que es justo lo que hace un mutex. Esto
> demuestra por qué los spinlocks puros rara vez son óptimos en
> userspace.

---

## 10. Errores comunes

### Error 1: count incorrecto en la barrera

```c
// MAL: main no participa pero se contó
pthread_barrier_init(&barrier, NULL, NUM_THREADS + 1);

for (int i = 0; i < NUM_THREADS; i++)
    pthread_create(&threads[i], NULL, worker, NULL);

// main nunca llama a pthread_barrier_wait() → deadlock permanente
for (int i = 0; i < NUM_THREADS; i++)
    pthread_join(threads[i], NULL);
```

**Solución**: `count` debe ser exactamente el número de threads
que llamarán a `pthread_barrier_wait`. Si main no participa, no
lo cuentes. Si main sí participa, inclúyelo Y haz que llame a wait:

```c
// BIEN: main participa
pthread_barrier_init(&barrier, NULL, NUM_THREADS + 1);
// ... crear threads ...
pthread_barrier_wait(&barrier);  // main también espera
// ... join ...
```

### Error 2: usar spinlock con sección crítica larga o con I/O

```c
// MAL: I/O dentro de spinlock
pthread_spin_lock(&slock);
FILE *f = fopen("data.txt", "r");
char buf[4096];
fread(buf, 1, sizeof(buf), f);  // puede bloquear en I/O de disco
fclose(f);
pthread_spin_unlock(&slock);
// Otros threads giran quemando CPU mientras esperan la lectura
```

**Solución**: usa mutex cuando la sección crítica puede bloquear:

```c
// BIEN: mutex para operaciones que pueden bloquear
pthread_mutex_lock(&mtx);
FILE *f = fopen("data.txt", "r");
// ...
pthread_mutex_unlock(&mtx);
```

### Error 3: doble lock de spinlock (auto-deadlock)

```c
void process_item(item_t *item) {
    pthread_spin_lock(&slock);
    if (item->needs_validation) {
        validate(item);  // esta función también toma slock → DEADLOCK
    }
    pthread_spin_unlock(&slock);
}

void validate(item_t *item) {
    pthread_spin_lock(&slock);   // ¡DEADLOCK! ya lo tenemos
    // ...
    pthread_spin_unlock(&slock);
}
```

A diferencia de mutex con `PTHREAD_MUTEX_RECURSIVE`, los spinlocks
POSIX **no soportan reentrada**. El thread gira para siempre
esperando a que él mismo libere el lock.

**Solución**: restructurar el código para no tomar el lock dos veces,
o usar `pthread_mutex_t` con tipo `RECURSIVE`:

```c
// Opción A: factorizar la lógica
void process_item(item_t *item) {
    pthread_spin_lock(&slock);
    if (item->needs_validation)
        validate_unlocked(item);  // versión interna sin lock
    pthread_spin_unlock(&slock);
}

// Opción B: usar mutex recursivo si reentrada es inevitable
```

### Error 4: destruir barrera con threads esperando

```c
// MAL: posible si un thread termina antes que otros pasen la barrera
pthread_barrier_destroy(&barrier);  // EBUSY o comportamiento indefinido
```

**Solución**: asegurar que todos los threads hayan pasado la barrera
(y no estén por volver a usarla) antes de destruir:

```c
// BIEN: join primero, destroy después
for (int i = 0; i < NUM_THREADS; i++)
    pthread_join(threads[i], NULL);
pthread_barrier_destroy(&barrier);
```

### Error 5: spinlock en sistema con un solo core o con preemption

```c
// MAL: spinlock en contenedor Docker con 1 vCPU
// Thread A tiene el lock, es preemptado
// Thread B gira todo su quantum esperando
// → rendimiento peor que con mutex
```

**Solución**: verificar el número de CPUs disponibles:

```c
#include <unistd.h>

long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
if (ncpus <= 1) {
    // Usar mutex, no spinlock
}
```

O simplemente usar mutex por defecto (su fast path es casi tan
rápido como un spinlock sin contención).

---

## 11. Cheatsheet

### Barriers

```
┌──────────────────────────────────────────────────────────┐
│                    pthread_barrier                        │
├──────────────────────────────────────────────────────────┤
│ #include <pthread.h>         // compilar con -pthread    │
│                                                          │
│ pthread_barrier_t bar;                                   │
│                                                          │
│ INIT (solo dinámica):                                    │
│   pthread_barrier_init(&bar, NULL, count);               │
│   pthread_barrier_init(&bar, &attr, count);  // pshared  │
│                                                          │
│ WAIT:                                                    │
│   int rc = pthread_barrier_wait(&bar);                   │
│   if (rc == PTHREAD_BARRIER_SERIAL_THREAD)               │
│       /* solo uno ejecuta esto */                        │
│                                                          │
│ DESTROY:                                                 │
│   pthread_barrier_destroy(&bar);                         │
│                                                          │
│ Propiedades:                                             │
│   • count fijo desde init                                │
│   • reutilizable (se reinicia automáticamente)           │
│   • un thread recibe SERIAL_THREAD, el resto 0           │
│   • puede ser PROCESS_SHARED (sin ROBUST)                │
│   • NO tiene timedwait ni init estática                  │
└──────────────────────────────────────────────────────────┘
```

### Spinlocks

```
┌──────────────────────────────────────────────────────────┐
│                   pthread_spinlock                        │
├──────────────────────────────────────────────────────────┤
│ #include <pthread.h>         // compilar con -pthread    │
│                                                          │
│ pthread_spinlock_t slock;                                │
│                                                          │
│ INIT (solo dinámica):                                    │
│   pthread_spin_init(&slock, PTHREAD_PROCESS_PRIVATE);    │
│   pthread_spin_init(&slock, PTHREAD_PROCESS_SHARED);     │
│                                                          │
│ LOCK:                                                    │
│   pthread_spin_lock(&slock);     // busy-wait            │
│   pthread_spin_trylock(&slock);  // EBUSY si ocupado     │
│                                                          │
│ UNLOCK:                                                  │
│   pthread_spin_unlock(&slock);                           │
│                                                          │
│ DESTROY:                                                 │
│   pthread_spin_destroy(&slock);                          │
│                                                          │
│ Propiedades:                                             │
│   • NO timedlock, NO recursivo, NO ROBUST                │
│   • NO init estática                                     │
│   • Solo para secciones < 1 µs en máquinas multi-core    │
│   • NUNCA bloquear (I/O, malloc, sleep) dentro           │
│   • double lock = deadlock (comportamiento indefinido)   │
└──────────────────────────────────────────────────────────┘
```

### Comparación rápida

```
┌─────────────┬───────────────┬────────────────┬──────────────┐
│             │   Spinlock    │     Mutex      │   Barrier    │
├─────────────┼───────────────┼────────────────┼──────────────┤
│ Propósito   │ Excl. mutua   │ Excl. mutua    │ Sincronizar  │
│             │ (busy-wait)   │ (sleep-wait)   │ N threads    │
│ Espera      │ Gira (CPU)    │ Duerme         │ Duerme       │
│ Costo       │ ~12 ns        │ ~25 ns         │ ~µs          │
│ Contención  │ Quema CPU     │ Eficiente      │ N/A          │
│ I/O dentro  │ NUNCA         │ OK             │ N/A          │
│ Recursivo   │ NO            │ Sí (tipo)      │ N/A          │
│ ROBUST      │ NO            │ Sí             │ NO           │
│ Init static │ NO            │ Sí             │ NO           │
│ timedlock   │ NO            │ Sí             │ NO           │
└─────────────┴───────────────┴────────────────┴──────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Game of Life con barriers

Implementa un **autómata celular** (Game of Life de Conway) paralelizado
con barriers:

1. Crear una grilla de 20x20 con un patrón inicial (por ejemplo, un
   "glider").
2. Dividir las filas entre `N` threads (cada thread procesa un rango
   de filas).
3. Usar **doble buffer** (grilla de lectura + grilla de escritura)
   para evitar interferencia.
4. Después de cada generación, usar una barrera para sincronizar.
   El thread serial intercambia los buffers e imprime la grilla.
5. Ejecutar 20 generaciones.

Reglas de Conway:
- Célula viva con 2-3 vecinos → vive
- Célula muerta con exactamente 3 vecinos → nace
- Cualquier otro caso → muere

**Pregunta de reflexión**: ¿Por qué necesitas doble buffer además de
la barrera? ¿Qué pasaría si todos los threads leyeran y escribieran
en la misma grilla, incluso con la barrera sincronizándolos entre
generaciones?

---

### Ejercicio 2: benchmark mutex vs spinlock vs atomic

Escribe un programa que compare tres mecanismos de protección para
un contador compartido:

1. `pthread_mutex_t` (tipo DEFAULT)
2. `pthread_spinlock_t`
3. `atomic_fetch_add` (sin lock)

Requisitos:
- Parametrizar: número de threads (1, 2, 4, 8) y número de
  incrementos por thread (10M).
- Medir el tiempo de cada combinación con `CLOCK_MONOTONIC`.
- Imprimir una tabla con los resultados.
- Verificar que el contador final sea correcto en los tres casos.

Ejecutar en tu máquina y analizar:
- ¿En qué punto el spinlock empieza a perder frente al mutex?
- ¿El atómico siempre gana?

**Pregunta de reflexión**: `atomic_fetch_add` no usa locks. ¿Significa
que siempre será más rápido? ¿Qué pasa cuando 8 threads hacen
`atomic_fetch_add` en la misma variable simultáneamente a nivel de
cache coherence? ¿Podría un diseño con contadores per-thread
(distribuidos) ser aún más rápido?

---

### Ejercicio 3: thread pool con barrera de parada

Implementa un thread pool simple que use una barrera para la
terminación coordinada:

1. Crear `N` threads worker que procesan tareas de una cola compartida
   (protegida con mutex + condvar).
2. Cada worker toma tareas y las ejecuta (simular con `usleep`).
3. Cuando se invoca `pool_shutdown()`:
   - Señalar a los workers que deben terminar (flag + broadcast).
   - Usar una **barrera** para esperar a que todos los workers
     hayan completado su tarea actual antes de destruir recursos.
4. Imprimir estadísticas: tareas completadas por cada worker.

Estructura sugerida:
```c
typedef struct {
    pthread_t       *threads;
    int              nthreads;
    task_queue_t     queue;
    pthread_barrier_t shutdown_barrier;
    atomic_bool      shutting_down;
} thread_pool_t;
```

**Pregunta de reflexión**: ¿Por qué usar una barrera para shutdown
en vez de simplemente hacer `pthread_join` a cada thread? ¿Qué
ventaja tiene saber que todos los workers terminaron su tarea actual
*al mismo tiempo* versus terminarlos uno por uno con join?
