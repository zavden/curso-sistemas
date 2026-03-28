# Core dumps

## Índice

1. [¿Qué es un core dump?](#qué-es-un-core-dump)
2. [Habilitar core dumps (ulimit)](#habilitar-core-dumps-ulimit)
3. [Dónde se guardan los core dumps](#dónde-se-guardan-los-core-dumps)
4. [Analizar un core dump con GDB](#analizar-un-core-dump-con-gdb)
5. [coredumpctl: gestión en systemd](#coredumpctl-gestión-en-systemd)
6. [Señales que generan core dumps](#señales-que-generan-core-dumps)
7. [Core dumps de programas Rust](#core-dumps-de-programas-rust)
8. [Generar core dumps manualmente](#generar-core-dumps-manualmente)
9. [Core dumps en producción](#core-dumps-en-producción)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## ¿Qué es un core dump?

Un core dump es una **instantánea completa** de la memoria de un proceso en el momento en que termina de forma anormal (crash). Es como la caja negra de un avión: te permite investigar qué pasó **sin necesidad de reproducir el bug**.

```
┌──────────────────────────────────────────────────────┐
│               Core dump: contenido                   │
├──────────────────────────────────────────────────────┤
│                                                      │
│  ┌────────────────────────┐                          │
│  │ Registros del CPU      │ PC, SP, flags, etc.     │
│  ├────────────────────────┤                          │
│  │ Stack de cada thread   │ Variables locales,       │
│  │                        │ frames de función        │
│  ├────────────────────────┤                          │
│  │ Heap                   │ Memoria dinámica         │
│  │                        │ (malloc, Vec, Box)       │
│  ├────────────────────────┤                          │
│  │ Segmento de datos      │ Variables globales,      │
│  │                        │ estáticas                │
│  ├────────────────────────┤                          │
│  │ Memory mappings        │ mmap, shared libraries   │
│  ├────────────────────────┤                          │
│  │ Señal recibida         │ SIGSEGV, SIGABRT, etc.  │
│  └────────────────────────┘                          │
│                                                      │
│  NO contiene: archivos abiertos, estado de red,      │
│  contenido de pipes, variables de entorno (a veces)  │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### ¿Cuándo se genera un core dump?

Un core dump se genera cuando un proceso recibe una señal cuya acción por defecto es "core dump" — típicamente por un bug que causa acceso inválido a memoria o una condición irrecuperable.

---

## Habilitar core dumps (ulimit)

Por defecto, la mayoría de distribuciones Linux tienen los core dumps **deshabilitados** (tamaño máximo = 0).

### Verificar el estado actual

```bash
# Ver límite actual del shell
$ ulimit -c
0         # 0 = deshabilitado

# Ver todos los límites
$ ulimit -a
core file size          (blocks, -c) 0
# ...
```

### Habilitar para la sesión actual

```bash
# Sin límite de tamaño
$ ulimit -c unlimited

# Verificar
$ ulimit -c
unlimited

# Limitar a 100 MB (en bloques de 512 bytes)
$ ulimit -c 204800
```

> **Nota**: `ulimit -c unlimited` solo afecta al shell actual y sus procesos hijos. Al cerrar la terminal, vuelve al default.

### Habilitar permanentemente

```bash
# /etc/security/limits.conf — para todos los usuarios
# (requiere reiniciar sesión)
*    soft    core    unlimited
*    hard    core    unlimited

# O solo para un usuario específico:
zavden    soft    core    unlimited
zavden    hard    core    unlimited
```

### Verificar que funciona

```c
// crash.c — programa que causa segfault
#include <stdio.h>

int main() {
    int *ptr = NULL;
    *ptr = 42;  // SIGSEGV: dereferenciar NULL
    return 0;
}
```

```bash
$ gcc -g -O0 -o crash crash.c
$ ulimit -c unlimited
$ ./crash
Segmentation fault (core dumped)    # ← "core dumped" confirma que se generó
```

---

## Dónde se guardan los core dumps

La ubicación depende de la configuración del sistema.

### Configuración del kernel

```bash
# Ver patrón actual
$ cat /proc/sys/kernel/core_pattern
|/usr/lib/systemd/systemd-coredump %P %u %g %s %t %c %h

# Si empieza con | → se envía a un programa (systemd-coredump, abrt, etc.)
# Si es un path → se guarda como archivo

# Patrón simple: core dump en el directorio actual
$ echo "core.%e.%p" | sudo tee /proc/sys/kernel/core_pattern
# %e = nombre del ejecutable
# %p = PID
# %t = timestamp
# %s = señal que causó el dump
# %h = hostname
```

### Patrones comunes por distribución

```
┌──────────────────┬──────────────────────────────────────────┐
│ Distribución     │ core_pattern por defecto                 │
├──────────────────┼──────────────────────────────────────────┤
│ Fedora (systemd) │ |/usr/lib/systemd/systemd-coredump ...  │
│ Ubuntu (systemd) │ |/usr/lib/systemd/systemd-coredump ...  │
│ Ubuntu (Apport)  │ |/usr/share/apport/apport ...           │
│ Arch (systemd)   │ |/usr/lib/systemd/systemd-coredump ...  │
│ Sin handler      │ core (en el directorio de trabajo)      │
│ Custom           │ /var/coredumps/core.%e.%p.%t            │
└──────────────────┴──────────────────────────────────────────┘
```

### Cambiar a archivo simple (para desarrollo)

```bash
# Guardar core dumps en /tmp con nombre descriptivo
$ echo "/tmp/core.%e.%p.%s.%t" | sudo tee /proc/sys/kernel/core_pattern

# Ahora los core dumps se guardan como:
# /tmp/core.crash.12345.11.1711374615
#            ^     ^    ^   ^
#            exe   PID  sig timestamp

# Para hacer permanente (sobrevive reboot):
# /etc/sysctl.conf:
# kernel.core_pattern = /tmp/core.%e.%p.%s.%t
```

### core_uses_pid

```bash
# Añadir PID al nombre del core automáticamente
$ cat /proc/sys/kernel/core_uses_pid
1    # 1 = el core se llama core.PID

$ echo 1 | sudo tee /proc/sys/kernel/core_uses_pid
```

---

## Analizar un core dump con GDB

### Abrir el core dump

```bash
# Sintaxis: gdb EJECUTABLE CORE_DUMP
$ gdb ./crash /tmp/core.crash.12345.11.1711374615

# O con core dump de systemd:
$ coredumpctl gdb ./crash
```

### Primer vistazo: ¿dónde crasheó?

```
(gdb) bt
#0  0x0000000000401126 in main () at crash.c:5

# Línea 5 de crash.c — el NULL pointer dereference
```

### Examinar el estado

```
# Ver el frame actual
(gdb) frame 0
#0  0x0000000000401126 in main () at crash.c:5
5       *ptr = 42;

# Ver la variable problemática
(gdb) print ptr
$1 = (int *) 0x0     ← NULL!

# Ver todas las variables locales
(gdb) info locals
ptr = 0x0

# Ver registros
(gdb) info registers
rax            0x0                 0
rip            0x401126            0x401126 <main+13>

# Ver la señal que causó el crash
(gdb) info signal SIGSEGV
Signal  Stop  Print  Pass  Description
SIGSEGV Yes   Yes    Yes   Segmentation fault
```

### Ejemplo completo: crash en programa complejo

```c
// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char *name;
    int value;
} Config;

Config* load_config(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return NULL;

    Config *cfg = malloc(sizeof(Config));
    char buf[256];
    if (fgets(buf, sizeof(buf), f)) {
        cfg->name = strdup(buf);
        cfg->value = atoi(buf);
    }
    fclose(f);
    return cfg;
}

void process(Config *cfg) {
    printf("Processing: %s = %d\n", cfg->name, cfg->value);
    // Bug: si load_config retorna NULL, esto es segfault
}

int main() {
    Config *cfg = load_config("/nonexistent/config.txt");
    process(cfg);  // cfg es NULL → SIGSEGV
    free(cfg);
    return 0;
}
```

```bash
$ gcc -g -O0 -o server server.c
$ ulimit -c unlimited
$ ./server
Segmentation fault (core dumped)
```

```
$ gdb ./server core.server.12345.11.1711374615

(gdb) bt
#0  0x00000000004011b5 in process (cfg=0x0) at server.c:24
#1  0x00000000004011e0 in main () at server.c:29

(gdb) frame 0
#0  process (cfg=0x0) at server.c:24
24      printf("Processing: %s = %d\n", cfg->name, cfg->value);

(gdb) print cfg
$1 = (Config *) 0x0

(gdb) frame 1
#1  main () at server.c:29
29      process(cfg);

(gdb) print cfg
$1 = (Config *) 0x0

# Diagnóstico: load_config retornó NULL porque el archivo no existe.
# main no verificó el retorno antes de llamar a process.
```

### Comandos útiles para análisis post-mortem

```
# Backtrace completo con argumentos
(gdb) bt full

# Backtrace de todos los threads
(gdb) thread apply all bt

# Ver memory mappings (qué bibliotecas estaban cargadas)
(gdb) info sharedlibrary

# Ver archivos fuente
(gdb) list

# Examinar memoria alrededor del crash
(gdb) x/20xw $rsp

# Ver la instrucción que causó el crash
(gdb) x/i $rip
```

---

## coredumpctl: gestión en systemd

En sistemas con systemd, `systemd-coredump` captura y almacena core dumps de forma organizada.

### Listar core dumps

```bash
$ coredumpctl list
TIME                            PID  UID  GID SIG     COREFILE EXE
Thu 2024-03-14 10:30:15 UTC    1234 1000 1000 SIGSEGV present  /home/user/crash
Thu 2024-03-14 11:45:30 UTC    5678 1000 1000 SIGABRT present  /home/user/server

# Filtrar por ejecutable
$ coredumpctl list crash

# Filtrar por PID
$ coredumpctl list 1234
```

### Ver información de un core dump

```bash
$ coredumpctl info 1234
           PID: 1234
           UID: 1000 (user)
           GID: 1000 (user)
        Signal: 11 (SEGV)
     Timestamp: Thu 2024-03-14 10:30:15 UTC
  Command Line: ./crash
    Executable: /home/user/crash
   Control Group: /user.slice/user-1000.slice/session-1.scope
          Unit: session-1.scope
         Slice: user-1000.slice
       Boot ID: abc123...
    Machine ID: def456...
      Hostname: myhost
       Storage: /var/lib/systemd/coredump/core.crash.1000.abc123.1234.1711374615000000
       Message: Process 1234 (crash) of user 1000 dumped core.

                Stack trace:
                #0  0x0000000000401126 main (crash.c:5)
```

### Abrir en GDB directamente

```bash
# Abrir el core dump más reciente del ejecutable
$ coredumpctl gdb crash

# Abrir un core dump específico por PID
$ coredumpctl gdb 1234

# Abrir el más reciente de cualquier programa
$ coredumpctl gdb
```

### Exportar core dump a archivo

```bash
# Extraer el core dump a un archivo
$ coredumpctl dump 1234 -o /tmp/core.1234

# Ahora puedes analizarlo con gdb normal
$ gdb ./crash /tmp/core.1234
```

### Configurar systemd-coredump

```bash
# /etc/systemd/coredump.conf
[Coredump]
Storage=external          # external (archivo), journal, none
Compress=yes              # comprimir con zstd
ProcessSizeMax=2G         # máximo tamaño de proceso a capturar
ExternalSizeMax=2G        # máximo tamaño del core dump
MaxUse=10G                # espacio total máximo para core dumps
KeepFree=1G               # espacio libre mínimo en disco

# Aplicar cambios
$ sudo systemctl daemon-reload
```

### Limpiar core dumps antiguos

```bash
# Ver espacio usado
$ du -sh /var/lib/systemd/coredump/

# Limpiar core dumps más antiguos que 3 días
$ sudo journalctl --vacuum-time=3d

# O manualmente:
$ sudo rm /var/lib/systemd/coredump/core.old_program.*
```

---

## Señales que generan core dumps

No todas las señales generan core dumps. Solo aquellas cuya acción por defecto es "Core":

```
┌─────────┬────────────────────────────────┬─────────────────┐
│ Señal   │ Causa típica                   │ Acción default  │
├─────────┼────────────────────────────────┼─────────────────┤
│ SIGSEGV │ Acceso a memoria inválida      │ Core            │
│ SIGBUS  │ Error de bus (alineación)      │ Core            │
│ SIGFPE  │ Error aritmético (div by zero) │ Core            │
│ SIGABRT │ abort() llamado                │ Core            │
│ SIGILL  │ Instrucción ilegal             │ Core            │
│ SIGSYS  │ Syscall inválida               │ Core            │
│ SIGQUIT │ Ctrl+\ en terminal             │ Core            │
│ SIGTRAP │ Breakpoint / trace trap        │ Core            │
│ SIGXCPU │ Tiempo de CPU excedido         │ Core            │
│ SIGXFSZ │ Tamaño de archivo excedido     │ Core            │
├─────────┼────────────────────────────────┼─────────────────┤
│ SIGTERM │ Terminación normal             │ Terminate (NO)  │
│ SIGKILL │ Kill forzado                   │ Terminate (NO)  │
│ SIGINT  │ Ctrl+C                         │ Terminate (NO)  │
│ SIGPIPE │ Escritura a pipe cerrado       │ Terminate (NO)  │
└─────────┴────────────────────────────────┴─────────────────┘
```

### Relación señal → bug

```
SIGSEGV → NULL pointer dereference, buffer overflow,
          use-after-free, stack overflow
SIGBUS  → Acceso desalineado, mmap a archivo truncado
SIGFPE  → División por cero (enteros), overflow en aritmética
SIGABRT → assert() falló, double free detectado por glibc,
          abort() explícito, Rust panic (con panic=abort)
SIGILL  → Código corrupto, instrucción incompatible con CPU
```

---

## Core dumps de programas Rust

### panic!() y core dumps

Por defecto, Rust hace **unwind** al paniquear — no genera core dump:

```bash
$ ./rust_program
thread 'main' panicked at 'index out of bounds: len is 3 but the index is 10'
# No core dump — el programa hizo unwind y salió con código 101
```

Para obtener core dumps en panics, usa `panic = "abort"`:

```toml
# Cargo.toml
[profile.dev]
panic = "abort"

[profile.release]
panic = "abort"
```

```bash
$ cargo build
$ ulimit -c unlimited
$ ./target/debug/my_program
# Ahora panic!() genera SIGABRT → core dump
```

### Analizar core dump de Rust

```bash
# Usar rust-gdb para pretty-printers
$ rust-gdb target/debug/my_program core.my_program.12345

(gdb) bt
#0  __GI_raise (sig=sig@entry=6) at ../sysdeps/unix/sysv/linux/raise.c:50
#1  __GI_abort () at abort.c:79
#2  std::sys::unix::abort_internal () at library/std/src/sys/unix/mod.rs:315
#3  std::process::abort () at library/std/src/process.rs:2115
#4  rust_panic () at library/std/src/panicking.rs:744
#5  std::panicking::rust_panic_with_hook () at library/std/src/panicking.rs:692
#6  my_program::process_data (data=Vec(size=3) = {1, 2, 3}, idx=10)
    at src/main.rs:5
#7  my_program::main () at src/main.rs:10

(gdb) frame 6
(gdb) print data
$1 = Vec(size=3) = {1, 2, 3}
(gdb) print idx
$2 = 10
```

### Segfaults en Rust (código unsafe)

```rust
fn main() {
    unsafe {
        let ptr: *const i32 = std::ptr::null();
        println!("{}", *ptr); // SIGSEGV
    }
}
```

```bash
$ ulimit -c unlimited
$ ./target/debug/my_program
Segmentation fault (core dumped)

$ rust-gdb target/debug/my_program core
(gdb) bt
#0  my_program::main () at src/main.rs:4
(gdb) print ptr
$1 = (*const i32) 0x0
```

---

## Generar core dumps manualmente

A veces quieres un core dump de un proceso que **no** ha crasheado — para inspeccionar su estado actual.

### Desde GDB

```bash
# Attach al proceso
$ gdb -p 12345

# Generar core dump sin matar el proceso
(gdb) generate-core-file
Saved corefile core.12345

(gdb) detach
# El proceso sigue corriendo normalmente
```

### Con gcore (sin GDB interactivo)

```bash
# gcore: genera core dump de un proceso en ejecución
$ gcore 12345
# Genera: core.12345

# Con nombre custom
$ gcore -o /tmp/snapshot 12345
# Genera: /tmp/snapshot.12345
```

### Con kill -SIGQUIT

```bash
# SIGQUIT (Ctrl+\) genera core dump Y termina el proceso
$ kill -QUIT 12345
# El proceso muere y genera core dump
```

### Con /proc/PID/coredump_filter

```bash
# Controlar qué segmentos de memoria se incluyen en el core
$ cat /proc/self/coredump_filter
33   # bitmask

# Bits:
# 0x01 = anonymous private (stack, heap)
# 0x02 = anonymous shared
# 0x04 = file-backed private
# 0x08 = file-backed shared
# 0x10 = ELF headers
# 0x20 = private huge pages
# 0x40 = shared huge pages
# 0x80 = private DAX pages

# Incluir todo:
$ echo 0x7f > /proc/self/coredump_filter
```

---

## Core dumps en producción

### Consideraciones de seguridad

```
┌──────────────────────────────────────────────────────────┐
│    ⚠ Core dumps en producción: consideraciones           │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  Los core dumps contienen TODA la memoria del proceso:   │
│  - Contraseñas en memoria                                │
│  - Tokens de sesión                                      │
│  - Datos de usuarios (PII)                               │
│  - Claves de cifrado                                     │
│  - Datos de tarjetas de crédito                          │
│                                                          │
│  Medidas:                                                │
│  - Restringir permisos del directorio de core dumps      │
│  - No enviar core dumps a servicios externos sin revisar │
│  - Cifrar el almacenamiento de core dumps                │
│  - Limpiar core dumps después del análisis               │
│  - Considerar fs.suid_dumpable para procesos con setuid  │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Configuración para producción

```bash
# /etc/sysctl.conf

# Deshabilitar core dumps para programas setuid
kernel.suid_dumpable = 0

# Core pattern que incluye metadatos
kernel.core_pattern = /var/coredumps/core.%e.%p.%t.%s

# Asegurar el directorio
$ sudo mkdir -p /var/coredumps
$ sudo chmod 700 /var/coredumps
$ sudo chown root:root /var/coredumps
```

### Limitar tamaño de core dumps por proceso

```bash
# En el script de inicio del servicio:
ulimit -c 1048576   # máximo 512 MB (en bloques de 512 bytes)

# En systemd unit:
[Service]
LimitCORE=500M
```

### Rotación automática

```bash
# Script de limpieza en cron:
# Eliminar core dumps más antiguos que 7 días
find /var/coredumps -name "core.*" -mtime +7 -delete

# O con systemd:
# /etc/systemd/coredump.conf
[Coredump]
MaxUse=5G
KeepFree=2G
```

---

## Errores comunes

### 1. Core dumps deshabilitados y no saberlo

```bash
# El programa crashea pero no dice "core dumped":
$ ./crash
Segmentation fault          # ← sin "(core dumped)"

# Verificar:
$ ulimit -c
0     # ← ¡deshabilitado!

# Solución:
$ ulimit -c unlimited
$ ./crash
Segmentation fault (core dumped)    # ← ahora sí
```

### 2. No encontrar el core dump

```bash
# ¿Dónde está el core?
$ ls core*
ls: cannot access 'core*': No such file or directory

# Verificar el patrón:
$ cat /proc/sys/kernel/core_pattern
|/usr/lib/systemd/systemd-coredump %P %u %g %s %t %c %h

# ¡Va a systemd! Usar coredumpctl:
$ coredumpctl list
$ coredumpctl gdb
```

### 3. Abrir core dump sin el ejecutable correcto

```bash
# INCORRECTO: core dump de la versión compilada ayer,
# ejecutable de hoy (recompilado con cambios)
$ gdb ./program_v2 core_from_v1
# GDB: warning: exec file is newer than core file
# Los números de línea y símbolos no coinciden

# CORRECTO: usar el MISMO ejecutable que generó el core
$ gdb ./program_v1 core_from_v1
```

### 4. Compilar sin símbolos de debug

```bash
# Sin -g: el core dump se puede analizar pero con información limitada
$ gcc -O0 -o crash crash.c     # sin -g
$ ./crash
Segmentation fault (core dumped)

$ gdb ./crash core
(gdb) bt
#0  0x0000000000401126 in main ()    # ← sin archivo:línea

# Con -g: información completa
$ gcc -g -O0 -o crash crash.c
```

### 5. Disco lleno impide generar el core dump

```bash
# El core dump no se genera si no hay espacio en disco
# core dumps pueden ser ENORMES (GB para programas grandes)

# Verificar:
$ df -h /tmp
# o
$ df -h /var/lib/systemd/coredump/

# Solución: limpiar espacio o cambiar core_pattern a un disco con espacio
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║             CORE DUMPS — REFERENCIA RÁPIDA               ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  HABILITAR                                                ║
║  ────────                                                 ║
║  ulimit -c unlimited              para esta sesión        ║
║  /etc/security/limits.conf        permanente              ║
║                                                           ║
║  UBICACIÓN                                                ║
║  ─────────                                                ║
║  cat /proc/sys/kernel/core_pattern                        ║
║  Si empieza con | → va a un programa (systemd)            ║
║  Si es path → archivo en esa ruta                         ║
║  Cambiar: echo "core.%e.%p" > /proc/sys/.../core_pattern ║
║                                                           ║
║  ANALIZAR CON GDB                                         ║
║  ────────────────                                         ║
║  gdb ./programa core_dump                                 ║
║  bt                    backtrace                          ║
║  bt full               backtrace con variables            ║
║  frame N               ir al frame N                      ║
║  info locals           variables locales del frame        ║
║  info registers        registros del CPU                  ║
║  print variable        ver valor de variable              ║
║  thread apply all bt   backtrace de todos los threads     ║
║                                                           ║
║  COREDUMPCTL (systemd)                                    ║
║  ────────────────────                                     ║
║  coredumpctl list              listar core dumps          ║
║  coredumpctl info PID          detalles                   ║
║  coredumpctl gdb PID           abrir en GDB               ║
║  coredumpctl gdb ./programa    último de ese ejecutable   ║
║  coredumpctl dump PID -o FILE  exportar a archivo         ║
║                                                           ║
║  GENERAR MANUALMENTE                                      ║
║  ───────────────────                                      ║
║  gcore PID                     sin matar el proceso       ║
║  kill -QUIT PID                mata + genera core         ║
║  gdb -p PID → generate-core-file → detach                ║
║                                                           ║
║  RUST                                                     ║
║  ────                                                     ║
║  panic = "abort" en Cargo.toml  para core dump en panic   ║
║  rust-gdb target/debug/prog core  con pretty-printers     ║
║                                                           ║
║  SEÑALES QUE GENERAN CORE                                 ║
║  ────────────────────────                                  ║
║  SIGSEGV (segfault), SIGABRT (abort), SIGBUS (bus error)  ║
║  SIGFPE (div/0), SIGILL (instrucción ilegal), SIGQUIT     ║
║                                                           ║
║  PATRONES (core_pattern)                                  ║
║  ───────────────────────                                   ║
║  %e = ejecutable  %p = PID  %t = timestamp                ║
║  %s = señal  %h = hostname  %u = UID                     ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Diagnóstico post-mortem con core dump

Compila y ejecuta este programa. Genera un core dump y analízalo para encontrar la causa del crash:

```c
// linked_crash.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct Node {
    int value;
    struct Node *next;
} Node;

Node* create_list(int n) {
    Node *head = NULL;
    for (int i = n - 1; i >= 0; i--) {
        Node *node = malloc(sizeof(Node));
        node->value = i * 10;
        node->next = head;
        head = node;
    }
    return head;
}

void free_list(Node *head) {
    while (head) {
        Node *next = head->next;
        free(head);
        head = next;
    }
}

int sum_list(Node *head) {
    int sum = 0;
    while (head) {
        sum += head->value;
        head = head->next;
    }
    return sum;
}

int main() {
    Node *list = create_list(5);
    printf("Sum: %d\n", sum_list(list));

    free_list(list);

    // Bug: use-after-free
    printf("Sum after free: %d\n", sum_list(list));

    return 0;
}
```

Pasos:
1. Compila con `gcc -g -O0 -o linked_crash linked_crash.c`.
2. Habilita core dumps y ejecuta.
3. Abre el core dump con GDB.
4. Usa `bt`, `frame`, `print` para entender el crash.
5. Identifica exactamente en qué nodo de la lista falla y por qué.

**Pregunta de reflexión**: ¿Es posible que este programa NO crashee en tu máquina? ¿Por qué? (Pista: glibc no siempre retorna la memoria al kernel inmediatamente después de `free`.) ¿Cómo podrías forzar el crash?

---

### Ejercicio 2: Core dump de un programa multi-thread

```c
// mt_crash.c
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

int *shared_data = NULL;

void* worker(void *arg) {
    int id = *(int*)arg;
    // Simular trabajo
    usleep(100000 * id);

    // Bug: si otro thread ya hizo free, esto crashea
    printf("Thread %d: data = %d\n", id, shared_data[id]);

    return NULL;
}

void* destroyer(void *arg) {
    usleep(150000); // esperar un poco
    free(shared_data);
    shared_data = NULL;
    printf("Destroyer: freed shared_data\n");
    return NULL;
}

int main() {
    shared_data = malloc(4 * sizeof(int));
    for (int i = 0; i < 4; i++) shared_data[i] = i * 100;

    pthread_t threads[5];
    int ids[5] = {0, 1, 2, 3, 4};

    // 4 workers + 1 destroyer
    for (int i = 0; i < 4; i++)
        pthread_create(&threads[i], NULL, worker, &ids[i]);
    pthread_create(&threads[4], NULL, destroyer, NULL);

    for (int i = 0; i < 5; i++)
        pthread_join(threads[i], NULL);

    return 0;
}
```

Pasos:
1. Compila con `gcc -g -O0 -pthread -o mt_crash mt_crash.c`.
2. Ejecuta varias veces hasta que crashee (puede requerir varios intentos).
3. Analiza el core dump:
   - `thread apply all bt` para ver todos los threads.
   - Identifica qué thread crasheó y cuál hizo el `free`.
   - Examina `shared_data` con `print shared_data`.

**Pregunta de reflexión**: ¿Qué herramienta usarías para detectar este bug de forma determinista sin depender de la suerte del timing? (Pista: ThreadSanitizer, Valgrind, AddressSanitizer.)

---

### Ejercicio 3: Workflow completo con coredumpctl

Realiza estas tareas usando `coredumpctl` (requiere systemd):

1. Lista todos los core dumps en tu sistema.
2. Escribe un pequeño programa C que haga `abort()` y ejecútalo.
3. Usa `coredumpctl info` para ver los detalles del crash.
4. Abre el core dump con `coredumpctl gdb`.
5. Exporta el core dump a un archivo con `coredumpctl dump -o`.
6. Abre el archivo exportado con `gdb` directamente.
7. Limpia los core dumps antiguos.

```c
// abort_test.c
#include <stdio.h>
#include <stdlib.h>

void deep_function(int depth) {
    if (depth <= 0) {
        printf("Aborting from depth %d\n", depth);
        abort();  // genera SIGABRT → core dump
    }
    deep_function(depth - 1);
}

int main() {
    printf("Starting...\n");
    deep_function(10);
    return 0;
}
```

Documenta el output de cada paso. Verifica que el backtrace muestra las 10 llamadas recursivas a `deep_function`.

**Pregunta de reflexión**: ¿Cuál es la diferencia entre `abort()` y `exit(1)`? ¿Por qué `abort()` genera core dump pero `exit(1)` no? ¿Qué señal envía `abort()` internamente?
