# T04 — Buffering

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, `labs/buf_modes.c`,
> `labs/fflush_demo.c`, `labs/setvbuf_demo.c`, `labs/crash_demo.c`

## Erratas detectadas

Sin erratas detectadas en el material fuente.

---

## Tabla de contenidos

1. [Qué es el buffering](#1--qué-es-el-buffering)
2. [Tres modos de buffering](#2--tres-modos-de-buffering)
3. [Por qué printf sin '\\n' no imprime](#3--por-qué-printf-sin-n-no-imprime)
4. [fflush — forzar el flush](#4--fflush--forzar-el-flush)
5. [setvbuf y setbuf — configurar el buffering](#5--setvbuf-y-setbuf--configurar-el-buffering)
6. [Cuándo stdout cambia de modo](#6--cuándo-stdout-cambia-de-modo)
7. [Buffer y crash — datos perdidos](#7--buffer-y-crash--datos-perdidos)
8. [strace para observar syscalls](#8--strace-para-observar-syscalls)
9. [BUFSIZ y rendimiento](#9--bufsiz-y-rendimiento)
10. [Comparación con Rust](#10--comparación-con-rust)

---

## 1 — Qué es el buffering

`stdio.h` no escribe datos al SO inmediatamente. Los acumula en un
**buffer** en memoria del proceso y los envía en lotes. Esto reduce las
syscalls (`write`) y mejora el rendimiento:

```c
// Sin buffering — 5 syscalls:
printf("H");  // → write(1, "H", 1)
printf("e");  // → write(1, "e", 1)
printf("l");  // → write(1, "l", 1)
printf("l");  // → write(1, "l", 1)
printf("o");  // → write(1, "o", 1)

// Con buffering — 1 syscall:
printf("H");  // → buffer: "H"
printf("e");  // → buffer: "He"
printf("l");  // → buffer: "Hel"
printf("l");  // → buffer: "Hell"
printf("o");  // → buffer: "Hello"
// flush       → write(1, "Hello", 5)
```

El buffering es la razón por la que `stdio` es eficiente: agrupa muchas
escrituras pequeñas en pocas syscalls grandes. Una syscall tiene un costo
fijo significativo (cambio de contexto usuario→kernel), así que reducir su
número es clave para el rendimiento.

---

## 2 — Tres modos de buffering

### Full buffering (`_IOFBF`)

Los datos se envían cuando el buffer se **llena** (típicamente 8192 bytes)
o cuando se llama `fclose`:

```c
FILE *f = fopen("log.txt", "w");
fprintf(f, "data");    // NO se escribe al disco — va al buffer
// ... más escrituras ...
fclose(f);             // fclose flushea → ahora sí está en disco
```

**Usado por**: archivos regulares abiertos con `fopen`.

### Line buffering (`_IOLBF`)

Los datos se envían cuando se encuentra un `'\n'` o cuando el buffer se
llena:

```c
printf("Hello");       // NO se imprime todavía (sin '\n')
printf(" World\n");    // Se imprime "Hello World\n" (el '\n' flushea)
```

**Usado por**: `stdout` cuando está conectado a una **terminal**.

### Unbuffered (`_IONBF`)

Los datos se envían **inmediatamente** — cada escritura genera una syscall:

```c
fprintf(stderr, "Error");   // Se imprime al instante
```

**Usado por**: `stderr` (siempre unbuffered).

### Tabla resumen

| Stream | Destino | Buffering por defecto |
|--------|---------|----------------------|
| `stdout` (terminal) | Terminal | Line buffered |
| `stdout` (redirigido) | Archivo/pipe | Full buffered |
| `stderr` | Siempre | Unbuffered |
| `fopen` (archivo) | Archivo | Full buffered |

| Modo | Constante | Cuándo flushea | Uso típico |
|------|-----------|----------------|------------|
| Full | `_IOFBF` | Buffer lleno o `fclose` | Archivos |
| Line | `_IOLBF` | `'\n'` o buffer lleno | stdout a terminal |
| None | `_IONBF` | Cada escritura | stderr, depuración |

---

## 3 — Por qué printf sin '\n' no imprime

El problema clásico de los principiantes:

```c
#include <stdio.h>
#include <unistd.h>

int main(void) {
    printf("Loading...");
    sleep(5);
    printf(" Done!\n");
    return 0;
}
```

### Lo que el usuario ve

```
(5 segundos de espera sin nada visible)
Loading... Done!
```

### Por qué

`stdout` es line buffered en terminal. `"Loading..."` no tiene `'\n'`, así
que queda en el buffer. Después de 5 segundos, `" Done!\n"` tiene `'\n'` →
flushea todo el buffer de golpe.

### Soluciones

```c
// 1. Agregar '\n':
printf("Loading...\n");

// 2. fflush explícito:
printf("Loading...");
fflush(stdout);         // fuerza el flush inmediatamente
sleep(5);
printf(" Done!\n");

// 3. Hacer stdout unbuffered:
setbuf(stdout, NULL);   // al inicio del programa
printf("Loading...");   // se imprime inmediatamente

// 4. Usar stderr:
fprintf(stderr, "Loading...");   // siempre unbuffered
```

La solución 2 (`fflush`) es la más común en la práctica: no cambia el
comportamiento general de stdout, solo fuerza un flush en el punto exacto
donde se necesita.

---

## 4 — fflush — forzar el flush

```c
int fflush(FILE *stream);
// Envía todos los datos del buffer al SO
// Retorna 0 si éxito, EOF si error
```

### Usos principales

**Output interactivo sin '\n'** (barras de progreso, prompts):

```c
printf("Progress: 50%%");
fflush(stdout);    // aparece inmediatamente en pantalla
```

**Durabilidad de datos críticos**:

```c
fprintf(log_file, "critical event");
fflush(log_file);  // se escribe al disco ahora
// Si el programa crashea, este dato está a salvo
```

### fflush(NULL)

```c
fflush(NULL);    // flushea TODOS los streams abiertos
```

### fflush(stdin) — no portátil

```c
fflush(stdin);   // UB según el estándar C
// Algunas implementaciones (Linux glibc, MSVC) lo permiten
// para descartar input no leído, pero NO es portátil
// No usar
```

---

## 5 — setvbuf y setbuf — configurar el buffering

### setvbuf

```c
int setvbuf(FILE *stream, char *buf, int mode, size_t size);
// Configura el modo de buffering
// DEBE llamarse ANTES de cualquier lectura/escritura
// mode: _IOFBF (full), _IOLBF (line), _IONBF (unbuffered)
// buf: buffer a usar (o NULL para que stdio aloque uno)
// size: tamaño del buffer (ignorado si _IONBF)
```

### Ejemplos

```c
// Hacer stdout unbuffered:
setvbuf(stdout, NULL, _IONBF, 0);
// Ahora cada printf produce un write inmediato

// Hacer un archivo line buffered:
FILE *f = fopen("log.txt", "w");
setvbuf(f, NULL, _IOLBF, 0);
// Ahora cada '\n' flushea al disco automáticamente

// Usar un buffer grande para máximo rendimiento:
static char big_buffer[65536];
setvbuf(f, big_buffer, _IOFBF, sizeof(big_buffer));
// Acumula hasta 64 KB antes de hacer write
```

### setbuf — versión simplificada

```c
void setbuf(FILE *stream, char *buf);

setbuf(stdout, NULL);    // equivale a setvbuf(stdout, NULL, _IONBF, 0)

char buf[BUFSIZ];
setbuf(f, buf);          // equivale a setvbuf(f, buf, _IOFBF, BUFSIZ)
```

### Regla: antes de cualquier I/O

`setvbuf` debe llamarse **inmediatamente** después de `fopen` y antes de
cualquier `fprintf`, `fgets`, etc. Llamarlo después de haber hecho I/O es
undefined behavior.

---

## 6 — Cuándo stdout cambia de modo

stdout detecta automáticamente su destino y ajusta el buffering:

```bash
./program              # stdout → terminal    → line buffered
./program > output.txt # stdout → archivo     → full buffered
./program | grep foo   # stdout → pipe        → full buffered
```

### La sorpresa con pipes y redirección

```c
printf("tick\n");
sleep(1);
printf("tock\n");
```

**En terminal** (line buffered): `"tick"` aparece, 1 segundo, `"tock"`
aparece. El `'\n'` flushea inmediatamente.

**En pipe** (full buffered): 1 segundo de espera, luego `"tick\ntock\n"`
aparece todo junto. El `'\n'` **no** causa flush porque el modo es full
buffered — solo flushea cuando el buffer se llena o el programa termina.

### Solución para pipes

```c
// Al inicio del programa, forzar line buffering:
setvbuf(stdout, NULL, _IOLBF, 0);

// O fflush después de cada output importante:
printf("tick\n");
fflush(stdout);
```

---

## 7 — Buffer y crash — datos perdidos

Si el programa crashea, los datos en el buffer de stdio **se pierden**:

```c
FILE *log = fopen("crash_log.txt", "w");
fprintf(log, "step 1: initialized\n");
fprintf(log, "step 2: processing\n");
fprintf(log, "step 3: risky operation\n");
abort();    // crash — fclose nunca se ejecuta
```

### Resultado: archivo vacío

```bash
cat crash_log.txt
# (vacío — 0 bytes)
```

Las tres líneas estaban en el buffer de stdio (en memoria del proceso).
`abort()` terminó el proceso, el SO liberó la memoria, y los datos se
perdieron. `fclose` nunca se ejecutó → no hubo flush.

### Solución: line buffering

```c
FILE *log = fopen("crash_log.txt", "w");
setvbuf(log, NULL, _IOLBF, 0);    // cada '\n' flushea al disco

fprintf(log, "step 1: initialized\n");    // → write inmediato
fprintf(log, "step 2: processing\n");     // → write inmediato
fprintf(log, "step 3: risky operation\n"); // → write inmediato
abort();    // crash — pero los datos ya están en disco
```

```bash
cat crash_log.txt
# step 1: initialized
# step 2: processing
# step 3: risky operation
```

### Alternativas

```c
// fflush después de cada escritura crítica:
fprintf(log, "critical\n");
fflush(log);

// Usar stderr (siempre unbuffered):
fprintf(stderr, "Debug: x = %d\n", x);

// Usar write() directamente (sin buffer de stdio):
#include <unistd.h>
write(STDERR_FILENO, "crash!\n", 7);
// write es async-signal-safe — se puede usar en signal handlers
```

---

## 8 — strace para observar syscalls

`strace` permite ver las syscalls que hace un programa. Es la herramienta
definitiva para entender el buffering:

### Unbuffered: una syscall por escritura

```bash
strace -e trace=write ./programa_unbuffered 2>&1 | grep 'write(3'
```

```
write(3, "line 0\n", 7) = 7
write(3, "line 1\n", 7) = 7
write(3, "line 2\n", 7) = 7
...
```

10 líneas → 10 syscalls `write`.

### Full buffered: una syscall para todo

```bash
strace -e trace=write ./programa_full 2>&1 | grep 'write(3'
```

```
write(3, "line 0\nline 1\nline 2\n...", 70) = 70
```

10 líneas → 1 syscall `write`. Los datos se acumularon en el buffer y se
escribieron en un solo lote al hacer `fclose`.

### Crash sin flush: ninguna syscall

```bash
strace -e trace=write ./crash_demo 2>&1 | grep 'write(3'
# (nada)
```

Ningún `write` al fd 3 (el archivo). Los `fprintf` nunca generaron
syscalls porque todo quedó en el buffer de stdio.

---

## 9 — BUFSIZ y rendimiento

### El valor de BUFSIZ

```c
#include <stdio.h>
printf("BUFSIZ = %d\n", BUFSIZ);   // 8192 en Linux (glibc)
```

Este es el tamaño por defecto del buffer de stdio para archivos con full
buffering. Cuando el buffer se llena (8192 bytes acumulados), se hace un
`write` automático.

### Impacto en rendimiento

| Modo | 10 líneas cortas | Syscalls | Eficiencia |
|------|------------------|----------|------------|
| Unbuffered | 10 × `write(3, "line N\n", 7)` | 10 | Baja |
| Full buffered | 1 × `write(3, "line 0\n...line 9\n", 70)` | 1 | Alta |

Para archivos grandes, la diferencia es dramática: escribir 1 MB byte a
byte genera ~1 millón de syscalls con unbuffered, pero solo ~128 con full
buffering (1MB / 8KB = 128).

### Cuándo sacrificar rendimiento por seguridad

- **Archivos de log**: line buffering (`_IOLBF`) — cada línea se escribe
  al disco inmediatamente. Más seguro ante crashes.
- **Datos masivos**: full buffering (`_IOFBF`) — máximo rendimiento.
- **Depuración**: unbuffered (`_IONBF`) o `stderr` — output inmediato.

---

## 10 — Comparación con Rust

| Aspecto | C `stdio` | Rust `std::io` |
|---------|-----------|----------------|
| Buffering por defecto | Automático (line/full/none) | `BufWriter` explícito |
| Flush manual | `fflush(f)` | `f.flush()?` |
| Crash safety | Datos perdidos sin flush | `BufWriter::drop` intenta flush |
| Configuración | `setvbuf` antes de I/O | `BufWriter::with_capacity(n, f)` |
| stderr | Siempre unbuffered | `eprintln!` va directo |

### Buffering explícito en Rust

```rust
use std::io::{Write, BufWriter};
use std::fs::File;

// Sin buffer (cada write! genera una syscall):
let mut f = File::create("data.txt")?;
write!(f, "line 1\n")?;   // syscall write

// Con buffer (acumula hasta capacidad):
let mut f = BufWriter::new(File::create("data.txt")?);
write!(f, "line 1\n")?;   // va al buffer
write!(f, "line 2\n")?;   // va al buffer
f.flush()?;                // fuerza write al SO

// Buffer de tamaño personalizado:
let mut f = BufWriter::with_capacity(65536, File::create("data.txt")?);
```

En Rust, el buffering es **explícito**: si quieres buffer, envuelves el
writer en `BufWriter`. En C, el buffering está integrado en `FILE` y es
implícito.

### Drop flush en Rust

```rust
{
    let mut f = BufWriter::new(File::create("log.txt")?);
    write!(f, "data")?;
}   // f.drop() intenta flush automáticamente
    // PERO: si el flush falla en drop, el error se ignora silenciosamente
    // Por eso es recomendable llamar flush() explícitamente antes del drop
```

### print! y eprint! en Rust

```rust
print!("Loading...");              // stdout — puede estar buffered
std::io::stdout().flush().unwrap(); // equivale a fflush(stdout)

eprint!("Error: {}", msg);        // stderr — siempre inmediato
```

### Lock de stdout en Rust

```rust
use std::io::{self, Write};

// Cada println! adquiere un lock de stdout.
// Para muchas escrituras, bloquear una sola vez:
let stdout = io::stdout();
let mut handle = stdout.lock();   // un solo lock
for i in 0..1000 {
    writeln!(handle, "line {}", i)?;  // sin lock por iteración
}
// Equivalente en rendimiento al buffering de C
```

---

## Ejercicios

### Ejercicio 1 — Observar los tres modos

```c
// Crear un programa que:
// 1. Escriba 3 mensajes a stdout SIN '\n' con sleep(1) entre ellos
// 2. Escriba 3 mensajes a stderr SIN '\n' con sleep(1) entre ellos
// 3. Escriba 3 líneas a un archivo y cierre
//
// Ejecutar en terminal y observar:
//   - ¿stderr aparece inmediatamente o espera?
//   - ¿stdout aparece uno a uno o todos juntos?
//   - ¿Cuándo aparece el contenido del archivo?
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex01.c -o ex01
```

<details><summary>Predicción</summary>

```
(segundo 0) [stderr] msg 1
(segundo 1) [stderr] msg 2
(segundo 2) [stderr] msg 3[stdout] msg 1[stdout] msg 2[stdout] msg 3
```

stderr es unbuffered: cada `fprintf(stderr, ...)` aparece al instante.
stdout es line buffered en terminal: los 3 mensajes sin `'\n'` se
acumulan en el buffer. Al terminar el programa (o al llegar un `'\n'`),
se flushean todos juntos. El archivo se escribe al disco en `fclose` —
no antes.

</details>

---

### Ejercicio 2 — fflush para output interactivo

```c
// Crear un programa que muestre un contador:
//   "Processing: 1/10..."  (esperar 500ms)
//   "\rProcessing: 2/10..."  (sobreescribir con \r)
//   ...
//   "\rProcessing: 10/10... Done!\n"
//
// Sin fflush, nada se ve hasta el final.
// Con fflush después de cada printf, el progreso es visible.
//
// Probar ambas versiones y comparar.
// Usar usleep(500000) para el delay de 500ms.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex02.c -o ex02
```

<details><summary>Predicción</summary>

Sin fflush: nada visible durante 5 segundos, luego "Processing: 10/10... Done!"
aparece de golpe.

Con fflush: el contador se actualiza visualmente cada 500ms, mostrando
1/10, 2/10, ..., 10/10 en la misma línea (gracias a `\r` que retorna el
cursor al inicio sin avanzar de línea). El `fflush(stdout)` después de
cada `printf` fuerza la visualización inmediata.

</details>

---

### Ejercicio 3 — Crash: full buffered vs line buffered

```c
// Crear un programa que:
// 1. Abra "crash_test.txt" en modo "w"
// 2. Escriba 5 líneas con fprintf
// 3. Llame abort() antes de fclose
//
// Ejecutar y verificar: ¿cuántas líneas tiene crash_test.txt?
//
// Repetir con setvbuf(f, NULL, _IOLBF, 0) antes de escribir.
// ¿Cuántas líneas ahora?
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex03.c -o ex03
```

<details><summary>Predicción</summary>

```
Sin setvbuf (full buffered):
  wc -l crash_test.txt → 0 (archivo vacío)
  Las 5 líneas estaban en el buffer, abort() las perdió.

Con setvbuf _IOLBF (line buffered):
  wc -l crash_test.txt → 5 (todas las líneas)
  Cada '\n' forzó un write al disco, las 5 líneas sobrevivieron.
```

Full buffered acumula hasta 8192 bytes — 5 líneas cortas no llenan el
buffer, así que `write` nunca se ejecutó antes del crash. Line buffered
flushea en cada `'\n'`, así que cada línea se escribió antes de la
siguiente.

</details>

---

### Ejercicio 4 — setvbuf: cambiar el modo de stdout

```c
// Crear un programa que imprima "tick" sin '\n', seguido de sleep(1),
// 5 veces, y luego '\n'.
//
// Ejecutar de 3 formas:
// a) Sin setvbuf (line buffered en terminal)
// b) Con setvbuf(stdout, NULL, _IONBF, 0) al inicio
// c) Con setvbuf(stdout, NULL, _IOFBF, 0) al inicio
//
// Predecir qué se ve en cada caso.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex04.c -o ex04
```

<details><summary>Predicción</summary>

```
a) Line buffered (default):
   (5 seg de espera) tick tick tick tick tick
   Todo aparece junto al llegar el '\n'.

b) Unbuffered (_IONBF):
   tick (1s) tick (1s) tick (1s) tick (1s) tick
   Cada printf aparece al instante — no necesita '\n'.

c) Full buffered (_IOFBF):
   (5 seg de espera) tick tick tick tick tick
   Igual que (a) pero por razón diferente: en full, ni '\n'
   fuerza flush. Se flushea al terminar el programa.
```

La diferencia entre (a) y (c): en (a), un `'\n'` forzaría flush; en
(c) no. Como no hay `'\n'` intermedio, el comportamiento observable es
el mismo para estas 5 escrituras. La diferencia se vería si hubiera
`'\n'` intermedios.

</details>

---

### Ejercicio 5 — strace: contar syscalls

```c
// Crear un programa que escriba 100 líneas a un archivo.
// Compilar DOS versiones:
//   a) Con full buffering (default)
//   b) Con setvbuf _IONBF (unbuffered)
//
// Ejecutar cada una con strace:
//   strace -e trace=write ./version_a 2>&1 | grep 'write(3' | wc -l
//   strace -e trace=write ./version_b 2>&1 | grep 'write(3' | wc -l
//
// Predecir cuántas syscalls write hace cada versión.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex05.c -o ex05
```

<details><summary>Predicción</summary>

```
Full buffered:  ~1-2 syscalls write (100 líneas cortas < 8192 bytes)
Unbuffered:     100 syscalls write (una por cada fprintf)
```

Con full buffering, 100 líneas de ~10 bytes = ~1000 bytes total. El
buffer (BUFSIZ = 8192) cabe todo sin llenarse. El único write ocurre
en `fclose` al hacer flush final. Con unbuffered, cada `fprintf` genera
su propio write — 100 llamadas al kernel.

</details>

---

### Ejercicio 6 — fflush(NULL) para flush global

```c
// Abrir 3 archivos con fopen.
// Escribir datos a cada uno SIN cerrarlos.
// Llamar fflush(NULL) — flushea todos los streams abiertos.
// Verificar con cat que los 3 archivos tienen los datos
// ANTES de llamar fclose.
//
// Luego cerrar los 3 archivos.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex06.c -o ex06
```

<details><summary>Predicción</summary>

```
Before fflush(NULL):
  file1.txt: (puede estar vacío — datos en buffer)
  file2.txt: (puede estar vacío)
  file3.txt: (puede estar vacío)

After fflush(NULL):
  file1.txt: "data for file 1"
  file2.txt: "data for file 2"
  file3.txt: "data for file 3"
```

`fflush(NULL)` flushea **todos** los streams abiertos del proceso. Esto
es útil antes de hacer `fork()` o ante situaciones donde se necesita
garantizar que todo está en disco sin cerrar individualmente cada archivo.

</details>

---

### Ejercicio 7 — Redirección cambia el buffering

```c
// Crear un programa que:
//   printf("line 1\n"); sleep(1);
//   printf("line 2\n"); sleep(1);
//   printf("line 3\n");
//
// Ejecutar de 3 formas:
//   ./ex07                     (terminal — line buffered)
//   ./ex07 | cat               (pipe — full buffered)
//   ./ex07 > out.txt && cat out.txt  (archivo — full buffered)
//
// En terminal: las líneas aparecen una a una.
// En pipe/archivo: ¿aparecen una a una o todas juntas?
//
// Agregar setvbuf(stdout, NULL, _IOLBF, 0) y repetir.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex07.c -o ex07
```

<details><summary>Predicción</summary>

```
Terminal: line 1 (1s) line 2 (1s) line 3 — una a una.
Pipe:     (2s) line 1\nline 2\nline 3 — todas juntas al final.
Archivo:  (2s) line 1\nline 2\nline 3 — todas juntas al final.

Con setvbuf _IOLBF:
Pipe:     line 1 (1s) line 2 (1s) line 3 — una a una (forzado).
Archivo:  line 1 (1s) line 2 (1s) line 3 — una a una (forzado).
```

En pipe y archivo, stdout cambia a full buffered automáticamente. Los
`'\n'` ya no fuerzan flush. Con `setvbuf _IOLBF` se fuerza line buffering
independientemente del destino — cada `'\n'` flushea.

</details>

---

### Ejercicio 8 — Log seguro con line buffering

```c
// Implementar un mini logger:
//   FILE *log_open(const char *path)   — abre con line buffering
//   void log_write(FILE *f, const char *level, const char *msg)
//   void log_close(FILE *f)
//
// log_write escribe: "[TIMESTAMP] [LEVEL] msg\n"
// El line buffering garantiza que cada línea llega a disco.
//
// Probar: abrir log, escribir 3 entradas, simular crash con abort().
// Verificar que las 3 entradas están en el archivo.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex08.c -o ex08
```

<details><summary>Predicción</summary>

```
$ ./ex08
Aborted (core dumped)

$ cat app.log
[1711900000] [INFO]  Application started
[1711900001] [WARN]  High memory usage
[1711900002] [ERROR] About to crash
```

Las 3 entradas sobreviven al `abort()` porque `log_open` configuró el
archivo con `setvbuf(f, NULL, _IOLBF, 0)`. Cada `fprintf` con `'\n'`
ejecutó un `write` al disco antes de la siguiente operación.

</details>

---

### Ejercicio 9 — Buffer personalizado con setvbuf

```c
// Crear un programa que:
// 1. Abra un archivo
// 2. Configure un buffer de 32 bytes con setvbuf(_IOFBF, 32)
// 3. Escriba 10 líneas de ~10 bytes cada una
// 4. Cierre el archivo
//
// Ejecutar con strace y contar cuántas syscalls write se hacen.
// Predecir: con buffer de 32 bytes y líneas de ~10 bytes,
// ¿cuántos writes se necesitan para 100 bytes de datos?
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex09.c -o ex09
```

<details><summary>Predicción</summary>

```
Buffer: 32 bytes
Datos: 10 líneas × ~10 bytes = ~100 bytes
Writes: ~100/32 ≈ 3-4 syscalls write

strace output:
  write(3, "line 0\nline 1\nline 2\n", 30) = 30
  write(3, "line 3\nline 4\nline 5\n", 30) = 30
  write(3, "line 6\nline 7\nline 8\n", 30) = 30
  write(3, "line 9\n", 7) = 7   (flush final en fclose)
```

El buffer de 32 bytes se llena después de ~3 líneas de 10 bytes. Al
llenarse, se hace `write` automáticamente y el buffer se vacía. El
`fclose` flushea el residuo. Con BUFSIZ (8192), todo cabría en un solo
write.

</details>

---

### Ejercicio 10 — write() directo vs fprintf buffered

```c
// Comparar:
//   a) fprintf(stderr, "msg\n")  — pasa por stdio (unbuffered)
//   b) write(STDERR_FILENO, "msg\n", 4)  — syscall directa
//
// Ambas son "inmediatas", pero write() tiene una ventaja:
// es async-signal-safe — se puede usar en signal handlers.
//
// Implementar un signal handler para SIGSEGV que:
//   - Imprima "Segmentation fault caught!\n" con write()
//   - NO use printf (no es async-signal-safe)
//
// Provocar un SIGSEGV con: *(int*)NULL = 42;
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
```

<details><summary>Predicción</summary>

```
Normal output (fprintf): Starting program...
Signal handler (write):  Segmentation fault caught!
```

`write(STDERR_FILENO, msg, len)` va directamente al kernel sin pasar
por el buffer de stdio. Es seguro en signal handlers porque no modifica
estado global (como el buffer de `FILE`). `fprintf`/`printf` NO son
async-signal-safe — usarlos en un signal handler es UB porque podrían
estar en medio de una operación de buffer cuando llega la señal.

</details>
