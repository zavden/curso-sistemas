# La Familia exec — Reemplazar el Programa de un Proceso

## Índice

1. [Qué hace exec](#1-qué-hace-exec)
2. [Las seis variantes: nomenclatura](#2-las-seis-variantes-nomenclatura)
3. [execvp: la más usada](#3-execvp-la-más-usada)
4. [execv: ruta explícita](#4-execv-ruta-explícita)
5. [execl y execlp: argumentos inline](#5-execl-y-execlp-argumentos-inline)
6. [execle y execve: controlar el entorno](#6-execle-y-execve-controlar-el-entorno)
7. [execvpe: ruta por PATH + entorno custom](#7-execvpe-ruta-por-path--entorno-custom)
8. [Qué se preserva y qué se descarta](#8-qué-se-preserva-y-qué-se-descarta)
9. [Errores de exec y manejo](#9-errores-de-exec-y-manejo)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué hace exec

`exec` **reemplaza** el programa actual del proceso por otro programa.
No crea un proceso nuevo — el PID se mantiene, pero el código, datos,
heap y stack se reemplazan completamente:

```
  Antes de exec:                 Después de exec:
  ┌──────────────────┐           ┌──────────────────┐
  │ PID: 1234        │           │ PID: 1234        │ ← mismo PID
  │                  │           │                  │
  │ Código: mi_prog  │  exec()   │ Código: /bin/ls  │ ← nuevo código
  │ Datos: variables │ ────────► │ Datos: (nuevos)  │ ← nuevos datos
  │ Heap: malloc'd   │           │ Heap: (nuevo)    │
  │ Stack: frames    │           │ Stack: main()    │ ← nuevo stack
  │                  │           │                  │
  │ fd 0,1,2 ────────│──────────│─ fd 0,1,2       │ ← se preservan*
  │ PID, PPID ───────│──────────│─ PID, PPID      │ ← se preservan
  └──────────────────┘           └──────────────────┘

  * Los fds sin O_CLOEXEC se preservan
```

### exec no retorna (en caso de éxito)

```c
printf("Antes de exec\n");
execl("/bin/ls", "ls", "-l", NULL);
// Si exec tiene éxito, NUNCA se llega aquí
printf("Esto nunca se imprime\n");

// Si exec falla, SÍ retorna (con -1):
perror("exec falló");
```

Esta es una propiedad única de exec: es la única función que, en caso
de éxito, no retorna. El código que estaba ejecutándose ya no existe
— fue reemplazado por el nuevo programa.

---

## 2. Las seis variantes: nomenclatura

Las funciones `exec` siguen un patrón de nombrado sistemático:

```
  exec + [l|v] + [p] + [e]

  l = list:    argumentos como lista de parámetros (variadic)
  v = vector:  argumentos como array (char *argv[])
  p = path:    busca el programa en $PATH
  e = environ: se pasa un entorno personalizado
```

### Tabla completa

```
  Función    Args        Búsqueda      Entorno
  ──────────────────────────────────────────────────────
  execl      lista       ruta exacta   hereda
  execlp     lista       $PATH         hereda
  execle     lista       ruta exacta   custom
  execv      array       ruta exacta   hereda
  execvp     array       $PATH         hereda
  execvpe*   array       $PATH         custom

  * execvpe es extensión GNU, no POSIX
```

### La syscall real: execve

Solo existe **una** syscall: `execve`. Todas las demás son wrappers
de glibc:

```c
// La única syscall real:
int execve(const char *pathname, char *const argv[],
           char *const envp[]);
```

```
  execlp("ls", "ls", "-l", NULL)
     │
     └──► glibc busca "ls" en $PATH → "/usr/bin/ls"
          glibc construye argv[] desde la lista variadic
          glibc pasa environ como envp
     │
     └──► execve("/usr/bin/ls", ["ls","-l",NULL], environ)
```

---

## 3. execvp: la más usada

```c
int execvp(const char *file, char *const argv[]);
```

- `v`: argumentos como array.
- `p`: busca en `$PATH`.
- Hereda el entorno del proceso actual.

### Ejemplo

```c
#include <stdio.h>
#include <unistd.h>

int main(void) {
    char *argv[] = {"ls", "-l", "-a", "/tmp", NULL};

    execvp("ls", argv);

    // Solo se llega aquí si exec falló
    perror("execvp");
    return 1;
}
```

### Por qué es la más usada

1. **Array de argumentos**: más práctico que lista variadic cuando
   los argumentos se construyen dinámicamente.
2. **Búsqueda en PATH**: no necesitas saber la ruta completa del
   programa.
3. **Hereda el entorno**: el comportamiento normal esperado.

### argv[0]: convención del nombre del programa

`argv[0]` es el nombre con el que el programa se ve a sí mismo.
Por convención, debe ser el nombre del ejecutable, pero puede ser
cualquier cosa:

```c
// Normal: argv[0] = nombre del programa
char *argv[] = {"ls", "-l", NULL};
execvp("ls", argv);

// Truco: argv[0] diferente al programa real
char *argv[] = {"mi_ls_custom", "-l", NULL};
execvp("ls", argv);
// ls se ejecuta pero argv[0] == "mi_ls_custom"
// Algunos programas cambian su comportamiento según argv[0]
// (ej: busybox, gzip/gunzip/zcat son el mismo binario)
```

### Construcción dinámica de argv

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void run_command(const char *cmd) {
    // Parsear el comando en tokens (simplificado)
    char *copy = strdup(cmd);
    char *argv[64];
    int argc = 0;

    char *token = strtok(copy, " ");
    while (token && argc < 63) {
        argv[argc++] = token;
        token = strtok(NULL, " ");
    }
    argv[argc] = NULL;

    execvp(argv[0], argv);
    perror("execvp");
    free(copy);
}
```

---

## 4. execv: ruta explícita

```c
int execv(const char *pathname, char *const argv[]);
```

Como `execvp` pero **sin búsqueda en PATH**. Debes dar la ruta
completa del ejecutable:

```c
char *argv[] = {"ls", "-l", NULL};

// Con execvp: busca "ls" en PATH
execvp("ls", argv);

// Con execv: ruta exacta obligatoria
execv("/usr/bin/ls", argv);  // ✓
execv("ls", argv);           // ✗ falla si "ls" no está en CWD
```

### Cuándo usar execv en lugar de execvp

```
  execv (ruta explícita):
  - Seguridad: sabes exactamente qué ejecutable se invoca
  - No depende de $PATH (que podría ser manipulado)
  - Programas en rutas no estándar (/opt/app/bin/tool)
  - Scripts de instalación/deploy

  execvp (búsqueda en PATH):
  - Conveniencia: el usuario elige la versión del programa
  - Shells y herramientas interactivas
  - Cuando el programa puede estar en distintas rutas
```

---

## 5. execl y execlp: argumentos inline

```c
int execl(const char *pathname, const char *arg, ... /*, NULL */);
int execlp(const char *file, const char *arg, ... /*, NULL */);
```

Las variantes con `l` toman los argumentos como parámetros individuales
terminados en `NULL`:

### execl: ruta exacta + argumentos lista

```c
execl("/bin/ls", "ls", "-l", "-a", "/tmp", NULL);
//    ^^^^^^^^    ^^    ^^    ^^    ^^^^    ^^^^
//    ruta        argv[0] argv[1] argv[2] argv[3] centinela
```

### execlp: PATH + argumentos lista

```c
execlp("ls", "ls", "-l", "-a", "/tmp", NULL);
//     ^^    ^^    ^^    ^^    ^^^^    ^^^^
//     busca  argv[0-3]                centinela
//     en PATH
```

### Cuándo usar las variantes `l`

Las variantes `l` son convenientes cuando los argumentos son
**conocidos en tiempo de compilación**:

```c
// CON lista (argumentos conocidos): más legible
execlp("gcc", "gcc", "-Wall", "-o", "prog", "main.c", NULL);

// CON vector (argumentos dinámicos): más flexible
char *argv[64];
int i = 0;
argv[i++] = "gcc";
argv[i++] = "-Wall";
if (debug) argv[i++] = "-g";
argv[i++] = "-o";
argv[i++] = output_name;
for (int j = 0; j < num_sources; j++)
    argv[i++] = sources[j];
argv[i] = NULL;
execvp("gcc", argv);
```

### El NULL final es obligatorio

```c
// MAL: sin NULL al final
execl("/bin/ls", "ls", "-l");
// Comportamiento indefinido — la función no sabe dónde
// termina la lista de argumentos

// BIEN:
execl("/bin/ls", "ls", "-l", (char *)NULL);
// El cast a (char *) evita warnings en algunos compiladores
```

---

## 6. execle y execve: controlar el entorno

```c
int execle(const char *pathname, const char *arg, ...
           /*, NULL, char *const envp[] */);
int execve(const char *pathname, char *const argv[],
           char *const envp[]);
```

Las variantes con `e` permiten pasar un **entorno personalizado**:

### execve (la syscall real)

```c
char *argv[] = {"my_program", "--verbose", NULL};
char *envp[] = {
    "HOME=/home/user",
    "PATH=/usr/bin:/bin",
    "LANG=es_ES.UTF-8",
    "MY_VAR=custom_value",
    NULL
};

execve("/usr/local/bin/my_program", argv, envp);
```

El nuevo programa **solo** ve las variables de entorno en `envp`.
Las variables del proceso actual **no se heredan**.

### execle (lista + entorno)

```c
char *envp[] = {
    "HOME=/home/user",
    "PATH=/usr/bin",
    NULL
};

execle("/usr/bin/env", "env", NULL, envp);
// Imprime solo HOME y PATH (nada más)
```

### Casos de uso para entorno personalizado

```
  Caso                           Por qué
  ──────────────────────────────────────────────────────
  Seguridad: limpiar entorno     evitar LD_PRELOAD,
                                 LD_LIBRARY_PATH, etc.
  Sandbox: limitar PATH          controlar qué programas
                                 puede ejecutar el hijo
  Testing: simular entorno       inyectar variables de
                                 prueba sin afectar el
                                 proceso actual
  Contenedores: aislar           cada proceso con su
                                 propio entorno
```

### Patrón: limpiar entorno por seguridad

```c
// Programa setuid: NO heredar el entorno del usuario
char *safe_env[] = {
    "PATH=/usr/bin:/bin",
    "HOME=/root",
    "LANG=C",
    NULL
};

execve("/usr/sbin/my_secure_tool", argv, safe_env);
```

### Heredar entorno pero agregar/modificar variables

Si quieres el entorno actual **más** algunas variables extra, usa
`setenv` antes de `execvp` (que hereda el entorno):

```c
setenv("MY_VAR", "value", 1);    // agregar
setenv("PATH", "/custom/bin:" PATH_ORIGINAL, 1);  // modificar

execvp("my_program", argv);     // hereda entorno modificado
```

---

## 7. execvpe: ruta por PATH + entorno custom

```c
#define _GNU_SOURCE
#include <unistd.h>

int execvpe(const char *file, char *const argv[],
            char *const envp[]);
```

`execvpe` es una **extensión GNU** (no POSIX) que combina:
- `v`: argumentos como array.
- `p`: búsqueda en PATH.
- `e`: entorno personalizado.

```c
char *argv[] = {"python3", "script.py", NULL};
char *envp[] = {
    "PATH=/usr/bin:/bin",
    "HOME=/home/user",
    "PYTHONPATH=/app/lib",
    NULL
};

execvpe("python3", argv, envp);
```

### ¿Qué PATH usa?

Atención: `execvpe` busca en el `PATH` del **entorno que le pasas**,
no en el `PATH` actual del proceso:

```c
char *envp[] = {
    "PATH=/opt/custom/bin",  // busca aquí
    NULL
};

execvpe("mytool", argv, envp);
// Busca /opt/custom/bin/mytool (NO en el PATH actual)
```

### Si no tienes execvpe (portabilidad)

```c
// Equivalente portable:
setenv("PATH", "/custom/path", 1);
setenv("MY_VAR", "value", 1);
execvp("program", argv);
// Pero esto modifica el entorno del proceso actual
// (no importa si exec tiene éxito, pero sí si falla)
```

---

## 8. Qué se preserva y qué se descarta

### Se PRESERVA después de exec

```
  Recurso                   Detalle
  ──────────────────────────────────────────────────────
  PID                       el mismo proceso
  PPID                      mismo padre
  UID/GID real              se mantienen
  UID/GID efectivo          se mantienen (salvo setuid)
  Session ID, Process group se mantienen
  Terminal de control       la misma
  CWD                       el mismo
  Umask                     la misma
  File descriptors          SIN O_CLOEXEC se mantienen
  Señales: disposición      SIG_DFL y SIG_IGN se mantienen
  Señales: máscara          se mantiene
  Nice value                se mantiene
  Locks (fcntl)             se mantienen
  Alarmas (alarm)           se mantienen
  Límites de recursos       se mantienen
```

### Se DESCARTA después de exec

```
  Recurso                   Detalle
  ──────────────────────────────────────────────────────
  Código (text)             reemplazado por nuevo programa
  Datos, BSS, heap          reemplazados
  Stack                     reemplazado (nuevo main)
  Mapeos mmap               descartados
  File descriptors con      cerrados automáticamente
  O_CLOEXEC
  Señales: handlers         reseteados a SIG_DFL
                            (el código del handler ya no existe)
  Threads                   todos los hilos mueren excepto
                            el que llamó exec
  Shared memory (shmat)     desadjuntada
  Semáforos POSIX            cerrados
  Timer POSIX               eliminados
  Directory streams (DIR*)  cerrados
```

### O_CLOEXEC: la conexión entre exec y file descriptors

```c
// fd SIN O_CLOEXEC: sobrevive a exec
int fd1 = open("file.txt", O_RDONLY);

// fd CON O_CLOEXEC: se cierra automáticamente en exec
int fd2 = open("file.txt", O_RDONLY | O_CLOEXEC);

execvp("child_program", argv);
// child_program hereda fd1 pero NO fd2
```

### Señales y exec

```c
// Antes de exec:
signal(SIGINT, my_handler);   // handler personalizado
signal(SIGTERM, SIG_IGN);     // ignorar
signal(SIGUSR1, SIG_DFL);     // default

// Después de exec:
// SIGINT  → SIG_DFL (handler no existe en el nuevo programa)
// SIGTERM → SIG_IGN (se preserva)
// SIGUSR1 → SIG_DFL (se preserva)
```

Los handlers personalizados se resetean a `SIG_DFL` porque el código
del handler ya no existe (fue reemplazado). `SIG_IGN` y `SIG_DFL`
se preservan porque no referencian código específico.

### Setuid/setgid en exec

Si el nuevo ejecutable tiene el bit setuid, `exec` cambia el
UID efectivo:

```bash
$ ls -l /usr/bin/passwd
-rwsr-xr-x 1 root root 68208 ... /usr/bin/passwd
#  ^
#  setuid bit

# Cuando un usuario ejecuta passwd:
# UID real    = 1000 (el usuario)
# UID efectivo = 0 (root) — por el setuid bit
```

---

## 9. Errores de exec y manejo

Si `exec` falla, retorna -1 y establece `errno`:

```
  Error           Causa
  ──────────────────────────────────────────────────────
  ENOENT          el archivo no existe
  EACCES          sin permiso de ejecución
  ENOEXEC         formato no reconocido (no es ELF,
                  ni script con #!)
  ENOMEM          sin memoria para el nuevo programa
  E2BIG           argv + envp demasiado grandes
  ETXTBSY         el archivo se está escribiendo
  ELOOP           demasiados symlinks
  ENAMETOOLONG    ruta demasiado larga
```

### Patrón: fork + exec con manejo de errores

```c
pid_t pid = fork();
if (pid == -1) {
    perror("fork");
    return -1;
}

if (pid == 0) {
    // Hijo: intentar exec
    execvp(program, argv);

    // Si llegamos aquí, exec falló
    fprintf(stderr, "exec %s: %s\n", program, strerror(errno));
    _exit(127);  // 127 = convención para "command not found"
}

// Padre: esperar al hijo
int status;
waitpid(pid, &status, 0);

if (WIFEXITED(status)) {
    int code = WEXITSTATUS(status);
    if (code == 127) {
        fprintf(stderr, "Programa '%s' no encontrado\n", program);
    }
}
```

### Exit codes por convención

```
  Código    Significado (convención de shells)
  ──────────────────────────────────────────────────────
  0         éxito
  1-125     error del programa
  126       archivo existe pero no es ejecutable
  127       programa no encontrado
  128+N     terminado por señal N (ej: 128+9=137 = SIGKILL)
```

### Scripts y #! (shebang)

`exec` puede ejecutar scripts si tienen la línea shebang:

```bash
#!/usr/bin/python3
print("Hello")
```

```c
execl("script.py", "script.py", NULL);
// El kernel lee "#!/usr/bin/python3" y realmente ejecuta:
// execve("/usr/bin/python3", ["script.py", "script.py"], envp)
```

Si el script no tiene shebang, `exec` falla con `ENOEXEC`. Las
variantes con `p` (execlp, execvp) intentan ejecutarlo con `/bin/sh`
como fallback:

```c
// archivo "mi_script" sin shebang:
// echo "hola"

execvp("mi_script", argv);
// execlp internamente intenta:
// 1. execve("mi_script", ...) → ENOEXEC
// 2. execve("/bin/sh", ["sh", "mi_script", ...]) → OK
```

---

## 10. Errores comunes

### Error 1: olvidar el NULL al final en las variantes `l`

```c
// MAL: sin NULL centinela
execl("/bin/ls", "ls", "-l");
// Comportamiento indefinido — lee basura del stack

// BIEN:
execl("/bin/ls", "ls", "-l", (char *)NULL);
```

El cast `(char *)NULL` es necesario en C porque `NULL` puede ser
`(void *)0` o `0`, y en una función variadic el compilador no puede
inferir el tipo correcto.

### Error 2: olvidar argv[0]

```c
// MAL: argv[0] no es el nombre del programa
char *argv[] = {"-l", "/tmp", NULL};
execvp("ls", argv);
// ls recibe argv[0] = "-l" (piensa que es su nombre)
// Comportamiento confuso

// BIEN: argv[0] = nombre del programa
char *argv[] = {"ls", "-l", "/tmp", NULL};
execvp("ls", argv);
```

### Error 3: no manejar el fallo de exec

```c
// MAL: continuar como si nada después de exec fallido
pid_t pid = fork();
if (pid == 0) {
    execvp("programa_inexistente", argv);
    // exec falló, pero el hijo sigue ejecutando
    // el código del padre — puede causar estragos
    printf("¿Soy el padre? ¿Soy el hijo? Caos.\n");
}

// BIEN: _exit inmediatamente si exec falla
if (pid == 0) {
    execvp("programa_inexistente", argv);
    perror("exec");
    _exit(127);
}
```

Si no haces `_exit` después de un exec fallido, el hijo continúa
ejecutando el código del padre. Esto puede causar que dos procesos
escriban al mismo archivo, acepten conexiones del mismo socket, etc.

### Error 4: fds que leakean al exec

```c
// MAL: el hijo hereda un fd sensible
int secret_fd = open("/etc/shadow", O_RDONLY);
// ...
pid_t pid = fork();
if (pid == 0) {
    execvp("untrusted_program", argv);
    // untrusted_program hereda secret_fd y puede leer /etc/shadow
}

// BIEN: usar O_CLOEXEC
int secret_fd = open("/etc/shadow", O_RDONLY | O_CLOEXEC);
// O cerrar antes de exec:
if (pid == 0) {
    close(secret_fd);
    execvp("untrusted_program", argv);
}
```

### Error 5: confundir exec con system

```c
// system() = fork + exec + wait (todo junto)
system("ls -l /tmp");
// Crea un shell, ejecuta el comando, espera que termine
// Vulnerable a inyección si el argumento viene del usuario

// exec() = solo reemplazar el programa
execl("/bin/ls", "ls", "-l", "/tmp", NULL);
// NO crea un shell, NO retorna (si tiene éxito)
// Más seguro: no hay interpretación de shell
```

`system()` pasa el comando a `/bin/sh -c "..."`, lo que permite
inyección si el argumento contiene metacaracteres del shell:

```c
// PELIGROSO:
char cmd[256];
snprintf(cmd, sizeof(cmd), "ls %s", user_input);
system(cmd);  // si user_input = "; rm -rf /", desastre

// SEGURO:
char *argv[] = {"ls", user_input, NULL};
execvp("ls", argv);  // user_input es un argumento, no un comando
```

---

## 11. Cheatsheet

```
  VARIANTES DE EXEC
  ──────────────────────────────────────────────────────────
  execl(path, arg0, arg1, ..., NULL)     lista + ruta
  execlp(file, arg0, arg1, ..., NULL)    lista + PATH
  execle(path, arg0, ..., NULL, envp)    lista + ruta + env
  execv(path, argv)                      array + ruta
  execvp(file, argv)                     array + PATH
  execvpe(file, argv, envp)              array + PATH + env (GNU)
  execve(path, argv, envp)               la syscall real

  NOMENCLATURA
  ──────────────────────────────────────────────────────────
  l = list (argumentos variadic, terminar con NULL)
  v = vector (argumentos en char *argv[])
  p = path (busca en $PATH)
  e = environ (entorno personalizado)

  CUÁNDO USAR CUÁL
  ──────────────────────────────────────────────────────────
  execvp     la más común: args dinámicos + busca en PATH
  execlp     args conocidos en compilación + busca en PATH
  execve     control total (seguridad, sandbox)
  execv      args dinámicos + ruta exacta (seguridad)

  ARGV
  ──────────────────────────────────────────────────────────
  argv[0] = nombre del programa (convención)
  argv debe terminar en NULL
  El cast (char *)NULL es necesario en variantes l

  RETORNO
  ──────────────────────────────────────────────────────────
  Éxito: NUNCA retorna (el programa fue reemplazado)
  Error: retorna -1, errno indica la causa

  QUÉ PRESERVA EXEC
  ──────────────────────────────────────────────────────────
  PID, PPID, UID/GID, CWD, umask, nice
  fds SIN O_CLOEXEC
  SIG_IGN y SIG_DFL, máscara de señales
  Locks fcntl, alarmas, límites de recursos

  QUÉ DESCARTA EXEC
  ──────────────────────────────────────────────────────────
  Código, datos, heap, stack (reemplazados)
  fds CON O_CLOEXEC (cerrados)
  Signal handlers (reseteados a SIG_DFL)
  Mapeos mmap, threads, timers

  ERRORES COMUNES
  ──────────────────────────────────────────────────────────
  ENOENT    archivo no existe
  EACCES    sin permiso de ejecución
  ENOEXEC   no es ejecutable (sin shebang)
  E2BIG     args demasiado grandes

  EXIT CODES (CONVENCIÓN)
  ──────────────────────────────────────────────────────────
  126       no ejecutable
  127       no encontrado
  128+N     terminado por señal N

  FORK + EXEC PATTERN
  ──────────────────────────────────────────────────────────
  pid = fork();
  if (pid == -1) { perror("fork"); return -1; }
  if (pid == 0) {
      // operaciones pre-exec (close, dup2, chdir...)
      execvp(program, argv);
      perror("exec");
      _exit(127);
  }
  waitpid(pid, &status, 0);
```

---

## 12. Ejercicios

### Ejercicio 1: mini-shell con exec

Escribe un programa que lea comandos del usuario y los ejecute usando
fork + exec:

```c
// Esqueleto:
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

#define MAX_ARGS 64

int main(void) {
    char line[1024];

    while (1) {
        printf("$ ");
        if (!fgets(line, sizeof(line), stdin)) break;

        // Eliminar newline
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;
        if (strcmp(line, "exit") == 0) break;

        // Parsear en tokens
        char *argv[MAX_ARGS];
        int argc = 0;
        // ... tokenizar line en argv ...

        // fork + exec
        pid_t pid = fork();
        if (pid == 0) {
            execvp(argv[0], argv);
            perror(argv[0]);
            _exit(127);
        }

        int status;
        waitpid(pid, &status, 0);
        // Imprimir exit code si no es 0
    }
    return 0;
}
```

Requisitos:
- Parsear el comando en tokens separados por espacios.
- Usar `execvp` para buscar el programa en PATH.
- Imprimir el exit code si el comando falla.
- Implementar el comando built-in `exit`.

**Pregunta de reflexión**: ¿por qué `cd` no puede implementarse con
fork+exec? ¿Qué pasaría si hicieras `fork()`, `chdir()` en el hijo y
luego `_exit()`? ¿Qué otros comandos deben ser "built-in" en un shell
y no pueden delegarse a un proceso externo?

---

### Ejercicio 2: ejecutar con entorno limpio

Escribe un programa `cleanexec` que ejecute un comando con un entorno
mínimo y controlado:

```bash
# Uso: ./cleanexec [opciones] programa [args...]
# -e VAR=valor   agregar variable al entorno
# -p PATH        establecer PATH (default: /usr/bin:/bin)

$ ./cleanexec env
PATH=/usr/bin:/bin
HOME=/root

$ ./cleanexec -e MY_VAR=hello -e DEBUG=1 env
PATH=/usr/bin:/bin
HOME=/root
MY_VAR=hello
DEBUG=1
```

```c
// Esqueleto:
// 1. Parsear opciones -e y -p
// 2. Construir envp[] con variables base + las -e
// 3. fork + execve con el entorno construido
```

Requisitos:
- Entorno base: solo `PATH` y `HOME`.
- `-e KEY=VALUE` agrega variables adicionales.
- `-p PATH` sobreescribe el PATH default.
- Usar `execve` (no `execvp`, para usar el entorno personalizado).
- Máximo 64 variables de entorno.

**Pregunta de reflexión**: ¿por qué los programas setuid deberían
limpiar el entorno antes de ejecutar otros programas? ¿Qué variables
de entorno podrían ser peligrosas si un usuario las manipula?
(Piensa en `LD_PRELOAD`, `LD_LIBRARY_PATH`, `IFS`, `PATH`.)

---

### Ejercicio 3: comparar todas las variantes de exec

Escribe un programa que ejecute el mismo comando (`/bin/echo "Hello"`)
usando cada una de las 6 variantes de exec, demostrando la sintaxis
de cada una:

```c
// Esqueleto:
void test_execl(void);
void test_execlp(void);
void test_execle(void);
void test_execv(void);
void test_execvp(void);
void test_execve(void);

// Cada función:
// 1. fork()
// 2. En el hijo: llamar a la variante correspondiente
// 3. En el padre: wait, imprimir qué variante se usó y el resultado

int main(void) {
    test_execl();
    test_execlp();
    test_execle();
    test_execv();
    test_execvp();
    test_execve();
    return 0;
}
```

Requisitos:
- Cada función imprime la firma exacta de la llamada a exec antes
  de ejecutarla.
- Para las variantes `e`, pasar un entorno con una variable extra
  `EXEC_VARIANT=nombre` y verificar que el programa la recibe
  (usando `/usr/bin/env` en lugar de `echo`).
- Probar `execlp` con un nombre sin ruta (`echo`) vs `execl` con
  ruta completa (`/bin/echo`).

**Pregunta de reflexión**: si `execve` es la única syscall real y
todas las demás son wrappers de glibc, ¿por qué existen tantas
variantes? ¿No sería suficiente con `execve`? Piensa en la ergonomía
del código: ¿cuántas líneas necesitas para llamar a `execve` vs
`execlp` para un caso simple como ejecutar `ls -l`?

---
