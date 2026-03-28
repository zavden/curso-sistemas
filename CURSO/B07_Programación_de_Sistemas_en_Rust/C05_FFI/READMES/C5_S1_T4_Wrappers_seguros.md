# Wrappers Seguros

## Índice

1. [¿Por qué wrappers?](#1-por-qué-wrappers)
2. [Anatomía de un wrapper seguro](#2-anatomía-de-un-wrapper-seguro)
3. [Gestión de recursos: RAII sobre C](#3-gestión-de-recursos-raii-sobre-c)
4. [Conversión de errores](#4-conversión-de-errores)
5. [Strings y buffers](#5-strings-y-buffers)
6. [Callbacks de C en Rust](#6-callbacks-de-c-en-rust)
7. [Wrapper completo: ejemplo con base de datos](#7-wrapper-completo-ejemplo-con-base-de-datos)
8. [Testing de wrappers FFI](#8-testing-de-wrappers-ffi)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. ¿Por qué wrappers?

Los bindings crudos generados por bindgen (o escritos a mano) son funcionales pero
**inseguros**: cada llamada requiere `unsafe`, los errores se reportan como enteros, los
recursos no se liberan automáticamente, y los strings son punteros crudos. Un wrapper seguro
traduce la API de C a una API idiomática de Rust.

```
┌─── Capas de una librería FFI ─────────────────────────────┐
│                                                            │
│  Tu código Rust                                            │
│  ┌──────────────────────────────────────┐                  │
│  │  API segura (wrapper)                │ ← String, Result │
│  │  fn open(path: &str) -> Result<Db>   │    RAII, tipos   │
│  ├──────────────────────────────────────┤                  │
│  │  Bindings crudos (bindgen/-sys)      │ ← *const, c_int  │
│  │  unsafe fn db_open(*const c_char)    │    unsafe, raw    │
│  ├──────────────────────────────────────┤                  │
│  │  Librería C compilada (.a / .so)     │                  │
│  │  db_open(), db_close(), etc.         │                  │
│  └──────────────────────────────────────┘                  │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Patrón -sys + wrapper

La convención en el ecosistema Rust es separar en dos crates:

```
┌─── Convención de crates FFI ──────────────────────────────┐
│                                                            │
│  mylib-sys (crate):                                        │
│  • Bindings crudos (extern "C", #[link])                   │
│  • Generados por bindgen                                   │
│  • build.rs con cc y/o pkg-config                          │
│  • Todo es unsafe y usa tipos de C                         │
│  • Nombre siempre termina en -sys                          │
│                                                            │
│  mylib (crate):                                            │
│  • Depende de mylib-sys                                    │
│  • API segura, idiomática                                  │
│  • Result en vez de códigos de error                       │
│  • Drop para liberar recursos                              │
│  • String/&str en vez de *const c_char                     │
│  • Documentación orientada al usuario Rust                 │
│                                                            │
│  Ejemplos reales:                                          │
│  openssl-sys → openssl                                     │
│  sqlite3-sys → rusqlite                                    │
│  libz-sys    → flate2                                      │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

En proyectos pequeños, puedes tener ambas capas en el mismo crate. La separación en dos
crates es útil cuando la capa -sys es reutilizable por múltiples wrappers.

---

## 2. Anatomía de un wrapper seguro

### Principios

Un buen wrapper FFI sigue estas reglas:

1. **unsafe contenido, no expuesto**: el `unsafe` está dentro del wrapper, el usuario nunca
   lo ve.
2. **Tipos Rust en la API pública**: `&str`, `String`, `&[u8]`, `Result<T, E>`, no
   `*const c_char`, `c_int`.
3. **RAII**: recursos de C se liberan automáticamente con `Drop`.
4. **Errores como Result**: códigos de error de C se traducen a `Result<T, Error>`.
5. **Sin invariantes ocultas**: el wrapper impide estados inválidos que C permitiría.

### Ejemplo mínimo: wrapper de abs

```rust
use std::ffi::c_int;

// Binding crudo
extern "C" {
    fn abs(x: c_int) -> c_int;
}

// Wrapper seguro — NO necesita unsafe en la firma
pub fn absolute(x: i32) -> i32 {
    // unsafe contenido aquí, no expuesto al caller
    unsafe { abs(x as c_int) as i32 }
}

fn main() {
    // El usuario llama a la API segura
    println!("{}", absolute(-42));  // Sin unsafe necesario
}
```

### Ejemplo: wrapper de strerror

```rust
use std::ffi::{CStr, c_int, c_char};

extern "C" {
    fn strerror(errnum: c_int) -> *const c_char;
}

/// Returns the error message for an errno value.
pub fn error_message(errno: i32) -> String {
    unsafe {
        let ptr = strerror(errno as c_int);
        if ptr.is_null() {
            return format!("Unknown error {}", errno);
        }
        CStr::from_ptr(ptr).to_string_lossy().into_owned()
    }
}

fn main() {
    println!("ENOENT: {}", error_message(2));   // "No such file or directory"
    println!("EACCES: {}", error_message(13));  // "Permission denied"
}
```

El usuario recibe un `String` limpio. Toda la complejidad de punteros, null checks y
conversión CStr está encapsulada.

---

## 3. Gestión de recursos: RAII sobre C

### El patrón Open/Close → New/Drop

C usa pares de funciones para adquirir y liberar recursos (`open`/`close`, `malloc`/`free`,
`fopen`/`fclose`). Rust usa RAII: el recurso se libera automáticamente cuando el valor sale
del scope.

```rust
use std::ffi::{CString, CStr, c_char, c_int};
use std::io;

// --- Bindings crudos ---
extern "C" {
    fn fopen(path: *const c_char, mode: *const c_char) -> *mut libc::FILE;
    fn fclose(file: *mut libc::FILE) -> c_int;
    fn fgets(buf: *mut c_char, size: c_int, file: *mut libc::FILE) -> *mut c_char;
    fn fputs(s: *const c_char, file: *mut libc::FILE) -> c_int;
}

// --- Wrapper seguro ---
pub struct File {
    ptr: *mut libc::FILE,
}

impl File {
    /// Open a file. Returns an error if the file cannot be opened.
    pub fn open(path: &str, mode: &str) -> io::Result<Self> {
        let c_path = CString::new(path)
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "null in path"))?;
        let c_mode = CString::new(mode)
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "null in mode"))?;

        let ptr = unsafe { fopen(c_path.as_ptr(), c_mode.as_ptr()) };

        if ptr.is_null() {
            Err(io::Error::last_os_error())
        } else {
            Ok(File { ptr })
        }
    }

    /// Read a line from the file.
    pub fn read_line(&self) -> io::Result<Option<String>> {
        let mut buf = [0u8; 4096];

        let result = unsafe {
            fgets(
                buf.as_mut_ptr() as *mut c_char,
                buf.len() as c_int,
                self.ptr,
            )
        };

        if result.is_null() {
            Ok(None)  // EOF
        } else {
            let c_str = unsafe { CStr::from_ptr(buf.as_ptr() as *const c_char) };
            Ok(Some(c_str.to_string_lossy().into_owned()))
        }
    }

    /// Write a string to the file.
    pub fn write_str(&self, s: &str) -> io::Result<()> {
        let c_str = CString::new(s)
            .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "null in string"))?;

        let result = unsafe { fputs(c_str.as_ptr(), self.ptr) };

        if result < 0 {
            Err(io::Error::last_os_error())
        } else {
            Ok(())
        }
    }
}

// RAII: Drop libera el recurso automáticamente
impl Drop for File {
    fn drop(&mut self) {
        unsafe {
            fclose(self.ptr);
        }
    }
}

fn main() -> io::Result<()> {
    // Escribir
    {
        let f = File::open("/tmp/wrapper_test.txt", "w")?;
        f.write_str("Hello from wrapper!\n")?;
        f.write_str("Second line\n")?;
    }  // f.drop() → fclose() se llama automáticamente

    // Leer
    let f = File::open("/tmp/wrapper_test.txt", "r")?;
    while let Some(line) = f.read_line()? {
        print!("{}", line);
    }
    // f.drop() → fclose() automático al salir de main

    Ok(())
}
```

```
┌─── Flujo de vida del recurso ─────────────────────────────┐
│                                                            │
│  C (manual):                                               │
│  FILE *f = fopen("file", "r");                             │
│  // usar f...                                              │
│  fclose(f);  ← DEBES recordar llamarlo                     │
│  // si hay error/return antes: leak de recurso             │
│                                                            │
│  Rust (RAII wrapper):                                      │
│  let f = File::open("file", "r")?;                         │
│  // usar f...                                              │
│  // File::drop() se llama AUTOMÁTICAMENTE                  │
│  // Incluso si hay ? que retorna temprano                  │
│  // Incluso si hay panic                                   │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Send y Sync

Si tu wrapper guarda un puntero a un recurso C, necesitas considerar thread safety:

```rust
pub struct CResource {
    ptr: *mut SomeCType,
}

// Por defecto, *mut T NO es Send ni Sync
// Si la API C es thread-safe, puedes implementarlos:

// Send: se puede mover entre threads
unsafe impl Send for CResource {}

// Sync: se puede compartir entre threads (&CResource)
// Solo si la API C soporta acceso concurrente
// unsafe impl Sync for CResource {}

// Si NO es thread-safe, no implementes nada:
// el compilador impedirá compartir entre threads
```

---

## 4. Conversión de errores

### Patrón: código de error → Result

La mayoría de funciones C retornan un código de error (0 = éxito, -1 = error) y ponen
el detalle en `errno`:

```rust
use std::ffi::{CString, c_char, c_int};
use std::io;

extern "C" {
    fn chdir(path: *const c_char) -> c_int;
    fn mkdir(path: *const c_char, mode: libc::mode_t) -> c_int;
    fn rmdir(path: *const c_char) -> c_int;
}

/// Helper: convierte retorno C + errno a Result
fn check_errno(result: c_int) -> io::Result<()> {
    if result == 0 {
        Ok(())
    } else {
        Err(io::Error::last_os_error())
    }
}

/// Helper: convierte &str a CString con error idiomático
fn to_cstring(s: &str) -> io::Result<CString> {
    CString::new(s).map_err(|_| {
        io::Error::new(io::ErrorKind::InvalidInput, "null byte in string")
    })
}

pub fn change_dir(path: &str) -> io::Result<()> {
    let c_path = to_cstring(path)?;
    check_errno(unsafe { chdir(c_path.as_ptr()) })
}

pub fn make_dir(path: &str, mode: u32) -> io::Result<()> {
    let c_path = to_cstring(path)?;
    check_errno(unsafe { mkdir(c_path.as_ptr(), mode as libc::mode_t) })
}

pub fn remove_dir(path: &str) -> io::Result<()> {
    let c_path = to_cstring(path)?;
    check_errno(unsafe { rmdir(c_path.as_ptr()) })
}
```

### Error type custom

Para librerías con códigos de error propios (no errno):

```rust
use std::fmt;

// La librería C define sus propios códigos de error
const LIB_OK: i32 = 0;
const LIB_ERR_NOT_FOUND: i32 = 1;
const LIB_ERR_PERMISSION: i32 = 2;
const LIB_ERR_CORRUPT: i32 = 3;
const LIB_ERR_FULL: i32 = 4;

#[derive(Debug)]
pub enum LibError {
    NotFound,
    Permission,
    Corrupt,
    Full,
    Unknown(i32),
}

impl fmt::Display for LibError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            LibError::NotFound => write!(f, "resource not found"),
            LibError::Permission => write!(f, "permission denied"),
            LibError::Corrupt => write!(f, "data corrupted"),
            LibError::Full => write!(f, "storage full"),
            LibError::Unknown(code) => write!(f, "unknown error (code {})", code),
        }
    }
}

impl std::error::Error for LibError {}

impl From<i32> for LibError {
    fn from(code: i32) -> Self {
        match code {
            LIB_ERR_NOT_FOUND => LibError::NotFound,
            LIB_ERR_PERMISSION => LibError::Permission,
            LIB_ERR_CORRUPT => LibError::Corrupt,
            LIB_ERR_FULL => LibError::Full,
            other => LibError::Unknown(other),
        }
    }
}

fn check_lib_error(code: i32) -> Result<(), LibError> {
    if code == LIB_OK {
        Ok(())
    } else {
        Err(LibError::from(code))
    }
}
```

### Funciones que retornan puntero NULL en error

```rust
use std::ffi::{CString, CStr, c_char};
use std::io;

extern "C" {
    fn realpath(path: *const c_char, resolved: *mut c_char) -> *mut c_char;
}

/// Resolve a path to its canonical absolute form.
pub fn canonical_path(path: &str) -> io::Result<String> {
    let c_path = CString::new(path)
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "null in path"))?;

    let result = unsafe { realpath(c_path.as_ptr(), std::ptr::null_mut()) };

    if result.is_null() {
        return Err(io::Error::last_os_error());
    }

    let resolved = unsafe { CStr::from_ptr(result) };
    let path_str = resolved.to_string_lossy().into_owned();

    // realpath con NULL segundo argumento usa malloc → debemos liberar
    unsafe { libc::free(result as *mut libc::c_void); }

    Ok(path_str)
}
```

---

## 5. Strings y buffers

### Patrón: output buffer

Muchas funciones C reciben un buffer donde escriben el resultado. El wrapper debe manejar
la asignación del buffer:

```rust
use std::ffi::{CStr, c_char, c_int};
use std::io;

extern "C" {
    fn gethostname(name: *mut c_char, len: libc::size_t) -> c_int;
    fn getcwd(buf: *mut c_char, size: libc::size_t) -> *mut c_char;
}

/// Get the system hostname.
pub fn hostname() -> io::Result<String> {
    let mut buf = vec![0u8; 256];

    let result = unsafe {
        gethostname(buf.as_mut_ptr() as *mut c_char, buf.len())
    };

    if result != 0 {
        return Err(io::Error::last_os_error());
    }

    let c_str = unsafe { CStr::from_ptr(buf.as_ptr() as *const c_char) };
    Ok(c_str.to_string_lossy().into_owned())
}

/// Get the current working directory.
pub fn current_dir() -> io::Result<String> {
    let mut buf = vec![0u8; libc::PATH_MAX as usize];

    let result = unsafe {
        getcwd(buf.as_mut_ptr() as *mut c_char, buf.len())
    };

    if result.is_null() {
        return Err(io::Error::last_os_error());
    }

    let c_str = unsafe { CStr::from_ptr(buf.as_ptr() as *const c_char) };
    Ok(c_str.to_string_lossy().into_owned())
}

fn main() -> io::Result<()> {
    println!("Hostname: {}", hostname()?);
    println!("CWD: {}", current_dir()?);
    Ok(())
}
```

### Patrón: buffer con retry de tamaño

Algunas funciones retornan un error si el buffer es pequeño. El wrapper puede reintentar
con un buffer más grande:

```rust
use std::ffi::{CStr, c_char};
use std::io;

extern "C" {
    fn getcwd(buf: *mut c_char, size: libc::size_t) -> *mut c_char;
}

pub fn current_dir_dynamic() -> io::Result<String> {
    let mut size: usize = 256;

    loop {
        let mut buf = vec![0u8; size];

        let result = unsafe {
            getcwd(buf.as_mut_ptr() as *mut c_char, buf.len())
        };

        if !result.is_null() {
            let c_str = unsafe { CStr::from_ptr(buf.as_ptr() as *const c_char) };
            return Ok(c_str.to_string_lossy().into_owned());
        }

        let err = io::Error::last_os_error();
        if err.raw_os_error() == Some(libc::ERANGE) {
            // Buffer too small, double and retry
            size *= 2;
            if size > 1_000_000 {
                return Err(io::Error::new(
                    io::ErrorKind::Other,
                    "path unreasonably long",
                ));
            }
        } else {
            return Err(err);
        }
    }
}
```

### Patrón: slice → puntero + longitud

```rust
use std::ffi::c_int;

extern "C" {
    fn compute_sum(data: *const f64, count: c_int) -> f64;
    fn sort_array(data: *mut c_int, count: c_int);
}

/// Compute the sum of a slice of f64 values.
pub fn sum(data: &[f64]) -> f64 {
    if data.is_empty() {
        return 0.0;
    }
    unsafe { compute_sum(data.as_ptr(), data.len() as c_int) }
}

/// Sort a slice of i32 values in place.
pub fn sort(data: &mut [i32]) {
    if data.is_empty() {
        return;
    }
    unsafe { sort_array(data.as_mut_ptr() as *mut c_int, data.len() as c_int) }
}
```

El wrapper convierte slices de Rust (`&[T]`, `&mut [T]`) a puntero + longitud, y maneja
el caso de slice vacío (pasar puntero null o longitud 0 a C puede ser undefined behavior
según la librería).

---

## 6. Callbacks de C en Rust

### Funciones C que reciben function pointers

```c
// sort.h
typedef int (*compare_fn)(const void *a, const void *b);

void custom_sort(int *array, int count, compare_fn cmp);

typedef void (*event_handler)(int event_type, const char *message, void *user_data);

void register_handler(event_handler handler, void *user_data);
void trigger_event(int event_type, const char *message);
```

### Wrapper con callback estático (función Rust)

```rust
use std::ffi::{c_int, c_void};

extern "C" {
    fn custom_sort(
        array: *mut c_int,
        count: c_int,
        cmp: extern "C" fn(*const c_void, *const c_void) -> c_int,
    );
}

extern "C" fn ascending(a: *const c_void, b: *const c_void) -> c_int {
    unsafe {
        let a = *(a as *const c_int);
        let b = *(b as *const c_int);
        a - b
    }
}

extern "C" fn descending(a: *const c_void, b: *const c_void) -> c_int {
    unsafe {
        let a = *(a as *const c_int);
        let b = *(b as *const c_int);
        b - a
    }
}

pub fn sort_ascending(data: &mut [i32]) {
    unsafe {
        custom_sort(
            data.as_mut_ptr() as *mut c_int,
            data.len() as c_int,
            ascending,
        );
    }
}

pub fn sort_descending(data: &mut [i32]) {
    unsafe {
        custom_sort(
            data.as_mut_ptr() as *mut c_int,
            data.len() as c_int,
            descending,
        );
    }
}
```

### Wrapper con closure via user_data

El patrón `void *user_data` en C es la forma de pasar contexto arbitrario a callbacks.
Lo podemos usar para pasar closures de Rust:

```rust
use std::ffi::{c_int, c_void, c_char, CStr};

// C API con user_data
extern "C" {
    fn register_handler(
        handler: extern "C" fn(c_int, *const c_char, *mut c_void),
        user_data: *mut c_void,
    );
    fn trigger_event(event_type: c_int, message: *const c_char);
}

// Trampoline: función extern "C" que llama a nuestro closure
extern "C" fn trampoline<F>(event_type: c_int, message: *const c_char, user_data: *mut c_void)
where
    F: FnMut(i32, &str),
{
    let closure = unsafe { &mut *(user_data as *mut F) };
    let msg = unsafe {
        if message.is_null() {
            ""
        } else {
            CStr::from_ptr(message).to_str().unwrap_or("<invalid utf8>")
        }
    };
    closure(event_type as i32, msg);
}

/// Register an event handler using a Rust closure.
pub fn on_event<F>(mut callback: F)
where
    F: FnMut(i32, &str) + 'static,
{
    let user_data = &mut callback as *mut F as *mut c_void;

    unsafe {
        register_handler(trampoline::<F>, user_data);
    }

    // IMPORTANTE: callback no debe dropearse mientras el handler
    // está registrado. En un caso real, usarías Box::into_raw
    // para transferir ownership.
    std::mem::forget(callback);
}
```

```
┌─── Cómo funciona el trampoline ───────────────────────────┐
│                                                            │
│  Rust:  on_event(|type, msg| println!("{}: {}", type, msg))│
│              │                                             │
│              ▼                                             │
│  Box closure → *mut c_void (user_data)                     │
│              │                                             │
│              ▼                                             │
│  C:    register_handler(trampoline::<F>, user_data)        │
│                                                            │
│  ... más tarde, cuando ocurre un evento ...                │
│                                                            │
│  C:    handler(event_type, message, user_data)             │
│              │                                             │
│              ▼                                             │
│  trampoline::<F>(event_type, message, user_data)           │
│              │                                             │
│              ▼                                             │
│  user_data → &mut F → closure(event_type, msg)             │
│              │                                             │
│              ▼                                             │
│  Tu closure se ejecuta con los datos del evento            │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Versión más segura con Box

```rust
use std::ffi::{c_int, c_void, c_char, CStr};

extern "C" {
    fn set_callback(
        cb: Option<extern "C" fn(c_int, *mut c_void)>,
        user_data: *mut c_void,
    );
}

struct CallbackHandle {
    _data: *mut c_void,
    _drop_fn: unsafe fn(*mut c_void),
}

impl Drop for CallbackHandle {
    fn drop(&mut self) {
        unsafe { (self._drop_fn)(self._data); }
    }
}

pub fn set_handler<F: FnMut(i32) + 'static>(callback: F) -> CallbackHandle {
    let boxed: Box<F> = Box::new(callback);
    let raw = Box::into_raw(boxed);

    extern "C" fn call<F: FnMut(i32)>(value: c_int, user_data: *mut c_void) {
        let closure = unsafe { &mut *(user_data as *mut F) };
        closure(value as i32);
    }

    unsafe fn drop_box<F>(ptr: *mut c_void) {
        drop(Box::from_raw(ptr as *mut F));
    }

    unsafe {
        set_callback(Some(call::<F>), raw as *mut c_void);
    }

    CallbackHandle {
        _data: raw as *mut c_void,
        _drop_fn: drop_box::<F>,
    }
}
```

---

## 7. Wrapper completo: ejemplo con base de datos

Veamos un ejemplo realista que combina todos los patrones. Supongamos una librería C de
base de datos key-value:

```c
// kvdb.h
#ifndef KVDB_H
#define KVDB_H

#define KVDB_OK            0
#define KVDB_ERR_NOT_FOUND 1
#define KVDB_ERR_IO        2
#define KVDB_ERR_CORRUPT   3
#define KVDB_ERR_FULL      4

typedef struct kvdb kvdb_t;

kvdb_t *kvdb_open(const char *path);
void    kvdb_close(kvdb_t *db);
int     kvdb_put(kvdb_t *db, const char *key, const char *value);
int     kvdb_get(kvdb_t *db, const char *key, char *value, int value_size);
int     kvdb_delete(kvdb_t *db, const char *key);
int     kvdb_count(kvdb_t *db);

typedef void (*kvdb_iter_fn)(const char *key, const char *value, void *user_data);
int     kvdb_foreach(kvdb_t *db, kvdb_iter_fn fn, void *user_data);

#endif
```

### Wrapper completo

```rust
use std::ffi::{CStr, CString, c_char, c_int, c_void};
use std::fmt;

// --- Raw bindings (normalmente en un módulo separado o crate -sys) ---

#[repr(C)]
struct kvdb_t {
    _private: [u8; 0],
}

const KVDB_OK: c_int = 0;
const KVDB_ERR_NOT_FOUND: c_int = 1;
const KVDB_ERR_IO: c_int = 2;
const KVDB_ERR_CORRUPT: c_int = 3;
const KVDB_ERR_FULL: c_int = 4;

extern "C" {
    fn kvdb_open(path: *const c_char) -> *mut kvdb_t;
    fn kvdb_close(db: *mut kvdb_t);
    fn kvdb_put(db: *mut kvdb_t, key: *const c_char, value: *const c_char) -> c_int;
    fn kvdb_get(
        db: *mut kvdb_t,
        key: *const c_char,
        value: *mut c_char,
        value_size: c_int,
    ) -> c_int;
    fn kvdb_delete(db: *mut kvdb_t, key: *const c_char) -> c_int;
    fn kvdb_count(db: *mut kvdb_t) -> c_int;
    fn kvdb_foreach(
        db: *mut kvdb_t,
        f: extern "C" fn(*const c_char, *const c_char, *mut c_void),
        user_data: *mut c_void,
    ) -> c_int;
}

// --- Error type ---

#[derive(Debug)]
pub enum DbError {
    NotFound,
    Io,
    Corrupt,
    Full,
    OpenFailed(String),
    InvalidInput(String),
    Unknown(i32),
}

impl fmt::Display for DbError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            DbError::NotFound => write!(f, "key not found"),
            DbError::Io => write!(f, "I/O error"),
            DbError::Corrupt => write!(f, "database corrupted"),
            DbError::Full => write!(f, "database full"),
            DbError::OpenFailed(path) => write!(f, "failed to open database: {}", path),
            DbError::InvalidInput(msg) => write!(f, "invalid input: {}", msg),
            DbError::Unknown(code) => write!(f, "unknown error (code {})", code),
        }
    }
}

impl std::error::Error for DbError {}

fn check_result(code: c_int) -> Result<(), DbError> {
    match code {
        KVDB_OK => Ok(()),
        KVDB_ERR_NOT_FOUND => Err(DbError::NotFound),
        KVDB_ERR_IO => Err(DbError::Io),
        KVDB_ERR_CORRUPT => Err(DbError::Corrupt),
        KVDB_ERR_FULL => Err(DbError::Full),
        other => Err(DbError::Unknown(other as i32)),
    }
}

fn to_c(s: &str) -> Result<CString, DbError> {
    CString::new(s).map_err(|e| {
        DbError::InvalidInput(format!("null byte at position {}", e.nul_position()))
    })
}

// --- Safe wrapper ---

/// A key-value database backed by a C library.
///
/// The database is automatically closed when this value is dropped.
pub struct Database {
    ptr: *mut kvdb_t,
}

// Safety: kvdb_t is internally synchronized (assumed thread-safe)
unsafe impl Send for Database {}

impl Database {
    /// Open a database at the given path.
    pub fn open(path: &str) -> Result<Self, DbError> {
        let c_path = to_c(path)?;
        let ptr = unsafe { kvdb_open(c_path.as_ptr()) };

        if ptr.is_null() {
            Err(DbError::OpenFailed(path.to_string()))
        } else {
            Ok(Database { ptr })
        }
    }

    /// Store a key-value pair.
    pub fn put(&self, key: &str, value: &str) -> Result<(), DbError> {
        let c_key = to_c(key)?;
        let c_value = to_c(value)?;
        check_result(unsafe { kvdb_put(self.ptr, c_key.as_ptr(), c_value.as_ptr()) })
    }

    /// Retrieve the value for a key. Returns None if not found.
    pub fn get(&self, key: &str) -> Result<Option<String>, DbError> {
        let c_key = to_c(key)?;
        let mut buf = vec![0u8; 4096];

        let result = unsafe {
            kvdb_get(
                self.ptr,
                c_key.as_ptr(),
                buf.as_mut_ptr() as *mut c_char,
                buf.len() as c_int,
            )
        };

        match result {
            KVDB_OK => {
                let c_str = unsafe { CStr::from_ptr(buf.as_ptr() as *const c_char) };
                Ok(Some(c_str.to_string_lossy().into_owned()))
            }
            KVDB_ERR_NOT_FOUND => Ok(None),
            other => Err(check_result(other).unwrap_err()),
        }
    }

    /// Delete a key. Returns Ok(true) if deleted, Ok(false) if not found.
    pub fn delete(&self, key: &str) -> Result<bool, DbError> {
        let c_key = to_c(key)?;
        let result = unsafe { kvdb_delete(self.ptr, c_key.as_ptr()) };

        match result {
            KVDB_OK => Ok(true),
            KVDB_ERR_NOT_FOUND => Ok(false),
            other => Err(check_result(other).unwrap_err()),
        }
    }

    /// Return the number of entries in the database.
    pub fn count(&self) -> usize {
        let n = unsafe { kvdb_count(self.ptr) };
        n.max(0) as usize
    }

    /// Iterate over all key-value pairs.
    pub fn for_each<F>(&self, mut callback: F) -> Result<(), DbError>
    where
        F: FnMut(&str, &str),
    {
        extern "C" fn trampoline<F: FnMut(&str, &str)>(
            key: *const c_char,
            value: *const c_char,
            user_data: *mut c_void,
        ) {
            let callback = unsafe { &mut *(user_data as *mut F) };
            let key = unsafe { CStr::from_ptr(key) }.to_string_lossy();
            let value = unsafe { CStr::from_ptr(value) }.to_string_lossy();
            callback(&key, &value);
        }

        let user_data = &mut callback as *mut F as *mut c_void;
        check_result(unsafe {
            kvdb_foreach(self.ptr, trampoline::<F>, user_data)
        })
    }
}

impl Drop for Database {
    fn drop(&mut self) {
        unsafe { kvdb_close(self.ptr); }
    }
}
```

### Uso del wrapper

```rust
fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Abrir base de datos (auto-close al salir)
    let db = Database::open("/tmp/mydb")?;

    // Insertar datos
    db.put("name", "Alice")?;
    db.put("city", "Madrid")?;

    // Leer datos
    match db.get("name")? {
        Some(value) => println!("name = {}", value),
        None => println!("name not found"),
    }

    // Contar entries
    println!("Total entries: {}", db.count());

    // Iterar con closure
    db.for_each(|key, value| {
        println!("  {} → {}", key, value);
    })?;

    // Eliminar
    let deleted = db.delete("city")?;
    println!("Deleted 'city': {}", deleted);

    Ok(())
    // db se cierra automáticamente aquí
}
```

Compara esta API con los bindings crudos:
- `Database::open` vs `kvdb_open` + check null + CString
- `db.get("key")?` vs `kvdb_get(ptr, cstr.as_ptr(), buf.as_mut_ptr(), ...)` + check code
- Drop automático vs `kvdb_close` manual
- Closure en `for_each` vs function pointer + void* user_data

---

## 8. Testing de wrappers FFI

### Tests unitarios

```rust
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_hostname() {
        let name = hostname().expect("should get hostname");
        assert!(!name.is_empty(), "hostname should not be empty");
        println!("hostname: {}", name);
    }

    #[test]
    fn test_current_dir() {
        let dir = current_dir().expect("should get cwd");
        assert!(dir.starts_with('/'), "cwd should be absolute path");
    }

    #[test]
    fn test_error_message() {
        let msg = error_message(2);  // ENOENT
        assert!(msg.contains("No such file") || msg.contains("not found"),
            "expected ENOENT message, got: {}", msg);
    }

    #[test]
    fn test_canonical_path() {
        let path = canonical_path("/tmp").expect("should resolve /tmp");
        // /tmp might be a symlink to /private/tmp on macOS
        assert!(path.starts_with('/'));
    }

    #[test]
    fn test_canonical_path_not_found() {
        let result = canonical_path("/nonexistent/path/that/does/not/exist");
        assert!(result.is_err());
    }
}
```

### Testing con recursos temporales

```rust
#[cfg(test)]
mod tests {
    use super::*;
    use std::fs;

    // Helper: crear directorio temporal para tests
    fn temp_dir() -> String {
        let dir = format!("/tmp/rust_ffi_test_{}", std::process::id());
        fs::create_dir_all(&dir).unwrap();
        dir
    }

    #[test]
    fn test_file_wrapper() {
        let dir = temp_dir();
        let path = format!("{}/test.txt", dir);

        // Escribir
        {
            let f = File::open(&path, "w").expect("should open for writing");
            f.write_str("line 1\n").unwrap();
            f.write_str("line 2\n").unwrap();
        }  // File se cierra aquí

        // Leer
        {
            let f = File::open(&path, "r").expect("should open for reading");
            let line1 = f.read_line().unwrap();
            assert_eq!(line1, Some("line 1\n".to_string()));
            let line2 = f.read_line().unwrap();
            assert_eq!(line2, Some("line 2\n".to_string()));
            let eof = f.read_line().unwrap();
            assert_eq!(eof, None);
        }

        // Cleanup
        fs::remove_dir_all(&dir).unwrap();
    }

    #[test]
    fn test_file_not_found() {
        let result = File::open("/nonexistent/file.txt", "r");
        assert!(result.is_err());
    }
}
```

---

## 9. Errores comunes

### Error 1: no implementar Drop para recursos C

```rust
// MAL: el recurso C nunca se libera
pub struct Connection {
    ptr: *mut ConnectionRaw,
}

impl Connection {
    pub fn open(addr: &str) -> Result<Self, Error> {
        // ...
        Ok(Connection { ptr })
    }
    // Sin Drop → connection_close() nunca se llama
    // → resource leak en cada uso
}

// BIEN: siempre implementar Drop
impl Drop for Connection {
    fn drop(&mut self) {
        if !self.ptr.is_null() {
            unsafe { connection_close(self.ptr); }
        }
    }
}
```

**Por qué falla**: sin `Drop`, el puntero se destruye pero el recurso C que representa
(file descriptor, conexión, memoria) no se libera. Cada instancia creada es un leak.

### Error 2: exponer unsafe en la API pública

```rust
// MAL: el usuario tiene que usar unsafe
pub fn read_file(path: *const c_char) -> *const c_char {
    unsafe { ffi_read_file(path) }
}

// BIEN: API segura con tipos Rust
pub fn read_file(path: &str) -> Result<String, io::Error> {
    let c_path = CString::new(path)
        .map_err(|_| io::Error::new(io::ErrorKind::InvalidInput, "null in path"))?;

    let result = unsafe { ffi_read_file(c_path.as_ptr()) };

    if result.is_null() {
        return Err(io::Error::last_os_error());
    }

    let content = unsafe { CStr::from_ptr(result) };
    let owned = content.to_string_lossy().into_owned();

    unsafe { ffi_free_string(result as *mut c_char); }

    Ok(owned)
}
```

**Por qué falla**: el propósito del wrapper es que el usuario no necesite `unsafe`. Si la
API pública usa punteros crudos, no has wrapeado nada — solo moviste el problema.

### Error 3: usar el recurso después de drop

```rust
pub struct Database {
    ptr: *mut db_t,
}

impl Drop for Database {
    fn drop(&mut self) {
        unsafe { db_close(self.ptr); }
        // Después de close, self.ptr es un dangling pointer
        // Si alguien tiene una referencia, puede usar-after-free
    }
}

// El compilador de Rust previene esto con lifetimes:
// let db = Database::open("path")?;
// let value = db.get("key")?;  // borrowea &db
// drop(db);                     // Error: db todavía está borrowed
// println!("{}", value);        // value tiene lifetime de &db
```

Este error es más relevante cuando intentas almacenar punteros derivados del recurso en
structs separados. El borrow checker de Rust te protege en la mayoría de los casos.

### Error 4: no manejar panics en callbacks

```rust
// MAL: si el closure hace panic, el panic se propaga a través de C
// Esto es undefined behavior
extern "C" fn trampoline(user_data: *mut c_void) {
    let callback = unsafe { &mut *(user_data as *mut Box<dyn FnMut()>) };
    callback();  // Si esto hace panic → UB al cruzar boundary C
}

// BIEN: catch_unwind en el trampoline
extern "C" fn trampoline_safe(user_data: *mut c_void) {
    let callback = unsafe { &mut *(user_data as *mut Box<dyn FnMut()>) };
    let result = std::panic::catch_unwind(std::panic::AssertUnwindSafe(|| {
        callback();
    }));
    if let Err(e) = result {
        eprintln!("Panic in FFI callback: {:?}", e);
        // No propagar el panic a C
    }
}
```

**Por qué falla**: un panic de Rust que se propaga a través de un stack frame C es
undefined behavior. El stack de C no sabe manejar panics. Siempre usa `catch_unwind`
en callbacks que pasan por C.

### Error 5: olvidar que CString::new puede fallar

```rust
// MAL: unwrap incondicional
pub fn open(path: &str) -> Result<Handle, Error> {
    let c_path = CString::new(path).unwrap();  // Panic si path contiene \0
    // ...
}

// BIEN: propagar el error
pub fn open(path: &str) -> Result<Handle, Error> {
    let c_path = CString::new(path).map_err(|_| {
        Error::InvalidInput("path contains null byte".to_string())
    })?;
    // ...
}
```

**Por qué falla**: si el usuario pasa un string con `\0` (quizás leyendo datos binarios),
el `unwrap` hace panic en vez de retornar un error. Un wrapper robusto nunca hace panic por
input del usuario.

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════════╗
║                       WRAPPERS SEGUROS                                 ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                        ║
║  PRINCIPIOS                                                            ║
║  ──────────                                                            ║
║  • unsafe dentro, no en la API pública                                 ║
║  • Tipos Rust en la firma: &str, Result, &[T]                          ║
║  • RAII: impl Drop para liberar recursos C                             ║
║  • Errores como Result, no códigos de retorno                          ║
║                                                                        ║
║  RAII                                                                  ║
║  ────                                                                  ║
║  struct Wrapper { ptr: *mut CType }                                    ║
║  impl Wrapper {                                                        ║
║      fn new() → Result<Self> { ptr = unsafe { c_open() }; ... }        ║
║  }                                                                     ║
║  impl Drop for Wrapper {                                               ║
║      fn drop(&mut self) { unsafe { c_close(self.ptr); } }              ║
║  }                                                                     ║
║                                                                        ║
║  ERRORES                                                               ║
║  ───────                                                               ║
║  Código C → Result:                                                    ║
║  fn check(code: c_int) -> Result<(), Error> {                          ║
║      if code == 0 { Ok(()) } else { Err(Error::from(code)) }           ║
║  }                                                                     ║
║  NULL → Result:                                                        ║
║  if ptr.is_null() { Err(io::Error::last_os_error()) }                  ║
║                                                                        ║
║  STRINGS                                                               ║
║  ───────                                                               ║
║  &str → C: CString::new(s)?.as_ptr()                                  ║
║  C → String: CStr::from_ptr(ptr).to_string_lossy().into_owned()        ║
║  Output buffer: let mut buf = vec![0u8; N]; ... CStr::from_ptr(buf)    ║
║                                                                        ║
║  SLICES                                                                ║
║  ──────                                                                ║
║  &[T] → C: data.as_ptr(), data.len() as c_int                         ║
║  &mut [T] → C: data.as_mut_ptr(), data.len() as c_int                 ║
║  Verificar !data.is_empty() antes de pasar puntero                     ║
║                                                                        ║
║  CALLBACKS                                                             ║
║  ─────────                                                             ║
║  Estático: extern "C" fn mi_fn(args) { ... }                           ║
║  Closure:  trampoline genérico + user_data = Box::into_raw(closure)    ║
║  SIEMPRE:  catch_unwind en trampolines (panic → UB si cruza C)        ║
║                                                                        ║
║  THREAD SAFETY                                                         ║
║  ─────────────                                                         ║
║  unsafe impl Send for Wrapper {}   si es movible entre threads         ║
║  unsafe impl Sync for Wrapper {}   si es compartible (&T) entre threads║
║  No implementar si la API C no es thread-safe                          ║
║                                                                        ║
║  CONVENCIÓN -sys                                                       ║
║  ───────────────                                                       ║
║  mylib-sys: bindings crudos + build.rs (extern "C", *const, c_int)     ║
║  mylib: wrapper seguro (String, Result, Drop, closures)                ║
║                                                                        ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: wrapper RAII para file descriptors

Crea un wrapper seguro sobre las syscalls `open`, `read`, `write`, `close` de libc:

1. Define `struct FileDescriptor { fd: c_int }`.
2. Implementa:
   - `FileDescriptor::open(path: &str, flags: i32) -> io::Result<Self>` — abre un archivo.
   - `fn read(&self, buf: &mut [u8]) -> io::Result<usize>` — lee datos.
   - `fn write(&self, data: &[u8]) -> io::Result<usize>` — escribe datos.
3. Implementa `Drop` que llame a `libc::close(self.fd)`.
4. Prueba escribiendo y leyendo un archivo temporal.

Verifica que el file descriptor se cierra automáticamente: abre muchos archivos en un loop
sin cerrar explícitamente y comprueba que no obtienes "too many open files".

**Pista**: `libc::open` retorna -1 en error. Usa `io::Error::last_os_error()` para obtener
el error real.

> **Pregunta de reflexión**: ¿qué pasaría si `FileDescriptor` implementara `Clone`?
> Dos instancias tendrían el mismo `fd` y ambas llamarían `close` al dropearse. ¿Qué
> nombre tiene este bug? ¿Cómo lo prevendrías?

---

### Ejercicio 2: tipo de error custom para una librería C

Supón una librería C que define estos códigos de error:
```c
#define ERR_OK       0
#define ERR_NOMEM    1
#define ERR_IO       2
#define ERR_PARSE    3
#define ERR_TIMEOUT  4
#define ERR_AUTH     5
```

1. Crea un `enum LibError` con una variante para cada código más `Unknown(i32)`.
2. Implementa `Display` y `std::error::Error`.
3. Implementa `From<i32>` para convertir códigos a variantes.
4. Crea un helper `fn check(code: i32) -> Result<(), LibError>`.
5. Simula 3 funciones C que retornan estos códigos y escribe wrappers seguros que usen
   `Result<T, LibError>`.

**Pista**: `std::error::Error` solo requiere `Debug + Display`. Implementa ambos traits.

> **Pregunta de reflexión**: ¿por qué la variante `Unknown(i32)` es importante? ¿Qué pasa
> si la librería C agrega un nuevo código de error en una versión futura y tu enum no lo
> contempla?

---

### Ejercicio 3: wrapper con callback para qsort

Crea un wrapper seguro para `qsort` de libc que acepte closures de Rust:

1. La firma C es: `void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *))`.
2. Tu wrapper debe tener la firma: `fn sort_by<T, F>(slice: &mut [T], compare: F)` donde
   `F: FnMut(&T, &T) -> std::cmp::Ordering`.
3. Usa el patrón trampoline con una función genérica `extern "C" fn` y pasa el closure
   como contexto a través del puntero (necesitarás una variable thread-local o un wrapper
   struct ya que `qsort` no tiene `user_data`).
4. Prueba ordenando un `Vec<i32>` ascendente y descendente, y un `Vec<String>` por longitud.

**Pista**: dado que `qsort` no tiene `void *user_data`, puedes usar un `thread_local!`
para pasar el closure de forma segura. Alternativamente, puedes usar el puntero `base`
con un truco de generics.

> **Pregunta de reflexión**: `qsort` no tiene parámetro `user_data`, a diferencia de
> `qsort_r` (que sí lo tiene). ¿Por qué la falta de `user_data` hace tan difícil pasar
> closures de Rust? ¿Qué problema introduce usar `thread_local!` como alternativa?
