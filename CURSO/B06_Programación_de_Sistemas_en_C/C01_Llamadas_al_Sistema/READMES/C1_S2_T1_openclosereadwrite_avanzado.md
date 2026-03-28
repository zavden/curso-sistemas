# open/close/read/write Avanzado

## Índice

1. [File descriptors: la tabla de descriptores](#1-file-descriptors-la-tabla-de-descriptores)
2. [open y openat: flags avanzados](#2-open-y-openat-flags-avanzados)
3. [O_CLOEXEC: cerrar fd al hacer exec](#3-o_cloexec-cerrar-fd-al-hacer-exec)
4. [O_NONBLOCK: I/O sin bloquear](#4-o_nonblock-io-sin-bloquear)
5. [read: short reads y EOF](#5-read-short-reads-y-eof)
6. [write: short writes y EPIPE](#6-write-short-writes-y-epipe)
7. [Wrappers robustos: readn y writen](#7-wrappers-robustos-readn-y-writen)
8. [lseek: acceso aleatorio](#8-lseek-acceso-aleatorio)
9. [pread y pwrite: I/O posicional atómico](#9-pread-y-pwrite-io-posicional-atómico)
10. [close: sutilezas y trampas](#10-close-sutilezas-y-trampas)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. File descriptors: la tabla de descriptores

Antes de profundizar en las syscalls, es necesario entender la estructura que hay detrás. Un **file descriptor** (fd) es un entero no negativo que el kernel asigna cuando abres un archivo, socket, pipe, o cualquier otro recurso de I/O.

### La tabla de tres niveles

```
 Proceso (task_struct)          Kernel
┌─────────────────────┐    ┌─────────────────────────────────┐
│ fd table            │    │ Open File Table (global)        │
│ ┌────┬────────────┐ │    │ ┌───┬──────────────────────┐    │
│ │ 0  │ ──────────►├─┼───►│ │   │ offset: 0            │    │
│ │ 1  │ ──────────►├─┼──┐ │ │   │ flags: O_RDONLY      │    │
│ │ 2  │ ──────────►├─┼─┐│ │ │   │ inode ───────────►┌──┴──┐│
│ │ 3  │ ──────────►├─┼┐││ │ └───┘                   │inode││
│ │ 4  │ (vacío)    │ │││││ │                         │/etc/││
│ └────┴────────────┘ │││││ │ ┌───┬──────────────────┐│hosts││
│                     ││└┼┼►│ │   │ offset: 0        ││     ││
│                     ││ ││ │ │   │ flags: O_WRONLY   │└─────┘│
│                     ││ ││ │ │   │ inode ──────────► ...    │
│                     ││ └┼►│ └───┘                         │
│                     ││  │ │                               │
│                     │└──┼►│ (fd 1 y 2 pueden apuntar     │
│                     │   │ │  al mismo file description    │
│                     │   │ │  si stdout y stderr van al    │
│                     │   │ │  mismo terminal)              │
└─────────────────────┘   │ └───────────────────────────────┘
                          │
                          └─ Cada entry tiene su propio offset
```

Tres niveles:

| Nivel | Estructura | Alcance | Contiene |
|-------|-----------|---------|----------|
| fd table | Array por proceso | Por proceso | Índice → open file description |
| Open file description | Struct del kernel | Global (compartible) | Offset, flags, puntero a inode |
| inode | Struct del kernel | Global (por archivo) | Metadatos, bloques de datos |

Puntos clave:
- Los fds 0, 1, 2 están preocupados por convención: stdin, stdout, stderr.
- `open` retorna el **menor fd disponible**. Si cierras fd 0 y abres algo, obtienes fd 0.
- Después de `fork()`, padre e hijo **comparten** las open file descriptions (mismo offset).
- `dup()/dup2()` crean otro fd que apunta a la **misma** open file description (mismo offset, mismos flags).

### Ver los fd de un proceso

```bash
# Los fd de tu shell actual
ls -la /proc/self/fd/
# 0 -> /dev/pts/0  (stdin)
# 1 -> /dev/pts/0  (stdout)
# 2 -> /dev/pts/0  (stderr)
# 3 -> /proc/self/fd (opendir para ls)

# Los fd de un proceso específico
ls -la /proc/$(pidof nginx)/fd/
```

---

## 2. open y openat: flags avanzados

La firma completa de `open`:

```c
#include <fcntl.h>

int open(const char *pathname, int flags);              // abrir existente
int open(const char *pathname, int flags, mode_t mode); // crear nuevo
```

`flags` es un OR de constantes que controlan el comportamiento:

### Flags de acceso (obligatorio, exactamente uno)

| Flag | Valor | Significado |
|------|:-----:|-------------|
| `O_RDONLY` | 0 | Solo lectura |
| `O_WRONLY` | 1 | Solo escritura |
| `O_RDWR` | 2 | Lectura y escritura |

### Flags de creación (opcionales, para crear archivos)

| Flag | Significado |
|------|-------------|
| `O_CREAT` | Crear el archivo si no existe (requiere argumento `mode`) |
| `O_EXCL` | Con `O_CREAT`: fallar si el archivo ya existe (creación atómica) |
| `O_TRUNC` | Si el archivo existe, truncar a tamaño 0 |
| `O_DIRECTORY` | Fallar si no es un directorio |
| `O_TMPFILE` | Crear archivo temporal sin nombre (Linux 3.11+) |

### Flags de comportamiento (opcionales)

| Flag | Significado |
|------|-------------|
| `O_APPEND` | Escrituras siempre al final (atómico) |
| `O_NONBLOCK` | No bloquear en read/write |
| `O_CLOEXEC` | Cerrar el fd al hacer exec |
| `O_SYNC` | Escrituras síncronas (esperar a disco) |
| `O_DSYNC` | Como O_SYNC pero solo para datos (no metadatos) |
| `O_NOFOLLOW` | No seguir symlinks |
| `O_NOCTTY` | No asignar terminal de control |

### Ejemplos prácticos

```c
// Abrir para lectura
int fd = open("/etc/hosts", O_RDONLY);

// Crear o truncar para escritura (patrón "escribir archivo nuevo")
int fd = open("/tmp/output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);

// Crear exclusivamente (fallar si ya existe — útil para lockfiles)
int fd = open("/var/run/app.pid", O_WRONLY | O_CREAT | O_EXCL, 0644);
if (fd == -1 && errno == EEXIST) {
    fprintf(stderr, "Another instance is running\n");
}

// Append: log files (múltiples procesos escriben sin pisarse)
int fd = open("/var/log/app.log", O_WRONLY | O_CREAT | O_APPEND, 0644);

// Seguro: no seguir symlinks, cerrar al exec
int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
```

### mode: permisos del archivo nuevo

El argumento `mode` se aplica solo cuando `O_CREAT` está presente. Se combina con el **umask** del proceso:

```c
// Permisos solicitados: 0666 (rw-rw-rw-)
// umask típico:          0022
// Permisos reales:       0644 (rw-r--r--)

int fd = open("/tmp/file", O_WRONLY | O_CREAT | O_TRUNC, 0666);
```

```
 permisos = mode & ~umask
 0666 & ~0022 = 0666 & 0755 = 0644
```

### openat: versión moderna relativa a directorio

```c
#include <fcntl.h>

int openat(int dirfd, const char *pathname, int flags, ...);
```

`openat` abre un archivo relativo a un directory fd, en vez de relativo al CWD. Resuelve race conditions cuando el CWD puede cambiar entre threads:

```c
// Abrir /etc/hosts de forma segura
int dirfd = open("/etc", O_RDONLY | O_DIRECTORY);
int fd = openat(dirfd, "hosts", O_RDONLY);
close(dirfd);

// AT_FDCWD: usa el CWD actual (equivale a open normal)
int fd = openat(AT_FDCWD, "relative/path", O_RDONLY);
```

En la práctica, `open()` de glibc invoca la syscall `openat(AT_FDCWD, ...)` internamente.

---

## 3. O_CLOEXEC: cerrar fd al hacer exec

Cuando un proceso ejecuta `exec` (reemplaza su programa por otro), los file descriptors **permanecen abiertos** por defecto. Esto puede filtrar fds sensibles al nuevo programa:

```
 Sin O_CLOEXEC:

 Proceso padre                   Después de exec
┌──────────────────┐          ┌──────────────────┐
│ fd 0: stdin      │          │ fd 0: stdin      │
│ fd 1: stdout     │          │ fd 1: stdout     │
│ fd 2: stderr     │          │ fd 2: stderr     │
│ fd 3: /etc/shadow│  exec ─► │ fd 3: /etc/shadow│ ← ¡filtrado!
│ fd 4: socket DB  │          │ fd 4: socket DB  │ ← ¡filtrado!
└──────────────────┘          └──────────────────┘
 El nuevo programa hereda los fds abiertos
```

`O_CLOEXEC` marca el fd para cerrarse automáticamente cuando se ejecute `exec`:

```c
// El fd se cierra automáticamente si hacemos exec
int fd = open("/etc/shadow", O_RDONLY | O_CLOEXEC);
```

```
 Con O_CLOEXEC:

 Proceso padre                   Después de exec
┌──────────────────┐          ┌──────────────────┐
│ fd 0: stdin      │          │ fd 0: stdin      │
│ fd 1: stdout     │          │ fd 1: stdout     │
│ fd 2: stderr     │          │ fd 2: stderr     │
│ fd 3: /etc/shadow│  exec ─► │ fd 3: (cerrado)  │ ✓
│    (CLOEXEC)     │          │                  │
└──────────────────┘          └──────────────────┘
```

### Regla: siempre usar O_CLOEXEC

Es una buena práctica usar `O_CLOEXEC` en **todos** los `open` a menos que específicamente necesites que el fd sobreviva al exec. Previene:
- Filtración de file descriptors a procesos hijos.
- File descriptor exhaustion en servidores que hacen fork+exec.
- Vulnerabilidades de seguridad (acceso a archivos/sockets del padre).

```c
// ✓ Recomendado
int fd = open(path, O_RDONLY | O_CLOEXEC);

// También disponible para otros recursos
int sockfd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
int pipefd[2];
pipe2(pipefd, O_CLOEXEC);
int epfd = epoll_create1(EPOLL_CLOEXEC);
```

### Configurar CLOEXEC después de open (método legacy)

```c
#include <fcntl.h>

int fd = open(path, O_RDONLY);
// Race condition: entre open y fcntl, otro thread puede hacer fork+exec

int flags = fcntl(fd, F_GETFD);
fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
```

Este método tiene una **race condition**: entre `open` y `fcntl`, otro thread puede hacer `fork`+`exec` y heredar el fd. Por eso `O_CLOEXEC` es preferible — es atómico.

---

## 4. O_NONBLOCK: I/O sin bloquear

Por defecto, `read` en un pipe o socket sin datos se **bloquea** (el proceso se duerme hasta que haya datos). Con `O_NONBLOCK`, `read` retorna inmediatamente con `EAGAIN` si no hay datos:

```
 Blocking (default):                 Non-blocking (O_NONBLOCK):

 read(fd, buf, n)                    read(fd, buf, n)
    │                                   │
    ▼                                   ▼
 ¿Hay datos?                        ¿Hay datos?
    │                                   │
  No │  Sí                           No │  Sí
    │   │                              │   │
    ▼   ▼                              ▼   ▼
 Dormir  Retornar              Retornar -1  Retornar
 hasta   datos                 errno=EAGAIN datos
 que                           (no espera)
 lleguen
```

### Abrir con O_NONBLOCK

```c
int fd = open("/dev/ttyS0", O_RDWR | O_NONBLOCK);

char buf[256];
ssize_t n = read(fd, buf, sizeof(buf));
if (n == -1) {
    if (errno == EAGAIN) {
        // No hay datos ahora — no es un error
        // Hacer otra cosa y reintentar después
    } else {
        perror("read");
    }
}
```

### Configurar O_NONBLOCK después de open

```c
// Obtener flags actuales
int flags = fcntl(fd, F_GETFL);

// Activar O_NONBLOCK
fcntl(fd, F_SETFL, flags | O_NONBLOCK);

// Desactivar O_NONBLOCK
fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
```

### Cuándo usar O_NONBLOCK

| Escenario | ¿O_NONBLOCK? |
|-----------|:------------:|
| Leer un archivo regular | No (read de archivos nunca bloquea, siempre hay datos o EOF) |
| Leer de un pipe/socket | Sí, si necesitas multiplexar varios fds (con poll/epoll) |
| Leer de un terminal/serial | Sí, si no quieres que el programa se cuelgue |
| Escribir a un pipe/socket | Sí, si el buffer puede llenarse y no quieres bloquear |
| connect() a un servidor remoto | Sí, si quieres timeout control |

En archivos regulares, `O_NONBLOCK` no tiene efecto práctico — `read` siempre retorna inmediatamente (con datos o EOF). `O_NONBLOCK` es relevante para pipes, sockets, terminales y dispositivos.

---

## 5. read: short reads y EOF

```c
#include <unistd.h>

ssize_t read(int fd, void *buf, size_t count);
```

`read` **puede retornar menos bytes de los solicitados**. Esto se llama **short read** y es comportamiento normal, no un error.

### Valores de retorno de read

| Retorno | Significado |
|:-------:|-------------|
| > 0 | Bytes leídos (puede ser < count — short read) |
| 0 | EOF — no hay más datos (el escritor cerró el fd o fin de archivo) |
| -1 | Error (ver errno) |

### Cuándo ocurre un short read

```c
char buf[4096];
ssize_t n = read(fd, buf, 4096);
// n podría ser 4096, o 1000, o 1, o 0...
```

| Causa | Ejemplo |
|-------|---------|
| Quedan menos bytes que count | Archivo de 100 bytes, lees 4096 → n = 100 |
| Pipe/socket: solo llegaron N bytes | El escritor envió 500 bytes → n = 500 |
| Terminal: el usuario presionó Enter | Escribió "hello\n" → n = 6 |
| Señal interrumpió (con SA_RESTART) | El kernel reintenta pero retorna lo que tenía |
| Buffer del kernel parcialmente lleno | Red con MTU pequeño → fragmentos |

### El loop de lectura correcto

Si necesitas exactamente N bytes (por ejemplo, un header binario de protocolo):

```c
// Leer exactamente 'count' bytes o fallar
ssize_t readn(int fd, void *buf, size_t count) {
    size_t total = 0;
    char *p = buf;

    while (total < count) {
        ssize_t n = read(fd, p + total, count - total);
        if (n == -1) {
            if (errno == EINTR)
                continue;   // señal interrumpió — reintentar
            return -1;       // error real
        }
        if (n == 0)
            break;           // EOF — no hay más datos
        total += n;
    }
    return total;
}
```

### EOF no es un error

```c
// Leer hasta EOF (patrón de lectura de archivo completo)
char buf[4096];
ssize_t n;
while ((n = read(fd, buf, sizeof(buf))) > 0) {
    // procesar buf[0..n-1]
}
if (n == -1) {
    perror("read");  // error real
}
// n == 0: llegamos a EOF — todo bien
```

---

## 6. write: short writes y EPIPE

```c
#include <unistd.h>

ssize_t write(int fd, const void *buf, size_t count);
```

Al igual que `read`, `write` puede escribir **menos bytes de los solicitados** (short write).

### Valores de retorno de write

| Retorno | Significado |
|:-------:|-------------|
| > 0 | Bytes escritos (puede ser < count — short write) |
| 0 | Nada escrito (raro para archivos, posible en sockets non-blocking) |
| -1 | Error (ver errno) |

### Cuándo ocurre un short write

| Causa | Ejemplo |
|-------|---------|
| Disco casi lleno | Solo caben 500 bytes más → n = 500 |
| Pipe/socket: buffer del kernel lleno | Buffer de 64K, escribes 100K → n = 65536 |
| Señal interrumpió | n bytes escritos antes de la señal |
| Límite de tamaño de archivo (RLIMIT_FSIZE) | Superado el límite → SIGXFSZ |

### EPIPE: el lector cerró

Cuando escribes a un pipe o socket cuyo otro extremo fue cerrado:

```c
ssize_t n = write(pipe_fd, data, len);
// n == -1, errno == EPIPE
// Además: el proceso recibe SIGPIPE (acción default: terminar)
```

```
 Escritor                    Pipe              Lector
┌──────────┐           ┌──────────┐        ┌──────────┐
│write() ──┼──────────►│ ████████ │        │ CERRADO  │
│          │           │ (buffer) │───X───►│          │
│ EPIPE ◄──┼───────────│          │        │ close()  │
│ SIGPIPE  │           └──────────┘        └──────────┘
└──────────┘
```

Si no quieres que SIGPIPE mate tu programa:

```c
// Opción 1: ignorar SIGPIPE globalmente
signal(SIGPIPE, SIG_IGN);
// Ahora write retorna -1/EPIPE sin señal

// Opción 2: usar MSG_NOSIGNAL en send (solo sockets)
send(sockfd, data, len, MSG_NOSIGNAL);
```

### O_APPEND: escrituras atómicas al final

Con `O_APPEND`, cada `write` se posiciona automáticamente al final del archivo **antes** de escribir. Esto es atómico — múltiples procesos pueden escribir sin pisarse:

```c
// Log file seguro para múltiples procesos
int fd = open("/var/log/app.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
write(fd, "proceso A escribió\n", 19);
// Siempre añade al final, incluso si otro proceso escribió mientras tanto
```

Sin `O_APPEND`, necesitarías `lseek(fd, 0, SEEK_END)` antes de cada `write`, pero eso no es atómico — otro proceso puede escribir entre el seek y el write.

---

## 7. Wrappers robustos: readn y writen

Estos wrappers manejan short reads/writes y EINTR. Son la base de cualquier programa robusto:

### readn: leer exactamente N bytes

```c
#include <unistd.h>
#include <errno.h>

// Lee exactamente 'count' bytes.
// Retorna: count en éxito, < count si EOF prematuro, -1 en error.
ssize_t readn(int fd, void *buf, size_t count) {
    size_t nleft = count;
    char *ptr = buf;

    while (nleft > 0) {
        ssize_t nread = read(fd, ptr, nleft);
        if (nread == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (nread == 0)
            break;            // EOF
        nleft -= nread;
        ptr += nread;
    }
    return count - nleft;     // bytes realmente leídos
}
```

### writen: escribir exactamente N bytes

```c
#include <unistd.h>
#include <errno.h>

// Escribe exactamente 'count' bytes.
// Retorna: count en éxito, -1 en error.
ssize_t writen(int fd, const void *buf, size_t count) {
    size_t nleft = count;
    const char *ptr = buf;

    while (nleft > 0) {
        ssize_t nwritten = write(fd, ptr, nleft);
        if (nwritten == -1) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        nleft -= nwritten;
        ptr += nwritten;
    }
    return count;
}
```

### Uso

```c
// Leer exactamente un header de 16 bytes
struct header hdr;
ssize_t n = readn(fd, &hdr, sizeof(hdr));
if (n != sizeof(hdr)) {
    if (n == -1) perror("readn");
    else fprintf(stderr, "truncated header: %zd/%zu bytes\n", n, sizeof(hdr));
    return -1;
}

// Escribir un mensaje completo
ssize_t n = writen(fd, message, msg_len);
if (n == -1) {
    perror("writen");
    return -1;
}
```

Estos wrappers (o variantes similares) se usan en prácticamente todo programa serio de red y I/O.

---

## 8. lseek: acceso aleatorio

```c
#include <unistd.h>

off_t lseek(int fd, off_t offset, int whence);
```

`lseek` mueve la posición de lectura/escritura del fd sin leer ni escribir datos:

### whence: punto de referencia

| whence | Significado | Posición resultante |
|--------|------------|---------------------|
| `SEEK_SET` | Desde el inicio del archivo | `offset` |
| `SEEK_CUR` | Desde la posición actual | `posición_actual + offset` |
| `SEEK_END` | Desde el final del archivo | `tamaño_archivo + offset` |

### Ejemplos

```c
// Ir al inicio del archivo
lseek(fd, 0, SEEK_SET);

// Ir al byte 100
lseek(fd, 100, SEEK_SET);

// Avanzar 50 bytes desde la posición actual
lseek(fd, 50, SEEK_CUR);

// Ir al final del archivo
lseek(fd, 0, SEEK_END);

// Retroceder 10 bytes desde el final
lseek(fd, -10, SEEK_END);

// Obtener la posición actual sin moverse
off_t pos = lseek(fd, 0, SEEK_CUR);

// Obtener el tamaño del archivo
off_t size = lseek(fd, 0, SEEK_END);
lseek(fd, 0, SEEK_SET);  // volver al inicio
```

### Sparse files: seek más allá del final

Si haces `lseek` más allá del final del archivo y escribes, se crea un **hole** — un espacio que lógicamente contiene ceros pero no ocupa espacio en disco:

```c
int fd = open("/tmp/sparse", O_WRONLY | O_CREAT | O_TRUNC, 0644);

// Escribir 1 byte al inicio
write(fd, "A", 1);

// Saltar a 1 GiB
lseek(fd, 1L * 1024 * 1024 * 1024, SEEK_SET);

// Escribir 1 byte
write(fd, "B", 1);

close(fd);
```

```bash
ls -lh /tmp/sparse
# -rw-r--r-- 1 user user 1.0G ...   ← tamaño lógico: 1 GiB

du -h /tmp/sparse
# 8.0K /tmp/sparse                   ← tamaño real: 8 KiB
```

Esto es lo mismo que hacen `qemu-img create` y `truncate` para crear archivos sparse.

### Dónde lseek no funciona

`lseek` no funciona en pipes, sockets ni terminales (retorna `ESPIPE`):

```c
off_t pos = lseek(STDIN_FILENO, 0, SEEK_CUR);
// Si stdin es un pipe: pos == -1, errno == ESPIPE
// Si stdin es un archivo (redirección): pos >= 0
```

---

## 9. pread y pwrite: I/O posicional atómico

```c
#include <unistd.h>

ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
```

`pread`/`pwrite` leen/escriben en una posición específica **sin modificar el offset del fd**. Son equivalentes a `lseek` + `read`/`write` pero **atómicos**:

```c
// Sin pread: dos operaciones, no atómico
lseek(fd, offset, SEEK_SET);   // otro thread puede hacer lseek aquí
read(fd, buf, count);           // lee desde posición incorrecta

// Con pread: una operación, atómico
pread(fd, buf, count, offset);  // lee desde offset sin cambiar posición
```

### Por qué importa la atomicidad

Si múltiples threads comparten un fd (que comparten la misma open file description y por tanto el mismo offset), `lseek` + `read` es una race condition:

```
 Thread A                           Thread B
 lseek(fd, 100, SEEK_SET)
                                    lseek(fd, 200, SEEK_SET)  ← ¡cambia el offset!
 read(fd, buf, 50)                  ← lee desde 200, no desde 100
```

Con `pread`:

```
 Thread A                           Thread B
 pread(fd, buf, 50, 100)            pread(fd, buf, 50, 200)
 ← lee desde 100 correctamente     ← lee desde 200 correctamente
 (el offset del fd no cambia)       (no hay interferencia)
```

### Uso típico

```c
// Leer un bloque específico de un archivo de base de datos
#define BLOCK_SIZE 4096

char block[BLOCK_SIZE];
ssize_t n = pread(db_fd, block, BLOCK_SIZE, block_num * BLOCK_SIZE);
if (n != BLOCK_SIZE) {
    // handle error or short read
}

// Escribir un bloque sin afectar la posición de otros threads
pwrite(db_fd, block, BLOCK_SIZE, block_num * BLOCK_SIZE);
```

---

## 10. close: sutilezas y trampas

```c
#include <unistd.h>

int close(int fd);
```

`close` parece simple, pero tiene comportamientos sutiles:

### El fd se libera inmediatamente

Después de `close(fd)`, el número de fd queda disponible para reutilización. El próximo `open` puede retornar el mismo número:

```c
int fd1 = open("file1", O_RDONLY);  // fd1 = 3
close(fd1);
int fd2 = open("file2", O_RDONLY);  // fd2 = 3 (reutilizado)
```

Si guardas `fd1` y lo usas después del `close`, operarías sobre `file2` sin saberlo. Este bug es tan sutil que tiene nombre: **use-after-close** (análogo a use-after-free).

### Cerrar un fd no vacía los buffers de stdio

```c
FILE *fp = fopen("output.txt", "w");
fprintf(fp, "datos importantes");
close(fileno(fp));   // ✗ cierra el fd PERO stdio tiene datos en buffer
// Los datos en el buffer de stdio se pierden

fclose(fp);          // ✓ flush el buffer de stdio Y cierra el fd
```

Regla: si abriste con `fopen`, cierra con `fclose`. Si abriste con `open`, cierra con `close`.

### EINTR en close: no reintentar en Linux

Como se explicó en T02: en Linux, `close` libera el fd **incluso cuando retorna EINTR**. Reintentar puede cerrar un fd que otro thread ya reutilizó:

```c
// ✓ CORRECTO en Linux
if (close(fd) == -1 && errno != EINTR) {
    perror("close");
}
fd = -1;  // marcar como cerrado para evitar use-after-close

// ✗ PELIGROSO
while (close(fd) == -1 && errno == EINTR);  // ¡puede cerrar fd equivocado!
```

### Doble close: un bug silencioso

```c
close(fd);
// ... más código ...
close(fd);  // ✗ el fd puede haber sido reutilizado por otro open
```

El segundo `close` puede cerrar un fd completamente diferente que otro hilo abrió entre medio. Prevención: asignar -1 al fd después de cerrarlo:

```c
close(fd);
fd = -1;

// Después, si alguien intenta cerrar de nuevo:
if (fd != -1) close(fd);
```

### ¿Puede close perder datos?

Sí. Si el filesystem tiene escrituras pendientes y ocurre un error al flushearlas, `close` retorna -1 con `EIO` o `ENOSPC`. Los datos que creías escritos podrían haberse perdido:

```c
// Para datos críticos, fsync antes de close
if (fsync(fd) == -1) {
    perror("fsync");  // los datos no llegaron a disco
}
if (close(fd) == -1) {
    perror("close");  // último check
}
```

---

## 11. Errores comunes

### Error 1: no manejar short reads en protocolos binarios

```c
struct packet pkt;
read(sock_fd, &pkt, sizeof(pkt));
// Si read retorna 4 de 32 bytes, pkt tiene basura en los últimos 28 bytes
process_packet(&pkt);  // comportamiento indefinido
```

**Solución**: usar `readn` para leer exactamente `sizeof(pkt)` bytes.

### Error 2: olvidar O_CLOEXEC y filtrar fds

```c
int fd = open("/etc/shadow", O_RDONLY);  // sin O_CLOEXEC
// fork + exec: el proceso hijo hereda acceso a /etc/shadow
```

**Solución**: `O_CLOEXEC` en todos los `open` por defecto.

### Error 3: usar fd después de close (use-after-close)

```c
int log_fd = open("app.log", O_WRONLY | O_APPEND | O_CREAT, 0644);
// ... en algún lugar del código ...
close(log_fd);
// ... mucho después, otro open reutiliza el mismo número ...
int sock = socket(AF_INET, SOCK_STREAM, 0);  // podría ser el mismo número
// ... y el código de logging sigue escribiendo a log_fd ...
write(log_fd, "log message\n", 12);  // escribe al socket, no al log
```

**Solución**: asignar -1 después de close. En código complejo, usar structs que encapsulen el fd con un flag de "abierto/cerrado".

### Error 4: O_CREAT sin argumento mode

```c
// ✗ Comportamiento indefinido — mode es basura del stack
int fd = open("/tmp/file", O_WRONLY | O_CREAT | O_TRUNC);

// ✓ CORRECTO — siempre especificar mode con O_CREAT
int fd = open("/tmp/file", O_WRONLY | O_CREAT | O_TRUNC, 0644);
```

El compilador no avisa de este error porque `open` es variádico. El archivo se crea con permisos aleatorios.

### Error 5: ignorar el retorno de write sin short write handling

```c
// ✗ Si write escribe 500 de 1000 bytes, los otros 500 se pierden silenciosamente
write(fd, buf, 1000);

// ✓ Manejar short writes
ssize_t n = writen(fd, buf, 1000);
if (n == -1) {
    perror("writen");
}
```

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│           open/close/read/write — REFERENCIA                    │
├──────────────────────────────────────────────────────────────────┤
│ OPEN FLAGS                                                       │
│   O_RDONLY | O_WRONLY | O_RDWR      Acceso (exactamente uno)    │
│   O_CREAT                          Crear si no existe (+mode)   │
│   O_EXCL                           Fallar si existe (atómico)   │
│   O_TRUNC                          Truncar a 0 si existe       │
│   O_APPEND                         Escribir siempre al final   │
│   O_CLOEXEC                        Cerrar al exec (¡siempre!)  │
│   O_NONBLOCK                       No bloquear en I/O          │
│   O_SYNC                           Escrituras síncronas        │
│   O_NOFOLLOW                       No seguir symlinks          │
├──────────────────────────────────────────────────────────────────┤
│ READ                                                             │
│   > 0        Bytes leídos (puede ser < count: short read)       │
│   0          EOF (fin de archivo / escritor cerró pipe)          │
│   -1         Error (EINTR → reintentar, EAGAIN → sin datos)     │
├──────────────────────────────────────────────────────────────────┤
│ WRITE                                                            │
│   > 0        Bytes escritos (puede ser < count: short write)    │
│   -1         Error (EINTR → reintentar, EPIPE → lector cerró)  │
├──────────────────────────────────────────────────────────────────┤
│ LSEEK                                                            │
│   lseek(fd, 0, SEEK_SET)    Inicio del archivo                  │
│   lseek(fd, n, SEEK_CUR)    Avanzar n desde posición actual    │
│   lseek(fd, 0, SEEK_END)    Final del archivo                   │
│   lseek(fd, 0, SEEK_CUR)    Obtener posición actual            │
├──────────────────────────────────────────────────────────────────┤
│ PREAD / PWRITE                                                   │
│   pread(fd, buf, n, offset)   Leer sin cambiar offset (atómico)│
│   pwrite(fd, buf, n, offset)  Escribir sin cambiar offset      │
├──────────────────────────────────────────────────────────────────┤
│ CLOSE                                                            │
│   close(fd); fd = -1;          Siempre invalidar después       │
│   No reintentar en EINTR        (Linux: fd ya se liberó)        │
│   fsync(fd) antes de close      Para datos críticos             │
│   fclose(fp) para FILE*         (no close(fileno(fp)))          │
├──────────────────────────────────────────────────────────────────┤
│ PATRONES                                                         │
│   while((n=read(fd,b,sz))>0)   Leer hasta EOF                  │
│   readn(fd, buf, exact)         Leer exactamente N bytes        │
│   writen(fd, buf, exact)        Escribir exactamente N bytes    │
└──────────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: Copiar archivo con manejo robusto

Escribe un programa `mycopy src dst` que copie un archivo usando solo syscalls (`open`, `read`, `write`, `close`):

1. Abrir `src` con `O_RDONLY | O_CLOEXEC`.
2. Abrir `dst` con `O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC`, modo `0644`.
3. Copiar en un loop con buffer de 4096 bytes usando `readn`/`writen` (o al menos manejar short reads/writes y EINTR).
4. Verificar el retorno de **todas** las syscalls, incluyendo `close`.
5. Hacer `fsync` del destino antes de cerrarlo.

Prueba con:
- Un archivo de texto normal.
- `/dev/urandom` como fuente (leer 1 MiB con un límite) para verificar que maneja datos binarios.
- Un archivo que no existe como fuente (debe dar error legible).
- Un destino en directorio sin permisos (debe dar error legible).

```bash
strace -e trace=openat,read,write,close,fsync ./mycopy /etc/hosts /tmp/hosts_copy
```

> **Pregunta de reflexión**: si lees de un pipe en vez de un archivo, `readn` podría bloquear indefinidamente esperando datos que nunca llegan. ¿Cómo modificarías `readn` para soportar un timeout? (Pista: `poll` antes de `read`.)

### Ejercicio 2: Observar O_CLOEXEC con fork+exec

1. Escribe un programa padre que:
   - Abra un archivo **sin** O_CLOEXEC y otro **con** O_CLOEXEC.
   - Imprima ambos fd numbers.
   - Haga `fork()` + `execvp()` para ejecutar un programa hijo.
2. El programa hijo (otro binario) debe:
   - Recibir los dos fd numbers como argumentos.
   - Intentar `read` de cada uno e imprimir si tuvo éxito o `EBADF`.
3. Compila ambos y ejecuta el padre.
4. ¿Cuál fd es accesible en el hijo y cuál da `EBADF`?

```c
// padre.c
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    int fd_leak = open("/etc/hosts", O_RDONLY);            // SIN cloexec
    int fd_safe = open("/etc/hosts", O_RDONLY | O_CLOEXEC); // CON cloexec

    printf("fd_leak=%d, fd_safe=%d\n", fd_leak, fd_safe);

    char arg1[16], arg2[16];
    snprintf(arg1, sizeof(arg1), "%d", fd_leak);
    snprintf(arg2, sizeof(arg2), "%d", fd_safe);

    execl("./hijo", "hijo", arg1, arg2, NULL);
    perror("execl");
    return 1;
}
```

> **Pregunta de reflexión**: en un servidor que hace `fork` + `exec` para cada request, ¿cuántos fds se filtrarían después de 10,000 requests si no usas `O_CLOEXEC`? ¿Podrían agotarse los fds del sistema?

### Ejercicio 3: pread para lectura paralela

Escribe un programa que lea un archivo grande dividiéndolo en 4 chunks, uno por thread, usando `pread`:

1. Abrir el archivo con `open`.
2. Obtener su tamaño con `lseek(fd, 0, SEEK_END)`.
3. Dividir en 4 partes iguales.
4. Crear 4 threads, cada uno con su offset y longitud.
5. Cada thread usa `pread` para leer su chunk (sin interferir con los otros).
6. Contar cuántas veces aparece la letra 'e' en cada chunk.
7. Sumar los conteos de todos los threads.

Datos clave:
- Todos los threads usan el **mismo fd** — pread no modifica el offset.
- No necesitas mutex para el fd (pread es thread-safe).
- Necesitas mutex (o variables locales) para el conteo.

```bash
# Probar con un archivo grande
./parallel_count /usr/share/dict/words
# Thread 0: 12345 'e's in bytes 0-50000
# Thread 1: 11234 'e's in bytes 50001-100000
# ...
# Total: 45678 'e's
```

> **Pregunta de reflexión**: ¿qué pasaría si usaras `lseek` + `read` en vez de `pread`? ¿Necesitarías un mutex alrededor de cada lectura? ¿Cómo afectaría eso al paralelismo?
