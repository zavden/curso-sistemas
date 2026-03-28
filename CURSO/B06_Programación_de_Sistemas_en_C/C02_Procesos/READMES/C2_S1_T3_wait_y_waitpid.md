# wait y waitpid — Recoger el Estado de Salida de un Hijo

## Índice

1. [Por qué hay que esperar a los hijos](#1-por-qué-hay-que-esperar-a-los-hijos)
2. [Procesos zombie](#2-procesos-zombie)
3. [wait: esperar a cualquier hijo](#3-wait-esperar-a-cualquier-hijo)
4. [waitpid: control fino](#4-waitpid-control-fino)
5. [Macros de estado](#5-macros-de-estado)
6. [WNOHANG: espera no bloqueante](#6-wnohang-espera-no-bloqueante)
7. [Recoger múltiples hijos](#7-recoger-múltiples-hijos)
8. [SIGCHLD y reaping asíncrono](#8-sigchld-y-reaping-asíncrono)
9. [waitid: interfaz moderna](#9-waitid-interfaz-moderna)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Por qué hay que esperar a los hijos

Cuando un proceso hijo termina, el kernel **no libera completamente** su
entrada en la tabla de procesos. Retiene el PID y el estado de salida hasta
que el padre lo recoja. Esto es necesario porque el padre podría querer
saber:

- **Si terminó bien** (código de salida 0).
- **Qué código de error** devolvió (1-255).
- **Si fue matado** por una señal (y cuál).
- **Si generó un core dump**.

```
  Ciclo de vida de un proceso:

  fork()     exec()     exit()      wait()
    │          │          │           │
    ▼          ▼          ▼           ▼
  ┌─────┐   ┌─────┐   ┌─────────┐  ┌──────────┐
  │NUEVO │──▶│EJECU│──▶│ ZOMBIE  │──▶│ELIMINADO │
  │      │   │TANDO│   │(defunct)│  │(PID libre)│
  └─────┘   └─────┘   └─────────┘  └──────────┘
                           │
                     El kernel retiene:
                     - PID
                     - Exit status
                     - Tiempos de CPU
```

Si el padre nunca llama a `wait`, la entrada **zombie** permanece
indefinidamente. Esto es un **resource leak**: no consume CPU ni memoria
significativa, pero ocupa un slot en la tabla de procesos (limitada por
`/proc/sys/kernel/pid_max`, típicamente 32768 o 4194304).

---

## 2. Procesos zombie

Un **zombie** (o **defunct**) es un proceso que ya terminó pero cuyo padre
aún no ha recogido su estado.

### Cómo identificar zombies

```bash
# Estado Z en ps
ps aux | grep Z

# Aparece como <defunct>
ps -eo pid,ppid,stat,comm | grep defunct

# Contar zombies en el sistema
ps -eo stat | grep -c Z
```

### Cómo se crean

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void)
{
    pid_t pid = fork();

    if (pid == 0) {
        /* Child: exits immediately */
        printf("child PID %d exiting\n", getpid());
        _exit(0);
    }

    /* Parent: sleeps without calling wait */
    printf("parent PID %d sleeping (child is now zombie)\n", getpid());
    sleep(60);  /* During this time, child is zombie */

    return 0;
}
```

```
  $ ./zombie_demo &
  parent PID 1234 sleeping (child is now zombie)
  child PID 1235 exiting

  $ ps -o pid,ppid,stat,comm -p 1235
    PID  PPID STAT COMMAND
   1235  1234 Z+   zombie_demo <defunct>
```

### Qué pasa cuando el padre muere

Si el padre muere sin recoger al zombie, el hijo es **re-adoptado** por
PID 1 (init/systemd). El nuevo padre adoptivo llama `wait` automáticamente
y el zombie desaparece:

```
  Padre muere:

  Padre (PID 100)          Hijo zombie (PID 101)
       │                        │
       × (exit)                 │
                                │
  init (PID 1) ◄───── adopta ──┘
       │
  wait(101) ──▶ zombie eliminado
```

Esto significa que un zombie **no se acumula para siempre** a menos que el
padre siga vivo sin hacer `wait`. El problema real son los daemons de larga
duración que hacen fork sin recoger hijos.

---

## 3. wait: esperar a cualquier hijo

```c
#include <sys/wait.h>

pid_t wait(int *status);
```

- **Bloquea** hasta que **cualquier** hijo termine.
- Devuelve el PID del hijo que terminó.
- Almacena el estado de salida en `*status` (puede ser `NULL` si no interesa).
- Devuelve `-1` si no hay hijos (`errno = ECHILD`).

### Ejemplo básico

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Child */
        printf("child: doing work...\n");
        sleep(2);
        _exit(42);  /* Exit code 42 */
    }

    /* Parent */
    int status;
    pid_t finished = wait(&status);

    if (finished == -1) {
        perror("wait");
        return 1;
    }

    printf("child %d finished, status = 0x%04x\n", finished, status);

    if (WIFEXITED(status)) {
        printf("exit code: %d\n", WEXITSTATUS(status));
    }

    return 0;
}
```

```
  $ ./wait_basic
  child: doing work...
  (2 seconds pass)
  child 1235 finished, status = 0x2a00
  exit code: 42
```

### Limitación de wait

`wait` no permite:
- Esperar a un hijo **específico** (recoge el primero que termine).
- Hacer espera **no bloqueante**.
- Detectar paradas (`SIGSTOP`) o reanudaciones (`SIGCONT`).

Para todo eso existe `waitpid`.

---

## 4. waitpid: control fino

```c
#include <sys/wait.h>

pid_t waitpid(pid_t pid, int *status, int options);
```

### El parámetro pid

| Valor       | Significado                              |
|-------------|------------------------------------------|
| `> 0`       | Esperar al hijo con ese PID exacto       |
| `-1`        | Esperar a **cualquier** hijo (= `wait`)  |
| `0`         | Cualquier hijo del mismo **grupo de proceso** |
| `< -1`      | Cualquier hijo del grupo `abs(pid)`      |

### El parámetro options

| Flag           | Efecto                                        |
|----------------|-----------------------------------------------|
| `0`            | Bloquear hasta que el hijo termine             |
| `WNOHANG`     | Retornar inmediatamente si nadie ha terminado  |
| `WUNTRACED`   | También reportar hijos **detenidos** (SIGSTOP) |
| `WCONTINUED`  | También reportar hijos **reanudados** (SIGCONT)|

Los flags se combinan con OR:

```c
/* Non-blocking, also report stopped children */
waitpid(-1, &status, WNOHANG | WUNTRACED);
```

### Ejemplo: esperar a un hijo específico

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    pid_t fast = fork();
    if (fast == 0) {
        _exit(1);  /* Fast child */
    }

    pid_t slow = fork();
    if (slow == 0) {
        sleep(3);
        _exit(2);  /* Slow child */
    }

    /* Wait specifically for the slow child */
    int status;
    printf("waiting for slow child %d...\n", slow);
    waitpid(slow, &status, 0);
    printf("slow child finished with code %d\n", WEXITSTATUS(status));

    /* Now reap the fast child (already a zombie by now) */
    waitpid(fast, &status, 0);
    printf("fast child finished with code %d\n", WEXITSTATUS(status));

    return 0;
}
```

### Equivalencia con wait

```c
wait(&status);
/* es exactamente igual a: */
waitpid(-1, &status, 0);
```

---

## 5. Macros de estado

El entero `status` que devuelven `wait`/`waitpid` **no es** el código de
salida directamente. Es un valor empaquetado que hay que decodificar con
macros de `<sys/wait.h>`:

```
  Layout del status (simplificado, depende de implementación):

  Bits:  15       8  7       0
        ┌──────────┬──────────┐
        │exit code │  0x00    │  ← terminación normal (exit/return)
        ├──────────┼──────────┤
        │  0x00    │ signal # │  ← matado por señal
        ├──────────┼──────────┤
        │  0x00    │signal|80 │  ← señal + core dump (bit 7)
        ├──────────┼──────────┤
        │ signal # │  0x7f    │  ← detenido (stopped)
        └──────────┴──────────┘
```

### Macros de terminación normal

```c
if (WIFEXITED(status)) {
    /* The child called exit() or returned from main */
    int code = WEXITSTATUS(status);  /* 0-255 */
    printf("exited with code %d\n", code);
}
```

- `WIFEXITED(status)` — `true` si el hijo terminó normalmente.
- `WEXITSTATUS(status)` — extrae el código de salida (solo los 8 bits
  bajos, por eso el rango es 0-255).

### Macros de terminación por señal

```c
if (WIFSIGNALED(status)) {
    /* The child was killed by a signal */
    int sig = WTERMSIG(status);
    printf("killed by signal %d (%s)\n", sig, strsignal(sig));

    #ifdef WCOREDUMP
    if (WCOREDUMP(status)) {
        printf("core dump generated\n");
    }
    #endif
}
```

- `WIFSIGNALED(status)` — `true` si el hijo fue matado por una señal.
- `WTERMSIG(status)` — número de la señal que lo mató.
- `WCOREDUMP(status)` — `true` si generó core dump (no estándar POSIX,
  pero disponible en Linux con `#ifdef`).

### Macros de parada/reanudación

```c
if (WIFSTOPPED(status)) {
    /* Requires WUNTRACED in options */
    int sig = WSTOPSIG(status);
    printf("stopped by signal %d (%s)\n", sig, strsignal(sig));
}

if (WIFCONTINUED(status)) {
    /* Requires WCONTINUED in options */
    printf("resumed by SIGCONT\n");
}
```

### Ejemplo completo: decodificar cualquier estado

```c
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

void print_status(pid_t pid, int status)
{
    printf("child %d: ", pid);

    if (WIFEXITED(status)) {
        printf("exited, code=%d\n", WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        printf("killed by signal %d (%s)",
               WTERMSIG(status), strsignal(WTERMSIG(status)));
        #ifdef WCOREDUMP
        if (WCOREDUMP(status))
            printf(" (core dumped)");
        #endif
        printf("\n");
    } else if (WIFSTOPPED(status)) {
        printf("stopped by signal %d (%s)\n",
               WSTOPSIG(status), strsignal(WSTOPSIG(status)));
    } else if (WIFCONTINUED(status)) {
        printf("continued\n");
    }
}
```

### Tabla resumen de macros

| Macro               | Cuándo es true                     | Extrae            |
|----------------------|------------------------------------|--------------------|
| `WIFEXITED(s)`       | Terminó con `exit`/`_exit`/`return`| —                  |
| `WEXITSTATUS(s)`     | Solo si `WIFEXITED`                | Código (0-255)     |
| `WIFSIGNALED(s)`     | Matado por señal                   | —                  |
| `WTERMSIG(s)`        | Solo si `WIFSIGNALED`              | Número de señal    |
| `WCOREDUMP(s)`       | Solo si `WIFSIGNALED`              | Core dump (bool)   |
| `WIFSTOPPED(s)`      | Detenido (con `WUNTRACED`)         | —                  |
| `WSTOPSIG(s)`        | Solo si `WIFSTOPPED`               | Señal de parada    |
| `WIFCONTINUED(s)`    | Reanudado (con `WCONTINUED`)       | —                  |

---

## 6. WNOHANG: espera no bloqueante

`WNOHANG` hace que `waitpid` retorne **inmediatamente** si ningún hijo ha
cambiado de estado:

```c
pid_t result = waitpid(-1, &status, WNOHANG);

if (result > 0) {
    /* A child changed state */
    print_status(result, status);
} else if (result == 0) {
    /* No child has changed state yet */
    printf("no children ready\n");
} else {
    /* result == -1 */
    if (errno == ECHILD)
        printf("no children to wait for\n");
    else
        perror("waitpid");
}
```

```
  Comparación:

  waitpid(-1, &status, 0)        waitpid(-1, &status, WNOHANG)
  ┌─────────────────────┐        ┌─────────────────────┐
  │ ¿Algún hijo listo?  │        │ ¿Algún hijo listo?  │
  │       │              │        │       │              │
  │   Sí──┼──▶ retorna   │        │   Sí──┼──▶ retorna   │
  │       │              │        │       │              │
  │   No──┼──▶ BLOQUEA   │        │   No──┼──▶ retorna 0  │
  │       │   hasta que  │        │                      │
  │       │   uno termine│        └─────────────────────┘
  └─────────────────────┘
```

### Caso de uso: polling mientras se hace otro trabajo

```c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

int main(void)
{
    pid_t child = fork();
    if (child == 0) {
        sleep(5);
        _exit(0);
    }

    /* Do other work while waiting */
    for (;;) {
        int status;
        pid_t result = waitpid(child, &status, WNOHANG);

        if (result > 0) {
            printf("child finished: code %d\n", WEXITSTATUS(status));
            break;
        }
        if (result == -1) {
            perror("waitpid");
            break;
        }

        /* result == 0: child still running */
        printf("child still running, doing other work...\n");
        sleep(1);  /* Simulate work */
    }

    return 0;
}
```

> **Nota**: El polling con `WNOHANG` + `sleep` consume CPU innecesariamente.
> Para espera eficiente, es mejor usar `SIGCHLD` (sección 8) o `signalfd`.

---

## 7. Recoger múltiples hijos

Cuando se crean varios hijos, hay que recoger a **todos**. El patrón
estándar es un loop con `wait` o `waitpid`:

### Patrón 1: wait en loop

```c
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#define NUM_CHILDREN 5

int main(void)
{
    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();
        if (pid == -1) {
            perror("fork");
            break;
        }
        if (pid == 0) {
            /* Child: sleep random time, exit with index */
            sleep(i + 1);
            _exit(i);
        }
        printf("launched child %d (PID %d)\n", i, pid);
    }

    /* Reap all children */
    int status;
    pid_t pid;
    while ((pid = wait(&status)) > 0) {
        printf("child %d: ", pid);
        if (WIFEXITED(status))
            printf("code %d\n", WEXITSTATUS(status));
        else if (WIFSIGNALED(status))
            printf("signal %d\n", WTERMSIG(status));
    }
    /* Loop ends when wait returns -1 with errno == ECHILD */

    printf("all children collected\n");
    return 0;
}
```

### Patrón 2: waitpid con PIDs guardados

```c
pid_t children[NUM_CHILDREN];
int num_children = 0;

/* Launch */
for (int i = 0; i < NUM_CHILDREN; i++) {
    children[i] = fork();
    if (children[i] == 0) {
        /* ... child work ... */
        _exit(0);
    }
    if (children[i] > 0)
        num_children++;
}

/* Reap in order of creation */
for (int i = 0; i < num_children; i++) {
    int status;
    waitpid(children[i], &status, 0);
    printf("child[%d] (PID %d) done: code %d\n",
           i, children[i], WEXITSTATUS(status));
}
```

### Patrón 3: recoger en orden de finalización

```c
/* Reap in order of completion (who finishes first) */
int remaining = num_children;
while (remaining > 0) {
    int status;
    pid_t pid = waitpid(-1, &status, 0);
    if (pid == -1) break;

    /* Find which child this was */
    for (int i = 0; i < NUM_CHILDREN; i++) {
        if (children[i] == pid) {
            printf("child[%d] finished first, code %d\n",
                   i, WEXITSTATUS(status));
            break;
        }
    }
    remaining--;
}
```

---

## 8. SIGCHLD y reaping asíncrono

Cuando un hijo termina, el kernel envía `SIGCHLD` al padre. Esto permite
recoger hijos **sin bloquear** ni hacer polling:

```
  Flujo SIGCHLD:

  Hijo termina ──▶ kernel envía SIGCHLD al padre
                          │
                     ┌────┴────┐
                     │ handler │──▶ waitpid(-1, ..., WNOHANG) en loop
                     └─────────┘
```

### Implementación con sigaction

```c
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

volatile sig_atomic_t got_sigchld = 0;

void sigchld_handler(int sig)
{
    (void)sig;
    got_sigchld = 1;
    /* Do NOT call wait() here - not always async-signal-safe.
       Just set a flag and handle in main loop. */
}

int main(void)
{
    struct sigaction sa = {
        .sa_handler = sigchld_handler,
        .sa_flags   = SA_RESTART | SA_NOCLDSTOP,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    /* Launch children */
    for (int i = 0; i < 3; i++) {
        if (fork() == 0) {
            sleep(i + 1);
            _exit(i);
        }
    }

    int children_left = 3;
    while (children_left > 0) {
        /* Main work loop */
        pause();  /* Sleep until a signal arrives */

        if (got_sigchld) {
            got_sigchld = 0;

            /* Reap ALL available children (multiple may have died) */
            int status;
            pid_t pid;
            while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
                printf("reaped child %d, code %d\n",
                       pid, WEXITSTATUS(status));
                children_left--;
            }
        }
    }

    return 0;
}
```

### Puntos críticos del handler SIGCHLD

1. **Loop con WNOHANG**: Múltiples hijos pueden morir antes de que se
   ejecute el handler. Las señales no se encolan (una señal SIGCHLD puede
   representar N hijos muertos). Por eso se hace `while (waitpid(...) > 0)`.

2. **SA_NOCLDSTOP**: Sin este flag, `SIGCHLD` también se envía cuando un
   hijo se **detiene** (SIGSTOP), no solo cuando muere. Normalmente solo
   interesa la muerte.

3. **SA_RESTART**: Sin este flag, `SIGCHLD` interrumpe syscalls del padre
   (read, write, accept...) con `EINTR`. `SA_RESTART` las reinicia
   automáticamente.

4. **No llamar wait() en el handler**: Aunque `waitpid` está listado como
   async-signal-safe, es más seguro y limpio manejar en el loop principal.
   El handler solo pone un flag.

### SIG_IGN: descarte automático de zombies

Si **no necesitas** el código de salida de los hijos:

```c
/* Tell kernel: automatically reap children, don't create zombies */
signal(SIGCHLD, SIG_IGN);

/* Or with sigaction: */
struct sigaction sa = {
    .sa_handler = SIG_IGN,
    .sa_flags   = SA_NOCLDWAIT,  /* POSIX way */
};
sigaction(SIGCHLD, &sa, NULL);
```

Con `SIG_IGN`, el kernel **no crea zombies**: cuando un hijo muere, se
limpia automáticamente. `wait` nunca encontrará hijos terminados
(devuelve `-1` con `ECHILD`).

> **Cuándo usar**: Daemons que lanzan muchos hijos cuyos resultados no
> importan. No usar si necesitas saber si el hijo tuvo éxito.

---

## 9. waitid: interfaz moderna

`waitid` (POSIX.1-2001) es más flexible que `waitpid`:

```c
#include <sys/wait.h>

int waitid(idtype_t idtype, id_t id, siginfo_t *infop, int options);
```

### Parámetros

| idtype     | id              | Espera a                        |
|------------|------------------|---------------------------------|
| `P_PID`    | PID del hijo     | Ese hijo específico             |
| `P_PGID`   | Group ID         | Cualquier hijo del grupo        |
| `P_ALL`    | Ignorado         | Cualquier hijo                  |

Options adicionales:

| Flag        | Efecto                                           |
|-------------|--------------------------------------------------|
| `WEXITED`   | Reportar hijos que terminaron                     |
| `WSTOPPED`  | Reportar hijos detenidos                          |
| `WCONTINUED`| Reportar hijos reanudados                         |
| `WNOHANG`   | No bloquear                                       |
| `WNOWAIT`   | **No consumir** el evento (peek sin reap)         |

### Ventaja principal: WNOWAIT

`WNOWAIT` permite **inspeccionar** el estado de un hijo sin recogerlo.
Otro `waitid`/`waitpid` posterior aún lo verá:

```c
siginfo_t info;

/* Peek: see if child exited, but don't reap */
if (waitid(P_PID, child_pid, &info, WEXITED | WNOHANG | WNOWAIT) == 0) {
    if (info.si_pid != 0) {
        printf("child would report code %d (not reaped yet)\n",
               info.si_status);
    }
}

/* Later: actually reap */
waitid(P_PID, child_pid, &info, WEXITED);
```

### Campos de siginfo_t relevantes

```c
siginfo_t info;
waitid(P_ALL, 0, &info, WEXITED);

info.si_pid;     /* PID of the child */
info.si_uid;     /* Real UID of the child */
info.si_status;  /* Exit code OR signal number */
info.si_code;    /* CLD_EXITED, CLD_KILLED, CLD_DUMPED,
                    CLD_STOPPED, CLD_CONTINUED */
```

| si_code         | Significado                        |
|------------------|------------------------------------|
| `CLD_EXITED`    | Terminó con exit()                 |
| `CLD_KILLED`    | Matado por señal (sin core)        |
| `CLD_DUMPED`    | Matado por señal (con core dump)   |
| `CLD_STOPPED`   | Detenido                           |
| `CLD_CONTINUED` | Reanudado por SIGCONT              |

`waitid` es más limpio que decodificar con macros `WIFEXITED`/etc., pero
`waitpid` sigue siendo la interfaz más usada por familiaridad y
portabilidad.

---

## 10. Errores comunes

### Error 1: No recoger hijos → acumulación de zombies

```c
/* MAL: fork en loop sin wait */
for (int i = 0; i < 1000; i++) {
    if (fork() == 0) {
        do_work();
        _exit(0);
    }
    /* Parent continues without waiting → 1000 zombies */
}

/* BIEN: recoger con WNOHANG en cada iteración */
for (int i = 0; i < 1000; i++) {
    if (fork() == 0) {
        do_work();
        _exit(0);
    }

    /* Reap any finished children */
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
        ;  /* Reap all available */
}
/* Final cleanup */
while (waitpid(-1, NULL, 0) > 0)
    ;
```

### Error 2: Un solo waitpid en el handler SIGCHLD

```c
/* MAL: solo recoge uno, pero pueden haber muerto varios */
void handler(int sig) {
    (void)sig;
    int status;
    waitpid(-1, &status, WNOHANG);  /* Only reaps ONE */
}

/* BIEN: loop hasta agotar todos */
void handler(int sig) {
    (void)sig;
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
        ;
}
```

Las señales no se encolan: si tres hijos mueren casi simultáneamente,
pueden generar un solo `SIGCHLD`. El loop garantiza que se recogen todos.

### Error 3: Usar WEXITSTATUS sin verificar WIFEXITED

```c
/* MAL: si el hijo fue matado por señal, WEXITSTATUS da basura */
waitpid(pid, &status, 0);
int code = WEXITSTATUS(status);  /* Undefined if killed by signal */

/* BIEN: verificar primero */
waitpid(pid, &status, 0);
if (WIFEXITED(status)) {
    int code = WEXITSTATUS(status);
} else if (WIFSIGNALED(status)) {
    int sig = WTERMSIG(status);
}
```

### Error 4: Ignorar EINTR

```c
/* MAL: EINTR no significa error real */
if (waitpid(pid, &status, 0) == -1) {
    perror("waitpid");  /* Could just be EINTR */
    exit(1);
}

/* BIEN: reintentar en EINTR */
pid_t result;
do {
    result = waitpid(pid, &status, 0);
} while (result == -1 && errno == EINTR);

if (result == -1) {
    perror("waitpid");
    exit(1);
}
```

`EINTR` ocurre cuando una señal interrumpe `waitpid`. Es transitorio,
no un error real. Usar `SA_RESTART` en el handler de la señal también
evita esto.

### Error 5: Confundir el status con el exit code

```c
/* MAL: el status NO es el exit code directamente */
waitpid(pid, &status, 0);
printf("exit code = %d\n", status);  /* Wrong! status = 0x2a00 for exit(42) */

/* BIEN: usar las macros */
waitpid(pid, &status, 0);
if (WIFEXITED(status))
    printf("exit code = %d\n", WEXITSTATUS(status));  /* 42 */
```

El exit code está en los bits 15-8 del status. Por eso `exit(42)` produce
`status = 0x2a00` (42 << 8 = 0x2a00), no `42`.

---

## 11. Cheatsheet

```
  ┌──────────────────────────────────────────────────────────────┐
  │                    wait / waitpid                            │
  ├──────────────────────────────────────────────────────────────┤
  │                                                              │
  │  Básico:                                                     │
  │    wait(&status)              cualquier hijo, bloquea        │
  │    waitpid(pid, &st, 0)      hijo específico, bloquea       │
  │    waitpid(-1, &st, 0)       cualquier hijo (= wait)        │
  │    waitpid(-1, &st, WNOHANG) no bloquea, retorna 0 si nada │
  │                                                              │
  │  Macros de estado:                                           │
  │    WIFEXITED(s)   → WEXITSTATUS(s)     código 0-255         │
  │    WIFSIGNALED(s) → WTERMSIG(s)        número de señal      │
  │    WIFSTOPPED(s)  → WSTOPSIG(s)        señal de parada      │
  │    WIFCONTINUED(s)                      reanudado            │
  │    WCOREDUMP(s)                         core dump (Linux)    │
  │                                                              │
  │  Patrones:                                                   │
  │    while (wait(&st) > 0) {}         recoger todos            │
  │    while (waitpid(-1,&st,WNOHANG)>0)  reap no bloqueante    │
  │    signal(SIGCHLD, SIG_IGN)         no crear zombies         │
  │                                                              │
  │  SIGCHLD handler:                                            │
  │    SA_RESTART | SA_NOCLDSTOP                                 │
  │    Loop WNOHANG (señales no se encolan)                      │
  │                                                              │
  │  waitid(P_PID, pid, &info, WEXITED|WNOWAIT)                │
  │    → peek sin consumir + info.si_code más limpio             │
  │                                                              │
  │  Retornos:                                                   │
  │    > 0   PID del hijo recogido                               │
  │    0     WNOHANG y nadie listo                               │
  │    -1    error (ECHILD = no hay hijos, EINTR = señal)        │
  │                                                              │
  │  Zombie = hijo muerto + padre no hizo wait                   │
  │  Solución: wait, SIGCHLD handler, o SIG_IGN                  │
  └──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Monitor de procesos

Escribe un programa que lance N procesos hijos (N dado por argumento).
Cada hijo duerme un tiempo aleatorio (1-5 segundos) y sale con un código
entre 0 y 3. El padre debe:

1. Imprimir cuándo **lanza** cada hijo (con PID).
2. Recoger a **todos** los hijos en **orden de finalización**.
3. Imprimir por cada uno: PID, código de salida, tiempo que tardó.
4. Al final, imprimir un resumen: cuántos terminaron con código 0, 1, 2, 3.

**Predicción antes de codificar**: Si lanzas 5 hijos con tiempos aleatorios,
¿el orden de recolección será el mismo que el de lanzamiento? ¿Por qué?

**Pregunta de reflexión**: ¿Qué pasaría si un hijo es matado con `kill -9`
antes de terminar? ¿Tu programa lo detectaría correctamente?

---

### Ejercicio 2: Pool de workers con límite

Implementa un pool de procesos que ejecute M tareas con un máximo de N
workers simultáneos (M > N). Cada tarea es simplemente `sleep(seconds)`.

Especificación:
- Array de tareas: `int tasks[] = {3, 1, 4, 1, 5, 2, 3, 1};`
- Máximo de workers simultáneos: `MAX_WORKERS = 3`.
- Cuando un worker termina, lanza el siguiente task pendiente.
- Usar `WNOHANG` en un loop para detectar finalizaciones.

```
  Timeline esperado (MAX_WORKERS=3):

  t=0: launch task[0]=3s, task[1]=1s, task[2]=4s
  t=1: task[1] done → launch task[3]=1s
  t=2: task[3] done → launch task[4]=5s
  t=3: task[0] done → launch task[5]=2s
  t=4: task[2] done → launch task[6]=3s
  t=5: task[5] done → launch task[7]=1s
  ...
```

**Predicción antes de codificar**: ¿Cuánto tardará en total ejecutar todas
las tareas con 3 workers? Compáralo con ejecutarlas secuencialmente.

**Pregunta de reflexión**: ¿Qué ventaja tendría usar `SIGCHLD` en lugar del
polling con `WNOHANG + sleep` para detectar cuándo un worker termina?

---

### Ejercicio 3: Timeout para subprocesos

Escribe una función `run_with_timeout` que ejecute un comando externo con
un límite de tiempo:

```c
typedef struct {
    int    timed_out;    /* 1 if killed by timeout */
    int    exit_code;    /* valid only if !timed_out and exited normally */
    int    signal_num;   /* valid only if killed by signal */
} run_result_t;

run_result_t run_with_timeout(const char *cmd, int timeout_secs);
```

Implementación:
1. `fork()` → hijo ejecuta el comando con `execlp("/bin/sh", "sh", "-c", cmd, NULL)`.
2. Padre usa un loop con `waitpid(WNOHANG)` + `sleep(1)` para verificar
   si el hijo terminó. Decrementar un contador de timeout.
3. Si se agota el timeout, enviar `SIGTERM` al hijo. Esperar 2 segundos
   más. Si sigue vivo, enviar `SIGKILL`.
4. Rellenar y devolver `run_result_t`.

Probar con:
```c
run_result_t r;
r = run_with_timeout("sleep 2 && echo done", 5);   /* Should succeed */
r = run_with_timeout("sleep 100", 3);               /* Should timeout */
r = run_with_timeout("exit 42", 5);                 /* Should report 42 */
```

**Predicción antes de codificar**: ¿Por qué se envía primero `SIGTERM` y
luego `SIGKILL` en lugar de enviar `SIGKILL` directamente?

**Pregunta de reflexión**: ¿Qué problema tiene el enfoque `sleep(1)` + polling
si necesitas precisión de milisegundos? ¿Qué alternativa usarías?
