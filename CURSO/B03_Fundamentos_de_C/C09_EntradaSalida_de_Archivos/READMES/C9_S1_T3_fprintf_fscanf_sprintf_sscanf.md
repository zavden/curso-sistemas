# T03 — fprintf, fscanf, sprintf, sscanf

> **Fuentes**: `README.md`, `LABS.md`, `labs/README.md`, `labs/fprintf_log.c`,
> `labs/fscanf_parse.c`, `labs/snprintf_buf.c`, `labs/sscanf_extract.c`

## Erratas detectadas

Sin erratas detectadas en el material fuente.

---

## Tabla de contenidos

1. [La familia printf](#1--la-familia-printf)
2. [fprintf — escritura formateada a archivo](#2--fprintf--escritura-formateada-a-archivo)
3. [Especificadores de formato](#3--especificadores-de-formato)
4. [sprintf vs snprintf](#4--sprintf-vs-snprintf)
5. [La familia scanf](#5--la-familia-scanf)
6. [sscanf y el patrón fgets + sscanf](#6--sscanf-y-el-patrón-fgets--sscanf)
7. [Peligros de scanf](#7--peligros-de-scanf)
8. [Vulnerabilidades de format string](#8--vulnerabilidades-de-format-string)
9. [Funciones v* (wrappers con va_list)](#9--funciones-v-wrappers-con-va_list)
10. [Comparación con Rust](#10--comparación-con-rust)

---

## 1 — La familia printf

Todas escriben con formato — difieren en el **destino**:

```c
int printf(const char *fmt, ...);                       // → stdout
int fprintf(FILE *f, const char *fmt, ...);             // → archivo/stream
int sprintf(char *str, const char *fmt, ...);           // → string (PELIGROSO)
int snprintf(char *str, size_t n, const char *fmt, ...); // → string (SEGURO)
int dprintf(int fd, const char *fmt, ...);              // → file descriptor (POSIX)
```

### Valor de retorno

Todas retornan el **número de caracteres escritos** (sin contar `'\0'`).
`snprintf` retorna lo que **habría escrito** si el buffer fuera suficiente —
esto permite detectar truncación.

Excepción: retornan un valor **negativo** si hay un error de codificación.

---

## 2 — fprintf — escritura formateada a archivo

```c
FILE *f = fopen("log.txt", "w");

fprintf(f, "User: %s, Age: %d\n", "Alice", 30);
fprintf(f, "Score: %.2f%%\n", 95.5);      // %% imprime un % literal
fprintf(f, "Timestamp: %ld\n", (long)time(NULL));

fclose(f);
```

### fprintf a stderr

```c
fprintf(stderr, "Error: %s\n", strerror(errno));
```

`fprintf(stderr, ...)` es la forma correcta de emitir mensajes de error.
Se separan de stdout y se pueden redirigir independientemente.

### printf es fprintf a stdout

```c
printf("hello\n");
// es exactamente:
fprintf(stdout, "hello\n");
```

---

## 3 — Especificadores de formato

### Enteros

```c
printf("%d",   42);          // 42 (decimal, signed)
printf("%u",   42u);         // 42 (decimal, unsigned)
printf("%x",   255);         // ff (hexadecimal minúscula)
printf("%X",   255);         // FF (hexadecimal mayúscula)
printf("%o",   8);           // 10 (octal)
printf("%ld",  42L);         // long
printf("%lld", 42LL);        // long long
printf("%zu",  sizeof(int)); // size_t (crucial — usar siempre %zu para size_t)
```

### Flotantes

```c
printf("%f",   3.14);        // 3.140000 (6 decimales por defecto)
printf("%.2f", 3.14);        // 3.14 (2 decimales)
printf("%e",   3.14);        // 3.140000e+00 (notación científica)
printf("%g",   3.14);        // 3.14 (la más corta entre %f y %e)
```

### Strings y caracteres

```c
printf("%s",   "hello");     // hello
printf("%.3s", "hello");     // hel (máximo 3 chars)
printf("%c",   'A');         // A
```

### Punteros

```c
printf("%p", (void *)ptr);   // 0x7ffd1234abcd
```

### Ancho y alineación

```c
printf("%10d",  42);          //         42 (derecha, ancho 10)
printf("%-10d", 42);          // 42         (izquierda, ancho 10)
printf("%010d", 42);          // 0000000042 (relleno con ceros)
printf("%+d",   42);          // +42 (siempre mostrar signo)
```

### Tabla resumen para scanf

En `scanf`/`fscanf`/`sscanf`, los especificadores difieren ligeramente:

| Tipo | printf | scanf |
|------|--------|-------|
| `int` | `%d` | `%d` |
| `float` | `%f` | `%f` |
| `double` | `%f` (promoción a double) | `%lf` (requiere `l`) |
| `char*` | `%s` | `%s` (peligroso sin límite) |
| `char*` (con límite) | — | `%49s` (máx 49 chars) |

La diferencia importante: en `printf`, `%f` funciona tanto para `float` como
para `double` (por promoción de argumentos variádicos). En `scanf`, **`%lf`
es obligatorio para `double`** y `%f` es para `float`.

---

## 4 — sprintf vs snprintf

### sprintf — NUNCA usar

```c
char buf[20];
sprintf(buf, "Hello, %s! Score: %d", name, score);
// Si el resultado es > 19 chars → buffer overflow → UB
// sprintf NO verifica el tamaño del buffer
```

### snprintf — SIEMPRE usar

```c
char buf[20];
int n = snprintf(buf, sizeof(buf), "Hello, %s! Score: %d", name, score);
// Escribe máximo sizeof(buf)-1 chars + '\0'
// SIEMPRE termina con '\0'
// Retorna cuántos chars HABRÍA escrito sin límite
```

### Detectar truncación

```c
if (n >= (int)sizeof(buf)) {
    printf("Truncated! Needed %d+1 bytes, had %zu\n", n, sizeof(buf));
}
```

Si el retorno `n` es mayor o igual al tamaño del buffer, hubo truncación.
El string fue cortado pero sigue siendo válido (termina en `'\0'`).

### Patrón string builder

Para construir strings incrementalmente sin overflow:

```c
char report[80];
int pos = 0;

pos += snprintf(report + pos, sizeof(report) - pos, "Name: %s\n", name);
pos += snprintf(report + pos, sizeof(report) - pos, "Age: %d\n", age);
pos += snprintf(report + pos, sizeof(report) - pos, "Score: %.1f\n", score);

if (pos >= (int)sizeof(report)) {
    printf("Report truncated\n");
}
```

Cada llamada escribe a partir de `report + pos` con un límite de
`sizeof(report) - pos` bytes restantes. El buffer nunca se desborda.

**Regla**: NUNCA usar `sprintf`. SIEMPRE usar `snprintf`.

---

## 5 — La familia scanf

Todas leen con formato — difieren en la **fuente**:

```c
int scanf(const char *fmt, ...);                    // ← stdin
int fscanf(FILE *f, const char *fmt, ...);          // ← archivo
int sscanf(const char *str, const char *fmt, ...);  // ← string
```

### Valor de retorno

Retornan el **número de campos leídos exitosamente**, no el número de bytes.
Retornan `EOF` si hay error o fin de entrada antes de leer cualquier campo.

```c
int age;
double score;
char name[50];

int n = fscanf(f, "%49s %d %lf", name, &age, &score);
if (n == 3) {
    // Los 3 campos se parsearon correctamente
} else if (n == EOF) {
    // Fin de archivo o error de lectura
} else {
    // Parseo parcial — n campos leídos antes de fallar
}
```

### fscanf con líneas corruptas

Cuando `fscanf` falla a mitad de una línea, deja los caracteres **no
consumidos** en el stream. Si no los eliminas, el siguiente `fscanf`
intenta leer los mismos datos → loop infinito:

```c
while ((fields = fscanf(f, "%63s %lf", name, &value)) != EOF) {
    if (fields == 2) {
        // OK — procesamos la línea
    } else {
        // Error de parseo — consumir el residuo:
        int ch;
        while ((ch = fgetc(f)) != '\n' && ch != EOF);
    }
}
```

---

## 6 — sscanf y el patrón fgets + sscanf

### sscanf — parsear un string

```c
const char *line = "2026-03-18 ERROR Connection refused on port 8080";

int year, month, day;
char level[16];
char message[128];

int n = sscanf(line, "%d-%d-%d %15s %127[^\n]",
               &year, &month, &day, level, message);

if (n == 5) {
    printf("%04d-%02d-%02d [%s] %s\n", year, month, day, level, message);
}
```

### El especificador %[^\n]

`%[^\n]` es un **scanset invertido**: lee todos los caracteres **excepto**
`'\n'`. A diferencia de `%s` (que se detiene en cualquier whitespace),
`%[^\n]` captura espacios, tabs y todo lo demás. Siempre usar con límite:
`%127[^\n]`.

### El patrón recomendado: fgets + sscanf

```c
char buf[256];
while (fgets(buf, sizeof(buf), f)) {
    char name[64];
    int age;
    double score;

    if (sscanf(buf, "%63[^,],%d,%lf", name, &age, &score) == 3) {
        printf("%s: age %d, score %.1f\n", name, age, score);
    } else {
        printf("Parse error: %s", buf);
    }
}
```

### Por qué fgets + sscanf es superior

| Aspecto | `fscanf` directo | `fgets` + `sscanf` |
|---------|------------------|--------------------|
| Buffer overflow | Posible sin límites | `fgets` limita |
| Línea corrupta | Residuo en stream → loop | `fgets` consume toda la línea |
| Depuración | No puedes ver la línea original | `buf` contiene la línea para logging |
| Complejidad | Más simple (una función) | Dos funciones pero más robusto |

**Recomendación**: usar `fgets` + `sscanf` en lugar de `scanf`/`fscanf`
directo siempre que sea posible.

### sscanf valida sintaxis, no semántica

```c
int a, b, c, d;
sscanf("256.1.2.3", "%d.%d.%d.%d", &a, &b, &c, &d);
// Parsea exitosamente 4 campos: a=256, b=1, c=2, d=3
// Pero 256 NO es un octeto IPv4 válido (rango 0-255)
// La validación semántica es responsabilidad del programador
```

---

## 7 — Peligros de scanf

### 1. %s sin límite → buffer overflow

```c
char name[10];
scanf("%s", name);     // input > 9 chars → overflow → UB
scanf("%9s", name);    // CORRECTO — máximo 9 chars + '\0'
```

### 2. scanf deja '\n' residual en stdin

```c
int n;
scanf("%d", &n);       // input "42\n" — consume "42", deja '\n'
char c;
scanf("%c", &c);       // c = '\n' — lee el residuo, no la siguiente entrada

// Solución: espacio antes de %c consume whitespace:
scanf(" %c", &c);      // el espacio consume '\n' y otros whitespace
```

### 3. Input inválido deja datos atascados

```c
int x;
if (scanf("%d", &x) != 1) {
    // Input "abc" → "abc" sigue en stdin
    // El siguiente scanf("%d") también falla
    // Hay que consumir la entrada inválida:
    int ch;
    while ((ch = getchar()) != '\n' && ch != EOF);
}
```

### 4. %s solo lee hasta whitespace

```c
char name[50];
scanf("%49s", name);   // input "Alice Bob" → name = "Alice"
                       // "Bob" queda en stdin
```

### 5. %lf obligatorio para double en scanf

```c
double val;
scanf("%f", &val);     // INCORRECTO — %f es para float
scanf("%lf", &val);    // CORRECTO — %lf para double
// En printf, %f funciona para ambos por promoción. En scanf, no.
```

---

## 8 — Vulnerabilidades de format string

Si el **format string viene del usuario**, es una vulnerabilidad grave:

```c
char user_input[100];
fgets(user_input, sizeof(user_input), stdin);

// VULNERABLE:
printf(user_input);
// Si input = "%x %x %x": lee valores del stack
// Si input = "%n": ESCRIBE en memoria (número de chars impresos hasta ese punto)
// Puede llevar a ejecución de código arbitrario

// SEGURO:
printf("%s", user_input);    // el usuario no controla el formato
fputs(user_input, stdout);   // alternativa sin formateo
```

### Detección con el compilador

```c
// gcc -Wformat-security (incluido en -Wall) avisa:
printf(user_input);
// warning: format not a string literal and no format arguments [-Wformat-security]
```

### La regla

El primer argumento de `printf`/`fprintf`/etc. **siempre** debe ser un
**string literal**, nunca una variable controlable por el usuario.

El ataque de format string es uno de los más antiguos y peligrosos en C.
Históricamente explotó funciones como `syslog(user_message)` y
`fprintf(log, user_message)`.

---

## 9 — Funciones v* (wrappers con va_list)

Para crear **wrappers** de printf con argumentos variables, se usan las
funciones `v*` que aceptan `va_list`:

```c
#include <stdarg.h>

void log_msg(const char *level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);

    fprintf(stderr, "[%s] ", level);
    vfprintf(stderr, fmt, args);     // vfprintf acepta va_list
    fprintf(stderr, "\n");

    va_end(args);
}

log_msg("ERROR", "Failed to open %s: %s", path, strerror(errno));
// [ERROR] Failed to open /tmp/data: No such file or directory
```

### La familia v*

| Función | Destino |
|---------|---------|
| `vprintf` | stdout |
| `vfprintf` | archivo |
| `vsprintf` | string (peligroso) |
| `vsnprintf` | string (seguro) |
| `vscanf` | stdin |
| `vfscanf` | archivo |
| `vsscanf` | string |

### Atributo format para verificación

```c
__attribute__((format(printf, 3, 4)))
void log_msg(const char *level, const char *fmt, ...);
// GCC/Clang verifican que los argumentos coincidan con el formato
// El 3 indica que el parámetro 3 es el format string
// El 4 indica que los argumentos variádicos empiezan en el parámetro 4
```

Con este atributo, el compilador avisa si pasas un `int` donde el formato
espera `%s`, exactamente como lo hace con `printf` nativo.

---

## 10 — Comparación con Rust

| Aspecto | C `printf`/`scanf` | Rust `format!`/parsing |
|---------|--------------------|-----------------------|
| Formateo | `printf("%d %s", n, s)` | `format!("{} {}", n, s)` |
| Tipo seguro | No — mismatch es UB | Sí — verificado en compilación |
| Buffer overflow | `sprintf` vulnerable | Imposible — `String` crece |
| Truncación | `snprintf` + verificación manual | No aplica — `String` crece |
| Parseo | `scanf`/`sscanf` con `%d`, `%s` | `.parse::<i32>()` con `Result` |
| Format string attack | Posible si formato es variable | Imposible — formato es macro |

### Formateo en Rust

```rust
// Equivalente a printf/fprintf:
println!("User: {}, Age: {}", name, age);         // → stdout
eprintln!("Error: {}", message);                    // → stderr
writeln!(file, "Score: {:.2}", score)?;             // → archivo

// Equivalente a snprintf:
let s = format!("User: {}, Score: {}", name, score);
// String crece dinámicamente — nunca hay truncación
```

### Parseo en Rust

```rust
// Equivalente a sscanf — pero type-safe:
let input = "42";
let n: i32 = input.parse().unwrap();  // Result<i32, ParseIntError>

// Parseo de líneas CSV:
let line = "Alice,30,95.5";
let parts: Vec<&str> = line.split(',').collect();
let name = parts[0];
let age: i32 = parts[1].parse()?;
let score: f64 = parts[2].parse()?;
```

### Por qué Rust no tiene format string attacks

```rust
// En Rust, el formato es una macro compilada:
let user_input = "%x %x %x";
println!("{}", user_input);   // imprime literalmente "%x %x %x"
// El usuario NO puede inyectar especificadores de formato
// porque {} es interpretado por el compilador, no en runtime

// println!(user_input);  // ERROR de compilación
// "format argument must be a string literal"
```

Rust resuelve en compilación los dos problemas principales de printf en C:
la verificación de tipos de los argumentos y la prevención de format string
injection.

---

## Ejercicios

### Ejercicio 1 — fprintf: escribir log con timestamp

```c
// Crear un programa que escriba un archivo "app.log" con:
//   [TIMESTAMP] [LEVEL] mensaje
//
// Usar fprintf con formatos:
//   %-12ld para timestamp (alineado izquierda, ancho 12)
//   %-8s   para level (alineado izquierda, ancho 8)
//   %s     para mensaje
//
// Escribir 5 entradas con niveles: DEBUG, INFO, WARN, ERROR, INFO.
// Releer y mostrar con fgets.
// Usar fprintf(stderr, ...) para un mensaje de confirmación.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex01.c -o ex01
```

<details><summary>Predicción</summary>

```
(stderr) Log written to app.log
--- Contents of app.log ---
1711900000   DEBUG    Application starting
1711900001   INFO     Configuration loaded
1711900002   WARN     Disk usage at 85%
1711900003   ERROR    Database connection timeout
1711900004   INFO     Retry successful
```

`%-12ld` alinea el timestamp a la izquierda con ancho mínimo 12.
`%-8s` alinea el nivel a la izquierda con ancho 8 (padded con espacios).
`fprintf(stderr, ...)` va a stderr — redirigir con `2>/dev/null` lo oculta.

</details>

---

### Ejercicio 2 — fscanf: parsear con manejo de errores

```c
// Crear un archivo "scores.dat" con formato "nombre puntuación":
//   Alice 95.5
//   Bob 87.2
//   CORRUPTED
//   Carol 91.8
//   Dave xyz
//
// Parsear con fscanf("%49s %lf", name, &score).
// Para cada línea, imprimir:
//   - Si éxito (fields==2): nombre y puntuación
//   - Si error: "SKIP" y el número de campos leídos
// Consumir el residuo de líneas corruptas con fgetc.
// Imprimir estadísticas: parseadas, fallidas, total.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex02.c -o ex02
```

<details><summary>Predicción</summary>

```
  Line 1: Alice     95.50  [OK: 2 fields]
  Line 2: Bob       87.20  [OK: 2 fields]
  Line 3: parse error (got 1 field)  [SKIP]
  Line 4: Carol     91.80  [OK: 2 fields]
  Line 5: parse error (got 1 field)  [SKIP]

Result: 3 parsed, 2 failed, 5 total
```

"CORRUPTED" se lee como `%s` (1 campo) pero no hay double después.
"Dave xyz": "Dave" se lee como `%s`, pero "xyz" no es un double válido.
Ambas fallan con `fields == 1`. Sin consumir el residuo, el loop se
atascaria intentando parsear "xyz" o la siguiente palabra indefinidamente.

</details>

---

### Ejercicio 3 — snprintf: detección de truncación

```c
// Implementar:
//   int format_user(char *buf, size_t size,
//                   const char *name, int age, double score)
// que formatee: "Name: <name>, Age: <age>, Score: <score>"
//
// Probar con:
//   - Buffer de 100 bytes (sin truncación)
//   - Buffer de 30 bytes (truncación)
//   - Buffer de 10 bytes (truncación severa)
//
// Para cada caso, imprimir:
//   - El string resultante
//   - Bytes necesarios vs capacidad
//   - Si hubo truncación
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex03.c -o ex03
```

<details><summary>Predicción</summary>

```
buf[100]: "Name: Alice, Age: 30, Score: 95.50"
  needed=34, capacity=100, truncated=NO

buf[30]:  "Name: Alice, Age: 30, Score:"
  needed=34, capacity=30, truncated=YES

buf[10]:  "Name: Ali"
  needed=34, capacity=10, truncated=YES
```

`snprintf` siempre retorna 34 (lo que habría escrito), sin importar el
tamaño del buffer. Con buffer de 30: escribe 29 chars + `'\0'`. Con
buffer de 10: escribe 9 chars + `'\0'`. En ambos casos truncados, el
string es válido y termina en `'\0'`.

</details>

---

### Ejercicio 4 — snprintf string builder

```c
// Construir un string "report" con el patrón string builder:
//
// struct Metric { const char *name; double value; const char *unit; };
//
// Dado un array de 5 métricas, construir un string como:
//   "cpu=72.5°C gpu=68.3°C ram=85.0% disk=62.1% net=45.2Mbps"
//
// Usar snprintf(buf+pos, sizeof(buf)-pos, ...) en un loop.
// Probar con buffer de 80 bytes (cabe todo) y 40 bytes (truncación).
// Detectar y reportar la truncación.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex04.c -o ex04
```

<details><summary>Predicción</summary>

```
Buffer 80: "cpu=72.5°C gpu=68.3°C ram=85.0% disk=62.1% net=45.2Mbps"
  Used: 57/80 bytes, truncated=NO

Buffer 40: "cpu=72.5°C gpu=68.3°C ram=85.0%"
  Used: 40/40 bytes, truncated=YES at entry 3 ("disk")
```

El builder verifica `pos + ret >= sizeof(buf)` después de cada
`snprintf`. Cuando se detecta truncación, el loop se detiene y reporta
qué entrada causó el desbordamiento. El buffer siempre queda terminado
en `'\0'`.

</details>

---

### Ejercicio 5 — sscanf: parsear fecha y hora

```c
// Parsear strings con formato de log:
//   "2026-03-18 14:30:45 ERROR Connection timeout"
//
// Usar sscanf con formato:
//   "%d-%d-%d %d:%d:%d %15s %127[^\n]"
//
// Extraer: año, mes, día, hora, minuto, segundo, nivel, mensaje.
// Verificar que sscanf retorna 8 campos.
//
// Probar con 5 líneas de log, incluyendo una mal formateada.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex05.c -o ex05
```

<details><summary>Predicción</summary>

```
Line 1: 2026-03-18 14:30:45 [ERROR] Connection timeout
Line 2: 2026-03-18 14:31:02 [INFO]  Server restarted
Line 3: PARSE ERROR — got 2 fields (expected 8)
Line 4: 2026-03-18 14:32:10 [WARN]  High latency detected
```

`%[^\n]` captura el mensaje completo incluyendo espacios. La línea
mal formateada (sin el formato fecha-hora-nivel) falla con `fields < 8`.
`sscanf` no modifica las variables de los campos que no pudo leer.

</details>

---

### Ejercicio 6 — fgets + sscanf: parsear CSV

```c
// Crear un archivo CSV:
//   Alice,30,95.5
//   Bob,25,87.2
//   INVALID
//   Carol,35,91.8
//   ,40,50.0
//
// Usar fgets + sscanf con formato "%63[^,],%d,%lf".
// Para cada línea:
//   - Si 3 campos: imprimir en formato tabla
//   - Si < 3 campos: imprimir error con la línea original
//
// Contar: exitosas vs fallidas.
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex06.c -o ex06
```

<details><summary>Predicción</summary>

```
NAME       AGE   SCORE
----       ---   -----
Alice       30    95.5   OK
Bob         25    87.2   OK
PARSE ERROR (line 3: "INVALID")
Carol       35    91.8   OK
PARSE ERROR (line 5: ",40,50.0")

Result: 3 OK, 2 errors
```

"INVALID" no tiene comas → `%[^,]` lee todo como un campo, no encuentra
coma para el `%d` → falla. ",40,50.0" tiene coma al inicio → `%[^,]` lee
string vacío (0 chars) lo cual puede fallar o dar resultado inesperado según
la implementación. `fgets` ya consumió la línea completa, así que no hay
residuo en el stream.

</details>

---

### Ejercicio 7 — Especificadores de formato avanzados

```c
// Imprimir la siguiente tabla usando printf con alineación:
//
//   ID  | NAME       | SCORE  | GRADE
//   ----|------------|--------|------
//     1 | Alice      |  95.50 | A
//     2 | Bob        |  87.20 | B+
//   100 | Charlie    |  91.80 | A-
//
// Usar: %4d, %-10s, %6.2f para las columnas.
// Imprimir la misma tabla a un archivo con fprintf.
// Imprimir el valor 255 en decimal (%d), hex (%x, %X),
// octal (%o), y como char (%c).
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex07.c -o ex07
```

<details><summary>Predicción</summary>

```
  ID | NAME       | SCORE  | GRADE
  ---|------------|--------|------
    1 | Alice      |  95.50 | A
    2 | Bob        |  87.20 | B+
  100 | Charlie    |  91.80 | A-

255: decimal=255, hex=ff, HEX=FF, octal=377, char=ÿ
```

`%4d` alinea el ID a la derecha en 4 columnas. `%-10s` alinea el nombre
a la izquierda en 10. `%6.2f` alinea el score a la derecha en 6 con 2
decimales. 255 en octal es 377 (3×64 + 7×8 + 7). Como char (ASCII
extendido) es `ÿ` (Latin-1).

</details>

---

### Ejercicio 8 — Vulnerabilidad de format string

```c
// Demostrar por qué printf(user_input) es peligroso:
//
// 1. Crear un string: const char *safe = "Hello %s, score: %d";
//    Llamar printf(safe, "Alice", 95); — funciona bien.
//
// 2. Crear un string que simule input malicioso:
//    const char *evil = "Hello %x %x %x %x";
//    Llamar printf(evil); — ¿qué imprime? (lee del stack)
//
// 3. Mostrar la forma segura: printf("%s", user_string);
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wformat-security ex08.c -o ex08
// Observar el warning del compilador para el caso 2.
//
// NOTA: este ejercicio es educativo. No usar estas técnicas
// fuera de contextos de aprendizaje o pentesting autorizado.
```

<details><summary>Predicción</summary>

```
Safe: Hello Alice, score: 95

Dangerous: Hello 7ffe3a20 4005a0 2 deadbeef
(valores del stack — impredecibles, cambian cada ejecución)

Correct: Hello %x %x %x %x
(imprime literalmente los especificadores)

Compiler warning:
  warning: format not a string literal and no format arguments [-Wformat-security]
```

`printf(evil)` sin argumentos interpreta `%x` como "leer un int del stack
y mostrar en hex". Los valores son basura del stack — en un ataque real,
`%n` escribiría en memoria. La forma segura `printf("%s", evil)` trata
el string como dato, no como formato.

</details>

---

### Ejercicio 9 — Wrapper con vfprintf

```c
// Implementar:
//   void log_msg(FILE *out, const char *level,
//                const char *fmt, ...)
//   __attribute__((format(printf, 3, 4)));
//
// Que escriba: [LEVEL] mensaje\n
// Usar va_start, vfprintf, va_end internamente.
//
// Probar:
//   log_msg(stderr, "ERROR", "File %s not found", "data.txt");
//   log_msg(stdout, "INFO", "Loaded %d records in %.2f ms", 100, 3.14);
//   log_msg(logfile, "DEBUG", "Pointer value: %p", ptr);
//
// Compilar con tipos incorrectos para ver que el atributo format
// detecta errores: log_msg(stderr, "ERROR", "Value: %d", "wrong_type");
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex09.c -o ex09
```

<details><summary>Predicción</summary>

```
[ERROR] File data.txt not found
[INFO] Loaded 100 records in 3.14 ms
[DEBUG] Pointer value: 0x7ffd12345678

With wrong type:
  warning: format '%d' expects argument of type 'int',
  but argument 4 has type 'char *' [-Wformat=]
```

El atributo `format(printf, 3, 4)` le dice a GCC que el parámetro 3 es
el format string y los variádicos empiezan en el 4. Con esto, GCC aplica
la misma verificación de tipos que hace con `printf` nativo.

</details>

---

### Ejercicio 10 — sscanf: validar IP con semántica

```c
// Implementar:
//   int parse_ipv4(const char *str, int octets[4])
// que:
// 1. Use sscanf con "%d.%d.%d.%d%c" (el %c extra detecta basura)
// 2. Verifique que sscanf retorna exactamente 4 (no 5)
// 3. Verifique que cada octeto está en rango [0, 255]
// 4. Retorne 0 si válida, -1 si inválida
//
// Probar con:
//   "192.168.1.100"     → válida
//   "10.0.0.1"          → válida
//   "256.1.2.3"         → inválida (rango)
//   "not.an.ip"         → inválida (sintaxis)
//   "192.168.1.1.extra" → inválida (chars extra detectados por %c)
//   "172.16"            → inválida (campos insuficientes)
//
// Compilar: gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
```

<details><summary>Predicción</summary>

```
"192.168.1.100"      → VALID  (192.168.1.100)
"10.0.0.1"           → VALID  (10.0.0.1)
"256.1.2.3"          → INVALID (octet 0 out of range: 256)
"not.an.ip"          → INVALID (sscanf returned 0, expected 4)
"192.168.1.1.extra"  → INVALID (trailing characters detected)
"172.16"             → INVALID (sscanf returned 2, expected 4)
```

El truco del `%c` al final: si `sscanf` retorna 5 (leyó el char extra),
hay basura después de la IP. Si retorna exactamente 4, la IP consumió
todo el string limpiamente. Este patrón de "sentinel char" es útil para
detectar datos extra que `sscanf` normalmente ignora.

</details>
