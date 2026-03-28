# signal() vs sigaction() — Registrar Handlers de Señales

## Índice

1. [Qué son las señales](#1-qué-son-las-señales)
2. [signal(): la interfaz clásica](#2-signal-la-interfaz-clásica)
3. [Problemas de signal()](#3-problemas-de-signal)
4. [sigaction(): la interfaz moderna](#4-sigaction-la-interfaz-moderna)
5. [Campos de struct sigaction](#5-campos-de-struct-sigaction)
6. [sa_flags en detalle](#6-sa_flags-en-detalle)
7. [Disposición de señales: SIG_DFL, SIG_IGN, handler](#7-disposición-de-señales-sig_dfl-sig_ign-handler)
8. [Señales no capturables: SIGKILL y SIGSTOP](#8-señales-no-capturables-sigkill-y-sigstop)
9. [Herencia en fork y exec](#9-herencia-en-fork-y-exec)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué son las señales

Las señales son **notificaciones asíncronas** que el kernel envía a un
proceso para informarle de un evento. Son el mecanismo más antiguo de IPC
en Unix:

```
  ┌──────────────┐     señal     ┌──────────────┐
  │   Kernel     │──────────────▶│   Proceso     │
  │              │               │              │
  │ (o proceso)  │               │ 1. Interrumpe│
  │              │               │    ejecución │
  │              │               │ 2. Ejecuta   │
  │              │               │    handler   │
  │              │               │ 3. Reanuda   │
  └──────────────┘               └──────────────┘
```

### Orígenes de señales

| Origen           | Ejemplo                                  | Señal          |
|------------------|------------------------------------------|----------------|
| Terminal         | Ctrl+C                                   | `SIGINT`       |
| Terminal         | Ctrl+\                                   | `SIGQUIT`      |
| Terminal         | Ctrl+Z                                   | `SIGTSTP`      |
| Kernel           | Acceso a memoria inválido                | `SIGSEGV`      |
| Kernel           | División por cero                        | `SIGFPE`       |
| Kernel           | Pipe roto (lector cerró)                 | `SIGPIPE`      |
| Kernel           | Hijo terminó                             | `SIGCHLD`      |
| Kernel           | Límite de CPU excedido                   | `SIGXCPU`      |
| Proceso          | `kill(pid, sig)`                         | Cualquiera     |
| Proceso          | `raise(sig)` (a sí mismo)               | Cualquiera     |
| Shell            | `kill -TERM 1234`                        | `SIGTERM`      |
| Timer            | `alarm(seconds)`                         | `SIGALRM`      |

### Tres acciones posibles

Ante cada señal, un proceso puede:

1. **Acción por defecto** (SIG_DFL) — depende de la señal: terminar,
   ignorar, parar, o generar core dump.
2. **Ignorar** (SIG_IGN) — la señal se descarta silenciosamente.
3. **Capturar** — ejecutar un handler definido por el programador.

---

## 2. signal(): la interfaz clásica

```c
#include <signal.h>

typedef void (*sighandler_t)(int);

sighandler_t signal(int signum, sighandler_t handler);
```

- `signum`: número de señal (ej: `SIGINT`).
- `handler`: función a ejecutar, `SIG_DFL`, o `SIG_IGN`.
- Retorna el handler **anterior**, o `SIG_ERR` en error.

### Ejemplo básico

```c
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

void handle_sigint(int sig)
{
    (void)sig;
    const char msg[] = "\ncaught SIGINT, use SIGTERM to stop\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

int main(void)
{
    signal(SIGINT, handle_sigint);

    printf("PID %d running. Press Ctrl+C...\n", getpid());

    while (1)
        pause();  /* Sleep until a signal arrives */

    return 0;
}
```

```
  $ ./signal_demo
  PID 1234 running. Press Ctrl+C...
  ^C
  caught SIGINT, use SIGTERM to stop
  ^C
  caught SIGINT, use SIGTERM to stop
  (killed with: kill -TERM 1234)
```

### Ignorar una señal

```c
signal(SIGPIPE, SIG_IGN);  /* Don't die on broken pipe */
```

### Restaurar acción por defecto

```c
signal(SIGINT, SIG_DFL);  /* Ctrl+C kills again */
```

---

## 3. Problemas de signal()

`signal()` tiene problemas históricos que la hacen **no apta para código
serio**:

### Problema 1: Comportamiento no portable

El estándar C no especifica qué pasa cuando el handler se ejecuta.
Históricamente, hay dos comportamientos:

```
  System V (AT&T):              BSD:
  ┌─────────────────────┐      ┌─────────────────────┐
  │ Señal llega          │      │ Señal llega          │
  │ → handler ejecuta    │      │ → handler ejecuta    │
  │ → disposición se     │      │ → disposición se     │
  │   RESETEA a SIG_DFL  │      │   MANTIENE           │
  │                      │      │                      │
  │ Si la señal llega    │      │ Señal bloqueada      │
  │ otra vez durante el  │      │ durante el handler   │
  │ handler → SIG_DFL    │      │ (no reentrancia)     │
  │ (probablemente muere)│      │                      │
  └─────────────────────┘      └─────────────────────┘
```

En Linux, `signal()` usa semántica BSD (handler se mantiene). Pero en
otros Unix puede ser System V. **No se puede escribir código portable
con `signal()`**.

### Problema 2: Reset del handler (System V)

```c
/* In System V semantics: */
void handler(int sig)
{
    /* Handler is now reset to SIG_DFL! */
    /* If SIGINT arrives again RIGHT NOW, the process dies */
    signal(SIGINT, handler);  /* Race condition: reinstall */
    /* Between the reset and reinstall, we're vulnerable */
}
```

### Problema 3: No control sobre bloqueo de señales

Con `signal()` no se puede:
- Bloquear **otras** señales durante la ejecución del handler.
- Especificar flags como `SA_RESTART` o `SA_NOCLDSTOP`.
- Obtener información extra sobre la señal (siginfo_t).

### Problema 4: Interrupción de syscalls

Con `signal()`, las syscalls bloqueantes (`read`, `write`, `accept`, etc.)
se interrumpen cuando llega una señal. Algunas implementaciones las
reinician automáticamente, otras no. El comportamiento es impredecible:

```c
/* Did read fail because of an error, or because a signal arrived? */
ssize_t n = read(fd, buf, sizeof(buf));
if (n == -1) {
    if (errno == EINTR)
        /* Signal interrupted read — retry? */
    else
        /* Real error */
}
```

---

## 4. sigaction(): la interfaz moderna

```c
#include <signal.h>

int sigaction(int signum, const struct sigaction *act,
              struct sigaction *oldact);
```

- `signum`: señal a configurar.
- `act`: nueva configuración (NULL para solo leer la actual).
- `oldact`: almacena la configuración anterior (NULL si no interesa).
- Retorna 0 en éxito, -1 en error.

### Ejemplo equivalente al de signal()

```c
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

void handle_sigint(int sig)
{
    (void)sig;
    const char msg[] = "\ncaught SIGINT\n";
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
}

int main(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));  /* Or use = {0} */

    sa.sa_handler = handle_sigint;
    sigemptyset(&sa.sa_mask);    /* Don't block extra signals */
    sa.sa_flags = SA_RESTART;    /* Restart interrupted syscalls */

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    printf("PID %d running. Press Ctrl+C...\n", getpid());

    while (1)
        pause();

    return 0;
}
```

### Ventajas sobre signal()

| Aspecto                    | signal()           | sigaction()          |
|----------------------------|--------------------|----------------------|
| Handler se resetea         | Depende del SO     | Nunca (controlado)   |
| Bloqueo durante handler   | No controlable     | sa_mask lo define    |
| Reinicio de syscalls       | Impredecible       | SA_RESTART explícito |
| Información extra (siginfo)| No                 | SA_SIGINFO           |
| Portabilidad               | Problemática       | POSIX garantizado    |
| Lectura del handler actual | Cambia disposición | oldact sin modificar |

---

## 5. Campos de struct sigaction

```c
struct sigaction {
    /* Handler: elegir UNO de estos dos */
    void     (*sa_handler)(int);
    void     (*sa_siginfo)(int, siginfo_t *, void *);

    sigset_t   sa_mask;     /* Signals to block during handler */
    int        sa_flags;    /* Behavior flags */
};
```

### sa_handler

La función que se ejecuta cuando llega la señal. Recibe el número de señal:

```c
void my_handler(int sig)
{
    /* sig == signal number */
}

sa.sa_handler = my_handler;
sa.sa_handler = SIG_DFL;    /* Default action */
sa.sa_handler = SIG_IGN;    /* Ignore signal */
```

### sa_sigaction (con SA_SIGINFO)

Handler extendido que recibe información detallada:

```c
void my_handler(int sig, siginfo_t *info, void *ucontext)
{
    /* sig: signal number */
    /* info: detailed information */
    /* ucontext: CPU context (rarely used) */

    printf("signal %d from PID %d, UID %d\n",
           sig, info->si_pid, info->si_uid);
}

sa.sa_sigaction = my_handler;
sa.sa_flags = SA_SIGINFO;  /* Must set this flag */
```

Campos útiles de `siginfo_t`:

| Campo         | Contenido                              |
|---------------|----------------------------------------|
| `si_pid`      | PID del proceso que envió la señal     |
| `si_uid`      | UID real del remitente                 |
| `si_status`   | Exit status (para SIGCHLD)             |
| `si_code`     | Cómo se generó (SI_USER, SI_KERNEL...) |
| `si_addr`     | Dirección que causó la falta (SIGSEGV) |
| `si_value`    | Valor adjunto (señales real-time)      |

### sa_mask

Conjunto de señales **adicionales** que se bloquean durante la ejecución
del handler. La propia señal que disparó el handler **siempre se bloquea**
automáticamente (a menos que se use `SA_NODEFER`):

```c
struct sigaction sa;
sa.sa_handler = my_handler;

sigemptyset(&sa.sa_mask);
sigaddset(&sa.sa_mask, SIGTERM);  /* Also block SIGTERM during handler */
sigaddset(&sa.sa_mask, SIGQUIT);  /* Also block SIGQUIT during handler */

/* During handler execution:
   - SIGINT blocked (automatically, because it's the handled signal)
   - SIGTERM blocked (because we added it to sa_mask)
   - SIGQUIT blocked (because we added it to sa_mask)
   - All others can still arrive */
```

```
  Sin sa_mask:                   Con sa_mask = {SIGTERM, SIGQUIT}:
  ┌──────────────────┐          ┌──────────────────┐
  │ handler(SIGINT)  │          │ handler(SIGINT)  │
  │                  │          │                  │
  │ SIGINT: blocked  │          │ SIGINT: blocked  │
  │ SIGTERM: can hit │          │ SIGTERM: blocked │
  │ SIGQUIT: can hit │          │ SIGQUIT: blocked │
  │ SIGUSR1: can hit │          │ SIGUSR1: can hit │
  └──────────────────┘          └──────────────────┘
```

---

## 6. sa_flags en detalle

| Flag            | Efecto                                              |
|-----------------|-----------------------------------------------------|
| `SA_RESTART`    | Reiniciar syscalls interrumpidas automáticamente     |
| `SA_SIGINFO`    | Usar `sa_sigaction` en lugar de `sa_handler`         |
| `SA_NOCLDSTOP`  | No enviar SIGCHLD cuando un hijo se **detiene**      |
| `SA_NOCLDWAIT`  | No crear zombies al morir los hijos                  |
| `SA_NODEFER`    | No bloquear la señal automáticamente durante handler |
| `SA_RESETHAND`  | Resetear a SIG_DFL después de ejecutar (System V)    |
| `SA_ONSTACK`    | Ejecutar handler en stack alternativo (sigaltstack)  |

### SA_RESTART: reiniciar syscalls

Sin `SA_RESTART`, cuando una señal interrumpe una syscall bloqueante,
esta retorna `-1` con `errno = EINTR`:

```c
/* Without SA_RESTART: */
sa.sa_flags = 0;
sigaction(SIGCHLD, &sa, NULL);

/* read() is blocking, SIGCHLD arrives */
n = read(fd, buf, sizeof(buf));
/* n == -1, errno == EINTR → must manually retry */

/* With SA_RESTART: */
sa.sa_flags = SA_RESTART;
sigaction(SIGCHLD, &sa, NULL);

/* read() is blocking, SIGCHLD arrives */
n = read(fd, buf, sizeof(buf));
/* read is automatically restarted — no EINTR visible */
```

Syscalls que `SA_RESTART` **puede** reiniciar:
- `read`, `write`, `recv`, `send`
- `wait`, `waitpid`
- `accept`, `connect`
- `flock`, `fcntl(F_SETLKW)`

Syscalls que `SA_RESTART` **no** reinicia:
- `poll`, `select`, `epoll_wait`
- `sleep`, `nanosleep`, `usleep`
- `sem_wait`, `msgrcv`, `msgsnd`

Para estas últimas, siempre hay que manejar `EINTR` manualmente.

### SA_NOCLDSTOP y SA_NOCLDWAIT

```c
/* Only care about child death, not stop/continue */
sa.sa_handler = sigchld_handler;
sa.sa_flags = SA_RESTART | SA_NOCLDSTOP;

/* Auto-reap children (no zombies, no handler needed) */
sa.sa_handler = SIG_IGN;
sa.sa_flags = SA_NOCLDWAIT;
sigaction(SIGCHLD, &sa, NULL);
```

### SA_NODEFER: permitir reentrancia

```c
/* Dangerous: handler can be interrupted by the same signal */
sa.sa_flags = SA_NODEFER;
/* Use only when you're certain the handler is reentrant */
```

---

## 7. Disposición de señales: SIG_DFL, SIG_IGN, handler

### Acciones por defecto (SIG_DFL)

| Acción    | Señales                                          |
|-----------|--------------------------------------------------|
| Terminate | SIGTERM, SIGINT, SIGPIPE, SIGALRM, SIGUSR1/2... |
| Core dump | SIGSEGV, SIGABRT, SIGFPE, SIGBUS, SIGQUIT...    |
| Stop      | SIGSTOP, SIGTSTP, SIGTTIN, SIGTTOU               |
| Continue  | SIGCONT                                           |
| Ignore    | SIGCHLD, SIGURG, SIGWINCH                         |

### Consultar la disposición actual

```c
struct sigaction old;

/* Read without modifying */
sigaction(SIGINT, NULL, &old);

if (old.sa_handler == SIG_DFL)
    printf("SIGINT: default action\n");
else if (old.sa_handler == SIG_IGN)
    printf("SIGINT: ignored\n");
else
    printf("SIGINT: custom handler at %p\n", (void *)old.sa_handler);
```

### Guardar y restaurar

```c
struct sigaction old_sigint, old_sigterm;

/* Save current, install new */
struct sigaction sa = { .sa_handler = my_handler, .sa_flags = SA_RESTART };
sigemptyset(&sa.sa_mask);

sigaction(SIGINT, &sa, &old_sigint);
sigaction(SIGTERM, &sa, &old_sigterm);

/* ... do work ... */

/* Restore original handlers */
sigaction(SIGINT, &old_sigint, NULL);
sigaction(SIGTERM, &old_sigterm, NULL);
```

---

## 8. Señales no capturables: SIGKILL y SIGSTOP

Dos señales **nunca** pueden ser capturadas, ignoradas ni bloqueadas:

| Señal      | Número | Acción                    | Por qué no capturables         |
|------------|--------|---------------------------|--------------------------------|
| `SIGKILL`  | 9      | Terminar inmediatamente   | Garantía de poder matar cualquier proceso |
| `SIGSTOP`  | 19     | Detener el proceso        | Garantía de poder parar cualquier proceso |

```c
/* These always fail silently or with error: */
signal(SIGKILL, my_handler);    /* No effect */
signal(SIGSTOP, SIG_IGN);      /* No effect */

struct sigaction sa = { .sa_handler = my_handler };
sigemptyset(&sa.sa_mask);
sigaction(SIGKILL, &sa, NULL);  /* Returns -1, errno = EINVAL */
```

### SIGTERM vs SIGKILL

```
  Buenas prácticas para matar un proceso:

  1. kill -TERM pid       Pedir que termine (handler puede cleanup)
         │
         ├─ proceso termina ──▶ OK
         │
         ├─ 5 segundos...
         │
  2. kill -KILL pid       Forzar (kernel mata, sin cleanup)
         │
         └─ proceso muere ──▶ Siempre funciona
```

Por eso los servidores implementan handlers de `SIGTERM` para hacer
shutdown limpio (cerrar conexiones, guardar estado, flush buffers), pero
nunca de `SIGKILL` (es imposible).

---

## 9. Herencia en fork y exec

### fork: hereda disposiciones

El hijo hereda **todas** las disposiciones de señales del padre:

```c
signal(SIGINT, SIG_IGN);
signal(SIGTERM, my_handler);

pid_t pid = fork();
if (pid == 0) {
    /* Child inherits:
       SIGINT  → SIG_IGN     (still ignored)
       SIGTERM → my_handler  (same handler function)
       All others → unchanged */
}
```

### exec: resetea handlers, mantiene SIG_IGN

`exec` tiene reglas específicas:

```
  Antes de exec          Después de exec
  ──────────────         ───────────────
  SIG_DFL          →     SIG_DFL         (sin cambio)
  SIG_IGN          →     SIG_IGN         (se mantiene)
  my_handler       →     SIG_DFL         (reset: el código del handler
                                           ya no existe en memoria)
```

```c
signal(SIGINT, SIG_IGN);      /* Will persist through exec */
signal(SIGTERM, my_handler);  /* Will be reset to SIG_DFL by exec */

if (fork() == 0) {
    execvp("my_program", argv);
    /* In my_program:
       SIGINT  → SIG_IGN  (inherited! program can't be Ctrl+C'd)
       SIGTERM → SIG_DFL  (handler was reset, default = terminate) */
}
```

### Implicación: limpiar señales antes de exec

Si el padre ignora señales, el hijo exec'd las hereda ignoradas. Esto
puede ser problemático:

```c
if (pid == 0) {
    /* Reset signals that parent might have set to SIG_IGN */
    struct sigaction sa = { .sa_handler = SIG_DFL };
    sigemptyset(&sa.sa_mask);

    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGPIPE, &sa, NULL);
    sigaction(SIGCHLD, &sa, NULL);

    /* Also unblock any blocked signals */
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);

    execvp(cmd, argv);
    _exit(127);
}
```

### La máscara de señales también se hereda

La máscara de señales bloqueadas (configurada con `sigprocmask`) se
hereda tanto en `fork` como en `exec`:

```c
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGCHLD);
sigprocmask(SIG_BLOCK, &mask, NULL);

if (fork() == 0) {
    /* Child has SIGCHLD blocked — might cause problems */
    execvp("child_program", argv);
    /* child_program starts with SIGCHLD blocked!
       If it relies on SIGCHLD, it will malfunction */
}
```

---

## 10. Errores comunes

### Error 1: Usar signal() en código de producción

```c
/* MAL: comportamiento no portable */
signal(SIGTERM, cleanup_handler);
/* On System V: handler resets after first signal
   On BSD/Linux: handler persists
   Result: code works on Linux, breaks on Solaris */

/* BIEN: siempre usar sigaction */
struct sigaction sa = {
    .sa_handler = cleanup_handler,
    .sa_flags   = SA_RESTART,
};
sigemptyset(&sa.sa_mask);
sigaction(SIGTERM, &sa, NULL);
```

`signal()` es aceptable en dos contextos: programas triviales/ejemplos
y para establecer `SIG_IGN` o `SIG_DFL` (donde no hay ambigüedad).

### Error 2: No inicializar struct sigaction

```c
/* MAL: campos basura en la estructura */
struct sigaction sa;
sa.sa_handler = my_handler;
sigaction(SIGTERM, &sa, NULL);
/* sa_mask, sa_flags have garbage values → undefined behavior */

/* BIEN: inicializar todo */
struct sigaction sa;
memset(&sa, 0, sizeof(sa));
sa.sa_handler = my_handler;
sigemptyset(&sa.sa_mask);
sa.sa_flags = SA_RESTART;
sigaction(SIGTERM, &sa, NULL);

/* O con designated initializer (C99+): */
struct sigaction sa = {
    .sa_handler = my_handler,
    .sa_flags   = SA_RESTART,
};
sigemptyset(&sa.sa_mask);
sigaction(SIGTERM, &sa, NULL);
```

### Error 3: Olvidar SA_RESTART y no manejar EINTR

```c
/* MAL: señal interrumpe read, se pierde el dato */
sigaction(SIGCHLD, &sa, NULL);  /* sa_flags = 0, no SA_RESTART */

ssize_t n = read(fd, buf, sizeof(buf));
if (n == -1) {
    perror("read");  /* "Interrupted system call" — not a real error! */
    exit(1);
}

/* BIEN opción A: usar SA_RESTART */
sa.sa_flags = SA_RESTART;

/* BIEN opción B: retry loop */
ssize_t n;
do {
    n = read(fd, buf, sizeof(buf));
} while (n == -1 && errno == EINTR);
```

### Error 4: Llamar funciones no seguras en el handler

```c
/* MAL: printf no es async-signal-safe */
void handler(int sig)
{
    printf("caught signal %d\n", sig);  /* Can deadlock or corrupt */
    malloc(100);                         /* Not safe in handler */
    exit(1);                             /* Calls atexit handlers — unsafe */
}

/* BIEN: solo funciones async-signal-safe */
void handler(int sig)
{
    /* write() IS async-signal-safe */
    const char msg[] = "caught signal\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    _exit(1);  /* _exit is safe (no atexit, no stdio flush) */
}
```

Este tema se profundiza en T03 (Signal handlers seguros).

### Error 5: No limpiar señales heredadas antes de exec

```c
/* MAL: el padre ignora SIGPIPE, el hijo exec'd lo hereda */
signal(SIGPIPE, SIG_IGN);

if (fork() == 0) {
    /* Child program inherits SIGPIPE=SIG_IGN
       If it's a network program, it won't detect broken connections
       via SIGPIPE — it gets EPIPE on write instead, which it
       might not check */
    execvp("server", argv);
    _exit(127);
}

/* BIEN: restaurar antes de exec */
if (fork() == 0) {
    signal(SIGPIPE, SIG_DFL);  /* Reset for child */
    execvp("server", argv);
    _exit(127);
}
```

---

## 11. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │              signal() vs sigaction()                          │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  signal() — NO USAR en código serio                          │
  │    signal(SIGTERM, handler)    instalar handler              │
  │    signal(SIGPIPE, SIG_IGN)   ignorar                       │
  │    signal(SIGINT, SIG_DFL)    restaurar default              │
  │    Problemas: reset en SysV, no SA_RESTART, no sa_mask       │
  │                                                              │
  │  sigaction() — SIEMPRE preferir                              │
  │    struct sigaction sa = {                                    │
  │        .sa_handler = handler,     /* o sa_sigaction + flag */ │
  │        .sa_flags   = SA_RESTART,  /* reiniciar syscalls */   │
  │    };                                                        │
  │    sigemptyset(&sa.sa_mask);      /* signals extra blocked */│
  │    sigaction(SIGTERM, &sa, &old); /* install, save old */    │
  │                                                              │
  │  sa_flags:                                                   │
  │    SA_RESTART     reiniciar syscalls interrumpidas            │
  │    SA_SIGINFO     usar sa_sigaction (siginfo_t *info)        │
  │    SA_NOCLDSTOP   SIGCHLD solo en muerte, no en stop         │
  │    SA_NOCLDWAIT   auto-reap hijos (no zombies)              │
  │    SA_NODEFER     no bloquear la señal durante handler       │
  │    SA_RESETHAND   reset a SIG_DFL tras ejecutar (SysV)      │
  │    SA_ONSTACK     handler en stack alternativo               │
  │                                                              │
  │  No capturables: SIGKILL (9), SIGSTOP (19)                  │
  │                                                              │
  │  Herencia:                                                   │
  │    fork → hereda disposiciones + máscara                     │
  │    exec → SIG_IGN se mantiene, handlers reset a SIG_DFL     │
  │           máscara de bloqueo se mantiene                     │
  │           → limpiar antes de exec                            │
  │                                                              │
  │  Disposición:                                                │
  │    SIG_DFL   acción por defecto (term/core/stop/ignore)      │
  │    SIG_IGN   ignorar la señal                                │
  │    handler   función custom                                  │
  │                                                              │
  │  Leer sin modificar:                                         │
  │    sigaction(sig, NULL, &old)  → old tiene la config actual  │
  └──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Wrapper portable de signal

Implementa una función `Signal` (con S mayúscula, convención de Stevens)
que use `sigaction` internamente pero tenga la misma interfaz simple que
`signal`:

```c
typedef void (*sighandler_t)(int);

sighandler_t Signal(int signum, sighandler_t handler);
```

Requisitos:
1. Usar `sigaction` internamente.
2. Establecer `SA_RESTART` siempre (excepto para `SIGALRM`, que
   típicamente se quiere que interrumpa syscalls).
3. No resetear el handler tras ejecución (semántica BSD).
4. Retornar el handler anterior, o `SIG_ERR` en error.
5. Si `handler` es `SIG_IGN` o `SIG_DFL`, simplemente pasarlo.

Probar con:
```c
sighandler_t old = Signal(SIGINT, my_handler);
printf("old handler was: %s\n",
       old == SIG_DFL ? "default" : old == SIG_IGN ? "ignore" : "custom");
Signal(SIGINT, old);  /* Restore */
```

**Predicción antes de codificar**: ¿Por qué SIGALRM es el único caso
donde no queremos SA_RESTART? ¿Qué patrón usa alarm() que depende de
interrumpir syscalls?

**Pregunta de reflexión**: Muchos libros de programación Unix (Stevens,
Kerrisk) incluyen esta función `Signal`. ¿Por qué crees que POSIX no
simplemente arregló el comportamiento de `signal()` en vez de crear
`sigaction()`?

---

### Ejercicio 2: Inspector de disposiciones

Escribe un programa que muestre la disposición actual de todas las señales
del proceso (1 a 31 estándar):

```
  $ ./sig_inspector
  Signal  Name       Disposition
  ─────────────────────────────────
   1      SIGHUP     default (terminate)
   2      SIGINT     default (terminate)
   3      SIGQUIT    default (core dump)
   ...
   9      SIGKILL    [not changeable]
  13      SIGPIPE    ignored
  14      SIGALRM    handler at 0x401234
  ...
  19      SIGSTOP    [not changeable]
  ...
```

Requisitos:
1. Usar `sigaction(sig, NULL, &old)` para leer sin modificar.
2. Manejar SIGKILL/SIGSTOP (sigaction devuelve EINVAL).
3. Mostrar la acción por defecto correcta para cada señal (terminate,
   core dump, stop, ignore).
4. Aceptar un argumento PID opcional: si se da, leer `/proc/PID/status`
   y parsear `SigIgn`, `SigCgt`, `SigBlk` (bitmasks en hex) para
   mostrar la disposición de ese proceso.

**Predicción antes de codificar**: Si un proceso ignora SIGPIPE y
SIGCHLD, ¿qué valor tendrá el campo `SigIgn` en `/proc/PID/status`?
(Pista: SIGPIPE=13, SIGCHLD=17, y son bitmasks).

**Pregunta de reflexión**: ¿Por qué `sigaction` con `act=NULL` es seguro
para inspeccionar, pero `signal(sig, handler)` no se puede usar para
leer sin modificar la disposición (a menos que la reinstales inmediatamente)?

---

### Ejercicio 3: Graceful shutdown framework

Implementa un framework de shutdown limpio que un servidor pueda usar:

```c
typedef void (*shutdown_fn)(void *userdata);

typedef struct {
    shutdown_fn  fn;
    void        *userdata;
    const char  *name;       /* For logging */
} shutdown_hook_t;

int  shutdown_init(void);
int  shutdown_register(shutdown_fn fn, void *userdata, const char *name);
int  shutdown_requested(void);  /* Check if SIGTERM/SIGINT received */
void shutdown_execute(void);    /* Run all hooks in reverse order */
```

Comportamiento:
1. `shutdown_init` instala handlers para SIGTERM y SIGINT con `sigaction`.
2. El handler solo pone un flag (`volatile sig_atomic_t`).
3. `shutdown_requested` retorna 1 si se recibió la señal.
4. `shutdown_execute` ejecuta todos los hooks registrados en orden
   inverso (LIFO, como `atexit`).
5. Si se recibe una segunda señal durante `shutdown_execute`, imprimir
   "forced shutdown" y llamar `_exit(1)`.

Probar con:
```c
shutdown_init();
shutdown_register(close_database, db, "database");
shutdown_register(flush_logs, logger, "logger");
shutdown_register(close_socket, sock, "socket");

while (!shutdown_requested()) {
    handle_request();
}

printf("shutting down...\n");
shutdown_execute();
/* socket closed, then logs flushed, then database closed */
```

**Predicción antes de codificar**: ¿Por qué el orden de ejecución es
inverso al de registro? ¿Qué dependencia implícita asume este diseño?

**Pregunta de reflexión**: ¿Por qué el handler solo pone un flag en vez
de ejecutar el shutdown directamente? ¿Qué problemas habría si llamaras
`close_database()` dentro del signal handler?
