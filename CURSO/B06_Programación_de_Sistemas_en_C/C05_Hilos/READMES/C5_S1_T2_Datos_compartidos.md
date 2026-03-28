# Datos compartidos y race conditions

## Índice
1. [Qué comparten los threads](#1-qué-comparten-los-threads)
2. [Race conditions](#2-race-conditions)
3. [Anatomía de un data race](#3-anatomía-de-un-data-race)
4. [El modelo de memoria del hardware](#4-el-modelo-de-memoria-del-hardware)
5. [Detectar data races: ThreadSanitizer](#5-detectar-data-races-threadsanitizer)
6. [C11 atomics: solución para casos simples](#6-c11-atomics-solución-para-casos-simples)
7. [Memory ordering](#7-memory-ordering)
8. [Patrones peligrosos](#8-patrones-peligrosos)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué comparten los threads

Todos los threads de un proceso comparten:

```
  Compartido (peligro de race conditions):
  ┌─────────────────────────────────────────┐
  │ Variables globales                      │
  │ Variables static                        │
  │ Heap (malloc'd)                         │
  │ File descriptors                        │
  │ Signal handlers (disposición)           │
  │ cwd, umask, environment                 │
  └─────────────────────────────────────────┘

  Privado (seguro sin sincronización):
  ┌─────────────────────────────────────────┐
  │ Stack (variables locales)               │
  │ Registros del CPU                       │
  │ errno (es thread-local desde NPTL)      │
  │ Signal mask                             │
  │ Thread-local storage (__thread / TLS)   │
  │ Thread ID (pthread_t / TID)             │
  └─────────────────────────────────────────┘
```

Cualquier dato en la zona "compartida" que sea accedido por más de un
thread, donde al menos uno escribe, es un **potencial data race**.

### Categorías de datos compartidos

```c
int global_var = 0;               // compartida — globales

static int file_static = 0;      // compartida — statics

void *func(void *arg)
{
    static int func_static = 0;   // compartida — static en función
    int local_var = 0;            // privada — stack del thread
    int *heap_ptr = malloc(4);    // el puntero es privado (stack),
                                  // pero si otro thread tiene el mismo
                                  // puntero, el dato es compartido
    return NULL;
}
```

Regla simple: si puedes acceder al dato desde más de un thread,
es compartido. El **heap** es el caso más sutil — la memoria en sí
es compartida, pero si solo un thread tiene el puntero, es
efectivamente privada.

---

## 2. Race conditions

Una **race condition** ocurre cuando el resultado de un programa
depende del orden de ejecución de los threads, y al menos un orden
produce un resultado incorrecto.

Un **data race** es un tipo específico de race condition: dos threads
acceden a la misma variable, al menos uno escribe, y no hay
sincronización. En C11, un data race es **comportamiento indefinido**
(no solo "resultado impredecible" — el compilador puede hacer
cualquier cosa).

### Ejemplo: contador compartido

```c
#include <stdio.h>
#include <pthread.h>

int counter = 0;  // compartida

void *increment(void *arg)
{
    for (int i = 0; i < 1000000; i++) {
        counter++;  // DATA RACE
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

    printf("Counter: %d (expected 2000000)\n", counter);
    return 0;
}
```

```bash
$ gcc -o race race.c -pthread
$ ./race
Counter: 1247831 (expected 2000000)
$ ./race
Counter: 1389542 (expected 2000000)
$ ./race
Counter: 1156723 (expected 2000000)
```

Cada ejecución da un resultado diferente, y siempre menor que 2000000.
¿Por qué?

---

## 3. Anatomía de un data race

`counter++` parece una instrucción, pero se compila a **tres**
operaciones:

```
  C:          counter++

  CPU:        1. LOAD  counter → registro     (leer valor actual)
              2. ADD   registro, 1             (incrementar)
              3. STORE registro → counter      (escribir resultado)
```

Cuando dos threads hacen esto sin sincronización, las operaciones se
**intercalan**:

### Ejecución correcta (secuencial):

```
  counter = 0

  Thread A                Thread B
  ────────                ────────
  LOAD  0 → reg
  ADD   reg + 1 = 1
  STORE 1 → counter
                          LOAD  1 → reg
                          ADD   reg + 1 = 2
                          STORE 2 → counter

  counter = 2  ✓
```

### Ejecución con race (intercalada):

```
  counter = 0

  Thread A                Thread B
  ────────                ────────
  LOAD  0 → regA
                          LOAD  0 → regB         ← ¡lee 0, no 1!
  ADD   regA + 1 = 1
                          ADD   regB + 1 = 1
  STORE 1 → counter
                          STORE 1 → counter      ← ¡sobreescribe!

  counter = 1  ✗  (perdimos un incremento)
```

Esto se llama **lost update** (actualización perdida). Con millones
de iteraciones, miles de incrementos se pierden.

### Diagrama temporal

```
  Tiempo ──────────────────────────────────────────►

  Thread A:  [LOAD 0] [ADD=1] [STORE 1]
                 ↕ ventana de vulnerabilidad
  Thread B:       [LOAD 0] [ADD=1] [STORE 1]
                                       ↑
                              Sobreescribe el 1 de Thread A
                              con su propio 1 → un incremento
                              perdido
```

La "ventana de vulnerabilidad" es el tiempo entre LOAD y STORE.
Cuanto más larga, más probable la race condition. Pero incluso
con una ventana de 1 nanosegundo, es suficiente para un bug.

---

## 4. El modelo de memoria del hardware

El problema es peor de lo que parece. No solo las instrucciones se
intercalan — el hardware agrega complicaciones:

### Cachés por core

Cada core del CPU tiene su propia caché (L1, L2). Las escrituras a
memoria no son instantáneamente visibles para otros cores:

```
  Core 0                    Core 1
  ┌──────────┐              ┌──────────┐
  │ L1 cache │              │ L1 cache │
  │ counter=1│              │ counter=0│ ← valor stale
  └────┬─────┘              └────┬─────┘
       │                         │
  ┌────┴─────────────────────────┴────┐
  │           L3 cache / RAM          │
  │           counter = ???           │
  └───────────────────────────────────┘
```

El protocolo de coherencia de caché (MESI/MOESI) eventualmente
sincroniza, pero no instantáneamente.

### Store buffers y reordenamiento

El CPU reordena instrucciones para rendimiento. Una escritura puede
quedar en el "store buffer" y no ser visible para otros cores
inmediatamente:

```c
// Thread A                    // Thread B
data = 42;                     while (ready == 0) ;
ready = 1;                     printf("%d\n", data);
```

Sin barreras de memoria, el CPU de Thread A podría hacer `ready = 1`
visible antes que `data = 42`. Thread B vería `ready == 1` pero
`data` podría ser 0 (o basura).

### Modelos de memoria por arquitectura

```
┌──────────────┬──────────────────────────────────────────────┐
│ Arquitectura │ Modelo                                      │
├──────────────┼──────────────────────────────────────────────┤
│ x86/x86_64   │ TSO (Total Store Order) — relativamente     │
│              │ estricto, stores se ven en orden, pero       │
│              │ store→load puede reordenarse                │
├──────────────┼──────────────────────────────────────────────┤
│ ARM          │ Weakly ordered — casi todo puede             │
│              │ reordenarse, necesita barreras explícitas    │
├──────────────┼──────────────────────────────────────────────┤
│ RISC-V       │ RVWMO — weak, similar a ARM                 │
└──────────────┴──────────────────────────────────────────────┘
```

Un programa que "funciona" en x86 puede fallar en ARM porque x86
es más estricto con el ordenamiento. Usar C11 atomics con el memory
ordering correcto garantiza portabilidad.

---

## 5. Detectar data races: ThreadSanitizer

ThreadSanitizer (TSan) es una herramienta de clang/gcc que detecta
data races en tiempo de ejecución:

```bash
$ gcc -fsanitize=thread -g -o race race.c -pthread
$ ./race
==================
WARNING: ThreadSanitizer: data race (pid=1234)
  Write of size 4 at 0x56049c by thread T2:
    #0 increment race.c:8 (race+0x1234)

  Previous write of size 4 at 0x56049c by thread T1:
    #0 increment race.c:8 (race+0x1234)

  Location is global 'counter' of size 4 at 0x56049c
==================
Counter: 1387254 (expected 2000000)
ThreadSanitizer: reported 1 warnings
```

TSan instrumenta todos los accesos a memoria y verifica que haya
sincronización (locks, atomics) entre accesos conflictivos.

### Opciones de compilación

```bash
# GCC
gcc -fsanitize=thread -g -O1 -o prog prog.c -pthread

# Clang
clang -fsanitize=thread -g -O1 -o prog prog.c -pthread
```

**Importante**: `-O1` o superior ayuda a TSan a ser más preciso.
`-O0` puede producir falsos positivos. `-g` agrega info de debug
para reportar números de línea.

### Limitaciones

- Overhead de ~5-15x en velocidad y ~5-10x en memoria
- Solo detecta races que **ocurren** en la ejecución — si un path
  no se ejecuta, la race no se detecta
- No detecta problemas con volatile o inline assembly
- No puede combinarse con ASan (AddressSanitizer) en la misma
  compilación

### Helgrind (Valgrind)

Alternativa a TSan, parte de Valgrind:

```bash
$ gcc -g -o race race.c -pthread
$ valgrind --tool=helgrind ./race
==1234== Possible data race during write of size 4
==1234==    at 0x401234: increment (race.c:8)
```

Más lento que TSan (~20-100x) pero no requiere recompilar.

---

## 6. C11 atomics: solución para casos simples

Para operaciones simples (contadores, flags, punteros), C11
proporciona tipos atómicos que garantizan acceso libre de data races:

```c
#include <stdatomic.h>

_Atomic int counter = 0;

void *increment(void *arg)
{
    for (int i = 0; i < 1000000; i++) {
        atomic_fetch_add(&counter, 1);  // atómico
    }
    return NULL;
}

// Ahora counter siempre será 2000000
```

### Tipos atómicos

```c
_Atomic int    ai;         // int atómico
_Atomic long   al;         // long atómico
_Atomic _Bool  ab;         // bool atómico
atomic_int     ai2;        // typedef equivalente
atomic_flag    af;         // el tipo más básico (lock-free garantizado)
```

### Operaciones atómicas

```c
// Almacenar
atomic_store(&ai, 42);

// Cargar
int val = atomic_load(&ai);

// Read-modify-write
int old = atomic_fetch_add(&ai, 1);     // retorna valor anterior
int old = atomic_fetch_sub(&ai, 1);
int old = atomic_fetch_or(&ai, mask);
int old = atomic_fetch_and(&ai, mask);
int old = atomic_fetch_xor(&ai, mask);

// Exchange
int old = atomic_exchange(&ai, new_val);

// Compare-and-swap (CAS) — la más poderosa
int expected = 5;
_Bool ok = atomic_compare_exchange_strong(&ai, &expected, 10);
// Si ai == 5: establece ai = 10, retorna true
// Si ai != 5: establece expected = valor actual, retorna false
```

### Compare-and-swap (CAS)

CAS es la primitiva fundamental para algoritmos lock-free:

```c
// Incremento lock-free con CAS (equivalente a atomic_fetch_add)
void cas_increment(_Atomic int *val)
{
    int old = atomic_load(val);
    while (!atomic_compare_exchange_weak(val, &old, old + 1)) {
        // Si falló, old se actualizó al valor actual → reintentar
    }
}
```

`_weak` vs `_strong`: `weak` puede fallar espuriamente (retornar false
incluso cuando el valor coincide). Es más eficiente en un loop de
reintento en algunas arquitecturas (ARM). `strong` nunca falla
espuriamente pero puede ser más lento.

### atomic_flag — spinlock mínimo

`atomic_flag` es el único tipo garantizado lock-free en todas las
plataformas:

```c
atomic_flag lock = ATOMIC_FLAG_INIT;

void spin_lock(atomic_flag *lk)
{
    while (atomic_flag_test_and_set(lk)) {
        // busy wait — consume CPU
    }
}

void spin_unlock(atomic_flag *lk)
{
    atomic_flag_clear(lk);
}
```

**Advertencia**: los spinlocks son útiles solo cuando la sección
crítica es extremadamente corta (< 100ns). Para el resto, usar
`pthread_mutex`.

---

## 7. Memory ordering

Las operaciones atómicas aceptan un parámetro de **memory ordering**
que controla qué reordenamientos puede hacer el hardware/compilador:

```c
atomic_store_explicit(&var, 42, memory_order_release);
int val = atomic_load_explicit(&var, memory_order_acquire);
```

### Los 6 orderings de C11

```
┌─────────────────────┬────────────────────────────────────────────┐
│ Ordering            │ Garantía                                   │
├─────────────────────┼────────────────────────────────────────────┤
│ memory_order_relaxed│ Solo atomicidad, sin orden                │
│                     │ (más rápido, solo para contadores)        │
├─────────────────────┼────────────────────────────────────────────┤
│ memory_order_acquire│ Las lecturas/escrituras DESPUÉS de esta   │
│ (loads)             │ operación no se pueden mover ANTES        │
├─────────────────────┼────────────────────────────────────────────┤
│ memory_order_release│ Las lecturas/escrituras ANTES de esta     │
│ (stores)            │ operación no se pueden mover DESPUÉS      │
├─────────────────────┼────────────────────────────────────────────┤
│ memory_order_acq_rel│ Combina acquire + release                 │
│ (read-modify-write) │                                           │
├─────────────────────┼────────────────────────────────────────────┤
│ memory_order_seq_cst│ Orden total global — todos los threads    │
│ (default)           │ ven las operaciones en el mismo orden     │
│                     │ (más lento, más seguro)                   │
├─────────────────────┼────────────────────────────────────────────┤
│ memory_order_consume│ Deprecated en la práctica — tratar como   │
│                     │ acquire                                   │
└─────────────────────┴────────────────────────────────────────────┘
```

### Patrón acquire/release

El patrón más común: un thread "publica" datos (release), otro thread
los "consume" (acquire):

```c
_Atomic int ready = 0;
int data = 0;  // no atómico, protegido por el protocolo

// Productor
void produce(void)
{
    data = 42;                                            // preparar datos
    atomic_store_explicit(&ready, 1, memory_order_release); // publicar
    //          ↑ barrera: data=42 es visible ANTES de ready=1
}

// Consumidor
void consume(void)
{
    while (atomic_load_explicit(&ready, memory_order_acquire) != 1)
        ;  // esperar
    //          ↑ barrera: data se lee DESPUÉS de ver ready=1
    printf("%d\n", data);  // siempre 42, nunca 0
}
```

Sin las barreras acquire/release, el consumidor podría ver `ready=1`
pero `data=0` (especialmente en ARM).

### seq_cst vs acquire/release

`seq_cst` (el default) es más restrictivo: garantiza un **orden total
global**. Es más fácil de razonar pero más lento en ARM (~2x):

```c
// Estas dos líneas son equivalentes:
atomic_store(&var, 42);
atomic_store_explicit(&var, 42, memory_order_seq_cst);
```

Regla práctica: empieza con `seq_cst` (default). Solo baja a
`acquire/release` o `relaxed` si tienes un bottleneck medido y
entiendes las implicaciones.

### relaxed — solo para contadores estadísticos

```c
// Counter de métricas: no importa el orden, solo el valor final
_Atomic long request_count = 0;

void handle_request(void)
{
    atomic_fetch_add_explicit(&request_count, 1, memory_order_relaxed);
    // Más rápido que seq_cst, correcto para estadísticas
}
```

---

## 8. Patrones peligrosos

### 8.1 Double-checked locking (roto sin atomics)

```c
// MAL: patrón clásico roto
static Resource *instance = NULL;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

Resource *get_instance(void)
{
    if (instance == NULL) {          // primera lectura sin lock
        pthread_mutex_lock(&mtx);
        if (instance == NULL) {      // segunda lectura con lock
            instance = create_resource();
        }
        pthread_mutex_unlock(&mtx);
    }
    return instance;
}
// Problema: la primera lectura de instance es un data race
// El compilador/CPU puede ver un puntero no-NULL antes de
// que el contenido de la Resource esté completamente escrito
```

```c
// BIEN: con atomics
static _Atomic(Resource *) instance = NULL;
static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;

Resource *get_instance(void)
{
    Resource *p = atomic_load_explicit(&instance, memory_order_acquire);
    if (p == NULL) {
        pthread_mutex_lock(&mtx);
        p = atomic_load_explicit(&instance, memory_order_relaxed);
        if (p == NULL) {
            p = create_resource();
            atomic_store_explicit(&instance, p, memory_order_release);
        }
        pthread_mutex_unlock(&mtx);
    }
    return p;
}

// O más simple: usar pthread_once
static pthread_once_t once = PTHREAD_ONCE_INIT;
static Resource *instance;

void init_instance(void) { instance = create_resource(); }

Resource *get_instance(void)
{
    pthread_once(&once, init_instance);
    return instance;
}
```

### 8.2 Flag de terminación sin atomic

```c
// MAL: el compilador puede optimizar el loop
int running = 1;  // no atómico

void *worker(void *arg)
{
    while (running) {  // compilador puede cachear en registro → loop infinito
        do_work();
    }
    return NULL;
}

void stop_workers(void)
{
    running = 0;  // data race
}
```

```c
// BIEN: usar _Atomic
_Atomic int running = 1;

void *worker(void *arg)
{
    while (atomic_load(&running)) {
        do_work();
    }
    return NULL;
}
```

`volatile` **no es suficiente**: `volatile` previene la optimización
del compilador pero no agrega barreras de memoria ni garantiza
atomicidad. `_Atomic` hace ambas cosas.

### 8.3 Publicar un puntero a estructura parcialmente inicializada

```c
// MAL: otro thread puede ver node no-NULL pero con campos sin inicializar
struct node *head = NULL;

void insert(int value)
{
    struct node *n = malloc(sizeof(*n));
    n->value = value;
    n->next = head;
    head = n;  // data race: otro thread puede leer head
    // Peor: head=n puede ser visible antes que n->value=value
}
```

```c
// BIEN: usar atomic con release/acquire
_Atomic(struct node *) head = NULL;

void insert(int value)
{
    struct node *n = malloc(sizeof(*n));
    n->value = value;
    n->next = atomic_load_explicit(&head, memory_order_relaxed);
    atomic_store_explicit(&head, n, memory_order_release);
    // release: n->value y n->next son visibles antes que head=n
}

struct node *read_head(void)
{
    struct node *h = atomic_load_explicit(&head, memory_order_acquire);
    // acquire: si h != NULL, h->value y h->next son válidos
    return h;
}
// Nota: esto solo es correcto para un escritor y múltiples lectores.
// Para múltiples escritores, se necesita CAS.
```

### 8.4 Acceso no atómico a structs grandes

```c
// MAL: struct de 128 bytes no se puede leer/escribir atómicamente
struct config {
    char host[64];
    int port;
    int timeout;
    char cert_path[56];
};
struct config current_config;  // compartida entre threads

// Thread A: escribe la config completa
current_config = new_config;  // NO es atómico para structs grandes

// Thread B: lee la config
struct config c = current_config;  // puede ver mitad vieja, mitad nueva
// "torn read": host de la config nueva, port de la vieja
```

```c
// BIEN: usar mutex para structs grandes
pthread_mutex_t config_mtx = PTHREAD_MUTEX_INITIALIZER;
struct config current_config;

void update_config(const struct config *new_cfg)
{
    pthread_mutex_lock(&config_mtx);
    current_config = *new_cfg;
    pthread_mutex_unlock(&config_mtx);
}

void read_config(struct config *out)
{
    pthread_mutex_lock(&config_mtx);
    *out = current_config;
    pthread_mutex_unlock(&config_mtx);
}
```

---

## 9. Errores comunes

### Error 1: Asumir que una operación C es atómica

```c
// MAL: "es solo una instrucción, ¿no?"
counter++;          // LOAD + ADD + STORE = 3 operaciones
ptr = new_ptr;      // puede ser atómico en x86 si está alineado,
                    //   pero el compilador no lo garantiza
flag = 1;           // puede ser atómico en la práctica, pero
                    //   es un data race según C11 → UB

// BIEN: usar _Atomic para todo lo compartido
_Atomic int counter = 0;
atomic_fetch_add(&counter, 1);
```

**Por qué importa**: "funciona en mi máquina" no significa que sea
correcto. El compilador puede reordenar, el CPU puede reordenar,
y en ARM los comportamientos son diferentes a x86.

### Error 2: Usar volatile en vez de _Atomic

```c
// MAL: volatile no es para threading
volatile int counter = 0;

void *increment(void *arg)
{
    for (int i = 0; i < 1000000; i++)
        counter++;  // sigue siendo LOAD+ADD+STORE, no atómico
    return NULL;
}
// volatile solo previene optimizaciones del compilador.
// No agrega barreras de memoria ni hace la operación atómica.

// BIEN: _Atomic
_Atomic int counter = 0;
atomic_fetch_add(&counter, 1);
```

`volatile` es para hardware-mapped I/O y signal handlers con
`sig_atomic_t`. Para threads, siempre usar `_Atomic` o mutexes.

### Error 3: Race condition en check-then-act

```c
// MAL: time-of-check to time-of-use (TOCTOU)
if (counter < MAX) {     // Thread A: lee counter = 99
    counter++;           // Thread B: también lee 99, ambos incrementan
}                        // counter = 101 > MAX — invariante roto

// BIEN: hacer check-and-act atómicamente
int old;
do {
    old = atomic_load(&counter);
    if (old >= MAX) return;  // no incrementar
} while (!atomic_compare_exchange_weak(&counter, &old, old + 1));

// O más simple: usar mutex
pthread_mutex_lock(&mtx);
if (counter < MAX)
    counter++;
pthread_mutex_unlock(&mtx);
```

### Error 4: Compartir datos entre threads sin darse cuenta

```c
// MAL: strtok usa estado global interno (no thread-safe)
void *parse(void *arg)
{
    char *line = strdup((char *)arg);
    char *tok = strtok(line, ",");  // ¡estado global compartido!
    while (tok) {
        process(tok);
        tok = strtok(NULL, ",");
    }
    free(line);
    return NULL;
}
// Dos threads llamando strtok corrompen el estado interno

// BIEN: usar la versión reentrante
char *saveptr;
char *tok = strtok_r(line, ",", &saveptr);
while (tok) {
    process(tok);
    tok = strtok_r(NULL, ",", &saveptr);
}
```

Funciones no thread-safe comunes y sus alternativas:

```
┌─────────────────┬─────────────────────────────────────┐
│ No thread-safe  │ Thread-safe (reentrante)            │
├─────────────────┼─────────────────────────────────────┤
│ strtok()        │ strtok_r()                          │
│ localtime()     │ localtime_r()                       │
│ ctime()         │ ctime_r()                           │
│ asctime()       │ asctime_r()                         │
│ strerror()      │ strerror_r()                        │
│ gethostbyname() │ getaddrinfo()                       │
│ rand()          │ rand_r() o drand48_r()              │
│ readdir()       │ readdir_r() (deprecated) o mutex    │
│ getenv()        │ secure_getenv() + mutex (no _r)     │
└─────────────────┴─────────────────────────────────────┘
```

### Error 5: Pensar que mutex es la única solución

```c
// Overkill: mutex para un simple contador
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;

void increment(void) {
    pthread_mutex_lock(&mtx);
    counter++;
    pthread_mutex_unlock(&mtx);
}
// Funciona, pero es ~10x más lento que atomic_fetch_add

// BIEN: atomic es suficiente para operaciones simples
_Atomic int counter = 0;
void increment(void) {
    atomic_fetch_add(&counter, 1);
}

// Mutex es necesario cuando:
// - Múltiples variables deben actualizarse juntas
// - Estructuras de datos complejas (listas, árboles)
// - Check-then-act compuesto
// - Necesitas esperar una condición (con condvar)
```

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│              DATOS COMPARTIDOS Y RACE CONDITIONS                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Data race = 2+ threads acceden a misma variable,                  │
│              al menos uno escribe, sin sincronización.              │
│              → Comportamiento indefinido en C11.                   │
│                                                                     │
│  Soluciones:                                                        │
│    1. _Atomic (operaciones simples: contadores, flags, punteros)   │
│    2. pthread_mutex (secciones críticas, datos complejos)          │
│    3. Evitar compartir (thread-local, copias privadas)             │
│                                                                     │
│  C11 atomics:                                                       │
│    #include <stdatomic.h>                                           │
│    _Atomic int x = 0;                                              │
│    atomic_store(&x, 42)                                            │
│    int v = atomic_load(&x)                                          │
│    int old = atomic_fetch_add(&x, 1)                               │
│    int old = atomic_exchange(&x, new)                              │
│    bool ok = atomic_compare_exchange_strong(&x, &expected, desired)│
│                                                                     │
│  Memory ordering (de más restrictivo a menos):                      │
│    seq_cst  — orden total global (default, más seguro)             │
│    acq_rel  — acquire + release (read-modify-write)                │
│    acquire  — lecturas después no se mueven antes                  │
│    release  — escrituras antes no se mueven después                │
│    relaxed  — solo atomicidad, sin orden (solo contadores)         │
│                                                                     │
│  Patrón acquire/release:                                            │
│    Productor: escribe datos, luego atomic_store(..., release)      │
│    Consumidor: atomic_load(..., acquire), luego lee datos           │
│                                                                     │
│  Detectar races:                                                    │
│    gcc -fsanitize=thread -g -O1 prog.c -pthread                    │
│    valgrind --tool=helgrind ./prog                                  │
│                                                                     │
│  No thread-safe → usar versión _r():                               │
│    strtok→strtok_r  localtime→localtime_r  rand→rand_r            │
│                                                                     │
│  Reglas de oro:                                                     │
│    1. Si 2+ threads tocan una variable y alguno escribe → proteger │
│    2. _Atomic para una variable simple, mutex para lo demás        │
│    3. volatile NO es para threads, usar _Atomic                    │
│    4. counter++ NO es atómico — es LOAD+ADD+STORE                  │
│    5. Usar ThreadSanitizer en CI — los races son intermitentes     │
│    6. Empezar con seq_cst, optimizar solo si necesario             │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Demostrar y medir el data race

Escribe un programa con N threads que incrementan un contador global
M veces cada uno. Mide la diferencia entre el resultado esperado
(N*M) y el real para diferentes valores de N y M:

```c
// Probar sin protección, con _Atomic, y con mutex
// Medir tiempo de cada variante con clock_gettime

// N=2,  M=1000000
// N=4,  M=1000000
// N=8,  M=1000000
// N=16, M=1000000
```

Generar una tabla con:
- Resultado obtenido vs esperado (sin protección)
- Tiempo de ejecución de cada variante
- Overhead porcentual de atomic vs mutex vs sin protección

Compilar con `-fsanitize=thread` y verificar que TSan detecta la
race en la variante sin protección y no en las otras dos.

**Pregunta de reflexión**: ¿La diferencia entre resultado obtenido y
esperado crece linealmente con N? ¿Por qué con N=2 la diferencia
es relativamente menor que con N=8? ¿Qué rol juega la contención
de caché (cache line bouncing)?

### Ejercicio 2: Publicar datos con acquire/release

Implementa un productor-consumidor lock-free donde el productor
prepara una estructura de datos y la "publica" con release, y el
consumidor la lee con acquire:

```c
typedef struct {
    int x, y, z;
    char name[32];
} payload_t;

_Atomic int version = 0;
payload_t current_payload;  // protegida por el protocolo version

// Productor: solo uno
void publish(const payload_t *p);

// Consumidor: múltiples
int read_payload(payload_t *out);
// Retorna la versión leída
```

Verificar con TSan que no hay data races. Probar con 1 productor y
4 consumidores.

**Pregunta de reflexión**: Este patrón funciona con un solo productor.
¿Qué se rompe con dos productores? ¿Cómo lo resolverías sin mutex
(pista: sequence lock o CAS en version)?

### Ejercicio 3: Tabla hash thread-safe con granularidad fina

Implementa una tabla hash donde cada bucket tiene su propio mutex
(striped locking), en contraste con un solo mutex global:

```c
#define N_BUCKETS 64

typedef struct entry {
    char *key;
    int value;
    struct entry *next;
} entry_t;

typedef struct {
    entry_t *buckets[N_BUCKETS];
    pthread_mutex_t locks[N_BUCKETS];  // un mutex por bucket
} hashtable_t;

void ht_init(hashtable_t *ht);
int  ht_insert(hashtable_t *ht, const char *key, int value);
int  ht_lookup(hashtable_t *ht, const char *key, int *value);
int  ht_delete(hashtable_t *ht, const char *key);
void ht_destroy(hashtable_t *ht);
```

Benchmark: N threads haciendo inserts/lookups aleatorios. Comparar:
1. Un solo mutex global (serializa todo)
2. Un mutex por bucket (permite paralelismo entre buckets)
3. rwlock por bucket (permite lookups paralelos en el mismo bucket)

**Pregunta de reflexión**: ¿Cuántos buckets necesitas para que el
striped locking sea significativamente mejor que un mutex global?
¿Qué pasa si dos keys caen en el mismo bucket — se serializa? ¿Es
posible hacer la tabla hash completamente lock-free con CAS?
