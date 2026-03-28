# extern functions: llamar código externo siempre es unsafe

## Índice

1. [¿Por qué extern es unsafe?](#por-qué-extern-es-unsafe)
2. [extern "C": el ABI por defecto para FFI](#extern-c-el-abi-por-defecto-para-ffi)
3. [Llamar funciones de libc](#llamar-funciones-de-libc)
4. [Otros ABIs disponibles](#otros-abis-disponibles)
5. [Variadic functions (funciones con argumentos variables)](#variadic-functions)
6. [Function pointers y callbacks desde C](#function-pointers-y-callbacks-desde-c)
7. [Interacción con errno](#interacción-con-errno)
8. [Combinando los 5 superpoderes: ejemplo integrador](#combinando-los-5-superpoderes-ejemplo-integrador)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Por qué extern es unsafe?

Llamar a una función externa (declarada con `extern`) es **siempre unsafe** porque el compilador de Rust no puede verificar:

1. **Que la función existe** con esa firma exacta.
2. **Que los tipos coinciden** entre la declaración y la implementación real.
3. **Que la función no causa UB** (acceso a memoria inválida, data races, etc.).
4. **Que la función cumple las precondiciones** que Rust asume (punteros válidos, etc.).

```
┌──────────────────────────────────────────────────────┐
│        ¿Por qué extern siempre es unsafe?            │
├──────────────────────────────────────────────────────┤
│                                                      │
│  Rust compiler                    C compiler         │
│  ┌──────────────┐                ┌──────────────┐   │
│  │ extern "C" { │   ← link →    │ int foo() {  │   │
│  │   fn foo()→i32│              │   return 42; │   │
│  │ }             │              │ }             │   │
│  └──────────────┘                └──────────────┘   │
│         │                              │             │
│         │   El compilador de Rust      │             │
│         │   NO ve este código ─────────┘             │
│         │                                            │
│         └→ Confía CIEGAMENTE en tu declaración       │
│            Si la firma es incorrecta → UB            │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### Comparación: función Rust vs función extern

```rust
// Función Rust: el compilador verifica TODO
fn safe_add(a: i32, b: i32) -> i32 {
    a + b  // tipos verificados, overflow detectado en debug
}

// Función extern: el compilador verifica NADA del otro lado
extern "C" {
    fn external_add(a: i32, b: i32) -> i32;
    // ¿Existe? No sé
    // ¿Retorna i32? No sé
    // ¿Modifica estado global? No sé
    // ¿Causa segfault? No sé
}

fn main() {
    let a = safe_add(1, 2);  // safe

    // unsafe: yo garantizo que external_add existe, tiene esa firma,
    // y no causa UB
    let b = unsafe { external_add(1, 2) };
}
```

---

## extern "C": el ABI por defecto para FFI

`extern "C"` especifica que la función usa la convención de llamada de C, que es el estándar de facto para interoperabilidad entre lenguajes.

### Declarar funciones externas

```rust
// Bloque extern: declara funciones que existen en otra biblioteca
extern "C" {
    fn abs(x: i32) -> i32;
    fn sqrt(x: f64) -> f64;
    fn strlen(s: *const std::ffi::c_char) -> usize;
}

fn main() {
    unsafe {
        println!("abs(-42) = {}", abs(-42));       // 42
        println!("sqrt(2.0) = {}", sqrt(2.0));     // 1.4142...

        let s = std::ffi::CString::new("hello").unwrap();
        println!("strlen = {}", strlen(s.as_ptr())); // 5
    }
}
```

### Con #[link]: especificar la biblioteca

```rust
// Enlazar con libm (matemáticas)
#[link(name = "m")]
extern "C" {
    fn ceil(x: f64) -> f64;
    fn floor(x: f64) -> f64;
    fn pow(base: f64, exp: f64) -> f64;
}

fn main() {
    unsafe {
        println!("ceil(3.2) = {}", ceil(3.2));     // 4
        println!("floor(3.8) = {}", floor(3.8));   // 3
        println!("2^10 = {}", pow(2.0, 10.0));     // 1024
    }
}
```

### extern sin "C" (Rust ABI)

```rust
// Sin especificar ABI: usa el ABI de Rust (inestable, no para FFI)
extern {
    // Esto es extern "Rust" implícitamente — raro verlo
}

// extern "C" es lo que usarás en el 99% de los casos para FFI
```

> **Predicción**: ¿Qué pasa si declaras `fn abs(x: i64) -> i64` (con i64 en vez de i32) en el bloque extern? Compila sin problemas, pero al llamarla pasarás 8 bytes cuando C espera 4 → UB silencioso.

---

## Llamar funciones de libc

### Directamente (declaración manual)

```rust
use std::ffi::{c_int, c_char, CStr, CString};

extern "C" {
    fn getpid() -> c_int;
    fn getuid() -> u32;
    fn gethostname(name: *mut c_char, len: usize) -> c_int;
}

fn main() {
    unsafe {
        println!("PID: {}", getpid());
        println!("UID: {}", getuid());

        // gethostname: escribe en un buffer
        let mut buf = vec![0u8; 256];
        let ret = gethostname(buf.as_mut_ptr() as *mut c_char, buf.len());
        if ret == 0 {
            let hostname = CStr::from_ptr(buf.as_ptr() as *const c_char);
            println!("hostname: {}", hostname.to_string_lossy());
        }
    }
}
```

### Con el crate libc (recomendado)

El crate `libc` proporciona declaraciones correctas y completas para todas las funciones de libc:

```toml
# Cargo.toml
[dependencies]
libc = "0.2"
```

```rust
use std::ffi::CStr;

fn main() {
    unsafe {
        // getpid y getuid ya están declarados correctamente en libc
        println!("PID: {}", libc::getpid());
        println!("UID: {}", libc::getuid());

        // time
        let now = libc::time(std::ptr::null_mut());
        println!("epoch: {}", now);

        // uname: información del sistema
        let mut info: libc::utsname = std::mem::zeroed();
        if libc::uname(&mut info) == 0 {
            let sysname = CStr::from_ptr(info.sysname.as_ptr());
            let release = CStr::from_ptr(info.release.as_ptr());
            let machine = CStr::from_ptr(info.machine.as_ptr());
            println!("{} {} {}",
                sysname.to_string_lossy(),
                release.to_string_lossy(),
                machine.to_string_lossy(),
            );
        }

        // malloc/free (raro usarlos en Rust, pero posible)
        let ptr = libc::malloc(100) as *mut u8;
        if !ptr.is_null() {
            std::ptr::write_bytes(ptr, 0, 100); // memset a 0
            *ptr = 42;
            println!("allocated and wrote: {}", *ptr);
            libc::free(ptr as *mut libc::c_void);
        }
    }
}
```

### Operaciones de archivos con libc

```rust
use std::ffi::{CStr, CString};

fn main() {
    unsafe {
        // open → write → read → close
        let path = CString::new("/tmp/rust_extern_test.txt").unwrap();

        // O_WRONLY | O_CREAT | O_TRUNC
        let fd = libc::open(
            path.as_ptr(),
            libc::O_WRONLY | libc::O_CREAT | libc::O_TRUNC,
            0o644,
        );

        if fd < 0 {
            eprintln!("open failed");
            return;
        }

        let message = b"hello from extern!\n";
        let written = libc::write(
            fd,
            message.as_ptr() as *const libc::c_void,
            message.len(),
        );
        println!("wrote {} bytes", written);
        libc::close(fd);

        // Leer de vuelta
        let fd = libc::open(path.as_ptr(), libc::O_RDONLY);
        if fd >= 0 {
            let mut buf = [0u8; 256];
            let n = libc::read(
                fd,
                buf.as_mut_ptr() as *mut libc::c_void,
                buf.len(),
            );
            if n > 0 {
                let content = std::str::from_utf8(&buf[..n as usize]).unwrap();
                print!("read back: {}", content);
            }
            libc::close(fd);
        }

        // Limpiar
        libc::unlink(path.as_ptr());
    }
}
```

---

## Otros ABIs disponibles

Rust soporta varios ABIs además de `"C"`:

```rust
// Los ABIs más relevantes:
extern "C"          { /* ... */ }  // ABI de C — el estándar para FFI
extern "system"     { /* ... */ }  // stdcall en Windows, C en otros
extern "C-unwind"   { /* ... */ }  // C que permite unwinding de panics
extern "stdcall"    { /* ... */ }  // Windows API
extern "fastcall"   { /* ... */ }  // Registros para primeros args
extern "cdecl"      { /* ... */ }  // Caller limpia stack (default en C x86)

// Para exportar funciones Rust con un ABI específico:
pub extern "C" fn my_function(x: i32) -> i32 {
    x * 2
}
```

### extern "system": portable Windows/Linux

```rust
// En Windows: usa stdcall (convención de Windows API)
// En Linux/macOS: usa C (la convención estándar)
extern "system" {
    // Esto funciona en ambas plataformas
}

// Útil para código que debe compilar en Windows y Linux
// sin #[cfg] para cada función
```

### extern "C-unwind": panics a través de FFI

```rust
// Rust 1.71+: permite que un panic cruce el boundary de FFI
// En lugar de abortar, el panic se propaga correctamente

extern "C-unwind" {
    fn might_call_rust_that_panics();
}

// Si exportas una función que podría paniquear:
pub extern "C-unwind" fn callback() {
    panic!("this panic can cross FFI boundary safely");
}

// Sin "C-unwind", un panic en extern "C" es UB
// (en la práctica: abort)
```

```
┌────────────────────────────────────────────────────┐
│          extern "C" vs "C-unwind"                  │
├────────────────────────────────────────────────────┤
│                                                    │
│  extern "C":                                       │
│  - Panic en callback → abort (UB técnicamente)     │
│  - Más eficiente (no prepara unwinding)            │
│  - Usar cuando el código NUNCA paniquea             │
│                                                    │
│  extern "C-unwind":                                │
│  - Panic en callback → se propaga correctamente    │
│  - Ligero overhead de unwinding                    │
│  - Usar cuando callbacks Rust podrían paniquear    │
│                                                    │
└────────────────────────────────────────────────────┘
```

---

## Variadic functions

Algunas funciones C aceptan un número variable de argumentos (como `printf`). Rust puede llamarlas pero no definirlas (salvo en nightly).

### Llamar funciones variádicas

```rust
extern "C" {
    // El ... indica argumentos variables
    fn printf(format: *const std::ffi::c_char, ...) -> i32;
    fn snprintf(
        buf: *mut std::ffi::c_char,
        size: usize,
        format: *const std::ffi::c_char,
        ...
    ) -> i32;
}

fn main() {
    unsafe {
        let fmt = b"Hello %s, you are %d years old\n\0".as_ptr()
            as *const std::ffi::c_char;
        let name = b"Rust\0".as_ptr() as *const std::ffi::c_char;

        printf(fmt, name, 9i32);
        // Hello Rust, you are 9 years old

        // snprintf: escribir en buffer
        let mut buf = [0u8; 256];
        let fmt2 = b"PI = %.4f\0".as_ptr() as *const std::ffi::c_char;
        let n = snprintf(
            buf.as_mut_ptr() as *mut std::ffi::c_char,
            buf.len(),
            fmt2,
            std::f64::consts::PI,  // variadic: f64 se pasa como double
        );

        if n > 0 {
            let result = std::str::from_utf8(&buf[..n as usize]).unwrap();
            println!("snprintf result: {}", result); // PI = 3.1416
        }
    }
}
```

> **Cuidado**: en funciones variádicas de C, los tipos `float` se promueven a `double` y los tipos menores que `int` se promueven a `int`. Siempre pasa `f64` (no `f32`) y `i32`/`u32` como mínimo.

### open() como ejemplo de variádica real

```rust
fn main() {
    unsafe {
        let path = std::ffi::CString::new("/tmp/test.txt").unwrap();

        // open() tiene firma: int open(const char *path, int flags, ...);
        // El tercer argumento (mode) solo se pasa con O_CREAT
        let fd = libc::open(
            path.as_ptr(),
            libc::O_WRONLY | libc::O_CREAT | libc::O_TRUNC,
            0o644 as libc::mode_t, // argumento variádico
        );

        if fd >= 0 {
            libc::close(fd);
        }
    }
}
```

---

## Function pointers y callbacks desde C

### Pasar function pointers a C

```rust
extern "C" {
    // qsort: ordenar un array usando un comparador proporcionado por el caller
    fn qsort(
        base: *mut std::ffi::c_void,
        num: usize,
        size: usize,
        compar: extern "C" fn(
            *const std::ffi::c_void,
            *const std::ffi::c_void,
        ) -> i32,
    );
}

// El comparador debe ser extern "C" — función libre, no closure
extern "C" fn compare_i32(a: *const std::ffi::c_void, b: *const std::ffi::c_void) -> i32 {
    unsafe {
        let a = *(a as *const i32);
        let b = *(b as *const i32);
        a - b // negativo si a<b, 0 si a==b, positivo si a>b
    }
}

fn main() {
    let mut data = [50, 20, 40, 10, 30];

    unsafe {
        qsort(
            data.as_mut_ptr() as *mut std::ffi::c_void,
            data.len(),
            std::mem::size_of::<i32>(),
            compare_i32,
        );
    }

    println!("{:?}", data); // [10, 20, 30, 40, 50]
}
```

### Callbacks con user_data (void pointer)

Muchas APIs de C pasan un `void*` de "user data" al callback para permitir contexto:

```rust
use std::ffi::c_void;

// Simulación de API C con callback + user data
mod c_api {
    use std::ffi::c_void;

    pub type Callback = extern "C" fn(value: i32, user_data: *mut c_void);

    pub unsafe fn process_array(
        data: *const i32,
        len: usize,
        callback: Callback,
        user_data: *mut c_void,
    ) {
        for i in 0..len {
            callback(*data.add(i), user_data);
        }
    }
}

// Nuestro contexto
struct Accumulator {
    sum: i64,
    count: u32,
}

// Callback extern "C" que accede al contexto via user_data
extern "C" fn accumulate(value: i32, user_data: *mut c_void) {
    // SAFETY: el caller garantiza que user_data apunta a un Accumulator válido
    let acc = unsafe { &mut *(user_data as *mut Accumulator) };
    acc.sum += value as i64;
    acc.count += 1;
}

fn main() {
    let data = [10, 20, 30, 40, 50];
    let mut acc = Accumulator { sum: 0, count: 0 };

    unsafe {
        c_api::process_array(
            data.as_ptr(),
            data.len(),
            accumulate,
            &mut acc as *mut Accumulator as *mut c_void,
        );
    }

    println!("sum: {}, count: {}, avg: {}",
        acc.sum, acc.count, acc.sum / acc.count as i64);
    // sum: 150, count: 5, avg: 30
}
```

```
┌───────────────────────────────────────────────────────┐
│        Patrón callback + user_data                    │
├───────────────────────────────────────────────────────┤
│                                                       │
│  Rust                          "C API"                │
│  ────                          ──────                 │
│  ┌──────────────┐              ┌──────────────────┐  │
│  │ Accumulator  │◄─ user_data ─│ process_array()  │  │
│  │  sum: 150    │              │                  │  │
│  │  count: 5    │  callback ──→│  accumulate()    │  │
│  └──────────────┘              └──────────────────┘  │
│                                                       │
│  1. Crear struct con contexto                         │
│  2. Pasar &mut struct como *mut c_void                │
│  3. En callback: cast *mut c_void → &mut Struct       │
│  4. Usar el struct normalmente                        │
│                                                       │
└───────────────────────────────────────────────────────┘
```

### Option<extern "C" fn> como callback opcional

Gracias a la niche optimization, `Option<extern "C" fn(...)>` tiene el mismo tamaño que un function pointer (null = None):

```rust
extern "C" {
    // Muchas APIs C aceptan NULL como "sin callback"
    fn signal(
        signum: i32,
        handler: Option<extern "C" fn(i32)>,
    ) -> Option<extern "C" fn(i32)>;
}

extern "C" fn my_handler(sig: i32) {
    // NOTA: en un handler de señal real, solo operaciones async-signal-safe
    // Escribir a stdout NO es safe en un handler de señal
    unsafe {
        let msg = b"Signal received\n";
        libc::write(1, msg.as_ptr() as *const libc::c_void, msg.len());
    }
}

fn main() {
    unsafe {
        // Registrar handler
        signal(libc::SIGUSR1, Some(my_handler));
        // Desregistrar (restaurar default)
        // signal(libc::SIGUSR1, None);
    }
}
```

---

## Interacción con errno

Muchas funciones C indican error retornando -1 y poniendo un código en `errno` (variable global thread-local).

### Leer errno

```rust
use std::ffi::CString;
use std::io;

fn main() {
    unsafe {
        let bad_path = CString::new("/nonexistent/file.txt").unwrap();
        let fd = libc::open(bad_path.as_ptr(), libc::O_RDONLY);

        if fd < 0 {
            // errno se accede via io::Error::last_os_error() en Rust
            let err = io::Error::last_os_error();
            println!("open failed: {} (errno {})", err, err.raw_os_error().unwrap());
            // open failed: No such file or directory (errno 2)
        }

        // O directamente con libc:
        let errno_val = *libc::__errno_location(); // Linux
        println!("raw errno: {}", errno_val); // 2 (ENOENT)
    }
}
```

### Patrón: wrapper que convierte errno en Result

```rust
use std::ffi::CString;
use std::io;

fn safe_open(path: &str, flags: i32) -> io::Result<i32> {
    let c_path = CString::new(path)
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "null byte in path"))?;

    // SAFETY: c_path es un CString válido (null-terminated, sin nulls internos)
    let fd = unsafe { libc::open(c_path.as_ptr(), flags) };

    if fd < 0 {
        Err(io::Error::last_os_error())
    } else {
        Ok(fd)
    }
}

fn safe_close(fd: i32) -> io::Result<()> {
    // SAFETY: fd debe ser un file descriptor válido
    let ret = unsafe { libc::close(fd) };
    if ret < 0 {
        Err(io::Error::last_os_error())
    } else {
        Ok(())
    }
}

fn main() {
    match safe_open("/etc/hostname", libc::O_RDONLY) {
        Ok(fd) => {
            println!("opened fd: {}", fd);
            let mut buf = [0u8; 256];
            let n = unsafe {
                libc::read(fd, buf.as_mut_ptr() as *mut libc::c_void, buf.len())
            };
            if n > 0 {
                let hostname = std::str::from_utf8(&buf[..n as usize])
                    .unwrap_or("(invalid utf8)");
                print!("hostname: {}", hostname);
            }
            safe_close(fd).unwrap();
        }
        Err(e) => eprintln!("failed: {}", e),
    }
}
```

---

## Combinando los 5 superpoderes: ejemplo integrador

Este ejemplo usa los 5 superpoderes de unsafe juntos para crear un wrapper completo sobre una API de sistema:

```rust
use std::ffi::{c_void, CStr};
use std::sync::atomic::{AtomicU64, Ordering};

// ═══ Superpoder 5: llamar funciones extern ═══
extern "C" {
    fn getenv(name: *const i8) -> *const i8;
}

// ═══ Superpoder 3: static mut (aquí con alternativa atómica) ═══
static CALL_COUNT: AtomicU64 = AtomicU64::new(0);

// ═══ Superpoder 4: unsafe trait ═══
/// # Safety
/// El tipo debe tener un layout fijo y ser safe para transmitir como bytes.
unsafe trait Pod: Copy + Sized {}

unsafe impl Pod for u8 {}
unsafe impl Pod for i32 {}
unsafe impl Pod for f64 {}

// ═══ Superpoder 2: unsafe fn ═══
/// # Safety
/// - `ptr` debe apuntar a al menos `count` valores `T` válidos
/// - Los valores deben estar inicializados
unsafe fn sum_pod_values<T: Pod + Into<f64>>(ptr: *const T, count: usize) -> f64 {
    let mut total: f64 = 0.0;
    for i in 0..count {
        // ═══ Superpoder 1: dereferenciar raw pointer ═══
        total += (*ptr.add(i)).into();
    }
    total
}

fn get_env_or_default(name: &str, default: &str) -> String {
    let c_name = std::ffi::CString::new(name).unwrap();

    // SAFETY: getenv es una función estándar de C,
    // c_name es null-terminated
    let ptr = unsafe { getenv(c_name.as_ptr()) };

    if ptr.is_null() {
        default.to_string()
    } else {
        // SAFETY: getenv retorna un puntero válido a string C
        // si no es null (verificado arriba)
        unsafe { CStr::from_ptr(ptr) }
            .to_string_lossy()
            .into_owned()
    }
}

fn safe_sum(data: &[i32]) -> f64 {
    CALL_COUNT.fetch_add(1, Ordering::Relaxed);

    // SAFETY: data.as_ptr() apunta a data.len() valores i32 válidos
    // (garantizado por el sistema de tipos de Rust para &[i32])
    unsafe { sum_pod_values(data.as_ptr(), data.len()) }
}

fn main() {
    // Usar las funciones safe que encapsulan los 5 superpoderes
    let home = get_env_or_default("HOME", "/tmp");
    println!("HOME = {}", home);

    let data = [10, 20, 30, 40, 50];
    let total = safe_sum(&data);
    println!("sum = {}", total); // sum = 150

    let total2 = safe_sum(&[1, 2, 3]);
    println!("sum2 = {}", total2); // sum2 = 6

    println!("total calls: {}", CALL_COUNT.load(Ordering::Relaxed)); // 2
}
```

```
┌──────────────────────────────────────────────────────────┐
│           Los 5 superpoderes en acción                   │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  1. *ptr.add(i)         → dereferenciar raw pointer     │
│  2. unsafe fn sum_pod   → función con precondiciones     │
│  3. CALL_COUNT          → estado global (atómico aquí)   │
│  4. unsafe trait Pod    → garantía de layout             │
│  5. getenv(...)         → llamar función C extern        │
│                                                          │
│  Cada superpoder está ENCAPSULADO en una API safe:       │
│  - safe_sum() envuelve sum_pod_values()                  │
│  - get_env_or_default() envuelve getenv()                │
│  - AtomicU64 reemplaza static mut                        │
│                                                          │
│  El código en main() es 100% safe                        │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. Declarar firma incorrecta en extern

```rust
extern "C" {
    // INCORRECTO: getpid retorna pid_t (i32), no u64
    // fn getpid() -> u64;

    // CORRECTO:
    fn getpid() -> i32;
}

// El compilador NO detecta este error.
// En runtime: leerás basura en los bytes extra (u64 vs i32)
// o interpretarás el valor incorrectamente.
//
// SOLUCIÓN: usar el crate libc que tiene las firmas correctas,
// o verificar contra las man pages (man 2 getpid).
```

### 2. Olvidar el null terminator en strings para C

```rust
fn main() {
    let name = "hello";

    extern "C" {
        fn puts(s: *const std::ffi::c_char) -> i32;
    }

    unsafe {
        // INCORRECTO: &str de Rust NO tiene null terminator
        // puts(name.as_ptr() as *const _);
        // → lee memoria más allá del string hasta encontrar un 0

        // CORRECTO: usar CString
        let c_name = std::ffi::CString::new(name).unwrap();
        puts(c_name.as_ptr());
    }
}
```

### 3. No verificar el retorno de funciones C

```rust
fn main() {
    unsafe {
        let ptr = libc::malloc(1024);

        // INCORRECTO: usar ptr sin verificar null
        // *(ptr as *mut u8) = 42;

        // CORRECTO: verificar null
        if ptr.is_null() {
            eprintln!("malloc failed");
            return;
        }

        *(ptr as *mut u8) = 42;
        libc::free(ptr);
    }
}
```

### 4. Mezclar allocators (Rust alloc + C free)

```rust
fn main() {
    // INCORRECTO: mezclar allocators
    let s = String::from("hello");
    let ptr = s.as_ptr();
    std::mem::forget(s);
    // unsafe { libc::free(ptr as *mut libc::c_void); }
    // → UB: Rust usó su allocator, C free usa otro

    // CORRECTO: cada allocator libera lo suyo
    // - Rust alloc → Rust dealloc (drop, Box::from_raw, etc.)
    // - C malloc → C free
    // - C custom_alloc → C custom_free

    unsafe {
        let c_ptr = libc::malloc(100);
        if !c_ptr.is_null() {
            // Usar...
            libc::free(c_ptr); // ✓ mismo allocator
        }
    }
}
```

### 5. Asumir que extern "C" fn puede ser un closure

```rust
extern "C" {
    fn qsort(
        base: *mut std::ffi::c_void,
        num: usize,
        size: usize,
        compar: extern "C" fn(*const std::ffi::c_void, *const std::ffi::c_void) -> i32,
    );
}

fn main() {
    let multiplier = 2;

    // INCORRECTO: un closure que captura variables NO es un function pointer
    // let compare = |a: *const _, b: *const _| -> i32 {
    //     multiplier; // captura → no es fn pointer
    //     0
    // };

    // CORRECTO: solo funciones libres (sin captura) pueden ser extern "C" fn
    extern "C" fn compare(
        a: *const std::ffi::c_void,
        b: *const std::ffi::c_void,
    ) -> i32 {
        unsafe {
            let a = *(a as *const i32);
            let b = *(b as *const i32);
            a - b
        }
    }

    // Para pasar contexto → usar el patrón user_data (ver sección anterior)
}
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║        EXTERN FUNCTIONS — REFERENCIA RÁPIDA              ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  DECLARAR FUNCIONES EXTERNAS                             ║
║  ───────────────────────────                              ║
║  extern "C" {                                             ║
║      fn foo(x: i32) -> i32;                              ║
║      fn bar(s: *const c_char, ...);   // variádica       ║
║  }                                                        ║
║  #[link(name = "m")]                                      ║
║  extern "C" { fn sqrt(x: f64) -> f64; }                  ║
║                                                           ║
║  LLAMAR (siempre unsafe)                                  ║
║  ───────────────────────                                  ║
║  let result = unsafe { foo(42) };                        ║
║                                                           ║
║  ABIs                                                     ║
║  ────                                                     ║
║  "C"         estándar FFI (99% de los casos)             ║
║  "C-unwind"  C + permite panic unwinding                 ║
║  "system"    stdcall en Windows, C en otros               ║
║                                                           ║
║  CRATE libc                                               ║
║  ──────────                                               ║
║  libc::getpid()        firmas correctas garantizadas     ║
║  libc::open(...)       todas las funciones POSIX         ║
║  libc::c_int, etc      tipos C correctos                 ║
║                                                           ║
║  CALLBACKS                                                ║
║  ─────────                                                ║
║  extern "C" fn callback(x: i32) { ... }                  ║
║  - Solo funciones libres (sin captura)                    ║
║  - Para contexto: user_data (*mut c_void)                ║
║  - Option<extern "C" fn> = nullable function pointer     ║
║                                                           ║
║  ERRNO                                                    ║
║  ─────                                                    ║
║  std::io::Error::last_os_error()    forma idiomática     ║
║  *libc::__errno_location()          acceso directo       ║
║                                                           ║
║  VARIADIC                                                 ║
║  ────────                                                 ║
║  extern "C" { fn printf(fmt: *const c_char, ...); }      ║
║  float → double, short → int (promoción automática C)    ║
║                                                           ║
║  REGLAS CRÍTICAS                                          ║
║  ───────────────                                          ║
║  □ Verificar firma contra man pages o crate libc         ║
║  □ Usar CString para strings (null terminator)           ║
║  □ Verificar retorno (null, -1, etc.)                    ║
║  □ No mezclar allocators (malloc↔free, Box↔drop)         ║
║  □ Closures NO son function pointers                     ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Wrapper safe sobre getaddrinfo

Crea un wrapper safe sobre la función `getaddrinfo` de libc para resolver nombres DNS:

```rust
use std::ffi::CString;
use std::net::IpAddr;

// libc::getaddrinfo resuelve un hostname a direcciones IP
// Firma (simplificada):
// int getaddrinfo(
//     const char *node,
//     const char *service,
//     const struct addrinfo *hints,
//     struct addrinfo **res
// );
// void freeaddrinfo(struct addrinfo *res);

fn resolve_hostname(hostname: &str) -> std::io::Result<Vec<IpAddr>> {
    todo!()
}
```

**Pistas**:
- Usa `libc::getaddrinfo`, `libc::freeaddrinfo`, `libc::addrinfo`.
- Inicializa `hints` con `mem::zeroed()`, pon `ai_family = AF_UNSPEC` para IPv4+IPv6.
- Itera la lista enlazada: `while !result.is_null() { result = (*result).ai_next; }`.
- Convierte `sockaddr` a `IpAddr`: verifica `ai_family` (AF_INET → `sockaddr_in`, AF_INET6 → `sockaddr_in6`).
- **Siempre** llama `freeaddrinfo` al terminar (usa un guard con `Drop` o asegúrate de llamarlo en todos los paths).

Prueba con:
```rust
let ips = resolve_hostname("localhost")?;
println!("localhost: {:?}", ips);
```

**Pregunta de reflexión**: ¿Por qué es importante llamar `freeaddrinfo` incluso si vas a salir del programa? ¿Qué tipo de recurso libera internamente?

---

### Ejercicio 2: Ejecutar un comando con fork+exec

Usa las funciones de libc `fork`, `execvp`, `waitpid` para ejecutar un comando externo:

```rust
fn run_command(program: &str, args: &[&str]) -> std::io::Result<i32> {
    todo!()
    // 1. fork() → si retorna 0, estás en el hijo
    // 2. Hijo: execvp(program, argv) — nunca retorna si OK
    // 3. Padre: waitpid(pid, &status, 0)
    // 4. Retornar el exit code (WEXITSTATUS macro)
}
```

**Pistas**:
- `args` para `execvp` es un array de `*const c_char` terminado en null.
- Convierte cada `&str` a `CString`, luego extrae `.as_ptr()`.
- El último elemento del array debe ser `std::ptr::null()`.
- `WEXITSTATUS`: `(status >> 8) & 0xFF` en Linux.
- Si `fork()` retorna negativo → error, 0 → hijo, positivo → padre (retorna PID del hijo).

Prueba con:
```rust
let code = run_command("echo", &["hello", "world"])?;
assert_eq!(code, 0);

let code = run_command("ls", &["-la", "/tmp"])?;
println!("ls exited with: {}", code);
```

**Pregunta de reflexión**: ¿Qué pasa si `execvp` falla en el proceso hijo? ¿Cómo comunicarías el error al padre? (Pista: el patrón estándar usa un pipe creado antes de fork.)

---

### Ejercicio 3: Librería de timing con clock_gettime

Crea una librería de medición de tiempo que use `clock_gettime` directamente:

```rust
struct Instant {
    secs: i64,
    nanos: i64,
}

impl Instant {
    /// Captura el tiempo actual usando CLOCK_MONOTONIC
    fn now() -> Self { todo!() }

    /// Calcula la diferencia en nanosegundos
    fn elapsed_nanos(&self) -> u64 { todo!() }

    /// Calcula la diferencia en microsegundos
    fn elapsed_micros(&self) -> u64 { todo!() }

    /// Calcula la diferencia en milisegundos
    fn elapsed_millis(&self) -> u64 { todo!() }

    /// Diferencia entre dos instantes
    fn duration_since(&self, earlier: &Instant) -> u64 { todo!() }
}

/// Mide cuánto tarda un closure en ejecutar (en nanosegundos)
fn bench<F: FnOnce() -> R, R>(f: F) -> (R, u64) { todo!() }
```

**Pistas**:
- `libc::clock_gettime(libc::CLOCK_MONOTONIC, &mut ts)` donde `ts: libc::timespec`.
- `timespec` tiene campos `tv_sec` (segundos) y `tv_nsec` (nanosegundos).
- Para `elapsed`: captura un nuevo `now()` y calcula la diferencia.
- Cuidado con el carry: si `nanos` del resultado es negativo, resta 1 segundo.
- `CLOCK_MONOTONIC` es monótono (nunca retrocede), ideal para medir duraciones.

Prueba con:
```rust
let start = Instant::now();
// Operación costosa
let sum: u64 = (0..1_000_000).sum();
let elapsed = start.elapsed_micros();
println!("sum={}, took {}μs", sum, elapsed);

let (result, nanos) = bench(|| {
    (0..100_000).map(|x| x * x).sum::<u64>()
});
println!("result={}, took {}ns", result, nanos);
```

**Pregunta de reflexión**: ¿Por qué usamos `CLOCK_MONOTONIC` y no `CLOCK_REALTIME`? ¿Qué pasaría con las mediciones si el usuario ajusta la hora del sistema o si NTP corrige el reloj durante la medición?
