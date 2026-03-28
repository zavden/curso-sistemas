# #[no_mangle] y extern "C"

## Índice

1. [Exportar funciones Rust para C](#1-exportar-funciones-rust-para-c)
2. [#[no_mangle]: desactivar name mangling](#2-no_mangle-desactivar-name-mangling)
3. [extern "C": usar la ABI de C](#3-extern-c-usar-la-abi-de-c)
4. [Tipos de librería: staticlib y cdylib](#4-tipos-de-librería-staticlib-y-cdylib)
5. [Compilar y linkar desde C](#5-compilar-y-linkar-desde-c)
6. [Pasar y retornar tipos](#6-pasar-y-retornar-tipos)
7. [Manejo de panics en el boundary](#7-manejo-de-panics-en-el-boundary)
8. [Patrones completos](#8-patrones-completos)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. Exportar funciones Rust para C

Hasta ahora hemos llamado C desde Rust. Ahora hacemos lo inverso: escribir funciones en
Rust que pueda llamar código C. Esto es útil para:

- Reescribir componentes críticos de un proyecto C en Rust (seguridad, rendimiento)
- Crear librerías Rust usables desde C, Python, Ruby, Go (todos hablan C ABI)
- Plugins de Rust para aplicaciones C/C++ existentes

```
┌─── Dirección de la llamada ───────────────────────────────┐
│                                                            │
│  S01: C → compilar → libfoo.a    Rust → extern "C" → call │
│        (Rust llama a C)                                    │
│                                                            │
│  S02: Rust → compilar → libbar.a    C → #include → call   │
│        (C llama a Rust)                                    │
│                                                            │
│  Para que C pueda llamar a Rust, necesitamos:              │
│  1. #[no_mangle] — nombre predecible en el binario        │
│  2. extern "C"   — ABI compatible con C                    │
│  3. crate-type   — producir .a o .so                       │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### El ejemplo más simple

```rust
// src/lib.rs

/// A function callable from C.
#[no_mangle]
pub extern "C" fn add(a: i32, b: i32) -> i32 {
    a + b
}
```

```c
// main.c
#include <stdio.h>

// Declarar la función que viene de Rust
extern int add(int a, int b);

int main() {
    int result = add(3, 4);
    printf("3 + 4 = %d\n", result);  // 7
    return 0;
}
```

Dos atributos, una función Rust que C puede llamar. Veamos cada pieza en detalle.

---

## 2. #[no_mangle]: desactivar name mangling

### ¿Qué es name mangling?

El compilador de Rust transforma los nombres de funciones para incluir información del
módulo, tipos genéricos y un hash. Esto permite que funciones con el mismo nombre en
distintos módulos coexistan sin conflicto:

```
┌─── Name mangling en Rust ─────────────────────────────────┐
│                                                            │
│  Código Rust:          Símbolo en el binario:              │
│  ─────────────         ─────────────────────               │
│  fn add(a: i32)   →    _ZN7mylib3add17h8a3f2e1b4c5d6e7fE  │
│  fn add(a: f64)   →    _ZN7mylib3add17hab1234567890abcdE   │
│                                                            │
│  Los nombres mangledos son únicos pero ilegibles           │
│  y cambian entre compilaciones                             │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

C no tiene name mangling — el símbolo de `add` en el binario es literalmente `add`.
Para que C encuentre nuestra función, necesitamos que Rust use el nombre literal:

```rust
// SIN #[no_mangle]: el símbolo es algo como _ZN7mylib3add17h...
fn add(a: i32, b: i32) -> i32 { a + b }

// CON #[no_mangle]: el símbolo es exactamente "add"
#[no_mangle]
pub fn add(a: i32, b: i32) -> i32 { a + b }
```

### Verificar los símbolos

```bash
# Compilar la librería Rust
cargo build --release

# Ver los símbolos exportados
nm -D target/release/libmylib.so | grep add
# Sin no_mangle: T _ZN7mylib3add17h8a3f2e1b4c5d6e7fE
# Con no_mangle: T add

# O con objdump
objdump -T target/release/libmylib.so | grep add
```

### pub es necesario

`#[no_mangle]` sin `pub` funciona pero genera un warning — la función no se exporta
realmente del crate:

```rust
// WARNING: function `add` is marked #[no_mangle] but not exported
#[no_mangle]
fn add(a: i32, b: i32) -> i32 { a + b }

// CORRECTO: pub exporta la función del crate
#[no_mangle]
pub fn add(a: i32, b: i32) -> i32 { a + b }
```

---

## 3. extern "C": usar la ABI de C

### Funciones Rust con ABI de C

`extern "C"` le dice al compilador que use la calling convention de C para esta función:

```rust
// ABI de Rust (por defecto) — solo Rust puede llamarla
pub fn rust_add(a: i32, b: i32) -> i32 { a + b }

// ABI de C — cualquier lenguaje que hable C ABI puede llamarla
pub extern "C" fn c_add(a: i32, b: i32) -> i32 { a + b }

// Combinado: nombre legible + ABI de C = callable desde C
#[no_mangle]
pub extern "C" fn add(a: i32, b: i32) -> i32 { a + b }
```

### ¿Qué cambia con extern "C"?

```
┌─── Diferencias entre ABI Rust y ABI C ────────────────────┐
│                                                            │
│  ABI Rust:                                                 │
│  • Argumentos pueden ir en cualquier registro              │
│  • Structs pueden pasarse de formas optimizadas            │
│  • El compilador decide todo → puede cambiar               │
│                                                            │
│  ABI C (extern "C"):                                       │
│  • Argumentos en registros fijos (rdi, rsi, rdx... en x86) │
│  • Structs en stack si son grandes                         │
│  • Definido por la plataforma → estable                    │
│                                                            │
│  extern "C" es NECESARIO para que C pueda encontrar        │
│  los argumentos donde los espera                           │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Combinación obligatoria

Para que C llame a Rust, **siempre** necesitas ambos:

```rust
// INCOMPLETO: nombre correcto pero ABI incorrecta
#[no_mangle]
pub fn add(a: i32, b: i32) -> i32 { a + b }
// C encuentra "add" pero los argumentos están en los registros incorrectos
// → valores basura o crash

// INCOMPLETO: ABI correcta pero nombre inaccesible
pub extern "C" fn add(a: i32, b: i32) -> i32 { a + b }
// C no puede encontrar el símbolo (nombre mangled)

// CORRECTO: ambos
#[no_mangle]
pub extern "C" fn add(a: i32, b: i32) -> i32 { a + b }
```

---

## 4. Tipos de librería: staticlib y cdylib

### Configurar el crate type

Para que `cargo build` produzca una librería C en vez de un binario Rust, configura el
`crate-type` en Cargo.toml:

```toml
# Cargo.toml
[package]
name = "mylib"
version = "0.1.0"
edition = "2021"

[lib]
# Producir una librería estática (.a) para C
crate-type = ["staticlib"]

# O una librería dinámica (.so / .dylib / .dll) para C
# crate-type = ["cdylib"]

# O ambas
# crate-type = ["staticlib", "cdylib"]
```

### staticlib vs cdylib

```
┌─── staticlib ─────────────────────────────────────────────┐
│                                                            │
│  cargo build → target/release/libmylib.a                   │
│                                                            │
│  • Se incorpora al binario C en tiempo de compilación      │
│  • El binario resultante es autosuficiente                 │
│  • Incluye el runtime de Rust (allocator, panic handler)   │
│  • Archivo más grande                                      │
│  • No necesita el .a en runtime                            │
│                                                            │
└────────────────────────────────────────────────────────────┘

┌─── cdylib ────────────────────────────────────────────────┐
│                                                            │
│  cargo build → target/release/libmylib.so  (Linux)         │
│                target/release/libmylib.dylib (macOS)       │
│                target/release/mylib.dll (Windows)           │
│                                                            │
│  • Se carga en runtime (como cualquier .so)                │
│  • Binario C más pequeño                                   │
│  • Necesita el .so presente en el sistema                  │
│  • Puede ser cargado dinámicamente (dlopen)                │
│  • Útil para plugins                                       │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Archivos producidos

```bash
# Compilar
cargo build --release

# staticlib produce:
ls target/release/libmylib.a

# cdylib produce:
ls target/release/libmylib.so     # Linux
ls target/release/libmylib.dylib  # macOS

# Ver qué exporta la librería
nm target/release/libmylib.a | grep " T "      # Símbolos definidos
nm -D target/release/libmylib.so | grep " T "   # Símbolos exportados
```

---

## 5. Compilar y linkar desde C

### Proyecto completo con staticlib

Estructura:

```
project/
├── rust_lib/
│   ├── Cargo.toml
│   └── src/
│       └── lib.rs
├── c_app/
│   ├── main.c
│   └── mylib.h
└── Makefile
```

```rust
// rust_lib/src/lib.rs
use std::ffi::{c_int, c_double};

#[no_mangle]
pub extern "C" fn rust_add(a: c_int, b: c_int) -> c_int {
    a + b
}

#[no_mangle]
pub extern "C" fn rust_factorial(n: c_int) -> c_int {
    (1..=n).product()
}

#[no_mangle]
pub extern "C" fn rust_fibonacci(n: c_int) -> c_int {
    if n <= 1 {
        return n;
    }
    let mut a = 0;
    let mut b = 1;
    for _ in 2..=n {
        let temp = a + b;
        a = b;
        b = temp;
    }
    b
}

#[no_mangle]
pub extern "C" fn rust_distance(x1: c_double, y1: c_double, x2: c_double, y2: c_double) -> c_double {
    ((x2 - x1).powi(2) + (y2 - y1).powi(2)).sqrt()
}
```

```toml
# rust_lib/Cargo.toml
[package]
name = "mylib"
version = "0.1.0"
edition = "2021"

[lib]
crate-type = ["staticlib"]
```

```c
// c_app/mylib.h
#ifndef MYLIB_H
#define MYLIB_H

int rust_add(int a, int b);
int rust_factorial(int n);
int rust_fibonacci(int n);
double rust_distance(double x1, double y1, double x2, double y2);

#endif
```

```c
// c_app/main.c
#include <stdio.h>
#include "mylib.h"

int main() {
    printf("add(3, 4) = %d\n", rust_add(3, 4));
    printf("factorial(10) = %d\n", rust_factorial(10));
    printf("fibonacci(10) = %d\n", rust_fibonacci(10));
    printf("distance(0,0 -> 3,4) = %.2f\n", rust_distance(0, 0, 3, 4));
    return 0;
}
```

### Compilar y linkar

```bash
# 1. Compilar la librería Rust
cd rust_lib
cargo build --release
cd ..

# 2. Compilar el programa C, linkando con la librería Rust
gcc c_app/main.c \
    -I c_app \
    -L rust_lib/target/release \
    -lmylib \
    -lpthread -ldl -lm \
    -o my_program

# 3. Ejecutar
./my_program
```

### ¿Por qué -lpthread -ldl -lm?

La librería estática de Rust incluye el runtime de Rust, que depende de:
- `-lpthread`: threads (el allocator y el panic handler los usan)
- `-ldl`: dynamic loading (para cargar librerías compartidas)
- `-lm`: math library

Sin ellas, obtendrías errores de linking como `undefined reference to 'pthread_create'`.

```
┌─── Dependencias al linkar staticlib de Rust ──────────────┐
│                                                            │
│  gcc main.c -lmylib                                        │
│       ↓                                                    │
│  libmylib.a contiene:                                      │
│  ├── tus funciones (rust_add, etc.)                        │
│  ├── runtime de Rust (allocator, panic, etc.)              │
│  └── dependencias → necesitan: -lpthread -ldl -lm          │
│                                                            │
│  Con cdylib (.so) no necesitas las -l extras:              │
│  la .so ya incluye todas sus dependencias resueltas        │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Makefile

```makefile
# Makefile
RUST_LIB = rust_lib/target/release/libmylib.a

.PHONY: all clean

all: my_program

$(RUST_LIB):
	cd rust_lib && cargo build --release

my_program: c_app/main.c $(RUST_LIB)
	gcc c_app/main.c \
		-I c_app \
		-L rust_lib/target/release \
		-lmylib \
		-lpthread -ldl -lm \
		-o $@

clean:
	rm -f my_program
	cd rust_lib && cargo clean
```

### Con cdylib (librería dinámica)

```bash
# Compilar
cd rust_lib && cargo build --release && cd ..

# Linkar (sin las -l extras)
gcc c_app/main.c \
    -I c_app \
    -L rust_lib/target/release \
    -lmylib \
    -o my_program

# Ejecutar (necesita la .so en el path)
LD_LIBRARY_PATH=rust_lib/target/release ./my_program
```

---

## 6. Pasar y retornar tipos

### Tipos primitivos

Los tipos primitivos de Rust y C son directamente compatibles con `extern "C"`:

```rust
use std::ffi::{c_int, c_uint, c_long, c_double, c_float, c_char};

#[no_mangle]
pub extern "C" fn process_int(x: c_int) -> c_int { x * 2 }

#[no_mangle]
pub extern "C" fn process_float(x: c_float) -> c_float { x * 2.0 }

#[no_mangle]
pub extern "C" fn process_double(x: c_double) -> c_double { x * 2.0 }

#[no_mangle]
pub extern "C" fn max_of(a: c_int, b: c_int) -> c_int {
    if a > b { a } else { b }
}
```

### Punteros y arrays

```rust
use std::ffi::{c_int, c_double};

/// Sum an array of integers. Called from C with pointer + length.
#[no_mangle]
pub extern "C" fn sum_array(data: *const c_int, len: c_int) -> c_int {
    if data.is_null() || len <= 0 {
        return 0;
    }

    let slice = unsafe { std::slice::from_raw_parts(data, len as usize) };
    slice.iter().sum()
}

/// Sort an array of doubles in place.
#[no_mangle]
pub extern "C" fn sort_doubles(data: *mut c_double, len: c_int) {
    if data.is_null() || len <= 0 {
        return;
    }

    let slice = unsafe { std::slice::from_raw_parts_mut(data, len as usize) };
    slice.sort_by(|a, b| a.partial_cmp(b).unwrap_or(std::cmp::Ordering::Equal));
}
```

```c
// Uso desde C
#include <stdio.h>

extern int sum_array(const int *data, int len);
extern void sort_doubles(double *data, int len);

int main() {
    int nums[] = {5, 3, 8, 1, 9};
    printf("Sum: %d\n", sum_array(nums, 5));  // 26

    double vals[] = {3.14, 1.41, 2.71, 0.57};
    sort_doubles(vals, 4);
    for (int i = 0; i < 4; i++) {
        printf("%.2f ", vals[i]);
    }
    printf("\n");  // 0.57 1.41 2.71 3.14

    return 0;
}
```

### Strings

```rust
use std::ffi::{CStr, CString, c_char};

/// Return the length of a C string.
#[no_mangle]
pub extern "C" fn rust_strlen(s: *const c_char) -> usize {
    if s.is_null() {
        return 0;
    }
    let c_str = unsafe { CStr::from_ptr(s) };
    c_str.to_bytes().len()
}

/// Convert a string to uppercase. Returns a newly allocated C string.
/// The caller must free it with rust_free_string.
#[no_mangle]
pub extern "C" fn rust_to_upper(s: *const c_char) -> *mut c_char {
    if s.is_null() {
        return std::ptr::null_mut();
    }

    let c_str = unsafe { CStr::from_ptr(s) };
    let rust_str = c_str.to_string_lossy();
    let upper = rust_str.to_uppercase();

    match CString::new(upper) {
        Ok(cs) => cs.into_raw(),  // Transfiere ownership al caller
        Err(_) => std::ptr::null_mut(),
    }
}

/// Free a string allocated by Rust.
#[no_mangle]
pub extern "C" fn rust_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)); }
    }
}
```

```c
// Uso desde C
#include <stdio.h>

extern int rust_strlen(const char *s);
extern char *rust_to_upper(const char *s);
extern void rust_free_string(char *s);

int main() {
    printf("strlen: %d\n", rust_strlen("hello"));  // 5

    char *upper = rust_to_upper("hello world");
    if (upper) {
        printf("upper: %s\n", upper);  // HELLO WORLD
        rust_free_string(upper);       // IMPORTANTE: liberar
    }

    return 0;
}
```

### Structs repr(C)

```rust
use std::ffi::c_int;

#[repr(C)]
pub struct Point {
    pub x: f64,
    pub y: f64,
}

#[repr(C)]
pub struct Rect {
    pub origin: Point,
    pub width: f64,
    pub height: f64,
}

#[no_mangle]
pub extern "C" fn point_distance(a: *const Point, b: *const Point) -> f64 {
    if a.is_null() || b.is_null() {
        return -1.0;
    }
    let a = unsafe { &*a };
    let b = unsafe { &*b };
    ((b.x - a.x).powi(2) + (b.y - a.y).powi(2)).sqrt()
}

#[no_mangle]
pub extern "C" fn rect_area(r: *const Rect) -> f64 {
    if r.is_null() {
        return -1.0;
    }
    let r = unsafe { &*r };
    r.width * r.height
}

/// Create a rect. Returns by value (small structs are fine).
#[no_mangle]
pub extern "C" fn make_rect(x: f64, y: f64, w: f64, h: f64) -> Rect {
    Rect {
        origin: Point { x, y },
        width: w,
        height: h,
    }
}
```

---

## 7. Manejo de panics en el boundary

### El problema: panic a través de C es UB

Un panic de Rust que se propaga a través de un stack frame de C es **undefined behavior**.
El stack unwinding de Rust no sabe navegar frames de C:

```rust
// MAL: si la operación hace panic, UB
#[no_mangle]
pub extern "C" fn dangerous_divide(a: i32, b: i32) -> i32 {
    a / b  // Panic si b == 0
}
```

### Solución: catch_unwind

```rust
use std::ffi::c_int;
use std::panic;

#[no_mangle]
pub extern "C" fn safe_divide(a: c_int, b: c_int, result: *mut c_int) -> c_int {
    if result.is_null() {
        return -1;
    }

    let outcome = panic::catch_unwind(|| {
        if b == 0 {
            return Err(-2);  // Division by zero
        }
        Ok(a / b)
    });

    match outcome {
        Ok(Ok(value)) => {
            unsafe { *result = value; }
            0  // Success
        }
        Ok(Err(code)) => code,       // Error lógico
        Err(_) => -99,                // Panic capturado
    }
}
```

### Patrón general: wrapper con catch_unwind

```rust
use std::panic;
use std::ffi::c_int;

/// Wrap a closure, catching panics and returning an error code.
fn ffi_boundary<F: FnOnce() -> c_int + panic::UnwindSafe>(f: F) -> c_int {
    match panic::catch_unwind(f) {
        Ok(result) => result,
        Err(_) => {
            eprintln!("Rust panic caught at FFI boundary");
            -1  // Error code for panic
        }
    }
}

#[no_mangle]
pub extern "C" fn complex_operation(input: c_int) -> c_int {
    ffi_boundary(|| {
        // Tu lógica aquí — si hace panic, se captura
        if input < 0 {
            panic!("negative input!");  // Capturado, no UB
        }
        input * 2
    })
}
```

### abort en vez de unwind

Otra opción es configurar el perfil para que panics hagan abort directamente:

```toml
# Cargo.toml
[profile.release]
panic = "abort"
```

Con `panic = "abort"`, un panic termina el proceso inmediatamente sin intentar unwind.
Esto es más seguro para FFI (no hay riesgo de UB) pero menos informativo (sin mensaje
de error recuperable).

---

## 8. Patrones completos

### Librería de string utils

```rust
// src/lib.rs
use std::ffi::{CStr, CString, c_char, c_int};
use std::panic;

// --- Helper ---
fn ffi_catch<F: FnOnce() -> c_int + panic::UnwindSafe>(f: F) -> c_int {
    match panic::catch_unwind(f) {
        Ok(r) => r,
        Err(_) => -1,
    }
}

// --- String functions ---

/// Check if a string contains a substring. Returns 1 (true) or 0 (false).
#[no_mangle]
pub extern "C" fn rust_contains(haystack: *const c_char, needle: *const c_char) -> c_int {
    if haystack.is_null() || needle.is_null() {
        return 0;
    }
    ffi_catch(|| {
        let h = unsafe { CStr::from_ptr(haystack) }.to_string_lossy();
        let n = unsafe { CStr::from_ptr(needle) }.to_string_lossy();
        if h.contains(n.as_ref()) { 1 } else { 0 }
    })
}

/// Count occurrences of a character in a string.
#[no_mangle]
pub extern "C" fn rust_count_char(s: *const c_char, c: c_char) -> c_int {
    if s.is_null() {
        return 0;
    }
    let c_str = unsafe { CStr::from_ptr(s) };
    let byte = c as u8;
    c_str.to_bytes().iter().filter(|&&b| b == byte).count() as c_int
}

/// Reverse a string. Returns newly allocated string.
/// Caller must free with rust_free_string.
#[no_mangle]
pub extern "C" fn rust_reverse(s: *const c_char) -> *mut c_char {
    if s.is_null() {
        return std::ptr::null_mut();
    }
    let c_str = unsafe { CStr::from_ptr(s) };
    let reversed: String = c_str.to_string_lossy().chars().rev().collect();

    match CString::new(reversed) {
        Ok(cs) => cs.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

/// Repeat a string n times. Returns newly allocated string.
/// Caller must free with rust_free_string.
#[no_mangle]
pub extern "C" fn rust_repeat(s: *const c_char, n: c_int) -> *mut c_char {
    if s.is_null() || n <= 0 {
        return std::ptr::null_mut();
    }
    let c_str = unsafe { CStr::from_ptr(s) };
    let repeated = c_str.to_string_lossy().repeat(n as usize);

    match CString::new(repeated) {
        Ok(cs) => cs.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn rust_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)); }
    }
}
```

```c
// Header correspondiente
#ifndef RUST_STRINGS_H
#define RUST_STRINGS_H

int rust_contains(const char *haystack, const char *needle);
int rust_count_char(const char *s, char c);
char *rust_reverse(const char *s);
char *rust_repeat(const char *s, int n);
void rust_free_string(char *s);

#endif
```

```c
// main.c
#include <stdio.h>
#include "rust_strings.h"

int main() {
    printf("contains: %d\n", rust_contains("hello world", "world"));  // 1
    printf("count 'l': %d\n", rust_count_char("hello world", 'l'));    // 3

    char *rev = rust_reverse("hello");
    if (rev) {
        printf("reverse: %s\n", rev);  // olleh
        rust_free_string(rev);
    }

    char *rep = rust_repeat("ab", 3);
    if (rep) {
        printf("repeat: %s\n", rep);   // ababab
        rust_free_string(rep);
    }

    return 0;
}
```

### Librería de colección (Vec como array dinámico)

```rust
use std::ffi::c_int;

/// An opaque dynamic array of integers.
pub struct IntVec {
    data: Vec<c_int>,
}

#[no_mangle]
pub extern "C" fn intvec_new() -> *mut IntVec {
    Box::into_raw(Box::new(IntVec { data: Vec::new() }))
}

#[no_mangle]
pub extern "C" fn intvec_free(v: *mut IntVec) {
    if !v.is_null() {
        unsafe { drop(Box::from_raw(v)); }
    }
}

#[no_mangle]
pub extern "C" fn intvec_push(v: *mut IntVec, value: c_int) {
    if v.is_null() { return; }
    let v = unsafe { &mut *v };
    v.data.push(value);
}

#[no_mangle]
pub extern "C" fn intvec_get(v: *const IntVec, index: c_int) -> c_int {
    if v.is_null() || index < 0 {
        return -1;
    }
    let v = unsafe { &*v };
    v.data.get(index as usize).copied().unwrap_or(-1)
}

#[no_mangle]
pub extern "C" fn intvec_len(v: *const IntVec) -> c_int {
    if v.is_null() { return 0; }
    let v = unsafe { &*v };
    v.data.len() as c_int
}

#[no_mangle]
pub extern "C" fn intvec_sort(v: *mut IntVec) {
    if v.is_null() { return; }
    let v = unsafe { &mut *v };
    v.data.sort();
}

#[no_mangle]
pub extern "C" fn intvec_sum(v: *const IntVec) -> c_int {
    if v.is_null() { return 0; }
    let v = unsafe { &*v };
    v.data.iter().sum()
}
```

```c
// Uso desde C
typedef struct IntVec IntVec;

extern IntVec *intvec_new(void);
extern void intvec_free(IntVec *v);
extern void intvec_push(IntVec *v, int value);
extern int intvec_get(const IntVec *v, int index);
extern int intvec_len(const IntVec *v);
extern void intvec_sort(IntVec *v);
extern int intvec_sum(const IntVec *v);

int main() {
    IntVec *v = intvec_new();

    intvec_push(v, 5);
    intvec_push(v, 3);
    intvec_push(v, 8);
    intvec_push(v, 1);

    printf("len: %d\n", intvec_len(v));   // 4
    printf("sum: %d\n", intvec_sum(v));   // 17

    intvec_sort(v);
    for (int i = 0; i < intvec_len(v); i++) {
        printf("[%d] = %d\n", i, intvec_get(v, i));
    }
    // [0] = 1, [1] = 3, [2] = 5, [3] = 8

    intvec_free(v);  // Libera el Vec y el Box
    return 0;
}
```

Este patrón de **tipo opaco** (el caller C solo ve un puntero, nunca los campos internos)
se desarrolla en profundidad en el tema T03.

---

## 9. Errores comunes

### Error 1: olvidar #[no_mangle] o extern "C"

```rust
// MAL: solo extern "C" — C no encuentra el símbolo
pub extern "C" fn add(a: i32, b: i32) -> i32 { a + b }
// Símbolo: _ZN5mylib3add17h... → C no puede linkar

// MAL: solo #[no_mangle] — argumentos en registros incorrectos
#[no_mangle]
pub fn add(a: i32, b: i32) -> i32 { a + b }
// Símbolo: add → C lo encuentra pero ABI diferente → basura

// BIEN: ambos
#[no_mangle]
pub extern "C" fn add(a: i32, b: i32) -> i32 { a + b }
```

**Por qué falla**: sin `#[no_mangle]`, el linker de C no encuentra la función. Sin
`extern "C"`, la encuentra pero los argumentos se pasan por la ABI incorrecta.

### Error 2: retornar tipos Rust complejos

```rust
// MAL: String no tiene layout C-compatible
#[no_mangle]
pub extern "C" fn get_name() -> String {
    "Alice".to_string()
    // String es (ptr, len, capacity) — C no sabe interpretarlo
}

// MAL: Vec no tiene layout C-compatible
#[no_mangle]
pub extern "C" fn get_data() -> Vec<i32> {
    vec![1, 2, 3]
}

// BIEN: retornar *mut c_char (con free function)
#[no_mangle]
pub extern "C" fn get_name() -> *mut std::ffi::c_char {
    let cs = std::ffi::CString::new("Alice").unwrap();
    cs.into_raw()
}

// BIEN: llenar un buffer proporcionado por C
#[no_mangle]
pub extern "C" fn get_data(out: *mut i32, capacity: i32) -> i32 {
    let data = [1, 2, 3];
    let count = data.len().min(capacity as usize);
    unsafe {
        std::ptr::copy_nonoverlapping(data.as_ptr(), out, count);
    }
    count as i32
}
```

**Por qué falla**: `String`, `Vec`, `HashMap` y otros tipos de Rust tienen layouts internos
que C no comprende. Solo tipos primitivos, `#[repr(C)]` structs y punteros crudos cruzan el
boundary.

### Error 3: no proveer función de liberación

```rust
// MAL: C recibe un puntero a memoria de Rust pero no puede liberarla
#[no_mangle]
pub extern "C" fn create_message() -> *mut std::ffi::c_char {
    std::ffi::CString::new("hello").unwrap().into_raw()
    // ¿Cómo libera C esta memoria? free() de C NO funciona
    // porque Rust puede usar un allocator diferente
}

// BIEN: proveer función de liberación
#[no_mangle]
pub extern "C" fn create_message() -> *mut std::ffi::c_char {
    std::ffi::CString::new("hello").unwrap().into_raw()
}

#[no_mangle]
pub extern "C" fn free_message(s: *mut std::ffi::c_char) {
    if !s.is_null() {
        unsafe { drop(std::ffi::CString::from_raw(s)); }
    }
}
```

**Por qué falla**: la memoria asignada por el allocator de Rust **debe** liberarse por
el allocator de Rust. Llamar `free()` de C sobre memoria de Rust es undefined behavior
(allocators distintos).

### Error 4: no manejar punteros NULL de entrada

```rust
// MAL: asume que el puntero es válido
#[no_mangle]
pub extern "C" fn process(data: *const i32, len: i32) -> i32 {
    let slice = unsafe { std::slice::from_raw_parts(data, len as usize) };
    // Si data es NULL → undefined behavior (dereferencing null)
    slice.iter().sum()
}

// BIEN: verificar null y len
#[no_mangle]
pub extern "C" fn process(data: *const i32, len: i32) -> i32 {
    if data.is_null() || len <= 0 {
        return 0;
    }
    let slice = unsafe { std::slice::from_raw_parts(data, len as usize) };
    slice.iter().sum()
}
```

**Por qué falla**: C puede pasar NULL por error o intencionalmente. Deferenciar un puntero
NULL es UB tanto en C como en Rust. Siempre verifica antes de usar.

### Error 5: panic cruzando el boundary FFI

```rust
// MAL: panic se propaga a través de C → UB
#[no_mangle]
pub extern "C" fn parse_int(s: *const std::ffi::c_char) -> i32 {
    let c_str = unsafe { std::ffi::CStr::from_ptr(s) };
    let s = c_str.to_str().unwrap();     // Puede hacer panic
    s.parse::<i32>().unwrap()             // Puede hacer panic
}

// BIEN: usar catch_unwind o retornar Result como código
#[no_mangle]
pub extern "C" fn parse_int(s: *const std::ffi::c_char, result: *mut i32) -> i32 {
    if s.is_null() || result.is_null() {
        return -1;
    }
    match std::panic::catch_unwind(|| {
        let c_str = unsafe { std::ffi::CStr::from_ptr(s) };
        let s = c_str.to_str().map_err(|_| -2)?;
        s.parse::<i32>().map_err(|_| -3)
    }) {
        Ok(Ok(value)) => { unsafe { *result = value; } 0 }
        Ok(Err(code)) => code,
        Err(_) => -99,  // Panic
    }
}
```

**Por qué falla**: `.unwrap()` puede hacer panic si el valor es `Err` o `None`. Un panic
que cruza un stack frame de C es undefined behavior.

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════════╗
║                    #[no_mangle] y extern "C"                           ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                        ║
║  EXPORTAR FUNCIÓN                                                      ║
║  ────────────────                                                      ║
║  #[no_mangle]                        desactivar name mangling          ║
║  pub extern "C" fn name(args) -> T   ABI de C + exportar              ║
║  Siempre usar AMBOS juntos                                             ║
║                                                                        ║
║  CRATE TYPE (Cargo.toml)                                               ║
║  ────────────────────────                                               ║
║  [lib]                                                                 ║
║  crate-type = ["staticlib"]   → libfoo.a  (linkar con -lpthread etc.)  ║
║  crate-type = ["cdylib"]      → libfoo.so (auto-contenida)             ║
║                                                                        ║
║  COMPILAR DESDE C                                                      ║
║  ────────────────                                                      ║
║  cargo build --release                                                 ║
║  gcc main.c -L target/release -lmylib -lpthread -ldl -lm -o prog      ║
║                                                                        ║
║  TIPOS QUE CRUZAN FFI                                                  ║
║  ────────────────────                                                  ║
║  ✓ c_int, c_double, f32, f64, i32, u64...   primitivos                ║
║  ✓ *const T, *mut T                          punteros                  ║
║  ✓ #[repr(C)] struct                         structs C-layout          ║
║  ✗ String, Vec, Box, HashMap, Option          tipos Rust complejos     ║
║                                                                        ║
║  STRINGS                                                               ║
║  ───────                                                               ║
║  C → Rust: CStr::from_ptr(ptr).to_string_lossy()                      ║
║  Rust → C: CString::new(s).unwrap().into_raw()                         ║
║  SIEMPRE proveer rust_free_string() para liberar                       ║
║                                                                        ║
║  ARRAYS/SLICES                                                         ║
║  ─────────────                                                         ║
║  C → Rust: slice::from_raw_parts(ptr, len)     (check null first!)    ║
║  Rust → C: llenar buffer de C, o devolver ptr + proveer free           ║
║                                                                        ║
║  PANICS                                                                ║
║  ──────                                                                ║
║  panic::catch_unwind(|| { ... })    capturar en el boundary            ║
║  panic = "abort" en Cargo.toml      alternativa: abort directo         ║
║  NUNCA dejar que panic cruce un frame de C                             ║
║                                                                        ║
║  VERIFICACIÓN                                                          ║
║  ────────────                                                          ║
║  nm -D libfoo.so | grep " T "      ver símbolos exportados            ║
║  nm libfoo.a | grep " T "          ver símbolos en .a                  ║
║  ldd programa                       ver dependencias dinámicas         ║
║                                                                        ║
║  REGLAS DE ORO                                                         ║
║  ────────────                                                          ║
║  1. #[no_mangle] + pub extern "C" siempre juntos                      ║
║  2. Solo tipos FFI-safe en firmas (no String, Vec, Option)             ║
║  3. Verificar punteros NULL antes de usar                              ║
║  4. catch_unwind o panic="abort" para prevenir UB                      ║
║  5. Proveer función free para cada función que retorna puntero         ║
║  6. Memoria de Rust → free de Rust (nunca free() de C)                 ║
║                                                                        ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: librería matemática de Rust para C

Crea una librería Rust (`staticlib`) con las siguientes funciones exportadas:

1. `rust_gcd(a: i32, b: i32) -> i32` — máximo común divisor (algoritmo de Euclides).
2. `rust_is_prime(n: i32) -> i32` — retorna 1 si es primo, 0 si no.
3. `rust_nth_prime(n: i32) -> i32` — retorna el n-ésimo primo (1-indexed).

Escribe el header `.h` correspondiente y un `main.c` que llame a cada función y muestre
los resultados. Compila con `cargo build --release` + `gcc`.

**Pista**: para las dependencias de link, usa `gcc main.c -L target/release -lmylib -lpthread -ldl -lm`.

> **Pregunta de reflexión**: ¿qué pasa si `rust_nth_prime` recibe un valor negativo?
> ¿Tu implementación hace panic? Si es así, ¿cómo se comporta el programa C? ¿Cómo
> lo prevendrías?

---

### Ejercicio 2: procesamiento de arrays

Crea una librería Rust que exporte funciones para procesar arrays de C:

1. `rust_array_sum(data: *const f64, len: i32) -> f64`
2. `rust_array_mean(data: *const f64, len: i32) -> f64`
3. `rust_array_min(data: *const f64, len: i32) -> f64`
4. `rust_array_max(data: *const f64, len: i32) -> f64`
5. `rust_array_sort(data: *mut f64, len: i32)` — ordena in-place.

Cada función debe verificar que el puntero no es NULL y que len > 0. Escribe un programa C
que cree un array, llame a cada función y muestre los resultados.

**Pista**: usa `std::slice::from_raw_parts` para convertir puntero+longitud a slice, y
`from_raw_parts_mut` para la versión mutable.

> **Pregunta de reflexión**: ¿por qué verificamos que `data` no es NULL **antes** de
> llamar a `from_raw_parts`? ¿Qué pasaría si simplemente dejáramos que Rust maneje
> el null pointer? ¿Es lo mismo que en C?

---

### Ejercicio 3: string processing con gestión de memoria

Crea una librería Rust que exporte:

1. `rust_concat(a: *const c_char, b: *const c_char) -> *mut c_char` — concatena dos
   strings.
2. `rust_replace(s: *const c_char, from: *const c_char, to: *const c_char) -> *mut c_char`
   — reemplaza todas las ocurrencias.
3. `rust_split_count(s: *const c_char, delimiter: c_char) -> i32` — cuenta cuántos
   segmentos produce el split.
4. `rust_free_string(s: *mut c_char)` — libera strings retornados por Rust.

En el programa C, usa las funciones y asegúrate de llamar a `rust_free_string` para cada
puntero retornado. Verifica con `valgrind ./programa` que no hay memory leaks.

**Pista**: `CString::new(s).unwrap().into_raw()` transfiere ownership al caller.
`CString::from_raw(ptr)` la retoma para Drop.

> **Pregunta de reflexión**: ¿por qué no puedes usar `free()` de C para liberar los
> strings retornados por Rust? ¿Qué allocator usa Rust por defecto? ¿Qué pasaría en
> la práctica si mezclas allocators?
