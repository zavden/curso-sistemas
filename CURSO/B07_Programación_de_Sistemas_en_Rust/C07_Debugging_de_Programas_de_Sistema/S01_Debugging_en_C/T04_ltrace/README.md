# ltrace: trazar llamadas a bibliotecas dinámicas

## Índice

1. [¿Qué es ltrace?](#qué-es-ltrace)
2. [Uso básico](#uso-básico)
3. [Filtrar por biblioteca o función](#filtrar-por-biblioteca-o-función)
4. [Formato de salida y decodificación](#formato-de-salida-y-decodificación)
5. [Timing y estadísticas](#timing-y-estadísticas)
6. [Comparación: ltrace vs strace](#comparación-ltrace-vs-strace)
7. [ltrace con programas Rust y C++](#ltrace-con-programas-rust-y-c)
8. [Casos de uso prácticos](#casos-de-uso-prácticos)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Qué es ltrace?

`ltrace` intercepta y registra las llamadas a **bibliotecas dinámicas** (shared libraries) que un programa realiza. Mientras strace captura syscalls (interfaz con el kernel), ltrace captura llamadas a funciones de libc, libm, libpthread y cualquier otra `.so`.

```
┌──────────────────────────────────────────────────────┐
│              Niveles de interceptación                │
├──────────────────────────────────────────────────────┤
│                                                      │
│  Tu programa                                         │
│      │                                               │
│      ├── printf("hello")     ← ltrace captura aquí  │
│      ├── malloc(100)         ← ltrace captura aquí  │
│      ├── strlen(s)           ← ltrace captura aquí  │
│      │                                               │
│      │   ┌─ libc.so ─────────────────┐              │
│      │   │  printf → write()         │              │
│      │   │  malloc → brk()/mmap()    │              │
│      │   │  strlen → (cálculo puro)  │              │
│      │   └───────────────────────────┘              │
│      │                                               │
│      ├── write(1, "hello", 5) ← strace captura aquí│
│      ├── brk(0x...)           ← strace captura aquí│
│      │                                               │
│      │   ┌─ kernel ─────────────────┐               │
│      │   │  sys_write()             │               │
│      │   │  sys_brk()               │               │
│      │   └──────────────────────────┘               │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### Instalar ltrace

```bash
# Fedora / RHEL
sudo dnf install ltrace

# Ubuntu / Debian
sudo apt install ltrace

# Arch
sudo pacman -S ltrace
```

> **Nota**: en kernels recientes con restricciones de `ptrace`, puede ser necesario ejecutar ltrace como root o ajustar `/proc/sys/kernel/yama/ptrace_scope`.

```bash
# Si ltrace falla con "permission denied":
$ cat /proc/sys/kernel/yama/ptrace_scope
1    # 1 = solo el padre puede trazar

# Temporalmente permitir trazar cualquier proceso:
$ echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
```

---

## Uso básico

### Trazar un programa

```bash
$ ltrace ./program
__libc_start_main(0x401150, 1, 0x7ffd..., ...) = 0
malloc(100)                              = 0x55a234...
strcpy(0x55a234..., "hello")             = "hello"
strlen("hello")                          = 5
printf("message: %s (%d chars)\n", "hello", 5) = 24
free(0x55a234...)                        = <void>
+++ exited (status 0) +++
```

Cada línea muestra:
- **Nombre de la función** llamada.
- **Argumentos** (ltrace decodifica strings, punteros, flags).
- **Valor de retorno** (después del `=`).

### Con un comando del sistema

```bash
$ ltrace ls /tmp
__libc_start_main(...)                   = 0
setlocale(LC_ALL, "")                    = "en_US.UTF-8"
opendir("/tmp")                          = 0x55a234...
readdir(0x55a234...)                     = 0x55a234...
strcmp(".", ".")                          = 0
readdir(0x55a234...)                     = 0x55a234...
strcmp("..", "..")                        = 0
readdir(0x55a234...)                     = 0x55a234...
...
closedir(0x55a234...)                    = 0
```

---

## Filtrar por biblioteca o función

### -e: filtrar por nombre de función

```bash
# Solo llamadas a malloc y free
$ ltrace -e malloc+free ./program
malloc(100)                              = 0x55a234...
malloc(200)                              = 0x55a235...
free(0x55a234...)                        = <void>
free(0x55a235...)                        = <void>

# Solo funciones que empiezan con "str"
$ ltrace -e 'str*' ./program
strcpy(0x55a234..., "hello")             = "hello"
strlen("hello")                          = 5
strcmp("hello", "world")                  = -15

# Solo printf y puts
$ ltrace -e printf+puts ./program
printf("count: %d\n", 42)               = 10
puts("done")                             = 5
```

### -l: filtrar por biblioteca

```bash
# Solo llamadas a libm (matemáticas)
$ ltrace -l libm.so* ./math_program
sqrt(2)                                  = 1.414214
sin(3.141593)                            = 0.000000
cos(0)                                   = 1.000000
pow(2, 10)                               = 1024.000000

# Solo llamadas a libpthread
$ ltrace -l libpthread.so* ./threaded_program
pthread_create(0x7ffd..., NULL, 0x401200, NULL) = 0
pthread_mutex_lock(0x404040)             = 0
pthread_mutex_unlock(0x404040)           = 0
pthread_join(0x7f..., NULL)              = 0
```

### Combinar con syscalls (-S)

```bash
# Mostrar tanto llamadas a bibliotecas como syscalls
$ ltrace -S ./program
malloc(100)                              = 0x55a234...
SYS_brk(0)                              = 0x55a300...
SYS_brk(0x55a321...)                     = 0x55a321...
strcpy(0x55a234..., "hello")             = "hello"
printf("hello\n")                        = 6
SYS_write(1, "hello\n", 6)              = 6
```

Esto muestra la **relación** entre llamadas de biblioteca y las syscalls que generan internamente. `printf` → `SYS_write`, `malloc` → `SYS_brk`.

---

## Formato de salida y decodificación

### -s N: longitud de strings

```bash
# Default: strings truncados a 32 caracteres
$ ltrace ./program
puts("This is a very long string t"...) = 50

# Mostrar strings completos
$ ltrace -s 200 ./program
puts("This is a very long string that we want to see in full for debugging") = 70
```

### -n N: indentar llamadas anidadas

```bash
# Mostrar profundidad de llamadas (útil para ver qué llama a qué)
$ ltrace -n 4 ./program
__libc_start_main(...)
    malloc(100) = 0x55a234...
    process_data(0x55a234...)
        strlen("hello") = 5
        printf("len=%d\n", 5) = 6
    free(0x55a234...)
```

### -o FILE: salida a archivo

```bash
# Escribir traza a archivo
$ ltrace -o trace.log ./program

# El programa imprime a stdout/stderr normalmente
# La traza va a trace.log
```

### -x: trazar funciones internas del programa

```bash
# Por defecto, ltrace solo traza funciones de bibliotecas externas.
# Con -x puedes trazar funciones del propio programa:

$ ltrace -x 'process_*' ./program
process_init()                           = 0
process_data("hello")                    = 5
process_cleanup()                        = <void>
```

### Decodificación de argumentos

ltrace decodifica automáticamente tipos comunes:

```bash
$ ltrace ./program
# Strings se muestran como texto:
strcmp("hello", "world")                 = -15

# Punteros como direcciones hex:
malloc(256)                              = 0x55a2340

# NULL como 0 o NULL:
fopen("/etc/config", "r")               = NULL

# Flags como constantes (si tiene config):
open("/tmp/test", O_WRONLY|O_CREAT, 0644) = 3

# Valores de retorno con su tipo:
atoi("42")                               = 42
atof("3.14")                             = 3.140000
```

---

## Timing y estadísticas

### -T: tiempo de cada llamada

```bash
$ ltrace -T -e malloc+free+strlen ./program
malloc(100)                              = 0x55a234... <0.000012>
strlen("hello world")                    = 11 <0.000001>
free(0x55a234...)                        = <void> <0.000003>
```

### -t, -tt: timestamps

```bash
# Hora del día
$ ltrace -t -e 'str*' ./program
14:30:15 strcmp("hello", "world")        = -15
14:30:15 strlen("hello")                = 5

# Con microsegundos
$ ltrace -tt -e 'str*' ./program
14:30:15.123456 strcmp("hello", "world") = -15
14:30:15.123489 strlen("hello")         = 5
```

### -r: tiempo relativo

```bash
$ ltrace -r -e malloc+free ./program
  0.000000 malloc(100)                   = 0x55a234...
  0.000045 malloc(200)                   = 0x55a235...
  0.001234 free(0x55a234...)             = <void>
  0.000003 free(0x55a235...)             = <void>
```

### -c: resumen estadístico

```bash
$ ltrace -c ./program
% time     seconds  usecs/call     calls      function
------ ----------- ----------- --------- --------------------
 45.23    0.000234          23        10 malloc
 30.12    0.000156           7        22 strlen
 15.65    0.000081           8        10 free
  5.00    0.000026           8         3 printf
  4.00    0.000021           7         3 strcmp
------ ----------- ----------- --------- --------------------
100.00    0.000518                    48 total
```

### -c combinado con filtro

```bash
# Estadísticas solo de funciones de memoria
$ ltrace -c -e malloc+free+calloc+realloc ./program
% time     seconds  usecs/call     calls      function
------ ----------- ----------- --------- --------------------
 60.00    0.000300          30        10 malloc
 25.00    0.000125          12        10 free
 10.00    0.000050          25         2 realloc
  5.00    0.000025          12         2 calloc
------ ----------- ----------- --------- --------------------
100.00    0.000500                    24 total

# ¿10 malloc pero solo 10 free? Parece correcto (sin leak aparente)
# ¿100 malloc y 80 free? → posible memory leak
```

---

## Comparación: ltrace vs strace

```
┌─────────────────────┬─────────────────────┬─────────────────────┐
│                     │ ltrace              │ strace              │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ Intercepta          │ Llamadas a          │ Llamadas al         │
│                     │ bibliotecas (.so)   │ kernel (syscalls)   │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ Nivel               │ Espacio de usuario  │ Interfaz kernel     │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ Ejemplos            │ malloc, printf,     │ write, read, open,  │
│                     │ strcmp, pthread_*    │ mmap, clone, ioctl  │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ Mecanismo           │ LD breakpoints      │ ptrace              │
│                     │ (PLT hooking)       │                     │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ Funciona con        │ Solo binarios       │ Cualquier binario   │
│                     │ dinámicamente       │ (estático o         │
│                     │ enlazados           │ dinámico)           │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ Overhead            │ Moderado            │ Moderado            │
├─────────────────────┼─────────────────────┼─────────────────────┤
│ Cuándo usar         │ "¿Qué funciones de  │ "¿Qué hace el      │
│                     │ libc llama?"        │ programa a nivel    │
│                     │ "¿Cuántos malloc?"  │ de kernel?"         │
│                     │ "¿Qué strings       │ "¿Qué archivos     │
│                     │ compara?"           │ abre?"              │
└─────────────────────┴─────────────────────┴─────────────────────┘
```

### Ejemplo lado a lado

```bash
# strace ve: write(1, "hello\n", 6) = 6
# ltrace ve: printf("hello\n") = 6

# strace ve: brk(0x55a321...) = 0x55a321...
# ltrace ve: malloc(100) = 0x55a234...

# strace ve: futex(0x404040, FUTEX_WAIT, ...) = 0
# ltrace ve: pthread_mutex_lock(0x404040) = 0
```

ltrace te dice **qué quiso hacer** el programa (llamar a `printf`). strace te dice **cómo lo hizo** el kernel (syscall `write`).

### Usar ambos juntos

```bash
# ltrace -S muestra ambos niveles
$ ltrace -S -e malloc ./program
malloc(100)                              = 0x55a234...
SYS_brk(0)                              = 0x55a300000
SYS_brk(0x55a321000)                     = 0x55a321000
# Vemos que malloc pidió memoria al kernel con brk()
```

---

## ltrace con programas Rust y C++

### Rust: binarios dinámicamente enlazados

```bash
# Rust por defecto enlaza estáticamente la stdlib de Rust,
# pero dinámicamente con libc. ltrace ve las llamadas a libc:

$ ltrace target/debug/my_rust_program
__libc_start_main(...)                   = 0
malloc(1024)                             = 0x55a234...
malloc(64)                               = 0x55a235...
write(1, "Hello, world!\n", 14)          = 14
free(0x55a235...)                        = <void>
free(0x55a234...)                        = <void>
```

### Rust: funciones internas con -x

```bash
# Trazar funciones internas del binario Rust
$ ltrace -x '*main*' target/debug/my_program
my_program::main()                       = <void>
```

### C++: name mangling

```bash
# C++ tiene nombres mangled
$ ltrace ./cpp_program
_ZNSt6vectorIiSaIiEE9push_backERKi(0x7ffd..., 0x7ffd...) = <void>
# Ilegible...

# Desmanglar con c++filt
$ ltrace ./cpp_program 2>&1 | c++filt
std::vector<int>::push_back(int const&)(0x7ffd..., 0x7ffd...) = <void>

# O con ltrace -C (demangle automático en algunas versiones)
$ ltrace -C ./cpp_program
```

### Limitación: binarios estáticos

```bash
# ltrace NO funciona con binarios estáticamente enlazados
$ ltrace ./static_binary
# (no output o error)

# Verificar si un binario es estático o dinámico:
$ file ./program
./program: ELF 64-bit LSB executable, dynamically linked, interpreter /lib64/ld-linux-x86-64.so.2

$ ldd ./program
linux-vdso.so.1 => ...
libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6
/lib64/ld-linux-x86-64.so.2

# Si dice "statically linked" → ltrace no funcionará
# Usar strace en su lugar
```

---

## Casos de uso prácticos

### 1. Encontrar memory leaks (malloc sin free)

```bash
$ ltrace -e malloc+free -c ./program
% time     seconds  usecs/call     calls      function
------ ----------- ----------- --------- --------------------
 65.00    0.000650          6       100 malloc
 35.00    0.000350          4        85 free
------ ----------- ----------- --------- --------------------
100.00    0.001000                   185 total

# 100 malloc pero solo 85 free → 15 leaks potenciales
```

Para encontrar CUÁLES:

```bash
# Traza detallada para emparejar malloc/free
$ ltrace -e malloc+free ./program 2>&1 | head -20
malloc(64)   = 0x55a230  ← ¿tiene free correspondiente?
malloc(128)  = 0x55a280
free(0x55a230)           ← sí, este se libera
malloc(256)  = 0x55a310
malloc(64)   = 0x55a420
free(0x55a280)           ← este también
# 0x55a310 y 0x55a420 nunca se liberan → leak
```

### 2. Detectar qué configuración carga un programa

```bash
$ ltrace -e 'fopen+fclose' -s 200 ./my_app
fopen("/home/user/.config/myapp/settings.json", "r") = 0x55a234...
fopen("/usr/share/myapp/defaults.json", "r")         = 0x55a235...
fclose(0x55a234...)                                   = 0
fclose(0x55a235...)                                   = 0
```

### 3. Comparar implementaciones

```bash
# ¿Qué funciones de string usa la versión 1?
$ ltrace -c -e 'str*+mem*' ./program_v1
     calls      function
 --------- --------------------
       500 strcmp
       200 strlen
       150 memcpy
        50 strcpy
       900 total

# ¿Y la versión 2 (optimizada)?
$ ltrace -c -e 'str*+mem*' ./program_v2
     calls      function
 --------- --------------------
       100 memcmp       # reemplazó strcmp con memcmp
        50 memcpy
       150 total

# v2 hace 6x menos operaciones de string
```

### 4. Entender el comportamiento de una biblioteca desconocida

```bash
# ¿Qué funciones de libcurl usa mi programa?
$ ltrace -l libcurl.so* -s 200 ./http_client
curl_easy_init()                         = 0x55a234...
curl_easy_setopt(0x55a234..., CURLOPT_URL, "https://api.example.com/data") = 0
curl_easy_setopt(0x55a234..., CURLOPT_WRITEFUNCTION, 0x401200) = 0
curl_easy_perform(0x55a234...)           = 0
curl_easy_getinfo(0x55a234..., CURLINFO_RESPONSE_CODE, 0x7ffd...) = 0
curl_easy_cleanup(0x55a234...)           = <void>
```

### 5. Depurar errores de resolución DNS

```bash
$ ltrace -e 'getaddrinfo+gethostbyname+gai_strerror' -s 200 ./program
getaddrinfo("myserver.local", "8080", 0x7ffd..., 0x7ffd...) = -2
gai_strerror(-2) = "Name or service not known"
# ¡El programa no puede resolver "myserver.local"!
```

---

## Errores comunes

### 1. Usar ltrace con binarios estáticos

```bash
# ltrace requiere enlazado dinámico (shared libraries)
$ file ./my_program
./my_program: ELF 64-bit LSB executable, statically linked

$ ltrace ./my_program
# (nada o error)

# Solución: recompilar con enlazado dinámico (default en gcc)
# o usar strace en su lugar
```

### 2. Confundir ltrace con strace

```bash
# Quieres ver qué archivos abre el programa:
$ ltrace -e open ./program     # ← open no es función de libc estándar
# (no output)

# CORRECTO: usar strace para syscalls
$ strace -e openat ./program   # ← openat es la syscall

# O ltrace para la función de libc:
$ ltrace -e fopen ./program    # ← fopen es la función de libc
```

### 3. No usar -s para ver strings completos

```bash
# Default: strings truncados
$ ltrace ./program
fopen("/home/user/.config/myap"..., "r") = NULL
# ¿Cuál es el path completo?

# Con -s 200:
$ ltrace -s 200 ./program
fopen("/home/user/.config/myapp/very-long-config-name.toml", "r") = NULL
# Ahora sí ves el path completo
```

### 4. Olvidar que ltrace no ve funciones inline o estáticas

```bash
# Funciones marcadas como static o inline en C no pasan por la PLT
# ltrace no las ve

// En el código:
static int helper(int x) { return x * 2; }  // invisible para ltrace
int main() {
    int r = helper(21);  // ltrace no lo muestra
    printf("%d\n", r);   // ltrace SÍ lo muestra
}
```

### 5. Permisos de ptrace en sistemas modernos

```bash
$ ltrace ./program
ltrace: Couldn't attach to process: Operation not permitted

# Causa: yama ptrace_scope restrictivo
$ cat /proc/sys/kernel/yama/ptrace_scope
1

# Soluciones:
# 1. Ejecutar como root: sudo ltrace ./program
# 2. Relajar ptrace_scope temporalmente:
#    echo 0 | sudo tee /proc/sys/kernel/yama/ptrace_scope
# 3. Dar capacidad CAP_SYS_PTRACE al binario de ltrace
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║              LTRACE — REFERENCIA RÁPIDA                  ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  USO BÁSICO                                               ║
║  ──────────                                               ║
║  ltrace ./program               trazar programa           ║
║  ltrace -p PID                  attach a proceso          ║
║  ltrace -o FILE ./program       salida a archivo          ║
║                                                           ║
║  FILTRAR                                                  ║
║  ───────                                                  ║
║  -e FUNC                        función específica        ║
║  -e malloc+free                 varias funciones          ║
║  -e 'str*'                      patrón con wildcard       ║
║  -l libm.so*                    solo de esa biblioteca    ║
║  -x 'my_func*'                 funciones internas        ║
║  -S                             incluir syscalls también  ║
║                                                           ║
║  FORMATO                                                  ║
║  ───────                                                  ║
║  -s N                           longitud de strings       ║
║  -n N                           indentación de profundidad║
║  -C                             demangling C++            ║
║                                                           ║
║  TIMING                                                   ║
║  ──────                                                   ║
║  -T                             duración de cada llamada  ║
║  -t / -tt                       timestamp absoluto        ║
║  -r                             tiempo relativo           ║
║                                                           ║
║  ESTADÍSTICAS                                             ║
║  ────────────                                             ║
║  -c                             resumen al final          ║
║                                                           ║
║  PROCESOS                                                 ║
║  ────────                                                 ║
║  -f                             seguir fork/clone         ║
║  -p PID                         attach a proceso          ║
║                                                           ║
║  LTRACE vs STRACE                                         ║
║  ────────────────                                         ║
║  ltrace: malloc, printf, strcmp  (funciones de .so)       ║
║  strace: write, read, openat    (syscalls del kernel)    ║
║  ltrace -S: ambos niveles                                ║
║                                                           ║
║  PATRONES COMUNES                                         ║
║  ────────────────                                         ║
║  ltrace -e malloc+free -c prog  contar alloc/free        ║
║  ltrace -e fopen -s 200 prog    ver qué archivos abre   ║
║  ltrace -l libcurl.so* prog     uso de una biblioteca    ║
║  ltrace -S -e malloc prog       malloc → syscall         ║
║                                                           ║
║  REQUISITOS                                               ║
║  ──────────                                               ║
║  Solo binarios dinámicamente enlazados                    ║
║  ptrace_scope puede bloquear (ver /proc/sys/kernel/yama) ║
║  No ve funciones static/inline                           ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Auditoría de uso de memoria

Escribe un programa C que haga varias asignaciones de memoria, algunas con leak intencional:

```c
// memaudit.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* create_greeting(const char *name) {
    // strlen + 10 para "Hello, " + "!\n" + null
    char *buf = malloc(strlen(name) + 10);
    sprintf(buf, "Hello, %s!\n", name);
    return buf;
}

void process_names(const char **names, int count) {
    for (int i = 0; i < count; i++) {
        char *greeting = create_greeting(names[i]);
        printf("%s", greeting);

        // Bug: solo libera los nombres con índice par
        if (i % 2 == 0) {
            free(greeting);
        }
        // Los índices impares se pierden → memory leak
    }
}

int main() {
    const char *names[] = {"Alice", "Bob", "Charlie", "Diana", "Eve", "Frank"};
    process_names(names, 6);

    // Asignación adicional sin liberar
    char *extra = malloc(1024);
    strcpy(extra, "this will leak");

    return 0;
}
```

Usa ltrace para:
1. Contar cuántos `malloc` y `free` se ejecutan (`-c`).
2. Listar cada `malloc` con su dirección de retorno y cada `free` con su argumento.
3. Identificar qué direcciones de `malloc` nunca se pasan a `free`.
4. Calcular cuántos bytes se pierden.

**Pregunta de reflexión**: ltrace puede detectar el **número** de leaks, pero ¿puede decirte **en qué línea** del código fuente ocurrieron? ¿Qué herramienta sería mejor para eso? (Pista: Valgrind, AddressSanitizer.)

---

### Ejercicio 2: Comparar ltrace y strace

Escribe un programa que haga operaciones de archivo y red, y trárzalo con **ambas** herramientas:

```c
// dual_trace.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
    // Operación de archivo
    FILE *f = fopen("/etc/hostname", "r");
    if (f) {
        char buf[256];
        fgets(buf, sizeof(buf), f);
        printf("Hostname: %s", buf);
        fclose(f);
    }

    // Operación de red (connect que fallará)
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock >= 0) {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(9999);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        int ret = connect(sock, (struct sockaddr*)&addr, sizeof(addr));
        if (ret < 0) {
            perror("connect failed");
        }
        close(sock);
    }

    return 0;
}
```

Ejecuta:
```bash
ltrace -o ltrace.log ./dual_trace
strace -o strace.log ./dual_trace
ltrace -S -o combined.log ./dual_trace
```

Compara las tres salidas y documenta:
1. ¿Qué ve ltrace que strace no ve?
2. ¿Qué ve strace que ltrace no ve?
3. ¿Cómo se relacionan `fopen` (ltrace) con `openat` (strace)?
4. ¿Cómo se ve la llamada `connect` en cada herramienta?

**Pregunta de reflexión**: si el programa tuviera un bug donde `fclose` no se llama, ¿qué herramienta lo detectaría más fácilmente — ltrace o strace? ¿Por qué?

---

### Ejercicio 3: Análisis de rendimiento con ltrace

Escribe dos versiones de un programa que busca un string en un array — una ineficiente y una optimizada:

```c
// search_v1.c — versión ineficiente
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main() {
    const char *words[] = {
        "alpha", "bravo", "charlie", "delta", "echo",
        "foxtrot", "golf", "hotel", "india", "juliet"
    };
    int n = 10;
    const char *target = "hotel";

    // Búsqueda ineficiente: recalcula strlen en cada comparación
    int found = -1;
    for (int i = 0; i < n; i++) {
        if (strlen(words[i]) == strlen(target) &&
            strcmp(words[i], target) == 0) {
            found = i;
            break;
        }
    }

    printf("Found '%s' at index %d\n", target, found);
    return 0;
}
```

```c
// search_v2.c — versión optimizada
#include <stdio.h>
#include <string.h>

int main() {
    const char *words[] = {
        "alpha", "bravo", "charlie", "delta", "echo",
        "foxtrot", "golf", "hotel", "india", "juliet"
    };
    int n = 10;
    const char *target = "hotel";

    int found = -1;
    for (int i = 0; i < n; i++) {
        if (strcmp(words[i], target) == 0) {
            found = i;
            break;
        }
    }

    printf("Found '%s' at index %d\n", target, found);
    return 0;
}
```

Usa `ltrace -c -e 'str*'` para comparar:
1. ¿Cuántas llamadas a `strlen` hace v1 vs v2?
2. ¿Cuántas llamadas a `strcmp` hace cada versión?
3. ¿Cuál es el total de llamadas a funciones de string?

**Pregunta de reflexión**: en la versión 1, `strlen(target)` se recalcula en cada iteración del loop. ¿El compilador podría optimizar esto (sacar el cálculo fuera del loop)? ¿Con qué flag de optimización? Prueba con `gcc -O2` y verifica con ltrace si el compilador efectivamente optimizó.
