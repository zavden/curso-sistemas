# Pipes Anónimos — Comunicación Padre↔Hijo con pipe()

## Índice

1. [Qué es un pipe](#1-qué-es-un-pipe)
2. [pipe(): crear un pipe](#2-pipe-crear-un-pipe)
3. [Pipe + fork: comunicación padre↔hijo](#3-pipe--fork-comunicación-padrehijo)
4. [Cierre de extremos](#4-cierre-de-extremos)
5. [EOF y SIGPIPE](#5-eof-y-sigpipe)
6. [Pipes bidireccionales](#6-pipes-bidireccionales)
7. [Capacidad y bloqueo](#7-capacidad-y-bloqueo)
8. [pipe2: flags atómicos](#8-pipe2-flags-atómicos)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es un pipe

Un pipe es un **canal unidireccional** de bytes entre dos file
descriptors. Uno escribe, el otro lee. Los datos fluyen en orden FIFO
y son **efímeros** — no se almacenan en disco:

```
  Escritor                          Lector
  ┌──────────┐                    ┌──────────┐
  │          │   ┌────────────┐   │          │
  │ write(w) │──▶│   pipe     │──▶│ read(r)  │
  │          │   │ (buffer    │   │          │
  │          │   │  en kernel)│   │          │
  └──────────┘   └────────────┘   └──────────┘
       fd[1]        65536 bytes       fd[0]
     (write end)    (típico)       (read end)
```

### Características

| Propiedad         | Valor                                    |
|-------------------|------------------------------------------|
| Dirección         | Unidireccional (un escritor, un lector)  |
| Almacenamiento    | Buffer en kernel (no disco)              |
| Persistencia      | Existe mientras algún fd esté abierto    |
| Capacidad         | 65536 bytes (Linux default, configurable)|
| Orden             | FIFO estricto                            |
| Fronteras         | No — es un stream de bytes sin mensajes  |
| Nombre            | No tiene nombre en el filesystem         |
| Relación          | Solo procesos relacionados (fork)        |

### Por qué "anónimo"

No tiene nombre en el filesystem (a diferencia de los named pipes/FIFOs).
Solo se puede compartir heredando los file descriptors a través de
`fork()`. Procesos no relacionados no pueden acceder a un pipe anónimo.

---

## 2. pipe(): crear un pipe

```c
#include <unistd.h>

int pipe(int pipefd[2]);
```

- `pipefd[0]`: extremo de **lectura** (read end).
- `pipefd[1]`: extremo de **escritura** (write end).
- Retorna 0 en éxito, -1 en error.

### Convención de índices

```
  pipefd[0] = Read  (input)    ← Leer datos del pipe
  pipefd[1] = Write (output)   ← Escribir datos al pipe

  Mnemotécnica: 0 = stdin, 1 = stdout
                0 = boca (recibe), 1 = mano (produce)
```

### Ejemplo mínimo (mismo proceso)

```c
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    int pipefd[2];

    if (pipe(pipefd) == -1) {
        perror("pipe");
        return 1;
    }

    /* Write to the pipe */
    const char *msg = "hello pipe";
    write(pipefd[1], msg, strlen(msg));

    /* Read from the pipe */
    char buf[64];
    ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
    buf[n] = '\0';

    printf("read: '%s'\n", buf);

    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}
```

> **Nota**: Un pipe dentro del mismo proceso tiene poco sentido práctico.
> El valor real aparece con `fork()`.

---

## 3. Pipe + fork: comunicación padre↔hijo

El patrón fundamental: crear el pipe **antes** de fork. Ambos procesos
heredan ambos extremos. Cada uno cierra el extremo que no usa:

### Padre escribe, hijo lee

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) { perror("pipe"); return 1; }

    pid_t pid = fork();
    if (pid == -1) { perror("fork"); return 1; }

    if (pid == 0) {
        /* ── Child: reads from pipe ── */
        close(pipefd[1]);  /* Close write end (don't need it) */

        char buf[256];
        ssize_t n;
        while ((n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0) {
            buf[n] = '\0';
            printf("child received: %s\n", buf);
        }

        close(pipefd[0]);
        _exit(0);
    }

    /* ── Parent: writes to pipe ── */
    close(pipefd[0]);  /* Close read end (don't need it) */

    const char *messages[] = {"hello", "world", "goodbye"};
    for (int i = 0; i < 3; i++) {
        write(pipefd[1], messages[i], strlen(messages[i]));
        write(pipefd[1], "\n", 1);
    }

    close(pipefd[1]);  /* Close write end → child sees EOF */

    waitpid(pid, NULL, 0);
    return 0;
}
```

### Flujo de file descriptors

```
  Antes de fork:
  ┌─────────────────────────┐
  │ Proceso                 │
  │  pipefd[0] = 3 (read)   │
  │  pipefd[1] = 4 (write)  │
  └─────────────────────────┘

  Después de fork:
  ┌──────────────────┐         ┌──────────────────┐
  │ Parent           │         │ Child            │
  │  pipefd[0] = 3 ──┼── R ───┼── pipefd[0] = 3  │
  │  pipefd[1] = 4 ──┼── W ───┼── pipefd[1] = 4  │
  └──────────────────┘         └──────────────────┘
  Ambos comparten el mismo pipe (misma open file description)

  Después de cerrar extremos no usados:
  ┌──────────────────┐         ┌──────────────────┐
  │ Parent           │         │ Child            │
  │  pipefd[0] = ✗   │  pipe   │  pipefd[0] = 3   │
  │  pipefd[1] = 4 ──┼────W──▶R──┼── ✗ = pipefd[1]│
  └──────────────────┘         └──────────────────┘
  Parent escribe → Child lee
```

### Hijo escribe, padre lee

```c
if (pid == 0) {
    /* Child: writes */
    close(pipefd[0]);

    char msg[64];
    snprintf(msg, sizeof(msg), "child PID=%d reporting", getpid());
    write(pipefd[1], msg, strlen(msg));

    close(pipefd[1]);
    _exit(0);
}

/* Parent: reads */
close(pipefd[1]);

char buf[256];
ssize_t n = read(pipefd[0], buf, sizeof(buf) - 1);
if (n > 0) {
    buf[n] = '\0';
    printf("parent got: %s\n", buf);
}

close(pipefd[0]);
waitpid(pid, NULL, 0);
```

---

## 4. Cierre de extremos

El cierre correcto de extremos es **crítico**. Los errores más comunes
con pipes vienen de no cerrar los extremos innecesarios.

### Por qué cerrar el write end no usado

Si el proceso lector no cierra su copia del write end, **nunca verá
EOF**. El pipe mantiene un contador de escritores. Solo cuando TODOS
los write ends se cierran, `read()` retorna 0 (EOF):

```c
/* INCORRECTO: child never sees EOF */
if (pid == 0) {
    /* Child: reads, but FORGOT to close pipefd[1] */
    /* close(pipefd[1]); ← MISSING */

    char buf[256];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        /* process data */
    }
    /* NEVER REACHES HERE: child still has write end open,
       so read() blocks forever waiting for more data */
}
```

```
  Pipe reference counts:

  Ambos extremos abiertos en padre e hijo:
    readers: 2 (parent + child have pipefd[0])
    writers: 2 (parent + child have pipefd[1])

  Después de cerrar correctamente:
    readers: 1 (solo child tiene pipefd[0])
    writers: 1 (solo parent tiene pipefd[1])

  Parent cierra pipefd[1]:
    writers: 0 → read() en child retorna 0 (EOF)
```

### Por qué cerrar el read end no usado

Si el proceso escritor no cierra su copia del read end, desperdicia un
fd. Además, si el lector muere, el escritor no recibirá `SIGPIPE` ni
`EPIPE` porque **él mismo** tiene el read end abierto:

```c
/* INCORRECTO: parent keeps read end, masking broken pipe */
/* Parent: writes */
/* close(pipefd[0]); ← MISSING */

/* If child dies, parent still has the read end open.
   write() succeeds (data goes to kernel buffer)
   even though nobody will ever read it. */
```

### Regla universal

> Cada proceso cierra los extremos del pipe que **no usa**, inmediatamente
> después de fork.

---

## 5. EOF y SIGPIPE

### EOF: todos los escritores cerraron

Cuando todos los file descriptors del write end se cierran, `read()`
retorna **0** (EOF):

```c
/* Writer closes: */
close(pipefd[1]);

/* Reader sees: */
ssize_t n = read(pipefd[0], buf, sizeof(buf));
/* n == 0 → EOF → exit read loop */
```

### SIGPIPE: todos los lectores cerraron

Cuando todos los file descriptors del read end se cierran y un proceso
intenta `write()`, recibe **SIGPIPE** (acción default: terminar).
Si SIGPIPE es ignorado, `write()` retorna -1 con `errno = EPIPE`:

```c
/* Reader closes (or dies): */
close(pipefd[0]);

/* Writer tries to write: */
signal(SIGPIPE, SIG_IGN);  /* Or we'd die */

ssize_t n = write(pipefd[1], "data", 4);
/* n == -1, errno == EPIPE */
```

```
  ┌──────────────────────────────────────────────┐
  │  Pipe lifecycle signals:                     │
  │                                              │
  │  Escritor cierra → Lector ve EOF (read=0)    │
  │  Lector cierra   → Escritor ve SIGPIPE/EPIPE │
  │                                              │
  │  Esto es lo que permite:                     │
  │    yes | head -10                            │
  │    head lee 10 líneas, cierra pipe           │
  │    yes recibe SIGPIPE, muere                 │
  │    → sin SIGPIPE, yes correría para siempre  │
  └──────────────────────────────────────────────┘
```

---

## 6. Pipes bidireccionales

Un pipe es unidireccional. Para comunicación bidireccional, se necesitan
**dos pipes**:

```c
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    int parent_to_child[2];  /* Parent writes, child reads */
    int child_to_parent[2];  /* Child writes, parent reads */

    pipe(parent_to_child);
    pipe(child_to_parent);

    pid_t pid = fork();

    if (pid == 0) {
        /* ── Child ── */
        close(parent_to_child[1]);  /* Close write end */
        close(child_to_parent[0]);  /* Close read end */

        int r = parent_to_child[0];
        int w = child_to_parent[1];

        /* Read question from parent */
        char buf[256];
        ssize_t n = read(r, buf, sizeof(buf) - 1);
        buf[n] = '\0';
        printf("child got: %s\n", buf);

        /* Send answer to parent */
        const char *reply = "42";
        write(w, reply, strlen(reply));

        close(r);
        close(w);
        _exit(0);
    }

    /* ── Parent ── */
    close(parent_to_child[0]);
    close(child_to_parent[1]);

    int w = parent_to_child[1];
    int r = child_to_parent[0];

    /* Send question */
    const char *question = "what is the answer?";
    write(w, question, strlen(question));
    close(w);  /* Signal end of question */

    /* Read answer */
    char buf[256];
    ssize_t n = read(r, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    printf("parent got: %s\n", buf);

    close(r);
    waitpid(pid, NULL, 0);
    return 0;
}
```

```
  Parent                              Child
  ┌──────────┐                       ┌──────────┐
  │          │ parent_to_child       │          │
  │    W ────┼───────────────────▶R──┼──        │
  │          │                       │          │
  │          │ child_to_parent       │          │
  │    R ◀───┼───────────────────W───┼──        │
  │          │                       │          │
  └──────────┘                       └──────────┘
```

### socketpair: bidireccional en un paso

`socketpair` crea dos sockets conectados, cada uno bidireccional:

```c
#include <sys/socket.h>

int sv[2];
socketpair(AF_UNIX, SOCK_STREAM, 0, sv);

/* sv[0] and sv[1] are both read/write */
/* Parent uses sv[0], child uses sv[1] (or vice versa) */

pid_t pid = fork();
if (pid == 0) {
    close(sv[0]);
    /* Child reads and writes on sv[1] */
    write(sv[1], "hello", 5);
    read(sv[1], buf, sizeof(buf));
    close(sv[1]);
    _exit(0);
}

close(sv[1]);
/* Parent reads and writes on sv[0] */
read(sv[0], buf, sizeof(buf));
write(sv[0], "world", 5);
close(sv[0]);
```

---

## 7. Capacidad y bloqueo

### Buffer del pipe

En Linux, el buffer por defecto es **65536 bytes** (16 páginas de 4KiB).
Se puede consultar y modificar con `fcntl`:

```c
/* Read pipe capacity */
int capacity = fcntl(pipefd[0], F_GETPIPE_SZ);
printf("pipe capacity: %d bytes\n", capacity);

/* Set pipe capacity (up to /proc/sys/fs/pipe-max-size) */
fcntl(pipefd[0], F_SETPIPE_SZ, 1048576);  /* 1 MiB */
```

```bash
# System-wide maximum
cat /proc/sys/fs/pipe-max-size
# 1048576 (1 MiB, default)
```

### Comportamiento de bloqueo

| Operación    | Pipe vacío              | Pipe lleno              |
|-------------|-------------------------|-------------------------|
| `read()`    | **Bloquea** hasta datos | Lee lo disponible       |
| `write()`   | Escribe inmediatamente  | **Bloquea** hasta espacio|

### Atomicidad de write

POSIX garantiza que escrituras de hasta **PIPE_BUF** bytes son
**atómicas** — no se intercalan con escrituras de otros procesos:

```c
#include <limits.h>
/* PIPE_BUF = 4096 on Linux */

/* ATOMIC: write <= PIPE_BUF */
write(pipefd[1], buf, 4096);
/* All 4096 bytes appear contiguously in the pipe */

/* NOT ATOMIC: write > PIPE_BUF */
write(pipefd[1], buf, 65536);
/* May be interleaved with writes from other processes */
```

Esto es relevante cuando múltiples procesos escriben al mismo pipe
(por ejemplo, varios workers reportando a un proceso coordinador).

### Non-blocking pipe

```c
#include <fcntl.h>

/* Make read end non-blocking */
int flags = fcntl(pipefd[0], F_GETFL);
fcntl(pipefd[0], F_SETFL, flags | O_NONBLOCK);

/* Now read returns -1 with EAGAIN if pipe is empty */
ssize_t n = read(pipefd[0], buf, sizeof(buf));
if (n == -1 && errno == EAGAIN) {
    /* No data available — do something else */
}
```

---

## 8. pipe2: flags atómicos

`pipe2` (Linux 2.6.27+) crea el pipe con flags aplicados atómicamente:

```c
#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>

int pipefd[2];
pipe2(pipefd, O_NONBLOCK | O_CLOEXEC);

/* Equivalent to:
   pipe(pipefd);
   fcntl(pipefd[0], F_SETFL, O_NONBLOCK);
   fcntl(pipefd[1], F_SETFL, O_NONBLOCK);
   fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
   fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);
   But atomic — no race window between pipe() and fcntl() */
```

Flags disponibles:

| Flag          | Efecto                                  |
|---------------|-----------------------------------------|
| `O_NONBLOCK`  | Ambos extremos non-blocking             |
| `O_CLOEXEC`   | Ambos extremos close-on-exec            |
| `O_DIRECT`    | Modo "packet" — preserva fronteras (Linux 3.4+)|

### O_DIRECT: modo paquete

Con `O_DIRECT`, cada `write()` se convierte en un "paquete" que `read()`
lee como unidad independiente (hasta PIPE_BUF bytes):

```c
pipe2(pipefd, O_DIRECT);

/* Writer: each write is a discrete message */
write(pipefd[1], "hello", 5);
write(pipefd[1], "world", 5);

/* Reader: each read gets exactly one message */
char buf[256];
ssize_t n;

n = read(pipefd[0], buf, sizeof(buf));  /* n=5: "hello" */
n = read(pipefd[0], buf, sizeof(buf));  /* n=5: "world" */

/* Without O_DIRECT, a single read might return "helloworld" */
```

---

## 9. Errores comunes

### Error 1: No cerrar extremos no usados del pipe

```c
/* MAL: child tiene write end abierto → read nunca ve EOF */
pipe(pipefd);
if (fork() == 0) {
    /* Child reads but forgot to close write end */
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
        process(buf, n);
    /* NEVER EXITS: child's own pipefd[1] keeps pipe alive */
}
close(pipefd[0]);
write(pipefd[1], data, len);
close(pipefd[1]);
/* Parent closed write end, but child still has it → no EOF */

/* BIEN: cerrar inmediatamente después de fork */
if (fork() == 0) {
    close(pipefd[1]);  /* First thing: close unused end */
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0)
        process(buf, n);
    /* EOF when parent closes pipefd[1] ✓ */
    close(pipefd[0]);
    _exit(0);
}
close(pipefd[0]);  /* Parent closes unused read end */
```

### Error 2: Crear el pipe después de fork

```c
/* MAL: cada proceso tiene su propio pipe — no conectados */
pid_t pid = fork();
if (pid == 0) {
    int pipefd[2];
    pipe(pipefd);  /* Child's own pipe, not shared with parent */
    /* ... */
}

/* BIEN: pipe ANTES de fork */
int pipefd[2];
pipe(pipefd);      /* Both ends exist in parent */
pid_t pid = fork();  /* Child inherits both ends */
```

### Error 3: Asumir que write escribe todo

```c
/* MAL: write puede escribir menos bytes (short write) */
write(pipefd[1], large_buf, 100000);
/* May write only 65536 bytes (pipe capacity) and block,
   or write partially if non-blocking */

/* BIEN: loop de escritura completa */
ssize_t write_all(int fd, const void *buf, size_t len)
{
    const char *p = buf;
    size_t remaining = len;

    while (remaining > 0) {
        ssize_t n = write(fd, p, remaining);
        if (n == -1) {
            if (errno == EINTR) continue;
            return -1;
        }
        p += n;
        remaining -= n;
    }
    return len;
}
```

### Error 4: Deadlock con pipes bidireccionales

```c
/* MAL: ambos procesos bloquean esperando al otro */
/* Parent writes to child, then reads response */
write(to_child[1], big_data, BIG);  /* Fills pipe, blocks */
/* Child also tries to write back before reading: */
/* write(to_parent[1], big_data, BIG); → also blocks */
/* DEADLOCK: both waiting for the other to read */

/* BIEN: usar poll o alternating read/write */
/* Or use non-blocking I/O + poll to avoid deadlock */
struct pollfd fds[2];
fds[0].fd = to_child[1];   fds[0].events = POLLOUT;
fds[1].fd = from_child[0]; fds[1].events = POLLIN;

while (to_send > 0 || to_recv > 0) {
    poll(fds, 2, -1);
    if (fds[0].revents & POLLOUT) { /* write some */ }
    if (fds[1].revents & POLLIN)  { /* read some */ }
}
```

### Error 5: No manejar SIGPIPE en escritor

```c
/* MAL: si el lector muere, el escritor muere por SIGPIPE */
while (1) {
    write(pipefd[1], data, len);
    /* If reader closed → SIGPIPE → writer dies unexpectedly */
}

/* BIEN: ignorar SIGPIPE, manejar EPIPE */
signal(SIGPIPE, SIG_IGN);

while (1) {
    ssize_t n = write(pipefd[1], data, len);
    if (n == -1) {
        if (errno == EPIPE) {
            printf("reader closed, stopping\n");
            break;
        }
        perror("write");
        break;
    }
}
```

---

## 10. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │                   Pipes Anónimos                              │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Crear:                                                      │
  │    pipe(pipefd)                  pipefd[0]=read, [1]=write   │
  │    pipe2(pipefd, O_NONBLOCK|O_CLOEXEC)    con flags (Linux)  │
  │                                                              │
  │  Patrón fundamental:                                         │
  │    pipe(pipefd);                                             │
  │    fork();                                                   │
  │    if child: close(pipefd[1]); read(pipefd[0]);              │
  │    if parent: close(pipefd[0]); write(pipefd[1]);            │
  │                                                              │
  │  Cierre de extremos (CRÍTICO):                               │
  │    Cada proceso cierra lo que NO usa, justo después de fork  │
  │    Si lector no cierra write end → nunca ve EOF              │
  │    Si escritor no cierra read end → no ve SIGPIPE/EPIPE      │
  │                                                              │
  │  Señales del lifecycle:                                      │
  │    Todos los writers cierran → read() retorna 0 (EOF)        │
  │    Todos los readers cierran → write() → SIGPIPE / EPIPE    │
  │                                                              │
  │  Capacidad:                                                  │
  │    Default 65536 bytes (Linux)                               │
  │    fcntl(fd, F_GETPIPE_SZ)     leer capacidad               │
  │    fcntl(fd, F_SETPIPE_SZ, n)  cambiar (hasta pipe-max-size)│
  │                                                              │
  │  Atomicidad:                                                 │
  │    write ≤ PIPE_BUF (4096) → atómico (no se intercala)      │
  │    write > PIPE_BUF        → puede intercalarse              │
  │                                                              │
  │  Bloqueo:                                                    │
  │    read en pipe vacío → bloquea                              │
  │    write en pipe lleno → bloquea                             │
  │    O_NONBLOCK → EAGAIN en vez de bloquear                   │
  │                                                              │
  │  Bidireccional:                                              │
  │    Dos pipes (4 fds)                                         │
  │    O socketpair(AF_UNIX, SOCK_STREAM, 0, sv) → 2 fds        │
  │                                                              │
  │  pipe2 flags: O_NONBLOCK, O_CLOEXEC, O_DIRECT               │
  │  O_DIRECT: preserva fronteras de mensajes (Linux 3.4+)      │
  └──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Pipeline de procesos

Implementa una función que conecte N procesos en serie con pipes,
simulando `cmd1 | cmd2 | cmd3`:

```c
typedef struct {
    const char *path;
    char *const *argv;
} command_t;

int run_pipeline(command_t *cmds, int n);
```

Requisitos:
1. Crear N-1 pipes.
2. Fork N hijos, cada uno con stdin/stdout redirigido al pipe adecuado
   (usar `dup2`).
3. El primer comando lee de stdin real, el último escribe a stdout real.
4. Cada hijo cierra **todos** los extremos de pipe que no usa.
5. El padre cierra **todos** los extremos y espera a todos los hijos.
6. Retornar el exit code del **último** comando.

Probar con:
```c
command_t cmds[] = {
    {"ls",   (char *[]){"ls", "/usr/bin", NULL}},
    {"grep", (char *[]){"grep", "python", NULL}},
    {"wc",   (char *[]){"wc", "-l", NULL}},
};
int code = run_pipeline(cmds, 3);
```

**Predicción antes de codificar**: Si tienes 3 comandos, ¿cuántos pipes
creas? ¿Cuántos fds tiene el padre abiertos antes de empezar a cerrar?

**Pregunta de reflexión**: ¿Por qué el padre debe cerrar todos los
extremos de todos los pipes, no solo los suyos? ¿Qué pasa si el padre
deja abierto el write end del pipe entre cmd1 y cmd2?

---

### Ejercicio 2: Process pool con pipe de tareas

Implementa un pool de workers que leen tareas desde un pipe compartido:

```c
#define NUM_WORKERS 4

typedef struct {
    int task_id;
    int duration_ms;
} task_t;
```

Arquitectura:
1. Padre crea un pipe.
2. Fork N workers. Cada worker lee `task_t` del pipe en un loop.
3. El padre escribe tareas al pipe.
4. Cuando acaban las tareas, el padre cierra el write end → workers
   ven EOF y salen.
5. Padre espera a todos los workers.

```
  ┌──────────┐         ┌──────────┐
  │ Parent   │         │ Worker 1 │
  │          │  pipe   │          │
  │  write ──┼────┬───▶│ read     │
  │  tasks   │    │    └──────────┘
  │          │    │    ┌──────────┐
  │          │    ├───▶│ Worker 2 │
  │          │    │    └──────────┘
  │          │    │    ┌──────────┐
  │          │    └───▶│ Worker 3 │
  └──────────┘         └──────────┘
```

**Predicción antes de codificar**: Si escribes un `task_t` de 8 bytes
(que es menor que PIPE_BUF), ¿está garantizado que un solo worker lee
la tarea completa, o podría leer mitad de una tarea?

**Pregunta de reflexión**: ¿Qué pasa si un worker tarda mucho en una
tarea mientras los otros están idle? ¿Este diseño tiene load balancing
automático? ¿Por qué?

---

### Ejercicio 3: Subprocess con captura de stdout y stderr

Implementa una función que ejecute un comando y capture por separado
su stdout y stderr:

```c
typedef struct {
    char  *stdout_buf;   /* Captured stdout (caller frees) */
    size_t stdout_len;
    char  *stderr_buf;   /* Captured stderr (caller frees) */
    size_t stderr_len;
    int    exit_code;
} capture_result_t;

capture_result_t capture_command(const char *cmd, char *const argv[]);
```

Requisitos:
1. Crear dos pipes: uno para stdout, otro para stderr.
2. Fork + exec. El hijo redirige fd 1 al pipe de stdout y fd 2 al
   pipe de stderr.
3. El padre lee ambos pipes con `poll()` hasta EOF en ambos.
4. Acumular los datos en buffers dinámicos (realloc).
5. Esperar al hijo con waitpid.
6. Retornar los buffers y el exit code.

Probar con:
```c
capture_result_t r = capture_command("ls",
    (char *[]){"ls", "/tmp", "/nonexistent", NULL});

printf("stdout (%zu bytes): %s\n", r.stdout_len, r.stdout_buf);
printf("stderr (%zu bytes): %s\n", r.stderr_len, r.stderr_buf);
printf("exit code: %d\n", r.exit_code);

free(r.stdout_buf);
free(r.stderr_buf);
```

**Predicción antes de codificar**: ¿Por qué necesitas `poll()` para leer
los dos pipes? ¿Qué pasa si lees stdout hasta EOF primero y luego
stderr, y el hijo escribe mucho a stderr mientras esperas en stdout?

**Pregunta de reflexión**: `popen()` solo captura stdout (con `"r"`) o
solo escribe a stdin (con `"w"`). ¿Por qué la biblioteca estándar no
ofrece captura de stdout+stderr simultánea?
