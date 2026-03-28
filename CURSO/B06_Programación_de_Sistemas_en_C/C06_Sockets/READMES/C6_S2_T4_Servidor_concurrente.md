# Servidor concurrente

## Índice
1. [El problema del servidor iterativo](#1-el-problema-del-servidor-iterativo)
2. [Estrategia 1: fork por conexión](#2-estrategia-1-fork-por-conexión)
3. [Estrategia 2: thread por conexión](#3-estrategia-2-thread-por-conexión)
4. [Estrategia 3: pre-fork (pool de procesos)](#4-estrategia-3-pre-fork-pool-de-procesos)
5. [Estrategia 4: thread pool](#5-estrategia-4-thread-pool)
6. [Estrategia 5: event loop single-threaded](#6-estrategia-5-event-loop-single-threaded)
7. [Estrategia 6: event loop + thread pool (híbrido)](#7-estrategia-6-event-loop--thread-pool-híbrido)
8. [Comparación de estrategias](#8-comparación-de-estrategias)
9. [Servidores reales: qué usan](#9-servidores-reales-qué-usan)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El problema del servidor iterativo

Un servidor iterativo atiende **un cliente a la vez**. Mientras
procesa al cliente A, los clientes B, C, D esperan en la cola de
`listen()`:

```
Servidor iterativo:

Cliente A: ══════════════╗
Cliente B: ──────────────╬══════════════╗
Cliente C: ──────────────╬─────────────╬══════════════╗
                         ║             ║              ║
                    accept+process  accept+process  accept+process
                    (A bloquea      (B bloquea
                     a B y C)        a C)
```

```
Servidor concurrente:

Cliente A: ══════════════╗
Cliente B: ══════════════╗  (en paralelo)
Cliente C: ══════════════╗  (en paralelo)
```

### ¿Qué significa "concurrente"?

- **Concurrente**: múltiples clientes progresando al mismo tiempo
  (pueden ser paralelos o multiplexados en un solo core)
- **Paralelo**: múltiples clientes ejecutándose literalmente al
  mismo tiempo en múltiples CPUs

Un servidor con epoll en un solo thread es **concurrente** pero no
paralelo. Un servidor con un thread por conexión puede ser ambos.

---

## 2. Estrategia 1: fork por conexión

La forma más antigua y simple de Unix. Cada conexión nueva se maneja
en un **proceso hijo** independiente:

```
                    ┌──────────────────┐
                    │    Padre         │
                    │  listen+accept   │
                    └──────┬───────────┘
                           │ fork()
              ┌────────────┼────────────┐
              ▼            ▼            ▼
         ┌────────┐   ┌────────┐   ┌────────┐
         │ Hijo 1 │   │ Hijo 2 │   │ Hijo 3 │
         │ fd=5   │   │ fd=6   │   │ fd=7   │
         │ (A)    │   │ (B)    │   │ (C)    │
         └────────┘   └────────┘   └────────┘
```

### Implementación

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT     8080
#define BUF_SIZE 4096

// Recolectar zombies sin bloquear
void sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

void handle_client(int client_fd) {
    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
        write(client_fd, buf, n);  // echo
    }
    close(client_fd);
    _exit(0);
}

int main(void) {
    // Recoger hijos automáticamente
    struct sigaction sa = {
        .sa_handler = sigchld_handler,
        .sa_flags   = SA_RESTART | SA_NOCLDSTOP
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);
    printf("Fork server on port %d\n", PORT);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd == -1) continue;

        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // Hijo: no necesita el listening socket
            close(listen_fd);
            handle_client(client_fd);
            // handle_client llama _exit, pero por si acaso:
            _exit(0);
        }

        // Padre: no necesita el client socket
        close(client_fd);
    }

    return 0;
}
```

### Puntos clave

| Aspecto | Detalle |
|---------|---------|
| Aislamiento | Total — un hijo crasheando no afecta al padre ni a otros |
| Zombies | **Obligatorio** recogerlos con `SIGCHLD` + `waitpid(WNOHANG)` |
| close() en padre | El padre DEBE cerrar `client_fd` (si no, el fd nunca se libera) |
| close() en hijo | El hijo DEBE cerrar `listen_fd` (no lo necesita) |
| Costo | ~1-10 ms por fork (copiar tablas de proceso, COW pages) |
| Escalabilidad | Cientos de conexiones OK, miles = problemas de memoria |

### ¿Cuándo usar fork por conexión?

- Protocolos simples y de corta duración (tipo FTP, sendmail clásico)
- Cuando el aislamiento es crítico (un bug del cliente no mata al servidor)
- Scripts CGI (el modelo original de la web en los 90s)

---

## 3. Estrategia 2: thread por conexión

Similar a fork, pero con threads. Más ligero porque los threads
comparten memoria:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT     8080
#define BUF_SIZE 4096

void *handle_client(void *arg) {
    int client_fd = (int)(intptr_t)arg;

    char buf[BUF_SIZE];
    ssize_t n;
    while ((n = read(client_fd, buf, sizeof(buf))) > 0) {
        write(client_fd, buf, n);
    }

    close(client_fd);
    return NULL;
}

int main(void) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);
    printf("Thread-per-connection server on port %d\n", PORT);

    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) continue;

        pthread_t tid;
        int ret = pthread_create(&tid, NULL, handle_client,
                                 (void *)(intptr_t)client_fd);
        if (ret != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(ret));
            close(client_fd);
            continue;
        }
        pthread_detach(tid);  // no vamos a hacer join
    }

    return 0;
}
```

### Fork vs thread por conexión

| Aspecto | Fork | Thread |
|---------|------|--------|
| Costo de creación | ~1-10 ms | ~50-100 µs |
| Memoria por unidad | ~MB (COW, tablas de proceso) | ~8 KB (stack) + ~KB (TLS) |
| Aislamiento | Total (espacios de memoria separados) | Nulo (comparten todo) |
| Comunicación | IPC (pipes, shm, sockets) | Variables compartidas (+ mutex) |
| Crash de un worker | Solo muere ese hijo | Puede tumbar todo el proceso |
| Máximo práctico | ~1000 | ~10,000 (limitado por memoria de stacks) |

### Problema: sin límite de threads

```c
// MAL: 50,000 conexiones → 50,000 threads
//   50,000 × 8 MB stack = 400 GB (virtual, pero aún así)
//   Context switches entre 50,000 threads = lento
for (;;) {
    int fd = accept(listen_fd, NULL, NULL);
    pthread_create(&tid, NULL, handler, ...);  // sin límite
}
```

Esto motiva la siguiente estrategia: thread pool.

---

## 4. Estrategia 3: pre-fork (pool de procesos)

En vez de hacer fork por cada conexión, se crean N procesos
**al inicio** y cada uno hace su propio accept:

```
                    ┌──────────────────┐
                    │    Padre         │
                    │  fork N hijos    │
                    │  (supervisor)    │
                    └──────────────────┘

         ┌────────────┬────────────┬────────────┐
         ▼            ▼            ▼            ▼
    ┌─────────┐  ┌─────────┐  ┌─────────┐  ┌─────────┐
    │ Worker 1│  │ Worker 2│  │ Worker 3│  │ Worker 4│
    │ accept()│  │ accept()│  │ accept()│  │ accept()│
    │  loop   │  │  loop   │  │  loop   │  │  loop   │
    └─────────┘  └─────────┘  └─────────┘  └─────────┘

    Todos hacen accept() en el mismo listen_fd.
    El kernel entrega cada conexión a UN solo worker.
```

### Implementación

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT       8080
#define NUM_WORKERS 4
#define BUF_SIZE   4096

void worker_loop(int listen_fd, int worker_id) {
    printf("Worker %d (pid %d) started\n", worker_id, getpid());

    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) continue;

        // Procesar cliente (echo)
        char buf[BUF_SIZE];
        ssize_t n;
        while ((n = read(client_fd, buf, sizeof(buf))) > 0)
            write(client_fd, buf, n);

        close(client_fd);
    }
}

int main(void) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);

    // Pre-fork workers
    for (int i = 0; i < NUM_WORKERS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            worker_loop(listen_fd, i);
            _exit(0);  // nunca llega aquí
        }
    }

    printf("Pre-fork server: %d workers on port %d\n",
           NUM_WORKERS, PORT);

    // Padre: esperar a los hijos (supervisor simple)
    for (;;) {
        int status;
        pid_t pid = wait(&status);
        if (pid > 0) {
            fprintf(stderr, "Worker %d died, restarting...\n", pid);
            if (fork() == 0) {
                worker_loop(listen_fd, -1);
                _exit(0);
            }
        }
    }

    return 0;
}
```

### Thundering herd problem

Cuando una conexión llega, **todos los workers** durmiendo en
`accept()` despiertan, pero solo uno obtiene la conexión. Los demás
despiertan para nada y vuelven a dormir. Con muchos workers, esto
desperdicia CPU:

```
Conexión llega:
  Worker 1: ¡despierta! → accept() → obtiene la conexión ✓
  Worker 2: ¡despierta! → accept() → EAGAIN (otro la tomó) ✗
  Worker 3: ¡despierta! → accept() → EAGAIN ✗
  Worker 4: ¡despierta! → accept() → EAGAIN ✗
```

Soluciones:
- **`SO_REUSEPORT`** (Linux 3.9+): cada worker tiene su propio
  listen socket; el kernel distribuye las conexiones
- **`EPOLLEXCLUSIVE`** (Linux 4.5+): solo un worker despierta
- **Mutex antes de accept**: serializar con un lock de archivo

```c
// Solución con SO_REUSEPORT: cada worker crea su propio socket
void worker_with_reuseport(int worker_id) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(fd, 128);

    // Solo este worker recibe conexiones en su socket
    for (;;) {
        int client_fd = accept(fd, NULL, NULL);
        if (client_fd == -1) continue;
        // ... procesar ...
        close(client_fd);
    }
}
```

---

## 5. Estrategia 4: thread pool

Un número fijo de threads consume conexiones de una cola compartida.
Es la forma más común en servidores modernos de aplicación:

```
                    ┌──────────────────┐
                    │  Main thread     │
                    │  accept() loop   │
                    └──────┬───────────┘
                           │ encolar fd
                           ▼
                    ┌──────────────────┐
                    │  Cola de trabajo  │
                    │  [fd7] [fd9] [fd12]│
                    └──────┬───────────┘
                           │ desencolar fd
              ┌────────────┼────────────┐
              ▼            ▼            ▼
         ┌────────┐   ┌────────┐   ┌────────┐
         │Thread 1│   │Thread 2│   │Thread 3│
         │procesar│   │procesar│   │procesar│
         │ fd=7   │   │ fd=9   │   │ fd=12  │
         └────────┘   └────────┘   └────────┘
```

### Implementación

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT         8080
#define POOL_SIZE    8
#define QUEUE_SIZE   128
#define BUF_SIZE     4096

// --- Cola thread-safe ---
typedef struct {
    int              fds[QUEUE_SIZE];
    int              head, tail, count;
    pthread_mutex_t  mutex;
    pthread_cond_t   not_empty;
    pthread_cond_t   not_full;
} task_queue_t;

void queue_init(task_queue_t *q) {
    q->head = q->tail = q->count = 0;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_push(task_queue_t *q, int fd) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == QUEUE_SIZE)
        pthread_cond_wait(&q->not_full, &q->mutex);
    q->fds[q->tail] = fd;
    q->tail = (q->tail + 1) % QUEUE_SIZE;
    q->count++;
    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->mutex);
}

int queue_pop(task_queue_t *q) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == 0)
        pthread_cond_wait(&q->not_empty, &q->mutex);
    int fd = q->fds[q->head];
    q->head = (q->head + 1) % QUEUE_SIZE;
    q->count--;
    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->mutex);
    return fd;
}

// --- Workers ---
static task_queue_t work_queue;

void *worker(void *arg) {
    int id = (int)(intptr_t)arg;
    (void)id;

    for (;;) {
        int client_fd = queue_pop(&work_queue);

        char buf[BUF_SIZE];
        ssize_t n;
        while ((n = read(client_fd, buf, sizeof(buf))) > 0)
            write(client_fd, buf, n);

        close(client_fd);
    }
    return NULL;
}

int main(void) {
    int listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(listen_fd, 128);

    // Crear pool de threads
    queue_init(&work_queue);
    pthread_t threads[POOL_SIZE];
    for (int i = 0; i < POOL_SIZE; i++)
        pthread_create(&threads[i], NULL, worker, (void *)(intptr_t)i);

    printf("Thread pool server: %d workers on port %d\n",
           POOL_SIZE, PORT);

    // Main thread: accept y encolar
    for (;;) {
        int client_fd = accept(listen_fd, NULL, NULL);
        if (client_fd == -1) continue;
        queue_push(&work_queue, client_fd);
    }

    return 0;
}
```

### Dimensionamiento del pool

```
                 ┌─────────────────────────────────────┐
                 │  ¿Tipo de trabajo?                  │
                 └──────┬──────────────────────────────┘
                   CPU-bound │          I/O-bound
                        ▼                    ▼
              N = número de CPUs       N = CPUs × 2-4
              (más threads = más       (los threads duermen
               context switches,        en I/O, así que
               no más throughput)        más threads = más
                                         concurrencia útil)

Regla práctica:
  CPU-bound:  pool_size = nproc
  I/O-bound:  pool_size = nproc × 2
  Mixto:      medir y ajustar
```

```c
#include <unistd.h>
long ncpus = sysconf(_SC_NPROCESSORS_ONLN);
int pool_size = ncpus * 2;  // para I/O-bound
```

---

## 6. Estrategia 5: event loop single-threaded

Un solo thread con `epoll` maneja todas las conexiones
concurrentemente sin threads ni procesos adicionales:

```
┌──────────────────────────────────────────┐
│  Un solo thread                          │
│                                          │
│  epoll_wait()                            │
│    ├── listen_fd listo → accept          │
│    ├── client_fd[3] listo → read+write   │
│    ├── client_fd[7] listo → read+write   │
│    └── timer_fd listo → estadísticas     │
│                                          │
│  Concurrente pero NO paralelo            │
│  (todo en un core)                       │
└──────────────────────────────────────────┘
```

Ya vimos esta estrategia en los tópicos anteriores (select, poll,
epoll). Sus ventajas:

| Ventaja | Detalle |
|---------|---------|
| Sin locks | Un solo thread, no hay data races |
| Sin context switches | No hay threads compitiendo |
| Mínima memoria | Sin stacks adicionales |
| Muy escalable en I/O | C10K+ con epoll |

### Limitación: operaciones CPU-bound bloquean todo

```
Event loop:

  [client 1: read+write 1ms] → OK
  [client 2: read+write 1ms] → OK
  [client 3: compute 500ms]  → ¡BLOQUEA! Clientes 4-100 esperan
  [client 4: read+write 1ms] → esperó 500ms
```

Si alguna operación tarda (compresión, criptografía, consulta a
base de datos), bloquea el event loop entero. La solución es la
estrategia híbrida.

---

## 7. Estrategia 6: event loop + thread pool (híbrido)

El **reactor pattern**: un event loop acepta conexiones y lee datos,
pero delega el trabajo pesado a un thread pool:

```
┌────────────────────────────────────────────────────┐
│  Event loop thread (epoll)                          │
│                                                     │
│  epoll_wait()                                       │
│    ├── listen_fd → accept, registrar nuevo fd       │
│    ├── client_fd → read request                     │
│    │     └── encolar tarea al thread pool ──────────┤─┐
│    └── eventfd → respuesta del pool ◀───────────────┤─┤
│          └── write response al cliente              │ │
└─────────────────────────────────────────────────────┘ │
                                                         │
┌────────────────────────────────────────────────────┐   │
│  Thread pool                                        │   │
│                                                     │   │
│  Worker 1: procesar request → notificar event loop ─┤───┘
│  Worker 2: procesar request → notificar event loop  │
│  Worker 3: (idle, esperando tarea)                  │
└─────────────────────────────────────────────────────┘
```

### Implementación esquemática

```c
#include <sys/epoll.h>
#include <sys/eventfd.h>

// Estructura para una tarea completada
typedef struct {
    int   client_fd;
    char *response;
    size_t response_len;
} completed_task_t;

// Cola de resultados (thread pool → event loop)
static completed_task_t results[1024];
static int result_count = 0;
static pthread_mutex_t result_mutex = PTHREAD_MUTEX_INITIALIZER;
static int notify_fd;  // eventfd para despertar el event loop

// Worker del thread pool
void *worker(void *arg) {
    for (;;) {
        // Obtener tarea de la cola de trabajo
        task_t task = dequeue_task();

        // Procesar (CPU-bound o I/O bloqueante)
        char *response = process_request(task.request, task.req_len);

        // Poner resultado en la cola de resultados
        pthread_mutex_lock(&result_mutex);
        results[result_count++] = (completed_task_t){
            .client_fd    = task.client_fd,
            .response     = response,
            .response_len = strlen(response)
        };
        pthread_mutex_unlock(&result_mutex);

        // Despertar el event loop
        uint64_t val = 1;
        write(notify_fd, &val, sizeof(val));
    }
}

// Event loop (main thread)
void event_loop(int listen_fd) {
    int epfd = epoll_create1(0);
    notify_fd = eventfd(0, EFD_NONBLOCK);

    // Registrar listen_fd y notify_fd en epoll
    struct epoll_event ev;
    ev.events = EPOLLIN; ev.data.fd = listen_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev);
    ev.data.fd = notify_fd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, notify_fd, &ev);

    struct epoll_event events[64];
    for (;;) {
        int n = epoll_wait(epfd, events, 64, -1);

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {
                // Nueva conexión
                int client_fd = accept(listen_fd, NULL, NULL);
                ev.events = EPOLLIN; ev.data.fd = client_fd;
                epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &ev);

            } else if (fd == notify_fd) {
                // Resultados del thread pool
                uint64_t val;
                read(notify_fd, &val, sizeof(val));

                pthread_mutex_lock(&result_mutex);
                for (int j = 0; j < result_count; j++) {
                    write(results[j].client_fd,
                          results[j].response,
                          results[j].response_len);
                    free(results[j].response);
                    close(results[j].client_fd);
                    epoll_ctl(epfd, EPOLL_CTL_DEL,
                              results[j].client_fd, NULL);
                }
                result_count = 0;
                pthread_mutex_unlock(&result_mutex);

            } else {
                // Datos de un cliente → leer y encolar tarea
                char buf[4096];
                ssize_t n = read(fd, buf, sizeof(buf));
                if (n <= 0) {
                    close(fd);
                } else {
                    enqueue_task(fd, buf, n);
                }
            }
        }
    }
}
```

Este es el patrón que usan Node.js (libuv), Nginx (parcialmente),
y muchos servidores de alto rendimiento.

---

## 8. Comparación de estrategias

```
┌────────────────────┬──────────┬──────────┬───────────┬─────────────┐
│                    │ Fork/    │ Thread/  │Thread pool│ Event loop  │
│                    │ conn     │ conn     │           │ (epoll)     │
├────────────────────┼──────────┼──────────┼───────────┼─────────────┤
│ Concurrencia       │ Procesos │ Threads  │ Threads   │ Multiplexed │
│ Paralelismo        │ Sí       │ Sí       │ Sí        │ No*         │
│ Aislamiento        │ Total    │ Ninguno  │ Ninguno   │ N/A         │
│ Creación por conn  │ ~ms      │ ~µs      │ 0 (ya     │ 0           │
│                    │          │          │ existen)  │             │
│ Memoria por conn   │ ~MB      │ ~64 KB   │ 0 (compar-│ ~bytes      │
│                    │          │          │ tido)     │ (struct)    │
│ Max conexiones     │ ~1K      │ ~10K     │ ~100K     │ ~1M         │
│ Complejidad        │ Baja     │ Media    │ Media-Alta│ Alta        │
│ Locks necesarios   │ No       │ Sí       │ Sí        │ No          │
│ CPU-bound work     │ OK       │ OK       │ OK        │ Bloquea     │
│ I/O-bound work     │ OK       │ OK       │ OK        │ Excelente   │
│ Ejemplo real       │ Apache   │ Tomcat   │ Java EE   │ nginx,      │
│                    │ (prefork)│          │           │ Node.js     │
└────────────────────┴──────────┴──────────┴───────────┴─────────────┘

* Event loop + thread pool = paralelo para CPU-bound
```

### Diagrama de decisión

```
¿Qué tipo de servidor necesitas?

  ┌──────────────────────┐
  │ ¿Cuántas conexiones  │
  │  simultáneas?        │
  └──────┬───────────────┘
    <100 │     100-10K        >10K
     ▼   │        ▼              ▼
 fork o   │  thread pool    event loop
 thread   │  (fácil y        (epoll)
 por conn │   eficiente)     o híbrido
          │
          │  ┌────────────────────┐
          └──│ ¿Necesitas         │
             │  aislamiento?      │
             └──────┬─────────────┘
                Sí  │  No
                ▼   │  ▼
             pre-fork  thread pool
```

---

## 9. Servidores reales: qué usan

| Servidor | Estrategia | Detalle |
|----------|-----------|---------|
| **Apache** (prefork) | Pre-fork | N procesos, cada uno atiende 1 conexión |
| **Apache** (worker) | Híbrido | N procesos × M threads por proceso |
| **Apache** (event) | Event + threads | Event loop + thread pool |
| **nginx** | Event loop multi-proceso | N workers (procesos), cada uno con epoll |
| **Node.js** | Event loop + thread pool | libuv: epoll para I/O, pool para fs/DNS |
| **HAProxy** | Event loop multi-thread | epoll por thread |
| **Redis** | Event loop single-threaded | Todo en un thread (I/O-bound) |
| **PostgreSQL** | Fork por conexión | Un proceso por cliente |
| **MySQL** | Thread por conexión | Un thread por cliente |
| **Go net/http** | Goroutine por conexión | Goroutines en runtime con epoll interno |
| **Tokio (Rust)** | Event loop multi-thread | epoll + work-stealing thread pool |

### nginx en detalle

```
nginx:
                    ┌────────────────┐
                    │  Master process │
                    │  (config,       │
                    │   signals)      │
                    └──────┬─────────┘
                           │
         ┌─────────────────┼─────────────────┐
         ▼                 ▼                 ▼
    ┌──────────┐     ┌──────────┐     ┌──────────┐
    │ Worker 1 │     │ Worker 2 │     │ Worker 3 │
    │ (proceso)│     │ (proceso)│     │ (proceso)│
    │ epoll    │     │ epoll    │     │ epoll    │
    │ event    │     │ event    │     │ event    │
    │ loop     │     │ loop     │     │ loop     │
    └──────────┘     └──────────┘     └──────────┘

    Cada worker: un proceso con su propio event loop.
    Usa accept_mutex o EPOLLEXCLUSIVE para evitar
    thundering herd.

    Resultado: ~10,000+ conexiones por worker,
    N workers ≈ N CPUs.
```

### Redis: por qué single-threaded funciona

```
Redis (single-threaded event loop):

  Todo en memoria → cada operación < 1 µs
  Sin I/O de disco (o mínimo con RDB/AOF async)
  Sin locks → sin overhead de sincronización
  epoll maneja 100K+ conexiones

  Throughput: ~100,000 operaciones/segundo
  (limitado por red, no por CPU)
```

Redis demuestra que single-threaded + event loop puede ser más
rápido que multi-threaded cuando el trabajo por request es
mínimo y I/O-bound.

---

## 10. Errores comunes

### Error 1: zombie processes con fork por conexión

```c
// MAL: fork sin recoger hijos
for (;;) {
    int fd = accept(listen_fd, NULL, NULL);
    if (fork() == 0) {
        handle_client(fd);
        _exit(0);
    }
    close(fd);
    // ¡Nunca se llama wait()! → zombies se acumulan
}
```

Después de unas horas, `ps aux` muestra cientos de procesos `<defunct>`.

**Solución**: instalar handler para SIGCHLD:

```c
struct sigaction sa = {
    .sa_handler = sigchld_handler,  // llama waitpid(WNOHANG)
    .sa_flags   = SA_RESTART | SA_NOCLDSTOP
};
sigaction(SIGCHLD, &sa, NULL);
```

### Error 2: no cerrar fds heredados después de fork

```c
// MAL: el hijo hereda listen_fd
if (fork() == 0) {
    // listen_fd sigue abierto en el hijo
    // Si el padre muere, el puerto sigue ocupado
    // por este hijo que no lo necesita
    handle_client(client_fd);
    _exit(0);
}
// MAL: el padre mantiene client_fd
// close(client_fd);  ← ¡olvidado!
// El fd nunca se cierra hasta que el padre muera
// → fd leak, el cliente no recibe EOF
```

**Solución**: cerrar los fds que no se necesitan en cada lado:

```c
if (fork() == 0) {
    close(listen_fd);    // hijo no acepta conexiones
    handle_client(client_fd);
    _exit(0);
}
close(client_fd);        // padre no habla con este cliente
```

### Error 3: thread pool sin límite de cola

```c
// MAL: cola ilimitada
void queue_push(queue_t *q, int fd) {
    // Si los workers están ocupados y llegan 100K conexiones,
    // la cola crece hasta consumir toda la memoria
    q->fds[q->count++] = fd;
}
```

**Solución**: cola acotada con backpressure:

```c
// BIEN: esperar si la cola está llena
void queue_push(queue_t *q, int fd) {
    pthread_mutex_lock(&q->mutex);
    while (q->count == QUEUE_SIZE)
        pthread_cond_wait(&q->not_full, &q->mutex);  // backpressure
    // ... insertar ...
}

// O rechazar si está llena (fail-fast):
int queue_try_push(queue_t *q, int fd) {
    pthread_mutex_lock(&q->mutex);
    if (q->count == QUEUE_SIZE) {
        pthread_mutex_unlock(&q->mutex);
        return -1;  // cola llena, rechazar conexión
    }
    // ... insertar ...
}
```

### Error 4: bloquear el event loop con operaciones lentas

```c
// MAL: operación bloqueante en el event loop
if (events[i].events & EPOLLIN) {
    char buf[4096];
    read(fd, buf, sizeof(buf));

    // Consultar base de datos (puede tardar 50ms)
    char *result = query_database(buf);  // ¡BLOQUEA!

    write(fd, result, strlen(result));
}
// Todos los demás clientes esperaron 50ms
```

**Solución**: delegar al thread pool:

```c
if (events[i].events & EPOLLIN) {
    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf));
    enqueue_task(fd, buf, n);  // thread pool procesa async
}
```

### Error 5: race condition con pthread_detach

```c
// POTENCIAL BUG: si el thread termina antes de pthread_detach
pthread_t tid;
pthread_create(&tid, NULL, handler, (void *)(intptr_t)fd);
// ← el thread podría terminar aquí (rápido)
pthread_detach(tid);
// En la práctica funciona porque detach después de terminado es válido,
// pero es más limpio crear con atributo detached:
```

**Solución más limpia**: crear threads ya detached:

```c
pthread_attr_t attr;
pthread_attr_init(&attr);
pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

pthread_t tid;
pthread_create(&tid, &attr, handler, arg);

pthread_attr_destroy(&attr);
```

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│               Servidor Concurrente                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FORK POR CONEXIÓN:                                          │
│    accept → fork → hijo: close(listen_fd), procesar          │
│                  → padre: close(client_fd)                   │
│    ¡SIGCHLD handler obligatorio! (waitpid WNOHANG)           │
│    Aislamiento total, ~1K conexiones máx                     │
│                                                              │
│  THREAD POR CONEXIÓN:                                        │
│    accept → pthread_create → detach                          │
│    Cuidado: sin límite → miles de threads = problema         │
│    ~10K conexiones máx                                       │
│                                                              │
│  PRE-FORK (pool de procesos):                                │
│    fork N hijos al inicio, cada uno: accept loop             │
│    Thundering herd → SO_REUSEPORT o accept_mutex             │
│    Padre = supervisor (reiniciar hijos muertos)              │
│                                                              │
│  THREAD POOL:                                                │
│    Main: accept → encolar fd                                 │
│    Workers: desencolar → procesar → close                    │
│    Cola bounded (mutex + condvar + backpressure)             │
│    pool_size: CPU-bound=nproc, I/O-bound=nproc×2             │
│                                                              │
│  EVENT LOOP (epoll):                                         │
│    Un thread, epoll_wait, todo non-blocking                  │
│    Sin locks, sin context switches                           │
│    C10K+, pero bloquea si hay trabajo CPU-bound              │
│                                                              │
│  HÍBRIDO (event loop + thread pool):                         │
│    epoll para I/O, pool para CPU-bound                       │
│    eventfd para notificar event loop desde pool              │
│    Patrón de nginx, Node.js, Tokio                           │
│                                                              │
│  DECISIÓN RÁPIDA:                                            │
│    <100 conn:   fork o thread por conexión                   │
│    100-10K:     thread pool                                  │
│    >10K:        epoll event loop (+ pool si CPU-bound)       │
│    Aislamiento: pre-fork                                     │
│                                                              │
│  TRAMPAS:                                                    │
│    • Fork: zombies, fd leaks, thundering herd                │
│    • Thread: sin límite, race conditions                     │
│    • Event loop: bloqueo por operación lenta                 │
│    • Thread pool: cola sin límite → OOM                      │
└──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: comparar las cuatro estrategias

Implementa un servidor echo TCP en **cuatro versiones**:

1. Fork por conexión
2. Thread por conexión
3. Thread pool (4 workers)
4. Event loop con epoll

Todos en el mismo puerto (uno a la vez). Para cada versión:

- Medir el throughput con un cliente que envía 10,000 mensajes de
  1 KB lo más rápido posible.
- Medir la latencia: enviar un mensaje, medir cuánto tarda en
  recibir el echo.
- Probar con 1, 10, y 50 clientes simultáneos.

Herramienta sugerida para benchmarking:
```bash
# Usar un script con múltiples nc en paralelo, o:
# Un programa C que abra N conexiones y mida tiempos
```

**Pregunta de reflexión**: ¿cuál es la estrategia más rápida con
1 cliente? ¿Y con 50? ¿Cuál escala mejor? ¿Cuál tiene la menor
latencia? ¿Los resultados coinciden con la teoría?

---

### Ejercicio 2: thread pool con graceful shutdown

Implementa un servidor con thread pool que haga shutdown limpio:

1. **Thread pool** de N workers (N por argumento).
2. **Cola acotada** con backpressure (si la cola está llena,
   `accept` espera en vez de rechazar).
3. **Señal SIGINT** (Ctrl+C):
   - Dejar de aceptar nuevas conexiones.
   - Esperar a que los workers terminen las conexiones actuales.
   - Vaciar la cola (cerrar los fds pendientes).
   - Hacer join de todos los threads.
   - Imprimir estadísticas: conexiones atendidas, tiempo de vida.
4. No usar `pthread_cancel` — señalizar el shutdown con un flag
   y un fd especial en la cola (sentinel value `-1`).

```bash
./pool_server 4 9000 &
# Conectar varios clientes
kill -INT %1
# Debe cerrar limpiamente sin conexiones cortadas
```

**Pregunta de reflexión**: ¿por qué usar un sentinel value (`-1`)
en la cola en vez de `pthread_cancel`? ¿Qué problemas puede causar
`pthread_cancel` si un thread tiene un mutex tomado o un fd
abierto?

---

### Ejercicio 3: servidor HTTP mínimo con epoll

Implementa un servidor HTTP/1.0 básico que sirva archivos
estáticos:

1. Escuchar en un puerto, usar epoll (LT está bien).
2. Parsear la línea de request: `GET /path HTTP/1.0\r\n...\r\n\r\n`
3. Si el archivo existe en un directorio raíz (`./www/`), responder:
   ```
   HTTP/1.0 200 OK\r\n
   Content-Length: <size>\r\n
   Content-Type: text/html\r\n
   \r\n
   <contenido del archivo>
   ```
4. Si no existe: `HTTP/1.0 404 Not Found\r\n\r\n`
5. Cerrar la conexión después de cada respuesta (HTTP/1.0).

Probar:
```bash
mkdir -p www && echo "<h1>Hello</h1>" > www/index.html
./http_server 8080 &
curl http://localhost:8080/index.html
curl http://localhost:8080/noexiste.html
```

Bonus: detectar el Content-Type por la extensión del archivo
(`.html` → `text/html`, `.css` → `text/css`, `.jpg` → `image/jpeg`).

**Pregunta de reflexión**: este servidor usa epoll pero lee el
archivo del disco con `read()` (bloqueante). Si un cliente pide un
archivo de 1 GB, el event loop se bloquea mientras lee del disco.
¿Cómo resolverías esto? (Pista: piensa en `sendfile()` para
zero-copy, o en delegar la lectura a un thread pool.)
