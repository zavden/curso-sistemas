# Named Pipes (FIFOs) — Comunicación entre Procesos no Relacionados

## Índice

1. [Pipes anónimos vs named pipes](#1-pipes-anónimos-vs-named-pipes)
2. [mkfifo: crear un FIFO](#2-mkfifo-crear-un-fifo)
3. [Abrir un FIFO: semántica de open](#3-abrir-un-fifo-semántica-de-open)
4. [Comunicación entre procesos independientes](#4-comunicación-entre-procesos-independientes)
5. [Múltiples escritores y lectores](#5-múltiples-escritores-y-lectores)
6. [Non-blocking y poll](#6-non-blocking-y-poll)
7. [Persistencia y limpieza](#7-persistencia-y-limpieza)
8. [Patrones de uso](#8-patrones-de-uso)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Pipes anónimos vs named pipes

Un **named pipe** (FIFO) es un pipe que tiene un nombre en el filesystem.
Esto permite que procesos **no relacionados** (que no comparten un
ancestro fork) se comuniquen:

```
  Pipe anónimo:                    Named pipe (FIFO):
  ┌──────────────┐                ┌──────────────┐
  │ Solo entre   │                │ Entre        │
  │ padre/hijo   │                │ CUALQUIER    │
  │ (heredan fd) │                │ proceso      │
  │              │                │              │
  │ Sin nombre   │                │ /tmp/my_fifo │
  │ en filesystem│                │ visible con  │
  │              │                │ ls -l        │
  └──────────────┘                └──────────────┘
```

| Aspecto          | Pipe anónimo        | Named pipe (FIFO)       |
|------------------|---------------------|-------------------------|
| Nombre en filesystem | No              | Sí (ruta)               |
| Procesos         | Relacionados (fork) | Cualquiera              |
| Creación         | `pipe()`            | `mkfifo()`              |
| Persistencia     | Muere con los fds   | Persiste en filesystem  |
| Semántica I/O    | Idéntica            | Idéntica                |
| Capacidad/FIFO   | Igual               | Igual                   |
| Bidireccional    | No                  | No                      |

La semántica de lectura/escritura es idéntica: stream de bytes, FIFO,
buffer en kernel, mismas reglas de EOF y SIGPIPE.

---

## 2. mkfifo: crear un FIFO

### Desde C

```c
#include <sys/stat.h>

int mkfifo(const char *pathname, mode_t mode);
```

- `pathname`: ruta donde crear el FIFO.
- `mode`: permisos (como `open`), modificados por umask.
- Retorna 0 en éxito, -1 en error.

```c
#include <stdio.h>
#include <sys/stat.h>

int main(void)
{
    const char *path = "/tmp/my_fifo";

    if (mkfifo(path, 0666) == -1) {
        perror("mkfifo");
        return 1;
    }

    printf("FIFO created: %s\n", path);
    return 0;
}
```

### Desde la línea de comandos

```bash
$ mkfifo /tmp/my_fifo

$ ls -l /tmp/my_fifo
prw-r--r-- 1 user user 0 mar 20 10:00 /tmp/my_fifo
#^
#└─ 'p' = pipe (FIFO)

$ stat /tmp/my_fifo
  File: /tmp/my_fifo
  Size: 0         Blocks: 0    IO Block: 4096   fifo
```

### Errores de mkfifo

| errno     | Causa                                   |
|-----------|-----------------------------------------|
| `EEXIST`  | Ya existe un archivo con ese nombre     |
| `ENOENT`  | El directorio padre no existe           |
| `EACCES`  | Sin permiso de escritura en el directorio|

### Manejo de EEXIST

```c
if (mkfifo(path, 0666) == -1) {
    if (errno == EEXIST) {
        /* Check if the existing file is actually a FIFO */
        struct stat st;
        if (stat(path, &st) == 0 && S_ISFIFO(st.st_mode)) {
            /* OK — reuse existing FIFO */
        } else {
            fprintf(stderr, "%s exists but is not a FIFO\n", path);
            return 1;
        }
    } else {
        perror("mkfifo");
        return 1;
    }
}
```

---

## 3. Abrir un FIFO: semántica de open

La diferencia clave con archivos regulares: `open()` en un FIFO
**bloquea** hasta que haya un proceso en el otro extremo:

```c
/* Process A: open for reading */
int fd = open("/tmp/my_fifo", O_RDONLY);
/* BLOCKS until some process opens the FIFO for writing */

/* Process B: open for writing */
int fd = open("/tmp/my_fifo", O_WRONLY);
/* BLOCKS until some process opens the FIFO for reading */
```

```
  Proceso A                        Proceso B
  ──────────                       ──────────
  open(RDONLY) ──▶ bloquea...      (no ha abierto aún)

                                   open(WRONLY) ──▶ bloquea...

  ◀── ambos desbloquean ──▶

  Ahora ambos tienen fd abierto y pueden comunicarse.
```

### Abrir con O_RDWR (truco para no bloquear)

```c
/* Opens immediately — both read and write ends satisfied by one process */
int fd = open("/tmp/my_fifo", O_RDWR);
/* Returns immediately even if no one else has opened the FIFO */
```

Esto funciona porque `O_RDWR` satisface ambos requisitos (reader y
writer presentes). Útil para servidores que quieren abrir el FIFO sin
esperar clientes, pero pierde la semántica de EOF (el servidor tiene
su propio write end abierto, así que nunca ve EOF).

### O_NONBLOCK

Con `O_NONBLOCK`, el comportamiento cambia:

| Modo             | Sin O_NONBLOCK       | Con O_NONBLOCK              |
|------------------|----------------------|-----------------------------|
| `O_RDONLY`       | Bloquea hasta writer | Retorna inmediatamente      |
| `O_WRONLY`       | Bloquea hasta reader | Error `ENXIO` si no hay reader|

```c
/* Non-blocking read end: returns fd even without writer */
int fd = open("/tmp/my_fifo", O_RDONLY | O_NONBLOCK);
/* fd is valid. read() will return EAGAIN if no data. */

/* Non-blocking write end: FAILS if no reader */
int fd = open("/tmp/my_fifo", O_WRONLY | O_NONBLOCK);
/* Returns -1, errno = ENXIO if no process has read end open */
```

---

## 4. Comunicación entre procesos independientes

### Writer (un programa)

```c
/* writer.c */
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

int main(void)
{
    const char *fifo = "/tmp/my_fifo";

    printf("opening FIFO for writing...\n");
    int fd = open(fifo, O_WRONLY);
    if (fd == -1) { perror("open"); return 1; }

    printf("connected! sending messages...\n");

    const char *messages[] = {
        "hello from writer",
        "second message",
        "goodbye",
    };

    for (int i = 0; i < 3; i++) {
        write(fd, messages[i], strlen(messages[i]));
        write(fd, "\n", 1);
        sleep(1);
    }

    close(fd);
    printf("writer done\n");
    return 0;
}
```

### Reader (otro programa)

```c
/* reader.c */
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    const char *fifo = "/tmp/my_fifo";

    printf("opening FIFO for reading...\n");
    int fd = open(fifo, O_RDONLY);
    if (fd == -1) { perror("open"); return 1; }

    printf("connected! reading...\n");

    char buf[256];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';
        printf("received: %s", buf);
    }

    printf("writer closed (EOF)\n");
    close(fd);
    return 0;
}
```

```bash
# Terminal 1:
$ mkfifo /tmp/my_fifo
$ ./reader
opening FIFO for reading...
(blocks until writer opens)

# Terminal 2:
$ ./writer
opening FIFO for writing...
connected! sending messages...

# Terminal 1 continues:
connected! reading...
received: hello from writer
received: second message
received: goodbye
writer closed (EOF)
```

### También funciona con herramientas de shell

```bash
# Terminal 1: read from FIFO
cat /tmp/my_fifo

# Terminal 2: write to FIFO
echo "hello from shell" > /tmp/my_fifo

# Or even:
ls -la > /tmp/my_fifo    # In terminal 2
                          # Output appears in terminal 1
```

---

## 5. Múltiples escritores y lectores

### Múltiples escritores, un lector

Esto funciona bien. Las escrituras de hasta PIPE_BUF bytes son atómicas
y no se intercalan:

```
  Writer A ──┐
              │  ┌────────┐
  Writer B ──┼─▶│  FIFO  │──▶ Reader
              │  └────────┘
  Writer C ──┘
```

```c
/* Each writer: */
int fd = open("/tmp/my_fifo", O_WRONLY);
char msg[PIPE_BUF];  /* Up to 4096 bytes */
int len = snprintf(msg, sizeof(msg), "[PID %d] my message\n", getpid());
write(fd, msg, len);  /* Atomic if len <= PIPE_BUF */
close(fd);
```

### El problema de EOF con múltiples escritores

Cuando el **último** escritor cierra, el lector ve EOF. Si los
escritores abren y cierran frecuentemente, el lector ve EOF entre
cada uno:

```c
/* Reader naive: exits on first EOF */
int fd = open(fifo, O_RDONLY);
while ((n = read(fd, buf, sizeof(buf))) > 0)
    process(buf, n);
/* EOF! But another writer might connect soon... */
close(fd);  /* Reader exits — misses future writers */
```

### Solución: re-abrir o usar O_RDWR

```c
/* Solution 1: reopen after EOF */
for (;;) {
    int fd = open(fifo, O_RDONLY);
    if (fd == -1) break;

    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        process(buf, n);

    close(fd);
    /* Loop back to open — blocks until next writer */
}

/* Solution 2: keep write end open (no EOF ever) */
int fd = open(fifo, O_RDWR);  /* Server holds both ends */
/* read() blocks when no data, never returns EOF */
while ((n = read(fd, buf, sizeof(buf))) > 0)
    process(buf, n);
```

### Múltiples lectores

Múltiples lectores del mismo FIFO **compiten** por los datos. Cada
byte es leído por exactamente un lector (no broadcast):

```
                  ┌────────┐
  Writer ──▶│  FIFO  │──┬──▶ Reader A (gets some data)
                  └────────┘──┴──▶ Reader B (gets rest)
```

Esto puede usarse como un mecanismo de load balancing simple, pero
no hay garantía de distribución equitativa.

---

## 6. Non-blocking y poll

### FIFO con poll

```c
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    const char *fifo = "/tmp/my_fifo";

    /* Open non-blocking to avoid blocking on open */
    int fd = open(fifo, O_RDONLY | O_NONBLOCK);
    if (fd == -1) { perror("open"); return 1; }

    struct pollfd pfd = { .fd = fd, .events = POLLIN };

    printf("waiting for data on FIFO...\n");

    for (;;) {
        int ret = poll(&pfd, 1, 5000);  /* 5 second timeout */

        if (ret == 0) {
            printf("timeout, still waiting...\n");
            continue;
        }

        if (pfd.revents & POLLIN) {
            char buf[256];
            ssize_t n = read(fd, buf, sizeof(buf) - 1);

            if (n == 0) {
                /* EOF — writer closed */
                printf("writer disconnected\n");
                /* For persistent server: reopen or use O_RDWR */
                break;
            }

            if (n > 0) {
                buf[n] = '\0';
                printf("data: %s", buf);
            }
        }

        if (pfd.revents & POLLHUP) {
            printf("writer hung up\n");
            break;
        }
    }

    close(fd);
    return 0;
}
```

### POLLHUP en FIFOs

Cuando todos los escritores cierran, `poll` reporta `POLLHUP` en el
read end. Esto es equivalente al EOF de `read()`:

```
  Writers abiertos:   poll → POLLIN cuando hay datos
  Writers cierran:    poll → POLLHUP
```

---

## 7. Persistencia y limpieza

### El FIFO persiste en el filesystem

A diferencia del pipe anónimo, un FIFO **no desaparece** cuando los
procesos terminan. El archivo permanece en el filesystem:

```bash
$ mkfifo /tmp/my_fifo
$ ls -l /tmp/my_fifo
prw-r--r-- 1 user user 0 mar 20 10:00 /tmp/my_fifo

# Processes come and go, FIFO stays

$ rm /tmp/my_fifo   # Must explicitly remove
```

### Limpieza en C

```c
/* Create FIFO */
mkfifo("/tmp/my_fifo", 0666);

/* ... use it ... */

/* Remove when done */
unlink("/tmp/my_fifo");
```

### Limpieza con atexit o signal handlers

```c
static const char *fifo_path = "/tmp/my_fifo";

void cleanup(void)
{
    unlink(fifo_path);
}

int main(void)
{
    mkfifo(fifo_path, 0666);
    atexit(cleanup);

    /* Also handle signals for clean removal */
    struct sigaction sa = { .sa_handler = exit };  /* calls atexit */
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);

    /* ... */
}
```

### Ubicación recomendada

| Ubicación       | Uso                                          |
|-----------------|----------------------------------------------|
| `/tmp/`         | Temporal, cualquier usuario                  |
| `/var/run/`     | Daemons, limpiado en reboot                 |
| `/run/user/UID/`| Per-user runtime, systemd                    |
| Directorio app  | Específico de la aplicación                  |

---

## 8. Patrones de uso

### Patrón 1: Servidor de log

```c
/* log_server.c: reads log messages from FIFO */
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int main(void)
{
    const char *fifo = "/tmp/log_fifo";
    mkfifo(fifo, 0666);

    /* Open O_RDWR so we never get EOF */
    int fd = open(fifo, O_RDWR);

    FILE *logfile = fopen("/var/log/myapp.log", "a");

    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf) - 1)) > 0) {
        buf[n] = '\0';

        time_t now = time(NULL);
        struct tm *tm = localtime(&now);
        char ts[32];
        strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);

        fprintf(logfile, "[%s] %s", ts, buf);
        fflush(logfile);
    }

    fclose(logfile);
    close(fd);
    unlink(fifo);
    return 0;
}
```

```bash
# Any process can send log messages:
echo "user login: admin" > /tmp/log_fifo
echo "error: disk full" > /tmp/log_fifo
```

### Patrón 2: Comando remoto (control FIFO)

```c
/* daemon.c: accepts commands via FIFO */
int fd = open("/tmp/daemon_ctl", O_RDWR);

char cmd[256];
ssize_t n;
while ((n = read(fd, cmd, sizeof(cmd) - 1)) > 0) {
    cmd[n] = '\0';

    /* Strip newline */
    char *nl = strchr(cmd, '\n');
    if (nl) *nl = '\0';

    if (strcmp(cmd, "reload") == 0)
        reload_config();
    else if (strcmp(cmd, "status") == 0)
        print_status();
    else if (strcmp(cmd, "stop") == 0)
        break;
    else
        fprintf(stderr, "unknown command: %s\n", cmd);
}
```

```bash
# Control the daemon from shell:
echo "reload" > /tmp/daemon_ctl
echo "status" > /tmp/daemon_ctl
echo "stop"   > /tmp/daemon_ctl
```

### Patrón 3: Protocolo con longitud prefijada

Como los FIFOs son streams de bytes sin fronteras, necesitas un
protocolo para delimitar mensajes:

```c
/* Protocol: 4-byte length prefix + payload */
typedef struct {
    uint32_t length;    /* Payload length in network byte order */
    char     payload[]; /* Flexible array member */
} message_t;

/* Send */
int send_message(int fd, const char *data, size_t len)
{
    uint32_t net_len = htonl(len);
    if (write(fd, &net_len, 4) != 4) return -1;
    if (write(fd, data, len) != (ssize_t)len) return -1;
    return 0;
}

/* Receive */
int recv_message(int fd, char *buf, size_t bufsize, size_t *out_len)
{
    uint32_t net_len;
    if (read(fd, &net_len, 4) != 4) return -1;

    uint32_t len = ntohl(net_len);
    if (len > bufsize) return -1;

    size_t total = 0;
    while (total < len) {
        ssize_t n = read(fd, buf + total, len - total);
        if (n <= 0) return -1;
        total += n;
    }

    *out_len = len;
    return 0;
}
```

### Patrón 4: Bidireccional con dos FIFOs

```c
/* Server creates two FIFOs */
mkfifo("/tmp/app_request", 0666);   /* Client → Server */
mkfifo("/tmp/app_response", 0666);  /* Server → Client */

/* Server: */
int req_fd = open("/tmp/app_request", O_RDONLY);
int res_fd = open("/tmp/app_response", O_WRONLY);

/* Client: */
int req_fd = open("/tmp/app_request", O_WRONLY);
int res_fd = open("/tmp/app_response", O_RDONLY);
```

```
  Client                              Server
  ┌──────────┐                       ┌──────────┐
  │          │   app_request         │          │
  │    W ────┼───────────────────▶R──┼──        │
  │          │                       │          │
  │          │   app_response        │          │
  │    R ◀───┼───────────────────W───┼──        │
  └──────────┘                       └──────────┘
```

---

## 9. Errores comunes

### Error 1: Bloquear en open sin prever el otro extremo

```c
/* MAL: bloquea para siempre si nadie abre el otro extremo */
int fd = open("/tmp/my_fifo", O_RDONLY);
/* If no process ever opens it for writing → hangs forever */
/* No timeout, no way to cancel (except signal) */

/* BIEN: usar O_NONBLOCK o O_RDWR según el caso */
/* Option A: non-blocking open + poll */
int fd = open("/tmp/my_fifo", O_RDONLY | O_NONBLOCK);

/* Option B: O_RDWR for server pattern */
int fd = open("/tmp/my_fifo", O_RDWR);
```

### Error 2: No manejar EOF en servidor persistente

```c
/* MAL: servidor sale cuando el primer writer cierra */
int fd = open(fifo, O_RDONLY);
while ((n = read(fd, buf, sizeof(buf))) > 0)
    process(buf, n);
/* EOF → loop exits → server stops */
/* Next writer has nobody to talk to */

/* BIEN: reabrir o usar O_RDWR */
/* Option A: O_RDWR — never sees EOF */
int fd = open(fifo, O_RDWR);
while ((n = read(fd, buf, sizeof(buf))) > 0)
    process(buf, n);

/* Option B: reopen loop */
for (;;) {
    int fd = open(fifo, O_RDONLY);
    if (fd == -1) break;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        process(buf, n);
    close(fd);
}
```

### Error 3: No limpiar el FIFO del filesystem

```c
/* MAL: FIFO queda huérfano en /tmp */
mkfifo("/tmp/my_fifo", 0666);
/* Program crashes or exits without unlink */
/* /tmp/my_fifo stays forever */

/* BIEN: cleanup con atexit + señales */
atexit(cleanup);
/* Or use a path in /run that's cleaned on reboot */
mkfifo("/run/myapp/ctl", 0666);
```

### Error 4: Múltiples escritores con mensajes > PIPE_BUF

```c
/* MAL: mensajes largos de múltiples escritores se intercalan */
/* Writer A: */ write(fd, big_msg_a, 8192);  /* > PIPE_BUF */
/* Writer B: */ write(fd, big_msg_b, 8192);  /* > PIPE_BUF */
/* Reader sees: interleaved mess of A and B data */

/* BIEN: mantener mensajes ≤ PIPE_BUF, o serializar con un lock */
/* Option A: short messages */
char msg[PIPE_BUF];
int len = snprintf(msg, sizeof(msg), "PID=%d data=%s\n", getpid(), data);
write(fd, msg, len);  /* Atomic if len <= 4096 */

/* Option B: flock on the FIFO fd */
flock(fd, LOCK_EX);
write(fd, header, header_len);
write(fd, payload, payload_len);
flock(fd, LOCK_UN);
```

### Error 5: Asumir que un FIFO es como un archivo regular

```c
/* MAL: intentar seek en un FIFO */
lseek(fifo_fd, 0, SEEK_SET);
/* Returns -1, errno = ESPIPE — can't seek on pipes/FIFOs */

/* MAL: intentar mmap un FIFO */
mmap(NULL, 4096, PROT_READ, MAP_SHARED, fifo_fd, 0);
/* Returns MAP_FAILED — can't mmap pipes/FIFOs */

/* MAL: stat y esperar un tamaño válido */
struct stat st;
fstat(fifo_fd, &st);
/* st.st_size is always 0 for FIFOs — doesn't reflect buffered data */
```

---

## 10. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │                  Named Pipes (FIFOs)                          │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Crear:                                                      │
  │    mkfifo(path, 0666)        desde C                         │
  │    mkfifo /tmp/my_fifo       desde shell                     │
  │    mkfifoat(dirfd, name, mode)  relativo a directorio        │
  │                                                              │
  │  Abrir (bloqueo por defecto):                                │
  │    open(path, O_RDONLY)      bloquea hasta que haya writer   │
  │    open(path, O_WRONLY)      bloquea hasta que haya reader   │
  │    open(path, O_RDWR)        no bloquea (truco: ambos lados)│
  │                                                              │
  │  O_NONBLOCK:                                                 │
  │    O_RDONLY|O_NONBLOCK       retorna fd sin writer (OK)      │
  │    O_WRONLY|O_NONBLOCK       ENXIO si no hay reader          │
  │                                                              │
  │  Semántica I/O (idéntica a pipe anónimo):                    │
  │    read en vacío → bloquea (o EAGAIN)                        │
  │    write en lleno → bloquea (o EAGAIN)                       │
  │    Todos writers cierran → read retorna 0 (EOF)              │
  │    Todos readers cierran → write → SIGPIPE/EPIPE            │
  │    write ≤ PIPE_BUF (4096) → atómico                        │
  │                                                              │
  │  Servidor persistente:                                       │
  │    O_RDWR → nunca EOF (mantiene write end propio)            │
  │    O reabrir en loop después de EOF                          │
  │                                                              │
  │  Múltiples escritores:                                       │
  │    Funciona bien si mensajes ≤ PIPE_BUF                     │
  │    > PIPE_BUF → intercalamiento                              │
  │                                                              │
  │  Limpieza:                                                   │
  │    unlink(path)    remover del filesystem                    │
  │    atexit(cleanup) + signal handlers                         │
  │                                                              │
  │  No soporta: lseek, mmap, st_size, ftruncate                │
  │                                                              │
  │  Shell:                                                      │
  │    echo "data" > /tmp/fifo    escribir                       │
  │    cat /tmp/fifo              leer                           │
  │    cat < /tmp/fifo            leer (redirección)             │
  └──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Chat entre dos terminales

Implementa un chat bidireccional entre dos procesos usando dos FIFOs:

```bash
# Terminal 1:
$ ./chat alice

# Terminal 2:
$ ./chat bob
```

```c
void chat(const char *my_name);
```

Requisitos:
1. Crear dos FIFOs: `/tmp/chat_a_to_b` y `/tmp/chat_b_to_a`.
2. El primer usuario que ejecuta el programa los crea; el segundo los
   reutiliza.
3. Usar `poll` para vigilar simultáneamente stdin (input del usuario) y
   el FIFO de lectura (mensajes del otro usuario).
4. Mostrar cada mensaje con el nombre del remitente y timestamp.
5. Comando `/quit` para salir limpiamente (enviar notificación al otro
   y limpiar los FIFOs).

```
  [alice] hello bob!
  [bob]   hi alice, how are you?
  [alice] great, thanks!
  [bob]   /quit
  bob has left the chat.
```

**Predicción antes de codificar**: ¿Qué pasa si alice abre el FIFO
para lectura antes de que bob lo abra para escritura? ¿Cómo evitas el
deadlock en el open?

**Pregunta de reflexión**: ¿Por qué los FIFOs son inadecuados para un
chat con más de 2 participantes? ¿Qué mecanismo de IPC sería mejor
para un chat grupal?

---

### Ejercicio 2: Servidor de comandos con FIFO

Implementa un daemon que reciba comandos a través de un FIFO y
responda por otro FIFO:

```c
/* daemon */
void command_server(const char *cmd_fifo, const char *resp_fifo);

/* client */
char *send_command(const char *cmd_fifo, const char *resp_fifo,
                   const char *command);
```

Comandos soportados:
- `uptime` → devuelve cuánto lleva corriendo el daemon.
- `count` → devuelve cuántos comandos ha procesado.
- `date` → devuelve la fecha/hora actual.
- `echo <text>` → devuelve `<text>`.
- `stop` → shutdown limpio.

```bash
$ ./cmd_server &
server listening on /tmp/cmd_fifo

$ ./cmd_client uptime
server uptime: 45 seconds

$ ./cmd_client count
commands processed: 3

$ ./cmd_client echo hello world
hello world

$ ./cmd_client stop
server stopping...
```

**Predicción antes de codificar**: Si dos clientes envían comandos
simultáneamente, ¿qué pasa con las respuestas? ¿Puede el cliente A
recibir la respuesta del cliente B?

**Pregunta de reflexión**: ¿Cómo resolverías el problema de múltiples
clientes simultáneos? (Pista: cada cliente podría crear su propio FIFO
de respuesta e incluir su nombre en el comando.)

---

### Ejercicio 3: Log aggregator con FIFO

Implementa un agregador de logs que reciba mensajes de múltiples
procesos a través de un FIFO y los escriba a un archivo con formato:

```
[2026-03-20 10:15:32] [PID:1234] [INFO] user logged in
[2026-03-20 10:15:33] [PID:1235] [ERROR] database timeout
[2026-03-20 10:15:34] [PID:1234] [DEBUG] query took 42ms
```

Protocolo de mensaje (cabe en PIPE_BUF para atomicidad):
```c
#define MAX_MSG 4000  /* < PIPE_BUF (4096), leaves room for header */

typedef struct {
    char level[8];     /* INFO, WARN, ERROR, DEBUG */
    char message[MAX_MSG];
} log_entry_t;
```

Requisitos:
1. Servidor usa `O_RDWR` para persistir sin EOF.
2. Múltiples clientes envían simultáneamente.
3. Cada mensaje completo (struct) cabe en PIPE_BUF → atómico.
4. Rotación de log: al recibir SIGHUP, cerrar y reabrir el archivo
   de log (permite `mv log.txt log.old && kill -HUP pid`).
5. Estadísticas con SIGUSR1: imprimir conteo por nivel.

**Predicción antes de codificar**: Si un cliente envía un `log_entry_t`
de exactamente `sizeof(log_entry_t)` bytes y eso es < 4096, ¿está
garantizado que el servidor lee la estructura completa en un solo
`read()`, o podría leer la mitad?

**Pregunta de reflexión**: `syslog()`/`journald` resuelven el mismo
problema. ¿Qué ventajas tiene un FIFO custom sobre syslog, y qué
desventajas?
