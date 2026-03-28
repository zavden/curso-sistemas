# T01 — FILE*, fopen, fclose

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, `labs/open_close.c`,
> `labs/open_modes.c`, `labs/std_streams.c`, `labs/fd_limit.c`

## Erratas detectadas

Sin erratas detectadas en el material fuente.

---

## Tabla de contenidos

1. [El tipo FILE](#1--el-tipo-file)
2. [fopen — abrir archivos](#2--fopen--abrir-archivos)
3. [Modos de apertura](#3--modos-de-apertura)
4. [fclose — cerrar archivos](#4--fclose--cerrar-archivos)
5. [stdin, stdout, stderr](#5--stdin-stdout-stderr)
6. [Verificación de errores](#6--verificación-de-errores)
7. [Patrón seguro de manejo de archivos](#7--patrón-seguro-de-manejo-de-archivos)
8. [tmpfile — archivos temporales](#8--tmpfile--archivos-temporales)
9. [Errores comunes](#9--errores-comunes)
10. [Comparación con Rust](#10--comparación-con-rust)

---

## 1 — El tipo FILE

`FILE` es un **tipo opaco** definido en `<stdio.h>` que representa un
**stream** (flujo de datos). No se puede crear directamente — se obtiene con
`fopen` y se usa siempre como puntero:

```c
#include <stdio.h>

FILE *f = fopen("data.txt", "r");
// f apunta a una estructura interna del sistema
```

### Qué contiene FILE internamente

Aunque es opaco, internamente un `FILE` contiene:

- **File descriptor**: el número entero que el kernel usa para identificar el
  archivo abierto (0, 1, 2, 3, ...)
- **Buffer**: memoria donde se acumulan datos antes de escribirlos al disco
  (o después de leerlos)
- **Posición**: el offset actual dentro del archivo (dónde se leerá/escribirá
  el siguiente byte)
- **Flags de estado**: indicadores de error (`ferror`) y fin de archivo (`feof`)
- **Modo**: lectura, escritura, o ambos

`FILE` es el ejemplo clásico de opaque type en C — exactamente el patrón que
vimos con `typedef struct Stack Stack` en el tópico anterior.

---

## 2 — fopen — abrir archivos

```c
FILE *fopen(const char *path, const char *mode);
// Retorna FILE* si éxito
// Retorna NULL si error (y establece errno)
```

### Uso básico

```c
FILE *f = fopen("data.txt", "r");
if (f == NULL) {
    perror("fopen");    // imprime: fopen: No such file or directory
    return -1;
}
// ... usar f ...
fclose(f);
```

**Regla absoluta**: verificar **siempre** que `fopen` no retornó `NULL` antes
de usar el `FILE*`. Usar un puntero `NULL` causa segfault.

---

## 3 — Modos de apertura

### Modos de texto

| Modo | Lectura | Escritura | Crea | Trunca | Posición |
|------|---------|-----------|------|--------|----------|
| `"r"` | Sí | No | No | No | Inicio |
| `"w"` | No | Sí | Sí | **Sí** | Inicio |
| `"a"` | No | Sí | Sí | No | Final |
| `"r+"` | Sí | Sí | No | No | Inicio |
| `"w+"` | Sí | Sí | Sí | **Sí** | Inicio |
| `"a+"` | Sí | Sí | Sí | No | Final (write) |

### Los tres modos básicos explicados

**`"r"` (read)**: el archivo **debe existir**. Si no existe, `fopen` retorna
`NULL`. Solo lectura.

**`"w"` (write)**: crea el archivo si no existe. Si **ya existe, BORRA todo
su contenido** (trunca a 0 bytes). Este es el peligro más común — abrir un
archivo con `"w"` destruye su contenido inmediatamente.

**`"a"` (append)**: crea si no existe. Si ya existe, **no borra nada** — todas
las escrituras van al final del archivo. Es el modo seguro para agregar datos.

### Diferencia entre r+ y w+

Ambos permiten lectura **y** escritura, pero:

- `"r+"`: no trunca, no crea. El archivo debe existir. La posición empieza en
  el byte 0 — escribir sobrescribe los bytes existentes sin borrar el resto.
- `"w+"`: **trunca** primero (borra todo), luego permite leer y escribir.
  Equivale a `"w"` con la capacidad adicional de leer.

```c
// r+ sobrescribe in-place:
FILE *f = fopen("data.txt", "r+");
fprintf(f, "OVERWRITTEN");   // sobrescribe los primeros 11 bytes
// El resto del archivo queda intacto

// w+ borra todo primero:
FILE *f = fopen("data.txt", "w+");
// El archivo ahora está vacío, aunque existía con contenido
```

### Modos binarios

Agregar `'b'` al modo: `"rb"`, `"wb"`, `"ab"`, `"r+b"`, `"w+b"`, `"a+b"`.

```c
FILE *f = fopen("image.png", "rb");    // lectura binaria
FILE *g = fopen("output.bin", "wb");   // escritura binaria
```

En **Linux**, `'b'` no tiene efecto — no hay distinción texto/binario.
En **Windows**, `'b'` evita la conversión automática `\n` ↔ `\r\n`.
Para **portabilidad**, siempre usar `'b'` para archivos binarios.

### Modo exclusivo (C11)

```c
FILE *f = fopen("output.txt", "wx");   // write exclusivo
if (f == NULL && errno == EEXIST) {
    printf("File already exists — not overwriting\n");
}
```

`"wx"` falla si el archivo ya existe. Útil para evitar sobrescrituras
accidentales. Equivale a `O_CREAT | O_EXCL` en POSIX.

---

## 4 — fclose — cerrar archivos

```c
int fclose(FILE *stream);
// Retorna 0 si éxito, EOF si error
```

### Qué hace fclose internamente

1. **Flushea el buffer**: escribe al disco todos los datos pendientes
2. **Cierra el file descriptor**: libera el FD para reutilización
3. **Libera la memoria**: free del struct `FILE` interno

### Por qué es obligatorio cerrar

```c
// Tres consecuencias de no cerrar:

// 1. Datos perdidos — el buffer puede tener datos no escritos al disco
// 2. File descriptor leak — el SO limita los FDs por proceso (ulimit -n)
// 3. Memory leak — el struct FILE no se libera
```

### Verificar el retorno de fclose

```c
if (fclose(f) != 0) {
    perror("fclose");
    // Un error en fclose puede indicar que los datos
    // NO se escribieron al disco (ej: disco lleno)
}
```

Ignorar el retorno de `fclose` puede ocultar pérdida de datos en escritura.

### fclose(NULL) es UB

```c
// NUNCA pasar NULL a fclose:
fclose(NULL);    // Undefined Behavior

// Verificar antes:
if (f) fclose(f);
```

---

## 5 — stdin, stdout, stderr

Los tres **streams estándar** están abiertos automáticamente al iniciar
cualquier programa C:

| Stream | FD | Propósito | Buffering | Redirección |
|--------|-----|-----------|-----------|-------------|
| `stdin` | 0 | Entrada (teclado) | Line-buffered | `< file` |
| `stdout` | 1 | Salida normal | Line-buffered (terminal) / Full-buffered (pipe/file) | `> file` |
| `stderr` | 2 | Salida de error | Sin buffer | `2> file` |

### stdout vs stderr

```c
fprintf(stdout, "Normal output\n");    // va a stdout
fprintf(stderr, "Error message\n");    // va a stderr
printf("Also stdout\n");              // printf = fprintf(stdout, ...)
```

Son streams **independientes**. Al redirigir, se separan:

```bash
./prog > output.txt      # solo stdout va al archivo, stderr a terminal
./prog 2> errors.txt     # solo stderr va al archivo, stdout a terminal
./prog > out 2> err      # cada uno a su archivo
./prog > all.txt 2>&1    # ambos al mismo archivo
```

### Por qué stderr no tiene buffer

`stderr` se imprime **inmediatamente** (unbuffered), sin esperar a que se
llene un buffer. Esto garantiza que los mensajes de error se vean aunque el
programa crashee — si `stdout` tiene datos en buffer cuando ocurre un
segfault, esos datos se pierden; los de `stderr` ya se imprimieron.

### No cerrar los streams estándar

```c
fclose(stdout);    // legal pero causa problemas
// Cualquier printf posterior falla silenciosamente
```

---

## 6 — Verificación de errores

### errno, perror, strerror

Cuando `fopen` falla, establece `errno` con el código de error:

```c
#include <errno.h>
#include <string.h>

FILE *f = fopen(path, "r");
if (!f) {
    // Tres formas de reportar el error:

    perror(path);
    // Imprime: path: No such file or directory

    fprintf(stderr, "Error %d: %s\n", errno, strerror(errno));
    // Imprime: Error 2: No such file or directory

    // Comparar con constantes:
    if (errno == ENOENT) printf("File does not exist\n");
}
```

### Códigos de error comunes de fopen

| errno | Constante | Significado |
|-------|-----------|-------------|
| 2 | `ENOENT` | Archivo no existe |
| 13 | `EACCES` | Sin permisos |
| 24 | `EMFILE` | Demasiados archivos abiertos por el proceso |
| 17 | `EEXIST` | Ya existe (con modo `"wx"`) |
| 21 | `EISDIR` | La ruta es un directorio |
| 28 | `ENOSPC` | Disco lleno |

### feof y ferror — después de leer

Cuando un loop de lectura termina, hay que distinguir **por qué**:

```c
while (fgets(buf, sizeof(buf), f)) {
    // procesar línea...
}

// ¿Terminó por fin de archivo o por error?
if (feof(f)) {
    // Fin del archivo — terminación normal
} else if (ferror(f)) {
    // Error de I/O — algo falló
    perror("read error");
}
```

### clearerr — resetear indicadores

```c
clearerr(f);
// Limpia los flags de error y EOF del stream
// Permite seguir leyendo/escribiendo después de un error
```

---

## 7 — Patrón seguro de manejo de archivos

### Patrón con goto cleanup

```c
int process_file(const char *path) {
    int result = -1;

    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }

    char buf[256];
    while (fgets(buf, sizeof(buf), f)) {
        // procesar línea...
        if (/* error de procesamiento */) {
            goto cleanup;
        }
    }

    if (ferror(f)) {
        perror("read error");
        goto cleanup;
    }

    result = 0;    // éxito

cleanup:
    fclose(f);
    return result;
}
```

Este patrón garantiza que `fclose` se ejecuta siempre, incluso si hay
errores intermedios. El `goto cleanup` es idiomático en C para manejo de
recursos — es el equivalente a RAII/defer/try-finally de otros lenguajes.

### Reglas del patrón

1. Verificar `fopen` retorno inmediatamente
2. Un solo punto de `fclose` al final (label `cleanup`)
3. Todo error salta a `cleanup` con `goto`
4. La variable `result` indica éxito/error

---

## 8 — tmpfile — archivos temporales

```c
FILE *tmp = tmpfile();
if (!tmp) { perror("tmpfile"); return -1; }

fprintf(tmp, "data temporal\n");
rewind(tmp);    // volver al inicio para leer

char buf[100];
fgets(buf, sizeof(buf), tmp);
printf("Leído: %s", buf);

fclose(tmp);    // el archivo se elimina automáticamente
```

### Características de tmpfile

- Crea un archivo temporal **sin nombre visible** en el sistema de archivos
- Se elimina automáticamente al llamar `fclose` (o al terminar el programa)
- Abierto en modo `"w+b"` (lectura+escritura binaria)
- No requiere inventar nombres únicos ni limpiar manualmente
- Más seguro que crear archivos temporales manualmente (sin race conditions
  de nombres)

---

## 9 — Errores comunes

### 1. No verificar el retorno de fopen

```c
FILE *f = fopen("noexiste.txt", "r");
fgets(buf, sizeof(buf), f);    // SEGFAULT si f es NULL
```

### 2. Usar el modo incorrecto

```c
FILE *f = fopen("data.txt", "r");
fprintf(f, "hello");    // no escribe — modo solo lectura
// No da error inmediato — los datos se descartan silenciosamente
```

### 3. "w" destruye contenido existente

```c
FILE *f = fopen("important.txt", "w");
// ¡El contenido anterior se BORRÓ al abrir!
// Usar "a" para agregar, o "r+" para modificar sin truncar
```

### 4. Olvidar fclose (file descriptor leak)

```c
void process(void) {
    FILE *f = fopen("data.txt", "r");
    // ... procesar ...
    return;    // LEAK — f no se cerró, FD no se liberó
}
```

Con el límite de FDs por proceso (típicamente 1024, consultable con
`ulimit -n`), olvidar `fclose` en un loop eventualmente causa `EMFILE`:

```c
// Cada fopen consume un FD. Sin fclose, se agotan:
for (int i = 0; i < 2000; i++) {
    FILE *f = fopen(filename, "w");
    // Sin fclose → EMFILE después de ~1021 archivos
    // (1024 - 3 para stdin/stdout/stderr)
}
```

### 5. Usar FILE* después de fclose

```c
fclose(f);
fgets(buf, sizeof(buf), f);    // UB — f ya fue cerrado y liberado
```

Después de `fclose`, el puntero `f` apunta a memoria liberada. Cualquier
uso es undefined behavior. Buena práctica: `f = NULL` después de `fclose`.

---

## 10 — Comparación con Rust

| Aspecto | C `FILE*` | Rust `std::fs::File` |
|---------|-----------|---------------------|
| Apertura | `fopen(path, "r")` → `NULL` si error | `File::open(path)` → `Result<File, Error>` |
| Cierre | `fclose(f)` — manual, olvidable | Automático al salir de scope (`Drop`) |
| Errores | `errno` global, fácil de ignorar | `Result<>` obliga a manejar errores |
| Buffer | Integrado en `FILE` | Separado: `BufReader` / `BufWriter` |
| Modos | String: `"r"`, `"w"`, `"a"` | Métodos: `.read(true)`, `.write(true)`, `.create(true)` |
| Seguridad | UB si NULL, use-after-free posible | Compilador previene ambos |

### Ejemplo comparativo

```c
// C — muchas formas de equivocarse:
FILE *f = fopen("data.txt", "r");
if (!f) { perror("fopen"); return -1; }   // fácil olvidar
char buf[256];
while (fgets(buf, sizeof(buf), f)) {
    // procesar
}
fclose(f);    // fácil olvidar
```

```rust
// Rust — el compilador te obliga a manejar errores:
use std::fs::File;
use std::io::{BufRead, BufReader};

let f = File::open("data.txt")?;  // ? propaga error automáticamente
let reader = BufReader::new(f);
for line in reader.lines() {
    let line = line?;  // cada lectura también puede fallar
    // procesar
}
// f se cierra automáticamente aquí (Drop trait)
// No hay fclose que olvidar
```

### RAII en Rust vs goto cleanup en C

En C, el patrón `goto cleanup` garantiza la liberación de recursos
manualmente. En Rust, el trait `Drop` lo hace automáticamente cuando la
variable sale de scope:

```rust
{
    let f = File::open("data.txt")?;
    // usar f...
}   // f.drop() se llama automáticamente aquí
    // Equivale a fclose(f) pero imposible de olvidar
```

### OpenOptions en Rust (equivalente a modos de fopen)

```rust
use std::fs::OpenOptions;

// Equivalente a "w" (crear + truncar):
let f = OpenOptions::new().write(true).create(true).truncate(true).open("file.txt")?;

// Equivalente a "a" (append):
let f = OpenOptions::new().append(true).create(true).open("file.txt")?;

// Equivalente a "r+" (read + write, debe existir):
let f = OpenOptions::new().read(true).write(true).open("file.txt")?;

// Equivalente a "wx" (crear exclusivo):
let f = OpenOptions::new().write(true).create_new(true).open("file.txt")?;
```

Los métodos builder de `OpenOptions` son más explícitos que los strings de
modo de `fopen` — cada flag es un booleano independiente, sin necesidad de
memorizar combinaciones de letras.

---

## Ejercicios

### Ejercicio 1 — fopen y fclose básico

```c
// Crear un programa que:
// 1. Abra "greeting.txt" en modo "w"
// 2. Escriba "Hello, World!\n" con fprintf
// 3. Cierre el archivo verificando el retorno de fclose
// 4. Reabra "greeting.txt" en modo "r"
// 5. Lea el contenido con fgets e imprima en pantalla
// 6. Cierre
//
// Verificar que el archivo se creó con: cat greeting.txt
// Eliminar al final: rm greeting.txt
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex01.c -o ex01
```

<details><summary>Predicción</summary>

```
Written to greeting.txt
Closed successfully
Read back: Hello, World!
```

`fopen("greeting.txt", "w")` crea el archivo (o lo trunca si existe).
`fprintf` escribe en el buffer del `FILE`. `fclose` flushea el buffer al
disco, cierra el FD y libera la memoria. La reapertura con `"r"` lee lo
que se escribió. `fgets` incluye el `\n` en el buffer pero `%s` lo
imprime.

</details>

---

### Ejercicio 2 — Verificación de errores con perror

```c
// Crear un programa que intente abrir 3 archivos:
//   1. "exists.txt" en modo "r" (crear primero con touch o modo "w")
//   2. "noexiste.txt" en modo "r" (no debe existir)
//   3. "/root/forbidden.txt" en modo "w" (sin permisos)
//
// Para cada intento, imprimir:
//   - Si tuvo éxito: "OK: opened <path>"
//   - Si falló: usar perror(<path>) y también imprimir errno y strerror(errno)
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex02.c -o ex02
```

<details><summary>Predicción</summary>

```
OK: opened exists.txt
noexiste.txt: No such file or directory (errno=2)
/root/forbidden.txt: Permission denied (errno=13)
```

Caso 1: éxito — el archivo existe y es legible. Caso 2: `ENOENT` (errno 2)
— `"r"` requiere que el archivo exista. Caso 3: `EACCES` (errno 13) — un
usuario normal no tiene permisos de escritura en `/root/`. `perror` usa
`errno` internamente para obtener el mensaje.

</details>

---

### Ejercicio 3 — Modos w, a, r+ en secuencia

```c
// Demostrar la diferencia entre los tres modos con un solo archivo:
//
// 1. Abrir "modes.txt" con "w", escribir "AAA\n", cerrar
// 2. Abrir con "a", escribir "BBB\n", cerrar
// 3. Abrir con "r+", escribir "XX", cerrar
// 4. Abrir con "w", escribir "CCC\n", cerrar
//
// Después de CADA paso, abrir con "r", leer e imprimir todo el contenido.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex03.c -o ex03
```

<details><summary>Predicción</summary>

```
After step 1 (w):
  AAA

After step 2 (a):
  AAA
  BBB

After step 3 (r+):
  XXA
  BBB

After step 4 (w):
  CCC
```

Paso 1: `"w"` crea y escribe. Paso 2: `"a"` agrega sin borrar. Paso 3:
`"r+"` sobrescribe los primeros 2 bytes (`AA` → `XX`) sin truncar — el
tercer byte `A` y la segunda línea quedan intactos. Paso 4: `"w"` trunca
todo — las líneas anteriores desaparecen, solo queda `CCC`.

</details>

---

### Ejercicio 4 — stdout vs stderr con redirección

```c
// Crear un programa que escriba intercaladamente a stdout y stderr:
//   fprintf(stdout, "OUT 1\n");
//   fprintf(stderr, "ERR 1\n");
//   printf("OUT 2\n");
//   fprintf(stderr, "ERR 2\n");
//   fprintf(stdout, "OUT 3\n");
//
// Ejecutar de 4 formas:
//   ./ex04                    (todo a terminal)
//   ./ex04 > out.txt          (solo stdout al archivo)
//   ./ex04 2> err.txt         (solo stderr al archivo)
//   ./ex04 > out.txt 2>&1     (ambos al mismo archivo)
//
// Verificar con cat qué contiene cada archivo.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex04.c -o ex04
```

<details><summary>Predicción</summary>

Sin redirección: todo mezclado en terminal (OUT y ERR intercalados).

`> out.txt`: terminal muestra `ERR 1` y `ERR 2`; `out.txt` contiene
`OUT 1`, `OUT 2`, `OUT 3`.

`2> err.txt`: terminal muestra `OUT 1`, `OUT 2`, `OUT 3`;
`err.txt` contiene `ERR 1`, `ERR 2`.

`> out.txt 2>&1`: terminal vacía; `out.txt` contiene las 5 líneas
(el orden puede variar porque stderr es unbuffered y stdout es
line-buffered — stderr puede aparecer antes de stdout en el archivo).

</details>

---

### Ejercicio 5 — File descriptor leak con EMFILE

```c
// Crear un programa que:
// 1. Reduzca el límite de FDs a 20 con setrlimit
// 2. Abra archivos en un loop SIN cerrarlos
// 3. Cuando fopen falle, imprima:
//    - El número de archivos abiertos exitosamente
//    - errno y strerror(errno)
// 4. Cierre todos los archivos y elimine los temporales
//
// Predecir: con límite 20 y 3 FDs ya usados (stdin/stdout/stderr),
// ¿cuántos archivos nuevos se podrán abrir?
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex05.c -o ex05
```

<details><summary>Predicción</summary>

```
Limit set to 20 FDs
Opened 17 files before failure
Error: Too many open files (errno=24, EMFILE)
```

Con límite 20: FDs 0-19 disponibles. FDs 0,1,2 ya usados por
stdin/stdout/stderr. Quedan FDs 3-19 = 17 archivos nuevos. El intento
#18 fallaría con `EMFILE` (errno 24). Después del cleanup, todos los
FDs se liberan.

</details>

---

### Ejercicio 6 — Modo exclusivo "wx" (C11)

```c
// Crear un programa que:
// 1. Intente abrir "unique.txt" con modo "wx"
// 2. Si tiene éxito: escribir "Created!\n" y cerrar
// 3. Intentar abrir de nuevo con "wx"
// 4. Verificar que falla con EEXIST
//
// Esto demuestra cómo "wx" previene sobrescrituras accidentales.
// Comparar con "w" que silenciosamente trunca.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex06.c -o ex06
```

<details><summary>Predicción</summary>

```
First open with "wx": OK — created unique.txt
Second open with "wx": FAILED — File exists (errno=17, EEXIST)
Open with "w" (same file): OK — but content was truncated!
```

El primer `"wx"` crea el archivo exitosamente. El segundo `"wx"` falla
porque el archivo ya existe — `EEXIST` (errno 17). Si se abriera con
`"w"` en vez de `"wx"`, abriría exitosamente pero borraría el contenido
existente sin avisar. `"wx"` es la forma segura de crear archivos nuevos.

</details>

---

### Ejercicio 7 — Patrón goto cleanup

```c
// Implementar:
//   int count_lines(const char *path)
// que:
// 1. Abra el archivo con "r"
// 2. Cuente las líneas (con fgets en un loop)
// 3. Después del loop, verifique ferror para distinguir error de EOF
// 4. Cierre con fclose en un label cleanup (patrón goto)
// 5. Retorne el conteo si éxito, -1 si error
//
// Probar con un archivo existente y uno inexistente.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex07.c -o ex07
```

<details><summary>Predicción</summary>

```
count_lines("ex07.c") = N   (depende del tamaño del archivo)
count_lines("noexiste.txt") = -1
  fopen: No such file or directory
```

La función usa un solo punto de `fclose` (label `cleanup`). Si `fgets`
retorna `NULL` por `ferror`, el resultado se deja como -1 y se salta
a cleanup. Si es por `feof`, es terminación normal y result se asigna
el conteo. El `goto cleanup` garantiza que `fclose` se ejecuta siempre.

</details>

---

### Ejercicio 8 — tmpfile

```c
// Crear un programa que:
// 1. Cree un archivo temporal con tmpfile()
// 2. Escriba 10 líneas con fprintf: "Line N: temporary data\n"
// 3. Use rewind() para volver al inicio
// 4. Lea y muestre todo el contenido con fgets
// 5. Cierre (el archivo se elimina automáticamente)
//
// Verificar que no queda ningún archivo residual después de ejecutar.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex08.c -o ex08
```

<details><summary>Predicción</summary>

```
Writing 10 lines to tmpfile...
Reading back:
  Line 0: temporary data
  Line 1: temporary data
  ...
  Line 9: temporary data
Closed — tmpfile removed automatically
```

`tmpfile()` crea un archivo sin nombre visible. `rewind(tmp)` mueve la
posición al byte 0 para poder leer lo que se escribió. Después de
`fclose`, el archivo se elimina automáticamente — no queda en disco.
`ls` no mostrará archivos nuevos.

</details>

---

### Ejercicio 9 — Contar líneas y caracteres

```c
// Implementar un mini wc (word count):
//   void file_stats(const char *path)
// que imprima:
//   - Número de líneas
//   - Número de caracteres (bytes)
//   - Número de palabras (separadas por espacios/tabs/newlines)
//
// Usar fgetc para leer carácter por carácter.
// Manejar errores de apertura con perror.
// Probar con el propio archivo fuente: ./ex09 ex09.c
// Comparar resultado con: wc ex09.c
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex09.c -o ex09
```

<details><summary>Predicción</summary>

```
ex09.c: lines=N chars=M words=W
```

El resultado debe coincidir con la salida de `wc ex09.c` (que muestra
lines, words, chars en ese orden). `fgetc` retorna `int` (no `char`)
porque necesita representar tanto los 256 valores de `unsigned char`
como el valor especial `EOF` (-1). Se cuenta una "palabra" cada vez que
se transita de un carácter espacio/tab/newline a un carácter no-espacio.

</details>

---

### Ejercicio 10 — Copiar archivo con verificación completa

```c
// Implementar:
//   int copy_file(const char *src, const char *dst)
// que:
// 1. Abra src con "r" — si falla, retorne -1 con perror
// 2. Abra dst con "wx" — si falla (ya existe), pregunte al usuario
//    con fprintf(stderr, "Overwrite? [y/n] ") y fgets desde stdin
// 3. Copie línea por línea con fgets/fputs
// 4. Verifique ferror en ambos streams
// 5. Cierre ambos con fclose, verificando retornos
// 6. Retorne 0 si éxito, -1 si error
//
// Probar: crear un archivo fuente, copiarlo, verificar con diff.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
```

<details><summary>Predicción</summary>

```
$ ./ex10 source.txt copy.txt
Copied source.txt → copy.txt (N lines)

$ ./ex10 source.txt copy.txt
copy.txt already exists. Overwrite? [y/n] y
Copied source.txt → copy.txt (N lines)

$ ./ex10 source.txt copy.txt
copy.txt already exists. Overwrite? [y/n] n
Aborted.

$ diff source.txt copy.txt
(sin salida — archivos idénticos)
```

`"wx"` detecta si el destino ya existe. Si falla con `EEXIST`, se abre
con `"w"` solo si el usuario confirma. El patrón necesita dos labels de
cleanup o un manejo cuidadoso para cerrar ambos `FILE*`. `diff` sin
salida confirma que la copia es exacta.

</details>
