# Mapeo de Archivos con mmap

## Índice

1. [Qué es mmap y por qué importa](#1-qué-es-mmap-y-por-qué-importa)
2. [La syscall mmap: firma y parámetros](#2-la-syscall-mmap-firma-y-parámetros)
3. [Protección: PROT_READ, PROT_WRITE, PROT_EXEC](#3-protección-prot_read-prot_write-prot_exec)
4. [MAP_SHARED vs MAP_PRIVATE](#4-map_shared-vs-map_private)
5. [munmap: desmapear memoria](#5-munmap-desmapear-memoria)
6. [msync: forzar escritura a disco](#6-msync-forzar-escritura-a-disco)
7. [Mapeo de archivos paso a paso](#7-mapeo-de-archivos-paso-a-paso)
8. [Operaciones sobre regiones mapeadas](#8-operaciones-sobre-regiones-mapeadas)
9. [Alineación, tamaño y páginas](#9-alineación-tamaño-y-páginas)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es mmap y por qué importa

`mmap` proyecta el contenido de un archivo directamente en el espacio
de direcciones virtual del proceso. En lugar de usar `read()`/`write()`
para copiar datos entre el kernel y el usuario, accedes al archivo como
si fuera un array en memoria:

```
  Con read/write:
  ┌─────────────┐    read()     ┌──────────────┐
  │  Archivo    │──────────────►│  Buffer      │
  │  en disco   │    copia      │  en usuario   │
  └─────────────┘               └──────────────┘
       ↕ I/O                    proceso trabaja
  ┌─────────────┐               con la copia
  │  Page cache │
  │  del kernel │
  └─────────────┘

  Con mmap:
  ┌─────────────┐               ┌──────────────┐
  │  Archivo    │               │  Espacio     │
  │  en disco   │               │  virtual del │
  └─────────────┘               │  proceso     │
       ↕ I/O                    │              │
  ┌─────────────┐  mismas       │  ptr[0]      │
  │  Page cache │──páginas─────►│  ptr[1]      │
  │  del kernel │  compartidas  │  ptr[2]      │
  └─────────────┘               │  ...         │
                                └──────────────┘
```

La diferencia clave: con `mmap`, las páginas del page cache del kernel
se mapean **directamente** en el espacio del proceso. No hay copia. El
proceso lee y escribe la misma memoria que el kernel usa para cachear
el archivo.

### Ventajas principales

```
  Ventaja                     Explicación
  ──────────────────────────────────────────────────────────
  Sin copia (zero-copy)       No hay buffer intermedio
  Acceso aleatorio natural    ptr[offset] en vez de lseek+read
  Lazy loading                Solo carga las páginas que tocas
  Compartir entre procesos    MAP_SHARED = mismas páginas físicas
  El kernel gestiona I/O      Page fault → carga bajo demanda
```

---

## 2. La syscall mmap: firma y parámetros

```c
#include <sys/mman.h>

void *mmap(void *addr, size_t length, int prot, int flags,
           int fd, off_t offset);
```

### Parámetros

```
  Parámetro   Propósito                    Valor típico
  ──────────────────────────────────────────────────────────
  addr        dirección sugerida           NULL (kernel elige)
  length      tamaño del mapeo en bytes    tamaño del archivo
  prot        permisos de memoria          PROT_READ | PROT_WRITE
  flags       tipo de mapeo                MAP_SHARED o MAP_PRIVATE
  fd          descriptor del archivo       open() previo
  offset      offset dentro del archivo    0 (o múltiplo de página)
```

### Retorno

- **Éxito**: puntero a la región mapeada.
- **Error**: `MAP_FAILED` (no `NULL`).

```c
void *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
if (ptr == MAP_FAILED) {
    perror("mmap");
    // manejar error
}
```

> **Atención**: `MAP_FAILED` se define como `((void *)-1)`, no como
> `NULL`. Verificar `ptr == NULL` es un error que deja pasar fallos.

---

## 3. Protección: PROT_READ, PROT_WRITE, PROT_EXEC

Los flags de protección definen qué operaciones permite la MMU sobre
las páginas mapeadas:

```
  Flag          Permite              Nota
  ──────────────────────────────────────────────────────────
  PROT_READ     leer la memoria      necesario para casi todo
  PROT_WRITE    escribir             requiere fd con O_RDWR
                                     (si MAP_SHARED)
  PROT_EXEC     ejecutar código      JIT, plugins dinámicos
  PROT_NONE     sin acceso           reservar rango sin usar
```

### Combinaciones comunes

```c
// Solo lectura (consultar datos, archivo de configuración)
PROT_READ

// Lectura + escritura (modificar archivo)
PROT_READ | PROT_WRITE

// Lectura + ejecución (cargar código: JIT, .so)
PROT_READ | PROT_EXEC
```

### Coherencia con el modo de open

El fd debe tener permisos compatibles con el `prot` solicitado:

```
  prot                    fd necesita
  ──────────────────────────────────────────────────────
  PROT_READ               O_RDONLY o O_RDWR
  PROT_WRITE + MAP_SHARED O_RDWR (escritura real al archivo)
  PROT_WRITE + MAP_PRIVATE O_RDONLY basta (copy-on-write)
```

Si los permisos no coinciden, `mmap` falla con `EACCES`.

### Violaciones de protección

Acceder a memoria mapeada de forma incompatible con `prot` genera
`SIGSEGV` (segfault):

```c
void *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);

// Esto genera SIGSEGV:
((char *)ptr)[0] = 'x';  // ¡escribir en región PROT_READ!
```

### mprotect: cambiar protección después del mapeo

```c
#include <sys/mman.h>

// Cambiar una región de solo lectura a lectura+escritura
if (mprotect(ptr, size, PROT_READ | PROT_WRITE) == -1) {
    perror("mprotect");
}
```

`mprotect` opera sobre **páginas completas**. El puntero y el tamaño
deben estar alineados a página.

---

## 4. MAP_SHARED vs MAP_PRIVATE

Esta es la decisión más importante al usar `mmap`:

### MAP_SHARED

Las modificaciones son **visibles** para otros procesos que mapean el
mismo archivo y se **escriben al archivo** en disco:

```
  Proceso A                     Proceso B
  ┌────────────────┐            ┌────────────────┐
  │ ptr_a[100]='X' │            │ ptr_b[100]     │
  └───────┬────────┘            └───────┬────────┘
          │                             │
          ▼                             ▼
  ┌─────────────────────────────────────────────┐
  │              Páginas físicas                │
  │              (page cache)                   │
  │     ptr_a[100] == ptr_b[100] == 'X'         │
  └──────────────────────┬──────────────────────┘
                         │
                         ▼ (writeback al disco)
                   ┌──────────┐
                   │ Archivo  │
                   └──────────┘
```

- Cambios visibles inmediatamente entre procesos.
- Escritura a disco eventualmente (o con `msync`).
- IPC eficiente: múltiples procesos comparten memoria sin copias.

### MAP_PRIVATE

Crea una copia **copy-on-write (CoW)**. Las modificaciones son privadas
del proceso y **nunca** se escriben al archivo:

```
  Antes de escribir:
  ┌────────────────┐            ┌────────────────┐
  │ ptr_a          │            │ ptr_b          │
  └───────┬────────┘            └───────┬────────┘
          │                             │
          └──────────┬──────────────────┘
                     ▼
          ┌─────────────────┐
          │  Página física  │  ← compartida (read-only)
          │  del page cache │
          └─────────────────┘

  Después de que A escribe ptr_a[0] = 'X':
  ┌────────────────┐            ┌────────────────┐
  │ ptr_a          │            │ ptr_b          │
  └───────┬────────┘            └───────┬────────┘
          │                             │
          ▼                             ▼
  ┌─────────────────┐       ┌─────────────────┐
  │  Copia privada  │       │  Página original│
  │  ptr_a[0]='X'   │       │  (sin cambiar)  │
  └─────────────────┘       └─────────────────┘
```

- El kernel no copia hasta que escribes (CoW = eficiente).
- Usado internamente por el loader de ejecutables (`.text` de programas).
- Útil para crear "snapshots" en memoria de un archivo.

### Cuándo usar cada uno

```
  Caso de uso                          Flag
  ──────────────────────────────────────────────────────
  Modificar un archivo en disco        MAP_SHARED
  IPC entre procesos                   MAP_SHARED
  Leer archivo sin modificar           MAP_SHARED o MAP_PRIVATE
  Copia privada para manipular         MAP_PRIVATE
  Ejecutar código (.text de ELF)       MAP_PRIVATE
```

---

## 5. munmap: desmapear memoria

```c
#include <sys/mman.h>

int munmap(void *addr, size_t length);
```

`munmap` elimina el mapeo del espacio virtual del proceso:

```c
if (munmap(ptr, size) == -1) {
    perror("munmap");
}
// Después de munmap, acceder a ptr genera SIGSEGV
```

### Reglas de munmap

1. `addr` debe ser el mismo puntero que devolvió `mmap` (alineado a página).
2. `length` debe coincidir con el tamaño original del mapeo.
3. Para `MAP_SHARED`, los datos **no** se sincronizan automáticamente
   a disco al desmapear. Usa `msync` antes si necesitas durabilidad.
4. El proceso también puede desmapear solo una **porción** del mapeo
   (debe estar alineada a página).
5. Cuando el proceso termina, el kernel desmapea automáticamente todo.

### Desmapeo parcial

```c
// Mapeo de 4 páginas
size_t page = sysconf(_SC_PAGESIZE);  // 4096 típicamente
void *ptr = mmap(NULL, 4 * page, PROT_READ, MAP_SHARED, fd, 0);

// Desmapear solo la tercera página
munmap((char *)ptr + 2 * page, page);

// Ahora: páginas 0,1 y 3 siguen mapeadas; página 2 genera SIGSEGV
```

---

## 6. msync: forzar escritura a disco

```c
#include <sys/mman.h>

int msync(void *addr, size_t length, int flags);
```

Con `MAP_SHARED`, las modificaciones llegan al page cache inmediatamente
pero la escritura a disco es **asíncrona** (el kernel decide cuándo).
`msync` fuerza la sincronización:

### Flags de msync

```
  Flag           Comportamiento
  ──────────────────────────────────────────────────────
  MS_SYNC        sincrónico — bloquea hasta que los datos
                 están en disco (durabilidad garantizada)
  MS_ASYNC       asincrónico — inicia el writeback pero
                 retorna inmediatamente
  MS_INVALIDATE  invalida otras copias del mapeo
                 (fuerza re-lectura desde disco)
```

### Ejemplo

```c
// Modificar datos
memcpy(ptr + offset, new_data, len);

// Forzar escritura a disco (sincrónico)
if (msync(ptr, mapping_size, MS_SYNC) == -1) {
    perror("msync");
}
// Aquí los datos están garantizados en disco
```

### ¿Cuándo necesitas msync?

```
  Escenario                          ¿msync necesario?
  ──────────────────────────────────────────────────────
  Base de datos (durabilidad)        SÍ — MS_SYNC
  Log de auditoría                   SÍ — MS_SYNC
  Cache en memoria                   NO — si se pierde, se recrea
  IPC entre procesos                 NO — page cache es suficiente
  Antes de munmap (datos críticos)   SÍ — MS_SYNC
```

---

## 7. Mapeo de archivos paso a paso

### Ejemplo completo: leer un archivo con mmap

```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s archivo\n", argv[0]);
        return 1;
    }

    // 1. Abrir el archivo
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    // 2. Obtener el tamaño
    struct stat sb;
    if (fstat(fd, &sb) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }

    if (sb.st_size == 0) {
        fprintf(stderr, "Archivo vacío\n");
        close(fd);
        return 1;
    }

    // 3. Mapear el archivo
    char *data = mmap(NULL, sb.st_size, PROT_READ,
                      MAP_SHARED, fd, 0);
    if (data == MAP_FAILED) {
        perror("mmap");
        close(fd);
        return 1;
    }

    // 4. El fd ya no es necesario después de mmap
    close(fd);

    // 5. Usar el mapeo — acceder como un array
    // Ejemplo: contar líneas
    long lines = 0;
    for (off_t i = 0; i < sb.st_size; i++) {
        if (data[i] == '\n') lines++;
    }
    printf("Líneas: %ld\n", lines);

    // 6. Desmapear
    munmap(data, sb.st_size);
    return 0;
}
```

### Ejemplo completo: modificar un archivo con mmap

```c
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <ctype.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s archivo\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDWR);  // O_RDWR para MAP_SHARED+WRITE
    if (fd == -1) { perror("open"); return 1; }

    struct stat sb;
    fstat(fd, &sb);

    char *data = mmap(NULL, sb.st_size,
                      PROT_READ | PROT_WRITE,  // leer + escribir
                      MAP_SHARED,               // cambios al archivo
                      fd, 0);
    if (data == MAP_FAILED) { perror("mmap"); close(fd); return 1; }
    close(fd);

    // Convertir todo a mayúsculas — modifica el archivo directamente
    for (off_t i = 0; i < sb.st_size; i++) {
        data[i] = toupper((unsigned char)data[i]);
    }

    // Asegurar que llega a disco
    msync(data, sb.st_size, MS_SYNC);

    munmap(data, sb.st_size);
    printf("Archivo convertido a mayúsculas\n");
    return 0;
}
```

### El fd se puede cerrar después de mmap

Un detalle que sorprende a muchos: después de `mmap`, el `fd` se puede
cerrar. El mapeo sigue siendo válido porque el kernel mantiene una
referencia interna al archivo (vía el inodo):

```
  mmap(NULL, size, ..., fd, 0);
  close(fd);  // ✓ El mapeo sigue funcionando

  // El kernel internamente:
  // mmap incrementó el refcount del struct file
  // close decrementó, pero refcount > 0 por el mapeo
  // munmap será quien decremente la última referencia
```

Cerrar el fd después de mmap es una buena práctica: evita que se agoten
los descriptores en programas que mapean muchos archivos.

---

## 8. Operaciones sobre regiones mapeadas

### Acceso como array

```c
// El mapeo es simplemente memoria — usa punteros normales
char *data = mmap(...);

// Leer byte 1000
char c = data[1000];

// Escribir (si PROT_WRITE + MAP_SHARED)
data[1000] = 'A';

// Copiar un bloque
memcpy(buffer, data + offset, length);

// Buscar un byte
char *found = memchr(data, '\n', size);
```

### Operaciones con estructuras

```c
typedef struct {
    int id;
    char name[60];
} record_t;

// Mapear archivo de registros
record_t *records = mmap(NULL, file_size,
                         PROT_READ | PROT_WRITE,
                         MAP_SHARED, fd, 0);

int num_records = file_size / sizeof(record_t);

// Acceso directo por índice
printf("Registro 42: %s\n", records[42].name);

// Modificar
records[42].id = 100;
```

### Mapeo parcial con offset

Puedes mapear solo una porción del archivo especificando un offset:

```c
// Mapear 4096 bytes empezando en la posición 8192
void *ptr = mmap(NULL, 4096, PROT_READ, MAP_SHARED,
                 fd, 8192);
```

El offset **debe ser múltiplo del tamaño de página** (`sysconf(_SC_PAGESIZE)`).
Si necesitas acceder a un offset no alineado, mapea desde el múltiplo
anterior y calcula el desplazamiento:

```c
off_t target = 12345;
size_t page = sysconf(_SC_PAGESIZE);           // 4096
off_t aligned = (target / page) * page;         // 12288
size_t adjust = target - aligned;               // 57

void *base = mmap(NULL, length + adjust, PROT_READ,
                  MAP_SHARED, fd, aligned);

char *data = (char *)base + adjust;  // apunta a offset 12345
```

### Page faults: carga bajo demanda

Cuando `mmap` retorna, **ninguna página se ha cargado del disco**. La
memoria está reservada en el espacio virtual pero las páginas físicas
se asignan al primer acceso (page fault):

```
  mmap retorna ptr

  ptr[0]    → page fault → kernel carga página 0 del disco → OK
  ptr[100]  → page fault → kernel carga página 0 (misma) → OK
  ptr[5000] → page fault → kernel carga página 1 del disco → OK

  Páginas no tocadas NUNCA se cargan
```

Esto hace que `mmap` de un archivo de 1 GiB sea instantáneo: solo
carga lo que realmente usas.

### madvise: orientar al kernel

```c
#include <sys/mman.h>

int madvise(void *addr, size_t length, int advice);
```

`madvise` le dice al kernel cómo vas a usar la memoria para que
optimice el prefetch y la gestión de páginas:

```
  Advice              Efecto
  ──────────────────────────────────────────────────────
  MADV_SEQUENTIAL     lectura secuencial — prefetch agresivo
  MADV_RANDOM         acceso aleatorio — no prefetch
  MADV_WILLNEED       voy a necesitar estas páginas — cargar ya
  MADV_DONTNEED       ya no necesito estas páginas — liberar
  MADV_HUGEPAGE       usar huge pages (2MiB) si es posible
```

```c
// Archivo que leeremos secuencialmente
madvise(data, size, MADV_SEQUENTIAL);

// Precargar un rango que vamos a necesitar pronto
madvise(data + future_offset, future_length, MADV_WILLNEED);

// Liberar páginas que ya procesamos
madvise(data + processed_offset, processed_length, MADV_DONTNEED);
```

---

## 9. Alineación, tamaño y páginas

### Tamaño de página

`mmap` trabaja internamente con **páginas** (unidad mínima de la MMU):

```c
long page_size = sysconf(_SC_PAGESIZE);  // 4096 en x86-64
```

### Qué pasa cuando el archivo no es múltiplo de página

```
  Archivo: 6000 bytes
  Página: 4096 bytes

  mmap mapea 2 páginas (8192 bytes):
  ┌──────────────────────┬──────────────────────┐
  │     Página 0         │     Página 1         │
  │     4096 bytes       │     4096 bytes       │
  │                      │                      │
  │ [datos 0-4095]       │ [datos 4096-5999]    │
  │                      │ [ceros 6000-8191]    │
  └──────────────────────┴──────────────────────┘
                                  ↑
                           Relleno con ceros
                           (no en el archivo)
```

Los bytes entre el final del archivo y el final de la última página
se rellenan con ceros. Puedes leerlos, pero **escribir más allá del
tamaño del archivo no lo agranda**:

```c
struct stat sb;
fstat(fd, &sb);  // sb.st_size = 6000

char *data = mmap(NULL, sb.st_size, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, 0);

data[5999] = 'X';  // ✓ dentro del archivo
data[6000] = 'Y';  // ✗ más allá del archivo — escribe al padding
                    //   pero NO modifica el archivo
data[8192] = 'Z';  // ✗ fuera del mapeo — SIGSEGV (o SIGBUS)
```

### SIGBUS: acceso más allá del archivo

Si el archivo se trunca mientras está mapeado (otro proceso lo reduce),
acceder a páginas que ya no existen genera `SIGBUS`:

```c
// Proceso A:
char *data = mmap(NULL, 1000000, PROT_READ, MAP_SHARED, fd, 0);

// Proceso B:
ftruncate(fd, 100);  // trunca a 100 bytes

// Proceso A:
char c = data[50000];  // SIGBUS — esa parte del archivo ya no existe
```

### Agrandar un archivo mapeado

`mmap` no agranda el archivo. Para escribir más allá del tamaño actual,
primero debes agrandar con `ftruncate`:

```c
int fd = open("data.bin", O_RDWR | O_CREAT, 0644);

// Establecer tamaño del archivo
ftruncate(fd, desired_size);

// Ahora mapear
char *data = mmap(NULL, desired_size, PROT_READ | PROT_WRITE,
                  MAP_SHARED, fd, 0);
```

### mremap: redimensionar un mapeo (Linux)

```c
#define _GNU_SOURCE
#include <sys/mman.h>

void *mremap(void *old_address, size_t old_size,
             size_t new_size, int flags);

// Ejemplo: crecer un mapeo
data = mremap(data, old_size, new_size, MREMAP_MAYMOVE);
if (data == MAP_FAILED) {
    perror("mremap");
}
```

`MREMAP_MAYMOVE` permite al kernel mover el mapeo a otra dirección
virtual si no puede expandirlo in-place. Todos los punteros anteriores
al mapeo quedan **inválidos** después de mremap con movimiento.

---

## 10. Errores comunes

### Error 1: comparar retorno con NULL en lugar de MAP_FAILED

```c
// MAL: MAP_FAILED es (void*)-1, no NULL
void *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
if (ptr == NULL) {  // ¡NUNCA detecta el error!
    perror("mmap");
}

// BIEN:
if (ptr == MAP_FAILED) {
    perror("mmap");
    return -1;
}
```

### Error 2: mapear con PROT_WRITE+MAP_SHARED sobre fd O_RDONLY

```c
// MAL: el fd solo permite lectura
int fd = open("data.txt", O_RDONLY);
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
// mmap falla con EACCES

// BIEN: usar O_RDWR si necesitas escribir con MAP_SHARED
int fd = open("data.txt", O_RDWR);
void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
```

> Recuerda: `MAP_PRIVATE` con `PROT_WRITE` funciona con `O_RDONLY`
> porque las escrituras van a una copia privada, no al archivo.

### Error 3: olvidar ftruncate antes de escribir en un archivo nuevo

```c
// MAL: archivo nuevo tiene tamaño 0
int fd = open("new.dat", O_RDWR | O_CREAT | O_TRUNC, 0644);
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
// mmap puede retornar MAP_FAILED (no puedes mapear 0 bytes)
// O mapea pero escribir genera SIGBUS

// BIEN: establecer tamaño primero
int fd = open("new.dat", O_RDWR | O_CREAT | O_TRUNC, 0644);
ftruncate(fd, 4096);  // ahora el archivo tiene 4096 bytes
void *ptr = mmap(NULL, 4096, PROT_READ | PROT_WRITE,
                 MAP_SHARED, fd, 0);
```

### Error 4: offset no alineado a página

```c
// MAL: offset 1000 no es múltiplo de 4096
void *ptr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 1000);
// mmap falla con EINVAL

// BIEN: alinear a página y ajustar puntero
long page = sysconf(_SC_PAGESIZE);
off_t aligned = (1000 / page) * page;    // 0
size_t adjust = 1000 - aligned;           // 1000

void *base = mmap(NULL, size + adjust, PROT_READ,
                  MAP_SHARED, fd, aligned);
char *data = (char *)base + adjust;  // apunta al byte 1000
```

### Error 5: asumir que msync no es necesario antes de crash

```c
// MAL: confiar en que MAP_SHARED escribe inmediatamente
data[0] = 'X';
// Si el sistema se cae AQUÍ, el dato puede perderse
// (estaba en page cache pero no en disco)

// BIEN: para datos críticos, sincronizar explícitamente
data[0] = 'X';
msync(data, page_size, MS_SYNC);
// Ahora el dato está garantizado en disco
```

Para datos no críticos (caches, archivos temporales), no necesitas
`msync` — el kernel los escribirá eventualmente y los preserva en
un shutdown limpio.

---

## 11. Cheatsheet

```
  MAPEAR / DESMAPEAR
  ──────────────────────────────────────────────────────────
  mmap(NULL, len, prot, flags, fd, off)  crear mapeo
  munmap(ptr, len)                       eliminar mapeo
  Retorno de mmap: MAP_FAILED en error (NO es NULL)
  fd se puede close() después de mmap

  PROTECCIÓN
  ──────────────────────────────────────────────────────────
  PROT_READ                 lectura
  PROT_WRITE                escritura
  PROT_EXEC                 ejecución
  PROT_NONE                 sin acceso
  mprotect(ptr, len, prot)  cambiar protección

  FLAGS PRINCIPALES
  ──────────────────────────────────────────────────────────
  MAP_SHARED                visible entre procesos,
                            escribe al archivo
  MAP_PRIVATE               copia privada (CoW),
                            NO modifica el archivo

  SINCRONIZACIÓN
  ──────────────────────────────────────────────────────────
  msync(ptr, len, MS_SYNC)  sincrónico (bloquea)
  msync(ptr, len, MS_ASYNC) asincrónico (inicia writeback)
  MS_INVALIDATE              invalida otras copias

  HINTS AL KERNEL
  ──────────────────────────────────────────────────────────
  madvise(ptr, len, MADV_SEQUENTIAL)   lectura secuencial
  madvise(ptr, len, MADV_RANDOM)       acceso aleatorio
  madvise(ptr, len, MADV_WILLNEED)     precargar
  madvise(ptr, len, MADV_DONTNEED)     liberar

  REDIMENSIONAR (LINUX)
  ──────────────────────────────────────────────────────────
  mremap(ptr, old_sz, new_sz, MREMAP_MAYMOVE)

  REGLAS DE ALINEACIÓN
  ──────────────────────────────────────────────────────────
  offset debe ser múltiplo de sysconf(_SC_PAGESIZE)
  El mapeo se redondea hacia arriba al siguiente múltiplo
  de página
  Bytes entre st_size y fin de página se rellenan con ceros

  FLUJO TÍPICO
  ──────────────────────────────────────────────────────────
  fd = open(path, O_RDWR);
  fstat(fd, &sb);
  ptr = mmap(NULL, sb.st_size, PROT_READ|PROT_WRITE,
             MAP_SHARED, fd, 0);
  close(fd);
  // ... usar ptr como array ...
  msync(ptr, sb.st_size, MS_SYNC);  // si datos críticos
  munmap(ptr, sb.st_size);
```

---

## 12. Ejercicios

### Ejercicio 1: grep con mmap

Escribe un programa que busque una cadena en un archivo usando `mmap`
en lugar de `read()`. Debe imprimir cada línea que contiene la cadena,
con su número de línea.

```c
// Esqueleto:
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Uso: %s patrón archivo\n", argv[0]);
        return 1;
    }

    const char *pattern = argv[1];
    const char *filepath = argv[2];

    // 1. Abrir y obtener tamaño
    // 2. mmap con PROT_READ + MAP_SHARED
    // 3. Cerrar fd
    // 4. Recorrer el mapeo línea por línea
    //    - Encontrar cada '\n' con memchr
    //    - Buscar el patrón en cada línea con memmem
    //    - Si lo encuentra, imprimir número de línea y contenido
    // 5. munmap

    return 0;
}
```

Requisitos:
- Usar `madvise(MADV_SEQUENTIAL)` para optimizar la lectura.
- Manejar archivos que no terminan en `\n`.
- Manejar archivos vacíos (no mapear tamaño 0).

**Pregunta de reflexión**: ¿qué ventaja tiene esta implementación sobre
una que usa `read()` con un buffer de 4096 bytes? ¿Y qué desventaja
tiene si el archivo es de 100 GiB en un sistema con 4 GiB de RAM?
¿El programa falla? ¿Por qué sí o por qué no?

---

### Ejercicio 2: editor hexadecimal in-place

Escribe un programa que mapee un archivo con `MAP_SHARED` y permita
modificar bytes individuales por offset:

```c
// Uso: ./hexedit archivo offset valor
// Ejemplo: ./hexedit data.bin 0x100 0x41  (pone 'A' en offset 256)
```

```c
// Esqueleto:
int main(int argc, char *argv[]) {
    // Parsear argumentos: archivo, offset (hex), valor (hex)
    // Abrir con O_RDWR
    // fstat para verificar que offset < st_size
    // mmap con PROT_READ | PROT_WRITE, MAP_SHARED
    // Mostrar: "Antes: 0xNN"
    // Modificar el byte
    // Mostrar: "Después: 0xNN"
    // msync + munmap
}
```

Requisitos:
- Aceptar offsets y valores en hexadecimal (`strtoul` con base 16).
- Verificar que el offset está dentro del archivo.
- Mostrar el valor anterior y nuevo.
- Usar `msync(MS_SYNC)` para garantizar la escritura.

**Pregunta de reflexión**: este programa modifica el archivo
**directamente** sin crear una copia temporal. ¿Qué implicaciones tiene
esto para la recuperación ante errores? Si el programa falla a mitad de
una serie de modificaciones, ¿hay forma de "revertir"? ¿Cómo manejaría
este problema una base de datos real?

---

### Ejercicio 3: copia de archivo con mmap

Escribe un programa que copie un archivo usando `mmap` en lugar de
`read()`/`write()`:

```c
// Esqueleto:
int copy_with_mmap(const char *src_path, const char *dst_path) {
    // 1. Abrir source con O_RDONLY
    // 2. fstat para obtener tamaño
    // 3. Crear destino con O_RDWR | O_CREAT | O_TRUNC
    // 4. ftruncate destino al tamaño del source
    // 5. mmap source con PROT_READ + MAP_PRIVATE
    // 6. mmap destino con PROT_READ | PROT_WRITE + MAP_SHARED
    // 7. memcpy(dst_map, src_map, size)
    // 8. msync destino
    // 9. munmap ambos, close ambos

    return 0;
}
```

Requisitos:
- Manejar archivos vacíos (no mapear tamaño 0, solo crear vacío).
- Preservar permisos del archivo original (usar `fstat` + `fchmod`).
- Medir el tiempo de ejecución y comparar con una copia usando
  `read()`/`write()` con buffer de 64KiB.

**Pregunta de reflexión**: en la copia con `mmap`, ¿cuántas veces se
copian los datos? Piensa en el flujo: disco → page cache → espacio
virtual del source → `memcpy` → espacio virtual del destino → page
cache del destino → disco. ¿Es realmente "zero-copy"? ¿Qué syscall
lograría una copia verdaderamente zero-copy entre dos archivos?

---
