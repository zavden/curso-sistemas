# Ejecución

## Índice
1. [De la estructura parseada a la ejecución](#1-de-la-estructura-parseada-a-la-ejecución)
2. [Ejecutar un comando simple](#2-ejecutar-un-comando-simple)
3. [Redirecciones con dup2](#3-redirecciones-con-dup2)
4. [Ejecutar un pipeline](#4-ejecutar-un-pipeline)
5. [Pipeline de N comandos (caso general)](#5-pipeline-de-n-comandos-caso-general)
6. [Background con &](#6-background-con-)
7. [Manejo de errores de exec](#7-manejo-de-errores-de-exec)
8. [Integración completa](#8-integración-completa)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. De la estructura parseada a la ejecución

En el tópico anterior construimos un parser que produce una
estructura `pipeline_t`. Ahora la ejecutamos:

```
pipeline_t:
  num_commands: 2
  background: false
  commands[0]: argv=["ls", "-la", NULL]
  commands[1]: argv=["grep", "test", NULL], redir_out="out.txt"

             ┌──────────┐ pipe ┌──────────┐
             │ fork+exec│─────▶│ fork+exec│──▶ out.txt
             │  ls -la  │      │grep test │
             └──────────┘      └──────────┘
```

### Lógica de decisión

```c
void execute_pipeline(pipeline_t *pl) {
    if (pl->num_commands == 1) {
        // Comando simple: fork + exec (con redirecciones)
        execute_simple(&pl->commands[0], pl->background);
    } else {
        // Pipeline: crear pipes entre cada par de comandos
        execute_pipe_chain(pl);
    }
}
```

---

## 2. Ejecutar un comando simple

El patrón fundamental: fork → configurar redirecciones → exec:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>

// Asume que command_t y pipeline_t están definidos (del tópico anterior)

int execute_simple(command_t *cmd, int background) {
    pid_t pid = fork();
    if (pid == -1) {
        perror("minish: fork");
        return -1;
    }

    if (pid == 0) {
        // --- Hijo ---

        // Aplicar redirecciones
        if (apply_redirections(cmd) == -1)
            _exit(1);

        // Ejecutar
        execvp(cmd->argv[0], cmd->argv);

        // Si llegamos aquí, execvp falló
        fprintf(stderr, "minish: %s: command not found\n", cmd->argv[0]);
        _exit(127);
    }

    // --- Padre ---
    if (!background) {
        int status;
        waitpid(pid, &status, 0);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
    } else {
        printf("[bg] %d\n", pid);
        return 0;
    }
}
```

### ¿Por qué execvp?

`execvp` busca el comando en `$PATH` y acepta un array `argv`:

```
exec variantes:
  execvp(file, argv)    ← la mejor para una shell
    v = vector (array de args)
    p = busca en PATH

  Otras:
  execv(path, argv)     ← ruta completa, no busca PATH
  execlp(file, arg0, ..., NULL) ← args como parámetros
  execve(path, argv, envp)      ← ruta + entorno explícito
```

### Flujo completo

```
Padre                              Hijo
──────                             ──────
fork() ─────────────────────┐
  │                         │
  │                         ▼
  │                    apply_redirections()
  │                         │
  │                    execvp("ls", ["ls","-la",NULL])
  │                         │
  │                    ← proceso reemplazado por ls
  │                    ← ls ejecuta y termina
  │                         │
waitpid(pid) ◀──────────────┘
  │
  ▼
retornar exit status
```

---

## 3. Redirecciones con dup2

Las redirecciones se aplican en el hijo, **antes** de `exec`:

```c
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int apply_redirections(command_t *cmd) {
    // Redirección de entrada: < archivo
    if (cmd->redir_in != NULL) {
        int fd = open(cmd->redir_in, O_RDONLY);
        if (fd == -1) {
            fprintf(stderr, "minish: %s: No such file or directory\n",
                    cmd->redir_in);
            return -1;
        }
        dup2(fd, STDIN_FILENO);   // fd → stdin
        close(fd);                // cerrar el fd original
    }

    // Redirección de salida: > o >> archivo
    if (cmd->redir_out != NULL) {
        int flags = O_WRONLY | O_CREAT;
        if (cmd->append_out)
            flags |= O_APPEND;     // >>
        else
            flags |= O_TRUNC;      // >

        int fd = open(cmd->redir_out, flags, 0644);
        if (fd == -1) {
            fprintf(stderr, "minish: %s: Permission denied\n",
                    cmd->redir_out);
            return -1;
        }
        dup2(fd, STDOUT_FILENO);  // fd → stdout
        close(fd);
    }

    // Redirección de stderr: 2> archivo (extensión)
    if (cmd->redir_err != NULL) {
        int fd = open(cmd->redir_err, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            fprintf(stderr, "minish: %s: Permission denied\n",
                    cmd->redir_err);
            return -1;
        }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    return 0;
}
```

### Diagrama de dup2

```
Antes de dup2:
  fd 0 (stdin)  → terminal
  fd 1 (stdout) → terminal
  fd 2 (stderr) → terminal
  fd 3           → open("output.txt")

dup2(3, 1):  // fd 3 → reemplaza fd 1
  fd 0 (stdin)  → terminal
  fd 1 (stdout) → output.txt    ← redirigido
  fd 2 (stderr) → terminal
  fd 3           → output.txt   ← se cierra después

close(3):
  fd 0 (stdin)  → terminal
  fd 1 (stdout) → output.txt    ← exec hereda esto
  fd 2 (stderr) → terminal
```

### ¿Por qué en el hijo y no en el padre?

Las redirecciones se aplican en el hijo porque:
1. No queremos redirigir el stdout del shell mismo
2. `fork()` copia los fds — el hijo tiene su propia tabla
3. `exec` hereda los fds (excepto los marcados con `O_CLOEXEC`)

---

## 4. Ejecutar un pipeline

Un pipeline de dos comandos necesita un pipe para conectarlos:

```
ls -la | grep test

  Padre crea pipe:  pipe_fd[0] (lectura), pipe_fd[1] (escritura)

  ┌────────────────┐           ┌────────────────┐
  │  Hijo 1 (ls)   │           │  Hijo 2 (grep) │
  │                │           │                │
  │  stdout → fd[1]│──pipe───▶│  stdin ← fd[0] │
  │  close(fd[0])  │           │  close(fd[1])  │
  │  execvp("ls")  │           │  execvp("grep")│
  └────────────────┘           └────────────────┘

  Padre cierra ambos extremos del pipe y espera a los dos hijos.
```

### Implementación para 2 comandos

```c
int execute_two_commands(command_t *cmd1, command_t *cmd2) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("minish: pipe");
        return -1;
    }

    // --- Hijo 1: ejecuta cmd1, stdout → pipe ---
    pid_t pid1 = fork();
    if (pid1 == 0) {
        close(pipe_fd[0]);              // no lee del pipe

        dup2(pipe_fd[1], STDOUT_FILENO); // stdout → pipe write end
        close(pipe_fd[1]);

        apply_redirections(cmd1);        // otras redirecciones (< file)
        execvp(cmd1->argv[0], cmd1->argv);
        fprintf(stderr, "minish: %s: command not found\n", cmd1->argv[0]);
        _exit(127);
    }

    // --- Hijo 2: ejecuta cmd2, stdin ← pipe ---
    pid_t pid2 = fork();
    if (pid2 == 0) {
        close(pipe_fd[1]);              // no escribe al pipe

        dup2(pipe_fd[0], STDIN_FILENO);  // stdin ← pipe read end
        close(pipe_fd[0]);

        apply_redirections(cmd2);        // otras redirecciones (> file)
        execvp(cmd2->argv[0], cmd2->argv);
        fprintf(stderr, "minish: %s: command not found\n", cmd2->argv[0]);
        _exit(127);
    }

    // --- Padre: cerrar pipe y esperar ---
    close(pipe_fd[0]);
    close(pipe_fd[1]);

    int status;
    waitpid(pid1, &status, 0);
    waitpid(pid2, &status, 0);

    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
```

### ¿Por qué el padre debe cerrar el pipe?

Si el padre no cierra `pipe_fd[1]`, el hijo 2 (grep) nunca ve
EOF en su stdin, porque el pipe tiene otro escritor abierto (el
padre). grep se quedaría esperando entrada indefinidamente:

```
Sin close en padre:
  pipe_fd[1] abierto en: hijo 1 (OK, escribe) + padre (problema)
  hijo 1 termina → cierra su copia de fd[1]
  pero pipe_fd[1] del padre sigue abierto
  → hijo 2 nunca ve EOF → cuelga para siempre

Con close en padre:
  padre cierra pipe_fd[0] y pipe_fd[1]
  hijo 1 termina → cierra su copia de fd[1]
  → no queda nadie con fd[1] abierto
  → hijo 2 ve EOF → termina normalmente
```

---

## 5. Pipeline de N comandos (caso general)

Para `cmd1 | cmd2 | cmd3 | ... | cmdN`, necesitamos N-1 pipes:

```
cmd1 | cmd2 | cmd3

  pipe[0]         pipe[1]
  ┌─────┐         ┌─────┐
  │fd[0]│         │fd[0]│
  │fd[1]│         │fd[1]│
  └─────┘         └─────┘

  cmd1:  stdout → pipe[0].fd[1]
  cmd2:  stdin  ← pipe[0].fd[0],  stdout → pipe[1].fd[1]
  cmd3:  stdin  ← pipe[1].fd[0]
```

### Implementación genérica

```c
int execute_pipeline(pipeline_t *pl) {
    int n = pl->num_commands;

    if (n == 1) {
        return execute_simple(&pl->commands[0], pl->background);
    }

    // Crear N-1 pipes
    int pipes[MAX_PIPELINE][2];
    for (int i = 0; i < n - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("minish: pipe");
            return -1;
        }
    }

    // Crear N hijos
    pid_t pids[MAX_PIPELINE];

    for (int i = 0; i < n; i++) {
        pids[i] = fork();
        if (pids[i] == -1) {
            perror("minish: fork");
            return -1;
        }

        if (pids[i] == 0) {
            // --- Hijo i ---

            // Si no es el primer comando: stdin ← pipe anterior
            if (i > 0) {
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }

            // Si no es el último comando: stdout → pipe siguiente
            if (i < n - 1) {
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // Cerrar TODOS los pipes en el hijo
            // (las copias que no se usan)
            for (int j = 0; j < n - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            // Aplicar redirecciones del usuario
            apply_redirections(&pl->commands[i]);

            // Ejecutar
            execvp(pl->commands[i].argv[0], pl->commands[i].argv);
            fprintf(stderr, "minish: %s: command not found\n",
                    pl->commands[i].argv[0]);
            _exit(127);
        }
    }

    // --- Padre: cerrar todos los pipes ---
    for (int i = 0; i < n - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // Esperar a todos los hijos
    int last_status = 0;
    if (!pl->background) {
        for (int i = 0; i < n; i++) {
            int status;
            waitpid(pids[i], &status, 0);
            if (i == n - 1) {
                last_status = WIFEXITED(status) ? WEXITSTATUS(status) : 1;
            }
        }
    } else {
        printf("[bg] pipeline: pids");
        for (int i = 0; i < n; i++)
            printf(" %d", pids[i]);
        printf("\n");
    }

    return last_status;
}
```

### ¿Por qué cerrar TODOS los pipes en cada hijo?

Cada hijo hereda **todos** los fds del pipe array. Si el hijo 1
no cierra `pipes[1][0]` y `pipes[1][1]`, mantiene abiertos los
extremos del pipe entre cmd2 y cmd3, lo que impide que cmd3 vea
EOF:

```
Hijo 0 debe cerrar:
  pipes[0][0]  (no lo lee — su stdin viene del terminal)
  pipes[1][0]  (no le pertenece)
  pipes[1][1]  (no le pertenece)
  ← mantiene pipes[0][1] vía dup2 en stdout

Hijo 1 debe cerrar:
  pipes[0][1]  (no escribe ahí — ya hizo dup2 a stdin)
  pipes[1][0]  (no lo lee — su stdin viene de pipes[0])
  ← mantiene pipes[0][0] vía dup2 en stdin
  ← mantiene pipes[1][1] vía dup2 en stdout

Hijo 2 debe cerrar:
  pipes[0][0]  (no le pertenece)
  pipes[0][1]  (no le pertenece)
  pipes[1][1]  (no escribe ahí)
  ← mantiene pipes[1][0] vía dup2 en stdin
```

El patrón más simple: en cada hijo, **después de dup2**, cerrar
todos los fds del array de pipes. Los fds dup2'd a 0/1 no se
afectan porque son fds diferentes.

### Diagrama del pipeline de 3

```
        pipe[0]           pipe[1]
        r    w            r    w
Padre: [fd0][fd1]       [fd2][fd3]     ← crea pipes
  │
  ├── fork → Hijo 0:
  │           dup2(fd1, stdout)         ← stdout al pipe[0]
  │           close(fd0,fd1,fd2,fd3)    ← cerrar todos
  │           execvp(cmd1)
  │
  ├── fork → Hijo 1:
  │           dup2(fd0, stdin)          ← stdin del pipe[0]
  │           dup2(fd3, stdout)         ← stdout al pipe[1]
  │           close(fd0,fd1,fd2,fd3)
  │           execvp(cmd2)
  │
  └── fork → Hijo 2:
              dup2(fd2, stdin)          ← stdin del pipe[1]
              close(fd0,fd1,fd2,fd3)
              execvp(cmd3)

Padre: close(fd0,fd1,fd2,fd3)
       waitpid × 3
```

---

## 6. Background con &

Cuando el usuario termina un comando con `&`, la shell no debe
esperar a que termine:

```c
if (!pl->background) {
    // Foreground: esperar
    for (int i = 0; i < n; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }
} else {
    // Background: no esperar, mostrar PID
    printf("[1] %d\n", pids[n - 1]);
    // Los procesos zombie se recolectan con SIGCHLD
}
```

### Recolección de procesos background

Sin recolección, los procesos background terminados quedan como
zombies. Necesitamos un handler de SIGCHLD:

```c
#include <signal.h>
#include <sys/wait.h>
#include <stdio.h>

void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        // Podemos mostrar un mensaje, pero solo funciones
        // async-signal-safe. write() es safe, printf() no.
        // Para simplificar, lo dejamos silencioso o usamos
        // un flag que el REPL revisa.
    }
}

// En main, antes del REPL loop:
struct sigaction sa = {
    .sa_handler = sigchld_handler,
    .sa_flags   = SA_RESTART | SA_NOCLDSTOP
};
sigemptyset(&sa.sa_mask);
sigaction(SIGCHLD, &sa, NULL);
```

### Notificación de terminación

Las shells reales notifican cuando un proceso background termina.
Para hacerlo de forma async-signal-safe:

```c
// Flag global que el handler activa
static volatile sig_atomic_t child_exited = 0;
static pid_t last_exited_pid = 0;

void sigchld_handler(int sig) {
    (void)sig;
    int status;
    pid_t pid;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        last_exited_pid = pid;
        child_exited = 1;
    }
}

// En el REPL loop, antes del prompt:
if (child_exited) {
    child_exited = 0;
    printf("\n[Done] %d\n", last_exited_pid);
}
```

---

## 7. Manejo de errores de exec

### Comando no encontrado

Cuando `execvp` falla, el código **después** de execvp se ejecuta
en el hijo. Es crucial que el hijo llame a `_exit()`, no a
`return` (que continuaría el REPL loop en el hijo):

```c
if (pid == 0) {
    execvp(cmd->argv[0], cmd->argv);

    // Solo llegamos aquí si execvp falló
    switch (errno) {
    case ENOENT:
        fprintf(stderr, "minish: %s: command not found\n",
                cmd->argv[0]);
        _exit(127);  // convención de bash
    case EACCES:
        fprintf(stderr, "minish: %s: Permission denied\n",
                cmd->argv[0]);
        _exit(126);  // convención de bash
    default:
        fprintf(stderr, "minish: %s: %s\n",
                cmd->argv[0], strerror(errno));
        _exit(1);
    }
}
```

### Códigos de salida convencionales

| Código | Significado (convención bash) |
|--------|-------------------------------|
| 0 | Éxito |
| 1-125 | Error del programa |
| 126 | Comando encontrado pero no ejecutable |
| 127 | Comando no encontrado |
| 128+N | Terminado por señal N |

### Variable `$?`

Nuestra shell puede guardar el último exit status:

```c
static int last_exit_status = 0;

// Después de waitpid:
if (WIFEXITED(status))
    last_exit_status = WEXITSTATUS(status);
else if (WIFSIGNALED(status))
    last_exit_status = 128 + WTERMSIG(status);
```

---

## 8. Integración completa

Juntando parser + ejecución en un REPL funcional:

```c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

// --- Incluir aquí las definiciones de ---
// token_list_t, command_t, pipeline_t
// tokenize(), parse_tokens()
// apply_redirections(), execute_pipeline()
// (de este tópico y el anterior)

static int last_exit_status = 0;

void sigchld_handler(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0)
        ;
}

void print_prompt(void) {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd))) {
        char *home = getenv("HOME");
        if (home && strncmp(cwd, home, strlen(home)) == 0)
            printf("minish:~%s$ ", cwd + strlen(home));
        else
            printf("minish:%s$ ", cwd);
    } else {
        printf("minish$ ");
    }
    fflush(stdout);
}

int main(void) {
    // Instalar handler para SIGCHLD
    struct sigaction sa = {
        .sa_handler = sigchld_handler,
        .sa_flags   = SA_RESTART | SA_NOCLDSTOP
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);

    char *line = NULL;
    size_t bufsize = 0;

    for (;;) {
        print_prompt();

        ssize_t n = getline(&line, &bufsize, stdin);
        if (n == -1) break;  // EOF

        // Quitar newline
        if (n > 0 && line[n - 1] == '\n')
            line[n - 1] = '\0';

        // Ignorar vacío
        if (line[0] == '\0') continue;

        // Tokenizar
        token_list_t tl;
        int ntokens = tokenize(line, &tl);
        if (ntokens <= 0) continue;

        // Parsear
        pipeline_t pl;
        if (parse_tokens(tl.tokens, tl.count, &pl) == -1)
            continue;
        if (pl.commands[0].argc == 0) continue;

        // Built-ins (mínimo para funcionar)
        if (strcmp(pl.commands[0].argv[0], "exit") == 0) {
            break;
        }
        if (strcmp(pl.commands[0].argv[0], "cd") == 0) {
            const char *dir = pl.commands[0].argv[1];
            if (dir == NULL) dir = getenv("HOME");
            if (chdir(dir) == -1)
                fprintf(stderr, "minish: cd: %s: %s\n",
                        dir, strerror(errno));
            continue;
        }

        // Ejecutar
        last_exit_status = execute_pipeline(&pl);
    }

    free(line);
    printf("\nexit\n");
    return last_exit_status;
}
```

```bash
gcc -o minish minish.c && ./minish

minish:~$ ls -la
minish:~$ echo hello | tr a-z A-Z
HELLO
minish:~$ cat /etc/hostname > /tmp/test.txt
minish:~$ wc -l < /etc/passwd
minish:~$ ls -la | grep test | wc -l
minish:~$ sleep 10 &
[1] 12345
minish:~$ exit
```

---

## 9. Errores comunes

### Error 1: no cerrar pipes en el padre

```c
// MAL: padre mantiene pipes abiertos
for (int i = 0; i < n; i++) {
    pids[i] = fork();
    if (pids[i] == 0) {
        // hijo configura y execvp
    }
}
// Olvidó cerrar pipes → hijos nunca ven EOF → cuelgan
waitpid(...);  // cuelga para siempre
```

**Solución**: cerrar todos los pipes en el padre después de todos
los forks:

```c
for (int i = 0; i < n - 1; i++) {
    close(pipes[i][0]);
    close(pipes[i][1]);
}
```

### Error 2: _exit vs exit en el hijo tras exec fallido

```c
// MAL: usar exit() en el hijo
if (pid == 0) {
    execvp(cmd->argv[0], cmd->argv);
    perror("execvp");
    exit(1);  // ¡PELIGROSO! Ejecuta atexit handlers del padre,
              // flushea buffers de stdio del padre duplicados
}
```

`exit()` ejecuta `atexit` handlers y flushea buffers de stdio. En
un hijo post-fork, eso puede duplicar output o corromper archivos.

**Solución**: siempre usar `_exit()` después de un exec fallido:

```c
if (pid == 0) {
    execvp(cmd->argv[0], cmd->argv);
    perror("execvp");
    _exit(127);  // termina limpiamente sin side effects
}
```

### Error 3: no cerrar todos los pipes en cada hijo

```c
// MAL: hijo 0 solo cierra su pipe, olvida los demás
if (i == 0) {
    dup2(pipes[0][1], STDOUT_FILENO);
    close(pipes[0][0]);
    close(pipes[0][1]);
    // pipes[1][0] y pipes[1][1] siguen abiertos en este hijo
    // → hijo 2 nunca ve EOF porque este hijo tiene pipe[1][1] abierto
}
```

**Solución**: cerrar TODOS los pipes en cada hijo:

```c
for (int j = 0; j < n - 1; j++) {
    close(pipes[j][0]);
    close(pipes[j][1]);
}
```

### Error 4: hacer dup2 después de close

```c
// MAL: cerrar antes de dup2
close(pipes[0][1]);
dup2(pipes[0][1], STDOUT_FILENO);  // fd ya cerrado → falla
```

**Solución**: dup2 primero, close después:

```c
dup2(pipes[0][1], STDOUT_FILENO);  // duplicar primero
close(pipes[0][1]);                 // ahora se puede cerrar
```

### Error 5: el hijo continúa el REPL loop

```c
// MAL: si exec falla y no se llama _exit
if (pid == 0) {
    execvp(cmd->argv[0], cmd->argv);
    // execvp falló pero no hacemos _exit
    // ¡El hijo continúa el for(;;) del REPL!
    // Ahora hay DOS shells leyendo del mismo terminal
}
```

Esto crea un caos donde dos procesos compiten por leer stdin.

**Solución**: siempre `_exit()` después de exec fallido (como se
mostró arriba).

---

## 10. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│              Ejecución de Comandos — Mini-shell              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  COMANDO SIMPLE:                                             │
│    fork()                                                    │
│      hijo:  apply_redirections() → execvp() → _exit(127)     │
│      padre: waitpid() (foreground) o print PID (background)  │
│                                                              │
│  REDIRECCIONES (en el hijo, antes de exec):                  │
│    <  file:  open(O_RDONLY) → dup2(fd, STDIN)  → close(fd)   │
│    >  file:  open(O_WRONLY|O_CREAT|O_TRUNC) → dup2(fd, STDOUT)│
│    >> file:  open(O_WRONLY|O_CREAT|O_APPEND)→ dup2(fd, STDOUT)│
│    2> file:  open(...) → dup2(fd, STDERR) → close(fd)        │
│                                                              │
│  PIPELINE (N comandos):                                      │
│    1. Crear N-1 pipes                                        │
│    2. fork N hijos:                                          │
│       hijo i>0:    dup2(pipes[i-1][0], STDIN)                │
│       hijo i<N-1:  dup2(pipes[i][1], STDOUT)                 │
│       cada hijo:   cerrar TODOS los pipes, luego execvp      │
│    3. Padre: cerrar TODOS los pipes                          │
│    4. Padre: waitpid × N                                     │
│                                                              │
│  BACKGROUND (&):                                             │
│    No hacer waitpid → imprimir PID                           │
│    SIGCHLD handler con waitpid(WNOHANG) para recolectar      │
│                                                              │
│  EXIT CODES:                                                 │
│    0     = éxito                                             │
│    127   = command not found (ENOENT)                        │
│    126   = permission denied (EACCES)                        │
│    128+N = killed by signal N                                │
│                                                              │
│  REGLAS DE ORO:                                              │
│    • dup2 ANTES de close                                     │
│    • _exit() (no exit()) después de exec fallido              │
│    • Cerrar TODOS los pipes en CADA hijo y en el padre       │
│    • execvp para buscar en PATH                              │
│    • Redirecciones se aplican en el hijo, no en el padre     │
└──────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: shell con comandos simples y redirecciones

Implementa una mini-shell que ejecute:

1. Comandos simples: `ls`, `echo hello`, `cat /etc/hostname`.
2. Redirección de salida: `echo hello > file.txt`
3. Redirección de entrada: `wc -l < /etc/passwd`
4. Redirección append: `echo line2 >> file.txt`
5. Combinadas: `sort < unsorted.txt > sorted.txt`

Verificar:
```bash
minish$ echo "hello world" > /tmp/test.txt
minish$ cat < /tmp/test.txt
hello world
minish$ echo "second line" >> /tmp/test.txt
minish$ wc -l < /tmp/test.txt
2
```

No implementar pipes todavía — solo redirecciones.

**Pregunta de reflexión**: ¿qué pasa si el archivo de redirección
de salida ya existe y tiene datos? ¿Cuál es la diferencia entre
cómo lo manejan `>` (truncar a 0) y bash con `set -o noclobber`?

---

### Ejercicio 2: pipeline completo

Extiende la shell del ejercicio 1 con pipelines:

1. Dos comandos: `ls -la | grep test`
2. Tres comandos: `cat /etc/passwd | grep root | wc -l`
3. Pipes con redirecciones: `cat < input.txt | sort | head -5 > output.txt`
4. Verificar que los pipes se cierran correctamente:
   - `yes | head -3` debe imprimir 3 líneas y terminar (no colgar).
   - `cat /dev/urandom | head -c 100 > /tmp/rand.bin` debe terminar.

El test de `yes | head` es crítico: si los pipes no se cierran
bien, `yes` nunca recibe SIGPIPE y sigue escribiendo para siempre.

**Pregunta de reflexión**: en bash, el exit status de un pipeline
es el del **último** comando. Así que `false | true` retorna 0.
¿Es esto útil o peligroso? Bash tiene `set -o pipefail` que
retorna el exit status del primer comando que falla. ¿Cuándo
preferirías cada comportamiento?

---

### Ejercicio 3: background y status

Extiende con soporte de background:

1. `sleep 5 &` debe retornar inmediatamente con el PID.
2. `sleep 1 & sleep 2 & sleep 3 &` debe lanzar los tres en
   paralelo.
3. Cuando un proceso background termina, mostrar un mensaje antes
   del siguiente prompt:
   ```
   minish$ sleep 2 &
   [1] 12345
   minish$ echo hello
   hello
   [Done] 12345
   minish$
   ```
4. Implementar un comando built-in `jobs` que liste los procesos
   background activos con su PID y comando.

Mantener una lista de jobs en un array:
```c
typedef struct {
    pid_t pid;
    char  cmdline[256];
    int   running;  // 1 = running, 0 = finished
} job_t;
```

**Pregunta de reflexión**: cuando un proceso background escribe a
stdout, su salida se mezcla con el prompt de la shell y la entrada
del usuario. ¿Cómo manejan esto las shells reales? (Pista: piensa
en terminal process groups y `tcsetpgrp`.)
