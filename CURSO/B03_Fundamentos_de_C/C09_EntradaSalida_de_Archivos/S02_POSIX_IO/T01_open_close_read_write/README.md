# T01 — open, close, read, write

## File descriptors

En POSIX, un archivo abierto se representa con un **file
descriptor** (fd): un entero pequeño no negativo:

```c
// Tres file descriptors están abiertos al inicio:
// 0 — stdin  (STDIN_FILENO)
// 1 — stdout (STDOUT_FILENO)
// 2 — stderr (STDERR_FILENO)

// Cada proceso tiene una tabla de file descriptors:
// fd 0 → stdin
// fd 1 → stdout
// fd 2 → stderr
// fd 3 → primer archivo que abras
// fd 4 → siguiente archivo
// ...

// Los fds son índices en la tabla del kernel.
// Cada fd apunta a una estructura con: offset, flags, inode, etc.
```

```c
// Relación con stdio:
// FILE* es un wrapper de alto nivel sobre un fd:
// fopen("file", "r") internamente llama a open("file", O_RDONLY)
// fprintf(f, ...) internamente llama a write(fd, ...)
// fclose(f) internamente llama a close(fd)

// Obtener el fd de un FILE*:
#include <stdio.h>
FILE *f = fopen("data.txt", "r");
int fd = fileno(f);    // fd subyacente

// Crear un FILE* a partir de un fd:
int fd = open("data.txt", O_RDONLY);
FILE *f = fdopen(fd, "r");
```

## open — Abrir archivos

```c
#include <fcntl.h>      // open, O_* flags
#include <sys/stat.h>   // mode_t, permisos
#include <unistd.h>     // close, read, write

int open(const char *path, int flags);
int open(const char *path, int flags, mode_t mode);
// Retorna fd si éxito, -1 si error (errno se establece).

// flags obligatorios (uno de estos):
// O_RDONLY — solo lectura
// O_WRONLY — solo escritura
// O_RDWR   — lectura y escritura

// flags opcionales (se combinan con |):
// O_CREAT    — crear si no existe (requiere mode)
// O_TRUNC    — truncar a tamaño 0 si existe
// O_APPEND   — escribir siempre al final
// O_EXCL     — con O_CREAT: fallar si ya existe
// O_CLOEXEC  — cerrar en exec() (recomendado)
```

```c
// Ejemplos:

// Abrir para lectura:
int fd = open("data.txt", O_RDONLY);
if (fd == -1) {
    perror("open");
    return -1;
}

// Crear para escritura (truncar si existe):
int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
// 0644 = rw-r--r-- (owner read/write, group/others read)

// Crear para escritura (fallar si existe):
int fd = open("new.txt", O_WRONLY | O_CREAT | O_EXCL, 0644);
if (fd == -1 && errno == EEXIST) {
    printf("File already exists\n");
}

// Abrir para append:
int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
```

### Permisos (mode)

```c
// mode se requiere con O_CREAT. Formato octal:
// 0644 = rw-r--r--
// 0755 = rwxr-xr-x
// 0600 = rw-------

// Los permisos reales son: mode & ~umask
// Con umask = 022:
//   open("f", O_CREAT, 0666) → permisos reales = 0644

// Constantes simbólicas:
// S_IRUSR (0400) — owner read
// S_IWUSR (0200) — owner write
// S_IXUSR (0100) — owner execute
// S_IRGRP (0040) — group read
// S_IROTH (0004) — others read

int fd = open("file", O_CREAT | O_WRONLY,
              S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);  // 0644
```

## close — Cerrar file descriptors

```c
int close(int fd);
// Retorna 0 si éxito, -1 si error.

close(fd);

// SIEMPRE cerrar los fds:
// - El proceso tiene un límite (ulimit -n, típicamente 1024)
// - No cerrar → file descriptor leak
// - En servidores de larga duración, esto causa problemas
```

## write — Escribir datos

```c
ssize_t write(int fd, const void *buf, size_t count);
// Escribe hasta count bytes de buf al fd.
// Retorna el número de bytes escritos, o -1 si error.

const char *msg = "Hello, World!\n";
ssize_t n = write(STDOUT_FILENO, msg, strlen(msg));
if (n == -1) {
    perror("write");
}
```

```c
// write puede escribir MENOS bytes de los pedidos (short write):
// Esto ocurre con pipes, sockets, señales, disco lleno.

// Patrón: write completo con loop:
ssize_t write_all(int fd, const void *buf, size_t count) {
    const char *p = buf;
    size_t remaining = count;

    while (remaining > 0) {
        ssize_t n = write(fd, p, remaining);
        if (n == -1) {
            if (errno == EINTR) continue;    // interrupted by signal
            return -1;
        }
        p += n;
        remaining -= n;
    }
    return count;
}
```

## read — Leer datos

```c
ssize_t read(int fd, void *buf, size_t count);
// Lee hasta count bytes del fd a buf.
// Retorna:
//   > 0: número de bytes leídos
//   = 0: fin de archivo (EOF)
//   -1:  error (errno se establece)

char buf[1024];
ssize_t n = read(fd, buf, sizeof(buf));
if (n == -1) {
    perror("read");
} else if (n == 0) {
    printf("EOF\n");
} else {
    // buf contiene n bytes (NO terminado en '\0')
    // Si necesitas un string: buf[n] = '\0';
    write(STDOUT_FILENO, buf, n);
}
```

```c
// read puede leer MENOS bytes de los pedidos (short read):
// Esto es NORMAL — read retorna lo que está disponible.

// Patrón: read completo con loop:
ssize_t read_all(int fd, void *buf, size_t count) {
    char *p = buf;
    size_t remaining = count;

    while (remaining > 0) {
        ssize_t n = read(fd, p, remaining);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;    // EOF
        p += n;
        remaining -= n;
    }
    return count - remaining;
}
```

## Ejemplo: copiar archivo con POSIX I/O

```c
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int copy_file(const char *src, const char *dst) {
    int result = -1;
    int fd_in = -1, fd_out = -1;

    fd_in = open(src, O_RDONLY);
    if (fd_in == -1) { perror(src); goto cleanup; }

    fd_out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) { perror(dst); goto cleanup; }

    char buf[4096];
    ssize_t n;
    while ((n = read(fd_in, buf, sizeof(buf))) > 0) {
        if (write_all(fd_out, buf, n) == -1) {
            perror("write");
            goto cleanup;
        }
    }
    if (n == -1) { perror("read"); goto cleanup; }

    result = 0;

cleanup:
    if (fd_in != -1) close(fd_in);
    if (fd_out != -1) close(fd_out);
    return result;
}
```

## errno y manejo de errores

```c
#include <errno.h>
#include <string.h>

int fd = open("noexiste.txt", O_RDONLY);
if (fd == -1) {
    // errno contiene el código de error:
    printf("Error %d: %s\n", errno, strerror(errno));

    // Errores comunes de open:
    // ENOENT — archivo no existe
    // EACCES — sin permisos
    // EMFILE — límite de fds del proceso
    // ENFILE — límite de fds del sistema
    // EISDIR — es un directorio (y no se puede abrir para escritura)
    // EEXIST — ya existe (con O_EXCL)

    // perror es el shortcut:
    perror("open");    // "open: No such file or directory"
}
```

---

## Ejercicios

### Ejercicio 1 — Echo con read/write

```c
// Implementar un programa que lea de stdin con read()
// y escriba a stdout con write(), en un loop.
// Usar un buffer de 1024 bytes.
// Terminar cuando read retorne 0 (EOF, Ctrl+D).
// NO usar ninguna función de stdio.h.
```

### Ejercicio 2 — File copy POSIX

```c
// Implementar copy_file(src, dst) usando solo open/read/write/close.
// Usar write_all para manejar short writes.
// Si dst existe, pedir confirmación antes de sobrescribir.
// Preservar los permisos del archivo original (usar stat/fstat).
```

### Ejercicio 3 — Contar bytes y líneas

```c
// Implementar wc simplificado usando solo POSIX I/O:
// - Leer el archivo con read() en bloques de 4096.
// - Contar bytes, líneas ('\n') y palabras (transiciones de whitespace).
// - Imprimir los resultados con write() (sin printf).
// Comparar la salida con wc(1).
```
