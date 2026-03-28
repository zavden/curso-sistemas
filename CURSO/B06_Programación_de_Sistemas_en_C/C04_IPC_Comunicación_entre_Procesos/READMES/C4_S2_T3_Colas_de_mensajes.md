# Colas de mensajes

## Índice
1. [Concepto y motivación](#1-concepto-y-motivación)
2. [POSIX: mq_open y familia](#2-posix-mq_open-y-familia)
3. [Atributos de la cola](#3-atributos-de-la-cola)
4. [Notificación asíncrona: mq_notify](#4-notificación-asíncrona-mq_notify)
5. [System V: msgget, msgsnd, msgrcv](#5-system-v-msgget-msgsnd-msgrcv)
6. [Filtrado por tipo de mensaje (System V)](#6-filtrado-por-tipo-de-mensaje-system-v)
7. [Patrones de uso](#7-patrones-de-uso)
8. [Comparación con pipes y memoria compartida](#8-comparación-con-pipes-y-memoria-compartida)
9. [Herramientas de diagnóstico](#9-herramientas-de-diagnóstico)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Concepto y motivación

Las colas de mensajes son un mecanismo IPC que combina lo mejor de pipes
y memoria compartida:

- Como los pipes: **sincronización incluida** (bloquean al leer de cola
  vacía o escribir en cola llena)
- Como memoria compartida: **comunicación entre procesos no relacionados**
  (identificados por nombre o clave)
- Mejor que ambos: **preservan límites de mensajes** (cada `send` produce
  exactamente un `receive`, sin fragmentación)

```
  Pipes:
  write("Hello") + write("World") → read puede dar "HelloWorld"
  (stream de bytes, sin fronteras)

  Colas de mensajes:
  send("Hello") + send("World") → receive da "Hello", luego "World"
  (mensajes discretos con fronteras preservadas)
```

```
  Proceso A                Cola                    Proceso B
  ┌─────────┐    send()   ┌──┬──┬──┬──┐  recv()  ┌─────────┐
  │ Producer│ ──────────► │M4│M3│M2│M1│ ───────► │ Consumer│
  └─────────┘             └──┴──┴──┴──┘           └─────────┘
                          Cola FIFO en el kernel
                          (o con prioridad en POSIX)
```

Ventajas adicionales:
- **Prioridad** (POSIX): mensajes de alta prioridad se entregan primero
- **Tipos** (System V): el receptor puede filtrar por tipo de mensaje
- **Notificación asíncrona** (POSIX): aviso por señal o thread cuando
  llega un mensaje
- **Persistencia**: la cola persiste independientemente de los procesos

---

## 2. POSIX: mq_open y familia

### 2.1 Crear / abrir

```c
#include <mqueue.h>

mqd_t mq_open(const char *name, int oflag);
mqd_t mq_open(const char *name, int oflag, mode_t mode,
              struct mq_attr *attr);
// name: empieza con '/', sin más '/' (e.g., "/myqueue")
// oflag: O_RDONLY, O_WRONLY, O_RDWR, O_CREAT, O_EXCL, O_NONBLOCK
// mode: permisos (solo con O_CREAT)
// attr: atributos (NULL = defaults del sistema)
// Retorna: descriptor de cola (mqd_t), o (mqd_t)-1 en error
```

```c
// Crear cola con atributos personalizados
struct mq_attr attr = {
    .mq_flags   = 0,          // flags (0 o O_NONBLOCK)
    .mq_maxmsg  = 10,         // máximo de mensajes en la cola
    .mq_msgsize = 256,        // tamaño máximo de cada mensaje (bytes)
    .mq_curmsgs = 0,          // mensajes actuales (ignorado en open)
};

mqd_t mq = mq_open("/myqueue", O_CREAT | O_RDWR, 0666, &attr);
if (mq == (mqd_t)-1) {
    perror("mq_open");
    exit(1);
}

// Abrir existente (otro proceso)
mqd_t mq = mq_open("/myqueue", O_RDONLY);
if (mq == (mqd_t)-1) {
    perror("mq_open");
    exit(1);
}
```

### 2.2 Enviar mensajes

```c
int mq_send(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
            unsigned int msg_prio);
// msg_len: debe ser ≤ mq_msgsize del atributo
// msg_prio: 0-31 (mayor número = mayor prioridad)
// Bloquea si la cola está llena (a menos que O_NONBLOCK)
// Retorna: 0 en éxito, -1 en error
```

```c
const char *msg = "Hello from producer";
if (mq_send(mq, msg, strlen(msg) + 1, 0) == -1) {
    perror("mq_send");
}

// Con prioridad
mq_send(mq, "urgent!", 8, 10);     // prioridad 10
mq_send(mq, "normal", 7, 0);       // prioridad 0
// El receptor recibirá "urgent!" primero
```

### 2.3 Recibir mensajes

```c
ssize_t mq_receive(mqd_t mqdes, char *msg_ptr, size_t msg_len,
                   unsigned int *msg_prio);
// msg_len: debe ser ≥ mq_msgsize (¡no el tamaño del mensaje real!)
// msg_prio: si no es NULL, recibe la prioridad del mensaje
// Bloquea si la cola está vacía (a menos que O_NONBLOCK)
// Retorna: tamaño del mensaje recibido, o -1 en error
```

**Punto crítico**: `msg_len` debe ser ≥ `mq_attr.mq_msgsize`, no el
tamaño que esperas recibir. Si pasas un buffer más pequeño, `mq_receive`
falla con `EMSGSIZE`.

```c
struct mq_attr attr;
mq_getattr(mq, &attr);

char *buf = malloc(attr.mq_msgsize);
unsigned int prio;

ssize_t n = mq_receive(mq, buf, attr.mq_msgsize, &prio);
if (n == -1) {
    perror("mq_receive");
} else {
    printf("Received [prio=%u, %zd bytes]: %s\n", prio, n, buf);
}
free(buf);
```

### 2.4 Operaciones con timeout

```c
#include <time.h>

int mq_timedsend(mqd_t mqdes, const char *msg_ptr, size_t msg_len,
                 unsigned int msg_prio, const struct timespec *abs_timeout);

ssize_t mq_timedreceive(mqd_t mqdes, char *msg_ptr, size_t msg_len,
                        unsigned int *msg_prio,
                        const struct timespec *abs_timeout);
```

El timeout es **absoluto** (tiempo del reloj, no relativo):

```c
struct timespec ts;
clock_gettime(CLOCK_REALTIME, &ts);
ts.tv_sec += 5;  // 5 segundos desde ahora

ssize_t n = mq_timedreceive(mq, buf, attr.mq_msgsize, &prio, &ts);
if (n == -1 && errno == ETIMEDOUT) {
    printf("No message in 5 seconds\n");
}
```

### 2.5 Cerrar y eliminar

```c
int mq_close(mqd_t mqdes);        // cerrar en este proceso
int mq_unlink(const char *name);  // eliminar del filesystem
```

Como con archivos: `mq_unlink` elimina el nombre pero la cola se
destruye solo cuando todos los procesos la cierren.

```c
mq_close(mq);
mq_unlink("/myqueue");
```

### 2.6 Ejemplo completo POSIX

**Productor**:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>

#define QUEUE_NAME "/demo_queue"
#define MAX_MSG_SIZE 256
#define MAX_MSGS 10

int main(void)
{
    struct mq_attr attr = {
        .mq_maxmsg  = MAX_MSGS,
        .mq_msgsize = MAX_MSG_SIZE,
    };

    mqd_t mq = mq_open(QUEUE_NAME, O_CREAT | O_WRONLY, 0666, &attr);
    if (mq == (mqd_t)-1) { perror("mq_open"); exit(1); }

    for (int i = 1; i <= 5; i++) {
        char msg[MAX_MSG_SIZE];
        snprintf(msg, sizeof(msg), "Message %d from PID %d", i, getpid());

        unsigned int prio = (i == 5) ? 5 : 0;  // último con prioridad alta
        if (mq_send(mq, msg, strlen(msg) + 1, prio) == -1) {
            perror("mq_send");
            break;
        }
        printf("Sent [prio=%u]: %s\n", prio, msg);
    }

    mq_close(mq);
    return 0;
}
```

**Consumidor**:

```c
#include <stdio.h>
#include <stdlib.h>
#include <mqueue.h>
#include <fcntl.h>

#define QUEUE_NAME "/demo_queue"

int main(void)
{
    mqd_t mq = mq_open(QUEUE_NAME, O_RDONLY);
    if (mq == (mqd_t)-1) { perror("mq_open"); exit(1); }

    struct mq_attr attr;
    mq_getattr(mq, &attr);

    char *buf = malloc(attr.mq_msgsize);

    printf("Waiting for messages (queue has %ld)...\n", attr.mq_curmsgs);

    for (int i = 0; i < 5; i++) {
        unsigned int prio;
        ssize_t n = mq_receive(mq, buf, attr.mq_msgsize, &prio);
        if (n == -1) { perror("mq_receive"); break; }
        printf("Received [prio=%u, %zd bytes]: %s\n", prio, n, buf);
    }

    free(buf);
    mq_close(mq);
    mq_unlink(QUEUE_NAME);
    return 0;
}
```

Compilar con `-lrt`:

```bash
gcc -o producer producer.c -lrt
gcc -o consumer consumer.c -lrt
```

Salida del consumidor (nota el orden por prioridad):

```
Received [prio=5]: Message 5 from PID 1234     ← prioridad alta primero
Received [prio=0]: Message 1 from PID 1234
Received [prio=0]: Message 2 from PID 1234
Received [prio=0]: Message 3 from PID 1234
Received [prio=0]: Message 4 from PID 1234
```

---

## 3. Atributos de la cola

```c
int mq_getattr(mqd_t mqdes, struct mq_attr *attr);
int mq_setattr(mqd_t mqdes, const struct mq_attr *newattr,
               struct mq_attr *oldattr);
```

`mq_setattr` solo puede modificar `mq_flags` (activar/desactivar
`O_NONBLOCK`). Los otros campos (`mq_maxmsg`, `mq_msgsize`) son
inmutables después de crear la cola.

```c
struct mq_attr attr;
mq_getattr(mq, &attr);

printf("Flags:    %ld\n", attr.mq_flags);     // 0 o O_NONBLOCK
printf("Max msgs: %ld\n", attr.mq_maxmsg);    // capacidad
printf("Msg size: %ld\n", attr.mq_msgsize);   // bytes por mensaje
printf("Cur msgs: %ld\n", attr.mq_curmsgs);   // mensajes pendientes
```

### Cambiar a no-bloqueante dinámicamente

```c
struct mq_attr new_attr = {.mq_flags = O_NONBLOCK};
struct mq_attr old_attr;
mq_setattr(mq, &new_attr, &old_attr);

// Ahora mq_send/mq_receive retornan EAGAIN en vez de bloquear
ssize_t n = mq_receive(mq, buf, msgsize, NULL);
if (n == -1 && errno == EAGAIN) {
    printf("Cola vacía (no-bloqueante)\n");
}

// Restaurar modo bloqueante
mq_setattr(mq, &old_attr, NULL);
```

### Límites del sistema (Linux)

```bash
$ cat /proc/sys/fs/mqueue/msg_max        # max mensajes por cola (default 10)
$ cat /proc/sys/fs/mqueue/msgsize_max    # max bytes por mensaje (default 8192)
$ cat /proc/sys/fs/mqueue/queues_max     # max colas en el sistema (default 256)
```

Un usuario no privilegiado no puede crear colas con `mq_maxmsg` mayor
que `msg_max`. Root puede superar el límite. Esto es una fuente frecuente
de errores:

```c
// Falla si msg_max del sistema es 10
struct mq_attr attr = {.mq_maxmsg = 1000, .mq_msgsize = 4096};
mqd_t mq = mq_open("/big", O_CREAT | O_RDWR, 0666, &attr);
// errno = EINVAL si no eres root
```

### Filesystem virtual

En Linux, las colas POSIX viven en un filesystem especial que se monta
normalmente en `/dev/mqueue`:

```bash
$ mount | grep mqueue
mqueue on /dev/mqueue type mqueue (rw,nosuid,nodev,noexec,relatime)

$ ls -la /dev/mqueue/
-rw-r--r-- 1 user user 80 Mar 20 10:00 demo_queue

$ cat /dev/mqueue/demo_queue
QSIZE:128        NOTIFY:0     SIGNO:0     NOTIFY_PID:0
```

`QSIZE` es el total de bytes en la cola. El archivo en `/dev/mqueue/`
también se puede eliminar con `rm` (equivalente a `mq_unlink`).

---

## 4. Notificación asíncrona: mq_notify

En lugar de bloquear en `mq_receive`, puedes pedir que el kernel te
avise cuando llega un mensaje a una cola vacía:

```c
int mq_notify(mqd_t mqdes, const struct sigevent *sevp);
```

El `sigevent` determina cómo se entrega la notificación:

```
┌─────────────────────┬──────────────────────────────────────────┐
│ sigev_notify         │ Comportamiento                         │
├─────────────────────┼──────────────────────────────────────────┤
│ SIGEV_SIGNAL        │ Enviar una señal (sigev_signo)          │
│ SIGEV_THREAD        │ Crear un thread que ejecute una función │
│ SIGEV_NONE          │ No hacer nada (registrar sin efecto)    │
└─────────────────────┴──────────────────────────────────────────┘
```

**Reglas importantes**:
1. Solo se notifica cuando la cola pasa de **vacía a no vacía** (0→1
   mensaje). No se notifica si la cola ya tenía mensajes.
2. La notificación es **one-shot**: después de recibirla, debes volver
   a registrarte con `mq_notify`.
3. Solo **un proceso** puede registrarse para notificación por cola.

### 4.1 Notificación por señal

```c
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <mqueue.h>
#include <fcntl.h>
#include <unistd.h>

#define QUEUE_NAME "/notify_demo"

static mqd_t mq;

void handler(int sig, siginfo_t *info, void *context)
{
    // Re-registrar notificación (one-shot)
    struct sigevent sev = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo  = SIGUSR1,
    };
    mq_notify(mq, &sev);

    // Drenar todos los mensajes disponibles
    struct mq_attr attr;
    mq_getattr(mq, &attr);

    char *buf = malloc(attr.mq_msgsize);
    unsigned int prio;
    ssize_t n;

    while ((n = mq_receive(mq, buf, attr.mq_msgsize, &prio)) > 0) {
        // write es async-signal-safe, printf no lo es
        write(STDOUT_FILENO, "Got: ", 5);
        write(STDOUT_FILENO, buf, n - 1);
        write(STDOUT_FILENO, "\n", 1);
    }
    free(buf);
}

int main(void)
{
    struct mq_attr attr = {.mq_maxmsg = 10, .mq_msgsize = 256};
    mq = mq_open(QUEUE_NAME, O_CREAT | O_RDONLY | O_NONBLOCK, 0666, &attr);
    if (mq == (mqd_t)-1) { perror("mq_open"); exit(1); }

    // Instalar handler para SIGUSR1
    struct sigaction sa = {
        .sa_sigaction = handler,
        .sa_flags     = SA_SIGINFO,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    // Registrar notificación
    struct sigevent sev = {
        .sigev_notify = SIGEV_SIGNAL,
        .sigev_signo  = SIGUSR1,
    };
    if (mq_notify(mq, &sev) == -1) {
        perror("mq_notify");
        exit(1);
    }

    printf("Waiting for messages on %s (PID %d)...\n", QUEUE_NAME, getpid());

    // Hacer otro trabajo o simplemente esperar
    while (1)
        pause();

    mq_close(mq);
    mq_unlink(QUEUE_NAME);
    return 0;
}
```

### 4.2 Notificación por thread

Más simple y seguro que señales — la función se ejecuta en un nuevo
thread:

```c
static void notify_thread(union sigval sv)
{
    mqd_t mq = *((mqd_t *)sv.sival_ptr);

    // Re-registrar (one-shot)
    struct sigevent sev = {
        .sigev_notify          = SIGEV_THREAD,
        .sigev_notify_function = notify_thread,
        .sigev_value.sival_ptr = sv.sival_ptr,
    };
    mq_notify(mq, &sev);

    // Leer mensajes (podemos usar printf aquí, no es un signal handler)
    struct mq_attr attr;
    mq_getattr(mq, &attr);
    char *buf = malloc(attr.mq_msgsize);
    unsigned int prio;
    ssize_t n;

    while ((n = mq_receive(mq, buf, attr.mq_msgsize, &prio)) > 0) {
        printf("Thread notify [prio=%u]: %.*s\n", prio, (int)(n - 1), buf);
    }
    free(buf);
}

int main(void)
{
    // ... abrir cola con O_NONBLOCK ...

    struct sigevent sev = {
        .sigev_notify          = SIGEV_THREAD,
        .sigev_notify_function = notify_thread,
        .sigev_value.sival_ptr = &mq,  // pasar el descriptor
    };
    mq_notify(mq, &sev);

    // Hacer otro trabajo...
    while (1)
        sleep(60);
}
```

### 4.3 Integración con epoll (Linux)

En Linux, `mqd_t` es un file descriptor. Se puede usar directamente
con `epoll`/`poll`/`select`:

```c
#include <sys/epoll.h>

mqd_t mq = mq_open("/myqueue", O_RDONLY | O_NONBLOCK);

int epfd = epoll_create1(0);
struct epoll_event ev = {
    .events = EPOLLIN,
    .data.fd = mq,  // funciona porque mqd_t = int en Linux
};
epoll_ctl(epfd, EPOLL_CTL_ADD, mq, &ev);

// Event loop
struct epoll_event events[10];
while (1) {
    int n = epoll_wait(epfd, events, 10, -1);
    for (int i = 0; i < n; i++) {
        if (events[i].data.fd == mq) {
            // Leer mensaje(s)
            char buf[256];
            ssize_t bytes;
            while ((bytes = mq_receive(mq, buf, sizeof(buf), NULL)) > 0) {
                printf("Got: %.*s\n", (int)bytes, buf);
            }
        }
    }
}
```

**Nota**: esto es una extensión de Linux. POSIX no garantiza que `mqd_t`
sea un file descriptor. En sistemas portátiles, usar `mq_notify` o
bloquear en `mq_receive`.

---

## 5. System V: msgget, msgsnd, msgrcv

### 5.1 Crear / obtener cola

```c
#include <sys/msg.h>

int msgget(key_t key, int msgflg);
// Retorna: msqid, o -1 en error
```

```c
key_t key = ftok("/tmp/msgfile", 'Q');
int msqid = msgget(key, IPC_CREAT | 0666);
if (msqid == -1) { perror("msgget"); exit(1); }
```

### 5.2 Estructura del mensaje

System V requiere que los mensajes tengan un `long` al inicio que
indica el **tipo**:

```c
struct msgbuf {
    long mtype;       // tipo de mensaje (debe ser > 0)
    char mtext[256];  // datos (tamaño variable)
};
```

El tipo es obligatorio y debe ser > 0. Es la base del sistema de
filtrado de System V.

### 5.3 Enviar

```c
int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg);
// msgp: puntero al mensaje (debe tener long mtype al inicio)
// msgsz: tamaño de los DATOS (sin contar mtype)
// msgflg: 0 (bloquear si lleno) o IPC_NOWAIT
```

```c
struct msgbuf msg;
msg.mtype = 1;  // tipo 1
snprintf(msg.mtext, sizeof(msg.mtext), "Hello from %d", getpid());

// msgsz = tamaño de mtext, NO sizeof(struct msgbuf)
if (msgsnd(msqid, &msg, strlen(msg.mtext) + 1, 0) == -1) {
    perror("msgsnd");
}
```

### 5.4 Recibir

```c
ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp,
               int msgflg);
// msgtyp: filtro por tipo de mensaje (ver sección 6)
// Retorna: bytes recibidos (sin contar mtype), o -1 en error
```

```c
struct msgbuf msg;
ssize_t n = msgrcv(msqid, &msg, sizeof(msg.mtext), 0, 0);
if (n == -1) { perror("msgrcv"); exit(1); }
printf("Type: %ld, Message: %s\n", msg.mtype, msg.mtext);
```

### 5.5 Control

```c
int msgctl(int msqid, int cmd, struct msqid_ds *buf);
// cmd: IPC_STAT, IPC_SET, IPC_RMID
```

```c
// Obtener información
struct msqid_ds info;
msgctl(msqid, IPC_STAT, &info);
printf("Messages in queue: %lu\n", info.msg_qnum);
printf("Max bytes in queue: %lu\n", info.msg_qbytes);

// Eliminar la cola
msgctl(msqid, IPC_RMID, NULL);
```

---

## 6. Filtrado por tipo de mensaje (System V)

La killer feature de las colas System V es el filtrado por tipo en
`msgrcv`. El parámetro `msgtyp` controla qué mensajes se reciben:

```
┌──────────────┬──────────────────────────────────────────────────┐
│ msgtyp       │ Comportamiento                                  │
├──────────────┼──────────────────────────────────────────────────┤
│ 0            │ Recibir el primer mensaje (FIFO, cualquier tipo)│
├──────────────┼──────────────────────────────────────────────────┤
│ > 0          │ Recibir el primer mensaje de tipo == msgtyp     │
├──────────────┼──────────────────────────────────────────────────┤
│ < 0          │ Recibir el primer mensaje con tipo ≤ |msgtyp|   │
│              │ (el de menor tipo primero — prioridad)          │
└──────────────┴──────────────────────────────────────────────────┘
```

### Multiplexación por tipo

Un servidor puede usar el tipo para distinguir clientes:

```
  Cliente 1 (PID 1001)         Servidor              Cliente 2 (PID 1002)
  ┌──────────┐                 ┌──────────┐           ┌──────────┐
  │ send     │   type=1        │          │  type=1   │ send     │
  │ type=1   │ ──────────────► │ recv     │ ◄──────── │ type=1   │
  │          │                 │ type=0   │           │          │
  │ recv     │   type=1001     │          │ type=1002 │ recv     │
  │ type=1001│ ◄────────────── │ send     │ ────────► │ type=1002│
  └──────────┘                 │ type=PID │           └──────────┘
                               └──────────┘
```

```c
// Protocolo: peticiones con type=1, respuestas con type=PID del cliente
struct request {
    long mtype;          // siempre 1 (petición al servidor)
    pid_t client_pid;    // para que el servidor sepa a quién responder
    char command[240];
};

struct response {
    long mtype;          // PID del cliente destino
    int status;
    char result[248];
};

// Servidor: recibe peticiones tipo 1, responde con tipo = PID cliente
void server_loop(int msqid)
{
    struct request req;
    struct response resp;

    while (1) {
        // Recibir cualquier petición (tipo 1)
        if (msgrcv(msqid, &req, sizeof(req) - sizeof(long), 1, 0) == -1)
            break;

        printf("Request from PID %d: %s\n", req.client_pid, req.command);

        // Procesar y responder
        resp.mtype = req.client_pid;  // dirigir al cliente correcto
        resp.status = 0;
        snprintf(resp.result, sizeof(resp.result), "OK: %s", req.command);

        msgsnd(msqid, &resp, sizeof(resp) - sizeof(long), 0);
    }
}

// Cliente: envía petición tipo 1, recibe respuesta tipo = su PID
void client_request(int msqid, const char *cmd)
{
    struct request req = {.mtype = 1, .client_pid = getpid()};
    strncpy(req.command, cmd, sizeof(req.command) - 1);
    msgsnd(msqid, &req, sizeof(req) - sizeof(long), 0);

    // Recibir SOLO la respuesta para este cliente
    struct response resp;
    msgrcv(msqid, &resp, sizeof(resp) - sizeof(long), getpid(), 0);
    printf("Response [status=%d]: %s\n", resp.status, resp.result);
}
```

### Prioridad con msgtyp negativo

```c
// Mensajes en cola: tipo 1, tipo 3, tipo 2, tipo 5

// msgtyp = -3: recibir el mensaje con menor tipo ≤ 3
msgrcv(msqid, &msg, size, -3, 0);  // recibe tipo 1 (menor ≤ 3)
msgrcv(msqid, &msg, size, -3, 0);  // recibe tipo 2
msgrcv(msqid, &msg, size, -3, 0);  // recibe tipo 3
msgrcv(msqid, &msg, size, -3, 0);  // bloquea (tipo 5 > 3)
```

---

## 7. Patrones de uso

### 7.1 Cola de trabajo (job queue)

Múltiples productores encolan tareas, múltiples consumidores las
procesan:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mqueue.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#define QUEUE_NAME "/job_queue"
#define MAX_JOBS 100
#define JOB_SIZE 512

typedef struct {
    int job_id;
    int priority;          // 0=normal, 1=high, 2=critical
    char payload[JOB_SIZE - 2 * sizeof(int)];
} job_t;

// Worker: consume trabajos
void worker(int id)
{
    mqd_t mq = mq_open(QUEUE_NAME, O_RDONLY);
    if (mq == (mqd_t)-1) { perror("mq_open"); _exit(1); }

    struct mq_attr attr;
    mq_getattr(mq, &attr);
    char *buf = malloc(attr.mq_msgsize);

    while (1) {
        unsigned int prio;
        ssize_t n = mq_receive(mq, buf, attr.mq_msgsize, &prio);
        if (n == -1) break;

        job_t *job = (job_t *)buf;
        printf("Worker %d: job #%d [prio=%u]: %s\n",
               id, job->job_id, prio, job->payload);
        usleep(100000);  // simular trabajo
    }

    free(buf);
    mq_close(mq);
}

// Dispatcher: encola trabajos
void enqueue_job(mqd_t mq, int job_id, int priority, const char *payload)
{
    job_t job = {.job_id = job_id, .priority = priority};
    strncpy(job.payload, payload, sizeof(job.payload) - 1);

    if (mq_send(mq, (char *)&job, sizeof(job), priority) == -1) {
        perror("mq_send");
    }
}
```

### 7.2 Request-Reply con dos colas

Patrón limpio para comunicación bidireccional:

```
  Cliente                                         Servidor
  ┌──────────┐    /request_queue     ┌──────────────┐
  │          │ ─────────────────────►│              │
  │ mq_send  │                       │ mq_receive   │
  │          │    /reply_queue       │              │
  │ mq_recv  │ ◄─────────────────── │ mq_send      │
  └──────────┘                       └──────────────┘
```

```c
#define REQ_QUEUE "/req_queue"
#define REP_QUEUE "/rep_queue"

// Servidor
void server(void)
{
    struct mq_attr attr = {.mq_maxmsg = 10, .mq_msgsize = 256};
    mqd_t req_mq = mq_open(REQ_QUEUE, O_CREAT | O_RDONLY, 0666, &attr);
    mqd_t rep_mq = mq_open(REP_QUEUE, O_CREAT | O_WRONLY, 0666, &attr);

    char buf[256];
    while (1) {
        ssize_t n = mq_receive(req_mq, buf, 256, NULL);
        if (n <= 0) break;

        printf("Server got: %s\n", buf);

        // Procesar y responder
        char reply[256];
        snprintf(reply, sizeof(reply), "Processed: %s", buf);
        mq_send(rep_mq, reply, strlen(reply) + 1, 0);
    }

    mq_close(req_mq); mq_close(rep_mq);
    mq_unlink(REQ_QUEUE); mq_unlink(REP_QUEUE);
}

// Cliente
void client(const char *request)
{
    mqd_t req_mq = mq_open(REQ_QUEUE, O_WRONLY);
    mqd_t rep_mq = mq_open(REP_QUEUE, O_RDONLY);

    mq_send(req_mq, request, strlen(request) + 1, 0);

    char reply[256];
    mq_receive(rep_mq, reply, 256, NULL);
    printf("Client got reply: %s\n", reply);

    mq_close(req_mq); mq_close(rep_mq);
}
```

**Limitación**: con un solo reply queue, las respuestas de múltiples
clientes se mezclan. Soluciones:
- Una cola de respuesta por cliente (`/reply_PID`)
- Usar el filtrado por tipo de System V
- Incluir un ID de correlación en el mensaje

### 7.3 Mensajes estructurados con header

```c
typedef enum {
    MSG_PING = 1,
    MSG_DATA,
    MSG_SHUTDOWN,
} msg_type_t;

typedef struct {
    msg_type_t type;
    uint32_t sequence;
    uint32_t payload_len;
    pid_t sender;
    char payload[];  // flexible array member
} msg_header_t;

void send_message(mqd_t mq, msg_type_t type, uint32_t seq,
                  const void *data, size_t len)
{
    size_t total = sizeof(msg_header_t) + len;
    msg_header_t *msg = malloc(total);
    msg->type = type;
    msg->sequence = seq;
    msg->payload_len = len;
    msg->sender = getpid();
    if (len > 0) memcpy(msg->payload, data, len);

    mq_send(mq, (char *)msg, total, (type == MSG_SHUTDOWN) ? 10 : 0);
    free(msg);
}
```

---

## 8. Comparación con pipes y memoria compartida

```
┌──────────────────┬───────────┬──────────────┬──────────────────┐
│ Aspecto          │ Pipe/FIFO │ Mem. compart. │ Cola de mensajes│
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Datos            │ Stream    │ Raw memory   │ Mensajes discretos│
│                  │ (bytes)   │              │                  │
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Fronteras        │ No        │ No (manual)  │ Sí (automáticas) │
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Sincronización   │ Implícita │ Ninguna      │ Implícita        │
│                  │ (bloqueo) │ (manual)     │ (bloqueo)        │
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Velocidad        │ Media     │ Máxima       │ Media            │
│                  │ (2 copias)│ (0 copias)   │ (2 copias)       │
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Dirección        │ Unidirec. │ Bidirec.     │ Bidireccional    │
│                  │           │              │ (misma cola)     │
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Prioridad        │ No        │ No           │ Sí (POSIX)       │
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Filtrado         │ No        │ No           │ Sí (SysV tipos)  │
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Notificación     │ poll/epoll│ No           │ mq_notify +      │
│ asíncrona        │           │              │ poll/epoll(Linux)│
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Persistencia     │ No (pipe) │ Sí           │ Sí               │
│                  │ No (FIFO  │              │                  │
│                  │  contenido)│             │                  │
├──────────────────┼───────────┼──────────────┼──────────────────┤
│ Cuándo usar      │ Simple    │ Alto         │ Mensajes         │
│                  │ padre↔hijo│ rendimiento  │ estructurados,   │
│                  │ o pipeline│ datos grandes│ prioridad,       │
│                  │           │              │ desacoplamiento  │
└──────────────────┴───────────┴──────────────┴──────────────────┘
```

---

## 9. Herramientas de diagnóstico

### POSIX

```bash
# Listar colas
$ ls -la /dev/mqueue/
-rw-r--r-- 1 user user 80 Mar 20 10:00 demo_queue

# Ver estado de una cola
$ cat /dev/mqueue/demo_queue
QSIZE:42        NOTIFY:0     SIGNO:0     NOTIFY_PID:0

# Eliminar manualmente
$ rm /dev/mqueue/demo_queue

# Límites del sistema
$ ls /proc/sys/fs/mqueue/
msg_default  msg_max  msgsize_default  msgsize_max  queues_max
```

### System V

```bash
# Listar colas
$ ipcs -q
------ Message Queues --------
key        msqid      owner      perms      used-bytes   messages
0x51010105 32768      user       666        128          2

# Detalle
$ ipcs -q -i 32768

# Eliminar
$ ipcrm -q 32768
# o por clave:
$ ipcrm -Q 0x51010105
```

### Monitorizar uso de recursos

```bash
# POSIX: espacio del filesystem
$ df -h /dev/mqueue

# System V: límites
$ ipcs -q -l
------ Messages Limits --------
max queues system wide = 32000
max size of message (bytes) = 8192
default max size of queue (bytes) = 16384
```

---

## 10. Errores comunes

### Error 1: Buffer de recepción menor que mq_msgsize (POSIX)

```c
// MAL: buffer de 64 bytes pero mq_msgsize es 256
mqd_t mq = mq_open("/q", O_RDONLY);
char buf[64];
mq_receive(mq, buf, sizeof(buf), NULL);
// Error: EMSGSIZE — buf debe ser ≥ mq_msgsize, no ≥ mensaje real

// BIEN: siempre usar mq_msgsize
struct mq_attr attr;
mq_getattr(mq, &attr);
char *buf = malloc(attr.mq_msgsize);
mq_receive(mq, buf, attr.mq_msgsize, NULL);
free(buf);
```

**Por qué importa**: el kernel verifica el tamaño del buffer contra
`mq_msgsize` del atributo de la cola, no contra el tamaño real del
mensaje. Aunque el mensaje sea de 10 bytes, si `mq_msgsize=256`, tu
buffer debe ser de al menos 256.

### Error 2: msgsz incluye mtype en System V

```c
// MAL: incluir sizeof(long) en msgsz
struct msgbuf msg = {.mtype = 1};
strcpy(msg.mtext, "hello");
msgsnd(msqid, &msg, sizeof(msg), 0);  // ¡mal! incluye mtype

// BIEN: msgsz es solo el tamaño de los datos, sin mtype
msgsnd(msqid, &msg, sizeof(msg) - sizeof(long), 0);

// O mejor, solo el largo real del texto:
msgsnd(msqid, &msg, strlen(msg.mtext) + 1, 0);
```

**Por qué importa**: si incluyes `sizeof(long)` de más, desperdicias
espacio en la cola. Si el mensaje es grande, la cola se llena antes.
Peor: `msgrcv` también requiere el tamaño sin `mtype`.

### Error 3: mtype = 0 en System V

```c
// MAL: tipo 0 no es válido
struct msgbuf msg = {.mtype = 0};  // ← inválido
msgsnd(msqid, &msg, size, 0);
// Error: EINVAL — mtype debe ser > 0

// BIEN:
struct msgbuf msg = {.mtype = 1};  // ← tipo válido (1+)
```

### Error 4: No re-registrar mq_notify

```c
// MAL: solo recibe UNA notificación
mq_notify(mq, &sev);  // registrar
// ... llega mensaje → notificación
// ... llega otro mensaje → SILENCIO (one-shot agotado)

// BIEN: re-registrar dentro del handler/callback
void notify_handler(union sigval sv)
{
    mqd_t mq = *((mqd_t *)sv.sival_ptr);

    // PRIMERO re-registrar, LUEGO leer
    struct sigevent sev = {
        .sigev_notify          = SIGEV_THREAD,
        .sigev_notify_function = notify_handler,
        .sigev_value           = sv,
    };
    mq_notify(mq, &sev);  // re-registrar antes de mq_receive

    // Ahora leer (puede haber múltiples mensajes)
    // ...
}
```

**Punto sutil**: re-registrar **antes** de `mq_receive`. Si lo haces
después, un mensaje que llegue entre el `receive` y el `notify` se
pierde (no genera notificación porque la cola no estaba vacía al
re-registrar).

### Error 5: No limpiar colas persistentes

```c
// MAL: la cola persiste en /dev/mqueue o en el kernel
mqd_t mq = mq_open("/temp_queue", O_CREAT | O_RDWR, 0666, &attr);
// ... usar ...
mq_close(mq);
// /dev/mqueue/temp_queue sigue existiendo

// BIEN: limpiar con unlink
mq_close(mq);
mq_unlink("/temp_queue");

// O patrón temprano:
mqd_t mq = mq_open("/temp_queue", O_CREAT | O_RDWR, 0666, &attr);
mq_unlink("/temp_queue");  // eliminar nombre ahora
// Cola funciona mientras haya descriptores abiertos
// Se destruye automáticamente al cerrar el último
```

---

## 11. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                    COLAS DE MENSAJES                               │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  POSIX:                                                             │
│    mq = mq_open("/name", O_CREAT|O_RDWR, 0666, &attr)             │
│    mq_send(mq, buf, len, priority)       // enviar                 │
│    n = mq_receive(mq, buf, mq_msgsize, &prio)  // recibir         │
│    mq_timedsend / mq_timedreceive        // con timeout (abs)      │
│    mq_getattr(mq, &attr)                 // leer atributos        │
│    mq_setattr(mq, &new, &old)            // solo mq_flags         │
│    mq_notify(mq, &sigevent)              // notificación (1-shot) │
│    mq_close(mq) / mq_unlink("/name")     // cerrar / eliminar     │
│                                                                     │
│  System V:                                                          │
│    msqid = msgget(key, IPC_CREAT|0666)                             │
│    msgsnd(msqid, &msg, size_sin_mtype, 0)                          │
│    n = msgrcv(msqid, &msg, size, msgtyp, 0)                        │
│      msgtyp=0: FIFO  |  >0: solo tipo  |  <0: menor tipo ≤|typ|  │
│    msgctl(msqid, IPC_RMID, NULL)                                   │
│                                                                     │
│  struct mq_attr:                                                    │
│    mq_maxmsg   — max mensajes en cola  (inmutable post-creación)   │
│    mq_msgsize  — max bytes por mensaje (inmutable post-creación)   │
│    mq_curmsgs  — mensajes actuales     (solo lectura)              │
│    mq_flags    — 0 o O_NONBLOCK        (modificable)              │
│                                                                     │
│  mq_notify (one-shot, re-registrar en callback):                    │
│    SIGEV_SIGNAL — enviar señal                                     │
│    SIGEV_THREAD — crear thread con función                         │
│    Re-registrar ANTES de mq_receive                                │
│                                                                     │
│  Reglas de oro:                                                     │
│    1. Buffer de receive ≥ mq_msgsize (POSIX), no tamaño esperado  │
│    2. msgsz = tamaño SIN mtype (System V)                          │
│    3. mtype > 0 siempre (System V)                                 │
│    4. mq_notify es one-shot: re-registrar cada vez                 │
│    5. Limpiar: mq_unlink / msgctl(IPC_RMID)                       │
│                                                                     │
│  Compilar: gcc -o prog prog.c -lrt                                 │
│  Diagnóstico:                                                       │
│    POSIX: ls /dev/mqueue/ | cat /dev/mqueue/name                   │
│    SysV:  ipcs -q | ipcrm -q <id>                                  │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Sistema de logging centralizado

Implementa un logger centralizado donde múltiples procesos envían
mensajes de log a través de una cola POSIX, y un proceso `log_daemon`
los recibe y escribe a un archivo:

```c
typedef struct {
    int level;          // 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    pid_t sender;
    struct timespec timestamp;
    char message[200];
} log_entry_t;
```

Requisitos:
- El `log_daemon` usa prioridades POSIX: level ERROR se procesa antes
  que INFO
- Soportar al menos 5 procesos productores simultáneos
- `log_daemon` debe manejar SIGTERM para flush + cierre limpio
- Si la cola se llena, los productores usan `mq_timedsend` con
  timeout de 1 segundo e imprimen un warning a stderr

**Pregunta de reflexión**: ¿Qué pasa si el `log_daemon` muere
inesperadamente? Los mensajes encolados se preservan (la cola persiste),
pero ¿qué pasa con los productores que intentan enviar cuando la cola se
llena? ¿Cómo diseñarías el sistema para detectar que el daemon murió?

### Ejercicio 2: RPC simplificado con System V

Implementa un sistema request-reply con colas System V donde los
clientes envían peticiones con tipo=1 y reciben respuestas con
tipo=su PID:

```c
// Operaciones soportadas
typedef enum { OP_ADD, OP_SUB, OP_MUL, OP_DIV } operation_t;

typedef struct {
    long mtype;          // 1 para requests
    pid_t client_pid;
    uint32_t request_id;
    operation_t op;
    double a, b;
} request_t;

typedef struct {
    long mtype;          // PID del cliente
    uint32_t request_id;
    int status;          // 0=ok, -1=error
    double result;
    char error_msg[64];
} response_t;
```

Implementar:
- Servidor que procesa operaciones y maneja división por cero
- Múltiples clientes concurrentes (cada uno con su PID como filtro)
- Timeout de 3 segundos para la respuesta (si el servidor muere)
- `request_id` para correlacionar requests con responses

**Pregunta de reflexión**: El filtrado por tipo de System V escala
linealmente con el número de mensajes en la cola (el kernel recorre
la lista buscando el tipo). ¿A partir de cuántos mensajes esto se
vuelve un problema? ¿Cómo se compara con la alternativa de una cola
de respuesta separada por cliente?

### Ejercicio 3: Pipeline con colas de mensajes

Reimplementa el concepto de un pipeline Unix (`cmd1 | cmd2 | cmd3`)
usando colas de mensajes POSIX en vez de pipes. Cada "etapa" es un
thread que lee de una cola, procesa, y escribe en otra:

```c
// Pipeline: read_file → to_upper → line_number → write_stdout
// Cada etapa conectada por una cola POSIX

typedef struct {
    int sequence;           // número de línea
    int is_eof;             // señal de fin
    char line[480];
} pipeline_msg_t;
```

Implementar:
- `read_file`: lee un archivo línea por línea, envía cada línea como
  mensaje con prioridad 0, al final envía mensaje con `is_eof=1` con
  prioridad máxima (para que se procese último? o primero? Decidir)
- `to_upper`: convierte a mayúsculas
- `line_number`: agrega número de línea al inicio
- `write_stdout`: imprime a pantalla
- Medir throughput y comparar con `cat file | tr a-z A-Z | nl`

**Pregunta de reflexión**: El mensaje EOF con prioridad máxima se
procesaría **antes** que los mensajes de datos pendientes. ¿Qué
prioridad debería tener para que se procese último? ¿Qué pasa si
usas prioridad 0 para todo — se preserva el orden FIFO dentro de
la misma prioridad?
