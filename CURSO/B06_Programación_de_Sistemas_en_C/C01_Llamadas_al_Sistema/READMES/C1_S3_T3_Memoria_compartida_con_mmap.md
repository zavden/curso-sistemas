# Memoria Compartida con mmap — shm_open y Archivos

## Índice

1. [Memoria compartida entre procesos no relacionados](#1-memoria-compartida-entre-procesos-no-relacionados)
2. [shm_open: objetos de memoria compartida POSIX](#2-shm_open-objetos-de-memoria-compartida-posix)
3. [Flujo completo: shm_open + mmap](#3-flujo-completo-shm_open--mmap)
4. [shm_unlink y ciclo de vida](#4-shm_unlink-y-ciclo-de-vida)
5. [mmap con archivo regular como IPC](#5-mmap-con-archivo-regular-como-ipc)
6. [Sincronización en memoria compartida](#6-sincronización-en-memoria-compartida)
7. [System V shared memory vs POSIX](#7-system-v-shared-memory-vs-posix)
8. [Patrones prácticos](#8-patrones-prácticos)
9. [Seguridad y permisos](#9-seguridad-y-permisos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Memoria compartida entre procesos no relacionados

En el tópico anterior vimos que `MAP_SHARED | MAP_ANONYMOUS` solo
funciona entre procesos con relación padre-hijo (post-fork). Para
procesos **independientes** (lanzados por separado), necesitamos un
**nombre** que ambos procesos puedan referenciar:

```
  MAP_SHARED anónimo:            shm_open + mmap:
  ┌────────┐    fork    ┌────────┐
  │ Padre  │───────────►│ Hijo   │     ┌────────┐  ┌────────┐
  │ mmap() │ herencia   │ mmap() │     │ Proc A │  │ Proc B │
  └────┬───┘            └────┬───┘     └────┬───┘  └────┬───┘
       │                     │              │           │
       └─────────┬───────────┘              │           │
                 ▼                          ▼           ▼
          ┌────────────┐             shm_open("/datos")
          │ Páginas    │                    │
          │ anónimas   │                    ▼
          └────────────┘             ┌────────────┐
                                     │ /dev/shm/  │
                                     │ datos      │
                                     └──────┬─────┘
                                            │
                                            ▼
                                     ┌────────────┐
                                     │ Páginas    │
                                     │ compartidas│
                                     └────────────┘
```

Dos mecanismos permiten esto:
1. **shm_open + mmap**: objetos POSIX de memoria compartida (recomendado).
2. **mmap sobre un archivo regular**: usa el filesystem como punto de encuentro.

---

## 2. shm_open: objetos de memoria compartida POSIX

```c
#include <sys/mman.h>
#include <fcntl.h>

int shm_open(const char *name, int oflag, mode_t mode);
int shm_unlink(const char *name);
```

`shm_open` crea o abre un **objeto de memoria compartida** — un pseudo-archivo
que vive en un filesystem en RAM (`tmpfs`), no en disco:

```
  shm_open("/mi_segmento", O_CREAT | O_RDWR, 0644)
      │
      ▼
  Crea /dev/shm/mi_segmento
  (archivo en tmpfs — solo en RAM, nunca toca disco)
      │
      ▼
  Retorna un fd normal → usarlo con ftruncate + mmap
```

### Parámetros

```
  Parámetro    Descripción
  ──────────────────────────────────────────────────────────
  name         nombre con "/" inicial: "/mi_segmento"
               NO subdirectorios: "/sub/dir" es inválido
  oflag        O_CREAT | O_RDWR (crear/abrir para leer+escribir)
               O_RDONLY (solo lectura)
               O_EXCL (fallar si ya existe)
               O_TRUNC (truncar a tamaño 0)
  mode         permisos (como open): 0644, 0600, etc.
               solo aplica con O_CREAT
```

### ¿Dónde vive el objeto?

En Linux, `shm_open("/nombre")` crea el archivo `/dev/shm/nombre`:

```bash
$ ls -la /dev/shm/
total 0
drwxrwxrwt.  2 root root    60 mar 19 10:00 .
drwxr-xr-x. 20 root root  4320 mar 19 08:00 ..
-rw-r--r--.  1 user user  4096 mar 19 10:00 mi_segmento
```

`/dev/shm` es un filesystem `tmpfs` montado en RAM. Los objetos:
- **No persisten** tras un reboot.
- Son **rápidos** (no hay I/O a disco).
- Consumen **RAM** (cuentan contra la memoria del sistema).
- Son visibles con `ls /dev/shm/`.

### Compilación

`shm_open` y `shm_unlink` están en la biblioteca `librt`:

```bash
gcc program.c -o program -lrt
```

En sistemas modernos con glibc reciente, `-lrt` puede no ser
necesario (las funciones están en libc directamente), pero incluirlo
asegura portabilidad.

---

## 3. Flujo completo: shm_open + mmap

### Proceso escritor (crea y escribe)

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHM_NAME "/demo_shm"
#define SHM_SIZE 4096

typedef struct {
    int counter;
    char message[256];
} shared_data_t;

int main(void) {
    // 1. Crear objeto de memoria compartida
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0644);
    if (fd == -1) {
        perror("shm_open");
        return 1;
    }

    // 2. Establecer tamaño (inicialmente es 0)
    if (ftruncate(fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        close(fd);
        shm_unlink(SHM_NAME);
        return 1;
    }

    // 3. Mapear a memoria
    shared_data_t *data = mmap(NULL, SHM_SIZE,
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        shm_unlink(SHM_NAME);
        return 1;
    }

    // 4. fd ya no es necesario
    close(fd);

    // 5. Escribir datos
    data->counter = 42;
    snprintf(data->message, sizeof(data->message),
             "Escrito por PID %d", getpid());

    printf("Datos escritos. Ejecute el lector.\n");
    printf("Presione Enter para limpiar...\n");
    getchar();

    // 6. Limpiar
    munmap(data, SHM_SIZE);
    shm_unlink(SHM_NAME);  // eliminar el objeto
    return 0;
}
```

### Proceso lector (abre y lee)

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#define SHM_NAME "/demo_shm"
#define SHM_SIZE 4096

typedef struct {
    int counter;
    char message[256];
} shared_data_t;

int main(void) {
    // 1. Abrir objeto existente (sin O_CREAT)
    int fd = shm_open(SHM_NAME, O_RDONLY, 0);
    if (fd == -1) {
        perror("shm_open (¿el escritor está corriendo?)");
        return 1;
    }

    // 2. Mapear solo lectura
    shared_data_t *data = mmap(NULL, SHM_SIZE,
                                PROT_READ,
                                MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }
    close(fd);

    // 3. Leer datos
    printf("Counter: %d\n", data->counter);
    printf("Message: %s\n", data->message);

    // 4. Limpiar (no hacer shm_unlink — no somos el dueño)
    munmap(data, SHM_SIZE);
    return 0;
}
```

### Diagrama del flujo

```
  Escritor                           Lector
  ────────────────────────           ────────────────────────
  shm_open(O_CREAT|O_RDWR)
    → crea /dev/shm/demo_shm
  ftruncate(fd, 4096)
    → establece tamaño
  mmap(MAP_SHARED)
    → mapea en su espacio
  close(fd)

  data->counter = 42                 shm_open(O_RDONLY)
  data->message = "..."                → abre /dev/shm/demo_shm
                                     mmap(PROT_READ, MAP_SHARED)
                                       → mapea en su espacio
                                     close(fd)

                                     lee data->counter  → 42
                                     lee data->message  → "..."

                                     munmap()
  munmap()
  shm_unlink()
    → elimina /dev/shm/demo_shm
```

---

## 4. shm_unlink y ciclo de vida

### Cuándo existe el objeto

```
  shm_open(O_CREAT)     El objeto existe en /dev/shm/
       │
       ├── Otros procesos pueden abrirlo con shm_open()
       │
  shm_unlink()          El nombre se elimina de /dev/shm/
       │                PERO: los mapeos existentes siguen válidos
       │
  último munmap()       La memoria se libera realmente
  o último proceso
  que lo tenía termina
```

`shm_unlink` funciona como `unlink` en archivos:
- Elimina el **nombre** del filesystem.
- Los procesos que ya tienen el objeto mapeado **siguen usándolo**.
- La memoria se libera cuando el último proceso desmapea o termina.

### Patrón: crear, mapear, unlink inmediato

```c
int fd = shm_open("/temp_data", O_CREAT | O_RDWR, 0600);
ftruncate(fd, size);
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
close(fd);

// Eliminar el nombre inmediatamente
shm_unlink("/temp_data");
// El mapeo sigue válido, pero nadie más puede abrirlo
// Se limpia automáticamente cuando este proceso termina

// fork() aquí: el hijo hereda el mapeo
```

Este patrón evita dejar objetos huérfanos en `/dev/shm` si el
programa falla. Solo tiene sentido para IPC padre-hijo donde los
procesos heredan el mapeo por fork.

### Verificar objetos huérfanos

```bash
# Ver qué hay en memoria compartida:
$ ls -la /dev/shm/
-rw-r--r--. 1 user user 4096 mar 19 10:00 demo_shm

# Eliminar manualmente un objeto huérfano:
$ rm /dev/shm/demo_shm

# O equivalente desde código:
shm_unlink("/demo_shm");
```

---

## 5. mmap con archivo regular como IPC

Puedes usar un archivo regular en disco como punto de encuentro para
compartir memoria entre procesos:

```c
// Proceso A:
int fd = open("/tmp/shared_data.bin", O_RDWR | O_CREAT, 0644);
ftruncate(fd, 4096);
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
close(fd);

// Proceso B:
int fd = open("/tmp/shared_data.bin", O_RDWR);
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
close(fd);

// Ambos ven los mismos datos a través del page cache
```

### ¿Cómo funciona?

Cuando ambos procesos mapean el mismo archivo con `MAP_SHARED`, el
kernel usa las **mismas páginas físicas** del page cache:

```
  Proceso A              Page cache              Proceso B
  ┌─────────┐           ┌──────────┐           ┌─────────┐
  │  espacio │           │ páginas  │           │  espacio │
  │  virtual │──────────►│ del      │◄──────────│  virtual │
  │  ptr_a   │  mismas   │ archivo  │  mismas   │  ptr_b   │
  └─────────┘  páginas   └─────┬────┘  páginas  └─────────┘
                               │
                               ▼ (writeback periódico)
                         ┌──────────┐
                         │ Archivo  │
                         │ en disco │
                         └──────────┘
```

Las escrituras de un proceso son visibles inmediatamente para el otro
(a través del page cache), **sin esperar** a que se escriban a disco.

### shm_open vs archivo regular

```
  Aspecto                shm_open (/dev/shm)    Archivo regular
  ──────────────────────────────────────────────────────────────
  Almacenamiento         tmpfs (solo RAM)        disco + page cache
  Persistencia           hasta reboot            persiste en disco
  Rendimiento            rápido (sin I/O)        similar (page cache)
  Visibilidad            /dev/shm/nombre         ruta en filesystem
  Subdirectorios         no permitidos           sí
  Portabilidad           POSIX                   más universal
  Tamaño máximo          limitado por RAM+swap   limitado por disco
  Tras reinicio          desaparece              datos persisten
```

### ¿Cuándo usar archivo regular?

```
  Escenario                                  Mecanismo
  ──────────────────────────────────────────────────────────
  IPC temporal, alta velocidad               shm_open
  Datos que sobreviven reboot                archivo regular
  Múltiples máquinas (NFS)                   archivo regular
  Sin acceso a /dev/shm (contenedores)       archivo regular
  Base de datos mapeada (SQLite WAL)         archivo regular
```

---

## 6. Sincronización en memoria compartida

La memoria compartida **no tiene sincronización integrada**. Sin
coordinación, las lecturas y escrituras concurrentes producen race
conditions. Opciones de sincronización:

### Opción 1: mutex en memoria compartida (pthread)

```c
#include <pthread.h>
#include <sys/mman.h>

typedef struct {
    pthread_mutex_t mutex;
    int data;
} shared_t;

// CREADOR: inicializar mutex para compartir entre procesos
shared_t *shared = mmap(NULL, sizeof(shared_t),
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);

pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);

pthread_mutex_init(&shared->mutex, &attr);
pthread_mutexattr_destroy(&attr);

// USO (en cualquier proceso):
pthread_mutex_lock(&shared->mutex);
shared->data++;
pthread_mutex_unlock(&shared->mutex);
```

> **Clave**: `PTHREAD_PROCESS_SHARED` es obligatorio. Sin esto, el mutex
> solo funciona entre hilos del mismo proceso.

### Opción 2: semáforos POSIX con nombre

```c
#include <semaphore.h>
#include <fcntl.h>

// Crear semáforo con nombre (accesible entre procesos)
sem_t *sem = sem_open("/my_sem", O_CREAT, 0644, 1);

// Usar como mutex:
sem_wait(sem);   // lock
// ... acceder a memoria compartida ...
sem_post(sem);   // unlock

// Limpiar:
sem_close(sem);
sem_unlink("/my_sem");  // eliminar nombre
```

Los semáforos con nombre viven en `/dev/shm/sem.my_sem`.

### Opción 3: operaciones atómicas de C11

```c
#include <stdatomic.h>

typedef struct {
    _Atomic int counter;
    _Atomic int flag;
} shared_atomic_t;

// Escritor:
atomic_store(&shared->counter, 42);
atomic_store_explicit(&shared->flag, 1, memory_order_release);

// Lector:
while (atomic_load_explicit(&shared->flag,
                             memory_order_acquire) == 0) {
    // spin-wait (ineficiente pero simple)
}
int value = atomic_load(&shared->counter);
```

### Comparación de mecanismos de sincronización

```
  Mecanismo              Ventaja              Desventaja
  ──────────────────────────────────────────────────────────
  pthread_mutex          familiar, robusto    necesita
  (PROCESS_SHARED)       (robust mutexes)     PROCESS_SHARED

  sem_open               funciona entre       overhead de
  (semáforo con nombre)  procesos distintos   nombre en /dev/shm

  Atómicos C11           sin syscalls,        solo para datos
                         ultra rápido         simples, spin-wait

  flock en archivo       simple               granularidad gruesa,
                                              no para alta frecuencia
```

### Robust mutexes: recuperación ante muerte del dueño

Si un proceso muere mientras tiene un mutex bloqueado, los demás
procesos se quedarían esperando indefinidamente. Los **robust mutexes**
detectan esta situación:

```c
pthread_mutexattr_t attr;
pthread_mutexattr_init(&attr);
pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
pthread_mutexattr_setrobust(&attr, PTHREAD_MUTEX_ROBUST);

pthread_mutex_init(&shared->mutex, &attr);

// Cuando el dueño muere, el siguiente lock retorna EOWNERDEAD:
int ret = pthread_mutex_lock(&shared->mutex);
if (ret == EOWNERDEAD) {
    // El dueño anterior murió con el mutex bloqueado
    // Los datos pueden estar inconsistentes — reparar si es posible
    pthread_mutex_consistent(&shared->mutex);
    // Ahora el mutex es usable nuevamente
}
```

---

## 7. System V shared memory vs POSIX

Linux tiene dos APIs de memoria compartida. La API POSIX (`shm_open`)
es la recomendada, pero código legacy usa la API System V:

### API System V (legacy)

```c
#include <sys/ipc.h>
#include <sys/shm.h>

// Crear segmento
key_t key = ftok("/tmp/myapp", 'A');
int shmid = shmget(key, 4096, IPC_CREAT | 0644);

// Adjuntar al espacio de direcciones
void *ptr = shmat(shmid, NULL, 0);

// Usar ptr...

// Desadjuntar
shmdt(ptr);

// Eliminar (cuando nadie más lo use)
shmctl(shmid, IPC_RMID, NULL);
```

### Comparación

```
  Aspecto                POSIX (shm_open)       System V (shmget)
  ──────────────────────────────────────────────────────────────
  Identificador          nombre "/string"       key_t numérico
  Crear                  shm_open + ftruncate   shmget
  Mapear                 mmap (estándar)        shmat (propietario)
  Desmapear              munmap                 shmdt
  Eliminar               shm_unlink             shmctl(IPC_RMID)
  Listar                 ls /dev/shm/           ipcs -m
  Filesystem             /dev/shm (visible)     kernel (opaco)
  Redimensionar          ftruncate              no
  Integración con fd     sí (fd normal)         no
  Recomendación          ✓ usar este            legacy
```

### Herramientas para System V

```bash
# Listar segmentos de memoria compartida System V:
ipcs -m

# Eliminar un segmento huérfano:
ipcrm -m <shmid>

# Listar todo (semáforos, colas de mensajes, memoria):
ipcs -a
```

### ¿Cuándo encontrarás System V?

- PostgreSQL (usa System V shared memory para buffers compartidos).
- Software legacy que no se ha migrado.
- Código portable a Unix muy antiguos sin soporte POSIX shm.

Para código nuevo, **siempre usa shm_open**.

---

## 8. Patrones prácticos

### Patrón 1: ring buffer en memoria compartida

```c
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <stdatomic.h>

#define RING_SIZE 1024
#define MSG_SIZE 256

typedef struct {
    _Atomic unsigned int head;  // próxima posición de escritura
    _Atomic unsigned int tail;  // próxima posición de lectura
    char messages[RING_SIZE][MSG_SIZE];
} ring_buffer_t;

ring_buffer_t *ring_create(const char *name) {
    int fd = shm_open(name, O_CREAT | O_RDWR, 0644);
    ftruncate(fd, sizeof(ring_buffer_t));
    ring_buffer_t *ring = mmap(NULL, sizeof(ring_buffer_t),
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, 0);
    close(fd);

    atomic_store(&ring->head, 0);
    atomic_store(&ring->tail, 0);
    return ring;
}

ring_buffer_t *ring_open(const char *name) {
    int fd = shm_open(name, O_RDWR, 0);
    ring_buffer_t *ring = mmap(NULL, sizeof(ring_buffer_t),
                                PROT_READ | PROT_WRITE,
                                MAP_SHARED, fd, 0);
    close(fd);
    return ring;
}

int ring_push(ring_buffer_t *ring, const char *msg) {
    unsigned int head = atomic_load(&ring->head);
    unsigned int tail = atomic_load(&ring->tail);
    unsigned int next = (head + 1) % RING_SIZE;

    if (next == tail) return -1;  // lleno

    strncpy(ring->messages[head], msg, MSG_SIZE - 1);
    ring->messages[head][MSG_SIZE - 1] = '\0';
    atomic_store(&ring->head, next);
    return 0;
}

int ring_pop(ring_buffer_t *ring, char *buf, size_t bufsize) {
    unsigned int head = atomic_load(&ring->head);
    unsigned int tail = atomic_load(&ring->tail);

    if (head == tail) return -1;  // vacío

    strncpy(buf, ring->messages[tail], bufsize - 1);
    buf[bufsize - 1] = '\0';
    atomic_store(&ring->tail, (tail + 1) % RING_SIZE);
    return 0;
}
```

### Patrón 2: scoreboard de estado (múltiples workers)

```c
#define MAX_WORKERS 64

typedef struct {
    pid_t pid;
    int status;        // 0=idle, 1=busy, 2=done
    int requests_handled;
    time_t last_active;
} worker_info_t;

typedef struct {
    int num_workers;
    worker_info_t workers[MAX_WORKERS];
} scoreboard_t;

// Master crea el scoreboard:
scoreboard_t *create_scoreboard(int num_workers) {
    int fd = shm_open("/app_scoreboard", O_CREAT | O_RDWR, 0600);
    ftruncate(fd, sizeof(scoreboard_t));
    scoreboard_t *sb = mmap(NULL, sizeof(scoreboard_t),
                             PROT_READ | PROT_WRITE,
                             MAP_SHARED, fd, 0);
    close(fd);
    sb->num_workers = num_workers;
    return sb;
}

// Worker actualiza su slot:
void worker_update(scoreboard_t *sb, int slot, int status) {
    sb->workers[slot].pid = getpid();
    sb->workers[slot].status = status;
    sb->workers[slot].last_active = time(NULL);
}

// Monitor lee el scoreboard:
void print_status(scoreboard_t *sb) {
    for (int i = 0; i < sb->num_workers; i++) {
        printf("Worker %d: PID=%d status=%d reqs=%d\n",
               i, sb->workers[i].pid,
               sb->workers[i].status,
               sb->workers[i].requests_handled);
    }
}
```

Este patrón lo usan servidores como Apache (mod_status) y PHP-FPM
para exponer el estado de sus workers.

### Patrón 3: archivo mapeado como base de datos simple

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#define MAX_RECORDS 10000

typedef struct {
    int id;
    char name[60];
} record_t;

typedef struct {
    int count;
    record_t records[MAX_RECORDS];
} database_t;

database_t *db_open(const char *path) {
    int fd = open(path, O_RDWR | O_CREAT, 0644);

    struct stat sb;
    fstat(fd, &sb);

    if (sb.st_size < (off_t)sizeof(database_t)) {
        ftruncate(fd, sizeof(database_t));
    }

    database_t *db = mmap(NULL, sizeof(database_t),
                           PROT_READ | PROT_WRITE,
                           MAP_SHARED, fd, 0);
    close(fd);
    return db;
}

int db_insert(database_t *db, int id, const char *name) {
    if (db->count >= MAX_RECORDS) return -1;

    record_t *rec = &db->records[db->count];
    rec->id = id;
    strncpy(rec->name, name, sizeof(rec->name) - 1);
    rec->name[sizeof(rec->name) - 1] = '\0';
    db->count++;
    return 0;
}

record_t *db_find(database_t *db, int id) {
    for (int i = 0; i < db->count; i++) {
        if (db->records[i].id == id) {
            return &db->records[i];
        }
    }
    return NULL;
}

void db_sync(database_t *db) {
    msync(db, sizeof(database_t), MS_SYNC);
}

void db_close(database_t *db) {
    msync(db, sizeof(database_t), MS_SYNC);
    munmap(db, sizeof(database_t));
}
```

> **Nota**: esto es una simplificación didáctica. Una base de datos real
> necesita journaling, locks, índices y manejo de corrupción. Pero
> ilustra cómo mmap puede dar acceso de base de datos a un archivo.

---

## 9. Seguridad y permisos

### Permisos del objeto shm

```c
// Solo el owner puede acceder:
shm_open("/private_data", O_CREAT | O_RDWR, 0600);

// Owner y grupo pueden leer, owner escribe:
shm_open("/team_data", O_CREAT | O_RDWR, 0640);

// Todos leen, owner escribe:
shm_open("/public_data", O_CREAT | O_RDWR, 0644);
```

### Riesgos de seguridad

```
  Riesgo                     Mitigación
  ──────────────────────────────────────────────────────────
  Datos sensibles en         permisos restrictivos (0600)
  /dev/shm visibles

  Proceso malicioso          validar datos leídos de
  corrompe datos             memoria compartida

  Objeto huérfano con        shm_unlink cuando ya no
  datos sensibles            se necesita

  Race condition en          usar O_EXCL para creación
  creación                   exclusiva

  Otro usuario ve el         usar nombre impredecible
  nombre del segmento        o permisos 0600
```

### Creación segura con O_EXCL

```c
// Solo uno de los procesos debe crear el segmento.
// El creador usa O_EXCL:
int fd = shm_open("/my_segment", O_CREAT | O_EXCL | O_RDWR, 0600);
if (fd == -1) {
    if (errno == EEXIST) {
        // Ya existe — abrirlo sin O_CREAT ni O_EXCL
        fd = shm_open("/my_segment", O_RDWR, 0);
    } else {
        perror("shm_open");
        return -1;
    }
}
```

### Limpiar datos sensibles antes de liberar

```c
// Antes de munmap, sobrescribir datos sensibles:
explicit_bzero(shared_ptr, shared_size);
// O:
memset_s(shared_ptr, shared_size, 0, shared_size);

munmap(shared_ptr, shared_size);
shm_unlink("/sensitive_data");
```

`explicit_bzero` garantiza que el compilador no optimice la
limpieza (a diferencia de `memset` que puede ser eliminado si el
compilador detecta que los datos no se leen después).

---

## 10. Errores comunes

### Error 1: olvidar ftruncate después de shm_open con O_CREAT

```c
// MAL: objeto nuevo tiene tamaño 0
int fd = shm_open("/data", O_CREAT | O_RDWR, 0644);
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
// SIGBUS al acceder — el objeto tiene 0 bytes

// BIEN: establecer tamaño antes de mapear
int fd = shm_open("/data", O_CREAT | O_RDWR, 0644);
ftruncate(fd, 4096);  // ahora tiene 4096 bytes
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
```

### Error 2: no limpiar objetos shm huérfanos

```c
// MAL: el programa falla antes de shm_unlink
shm_open("/worker_data", O_CREAT | O_RDWR, 0644);
// ... programa falla aquí ...
// /dev/shm/worker_data queda huérfano consumiendo RAM

// BIEN: registrar cleanup para señales
static const char *g_shm_name = NULL;

void cleanup(int sig) {
    if (g_shm_name) shm_unlink(g_shm_name);
    _exit(128 + sig);
}

int main(void) {
    g_shm_name = "/worker_data";
    signal(SIGINT, cleanup);
    signal(SIGTERM, cleanup);

    int fd = shm_open(g_shm_name, O_CREAT | O_RDWR, 0644);
    // ...
}

// TAMBIÉN: atexit para salida normal
void cleanup_atexit(void) {
    if (g_shm_name) shm_unlink(g_shm_name);
}
// atexit(cleanup_atexit);
```

### Error 3: usar tipos con padding o punteros en memoria compartida

```c
// MAL: punteros en memoria compartida
typedef struct {
    char *name;  // ¡PUNTERO! Inválido en otro proceso
    int value;
} bad_shared_t;
// El puntero apunta al espacio virtual del proceso original
// En otro proceso, esa dirección es basura

// BIEN: almacenar datos inline
typedef struct {
    char name[64];  // datos inline, no punteros
    int value;
} good_shared_t;
```

Los punteros son direcciones virtuales **específicas del proceso**.
En otro proceso, la misma dirección virtual apunta a otra cosa (o a
nada). En memoria compartida, **nunca uses punteros**. Usa:
- Arrays de tamaño fijo en lugar de punteros a strings.
- Offsets relativos al inicio del mapeo en lugar de punteros absolutos.

### Error 4: asumir que la estructura tiene el mismo layout en todos los procesos

```c
// PELIGRO: si el escritor y lector se compilan con distintas
// opciones o versiones del compilador, el padding puede diferir

typedef struct {
    char flag;     // 1 byte
    // 3 bytes de padding (depende del compilador/opciones)
    int value;     // 4 bytes
} shared_t;

// MITIGACIÓN: usar tipos de tamaño fijo y verificar
#include <stdint.h>
#include <assert.h>

typedef struct {
    uint8_t  flag;
    uint8_t  _pad[3];
    int32_t  value;
} shared_t;

// Verificar en tiempo de compilación:
_Static_assert(sizeof(shared_t) == 8,
               "shared_t size mismatch");
_Static_assert(offsetof(shared_t, value) == 4,
               "value offset mismatch");
```

### Error 5: no sincronizar accesos concurrentes

```c
// MAL: leer mientras otro proceso escribe
// Escritor:
shared->count = 10;
strcpy(shared->name, "datos nuevos");
// El lector puede ver count=10 pero name viejo (lectura parcial)

// BIEN: usar mutex o flag atómico
pthread_mutex_lock(&shared->mutex);
shared->count = 10;
strcpy(shared->name, "datos nuevos");
pthread_mutex_unlock(&shared->mutex);
```

Sin sincronización, un lector puede ver un estado **parcialmente
actualizado**: algunos campos nuevos y otros viejos. Esto se llama
una "torn read".

---

## 11. Cheatsheet

```
  POSIX SHARED MEMORY (shm_open)
  ──────────────────────────────────────────────────────────
  shm_open("/name", O_CREAT|O_RDWR, 0644)  crear/abrir
  shm_open("/name", O_RDONLY, 0)            abrir lectura
  ftruncate(fd, size)                       establecer tamaño
  mmap(NULL, size, prot, MAP_SHARED, fd, 0) mapear
  close(fd)                                 cerrar fd (mapeo sigue)
  shm_unlink("/name")                       eliminar nombre
  Compilar: gcc -lrt

  UBICACIÓN EN LINUX
  ──────────────────────────────────────────────────────────
  Objetos shm:   /dev/shm/nombre
  Semáforos:      /dev/shm/sem.nombre
  Tipo fs:        tmpfs (solo RAM)

  ARCHIVO REGULAR COMO IPC
  ──────────────────────────────────────────────────────────
  open(path, O_RDWR|O_CREAT, 0644)
  ftruncate(fd, size)
  mmap(NULL, size, prot, MAP_SHARED, fd, 0)
  Ventaja: persiste en disco, funciona en NFS
  Desventaja: I/O a disco eventual

  SINCRONIZACIÓN
  ──────────────────────────────────────────────────────────
  pthread_mutex + PTHREAD_PROCESS_SHARED
  sem_open("/name", O_CREAT, 0644, 1)
  Atómicos C11: _Atomic, atomic_store, atomic_load
  Robust mutex: PTHREAD_MUTEX_ROBUST + EOWNERDEAD

  SYSTEM V (LEGACY)
  ──────────────────────────────────────────────────────────
  ftok + shmget + shmat + shmdt + shmctl
  ipcs -m (listar)    ipcrm -m shmid (eliminar)
  No usar en código nuevo

  REGLAS DE DISEÑO
  ──────────────────────────────────────────────────────────
  NO usar punteros → usar arrays inline u offsets
  NO asumir layout → usar tipos fijos + _Static_assert
  NO acceder sin sync → mutex, semáforo o atómicos
  SÍ limpiar con shm_unlink al terminar
  SÍ usar O_EXCL para creación exclusiva
  SÍ explicit_bzero datos sensibles antes de unlink

  DECISIÓN RÁPIDA
  ──────────────────────────────────────────────────────────
  IPC padre↔hijo       → MAP_SHARED|MAP_ANONYMOUS (T02)
  IPC procesos distintos → shm_open + mmap
  Datos que persisten    → mmap sobre archivo regular
  Código legacy          → System V (shmget)
```

---

## 12. Ejercicios

### Ejercicio 1: chat entre dos procesos

Implementa dos programas (`sender` y `receiver`) que se comuniquen a
través de un segmento de memoria compartida POSIX:

```c
// Estructura compartida:
#define SHM_NAME "/simple_chat"

typedef struct {
    _Atomic int message_ready;  // 0=vacío, 1=nuevo mensaje
    _Atomic int shutdown;       // 1=terminar
    char message[512];
} chat_t;

// sender.c: lee de stdin, escribe al segmento
// receiver.c: lee del segmento, imprime en stdout
```

Requisitos:
- `sender` crea el segmento; `receiver` lo abre.
- `sender` hace `shm_unlink` al terminar (cuando usuario escribe "exit").
- Usar `_Atomic int` para señalizar mensajes nuevos.
- `receiver` hace polling (spin-wait con `usleep` para no quemar CPU).

**Pregunta de reflexión**: el polling con `usleep` es ineficiente.
¿Qué mecanismo usarías para que el receiver se **bloquee** hasta que
haya un mensaje nuevo, sin consumir CPU? Piensa en semáforos, pipes
como señalización, o `futex`.

---

### Ejercicio 2: tabla de métricas compartida

Escribe un programa que lance N workers como procesos hijo. Cada worker
actualiza sus métricas en una tabla compartida. Un hilo monitor en el
padre imprime las métricas periódicamente:

```c
// Esqueleto:
#define MAX_WORKERS 8
#define SHM_NAME "/metrics"

typedef struct {
    pid_t pid;
    _Atomic int requests;
    _Atomic int errors;
    _Atomic long bytes_processed;
} worker_metrics_t;

typedef struct {
    int num_workers;
    worker_metrics_t workers[MAX_WORKERS];
} metrics_table_t;

// 1. Crear segmento shm con la tabla
// 2. fork N workers
// 3. Cada worker simula trabajo:
//    incrementa requests, errors, bytes_processed
// 4. Padre imprime tabla cada 2 segundos
// 5. Ctrl+C limpia todo
```

Requisitos:
- Usar `shm_open` (no MAP_ANONYMOUS, para practicar).
- Verificar que el segmento aparece en `/dev/shm/`.
- Limpiar con `shm_unlink` al terminar.
- Usar atómicos para las actualizaciones de métricas.

**Pregunta de reflexión**: ¿por qué este ejercicio usa `_Atomic`
en lugar de un mutex? ¿En qué caso `_Atomic` sería insuficiente
y necesitarías un mutex? Piensa en qué operaciones necesitan ser
atómicas **en grupo** (no individualmente).

---

### Ejercicio 3: base de datos persistente con mmap

Implementa una base de datos simple de "usuarios" respaldada por un
archivo regular mapeado con `mmap`. Debe funcionar como un programa
interactivo con comandos:

```
$ ./userdb users.dat
> add 1 "Alice"
Added user 1: Alice
> add 2 "Bob"
Added user 2: Bob
> get 1
User 1: Alice
> list
  1: Alice
  2: Bob
> quit
Data synced to disk.

$ ./userdb users.dat
> list
  1: Alice
  2: Bob
```

```c
// Esqueleto:
#define MAX_USERS 1000

typedef struct {
    int32_t id;
    char name[60];
} user_t;

typedef struct {
    int32_t count;
    int32_t _pad;
    user_t users[MAX_USERS];
} userdb_t;

_Static_assert(sizeof(user_t) == 64, "user_t size check");
```

Requisitos:
- Usar archivo regular (no `shm_open`) para que los datos persistan.
- `ftruncate` al crear, verificar tamaño al abrir existente.
- `msync(MS_SYNC)` antes de salir.
- Comandos: `add <id> <name>`, `get <id>`, `del <id>`, `list`, `quit`.
- Usar `_Static_assert` para verificar tamaño de estructuras.

**Pregunta de reflexión**: si dos instancias del programa abren
`users.dat` simultáneamente, ambas ven los cambios de la otra (vía
page cache). Pero sin locking, ¿qué puede ir mal? Diseña un escenario
específico donde ambas instancias ejecutan `add` al mismo tiempo y
los datos se corrompen. ¿Cómo lo arreglarías usando `flock` o un
mutex en el archivo mapeado?

---
