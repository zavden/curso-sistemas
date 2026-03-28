# Qué es una Syscall

## Índice

1. [El problema: por qué el programa no puede hacerlo solo](#1-el-problema-por-qué-el-programa-no-puede-hacerlo-solo)
2. [User space vs kernel space](#2-user-space-vs-kernel-space)
3. [Anatomía de una syscall: qué ocurre paso a paso](#3-anatomía-de-una-syscall-qué-ocurre-paso-a-paso)
4. [La tabla de syscalls](#4-la-tabla-de-syscalls)
5. [Wrappers de glibc: la capa que usas sin saberlo](#5-wrappers-de-glibc-la-capa-que-usas-sin-saberlo)
6. [Invocar syscalls directamente: syscall()](#6-invocar-syscalls-directamente-syscall)
7. [Syscalls sin wrapper: syscall() como último recurso](#7-syscalls-sin-wrapper-syscall-como-último-recurso)
8. [Categorías de syscalls](#8-categorías-de-syscalls)
9. [El costo de una syscall](#9-el-costo-de-una-syscall)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. El problema: por qué el programa no puede hacerlo solo

Cuando escribes un programa en C que lee un archivo, no accedes al disco directamente. Cuando imprimes en pantalla, no hablas con la tarjeta de video. Cuando abres un socket de red, no tocas la tarjeta de red. Todo eso lo hace el **kernel** — el programa solo le pide que lo haga.

Esta petición es una **syscall** (system call / llamada al sistema).

```c
#include <stdio.h>

int main(void) {
    printf("hello\n");   // parece simple, pero internamente...
    return 0;
}
```

Lo que realmente ocurre:

```
 Tu programa                   glibc                    Kernel
┌──────────────┐         ┌──────────────┐        ┌──────────────────┐
│ printf("hello│         │              │        │                  │
│ \n")         │────────►│ Formatea el  │        │                  │
│              │         │ string en un │        │                  │
│              │         │ buffer       │        │                  │
│              │         │      │       │        │                  │
│              │         │      ▼       │        │                  │
│              │         │ write(1,     │───────►│ Copia datos al   │
│              │         │  "hello\n",  │ syscall│ buffer del TTY   │
│              │         │  6)          │        │ Programa el      │
│              │         │              │◄───────│ dispositivo       │
│              │         │              │ return │                  │
└──────────────┘         └──────────────┘        └──────────────────┘
  User space               User space              Kernel space
```

`printf` no es una syscall — es una función de biblioteca que eventualmente invoca la syscall `write`. La distinción entre "función de biblioteca" y "syscall" es fundamental en este bloque.

---

## 2. User space vs kernel space

Linux divide la memoria y los privilegios de ejecución en dos dominios:

### Los anillos de protección (x86)

```
 Ring 3 (User space)              Ring 0 (Kernel space)
┌─────────────────────────┐    ┌─────────────────────────┐
│                         │    │                         │
│ • Tu programa           │    │ • Acceso al hardware    │
│ • Bibliotecas (glibc)   │    │ • Gestión de memoria    │
│ • Shell, editores       │    │ • Scheduler de procesos │
│                         │    │ • Filesystem            │
│ NO puede:               │    │ • Drivers               │
│ • Tocar hardware        │    │ • Stack de red          │
│ • Acceder memoria de    │    │                         │
│   otros procesos        │    │ PUEDE todo              │
│ • Ejecutar instrucciones│    │                         │
│   privilegiadas (IN/OUT,│    │                         │
│   HLT, MOV CR3, etc.)  │    │                         │
│                         │    │                         │
└────────────┬────────────┘    └────────────┬────────────┘
             │                              │
             │         SYSCALL              │
             └──────────────────────────────┘
             La ÚNICA forma legítima de cruzar
```

El procesador x86 tiene 4 anillos de protección (0–3), pero Linux solo usa dos: ring 0 (kernel) y ring 3 (usuario). Cuando un programa en ring 3 intenta ejecutar una instrucción privilegiada, el procesador genera una **excepción** — el programa se mata con SIGSEGV o SIGILL.

### Por qué existe esta separación

Sin separación, cualquier programa podría:
- Leer la memoria de otros procesos (contraseñas, claves de cifrado).
- Escribir directamente en disco, corrompiendo datos de otros usuarios.
- Apagar el sistema (`HLT`).
- Reprogramar la tabla de interrupciones y tomar control total.

La separación no es una convención — está **enforced por hardware** (MMU + anillos de protección del CPU).

### Qué implica para el programador de sistemas

Cada operación que toque algo fuera de tu espacio de memoria **requiere una syscall**:

| Operación | Syscall |
|-----------|---------|
| Leer/escribir archivos | `read`, `write`, `open`, `close` |
| Crear procesos | `fork`, `clone`, `execve` |
| Enviar/recibir por red | `sendto`, `recvfrom`, `socket` |
| Reservar memoria | `mmap`, `brk` |
| Obtener la hora | `clock_gettime`, `gettimeofday` |
| Dormir | `nanosleep`, `clock_nanosleep` |
| Terminar el programa | `exit_group` |

Operaciones que NO necesitan syscall:
- Aritmética (`a + b`).
- Acceder a memoria que ya te pertenece (`array[i]`).
- Llamar a funciones puras de biblioteca (`strlen`, `memcpy`, `qsort`).

---

## 3. Anatomía de una syscall: qué ocurre paso a paso

Cuando tu programa ejecuta `write(1, "hello\n", 6)`, estos son los pasos exactos:

### En x86-64 Linux (la arquitectura de tu sistema)

```
 1. glibc pone los argumentos en registros:
    RAX = 1      (número de syscall: SYS_write)
    RDI = 1      (fd: stdout)
    RSI = &"hello\n"  (buffer)
    RDX = 6      (count)

 2. Ejecuta la instrucción SYSCALL
    (instrucción de CPU que transfiere control al kernel)

 3. CPU cambia a ring 0 (kernel mode)
    ┌──────────────────────────────────────┐
    │ CPU automáticamente:                 │
    │ • Guarda RIP (instruction pointer)   │
    │ • Guarda RFLAGS                      │
    │ • Cambia RSP al kernel stack         │
    │ • Salta a entry_SYSCALL_64           │
    │   (dirección en MSR LSTAR)           │
    └──────────────────────────────────────┘

 4. Kernel: entry_SYSCALL_64
    • Guarda registros del usuario en pt_regs
    • Verifica que RAX < NR_syscalls
    • Busca en sys_call_table[RAX]
    • Llama a __x64_sys_write(fd=1, buf=&"hello", count=6)

 5. __x64_sys_write:
    • Busca el file descriptor 1 en la tabla de FDs del proceso
    • Encuentra que fd 1 apunta al TTY
    • Copia "hello\n" del user space al kernel space
    • Llama al driver del TTY
    • Retorna 6 (bytes escritos) o un error negativo

 6. Kernel restaura registros del usuario
    • Pone valor de retorno en RAX
    • Ejecuta SYSRET (o IRET)
    • CPU cambia a ring 3

 7. De vuelta en user space:
    • glibc lee RAX
    • Si RAX es negativo → pone -RAX en errno, retorna -1
    • Si RAX >= 0 → retorna RAX (6 en este caso)
```

### Context switch: el costo real

El paso entre user space y kernel space es un **context switch parcial**. No es tan costoso como un cambio de proceso completo, pero implica:

- Guardar y restaurar registros.
- Cambiar el stack pointer al kernel stack.
- Flush de pipelines de la CPU (mitigaciones de Spectre/Meltdown).
- Verificaciones de seguridad.

Esto toma del orden de **cientos de nanosegundos** — insignificante para una operación aislada, pero significativo si haces millones de syscalls por segundo (por eso `mmap` puede ser más eficiente que `read`/`write` para archivos grandes — se explora en S03).

---

## 4. La tabla de syscalls

El kernel mantiene una tabla (array) donde cada índice es un **número de syscall** y el valor es un puntero a la función del kernel que lo implementa:

```c
// Conceptualmente (el código real es más complejo):
typedef long (*sys_call_fn)(unsigned long, unsigned long, ...);

sys_call_fn sys_call_table[] = {
    [0]   = sys_read,        // SYS_read
    [1]   = sys_write,       // SYS_write
    [2]   = sys_open,        // SYS_open
    [3]   = sys_close,       // SYS_close
    [4]   = sys_stat,        // SYS_stat
    // ... hasta ~450 en Linux 6.x
    [62]  = sys_kill,        // SYS_kill
    [231] = sys_exit_group,  // SYS_exit_group
    [435] = sys_clone3,      // SYS_clone3 (añadida en Linux 5.3)
};
```

### Consultar los números de syscall

```bash
# En tu sistema (x86-64)
grep -E "^#define __NR_" /usr/include/asm/unistd_64.h | head -20

# #define __NR_read                   0
# #define __NR_write                  1
# #define __NR_open                   2
# #define __NR_close                  3
# #define __NR_stat                   4
# #define __NR_fstat                  5
# ...
```

En C, los números están disponibles como constantes `SYS_*` vía `<sys/syscall.h>`:

```c
#include <sys/syscall.h>
#include <stdio.h>

int main(void) {
    printf("SYS_read  = %d\n", SYS_read);   // 0
    printf("SYS_write = %d\n", SYS_write);  // 1
    printf("SYS_open  = %d\n", SYS_open);   // 2
    printf("SYS_fork  = %d\n", SYS_fork);   // 57
    printf("SYS_clone = %d\n", SYS_clone);  // 56
    return 0;
}
```

### Los números son estables pero dependen de la arquitectura

Los números de syscall **nunca cambian** una vez asignados (estabilidad ABI de Linux). Pero son **diferentes** entre arquitecturas:

| Syscall | x86-64 | ARM64 (aarch64) | x86 (32-bit) |
|---------|:------:|:--------:|:--------:|
| `read` | 0 | 63 | 3 |
| `write` | 1 | 64 | 4 |
| `open` | 2 | — (usa `openat`) | 5 |
| `fork` | 57 | — (usa `clone`) | 2 |

Algunas syscalls no existen en arquitecturas nuevas (ARM64 no tiene `open`, solo `openat`; no tiene `fork`, solo `clone`). Los wrappers de glibc abstraen estas diferencias.

### Nuevas syscalls

Linux añade syscalls con cada versión del kernel (nunca elimina las existentes). Ejemplos recientes:

| Syscall | Kernel | Propósito |
|---------|--------|-----------|
| `io_uring_setup` | 5.1 | I/O asíncrono de alto rendimiento |
| `clone3` | 5.3 | Versión extensible de clone |
| `close_range` | 5.9 | Cerrar un rango de file descriptors |
| `landlock_create_ruleset` | 5.13 | Sandboxing sin root |
| `cachestat` | 6.5 | Estadísticas de page cache |

---

## 5. Wrappers de glibc: la capa que usas sin saberlo

Cuando escribes `write(fd, buf, count)` en C, no estás invocando la syscall directamente — estás llamando al **wrapper de glibc**. Este wrapper:

1. Pone los argumentos en los registros correctos (convención ABI de Linux).
2. Ejecuta la instrucción `syscall`.
3. Verifica el valor de retorno.
4. Si es error, pone el código en `errno` y retorna `-1`.

```
 Tu código C                glibc wrapper              Kernel
┌─────────────┐        ┌──────────────────┐       ┌──────────────┐
│ ssize_t n = │        │ Poner args en    │       │              │
│   write(1,  │───────►│ registros        │       │              │
│   buf, 6);  │        │ RAX=1, RDI=1,   │       │              │
│             │        │ RSI=buf, RDX=6   │       │              │
│             │        │      │           │       │              │
│             │        │  syscall         │──────►│ sys_write()  │
│             │        │  instruction     │       │ Escribe datos│
│             │        │      │           │◄──────│ Retorna 6    │
│             │        │ if (ret < 0):    │       │ o -EFAULT    │
│             │        │   errno = -ret   │       │              │
│ if (n == -1)│◄───────│   return -1      │       │              │
│   perror(.) │        │ else:            │       │              │
│             │        │   return ret     │       │              │
└─────────────┘        └──────────────────┘       └──────────────┘
```

### La convención de error: la transformación más importante

El kernel retorna errores como **valores negativos** (ej: `-ENOENT` = -2, `-EPERM` = -1). Pero la convención POSIX de C es retornar `-1` y poner el código de error en la variable global `errno`. glibc hace esta traducción:

```c
// Lo que el kernel retorna:
//   Éxito: valor >= 0 (ej: bytes escritos)
//   Error: valor < 0  (ej: -ENOENT = -2)

// Lo que glibc te da:
//   Éxito: valor >= 0 (mismo)
//   Error: retorna -1, errno = ENOENT (2)
```

Si usaras `syscall()` directamente, tendrías que hacer esta traducción tú mismo.

### Wrappers que añaden funcionalidad

Algunos wrappers de glibc hacen más que solo traducir:

- **`printf`**: formatea strings, maneja buffers, y eventualmente llama a `write`.
- **`fopen`/`fread`/`fwrite`**: buffered I/O sobre `open`/`read`/`write`.
- **`malloc`**: lógica compleja de heap management que usa `brk`/`mmap` internamente.
- **`exit`**: llama a `atexit` handlers, flush de buffers de stdio, luego `exit_group`.

### Wrappers que son casi transparentes

Otros wrappers son 1:1 — solo hacen la traducción de error:

- **`read`/`write`**: wrapper directo de la syscall.
- **`open`/`close`**: wrapper directo.
- **`fork`**: wrapper de `clone` con flags específicos.
- **`kill`**: wrapper directo.

---

## 6. Invocar syscalls directamente: syscall()

La función `syscall()` de glibc permite invocar cualquier syscall por su número, sin pasar por el wrapper específico:

```c
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

int main(void) {
    const char msg[] = "hello from raw syscall\n";

    // Usando el wrapper de glibc (la forma normal):
    write(STDOUT_FILENO, msg, sizeof(msg) - 1);

    // Usando syscall() directamente (equivalente):
    long ret = syscall(SYS_write, STDOUT_FILENO, msg, sizeof(msg) - 1);

    if (ret == -1) {
        perror("syscall write");
        return 1;
    }

    printf("syscall returned: %ld\n", ret);
    return 0;
}
```

```bash
gcc -o raw_syscall raw_syscall.c
./raw_syscall
# hello from raw syscall
# hello from raw syscall
# syscall returned: 23
```

### Otro ejemplo: getpid y gettid

```c
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

int main(void) {
    // getpid tiene wrapper en glibc
    pid_t pid = getpid();

    // gettid NO tenía wrapper en glibc hasta 2.30
    // En sistemas antiguos, se invocaba así:
    pid_t tid = syscall(SYS_gettid);

    printf("PID: %d, TID: %d\n", pid, tid);
    // En un programa single-threaded, PID == TID

    return 0;
}
```

---

## 7. Syscalls sin wrapper: syscall() como último recurso

Hay dos razones para usar `syscall()` en vez del wrapper:

### 1. La syscall es demasiado nueva

Cuando el kernel añade una syscall nueva, glibc puede tardar meses o años en incluir un wrapper. Durante ese período, la única forma de usarla es `syscall()`:

```c
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <stdio.h>

// close_range (Linux 5.9) — cierra un rango de file descriptors
// glibc 2.34+ tiene wrapper, pero versiones anteriores no
#ifndef SYS_close_range
#define SYS_close_range 436  // número en x86-64
#endif

int main(void) {
    // Cerrar todos los FDs del 3 al 1023
    long ret = syscall(SYS_close_range, 3, 1023, 0);
    if (ret == -1) {
        perror("close_range");
    } else {
        printf("FDs 3-1023 cerrados\n");
    }
    return 0;
}
```

### 2. Quieres evitar la semántica del wrapper

Algunos wrappers modifican el comportamiento. Por ejemplo, `getpid()` de glibc cachea el PID — no hace la syscall cada vez. Si necesitas el valor real después de un `clone()` exótico:

```c
// glibc getpid() puede retornar un valor cacheado
pid_t cached_pid = getpid();

// syscall(SYS_getpid) SIEMPRE pregunta al kernel
pid_t real_pid = syscall(SYS_getpid);
```

### En la práctica

El 99% del tiempo usarás los wrappers normales. `syscall()` es para:
- Syscalls recientes sin wrapper en tu versión de glibc.
- Bypasear comportamiento del wrapper (raro).
- Entender cómo funciona todo internamente (educación — este curso).

---

## 8. Categorías de syscalls

Linux tiene ~450 syscalls (Linux 6.x). Se agrupan en categorías:

### Archivos y filesystem

| Syscall | Wrapper glibc | Propósito |
|---------|--------------|-----------|
| `openat` | `open()` | Abrir archivo |
| `close` | `close()` | Cerrar file descriptor |
| `read` | `read()` | Leer de un fd |
| `write` | `write()` | Escribir a un fd |
| `lseek` | `lseek()` | Mover posición en un fd |
| `fstat` | `fstat()` | Obtener metadatos de archivo |
| `unlinkat` | `unlink()` | Eliminar archivo |
| `renameat2` | `rename()` | Renombrar archivo |
| `mkdirat` | `mkdir()` | Crear directorio |

### Procesos

| Syscall | Wrapper glibc | Propósito |
|---------|--------------|-----------|
| `clone` / `clone3` | `fork()`, `pthread_create()` | Crear proceso/hilo |
| `execve` | `execvp()` y familia | Reemplazar programa |
| `wait4` | `waitpid()` | Esperar a proceso hijo |
| `exit_group` | `exit()` | Terminar proceso |
| `kill` | `kill()` | Enviar señal |
| `getpid` | `getpid()` | Obtener PID |

### Memoria

| Syscall | Wrapper glibc | Propósito |
|---------|--------------|-----------|
| `mmap` | `mmap()` | Mapear memoria |
| `munmap` | `munmap()` | Desmapear memoria |
| `mprotect` | `mprotect()` | Cambiar permisos de memoria |
| `brk` | `sbrk()` | Ajustar el heap |

### Red

| Syscall | Wrapper glibc | Propósito |
|---------|--------------|-----------|
| `socket` | `socket()` | Crear socket |
| `bind` | `bind()` | Asignar dirección a socket |
| `listen` | `listen()` | Escuchar conexiones |
| `accept4` | `accept()` | Aceptar conexión |
| `connect` | `connect()` | Conectar a dirección remota |
| `sendto` | `send()`, `sendto()` | Enviar datos |
| `recvfrom` | `recv()`, `recvfrom()` | Recibir datos |

### Información del sistema

| Syscall | Wrapper glibc | Propósito |
|---------|--------------|-----------|
| `uname` | `uname()` | Info del kernel/hostname |
| `clock_gettime` | `clock_gettime()` | Hora actual |
| `getuid` | `getuid()` | UID del proceso |
| `getrlimit` | `getrlimit()` | Límites de recursos |

En S02–S04 explorarás las de archivos, mmap e inotify en profundidad. En C02–C06 trabajarás con procesos, señales, IPC y sockets.

---

## 9. El costo de una syscall

Una syscall no es gratis. El context switch entre user space y kernel space tiene un costo medible:

```c
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#include <time.h>
#include <stdio.h>

#define ITERATIONS 1000000

int main(void) {
    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (int i = 0; i < ITERATIONS; i++) {
        syscall(SYS_getpid);  // syscall simple y rápida
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) +
                     (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("%d syscalls en %.3f s\n", ITERATIONS, elapsed);
    printf("%.0f ns por syscall\n", (elapsed / ITERATIONS) * 1e9);

    return 0;
}
```

```bash
gcc -O2 -o syscall_bench syscall_bench.c
./syscall_bench
# 1000000 syscalls en 0.150 s
# 150 ns por syscall
```

~100–300 ns por syscall en hardware moderno. Comparación:

| Operación | Tiempo aproximado |
|-----------|:-----------------:|
| Suma entera | ~0.3 ns |
| Acceso a L1 cache | ~1 ns |
| Acceso a L3 cache | ~10 ns |
| **Syscall simple** | **~100–300 ns** |
| Acceso a RAM | ~100 ns |
| Lectura de SSD | ~10,000 ns |
| Lectura de HDD | ~10,000,000 ns |

Las syscalls son ~300× más caras que una operación en memoria. Por eso:
- **Buffered I/O** (stdio) acumula datos antes de hacer `write`.
- **mmap** mapea un archivo entero en vez de hacer miles de `read`.
- **io_uring** permite batch de operaciones I/O en una sola syscall.

### vDSO: syscalls que no son syscalls

Para syscalls que se invocan constantemente (como `clock_gettime` y `gettimeofday`), Linux implementa **vDSO** (virtual Dynamic Shared Object): una página de memoria del kernel mapeada en user space de solo lectura. La "syscall" se ejecuta completamente en user space leyendo datos compartidos — sin context switch:

```bash
# Ver el vDSO cargado en tu proceso
cat /proc/self/maps | grep vdso
# 7fff12345000-7fff12347000 r-xp ... [vdso]

# Las funciones del vDSO
objdump -T /usr/lib64/vdso/vdso64.so 2>/dev/null || \
  LD_SHOW_AUXV=1 /bin/true | grep SYSINFO
```

`clock_gettime` via vDSO toma ~20 ns en vez de ~150 ns. glibc detecta automáticamente si la syscall tiene implementación vDSO y la usa.

---

## 10. Errores comunes

### Error 1: confundir funciones de biblioteca con syscalls

```c
// "printf es una syscall"  ← INCORRECTO
printf("hello\n");

// printf es una función de glibc que eventualmente usa la syscall write
// La distinción importa porque:
// - printf bufferiza, write no
// - printf formatea, write no
// - printf puede no llamar a write inmediatamente
```

**Cómo distinguirlos**: las syscalls están en la sección 2 del manual (`man 2 write`), las funciones de biblioteca en la sección 3 (`man 3 printf`).

```bash
man 2 write    # syscall
man 3 printf   # función de biblioteca
man 2 open     # syscall
man 3 fopen    # función de biblioteca (wrapper sobre open)
```

### Error 2: no verificar el retorno de una syscall

```c
// ✗ INCORRECTO — ignora errores
write(fd, buf, len);
read(fd, buf, sizeof(buf));

// ✓ CORRECTO — siempre verificar
ssize_t n = write(fd, buf, len);
if (n == -1) {
    perror("write");
    // manejar el error
}
```

Toda syscall puede fallar. Ignorar el retorno es un bug — el programa continúa asumiendo que la operación funcionó cuando no fue así. El manejo de errores con `errno` se profundiza en T02.

### Error 3: asumir que write escribe todo

```c
ssize_t n = write(fd, buf, 10000);
// n podría ser 4096, no 10000 — "short write"
```

**Por qué pasa**: `write` puede escribir menos bytes de los solicitados (por señales, buffer lleno, etc.). El programa debe verificar y reintentar. Esto se cubre a fondo en S02/T01.

### Error 4: creer que los números de syscall son universales

```c
// "SYS_write es siempre 1"  ← solo en x86-64
// En ARM64, SYS_write = 64
// En x86 (32-bit), SYS_write = 4
```

**Solución**: nunca hardcodear números. Siempre usar `SYS_write`, `SYS_read`, etc. de `<sys/syscall.h>`.

### Error 5: usar syscall() cuando hay wrapper

```c
// ✗ Innecesario y menos portable
long n = syscall(SYS_write, fd, buf, len);

// ✓ Usar el wrapper — más legible, portable, y correcto
ssize_t n = write(fd, buf, len);
```

`syscall()` solo se justifica cuando no existe wrapper (syscall nueva) o necesitas bypasear el wrapper.

---

## 11. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                   SYSCALLS — REFERENCIA                          │
├──────────────────────────────────────────────────────────────────┤
│ CONCEPTOS                                                        │
│   User space (ring 3)    Tu programa y bibliotecas              │
│   Kernel space (ring 0)  Hardware, memoria, filesystem          │
│   Syscall                Única forma de cruzar user → kernel    │
│   Wrapper glibc          Función C que invoca la syscall        │
│   errno                  Variable global con código de error    │
├──────────────────────────────────────────────────────────────────┤
│ INVOCAR SYSCALLS                                                 │
│   write(fd, buf, n)         Wrapper (recomendado)               │
│   syscall(SYS_write, ...)   Directo (solo si no hay wrapper)    │
├──────────────────────────────────────────────────────────────────┤
│ NÚMEROS DE SYSCALL                                               │
│   #include <sys/syscall.h>   SYS_read, SYS_write, ...          │
│   /usr/include/asm/          Header con #define __NR_*          │
│     unistd_64.h                                                  │
│   man 2 syscalls             Lista completa                     │
│   ausyscall --dump           Nombres ↔ números                  │
├──────────────────────────────────────────────────────────────────┤
│ MANUAL                                                           │
│   man 2 <nombre>             Documentación de la syscall        │
│   man 3 <nombre>             Documentación de función glibc     │
│   man 7 <tema>               Conceptos generales                │
├──────────────────────────────────────────────────────────────────┤
│ CONVENCIÓN DE RETORNO                                            │
│   Kernel:  >= 0 = éxito, < 0 = -errno                          │
│   glibc:   >= 0 = éxito, -1 = error (errno set)                │
├──────────────────────────────────────────────────────────────────┤
│ COSTO                                                            │
│   ~100–300 ns por syscall                                       │
│   vDSO: ~20 ns (clock_gettime, gettimeofday)                    │
│   Minimizar syscalls → buffered I/O, mmap, io_uring             │
└──────────────────────────────────────────────────────────────────┘
```

---

## 12. Ejercicios

### Ejercicio 1: Inspeccionar syscalls con los headers

1. Encuentra el archivo de definiciones de syscalls en tu sistema:
   ```bash
   find /usr/include -name "unistd_64.h" 2>/dev/null
   ```
2. Cuenta cuántas syscalls define:
   ```bash
   grep -c "^#define __NR_" /usr/include/asm/unistd_64.h
   ```
3. Escribe un programa que imprima los números de las syscalls que usarás en este bloque:
   ```c
   #include <sys/syscall.h>
   #include <stdio.h>

   int main(void) {
       printf("write     = %d\n", SYS_write);
       printf("read      = %d\n", SYS_read);
       printf("openat    = %d\n", SYS_openat);
       printf("close     = %d\n", SYS_close);
       printf("fork      = %d\n", SYS_fork);
       printf("execve    = %d\n", SYS_execve);
       printf("mmap      = %d\n", SYS_mmap);
       printf("socket    = %d\n", SYS_socket);
       printf("clone     = %d\n", SYS_clone);
       printf("exit_group= %d\n", SYS_exit_group);
       return 0;
   }
   ```
4. Compila y ejecuta. ¿Los números coinciden con lo que ves en `unistd_64.h`?

> **Pregunta de reflexión**: Linux nunca elimina ni reasigna números de syscall. ¿Por qué esta garantía de estabilidad es crucial para que los binarios compilados hace 10 años sigan funcionando hoy?

### Ejercicio 2: write() wrapper vs syscall()

1. Escribe un programa que imprima "hello" de tres formas distintas:
   ```c
   #define _GNU_SOURCE
   #include <unistd.h>
   #include <sys/syscall.h>
   #include <stdio.h>

   int main(void) {
       const char msg1[] = "1: printf\n";
       const char msg2[] = "2: write wrapper\n";
       const char msg3[] = "3: raw syscall\n";

       // Forma 1: función de biblioteca
       printf("%s", msg1);

       // Forma 2: wrapper de glibc
       write(STDOUT_FILENO, msg2, sizeof(msg2) - 1);

       // Forma 3: syscall directa
       syscall(SYS_write, STDOUT_FILENO, msg3, sizeof(msg3) - 1);

       return 0;
   }
   ```
2. Compila y ejecuta. ¿Las tres producen el mismo resultado?
3. Redirige la salida a un archivo: `./program > output.txt`. ¿Cambió el orden? ¿Por qué? (Pista: buffering de stdio).
4. Ejecuta con `strace`: `strace -e write ./program`. ¿Cuántas syscalls `write` se generaron? ¿Las formas 1 y 2 producen la misma traza?

> **Pregunta de reflexión**: cuando redirigiste a un archivo, printf probablemente cambió de line-buffered a fully-buffered. ¿Eso significa que printf se ejecutó después de las otras dos formas? ¿O se ejecutó en el mismo orden pero el `write` subyacente ocurrió después? ¿Qué implicaciones tiene esto para logging en un programa que puede crashear?

### Ejercicio 3: Medir el costo de una syscall

1. Compila y ejecuta el benchmark de la sección 9 (el que mide `getpid` en loop).
2. Modifica para comparar tres operaciones:
   - `getpid()` — wrapper glibc (cached en muchas versiones).
   - `syscall(SYS_getpid)` — fuerza la syscall real.
   - Una suma simple `sum += i` — operación pura en user space.
3. Ejecuta cada una 10 millones de veces y compara tiempos.
4. ¿Cuál es la diferencia entre `getpid()` y `syscall(SYS_getpid)`? Si `getpid()` es más rápido, ¿por qué? (Pista: glibc cachea el resultado).
5. ¿Cuántas veces más lenta es la syscall real comparada con la suma?

> **Pregunta de reflexión**: si una syscall toma ~200 ns y tu programa hace 1000 syscalls para procesar un request HTTP, ¿cuántos microsegundos se gastan solo en context switches? ¿Es significativo comparado con el I/O de red? ¿En qué tipo de aplicaciones el costo de las syscalls se vuelve el cuello de botella?
