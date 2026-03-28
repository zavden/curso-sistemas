# Memoria compartida

## Índice
1. [Concepto y motivación](#1-concepto-y-motivación)
2. [System V: shmget, shmat, shmdt, shmctl](#2-system-v-shmget-shmat-shmdt-shmctl)
3. [POSIX: shm_open y mmap](#3-posix-shm_open-y-mmap)
4. [Sincronización: el problema fundamental](#4-sincronización-el-problema-fundamental)
5. [Patrones de uso](#5-patrones-de-uso)
6. [Persistencia y limpieza](#6-persistencia-y-limpieza)
7. [Herramientas de diagnóstico](#7-herramientas-de-diagnóstico)
8. [Comparación System V vs POSIX](#8-comparación-system-v-vs-posix)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Concepto y motivación

Los mecanismos IPC que hemos visto hasta ahora (pipes, FIFOs) tienen una
limitación: los datos se **copian** entre procesos a través del kernel.
El productor hace `write()` (copia usuario→kernel), el consumidor hace
`read()` (copia kernel→usuario). Para grandes volúmenes de datos, estas
dos copias son costosas.

La memoria compartida elimina ambas copias: múltiples procesos mapean
la **misma región de memoria física** en sus espacios de direcciones
virtuales. Un proceso escribe directamente y el otro lee directamente,
sin intermediación del kernel:

```
  Pipes / FIFOs:
  ┌──────────┐    write()    ┌──────────┐    read()     ┌──────────┐
  │ Proceso A│ ────────────► │  Kernel  │ ────────────► │ Proceso B│
  │  (user)  │  copia 1     │ (buffer) │  copia 2     │  (user)  │
  └──────────┘               └──────────┘               └──────────┘

  Memoria compartida:
  ┌──────────┐                                          ┌──────────┐
  │ Proceso A│                                          │ Proceso B│
  │          │    ┌──────────────────────┐              │          │
  │  ptr ────┼───►│  Región compartida   │◄────── ptr  ┼──────────┤
  │          │    │  (memoria física)    │              │          │
  └──────────┘    └──────────────────────┘              └──────────┘
                    0 copias, acceso directo
```

**Ventaja**: el mecanismo IPC más rápido — velocidad de acceso a memoria.

**Desventaja**: sin sincronización incluida. Los procesos deben coordinar
el acceso (semáforos, mutexes, etc.) para evitar race conditions. Pipes
y colas de mensajes incluyen sincronización implícita (bloquean al
leer/escribir); la memoria compartida no.

---

## 2. System V: shmget, shmat, shmdt, shmctl

La API System V para memoria compartida usa un modelo de cuatro pasos:
crear/obtener → adjuntar → usar → desadjuntar → eliminar.

### 2.1 Claves IPC: ftok

System V identifica los recursos IPC con claves numéricas (`key_t`).
`ftok()` genera una clave a partir de un pathname y un id de proyecto:

```c
#include <sys/ipc.h>

key_t ftok(const char *pathname, int proj_id);
// pathname: debe existir y ser accesible
// proj_id: solo se usan los 8 bits bajos (1-255)
// Retorna: clave IPC, o -1 en error
```

```c
key_t key = ftok("/tmp/myapp", 'A');  // 'A' = 65
if (key == -1) {
    perror("ftok");
    exit(1);
}
```

**Cuidado**: `ftok` usa el inode del archivo. Si el archivo se borra y
se recrea, el inode cambia → clave diferente → segmento diferente.
Alternativa: usar `IPC_PRIVATE` para crear sin clave (solo útil entre
padre e hijos tras fork).

### 2.2 shmget — crear u obtener segmento

```c
#include <sys/shm.h>

int shmget(key_t key, size_t size, int shmflg);
// Retorna: ID del segmento (shmid), o -1 en error
```

Flags comunes:

```
┌─────────────────┬────────────────────────────────────────────┐
│ Flag            │ Significado                                │
├─────────────────┼────────────────────────────────────────────┤
│ IPC_CREAT       │ Crear si no existe                        │
│ IPC_EXCL        │ Fallar si ya existe (con IPC_CREAT)       │
│ 0666            │ Permisos (lectura/escritura para todos)   │
│ IPC_PRIVATE     │ key = 0, crea siempre uno nuevo           │
└─────────────────┴────────────────────────────────────────────┘
```

```c
// Crear un segmento nuevo de 4096 bytes
int shmid = shmget(key, 4096, IPC_CREAT | IPC_EXCL | 0666);
if (shmid == -1) {
    if (errno == EEXIST) {
        // Ya existe, obtener el existente
        shmid = shmget(key, 0, 0);
    } else {
        perror("shmget");
        exit(1);
    }
}
```

El tamaño se redondea al múltiplo superior de `PAGE_SIZE` (normalmente
4096). Solicitar 1 byte reserva una página completa.

### 2.3 shmat — adjuntar al espacio de direcciones

```c
void *shmat(int shmid, const void *shmaddr, int shmflg);
// shmaddr: NULL = kernel elige dirección (recomendado)
// shmflg: SHM_RDONLY para solo lectura, 0 para lectura/escritura
// Retorna: puntero a la región, o (void *)-1 en error
```

```c
char *data = shmat(shmid, NULL, 0);
if (data == (void *)-1) {
    perror("shmat");
    exit(1);
}

// Ahora puedes usar 'data' como cualquier puntero
sprintf(data, "Hello from PID %d", getpid());
printf("Leído: %s\n", data);
```

**Después de fork**: el hijo hereda los segmentos adjuntados. Ambos
procesos ven la misma memoria física — esto es lo que hace útil
`IPC_PRIVATE`.

### 2.4 shmdt — desadjuntar

```c
int shmdt(const void *shmaddr);
// Retorna: 0 en éxito, -1 en error
```

Desadjuntar **no destruye** el segmento. Solo desconecta el mapeo del
proceso actual. El segmento persiste hasta que se elimina explícitamente
con `shmctl`.

```c
if (shmdt(data) == -1) {
    perror("shmdt");
}
```

### 2.5 shmctl — control y eliminación

```c
int shmctl(int shmid, int cmd, struct shmid_ds *buf);
```

Comandos principales:

```
┌──────────────┬──────────────────────────────────────────────┐
│ Comando      │ Efecto                                      │
├──────────────┼──────────────────────────────────────────────┤
│ IPC_STAT     │ Obtener información (struct shmid_ds)       │
│ IPC_SET      │ Modificar permisos / uid / gid              │
│ IPC_RMID     │ Marcar para eliminación                     │
│ SHM_LOCK     │ Bloquear en RAM (no swap) — requiere CAP    │
│ SHM_UNLOCK   │ Desbloquear de RAM                          │
└──────────────┴──────────────────────────────────────────────┘
```

```c
// Marcar para eliminación
// El segmento se destruye cuando TODOS los procesos lo desadjunten
if (shmctl(shmid, IPC_RMID, NULL) == -1) {
    perror("shmctl IPC_RMID");
}
```

`IPC_RMID` marca el segmento como "condenado": no se puede adjuntar
con `shmat` desde nuevos procesos, pero los que ya lo tienen adjuntado
siguen usándolo. El kernel lo libera cuando el último `shmdt` ocurre.

### 2.6 Ejemplo completo: productor y consumidor

**Productor** (`writer.c`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_SIZE 4096

typedef struct {
    int ready;           // flag simple de sincronización
    int length;
    char message[SHM_SIZE - 2 * sizeof(int)];
} shm_data_t;

int main(void)
{
    key_t key = ftok("/tmp/shmfile", 'R');
    if (key == -1) { perror("ftok"); exit(1); }

    int shmid = shmget(key, sizeof(shm_data_t), IPC_CREAT | 0666);
    if (shmid == -1) { perror("shmget"); exit(1); }

    shm_data_t *shm = shmat(shmid, NULL, 0);
    if (shm == (void *)-1) { perror("shmat"); exit(1); }

    // Escribir mensaje
    const char *msg = "Hello from shared memory!";
    strncpy(shm->message, msg, sizeof(shm->message) - 1);
    shm->length = strlen(msg);
    shm->ready = 1;  // señalar que los datos están listos

    printf("Writer: message written, waiting for reader to finish...\n");

    // Esperar a que el lector consuma (busy wait — solo para demostración)
    while (shm->ready != 0)
        ;

    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}
```

**Consumidor** (`reader.c`):

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#define SHM_SIZE 4096

typedef struct {
    int ready;
    int length;
    char message[SHM_SIZE - 2 * sizeof(int)];
} shm_data_t;

int main(void)
{
    key_t key = ftok("/tmp/shmfile", 'R');
    if (key == -1) { perror("ftok"); exit(1); }

    int shmid = shmget(key, sizeof(shm_data_t), 0666);
    if (shmid == -1) { perror("shmget"); exit(1); }

    shm_data_t *shm = shmat(shmid, NULL, SHM_RDONLY);
    if (shm == (void *)-1) { perror("shmat"); exit(1); }

    // Esperar datos (busy wait — solo para demostración)
    while (shm->ready != 1)
        ;

    printf("Reader: received [%d bytes]: %s\n", shm->length, shm->message);

    // Señalar que ya leímos (necesitamos escritura para esto)
    // Con SHM_RDONLY no podemos escribir ready=0
    // En producción: usar semáforo externo

    shmdt(shm);
    return 0;
}
```

**Problema del ejemplo**: el busy-wait con `shm->ready` es una race
condition. Sin barreras de memoria (`__sync_synchronize()` o atómicos),
el compilador/CPU puede reordenar las lecturas y escrituras. En
producción, siempre usar semáforos o mutexes.

---

## 3. POSIX: shm_open y mmap

La API POSIX es más moderna, usa file descriptors (integrable con
`poll`/`close`/`fstat`), y sigue la filosofía "todo es un archivo":

```
  System V                          POSIX
  ┌─────────────────┐               ┌─────────────────┐
  │ ftok() → key    │               │ nombre "/xxx"   │
  │ shmget() → id   │               │ shm_open() → fd │
  │ shmat() → ptr   │               │ ftruncate(fd)   │
  │ shmdt()         │               │ mmap(fd) → ptr  │
  │ shmctl(IPC_RMID)│               │ munmap()        │
  └─────────────────┘               │ close(fd)       │
                                    │ shm_unlink()    │
                                    └─────────────────┘
```

### 3.1 shm_open — crear/abrir objeto de memoria compartida

```c
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

int shm_open(const char *name, int oflag, mode_t mode);
// name: debe empezar con '/' y NO contener más '/' (e.g., "/myshm")
// oflag: O_CREAT, O_EXCL, O_RDWR, O_RDONLY
// mode: permisos (solo con O_CREAT)
// Retorna: fd, o -1 en error
```

El objeto se crea en un filesystem virtual (normalmente `/dev/shm` en
Linux). Es visible como un archivo:

```bash
$ ls -la /dev/shm/
-rw-r--r-- 1 user user 4096 Mar 20 10:00 myshm
```

```c
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
if (fd == -1) {
    perror("shm_open");
    exit(1);
}
```

### 3.2 ftruncate — establecer el tamaño

Un objeto `shm_open` nuevo tiene tamaño 0. Debes darle tamaño antes
de mapear:

```c
if (ftruncate(fd, 4096) == -1) {
    perror("ftruncate");
    exit(1);
}
```

Solo el creador hace `ftruncate`. El que se conecta después puede
consultar el tamaño con `fstat`:

```c
struct stat sb;
fstat(fd, &sb);
size_t size = sb.st_size;  // tamaño actual del objeto
```

### 3.3 mmap — mapear en memoria

```c
void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
// addr: NULL = kernel elige
// prot: PROT_READ, PROT_WRITE, PROT_READ | PROT_WRITE
// flags: MAP_SHARED (compartir cambios) o MAP_PRIVATE (copy-on-write)
// Retorna: puntero, o MAP_FAILED en error
```

**MAP_SHARED vs MAP_PRIVATE**:

```
┌──────────────┬────────────────────────────────────────────────┐
│ MAP_SHARED   │ Escrituras visibles para todos los que mapean │
│              │ y se persisten al archivo/objeto subyacente   │
├──────────────┼────────────────────────────────────────────────┤
│ MAP_PRIVATE  │ Copy-on-write: escrituras son privadas al     │
│              │ proceso, no afectan al objeto subyacente      │
└──────────────┴────────────────────────────────────────────────┘
```

Para IPC, siempre usar `MAP_SHARED`.

```c
char *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
if (ptr == MAP_FAILED) {
    perror("mmap");
    exit(1);
}

close(fd);  // el mapeo persiste incluso después de cerrar el fd

sprintf(ptr, "Hello POSIX shared memory!");
```

### 3.4 munmap — desmapear

```c
if (munmap(ptr, 4096) == -1) {
    perror("munmap");
}
```

### 3.5 shm_unlink — eliminar el objeto

```c
if (shm_unlink("/myshm") == -1) {
    perror("shm_unlink");
}
```

Similar a `unlink` para archivos: el nombre desaparece inmediatamente,
pero el objeto se destruye solo cuando todos los mapeos se cierran.

### 3.6 Ejemplo completo POSIX

**Productor**:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHM_NAME "/demo_shm"
#define SHM_SIZE 4096

typedef struct {
    _Atomic int ready;
    int length;
    char message[SHM_SIZE - sizeof(_Atomic int) - sizeof(int)];
} shm_data_t;

int main(void)
{
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (fd == -1) { perror("shm_open"); exit(1); }

    if (ftruncate(fd, sizeof(shm_data_t)) == -1) {
        perror("ftruncate"); exit(1);
    }

    shm_data_t *shm = mmap(NULL, sizeof(shm_data_t),
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);

    const char *msg = "Hello POSIX shared memory!";
    strncpy(shm->message, msg, sizeof(shm->message) - 1);
    shm->length = strlen(msg);
    shm->ready = 1;

    printf("Writer: wrote %d bytes, waiting...\n", shm->length);
    while (shm->ready != 0)
        ;  // busy wait (demostración — usar semáforo en producción)

    munmap(shm, sizeof(shm_data_t));
    shm_unlink(SHM_NAME);
    printf("Writer: done, cleaned up\n");
    return 0;
}
```

**Consumidor**:

```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#define SHM_NAME "/demo_shm"
#define SHM_SIZE 4096

typedef struct {
    _Atomic int ready;
    int length;
    char message[SHM_SIZE - sizeof(_Atomic int) - sizeof(int)];
} shm_data_t;

int main(void)
{
    int fd = shm_open(SHM_NAME, O_RDWR, 0);
    if (fd == -1) { perror("shm_open"); exit(1); }

    struct stat sb;
    fstat(fd, &sb);

    shm_data_t *shm = mmap(NULL, sb.st_size,
                            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm == MAP_FAILED) { perror("mmap"); exit(1); }
    close(fd);

    while (shm->ready != 1)
        ;

    printf("Reader: [%d bytes] %s\n", shm->length, shm->message);
    shm->ready = 0;  // señalar al escritor

    munmap(shm, sb.st_size);
    return 0;
}
```

Compilar con `-lrt` (en algunos sistemas, la biblioteca de tiempo real
contiene `shm_open`):

```bash
gcc -o writer writer.c -lrt
gcc -o reader reader.c -lrt
```

---

## 4. Sincronización: el problema fundamental

La memoria compartida es **raw memory** — no tiene ningún mecanismo de
sincronización incorporado. Sin sincronización, cualquier acceso
concurrente es un data race (comportamiento indefinido en C11).

### El problema del busy-wait

Los ejemplos anteriores usaron busy-wait (`while (flag != 1)`). Problemas:

1. **Desperdicio de CPU**: consume 100% de un core girando
2. **Sin barreras de memoria**: el compilador puede optimizar el bucle
   a leer una sola vez y cachear el valor → bucle infinito
3. **Reordenamiento**: la CPU puede ver `ready=1` antes de que los datos
   estén realmente escritos

### Soluciones de sincronización

```
┌────────────────────────┬───────────┬───────────┬────────────────┐
│ Mecanismo              │ Entre     │ Overhead  │ Complejidad    │
│                        │ procesos  │           │                │
├────────────────────────┼───────────┼───────────┼────────────────┤
│ Semáforos POSIX        │ Sí        │ Bajo      │ Media          │
│ (sem_open)             │           │           │                │
├────────────────────────┼───────────┼───────────┼────────────────┤
│ Semáforos System V     │ Sí        │ Medio     │ Alta           │
│ (semget/semop)         │           │           │                │
├────────────────────────┼───────────┼───────────┼────────────────┤
│ pthread_mutex con      │ Sí        │ Bajo      │ Media          │
│ PTHREAD_PROCESS_SHARED │           │           │                │
├────────────────────────┼───────────┼───────────┼────────────────┤
│ C11 _Atomic            │ Sí        │ Mínimo   │ Baja (simple)  │
│                        │           │           │ Alta (avanzado)│
├────────────────────────┼───────────┼───────────┼────────────────┤
│ futex (Linux)          │ Sí        │ Mínimo   │ Muy alta       │
└────────────────────────┴───────────┴───────────┴────────────────┘
```

### pthread_mutex en memoria compartida

Un mutex puede vivir **dentro** de la región compartida si se configura
con `PTHREAD_PROCESS_SHARED`:

```c
#include <pthread.h>

typedef struct {
    pthread_mutex_t mutex;
    int counter;
    char data[4080];
} shared_t;

void init_shared(shared_t *shm)
{
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    // Opcional: robusto (se puede recuperar si el holder muere)
    pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);

    pthread_mutex_init(&shm->mutex, &attr);
    pthread_mutexattr_destroy(&attr);

    shm->counter = 0;
}

void safe_increment(shared_t *shm)
{
    int ret = pthread_mutex_lock(&shm->mutex);
    if (ret == EOWNERDEAD) {
        // El proceso que tenía el lock murió
        pthread_mutex_consistent(&shm->mutex);
    }
    shm->counter++;
    pthread_mutex_unlock(&shm->mutex);
}
```

**PTHREAD_MUTEX_ROBUST**: si un proceso muere mientras tiene el lock,
el siguiente `pthread_mutex_lock()` retorna `EOWNERDEAD` en lugar de
bloquearse para siempre. El nuevo propietario llama
`pthread_mutex_consistent()` para marcar los datos como consistentes
(después de repararlos si es necesario).

### C11 atomics para flags simples

Para un flag simple de ready/done, `_Atomic` es suficiente:

```c
#include <stdatomic.h>

typedef struct {
    _Atomic int ready;
    _Atomic int sequence;
    char data[4088];
} lockfree_shm_t;

// Productor
void produce(lockfree_shm_t *shm, const char *msg)
{
    // Escribir datos primero
    strncpy(shm->data, msg, sizeof(shm->data) - 1);
    // Barrera: asegurar que data se escribió antes de ready
    atomic_store_explicit(&shm->ready, 1, memory_order_release);
}

// Consumidor
int consume(lockfree_shm_t *shm, char *buf, size_t bufsz)
{
    // Esperar con barrera de adquisición
    while (atomic_load_explicit(&shm->ready, memory_order_acquire) != 1)
        ;  // sigue siendo busy-wait, pero al menos es correcto
    strncpy(buf, shm->data, bufsz - 1);
    return 0;
}
```

`memory_order_release` en el productor garantiza que todas las escrituras
anteriores (el `strncpy` a `data`) son visibles para cualquier thread/proceso
que vea `ready=1` con `memory_order_acquire`.

---

## 5. Patrones de uso

### 5.1 Ring buffer en memoria compartida

Un patrón clásico para streaming de datos entre procesos:

```c
#include <stdatomic.h>
#include <string.h>

#define RING_SIZE (64 * 1024)  // 64 KB de datos

typedef struct {
    _Atomic size_t write_pos;   // solo el productor modifica
    _Atomic size_t read_pos;    // solo el consumidor modifica
    char buffer[RING_SIZE];
} ring_buffer_t;

void ring_init(ring_buffer_t *rb)
{
    atomic_store(&rb->write_pos, 0);
    atomic_store(&rb->read_pos, 0);
}

// Espacio disponible para escribir
size_t ring_write_avail(ring_buffer_t *rb)
{
    size_t w = atomic_load_explicit(&rb->write_pos, memory_order_relaxed);
    size_t r = atomic_load_explicit(&rb->read_pos, memory_order_acquire);
    return RING_SIZE - (w - r);  // funciona con overflow por ser unsigned
}

// Escribir datos (retorna bytes escritos)
size_t ring_write(ring_buffer_t *rb, const void *data, size_t len)
{
    size_t avail = ring_write_avail(rb);
    if (len > avail - 1)  // reservar 1 byte para distinguir lleno de vacío
        len = avail - 1;
    if (len == 0) return 0;

    size_t w = atomic_load_explicit(&rb->write_pos, memory_order_relaxed);
    size_t pos = w % RING_SIZE;

    // Puede necesitar dos copias si envolvemos el buffer
    size_t first = RING_SIZE - pos;
    if (first > len) first = len;
    memcpy(rb->buffer + pos, data, first);
    if (len > first)
        memcpy(rb->buffer, (const char *)data + first, len - first);

    atomic_store_explicit(&rb->write_pos, w + len, memory_order_release);
    return len;
}

// Leer datos (retorna bytes leídos)
size_t ring_read(ring_buffer_t *rb, void *data, size_t len)
{
    size_t r = atomic_load_explicit(&rb->read_pos, memory_order_relaxed);
    size_t w = atomic_load_explicit(&rb->write_pos, memory_order_acquire);
    size_t avail = w - r;
    if (len > avail) len = avail;
    if (len == 0) return 0;

    size_t pos = r % RING_SIZE;
    size_t first = RING_SIZE - pos;
    if (first > len) first = len;
    memcpy(data, rb->buffer + pos, first);
    if (len > first)
        memcpy((char *)data + first, rb->buffer, len - first);

    atomic_store_explicit(&rb->read_pos, r + len, memory_order_release);
    return len;
}
```

Este ring buffer es **lock-free** con un solo productor y un solo
consumidor (SPSC). Para múltiples productores/consumidores se necesitan
mutexes.

### 5.2 Memoria compartida entre padre e hijo con mmap anónimo

No siempre necesitas un nombre. Con `MAP_ANONYMOUS | MAP_SHARED` puedes
compartir memoria entre padre e hijo sin `shm_open`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    // Crear mapeo anónimo compartido
    int *counter = mmap(NULL, sizeof(int),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS,
                        -1, 0);  // fd=-1, offset=0 para anónimo
    if (counter == MAP_FAILED) { perror("mmap"); exit(1); }

    *counter = 0;

    pid_t pid = fork();
    if (pid == 0) {
        // Hijo incrementa
        for (int i = 0; i < 1000; i++)
            (*counter)++;  // ¡race condition sin sincronización!
        _exit(0);
    }

    // Padre incrementa
    for (int i = 0; i < 1000; i++)
        (*counter)++;

    waitpid(pid, NULL, 0);
    printf("Counter: %d (expected 2000)\n", *counter);
    // Probablemente < 2000 por la race condition

    munmap(counter, sizeof(int));
    return 0;
}
```

### 5.3 Mapear un archivo regular como memoria compartida

`mmap` no requiere `shm_open` — funciona con cualquier fd. Esto permite
usar un archivo regular como respaldo persistente:

```c
int fd = open("database.dat", O_RDWR | O_CREAT, 0644);
ftruncate(fd, sizeof(my_data_t));

my_data_t *db = mmap(NULL, sizeof(my_data_t),
                      PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
close(fd);

// Modificar la estructura — los cambios se persisten al archivo
db->record_count++;

// Forzar escritura a disco (opcional — el kernel lo hace periódicamente)
msync(db, sizeof(my_data_t), MS_SYNC);

munmap(db, sizeof(my_data_t));
```

`msync` controla cuándo se escriben los datos a disco:

```
┌──────────┬──────────────────────────────────────────────────┐
│ Flag     │ Comportamiento                                  │
├──────────┼──────────────────────────────────────────────────┤
│ MS_SYNC  │ Bloquea hasta que los datos estén en disco      │
│ MS_ASYNC │ Inicia escritura pero no espera                 │
│ MS_INVALIDATE │ Invalida cachés para otros mapeos          │
└──────────┴──────────────────────────────────────────────────┘
```

---

## 6. Persistencia y limpieza

### System V: persiste hasta reboot o eliminación explícita

Los segmentos System V **no se eliminan** cuando los procesos terminan.
Se quedan en el kernel hasta que alguien ejecute `shmctl(IPC_RMID)` o
el sistema se reinicie. Esto es un problema frecuente:

```bash
# Ver segmentos huérfanos
$ ipcs -m

------ Shared Memory Segments --------
key        shmid      owner      perms      bytes      nattch
0x52010105 32769      user       666        4096       0

# Eliminar manualmente
$ ipcrm -m 32769
```

Siempre limpiar en el programa:

```c
// Patrón: marcar para eliminación tempranamente
int shmid = shmget(key, size, IPC_CREAT | 0666);
void *ptr = shmat(shmid, NULL, 0);

// Marcar para eliminación inmediatamente después de adjuntar
// El segmento se destruirá cuando el último proceso haga shmdt
shmctl(shmid, IPC_RMID, NULL);

// ... usar ptr ...

shmdt(ptr);  // al ser el último, se destruye
```

### POSIX: persiste en /dev/shm hasta shm_unlink

```bash
# Ver objetos POSIX
$ ls -la /dev/shm/

# Eliminar manualmente
$ rm /dev/shm/myshm
```

Patrón de limpieza con `atexit`:

```c
static const char *shm_name = NULL;

void cleanup_shm(void)
{
    if (shm_name) {
        shm_unlink(shm_name);
        shm_name = NULL;
    }
}

int main(void)
{
    shm_name = "/myapp_shm";
    atexit(cleanup_shm);

    int fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    // ...
}
```

### Signal handler para limpieza

`atexit` no se ejecuta si el proceso muere por señal. Para cubrir
ese caso:

```c
static volatile sig_atomic_t should_exit = 0;

void handle_signal(int sig)
{
    should_exit = 1;
}

int main(void)
{
    struct sigaction sa = {.sa_handler = handle_signal};
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // ... setup shm ...

    while (!should_exit) {
        // ... trabajo ...
    }

    // Limpieza ordenada
    munmap(ptr, size);
    shm_unlink(SHM_NAME);
    return 0;
}
```

---

## 7. Herramientas de diagnóstico

### ipcs — listar recursos IPC System V

```bash
$ ipcs -a          # todo: shm, semáforos, colas de mensajes
$ ipcs -m          # solo memoria compartida
$ ipcs -m -p       # incluir PIDs de creador/último adjuntor
$ ipcs -m -t       # incluir timestamps
$ ipcs -m -l       # mostrar límites del sistema
```

Salida típica:

```
------ Shared Memory Segments --------
key        shmid      owner  perms  bytes   nattch  status
0x52010105 32769      user   666    4096    2
0x00000000 65538      user   600    65536   1       dest
```

`dest` en status significa marcado para eliminación (IPC_RMID).
`nattch` = 0 sin `dest` = **segmento huérfano** (posible fuga).

### ipcrm — eliminar recursos

```bash
$ ipcrm -m <shmid>    # por ID
$ ipcrm -M <key>      # por clave
```

### /proc/sysvipc/shm

Información detallada del kernel:

```bash
$ cat /proc/sysvipc/shm
       key      shmid perms       size  cpid  lpid nattch   ...
  82010105      32769   666       4096  1234  5678      2   ...
```

### Para POSIX

```bash
$ ls -la /dev/shm/              # listar objetos
$ stat /dev/shm/myshm           # detalles de un objeto
$ cat /proc/<pid>/maps | grep shm  # ver mapeos de un proceso
```

### Límites del sistema

```bash
# System V
$ cat /proc/sys/kernel/shmmax   # tamaño máximo de un segmento
$ cat /proc/sys/kernel/shmall   # total de páginas compartidas
$ cat /proc/sys/kernel/shmmni   # número máximo de segmentos

# POSIX (/dev/shm es un tmpfs)
$ df -h /dev/shm                # espacio disponible (típicamente 50% RAM)
$ mount | grep shm              # opciones de montaje
```

---

## 8. Comparación System V vs POSIX

```
┌─────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto             │ System V             │ POSIX                │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Identificación      │ key_t (ftok)         │ nombre "/xxx"        │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ API                 │ shmget/shmat/shmdt   │ shm_open/mmap/munmap │
│                     │ /shmctl              │ /shm_unlink          │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Handle              │ shmid (int)          │ fd (int)             │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Integración         │ Ninguna              │ poll/select/close/   │
│ con fd              │                      │ fstat/fchmod         │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Tamaño dinámico     │ No (fijo al crear)   │ Sí (ftruncate)       │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Namespace           │ Global (kernel)      │ /dev/shm (filesystem)│
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Visibilidad         │ ipcs -m              │ ls /dev/shm/         │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Limpieza            │ ipcrm -m / shmctl    │ rm / shm_unlink      │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Persistencia        │ Kernel (hasta reboot)│ tmpfs (hasta reboot) │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Respaldo a disco    │ No                   │ No (tmpfs=RAM)       │
│ (persistencia real) │                      │ Sí (archivo regular  │
│                     │                      │     con mmap)        │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Portabilidad        │ Unix, Linux, macOS   │ POSIX (Linux, macOS, │
│                     │ Solaris, AIX, HP-UX  │ FreeBSD, Solaris)    │
├─────────────────────┼──────────────────────┼──────────────────────┤
│ Recomendación       │ Solo código legacy   │ Preferir para nuevo  │
│                     │                      │ desarrollo           │
└─────────────────────┴──────────────────────┴──────────────────────┘
```

**Regla general**: usar POSIX para código nuevo. System V solo cuando
se necesite interoperar con software existente que lo use o en sistemas
que no soporten la API POSIX.

---

## 9. Errores comunes

### Error 1: Olvidar sincronización

```c
// MAL: data race — comportamiento indefinido
shm->counter++;  // dos procesos hacen esto concurrentemente
// Resultado: valores perdidos, datos corruptos

// BIEN: usar atómicos o mutex
atomic_fetch_add(&shm->counter, 1);  // correcto con _Atomic

// O con mutex:
pthread_mutex_lock(&shm->mutex);
shm->counter++;
pthread_mutex_unlock(&shm->mutex);
```

**Por qué importa**: un `shm->counter++` se compila a load + add + store
(tres instrucciones). Si dos procesos lo hacen simultáneamente, uno puede
pisar la escritura del otro. Con 1000 incrementos desde dos procesos,
el resultado podría ser 1001 en vez de 2000.

### Error 2: No eliminar segmentos System V

```c
// MAL: el segmento persiste para siempre
int shmid = shmget(key, 4096, IPC_CREAT | 0666);
void *p = shmat(shmid, NULL, 0);
// ... usar ...
shmdt(p);
// Fin del programa — segmento huérfano en el kernel

// BIEN: marcar para eliminación
shmctl(shmid, IPC_RMID, NULL);
shmdt(p);
```

**Diagnóstico**: ejecutar `ipcs -m` periódicamente para buscar segmentos
con `nattch = 0` que no estén marcados como `dest`. Cada uno es una fuga.

### Error 3: Usar MAP_PRIVATE para IPC

```c
// MAL: cada proceso ve su propia copia
void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
               MAP_PRIVATE, fd, 0);  // ← ¡MAP_PRIVATE!
// Escrituras de un proceso NO son visibles para el otro

// BIEN: MAP_SHARED para compartir
void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
               MAP_SHARED, fd, 0);
```

`MAP_PRIVATE` crea una copia copy-on-write. Es útil para cargar
bibliotecas compartidas (cada proceso puede modificar su copia), pero
inútil para IPC.

### Error 4: Almacenar punteros en memoria compartida

```c
// MAL: los punteros son direcciones virtuales del proceso actual
typedef struct {
    char *name;     // ← ¡puntero! Inválido en otro proceso
    int value;
} bad_entry_t;

bad_entry_t *shm = /* mmap... */;
shm->name = malloc(64);  // dirección del heap de ESTE proceso
strcpy(shm->name, "test");
// Otro proceso lee shm->name → SIGSEGV (dirección no mapeada)

// BIEN: almacenar datos inline o usar offsets
typedef struct {
    char name[64];   // datos inline, no puntero
    int value;
} good_entry_t;

// O usar offsets relativos al inicio de la región compartida:
typedef struct {
    size_t name_offset;  // offset desde el inicio del mapeo
    int value;
} offset_entry_t;
```

**Regla**: nunca almacenar punteros absolutos en memoria compartida.
Cada proceso mapea la región en una dirección virtual diferente. Usar
datos inline, offsets relativos, o índices.

### Error 5: No llamar ftruncate antes de mmap

```c
// MAL: shm_open crea con tamaño 0
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
// Olvidar ftruncate
void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
               MAP_SHARED, fd, 0);
// mmap "funciona" pero el objeto tiene 0 bytes
// Acceder a p → SIGBUS (acceso fuera del archivo subyacente)

// BIEN:
int fd = shm_open("/myshm", O_CREAT | O_RDWR, 0666);
ftruncate(fd, 4096);  // ← dar tamaño primero
void *p = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
               MAP_SHARED, fd, 0);
// Ahora el acceso funciona correctamente
```

**SIGBUS** es la señal que el kernel envía cuando accedes a una región
mapeada que no tiene respaldo (el archivo/objeto es más pequeño que el
mapeo). Es un error particularmente confuso porque `mmap` no falla.

---

## 10. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                    MEMORIA COMPARTIDA                              │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  System V:                                                          │
│    key = ftok(path, id)                                             │
│    shmid = shmget(key, size, IPC_CREAT|0666)                       │
│    ptr = shmat(shmid, NULL, 0)        // adjuntar                  │
│    shmdt(ptr)                          // desadjuntar              │
│    shmctl(shmid, IPC_RMID, NULL)      // marcar eliminación       │
│                                                                     │
│  POSIX:                                                             │
│    fd = shm_open("/name", O_CREAT|O_RDWR, 0666)                   │
│    ftruncate(fd, size)                 // ¡obligatorio!            │
│    ptr = mmap(NULL, size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0)│
│    close(fd)                           // mapeo persiste           │
│    munmap(ptr, size)                   // desmapear               │
│    shm_unlink("/name")                // eliminar objeto          │
│                                                                     │
│  mmap anónimo (padre↔hijo):                                        │
│    ptr = mmap(NULL, size, PROT_READ|PROT_WRITE,                    │
│               MAP_SHARED|MAP_ANONYMOUS, -1, 0)                     │
│                                                                     │
│  Sincronización (obligatoria):                                      │
│    - C11 _Atomic: flags simples, contadores                        │
│    - pthread_mutex + PTHREAD_PROCESS_SHARED: secciones críticas    │
│    - Semáforos POSIX/SysV: señalización entre procesos             │
│                                                                     │
│  Reglas de oro:                                                     │
│    1. NUNCA acceder sin sincronización → data race               │
│    2. NUNCA almacenar punteros → usar datos inline u offsets      │
│    3. SIEMPRE ftruncate antes de mmap con shm_open                │
│    4. SIEMPRE limpiar: shmctl(IPC_RMID) / shm_unlink             │
│    5. Compilar con -lrt (si shm_open no enlaza)                    │
│                                                                     │
│  Diagnóstico:                                                       │
│    ipcs -m / ipcrm -m <id>       (System V)                       │
│    ls /dev/shm/ / rm /dev/shm/x  (POSIX)                          │
│    cat /proc/<pid>/maps          (ver mapeos)                      │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Contador compartido con sincronización

Crea dos programas: un `incrementer` y un `reader`. El incrementer
crea una región de memoria compartida POSIX con un contador protegido
por un `pthread_mutex` con `PTHREAD_PROCESS_SHARED` y
`PTHREAD_MUTEX_ROBUST`. Lanza N instancias del incrementer (cada una
incrementa M veces) y verifica con el reader que el resultado final
es exactamente N*M:

```c
// Estructura en la región compartida
typedef struct {
    pthread_mutex_t mutex;
    _Atomic int initialized;
    long counter;
    int n_processes;
    _Atomic int finished;
} shared_counter_t;
```

Verificar:
- El resultado es correcto con 4 procesos y 100000 incrementos cada uno
- Si un incrementer muere (kill -9), los demás no se bloquean (robusto)
- La limpieza de `shm_unlink` ocurre al final

**Pregunta de reflexión**: Si reemplazas `pthread_mutex` por
`_Atomic long counter` con `atomic_fetch_add`, ¿el resultado sigue
siendo correcto? ¿En qué escenarios un mutex es necesario y un atómico
no basta?

### Ejercicio 2: Ring buffer IPC

Implementa un productor y un consumidor que se comuniquen a través de
un ring buffer lock-free (SPSC) en memoria compartida POSIX. El productor
lee líneas de stdin y las escribe al ring buffer. El consumidor lee del
ring buffer y escribe a stdout:

Requisitos:
- El ring buffer debe manejar wrap-around correctamente
- Usar `memory_order_acquire/release` para las posiciones
- No usar busy-wait puro: si no hay datos, el consumidor debe hacer
  `usleep(100)` antes de reintentar
- Medir el throughput: ¿cuántos MB/s puede transferir?
- Comparar con la misma comunicación usando un pipe anónimo

**Pregunta de reflexión**: El ring buffer SPSC lock-free funciona sin
mutex porque solo hay un lector y un escritor. ¿Qué se rompe exactamente
si agregas un segundo productor? ¿Basta con hacer `write_pos` atómico
o se necesita más?

### Ejercicio 3: Base de datos en memoria mapeada

Implementa una mini base de datos de registros fijos mapeada a un
archivo regular con `mmap`:

```c
#define MAX_RECORDS 1000
#define NAME_LEN 64

typedef struct {
    int id;
    char name[NAME_LEN];
    double value;
    int active;  // 0 = deleted
} record_t;

typedef struct {
    int magic;           // 0xDEADBEEF para validar formato
    int version;
    _Atomic int count;   // registros activos
    int next_id;
    record_t records[MAX_RECORDS];
} database_t;
```

Operaciones: `db_create`, `db_open`, `db_insert`, `db_find_by_id`,
`db_delete`, `db_list`, `db_close`. Usar `msync(MS_SYNC)` después de
cada escritura para persistir. Verificar que después de matar el proceso
y reiniciar, los datos persisten.

**Pregunta de reflexión**: ¿Qué pasa si el proceso muere a mitad de un
`db_insert` (después de escribir `name` pero antes de escribir `active=1`)?
¿Cómo podrías hacer las operaciones "atómicas" respecto a crashes?
(Pista: write-ahead log, double-buffering, o un campo de checksum.)
