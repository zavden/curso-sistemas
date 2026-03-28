# Mapeo Anónimo — mmap sin archivo

## Índice

1. [Qué es un mapeo anónimo](#1-qué-es-un-mapeo-anónimo)
2. [MAP_ANONYMOUS: sintaxis y uso](#2-map_anonymous-sintaxis-y-uso)
3. [malloc y mmap: la conexión interna](#3-malloc-y-mmap-la-conexión-interna)
4. [MAP_PRIVATE anónimo: memoria privada](#4-map_private-anónimo-memoria-privada)
5. [MAP_SHARED anónimo: IPC entre padre e hijo](#5-map_shared-anónimo-ipc-entre-padre-e-hijo)
6. [Comparación con otros mecanismos de asignación](#6-comparación-con-otros-mecanismos-de-asignación)
7. [Overcommit y OOM killer](#7-overcommit-y-oom-killer)
8. [Huge pages con mapeo anónimo](#8-huge-pages-con-mapeo-anónimo)
9. [Patrones prácticos](#9-patrones-prácticos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es un mapeo anónimo

En el tópico anterior, `mmap` mapeaba un **archivo** a memoria. Un mapeo
anónimo no tiene archivo asociado — es simplemente una región de memoria
virtual inicializada a ceros que el kernel asigna al proceso:

```
  Mapeo de archivo:
  ┌──────────┐     mmap(fd)     ┌──────────────┐
  │ Archivo  │ ────────────────►│ Memoria      │
  │ en disco │   respaldado     │ del proceso  │
  └──────────┘   por archivo    └──────────────┘

  Mapeo anónimo:
                  mmap(-1)      ┌──────────────┐
  (sin archivo) ───────────────►│ Memoria      │
                  MAP_ANONYMOUS │ del proceso  │
                  inicializada  │ (todo ceros) │
                  a ceros       └──────────────┘
```

### ¿Para qué sirve memoria sin archivo?

```
  Uso                          Quién lo hace
  ──────────────────────────────────────────────────────
  Asignar bloques grandes      malloc (internamente)
  Pool de memoria              asignadores custom
  Stacks de hilos              pthread_create
  Memoria compartida padre↔hijo  fork + MAP_SHARED
  Buffers grandes de I/O       aplicaciones de red
  Arenas de memoria            jemalloc, tcmalloc
```

---

## 2. MAP_ANONYMOUS: sintaxis y uso

```c
#include <sys/mman.h>

void *ptr = mmap(NULL,           // kernel elige la dirección
                 size,            // tamaño en bytes
                 PROT_READ | PROT_WRITE,  // permisos
                 MAP_ANONYMOUS | MAP_PRIVATE,  // anónimo + privado
                 -1,              // fd = -1 (sin archivo)
                 0);              // offset = 0 (ignorado)
```

### Diferencias con mmap de archivo

```
  Parámetro    Con archivo          Anónimo
  ──────────────────────────────────────────────────────
  fd           descriptor válido    -1
  offset       posición en archivo  0 (ignorado)
  flags        MAP_SHARED/PRIVATE   MAP_ANONYMOUS | ...
  Contenido    datos del archivo    ceros
  Persistencia en disco             solo en RAM
```

### Ejemplo básico

```c
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>

int main(void) {
    size_t size = 4096;

    // Asignar una página de memoria anónima
    void *ptr = mmap(NULL, size,
                     PROT_READ | PROT_WRITE,
                     MAP_ANONYMOUS | MAP_PRIVATE,
                     -1, 0);

    if (ptr == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    // La memoria está inicializada a ceros
    // Usarla como cualquier buffer
    strcpy(ptr, "Hola desde mmap anónimo");
    printf("%s\n", (char *)ptr);

    // Liberar
    munmap(ptr, size);
    return 0;
}
```

### Forma antigua: /dev/zero

Antes de `MAP_ANONYMOUS`, se lograba el mismo efecto mapeando `/dev/zero`:

```c
// Forma antigua (POSIX portable, sin MAP_ANONYMOUS):
int fd = open("/dev/zero", O_RDWR);
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE, fd, 0);
close(fd);

// Forma moderna (Linux, BSD, macOS):
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
```

`MAP_ANONYMOUS` es más eficiente (no abre un fd) y es universalmente
soportado en sistemas modernos. Solo necesitas la forma `/dev/zero`
para portabilidad a Unix muy antiguos.

---

## 3. malloc y mmap: la conexión interna

`malloc` no es una syscall — es una función de biblioteca (glibc) que
gestiona la memoria usando dos mecanismos del kernel:

```
  malloc(size)
      │
      ├── size < MMAP_THRESHOLD (128 KiB por defecto)
      │   └── brk() / sbrk()
      │       Extiende el heap del proceso
      │       ┌──────────────────────────────────┐
      │       │  Heap (brk)                      │
      │       │  ┌────┬────┬────┬────┬──────┐    │
      │       │  │ 32B│ 64B│ 16B│free│      │    │
      │       │  └────┴────┴────┴────┴──────┘    │
      │       │  glibc gestiona chunks internos  │
      │       └──────────────────────────────────┘
      │
      └── size >= MMAP_THRESHOLD
          └── mmap(MAP_ANONYMOUS | MAP_PRIVATE)
              Mapeo anónimo independiente
              ┌──────────────────┐
              │  Región mmap     │
              │  (fuera del heap)│
              └──────────────────┘
              free() → munmap() directo
```

### ¿Por qué malloc usa mmap para bloques grandes?

```
  Ventaja                     Explicación
  ──────────────────────────────────────────────────────
  free() devuelve al kernel   munmap libera páginas
                              inmediatamente (brk no puede
                              si hay bloques después)
  Sin fragmentación del heap  cada bloque grande es
                              independiente
  Inicialización a ceros      el kernel garantiza ceros
                              (seguridad: sin data leaks)
```

### Observar el comportamiento con strace

```bash
# Asignación pequeña: usa brk
$ strace -e trace=brk,mmap ./prog_small
brk(0x5555557a3000)  = 0x5555557a3000

# Asignación grande: usa mmap
$ strace -e trace=brk,mmap ./prog_large
mmap(NULL, 1052672, PROT_READ|PROT_WRITE,
     MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f...
```

### MMAP_THRESHOLD

glibc ajusta el umbral dinámicamente (entre 128 KiB y 32 MiB). Puedes
fijarlo manualmente, aunque rara vez es necesario:

```c
#include <malloc.h>

// Forzar que malloc use mmap para bloques >= 64 KiB
mallopt(M_MMAP_THRESHOLD, 64 * 1024);
```

### El mapa de memoria de un proceso

```bash
$ cat /proc/self/maps
# ...
55c5a3400000-55c5a3421000 r--p   ejecutable (.text)
55c5a3600000-55c5a3602000 rw-p   datos (.data, .bss)
55c5a3800000-55c5a3821000 rw-p   [heap] ← brk
7f8a10000000-7f8a10021000 rw-p   mmap anónimo (malloc grande)
7f8a12000000-7f8a12200000 r--p   libc.so mapeado
# ...
7ffc5e800000-7ffc5e821000 rw-p   [stack]
```

Cada `mmap(MAP_ANONYMOUS)` aparece como una región `rw-p` sin nombre
de archivo. Las regiones del heap aparecen etiquetadas como `[heap]`.

---

## 4. MAP_PRIVATE anónimo: memoria privada

`MAP_ANONYMOUS | MAP_PRIVATE` es el caso más común: memoria privada
del proceso, invisible a otros.

### Después de fork

```
  Antes de fork:
  ┌──────────────────────┐
  │  Proceso padre       │
  │  ptr → [datos]       │
  └──────────────────────┘

  Después de fork (copy-on-write):
  ┌──────────────────────┐     ┌──────────────────────┐
  │  Proceso padre       │     │  Proceso hijo        │
  │  ptr → ──────┐       │     │  ptr → ──────┐       │
  └──────────────┼───────┘     └──────────────┼───────┘
                 │                             │
                 └──────────┬──────────────────┘
                            ▼
                 ┌──────────────────┐
                 │  Página física   │  ← compartida, read-only
                 │  (hasta que uno  │
                 │   escriba → CoW) │
                 └──────────────────┘
```

Con `MAP_PRIVATE`, después del `fork` cada proceso tiene su **copia
independiente** (via CoW). Las modificaciones de uno no afectan al otro.

### Uso típico: buffer temporal grande

```c
// Asignar 10 MiB para procesamiento temporal
size_t size = 10 * 1024 * 1024;
void *buf = mmap(NULL, size,
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE,
                 -1, 0);
if (buf == MAP_FAILED) { perror("mmap"); return -1; }

// Usar buf...
process_data(buf, size);

// Liberar — devuelve toda la memoria al kernel inmediatamente
munmap(buf, size);
```

Ventaja sobre `malloc`: `munmap` **garantiza** que la memoria se devuelve
al sistema operativo. Con `free()`, glibc puede retenerla en el heap
para reutilización futura.

---

## 5. MAP_SHARED anónimo: IPC entre padre e hijo

`MAP_ANONYMOUS | MAP_SHARED` crea memoria que **se comparte** entre
procesos relacionados por `fork`. Este es un mecanismo de IPC simple
y eficiente:

```
  Antes de fork:
  padre: ptr = mmap(MAP_ANONYMOUS | MAP_SHARED)

  Después de fork:
  ┌──────────────────────┐     ┌──────────────────────┐
  │  Proceso padre       │     │  Proceso hijo        │
  │  ptr → ──────┐       │     │  ptr → ──────┐       │
  └──────────────┼───────┘     └──────────────┼───────┘
                 │                             │
                 └──────────┬──────────────────┘
                            ▼
                 ┌──────────────────┐
                 │  Página física   │  ← COMPARTIDA
                 │  Escrituras de   │
                 │  uno son visibles│
                 │  para el otro    │
                 └──────────────────┘
```

### Ejemplo: padre e hijo comparten un contador

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void) {
    // Asignar memoria compartida anónima
    int *counter = mmap(NULL, sizeof(int),
                        PROT_READ | PROT_WRITE,
                        MAP_SHARED | MAP_ANONYMOUS,
                        -1, 0);
    if (counter == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    *counter = 0;

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        // Hijo: incrementar 1000 veces
        for (int i = 0; i < 1000; i++) {
            (*counter)++;
        }
        _exit(0);
    }

    // Padre: incrementar 1000 veces
    for (int i = 0; i < 1000; i++) {
        (*counter)++;
    }

    wait(NULL);  // esperar al hijo

    printf("Contador: %d\n", *counter);
    // NOTA: el resultado puede ser < 2000 por race condition
    // (necesita sincronización — ver más adelante)

    munmap(counter, sizeof(int));
    return 0;
}
```

> **Advertencia**: el ejemplo anterior tiene un race condition.
> `(*counter)++` no es atómico — requiere sincronización. Veremos
> cómo resolver esto en el capítulo de hilos con operaciones atómicas
> y mutexes en memoria compartida.

### Ejemplo: compartir estructura entre procesos

```c
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    int ready;          // flag simple de sincronización
    int result;
    char message[256];
} shared_data_t;

int main(void) {
    shared_data_t *shared = mmap(NULL, sizeof(shared_data_t),
                                  PROT_READ | PROT_WRITE,
                                  MAP_SHARED | MAP_ANONYMOUS,
                                  -1, 0);
    if (shared == MAP_FAILED) { perror("mmap"); return 1; }

    shared->ready = 0;

    pid_t pid = fork();
    if (pid == 0) {
        // Hijo: hacer trabajo y reportar resultado
        shared->result = 42;
        snprintf(shared->message, sizeof(shared->message),
                 "Calculado por PID %d", getpid());
        shared->ready = 1;  // señalizar al padre
        _exit(0);
    }

    // Padre: esperar resultado
    wait(NULL);

    if (shared->ready) {
        printf("Resultado: %d\n", shared->result);
        printf("Mensaje: %s\n", shared->message);
    }

    munmap(shared, sizeof(shared_data_t));
    return 0;
}
```

### MAP_SHARED anónimo vs otros mecanismos de IPC

```
  Mecanismo             Ventaja                 Limitación
  ──────────────────────────────────────────────────────────
  MAP_SHARED anónimo    simple, rápido,         solo padre↔hijo
                        sin archivos            (post-fork)
  shm_open + mmap       procesos no             necesita nombre
                        relacionados            y limpieza
  pipe                  flujo de datos          solo secuencial,
                                                copia datos
  socket Unix           bidireccional           overhead de
                                                protocolo
```

La limitación clave: `MAP_SHARED | MAP_ANONYMOUS` solo funciona
entre procesos que comparten el mapeo por herencia de `fork`. Para
procesos no relacionados, necesitas `shm_open()` (siguiente tópico).

---

## 6. Comparación con otros mecanismos de asignación

### mmap anónimo vs malloc

```
  Aspecto               malloc               mmap anónimo
  ──────────────────────────────────────────────────────────
  Granularidad          bytes                páginas (4 KiB)
  Overhead mínimo       ~16-32 bytes/alloc   4096 bytes
  Allocs pequeños       eficiente            desperdicio
  Allocs grandes        usa mmap interno     directo
  free() devuelve       quizá (heap cache)   siempre (munmap)
  Inicialización        indefinida           ceros garantizados
  Thread-safe           sí (con locks)       sí (kernel)
  Fragmentación         posible              no (independiente)
```

### mmap anónimo vs calloc

```c
// calloc: asigna e inicializa a ceros
int *arr = calloc(1000, sizeof(int));

// mmap: también da ceros, pero a nivel de página
int *arr = mmap(NULL, 1000 * sizeof(int),
                PROT_READ | PROT_WRITE,
                MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
```

Diferencia sutil: `calloc` puede ser más eficiente para tamaños
pequeños (reutiliza memoria ya en el heap). Para bloques grandes,
ambos terminan usando `mmap` internamente y el kernel proporciona
páginas ya rellenas de ceros sin costo adicional (la "zero page").

### mmap anónimo vs stack (variables locales)

```
  Aspecto               Stack                mmap anónimo
  ──────────────────────────────────────────────────────────
  Tamaño máximo         ~8 MiB (ulimit -s)   limitado por RAM
  Velocidad             muy rápida            syscall (lenta)
  Cuándo usar           datos temporales      datos grandes o
                        pequeños              que sobreviven
                                              al scope
```

---

## 7. Overcommit y OOM killer

### El problema de overcommit

Linux permite reservar **más memoria virtual** que la RAM+swap física
disponible. Esto funciona porque la mayoría de los procesos no usan
toda la memoria que reservan:

```
  RAM física: 4 GiB
  Swap: 2 GiB
  Total real: 6 GiB

  Proceso A: mmap(2 GiB) → OK   (usa 500 MiB realmente)
  Proceso B: mmap(3 GiB) → OK   (usa 1 GiB realmente)
  Proceso C: mmap(4 GiB) → OK   (usa 200 MiB realmente)
  Total reservado: 9 GiB         (Total usado: 1.7 GiB)
  ─────────────────────────────
  Sin overcommit, C habría fallado.
  Con overcommit, todos arrancan bien.
```

### Cuándo falla: OOM killer

Si los procesos intentan **usar** más memoria de la que realmente existe,
el kernel invoca al **OOM killer** para matar procesos y liberar memoria:

```
  ┌──────────────────────────────────────────────────┐
  │  Proceso C intenta escribir en una página        │
  │  reservada pero sin memoria física disponible    │
  │                     │                            │
  │                     ▼                            │
  │  Kernel: "No hay páginas libres"                 │
  │                     │                            │
  │                     ▼                            │
  │  OOM killer selecciona un proceso para matar     │
  │  (basado en oom_score: memoria usada, prioridad) │
  │                     │                            │
  │                     ▼                            │
  │  SIGKILL al proceso seleccionado                 │
  └──────────────────────────────────────────────────┘
```

### Configuración de overcommit

```bash
# Ver la política actual:
cat /proc/sys/vm/overcommit_memory

# Valores:
# 0 = heurístico (default) — el kernel decide si es "razonable"
# 1 = siempre permitir (nunca rechazar mmap/malloc)
# 2 = no overcommit — solo permite RAM + swap × overcommit_ratio

cat /proc/sys/vm/overcommit_ratio  # default: 50 (%)
# Con modo 2: límite = swap + RAM × 50%
```

### oom_score y protección

```bash
# Ver el score OOM de un proceso:
cat /proc/PID/oom_score

# Proteger un proceso (valor -1000 a 1000):
echo -1000 > /proc/PID/oom_score_adj   # inmune al OOM killer
echo  1000 > /proc/PID/oom_score_adj   # primera víctima
```

### Implicación para mmap

`mmap(MAP_ANONYMOUS)` casi **nunca falla** por falta de memoria (con
overcommit habilitado). El fallo ocurre más tarde, cuando intentas
**escribir** en las páginas:

```c
// Esto CASI SIEMPRE tiene éxito (solo reserva espacio virtual):
void *ptr = mmap(NULL, 100ULL * 1024 * 1024 * 1024,  // 100 GiB
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
// ptr != MAP_FAILED (probablemente)

// El fallo real ocurre aquí, cuando tocas las páginas:
memset(ptr, 0xFF, 100ULL * 1024 * 1024 * 1024);
// → OOM killer mata algún proceso (quizá este)
```

### MAP_POPULATE: forzar asignación inmediata

```c
// Forzar que el kernel asigne páginas físicas ahora (no lazy):
void *ptr = mmap(NULL, size,
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_POPULATE,
                 -1, 0);
```

Con `MAP_POPULATE`, si no hay memoria suficiente, `mmap` falla
inmediatamente en lugar de tener éxito ahora y OOM después. Útil para
aplicaciones que necesitan **garantías** de memoria (bases de datos,
sistemas en tiempo real).

### MAP_NORESERVE: lo opuesto

```c
// No reservar swap para este mapeo:
void *ptr = mmap(NULL, huge_size,
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE,
                 -1, 0);
```

Útil para asignar espacios virtuales muy grandes donde sabes que solo
usarás una fracción pequeña (tablas hash sparse, por ejemplo).

---

## 8. Huge pages con mapeo anónimo

Las páginas normales de x86-64 son de 4 KiB. Para mapeos muy grandes,
el overhead de gestión de miles de páginas pequeñas afecta el rendimiento
(TLB misses). Las **huge pages** (2 MiB o 1 GiB) reducen este overhead:

```
  Páginas normales (4 KiB):     Huge pages (2 MiB):
  ┌──┬──┬──┬──┬──┬──┬──┬──┐    ┌──────────────────┐
  │p1│p2│p3│p4│p5│p6│p7│p8│    │    huge page 1   │
  └──┴──┴──┴──┴──┴──┴──┴──┘    └──────────────────┘
  8 entradas TLB necesarias     1 entrada TLB suficiente
```

### Transparent Huge Pages (THP)

Linux puede usar huge pages automáticamente sin cambios en el código:

```c
// El kernel puede promover a huge page automáticamente:
void *ptr = mmap(NULL, 64 * 1024 * 1024,  // 64 MiB
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);

// Sugerencia explícita:
madvise(ptr, 64 * 1024 * 1024, MADV_HUGEPAGE);
```

### Huge pages explícitas con MAP_HUGETLB

```c
// Requiere huge pages preasignadas en el sistema:
// echo 512 > /proc/sys/vm/nr_hugepages

void *ptr = mmap(NULL, 2 * 1024 * 1024,  // exactamente 2 MiB
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | MAP_HUGETLB,
                 -1, 0);
if (ptr == MAP_FAILED) {
    perror("mmap MAP_HUGETLB");
    // Probablemente no hay huge pages disponibles
}
```

### Cuándo usar huge pages

```
  Caso de uso                  Beneficio
  ──────────────────────────────────────────────────────
  Bases de datos (buffers)     menos TLB misses
  VMs (QEMU/KVM)              acceso a memoria del guest
  Computación científica       arrays enormes
  JVMs con heaps grandes       reducción de overhead GC

  NO usar para:
  Asignaciones < 2 MiB         desperdicio de memoria
  Muchos mapeos pequeños       huge pages son escasas
```

---

## 9. Patrones prácticos

### Patrón 1: pool de objetos con mmap

```c
#include <stdio.h>
#include <sys/mman.h>
#include <stdint.h>

typedef struct {
    void *base;
    size_t obj_size;
    size_t capacity;
    size_t count;
    uint8_t *bitmap;  // 1 = usado, 0 = libre
} pool_t;

int pool_init(pool_t *pool, size_t obj_size, size_t capacity) {
    pool->obj_size = obj_size;
    pool->capacity = capacity;
    pool->count = 0;

    // Asignar memoria para los objetos
    size_t data_size = obj_size * capacity;
    pool->base = mmap(NULL, data_size,
                      PROT_READ | PROT_WRITE,
                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (pool->base == MAP_FAILED) return -1;

    // Bitmap para rastrear qué slots están usados
    size_t bitmap_size = (capacity + 7) / 8;
    pool->bitmap = mmap(NULL, bitmap_size,
                        PROT_READ | PROT_WRITE,
                        MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (pool->bitmap == MAP_FAILED) {
        munmap(pool->base, data_size);
        return -1;
    }

    return 0;
}

void *pool_alloc(pool_t *pool) {
    for (size_t i = 0; i < pool->capacity; i++) {
        if (!(pool->bitmap[i / 8] & (1 << (i % 8)))) {
            pool->bitmap[i / 8] |= (1 << (i % 8));
            pool->count++;
            return (char *)pool->base + i * pool->obj_size;
        }
    }
    return NULL;  // pool lleno
}

void pool_free(pool_t *pool, void *ptr) {
    size_t index = ((char *)ptr - (char *)pool->base) / pool->obj_size;
    pool->bitmap[index / 8] &= ~(1 << (index % 8));
    pool->count--;
}

void pool_destroy(pool_t *pool) {
    munmap(pool->base, pool->obj_size * pool->capacity);
    munmap(pool->bitmap, (pool->capacity + 7) / 8);
}
```

### Patrón 2: stack growable con guard page

```c
#include <sys/mman.h>
#include <signal.h>

// Asignar un "stack" con una guard page al final
// que genera SIGSEGV si se desborda
void *create_guarded_region(size_t usable_size) {
    size_t page = sysconf(_SC_PAGESIZE);
    size_t total = usable_size + page;  // +1 guard page

    // Mapear todo como sin acceso
    void *base = mmap(NULL, total,
                      PROT_NONE,
                      MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (base == MAP_FAILED) return MAP_FAILED;

    // Hacer accesible todo excepto la primera página (guard)
    if (mprotect((char *)base + page, usable_size,
                 PROT_READ | PROT_WRITE) == -1) {
        munmap(base, total);
        return MAP_FAILED;
    }

    // Retornar puntero a la región usable (después del guard)
    return (char *)base + page;
}
```

Este es exactamente el patrón que usa `pthread_create` para los stacks
de hilos: una guard page al límite del stack detecta desbordamientos.

### Patrón 3: tabla hash con reserva virtual grande

```c
// Reservar 1 GiB de espacio virtual pero sin usar RAM
// Solo las páginas tocadas consumen memoria real
#define TABLE_SIZE (1024ULL * 1024 * 1024)  // 1 GiB

typedef struct {
    int key;
    int value;
    int occupied;
} entry_t;

entry_t *hash_table = mmap(NULL, TABLE_SIZE,
                            PROT_READ | PROT_WRITE,
                            MAP_ANONYMOUS | MAP_PRIVATE | MAP_NORESERVE,
                            -1, 0);

// Solo las entradas que realmente usemos consumirán RAM
// El resto permanece como páginas virtuales apuntando a la zero page
```

### Patrón 4: fork + memoria compartida para trabajo paralelo

```c
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

// Dividir un array entre N procesos hijo
void parallel_compute(double *input, double *output,
                      size_t n, int nprocs) {
    // Asignar resultados en memoria compartida
    size_t out_size = n * sizeof(double);
    double *shared_out = mmap(NULL, out_size,
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED | MAP_ANONYMOUS,
                              -1, 0);

    size_t chunk = n / nprocs;

    for (int i = 0; i < nprocs; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Hijo: procesar su porción
            size_t start = i * chunk;
            size_t end = (i == nprocs - 1) ? n : start + chunk;

            for (size_t j = start; j < end; j++) {
                shared_out[j] = input[j] * input[j];  // ejemplo
            }
            _exit(0);
        }
    }

    // Padre: esperar a todos los hijos
    for (int i = 0; i < nprocs; i++) {
        wait(NULL);
    }

    // shared_out contiene los resultados de todos los hijos
    // Copiar al output
    for (size_t i = 0; i < n; i++) {
        output[i] = shared_out[i];
    }

    munmap(shared_out, out_size);
}
```

---

## 10. Errores comunes

### Error 1: usar mmap anónimo para asignaciones pequeñas

```c
// MAL: desperdiciar una página entera para 32 bytes
int *x = mmap(NULL, sizeof(int),
              PROT_READ | PROT_WRITE,
              MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
// Asigna 4096 bytes para guardar 4 bytes (99.9% desperdicio)
// Además, cada mmap es una syscall (~300ns)

// BIEN: usar malloc para asignaciones pequeñas
int *x = malloc(sizeof(int));
// malloc agrupa muchas asignaciones pequeñas en pocas páginas
```

### Error 2: olvidar que MAP_PRIVATE anónimo no comparte con fork

```c
// MAL: esperar que hijo vea los cambios del padre
int *data = mmap(NULL, sizeof(int),
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE,  // ← PRIVATE
                 -1, 0);
*data = 0;

if (fork() == 0) {
    *data = 42;  // modifica COPIA privada del hijo
    _exit(0);
}
wait(NULL);
printf("%d\n", *data);  // imprime 0, NO 42

// BIEN: usar MAP_SHARED para IPC
int *data = mmap(NULL, sizeof(int),
                 PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_SHARED,  // ← SHARED
                 -1, 0);
```

### Error 3: asumir que MAP_SHARED anónimo funciona entre procesos no relacionados

```c
// MAL: dos programas independientes no comparten MAP_ANONYMOUS
// Programa A:
int *shared = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                   MAP_SHARED | MAP_ANONYMOUS, -1, 0);
// Programa B no puede acceder a esta memoria

// BIEN: para procesos no relacionados, usar shm_open:
int shm_fd = shm_open("/my_shared", O_CREAT | O_RDWR, 0644);
ftruncate(shm_fd, 4096);
int *shared = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                   MAP_SHARED, shm_fd, 0);
```

### Error 4: no verificar MAP_FAILED

```c
// MAL: continuar con puntero inválido
void *buf = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
memset(buf, 0, size);  // SIGSEGV si mmap falló

// BIEN:
void *buf = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
if (buf == MAP_FAILED) {
    perror("mmap");
    return -1;
}
```

### Error 5: race condition con MAP_SHARED sin sincronización

```c
// MAL: incremento no atómico en memoria compartida
int *counter = mmap(NULL, sizeof(int), PROT_READ | PROT_WRITE,
                    MAP_SHARED | MAP_ANONYMOUS, -1, 0);

if (fork() == 0) {
    for (int i = 0; i < 100000; i++)
        (*counter)++;  // NO es atómico — race condition
    _exit(0);
}
for (int i = 0; i < 100000; i++)
    (*counter)++;

wait(NULL);
printf("%d\n", *counter);  // casi siempre < 200000

// BIEN: usar operaciones atómicas de C11
#include <stdatomic.h>

_Atomic int *counter = mmap(...);
atomic_fetch_add(counter, 1);  // atómico y correcto
```

---

## 11. Cheatsheet

```
  MAPEO ANÓNIMO BÁSICO
  ──────────────────────────────────────────────────────────
  mmap(NULL, size, PROT_READ|PROT_WRITE,
       MAP_ANONYMOUS|MAP_PRIVATE, -1, 0)
  → memoria privada, ceros, como malloc grande

  mmap(NULL, size, PROT_READ|PROT_WRITE,
       MAP_ANONYMOUS|MAP_SHARED, -1, 0)
  → memoria compartida padre↔hijo (post-fork)

  MALLOC INTERNAMENTE
  ──────────────────────────────────────────────────────────
  malloc(< 128 KiB)  → brk/sbrk (heap)
  malloc(≥ 128 KiB)  → mmap(MAP_ANONYMOUS|MAP_PRIVATE)
  free() de mmap     → munmap (devuelve al kernel)
  free() de brk      → puede retener en heap

  FLAGS ADICIONALES
  ──────────────────────────────────────────────────────────
  MAP_POPULATE        asignar páginas físicas ahora
  MAP_NORESERVE       no reservar swap (tablas sparse)
  MAP_HUGETLB         usar huge pages (2 MiB)
  MAP_LOCKED          no permitir swap (mlock equivalente)

  PRIVATE vs SHARED (ANÓNIMO)
  ──────────────────────────────────────────────────────────
  MAP_PRIVATE         CoW después de fork
                      cada proceso ve su copia
  MAP_SHARED          mismas páginas tras fork
                      cambios visibles mutuamente
                      necesita sincronización

  OVERCOMMIT
  ──────────────────────────────────────────────────────────
  /proc/sys/vm/overcommit_memory
    0 = heurístico (default)
    1 = siempre permitir
    2 = estricto (RAM + swap × ratio)
  OOM killer: SIGKILL al proceso con mayor oom_score
  /proc/PID/oom_score_adj: -1000 (inmune) a 1000

  HUGE PAGES
  ──────────────────────────────────────────────────────────
  THP automático:  madvise(ptr, len, MADV_HUGEPAGE)
  Explícito:       MAP_HUGETLB (requiere preasignación)
  Tamaño:          2 MiB (x86-64), 1 GiB (opciones avanzadas)

  CUÁNDO USAR MMAP ANÓNIMO
  ──────────────────────────────────────────────────────────
  Buffers grandes (> 128 KiB)   → sí
  Asignaciones pequeñas         → no, usar malloc
  IPC padre↔hijo simple         → MAP_SHARED|MAP_ANONYMOUS
  IPC entre procesos distintos  → shm_open + mmap
  Pool de memoria               → sí
  Guard pages                   → PROT_NONE + mprotect
```

---

## 12. Ejercicios

### Ejercicio 1: verificar el comportamiento de malloc

Escribe un programa que haga asignaciones de distintos tamaños con
`malloc` y use `strace` para observar cuándo usa `brk` y cuándo usa
`mmap`:

```c
// Esqueleto:
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Asignación 1 KiB ===\n");
    void *p1 = malloc(1024);

    printf("=== Asignación 64 KiB ===\n");
    void *p2 = malloc(64 * 1024);

    printf("=== Asignación 256 KiB ===\n");
    void *p3 = malloc(256 * 1024);

    printf("=== Asignación 1 MiB ===\n");
    void *p4 = malloc(1024 * 1024);

    // Verificar que los punteros están en regiones distintas
    printf("p1 = %p\n", p1);
    printf("p2 = %p\n", p2);
    printf("p3 = %p\n", p3);
    printf("p4 = %p\n", p4);

    // Liberar
    free(p1); free(p2); free(p3); free(p4);
    return 0;
}
```

Ejecutar con: `strace -e trace=brk,mmap,munmap ./prog`

Requisitos:
- Documentar a partir de qué tamaño `malloc` cambia de `brk` a `mmap`.
- Verificar que `free()` de bloques `mmap` genera `munmap`.
- Revisar `/proc/self/maps` dentro del programa para ver las regiones.

**Pregunta de reflexión**: si `free()` de un bloque grande hace `munmap`
inmediatamente, ¿por qué `free()` de un bloque pequeño NO hace `brk`
para devolver la memoria? ¿Qué limitación del heap lo impide?

---

### Ejercicio 2: IPC padre-hijo con memoria compartida

Implementa un programa donde el padre crea N hijos, cada uno calcula
la suma de un rango de un array, y el padre reúne los resultados usando
`MAP_SHARED | MAP_ANONYMOUS`:

```c
// Esqueleto:
#include <stdio.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

#define ARRAY_SIZE 10000000
#define NUM_WORKERS 4

int main(void) {
    // 1. Asignar array de entrada con mmap (MAP_PRIVATE)
    // 2. Llenar con datos (ej: input[i] = i)
    // 3. Asignar array de resultados parciales (MAP_SHARED)
    //    → un long long por worker
    // 4. fork NUM_WORKERS hijos
    //    Cada hijo suma su porción del array y escribe en results[i]
    // 5. Padre espera a todos los hijos
    // 6. Padre suma los resultados parciales
    // 7. Verificar contra la fórmula: n*(n-1)/2

    return 0;
}
```

Requisitos:
- Cada hijo calcula la suma de `ARRAY_SIZE / NUM_WORKERS` elementos.
- El último hijo toma los elementos restantes.
- Verificar el resultado con la fórmula aritmética.

**Pregunta de reflexión**: ¿por qué el array de entrada puede usar
`MAP_PRIVATE` pero el array de resultados debe usar `MAP_SHARED`?
¿Qué pasaría si ambos fueran `MAP_PRIVATE`?

---

### Ejercicio 3: asignador de memoria simple con mmap

Implementa un asignador de memoria simple que usa `mmap(MAP_ANONYMOUS)`
para obtener bloques y los gestiona internamente:

```c
// Esqueleto:
#include <sys/mman.h>
#include <stddef.h>

#define BLOCK_SIZE (64 * 1024)  // 64 KiB por bloque

typedef struct block_header {
    size_t size;
    int free;
    struct block_header *next;
} block_header_t;

static block_header_t *head = NULL;

void *my_malloc(size_t size) {
    // 1. Buscar un bloque libre que quepa (first-fit)
    // 2. Si no hay, pedir un nuevo bloque al kernel con mmap
    // 3. Retornar puntero después del header
}

void my_free(void *ptr) {
    // 1. Retroceder al header
    // 2. Marcar como libre
    // 3. Opcionalmente: munmap si el bloque completo está libre
}
```

Requisitos:
- Cada llamada a `mmap` pide al menos `BLOCK_SIZE` bytes.
- Usar headers para rastrear bloques asignados/libres.
- Implementar al menos first-fit para reutilización.
- Escribir un programa de prueba que haga alloc/free/alloc.

**Pregunta de reflexión**: los asignadores reales como jemalloc y
tcmalloc usan arenas por hilo y listas segregadas por tamaño. ¿Por
qué un asignador con una sola lista enlazada (como este ejercicio)
tiene mal rendimiento en programas multihilo? ¿Qué es la "false
sharing" y cómo afecta a los headers de bloques contiguos?

---
