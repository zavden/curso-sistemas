# Redirección con dup2

## Índice
1. [Duplicación de file descriptors](#1-duplicación-de-file-descriptors)
2. [dup, dup2 y dup3](#2-dup-dup2-y-dup3)
3. [Tabla de file descriptors del kernel](#3-tabla-de-file-descriptors-del-kernel)
4. [Redirección de stdout y stderr](#4-redirección-de-stdout-y-stderr)
5. [Redirección de entrada](#5-redirección-de-entrada)
6. [Implementar tuberías de shell](#6-implementar-tuberías-de-shell)
7. [Pipeline de N comandos](#7-pipeline-de-n-comandos)
8. [Combinación de redirecciones](#8-combinación-de-redirecciones)
9. [Patrones avanzados](#9-patrones-avanzados)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Duplicación de file descriptors

Cada proceso tiene una **tabla de file descriptors** — un array donde cada
índice apunta a una entrada en la tabla de archivos abiertos del kernel.
Cuando duplicas un file descriptor, creas un **nuevo índice** que apunta a
la **misma entrada**:

```
Antes de dup2(fd, 1):

  Tabla FD del proceso          Tabla de archivos del kernel
  ┌────┬──────────┐
  │ 0  │ stdin    │ ──────────► terminal (lectura)
  ├────┼──────────┤
  │ 1  │ stdout   │ ──────────► terminal (escritura)
  ├────┼──────────┤
  │ 2  │ stderr   │ ──────────► terminal (escritura)
  ├────┼──────────┤
  │ 3  │ fd       │ ──────────► archivo "log.txt"
  └────┴──────────┘

Después de dup2(fd, 1):

  Tabla FD del proceso          Tabla de archivos del kernel
  ┌────┬──────────┐
  │ 0  │ stdin    │ ──────────► terminal (lectura)
  ├────┼──────────┤
  │ 1  │ stdout   │ ──┐
  ├────┼──────────┤   ├──────► archivo "log.txt"  (refcount = 2)
  │ 2  │ stderr   │   │         terminal (escritura) fue cerrado
  ├────┼──────────┤   │
  │ 3  │ fd       │ ──┘
  └────┴──────────┘
```

El efecto: cualquier `printf()` o `write(1, ...)` ahora escribe en `log.txt`
en lugar de la terminal. El proceso hijo que llame `exec` **no sabe** que
stdout fue redirigido — simplemente escribe en fd 1 como siempre.

Esta es la base de toda la redirección de I/O en Unix.

---

## 2. dup, dup2 y dup3

### dup — duplicar al menor fd disponible

```c
#include <unistd.h>

int dup(int oldfd);
// Retorna: nuevo fd (el menor disponible), o -1 en error
```

No tienes control sobre **qué** número de fd obtienes. Útil cuando solo
necesitas una copia de respaldo:

```c
int backup = dup(STDOUT_FILENO);  // backup podría ser 3, 4, 5...
if (backup == -1) {
    perror("dup");
    exit(1);
}
```

### dup2 — duplicar a un fd específico

```c
int dup2(int oldfd, int newfd);
// Retorna: newfd en éxito, o -1 en error
```

Comportamiento clave:
- Si `newfd` ya está abierto, lo **cierra primero** (silenciosamente)
- Si `oldfd == newfd`, retorna `newfd` sin hacer nada
- La operación es **atómica** — no hay ventana donde `newfd` está cerrado
  pero aún no duplicado

```c
// Redirigir stdout a un archivo
int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
if (fd == -1) { perror("open"); exit(1); }

if (dup2(fd, STDOUT_FILENO) == -1) {
    perror("dup2");
    exit(1);
}
close(fd);  // fd 1 ya apunta al archivo, fd original ya no se necesita

printf("Esto va al archivo\n");  // ¡escribe en output.txt!
```

### dup3 — duplicar con flags (Linux 2.6.27+)

```c
#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>

int dup3(int oldfd, int newfd, int flags);
// flags: O_CLOEXEC (el único flag válido)
```

Resuelve una race condition: con `dup2` + `fcntl(F_SETFD, FD_CLOEXEC)`,
otro thread podría hacer `fork+exec` entre ambas llamadas, filtrando el fd.
`dup3` lo hace atómicamente.

```c
// Duplicar con close-on-exec en una sola llamada
if (dup3(oldfd, newfd, O_CLOEXEC) == -1) {
    perror("dup3");
    exit(1);
}
```

### Comparación

```
┌───────────┬─────────────────┬──────────┬───────────────┐
│ Función   │ Elige fd destino│ Atómica  │ O_CLOEXEC     │
├───────────┼─────────────────┼──────────┼───────────────┤
│ dup()     │ No (menor libre)│ Sí       │ No            │
│ dup2()    │ Sí              │ Sí       │ No            │
│ dup3()    │ Sí              │ Sí       │ Sí (flag)     │
│ fcntl     │ Sí (≥ arg)      │ Sí       │ F_DUPFD_CLOEXEC│
└───────────┴─────────────────┴──────────┴───────────────┘
```

`fcntl(oldfd, F_DUPFD, min)` duplica `oldfd` al menor fd ≥ `min`. Con
`F_DUPFD_CLOEXEC` establece close-on-exec atómicamente. Es útil cuando
necesitas un fd mínimo pero no uno exacto.

---

## 3. Tabla de file descriptors del kernel

Para entender dup2 hay que conocer las tres tablas:

```
  Proceso A           Kernel                    Sistema de archivos
  ┌────────┐     ┌─────────────────┐
  │ FD 0 ──┼────►│ Open file desc. │──► offset, flags, mode
  │ FD 1 ──┼──┐  │   (refcount=1)  │──► vnode / inode ──────► /dev/tty
  │ FD 2 ──┼──┤  └─────────────────┘
  └────────┘  │  ┌─────────────────┐
              ├─►│ Open file desc. │──► offset, flags, mode
              │  │   (refcount=2)  │──► vnode / inode ──────► /dev/tty
  Proceso B   │  └─────────────────┘
  ┌────────┐  │  ┌─────────────────┐
  │ FD 0 ──┼──┘  │ Open file desc. │──► offset, flags, mode
  │ FD 1 ──┼────►│   (refcount=1)  │──► vnode / inode ──────► log.txt
  └────────┘     └─────────────────┘
```

Puntos clave:

- **File descriptor**: índice local al proceso (0, 1, 2, ...)
- **Open file description**: estructura del kernel con offset, flags de
  acceso, referencia al inode. `dup2` hace que dos fd apunten a la **misma**
  open file description
- **Compartir offset**: si fd 1 y fd 3 apuntan a la misma descripción,
  un `write(1, ...)` avanza el offset que `write(3, ...)` también ve
- **fork** duplica la tabla de fd, pero los fd del hijo apuntan a las
  **mismas** open file descriptions que el padre (offset compartido)
- **open** siempre crea una **nueva** open file description

```c
// Demostrar offset compartido tras dup2
int fd = open("test.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
int fd2 = dup(fd);

write(fd, "Hello", 5);           // offset avanza a 5
off_t pos = lseek(fd2, 0, SEEK_CUR);
printf("offset en fd2: %ld\n", pos);  // Imprime 5 — ¡compartido!

close(fd);
close(fd2);
```

---

## 4. Redirección de stdout y stderr

### Redirección simple: `comando > archivo`

```c
void redirect_stdout(const char *filename)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) {
        perror("open");
        exit(1);
    }
    if (dup2(fd, STDOUT_FILENO) == -1) {
        perror("dup2");
        exit(1);
    }
    close(fd);  // ya no necesitamos el fd original
}

// Uso: implementar "ls -l > listing.txt"
pid_t pid = fork();
if (pid == 0) {
    redirect_stdout("listing.txt");
    execlp("ls", "ls", "-l", NULL);
    perror("exec");
    _exit(127);
}
waitpid(pid, NULL, 0);
```

### Append: `comando >> archivo`

La diferencia es un solo flag en `open`:

```c
void redirect_stdout_append(const char *filename)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd == -1) { perror("open"); exit(1); }
    if (dup2(fd, STDOUT_FILENO) == -1) { perror("dup2"); exit(1); }
    close(fd);
}
```

`O_APPEND` garantiza que cada `write` se posiciona al final atómicamente,
incluso con múltiples procesos escribiendo al mismo archivo.

### Redirección de stderr: `comando 2> errlog`

```c
void redirect_stderr(const char *filename)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); exit(1); }
    if (dup2(fd, STDERR_FILENO) == -1) { perror("dup2"); exit(1); }
    close(fd);
}
```

### Combinar stdout y stderr: `comando > archivo 2>&1`

El orden importa. Bash procesa las redirecciones **de izquierda a derecha**:

```
  1) > archivo     →  dup2(fd_archivo, 1)   stdout → archivo
  2) 2>&1          →  dup2(1, 2)            stderr → lo que sea fd 1 → archivo
```

```c
void redirect_both(const char *filename)
{
    int fd = open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); exit(1); }

    // Primero: stdout al archivo
    if (dup2(fd, STDOUT_FILENO) == -1) { perror("dup2"); exit(1); }
    // Segundo: stderr a donde apunta stdout (el archivo)
    if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1) { perror("dup2"); exit(1); }

    close(fd);
}
```

Si invirtieras el orden (primero `2>&1`, luego `> archivo`), stderr iría
a la terminal original y solo stdout al archivo. Diagrama:

```
  Orden CORRECTO: > archivo 2>&1
  ┌──────┐     ┌──────┐
  │ fd 1 │────►│      │
  │      │     │ file │    Ambos al archivo
  │ fd 2 │────►│      │
  └──────┘     └──────┘

  Orden INCORRECTO: 2>&1 > archivo
  ┌──────┐     ┌──────────┐
  │ fd 1 │────►│ archivo  │    Solo stdout al archivo
  │      │     └──────────┘
  │ fd 2 │────►┌──────────┐
  └──────┘     │ terminal │    stderr a la terminal
               └──────────┘
```

---

## 5. Redirección de entrada

### Entrada desde archivo: `comando < archivo`

```c
void redirect_stdin(const char *filename)
{
    int fd = open(filename, O_RDONLY);
    if (fd == -1) { perror("open"); exit(1); }
    if (dup2(fd, STDIN_FILENO) == -1) { perror("dup2"); exit(1); }
    close(fd);
}

// Uso: implementar "wc -l < data.txt"
pid_t pid = fork();
if (pid == 0) {
    redirect_stdin("data.txt");
    execlp("wc", "wc", "-l", NULL);
    perror("exec");
    _exit(127);
}
waitpid(pid, NULL, 0);
```

### Guardar y restaurar un fd

A veces necesitas redirigir temporalmente y luego volver al original:

```c
void run_silently(const char *cmd)
{
    // 1. Guardar stdout original
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout == -1) { perror("dup"); return; }

    // 2. Redirigir stdout a /dev/null
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull == -1) { perror("open"); close(saved_stdout); return; }
    dup2(devnull, STDOUT_FILENO);
    close(devnull);

    // 3. Ejecutar algo — su stdout va a /dev/null
    printf("Esto no se ve\n");
    fflush(stdout);  // importante: vaciar buffers de stdio

    // 4. Restaurar stdout original
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    printf("Esto sí se ve\n");
}
```

**Punto crítico**: `fflush(stdout)` antes de restaurar. Si no, los datos
en el buffer de stdio podrían ir al destino equivocado al hacer flush
después de restaurar.

---

## 6. Implementar tuberías de shell

Una tubería `ls | wc -l` conecta el stdout de un proceso con el stdin
del siguiente. Es la combinación de `pipe()` + `fork()` + `dup2()`:

```
                    pipe
  ls ──── fd[1] ═══════════ fd[0] ──── wc
  (stdout)    escritura    lectura    (stdin)
```

### Implementación paso a paso

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

// Implementar: ls -la /tmp | grep log
int main(void)
{
    int pipefd[2];

    // 1. Crear el pipe ANTES de fork
    if (pipe(pipefd) == -1) {
        perror("pipe");
        exit(1);
    }

    // 2. Primer hijo: ls (productor)
    pid_t pid1 = fork();
    if (pid1 == -1) { perror("fork"); exit(1); }

    if (pid1 == 0) {
        // Hijo 1: escribe en el pipe
        close(pipefd[0]);                       // no lee del pipe
        dup2(pipefd[1], STDOUT_FILENO);         // stdout → pipe write end
        close(pipefd[1]);                       // fd original ya no necesario

        execlp("ls", "ls", "-la", "/tmp", NULL);
        perror("exec ls");
        _exit(127);
    }

    // 3. Segundo hijo: grep (consumidor)
    pid_t pid2 = fork();
    if (pid2 == -1) { perror("fork"); exit(1); }

    if (pid2 == 0) {
        // Hijo 2: lee del pipe
        close(pipefd[1]);                       // no escribe en el pipe
        dup2(pipefd[0], STDIN_FILENO);          // stdin → pipe read end
        close(pipefd[0]);                       // fd original ya no necesario

        execlp("grep", "grep", "log", NULL);
        perror("exec grep");
        _exit(127);
    }

    // 4. Padre: cerrar AMBOS extremos (crucial)
    close(pipefd[0]);
    close(pipefd[1]);

    // 5. Esperar a ambos hijos
    waitpid(pid1, NULL, 0);
    waitpid(pid2, NULL, 0);

    return 0;
}
```

### Flujo temporal detallado

```
  Padre              Hijo 1 (ls)         Hijo 2 (grep)
  ──────             ───────────         ─────────────
  pipe()
  fork() ──────────► nace
  fork() ──────────────────────────────► nace
  close(fd[0])       close(fd[0])        close(fd[1])
  close(fd[1])       dup2(fd[1], 1)      dup2(fd[0], 0)
  waitpid(pid1)      close(fd[1])        close(fd[0])
  waitpid(pid2)      exec("ls")          exec("grep")
                     write(1, data)──────► read(0, data)
                     exit(0)             escribe a terminal
                     (pipe write end     EOF en stdin
                      cerrado → EOF)     exit(0)
```

### Por qué el padre DEBE cerrar ambos extremos

Si el padre no cierra `pipefd[0]`:
- El read end tiene refcount > 0 incluso después de que hijo 2 lo cierre
- `grep` nunca ve EOF → **se cuelga**

Si el padre no cierra `pipefd[1]`:
- El write end tiene refcount > 0
- Cuando `ls` termina, hijo 2 sigue esperando datos → **se cuelga**

Regla de oro: **cada proceso cierra los extremos del pipe que no usa**.

---

## 7. Pipeline de N comandos

Un shell real necesita encadenar N comandos: `cat file | sort | uniq | wc -l`.
El patrón generalizado:

```
  cmd[0] ══pipe0══ cmd[1] ══pipe1══ cmd[2] ══pipe2══ cmd[3]
  stdout→wr       rd→stdin         rd→stdin          rd→stdin
                   stdout→wr       stdout→wr
```

### Implementación genérica

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

// Cada comando es un array de strings terminado en NULL
// cmds[i] = {"cmd", "arg1", "arg2", NULL}
void run_pipeline(char **cmds[], int n)
{
    int prev_pipe[2] = {-1, -1};  // pipe del comando anterior

    pid_t *pids = calloc(n, sizeof(pid_t));
    if (!pids) { perror("calloc"); exit(1); }

    for (int i = 0; i < n; i++) {
        int curr_pipe[2] = {-1, -1};

        // Crear pipe para todos excepto el último comando
        if (i < n - 1) {
            if (pipe(curr_pipe) == -1) {
                perror("pipe");
                exit(1);
            }
        }

        pids[i] = fork();
        if (pids[i] == -1) { perror("fork"); exit(1); }

        if (pids[i] == 0) {
            // ─── Hijo ───

            // Conectar stdin al pipe anterior (excepto primer comando)
            if (i > 0) {
                dup2(prev_pipe[0], STDIN_FILENO);
                close(prev_pipe[0]);
                close(prev_pipe[1]);
            }

            // Conectar stdout al pipe actual (excepto último comando)
            if (i < n - 1) {
                dup2(curr_pipe[1], STDOUT_FILENO);
                close(curr_pipe[0]);
                close(curr_pipe[1]);
            }

            execvp(cmds[i][0], cmds[i]);
            fprintf(stderr, "exec %s: ", cmds[i][0]);
            perror("");
            _exit(127);
        }

        // ─── Padre ───

        // Cerrar pipe anterior (ya fue entregado al hijo)
        if (prev_pipe[0] != -1) {
            close(prev_pipe[0]);
            close(prev_pipe[1]);
        }

        // El pipe actual se convierte en el anterior
        prev_pipe[0] = curr_pipe[0];
        prev_pipe[1] = curr_pipe[1];
    }

    // Cerrar último pipe residual en el padre
    if (prev_pipe[0] != -1) {
        close(prev_pipe[0]);
        close(prev_pipe[1]);
    }

    // Esperar a todos los hijos
    for (int i = 0; i < n; i++) {
        int status;
        waitpid(pids[i], &status, 0);
    }

    free(pids);
}

// Ejemplo: cat /etc/passwd | sort | head -5
int main(void)
{
    char *cmd0[] = {"cat", "/etc/passwd", NULL};
    char *cmd1[] = {"sort", NULL};
    char *cmd2[] = {"head", "-5", NULL};
    char **pipeline[] = {cmd0, cmd1, cmd2};

    run_pipeline(pipeline, 3);
    return 0;
}
```

### Diagrama de fd para 3 comandos

```
  Iteración 0 (cat):    Iteración 1 (sort):    Iteración 2 (head):
  ┌─────────┐           ┌──────────┐           ┌──────────┐
  │   cat   │           │   sort   │           │   head   │
  │         │           │          │           │          │
  │ stdin ← │ terminal  │ stdin ← ─┤ pipe0[0] │ stdin ← ─┤ pipe1[0]
  │ stdout→ │ pipe0[1]  │ stdout→ ─┤ pipe1[1] │ stdout → │ terminal
  └─────────┘           └──────────┘           └──────────┘
          ╚═══pipe0═══╝          ╚═══pipe1═══╝
```

---

## 8. Combinación de redirecciones

Un shell real permite combinar pipes con redirecciones a archivos:

### `cmd < input > output`

```c
pid_t pid = fork();
if (pid == 0) {
    // Redirigir stdin desde archivo
    int in = open("input.txt", O_RDONLY);
    dup2(in, STDIN_FILENO);
    close(in);

    // Redirigir stdout a archivo
    int out = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(out, STDOUT_FILENO);
    close(out);

    execlp("sort", "sort", NULL);  // sort < input.txt > output.txt
    _exit(127);
}
waitpid(pid, NULL, 0);
```

### `cmd1 | cmd2 > output 2>&1`

```c
int pipefd[2];
pipe(pipefd);

pid_t pid1 = fork();
if (pid1 == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    execlp("find", "find", "/", "-name", "*.conf", NULL);
    _exit(127);
}

pid_t pid2 = fork();
if (pid2 == 0) {
    close(pipefd[1]);
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);

    // Redirigir salida a archivo
    int out = open("results.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    dup2(out, STDOUT_FILENO);
    dup2(STDOUT_FILENO, STDERR_FILENO);  // 2>&1
    close(out);

    execlp("head", "head", "-20", NULL);
    _exit(127);
}

close(pipefd[0]);
close(pipefd[1]);
waitpid(pid1, NULL, 0);
waitpid(pid2, NULL, 0);
```

### Función de redirección genérica

```c
typedef struct {
    int target_fd;       // 0=stdin, 1=stdout, 2=stderr
    const char *file;    // archivo destino (NULL si es dup de otro fd)
    int source_fd;       // fd fuente para 2>&1 (ignorado si file != NULL)
    int flags;           // O_RDONLY, O_WRONLY|O_CREAT|O_TRUNC, etc.
} redirect_t;

void apply_redirections(redirect_t *redirs, int count)
{
    for (int i = 0; i < count; i++) {
        if (redirs[i].file != NULL) {
            int fd = open(redirs[i].file, redirs[i].flags, 0644);
            if (fd == -1) { perror("open"); _exit(1); }
            if (dup2(fd, redirs[i].target_fd) == -1) {
                perror("dup2"); _exit(1);
            }
            close(fd);
        } else {
            // Duplicar desde otro fd (e.g., 2>&1)
            if (dup2(redirs[i].source_fd, redirs[i].target_fd) == -1) {
                perror("dup2"); _exit(1);
            }
        }
    }
}

// Uso: cmd > out.txt 2>&1
redirect_t redirs[] = {
    {STDOUT_FILENO, "out.txt", -1, O_WRONLY | O_CREAT | O_TRUNC},
    {STDERR_FILENO, NULL, STDOUT_FILENO, 0},  // 2>&1
};
apply_redirections(redirs, 2);
```

---

## 9. Patrones avanzados

### Capturar la salida de un comando

```c
// Capturar stdout de un comando en un buffer
char *capture_output(char *const argv[])
{
    int pipefd[2];
    if (pipe(pipefd) == -1) return NULL;

    pid_t pid = fork();
    if (pid == -1) { close(pipefd[0]); close(pipefd[1]); return NULL; }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipefd[1]);

    // Leer toda la salida
    size_t capacity = 4096, len = 0;
    char *buf = malloc(capacity);
    if (!buf) { close(pipefd[0]); return NULL; }

    ssize_t n;
    while ((n = read(pipefd[0], buf + len, capacity - len - 1)) > 0) {
        len += n;
        if (len + 1 >= capacity) {
            capacity *= 2;
            char *tmp = realloc(buf, capacity);
            if (!tmp) { free(buf); close(pipefd[0]); return NULL; }
            buf = tmp;
        }
    }
    close(pipefd[0]);
    buf[len] = '\0';

    int status;
    waitpid(pid, &status, 0);

    return buf;
}
```

### Here-document (<<EOF)

Alimentar datos al stdin de un proceso mediante un pipe:

```c
void run_with_input(char *const argv[], const char *input)
{
    int pipefd[2];
    if (pipe(pipefd) == -1) { perror("pipe"); return; }

    pid_t pid = fork();
    if (pid == 0) {
        close(pipefd[1]);
        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[0]);
        execvp(argv[0], argv);
        _exit(127);
    }

    close(pipefd[0]);

    // Escribir toda la entrada
    size_t total = strlen(input);
    size_t written = 0;
    while (written < total) {
        ssize_t n = write(pipefd[1], input + written, total - written);
        if (n == -1) {
            if (errno == EINTR) continue;
            break;  // EPIPE: el hijo cerró stdin
        }
        written += n;
    }
    close(pipefd[1]);  // señalar EOF al hijo

    waitpid(pid, NULL, 0);
}

// Uso: echo input | bc
char *args[] = {"bc", NULL};
run_with_input(args, "2 + 3 * 4\n");
```

### Redirigir a /dev/null (silenciar)

```c
void run_silent(char *const argv[])
{
    pid_t pid = fork();
    if (pid == 0) {
        int devnull = open("/dev/null", O_RDWR);
        if (devnull == -1) _exit(1);

        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        if (devnull > STDERR_FILENO)
            close(devnull);

        execvp(argv[0], argv);
        _exit(127);
    }
    waitpid(pid, NULL, 0);
}
```

Nota: si `/dev/null` se abrió y resultó en fd = 0 (porque stdin ya estaba
cerrado), no debemos cerrarlo después de los `dup2`. La guarda
`if (devnull > STDERR_FILENO)` previene esto.

### Intercambiar stdout y stderr

```c
// Patrón: 3>&1 1>&2 2>&3 3>&-
void swap_stdout_stderr(void)
{
    int tmp = dup(STDOUT_FILENO);    // tmp = 3 (copia de stdout)
    dup2(STDERR_FILENO, STDOUT_FILENO);  // stdout → donde estaba stderr
    dup2(tmp, STDERR_FILENO);            // stderr → donde estaba stdout
    close(tmp);
}
```

---

## 10. Errores comunes

### Error 1: No cerrar el fd original después de dup2

```c
// MAL: fuga de file descriptor
int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, STDOUT_FILENO);
// fd sigue abierto, consumiendo un slot en la tabla

// BIEN: cerrar después de duplicar
int fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, STDOUT_FILENO);
close(fd);  // fd ya no se necesita, STDOUT_FILENO apunta al archivo
```

**Por qué importa**: cada proceso tiene un límite de file descriptors
(por defecto 1024). En un servidor que redirige miles de veces, la fuga
acumula fd muertos hasta alcanzar EMFILE.

### Error 2: Orden incorrecto en `> archivo 2>&1`

```c
// MAL: stderr va a la terminal, no al archivo
dup2(STDOUT_FILENO, STDERR_FILENO);  // 2>&1 primero: stderr → terminal
int fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, STDOUT_FILENO);             // solo stdout → archivo
close(fd);

// BIEN: primero redirigir stdout, luego stderr
int fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, STDOUT_FILENO);             // stdout → archivo
dup2(STDOUT_FILENO, STDERR_FILENO);  // stderr → archivo (sigue a stdout)
close(fd);
```

**Regla**: las redirecciones se aplican **en orden secuencial**. `2>&1`
copia el destino actual de fd 1, no el futuro.

### Error 3: No vaciar buffers de stdio antes de dup2

```c
// MAL: "Hello" podría ir al archivo en vez de la terminal
printf("Hello");  // queda en buffer de stdio
int fd = open("log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, STDOUT_FILENO);
close(fd);
// El buffer de printf se vacía después — "Hello" va a log.txt

// BIEN: fflush antes de redirigir
printf("Hello");
fflush(stdout);  // forzar escritura a terminal
int fd = open("log.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
dup2(fd, STDOUT_FILENO);
close(fd);
```

**Especialmente importante** al combinar `fork()` con `dup2()`: el fork
duplica los buffers de stdio, y ambos procesos podrían hacer flush al
nuevo destino.

### Error 4: Olvidar cerrar extremos del pipe en el padre

```c
// MAL: grep se cuelga esperando EOF
int pipefd[2];
pipe(pipefd);
if (fork() == 0) {
    close(pipefd[0]);
    dup2(pipefd[1], STDOUT_FILENO);
    close(pipefd[1]);
    execlp("ls", "ls", NULL);
    _exit(127);
}
if (fork() == 0) {
    close(pipefd[1]);
    dup2(pipefd[0], STDIN_FILENO);
    close(pipefd[0]);
    execlp("grep", "grep", "txt", NULL);
    _exit(127);
}
// Padre NO cierra pipefd[0] ni pipefd[1]
// grep nunca recibe EOF porque el padre mantiene pipefd[1] abierto
wait(NULL); wait(NULL);  // deadlock

// BIEN:
close(pipefd[0]);
close(pipefd[1]);
wait(NULL); wait(NULL);
```

### Error 5: dup2 cuando oldfd no es válido

```c
// MAL: no verificar que open tuvo éxito
int fd = open("maybe_missing.txt", O_RDONLY);
// Si open falló, fd = -1
dup2(fd, STDIN_FILENO);  // dup2(-1, 0) retorna -1 con EBADF
// stdin queda como estaba, pero no verificamos el error
execlp("cat", "cat", NULL);  // cat lee de terminal, no del archivo

// BIEN: verificar cada paso
int fd = open("maybe_missing.txt", O_RDONLY);
if (fd == -1) {
    perror("open");
    _exit(1);
}
if (dup2(fd, STDIN_FILENO) == -1) {
    perror("dup2");
    _exit(1);
}
close(fd);
```

---

## 11. Cheatsheet

```
┌─────────────────────────────────────────────────────────────────────┐
│                    REDIRECCIÓN CON DUP2                            │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  Funciones:                                                         │
│    dup(oldfd)              → duplica al menor fd libre              │
│    dup2(oldfd, newfd)      → duplica a newfd (cierra si abierto)    │
│    dup3(oldfd, newfd, fl)  → dup2 + O_CLOEXEC atómico              │
│                                                                     │
│  Redirecciones de shell:                                            │
│    > archivo   → open(O_WRONLY|O_CREAT|O_TRUNC) + dup2(fd, 1)     │
│    >> archivo  → open(O_WRONLY|O_CREAT|O_APPEND) + dup2(fd, 1)    │
│    < archivo   → open(O_RDONLY) + dup2(fd, 0)                      │
│    2> archivo  → open(O_WRONLY|O_CREAT|O_TRUNC) + dup2(fd, 2)     │
│    2>&1        → dup2(1, 2)   (stderr a donde apunta stdout)       │
│    > f 2>&1    → dup2(fd,1) LUEGO dup2(1,2)  ← ¡orden importa!   │
│                                                                     │
│  Pipe de shell (A | B):                                             │
│    pipe(pipefd)                                                     │
│    fork A: close(pipefd[0]), dup2(pipefd[1], 1), close(pipefd[1]) │
│    fork B: close(pipefd[1]), dup2(pipefd[0], 0), close(pipefd[0]) │
│    padre: close(pipefd[0]), close(pipefd[1]), wait, wait           │
│                                                                     │
│  Guardar/restaurar:                                                 │
│    int saved = dup(STDOUT_FILENO);    // backup                    │
│    dup2(fd, STDOUT_FILENO);           // redirigir                 │
│    fflush(stdout);                    // ¡vaciar buffers!          │
│    dup2(saved, STDOUT_FILENO);        // restaurar                 │
│    close(saved);                                                    │
│                                                                     │
│  Reglas de oro:                                                     │
│    1. close(fd) después de dup2(fd, target) — evitar fugas         │
│    2. fflush() antes de redirigir — evitar buffers perdidos        │
│    3. Orden de redirecciones = orden de ejecución de dup2          │
│    4. Padre SIEMPRE cierra ambos extremos de pipes                 │
│    5. Verificar retorno de open() Y dup2()                         │
│                                                                     │
│  Constantes:                                                        │
│    STDIN_FILENO  = 0                                                │
│    STDOUT_FILENO = 1                                                │
│    STDERR_FILENO = 2                                                │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Mini-shell con redirecciones

Implementa una función `execute_with_redirections()` que reciba un comando
y un array de redirecciones, y ejecute el comando con todas las redirecciones
aplicadas:

```c
typedef struct {
    enum { REDIR_IN, REDIR_OUT, REDIR_APPEND, REDIR_ERR, REDIR_DUP } type;
    const char *filename;   // NULL para REDIR_DUP
    int dup_source;         // solo para REDIR_DUP (e.g., 1 para 2>&1)
} redir_t;

int execute_with_redirections(char *const argv[], redir_t *redirs, int n_redirs);
```

Debe soportar:
- `cmd > file`, `cmd >> file`, `cmd < file`, `cmd 2> file`
- `cmd > file 2>&1` y `cmd 2>&1 > file` (diferente resultado)
- Retornar el exit status del comando

**Pregunta de reflexión**: ¿Qué ocurre si el usuario escribe
`cmd > /dev/full`? ¿Cómo se propaga el error de escritura al exit status?

### Ejercicio 2: Pipeline con redirecciones en los extremos

Extiende la función `run_pipeline()` de la sección 7 para soportar
redirecciones en el primer y último comando del pipeline:

```c
typedef struct {
    char **argv;
    const char *stdin_file;   // NULL = heredar, solo válido para cmd[0]
    const char *stdout_file;  // NULL = heredar, solo válido para cmd[n-1]
    int append;               // 1 = >>, 0 = >
} pipeline_cmd_t;

void run_pipeline_ext(pipeline_cmd_t *cmds, int n);
```

Implementar el equivalente de:
`sort < unsorted.txt | uniq | head -20 > results.txt`

**Pregunta de reflexión**: Si `sort` tarda mucho pero `head -20` ya tiene
sus 20 líneas, ¿qué pasa con `sort` y `uniq`? ¿Quién les envía la señal
y por qué?

### Ejercicio 3: Tee con dup2

Implementa un `tee` casero que duplique stdout a un archivo sin usar el
comando `tee` externo. El proceso padre lee de stdin y escribe tanto a
stdout como a un archivo:

```c
void my_tee(const char *filename, int append);
// Lee stdin, escribe a stdout Y al archivo simultáneamente
```

Extensión: implementa el equivalente de `cmd | tee log.txt | other_cmd`
creando la infraestructura de pipes y dup2 necesaria.

**Pregunta de reflexión**: El `tee` real puede recibir SIGPIPE si uno de
sus destinos (stdout o el pipe al siguiente comando) se cierra. ¿Debería
`tee` ignorar SIGPIPE y seguir escribiendo al otro destino, o terminar
inmediatamente? ¿Qué hace GNU tee con la opción `--output-error`?
