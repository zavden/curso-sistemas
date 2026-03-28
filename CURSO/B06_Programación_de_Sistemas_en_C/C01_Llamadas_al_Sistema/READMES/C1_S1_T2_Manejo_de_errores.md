# Manejo de Errores en Syscalls

## Índice

1. [La convención de error en POSIX](#1-la-convención-de-error-en-posix)
2. [errno: la variable global de error](#2-errno-la-variable-global-de-error)
3. [Códigos de error comunes](#3-códigos-de-error-comunes)
4. [perror: imprimir el error con contexto](#4-perror-imprimir-el-error-con-contexto)
5. [strerror: obtener el mensaje como string](#5-strerror-obtener-el-mensaje-como-string)
6. [Patrones de manejo de errores en C](#6-patrones-de-manejo-de-errores-en-c)
7. [Convenciones de retorno: no todas las syscalls siguen la misma regla](#7-convenciones-de-retorno-no-todas-las-syscalls-siguen-la-misma-regla)
8. [errno y threads: thread-safety](#8-errno-y-threads-thread-safety)
9. [Errores que no son errores: EINTR y EAGAIN](#9-errores-que-no-son-errores-eintr-y-eagain)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. La convención de error en POSIX

En T01 viste que el kernel retorna errores como valores negativos (`-ENOENT`, `-EPERM`) y glibc los transforma. Aquí profundizamos en **cómo detectar, interpretar y manejar esos errores** en tu código.

La convención POSIX es simple:

```
 Syscall exitosa:     retorna un valor >= 0
                      (bytes leídos, file descriptor, PID, etc.)

 Syscall con error:   retorna -1
                      errno contiene el código de error específico
```

```c
int fd = open("/etc/hosts", O_RDONLY);

if (fd == -1) {
    // Algo falló. ¿Qué exactamente?
    // errno tiene la respuesta
    printf("Error code: %d\n", errno);  // ej: 2
    perror("open");                      // open: No such file or directory
}
```

**Regla fundamental**: siempre verificar el retorno de una syscall **antes** de usar el resultado. Un `fd` de -1 usado en `read` provocará otro error, enmascarando el original.

---

## 2. errno: la variable global de error

`errno` es una variable (técnicamente una macro que expande a un l-value thread-local) definida en `<errno.h>`:

```c
#include <errno.h>

// Después de que una syscall falla:
if (write(fd, buf, len) == -1) {
    // errno ahora contiene el código de error
    if (errno == ENOSPC) {
        fprintf(stderr, "Disk full!\n");
    } else if (errno == EIO) {
        fprintf(stderr, "I/O error!\n");
    }
}
```

### Reglas críticas de errno

**1. errno solo es válido inmediatamente después de un error**

```c
// ✗ INCORRECTO
write(fd, buf, len);
printf("something\n");   // printf puede cambiar errno internamente
if (errno == ENOSPC) {   // errno ya no refleja el write
    // ...
}

// ✓ CORRECTO
if (write(fd, buf, len) == -1) {
    int saved_errno = errno;   // guardar antes de que algo lo sobreescriba
    printf("write failed\n");  // esto puede cambiar errno
    errno = saved_errno;       // restaurar si lo necesitas después
    // usar saved_errno
}
```

**2. Una syscall exitosa NO resetea errno a 0**

```c
errno = 42;              // valor basura previo
int fd = open("/etc/hosts", O_RDONLY);
// open exitoso — retorna fd >= 0
// PERO errno sigue siendo 42, no 0

// Por eso SIEMPRE se verifica el retorno primero, no errno
if (fd == -1) {           // solo mirar errno si fd == -1
    perror("open");
}
```

**3. Nunca comparar errno sin haber verificado que la syscall falló**

```c
// ✗ INCORRECTO — errno puede tener basura de una llamada anterior
read(fd, buf, sizeof(buf));
if (errno == EINTR) {   // errno podría ser EINTR de hace 5 líneas
    // ...
}

// ✓ CORRECTO
ssize_t n = read(fd, buf, sizeof(buf));
if (n == -1 && errno == EINTR) {
    // ahora sí, read falló con EINTR
}
```

### Dónde están definidos los códigos

```bash
# Ver todas las constantes de error
grep -r "^#define E" /usr/include/asm-generic/errno-base.h
grep -r "^#define E" /usr/include/asm-generic/errno.h

# Resultado (parcial):
# #define EPERM            1  /* Operation not permitted */
# #define ENOENT           2  /* No such file or directory */
# #define ESRCH            3  /* No such process */
# #define EINTR            4  /* Interrupted system call */
# #define EIO              5  /* I/O error */
# ...
# #define EAGAIN          11  /* Try again */
# #define ENOMEM          12  /* Out of memory */
# #define EACCES          13  /* Permission denied */
# ...
```

---

## 3. Códigos de error comunes

Estos son los errores que encontrarás repetidamente en programación de sistemas. Conocerlos de memoria ahorra horas de debugging:

### Errores de permisos y existencia

| Código | Nombre | Significado | Ejemplo |
|:------:|--------|-------------|---------|
| 1 | `EPERM` | Operation not permitted | `kill()` sin permisos |
| 2 | `ENOENT` | No such file or directory | `open()` de archivo inexistente |
| 13 | `EACCES` | Permission denied | `open()` sin permisos de lectura |
| 17 | `EEXIST` | File exists | `mkdir()` de directorio existente |
| 20 | `ENOTDIR` | Not a directory | `opendir()` sobre un archivo |
| 21 | `EISDIR` | Is a directory | `write()` a un directorio |

### Errores de recursos

| Código | Nombre | Significado | Ejemplo |
|:------:|--------|-------------|---------|
| 12 | `ENOMEM` | Out of memory | `malloc()`/`mmap()` sin RAM |
| 23 | `ENFILE` | Too many open files (sistema) | Límite global de FDs |
| 24 | `EMFILE` | Too many open files (proceso) | Límite por proceso de FDs |
| 28 | `ENOSPC` | No space left on device | `write()` en disco lleno |

### Errores de I/O y file descriptors

| Código | Nombre | Significado | Ejemplo |
|:------:|--------|-------------|---------|
| 5 | `EIO` | I/O error | Error de disco/hardware |
| 9 | `EBADF` | Bad file descriptor | `read()` con fd cerrado |
| 14 | `EFAULT` | Bad address | `read()` con puntero inválido |
| 32 | `EPIPE` | Broken pipe | `write()` a pipe sin lector |

### Errores de control de flujo

| Código | Nombre | Significado | Ejemplo |
|:------:|--------|-------------|---------|
| 4 | `EINTR` | Interrupted system call | Señal recibida durante `read()` |
| 11 | `EAGAIN` / `EWOULDBLOCK` | Try again | `read()` no-blocking sin datos |
| 115 | `EINPROGRESS` | Operation in progress | `connect()` no-blocking |

`EAGAIN` y `EWOULDBLOCK` tienen el mismo valor numérico (11) en Linux. Son el mismo error con dos nombres — se usan indistintamente.

---

## 4. perror: imprimir el error con contexto

`perror` imprime un mensaje de error a stderr, combinando tu string de contexto con la descripción del errno actual:

```c
#include <stdio.h>
#include <fcntl.h>

int main(void) {
    int fd = open("/no/existe", O_RDONLY);
    if (fd == -1) {
        perror("open /no/existe");
        // Imprime: open /no/existe: No such file or directory
        return 1;
    }
    return 0;
}
```

El formato es: `<tu_string>: <descripción_del_error>\n`, enviado a **stderr** (no stdout).

### Buenas prácticas para el string de perror

```c
// ✗ Genérico — no dice qué falló
perror("error");
// error: No such file or directory

// ✗ Demasiado verboso
perror("There was an error opening the file");
// There was an error opening the file: No such file or directory

// ✓ Conciso — identifica la operación y el recurso
perror("open /etc/shadow");
// open /etc/shadow: Permission denied

// ✓ Con variable
char path[] = "/etc/shadow";
int fd = open(path, O_RDONLY);
if (fd == -1) {
    perror(path);
    // /etc/shadow: Permission denied
}
```

### perror vs fprintf con strerror

```c
// Estas dos líneas producen el mismo resultado:
perror("open");
fprintf(stderr, "open: %s\n", strerror(errno));

// Pero fprintf permite más control sobre el formato:
fprintf(stderr, "ERROR: open('%s') failed: %s (errno=%d)\n",
        path, strerror(errno), errno);
// ERROR: open('/etc/shadow') failed: Permission denied (errno=13)
```

---

## 5. strerror: obtener el mensaje como string

`strerror` convierte un código de error numérico en su mensaje legible:

```c
#include <string.h>  // strerror
#include <errno.h>
#include <stdio.h>

int main(void) {
    printf("Error 2:  %s\n", strerror(2));   // No such file or directory
    printf("Error 13: %s\n", strerror(13));  // Permission denied
    printf("Error 28: %s\n", strerror(28));  // No space left on device

    // Uso típico: construir mensajes de error personalizados
    int fd = open("/etc/shadow", O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "[%s:%d] open failed: %s\n",
                __FILE__, __LINE__, strerror(errno));
    }
    return 0;
}
```

### strerror vs strerror_r: thread-safety

`strerror` retorna un puntero a un buffer estático que puede ser sobreescrito por otra llamada. En programas multi-hilo, usa `strerror_r`:

```c
// ✗ No thread-safe
char *msg = strerror(errno);
// Otro thread puede llamar strerror y sobreescribir el buffer

// ✓ Thread-safe (versión GNU)
char buf[256];
char *msg = strerror_r(errno, buf, sizeof(buf));
fprintf(stderr, "Error: %s\n", msg);

// ✓ Thread-safe (versión XSI/POSIX — diferente interfaz)
char buf[256];
int ret = strerror_r(errno, buf, sizeof(buf));  // retorna 0 en éxito
fprintf(stderr, "Error: %s\n", buf);
```

Para evitar confusiones con las dos versiones de `strerror_r`, en código single-thread `strerror` es perfectamente válido.

### Imprimir todos los errores del sistema

```c
#include <string.h>
#include <stdio.h>

int main(void) {
    for (int i = 1; i < 135; i++) {
        char *msg = strerror(i);
        // strerror retorna "Unknown error N" para códigos no definidos
        if (msg[0] != 'U') {  // filtrar "Unknown error"
            printf("%3d: %s\n", i, msg);
        }
    }
    return 0;
}
```

---

## 6. Patrones de manejo de errores en C

### Patrón 1: verificar y salir (programas simples)

```c
int fd = open(path, O_RDONLY);
if (fd == -1) {
    perror(path);
    exit(EXIT_FAILURE);
}
// usar fd...
```

Directo. Para programas de línea de comandos donde un error es fatal.

### Patrón 2: verificar y retornar error (funciones de biblioteca)

```c
// La función retorna -1 en error, preservando errno
int read_entire_file(const char *path, char *buf, size_t bufsize) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        return -1;  // errno ya está set por open
    }

    ssize_t n = read(fd, buf, bufsize);
    if (n == -1) {
        int saved = errno;  // guardar errno de read
        close(fd);          // close puede cambiar errno
        errno = saved;      // restaurar el error original
        return -1;
    }

    close(fd);
    return (int)n;
}

// El caller maneja el error:
int n = read_entire_file("/etc/hosts", buf, sizeof(buf));
if (n == -1) {
    perror("read_entire_file");
}
```

Nota cómo se guarda y restaura `errno` alrededor de `close` — si no lo haces, el `errno` que el caller ve sería el de `close`, no el del error original.

### Patrón 3: goto cleanup (recursos múltiples)

Cuando la función abre múltiples recursos, `goto` es el patrón estándar de C para cleanup ordenado:

```c
int process_files(const char *input_path, const char *output_path) {
    int ret = -1;
    int fd_in = -1;
    int fd_out = -1;
    char *buf = NULL;

    fd_in = open(input_path, O_RDONLY);
    if (fd_in == -1) {
        perror(input_path);
        goto cleanup;
    }

    fd_out = open(output_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_out == -1) {
        perror(output_path);
        goto cleanup;
    }

    buf = malloc(4096);
    if (buf == NULL) {
        perror("malloc");
        goto cleanup;
    }

    ssize_t n;
    while ((n = read(fd_in, buf, 4096)) > 0) {
        if (write(fd_out, buf, n) != n) {
            perror("write");
            goto cleanup;
        }
    }
    if (n == -1) {
        perror("read");
        goto cleanup;
    }

    ret = 0;  // éxito

cleanup:
    free(buf);               // free(NULL) es seguro
    if (fd_out != -1) close(fd_out);
    if (fd_in != -1) close(fd_in);
    return ret;
}
```

El `goto cleanup` no es "goto malo" — es el idioma estándar de C para manejo de recursos. Cada recurso se libera en orden inverso, y los checks `-1`/`NULL` aseguran que solo se libera lo que se adquirió.

### Patrón 4: helper que aborta (die pattern)

Para prototipos y herramientas de una sola ejecución donde no vale la pena manejar cada error:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>

void die(const char *fmt, ...) {
    int saved_errno = errno;
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (saved_errno) {
        fprintf(stderr, ": %s", strerror(saved_errno));
    }
    fprintf(stderr, "\n");
    exit(EXIT_FAILURE);
}

// Uso:
int fd = open(path, O_RDONLY);
if (fd == -1) die("open '%s'", path);
// open '/etc/shadow': Permission denied
```

Útil para reducir boilerplate. No apto para bibliotecas (una biblioteca no debería abortar el programa).

---

## 7. Convenciones de retorno: no todas las syscalls siguen la misma regla

La mayoría de syscalls retornan -1 en error, pero hay excepciones:

### Retornan -1 en error (la mayoría)

```c
int fd = open(path, flags);         // -1 en error, fd >= 0 en éxito
ssize_t n = read(fd, buf, count);   // -1 en error, bytes leídos en éxito
int ret = close(fd);                // -1 en error, 0 en éxito
pid_t pid = fork();                 // -1 en error, 0 (hijo) o pid (padre) en éxito
```

### Retornan NULL en error

```c
FILE *fp = fopen(path, "r");           // NULL en error (función de stdio, no syscall)
void *p = mmap(NULL, len, ...);        // MAP_FAILED ((void *)-1) en error, NO NULL
DIR *d = opendir(path);               // NULL en error
```

`mmap` es un caso especial — retorna `MAP_FAILED` (que es `(void *)-1`), no NULL:

```c
void *addr = mmap(NULL, 4096, PROT_READ, MAP_PRIVATE, fd, 0);

// ✗ INCORRECTO
if (addr == NULL) { /* ... */ }

// ✓ CORRECTO
if (addr == MAP_FAILED) {
    perror("mmap");
}
```

### Nunca fallan (en la práctica)

```c
pid_t pid = getpid();   // siempre retorna el PID, nunca falla
uid_t uid = getuid();   // siempre retorna el UID
```

### Retornan el error directamente (no usan errno)

Las funciones de pthreads retornan 0 en éxito o un código de error **directamente** (no ponen errno):

```c
#include <pthread.h>

pthread_mutex_t lock;
int err = pthread_mutex_lock(&lock);
if (err != 0) {
    // err contiene el código de error (ej: EDEADLK)
    // errno NO fue modificado
    fprintf(stderr, "mutex_lock: %s\n", strerror(err));
}
```

### getaddrinfo: retorno propio

```c
#include <netdb.h>

struct addrinfo *res;
int err = getaddrinfo("example.com", "80", NULL, &res);
if (err != 0) {
    // NO usar perror ni strerror — usar gai_strerror
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
}
```

### Resumen de convenciones

| Familia | Retorno en error | Variable de error | Función para mensaje |
|---------|:----------------:|:-----------------:|---------------------|
| Syscalls POSIX | -1 | `errno` | `strerror(errno)`, `perror()` |
| mmap | `MAP_FAILED` | `errno` | `perror()` |
| stdio (fopen, etc.) | NULL | `errno` | `perror()` |
| pthreads | código de error > 0 | (directo) | `strerror(err)` |
| getaddrinfo | código de error | (directo) | `gai_strerror(err)` |

---

## 8. errno y threads: thread-safety

`errno` parece una variable global, y en la era de procesos single-thread lo era. Pero en programas multi-hilo, cada thread tiene su **propia copia de errno**:

```c
// errno está definido como:
#define errno (*__errno_location())
// __errno_location() retorna un puntero diferente para cada thread
```

Esto significa que si el thread A hace una syscall que falla, y el thread B hace una syscall exitosa al mismo tiempo, los valores de errno no se pisan entre sí.

```c
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

void *thread_func(void *arg) {
    int id = *(int *)arg;

    // Abrir un archivo que no existe
    int fd = open("/no/existe", O_RDONLY);
    if (fd == -1) {
        // Este errno es local a este thread
        printf("Thread %d: errno = %d (%s)\n",
               id, errno, strerror(errno));
    }
    return NULL;
}

int main(void) {
    pthread_t t1, t2;
    int id1 = 1, id2 = 2;

    pthread_create(&t1, NULL, thread_func, &id1);
    pthread_create(&t2, NULL, thread_func, &id2);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    return 0;
}
```

```bash
gcc -pthread -o errno_threads errno_threads.c
./errno_threads
# Thread 1: errno = 2 (No such file or directory)
# Thread 2: errno = 2 (No such file or directory)
# Cada thread tiene su propio errno — no se interfieren
```

---

## 9. Errores que no son errores: EINTR y EAGAIN

Dos códigos de error son especiales porque no indican un fallo real — indican que debes **reintentar** la operación.

### EINTR: una señal interrumpió la syscall

Cuando una syscall bloqueante (como `read` esperando datos) es interrumpida por una señal, retorna -1 con `errno == EINTR`. No significa que haya un error — significa "una señal llegó, reintenta":

```c
ssize_t safe_read(int fd, void *buf, size_t count) {
    ssize_t n;
    do {
        n = read(fd, buf, count);
    } while (n == -1 && errno == EINTR);
    return n;
}
```

El loop `do { ... } while (errno == EINTR)` es tan común que se vuelve automático. Sin él, una señal inofensiva (como `SIGCHLD` cuando un proceso hijo termina) puede hacer que tu programa falle innecesariamente.

### EAGAIN / EWOULDBLOCK: no hay datos ahora, intenta luego

En file descriptors configurados como non-blocking (`O_NONBLOCK`), si no hay datos disponibles, `read` retorna -1 con `errno == EAGAIN`:

```c
// fd configurado como non-blocking
ssize_t n = read(fd, buf, sizeof(buf));
if (n == -1) {
    if (errno == EAGAIN) {
        // No hay datos ahora — no es un error
        // Hacer otra cosa y reintentar después
        // (o usar poll/epoll para esperar)
    } else {
        // Error real
        perror("read");
    }
}
```

### Combinar ambos: el patrón robusto

```c
ssize_t robust_read(int fd, void *buf, size_t count) {
    ssize_t n;
    do {
        n = read(fd, buf, count);
    } while (n == -1 && (errno == EINTR || errno == EAGAIN));
    return n;
}
```

En la práctica, combinar ambos en un loop solo tiene sentido si el fd es blocking. Si es non-blocking, `EAGAIN` significa "no hay datos" y un loop sin espera consume CPU inútilmente — ahí usas `poll`/`epoll` (se ve en C06 Sockets).

### EINTR en otras syscalls

No solo `read`/`write` — muchas syscalls pueden retornar EINTR:

| Syscall | ¿Puede retornar EINTR? |
|---------|:----------------------:|
| `read`, `write` | Sí |
| `open` | Sí (raro, pero posible) |
| `close` | Sí (y NO reintentar — ver sección 10) |
| `connect` | Sí |
| `accept` | Sí |
| `wait`, `waitpid` | Sí |
| `nanosleep` | Sí |
| `poll`, `select` | Sí |
| `sem_wait` | Sí |

---

## 10. Errores comunes

### Error 1: revisar errno sin verificar el retorno

```c
// ✗ errno puede tener basura de una llamada anterior
read(fd, buf, sizeof(buf));
if (errno != 0) {  // ¡MAL! errno puede no ser 0 incluso si read tuvo éxito
    perror("read");
}

// ✓ Siempre verificar el retorno primero
ssize_t n = read(fd, buf, sizeof(buf));
if (n == -1) {
    perror("read");
}
```

### Error 2: llamar a una función entre el error y la lectura de errno

```c
if (open(path, O_RDONLY) == -1) {
    // ✗ log_message puede llamar syscalls internamente y modificar errno
    log_message("Failed to open file");
    perror("open");  // errno ya no refleja el error de open

    // ✓ Guardar errno inmediatamente
    int saved = errno;
    log_message("Failed to open file");
    fprintf(stderr, "open: %s\n", strerror(saved));
}
```

### Error 3: reintentar close() tras EINTR

```c
// ✗ INCORRECTO en Linux — puede cerrar un fd reasignado
while (close(fd) == -1 && errno == EINTR)
    ;  // ¡NO HACER ESTO!
```

**Por qué es peligroso**: en Linux, `close` libera el fd **incluso cuando retorna EINTR**. Si reintentamos, otro thread podría haber reutilizado ese número de fd, y estaríamos cerrando el fd equivocado.

```c
// ✓ CORRECTO en Linux — close una vez, ignorar EINTR
if (close(fd) == -1 && errno != EINTR) {
    perror("close");
}
```

### Error 4: comparar mmap con NULL en vez de MAP_FAILED

```c
// ✗ mmap NUNCA retorna NULL como indicador de error
void *p = mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd, 0);
if (p == NULL) {
    // ¡Nunca se ejecuta! mmap retorna MAP_FAILED en error, no NULL
}

// ✓ CORRECTO
if (p == MAP_FAILED) {
    perror("mmap");
}
```

### Error 5: no manejar short writes

```c
// ✗ Asume que write escribe todo
write(fd, buf, len);

// ✓ CORRECTO — manejar escritura parcial
ssize_t total = 0;
while (total < (ssize_t)len) {
    ssize_t n = write(fd, buf + total, len - total);
    if (n == -1) {
        if (errno == EINTR) continue;
        perror("write");
        break;
    }
    total += n;
}
```

`write` puede escribir menos bytes de los solicitados. Si no manejas esto, pierdes datos silenciosamente. Este patrón se explora a fondo en S02/T01.

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│              MANEJO DE ERRORES — REFERENCIA                      │
├──────────────────────────────────────────────────────────────────┤
│ VERIFICAR RETORNO                                                │
│   if (ret == -1)        Syscalls POSIX                          │
│   if (ptr == NULL)      fopen, opendir, malloc                  │
│   if (ptr == MAP_FAILED) mmap                                   │
│   if (err != 0)         pthreads, getaddrinfo                   │
├──────────────────────────────────────────────────────────────────┤
│ INTERPRETAR EL ERROR                                             │
│   errno                 Código numérico (ej: 2)                 │
│   strerror(errno)       Mensaje legible ("No such file...")     │
│   perror("context")     Imprime "context: mensaje\n" a stderr   │
├──────────────────────────────────────────────────────────────────┤
│ CÓDIGOS FRECUENTES                                               │
│   EPERM    (1)   No tienes permiso para esa operación           │
│   ENOENT   (2)   Archivo o directorio no existe                 │
│   EINTR    (4)   Señal interrumpió la syscall — reintentar      │
│   EIO      (5)   Error de I/O (hardware)                        │
│   EBADF    (9)   File descriptor inválido                       │
│   EAGAIN  (11)   No hay datos ahora (non-blocking) — reintentar │
│   ENOMEM  (12)   Sin memoria                                    │
│   EACCES  (13)   Permiso denegado (archivo)                     │
│   EFAULT  (14)   Puntero inválido                               │
│   EEXIST  (17)   El archivo ya existe                           │
│   EMFILE  (24)   Demasiados archivos abiertos (proceso)         │
│   ENOSPC  (28)   Disco lleno                                    │
│   EPIPE   (32)   Pipe roto (lector cerrado)                     │
├──────────────────────────────────────────────────────────────────┤
│ PATRONES                                                         │
│   perror + exit          Programas simples                      │
│   return -1 + errno      Funciones de biblioteca                │
│   goto cleanup           Múltiples recursos                      │
│   die() helper           Prototipos rápidos                     │
│   do{...}while(EINTR)    Reintentar tras señal                  │
├──────────────────────────────────────────────────────────────────┤
│ REGLAS                                                           │
│   ✓ Verificar retorno ANTES de leer errno                       │
│   ✓ Guardar errno inmediatamente si necesitas usarlo después    │
│   ✓ Reintentar en EINTR (excepto close en Linux)                │
│   ✗ No asumir que errno es 0 tras una syscall exitosa           │
│   ✗ No llamar funciones entre error y lectura de errno          │
│   ✗ No reintentar close tras EINTR en Linux                     │
└──────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Explorar errno y sus mensajes

1. Escribe un programa que imprima todos los códigos de error del 1 al 133 con su mensaje:
   ```c
   #include <string.h>
   #include <stdio.h>
   #include <errno.h>

   int main(void) {
       for (int i = 1; i <= 133; i++) {
           printf("%3d  %-20s  %s\n", i, /* nombre */, strerror(i));
       }
       return 0;
   }
   ```
   Nota: no hay forma estándar de obtener el nombre simbólico (`ENOENT`) desde el número en C. Imprime solo el número y el mensaje. Compara con la salida de `errno -l` si tienes el paquete `moreutils`.

2. Provoca errores específicos y verifica que obtienes el errno correcto:
   ```c
   open("/no/existe", O_RDONLY);        // ¿errno == ENOENT (2)?
   open("/etc/shadow", O_RDONLY);       // ¿errno == EACCES (13)?
   open("/etc/hosts", O_CREAT | O_EXCL, 0644);  // ¿errno == EEXIST (17)?
   close(9999);                         // ¿errno == EBADF (9)?
   ```

> **Pregunta de reflexión**: ¿por qué `open("/etc/shadow", O_RDONLY)` retorna `EACCES` y no `EPERM`? ¿Cuál es la diferencia entre ambos? (Pista: `EPERM` se usa cuando la operación no está permitida sin importar los permisos; `EACCES` cuando los permisos del filesystem lo bloquean.)

### Ejercicio 2: Implementar el patrón goto cleanup

Escribe una función `copy_file(src, dst)` que:
1. Abra el archivo fuente para lectura.
2. Abra el archivo destino para escritura (crear si no existe, truncar si existe).
3. Lea en un buffer de 4096 bytes y escriba al destino en un loop.
4. Maneje correctamente:
   - EINTR en read y write (reintentar).
   - Short writes (loop hasta escribir todo).
   - Cualquier error: imprimir con perror, hacer cleanup de todos los recursos abiertos.
5. Use el patrón `goto cleanup` con inicialización a -1/NULL.
6. Preservar errno del error original a través del cleanup.

Prueba con:
- Archivo que existe → copia exitosa.
- Archivo fuente que no existe → error con perror.
- Destino en directorio sin permisos → error con perror.

> **Pregunta de reflexión**: en tu función, ¿qué pasa si `close(fd_out)` falla después de un `write` exitoso? ¿Los datos están en el disco o podrían haberse perdido? (Pista: buffered writes del kernel.)

### Ejercicio 3: EINTR en la práctica

1. Escribe un programa que:
   - Registre un handler para SIGALRM que solo ponga un flag (`volatile sig_atomic_t`).
   - Configure una alarma con `alarm(1)` (se dispara en 1 segundo).
   - Llame a `read(STDIN_FILENO, buf, sizeof(buf))` para leer de stdin.
   - Si `read` retorna -1 con EINTR, imprima "Interrupted by signal, retrying..." y reintente.
2. Ejecuta el programa sin escribir nada en stdin. Después de 1 segundo, SIGALRM llega y debería interrumpir el `read`.
3. ¿Se imprime el mensaje de retry? ¿El programa reintenta correctamente?
4. Modifica para usar el patrón `do { ... } while (errno == EINTR)`.

> **Pregunta de reflexión**: si usas `SA_RESTART` en `sigaction` al registrar el handler de SIGALRM, ¿`read` retornaría EINTR o se reiniciaría automáticamente? ¿En qué syscalls funciona `SA_RESTART` y en cuáles no?
