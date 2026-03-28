# Tipos Opacos

## Índice

1. [¿Qué es un tipo opaco?](#1-qué-es-un-tipo-opaco)
2. [Box\<T\> como puntero opaco](#2-boxt-como-puntero-opaco)
3. [Constructor, destructor y métodos](#3-constructor-destructor-y-métodos)
4. [Ownership a través del boundary](#4-ownership-a-través-del-boundary)
5. [Tipos opacos con estado complejo](#5-tipos-opacos-con-estado-complejo)
6. [Manejo de errores con tipos opacos](#6-manejo-de-errores-con-tipos-opacos)
7. [Patrones completos](#7-patrones-completos)
8. [Tipos opacos y cbindgen](#8-tipos-opacos-y-cbindgen)
9. [Errores comunes](#9-errores-comunes)
10. [Cheatsheet](#10-cheatsheet)
11. [Ejercicios](#11-ejercicios)

---

## 1. ¿Qué es un tipo opaco?

Un tipo opaco es un tipo cuya estructura interna está oculta al caller. El código C solo
ve un puntero — no sabe qué hay dentro, no puede crear instancias directamente, no puede
acceder a los campos. Solo puede usar las funciones que le provees.

```
┌─── Tipo transparente vs opaco ────────────────────────────┐
│                                                            │
│  Transparente (#[repr(C)] struct):                         │
│  ┌─────────────────────────────────┐                       │
│  │ typedef struct {                │  C ve todo:            │
│  │     double x;                   │  puede crear, leer,   │
│  │     double y;                   │  modificar campos     │
│  │ } Point;                        │                       │
│  │ Point p = { .x = 1, .y = 2 };  │                       │
│  │ p.x = 3;                       │                       │
│  └─────────────────────────────────┘                       │
│                                                            │
│  Opaco (puntero a tipo incompleto):                        │
│  ┌─────────────────────────────────┐                       │
│  │ typedef struct Database Database│  C solo ve un puntero: │
│  │                                 │  no puede crear,      │
│  │ Database *db = db_open("path"); │  no ve campos,        │
│  │ db_query(db, "SELECT ...");     │  solo usa funciones   │
│  │ db_close(db);                   │                       │
│  └─────────────────────────────────┘                       │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### ¿Por qué usar tipos opacos?

```
┌─── Ventajas de tipos opacos ──────────────────────────────┐
│                                                            │
│  1. Encapsulación:                                         │
│     El caller C no depende del layout interno              │
│     Puedes cambiar los campos de Rust sin romper C         │
│                                                            │
│  2. Seguridad:                                             │
│     C no puede corromper campos internos                   │
│     Los invariantes los mantiene Rust                      │
│                                                            │
│  3. Tipos Rust complejos:                                  │
│     Vec, String, HashMap, BTreeMap, etc.                   │
│     no tienen repr(C) — solo funcionan como opacos        │
│                                                            │
│  4. RAII desde C:                                          │
│     Constructor (new) + destructor (free)                  │
│     Rust gestiona la memoria internamente                  │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

---

## 2. Box\<T\> como puntero opaco

`Box<T>` es un puntero owned a un valor en el heap. Es el mecanismo perfecto para pasar
tipos Rust a C como punteros opacos:

```
┌─── Box<T> en FFI ─────────────────────────────────────────┐
│                                                            │
│  Rust side:                                                │
│  ┌──────────┐     ┌──────────────────────┐                 │
│  │ Box<Db>  │────→│ Db {                 │                 │
│  │ (stack)  │     │   conn: Connection,  │   heap          │
│  └──────────┘     │   cache: HashMap,    │                 │
│                   │   config: Config,    │                 │
│                   │ }                    │                 │
│                   └──────────────────────┘                 │
│                                                            │
│  FFI boundary (Box::into_raw):                             │
│  ┌──────────┐     ┌──────────────────────┐                 │
│  │ *mut Db  │────→│ Db { ... }           │                 │
│  │ (C tiene │     │ (sigue en el heap)   │                 │
│  │ el ptr)  │     │ (Rust no lo dropea)  │                 │
│  └──────────┘     └──────────────────────┘                 │
│                                                            │
│  Devolver a Rust (Box::from_raw):                          │
│  ┌──────────┐     ┌──────────────────────┐                 │
│  │ Box<Db>  │────→│ Db { ... }           │                 │
│  │ (Rust    │     │ (Drop al salir       │                 │
│  │ retoma)  │     │  del scope)          │                 │
│  └──────────┘     └──────────────────────┘                 │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Las tres operaciones fundamentales

```rust
use std::ffi::c_int;

pub struct Counter {
    value: i64,
    name: String,
}

// 1. CREAR: Box::new + Box::into_raw
//    Rust → C: transfiere ownership al caller C
#[no_mangle]
pub extern "C" fn counter_new(initial: c_int) -> *mut Counter {
    let counter = Counter {
        value: initial as i64,
        name: String::from("default"),
    };
    Box::into_raw(Box::new(counter))
    // Box se consume, el puntero queda vivo
    // C ahora "posee" esta memoria
}

// 2. USAR: convertir puntero a referencia temporalmente
//    C pasa el puntero, Rust lo usa sin tomar ownership
#[no_mangle]
pub extern "C" fn counter_increment(ptr: *mut Counter) {
    if ptr.is_null() { return; }
    let counter = unsafe { &mut *ptr };  // Borrow temporal
    counter.value += 1;
    // counter (la referencia) se dropea, pero el dato sigue vivo
}

#[no_mangle]
pub extern "C" fn counter_get(ptr: *const Counter) -> c_int {
    if ptr.is_null() { return -1; }
    let counter = unsafe { &*ptr };  // Borrow inmutable
    counter.value as c_int
}

// 3. DESTRUIR: Box::from_raw retoma ownership → Drop
//    C → Rust: devuelve ownership, Rust libera
#[no_mangle]
pub extern "C" fn counter_free(ptr: *mut Counter) {
    if ptr.is_null() { return; }
    unsafe {
        drop(Box::from_raw(ptr));
        // Box retoma ownership del puntero
        // Drop se ejecuta: String se libera, etc.
    }
}
```

```c
// Uso desde C
typedef struct Counter Counter;

extern Counter *counter_new(int initial);
extern void counter_increment(Counter *c);
extern int counter_get(const Counter *c);
extern void counter_free(Counter *c);

int main() {
    Counter *c = counter_new(0);

    counter_increment(c);
    counter_increment(c);
    counter_increment(c);
    printf("Value: %d\n", counter_get(c));  // 3

    counter_free(c);  // Libera toda la memoria interna
    return 0;
}
```

---

## 3. Constructor, destructor y métodos

### Patrón OOP en C con tipos opacos

Los tipos opacos permiten implementar un estilo orientado a objetos en C usando
convenciones de nombrado:

```
┌─── Patrón de nombrado ────────────────────────────────────┐
│                                                            │
│  tipo_new(args)        → *mut Tipo    constructor          │
│  tipo_free(ptr)        → void         destructor           │
│  tipo_method(ptr, ...) → result       método               │
│  tipo_get_field(ptr)   → value        getter               │
│  tipo_set_field(ptr,v) → void         setter               │
│                                                            │
│  Ejemplo con StringBuffer:                                 │
│  strbuf_new()                                              │
│  strbuf_free(sb)                                           │
│  strbuf_push(sb, "text")                                   │
│  strbuf_len(sb) → int                                      │
│  strbuf_as_str(sb) → const char*                           │
│  strbuf_clear(sb)                                          │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Ejemplo: StringBuffer

```rust
use std::ffi::{CStr, CString, c_char, c_int};

pub struct StringBuffer {
    data: String,
}

#[no_mangle]
pub extern "C" fn strbuf_new() -> *mut StringBuffer {
    Box::into_raw(Box::new(StringBuffer {
        data: String::new(),
    }))
}

#[no_mangle]
pub extern "C" fn strbuf_with_capacity(capacity: c_int) -> *mut StringBuffer {
    Box::into_raw(Box::new(StringBuffer {
        data: String::with_capacity(capacity.max(0) as usize),
    }))
}

#[no_mangle]
pub extern "C" fn strbuf_free(sb: *mut StringBuffer) {
    if !sb.is_null() {
        unsafe { drop(Box::from_raw(sb)); }
    }
}

#[no_mangle]
pub extern "C" fn strbuf_push(sb: *mut StringBuffer, text: *const c_char) {
    if sb.is_null() || text.is_null() { return; }
    let sb = unsafe { &mut *sb };
    let c_str = unsafe { CStr::from_ptr(text) };
    sb.data.push_str(&c_str.to_string_lossy());
}

#[no_mangle]
pub extern "C" fn strbuf_push_char(sb: *mut StringBuffer, c: c_char) {
    if sb.is_null() { return; }
    let sb = unsafe { &mut *sb };
    sb.data.push(c as u8 as char);
}

#[no_mangle]
pub extern "C" fn strbuf_len(sb: *const StringBuffer) -> c_int {
    if sb.is_null() { return 0; }
    let sb = unsafe { &*sb };
    sb.data.len() as c_int
}

/// Returns a pointer to the internal string data.
/// Valid until the next mutation or free.
#[no_mangle]
pub extern "C" fn strbuf_as_str(sb: *const StringBuffer) -> *const c_char {
    if sb.is_null() { return std::ptr::null(); }
    let sb = unsafe { &*sb };
    // Retornar puntero a datos internos — válido mientras sb existe
    // Necesitamos agregar null terminator
    // Usamos CString temporal y lo leakeamos como workaround
    // Mejor alternativa: mantener un CString cacheado internamente
    sb.data.as_ptr() as *const c_char
}

#[no_mangle]
pub extern "C" fn strbuf_clear(sb: *mut StringBuffer) {
    if sb.is_null() { return; }
    let sb = unsafe { &mut *sb };
    sb.data.clear();
}

/// Returns a newly allocated C string copy. Caller must free with strbuf_free_str.
#[no_mangle]
pub extern "C" fn strbuf_to_cstring(sb: *const StringBuffer) -> *mut c_char {
    if sb.is_null() { return std::ptr::null_mut(); }
    let sb = unsafe { &*sb };
    match CString::new(sb.data.as_str()) {
        Ok(cs) => cs.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn strbuf_free_str(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)); }
    }
}
```

```c
// Uso desde C
typedef struct StringBuffer StringBuffer;

int main() {
    StringBuffer *sb = strbuf_new();

    strbuf_push(sb, "Hello");
    strbuf_push(sb, ", ");
    strbuf_push(sb, "world!");

    printf("Length: %d\n", strbuf_len(sb));  // 13

    char *str = strbuf_to_cstring(sb);
    printf("Content: %s\n", str);  // Hello, world!
    strbuf_free_str(str);

    strbuf_clear(sb);
    printf("After clear: %d\n", strbuf_len(sb));  // 0

    strbuf_free(sb);
    return 0;
}
```

---

## 4. Ownership a través del boundary

### Modelo de ownership FFI

En FFI, la ownership se transfiere explícitamente con funciones. No hay RAII automático
desde C — el caller debe llamar a la función free manualmente.

```
┌─── Transfer de ownership ─────────────────────────────────┐
│                                                            │
│  Rust owns it:                                             │
│  let db = Database::new();                                 │
│  // Rust dropea al salir del scope                         │
│                                                            │
│  Transfer Rust → C:                                        │
│  let ptr = Box::into_raw(Box::new(db));                    │
│  // Rust YA NO es dueño. No hay Drop automático.           │
│  // C ahora es responsable.                                │
│                                                            │
│  C uses it:                                                │
│  db_query(ptr, "SELECT ...");                              │
│  // ptr sigue válido                                       │
│                                                            │
│  Transfer C → Rust (para destruir):                        │
│  db_free(ptr);                                             │
│  // Internamente: Box::from_raw(ptr) → Drop               │
│  // ptr es ahora inválido — use-after-free si se usa       │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Reglas de ownership en FFI

```
┌─── ¿Quién es dueño? ─────────────────────────────────────┐
│                                                            │
│  Regla 1: tipo_new() → *mut T                             │
│  El caller recibe ownership. DEBE llamar tipo_free().      │
│                                                            │
│  Regla 2: tipo_method(ptr, ...) → result                  │
│  Borrow temporal. El caller sigue siendo dueño.            │
│  ptr sigue válido después de la llamada.                   │
│                                                            │
│  Regla 3: tipo_free(ptr)                                   │
│  Devuelve ownership a Rust para destrucción.               │
│  ptr es INVÁLIDO después de esta llamada.                  │
│                                                            │
│  Regla 4: tipo_take(ptr) → *mut OtroTipo                  │
│  Transfiere ownership de un recurso interno.               │
│  El caller ahora posee OtroTipo.                           │
│                                                            │
│  Regla 5: tipo_get_ref(ptr) → *const Data                 │
│  Borrow de dato interno. Válido mientras ptr exista        │
│  y no se mute. NO llamar free sobre este puntero.          │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Documentar ownership en el header

```c
// mylib.h — ownership documentada en comentarios

/// Creates a new Database. The caller owns the returned pointer
/// and MUST call db_free() when done.
Database *db_new(const char *path);

/// Executes a query. Borrows the database temporarily.
/// The database remains valid after this call.
int db_query(Database *db, const char *sql);

/// Returns a copy of the value. The caller owns the returned string
/// and MUST call db_free_string() when done.
char *db_get(Database *db, const char *key);

/// Frees a Database. The pointer is invalid after this call.
/// Passing NULL is safe (no-op).
void db_free(Database *db);

/// Frees a string returned by db_get().
/// Passing NULL is safe (no-op).
void db_free_string(char *s);
```

---

## 5. Tipos opacos con estado complejo

### HashMap expuesto a C

```rust
use std::collections::HashMap;
use std::ffi::{CStr, CString, c_char, c_int};

pub struct KeyValueStore {
    map: HashMap<String, String>,
}

#[no_mangle]
pub extern "C" fn kvs_new() -> *mut KeyValueStore {
    Box::into_raw(Box::new(KeyValueStore {
        map: HashMap::new(),
    }))
}

#[no_mangle]
pub extern "C" fn kvs_free(kvs: *mut KeyValueStore) {
    if !kvs.is_null() {
        unsafe { drop(Box::from_raw(kvs)); }
        // Drop de KeyValueStore → Drop de HashMap → Drop de cada String
    }
}

#[no_mangle]
pub extern "C" fn kvs_set(
    kvs: *mut KeyValueStore,
    key: *const c_char,
    value: *const c_char,
) -> c_int {
    if kvs.is_null() || key.is_null() || value.is_null() {
        return -1;
    }
    let kvs = unsafe { &mut *kvs };
    let key = unsafe { CStr::from_ptr(key) }.to_string_lossy().into_owned();
    let value = unsafe { CStr::from_ptr(value) }.to_string_lossy().into_owned();
    kvs.map.insert(key, value);
    0
}

/// Get a value by key. Returns a newly allocated string.
/// Caller must free with kvs_free_string.
/// Returns NULL if key not found.
#[no_mangle]
pub extern "C" fn kvs_get(
    kvs: *const KeyValueStore,
    key: *const c_char,
) -> *mut c_char {
    if kvs.is_null() || key.is_null() {
        return std::ptr::null_mut();
    }
    let kvs = unsafe { &*kvs };
    let key = unsafe { CStr::from_ptr(key) }.to_string_lossy();

    match kvs.map.get(key.as_ref()) {
        Some(value) => match CString::new(value.as_str()) {
            Ok(cs) => cs.into_raw(),
            Err(_) => std::ptr::null_mut(),
        },
        None => std::ptr::null_mut(),
    }
}

#[no_mangle]
pub extern "C" fn kvs_remove(kvs: *mut KeyValueStore, key: *const c_char) -> c_int {
    if kvs.is_null() || key.is_null() { return -1; }
    let kvs = unsafe { &mut *kvs };
    let key = unsafe { CStr::from_ptr(key) }.to_string_lossy();

    if kvs.map.remove(key.as_ref()).is_some() { 1 } else { 0 }
}

#[no_mangle]
pub extern "C" fn kvs_len(kvs: *const KeyValueStore) -> c_int {
    if kvs.is_null() { return 0; }
    let kvs = unsafe { &*kvs };
    kvs.map.len() as c_int
}

#[no_mangle]
pub extern "C" fn kvs_contains(kvs: *const KeyValueStore, key: *const c_char) -> c_int {
    if kvs.is_null() || key.is_null() { return 0; }
    let kvs = unsafe { &*kvs };
    let key = unsafe { CStr::from_ptr(key) }.to_string_lossy();
    if kvs.map.contains_key(key.as_ref()) { 1 } else { 0 }
}

/// Iterate over all key-value pairs.
/// The callback receives key, value, and user_data for each entry.
#[no_mangle]
pub extern "C" fn kvs_foreach(
    kvs: *const KeyValueStore,
    callback: extern "C" fn(*const c_char, *const c_char, *mut std::ffi::c_void),
    user_data: *mut std::ffi::c_void,
) {
    if kvs.is_null() { return; }
    let kvs = unsafe { &*kvs };

    for (key, value) in &kvs.map {
        // Crear CStrings temporales para pasar a C
        if let (Ok(c_key), Ok(c_value)) = (CString::new(key.as_str()), CString::new(value.as_str())) {
            callback(c_key.as_ptr(), c_value.as_ptr(), user_data);
        }
    }
}

#[no_mangle]
pub extern "C" fn kvs_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)); }
    }
}
```

```c
// Uso desde C
#include <stdio.h>

typedef struct KeyValueStore KeyValueStore;

extern KeyValueStore *kvs_new(void);
extern void kvs_free(KeyValueStore *kvs);
extern int kvs_set(KeyValueStore *kvs, const char *key, const char *value);
extern char *kvs_get(const KeyValueStore *kvs, const char *key);
extern int kvs_remove(KeyValueStore *kvs, const char *key);
extern int kvs_len(const KeyValueStore *kvs);
extern int kvs_contains(const KeyValueStore *kvs, const char *key);
extern void kvs_foreach(const KeyValueStore *kvs,
    void (*callback)(const char*, const char*, void*), void *user_data);
extern void kvs_free_string(char *s);

void print_entry(const char *key, const char *value, void *data) {
    printf("  %s = %s\n", key, value);
}

int main() {
    KeyValueStore *kvs = kvs_new();

    kvs_set(kvs, "name", "Alice");
    kvs_set(kvs, "city", "Madrid");
    kvs_set(kvs, "lang", "Rust");

    printf("Count: %d\n", kvs_len(kvs));          // 3
    printf("Has 'name': %d\n", kvs_contains(kvs, "name"));  // 1

    char *val = kvs_get(kvs, "city");
    if (val) {
        printf("city = %s\n", val);  // Madrid
        kvs_free_string(val);
    }

    printf("All entries:\n");
    kvs_foreach(kvs, print_entry, NULL);

    kvs_remove(kvs, "lang");
    printf("After remove: %d\n", kvs_len(kvs));  // 2

    kvs_free(kvs);
    return 0;
}
```

### Vec expuesto a C

```rust
use std::ffi::c_int;

pub struct IntList {
    data: Vec<c_int>,
}

#[no_mangle]
pub extern "C" fn intlist_new() -> *mut IntList {
    Box::into_raw(Box::new(IntList { data: Vec::new() }))
}

#[no_mangle]
pub extern "C" fn intlist_free(list: *mut IntList) {
    if !list.is_null() {
        unsafe { drop(Box::from_raw(list)); }
    }
}

#[no_mangle]
pub extern "C" fn intlist_push(list: *mut IntList, value: c_int) {
    if list.is_null() { return; }
    unsafe { &mut *list }.data.push(value);
}

#[no_mangle]
pub extern "C" fn intlist_pop(list: *mut IntList) -> c_int {
    if list.is_null() { return -1; }
    unsafe { &mut *list }.data.pop().unwrap_or(-1)
}

#[no_mangle]
pub extern "C" fn intlist_get(list: *const IntList, index: c_int) -> c_int {
    if list.is_null() || index < 0 { return -1; }
    unsafe { &*list }.data.get(index as usize).copied().unwrap_or(-1)
}

#[no_mangle]
pub extern "C" fn intlist_len(list: *const IntList) -> c_int {
    if list.is_null() { return 0; }
    unsafe { &*list }.data.len() as c_int
}

#[no_mangle]
pub extern "C" fn intlist_sort(list: *mut IntList) {
    if list.is_null() { return; }
    unsafe { &mut *list }.data.sort();
}

/// Returns a pointer to the internal data for reading.
/// Valid until the next mutation.
#[no_mangle]
pub extern "C" fn intlist_data(list: *const IntList) -> *const c_int {
    if list.is_null() { return std::ptr::null(); }
    unsafe { &*list }.data.as_ptr()
}
```

---

## 6. Manejo de errores con tipos opacos

### Patrón: resultado en output parameter

```rust
use std::ffi::{CStr, CString, c_char, c_int};

pub struct Config {
    values: std::collections::HashMap<String, String>,
}

#[repr(C)]
pub enum ConfigError {
    Ok = 0,
    NullPointer = 1,
    KeyNotFound = 2,
    ParseError = 3,
    IoError = 4,
}

#[no_mangle]
pub extern "C" fn config_new() -> *mut Config {
    Box::into_raw(Box::new(Config {
        values: std::collections::HashMap::new(),
    }))
}

#[no_mangle]
pub extern "C" fn config_free(cfg: *mut Config) {
    if !cfg.is_null() {
        unsafe { drop(Box::from_raw(cfg)); }
    }
}

/// Load config from a file.
/// Returns error code. On success, *out is set to the new Config.
#[no_mangle]
pub extern "C" fn config_load(
    path: *const c_char,
    out: *mut *mut Config,
) -> ConfigError {
    if path.is_null() || out.is_null() {
        return ConfigError::NullPointer;
    }

    let path = unsafe { CStr::from_ptr(path) }.to_string_lossy();
    let content = match std::fs::read_to_string(path.as_ref()) {
        Ok(c) => c,
        Err(_) => return ConfigError::IoError,
    };

    let mut values = std::collections::HashMap::new();
    for line in content.lines() {
        if let Some((key, value)) = line.split_once('=') {
            values.insert(key.trim().to_string(), value.trim().to_string());
        }
    }

    let cfg = Box::into_raw(Box::new(Config { values }));
    unsafe { *out = cfg; }
    ConfigError::Ok
}

/// Get a config value. Returns error code.
/// On success, *out is set to a newly allocated string (free with config_free_string).
#[no_mangle]
pub extern "C" fn config_get(
    cfg: *const Config,
    key: *const c_char,
    out: *mut *mut c_char,
) -> ConfigError {
    if cfg.is_null() || key.is_null() || out.is_null() {
        return ConfigError::NullPointer;
    }

    let cfg = unsafe { &*cfg };
    let key = unsafe { CStr::from_ptr(key) }.to_string_lossy();

    match cfg.values.get(key.as_ref()) {
        Some(value) => {
            match CString::new(value.as_str()) {
                Ok(cs) => {
                    unsafe { *out = cs.into_raw(); }
                    ConfigError::Ok
                }
                Err(_) => ConfigError::ParseError,
            }
        }
        None => ConfigError::KeyNotFound,
    }
}

/// Get a config value as integer.
#[no_mangle]
pub extern "C" fn config_get_int(
    cfg: *const Config,
    key: *const c_char,
    out: *mut c_int,
) -> ConfigError {
    if cfg.is_null() || key.is_null() || out.is_null() {
        return ConfigError::NullPointer;
    }

    let cfg = unsafe { &*cfg };
    let key = unsafe { CStr::from_ptr(key) }.to_string_lossy();

    match cfg.values.get(key.as_ref()) {
        Some(value) => match value.parse::<i32>() {
            Ok(n) => {
                unsafe { *out = n; }
                ConfigError::Ok
            }
            Err(_) => ConfigError::ParseError,
        },
        None => ConfigError::KeyNotFound,
    }
}

#[no_mangle]
pub extern "C" fn config_free_string(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)); }
    }
}
```

```c
// Uso desde C
int main() {
    Config *cfg = NULL;
    ConfigError err = config_load("app.conf", &cfg);

    if (err != CONFIG_ERROR_OK) {
        fprintf(stderr, "Failed to load config: %d\n", err);
        return 1;
    }

    char *host = NULL;
    err = config_get(cfg, "host", &host);
    if (err == CONFIG_ERROR_OK) {
        printf("Host: %s\n", host);
        config_free_string(host);
    }

    int port;
    err = config_get_int(cfg, "port", &port);
    if (err == CONFIG_ERROR_OK) {
        printf("Port: %d\n", port);
    }

    config_free(cfg);
    return 0;
}
```

---

## 7. Patrones completos

### Logger opaco

```rust
use std::ffi::{CStr, c_char, c_int};
use std::fs::{File, OpenOptions};
use std::io::Write;
use std::sync::Mutex;

pub struct Logger {
    file: Mutex<File>,
    level: LogLevel,
    prefix: String,
}

#[repr(C)]
#[derive(Clone, Copy, PartialEq, PartialOrd)]
pub enum LogLevel {
    Error = 0,
    Warn = 1,
    Info = 2,
    Debug = 3,
}

#[no_mangle]
pub extern "C" fn logger_new(
    path: *const c_char,
    level: LogLevel,
    prefix: *const c_char,
) -> *mut Logger {
    if path.is_null() { return std::ptr::null_mut(); }

    let path = unsafe { CStr::from_ptr(path) }.to_string_lossy();
    let prefix = if prefix.is_null() {
        String::new()
    } else {
        unsafe { CStr::from_ptr(prefix) }.to_string_lossy().into_owned()
    };

    let file = match OpenOptions::new().create(true).append(true).open(path.as_ref()) {
        Ok(f) => f,
        Err(_) => return std::ptr::null_mut(),
    };

    Box::into_raw(Box::new(Logger {
        file: Mutex::new(file),
        level,
        prefix,
    }))
}

#[no_mangle]
pub extern "C" fn logger_free(logger: *mut Logger) {
    if !logger.is_null() {
        unsafe { drop(Box::from_raw(logger)); }
    }
}

#[no_mangle]
pub extern "C" fn logger_log(
    logger: *const Logger,
    level: LogLevel,
    message: *const c_char,
) -> c_int {
    if logger.is_null() || message.is_null() { return -1; }

    let logger = unsafe { &*logger };
    if level > logger.level { return 0; }  // Filtered out

    let msg = unsafe { CStr::from_ptr(message) }.to_string_lossy();
    let level_str = match level {
        LogLevel::Error => "ERROR",
        LogLevel::Warn => "WARN",
        LogLevel::Info => "INFO",
        LogLevel::Debug => "DEBUG",
    };

    let line = if logger.prefix.is_empty() {
        format!("[{}] {}\n", level_str, msg)
    } else {
        format!("[{}][{}] {}\n", logger.prefix, level_str, msg)
    };

    match logger.file.lock() {
        Ok(mut file) => {
            if file.write_all(line.as_bytes()).is_ok() { 0 } else { -2 }
        }
        Err(_) => -3,
    }
}

#[no_mangle]
pub extern "C" fn logger_error(logger: *const Logger, msg: *const c_char) -> c_int {
    logger_log(logger, LogLevel::Error, msg)
}

#[no_mangle]
pub extern "C" fn logger_info(logger: *const Logger, msg: *const c_char) -> c_int {
    logger_log(logger, LogLevel::Info, msg)
}

#[no_mangle]
pub extern "C" fn logger_set_level(logger: *mut Logger, level: LogLevel) {
    if logger.is_null() { return; }
    let logger = unsafe { &mut *logger };
    logger.level = level;
}
```

```c
// Uso desde C
int main() {
    Logger *log = logger_new("/tmp/app.log", LOG_LEVEL_INFO, "myapp");
    if (!log) {
        fprintf(stderr, "Failed to create logger\n");
        return 1;
    }

    logger_info(log, "Application started");
    logger_info(log, "Processing data...");
    logger_error(log, "Something went wrong");

    logger_set_level(log, LOG_LEVEL_DEBUG);
    logger_log(log, LOG_LEVEL_DEBUG, "Debug message now visible");

    logger_free(log);
    return 0;
}
```

---

## 8. Tipos opacos y cbindgen

cbindgen maneja tipos opacos automáticamente. Cuando encuentra un tipo que no es `repr(C)`,
lo genera como tipo opaco en el header:

```rust
// Rust: tipo sin repr(C) → cbindgen lo hace opaco
pub struct Database {
    conn: String,          // String no es FFI-safe
    cache: Vec<u8>,        // Vec no es FFI-safe
}

#[no_mangle]
pub extern "C" fn db_new() -> *mut Database {
    Box::into_raw(Box::new(Database {
        conn: String::new(),
        cache: Vec::new(),
    }))
}

#[no_mangle]
pub extern "C" fn db_free(db: *mut Database) {
    if !db.is_null() { unsafe { drop(Box::from_raw(db)); } }
}
```

cbindgen genera:

```c
// Header generado
typedef struct Database Database;

Database *db_new(void);
void db_free(Database *db);
```

El `typedef struct Database Database;` es una **forward declaration** — dice que existe
un tipo `Database` sin revelar su contenido. C puede usar punteros a este tipo pero no
puede acceder a campos ni crear instancias.

### Configuración para tipos opacos

```toml
# cbindgen.toml
[export]
# Excluir tipos que no quieres en el header
exclude = ["InternalHelper"]

# Tipos que siempre se tratan como opacos
[export.body]
# "TypeName" = "opaque"
```

---

## 9. Errores comunes

### Error 1: double free

```c
// MAL: liberar dos veces
Database *db = db_new();
db_free(db);
db_free(db);  // ← DOUBLE FREE: undefined behavior
```

```rust
// Protección parcial: verificar null y poner a null después
// (pero C no puede "poner a null" a través de un puntero por valor)

// Solución: documentar claramente en el header
/// Free a database. The pointer is INVALID after this call.
/// Calling db_free twice on the same pointer is undefined behavior.
/// Passing NULL is safe (no-op).
#[no_mangle]
pub extern "C" fn db_free(db: *mut Database) {
    if !db.is_null() {
        unsafe { drop(Box::from_raw(db)); }
    }
}
```

**Por qué falla**: `Box::from_raw` retoma ownership del puntero y lo libera con Drop.
Llamarlo dos veces intenta liberar la misma memoria dos veces → corrupción del heap.

### Error 2: usar el puntero después de free

```c
// MAL: use-after-free
Database *db = db_new();
db_put(db, "key", "value");
db_free(db);
db_get(db, "key");  // ← USE AFTER FREE: la memoria fue liberada
```

```rust
// No hay protección perfecta desde Rust para esto
// Solo puedes verificar null (pero db no es null, solo inválido)

// La mejor protección es buena documentación y disciplina en C:
// db_free(db);
// db = NULL;  // Buena práctica en C
// if (db) db_get(db, "key");  // Ahora es seguro
```

**Por qué falla**: después de `db_free`, la memoria puede ser reutilizada por otra
asignación. Acceder a ella lee datos de otro objeto o causa crash. Este es uno de los
bugs más comunes y peligrosos en C.

### Error 3: mezclar allocators

```c
// MAL: usar free() de C para memoria de Rust
char *s = rust_to_upper("hello");  // Asignado por Rust (jemalloc/system)
free(s);  // ← Liberado por C (glibc malloc) → UNDEFINED BEHAVIOR

// BIEN: usar la función free de Rust
char *s = rust_to_upper("hello");
rust_free_string(s);  // Liberado por el mismo allocator que lo creó
```

**Por qué falla**: Rust puede usar un allocator diferente al de C (`jemalloc`, un
allocator custom, etc.). Incluso con el mismo allocator del sistema, mezclar
`Box::from_raw` con `free()` es UB porque `Box` puede tener metadata interna diferente.

### Error 4: olvidar la función free

```rust
// MAL: retornar string asignada sin proveer free
#[no_mangle]
pub extern "C" fn get_greeting() -> *mut c_char {
    CString::new("Hello!").unwrap().into_raw()
    // ¿Cómo libera C esta memoria?
    // No hay función free exportada → memory leak garantizado
}

// BIEN: siempre acompañar con función free
#[no_mangle]
pub extern "C" fn get_greeting() -> *mut c_char {
    CString::new("Hello!").unwrap().into_raw()
}

#[no_mangle]
pub extern "C" fn free_greeting(s: *mut c_char) {
    if !s.is_null() {
        unsafe { drop(CString::from_raw(s)); }
    }
}
```

**Regla**: por cada función que retorna un puntero a memoria asignada por Rust, **debe**
existir una función free correspondiente. Sin excepción.

### Error 5: crear Box::from_raw de un puntero que no vino de Box::into_raw

```rust
#[no_mangle]
pub extern "C" fn process(data: *mut i32) {
    // MAL: data fue asignado por C con malloc, no por Box
    let boxed = unsafe { Box::from_raw(data) };
    // Drop intenta liberar con el deallocator de Rust
    // Pero la memoria fue asignada con malloc de C → UB
}
```

**Por qué falla**: `Box::from_raw` asume que el puntero vino de `Box::into_raw` (o
`Box::new` seguido de `into_raw`). Si viene de `malloc` u otra fuente, el deallocator
no coincide.

---

## 10. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════════╗
║                        TIPOS OPACOS                                    ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                        ║
║  CREAR (Rust → C)                                                      ║
║  ────────────────                                                      ║
║  let obj = MyType::new(...);                                           ║
║  Box::into_raw(Box::new(obj))    → *mut MyType para C                  ║
║  // Rust ya no es dueño. C debe llamar free.                           ║
║                                                                        ║
║  USAR (C pasa puntero a Rust)                                          ║
║  ────────────────────────────                                          ║
║  let obj = unsafe { &*ptr };         borrow inmutable                  ║
║  let obj = unsafe { &mut *ptr };     borrow mutable                    ║
║  // El dato sigue vivo después. Ownership no cambia.                   ║
║                                                                        ║
║  DESTRUIR (C → Rust)                                                   ║
║  ───────────────────                                                   ║
║  unsafe { drop(Box::from_raw(ptr)); }                                  ║
║  // Rust retoma ownership. Drop se ejecuta.                            ║
║  // ptr es INVÁLIDO después de esto.                                   ║
║                                                                        ║
║  PATRÓN COMPLETO                                                       ║
║  ───────────────                                                       ║
║  tipo_new(args) → *mut T       constructor (Box::into_raw)             ║
║  tipo_method(ptr, ...) → val   método (&*ptr / &mut *ptr)              ║
║  tipo_free(ptr)                destructor (Box::from_raw + drop)       ║
║  tipo_free_string(s)           free para strings retornados            ║
║                                                                        ║
║  NULL SAFETY                                                           ║
║  ───────────                                                           ║
║  if ptr.is_null() { return error; }  SIEMPRE antes de &*ptr            ║
║  tipo_free(NULL) debe ser no-op      convención estándar               ║
║                                                                        ║
║  STRINGS RETORNADOS                                                    ║
║  ──────────────────                                                    ║
║  CString::new(s).unwrap().into_raw() → *mut c_char                     ║
║  Caller debe liberar con tipo_free_string()                            ║
║  NUNCA usar free() de C para memoria de Rust                           ║
║                                                                        ║
║  CBINDGEN                                                              ║
║  ────────                                                              ║
║  Structs sin repr(C) → forward declaration automática                  ║
║  typedef struct MyType MyType;  (tipo opaco en header)                 ║
║                                                                        ║
║  OWNERSHIP RULES                                                       ║
║  ───────────────                                                       ║
║  1. new() → caller posee el puntero                                    ║
║  2. method(ptr) → borrow, caller sigue siendo dueño                    ║
║  3. free(ptr) → Rust retoma y libera, ptr es inválido                  ║
║  4. Cada puntero retornado necesita una función free                   ║
║  5. Double free = UB. Use-after-free = UB.                             ║
║  6. Solo Box::from_raw con punteros de Box::into_raw                   ║
║                                                                        ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 11. Ejercicios

### Ejercicio 1: stack opaco

Implementa un stack (LIFO) como tipo opaco para C:

1. Define `struct Stack` internamente con `Vec<i32>`.
2. Exporta: `stack_new() -> *mut Stack`, `stack_free(ptr)`,
   `stack_push(ptr, value)`, `stack_pop(ptr) -> i32` (-1 si vacío),
   `stack_peek(ptr) -> i32` (-1 si vacío), `stack_len(ptr) -> i32`,
   `stack_is_empty(ptr) -> i32`.
3. Escribe un programa C que use el stack para invertir una secuencia de números.
4. Genera el header con cbindgen.

Verifica que `stack_free` libera toda la memoria (usa valgrind si está disponible).

**Pista**: `Vec::push`, `Vec::pop`, `Vec::last` son las operaciones internas.

> **Pregunta de reflexión**: ¿por qué usamos `i32` como tipo de retorno para
> `stack_pop` con -1 como sentinel en vez de un parámetro de salida? ¿Qué desventaja
> tiene el sentinel? ¿Cómo diseñarías la API si el stack pudiera contener cualquier
> valor incluyendo -1?

---

### Ejercicio 2: tokenizer opaco

Crea un tokenizer de strings como tipo opaco:

1. `struct Tokenizer` que internamente almacena un `Vec<String>` con los tokens
   y un índice de posición actual.
2. `tokenizer_new(input: *const c_char, delimiter: c_char) -> *mut Tokenizer` — divide
   el string por el delimitador.
3. `tokenizer_next(ptr) -> *mut c_char` — retorna el siguiente token (owned, caller debe
   free) o NULL si no hay más.
4. `tokenizer_reset(ptr)` — vuelve al primer token.
5. `tokenizer_count(ptr) -> i32` — número total de tokens.
6. `tokenizer_free(ptr)` y `tokenizer_free_token(s)`.

Escribe un programa C que tokenice `"one:two:three:four"` por `:` e imprima cada token.

**Pista**: la posición actual es un `usize` dentro del struct. `tokenizer_next` la
incrementa y retorna el token en esa posición.

> **Pregunta de reflexión**: el tokenizer copia todos los tokens al crearse. ¿Sería
> más eficiente almacenar el string original y calcular los tokens on-demand? ¿Qué
> complicación de lifetime introduciría eso en el contexto FFI?

---

### Ejercicio 3: registro de usuarios opaco con serialización

Crea un registro de usuarios como tipo opaco:

1. `struct UserRegistry` con `HashMap<u32, User>` donde `User` tiene `name: String`,
   `email: String`, `age: u32`.
2. Exporta: `registry_new`, `registry_free`, `registry_add(ptr, id, name, email, age)`,
   `registry_remove(ptr, id)`, `registry_get_name(ptr, id) -> *mut c_char`,
   `registry_count(ptr) -> i32`.
3. Agrega `registry_to_json(ptr) -> *mut c_char` que serialice todo el registro a JSON
   usando `serde_json`.
4. Agrega `registry_from_json(json: *const c_char) -> *mut UserRegistry` que deserialice.
5. Genera el header con cbindgen y escribe un programa C que agregue usuarios, serialice
   a JSON, imprima el JSON, y reconstruya el registro desde el JSON.

**Pista**: `User` necesita `#[derive(Serialize, Deserialize)]` pero NO necesita `repr(C)`
porque es interno al tipo opaco.

> **Pregunta de reflexión**: el struct `User` no tiene `#[repr(C)]` porque es interno.
> ¿Qué pasaría si quisieras que C acceda directamente a los campos de un usuario?
> ¿Podrías tener una versión opaca para el registro y una versión `repr(C)` para
> usuarios individuales? ¿Cómo coexistirían?
