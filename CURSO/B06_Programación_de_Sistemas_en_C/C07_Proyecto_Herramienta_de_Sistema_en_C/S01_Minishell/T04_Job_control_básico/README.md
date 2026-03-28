# Job control básico

## Índice
1. [Sesiones, process groups y terminal](#1-sesiones-process-groups-y-terminal)
2. [La tabla de jobs](#2-la-tabla-de-jobs)
3. [Crear process groups al ejecutar](#3-crear-process-groups-al-ejecutar)
4. [Background con &](#4-background-con-)
5. [El built-in jobs](#5-el-built-in-jobs)
6. [El built-in fg](#6-el-built-in-fg)
7. [El built-in bg](#7-el-built-in-bg)
8. [SIGCHLD y reaping de jobs background](#8-sigchld-y-reaping-de-jobs-background)
9. [Notificación de jobs terminados](#9-notificación-de-jobs-terminados)
10. [Integración con el REPL](#10-integración-con-el-repl)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Sesiones, process groups y terminal

Para entender job control, necesitamos tres conceptos del kernel:

```
Sesión (SID = PID de la shell)
├── Process Group 1 (PGID = PID del líder)  ← foreground
│   ├── Proceso A (líder del grupo)
│   └── Proceso B (mismo pipeline)
├── Process Group 2 (PGID = PID del líder)  ← background job 1
│   └── Proceso C
└── Process Group 3 (PGID = PID del líder)  ← background job 2
    ├── Proceso D
    └── Proceso E

Terminal (tty):
  └── foreground process group ← recibe Ctrl+C, Ctrl+Z
```

### Session

Una sesión agrupa a la shell y todos los procesos que lanza. Se
crea con `setsid()`. La shell interactiva típicamente es el
**session leader** (su PID == SID):

```c
#include <unistd.h>

pid_t sid = getsid(0);    // obtener SID del proceso actual
// setsid();              // crear nueva sesión (rara vez necesario en la shell)
```

### Process group

Un process group agrupa los procesos de un mismo **job** (un
pipeline es un job). Cada proceso tiene un PGID:

```c
#include <unistd.h>

// Obtener el PGID de un proceso
pid_t pgid = getpgid(pid);

// Crear nuevo process group: el proceso se convierte en líder
setpgid(0, 0);      // equivale a setpgid(getpid(), getpid())

// Mover un proceso a un process group existente
setpgid(child_pid, pgid_leader);
```

`setpgid()` tiene restricciones:
- Solo puedes cambiar el PGID de tus hijos
- Solo antes de que el hijo haga exec (race condition)
- El PGID objetivo debe existir en la misma sesión

### Controlling terminal

El terminal tiene **un** foreground process group. Solo ese grupo
recibe señales del teclado (SIGINT, SIGTSTP, SIGQUIT):

```c
#include <unistd.h>

// ¿Quién es el foreground process group del terminal?
pid_t fg_pgid = tcgetpgrp(STDIN_FILENO);

// Poner un group en foreground
tcsetpgrp(STDIN_FILENO, new_pgid);
```

### Diagrama de señales de teclado

```
                  Terminal (tty)
                       │
                  Ctrl+C/Ctrl+Z
                       │
                       ▼
            ┌─────────────────────┐
            │  foreground process │
            │       group        │ ← señal entregada a TODOS
            │  (PGID = tcgetpgrp)│   los procesos de este grupo
            └─────────────────────┘

            ┌─────────────────────┐
            │  background process │ ← NO recibe Ctrl+C/Ctrl+Z
            │       group        │
            └─────────────────────┘
```

### ¿Por qué importa esto?

Sin process groups separados:

```
shell (PGID 1000) ← ignora SIGINT
  └── sleep 60 (PGID 1000)  ← también en PGID 1000

Ctrl+C → SIGINT a PGID 1000 → la shell lo ignora, sleep muere
→ funciona, PERO si lanzamos background:

shell (PGID 1000)
  ├── sleep 60 & (PGID 1000)  ← background
  └── cat (PGID 1000)          ← foreground

Ctrl+C → SIGINT a PGID 1000 → mata AMBOS: cat Y sleep
→ No queríamos matar sleep, solo cat
```

Con process groups separados:

```
shell (PGID 1000, session leader)
  ├── sleep 60 & (PGID 2001, background)  ← no recibe SIGINT
  └── cat (PGID 2002, foreground)          ← recibe SIGINT

Ctrl+C → SIGINT a PGID 2002 → solo mata cat
→ sleep sigue corriendo ✓
```

---

## 2. La tabla de jobs

La shell mantiene una tabla interna con todos los jobs activos
(background y stopped). Cada job tiene un número (`[1]`, `[2]`),
un estado, y los PIDs involucrados.

### Estados de un job

```
                  fork + &
RUNNING  ◄────────────────── nuevo job background
   │
   │ Ctrl+Z (SIGTSTP)
   ▼
STOPPED
   │
   │ fg → SIGCONT + waitpid
   │ bg → SIGCONT (sin waitpid)
   ▼
RUNNING
   │
   │ proceso termina
   ▼
 DONE    → se notifica y se elimina de la tabla
```

### Estructura de datos

```c
#include <sys/types.h>

#define MAX_JOBS 64

typedef enum {
    JOB_RUNNING,
    JOB_STOPPED,
    JOB_DONE
} job_state_t;

typedef struct {
    int       id;                // número de job: [1], [2], ...
    pid_t     pgid;              // process group ID (== PID del primer hijo)
    job_state_t state;
    char      cmdline[256];      // comando original para mostrar al usuario
    int       num_pids;          // cuántos procesos tiene el pipeline
    pid_t     pids[16];          // PIDs individuales del pipeline
} job_t;

// Tabla global de jobs
static job_t jobs[MAX_JOBS];
static int next_job_id = 1;
```

### ¿Por qué guardar PGID y no solo PID?

Un pipeline como `cat | sort | uniq` crea tres procesos. El job
es el pipeline completo. Todas las operaciones de job control
(fg, bg, kill) se envían al **process group**, no a PIDs individuales:

```
Job [1]: cat | sort | uniq
  PGID: 3001
  PIDs: [3001, 3002, 3003]

  kill(-3001, SIGCONT);    // envía SIGCONT a todo el group
  tcsetpgrp(0, 3001);     // pone el group en foreground
```

### Operaciones sobre la tabla

```c
// Añadir un job a la tabla
int add_job(pid_t pgid, pid_t *pids, int num_pids, const char *cmdline,
            job_state_t state)
{
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pgid == 0) {  // slot vacío
            jobs[i].id       = next_job_id++;
            jobs[i].pgid     = pgid;
            jobs[i].state    = state;
            jobs[i].num_pids = num_pids;
            strncpy(jobs[i].cmdline, cmdline, sizeof(jobs[i].cmdline) - 1);
            jobs[i].cmdline[sizeof(jobs[i].cmdline) - 1] = '\0';
            for (int j = 0; j < num_pids; j++)
                jobs[i].pids[j] = pids[j];
            return jobs[i].id;
        }
    }
    fprintf(stderr, "minish: too many jobs\n");
    return -1;
}

// Buscar job por ID ([1], [2], ...)
job_t *find_job_by_id(int job_id) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pgid != 0 && jobs[i].id == job_id)
            return &jobs[i];
    }
    return NULL;
}

// Buscar job por PGID
job_t *find_job_by_pgid(pid_t pgid) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pgid == pgid)
            return &jobs[i];
    }
    return NULL;
}

// Eliminar un job de la tabla
void remove_job(job_t *job) {
    job->pgid = 0;  // marcar slot como vacío
    job->id   = 0;
}
```

---

## 3. Crear process groups al ejecutar

Cada pipeline debe ejecutarse en su propio process group. El PGID
del grupo es el PID del primer proceso del pipeline.

### Race condition con setpgid

Hay una race condition sutil: entre `fork()` y `setpgid()` en el
hijo, el padre podría intentar hacer `setpgid()` sobre un hijo que
ya hizo exec, lo cual falla. La solución es que **tanto el padre
como el hijo** llamen a `setpgid()`:

```c
pid_t pid = fork();

if (pid == 0) {
    // --- Hijo ---
    setpgid(0, pgid);     // hijo se pone en el group
    // ... exec ...
} else {
    // --- Padre ---
    setpgid(pid, pgid);   // padre también, por si el hijo aún no lo hizo
    // Si el hijo ya lo hizo, setpgid retorna -1 con EACCES
    // (es esperado, no es un error real)
}
```

La doble llamada garantiza que, sin importar quién corra primero,
el proceso estará en el group correcto antes de continuar.

### Ejecución de pipeline con process groups

```c
int execute_pipeline_jc(pipeline_t *pl, const char *cmdline) {
    int n = pl->num_commands;
    pid_t pids[16];
    pid_t pgid = 0;

    // Crear pipes
    int pipes[16][2];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("minish: pipe");
            return -1;
        }
    }

    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("minish: fork");
            return -1;
        }

        if (pids[i] == 0) {
            // --- Hijo ---

            // Process group: el primer hijo crea el grupo,
            // los siguientes se unen al grupo del primero
            if (i == 0) {
                setpgid(0, 0);         // nuevo group: PGID = mi PID
            } else {
                setpgid(0, pgid);      // unirme al group del primero
            }

            // Restaurar señales
            signal(SIGINT,  SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            signal(SIGCHLD, SIG_DFL);

            // Configurar pipes
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < n - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Cerrar todos los pipes
            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Redirecciones del usuario
            apply_redirections(&pl->commands[i]);

            // Built-in en pipeline
            builtin_fn fn = find_builtin(pl->commands[i].argv[0]);
            if (fn) {
                _exit(fn(pl->commands[i].argv));
            }

            execvp(pl->commands[i].argv[0], pl->commands[i].argv);
            fprintf(stderr, "minish: %s: command not found\n",
                    pl->commands[i].argv[0]);
            _exit(127);
        }

        // --- Padre ---
        if (i == 0) {
            pgid = pids[0];           // PGID del grupo = PID del primer hijo
        }
        setpgid(pids[i], pgid);       // doble setpgid (padre también)
    }

    // Padre cierra todos los pipes
    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    if (pl->background) {
        // Job en background: añadir a la tabla y retornar
        int jid = add_job(pgid, pids, n, cmdline, JOB_RUNNING);
        printf("[%d] %d\n", jid, pgid);
        return 0;
    } else {
        // Job en foreground: dar control del terminal y esperar
        return wait_for_foreground(pgid, pids, n, cmdline);
    }
}
```

### Esperar un foreground job

```c
int wait_for_foreground(pid_t pgid, pid_t *pids, int n,
                        const char *cmdline)
{
    // Dar el terminal al process group del job
    tcsetpgrp(STDIN_FILENO, pgid);

    // Esperar a todos los procesos del pipeline
    int status = 0;
    int stopped = 0;

    for (int i = 0; i < n; i++) {
        int s;
        // WUNTRACED: también retorna si el hijo fue detenido
        waitpid(pids[i], &s, WUNTRACED);

        if (WIFSTOPPED(s)) {
            stopped = 1;
        }
        // Guardar el status del último comando del pipeline
        if (i == n - 1) {
            status = s;
        }
    }

    // Devolver el terminal a la shell
    tcsetpgrp(STDIN_FILENO, getpgrp());

    if (stopped) {
        // Algún proceso fue detenido (Ctrl+Z)
        int jid = add_job(pgid, pids, n, cmdline, JOB_STOPPED);
        printf("\n[%d]+ Stopped\t\t%s\n", jid, cmdline);
        return 148;  // 128 + 20 (SIGTSTP = 20)
    }

    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}
```

### Flujo visual

```
minish$ cat | sort | uniq

fork() → hijo 1 (PID 3001)
  setpgid(0, 0)               → PGID 3001 (nuevo grupo)
  exec("cat")

fork() → hijo 2 (PID 3002)
  setpgid(0, 3001)            → PGID 3001 (mismo grupo)
  exec("sort")

fork() → hijo 3 (PID 3003)
  setpgid(0, 3001)            → PGID 3001 (mismo grupo)
  exec("uniq")

Padre:
  setpgid(3001, 3001)         → por si hijo 1 aún no lo hizo
  setpgid(3002, 3001)         → por si hijo 2 aún no lo hizo
  setpgid(3003, 3001)         → por si hijo 3 aún no lo hizo
  tcsetpgrp(0, 3001)          → foreground = grupo 3001
  waitpid(3001), waitpid(3002), waitpid(3003)
  tcsetpgrp(0, shell_pgid)   → foreground = shell otra vez
```

---

## 4. Background con &

Cuando el usuario termina un comando con `&`, el job se ejecuta
en background. La diferencia con foreground es mínima:

```
Foreground:
  1. Crear process group
  2. tcsetpgrp(0, pgid)       ← dar terminal al job
  3. waitpid(..., WUNTRACED)  ← esperar a que termine
  4. tcsetpgrp(0, shell_pgid) ← devolver terminal

Background:
  1. Crear process group
  2. Añadir a tabla de jobs   ← no dar terminal
  3. Imprimir [N] PID         ← no esperar
  4. Retornar al prompt
```

### ¿Qué pasa si un proceso background intenta leer del terminal?

El kernel envía `SIGTTIN` al proceso. El comportamiento por
defecto es detener el proceso:

```
minish$ cat &
[1] 4001

[1]+ Stopped (ttin)    cat

# cat intentó leer de stdin → SIGTTIN → stopped
# Solo el foreground process group puede leer del terminal
```

Similarmente, si un proceso background intenta escribir al terminal
y el terminal tiene `TOSTOP` habilitado, recibe `SIGTTOU`. En la
práctica, la mayoría de terminales permiten escritura desde
background (TOSTOP está deshabilitado por defecto).

```
Señales del terminal para background processes:

Operación          Señal       Acción por defecto
─────────────────────────────────────────────────
Leer del terminal  SIGTTIN     Stop el proceso
Escribir (TOSTOP)  SIGTTOU     Stop el proceso
Ctrl+C             -           No se entrega
Ctrl+Z             -           No se entrega
```

---

## 5. El built-in jobs

`jobs` muestra todos los jobs activos con su estado:

```
minish$ sleep 100 &
[1] 5001
minish$ sleep 200 &
[2] 5002
minish$ sleep 300
^Z
[3]+ Stopped         sleep 300
minish$ jobs
[1]   Running         sleep 100 &
[2]-  Running         sleep 200 &
[3]+  Stopped         sleep 300
```

### Convenciones de notación

- `+` marca el job **actual** (current): el más reciente que fue
  detenido o enviado a background. Es el que `fg` y `bg` usan
  sin argumentos
- `-` marca el job **anterior** (previous): el segundo más reciente

### Implementación

```c
int builtin_jobs(char **argv) {
    (void)argv;

    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pgid == 0)
            continue;

        const char *state_str;
        switch (jobs[i].state) {
            case JOB_RUNNING: state_str = "Running";  break;
            case JOB_STOPPED: state_str = "Stopped";  break;
            case JOB_DONE:    state_str = "Done";      break;
        }

        // Determinar si es current (+) o previous (-)
        char marker = ' ';
        if (jobs[i].id == current_job_id)
            marker = '+';
        else if (jobs[i].id == previous_job_id)
            marker = '-';

        printf("[%d]%c  %-10s\t%s%s\n",
               jobs[i].id,
               marker,
               state_str,
               jobs[i].cmdline,
               jobs[i].state == JOB_RUNNING ? " &" : "");
    }
    return 0;
}
```

### Tracking de current y previous job

```c
static int current_job_id  = 0;   // el job "+"
static int previous_job_id = 0;   // el job "-"

// Actualizar al crear/detener un job
void update_current_job(int new_id) {
    if (new_id != current_job_id) {
        previous_job_id = current_job_id;
        current_job_id  = new_id;
    }
}
```

Cada vez que un job se detiene (Ctrl+Z) o se envía a background,
se convierte en el current job.

---

## 6. El built-in fg

`fg` mueve un job al foreground: le da el terminal, le envía
`SIGCONT` (para resumir si estaba stopped), y espera a que termine.

```
minish$ sleep 100 &
[1] 5001
minish$ fg %1
sleep 100          ← la shell espera
```

### Sintaxis

```
fg          → resume el current job (+)
fg %N       → resume el job N
fg %string  → resume el job cuyo cmdline empieza con string
```

### Implementación

```c
int builtin_fg(char **argv) {
    job_t *job;

    if (argv[1] == NULL) {
        // fg sin argumentos: current job
        job = find_job_by_id(current_job_id);
        if (job == NULL) {
            fprintf(stderr, "minish: fg: no current job\n");
            return 1;
        }
    } else if (argv[1][0] == '%') {
        // fg %N
        int jid = atoi(&argv[1][1]);
        job = find_job_by_id(jid);
        if (job == NULL) {
            fprintf(stderr, "minish: fg: %%%d: no such job\n", jid);
            return 1;
        }
    } else {
        fprintf(stderr, "minish: fg: usage: fg [%%job_id]\n");
        return 1;
    }

    // Imprimir el comando que se reanuda
    printf("%s\n", job->cmdline);

    // Poner el job en foreground
    job->state = JOB_RUNNING;
    tcsetpgrp(STDIN_FILENO, job->pgid);

    // Si estaba stopped, enviar SIGCONT a todo el process group
    kill(-job->pgid, SIGCONT);  // kill con PGID negativo → al grupo

    // Esperar al job
    int status = 0;
    int stopped = 0;

    for (int i = 0; i < job->num_pids; i++) {
        int s;
        pid_t ret = waitpid(job->pids[i], &s, WUNTRACED);
        if (ret == -1)
            continue;  // ya fue recolectado

        if (WIFSTOPPED(s)) {
            stopped = 1;
        }
        if (i == job->num_pids - 1) {
            status = s;
        }
    }

    // Devolver terminal a la shell
    tcsetpgrp(STDIN_FILENO, getpgrp());

    if (stopped) {
        job->state = JOB_STOPPED;
        update_current_job(job->id);
        printf("\n[%d]+ Stopped\t\t%s\n", job->id, job->cmdline);
        return 148;
    }

    // Job terminó: eliminarlo de la tabla
    remove_job(job);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
```

### Flujo visual de fg

```
Estado inicial:
  Job [1] STOPPED: sleep 100 (PGID 5001)
  Terminal: foreground = shell (PGID 1000)

fg %1:
  1. tcsetpgrp(0, 5001)       → terminal: foreground = 5001
  2. kill(-5001, SIGCONT)      → sleep recibe SIGCONT, reanuda
  3. waitpid(5001, WUNTRACED)  → shell espera
  4. sleep termina (o ^Z)
  5. tcsetpgrp(0, 1000)        → terminal: foreground = shell

                Terminal
                   │
  Antes:    fg = shell(1000)      bg = sleep(5001) STOPPED
  fg %1:    fg = sleep(5001)      (shell espera)
  Termina:  fg = shell(1000)      (sleep ya no existe)
```

### ¿Por qué kill con PGID negativo?

`kill(-pgid, sig)` envía la señal a **todos** los procesos del
process group. Esto es necesario para pipelines: si el job es
`cat | sort | uniq` y fue detenido, necesitamos resumir los tres
procesos:

```c
kill(-3001, SIGCONT);
// Equivalente a:
// kill(3001, SIGCONT);  // cat
// kill(3002, SIGCONT);  // sort
// kill(3003, SIGCONT);  // uniq
```

---

## 7. El built-in bg

`bg` reanuda un job stopped en **background**: le envía `SIGCONT`
pero **no** le da el terminal ni espera.

```
minish$ sleep 100
^Z
[1]+ Stopped         sleep 100
minish$ bg %1
[1]+ sleep 100 &
minish$                   ← shell disponible inmediatamente
```

### Implementación

```c
int builtin_bg(char **argv) {
    job_t *job;

    if (argv[1] == NULL) {
        // bg sin argumentos: current job
        job = find_job_by_id(current_job_id);
        if (job == NULL) {
            fprintf(stderr, "minish: bg: no current job\n");
            return 1;
        }
    } else if (argv[1][0] == '%') {
        int jid = atoi(&argv[1][1]);
        job = find_job_by_id(jid);
        if (job == NULL) {
            fprintf(stderr, "minish: bg: %%%d: no such job\n", jid);
            return 1;
        }
    } else {
        fprintf(stderr, "minish: bg: usage: bg [%%job_id]\n");
        return 1;
    }

    if (job->state != JOB_STOPPED) {
        fprintf(stderr, "minish: bg: job %d already running\n", job->id);
        return 1;
    }

    // Reanudar en background
    job->state = JOB_RUNNING;
    printf("[%d]+ %s &\n", job->id, job->cmdline);

    // Enviar SIGCONT al process group (sin dar terminal)
    kill(-job->pgid, SIGCONT);

    return 0;
}
```

### Diferencia clave entre fg y bg

```
                    fg                          bg
                    ──                          ──
tcsetpgrp()         SÍ (dar terminal al job)    NO
kill(-pgid,SIGCONT) SÍ                          SÍ
waitpid()           SÍ (bloquear)               NO
Job puede leer tty  SÍ                          NO (→ SIGTTIN)
```

---

## 8. SIGCHLD y reaping de jobs background

Los jobs background terminan **asíncronamente** — la shell no
les hace waitpid inmediatamente. Necesitamos un handler de SIGCHLD
para recolectarlos.

### El problema de los zombies background

Sin reaping, cada proceso background que termina se queda como
zombie:

```
minish$ sleep 1 &
[1] 6001
minish$             ← un segundo después...
# PID 6001 terminó pero nadie hizo waitpid
# → zombie hasta que la shell termine
```

### Handler de SIGCHLD

```c
// Variable global: flag para indicar que hay jobs que notificar
static volatile sig_atomic_t sigchld_received = 0;

void sigchld_handler(int sig) {
    (void)sig;
    sigchld_received = 1;
    // No hacer waitpid aquí — es delicado en signal handlers.
    // Solo marcar un flag y procesar en el REPL loop.
}

// Instalar el handler:
void setup_sigchld(void) {
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sa.sa_flags   = SA_RESTART | SA_NOCLDSTOP;
    // SA_NOCLDSTOP: no recibir SIGCHLD cuando un hijo se detiene
    //               (solo cuando termina)
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);
}
```

### Procesar hijos terminados (fuera del handler)

```c
void reap_background_jobs(void) {
    if (!sigchld_received)
        return;
    sigchld_received = 0;

    pid_t pid;
    int status;

    // waitpid con WNOHANG: no bloquear, recolectar todos los
    // hijos que hayan terminado
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {

        // Buscar a qué job pertenece este PID
        job_t *job = NULL;
        for (int i = 0; i < MAX_JOBS; i++) {
            if (jobs[i].pgid == 0) continue;
            for (int j = 0; j < jobs[i].num_pids; j++) {
                if (jobs[i].pids[j] == pid) {
                    job = &jobs[i];
                    break;
                }
            }
            if (job) break;
        }

        if (job == NULL)
            continue;  // proceso que no es un job conocido

        if (WIFEXITED(status) || WIFSIGNALED(status)) {
            // Verificar si TODOS los procesos del job terminaron
            int all_done = 1;
            for (int j = 0; j < job->num_pids; j++) {
                if (job->pids[j] == pid) {
                    job->pids[j] = -1;  // marcar como recolectado
                } else if (job->pids[j] > 0) {
                    // Intentar recolectar sin bloquear
                    int s;
                    pid_t r = waitpid(job->pids[j], &s, WNOHANG);
                    if (r == job->pids[j]) {
                        job->pids[j] = -1;
                    } else {
                        all_done = 0;
                    }
                }
            }

            if (all_done) {
                job->state = JOB_DONE;
                // La notificación se imprime en el REPL loop
            }
        } else if (WIFSTOPPED(status)) {
            job->state = JOB_STOPPED;
            update_current_job(job->id);
        }
    }
}
```

### ¿Por qué no hacer waitpid dentro del handler?

Los signal handlers deben ser mínimos — solo funciones
async-signal-safe. `printf`, `malloc`, y muchas otras funciones
**no** son async-signal-safe. Si el handler interrumpe a `printf`
a mitad de camino y el handler también llama `printf`, el resultado
es comportamiento indefinido.

El patrón seguro es:

```
Signal handler:
  → poner un flag (sig_atomic_t)

REPL loop (código normal):
  → verificar el flag
  → hacer waitpid + printf + actualizar tabla
```

---

## 9. Notificación de jobs terminados

Cuando un job background termina, la shell debe notificar al
usuario. Bash hace esto justo antes de imprimir el siguiente prompt:

```
minish$ sleep 1 &
[1] 7001
minish$ ls
file1  file2
[1]+  Done                    sleep 1
minish$
```

### Implementación

```c
void notify_completed_jobs(void) {
    for (int i = 0; i < MAX_JOBS; i++) {
        if (jobs[i].pgid == 0)
            continue;

        if (jobs[i].state == JOB_DONE) {
            printf("[%d]+  Done\t\t\t%s\n", jobs[i].id, jobs[i].cmdline);
            remove_job(&jobs[i]);
        }
    }
}
```

### Integración en el prompt

```c
// En el REPL loop, justo antes de print_prompt():
reap_background_jobs();
notify_completed_jobs();
print_prompt();
```

### Notificación de stopped por SIGTTIN

Si un job background intenta leer del terminal, recibe SIGTTIN
y se detiene. Podemos detectar esto con WIFSTOPPED en el reaper:

```
minish$ cat &
[1] 8001
minish$              ← inmediatamente:
[1]+ Stopped (ttin)  cat
```

---

## 10. Integración con el REPL

Veamos cómo todos los componentes se conectan en el REPL principal:

### REPL completo con job control

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <termios.h>

// Forward declarations
int tokenize(const char *line, char **tokens, int max_tokens);
int parse_pipeline(char **tokens, int ntokens, pipeline_t *pl);
int execute_pipeline_jc(pipeline_t *pl, const char *cmdline);
void reap_background_jobs(void);
void notify_completed_jobs(void);

// Dispatch table de built-ins
typedef int (*builtin_fn)(char **argv);

typedef struct {
    const char  *name;
    builtin_fn   func;
} builtin_entry_t;

static builtin_entry_t builtins[] = {
    {"cd",     builtin_cd},
    {"exit",   builtin_exit},
    {"export", builtin_export},
    {"unset",  builtin_unset},
    {"echo",   builtin_echo},
    {"pwd",    builtin_pwd},
    {"jobs",   builtin_jobs},
    {"fg",     builtin_fg},
    {"bg",     builtin_bg},
    {NULL, NULL}
};

builtin_fn find_builtin(const char *name) {
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(builtins[i].name, name) == 0)
            return builtins[i].func;
    }
    return NULL;
}

int main(void) {
    char *line = NULL;
    size_t bufsize = 0;

    // La shell debe ser el líder de su propio process group
    // para controlar el terminal
    pid_t shell_pgid = getpid();
    setpgid(shell_pgid, shell_pgid);
    tcsetpgrp(STDIN_FILENO, shell_pgid);

    // Configurar señales
    signal(SIGINT,  SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    signal(SIGTSTP, SIG_IGN);
    signal(SIGTTIN, SIG_IGN);  // ignorar para que la shell no se detenga
    signal(SIGTTOU, SIG_IGN);  // ignorar para poder usar tcsetpgrp
    setup_sigchld();

    // Handler de SIGINT para reimprimir prompt
    struct sigaction sa_int;
    sa_int.sa_handler = shell_sigint_handler;
    sa_int.sa_flags   = 0;  // no SA_RESTART
    sigemptyset(&sa_int.sa_mask);
    sigaction(SIGINT, &sa_int, NULL);

    for (;;) {
        // Notificar jobs terminados antes del prompt
        reap_background_jobs();
        notify_completed_jobs();

        // Prompt
        fprintf(stderr, "minish$ ");

        // Leer línea
        ssize_t n = getline(&line, &bufsize, stdin);
        if (n == -1) {
            if (errno == EINTR) {
                clearerr(stdin);
                continue;
            }
            printf("\n");
            break;  // EOF (Ctrl+D)
        }

        // Eliminar newline
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';

        // Línea vacía
        if (line[0] == '\0')
            continue;

        // Guardar cmdline original para la tabla de jobs
        char cmdline[256];
        strncpy(cmdline, line, sizeof(cmdline) - 1);
        cmdline[sizeof(cmdline) - 1] = '\0';

        // Tokenizar
        char *tokens[128];
        int ntokens = tokenize(line, tokens, 128);
        if (ntokens <= 0)
            continue;

        // Parsear
        pipeline_t pl;
        if (parse_pipeline(tokens, ntokens, &pl) == -1)
            continue;

        // Built-in (comando simple, no pipeline)
        if (pl.num_commands == 1 && !pl.background) {
            builtin_fn fn = find_builtin(pl.commands[0].argv[0]);
            if (fn != NULL) {
                last_exit_status = fn(pl.commands[0].argv);
                continue;
            }
        }

        // Ejecutar pipeline con job control
        last_exit_status = execute_pipeline_jc(&pl, cmdline);
    }

    free(line);
    return last_exit_status;
}
```

### Señales que la shell debe ignorar

```
Señal      ¿Por qué ignorar?
─────────────────────────────────────────────────────────────
SIGINT     Ctrl+C no debe matar la shell
SIGQUIT    Ctrl+\ no debe matar la shell
SIGTSTP    Ctrl+Z no debe detener la shell
SIGTTIN    La shell puede necesitar leer del terminal
           aun siendo background (durante tcsetpgrp)
SIGTTOU    La shell debe poder llamar tcsetpgrp
           sin ser detenida
```

`SIGTTIN` y `SIGTTOU` son nuevos en job control. Cuando la shell
llama `tcsetpgrp()` para devolver el terminal a sí misma después
de que un foreground job termina, la shell técnicamente es un
proceso background en ese instante (el terminal pertenece al job
que acaba de terminar). Sin ignorar `SIGTTOU`, la shell se
detendría al intentar `tcsetpgrp()`.

### Diagrama de flujo completo

```
                    ┌─────────────────────────────────┐
                    │         reap_background_jobs()   │
                    │       notify_completed_jobs()     │
                    └───────────────┬─────────────────┘
                                    ▼
                    ┌─────────────────────────────────┐
                    │         print_prompt()            │
                    │         getline()                 │
                    └───────────────┬─────────────────┘
                                    ▼
                              ┌───────────┐
                              │ tokenize  │
                              │  parse    │
                              └─────┬─────┘
                                    │
                         ┌──────────┴──────────┐
                         │                     │
                    builtin_fn?           fork + exec
                    ┌─────┴─────┐      ┌───────┴───────┐
                    │           │      │               │
              cmd simple  pipeline  foreground     background
                    │        │      ┌──┴──┐        ┌──┴──┐
               ejecutar   hijo:   setpgid   add_job    add_job
               en shell   exec    tcsetpgrp  print [N]
                    │             waitpid     retornar
                    │             tcsetpgrp
                    ▼             (shell)
              ┌──────────┐        │
              │ continuar │◄──────┘
              │   REPL    │
              └──────────┘
```

---

## 11. Errores comunes

### Error 1: no hacer doble setpgid (race condition)

```c
// MAL: solo setpgid en el hijo
pid_t pid = fork();
if (pid == 0) {
    setpgid(0, pgid);   // ¿y si el padre hace tcsetpgrp antes?
    exec(...);
}
// Padre inmediatamente hace:
tcsetpgrp(STDIN_FILENO, pgid);
// El hijo podría no haber ejecutado setpgid aún
// → tcsetpgrp falla con EPERM (pgid no existe en esta sesión)
```

**Solución**: tanto padre como hijo llaman `setpgid()`:

```c
if (pid == 0) {
    setpgid(0, pgid);
    exec(...);
} else {
    setpgid(pid, pgid);  // garantiza que el grupo existe
    tcsetpgrp(STDIN_FILENO, pgid);
}
```

### Error 2: no ignorar SIGTTOU en la shell

```c
// MAL: la shell no ignora SIGTTOU
tcsetpgrp(STDIN_FILENO, shell_pgid);
// Si la shell no es foreground en ese instante,
// recibe SIGTTOU → la shell se detiene → terminal cuelga
```

**Solución**: ignorar `SIGTTOU` en la shell:

```c
signal(SIGTTOU, SIG_IGN);
// Ahora tcsetpgrp siempre funciona sin detener la shell
```

### Error 3: no devolver el terminal a la shell después de fg

```c
// MAL: fg sin restaurar terminal
tcsetpgrp(STDIN_FILENO, job->pgid);
kill(-job->pgid, SIGCONT);
waitpid(...);
// Olvidamos: tcsetpgrp(STDIN_FILENO, shell_pgid);
// → la shell ya no es foreground
// → la próxima lectura del terminal genera SIGTTIN
// → la shell se detiene (si no ignoramos SIGTTIN)
```

**Solución**: siempre restaurar el terminal después de waitpid:

```c
waitpid(...);
tcsetpgrp(STDIN_FILENO, getpgrp());  // devolver terminal a la shell
```

### Error 4: usar kill(pid, SIGCONT) en vez de kill(-pgid, SIGCONT)

```c
// MAL: SIGCONT solo al primer proceso del pipeline
kill(job->pids[0], SIGCONT);
// sort y uniq siguen stopped → el pipeline se cuelga
```

```c
// BIEN: SIGCONT a todo el process group
kill(-job->pgid, SIGCONT);
// cat, sort y uniq reciben SIGCONT → todos reanudan
```

### Error 5: recolectar foreground jobs en SIGCHLD handler

```c
// MAL: el handler de SIGCHLD recolecta TODO
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
    // Esto recolecta también los hijos del foreground job
    // → el waitpid del REPL loop falla con ECHILD
    // → la shell piensa que el job terminó instantáneamente
}
```

**Solución**: usar `SA_NOCLDSTOP` y ser cuidadoso con qué se
recolecta. Una alternativa común es no usar SIGCHLD handler y
solo hacer reaping manual en el REPL loop:

```c
// Enfoque simple: sin handler, reap en el REPL loop
void reap_background_jobs(void) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0) {
        // Actualizar la tabla de jobs...
    }
}
```

Este enfoque es más simple y evita las complejidades de signal
handlers vs foreground waits. La desventaja es que los zombies
background persisten hasta el próximo prompt (pero solo un instante).

---

## 12. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              Job Control — Mini-shell                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  JERARQUÍA:                                                  │
│    Session > Process Group > Process                         │
│    Un job = un pipeline = un process group                   │
│                                                              │
│  CREAR PROCESS GROUP:                                        │
│    setpgid(0, 0)          hijo: crear grupo (PGID = mi PID) │
│    setpgid(pid, pgid)     padre: confirmar (doble setpgid)  │
│                                                              │
│  CONTROLAR TERMINAL:                                         │
│    tcsetpgrp(fd, pgid)    dar foreground a un grupo          │
│    tcgetpgrp(fd)          ¿quién es foreground?              │
│                                                              │
│  SEÑALES:                                                    │
│    kill(-pgid, sig)       enviar señal a todo el grupo       │
│    SIGCONT                reanudar un proceso stopped        │
│    SIGTTIN                background intenta leer tty        │
│    SIGTTOU                background intenta escribir tty    │
│                                                              │
│  TABLA DE JOBS:                                              │
│    add_job(pgid, pids, ...)  añadir nuevo job                │
│    find_job_by_id(N)         buscar por [N]                  │
│    remove_job(job)           eliminar job completado         │
│                                                              │
│  BUILT-INS DE JOB CONTROL:                                   │
│    jobs        mostrar todos los jobs                        │
│    fg [%N]     foreground: tcsetpgrp + SIGCONT + waitpid     │
│    bg [%N]     background: SIGCONT (sin tcsetpgrp ni wait)   │
│                                                              │
│  FOREGROUND vs BACKGROUND:                                   │
│    FG: setpgid → tcsetpgrp → waitpid → tcsetpgrp(shell)     │
│    BG: setpgid → add_job → print → retornar                 │
│                                                              │
│  REAPING:                                                    │
│    waitpid(-1, WNOHANG | WUNTRACED)  recolectar sin bloqueo │
│    Llamar antes de cada prompt                               │
│    Notificar jobs DONE al usuario                            │
│                                                              │
│  SHELL DEBE IGNORAR:                                         │
│    SIGINT, SIGQUIT, SIGTSTP, SIGTTIN, SIGTTOU               │
│                                                              │
│  HIJOS DEBEN RESTAURAR:                                      │
│    signal(SIGINT/SIGQUIT/SIGTSTP, SIG_DFL)                  │
│                                                              │
│  ESTADOS DEL JOB:                                            │
│    RUNNING ──Ctrl+Z──▶ STOPPED ──fg/bg──▶ RUNNING           │
│    RUNNING ──termina──▶ DONE ──notificar──▶ (eliminado)      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: tabla de jobs con kill %N

Implementa una versión de la tabla de jobs que soporte el comando
`kill %N` para enviar SIGTERM a un job específico.

**Pasos**:
1. Define la estructura `job_t` con id, pgid, state, cmdline
2. Implementa `add_job()`, `find_job_by_id()`, `remove_job()`
3. Implementa `builtin_jobs()` que liste los jobs activos
4. Implementa `builtin_kill()` que parsee `%N` y envíe SIGTERM
5. Implementa el reaping con waitpid(WNOHANG) en el REPL loop

**Predicción antes de ejecutar**: si lanzas `sleep 100 &` y luego
haces `kill %1`, ¿qué señal recibe sleep? ¿Qué estado muestra
`jobs` inmediatamente después? ¿Y después del próximo prompt
(cuando se hace reaping)?

> **Pregunta de reflexión**: ¿por qué `kill %1` envía la señal
> con `kill(-pgid, SIGTERM)` (PGID negativo) en vez de
> `kill(pid, SIGTERM)`? ¿Qué pasaría si el job fuera un pipeline
> de tres comandos y solo mataras al primero?

### Ejercicio 2: fg con pipeline stopped

Implementa el flujo completo de Ctrl+Z → fg con un pipeline:

**Pasos**:
1. Lanza `cat | sort` (pipeline de 2 comandos)
2. Crea un process group para el pipeline (setpgid en ambos hijos)
3. Pon el group en foreground con tcsetpgrp
4. Usa waitpid con WUNTRACED para detectar SIGTSTP
5. Cuando detectes stopped: add_job como JOB_STOPPED
6. Implementa `fg` que haga tcsetpgrp + kill(-pgid, SIGCONT) + waitpid

**Predicción antes de ejecutar**: al presionar Ctrl+Z durante
`cat | sort`, ¿cuántos procesos reciben SIGTSTP? ¿El `waitpid`
del padre retorna una o dos veces? ¿Qué ocurre si haces fg y
solo envías SIGCONT a uno de los dos procesos?

> **Pregunta de reflexión**: cuando fg reanuda un pipeline stopped,
> ¿en qué orden deben recibir SIGCONT los procesos? ¿Importa el
> orden, o `kill(-pgid, SIGCONT)` lo resuelve atómicamente? ¿Qué
> pasa si sort ya leyó todo de cat antes del Ctrl+Z?

### Ejercicio 3: notificación asíncrona de jobs

Implementa el ciclo completo de notificación de jobs background:

**Pasos**:
1. Configura el handler de SIGCHLD (solo poner un flag)
2. Implementa `reap_background_jobs()` con waitpid(WNOHANG) en loop
3. Implementa `notify_completed_jobs()` que imprima los jobs DONE
4. Integra ambas funciones en el REPL loop, antes de print_prompt
5. Prueba: `sleep 2 &`, espera 3 segundos, presiona Enter

**Predicción antes de ejecutar**: si lanzas `sleep 1 &` y
`sleep 2 &`, luego esperas 3 segundos y presionas Enter, ¿en qué
orden se notifican los jobs terminados? ¿El reap los recoge a
ambos en la misma iteración del while(waitpid)?

> **Pregunta de reflexión**: ¿qué problema habría si hicieras
> el waitpid **dentro** del signal handler de SIGCHLD en vez de
> en el REPL loop? Considera el caso donde el foreground job
> está corriendo y el handler de SIGCHLD lo recolecta antes que
> el waitpid del REPL. ¿Qué retorna el waitpid del REPL?
