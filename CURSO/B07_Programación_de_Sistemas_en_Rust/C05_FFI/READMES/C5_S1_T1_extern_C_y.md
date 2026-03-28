# extern "C" y #[link]

## Índice

1. [¿Qué es FFI?](#1-qué-es-ffi)
2. [ABI y calling conventions](#2-abi-y-calling-conventions)
3. [extern "C": declarar funciones externas](#3-extern-c-declarar-funciones-externas)
4. [#[link]: especificar la librería](#4-link-especificar-la-librería)
5. [Linking estático vs dinámico](#5-linking-estático-vs-dinámico)
6. [Llamar funciones de libc](#6-llamar-funciones-de-libc)
7. [Build scripts (build.rs)](#7-build-scripts-buildrs)
8. [Debugging de errores de linking](#8-debugging-de-errores-de-linking)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. ¿Qué es FFI?

FFI (Foreign Function Interface) es el mecanismo para llamar funciones escritas en otro
lenguaje desde Rust, y viceversa. En la práctica, casi siempre significa interoperar con C,
porque C define la **ABI estándar** que la mayoría de lenguajes y sistemas operativos usan
como lingua franca.

```
┌─── ¿Por qué FFI con C? ──────────────────────────────────┐
│                                                            │
│  • El kernel de Linux expone su API como funciones C       │
│  • Librerías del sistema (OpenSSL, zlib, SQLite) son C     │
│  • C define la ABI del sistema operativo                   │
│  • Bases de código existentes con millones de líneas en C  │
│  • Otros lenguajes (Python, Ruby, Go) también hablan C ABI │
│                                                            │
│  Rust ←→ C ABI ←→ { C, C++, Python, Go, ... }             │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Lo que FFI NO te da

FFI opera a nivel de **funciones y tipos primitivos**. No hay:

- Manejo automático de memoria entre lenguajes
- Traducción de errores (panic de Rust ≠ error de C)
- Tipos complejos (String de Rust ≠ char* de C)
- Seguridad de tipos entre boundaries

Todo esto es tu responsabilidad. Por eso FFI siempre implica `unsafe`.

---

## 2. ABI y calling conventions

### ¿Qué es una ABI?

ABI (Application Binary Interface) define cómo dos programas compilados se comunican a nivel
binario. Incluye:

```
┌─── Componentes de una ABI ────────────────────────────────┐
│                                                            │
│  1. Calling convention:                                    │
│     ¿Cómo se pasan argumentos? ¿En registros o stack?     │
│     ¿Quién limpia el stack, el caller o el callee?         │
│                                                            │
│  2. Name mangling:                                         │
│     ¿Cómo se nombran los símbolos en el binario?           │
│     C: "printf" → "printf"                                 │
│     C++: "ns::foo(int)" → "_ZN2ns3fooEi"                  │
│     Rust: "my::func" → "_ZN2my4func17h..."                │
│                                                            │
│  3. Layout de tipos:                                       │
│     ¿Cuántos bytes ocupa un int? ¿Alineamiento?           │
│     ¿Orden de campos en structs?                           │
│                                                            │
│  4. Return values:                                         │
│     ¿En qué registro va el valor de retorno?               │
│     ¿Y si es un struct grande?                             │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Calling conventions en Rust

```rust
// ABI de Rust (por defecto) — solo Rust la entiende
fn rust_abi(x: i32, y: i32) -> i32 {
    x + y
}

// ABI de C — compatible con cualquier lenguaje que hable C ABI
extern "C" fn c_abi(x: i32, y: i32) -> i32 {
    x + y
}

// Otras ABIs (menos comunes)
// extern "system" — stdcall en Windows, C en Unix
// extern "stdcall" — calling convention de Win32 API
// extern "fastcall" — argumentos en registros
```

La ABI de Rust **no es estable**: puede cambiar entre versiones del compilador. La ABI de C
está definida por el sistema operativo y la plataforma, y es estable por décadas.

```
┌─── ABI de Rust vs ABI de C ───────────────────────────────┐
│                                                            │
│  Rust ABI:                                                 │
│  • Layout de structs: el compilador reordena campos        │
│  • Name mangling: incluye hash                             │
│  • Calling convention: puede cambiar entre versiones       │
│  • Solo compatible consigo misma (misma versión)           │
│                                                            │
│  C ABI (extern "C"):                                       │
│  • Layout de structs: orden declarado, padding predecible  │
│  • Name mangling: ninguno (el nombre es literal)           │
│  • Calling convention: definida por la plataforma           │
│  • Compatible con todo el ecosistema nativo                │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

## 3. extern "C": declarar funciones externas

Para llamar una función C desde Rust, la **declaras** con `extern "C"` dentro de un bloque
`extern`. No la defines — el cuerpo de la función está en el código C compilado.

### Sintaxis básica

```rust
// Declarar que existe una función C llamada "abs"
// con esta firma. El linker la resolverá.
extern "C" {
    fn abs(x: std::ffi::c_int) -> std::ffi::c_int;
}

fn main() {
    // Llamar funciones extern siempre es unsafe
    let result = unsafe { abs(-42) };
    println!("abs(-42) = {}", result);  // 42
}
```

### ¿Por qué es unsafe?

Llamar funciones `extern` es `unsafe` porque el compilador **no puede verificar**:

```
┌─── Garantías que Rust NO puede comprobar ─────────────────┐
│                                                            │
│  1. Que la función exista (error de linker, no compilador) │
│  2. Que la firma sea correcta (tipos, número de args)      │
│  3. Que la función no haga memory corruption               │
│  4. Que la función no acceda a globals de forma insegura   │
│  5. Que la función sea thread-safe                         │
│  6. Que no cause undefined behavior con ciertos inputs     │
│                                                            │
│  El contrato es del programador: "yo sé lo que hago"       │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Declarar múltiples funciones

```rust
use std::ffi::c_int;

extern "C" {
    fn abs(x: c_int) -> c_int;
    fn atoi(s: *const std::ffi::c_char) -> c_int;
    fn rand() -> c_int;
    fn srand(seed: std::ffi::c_uint);
    fn getpid() -> c_int;
}

fn main() {
    unsafe {
        println!("PID: {}", getpid());
        srand(42);
        println!("Random: {}", rand());
        println!("abs(-7): {}", abs(-7));
    }
}
```

### Funciones variádicas (variadic)

Algunas funciones C aceptan un número variable de argumentos (como `printf`). En Rust se
declaran con `...`:

```rust
extern "C" {
    // int printf(const char *fmt, ...);
    fn printf(fmt: *const std::ffi::c_char, ...) -> std::ffi::c_int;
}

fn main() {
    let fmt = std::ffi::CString::new("Hello %s, you are %d years old\n").unwrap();
    let name = std::ffi::CString::new("Alice").unwrap();

    unsafe {
        printf(fmt.as_ptr(), name.as_ptr(), 30 as std::ffi::c_int);
    }
}
```

> **Nota**: las funciones variádicas son difíciles de usar correctamente desde Rust porque
> los tipos de los argumentos extra no se verifican en compilación. Prefiere alternativas
> más seguras cuando existan.

### Structs con repr(C)

Para pasar structs entre Rust y C, necesitas que Rust use el mismo layout de memoria que C:

```rust
// repr(C): usa el layout de C (orden declarado, padding estándar)
#[repr(C)]
struct Point {
    x: f64,
    y: f64,
}

#[repr(C)]
struct Rect {
    origin: Point,
    width: f64,
    height: f64,
}

extern "C" {
    fn calculate_area(rect: *const Rect) -> f64;
}
```

```
┌─── Layout sin repr(C) vs con repr(C) ─────────────────────┐
│                                                             │
│  struct Foo { a: u8, b: u32, c: u8 }                       │
│                                                             │
│  Sin repr(C) (Rust puede reordenar):                        │
│  ┌────────┬──┬──┬─────────┐                                 │
│  │ b (4B) │c │a │ pad (1) │  = 8 bytes (optimizado)         │
│  └────────┴──┴──┴─────────┘                                 │
│  El compilador reordena para minimizar padding               │
│                                                             │
│  Con repr(C) (orden fijo):                                  │
│  ┌──┬─────────┬────────┬──┬─────────┐                       │
│  │a │ pad (3) │ b (4B) │c │ pad (3) │  = 12 bytes           │
│  └──┴─────────┴────────┴──┴─────────┘                       │
│  Mismo layout que C: campos en orden declarado               │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

Sin `#[repr(C)]`, Rust puede reordenar campos para optimizar el tamaño. El código C
esperaría campos en el orden que los declaraste. La discrepancia causa lecturas de memoria
corruptas.

---

## 4. #[link]: especificar la librería

`extern "C"` declara las funciones pero no dice **dónde encontrarlas**. `#[link]` le dice
al linker qué librería contiene esas funciones.

### Link a librerías del sistema

```rust
// Buscar funciones en libm (math library)
// En Linux: busca libm.so (dinámico) o libm.a (estático)
#[link(name = "m")]
extern "C" {
    fn sqrt(x: f64) -> f64;
    fn sin(x: f64) -> f64;
    fn cos(x: f64) -> f64;
    fn pow(x: f64, y: f64) -> f64;
}

fn main() {
    unsafe {
        println!("sqrt(144) = {}", sqrt(144.0));      // 12.0
        println!("sin(π/2) = {}", sin(std::f64::consts::FRAC_PI_2));  // 1.0
        println!("2^10 = {}", pow(2.0, 10.0));        // 1024.0
    }
}
```

### ¿Cómo encuentra el linker la librería?

```
┌─── Resolución de #[link(name = "m")] ─────────────────────┐
│                                                            │
│  El linker busca (en Linux):                               │
│                                                            │
│  1. Librerías dinámicas:                                   │
│     /lib/x86_64-linux-gnu/libm.so.6                        │
│     /usr/lib/libm.so                                       │
│                                                            │
│  2. Librerías estáticas:                                   │
│     /usr/lib/libm.a                                        │
│                                                            │
│  3. Rutas en LD_LIBRARY_PATH                               │
│                                                            │
│  4. Rutas pasadas con -L (via build.rs)                    │
│                                                            │
│  El nombre "m" se traduce a "libm.so" / "libm.a"           │
│  El prefijo "lib" y la extensión los agrega el linker      │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Tipos de link

```rust
// Linking dinámico (por defecto): busca libfoo.so en runtime
#[link(name = "foo")]
extern "C" { /* ... */ }

// Equivalente explícito
#[link(name = "foo", kind = "dylib")]
extern "C" { /* ... */ }

// Linking estático: incorpora libfoo.a al binario
#[link(name = "foo", kind = "static")]
extern "C" { /* ... */ }

// Framework (solo macOS)
#[link(name = "CoreFoundation", kind = "framework")]
extern "C" { /* ... */ }
```

### libc implícito

Las funciones de la librería estándar de C (libc) se linkan automáticamente — no necesitas
`#[link]`:

```rust
// Funciones de libc NO necesitan #[link]
// libc se linka implícitamente en todos los programas
extern "C" {
    fn getpid() -> std::ffi::c_int;
    fn getuid() -> std::ffi::c_uint;
    fn sleep(seconds: std::ffi::c_uint) -> std::ffi::c_uint;
}

fn main() {
    unsafe {
        println!("PID: {}", getpid());
        println!("UID: {}", getuid());
    }
}
```

### Link a tu propia librería C

Supongamos que tienes una librería C llamada `mymath`:

```c
// mymath.h
int add(int a, int b);
double average(const double *values, int count);

// mymath.c
#include "mymath.h"

int add(int a, int b) {
    return a + b;
}

double average(const double *values, int count) {
    double sum = 0.0;
    for (int i = 0; i < count; i++) {
        sum += values[i];
    }
    return count > 0 ? sum / count : 0.0;
}
```

Compilar la librería C:

```bash
# Crear librería estática
gcc -c mymath.c -o mymath.o
ar rcs libmymath.a mymath.o

# O librería dinámica
gcc -shared -fPIC -o libmymath.so mymath.c
```

Usarla desde Rust:

```rust
use std::ffi::c_int;

#[link(name = "mymath")]
extern "C" {
    fn add(a: c_int, b: c_int) -> c_int;
    fn average(values: *const f64, count: c_int) -> f64;
}

fn main() {
    unsafe {
        println!("3 + 4 = {}", add(3, 4));

        let values = [1.0, 2.0, 3.0, 4.0, 5.0];
        let avg = average(values.as_ptr(), values.len() as c_int);
        println!("average = {}", avg);  // 3.0
    }
}
```

---

## 5. Linking estático vs dinámico

### Diferencias

```
┌─── Linking estático ──────────────────────────────────────┐
│                                                            │
│  Compilación:                                              │
│  ┌──────────┐   ┌────────────┐   ┌────────────────────┐   │
│  │ main.o   │ + │ libfoo.a   │ → │ my_program         │   │
│  └──────────┘   └────────────┘   │ (contiene todo)     │   │
│                                  └────────────────────┘   │
│                                                            │
│  Ejecución: el binario es autosuficiente                   │
│  • Mayor tamaño del binario                                │
│  • No necesita .so en el sistema                           │
│  • Más portable                                            │
│  • No se beneficia de updates de la librería               │
│                                                            │
└────────────────────────────────────────────────────────────┘

┌─── Linking dinámico ──────────────────────────────────────┐
│                                                            │
│  Compilación:                                              │
│  ┌──────────┐                   ┌────────────────────┐    │
│  │ main.o   │ + referencia   →  │ my_program         │    │
│  └──────────┘   a libfoo.so     │ (necesita libfoo)  │    │
│                                  └────────────────────┘    │
│                                                            │
│  Ejecución: busca libfoo.so en runtime                     │
│  • Binario más pequeño                                     │
│  • Requiere .so instalado en el sistema                    │
│  • Se beneficia de updates de la librería                  │
│  • Puede fallar en runtime si falta la .so                 │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Verificar linking

```bash
# Ver qué librerías dinámicas necesita un binario
ldd target/debug/my_program

# Ejemplo de output:
# linux-vdso.so.1
# libm.so.6 => /lib/x86_64-linux-gnu/libm.so.6
# libgcc_s.so.1 => /lib/x86_64-linux-gnu/libgcc_s.so.1
# libc.so.6 => /lib/x86_64-linux-gnu/libc.so.6

# Ver los símbolos que exporta/importa un binario
nm target/debug/my_program | grep " U "   # Undefined = importados
nm target/debug/my_program | grep " T "   # Text = definidos

# Ver símbolos en una librería
nm libmymath.a
```

---

## 6. Llamar funciones de libc

### El crate libc

En lugar de declarar funciones de libc manualmente, usa el crate `libc` que proporciona
bindings verificados para todas las plataformas:

```toml
[dependencies]
libc = "0.2"
```

```rust
use libc;
use std::ffi::CString;

fn main() {
    unsafe {
        // Process info
        println!("PID: {}", libc::getpid());
        println!("UID: {}", libc::getuid());
        println!("GID: {}", libc::getgid());

        // Hostname
        let mut buf = [0u8; 256];
        if libc::gethostname(buf.as_mut_ptr() as *mut libc::c_char, buf.len()) == 0 {
            let hostname = std::ffi::CStr::from_ptr(buf.as_ptr() as *const libc::c_char);
            println!("Hostname: {}", hostname.to_string_lossy());
        }

        // Environment variable
        let key = CString::new("HOME").unwrap();
        let val = libc::getenv(key.as_ptr());
        if !val.is_null() {
            let home = std::ffi::CStr::from_ptr(val);
            println!("HOME: {}", home.to_string_lossy());
        }
    }
}
```

### Manual vs crate libc

```rust
// Manual: declaras tú la función
extern "C" {
    fn getpid() -> i32;  // ← ¿Es i32? ¿O c_int? ¿Y en Windows?
}

// Con crate libc: tipos y firmas verificados por plataforma
use libc;
// libc::getpid() retorna libc::pid_t (que es c_int en Linux)
// Los tipos son correctos para cada target
```

```
┌─── Ventajas del crate libc ───────────────────────────────┐
│                                                            │
│  • Firmas verificadas para Linux, macOS, BSDs, Windows     │
│  • Tipos platform-specific correctos (pid_t, size_t, etc.) │
│  • Constantes (#define en C) disponibles como const        │
│  • Structs con repr(C) correctos (stat, sockaddr, etc.)    │
│  • Se actualiza con cada release de cada OS                │
│                                                            │
│  Declarar manualmente:                                     │
│  • Útil para aprender cómo funciona FFI                    │
│  • Necesario para librerías propias o poco comunes         │
│  • Arriesgado si no conoces la firma exacta                │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Ejemplo: llamadas al sistema

```rust
use libc;
use std::mem;

fn main() {
    // Información del sistema
    unsafe {
        // uname: información del kernel
        let mut info: libc::utsname = mem::zeroed();
        if libc::uname(&mut info) == 0 {
            let sysname = std::ffi::CStr::from_ptr(info.sysname.as_ptr());
            let release = std::ffi::CStr::from_ptr(info.release.as_ptr());
            let machine = std::ffi::CStr::from_ptr(info.machine.as_ptr());

            println!("OS: {} {}", sysname.to_string_lossy(), release.to_string_lossy());
            println!("Arch: {}", machine.to_string_lossy());
        }

        // sysconf: configuración del sistema
        let page_size = libc::sysconf(libc::_SC_PAGESIZE);
        let nprocs = libc::sysconf(libc::_SC_NPROCESSORS_ONLN);
        println!("Page size: {} bytes", page_size);
        println!("CPUs online: {}", nprocs);
    }
}
```

### Ejemplo: time y sleep

```rust
use libc;
use std::mem;

fn main() {
    unsafe {
        // time(): segundos desde epoch
        let now = libc::time(std::ptr::null_mut());
        println!("Unix timestamp: {}", now);

        // nanosleep: dormir con precisión de nanosegundos
        let req = libc::timespec {
            tv_sec: 0,
            tv_nsec: 500_000_000,  // 500 ms
        };
        let mut rem: libc::timespec = mem::zeroed();
        println!("Sleeping 500ms...");
        libc::nanosleep(&req, &mut rem);
        println!("Awake!");

        // gettimeofday: hora con microsegundos
        let mut tv: libc::timeval = mem::zeroed();
        libc::gettimeofday(&mut tv, std::ptr::null_mut());
        println!("Time: {}.{:06}", tv.tv_sec, tv.tv_usec);
    }
}
```

---

## 7. Build scripts (build.rs)

Cuando necesitas compilar código C como parte de tu proyecto Rust, o encontrar librerías
en rutas no estándar, usas un **build script**.

### Estructura del proyecto

```
my_project/
├── Cargo.toml
├── build.rs          ← Build script (se ejecuta antes de compilar src/)
├── src/
│   └── main.rs
└── c_src/            ← Código fuente C
    ├── mymath.h
    └── mymath.c
```

### Compilar C con el crate cc

```toml
# Cargo.toml
[build-dependencies]
cc = "1"
```

```rust
// build.rs
fn main() {
    // Compilar mymath.c y crear libmymath.a
    cc::Build::new()
        .file("c_src/mymath.c")
        .compile("mymath");
    // "mymath" → produce libmymath.a y lo agrega al link path

    // Recompilar si el código C cambia
    println!("cargo::rerun-if-changed=c_src/mymath.c");
    println!("cargo::rerun-if-changed=c_src/mymath.h");
}
```

```rust
// src/main.rs
use std::ffi::c_int;

extern "C" {
    fn add(a: c_int, b: c_int) -> c_int;
    fn average(values: *const f64, count: c_int) -> f64;
}

fn main() {
    unsafe {
        println!("add(3, 4) = {}", add(3, 4));

        let vals = [10.0, 20.0, 30.0];
        println!("avg = {}", average(vals.as_ptr(), vals.len() as c_int));
    }
}
```

### cc::Build opciones

```rust
// build.rs — configuración avanzada
fn main() {
    cc::Build::new()
        .file("c_src/foo.c")
        .file("c_src/bar.c")           // Múltiples archivos
        .include("c_src/include")       // -I para headers
        .define("DEBUG", "1")           // -DDEBUG=1
        .flag("-Wall")                  // Flag extra para gcc/clang
        .opt_level(2)                   // -O2
        .compile("mylib");

    // Indicar al linker dónde buscar librerías externas
    println!("cargo::rustc-link-search=native=/usr/local/lib");

    // Linkar con una librería del sistema
    println!("cargo::rustc-link-lib=z");  // libz (zlib)
}
```

### Instrucciones cargo:: en build.rs

```rust
// build.rs — comunicación con Cargo
fn main() {
    // Agregar ruta de búsqueda de librerías
    println!("cargo::rustc-link-search=native=/opt/mylib/lib");

    // Linkar con una librería
    println!("cargo::rustc-link-lib=mylib");            // dinámico (por defecto)
    println!("cargo::rustc-link-lib=static=mylib");     // estático
    println!("cargo::rustc-link-lib=dylib=mylib");      // dinámico explícito

    // Recompilar si estos archivos cambian
    println!("cargo::rerun-if-changed=c_src/");
    println!("cargo::rerun-if-changed=build.rs");

    // Recompilar si una variable de entorno cambia
    println!("cargo::rerun-if-env-changed=MYLIB_PATH");

    // Pasar un valor al código Rust
    println!("cargo::rustc-cfg=has_feature_x");
    // Luego en Rust: #[cfg(has_feature_x)]
}
```

```
┌─── Flujo de compilación con build.rs ─────────────────────┐
│                                                            │
│  cargo build                                               │
│       │                                                    │
│       ▼                                                    │
│  1. Compila build.rs                                       │
│       │                                                    │
│       ▼                                                    │
│  2. Ejecuta build.rs                                       │
│     • cc::Build compila C → libmylib.a                     │
│     • Emite cargo::rustc-link-* a stdout                   │
│       │                                                    │
│       ▼                                                    │
│  3. Cargo lee las instrucciones de build.rs                 │
│     • Agrega libmylib.a al link path                       │
│       │                                                    │
│       ▼                                                    │
│  4. Compila src/main.rs                                    │
│     • extern "C" se resuelve contra libmylib.a             │
│       │                                                    │
│       ▼                                                    │
│  5. Link final → binario ejecutable                        │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### pkg-config para encontrar librerías del sistema

```toml
# Cargo.toml
[build-dependencies]
pkg-config = "0.3"
```

```rust
// build.rs
fn main() {
    // pkg-config busca la librería y emite las instrucciones de link
    pkg_config::Config::new()
        .atleast_version("1.2")
        .probe("zlib")
        .expect("zlib not found. Install with: sudo dnf install zlib-devel");

    // pkg-config automáticamente emite:
    // cargo::rustc-link-lib=z
    // cargo::rustc-link-search=/usr/lib64/
    // etc.
}
```

---

## 8. Debugging de errores de linking

### Error: undefined reference

```
error: linking with `cc` failed
  = note: /usr/bin/ld: main.o: undefined reference to `my_function'
```

**Causa**: la función existe en el código fuente pero no en la librería que se linka.

**Soluciones**:
```bash
# ¿La función existe en la librería?
nm -D libmylib.so | grep my_function     # Dinámico
nm libmylib.a | grep my_function          # Estático

# ¿La librería está en el search path?
# En build.rs: cargo::rustc-link-search=native=/path/to/lib

# ¿El nombre del símbolo coincide?
# C++ hace name mangling — necesitas extern "C" en el header C++
```

### Error: cannot find -lfoo

```
error: linking with `cc` failed
  = note: /usr/bin/ld: cannot find -lmylib
```

**Causa**: el linker no encuentra `libmylib.so` ni `libmylib.a`.

**Soluciones**:
```bash
# ¿Dónde está la librería?
find /usr -name "libmylib*" 2>/dev/null
ldconfig -p | grep mylib

# Agregar la ruta en build.rs
# println!("cargo::rustc-link-search=native=/path/to/dir");

# O instalar el paquete -devel correspondiente
# sudo dnf install mylib-devel
```

### Error: wrong ELF class

```
/usr/bin/ld: libfoo.a: error adding symbols: file format not recognized
```

**Causa**: la librería fue compilada para una arquitectura diferente (e.g., 32-bit vs 64-bit).

```bash
# Verificar arquitectura de la librería
file libfoo.a
# Debería ser: ELF 64-bit LSB relocatable, x86-64

# Verificar tu target
rustc --print target-list | grep x86
```

### Error en runtime: cannot open shared object

```
error while loading shared libraries: libfoo.so: cannot open shared object file
```

**Causa**: la librería dinámica no está en el runtime path.

```bash
# Verificar qué necesita el binario
ldd target/debug/my_program

# Agregar al path de búsqueda
export LD_LIBRARY_PATH=/path/to/lib:$LD_LIBRARY_PATH

# O instalarlo permanentemente
sudo cp libfoo.so /usr/local/lib/
sudo ldconfig
```

---

## 9. Errores comunes

### Error 1: firma incorrecta en extern

```rust
// MAL: tipos de Rust no coinciden con tipos de C
extern "C" {
    // C: int abs(int x);
    fn abs(x: i64) -> i64;  // ← i64 ≠ c_int (que es i32 en la mayoría)
}

fn main() {
    unsafe {
        // Puede funcionar en algunas plataformas por coincidencia,
        // pero es undefined behavior
        println!("{}", abs(-42));
    }
}

// BIEN: usar tipos de std::ffi que coinciden con los de C
use std::ffi::c_int;

extern "C" {
    fn abs(x: c_int) -> c_int;
}
```

**Por qué falla**: `int` en C es `c_int` en Rust (típicamente 32 bits). Si declaras `i64`,
el calling convention pasa el valor en el registro equivocado o con el tamaño incorrecto.
Puede funcionar "de casualidad" y fallar después — la peor clase de bug.

### Error 2: olvidar repr(C) en structs

```rust
// MAL: Rust puede reordenar los campos
struct Point {
    x: f64,
    y: f64,
}

extern "C" {
    fn process_point(p: *const Point);
    // C espera x primero, luego y.
    // Rust puede reordenar → C lee y donde espera x
}

// BIEN: repr(C) garantiza el layout de C
#[repr(C)]
struct Point {
    x: f64,
    y: f64,
}
```

**Por qué falla**: sin `#[repr(C)]`, el compilador Rust puede reordenar los campos del
struct para optimizar alineamiento. El código C asume el orden de declaración. El resultado
es corrupción silenciosa de datos.

### Error 3: pasar String de Rust como char*

```rust
// MAL: &str de Rust NO es un C string (no tiene null terminator)
extern "C" {
    fn puts(s: *const std::ffi::c_char) -> std::ffi::c_int;
}

fn main() {
    let msg = "Hello, world!";
    unsafe {
        // msg.as_ptr() apunta a bytes UTF-8 SIN null terminator
        // puts lee hasta encontrar un \0 → lee basura más allá del string
        puts(msg.as_ptr() as *const std::ffi::c_char);  // ← UB!
    }
}

// BIEN: usar CString que agrega el null terminator
use std::ffi::CString;

fn main() {
    let msg = CString::new("Hello, world!").unwrap();
    unsafe {
        puts(msg.as_ptr());  // Correcto: tiene \0 al final
    }
}
```

**Por qué falla**: los strings de Rust (`&str`, `String`) almacenan longitud explícita y
**no tienen null terminator**. Las funciones C esperan `char*` terminado en `\0`. Sin él,
la función C lee más allá del buffer → undefined behavior.

### Error 4: CString dropeado antes de usar el puntero

```rust
use std::ffi::{CString, c_char};

extern "C" {
    fn puts(s: *const c_char) -> std::ffi::c_int;
}

// MAL: CString se dropea antes de que puts lo use
fn bad() {
    let ptr = CString::new("hello").unwrap().as_ptr();
    // CString temporal se dropea aquí ↑
    // ptr ahora apunta a memoria liberada
    unsafe { puts(ptr); }  // ← use-after-free!
}

// BIEN: mantener CString vivo mientras se usa el puntero
fn good() {
    let msg = CString::new("hello").unwrap();
    unsafe { puts(msg.as_ptr()); }
    // msg se dropea DESPUÉS de puts
}
```

**Por qué falla**: `CString::new("hello").unwrap().as_ptr()` crea un `CString` temporal que
se destruye al final de la expresión. El puntero queda colgando (dangling pointer).

### Error 5: no verificar valores de retorno de funciones C

```rust
use libc;
use std::ffi::CString;

// MAL: ignorar el código de error
fn bad_open() {
    let path = CString::new("/nonexistent/file").unwrap();
    unsafe {
        let fd = libc::open(path.as_ptr(), libc::O_RDONLY);
        // fd es -1 si falló, pero lo usamos sin verificar
        let mut buf = [0u8; 1024];
        libc::read(fd, buf.as_mut_ptr() as *mut libc::c_void, buf.len());
        // Leer de fd=-1 es undefined behavior
    }
}

// BIEN: verificar retorno y consultar errno
fn good_open() -> Result<(), String> {
    let path = CString::new("/nonexistent/file").unwrap();
    unsafe {
        let fd = libc::open(path.as_ptr(), libc::O_RDONLY);
        if fd == -1 {
            let err = std::io::Error::last_os_error();
            return Err(format!("open failed: {}", err));
        }

        // Usar fd...

        libc::close(fd);
    }
    Ok(())
}
```

**Por qué falla**: las funciones C reportan errores mediante valores de retorno (típicamente
-1 o NULL) y `errno`. Ignorar el retorno y usar un valor inválido causa crashes o corrupción.

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════════╗
║                       extern "C" y #[link]                             ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                        ║
║  DECLARAR FUNCIONES C                                                  ║
║  ────────────────────                                                  ║
║  extern "C" {                                                          ║
║      fn nombre(args: tipos_ffi) -> retorno;                            ║
║  }                                                                     ║
║  unsafe { nombre(args) }         // llamar siempre en unsafe           ║
║                                                                        ║
║  LINKAR LIBRERÍA                                                       ║
║  ───────────────                                                       ║
║  #[link(name = "foo")]           // busca libfoo.so / libfoo.a         ║
║  #[link(name = "foo", kind = "static")]   // forzar estático           ║
║  libc se linka implícitamente (sin #[link])                            ║
║                                                                        ║
║  TIPOS FFI (std::ffi)                                                  ║
║  ────────────────────                                                  ║
║  c_int, c_uint, c_long, c_ulong   // enteros de C                     ║
║  c_char, c_uchar                   // char de C                        ║
║  c_float, c_double                 // punto flotante                   ║
║  c_void                            // void (para punteros)             ║
║  CString::new("hi").unwrap()       // Rust String → C string (\0)     ║
║  CStr::from_ptr(ptr)               // C string → &CStr               ║
║                                                                        ║
║  STRUCTS                                                               ║
║  ───────                                                               ║
║  #[repr(C)]                        // OBLIGATORIO para structs FFI     ║
║  struct Foo { x: c_int, y: c_int } // layout idéntico a C             ║
║                                                                        ║
║  BUILD.RS                                                              ║
║  ────────                                                              ║
║  cc::Build::new()                                                      ║
║      .file("c_src/foo.c")                                              ║
║      .include("c_src/")                                                ║
║      .compile("foo");                                                  ║
║  println!("cargo::rerun-if-changed=c_src/");                           ║
║  println!("cargo::rustc-link-search=native=/path");                    ║
║  println!("cargo::rustc-link-lib=nombre");                             ║
║                                                                        ║
║  CRATE LIBC                                                            ║
║  ──────────                                                            ║
║  libc = "0.2"                      // bindings verificados             ║
║  libc::getpid(), libc::open(), libc::read(), etc.                      ║
║  Tipos: libc::pid_t, libc::size_t, libc::c_int, etc.                  ║
║                                                                        ║
║  DEBUGGING                                                             ║
║  ─────────                                                             ║
║  ldd binario                       // dependencias dinámicas           ║
║  nm -D libfoo.so | grep sym        // buscar símbolo en .so            ║
║  nm libfoo.a | grep sym            // buscar símbolo en .a             ║
║  file libfoo.a                     // verificar arquitectura           ║
║  pkg-config --libs libname         // flags de link                    ║
║                                                                        ║
║  REGLAS DE ORO                                                         ║
║  ────────────                                                          ║
║  • Siempre #[repr(C)] en structs que cruzan el boundary                ║
║  • Siempre CString para pasar strings a C (nunca &str)                 ║
║  • Siempre verificar valores de retorno de funciones C                 ║
║  • Mantener CString vivo mientras se usa .as_ptr()                     ║
║  • Usar std::ffi::c_int en vez de i32 (portabilidad)                   ║
║                                                                        ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: llamar funciones de libc manualmente

Sin usar el crate `libc`, declara y llama las siguientes funciones con `extern "C"`:
1. `getpid()` — obtener el PID del proceso actual.
2. `getuid()` — obtener el UID del usuario.
3. `time(NULL)` — obtener el timestamp Unix actual.
4. `abs(-42)` — valor absoluto.

Para cada una, usa los tipos de `std::ffi` (`c_int`, `c_uint`, etc.). Muestra los resultados
por pantalla.

Después, reescribe el programa usando el crate `libc` y compara: ¿cuántas líneas te ahorras?
¿Qué tipo de errores evitas?

**Pista**: `time()` recibe un `*mut time_t`. Puedes pasar `std::ptr::null_mut()` como
equivalente de NULL.

> **Pregunta de reflexión**: ¿Qué pasaría si declaras `getpid` como `fn getpid() -> i64`
> en una plataforma donde `pid_t` es `i32`? ¿El programa compilaría? ¿Se ejecutaría
> correctamente? ¿Por qué es peligroso aunque "funcione"?

---

### Ejercicio 2: compilar y usar una librería C propia

Crea un proyecto Rust con una librería C integrada:
1. Escribe `c_src/mathutils.c` con dos funciones:
   - `int factorial(int n)` — factorial iterativo.
   - `int fibonacci(int n)` — n-ésimo número de Fibonacci.
2. Escribe `c_src/mathutils.h` con las declaraciones.
3. Crea un `build.rs` que use el crate `cc` para compilar `mathutils.c`.
4. En `src/main.rs`, declara las funciones con `extern "C"` y llámalas para
   `n = 0, 1, 5, 10, 20`.
5. Agrega `println!("cargo::rerun-if-changed=c_src/")` en build.rs.

Verifica que `cargo build` compila el C automáticamente y que `cargo run` muestra los
resultados correctos.

**Pista**: la estructura del proyecto debe ser: `Cargo.toml`, `build.rs`, `c_src/mathutils.c`,
`c_src/mathutils.h`, `src/main.rs`. En `Cargo.toml` agrega `[build-dependencies] cc = "1"`.

> **Pregunta de reflexión**: si modificas `mathutils.c` y ejecutas `cargo build` de nuevo,
> ¿se recompila automáticamente? ¿Qué papel juega `cargo::rerun-if-changed`? ¿Qué pasa
> si lo omites?

---

### Ejercicio 3: wrapper seguro para gethostname

Crea una función Rust segura (sin `unsafe` en la firma) que encapsule `gethostname()`:
1. Declara `gethostname` con `extern "C"` (firma: `fn gethostname(name: *mut c_char, len: size_t) -> c_int`).
2. Escribe `fn get_hostname() -> Result<String, std::io::Error>` que:
   - Cree un buffer de 256 bytes.
   - Llame a `gethostname` dentro de un bloque `unsafe`.
   - Verifique el valor de retorno (0 = éxito, -1 = error).
   - Si falla, retorne `std::io::Error::last_os_error()`.
   - Si tiene éxito, convierta el `*c_char` a `String` usando `CStr::from_ptr`.
3. En `main`, llama a `get_hostname()` sin necesidad de `unsafe`.

**Pista**: para el buffer, usa `let mut buf = vec![0u8; 256]` y castea con
`buf.as_mut_ptr() as *mut c_char`.

> **Pregunta de reflexión**: la función `get_hostname()` es segura para el caller, pero
> internamente usa `unsafe`. ¿Qué responsabilidades asume el autor de esta función? ¿Qué
> invariantes debe garantizar para que el `unsafe` interno no cause problemas?
