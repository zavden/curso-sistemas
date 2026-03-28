# strace avanzado

## Índice

1. [strace: más allá de lo básico](#strace-más-allá-de-lo-básico)
2. [Filtrar por syscall (-e)](#filtrar-por-syscall--e)
3. [Timing: medir duración de syscalls](#timing-medir-duración-de-syscalls)
4. [Conteo y estadísticas (-c)](#conteo-y-estadísticas--c)
5. [Follow forks (-f)](#follow-forks--f)
6. [Formato de salida avanzado](#formato-de-salida-avanzado)
7. [Attach a procesos en ejecución (-p)](#attach-a-procesos-en-ejecución--p)
8. [Filtrar por ruta y descriptor (-P, -e read/write)](#filtrar-por-ruta-y-descriptor)
9. [Inyección de fallos (fault injection)](#inyección-de-fallos-fault-injection)
10. [Patrones de diagnóstico](#patrones-de-diagnóstico)
11. [Errores comunes](#errores-comunes)
12. [Cheatsheet](#cheatsheet)
13. [Ejercicios](#ejercicios)

---

## strace: más allá de lo básico

`strace` intercepta y registra las **llamadas al sistema** (syscalls) que un programa realiza. Cada interacción con el kernel — abrir archivos, leer/escribir, crear procesos, sockets — pasa por syscalls, y strace las captura todas.

```
┌──────────────────────────────────────────────────────┐
│             Programa de usuario                      │
│  open(), read(), write(), mmap(), fork(), ...        │
├──────────────────────────────────────────────────────┤
│  ← strace intercepta AQUÍ (ptrace) →                │
├──────────────────────────────────────────────────────┤
│             Kernel de Linux                          │
│  sys_open(), sys_read(), sys_write(), ...            │
└──────────────────────────────────────────────────────┘
```

### Formato de cada línea

```
syscall(arg1, arg2, ...) = retorno
```

```bash
$ strace cat /etc/hostname 2>&1 | head -5
execve("/usr/bin/cat", ["cat", "/etc/hostname"], ...) = 0
brk(NULL)                               = 0x556789abc000
openat(AT_FDCWD, "/etc/hostname", O_RDONLY) = 3
read(3, "myhost\n", 131072)             = 7
write(1, "myhost\n", 7)                 = 7
```

Cada línea muestra: nombre de la syscall, argumentos (strace decodifica flags y structs), y el valor de retorno (`= -1 ENOENT` indica error con código errno).

---

## Filtrar por syscall (-e)

Sin filtro, strace genera cientos o miles de líneas. Los filtros son esenciales.

### Filtrar por nombre de syscall

```bash
# Solo open/openat
strace -e openat ./program

# Solo read y write
strace -e read,write ./program

# Solo llamadas de red
strace -e socket,connect,bind,listen,accept,sendto,recvfrom ./program
```

### Filtrar por categoría (trace=)

```bash
# Todas las syscalls de archivos
strace -e trace=file ./program
# Incluye: open, stat, access, unlink, rename, chmod, ...

# Todas las syscalls de red
strace -e trace=network ./program
# Incluye: socket, connect, bind, listen, accept, send, recv, ...

# Todas las syscalls de procesos
strace -e trace=process ./program
# Incluye: fork, clone, execve, wait, exit, ...

# Todas las syscalls de memoria
strace -e trace=memory ./program
# Incluye: mmap, munmap, mprotect, brk, ...

# Todas las syscalls de señales
strace -e trace=signal ./program
# Incluye: kill, sigaction, sigprocmask, ...

# Solo las que fallan
strace -e trace=file -z ./program    # solo exitosas (-z)
strace -e trace=file -Z ./program    # solo fallidas (-Z)
```

### Tabla de categorías

```
┌──────────────────┬──────────────────────────────────────┐
│ Categoría        │ Syscalls incluidas                   │
├──────────────────┼──────────────────────────────────────┤
│ trace=file       │ open, stat, access, unlink, rename,  │
│                  │ chmod, chown, link, symlink, mkdir,   │
│                  │ readlink, truncate, ...               │
├──────────────────┼──────────────────────────────────────┤
│ trace=network    │ socket, connect, bind, listen,       │
│                  │ accept, send, recv, shutdown, ...     │
├──────────────────┼──────────────────────────────────────┤
│ trace=process    │ fork, clone, execve, wait, exit,     │
│                  │ kill, ...                             │
├──────────────────┼──────────────────────────────────────┤
│ trace=memory     │ mmap, munmap, mprotect, brk,         │
│                  │ mremap, ...                           │
├──────────────────┼──────────────────────────────────────┤
│ trace=signal     │ kill, sigaction, sigprocmask,         │
│                  │ rt_sigaction, ...                     │
├──────────────────┼──────────────────────────────────────┤
│ trace=ipc        │ shmget, semget, msgget, ...           │
├──────────────────┼──────────────────────────────────────┤
│ trace=desc       │ read, write, close, dup, select,     │
│                  │ poll, epoll, ...                      │
└──────────────────┴──────────────────────────────────────┘
```

### Negar filtros

```bash
# Todo EXCEPTO read y write (reducir ruido)
strace -e trace=\!read,write ./program

# Todo excepto llamadas de memoria
strace -e trace=\!memory ./program
```

---

## Timing: medir duración de syscalls

### -T: tiempo de cada syscall

```bash
$ strace -T -e read,write cat /etc/hostname
read(3, "myhost\n", 131072)    = 7 <0.000015>
write(1, "myhost\n", 7)        = 7 <0.000008>
```

El valor entre `< >` es el tiempo en segundos que la syscall tardó dentro del kernel. Útil para detectar I/O lento.

### -t, -tt, -ttt: timestamps

```bash
# -t: hora del día (segundos)
$ strace -t -e openat cat /etc/hostname
14:30:15 openat(AT_FDCWD, "/etc/hostname", O_RDONLY) = 3

# -tt: hora del día (microsegundos)
$ strace -tt -e openat cat /etc/hostname
14:30:15.123456 openat(AT_FDCWD, "/etc/hostname", O_RDONLY) = 3

# -ttt: epoch (segundos.microsegundos desde 1970)
$ strace -ttt -e openat cat /etc/hostname
1711374615.123456 openat(AT_FDCWD, "/etc/hostname", O_RDONLY) = 3
```

### -r: tiempo relativo entre syscalls

```bash
$ strace -r -e read,write cat /etc/hostname
     0.000000 read(3, "myhost\n", 131072) = 7
     0.000032 write(1, "myhost\n", 7)     = 7
     0.000018 read(3, "", 131072)          = 0
```

El número es el tiempo transcurrido **desde la syscall anterior**. Ideal para encontrar qué operación introduce la latencia.

### Combinar -T y -tt

```bash
# Timestamp absoluto + duración de cada syscall
$ strace -tt -T -e trace=network ./my_server 2>trace.log

# En trace.log:
14:30:15.100000 connect(3, {sa_family=AF_INET, sin_port=5432, ...}) = 0 <0.003456>
14:30:15.104000 sendto(3, "SELECT * FROM...", 42, ...) = 42 <0.000012>
14:30:15.104100 recvfrom(3, "...", 4096, ...)          = 256 <0.045678>
#                                                              ^^^^^^^^
#                                          ¡La DB tardó 45ms en responder!
```

---

## Conteo y estadísticas (-c)

### -c: resumen al final

```bash
$ strace -c ls /tmp
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- --------
 30.42    0.000142          14        10           read
 25.11    0.000117          11        10         1 openat
 15.67    0.000073           7        10           close
 10.93    0.000051           5        10           fstat
  8.35    0.000039           4         9           mmap
  4.92    0.000023          11         2           write
  ...
------ ----------- ----------- --------- --------- --------
100.00    0.000467                    72         3 total
```

Muestra:
- `% time`: porcentaje del tiempo total en cada syscall.
- `seconds`: tiempo total en esa syscall.
- `usecs/call`: promedio por llamada.
- `calls`: número de invocaciones.
- `errors`: cuántas retornaron error.

### -C: resumen + traza detallada

```bash
# -C = combina -c con la traza completa
$ strace -C -e trace=file ls /tmp
# Primero la traza detallada...
openat(AT_FDCWD, "/tmp", O_RDONLY|O_NONBLOCK|O_DIRECTORY) = 3
# ...luego el resumen al final
```

### -S: ordenar el resumen

```bash
# Ordenar por tiempo total (default)
strace -c -S time ls /tmp

# Ordenar por número de llamadas
strace -c -S calls ls /tmp

# Ordenar por nombre de syscall
strace -c -S name ls /tmp

# Ordenar por errores
strace -c -S errors ls /tmp
```

### Comparar dos implementaciones

```bash
# ¿Cuál hace menos syscalls?
$ strace -c ./program_v1 2>&1 | tail -3
100.00    0.005432                   850         5 total

$ strace -c ./program_v2 2>&1 | tail -3
100.00    0.001234                   120         2 total

# program_v2 hace 7x menos syscalls → probablemente más eficiente
```

---

## Follow forks (-f)

### -f: seguir procesos hijo

```bash
# Sin -f: solo ves el proceso padre
$ strace ./program_that_forks
clone(...)                              = 12345
# El hijo (PID 12345) no se traza

# Con -f: trazas padre E hijos
$ strace -f ./program_that_forks
[pid 12344] clone(...) = 12345
[pid 12345] execve("/bin/ls", ["ls", "-la"], ...) = 0
[pid 12345] openat(AT_FDCWD, ".", ...) = 3
[pid 12344] wait4(12345, ...) = 12345
```

Cada línea tiene `[pid N]` para distinguir qué proceso hizo la syscall.

### -ff con -o: un archivo por proceso

```bash
# Cada PID escribe en su propio archivo
$ strace -ff -o trace ./my_server

# Genera:
# trace.12344  (proceso principal)
# trace.12345  (primer hijo)
# trace.12346  (segundo hijo)
```

### Ejemplo: trazar un script de shell

```bash
$ strace -f -e trace=process bash -c 'echo hello; ls /tmp'
execve("/usr/bin/bash", ["bash", "-c", "echo hello; ls /tmp"], ...) = 0
[pid 1234] clone(...) = 1235
[pid 1235] execve("/usr/bin/echo", ["echo", "hello"], ...) = 0
[pid 1235] exit_group(0)                 = ?
[pid 1234] wait4(-1, ...)                = 1235
[pid 1234] clone(...) = 1236
[pid 1236] execve("/usr/bin/ls", ["ls", "/tmp"], ...) = 0
[pid 1236] exit_group(0)                 = ?
```

> **Predicción**: si ejecutas `strace -f -e trace=process bash -c 'ls | wc -l'`, ¿cuántos `clone` verás? Al menos 2: uno para `ls` y otro para `wc` (el pipe requiere dos procesos).

---

## Formato de salida avanzado

### -o: escribir a archivo

```bash
# Escribir traza a archivo (stderr queda libre para el programa)
strace -o trace.log ./program

# El programa imprime a stdout/stderr normalmente
# La traza va a trace.log
```

### -s N: longitud máxima de strings

```bash
# Default: strings truncados a 32 caracteres
$ strace -e write ./program
write(1, "This is a very long message t"..., 80) = 80

# Mostrar strings más largos
$ strace -s 200 -e write ./program
write(1, "This is a very long message that we want to see in full", 80) = 80

# Mostrar strings completos (sin límite práctico)
$ strace -s 9999 -e write ./program
```

### -v: verbose (no abreviar structs)

```bash
# Sin -v: structs abreviados
$ strace -e stat ls
stat("/etc", {st_mode=S_IFDIR|0755, st_size=4096, ...}) = 0

# Con -v: structs completos
$ strace -v -e stat ls
stat("/etc", {st_dev=makedev(0x8, 0x2), st_ino=123456,
  st_mode=S_IFDIR|0755, st_nlink=152, st_uid=0, st_gid=0,
  st_blksize=4096, st_blocks=8, st_size=4096,
  st_atime=2024/03/15-10:30:00, st_mtime=2024/03/14-08:00:00,
  st_ctime=2024/03/14-08:00:00}) = 0
```

### -x, -xx: hexadecimal

```bash
# -x: strings no-ASCII en hex
$ strace -x -e write ./binary_writer
write(1, "\x00\x01\x02hello\xff", 9) = 9

# -xx: todo en hex
$ strace -xx -e write ./binary_writer
write(1, "\x68\x65\x6c\x6c\x6f", 5) = 5
```

### -y: mostrar paths de file descriptors

```bash
# Sin -y: solo números de fd
$ strace -e read ./program
read(3, "data...", 4096) = 100
# ¿Qué es fd 3?

# Con -y: path del archivo asociado al fd
$ strace -y -e read ./program
read(3</etc/config.ini>, "data...", 4096) = 100
# ¡fd 3 es /etc/config.ini!

# -yy: aún más detalle (socket info)
$ strace -yy -e recvfrom ./server
recvfrom(4<TCP:[127.0.0.1:8080->127.0.0.1:54321]>, ...) = 100
```

---

## Attach a procesos en ejecución (-p)

### Attach y detach

```bash
# Encontrar el PID
$ pidof my_server
12345

# Attach
$ strace -p 12345
# strace comienza a mostrar las syscalls del proceso
# El proceso NO se detiene (a diferencia de GDB)

# Ctrl+C para dejar de trazar (el proceso sigue corriendo)
```

### Attach con filtros

```bash
# Solo ver I/O de red de un servidor en producción
strace -p 12345 -e trace=network -tt -T

# Ver qué archivos está abriendo
strace -p 12345 -e trace=file -y

# Estadísticas de un proceso durante 10 segundos
timeout 10 strace -p 12345 -c
```

### Attach a todos los threads

```bash
# -f con -p sigue los threads existentes y nuevos
strace -f -p 12345

# Cada thread tiene su [pid]:
[pid 12345] epoll_wait(5, ...) = 1
[pid 12346] read(7, "GET / HTTP/1.1\r\n", 4096) = 50
[pid 12347] write(8, "HTTP/1.1 200 OK\r\n", 17) = 17
```

---

## Filtrar por ruta y descriptor

### -P: filtrar por path

```bash
# Solo syscalls que involucren /etc/passwd
strace -P /etc/passwd ./program
openat(AT_FDCWD, "/etc/passwd", O_RDONLY) = 3
read(3, "root:x:0:0:...", 4096) = 2345
close(3) = 0

# Múltiples paths
strace -P /etc/passwd -P /etc/shadow ./program
```

### -e read=fd y -e write=fd: volcar datos

```bash
# Volcar todo lo que se lee del fd 3
strace -e read=3 ./program
read(3, "data content here", 4096) = 17
 | 00000  64 61 74 61 20 63 6f 6e  74 65 6e 74 20 68 65 72  data con tent her |
 | 00010  65                                                  e                |

# Volcar todo lo que se escribe a stdout (fd 1)
strace -e write=1 ./program

# Volcar lectura y escritura de todos los fds
strace -e read=all -e write=all ./program
```

---

## Inyección de fallos (fault injection)

strace puede **inyectar errores** en syscalls específicas para simular condiciones de fallo. Invaluable para testing de manejo de errores.

### Sintaxis básica

```bash
# Hacer que open falle con ENOENT (archivo no encontrado)
strace -e inject=openat:error=ENOENT ./program

# Hacer que malloc falle (brk/mmap retornan error)
strace -e inject=brk:error=ENOMEM ./program

# Hacer que connect falle
strace -e inject=connect:error=ECONNREFUSED ./program
```

### Controlar cuándo inyectar

```bash
# Solo fallar la 3ra invocación de openat
strace -e inject=openat:error=ENOENT:when=3 ./program

# Fallar las invocaciones 3 a 5
strace -e inject=openat:error=ENOENT:when=3..5 ./program

# Fallar a partir de la 2da invocación
strace -e inject=openat:error=ENOENT:when=2+ ./program

# Fallar 1 de cada 3 invocaciones (probabilístico)
strace -e inject=openat:error=ENOENT:when=1+3 ./program
```

### Inyectar delay

```bash
# Añadir 100ms de delay a cada read
strace -e inject=read:delay_enter=100000 ./program

# Delay solo al retornar
strace -e inject=write:delay_exit=50000 ./program
```

### Ejemplo: probar que tu programa maneja ENOSPC

```bash
# Simular disco lleno en la 5ta escritura
$ strace -e inject=write:error=ENOSPC:when=5 ./backup_tool
# ¿Tu programa muestra un mensaje de error claro?
# ¿Libera recursos correctamente?
# ¿Sale con código de error apropiado?
```

```
┌───────────────────────────────────────────────────────┐
│        Fault injection: errores útiles para testing   │
├───────────────────────┬───────────────────────────────┤
│ Error                 │ Simula                        │
├───────────────────────┼───────────────────────────────┤
│ ENOENT                │ Archivo no encontrado         │
│ EACCES / EPERM        │ Sin permisos                  │
│ ENOSPC                │ Disco lleno                   │
│ ENOMEM                │ Sin memoria                   │
│ ECONNREFUSED          │ Servidor no disponible        │
│ ETIMEDOUT             │ Timeout de conexión           │
│ EIO                   │ Error de I/O en disco         │
│ EMFILE                │ Demasiados archivos abiertos  │
│ EAGAIN                │ Recurso temporalmente no disp.│
└───────────────────────┴───────────────────────────────┘
```

---

## Patrones de diagnóstico

### ¿Por qué mi programa no encuentra un archivo?

```bash
$ strace -e trace=file -y ./program 2>&1 | grep -i "no such\|enoent"
openat(AT_FDCWD, "/wrong/path/config.toml", O_RDONLY) = -1 ENOENT
# ¡Está buscando en /wrong/path/ en vez de /correct/path/!
```

### ¿Por qué mi programa es lento?

```bash
# Paso 1: vista general de syscalls
$ strace -c ./slow_program
% time     seconds  usecs/call     calls  syscall
 92.30    0.850000        8500       100  nanosleep
#         ^^^^^^^^                   ^^^
# 850ms en 100 llamadas a nanosleep → alguien puso sleep en un loop

# Paso 2: ver con timing dónde tarda
$ strace -tt -T -e nanosleep ./slow_program
14:30:15.100000 nanosleep({tv_sec=0, tv_nsec=10000000}, NULL) = 0 <0.010234>
14:30:15.110300 nanosleep({tv_sec=0, tv_nsec=10000000}, NULL) = 0 <0.010156>
# 10ms sleep en cada iteración × 100 = 1 segundo perdido
```

### ¿Qué archivos de configuración lee mi programa?

```bash
$ strace -e openat -y ./program 2>&1 | grep -v "\.so\|/proc\|/sys"
openat(AT_FDCWD, "/home/user/.config/myapp/config.toml", O_RDONLY) = 3
openat(AT_FDCWD, "/etc/myapp/defaults.conf", O_RDONLY) = -1 ENOENT
openat(AT_FDCWD, "/home/user/.myapprc", O_RDONLY) = 4
```

### ¿Por qué mi servidor no acepta conexiones?

```bash
$ strace -e trace=network -p $(pidof my_server)
socket(AF_INET, SOCK_STREAM, IPPROTO_TCP) = 3
bind(3, {sa_family=AF_INET, sin_port=htons(8080), sin_addr=inet_addr("127.0.0.1")}, 16) = -1 EADDRINUSE
# ¡El puerto 8080 ya está en uso!
```

### ¿Hay memory leaks (muchos mmap sin munmap)?

```bash
$ strace -e mmap,munmap,brk -c ./program
% time     seconds  usecs/call     calls  syscall
 60.00    0.000300          3       100    mmap
 30.00    0.000150          3        50    munmap
# 100 mmap pero solo 50 munmap → posible leak
```

---

## Errores comunes

### 1. Olvidar que strace escribe a stderr

```bash
# INCORRECTO: redirigir solo stdout
strace ls /tmp > output.txt
# output.txt está vacío, la traza va a la terminal (stderr)

# CORRECTO: redirigir stderr o usar -o
strace ls /tmp 2> trace.log        # traza a archivo
strace -o trace.log ls /tmp        # forma preferida
strace ls /tmp 2>&1 | grep openat  # combinar para grep
```

### 2. No usar -f con programas que hacen fork

```bash
# INCORRECTO: solo ves el proceso padre
strace ./my_server
# No ves las syscalls de los workers

# CORRECTO: seguir hijos
strace -f ./my_server
```

### 3. No usar -y para identificar file descriptors

```bash
# INCORRECTO: ¿qué es fd 7?
read(7, "data", 4096) = 4

# CORRECTO: con -y ves el archivo
read(7</var/log/app.log>, "data", 4096) = 4
```

### 4. Trazar TODO sin filtrar (demasiado ruido)

```bash
# INCORRECTO: miles de líneas, imposible encontrar algo
strace ./complex_program 2>trace.log
wc -l trace.log
# 50000 líneas

# CORRECTO: filtrar por lo que buscas
strace -e trace=file ./complex_program 2>trace.log
wc -l trace.log
# 200 líneas — manejable
```

### 5. No considerar el overhead de strace

```bash
# strace añade overhead significativo (ptrace tiene coste)
# No medir performance con strace activo

# Para medir performance de syscalls, usar -c (resumen)
# que tiene menos overhead que la traza completa
strace -c ./program  # mejor que strace -o trace.log ./program

# O usar perf para mediciones de performance (sin ptrace overhead)
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║            STRACE AVANZADO — REFERENCIA RÁPIDA            ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  FILTRAR                                                  ║
║  ───────                                                  ║
║  -e SYSCALL              solo esa syscall                ║
║  -e trace=file           categoría: archivos             ║
║  -e trace=network        categoría: red                  ║
║  -e trace=process        categoría: procesos             ║
║  -e trace=memory         categoría: memoria              ║
║  -e trace=desc           categoría: descriptores         ║
║  -e trace=\!read,write   excluir syscalls                ║
║  -Z                      solo syscalls fallidas          ║
║  -z                      solo syscalls exitosas          ║
║  -P /path                solo operaciones en ese path    ║
║                                                           ║
║  TIMING                                                   ║
║  ──────                                                   ║
║  -T                      duración de cada syscall        ║
║  -t / -tt / -ttt         timestamp (s / μs / epoch)      ║
║  -r                      tiempo relativo entre syscalls  ║
║                                                           ║
║  ESTADÍSTICAS                                             ║
║  ────────────                                             ║
║  -c                      resumen al final                ║
║  -C                      resumen + traza completa        ║
║  -S time|calls|name      ordenar resumen                 ║
║                                                           ║
║  PROCESOS                                                 ║
║  ────────                                                 ║
║  -f                      seguir fork/clone               ║
║  -ff -o PREFIX           un archivo por PID              ║
║  -p PID                  attach a proceso existente      ║
║                                                           ║
║  FORMATO                                                  ║
║  ───────                                                  ║
║  -o FILE                 salida a archivo                ║
║  -s N                    longitud máx de strings (def:32)║
║  -v                      verbose (structs completos)     ║
║  -y / -yy                mostrar paths/socket info en fd ║
║  -x / -xx                hex para no-ASCII / todo hex    ║
║  -e read=FD              volcar datos leídos             ║
║  -e write=FD             volcar datos escritos            ║
║                                                           ║
║  FAULT INJECTION                                          ║
║  ───────────────                                          ║
║  -e inject=SYS:error=ERR         inyectar error          ║
║  -e inject=SYS:error=ERR:when=N  solo la N-ésima        ║
║  -e inject=SYS:delay_enter=μs    inyectar latencia      ║
║                                                           ║
║  PATRONES COMUNES                                         ║
║  ────────────────                                         ║
║  strace -e trace=file -y ./prog   ¿qué archivos abre?   ║
║  strace -c ./prog                 perfil de syscalls     ║
║  strace -tt -T -e network -p PID  debug servidor         ║
║  strace -e inject=open:error=ENOENT  test error handling ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Diagnosticar un programa "lento"

Crea este programa que es intencionalmente lento:

```c
// slow.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

void process_file(const char *path) {
    for (int i = 0; i < 50; i++) {
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        char buf[1];
        // Lee byte a byte (ineficiente)
        while (read(fd, buf, 1) == 1) {
            // procesar...
        }
        close(fd);
        usleep(10000); // 10ms sleep innecesario
    }
}

int main() {
    process_file("/etc/hostname");
    return 0;
}
```

Usa strace para:
1. Identificar cuántas syscalls `read` se ejecutan (`-c`).
2. Medir cuánto tiempo se gasta en `nanosleep` vs `read` (`-c -S time`).
3. Verificar que el archivo se abre 50 veces (`-e openat -c`).
4. Medir el tiempo entre operaciones (`-r -e read,nanosleep`).

**Pregunta de reflexión**: ¿Cómo optimizarías este programa? ¿Cuántas syscalls se ahorraría leyendo en bloques de 4096 bytes en vez de 1 byte? Calcula el ratio.

---

### Ejercicio 2: Encontrar por qué un programa falla

Crea este programa que falla misteriosamente:

```c
// mystery.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    const char *config_paths[] = {
        "./config.toml",
        "~/.config/myapp/config.toml",  // bug: ~ no se expande
        "/etc/myapp/config.toml",
        NULL
    };

    FILE *config = NULL;
    for (int i = 0; config_paths[i]; i++) {
        config = fopen(config_paths[i], "r");
        if (config) break;
    }

    if (!config) {
        fprintf(stderr, "Error: cannot find config file\n");
        return 1;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), config)) {
        printf("%s", buf);
    }

    fclose(config);
    return 0;
}
```

Usa strace para:
1. Ver exactamente qué paths intenta abrir (`-e openat -v`).
2. Identificar el bug (pista: `~` no funciona como esperas).
3. Ver los códigos de error de cada intento (`-e openat`).
4. Crear el archivo de config en uno de los paths y verificar que funciona.

**Pregunta de reflexión**: ¿Por qué `~` no se expande en `fopen`? ¿Quién es responsable de expandir `~` en un sistema Unix — el kernel, la librería C, o el shell?

---

### Ejercicio 3: Fault injection para testing

Usa el programa de tu elección (o crea uno simple que lea un archivo, procese datos, y escriba resultados) y aplica fault injection para verificar que maneja errores correctamente:

1. **Simular archivo no encontrado**:
   ```bash
   strace -e inject=openat:error=ENOENT ./program
   ```
   ¿El programa muestra un mensaje de error útil o hace segfault?

2. **Simular disco lleno**:
   ```bash
   strace -e inject=write:error=ENOSPC:when=3+ ./program
   ```
   ¿El programa detecta que la escritura falló?

3. **Simular error de lectura**:
   ```bash
   strace -e inject=read:error=EIO:when=2 ./program
   ```
   ¿El programa maneja el error de I/O?

4. **Simular latencia extrema**:
   ```bash
   strace -e inject=read:delay_enter=500000 ./program
   ```
   ¿El programa tiene timeout o se queda colgado?

Documenta el comportamiento del programa en cada escenario. Si falla sin mensaje claro, mejora el manejo de errores.

**Pregunta de reflexión**: ¿Podrías lograr lo mismo que fault injection sin strace? ¿Qué alternativas existen (LD_PRELOAD, mocks, kernel namespaces)?
