# File Locking — Bloqueo de Archivos

## Índice

1. [Por qué necesitamos file locking](#1-por-qué-necesitamos-file-locking)
2. [Advisory vs mandatory locking](#2-advisory-vs-mandatory-locking)
3. [flock: bloqueo por archivo completo](#3-flock-bloqueo-por-archivo-completo)
4. [fcntl: bloqueo por rango de bytes](#4-fcntl-bloqueo-por-rango-de-bytes)
5. [lockf: interfaz simplificada sobre fcntl](#5-lockf-interfaz-simplificada-sobre-fcntl)
6. [Open file description locks (OFD)](#6-open-file-description-locks-ofd)
7. [Comparación de mecanismos](#7-comparación-de-mecanismos)
8. [Patrones prácticos](#8-patrones-prácticos)
9. [Lock files: bloqueo por convención](#9-lock-files-bloqueo-por-convención)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Por qué necesitamos file locking

Cuando múltiples procesos acceden al mismo archivo simultáneamente,
sin coordinación los resultados son impredecibles:

```
  Proceso A                     Proceso B
  ─────────────────────────     ─────────────────────────
  lee saldo: 1000               lee saldo: 1000
  calcula: 1000 - 200 = 800
                                calcula: 1000 - 300 = 700
  escribe: 800
                                escribe: 700  ← ¡pierde los -200!

  Resultado: 700 (debería ser 500)
```

Este es un **race condition** clásico. File locking permite serializar
el acceso:

```
  Proceso A                     Proceso B
  ─────────────────────────     ─────────────────────────
  lock(archivo)
  lee saldo: 1000               lock(archivo) → BLOQUEADO
  calcula: 800                     (espera...)
  escribe: 800                     (espera...)
  unlock(archivo)
                                lock(archivo) → OK
                                lee saldo: 800
                                calcula: 500
                                escribe: 500
                                unlock(archivo)

  Resultado: 500 ✓
```

### Escenarios que requieren locking

```
  Escenario                     Mecanismo típico
  ──────────────────────────────────────────────────────
  PID file de daemon            lock file + flock
  Base de datos (SQLite)        fcntl byte-range locks
  Log compartido                O_APPEND (no necesita lock)
  Archivo de configuración      lock file o flock
  Cola de trabajo en archivo    fcntl con rangos
  Acceso exclusivo a recurso    flock o lock file
```

> **Nota**: `O_APPEND` garantiza escrituras atómicas al final del archivo
> (el seek+write es atómico en el kernel). Para logs simples esto basta
> y no necesitas file locking explícito.

---

## 2. Advisory vs mandatory locking

Linux soporta dos modelos de locking fundamentalmente distintos:

### Advisory locking (recomendado)

```
  Proceso cooperativo A         Proceso no cooperativo C
  ─────────────────────────     ─────────────────────────
  lock(fd) → OK                 write(fd, data)  ← ¡pasa!
  read(fd)
  unlock(fd)                    El kernel NO bloquea a C
```

El lock advisory es un **contrato social**: solo funciona si **todos**
los procesos que acceden al archivo verifican el lock antes de operar.
Un proceso que no intente adquirir el lock puede leer/escribir libremente.

Esto parece una debilidad, pero es el modelo dominante en Unix porque:
- Es eficiente (no hay verificación en cada read/write).
- No tiene efectos secundarios sorpresivos.
- Los procesos bien escritos lo respetan.
- Es lo que usan SQLite, PostgreSQL, dpkg, etc.

### Mandatory locking (obsoleto)

Mandatory locking fuerza al kernel a bloquear `read()`/`write()` de
cualquier proceso que no tenga el lock, incluso si no lo pidió.

```
  Para activar mandatory locking (Linux):
  1. mount -o mand /dev/sda1 /mnt       ← opción de montaje
  2. chmod g+s,g-x archivo               ← setgid sin exec
```

**Mandatory locking está deprecado en Linux desde el kernel 4.5 y
eliminado en el kernel 5.15**. No lo uses en código nuevo. Las razones:

- Impacto de rendimiento en cada operación de I/O.
- Problemas con `mmap` (no se puede forzar el lock en páginas mapeadas).
- Race conditions inherentes al diseño.
- Ningún filesystem moderno lo soporta correctamente.

**Todo el file locking en práctica es advisory.**

---

## 3. flock: bloqueo por archivo completo

```c
#include <sys/file.h>

int flock(int fd, int operation);
```

`flock` es el mecanismo más simple: bloquea el archivo **completo**,
sin granularidad de rangos.

### Operaciones

```
  Operación            Efecto
  ──────────────────────────────────────────────────────
  LOCK_SH              lock compartido (lectura)
  LOCK_EX              lock exclusivo (escritura)
  LOCK_UN              desbloquear
  LOCK_NB              no bloquear (combinar con |)
```

### Semántica de locks compartidos vs exclusivos

```
  ¿Puedo obtener...?     Ya hay LOCK_SH    Ya hay LOCK_EX
  ─────────────────────────────────────────────────────────
  LOCK_SH                SÍ (múltiples)    NO (bloqueado)
  LOCK_EX                NO (bloqueado)    NO (bloqueado)
```

Múltiples lectores pueden coexistir, pero un escritor es exclusivo:

```
            Lectores                Escritor
  ┌───┐ ┌───┐ ┌───┐              ┌───┐
  │ A │ │ B │ │ C │              │ D │
  │SH │ │SH │ │SH │              │EX │
  └─┬─┘ └─┬─┘ └─┬─┘              └─┬─┘
    │     │     │                   │
    ▼     ▼     ▼                   ▼
  ┌─────────────────┐          ┌─────────────┐
  │ Todos acceden   │          │ Solo D      │
  │ simultáneamente │          │ accede      │
  └─────────────────┘          └─────────────┘
```

### Ejemplo: lectura con lock compartido

```c
#include <stdio.h>
#include <sys/file.h>
#include <unistd.h>
#include <fcntl.h>

int read_with_lock(const char *path, char *buf, size_t size) {
    int fd = open(path, O_RDONLY);
    if (fd == -1) { perror("open"); return -1; }

    // Lock compartido — otros lectores OK, escritores esperan
    if (flock(fd, LOCK_SH) == -1) {
        perror("flock LOCK_SH");
        close(fd);
        return -1;
    }

    ssize_t n = read(fd, buf, size);

    // flock se libera automáticamente en close(),
    // pero el desbloqueo explícito es más claro
    flock(fd, LOCK_UN);
    close(fd);
    return (int)n;
}
```

### Ejemplo: escritura con lock exclusivo

```c
int write_with_lock(const char *path, const char *data, size_t len) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd == -1) { perror("open"); return -1; }

    if (flock(fd, LOCK_EX) == -1) {
        perror("flock LOCK_EX");
        close(fd);
        return -1;
    }

    ssize_t written = write(fd, data, len);

    flock(fd, LOCK_UN);
    close(fd);
    return (int)written;
}
```

### Lock no bloqueante

```c
if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
    if (errno == EWOULDBLOCK) {
        printf("Otro proceso tiene el lock\n");
        // Decidir: reintentar, abortar, esperar...
    } else {
        perror("flock");
    }
}
```

### Semántica de flock en Linux

Reglas importantes:

1. **El lock se asocia a la open file description**, no al fd ni al proceso.
2. `dup()` o `dup2()` comparten el mismo lock (misma open file description).
3. `fork()` también comparte el lock con el hijo (hereda el fd).
4. El lock se libera cuando **todos** los fd que apuntan a esa open file
   description se cierran.
5. `open()` del mismo archivo crea una **nueva** open file description
   con su propio lock independiente.

```
  Proceso padre                 Proceso hijo (fork)
  fd=3 ──┐                     fd=3 ──┐
         ├── open file desc ────────────┘
         │   (LOCK_EX)
         │
  fd=4 = dup(fd=3) ──┘         Comparten el lock

  close(fd=3) en padre: lock SIGUE activo (fd=4 y hijo aún lo tienen)
  close(fd=4) en padre + close(fd=3) en hijo: lock se libera
```

### flock y NFS

`flock` **no funciona** en NFS en muchas implementaciones. Algunas versiones
de Linux emulan `flock` sobre NFS usando `fcntl`, pero el comportamiento
no es confiable. Para NFS, usa `fcntl` directamente.

---

## 4. fcntl: bloqueo por rango de bytes

```c
#include <fcntl.h>

int fcntl(int fd, int cmd, struct flock *lock);
```

`fcntl` permite bloquear **rangos específicos** dentro de un archivo,
lo que habilita acceso concurrente a distintas secciones:

```
  Archivo: registro de empleados
  ┌──────────┬──────────┬──────────┬──────────┐
  │ Reg. 0   │ Reg. 1   │ Reg. 2   │ Reg. 3   │
  │ 0-99     │ 100-199  │ 200-299  │ 300-399  │
  │ [Proc A] │ [libre]  │ [Proc B] │ [libre]  │
  │ LOCK_EX  │          │ LOCK_EX  │          │
  └──────────┴──────────┴──────────┴──────────┘

  A y B trabajan en registros diferentes simultáneamente
```

### struct flock

```c
struct flock {
    short l_type;    // F_RDLCK, F_WRLCK, F_UNLCK
    short l_whence;  // SEEK_SET, SEEK_CUR, SEEK_END
    off_t l_start;   // offset de inicio del rango
    off_t l_len;     // longitud (0 = hasta EOF)
    pid_t l_pid;     // PID del dueño (solo F_GETLK)
};
```

### Comandos

```
  Comando      Comportamiento
  ──────────────────────────────────────────────────────
  F_SETLK      establecer lock (no bloqueante)
  F_SETLKW     establecer lock (bloqueante — wait)
  F_GETLK      consultar quién tiene el lock
```

### Ejemplo: lock de un rango

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int lock_range(int fd, int type, off_t start, off_t len) {
    struct flock fl = {
        .l_type   = type,      // F_RDLCK, F_WRLCK, F_UNLCK
        .l_whence = SEEK_SET,
        .l_start  = start,
        .l_len    = len,       // 0 = hasta el final del archivo
    };

    if (fcntl(fd, F_SETLKW, &fl) == -1) {
        perror("fcntl F_SETLKW");
        return -1;
    }
    return 0;
}

int unlock_range(int fd, off_t start, off_t len) {
    return lock_range(fd, F_UNLCK, start, len);
}
```

### Ejemplo: acceso concurrente a registros

```c
#define RECORD_SIZE 128

typedef struct {
    char name[64];
    int balance;
    char padding[60];
} record_t;

int update_record(int fd, int record_num, int amount) {
    off_t offset = (off_t)record_num * RECORD_SIZE;

    // Lock exclusivo solo del registro que vamos a modificar
    if (lock_range(fd, F_WRLCK, offset, RECORD_SIZE) == -1)
        return -1;

    // Leer registro
    record_t rec;
    pread(fd, &rec, sizeof(rec), offset);

    // Modificar
    rec.balance += amount;

    // Escribir
    pwrite(fd, &rec, sizeof(rec), offset);

    // Desbloquear
    unlock_range(fd, offset, RECORD_SIZE);
    return 0;
}
```

### Consultar locks existentes: F_GETLK

```c
int who_has_lock(int fd, off_t start, off_t len) {
    struct flock fl = {
        .l_type   = F_WRLCK,    // "¿puedo obtener escritura?"
        .l_whence = SEEK_SET,
        .l_start  = start,
        .l_len    = len,
    };

    if (fcntl(fd, F_GETLK, &fl) == -1) {
        perror("fcntl F_GETLK");
        return -1;
    }

    if (fl.l_type == F_UNLCK) {
        printf("Rango libre\n");
    } else {
        printf("Bloqueado por PID %d, tipo %s, rango [%ld, %ld)\n",
               fl.l_pid,
               fl.l_type == F_RDLCK ? "READ" : "WRITE",
               (long)fl.l_start,
               (long)(fl.l_start + fl.l_len));
    }
    return 0;
}
```

### Semántica de fcntl locks (POSIX)

Las reglas de `fcntl` son **muy diferentes** a las de `flock`:

1. **El lock se asocia al proceso (PID)**, no al fd ni a la open file
   description.
2. **Cerrar CUALQUIER fd** del archivo libera **TODOS** los locks del
   proceso sobre ese archivo.
3. Los locks **no se heredan** a través de `fork()`.
4. Los locks **sí sobreviven** a `exec()`.

```
  Proceso con PID 1234:
  fd=3 = open("data.db")    → lock rango [0, 100)
  fd=4 = open("data.db")    → lock rango [100, 200)

  close(fd=3);  ← ¡SORPRESA! libera AMBOS locks
                   porque ambos pertenecen a PID 1234
                   sobre el mismo archivo (mismo inodo)
```

Esta es la trampa más peligrosa de `fcntl` locks. Es la razón por la
que se crearon los OFD locks (sección 6).

### Lock de todo el archivo con fcntl

```c
// l_start = 0, l_len = 0 → desde offset 0 hasta EOF (y más allá)
struct flock fl = {
    .l_type   = F_WRLCK,
    .l_whence = SEEK_SET,
    .l_start  = 0,
    .l_len    = 0,   // 0 = especial: hasta el final
};
fcntl(fd, F_SETLKW, &fl);
```

### Deadlock detection

`fcntl` con `F_SETLKW` detecta deadlocks y devuelve `EDEADLK`:

```c
if (fcntl(fd, F_SETLKW, &fl) == -1) {
    if (errno == EDEADLK) {
        fprintf(stderr, "Deadlock detectado\n");
        // Abortar o reintentar con diferente orden
    }
}
```

```
  Deadlock:
  Proceso A: tiene lock en rango [0,100), pide [100,200)
  Proceso B: tiene lock en rango [100,200), pide [0,100)
  → El kernel detecta el ciclo y EDEADLK al segundo proceso
```

---

## 5. lockf: interfaz simplificada sobre fcntl

```c
#include <unistd.h>

int lockf(int fd, int cmd, off_t len);
```

`lockf` es un wrapper sobre `fcntl` que simplifica la interfaz para
locks exclusivos basados en la posición actual del cursor:

```
  Comando      Equivalente fcntl
  ──────────────────────────────────────────────────────
  F_LOCK       F_SETLKW + F_WRLCK (bloqueante)
  F_TLOCK      F_SETLK + F_WRLCK (no bloqueante)
  F_ULOCK      F_SETLK + F_UNLCK
  F_TEST       F_GETLK (consultar)
```

### Ejemplo

```c
int fd = open("data.bin", O_RDWR);

// Posicionar al inicio del registro 5
lseek(fd, 5 * RECORD_SIZE, SEEK_SET);

// Lock exclusivo desde posición actual, longitud RECORD_SIZE
if (lockf(fd, F_LOCK, RECORD_SIZE) == -1) {
    perror("lockf");
}

// ... trabajar con el registro ...

// Desbloquear (volver a la posición del lock)
lseek(fd, 5 * RECORD_SIZE, SEEK_SET);
lockf(fd, F_ULOCK, RECORD_SIZE);
```

### Limitaciones

- Solo soporta locks exclusivos (no hay lock compartido/lectura).
- Usa la posición actual del cursor (debes hacer `lseek` primero).
- Tiene las mismas trampas que `fcntl` (lock asociado al proceso).

`lockf` es útil para scripts simples o cuando solo necesitas exclusión
mutua. Para locks de lectura o semántica más controlada, usa `fcntl`
directamente.

---

## 6. Open file description locks (OFD)

Los OFD locks fueron añadidos en Linux 3.15 para corregir las trampas
de `fcntl` POSIX locks. Usan la misma `struct flock` pero con comandos
distintos:

```c
#include <fcntl.h>

// Comandos OFD (en lugar de F_SETLK/F_SETLKW/F_GETLK):
F_OFD_SETLK    // no bloqueante
F_OFD_SETLKW   // bloqueante (wait)
F_OFD_GETLK    // consultar
```

### Diferencias con POSIX fcntl locks

```
  Propiedad              POSIX (F_SETLK)    OFD (F_OFD_SETLK)
  ──────────────────────────────────────────────────────────────
  Lock asociado a...     proceso (PID)       open file description
  close() de otro fd     libera TODOS        solo si es el mismo
                         los locks            open file desc
  fork()                 NO hereda           SÍ hereda (comparte)
  dup()                  misma semántica     comparte lock
  Hilos                  todos comparten     cada open() independiente
  Deadlock detection     sí (EDEADLK)       no (se bloquea)
  Campo l_pid            PID del dueño       debe ser 0
```

### El lock se asocia a la open file description

```
  POSIX lock (F_SETLK):
  ┌─────────┐  ┌─────────┐
  │ fd = 3  │  │ fd = 4  │     Ambos = open("data.db")
  └────┬────┘  └────┬────┘     Ambos del mismo proceso
       │            │
       └──────┬─────┘
              ▼
       ┌────────────┐
       │  Proceso   │  ← lock pertenece al PROCESO
       │  PID 1234  │     close(fd=3) libera TODO
       └────────────┘

  OFD lock (F_OFD_SETLK):
  ┌─────────┐  ┌─────────┐
  │ fd = 3  │  │ fd = 4  │     Cada open() crea su propia
  └────┬────┘  └────┬────┘     open file description
       │            │
       ▼            ▼
  ┌─────────┐  ┌─────────┐
  │  OFD A  │  │  OFD B  │  ← lock pertenece a CADA OFD
  │ lock #1 │  │ lock #2 │     close(fd=3) solo libera lock #1
  └─────────┘  └─────────┘
```

### Ejemplo: OFD lock seguro para hilos

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>

int ofd_lock(int fd, int type, off_t start, off_t len) {
    struct flock fl = {
        .l_type   = type,
        .l_whence = SEEK_SET,
        .l_start  = start,
        .l_len    = len,
        .l_pid    = 0,  // DEBE ser 0 para OFD locks
    };

    if (fcntl(fd, F_OFD_SETLKW, &fl) == -1) {
        perror("fcntl F_OFD_SETLKW");
        return -1;
    }
    return 0;
}

int ofd_unlock(int fd, off_t start, off_t len) {
    return ofd_lock(fd, F_UNLCK, start, len);
}
```

### Por qué OFD locks son mejores para hilos

Con POSIX locks, todos los hilos de un proceso comparten los mismos
locks (porque están asociados al PID). Un hilo puede accidentalmente
liberar el lock de otro:

```
  POSIX lock + threads (PELIGROSO):

  Hilo A                        Hilo B
  ────────────────────          ────────────────────
  fd_a = open("db")            fd_b = open("db")
  fcntl(fd_a, F_SETLKW)
  // tiene el lock
                                close(fd_b) ← ¡LIBERA el lock de A!
  write(fd_a, ...)  ← ¡sin protección!
```

Con OFD locks, cada `open()` tiene su lock independiente:

```
  OFD lock + threads (SEGURO):

  Hilo A                        Hilo B
  ────────────────────          ────────────────────
  fd_a = open("db")            fd_b = open("db")
  fcntl(fd_a, F_OFD_SETLKW)
  // tiene el lock
                                close(fd_b) ← solo libera locks de fd_b
  write(fd_a, ...)  ← protegido ✓
```

### Cuándo usar OFD locks

- Programas multihilo que necesitan file locking.
- Bibliotecas que no pueden garantizar que el usuario no abra/cierre
  el mismo archivo por separado.
- Cualquier código nuevo que necesite `fcntl`-style byte-range locks.
- SQLite los usa cuando están disponibles.

---

## 7. Comparación de mecanismos

```
  ┌────────────────┬──────────┬──────────┬──────────┬──────────┐
  │ Característica │  flock   │  fcntl   │  lockf   │  OFD     │
  ├────────────────┼──────────┼──────────┼──────────┼──────────┤
  │ Granularidad   │ archivo  │ rango    │ rango    │ rango    │
  │                │ completo │ de bytes │ de bytes │ de bytes │
  ├────────────────┼──────────┼──────────┼──────────┼──────────┤
  │ Lock compartido│ sí       │ sí       │ no       │ sí       │
  ├────────────────┼──────────┼──────────┼──────────┼──────────┤
  │ Asociado a     │ open file│ proceso  │ proceso  │ open file│
  │                │ desc     │ (PID)    │ (PID)    │ desc     │
  ├────────────────┼──────────┼──────────┼──────────┼──────────┤
  │ close() libera │ solo si  │ TODOS    │ TODOS    │ solo su  │
  │                │ último fd│ los locks│ los locks│ OFD      │
  ├────────────────┼──────────┼──────────┼──────────┼──────────┤
  │ fork() hereda  │ sí       │ no       │ no       │ sí       │
  ├────────────────┼──────────┼──────────┼──────────┼──────────┤
  │ Thread-safe    │ sí       │ NO       │ NO       │ sí       │
  ├────────────────┼──────────┼──────────┼──────────┼──────────┤
  │ NFS            │ no*      │ sí       │ sí       │ sí       │
  ├────────────────┼──────────┼──────────┼──────────┼──────────┤
  │ Deadlock det.  │ no       │ sí       │ no       │ no       │
  ├────────────────┼──────────┼──────────┼──────────┼──────────┤
  │ Portabilidad   │ BSD/Linux│ POSIX    │ POSIX    │ Linux    │
  │                │          │          │          │ 3.15+    │
  └────────────────┴──────────┴──────────┴──────────┴──────────┘

  * Algunas versiones de Linux emulan flock sobre NFS con fcntl
```

### Diagrama de decisión

```
  ¿Necesitas lock por rango de bytes?
  ├── NO → ¿Necesitas NFS?
  │        ├── NO → flock (más simple, seguro)
  │        └── SÍ → fcntl con l_len = 0
  │
  └── SÍ → ¿Programa multihilo?
           ├── NO → fcntl (POSIX, con cuidado)
           └── SÍ → OFD locks (si Linux ≥ 3.15)
                     └── fallback: fcntl + mutex
```

---

## 8. Patrones prácticos

### Patrón 1: PID file para daemons

Un daemon debe asegurar que solo una instancia ejecute a la vez:

```c
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <string.h>

int acquire_pidfile(const char *pidfile) {
    int fd = open(pidfile, O_RDWR | O_CREAT, 0644);
    if (fd == -1) {
        perror("open pidfile");
        return -1;
    }

    // Lock no bloqueante — si otro daemon ya tiene el lock, falla
    if (flock(fd, LOCK_EX | LOCK_NB) == -1) {
        if (errno == EWOULDBLOCK) {
            // Leer PID del otro daemon
            char buf[32];
            ssize_t n = read(fd, buf, sizeof(buf) - 1);
            if (n > 0) {
                buf[n] = '\0';
                fprintf(stderr, "Ya hay una instancia: PID %s", buf);
            }
        }
        close(fd);
        return -1;
    }

    // Truncar y escribir nuestro PID
    ftruncate(fd, 0);
    char pid_str[32];
    int len = snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());
    write(fd, pid_str, len);

    // NO cerrar fd — el lock se mantiene mientras el proceso viva
    // El kernel lo libera automáticamente cuando el proceso termina
    return fd;
}
```

### Patrón 2: actualización atómica de archivo de configuración

```c
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>

int update_config(const char *config_path,
                  const char *new_content, size_t len) {
    // Abrir el archivo de configuración original para lock
    int lock_fd = open(config_path, O_RDONLY);
    if (lock_fd == -1) { perror("open"); return -1; }

    if (flock(lock_fd, LOCK_EX) == -1) {
        perror("flock");
        close(lock_fd);
        return -1;
    }

    // Escribir a un temporal en el mismo directorio
    char tmp_path[256];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp.XXXXXX", config_path);
    int tmp_fd = mkstemp(tmp_path);
    if (tmp_fd == -1) {
        perror("mkstemp");
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
        return -1;
    }

    write(tmp_fd, new_content, len);
    fsync(tmp_fd);
    close(tmp_fd);

    // rename atómico
    if (rename(tmp_path, config_path) == -1) {
        perror("rename");
        unlink(tmp_path);
        flock(lock_fd, LOCK_UN);
        close(lock_fd);
        return -1;
    }

    flock(lock_fd, LOCK_UN);
    close(lock_fd);
    return 0;
}
```

### Patrón 3: lock con timeout

Ni `flock` ni `fcntl` tienen timeout nativo. Se puede implementar
con `alarm()` o con un loop de reintentos:

```c
#include <sys/file.h>
#include <time.h>
#include <errno.h>

int flock_timeout(int fd, int operation, int timeout_secs) {
    struct timespec start, now;
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (1) {
        if (flock(fd, operation | LOCK_NB) == 0) {
            return 0;  // éxito
        }

        if (errno != EWOULDBLOCK) {
            return -1;  // error real
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        if (now.tv_sec - start.tv_sec >= timeout_secs) {
            errno = ETIMEDOUT;
            return -1;  // timeout
        }

        usleep(10000);  // 10ms entre reintentos
    }
}
```

### Patrón 4: verificar si un lock está disponible (sin adquirirlo)

```c
// Con fcntl F_GETLK:
int is_locked(int fd, off_t start, off_t len) {
    struct flock fl = {
        .l_type   = F_WRLCK,
        .l_whence = SEEK_SET,
        .l_start  = start,
        .l_len    = len,
    };

    if (fcntl(fd, F_GETLK, &fl) == -1) return -1;

    return (fl.l_type != F_UNLCK);  // 1 = locked, 0 = free
}

// Con flock (intento + devolución inmediata):
int is_flock_locked(int fd) {
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        flock(fd, LOCK_UN);  // lo teníamos, liberar
        return 0;  // no estaba bloqueado
    }
    if (errno == EWOULDBLOCK) return 1;  // bloqueado
    return -1;  // error
}
```

---

## 9. Lock files: bloqueo por convención

Un **lock file** es un archivo cuya mera **existencia** indica que un
recurso está en uso. No usa `flock` ni `fcntl` — es un protocolo
basado en convención del filesystem:

### Creación atómica con O_EXCL

```c
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int acquire_lock_file(const char *lock_path) {
    // O_CREAT | O_EXCL: falla si el archivo ya existe
    // Esto es atómico en el filesystem
    int fd = open(lock_path, O_WRONLY | O_CREAT | O_EXCL, 0644);
    if (fd == -1) {
        if (errno == EEXIST) {
            return -1;  // lock file existe — otro lo tiene
        }
        perror("open lock file");
        return -1;
    }

    // Escribir PID para diagnóstico
    char pid_str[32];
    int len = snprintf(pid_str, sizeof(pid_str), "%d\n", getpid());
    write(fd, pid_str, len);
    close(fd);
    return 0;
}

int release_lock_file(const char *lock_path) {
    return unlink(lock_path);
}
```

### Problema: stale lock files

Si el proceso muere sin borrar el lock file, queda un lock "fantasma":

```c
int acquire_or_break_stale(const char *lock_path) {
    // Intentar crear
    if (acquire_lock_file(lock_path) == 0) return 0;

    // Ya existe — verificar si el proceso sigue vivo
    int fd = open(lock_path, O_RDONLY);
    if (fd == -1) return -1;

    char buf[32];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    close(fd);

    if (n <= 0) {
        // Lock file vacío o ilegible — asumir stale
        unlink(lock_path);
        return acquire_lock_file(lock_path);
    }

    buf[n] = '\0';
    pid_t pid = atoi(buf);

    // kill con señal 0: verifica si el proceso existe
    if (kill(pid, 0) == -1 && errno == ESRCH) {
        // Proceso no existe — stale lock
        fprintf(stderr, "Eliminando stale lock (PID %d muerto)\n", pid);
        unlink(lock_path);
        return acquire_lock_file(lock_path);
    }

    // El proceso sigue vivo
    return -1;
}
```

### Lock file vs flock

```
  Lock file (O_EXCL)              flock
  ──────────────────────────────────────────────────────
  Visible con ls                  Invisible
  Sobrevive al crash (stale)      Kernel limpia en exit
  Funciona en NFS (casi siempre)  No funciona en NFS
  Race condition en stale check   Sin stale locks
  dpkg, apt, sendmail lo usan    daemons modernos
```

**Recomendación**: para daemons, prefiere `flock` (el kernel limpia
automáticamente). Para coordinación entre máquinas (NFS), lock files
con `O_EXCL` + detección de stale.

---

## 10. Errores comunes

### Error 1: asumir que fcntl locks persisten al cerrar otro fd

```c
// MAL: fcntl lock desaparece al cerrar fd_reader
int fd_writer = open("data.db", O_RDWR);
int fd_reader = open("data.db", O_RDONLY);

struct flock fl = { .l_type = F_WRLCK, .l_whence = SEEK_SET,
                    .l_start = 0, .l_len = 0 };
fcntl(fd_writer, F_SETLKW, &fl);  // lock exclusivo

// ... alguna biblioteca interna abre y cierra "data.db" ...
close(fd_reader);  // ¡LIBERA TODOS los locks del proceso!

write(fd_writer, data, len);  // ¡SIN PROTECCIÓN!

// BIEN: usar OFD locks, o asegurar que nadie más abra el archivo
```

### Error 2: no verificar errores de flock/fcntl

```c
// MAL: ignorar el retorno
flock(fd, LOCK_EX);
// Si falla (por ejemplo, fd inválido), operamos sin protección

// BIEN: siempre verificar
if (flock(fd, LOCK_EX) == -1) {
    perror("flock");
    close(fd);
    return -1;
}
```

### Error 3: deadlock entre flock y fcntl

```c
// MAL: mezclar mecanismos de locking en el mismo archivo
// flock y fcntl usan sistemas independientes en el kernel
// No se ven mutuamente

// Proceso A:
flock(fd, LOCK_EX);      // lock con flock

// Proceso B:
struct flock fl = { .l_type = F_WRLCK, ... };
fcntl(fd, F_SETLKW, &fl);  // lock con fcntl — NO espera a flock

// ¡Ambos "tienen" el lock! Los mecanismos son independientes

// BIEN: usar UN solo mecanismo consistentemente en todo el proyecto
```

### Error 4: lock file sin cleanup en señales

```c
// MAL: Ctrl+C deja el lock file huérfano
int main(void) {
    acquire_lock_file("/tmp/myapp.lock");
    // ... trabajo largo ...
    release_lock_file("/tmp/myapp.lock");
}

// BIEN: instalar handler para limpiar
static const char *g_lockfile = NULL;

void cleanup_handler(int sig) {
    if (g_lockfile) unlink(g_lockfile);
    _exit(128 + sig);
}

int main(void) {
    g_lockfile = "/tmp/myapp.lock";
    signal(SIGINT, cleanup_handler);
    signal(SIGTERM, cleanup_handler);

    acquire_lock_file(g_lockfile);
    // ... trabajo ...
    release_lock_file(g_lockfile);
    g_lockfile = NULL;
}

// AÚN MEJOR: usar flock en lugar de lock file
// El kernel libera flock automáticamente al terminar
```

### Error 5: upgrade de lock sin manejar la ventana de unlock

```c
// MAL: upgrade de SH a EX con flock puede soltar el lock
flock(fd, LOCK_SH);
// ... lectura ...

// Para hacer upgrade, flock libera SH y luego adquiere EX
// Hay una ventana donde NO tenemos ningún lock
flock(fd, LOCK_EX);
// Otro proceso pudo haber modificado el archivo entre SH→EX

// BIEN: releer después del upgrade
flock(fd, LOCK_SH);
read(fd, buf, size);

flock(fd, LOCK_EX);  // upgrade
// Re-leer para verificar que nada cambió
lseek(fd, 0, SEEK_SET);
read(fd, buf, size);
// Ahora sí, modificar con datos actualizados
```

Con `fcntl`, el upgrade de `F_RDLCK` a `F_WRLCK` es atómico
**solo si no hay conflicto**. Si otro proceso tiene `F_RDLCK`, el
upgrade bloquea y al despertar la lectura puede estar desactualizada.

---

## 11. Cheatsheet

```
  flock — ARCHIVO COMPLETO
  ──────────────────────────────────────────────────────────
  flock(fd, LOCK_SH)       lock compartido (lectura)
  flock(fd, LOCK_EX)       lock exclusivo (escritura)
  flock(fd, LOCK_UN)       desbloquear
  flock(fd, LOCK_EX|LOCK_NB)  no bloqueante (EWOULDBLOCK)
  Asociado a: open file description
  close() libera: solo si es el último fd de esa OFD

  fcntl — RANGO DE BYTES (POSIX)
  ──────────────────────────────────────────────────────────
  F_SETLK   + struct flock  establecer (no bloqueante)
  F_SETLKW  + struct flock  establecer (bloqueante)
  F_GETLK   + struct flock  consultar quién tiene lock
  l_type: F_RDLCK, F_WRLCK, F_UNLCK
  l_len = 0: hasta EOF
  Asociado a: proceso (PID)
  close() libera: TODOS los locks del proceso en ese archivo

  OFD LOCKS — RANGO DE BYTES (LINUX 3.15+)
  ──────────────────────────────────────────────────────────
  F_OFD_SETLK / F_OFD_SETLKW / F_OFD_GETLK
  l_pid DEBE ser 0
  Asociado a: open file description (no al proceso)
  close() libera: solo locks de esa OFD
  Thread-safe ✓

  lockf — WRAPPER SIMPLE
  ──────────────────────────────────────────────────────────
  lockf(fd, F_LOCK, len)   lock exclusivo (bloqueante)
  lockf(fd, F_TLOCK, len)  lock exclusivo (no bloqueante)
  lockf(fd, F_ULOCK, len)  desbloquear
  lockf(fd, F_TEST, len)   consultar
  Usa posición actual del cursor

  LOCK FILES
  ──────────────────────────────────────────────────────────
  open(path, O_CREAT|O_EXCL)  crear atómicamente
  unlink(path)                 liberar
  Sobrevive a crashes → verificar stale con kill(pid, 0)

  DECISIÓN RÁPIDA
  ──────────────────────────────────────────────────────────
  Lock simple de archivo → flock
  Lock por rangos        → OFD locks (o fcntl si necesitas
                           portabilidad fuera de Linux)
  Lock entre máquinas    → lock file con O_EXCL
  Nunca mezclar flock con fcntl en el mismo archivo
```

---

## 12. Ejercicios

### Ejercicio 1: escritura serializada a un log

Escribe un programa que lance N procesos hijo (con `fork`), cada uno
escribe 100 líneas a un archivo de log compartido. Sin locking, las
líneas se entremezclan. Implementa dos versiones: una sin lock (para
ver el problema) y otra con `flock`.

```c
// Esqueleto:
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <string.h>

void worker(int id, const char *logfile, int use_lock) {
    int fd = open(logfile, O_WRONLY | O_APPEND);
    if (fd == -1) { perror("open"); _exit(1); }

    for (int i = 0; i < 100; i++) {
        if (use_lock) {
            // Adquirir lock exclusivo
        }

        char line[128];
        int len = snprintf(line, sizeof(line),
                          "Worker %d: línea %03d — datos de prueba\n",
                          id, i);
        write(fd, line, len);

        if (use_lock) {
            // Liberar lock
        }
    }
    close(fd);
}
```

Requisitos:
- Crear 5 procesos hijo.
- Comparar el resultado con y sin lock.
- Verificar: ¿cuántas líneas tiene el archivo final? ¿Alguna está corrupta?

**Pregunta de reflexión**: en este caso, ¿realmente necesitas `flock`?
Dado que usas `O_APPEND`, cada `write` ya es atómico si la línea cabe
en `PIPE_BUF` (4096 bytes en Linux). ¿En qué escenario `O_APPEND` no
sería suficiente y necesitarías un lock explícito?

---

### Ejercicio 2: base de datos de registros con byte-range locks

Implementa un sistema simple de registros de longitud fija donde
múltiples procesos pueden leer y escribir registros diferentes
simultáneamente.

```c
// Esqueleto:
#define RECORD_SIZE 64

typedef struct {
    int id;
    char name[48];
    int value;
    int _padding;
} record_t;

// Funciones a implementar:
int db_read_record(int fd, int record_num, record_t *rec);
int db_write_record(int fd, int record_num, const record_t *rec);
int db_lock_record(int fd, int record_num, int exclusive);
int db_unlock_record(int fd, int record_num);
```

Requisitos:
- Usar `fcntl` (o OFD locks) para bloquear solo el registro accedido.
- `db_read_record` adquiere lock compartido, lee, desbloquea.
- `db_write_record` adquiere lock exclusivo, escribe, desbloquea.
- Programa de prueba con múltiples procesos leyendo/escribiendo
  registros diferentes en paralelo.

**Pregunta de reflexión**: si un proceso necesita leer el registro 5
y luego escribir el registro 10 basándose en lo leído, ¿basta con
bloquear cada registro individualmente? ¿Qué race condition podría
ocurrir entre el `unlock(5)` y el `lock(10)`? ¿Cómo lo resolverías?

---

### Ejercicio 3: daemon con PID file y detección de stale

Implementa un daemon que use un PID file con `flock` para garantizar
instancia única:

```c
// Esqueleto:
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/file.h>
#include <signal.h>

#define PIDFILE "/tmp/mydaemon.pid"

static int pidfile_fd = -1;

void cleanup(int sig) {
    // Liberar PID file
    // ¿Qué funciones son async-signal-safe?
}

int main(void) {
    // 1. Abrir/crear PID file
    // 2. Intentar flock no bloqueante
    // 3. Si falla: reportar PID existente y salir
    // 4. Truncar y escribir nuestro PID
    // 5. Instalar signal handlers para cleanup
    // 6. Bucle principal del daemon
    // 7. Cleanup al salir normalmente
}
```

Requisitos:
- Usar `flock` (no lock file con `O_EXCL`) para evitar stale locks.
- Escribir el PID en el archivo para diagnóstico.
- Limpiar correctamente en SIGTERM y SIGINT.
- Probar: lanzar dos instancias y verificar que la segunda falla.
- Probar: matar la primera con `kill -9` y verificar que la segunda
  puede arrancar (el kernel liberó el `flock`).

**Pregunta de reflexión**: ¿por qué `flock` es superior a un lock file
con `O_EXCL` para este caso? Piensa en qué pasa con cada mecanismo
cuando el daemon muere por `kill -9` (que no puede capturarse con un
signal handler).

---
