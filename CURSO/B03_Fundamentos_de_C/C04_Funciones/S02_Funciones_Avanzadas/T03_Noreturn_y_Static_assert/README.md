# T03 — _Noreturn y _Static_assert

## _Noreturn — Funciones que nunca retornan

`_Noreturn` indica que una función **nunca** retorna al caller.
Típicamente porque termina el programa o entra en un loop
infinito:

```c
#include <stdlib.h>
#include <stdio.h>
#include <stdnoreturn.h>    // macro noreturn = _Noreturn

noreturn void die(const char *msg) {
    fprintf(stderr, "FATAL: %s\n", msg);
    exit(1);                // termina el programa
}

noreturn void infinite_loop(void) {
    for (;;) {
        // procesar eventos...
    }
}
```

```c
// Sin el header, usar _Noreturn directamente:
_Noreturn void die(const char *msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

// En C23, se usa el atributo estándar:
// [[noreturn]] void die(const char *msg);
```

### Para qué sirve

```c
// 1. El compilador puede optimizar eliminando código inalcanzable:
int process(int *data) {
    if (data == NULL) {
        die("data is NULL");
        // El compilador sabe que die() nunca retorna.
        // No necesita generar código después de esta llamada.
    }
    return data[0];    // el compilador sabe que data != NULL aquí
}

// 2. Elimina warnings de "control reaches end of non-void function":
int get_value(int type) {
    switch (type) {
        case 1: return 10;
        case 2: return 20;
    }
    die("unknown type");
    // Sin noreturn: warning "control reaches end..."
    // Con noreturn: el compilador sabe que die() no retorna → sin warning
}
```

### Funciones noreturn de la biblioteca estándar

```c
// Estas funciones nunca retornan:
exit(1);          // termina el programa
abort();          // termina con señal SIGABRT
_Exit(1);         // termina sin cleanup
quick_exit(1);    // termina con at_quick_exit handlers

// longjmp tampoco retorna al caller (salta a otro punto),
// pero no se marca como _Noreturn porque técnicamente
// "retorna" al punto del setjmp.
```

### Warning si la función intenta retornar

```c
noreturn void bad(int x) {
    if (x > 0) {
        exit(1);
    }
    // Si x <= 0, la función retorna → UB
    // warning: 'noreturn' function does return
}

// TODA ruta de ejecución debe NO retornar.
```

### __attribute__((noreturn)) — Extensión GCC

```c
// Antes de C11, se usaba la extensión GCC:
void die(const char *msg) __attribute__((noreturn));

// En la práctica, muchos proyectos usan la extensión porque
// es más antigua y portada a Clang también.

// Equivalencias:
_Noreturn void die(const char *msg);                    // C11
noreturn void die(const char *msg);                     // C11 + stdnoreturn.h
void die(const char *msg) __attribute__((noreturn));    // GCC/Clang
[[noreturn]] void die(const char *msg);                 // C23
```

## _Static_assert — Verificación en compilación

`_Static_assert` verifica una condición en **tiempo de compilación**.
Si la condición es falsa, la compilación falla con un mensaje:

```c
#include <assert.h>    // para el macro static_assert

static_assert(sizeof(int) == 4, "int must be 4 bytes");
// Si sizeof(int) != 4, la compilación falla:
// error: static assertion failed: "int must be 4 bytes"

static_assert(sizeof(int) >= 4, "need at least 32-bit ints");
// OK en la mayoría de plataformas
```

### Diferencia con assert()

```c
#include <assert.h>

// assert() — runtime:
assert(x > 0);
// Se ejecuta en runtime.
// Si falla: imprime mensaje y llama a abort()
// Se puede desactivar con #define NDEBUG

// static_assert / _Static_assert — compilación:
static_assert(sizeof(int) == 4, "need 32-bit ints");
// Se verifica en compilación.
// Si falla: la compilación se detiene con error.
// No genera código — costo zero en runtime.
// No se puede desactivar con NDEBUG.
```

### Usos prácticos

```c
// 1. Verificar supuestos sobre la plataforma:
static_assert(sizeof(void *) == 8, "64-bit platform required");
static_assert(CHAR_BIT == 8, "need 8-bit bytes");

// 2. Verificar layout de structs:
struct Packet {
    uint8_t  type;
    uint8_t  flags;
    uint16_t length;
    uint32_t payload;
};
static_assert(sizeof(struct Packet) == 8,
              "Packet must be exactly 8 bytes (check padding)");

// 3. Verificar que un enum no creció sin actualizar la tabla:
enum Color { RED, GREEN, BLUE, COLOR_COUNT };
static const char *color_names[] = { "red", "green", "blue" };
static_assert(sizeof(color_names) / sizeof(color_names[0]) == COLOR_COUNT,
              "color_names out of sync with Color enum");

// 4. Verificar alineación:
static_assert(alignof(double) == 8, "need 8-byte aligned doubles");

// 5. Verificar que un tipo cabe en otro:
static_assert(sizeof(int) <= sizeof(long),
              "int must fit in long");
```

### Dónde puede aparecer

```c
// static_assert puede aparecer donde van las declaraciones:

// En scope de archivo:
static_assert(sizeof(int) == 4, "...");

// Dentro de una función:
void foo(void) {
    static_assert(sizeof(int) == 4, "...");
    // ...
}

// Dentro de un struct:
struct Data {
    int value;
    static_assert(sizeof(int) >= 4, "...");
    char name[32];
};
// Nota: dentro de structs es válido desde C11 pero algunos
// compiladores lo tratan como extensión. Es más seguro
// ponerlo fuera del struct.
```

### C23: sin mensaje obligatorio

```c
// En C11: el mensaje es obligatorio:
static_assert(sizeof(int) == 4, "int must be 4 bytes");

// En C23: el mensaje es opcional:
static_assert(sizeof(int) == 4);    // OK en C23
// Si falla, el compilador genera un mensaje automático

// En C11 sin mensaje: usar un string vacío o descriptivo:
static_assert(sizeof(int) == 4, "");   // funciona pero poco útil
```

### _Static_assert vs static_assert

```c
// _Static_assert es el keyword de C11:
_Static_assert(sizeof(int) == 4, "...");

// static_assert es un macro definido en <assert.h>:
// #define static_assert _Static_assert
#include <assert.h>
static_assert(sizeof(int) == 4, "...");

// En C23, static_assert se convierte en keyword.
// Ya no necesita <assert.h>.

// Para C11: incluir <assert.h> y usar static_assert.
```

## Combinando _Noreturn y _Static_assert

```c
#include <assert.h>
#include <stdnoreturn.h>
#include <stdlib.h>
#include <stdio.h>

// Verificar supuestos en compilación:
static_assert(sizeof(int) >= 4, "need 32-bit ints");

// Función de error que nunca retorna:
noreturn void fatal(const char *file, int line, const char *msg) {
    fprintf(stderr, "%s:%d: FATAL: %s\n", file, line, msg);
    abort();
}

// Macro que combina ambos conceptos:
#define FATAL(msg) fatal(__FILE__, __LINE__, msg)

// Verificación en runtime con noreturn:
#define VERIFY(cond) do { \
    if (!(cond)) FATAL("assertion failed: " #cond); \
} while (0)

int main(void) {
    int *data = malloc(100 * sizeof(int));
    VERIFY(data != NULL);    // si falla → FATAL → nunca retorna

    // usar data...
    free(data);
    return 0;
}
```

## Tabla resumen

| Feature | Tipo | Cuándo evalúa | Costo runtime |
|---|---|---|---|
| static_assert | Declaración | Compilación | Zero |
| assert() | Macro → abort() | Runtime | Bajo (se puede desactivar) |
| _Noreturn | Specifier | N/A | Zero (es metadata para el compilador) |

| _Noreturn | Equivalentes |
|---|---|
| _Noreturn void f(); | C11 keyword |
| noreturn void f(); | C11 + stdnoreturn.h |
| [[noreturn]] void f(); | C23 attribute |
| void f() __attribute__((noreturn)); | GCC/Clang extensión |

---

## Ejercicios

### Ejercicio 1 — _Noreturn

```c
// Implementar noreturn void panic(const char *fmt, ...)
// que imprima un mensaje a stderr y llame a abort().
// Usar __attribute__((format)) para verificación de formato.
// Verificar que el compilador no genera warning de
// "control reaches end of non-void function" después de panic().
```

### Ejercicio 2 — static_assert

```c
// Agregar static_asserts para verificar:
// 1. sizeof(char) == 1
// 2. Un struct específico no tiene padding inesperado
// 3. Un array de nombres tiene el mismo número de elementos
//    que un enum

// Intentar hacer que uno falle y ver el mensaje.
```

### Ejercicio 3 — Error handling

```c
// Crear un mini-framework de error handling:
// - static_assert para verificar supuestos de plataforma
// - noreturn void fatal_error(...) para errores irrecuperables
// - macro ENSURE(cond, msg) que llama a fatal_error si cond es false
// Probar con una función que abre un archivo y lee datos.
```
