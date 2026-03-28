# Señales en Tiempo Real — SIGRTMIN-SIGRTMAX, sigqueue y Datos Adjuntos

## Índice

1. [Limitaciones de las señales estándar](#1-limitaciones-de-las-señales-estándar)
2. [Señales de tiempo real: SIGRTMIN a SIGRTMAX](#2-señales-de-tiempo-real-sigrtmin-a-sigrtmax)
3. [Encolamiento garantizado](#3-encolamiento-garantizado)
4. [sigqueue: enviar con datos adjuntos](#4-sigqueue-enviar-con-datos-adjuntos)
5. [Recibir datos con SA_SIGINFO](#5-recibir-datos-con-sa_siginfo)
6. [Orden de entrega](#6-orden-de-entrega)
7. [sigwaitinfo y sigtimedwait](#7-sigwaitinfo-y-sigtimedwait)
8. [Límites del sistema](#8-límites-del-sistema)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Limitaciones de las señales estándar

Las señales estándar (1-31) tienen tres limitaciones fundamentales que
las hacen inadecuadas para comunicación entre procesos confiable:

```
  Problema 1: No se encolan

  SIGUSR1 bloqueada:
    llega #1 → pendiente
    llega #2 → descartada    ← se pierde
    llega #3 → descartada    ← se pierde
    desbloquear → entrega UNA vez

  Problema 2: No llevan datos

    kill(pid, SIGUSR1);
    /* El receptor sabe QUE llegó SIGUSR1,
       pero no sabe POR QUÉ ni con qué contexto */

  Problema 3: Solo ~30 señales, la mayoría con significado fijo

    Solo SIGUSR1 y SIGUSR2 están disponibles
    para uso del programador → insuficiente
```

Las señales de tiempo real resuelven los tres problemas.

---

## 2. Señales de tiempo real: SIGRTMIN a SIGRTMAX

POSIX define un rango de señales de tiempo real (RT signals) desde
`SIGRTMIN` hasta `SIGRTMAX`. En Linux típicamente hay 32 señales RT:

```c
#include <signal.h>
#include <stdio.h>

int main(void)
{
    printf("SIGRTMIN = %d\n", SIGRTMIN);  /* Typically 34 */
    printf("SIGRTMAX = %d\n", SIGRTMAX);  /* Typically 64 */
    printf("available: %d RT signals\n", SIGRTMAX - SIGRTMIN + 1);
    return 0;
}
```

```
  Señales estándar         Señales de tiempo real
  ──────────────────       ──────────────────────
  1  SIGHUP                34  SIGRTMIN+0
  2  SIGINT                35  SIGRTMIN+1
  ...                      36  SIGRTMIN+2
  10 SIGUSR1               ...
  12 SIGUSR2               63  SIGRTMAX-1
  ...                      64  SIGRTMAX
  31 SIGSYS
  ──────────────────       ──────────────────────
  Significado fijo         Sin significado predefinido
  No se encolan            Se encolan
  No llevan datos          Llevan union sigval
```

### Referenciar señales RT

Siempre usar `SIGRTMIN + offset`, nunca números absolutos:

```c
/* BIEN: portable */
#define SIG_NOTIFY   (SIGRTMIN + 0)
#define SIG_RELOAD   (SIGRTMIN + 1)
#define SIG_SHUTDOWN (SIGRTMIN + 2)
#define SIG_STATUS   (SIGRTMIN + 3)

/* MAL: los números absolutos varían entre sistemas */
#define SIG_NOTIFY   34  /* Wrong on some platforms */
```

> **Importante**: `SIGRTMIN` puede no ser una constante en tiempo de
> compilación (en glibc es una llamada a función). No usarlo en
> inicializadores estáticos ni en `switch/case`.

### Acción por defecto

La acción por defecto de todas las señales RT es **terminar el proceso**.
Siempre hay que instalar un handler antes de usarlas.

---

## 3. Encolamiento garantizado

La diferencia fundamental: las señales RT **se encolan**. Si se envían
N instancias mientras la señal está bloqueada, las N se retienen y
entregan al desbloquear:

```c
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define MY_SIG (SIGRTMIN + 0)

volatile sig_atomic_t count = 0;

void handler(int sig)
{
    (void)sig;
    count++;
}

int main(void)
{
    struct sigaction sa = {
        .sa_handler = handler,
        .sa_flags   = SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(MY_SIG, &sa, NULL);

    /* Block the signal */
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, MY_SIG);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    /* Send 5 instances while blocked */
    for (int i = 0; i < 5; i++)
        kill(getpid(), MY_SIG);

    printf("before unblock: count = %d\n", count);
    /* Output: 0 (all blocked) */

    /* Unblock → all 5 are delivered */
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    printf("after unblock: count = %d\n", count);
    /* Output: 5 (all queued and delivered) */

    return 0;
}
```

### Comparación con señal estándar

```
  Misma prueba con SIGUSR1:

  Enviar 5 SIGUSR1 mientras bloqueada
  Desbloquear
  → handler ejecuta 1 vez (las otras 4 se perdieron)

  Misma prueba con SIGRTMIN:

  Enviar 5 SIGRTMIN mientras bloqueada
  Desbloquear
  → handler ejecuta 5 veces (todas encoladas)
```

---

## 4. sigqueue: enviar con datos adjuntos

`kill()` solo envía el número de señal. `sigqueue()` permite adjuntar
un valor:

```c
#include <signal.h>

int sigqueue(pid_t pid, int sig, const union sigval value);
```

```c
union sigval {
    int   sival_int;    /* Integer value */
    void *sival_ptr;    /* Pointer value (only useful within same process) */
};
```

### Enviar un entero

```c
#include <signal.h>
#include <stdio.h>

#define SIG_EVENT (SIGRTMIN + 0)

int send_event(pid_t target, int event_code)
{
    union sigval val;
    val.sival_int = event_code;

    if (sigqueue(target, SIG_EVENT, val) == -1) {
        perror("sigqueue");
        return -1;
    }
    return 0;
}

/* Usage: */
send_event(server_pid, 42);   /* Send event code 42 */
send_event(server_pid, 100);  /* Send event code 100 */
send_event(server_pid, 7);    /* Send event code 7 */
/* All three are queued and delivered in order */
```

### Enviar un puntero (mismo proceso)

```c
/* Only meaningful within the same process (e.g., raise + handler) */
struct request *req = create_request();

union sigval val;
val.sival_ptr = req;
sigqueue(getpid(), SIG_PROCESS, val);

/* Handler can access the pointer: */
void handler(int sig, siginfo_t *info, void *ctx)
{
    struct request *r = info->si_value.sival_ptr;
    /* Process r... */
}
```

> **Cuidado**: los punteros **no tienen sentido** entre procesos
> diferentes porque cada proceso tiene su propio espacio de direcciones.
> Solo usar `sival_int` para IPC entre procesos.

### sigqueue vs kill

| Aspecto            | `kill(pid, sig)`   | `sigqueue(pid, sig, val)` |
|--------------------|--------------------|---------------------------|
| Datos adjuntos     | No                 | Sí (union sigval)         |
| Encolamiento       | No (estándar)      | Sí (RT signals)           |
| Señal 0 (probe)    | Sí                 | Sí                        |
| Enviar a grupo     | Sí (pid negativo)  | No (solo un proceso)      |
| Permisos           | Mismos             | Mismos                    |

---

## 5. Recibir datos con SA_SIGINFO

Para recibir los datos adjuntos, el handler debe usar `SA_SIGINFO` y
la firma extendida:

```c
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define SIG_EVENT (SIGRTMIN + 0)

void event_handler(int sig, siginfo_t *info, void *ucontext)
{
    (void)sig;
    (void)ucontext;

    int event_code = info->si_value.sival_int;
    pid_t sender   = info->si_pid;
    uid_t uid      = info->si_uid;

    /* Write is async-signal-safe */
    char buf[128];
    int len = snprintf(buf, sizeof(buf),
                       "event=%d from PID=%d UID=%d\n",
                       event_code, sender, uid);
    write(STDOUT_FILENO, buf, len);
}

int main(void)
{
    struct sigaction sa = {
        .sa_sigaction = event_handler,  /* Extended handler */
        .sa_flags     = SA_SIGINFO | SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIG_EVENT, &sa, NULL);

    printf("PID %d waiting for events...\n", getpid());

    while (1)
        pause();

    return 0;
}
```

### Campos relevantes de siginfo_t para RT signals

```c
void handler(int sig, siginfo_t *info, void *ctx)
{
    info->si_signo;           /* Signal number */
    info->si_code;            /* How it was sent */
    info->si_pid;             /* Sender PID */
    info->si_uid;             /* Sender real UID */
    info->si_value.sival_int; /* Attached integer */
    info->si_value.sival_ptr; /* Attached pointer */
}
```

### si_code para distinguir el origen

| si_code     | Significado                               |
|-------------|-------------------------------------------|
| `SI_USER`   | Enviada con `kill()` (sin datos adjuntos) |
| `SI_QUEUE`  | Enviada con `sigqueue()` (con datos)      |
| `SI_TIMER`  | Generada por timer (timer_create)         |
| `SI_MESGQ`  | Generada por cola de mensajes POSIX       |
| `SI_ASYNCIO`| Generada por I/O asíncrono                |

```c
void handler(int sig, siginfo_t *info, void *ctx)
{
    (void)sig; (void)ctx;

    if (info->si_code == SI_QUEUE) {
        /* sigqueue: data is valid */
        int data = info->si_value.sival_int;
        process_event(data);
    } else if (info->si_code == SI_USER) {
        /* kill(): no data attached */
        handle_plain_signal();
    }
}
```

---

## 6. Orden de entrega

Cuando varias señales RT están pendientes, POSIX garantiza:

### Regla 1: Señales con número menor se entregan primero

```
  Pendientes: SIGRTMIN+3, SIGRTMIN+0, SIGRTMIN+1

  Orden de entrega:
    1. SIGRTMIN+0
    2. SIGRTMIN+1
    3. SIGRTMIN+3
```

Esto permite asignar **prioridades**: usar `SIGRTMIN+0` para lo más
urgente y números mayores para menos prioritario.

### Regla 2: Misma señal → orden FIFO

```
  Pendientes: SIGRTMIN+0 (data=A), SIGRTMIN+0 (data=B), SIGRTMIN+0 (data=C)

  Orden de entrega:
    1. SIGRTMIN+0 (data=A)  ← primera enviada
    2. SIGRTMIN+0 (data=B)
    3. SIGRTMIN+0 (data=C)  ← última enviada
```

### Regla 3: Señales estándar antes que RT

```
  Pendientes: SIGRTMIN+0, SIGUSR1, SIGRTMIN+2

  Orden de entrega (Linux):
    1. SIGUSR1       ← estándar primero
    2. SIGRTMIN+0    ← RT en orden numérico
    3. SIGRTMIN+2
```

### Ejemplo: verificar el orden

```c
#define SIG_HIGH (SIGRTMIN + 0)  /* Highest priority */
#define SIG_MED  (SIGRTMIN + 1)
#define SIG_LOW  (SIGRTMIN + 2)  /* Lowest priority */

void handler(int sig, siginfo_t *info, void *ctx)
{
    (void)ctx;
    char buf[64];
    int len = snprintf(buf, sizeof(buf),
                       "sig=RTMIN+%d data=%d\n",
                       sig - SIGRTMIN, info->si_value.sival_int);
    write(STDOUT_FILENO, buf, len);
}

int main(void)
{
    /* Install handler for all three */
    struct sigaction sa = {
        .sa_sigaction = handler,
        .sa_flags     = SA_SIGINFO | SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIG_HIGH, &sa, NULL);
    sigaction(SIG_MED, &sa, NULL);
    sigaction(SIG_LOW, &sa, NULL);

    /* Block all three */
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIG_HIGH);
    sigaddset(&mask, SIG_MED);
    sigaddset(&mask, SIG_LOW);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    /* Send in reverse priority order */
    union sigval val;

    val.sival_int = 30;
    sigqueue(getpid(), SIG_LOW, val);

    val.sival_int = 20;
    sigqueue(getpid(), SIG_MED, val);

    val.sival_int = 10;
    sigqueue(getpid(), SIG_HIGH, val);

    /* Unblock → delivered in priority order */
    printf("unblocking...\n");
    sigprocmask(SIG_SETMASK, &oldmask, NULL);

    return 0;
}
```

```
  $ ./rt_order
  unblocking...
  sig=RTMIN+0 data=10
  sig=RTMIN+1 data=20
  sig=RTMIN+2 data=30
```

---

## 7. sigwaitinfo y sigtimedwait

En lugar de handlers asíncronos, se pueden **esperar señales
síncronamente** como si fueran eventos:

### sigwaitinfo

```c
#include <signal.h>

int sigwaitinfo(const sigset_t *set, siginfo_t *info);
```

Bloquea hasta que una señal del conjunto `set` esté pendiente. La
consume y la devuelve **sin ejecutar el handler**:

```c
#define SIG_CMD (SIGRTMIN + 0)

int main(void)
{
    /* Block the signal (REQUIRED for sigwaitinfo) */
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIG_CMD);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    printf("PID %d waiting for commands...\n", getpid());

    while (1) {
        siginfo_t info;
        int sig = sigwaitinfo(&mask, &info);

        if (sig == -1) {
            if (errno == EINTR) continue;
            perror("sigwaitinfo");
            break;
        }

        printf("received signal %d, data=%d, from PID=%d\n",
               sig, info.si_value.sival_int, info.si_pid);

        if (info.si_value.sival_int == -1) {
            printf("shutdown command received\n");
            break;
        }
    }

    return 0;
}
```

### sigtimedwait: con timeout

```c
#include <signal.h>

int sigtimedwait(const sigset_t *set, siginfo_t *info,
                 const struct timespec *timeout);
```

Igual que `sigwaitinfo` pero con timeout:

```c
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIG_CMD);
sigprocmask(SIG_BLOCK, &mask, NULL);

struct timespec timeout = { .tv_sec = 5, .tv_nsec = 0 };
siginfo_t info;

int sig = sigtimedwait(&mask, &info, &timeout);

if (sig == -1) {
    if (errno == EAGAIN)
        printf("timed out after 5 seconds\n");
    else
        perror("sigtimedwait");
} else {
    printf("signal %d received, data=%d\n",
           sig, info.si_value.sival_int);
}
```

### Ventajas sobre handlers

| Aspecto                  | Handler asíncrono       | sigwaitinfo síncrono    |
|--------------------------|-------------------------|-------------------------|
| Seguridad de funciones   | Solo async-signal-safe  | Cualquier función       |
| Control de flujo         | Interrumpe en cualquier punto | Se procesa en el loop |
| Datos adjuntos           | info en handler         | info en retorno         |
| Complejidad              | Flags, race conditions  | Secuencial, simple      |
| Múltiples señales        | Un handler por señal    | Un loop para todas      |

```c
/* Synchronous approach: much simpler */
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGRTMIN + 0);
sigaddset(&mask, SIGRTMIN + 1);
sigaddset(&mask, SIGTERM);
sigprocmask(SIG_BLOCK, &mask, NULL);

while (1) {
    siginfo_t info;
    int sig = sigwaitinfo(&mask, &info);

    switch (sig) {
    case SIGTERM:
        goto cleanup;
    default:
        if (sig >= SIGRTMIN)
            process_event(sig - SIGRTMIN, info.si_value.sival_int);
        break;
    }
}
```

> **Requisito clave**: La señal **debe estar bloqueada** antes de llamar
> a `sigwaitinfo`. Si no lo está, el handler se ejecuta antes de que
> `sigwaitinfo` la vea.

---

## 8. Límites del sistema

### Cola de señales RT pendientes

El kernel tiene un límite de señales encoladas por usuario:

```bash
# Check the limit
ulimit -i
# Typical: 63204

# Or from /proc
cat /proc/sys/kernel/rtsig-max
# (deprecated on modern kernels — use RLIMIT_SIGPENDING)

# Per-process limit
grep "Max pending signals" /proc/self/limits
```

```c
#include <sys/resource.h>

struct rlimit rl;
getrlimit(RLIMIT_SIGPENDING, &rl);
printf("max pending signals: %lu\n", (unsigned long)rl.rlim_cur);
```

### Qué pasa cuando se llena la cola

Si se excede el límite, `sigqueue()` falla con `EAGAIN`:

```c
union sigval val = { .sival_int = 42 };

if (sigqueue(target_pid, SIGRTMIN, val) == -1) {
    if (errno == EAGAIN) {
        /* Queue is full — signal not delivered */
        fprintf(stderr, "signal queue full, dropping event\n");
        /* Retry later or use alternative IPC */
    } else {
        perror("sigqueue");
    }
}
```

### SIGRTMIN reservadas por glibc

En Linux con glibc, los primeros 2-3 valores de `SIGRTMIN` están
**reservados** internamente para NPTL (la implementación de threads):

```c
/* glibc may reserve SIGRTMIN+0, SIGRTMIN+1, SIGRTMIN+2 */
/* __SIGRTMIN (kernel) = 32 */
/* SIGRTMIN (glibc)    = 34  (after reservations) */

/* Always use SIGRTMIN, never __SIGRTMIN or raw numbers */
```

Por eso `SIGRTMIN` es una macro que llama a una función — su valor
depende de cuántas señales reservó la implementación de threads.

---

## 9. Errores comunes

### Error 1: Usar números absolutos en vez de SIGRTMIN+offset

```c
/* MAL: el número 34 no es SIGRTMIN en todas las plataformas */
sigaction(34, &sa, NULL);
kill(pid, 34);

/* BIEN: siempre relativo a SIGRTMIN */
sigaction(SIGRTMIN + 0, &sa, NULL);
kill(pid, SIGRTMIN + 0);
```

`SIGRTMIN` es 34 en Linux x86_64 con glibc, pero puede variar en
otras arquitecturas, otros libc (musl), u otros Unix.

### Error 2: Olvidar SA_SIGINFO al usar sigqueue

```c
/* MAL: handler sin SA_SIGINFO no recibe los datos */
struct sigaction sa = {
    .sa_handler = simple_handler,  /* void handler(int sig) */
    .sa_flags   = SA_RESTART,      /* No SA_SIGINFO! */
};
sigaction(SIGRTMIN, &sa, NULL);

/* sigqueue sends data, but handler can't access it */
union sigval val = { .sival_int = 42 };
sigqueue(getpid(), SIGRTMIN, val);
/* handler(SIGRTMIN) is called, but 42 is lost */

/* BIEN: usar SA_SIGINFO + sa_sigaction */
struct sigaction sa = {
    .sa_sigaction = extended_handler,  /* void handler(int, siginfo_t*, void*) */
    .sa_flags     = SA_SIGINFO | SA_RESTART,
};
sigaction(SIGRTMIN, &sa, NULL);
```

### Error 3: No bloquear la señal antes de sigwaitinfo

```c
/* MAL: señal no bloqueada → handler se ejecuta antes de sigwaitinfo */
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGRTMIN);
/* Missing: sigprocmask(SIG_BLOCK, &mask, NULL); */

siginfo_t info;
sigwaitinfo(&mask, &info);
/* Signal delivered to handler (or default action) instead */

/* BIEN: bloquear primero */
sigprocmask(SIG_BLOCK, &mask, NULL);
sigwaitinfo(&mask, &info);  /* Now works correctly */
```

### Error 4: No manejar EAGAIN en sigqueue

```c
/* MAL: asumir que sigqueue siempre funciona */
for (int i = 0; i < 100000; i++) {
    union sigval val = { .sival_int = i };
    sigqueue(target, SIGRTMIN, val);
    /* May fail silently with EAGAIN if queue is full */
}

/* BIEN: verificar y reintentar */
for (int i = 0; i < 100000; i++) {
    union sigval val = { .sival_int = i };
    while (sigqueue(target, SIGRTMIN, val) == -1) {
        if (errno == EAGAIN) {
            usleep(1000);  /* Queue full — wait and retry */
            continue;
        }
        perror("sigqueue");
        break;
    }
}
```

### Error 5: Usar sival_ptr entre procesos diferentes

```c
/* MAL: puntero no tiene sentido en otro proceso */
struct data *d = malloc(sizeof(*d));
union sigval val = { .sival_ptr = d };
sigqueue(other_pid, SIGRTMIN, val);
/* Receiver gets a pointer into sender's address space
   → segfault or corruption if dereferenced */

/* BIEN: usar sival_int para IPC entre procesos */
union sigval val = { .sival_int = event_id };
sigqueue(other_pid, SIGRTMIN, val);
```

---

## 10. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │              Señales de Tiempo Real                           │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Rango: SIGRTMIN (34) a SIGRTMAX (64) — usar offset:         │
  │    #define MY_SIG (SIGRTMIN + 0)                             │
  │    NUNCA usar números absolutos                              │
  │                                                              │
  │  vs Estándar:                                                │
  │    Estándar: no se encolan, sin datos, ~30 fijas             │
  │    RT:       se encolan, union sigval, ~30 libres            │
  │                                                              │
  │  Enviar:                                                     │
  │    kill(pid, sig)                  sin datos                  │
  │    sigqueue(pid, sig, val)         con sival_int o sival_ptr │
  │      → EAGAIN si cola llena                                  │
  │                                                              │
  │  Recibir asíncrono (handler):                                │
  │    sa.sa_sigaction = handler       (int, siginfo_t*, void*)  │
  │    sa.sa_flags = SA_SIGINFO        obligatorio para datos    │
  │    info->si_value.sival_int        dato adjunto              │
  │    info->si_pid, si_uid            remitente                 │
  │    info->si_code == SI_QUEUE       enviada con sigqueue      │
  │                                                              │
  │  Recibir síncrono (sin handler):                             │
  │    sigprocmask(SIG_BLOCK, ...)     PRIMERO bloquear          │
  │    sigwaitinfo(&mask, &info)       esperar (sin timeout)     │
  │    sigtimedwait(&mask, &info, &ts) esperar con timeout       │
  │      → devuelve número de señal, info tiene los datos        │
  │      → EAGAIN en timeout                                     │
  │                                                              │
  │  Orden de entrega:                                           │
  │    1. Estándar antes que RT                                  │
  │    2. RT menor número primero (SIGRTMIN+0 es más prioritaria)│
  │    3. Misma señal: FIFO                                      │
  │                                                              │
  │  Límites:                                                    │
  │    RLIMIT_SIGPENDING (ulimit -i)  ~63000 por usuario         │
  │    sigqueue EAGAIN si lleno                                  │
  │    SIGRTMIN+0..+2 puede estar reservado por glibc/NPTL       │
  │                                                              │
  │  Default: terminate (siempre instalar handler)               │
  └──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Cola de eventos con señales RT

Implementa un sistema productor-consumidor usando señales RT:

**Productor** (`event_sender`):
```c
int main(int argc, char *argv[])
{
    pid_t target = atoi(argv[1]);

    /* Send various events */
    send_event(target, SIGRTMIN + 0, 1001);  /* Event type 0, id 1001 */
    send_event(target, SIGRTMIN + 0, 1002);
    send_event(target, SIGRTMIN + 1, 2001);  /* Event type 1, id 2001 */
    send_event(target, SIGRTMIN + 0, 1003);
    send_event(target, SIGRTMIN + 2, 3001);  /* Event type 2, id 3001 */
}
```

**Consumidor** (`event_receiver`):
- Usa `sigwaitinfo` (síncrono, no handlers).
- Bloquea SIGRTMIN+0 a SIGRTMIN+2 y SIGTERM.
- Imprime cada evento con tipo, dato, y PID del remitente.
- SIGTERM causa shutdown limpio.

```
  $ ./event_receiver &
  PID 5678 listening for events...

  $ ./event_sender 5678
  (sender exits)

  event_receiver output:
  [type=0] event_id=1001 from PID=5700
  [type=0] event_id=1002 from PID=5700
  [type=0] event_id=1003 from PID=5700
  [type=1] event_id=2001 from PID=5700
  [type=2] event_id=3001 from PID=5700
```

**Predicción antes de codificar**: ¿En qué orden se entregarán los 5
eventos? ¿Los de tipo 0 llegan antes que los de tipo 1 aunque se
enviaron intercalados?

**Pregunta de reflexión**: ¿Qué límite práctico tiene este sistema como
mecanismo de IPC comparado con pipes o colas de mensajes POSIX?

---

### Ejercicio 2: Heartbeat monitor con sigtimedwait

Implementa un monitor que espere heartbeats periódicos de un proceso
hijo. Si no recibe un heartbeat en N segundos, declara al hijo como
muerto:

```c
#define SIG_HEARTBEAT (SIGRTMIN + 0)

void heartbeat_monitor(const char *cmd, char *const argv[],
                       int interval_secs, int max_misses);
```

**Hijo**: cada `interval_secs` envía `sigqueue(ppid, SIG_HEARTBEAT, val)`
donde `val` contiene un contador secuencial.

**Padre (monitor)**:
1. Fork+exec el hijo.
2. Loop con `sigtimedwait` esperando `SIG_HEARTBEAT` con timeout de
   `interval_secs * 2`.
3. Verificar que el contador secuencial es consecutivo (detectar
   heartbeats perdidos).
4. Si se pierden `max_misses` heartbeats consecutivos, matar al hijo
   y relanzar.
5. También manejar SIGCHLD (hijo murió inesperadamente).

```
  $ ./heartbeat_monitor ./my_server
  [monitor] started child PID 1234
  [monitor] heartbeat #1 from child (on time)
  [monitor] heartbeat #2 from child (on time)
  [monitor] heartbeat #3 MISSED (timeout)
  [monitor] heartbeat #4 MISSED (timeout)
  [monitor] heartbeat #5 MISSED → killing child
  [monitor] restarting child...
  [monitor] started child PID 1235
```

**Predicción antes de codificar**: ¿Qué pasa si el hijo envía heartbeats
más rápido que el padre los consume? ¿Se encolan o se pierden?

**Pregunta de reflexión**: Los sistemas de producción (Kubernetes, systemd)
usan mecanismos diferentes para healthchecks (HTTP endpoints, sd_notify).
¿Por qué no usan señales RT?

---

### Ejercicio 3: Dispatcher de prioridades

Implementa un dispatcher que procese eventos de diferentes prioridades
usando el orden de entrega garantizado de señales RT:

```c
#define SIG_CRITICAL (SIGRTMIN + 0)  /* Highest priority */
#define SIG_HIGH     (SIGRTMIN + 1)
#define SIG_NORMAL   (SIGRTMIN + 2)
#define SIG_LOW      (SIGRTMIN + 3)  /* Lowest priority */

typedef void (*event_fn)(int priority, int data);

void priority_dispatcher(event_fn callbacks[4], int duration_secs);
```

Requisitos:
1. Bloquear las 4 señales.
2. Loop con `sigwaitinfo`.
3. Llamar al callback correspondiente con la prioridad y el dato.
4. Medir y reportar latencia de procesamiento por prioridad.
5. Cada 10 eventos, imprimir estadísticas:
   - Eventos procesados por prioridad.
   - Latencia promedio (si se puede medir).

Programa auxiliar de prueba que envíe ráfagas mixtas:
```c
/* Send a burst of mixed-priority events */
for (int i = 0; i < 20; i++) {
    int prio = rand() % 4;
    int sig = SIGRTMIN + prio;
    union sigval val = { .sival_int = i };
    sigqueue(dispatcher_pid, sig, val);
}
```

**Predicción antes de codificar**: Si se envían 5 eventos LOW, 5 NORMAL,
5 HIGH y 5 CRITICAL simultáneamente (todos bloqueados), ¿en qué orden
exacto se procesan?

**Pregunta de reflexión**: ¿Es este dispatcher un sistema de prioridades
"real" (preemptive) o simplemente un ordenamiento al momento de
desbloquear? ¿Qué pasaría si llegan eventos CRITICAL mientras se está
procesando un evento LOW?
