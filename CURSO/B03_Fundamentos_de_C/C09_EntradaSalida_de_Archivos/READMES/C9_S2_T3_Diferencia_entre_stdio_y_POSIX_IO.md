# T03 — Diferencia entre stdio y POSIX I/O

> Sin erratas detectadas en el material fuente.

---

## 1. Dos niveles de I/O — La cadena completa

Linux ofrece dos APIs de I/O. Una está construida sobre la otra:

```
Tu código
    │
    ├── stdio (alto nivel)     ← fopen, fread, fprintf, fclose
    │       │
    │       ├── buffer en espacio de usuario (8 KB típico)
    │       │
    │       └── syscalls ─────── open, read, write, close
    │                                │
    └── POSIX I/O (bajo nivel) ──────┘
                                     │
                              ┌──────┘
                              │
                         Kernel Linux
                              │
                         Hardware (disco, red, terminal)
```

```c
// stdio usa POSIX internamente:
//   fopen("f", "r")   → open("f", O_RDONLY)      + crea buffer
//   fread(buf, 1, n, f) → read(fd, internal_buf, 8192) cuando buffer vacío
//   fprintf(f, ...)   → formatea string → copia al buffer → write() cuando lleno
//   fclose(f)         → fflush(f) + close(fd) + free(buffer)
//   fseek(f, ...)     → fflush(f) + lseek(fd, ...)

// Lo que stdio AGREGA sobre POSIX:
// 1. Buffering automático (reduce syscalls)
// 2. Formateo (printf, scanf)
// 3. Portabilidad (funciona en Windows, donde POSIX no existe)
// 4. Conversión texto/binario (newline: \n ↔ \r\n en Windows)
// 5. Thread safety (locks internos en cada operación)
```

### Analogía

```
POSIX I/O = enviar cartas una por una al correo
stdio     = acumular cartas en un buzón y enviarlas todas juntas

El resultado es el mismo. La eficiencia es muy diferente.
```

---

## 2. Tabla de equivalencias

| stdio (`FILE*`) | POSIX (`int fd`) | Notas |
|----------------|-----------------|-------|
| `fopen(path, mode)` | `open(path, flags, perm)` | stdio: string mode (`"r"`). POSIX: flags bitwise (`O_RDONLY`) |
| `fclose(f)` | `close(fd)` | `fclose` hace `fflush` + `close` |
| `fread(buf, 1, n, f)` | `read(fd, buf, n)` | stdio retorna elementos, POSIX retorna bytes |
| `fwrite(buf, 1, n, f)` | `write(fd, buf, n)` | POSIX puede short write; `fwrite` no |
| `fprintf(f, fmt, ...)` | — (manual: `snprintf` + `write`) | Sin equivalente directo en POSIX |
| `fgets(buf, n, f)` | — (manual: `read` byte a byte o buffer con búsqueda de `'\n'`) | POSIX no entiende de líneas |
| `fseek(f, off, whence)` | `lseek(fd, off, whence)` | `fseek` retorna 0/-1; `lseek` retorna posición |
| `ftell(f)` | `lseek(fd, 0, SEEK_CUR)` | `lseek` hace ambas cosas |
| `rewind(f)` | `lseek(fd, 0, SEEK_SET)` | `rewind` también limpia el flag de error |
| `feof(f)` / `ferror(f)` | Verificar retorno de `read()` | POSIX: 0=EOF, -1=error |
| `fileno(f)` | — | Obtener fd de un `FILE*` |
| `fdopen(fd, mode)` | — | Crear `FILE*` desde un fd |
| — | `dup(fd)` / `dup2(fd, newfd)` | Sin equivalente stdio |
| — | `mmap(...)` | Requiere fd, no existe en stdio |
| — | `poll` / `epoll` / `select` | Requieren fds |

---

## 3. Buffering — La diferencia fundamental

### Cómo funciona el buffer de stdio

```c
// Cuando haces fwrite(buf, 1, 100, f):
//   1. stdio copia 100 bytes a su buffer INTERNO (en user space)
//   2. Si el buffer NO está lleno → retorna inmediatamente (0 syscalls)
//   3. Si el buffer SE LLENA → llama write(fd, internal_buf, 8192)
//
// Cuando haces write(fd, buf, 100):
//   1. SIEMPRE hace una syscall (cambio user→kernel→user)
//   2. Retorna cuando el kernel aceptó los datos

// El costo de una syscall:
//   ~200-500 nanosegundos (cambio de contexto, seguridad, etc.)
//   Con 4 millones de syscalls: ~1-2 SEGUNDOS solo en overhead
//   Con 500 syscalls (buffered): ~0.0001 segundos de overhead
```

### stdio NO hace buffering de lectura mágico

```c
// Un malentendido común:
// "fread con buffer de 4096 y read con buffer de 4096 hacen lo mismo"

// FALSO. fread(buf, 1, 4096, f) puede NO hacer ninguna syscall:
//   - stdio tiene un buffer interno de 8192 bytes
//   - Primera fread(4096): stdio llama read(fd, internal, 8192)
//     → obtiene 8192 bytes del kernel
//     → copia 4096 bytes a tu buf
//     → guarda 4096 bytes en su buffer
//   - Segunda fread(4096): NO hay syscall
//     → copia los 4096 bytes que ya tenía guardados
//
// Resultado para 64 KB:
//   fread(4096) × 16 llamadas → solo 8 syscalls reales
//   read(4096)  × 16 llamadas → 16 syscalls reales
```

### Dónde POSIX gana: control total

```c
// Con POSIX, tú eliges el tamaño del buffer:
char buf[65536];   // 64 KB — menos syscalls que los 8 KB de stdio
while ((n = read(fd, buf, sizeof(buf))) > 0) { ... }
// 64KB / 64KB = 1024 syscalls para 64 MB

// Con stdio, el buffer interno es fijo (BUFSIZ, típicamente 8192).
// Puedes cambiarlo con setvbuf, pero es poco común.
```

---

## 4. Cuándo usar stdio

### Texto formateado

```c
// fprintf es incomparablemente más cómodo que write para texto:

// stdio:
fprintf(log, "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s\n",
        tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec,
        level, message);

// POSIX equivalente:
char buf[512];
int len = snprintf(buf, sizeof(buf),
    "[%04d-%02d-%02d %02d:%02d:%02d] %s: %s\n",
    tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday,
    tm.tm_hour, tm.tm_min, tm.tm_sec,
    level, message);
write(fd, buf, (size_t)len);
// Dos pasos en vez de uno, y tú gestionas el buffer.
```

### Interacción con usuario

```c
// stdio maneja line buffering automáticamente en terminales:
printf("Enter name: ");   // Se muestra inmediatamente (line-buffered, ve el \n pendiente)
fgets(name, sizeof(name), stdin);   // Lee una línea completa

// Con POSIX:
write(STDOUT_FILENO, "Enter name: ", 12);
// Luego leer de STDIN_FILENO... ¿hasta dónde?
// read() no entiende de líneas — necesitas buscar '\n' manualmente.
```

### Portabilidad (Windows)

```c
// stdio es C estándar → funciona en TODOS los sistemas:
//   Linux, macOS, Windows, embebidos, etc.

// POSIX I/O NO existe en Windows:
//   open()  → CreateFile()
//   read()  → ReadFile()
//   write() → WriteFile()
//   close() → CloseHandle()
//   Tipos y semántica diferentes.

// Si tu código debe compilar en Windows: stdio obligatorio.
// Si solo corre en Linux/Unix: puedes elegir cualquiera.
```

### Archivos de configuración

```c
// Leer un archivo de configuración línea a línea:
FILE *f = fopen("/etc/app.conf", "r");
char line[256];
while (fgets(line, sizeof(line), f)) {
    char key[64], value[64];
    if (sscanf(line, "%63s = %63s", key, value) == 2) {
        apply_config(key, value);
    }
}
fclose(f);
// fgets + sscanf: simple, seguro, portable.
```

---

## 5. Cuándo usar POSIX I/O

### Sockets y redes

```c
// socket() retorna un fd, no un FILE*:
int sockfd = socket(AF_INET, SOCK_STREAM, 0);
connect(sockfd, ...);
write(sockfd, request, request_len);
read(sockfd, response, sizeof(response));
close(sockfd);

// Se PUEDE crear un FILE* con fdopen, pero es poco común:
// - Sockets necesitan control fino de envío/recepción
// - El buffering de stdio puede causar deadlocks en protocolos interactivos
//   (espera datos que están en tu buffer sin enviar)
```

### Multiplexación con poll/epoll

```c
// poll, epoll y select trabajan con fds:
struct pollfd fds[100];
fds[0].fd = listen_fd;
fds[0].events = POLLIN;
// ...
poll(fds, nfds, timeout);

// No puedes pasar un FILE* a poll.
// No puedes hacer non-blocking I/O con stdio.
```

### mmap (memory-mapped I/O)

```c
// mmap requiere un fd:
int fd = open("data.bin", O_RDONLY);
void *map = mmap(NULL, file_size, PROT_READ, MAP_PRIVATE, fd, 0);
// Acceder a map como un array — el kernel carga páginas bajo demanda.
// No hay equivalente en stdio.
```

### Signal handlers

```c
// write() es async-signal-safe; fprintf() NO:
void handler(int sig) {
    // SEGURO:
    const char msg[] = "Signal caught\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);

    // PELIGROSO — puede causar deadlock:
    // fprintf(stderr, "Signal %d caught\n", sig);
    // ¿Por qué? Si la señal interrumpe otro fprintf que tiene el lock
    // del buffer de stderr, fprintf en el handler intenta tomar el mismo
    // lock → deadlock.
}
```

### Flags especiales de open

```c
// Estas funcionalidades solo existen con open():

// Non-blocking I/O:
int fd = open("/dev/ttyUSB0", O_RDWR | O_NONBLOCK);

// Escritura síncrona (cada write va al disco, no al cache):
int fd = open("critical.log", O_WRONLY | O_CREAT | O_SYNC, 0644);

// Close-on-exec (no heredar fd al hacer exec):
int fd = open("secret.conf", O_RDONLY | O_CLOEXEC);

// Creación exclusiva atómica:
int fd = open("lockfile", O_WRONLY | O_CREAT | O_EXCL, 0644);

// Permisos específicos al crear:
int fd = open("private.key", O_WRONLY | O_CREAT | O_TRUNC, 0600);

// fopen no tiene equivalente para ninguno de estos flags.
```

---

## 6. fileno y fdopen — Puente entre APIs

### fileno: obtener el fd de un FILE*

```c
#include <stdio.h>

FILE *f = fopen("data.txt", "w");
int fd = fileno(f);
// fd es el file descriptor subyacente del FILE*.

// Uso típico: necesitas el fd para una operación POSIX
// sobre un archivo que abriste con stdio:
fstat(fileno(f), &st);       // Obtener tamaño/permisos
fchmod(fileno(f), 0600);     // Cambiar permisos
fsync(fileno(f));             // Forzar escritura a disco
flock(fileno(f), LOCK_EX);   // Lock exclusivo
```

### fdopen: crear FILE* desde un fd

```c
#include <stdio.h>

int fd = open("data.txt", O_RDONLY);
FILE *f = fdopen(fd, "r");
// Ahora puedes usar fgets, fprintf, etc. sobre el fd.

// Uso típico: recibes un fd (de socket, pipe, etc.)
// y quieres usar el formateo de stdio:
int pipefd[2];
pipe(pipefd);
FILE *pipe_read = fdopen(pipefd[0], "r");
char line[256];
fgets(line, sizeof(line), pipe_read);  // Leer línea del pipe
```

### Regla crítica: propiedad del fd

```c
// Después de fdopen, el FILE* es DUEÑO del fd.
// fclose(f) cierra TANTO el FILE* como el fd.

int fd = open("data.txt", O_RDONLY);
FILE *f = fdopen(fd, "r");
fclose(f);     // Cierra f Y fd
// close(fd);  // ¡NO! fd ya está cerrado → double close → UB

// En la otra dirección:
FILE *f = fopen("data.txt", "r");
int fd = fileno(f);
fclose(f);     // Cierra f Y fd
// close(fd);  // ¡NO! Same problem.

// Resumen:
// - Un recurso, un dueño, un close.
// - fclose siempre cierra el fd subyacente.
// - Nunca hacer close(fd) después de fclose.
// - Nunca hacer fclose después de close(fd).
```

---

## 7. Peligro de mezclar APIs

### El problema: orden invertido

```c
FILE *f = fopen("out.txt", "w");
int fd = fileno(f);

fprintf(f, "AAA\n");                      // → buffer de stdio (NO al kernel)
write(fd, "BBB\n", 4);                    // → directo al kernel
fprintf(f, "CCC\n");                      // → buffer de stdio

fclose(f);  // fflush + close → AAA y CCC van al kernel AHORA

// Contenido del archivo: BBB\nAAA\nCCC\n
// ¡BBB aparece primero porque write() fue directo!
// AAA y CCC estaban esperando en el buffer de stdio.
```

### La solución: fflush antes de cambiar de API

```c
FILE *f = fopen("out.txt", "w");
int fd = fileno(f);

fprintf(f, "AAA\n");
fflush(f);                    // Forzar: AAA va al kernel AHORA
write(fd, "BBB\n", 4);       // BBB va al kernel
fprintf(f, "CCC\n");

fclose(f);  // CCC va al kernel

// Contenido: AAA\nBBB\nCCC\n ← orden correcto
```

### La mejor solución: no mezclar

```c
// Opción 1: usar SOLO stdio
FILE *f = fopen("out.txt", "w");
fprintf(f, "AAA\n");
fprintf(f, "BBB\n");
fprintf(f, "CCC\n");
fclose(f);

// Opción 2: usar SOLO POSIX
int fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
write(fd, "AAA\n", 4);
write(fd, "BBB\n", 4);
write(fd, "CCC\n", 4);
close(fd);

// Opción 3: si recibes un fd pero quieres stdio, usar fdopen
int fd = open("out.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
FILE *f = fdopen(fd, "w");
// Ahora usar SOLO f (stdio). fclose(f) cierra todo.
```

### Caso especial: stdout

```c
// printf() usa stdout (FILE*).
// write(STDOUT_FILENO, ...) usa fd 1.
// Son el MISMO recurso — mezclarlos tiene el mismo problema:

printf("Via printf\n");                       // buffer de stdout
write(STDOUT_FILENO, "Via write\n", 10);     // directo al kernel

// Con stdout en modo line-buffered (terminal), el '\n' de printf
// fuerza un flush, así que el orden suele ser correcto.
// Con stdout redirigido a archivo (full-buffered), el orden se rompe.

// Regla segura: fflush(stdout) antes de write(STDOUT_FILENO, ...).
```

---

## 8. Rendimiento — El impacto de las syscalls

### Benchmark: 4 estrategias para escribir 4 MB

| Estrategia | Tiempo | Syscalls | Análisis |
|-----------|--------|----------|----------|
| `write()` 1 byte a la vez | ~800 ms | 4,194,304 | Cada byte = 1 syscall. Catastrófico |
| `write()` buffer 4096 | ~3 ms | 1,024 | 4096× menos syscalls = 200-400× más rápido |
| `fputc()` 1 byte a la vez | ~20 ms | ~512 | stdio acumula ~8 KB → solo ~512 syscalls |
| `fwrite()` buffer 4096 | ~2 ms | ~1,024 | Similar a write(4096) + overhead mínimo de stdio |

### Lecciones clave

```c
// 1. write(1 byte) vs fputc(1 byte):
//    fputc es ~40x más rápido porque stdio bufferiza.
//    Ambos escriben 1 byte desde tu perspectiva.
//    La diferencia son 4M syscalls vs ~512 syscalls.

// 2. write(4096) vs fwrite(4096):
//    Rendimiento casi idéntico.
//    Cuando el programador ya usa buffers grandes,
//    stdio agrega poco beneficio (pero tampoco estorba).

// 3. La lección:
//    - Si usas POSIX I/O: SIEMPRE usa buffers grandes (≥4096 bytes)
//    - Si usas stdio: puedes escribir byte a byte sin penalización
//    - stdio existe para proteger al programador de sus propios errores
//      de buffering.
```

### Verificar con strace

```bash
# Contar syscalls reales:
strace -c ./program 2>&1 | tail -20

# Ver cada syscall individualmente:
strace -e trace=read,write ./program 2>&1 | head -30

# Solo contar write:
strace -c -e trace=write ./program 2>&1 | grep write
```

---

## 9. Tabla de decisión — Escenarios prácticos

| Escenario | API | Justificación |
|-----------|-----|---------------|
| Leer CSV y generar reporte formateado | **stdio** | `fgets` + `sscanf` para parsear, `fprintf` con `%-20s %10d` para columnas |
| Servidor TCP con epoll (1000 conexiones) | **POSIX** | `epoll` requiere fds. Sockets son fds |
| Logger que escribe inmediatamente al disco | **POSIX** | `open` con `O_SYNC` garantiza escritura a disco sin cache |
| Copiar archivos binarios grandes (>1 GB) | **POSIX** | Buffer grande (64 KB+) o `sendfile()` / `copy_file_range()` |
| Signal handler que reporta errores | **POSIX** | `write()` es async-signal-safe, `fprintf()` no |
| Parsear archivos de configuración | **stdio** | Lectura línea a línea, formateo con sscanf. Portable |
| Comunicación con hardware serial | **POSIX** | `O_NONBLOCK`, control de terminal con `termios` |
| Script rápido que lee/escribe texto | **stdio** | Menos código, menos errores, buffers automáticos |
| Crear archivo con permisos 0600 | **POSIX** | `open(path, O_CREAT, 0600)`. `fopen` no controla permisos |
| Programa que debe compilar en Windows | **stdio** | C estándar. POSIX no existe en Windows |

### Criterios rápidos

```
¿Necesitas sockets, mmap, poll/epoll, O_NONBLOCK?  → POSIX
¿Escribes en un signal handler?                      → POSIX (write)
¿Necesitas permisos específicos al crear?             → POSIX (open)
¿Es texto formateado?                                → stdio (fprintf)
¿Debe funcionar en Windows?                           → stdio
¿No tienes preferencia?                               → stdio (más seguro por defecto)
```

---

## 10. Comparación con Rust

Rust unifica ambos niveles de C en una sola jerarquía de traits:

| Concepto C | Rust | Notas |
|-----------|------|-------|
| `FILE*` con buffering | `BufReader<File>` / `BufWriter<File>` | Buffering explícito, no automático |
| `int fd` sin buffering | `std::fs::File` | File SIN buffer por defecto |
| `fopen("f", "r")` | `File::open("f")?` | Retorna `Result`, no NULL |
| `open(flags)` | `OpenOptions::new().read(true).write(true).open("f")?` | Builder pattern |
| `fprintf` | `write!(f, "{}", val)?` o `writeln!` | Macro, type-safe en compilación |
| `fgets` | `reader.read_line(&mut s)?` | Retorna `Result<usize>` |
| `fileno(f)` | `f.as_raw_fd()` | Trait `AsRawFd` (solo Unix) |
| `fdopen(fd, "r")` | `File::from_raw_fd(fd)` | Toma ownership (unsafe) |
| `fclose` / `close` | Automático (Drop) | RAII: se cierra al salir del scope |
| Mezclar stdio/POSIX | No existe el problema | Un solo sistema de tipos |

```rust
use std::fs::File;
use std::io::{self, BufRead, BufReader, Write, BufWriter};

fn process_log(path: &str) -> io::Result<()> {
    // Sin buffer (como POSIX read/write):
    let file = File::open(path)?;

    // Con buffer (como stdio fgets):
    let reader = BufReader::new(file);

    // Iterar líneas (como fgets en loop):
    for line in reader.lines() {
        let line = line?;
        println!("{}", line);
    }
    Ok(())
}

fn write_fast(path: &str) -> io::Result<()> {
    let file = File::create(path)?;

    // BufWriter agrupa writes como stdio:
    let mut writer = BufWriter::new(file);

    for i in 0..1000 {
        writeln!(writer, "Line {}", i)?;   // Buffered
    }
    // writer.flush() implícito en Drop, o explícito con writer.flush()?
    Ok(())
}
```

**Diferencia filosófica**:
- **C**: dos APIs separadas (stdio vs POSIX), mezclarlas es peligroso, el programador elige la correcta.
- **Rust**: una API (`Read`/`Write` traits), buffering es una capa explícita (`BufReader`/`BufWriter`), no hay forma de "mezclar" porque el sistema de ownership impide acceder al fd subyacente sin pasar por el wrapper.

```rust
// En Rust, el problema de "mezclar APIs" no existe:
let file = File::create("out.txt")?;
let mut buf = BufWriter::new(file);

writeln!(buf, "AAA")?;   // Va al buffer
// write(fd, "BBB", 3);  // No puedes — buf es dueño del File
//                        // Para acceder al fd necesitarías buf.get_ref().as_raw_fd()
//                        // y hacer unsafe. El sistema de tipos te protege.
```

---

## Ejercicios

### Ejercicio 1 — Copiar archivo con ambas APIs

```c
// Implementa dos funciones:
//   int copy_stdio(const char *src, const char *dst);
//   int copy_posix(const char *src, const char *dst);
//
// Ambas copian un archivo. Buffer de 4096 bytes.
// copy_posix debe manejar short writes.
//
// Crear archivo de prueba: dd if=/dev/urandom of=test.bin bs=1K count=64
// Ejecutar ambas. Verificar con md5sum que las copias son idénticas.
//
// Contar syscalls con: strace -c ./programa 2>&1 | tail -20
// ¿Cuántas syscalls read hace cada una? ¿Por qué la diferencia?
```

<details><summary>Predicción</summary>

Para 64 KB (65536 bytes) con buffer de 4096:

**copy_posix**: `read(fd, buf, 4096)` hace 16 syscalls (65536 / 4096). Cada `read()` es una syscall directa.

**copy_stdio**: `fread(buf, 1, 4096, f)` hace ~8 syscalls. stdio tiene un buffer interno de 8192 bytes (BUFSIZ). Cada syscall `read` real pide 8192 bytes al kernel. Dos llamadas a `fread(4096)` consumen un buffer de 8192 → mitad de syscalls.

Las copias serán idénticas (mismo md5sum). La diferencia es solo interna: quién gestiona el buffer más grande.

Para la escritura: `fwrite` y `write(4096)` harán cantidades similares de syscalls `write` porque el buffer del usuario ya es grande.

</details>

### Ejercicio 2 — Demostrar el orden invertido al mezclar

```c
// 1. Abre "mixed.txt" con fopen("mixed.txt", "w")
// 2. fprintf: "FIRST\n"
// 3. write(fileno(f), "SECOND\n", 7)
// 4. fprintf: "THIRD\n"
// 5. fclose
//
// Lee y muestra el contenido.
// ¿El orden es FIRST, SECOND, THIRD?
//
// Repite con fflush(f) entre el paso 2 y 3.
// ¿Ahora el orden es correcto?
//
// Repite una tercera vez redirigiendo stdout del programa a un archivo:
//   ./mix_test > salida.txt
// ¿El orden de printf cambia vs ejecutar en terminal?
```

<details><summary>Predicción</summary>

**Sin fflush**: el archivo contiene "SECOND\nFIRST\nTHIRD\n". `write()` va directo al kernel. `fprintf` escribe al buffer de stdio, que se vuelca en `fclose`. SECOND aparece primero porque llegó al kernel primero.

**Con fflush**: el archivo contiene "FIRST\nSECOND\nTHIRD\n". `fflush` fuerza FIRST al kernel antes de que `write` envíe SECOND. Orden correcto.

**Redirigiendo stdout**: si el programa también usa `printf` para mensajes de diagnóstico, al redirigir a archivo stdout cambia de line-buffered a full-buffered. Los `printf` se acumularán en el buffer hasta `exit()` o buffer lleno, pudiendo aparecer después de `write(STDOUT_FILENO, ...)` si se mezclan.

</details>

### Ejercicio 3 — Benchmark: 4 estrategias de escritura

```c
// Escribe 4 MB a un archivo temporal con 4 estrategias:
//   1. write(fd, &byte, 1) en loop (4M syscalls)
//   2. write(fd, buf, 4096) en loop (~1K syscalls)
//   3. fputc(byte, f) en loop (stdio buffered ~512 syscalls)
//   4. fwrite(buf, 1, 4096, f) en loop (~1K syscalls)
//
// Medir con clock_gettime(CLOCK_MONOTONIC).
// Imprimir tabla: Estrategia | Tiempo | Ratio vs write(4096)
//
// Verificar con strace -c que los conteos de syscall coinciden
// con las predicciones.
```

<details><summary>Predicción</summary>

Tiempos esperados (varían según hardware, orden de magnitud):

| Estrategia | Tiempo | Ratio | Syscalls |
|-----------|--------|-------|----------|
| write(1 byte) | ~500-1000 ms | ~200-400x | 4,194,304 |
| write(4096) | ~2-5 ms | 1x (baseline) | 1,024 |
| fputc(1 byte) | ~15-30 ms | ~5-10x | ~512 |
| fwrite(4096) | ~2-5 ms | ~1x | ~1,024 |

La estrategia 1 es catastrófica: cada byte causa una transición user→kernel→user. La estrategia 3 (fputc) es ~40x más rápida que write(1) aunque ambas "escriben 1 byte" — porque stdio acumula internamente ~8192 bytes antes de hacer la syscall real.

Las estrategias 2 y 4 son casi idénticas porque cuando el programador ya usa buffers de 4 KB, stdio agrega overhead mínimo.

</details>

### Ejercicio 4 — fileno y fdopen en acción

```c
// Parte A — fileno:
//   Abre un archivo con fopen, escribe con fprintf.
//   Usa fileno() + fstat() para obtener tamaño y permisos.
//   Usa fileno() + fsync() para forzar escritura a disco.
//   Imprime: "Size: N bytes, Permissions: 0644"
//
// Parte B — fdopen:
//   Crea un pipe con pipe(pipefd).
//   fork(). El hijo escribe al pipe con write().
//   El padre crea FILE* con fdopen(pipefd[0], "r").
//   El padre lee líneas con fgets() y las imprime.
//   Verificar: fclose(f) ¿también cierra el fd del pipe?
```

<details><summary>Predicción</summary>

**Parte A**: `fileno(f)` retorna 3 (primer fd después de 0,1,2). `fstat(fileno(f), &st)` da `st.st_size` (tamaño del archivo hasta el último flush) y `st.st_mode & 0777` (permisos, 0644). `fsync(fileno(f))` fuerza los datos al disco físico (no solo al page cache).

Importante: para que fstat reporte el tamaño correcto, primero hay que hacer `fflush(f)` — los datos en el buffer de stdio no están en el archivo todavía.

**Parte B**: El hijo escribe "Hello from child\n" con `write(pipefd[1], ...)`. El padre hace `fdopen(pipefd[0], "r")` y lee con `fgets`. fgets retorna "Hello from child\n". `fclose(f)` cierra tanto el FILE* como pipefd[0].

Para verificar: después de fclose, intentar `read(pipefd[0], buf, 1)` retornará -1 con errno EBADF (fd inválido), confirmando que fclose cerró el fd.

</details>

### Ejercicio 5 — Elegir la API correcta

```c
// Para cada escenario, implementa la solución con la API apropiada
// y justifica tu elección en un comentario:
//
// a) Leer /etc/passwd y mostrar usuario:UID para cada línea
// b) Escribir un mensaje en un signal handler para SIGINT
// c) Crear un archivo con permisos 0600 y escribir una clave secreta
// d) Leer un archivo grande y contar la frecuencia de cada byte (0-255)
// e) Copiar stdin a stdout (como cat sin argumentos)
//
// Para (e): implementar con stdio Y con POSIX.
// ¿Cuál es más simple? ¿Cuál maneja mejor stdin desde un pipe?
```

<details><summary>Predicción</summary>

a) **stdio**: `fgets` + `sscanf(line, "%[^:]:%*[^:]:%d", user, &uid)`. Lectura línea a línea es natural con fgets.

b) **POSIX**: `write(STDERR_FILENO, msg, len)` dentro del handler. No se puede usar fprintf (no es async-signal-safe).

c) **POSIX**: `open("secret.key", O_WRONLY | O_CREAT | O_TRUNC, 0600)`. fopen no permite especificar permisos.

d) **POSIX con buffer grande**: `read(fd, buf, 65536)` en loop, iterando cada byte del buffer. Los contadores van en `int freq[256] = {0}`. POSIX con buffer grande > stdio aquí, porque solo contamos bytes — no necesitamos formateo.

e) **stdio**: `while ((c = fgetc(stdin)) != EOF) fputc(c, stdout)` — 2 líneas. **POSIX**: loop con `read(STDIN_FILENO, buf, 4096)` + `write(STDOUT_FILENO, buf, n)` — más líneas pero más eficiente para volumen grande. Para pipes, ambos funcionan correctamente; POSIX es ligeramente más eficiente si el buffer es grande.

</details>

### Ejercicio 6 — strace para entender el buffering

```c
// Escribe un programa simple que:
//   printf("A");
//   printf("B");
//   printf("C\n");
//   fprintf(stderr, "X");
//   fprintf(stderr, "Y\n");
//
// Ejecuta con strace -e trace=write para ver las syscalls.
//
// Preguntas:
// 1. ¿Cuántas syscalls write se generan para stdout?
// 2. ¿Cuántas para stderr?
// 3. Si rediriges stdout a un archivo (./prog > out.txt),
//    ¿cambia el número de syscalls? ¿Por qué?
// 4. Si agregas setbuf(stdout, NULL) al inicio, ¿qué cambia?
```

<details><summary>Predicción</summary>

**En terminal (stdout line-buffered, stderr unbuffered)**:
- stdout: 1 syscall `write(1, "ABC\n", 4)` — los tres printf se acumulan en el buffer y se envían juntos al encontrar '\n'.
- stderr: 2 syscalls: `write(2, "X", 1)` y `write(2, "Y\n", 2)` — stderr es unbuffered, cada fprintf es una syscall.

**Redirigido a archivo (stdout full-buffered)**:
- stdout: 1 syscall al final (en exit/fflush implícito). Posiblemente `write(1, "ABC\n", 4)`.
- stderr: sigue siendo 2 syscalls (stderr siempre es unbuffered).

El número no cambia mucho para este ejemplo pequeño, pero la diferencia sería visible con más datos: en terminal, cada '\n' causa un flush; redirigido, todo se acumula hasta que el buffer de 8 KB se llena.

**Con `setbuf(stdout, NULL)` (unbuffered)**:
- stdout: 3 syscalls: `write(1, "A", 1)`, `write(1, "B", 1)`, `write(1, "C\n", 2)`. Cada printf es una syscall inmediata — como stderr.

</details>

### Ejercicio 7 — safe_signal_write: wrapper para signal handlers

```c
// Implementa una función que sea segura para usar en signal handlers:
//
// void safe_write_str(int fd, const char *str);
// void safe_write_int(int fd, int value);
//
// Restricciones de async-signal-safety:
// - NO usar printf, fprintf, sprintf, snprintf
// - NO usar malloc, free
// - NO usar strlen (¿es segura? investiga)
// - SOLO usar write()
//
// Para safe_write_int: convertir int a dígitos manualmente
// (como el ejercicio del T01).
//
// Registrar un handler para SIGINT que imprima:
//   "Signal 2 received, count = N\n"
// donde N es un contador de señales (usar volatile sig_atomic_t).
// Probar con: ./signal_test  (luego Ctrl+C varias veces, Ctrl+\ para salir)
```

<details><summary>Predicción</summary>

`strlen` NO está en la lista POSIX de funciones async-signal-safe. Para ser estrictamente correcto, hay que calcular la longitud manualmente o usar un buffer de tamaño fijo.

`safe_write_str`: recorrer la cadena contando hasta '\0', luego `write(fd, str, len)`. O escribir byte a byte (menos eficiente pero simple).

`safe_write_int`: usar el algoritmo `% 10` / `/ 10` para extraer dígitos, escribir en un buffer local (stack), invertir, luego `write()`.

El handler:
```c
volatile sig_atomic_t count = 0;

void handler(int sig) {
    count++;
    safe_write_str(STDERR_FILENO, "Signal ");
    safe_write_int(STDERR_FILENO, sig);
    safe_write_str(STDERR_FILENO, " received, count = ");
    safe_write_int(STDERR_FILENO, count);
    safe_write_str(STDERR_FILENO, "\n");
}
```

Cada Ctrl+C incrementa el contador. La salida es correcta y thread-safe porque solo usa `write()`.

</details>

### Ejercicio 8 — Detectar si stdout es terminal o archivo

```c
// Usa isatty(STDOUT_FILENO) para detectar si stdout va a un terminal
// o está redirigido a archivo/pipe.
//
// Si es terminal: imprimir con colores ANSI ("\033[31m" rojo, etc.)
// Si es archivo/pipe: imprimir sin colores (texto plano)
//
// Programa: ./colorize
//   En terminal: muestra "ERROR" en rojo, "WARNING" en amarillo, "OK" en verde
//   Redirigido: muestra los mismos mensajes sin códigos de escape
//
// Preguntas:
// 1. ¿Por qué ls usa colores en terminal pero no en ls | cat?
// 2. ¿Qué buffering usa stdout en cada caso?
// 3. ¿grep --color=always fuerza colores incluso en pipe?
```

<details><summary>Predicción</summary>

`isatty(STDOUT_FILENO)` retorna 1 si stdout es un terminal, 0 si es archivo/pipe.

```c
if (isatty(STDOUT_FILENO)) {
    printf("\033[31mERROR\033[0m: something failed\n");
} else {
    printf("ERROR: something failed\n");
}
```

Respuestas:
1. `ls` internamente usa `isatty()`. Si stdout es terminal → colores. Si es pipe (`ls | cat`) → isatty retorna 0 → sin colores. Es exactamente el patrón que implementamos.

2. Terminal: stdout es line-buffered (flush en cada '\n'). Archivo/pipe: stdout es full-buffered (flush cuando buffer de 8 KB se llena o en exit).

3. Sí, `--color=always` ignora isatty y fuerza los códigos ANSI. Los códigos aparecen como texto basura si el receptor no los interpreta (ej: `ls --color=always | cat` muestra los códigos visibles porque cat no los procesa como un terminal haría).

</details>

### Ejercicio 9 — Implementar tee con POSIX I/O

```c
// Implementa una versión simple de tee(1):
// Lee de stdin y escribe a stdout Y a un archivo simultáneamente.
//
// Uso: ./mytee output.txt
//      echo "hello" | ./mytee saved.txt
//      cat /etc/hostname | ./mytee copy.txt
//
// Usar solo POSIX I/O (read, write, open, close).
// Buffer de 4096 bytes. Manejar short writes en ambos destinos.
//
// Extra: agregar flag -a para append (O_APPEND en vez de O_TRUNC).
//
// Verificar: echo "test" | ./mytee out.txt && cat out.txt
//            echo "append" | ./mytee -a out.txt && cat out.txt
```

<details><summary>Predicción</summary>

Flujo:
1. Abrir archivo destino con `open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644)` (o `O_APPEND` con flag `-a`)
2. Loop: `read(STDIN_FILENO, buf, 4096)` hasta retorno 0 (EOF)
3. Por cada bloque leído:
   - `write_all(STDOUT_FILENO, buf, n)` — escribir a stdout
   - `write_all(fd, buf, n)` — escribir al archivo
4. Cerrar fd

Con `echo "hello" | ./mytee saved.txt`:
- stdout muestra "hello\n"
- saved.txt contiene "hello\n"

Con `-a` (append), la segunda ejecución agrega al final en vez de sobrescribir. Después de ambas ejecuciones, saved.txt contiene "test\nappend\n".

Se necesita `write_all` para ambos destinos porque tanto stdout (si es pipe) como el archivo podrían tener short writes.

</details>

### Ejercicio 10 — Resumen: refactorizar de POSIX a stdio y viceversa

```c
// Parte A: Dado este código POSIX, reescribirlo con stdio:
//
// int fd = open("log.txt", O_WRONLY | O_CREAT | O_APPEND, 0644);
// char buf[256];
// int len = snprintf(buf, sizeof(buf), "[%ld] %s: %s\n",
//                    time(NULL), level, msg);
// write(fd, buf, len);
// close(fd);
//
// Parte B: Dado este código stdio, reescribirlo con POSIX:
//
// FILE *f = fopen("data.bin", "rb");
// struct Record rec;
// while (fread(&rec, sizeof(rec), 1, f) == 1) {
//     if (rec.active) process(&rec);
// }
// fclose(f);
//
// Preguntas:
// 1. ¿En Parte A, qué se pierde al pasar a stdio? (O_APPEND atomicidad)
// 2. ¿En Parte B, qué se gana al pasar a POSIX? (control del buffer)
// 3. ¿Cuál refactorización fue más natural?
```

<details><summary>Predicción</summary>

**Parte A (POSIX → stdio)**:
```c
FILE *f = fopen("log.txt", "a");   // "a" = append
fprintf(f, "[%ld] %s: %s\n", time(NULL), level, msg);
fclose(f);
```
Se pierde: `O_APPEND` atómico. `fopen("a")` usa `O_APPEND` internamente, pero el `fprintf` pasa por el buffer de stdio. Si otro proceso escribe al mismo archivo, con POSIX el write atómico de O_APPEND protege. Con stdio, el buffer podría acumular datos y el write real ocurre después, potencialmente mezclando datos. Para un logger multiproces, POSIX es más seguro.

**Parte B (stdio → POSIX)**:
```c
int fd = open("data.bin", O_RDONLY);
struct Record rec;
ssize_t n;
while ((n = read(fd, &rec, sizeof(rec))) == (ssize_t)sizeof(rec)) {
    if (rec.active) process(&rec);
}
close(fd);
```
Se gana: control del buffer (podrías leer N registros de golpe) y se pierde la conveniencia del conteo de elementos de fread.

La refactorización A (POSIX→stdio) fue más natural porque el código original ya construía el string con snprintf — fprintf elimina ese paso intermedio.

</details>
