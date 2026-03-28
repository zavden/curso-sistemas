# Thread-local storage

## Índice
1. [Concepto y motivación](#1-concepto-y-motivación)
2. [__thread y _Thread_local](#2-__thread-y-_thread_local)
3. [pthread_key_create: TLS dinámico](#3-pthread_key_create-tls-dinámico)
4. [Destructores](#4-destructores)
5. [errno: el TLS más famoso](#5-errno-el-tls-más-famoso)
6. [Patrones de uso](#6-patrones-de-uso)
7. [Implementación interna](#7-implementación-interna)
8. [TLS y fork](#8-tls-y-fork)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Concepto y motivación

Thread-local storage (TLS) proporciona variables que son **globales
dentro de un thread** pero **privadas entre threads**. Cada thread
tiene su propia copia independiente:

```
  Variable global:                  Variable thread-local:
  ┌────────┐                        ┌────────┐
  │ Proceso│                        │ Proceso│
  │        │                        │        │
  │  ┌───┐ │                        │  T1: ┌───┐
  │  │ X │ │ ← todos los threads    │      │ X │ ← copia de T1
  │  └───┘ │    ven el mismo X      │  T2: ┌───┐
  │        │                        │      │ X │ ← copia de T2
  │        │                        │  T3: ┌───┐
  └────────┘                        │      │ X │ ← copia de T3
                                    └────────┘
```

### ¿Para qué sirve?

1. **Convertir funciones no thread-safe en thread-safe** sin cambiar
   la API (como `errno`, `strtok` interno)
2. **Evitar sincronización**: cada thread tiene su propia copia, no
   hay contención
3. **Estado per-thread**: buffers de trabajo, caches, contadores,
   conexiones a BD

### La alternativa: pasar contexto explícitamente

```c
// Sin TLS: pasar contexto por parámetro (más explícito, más tedioso)
void process(context_t *ctx, data_t *data) {
    log(ctx->logger, "processing...");
    // ctx se pasa a todas las funciones → "parameter drilling"
}

// Con TLS: el contexto es implícito (más limpio, menos visible)
__thread context_t *ctx;
void process(data_t *data) {
    log(ctx->logger, "processing...");
    // ctx es invisible en la firma pero presente en cada thread
}
```

---

## 2. __thread y _Thread_local

### __thread (GCC/Clang extension, pre-C11)

```c
__thread int counter = 0;          // int thread-local
__thread char buffer[256];         // array thread-local
__thread const char *name = NULL;  // puntero thread-local
```

### _Thread_local (C11 estándar)

```c
#include <threads.h>  // o simplemente usar la keyword

_Thread_local int counter = 0;
```

En la práctica, `__thread` y `_Thread_local` son equivalentes en
GCC/Clang. La macro `thread_local` (minúsculas) está definida en
`<threads.h>` de C11:

```c
// C11
#include <threads.h>
thread_local int counter = 0;

// Pre-C11 / extensión
__thread int counter = 0;

// GCC/Clang aceptan ambos
```

### Restricciones

```
┌────────────────────────────┬──────────────────────────────────┐
│ Permitido                  │ No permitido                     │
├────────────────────────────┼──────────────────────────────────┤
│ Tipos escalares (int, ptr) │ VLA (variable-length arrays)     │
├────────────────────────────┼──────────────────────────────────┤
│ Arrays de tamaño fijo      │ Inicialización con valor         │
│                            │ no constante en compilación      │
├────────────────────────────┼──────────────────────────────────┤
│ Structs                    │ Designadores complejos           │
├────────────────────────────┼──────────────────────────────────┤
│ static y extern            │ auto (ya es per-thread vía stack)│
├────────────────────────────┼──────────────────────────────────┤
│ Inicialización constante   │ Llamadas a funciones como        │
│ (literales, 0, NULL)       │ inicializador                    │
└────────────────────────────┴──────────────────────────────────┘
```

```c
// OK
__thread int x = 42;
__thread double arr[100];
__thread struct { int a; char b[32]; } state = {0};
__thread void *ptr = NULL;

// ERROR: inicializador no constante
__thread int y = compute_value();      // ✗
__thread time_t now = time(NULL);      // ✗
__thread int *p = malloc(sizeof(int)); // ✗
```

Para inicialización compleja, usar `pthread_key_create` con un
destructor, o inicializar en la función del thread.

### Ejemplo completo

```c
#include <stdio.h>
#include <pthread.h>

__thread int my_id = -1;
__thread int call_count = 0;

void do_work(void)
{
    call_count++;
    printf("Thread %d: call #%d\n", my_id, call_count);
}

void *worker(void *arg)
{
    my_id = (int)(intptr_t)arg;  // cada thread establece su ID

    for (int i = 0; i < 3; i++)
        do_work();

    printf("Thread %d: total calls = %d\n", my_id, call_count);
    return NULL;
}

int main(void)
{
    pthread_t threads[3];
    for (int i = 0; i < 3; i++)
        pthread_create(&threads[i], NULL, worker, (void *)(intptr_t)i);

    for (int i = 0; i < 3; i++)
        pthread_join(threads[i], NULL);

    printf("Main: my call_count = %d\n", call_count);
    // Main tiene su propia copia, sigue en 0
    return 0;
}
```

Salida (orden variable entre threads):
```
Thread 0: call #1
Thread 0: call #2
Thread 0: call #3
Thread 0: total calls = 3
Thread 1: call #1
...
Main: my call_count = 0
```

Cada thread tiene su propio `call_count` independiente.

---

## 3. pthread_key_create: TLS dinámico

La API POSIX para TLS en tiempo de ejecución. Útil cuando:
- Necesitas inicialización compleja (malloc, open, etc.)
- Necesitas un destructor al terminar el thread
- El tipo de dato se determina en runtime
- Trabajas con bibliotecas que no pueden asumir `__thread`

### Crear clave

```c
#include <pthread.h>

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *));
// key: recibe la clave creada (compartida por todos los threads)
// destructor: función llamada al terminar cada thread (puede ser NULL)
// Retorna: 0 en éxito
```

La clave es **global** — todos los threads la usan para acceder a
su propio dato. Pero el **valor** asociado es per-thread.

### Establecer y obtener valor

```c
int pthread_setspecific(pthread_key_t key, const void *value);
// Establece el valor TLS para el thread actual

void *pthread_getspecific(pthread_key_t key);
// Obtiene el valor TLS del thread actual
// Retorna NULL si no se ha establecido
```

### Ejemplo

```c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

pthread_key_t buffer_key;

void destroy_buffer(void *buf)
{
    printf("Destroying buffer for thread %lu\n",
           (unsigned long)pthread_self());
    free(buf);
}

char *get_thread_buffer(void)
{
    char *buf = pthread_getspecific(buffer_key);
    if (buf == NULL) {
        // Primera vez en este thread — inicializar
        buf = malloc(1024);
        if (buf == NULL) return NULL;
        buf[0] = '\0';
        pthread_setspecific(buffer_key, buf);
    }
    return buf;
}

void *worker(void *arg)
{
    int id = (int)(intptr_t)arg;

    char *buf = get_thread_buffer();
    snprintf(buf, 1024, "Thread %d was here", id);
    printf("%s\n", buf);

    // No necesitamos free — el destructor lo hace automáticamente
    return NULL;
}

int main(void)
{
    pthread_key_create(&buffer_key, destroy_buffer);

    pthread_t threads[3];
    for (int i = 0; i < 3; i++)
        pthread_create(&threads[i], NULL, worker, (void *)(intptr_t)i);

    for (int i = 0; i < 3; i++)
        pthread_join(threads[i], NULL);

    pthread_key_delete(buffer_key);
    return 0;
}
```

### Límite de claves

POSIX garantiza al menos `PTHREAD_KEYS_MAX` (128) claves por proceso.
En Linux es 1024. Cada `pthread_key_create` consume una:

```c
#include <limits.h>
printf("Max keys: %d\n", PTHREAD_KEYS_MAX);  // 1024 en Linux
```

Las claves son un recurso global escaso. No crear una por objeto —
crear una por tipo de dato y almacenar un puntero a tu estructura.

### pthread_key_delete

```c
int pthread_key_delete(pthread_key_t key);
// Libera la clave. NO llama destructores de valores existentes.
// Los valores TLS se invalidan pero no se liberan.
```

**Cuidado**: `pthread_key_delete` no ejecuta destructores. Solo
marca la clave como disponible para reutilización. Los destructores
solo se ejecutan al terminar el thread.

---

## 4. Destructores

Los destructores de `pthread_key_create` se ejecutan automáticamente
cuando un thread termina (return, pthread_exit, cancelación):

```c
void my_destructor(void *value)
{
    // value es el puntero que el thread tenía para esta clave
    // Liberar recursos, cerrar conexiones, etc.
    free(value);
}

pthread_key_create(&key, my_destructor);
```

### Orden de ejecución

1. El thread termina
2. Para cada clave con destructor no-NULL y valor no-NULL:
   - El valor se establece a NULL
   - Se llama al destructor con el valor anterior
3. Si algún destructor establece un nuevo valor no-NULL para una
   clave (re-registra datos), el proceso se repite
4. Se repite hasta `PTHREAD_DESTRUCTOR_ITERATIONS` veces (al menos 4)

```
  Thread termina
       │
       ▼
  ┌─────────────────────────────┐
  │ Para cada key con value≠NULL │◄────────────────────┐
  │   value_copy = value         │                      │
  │   value = NULL               │                      │
  │   destructor(value_copy)     │                      │
  └──────────────┬───────────────┘                      │
                 │                                      │
                 ▼                                      │
  ┌──────────────────────────────┐                      │
  │ ¿Algún destructor puso un   │── Sí (y iteration   ─┘
  │  nuevo valor no-NULL?       │    < MAX_ITERATIONS)
  │                              │
  └──────────────┬───────────────┘
                 │ No
                 ▼
            Thread destruido
```

### Destructores y __thread

Las variables `__thread` **no tienen destructores**. Si una variable
`__thread` es un puntero a memoria heap, esa memoria se pierde al
terminar el thread:

```c
// MAL: fuga de memoria con __thread
__thread char *buffer = NULL;

void *worker(void *arg)
{
    buffer = malloc(4096);
    // ... usar buffer ...
    return NULL;  // buffer se pierde — nadie hace free
}

// BIEN: opción 1 — free explícito antes de retornar
void *worker(void *arg)
{
    buffer = malloc(4096);
    // ... usar buffer ...
    free(buffer);
    return NULL;
}

// BIEN: opción 2 — usar pthread_key con destructor
// (ver sección 3)

// BIEN: opción 3 — cleanup handler
void *worker(void *arg)
{
    buffer = malloc(4096);
    pthread_cleanup_push(free, buffer);
    // ... usar buffer (incluso si es cancelado, free se ejecuta) ...
    pthread_cleanup_pop(1);  // 1 = ejecutar free
    return NULL;
}
```

---

## 5. errno: el TLS más famoso

`errno` es el ejemplo canónico de TLS. Originalmente era una variable
global, lo que la hacía inútil con threads:

```c
// Si errno fuera global:
// Thread A: read() falla, errno = EINTR
// Thread B: write() falla, errno = ENOSPC
// Thread A: if (errno == EINTR)  ← ¡ve ENOSPC!

// Con TLS, cada thread tiene su propio errno
```

### Implementación

En glibc/Linux, `errno` es una macro que expande a una llamada a
función que retorna un puntero a un int thread-local:

```c
// En <errno.h>:
extern int *__errno_location(void);
#define errno (*__errno_location())

// Internamente, __errno_location retorna un puntero al errno
// del thread actual, almacenado en el TLS del thread
```

Esto significa que `errno` es thread-safe automáticamente. Cada
thread lee y escribe su propia copia.

### Otras variables "globales" que son TLS

```
┌──────────────────────┬──────────────────────────────────┐
│ Variable             │ Cómo es thread-safe              │
├──────────────────────┼──────────────────────────────────┤
│ errno                │ Macro → __errno_location() (TLS) │
├──────────────────────┼──────────────────────────────────┤
│ h_errno              │ TLS (gethostbyname, deprecated)  │
├──────────────────────┼──────────────────────────────────┤
│ strtok internal state│ NO thread-safe → usar strtok_r   │
├──────────────────────┼──────────────────────────────────┤
│ localtime buf        │ NO thread-safe → usar localtime_r│
├──────────────────────┼──────────────────────────────────┤
│ getenv result        │ NO thread-safe (retorna puntero  │
│                      │ a environ, que es compartido)    │
└──────────────────────┴──────────────────────────────────┘
```

---

## 6. Patrones de uso

### 6.1 Logger per-thread

```c
#include <stdio.h>
#include <pthread.h>

__thread FILE *thread_log = NULL;
__thread int thread_id = -1;

void init_thread_logging(int id)
{
    thread_id = id;
    char path[64];
    snprintf(path, sizeof(path), "/tmp/thread_%d.log", id);
    thread_log = fopen(path, "w");
}

void thread_log_msg(const char *msg)
{
    if (thread_log) {
        fprintf(thread_log, "[T%d] %s\n", thread_id, msg);
        fflush(thread_log);
    }
}

void cleanup_thread_logging(void)
{
    if (thread_log) {
        fclose(thread_log);
        thread_log = NULL;
    }
}

void *worker(void *arg)
{
    int id = (int)(intptr_t)arg;
    init_thread_logging(id);
    pthread_cleanup_push((void(*)(void*))cleanup_thread_logging, NULL);

    thread_log_msg("started");
    // ... trabajo ...
    thread_log_msg("finished");

    pthread_cleanup_pop(1);  // cierra el archivo
    return NULL;
}
```

### 6.2 Lazy initialization con pthread_once + TLS

```c
static pthread_key_t db_conn_key;
static pthread_once_t key_once = PTHREAD_ONCE_INIT;

void make_key(void)
{
    pthread_key_create(&db_conn_key, close_db_connection);
}

db_connection_t *get_db_connection(void)
{
    pthread_once(&key_once, make_key);

    db_connection_t *conn = pthread_getspecific(db_conn_key);
    if (conn == NULL) {
        conn = open_db_connection("localhost", 5432);
        pthread_setspecific(db_conn_key, conn);
    }
    return conn;
}

// Cada thread obtiene su propia conexión
// Se crea la primera vez que la pide
// Se cierra automáticamente al terminar el thread
```

### 6.3 Convertir API no thread-safe en thread-safe

Ejemplo: hacer una versión thread-safe de una función que usa buffer
estático interno:

```c
// API original (no thread-safe):
// char *format_number(int n) — retorna puntero a buffer estático

// Versión thread-safe con TLS:
__thread char format_buf[64];

char *format_number(int n)
{
    snprintf(format_buf, sizeof(format_buf), "%'d", n);
    return format_buf;
    // Cada thread tiene su propio format_buf
    // El puntero retornado es válido hasta la próxima llamada
    // desde EL MISMO thread
}
```

### 6.4 Contadores per-thread (evitar contención)

Cuando muchos threads incrementan un contador, un `_Atomic` crea
contención de cache line. Una alternativa: contar por thread y
sumar al final:

```c
#define MAX_THREADS 64
__thread long local_counter = 0;

// Cada thread incrementa sin sincronización
void count_event(void)
{
    local_counter++;  // sin atomic, sin lock — velocidad máxima
}

// Para obtener el total, necesitas un mecanismo para sumar
// las copias de todos los threads. Opciones:
// 1. Cada thread reporta su total al terminar
// 2. Array global con slot por thread
// 3. pthread_key con destructor que acumula

// Opción 2: array global
_Atomic long totals[MAX_THREADS];

void *worker(void *arg)
{
    int id = (int)(intptr_t)arg;
    for (int i = 0; i < 10000000; i++)
        local_counter++;

    atomic_store(&totals[id], local_counter);
    return NULL;
}

long get_total(int n_threads)
{
    long sum = 0;
    for (int i = 0; i < n_threads; i++)
        sum += atomic_load(&totals[i]);
    return sum;
}
```

---

## 7. Implementación interna

### __thread: ELF TLS

Las variables `__thread` se implementan usando secciones especiales
en el ELF (`.tdata` para inicializados, `.tbss` para no inicializados):

```
  ELF binary:
  ┌────────────┐
  │ .text      │  código
  │ .data      │  variables globales inicializadas
  │ .bss       │  variables globales sin inicializar
  │ .tdata     │  variables __thread inicializadas (template)
  │ .tbss      │  variables __thread sin inicializar (template)
  └────────────┘

  En runtime, para cada thread:
  ┌─────────────────────────────────┐
  │ Thread Control Block (TCB)     │
  │ ┌───────────────────────────┐  │
  │ │ Copia de .tdata + .tbss  │  │  ← copiado del template
  │ │ __thread int counter = 0  │  │
  │ │ __thread char buf[256]    │  │
  │ └───────────────────────────┘  │
  │ Stack del thread               │
  └─────────────────────────────────┘
```

El acceso a una variable `__thread` se traduce en un cálculo de
offset desde el registro de segmento (x86: `%fs`, x86_64: `%fs`):

```asm
; Acceso a __thread int counter
mov eax, DWORD PTR fs:[counter@tpoff]
; fs apunta al TLS del thread actual
; counter@tpoff es el offset de counter dentro del TLS
```

Es una sola instrucción — muy rápido, comparable a acceder a una
variable global.

### pthread_key: tabla de punteros

`pthread_getspecific` / `pthread_setspecific` usan una tabla interna
por thread:

```
  Thread 1:                    Thread 2:
  ┌────────────────────┐       ┌────────────────────┐
  │ TLS table          │       │ TLS table          │
  │ key 0: 0x1234 ─────┼──►   │ key 0: 0x5678 ─────┼──► heap data
  │ key 1: NULL        │       │ key 1: 0x9abc ─────┼──► heap data
  │ key 2: 0xabcd ─────┼──►   │ key 2: NULL        │
  │ ...                │       │ ...                │
  └────────────────────┘       └────────────────────┘
```

`pthread_getspecific` es una búsqueda indexada — O(1) pero con
overhead de llamada a función (~5-10ns vs ~1ns para `__thread`).

### Comparación de rendimiento

```
┌──────────────────────┬──────────────┬───────────────────────┐
│ Operación            │ __thread     │ pthread_getspecific    │
├──────────────────────┼──────────────┼───────────────────────┤
│ Lectura              │ ~1ns         │ ~5-10ns               │
│                      │ (1 instruc.) │ (llamada a función)   │
├──────────────────────┼──────────────┼───────────────────────┤
│ Escritura            │ ~1ns         │ ~10-15ns              │
├──────────────────────┼──────────────┼───────────────────────┤
│ Destructor automático│ No           │ Sí                    │
├──────────────────────┼──────────────┼───────────────────────┤
│ Init compleja        │ No           │ Sí (lazy init)        │
│ (malloc, open)       │              │                       │
├──────────────────────┼──────────────┼───────────────────────┤
│ Número ilimitado     │ Sí           │ No (PTHREAD_KEYS_MAX) │
├──────────────────────┼──────────────┼───────────────────────┤
│ DSO/biblioteca       │ Sí (ELF TLS) │ Sí                    │
│ dinámica             │              │                       │
└──────────────────────┴──────────────┴───────────────────────┘
```

**Regla**: usar `__thread` para datos simples (int, punteros,
arrays fijos). Usar `pthread_key` cuando necesitas destructores o
inicialización compleja.

---

## 8. TLS y fork

Después de `fork()`, el hijo tiene un solo thread (el que llamó
fork). Las variables TLS de ese thread se copian. Las de los demás
threads del padre **no existen** en el hijo:

```c
__thread int x = 0;

void *thread_func(void *arg)
{
    x = 42;
    // Si fork() ocurre en otro thread, este thread no existe en el hijo
    // x=42 de este thread se pierde
    return NULL;
}

int main(void)
{
    x = 10;
    pthread_t t;
    pthread_create(&t, NULL, thread_func, NULL);

    pid_t pid = fork();
    if (pid == 0) {
        // Hijo: x = 10 (valor del main thread que hizo fork)
        // thread_func no existe aquí, su x=42 es inaccesible
        printf("Child: x = %d\n", x);  // 10
        _exit(0);
    }
    pthread_join(t, NULL);
    waitpid(pid, NULL, 0);
}
```

Para `pthread_key`: los destructores no se ejecutan en el hijo
para las claves heredadas. Si el valor era un puntero a recurso
compartido, puede causar problemas (double free, uso de recurso
cerrado).

**Regla**: evitar `fork` en programas multi-thread. Si es necesario,
usar `pthread_atfork` para registrar handlers que limpien el estado
TLS.

---

## 9. Errores comunes

### Error 1: Retornar puntero a variable __thread

```c
// MAL: el puntero es válido solo mientras el thread vive
__thread char buffer[256];

char *get_thread_buffer(void)
{
    return buffer;  // puntero a TLS de ESTE thread
}

void *worker(void *arg)
{
    char *buf = get_thread_buffer();
    snprintf(buf, 256, "hello");
    return buf;  // ← ¡puntero a TLS que se destruye al retornar!
}

// Main:
void *retval;
pthread_join(t, &retval);
printf("%s\n", (char *)retval);  // ¡UB! TLS del thread ya no existe
```

**Por qué importa**: la memoria TLS de un thread se libera cuando el
thread termina. Un puntero a esa memoria se convierte en dangling.

### Error 2: Asumir que __thread es zero-initialized en threads creados

```c
__thread int initialized = 0;
__thread connection_t *conn = NULL;

// Esto FUNCIONA correctamente:
// Cada nuevo thread obtiene una copia fresca del template (.tdata)
// con los valores iniciales (0 y NULL respectivamente)

// Pero cuidado con static local:
void do_something(void)
{
    static __thread int first_call = 1;  // OK, pero...
    if (first_call) {
        first_call = 0;
        init_something();
    }
    // Cada thread ejecuta init_something() una vez — correcto
}
```

Las variables `__thread` se inicializan con el valor del template
ELF, que es el valor escrito en el código fuente. Si no hay
inicializador, se inicializan a cero (como `.bss`).

### Error 3: Fuga de memoria con pthread_key_delete

```c
// MAL: delete no llama destructores
pthread_key_t key;
pthread_key_create(&key, free);  // destructor = free

void *worker(void *arg) {
    pthread_setspecific(key, malloc(1024));
    return NULL;
}

// Después de que todos los workers terminen:
pthread_key_delete(key);  // NO llama free en valores no recogidos
// Si algún thread no terminó antes del delete, su buffer se pierde

// BIEN: asegurar que todos los threads terminen ANTES de key_delete
for (int i = 0; i < n; i++)
    pthread_join(threads[i], NULL);  // destructores se ejecutan aquí
pthread_key_delete(key);  // ahora seguro
```

### Error 4: Exceder PTHREAD_KEYS_MAX

```c
// MAL: crear una key por objeto
for (int i = 0; i < 10000; i++) {
    pthread_key_t key;
    if (pthread_key_create(&key, free) != 0) {
        // EAGAIN: se acabaron las claves (máx 1024 en Linux)
        perror("key_create");
    }
}

// BIEN: una key para un tipo, almacenar struct con todos los datos
typedef struct {
    int id;
    char *name;
    connection_t *conn;
    buffer_t *buf;
} thread_context_t;

pthread_key_t ctx_key;  // UNA sola key
pthread_key_create(&ctx_key, destroy_context);

void *worker(void *arg) {
    thread_context_t *ctx = calloc(1, sizeof(*ctx));
    ctx->id = (int)(intptr_t)arg;
    ctx->name = strdup("worker");
    ctx->conn = open_connection();
    ctx->buf = alloc_buffer(4096);
    pthread_setspecific(ctx_key, ctx);
    // ...
}
```

### Error 5: Tomar dirección de variable __thread entre threads

```c
// MAL: pasar puntero a __thread de un thread a otro
__thread int my_counter = 0;

void *worker(void *arg)
{
    int *parent_counter = (int *)arg;
    *parent_counter = 42;  // escribe en TLS de OTRO thread
    // Funciona si el otro thread sigue vivo, pero:
    // - Es una race condition (sin sincronización)
    // - Si el thread original termina, puntero dangling
    return NULL;
}

int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, worker, &my_counter);
    // ↑ pasa puntero al TLS del main thread
    // Thread puede escribir en nuestro TLS — funciona pero peligroso
}

// BIEN: usar variable compartida normal (global o heap), con sincronización
int shared_counter = 0;
pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
```

El propósito de TLS es **evitar** compartir. Tomar la dirección de
una variable TLS y pasarla a otro thread derrota ese propósito.

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                   THREAD-LOCAL STORAGE                             │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  __thread / _Thread_local (compile-time):                           │
│    __thread int x = 0;           // cada thread tiene su x        │
│    _Thread_local int x = 0;      // C11 estándar (equivalente)    │
│    thread_local int x = 0;       // macro C11 (<threads.h>)       │
│                                                                     │
│    ✅ Rápido (~1ns, instrucción directa)                           │
│    ✅ Sin límite de variables                                      │
│    ❌ Solo inicialización constante                                │
│    ❌ Sin destructores automáticos                                 │
│                                                                     │
│  pthread_key (runtime):                                             │
│    pthread_key_create(&key, destructor)  // crear clave global    │
│    pthread_setspecific(key, value)        // set per-thread value  │
│    pthread_getspecific(key)               // get per-thread value  │
│    pthread_key_delete(key)               // liberar clave         │
│                                                                     │
│    ✅ Destructores automáticos al terminar thread                  │
│    ✅ Init compleja (malloc, open, connect)                        │
│    ❌ Más lento (~5-10ns, llamada a función)                       │
│    ❌ Límite: PTHREAD_KEYS_MAX (1024 en Linux)                     │
│    ❌ key_delete NO llama destructores                             │
│                                                                     │
│  Cuándo usar cada uno:                                              │
│    Dato simple (int, ptr, array fijo): __thread                    │
│    Recurso que necesita cleanup (fd, malloc, conn): pthread_key    │
│    Portabilidad máxima: pthread_key                                │
│    Rendimiento en hot path: __thread                               │
│                                                                     │
│  Reglas de oro:                                                     │
│    1. No retornar puntero a variable __thread fuera del thread     │
│    2. No pasar dirección de __thread a otro thread                 │
│    3. Una pthread_key por tipo de dato, no por instancia           │
│    4. join threads ANTES de key_delete                             │
│    5. fork + TLS = problemas — evitar o usar pthread_atfork       │
│                                                                     │
│  Compilar: gcc -o prog prog.c -pthread                             │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Memory allocator per-thread

Implementa un allocator simple que mantiene un free-list per-thread
para evitar contención en malloc:

```c
#define POOL_SIZE (1024 * 1024)  // 1MB por thread
#define BLOCK_SIZE 64            // bloques de 64 bytes

typedef struct block {
    struct block *next;
} block_t;

// TLS: cada thread tiene su propio pool
__thread char pool_memory[POOL_SIZE];
__thread block_t *free_list = NULL;
__thread int pool_initialized = 0;

void pool_init(void);
void *pool_alloc(void);    // retorna bloque de BLOCK_SIZE o NULL
void pool_free(void *ptr);
```

Benchmark: N threads haciendo 1M alloc+free. Comparar con:
1. malloc/free global (con lock interno de glibc)
2. Pool per-thread con __thread
3. Pool per-thread con pthread_key (con destructor que libera el pool)

**Pregunta de reflexión**: ¿Qué pasa si un thread hace `pool_alloc`
y pasa el puntero a otro thread que hace `pool_free`? El bloque
vuelve al free-list del thread equivocado. ¿Cómo podrías detectar
o manejar esto? (Pista: jemalloc y tcmalloc resuelven este problema.)

### Ejercicio 2: Contexto de request implícito

Implementa un sistema donde cada thread de un servidor web tiene
un contexto de request implícito accesible desde cualquier función
sin pasarlo por parámetro:

```c
typedef struct {
    int request_id;
    char client_ip[46];
    time_t start_time;
    int response_status;
    char *response_body;      // heap-allocated
    FILE *access_log;         // per-thread log file
} request_context_t;

void ctx_init(int request_id, const char *client_ip);
request_context_t *ctx_get(void);
void ctx_cleanup(void);  // o automático con destructor

// Funciones que usan el contexto implícitamente:
void log_access(const char *msg);         // usa ctx->access_log
void set_response(int status, const char *body);
```

Usar `pthread_key` con destructor para cleanup automático. Simular
10 requests concurrentes y verificar que cada thread ve su propio
contexto.

**Pregunta de reflexión**: El patrón de contexto implícito (TLS)
vs explícito (pasar `ctx` por parámetro) tiene trade-offs. ¿Cuál
es más fácil de testear unitariamente? ¿Cuál hace más obvias las
dependencias? ¿Por qué frameworks como Go usan `context.Context`
explícito mientras que Java usa `ThreadLocal`?

### Ejercicio 3: Contadores distribuidos de alta performance

Implementa un sistema de contadores donde cada thread incrementa
localmente y un thread recolector suma los totales periódicamente:

```c
typedef struct {
    const char *name;
    __thread long local_count;   // ← esto no compila directamente
    // Necesitas un diseño alternativo
} counter_t;

// Diseño real:
#define MAX_COUNTERS 32
#define MAX_THREADS  64

typedef struct {
    long counts[MAX_THREADS];  // padded to cache line
    const char *name;
} dist_counter_t;

__thread int my_thread_slot = -1;

void counter_inc(dist_counter_t *c);
long counter_read(dist_counter_t *c);  // suma todos los slots
void counter_reset(dist_counter_t *c);
```

Requisitos:
- `counter_inc` debe ser lock-free y cache-friendly
- Padding de 64 bytes entre slots para evitar false sharing
- Comparar throughput con `_Atomic long` simple

**Pregunta de reflexión**: ¿Por qué padding de 64 bytes? ¿Qué es
false sharing y cómo un `_Atomic long counts[64]` sin padding
causa contención de cache incluso si cada thread escribe en su
propio slot? Investiga `__attribute__((aligned(64)))`.
