# Tipos FFI

## Índice

1. [El problema de los tipos entre lenguajes](#1-el-problema-de-los-tipos-entre-lenguajes)
2. [Tipos numéricos: std::ffi y equivalencias C](#2-tipos-numéricos-stdffi-y-equivalencias-c)
3. [Strings: CStr y CString](#3-strings-cstr-y-cstring)
4. [Punteros: *const T, *mut T y c_void](#4-punteros-const-t-mut-t-y-c_void)
5. [OsStr y OsString: strings del sistema operativo](#5-osstr-y-osstring-strings-del-sistema-operativo)
6. [Enums y constantes](#6-enums-y-constantes)
7. [Structs y uniones](#7-structs-y-uniones)
8. [Option como puntero nullable](#8-option-como-puntero-nullable)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. El problema de los tipos entre lenguajes

C y Rust tienen sistemas de tipos fundamentalmente distintos. C no tiene `String`, `Vec`,
`Option`, ni `Result`. Rust no tiene `char*`, `int` de tamaño variable, ni `NULL`. Para
comunicarse, necesitan un vocabulario común de tipos que ambos entiendan a nivel binario.

```
┌─── Tipos que NO cruzan el boundary FFI ───────────────────┐
│                                                            │
│  Rust exclusivo:                                           │
│  • String, &str        (no null-terminated, tiene len)     │
│  • Vec<T>              (heap + len + capacity)             │
│  • Option<T>           (discriminante + valor)             │
│  • Result<T, E>        (discriminante + variante)          │
│  • Box<T>              (puntero owned, dropeado por Rust)  │
│  • bool de Rust        (1 byte, solo 0 o 1)                │
│                                                            │
│  C exclusivo:                                              │
│  • char*               (puntero a bytes null-terminated)   │
│  • int, long           (tamaño depende de plataforma)      │
│  • NULL                (puntero nulo como valor especial)   │
│  • union               (sin discriminante)                 │
│  • bitfields           (sin equivalente directo en Rust)   │
│                                                            │
└────────────────────────────────────────────────────────────┘

┌─── Tipos que SÍ cruzan el boundary ───────────────────────┐
│                                                            │
│  • Enteros de tamaño fijo: i8/u8, i16/u16, i32/u32, i64   │
│  • Flotantes: f32, f64                                     │
│  • Punteros crudos: *const T, *mut T                       │
│  • Structs con #[repr(C)]                                  │
│  • Tipos de std::ffi: c_int, c_char, CStr, CString...     │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

## 2. Tipos numéricos: std::ffi y equivalencias C

### ¿Por qué no usar i32 directamente?

`int` en C no es siempre 32 bits. Aunque en la práctica lo es en Linux x86_64, el estándar
C solo garantiza que `int` es **al menos** 16 bits. `long` puede ser 32 o 64 bits según la
plataforma. Usar tipos de `std::ffi` garantiza correspondencia correcta en toda plataforma.

### Tabla de equivalencias

| C                | Rust (`std::ffi`)       | Tamaño típico (x86_64 Linux) |
|------------------|-------------------------|------------------------------|
| `char`           | `c_char`                | 1 byte (signed o unsigned)   |
| `signed char`    | `c_schar`               | 1 byte, signed               |
| `unsigned char`  | `c_uchar`               | 1 byte, unsigned             |
| `short`          | `c_short`               | 2 bytes                      |
| `unsigned short` | `c_ushort`              | 2 bytes                      |
| `int`            | `c_int`                 | 4 bytes                      |
| `unsigned int`   | `c_uint`                | 4 bytes                      |
| `long`           | `c_long`                | 8 bytes                      |
| `unsigned long`  | `c_ulong`               | 8 bytes                      |
| `long long`      | `c_longlong`            | 8 bytes                      |
| `unsigned long long` | `c_ulonglong`       | 8 bytes                      |
| `float`          | `c_float` (= `f32`)     | 4 bytes                      |
| `double`         | `c_double` (= `f64`)    | 8 bytes                      |
| `void`           | `c_void`                | —                            |
| `size_t`         | `usize`                 | 8 bytes (64-bit)             |
| `ssize_t`        | `isize`                 | 8 bytes (64-bit)             |

### Uso en declaraciones extern

```rust
use std::ffi::{c_int, c_uint, c_long, c_double, c_char};

extern "C" {
    // int abs(int x);
    fn abs(x: c_int) -> c_int;

    // double sqrt(double x);
    fn sqrt(x: c_double) -> c_double;

    // long strtol(const char *str, char **endptr, int base);
    fn strtol(str: *const c_char, endptr: *mut *mut c_char, base: c_int) -> c_long;

    // unsigned int sleep(unsigned int seconds);
    fn sleep(seconds: c_uint) -> c_uint;
}
```

### ¿Cuándo sí usar tipos fijos?

Si la API C usa tipos de tamaño explícito (`int32_t`, `uint64_t`), usa los equivalentes
directos de Rust:

```rust
// C: #include <stdint.h>
// int32_t, uint32_t, int64_t, uint64_t, etc.

extern "C" {
    // void process(int32_t x, uint64_t y);
    fn process(x: i32, y: u64);  // Tamaño idéntico garantizado
}
```

Los tipos `int32_t` etc. de C tienen tamaño fijo por definición, así que `i32` y `u32`
de Rust son equivalentes directos. Usa `c_int` solo para `int`, `c_long` para `long`, etc.

### El caso especial de c_char

`c_char` puede ser `i8` o `u8` dependiendo de la plataforma:

```
┌─── c_char según plataforma ───────────────────────────────┐
│                                                            │
│  Linux x86_64:    c_char = i8  (signed char)               │
│  Linux ARM:       c_char = u8  (unsigned char)             │
│  macOS:           c_char = i8                              │
│                                                            │
│  Por eso std::ffi::c_char existe como tipo alias:          │
│  se adapta automáticamente a la plataforma target          │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

## 3. Strings: CStr y CString

### El problema fundamental

```
┌─── String en Rust ────────────────────────────────────────┐
│                                                            │
│  String / &str:                                            │
│  ┌─────┬─────────────────────────┐                         │
│  │ ptr │ len                     │  ← fat pointer          │
│  └──┬──┴─────────────────────────┘                         │
│     │                                                      │
│     ▼                                                      │
│  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┐                 │
│  │H │e │l │l │o │, │  │w │o │r │l │d │! │  ← UTF-8       │
│  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┘                 │
│  Sin \0 al final. Longitud conocida por len.               │
│                                                            │
└────────────────────────────────────────────────────────────┘

┌─── String en C ───────────────────────────────────────────┐
│                                                            │
│  char*:                                                    │
│  ┌─────┐                                                   │
│  │ ptr │  ← raw pointer                                    │
│  └──┬──┘                                                   │
│     │                                                      │
│     ▼                                                      │
│  ┌──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬──┬────┐            │
│  │H │e │l │l │o │, │  │w │o │r │l │d │! │ \0 │            │
│  └──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴──┴────┘            │
│  Longitud determinada buscando \0. No necesariamente UTF-8.│
│                                                            │
└────────────────────────────────────────────────────────────┘
```

Dos diferencias críticas:
1. **Null terminator**: C requiere `\0` al final; Rust no.
2. **Encoding**: Rust garantiza UTF-8; C usa bytes arbitrarios.

### CString: Rust → C (owned)

`CString` es la versión **owned** de un C string. Es el equivalente de `String` pero con
null terminator y sin garantía de UTF-8.

```rust
use std::ffi::{CString, c_char, c_int};

extern "C" {
    fn puts(s: *const c_char) -> c_int;
    fn strlen(s: *const c_char) -> usize;
}

fn main() {
    // Crear CString desde &str
    let msg = CString::new("Hello from Rust!").unwrap();

    unsafe {
        puts(msg.as_ptr());  // as_ptr() → *const c_char

        let len = strlen(msg.as_ptr());
        println!("C strlen: {}", len);  // 16
    }

    // CString se dropea aquí → libera la memoria
}
```

### ¿Por qué CString::new puede fallar?

```rust
use std::ffi::CString;

// CString::new falla si el string contiene un byte nulo interior
let ok = CString::new("hello");           // Ok
let err = CString::new("hel\0lo");        // Err(NulError)

// El \0 interior confundiría a C: pensaría que el string termina en "hel"
match CString::new("hel\0lo") {
    Ok(_) => unreachable!(),
    Err(e) => {
        println!("Null byte at position: {}", e.nul_position());  // 3
    }
}

// Si estás seguro de que no hay null bytes:
let safe = CString::new("definitely no nulls").expect("unexpected null byte");
```

### CStr: C → Rust (borrowed)

`CStr` es la versión **borrowed** de un C string. Es el equivalente de `&str` para strings
de C. No posee la memoria — solo la referencia.

```rust
use std::ffi::{CStr, c_char};

extern "C" {
    fn getenv(name: *const c_char) -> *const c_char;
}

fn get_env_var(name: &str) -> Option<String> {
    let c_name = std::ffi::CString::new(name).ok()?;

    unsafe {
        let ptr = getenv(c_name.as_ptr());

        if ptr.is_null() {
            return None;
        }

        // CStr::from_ptr: construye &CStr desde un *const c_char
        // Busca el \0 para determinar la longitud
        let c_str = CStr::from_ptr(ptr);

        // to_str(): convierte a &str (falla si no es UTF-8 válido)
        // to_string_lossy(): convierte a Cow<str> (reemplaza bytes inválidos con �)
        Some(c_str.to_string_lossy().into_owned())
    }
}

fn main() {
    match get_env_var("HOME") {
        Some(home) => println!("HOME = {}", home),
        None => println!("HOME not set"),
    }
}
```

### Diagrama de conversiones

```
┌─── Conversiones entre tipos de string ────────────────────────────────┐
│                                                                        │
│  Rust owned         Rust borrowed       C pointer                      │
│  ──────────         ─────────────       ─────────                      │
│                                                                        │
│  String ──────────→ &str                                               │
│    │  .as_str()                                                        │
│    │                                                                   │
│    │  CString::new(string)?                                            │
│    ▼                                                                   │
│  CString ─────────→ &CStr ────────────→ *const c_char                 │
│    │  .as_c_str()     │  .as_ptr()                                     │
│    │                  │                                                 │
│    │  .to_str()?      │  .to_str()?                                    │
│    │  .to_string_lossy()  .to_string_lossy()                           │
│    ▼                  ▼                                                │
│  String ←──────────  &str                                              │
│                                                                        │
│                       CStr::from_ptr(ptr) ←──── *const c_char          │
│                       (unsafe: busca \0)                               │
│                                                                        │
│  Resumen:                                                              │
│  Rust → C:  CString::new(s)?.as_ptr()                                  │
│  C → Rust:  CStr::from_ptr(ptr).to_string_lossy()                     │
│                                                                        │
└────────────────────────────────────────────────────────────────────────┘
```

### CStr::to_str vs to_string_lossy

```rust
use std::ffi::CStr;

fn demonstrate_conversion(c_str: &CStr) {
    // to_str(): retorna Result<&str, Utf8Error>
    // Falla si los bytes no son UTF-8 válido
    match c_str.to_str() {
        Ok(s) => println!("Valid UTF-8: {}", s),
        Err(e) => println!("Invalid UTF-8: {}", e),
    }

    // to_string_lossy(): siempre tiene éxito
    // Reemplaza bytes inválidos con U+FFFD (�)
    // Retorna Cow<str>: &str si era UTF-8, String si tuvo que reemplazar
    let s = c_str.to_string_lossy();
    println!("Lossy: {}", s);

    // to_bytes(): acceso a los bytes crudos (sin el \0)
    let bytes = c_str.to_bytes();
    println!("Raw bytes: {:?}", bytes);

    // to_bytes_with_nul(): bytes incluyendo el \0
    let bytes_nul = c_str.to_bytes_with_nul();
    println!("Bytes with nul: {:?}", bytes_nul);
}
```

### CString: métodos importantes

```rust
use std::ffi::CString;

fn cstring_methods() {
    let cs = CString::new("hello").unwrap();

    // as_ptr(): *const c_char — para pasar a funciones C
    let ptr = cs.as_ptr();

    // as_c_str(): &CStr — versión borrowed
    let borrowed: &std::ffi::CStr = cs.as_c_str();

    // into_raw(): consume el CString y retorna *mut c_char
    // TÚ eres responsable de liberar la memoria después
    let raw = cs.into_raw();

    // Reconstruir desde raw (para liberarla)
    unsafe {
        let cs_back = CString::from_raw(raw);
        // cs_back se dropea aquí → memoria liberada
    }

    // into_bytes(): consume y retorna Vec<u8> (sin \0)
    let cs2 = CString::new("world").unwrap();
    let bytes: Vec<u8> = cs2.into_bytes();

    // into_bytes_with_nul(): consume y retorna Vec<u8> (con \0)
    let cs3 = CString::new("test").unwrap();
    let bytes_nul: Vec<u8> = cs3.into_bytes_with_nul();
}
```

---

## 4. Punteros: *const T, *mut T y c_void

### Punteros crudos en FFI

Los punteros crudos de Rust (`*const T`, `*mut T`) son el equivalente directo de los
punteros de C. No tienen las garantías de las referencias de Rust (lifetime, no-null,
no-aliasing):

```rust
use std::ffi::{c_int, c_void, c_char};

extern "C" {
    // const int *data → *const c_int
    fn process_data(data: *const c_int, len: c_int);

    // int *output → *mut c_int
    fn fill_buffer(output: *mut c_int, capacity: c_int) -> c_int;

    // void *ptr → *mut c_void (puntero genérico)
    fn malloc(size: usize) -> *mut c_void;
    fn free(ptr: *mut c_void);

    // const char * → *const c_char
    fn puts(s: *const c_char) -> c_int;
}
```

### Pasar arrays a C

```rust
use std::ffi::c_int;

extern "C" {
    // void sort(int *array, int count);
    fn qsort(
        base: *mut c_void,
        nmemb: usize,
        size: usize,
        compar: extern "C" fn(*const c_void, *const c_void) -> c_int,
    );
}

fn main() {
    let mut numbers: Vec<c_int> = vec![5, 3, 8, 1, 9, 2, 7];

    // Pasar slice/Vec como puntero + longitud
    extern "C" fn compare(a: *const c_void, b: *const c_void) -> c_int {
        unsafe {
            let a = *(a as *const c_int);
            let b = *(b as *const c_int);
            a - b
        }
    }

    unsafe {
        qsort(
            numbers.as_mut_ptr() as *mut c_void,
            numbers.len(),
            std::mem::size_of::<c_int>(),
            compare,
        );
    }

    println!("Sorted: {:?}", numbers);  // [1, 2, 3, 5, 7, 8, 9]
}
```

### c_void: el puntero genérico

`void*` en C es un puntero a "cualquier cosa" — el equivalente de un puntero sin tipo.
En Rust es `*mut c_void` o `*const c_void`:

```rust
use std::ffi::c_void;

extern "C" {
    fn malloc(size: usize) -> *mut c_void;
    fn free(ptr: *mut c_void);
    fn memcpy(dest: *mut c_void, src: *const c_void, n: usize) -> *mut c_void;
}

fn main() {
    unsafe {
        // Asignar 100 bytes
        let ptr = malloc(100);
        if ptr.is_null() {
            panic!("malloc failed");
        }

        // Castear a tipo concreto
        let int_ptr = ptr as *mut i32;
        *int_ptr = 42;

        // Copiar memoria
        let mut dest = [0i32; 1];
        memcpy(
            dest.as_mut_ptr() as *mut c_void,
            ptr as *const c_void,
            std::mem::size_of::<i32>(),
        );
        println!("Copied: {}", dest[0]);  // 42

        // Liberar
        free(ptr);
    }
}
```

### Verificar punteros nulos

```rust
use std::ffi::{CStr, c_char};

extern "C" {
    fn getenv(name: *const c_char) -> *const c_char;
}

fn safe_getenv(name: &str) -> Option<String> {
    let c_name = std::ffi::CString::new(name).ok()?;

    unsafe {
        let ptr = getenv(c_name.as_ptr());

        // SIEMPRE verificar null antes de desreferenciar
        if ptr.is_null() {
            return None;
        }

        // Ahora es seguro construir CStr
        let c_str = CStr::from_ptr(ptr);
        Some(c_str.to_string_lossy().into_owned())
    }
}
```

### Punteros a punteros

Algunas funciones C usan `char**` o `int**` para devolver valores o para arrays de strings:

```rust
use std::ffi::{CStr, c_char, c_int};

extern "C" {
    // int strtol(const char *str, char **endptr, int base);
    fn strtol(str: *const c_char, endptr: *mut *mut c_char, base: c_int) -> std::ffi::c_long;
}

fn parse_number(s: &str) -> Option<i64> {
    let c_str = std::ffi::CString::new(s).ok()?;
    let mut endptr: *mut c_char = std::ptr::null_mut();

    unsafe {
        let result = strtol(c_str.as_ptr(), &mut endptr, 10);

        // endptr apunta al primer carácter no parseado
        if endptr == c_str.as_ptr() as *mut c_char {
            None  // No se pudo parsear nada
        } else {
            Some(result as i64)
        }
    }
}

fn main() {
    println!("{:?}", parse_number("42abc"));   // Some(42)
    println!("{:?}", parse_number("abc"));     // None
    println!("{:?}", parse_number(""));        // None
}
```

---

## 5. OsStr y OsString: strings del sistema operativo

### ¿Por qué existen?

Los nombres de archivo en Unix pueden contener **cualquier byte excepto `\0` y `/`**. No
necesitan ser UTF-8 válido. Rust tiene `OsStr`/`OsString` para representar estas cadenas
del sistema operativo:

```
┌─── Tipos de string y sus dominios ────────────────────────┐
│                                                            │
│  &str / String:                                            │
│  • Siempre UTF-8 válido                                    │
│  • Para texto legible                                      │
│                                                            │
│  &CStr / CString:                                          │
│  • Null-terminated, bytes arbitrarios                      │
│  • Para pasar strings a funciones C                        │
│                                                            │
│  &OsStr / OsString:                                        │
│  • Bytes del SO (en Unix: cualquier byte excepto \0)       │
│  • Para paths y argumentos del sistema operativo           │
│  • En Unix: extensión de UTF-8 (WTF-8 en Windows)         │
│                                                            │
│  &Path / PathBuf:                                          │
│  • Wrapper newtype sobre OsStr/OsString                    │
│  • Agrega métodos de path (join, extension, etc.)          │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Conversiones entre tipos de string

```rust
use std::ffi::{CStr, CString, OsStr, OsString};
use std::os::unix::ffi::{OsStrExt, OsStringExt};
use std::path::Path;

fn string_conversions() {
    // --- &str / String → otros ---
    let s = "hello.txt";

    let os: &OsStr = OsStr::new(s);                      // &str → &OsStr
    let path: &Path = Path::new(s);                       // &str → &Path
    let cs = CString::new(s).unwrap();                    // &str → CString

    // --- &OsStr → otros ---
    let os_str: &OsStr = OsStr::new("hello.txt");

    let opt_str: Option<&str> = os_str.to_str();          // puede fallar
    let lossy: std::borrow::Cow<str> = os_str.to_string_lossy();
    let bytes: &[u8] = os_str.as_bytes();                 // Unix only
    let path: &Path = Path::new(os_str);                  // &OsStr → &Path

    // --- &OsStr → CString (para pasar a C) ---
    // OsStr puede contener \0, así que CString::new puede fallar
    let c_str = CString::new(os_str.as_bytes());          // Unix only

    // --- bytes → OsStr (datos del SO) ---
    let raw_bytes = b"/tmp/\xff\xfefile";                 // No es UTF-8 válido
    let os_str = OsStr::from_bytes(raw_bytes);            // Unix only
    println!("Path: {}", os_str.to_string_lossy());       // /tmp/��file
}
```

### Ejemplo: listar archivos con bytes no-UTF-8

```rust
use std::ffi::{CStr, CString, c_char};
use std::os::unix::ffi::OsStrExt;

extern "C" {
    fn opendir(name: *const c_char) -> *mut libc::DIR;
    fn readdir(dirp: *mut libc::DIR) -> *mut libc::dirent;
    fn closedir(dirp: *mut libc::DIR) -> std::ffi::c_int;
}

fn list_dir(path: &std::path::Path) -> std::io::Result<Vec<std::ffi::OsString>> {
    let c_path = CString::new(path.as_os_str().as_bytes())
        .map_err(|_| std::io::Error::new(std::io::ErrorKind::InvalidInput, "null in path"))?;

    unsafe {
        let dir = opendir(c_path.as_ptr());
        if dir.is_null() {
            return Err(std::io::Error::last_os_error());
        }

        let mut entries = Vec::new();

        loop {
            // Reset errno before calling readdir
            *libc::__errno_location() = 0;

            let entry = readdir(dir);
            if entry.is_null() {
                break;
            }

            let name = CStr::from_ptr((*entry).d_name.as_ptr());
            let os_name = std::ffi::OsStr::from_bytes(name.to_bytes());

            // Skip . and ..
            if os_name != "." && os_name != ".." {
                entries.push(os_name.to_owned());
            }
        }

        closedir(dir);
        Ok(entries)
    }
}
```

---

## 6. Enums y constantes

### Constantes de C como const en Rust

Las `#define` y `enum` de C se traducen a constantes en Rust:

```c
// En C:
#define MAX_PATH 4096
#define O_RDONLY 0
#define O_WRONLY 1
#define O_RDWR   2

enum color { RED = 0, GREEN = 1, BLUE = 2 };
```

```rust
// En Rust: como constantes
const MAX_PATH: usize = 4096;
const O_RDONLY: std::ffi::c_int = 0;
const O_WRONLY: std::ffi::c_int = 1;
const O_RDWR: std::ffi::c_int = 2;

// O como enum repr(C) si los valores son mutuamente excluyentes
#[repr(C)]
enum Color {
    Red = 0,
    Green = 1,
    Blue = 2,
}
```

### repr(C) vs repr(i32) para enums FFI

```rust
// repr(C): usa el tamaño que C usaría (normalmente int = 4 bytes)
#[repr(C)]
enum LogLevel {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
}

// repr(i32): fuerza exactamente 4 bytes (más portátil para FFI)
#[repr(i32)]
enum ErrorCode {
    Success = 0,
    NotFound = -1,
    Permission = -2,
    IoError = -3,
}
```

### Flags como bitfields

Las combinaciones de flags en C (`O_RDWR | O_CREAT | O_TRUNC`) no son enums — son
constantes que se combinan con OR. En Rust se manejan como enteros:

```rust
use std::ffi::c_int;

// Flags como constantes (no como enum — se combinan con |)
const O_RDONLY: c_int = 0;
const O_WRONLY: c_int = 1;
const O_RDWR: c_int = 2;
const O_CREAT: c_int = 0o100;
const O_TRUNC: c_int = 0o1000;
const O_APPEND: c_int = 0o2000;

extern "C" {
    fn open(path: *const std::ffi::c_char, flags: c_int, ...) -> c_int;
}

fn main() {
    let path = std::ffi::CString::new("/tmp/test.txt").unwrap();

    unsafe {
        // Combinar flags con OR, exactamente como en C
        let fd = open(
            path.as_ptr(),
            O_WRONLY | O_CREAT | O_TRUNC,
            0o644 as c_int,  // mode_t para el archivo
        );

        if fd == -1 {
            eprintln!("open failed: {}", std::io::Error::last_os_error());
        } else {
            libc::close(fd);
        }
    }
}
```

### El crate bitflags

Para flags más ergonómicos y seguros, usa `bitflags`:

```rust
// Cargo.toml: bitflags = "2"

use bitflags::bitflags;

bitflags! {
    #[derive(Debug, Clone, Copy)]
    struct OpenFlags: i32 {
        const RDONLY = 0;
        const WRONLY = 1;
        const RDWR   = 2;
        const CREAT  = 0o100;
        const TRUNC  = 0o1000;
        const APPEND = 0o2000;
    }
}

fn open_file(path: &str, flags: OpenFlags) -> std::io::Result<i32> {
    let c_path = std::ffi::CString::new(path).unwrap();
    let fd = unsafe {
        libc::open(c_path.as_ptr(), flags.bits(), 0o644)
    };
    if fd == -1 {
        Err(std::io::Error::last_os_error())
    } else {
        Ok(fd)
    }
}

fn main() {
    let flags = OpenFlags::WRONLY | OpenFlags::CREAT | OpenFlags::TRUNC;
    println!("Flags: {:?}", flags);  // WRONLY | CREAT | TRUNC

    match open_file("/tmp/test.txt", flags) {
        Ok(fd) => {
            println!("Opened fd={}", fd);
            unsafe { libc::close(fd); }
        }
        Err(e) => eprintln!("Error: {}", e),
    }
}
```

---

## 7. Structs y uniones

### Structs repr(C)

```rust
use std::ffi::{c_int, c_char, c_long};

// C struct:
// struct passwd {
//     char *pw_name;
//     char *pw_passwd;
//     uid_t pw_uid;
//     gid_t pw_gid;
//     char *pw_gecos;
//     char *pw_dir;
//     char *pw_shell;
// };

#[repr(C)]
struct Passwd {
    pw_name: *mut c_char,
    pw_passwd: *mut c_char,
    pw_uid: u32,    // uid_t en Linux x86_64
    pw_gid: u32,    // gid_t en Linux x86_64
    pw_gecos: *mut c_char,
    pw_dir: *mut c_char,
    pw_shell: *mut c_char,
}

extern "C" {
    fn getpwuid(uid: u32) -> *mut Passwd;
}

fn get_username(uid: u32) -> Option<String> {
    unsafe {
        let pw = getpwuid(uid);
        if pw.is_null() {
            return None;
        }

        let name = std::ffi::CStr::from_ptr((*pw).pw_name);
        Some(name.to_string_lossy().into_owned())
    }
}

fn main() {
    let uid = unsafe { libc::getuid() };
    match get_username(uid) {
        Some(name) => println!("User {}: {}", uid, name),
        None => println!("Unknown user {}", uid),
    }
}
```

### Structs opacos

Cuando C usa un struct opaco (solo visible como puntero, sin campos públicos):

```c
// C header:
typedef struct Database Database;
Database *db_open(const char *path);
void db_close(Database *db);
int db_get(Database *db, const char *key, char *value, int value_size);
```

```rust
use std::ffi::{c_char, c_int};

// Tipo opaco: enum vacío con repr(C)
// No puedes crear instancias, solo usar punteros
#[repr(C)]
pub struct Database {
    _private: [u8; 0],   // Zero-sized, no instanciable
}

extern "C" {
    fn db_open(path: *const c_char) -> *mut Database;
    fn db_close(db: *mut Database);
    fn db_get(
        db: *mut Database,
        key: *const c_char,
        value: *mut c_char,
        value_size: c_int,
    ) -> c_int;
}
```

### Unions repr(C)

Las uniones de C se representan con `union` en Rust:

```rust
use std::ffi::c_int;

// C union:
// union Value {
//     int i;
//     float f;
//     char s[32];
// };

#[repr(C)]
union Value {
    i: c_int,
    f: f32,
    s: [std::ffi::c_char; 32],
}

fn main() {
    let mut v = Value { i: 42 };

    // Acceder a un campo de union es unsafe:
    // el compilador no sabe qué campo está "activo"
    unsafe {
        println!("As int: {}", v.i);          // 42
        println!("As float: {}", v.f);        // Basura (mismos bits)

        v.f = 3.14;
        println!("As float: {}", v.f);        // 3.14
        println!("As int: {}", v.i);          // 1078523331 (bits de 3.14)
    }
}
```

### mem::zeroed para inicializar structs

Cuando C espera que inicialices un struct antes de pasarlo (como `memset(&s, 0, sizeof(s))`):

```rust
use std::mem;

#[repr(C)]
struct Stats {
    count: u64,
    sum: f64,
    min: f64,
    max: f64,
}

extern "C" {
    fn compute_stats(data: *const f64, len: usize, out: *mut Stats) -> std::ffi::c_int;
}

fn get_stats(data: &[f64]) -> Option<Stats> {
    // mem::zeroed: llena todos los bytes con 0 (como memset en C)
    let mut stats: Stats = unsafe { mem::zeroed() };

    unsafe {
        let result = compute_stats(data.as_ptr(), data.len(), &mut stats);
        if result == 0 {
            Some(stats)
        } else {
            None
        }
    }
}
```

> **Cuidado**: `mem::zeroed()` es undefined behavior si el tipo no es válido con todos
> los bytes a cero. Es seguro para `c_int`, `f64`, punteros (null es 0), pero NO para
> `bool` (0 es false, OK) ni para enums sin variante 0, ni para referencias.

---

## 8. Option como puntero nullable

Rust tiene una optimización especial: `Option<&T>`, `Option<Box<T>>`, y
`Option<NonNull<T>>` tienen el mismo layout que un puntero C nullable. `None` es `NULL`.

```rust
use std::ptr::NonNull;
use std::ffi::{c_char, c_int};

extern "C" {
    // char *getenv(const char *name);
    // Retorna NULL si la variable no existe
    fn getenv(name: *const c_char) -> *const c_char;

    // Equivalente semántico usando Option:
    // fn getenv(name: *const c_char) -> Option<NonNull<c_char>>;
}

// Función que aprovecha esta equivalencia
extern "C" {
    // FILE *fopen(const char *path, const char *mode);
    // Retorna NULL on error
    fn fopen(path: *const c_char, mode: *const c_char) -> Option<NonNull<libc::FILE>>;
    fn fclose(stream: NonNull<libc::FILE>) -> c_int;
}

fn open_file(path: &str) -> Option<NonNull<libc::FILE>> {
    let c_path = std::ffi::CString::new(path).ok()?;
    let c_mode = std::ffi::CString::new("r").unwrap();

    // fopen retorna Option<NonNull<FILE>>
    // Si es NULL, Option es None automáticamente
    unsafe { fopen(c_path.as_ptr(), c_mode.as_ptr()) }
}
```

```
┌─── Layout de Option<NonNull<T>> ──────────────────────────┐
│                                                            │
│  Option<NonNull<T>>   en memoria:  [8 bytes del puntero]   │
│  None                 en memoria:  [0x0000000000000000]    │
│  Some(ptr)            en memoria:  [0x00007fff12345678]    │
│                                                            │
│  *mut T (C pointer)   en memoria:  [8 bytes del puntero]   │
│  NULL                 en memoria:  [0x0000000000000000]    │
│  valid                en memoria:  [0x00007fff12345678]    │
│                                                            │
│  ¡Mismo layout! El compilador optimiza el discriminante    │
│  de Option usando el valor 0 como None.                    │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

Esta optimización se llama **nullable pointer optimization** (NPO) y permite escribir APIs
FFI más idiomáticas sin costo de representación.

### Callback nullable

```rust
use std::ffi::c_int;

// C: void register_callback(void (*cb)(int));
// cb puede ser NULL (sin callback)
extern "C" {
    fn register_callback(cb: Option<extern "C" fn(c_int)>);
}

extern "C" fn my_callback(value: c_int) {
    println!("Callback: {}", value);
}

fn main() {
    unsafe {
        register_callback(Some(my_callback));  // Registrar callback
        register_callback(None);               // Desregistrar (NULL)
    }
}
```

---

## 9. Errores comunes

### Error 1: confundir CStr y CString

```rust
use std::ffi::{CStr, CString, c_char};

extern "C" {
    fn puts(s: *const c_char) -> std::ffi::c_int;
}

// MAL: intentar crear CStr directamente desde &str
fn bad() {
    // CStr no tiene constructor desde &str
    // CStr es para wrappear punteros que ya tienen \0
    // let cs = CStr::new("hello");  // No existe este método
}

// BIEN: CString para crear, CStr para recibir
fn good() {
    // Crear un C string desde Rust: CString
    let owned = CString::new("hello").unwrap();
    unsafe { puts(owned.as_ptr()); }

    // Wrappear un puntero de C: CStr
    unsafe {
        let ptr: *const c_char = owned.as_ptr();
        let borrowed = CStr::from_ptr(ptr);
        println!("{}", borrowed.to_string_lossy());
    }
}
```

**Regla**: `CString` es para **crear** (Rust → C). `CStr` es para **recibir** (C → Rust).
La relación es como `String` / `&str`.

### Error 2: intentar crear CString con null interior

```rust
use std::ffi::CString;

// MAL: no manejar el error de NulError
fn bad(data: &[u8]) {
    let cs = CString::new(data).unwrap();  // Panic si data contiene \0
}

// BIEN: manejar el caso o garantizar que no hay nulls
fn good(data: &[u8]) -> Option<std::ffi::CString> {
    CString::new(data).ok()
}

// ALTERNATIVA: si necesitas bytes con nulls, no uses CString
// Pasa puntero + longitud directamente
fn pass_raw_data(data: &[u8]) {
    // data.as_ptr() + data.len(), sin null terminator
}
```

**Por qué falla**: un C string termina en el primer `\0`. Si pones `\0` en el medio,
C lo interpretaría como el final del string. `CString::new` previene esto con un error.

### Error 3: usar size_of mal en FFI

```rust
use std::ffi::c_int;

extern "C" {
    fn read(fd: c_int, buf: *mut std::ffi::c_void, count: usize) -> isize;
}

// MAL: confundir tamaño de Rust con tamaño de C
fn bad() {
    let mut buf = [0i32; 10];
    unsafe {
        // Quiere leer 10 enteros, pero pasa 10 bytes
        read(0, buf.as_mut_ptr() as *mut _, 10);
    }
}

// BIEN: calcular bytes correctamente
fn good() {
    let mut buf = [0i32; 10];
    let byte_count = buf.len() * std::mem::size_of::<i32>();
    unsafe {
        read(0, buf.as_mut_ptr() as *mut _, byte_count);
    }
}
```

**Por qué falla**: las funciones C como `read`, `memcpy`, `malloc` trabajan en **bytes**,
no en elementos. Olvidar multiplicar por `size_of::<T>()` resulta en leer menos datos de
los esperados o en buffer overflows.

### Error 4: asumir que c_char es i8

```rust
use std::ffi::c_char;

// MAL: comparar c_char como si fuera siempre signed
fn bad_check(c: c_char) -> bool {
    c > 0  // En ARM, c_char es unsigned → siempre >= 0
}

// BIEN: castear explícitamente al tipo que necesitas
fn good_check(c: c_char) -> bool {
    (c as u8).is_ascii_alphabetic()
}

// O usar los métodos del tipo que esperas
fn is_printable(c: c_char) -> bool {
    let byte = c as u8;
    byte >= 0x20 && byte <= 0x7E
}
```

**Por qué falla**: `c_char` es `i8` en x86 pero `u8` en ARM. Código que asume signedness
compila en una plataforma y tiene bugs silenciosos en otra.

### Error 5: retornar referencias a datos temporales a través de FFI

```rust
use std::ffi::{CString, c_char};

// MAL: retorna puntero a CString que se libera inmediatamente
extern "C" fn bad_get_name() -> *const c_char {
    let name = CString::new("Alice").unwrap();
    name.as_ptr()
    // name se dropea aquí → puntero colgante
}

// BIEN: usar into_raw (transfiere ownership al caller C)
extern "C" fn good_get_name() -> *mut c_char {
    let name = CString::new("Alice").unwrap();
    name.into_raw()  // Caller debe llamar a free_name() después
}

// Función para liberar (el caller C debe llamar esta)
extern "C" fn free_name(ptr: *mut c_char) {
    if !ptr.is_null() {
        unsafe { drop(CString::from_raw(ptr)); }
    }
}
```

**Por qué falla**: `as_ptr()` retorna un puntero al contenido del `CString`, pero el
`CString` se destruye al salir del scope. `into_raw()` consume el `CString` sin destruirlo
— el caller ahora es dueño de esa memoria y debe liberarla.

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════════╗
║                          TIPOS FFI                                     ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                        ║
║  NUMÉRICOS (std::ffi)                                                  ║
║  ────────────────────                                                  ║
║  C int/uint        → c_int / c_uint                                    ║
║  C long/ulong      → c_long / c_ulong                                 ║
║  C char             → c_char (i8 o u8 según plataforma)               ║
║  C float/double     → c_float / c_double (= f32/f64)                  ║
║  C void             → c_void                                          ║
║  C size_t/ssize_t   → usize / isize                                   ║
║  C int32_t/uint64_t → i32 / u64 (tamaño fijo OK)                      ║
║                                                                        ║
║  STRINGS                                                               ║
║  ───────                                                               ║
║  Rust → C:  CString::new(s)?.as_ptr()     → *const c_char             ║
║  C → Rust:  CStr::from_ptr(ptr)           → &CStr (unsafe)            ║
║             .to_str()?                     → &str (puede fallar)       ║
║             .to_string_lossy()             → Cow<str> (siempre OK)     ║
║  Transfer:  cstring.into_raw()             → *mut c_char (no free)     ║
║             CString::from_raw(ptr)         → CString (retoma owner)    ║
║                                                                        ║
║  PUNTEROS                                                              ║
║  ────────                                                              ║
║  const T*  → *const T           T*        → *mut T                     ║
║  void*     → *mut c_void        NULL      → std::ptr::null()           ║
║  ptr.is_null()                  verificar SIEMPRE antes de usar        ║
║  Option<NonNull<T>> == *mut T   (same layout, NPO)                     ║
║  Option<extern "C" fn()>        callback nullable                      ║
║                                                                        ║
║  OS STRINGS                                                            ║
║  ──────────                                                            ║
║  &OsStr / OsString             paths y args del SO                     ║
║  OsStr::new(s)                  &str → &OsStr                          ║
║  os_str.as_bytes()              &OsStr → &[u8] (Unix only)             ║
║  OsStr::from_bytes(b)           &[u8] → &OsStr (Unix only)            ║
║                                                                        ║
║  STRUCTS Y ENUMS                                                       ║
║  ───────────────                                                       ║
║  #[repr(C)] struct              layout de C obligatorio                ║
║  #[repr(C)] enum                tamaño de int                          ║
║  #[repr(i32)] enum              tamaño explícito                       ║
║  #[repr(C)] union               union de C                             ║
║  mem::zeroed()                  inicializar como memset(0)             ║
║  Struct opaco: struct X { _p: [u8; 0] }  solo como puntero            ║
║                                                                        ║
║  FLAGS                                                                 ║
║  ─────                                                                 ║
║  const FLAG: c_int = N;         constantes combinables con |           ║
║  bitflags! { struct F: i32 { const X = N; } }  tipo seguro            ║
║                                                                        ║
║  REGLAS                                                                ║
║  ──────                                                                ║
║  1. CString para crear, CStr para recibir                              ║
║  2. Mantener CString vivo mientras se usa as_ptr()                     ║
║  3. Verificar null antes de CStr::from_ptr                             ║
║  4. Usar std::ffi types, no i32/u64 para int/long                      ║
║  5. #[repr(C)] en TODO struct/enum que cruce FFI                       ║
║  6. mem::size_of::<T>() * count para bytes                             ║
║                                                                        ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: wrapper seguro para getenv y setenv

Escribe dos funciones seguras (sin `unsafe` en su firma pública) que encapsulen las
funciones C `getenv` y `setenv`:

1. `fn get_env(name: &str) -> Option<String>` — retorna la variable de entorno o `None`.
2. `fn set_env(name: &str, value: &str) -> Result<(), std::io::Error>` — establece una
   variable de entorno.

Usa `CString` para pasar los strings y `CStr` para recibir el resultado de `getenv`.
Maneja el caso de null en `getenv` (retorna `None`) y verifica el retorno de `setenv`
(0 = éxito, -1 = error).

**Pista**: las firmas C son `char *getenv(const char *name)` y
`int setenv(const char *name, const char *value, int overwrite)`.

> **Pregunta de reflexión**: `getenv` retorna un puntero a memoria interna de libc. ¿Es
> seguro guardar ese `*const c_char` para usarlo después? ¿Qué podría invalidarlo?
> ¿Por qué inmediatamente copiamos el contenido a un `String` owned?

---

### Ejercicio 2: struct repr(C) con función C

Crea un ejercicio que pase structs entre Rust y C:

1. Define un struct `#[repr(C)] Point { x: f64, y: f64 }`.
2. Escribe un archivo C (`c_src/geometry.c`) con la función
   `double distance(const Point *a, const Point *b)` que calcule la distancia euclidiana.
3. Usa `build.rs` con `cc` para compilar el C.
4. En Rust, declara la función con `extern "C"`, crea dos `Point`, y llama a `distance`
   pasando punteros.
5. Verifica que el resultado coincida con el cálculo en Rust puro
   (`((a.x-b.x).powi(2) + (a.y-b.y).powi(2)).sqrt()`).

**Pista**: en C, declara `typedef struct { double x; double y; } Point;` — debe coincidir
exactamente con el layout del struct Rust.

> **Pregunta de reflexión**: si olvidas `#[repr(C)]` en el struct `Point`, ¿el programa
> compilaría? ¿Se ejecutaría? ¿Qué clase de error obtendrías — crash, resultado incorrecto,
> o nada visible? ¿Por qué eso lo hace tan peligroso?

---

### Ejercicio 3: conversiones de strings completas

Escribe un programa que demuestre todas las conversiones de strings FFI:

1. Crea un `String` de Rust con el contenido `"Héllo wörld"` (con caracteres UTF-8
   multi-byte).
2. Conviértelo a `CString` y muestra su longitud en bytes (`.to_bytes().len()`).
3. Obtén el `*const c_char` con `.as_ptr()` y constrúyelo de vuelta con `CStr::from_ptr`.
4. Convierte el `CStr` a `&str` con `.to_str()` — debería funcionar porque es UTF-8 válido.
5. Ahora simula datos no-UTF-8: crea un `CStr` desde bytes `b"hello\xff\xfeworld\0"` usando
   `CStr::from_bytes_with_nul`.
6. Intenta `.to_str()` (debería fallar) y usa `.to_string_lossy()` como fallback.
7. Convierte los bytes a `OsStr` (sin el null) y muéstralo.

**Pista**: `CStr::from_bytes_with_nul(bytes)` crea un `&CStr` desde un slice que incluye
el null terminator.

> **Pregunta de reflexión**: ¿En qué situaciones reales encontrarías strings que no son
> UTF-8 en Linux? Piensa en nombres de archivo, variables de entorno, output de programas
> legacy, y datos de red. ¿Cómo cambia tu estrategia (`.to_str()` vs `.to_string_lossy()`)
> según el contexto?
