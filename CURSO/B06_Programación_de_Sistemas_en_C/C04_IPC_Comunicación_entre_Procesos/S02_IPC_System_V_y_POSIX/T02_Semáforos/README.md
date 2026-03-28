# Semáforos

## Índice
1. [Concepto de semáforo](#1-concepto-de-semáforo)
2. [Semáforos POSIX con nombre: sem_open](#2-semáforos-posix-con-nombre-sem_open)
3. [Semáforos POSIX sin nombre: sem_init](#3-semáforos-posix-sin-nombre-sem_init)
4. [System V: semget, semop, semctl](#4-system-v-semget-semop-semctl)
5. [Patrones clásicos](#5-patrones-clásicos)
6. [Semáforos vs mutexes](#6-semáforos-vs-mutexes)
7. [Problemas y trampas](#7-problemas-y-trampas)
8. [Herramientas de diagnóstico](#8-herramientas-de-diagnóstico)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Concepto de semáforo

Un semáforo es un **contador entero no negativo** con dos operaciones
atómicas:

- **wait** (P, proberen, decrementar): si el valor es > 0, lo decrementa.
  Si es 0, **bloquea** al proceso hasta que otro lo incremente.
- **post** (V, verhogen, incrementar): incrementa el valor y despierta a
  un proceso bloqueado (si hay alguno).

```
  Semáforo binario (valor inicial = 1):     Semáforo contador (valor inicial = N):

  Proceso A         sem         Proceso B    Permite hasta N accesos simultáneos
  ─────────         ───         ─────────
  wait(sem)    1 → 0                         wait(sem)   3 → 2   (pasa)
  [sección     0    wait(sem)                wait(sem)   2 → 1   (pasa)
   crítica]         [bloqueado]              wait(sem)   1 → 0   (pasa)
  post(sem)    0 → 1                         wait(sem)   0       (¡bloquea!)
                    1 → 0                    ...
                    [sección                 post(sem)   0 → 1   (despierta uno)
                     crítica]
                    post(sem)
                    0 → 1
```

Dijkstra los inventó en 1965. Las operaciones originales se llaman P
(del holandés *proberen*, intentar) y V (*verhogen*, incrementar).
POSIX usa `sem_wait` y `sem_post`.

### Tipos de semáforo

```
┌───────────────────┬──────────────────────────────────────────┐
│ Tipo              │ Uso                                      │
├───────────────────┼──────────────────────────────────────────┤
│ Binario (0 o 1)   │ Exclusión mutua (como un mutex)         │
├───────────────────┼──────────────────────────────────────────┤
│ Contador (0..N)   │ Limitar concurrencia: pool de recursos, │
│                   │ N conexiones, M workers                 │
├───────────────────┼──────────────────────────────────────────┤
│ Con nombre        │ IPC entre procesos no relacionados      │
│ (sem_open)        │ Persiste en el filesystem               │
├───────────────────┼──────────────────────────────────────────┤
│ Sin nombre        │ Entre threads, o entre padre/hijo en    │
│ (sem_init)        │ memoria compartida                      │
└───────────────────┴──────────────────────────────────────────┘
```

---

## 2. Semáforos POSIX con nombre: sem_open

Los semáforos con nombre persisten en el filesystem (normalmente
`/dev/shm/sem.nombre`) y permiten sincronización entre procesos no
relacionados, similar a las named pipes.

### 2.1 Crear / abrir

```c
#include <semaphore.h>
#include <fcntl.h>

// Crear (con valor inicial)
sem_t *sem_open(const char *name, int oflag, mode_t mode, unsigned int value);

// Abrir existente
sem_t *sem_open(const char *name, int oflag);

// Retorna: puntero a sem_t, o SEM_FAILED en error
```

El nombre sigue las mismas reglas que `shm_open`: empieza con `/`,
sin más `/`.

```c
// Creador: crear con valor inicial 1 (semáforo binario)
sem_t *sem = sem_open("/myapp_lock", O_CREAT | O_EXCL, 0666, 1);
if (sem == SEM_FAILED) {
    if (errno == EEXIST) {
        // Ya existe, abrir sin crear
        sem = sem_open("/myapp_lock", 0);
    } else {
        perror("sem_open");
        exit(1);
    }
}

// Otro proceso: abrir existente
sem_t *sem = sem_open("/myapp_lock", 0);
if (sem == SEM_FAILED) {
    perror("sem_open");
    exit(1);
}
```

### 2.2 Operaciones

```c
int sem_wait(sem_t *sem);      // decrementar o bloquear
int sem_trywait(sem_t *sem);   // no bloqueante (EAGAIN si valor == 0)
int sem_timedwait(sem_t *sem, const struct timespec *abs_timeout);
int sem_post(sem_t *sem);      // incrementar, despertar a uno
int sem_getvalue(sem_t *sem, int *sval);  // leer valor actual
```

```c
// Sección crítica protegida por semáforo
sem_wait(sem);       // adquirir (bloquea si otro lo tiene)
// ... sección crítica ...
sem_post(sem);       // liberar

// Intentar sin bloquear
if (sem_trywait(sem) == -1) {
    if (errno == EAGAIN) {
        printf("Recurso ocupado, intentar más tarde\n");
    }
}

// Esperar con timeout (tiempo absoluto, no relativo)
struct timespec ts;
clock_gettime(CLOCK_REALTIME, &ts);
ts.tv_sec += 5;  // timeout de 5 segundos

if (sem_timedwait(sem, &ts) == -1) {
    if (errno == ETIMEDOUT) {
        printf("Timeout: no se obtuvo el semáforo en 5s\n");
    }
}
```

### 2.3 Cerrar y eliminar

```c
int sem_close(sem_t *sem);      // cerrar en este proceso
int sem_unlink(const char *name);  // eliminar del filesystem
```

`sem_close` desconecta el semáforo del proceso actual pero no lo
destruye. `sem_unlink` elimina el nombre; el semáforo se destruye
cuando todos los procesos lo cierran (como `unlink` para archivos).

```c
sem_close(sem);
sem_unlink("/myapp_lock");  // solo el "dueño" debería hacer esto
```

### 2.4 Ejemplo: exclusión mutua entre procesos

Dos instancias del mismo programa compiten por un recurso:

```c
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <fcntl.h>
#include <unistd.h>

#define SEM_NAME "/demo_mutex"

int main(void)
{
    // O_CREAT sin O_EXCL: crear si no existe, abrir si existe
    sem_t *sem = sem_open(SEM_NAME, O_CREAT, 0666, 1);
    if (sem == SEM_FAILED) { perror("sem_open"); exit(1); }

    printf("[%d] Waiting for lock...\n", getpid());
    sem_wait(sem);

    printf("[%d] Got lock! Working for 5 seconds...\n", getpid());
    sleep(5);
    printf("[%d] Done, releasing lock\n", getpid());

    sem_post(sem);
    sem_close(sem);

    return 0;
}
```

```bash
# Terminal 1               # Terminal 2
$ ./prog                   $ ./prog
[1234] Waiting...          [5678] Waiting...
[1234] Got lock!           # (bloqueado 5 segundos)
[1234] Done, releasing     [5678] Got lock!
                           [5678] Done, releasing
```

---

## 3. Semáforos POSIX sin nombre: sem_init

Los semáforos sin nombre no tienen identidad en el filesystem. Viven
en memoria proporcionada por el usuario: una variable global (para
threads), o una región de memoria compartida (para procesos).

### 3.1 Crear y destruir

```c
int sem_init(sem_t *sem, int pshared, unsigned int value);
// sem: puntero a memoria del usuario (NO de sem_open)
// pshared: 0 = entre threads, !=0 = entre procesos
// value: valor inicial

int sem_destroy(sem_t *sem);
```

### 3.2 Entre threads

```c
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>

sem_t sem;
int shared_counter = 0;

void *worker(void *arg)
{
    for (int i = 0; i < 100000; i++) {
        sem_wait(&sem);
        shared_counter++;
        sem_post(&sem);
    }
    return NULL;
}

int main(void)
{
    sem_init(&sem, 0, 1);  // pshared=0: solo threads

    pthread_t t1, t2;
    pthread_create(&t1, NULL, worker, NULL);
    pthread_create(&t2, NULL, worker, NULL);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    printf("Counter: %d (expected 200000)\n", shared_counter);

    sem_destroy(&sem);
    return 0;
}
```

### 3.3 Entre procesos (en memoria compartida)

Para usar `sem_init` entre procesos, el semáforo debe estar en memoria
compartida y `pshared` debe ser distinto de 0:

```c
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    sem_t sem;
    int counter;
} shared_t;

int main(void)
{
    // Memoria compartida anónima (padre↔hijo)
    shared_t *shm = mmap(NULL, sizeof(shared_t),
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shm == MAP_FAILED) { perror("mmap"); exit(1); }

    sem_init(&shm->sem, 1, 1);  // pshared=1: entre procesos
    shm->counter = 0;

    pid_t pid = fork();
    if (pid == 0) {
        for (int i = 0; i < 100000; i++) {
            sem_wait(&shm->sem);
            shm->counter++;
            sem_post(&shm->sem);
        }
        _exit(0);
    }

    for (int i = 0; i < 100000; i++) {
        sem_wait(&shm->sem);
        shm->counter++;
        sem_post(&shm->sem);
    }

    waitpid(pid, NULL, 0);
    printf("Counter: %d (expected 200000)\n", shm->counter);

    sem_destroy(&shm->sem);
    munmap(shm, sizeof(shared_t));
    return 0;
}
```

### 3.4 sem_open vs sem_init

```
┌──────────────────┬─────────────────────┬─────────────────────┐
│ Aspecto          │ sem_open (con nombre)│ sem_init (sin nombre)│
├──────────────────┼─────────────────────┼─────────────────────┤
│ Identificación   │ Nombre "/xxx"       │ Dirección en memoria│
├──────────────────┼─────────────────────┼─────────────────────┤
│ Entre procesos   │ Sí (cualquier proc) │ Sí (si pshared=1    │
│ no relacionados  │                     │ + memoria compartida)│
├──────────────────┼─────────────────────┼─────────────────────┤
│ Entre threads    │ Sí (compartir ptr)  │ Sí (pshared=0)      │
├──────────────────┼─────────────────────┼─────────────────────┤
│ Persistencia     │ Filesystem (/dev/shm)│ Memoria del usuario │
├──────────────────┼─────────────────────┼─────────────────────┤
│ Limpieza         │ sem_close+sem_unlink│ sem_destroy          │
├──────────────────┼─────────────────────┼─────────────────────┤
│ Cuándo usar      │ Procesos sin relación│ Threads, o padre/   │
│                  │ de parentesco       │ hijo con mmap       │
└──────────────────┴─────────────────────┴─────────────────────┘
```

---

## 4. System V: semget, semop, semctl

La API System V es significativamente más compleja. Un "semáforo System V"
es en realidad un **conjunto** (array) de semáforos, no uno solo.

### 4.1 semget — crear u obtener conjunto

```c
#include <sys/sem.h>

int semget(key_t key, int nsems, int semflg);
// key: clave IPC (ftok) o IPC_PRIVATE
// nsems: número de semáforos en el conjunto
// semflg: IPC_CREAT | permisos
// Retorna: semid, o -1 en error
```

```c
key_t key = ftok("/tmp/semfile", 'S');
int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
if (semid == -1) {
    if (errno == EEXIST)
        semid = semget(key, 0, 0);
    else {
        perror("semget"); exit(1);
    }
}
```

### 4.2 semctl — control e inicialización

```c
int semctl(int semid, int semnum, int cmd, ...);
// semnum: índice del semáforo en el conjunto (0-based)
```

Comandos principales:

```
┌──────────┬──────────────────────────────────────────────────┐
│ Comando  │ Efecto                                          │
├──────────┼──────────────────────────────────────────────────┤
│ SETVAL   │ Establecer valor de un semáforo                 │
│ GETVAL   │ Obtener valor actual                            │
│ SETALL   │ Establecer todos los valores (array)            │
│ GETALL   │ Obtener todos los valores                       │
│ IPC_RMID │ Eliminar el conjunto completo                   │
│ IPC_STAT │ Obtener información (struct semid_ds)           │
│ GETNCNT  │ Procesos esperando que el valor aumente         │
│ GETZCNT  │ Procesos esperando que el valor llegue a 0      │
└──────────┴──────────────────────────────────────────────────┘
```

Para `SETVAL` y `SETALL` se necesita un argumento union:

```c
// Definir la unión (no siempre la define el sistema)
union semun {
    int val;               // SETVAL
    struct semid_ds *buf;  // IPC_STAT, IPC_SET
    unsigned short *array; // SETALL, GETALL
};

// Inicializar semáforo 0 con valor 1
union semun arg;
arg.val = 1;
if (semctl(semid, 0, SETVAL, arg) == -1) {
    perror("semctl SETVAL");
    exit(1);
}
```

**Problema de inicialización**: hay una race condition entre `semget`
(crear) y `semctl(SETVAL)` (inicializar). Otro proceso podría hacer
`semop` entre ambas llamadas, operando sobre un semáforo no inicializado
(valor 0). Soluciones:
- Usar `IPC_EXCL` para que solo un proceso cree e inicialice
- Verificar `sem_otime` (tiempo de última operación): si es 0, el
  semáforo aún no ha sido usado

### 4.3 semop — operaciones atómicas

```c
int semop(int semid, struct sembuf *sops, size_t nsops);

struct sembuf {
    unsigned short sem_num;  // índice del semáforo
    short          sem_op;   // operación
    short          sem_flg;  // flags
};
```

El campo `sem_op` determina la operación:

```
┌──────────┬──────────────────────────────────────────────────┐
│ sem_op   │ Comportamiento                                  │
├──────────┼──────────────────────────────────────────────────┤
│ > 0      │ Sumar sem_op al valor (post/release)            │
│ < 0      │ Si valor ≥ |sem_op|: restar                     │
│          │ Si no: bloquear hasta que sea posible            │
│ == 0     │ Bloquear hasta que el valor sea exactamente 0   │
└──────────┴──────────────────────────────────────────────────┘
```

Flags:

```
┌──────────────┬──────────────────────────────────────────────┐
│ Flag         │ Efecto                                      │
├──────────────┼──────────────────────────────────────────────┤
│ IPC_NOWAIT   │ No bloquear (retornar EAGAIN)               │
│ SEM_UNDO     │ Deshacer la operación al terminar el proceso│
└──────────────┴──────────────────────────────────────────────┘
```

```c
// Wait (P): decrementar en 1
struct sembuf op_wait = {
    .sem_num = 0,       // primer semáforo del conjunto
    .sem_op  = -1,      // decrementar
    .sem_flg = SEM_UNDO // deshacer si el proceso muere
};
if (semop(semid, &op_wait, 1) == -1) {
    perror("semop wait");
}

// Post (V): incrementar en 1
struct sembuf op_post = {
    .sem_num = 0,
    .sem_op  = 1,
    .sem_flg = SEM_UNDO
};
if (semop(semid, &op_post, 1) == -1) {
    perror("semop post");
}
```

### 4.4 SEM_UNDO: recuperación automática

`SEM_UNDO` es la killer feature de los semáforos System V. Si un proceso
muere mientras tiene un semáforo adquirido, el kernel **deshace** la
operación automáticamente. Esto previene deadlocks permanentes:

```
  Sin SEM_UNDO:
  Proceso A: semop(-1) → valor = 0
  Proceso A: kill -9 ─────────────── valor queda en 0 PARA SIEMPRE
  Proceso B: semop(-1) → bloqueado eternamente (deadlock)

  Con SEM_UNDO:
  Proceso A: semop(-1, SEM_UNDO) → valor = 0, undo_count = +1
  Proceso A: kill -9 ─────────────── kernel aplica undo: valor += 1 = 1
  Proceso B: semop(-1) → valor = 0 (continúa normalmente)
```

El kernel mantiene un registro de ajustes (`semadj`) por proceso.
Al terminar el proceso, aplica todos los ajustes pendientes.

### 4.5 Operaciones atómicas sobre múltiples semáforos

La gran ventaja de `semop`: puede operar sobre **múltiples semáforos**
atómicamente en una sola llamada:

```c
// Adquirir dos recursos atómicamente (sin riesgo de deadlock)
struct sembuf ops[2] = {
    {.sem_num = 0, .sem_op = -1, .sem_flg = SEM_UNDO},  // recurso A
    {.sem_num = 1, .sem_op = -1, .sem_flg = SEM_UNDO},  // recurso B
};
// AMBAS operaciones se ejecutan atómicamente o ninguna
semop(semid, ops, 2);

// Liberar ambos
struct sembuf rel[2] = {
    {.sem_num = 0, .sem_op = 1, .sem_flg = SEM_UNDO},
    {.sem_num = 1, .sem_op = 1, .sem_flg = SEM_UNDO},
};
semop(semid, rel, 2);
```

Esto resuelve el problema del **dining philosophers**: adquirir ambos
tenedores atómicamente evita deadlock sin necesidad de ordenamiento.

### 4.6 semtimedop — con timeout

```c
#include <sys/sem.h>

int semtimedop(int semid, struct sembuf *sops, size_t nsops,
               const struct timespec *timeout);
// timeout: tiempo RELATIVO (a diferencia de sem_timedwait que es absoluto)
```

```c
struct timespec ts = {.tv_sec = 3, .tv_nsec = 0};  // 3 segundos
struct sembuf op = {0, -1, SEM_UNDO};

if (semtimedop(semid, &op, 1, &ts) == -1) {
    if (errno == EAGAIN) {
        printf("Timeout\n");
    }
}
```

### 4.7 Ejemplo completo System V

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <unistd.h>

union semun {
    int val;
    struct semid_ds *buf;
    unsigned short *array;
};

// Helper: wait (P)
void sem_p(int semid, int semnum)
{
    struct sembuf op = {semnum, -1, SEM_UNDO};
    if (semop(semid, &op, 1) == -1) {
        perror("semop P"); exit(1);
    }
}

// Helper: post (V)
void sem_v(int semid, int semnum)
{
    struct sembuf op = {semnum, 1, SEM_UNDO};
    if (semop(semid, &op, 1) == -1) {
        perror("semop V"); exit(1);
    }
}

int main(void)
{
    key_t key = ftok("/tmp/semexample", 'A');
    int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);

    int is_creator = (semid != -1);
    if (!is_creator) {
        semid = semget(key, 0, 0);
        if (semid == -1) { perror("semget"); exit(1); }
    } else {
        // Solo el creador inicializa
        union semun arg = {.val = 1};
        semctl(semid, 0, SETVAL, arg);
    }

    printf("[%d] Acquiring semaphore...\n", getpid());
    sem_p(semid, 0);
    printf("[%d] Got it! Working...\n", getpid());
    sleep(3);
    printf("[%d] Releasing\n", getpid());
    sem_v(semid, 0);

    // Solo el creador limpia
    if (is_creator) {
        semctl(semid, 0, IPC_RMID);
    }

    return 0;
}
```

---

## 5. Patrones clásicos

### 5.1 Productor-Consumidor con buffer limitado

El problema clásico: un buffer de N slots, un productor que inserta y
un consumidor que extrae. Tres semáforos:

```
  empty = N   (slots vacíos disponibles)
  full  = 0   (slots con datos disponibles)
  mutex = 1   (proteger acceso al buffer)

  Productor:                      Consumidor:
  ──────────                      ──────────
  sem_wait(empty)   // hay slot?  sem_wait(full)    // hay dato?
  sem_wait(mutex)   // exclusión  sem_wait(mutex)   // exclusión
  buffer[in] = item               item = buffer[out]
  in = (in+1) % N                 out = (out+1) % N
  sem_post(mutex)                 sem_post(mutex)
  sem_post(full)    // hay dato   sem_post(empty)   // hay slot
```

```c
#include <stdio.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>
#include <unistd.h>

#define BUFFER_SIZE 5

int buffer[BUFFER_SIZE];
int in_idx = 0, out_idx = 0;

sem_t sem_empty;  // slots vacíos
sem_t sem_full;   // slots con datos
sem_t sem_mutex;  // exclusión mutua

void *producer(void *arg)
{
    for (int i = 1; i <= 20; i++) {
        sem_wait(&sem_empty);        // esperar slot vacío
        sem_wait(&sem_mutex);        // entrar a sección crítica

        buffer[in_idx] = i;
        printf("Produced: %d [slot %d]\n", i, in_idx);
        in_idx = (in_idx + 1) % BUFFER_SIZE;

        sem_post(&sem_mutex);        // salir de sección crítica
        sem_post(&sem_full);         // señalar dato disponible

        usleep(rand() % 100000);
    }
    return NULL;
}

void *consumer(void *arg)
{
    for (int i = 0; i < 20; i++) {
        sem_wait(&sem_full);         // esperar dato
        sem_wait(&sem_mutex);        // entrar a sección crítica

        int item = buffer[out_idx];
        printf("Consumed: %d [slot %d]\n", item, out_idx);
        out_idx = (out_idx + 1) % BUFFER_SIZE;

        sem_post(&sem_mutex);        // salir de sección crítica
        sem_post(&sem_empty);        // señalar slot libre

        usleep(rand() % 150000);
    }
    return NULL;
}

int main(void)
{
    sem_init(&sem_empty, 0, BUFFER_SIZE);  // N slots vacíos
    sem_init(&sem_full, 0, 0);             // 0 datos inicialmente
    sem_init(&sem_mutex, 0, 1);            // exclusión mutua

    pthread_t prod, cons;
    pthread_create(&prod, NULL, producer, NULL);
    pthread_create(&cons, NULL, consumer, NULL);

    pthread_join(prod, NULL);
    pthread_join(cons, NULL);

    sem_destroy(&sem_empty);
    sem_destroy(&sem_full);
    sem_destroy(&sem_mutex);

    return 0;
}
```

### 5.2 Pool de recursos (semáforo contador)

Limitar el número de conexiones simultáneas a una base de datos:

```c
#define MAX_CONNECTIONS 5

sem_t conn_pool;

void init_pool(void)
{
    sem_init(&conn_pool, 0, MAX_CONNECTIONS);
}

void *handle_request(void *arg)
{
    int req_id = *(int *)arg;

    sem_wait(&conn_pool);  // adquirir una conexión del pool
    printf("Request %d: got connection\n", req_id);

    // Simular query a la base de datos
    sleep(2);

    printf("Request %d: releasing connection\n", req_id);
    sem_post(&conn_pool);  // devolver conexión al pool

    return NULL;
}
```

### 5.3 Barrera de sincronización

Hacer que N procesos esperen a que todos lleguen a un punto antes
de continuar:

```c
typedef struct {
    sem_t mutex;
    sem_t barrier;
    int count;
    int n;
} barrier_t;

void barrier_init(barrier_t *b, int n)
{
    sem_init(&b->mutex, 1, 1);    // pshared=1 para procesos
    sem_init(&b->barrier, 1, 0);  // bloqueado inicialmente
    b->count = 0;
    b->n = n;
}

void barrier_wait(barrier_t *b)
{
    sem_wait(&b->mutex);
    b->count++;
    if (b->count == b->n) {
        // Último en llegar: despertar a todos
        for (int i = 0; i < b->n - 1; i++)
            sem_post(&b->barrier);
        b->count = 0;  // resetear para reutilización
        sem_post(&b->mutex);
    } else {
        sem_post(&b->mutex);
        sem_wait(&b->barrier);  // esperar a que todos lleguen
    }
}
```

### 5.4 Lectores-Escritores (prioridad a lectores)

Múltiples lectores simultáneos, pero solo un escritor exclusivo:

```c
typedef struct {
    sem_t rw_mutex;     // acceso exclusivo para escritores
    sem_t count_mutex;  // proteger reader_count
    int reader_count;
} rw_lock_t;

void rw_init(rw_lock_t *rw)
{
    sem_init(&rw->rw_mutex, 0, 1);
    sem_init(&rw->count_mutex, 0, 1);
    rw->reader_count = 0;
}

void read_lock(rw_lock_t *rw)
{
    sem_wait(&rw->count_mutex);
    rw->reader_count++;
    if (rw->reader_count == 1)
        sem_wait(&rw->rw_mutex);   // primer lector bloquea escritores
    sem_post(&rw->count_mutex);
}

void read_unlock(rw_lock_t *rw)
{
    sem_wait(&rw->count_mutex);
    rw->reader_count--;
    if (rw->reader_count == 0)
        sem_post(&rw->rw_mutex);   // último lector libera escritores
    sem_post(&rw->count_mutex);
}

void write_lock(rw_lock_t *rw)
{
    sem_wait(&rw->rw_mutex);       // acceso exclusivo
}

void write_unlock(rw_lock_t *rw)
{
    sem_post(&rw->rw_mutex);
}
```

**Problema**: si lectores llegan continuamente, un escritor puede
esperar indefinidamente (starvation). La solución con prioridad a
escritores requiere un semáforo adicional.

---

## 6. Semáforos vs mutexes

```
┌────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto            │ Mutex                │ Semáforo             │
├────────────────────┼──────────────────────┼──────────────────────┤
│ Valores posibles   │ 0 (locked) o         │ 0, 1, 2, ... N      │
│                    │ 1 (unlocked)         │                      │
├────────────────────┼──────────────────────┼──────────────────────┤
│ Propiedad          │ Solo el thread que   │ Cualquier thread     │
│                    │ hizo lock puede      │ puede hacer post     │
│                    │ hacer unlock         │                      │
├────────────────────┼──────────────────────┼──────────────────────┤
│ Propósito          │ Exclusión mutua      │ Señalización /       │
│                    │ (proteger datos)     │ conteo de recursos   │
├────────────────────┼──────────────────────┼──────────────────────┤
│ Recursive          │ PTHREAD_MUTEX_       │ No (pero valor > 1   │
│                    │ RECURSIVE            │ permite múltiples)   │
├────────────────────┼──────────────────────┼──────────────────────┤
│ Robusto            │ PTHREAD_MUTEX_ROBUST │ SEM_UNDO (SysV)     │
│ (death handling)   │                      │                      │
├────────────────────┼──────────────────────┼──────────────────────┤
│ Priority           │ Sí (PI mutexes)      │ No                   │
│ inheritance        │                      │                      │
├────────────────────┼──────────────────────┼──────────────────────┤
│ Condvar            │ Sí (pthread_cond)    │ No (el semáforo ES   │
│                    │                      │ la señalización)     │
└────────────────────┴──────────────────────┴──────────────────────┘
```

**Cuándo usar cada uno**:

- **Mutex**: proteger acceso a datos compartidos. El mismo thread que
  adquiere debe liberar. Es más eficiente y tiene mejores diagnósticos
  (detección de deadlock, errorchecking).

- **Semáforo binario**: señalización entre threads/procesos. Un thread
  señala (`post`) y otro espera (`wait`). No hay concepto de "propietario".

- **Semáforo contador**: limitar acceso concurrente a N instancias de
  un recurso (pool de conexiones, workers, etc.).

---

## 7. Problemas y trampas

### Priority inversion

```
  Thread L (baja prioridad): adquiere semáforo
  Thread M (media prioridad): ejecuta trabajo CPU-intensivo
  Thread H (alta prioridad): wait(sem) → bloqueado

  Resultado: H espera a L, pero M impide que L ejecute
  → H efectivamente tiene la prioridad de L (inversión)
```

Los mutexes con `PTHREAD_PRIO_INHERIT` resuelven esto elevando
temporalmente la prioridad de L. Los semáforos no tienen propiedad,
así que no pueden implementar priority inheritance.

### EINTR: signals interrumpen sem_wait

`sem_wait` puede ser interrumpido por una señal (retorna -1 con
`errno == EINTR`). Siempre reintentar:

```c
int safe_sem_wait(sem_t *sem)
{
    while (sem_wait(sem) == -1) {
        if (errno != EINTR) return -1;
        // EINTR: señal interrumpió, reintentar
    }
    return 0;
}
```

### Máximo valor de un semáforo POSIX

El estándar requiere al menos 32767 (`SEM_VALUE_MAX`). En Linux es
normalmente `INT_MAX` (2147483647). Hacer `sem_post` más allá del
máximo retorna `EOVERFLOW`.

```c
#include <limits.h>
printf("SEM_VALUE_MAX = %d\n", SEM_VALUE_MAX);
```

### sem_open en macOS

macOS no soporta semáforos sin nombre (`sem_init`). Solo funciona
`sem_open`. Si escribes código portátil, prefiere `sem_open` o
usa `dispatch_semaphore` en macOS.

---

## 8. Herramientas de diagnóstico

### POSIX

```bash
# Listar semáforos con nombre
$ ls -la /dev/shm/sem.*
-rw-r--r-- 1 user user 32 Mar 20 10:00 /dev/shm/sem.myapp_lock

# Eliminar manualmente
$ rm /dev/shm/sem.myapp_lock
```

### System V

```bash
# Listar semáforos
$ ipcs -s
------ Semaphore Arrays --------
key        semid      owner      perms      nsems
0x41010105 0          user       666        1

# Detalle: valores actuales
$ ipcs -s -i <semid>
Semaphore Array semid=0
uid=1000  gid=1000  cuid=1000  cgid=1000
mode=0666, access_perms=0666
nsems = 1
otime = Thu Mar 20 10:00:00 2026
ctime = Thu Mar 20 09:59:50 2026
semnum     value      ncount     zcount     pid
0          1          0          0          1234

# Eliminar
$ ipcrm -s <semid>

# Límites del sistema
$ ipcs -s -l
------ Semaphore Limits --------
max number of arrays = 32000
max semaphores per array = 32000
max semaphores system wide = 1024000000
max ops per semop call = 500
semaphore max value = 32767
```

### /proc

```bash
# Límites configurables
$ cat /proc/sys/kernel/sem
# SEMMSL  SEMMNS  SEMOPM  SEMMNI
# 32000   1024000000  500  32000
```

Los cuatro valores: máximo de semáforos por conjunto, total en el
sistema, máximo de operaciones por `semop`, máximo de conjuntos.

---

## 9. Errores comunes

### Error 1: No inicializar semáforos System V

```c
// MAL: race condition entre crear y usar
int semid = semget(key, 1, IPC_CREAT | 0666);
// Otro proceso puede hacer semop AQUÍ, con valor no inicializado (0)
// → se bloquea inesperadamente o actúa con valor basura
union semun arg = {.val = 1};
semctl(semid, 0, SETVAL, arg);

// BIEN: usar IPC_EXCL para que solo el creador inicialice
int semid = semget(key, 1, IPC_CREAT | IPC_EXCL | 0666);
if (semid != -1) {
    // Solo este proceso llegó primero → inicializar
    union semun arg = {.val = 1};
    semctl(semid, 0, SETVAL, arg);
} else if (errno == EEXIST) {
    semid = semget(key, 0, 0);
    // Esperar a que el creador inicialice (verificar sem_otime != 0)
}
```

**Por qué importa**: un semáforo SysV no inicializado tiene valor 0
(o indefinido). Si un proceso intenta `sem_wait(-1)`, se bloquea.
Si tiene un valor aleatorio alto, la exclusión mutua falla.

### Error 2: post sin wait previo (desbordamiento)

```c
// MAL: llamar post sin haber hecho wait → valor crece sin límite
for (int i = 0; i < 1000; i++) {
    sem_post(&sem);  // valor sube a 1000
}
// Ahora 1000 threads pueden entrar a la "sección crítica" a la vez

// BIEN: siempre emparejar wait/post
sem_wait(&sem);
// ... sección crítica ...
sem_post(&sem);
```

### Error 3: No manejar EINTR

```c
// MAL: sem_wait retorna -1 por una señal, no se reintenta
if (sem_wait(sem) == -1) {
    perror("sem_wait");
    exit(1);  // ¡sale del programa por una señal inocente!
}

// BIEN: reintentar en EINTR
while (sem_wait(sem) == -1) {
    if (errno == EINTR) continue;  // señal, reintentar
    perror("sem_wait");
    exit(1);
}
```

**Por qué importa**: si tu programa maneja SIGCHLD, SIGALRM, o cualquier
otra señal, `sem_wait` se interrumpirá. Ignorar EINTR causa que la
sección crítica se ejecute sin el semáforo adquirido.

### Error 4: sem_destroy o sem_unlink mientras alguien espera

```c
// MAL: destruir semáforo con threads bloqueados en sem_wait
sem_destroy(&sem);  // comportamiento indefinido si alguien está en sem_wait

// BIEN: asegurar que nadie está esperando antes de destruir
// 1. Señalar a todos que deben terminar (flag + post suficientes)
should_exit = 1;
for (int i = 0; i < n_waiters; i++)
    sem_post(&sem);  // despertar a cada waiter
// 2. Esperar a que todos terminen (join threads / wait procesos)
// 3. Ahora sí destruir
sem_destroy(&sem);
```

### Error 5: Olvidar sem_unlink para semáforos con nombre

```c
// MAL: el semáforo persiste en /dev/shm/sem.mylock para siempre
sem_t *sem = sem_open("/mylock", O_CREAT, 0666, 1);
// ... usar ...
sem_close(sem);
// Fin: /dev/shm/sem.mylock sigue ahí

// BIEN: limpiar con sem_unlink
sem_close(sem);
sem_unlink("/mylock");

// O unlink tempranamente (como con archivos):
sem_t *sem = sem_open("/mylock", O_CREAT, 0666, 1);
sem_unlink("/mylock");  // eliminar nombre inmediatamente
// El semáforo funciona mientras haya procesos con sem_open
// Se destruye automáticamente cuando el último hace sem_close
```

**Patrón**: llamar `sem_unlink` justo después de `sem_open` si todos
los procesos que lo necesitan ya lo abrieron o son hijos del creador.

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                        SEMÁFOROS                                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  POSIX con nombre (entre procesos no relacionados):                 │
│    sem = sem_open("/name", O_CREAT, 0666, valor_inicial)           │
│    sem_wait(sem)        // P: decrementar o bloquear               │
│    sem_post(sem)        // V: incrementar, despertar               │
│    sem_trywait(sem)     // no bloqueante (EAGAIN)                  │
│    sem_timedwait(sem, &abs_ts)  // con timeout                     │
│    sem_getvalue(sem, &val)                                          │
│    sem_close(sem)       // cerrar en este proceso                  │
│    sem_unlink("/name")  // eliminar del filesystem                 │
│                                                                     │
│  POSIX sin nombre (threads o procesos con mmap):                    │
│    sem_init(&sem, pshared, valor)  // pshared: 0=threads, 1=procs  │
│    sem_wait/post/trywait/timedwait/getvalue (igual que arriba)      │
│    sem_destroy(&sem)                                                │
│                                                                     │
│  System V:                                                          │
│    semid = semget(key, nsems, IPC_CREAT|0666)                      │
│    semctl(semid, semnum, SETVAL, (union semun){.val=N})             │
│    semop(semid, &(struct sembuf){0, -1, SEM_UNDO}, 1)  // wait    │
│    semop(semid, &(struct sembuf){0, +1, SEM_UNDO}, 1)  // post    │
│    semtimedop(semid, sops, nsops, &rel_timeout)                    │
│    semctl(semid, 0, IPC_RMID)     // eliminar                      │
│                                                                     │
│  Patrones:                                                          │
│    Mutex:      valor=1, wait/post alrededor de sección crítica     │
│    Señalización: valor=0, wait espera evento, post señala evento    │
│    Pool(N):   valor=N, wait adquiere recurso, post lo devuelve     │
│    Prod-Cons: empty=N + full=0 + mutex=1 (3 semáforos)             │
│                                                                     │
│  Reglas de oro:                                                     │
│    1. SIEMPRE manejar EINTR en sem_wait                            │
│    2. SEM_UNDO en SysV para evitar deadlock si el proceso muere    │
│    3. sem_unlink para limpiar semáforos con nombre                  │
│    4. No destruir semáforo con waiters pendientes                  │
│    5. Emparejar cada wait con exactamente un post                   │
│                                                                     │
│  Compilar: gcc -o prog prog.c -lpthread [-lrt]                     │
│  Diagnóstico: ls /dev/shm/sem.* (POSIX) | ipcs -s (SysV)          │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Productor-Consumidor multi-proceso

Implementa el problema productor-consumidor con **procesos** (no threads)
usando memoria compartida POSIX + semáforos con nombre. Lanza P
productores y C consumidores como procesos independientes:

- Buffer circular de N slots en `shm_open`/`mmap`
- 3 semáforos con nombre: `/buf_empty` (N), `/buf_full` (0), `/buf_mutex` (1)
- Cada productor genera M ítems (enteros secuenciales con PID como prefijo)
- Cada consumidor lee e imprime lo que recibe
- El primer proceso que se lanza crea todo; los demás se conectan
- Al final, verificar que no se perdió ningún ítem

```bash
# En 4 terminales:
$ ./producer 1     # produce items 1001-1020
$ ./producer 2     # produce items 2001-2020
$ ./consumer A     # imprime lo que llegue
$ ./consumer B     # imprime lo que llegue
```

**Pregunta de reflexión**: Si un consumidor muere con `kill -9` mientras
tiene `buf_mutex` adquirido, ¿qué pasa con el sistema? ¿Cómo se
compara usar semáforos con nombre vs `pthread_mutex` con
`PTHREAD_MUTEX_ROBUST` en memoria compartida para manejar este caso?

### Ejercicio 2: Rate limiter con semáforo contador

Implementa un rate limiter que permita máximo N operaciones por segundo
usando un semáforo contador. Un thread "recargador" hace `sem_post` N
veces cada segundo, y los threads de trabajo hacen `sem_wait` antes de
cada operación:

```c
// rate_limiter.h
typedef struct {
    sem_t tokens;
    int max_per_second;
    pthread_t refiller;
    volatile int running;
} rate_limiter_t;

int rl_init(rate_limiter_t *rl, int max_per_second);
void rl_acquire(rate_limiter_t *rl);   // bloquea si se agotó el rate
void rl_destroy(rate_limiter_t *rl);
```

Verificar que con `max_per_second=10` y 50 threads, solo se ejecutan
~10 operaciones por segundo. Medir la distribución temporal.

**Pregunta de reflexión**: ¿Qué pasa si las operaciones son tan lentas
que los tokens se acumulan (más de N en el semáforo)? ¿Cómo podrías
limitar el número máximo de tokens acumulados (token bucket vs leaky
bucket)?

### Ejercicio 3: Dining philosophers

Implementa el problema de los filósofos comensales con 5 filósofos y
5 tenedores. Compara tres soluciones:

1. **Naïve (con deadlock)**: cada filósofo toma tenedor izquierdo, luego
   derecho → demostrar que se produce deadlock
2. **Ordenamiento de recursos**: tomar siempre el tenedor de menor
   número primero
3. **System V atómico**: usar `semop` con 2 operaciones para tomar ambos
   tenedores atómicamente

Para cada solución, medir cuántas comidas completan los 5 filósofos en
10 segundos y si se produce deadlock o starvation.

```c
#define N_PHILOSOPHERS 5
#define EAT_TIME_US    10000   // 10ms
#define THINK_TIME_US  10000   // 10ms
```

**Pregunta de reflexión**: La solución 3 (semop atómico) ¿tiene el
mismo rendimiento que la solución 2 (ordenamiento)? ¿Cuál permite más
paralelismo? ¿Qué pasa si un filósofo muere con `kill -9` teniendo
ambos tenedores en cada solución?
