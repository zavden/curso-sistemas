# pthread_create y pthread_join

## Índice
1. [Procesos vs hilos](#1-procesos-vs-hilos)
2. [El modelo de threads en Linux](#2-el-modelo-de-threads-en-linux)
3. [pthread_create](#3-pthread_create)
4. [pthread_join](#4-pthread_join)
5. [Paso de argumentos](#5-paso-de-argumentos)
6. [Retorno de valores](#6-retorno-de-valores)
7. [Threads detached](#7-threads-detached)
8. [Atributos de thread: pthread_attr_t](#8-atributos-de-thread-pthread_attr_t)
9. [Identidad y self](#9-identidad-y-self)
10. [Cancelación](#10-cancelación)
11. [Terminación de threads](#11-terminación-de-threads)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Procesos vs hilos

Un proceso tiene su propio espacio de direcciones: memoria, file
descriptors, signal handlers, PID. Crear un proceso con `fork()` copia
todo esto. Un **hilo** (thread) comparte todo con los demás hilos del
mismo proceso, excepto su propia pila y registros:

```
  Proceso con 3 hilos:

  ┌─────────────────────────────────────────────────────┐
  │                    Proceso (PID)                    │
  │                                                     │
  │  Compartido por todos los hilos:                    │
  │  ┌─────────────────────────────────────────────┐   │
  │  │ Código (.text)                              │   │
  │  │ Datos globales (.data, .bss, heap)          │   │
  │  │ File descriptors                            │   │
  │  │ Signal handlers (disposición)               │   │
  │  │ Directorio de trabajo (cwd)                 │   │
  │  │ User/Group IDs                              │   │
  │  │ Espacio de direcciones (mmap, shm)          │   │
  │  └─────────────────────────────────────────────┘   │
  │                                                     │
  │  Privado por hilo:                                  │
  │  ┌──────────┐  ┌──────────┐  ┌──────────┐         │
  │  │ Thread 1 │  │ Thread 2 │  │ Thread 3 │         │
  │  │          │  │          │  │          │         │
  │  │ Stack    │  │ Stack    │  │ Stack    │         │
  │  │ Registros│  │ Registros│  │ Registros│         │
  │  │ TID      │  │ TID      │  │ TID      │         │
  │  │ errno    │  │ errno    │  │ errno    │         │
  │  │ Sig mask │  │ Sig mask │  │ Sig mask │         │
  │  │ TLS      │  │ TLS      │  │ TLS      │         │
  │  └──────────┘  └──────────┘  └──────────┘         │
  └─────────────────────────────────────────────────────┘
```

### Tabla comparativa

```
┌────────────────────┬────────────────────┬────────────────────┐
│ Aspecto            │ fork() (proceso)   │ pthread (hilo)     │
├────────────────────┼────────────────────┼────────────────────┤
│ Creación           │ ~100µs (COW)       │ ~10µs              │
├────────────────────┼────────────────────┼────────────────────┤
│ Memoria            │ Copia (COW)        │ Compartida         │
├────────────────────┼────────────────────┼────────────────────┤
│ Comunicación       │ IPC (pipes, shm)   │ Variables globales │
├────────────────────┼────────────────────┼────────────────────┤
│ Aislamiento        │ Total              │ Ninguno            │
├────────────────────┼────────────────────┼────────────────────┤
│ Crash de un hilo/  │ Solo muere el      │ Muere TODO el      │
│ proceso            │ hijo               │ proceso            │
├────────────────────┼────────────────────┼────────────────────┤
│ File descriptors   │ Copia independiente│ Compartidos        │
├────────────────────┼────────────────────┼────────────────────┤
│ Señales            │ Independientes     │ Compartidas        │
│                    │                    │ (disposición)      │
│                    │                    │ Máscara por thread │
├────────────────────┼────────────────────┼────────────────────┤
│ Context switch     │ ~5µs (TLB flush)   │ ~1µs (mismo AS)    │
└────────────────────┴────────────────────┴────────────────────┘
```

**Cuándo usar threads**: tareas que necesitan compartir datos en
memoria de forma eficiente, I/O multiplexado, paralelismo de cómputo.

**Cuándo usar procesos**: aislamiento de fallos, ejecutar programas
externos, seguridad (privilege separation).

---

## 2. El modelo de threads en Linux

Linux no distingue entre procesos y threads a nivel de kernel. Ambos
son **tareas** (`task_struct`). La diferencia es qué comparten:

```
  fork():   clone(SIGCHLD)
            → nueva tarea con COPIA de mm, files, fs, signal

  pthread:  clone(CLONE_VM | CLONE_FS | CLONE_FILES |
                  CLONE_SIGHAND | CLONE_THREAD | ...)
            → nueva tarea que COMPARTE mm, files, fs, signal
```

```
┌───────────────┬──────────────────────────────────────────┐
│ Flag clone()  │ Qué comparte                             │
├───────────────┼──────────────────────────────────────────┤
│ CLONE_VM      │ Espacio de direcciones (memoria virtual) │
│ CLONE_FS      │ cwd, root, umask                         │
│ CLONE_FILES   │ Tabla de file descriptors                │
│ CLONE_SIGHAND │ Tabla de signal handlers                 │
│ CLONE_THREAD  │ Thread group (mismo PID visto desde fuera│
│               │ pero TID distinto)                       │
└───────────────┴──────────────────────────────────────────┘
```

Cada thread tiene su propio **TID** (Thread ID), visible en
`/proc/PID/task/TID/`. La función `gettid()` retorna el TID.
`getpid()` retorna el mismo PID para todos los threads del grupo.

```bash
# Ver threads de un proceso
$ ls /proc/1234/task/
1234  1235  1236    # TIDs

$ cat /proc/1234/status | grep Threads
Threads: 3

# ps con threads
$ ps -T -p 1234
  PID  SPID TTY          TIME CMD
 1234  1234 pts/0    00:00:01 myapp
 1234  1235 pts/0    00:00:00 myapp
 1234  1236 pts/0    00:00:00 myapp
```

### NPTL (Native POSIX Threads Library)

Linux usa NPTL (desde glibc 2.3.2 / kernel 2.6), que implementa threads
como tareas del kernel (1:1 model). Cada pthread es un kernel thread.
El modelo anterior (LinuxThreads) tenía problemas con señales y PID.

---

## 3. pthread_create

```c
#include <pthread.h>

int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
                   void *(*start_routine)(void *), void *arg);
// thread: recibe el ID del nuevo thread
// attr: atributos (NULL = defaults)
// start_routine: función que ejecutará el thread
// arg: argumento para start_routine
// Retorna: 0 en éxito, o código de error (NO usa errno)
```

**Importante**: las funciones pthread retornan el código de error
directamente (no usan `errno`). El valor retornado es 0 en éxito o
un código positivo de error (EAGAIN, EINVAL, EPERM, etc.).

### Ejemplo mínimo

```c
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

void *thread_func(void *arg)
{
    printf("Thread running! Arg: %s\n", (char *)arg);
    sleep(1);
    printf("Thread done!\n");
    return NULL;
}

int main(void)
{
    pthread_t tid;
    int err = pthread_create(&tid, NULL, thread_func, "hello");
    if (err != 0) {
        fprintf(stderr, "pthread_create: %s\n", strerror(err));
        return 1;
    }

    printf("Main: thread created (tid=%lu)\n", (unsigned long)tid);

    // Esperar a que termine
    err = pthread_join(tid, NULL);
    if (err != 0) {
        fprintf(stderr, "pthread_join: %s\n", strerror(err));
        return 1;
    }

    printf("Main: thread joined\n");
    return 0;
}
```

Compilar siempre con `-pthread`:

```bash
gcc -o threads threads.c -pthread
```

El flag `-pthread` hace dos cosas: define `_REENTRANT` y enlaza con
`-lpthread`. Usar `-pthread` en vez de solo `-lpthread` es la forma
correcta.

### Orden de ejecución

No hay garantía de quién ejecuta primero — el main thread o el nuevo
thread. El scheduler del kernel decide:

```c
pthread_create(&tid, NULL, func, NULL);
printf("A\n");  // ¿se imprime antes o después de "B" en func?

void *func(void *arg) {
    printf("B\n");
    return NULL;
}

// Posible: A B   o   B A   — no determinista
```

---

## 4. pthread_join

```c
int pthread_join(pthread_t thread, void **retval);
// thread: ID del thread a esperar
// retval: si no es NULL, recibe el valor retornado por el thread
// Retorna: 0 en éxito, código de error en fallo
```

`pthread_join` es el equivalente de `waitpid` para threads. Bloquea
hasta que el thread especificado termine. Es esencial por dos razones:

1. **Recolectar recursos**: un thread terminado que no ha sido joined
   es un "zombie thread" — su stack y estructura interna no se liberan.
2. **Sincronización**: saber que el trabajo del thread terminó antes
   de usar sus resultados.

```c
// Esperar múltiples threads
#define N_THREADS 4
pthread_t threads[N_THREADS];

// Crear todos
for (int i = 0; i < N_THREADS; i++) {
    pthread_create(&threads[i], NULL, worker, NULL);
}

// Esperar a todos
for (int i = 0; i < N_THREADS; i++) {
    pthread_join(threads[i], NULL);
}
printf("All threads finished\n");
```

### Reglas de pthread_join

- Un thread solo puede ser joined **una vez**. Llamar join dos veces
  es comportamiento indefinido.
- Un thread solo puede ser joined por **un** thread. Si dos threads
  intentan join al mismo, el comportamiento es indefinido.
- No puedes hacer join a ti mismo (`pthread_join(pthread_self(), NULL)`
  → `EDEADLK`).
- Solo threads **joinable** pueden ser joined (ver detached abajo).

### Sin join: fugas de memoria

```c
// MAL: crear threads sin join ni detach
void *func(void *arg) {
    // trabajo
    return NULL;
}

int main(void)
{
    for (int i = 0; i < 1000; i++) {
        pthread_t t;
        pthread_create(&t, NULL, func, NULL);
        // ¡No join ni detach! → stack (~8MB) no se libera
    }
    // Fuga: ~8GB de stacks de threads zombies
}
```

---

## 5. Paso de argumentos

El parámetro `arg` de `pthread_create` es `void *`. Hay varias formas
de pasar datos:

### 5.1 Puntero a dato existente (cuidado con el lifetime)

```c
void *print_num(void *arg)
{
    int *num = (int *)arg;
    printf("Number: %d\n", *num);
    return NULL;
}

int main(void)
{
    int value = 42;
    pthread_t t;
    pthread_create(&t, NULL, print_num, &value);
    // ¡value debe seguir vivo hasta que el thread lo lea!
    pthread_join(t, NULL);
}
```

### 5.2 Error clásico: puntero a variable del loop

```c
// MAL: todos los threads ven el mismo puntero
#define N 5
pthread_t threads[N];
for (int i = 0; i < N; i++) {
    pthread_create(&threads[i], NULL, func, &i);
    // &i apunta siempre a la misma variable
    // Cuando el thread lee *arg, i ya puede ser 1, 2, 3, 4 o 5
}
// Resultado probable: varios threads imprimen "5"

// BIEN: copiar el valor con cast a intptr_t
for (int i = 0; i < N; i++) {
    pthread_create(&threads[i], NULL, func, (void *)(intptr_t)i);
}
// En el thread:
void *func(void *arg) {
    int my_i = (int)(intptr_t)arg;  // valor copiado, no puntero
    printf("I am thread %d\n", my_i);
    return NULL;
}
```

### 5.3 Struct para múltiples argumentos

```c
typedef struct {
    int id;
    const char *name;
    double value;
} thread_arg_t;

void *worker(void *arg)
{
    thread_arg_t *ta = (thread_arg_t *)arg;
    printf("Thread %d (%s): value=%.2f\n", ta->id, ta->name, ta->value);
    return NULL;
}

int main(void)
{
    thread_arg_t args[3] = {
        {0, "alpha", 1.1},
        {1, "beta",  2.2},
        {2, "gamma", 3.3},
    };

    pthread_t threads[3];
    for (int i = 0; i < 3; i++) {
        pthread_create(&threads[i], NULL, worker, &args[i]);
        // Cada thread tiene su propio args[i] — seguro
    }

    for (int i = 0; i < 3; i++)
        pthread_join(threads[i], NULL);
}
```

### 5.4 Struct en el heap (thread es dueño)

Cuando el creador no puede mantener los datos vivos:

```c
void *worker(void *arg)
{
    thread_arg_t *ta = (thread_arg_t *)arg;
    // ... usar ta ...
    free(ta);  // el thread libera su propia copia
    return NULL;
}

// Crear thread con argumento dinámico
thread_arg_t *ta = malloc(sizeof(*ta));
ta->id = i;
ta->name = strdup("worker");
ta->value = 3.14;
pthread_create(&t, NULL, worker, ta);
// No hacer free aquí — el thread lo hace
```

---

## 6. Retorno de valores

El thread retorna un `void *` desde su función, y `pthread_join` lo
recoge:

### 6.1 Cast directo (valores pequeños)

```c
void *compute(void *arg)
{
    int input = (int)(intptr_t)arg;
    int result = input * input;
    return (void *)(intptr_t)result;
}

int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, compute, (void *)(intptr_t)7);

    void *retval;
    pthread_join(t, &retval);
    int result = (int)(intptr_t)retval;
    printf("Result: %d\n", result);  // 49
}
```

### 6.2 Struct en el heap

```c
typedef struct {
    double sum;
    int count;
} result_t;

void *process(void *arg)
{
    result_t *res = malloc(sizeof(*res));
    res->sum = 3.14;
    res->count = 42;
    return res;  // caller hará free
}

int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, process, NULL);

    void *retval;
    pthread_join(t, &retval);
    result_t *res = (result_t *)retval;
    printf("Sum=%.2f, Count=%d\n", res->sum, res->count);
    free(res);
}
```

### 6.3 NUNCA retornar puntero a variable local

```c
// MAL: la pila del thread se destruye al retornar
void *bad_func(void *arg)
{
    int result = 42;
    return &result;  // ← ¡puntero a variable local de un stack que se destruye!
}

int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, bad_func, NULL);
    void *retval;
    pthread_join(t, &retval);
    int *p = (int *)retval;
    printf("%d\n", *p);  // ¡comportamiento indefinido! Stack ya liberado
}
```

### 6.4 Valor especial: PTHREAD_CANCELED

Si el thread fue cancelado (ver sección 10), `pthread_join` recibe
`PTHREAD_CANCELED` como retval:

```c
void *retval;
pthread_join(t, &retval);
if (retval == PTHREAD_CANCELED) {
    printf("Thread was canceled\n");
}
```

---

## 7. Threads detached

Un thread **detached** libera sus recursos automáticamente al terminar,
sin necesidad de `pthread_join`. No puedes hacer join a un thread
detached.

### Crear detached

```c
// Opción 1: detach después de crear
pthread_t t;
pthread_create(&t, NULL, func, NULL);
pthread_detach(t);  // no necesitamos join

// Opción 2: crear ya detached con atributos
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

pthread_t t;
pthread_create(&t, &attr, func, NULL);
pthread_attr_destroy(&attr);
// t es detached desde el inicio

// Opción 3: el thread se detacha a sí mismo
void *func(void *arg)
{
    pthread_detach(pthread_self());
    // ... trabajo ...
    return NULL;  // recursos se liberan automáticamente
}
```

### Joinable vs Detached

```
┌─────────────────┬────────────────────┬────────────────────┐
│ Aspecto         │ Joinable (default) │ Detached           │
├─────────────────┼────────────────────┼────────────────────┤
│ Recursos al     │ Retenidos hasta    │ Liberados          │
│ terminar        │ pthread_join       │ automáticamente    │
├─────────────────┼────────────────────┼────────────────────┤
│ pthread_join    │ Obligatorio        │ Prohibido (EINVAL) │
├─────────────────┼────────────────────┼────────────────────┤
│ Retorno de valor│ Sí (via join)      │ No                 │
├─────────────────┼────────────────────┼────────────────────┤
│ Cuándo usar     │ Necesitas el       │ Fire-and-forget,   │
│                 │ resultado o saber  │ daemon threads,    │
│                 │ cuándo terminó     │ event handlers     │
└─────────────────┴────────────────────┴────────────────────┘
```

### Patrón: servidor con threads detached

```c
void *handle_client(void *arg)
{
    int fd = (int)(intptr_t)arg;
    pthread_detach(pthread_self());

    // Procesar cliente...
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));
    // ...

    close(fd);
    return NULL;
}

// Accept loop
while (1) {
    int client_fd = accept(listen_fd, NULL, NULL);
    if (client_fd == -1) continue;

    pthread_t t;
    pthread_create(&t, NULL, handle_client,
                   (void *)(intptr_t)client_fd);
    // No join — el thread se limpia solo
}
```

---

## 8. Atributos de thread: pthread_attr_t

Los atributos controlan las propiedades del thread al crearlo:

```c
pthread_attr_t attr;
pthread_attr_init(&attr);    // inicializar con defaults

// Configurar...
pthread_attr_setXXX(&attr, value);

// Crear thread con atributos
pthread_create(&t, &attr, func, arg);

// Liberar atributos (el thread no se ve afectado)
pthread_attr_destroy(&attr);
```

### Atributos disponibles

```
┌─────────────────────────┬─────────────────────────────────────────┐
│ Atributo                │ Descripción                             │
├─────────────────────────┼─────────────────────────────────────────┤
│ detachstate             │ PTHREAD_CREATE_JOINABLE (default)       │
│                         │ PTHREAD_CREATE_DETACHED                 │
├─────────────────────────┼─────────────────────────────────────────┤
│ stacksize               │ Tamaño del stack (default ~8MB)         │
├─────────────────────────┼─────────────────────────────────────────┤
│ stackaddr               │ Dirección del stack (avanzado)          │
├─────────────────────────┼─────────────────────────────────────────┤
│ guardsize               │ Zona de guarda al final del stack       │
│                         │ (default PAGE_SIZE, detecta overflow)   │
├─────────────────────────┼─────────────────────────────────────────┤
│ schedpolicy             │ SCHED_OTHER, SCHED_FIFO, SCHED_RR      │
├─────────────────────────┼─────────────────────────────────────────┤
│ schedparam              │ Prioridad (para FIFO/RR)                │
├─────────────────────────┼─────────────────────────────────────────┤
│ inheritsched            │ Heredar scheduling del creador o usar   │
│                         │ el configurado en attr                  │
└─────────────────────────┴─────────────────────────────────────────┘
```

### Configurar tamaño de stack

El default es ~8MB (consultable con `ulimit -s`). Para muchos threads,
esto desperdicia espacio de direcciones virtual:

```c
pthread_attr_t attr;
pthread_attr_init(&attr);

// Stack de 64KB (suficiente si no usas arrays grandes en el stack)
pthread_attr_setstacksize(&attr, 64 * 1024);

// Verificar
size_t stacksize;
pthread_attr_getstacksize(&attr, &stacksize);
printf("Stack size: %zu bytes\n", stacksize);

// El mínimo es PTHREAD_STACK_MIN (normalmente 16384)
```

Para crear miles de threads (e.g., servidor con un thread por conexión),
reducir el stack es esencial:

```c
// 1000 threads × 8MB = 8GB de espacio virtual
// 1000 threads × 64KB = 64MB de espacio virtual
```

### Consultar atributos de un thread existente

```c
pthread_attr_t attr;
pthread_getattr_np(pthread_self(), &attr);  // _np = non-portable (Linux)

size_t stacksize;
pthread_attr_getstacksize(&attr, &stacksize);

void *stackaddr;
pthread_attr_getstackaddr(&attr, &stackaddr);

int detachstate;
pthread_attr_getdetachstate(&attr, &detachstate);

printf("Stack: %p, size: %zu, detached: %s\n",
       stackaddr, stacksize,
       detachstate == PTHREAD_CREATE_DETACHED ? "yes" : "no");

pthread_attr_destroy(&attr);
```

---

## 9. Identidad y self

### pthread_self — obtener tu propio ID

```c
pthread_t pthread_self(void);
```

`pthread_t` es un tipo opaco. En Linux/glibc es un `unsigned long`,
pero en otros sistemas puede ser un struct. Nunca comparar con `==`:

```c
// MAL: comparar pthread_t con ==
if (tid1 == tid2)  // no portátil

// BIEN: usar pthread_equal
if (pthread_equal(tid1, tid2)) {
    printf("Same thread\n");
}
```

### gettid — Thread ID del kernel (Linux)

```c
#include <sys/types.h>
#include <unistd.h>
#include <sys/syscall.h>

pid_t gettid(void);  // desde glibc 2.30
// En sistemas más antiguos:
// pid_t tid = syscall(SYS_gettid);
```

`pthread_self()` retorna el ID del espacio de usuario (opaco).
`gettid()` retorna el TID del kernel (un entero, visible en `/proc`).
Son cosas diferentes:

```c
void *func(void *arg)
{
    printf("pthread_self: %lu\n", (unsigned long)pthread_self());
    printf("gettid:       %d\n", gettid());
    printf("getpid:       %d\n", getpid());  // mismo para todos
    return NULL;
}
```

---

## 10. Cancelación

Puedes pedir que un thread termine prematuramente:

```c
int pthread_cancel(pthread_t thread);
// Envía una solicitud de cancelación
```

La cancelación no es inmediata — el thread la procesa en un **cancellation
point** (funciones como `read`, `write`, `sleep`, `pthread_cond_wait`,
`sem_wait`, etc.):

```c
void *worker(void *arg)
{
    while (1) {
        // sleep es un cancellation point
        sleep(1);  // ← aquí se puede cancelar
        printf("Working...\n");
    }
    return NULL;
}

int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, worker, NULL);

    sleep(3);
    pthread_cancel(t);   // solicitar cancelación

    void *retval;
    pthread_join(t, &retval);
    if (retval == PTHREAD_CANCELED)
        printf("Thread was canceled\n");
}
```

### Tipos de cancelación

```c
// Controlar cuándo se procesa la cancelación
int pthread_setcancelstate(int state, int *oldstate);
// PTHREAD_CANCEL_ENABLE (default) — cancelación habilitada
// PTHREAD_CANCEL_DISABLE — cancelación diferida (se acumula)

int pthread_setcanceltype(int type, int *oldtype);
// PTHREAD_CANCEL_DEFERRED (default) — en cancellation points
// PTHREAD_CANCEL_ASYNCHRONOUS — en cualquier momento (¡peligroso!)
```

### Cleanup handlers

Registrar funciones que se ejecutan si el thread es cancelado (como
un `finally` en otros lenguajes):

```c
void cleanup_mutex(void *arg)
{
    pthread_mutex_t *mtx = (pthread_mutex_t *)arg;
    pthread_mutex_unlock(mtx);
}

void *worker(void *arg)
{
    pthread_mutex_t *mtx = (pthread_mutex_t *)arg;

    pthread_mutex_lock(mtx);
    pthread_cleanup_push(cleanup_mutex, mtx);

    // Si el thread es cancelado aquí (e.g., en una read),
    // cleanup_mutex se ejecuta → el mutex se libera
    read(fd, buf, sizeof(buf));  // cancellation point

    pthread_cleanup_pop(1);  // 1 = ejecutar el cleanup al salir
    // 0 = solo desregistrar sin ejecutar
}
```

`pthread_cleanup_push` y `pthread_cleanup_pop` deben usarse en pares
y en el mismo scope léxico (son macros que abren y cierran `{}`).

---

## 11. Terminación de threads

Un thread termina cuando:

1. **Retorna** de su función start: `return value;`
2. **Llama `pthread_exit`**: `pthread_exit(value);`
3. **Es cancelado**: `pthread_cancel(thread);`
4. **Otro thread del proceso llama `exit()`**: termina todo el proceso
5. **El main thread retorna de `main()`**: termina todo el proceso

### pthread_exit vs return

```c
void *func(void *arg)
{
    // Estas son equivalentes:
    return (void *)(intptr_t)42;
    // o:
    pthread_exit((void *)(intptr_t)42);
}
```

Diferencia clave: si el **main thread** llama `pthread_exit` en vez de
`return`, el proceso **no termina** — los demás threads siguen ejecutando:

```c
int main(void)
{
    pthread_t t;
    pthread_create(&t, NULL, long_running_func, NULL);

    // return 0;  ← esto mata a todos los threads
    pthread_exit(NULL);  // ← main termina pero el thread sigue
}
```

Esto es útil cuando main no necesita esperar a los threads pero no
quiere matarlos. El proceso termina cuando el último thread termina.

### exit() vs pthread_exit() vs _exit()

```
┌─────────────────┬────────────────────────────────────────────┐
│ Función         │ Efecto                                     │
├─────────────────┼────────────────────────────────────────────┤
│ return (main)   │ Llama exit() → atexit handlers → termina  │
│                 │ todo el proceso                            │
├─────────────────┼────────────────────────────────────────────┤
│ exit()          │ Ejecuta atexit handlers, cierra stdio,    │
│                 │ termina TODO el proceso (todos los threads)│
├─────────────────┼────────────────────────────────────────────┤
│ _exit()         │ Termina el proceso inmediatamente (sin     │
│                 │ atexit, sin flush)                         │
├─────────────────┼────────────────────────────────────────────┤
│ pthread_exit()  │ Termina SOLO el thread actual. Si es el   │
│                 │ último, termina el proceso.                │
├─────────────────┼────────────────────────────────────────────┤
│ return (thread) │ Equivale a pthread_exit(valor)             │
└─────────────────┴────────────────────────────────────────────┘
```

---

## 12. Errores comunes

### Error 1: Puntero a variable del loop como argumento

```c
// MAL: todos los threads comparten &i
for (int i = 0; i < N; i++) {
    pthread_create(&threads[i], NULL, func, &i);
}
// Cuando el thread lee *arg, i puede ser cualquier valor

// BIEN: pasar por valor con cast
for (int i = 0; i < N; i++) {
    pthread_create(&threads[i], NULL, func, (void *)(intptr_t)i);
}
```

**Por qué importa**: es el bug más frecuente en código con threads. Los
threads comparten memoria, así que `&i` es la misma dirección para todos.
El valor de `i` cambia antes de que el thread lo lea.

### Error 2: No hacer join ni detach

```c
// MAL: thread zombie — stack no se libera
pthread_t t;
pthread_create(&t, NULL, func, NULL);
// Ni join ni detach → ~8MB de stack perdidos por thread

// BIEN: una de las dos opciones
pthread_join(t, NULL);        // esperar
// o:
pthread_detach(t);            // fire-and-forget
```

**Por qué importa**: en un servidor que crea un thread por conexión
sin detach/join, eventualmente se agota la memoria virtual.

### Error 3: Retornar puntero a variable local del thread

```c
// MAL: el stack del thread se destruye al retornar
void *func(void *arg)
{
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "result: %d", 42);
    return buffer;  // ← puntero a stack que se destruye
}

// BIEN: usar heap
void *func(void *arg)
{
    char *buffer = malloc(256);
    snprintf(buffer, 256, "result: %d", 42);
    return buffer;  // caller hace free
}
```

### Error 4: Olvidar -pthread al compilar

```bash
# MAL: compila pero el comportamiento es indefinido
gcc -o prog prog.c -lpthread

# BIEN: -pthread activa defines necesarios + enlaza
gcc -o prog prog.c -pthread
```

Sin `-pthread`, macros como `_REENTRANT` no se definen, y algunas
funciones de biblioteca pueden no ser thread-safe.

### Error 5: Asumir orden de ejecución

```c
// MAL: asumir que el thread ya ejecutó antes de continuar
int shared_data = 0;

void *func(void *arg) {
    shared_data = 42;
    return NULL;
}

pthread_create(&t, NULL, func, NULL);
printf("Data: %d\n", shared_data);  // ¿0 o 42? No determinista
// BIEN: usar join para sincronizar
pthread_join(t, NULL);
printf("Data: %d\n", shared_data);  // siempre 42
```

---

## 13. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                 PTHREAD_CREATE Y PTHREAD_JOIN                      │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Crear y esperar:                                                   │
│    pthread_create(&tid, NULL, func, arg)   // crear thread         │
│    pthread_join(tid, &retval)              // esperar + recoger    │
│    pthread_detach(tid)                     // fire-and-forget      │
│                                                                     │
│  Identidad:                                                         │
│    pthread_self()           // mi pthread_t (opaco)                │
│    pthread_equal(t1, t2)   // comparar threads                    │
│    gettid()                // TID del kernel (Linux)               │
│                                                                     │
│  Pasar argumentos:                                                  │
│    Un int:    (void *)(intptr_t)i    // en thread: (intptr_t)arg  │
│    Struct:    &args[i]               // array o heap               │
│    ¡NUNCA!    &i del loop            // race condition             │
│                                                                     │
│  Retornar valores:                                                  │
│    Un int:    return (void *)(intptr_t)result                      │
│    Struct:    return malloc'd_struct  // caller hace free          │
│    ¡NUNCA!    return &local_var      // stack destruido            │
│                                                                     │
│  Terminación:                                                       │
│    return val        — termina thread, retorna val                 │
│    pthread_exit(val) — termina thread, retorna val                 │
│    exit()            — termina TODO el proceso                     │
│    pthread_cancel(t) — solicitar cancelación                       │
│                                                                     │
│  Atributos:                                                         │
│    pthread_attr_init(&attr)                                         │
│    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED)     │
│    pthread_attr_setstacksize(&attr, 64*1024)                       │
│    pthread_create(&t, &attr, func, arg)                            │
│    pthread_attr_destroy(&attr)                                      │
│                                                                     │
│  Cancelación:                                                       │
│    pthread_cancel(t)                  // solicitar                  │
│    pthread_setcancelstate(DISABLE/ENABLE, &old)                    │
│    pthread_cleanup_push(func, arg)    // registrar cleanup         │
│    pthread_cleanup_pop(execute)       // desregistrar              │
│                                                                     │
│  Compilar: gcc -o prog prog.c -pthread                             │
│                                                                     │
│  Reglas de oro:                                                     │
│    1. Siempre join O detach — nunca ni uno ni otro                 │
│    2. Nunca pasar &loop_var — pasar por valor con cast             │
│    3. Nunca retornar &local — usar heap                            │
│    4. Compilar con -pthread, no solo -lpthread                     │
│    5. No asumir orden de ejecución                                 │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 14. Ejercicios

### Ejercicio 1: Suma paralela de un array

Divide un array de N enteros en T segmentos y crea un thread por
segmento. Cada thread calcula la suma parcial de su segmento y la
retorna via `pthread_join`. El main thread suma las parciales:

```c
#define N 10000000
#define T 4

typedef struct {
    const int *data;
    size_t start;
    size_t end;
} segment_t;

void *sum_segment(void *arg);
```

Requisitos:
- Manejar el caso donde N no es divisible por T (último segmento
  más corto)
- Retornar la suma parcial como `(void *)(intptr_t)sum` si cabe en
  long, o como struct malloced si no
- Medir tiempo con `clock_gettime(CLOCK_MONOTONIC)` y comparar T=1
  vs T=4 vs T=8

**Pregunta de reflexión**: Si el array es de 10M enteros y usas 4
threads, ¿la mejora es 4x? ¿Qué factores limitan el speedup (overhead
de creación, cache lines compartidas, memory bandwidth)?

### Ejercicio 2: Thread pool simple

Implementa un pool de N threads reutilizables que procesan trabajos
de una cola. En este ejercicio, la "cola" puede ser un simple array
con un índice, protegido por un mutex (se verá en el siguiente tema):

```c
typedef struct {
    void (*function)(void *arg);
    void *arg;
} job_t;

typedef struct {
    pthread_t *threads;
    int n_threads;
    job_t *jobs;
    int n_jobs;
    int next_job;        // índice del próximo job
    pthread_mutex_t mtx; // proteger next_job
} thread_pool_t;

thread_pool_t *pool_create(int n_threads, job_t *jobs, int n_jobs);
void pool_wait(thread_pool_t *pool);
void pool_destroy(thread_pool_t *pool);
```

Nota: este ejercicio usa `pthread_mutex_t`, que se cubre en T03. Si
prefieres, usa `_Atomic int next_job` con `atomic_fetch_add` como
sincronización mínima.

**Pregunta de reflexión**: ¿Qué ventaja tiene un thread pool sobre
crear un thread nuevo para cada tarea? Piensa en el overhead de
`pthread_create` (~10µs) si tienes 100000 tareas que toman 1µs cada una.

### Ejercicio 3: Parallel file search

Crea un programa que busque un string en múltiples archivos en
paralelo. Cada thread procesa un archivo y reporta las líneas que
contienen el string:

```c
typedef struct {
    int thread_id;
    const char *filename;
    const char *pattern;
    int matches;          // resultado: número de coincidencias
} search_arg_t;

void *search_file(void *arg);
```

Requisitos:
- Recibir por argv: patrón y lista de archivos
- Un thread por archivo (hasta un máximo de 8)
- Si hay más de 8 archivos, los threads restantes esperan a que
  uno termine (o implementar un pool simple)
- Thread-safe: cada thread imprime sus resultados con el nombre del
  archivo y el número de línea (las líneas pueden intercalarse)
- Al final, el main thread imprime el total de coincidencias

**Pregunta de reflexión**: Si dos threads imprimen al mismo tiempo con
`printf`, ¿las líneas se mezclan carácter por carácter o línea por
línea? ¿Qué dice POSIX sobre la atomicidad de `printf`? ¿Cómo podrías
garantizar que las salidas no se entremezclan?
