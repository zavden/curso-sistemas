# T04 — Herramienta integrada: Valgrind, ASan, Miri

## Objetivo

Dominar el **flujo de trabajo** de depuración de memoria usando las
tres herramientas fundamentales: **Valgrind memcheck** (C), **ASan**
(C, compilador), y **Miri** (Rust). Aprender a integrarlas en el
ciclo de desarrollo como un checklist antes de commit.

---

## 1. Panorama de herramientas

### 1.1 Tres herramientas, tres enfoques

| Herramienta | Lenguaje | Mecanismo | Overhead | Detección |
|-------------|:--------:|-----------|:--------:|-----------|
| **Valgrind** | C/C++ | Instrumentación binaria (runtime) | 10-50x | Leaks, UAF, double free, reads no inicializados, invalid writes |
| **ASan** | C/C++ | Instrumentación del compilador | 2-3x | UAF, double free, buffer overflow, stack overflow, leaks |
| **Miri** | Rust | Intérprete de MIR | ~100x | UB en unsafe: UAF, double free, data races, invalid alignment |

### 1.2 Cuándo usar cada una

```
¿Es C/C++?
├── Sí → ¿Puedes recompilar?
│         ├── Sí → ASan (más rápido, integrado en CI)
│         └── No → Valgrind (funciona con binarios existentes)
└── No → ¿Es Rust?
          ├── Safe Rust → No necesitas herramientas (el compilador lo previene)
          └── Unsafe Rust → Miri (detecta UB en unsafe blocks)
```

### 1.3 Qué detecta cada una

| Error | Valgrind | ASan | Miri |
|-------|:--------:|:----:|:----:|
| Memory leak | ✓ | ✓ (LeakSan) | ✓ |
| Use-after-free | ✓ | ✓ | ✓ |
| Double free | ✓ | ✓ | ✓ |
| Buffer overflow (heap) | ✓ | ✓ | ✓ |
| Buffer overflow (stack) | ✗ | ✓ | ✓ |
| Uninitialized read | ✓ | ✗ | ✓ |
| Invalid alignment | ✗ | ✗ | ✓ |
| Data races | ✓ (Helgrind/DRD) | ✓ (TSan) | ✓ |
| Invalid pointer arithmetic | ✗ | ✗ | ✓ |

---

## 2. Valgrind memcheck

### 2.1 Cómo funciona

Valgrind ejecuta el programa en una CPU **virtual** (no modifica el
binario, lo traduce instrucción por instrucción). Memcheck mantiene
un **shadow memory** que rastrea dos propiedades por cada byte:

1. **Addressability**: ¿es válido acceder a esta dirección?
2. **Definedness**: ¿el byte tiene un valor definido?

```
Memoria real:     [ 42 ][ ?? ][ FF ][ 00 ]
Shadow (addr):    [ OK ][ OK ][ FREED ][ OK ]
Shadow (defined): [ DEF ][ UNDEF ][ -- ][ DEF ]
```

### 2.2 Uso básico

```bash
# Compilar con símbolos de debug
gcc -g -O0 -o program program.c

# Ejecutar con Valgrind
valgrind --leak-check=full --show-leak-kinds=all \
         --track-origins=yes ./program
```

| Flag | Efecto |
|------|--------|
| `--leak-check=full` | Reporte detallado de leaks |
| `--show-leak-kinds=all` | Mostrar todos los tipos de leak |
| `--track-origins=yes` | Rastrear origen de valores no inicializados |
| `--error-exitcode=1` | Retornar exitcode 1 si hay errores (para CI) |
| `-q` | Modo silencioso (solo errores) |

### 2.3 Interpretar la salida

```
==1234== Invalid read of size 4          ← tipo de error
==1234==    at 0x...: main (prog.c:15)   ← dónde ocurrió
==1234==  Address 0x... is 0 bytes       ← info del bloque
==1234==    inside a block of size 4 free'd
==1234==    at 0x...: free (...)         ← dónde se liberó
==1234==    by 0x...: main (prog.c:14)
==1234==  Block was alloc'd at           ← dónde se asignó
==1234==    at 0x...: malloc (...)
==1234==    by 0x...: main (prog.c:10)
```

Tres secciones clave:
1. **Qué**: tipo de error (Invalid read, Invalid free, etc.)
2. **Dónde**: stack trace del error.
3. **Contexto**: cuándo se asignó y liberó el bloque.

### 2.4 Categorías de leaks

```
==1234== LEAK SUMMARY:
==1234==    definitely lost: 48 bytes in 3 blocks
==1234==    indirectly lost: 64 bytes in 4 blocks
==1234==      possibly lost: 16 bytes in 1 blocks
==1234==    still reachable: 8 bytes in 1 blocks
==1234==         suppressed: 0 bytes in 0 blocks
```

| Categoría | Significado | Acción |
|-----------|-------------|--------|
| **definitely lost** | Sin puntero al bloque | Arreglar siempre |
| **indirectly lost** | Solo accesible desde bloque lost | Se arregla al arreglar el definitely |
| **possibly lost** | Puntero al interior (no al inicio) | Investigar |
| **still reachable** | Hay punteros, no se liberó al exit | Usualmente ignorable |

---

## 3. AddressSanitizer (ASan)

### 3.1 Cómo funciona

ASan instrumenta el código **durante la compilación**. Inserta
checks antes de cada acceso a memoria:

```
Código original:          Código instrumentado:
*p = 42;                  if (is_poisoned(p))
                              report_error();
                          *p = 42;
```

ASan mantiene **shadow memory** (1 byte shadow por 8 bytes reales)
que indica si cada grupo de 8 bytes es accesible.

### 3.2 Red zones y quarantine

**Red zones**: bytes "envenenados" alrededor de cada asignación para
detectar overflows:

```
[red zone][    datos asignados    ][red zone]
 poisoned       accesible           poisoned

Acceder al red zone = "heap-buffer-overflow"
```

**Quarantine**: bloques liberados no se reciclan inmediatamente. Se
mantienen en una cola y se marcan como "poisoned". Acceder a un
bloque en quarantine = "heap-use-after-free".

### 3.3 Uso

```bash
# Compilar con ASan
gcc -fsanitize=address -g -O1 -o program program.c

# Ejecutar (ASan activado automáticamente)
./program

# Con LeakSanitizer (incluido en ASan por defecto en Linux)
ASAN_OPTIONS=detect_leaks=1 ./program
```

| Flag | Efecto |
|------|--------|
| `-fsanitize=address` | Activar ASan |
| `-fno-omit-frame-pointer` | Stack traces más completos |
| `-O1` | Optimización mínima (ASan funciona con -O0 a -O2) |
| `-g` | Símbolos de debug |

### 3.4 Comparación ASan vs Valgrind

| Aspecto | ASan | Valgrind |
|---------|:----:|:--------:|
| Overhead | 2-3x | 10-50x |
| Requiere recompilar | Sí | No |
| Stack overflow | ✓ | ✗ |
| Uninitialized reads | ✗ (usar MSan) | ✓ |
| Multithread | ✗ (usar TSan) | ✓ (Helgrind) |
| Precisión | Byte-level | Byte-level |
| Uso en CI | Excelente | Posible pero lento |
| Plataformas | Linux, macOS, Windows | Linux, macOS |

---

## 4. Miri (Rust)

### 4.1 Qué es Miri

Miri es un **intérprete de MIR** (Mid-level Intermediate
Representation) de Rust. Ejecuta el programa paso a paso, verificando
cada operación contra las reglas de **Stacked Borrows** y el modelo
de memoria de Rust.

### 4.2 Qué detecta

- **Use-after-free** con raw pointers en `unsafe`.
- **Double free** con raw pointers.
- **Invalid alignment** (acceder a un `u32` en dirección no alineada a 4).
- **Out-of-bounds** access.
- **Data races** (accesos concurrentes sin sincronización).
- **Violación de Stacked Borrows** (usar una referencia invalidada
  por otra operación unsafe).
- **Integer overflow** en debug mode.

### 4.3 Uso

```bash
# Instalar Miri (requiere nightly)
rustup +nightly component add miri

# Ejecutar con Miri
cargo +nightly miri run

# Ejecutar tests con Miri
cargo +nightly miri test
```

### 4.4 Ejemplo de detección

```rust
fn main() {
    unsafe {
        let p = Box::into_raw(Box::new(42));
        drop(Box::from_raw(p));
        println!("{}", *p);  // UAF
    }
}
```

```
error: Undefined Behavior: dereferencing pointer at alloc123
       which was deallocated
  --> src/main.rs:5:24
   |
5  |         println!("{}", *p);
   |                        ^^ dereferencing pointer at alloc123
   |                           which was deallocated
```

### 4.5 Limitaciones de Miri

| Limitación | Detalle |
|-----------|---------|
| Velocidad | ~100x más lento (interpreta cada instrucción) |
| FFI | No puede ejecutar código C (llamadas a libc limitadas) |
| Threads | Soporte limitado (simula scheduling) |
| I/O | Soporte parcial (filesystem, stdin/stdout) |
| Cobertura | Solo detecta UB que se **ejecuta** (no análisis estático) |

---

## 5. Flujo de trabajo integrado

### 5.1 Checklist antes de commit

```
Código C:
  □ Compilar con -Wall -Wextra -Werror
  □ Ejecutar tests con ASan: -fsanitize=address
  □ Ejecutar Valgrind en escenarios complejos
  □ Verificar 0 leaks en LEAK SUMMARY

Código Rust:
  □ cargo clippy (lints estáticos)
  □ cargo test (safe Rust — ya es seguro)
  □ cargo +nightly miri test (si hay unsafe)
  □ Verificar 0 warnings de clippy
```

### 5.2 Integración en CI

```yaml
# .github/workflows/memory.yml (ejemplo conceptual)
jobs:
  asan:
    runs-on: ubuntu-latest
    steps:
      - run: gcc -fsanitize=address -g -o test test.c
      - run: ./test

  valgrind:
    runs-on: ubuntu-latest
    steps:
      - run: gcc -g -o test test.c
      - run: valgrind --error-exitcode=1 --leak-check=full ./test

  miri:
    runs-on: ubuntu-latest
    steps:
      - run: rustup +nightly component add miri
      - run: cargo +nightly miri test
```

### 5.3 Estrategia por fase del proyecto

| Fase | C | Rust |
|------|---|------|
| Desarrollo | ASan siempre activado | `cargo clippy` + `cargo test` |
| Pre-commit | Valgrind (más exhaustivo) | `cargo miri test` (si unsafe) |
| CI | ASan + Valgrind | clippy + test + miri |
| Producción | Sin sanitizers (overhead) | Release build (seguro por diseño) |

---

## 6. Otros sanitizers (familia *San)

### 6.1 Tabla de sanitizers

| Sanitizer | Flag | Detecta |
|-----------|------|---------|
| **ASan** | `-fsanitize=address` | UAF, double free, overflow, leaks |
| **MSan** | `-fsanitize=memory` | Lecturas de memoria no inicializada |
| **TSan** | `-fsanitize=thread` | Data races |
| **UBSan** | `-fsanitize=undefined` | Undefined behavior (overflow, null deref, etc.) |

### 6.2 Regla importante

**No combinar ASan con MSan ni TSan** — son incompatibles (usan
shadow memory de formas conflictivas). Se ejecutan por separado:

```bash
# Run 1: ASan (memory errors)
gcc -fsanitize=address -g -o test_asan test.c && ./test_asan

# Run 2: MSan (uninitialized reads)
# Solo clang soporta MSan
clang -fsanitize=memory -g -o test_msan test.c && ./test_msan

# Run 3: TSan (data races)
gcc -fsanitize=thread -g -o test_tsan test.c && ./test_tsan

# UBSan se puede combinar con los demás
gcc -fsanitize=address,undefined -g -o test test.c && ./test
```

---

## 7. Programa en C — Errores detectados por herramientas

Este programa contiene errores **intencionales** que Valgrind y ASan
detectan. Cada demo muestra un tipo de error y el reporte esperado.

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 *  Memory Error Demonstrations for Valgrind/ASan
 *
 *  Compile normally:  gcc -g -O0 -o memtools memtools.c
 *  With ASan:         gcc -fsanitize=address -g -o memtools memtools.c
 *  With Valgrind:     valgrind --leak-check=full ./memtools <demo>
 *
 *  Usage: ./memtools <1-6>
 *  Each demo triggers a different memory error.
 * ============================================================ */

/* ============================================================
 *  Demo 1: Memory leak — forgotten free
 * ============================================================ */
void demo1_leak(void) {
    printf("=== Demo 1: Memory leak ===\n");

    int *arr = malloc(10 * sizeof(int));
    for (int i = 0; i < 10; i++)
        arr[i] = i * 10;

    printf("  arr[5] = %d\n", arr[5]);

    /* Intentionally NOT freeing arr */
    printf("  Exiting without free(arr) — leak!\n");
    printf("\n  Expected Valgrind output:\n");
    printf("    definitely lost: 40 bytes in 1 blocks\n");
    printf("    at malloc -> demo1_leak (memtools.c:line)\n");

    /* To fix: free(arr); */
}

/* ============================================================
 *  Demo 2: Use-after-free
 * ============================================================ */
void demo2_uaf(void) {
    printf("=== Demo 2: Use-after-free ===\n");

    char *name = malloc(16);
    strcpy(name, "Alice");
    printf("  name = %s\n", name);

    free(name);
    printf("  Freed name.\n");

    /* USE-AFTER-FREE: reading from freed memory */
    printf("  Reading freed memory: %c\n", name[0]);

    printf("\n  Expected ASan output:\n");
    printf("    heap-use-after-free on address 0x...\n");
    printf("    READ of size 1\n");
}

/* ============================================================
 *  Demo 3: Double free
 * ============================================================ */
void demo3_double_free(void) {
    printf("=== Demo 3: Double free ===\n");

    int *p = malloc(sizeof(int));
    *p = 42;
    printf("  *p = %d\n", *p);

    free(p);
    printf("  First free OK.\n");

    free(p);  /* DOUBLE FREE */
    printf("  Second free — should have been caught!\n");

    printf("\n  Expected output:\n");
    printf("    Valgrind: Invalid free()\n");
    printf("    ASan: attempting double-free\n");
    printf("    glibc: double free or corruption\n");
}

/* ============================================================
 *  Demo 4: Heap buffer overflow
 * ============================================================ */
void demo4_overflow(void) {
    printf("=== Demo 4: Heap buffer overflow ===\n");

    char *buf = malloc(8);
    strcpy(buf, "Hello");
    printf("  buf = %s (within bounds)\n", buf);

    /* Write beyond allocated size */
    strcpy(buf, "This string is way too long for 8 bytes!");
    printf("  Wrote beyond buffer!\n");

    printf("\n  Expected ASan output:\n");
    printf("    heap-buffer-overflow on address 0x...\n");
    printf("    WRITE of size 41\n");

    free(buf);
}

/* ============================================================
 *  Demo 5: Uninitialized read
 * ============================================================ */
void demo5_uninit(void) {
    printf("=== Demo 5: Uninitialized read ===\n");

    int *arr = malloc(5 * sizeof(int));
    /* NOT initializing arr */

    int sum = 0;
    for (int i = 0; i < 5; i++)
        sum += arr[i];  /* Reading uninitialized memory */

    printf("  sum = %d (from uninitialized data)\n", sum);

    printf("\n  Expected Valgrind output:\n");
    printf("    Conditional jump depends on uninitialised value\n");
    printf("    (or: Use of uninitialised value of size 4)\n");
    printf("  Note: ASan does NOT detect this. Use MSan or Valgrind.\n");

    free(arr);
}

/* ============================================================
 *  Demo 6: All-in-one — multiple errors in one function
 * ============================================================ */
void demo6_combined(void) {
    printf("=== Demo 6: Combined errors ===\n");

    /* Leak */
    int *leaked = malloc(32);
    leaked[0] = 1;
    printf("  Allocated 'leaked' (will not free)\n");

    /* UAF */
    char *temp = malloc(16);
    strcpy(temp, "temporary");
    free(temp);
    printf("  temp after free: %c (UAF)\n", temp[0]);

    /* Overflow */
    char *small = malloc(4);
    small[4] = 'X';  /* one byte past the end */
    printf("  Wrote 1 byte past allocation\n");
    free(small);

    printf("\n  Valgrind/ASan would report:\n");
    printf("    1. heap-use-after-free (temp[0])\n");
    printf("    2. heap-buffer-overflow (small[4])\n");
    printf("    3. definitely lost: 32 bytes (leaked)\n");
    printf("  Three errors in one function!\n");

    (void)leaked;  /* suppress unused warning */
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <demo 1-6>\n", argv[0]);
        printf("  1: Memory leak\n");
        printf("  2: Use-after-free\n");
        printf("  3: Double free\n");
        printf("  4: Heap buffer overflow\n");
        printf("  5: Uninitialized read\n");
        printf("  6: Combined errors\n");
        printf("\nRun with:\n");
        printf("  valgrind --leak-check=full ./memtools 1\n");
        printf("  gcc -fsanitize=address -g -o memtools memtools.c "
               "&& ./memtools 2\n");
        return 0;
    }

    int demo = atoi(argv[1]);
    switch (demo) {
        case 1: demo1_leak();        break;
        case 2: demo2_uaf();         break;
        case 3: demo3_double_free(); break;
        case 4: demo4_overflow();    break;
        case 5: demo5_uninit();      break;
        case 6: demo6_combined();    break;
        default:
            printf("Unknown demo: %d (use 1-6)\n", demo);
            return 1;
    }

    return 0;
}
```

### Compilar y ejecutar

```bash
# Compilación normal (para Valgrind)
gcc -std=c11 -Wall -Wextra -g -O0 -o memtools memtools.c

# Demo 1: leak (Valgrind)
valgrind --leak-check=full ./memtools 1

# Demo 2: UAF (ASan — más rápido)
gcc -fsanitize=address -g -o memtools_asan memtools.c
./memtools_asan 2

# Demo 5: uninitialized (solo Valgrind detecta esto)
valgrind --track-origins=yes ./memtools 5
```

### Salida esperada — Valgrind demo 1 (leak)

```
==PID== HEAP SUMMARY:
==PID==     in use at exit: 40 bytes in 1 blocks
==PID==   total heap usage: 2 allocs, 1 frees, 1,064 bytes allocated
==PID==
==PID== 40 bytes in 1 blocks are definitely lost in loss record 1 of 1
==PID==    at 0x...: malloc (vg_replace_malloc.c:...)
==PID==    by 0x...: demo1_leak (memtools.c:30)
==PID==    by 0x...: main (memtools.c:128)
==PID==
==PID== LEAK SUMMARY:
==PID==    definitely lost: 40 bytes in 1 blocks
```

### Salida esperada — ASan demo 2 (UAF)

```
=================================================================
==PID==ERROR: AddressSanitizer: heap-use-after-free on address 0x...
READ of size 1 at 0x... thread T0
    #0 0x... in demo2_uaf memtools.c:52
    #1 0x... in main memtools.c:129
0x... is located 0 bytes inside of 16-byte region
freed by thread T0 here:
    #0 0x... in free
    #1 0x... in demo2_uaf memtools.c:49
previously allocated by thread T0 here:
    #0 0x... in malloc
    #1 0x... in demo2_uaf memtools.c:45
```

---

## 8. Programa en Rust — Miri y safe Rust

```rust
/* ============================================================
 *  Memory Safety Tools in Rust
 *
 *  Safe Rust: no tools needed (compiler prevents errors)
 *  Unsafe Rust: use Miri (cargo +nightly miri run)
 * ============================================================ */

/* ============================================================
 *  Demo 1: Safe Rust — compiler prevents everything
 * ============================================================ */
fn demo1() {
    println!("\n=== Demo 1: Safe Rust needs no tools ===");

    // The compiler prevents all memory errors at compile time:

    // 1. Memory leak (mostly prevented by Drop)
    {
        let v = vec![1, 2, 3];
        println!("  vec = {:?}", v);
    } // v dropped — no leak

    // 2. Use-after-free (prevented by borrow checker)
    // let r;
    // { let x = 42; r = &x; } // ERROR: x doesn't live long enough
    // println!("{}", r);

    // 3. Double free (prevented by move semantics)
    // let a = Box::new(42); let b = a; drop(a); // ERROR: moved

    // 4. Buffer overflow (prevented by bounds checking)
    let arr = vec![1, 2, 3];
    // arr[10]; // panics at runtime (not UB — controlled crash)
    println!("  arr.get(10) = {:?} (None, not UB)", arr.get(10));

    // 5. Uninitialized read (prevented — all vars must be initialized)
    // let x: i32; println!("{}", x); // ERROR: use of uninitialized

    println!("  All 5 error types prevented by the compiler.");
    println!("  No Valgrind, no ASan, no runtime overhead.");
}

/* ============================================================
 *  Demo 2: Unsafe Rust — where Miri is needed
 * ============================================================ */
fn demo2() {
    println!("\n=== Demo 2: Unsafe Rust — Miri territory ===");

    // Safe unsafe: correct usage of raw pointers
    let value = 42;
    let ptr: *const i32 = &value;
    let read = unsafe { *ptr };
    println!("  Safe raw pointer read: {}", read);

    // Miri would catch THESE (commented out to avoid UB):
    println!("\n  Examples Miri catches (if uncommented):");
    println!("  1. Use-after-free:");
    println!("     let p = Box::into_raw(Box::new(42));");
    println!("     drop(Box::from_raw(p));");
    println!("     *p  // Miri: dereferencing deallocated pointer");

    println!("  2. Out-of-bounds:");
    println!("     let arr = [1, 2, 3];");
    println!("     let p = arr.as_ptr();");
    println!("     *p.add(10)  // Miri: out-of-bounds pointer");

    println!("  3. Invalid alignment:");
    println!("     let bytes: [u8; 4] = [0; 4];");
    println!("     let p = bytes.as_ptr() as *const u32;");
    println!("     *p  // Miri: accessing with wrong alignment");
}

/* ============================================================
 *  Demo 3: Bounds checking — runtime safety
 * ============================================================ */
fn demo3() {
    println!("\n=== Demo 3: Bounds checking ===");

    let v = vec![10, 20, 30, 40, 50];

    // Safe access with bounds checking
    println!("  v[2] = {} (checked, panics on OOB)", v[2]);
    println!("  v.get(2) = {:?} (returns Option)", v.get(2));
    println!("  v.get(99) = {:?} (returns None)", v.get(99));

    // Iterate safely
    let sum: i32 = v.iter().sum();
    println!("  sum = {} (iterator — no index needed)", sum);

    println!("\n  In C: arr[99] reads garbage (UB, no bounds check).");
    println!("  In Rust: v[99] panics (controlled crash, not UB).");
    println!("  v.get(99) returns None (no crash, no UB).");
}

/* ============================================================
 *  Demo 4: Clippy — static analysis
 * ============================================================ */
fn demo4() {
    println!("\n=== Demo 4: Clippy — static analysis ===");

    println!("  cargo clippy catches code smells:");
    println!();

    // Examples of what clippy catches:
    // 1. Unnecessary clone
    let s = String::from("hello");
    let _r = &s;  // clippy would warn if we did s.clone() unnecessarily
    println!("  1. Unnecessary .clone() when a borrow suffices");

    // 2. Manual implementation of what a method does
    let v = vec![1, 2, 3];
    let _has_two = v.iter().any(|&x| x == 2);
    // vs: v.contains(&2) — clippy suggests this
    println!("  2. Use .contains() instead of .iter().any()");

    // 3. Using unwrap() in production code
    let maybe: Option<i32> = Some(42);
    let _val = maybe.unwrap_or(0);
    println!("  3. Use .unwrap_or() instead of .unwrap()");

    println!();
    println!("  Run: cargo clippy -- -W clippy::all");
    println!("  Clippy has 600+ lints for correctness, style,");
    println!("  performance, and complexity.");
}

/* ============================================================
 *  Demo 5: The Rust safety pyramid
 * ============================================================ */
fn demo5() {
    println!("\n=== Demo 5: The Rust safety pyramid ===");

    println!("  Layer 4 (top):  Miri          — unsafe UB detection");
    println!("  Layer 3:        Clippy        — static analysis / lints");
    println!("  Layer 2:        cargo test    — logic correctness");
    println!("  Layer 1 (base): Compiler      — type/borrow/lifetime");
    println!();
    println!("  ┌─────────────────────────────┐");
    println!("  │          Miri               │ ← only for unsafe");
    println!("  ├─────────────────────────────┤");
    println!("  │         Clippy              │ ← all code");
    println!("  ├─────────────────────────────┤");
    println!("  │       cargo test            │ ← all code");
    println!("  ├─────────────────────────────┤");
    println!("  │     Compiler (rustc)        │ ← all code");
    println!("  │  ownership + borrowing +    │");
    println!("  │  lifetimes + type system    │");
    println!("  └─────────────────────────────┘");
    println!();
    println!("  Most Rust code never needs beyond Layer 2.");
    println!("  The compiler catches what Valgrind/ASan catch in C.");
}

/* ============================================================
 *  Demo 6: Workflow comparison — C vs Rust
 * ============================================================ */
fn demo6() {
    println!("\n=== Demo 6: Workflow — C vs Rust ===");

    println!("  C workflow:");
    println!("  ┌──────────────────────────────────────────────┐");
    println!("  │ 1. Write code                                │");
    println!("  │ 2. gcc -Wall -Wextra -g program.c            │");
    println!("  │ 3. Run tests                                 │");
    println!("  │ 4. gcc -fsanitize=address program.c          │");
    println!("  │ 5. Run tests with ASan                       │");
    println!("  │ 6. valgrind --leak-check=full ./program      │");
    println!("  │ 7. Fix leaks, UAF, double free               │");
    println!("  │ 8. Repeat 4-7 until clean                    │");
    println!("  │ 9. Commit                                    │");
    println!("  └──────────────────────────────────────────────┘");
    println!();
    println!("  Rust workflow:");
    println!("  ┌──────────────────────────────────────────────┐");
    println!("  │ 1. Write code                                │");
    println!("  │ 2. cargo build  (compiler catches mem errors) │");
    println!("  │ 3. cargo clippy (static analysis)            │");
    println!("  │ 4. cargo test                                │");
    println!("  │ 5. Commit                                    │");
    println!("  │                                              │");
    println!("  │ (If unsafe code: add cargo miri test)        │");
    println!("  └──────────────────────────────────────────────┘");
    println!();
    println!("  C: 9 steps, multiple tool runs, fix-rerun cycles.");
    println!("  Rust: 5 steps, memory safety guaranteed at step 2.");
}

/* ============================================================
 *  Demo 7: Common Valgrind/ASan errors decoded
 * ============================================================ */
fn demo7() {
    println!("\n=== Demo 7: Error message decoder ===");

    println!("  Valgrind messages and what they mean:");
    println!();
    println!("  Message                              | Error");
    println!("  -------------------------------------|------------------");
    println!("  Invalid read of size N               | Use-after-free");
    println!("  Invalid write of size N              | Write-after-free");
    println!("  Invalid free()                       | Double free");
    println!("  Conditional jump on uninitialised    | Uninitialized read");
    println!("  definitely lost: N bytes             | Memory leak");
    println!("  indirectly lost: N bytes             | Leak (reachable");
    println!("                                       |  from lost block)");
    println!();
    println!("  ASan messages and what they mean:");
    println!();
    println!("  Message                              | Error");
    println!("  -------------------------------------|------------------");
    println!("  heap-use-after-free                  | Use-after-free");
    println!("  attempting double-free               | Double free");
    println!("  heap-buffer-overflow                 | Write past end");
    println!("  stack-buffer-overflow                | Stack overflow");
    println!("  detected memory leaks                | Memory leak");
    println!();
    println!("  Miri messages and what they mean:");
    println!();
    println!("  Message                              | Error");
    println!("  -------------------------------------|------------------");
    println!("  dereferencing deallocated pointer     | Use-after-free");
    println!("  out-of-bounds pointer                | Buffer overflow");
    println!("  accessing with wrong alignment       | Alignment error");
    println!("  data race detected                   | Data race");
}

/* ============================================================
 *  Demo 8: Pre-commit checklist
 * ============================================================ */
fn demo8() {
    println!("\n=== Demo 8: Pre-commit checklist ===");

    println!("  C code checklist:");
    println!("  [x] Compile with -Wall -Wextra -Werror");
    println!("  [x] Run tests");
    println!("  [x] Compile with -fsanitize=address");
    println!("  [x] Run tests under ASan (0 errors)");
    println!("  [x] Run Valgrind --leak-check=full (0 leaks)");
    println!("  [x] Check: every malloc has a matching free");
    println!("  [x] Check: no pointer used after free");
    println!("  [x] Check: free(p); p = NULL; pattern used");
    println!();

    println!("  Rust code checklist:");
    println!("  [x] cargo build (0 errors)");
    println!("  [x] cargo clippy (0 warnings)");
    println!("  [x] cargo test (all pass)");
    println!("  [x] If unsafe: cargo +nightly miri test (0 UB)");
    println!("  [x] cargo fmt (consistent style)");
    println!();

    println!("  Key difference:");
    println!("  C: checklist needed because compiler doesn't help.");
    println!("  Rust: step 1 (cargo build) catches most issues.");
    println!("  The rest is for logic bugs, not memory bugs.");
}

fn main() {
    demo1();
    demo2();
    demo3();
    demo4();
    demo5();
    demo6();
    demo7();
    demo8();
}
```

### Compilar y ejecutar

```bash
rustc -o memtools_rs memtools.rs
./memtools_rs
```

### Salida esperada (extracto)

```
=== Demo 1: Safe Rust needs no tools ===
  vec = [1, 2, 3]
  arr.get(10) = None (None, not UB)
  All 5 error types prevented by the compiler.
  No Valgrind, no ASan, no runtime overhead.

=== Demo 5: The Rust safety pyramid ===
  Layer 4 (top):  Miri          — unsafe UB detection
  Layer 3:        Clippy        — static analysis / lints
  Layer 2:        cargo test    — logic correctness
  Layer 1 (base): Compiler      — type/borrow/lifetime

  Most Rust code never needs beyond Layer 2.

=== Demo 6: Workflow — C vs Rust ===
  C: 9 steps, multiple tool runs, fix-rerun cycles.
  Rust: 5 steps, memory safety guaranteed at step 2.
```

---

## 9. Ejercicios

### Ejercicio 1: Interpretar Valgrind

Valgrind reporta:

```
==PID== Invalid read of size 8
==PID==    at 0x...: process (main.c:25)
==PID==  Address 0x... is 16 bytes inside a block of size 32 free'd
==PID==    at 0x...: free (vg_replace_malloc.c:...)
==PID==    by 0x...: cleanup (main.c:20)
```

Qué error es? En qué línea se liberó? En qué línea se accedió?

<details><summary>Prediccion</summary>

Es un **use-after-free**. El bloque fue:
- **Asignado**: (no mostrado en este extracto, sería más abajo).
- **Liberado**: en `cleanup` (main.c:20) con `free()`.
- **Accedido**: en `process` (main.c:25) con una lectura de 8 bytes.

La lectura ocurre en el offset 16 dentro del bloque de 32 bytes.
Esto sugiere acceso a un campo de un struct:

```c
// main.c:20 (cleanup)
free(record);

// main.c:25 (process) — after cleanup was called
long val = record->field_at_offset_16;  // UAF
```

Fix: no llamar `process` después de `cleanup`, o copiar los datos
necesarios antes de liberar.

</details>

### Ejercicio 2: ASan vs Valgrind

Tienes un programa que lee memoria no inicializada. Qué herramienta
lo detecta: ASan o Valgrind?

<details><summary>Prediccion</summary>

**Valgrind** lo detecta. ASan **no**.

ASan detecta errores de **addressability** (acceder a memoria no
asignada, liberada, o fuera de límites). Pero no rastrea si un byte
fue **inicializado** o no.

Para lecturas no inicializadas:
- **Valgrind memcheck**: detecta con shadow memory que rastrea
  "definedness" de cada byte. Reporta "Conditional jump depends on
  uninitialised value" o "Use of uninitialised value".
- **MSan** (MemorySanitizer, solo Clang): detecta con
  instrumentación del compilador. Más rápido que Valgrind.
- **ASan**: no detecta.

```bash
# Valgrind detecta:
valgrind --track-origins=yes ./program

# MSan detecta (solo clang):
clang -fsanitize=memory -g -o program program.c
./program
```

</details>

### Ejercicio 3: Miri vs compilador

Qué errores detecta **Miri** que el compilador de Rust no detecta?

<details><summary>Prediccion</summary>

El compilador de Rust previene errores de memoria en **safe code**.
Miri detecta errores en **unsafe code** que el compilador no verifica:

1. **Use-after-free con raw pointers**: `unsafe { *freed_ptr }`
   compila pero es UB.
2. **Out-of-bounds con pointer arithmetic**: `unsafe { *ptr.add(100) }`
   compila pero es UB si el offset es inválido.
3. **Invalid alignment**: `unsafe { *(misaligned_ptr as *const u32) }`
   compila pero es UB.
4. **Stacked Borrows violation**: crear una `&mut` a través de un
   `*mut` que ya tiene un `&` activo.
5. **Data races**: dos `unsafe` accesos concurrentes sin
   sincronización.
6. **Integer overflow**: puede ser UB en ciertos contextos.

En resumen: Miri es el "Valgrind de Rust" pero solo para código
`unsafe`. Safe Rust no necesita Miri porque el compilador ya
garantiza la ausencia de estos errores.

</details>

### Ejercicio 4: Red zones de ASan

ASan pone "red zones" alrededor de las asignaciones. Si malloc(8)
retorna un bloque, cómo se ve la memoria con ASan?

<details><summary>Prediccion</summary>

```
Dirección:  ...00  ...08  ...10  ...18  ...20  ...28  ...30
            [rz_L] [data ] [data ] [rz_R] [rz_R]
Shadow:     [0xFF] [0x00 ] [0x00 ] [0xFF] [0xFF]
             ^      ^       ^       ^
             red     accesible       red zone
             zone    (8 bytes)       derecho
             izq
```

Los red zones son regiones "envenenadas" (shadow byte = 0xFF) que
rodean la asignación. Si el programa escribe en buf[8] (un byte
después del final), accede al red zone derecho y ASan reporta:

```
heap-buffer-overflow on address 0x...18
WRITE of size 1
```

Tamaño típico de red zones: 16-64 bytes a cada lado. Esto permite
detectar overflows de hasta esa distancia. Un overflow que salta
más allá del red zone podría no ser detectado (raro en la práctica).

</details>

### Ejercicio 5: Valgrind quarantine vs ASan quarantine

Tanto Valgrind como ASan mantienen bloques liberados en "quarantine"
para detectar UAF. Cuál es la diferencia?

<details><summary>Prediccion</summary>

**Valgrind**: marca los bloques liberados como "no accesibles" en
su shadow memory. Si el programa lee/escribe en un bloque liberado,
Valgrind lo detecta. El bloque se recicla normalmente por el
allocator — Valgrind solo intercepta los accesos.

**ASan**: reemplaza malloc/free con sus propios. Los bloques
liberados entran en una **cola de quarantine** y se marcan como
"poisoned" en el shadow memory. Los bloques **no se reciclan**
inmediatamente — permanecen en quarantine durante un tiempo
configurable (`ASAN_OPTIONS=quarantine_size_mb=256`).

Diferencia clave: ASan evita que el allocator **reutilice** el
bloque rápidamente, haciendo más probable detectar UAF. Si el bloque
se reutiliza inmediatamente (como en el allocator normal), un UAF
podría leer datos válidos de la nueva asignación y pasar
desapercibido. La quarantine de ASan previene esto.

Valgrind siempre detecta UAF porque instrumenta cada acceso a
memoria, sin importar si el bloque fue reutilizado o no.

</details>

### Ejercicio 6: CI pipeline

Diseña un pipeline de CI para un proyecto que tiene código C y Rust:

<details><summary>Prediccion</summary>

```yaml
jobs:
  # ---- C code ----
  c-build:
    steps:
      - gcc -Wall -Wextra -Werror -g -o program src/*.c

  c-asan:
    steps:
      - gcc -fsanitize=address,undefined -g -o program src/*.c
      - ./program  # ASan + UBSan activos
      # Falla si hay UAF, double free, overflow, o UB

  c-valgrind:
    steps:
      - gcc -g -O0 -o program src/*.c
      - valgrind --error-exitcode=1 --leak-check=full ./program
      # Falla si hay leaks o errores de memoria

  # ---- Rust code ----
  rust-build:
    steps:
      - cargo build
      - cargo clippy -- -D warnings  # warnings = errors

  rust-test:
    steps:
      - cargo test

  rust-miri:  # solo si hay unsafe
    steps:
      - rustup +nightly component add miri
      - cargo +nightly miri test
```

Orden de ejecución recomendado:
1. Build (rápido, detecta errores de compilación).
2. Clippy/warnings (rápido, detecta code smells).
3. Tests (medio, detecta errores lógicos).
4. ASan (medio, detecta errores de memoria).
5. Valgrind/Miri (lento, detección exhaustiva).

Los pasos rápidos primero para feedback rápido.

</details>

### Ejercicio 7: Falso negativo

Un programa tiene un UAF pero ni Valgrind ni ASan lo reportan.
Cómo es posible?

<details><summary>Prediccion</summary>

Posibles razones:

1. **El código UAF no se ejecutó**: Valgrind y ASan solo detectan
   errores que **ocurren** durante la ejecución. Si el path con
   UAF no se ejecuta en el test, no se detecta. Son herramientas
   **dinámicas**, no estáticas.

2. **El bloque fue reutilizado antes de la quarantine**: poco
   probable con ASan (tiene quarantine), pero posible con Valgrind
   si el acceso lee datos que coinciden con los esperados.

3. **El UAF ocurre en mmap'd memory**: Valgrind puede no rastrear
   regiones mapeadas con mmap directamente (depende de la config).

4. **Timing-dependent**: en código multithread, el UAF puede depender
   de un scheduling específico que no ocurre en testing.

Solución: aumentar cobertura de tests, usar fuzzing (AFL, libFuzzer)
para explorar más paths, y combinar herramientas dinámicas con
análisis estático (Coverity, clang-tidy).

En Rust safe: imposible — el compilador detecta UAF
**estáticamente** en todos los paths, no solo los ejecutados.

</details>

### Ejercicio 8: Suppressions de Valgrind

Valgrind reporta "still reachable: 72,704 bytes" en un programa
simple que no tiene leaks. De dónde vienen?

<details><summary>Prediccion</summary>

Los 72,704 bytes son **buffers internos de la libc/runtime** que
se asignan al inicio del programa y se liberan al terminar (o no,
porque el OS los reclama de todas formas).

Ejemplos comunes:
- Buffer de `stdout` (stdio).
- Tablas internas de glibc (locale, dl-open).
- Buffers de `printf` / `scanf`.

Estos son **"still reachable"** (hay punteros globales que apuntan
a ellos) y **no son bugs** del programa. Por eso Valgrind los
reporta como "still reachable" y no como "definitely lost".

Para ignorarlos:
```bash
# No mostrar "still reachable"
valgrind --show-leak-kinds=definite,possible ./program

# O usar un suppression file
valgrind --suppressions=my.supp ./program
```

En CI, usar `--show-leak-kinds=definite` para evitar falsos
positivos de la libc.

</details>

### Ejercicio 9: Miri y Stacked Borrows

```rust
fn main() {
    let mut x: i32 = 42;
    let raw: *mut i32 = &mut x;
    let reference: &i32 = &x;
    unsafe { *raw = 99; }
    println!("{}", reference);
}
```

Miri detecta un error aquí? Cuál?

<details><summary>Prediccion</summary>

**Sí, Miri detecta un error**: violación de **Stacked Borrows**.

El problema:
1. `raw = &mut x` crea un raw pointer mutable.
2. `reference = &x` crea una referencia inmutable.
3. `*raw = 99` escribe a través del raw pointer.

Bajo el modelo de Stacked Borrows, crear `&x` (referencia inmutable)
**invalida** el raw pointer mutable anterior. Escribir a través de
un raw pointer invalidado es undefined behavior.

Miri reporta:
```
error: Undefined Behavior: attempting a write access through
       a pointer that was derived from a SharedRef
```

Fix: no intercalar raw pointer writes con referencia immutable
accesses. O restructurar para que el raw pointer se use antes de
crear la referencia.

Este es un caso donde safe Rust no compila (no puedes tener `&mut`
y `&` simultáneamente), pero con raw pointers en unsafe puedes
evadir la verificación — Miri la recupera en runtime.

</details>

### Ejercicio 10: Herramienta ideal

Si pudieras diseñar una herramienta de verificación de memoria
perfecta, qué propiedades tendría?

<details><summary>Prediccion</summary>

La herramienta ideal combinaria:

| Propiedad | Valor ideal | Mejor actual |
|-----------|-------------|:------------:|
| Detección | Todos los errores | Miri (para Rust unsafe) |
| Falsos positivos | 0 | ASan (~0) |
| Falsos negativos | 0 | Compilador Rust (safe) |
| Overhead | 0% | Compilador Rust (0%) |
| Análisis | Estático (todos los paths) | Compilador Rust |
| Cuándo | Compile time | Compilador Rust |
| Lenguajes | Todos | Específico por herramienta |

La observación clave: **Rust ya se acerca mucho al ideal** para
safe code. El compilador:
- Detecta UAF, double free, data races, dangling refs.
- 0 falsos positivos, 0 falsos negativos (para safe code).
- 0% overhead en runtime.
- Análisis estático de todos los paths.
- En compile time.

La limitación: solo funciona para Rust y no cubre unsafe. Para C,
no existe una herramienta que combine análisis estático completo
con 0 falsos positivos — por eso se usan múltiples herramientas
(ASan + Valgrind + análisis estático + fuzzing).

El "diseño ideal" ya existe: es un **sistema de tipos** que previene
errores de memoria por construcción, como el ownership de Rust.

</details>
