# Máscara de Señales — sigprocmask, sigpending y Secciones Críticas

## Índice

1. [Qué es la máscara de señales](#1-qué-es-la-máscara-de-señales)
2. [sigset_t: el tipo conjunto de señales](#2-sigset_t-el-tipo-conjunto-de-señales)
3. [sigprocmask: bloquear y desbloquear](#3-sigprocmask-bloquear-y-desbloquear)
4. [Señales pendientes: sigpending](#4-señales-pendientes-sigpending)
5. [Secciones críticas](#5-secciones-críticas)
6. [sigsuspend: espera atómica](#6-sigsuspend-espera-atómica)
7. [Máscara durante signal handlers](#7-máscara-durante-signal-handlers)
8. [Herencia en fork y exec](#8-herencia-en-fork-y-exec)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Qué es la máscara de señales

Cada proceso (cada thread, en realidad) tiene una **máscara de señales
bloqueadas** — un conjunto de señales que el kernel **retiene** en vez
de entregar. La señal no se pierde: queda **pendiente** hasta que se
desbloquea.

```
  Señal llega al proceso
       │
       ▼
  ┌──────────────────┐
  │ ¿Está bloqueada? │
  │                  │
  │  No ──▶ entregar │──▶ handler o acción default
  │                  │
  │  Sí ──▶ pendiente│──▶ queda en cola hasta desbloqueo
  │         (no se   │
  │          pierde)  │
  └──────────────────┘
```

### Bloquear ≠ ignorar

| Concepto   | Qué pasa con la señal                         |
|------------|-----------------------------------------------|
| **Ignorar** (SIG_IGN) | Se descarta permanentemente         |
| **Bloquear** (máscara) | Se retiene; se entrega al desbloquear |

Una señal ignorada desaparece para siempre. Una señal bloqueada espera.

### Las señales estándar no se encolan

Si la misma señal llega **múltiples veces** mientras está bloqueada,
solo se retiene **una** instancia. Las señales estándar (1-31) no se
encolan:

```
  SIGUSR1 bloqueada:

  llega SIGUSR1 #1 → pendiente (1 instancia)
  llega SIGUSR1 #2 → ya pendiente, se descarta
  llega SIGUSR1 #3 → ya pendiente, se descarta

  desbloquear SIGUSR1 → se entrega UNA vez
```

> Las señales de tiempo real (SIGRTMIN-SIGRTMAX) sí se encolan. Se
> cubren en S02.

---

## 2. sigset_t: el tipo conjunto de señales

`sigset_t` es un tipo opaco que representa un conjunto de señales. Se
manipula exclusivamente con funciones dedicadas:

```c
#include <signal.h>

int sigemptyset(sigset_t *set);           /* {} — empty set */
int sigfillset(sigset_t *set);            /* {all signals} */
int sigaddset(sigset_t *set, int signum); /* Add one signal */
int sigdelset(sigset_t *set, int signum); /* Remove one signal */
int sigismember(const sigset_t *set, int signum); /* Test membership */
```

Todas retornan 0 en éxito, -1 en error (excepto `sigismember` que
retorna 0 o 1).

### Uso básico

```c
sigset_t mask;

/* Start with empty set, add specific signals */
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigaddset(&mask, SIGTERM);
/* mask = {SIGINT, SIGTERM} */

/* Start with all signals, remove some */
sigfillset(&mask);
sigdelset(&mask, SIGKILL);  /* Can't block SIGKILL anyway */
sigdelset(&mask, SIGSTOP);  /* Can't block SIGSTOP anyway */
/* mask = {all except SIGKILL, SIGSTOP} */

/* Test membership */
if (sigismember(&mask, SIGINT))
    printf("SIGINT is in the set\n");
```

### Siempre inicializar

```c
/* MAL: sigset_t sin inicializar tiene bits basura */
sigset_t mask;
sigaddset(&mask, SIGINT);  /* Undefined: other bits are random */

/* BIEN: inicializar primero */
sigset_t mask;
sigemptyset(&mask);        /* Clear all bits */
sigaddset(&mask, SIGINT);  /* Now only SIGINT is set */
```

---

## 3. sigprocmask: bloquear y desbloquear

```c
#include <signal.h>

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
```

- `how`: operación a realizar.
- `set`: conjunto de señales (NULL para solo leer la máscara actual).
- `oldset`: almacena la máscara anterior (NULL si no interesa).

### Valores de how

| how             | Efecto                                            |
|-----------------|---------------------------------------------------|
| `SIG_BLOCK`     | Añadir `set` a la máscara (bloquear más señales)  |
| `SIG_UNBLOCK`   | Quitar `set` de la máscara (desbloquear señales)  |
| `SIG_SETMASK`   | Reemplazar la máscara completamente por `set`     |

```
  Máscara actual: {SIGINT}

  SIG_BLOCK   + {SIGTERM}  → {SIGINT, SIGTERM}    (unión)
  SIG_UNBLOCK - {SIGINT}   → {}                    (resta)
  SIG_SETMASK = {SIGUSR1}  → {SIGUSR1}             (reemplazo)
```

### Bloquear señales

```c
sigset_t mask, oldmask;

sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigaddset(&mask, SIGTERM);

/* Block SIGINT and SIGTERM, save previous mask */
sigprocmask(SIG_BLOCK, &mask, &oldmask);

/* ... critical section: SIGINT and SIGTERM won't be delivered ... */

/* Restore previous mask (unblocks what was blocked) */
sigprocmask(SIG_SETMASK, &oldmask, NULL);
/* Any pending SIGINT/SIGTERM is now delivered */
```

### Leer la máscara actual

```c
sigset_t current;
sigprocmask(SIG_BLOCK, NULL, &current);
/* current now contains the current mask, nothing changed */

if (sigismember(&current, SIGINT))
    printf("SIGINT is currently blocked\n");
```

### Desbloquear una señal específica

```c
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);

sigprocmask(SIG_UNBLOCK, &mask, NULL);
/* SIGINT is now unblocked — if pending, delivered NOW */
```

---

## 4. Señales pendientes: sigpending

```c
#include <signal.h>

int sigpending(sigset_t *set);
```

Retorna el conjunto de señales que están **bloqueadas y pendientes**
(han llegado pero no se han entregado):

```c
#include <signal.h>
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    sigset_t mask, pending;

    /* Block SIGUSR1 */
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    /* Send SIGUSR1 to ourselves — it becomes pending */
    kill(getpid(), SIGUSR1);

    /* Check what's pending */
    sigpending(&pending);

    if (sigismember(&pending, SIGUSR1))
        printf("SIGUSR1 is pending\n");

    /* Unblock → signal delivered → default action = terminate */
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
    /* We never reach here (SIGUSR1 default = terminate) */

    return 0;
}
```

### Uso práctico: verificar si hay señales esperando

```c
int has_pending_signal(int signum)
{
    sigset_t pending;
    sigpending(&pending);
    return sigismember(&pending, signum);
}

/* In a long computation: */
for (int i = 0; i < N; i++) {
    do_work(i);

    if (has_pending_signal(SIGINT)) {
        printf("SIGINT pending, will handle after unblock\n");
        break;
    }
}
```

---

## 5. Secciones críticas

El uso principal de `sigprocmask` es proteger **secciones críticas**:
bloques de código que no deben ser interrumpidos por señales.

### Problema: estructura compartida con handler

```c
/* Global structure shared between main and handler */
struct {
    int count;
    char data[256];
    int valid;
} shared_state;

void handler(int sig)
{
    (void)sig;
    if (shared_state.valid)
        process(shared_state.data, shared_state.count);
}

void update_state(const char *new_data, int new_count)
{
    shared_state.valid = 0;       /* Mark invalid */
    /* ← SIGNAL HERE: handler sees valid=0, OK */
    shared_state.count = new_count;
    memcpy(shared_state.data, new_data, new_count);
    shared_state.valid = 1;       /* Mark valid */
    /* ← SIGNAL HERE: handler sees consistent state, OK */
}
```

Pero ¿qué pasa si la señal llega entre la escritura de `count` y
`data`? Con el flag `valid` se mitiga, pero no siempre se puede
diseñar así. La solución general es bloquear:

### Solución: bloquear durante la sección crítica

```c
void update_state(const char *new_data, int new_count)
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);  /* Block the signal that uses shared_state */
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    /* ── Critical section: SIGUSR1 cannot interrupt ── */
    shared_state.count = new_count;
    memcpy(shared_state.data, new_data, new_count);
    shared_state.valid = 1;
    /* ── End critical section ── */

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    /* If SIGUSR1 arrived during the critical section,
       it's delivered NOW with consistent state */
}
```

### Patrón: proteger fork+exec

```c
/*
 * Block SIGCHLD during fork to prevent race:
 * child might die before parent saves the PID
 */
sigset_t mask, oldmask;
sigemptyset(&mask);
sigaddset(&mask, SIGCHLD);
sigprocmask(SIG_BLOCK, &mask, &oldmask);

pid_t pid = fork();
if (pid == 0) {
    /* Child: restore mask before exec */
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    execvp(argv[0], argv);
    _exit(127);
}

/* Parent: safely save pid (SIGCHLD can't arrive yet) */
children[nchildren++] = pid;

/* Now it's safe to unblock SIGCHLD */
sigprocmask(SIG_SETMASK, &oldmask, NULL);
```

Sin bloqueo, el hijo podría morir y SIGCHLD llegar **antes** de que el
padre guarde el PID. El handler de SIGCHLD intentaría buscar un PID que
aún no está en el array.

---

## 6. sigsuspend: espera atómica

### El problema de la race condition

```c
/* MAL: race condition entre unblock y pause */
sigprocmask(SIG_UNBLOCK, &mask, NULL);
/* ← SIGNAL ARRIVES HERE: handler runs, sets flag */
pause();
/* pause() blocks forever — the signal already came and went! */
```

La señal puede llegar entre `sigprocmask` y `pause`. El handler pone
el flag, pero `pause` no ha empezado todavía. Cuando `pause` empieza,
la señal ya pasó y no hay más señales — bloqueo infinito.

### sigsuspend: desbloquear + esperar atómicamente

```c
#include <signal.h>

int sigsuspend(const sigset_t *mask);
```

`sigsuspend` hace **atómicamente**:
1. Reemplaza la máscara actual por `mask`.
2. Suspende el proceso hasta que llegue una señal **no bloqueada por
   mask** cuyo handler ejecute.
3. Restaura la máscara original al retornar.

Siempre retorna -1 con `errno = EINTR`.

```c
volatile sig_atomic_t got_signal = 0;

void handler(int sig)
{
    (void)sig;
    got_signal = 1;
}

int main(void)
{
    struct sigaction sa = { .sa_handler = handler };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    /* Block SIGUSR1 */
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGUSR1);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    /* Do work with SIGUSR1 blocked */
    while (!got_signal) {
        do_some_work();

        /* Atomically: unblock SIGUSR1 + sleep until signal */
        sigsuspend(&oldmask);
        /* oldmask = mask before we blocked SIGUSR1
           → SIGUSR1 is unblocked during the suspend
           → if it arrives, handler runs
           → sigsuspend returns
           → mask is restored (SIGUSR1 blocked again) */
    }

    printf("signal received, exiting\n");

    /* Restore original mask */
    sigprocmask(SIG_SETMASK, &oldmask, NULL);
    return 0;
}
```

### Diagrama de sigsuspend

```
  sigprocmask:  mask = {SIGUSR1}          (SIGUSR1 bloqueada)
                    │
              ┌─────┴──────┐
              │ sigsuspend  │
              │ (&oldmask)  │
              │             │
              │ 1. mask =   │ ← atómico: SIGUSR1 desbloqueada
              │    oldmask  │
              │ 2. sleep    │
              │    ...      │ ← esperando
              │    SIGUSR1! │ ← señal llega
              │ 3. handler  │ ← handler ejecuta
              │ 4. mask =   │ ← restaura: SIGUSR1 bloqueada de nuevo
              │   {SIGUSR1} │
              │ 5. return   │
              └─────┬──────┘
                    │
              got_signal == 1
```

### Comparación

```c
/* INCORRECTO: race condition */
sigprocmask(SIG_UNBLOCK, &mask, NULL);
pause();  /* Signal might arrive between these two lines */

/* CORRECTO: atómico */
sigsuspend(&oldmask);  /* Unblock + wait in one atomic step */
```

---

## 7. Máscara durante signal handlers

Cuando un handler se ejecuta, la señal que lo activó se **bloquea
automáticamente** (a menos que se use `SA_NODEFER`). Además, las
señales en `sa_mask` también se bloquean:

```c
struct sigaction sa;
sa.sa_handler = my_handler;
sigemptyset(&sa.sa_mask);
sigaddset(&sa.sa_mask, SIGTERM);  /* Also block SIGTERM */
sa.sa_flags = SA_RESTART;

sigaction(SIGINT, &sa, NULL);

/*
 * When SIGINT arrives and my_handler runs:
 * - SIGINT is blocked (automatic, same signal)
 * - SIGTERM is blocked (because of sa_mask)
 * - All others can still arrive
 *
 * When handler returns:
 * - Both are automatically unblocked (mask restored)
 */
```

### Bloquear todas las señales durante el handler

```c
struct sigaction sa;
sa.sa_handler = critical_handler;
sigfillset(&sa.sa_mask);  /* Block ALL signals during handler */
sa.sa_flags = SA_RESTART;

sigaction(SIGTERM, &sa, NULL);

/* critical_handler will never be interrupted by any signal */
```

### SA_NODEFER: permitir la misma señal

```c
sa.sa_flags = SA_NODEFER;
/* The handled signal is NOT blocked during handler.
   Handler can be interrupted by the SAME signal.
   Use only if handler is fully reentrant. */
```

---

## 8. Herencia en fork y exec

### fork: hijo hereda la máscara

```c
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigprocmask(SIG_BLOCK, &mask, NULL);

pid_t pid = fork();
if (pid == 0) {
    /* Child: SIGINT is blocked (inherited) */
    /* Must unblock if child needs SIGINT */
}
```

### exec: máscara se preserva

La máscara de señales bloqueadas **sobrevive a exec**. Esto es un
problema frecuente:

```c
/* Parent blocks SIGPIPE */
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGPIPE);
sigprocmask(SIG_BLOCK, &mask, NULL);

if (fork() == 0) {
    /* Child inherits SIGPIPE blocked */
    execvp("network_server", argv);
    /* network_server starts with SIGPIPE blocked!
       It may malfunction because it expects SIGPIPE
       to terminate broken connections */
}
```

### Limpiar antes de exec

```c
if (fork() == 0) {
    /* Reset signal mask to empty (unblock all) */
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);

    /* Also reset dispositions that were set to SIG_IGN */
    signal(SIGPIPE, SIG_DFL);
    signal(SIGCHLD, SIG_DFL);

    execvp(argv[0], argv);
    _exit(127);
}
```

### posix_spawnattr: limpiar máscara en spawn

```c
#include <spawn.h>

posix_spawnattr_t attr;
posix_spawnattr_init(&attr);

/* Set flags to control signal behavior */
posix_spawnattr_setflags(&attr,
    POSIX_SPAWN_SETSIGMASK | POSIX_SPAWN_SETSIGDEF);

/* Clear the signal mask for the spawned process */
sigset_t empty;
sigemptyset(&empty);
posix_spawnattr_setsigmask(&attr, &empty);

/* Reset these signals to default */
sigset_t defaults;
sigfillset(&defaults);
posix_spawnattr_setsigdefault(&attr, &defaults);

posix_spawnp(&pid, "cmd", NULL, &attr, argv, environ);
posix_spawnattr_destroy(&attr);
```

---

## 9. Errores comunes

### Error 1: No inicializar sigset_t

```c
/* MAL: bits basura → señales aleatorias bloqueadas */
sigset_t mask;
sigaddset(&mask, SIGINT);
sigprocmask(SIG_SETMASK, &mask, NULL);
/* Unknown signals may be blocked because mask wasn't cleared! */

/* BIEN: siempre inicializar */
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigprocmask(SIG_SETMASK, &mask, NULL);
```

### Error 2: Usar SIG_SETMASK cuando se quería SIG_BLOCK

```c
/* MAL: reemplaza toda la máscara, desbloqueando señales que
   otro código había bloqueado */
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigprocmask(SIG_SETMASK, &mask, NULL);
/* Now ONLY SIGINT is blocked — anything else is unblocked,
   even if it was blocked before! */

/* BIEN: añadir a la máscara existente */
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGINT);
sigprocmask(SIG_BLOCK, &mask, NULL);
/* SIGINT is now blocked IN ADDITION to whatever was already blocked */
```

`SIG_BLOCK` es aditivo (unión). `SIG_SETMASK` es destructivo (reemplazo).
Usar `SIG_BLOCK` para bloquear y `SIG_SETMASK` con la máscara guardada
para restaurar.

### Error 3: Race condition entre desbloquear y esperar

```c
/* MAL: ventana de race */
sigprocmask(SIG_UNBLOCK, &mask, NULL);
/* ← signal can arrive HERE → handler runs → sets flag */
pause();
/* pause blocks forever — signal already delivered */

/* BIEN: sigsuspend (atómico) */
sigsuspend(&oldmask);
```

### Error 4: No restaurar la máscara al salir de sección crítica

```c
/* MAL: señales quedan bloqueadas permanentemente */
void do_critical_work(void)
{
    sigset_t mask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, NULL);

    if (some_error) {
        return;  /* SIGINT stays blocked forever! */
    }

    /* ... */
    sigprocmask(SIG_UNBLOCK, &mask, NULL);
}

/* BIEN: guardar y restaurar con cleanup en todos los paths */
void do_critical_work(void)
{
    sigset_t mask, oldmask;
    sigemptyset(&mask);
    sigaddset(&mask, SIGINT);
    sigprocmask(SIG_BLOCK, &mask, &oldmask);

    if (some_error) {
        sigprocmask(SIG_SETMASK, &oldmask, NULL);  /* Restore! */
        return;
    }

    /* ... */

    sigprocmask(SIG_SETMASK, &oldmask, NULL);
}
```

### Error 5: Heredar máscara bloqueada a hijos vía exec

```c
/* MAL: el hijo hereda señales bloqueadas del padre */
sigset_t mask;
sigemptyset(&mask);
sigaddset(&mask, SIGCHLD);
sigaddset(&mask, SIGPIPE);
sigprocmask(SIG_BLOCK, &mask, NULL);

if (fork() == 0) {
    execvp("child_program", argv);
    /* child_program starts with SIGCHLD and SIGPIPE blocked
       — will malfunction if it uses pipes or fork! */
    _exit(127);
}

/* BIEN: limpiar la máscara en el hijo antes de exec */
if (fork() == 0) {
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);

    execvp("child_program", argv);
    _exit(127);
}
```

---

## 10. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │              Máscara de Señales                              │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  sigset_t — conjunto de señales:                             │
  │    sigemptyset(&set)          → {}                           │
  │    sigfillset(&set)           → {all}                        │
  │    sigaddset(&set, SIGINT)    → add                          │
  │    sigdelset(&set, SIGINT)    → remove                       │
  │    sigismember(&set, SIGINT)  → 0 or 1                       │
  │    Siempre inicializar antes de usar                         │
  │                                                              │
  │  sigprocmask(how, &set, &oldset):                            │
  │    SIG_BLOCK    mask |= set    (añadir)                      │
  │    SIG_UNBLOCK  mask &= ~set   (quitar)                      │
  │    SIG_SETMASK  mask = set     (reemplazar)                  │
  │    (NULL, NULL, &old)          (solo leer)                   │
  │                                                              │
  │  Sección crítica:                                            │
  │    sigprocmask(SIG_BLOCK, &mask, &oldmask);                  │
  │    /* ... código protegido ... */                             │
  │    sigprocmask(SIG_SETMASK, &oldmask, NULL);                │
  │    → restaurar en TODOS los paths (error, return, etc.)      │
  │                                                              │
  │  sigpending(&set):                                           │
  │    Señales bloqueadas que han llegado (esperando entrega)    │
  │    Señales estándar (1-31) no se encolan                     │
  │                                                              │
  │  sigsuspend(&mask):                                          │
  │    Atómicamente: mask = mask_arg + sleep + restore           │
  │    Resuelve: race condition entre unblock y pause            │
  │    Siempre retorna -1 con errno = EINTR                      │
  │                                                              │
  │  Durante handler (sa_mask):                                  │
  │    Señal propia: bloqueada automáticamente                   │
  │    sa_mask signals: bloqueadas durante handler               │
  │    SA_NODEFER: no bloquear la propia señal                   │
  │    sigfillset(&sa.sa_mask): bloquear TODO en handler         │
  │                                                              │
  │  Herencia:                                                   │
  │    fork → hereda máscara   exec → preserva máscara           │
  │    → Limpiar con SIG_SETMASK + sigemptyset antes de exec    │
  │                                                              │
  │  Bloquear ≠ Ignorar:                                         │
  │    Bloquear: señal pendiente, se entrega al desbloquear      │
  │    Ignorar: señal descartada permanentemente                 │
  │                                                              │
  │  No bloqueables: SIGKILL (9), SIGSTOP (19)                   │
  └──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Operación atómica con sección crítica

Escribe un programa que mantenga un archivo de contadores actualizado
por el programa principal, y que un handler de SIGUSR1 lea para imprimir
un snapshot:

```c
typedef struct {
    unsigned long requests;
    unsigned long errors;
    unsigned long bytes;
} counters_t;

volatile counters_t counters;  /* Shared with handler */
```

Requisitos:
1. El programa principal incrementa los contadores en un loop.
2. SIGUSR1 dispara un handler que lee los contadores con `write()`.
3. Las actualizaciones de contadores deben ser atómicas respecto al
   handler: bloquear SIGUSR1 durante la actualización.
4. Usar `sigprocmask(SIG_BLOCK/SIG_SETMASK)` para la sección crítica.
5. Verificar que sin la sección crítica, el handler puede leer valores
   inconsistentes (ej: requests incrementado pero bytes no todavía).

```bash
$ ./atomic_counters &
[1] 5678

$ kill -USR1 5678
snapshot: requests=1042 errors=3 bytes=1567234

$ kill -USR1 5678
snapshot: requests=2108 errors=7 bytes=3145921
```

**Predicción antes de codificar**: Si no bloqueas SIGUSR1 durante la
actualización de los tres contadores, ¿qué valor inconsistente podría
ver el handler? Da un ejemplo concreto.

**Pregunta de reflexión**: ¿Se podría usar `sig_atomic_t` para cada
contador individualmente en vez de bloquear señales? ¿Garantizaría eso
consistencia entre los tres contadores?

---

### Ejercicio 2: Espera de señal con sigsuspend

Implementa una función `wait_for_signal` que espere una señal específica
de forma segura (sin race conditions):

```c
int wait_for_signal(int signum, int timeout_secs);
/* Returns:
 *  0  — signal received
 * -1  — timeout (if timeout_secs > 0)
 */
```

Requisitos:
1. Bloquear la señal con `sigprocmask`.
2. Instalar un handler que ponga un flag.
3. Usar `sigsuspend` para esperar atómicamente.
4. Implementar timeout con `alarm()`:
   - Si `timeout_secs > 0`, programar `alarm` antes de `sigsuspend`.
   - Si SIGALRM llega primero, retornar -1.
5. Restaurar la máscara y el handler de SIGALRM al terminar.
6. Ser seguro para llamar múltiples veces.

Probar con:
```c
printf("waiting for SIGUSR1 (10s timeout)...\n");
int r = wait_for_signal(SIGUSR1, 10);
if (r == 0)
    printf("got SIGUSR1!\n");
else
    printf("timed out\n");
```

**Predicción antes de codificar**: ¿Por qué `sigsuspend` es necesario
aquí en vez de simplemente `sigprocmask(UNBLOCK) + pause()`? Describe
la secuencia exacta de eventos que causa la race condition.

**Pregunta de reflexión**: `sigtimedwait()` combina esperar una señal
con timeout en una sola llamada. ¿Por qué no es una solución universal?
(Pista: ¿qué pasa si quieres esperar una señal Y un fd al mismo tiempo?)

---

### Ejercicio 3: Spawn seguro

Implementa una función `safe_spawn` que lance un proceso hijo con una
máscara y disposiciones de señales limpias:

```c
typedef struct {
    const char  *path;
    char *const *argv;
    char *const *envp;       /* NULL = inherit */
    const char  *cwd;        /* NULL = inherit */
    int          stdin_fd;   /* -1 = inherit */
    int          stdout_fd;  /* -1 = inherit */
    int          stderr_fd;  /* -1 = inherit */
} spawn_opts_t;

pid_t safe_spawn(const spawn_opts_t *opts);
```

Requisitos:
1. Bloquear SIGCHLD antes de fork (proteger la tabla de hijos).
2. En el hijo, antes de exec:
   - Restaurar la máscara de señales a vacía (`sigemptyset`).
   - Restaurar SIGPIPE, SIGCHLD, SIGINT a SIG_DFL.
   - Aplicar redirecciones de fds (dup2).
   - Cambiar cwd si se especifica.
   - Cerrar fds > 2 que no sean los redirigidos.
3. En el padre, después de fork:
   - Guardar el PID del hijo.
   - Desbloquear SIGCHLD.
4. Retornar el PID, o -1 en error.

**Predicción antes de codificar**: ¿Por qué es importante bloquear
SIGCHLD antes de fork y no después? ¿Qué pasa si el hijo muere
entre fork() y la línea que guarda su PID?

**Pregunta de reflexión**: `posix_spawn` hace algo muy similar a lo que
implementas aquí. ¿Por qué todavía es valioso saber hacer fork+exec
manual con limpieza de señales, si posix_spawn existe?
