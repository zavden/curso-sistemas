# cbindgen

## Índice

1. [¿Por qué cbindgen?](#1-por-qué-cbindgen)
2. [Instalación y setup](#2-instalación-y-setup)
3. [Uso desde línea de comandos](#3-uso-desde-línea-de-comandos)
4. [Uso desde build.rs](#4-uso-desde-buildrs)
5. [Configuración con cbindgen.toml](#5-configuración-con-cbindgentoml)
6. [Qué genera y qué no](#6-qué-genera-y-qué-no)
7. [Atributos para controlar la generación](#7-atributos-para-controlar-la-generación)
8. [Proyecto completo: Rust lib + header + C app](#8-proyecto-completo-rust-lib--header--c-app)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. ¿Por qué cbindgen?

En el tema anterior escribimos los headers `.h` a mano. Esto tiene los mismos problemas
que escribir `extern "C"` a mano en el otro sentido: es tedioso, propenso a errores, y
se desincroniza cuando cambia el código Rust. `cbindgen` es el espejo de `bindgen`:

```
┌─── bindgen vs cbindgen ───────────────────────────────────┐
│                                                            │
│  bindgen:                                                  │
│  header.h  ──→  bindings.rs                                │
│  (C → Rust)     genera extern "C" { fn ... }               │
│                                                            │
│  cbindgen:                                                 │
│  lib.rs    ──→  header.h                                   │
│  (Rust → C)     genera prototipos de funciones C           │
│                                                            │
│  Juntos forman el ciclo completo de FFI:                   │
│  C headers → bindgen → Rust bindings → wrappers            │
│  Rust functions → cbindgen → C headers → C code            │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Qué hace cbindgen

cbindgen lee tu código Rust y genera un header C (o C++) con:

- Prototipos de funciones marcadas con `#[no_mangle] pub extern "C"`
- Definiciones de structs `#[repr(C)]`
- Definiciones de enums `#[repr(C)]` o `#[repr(i32)]` etc.
- Typedefs
- Constantes `pub const`
- Include guards (`#ifndef / #define / #endif`)

---

## 2. Instalación y setup

### Instalar cbindgen CLI

```bash
cargo install cbindgen

cbindgen --version
```

### Como build-dependency

```toml
# Cargo.toml
[build-dependencies]
cbindgen = "0.27"
```

---

## 3. Uso desde línea de comandos

### Generar header básico

Dado este código Rust:

```rust
// src/lib.rs
use std::ffi::{c_int, c_double};

#[no_mangle]
pub extern "C" fn add(a: c_int, b: c_int) -> c_int {
    a + b
}

#[no_mangle]
pub extern "C" fn distance(x1: c_double, y1: c_double, x2: c_double, y2: c_double) -> c_double {
    ((x2 - x1).powi(2) + (y2 - y1).powi(2)).sqrt()
}

#[repr(C)]
pub struct Point {
    pub x: c_double,
    pub y: c_double,
}

#[no_mangle]
pub extern "C" fn point_distance(a: *const Point, b: *const Point) -> c_double {
    if a.is_null() || b.is_null() {
        return -1.0;
    }
    let a = unsafe { &*a };
    let b = unsafe { &*b };
    ((b.x - a.x).powi(2) + (b.y - a.y).powi(2)).sqrt()
}
```

```bash
# Generar header C
cbindgen --crate mylib --output mylib.h --lang c

# Generar header C++ (usa namespace, extern "C" block, etc.)
cbindgen --crate mylib --output mylib.hpp --lang c++
```

### Output generado

```c
/* mylib.h — generado por cbindgen */
#ifndef MYLIB_H
#define MYLIB_H

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct {
  double x;
  double y;
} Point;

int add(int a, int b);

double distance(double x1, double y1, double x2, double y2);

double point_distance(const Point *a, const Point *b);

#endif /* MYLIB_H */
```

Observa lo que cbindgen hace automáticamente:
- Include guards (`#ifndef MYLIB_H`)
- Includes estándar (`stdint.h`, `stdbool.h`, etc.)
- `c_int` → `int`, `c_double` → `double`
- `*const Point` → `const Point *`
- `#[repr(C)] struct` → `typedef struct { ... } Point;`

---

## 4. Uso desde build.rs

La forma recomendada es generar el header automáticamente en cada compilación:

### build.rs básico

```rust
// build.rs
fn main() {
    let crate_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .with_language(cbindgen::Language::C)
        .generate()
        .expect("Unable to generate C bindings")
        .write_to_file("include/mylib.h");

    println!("cargo::rerun-if-changed=src/lib.rs");
}
```

### Estructura del proyecto

```
mylib/
├── Cargo.toml
├── build.rs
├── cbindgen.toml          ← configuración (opcional)
├── include/
│   └── mylib.h            ← generado por build.rs
└── src/
    └── lib.rs
```

### build.rs con configuración desde archivo

```rust
// build.rs
fn main() {
    let crate_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    // Lee configuración desde cbindgen.toml si existe
    let config = cbindgen::Config::from_file("cbindgen.toml")
        .unwrap_or_default();

    cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .with_config(config)
        .generate()
        .expect("Unable to generate bindings")
        .write_to_file("include/mylib.h");

    println!("cargo::rerun-if-changed=src/");
    println!("cargo::rerun-if-changed=cbindgen.toml");
}
```

---

## 5. Configuración con cbindgen.toml

### Archivo de configuración

`cbindgen.toml` se coloca en la raíz del proyecto y controla cómo se genera el header:

```toml
# cbindgen.toml

# Lenguaje de salida: "C" o "C++"
language = "C"

# Include guard
header = "/* Generated by cbindgen — do not edit manually */"
include_guard = "MYLIB_H"

# O usar #pragma once en vez de include guard
# pragma_once = true

# Includes adicionales
includes = []
sys_includes = ["stddef.h"]

# Estilo de los typedefs de structs
style = "both"  # Genera: typedef struct Point { ... } Point;
# "type" solo genera typedef
# "tag" solo genera struct tag
# "both" genera ambos

# Documentación
documentation = true
documentation_style = "auto"

# Formato de output
tab_width = 4
line_length = 100

# Prefijo para todos los nombres exportados (evita conflictos)
[export]
prefix = ""
# prefix = "mylib_"  → mylib_add, mylib_Point, etc.

# Renombrar tipos
[export.rename]
# "RustName" = "CName"

# Cómo generar enums
[enum]
rename_variants = "QualifiedScreamingSnakeCase"
# Opciones: None, ScreamingSnakeCase, QualifiedScreamingSnakeCase

# Configuración de funciones
[fn]
# Prefijo y sufijo para declaraciones de funciones
prefix = ""
postfix = ""
# postfix = "__attribute__((visibility(\"default\")))"

# Argumentos: "auto", "horizontal", "vertical"
args = "auto"
```

### Ejemplos de configuración común

```toml
# Para una librería con namespace/prefijo
[export]
prefix = "mylib_"

# Para C++ con namespace
[export]
# Solo aplica si language = "C++"
# [cxx]
# namespace = "mylib"

# Para deshabilitar ciertos ítems
[export]
exclude = ["InternalStruct", "helper_function"]

# Para controlar el orden de los ítems
[export]
item_types = ["Constants", "Globals", "Enums", "Structs", "Unions",
              "Typedefs", "OpaqueItems", "Functions"]
```

### Prefijo para evitar conflictos

```toml
# cbindgen.toml
[export]
prefix = "rs_"
```

```rust
// src/lib.rs
#[no_mangle]
pub extern "C" fn add(a: i32, b: i32) -> i32 { a + b }

#[repr(C)]
pub struct Point { pub x: f64, pub y: f64 }
```

Header generado con prefijo:

```c
// mylib.h
int rs_add(int a, int b);

typedef struct {
  double x;
  double y;
} rs_Point;
```

---

## 6. Qué genera y qué no

### cbindgen GENERA

```rust
// ✓ Funciones con #[no_mangle] pub extern "C"
#[no_mangle]
pub extern "C" fn foo(x: i32) -> i32 { x }
// → int foo(int x);

// ✓ Structs con #[repr(C)]
#[repr(C)]
pub struct Rect { pub x: f64, pub y: f64, pub w: f64, pub h: f64 }
// → typedef struct { double x; double y; double w; double h; } Rect;

// ✓ Enums con #[repr(C)] o #[repr(i32)] etc.
#[repr(C)]
pub enum Color { Red, Green, Blue }
// → enum Color { Red, Green, Blue };

// ✓ Constantes públicas
pub const MAX_SIZE: u32 = 1024;
// → #define MAX_SIZE 1024
// O: static const uint32_t MAX_SIZE = 1024;

// ✓ Type aliases
pub type Handle = *mut std::ffi::c_void;
// → typedef void *Handle;
```

### cbindgen NO GENERA

```rust
// ✗ Funciones sin extern "C" (ABI de Rust)
pub fn internal_fn() {}

// ✗ Funciones sin #[no_mangle]
pub extern "C" fn mangled_fn() {}

// ✗ Structs sin #[repr(C)]
pub struct RustStruct { data: Vec<u8> }

// ✗ Tipos genéricos (generics no tienen representación en C)
pub struct Container<T> { data: T }

// ✗ Traits e implementaciones
pub trait MyTrait { fn method(&self); }

// ✗ Funciones privadas
#[no_mangle]
extern "C" fn private_fn() {}  // Sin pub

// ✗ Macros
macro_rules! my_macro { ... }
```

### Tipos que cbindgen traduce

```
┌─── Traducciones de tipos ─────────────────────────────────┐
│                                                            │
│  Rust                    C                                 │
│  ────                    ─                                 │
│  i8 / u8                 int8_t / uint8_t                  │
│  i16 / u16               int16_t / uint16_t                │
│  i32 / u32               int32_t / uint32_t                │
│  i64 / u64               int64_t / uint64_t                │
│  isize / usize           intptr_t / uintptr_t              │
│  f32 / f64               float / double                    │
│  bool                    bool (stdbool.h)                  │
│  c_int                   int                               │
│  c_char                  char                              │
│  *const T                const T*                          │
│  *mut T                  T*                                │
│  ()                      void                              │
│  Option<extern "C" fn>   function_ptr_or_NULL              │
│  Option<&T>              const T* (nullable)               │
│  Option<NonNull<T>>      T* (nullable)                     │
│  Box<T> (opaco)          T* (con free function)            │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

## 7. Atributos para controlar la generación

### #[cbindgen::no_export]

cbindgen reconoce el atributo `cfg` para excluir ítems:

```rust
/// This struct is exported to C.
#[repr(C)]
pub struct PublicStruct {
    pub value: i32,
}

/// This struct is NOT exported (using cfg to hide from cbindgen).
#[cfg(not(cbindgen))]
#[repr(C)]
pub struct InternalStruct {
    pub data: i32,
}
```

Cuando cbindgen procesa el código, define `cbindgen` como cfg, así que `#[cfg(not(cbindgen))]`
oculta ítems del header generado.

### Documentación en el header

cbindgen convierte doc comments de Rust en comentarios C:

```rust
/// Calculate the area of a rectangle.
///
/// # Parameters
/// - `width`: The width of the rectangle.
/// - `height`: The height of the rectangle.
///
/// # Returns
/// The area (width * height).
#[no_mangle]
pub extern "C" fn rect_area(width: f64, height: f64) -> f64 {
    width * height
}
```

Genera:

```c
/**
 * Calculate the area of a rectangle.
 *
 * # Parameters
 * - `width`: The width of the rectangle.
 * - `height`: The height of the rectangle.
 *
 * # Returns
 * The area (width * height).
 */
double rect_area(double width, double height);
```

### Renombrar en el header

```rust
/// cbindgen:rename-all=ScreamingSnakeCase
#[repr(C)]
pub enum LogLevel {
    Error,
    Warning,
    Info,
    Debug,
}
```

Genera:

```c
enum LogLevel {
  ERROR,
  WARNING,
  INFO,
  DEBUG,
};
```

### Field rename

```rust
/// cbindgen:field-names=[x, y, z]
#[repr(C)]
pub struct Vec3(pub f64, pub f64, pub f64);
```

Genera:

```c
typedef struct {
  double x;
  double y;
  double z;
} Vec3;
```

---

## 8. Proyecto completo: Rust lib + header + C app

### Estructura

```
project/
├── rust_lib/
│   ├── Cargo.toml
│   ├── build.rs
│   ├── cbindgen.toml
│   ├── include/
│   │   └── mathlib.h      ← generado automáticamente
│   └── src/
│       └── lib.rs
├── c_app/
│   └── main.c
└── Makefile
```

### Código Rust

```rust
// rust_lib/src/lib.rs
use std::ffi::{c_int, c_double, c_char, CStr, CString};

/// Maximum number of elements in a stats computation.
pub const MAX_ELEMENTS: usize = 10_000;

/// Result of a statistical computation.
#[repr(C)]
pub struct Stats {
    pub count: c_int,
    pub sum: c_double,
    pub mean: c_double,
    pub min: c_double,
    pub max: c_double,
}

/// Error codes for library functions.
#[repr(C)]
pub enum MathError {
    /// No error.
    Ok = 0,
    /// Null pointer argument.
    NullPointer = 1,
    /// Invalid argument value.
    InvalidArg = 2,
    /// Overflow occurred.
    Overflow = 3,
}

/// Compute statistics for an array of doubles.
///
/// Returns MathError::Ok on success.
/// The result is written to `out`.
#[no_mangle]
pub extern "C" fn compute_stats(
    data: *const c_double,
    len: c_int,
    out: *mut Stats,
) -> MathError {
    if data.is_null() || out.is_null() {
        return MathError::NullPointer;
    }
    if len <= 0 {
        return MathError::InvalidArg;
    }

    let slice = unsafe { std::slice::from_raw_parts(data, len as usize) };

    let sum: f64 = slice.iter().sum();
    let count = slice.len();
    let mean = sum / count as f64;
    let min = slice.iter().cloned().fold(f64::INFINITY, f64::min);
    let max = slice.iter().cloned().fold(f64::NEG_INFINITY, f64::max);

    let stats = Stats {
        count: count as c_int,
        sum,
        mean,
        min,
        max,
    };

    unsafe { *out = stats; }
    MathError::Ok
}

/// Compute the greatest common divisor of two integers.
#[no_mangle]
pub extern "C" fn gcd(mut a: c_int, mut b: c_int) -> c_int {
    a = a.abs();
    b = b.abs();
    while b != 0 {
        let t = b;
        b = a % b;
        a = t;
    }
    a
}

/// Check if a number is prime. Returns 1 for prime, 0 otherwise.
#[no_mangle]
pub extern "C" fn is_prime(n: c_int) -> c_int {
    if n <= 1 { return 0; }
    if n <= 3 { return 1; }
    if n % 2 == 0 || n % 3 == 0 { return 0; }
    let mut i = 5;
    while i * i <= n {
        if n % i == 0 || n % (i + 2) == 0 { return 0; }
        i += 6;
    }
    1
}

/// Format a Stats struct as a string.
///
/// Returns a newly allocated C string. The caller must free it
/// with `free_string`.
#[no_mangle]
pub extern "C" fn stats_to_string(stats: *const Stats) -> *mut c_char {
    if stats.is_null() {
        return std::ptr::null_mut();
    }
    let s = unsafe { &*stats };
    let text = format!(
        "count={}, sum={:.2}, mean={:.2}, min={:.2}, max={:.2}",
        s.count, s.sum, s.mean, s.min, s.max
    );
    match CString::new(text) {
        Ok(cs) => cs.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

/// Free a string allocated by this library.
#[no_mangle]
pub extern "C" fn free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)); }
    }
}
```

### Cargo.toml

```toml
# rust_lib/Cargo.toml
[package]
name = "mathlib"
version = "0.1.0"
edition = "2021"

[lib]
crate-type = ["staticlib"]

[build-dependencies]
cbindgen = "0.27"
```

### cbindgen.toml

```toml
# rust_lib/cbindgen.toml
language = "C"
header = "/* Auto-generated by cbindgen. Do not edit. */"
include_guard = "MATHLIB_H"
style = "both"
documentation = true

sys_includes = ["stddef.h"]

[enum]
rename_variants = "QualifiedScreamingSnakeCase"
```

### build.rs

```rust
// rust_lib/build.rs
fn main() {
    let crate_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();

    let config = cbindgen::Config::from_file("cbindgen.toml")
        .unwrap_or_default();

    cbindgen::Builder::new()
        .with_crate(&crate_dir)
        .with_config(config)
        .generate()
        .expect("Unable to generate C header")
        .write_to_file("include/mathlib.h");

    println!("cargo::rerun-if-changed=src/lib.rs");
    println!("cargo::rerun-if-changed=cbindgen.toml");
}
```

### Header generado (automáticamente)

```c
/* Auto-generated by cbindgen. Do not edit. */

#ifndef MATHLIB_H
#define MATHLIB_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define MAX_ELEMENTS 10000

/**
 * Error codes for library functions.
 */
enum MathError {
  /**
   * No error.
   */
  MATH_ERROR_OK = 0,
  /**
   * Null pointer argument.
   */
  MATH_ERROR_NULL_POINTER = 1,
  /**
   * Invalid argument value.
   */
  MATH_ERROR_INVALID_ARG = 2,
  /**
   * Overflow occurred.
   */
  MATH_ERROR_OVERFLOW = 3,
};
typedef uint32_t MathError;

/**
 * Result of a statistical computation.
 */
typedef struct Stats {
  int count;
  double sum;
  double mean;
  double min;
  double max;
} Stats;

/**
 * Compute statistics for an array of doubles.
 */
MathError compute_stats(const double *data, int len, struct Stats *out);

/**
 * Free a string allocated by this library.
 */
void free_string(char *s);

/**
 * Compute the greatest common divisor of two integers.
 */
int gcd(int a, int b);

/**
 * Check if a number is prime. Returns 1 for prime, 0 otherwise.
 */
int is_prime(int n);

/**
 * Format a Stats struct as a string.
 */
char *stats_to_string(const struct Stats *stats);

#endif /* MATHLIB_H */
```

### Programa C

```c
// c_app/main.c
#include <stdio.h>
#include "../rust_lib/include/mathlib.h"

int main() {
    // GCD
    printf("gcd(12, 8) = %d\n", gcd(12, 8));    // 4
    printf("gcd(17, 5) = %d\n", gcd(17, 5));     // 1

    // Primes
    for (int i = 2; i <= 20; i++) {
        if (is_prime(i)) {
            printf("%d ", i);
        }
    }
    printf("\n");  // 2 3 5 7 11 13 17 19

    // Stats
    double data[] = {1.0, 2.0, 3.0, 4.0, 5.0, 10.0};
    Stats stats;
    MathError err = compute_stats(data, 6, &stats);

    if (err == MATH_ERROR_OK) {
        printf("Count: %d\n", stats.count);
        printf("Sum:   %.2f\n", stats.sum);
        printf("Mean:  %.2f\n", stats.mean);
        printf("Min:   %.2f\n", stats.min);
        printf("Max:   %.2f\n", stats.max);

        char *s = stats_to_string(&stats);
        if (s) {
            printf("String: %s\n", s);
            free_string(s);
        }
    }

    // Error handling
    err = compute_stats(NULL, 5, &stats);
    if (err == MATH_ERROR_NULL_POINTER) {
        printf("Correctly detected null pointer\n");
    }

    return 0;
}
```

### Makefile

```makefile
RUST_DIR = rust_lib
RUST_LIB = $(RUST_DIR)/target/release/libmathlib.a

.PHONY: all clean

all: mathapp

$(RUST_LIB): $(RUST_DIR)/src/lib.rs $(RUST_DIR)/cbindgen.toml
	cd $(RUST_DIR) && cargo build --release

mathapp: c_app/main.c $(RUST_LIB)
	gcc c_app/main.c \
		-I $(RUST_DIR)/include \
		-L $(RUST_DIR)/target/release \
		-lmathlib \
		-lpthread -ldl -lm \
		-o $@

clean:
	rm -f mathapp
	cd $(RUST_DIR) && cargo clean
```

```bash
make
./mathapp
```

---

## 9. Errores comunes

### Error 1: cbindgen no encuentra funciones

```rust
// cbindgen ignora estas funciones:

// Sin pub — no exportada
#[no_mangle]
extern "C" fn hidden_fn() {}

// Sin extern "C" — ABI de Rust
#[no_mangle]
pub fn rust_abi_fn() {}

// Sin #[no_mangle] — nombre mangled
pub extern "C" fn mangled_fn() {}

// CORRECTO: las tres condiciones
#[no_mangle]
pub extern "C" fn visible_fn() {}
```

**Por qué falla**: cbindgen busca funciones que tengan `pub`, `extern "C"`, y
`#[no_mangle]`. Si falta cualquiera, la función no aparece en el header.

### Error 2: struct sin repr(C) no aparece en el header

```rust
// NO aparece en el header — sin repr(C)
pub struct InternalData {
    pub values: Vec<f64>,
}

// SÍ aparece en el header
#[repr(C)]
pub struct Point {
    pub x: f64,
    pub y: f64,
}
```

**Por qué falla**: sin `#[repr(C)]`, cbindgen no puede garantizar que el layout sea
compatible con C. Correctamente no genera la definición. Si necesitas el tipo como puntero
opaco, usa el patrón de tipo opaco (tema siguiente).

### Error 3: header desactualizado

```rust
// Cambiaste la firma de esta función:
// era: fn add(a: i32, b: i32) -> i32
// ahora:
#[no_mangle]
pub extern "C" fn add(a: i32, b: i32, c: i32) -> i32 {
    a + b + c
}
// Pero el header sigue diciendo: int add(int a, int b);
```

```rust
// Solución: build.rs que regenera el header automáticamente
fn main() {
    // rerun-if-changed hace que build.rs se ejecute
    // cuando cambia el código fuente
    println!("cargo::rerun-if-changed=src/");

    cbindgen::Builder::new()
        .with_crate(std::env::var("CARGO_MANIFEST_DIR").unwrap())
        .generate()
        .unwrap()
        .write_to_file("include/mylib.h");
}
```

**Por qué falla**: si generas el header manualmente con `cbindgen` CLI y olvidas
regenerarlo después de un cambio, el header y el código Rust se desincronizan. Usar
`build.rs` con `rerun-if-changed` automatiza la regeneración.

### Error 4: tipos genéricos no soportados

```rust
// cbindgen NO puede generar esto:
#[repr(C)]
pub struct Container<T> {
    pub data: T,
    pub count: i32,
}
// Error: C no tiene generics

// Solución: instanciar tipos concretos
#[repr(C)]
pub struct IntContainer {
    pub data: i32,
    pub count: i32,
}

#[repr(C)]
pub struct FloatContainer {
    pub data: f64,
    pub count: i32,
}
```

**Por qué falla**: C no tiene generics ni templates (C++ sí, pero cbindgen genera C).
Cada tipo concreto debe tener su propia definición.

### Error 5: conflicto de nombres con headers del sistema

```rust
// MAL: "open" colisiona con open() de libc
#[no_mangle]
pub extern "C" fn open(path: *const std::ffi::c_char) -> i32 { 0 }

// BIEN: prefijar todas las funciones
#[no_mangle]
pub extern "C" fn mylib_open(path: *const std::ffi::c_char) -> i32 { 0 }
```

```toml
# O usar el prefijo automático en cbindgen.toml:
[export]
prefix = "mylib_"
```

**Por qué falla**: si tu función se llama `open`, `read`, `write`, etc., colisiona con
funciones del sistema al linkar. Siempre usa un prefijo único para tu librería.

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════════╗
║                           CBINDGEN                                     ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                        ║
║  INSTALACIÓN                                                           ║
║  ───────────                                                           ║
║  cargo install cbindgen                    (CLI)                       ║
║  [build-dependencies] cbindgen = "0.27"    (build.rs)                  ║
║                                                                        ║
║  CLI                                                                   ║
║  ───                                                                   ║
║  cbindgen --crate name -o header.h --lang c                            ║
║  cbindgen --crate name -o header.hpp --lang c++                        ║
║                                                                        ║
║  BUILD.RS                                                              ║
║  ────────                                                              ║
║  cbindgen::Builder::new()                                              ║
║      .with_crate(&crate_dir)                                           ║
║      .with_config(config)              // desde cbindgen.toml          ║
║      .generate()?                                                      ║
║      .write_to_file("include/lib.h");                                  ║
║  println!("cargo::rerun-if-changed=src/");                             ║
║                                                                        ║
║  QUÉ EXPORTA                                                          ║
║  ───────────                                                           ║
║  ✓ #[no_mangle] pub extern "C" fn   → prototipo C                     ║
║  ✓ #[repr(C)] pub struct            → typedef struct                   ║
║  ✓ #[repr(C/i32/u8)] pub enum       → enum + typedef                  ║
║  ✓ pub const X: tipo = valor        → #define o static const          ║
║  ✓ pub type Alias = *mut c_void     → typedef                         ║
║  ✗ Sin pub, sin extern "C", sin repr(C), generics, traits             ║
║                                                                        ║
║  CBINDGEN.TOML                                                         ║
║  ─────────────                                                         ║
║  language = "C"                        lenguaje de salida              ║
║  include_guard = "LIB_H"              #ifndef guard                    ║
║  style = "both"                        struct + typedef                ║
║  documentation = true                  doc comments → C comments       ║
║  [export]                                                              ║
║  prefix = "mylib_"                     prefijo para todo               ║
║  exclude = ["Internal"]                excluir ítems                   ║
║  [enum]                                                                ║
║  rename_variants = "ScreamingSnakeCase"                                ║
║                                                                        ║
║  OCULTAR DE CBINDGEN                                                   ║
║  ───────────────────                                                   ║
║  #[cfg(not(cbindgen))]                 ocultar ítems del header        ║
║                                                                        ║
║  DOC COMMENTS                                                          ║
║  ────────────                                                          ║
║  /// Rust doc → /** C doc */           automático                      ║
║                                                                        ║
║  CONVENCIÓN DE NOMBRES                                                 ║
║  ────────────────────                                                  ║
║  Prefijar todas las funciones: mylib_open, mylib_close                 ║
║  Evitar nombres que colisionen con libc (open, read, write)            ║
║                                                                        ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: generar header para una librería existente

Toma la librería de string utils del tema T01 (o créala de nuevo) con estas funciones:
- `rust_strlen`, `rust_to_upper`, `rust_reverse`, `rust_free_string`

1. Configura `cbindgen.toml` con include guard `RUST_STRINGS_H` y `documentation = true`.
2. Agrega `build.rs` para generar el header automáticamente en `include/rust_strings.h`.
3. Ejecuta `cargo build` y verifica que el header generado es correcto.
4. Compara el header generado con el que escribiste a mano en T01. ¿Hay diferencias?
5. Agrega una nueva función (`rust_trim`) al código Rust, ejecuta `cargo build`, y verifica
   que el header se actualiza automáticamente.

**Pista**: asegúrate de que cada función tiene `#[no_mangle]`, `pub`, y `extern "C"`, y
que cada struct tiene `#[repr(C)]`. Sin ellos, cbindgen no genera nada.

> **Pregunta de reflexión**: ¿qué ventaja tiene generar el header con cbindgen en `build.rs`
> vs generarlo manualmente con `cbindgen` CLI? ¿En qué caso usarías el CLI en vez de
> build.rs?

---

### Ejercicio 2: librería con enums y configuración

Crea una librería Rust con:
1. Un enum `#[repr(C)] ErrorCode` con variantes: `Ok`, `NotFound`, `PermissionDenied`,
   `InvalidInput`, `InternalError`.
2. Un struct `#[repr(C)] Config` con campos: `max_retries: i32`, `timeout_ms: i32`,
   `verbose: bool`.
3. Funciones: `config_default() -> Config`, `config_validate(*const Config) -> ErrorCode`,
   `error_message(ErrorCode) -> *const c_char`.
4. Configura `cbindgen.toml` con `rename_variants = "QualifiedScreamingSnakeCase"` para
   que las variantes del enum se generen como `ERROR_CODE_OK`, `ERROR_CODE_NOT_FOUND`, etc.
5. Genera el header y escribe un programa C que use la API.

**Pista**: para `error_message`, puedes retornar punteros a string literals estáticos
(no necesitan free): `b"ok\0".as_ptr() as *const c_char`.

> **Pregunta de reflexión**: cbindgen genera `typedef uint32_t ErrorCode;` para el enum
> repr(C). ¿Por qué usa un typedef en vez de `enum ErrorCode` directamente? ¿Qué
> garantiza sobre el tamaño del tipo?

---

### Ejercicio 3: proyecto completo con Makefile

Crea un proyecto con la estructura completa:
1. `rust_lib/` con una librería que exporte al menos 5 funciones (mezcla de tipos:
   funciones con enteros, con strings, con structs, y una función free).
2. `rust_lib/build.rs` que use cbindgen para generar el header.
3. `rust_lib/cbindgen.toml` con prefijo `mylib_` y documentación activada.
4. `c_app/main.c` que use todas las funciones.
5. Un `Makefile` en la raíz que compile todo con `make` y limpie con `make clean`.

Verifica el flujo completo:
- `make` compila Rust, genera header, compila C, produce ejecutable.
- Modifica una función Rust, ejecuta `make`, y confirma que el header y el programa C
  se actualizan.
- `make clean` limpia todo.

**Pista**: en el Makefile, la dependencia del header es implícita: `cargo build` ejecuta
`build.rs` que regenera el header.

> **Pregunta de reflexión**: si dos desarrolladores trabajan en paralelo — uno en el código
> Rust y otro en el código C — y el primero cambia la firma de una función, ¿cuándo se
> enteraría el segundo del cambio? ¿El header generado debería estar en git o en `.gitignore`?
> Argumenta ambas posiciones.
