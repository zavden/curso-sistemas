# T04 - ThreadSanitizer (TSan): data races, deadlocks, en C con pthreads y en Rust unsafe

## Índice

1. [Qué es ThreadSanitizer](#1-qué-es-threadsanitizer)
2. [Historia y contexto](#2-historia-y-contexto)
3. [El problema de la concurrencia](#3-el-problema-de-la-concurrencia)
4. [Data race: definición formal](#4-data-race-definición-formal)
5. [Data race vs race condition](#5-data-race-vs-race-condition)
6. [Arquitectura de TSan](#6-arquitectura-de-tsan)
7. [Shadow memory en TSan: vector clocks](#7-shadow-memory-en-tsan-vector-clocks)
8. [El algoritmo happens-before](#8-el-algoritmo-happens-before)
9. [Compilar con TSan en C](#9-compilar-con-tsan-en-c)
10. [Compilar con TSan en Rust](#10-compilar-con-tsan-en-rust)
11. [Bug 1: data race simple — dos hilos, una variable](#11-bug-1-data-race-simple--dos-hilos-una-variable)
12. [Bug 2: data race en struct compartido](#12-bug-2-data-race-en-struct-compartido)
13. [Bug 3: data race con array compartido](#13-bug-3-data-race-con-array-compartido)
14. [Bug 4: data race por falta de lock](#14-bug-4-data-race-por-falta-de-lock)
15. [Bug 5: data race por lock incorrecto](#15-bug-5-data-race-por-lock-incorrecto)
16. [Bug 6: data race con variable de condición](#16-bug-6-data-race-con-variable-de-condición)
17. [Bug 7: data race por flag de terminación](#17-bug-7-data-race-por-flag-de-terminación)
18. [Bug 8: deadlock (lock ordering)](#18-bug-8-deadlock-lock-ordering)
19. [Bug 9: data race en contador atómico mal usado](#19-bug-9-data-race-en-contador-atómico-mal-usado)
20. [Bug 10: data race en patrón productor-consumidor](#20-bug-10-data-race-en-patrón-productor-consumidor)
21. [Anatomía completa de un reporte TSan](#21-anatomía-completa-de-un-reporte-tsan)
22. [Leer el reporte: thread IDs y stack traces](#22-leer-el-reporte-thread-ids-y-stack-traces)
23. [TSan en Rust: data races con unsafe](#23-tsan-en-rust-data-races-con-unsafe)
24. [Data races imposibles en safe Rust](#24-data-races-imposibles-en-safe-rust)
25. [Ejemplo completo en C: thread_pool](#25-ejemplo-completo-en-c-thread_pool)
26. [Ejemplo completo en Rust unsafe: shared_counter](#26-ejemplo-completo-en-rust-unsafe-shared_counter)
27. [TSan con tests unitarios](#27-tsan-con-tests-unitarios)
28. [TSan con fuzzing](#28-tsan-con-fuzzing)
29. [TSAN_OPTIONS: configuración completa](#29-tsan_options-configuración-completa)
30. [Impacto en rendimiento detallado](#30-impacto-en-rendimiento-detallado)
31. [Falsos positivos en TSan](#31-falsos-positivos-en-tsan)
32. [Suppressions en TSan](#32-suppressions-en-tsan)
33. [TSan vs Helgrind (Valgrind)](#33-tsan-vs-helgrind-valgrind)
34. [Limitaciones de TSan](#34-limitaciones-de-tsan)
35. [Errores comunes](#35-errores-comunes)
36. [Programa de práctica: concurrent_map](#36-programa-de-práctica-concurrent_map)
37. [Ejercicios](#37-ejercicios)

---

## 1. Qué es ThreadSanitizer

ThreadSanitizer (TSan) detecta **data races** en programas multihilo. Un data race ocurre cuando dos hilos acceden a la misma ubicación de memoria simultáneamente, al menos uno de los accesos es de escritura, y no hay sincronización entre ellos.

```
┌──────────────────────────────────────────────────────────────────┐
│                   ThreadSanitizer (TSan)                          │
│                                                                  │
│  Detecta: data races y deadlocks                                 │
│                                                                  │
│  ┌─────────────────────────────────────────────────────────┐     │
│  │  Data races       │  Lock-order inversions (deadlocks)   │     │
│  │  Lock misuse      │  Thread leaks                        │     │
│  │  Signal handler   │  Atomics ordering violations         │     │
│  │  races            │                                      │     │
│  └─────────────────────────────────────────────────────────┘     │
│                                                                  │
│  NO detecta:                                                     │
│  - Buffer overflows (eso es ASan)                                │
│  - Memory leaks (eso es ASan/LSan)                               │
│  - Uninitialized memory (eso es MSan)                            │
│  - Undefined behavior aritmético (eso es UBSan)                  │
│  - Race conditions lógicas (sin data race subyacente)            │
│  - ABA problems en lock-free code                                │
│                                                                  │
│  Características:                                                │
│  • Overhead alto: ~5-15x CPU, ~5-10x RAM                        │
│  • Basado en vector clocks y shadow memory                       │
│  • Funciona en Clang Y GCC                                       │
│  • INCOMPATIBLE con ASan y MSan                                  │
│  • Compatible solo con UBSan                                     │
│  • Muy pocos falsos positivos (casi cero)                        │
└──────────────────────────────────────────────────────────────────┘
```

### ¿Por qué TSan es tan importante?

Los data races son los bugs más difíciles de encontrar en programación:

1. **No deterministas**: ocurren dependiendo del scheduling del OS
2. **Irreproducibles**: puede funcionar 1000 veces y fallar la 1001
3. **Silenciosos**: pueden corromper datos sin crashear
4. **Dependientes de hardware**: un data race puede manifestarse solo en una CPU específica (ARM vs x86)
5. **Dependientes de carga**: aparecen solo bajo carga alta

TSan los encuentra **determinísticamente**: si hay un data race **posible**, TSan lo detecta en una sola ejecución.

---

## 2. Historia y contexto

```
Línea temporal de TSan:

2009    TSan v1 (Valgrind-based, Google)
        Basado en el framework de Valgrind
        Extremadamente lento (~100-300x overhead)

2012    TSan v2 (compilador-based, Dmitry Vyukov, Google)
        Reescritura completa: instrumentación en compilación
        Overhead reducido a ~5-15x (10-20x más rápido que v1)
        Basado en vector clocks comprimidos
        Integrado en Clang/LLVM

2013    GCC 4.8 añade soporte para TSan
        Misma API de runtime, diferente instrumentación

2014    TSan adoptado en Google para Chrome, Go runtime
        Descubre miles de data races en producción

2015    Go integra TSan directamente: go build -race
        (Go usa TSan internamente, no la versión C)

2017    Soporte mejorado para C11/C++11 atomics
        TSan entiende memory_order_relaxed vs acquire/release

2019    Rust nightly añade soporte experimental para TSan
        RUSTFLAGS="-Zsanitizer=thread"
        En safe Rust: no debería encontrar nada (!)
        En unsafe Rust: puede encontrar data races reales

2023    TSan maduro, usado en:
        Chrome, Firefox, Android, Linux kernel (KCSAN),
        Go (go -race), LLVM itself
```

---

## 3. El problema de la concurrencia

```
┌──────────────────────────────────────────────────────────────────┐
│           ¿Por qué los bugs de concurrencia son especiales?      │
│                                                                  │
│  Bug secuencial:                                                 │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  Input A ──▶ Bug ──▶ Crash                                │    │
│  │  Determinista: mismo input = mismo crash                  │    │
│  │  Reproducible: ejecutar de nuevo con mismo input          │    │
│  │  Localizable: stack trace apunta al problema              │    │
│  └──────────────────────────────────────────────────────────┘    │
│                                                                  │
│  Bug de concurrencia:                                            │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  Input A ──┐                                              │    │
│  │            ├──▶ ¿Bug? ──▶ ¿Crash?                         │    │
│  │  Input B ──┘                                              │    │
│  │                                                           │    │
│  │  Depende de:                                              │    │
│  │  • Orden de scheduling de hilos (OS decide)               │    │
│  │  • Carga del sistema (otros procesos)                     │    │
│  │  • Número de CPUs (1 CPU vs 8 CPUs)                       │    │
│  │  • Modelo de memoria del hardware (x86 vs ARM)            │    │
│  │  • Cache coherency timing                                 │    │
│  │  • Interrupciones, context switches                       │    │
│  │                                                           │    │
│  │  Resultado: el bug aparece aleatoriamente                 │    │
│  │  "Works on my machine" ← frase más peligrosa              │    │
│  └──────────────────────────────────────────────────────────┘    │
└──────────────────────────────────────────────────────────────────┘
```

### El modelo de memoria de C11

C11 introdujo un modelo de memoria formal que define qué accesos concurrentes son válidos:

```
┌─────────────────────────────────────────────────────────────────┐
│              Modelo de memoria de C11                             │
│                                                                  │
│  Acceso concurrente a misma ubicación:                           │
│                                                                  │
│  Hilo A          Hilo B         ¿Es data race?                  │
│  ──────          ──────         ────────────────                 │
│  read            read           NO (dos reads son OK)            │
│  read            write          SÍ (sin sincronización)          │
│  write           read           SÍ (sin sincronización)          │
│  write           write          SÍ (sin sincronización)          │
│  atomic read     atomic write   NO (atomics son seguros)         │
│  mutex lock+read mutex lock+wr  NO (mutex sincroniza)            │
│                                                                  │
│  "Sin sincronización" = sin happens-before relationship          │
│  entre los dos accesos.                                          │
│                                                                  │
│  Sincronización válida en C:                                     │
│  • pthread_mutex_lock / unlock                                   │
│  • pthread_cond_wait / signal / broadcast                        │
│  • pthread_rwlock_*                                              │
│  • sem_wait / sem_post                                           │
│  • pthread_create / join (creación/destrucción de hilo)          │
│  • C11 atomics (_Atomic, atomic_load, atomic_store)              │
│  • pthread_barrier_wait                                          │
│  • pthread_once                                                  │
└─────────────────────────────────────────────────────────────────┘
```

---

## 4. Data race: definición formal

### Definición del estándar C11 (§5.1.2.4)

Un **data race** ocurre cuando:

1. Dos o más hilos acceden a la **misma ubicación de memoria**
2. Al menos uno de los accesos es de **escritura**
3. Los accesos no están **ordenados** por una relación happens-before

Si un programa tiene un data race, su comportamiento es **undefined** (UB). No es un crash definido ni un resultado predecible — es UB total, como signed overflow.

```
┌─────────────────────────────────────────────────────────────────┐
│              Data race = Undefined Behavior                      │
│                                                                  │
│  int x = 0;  // Variable compartida, NO atómica, NO protegida   │
│                                                                  │
│  Thread A:    Thread B:                                          │
│  x = 1;       x = 2;      ← Data race: dos writes sin sync     │
│                                                                  │
│  ¿Qué valor tiene x después?                                    │
│  Respuesta: UNDEFINED BEHAVIOR                                  │
│  NO es "1 o 2 dependiendo de quién gane".                        │
│  Es UB: el compilador puede hacer CUALQUIER COSA.                │
│                                                                  │
│  En la práctica, en x86 PROBABLEMENTE es 1 o 2.                 │
│  Pero el compilador podría:                                      │
│  • Cachear x en un registro y nunca leer de memoria              │
│  • Reordenar instrucciones alrededor del store                   │
│  • Eliminar uno de los stores como "redundante"                  │
│  • Escribir un valor parcial (torn write en 64-bit en 32-bit)    │
│                                                                  │
│  En ARM, la situación es AÚN PEOR:                               │
│  • La CPU puede reordenar stores visibles a otros cores          │
│  • Writes de 64 bits no son atómicos en ARM de 32 bits           │
│  • El cache puede mostrar valores "stale" a otros cores          │
└─────────────────────────────────────────────────────────────────┘
```

---

## 5. Data race vs race condition

Estos dos conceptos se confunden frecuentemente pero son distintos:

```
┌──────────────────────────────────────────────────────────────────────┐
│          Data race vs Race condition                                  │
│                                                                      │
│  DATA RACE (TSan lo detecta):                                        │
│  ─────────                                                           │
│  Definición: acceso concurrente sin sincronización a misma memoria   │
│  Resultado:  UNDEFINED BEHAVIOR                                     │
│  Ejemplo:                                                            │
│    int counter = 0;  // No protegido                                 │
│    // Thread A: counter++                                            │
│    // Thread B: counter++                                            │
│    // Data race: ambos leen-modifican-escriben sin lock              │
│  Solución: mutex, atomic, o eliminar acceso compartido               │
│                                                                      │
│  RACE CONDITION (TSan NO lo detecta):                                │
│  ──────────────                                                      │
│  Definición: resultado depende del orden de ejecución de hilos       │
│  Resultado:  comportamiento DEFINIDO pero incorrecto                 │
│  Ejemplo:                                                            │
│    pthread_mutex_t m;                                                │
│    int balance = 100;                                                │
│    // Thread A:                                                      │
│    pthread_mutex_lock(&m);                                           │
│    if (balance >= 50) { balance -= 50; } // withdraw                 │
│    pthread_mutex_unlock(&m);                                         │
│    // Thread B:                                                      │
│    pthread_mutex_lock(&m);                                           │
│    if (balance >= 80) { balance -= 80; } // withdraw                 │
│    pthread_mutex_unlock(&m);                                         │
│    // NO hay data race (protegido por mutex)                         │
│    // PERO: resultado depende de quién ejecuta primero               │
│    // Si A primero: balance = 50, B falla (50 < 80)                  │
│    // Si B primero: balance = 20, A falla (20 < 50)                  │
│    // Es race condition, pero NO data race                           │
│  Solución: lógica de aplicación (transacciones, colas, etc.)         │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────┐     │
│  │  Todo data race es una race condition.                       │     │
│  │  Pero NO toda race condition es un data race.                │     │
│  │  TSan detecta data races, no race conditions lógicas.        │     │
│  └─────────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 6. Arquitectura de TSan

```
┌──────────────────────────────────────────────────────────────────────┐
│                     Arquitectura de TSan v2                           │
│                                                                      │
│  ┌─────────────────────┐    ┌──────────────────────────────────┐     │
│  │  Código fuente (.c)  │    │  Instrumentación del compilador  │     │
│  │                      │───▶│                                  │     │
│  │  x = 42;             │    │  Para CADA load y store:         │     │
│  │  y = x + 1;          │    │  1. Registrar {thread, clock,    │     │
│  │                      │    │     addr, size, is_write}        │     │
│  └─────────────────────┘    │  2. Comparar con accesos previos │     │
│                              │     de OTROS threads              │     │
│                              │  3. Verificar happens-before      │     │
│                              └──────────────────────────────────┘     │
│                                          │                            │
│                                          ▼                            │
│  ┌──────────────────────────────────────────────────────────────┐     │
│  │                    Runtime library                            │     │
│  │                                                              │     │
│  │  • Intercepta pthread_mutex_lock/unlock                      │     │
│  │    → Registra relaciones happens-before                      │     │
│  │                                                              │     │
│  │  • Intercepta pthread_create/join                            │     │
│  │    → Registra inicio/fin de threads                          │     │
│  │                                                              │     │
│  │  • Intercepta atomic operations                              │     │
│  │    → Registra sincronización implícita                       │     │
│  │                                                              │     │
│  │  • Intercepta malloc/free                                    │     │
│  │    → Rastrear ownership de memoria                           │     │
│  │                                                              │     │
│  │  • Intercepta pthread_cond_wait/signal                       │     │
│  │    → Registra sincronización por condvar                     │     │
│  │                                                              │     │
│  │  • Gestiona vector clocks por thread                         │     │
│  └──────────────────────────────────────────────────────────────┘     │
│                                          │                            │
│                                          ▼                            │
│  ┌──────────────────────────────────────────────────────────────┐     │
│  │                    Shadow memory (4 shadow words por 8 bytes) │     │
│  │                                                              │     │
│  │  App memory:    [...8 bytes...]                               │     │
│  │  Shadow slot 0: {thread_id, clock, addr, is_write}           │     │
│  │  Shadow slot 1: {thread_id, clock, addr, is_write}           │     │
│  │  Shadow slot 2: {thread_id, clock, addr, is_write}           │     │
│  │  Shadow slot 3: {thread_id, clock, addr, is_write}           │     │
│  │                                                              │     │
│  │  Los 4 slots guardan los últimos 4 accesos a esos 8 bytes. │     │
│  │  Cuando un nuevo acceso llega, se compara con los 4 slots   │     │
│  │  para verificar happens-before con accesos de otros threads. │     │
│  └──────────────────────────────────────────────────────────────┘     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 7. Shadow memory en TSan: vector clocks

### Shadow word

```
┌─────────────────────────────────────────────────────────────────┐
│              Shadow word de TSan (8 bytes)                        │
│                                                                  │
│  Bit layout de un shadow word (64 bits):                         │
│                                                                  │
│  [63]     [62:42]        [41:32]       [31:0]                    │
│  is_write  unused/epoch   thread_id     position_in_8bytes       │
│  (1 bit)   (21 bits)     (10 bits)     (32 bits)                │
│                                                                  │
│  Más precisamente (simplificado):                                │
│  ┌────────┬─────────────────┬───────────┬──────────┐             │
│  │is_write│    epoch (clock) │ thread_id │ size+off │             │
│  │ 1 bit  │    ~21 bits      │ ~10 bits  │ ~32 bits │             │
│  └────────┴─────────────────┴───────────┴──────────┘             │
│                                                                  │
│  Cada 8 bytes de app memory tiene 4 shadow words (32 bytes).     │
│  Ratio: 8 bytes app → 32 bytes shadow = 4:1                     │
│  Total memory overhead: ~5-8x                                    │
│                                                                  │
│  Los 4 slots funcionan como mini-cache de accesos recientes:     │
│  Slot 0: último acceso del thread A                              │
│  Slot 1: último acceso del thread B                              │
│  Slot 2: último acceso del thread C                              │
│  Slot 3: último acceso del thread D                              │
│  Si hay un 5to thread, el slot más antiguo se reemplaza (LRU).   │
└─────────────────────────────────────────────────────────────────┘
```

### Vector clocks

```
┌──────────────────────────────────────────────────────────────────┐
│                    Vector clocks en TSan                          │
│                                                                  │
│  Cada thread tiene un "reloj" (epoch) que incrementa con cada   │
│  operación de sincronización. Un vector clock es un array de    │
│  épocas, una por thread:                                         │
│                                                                  │
│  Thread 0: epoch = 5                                             │
│  Thread 1: epoch = 3                                             │
│  Thread 2: epoch = 7                                             │
│  Vector clock de Thread 1: [5, 3, 7]                             │
│  (Thread 1 ha "visto" epoch 5 de T0, epoch 3 de T1, epoch 7     │
│   de T2, a través de sincronización)                             │
│                                                                  │
│  Cuando Thread 1 hace unlock(mutex):                             │
│    mutex.clock = Thread1.clock  (copia vector clock al mutex)    │
│  Cuando Thread 2 hace lock(mutex):                               │
│    Thread2.clock = max(Thread2.clock, mutex.clock)               │
│    → Thread 2 ahora "ha visto" todo lo que Thread 1 hizo         │
│    → Establece happens-before: T1.unlock → T2.lock              │
│                                                                  │
│  Para detectar data race:                                        │
│  Thread A escribe x con epoch 5                                  │
│  Thread B lee x                                                  │
│  TSan verifica: ¿el vector clock de B incluye epoch >= 5 de A?  │
│  Si NO → data race (B no ha sincronizado con A desde epoch 5)   │
│  Si SÍ → no race (B tiene happens-before con la escritura de A) │
└──────────────────────────────────────────────────────────────────┘
```

### Ejemplo visual de detección

```
Timeline:
                 t=1      t=2      t=3      t=4      t=5
Thread A:     x = 1    lock(m)  unlock(m)
Thread B:                                 lock(m)  y = x
                                                    ↑ ¿Data race?

Vector clocks:
  t=1: A escribe x.  Shadow de x: {A, epoch=1, write}
  t=2: A lock(m).    A.clock = [2, 0]
  t=3: A unlock(m).  m.clock = [3, 0]
  t=4: B lock(m).    B.clock = max([0, 1], [3, 0]) = [3, 1]
  t=5: B lee x.      TSan verifica: B.clock[A] = 3 >= 1 (epoch de write)
                      → happens-before existe → NO data race ✓

Sin el mutex:
  t=1: A escribe x.  Shadow de x: {A, epoch=1, write}
  t=5: B lee x.      B.clock[A] = 0 < 1 (epoch de write)
                      → NO happens-before → DATA RACE ✗
```

---

## 8. El algoritmo happens-before

### Definición

La relación **happens-before** (→) define un orden parcial entre operaciones:

```
┌──────────────────────────────────────────────────────────────────┐
│            Reglas de happens-before                               │
│                                                                  │
│  1. PROGRAM ORDER                                                │
│     Dentro del mismo thread, cada instrucción happens-before     │
│     la siguiente:                                                │
│     a = 1;     ─→    b = 2;     ─→    c = 3;                    │
│                                                                  │
│  2. MUTEX                                                        │
│     unlock(m)  ─→  lock(m)  (del SIGUIENTE holder)              │
│     Thread A: lock(m); x=1; unlock(m);                          │
│     Thread B:                lock(m); y=x; unlock(m);            │
│     unlock(m) de A  ─→  lock(m) de B                             │
│     Por transitividad: x=1  ─→  y=x  (B ve x=1)                │
│                                                                  │
│  3. THREAD CREATE/JOIN                                           │
│     pthread_create(T)  ─→  primera instrucción de T             │
│     última instrucción de T  ─→  pthread_join(T)                │
│                                                                  │
│  4. CONDITION VARIABLE                                           │
│     pthread_cond_signal/broadcast  ─→  pthread_cond_wait return │
│                                                                  │
│  5. ATOMIC OPERATIONS                                            │
│     atomic_store(release)  ─→  atomic_load(acquire) del mismo   │
│     objeto atómico                                               │
│                                                                  │
│  6. BARRIERS                                                     │
│     pthread_barrier_wait  ─→  pthread_barrier_wait              │
│     (todos los threads en la barrera sincronizan)                │
│                                                                  │
│  7. TRANSITIVIDAD                                                │
│     Si A ─→ B  y  B ─→ C, entonces A ─→ C                      │
│                                                                  │
│  Si dos accesos NO tienen relación happens-before y al menos    │
│  uno es write → DATA RACE                                        │
└──────────────────────────────────────────────────────────────────┘
```

### Lo que TSan intercepta

```
┌────────────────────────────────────────────────────┬────────────────┐
│ Función interceptada                                │ Relación HB    │
├────────────────────────────────────────────────────┼────────────────┤
│ pthread_mutex_lock / unlock                        │ unlock → lock  │
│ pthread_rwlock_rdlock / wrlock / unlock             │ unlock → lock  │
│ pthread_spin_lock / unlock                          │ unlock → lock  │
│ pthread_create                                      │ create → start │
│ pthread_join                                        │ end → join     │
│ pthread_cond_signal / broadcast                    │ signal → wait  │
│ pthread_cond_wait                                   │ (receiver)     │
│ pthread_barrier_wait                                │ all sync       │
│ pthread_once                                        │ once → others  │
│ sem_post / sem_wait                                 │ post → wait    │
│ atomic_store(release) / atomic_load(acquire)       │ store → load   │
│ atomic_exchange / compare_exchange                  │ exchange → load│
│ __atomic_* builtins                                 │ según orden    │
│ malloc / free                                       │ alloc tracking │
└────────────────────────────────────────────────────┴────────────────┘
```

---

## 9. Compilar con TSan en C

### Flags de compilación

```bash
# Básico:
clang -fsanitize=thread -g -O1 -pthread program.c -o program

# Recomendado:
clang -fsanitize=thread \
      -fno-omit-frame-pointer \
      -g \
      -O1 \
      -pthread \
      program.c -o program

# Con GCC:
gcc -fsanitize=thread \
    -fno-omit-frame-pointer \
    -g \
    -O1 \
    -pthread \
    program.c -o program

# Desglose:
# -fsanitize=thread         → Activa TSan
# -fno-omit-frame-pointer   → Stack traces legibles
# -g                        → Símbolos de debug
# -O1                       → Optimización leve (recomendado)
# -pthread                  → Enlazar con libpthread
```

### Con Makefile

```makefile
CC = clang
CFLAGS = -fsanitize=thread -fno-omit-frame-pointer -g -O1 -pthread
LDFLAGS = -fsanitize=thread -pthread

all: program

program: main.o worker.o
	$(CC) $(LDFLAGS) $^ -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f *.o program
```

### ¿Por qué -O1?

```
-O0: genera accesos redundantes a memoria que TSan reportaría
     como races "false" (en realidad son artefactos de compilación).
-O1: elimina artefactos sin perder información de debug.
-O2: puede optimizar AWAY accesos que realmente son races.
     Ejemplo: el compilador puede cachear en registro una variable
     compartida, eliminando el load visible a TSan.

Recomendación: usar -O1 para detección, -O2 solo si necesitas
               rendimiento en el binario de testing.
```

### CRUCIAL: todos los archivos deben compilarse con TSan

```bash
# ERROR: compilar un archivo sin TSan y enlazar con TSan
clang -c utils.c -o utils.o                      # Sin TSan
clang -fsanitize=thread main.c utils.o -o program # Con TSan
# Resultado: TSan no ve los accesos en utils.c
# → Puede perder data races en ese archivo

# CORRECTO: todo con TSan
clang -fsanitize=thread -c utils.c -o utils.o
clang -fsanitize=thread -c main.c -o main.o
clang -fsanitize=thread utils.o main.o -o program
```

---

## 10. Compilar con TSan en Rust

### Comando básico

```bash
# Compilar un programa:
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly run --target x86_64-unknown-linux-gnu

# Ejecutar tests:
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly test --target x86_64-unknown-linux-gnu
```

### Diferencia con MSan: no necesita -Zbuild-std

A diferencia de MSan, TSan **no** requiere `-Zbuild-std`. La stdlib de Rust no necesita recompilarse para TSan porque TSan funciona a nivel de accesos a memoria, no de inicialización.

```bash
# MSan (requiere -Zbuild-std):
RUSTFLAGS="-Zsanitizer=memory" \
    cargo +nightly run -Zbuild-std --target x86_64-unknown-linux-gnu

# TSan (NO requiere -Zbuild-std):
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly run --target x86_64-unknown-linux-gnu
```

### ¿Cuándo usar TSan en Rust?

```
┌─────────────────────────────────────────────────────────────────┐
│          TSan en Rust: ¿cuándo tiene sentido?                    │
│                                                                  │
│  Safe Rust: NUNCA debería encontrar data races                  │
│  ─────────                                                       │
│  El sistema de ownership + Send/Sync GARANTIZA ausencia de       │
│  data races. Si TSan encuentra algo en safe Rust, es un bug      │
│  del compilador de Rust.                                         │
│                                                                  │
│  Unsafe Rust: SÍ puede encontrar data races                     │
│  ────────────                                                    │
│  Cuando usas:                                                    │
│  • Raw pointers compartidos entre threads                        │
│  • Implementaciones manuales de Send/Sync                        │
│  • Código que bypasea el borrow checker con unsafe               │
│  • FFI con bibliotecas C que usan threads                        │
│  • Atomics con ordering incorrecto (Relaxed donde debería        │
│    ser Acquire/Release)                                          │
│                                                                  │
│  En resumen: TSan en Rust es para verificar que tu unsafe        │
│  code y tus implementaciones de Send/Sync son correctas.         │
└─────────────────────────────────────────────────────────────────┘
```

---

## 11. Bug 1: data race simple — dos hilos, una variable

### En C

```c
// race_simple.c
#include <stdio.h>
#include <pthread.h>

int counter = 0;  // Variable compartida, NO protegida

void *increment(void *arg) {
    for (int i = 0; i < 1000000; i++) {
        counter++;  // Data race: read-modify-write sin sincronización
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;

    pthread_create(&t1, NULL, increment, NULL);
    pthread_create(&t2, NULL, increment, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // Esperamos 2000000, pero probablemente es menos
    printf("Counter: %d\n", counter);
    return 0;
}
```

```bash
$ clang -fsanitize=thread -g -O1 -pthread race_simple.c -o race_simple
$ ./race_simple

==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x... by thread T2:
    #0 increment race_simple.c:8:9 (race_simple+0x...)
    #1 ... in __tsan_thread_start

  Previous write of size 4 at 0x... by thread T1:
    #0 increment race_simple.c:8:9 (race_simple+0x...)
    #1 ... in __tsan_thread_start

  Location is global 'counter' of size 4 at 0x...

  Thread T1 (tid=..., running) created by main thread at:
    #0 pthread_create ...
    #1 main race_simple.c:14:5

  Thread T2 (tid=..., running) created by main thread at:
    #0 pthread_create ...
    #1 main race_simple.c:15:5

SUMMARY: ThreadSanitizer: data race race_simple.c:8:9 in increment
==================
Counter: 1847293
ThreadSanitizer: reported 1 warnings
```

### Corrección

```c
// Opción 1: mutex
pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;

void *increment_safe(void *arg) {
    for (int i = 0; i < 1000000; i++) {
        pthread_mutex_lock(&m);
        counter++;
        pthread_mutex_unlock(&m);
    }
    return NULL;
}

// Opción 2: atomic (mejor rendimiento para este caso)
#include <stdatomic.h>

atomic_int atomic_counter = 0;

void *increment_atomic(void *arg) {
    for (int i = 0; i < 1000000; i++) {
        atomic_fetch_add(&atomic_counter, 1);
    }
    return NULL;
}
```

### En Rust unsafe

```rust
use std::thread;

static mut COUNTER: i32 = 0;  // Mutable static: unsafe para acceder

fn main() {
    let t1 = thread::spawn(|| {
        for _ in 0..1_000_000 {
            unsafe {
                COUNTER += 1;  // Data race: sin sincronización
            }
        }
    });

    let t2 = thread::spawn(|| {
        for _ in 0..1_000_000 {
            unsafe {
                COUNTER += 1;  // Data race
            }
        }
    });

    t1.join().unwrap();
    t2.join().unwrap();

    unsafe {
        println!("Counter: {}", COUNTER);
    }
}
```

---

## 12. Bug 2: data race en struct compartido

### En C

```c
// race_struct.c
#include <stdio.h>
#include <pthread.h>
#include <string.h>

typedef struct {
    int x;
    int y;
    char name[32];
} Point;

Point shared_point = {0, 0, "origin"};

void *writer(void *arg) {
    for (int i = 0; i < 100000; i++) {
        // Data race: modifica sin lock
        shared_point.x = i;
        shared_point.y = i * 2;
        snprintf(shared_point.name, 32, "point_%d", i);
    }
    return NULL;
}

void *reader(void *arg) {
    for (int i = 0; i < 100000; i++) {
        // Data race: lee sin lock
        int x = shared_point.x;
        int y = shared_point.y;
        char name[32];
        strncpy(name, shared_point.name, 32);

        // Invariante roto: y debería ser 2*x
        if (y != 2 * x) {
            // Esto pasa frecuentemente por la data race:
            // writer actualiza x pero aún no actualizó y
        }
    }
    return NULL;
}

int main() {
    pthread_t tw, tr;
    pthread_create(&tw, NULL, writer, NULL);
    pthread_create(&tr, NULL, reader, NULL);
    pthread_join(tw, NULL);
    pthread_join(tr, NULL);
    return 0;
}
```

```bash
$ clang -fsanitize=thread -g -O1 -pthread race_struct.c -o race_struct
$ ./race_struct

==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x... by thread T1:
    #0 writer race_struct.c:16:24 (race_struct+0x...)

  Previous read of size 4 at 0x... by thread T2:
    #0 reader race_struct.c:25:17 (race_struct+0x...)

  Location is global 'shared_point' of size 40 at 0x... (shared_point+0x0)
==================
```

TSan reporta **cada campo** con data race por separado. En este caso reportará races en `.x`, `.y`, y `.name`.

---

## 13. Bug 3: data race con array compartido

### En C

```c
// race_array.c
#include <stdio.h>
#include <pthread.h>

#define SIZE 100
int shared_array[SIZE];

void *fill_even(void *arg) {
    for (int i = 0; i < SIZE; i += 2) {
        shared_array[i] = i * 10;  // Escribe índices pares
    }
    return NULL;
}

void *fill_odd(void *arg) {
    for (int i = 1; i < SIZE; i += 2) {
        shared_array[i] = i * 10;  // Escribe índices impares
    }
    return NULL;
}

void *sum_all(void *arg) {
    int total = 0;
    for (int i = 0; i < SIZE; i++) {
        total += shared_array[i];  // Lee TODOS los índices
        // Data race con fill_even en pares
        // Data race con fill_odd en impares
    }
    int *result = (int *)arg;
    *result = total;
    return NULL;
}

int main() {
    pthread_t te, to, ts;
    int sum = 0;

    pthread_create(&te, NULL, fill_even, NULL);
    pthread_create(&to, NULL, fill_odd, NULL);
    pthread_create(&ts, NULL, sum_all, &sum);

    pthread_join(te, NULL);
    pthread_join(to, NULL);
    pthread_join(ts, NULL);

    printf("Sum: %d\n", sum);
    return 0;
}
```

**Nota**: si `fill_even` y `fill_odd` escriben en posiciones **disjuntas**, NO hay data race entre ellos (diferentes ubicaciones de memoria). Pero `sum_all` lee **todas** las posiciones mientras los otros hilos escriben, así que SÍ hay data race entre `sum_all` y los escritores.

### Corrección

```c
// Opción 1: completar escrituras antes de leer (barrier)
pthread_barrier_t barrier;
pthread_barrier_init(&barrier, NULL, 3);

void *fill_even_safe(void *arg) {
    for (int i = 0; i < SIZE; i += 2)
        shared_array[i] = i * 10;
    pthread_barrier_wait(&barrier);  // Esperar a que todos terminen
    return NULL;
}

void *fill_odd_safe(void *arg) {
    for (int i = 1; i < SIZE; i += 2)
        shared_array[i] = i * 10;
    pthread_barrier_wait(&barrier);
    return NULL;
}

void *sum_all_safe(void *arg) {
    pthread_barrier_wait(&barrier);  // Esperar a que escriban
    int total = 0;
    for (int i = 0; i < SIZE; i++)
        total += shared_array[i];
    *(int *)arg = total;
    return NULL;
}

// Opción 2: join los escritores antes de crear el lector
pthread_join(te, NULL);  // Esperar fill_even
pthread_join(to, NULL);  // Esperar fill_odd
// Ahora es seguro leer
pthread_create(&ts, NULL, sum_all, &sum);
pthread_join(ts, NULL);
```

---

## 14. Bug 4: data race por falta de lock

### En C

```c
// race_nolock.c
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    int *items;
    int count;
    int capacity;
    pthread_mutex_t lock;
} IntVec;

IntVec *vec_new(int cap) {
    IntVec *v = malloc(sizeof(IntVec));
    v->items = malloc(cap * sizeof(int));
    v->count = 0;
    v->capacity = cap;
    pthread_mutex_init(&v->lock, NULL);
    return v;
}

void vec_push(IntVec *v, int value) {
    pthread_mutex_lock(&v->lock);
    if (v->count < v->capacity) {
        v->items[v->count] = value;
        v->count++;
    }
    pthread_mutex_unlock(&v->lock);
}

int vec_get(IntVec *v, int index) {
    // Bug: lee sin tomar el lock
    // Data race con vec_push que modifica items y count
    if (index < v->count) {        // Data race en v->count
        return v->items[index];    // Data race en v->items[index]
    }
    return -1;
}

int vec_len(IntVec *v) {
    // Bug: lee sin lock
    return v->count;  // Data race con vec_push
}

void *producer(void *arg) {
    IntVec *v = (IntVec *)arg;
    for (int i = 0; i < 10000; i++) {
        vec_push(v, i);
    }
    return NULL;
}

void *consumer(void *arg) {
    IntVec *v = (IntVec *)arg;
    for (int i = 0; i < 10000; i++) {
        int len = vec_len(v);       // Data race
        if (len > 0) {
            int val = vec_get(v, len - 1);  // Data race
            (void)val;
        }
    }
    return NULL;
}

int main() {
    IntVec *v = vec_new(20000);

    pthread_t tp, tc;
    pthread_create(&tp, NULL, producer, v);
    pthread_create(&tc, NULL, consumer, v);

    pthread_join(tp, NULL);
    pthread_join(tc, NULL);

    printf("Final count: %d\n", vec_len(v));
    free(v->items);
    free(v);
    return 0;
}
```

### Corrección

```c
int vec_get_safe(IntVec *v, int index) {
    pthread_mutex_lock(&v->lock);  // Tomar lock
    int result = -1;
    if (index < v->count) {
        result = v->items[index];
    }
    pthread_mutex_unlock(&v->lock);
    return result;
}

int vec_len_safe(IntVec *v) {
    pthread_mutex_lock(&v->lock);
    int len = v->count;
    pthread_mutex_unlock(&v->lock);
    return len;
}
```

---

## 15. Bug 5: data race por lock incorrecto

### En C

```c
// race_wronglock.c
#include <stdio.h>
#include <pthread.h>

int balance_a = 1000;
int balance_b = 2000;
pthread_mutex_t lock_a = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_b = PTHREAD_MUTEX_INITIALIZER;

void *transfer_a_to_b(void *arg) {
    int amount = *(int *)arg;
    pthread_mutex_lock(&lock_a);
    balance_a -= amount;
    pthread_mutex_unlock(&lock_a);

    // Bug: usa lock_a para proteger balance_b
    pthread_mutex_lock(&lock_a);   // Lock INCORRECTO
    balance_b += amount;           // Debería usar lock_b
    pthread_mutex_unlock(&lock_a);
    return NULL;
}

void *read_balance_b(void *arg) {
    pthread_mutex_lock(&lock_b);   // Lock CORRECTO para balance_b
    int b = balance_b;             // Data race: transfer usa lock_a
    pthread_mutex_unlock(&lock_b);
    printf("Balance B: %d\n", b);
    return NULL;
}

int main() {
    int amount = 100;
    pthread_t t1, t2;

    pthread_create(&t1, NULL, transfer_a_to_b, &amount);
    pthread_create(&t2, NULL, read_balance_b, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    return 0;
}
```

TSan detecta esta race porque `balance_b` es accedido por un hilo con `lock_a` y por otro con `lock_b` — son locks **diferentes**, así que no hay happens-before entre los accesos.

### Corrección

```c
void *transfer_a_to_b_safe(void *arg) {
    int amount = *(int *)arg;
    pthread_mutex_lock(&lock_a);
    balance_a -= amount;
    pthread_mutex_unlock(&lock_a);

    pthread_mutex_lock(&lock_b);  // Lock CORRECTO
    balance_b += amount;
    pthread_mutex_unlock(&lock_b);
    return NULL;
}
```

---

## 16. Bug 6: data race con variable de condición

### En C

```c
// race_condvar.c
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>

int data = 0;
bool ready = false;  // Bug: NO protegida por mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;

void *producer(void *arg) {
    // Preparar datos
    data = 42;
    
    // Bug: modificar ready sin lock
    ready = true;  // Data race con el consumer

    pthread_cond_signal(&cond);  // Señalar sin lock
    return NULL;
}

void *consumer(void *arg) {
    pthread_mutex_lock(&mutex);
    while (!ready) {  // Bug: lee ready dentro del lock,
                      // pero producer escribe fuera del lock
        pthread_cond_wait(&cond, &mutex);
    }
    printf("Data: %d\n", data);
    pthread_mutex_unlock(&mutex);
    return NULL;
}

int main() {
    pthread_t tp, tc;
    pthread_create(&tc, NULL, consumer, NULL);
    pthread_create(&tp, NULL, producer, NULL);
    pthread_join(tp, NULL);
    pthread_join(tc, NULL);
    return 0;
}
```

### Corrección

```c
void *producer_safe(void *arg) {
    pthread_mutex_lock(&mutex);    // Lock ANTES de modificar
    data = 42;
    ready = true;
    pthread_cond_signal(&cond);    // Signal con lock (recomendado)
    pthread_mutex_unlock(&mutex);
    return NULL;
}

// Consumer permanece igual: ya usa mutex correctamente
```

---

## 17. Bug 7: data race por flag de terminación

Uno de los data races más comunes: un flag booleano para indicar que un hilo debe terminar.

### En C

```c
// race_flag.c
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

bool should_stop = false;  // Bug: flag compartido sin sincronización

void *worker(void *arg) {
    int count = 0;
    while (!should_stop) {  // Data race: lee sin sincronización
        count++;
        // Simular trabajo
    }
    printf("Worker did %d iterations\n", count);
    return NULL;
}

int main() {
    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);

    usleep(100000);  // Dejar correr 100ms

    should_stop = true;  // Data race: escribe sin sincronización

    pthread_join(t, NULL);
    return 0;
}
```

### ¿Por qué es un problema real?

```
┌─────────────────────────────────────────────────────────────────┐
│      ¿Por qué un simple bool flag es un data race?               │
│                                                                  │
│  "Solo estoy escribiendo true, ¿qué puede salir mal?"           │
│                                                                  │
│  1. El compilador puede cachear should_stop en un registro:     │
│     while (!should_stop) → el compilador ve que esta función    │
│     no modifica should_stop, así que puede generar:             │
│     registro = should_stop;                                      │
│     while (!registro) { ... }  // LOOP INFINITO                 │
│     (volatile NO es la solución correcta, es atomic)             │
│                                                                  │
│  2. En ARM, el store de true puede no ser visible al otro core  │
│     sin una barrera de memoria.                                  │
│                                                                  │
│  3. El estándar C dice que es UB, así que TODO puede pasar.     │
└─────────────────────────────────────────────────────────────────┘
```

### Corrección

```c
#include <stdatomic.h>

// Opción 1: atomic (la mejor para este caso)
atomic_bool should_stop = false;

void *worker_safe(void *arg) {
    int count = 0;
    while (!atomic_load(&should_stop)) {
        count++;
    }
    printf("Worker did %d iterations\n", count);
    return NULL;
}

// En main:
atomic_store(&should_stop, true);
```

---

## 18. Bug 8: deadlock (lock ordering)

TSan también puede detectar potenciales **deadlocks** por inversión del orden de locks.

### En C

```c
// deadlock.c
#include <stdio.h>
#include <pthread.h>

pthread_mutex_t lock_x = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t lock_y = PTHREAD_MUTEX_INITIALIZER;
int x = 0, y = 0;

void *thread_a(void *arg) {
    // Toma lock_x primero, luego lock_y
    pthread_mutex_lock(&lock_x);
    pthread_mutex_lock(&lock_y);   // Potencial deadlock
    x = 1;
    y = 2;
    pthread_mutex_unlock(&lock_y);
    pthread_mutex_unlock(&lock_x);
    return NULL;
}

void *thread_b(void *arg) {
    // Toma lock_y primero, luego lock_x (ORDEN INVERSO)
    pthread_mutex_lock(&lock_y);
    pthread_mutex_lock(&lock_x);   // Potencial deadlock
    y = 3;
    x = 4;
    pthread_mutex_unlock(&lock_x);
    pthread_mutex_unlock(&lock_y);
    return NULL;
}

int main() {
    pthread_t ta, tb;
    pthread_create(&ta, NULL, thread_a, NULL);
    pthread_create(&tb, NULL, thread_b, NULL);
    pthread_join(ta, NULL);
    pthread_join(tb, NULL);
    printf("x=%d, y=%d\n", x, y);
    return 0;
}
```

```
┌─────────────────────────────────────────────────────────────────┐
│              Deadlock: inversión de orden de locks               │
│                                                                  │
│  Thread A:              Thread B:                                │
│  lock(X) ✓              lock(Y) ✓                               │
│  lock(Y) ← ESPERA       lock(X) ← ESPERA                       │
│       ↑                       ↑                                  │
│       └───── DEADLOCK ────────┘                                  │
│                                                                  │
│  Thread A tiene X, espera Y                                      │
│  Thread B tiene Y, espera X                                      │
│  Ninguno puede avanzar → programa congelado                     │
└─────────────────────────────────────────────────────────────────┘
```

```bash
$ clang -fsanitize=thread -g -O1 -pthread deadlock.c -o deadlock
$ ./deadlock

==================
WARNING: ThreadSanitizer: lock-order-inversion (potential deadlock) (pid=12345)
  Cycle in lock order graph: M1 (0x...) => M2 (0x...) => M1

  Mutex M2 acquired here while holding mutex M1 in thread T1:
    #0 pthread_mutex_lock ...
    #1 thread_a deadlock.c:11:5

  Mutex M1 acquired here while holding mutex M2 in thread T2:
    #0 pthread_mutex_lock ...
    #1 thread_b deadlock.c:20:5

SUMMARY: ThreadSanitizer: lock-order-inversion (potential deadlock)
==================
```

TSan detecta el **potencial** deadlock incluso si no ocurre en esta ejecución particular. Analiza el grafo de orden de locks y detecta ciclos.

### Corrección

```c
// Siempre tomar locks en el MISMO orden:
void *thread_b_safe(void *arg) {
    // Mismo orden que thread_a: primero X, luego Y
    pthread_mutex_lock(&lock_x);
    pthread_mutex_lock(&lock_y);
    y = 3;
    x = 4;
    pthread_mutex_unlock(&lock_y);
    pthread_mutex_unlock(&lock_x);
    return NULL;
}
```

---

## 19. Bug 9: data race en contador atómico mal usado

Usar atomics no elimina automáticamente todos los data races. El **ordering** importa.

### En C

```c
// race_atomic_wrong.c
#include <stdio.h>
#include <pthread.h>
#include <stdatomic.h>

int data = 0;                          // NO atómico
atomic_int data_ready = 0;             // Atómico, pero ordering importa

void *producer(void *arg) {
    data = 42;                         // Escribe dato no-atómico

    // Bug: relaxed ordering NO garantiza que data=42 sea visible
    // antes de que el consumer vea data_ready=1
    atomic_store_explicit(&data_ready, 1, memory_order_relaxed);
    return NULL;
}

void *consumer(void *arg) {
    // Esperar a que data_ready sea 1
    while (atomic_load_explicit(&data_ready, memory_order_relaxed) == 0) {
        // spin
    }
    // Bug: data puede NO ser 42 todavía
    // memory_order_relaxed no garantiza visibilidad de data
    printf("Data: %d\n", data);  // Data race en `data`
    return NULL;
}

int main() {
    pthread_t tp, tc;
    pthread_create(&tc, NULL, consumer, NULL);
    pthread_create(&tp, NULL, producer, NULL);
    pthread_join(tp, NULL);
    pthread_join(tc, NULL);
    return 0;
}
```

```
┌─────────────────────────────────────────────────────────────────┐
│         memory_order y data races                                │
│                                                                  │
│  memory_order_relaxed:                                           │
│    Solo garantiza atomicidad de la operación individual.        │
│    NO establece happens-before con otras variables.              │
│    → data=42 puede NO ser visible aunque data_ready=1 lo sea.  │
│                                                                  │
│  memory_order_release (en store):                                │
│    Todas las escrituras ANTERIORES son visibles para quien       │
│    haga acquire del mismo atómico.                               │
│    → data=42 SERÁ visible cuando consumer vea data_ready=1.    │
│                                                                  │
│  memory_order_acquire (en load):                                 │
│    Ve todas las escrituras que el writer hizo antes de su        │
│    release en el mismo atómico.                                  │
│                                                                  │
│  En x86: relaxed y acquire/release suelen ser equivalentes      │
│  (x86 es "TSO" - Total Store Order).                             │
│  En ARM: son MUY diferentes (ARM es weakly ordered).             │
│  → Bugs por relaxed solo aparecen en ARM, pero TSan los         │
│    detecta en x86 también.                                       │
└─────────────────────────────────────────────────────────────────┘
```

### Corrección

```c
void *producer_safe(void *arg) {
    data = 42;
    // release: garantiza que data=42 es visible antes de data_ready=1
    atomic_store_explicit(&data_ready, 1, memory_order_release);
    return NULL;
}

void *consumer_safe(void *arg) {
    // acquire: ve todo lo que producer escribió antes de su release
    while (atomic_load_explicit(&data_ready, memory_order_acquire) == 0) {
        // spin
    }
    printf("Data: %d\n", data);  // Ahora es seguro: happens-before establecido
    return NULL;
}
```

---

## 20. Bug 10: data race en patrón productor-consumidor

### En C

```c
// race_prodcons.c
#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>

#define QUEUE_SIZE 16

typedef struct {
    int buffer[QUEUE_SIZE];
    int head;       // Próxima posición para escribir
    int tail;       // Próxima posición para leer
    int count;      // Número de elementos
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} Queue;

Queue q;

void queue_init(Queue *q) {
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_push(Queue *q, int value) {
    pthread_mutex_lock(&q->lock);
    while (q->count == QUEUE_SIZE) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }
    q->buffer[q->head] = value;
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

int queue_pop(Queue *q) {
    pthread_mutex_lock(&q->lock);
    while (q->count == 0) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }
    int value = q->buffer[q->tail];
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return value;
}

// Bug: verificación de count SIN lock
int queue_is_empty(Queue *q) {
    return q->count == 0;  // Data race: lee count sin lock
}

void *producer(void *arg) {
    for (int i = 0; i < 100; i++) {
        queue_push(&q, i);
    }
    queue_push(&q, -1);  // Señal de terminación
    return NULL;
}

void *consumer(void *arg) {
    while (1) {
        // Bug: verifica is_empty sin lock, luego hace pop
        // Entre el check y el pop, otro thread puede hacer pop
        if (!queue_is_empty(&q)) {  // Data race en count
            int val = queue_pop(&q);
            if (val == -1) break;
            printf("Got: %d\n", val);
        }
    }
    return NULL;
}

int main() {
    queue_init(&q);
    pthread_t tp, tc;
    pthread_create(&tp, NULL, producer, NULL);
    pthread_create(&tc, NULL, consumer, NULL);
    pthread_join(tp, NULL);
    pthread_join(tc, NULL);
    return 0;
}
```

### Corrección

```c
int queue_is_empty_safe(Queue *q) {
    pthread_mutex_lock(&q->lock);
    int empty = (q->count == 0);
    pthread_mutex_unlock(&q->lock);
    return empty;
}

// Mejor aún: el consumer simplemente usa queue_pop que ya espera
void *consumer_safe(void *arg) {
    while (1) {
        int val = queue_pop(&q);  // Espera si está vacío (cond_wait)
        if (val == -1) break;
        printf("Got: %d\n", val);
    }
    return NULL;
}
```

---

## 21. Anatomía completa de un reporte TSan

```
┌──────────────────────────────────────────────────────────────────────┐
│              Reporte TSan completo                                    │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 1: Tipo de error                                       │  │
│  │                                                                │  │
│  │ WARNING: ThreadSanitizer: data race (pid=12345)                │  │
│  │                                                                │  │
│  │ Tipos posibles:                                                │  │
│  │ • data race                                                    │  │
│  │ • lock-order-inversion (potential deadlock)                    │  │
│  │ • thread leak                                                  │  │
│  │ • destroy of a locked mutex                                    │  │
│  │ • signal-unsafe call inside of a signal                       │  │
│  │ • use of an invalid mutex (e.g. uninitialized)                │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 2: Acceso actual (el que TSan acaba de ver)            │  │
│  │                                                                │  │
│  │   Write of size 4 at 0x7f... by thread T2:                     │  │
│  │     #0 increment src/counter.c:12:9 (program+0x...)            │  │
│  │     #1 __tsan_thread_start ...                                 │  │
│  │                                                                │  │
│  │ • Write/Read: tipo de acceso                                   │  │
│  │ • size 4: tamaño del acceso (4 bytes = int)                    │  │
│  │ • 0x7f...: dirección de memoria                                │  │
│  │ • thread T2: qué thread hizo este acceso                       │  │
│  │ • Stack trace: dónde en el código                              │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 3: Acceso previo (con el que conflicta)                │  │
│  │                                                                │  │
│  │   Previous write of size 4 at 0x7f... by thread T1:            │  │
│  │     #0 increment src/counter.c:12:9 (program+0x...)            │  │
│  │     #1 __tsan_thread_start ...                                 │  │
│  │                                                                │  │
│  │ • Previous write/read: el acceso anterior conflictivo          │  │
│  │ • Misma dirección (0x7f...)                                    │  │
│  │ • Diferente thread (T1 vs T2)                                  │  │
│  │ • Al menos uno es write                                        │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 4: Ubicación de la memoria                             │  │
│  │                                                                │  │
│  │   Location is global 'counter' of size 4 at 0x... (prog+0x0)  │  │
│  │                                                                │  │
│  │ Formatos posibles:                                             │  │
│  │ • "Location is global 'NAME' ..." → variable global           │  │
│  │ • "Location is heap block of size N ..." → malloc/new          │  │
│  │ • "Location is stack of thread T1" → variable local            │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 5: Creación de threads                                 │  │
│  │                                                                │  │
│  │   Thread T1 (tid=12346, running) created by main thread at:    │  │
│  │     #0 pthread_create ...                                      │  │
│  │     #1 main src/main.c:25:5                                    │  │
│  │                                                                │  │
│  │   Thread T2 (tid=12347, running) created by main thread at:    │  │
│  │     #0 pthread_create ...                                      │  │
│  │     #1 main src/main.c:26:5                                    │  │
│  │                                                                │  │
│  │ • Muestra DÓNDE se creó cada thread involucrado                │  │
│  │ • Útil para identificar qué rol cumple cada thread             │  │
│  └────────────────────────────────────────────────────────────────┘  │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────────┐  │
│  │ SECCIÓN 6: Resumen                                             │  │
│  │                                                                │  │
│  │ SUMMARY: ThreadSanitizer: data race src/counter.c:12 in incr   │  │
│  │ ThreadSanitizer: reported 1 warnings                           │  │
│  └────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 22. Leer el reporte: thread IDs y stack traces

### Correspondencia de thread IDs

```
┌─────────────────────────────────────────────────────────────────┐
│              Thread IDs en TSan                                   │
│                                                                  │
│  T0 (o "main thread"): el hilo principal (main())               │
│  T1: primer hilo creado con pthread_create                       │
│  T2: segundo hilo creado                                         │
│  ...                                                             │
│                                                                  │
│  Los IDs son SECUENCIALES por orden de creación, no por TID     │
│  del sistema operativo.                                          │
│                                                                  │
│  El reporte siempre incluye la sección "Thread TN created by"    │
│  para que puedas ver EXACTAMENTE dónde se creó cada thread.     │
│                                                                  │
│  Ejemplo de lectura:                                             │
│  "Write by thread T2" + "T2 created at main.c:26"               │
│  → El hilo creado en main.c:26 es el que escribe                │
│  → Miras tu código en main.c:26 para ver qué función usa T2    │
└─────────────────────────────────────────────────────────────────┘
```

### Ejemplo de reporte real con múltiples races

```
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x000001200000 by thread T2:
    #0 worker src/pool.c:45:16
    #1 __tsan_thread_start

  Previous read of size 4 at 0x000001200000 by thread T1:
    #0 coordinator src/pool.c:78:12
    #1 __tsan_thread_start

  Location is heap block of size 160 at 0x0000011fffa0
    allocated by main thread at:
    #0 malloc
    #1 pool_create src/pool.c:22:20
    #2 main src/main.c:10:18

  Thread T1 (tid=12346, running) created by main thread at:
    #0 pthread_create
    #1 pool_start src/pool.c:55:5
    #2 main src/main.c:12:5

  Thread T2 (tid=12347, running) created by main thread at:
    #0 pthread_create
    #1 pool_start src/pool.c:56:5
    #2 main src/main.c:12:5

SUMMARY: ThreadSanitizer: data race src/pool.c:45:16 in worker
==================
```

Lectura:
1. **Qué pasó**: T2 escribió 4 bytes, T1 leyó los mismos 4 bytes, sin sincronización
2. **Dónde**: pool.c:45 (write) y pool.c:78 (read), en un bloque de heap allocado en pool.c:22
3. **Quiénes**: T1 es `coordinator` (creado en pool.c:55), T2 es `worker` (creado en pool.c:56)
4. **Acción**: agregar lock en pool.c:45 y pool.c:78, o usar atomic

---

## 23. TSan en Rust: data races con unsafe

### Patrón 1: mutable static

```rust
// El data race más simple en Rust unsafe:
static mut COUNTER: u64 = 0;

fn main() {
    let handles: Vec<_> = (0..4).map(|_| {
        std::thread::spawn(|| {
            for _ in 0..100_000 {
                unsafe {
                    COUNTER += 1;  // Data race
                }
            }
        })
    }).collect();

    for h in handles {
        h.join().unwrap();
    }

    unsafe {
        println!("Counter: {}", COUNTER);
    }
}
```

### Patrón 2: Send/Sync implementado incorrectamente

```rust
use std::cell::UnsafeCell;
use std::sync::Arc;
use std::thread;

// Wrapper que dice "soy thread-safe" pero no lo es
struct UnsafeCounter {
    value: UnsafeCell<i32>,
}

// Bug: implementar Send+Sync sin sincronización real
unsafe impl Send for UnsafeCounter {}
unsafe impl Sync for UnsafeCounter {}

impl UnsafeCounter {
    fn new(v: i32) -> Self {
        UnsafeCounter { value: UnsafeCell::new(v) }
    }

    fn increment(&self) {
        unsafe {
            let ptr = self.value.get();
            *ptr += 1;  // Data race: no hay sincronización
        }
    }

    fn get(&self) -> i32 {
        unsafe { *self.value.get() }  // Data race: lee sin sincronización
    }
}

fn main() {
    let counter = Arc::new(UnsafeCounter::new(0));

    let handles: Vec<_> = (0..4).map(|_| {
        let c = Arc::clone(&counter);
        thread::spawn(move || {
            for _ in 0..100_000 {
                c.increment();  // Data race
            }
        })
    }).collect();

    for h in handles {
        h.join().unwrap();
    }

    println!("Counter: {}", counter.get());
}
```

```bash
$ RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly run --target x86_64-unknown-linux-gnu

==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x... by thread T2:
    #0 unsafe_counter::UnsafeCounter::increment src/main.rs:20:13

  Previous write of size 4 at 0x... by thread T1:
    #0 unsafe_counter::UnsafeCounter::increment src/main.rs:20:13
==================
```

### Corrección en Rust

```rust
use std::sync::atomic::{AtomicI32, Ordering};
use std::sync::Arc;

struct SafeCounter {
    value: AtomicI32,
}

impl SafeCounter {
    fn new(v: i32) -> Self {
        SafeCounter { value: AtomicI32::new(v) }
    }

    fn increment(&self) {
        self.value.fetch_add(1, Ordering::Relaxed);
    }

    fn get(&self) -> i32 {
        self.value.load(Ordering::Relaxed)
    }
}
// AtomicI32 ya implementa Send + Sync automáticamente
```

---

## 24. Data races imposibles en safe Rust

```
┌──────────────────────────────────────────────────────────────────────┐
│         ¿Por qué safe Rust no tiene data races?                      │
│                                                                      │
│  1. OWNERSHIP: un valor solo tiene un dueño a la vez               │
│     let x = vec![1, 2, 3];                                          │
│     let handle = thread::spawn(move || {                             │
│         println!("{:?}", x);  // x se mueve al thread               │
│     });                                                              │
│     // x ya no existe aquí → no hay acceso compartido               │
│                                                                      │
│  2. BORROWING: un mutable XOR múltiples inmutables                  │
│     let mut x = 5;                                                   │
│     let r1 = &x;     // OK: borrow inmutable                        │
│     let r2 = &x;     // OK: múltiples inmutables                    │
│     // let r3 = &mut x;  // ERROR: no puedes mutably borrow         │
│     //                    // mientras hay borrows inmutables          │
│                                                                      │
│  3. SEND trait: un tipo solo puede enviarse a otro thread si         │
│     implementa Send                                                  │
│     Rc<T> NO implementa Send → no puedes compartir Rc entre threads │
│     Arc<T> SÍ implementa Send → puedes compartir Arc entre threads  │
│                                                                      │
│  4. SYNC trait: un tipo solo puede ser accedido por múltiples        │
│     threads si implementa Sync                                       │
│     Cell<T> NO es Sync → no puedes tener &Cell en múltiples threads │
│     Mutex<T> SÍ es Sync → puedes tener &Mutex en múltiples threads  │
│                                                                      │
│  5. Mutex<T> CONTIENE el dato protegido:                             │
│     let m = Mutex::new(data);                                        │
│     let guard = m.lock().unwrap();  // Retorna MutexGuard            │
│     *guard = new_value;             // Solo accesible con guard      │
│     // guard se dropea → unlock automático                           │
│     // IMPOSIBLE acceder a data sin el lock                          │
│                                                                      │
│  Estas garantías son EN COMPILACIÓN. Zero overhead en runtime.       │
│  El compilador de Rust hace el trabajo de TSan, gratis.              │
└──────────────────────────────────────────────────────────────────────┘
```

### Tabla: cómo Rust previene cada tipo de data race

```
┌─────────────────────────────┬──────────────────────────────────────┐
│ Data race en C              │ Prevención en safe Rust               │
├─────────────────────────────┼──────────────────────────────────────┤
│ Variable global compartida  │ static mut requiere unsafe;           │
│                             │ Usar AtomicI32 o Mutex<i32>           │
├─────────────────────────────┼──────────────────────────────────────┤
│ Struct compartido sin lock  │ Mutex<T> contiene T, obligatorio lock │
├─────────────────────────────┼──────────────────────────────────────┤
│ Array compartido            │ &mut [T] no se puede compartir        │
│                             │ Usar Arc<Mutex<Vec<T>>>               │
├─────────────────────────────┼──────────────────────────────────────┤
│ Flag de terminación bool    │ AtomicBool o channel                  │
├─────────────────────────────┼──────────────────────────────────────┤
│ Publicar dato vía atomic    │ Rust compiler enforces correct        │
│ con ordering incorrecto     │ Ordering enum; no UB                  │
├─────────────────────────────┼──────────────────────────────────────┤
│ Lock incorrecto             │ Mutex<T> ata el dato al lock;         │
│ (proteger con lock equivoc.)│ imposible acceder con lock equivocado │
├─────────────────────────────┼──────────────────────────────────────┤
│ TOCTOU (check-then-act      │ MutexGuard mantiene el lock           │
│ sin lock)                   │ durante todo el acceso                │
└─────────────────────────────┴──────────────────────────────────────┘
```

---

## 25. Ejemplo completo en C: thread_pool

Un thread pool con múltiples data races:

```c
// thread_pool.c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

#define MAX_TASKS 256
#define NUM_WORKERS 4

typedef void (*TaskFunc)(void *);

typedef struct {
    TaskFunc func;
    void *arg;
} Task;

typedef struct {
    Task tasks[MAX_TASKS];
    int head;
    int tail;
    int count;
    int tasks_completed;     // Bug 1: acceso sin lock
    bool shutdown;           // Bug 2: flag sin sincronización
    pthread_mutex_t lock;
    pthread_cond_t not_empty;
    pthread_t workers[NUM_WORKERS];
} ThreadPool;

// ────────────────────────────────────────────────────────────────
// Bug 1: tasks_completed accedido sin lock desde múltiples workers
// ────────────────────────────────────────────────────────────────
void *worker_func(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;

    while (1) {
        // Bug 2: lee shutdown sin lock
        if (pool->shutdown) break;

        pthread_mutex_lock(&pool->lock);

        while (pool->count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->lock);
        }

        if (pool->shutdown && pool->count == 0) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }

        // Extraer tarea
        Task task = pool->tasks[pool->tail];
        pool->tail = (pool->tail + 1) % MAX_TASKS;
        pool->count--;

        pthread_mutex_unlock(&pool->lock);

        // Ejecutar tarea
        task.func(task.arg);

        // Bug 1: incrementar tasks_completed sin lock
        pool->tasks_completed++;  // Data race
    }

    return NULL;
}

ThreadPool *pool_create(void) {
    ThreadPool *pool = (ThreadPool *)calloc(1, sizeof(ThreadPool));
    pthread_mutex_init(&pool->lock, NULL);
    pthread_cond_init(&pool->not_empty, NULL);

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_create(&pool->workers[i], NULL, worker_func, pool);
    }

    return pool;
}

void pool_submit(ThreadPool *pool, TaskFunc func, void *arg) {
    pthread_mutex_lock(&pool->lock);

    if (pool->count < MAX_TASKS) {
        pool->tasks[pool->head].func = func;
        pool->tasks[pool->head].arg = arg;
        pool->head = (pool->head + 1) % MAX_TASKS;
        pool->count++;
        pthread_cond_signal(&pool->not_empty);
    }

    pthread_mutex_unlock(&pool->lock);
}

// ────────────────────────────────────────────────────────────────
// Bug 3: lee tasks_completed sin lock
// ────────────────────────────────────────────────────────────────
int pool_completed(ThreadPool *pool) {
    return pool->tasks_completed;  // Data race con workers
}

// ────────────────────────────────────────────────────────────────
// Bug 2: shutdown sin sincronización adecuada
// ────────────────────────────────────────────────────────────────
void pool_destroy(ThreadPool *pool) {
    // Bug 2: escribe shutdown sin lock
    pool->shutdown = true;

    // Despertar todos los workers (correcto, pero shutdown tiene race)
    pthread_cond_broadcast(&pool->not_empty);

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(pool->workers[i], NULL);
    }

    printf("Total tasks completed: %d\n", pool->tasks_completed);
    pthread_mutex_destroy(&pool->lock);
    pthread_cond_destroy(&pool->not_empty);
    free(pool);
}

// ── Funciones de tarea ─────────────────────────────────────────
void compute_task(void *arg) {
    int *val = (int *)arg;
    *val = (*val) * (*val);  // Cuadrado
}

int main() {
    ThreadPool *pool = pool_create();

    int values[100];
    for (int i = 0; i < 100; i++) {
        values[i] = i;
        pool_submit(pool, compute_task, &values[i]);
    }

    // Bug 3: polling sin lock
    while (pool_completed(pool) < 100) {
        usleep(1000);
    }

    pool_destroy(pool);

    // Verificar resultados
    for (int i = 0; i < 10; i++) {
        printf("values[%d] = %d\n", i, values[i]);
    }

    return 0;
}
```

### Resumen de bugs

```
┌─────┬──────────────────────────────────────┬────────────────────────┐
│ Bug │ Descripción                           │ TSan reportará         │
├─────┼──────────────────────────────────────┼────────────────────────┤
│  1  │ tasks_completed++ sin lock            │ Write/write race entre │
│     │ desde múltiples workers               │ workers                │
├─────┼──────────────────────────────────────┼────────────────────────┤
│  2  │ shutdown flag escrito sin lock,       │ Write (main) / Read    │
│     │ leído sin lock en workers             │ (worker) race          │
├─────┼──────────────────────────────────────┼────────────────────────┤
│  3  │ pool_completed() lee tasks_completed │ Read (main) / Write    │
│     │ sin lock, workers escriben            │ (worker) race          │
└─────┴──────────────────────────────────────┴────────────────────────┘
```

### Corrección

```c
// Bug 1 + 3: proteger tasks_completed con el mismo lock del pool
void *worker_func_safe(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;
    while (1) {
        pthread_mutex_lock(&pool->lock);
        while (pool->count == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->not_empty, &pool->lock);
        }
        if (pool->shutdown && pool->count == 0) {
            pthread_mutex_unlock(&pool->lock);
            break;
        }
        Task task = pool->tasks[pool->tail];
        pool->tail = (pool->tail + 1) % MAX_TASKS;
        pool->count--;
        pthread_mutex_unlock(&pool->lock);

        task.func(task.arg);

        pthread_mutex_lock(&pool->lock);
        pool->tasks_completed++;  // Ahora protegido
        pthread_mutex_unlock(&pool->lock);
    }
    return NULL;
}

int pool_completed_safe(ThreadPool *pool) {
    pthread_mutex_lock(&pool->lock);
    int c = pool->tasks_completed;
    pthread_mutex_unlock(&pool->lock);
    return c;
}

// Bug 2: proteger shutdown con lock
void pool_destroy_safe(ThreadPool *pool) {
    pthread_mutex_lock(&pool->lock);
    pool->shutdown = true;
    pthread_cond_broadcast(&pool->not_empty);
    pthread_mutex_unlock(&pool->lock);

    for (int i = 0; i < NUM_WORKERS; i++) {
        pthread_join(pool->workers[i], NULL);
    }
    // ... cleanup
}
```

---

## 26. Ejemplo completo en Rust unsafe: shared_counter

```rust
// src/main.rs
use std::alloc::{alloc_zeroed, dealloc, Layout};
use std::ptr;
use std::thread;
use std::sync::Arc;

/// Contador compartido implementado con raw pointers
/// (intencionalmente buggy para demostrar TSan)
struct SharedCounter {
    count: *mut i64,
    max: i64,
}

// Bug: implementamos Send y Sync sin sincronización real
unsafe impl Send for SharedCounter {}
unsafe impl Sync for SharedCounter {}

impl SharedCounter {
    fn new(max: i64) -> Self {
        let layout = Layout::new::<i64>();
        let count = unsafe { alloc_zeroed(layout) as *mut i64 };
        SharedCounter { count, max }
    }

    fn increment(&self) -> bool {
        unsafe {
            let current = ptr::read_volatile(self.count);
            if current >= self.max {
                return false;
            }
            // Bug: read-modify-write sin sincronización
            // Otro thread puede leer el mismo current
            ptr::write_volatile(self.count, current + 1);
            true
        }
    }

    fn get(&self) -> i64 {
        unsafe {
            // Bug: lectura sin sincronización
            ptr::read_volatile(self.count)
        }
    }

    fn reset(&self) {
        unsafe {
            // Bug: escritura sin sincronización
            ptr::write_volatile(self.count, 0);
        }
    }
}

impl Drop for SharedCounter {
    fn drop(&mut self) {
        unsafe {
            dealloc(self.count as *mut u8, Layout::new::<i64>());
        }
    }
}

fn main() {
    let counter = Arc::new(SharedCounter::new(1_000_000));

    let handles: Vec<_> = (0..4).map(|id| {
        let c = Arc::clone(&counter);
        thread::spawn(move || {
            let mut local_count = 0u64;
            while c.increment() {
                local_count += 1;
            }
            println!("Thread {}: did {} increments", id, local_count);
        })
    }).collect();

    for h in handles {
        h.join().unwrap();
    }

    println!("Final count: {}", counter.get());
    // Debería ser max (1_000_000), pero por data races puede ser menor
}
```

```bash
$ RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly run --target x86_64-unknown-linux-gnu

==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x... by thread T3:
    #0 shared_counter::SharedCounter::increment src/main.rs:31:13

  Previous read of size 8 at 0x... by thread T2:
    #0 shared_counter::SharedCounter::increment src/main.rs:27:27
==================
```

### Corrección

```rust
use std::sync::atomic::{AtomicI64, Ordering};
use std::sync::Arc;

struct SafeCounter {
    count: AtomicI64,
    max: i64,
}

impl SafeCounter {
    fn new(max: i64) -> Self {
        SafeCounter {
            count: AtomicI64::new(0),
            max,
        }
    }

    fn increment(&self) -> bool {
        loop {
            let current = self.count.load(Ordering::Relaxed);
            if current >= self.max {
                return false;
            }
            // CAS: solo incrementa si nadie cambió el valor
            match self.count.compare_exchange_weak(
                current, current + 1,
                Ordering::Relaxed, Ordering::Relaxed
            ) {
                Ok(_) => return true,
                Err(_) => continue,  // Otro thread ganó, reintentar
            }
        }
    }

    fn get(&self) -> i64 {
        self.count.load(Ordering::Relaxed)
    }
}
```

---

## 27. TSan con tests unitarios

### En C

```bash
# Compilar tests con TSan:
clang -fsanitize=thread -fno-omit-frame-pointer -g -O1 -pthread \
      tests/test_pool.c src/pool.c -o test_pool

# Ejecutar:
./test_pool
# TSan reporta en el primer data race que detecte
```

### En Rust

```bash
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly test --target x86_64-unknown-linux-gnu
```

### Estructura de test para detectar data races

```c
// tests/test_counter.c
#include <assert.h>
#include <pthread.h>

#define NUM_THREADS 8
#define ITERATIONS 100000

// Test que DEBERÍA activar TSan si hay data race:
void test_concurrent_increment(void) {
    counter = 0;
    pthread_t threads[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_create(&threads[i], NULL, increment, NULL);
    }
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Con data race, counter probablemente < NUM_THREADS * ITERATIONS
    // Pero TSan reporta el race ANTES de llegar aquí
    assert(counter == NUM_THREADS * ITERATIONS);
}
```

**Nota sobre la estrategia de testing**: no necesitas ejecutar el test muchas veces para detectar data races. TSan detecta el race en la **primera** ejecución si el scheduling lo permite. A diferencia de testing manual (donde necesitas millones de ejecuciones para reproducir), TSan es determinístico en su detección.

---

## 28. TSan con fuzzing

TSan con fuzzing es posible pero tiene restricciones significativas:

```
┌─────────────────────────────────────────────────────────────────┐
│           TSan + fuzzing: consideraciones                        │
│                                                                  │
│  PROBLEMAS:                                                      │
│  1. TSan tiene ~10x overhead → fuzzing 10x más lento            │
│  2. TSan usa ~8x más RAM → límite de RAM para corpus            │
│  3. TSan + ASan son INCOMPATIBLES                                │
│     No puedes buscar data races y memory bugs al mismo tiempo   │
│  4. libFuzzer tiene su propio threading                          │
│     Puede causar falsos positivos con TSan                       │
│  5. La mayoría de harnesses de fuzzing son single-threaded       │
│     TSan no tiene mucho que hacer                                │
│                                                                  │
│  ¿CUÁNDO tiene sentido?                                          │
│  • Fuzzear una API thread-safe para encontrar data races         │
│  • El harness crea múltiples threads y los ejercita              │
│  • Ejemplo: fuzzear un concurrent hash map                       │
│                                                                  │
│  ALTERNATIVA RECOMENDADA:                                        │
│  1. Fuzzear con ASan+UBSan (buscar memory bugs)                 │
│  2. Tests unitarios multi-threaded con TSan (buscar data races)  │
│  3. Usar el corpus de fuzzing como input para tests TSan         │
└─────────────────────────────────────────────────────────────────┘
```

### Ejemplo: fuzzear una API thread-safe

```c
// fuzz_concurrent.c
#include <stdint.h>
#include <stddef.h>
#include <pthread.h>
#include "concurrent_map.h"

#define NUM_THREADS 4

typedef struct {
    ConcurrentMap *map;
    const uint8_t *ops;
    size_t ops_len;
} ThreadArg;

void *worker(void *arg) {
    ThreadArg *ta = (ThreadArg *)arg;
    for (size_t i = 0; i < ta->ops_len; i++) {
        uint8_t op = ta->ops[i];
        int key = op & 0x0F;
        int val = (op >> 4) & 0x0F;

        if (op & 0x80) {
            map_put(ta->map, key, val);
        } else {
            map_get(ta->map, key);
        }
    }
    return NULL;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if (size < NUM_THREADS) return 0;

    ConcurrentMap *map = map_create(64);

    size_t per_thread = size / NUM_THREADS;
    pthread_t threads[NUM_THREADS];
    ThreadArg args[NUM_THREADS];

    for (int i = 0; i < NUM_THREADS; i++) {
        args[i].map = map;
        args[i].ops = data + i * per_thread;
        args[i].ops_len = per_thread;
        pthread_create(&threads[i], NULL, worker, &args[i]);
    }

    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    map_destroy(map);
    return 0;
}
```

```bash
# Compilar con TSan + fuzzer (SIN ASan):
clang -fsanitize=fuzzer,thread -g -O1 -pthread \
      fuzz_concurrent.c src/concurrent_map.c -o fuzz_concurrent

# Ejecutar:
./fuzz_concurrent -max_len=256 -max_total_time=120
```

---

## 29. TSAN_OPTIONS: configuración completa

```bash
# Sintaxis:
TSAN_OPTIONS="option1=value1:option2=value2" ./program
```

### Opciones principales

```
┌─────────────────────────────────┬──────────┬──────────────────────────────────┐
│ Opción                          │ Default  │ Descripción                       │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ halt_on_error                   │ 0        │ 1=abortar en primer error        │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ second_deadlock_stack           │ 0        │ 1=mostrar stack de segundo lock  │
│                                 │          │ en detección de deadlock         │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ report_thread_leaks             │ 1        │ 1=reportar threads no joined     │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ report_destroy_locked           │ 1        │ 1=reportar destrucción de mutex  │
│                                 │          │ locked                           │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ report_signal_unsafe            │ 1        │ 1=reportar llamadas no-signal-   │
│                                 │          │ safe en handlers de señal        │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ detect_deadlocks                │ 1        │ 1=detectar potenciales deadlocks │
│                                 │          │ (lock order inversion)           │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ history_size                    │ 3        │ Tamaño del historial de accesos  │
│                                 │          │ por thread (2^N entradas)        │
│                                 │          │ Mayor = más memoria pero mejor   │
│                                 │          │ detección de races antiguos      │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ io_sync                         │ 1        │ 1=sincronizar operaciones de I/O │
│                                 │          │ (reduce falsos positivos con     │
│                                 │          │ printf/write compartidos)        │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ force_seq_cst_atomics           │ 0        │ 1=tratar todos los atomics como  │
│                                 │          │ seq_cst (detecta ordering bugs)  │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ exitcode                        │ 66       │ Código de salida en error        │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ symbolize                       │ 1        │ 1=resolver símbolos              │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ log_path                        │ stderr   │ Archivo para reportes            │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ suppressions                    │ ""       │ Archivo de suppressions          │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ print_suppressions              │ 0        │ 1=imprimir suppressions usadas   │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ verbosity                       │ 0        │ Nivel de verbosidad (0-3)        │
├─────────────────────────────────┼──────────┼──────────────────────────────────┤
│ strip_path_prefix               │ ""       │ Prefijo a eliminar de paths      │
└─────────────────────────────────┴──────────┴──────────────────────────────────┘
```

### Ejemplos de uso

```bash
# Detener en primer error (para CI):
TSAN_OPTIONS="halt_on_error=1" ./program

# Historial grande para races difíciles de detectar:
TSAN_OPTIONS="history_size=7" ./program
# 2^7 = 128 entradas de historial por thread (más RAM)

# Detección extra de ordering bugs en atomics:
TSAN_OPTIONS="force_seq_cst_atomics=1" ./program

# Log a archivo:
TSAN_OPTIONS="log_path=/tmp/tsan.log" ./program

# Debugging verbose:
TSAN_OPTIONS="verbosity=2:second_deadlock_stack=1" ./program

# Para CI:
TSAN_OPTIONS="halt_on_error=1:history_size=4:exitcode=1" ./tests
```

---

## 30. Impacto en rendimiento detallado

```
┌──────────────────────────────────────────────────────────────────────┐
│              Impacto de TSan en rendimiento                           │
│                                                                      │
│  Recurso          │ Sin TSan  │ TSan default │ TSan history=7         │
│  ─────────────────┼───────────┼──────────────┼────────────────        │
│  CPU (tiempo)     │ 1x        │ ~5-15x       │ ~10-20x               │
│  RAM              │ 1x        │ ~5-10x       │ ~8-15x                │
│  Tamaño binario   │ 1x        │ ~2-3x        │ ~2-3x                 │
│  Velocidad build  │ 1x        │ ~1.3x        │ ~1.3x                 │
│                                                                      │
│  ¿Por qué es TAN lento?                                              │
│                                                                      │
│  1. SHADOW MEMORY PESADO                                             │
│     4 shadow words (32 bytes) por cada 8 bytes de app               │
│     → Cada load/store toca shadow memory = más cache pressure       │
│                                                                      │
│  2. VECTOR CLOCKS                                                    │
│     Comparar vector clocks en cada acceso: O(N) donde N = threads   │
│     Con 8 threads: 8 comparaciones por acceso                       │
│                                                                      │
│  3. INTERCEPTORES PESADOS                                            │
│     Cada pthread_mutex_lock/unlock actualiza vector clocks          │
│     Overhead significativo por cada operación de sincronización     │
│                                                                      │
│  4. CACHE THRASHING                                                  │
│     Shadow memory es ~5x más grande que app memory                  │
│     → Datos de app son expulsados del cache constantemente          │
│                                                                      │
│  Comparación entre sanitizers:                                       │
│                                                                      │
│  UBSan:  CPU ~1.2x,   RAM ~1x     (el más ligero)                   │
│  ASan:   CPU ~2x,     RAM ~2-3x                                     │
│  MSan:   CPU ~3-5x,   RAM ~2-3x                                     │
│  TSan:   CPU ~5-15x,  RAM ~5-10x  (el más pesado)                   │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 31. Falsos positivos en TSan

TSan tiene una tasa de falsos positivos **muy baja** — prácticamente cero. Esto es por diseño: el algoritmo happens-before es conservador.

```
┌──────────────────────────────────────────────────────────────────────┐
│             Falsos positivos en TSan (raros)                         │
│                                                                      │
│  1. BIBLIOTECAS NO INSTRUMENTADAS                                    │
│     Si una biblioteca hace sincronización interna que TSan           │
│     no ve (ej: spinlock custom en assembly), TSan puede creer       │
│     que no hay sincronización.                                       │
│     Solución: recompilar la biblioteca con TSan                      │
│              o usar annotations                                      │
│                                                                      │
│  2. CUSTOM SYNCHRONIZATION PRIMITIVES                                │
│     Si usas tu propia implementación de locks/barriers con          │
│     atomic operations y memory fences, TSan puede no                 │
│     reconocer la sincronización.                                     │
│     Solución: usar TSan annotations:                                 │
│     __tsan_acquire(addr) / __tsan_release(addr)                      │
│                                                                      │
│  3. MEMORY-MAPPED I/O                                                │
│     Regiones compartidas entre procesos (no threads) vía mmap       │
│     TSan no sabe que otro proceso sincroniza vía la memoria          │
│     Solución: __tsan_acquire/__tsan_release manual                   │
│                                                                      │
│  4. SIGNAL HANDLERS (edge case)                                      │
│     TSan puede reportar races entre signal handlers y código         │
│     normal, incluso si el signal handler solo escribe a              │
│     sig_atomic_t (que es seguro por el estándar).                    │
│     Solución: supresión o usar __tsan annotations                    │
│                                                                      │
│  REGLA PRÁCTICA: si TSan reporta un data race, es casi              │
│  seguro que es un bug REAL. Investiga antes de suprimirlo.          │
└──────────────────────────────────────────────────────────────────────┘
```

### TSan annotations para sincronización custom

```c
// Para informar a TSan de sincronización que no reconoce:
#include <sanitizer/tsan_interface.h>

// Tu primitiva de sincronización custom:
void my_lock(my_mutex_t *m) {
    // ... tu implementación con atomics ...
    __tsan_acquire(m);  // Informar a TSan: "adquirí el lock"
}

void my_unlock(my_mutex_t *m) {
    __tsan_release(m);  // Informar a TSan: "liberé el lock"
    // ... tu implementación con atomics ...
}
```

---

## 32. Suppressions en TSan

Cuando hay un falso positivo (raro) o un data race conocido que no puedes corregir todavía, puedes suprimirlo:

### Archivo de suppressions

```
# tsan_suppressions.txt

# Suprimir race en función específica:
race:name_of_function

# Suprimir race en archivo:
race:filename.c

# Suprimir race en biblioteca:
race:libexternal.so

# Suprimir deadlock:
deadlock:function_with_known_safe_lock_order

# Suprimir thread leak:
thread:function_that_creates_daemon_threads

# Suprimir signal handler:
signal:handler_function
```

### Usar suppressions

```bash
# Vía variable de entorno:
TSAN_OPTIONS="suppressions=/path/to/tsan_suppressions.txt" ./program

# Verificar qué suppressions se usaron:
TSAN_OPTIONS="suppressions=supp.txt:print_suppressions=1" ./program
```

### Suprimir en código (annotations)

```c
// Marcar un acceso como "no es data race" (ignorar):
#include <sanitizer/tsan_interface.h>

void benign_race_function(void) {
    // TSan ignorará accesos a *ptr en este punto
    __tsan_benign_race(&shared_var, sizeof(shared_var));
    shared_var = new_value;  // TSan no reportará race aquí
}

// Alternativa: atributo de función
__attribute__((no_sanitize("thread")))
void function_with_known_race(void) {
    // Todo en esta función es ignorado por TSan
}
```

---

## 33. TSan vs Helgrind (Valgrind)

```
┌──────────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto                  │ TSan                 │ Helgrind (Valgrind)  │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Mecanismo                │ Instrumentación en   │ Emulación de CPU     │
│                          │ compilación          │ (binary translation) │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Velocidad                │ ~5-15x overhead      │ ~50-100x overhead    │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ RAM overhead             │ ~5-10x               │ ~10-30x              │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Requiere recompilar      │ SÍ                   │ NO                   │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Compilador requerido     │ Clang o GCC          │ Cualquiera           │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Algoritmo                │ Happens-before       │ Happens-before +     │
│                          │ (vector clocks)      │ lock-set             │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Falsos positivos         │ Muy pocos            │ Más (lock-set puede  │
│                          │                      │ ser conservador)     │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Detección de deadlocks   │ SÍ (lock ordering)   │ SÍ (más completo)   │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Plataformas              │ Linux x86_64,        │ Linux, macOS         │
│                          │ macOS (parcial)      │ (más plataformas)    │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ C11 atomics              │ Sí (entiende orderin)│ Limitado             │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ En CI                    │ Viable (~10x)        │ Muy lento (~100x)    │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Soporte Go               │ Sí (go -race)        │ No                   │
├──────────────────────────┼──────────────────────┼──────────────────────┤
│ Soporte Rust             │ Sí (nightly)         │ Sí (sin fuente)      │
└──────────────────────────┴──────────────────────┴──────────────────────┘
```

### ¿Cuándo usar cada uno?

```
┌─────────────────────────────────────────────────────────────────┐
│  Usa TSan cuando:                                                │
│  • Puedes recompilar todo el código                              │
│  • Necesitas integrar en CI (overhead aceptable ~10x)            │
│  • Usas C11 atomics y necesitas verificar ordering               │
│  • Estás en Linux x86_64                                         │
│                                                                  │
│  Usa Helgrind cuando:                                            │
│  • No puedes recompilar (binario cerrado)                        │
│  • Necesitas análisis puntual de un programa                     │
│  • Estás en una plataforma que TSan no soporta bien              │
│  • Necesitas detección de deadlocks más completa                 │
│                                                                  │
│  Usa Go -race cuando:                                            │
│  • Programa en Go (integrado, cero configuración)                │
│  • go test -race ejecuta todos los tests con detección           │
└─────────────────────────────────────────────────────────────────┘
```

---

## 34. Limitaciones de TSan

```
┌──────────────────────────────────────────────────────────────────────┐
│                   Limitaciones de TSan                                │
│                                                                      │
│  1. OVERHEAD ALTO                                                    │
│     ~5-15x CPU, ~5-10x RAM. Prohibitivo para producción.            │
│     No viable para benchmarks o tests de rendimiento.                │
│                                                                      │
│  2. INCOMPATIBLE CON ASan Y MSan                                     │
│     No puedes buscar data races y memory bugs al mismo tiempo.       │
│     Necesitas builds separados:                                      │
│     Build 1: ASan + UBSan (memory + UB)                              │
│     Build 2: TSan + UBSan (races + UB)                               │
│     Build 3: MSan + UBSan (uninit + UB)                              │
│                                                                      │
│  3. LÍMITE DE THREADS                                                │
│     TSan soporta ~8192 threads simultáneos.                          │
│     Si tu programa crea más, TSan puede crashear o dar               │
│     falsos negativos.                                                │
│                                                                      │
│  4. NO DETECTA RACE CONDITIONS LÓGICAS                               │
│     Solo detecta data races (acceso sin sincronización).             │
│     Race conditions con locks correctos no se detectan.              │
│     Ejemplo: TOCTOU con locks → no es data race.                    │
│                                                                      │
│  5. NO DETECTA ABA PROBLEMS                                         │
│     En código lock-free, ABA problems son bugs sutiles donde        │
│     un valor atómico cambia de A→B→A. TSan no los detecta.          │
│                                                                      │
│  6. HISTORY WINDOW LIMITADO                                          │
│     TSan solo recuerda los últimos N accesos por thread.             │
│     Data races entre accesos muy separados en el tiempo pueden      │
│     no detectarse. Aumentar history_size ayuda pero consume RAM.     │
│                                                                      │
│  7. SOLO LINUX x86_64 (completo)                                     │
│     Soporte parcial en macOS (funciona pero con limitaciones).       │
│     No funciona en Windows.                                          │
│     No funciona en ARM (TSan v2 solo soporta x86_64).               │
│                                                                      │
│  8. NO DETECTA RACES EN INLINE ASSEMBLY                              │
│     Accesos a memoria en inline asm no son instrumentados.           │
│     Solución: __tsan annotations alrededor del asm.                  │
│                                                                      │
│  9. VIRTUAL MEMORY REQUIREMENT                                       │
│     TSan necesita reservar grandes regiones de address space         │
│     para shadow memory. Puede conflictar con programas que           │
│     usan mmap extensivamente.                                        │
│                                                                      │
│  10. NO GARANTIZA ENCONTRAR TODOS LOS DATA RACES                    │
│      TSan es best-effort: depende del scheduling que ocurra          │
│      durante la ejecución. Si un race solo ocurre con un             │
│      scheduling específico que no se dio, TSan no lo ve.             │
│      Pero en la práctica, find rate es > 95% con tests              │
│      que ejercitan los paths concurrentes.                           │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 35. Errores comunes

### Error 1: mezclar TSan con ASan

```bash
# ERROR:
clang -fsanitize=thread,address program.c
# error: invalid argument '-fsanitize=address' not allowed with
#        '-fsanitize=thread'

# SOLUCIÓN: builds separados
# Build 1: ASan + UBSan
clang -fsanitize=address,undefined program.c -o prog_asan
# Build 2: TSan + UBSan
clang -fsanitize=thread,undefined program.c -o prog_tsan
```

### Error 2: olvidar -pthread

```bash
# ERROR: compilar sin -pthread
clang -fsanitize=thread program.c -o program
# Warning o error en linkeo si usas pthreads

# SOLUCIÓN:
clang -fsanitize=thread -pthread program.c -o program
```

### Error 3: compilar parcialmente con TSan

```bash
# ERROR: un archivo sin TSan
clang -c utils.c -o utils.o  # Sin TSan
clang -fsanitize=thread -pthread main.c utils.o -o program
# TSan no ve accesos en utils.c → falsos negativos

# SOLUCIÓN: TODO con TSan
clang -fsanitize=thread -c utils.c -o utils.o
clang -fsanitize=thread -pthread main.c utils.o -o program
```

### Error 4: creer que volatile = thread-safe

```c
// ERROR CONCEPTUAL: volatile NO es sincronización
volatile int flag = 0;  // volatile ≠ atomic

void *writer(void *arg) {
    flag = 1;  // SIGUE siendo data race (volatile no sincroniza)
    return NULL;
}

void *reader(void *arg) {
    while (!flag) {}  // SIGUE siendo data race
    return NULL;
}

// TSan reportará data race en flag.
// volatile solo previene que el compilador optimice away el acceso,
// NO proporciona ordering ni atomicidad.
```

```
┌─────────────────────────────────────────────────────────────────┐
│         volatile vs atomic vs mutex                              │
│                                                                  │
│  volatile:                                                       │
│    Garantiza: compilador no optimiza away el acceso              │
│    NO garantiza: atomicidad, ordering, visibilidad entre cores   │
│    Uso legítimo: memory-mapped I/O, signal handlers              │
│    Para threading: INÚTIL                                        │
│                                                                  │
│  _Atomic / atomic_*:                                             │
│    Garantiza: atomicidad + ordering configurable                 │
│    SÍ garantiza: visibilidad entre cores (con acquire/release)   │
│    Uso: flags, contadores, publicación de datos                  │
│    Para threading: CORRECTO para operaciones simples             │
│                                                                  │
│  pthread_mutex:                                                  │
│    Garantiza: exclusión mutua + full memory barrier              │
│    SÍ garantiza: todo dentro del lock es visible al next holder  │
│    Uso: operaciones compuestas (read-modify-write de structs)    │
│    Para threading: CORRECTO para operaciones complejas           │
└─────────────────────────────────────────────────────────────────┘
```

### Error 5: no usar halt_on_error en CI

```bash
# PROBLEMA: TSan reporta pero no falla el test
$ ./test_concurrent
# WARNING: ThreadSanitizer: data race ...
$ echo $?
# 0  ← CI piensa que todo está bien

# SOLUCIÓN:
TSAN_OPTIONS="halt_on_error=1:exitcode=1" ./test_concurrent
$ echo $?
# 1  ← CI detecta el fallo
```

### Error 6: suponer que "funciona en mi máquina" = no hay data races

```
┌─────────────────────────────────────────────────────────────────┐
│  "Mi programa corre 10000 veces sin problemas, no hay data      │
│   races" — FALSO                                                 │
│                                                                  │
│  Un data race puede:                                             │
│  • Manifestarse solo bajo carga alta                             │
│  • Depender del scheduling del OS (varía entre ejecuciones)      │
│  • Solo aparecer en CPUs con modelos de memoria débiles (ARM)    │
│  • Nunca crashear pero corromper datos silenciosamente            │
│                                                                  │
│  TSan lo encontraría en la PRIMERA ejecución.                    │
│                                                                  │
│  "Ejecutar muchas veces" NO es una estrategia válida para        │
│  verificar ausencia de data races. Usar TSan sí lo es.           │
└─────────────────────────────────────────────────────────────────┘
```

---

## 36. Programa de práctica: concurrent_map

### Descripción

Un hash map concurrente con múltiples data races. Soporta get, put, delete, y iteration.

### Implementación en C con bugs

```c
// concurrent_map.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdbool.h>

#define MAP_SIZE 64
#define MAX_KEY_LEN 32

// ═══════════════════════════════════════════════════════════════
//  Hash map concurrente con bugs de data races
// ═══════════════════════════════════════════════════════════════

typedef struct Entry {
    char key[MAX_KEY_LEN];
    int value;
    bool occupied;
    struct Entry *next;  // Para chaining
} Entry;

typedef struct {
    Entry *buckets[MAP_SIZE];
    int count;                    // Bug 1: sin sincronización
    pthread_mutex_t locks[MAP_SIZE]; // Un lock por bucket
    bool iterating;               // Bug 2: flag sin sincronización
} ConcurrentMap;

// ────────────────────────────────────────────────────────────────
// Hash function
// ────────────────────────────────────────────────────────────────
static unsigned int hash(const char *key) {
    unsigned int h = 0;
    while (*key) {
        h = h * 31 + (unsigned char)(*key);
        key++;
    }
    return h % MAP_SIZE;
}

ConcurrentMap *map_create(void) {
    ConcurrentMap *m = (ConcurrentMap *)calloc(1, sizeof(ConcurrentMap));
    for (int i = 0; i < MAP_SIZE; i++) {
        pthread_mutex_init(&m->locks[i], NULL);
    }
    return m;
}

// ────────────────────────────────────────────────────────────────
// Bug 1: count modificado sin lock global
// (cada bucket tiene su propio lock, pero count es global)
// ────────────────────────────────────────────────────────────────
void map_put(ConcurrentMap *m, const char *key, int value) {
    unsigned int idx = hash(key);

    pthread_mutex_lock(&m->locks[idx]);

    // Buscar si ya existe
    Entry *curr = m->buckets[idx];
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            curr->value = value;  // Actualizar
            pthread_mutex_unlock(&m->locks[idx]);
            return;
        }
        curr = curr->next;
    }

    // Crear nueva entry
    Entry *e = (Entry *)malloc(sizeof(Entry));
    strncpy(e->key, key, MAX_KEY_LEN - 1);
    e->key[MAX_KEY_LEN - 1] = '\0';
    e->value = value;
    e->occupied = true;
    e->next = m->buckets[idx];
    m->buckets[idx] = e;

    pthread_mutex_unlock(&m->locks[idx]);

    // Bug 1: count++ sin lock
    // Otro thread puede estar leyendo o modificando count
    m->count++;  // Data race
}

// ────────────────────────────────────────────────────────────────
// Bug 2: get no toma lock
// ────────────────────────────────────────────────────────────────
int map_get(ConcurrentMap *m, const char *key, int *value) {
    unsigned int idx = hash(key);

    // Bug 2: lee buckets sin lock
    // Otro thread puede estar modificando este bucket
    Entry *curr = m->buckets[idx];
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            *value = curr->value;  // Data race con map_put
            return 1;  // Encontrado
        }
        curr = curr->next;  // Data race: curr->next puede cambiar
    }
    return 0;  // No encontrado
}

// ────────────────────────────────────────────────────────────────
// Bug 3: delete modifica count sin lock
// ────────────────────────────────────────────────────────────────
int map_delete(ConcurrentMap *m, const char *key) {
    unsigned int idx = hash(key);

    pthread_mutex_lock(&m->locks[idx]);

    Entry *prev = NULL;
    Entry *curr = m->buckets[idx];

    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            if (prev) {
                prev->next = curr->next;
            } else {
                m->buckets[idx] = curr->next;
            }
            free(curr);
            pthread_mutex_unlock(&m->locks[idx]);

            // Bug 3: count-- sin lock global
            m->count--;  // Data race
            return 1;
        }
        prev = curr;
        curr = curr->next;
    }

    pthread_mutex_unlock(&m->locks[idx]);
    return 0;
}

// ────────────────────────────────────────────────────────────────
// Bug 4: iteración sin locks (lee TODOS los buckets)
// ────────────────────────────────────────────────────────────────
void map_foreach(ConcurrentMap *m,
                 void (*callback)(const char *key, int value, void *ctx),
                 void *ctx) {
    // Bug 4a: modifica iterating sin lock
    m->iterating = true;  // Data race

    for (int i = 0; i < MAP_SIZE; i++) {
        // Bug 4b: no toma lock del bucket
        Entry *curr = m->buckets[i];  // Data race con put/delete
        while (curr) {
            callback(curr->key, curr->value, ctx);
            curr = curr->next;  // Data race: next puede haber cambiado
        }
    }

    m->iterating = false;  // Data race
}

// ────────────────────────────────────────────────────────────────
// Bug 5: map_size lee count sin sincronización
// ────────────────────────────────────────────────────────────────
int map_size(ConcurrentMap *m) {
    return m->count;  // Data race con put/delete
}

void map_destroy(ConcurrentMap *m) {
    for (int i = 0; i < MAP_SIZE; i++) {
        Entry *curr = m->buckets[i];
        while (curr) {
            Entry *next = curr->next;
            free(curr);
            curr = next;
        }
        pthread_mutex_destroy(&m->locks[i]);
    }
    free(m);
}

// ────────────────────────────────────────────────────────────────
// Funciones de test
// ────────────────────────────────────────────────────────────────

void *writer_thread(void *arg) {
    ConcurrentMap *m = (ConcurrentMap *)arg;
    char key[32];
    for (int i = 0; i < 1000; i++) {
        snprintf(key, 32, "key_%d", i);
        map_put(m, key, i * 10);
    }
    return NULL;
}

void *reader_thread(void *arg) {
    ConcurrentMap *m = (ConcurrentMap *)arg;
    char key[32];
    for (int i = 0; i < 1000; i++) {
        snprintf(key, 32, "key_%d", i);
        int value;
        map_get(m, key, &value);
    }
    return NULL;
}

void *deleter_thread(void *arg) {
    ConcurrentMap *m = (ConcurrentMap *)arg;
    char key[32];
    for (int i = 0; i < 500; i++) {
        snprintf(key, 32, "key_%d", i * 2);  // Borrar pares
        map_delete(m, key);
    }
    return NULL;
}

void print_callback(const char *key, int value, void *ctx) {
    int *count = (int *)ctx;
    (*count)++;
}

void *iterator_thread(void *arg) {
    ConcurrentMap *m = (ConcurrentMap *)arg;
    for (int i = 0; i < 10; i++) {
        int count = 0;
        map_foreach(m, print_callback, &count);
    }
    return NULL;
}

int main() {
    ConcurrentMap *m = map_create();

    pthread_t writers[2], readers[2], deleter, iterator;

    // Lanzar todos los threads simultáneamente
    pthread_create(&writers[0], NULL, writer_thread, m);
    pthread_create(&writers[1], NULL, writer_thread, m);
    pthread_create(&readers[0], NULL, reader_thread, m);
    pthread_create(&readers[1], NULL, reader_thread, m);
    pthread_create(&deleter, NULL, deleter_thread, m);
    pthread_create(&iterator, NULL, iterator_thread, m);

    // Esperar a todos
    pthread_join(writers[0], NULL);
    pthread_join(writers[1], NULL);
    pthread_join(readers[0], NULL);
    pthread_join(readers[1], NULL);
    pthread_join(deleter, NULL);
    pthread_join(iterator, NULL);

    printf("Final size: %d\n", map_size(m));

    map_destroy(m);
    return 0;
}
```

### Resumen de bugs

```
┌─────┬──────────────────────────────────────┬─────────────────────────────┐
│ Bug │ Descripción                           │ TSan reportará              │
├─────┼──────────────────────────────────────┼─────────────────────────────┤
│  1  │ m->count++ y m->count-- sin lock     │ Write/write race en count  │
│     │ global (cada bucket tiene su lock     │ entre writers y deleters    │
│     │ pero count es transversal)            │                             │
├─────┼──────────────────────────────────────┼─────────────────────────────┤
│  2  │ map_get() no toma lock del bucket    │ Read (get) / Write (put)    │
│     │                                      │ race en entries del bucket  │
├─────┼──────────────────────────────────────┼─────────────────────────────┤
│  3  │ map_delete() modifica count sin      │ Write/write race en count   │
│     │ lock global (mismo que bug 1)        │                             │
├─────┼──────────────────────────────────────┼─────────────────────────────┤
│  4  │ map_foreach() itera sin locks        │ Read (foreach) / Write      │
│     │ en ningún bucket ni en iterating     │ (put/delete) races          │
├─────┼──────────────────────────────────────┼─────────────────────────────┤
│  5  │ map_size() lee count sin lock        │ Read (size) / Write (put/   │
│     │                                      │ delete) race en count       │
└─────┴──────────────────────────────────────┴─────────────────────────────┘
```

### Compilar y ejecutar

```bash
$ clang -fsanitize=thread -fno-omit-frame-pointer -g -O1 -pthread \
        concurrent_map.c -o concurrent_map
$ ./concurrent_map

==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 4 at 0x... by thread T1:
    #0 map_put concurrent_map.c:80:5

  Previous read of size 4 at 0x... by thread T3:
    #0 map_get concurrent_map.c:91:26

  Location is global 'count' of ConcurrentMap at 0x...
==================
```

### Corrección

```c
// Añadir un lock global para count:
typedef struct {
    // ... igual
    pthread_mutex_t count_lock;  // Lock separado para count
    // ... igual
} ConcurrentMap;

// Bug 1+3+5: proteger count
void map_put_safe(ConcurrentMap *m, const char *key, int value) {
    unsigned int idx = hash(key);
    pthread_mutex_lock(&m->locks[idx]);
    // ... insertar ...
    pthread_mutex_unlock(&m->locks[idx]);

    pthread_mutex_lock(&m->count_lock);
    m->count++;
    pthread_mutex_unlock(&m->count_lock);
}

// Bug 2: map_get debe tomar lock
int map_get_safe(ConcurrentMap *m, const char *key, int *value) {
    unsigned int idx = hash(key);
    pthread_mutex_lock(&m->locks[idx]);
    Entry *curr = m->buckets[idx];
    while (curr) {
        if (strcmp(curr->key, key) == 0) {
            *value = curr->value;
            pthread_mutex_unlock(&m->locks[idx]);
            return 1;
        }
        curr = curr->next;
    }
    pthread_mutex_unlock(&m->locks[idx]);
    return 0;
}

// Bug 4: foreach debe tomar TODOS los locks
void map_foreach_safe(ConcurrentMap *m,
                      void (*callback)(const char *, int, void *),
                      void *ctx) {
    // Tomar todos los locks en orden (evitar deadlock)
    for (int i = 0; i < MAP_SIZE; i++) {
        pthread_mutex_lock(&m->locks[i]);
    }

    for (int i = 0; i < MAP_SIZE; i++) {
        Entry *curr = m->buckets[i];
        while (curr) {
            callback(curr->key, curr->value, ctx);
            curr = curr->next;
        }
    }

    // Liberar en orden inverso
    for (int i = MAP_SIZE - 1; i >= 0; i--) {
        pthread_mutex_unlock(&m->locks[i]);
    }
}
```

### Implementación equivalente en Rust (safe)

```rust
use std::collections::HashMap;
use std::sync::{Arc, RwLock};
use std::thread;

fn main() {
    // En Rust safe, NO hay data races posibles:
    let map = Arc::new(RwLock::new(HashMap::new()));

    let mut handles = vec![];

    // Writers
    for _ in 0..2 {
        let m = Arc::clone(&map);
        handles.push(thread::spawn(move || {
            for i in 0..1000 {
                let key = format!("key_{}", i);
                m.write().unwrap().insert(key, i * 10);
                // write() toma write lock automáticamente
                // MutexGuard se dropea al final del bloque
            }
        }));
    }

    // Readers
    for _ in 0..2 {
        let m = Arc::clone(&map);
        handles.push(thread::spawn(move || {
            for i in 0..1000 {
                let key = format!("key_{}", i);
                let _val = m.read().unwrap().get(&key).copied();
                // read() toma read lock automáticamente
            }
        }));
    }

    for h in handles {
        h.join().unwrap();
    }

    println!("Final size: {}", map.read().unwrap().len());
    // IMPOSIBLE tener data race: el compilador lo garantiza
}
```

---

## 37. Ejercicios

### Ejercicio 1: interpretar reportes TSan

**Objetivo**: Aprender a leer reportes de TSan.

**Tareas**:

**a)** Dado este reporte, identifica:
   - ¿Qué threads están involucrados?
   - ¿Cuál escribe y cuál lee?
   - ¿Qué variable es la afectada?
   - ¿Dónde se creó cada thread?

```
==================
WARNING: ThreadSanitizer: data race (pid=54321)
  Write of size 8 at 0x7f8a00100020 by thread T3:
    #0 update_stats src/stats.c:45:16
    #1 worker_run src/worker.c:78:5

  Previous read of size 8 at 0x7f8a00100020 by thread T1:
    #0 print_stats src/stats.c:92:24
    #1 monitor_loop src/monitor.c:30:9

  Location is heap block of size 128 at 0x7f8a00100000
    allocated by main thread at:
    #0 calloc
    #1 stats_create src/stats.c:12:20
    #2 main src/main.c:8:18

  Thread T3 (tid=54324, running) created by main thread at:
    #0 pthread_create
    #1 pool_start src/worker.c:110:5
    #2 main src/main.c:15:5

  Thread T1 (tid=54322, running) created by main thread at:
    #0 pthread_create
    #1 start_monitor src/monitor.c:55:5
    #2 main src/main.c:12:5
==================
```

**b)** ¿Cuál es la solución más probable?

**c)** ¿Por qué la sección "Location" dice "heap block of size 128"?

---

### Ejercicio 2: encontrar data races con TSan

**Objetivo**: Usar TSan para encontrar todos los data races.

**Tareas**:

**a)** Compila y ejecuta con TSan:

```c
#include <stdio.h>
#include <pthread.h>
#include <stdbool.h>
#include <unistd.h>

typedef struct {
    double balance;
    char owner[32];
    int transaction_count;
} Account;

Account account = {1000.0, "Alice", 0};

void *deposit(void *arg) {
    double amount = *(double *)arg;
    account.balance += amount;
    account.transaction_count++;
    return NULL;
}

void *withdraw(void *arg) {
    double amount = *(double *)arg;
    if (account.balance >= amount) {
        usleep(1);  // Simular latencia
        account.balance -= amount;
        account.transaction_count++;
    }
    return NULL;
}

void *print_balance(void *arg) {
    printf("Balance: %.2f (txns: %d)\n",
           account.balance, account.transaction_count);
    return NULL;
}

int main() {
    double amt1 = 500.0, amt2 = 300.0, amt3 = 200.0;

    pthread_t t1, t2, t3, t4;
    pthread_create(&t1, NULL, deposit, &amt1);
    pthread_create(&t2, NULL, withdraw, &amt2);
    pthread_create(&t3, NULL, withdraw, &amt3);
    pthread_create(&t4, NULL, print_balance, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);
    pthread_join(t4, NULL);

    printf("Final: %.2f\n", account.balance);
    return 0;
}
```

**b)** ¿Cuántos data races hay? Lista cada uno con las variables involucradas.

**c)** Además del data race, ¿hay una race condition lógica? (pista: check-then-act en withdraw)

**d)** Corrige TODOS los data races con un mutex. ¿La race condition lógica también se arregla?

---

### Ejercicio 3: TSan en Rust unsafe

**Objetivo**: Detectar data races en implementación unsafe de Rust.

**Tareas**:

**a)** Implementa un `SharedStack<T: Copy>` en Rust unsafe:
   ```rust
   pub struct SharedStack<T: Copy> {
       data: *mut T,
       top: usize,      // Índice del próximo slot libre
       capacity: usize,
   }
   unsafe impl<T: Copy> Send for SharedStack<T> {}
   unsafe impl<T: Copy> Sync for SharedStack<T> {}
   ```
   Con operaciones: `new(cap)`, `push(val)`, `pop() -> Option<T>`, `len()`.

**b)** Usa `Arc<SharedStack<i32>>` con 4 threads: 2 pushers y 2 poppers.

**c)** Ejecuta con TSan. ¿Cuántas data races reporta?

**d)** Reescribe usando `Mutex<Vec<T>>` (safe Rust). Ejecuta con TSan. ¿Reporta algo?

**e)** Compara las dos implementaciones:
   - ¿Cuántas líneas de unsafe eliminaste?
   - ¿Cuál es la diferencia de rendimiento?
   - ¿Cuál preferirías en producción?

---

### Ejercicio 4: detectar deadlock con TSan

**Objetivo**: Usar TSan para detectar inversión de orden de locks.

**Tareas**:

**a)** Implementa un sistema de transferencias entre 4 cuentas:
   ```c
   Account accounts[4];
   pthread_mutex_t locks[4];

   void transfer(int from, int to, double amount) {
       pthread_mutex_lock(&locks[from]);
       pthread_mutex_lock(&locks[to]);
       // ... transferir ...
       pthread_mutex_unlock(&locks[to]);
       pthread_mutex_unlock(&locks[from]);
   }
   ```

**b)** Lanza 4 threads que hacen transferencias en diferentes direcciones:
   ```
   Thread 0: transfer(0, 1, ...)
   Thread 1: transfer(1, 2, ...)
   Thread 2: transfer(2, 3, ...)
   Thread 3: transfer(3, 0, ...)  ← Crea ciclo en el grafo de locks
   ```

**c)** Compila con TSan. ¿Detecta el potencial deadlock?

**d)** Corrige usando la técnica de "ordered locking" (siempre tomar el lock de menor índice primero).

**e)** Verifica con TSan que ya no hay lock-order-inversion.

---

## Navegación

- **Anterior**: [T03 - UndefinedBehaviorSanitizer (UBSan)](../T03_UndefinedBehaviorSanitizer/README.md)
- **Siguiente**: [C04/S01/T01 - Benchmarking y Profiling](../../../C04_Benchmarking_y_Profiling/S01/T01/README.md)

---

> **Sección 3: Sanitizers como Red de Seguridad** — Tópico 4 de 4 completado
>
> **Fin de la Sección 3**: Los cuatro sanitizers principales:
> 1. **ASan** — buffer overflows, use-after-free, double-free, memory leaks
> 2. **MSan** — lectura de memoria no inicializada
> 3. **UBSan** — signed overflow, null deref, bad shifts, alignment, float cast
> 4. **TSan** — data races, deadlocks, lock-order inversion
>
> **Fin del Capítulo 3: Fuzzing y Sanitizers** — 3 secciones, 12 tópicos completados