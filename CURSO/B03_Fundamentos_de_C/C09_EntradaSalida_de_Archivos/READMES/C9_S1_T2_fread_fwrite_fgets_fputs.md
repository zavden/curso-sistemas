# T02 — fread, fwrite, fgets, fputs

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, `labs/text_lines.c`,
> `labs/strip_newline.c`, `labs/binary_structs.c`, `labs/copy_file.c`,
> `labs/eof_error.c`

## Erratas detectadas

Sin erratas detectadas en el material fuente.

---

## Tabla de contenidos

1. [fgets — leer líneas de texto](#1--fgets--leer-líneas-de-texto)
2. [fputs y puts — escribir texto](#2--fputs-y-puts--escribir-texto)
3. [fgetc y fputc — un carácter a la vez](#3--fgetc-y-fputc--un-carácter-a-la-vez)
4. [Quitar el '\\n' de fgets](#4--quitar-el-n-de-fgets)
5. [fwrite — escritura binaria](#5--fwrite--escritura-binaria)
6. [fread — lectura binaria](#6--fread--lectura-binaria)
7. [Serializar y deserializar structs](#7--serializar-y-deserializar-structs)
8. [Portabilidad de archivos binarios](#8--portabilidad-de-archivos-binarios)
9. [while (!feof) — el error clásico](#9--while-feof--el-error-clásico)
10. [Comparación con Rust](#10--comparación-con-rust)

---

## 1 — fgets — leer líneas de texto

```c
char *fgets(char *str, int n, FILE *stream);
```

### Comportamiento

- Lee hasta `n-1` caracteres o hasta `'\n'` (lo que ocurra primero)
- **Incluye** el `'\n'` en el buffer (si cabe)
- **Siempre** termina con `'\0'`
- Retorna `str` si éxito, `NULL` si EOF o error

```c
char buf[100];
while (fgets(buf, sizeof(buf), f) != NULL) {
    printf("Line: %s", buf);    // sin \n extra — buf ya tiene uno
}
```

### Buffer más pequeño que la línea

Cuando la línea no cabe en el buffer, `fgets` lee hasta `n-1` caracteres y
**no incluye** el `'\n'`. Los caracteres restantes quedan pendientes para la
siguiente llamada:

```c
// Archivo contiene: "hello world\n" (12 bytes)

char buf[8];
fgets(buf, 8, f);
// buf = "hello w\0" — solo 7 chars (n-1), sin '\n'
// Quedan "orld\n" por leer en la siguiente llamada

fgets(buf, 8, f);
// buf = "orld\n\0" — el '\n' indica que la línea lógica terminó
```

### Detectar si la línea está completa

```c
size_t len = strlen(buf);
if (len > 0 && buf[len - 1] == '\n') {
    // Línea completa — el '\n' está presente
} else {
    // Línea truncada (buffer pequeño) o última línea sin '\n'
}
```

La presencia de `'\n'` al final del buffer es la señal de que se leyó una
línea lógica completa.

---

## 2 — fputs y puts — escribir texto

### fputs

```c
int fputs(const char *str, FILE *stream);
// Escribe str al stream. NO agrega '\n'.
// Retorna no-negativo si éxito, EOF si error.

fputs("hello world\n", f);    // '\n' explícito
fputs("hello ", f);
fputs("world\n", f);          // se pueden concatenar llamadas
```

### puts

```c
puts("hello");       // imprime "hello\n" — SÍ agrega '\n'
```

### Diferencia clave

| Función | Destino | Agrega `'\n'` |
|---------|---------|---------------|
| `fputs(s, f)` | Cualquier stream | No |
| `puts(s)` | Solo `stdout` | Sí |

`puts(s)` equivale a `fputs(s, stdout)` seguido de `fputc('\n', stdout)`.

---

## 3 — fgetc y fputc — un carácter a la vez

### fgetc

```c
int ch = fgetc(f);    // retorna int, NO char
```

Retorna el carácter como `unsigned char` convertido a `int`, o `EOF` (-1)
si fin de archivo o error.

**Regla crítica**: la variable debe ser `int`, no `char`:

```c
int ch;     // CORRECTO — puede almacenar EOF (-1)
// char ch; // INCORRECTO — en char firmado, 0xFF == -1 == EOF
//             → confunde el byte 0xFF con fin de archivo

while ((ch = fgetc(f)) != EOF) {
    putchar(ch);
}
```

Si `ch` fuera `char` y el archivo contuviera el byte `0xFF`, el loop
terminaría prematuramente porque `(char)0xFF == -1 == EOF`.

### fputc

```c
fputc('A', f);      // escribe un carácter
fputc('\n', f);     // escribe newline
```

### getchar y putchar

```c
int ch = getchar();     // equivale a fgetc(stdin)
putchar(ch);            // equivale a fputc(ch, stdout)
```

---

## 4 — Quitar el '\n' de fgets

`fgets` incluye el `'\n'` en el buffer. Tres métodos para eliminarlo:

### Método 1 — strcspn (recomendado)

```c
buf[strcspn(buf, "\n")] = '\0';
```

`strcspn(buf, "\n")` retorna la posición del primer `'\n'` (o la longitud
total si no hay `'\n'`). Al poner `'\0'` en esa posición, se elimina el
salto de línea. Funciona correctamente tanto si el `'\n'` está presente
como si no — no necesita verificación extra.

### Método 2 — strchr

```c
char *nl = strchr(buf, '\n');
if (nl) *nl = '\0';
```

### Método 3 — strlen

```c
size_t len = strlen(buf);
if (len > 0 && buf[len - 1] == '\n') buf[len - 1] = '\0';
```

### Por qué strcspn es el mejor

- No necesita `if` — funciona con o sin `'\n'`
- Una sola línea
- No recorre el string dos veces (a diferencia de strlen + verificación)

---

## 5 — fwrite — escritura binaria

```c
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
// Escribe nmemb elementos de tamaño size desde ptr al stream
// Retorna el número de ELEMENTOS escritos (no bytes)
// Si retorna < nmemb, hubo error
```

### Ejemplos

```c
// Escribir un array de ints:
int data[] = {1, 2, 3, 4, 5};
size_t written = fwrite(data, sizeof(int), 5, f);
if (written != 5) perror("fwrite");

// Escribir un struct:
struct Point p = {10, 20};
fwrite(&p, sizeof(struct Point), 1, f);

// Escribir un array de structs:
struct Point points[100];
fwrite(points, sizeof(struct Point), 100, f);
```

### size vs nmemb

Los parámetros `size` y `nmemb` permiten a `fwrite` reportar cuántos
**elementos lógicos** se escribieron. Si se escriben 3 de 5 structs antes
de un error, `fwrite` retorna 3 (no el número de bytes parcial).

Para copiar archivos byte a byte, es común usar `size=1` y `nmemb=N`:

```c
fwrite(buf, 1, bytes_read, f);   // escribir bytes_read bytes
```

---

## 6 — fread — lectura binaria

```c
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
// Lee nmemb elementos de tamaño size del stream a ptr
// Retorna el número de ELEMENTOS leídos
// Si retorna < nmemb: puede ser EOF o error → verificar con feof/ferror
```

### Ejemplo

```c
int data[5];
size_t got = fread(data, sizeof(int), 5, f);
if (got != 5) {
    if (feof(f))
        printf("EOF: only read %zu elements\n", got);
    else
        perror("fread");
}
```

### fread para copiar archivos

```c
unsigned char buf[4096];
size_t n;
while ((n = fread(buf, 1, sizeof(buf), fin)) > 0) {
    fwrite(buf, 1, n, fout);
}
```

Este patrón con bloques de 4096 bytes es significativamente más rápido que
copiar byte a byte con `fgetc`/`fputc`, porque reduce la cantidad de
llamadas a funciones de la biblioteca — incluso considerando que stdio
tiene buffers internos.

---

## 7 — Serializar y deserializar structs

### Patrón: count header + array de structs

```c
struct Sensor {
    int id;
    char label[32];
    double value;
};

// Guardar:
int save(const char *path, const struct Sensor *data, int count) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    fwrite(&count, sizeof(int), 1, f);              // header: cuántos
    fwrite(data, sizeof(struct Sensor), count, f);   // datos
    fclose(f);
    return 0;
}

// Cargar:
int load(const char *path, struct Sensor **data, int *count) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fread(count, sizeof(int), 1, f);                 // leer header
    *data = malloc(*count * sizeof(struct Sensor));
    if (!*data) { fclose(f); return -1; }

    size_t got = fread(*data, sizeof(struct Sensor), *count, f);
    fclose(f);
    if ((int)got != *count) { free(*data); return -1; }
    return 0;
}
```

### Verificación con memcmp

```c
if (memcmp(original, loaded, count * sizeof(struct Sensor)) == 0) {
    printf("PASS — data matches exactly\n");
}
```

### Inspección con xxd

```bash
xxd sensors.bin | head -20
```

Los primeros 4 bytes son el count en little-endian (`03 00 00 00`), seguido
de los structs con sus campos en representación binaria de memoria.

---

## 8 — Portabilidad de archivos binarios

Los archivos escritos con `fwrite` de structs **no son portables**:

### Tres problemas

**1. Endianness**: un `int` escrito en little-endian (x86) se lee
incorrectamente en big-endian (algunos ARM, MIPS, SPARC):

```
Little-endian: 0x01 0x00 0x00 0x00  →  valor 1
Big-endian:    0x01 0x00 0x00 0x00  →  valor 16777216 (0x01000000)
```

**2. Padding**: un struct puede tener padding diferente en otro
compilador o arquitectura. `sizeof(struct X)` no es universal.

**3. Tamaño de tipos**: `sizeof(int)` puede ser 2 o 4 según la
plataforma. `sizeof(long)` varía entre 4 (Windows 64-bit) y 8 (Linux
64-bit).

### Soluciones para portabilidad

- Usar tipos de tamaño fijo: `uint32_t`, `int64_t`, `float` (IEEE 754)
- Serializar campo por campo (no structs enteros)
- Definir un endianness (generalmente little-endian por convención x86)
- O usar formatos de texto: JSON, CSV, XML

### Verificar endianness

```bash
echo -n I | od -to2 | head -1
# 0000001 al inicio → little-endian (x86)
```

---

## 9 — while (!feof) — el error clásico

### El problema

```c
// INCORRECTO — una iteración extra:
while (!feof(f)) {
    size_t r = fread(&val, sizeof(int), 1, f);
    // En la última iteración: r==0, val conserva el valor anterior
    process(val);    // ¡procesa un dato duplicado!
}
```

### Por qué falla

`feof()` solo devuelve `true` **después** de que una lectura intentó pasar
el final del archivo. La secuencia es:

1. Iteración 5: `fread` lee el último int. El cursor queda al final pero
   `feof()` es 0 — no se ha intentado leer **más allá**.
2. Iteración 6: el loop entra porque `feof()` sigue siendo 0. `fread`
   intenta leer, no hay datos: retorna 0 y **ahora** `feof()` es 1. Pero
   `val` tiene el valor anterior → se procesa un duplicado.

### La solución correcta

```c
// CORRECTO — usar el retorno de fread como condición:
while (fread(&val, sizeof(int), 1, f) == 1) {
    process(val);
}

// DESPUÉS del loop, diagnosticar la causa:
if (feof(f)) {
    // Terminación normal — fin de archivo
} else if (ferror(f)) {
    // Error de I/O
    perror("read error");
}
```

### Lo mismo aplica a fgets

```c
// INCORRECTO:
while (!feof(f)) {
    fgets(buf, sizeof(buf), f);
    // Puede procesar una línea extra vacía
}

// CORRECTO:
while (fgets(buf, sizeof(buf), f) != NULL) {
    // process buf
}
```

**Regla general**: siempre usar el **valor de retorno de la función de
lectura** como condición del loop. Reservar `feof`/`ferror` para
diagnosticar la causa de terminación **después** del loop.

---

## 10 — Comparación con Rust

| Aspecto | C `stdio` | Rust `std::io` |
|---------|-----------|----------------|
| Leer línea | `fgets(buf, n, f)` — buffer fijo | `reader.read_line(&mut s)` — String crece |
| Escribir texto | `fputs(s, f)` | `write!(f, "{}", s)?` |
| Leer binario | `fread(ptr, size, n, f)` | `f.read_exact(&mut buf)?` |
| Escribir binario | `fwrite(ptr, size, n, f)` | `f.write_all(&buf)?` |
| EOF/error | `feof`/`ferror` post-loop | `Result<>` en cada operación |
| Buffer overflow | Posible si `n` incorrecto | Imposible — bounds checking |

### Lectura de líneas

```c
// C — buffer fijo, '\n' incluido, truncamiento posible:
char buf[256];
while (fgets(buf, sizeof(buf), f)) {
    buf[strcspn(buf, "\n")] = '\0';
    // procesar buf
}
```

```rust
// Rust — String crece dinámicamente, sin truncamiento:
use std::io::{BufRead, BufReader};

let reader = BufReader::new(f);
for line in reader.lines() {
    let line = line?;  // String sin '\n', error propagado
    // procesar line
}
```

En Rust, `lines()` retorna un iterador que elimina el `'\n'`
automáticamente y cada lectura retorna `Result` — no existe el problema de
`while (!feof)`.

### I/O binario

```c
// C — fread/fwrite con void*:
struct Point p;
fread(&p, sizeof(struct Point), 1, f);
// Sin verificación de tipos — cualquier ptr sirve
```

```rust
// Rust — serialización explícita (serde, bincode):
use serde::{Serialize, Deserialize};
use bincode;

#[derive(Serialize, Deserialize)]
struct Point { x: i32, y: i32 }

let p: Point = bincode::deserialize_from(&mut f)?;
// Tipo verificado en compilación, portabilidad configurable
```

Rust no tiene equivalente directo a `fread` de un struct crudo — la
comunidad prefiere serialización explícita con crates como `serde`/`bincode`,
que resuelven los problemas de portabilidad (endianness, padding) que C
ignora.

### Copia de archivos

```rust
// Rust — una línea:
std::fs::copy("source.txt", "dest.txt")?;

// O con control fino:
let mut src = File::open("source.txt")?;
let mut dst = File::create("dest.txt")?;
std::io::copy(&mut src, &mut dst)?;  // buffered internamente
```

---

## Ejercicios

### Ejercicio 1 — fgets con buffer pequeño

```c
// Crear un archivo "test.txt" con fputs que contenga:
//   - Una línea corta (< 10 chars)
//   - Una línea mediana (~30 chars)
//   - Una línea larga (~80 chars)
//   - Una línea final sin '\n'
//
// Leer con fgets y buffer de 15 bytes.
// Para cada chunk, imprimir: número, longitud, si tiene '\n', contenido.
// Contar cuántas líneas lógicas hay (una línea = chunks hasta encontrar '\n').
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex01.c -o ex01
```

<details><summary>Predicción</summary>

```
Chunk  1 | len= 6 | nl=yes | "short\n"
Chunk  2 | len=14 | nl=no  | "this is a medi"
Chunk  3 | len=14 | nl=no  | "um-length line"
Chunk  4 | len= 1 | nl=yes | "\n"
...
Logical lines: 4
```

Con buffer de 15: `fgets` lee máximo 14 caracteres. La línea corta cabe
completa (nl=yes). La mediana se divide en chunks sin `'\n'` hasta el
último que tiene `'\n'`. La última línea termina con nl=no porque no tiene
`'\n'` en el archivo. Cada secuencia de chunks hasta un `'\n'` (o EOF)
forma una línea lógica.

</details>

---

### Ejercicio 2 — Tres métodos para quitar '\n'

```c
// Leer un archivo línea por línea con fgets (buffer de 256).
// Para cada línea, quitar '\n' de tres formas independientes:
//   1. strcspn
//   2. strchr
//   3. strlen
//
// Imprimir la línea resultante de cada método y verificar que
// los tres producen el mismo resultado.
//
// Probar con un archivo que tenga líneas normales y una última
// línea sin '\n'.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex02.c -o ex02
```

<details><summary>Predicción</summary>

```
Line 1:
  strcspn: "Hello World"
  strchr:  "Hello World"
  strlen:  "Hello World"
  Match: yes

Line 4 (no trailing \n):
  strcspn: "last line"
  strchr:  "last line"
  strlen:  "last line"
  Match: yes
```

Los tres métodos producen el mismo resultado. `strcspn` es el más limpio
porque no necesita `if`. `strchr` y `strlen` necesitan verificar que el
`'\n'` existe antes de eliminarlo. Para la última línea sin `'\n'`, los
tres dejan el buffer sin cambios (ya no tiene `'\n'` que quitar).

</details>

---

### Ejercicio 3 — fwrite/fread con array de structs

```c
// Definir: struct Student { int id; char name[50]; double gpa; };
//
// Crear un array de 5 estudiantes con datos variados.
// Guardar en "students.bin":
//   1. Escribir count (int) como header
//   2. Escribir array de structs
//
// Cargar desde "students.bin":
//   1. Leer count
//   2. malloc para el array
//   3. Leer structs
//   4. Imprimir todos
//   5. Verificar con memcmp que coinciden
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex03.c -o ex03
```

<details><summary>Predicción</summary>

```
Wrote 5 students to students.bin
sizeof(struct Student) = 64
File size = 324 bytes (4 + 5*64)

Loaded 5 students:
  #1  Alice       GPA=3.90
  #2  Bob         GPA=3.50
  #3  Carol       GPA=3.75
  #4  David       GPA=2.80
  #5  Eve         GPA=4.00

Verification: PASS
```

sizeof(struct Student): int(4) + char[50](50) → offset 54, padding 2
para alinear double → offset 56 + double(8) = 64 bytes. El header es
4 bytes (int count), total 4 + 5×64 = 324 bytes. `memcmp` compara los
bytes crudos incluyendo padding — funciona porque los datos se escribieron
y leyeron en la misma arquitectura.

</details>

---

### Ejercicio 4 — Copiar archivo: byte vs bloque

```c
// Implementar dos funciones de copia:
//   long copy_byte(const char *src, const char *dst)   — fgetc/fputc
//   long copy_block(const char *src, const char *dst)  — fread/fwrite (4096)
//
// Ambas retornan bytes copiados o -1 si error.
//
// Crear archivo de prueba con dd:
//   dd if=/dev/urandom of=test.bin bs=1024 count=500
//
// Medir el tiempo de cada una con clock().
// Verificar integridad con cmp.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex04.c -o ex04
```

<details><summary>Predicción</summary>

```
Byte-by-byte: 512000 bytes in ~4.00 ms
Block (4096): 512000 bytes in ~0.80 ms
Speedup:      ~5x faster

cmp test.bin copy_byte.tmp → identical
cmp test.bin copy_block.tmp → identical
```

La copia por bloques es ~5x más rápida porque reduce las llamadas a
funciones de biblioteca. Aunque stdio tiene buffer interno (~8KB), cada
`fgetc` sigue necesitando verificar el buffer, actualizar posición, y
retornar — la sobrecarga por llamada se multiplica por 512000. Con bloques
de 4096, son solo ~125 llamadas a `fread`.

</details>

---

### Ejercicio 5 — while (!feof) vs forma correcta

```c
// Escribir 5 ints a "data.bin" con fwrite.
// Leer de DOS formas:
//
// Forma 1 (incorrecta): while (!feof(f)) { fread(&val, ...); print; }
// Forma 2 (correcta):   while (fread(&val, ...) == 1) { print; }
//
// Después de cada loop, mostrar feof() y ferror().
// Contar las iteraciones de cada forma.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex05.c -o ex05
```

<details><summary>Predicción</summary>

```
Wrote 5 ints to data.bin

Incorrect (!feof):
  1: val=10, 2: val=20, 3: val=30, 4: val=40, 5: val=50, 6: val=50
  Iterations: 6 (one extra!)

Correct (fread return):
  1: val=10, 2: val=20, 3: val=30, 4: val=40, 5: val=50
  Iterations: 5 (correct)
  Stopped because: EOF
```

La forma incorrecta ejecuta 6 iteraciones porque `feof` solo es true
**después** de que `fread` falla. En la iteración 6, `fread` retorna 0
pero `val` conserva el valor 50 de la iteración anterior → se procesa
un duplicado. La forma correcta usa el retorno de `fread` como condición
y termina en exactamente 5 iteraciones.

</details>

---

### Ejercicio 6 — Leer archivo con número de línea

```c
// Implementar:
//   void print_numbered(const char *path)
// que lea un archivo línea por línea con fgets (buffer 256),
// quite el '\n' con strcspn, e imprima cada línea con su número:
//   "  1: primera línea"
//   "  2: segunda línea"
//
// Manejar correctamente:
//   - Archivo inexistente (perror)
//   - Líneas más largas que 256 bytes (no duplicar números)
//
// Probar con el propio archivo fuente: ./ex06 ex06.c
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex06.c -o ex06
```

<details><summary>Predicción</summary>

```
  1: #include <stdio.h>
  2: #include <string.h>
  3:
  4: void print_numbered(const char *path) {
  ...
```

Para manejar líneas largas sin duplicar números: solo incrementar el
contador de línea cuando el chunk anterior terminó con `'\n'` (o es el
primer chunk). Verificar `buf[strlen(buf)-1] == '\n'` antes de
incrementar en el siguiente chunk.

</details>

---

### Ejercicio 7 — fputs: escribir líneas sin '\n' automático

```c
// Crear un programa que escriba un archivo "output.txt" usando
// SOLO fputs (no fprintf ni puts):
//   - Línea 1: "Name: Alice\n"
//   - Línea 2: "Age: 30\n"
//   - Línea 3: "Score: 95.5\n"
//
// Leer de vuelta con fgets e imprimir.
// Verificar que fputs NO agregó '\n' extra (el archivo tiene
// exactamente 3 líneas, no 6).
//
// Comparar: reescribir con puts() a stdout y notar que puts
// SÍ agrega '\n'.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex07.c -o ex07
```

<details><summary>Predicción</summary>

```
Written with fputs:
  Line 1: "Name: Alice"
  Line 2: "Age: 30"
  Line 3: "Score: 95.5"
Total lines: 3

With puts to stdout:
Name: Alice    ← puts agrega '\n' automáticamente
Age: 30
Score: 95.5
```

`fputs("Name: Alice\n", f)` escribe exactamente esos bytes — el `'\n'`
es parte del string, no agregado por fputs. `puts("Name: Alice")` agrega
`'\n'` automáticamente — si el string ya tuviera `'\n'`, habría un
salto de línea doble.

</details>

---

### Ejercicio 8 — fgetc con detección de tipo int

```c
// Escribir un archivo binario con fwrite que contenga el byte 0xFF
// entre otros bytes normales: {0x41, 0xFF, 0x42}  ('A', 0xFF, 'B')
//
// Leer con fgetc de DOS formas:
//   1. Con variable char ch — ¿qué pasa con 0xFF?
//   2. Con variable int ch — ¿funciona correctamente?
//
// Imprimir cada byte leído en hexadecimal.
// Explicar por qué char falla y int funciona.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex08.c -o ex08
```

<details><summary>Predicción</summary>

```
Reading with char (WRONG):
  0x41 ('A')
  (loop terminates — 0xFF interpreted as EOF!)

Reading with int (CORRECT):
  0x41 ('A')
  0xFF
  0x42 ('B')
  (EOF reached normally)
```

Con `char ch`: en plataformas donde `char` es firmado, `(char)0xFF == -1`.
Comparar `ch != EOF` da `(-1) != (-1)` → false → el loop termina al
encontrar 0xFF, perdiendo el byte 0x42. Con `int ch`: `0xFF` se almacena
como 255, que es distinto de `EOF` (-1) → el loop continúa correctamente.

</details>

---

### Ejercicio 9 — Posicionamiento: ftell y fseek

```c
// Escribir 10 structs Record { int id; double value; } a "records.bin".
// Sin cerrar el archivo:
//   1. Usar ftell para obtener el tamaño total
//   2. Usar fseek(SEEK_SET) para ir al record #5 (índice 4)
//   3. Leer ese record e imprimir
//   4. Usar fseek(SEEK_CUR, -sizeof(Record)) para releer el mismo
//   5. Usar fseek(SEEK_END, -sizeof(Record)) para leer el último
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex09.c -o ex09
```

<details><summary>Predicción</summary>

```
File size: 120 bytes (10 records × 12 bytes each)

Record at index 4: id=5, value=50.0
Re-read same record: id=5, value=50.0
Last record: id=10, value=100.0
```

sizeof(Record): int(4) + padding(4) + double(8) = 16 bytes (no 12 — el
double necesita alineación 8). Total: 160 bytes. `fseek(f, 4 * sizeof(Record), SEEK_SET)` salta al quinto record. `fseek(f, -sizeof(Record), SEEK_CUR)` retrocede un record desde la posición actual.
`fseek(f, -sizeof(Record), SEEK_END)` lee el último record.

</details>

---

### Ejercicio 10 — Texto vs binario: el mismo dato

```c
// Crear un array: int data[] = {1, 256, 65535, -1, 0};
//
// Guardar de DOS formas:
//   1. Texto: fprintf(f, "%d\n", data[i]) a "data.txt"
//   2. Binario: fwrite(data, sizeof(int), 5, f) a "data.bin"
//
// Comparar:
//   - Tamaño de cada archivo (stat o ftell)
//   - Contenido de data.txt (cat)
//   - Contenido de data.bin (xxd | head)
//   - Leer ambos de vuelta y verificar que los valores coinciden
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
```

<details><summary>Predicción</summary>

```
data.txt: 19 bytes  (text: "1\n256\n65535\n-1\n0\n")
data.bin: 20 bytes  (binary: 5 × sizeof(int) = 5 × 4)

data.txt content:
1
256
65535
-1
0

data.bin content (xxd):
0000: 0100 0000 0001 0000 ffff 0000 ffff ffff
0010: 0000 0000

Values match after reading both: yes
```

El archivo de texto varía en tamaño según el número de dígitos de cada
valor. El archivo binario siempre es exactamente `n × sizeof(int)` bytes,
independientemente del valor. El binario es más compacto para números
grandes y más rápido de leer/escribir, pero no es legible por humanos ni
portable entre arquitecturas.

</details>
