# T03 — ASan y Miri

---

## 1. El problema que resuelven

Valgrind y GDB son herramientas externas: instrumentan el programa desde
afuera. Esto tiene costos:

| Herramienta | Ralentización | Integración con CI | Detección en tiempo de |
|-------------|-------------|-------------------|----------------------|
| Valgrind | $10\text{-}20\times$ | Posible pero lenta | Ejecución |
| GDB | Manual | No aplica | Interactivo |
| **ASan** | $\sim 2\times$ | **Fácil** | **Compilación + ejecución** |
| **Miri** | $\sim 10\text{-}50\times$ | **Fácil** (`cargo miri test`) | **Ejecución** |

**AddressSanitizer (ASan)** es un sanitizer integrado en el compilador
(GCC/Clang). Se habilita con un flag de compilación y detecta errores de
memoria sin necesidad de herramientas externas.

**Miri** es un intérprete de MIR (la representación intermedia de Rust) que
detecta comportamiento indefinido en código `unsafe`. Es el equivalente de
Valgrind+ASan para Rust `unsafe`.

---

## 2. AddressSanitizer (ASan) en C

### Qué detecta

| Error | ASan | Valgrind |
|-------|------|---------|
| Heap buffer overflow | Sí | Sí |
| Stack buffer overflow | **Sí** | No |
| Global buffer overflow | **Sí** | No |
| Use-after-free | Sí | Sí |
| Use-after-return | **Sí** (con flag extra) | No |
| Double free | Sí | Sí |
| Memory leak | Sí (con LeakSanitizer) | Sí |
| Use-after-scope | **Sí** | No |

ASan detecta más tipos de errores que Valgrind, especialmente en el **stack**
y en variables **globales** — áreas donde Valgrind no tiene visibilidad.

### Cómo funciona

ASan instrumenta el código durante la compilación. Agrega "zonas rojas"
(red zones) alrededor de cada alocación — zonas de memoria marcadas como
inaccesibles. Si el programa lee o escribe en una zona roja, ASan aborta
inmediatamente con un reporte detallado.

```
Memoria sin ASan:
[  datos  ][  datos  ][  datos  ]
  buffer A    buffer B    buffer C

Memoria con ASan:
[RED][  datos  ][RED][  datos  ][RED][  datos  ][RED]
     buffer A        buffer B        buffer C

Overflow de A → cae en la zona roja → ASan lo detecta
```

### Habilitar ASan

```bash
gcc -g -O0 -fsanitize=address -o program program.c
```

O con Clang (reporte más legible):

```bash
clang -g -O0 -fsanitize=address -o program program.c
```

**No necesita herramienta externa** — el ejecutable se ejecuta directamente:

```bash
./program
```

Si hay un error de memoria, ASan aborta e imprime el reporte.

### Flags adicionales útiles

```bash
# Detectar leaks también (LeakSanitizer, incluido con ASan en Linux)
gcc -g -O0 -fsanitize=address -o program program.c
ASAN_OPTIONS=detect_leaks=1 ./program

# Detectar uso de variables de stack después de que la función retorne
gcc -g -O0 -fsanitize=address -fno-omit-frame-pointer \
    -fsanitize-address-use-after-scope -o program program.c

# Detectar acceso a stack de función que ya retornó
ASAN_OPTIONS=detect_stack_use_after_return=1 ./program
```

---

## 3. ASan en acción: heap buffer overflow

### El código

```c
#include <stdlib.h>
#include <string.h>

int main(void) {
    int *arr = malloc(10 * sizeof(int));
    arr[10] = 42;   // BUG: escribe 1 posición más allá del buffer
    free(arr);
    return 0;
}
```

### Compilar y ejecutar

```bash
gcc -g -O0 -fsanitize=address -o overflow overflow.c
./overflow
```

### Salida de ASan

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x604000000028
    at pc 0x0000004011a3 bp 0x7ffd3c8e0df0 sp 0x7ffd3c8e0de8
WRITE of size 4 at 0x604000000028 thread T0
    #0 0x4011a2 in main overflow.c:6
    #1 0x7f8a2c029d8f in __libc_start_call_main

0x604000000028 is located 0 bytes to the right of 40-byte region
[0x604000000000,0x604000000028)
allocated by thread T0 here:
    #0 0x7f8a2c47e887 in __interceptor_malloc
    #1 0x401172 in main overflow.c:5
```

### Cómo leer el reporte

```
heap-buffer-overflow → tipo de error

WRITE of size 4 at 0x...28 → se escribieron 4 bytes (un int)
                               en la dirección ...28

overflow.c:6 → línea del error: arr[10] = 42

0 bytes to the right of 40-byte region → el acceso está JUSTO después
del buffer de 40 bytes (10 ints × 4 bytes)

allocated ... overflow.c:5 → el buffer se alocó en la línea 5
```

### Comparación: el mismo error en Valgrind

```
==12345== Invalid write of size 4
==12345==    at 0x4011A2: main (overflow.c:6)
==12345==  Address 0x4a47068 is 0 bytes after a block of size 40 alloc'd
==12345==    at 0x4843839: malloc (...)
==12345==    by 0x401172: main (overflow.c:5)
```

Información similar, pero ASan es $\sim 5\text{-}10\times$ más rápido.

---

## 4. ASan: stack buffer overflow

Este es un error que **Valgrind no detecta**:

```c
#include <stdio.h>

void dangerous(void) {
    int arr[5];
    arr[5] = 999;    // BUG: overflow del array en stack
    printf("%d\n", arr[4]);
}

int main(void) {
    dangerous();
    return 0;
}
```

### Salida de ASan

```
==12345==ERROR: AddressSanitizer: stack-buffer-overflow on address 0x7ffd3c8e0e34
WRITE of size 4 at 0x7ffd3c8e0e34 thread T0
    #0 0x401192 in dangerous stack_overflow.c:5
    #1 0x4011e3 in main stack_overflow.c:10

Address 0x7ffd3c8e0e34 is located in stack of thread T0 at offset 52 in frame
    #0 0x401160 in dangerous stack_overflow.c:3

  This frame has 1 object(s):
    [32, 52) 'arr' (line 4)     <== Memory access at offset 52 overflows this variable
```

ASan identifica la **variable exacta** que se desbordó (`arr` en la línea 4)
y el offset del acceso ilegal. Valgrind no tendría esta información porque
no instrumenta el stack.

---

## 5. ASan: use-after-free en lista enlazada

```c
#include <stdlib.h>
#include <stdio.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

int main(void) {
    Node *a = malloc(sizeof(Node));
    Node *b = malloc(sizeof(Node));
    a->data = 10;
    a->next = b;
    b->data = 20;
    b->next = NULL;

    free(a);
    printf("a->data = %d\n", a->data);   // use-after-free
    return 0;
}
```

### Salida de ASan

```
==12345==ERROR: AddressSanitizer: heap-use-after-free on address 0x602000000010
READ of size 4 at 0x602000000010 thread T0
    #0 0x4011d3 in main uaf.c:17

0x602000000010 is located 0 bytes inside of 16-byte region
[0x602000000010,0x602000000020)
freed by thread T0 here:
    #0 0x7f8a2c47f2a7 in __interceptor_free
    #1 0x4011c2 in main uaf.c:16

previously allocated by thread T0 here:
    #0 0x7f8a2c47e887 in __interceptor_malloc
    #1 0x401182 in main uaf.c:10
```

ASan muestra tres puntos clave:
1. **Dónde se accedió** (uaf.c:17 — el `printf`)
2. **Dónde se liberó** (uaf.c:16 — el `free(a)`)
3. **Dónde se alocó** (uaf.c:10 — el `malloc`)

Esta triple trazabilidad (uso → free → malloc) es extremadamente útil para
bugs en destructores de estructuras de datos.

---

## 6. ASan: detección de leaks (LeakSanitizer)

En Linux, ASan incluye **LeakSanitizer (LSan)** automáticamente:

```c
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node *next;
} Node;

int main(void) {
    Node *head = malloc(sizeof(Node));
    head->data = 10;
    head->next = malloc(sizeof(Node));
    head->next->data = 20;
    head->next->next = NULL;

    // BUG: no se libera la lista
    return 0;
}
```

```bash
gcc -g -O0 -fsanitize=address -o leak leak.c
./leak
```

```
=================================================================
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 16 byte(s) in 1 object(s) allocated from:
    #0 0x7f8a2c47e887 in __interceptor_malloc
    #1 0x401152 in main leak.c:9

Indirect leak of 16 byte(s) in 1 object(s) allocated from:
    #0 0x7f8a2c47e887 in __interceptor_malloc
    #1 0x401172 in main leak.c:11

SUMMARY: AddressSanitizer: 32 byte(s) leaked in 2 allocation(s).
```

Misma clasificación que Valgrind: `Direct leak` ≈ `definitely lost`,
`Indirect leak` ≈ `indirectly lost`.

---

## 7. Otros sanitizers de C

ASan no es el único sanitizer. GCC y Clang ofrecen una familia completa:

### UndefinedBehaviorSanitizer (UBSan)

Detecta comportamiento indefinido: overflow de enteros con signo, shift
inválido, null dereference, conversiones de tipo ilegales.

```bash
gcc -g -O0 -fsanitize=undefined -o program program.c
```

```c
int main(void) {
    int x = __INT_MAX__;
    x += 1;   // signed integer overflow → UB
    return x;
}
```

```
runtime error: signed integer overflow: 2147483647 + 1
cannot be represented in type 'int'
```

### MemorySanitizer (MSan) — solo Clang

Detecta lecturas de memoria no inicializada (más preciso que Valgrind para
esto):

```bash
clang -g -O0 -fsanitize=memory -o program program.c
```

### ThreadSanitizer (TSan)

Detecta data races en programas multihilo:

```bash
gcc -g -O0 -fsanitize=thread -o program program.c -lpthread
```

### Combinaciones

```bash
# ASan + UBSan (la combinación más útil para DS)
gcc -g -O0 -fsanitize=address,undefined -o program program.c
```

**No se puede combinar** ASan con MSan o TSan (usan mecanismos incompatibles).
ASan + UBSan sí es compatible y recomendable.

---

## 8. Miri: el "Valgrind de Rust unsafe"

### Qué es Miri

Miri es un intérprete de la representación intermedia de Rust (MIR). Ejecuta
el programa instrucción por instrucción, verificando que cada operación
`unsafe` sea válida según las reglas del modelo de aliasing de Rust
(Stacked Borrows / Tree Borrows).

### Qué detecta

| Error | Miri | ASan (C) |
|-------|------|---------|
| Use-after-free con raw pointers | Sí | Sí |
| Buffer overflow | Sí | Sí |
| Dangling reference | **Sí** | N/A |
| Aliasing violation (Stacked Borrows) | **Sí** | N/A |
| Data race | **Sí** | No (TSan sí) |
| Memory leak | Sí | Sí |
| Uso de valor no inicializado | Sí | No (MSan sí) |
| Integer overflow en release | Sí | N/A (Rust panics en debug) |
| Misaligned pointer access | Sí | Sí |

La ventaja única de Miri: detecta violaciones del **modelo de aliasing** de
Rust. Si tu código `unsafe` invalida una referencia `&T` que Rust asume
válida, Miri lo atrapa. Ni ASan ni Valgrind pueden verificar esto.

### Instalar Miri

```bash
rustup component add miri
```

### Ejecutar Miri

```bash
cargo miri test      # ejecutar todos los tests bajo Miri
cargo miri run       # ejecutar el programa bajo Miri
```

---

## 9. Miri en acción

### Ejemplo 1: use-after-free con raw pointers

```rust
#[test]
fn test_use_after_free() {
    let ptr: *const i32;
    {
        let x = 42;
        ptr = &x as *const i32;
    }   // x se destruye aquí

    unsafe {
        println!("{}", *ptr);   // dangling pointer
    }
}
```

```bash
cargo miri test test_use_after_free
```

```
error: Undefined Behavior: dereferencing pointer to `x` which was
       already deallocated
  --> src/lib.rs:9:24
   |
9  |         println!("{}", *ptr);
   |                        ^^^^ dereferencing pointer to `x`
   |                             which was already deallocated
   |
   = help: this indicates a bug in the program: it performed an
     invalid operation, and caused Undefined Behavior
```

### Ejemplo 2: buffer overflow con raw pointers

```rust
#[test]
fn test_buffer_overflow() {
    let v = vec![1, 2, 3];
    let ptr = v.as_ptr();

    unsafe {
        let val = *ptr.add(3);   // 1 posición más allá del final
        println!("{}", val);
    }
}
```

```
error: Undefined Behavior: dereferencing pointer at offset 12,
       but that pointer only has 12 bytes of memory available
```

### Ejemplo 3: violación de aliasing (Stacked Borrows)

Este es un error que **solo Miri detecta**:

```rust
#[test]
fn test_aliasing_violation() {
    let mut x: i32 = 42;
    let ref1 = &mut x;
    let ptr = ref1 as *mut i32;

    let ref2 = &x;           // referencia compartida a x
    unsafe {
        *ptr = 99;            // escribir a través del raw pointer
    }
    println!("{}", ref2);     // leer a través de ref2
}
```

```
error: Undefined Behavior: attempting a write access through
       <TAG> which is a SharedReadOnly tag
```

El problema: `ref2` es una referencia compartida (`&x`), y Rust asume que
el valor detrás de `&x` no cambia. Escribir a través de `ptr` viola esa
asunción. El compilador podría optimizar asumiendo que `ref2` sigue
siendo 42, produciendo un programa incorrecto.

### Ejemplo 4: lista con raw pointers (correcto)

```rust
use std::ptr;

struct Node {
    data: i32,
    next: *mut Node,
}

struct Stack {
    head: *mut Node,
}

impl Stack {
    fn new() -> Self {
        Stack { head: ptr::null_mut() }
    }

    fn push(&mut self, data: i32) {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: self.head,
        }));
        self.head = node;
    }

    fn pop(&mut self) -> Option<i32> {
        if self.head.is_null() {
            return None;
        }
        unsafe {
            let node = Box::from_raw(self.head);
            self.head = node.next;
            Some(node.data)
        }
    }
}

impl Drop for Stack {
    fn drop(&mut self) {
        while self.pop().is_some() {}
    }
}

#[test]
fn test_stack() {
    let mut s = Stack::new();
    s.push(10);
    s.push(20);
    s.push(30);
    assert_eq!(s.pop(), Some(30));
    assert_eq!(s.pop(), Some(20));
    assert_eq!(s.pop(), Some(10));
    assert_eq!(s.pop(), None);
}
```

```bash
cargo miri test test_stack
```

```
test test_stack ... ok
```

Si Miri dice OK, el código `unsafe` es correcto respecto al modelo de
aliasing. Esto no garantiza ausencia de bugs lógicos, pero sí ausencia de
comportamiento indefinido.

---

## 10. Cuándo usar cada herramienta

### Árbol de decisión

```
¿Lenguaje?
├── C
│   ├── ¿Es CI o prueba rápida?
│   │   ├── Sí → ASan (-fsanitize=address,undefined)
│   │   └── No → ¿Necesitas analizar leaks en detalle?
│   │       ├── Sí → Valgrind --leak-check=full
│   │       └── No → ¿Necesitas depurar interactivamente?
│   │           └── Sí → GDB
│   └── En la práctica: ASan para CI, Valgrind para análisis profundo,
│       GDB para depuración interactiva
│
└── Rust
    ├── ¿Hay código unsafe?
    │   ├── Sí → cargo miri test (SIEMPRE)
    │   └── No → el compilador ya garantiza safety
    └── ¿Hay problemas de rendimiento?
        └── Sí → no es tema de safety, usar profiler
```

### Tabla comparativa completa

| Criterio | Valgrind | ASan | GDB | Miri |
|----------|---------|------|-----|------|
| Lenguaje | C/C++ | C/C++ | C/C++/Rust | Rust |
| Velocidad | $10\text{-}20\times$ | $\sim 2\times$ | Manual | $10\text{-}50\times$ |
| Heap errors | Sí | Sí | No directo | Sí |
| Stack errors | No | **Sí** | No directo | Sí |
| Aliasing UB | No | No | No | **Sí** |
| Leaks | Sí | Sí (LSan) | No | Sí |
| Interactivo | No | No | **Sí** | No |
| Integración CI | Lento | **Rápido** | No | **Rápido** |
| Recompilación | No | Sí | Solo -g | No |
| Cobertura | Dinámico | Dinámico | Manual | Dinámico |

### Flujo de trabajo recomendado

**Para C** (estructuras de datos):

```bash
# 1. Desarrollo: compilar con ASan siempre
CFLAGS="-g -O0 -fsanitize=address,undefined"
gcc $CFLAGS -o test_ds main.c list.c

# 2. Test rápido con ASan
./test_ds

# 3. Análisis profundo de leaks con Valgrind
gcc -g -O0 -o test_ds main.c list.c
valgrind --leak-check=full ./test_ds

# 4. Si algo falla: depuración interactiva con GDB
gcc -g -O0 -o test_ds main.c list.c
gdb ./test_ds
```

**Para Rust** (unsafe):

```bash
# 1. Tests normales
cargo test

# 2. Tests bajo Miri (cada vez que se toque código unsafe)
cargo miri test

# 3. Si Miri reporta error: leer el mensaje con atención,
#    los reportes de Miri son muy descriptivos
```

### Makefile integrado

```makefile
CC = gcc
CFLAGS = -g -O0 -Wall -Wextra

# Target normal
test_ds: main.c list.c list.h
	$(CC) $(CFLAGS) -o $@ main.c list.c

# Target con ASan
test_ds_asan: main.c list.c list.h
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o $@ main.c list.c

# Ejecutar con ASan
test-asan: test_ds_asan
	./test_ds_asan

# Ejecutar con Valgrind
test-valgrind: test_ds
	valgrind --leak-check=full --error-exitcode=1 ./test_ds

# Ejecutar ambos
test-all: test-asan test-valgrind
	@echo "All memory checks passed"

clean:
	rm -f test_ds test_ds_asan
```

```bash
make test-all    # ASan + Valgrind en secuencia
```

---

## Ejercicios

### Ejercicio 1 — ASan vs Valgrind: stack overflow

Compila y ejecuta con ASan y luego con Valgrind:

```c
#include <stdio.h>

int main(void) {
    int arr[10];
    for (int i = 0; i <= 10; i++)   // BUG: i <= 10, debería ser i < 10
        arr[i] = i * 2;
    printf("%d\n", arr[9]);
    return 0;
}
```

```bash
# Con ASan
gcc -g -O0 -fsanitize=address -o stack_test stack_test.c
./stack_test

# Con Valgrind
gcc -g -O0 -o stack_test stack_test.c
valgrind ./stack_test
```

**Prediccion**: ¿Cuál de los dos detecta el error? ¿Por qué?

<details><summary>Respuesta</summary>

**ASan detecta el error**. **Valgrind no**.

ASan:
```
ERROR: AddressSanitizer: stack-buffer-overflow on address ...
WRITE of size 4 at ... thread T0
    #0 ... in main stack_test.c:5
  'arr' (line 4)  <== Memory access at offset 40 overflows this variable
```

Valgrind: ejecuta sin errores y muestra `20` como resultado.

Razón: `arr` está en el **stack**. Valgrind instrumenta el heap (malloc/free)
pero no pone zonas rojas alrededor de variables de stack. El `arr[10] = 20`
escribe justo después del array, probablemente sobrescribiendo `i` o el
frame pointer — pero Valgrind no lo ve.

ASan sí coloca zonas rojas alrededor de variables de stack (esa es su
instrumentación durante compilación), así que detecta el overflow
inmediatamente.

**Regla**: para errores de stack, ASan es la herramienta correcta.

</details>

---

### Ejercicio 2 — ASan: use-after-free en lista

Compila este código con ASan. Predice la salida antes de ejecutar:

```c
#include <stdlib.h>
#include <stdio.h>

typedef struct Node { int data; struct Node *next; } Node;

int main(void) {
    Node *a = malloc(sizeof(Node));
    Node *b = malloc(sizeof(Node));
    a->data = 10; a->next = b;
    b->data = 20; b->next = NULL;

    Node *saved = a->next;   // saved apunta a b
    free(a);
    free(b);

    printf("saved->data = %d\n", saved->data);   // ¿safe?
    return 0;
}
```

**Prediccion**: ¿`saved->data` es un error? ¿Qué tipo de error reportará ASan?

<details><summary>Respuesta</summary>

**Sí, es un error**: `saved` apunta a `b`, y `b` ya fue liberado → use-after-free.

```
ERROR: AddressSanitizer: heap-use-after-free on address ...
READ of size 4 at ... thread T0
    #0 ... in main uaf_list.c:14

... is located 0 bytes inside of 16-byte region [...]
freed by thread T0 here:
    #0 ... in __interceptor_free
    #1 ... in main uaf_list.c:12

previously allocated by thread T0 here:
    #0 ... in __interceptor_malloc
    #1 ... in main uaf_list.c:8
```

El hecho de que `saved` "apunte a b" no importa — `b` fue liberado.
El puntero `saved` es un **dangling pointer**: su dirección es válida
como número, pero la memoria a la que apunta ya no le pertenece al
programa.

Sin sanitizers, este código probablemente "funcione" porque `free` no
suele borrar la memoria inmediatamente. Pero es UB.

</details>

---

### Ejercicio 3 — ASan + UBSan combinados

Compila con ambos sanitizers y encuentra los dos tipos de error:

```c
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int *arr = malloc(5 * sizeof(int));
    for (int i = 0; i < 5; i++)
        arr[i] = i * 500000000;    // signed overflow para i >= 5?

    printf("arr[5] = %d\n", arr[5]);   // buffer overflow
    free(arr);
    return 0;
}
```

```bash
gcc -g -O0 -fsanitize=address,undefined -o combined combined.c
./combined
```

**Prediccion**: ¿Cuántos errores se reportarán? ¿Cuál se detecta primero?

<details><summary>Respuesta</summary>

Se reportan **2 errores** si `i * 500000000` desborda:

1. **UBSan**: `runtime error: signed integer overflow: 4 * 500000000
   cannot be represented in type 'int'` — cuando `i = 4`,
   $4 \times 500{,}000{,}000 = 2{,}000{,}000{,}000$ que cabe en `int`
   ($2^{31}-1 = 2{,}147{,}483{,}647$). Así que en este caso específico
   **no hay overflow** en el loop.

2. **ASan**: `heap-buffer-overflow` en `arr[5]` — acceso una posición
   más allá del buffer.

El primer error detectado es el **buffer overflow** (ASan), porque
el cálculo `i * 500000000` no desborda para $i \in [0,4]$ (el máximo
es $2 \times 10^9 < 2^{31}-1$).

Si cambiamos a `i * 600000000`, entonces $i=4 \to 2.4 \times 10^9 > 2^{31}-1$ y
UBSan detectaría el overflow **antes** de que ASan detecte el buffer
overflow, porque el overflow ocurre en la última iteración del loop
(antes del acceso fuera de rango).

El orden de detección depende del orden de ejecución.

</details>

---

### Ejercicio 4 — Miri: dangling pointer en Rust

Ejecuta con `cargo miri test`. Predice el error:

```rust
#[test]
fn test_dangling() {
    let ptr: *const i32;
    {
        let val = vec![1, 2, 3];
        ptr = val.as_ptr();
    }   // val se dropea aquí, el Vec libera su buffer

    unsafe {
        assert_eq!(*ptr, 1);   // ¿válido?
    }
}
```

**Prediccion**: ¿Miri permitirá la lectura? ¿Qué error reportará?

<details><summary>Respuesta</summary>

**Miri aborta** con:

```
error: Undefined Behavior: dereferencing pointer to alloc1234
       which has been freed
  --> src/lib.rs:10:20
   |
10 |         assert_eq!(*ptr, 1);
   |                    ^^^^ pointer to freed allocation
```

El `Vec` se destruye al salir del scope interior. Cuando se destruye, libera
su buffer en el heap. `ptr` sigue apuntando a esa memoria liberada.

En código safe de Rust, esto es imposible — el borrow checker lo previene:
```rust
// Esto NO compila:
let ptr;
{
    let val = vec![1, 2, 3];
    ptr = &val[0];   // error: val does not live long enough
}
```

Pero con raw pointers (`*const`), el borrow checker no interviene — el
programador es responsable. Miri verifica en tiempo de ejecución lo que
el borrow checker no puede verificar estáticamente con raw pointers.

</details>

---

### Ejercicio 5 — Miri: violación de aliasing

¿Este código es correcto según Stacked Borrows? Ejecútalo con Miri:

```rust
#[test]
fn test_alias() {
    let mut data = 10i32;
    let ref_mut = &mut data;
    let ptr = ref_mut as *mut i32;

    // Crear una referencia compartida MIENTRAS existe el raw pointer
    let ref_shared = &data;

    unsafe {
        *ptr = 20;   // escribir a través del raw pointer
    }

    println!("shared sees: {}", ref_shared);
}
```

**Prediccion**: ¿Miri aceptará o rechazará este código?

<details><summary>Respuesta</summary>

**Miri rechaza** este código:

```
error: Undefined Behavior: attempting a write access using <TAG>
       at alloc1234[0x0], but that tag only grants SharedReadOnly
       permission for this location
```

El problema es la secuencia de eventos:

1. `ref_mut` = `&mut data` → permiso de escritura exclusivo
2. `ptr` = raw pointer derivado de `ref_mut`
3. `ref_shared` = `&data` → esto **invalida** `ref_mut` (y por extensión
   `ptr`) bajo Stacked Borrows, porque crear una referencia compartida
   "congela" la memoria
4. `*ptr = 20` → intenta escribir con un tag que ya fue invalidado

La corrección: no crear `ref_shared` mientras `ptr` podría ser usado:

```rust
#[test]
fn test_alias_fixed() {
    let mut data = 10i32;
    let ref_mut = &mut data;
    let ptr = ref_mut as *mut i32;

    unsafe {
        *ptr = 20;   // OK: escribir antes de crear ref_shared
    }

    let ref_shared = &data;   // OK: ptr ya no se usa después
    println!("shared sees: {}", ref_shared);   // imprime 20
}
```

</details>

---

### Ejercicio 6 — Stack unsafe en Rust con Miri

Implementa un stack con raw pointers y verifica con Miri:

```rust
use std::ptr;

struct Stack {
    head: *mut Node,
}

struct Node {
    data: i32,
    next: *mut Node,
}

// Implementar: new, push, pop, Drop
```

Escribe tests que verifiquen push/pop y ejecuta `cargo miri test`.

**Prediccion**: ¿Qué función de `Box` se usa para convertir entre
`Box<Node>` y raw pointer?

<details><summary>Respuesta</summary>

```rust
use std::ptr;

struct Node {
    data: i32,
    next: *mut Node,
}

struct Stack {
    head: *mut Node,
}

impl Stack {
    fn new() -> Self {
        Stack { head: ptr::null_mut() }
    }

    fn push(&mut self, data: i32) {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: self.head,
        }));
        self.head = node;
    }

    fn pop(&mut self) -> Option<i32> {
        if self.head.is_null() {
            return None;
        }
        unsafe {
            let node = Box::from_raw(self.head);
            self.head = node.next;
            Some(node.data)
        }
    }
}

impl Drop for Stack {
    fn drop(&mut self) {
        while self.pop().is_some() {}
    }
}

#[test]
fn test_push_pop() {
    let mut s = Stack::new();
    s.push(1);
    s.push(2);
    s.push(3);
    assert_eq!(s.pop(), Some(3));
    assert_eq!(s.pop(), Some(2));
    assert_eq!(s.pop(), Some(1));
    assert_eq!(s.pop(), None);
}

#[test]
fn test_drop_cleans_up() {
    let mut s = Stack::new();
    for i in 0..1000 {
        s.push(i);
    }
    // Drop se llama automáticamente — Miri verificará que no hay leaks
}
```

Funciones clave:
- `Box::into_raw(Box::new(...))` — aloca en heap, devuelve `*mut T`
  (ownership transferida al raw pointer)
- `Box::from_raw(ptr)` — reconstruye el `Box` para que se libere al salir
  del scope

`cargo miri test` debe pasar sin errores si la implementación es correcta.

</details>

---

### Ejercicio 7 — Comparar velocidad: ASan vs Valgrind

Mide el tiempo de ejecución de un programa que aloca y libera 100,000 nodos
bajo ASan y bajo Valgrind:

```c
#include <stdlib.h>

typedef struct Node { int data; struct Node *next; } Node;

int main(void) {
    Node *head = NULL;
    for (int i = 0; i < 100000; i++) {
        Node *n = malloc(sizeof(Node));
        n->data = i;
        n->next = head;
        head = n;
    }
    while (head) {
        Node *next = head->next;
        free(head);
        head = next;
    }
    return 0;
}
```

```bash
# Normal
gcc -g -O0 -o bench bench.c
time ./bench

# ASan
gcc -g -O0 -fsanitize=address -o bench_asan bench.c
time ./bench_asan

# Valgrind
time valgrind --leak-check=full ./bench
```

**Prediccion**: ¿Cuántas veces más lento será Valgrind comparado con ASan?

<details><summary>Respuesta</summary>

Resultados típicos:

| Modo | Tiempo |
|------|--------|
| Normal | $\sim 0.01$ s |
| ASan | $\sim 0.03$ s ($\sim 3\times$) |
| Valgrind | $\sim 0.5\text{-}1.0$ s ($\sim 50\text{-}100\times$) |

**Valgrind es $\sim 15\text{-}30\times$ más lento que ASan** para este
benchmark.

Razón: Valgrind es un emulador de CPU — cada instrucción se traduce a
código de instrumentación en runtime. ASan inserta las verificaciones
durante la compilación, así que el código corre nativamente con checks
adicionales.

Para 100,000 nodos la diferencia es de medio segundo. Para un millón de
nodos: ASan tarda $\sim 0.3$ s, Valgrind tarda $\sim 10$ s. En un pipeline
de CI con cientos de tests, esta diferencia es significativa.

**Recomendación**: ASan para CI/testing frecuente, Valgrind para análisis
profundo cuando ASan no es suficiente.

</details>

---

### Ejercicio 8 — Falso positivo y suppression

Este código usa una unión para type-punning (técnica legítima en C):

```c
#include <stdio.h>
#include <stdint.h>

union FloatBits {
    float f;
    uint32_t u;
};

int main(void) {
    union FloatBits fb;
    fb.f = 3.14f;
    printf("bits of 3.14: 0x%08x\n", fb.u);
    return 0;
}
```

**Prediccion**: ¿Algún sanitizer reportará esto como error? ¿Es un bug real?

<details><summary>Respuesta</summary>

**Ningún sanitizer reporta error** para este código.

- ASan: no hay acceso fuera de rango
- UBSan: type-punning con uniones es **definido** en C (C11 §6.5.2.3).
  En C++, sería UB — pero en C es legal
- Valgrind: no hay problema de memoria

Sin embargo, si se usa `memcpy` en vez de unión:

```c
float f = 3.14f;
uint32_t u;
memcpy(&u, &f, sizeof(u));   // también legal y portable
```

Ambos enfoques son correctos en C. La unión es más legible; `memcpy` es
más portable (funciona también en C++).

Este ejercicio ilustra que los sanitizers no reportan falsos positivos
para código idiomático de C. Los falsos positivos son raros y generalmente
ocurren con técnicas muy avanzadas (custom allocators, memory-mapped I/O).

</details>

---

### Ejercicio 9 — Miri con Node que almacena String

Modifica el stack unsafe de Rust para que almacene `String` en vez de `i32`.
Verifica con Miri que los Strings se dropean correctamente:

```rust
struct Node {
    data: String,
    next: *mut Node,
}
```

**Prediccion**: ¿Qué pasa si `Drop` no se implementa correctamente?
¿Miri detectará el leak de Strings?

<details><summary>Respuesta</summary>

```rust
use std::ptr;

struct Node {
    data: String,
    next: *mut Node,
}

struct Stack {
    head: *mut Node,
}

impl Stack {
    fn new() -> Self {
        Stack { head: ptr::null_mut() }
    }

    fn push(&mut self, data: String) {
        let node = Box::into_raw(Box::new(Node {
            data,
            next: self.head,
        }));
        self.head = node;
    }

    fn pop(&mut self) -> Option<String> {
        if self.head.is_null() {
            return None;
        }
        unsafe {
            let node = Box::from_raw(self.head);
            self.head = node.next;
            Some(node.data)
        }
    }
}

impl Drop for Stack {
    fn drop(&mut self) {
        while self.pop().is_some() {}
    }
}

#[test]
fn test_string_stack() {
    let mut s = Stack::new();
    s.push(String::from("hello"));
    s.push(String::from("world"));
    assert_eq!(s.pop(), Some(String::from("world")));
    assert_eq!(s.pop(), Some(String::from("hello")));
    assert_eq!(s.pop(), None);
}

#[test]
fn test_string_drop() {
    let mut s = Stack::new();
    s.push(String::from("leaked?"));
    s.push(String::from("also leaked?"));
    // Drop se llama aquí — debe liberar ambos Strings
}
```

**Sí, Miri detecta leaks de Strings**. Si `Drop` no se implementa o no
recorre todos los nodos:

```
error: memory leak: alloc1234 was never freed
   --> src/lib.rs:XX:YY
```

`Box::from_raw` en `pop` reconstruye el `Box<Node>`, y al salir del scope,
`Box` dropea el `Node`. El `Drop` de `Node` es automático y dropea el
`String` (que a su vez libera su buffer del heap).

Si se usara `ptr::drop_in_place(self.head)` sin `Box::from_raw`, el `Node`
se dropearía pero su **memoria** no se liberaría — Miri reportaría leak.

</details>

---

### Ejercicio 10 — Pipeline completo de verificación

Crea un `Makefile` (o `justfile`) que ejecute las 4 herramientas en
secuencia para un proyecto C + Rust:

```
1. gcc -fsanitize=address,undefined → ejecutar tests de C
2. valgrind --leak-check=full → ejecutar tests de C
3. cargo test → tests de Rust (safe)
4. cargo miri test → tests de Rust (unsafe)
```

**Prediccion**: ¿Es necesario compilar el programa de C dos veces? ¿Por qué?

<details><summary>Respuesta</summary>

**Sí**, se necesitan dos compilaciones de C:

1. Con `-fsanitize=address,undefined` para ASan/UBSan
2. Sin sanitizers para Valgrind (ASan y Valgrind son **incompatibles** —
   ejecutar un binario ASan bajo Valgrind causa errores)

```makefile
CC = gcc
CFLAGS = -g -O0 -Wall -Wextra
SRC = main.c list.c
HDR = list.h

# Binarios
test_ds: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -o $@ $(SRC)

test_ds_asan: $(SRC) $(HDR)
	$(CC) $(CFLAGS) -fsanitize=address,undefined -o $@ $(SRC)

# Tests C
test-asan: test_ds_asan
	./test_ds_asan
	@echo "ASan: PASS"

test-valgrind: test_ds
	valgrind --leak-check=full --error-exitcode=1 ./test_ds
	@echo "Valgrind: PASS"

# Tests Rust
test-rust:
	cargo test
	@echo "Cargo test: PASS"

test-miri:
	cargo miri test
	@echo "Miri: PASS"

# Pipeline completo
test-all: test-asan test-valgrind test-rust test-miri
	@echo "All checks passed"

clean:
	rm -f test_ds test_ds_asan
	cargo clean
```

```bash
make test-all
```

Si cualquier paso falla, `make` aborta con error. En CI:

```yaml
# .github/workflows/memory-check.yml
- run: make test-asan
- run: make test-valgrind
- run: make test-miri
```

</details>

Limpieza:

```bash
rm -f overflow stack_test bench bench_asan combined test_ds test_ds_asan
```
