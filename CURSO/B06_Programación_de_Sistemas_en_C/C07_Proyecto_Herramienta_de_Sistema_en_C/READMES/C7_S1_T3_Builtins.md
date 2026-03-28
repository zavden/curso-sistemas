# Built-ins

## Índice
1. [¿Qué es un built-in?](#1-qué-es-un-built-in)
2. [cd](#2-cd)
3. [exit](#3-exit)
4. [export y unset](#4-export-y-unset)
5. [echo (versión built-in)](#5-echo-versión-built-in)
6. [pwd](#6-pwd)
7. [Dispatch de built-ins](#7-dispatch-de-built-ins)
8. [Señales: Ctrl+C no mata la shell](#8-señales-ctrlc-no-mata-la-shell)
9. [Señales: Ctrl+Z y SIGTSTP](#9-señales-ctrlz-y-sigtstp)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Qué es un built-in?

Un built-in es un comando que la shell ejecuta **en su propio
proceso**, sin hacer fork+exec. Esto es necesario cuando el
comando debe **modificar el estado de la shell misma**:

```
Comando externo (ls, grep):         Built-in (cd, exit):
  shell: fork()                       shell: ejecuta directamente
    hijo: execvp("ls")                  chdir("/tmp")
    padre: waitpid()                    (modifica el CWD de la shell)
  shell continúa                      shell continúa
```

### ¿Por qué cd no puede ser externo?

```c
// Si cd fuera externo:
fork();
// hijo:
chdir("/tmp");    // cambia el CWD del HIJO
execvp("cd", ...); // no existe /usr/bin/cd (o si existe, no ayuda)
_exit(0);
// padre:
waitpid(...);
// El CWD del padre (la shell) NO cambió
// → cd no funcionaría
```

`chdir()` solo afecta al proceso que lo llama. Como fork crea un
proceso separado, un `cd` externo cambiaría el directorio del hijo
(que luego muere) sin afectar la shell.

### Built-ins típicos de una shell

| Built-in | Por qué debe ser built-in |
|----------|--------------------------|
| `cd` | Modifica el CWD del proceso shell |
| `exit` | Termina el proceso shell |
| `export` | Modifica el entorno del proceso shell |
| `unset` | Elimina variables del entorno |
| `pwd` | Podría ser externo, pero es más rápido como built-in |
| `echo` | Podría ser externo, pero shells lo implementan por rendimiento |
| `history` | Accede a datos internos de la shell |
| `jobs`/`fg`/`bg` | Accede a la tabla de jobs de la shell |
| `alias` | Modifica la tabla de alias interna |
| `source`/`.` | Ejecuta script en el contexto de la shell actual |

---

## 2. cd

### Implementación básica

```c
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int builtin_cd(char **argv) {
    const char *dir;

    if (argv[1] == NULL) {
        // cd sin argumentos: ir a $HOME
        dir = getenv("HOME");
        if (dir == NULL) {
            fprintf(stderr, "minish: cd: HOME not set\n");
            return 1;
        }
    } else if (strcmp(argv[1], "-") == 0) {
        // cd -: ir al directorio anterior ($OLDPWD)
        dir = getenv("OLDPWD");
        if (dir == NULL) {
            fprintf(stderr, "minish: cd: OLDPWD not set\n");
            return 1;
        }
        printf("%s\n", dir);  // bash imprime el directorio al usar cd -
    } else if (strcmp(argv[1], "~") == 0) {
        dir = getenv("HOME");
        if (dir == NULL) {
            fprintf(stderr, "minish: cd: HOME not set\n");
            return 1;
        }
    } else {
        dir = argv[1];
    }

    // Guardar directorio actual como OLDPWD
    char oldpwd[4096];
    if (getcwd(oldpwd, sizeof(oldpwd)) != NULL) {
        setenv("OLDPWD", oldpwd, 1);
    }

    // Cambiar directorio
    if (chdir(dir) == -1) {
        fprintf(stderr, "minish: cd: %s: %s\n", dir, strerror(errno));
        return 1;
    }

    // Actualizar PWD
    char newpwd[4096];
    if (getcwd(newpwd, sizeof(newpwd)) != NULL) {
        setenv("PWD", newpwd, 1);
    }

    return 0;
}
```

### Expansión de tilde (~)

En bash, `cd ~/Documents` expande `~` a `$HOME`. Nuestro
tokenizador no hace esta expansión, pero podemos agregarla en
el built-in o como un paso de expansión previo al parse:

```c
// Expansión simple de ~ al inicio de un argumento
char *expand_tilde(const char *arg) {
    if (arg[0] != '~')
        return (char *)arg;  // sin cambio

    const char *home = getenv("HOME");
    if (home == NULL) return (char *)arg;

    if (arg[1] == '\0') {
        // "~" solo → $HOME
        return strdup(home);
    }
    if (arg[1] == '/') {
        // "~/algo" → $HOME/algo
        static char buf[4096];
        snprintf(buf, sizeof(buf), "%s%s", home, arg + 1);
        return buf;
    }

    return (char *)arg;  // ~user: no implementado
}
```

---

## 3. exit

### Implementación

```c
#include <stdlib.h>

int builtin_exit(char **argv) {
    int status = 0;

    if (argv[1] != NULL) {
        // exit con código: exit 42
        char *endptr;
        long val = strtol(argv[1], &endptr, 10);
        if (*endptr != '\0') {
            fprintf(stderr, "minish: exit: %s: numeric argument required\n",
                    argv[1]);
            status = 2;  // bash usa 2 para argumento inválido
        } else {
            status = (int)(val & 0xFF);  // truncar a 8 bits
        }
    }

    // Aquí podrías: guardar historial, cerrar fds, etc.
    printf("exit\n");
    exit(status);
}
```

### ¿Por qué exit es un built-in?

`exit()` termina el proceso actual. Si se ejecutara en un hijo
(fork+exec), mataría al hijo, no a la shell. Además, la shell
necesita ejecutar cleanup antes de terminar (guardar historial,
matar procesos background, etc.).

### Graceful shutdown

```c
int builtin_exit(char **argv) {
    // Verificar si hay jobs en background
    int bg_count = count_background_jobs();
    if (bg_count > 0) {
        fprintf(stderr, "minish: there are %d running jobs\n", bg_count);
        // Bash: primera vez advierte, segunda vez sale
        // Simplificado: advertir y salir de todos modos
    }

    // Guardar historial (si implementamos)
    // save_history();

    int status = argv[1] ? atoi(argv[1]) : last_exit_status;
    exit(status);
}
```

---

## 4. export y unset

### export — crear/modificar variables de entorno

```c
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

int builtin_export(char **argv) {
    if (argv[1] == NULL) {
        // Sin argumentos: imprimir todas las variables
        extern char **environ;
        for (char **env = environ; *env != NULL; env++) {
            printf("export %s\n", *env);
        }
        return 0;
    }

    // export VAR=valor o export VAR
    for (int i = 1; argv[i] != NULL; i++) {
        char *eq = strchr(argv[i], '=');
        if (eq != NULL) {
            // VAR=valor: setenv
            *eq = '\0';
            setenv(argv[i], eq + 1, 1);  // overwrite=1
            *eq = '=';  // restaurar por si acaso
        } else {
            // VAR sin valor: marcar como exportada
            // (en nuestra mini-shell, si no tiene valor, ignorar)
            char *val = getenv(argv[i]);
            if (val != NULL) {
                setenv(argv[i], val, 1);
            } else {
                setenv(argv[i], "", 1);
            }
        }
    }

    return 0;
}
```

### unset — eliminar variable de entorno

```c
int builtin_unset(char **argv) {
    if (argv[1] == NULL) {
        fprintf(stderr, "minish: unset: not enough arguments\n");
        return 1;
    }

    for (int i = 1; argv[i] != NULL; i++) {
        unsetenv(argv[i]);
    }
    return 0;
}
```

### ¿Por qué deben ser built-ins?

```c
// Si export fuera externo:
fork();
// hijo: setenv("PATH", "/new/path", 1) → solo afecta al hijo
// padre: $PATH no cambió
```

`setenv()` modifica el entorno del proceso actual. Un proceso hijo
tiene una **copia** del entorno, no una referencia compartida. Los
cambios del hijo mueren con él.

---

## 5. echo (versión built-in)

`echo` podría ser externo (`/usr/bin/echo`), pero las shells lo
implementan como built-in para evitar el overhead de fork+exec en
algo tan frecuente:

```c
int builtin_echo(char **argv) {
    int newline = 1;    // imprimir \n al final
    int start = 1;

    // Manejar flags
    if (argv[1] != NULL && strcmp(argv[1], "-n") == 0) {
        newline = 0;
        start = 2;
    }

    for (int i = start; argv[i] != NULL; i++) {
        if (i > start) printf(" ");
        printf("%s", argv[i]);
    }

    if (newline) printf("\n");
    fflush(stdout);

    return 0;
}
```

---

## 6. pwd

```c
#include <unistd.h>
#include <stdio.h>

int builtin_pwd(char **argv) {
    (void)argv;
    char cwd[4096];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("minish: pwd");
        return 1;
    }
    printf("%s\n", cwd);
    return 0;
}
```

`pwd` también existe como externo (`/usr/bin/pwd`), pero como
built-in evita el fork y además puede usar la variable `$PWD`
interna para manejar symlinks de forma consistente.

---

## 7. Dispatch de built-ins

Necesitamos una función que determine si un comando es built-in y
lo ejecute directamente, sin fork:

```c
#include <string.h>
#include <stdbool.h>

// Tabla de built-ins
typedef int (*builtin_fn)(char **argv);

typedef struct {
    const char  *name;
    builtin_fn   func;
} builtin_entry_t;

static const builtin_entry_t builtins[] = {
    { "cd",      builtin_cd },
    { "exit",    builtin_exit },
    { "export",  builtin_export },
    { "unset",   builtin_unset },
    { "echo",    builtin_echo },
    { "pwd",     builtin_pwd },
    { NULL,      NULL }
};

// Buscar si un comando es built-in
builtin_fn find_builtin(const char *name) {
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (strcmp(name, builtins[i].name) == 0)
            return builtins[i].func;
    }
    return NULL;  // no es built-in
}

// Integración en el executor
int execute_pipeline(pipeline_t *pl) {
    // Los built-ins solo se ejecutan para comandos simples
    // (no en pipelines — en pipelines se ejecutan en el hijo)
    if (pl->num_commands == 1 && !pl->background) {
        builtin_fn fn = find_builtin(pl->commands[0].argv[0]);
        if (fn != NULL) {
            return fn(pl->commands[0].argv);
        }
    }

    // Ejecutar como comando externo o pipeline
    return execute_external(pl);
}
```

### Built-ins en pipelines

¿Qué pasa con `echo hello | cat`? El `echo` necesita ejecutarse
en un subproceso (para conectar su stdout al pipe), así que los
built-ins **dentro de pipelines** se ejecutan en un hijo:

```c
// En el código de pipeline, para cada hijo:
if (pids[i] == 0) {
    // Configurar pipes y redirecciones...

    // Intentar built-in en el hijo
    builtin_fn fn = find_builtin(pl->commands[i].argv[0]);
    if (fn != NULL) {
        int status = fn(pl->commands[i].argv);
        _exit(status);
    }

    // Si no es built-in, exec
    execvp(pl->commands[i].argv[0], pl->commands[i].argv);
    _exit(127);
}
```

> **Nota**: `cd` en un pipeline (`cd /tmp | echo done`) no tiene
> efecto en la shell padre porque se ejecuta en un hijo. Bash
> se comporta igual — es consistente con la semántica de fork.

---

## 8. Señales: Ctrl+C no mata la shell

### El problema

Sin manejo de señales, Ctrl+C envía `SIGINT` al **foreground
process group** completo, que incluye la shell y sus hijos:

```
Ctrl+C → SIGINT → todo el foreground process group:
  ├── shell (PID 1000)      ← muere
  └── hijo (PID 1001, "sleep 30")  ← muere
```

Queremos que Ctrl+C mate al hijo pero **no** a la shell:

```
Ctrl+C → SIGINT → foreground process group:
  ├── shell (PID 1000)      ← ignora SIGINT (sigue viva)
  └── hijo (PID 1001)       ← muere
```

### Solución: ignorar SIGINT en la shell

```c
#include <signal.h>

// En main(), antes del REPL loop:
void setup_shell_signals(void) {
    // La shell ignora SIGINT (Ctrl+C)
    signal(SIGINT, SIG_IGN);

    // La shell ignora SIGQUIT (Ctrl+\)
    signal(SIGQUIT, SIG_IGN);

    // La shell ignora SIGTSTP (Ctrl+Z)
    signal(SIGTSTP, SIG_IGN);

    // SIGCHLD para recolectar hijos background
    struct sigaction sa = {
        .sa_handler = sigchld_handler,
        .sa_flags   = SA_RESTART | SA_NOCLDSTOP
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGCHLD, &sa, NULL);
}
```

### Restaurar señales en los hijos

Los hijos deben responder normalmente a SIGINT (para que Ctrl+C
los mate). Las señales ignoradas con `SIG_IGN` se **heredan** por
fork y **persisten** por exec. Debemos restaurarlas:

```c
// En el hijo, antes de exec:
void restore_child_signals(void) {
    signal(SIGINT, SIG_DFL);    // restaurar default
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
}

// En el código de fork:
if (pid == 0) {
    restore_child_signals();
    // ... redirecciones ...
    execvp(cmd->argv[0], cmd->argv);
    _exit(127);
}
```

### Reimprimir el prompt después de Ctrl+C

Cuando el usuario presiona Ctrl+C sin un comando en foreground
(en el prompt vacío), queremos un comportamiento como bash:
interrumpir la línea actual y mostrar un prompt nuevo:

```c
// Handler para SIGINT (alternativa a SIG_IGN):
void shell_sigint_handler(int sig) {
    (void)sig;
    write(STDOUT_FILENO, "\n", 1);
    // Si usamos readline: rl_on_new_line(); rl_replace_line("", 0);
    // Con getline: simplemente interrumpe el read, REPL loop
    // imprime nuevo prompt
}

// Instalar:
struct sigaction sa = {
    .sa_handler = shell_sigint_handler,
    .sa_flags   = 0  // NO SA_RESTART, queremos que getline sea interrumpido
};
sigemptyset(&sa.sa_mask);
sigaction(SIGINT, &sa, NULL);
```

Sin `SA_RESTART`, SIGINT interrumpe `getline()` → retorna -1 con
`errno == EINTR` → el REPL loop imprime un nuevo prompt:

```c
for (;;) {
    print_prompt();
    ssize_t n = getline(&line, &bufsize, stdin);
    if (n == -1) {
        if (errno == EINTR) {
            clearerr(stdin);  // limpiar el estado de error de stdin
            continue;         // nuevo prompt
        }
        break;  // EOF real (Ctrl+D)
    }
    // ... procesar línea ...
}
```

---

## 9. Señales: Ctrl+Z y SIGTSTP

Ctrl+Z envía `SIGTSTP` al foreground process group. El
comportamiento esperado:

```
minish$ sleep 30    ← ejecutándose
^Z                   ← Ctrl+Z
[1]+ Stopped  sleep 30
minish$             ← shell retoma el control
```

### Implementación básica

```c
// La shell ignora SIGTSTP
signal(SIGTSTP, SIG_IGN);

// El hijo restaura SIGTSTP
// (ya lo hacemos en restore_child_signals)

// Después de waitpid, verificar si el hijo fue detenido:
int status;
pid_t pid = waitpid(fg_pid, &status, WUNTRACED);
// WUNTRACED: waitpid retorna también si el hijo fue detenido

if (WIFSTOPPED(status)) {
    // El hijo fue detenido (Ctrl+Z)
    printf("\n[%d] Stopped\n", pid);
    // Añadir a la lista de jobs como stopped
    add_job(pid, cmdline, JOB_STOPPED);
} else if (WIFEXITED(status)) {
    last_exit_status = WEXITSTATUS(status);
} else if (WIFSIGNALED(status)) {
    last_exit_status = 128 + WTERMSIG(status);
}
```

### Process groups (contexto)

Para un manejo correcto de señales, cada pipeline debería ejecutarse
en su propio **process group**. Esto permite que Ctrl+C afecte solo
al pipeline actual, no a la shell:

```c
// En el hijo (antes de exec):
setpgid(0, 0);  // crear nuevo process group con PID del hijo como PGID

// En el padre (para foreground):
// Poner el process group del hijo como foreground del terminal
tcsetpgrp(STDIN_FILENO, child_pid);

// Después de waitpid:
// Devolver el control del terminal a la shell
tcsetpgrp(STDIN_FILENO, getpgrp());
```

> **Nota**: el manejo completo de process groups es bastante
> complejo. Para nuestra mini-shell, ignorar SIGINT/SIGTSTP en
> la shell y restaurarlos en los hijos es suficiente para un
> comportamiento razonable.

---

## 10. Errores comunes

### Error 1: ejecutar cd con fork

```c
// MAL: cd en un hijo no tiene efecto
pid_t pid = fork();
if (pid == 0) {
    chdir(argv[1]);  // cambia CWD del hijo
    _exit(0);
}
waitpid(pid, NULL, 0);
// El CWD de la shell NO cambió
```

**Solución**: ejecutar cd directamente en el proceso de la shell.

### Error 2: no restaurar señales en los hijos

```c
// MAL: el hijo hereda SIG_IGN de la shell
signal(SIGINT, SIG_IGN);  // en la shell
// ...
if (fork() == 0) {
    // SIGINT sigue ignorado en el hijo
    // Ctrl+C no mata al hijo → el usuario no puede cancelar
    execvp(cmd->argv[0], cmd->argv);
}
```

**Solución**: restaurar SIG_DFL antes de exec:

```c
if (fork() == 0) {
    signal(SIGINT, SIG_DFL);
    signal(SIGQUIT, SIG_DFL);
    signal(SIGTSTP, SIG_DFL);
    execvp(...);
}
```

### Error 3: no limpiar stdin después de SIGINT

```c
// MAL: después de Ctrl+C, getline puede seguir en estado de error
ssize_t n = getline(&line, &bufsize, stdin);
if (n == -1) {
    // Sin clearerr, stdin queda en estado de error
    // Las siguientes llamadas a getline también fallan
    break;  // la shell sale por error, no por EOF
}
```

**Solución**: distinguir EINTR de EOF y limpiar:

```c
if (n == -1) {
    if (errno == EINTR) {
        clearerr(stdin);
        continue;  // nuevo prompt
    }
    break;  // EOF real
}
```

### Error 4: exit en pipeline no sale de la shell

```c
// El usuario espera que esto salga de la shell:
minish$ exit | cat
// Pero exit se ejecutó en un hijo (por el pipeline)
// → solo el hijo terminó, la shell sigue viva
```

Esto no es realmente un bug — bash se comporta igual. Pero es
una fuente de confusión. Los built-ins que modifican estado de
la shell (cd, exit, export) solo tienen efecto como comando
**simple**, no dentro de un pipeline.

### Error 5: no actualizar PWD después de cd

```c
// MAL: cd cambia el directorio pero PWD está desactualizado
chdir("/tmp");
// $PWD sigue mostrando el directorio anterior
// Programas que usan $PWD en vez de getcwd() se confunden
```

**Solución**: actualizar PWD y OLDPWD:

```c
char old[4096];
getcwd(old, sizeof(old));
setenv("OLDPWD", old, 1);

chdir(dir);

char new[4096];
getcwd(new, sizeof(new));
setenv("PWD", new, 1);
```

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│            Built-ins y Señales — Mini-shell                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ¿POR QUÉ BUILT-IN?                                         │
│    cd, exit, export, unset: modifican estado de la shell     │
│    echo, pwd: eficiencia (evitar fork+exec)                  │
│    jobs, fg, bg: acceden a datos internos de la shell        │
│                                                              │
│  DISPATCH:                                                   │
│    1. Verificar si argv[0] es built-in                       │
│    2. Si sí → ejecutar en la shell (sin fork)                │
│    3. Si no → fork + exec como comando externo               │
│    4. En pipelines → built-ins se ejecutan en hijo           │
│                                                              │
│  cd:                                                         │
│    cd          → chdir($HOME)                                │
│    cd dir      → chdir(dir)                                  │
│    cd -        → chdir($OLDPWD), print dir                   │
│    Actualizar: $OLDPWD (antes), $PWD (después)               │
│                                                              │
│  exit:                                                       │
│    exit        → exit(last_status)                            │
│    exit N      → exit(N & 0xFF)                              │
│    Avisar si hay jobs background                             │
│                                                              │
│  export / unset:                                             │
│    export VAR=val  → setenv("VAR", "val", 1)                 │
│    export          → imprimir todo el entorno                │
│    unset VAR       → unsetenv("VAR")                         │
│                                                              │
│  SEÑALES:                                                    │
│    Shell:                                                    │
│      SIGINT  (Ctrl+C)  → ignorar o handler que reimprima     │
│      SIGQUIT (Ctrl+\)  → ignorar                            │
│      SIGTSTP (Ctrl+Z)  → ignorar                            │
│      SIGCHLD           → waitpid(WNOHANG) para background    │
│                                                              │
│    Hijos (antes de exec):                                    │
│      signal(SIGINT, SIG_DFL)                                 │
│      signal(SIGQUIT, SIG_DFL)                                │
│      signal(SIGTSTP, SIG_DFL)                                │
│                                                              │
│    Ctrl+C en prompt → limpiar línea, nuevo prompt            │
│      - NO usar SA_RESTART → getline retorna EINTR            │
│      - clearerr(stdin) después de EINTR                      │
│                                                              │
│    Ctrl+C con hijo → mata al hijo, shell sigue               │
│      - waitpid con WUNTRACED para detectar stopped           │
│      - WIFSTOPPED(status) → hijo detenido por Ctrl+Z         │
│      - WIFSIGNALED(status) → hijo matado por señal            │
└──────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: built-ins esenciales

Implementa los built-ins mínimos para una shell funcional:

1. **cd**: con soporte para `cd`, `cd dir`, `cd -`, `cd ~`.
   Actualizar `$PWD` y `$OLDPWD`.
2. **exit**: con código de salida opcional. Avisar si hay jobs.
3. **pwd**: imprimir directorio actual.
4. **export**: `export VAR=val` y `export` (listar todo).
5. **unset**: `unset VAR`.

Verificar:
```bash
minish$ pwd
/home/user
minish$ cd /tmp
minish$ pwd
/tmp
minish$ cd -
/home/user
minish$ export MY_VAR=hello
minish$ echo $MY_VAR      # (requiere expansión de variables)
# o verificar con: env | grep MY_VAR
minish$ /usr/bin/env | grep MY_VAR
MY_VAR=hello
minish$ unset MY_VAR
minish$ /usr/bin/env | grep MY_VAR
(nada)
```

**Pregunta de reflexión**: `export` sin argumentos en bash muestra
`declare -x VAR="valor"`. ¿Por qué bash usa `declare` en la salida
en vez de `export`? (Pista: `declare -x` y `export` no son
idénticos — `declare` tiene más opciones como `-i`, `-r`, `-a`.)

---

### Ejercicio 2: manejo de señales

Implementa el manejo de señales completo:

1. **Ctrl+C en el prompt**: limpiar la línea e imprimir nuevo
   prompt (no salir de la shell).
2. **Ctrl+C con comando en foreground**: matar el comando, la
   shell sigue. Imprimir la señal:
   ```
   minish$ sleep 30
   ^C
   minish$
   ```
3. **Ctrl+D en prompt vacío**: salir de la shell con `exit`.
4. **Ctrl+D en medio de línea**: ignorar (no salir).
5. **Ctrl+\ (SIGQUIT)**: ignorar en la shell, el hijo recibe
   core dump.

Verificar:
```bash
minish$ sleep 100
^C                    # mata sleep, shell sigue
minish$ ^C            # nueva línea, nuevo prompt
minish$               # Ctrl+D aquí → exit
```

**Pregunta de reflexión**: si ejecutas `cat` (sin argumentos) en
tu mini-shell y presionas Ctrl+C, ¿se mata cat? ¿Qué pasa si
presionas Ctrl+D? ¿Son ambas señales o una es especial? (Pista:
Ctrl+D no es una señal — genera EOF en el terminal driver.)

---

### Ejercicio 3: expansión de variables básica

Implementa expansión de variables de entorno en el tokenizador:

1. **`$VAR`**: sustituir por el valor de la variable. Si no existe,
   sustituir por cadena vacía.
2. **`$?`**: sustituir por el exit status del último comando.
3. **`$$`**: sustituir por el PID de la shell.
4. **`$HOME`**, **`$USER`**, **`$PATH`**: deben funcionar.
5. Dentro de comillas dobles: expandir. Dentro de comillas simples:
   **no** expandir.

```bash
minish$ echo $HOME
/home/user
minish$ echo $USER
user
minish$ echo "Hello $USER"
Hello user
minish$ echo 'No expansion: $USER'
No expansion: $USER
minish$ false
minish$ echo $?
1
minish$ echo $$
12345
```

Implementar como un paso de **expansión** entre tokenización y
parseo:

```
línea → tokenize → expand_variables → parse → execute
```

**Pregunta de reflexión**: bash hace la expansión de variables
**antes** de la división en palabras (word splitting). Esto
significa que `FILES="a b c"; ls $FILES` se expande a
`ls a b c` (3 argumentos). ¿Cómo manejaría tu implementación
este caso? ¿Es necesario re-tokenizar después de la expansión?
