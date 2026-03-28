# T03 — `_Noreturn` y `_Static_assert`

## Erratas detectadas en el material base

| Archivo | Línea | Error | Corrección |
|---------|-------|-------|------------|
| `README.md` | 71–73 | Dice que `longjmp` "no se marca como `_Noreturn`". | En C11 (N1570 §7.13.2.1), `longjmp` **sí** está declarada como `_Noreturn void longjmp(jmp_buf env, int val);`. El estándar la marca `_Noreturn` porque nunca retorna a su caller (salta a otro punto del programa). Lo que sí es peculiar es que `longjmp` no termina el programa — ejecuta un non-local jump al `setjmp` correspondiente. |

---

## 1. `_Noreturn` — Funciones que nunca retornan

`_Noreturn` es un function specifier de C11 que indica al compilador que una función **nunca retorna al caller**. Típicamente porque termina el programa o entra en un loop infinito:

```c
#include <stdlib.h>
#include <stdio.h>
#include <stdnoreturn.h>  // macro: noreturn → _Noreturn

noreturn void die(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

noreturn void event_loop(void) {
    for (;;) {
        // procesar eventos...
    }
}
```

El compilador usa esta información para:

1. **Eliminar código inalcanzable** después de la llamada:

```c
int process(int *data) {
    if (data == NULL) {
        die("data is NULL");
        // El compilador sabe que die() no retorna.
        // No genera código después de esta línea.
    }
    return data[0];  // el compilador deduce que data != NULL aquí
}
```

2. **Suprimir warnings** de "control reaches end of non-void function":

```c
int get_value(int type) {
    switch (type) {
        case 1: return 10;
        case 2: return 20;
        case 3: return 30;
    }
    die("unknown type");
    // Sin noreturn en die(): warning "control reaches end..."
    // Con noreturn en die(): sin warning — el compilador sabe que
    // esta ruta no retorna
}
```

---

## 2. Cuatro formas de declarar `noreturn`

C tiene cuatro formas equivalentes, correspondientes a distintas eras del lenguaje:

```c
// 1. C11 keyword (sin header):
_Noreturn void die(const char *msg);

// 2. C11 + <stdnoreturn.h> (macro noreturn → _Noreturn):
#include <stdnoreturn.h>
noreturn void die(const char *msg);

// 3. GCC/Clang extension (pre-C11, aún soportada):
void die(const char *msg) __attribute__((noreturn));

// 4. C23 standard attribute:
[[noreturn]] void die(const char *msg);
```

En código nuevo con C11, usar `noreturn` (con `<stdnoreturn.h>`) o `_Noreturn` directamente. En proyectos que necesitan compatibilidad pre-C11 o usan extensiones GCC, la forma `__attribute__((noreturn))` es la más portada (Clang también la soporta).

Nota: en C23, `<stdnoreturn.h>` está deprecated — se reemplaza por el atributo `[[noreturn]]`.

---

## 3. Funciones `noreturn` de la biblioteca estándar

Estas funciones de `<stdlib.h>` están declaradas como `_Noreturn` en C11:

```c
_Noreturn void exit(int status);       // termina con cleanup (atexit handlers)
_Noreturn void abort(void);            // termina con SIGABRT
_Noreturn void _Exit(int status);      // termina sin cleanup
_Noreturn void quick_exit(int status); // termina con at_quick_exit handlers
```

Y en `<setjmp.h>`:

```c
_Noreturn void longjmp(jmp_buf env, int val);
```

`longjmp` es un caso especial: no termina el programa, sino que realiza un **non-local jump** al punto donde se hizo `setjmp`. Pero desde la perspectiva del caller, nunca retorna — por eso es `_Noreturn`.

`abort()` envía SIGABRT al proceso. El código de salida es `128 + 6 = 134` (128 + número de señal).

---

## 4. Warning y UB: función `noreturn` que retorna

Si una función marcada `noreturn` tiene un camino de ejecución donde retorna, ocurren dos cosas:

```c
noreturn void maybe_exit(int code) {
    if (code > 0) {
        exit(code);    // OK: no retorna
    }
    printf("Not exiting...\n");
    // Si code <= 0: la función retorna → UB
}
```

1. El compilador emite un **warning** (no error): `'noreturn' function does return`.
2. Si la función efectivamente retorna en runtime, el comportamiento es **indefinido**. El compilador puede haber generado código que asume que nunca se ejecuta el `return`, causando crashes o comportamiento impredecible.

**Toda ruta de ejecución** de una función `noreturn` debe terminar en `exit()`, `abort()`, un loop infinito, o otra función `noreturn`.

---

## 5. `_Static_assert` — Verificación en compilación

`_Static_assert` verifica una condición en **tiempo de compilación**. Si la condición es falsa, la compilación falla con el mensaje proporcionado:

```c
#include <assert.h>  // para el macro static_assert → _Static_assert

static_assert(sizeof(int) == 4, "int must be 4 bytes");
// Si sizeof(int) != 4:
// error: static assertion failed: "int must be 4 bytes"
```

Características clave:
- Se evalúa en **compilación**, no en runtime.
- Si falla, la compilación **se detiene** con error (no es un warning).
- No genera código — **costo runtime: cero**.
- No se puede desactivar con `NDEBUG` (a diferencia de `assert()`).
- La condición debe ser una **expresión constante** (evaluable en compilación).

### `_Static_assert` vs `static_assert`

```c
// _Static_assert es el keyword de C11:
_Static_assert(sizeof(int) == 4, "...");

// static_assert es un macro en <assert.h>:
// #define static_assert _Static_assert
#include <assert.h>
static_assert(sizeof(int) == 4, "...");

// En C23: static_assert se convierte en keyword (no necesita header)
// En C23: el mensaje es opcional:
static_assert(sizeof(int) == 4);  // OK en C23
```

### Dónde puede aparecer

`static_assert` puede ir donde van las declaraciones:

```c
// En scope de archivo:
static_assert(sizeof(int) == 4, "...");

// Dentro de una función:
void foo(void) {
    static_assert(sizeof(int) == 4, "...");
}

// Fuera de un struct (para verificar su layout):
struct Packet {
    uint8_t  type;
    uint8_t  flags;
    uint16_t length;
    uint32_t payload;
};
static_assert(sizeof(struct Packet) == 8,
              "Packet must be exactly 8 bytes");
```

---

## 6. Diferencia entre `static_assert` y `assert()`

|  | `static_assert` | `assert()` |
|--|-----------------|------------|
| **Cuándo evalúa** | Compilación | Runtime |
| **Si falla** | La compilación se detiene | Imprime mensaje + `abort()` |
| **Costo runtime** | Zero | Bajo (una comparación) |
| **Desactivable** | No | Sí, con `#define NDEBUG` |
| **Condición** | Expresión constante | Cualquier expresión |
| **Header** | `<assert.h>` (C11) | `<assert.h>` |

```c
#include <assert.h>

// Compilación — verifica supuestos de plataforma:
static_assert(sizeof(int) >= 4, "need 32-bit ints");

// Runtime — verifica estados del programa:
void process(int *ptr) {
    assert(ptr != NULL);  // se evalúa cada vez que se llama
    // ...
}
```

`static_assert` es para invariantes **conocidas en compilación** (tamaños, alignment, constantes). `assert()` es para condiciones que dependen del estado del programa en ejecución.

---

## 7. Usos prácticos de `static_assert`

### Verificar supuestos de plataforma

```c
#include <limits.h>
#include <stdint.h>

static_assert(sizeof(char) == 1, "char must be 1 byte");
static_assert(CHAR_BIT == 8, "need 8-bit bytes");
static_assert(sizeof(void *) == 8, "64-bit platform required");
static_assert(sizeof(int) <= sizeof(long), "int must fit in long");
```

### Verificar layout de structs (wire protocols, serialización)

```c
struct NetworkHeader {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_number;
    uint32_t ack_number;
};
static_assert(sizeof(struct NetworkHeader) == 12,
              "NetworkHeader must be exactly 12 bytes for wire format");
```

Si alguien agrega un campo sin actualizar el `static_assert`, la compilación falla inmediatamente — red de seguridad gratuita contra cambios accidentales.

### Sincronizar enums con arrays de nombres

```c
enum Color { RED, GREEN, BLUE, COLOR_COUNT };
static const char *color_names[] = { "red", "green", "blue" };

static_assert(sizeof(color_names) / sizeof(color_names[0]) == COLOR_COUNT,
              "color_names out of sync with Color enum");
```

Si agregas un valor al enum sin actualizar el array, la compilación falla.

### Verificar alignment

```c
#include <stdalign.h>

static_assert(alignof(double) == 8, "need 8-byte aligned doubles");
```

---

## 8. Combinando `noreturn` y `static_assert`: framework de errores

Un patrón práctico que combina ambos conceptos:

```c
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stdnoreturn.h>
#include <stdint.h>
#include <limits.h>

// ---- Verificaciones en compilación ----
static_assert(sizeof(int) >= 4, "need 32-bit ints");
static_assert(CHAR_BIT == 8, "need 8-bit bytes");

// ---- Función de error que nunca retorna ----
noreturn void fatal(const char *file, int line, const char *msg) {
    fflush(stdout);  // flush antes de abortar
    fprintf(stderr, "%s:%d: FATAL: %s\n", file, line, msg);
    abort();
}

// ---- Macros de conveniencia ----
#define FATAL(msg) fatal(__FILE__, __LINE__, msg)

#define ENSURE(cond, msg) do { \
    if (!(cond)) FATAL("assertion failed: " #cond " -- " msg); \
} while (0)
```

Este patrón tiene tres capas:

1. **`static_assert`** al inicio: detiene la compilación si los supuestos de plataforma son incorrectos.
2. **`fatal()` con `noreturn`**: imprime ubicación (`__FILE__`, `__LINE__`) y aborta. El compilador sabe que no retorna, así que no genera warnings en funciones que llaman a `FATAL`.
3. **`ENSURE` macro**: wrapper con `do {} while(0)` que stringify la condición (`#cond`) para incluirla en el mensaje de error.

El uso de `fflush(stdout)` antes de `fprintf(stderr)` es importante: sin él, la salida a stdout puede quedar en el buffer y no imprimirse antes del abort.

---

## 9. Comparación con Rust

### `noreturn` → tipo `!` (never type)

```rust
// En Rust, el tipo de retorno ! indica "nunca retorna":
fn die(msg: &str) -> ! {
    eprintln!("FATAL: {}", msg);
    std::process::exit(1);
}

// panic! también es divergente (retorna !):
fn get_value(t: i32) -> i32 {
    match t {
        1 => 10,
        2 => 20,
        3 => 30,
        _ => panic!("unknown type"),  // ! coerces to i32
    }
}
```

`!` es un tipo first-class en Rust: se puede usar como tipo de retorno, y coerce a cualquier otro tipo (por eso `panic!()` puede aparecer donde se espera un `i32`). En C, `_Noreturn` es metadata — no afecta el tipo de la función.

### `static_assert` → `const` assertions

```rust
// static_assert nativo en Rust (desde 1.79):
const _: () = assert!(std::mem::size_of::<i32>() == 4);

// O con el patrón tradicional:
const _: () = if std::mem::size_of::<i32>() != 4 {
    panic!("int must be 4 bytes");
};
```

Rust evalúa expresiones `const` en compilación. Si un `assert!` dentro de un contexto `const` falla, la compilación se detiene — mismo efecto que `static_assert` en C.

Rust no tiene un problema equivalente al de verificar layout de structs para wire protocols, porque `repr(C)` + `size_of` cumple ese rol con el mismo patrón de `const` assertions.

---

## Ejercicios

### Ejercicio 1 — `noreturn` elimina warning

Escribe una función `get_color_name` que usa un switch y llama a `die()` para el default. Compila una versión sin `noreturn` en `die()` y otra con `noreturn`, y observa la diferencia en warnings:

```c
// version_sin.c
#include <stdio.h>
#include <stdlib.h>

void die(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);
}

const char *get_color_name(int code) {
    switch (code) {
        case 0: return "red";
        case 1: return "green";
        case 2: return "blue";
    }
    die("unknown color code");
}

int main(void) {
    printf("Color 1: %s\n", get_color_name(1));
    return 0;
}
```

```c
// version_con.c — mismo código, pero agregar:
#include <stdnoreturn.h>
// y cambiar die() a:
noreturn void die(const char *msg) { ... }
```

```bash
echo "=== Sin noreturn ==="
gcc -std=c11 -Wall -Wextra -Wpedantic version_sin.c -o version_sin 2>&1

echo "=== Con noreturn ==="
gcc -std=c11 -Wall -Wextra -Wpedantic version_con.c -o version_con 2>&1
```

<details><summary>Predicción</summary>

```
=== Sin noreturn ===
version_sin.c: In function 'get_color_name':
version_sin.c:17:1: warning: control reaches end of non-void function [-Wreturn-type]

=== Con noreturn ===
(sin warnings)
```

Sin `noreturn`, el compilador ve que `die()` podría retornar (desde su perspectiva), y después de ella `get_color_name()` termina sin `return` — warning. Con `noreturn`, el compilador sabe que `die()` nunca retorna, así que la ruta está completa.

Ambos programas funcionan correctamente — imprimen `Color 1: green` — pero el warning indica un posible bug que `noreturn` resuelve formalmente.

</details>

---

### Ejercicio 2 — `noreturn` que retorna: UB

Crea una función marcada `noreturn` que tiene un camino donde retorna:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

noreturn void conditional_exit(int should_exit) {
    if (should_exit) {
        printf("Exiting now.\n");
        exit(0);
    }
    printf("Oops, returning from noreturn function...\n");
}

int main(void) {
    conditional_exit(0);
    printf("After conditional_exit\n");
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex2.c -o ex2
```

<details><summary>Predicción</summary>

Compilación:
```
ex2.c: In function 'conditional_exit':
ex2.c:10:1: warning: 'noreturn' function does return
```

Es un **warning**, no un error — el programa compila. Pero el comportamiento al ejecutar con `conditional_exit(0)` es **indefinido**. El compilador confió en la promesa de que la función no retorna y puede haber generado código sin el epílogo de retorno (restaurar registros, stack pointer). El programa podría:
- Funcionar "por casualidad"
- Crashear con segfault
- Ejecutar basura
- Cualquier otra cosa

Este es un caso donde el warning del compilador señala un bug real. Toda ruta de una función `noreturn` debe terminar en `exit()`, `abort()`, loop infinito, u otra función `noreturn`.

</details>

---

### Ejercicio 3 — `static_assert` básico: supuestos de plataforma

Escribe un programa que verifica varios supuestos sobre tu plataforma:

```c
#include <stdio.h>
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdalign.h>

static_assert(sizeof(char) == 1, "char must be 1 byte (C standard)");
static_assert(CHAR_BIT == 8, "need 8-bit bytes");
static_assert(sizeof(int) >= 4, "need at least 32-bit ints");
static_assert(sizeof(void *) == 8, "this program requires 64-bit pointers");
static_assert(sizeof(size_t) >= sizeof(int), "size_t must hold any int");
static_assert(alignof(int) >= 4, "ints need at least 4-byte alignment");

int main(void) {
    printf("Platform checks passed:\n");
    printf("  sizeof(char)    = %zu\n", sizeof(char));
    printf("  CHAR_BIT        = %d\n", CHAR_BIT);
    printf("  sizeof(int)     = %zu\n", sizeof(int));
    printf("  sizeof(void *)  = %zu\n", sizeof(void *));
    printf("  sizeof(size_t)  = %zu\n", sizeof(size_t));
    printf("  alignof(int)    = %zu\n", alignof(int));
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex3.c -o ex3
./ex3
```

<details><summary>Predicción</summary>

En una plataforma x86-64 típica (Linux 64-bit), todas las aserciones pasan:

```
Platform checks passed:
  sizeof(char)    = 1
  CHAR_BIT        = 8
  sizeof(int)     = 4
  sizeof(void *)  = 8
  sizeof(size_t)  = 8
  alignof(int)    = 4
```

Si compilas en un sistema de 32 bits, `sizeof(void *) == 8` falla y la compilación se detiene con error. Eso es exactamente lo que queremos: detectar en compilación que el programa no correrá correctamente en esa plataforma.

`sizeof(char) == 1` es una garantía del estándar C — siempre pasa. Se incluye como documentación ejecutable.

</details>

---

### Ejercicio 4 — `static_assert` que falla: ver el mensaje

Escribe un `static_assert` intencionalmente falso y observa el error:

```c
#include <assert.h>

static_assert(sizeof(int) == 2, "FAIL: int is not 2 bytes on this platform");

int main(void) {
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex4.c -o ex4
echo "Exit code: $?"
ls ex4 2>&1
```

<details><summary>Predicción</summary>

```
ex4.c:3:1: error: static assertion failed: "FAIL: int is not 2 bytes on this platform"
    3 | static_assert(sizeof(int) == 2, "FAIL: int is not 2 bytes on this platform");
      | ^~~~~~~~~~~~~
Exit code: 1
ls: cannot access 'ex4': No such file or directory
```

La compilación **falla** (error, no warning). No se genera ejecutable. El mensaje que proporcionaste aparece como parte del error — por eso es importante escribir mensajes descriptivos que expliquen qué se esperaba y por qué.

A diferencia de `assert()` (que falla en runtime), `static_assert` nunca llega a generar código. Es la forma más temprana posible de detectar un problema.

</details>

---

### Ejercicio 5 — Enum-array sync con `static_assert`

Usa `static_assert` para garantizar que un array de strings está sincronizado con un enum:

```c
#include <stdio.h>
#include <assert.h>

enum LogLevel {
    LOG_DEBUG,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL,
    LOG_LEVEL_COUNT
};

static const char *level_names[] = {
    "DEBUG", "INFO", "WARN", "ERROR", "FATAL"
};

static_assert(sizeof(level_names) / sizeof(level_names[0]) == LOG_LEVEL_COUNT,
              "level_names out of sync with LogLevel enum");

const char *log_level_name(enum LogLevel level) {
    if (level < 0 || level >= LOG_LEVEL_COUNT) {
        return "UNKNOWN";
    }
    return level_names[level];
}

int main(void) {
    for (int i = 0; i < LOG_LEVEL_COUNT; i++) {
        printf("[%d] %s\n", i, log_level_name(i));
    }
    return 0;
}
```

Después de verificar que compila, agrega `LOG_TRACE` al enum (antes de `LOG_LEVEL_COUNT`) **sin** actualizar `level_names` e intenta compilar de nuevo.

<details><summary>Predicción</summary>

Primera compilación (sincronizado):
```
[0] DEBUG
[1] INFO
[2] WARN
[3] ERROR
[4] FATAL
```

Después de agregar `LOG_TRACE` sin actualizar el array:
```
error: static assertion failed: "level_names out of sync with LogLevel enum"
```

`LOG_LEVEL_COUNT` pasó de 5 a 6, pero el array sigue con 5 elementos. El `static_assert` detecta el desajuste en compilación, antes de que cause un acceso fuera de bounds en runtime.

Este es uno de los usos más valiosos de `static_assert`: convertir un bug de runtime (out-of-bounds) en un error de compilación.

</details>

---

### Ejercicio 6 — Verificar layout de struct

Crea un struct para un protocolo binario y verifica su tamaño con `static_assert`:

```c
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

struct Message {
    uint8_t  version;
    uint8_t  type;
    uint16_t length;
    uint32_t sequence;
    uint32_t checksum;
};

static_assert(sizeof(struct Message) == 12,
              "Message must be 12 bytes for network protocol");

int main(void) {
    struct Message msg = {
        .version  = 1,
        .type     = 3,
        .length   = 100,
        .sequence = 42,
        .checksum = 0xDEADBEEF
    };

    printf("sizeof(Message) = %zu\n", sizeof(struct Message));
    printf("Message: v=%u type=%u len=%u seq=%u chk=0x%08X\n",
           msg.version, msg.type, msg.length, msg.sequence, msg.checksum);
    return 0;
}
```

<details><summary>Predicción</summary>

```
sizeof(Message) = 12
Message: v=1 type=3 len=100 seq=42 chk=0xDEADBEEF
```

El struct tiene 12 bytes: `uint8_t` (1) + `uint8_t` (1) + `uint16_t` (2) + `uint32_t` (4) + `uint32_t` (4) = 12. No hay padding porque los campos están alineados naturalmente: los dos `uint8_t` suman 2 bytes, que es el alignment de `uint16_t`.

Si alguien reordena los campos (por ejemplo, poniendo `uint32_t` primero y `uint8_t` después), el compilador podría insertar padding y el tamaño cambiaría. El `static_assert` detectaría el problema.

</details>

---

### Ejercicio 7 — `assert()` vs `static_assert`: dos capas

Demuestra la diferencia entre verificación en compilación y en runtime:

```c
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

// Compilación — siempre se verifica:
static_assert(sizeof(int32_t) == 4, "int32_t must be 4 bytes");

void divide(int a, int b) {
    // Runtime — depende de los argumentos:
    assert(b != 0 && "division by zero");
    printf("%d / %d = %d\n", a, b, a / b);
}

int main(void) {
    divide(10, 3);
    divide(20, 4);

    printf("\nNow triggering runtime assert...\n");
    fflush(stdout);
    divide(5, 0);  // assert falla aquí

    printf("This line is never reached.\n");
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex7.c -o ex7
./ex7
```

<details><summary>Predicción</summary>

```
10 / 3 = 3
20 / 4 = 5

Now triggering runtime assert...
ex7: ex7.c:10: divide: Assertion `b != 0 && "division by zero"' failed.
```

El `static_assert` pasó en compilación (invisible en la salida). Las dos primeras llamadas a `divide` funcionan. La tercera llamada con `b=0` dispara `assert()`, que imprime el archivo, línea, función y la expresión que falló, y luego llama a `abort()`.

El truco `b != 0 && "division by zero"` agrega un mensaje legible al assert: si `b != 0` es true, la expresión `true && "ptr"` es true (un string literal es truthy). Si `b == 0`, la expresión es false y el assert muestra todo el texto incluido el string.

</details>

---

### Ejercicio 8 — Framework `ENSURE` con `noreturn`

Implementa el patrón completo de error handling con `FATAL`/`ENSURE`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdnoreturn.h>

noreturn void fatal(const char *file, int line, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "%s:%d: FATAL: %s\n", file, line, msg);
    abort();
}

#define FATAL(msg) fatal(__FILE__, __LINE__, msg)

#define ENSURE(cond, msg) do { \
    if (!(cond)) FATAL("check failed: " #cond " -- " msg); \
} while (0)

char *safe_strdup(const char *s) {
    ENSURE(s != NULL, "NULL string");
    char *copy = malloc(strlen(s) + 1);
    ENSURE(copy != NULL, "malloc failed");
    strcpy(copy, s);
    return copy;
}

int safe_divide(int a, int b) {
    ENSURE(b != 0, "division by zero");
    return a / b;
}

int main(void) {
    char *greeting = safe_strdup("Hello");
    printf("Copied: %s\n", greeting);
    printf("10 / 3 = %d\n", safe_divide(10, 3));
    free(greeting);

    printf("\nTriggering ENSURE failure...\n");
    fflush(stdout);
    safe_divide(5, 0);

    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex8.c -o ex8
./ex8
```

<details><summary>Predicción</summary>

```
Copied: Hello
10 / 3 = 3

Triggering ENSURE failure...
ex8.c:24: FATAL: check failed: b != 0 -- division by zero
```

El programa imprime los resultados válidos y luego `ENSURE(b != 0, ...)` falla. El macro `#cond` se expande a la cadena literal `"b != 0"`, que se concatena con el mensaje. `__FILE__` y `__LINE__` apuntan a donde se escribió `ENSURE`, no a donde se definió `fatal` — lo que facilita localizar el bug.

Nota: `safe_divide` no produce warning de "control reaches end" porque `FATAL` llama a `fatal()` que es `noreturn`. El compilador sabe que si `ENSURE` falla, la función no retorna.

</details>

---

### Ejercicio 9 — `noreturn` y optimización: código inalcanzable

Verifica que el compilador elimina código inalcanzable después de `noreturn`:

```c
#include <stdio.h>
#include <stdlib.h>
#include <stdnoreturn.h>

noreturn void bail(void) {
    fprintf(stderr, "bailing out\n");
    exit(1);
}

int process(int x) {
    if (x < 0) {
        bail();
        printf("UNREACHABLE after bail\n");  // código muerto
        x = 999;                             // código muerto
    }
    return x * 2;
}

int main(void) {
    printf("process(5)  = %d\n", process(5));
    printf("process(-1) = ?\n");
    fflush(stdout);
    printf("process(-1) = %d\n", process(-1));
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic -O2 ex9.c -o ex9
./ex9
```

<details><summary>Predicción</summary>

```
process(5)  = 10
process(-1) = ?
bailing out
```

`process(5)` funciona normalmente (retorna 10). `process(-1)` entra en el `if`, llama a `bail()` que es `noreturn` — el programa termina con `exit(1)`. Las líneas `printf("UNREACHABLE...")` y `x = 999` nunca se ejecutan.

Con `-O2`, el compilador **elimina** esas líneas del binario completamente — no solo no se ejecutan, sino que no existen en el código máquina. El compilador podría además emitir un warning con `-Wunreachable-code` (depende de la versión de GCC).

Sin `noreturn` en `bail()`, el compilador no puede hacer esta eliminación porque no sabe que `bail()` no retorna.

</details>

---

### Ejercicio 10 — Mini error-handling completo: compilación + runtime

Combina todo en un programa que simula procesamiento de registros con ambas capas de verificación:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdnoreturn.h>
#include <stdint.h>
#include <limits.h>

// ==== Verificaciones en compilación ====
static_assert(sizeof(int) >= 4, "need 32-bit ints");
static_assert(CHAR_BIT == 8, "need 8-bit bytes");

struct Record {
    int32_t id;
    char    name[32];
    int32_t score;
};
static_assert(sizeof(struct Record) == 40,
              "Record layout changed -- update serialization code");

// ==== Error handling ====
noreturn void fatal(const char *file, int line, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "%s:%d: FATAL: %s\n", file, line, msg);
    abort();
}

#define FATAL(msg) fatal(__FILE__, __LINE__, msg)
#define ENSURE(cond, msg) do { \
    if (!(cond)) FATAL("failed: " #cond " -- " msg); \
} while (0)

// ==== Lógica de negocio ====
void print_records(const struct Record *recs, int count) {
    ENSURE(recs != NULL, "records pointer is NULL");
    ENSURE(count > 0 && count <= 1000, "count out of range");

    for (int i = 0; i < count; i++) {
        ENSURE(recs[i].id > 0, "invalid record id");
        printf("  [%d] %-10s %d\n", recs[i].id, recs[i].name, recs[i].score);
    }
}

int find_top_score(const struct Record *recs, int count) {
    ENSURE(recs != NULL && count > 0, "invalid input");
    int best = 0;
    for (int i = 0; i < count; i++) {
        if (recs[i].score > recs[best].score) {
            best = i;
        }
    }
    return best;
}

int main(void) {
    struct Record data[] = {
        { 1, "Alice",   95 },
        { 2, "Bob",     87 },
        { 3, "Charlie", 92 },
    };
    int n = sizeof(data) / sizeof(data[0]);

    printf("Records:\n");
    print_records(data, n);

    int top = find_top_score(data, n);
    printf("\nTop scorer: %s (%d)\n", data[top].name, data[top].score);

    printf("\nNow testing with NULL...\n");
    fflush(stdout);
    print_records(NULL, 3);

    printf("Never reached.\n");
    return 0;
}
```

```bash
gcc -std=c11 -Wall -Wextra -Wpedantic ex10.c -o ex10
./ex10
```

<details><summary>Predicción</summary>

```
Records:
  [1] Alice      95
  [2] Bob        87
  [3] Charlie    92

Top scorer: Alice (95)

Now testing with NULL...
ex10.c:38: FATAL: failed: recs != NULL -- records pointer is NULL
```

El programa funciona correctamente hasta la llamada con `NULL`. `ENSURE(recs != NULL, ...)` falla y llama a `FATAL`, que llama a `fatal()` (noreturn), que imprime la ubicación exacta y aborta. El exit code es 134 (SIGABRT).

Las tres capas de protección:
1. **`static_assert`** verificó en compilación que el struct `Record` tiene 40 bytes y que la plataforma tiene ints de 32+ bits.
2. **`ENSURE`** verificó en runtime que los punteros no son NULL y los rangos son válidos.
3. **`noreturn`** en `fatal()` permitió al compilador optimizar: sabe que después de `ENSURE` fallido, no hay retorno.

</details>
