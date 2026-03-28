# Mejora 1: Thread pool en lugar de fork

## Índice
1. [El problema de fork por conexión](#1-el-problema-de-fork-por-conexión)
2. [Thread pool: concepto](#2-thread-pool-concepto)
3. [Cola de trabajo thread-safe](#3-cola-de-trabajo-thread-safe)
4. [Worker threads](#4-worker-threads)
5. [Implementar el thread pool](#5-implementar-el-thread-pool)
6. [Integrar con el servidor HTTP](#6-integrar-con-el-servidor-http)
7. [Shutdown limpio del pool](#7-shutdown-limpio-del-pool)
8. [Servidor completo con thread pool](#8-servidor-completo-con-thread-pool)
9. [Comparar fork vs thread pool](#9-comparar-fork-vs-thread-pool)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El problema de fork por conexión

En S01 implementamos fork por conexión. Funciona, pero tiene
costos significativos:

```
Petición llega → fork() → handle → _exit()
                  │
                  ├── Copiar page tables (~0.1-1ms)
                  ├── Crear nuevo proceso en el kernel
                  ├── Context switch para ejecutar el hijo
                  ├── Recolectar zombie (waitpid o SA_NOCLDWAIT)
                  └── ~1000 peticiones/s máximo (típico)
```

### Problemas concretos

```
1. Overhead de fork:
   fork() copia page tables, mapeos de memoria, fds
   (copy-on-write mitiga, pero el setup sigue costando)

2. Sin reutilización:
   Cada conexión: crear proceso → trabajar → destruir
   Nunca se reutiliza un proceso ya creado

3. Escalabilidad:
   1000 conexiones simultáneas = 1000 procesos
   Cada proceso consume memoria para su stack (~8MB virtual)
   El scheduler del kernel se degrada con miles de procesos

4. IPC difícil:
   Procesos separados no comparten memoria
   Comunicación entre workers requiere pipes, shmem, etc.
```

### La solución: thread pool

Crear N hilos al inicio y reutilizarlos para todas las conexiones:

```
Fork por conexión:           Thread pool:

  petición 1 → fork()          petición 1 ─┐
  petición 2 → fork()          petición 2 ─┤ cola
  petición 3 → fork()          petición 3 ─┤
  petición 4 → fork()          petición 4 ─┘
       │                            │
  4 procesos creados           N workers (pre-creados)
  4 procesos destruidos        0 creaciones, 0 destrucciones
```

---

## 2. Thread pool: concepto

Un thread pool tiene tres componentes:

```
┌─────────────────────────────────────────────────────────┐
│                      Thread Pool                         │
│                                                          │
│  ┌──────────────────────────────────────┐                │
│  │          Cola de trabajo             │                │
│  │  ┌────┐ ┌────┐ ┌────┐ ┌────┐        │                │
│  │  │fd=5│ │fd=6│ │fd=7│ │    │ ← accept() añade fds   │
│  │  └────┘ └────┘ └────┘ └────┘        │                │
│  └──────────────┬───────────────────────┘                │
│                 │ workers toman de la cola               │
│     ┌───────────┼───────────┐                            │
│     ▼           ▼           ▼                            │
│  ┌────────┐ ┌────────┐ ┌────────┐                       │
│  │Worker 0│ │Worker 1│ │Worker 2│  ← N hilos pre-creados│
│  │handle()│ │ (idle) │ │handle()│                        │
│  └────────┘ └────────┘ └────────┘                       │
│                                                          │
└─────────────────────────────────────────────────────────┘

Main thread:
  loop { accept() → encolar client_fd }
```

### Flujo de vida

```
1. INICIO:
   Crear N worker threads
   Inicializar cola vacía (mutex + condvar)

2. OPERACIÓN:
   Main thread:  accept() → push(client_fd) → señalar condvar
   Worker idle:   wait(condvar) → pop() → handle_client() → volver a wait

3. SHUTDOWN:
   Señalar a todos los workers que deben terminar
   join todos los threads
   Destruir cola, mutex, condvar
```

### ¿Cuántos workers?

```
CPU-bound (cálculo intenso):
  N = número de cores (más hilos compiten por CPU sin beneficio)

I/O-bound (nuestro caso: leer disco, escribir socket):
  N = 2× a 4× cores (mientras uno espera I/O, otro trabaja)

Típico para servidor HTTP:
  N = número de cores × 2
  Ejemplo: 4 cores → 8 workers
```

---

## 3. Cola de trabajo thread-safe

La cola es el punto de sincronización entre el main thread
(productor) y los workers (consumidores). Necesita mutex +
condvar:

```c
#include <pthread.h>
#include <stdlib.h>

#define QUEUE_CAPACITY 1024

typedef struct {
    int items[QUEUE_CAPACITY];
    int head;           // índice del siguiente elemento a sacar
    int tail;           // índice donde insertar el siguiente
    int count;          // elementos actuales

    pthread_mutex_t mutex;
    pthread_cond_t  not_empty;   // señalar cuando hay trabajo
    pthread_cond_t  not_full;    // señalar cuando hay espacio
    int             shutdown;    // flag de shutdown
} work_queue_t;

void queue_init(work_queue_t *q) {
    q->head     = 0;
    q->tail     = 0;
    q->count    = 0;
    q->shutdown = 0;

    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->not_empty, NULL);
    pthread_cond_init(&q->not_full, NULL);
}

void queue_destroy(work_queue_t *q) {
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->not_empty);
    pthread_cond_destroy(&q->not_full);
}
```

### Push (productor — main thread)

```c
// Añadir un client_fd a la cola
// Bloquea si la cola está llena (backpressure)
// Retorna 0 en éxito, -1 si shutdown
int queue_push(work_queue_t *q, int client_fd) {
    pthread_mutex_lock(&q->mutex);

    // Esperar si la cola está llena
    while (q->count == QUEUE_CAPACITY && !q->shutdown) {
        pthread_cond_wait(&q->not_full, &q->mutex);
    }

    if (q->shutdown) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    q->items[q->tail] = client_fd;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;

    // Señalar a un worker que hay trabajo
    pthread_cond_signal(&q->not_empty);

    pthread_mutex_unlock(&q->mutex);
    return 0;
}
```

### Pop (consumidor — worker thread)

```c
// Sacar un client_fd de la cola
// Bloquea si la cola está vacía (worker espera trabajo)
// Retorna el fd, o -1 si shutdown
int queue_pop(work_queue_t *q) {
    pthread_mutex_lock(&q->mutex);

    // Esperar si la cola está vacía
    while (q->count == 0 && !q->shutdown) {
        pthread_cond_wait(&q->not_empty, &q->mutex);
    }

    if (q->shutdown && q->count == 0) {
        pthread_mutex_unlock(&q->mutex);
        return -1;
    }

    int fd = q->items[q->head];
    q->head = (q->head + 1) % QUEUE_CAPACITY;
    q->count--;

    // Señalar al productor que hay espacio
    pthread_cond_signal(&q->not_full);

    pthread_mutex_unlock(&q->mutex);
    return fd;
}
```

### Ring buffer

La cola usa un array circular (ring buffer) para evitar mover
elementos:

```
Capacidad: 8       head=2, tail=5, count=3

  ┌───┬───┬───┬───┬───┬───┬───┬───┐
  │   │   │ 7 │ 8 │ 9 │   │   │   │
  └───┴───┴─▲─┴───┴─▲─┴───┴───┴───┘
            │       │
          head    tail

push(10): items[tail=5] = 10, tail = 6, count = 4
pop():    fd = items[head=2] = 7, head = 3, count = 2

Cuando tail llega al final, wraparound: tail = (tail+1) % capacity
```

### Backpressure

Si la cola está llena, `queue_push` bloquea al main thread.
Esto es **backpressure**: el servidor deja de aceptar nuevas
conexiones hasta que algún worker termine y libere espacio.

```
Cola llena (1024 conexiones pendientes):
  accept() retorna client_fd
  queue_push() → cola llena → main thread bloquea
  → accept() no se llama → backlog de TCP se llena
  → kernel rechaza nuevas conexiones (o las encola en backlog)
  → worker termina → pop() → espacio en cola
  → main thread despierta → push() → vuelve a accept()
```

Sin backpressure, el servidor aceptaría conexiones ilimitadas
y eventualmente se quedaría sin memoria o file descriptors.

---

## 4. Worker threads

Cada worker es un hilo que ejecuta un loop infinito: esperar
trabajo → procesar → repetir:

```c
void *worker_thread(void *arg) {
    work_queue_t *queue = (work_queue_t *)arg;

    for (;;) {
        // Esperar trabajo (bloquea si la cola está vacía)
        int client_fd = queue_pop(queue);

        if (client_fd == -1) {
            break;  // shutdown
        }

        // Procesar la conexión
        // Obtener IP del cliente
        struct sockaddr_in addr;
        socklen_t addr_len = sizeof(addr);
        getpeername(client_fd, (struct sockaddr *)&addr, &addr_len);

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &addr.sin_addr,
                  client_ip, sizeof(client_ip));

        handle_client(client_fd, client_ip);

        close(client_fd);
    }

    return NULL;
}
```

### getpeername en vez de accept

En el modelo fork, obteníamos la IP del cliente directamente de
`accept()`. Con thread pool, el main thread hace accept y pasa
solo el fd. El worker usa `getpeername()` para obtener la
dirección del peer:

```c
// En fork: la IP viene de accept()
int client_fd = accept(listener, (struct sockaddr *)&addr, &len);
inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));

// En thread pool: la IP se obtiene del fd
int client_fd = queue_pop(queue);  // solo el fd
getpeername(client_fd, (struct sockaddr *)&addr, &len);
inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof(ip));
```

### Thread safety de handle_client

Con fork, cada proceso tiene su propia memoria — no hay race
conditions. Con threads, **todos los workers comparten memoria**.
Verificar que handle_client sea thread-safe:

```
✓ Variables locales (stack): cada thread tiene su propio stack
✓ Argumentos (client_fd, client_ip): valores diferentes por thread
✓ open/read/write/close: thread-safe por POSIX
✓ snprintf, strstr, strcmp: thread-safe (no usan estado global)
✓ stat, realpath: thread-safe

✗ Variables globales mutables: race conditions
✗ strtok(): no thread-safe (usar strtok_r)
✗ ctime(): no thread-safe (usar ctime_r o strftime)
✗ gmtime(): no thread-safe (usar gmtime_r)
✗ gethostbyname(): no thread-safe (usar getaddrinfo)
```

Funciones a reemplazar:

```c
// MAL (no thread-safe):
struct tm *gmt = gmtime(&now);

// BIEN (thread-safe):
struct tm gmt;
gmtime_r(&now, &gmt);
```

```c
// MAL (no thread-safe):
char *date = ctime(&now);

// BIEN (thread-safe):
char date[128];
struct tm tm;
gmtime_r(&now, &tm);
strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", &tm);
```

---

## 5. Implementar el thread pool

```c
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    pthread_t    *threads;      // array de thread IDs
    int           num_threads;  // número de workers
    work_queue_t  queue;        // cola compartida
} thread_pool_t;

// Crear el pool con N workers
int pool_create(thread_pool_t *pool, int num_threads) {
    pool->num_threads = num_threads;
    pool->threads = malloc(sizeof(pthread_t) * num_threads);
    if (pool->threads == NULL) {
        return -1;
    }

    queue_init(&pool->queue);

    // Lanzar los worker threads
    for (int i = 0; i < num_threads; i++) {
        int err = pthread_create(&pool->threads[i], NULL,
                                 worker_thread, &pool->queue);
        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            // Cleanup parcial
            pool->queue.shutdown = 1;
            pthread_cond_broadcast(&pool->queue.not_empty);
            for (int j = 0; j < i; j++) {
                pthread_join(pool->threads[j], NULL);
            }
            free(pool->threads);
            return -1;
        }
    }

    printf("Thread pool started with %d workers\n", num_threads);
    return 0;
}
```

### Enviar trabajo al pool

```c
// Encolar un client_fd para que un worker lo procese
int pool_submit(thread_pool_t *pool, int client_fd) {
    return queue_push(&pool->queue, client_fd);
}
```

---

## 6. Integrar con el servidor HTTP

El accept loop ahora encola fds en vez de hacer fork:

```c
void serve_with_pool(int listener, thread_pool_t *pool) {
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listener,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        // Encolar para un worker
        if (pool_submit(pool, client_fd) == -1) {
            // Pool en shutdown o cola llena
            const char *resp =
                "HTTP/1.1 503 Service Unavailable\r\n"
                "Content-Length: 19\r\n"
                "Connection: close\r\n"
                "\r\n"
                "Service Unavailable";
            write(client_fd, resp, strlen(resp));
            close(client_fd);
        }
    }
}
```

### Comparación visual

```
Fork:                           Thread pool:

Main:                           Main:
  accept()─►fork()               accept()─►queue_push()
     │        │                      │         │
     │     ┌──▼──┐                   │    ┌────▼────┐
     │     │hijo │                   │    │  Cola   │
     │     │handle│                  │    │ [5,6,7] │
     │     │close │                  │    └────┬────┘
     │     │_exit │                  │         │
     │     └─────┘                   │    workers pop():
     │                               │    ┌───┐ ┌───┐ ┌───┐
     ▼                               │    │W0 │ │W1 │ │W2 │
  accept()                           ▼    └───┘ └───┘ └───┘
                                  accept()

Costo: fork+exit por conexión    Costo: push+pop por conexión
       (~0.1-1ms)                       (~microsegundos)
```

---

## 7. Shutdown limpio del pool

### Señalar shutdown a los workers

```c
void pool_destroy(thread_pool_t *pool) {
    // 1. Señalar shutdown
    pthread_mutex_lock(&pool->queue.mutex);
    pool->queue.shutdown = 1;
    pthread_cond_broadcast(&pool->queue.not_empty);  // despertar TODOS
    pthread_cond_broadcast(&pool->queue.not_full);
    pthread_mutex_unlock(&pool->queue.mutex);

    // 2. Esperar a que todos los workers terminen
    for (int i = 0; i < pool->num_threads; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // 3. Cerrar los client_fds que quedaron en la cola sin procesar
    while (pool->queue.count > 0) {
        int fd = pool->queue.items[pool->queue.head];
        pool->queue.head = (pool->queue.head + 1) % QUEUE_CAPACITY;
        pool->queue.count--;
        close(fd);
    }

    // 4. Cleanup
    queue_destroy(&pool->queue);
    free(pool->threads);

    printf("Thread pool shut down\n");
}
```

### ¿Por qué broadcast y no signal?

`pthread_cond_signal` despierta **un** thread.
`pthread_cond_broadcast` despierta **todos** los threads.

En shutdown, necesitamos que **todos** los workers vean el flag
`shutdown` y terminen. Si usamos signal, solo un worker despierta,
los demás siguen dormidos indefinidamente:

```
signal:                    broadcast:
  Worker 0: despierta        Worker 0: despierta → ve shutdown → sale
  Worker 1: sigue dormido    Worker 1: despierta → ve shutdown → sale
  Worker 2: sigue dormido    Worker 2: despierta → ve shutdown → sale
  → join de 1 y 2 cuelga    → join de todos completa ✓
```

### Diagrama de shutdown

```
1. running = 0           (señal SIGINT)
2. accept loop termina   (EINTR)
3. pool_destroy():
     lock(mutex)
     shutdown = 1
     broadcast(not_empty)    ← despierta todos los workers
     unlock(mutex)

     Workers:                Pool:
       pop() retorna -1        join(thread[0])
       break del loop          join(thread[1])  ← espera
       return NULL             join(thread[2])

4. Cerrar fds huérfanos en la cola
5. queue_destroy, free
```

---

## 8. Servidor completo con thread pool

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <limits.h>

#define PORT           8080
#define BACKLOG        128
#define DOCROOT        "./www"
#define NUM_WORKERS    8
#define QUEUE_CAPACITY 1024
#define BUFSIZE        8192

// ─── Cola y Pool (secciones 3-5) ────────────────────
// work_queue_t, queue_init, queue_push, queue_pop
// thread_pool_t, pool_create, pool_submit, pool_destroy
// worker_thread
// ... (definidos arriba) ...

// ─── HTTP (S01: T01-T04) ───────────────────────────
// http_request_t, parse_http_request, get_mime_type,
// validate_path, serve_file, handle_client
// ... (definidos en S01) ...

// ─── NOTA: thread safety ───────────────────────────
// Reemplazar gmtime → gmtime_r en todas las funciones

void format_http_date_r(char *buf, size_t bufsize, time_t t) {
    struct tm gmt;
    gmtime_r(&t, &gmt);
    strftime(buf, bufsize, "%a, %d %b %Y %H:%M:%S GMT", &gmt);
}

// ─── Main ──────────────────────────────────────────

static volatile sig_atomic_t running = 1;

void shutdown_handler(int sig) {
    (void)sig;
    running = 0;
}

int main(int argc, char *argv[]) {
    int port         = PORT;
    int num_workers  = NUM_WORKERS;
    const char *docroot = DOCROOT;

    if (argc >= 2) port        = atoi(argv[1]);
    if (argc >= 3) docroot     = argv[2];
    if (argc >= 4) num_workers = atoi(argv[3]);

    // Verificar docroot
    struct stat st;
    if (stat(docroot, &st) == -1 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "Document root '%s' not found\n", docroot);
        return 1;
    }

    // Crear listener
    int listener = socket(AF_INET, SOCK_STREAM, 0);
    if (listener == -1) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(port),
        .sin_addr.s_addr = INADDR_ANY,
    };

    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        perror("bind"); close(listener); return 1;
    }
    if (listen(listener, BACKLOG) == -1) {
        perror("listen"); close(listener); return 1;
    }

    // Ignorar SIGPIPE (write a socket cerrado → EPIPE en vez de señal)
    signal(SIGPIPE, SIG_IGN);

    // Shutdown handler
    struct sigaction sa;
    sa.sa_handler = shutdown_handler;
    sa.sa_flags   = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT,  &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // Crear thread pool
    thread_pool_t pool;
    if (pool_create(&pool, num_workers) == -1) {
        close(listener);
        return 1;
    }

    char docroot_abs[PATH_MAX];
    realpath(docroot, docroot_abs);
    printf("minihttpd listening on 0.0.0.0:%d\n", port);
    printf("Document root: %s\n", docroot_abs);
    printf("Workers: %d\n\n", num_workers);

    // Accept loop
    while (running) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(listener,
                               (struct sockaddr *)&client_addr,
                               &client_len);
        if (client_fd == -1) {
            if (errno == EINTR) continue;
            perror("accept");
            continue;
        }

        if (pool_submit(&pool, client_fd) == -1) {
            close(client_fd);
        }
    }

    // Shutdown
    printf("\nShutting down...\n");
    pool_destroy(&pool);
    close(listener);
    printf("Goodbye.\n");

    return 0;
}
```

### SIGPIPE

Con fork, si el cliente cierra la conexión mientras el hijo
escribe, el hijo recibe `SIGPIPE` y muere — no es problema,
es un proceso separado.

Con threads, SIGPIPE mataría **todo el proceso** (todos los
workers). Debemos ignorarlo:

```c
signal(SIGPIPE, SIG_IGN);
// Ahora write() a socket cerrado retorna -1 con errno=EPIPE
// → el worker maneja el error y continúa con la siguiente conexión
```

### Compilar

```bash
gcc -Wall -Wextra -pthread -o minihttpd minihttpd.c
./minihttpd 8080 ./www 8
```

`-pthread` es necesario para pthreads. Sin este flag, `pthread_create`
puede fallar silenciosamente o el programa puede comportarse de
forma incorrecta.

---

## 9. Comparar fork vs thread pool

### Benchmark con ab

```bash
# Servidor fork:
ab -n 10000 -c 100 http://localhost:8080/index.html

# Servidor thread pool:
ab -n 10000 -c 100 http://localhost:8080/index.html
```

### Comparación cualitativa

```
                          Fork            Thread Pool
                          ────            ───────────
Creación por conexión     ~0.5ms (fork)   ~1μs (push+pop)
Memoria por conexión      ~MB (proceso)   ~KB (ya existente)
Concurrencia máxima       ~1000 procs     ilimitada (cola)
Aislamiento               Total           Compartido
Crash de un worker        Solo ese hijo   Todo el proceso
Thread safety             No necesario    Obligatorio
SIGPIPE handling          Mata al hijo    Mata al proceso
Complejidad               Baja            Media
Estado compartido         No              Sí (cuidado)
```

### ¿Cuándo usar cada uno?

```
Fork por conexión:
  ✓ Prototipos rápidos
  ✓ Cuando cada conexión necesita aislamiento fuerte
  ✓ Cuando el handler usa funciones no thread-safe que no puedes
    cambiar (librerías legacy)
  ✓ CGI tradicional

Thread pool:
  ✓ Servidores de producción
  ✓ Alto throughput (miles de req/s)
  ✓ Recursos compartidos (cache en memoria, counters)
  ✓ Cuando fork es prohibitivo (muchas conexiones cortas)
```

### Diagrama de rendimiento

```
Requests/segundo vs conexiones concurrentes (ilustrativo):

req/s
  │
  │  ████ thread pool
  │  ████████████████████████████████████
  │  ████████████████████████████████████████
  │
  │  ░░░░ fork
  │  ░░░░░░░░░░░░░░░░
  │  ░░░░░░░░░░░░░░░░░░░
  │  ░░░░░░░░░░░░░░░░░░░░░░ (se degrada)
  │
  └──────────────────────────────────────── conexiones
     10    50   100   500  1000

Fork se degrada: cada fork cuesta, scheduler se satura.
Thread pool se mantiene: workers reutilizados, cola absorbe picos.
```

---

## 10. Errores comunes

### Error 1: no proteger la cola con mutex

```c
// MAL: push/pop sin lock
void queue_push(work_queue_t *q, int fd) {
    q->items[q->tail] = fd;
    q->tail = (q->tail + 1) % QUEUE_CAPACITY;
    q->count++;
    // Race: dos threads hacen push al mismo tiempo
    // → ambos escriben en el mismo slot → fd perdido
}
```

**Solución**: siempre lock antes de acceder a la cola:

```c
pthread_mutex_lock(&q->mutex);
// ... modificar cola ...
pthread_mutex_unlock(&q->mutex);
```

### Error 2: usar signal en vez de broadcast para shutdown

```c
// MAL: signal solo despierta UN worker
pthread_cond_signal(&q->not_empty);
// Workers 1..N-1 siguen bloqueados en cond_wait
// → join cuelga para siempre
```

**Solución**: broadcast para despertar a **todos**:

```c
pthread_cond_broadcast(&q->not_empty);
```

### Error 3: no ignorar SIGPIPE

```c
// MAL: SIGPIPE mata todo el proceso
// Un cliente cierra la conexión mientras escribimos
write(client_fd, response, len);
// → SIGPIPE → todo el servidor muere → todos los workers mueren
```

**Solución**: ignorar SIGPIPE al inicio:

```c
signal(SIGPIPE, SIG_IGN);
// write retorna -1 con errno=EPIPE → el worker maneja el error
```

### Error 4: usar funciones no thread-safe

```c
// MAL: gmtime retorna puntero a buffer estático global
struct tm *gmt = gmtime(&now);
// Si dos workers llaman gmtime al mismo tiempo,
// el segundo sobreescribe el resultado del primero
```

**Solución**: usar las variantes `_r` (reentrant):

```c
struct tm gmt;
gmtime_r(&now, &gmt);  // cada thread usa su propio struct tm
```

### Error 5: no cerrar fds huérfanos en shutdown

```c
// MAL: pool_destroy sin cerrar fds de la cola
void pool_destroy(thread_pool_t *pool) {
    pool->queue.shutdown = 1;
    pthread_cond_broadcast(&pool->queue.not_empty);
    for (int i = 0; i < pool->num_threads; i++)
        pthread_join(pool->threads[i], NULL);
    // Fds que quedaron en la cola → leak
    // Clientes cuyas conexiones estaban encoladas nunca reciben
    // respuesta ni close → timeout del lado del cliente
}
```

**Solución**: cerrar todos los fds residuales:

```c
while (pool->queue.count > 0) {
    int fd = pool->queue.items[pool->queue.head];
    pool->queue.head = (pool->queue.head + 1) % QUEUE_CAPACITY;
    pool->queue.count--;
    close(fd);
}
```

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│          Thread Pool — Servidor HTTP                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ARQUITECTURA:                                               │
│    Main thread: accept() → queue_push(fd)                    │
│    N Workers:   queue_pop() → handle_client() → loop         │
│                                                              │
│  COLA THREAD-SAFE:                                           │
│    Ring buffer + mutex + 2 condvars                          │
│    not_empty: workers esperan trabajo                        │
│    not_full: productor espera espacio (backpressure)         │
│    shutdown flag: señalar fin                                │
│                                                              │
│  PUSH (main):                                                │
│    lock → while(full) wait(not_full) → items[tail]=fd       │
│    → tail++ → count++ → signal(not_empty) → unlock          │
│                                                              │
│  POP (worker):                                               │
│    lock → while(empty && !shutdown) wait(not_empty)          │
│    → fd=items[head] → head++ → count--                      │
│    → signal(not_full) → unlock → return fd                  │
│                                                              │
│  THREAD POOL API:                                            │
│    pool_create(&pool, N)     lanzar N workers               │
│    pool_submit(&pool, fd)    encolar conexión               │
│    pool_destroy(&pool)       shutdown + join + cleanup       │
│                                                              │
│  THREAD SAFETY:                                              │
│    gmtime → gmtime_r         (reentrant)                     │
│    ctime  → strftime          (reentrant)                    │
│    strtok → strtok_r         (reentrant)                     │
│    Variables locales: OK     (cada thread tiene su stack)    │
│    Variables globales: mutex  (o evitar)                     │
│                                                              │
│  SEÑALES:                                                    │
│    signal(SIGPIPE, SIG_IGN)  obligatorio con threads        │
│    SIGINT/SIGTERM → running=0 + accept EINTR                │
│                                                              │
│  SHUTDOWN:                                                   │
│    shutdown = 1                                              │
│    broadcast(not_empty)      despertar TODOS los workers     │
│    join todos los threads                                    │
│    Cerrar fds residuales en la cola                          │
│                                                              │
│  COMPILAR: gcc -pthread -o server server.c                  │
│                                                              │
│  DIMENSIONAR:                                                │
│    Workers = cores × 2  (para I/O-bound)                    │
│    Queue capacity: 256-4096  (según carga esperada)          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: cola thread-safe con tests

Implementa la cola de trabajo y verifica su correctitud con
múltiples productores y consumidores.

**Pasos**:
1. Implementa `work_queue_t` con ring buffer, mutex, 2 condvars
2. Implementa `queue_push()` y `queue_pop()` con bloqueo
3. Escribe un test: 4 threads productores envían 1000 ints cada
   uno, 4 threads consumidores los leen
4. Verifica: todos los 4000 ints fueron recibidos, ninguno
   repetido, ninguno perdido
5. Prueba con `valgrind --tool=helgrind` para detectar races

**Predicción antes de ejecutar**: si los 4 productores envían
los valores 0-999 cada uno, ¿los consumidores los reciben en
orden? ¿Pueden dos consumidores recibir el mismo valor?
¿Qué pasa si la capacidad de la cola es 1?

> **Pregunta de reflexión**: la cola usa `pthread_cond_wait` en
> un `while` loop (no `if`). ¿Por qué es necesario el while?
> ¿Qué es un spurious wakeup y cuándo ocurre? ¿Qué pasaría si
> usaras `if (count == 0) cond_wait(...)` en vez del while?

### Ejercicio 2: servidor HTTP con thread pool

Reemplaza el fork por conexión del servidor S01 con un thread
pool.

**Pasos**:
1. Implementa `thread_pool_t` con `pool_create`, `pool_submit`,
   `pool_destroy`
2. Implementa `worker_thread` que haga pop → handle_client → loop
3. Reemplaza el fork del accept loop por `pool_submit()`
4. Añade `signal(SIGPIPE, SIG_IGN)` al inicio
5. Reemplaza `gmtime()` por `gmtime_r()` en toda función que
   genere headers
6. Compila con `-pthread` y prueba con `ab -n 1000 -c 50`

**Predicción antes de ejecutar**: si creas un pool de 4 workers
y envías 100 peticiones concurrentes con `ab -c 100`, ¿cuántas
se procesan al mismo tiempo? ¿Las otras 96 esperan en la cola
o son rechazadas? ¿Qué pasa si la cola se llena?

> **Pregunta de reflexión**: con fork, un crash en handle_client
> (segfault, abort) solo mata al hijo — el padre sigue vivo.
> Con threads, un segfault en cualquier worker mata **todo el
> proceso**. ¿Cómo protegerías el servidor contra crashes en
> workers? ¿Es posible "aislar" un thread como se aísla un
> proceso? ¿Qué patrón usa nginx para combinar aislamiento
> de procesos con eficiencia de threads?

### Ejercicio 3: pool con estadísticas

Extiende el thread pool para llevar estadísticas de rendimiento:

**Pasos**:
1. Añade contadores atómicos: `total_requests`, `active_workers`,
   `max_queue_depth`
2. Usa `__atomic_add_fetch` para incrementar sin mutex
3. Cada worker incrementa `total_requests` al terminar handle_client
4. Cada worker incrementa `active_workers` al empezar y decrementa
   al terminar
5. Implementa un endpoint `GET /stats` que retorne las estadísticas
   en JSON

**Predicción antes de ejecutar**: si envías 1000 peticiones con
`ab -n 1000 -c 50`, ¿`total_requests` será exactamente 1000 al
final? ¿`active_workers` puede superar `num_workers`? ¿Cuál es
la diferencia entre usar `__atomic_add_fetch` y un mutex para
los contadores?

> **Pregunta de reflexión**: `max_queue_depth` necesita un
> "compare and swap" para actualizarse correctamente:
> leer el valor actual, comparar con el nuevo, escribir si es
> mayor. ¿Cómo implementarías esto con `__atomic_compare_exchange`?
> ¿Por qué un simple `if (depth > max) max = depth` no es
> thread-safe incluso con variables atómicas?
