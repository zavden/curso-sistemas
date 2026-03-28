# fork() — Duplicación de Procesos

## Índice

1. [Qué es fork y el modelo de procesos Unix](#1-qué-es-fork-y-el-modelo-de-procesos-unix)
2. [La syscall fork: firma y semántica](#2-la-syscall-fork-firma-y-semántica)
3. [Valor de retorno: distinguir padre e hijo](#3-valor-de-retorno-distinguir-padre-e-hijo)
4. [Qué se copia y qué se comparte](#4-qué-se-copia-y-qué-se-comparte)
5. [Copy-on-write: la optimización clave](#5-copy-on-write-la-optimización-clave)
6. [PIDs: getpid y getppid](#6-pids-getpid-y-getppid)
7. [Orden de ejecución padre vs hijo](#7-orden-de-ejecución-padre-vs-hijo)
8. [Herencia de file descriptors](#8-herencia-de-file-descriptors)
9. [fork en la práctica](#9-fork-en-la-práctica)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es fork y el modelo de procesos Unix

En Unix, la **única** forma de crear un proceso nuevo es `fork()`.
No existe una syscall "crear proceso con este programa" — en su lugar,
el modelo es:

```
  1. fork()  → duplicar el proceso actual
  2. exec()  → reemplazar el programa del hijo (opcional)

  Proceso padre (PID 100)
       │
       │ fork()
       │
       ├──────────────────┐
       │                  │
       ▼                  ▼
  Padre (PID 100)    Hijo (PID 101)
  continúa su        copia exacta del padre
  código              (mismo código, mismos datos)
                      │
                      │ exec() (opcional)
                      ▼
                  Nuevo programa
```

Este modelo de "copiar primero, reemplazar después" es el fundamento
de cómo funcionan los shells, servidores, daemons y toda la gestión
de procesos en Unix/Linux.

### Cada proceso tiene un padre

```
  PID 1 (init/systemd)
  ├── PID 500 (sshd)
  │   └── PID 1200 (bash)
  │       ├── PID 1300 (vim)
  │       └── PID 1301 (gcc)
  ├── PID 600 (nginx master)
  │   ├── PID 601 (nginx worker)
  │   └── PID 602 (nginx worker)
  └── PID 700 (cron)
```

Todos los procesos descienden de PID 1. Cuando un padre muere, sus
hijos son "adoptados" por PID 1 (o por el subreaper más cercano).

---

## 2. La syscall fork: firma y semántica

```c
#include <unistd.h>

pid_t fork(void);
```

`fork` no recibe argumentos. Crea un **nuevo proceso** que es una copia
casi exacta del proceso que la invocó. Después de `fork`, hay dos
procesos ejecutando el mismo código a partir de la misma instrucción.

```c
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("Antes de fork (un solo proceso)\n");

    pid_t pid = fork();
    // A partir de aquí, hay DOS procesos ejecutando

    printf("Después de fork (pid=%d)\n", pid);
    return 0;
}
```

Output:
```
Antes de fork (un solo proceso)
Después de fork (pid=1234)
Después de fork (pid=0)
```

El `printf` después de `fork` se ejecuta **dos veces**: una en el
padre y una en el hijo. La diferencia es el valor de `pid`.

---

## 3. Valor de retorno: distinguir padre e hijo

`fork` retorna **diferente valor** en cada proceso:

```
  Proceso               fork() retorna
  ──────────────────────────────────────────────────────
  Padre                 PID del hijo (> 0)
  Hijo                  0
  Error                 -1 (solo en el padre, el hijo
                        no se crea)
```

### Patrón estándar

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(void) {
    pid_t pid = fork();

    if (pid == -1) {
        // Error: no se pudo crear el proceso
        perror("fork");
        exit(1);
    }

    if (pid == 0) {
        // === CÓDIGO DEL HIJO ===
        printf("Soy el hijo, mi PID es %d\n", getpid());
        printf("Mi padre es PID %d\n", getppid());
        exit(0);  // el hijo termina aquí
    }

    // === CÓDIGO DEL PADRE ===
    // (pid contiene el PID del hijo)
    printf("Soy el padre (PID %d), mi hijo es PID %d\n",
           getpid(), pid);

    return 0;
}
```

### Diagrama de flujo

```
  pid_t pid = fork();
       │
       ├── pid == -1 ──► ERROR (no se creó el hijo)
       │
       ├── pid == 0  ──► HIJO (código del proceso hijo)
       │
       └── pid > 0   ──► PADRE (pid = PID del hijo)
```

### ¿Por qué el hijo recibe 0?

El hijo no necesita saber su propio PID a través de `fork` — puede
llamar a `getpid()`. Lo que necesita saber es "¿soy el hijo?", y
0 sirve como señal clara. El padre, en cambio, necesita el PID del
hijo para poder esperarlo (`wait`) o enviarle señales (`kill`).

---

## 4. Qué se copia y qué se comparte

### Se COPIA (independiente en el hijo)

```
  Recurso                   Detalle
  ──────────────────────────────────────────────────────
  Espacio de memoria        heap, stack, datos globales
                            (via copy-on-write)
  File descriptors          tabla de fds (mismos números,
                            apuntan a las mismas open file
                            descriptions)
  PIDs                      el hijo tiene su propio PID
  PID del padre (ppid)      en el hijo, ppid = PID del padre
  Señales pendientes        el hijo NO hereda señales pendientes
  Máscara de señales        se copia la máscara actual
  Valor de retorno de fork  diferente en padre e hijo
  Locks de memoria (mlock)  NO se heredan
  Timers (timer_create)     NO se heredan
  Operaciones de I/O async  NO se heredan
```

### Se COMPARTE (misma referencia)

```
  Recurso                   Detalle
  ──────────────────────────────────────────────────────
  Open file descriptions    el offset de archivo se comparte
                            (un seek en el hijo afecta al padre)
  Memoria mapeada           MAP_SHARED sigue siendo compartida
  (MAP_SHARED)
  Segmentos System V shm    siguen adjuntados
  IDs de sesión y grupo     se heredan
  Terminal de control       misma terminal
  Directorio de trabajo     se copia (no se comparte)
  Umask                     se copia
  Variables de entorno      se copian
  Nice value                se copia
  UID/GID (real y efectivo) se copian
```

### Diagrama

```
  Padre (PID 100)              Hijo (PID 101)
  ┌──────────────────┐         ┌──────────────────┐
  │ Stack (copia)    │         │ Stack (copia)     │
  │ Heap (CoW)       │         │ Heap (CoW)        │
  │ Datos globales   │         │ Datos globales    │
  │                  │         │                   │
  │ fd 0 ──┐         │         │ fd 0 ──┐          │
  │ fd 1 ──┤         │         │ fd 1 ──┤          │
  │ fd 2 ──┤         │         │ fd 2 ──┤          │
  │ fd 3 ──┤         │         │ fd 3 ──┤          │
  └────────┼─────────┘         └────────┼──────────┘
           │                            │
           └────────────┬───────────────┘
                        ▼
              ┌──────────────────┐
              │ Open file        │
              │ descriptions     │
              │ (compartidas)    │
              │ offset compartido│
              └──────────────────┘
```

---

## 5. Copy-on-write: la optimización clave

`fork` no copia toda la memoria del padre inmediatamente. Usa
**copy-on-write (CoW)**: las páginas se comparten como read-only
y solo se copian cuando uno de los procesos intenta escribir:

```
  Después de fork() (antes de escribir):
  ┌───────────┐  ┌───────────┐
  │  Padre    │  │   Hijo    │
  │ Página A ─┼──┼─ Página A │  ← misma página física
  │ Página B ─┼──┼─ Página B │     marcada read-only
  │ Página C ─┼──┼─ Página C │
  └───────────┘  └───────────┘

  Padre escribe en Página B:
  ┌───────────┐  ┌───────────┐
  │  Padre    │  │   Hijo    │
  │ Página A ─┼──┼─ Página A │  ← sigue compartida
  │ Página B' │  │ Página B ─┼──── original (read-only)
  │  (copia)  │  │           │
  │ Página C ─┼──┼─ Página C │  ← sigue compartida
  └───────────┘  └───────────┘
```

### Por qué CoW importa

```
  Sin CoW:
  fork() de proceso con 500 MiB de heap
  → Copiar 500 MiB inmediatamente (~100 ms)
  → Si el hijo solo hace exec(), 500 MiB copiados para nada

  Con CoW:
  fork() de proceso con 500 MiB de heap
  → Copiar solo las tablas de página (~1 ms)
  → Las páginas se copian bajo demanda, una a la vez
  → Si el hijo hace exec(), casi nada se copió realmente
```

### fork de un proceso grande

```c
// Proceso con 1 GiB de datos en memoria
char *big_data = malloc(1024 * 1024 * 1024);
memset(big_data, 'A', 1024 * 1024 * 1024);

pid_t pid = fork();
// fork() es rápido gracias a CoW
// No se copiaron 1 GiB — solo las page tables

if (pid == 0) {
    // Si el hijo solo lee big_data: sin copias
    printf("byte 0 = %c\n", big_data[0]);  // no copia

    // Si el hijo escribe: solo se copia ESA página (4 KiB)
    big_data[0] = 'B';  // page fault → copia 1 página

    exec(...);  // toda la memoria se descarta al hacer exec
}
```

### vfork: la alternativa pre-CoW (obsoleta)

Antes de CoW, `vfork` existía como optimización: el hijo comparte
**toda** la memoria con el padre y el padre se suspende hasta que
el hijo hace `exec` o `_exit`:

```c
pid_t pid = vfork();
if (pid == 0) {
    // ¡NO tocar variables! Comparte memoria con el padre
    // Solo exec() o _exit() son seguros
    execvp(program, args);
    _exit(127);
}
```

**No uses `vfork` en código nuevo**. CoW hace que `fork` sea casi
igual de rápido sin los peligros de compartir memoria.

### posix_spawn: alternativa moderna

```c
#include <spawn.h>

pid_t child;
int ret = posix_spawn(&child, "/bin/ls",
                       NULL,    // file_actions
                       NULL,    // attrp
                       argv,    // argv
                       environ); // envp
if (ret != 0) {
    fprintf(stderr, "posix_spawn: %s\n", strerror(ret));
}
```

`posix_spawn` combina fork+exec en una sola operación. Es más eficiente
en sistemas sin CoW y más portable (POSIX). Pero carece de la
flexibilidad de hacer operaciones entre fork y exec.

---

## 6. PIDs: getpid y getppid

```c
#include <unistd.h>

pid_t getpid(void);   // PID de este proceso
pid_t getppid(void);  // PID del padre
```

### Ejemplo

```c
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("Mi PID: %d, padre: %d\n", getpid(), getppid());

    pid_t pid = fork();
    if (pid == 0) {
        printf("Hijo — PID: %d, padre: %d\n",
               getpid(), getppid());
        return 0;
    }

    printf("Padre — PID: %d, hijo: %d\n", getpid(), pid);
    return 0;
}
```

Output posible:
```
Mi PID: 1000, padre: 999
Padre — PID: 1000, hijo: 1001
Hijo — PID: 1001, padre: 1000
```

### Adopción por init (PID 1)

Si el padre termina antes que el hijo, el hijo es **adoptado** por
PID 1 (systemd/init):

```c
pid_t pid = fork();
if (pid == 0) {
    sleep(5);
    // Si el padre ya terminó:
    printf("Mi nuevo padre: %d\n", getppid());
    // Imprime 1 (adoptado por init)
    return 0;
}
// Padre termina inmediatamente
return 0;
```

### Subreaper

Desde Linux 3.4, un proceso puede declararse como "subreaper":
los huérfanos dentro de su subárbol son adoptados por él en vez
de por PID 1:

```c
#include <sys/prctl.h>

// Este proceso adopta huérfanos de sus descendientes
prctl(PR_SET_CHILD_SUBREAPER, 1);
```

systemd usa esto para rastrear procesos de servicios: si un daemon
hace doble fork, el proceso resultante sigue siendo hijo del subreaper
de systemd.

---

## 7. Orden de ejecución padre vs hijo

Después de `fork`, el kernel puede ejecutar primero al padre o al hijo.
**No hay garantía de orden**:

```c
pid_t pid = fork();
if (pid == 0) {
    printf("HIJO primero\n");
} else {
    printf("PADRE primero\n");
}
// ¿Quién imprime primero? NO LO SABES
```

### No depender del orden

```c
// MAL: asumir que el padre ejecuta primero
pid_t pid = fork();
if (pid > 0) {
    shared_resource = 42;  // ¿se ejecuta antes que el hijo?
}
if (pid == 0) {
    printf("%d\n", shared_resource);  // ¿42 o 0?
    // CoW: cada uno tiene su copia, pero ¿cuándo?
}

// BIEN: usar mecanismos de sincronización explícitos
// wait(), pipes, señales, semáforos
```

### Scheduler policy

Históricamente, el kernel Linux solía ejecutar al hijo primero (para
aprovechar CoW antes de exec), pero esto cambió varias veces entre
versiones del kernel. Tu código **nunca debe depender** de este
comportamiento.

---

## 8. Herencia de file descriptors

Los file descriptors se **copian** en el hijo, pero apuntan a las
mismas **open file descriptions** en el kernel:

```
  Antes de fork:
  Padre: fd 3 → open file description (offset=0)

  Después de fork:
  Padre: fd 3 ─┐
                ├──► open file description (offset=0)
  Hijo:  fd 3 ─┘

  Si el hijo hace read(fd, buf, 100):
  → offset avanza a 100 para AMBOS
  Padre: fd 3 ─┐
                ├──► open file description (offset=100)
  Hijo:  fd 3 ─┘
```

### Implicación: offset compartido

```c
int fd = open("data.txt", O_RDONLY);

pid_t pid = fork();
if (pid == 0) {
    char buf[10];
    read(fd, buf, 10);  // lee bytes 0-9, offset → 10
    close(fd);
    _exit(0);
}

wait(NULL);  // esperar al hijo

char buf[10];
read(fd, buf, 10);  // lee bytes 10-19 (offset compartido)
close(fd);
```

### O_CLOEXEC y fork

`O_CLOEXEC` NO afecta a `fork` — solo a `exec`. Los fds se heredan
siempre en `fork`, tengan o no `O_CLOEXEC`:

```
  fd con O_CLOEXEC:
  fork()  → hijo hereda el fd ✓
  exec()  → fd se cierra automáticamente ✓

  fd sin O_CLOEXEC:
  fork()  → hijo hereda el fd ✓
  exec()  → fd sigue abierto (puede leakear)
```

### Patrón: cerrar fds innecesarios en el hijo

```c
pid_t pid = fork();
if (pid == 0) {
    // El hijo no necesita el fd del servidor
    close(server_fd);

    // Hacer trabajo del hijo...
    _exit(0);
}
// El padre no necesita el fd del cliente
close(client_fd);
```

### Pipes y fork

Los pipes son el mecanismo fundamental de IPC entre padre e hijo:

```c
int pipefd[2];
pipe(pipefd);  // pipefd[0]=lectura, pipefd[1]=escritura

pid_t pid = fork();
if (pid == 0) {
    // Hijo: leer del pipe
    close(pipefd[1]);  // cerrar extremo de escritura
    char buf[256];
    ssize_t n = read(pipefd[0], buf, sizeof(buf));
    write(STDOUT_FILENO, buf, n);
    close(pipefd[0]);
    _exit(0);
}

// Padre: escribir al pipe
close(pipefd[0]);  // cerrar extremo de lectura
const char *msg = "Hola desde el padre\n";
write(pipefd[1], msg, strlen(msg));
close(pipefd[1]);

wait(NULL);
```

```
  Después de fork:
  Padre                          Hijo
  pipefd[0] (lectura)  ──┐ ┌── pipefd[0] (lectura)
                          │ │
  pipefd[1] (escritura) ──┤ ├── pipefd[1] (escritura)
                          │ │
  Cerrar: pipefd[0]      │ │   Cerrar: pipefd[1]
                          │ │
  Padre escribe ──────────┘ └── Hijo lee
  en pipefd[1]                  de pipefd[0]
```

---

## 9. fork en la práctica

### Crear múltiples hijos

```c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

#define NUM_CHILDREN 4

int main(void) {
    pid_t children[NUM_CHILDREN];

    for (int i = 0; i < NUM_CHILDREN; i++) {
        pid_t pid = fork();

        if (pid == -1) {
            perror("fork");
            exit(1);
        }

        if (pid == 0) {
            // Hijo: hacer trabajo y salir
            printf("Hijo %d (PID %d) trabajando...\n",
                   i, getpid());
            sleep(1);  // simular trabajo
            printf("Hijo %d terminó\n", i);
            _exit(i);  // exit code = número del hijo
        }

        children[i] = pid;
    }

    // Padre: esperar a todos los hijos
    for (int i = 0; i < NUM_CHILDREN; i++) {
        int status;
        waitpid(children[i], &status, 0);
        if (WIFEXITED(status)) {
            printf("Hijo PID %d terminó con código %d\n",
                   children[i], WEXITSTATUS(status));
        }
    }

    return 0;
}
```

### Error clásico: fork bomb

```c
// ¡NUNCA ejecutar esto!
while (1) {
    fork();
}
// Cada proceso crea un hijo, que crea otro, exponencialmente
// Colapsa el sistema en segundos
```

```bash
# Equivalente en shell (la fork bomb clásica):
:(){ :|:& };:

# Protección: límite de procesos por usuario
$ ulimit -u
# Típicamente 4096 o similar
```

### _exit vs exit en el hijo

```c
pid_t pid = fork();
if (pid == 0) {
    // USAR _exit() en el hijo (no exit())
    _exit(0);
}
```

¿Por qué `_exit` y no `exit`?

```
  exit():
  1. Ejecuta handlers de atexit()
  2. Flushea buffers de stdio (printf, fwrite)
  3. Llama a _exit()

  _exit():
  1. Termina inmediatamente (syscall directa)

  Problema con exit() en el hijo:
  - Los buffers de stdio se heredan del padre
  - exit() los flushea → datos duplicados en la salida
  - Los handlers de atexit() del padre se ejecutan en el hijo
    (borrar archivos temporales del padre, por ejemplo)
```

```c
// Ejemplo del problema:
printf("Antes de fork");  // SIN \n → queda en buffer

pid_t pid = fork();
if (pid == 0) {
    exit(0);  // flushea "Antes de fork" OTRA VEZ
}
wait(NULL);
// Output: "Antes de forkAntes de fork"

// Solución 1: fflush antes de fork
fflush(stdout);
fork();

// Solución 2: _exit en el hijo
if (pid == 0) {
    _exit(0);  // no flushea buffers
}
```

---

## 10. Errores comunes

### Error 1: no verificar el retorno de fork

```c
// MAL: asumir que fork siempre funciona
pid_t pid = fork();
if (pid == 0) {
    // ...
}
// Si fork falló, pid == -1 y este código se ejecuta
// pensando que es el padre con hijo PID -1

// BIEN: verificar error
pid_t pid = fork();
if (pid == -1) {
    perror("fork");
    return -1;
}
if (pid == 0) {
    // hijo
}
```

`fork` puede fallar si:
- Se alcanzó el límite de procesos (`EAGAIN`).
- No hay memoria disponible (`ENOMEM`).

### Error 2: fork en un loop sin control

```c
// MAL: crear hijos que a su vez crean hijos
for (int i = 0; i < 10; i++) {
    fork();
    // Iteración 0: 1 proceso → 2
    // Iteración 1: 2 procesos → 4
    // Iteración 2: 4 procesos → 8
    // ...
    // Iteración 9: 512 → 1024 procesos
}

// BIEN: solo el padre hace fork
for (int i = 0; i < 10; i++) {
    pid_t pid = fork();
    if (pid == 0) {
        // Hijo: hacer trabajo y SALIR
        do_work(i);
        _exit(0);  // ← crucial: el hijo no sigue el loop
    }
}
```

### Error 3: no esperar a los hijos (crear zombies)

```c
// MAL: padre no hace wait → hijos se vuelven zombies
for (int i = 0; i < 5; i++) {
    pid_t pid = fork();
    if (pid == 0) {
        _exit(0);  // hijo termina
    }
    // Padre no hace wait → zombie
}
sleep(60);
// Durante estos 60 segundos, hay 5 zombies en la tabla de procesos

// BIEN: esperar a cada hijo (ver T03 para detalles)
```

### Error 4: usar exit() en lugar de _exit() en el hijo

```c
// MAL: exit() puede causar efectos secundarios
if (fork() == 0) {
    // No hacer exec()...
    exit(0);  // flushea buffers, ejecuta atexit handlers
}

// BIEN:
if (fork() == 0) {
    _exit(0);
}

// TAMBIÉN BIEN: si el hijo hace exec(), exit() es OK
// porque exec() reemplaza todo incluyendo handlers
```

### Error 5: asumir que las variables se comparten entre padre e hijo

```c
// MAL: esperar que el padre vea el cambio del hijo
int counter = 0;

pid_t pid = fork();
if (pid == 0) {
    counter = 42;  // modifica la COPIA del hijo
    _exit(0);
}
wait(NULL);
printf("counter = %d\n", counter);  // imprime 0, NO 42

// BIEN: usar memoria compartida (mmap MAP_SHARED)
// o comunicar vía pipes
```

---

## 11. Cheatsheet

```
  CREAR PROCESO
  ──────────────────────────────────────────────────────────
  pid_t pid = fork();
  → padre recibe PID del hijo (> 0)
  → hijo recibe 0
  → error: -1

  PATRÓN ESTÁNDAR
  ──────────────────────────────────────────────────────────
  pid_t pid = fork();
  if (pid == -1) { perror("fork"); exit(1); }
  if (pid == 0) {
      // hijo
      _exit(0);
  }
  // padre (pid = PID del hijo)

  PIDs
  ──────────────────────────────────────────────────────────
  getpid()     PID propio
  getppid()    PID del padre
  El hijo huérfano es adoptado por PID 1 (o subreaper)

  QUÉ SE HEREDA
  ──────────────────────────────────────────────────────────
  Memoria         CoW (copia al escribir)
  File descriptors  se copian (comparten open file description)
  Offsets           compartidos entre padre e hijo
  Señales           máscara se copia, pendientes NO
  UID/GID           se copian
  CWD, umask        se copian
  Env vars          se copian

  QUÉ NO SE HEREDA
  ──────────────────────────────────────────────────────────
  PID             el hijo tiene PID nuevo
  Locks (flock)   compartidos (misma OFD)
  Locks (fcntl)   NO se heredan
  Timers          NO se heredan
  I/O async       NO se hereda
  Señales pendientes  NO se heredan

  COPY-ON-WRITE
  ──────────────────────────────────────────────────────────
  fork() NO copia la memoria inmediatamente
  Páginas compartidas read-only hasta que alguien escribe
  Escribir → page fault → kernel copia esa página
  fork() de proceso de 1 GiB toma ~1 ms (no ~100 ms)

  _exit() vs exit()
  ──────────────────────────────────────────────────────────
  Hijo sin exec: usar _exit() (no flushea buffers)
  Hijo con exec: exit() OK (exec reemplaza todo)
  fflush(stdout) antes de fork evita duplicados

  MÚLTIPLES HIJOS
  ──────────────────────────────────────────────────────────
  for (i = 0; i < N; i++) {
      pid = fork();
      if (pid == 0) {
          do_work(i);
          _exit(0);    // ← CRUCIAL: hijo sale del loop
      }
      children[i] = pid;
  }
  // Padre: wait para cada hijo

  PELIGROS
  ──────────────────────────────────────────────────────────
  Fork bomb: while(1) fork(); → colapsa el sistema
  Zombies: hijo termina sin wait del padre
  Datos "compartidos": las variables son copias (CoW)
  Buffers duplicados: fflush antes de fork
```

---

## 12. Ejercicios

### Ejercicio 1: árbol de procesos

Escribe un programa que cree un árbol de procesos con la siguiente
estructura y lo verifique con `pstree`:

```
  Padre (P)
  ├── Hijo 1 (H1)
  │   ├── Nieto 1 (N1)
  │   └── Nieto 2 (N2)
  └── Hijo 2 (H2)
      └── Nieto 3 (N3)
```

```c
// Esqueleto:
#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    printf("Padre PID=%d\n", getpid());

    // Crear Hijo 1
    pid_t h1 = fork();
    if (h1 == 0) {
        printf("  Hijo 1 PID=%d, padre=%d\n",
               getpid(), getppid());
        // Crear Nieto 1 y Nieto 2
        // ...
        // Esperar a los nietos
        // _exit
    }

    // Crear Hijo 2
    // ...

    // Esperar a ambos hijos
    // ...
}
```

Requisitos:
- Cada proceso imprime su PID, el PID de su padre y su "nombre" (P, H1, N1, etc.).
- Verificar con `pstree -p PID_DEL_PADRE` en otra terminal.
- Cada proceso espera a sus hijos antes de terminar.
- Usar `sleep(30)` al final de cada proceso para poder observar el
  árbol con `pstree`.

**Pregunta de reflexión**: si el Hijo 1 termina antes que sus nietos
(N1 y N2), ¿qué pasa con ellos? ¿Quién se convierte en su padre?
¿Cómo puedes verificarlo con `getppid()` dentro de N1 y N2?

---

### Ejercicio 2: fork y file descriptors compartidos

Escribe un programa que demuestre que padre e hijo comparten el offset
de un file descriptor:

```c
// Esqueleto:
// 1. Crear archivo "test.txt" con contenido "ABCDEFGHIJ"
// 2. Abrir el archivo para lectura
// 3. fork()
// 4. Hijo: leer 3 bytes, imprimir, _exit
// 5. Padre: wait(), leer 3 bytes, imprimir
// 6. Verificar: el padre lee DONDE EL HIJO DEJÓ
```

Requisitos:
- Demostrar que el padre lee a partir del offset donde el hijo paró.
- Repetir el experimento abriendo el archivo **después** de fork
  (cada proceso abre su propio fd). ¿El offset se comparte?
- Escribir un tercer caso: hijo y padre escriben alternadamente a un
  archivo compartido (abrir antes de fork) y verificar que los datos
  no se superponen.

**Pregunta de reflexión**: si dos procesos abren el mismo archivo
independientemente (cada uno con su `open`), ¿comparten el offset?
¿Por qué la respuesta es diferente al caso de fd heredado por fork?
Dibuja el diagrama de fd table → open file description → inode para
ambos casos.

---

### Ejercicio 3: pool de workers con fork

Implementa un pool de N procesos worker que procesan tareas de una
cola. El padre distribuye tareas y recolecta resultados:

```c
// Esqueleto:
#define NUM_WORKERS 4

int main(void) {
    // Crear pipes de comunicación: padre→hijo y hijo→padre
    // para cada worker

    for (int i = 0; i < NUM_WORKERS; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            // Worker: leer tarea del pipe, procesarla,
            // escribir resultado al pipe de retorno
            worker_loop(i, task_pipe_read, result_pipe_write);
            _exit(0);
        }
    }

    // Padre: enviar tareas (ej: números para calcular factorial)
    // Leer resultados
    // Cuando no hay más tareas, cerrar pipes
    // Wait para todos los workers
}
```

Requisitos:
- Cada worker lee un entero del pipe, calcula su factorial y escribe
  el resultado.
- El padre distribuye 20 tareas entre los 4 workers.
- Usar `poll()` para leer resultados de múltiples pipes.
- Cerrar pipes correctamente para que los workers terminen.

**Pregunta de reflexión**: ¿por qué necesitas DOS pipes por worker
(uno para enviar tareas, otro para recibir resultados) en lugar de
uno solo? ¿Qué pasaría si padre e hijo leen y escriben al mismo pipe?
¿Cómo se relaciona esto con el patrón de pipes que usa el shell para
`cmd1 | cmd2`?

---
