# Ventajas de mmap sobre read/write

## Índice

1. [El costo real de read/write](#1-el-costo-real-de-readwrite)
2. [Zero-copy: eliminando la copia intermedia](#2-zero-copy-eliminando-la-copia-intermedia)
3. [Lazy loading y page faults bajo demanda](#3-lazy-loading-y-page-faults-bajo-demanda)
4. [Page cache del kernel: la capa invisible](#4-page-cache-del-kernel-la-capa-invisible)
5. [Acceso aleatorio eficiente](#5-acceso-aleatorio-eficiente)
6. [Archivos grandes: más allá de la RAM](#6-archivos-grandes-más-allá-de-la-ram)
7. [Cuándo read/write es mejor que mmap](#7-cuándo-readwrite-es-mejor-que-mmap)
8. [Benchmarks y mediciones prácticas](#8-benchmarks-y-mediciones-prácticas)
9. [Casos reales: quién usa qué](#9-casos-reales-quién-usa-qué)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El costo real de read/write

Cada llamada a `read()` o `write()` involucra pasos que parecen
invisibles pero tienen costo real:

```
  read(fd, buf, 4096):

  ① Syscall: user → kernel (cambio de contexto)
  ② Kernel busca la página en el page cache
     ├── Cache hit: página ya en RAM
     └── Cache miss: leer del disco (I/O real)
  ③ Kernel COPIA datos: page cache → buffer de usuario
  ④ Retorno: kernel → user (cambio de contexto)

  Total: 2 cambios de contexto + 1 copia de datos por llamada
```

Si lees un archivo de 1 GiB con buffer de 4 KiB:

```
  Archivo: 1 GiB = 262144 páginas de 4 KiB

  read() con buffer de 4 KiB:
    262144 syscalls × (~300 ns por syscall) ≈ 79 ms solo en syscalls
    262144 copias de 4 KiB = 1 GiB copiado de kernel a usuario
    Total overhead: ~79 ms + tiempo de copia

  read() con buffer de 64 KiB:
    16384 syscalls → ~5 ms en syscalls
    Mismo 1 GiB copiado
    Mejor, pero la copia sigue existiendo

  mmap:
    1 syscall (mmap) + 1 syscall (munmap)
    0 copias — accedes directamente al page cache
    Las páginas se cargan bajo demanda (page faults)
```

### Desglose del overhead de read/write

```
  Componente              Costo aproximado
  ──────────────────────────────────────────────────────
  Syscall (user↔kernel)   ~100-300 ns por invocación
  Copia de datos           ~0.5-2 GB/s (depende del CPU)
  Page cache lookup        ~50-100 ns
  Disk I/O (si miss)       ~50 µs (SSD) — ~5 ms (HDD)
```

El punto clave: incluso cuando los datos ya están en cache (sin I/O
a disco), `read` sigue haciendo **una copia y una syscall** cada vez.
`mmap` elimina ambos para accesos subsecuentes.

---

## 2. Zero-copy: eliminando la copia intermedia

### El problema de la copia con read/write

```
  Con read():
  ┌────────────┐  copy_to_user()  ┌────────────┐
  │ Page cache │ ────────────────►│ Buffer     │
  │ (kernel)   │  memcpy interno  │ (usuario)  │
  │            │  del kernel      │            │
  │ Página A   │                  │ Copia de A │
  └────────────┘                  └────────────┘
       ↑                               ↑
  datos en RAM                    OTRA copia en RAM
  una vez                         del MISMO contenido
```

```
  Con mmap():
  ┌────────────┐                  ┌────────────┐
  │ Page cache │                  │ Espacio    │
  │ (kernel)   │                  │ virtual    │
  │            │                  │ (usuario)  │
  │ Página A   │◄── tabla de ────│ ptr[...]   │
  └────────────┘    páginas       └────────────┘
       ↑              (MMU)
  datos en RAM                    La MMU redirige
  UNA sola vez                    al MISMO lugar físico
```

Con `mmap`, la MMU (Memory Management Unit) del CPU mapea las
direcciones virtuales del proceso directamente a las páginas físicas
del page cache. No existe copia — es la **misma memoria física**
vista desde dos espacios de direcciones.

### Implicación de rendimiento

Para un archivo de 100 MiB leído completamente:

```
  Método              Copias de datos    Syscalls
  ──────────────────────────────────────────────────────
  read(buf, 4K)       100 MiB × 1        25600
  read(buf, 64K)      100 MiB × 1        1600
  read(buf, 1M)       100 MiB × 1        100
  mmap + acceso       0 MiB              2 (mmap+munmap)
```

En todos los casos de `read`, los 100 MiB se copian de kernel a usuario.
Con `mmap`, el proceso accede directamente — sin copia ni syscalls
adicionales.

### sendfile: zero-copy para transferencia entre fds

Si necesitas enviar un archivo a un socket (servidor web), `read` +
`write` hace **dos** copias:

```
  read(file_fd, buf, size);   // disco → page cache → buffer usuario
  write(sock_fd, buf, size);  // buffer usuario → socket buffer kernel

  Total: 2 copias + 4 cambios de contexto
```

`sendfile` elimina la copia a espacio de usuario:

```c
#include <sys/sendfile.h>

// Zero-copy: page cache → socket buffer (sin pasar por usuario)
sendfile(sock_fd, file_fd, NULL, file_size);

// Total: 0 copias a usuario + 2 cambios de contexto
```

```
  read+write:
  Disco → [Page cache] →copy→ [User buf] →copy→ [Socket buf] → Red

  sendfile:
  Disco → [Page cache] ──────────────────→ [Socket buf] → Red
                         sin copia a usuario
```

### splice: zero-copy generalizado

```c
#include <fcntl.h>

// Mover datos entre dos fds sin pasar por espacio de usuario
// Al menos uno debe ser un pipe
splice(file_fd, NULL, pipe_fd, NULL, size, SPLICE_F_MOVE);
splice(pipe_fd, NULL, sock_fd, NULL, size, SPLICE_F_MOVE);
```

`splice` es la base de `sendfile` en Linux moderno. Funciona con
pipes como intermediario y puede mover datos entre cualquier
combinación de fd (archivo, socket, pipe).

---

## 3. Lazy loading y page faults bajo demanda

### Cómo carga mmap: demand paging

Cuando `mmap` retorna, **no se lee nada del disco**. Las páginas
se cargan una a una al primer acceso:

```
  mmap(NULL, 1GiB, PROT_READ, MAP_SHARED, fd, 0) → ptr
  // 0 bytes leídos del disco. Solo se reserva espacio virtual.

  Timeline:
  ─────────────────────────────────────────────────────────
  ptr[0]       → minor page fault → kernel busca en page cache
                                    → si no está: major fault (I/O)
                                    → mapea la página → continúa

  ptr[1000]    → misma página 0, ya cargada → sin fault

  ptr[5000]    → página 1 → otro page fault → cargar

  ptr[999999]  → página 244 → page fault → cargar

  Páginas 2-243: NUNCA se tocan → NUNCA se cargan
  ─────────────────────────────────────────────────────────
```

### Minor vs major page faults

```
  Tipo de fault        Causa                    Costo
  ──────────────────────────────────────────────────────
  Minor fault          página ya en page cache  ~1-5 µs
                       (otro proceso la cargó   (solo crear
                       o lectura reciente)       mapping MMU)

  Major fault          página NO está en RAM    ~50 µs (SSD)
                       → leer del disco          ~5 ms (HDD)
```

Puedes medir los page faults de un programa con:

```bash
# Ver page faults de un comando:
$ /usr/bin/time -v ./program 2>&1 | grep -i fault
    Minor (reclaiming a frame) page faults: 234
    Major (requiring I/O) page faults: 12

# En tiempo real con perf:
$ perf stat -e page-faults,minor-faults,major-faults ./program
```

### Ventaja para acceso parcial

```
  Archivo de 10 GiB. Solo necesitas leer los primeros 100 bytes
  y los últimos 100 bytes:

  Con read():
  Opción 1: leer 100 bytes, lseek al final, leer 100 bytes
             → 2 syscalls, rápido, pero necesitas saber los offsets

  Con mmap():
  ptr = mmap(NULL, 10GiB, PROT_READ, MAP_SHARED, fd, 0);
  // "Mapea" 10 GiB pero NO carga nada
  memcpy(buf1, ptr, 100);           // 1 page fault, carga 4 KiB
  memcpy(buf2, ptr + 10GiB - 100, 100);  // 1 page fault, carga 4 KiB
  // Total: 8 KiB leídos de disco para un archivo de 10 GiB
```

En este caso, tanto `read` como `mmap` son eficientes. Pero `mmap`
escala mejor cuando el patrón de acceso es impredecible — no necesitas
planificar tus `lseek`.

### Prefetch del kernel

El kernel detecta patrones de acceso secuencial y precarga páginas
adelantándose (readahead):

```
  Acceso secuencial detectado:
  ptr[0]     → fault → cargar página 0
  ptr[4096]  → fault → cargar página 1
  ptr[8192]  → kernel detecta patrón secuencial
               → precarga páginas 3, 4, 5, 6, ... automáticamente
  ptr[12288] → ¡sin fault! Ya estaba precargada
```

Puedes controlar esto con `madvise`:

```c
madvise(ptr, size, MADV_SEQUENTIAL);  // agresivo readahead
madvise(ptr, size, MADV_RANDOM);      // desactivar readahead
madvise(ptr, size, MADV_WILLNEED);    // precargar rango específico
```

---

## 4. Page cache del kernel: la capa invisible

El **page cache** es el mecanismo del kernel para cachear datos de
archivos en RAM. Tanto `read/write` como `mmap` lo usan:

```
  ┌─────────────────────────────────────────────────────┐
  │                    Proceso                          │
  │                                                     │
  │  read/write:          mmap:                         │
  │  ┌──────────┐        ┌──────────┐                   │
  │  │ buffer   │        │ espacio  │                   │
  │  │ usuario  │        │ virtual  │                   │
  │  └────┬─────┘        └────┬─────┘                   │
  │       │ copia              │ mapping directo (MMU)  │
  ├───────┼────────────────────┼────────────────────────┤
  │       ▼                    ▼                        │
  │  ┌──────────────────────────────────────────┐       │
  │  │           PAGE CACHE del kernel          │       │
  │  │                                          │       │
  │  │  Páginas en RAM de archivos recientes    │       │
  │  │  Compartido entre TODOS los procesos     │       │
  │  └──────────────────┬───────────────────────┘       │
  │                     │                               │
  │               Kernel                                │
  └─────────────────────┼───────────────────────────────┘
                        │ I/O (solo si la página no está en cache)
                        ▼
                  ┌──────────┐
                  │  Disco   │
                  └──────────┘
```

### Lo que implica

1. **Leer un archivo dos veces con read() es rápido la segunda vez**:
   la primera carga al page cache, la segunda lee del cache (no del disco).

2. **Un proceso que usa read() y otro que usa mmap() sobre el mismo
   archivo comparten las mismas páginas en cache**: el page cache es
   global.

3. **La RAM "libre" en Linux es casi siempre page cache**: el kernel
   llena la RAM libre con cache de archivos. `free -h` muestra esto:

```bash
$ free -h
              total    used    free   shared  buff/cache  available
Mem:          16Gi    4.2Gi   512Mi    256Mi      11.3Gi     11.5Gi
```

`buff/cache` (11.3 GiB) es RAM usada como page cache. Está "usada"
pero es recuperable — si un proceso necesita más RAM, el kernel descarta
páginas de cache.

### Ver el page cache de un archivo

```bash
# Qué porcentaje del archivo está en cache:
$ vmtouch /var/log/syslog
           Files: 1
     Directories: 0
  Resident Pages: 488/488  2M/2M  100%
         Elapsed: 0.000291 seconds

# Precargar un archivo completo al cache:
$ vmtouch -t /path/to/file

# Expulsar del cache:
$ vmtouch -e /path/to/file
```

### Coherencia read/write ↔ mmap

Si un proceso escribe con `write()` y otro lee con `mmap()`, ¿ven
datos consistentes? **Sí**, porque ambos operan sobre el page cache:

```
  Proceso A: write(fd, "HELLO", 5);
  → escribe al page cache (misma página física)

  Proceso B: ptr = mmap(MAP_SHARED, fd);
  → ptr[0..4] == "HELLO" (misma página física)
```

La coherencia es automática para MAP_SHARED. Con MAP_PRIVATE,
el proceso ve una **copia CoW** que puede divergir.

---

## 5. Acceso aleatorio eficiente

### El problema de lseek + read

Para acceso aleatorio a un archivo de registros:

```c
// Con read/write: cada acceso son 2 syscalls
record_t read_record(int fd, int index) {
    record_t rec;
    lseek(fd, index * sizeof(record_t), SEEK_SET);  // syscall 1
    read(fd, &rec, sizeof(record_t));                // syscall 2
    return rec;
}
// 1000 accesos aleatorios = 2000 syscalls
```

```c
// Con mmap: 0 syscalls por acceso
record_t *records = mmap(NULL, file_size, PROT_READ,
                         MAP_SHARED, fd, 0);

record_t read_record(record_t *records, int index) {
    return records[index];  // acceso directo por índice
}
// 1000 accesos aleatorios = 0 syscalls (solo page faults)
```

### Comparación de costos

```
  Operación                lseek+read       mmap
  ──────────────────────────────────────────────────────
  Primer acceso a          ~200 ns          ~3 µs
  registro en cache        (2 syscalls)     (page fault)

  Accesos subsecuentes     ~200 ns          ~10 ns
  en misma página          (2 syscalls)     (solo memoria)

  1000 accesos             ~200 µs          ~10 µs*
  aleatorios en cache      (2000 syscalls)  (accesos directos)

  * Asumiendo la mayoría en páginas ya cargadas
```

El page fault inicial es más caro que una syscall, pero los accesos
subsecuentes a la misma página son **accesos a memoria** (~10 ns),
no syscalls (~200 ns). Para patrones con localidad (acceso repetido
a las mismas zonas), mmap gana significativamente.

### pread como alternativa intermedia

```c
// pread: atómico, sin lseek, pero sigue siendo una syscall
record_t rec;
pread(fd, &rec, sizeof(rec), index * sizeof(rec));
// 1000 accesos = 1000 syscalls (mejor que lseek+read = 2000)
```

`pread` elimina el `lseek` pero sigue haciendo una copia por acceso.

---

## 6. Archivos grandes: más allá de la RAM

### ¿Puedo mmap un archivo de 100 GiB con 8 GiB de RAM?

**Sí**, en un sistema de 64 bits. `mmap` solo reserva espacio en la
**tabla de páginas virtuales**. Las páginas físicas se asignan bajo
demanda:

```
  Sistema: 8 GiB RAM, archivo de 100 GiB

  mmap(NULL, 100GiB, PROT_READ, MAP_SHARED, fd, 0)
  → Reserva 100 GiB de espacio virtual (hay ~128 TiB disponibles)
  → 0 bytes de RAM consumidos

  Acceso a ptr[offset]:
  → Si la página está en cache: ~10 ns
  → Si no: page fault → cargar del disco → evictar otra página
  → El kernel mantiene un "working set" en RAM
  → Las páginas menos usadas se evictan automáticamente (LRU)
```

```
  Working set del proceso (páginas activas en RAM):
  ┌──────────────────────────────────────────────────────┐
  │  Espacio virtual: 100 GiB                            │
  │  ┌───┬───┬   ┬───┬   ┬───┬   ┬───┬   ┬───┐         │
  │  │ P │ P │...│   │...│ P │...│   │...│ P │         │
  │  │ 0 │ 1 │   │   │   │42 │   │   │   │99k│         │
  │  └─┬─┴─┬─┘   └───┘   └─┬─┘   └───┘   └─┬─┘         │
  │    │   │     (no en      │     (no en     │           │
  │    │   │      RAM)       │      RAM)      │           │
  │    ▼   ▼                 ▼                ▼           │
  │  ┌─────────────────────────────────────────┐         │
  │  │  RAM física: ~8 GiB                     │         │
  │  │  Solo las páginas activas están aquí    │         │
  │  └─────────────────────────────────────────┘         │
  └──────────────────────────────────────────────────────┘
```

### Limitaciones en 32 bits

En sistemas de 32 bits, el espacio virtual es de ~3 GiB. Mapear
archivos mayores requiere mapear porciones (ventanas) y deslizarlas:

```c
// 32 bits: mapeo por ventanas
#define WINDOW_SIZE (256 * 1024 * 1024)  // 256 MiB

void process_large_file_32bit(int fd, off_t file_size) {
    for (off_t offset = 0; offset < file_size; offset += WINDOW_SIZE) {
        size_t len = WINDOW_SIZE;
        if (offset + len > file_size)
            len = file_size - offset;

        void *ptr = mmap(NULL, len, PROT_READ, MAP_SHARED, fd, offset);
        // procesar ventana...
        munmap(ptr, len);
    }
}
```

En 64 bits esto no es necesario — el espacio virtual de ~128 TiB
permite mapear cualquier archivo real.

### Presión de memoria y page eviction

Cuando la RAM se llena, el kernel evicta páginas del page cache para
hacer espacio:

```
  Página limpia (no modificada):
  → Se descarta directamente (se puede releer del disco)

  Página sucia (MAP_SHARED + escrita):
  → Se escribe al disco (writeback) antes de evictar

  Política de evicción: LRU aproximado
  → Las páginas menos usadas recientemente se evictan primero
```

`madvise` ayuda al kernel a evictar de forma inteligente:

```c
// Ya procesé este rango — puedes evictarlo
madvise(ptr + processed_start, processed_len, MADV_DONTNEED);

// Voy a necesitar este rango pronto — precárgalo
madvise(ptr + future_start, future_len, MADV_WILLNEED);
```

---

## 7. Cuándo read/write es mejor que mmap

`mmap` no es universalmente superior. Hay escenarios donde `read/write`
gana:

### 1. Lectura secuencial única

```
  Leer un archivo completo una sola vez, de inicio a fin:

  read() con buffer de 64 KiB:
  + Readahead del kernel optimizado para read
  + Pocas syscalls con buffer grande
  + Sin overhead de establecer mapeo y tabla de páginas
  + El kernel puede usar DMA directamente

  mmap():
  + Zero-copy
  - Overhead de establecer entries en la tabla de páginas
  - Page faults por cada página nueva (aunque sean minor)
  - Para lectura única, el overhead de setup no se amortiza
```

En benchmarks, para lectura secuencial completa con buffer grande,
`read` y `mmap` tienen rendimiento comparable. `mmap` no tiene una
ventaja clara en este caso.

### 2. Archivos pequeños

```
  Archivo de 100 bytes:

  read():
  1 syscall, 100 bytes copiados → ~300 ns total

  mmap():
  1 syscall (mmap) + 1 page fault + 1 syscall (munmap)
  → ~5 µs total (15× más lento por el setup)
```

Para archivos pequeños (< 4 KiB), el overhead de mmap/munmap y el
page fault inicial superan el costo de una copia con read.

### 3. Escritura secuencial a archivo nuevo

```c
// Con write(): natural y eficiente
int fd = open("output.bin", O_WRONLY | O_CREAT, 0644);
for (int i = 0; i < N; i++) {
    write(fd, &data[i], sizeof(data[i]));
}
close(fd);

// Con mmap(): incómodo — necesitas saber el tamaño final
int fd = open("output.bin", O_RDWR | O_CREAT, 0644);
ftruncate(fd, final_size);  // ¿cuánto va a medir?
void *ptr = mmap(NULL, final_size, ...);
memcpy(ptr, data, final_size);
munmap(ptr, final_size);
```

Si no conoces el tamaño final, `write` es mucho más natural. Con `mmap`
necesitas el tamaño por adelantado o hacer `ftruncate` + `mremap`
repetidamente.

### 4. Streams y pipes

```c
// mmap NO funciona con pipes, sockets, stdin ni terminales
int fd = 0;  // stdin
void *ptr = mmap(NULL, 4096, PROT_READ, MAP_SHARED, fd, 0);
// → MAP_FAILED con ENODEV

// Solo read/write funcionan con streams
char buf[4096];
read(0, buf, sizeof(buf));  // ✓
```

`mmap` requiere un archivo con tamaño conocido y posiciones (seekable).
No funciona con pipes, sockets, FIFOs ni dispositivos de caracteres.

### 5. Manejo fino de errores de I/O

```c
// Con read(): el error se detecta en el retorno
ssize_t n = read(fd, buf, size);
if (n == -1) {
    perror("read");  // errno tiene el error específico
    // Puedes reintentar, abortar, o manejar
}

// Con mmap(): el error llega como señal
char c = ptr[offset];  // Si hay error de I/O → SIGBUS
// Manejar SIGBUS es complejo y propenso a errores
```

### Tabla de decisión

```
  Escenario                           Mejor opción
  ──────────────────────────────────────────────────────
  Lectura secuencial única            read (buffer ≥ 64K)
  Acceso aleatorio frecuente          mmap
  Archivo < 4 KiB                     read
  Archivo grande con acceso parcial   mmap
  Base de datos / índice              mmap
  Escritura a archivo nuevo           write
  Pipes, sockets, stdin               read/write (único)
  IPC entre procesos                  mmap (MAP_SHARED)
  Servidor web (envío de archivos)    sendfile/splice
  Archivo leído por muchos procesos   mmap (comparte cache)
  Necesitas manejo fino de errores    read/write
```

---

## 8. Benchmarks y mediciones prácticas

### Programa de benchmark: read vs mmap

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// Sumar todos los bytes para forzar lectura real
static long sum_bytes_read(const char *path, size_t bufsize) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return -1;

    char *buf = malloc(bufsize);
    long sum = 0;
    ssize_t n;

    while ((n = read(fd, buf, bufsize)) > 0) {
        for (ssize_t i = 0; i < n; i++)
            sum += (unsigned char)buf[i];
    }

    free(buf);
    close(fd);
    return sum;
}

static long sum_bytes_mmap(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) return -1;

    struct stat sb;
    fstat(fd, &sb);

    char *data = mmap(NULL, sb.st_size, PROT_READ,
                      MAP_SHARED, fd, 0);
    close(fd);
    if (data == MAP_FAILED) return -1;

    madvise(data, sb.st_size, MADV_SEQUENTIAL);

    long sum = 0;
    for (off_t i = 0; i < sb.st_size; i++)
        sum += (unsigned char)data[i];

    munmap(data, sb.st_size);
    return sum;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s archivo\n", argv[0]);
        return 1;
    }

    // Warm up: primera lectura carga al page cache
    sum_bytes_read(argv[1], 65536);

    double t;

    t = now_sec();
    long s1 = sum_bytes_read(argv[1], 4096);
    printf("read(4K):  %.3f ms  (sum=%ld)\n",
           (now_sec() - t) * 1000, s1);

    t = now_sec();
    long s2 = sum_bytes_read(argv[1], 65536);
    printf("read(64K): %.3f ms  (sum=%ld)\n",
           (now_sec() - t) * 1000, s2);

    t = now_sec();
    long s3 = sum_bytes_mmap(argv[1]);
    printf("mmap:      %.3f ms  (sum=%ld)\n",
           (now_sec() - t) * 1000, s3);

    return 0;
}
```

### Resultados típicos (archivo de 100 MiB en cache)

```
  Método         Tiempo     Notas
  ──────────────────────────────────────────────────
  read(4K)       85 ms      25600 syscalls
  read(64K)      62 ms      1600 syscalls
  read(1M)       58 ms      100 syscalls
  mmap           45 ms      2 syscalls + page faults
```

Los resultados varían según el hardware, pero la tendencia es:
- `read` con buffer pequeño es el más lento (overhead de syscalls).
- `read` con buffer grande se acerca a `mmap`.
- `mmap` es ligeramente más rápido por la eliminación de copias.
- La diferencia real es mayor con acceso aleatorio repetido.

### Cómo medir page faults

```bash
# Page faults de tu programa:
$ perf stat -e page-faults ./my_program big_file
    Performance counter stats for './my_program big_file':
        25,600  page-faults

# Con /usr/bin/time:
$ /usr/bin/time -v ./my_program big_file 2>&1 | grep fault
    Minor (reclaiming a frame) page faults: 25600
    Major (requiring I/O) page faults: 0
```

### Cómo expulsar el page cache para medir cold reads

```bash
# Expulsar todo el page cache (requiere root):
$ sync && echo 3 | sudo tee /proc/sys/vm/drop_caches

# Expulsar un archivo específico:
$ vmtouch -e /path/to/file

# Ahora las lecturas serán "cold" (I/O real a disco)
```

---

## 9. Casos reales: quién usa qué

### Software que usa mmap

```
  Software         Uso de mmap
  ──────────────────────────────────────────────────────
  SQLite           mapea el archivo .db para lectura
  MongoDB          mapea archivos de datos (WiredTiger)
  Elasticsearch    archivos de índice Lucene
  Git              packfiles (.pack) para acceso aleatorio
  ld (linker)      mapea .o y .so para leer símbolos
  Kernel Linux     carga ejecutables ELF con mmap
  Chrome/Firefox   mapea fuentes, imágenes, caché
  grep/ripgrep     mmap para archivos grandes
```

### Software que prefiere read/write

```
  Software         Por qué read/write
  ──────────────────────────────────────────────────────
  PostgreSQL       control fino de I/O y buffers propios
  MySQL (InnoDB)   gestión de buffer pool propia
  nginx            sendfile para archivos estáticos
  Apache           sendfile o read+write
  cp, dd           lectura secuencial, write a destino
  tar              streams secuenciales
  ssh/scp          streams sobre sockets
```

### PostgreSQL: el caso contra mmap

PostgreSQL históricamente evita `mmap` para sus archivos de datos.
Razones:

1. **Manejo de errores**: un error de I/O en mmap genera SIGBUS.
   Con read, se obtiene errno y se puede manejar gracefully.

2. **Buffer management**: PostgreSQL quiere controlar qué páginas
   están en RAM y cuándo se escriben (WAL before data).

3. **Portabilidad**: comportamiento de mmap varía entre sistemas.

4. **fsync semántica**: `msync` + `fsync` tienen interacciones
   complicadas en algunos filesystems.

### Git: el caso a favor de mmap

Git usa mmap extensivamente para packfiles:

```
  pack-xxx.pack (puede ser de varios GiB):
  ┌──────────────────────────────────────────────┐
  │  objeto 1  │  objeto 2  │  ...  │ objeto N  │
  └──────────────────────────────────────────────┘

  Git necesita acceso aleatorio a objetos individuales.
  Con mmap:
  - Busca en el índice (pack-xxx.idx)
  - Accede al offset en el mmap del packfile
  - Solo carga las páginas de ese objeto
  - Múltiples operaciones git comparten el page cache
```

---

## 10. Errores comunes

### Error 1: usar mmap para archivos que se procesan una sola vez secuencialmente

```c
// INNECESARIO: mmap para leer un archivo de inicio a fin
void *data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
for (off_t i = 0; i < size; i++) {
    process_byte(data[i]);
}
munmap(data, size);

// IGUAL DE BUENO (y más simple):
char buf[65536];
ssize_t n;
while ((n = read(fd, buf, sizeof(buf))) > 0) {
    for (ssize_t i = 0; i < n; i++)
        process_byte(buf[i]);
}
```

Para procesamiento secuencial único, la complejidad adicional de mmap
no se justifica. `read` con buffer grande es igual de eficiente y
más simple.

### Error 2: esperar que mmap sea siempre más rápido

```c
// Archivo de 50 bytes — mmap es más lento:
void *data = mmap(NULL, 50, ...);    // syscall + page table setup
char c = ((char *)data)[0];          // page fault
munmap(data, 50);                    // syscall
// Total: ~5 µs

// read es más rápido para archivos pequeños:
char buf[50];
read(fd, buf, 50);                   // 1 syscall + 50 bytes copiados
// Total: ~300 ns
```

### Error 3: olvidar que SIGBUS puede ocurrir con mmap

```c
// MAL: no manejar SIGBUS
char *data = mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
// Si el archivo se trunca mientras lo leemos → SIGBUS
for (off_t i = 0; i < file_size; i++) {
    sum += data[i];  // SIGBUS posible
}

// BIEN: opciones
// 1. Verificar que nadie más modifica el archivo (flock)
// 2. Instalar handler de SIGBUS (complejo)
// 3. Aceptar el riesgo si es un escenario improbable
```

### Error 4: no usar madvise en patrones predecibles

```c
// MAL: dejar que el kernel adivine
void *data = mmap(NULL, huge_size, PROT_READ, MAP_SHARED, fd, 0);
// Acceso secuencial sin hint — kernel puede no activar readahead
for (off_t i = 0; i < huge_size; i++) { /* ... */ }

// BIEN: dar hints al kernel
void *data = mmap(NULL, huge_size, PROT_READ, MAP_SHARED, fd, 0);
madvise(data, huge_size, MADV_SEQUENTIAL);
for (off_t i = 0; i < huge_size; i++) { /* ... */ }
```

### Error 5: confundir zero-copy de mmap con zero-copy de sendfile

```c
// mmap + write a socket NO es zero-copy:
void *data = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
write(sock_fd, data, size);
// write() COPIA de data (page cache) a socket buffer
// Total: 1 copia

// sendfile SÍ es zero-copy:
sendfile(sock_fd, fd, NULL, size);
// Datos van de page cache a socket buffer sin copia de usuario
// Total: 0 copias (en hardware moderno con scatter-gather DMA)
```

`mmap` es zero-copy para **acceso del proceso**. Para **transferir
entre fds**, necesitas `sendfile` o `splice`.

---

## 11. Cheatsheet

```
  READ/WRITE vs MMAP: COMPARACIÓN DE COSTOS
  ──────────────────────────────────────────────────────────
                    read/write          mmap
  Syscalls          1 por operación     2 total (mmap+munmap)
  Copia de datos    1 por read/write    0 (zero-copy)
  Setup             ninguno             page table + VMA
  Primer acceso     ~300 ns (syscall)   ~3 µs (page fault)
  Acceso repetido   ~300 ns (syscall)   ~10 ns (memoria)
  Archivos < 4 KiB  ✓ más rápido       ✗ overhead
  Acceso aleatorio  ✗ lseek+read       ✓ ptr[offset]
  Streams/pipes     ✓ funciona          ✗ no funciona
  Error handling    ✓ errno             ✗ SIGBUS

  MECANISMOS ZERO-COPY
  ──────────────────────────────────────────────────────────
  mmap              zero-copy para acceso del proceso
  sendfile(out,in)  zero-copy archivo → socket
  splice            zero-copy entre fds vía pipe
  copy_file_range   zero-copy archivo → archivo (Linux 4.5+)

  PAGE CACHE
  ──────────────────────────────────────────────────────────
  read y mmap comparten el mismo page cache
  free -h: buff/cache muestra RAM usada como cache
  vmtouch: ver/manipular cache de archivos específicos
  echo 3 > /proc/sys/vm/drop_caches: limpiar (root)

  MADVISE: HINTS AL KERNEL
  ──────────────────────────────────────────────────────────
  MADV_SEQUENTIAL   lectura de inicio a fin → readahead
  MADV_RANDOM       acceso aleatorio → sin readahead
  MADV_WILLNEED     precargar un rango al cache
  MADV_DONTNEED     ya no necesito estas páginas

  CUÁNDO USAR CADA UNO
  ──────────────────────────────────────────────────────────
  mmap: acceso aleatorio, archivos grandes, bases de datos,
        IPC, archivos leídos repetidamente
  read: lectura secuencial única, archivos pequeños,
        streams, manejo fino de errores
  sendfile: enviar archivo a socket (servidor web)

  MEDIR RENDIMIENTO
  ──────────────────────────────────────────────────────────
  perf stat -e page-faults ./prog    contar page faults
  /usr/bin/time -v ./prog            minor/major faults
  strace -c ./prog                   contar syscalls
  vmtouch file                       estado del page cache
```

---

## 12. Ejercicios

### Ejercicio 1: benchmark read vs mmap

Escribe un programa que compare el rendimiento de `read()` (con
distintos tamaños de buffer) y `mmap()` para leer un archivo completo.
Debe funcionar con archivos de cualquier tamaño.

```c
// Esqueleto:
#include <stdio.h>
#include <time.h>

typedef struct {
    const char *name;
    double elapsed_ms;
    long checksum;
} result_t;

result_t bench_read(const char *path, size_t bufsize);
result_t bench_mmap(const char *path);

int main(int argc, char *argv[]) {
    // 1. Crear archivo de prueba si no existe:
    //    dd if=/dev/urandom of=test.dat bs=1M count=100
    // 2. Warm up (primera lectura para cachear)
    // 3. Ejecutar cada método 3 veces, reportar mediana
    // 4. Comparar checksums para verificar correctitud
}
```

Requisitos:
- Probar `read` con buffers de 4K, 16K, 64K, 256K y 1M.
- Ejecutar cada prueba 3 veces y reportar la mediana.
- Verificar que todos los métodos producen el mismo checksum.
- Probar con el archivo en cache y con cache expulsado (`vmtouch -e`).

**Pregunta de reflexión**: ¿cómo cambian los resultados si expulsas
el page cache antes de cada prueba? ¿Cuál de los dos (read vs mmap)
sufre más con cold reads? ¿Por qué? Piensa en cómo cada método
interactúa con el readahead del kernel.

---

### Ejercicio 2: grep con read vs mmap

Implementa una búsqueda de texto (`grep` simplificado) con dos
implementaciones: una usando `read()` y otra usando `mmap()`. Compara
el rendimiento con archivos de distintos tamaños.

```c
// Esqueleto:
int grep_read(const char *path, const char *pattern);
int grep_mmap(const char *path, const char *pattern);

// Ambas funciones deben:
// 1. Buscar 'pattern' en cada línea
// 2. Imprimir las líneas que coinciden con número de línea
// 3. Retornar el número de coincidencias
```

Requisitos:
- Usar `memmem()` para buscar el patrón en los datos mapeados.
- Para la versión read, manejar el caso de una línea que cruza el
  borde del buffer.
- Probar con archivos de 1 KiB, 1 MiB, 100 MiB y 1 GiB.

**Pregunta de reflexión**: la versión con `read()` necesita manejar
el caso de una línea que empieza en un buffer y termina en el siguiente.
¿Cómo lo resolverías? ¿Por qué la versión con `mmap` no tiene este
problema? ¿Qué implica esto para la complejidad del código?

---

### Ejercicio 3: copia de archivos — 4 métodos

Implementa una copia de archivo (`cp` simplificado) usando cuatro
métodos y compara rendimiento:

```c
// Método 1: read + write con buffer
int copy_rw(const char *src, const char *dst, size_t bufsize);

// Método 2: mmap source + write destino
int copy_mmap_write(const char *src, const char *dst);

// Método 3: mmap ambos + memcpy
int copy_mmap_mmap(const char *src, const char *dst);

// Método 4: copy_file_range (zero-copy kernel)
int copy_cfr(const char *src, const char *dst);
```

```c
// Método 4 usa copy_file_range (Linux 4.5+):
#include <unistd.h>

ssize_t copy_file_range(int fd_in, off_t *off_in,
                        int fd_out, off_t *off_out,
                        size_t len, unsigned int flags);
```

Requisitos:
- Preservar permisos del archivo original.
- Medir tiempo de cada método.
- Probar con archivos de 1 MiB, 100 MiB y 1 GiB.
- Verificar que la copia es idéntica (`cmp` o checksum).

**Pregunta de reflexión**: clasifica los 4 métodos por número de
copias de datos que se realizan. ¿Cuántas veces se copian los datos
en cada método (disco → page cache → espacio usuario → page cache
destino → disco)? ¿Cuál logra la menor cantidad de copias? ¿Por qué
`copy_file_range` existe si ya existía `sendfile`?

---
