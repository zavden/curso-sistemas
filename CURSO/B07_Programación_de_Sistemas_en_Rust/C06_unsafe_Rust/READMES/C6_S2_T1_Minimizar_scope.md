# Minimizar scope de unsafe

## Índice

1. [El principio: unsafe mínimo](#el-principio-unsafe-mínimo)
2. [Bloques unsafe pequeños vs grandes](#bloques-unsafe-pequeños-vs-grandes)
3. [Separar validación de operación unsafe](#separar-validación-de-operación-unsafe)
4. [Extraer la lógica safe fuera del bloque](#extraer-la-lógica-safe-fuera-del-bloque)
5. [El patrón safe shell / unsafe core](#el-patrón-safe-shell--unsafe-core)
6. [Encapsular unsafe en módulos pequeños](#encapsular-unsafe-en-módulos-pequeños)
7. [Auditar unsafe: grep y herramientas](#auditar-unsafe-grep-y-herramientas)
8. [Errores comunes](#errores-comunes)
9. [Cheatsheet](#cheatsheet)
10. [Ejercicios](#ejercicios)

---

## El principio: unsafe mínimo

El principio fundamental es: **cuanto menos código haya dentro de `unsafe`, más fácil es razonar sobre su corrección**.

```
┌──────────────────────────────────────────────────────┐
│          Filosofía de unsafe mínimo                  │
├──────────────────────────────────────────────────────┤
│                                                      │
│  Código safe: el compilador lo verifica              │
│  Código unsafe: TÚ lo verificas                      │
│                                                      │
│  Más código unsafe = más código que auditar          │
│  Menos código unsafe = menos superficie de ataque    │
│                                                      │
│  Objetivo: el mínimo número de líneas unsafe         │
│  necesario para que el programa funcione             │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### Analogía: el quirófano

Piensa en `unsafe` como una cirugía:

- No quieres que toda la habitación sea un quirófano — solo la mesa de operaciones.
- Todo lo que puedas preparar fuera (esterilizar, diagnosticar, planificar), hazlo fuera.
- En el quirófano, solo lo estrictamente necesario.
- Después de la cirugía, la recuperación ocurre fuera.

---

## Bloques unsafe pequeños vs grandes

### Mal: bloque unsafe gigante

```rust
use std::ffi::{CStr, CString};

fn get_hostname_bad() -> String {
    // INCORRECTO: todo dentro de unsafe
    unsafe {
        let mut buf = vec![0u8; 256];
        let ret = libc::gethostname(
            buf.as_mut_ptr() as *mut libc::c_char,
            buf.len(),
        );
        if ret != 0 {
            return String::from("unknown");
        }
        let c_str = CStr::from_ptr(buf.as_ptr() as *const libc::c_char);
        let hostname = c_str.to_string_lossy().into_owned();
        hostname.trim().to_string()
        // ← todo esto es safe, no necesita estar dentro de unsafe
    }
}
```

### Bien: bloque unsafe mínimo

```rust
use std::ffi::CStr;

fn get_hostname_good() -> String {
    // Preparación: safe
    let mut buf = vec![0u8; 256];

    // Solo la llamada a libc es unsafe
    // SAFETY: buf tiene 256 bytes válidos, gethostname escribe hasta buf.len()-1
    // bytes y añade null terminator
    let ret = unsafe {
        libc::gethostname(buf.as_mut_ptr() as *mut libc::c_char, buf.len())
    };

    // Procesamiento del resultado: safe
    if ret != 0 {
        return String::from("unknown");
    }

    // SAFETY: gethostname retornó 0, garantizando que buf contiene
    // un string C válido (null-terminated)
    let c_str = unsafe {
        CStr::from_ptr(buf.as_ptr() as *const libc::c_char)
    };

    c_str.to_string_lossy().trim().to_string()
}
```

### Comparación visual

```
  Versión BAD                    Versión GOOD
  ───────────                    ────────────
  ┌─ unsafe ─────────────┐      ┌─ safe ─────────────┐
  │ preparar buffer      │      │ preparar buffer     │
  │ llamar libc          │      └─────────────────────┘
  │ verificar retorno    │      ┌─ unsafe ────────────┐
  │ convertir a CStr     │      │ llamar libc         │
  │ convertir a String   │      └─────────────────────┘
  │ trim                 │      ┌─ safe ──────────────┐
  └──────────────────────┘      │ verificar retorno   │
                                └─────────────────────┘
  100% unsafe                   ┌─ unsafe ────────────┐
  (6 líneas que auditar)        │ convertir a CStr    │
                                └─────────────────────┘
                                ┌─ safe ──────────────┐
                                │ convertir a String  │
                                │ trim                │
                                └─────────────────────┘
                                ~15% unsafe
                                (2 líneas que auditar)
```

> **Predicción**: ¿Cuántas de las 6 líneas en la versión "bad" realmente necesitan unsafe? Solo 2: la llamada a `libc::gethostname` y `CStr::from_ptr`. Las otras 4 son operaciones safe que se benefician gratis de la verificación del compilador.

---

## Separar validación de operación unsafe

El patrón más poderoso: **valida TODO antes del bloque unsafe, de modo que las precondiciones ya están cumplidas al entrar**.

### Ejemplo: acceso sin bounds check

```rust
fn get_unchecked_safe(data: &[i32], index: usize) -> Option<i32> {
    // VALIDACIÓN: safe, el compilador la verifica
    if index >= data.len() {
        return None;
    }

    // OPERACIÓN: unsafe, pero sabemos que index < len
    // SAFETY: acabo de verificar que index < data.len()
    Some(unsafe { *data.as_ptr().add(index) })
}

fn main() {
    let data = [10, 20, 30];
    assert_eq!(get_unchecked_safe(&data, 1), Some(20));
    assert_eq!(get_unchecked_safe(&data, 5), None); // safe: no UB
}
```

### Ejemplo: conversión UTF-8

```rust
fn ascii_to_uppercase(input: &[u8]) -> Option<String> {
    // VALIDACIÓN: safe
    if !input.is_ascii() {
        return None;
    }

    // TRANSFORMACIÓN: safe
    let uppercased: Vec<u8> = input.iter()
        .map(|b| b.to_ascii_uppercase())
        .collect();

    // CONVERSIÓN: unsafe, pero sabemos que es ASCII válido (⊂ UTF-8)
    // SAFETY: to_ascii_uppercase produce solo bytes ASCII,
    // que son un subconjunto de UTF-8
    Some(unsafe { String::from_utf8_unchecked(uppercased) })
}

fn main() {
    assert_eq!(
        ascii_to_uppercase(b"hello world"),
        Some(String::from("HELLO WORLD"))
    );
    assert_eq!(ascii_to_uppercase(&[0xFF, 0xFE]), None);
}
```

### Ejemplo: slice desde puntero con validación

```rust
use std::slice;

fn safe_slice_from_raw<'a, T>(ptr: *const T, len: usize) -> Option<&'a [T]> {
    // VALIDACIÓN 1: puntero no nulo
    if ptr.is_null() {
        return None;
    }

    // VALIDACIÓN 2: longitud razonable (evitar overflow en cálculos de tamaño)
    if len > isize::MAX as usize / std::mem::size_of::<T>().max(1) {
        return None;
    }

    // VALIDACIÓN 3: alineación correcta
    if (ptr as usize) % std::mem::align_of::<T>() != 0 {
        return None;
    }

    // OPERACIÓN: unsafe, pero todas las precondiciones verificadas
    // SAFETY:
    // - ptr no es nulo (verificado)
    // - ptr está alineado (verificado)
    // - len no causa overflow (verificado)
    // - NOTA: no podemos verificar que la memoria esté inicializada,
    //   eso queda bajo responsabilidad del caller
    Some(unsafe { slice::from_raw_parts(ptr, len) })
}
```

```
┌─────────────────────────────────────────────────┐
│    Patrón: validar ANTES, operar DESPUÉS        │
│                                                 │
│    ┌───────────────────────────────┐            │
│    │ if ptr.is_null() → None      │ safe       │
│    │ if len > MAX → None          │ checks     │
│    │ if !aligned → None           │            │
│    └──────────────┬────────────────┘            │
│                   │ todas las precondiciones    │
│                   │ cumplidas                   │
│                   ▼                             │
│    ┌───────────────────────────────┐            │
│    │ unsafe { from_raw_parts() }  │ mínimo     │
│    └───────────────────────────────┘            │
│                                                 │
└─────────────────────────────────────────────────┘
```

---

## Extraer la lógica safe fuera del bloque

### Antes: lógica mezclada con unsafe

```rust
fn process_c_strings(ptrs: &[*const libc::c_char]) -> Vec<String> {
    let mut result = Vec::new();

    for &ptr in ptrs {
        unsafe {
            if ptr.is_null() {
                result.push(String::from("<null>"));
                continue;
            }

            let c_str = std::ffi::CStr::from_ptr(ptr);
            let rust_str = c_str.to_string_lossy();

            if rust_str.len() > 100 {
                result.push(format!("{}...", &rust_str[..97]));
            } else {
                result.push(rust_str.into_owned());
            }
        }
    }

    result
}
```

### Después: unsafe mínimo, lógica safe separada

```rust
fn process_c_strings(ptrs: &[*const libc::c_char]) -> Vec<String> {
    ptrs.iter().map(|&ptr| {
        // Paso 1: convertir puntero C a &str (unsafe mínimo)
        let rust_str = if ptr.is_null() {
            return String::from("<null>");
        } else {
            // SAFETY: ptr no es nulo (verificado arriba),
            // asumimos que apunta a string C válido (contrato del caller)
            let c_str = unsafe { std::ffi::CStr::from_ptr(ptr) };
            c_str.to_string_lossy()
        };

        // Paso 2: procesar el string (100% safe)
        if rust_str.len() > 100 {
            format!("{}...", &rust_str[..97])
        } else {
            rust_str.into_owned()
        }
    }).collect()
}
```

### Helper functions para reducir unsafe

```rust
/// Convierte un puntero C a string Rust. Retorna None si es nulo.
fn c_ptr_to_str(ptr: *const libc::c_char) -> Option<String> {
    if ptr.is_null() {
        return None;
    }
    // SAFETY: ptr no es nulo (verificado), debe apuntar a string C válido
    let c_str = unsafe { std::ffi::CStr::from_ptr(ptr) };
    Some(c_str.to_string_lossy().into_owned())
}

// Ahora el código que lo usa es 100% safe
fn format_user_info(name: *const libc::c_char, email: *const libc::c_char) -> String {
    let name = c_ptr_to_str(name).unwrap_or_else(|| "<unknown>".to_string());
    let email = c_ptr_to_str(email).unwrap_or_else(|| "<no email>".to_string());
    format!("{} <{}>", name, email)
}
```

---

## El patrón safe shell / unsafe core

Este patrón arquitectónico organiza el código en capas: un núcleo unsafe pequeño y bien auditado, rodeado por una capa safe que es la API pública.

### Estructura

```
┌─────────────────────────────────────────────────────┐
│                Código del usuario                    │
│          (100% safe, usa la API pública)             │
├─────────────────────────────────────────────────────┤
│            Safe Shell (API pública)                  │
│   - Valida inputs                                    │
│   - Convierte tipos Rust ↔ tipos C                  │
│   - Maneja errores con Result                        │
│   - Documenta el comportamiento                      │
├─────────────────────────────────────────────────────┤
│            Unsafe Core (privado)                     │
│   - Llamadas extern "C"                              │
│   - Dereference de raw pointers                      │
│   - Mínimo código, máxima documentación SAFETY       │
└─────────────────────────────────────────────────────┘
```

### Ejemplo completo: wrapper de timer POSIX

```rust
mod timer {
    use std::io;

    // ══════════════════════════════════════════════
    // UNSAFE CORE — privado, mínimo, bien documentado
    // ══════════════════════════════════════════════

    fn raw_clock_gettime(clock_id: i32) -> io::Result<libc::timespec> {
        let mut ts: libc::timespec = libc::timespec {
            tv_sec: 0,
            tv_nsec: 0,
        };

        // SAFETY: ts es una variable local válida, clock_gettime
        // escribe en ella si retorna 0
        let ret = unsafe { libc::clock_gettime(clock_id, &mut ts) };

        if ret != 0 {
            Err(io::Error::last_os_error())
        } else {
            Ok(ts)
        }
    }

    fn raw_nanosleep(secs: u64, nanos: u64) -> io::Result<()> {
        let req = libc::timespec {
            tv_sec: secs as libc::time_t,
            tv_nsec: nanos as i64,
        };

        // SAFETY: req es una estructura válida en el stack
        let ret = unsafe { libc::nanosleep(&req, std::ptr::null_mut()) };

        if ret != 0 {
            Err(io::Error::last_os_error())
        } else {
            Ok(())
        }
    }

    // ══════════════════════════════════════════════
    // SAFE SHELL — público, ergonómico, idiomático
    // ══════════════════════════════════════════════

    /// Un instante de tiempo monótono.
    pub struct Instant {
        secs: i64,
        nanos: i64,
    }

    /// Duración entre dos instantes.
    pub struct Duration {
        total_nanos: u64,
    }

    impl Instant {
        /// Captura el instante actual.
        pub fn now() -> Self {
            // El core unsafe se encarga de la llamada a C
            let ts = raw_clock_gettime(libc::CLOCK_MONOTONIC)
                .expect("clock_gettime(CLOCK_MONOTONIC) should never fail");
            Instant {
                secs: ts.tv_sec as i64,
                nanos: ts.tv_nsec as i64,
            }
        }

        /// Tiempo transcurrido desde este instante.
        pub fn elapsed(&self) -> Duration {
            let now = Self::now();
            self.duration_to(&now)
        }

        /// Duración entre self y un instante posterior.
        fn duration_to(&self, later: &Instant) -> Duration {
            let secs = later.secs - self.secs;
            let nanos = later.nanos - self.nanos;

            let total_nanos = if nanos < 0 {
                ((secs - 1) as u64) * 1_000_000_000 + (nanos + 1_000_000_000) as u64
            } else {
                (secs as u64) * 1_000_000_000 + nanos as u64
            };

            Duration { total_nanos }
        }
    }

    impl Duration {
        pub fn as_nanos(&self) -> u64 { self.total_nanos }
        pub fn as_micros(&self) -> u64 { self.total_nanos / 1_000 }
        pub fn as_millis(&self) -> u64 { self.total_nanos / 1_000_000 }
        pub fn as_secs_f64(&self) -> f64 { self.total_nanos as f64 / 1e9 }
    }

    impl std::fmt::Display for Duration {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            if self.total_nanos < 1_000 {
                write!(f, "{}ns", self.total_nanos)
            } else if self.total_nanos < 1_000_000 {
                write!(f, "{:.1}μs", self.total_nanos as f64 / 1e3)
            } else if self.total_nanos < 1_000_000_000 {
                write!(f, "{:.2}ms", self.total_nanos as f64 / 1e6)
            } else {
                write!(f, "{:.3}s", self.total_nanos as f64 / 1e9)
            }
        }
    }

    /// Duerme por la duración especificada.
    pub fn sleep_ms(ms: u64) -> io::Result<()> {
        let secs = ms / 1000;
        let nanos = (ms % 1000) * 1_000_000;
        raw_nanosleep(secs, nanos)
    }
}

// ═══ USO: 100% safe ═══

fn main() {
    let start = timer::Instant::now();

    // Simular trabajo
    let sum: u64 = (0..1_000_000).sum();

    let elapsed = start.elapsed();
    println!("sum = {}, took {}", sum, elapsed);
    // sum = 499999500000, took 1.23ms
}
```

### Análisis del ratio safe/unsafe

```
┌──────────────────────────────────────┐
│  timer module — análisis              │
├──────────────────────────────────────┤
│  Líneas unsafe: ~6 (2 funciones raw) │
│  Líneas safe:   ~80 (todo lo demás)  │
│  Ratio:         ~7% unsafe           │
│                                      │
│  Superficie de auditoría:            │
│  - clock_gettime con timespec local  │
│  - nanosleep con timespec local      │
│  → Ambas trivialmente correctas      │
│                                      │
│  API pública: 0% unsafe              │
│  El usuario NUNCA toca unsafe        │
└──────────────────────────────────────┘
```

---

## Encapsular unsafe en módulos pequeños

### Un módulo = un invariante

Cada módulo que contiene `unsafe` debe tener un invariante claro y documentado:

```rust
/// Módulo que provee un bitmap thread-safe.
///
/// INVARIANTE: `ptr` siempre apunta a `(capacity + 7) / 8` bytes
/// válidos, asignados con el allocator global.
/// Todos los bits más allá de `capacity` son siempre 0.
mod bitmap {
    pub struct Bitmap {
        ptr: *mut u8,
        capacity: usize, // número de bits
    }

    impl Bitmap {
        pub fn new(capacity: usize) -> Self {
            let byte_count = (capacity + 7) / 8;
            let ptr = if byte_count == 0 {
                std::ptr::NonNull::dangling().as_ptr()
            } else {
                let layout = std::alloc::Layout::array::<u8>(byte_count).unwrap();
                // SAFETY: layout.size() > 0 (byte_count > 0)
                let ptr = unsafe { std::alloc::alloc_zeroed(layout) };
                if ptr.is_null() {
                    std::alloc::handle_alloc_error(layout);
                }
                ptr
            };
            Bitmap { ptr, capacity }
        }

        pub fn set(&mut self, index: usize, value: bool) {
            assert!(index < self.capacity, "index {} out of bounds (cap {})", index, self.capacity);
            let byte_idx = index / 8;
            let bit_idx = index % 8;

            // SAFETY: index < capacity, por lo tanto byte_idx < (capacity+7)/8,
            // que es el número de bytes asignados
            unsafe {
                let byte = &mut *self.ptr.add(byte_idx);
                if value {
                    *byte |= 1 << bit_idx;
                } else {
                    *byte &= !(1 << bit_idx);
                }
            }
        }

        pub fn get(&self, index: usize) -> bool {
            assert!(index < self.capacity, "index {} out of bounds (cap {})", index, self.capacity);
            let byte_idx = index / 8;
            let bit_idx = index % 8;

            // SAFETY: misma justificación que set
            unsafe {
                let byte = *self.ptr.add(byte_idx);
                byte & (1 << bit_idx) != 0
            }
        }

        pub fn capacity(&self) -> usize {
            self.capacity
        }

        pub fn count_ones(&self) -> usize {
            let byte_count = (self.capacity + 7) / 8;
            // SAFETY: ptr apunta a byte_count bytes válidos
            let bytes = unsafe {
                std::slice::from_raw_parts(self.ptr, byte_count)
            };
            bytes.iter().map(|b| b.count_ones() as usize).sum()
        }
    }

    impl Drop for Bitmap {
        fn drop(&mut self) {
            let byte_count = (self.capacity + 7) / 8;
            if byte_count > 0 {
                let layout = std::alloc::Layout::array::<u8>(byte_count).unwrap();
                // SAFETY: ptr fue asignado con alloc_zeroed con este layout
                unsafe { std::alloc::dealloc(self.ptr, layout); }
            }
        }
    }

    // SAFETY: Bitmap tiene ownership exclusivo de la memoria apuntada por ptr.
    // No hay aliasing. Transferir a otro thread es safe.
    unsafe impl Send for Bitmap {}
}

// USO: completamente safe
fn main() {
    let mut bm = bitmap::Bitmap::new(100);
    bm.set(0, true);
    bm.set(42, true);
    bm.set(99, true);

    println!("bit 42: {}", bm.get(42));     // true
    println!("bit 50: {}", bm.get(50));     // false
    println!("ones: {}", bm.count_ones());  // 3
}
```

### Guía: cuándo crear un módulo separado

```
┌──────────────────────────────────────────────────┐
│  ¿Debería extraer esto a un módulo separado?     │
│                                                  │
│  ✓ Tiene un invariante propio (ej: "ptr válido") │
│  ✓ Tiene más de 2-3 bloques unsafe               │
│  ✓ Otros módulos podrían reusar la funcionalidad │
│  ✓ El invariante es independiente del negocio    │
│                                                  │
│  ✗ Es un solo bloque unsafe trivial              │
│  ✗ El unsafe está íntimamente ligado al negocio  │
│  ✗ Extraerlo añadiría complejidad sin beneficio  │
│                                                  │
└──────────────────────────────────────────────────┘
```

---

## Auditar unsafe: grep y herramientas

### Buscar unsafe en tu codebase

```bash
# Contar bloques unsafe
grep -rn "unsafe" src/ --include="*.rs" | wc -l

# Ver cada uso con contexto
grep -rn "unsafe" src/ --include="*.rs" -A 2

# Buscar unsafe sin comentario SAFETY
grep -rn "unsafe {" src/ --include="*.rs" -B 1 | grep -v SAFETY
```

### cargo-geiger: auditoría de dependencias

```bash
# Instalar
cargo install cargo-geiger

# Analizar uso de unsafe en tu proyecto y dependencias
cargo geiger

# Salida ejemplo:
# Functions  Expressions  Impls  Traits  Methods
# 0/0        2/2          0/0    0/0     0/0      my_project
# 5/20       50/200       2/3    1/1     3/10     libc
# 0/0        0/0          0/0    0/0     0/0      serde
```

### clippy lints para unsafe

```rust
// En lib.rs o main.rs:

// Requiere comentario SAFETY en cada bloque unsafe
#![warn(clippy::undocumented_unsafe_blocks)]

// Detecta bloques unsafe innecesarios
#![warn(unused_unsafe)]

// Requiere unsafe explícito dentro de unsafe fn (prep para 2024)
#![warn(unsafe_op_in_unsafe_fn)]

// Prohíbe unsafe completamente (para módulos que no lo necesitan)
// #![forbid(unsafe_code)]
```

### #[forbid(unsafe_code)] en módulos safe

```rust
// En un módulo que NO debe tener unsafe:
#[forbid(unsafe_code)]
mod business_logic {
    pub fn calculate_total(prices: &[f64], tax_rate: f64) -> f64 {
        let subtotal: f64 = prices.iter().sum();
        subtotal * (1.0 + tax_rate)
    }

    // Si intentas usar unsafe aquí → error de compilación
    // unsafe { ... } // ERROR: usage of unsafe is forbidden
}

// En un módulo que SÍ lo necesita: no poner forbid
mod ffi_wrapper {
    pub fn get_pid() -> i32 {
        // SAFETY: getpid() no tiene precondiciones
        unsafe { libc::getpid() }
    }
}
```

```
┌─────────────────────────────────────────────────┐
│   Organización típica de un proyecto            │
│                                                 │
│   src/                                          │
│   ├── main.rs          #[forbid(unsafe_code)]   │
│   ├── business.rs      #[forbid(unsafe_code)]   │
│   ├── api.rs           #[forbid(unsafe_code)]   │
│   ├── models.rs        #[forbid(unsafe_code)]   │
│   ├── ffi/                                      │
│   │   ├── mod.rs       ← unsafe permitido       │
│   │   ├── bindings.rs  ← unsafe permitido       │
│   │   └── wrappers.rs  ← safe shell             │
│   └── util/                                     │
│       └── raw_buf.rs   ← unsafe permitido       │
│                                                 │
│   Resultado: unsafe concentrado en 2-3 archivos │
│   El resto del proyecto es verificado por rustc  │
└─────────────────────────────────────────────────┘
```

---

## Errores comunes

### 1. Bloque unsafe que incluye lógica de negocio

```rust
fn process_data(ptr: *const u8, len: usize) -> String {
    // INCORRECTO: la lógica de negocio no necesita unsafe
    unsafe {
        let data = std::slice::from_raw_parts(ptr, len);
        let filtered: Vec<u8> = data.iter()
            .filter(|&&b| b.is_ascii_alphanumeric())
            .copied()
            .collect();
        let result = String::from_utf8(filtered).unwrap_or_default();
        result.to_uppercase()
    }

    // CORRECTO: unsafe mínimo
    // SAFETY: caller garantiza que ptr apunta a len bytes válidos
    // let data = unsafe { std::slice::from_raw_parts(ptr, len) };
    // let filtered: Vec<u8> = data.iter()
    //     .filter(|&&b| b.is_ascii_alphanumeric())
    //     .copied()
    //     .collect();
    // String::from_utf8(filtered).unwrap_or_default().to_uppercase()
}
```

### 2. Validar DENTRO del bloque unsafe

```rust
fn from_raw(ptr: *const i32, len: usize) -> Option<Vec<i32>> {
    // INCORRECTO: validación dentro de unsafe
    unsafe {
        if ptr.is_null() { return None; }
        if len == 0 { return Some(Vec::new()); }
        let slice = std::slice::from_raw_parts(ptr, len);
        Some(slice.to_vec())
    }

    // CORRECTO: validación ANTES de unsafe
    // if ptr.is_null() { return None; }
    // if len == 0 { return Some(Vec::new()); }
    // // SAFETY: ptr no es nulo (verificado), len > 0
    // let slice = unsafe { std::slice::from_raw_parts(ptr, len) };
    // Some(slice.to_vec())
}
```

### 3. No usar #[forbid(unsafe_code)] donde corresponde

```rust
// Si tu módulo de rutas HTTP tiene unsafe, algo está mal:
mod routes {
    // Este módulo no debería tener NINGÚN unsafe
    // Añadir #[forbid(unsafe_code)] lo garantiza

    pub fn handle_login(user: &str, pass: &str) -> Result<String, String> {
        // Lógica de negocio pura — 0% unsafe
        if user.is_empty() || pass.is_empty() {
            return Err("empty credentials".to_string());
        }
        Ok(format!("welcome, {}", user))
    }
}
```

### 4. Copiar-pegar bloques unsafe sin adaptar el comentario SAFETY

```rust
fn example_a(ptr: *const i32) -> i32 {
    // SAFETY: ptr apunta a un array de 10 elementos
    unsafe { *ptr.add(5) }
}

fn example_b(ptr: *const i32) -> i32 {
    // INCORRECTO: copió el comentario de example_a sin pensar
    // SAFETY: ptr apunta a un array de 10 elementos
    //         ← ¿de verdad? ¿quién lo garantiza aquí?
    unsafe { *ptr.add(5) }

    // CORRECTO: cada SAFETY debe razonarse independientemente
    // SAFETY: el caller debe garantizar que ptr apunta a al menos 6 elementos
    // unsafe { *ptr.add(5) }
}
```

### 5. No considerar que el módulo completo es el trust boundary

```rust
mod my_vec {
    pub struct MyVec {
        ptr: *mut i32,
        pub len: usize,  // ← ERROR: len es público
        cap: usize,
    }

    impl MyVec {
        pub fn get(&self, index: usize) -> Option<i32> {
            if index < self.len {
                // SAFETY: index < len, los primeros len están inicializados
                Some(unsafe { *self.ptr.add(index) })
                // ↑ INCORRECTO: código externo puede poner len = 999
                //   porque len es pub → unsafe ya no es safe
            } else {
                None
            }
        }
    }
}

// Código externo:
// let mut v = MyVec::new();
// v.len = 9999; // ← rompe el invariante
// v.get(5000);  // ← UB
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║          MINIMIZAR SCOPE — REFERENCIA RÁPIDA             ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  PRINCIPIO                                                ║
║  ─────────                                                ║
║  Menos unsafe = menos que auditar = menos bugs            ║
║  Solo las operaciones que REQUIEREN unsafe van dentro     ║
║                                                           ║
║  PATRÓN: VALIDAR → OPERAR                                ║
║  ────────────────────────                                  ║
║  1. Verificar precondiciones (safe)                       ║
║  2. unsafe { operación mínima }                           ║
║  3. Procesar resultado (safe)                             ║
║                                                           ║
║  PATRÓN: SAFE SHELL / UNSAFE CORE                        ║
║  ────────────────────────────────                          ║
║  mod wrapper {                                            ║
║      fn raw_call() → io::Result   // unsafe core privado ║
║      pub fn nice_api() → Result   // safe shell público   ║
║  }                                                        ║
║                                                           ║
║  HERRAMIENTAS                                             ║
║  ────────────                                             ║
║  #![warn(clippy::undocumented_unsafe_blocks)]             ║
║  #![warn(unsafe_op_in_unsafe_fn)]                         ║
║  #![forbid(unsafe_code)]  // en módulos 100% safe        ║
║  cargo geiger             // auditar dependencias         ║
║                                                           ║
║  ORGANIZACIÓN                                             ║
║  ────────────                                             ║
║  Un módulo = un invariante                                ║
║  Campos privados protegen el invariante                   ║
║  #[forbid(unsafe_code)] en business logic                 ║
║  unsafe concentrado en ffi/ o raw/ modules                ║
║                                                           ║
║  CHECKLIST                                                ║
║  ─────────                                                ║
║  □ ¿Esta línea NECESITA estar en unsafe?                  ║
║  □ ¿Puedo mover la validación FUERA del bloque?          ║
║  □ ¿Puedo extraer un helper safe?                        ║
║  □ ¿El comentario SAFETY es específico a ESTE bloque?    ║
║  □ ¿Los campos que protegen el invariante son privados?  ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Refactorizar unsafe excesivo

El siguiente código funciona pero tiene demasiado unsafe. Refactorízalo para minimizar el scope sin cambiar el comportamiento:

```rust
use std::ffi::{CStr, CString};

fn get_env_vars(names: &[&str]) -> Vec<(String, String)> {
    let mut result = Vec::new();

    for &name in names {
        unsafe {
            let c_name = CString::new(name).unwrap();
            let value_ptr = libc::getenv(c_name.as_ptr());

            if value_ptr.is_null() {
                continue;
            }

            let value = CStr::from_ptr(value_ptr);
            let value_string = value.to_string_lossy().into_owned();
            let name_upper = name.to_uppercase();
            let formatted = format!("{}={}", name_upper, value_string);

            if formatted.len() < 200 {
                result.push((name_upper, value_string));
            } else {
                result.push((name_upper, value_string[..100].to_string()));
            }
        }
    }

    result
}
```

Objetivos:
- Solo las llamadas a `libc::getenv` y `CStr::from_ptr` deben estar en bloques unsafe.
- Toda la lógica de formateo y filtrado debe ser safe.
- Añade comentarios `// SAFETY:` a cada bloque.

**Pregunta de reflexión**: después de tu refactorización, ¿cuántas líneas dentro de `unsafe` quedan? ¿Qué porcentaje del código total es unsafe ahora vs antes?

---

### Ejercicio 2: Safe shell para operaciones de archivos

Crea un módulo `safe_fs` que encapsule operaciones de archivos usando llamadas directas a libc, con la arquitectura safe shell / unsafe core:

```rust
mod safe_fs {
    use std::io;

    // UNSAFE CORE (privado)
    fn raw_open(path: &str, flags: i32, mode: u32) -> io::Result<i32> { todo!() }
    fn raw_read(fd: i32, buf: &mut [u8]) -> io::Result<usize> { todo!() }
    fn raw_write(fd: i32, data: &[u8]) -> io::Result<usize> { todo!() }
    fn raw_close(fd: i32) -> io::Result<()> { todo!() }

    // SAFE SHELL (público)
    pub struct File { fd: i32 }

    impl File {
        pub fn create(path: &str) -> io::Result<Self> { todo!() }
        pub fn open(path: &str) -> io::Result<Self> { todo!() }
        pub fn read(&self, buf: &mut [u8]) -> io::Result<usize> { todo!() }
        pub fn write(&self, data: &[u8]) -> io::Result<usize> { todo!() }
        pub fn read_to_string(&self) -> io::Result<String> { todo!() }
    }

    impl Drop for File {
        fn drop(&mut self) { todo!() }
    }
}
```

Prueba con:
```rust
let f = safe_fs::File::create("/tmp/test_safe_fs.txt")?;
f.write(b"hello from safe_fs!")?;
drop(f);

let f = safe_fs::File::open("/tmp/test_safe_fs.txt")?;
let content = f.read_to_string()?;
assert_eq!(content, "hello from safe_fs!");
```

**Pregunta de reflexión**: ¿Qué pasa si `File` se clona (si implementamos `Clone`)? Dos `File` tendrían el mismo `fd` y ambos llamarían `close` en `Drop` → double close. ¿Cómo lo evitarías?

---

### Ejercicio 3: Auditoría de código unsafe

Dado este proyecto simulado, aplica los principios de minimización. Para cada bloque unsafe: (a) ¿es necesario?, (b) ¿se puede reducir?, (c) ¿falta el comentario SAFETY?, (d) ¿hay campos que deberían ser privados?

```rust
pub struct RingBuffer {
    pub ptr: *mut u8,    // (d) ¿debería ser privado?
    pub cap: usize,      // (d) ¿debería ser privado?
    pub head: usize,
    pub tail: usize,
    pub len: usize,
}

impl RingBuffer {
    pub fn new(capacity: usize) -> Self {
        unsafe {
            let layout = std::alloc::Layout::array::<u8>(capacity).unwrap();
            let ptr = std::alloc::alloc(layout);
            if ptr.is_null() {
                panic!("allocation failed");
            }
            RingBuffer { ptr, cap: capacity, head: 0, tail: 0, len: 0 }
        }
    }

    pub fn push(&mut self, byte: u8) -> bool {
        unsafe {
            if self.len == self.cap {
                return false;
            }
            *self.ptr.add(self.head) = byte;
            self.head = (self.head + 1) % self.cap;
            self.len += 1;
            true
        }
    }

    pub fn pop(&mut self) -> Option<u8> {
        unsafe {
            if self.len == 0 {
                return None;
            }
            let byte = *self.ptr.add(self.tail);
            self.tail = (self.tail + 1) % self.cap;
            self.len -= 1;
            Some(byte)
        }
    }

    pub fn peek(&self) -> Option<u8> {
        unsafe {
            if self.len == 0 {
                None
            } else {
                Some(*self.ptr.add(self.tail))
            }
        }
    }
}

impl Drop for RingBuffer {
    fn drop(&mut self) {
        unsafe {
            let layout = std::alloc::Layout::array::<u8>(self.cap).unwrap();
            std::alloc::dealloc(self.ptr, layout);
        }
    }
}
```

Reescribe el código aplicando:
1. Todos los campos privados.
2. Validaciones fuera de unsafe.
3. Comentarios SAFETY en cada bloque.
4. Bloques unsafe mínimos (solo la operación de puntero).

**Pregunta de reflexión**: en `push`, la línea `self.head = (self.head + 1) % self.cap` es aritmética pura — ¿necesita estar dentro de unsafe? ¿Qué beneficio concreto hay en sacarla?
