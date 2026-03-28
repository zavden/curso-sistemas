# T03 — Buffer overflows

## Erratas detectadas en el material base

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 89-90 | Dice que `SIZE_MAX * sizeof(int)` produce "un valor muy pequeño". En realidad, `(2^64-1) * 4 mod 2^64 = 2^64 - 4`, que es casi `SIZE_MAX` | Un ejemplo correcto sería `count = (SIZE_MAX / sizeof(int)) + 2`, donde `count * 4` sí produce un valor pequeño (e.g. 4) |
| `labs/README.md` | 171-172 | Salida de `snprintf` dice `"Name: Bartholomew Lon"` con `48 chars needed` | La cadena formateada tiene 56 chars, no 48. El buffer de 32 bytes trunca a `"Name: Bartholomew Longname, Age"` (31 chars + `'\0'`), y snprintf retorna 56 |

---

## 1 — Qué es un buffer overflow

Un buffer overflow ocurre cuando un programa escribe más datos de los que caben en un buffer, sobrescribiendo memoria adyacente:

```c
char buf[8];
strcpy(buf, "this string is much longer than 8 bytes");
// Escribe 41 bytes en un buffer de 8
// Sobrescribe todo lo que esté después de buf en el stack
```

En el stack, las variables locales están junto a datos críticos:

```
Dirección alta
┌──────────────────┐
│ return address    │ ← dirección de retorno de la función
├──────────────────┤
│ saved RBP        │ ← frame pointer guardado (8 bytes en x86-64)
├──────────────────┤
│ buf[7]           │
│ ...              │
│ buf[0]           │ ← strcpy empieza aquí y sigue escribiendo →
└──────────────────┘
Dirección baja
```

`strcpy` escribe más allá de `buf`, sobreescribe el `saved RBP`, luego la `return address`. Cuando la función retorna, el CPU salta a una dirección corrupta → crash, o peor: ejecución de código arbitrario.

La parte insidiosa: el overflow puede ser **silencioso** — el programa sigue ejecutándose con datos corrompidos, produciendo resultados incorrectos sin avisar.

---

## 2 — Cómo ocurren los buffer overflows

### 2.1 — Funciones de string sin límite

```c
// gets — la función más peligrosa (eliminada en C11):
char buf[100];
gets(buf);           // lee stdin sin límite

// strcpy sin verificar longitud:
char dest[10];
strcpy(dest, user_input);      // si > 9 chars → overflow

// strcat sin verificar espacio:
char buf[20] = "hello";
strcat(buf, user_input);       // si "hello" + input > 19 → overflow

// sprintf sin límite:
char buf[50];
sprintf(buf, "User: %s, Data: %s", name, data);  // si muy largo → overflow
```

### 2.2 — Errores de cálculo de tamaño

```c
// Off-by-one — olvidar el terminador:
char buf[5];
strncpy(buf, "hello", 5);   // copia 5 chars, NO hay espacio para '\0'

// Calcular mal el espacio disponible con strncat:
char buf[100] = "prefix: ";
size_t remaining = sizeof(buf) - strlen(buf);  // 92
strncat(buf, data, remaining);     // ERROR: strncat agrega '\0' extra
strncat(buf, data, remaining - 1); // CORRECTO
```

### 2.3 — Integer overflow que causa buffer overflow

```c
// Si el tamaño se calcula con overflow aritmético:
size_t count = get_user_count();       // valor controlado por el usuario
size_t size = count * sizeof(int);     // si count es grande → overflow

// Ejemplo: count = (SIZE_MAX / 4) + 2  en 64-bit
// size = count * 4 = SIZE_MAX + 8 - 4 = wraps a 4
int *arr = malloc(size);               // aloca solo 4 bytes
for (size_t i = 0; i < count; i++) {
    arr[i] = 0;                        // escribe MÁS ALLÁ de lo alocado
}
```

### 2.4 — Stack vs heap buffer overflow

```c
// Stack overflow — buffer en el stack:
void vulnerable(const char *input) {
    char buf[64];           // en el stack
    strcpy(buf, input);     // overflow sobrescribe return address
}

// Heap overflow — buffer en el heap:
void vulnerable2(const char *input) {
    char *buf = malloc(64); // en el heap
    strcpy(buf, input);     // overflow corrompe metadata del heap
    free(buf);              // o sobrescribe datos de otros bloques
}
```

**Stack overflow**: puede sobrescribir la dirección de retorno → ejecución de código. Es el exploit más clásico.

**Heap overflow**: corrompe la metadata interna de `malloc`/`free` o datos de otros bloques. Más difícil de explotar pero igualmente peligroso.

---

## 3 — Exploits clásicos

### Stack smashing

Un atacante construye un input que sobrescribe la dirección de retorno con la dirección de su código:

```c
void vulnerable(const char *input) {
    char buf[64];
    strcpy(buf, input);
}

// Input malicioso:
// [64 bytes de relleno][8 bytes de RBP falso][dirección de código malicioso]
//
// Al retornar, el CPU salta a la dirección del atacante.
```

Históricamente, el shellcode (código del atacante) se colocaba en el mismo buffer. Las mitigaciones modernas (NX bit, ASLR) hacen esto mucho más difícil.

### El Morris Worm (1988)

El primer worm de Internet explotó un buffer overflow en `fingerd`:

```c
// El daemon usaba gets() para leer de la red:
char buf[512];
gets(buf);    // sin límite — input de la red

// El worm enviaba >512 bytes con shellcode
// y una dirección de retorno apuntando al buffer
// → ejecutaba código del atacante en el servidor
```

Este incidente motivó la creación del CERT y la eventual eliminación de `gets()` en C11.

---

## 4 — Mitigaciones del compilador y SO

### 4.1 — Stack canary (stack protector)

El compilador inserta un valor aleatorio ("canario") entre el buffer y la dirección de retorno. Al retornar de la función, verifica que el canario no fue modificado:

```
┌──────────────────┐
│ return address    │
├──────────────────┤
│ canary value      │ ← valor aleatorio, verificado al retornar
├──────────────────┤
│ buf[63]           │
│ ...               │
│ buf[0]            │ ← si strcpy desborda, corrompe el canario
└──────────────────┘
```

Si el canario cambió → `abort()` con el mensaje `*** stack smashing detected ***`.

```bash
gcc -fstack-protector         # protege funciones con buffers > 8 bytes
gcc -fstack-protector-strong  # protege más funciones (recomendado)
gcc -fstack-protector-all     # protege TODAS las funciones
```

**Limitación**: detecta **después** del overflow, al retornar. Si el atacante no toca el canario (overflow preciso), no se detecta.

### 4.2 — ASLR (Address Space Layout Randomization)

El SO aleatoriza las direcciones de stack, heap, bibliotecas compartidas y ejecutable (con PIE). El atacante no puede predecir dónde está el código o los datos.

```bash
# Verificar ASLR:
cat /proc/sys/kernel/randomize_va_space
# 2 = full ASLR (predeterminado en Linux moderno)

# Compilar con Position Independent Executable:
gcc -pie -fPIE prog.c
```

### 4.3 — NX bit (No-Execute / DEP)

Las páginas de memoria se marcan como ejecutables o no:
- **Stack**: NO ejecutable (NX)
- **Heap**: NO ejecutable
- **Código (.text)**: ejecutable

Si un atacante pone shellcode en el stack y salta ahí → segfault. Habilitado por defecto en Linux moderno.

### 4.4 — FORTIFY_SOURCE

GCC reemplaza funciones peligrosas por versiones que verifican el tamaño:

```c
// Con -D_FORTIFY_SOURCE=2 -O2:
char buf[10];
strcpy(buf, "this is too long");
// FORTIFY aborta DURANTE la copia:
// *** buffer overflow detected ***: terminated
```

`FORTIFY` reemplaza `strcpy` por `__strcpy_chk` que verifica `__builtin_object_size(dest)` contra la longitud del origen. Requiere `-O1` o superior (necesita inlining). Funciona con `memcpy`, `strcpy`, `strcat`, `sprintf`, etc.

**Ventaja sobre el canario**: detecta **durante** la copia, no al retornar.

---

## 5 — Detección con herramientas

### AddressSanitizer (ASan)

La herramienta más efectiva para detectar overflows. Detecta tanto stack como heap overflows:

```bash
gcc -fsanitize=address -g prog.c -o prog
./prog
```

```
==PID==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x...
WRITE of size 17 at 0x...
    #0 0x... in main prog.c:15
...
[32, 40) 'buf' <== Memory access at offset 40 overflows this variable
```

ASan reporta: tipo de error, tamaño de la escritura, ubicación exacta en el código fuente, variable afectada, y stack trace completo.

**Costo**: ~2x más lento, ~2-3x más memoria. Para desarrollo, no para producción.

### Valgrind (solo heap)

```bash
valgrind ./prog
```

Detecta overflows en el **heap**, no en el stack. Reporta escrituras inválidas con ubicación y tamaño del bloque alocado. Más lento que ASan (~20x), pero no requiere recompilar.

### Comparación de herramientas

| Herramienta | Stack | Heap | Cuándo detecta | Costo |
|-------------|:---:|:---:|----------------|-------|
| Stack canary | Sí | No | Al retornar | ~0% |
| FORTIFY_SOURCE | Sí | Sí | Durante la copia | ~0% |
| ASan | Sí | Sí | Durante la copia + reporte detallado | ~2x |
| Valgrind | No | Sí | Runtime (sin recompilar) | ~20x |

---

## 6 — Prevención: patrones seguros

### Regla 1 — Siempre limitar la cantidad de datos

```c
// MAL:                                    // BIEN:
gets(buf);                                 fgets(buf, sizeof(buf), stdin);
strcpy(dest, src);                         snprintf(dest, sizeof(dest), "%s", src);
sprintf(buf, "%s", data);                  snprintf(buf, sizeof(buf), "%s", data);
scanf("%s", buf);                          scanf("%99s", buf);  // o fgets
```

### Regla 2 — Verificar longitudes antes de copiar

```c
size_t src_len = strlen(src);
if (src_len >= sizeof(dest)) {
    fprintf(stderr, "string too long\n");
    return -1;
}
strcpy(dest, src);  // ahora es seguro
```

### Regla 3 — Usar snprintf para todo

```c
char buf[100];

// Formatear:
snprintf(buf, sizeof(buf), "User: %s", name);

// Concatenar (con offset):
int pos = 0;
pos += snprintf(buf + pos, sizeof(buf) - pos, "Hello ");
pos += snprintf(buf + pos, sizeof(buf) - pos, "%s!", name);

// Detectar truncación:
int n = snprintf(buf, sizeof(buf), "%s", data);
if (n >= (int)sizeof(buf)) { /* truncado */ }
```

### Regla 4 — Compilar con protecciones

```bash
# Desarrollo:
gcc -Wall -Wextra -fsanitize=address,undefined -g prog.c

# Producción:
gcc -Wall -Wextra \
    -fstack-protector-strong \
    -D_FORTIFY_SOURCE=2 \
    -O2 \
    -pie -fPIE \
    -z noexecstack \
    -z relro -z now \
    prog.c -o prog
```

`-z relro -z now`: Full RELRO — hace que la GOT (Global Offset Table) sea de solo lectura después de la carga, previniendo ataques que sobreescriben entradas de la GOT.

---

## 7 — Funciones a evitar vs recomendadas

| Evitar | Usar en su lugar | Razón |
|--------|------------------|-------|
| `gets()` | `fgets()` | Sin límite — eliminada en C11 |
| `strcpy()` | `snprintf()` o `strlcpy()` | No verifica tamaño del destino |
| `strcat()` | `snprintf()` con offset | No verifica espacio disponible |
| `sprintf()` | `snprintf()` | No verifica tamaño del buffer |
| `scanf("%s")` | `scanf("%99s")` o `fgets()` | Sin límite por defecto |
| `strncpy()` | `snprintf()` | No garantiza terminador `'\0'` |

---

## 8 — Comparación con Rust

| Aspecto | C | Rust |
|---------|---|------|
| Bounds checking | No hay — responsabilidad del programador | Automático en runtime (panic en OOB) |
| Stack overflow | UB silencioso, explotable | No hay buffers de tamaño fijo sin bounds checking |
| Heap overflow | UB, corrompe metadata de malloc | `Vec::push` gestiona capacidad automáticamente |
| Format strings | `sprintf` puede desbordar | `format!()` retorna `String` con tamaño dinámico |
| Mitigación | Herramientas externas (ASan, canary) | Garantías del compilador en safe Rust |
| Unsafe | Todo el código es "unsafe" | Solo bloques `unsafe` pueden hacer acceso directo |

```rust
// En Rust, los buffer overflows son imposibles en safe code:
let mut buf = vec![0u8; 8];
buf[8] = 0xFF;  // panic! — bounds checking en runtime

// Strings se manejan con tipos que gestionan su propia memoria:
let greeting = format!("Hello {}, age {}", name, age);
// No hay buffer fijo que desbordar — String crece según necesita

// Para buffers fijos, slices verifican bounds:
let mut buf = [0u8; 8];
let input = b"too long for this buffer";
buf.copy_from_slice(&input[..8]);  // OK: exactamente 8 bytes
buf.copy_from_slice(input);        // panic! — tamaños no coinciden
```

En Rust, la única forma de provocar un buffer overflow es con código `unsafe` (punteros crudos, `ptr::write`, etc.). En código `safe`, el compilador y el runtime lo impiden.

---

## Ejercicios

### Ejercicio 1 — Corrupción silenciosa de variables

```c
// ¿Qué imprime este programa compilado con -fno-stack-protector?
// ¿Las variables adyacentes se corrompen?
#include <stdio.h>
#include <string.h>

int main(void) {
    int after = 0xDEADBEEF;
    char buf[8];
    int before = 0x41414141;  // 'AAAA'

    printf("before = 0x%X\n", before);
    printf("after  = 0x%X\n", after);

    strcpy(buf, "AAAAAAAABBBB");  // 12 chars + '\0' = 13 bytes

    printf("buf    = %s\n", buf);
    printf("before = 0x%X\n", before);
    printf("after  = 0x%X\n", after);
    return 0;
}
```

<details><summary>Predicción</summary>

El comportamiento exacto es **indefinido** — depende de cómo el compilador ordene las variables en el stack. Pero el escenario probable en x86-64 con GCC y `-fno-stack-protector`:

```
before = 0x41414141
after  = 0xDEADBEEF
buf    = AAAAAAAABBBB
before = 0x42424242    ← "BBBB" en ASCII (0x42 = 'B')
after  = 0xDEADBEEF   ← probablemente intacto (overflow no llega)
```

El overflow de 13 bytes en un buffer de 8 escribe 5 bytes más allá de `buf`. Si `before` está adyacente a `buf` en el stack, sus 4 bytes se sobrescriben con `"BBBB"` (0x42424242). El byte `'\0'` del terminador cae un byte más adelante.

Lo alarmante: **no hay crash**. El programa sigue ejecutándose con datos corruptos. Sin el stack protector, esta corrupción es completamente silenciosa.

</details>

### Ejercicio 2 — Stack protector vs ASan

```c
// Compila este programa de tres formas y predice el resultado de cada una:
// 1. gcc -fno-stack-protector prog.c
// 2. gcc -fstack-protector-strong prog.c
// 3. gcc -fsanitize=address -g prog.c
#include <stdio.h>
#include <string.h>

void overflow(void) {
    char buf[16];
    strcpy(buf, "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA");  // 34 'A's
    printf("buf: %s\n", buf);
}

int main(void) {
    overflow();
    printf("Returned from overflow()\n");
    return 0;
}
```

<details><summary>Predicción</summary>

**Compilación 1** (`-fno-stack-protector`):
- El overflow ocurre silenciosamente
- `printf` imprime las 34 'A's (posiblemente con basura extra)
- Al retornar de `overflow()`, la dirección de retorno está corrompida
- Probable: **segfault** (crash) en `overflow()`'s `ret` instruction
- El mensaje "Returned from overflow()" **no se imprime**

**Compilación 2** (`-fstack-protector-strong`):
```
buf: AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA
*** stack smashing detected ***: terminated
Aborted
```
- El overflow ocurre, `printf` funciona
- Al retornar, el canario fue modificado → `abort()`
- "Returned from overflow()" **no se imprime**

**Compilación 3** (`-fsanitize=address`):
```
==PID==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x...
WRITE of size 35 at 0x...
    #0 0x... in overflow prog.c:5
```
- ASan detecta el overflow **durante** el `strcpy`, antes del `printf`
- El programa aborta inmediatamente con un reporte detallado
- Ni `buf:` ni "Returned" se imprimen

ASan es el más informativo: reporta la línea exacta, el tamaño de la escritura, y la variable afectada.

</details>

### Ejercicio 3 — Identificar vulnerabilidades

```c
// Este código tiene 4 vulnerabilidades. Identifícalas y explica
// por qué cada una puede causar un buffer overflow.
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void process_user(void) {
    char name[20];
    char greeting[40];
    char *dynamic;

    printf("Name: ");
    gets(name);                                          // [1]

    sprintf(greeting, "Welcome, %s! Have a great day.", name);  // [2]
    printf("%s\n", greeting);

    size_t len = strlen(name);
    dynamic = malloc(len);                               // [3]
    strcpy(dynamic, name);

    char combined[30];
    strcpy(combined, greeting);                          // [4]
    strcat(combined, " -- ");
    strcat(combined, name);

    free(dynamic);
}
```

<details><summary>Predicción</summary>

**Vulnerabilidad [1]** — `gets(name)`: lee stdin sin límite alguno. Si el usuario escribe ≥20 chars, desborda `name`. `gets` fue eliminada en C11 por ser inherentemente insegura. **Corrección**: `fgets(name, sizeof(name), stdin)`.

**Vulnerabilidad [2]** — `sprintf(greeting, ...)`: el formato `"Welcome, %s! Have a great day."` agrega 30 chars fijos + `strlen(name)`. Si `name` tiene >10 chars, el total supera 40 bytes y desborda `greeting`. **Corrección**: `snprintf(greeting, sizeof(greeting), ...)`.

**Vulnerabilidad [3]** — `malloc(len)` sin +1: `strlen` no cuenta el `'\0'`, pero `strcpy` lo copia. El buffer es 1 byte corto → off-by-one heap overflow. **Corrección**: `malloc(len + 1)`.

**Vulnerabilidad [4]** — `strcpy` + `strcat` + `strcat` en `combined[30]`: `greeting` puede tener hasta 40 chars (ya desborda combined de 30), más " -- " (4 chars) y `name` (hasta 20 chars). Total potencial: ~64 chars en 30 bytes. **Corrección**: usar `snprintf` con `sizeof(combined)`.

</details>

### Ejercicio 4 — FORTIFY_SOURCE en acción

```c
// Compila con:  gcc -O2 -D_FORTIFY_SOURCE=2 prog.c
// Y sin:        gcc -O2 prog.c
// ¿Qué diferencia hay en el comportamiento?
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[8];
    char *src = "Hello, buffer overflow world!";

    printf("Copying %zu bytes into %zu-byte buffer...\n",
           strlen(src) + 1, sizeof(buf));

    strcpy(buf, src);

    printf("buf: %s\n", buf);
    return 0;
}
```

<details><summary>Predicción</summary>

**Sin FORTIFY** (`gcc -O2`):
```
Copying 29 bytes into 8-byte buffer...
buf: Hello, buffer overflow world!
```
(O un crash, según qué se sobrescriba. El overflow ocurre pero GCC no lo intercepta en runtime.)

**Con FORTIFY** (`gcc -O2 -D_FORTIFY_SOURCE=2`):
```
Copying 29 bytes into 8-byte buffer...
*** buffer overflow detected ***: terminated
Aborted
```

FORTIFY reemplaza `strcpy` por `__strcpy_chk`, que compara `__builtin_object_size(buf)` (= 8) contra `strlen(src) + 1` (= 29). Como 29 > 8, aborta **durante** la copia, antes de que `printf("buf: ...")` se ejecute.

Nota: FORTIFY requiere `-O1` o superior porque necesita inlining para que `__builtin_object_size` funcione. Con `-O0`, FORTIFY no puede determinar el tamaño del buffer y no protege.

</details>

### Ejercicio 5 — Off-by-one

```c
// ¿Este código tiene un bug? ¿Cuál es?
#include <stdio.h>
#include <string.h>

void safe_copy(char *dest, size_t dest_size, const char *src) {
    strncpy(dest, src, dest_size);
    dest[dest_size] = '\0';  // asegurar terminador
}

int main(void) {
    char buf[10];
    safe_copy(buf, sizeof(buf), "hello world");
    printf("buf: \"%s\"\n", buf);
    return 0;
}
```

<details><summary>Predicción</summary>

**Sí, hay un off-by-one**. La línea `dest[dest_size] = '\0'` escribe en `dest[10]`, que es **1 byte más allá del final del array** (los índices válidos son 0-9).

El error está en el argumento de `strncpy` y el índice del terminador:

```c
// INCORRECTO:
strncpy(dest, src, dest_size);      // copia 10 bytes: "hello worl" (sin '\0')
dest[dest_size] = '\0';             // dest[10] = '\0' ← OVERFLOW (1 byte)

// CORRECTO:
strncpy(dest, src, dest_size - 1);  // copia 9 bytes: "hello wor"
dest[dest_size - 1] = '\0';         // dest[9] = '\0' ← dentro del array
```

El off-by-one es uno de los errores más comunes en C. Escribir un solo byte fuera del buffer es comportamiento indefinido — puede corromper datos adyacentes, disparar el stack canary, o parecer funcionar hasta que algo cambia.

</details>

### Ejercicio 6 — Integer overflow → buffer overflow

```c
// ¿Qué pasa si count es muy grande? ¿Dónde está la vulnerabilidad?
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

void process_items(size_t count) {
    size_t total_size = count * sizeof(uint32_t);

    printf("count = %zu, total_size = %zu\n", count, total_size);

    uint32_t *items = malloc(total_size);
    if (!items) {
        printf("malloc failed\n");
        return;
    }

    // Simular escritura de 'count' elementos
    memset(items, 0x41, count * sizeof(uint32_t));

    printf("items[0] = 0x%X\n", items[0]);
    free(items);
}

int main(void) {
    process_items(10);                          // normal
    process_items(SIZE_MAX / sizeof(uint32_t) + 2);  // overflow
    return 0;
}
```

<details><summary>Predicción</summary>

**Primera llamada** (`count = 10`):
```
count = 10, total_size = 40
items[0] = 0x41414141
```
Funciona correctamente: aloca 40 bytes, escribe 40 bytes.

**Segunda llamada** (`count = SIZE_MAX/4 + 2`):
```
count = 4611686018427387905, total_size = 4
```

`total_size = count * 4` produce un overflow aritmético que wraps a un valor pequeño (4 bytes en 64-bit). `malloc(4)` tiene éxito y aloca solo 4 bytes. Luego `memset` intenta escribir `count * 4` bytes (que también tiene overflow), pero el efecto real depende de la implementación.

La vulnerabilidad: el atacante controla `count` → controla el tamaño de la alocación → puede forzar un heap overflow masivo.

**Corrección**: verificar overflow antes de multiplicar:

```c
if (count > SIZE_MAX / sizeof(uint32_t)) {
    printf("overflow detected\n");
    return;
}
```

</details>

### Ejercicio 7 — Reescribir código vulnerable

```c
// Reescribe esta función para que sea segura.
// Identifica cada vulnerabilidad y aplica el patrón correcto.
#include <stdio.h>
#include <string.h>

void create_log_entry(const char *user, const char *action, int code) {
    char entry[80];
    char timestamp[20] = "2026-03-26 14:30";

    sprintf(entry, "[%s] User '%s' performed '%s' (code: %d)",
            timestamp, user, action, code);

    char filename[30];
    strcpy(filename, "/var/log/");
    strcat(filename, user);
    strcat(filename, ".log");

    printf("Would write to: %s\n", filename);
    printf("Entry: %s\n", entry);
}
```

<details><summary>Predicción</summary>

**Vulnerabilidades**:
1. `sprintf(entry, ...)`: si `user` + `action` son largos, desborda `entry[80]`
2. `strcpy(filename, "/var/log/")` + `strcat(filename, user)` + `strcat(filename, ".log")`: si `user` es largo, desborda `filename[30]`

**Versión segura**:

```c
void create_log_entry(const char *user, const char *action, int code) {
    char entry[80];
    char timestamp[20] = "2026-03-26 14:30";

    int n = snprintf(entry, sizeof(entry),
                     "[%s] User '%s' performed '%s' (code: %d)",
                     timestamp, user, action, code);
    if (n >= (int)sizeof(entry)) {
        fprintf(stderr, "WARNING: log entry truncated\n");
    }

    char filename[30];
    n = snprintf(filename, sizeof(filename),
                 "/var/log/%s.log", user);
    if (n >= (int)sizeof(filename)) {
        fprintf(stderr, "WARNING: filename truncated\n");
    }

    printf("Would write to: %s\n", filename);
    printf("Entry: %s\n", entry);
}
```

`snprintf` reemplaza tanto `sprintf` como la cadena `strcpy`+`strcat`+`strcat`. Es más seguro, más conciso, y permite detectar truncación.

</details>

### Ejercicio 8 — fgets y el newline

```c
// ¿Qué imprime si el usuario escribe "hello"? ¿Y si escribe 25 caracteres?
#include <stdio.h>
#include <string.h>

int main(void) {
    char buf[10];

    printf("Input: ");
    if (fgets(buf, sizeof(buf), stdin) == NULL) {
        return 1;
    }

    printf("Raw: \"%s\"\n", buf);
    printf("strlen: %zu\n", strlen(buf));

    // Remove newline
    buf[strcspn(buf, "\n")] = '\0';
    printf("Clean: \"%s\"\n", buf);
    printf("strlen: %zu\n", strlen(buf));

    return 0;
}
```

<details><summary>Predicción</summary>

**Si el usuario escribe "hello" (5 chars + Enter)**:
```
Raw: "hello\n"        ← fgets incluye el '\n'
strlen: 6             ← 5 chars + '\n'
Clean: "hello"        ← strcspn encontró '\n' y lo reemplazó
strlen: 5
```

`fgets` lee hasta `'\n'` o hasta `sizeof(buf)-1` chars, lo que ocurra primero. Con "hello\n" (6 chars), cabe en el buffer de 10.

**Si el usuario escribe 25 caracteres**:
```
Raw: "abcdefghi"      ← solo 9 chars (sizeof(buf) - 1)
strlen: 9             ← NO hay '\n' — fgets paró por tamaño
Clean: "abcdefghi"    ← strcspn no encontró '\n', no cambia nada
strlen: 9
```

`fgets` lee solo 9 chars y agrega `'\0'`. Los 16 chars restantes quedan en el buffer de stdin para la siguiente lectura. No hay `'\n'` en `buf` porque fgets paró antes de llegar a él.

El patrón `buf[strcspn(buf, "\n")] = '\0'` es seguro en ambos casos: si hay `'\n'` lo elimina, si no hay `'\n'` escribe `'\0'` sobre el `'\0'` existente (no-op).

</details>

### Ejercicio 9 — ASLR en acción

```c
// Ejecuta este programa 3 veces. ¿Las direcciones cambian?
#include <stdio.h>
#include <stdlib.h>

int global_var = 42;

int main(void) {
    int stack_var = 10;
    int *heap_var = malloc(sizeof(int));
    *heap_var = 20;

    printf("Code   (main):       %p\n", (void *)main);
    printf("Global (global_var): %p\n", (void *)&global_var);
    printf("Stack  (stack_var):  %p\n", (void *)&stack_var);
    printf("Heap   (heap_var):   %p\n", (void *)heap_var);

    free(heap_var);
    return 0;
}
```

<details><summary>Predicción</summary>

**Compilado sin PIE** (`gcc -no-pie`):
```
# Ejecución 1:                    Ejecución 2:
Code   (main):       0x401136     0x401136       ← IGUAL
Global (global_var): 0x404030     0x404030       ← IGUAL
Stack  (stack_var):  0x7fff...a   0x7fff...b     ← DIFERENTE
Heap   (heap_var):   0x5555...x   0x5555...y     ← DIFERENTE
```

Sin PIE, las secciones `.text` y `.data` del ejecutable tienen direcciones fijas. ASLR solo afecta stack, heap y bibliotecas compartidas.

**Compilado con PIE** (`gcc -pie -fPIE`, predeterminado en distros modernas):
```
# Las 4 direcciones cambian en cada ejecución
```

Con PIE, el ejecutable se carga en una dirección aleatoria → **todas** las direcciones cambian. Esto dificulta enormemente los exploits que dependen de conocer direcciones.

ASLR no es una protección perfecta (puede haber information leaks), pero eleva significativamente la dificultad de explotación.

</details>

### Ejercicio 10 — Patrón de copia segura completo

```c
// ¿Qué imprime este programa? ¿Todas las operaciones son seguras?
#include <stdio.h>
#include <string.h>

typedef struct {
    char name[16];
    char role[16];
    int active;
} User;

int init_user(User *u, const char *name, const char *role) {
    int truncated = 0;
    int n;

    n = snprintf(u->name, sizeof(u->name), "%s", name);
    if (n >= (int)sizeof(u->name)) {
        fprintf(stderr, "  WARNING: name truncated (%d -> %zu)\n",
                n, sizeof(u->name) - 1);
        truncated = 1;
    }

    n = snprintf(u->role, sizeof(u->role), "%s", role);
    if (n >= (int)sizeof(u->role)) {
        fprintf(stderr, "  WARNING: role truncated (%d -> %zu)\n",
                n, sizeof(u->role) - 1);
        truncated = 1;
    }

    u->active = 1;
    return truncated;
}

int main(void) {
    User u1, u2;

    printf("User 1:\n");
    init_user(&u1, "Alice", "admin");
    printf("  name='%s' role='%s' active=%d\n",
           u1.name, u1.role, u1.active);

    printf("User 2:\n");
    init_user(&u2, "Bartholomew Johnson", "senior_administrator");
    printf("  name='%s' role='%s' active=%d\n",
           u2.name, u2.role, u2.active);

    return 0;
}
```

<details><summary>Predicción</summary>

```
User 1:
  name='Alice' role='admin' active=1
User 2:
  WARNING: name truncated (19 -> 15)
  WARNING: role truncated (20 -> 15)
  name='Bartholomew Joh' role='senior_administ' active=1
```

- **User 1**: "Alice" (5 chars) y "admin" (5 chars) caben sin problemas en 16 bytes. Sin truncación.

- **User 2**: "Bartholomew Johnson" tiene 19 chars (>15 disponibles), se trunca a 15 + `'\0'` = "Bartholomew Joh". "senior_administrator" tiene 20 chars (>15 disponibles), se trunca a 15 + `'\0'` = "senior_administ".

Todas las operaciones son **seguras**:
- `snprintf` nunca desborda los campos de 16 bytes
- Siempre agrega `'\0'`
- `u->active` **nunca** se corrompe, a diferencia de lo que pasaría con `strcpy`
- La función retorna `1` si hubo truncación, permitiendo al llamador decidir qué hacer

Este es el patrón correcto para inicializar structs con strings de longitud variable: `snprintf` + verificación de truncación + campos separados que no se interfieren entre sí.

</details>
