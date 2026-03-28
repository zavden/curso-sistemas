# strace: Trazar Syscalls de un Programa

## Índice

1. [Qué es strace y por qué importa](#1-qué-es-strace-y-por-qué-importa)
2. [Uso básico: trazar un comando](#2-uso-básico-trazar-un-comando)
3. [Leer la salida de strace](#3-leer-la-salida-de-strace)
4. [Adjuntarse a un proceso existente: -p](#4-adjuntarse-a-un-proceso-existente--p)
5. [Filtrar syscalls: -e trace=](#5-filtrar-syscalls--e-trace)
6. [Timing: -t, -T, -r](#6-timing--t--t--r)
7. [Estadísticas: -c y -C](#7-estadísticas--c-y--c)
8. [Seguir procesos hijos: -f](#8-seguir-procesos-hijos--f)
9. [Salida a archivo: -o](#9-salida-a-archivo--o)
10. [Strings y buffers: -s y -x](#10-strings-y-buffers--s-y--x)
11. [Debugging con strace: casos reales](#11-debugging-con-strace-casos-reales)
12. [Errores comunes](#12-errores-comunes)
13. [Cheatsheet](#13-cheatsheet)
14. [Ejercicios](#14-ejercicios)

---

## 1. Qué es strace y por qué importa

`strace` intercepta y registra **todas las syscalls** que un programa hace. Es la herramienta de debugging más poderosa para entender qué hace un programa a nivel de sistema — sin necesidad de tener el código fuente.

```
 Sin strace:                     Con strace:
┌──────────────┐              ┌──────────────┐
│ ./programa   │              │ strace       │
│              │              │ ./programa   │
│ "Segfault"   │              │              │
│ ???          │              │ open("/etc/  │
│              │              │  config",    │
│              │              │  O_RDONLY)    │
│              │              │  = -1 ENOENT │
│              │              │ ← ¡ahí está  │
│              │              │   el problema!│
└──────────────┘              └──────────────┘
```

Casos de uso:
- **"¿Por qué falla este programa?"** — strace muestra qué archivo no encuentra, qué permiso falta, qué socket no conecta.
- **"¿Qué archivos abre este programa?"** — filtrar por `open`/`openat`.
- **"¿Por qué es lento?"** — ver dónde pasa tiempo el programa (I/O, sleeps, locks).
- **"¿Qué hace este binario?"** — entender programas sin código fuente.

---

## 2. Uso básico: trazar un comando

```bash
strace ls /tmp
```

```
execve("/usr/bin/ls", ["ls", "/tmp"], 0x7ffc... /* 58 vars */) = 0
brk(NULL)                               = 0x55f3a8c33000
mmap(NULL, 8192, PROT_READ|PROT_WRITE, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0) = 0x7f...
access("/etc/ld.so.preload", R_OK)      = -1 ENOENT (No such file or directory)
openat(AT_FDCWD, "/etc/ld.so.cache", O_RDONLY|O_CLOEXEC) = 3
...
openat(AT_FDCWD, "/tmp", O_RDONLY|O_NONBLOCK|O_CLOEXEC|O_DIRECTORY) = 3
newfstatat(3, "", {st_mode=S_IFDIR|S_ISVTX|0777, st_size=4096, ...}, AT_EMPTY_PATH) = 0
getdents64(3, /* 5 entries */, 32768)   = 152
getdents64(3, /* 0 entries */, 32768)   = 0
close(3)                                = 0
...
write(1, "file1.txt  file2.txt\n", 21)  = 21
close(1)                                = 0
exit_group(0)                           = ?
+++ exited with 0 +++
```

Cada línea es una syscall. El flujo visible: el programa carga bibliotecas dinámicas, abre `/tmp` como directorio, lee las entradas con `getdents64`, escribe el resultado a stdout con `write`, y termina.

### strace va a stderr

La salida de strace se envía a **stderr**, y la salida del programa va a stdout. Así no se mezclan:

```bash
# La salida de ls va a stdout, strace va a stderr
strace ls /tmp 2>strace.log
# stdout: file1.txt  file2.txt
# strace.log: contiene la traza
```

---

## 3. Leer la salida de strace

Cada línea tiene un formato consistente:

```
openat(AT_FDCWD, "/etc/hosts", O_RDONLY|O_CLOEXEC) = 3
│       │          │             │                    │
│       │          │             │                    └─ Valor de retorno
│       │          │             └─ Flags (OR de constantes)
│       │          └─ Argumento string (entre comillas)
│       └─ Argumento constante
└─ Nombre de la syscall
```

### Retornos exitosos

```
openat(AT_FDCWD, "/etc/hosts", O_RDONLY) = 3       ← fd 3
read(3, "127.0.0.1 localhost\n...", 4096) = 221    ← 221 bytes leídos
close(3)                                  = 0       ← éxito
write(1, "hello\n", 6)                    = 6       ← 6 bytes escritos
mmap(NULL, 4096, ...) = 0x7f8a12345000              ← dirección de memoria
fork()                = 1234                         ← PID del hijo
```

### Retornos con error

```
openat(AT_FDCWD, "/no/existe", O_RDONLY) = -1 ENOENT (No such file or directory)
│                                           │  │       │
│                                           │  │       └─ Descripción legible
│                                           │  └─ Nombre del errno
│                                           └─ Retorno -1 (error)

connect(3, {sa_family=AF_INET, sin_port=htons(8080), sin_addr=inet_addr("192.168.1.100")}, 16) = -1 ECONNREFUSED (Connection refused)
```

strace traduce automáticamente los valores numéricos a constantes legibles: `O_RDONLY` en vez de `0`, `ENOENT` en vez de `2`, `AF_INET` en vez de `2`.

### Structs y datos complejos

```
newfstatat(3, "", {st_mode=S_IFREG|0644, st_size=221, ...}, AT_EMPTY_PATH) = 0
│                  │
│                  └─ struct stat resumido (strace muestra los campos relevantes)

recvfrom(3, "HTTP/1.1 200 OK\r\n...", 4096, 0, NULL, NULL) = 348
│          │
│          └─ Buffer leído (strace muestra el contenido como string)

poll([{fd=3, events=POLLIN}], 1, 5000) = 1 ([{fd=3, revents=POLLIN}])
│    │                                       │
│    └─ Array de struct pollfd               └─ Resultado
```

### Syscalls que no retornan

```
exit_group(0)                           = ?
+++ exited with 0 +++
```

`exit_group` no retorna — el proceso termina. strace muestra `?` como retorno.

### Señales

```
--- SIGCHLD {si_signo=SIGCHLD, si_code=CLD_EXITED, si_pid=1234, si_status=0} ---
```

strace también muestra las señales recibidas por el proceso, con detalle de quién la envió y por qué.

---

## 4. Adjuntarse a un proceso existente: -p

```bash
# Trazar un proceso que ya está corriendo
sudo strace -p 1234

# Trazar por nombre (si hay un solo proceso con ese nombre)
sudo strace -p $(pidof nginx)
```

`-p` es esencial para diagnosticar programas que ya están corriendo y presentan problemas. Requiere `CAP_SYS_PTRACE` (generalmente root).

```bash
# Ejemplo: ¿por qué nginx está lento?
sudo strace -p $(pidof nginx) -e trace=network -T
# Muestra las syscalls de red con tiempo de cada una
```

Presiona `Ctrl+C` para detener la traza sin matar el proceso.

---

## 5. Filtrar syscalls: -e trace=

Sin filtros, strace muestra cientos de líneas. Los filtros reducen la salida a lo relevante:

### Por categoría

```bash
# Solo syscalls de archivos
strace -e trace=file ls /tmp
# openat, newfstatat, access, readlink...

# Solo syscalls de red
strace -e trace=network curl -s http://example.com
# socket, connect, sendto, recvfrom...

# Solo syscalls de procesos
strace -e trace=process bash -c "ls | wc -l"
# clone, execve, wait4, exit_group...

# Solo syscalls de memoria
strace -e trace=memory ./programa
# mmap, munmap, mprotect, brk...

# Solo syscalls de señales
strace -e trace=signal ./programa
# rt_sigaction, rt_sigprocmask, kill...

# Solo I/O de descriptores
strace -e trace=desc ./programa
# read, write, close, fcntl, dup2, poll...
```

### Por nombre específico

```bash
# Solo open y close
strace -e trace=openat,close ls /tmp

# Solo read y write
strace -e trace=read,write cat /etc/hosts

# Excluir syscalls específicas (mostrar todo MENOS estas)
strace -e trace=!mmap,!mprotect,!brk ls /tmp
```

### Categorías disponibles

| Categoría | Syscalls incluidas |
|-----------|-------------------|
| `file` | open, stat, chmod, chown, link, unlink, rename, mkdir... |
| `process` | fork, clone, execve, wait, exit... |
| `network` | socket, bind, listen, accept, connect, send, recv... |
| `signal` | sigaction, sigprocmask, kill, sigreturn... |
| `ipc` | shmget, semget, msgget, pipe... |
| `desc` | read, write, close, dup, fcntl, poll, select... |
| `memory` | mmap, munmap, mprotect, brk, madvise... |
| `%stat` | stat, fstat, lstat, newfstatat... (atajo) |
| `%open` | open, openat... (atajo) |

### Filtrar por retorno

```bash
# Solo syscalls que fallaron
strace -e status=failed ls /no/existe

# Solo syscalls que retornaron un error específico
strace -e status=failed -e trace=openat ./programa
# Muestra solo los openat que fallaron
```

---

## 6. Timing: -t, -T, -r

### -t: timestamp absoluto

```bash
strace -t ls /tmp
```

```
14:30:15 openat(AT_FDCWD, "/tmp", O_RDONLY|O_DIRECTORY) = 3
14:30:15 getdents64(3, /* 5 entries */, 32768) = 152
14:30:15 write(1, "file1.txt  file2.txt\n", 21) = 21
```

Variantes:
- `-t`: hora con segundos (`14:30:15`).
- `-tt`: hora con microsegundos (`14:30:15.123456`).
- `-ttt`: timestamp Unix (`1711028415.123456`).

### -T: duración de cada syscall

```bash
strace -T cat /etc/hosts
```

```
openat(AT_FDCWD, "/etc/hosts", O_RDONLY) = 3 <0.000012>
read(3, "127.0.0.1 localhost\n...", 131072) = 221 <0.000008>
write(1, "127.0.0.1 localhost\n...", 221) = 221 <0.000015>
close(3)                                 = 0 <0.000006>
```

El número entre `<>` al final es el tiempo que la syscall pasó dentro del kernel, en segundos. Útil para identificar qué syscall es lenta.

### -r: tiempo relativo entre syscalls

```bash
strace -r cat /etc/hosts
```

```
     0.000000 execve("/usr/bin/cat", ["cat", "/etc/hosts"], ...) = 0
     0.000412 brk(NULL) = 0x55f3...
     0.000035 mmap(NULL, 8192, ...) = 0x7f...
     ...
     0.000089 openat(AT_FDCWD, "/etc/hosts", O_RDONLY) = 3
     0.000028 read(3, "127.0.0.1...", 131072) = 221
     0.000019 write(1, "127.0.0.1...", 221) = 221
```

El número al inicio es el tiempo transcurrido desde la syscall anterior. Gaps grandes indican dónde el programa está esperando o procesando.

### Combinar opciones de timing

```bash
# Timestamp absoluto + duración de cada syscall
strace -tt -T -e trace=read,write cat /etc/hosts
```

```
14:30:15.123456 read(3, "127.0.0.1 localhost\n...", 131072) = 221 <0.000008>
14:30:15.123489 write(1, "127.0.0.1 localhost\n...", 221) = 221 <0.000015>
```

---

## 7. Estadísticas: -c y -C

### -c: resumen estadístico

En vez de mostrar cada syscall, `-c` muestra un resumen al final:

```bash
strace -c ls /tmp
```

```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 25.00    0.000050          10         5           close
 20.00    0.000040          13         3           openat
 15.00    0.000030           8         4           newfstatat
 10.00    0.000020          20         1           write
 10.00    0.000020          10         2           getdents64
  5.00    0.000010          10         1           read
  5.00    0.000010           5         2           mmap
  ...
------ ----------- ----------- --------- --------- ----------------
100.00    0.000200                    30         3 total
```

Columnas:
- **% time**: porcentaje del tiempo total en esa syscall.
- **seconds**: tiempo total acumulado.
- **usecs/call**: microsegundos promedio por invocación.
- **calls**: número de veces que se invocó.
- **errors**: cuántas veces retornó error.

### -C: resumen + traza individual

```bash
strace -C ls /tmp
# Muestra cada syscall Y el resumen al final
# Útil cuando quieres ver los detalles pero también el panorama general
```

### Combinar -c con filtros

```bash
# Estadísticas solo de syscalls de archivo
strace -c -e trace=file ls /tmp

# Estadísticas de un programa largo (adjuntándose por 10 segundos)
timeout 10 strace -c -p $(pidof nginx) 2>&1
```

---

## 8. Seguir procesos hijos: -f

Por defecto, strace solo traza el proceso principal. Si el programa hace `fork()` o `clone()`, los hijos no se trazan. `-f` los incluye:

```bash
# Sin -f: solo el shell
strace bash -c "ls | wc -l"
# Solo ves las syscalls de bash, no de ls ni wc

# Con -f: todos los procesos
strace -f bash -c "ls | wc -l"
```

```
[pid  1000] clone(child_stack=NULL, ...) = 1001
[pid  1000] clone(child_stack=NULL, ...) = 1002
[pid  1001] execve("/usr/bin/ls", ["ls"], ...) = 0
[pid  1002] execve("/usr/bin/wc", ["wc", "-l"], ...) = 0
[pid  1001] write(1, "file1.txt\nfile2.txt\n", 20) = 20
[pid  1002] read(0, "file1.txt\nfile2.txt\n", 131072) = 20
[pid  1002] write(1, "2\n", 2)  = 2
```

Cada línea muestra `[pid N]` para identificar qué proceso hizo la syscall. Puedes ver cómo bash crea dos hijos, ls escribe al pipe, y wc lee del pipe.

### -f con -o: un archivo por proceso

```bash
strace -f -o trace -ff bash -c "ls | wc -l"
# Crea: trace.1000 (bash), trace.1001 (ls), trace.1002 (wc)
```

`-ff` combinado con `-o` crea un archivo separado por PID — mucho más fácil de leer que todo mezclado.

---

## 9. Salida a archivo: -o

```bash
strace -o trace.log ls /tmp
# La traza va a trace.log, la salida de ls va a stdout normal
```

Ventaja sobre `2>`: `-o` no captura otros mensajes de stderr del programa — solo la traza de strace. Con `2>` capturas todo stderr.

```bash
# Combinar: traza a archivo + seguir hijos + filtro
strace -f -o debug.log -e trace=file,network ./server
# debug.log tiene solo syscalls de archivo y red de todos los procesos
```

---

## 10. Strings y buffers: -s y -x

### -s N: longitud de strings mostrados

Por defecto, strace trunca strings a 32 caracteres:

```bash
strace cat /etc/hosts
# read(3, "127.0.0.1\tlocalhost\n127.0.1"..., 131072) = 221
#                                              ^^^
#                              Truncado a 32 caracteres con "..."
```

Para ver el contenido completo:

```bash
strace -s 1024 cat /etc/hosts
# read(3, "127.0.0.1\tlocalhost\n127.0.1.1\tmy-hostname\n::1\tlocalhost...", 131072) = 221
```

```bash
# Ver strings completos (útil para debugging de protocolos)
strace -s 4096 -e trace=read,write curl -s http://example.com
```

### -x: mostrar bytes no imprimibles en hexadecimal

```bash
strace -x cat /bin/ls | head -1
# read(3, "\x7fELF\x02\x01\x01\x00\x00...", 131072) = 142144
```

### -e read=fd y -e write=fd: dump hexadecimal de I/O

```bash
# Dump hexadecimal de todo lo leído del fd 3
strace -e read=3 cat /etc/hosts

# Dump hexadecimal de todo lo escrito al fd 1 (stdout)
strace -e write=1 echo "hello"
```

Muestra el contenido completo en formato hexdump, útil para protocolos binarios.

---

## 11. Debugging con strace: casos reales

### Caso 1: "File not found" — ¿qué archivo busca?

```bash
./programa
# Error: configuration file not found
```

El programa no dice qué path intentó. strace lo revela:

```bash
strace -e trace=openat ./programa 2>&1 | grep -i "ENOENT"
# openat(AT_FDCWD, "/etc/programa/config.yml", O_RDONLY) = -1 ENOENT
# openat(AT_FDCWD, "/home/user/.config/programa/config.yml", O_RDONLY) = -1 ENOENT
# openat(AT_FDCWD, "./config.yml", O_RDONLY) = -1 ENOENT
```

Ahora sabes que busca en tres ubicaciones y cuál necesitas crear.

### Caso 2: "Permission denied" — ¿en qué archivo?

```bash
strace -e trace=openat,connect ./programa 2>&1 | grep "EACCES\|EPERM"
# openat(AT_FDCWD, "/var/run/programa.pid", O_WRONLY|O_CREAT, 0644) = -1 EACCES
```

El programa intenta crear un archivo en `/var/run/` sin permisos.

### Caso 3: programa cuelga — ¿en qué syscall?

```bash
# El programa no responde
sudo strace -p $(pidof programa)
# poll([{fd=5, events=POLLIN}], 1, -1     ← está bloqueado aquí
```

El programa está bloqueado en `poll` esperando datos en el fd 5. ¿Qué es fd 5?

```bash
ls -la /proc/$(pidof programa)/fd/5
# lrwx------ 1 user user 64 ... 5 -> socket:[12345]

# ¿Qué socket es?
ss -p | grep 12345
# tcp ESTAB 0 0 192.168.1.100:45678 192.168.1.200:3306
```

El programa espera respuesta de una base de datos MySQL en `192.168.1.200:3306`.

### Caso 4: programa lento — ¿dónde pierde tiempo?

```bash
strace -c ./programa_lento
```

```
% time     seconds  usecs/call     calls    errors syscall
------ ----------- ----------- --------- --------- ----------------
 92.00    4.600000       46000       100           nanosleep
  5.00    0.250000         250      1000           write
  2.00    0.100000         100      1000           read
```

El 92% del tiempo está en `nanosleep` — el programa tiene sleeps innecesarios.

### Caso 5: comparar qué hace un programa en dos escenarios

```bash
# Escenario que funciona
strace -o works.log -e trace=file ./programa --config A

# Escenario que falla
strace -o fails.log -e trace=file ./programa --config B

# Comparar
diff works.log fails.log
```

---

## 12. Errores comunes

### Error 1: interpretar errores benignos como problemas

```bash
strace ls /tmp 2>&1 | grep ENOENT
# access("/etc/ld.so.preload", R_OK) = -1 ENOENT (No such file or directory)
# "¡ls no encuentra un archivo! ¡Algo está roto!"
```

**No**. Muchos programas intentan abrir archivos opcionales y manejan el `ENOENT` internamente. `ld.so.preload` es un archivo de preload de bibliotecas que normalmente no existe — es comportamiento esperado.

**Regla**: un `ENOENT` en strace solo es un problema si el programa falla o se comporta incorrectamente después.

### Error 2: olvidar -f y perder los hijos

```bash
strace ./server
# "Solo veo la syscall accept... ¿y el worker que procesa el request?"
```

**Solución**: `strace -f ./server`. Los workers son procesos hijos creados con fork/clone.

### Error 3: no redirigir la traza y mezclarla con la salida

```bash
strace cat /etc/hosts
# La traza (stderr) y el contenido de /etc/hosts (stdout) se mezclan en la terminal
```

**Solución**: redirigir la traza:

```bash
strace -o trace.log cat /etc/hosts   # traza a archivo
# o
strace cat /etc/hosts 2>trace.log    # stderr a archivo
```

### Error 4: trazar programas y no poder leer la salida

```bash
strace ls /usr/bin
# Cientos de líneas de mmap, mprotect, brk... ¿dónde está lo importante?
```

**Solución**: filtrar. Empieza con lo que buscas:

```bash
# ¿Qué archivos abre?
strace -e trace=%open ls /usr/bin

# ¿Qué errores hay?
strace -e status=failed ls /usr/bin

# ¿Resumen general?
strace -c ls /usr/bin
```

### Error 5: no saber que strace ralentiza el programa

strace usa `ptrace` para interceptar cada syscall, lo que detiene el proceso brevemente en cada una. Un programa que hace millones de syscalls puede volverse 10-100× más lento bajo strace.

**Solución**: para medir rendimiento real, no usar strace. Usar `-c` para solo contar sin mostrar cada syscall (menos overhead). Para programas de producción, considerar `perf` o `bpftrace` como alternativas de menor impacto.

---

## 13. Cheatsheet

```
┌──────────────────────────────────────────────────────────────────┐
│                    strace — REFERENCIA                           │
├──────────────────────────────────────────────────────────────────┤
│ BÁSICO                                                           │
│   strace ./programa             Trazar desde el inicio          │
│   strace -p PID                 Adjuntarse a proceso existente  │
│   strace -o trace.log ./prog    Salida a archivo                │
│   strace -f ./prog              Seguir procesos hijos           │
│   strace -ff -o trace ./prog    Un archivo por PID              │
├──────────────────────────────────────────────────────────────────┤
│ FILTRAR                                                          │
│   -e trace=file                 Solo syscalls de archivos       │
│   -e trace=network              Solo syscalls de red            │
│   -e trace=process              Solo fork/exec/wait/exit        │
│   -e trace=desc                 Solo read/write/close/poll...   │
│   -e trace=memory               Solo mmap/munmap/brk/mprotect  │
│   -e trace=signal               Solo señales                   │
│   -e trace=openat,read,write    Syscalls específicas            │
│   -e trace=!mmap,!mprotect      Excluir syscalls               │
│   -e status=failed              Solo las que fallaron           │
├──────────────────────────────────────────────────────────────────┤
│ TIMING                                                           │
│   -t                            Hora (HH:MM:SS)                │
│   -tt                           Hora con microsegundos          │
│   -T                            Duración de cada syscall        │
│   -r                            Tiempo relativo entre syscalls  │
├──────────────────────────────────────────────────────────────────┤
│ ESTADÍSTICAS                                                     │
│   -c                            Resumen (sin detalle)           │
│   -C                            Resumen + detalle               │
├──────────────────────────────────────────────────────────────────┤
│ DATOS                                                            │
│   -s N                          Mostrar hasta N chars de string │
│   -x                            Hex para no-imprimibles         │
│   -e read=FD                    Hexdump de lectura del fd       │
│   -e write=FD                   Hexdump de escritura del fd     │
├──────────────────────────────────────────────────────────────────┤
│ WORKFLOW TÍPICO                                                  │
│   1. strace -c ./prog           Panorama general                │
│   2. strace -e trace=... ./prog Filtrar categoría relevante     │
│   3. strace -tt -T -s 256 ...  Detalle con timing y strings    │
│   4. strace -f -o log ./prog   Guardar traza completa          │
└──────────────────────────────────────────────────────────────────┘
```

---

## 14. Ejercicios

### Ejercicio 1: Anatomía de un programa simple

1. Escribe un programa mínimo en C:
   ```c
   #include <stdio.h>

   int main(void) {
       FILE *fp = fopen("/etc/hostname", "r");
       if (!fp) {
           perror("fopen");
           return 1;
       }
       char buf[256];
       if (fgets(buf, sizeof(buf), fp)) {
           printf("Hostname: %s", buf);
       }
       fclose(fp);
       return 0;
   }
   ```
2. Compila: `gcc -o hostname_reader hostname_reader.c`
3. Traza: `strace ./hostname_reader`
4. Identifica en la traza:
   - ¿Qué syscall corresponde a `fopen`? ¿Con qué flags?
   - ¿Qué syscall corresponde a `fgets`? ¿Cuántos bytes pidió leer?
   - ¿Qué syscall corresponde a `printf`? ¿Coincide el string con lo esperado?
   - ¿Qué syscall corresponde a `fclose`?
   - ¿Cuántas syscalls hay **antes** de `main`? ¿Qué hacen? (Pista: carga de bibliotecas dinámicas)
5. Ejecuta `strace -c ./hostname_reader`. ¿Cuántas syscalls totales? ¿Cuál se invocó más veces?

> **Pregunta de reflexión**: `fgets` probablemente pidió leer mucho más de lo necesario (ej: 4096 bytes). ¿Por qué la biblioteca stdio pide más datos de los que la aplicación necesita? ¿Qué ventaja tiene esto?

### Ejercicio 2: Diagnosticar un error con strace

1. Escribe un programa que intente abrir 3 archivos, uno de los cuales no existe:
   ```c
   #include <fcntl.h>
   #include <stdio.h>
   #include <unistd.h>

   int main(void) {
       const char *files[] = {"/etc/hosts", "/etc/no_existe", "/etc/passwd"};

       for (int i = 0; i < 3; i++) {
           int fd = open(files[i], O_RDONLY);
           if (fd == -1) {
               perror(files[i]);
               continue;
           }
           char buf[64];
           ssize_t n = read(fd, buf, sizeof(buf) - 1);
           if (n > 0) {
               buf[n] = '\0';
               printf("--- %s ---\n%s\n", files[i], buf);
           }
           close(fd);
       }
       return 0;
   }
   ```
2. Ejecuta con `strace -e status=failed ./programa`. ¿Solo muestra el `openat` que falla?
3. Ejecuta con `strace -e trace=openat ./programa`. ¿Ves los 3 intentos de apertura con sus retornos?
4. Ejecuta con `strace -e trace=openat,read,write,close ./programa`. Correlaciona cada `openat` con su `read` y `close` usando los file descriptors.

> **Pregunta de reflexión**: en la traza, ¿el segundo archivo (que falla) usa un file descriptor? ¿Qué fd se asigna al tercer archivo? ¿Es el mismo número que se usó para el primero (que ya se cerró)?

### Ejercicio 3: Comparar I/O buffered vs unbuffered

1. Escribe dos versiones de un programa que escriba "hello\n" 1000 veces:

   **Versión A (stdio — buffered):**
   ```c
   #include <stdio.h>

   int main(void) {
       for (int i = 0; i < 1000; i++) {
           printf("hello\n");
       }
       return 0;
   }
   ```

   **Versión B (syscall directa — unbuffered):**
   ```c
   #include <unistd.h>

   int main(void) {
       for (int i = 0; i < 1000; i++) {
           write(STDOUT_FILENO, "hello\n", 6);
       }
       return 0;
   }
   ```

2. Traza ambas con `strace -c`:
   ```bash
   strace -c ./version_a > /dev/null
   strace -c ./version_b > /dev/null
   ```
3. ¿Cuántas syscalls `write` hace cada versión? (La versión A debería hacer muchas menos porque stdio bufferiza.)
4. Ahora ejecuta la versión A con salida a terminal (sin `> /dev/null`):
   ```bash
   strace -c ./version_a
   ```
   ¿Cambió el número de `write`? ¿Por qué? (Pista: stdio usa line-buffering cuando stdout es un terminal, full-buffering cuando es un archivo.)

> **Pregunta de reflexión**: si la versión B hace 1000 syscalls `write` y cada una toma ~200 ns, ¿cuánto tiempo extra se gasta solo en context switches comparado con la versión A que podría hacer solo 1-2 syscalls `write`? En un programa de alto rendimiento que escribe millones de líneas de log, ¿cuál enfoque elegirías?
