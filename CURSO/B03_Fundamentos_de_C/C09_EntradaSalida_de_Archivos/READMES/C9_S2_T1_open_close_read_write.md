# T01 — open, close, read, write

## Erratas detectadas

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `labs/read_demo.c` | 38 | `write(STDOUT_FILENO, sep, 21)` — la cadena `"\n--- end of chunk ---\n"` tiene **22** bytes, no 21. Se omite el `'\n'` final del separador, causando que la siguiente línea se pegue al separador. | Usar `22` o mejor `strlen(sep)` para evitar contar manualmente. |

---

## 1. File descriptors — El identificador universal de POSIX

En POSIX, cada recurso abierto (archivo, pipe, socket, terminal) se identifica con un **file descriptor** (fd): un entero pequeño no negativo.

```c
// El kernel mantiene una tabla de fds por proceso:
// fd 0 → stdin   (STDIN_FILENO)
// fd 1 → stdout  (STDOUT_FILENO)
// fd 2 → stderr  (STDERR_FILENO)
// fd 3 → primer recurso que abras
// fd 4 → siguiente recurso
// ...

// Cada fd es un ÍNDICE en la tabla del proceso.
// Internamente, cada entrada apunta a una estructura del kernel
// que contiene: offset actual, flags, referencia al inode, etc.
```

### Relación entre FILE* y fd

`FILE*` de `<stdio.h>` es un wrapper de alto nivel sobre un fd:

```c
// stdio usa POSIX internamente:
//   fopen("f", "r")   → open("f", O_RDONLY)
//   fprintf(f, ...)    → escribe al buffer → write(fd, ...)
//   fclose(f)          → fflush + close(fd)

#include <stdio.h>

// Obtener el fd subyacente de un FILE*:
FILE *f = fopen("data.txt", "r");
int fd = fileno(f);     // fd del FILE*

// Crear un FILE* a partir de un fd existente:
int fd2 = open("data.txt", O_RDONLY);
FILE *f2 = fdopen(fd2, "r");
// CUIDADO: después de fdopen, fclose(f2) también cierra fd2.
// No hacer close(fd2) Y fclose(f2) — double close.
```

**Regla clave**: los fds siempre se asignan como el **número libre más bajo**. Si cierras fd=3 y abres otro recurso, el nuevo recibirá fd=3.

---

## 2. open — Abrir y crear archivos

```c
#include <fcntl.h>      // open, O_* flags
#include <sys/stat.h>   // mode_t, constantes de permisos

// Dos formas (misma función, C no tiene overloading real —
// open usa varargs internamente):
int open(const char *path, int flags);
int open(const char *path, int flags, mode_t mode);
// Retorna: fd si éxito, -1 si error (errno se establece).
```

### Flags obligatorios (exactamente uno)

| Flag | Valor | Descripción |
|------|-------|-------------|
| `O_RDONLY` | 0 | Solo lectura |
| `O_WRONLY` | 1 | Solo escritura |
| `O_RDWR` | 2 | Lectura y escritura |

### Flags opcionales (se combinan con `|`)

| Flag | Efecto | Notas |
|------|--------|-------|
| `O_CREAT` | Crear si no existe | **Requiere** tercer argumento `mode` |
| `O_TRUNC` | Truncar a 0 si existe | Destruye contenido previo |
| `O_APPEND` | Escribir siempre al final | Atómico incluso con múltiples procesos |
| `O_EXCL` | Falla si ya existe (con `O_CREAT`) | `errno = EEXIST`. Útil para lock files |
| `O_CLOEXEC` | Cerrar en `exec()` | Recomendado siempre. Evita fd leaks a procesos hijos |

### Combinaciones comunes

```c
// Equivalente a fopen("f", "r"):
int fd = open("f", O_RDONLY);

// Equivalente a fopen("f", "w"):
int fd = open("f", O_WRONLY | O_CREAT | O_TRUNC, 0644);

// Equivalente a fopen("f", "a"):
int fd = open("f", O_WRONLY | O_CREAT | O_APPEND, 0644);

// Equivalente a fopen("f", "wx") (C11 exclusivo):
int fd = open("f", O_WRONLY | O_CREAT | O_EXCL, 0644);

// Producción (con O_CLOEXEC siempre):
int fd = open("f", O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
```

### Permisos (mode)

El tercer argumento es obligatorio cuando se usa `O_CREAT`. Se expresa en octal:

```c
// Formato: 0UGO (User, Group, Others)
// 0644 = rw-r--r--   (archivos normales)
// 0755 = rwxr-xr-x   (ejecutables, directorios)
// 0600 = rw-------   (archivos privados: claves, configs)

// Constantes simbólicas (se combinan con |):
// S_IRUSR (0400) — user read
// S_IWUSR (0200) — user write
// S_IXUSR (0100) — user execute
// S_IRGRP (0040) — group read
// S_IROTH (0004) — others read

int fd = open("secret.key", O_WRONLY | O_CREAT | O_TRUNC,
              S_IRUSR | S_IWUSR);  // 0600
```

**umask modifica los permisos reales**: `permisos_reales = mode & ~umask`

```c
// Si umask = 022:
//   open("f", O_CREAT, 0666) → 0666 & ~022 = 0644
//   open("f", O_CREAT, 0777) → 0777 & ~022 = 0755
//   open("f", O_CREAT, 0644) → 0644 & ~022 = 0644 (sin cambio)
```

**Error frecuente**: olvidar `mode` con `O_CREAT`. Sin él, `mode` toma un valor basura del stack, y el archivo se crea con permisos impredecibles.

---

## 3. close — Liberar file descriptors

```c
#include <unistd.h>

int close(int fd);
// Retorna: 0 si éxito, -1 si error.
```

### Por qué siempre cerrar

```c
// Cada proceso tiene un límite de fds abiertos:
// $ ulimit -n
// 1024   (típico, configurable)

// No cerrar fds → fd leak:
// - En un servidor que procesa 1000 requests/segundo,
//   un fd leak por request agota el límite en ~1 segundo.
// - open() empieza a fallar con errno = EMFILE.

// Patrón: siempre cerrar en el camino inverso de apertura
int fd = open("data.txt", O_RDONLY);
if (fd == -1) { perror("open"); return 1; }
// ... usar fd ...
close(fd);
```

### Trampas de close

```c
// 1. close(-1) es un error (retorna -1, errno = EBADF)
//    → Verificar que el fd sea válido antes de cerrar

// 2. close(fd) y luego usar fd es undefined behavior
//    → Mejor poner fd = -1 después de cerrar

// 3. Double close: si otro thread abrió un fd con el mismo número,
//    cerras el fd equivocado
//    → Nunca cerrar dos veces. Usar fd = -1 como sentinel.

close(fd);
fd = -1;    // Sentinel: "este fd ya no es válido"
```

### close en NFS

En sistemas NFS, `close()` puede fallar con errores de escritura retardada. Es el único caso donde verificar el retorno de `close()` tiene impacto real.

---

## 4. write — Escribir datos

```c
#include <unistd.h>

ssize_t write(int fd, const void *buf, size_t count);
// Escribe hasta count bytes de buf al fd.
// Retorna:
//   > 0: número de bytes escritos (puede ser < count)
//   -1:  error (errno se establece)
//   0:   raro, pero posible (count era 0)
```

### Short writes — El problema real

`write()` puede escribir **menos** bytes de los pedidos. Esto ocurre con:
- Pipes y sockets (capacidad limitada del buffer del kernel)
- Señales que interrumpen la syscall (`EINTR`)
- Disco lleno (escribe lo que puede, luego falla)

```c
// ¡INCORRECTO! — Asume que write() siempre escribe todo:
write(fd, buf, len);    // Puede escribir solo parte

// CORRECTO — Loop para escritura completa:
ssize_t write_all(int fd, const void *buf, size_t count) {
    const char *p = buf;
    size_t remaining = count;

    while (remaining > 0) {
        ssize_t n = write(fd, p, remaining);
        if (n == -1) {
            if (errno == EINTR) continue;  // Señal interrumpió, reintentar
            return -1;                     // Error real
        }
        p += n;
        remaining -= n;
    }
    return (ssize_t)count;
}
```

**¿Cuándo NO necesitas `write_all`?** Para archivos regulares en discos locales, `write()` casi siempre escribe todo de una vez. Pero "casi siempre" no es "siempre" — el loop es la práctica segura.

### write a stdout

```c
// write() es una syscall — no tiene buffering ni formato:
const char *msg = "Hello from write()!\n";
ssize_t n = write(STDOUT_FILENO, msg, strlen(msg));
// Ventaja sobre printf: async-signal-safe (seguro en signal handlers)
```

---

## 5. read — Leer datos

```c
#include <unistd.h>

ssize_t read(int fd, void *buf, size_t count);
// Lee hasta count bytes del fd a buf.
// Retorna:
//   > 0: número de bytes leídos (puede ser < count)
//   = 0: fin de archivo (EOF)
//   -1:  error (errno se establece)
```

### Diferencias críticas con fgets/fread

| Aspecto | `read()` | `fgets()` / `fread()` |
|---------|----------|----------------------|
| Terminador `'\0'` | **NO** lo agrega | `fgets` siempre agrega `'\0'` |
| Buffering | Sin buffer (syscall directa) | Buffer en espacio de usuario |
| Retorno | Bytes leídos / 0=EOF / -1 | Elementos (fread) o puntero/NULL (fgets) |
| Líneas | No entiende de líneas | `fgets` lee hasta `'\n'` |

### Short reads — Son NORMALES

`read()` puede retornar menos bytes de los pedidos, incluso para archivos regulares (dependiendo del tamaño del buffer y cuánto quede por leer). No es un error:

```c
char buf[4096];
ssize_t n = read(fd, buf, sizeof(buf));
// Si el archivo tiene 100 bytes, n = 100 (no 4096)
// Si el archivo tiene 5000 bytes:
//   Primera lectura: n = 4096
//   Segunda lectura: n = 904
//   Tercera lectura: n = 0 (EOF)
```

### Patrón: leer archivo completo con loop

```c
// Este es el patrón universal para leer datos con POSIX I/O:
char buf[4096];
ssize_t n;

while ((n = read(fd, buf, sizeof(buf))) > 0) {
    // Procesar n bytes en buf
    // buf NO está terminado en '\0'
}

if (n == -1) {
    perror("read");    // Error real
}
// Si n == 0: llegamos a EOF normalmente
```

### read_all — Leer exactamente N bytes

```c
// Útil cuando SABES cuántos bytes esperas (ej: structs binarios):
ssize_t read_all(int fd, void *buf, size_t count) {
    char *p = buf;
    size_t remaining = count;

    while (remaining > 0) {
        ssize_t n = read(fd, p, remaining);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) break;    // EOF prematuro
        p += n;
        remaining -= n;
    }
    return (ssize_t)(count - remaining);
}

// Diferencia con el loop simple:
// - Loop simple: lee TODO lo que haya hasta EOF
// - read_all: lee EXACTAMENTE count bytes (o lo que haya si EOF antes)
```

---

## 6. errno y manejo de errores

Todas las syscalls POSIX usan el mismo patrón: retornan -1 en error y establecen `errno`.

```c
#include <errno.h>    // errno, códigos de error (ENOENT, EACCES, ...)
#include <string.h>   // strerror()
#include <stdio.h>    // perror()

int fd = open("noexiste.txt", O_RDONLY);
if (fd == -1) {
    // Tres formas de reportar el error:

    // 1. perror — más simple, imprime a stderr:
    perror("open");
    // Salida: "open: No such file or directory"

    // 2. strerror — obtener string del error:
    fprintf(stderr, "Error: %s\n", strerror(errno));

    // 3. errno directo — para lógica condicional:
    if (errno == ENOENT) {
        // Archivo no existe — quizás crearlo
    } else if (errno == EACCES) {
        // Sin permisos — reportar al usuario
    }
}
```

### Errores comunes por syscall

| errno | Nombre | `open` | `read` | `write` | `close` |
|-------|--------|--------|--------|---------|---------|
| 2 | `ENOENT` | Archivo no existe | — | — | — |
| 13 | `EACCES` | Sin permisos | — | — | — |
| 17 | `EEXIST` | Ya existe (O_EXCL) | — | — | — |
| 9 | `EBADF` | — | fd inválido | fd inválido | fd inválido |
| 24 | `EMFILE` | Límite de fds del proceso | — | — | — |
| 23 | `ENFILE` | Límite de fds del sistema | — | — | — |
| 28 | `ENOSPC` | — | — | Disco lleno | — |
| 4 | `EINTR` | — | Señal interrumpió | Señal interrumpió | — |
| 21 | `EISDIR` | Es directorio (escritura) | Es directorio | — | — |

### Trampa de errno

```c
// errno SOLO es válido inmediatamente después de una syscall que falló.
// Cualquier otra llamada puede modificarlo:

int fd = open("bad.txt", O_RDONLY);     // fd = -1, errno = ENOENT
printf("Checking...\n");                 // printf PUEDE modificar errno
if (errno == ENOENT) { /* ¿seguro? */ } // NO — errno puede haber cambiado

// CORRECTO: guardar errno antes de hacer otra cosa:
int fd = open("bad.txt", O_RDONLY);
if (fd == -1) {
    int saved_errno = errno;   // Guardar INMEDIATAMENTE
    printf("Error occurred\n");
    // Ahora usar saved_errno de forma segura
}
```

---

## 7. /proc/self/fd — Inspeccionar file descriptors

Linux expone los fds del proceso actual en `/proc/self/fd/` como symlinks:

```bash
$ ls -l /proc/self/fd
lrwx------ 1 user group 64 ... 0 -> /dev/pts/0    # stdin
lrwx------ 1 user group 64 ... 1 -> /dev/pts/0    # stdout
lrwx------ 1 user group 64 ... 2 -> /dev/pts/0    # stderr
lr-x------ 1 user group 64 ... 3 -> /path/to/file # archivo abierto
```

### Información que revela

```bash
# El tipo de permiso del symlink indica el modo de apertura:
# lr-x------  → O_RDONLY (lectura)
# l-wx------  → O_WRONLY (escritura)
# lrwx------  → O_RDWR (lectura+escritura)

# Para un PID específico:
ls -l /proc/1234/fd

# Contar fds abiertos de un proceso:
ls /proc/1234/fd | wc -l

# Diagnóstico de fd leaks:
# Si un servidor tiene cientos de fds apuntando al mismo recurso,
# probablemente hay un leak.
```

### Reutilización de fds

```c
int fd1 = open("a.txt", O_RDONLY);   // fd = 3
int fd2 = open("b.txt", O_RDONLY);   // fd = 4
close(fd1);                           // fd 3 libre
int fd3 = open("c.txt", O_RDONLY);   // fd = 3 (reutilizado)

// Regla del kernel: open() siempre asigna el fd libre más bajo.
// Por eso después de cerrar fd=3, el siguiente open() obtiene 3 de nuevo.
```

### /proc/self/fd desde C

```c
// Forma programática con system():
system("ls -l /proc/self/fd");

// Forma más robusta con opendir/readdir:
#include <dirent.h>
DIR *d = opendir("/proc/self/fd");
struct dirent *entry;
while ((entry = readdir(d)) != NULL) {
    if (entry->d_name[0] != '.') {
        char link[256];
        char path[280];
        snprintf(path, sizeof(path), "/proc/self/fd/%s", entry->d_name);
        ssize_t len = readlink(path, link, sizeof(link) - 1);
        if (len != -1) {
            link[len] = '\0';
            printf("fd %s -> %s\n", entry->d_name, link);
        }
    }
}
closedir(d);
```

---

## 8. Patrones seguros de uso

### goto cleanup — Manejo de múltiples recursos

```c
int process_files(const char *in, const char *out) {
    int result = -1;
    int fd_in = -1, fd_out = -1;

    fd_in = open(in, O_RDONLY);
    if (fd_in == -1) { perror(in); goto cleanup; }

    fd_out = open(out, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) { perror(out); goto cleanup; }

    // ... operar con fd_in y fd_out ...
    result = 0;

cleanup:
    if (fd_in  != -1) close(fd_in);
    if (fd_out != -1) close(fd_out);
    return result;
}

// ¿Por qué goto y no returns múltiples?
// Con 3+ recursos, los returns múltiples generan código duplicado
// para cerrar cada subconjunto de recursos abiertos.
// goto cleanup centraliza la limpieza en un solo lugar.
```

### Copy file — Patrón completo

```c
int copy_file(const char *src, const char *dst) {
    int result = -1;
    int fd_in = -1, fd_out = -1;

    fd_in = open(src, O_RDONLY);
    if (fd_in == -1) { perror(src); goto cleanup; }

    fd_out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) { perror(dst); goto cleanup; }

    char buf[4096];   // 4096 = tamaño típico de página/bloque
    ssize_t n;
    while ((n = read(fd_in, buf, sizeof(buf))) > 0) {
        const char *p = buf;
        size_t remaining = (size_t)n;
        while (remaining > 0) {
            ssize_t w = write(fd_out, p, remaining);
            if (w == -1) {
                if (errno == EINTR) continue;
                perror("write");
                goto cleanup;
            }
            p += w;
            remaining -= (size_t)w;
        }
    }
    if (n == -1) { perror("read"); goto cleanup; }
    result = 0;

cleanup:
    if (fd_in  != -1) close(fd_in);
    if (fd_out != -1) close(fd_out);
    return result;
}
```

### Peligro: mezclar stdio y POSIX I/O en el mismo fd

```c
// PELIGROSO:
FILE *f = fopen("data.txt", "w");
int fd = fileno(f);

fprintf(f, "Via stdio\n");    // En buffer de stdio
write(fd, "Via write\n", 10); // Directo al kernel

// El "Via write" puede aparecer ANTES que "Via stdio"
// porque stdio tiene buffering y write() no.

// SOLUCIÓN: fflush() antes de usar write() en el mismo fd:
fprintf(f, "Via stdio\n");
fflush(f);                     // Forzar flush del buffer
write(fd, "Via write\n", 10);  // Ahora el orden es correcto
```

---

## 9. O_CLOEXEC, O_APPEND y flags avanzados

### O_CLOEXEC — Cerrar fd automáticamente en exec()

```c
// Sin O_CLOEXEC:
int fd = open("secret.conf", O_RDONLY);
// Si luego haces exec() para lanzar otro programa,
// ese programa HEREDA fd y puede leer secret.conf.

// Con O_CLOEXEC:
int fd = open("secret.conf", O_RDONLY | O_CLOEXEC);
// El fd se cierra automáticamente al hacer exec().
// El programa hijo NO puede acceder a secret.conf.

// RECOMENDACIÓN: usar O_CLOEXEC SIEMPRE, a menos que
// intencionalmente quieras pasar el fd al proceso hijo.
```

### O_APPEND — Escrituras atómicas al final

```c
int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);

// Cada write() con O_APPEND hace atómicamente:
//   1. Mover el offset al final del archivo
//   2. Escribir los datos
// Ambos pasos son UNA operación atómica del kernel.

// ¿Por qué importa?
// Sin O_APPEND, dos procesos escribiendo al mismo archivo:
//   Proceso A: lseek(fd, 0, SEEK_END); write(fd, "AAA", 3);
//   Proceso B: lseek(fd, 0, SEEK_END); write(fd, "BBB", 3);
//   Resultado posible: "AAABBB" o "BBBAAA" o "AABBBA" (¡datos mezclados!)

// Con O_APPEND, el kernel garantiza que los datos no se solapan.
// Los writes pueden llegar en cualquier orden, pero nunca se pisan.
```

### O_NONBLOCK — Lectura/escritura sin bloquear

```c
// Útil con pipes, sockets, FIFOs, terminales:
int fd = open("/dev/tty", O_RDONLY | O_NONBLOCK);

char buf[256];
ssize_t n = read(fd, buf, sizeof(buf));
if (n == -1 && errno == EAGAIN) {
    // No hay datos disponibles ahora — en lugar de bloquear,
    // read() retorna inmediatamente con EAGAIN.
    // El programa puede hacer otra cosa y reintentar después.
}
```

---

## 10. Comparación con Rust

| Concepto C | Rust | Diferencia clave |
|-----------|------|-----------------|
| `int fd` | `std::fs::File` (envuelve fd) | `File` es un tipo con ownership |
| `open()` retorna -1 | `File::open()` retorna `Result<File>` | Errores tipados, no errno global |
| `close(fd)` manual | `Drop` automático | RAII: el fd se cierra al salir del scope |
| `O_CREAT\|O_TRUNC` | `File::create()` | API builder: `OpenOptions::new().write(true).create(true)` |
| `write_all` loop manual | `file.write_all()` built-in | La librería estándar lo incluye |
| `read` loop hasta EOF | `file.read_to_end()` o `read_to_string()` | Una sola llamada lee todo |
| `errno` global mutable | `io::Error` por valor | Sin estado global compartido |
| goto cleanup | `?` operator + Drop | Los recursos se liberan automáticamente |

```rust
use std::fs;
use std::io::{self, Read, Write};

fn copy_file(src: &str, dst: &str) -> io::Result<()> {
    let mut input = fs::File::open(src)?;       // ? propaga el error
    let mut output = fs::File::create(dst)?;

    let mut buf = [0u8; 4096];
    loop {
        let n = input.read(&mut buf)?;
        if n == 0 { break; }                    // EOF
        output.write_all(&buf[..n])?;           // write_all incluido
    }
    Ok(())
    // Drop cierra ambos Files automáticamente
}

// O más simple:
fn copy_simple(src: &str, dst: &str) -> io::Result<u64> {
    fs::copy(src, dst)   // Una sola línea
}
```

**Ventaja fundamental de Rust**: no hay forma de usar un fd después de cerrarlo (use-after-close), olvidar cerrarlo (el `Drop` lo hace), ni tener un double-close. El sistema de ownership previene estas tres categorías de bugs a nivel de compilación.

---

## Ejercicios

### Ejercicio 1 — Echo puro con POSIX I/O

```c
// Implementa un programa que lea de STDIN_FILENO con read()
// y escriba a STDOUT_FILENO con write(), en un loop.
// Buffer de 1024 bytes.
// Terminar cuando read() retorne 0 (EOF = Ctrl+D).
// NO usar NINGUNA función de <stdio.h>.
// Compilar: gcc -std=c11 -Wall -Wextra echo_posix.c -o echo_posix
// Probar:   echo "hello world" | ./echo_posix
//           ./echo_posix < /etc/hostname
```

<details><summary>Predicción</summary>

El programa debería funcionar como `cat` sin argumentos:
- Lee de stdin en bloques de hasta 1024 bytes
- Escribe exactamente lo que leyó a stdout
- Con `echo "hello world" | ./echo_posix`: una iteración, 12 bytes (11 + '\n')
- Con archivo: lee en bloques hasta EOF
- Con terminal interactivo: lee línea por línea (el terminal envía al presionar Enter), Ctrl+D envía EOF

Sin `<stdio.h>`, no puedes usar `printf` para mensajes — solo `write()`.

</details>

### Ejercicio 2 — Número a string sin printf

```c
// Escribe una función int_to_str(int n, char *buf, size_t size)
// que convierta un entero a string decimal en buf.
// Luego escribe el resultado a STDOUT_FILENO con write().
//
// Maneja: números positivos, negativos, y 0.
// NO usar sprintf, snprintf, printf, ni ninguna función de <stdio.h>.
//
// Ejemplo:
//   char buf[32];
//   int_to_str(-42, buf, sizeof(buf));
//   write(STDOUT_FILENO, buf, strlen(buf));
//   // Salida: "-42"
```

<details><summary>Predicción</summary>

Estrategia:
1. Manejar caso especial `n == 0` → escribir '0' y retornar
2. Si `n < 0`, escribir '-' y trabajar con `-n` (cuidado con INT_MIN)
3. Extraer dígitos con `% 10` y `/ 10` — salen en orden inverso
4. Revertir los dígitos, o escribir desde el final del buffer hacia atrás

Para INT_MIN (-2147483648): `-n` desborda en complemento a 2. Solución: usar `unsigned int` o tratar el último dígito por separado.

La salida de `int_to_str(12345, buf, 32)` debería dejar "12345" en `buf` con `'\0'` al final.

</details>

### Ejercicio 3 — Copy file con verificación

```c
// Implementa copy_file(src, dst) usando open/read/write/close.
// Requisitos:
//   1. Usar write_all con loop para short writes
//   2. Buffer de 4096 bytes
//   3. Si dst ya existe, reportar "File exists" y no sobrescribir
//      (usar O_EXCL)
//   4. Verificar que la copia es correcta comparando tamaños
//      con lseek (o stat)
//
// Probar:
//   ./copy_file /etc/hostname copia.txt
//   ./copy_file /etc/hostname copia.txt   (debe fallar: ya existe)
//   diff /etc/hostname copia.txt          (deben ser idénticos)
```

<details><summary>Predicción</summary>

Flujo:
1. `open(src, O_RDONLY)` → si falla, perror y salir
2. `open(dst, O_WRONLY | O_CREAT | O_EXCL, 0644)` → si falla con `EEXIST`, imprimir mensaje; otros errores → perror
3. Loop: `read(fd_in, buf, 4096)` → `write_all(fd_out, buf, n)` → hasta `read` retorne 0
4. Cerrar ambos fds

La segunda ejecución falla porque `O_EXCL` detecta que `copia.txt` ya existe → `errno = EEXIST` (17).

`diff` no debería mostrar diferencias.

</details>

### Ejercicio 4 — Observar partial reads

```c
// Lee un archivo usando un buffer de 8 bytes (intencionalmente pequeño).
// En cada iteración, imprime:
//   [iter N] read() = M bytes: "contenido"
// (puedes usar printf solo para las líneas de diagnóstico)
//
// Crear un archivo de prueba:
//   echo "ABCDEFGHIJKLMNOPQRSTUVWXYZ" > test.txt   (27 bytes con \n)
//
// Preguntas:
//   1. ¿Cuántas iteraciones serán necesarias?
//   2. ¿La última iteración leerá 8 bytes?
//   3. ¿Qué pasa si el buffer fuera de 1 byte?
```

<details><summary>Predicción</summary>

Con 27 bytes y buffer de 8:
- Iteración 1: `read()` = 8 bytes → "ABCDEFGH"
- Iteración 2: `read()` = 8 bytes → "IJKLMNOP"
- Iteración 3: `read()` = 8 bytes → "QRSTUVWX"
- Iteración 4: `read()` = 3 bytes → "YZ\n" ← partial read
- Iteración 5: `read()` = 0 → EOF

Total: 4 iteraciones con datos (32/8 no exacto, 27/8 = 3.375, así que 3 completas + 1 parcial).

La última iteración lee solo 3 bytes (lo que queda). Con buffer de 1 byte: 27 iteraciones (una por byte) — funciona pero es extremadamente ineficiente.

</details>

### Ejercicio 5 — Inspeccionar /proc/self/fd

```c
// Escribe un programa que:
// 1. Muestre los fds iniciales (debería ver 0, 1, 2)
// 2. Abra 3 archivos (a.txt, b.txt, c.txt)
// 3. Muestre los fds (debería ver 0-5)
// 4. Cierre b.txt (fd=4)
// 5. Abra d.txt — ¿qué fd recibe?
// 6. Muestre los fds finales
//
// Para mostrar fds, listar /proc/self/fd con opendir/readdir
// (no system() — practicar la API de directorios).
//
// Pregunta: ¿por qué d.txt recibe fd=4 y no fd=6?
```

<details><summary>Predicción</summary>

Secuencia de fds:
- Inicial: 0, 1, 2 (+ el fd del directorio abierto por opendir)
- Después de abrir a/b/c: 0, 1, 2, 3(a), 4(b), 5(c)
- Después de cerrar b (fd=4): 0, 1, 2, 3(a), -, 5(c)
- Después de abrir d: 0, 1, 2, 3(a), 4(d), 5(c) ← d.txt obtiene fd=4

d.txt recibe fd=4 porque el kernel siempre asigna el fd libre más bajo. Al cerrar fd=4 (b.txt), ese número queda disponible y es el más bajo libre.

Nota sobre opendir: `opendir()` también abre un fd internamente (para leer el directorio). Ese fd aparecerá en la lista.

</details>

### Ejercicio 6 — Creación exclusiva con O_EXCL

```c
// Implementa un "lock file" simple:
// 1. Intentar open("app.lock", O_WRONLY | O_CREAT | O_EXCL, 0644)
// 2. Si éxito: escribir el PID (getpid()) al archivo, hacer trabajo, eliminar lock
// 3. Si falla con EEXIST: leer el PID del lock file, reportar quién lo tiene
//
// Probar: ejecutar dos instancias simultáneamente
//   ./lockfile &
//   ./lockfile
// La segunda instancia debería reportar que el lock está tomado.
//
// Pregunta: ¿es este mecanismo race-condition free? ¿Por qué?
```

<details><summary>Predicción</summary>

`O_CREAT | O_EXCL` es atómico a nivel del kernel: "crear si no existe" es una sola operación. No hay ventana de tiempo entre verificar existencia y crear.

Primera instancia: `open()` éxito → escribe PID → trabaja → `unlink("app.lock")`.
Segunda instancia: `open()` falla con `EEXIST` → lee PID del lock file → reporta "Lock held by PID XXXX".

Sí es race-condition free para la creación. Pero tiene problemas reales:
- Si el proceso crashea sin borrar el lock → stale lock
- Dos instancias en diferentes filesystems no comparten locks
- No funciona en NFS (O_EXCL no es confiable en NFS antiguo)

Solución robusta para stale locks: verificar si el PID del lock file sigue activo con `kill(pid, 0)`.

</details>

### Ejercicio 7 — Mini wc con POSIX I/O

```c
// Implementa una versión simplificada de wc(1) usando solo POSIX I/O:
// - Leer con read() en bloques de 4096
// - Contar: bytes, líneas ('\n'), palabras (transiciones no-espacio→espacio)
// - Imprimir con write() (sin printf — usa la función int_to_str del ej. 2)
//
// Comparar salida con: wc <archivo>
//
// Pista para palabras: mantener un flag "in_word".
// Si el carácter actual no es whitespace y !in_word → nueva palabra.
```

<details><summary>Predicción</summary>

Para un archivo con "Hello World\nFoo Bar Baz\n":
- Bytes: 22 (11 + 1 + 11 + 1 = 24... depende del contenido exacto)
- Líneas: 2 (dos '\n')
- Palabras: 5 ("Hello", "World", "Foo", "Bar", "Baz")

Algoritmo de conteo de palabras:
```
in_word = false
for each byte:
    if isspace(byte):
        in_word = false
    else:
        if !in_word: words++
        in_word = true
```

El conteo de bytes es simplemente la suma de todos los `n` retornados por `read()`. El conteo de líneas es la cantidad de `'\n'` encontrados. Las palabras requieren el estado `in_word` entre iteraciones del loop de lectura (una palabra puede cruzar el límite del buffer).

</details>

### Ejercicio 8 — Benchmark de tamaño de buffer

```c
// Mide el tiempo de copiar un archivo grande (ej: 10 MB) con distintos
// tamaños de buffer: 1, 64, 512, 4096, 65536 bytes.
//
// Crear archivo de prueba:
//   dd if=/dev/urandom of=bigfile.dat bs=1M count=10
//
// Para cada tamaño, medir con clock_gettime(CLOCK_MONOTONIC).
// Imprimir tabla:
//   Buffer    Tiempo     Syscalls
//   1         X.XXXs     ~10485760
//   64        X.XXXs     ~163840
//   512       X.XXXs     ~20480
//   4096      X.XXXs     ~2560
//   65536     X.XXXs     ~160
//
// Pregunta: ¿a partir de qué tamaño el rendimiento deja de mejorar
// significativamente? ¿Por qué?
```

<details><summary>Predicción</summary>

Tendencia esperada:
- **1 byte**: extremadamente lento — ~10M syscalls. Cada read/write cruza la barrera user→kernel.
- **64 bytes**: ~16x más rápido que 1 byte
- **512 bytes**: mejora notable
- **4096 bytes**: muy rápido — coincide con el tamaño de página de memoria y bloque de filesystem
- **65536 bytes**: marginalmente más rápido que 4096

El rendimiento deja de mejorar significativamente alrededor de **4096-8192 bytes** porque:
1. El filesystem lee/escribe en bloques de 4096 bytes
2. El page cache opera en páginas de 4096 bytes
3. Buffers más grandes reducen syscalls pero el beneficio es logarítmico
4. El readahead del kernel precarga datos automáticamente

La columna de syscalls es `tamaño_archivo / tamaño_buffer` (para read) × 2 (read + write).

</details>

### Ejercicio 9 — Append desde múltiples procesos

```c
// Escribe un programa que:
// 1. Abre "shared.log" con O_APPEND
// 2. Escribe 1000 líneas: "PID=XXXXX line=NNN\n"
// 3. Cierra el archivo
//
// Ejecutar 3 instancias simultáneamente:
//   ./append_test & ./append_test & ./append_test &
//   wait
//
// Después verificar:
//   wc -l shared.log           # ¿Exactamente 3000 líneas?
//   sort shared.log | uniq -c  # ¿Cada línea aparece una sola vez?
//   grep -c "^PID" shared.log  # ¿Todas las líneas están completas?
//
// Pregunta: ¿se sobrescriben datos? ¿Se mezclan líneas parciales?
```

<details><summary>Predicción</summary>

Con `O_APPEND`:
- `wc -l` debería mostrar exactamente **3000 líneas** — sin pérdida de datos
- Cada línea debería aparecer exactamente una vez
- Todas las líneas deberían estar completas (empezar con "PID")

`O_APPEND` garantiza que cada `write()` es atómico en cuanto a posicionamiento: el kernel mueve al final y escribe en una sola operación. No hay sobreescritura.

Sin embargo, POSIX solo garantiza atomicidad para writes de hasta `PIPE_BUF` (4096 bytes en Linux) en pipes. Para archivos regulares, la atomicidad depende del filesystem — en ext4/xfs, writes pequeños (< 4096) son efectivamente atómicos. Para writes muy grandes, las líneas podrían intercalarse.

En este ejercicio, cada línea tiene ~25 bytes, mucho menor que el límite, así que todo debería funcionar correctamente.

</details>

### Ejercicio 10 — Detectar fd leaks

```c
// Escribe un programa con un fd leak intencional:
//   - En un loop de 2000 iteraciones, abre un archivo sin cerrarlo
//   - Captura el error cuando open() falle
//   - Reporta: en qué iteración falló y qué errno recibió
//
// Después, arregla el leak y verifica que el programa completa
// las 2000 iteraciones sin error.
//
// Extra: antes del loop, consulta el límite con getrlimit(RLIMIT_NOFILE)
// y predice en qué iteración fallará.
//
// Pregunta: si el límite es 1024, ¿fallará en la iteración 1024 o antes?
```

<details><summary>Predicción</summary>

Con `ulimit -n = 1024`:
- fds 0, 1, 2 ya ocupados → quedan 1021 fds disponibles
- La primera iteración abre fd=3, la segunda fd=4, etc.
- Fallará en la iteración **1022** (fd 1024 no existe, el último válido es 1023)

Espera... el límite 1024 significa fds 0 a 1023 son válidos. Con 0, 1, 2 ocupados, quedan 1021 slots (fds 3 a 1023). La iteración 1022 intentará abrir el fd 1024, que excede el límite → `errno = EMFILE` ("Too many open files").

Nota: `getrlimit(RLIMIT_NOFILE, &rl)` retorna `rl.rlim_cur` (soft limit) y `rl.rlim_max` (hard limit). El soft limit es el que aplica; se puede subir hasta el hard limit con `setrlimit()`.

Después de arreglar el leak (agregar `close(fd)` en cada iteración), el programa solo usa fd=3 repetidamente (abre, cierra, el siguiente open reutiliza fd=3) y completa sin problemas.

</details>
