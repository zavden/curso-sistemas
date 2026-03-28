# Documentar invariantes: safety comments

## Índice

1. [¿Por qué documentar unsafe?](#por-qué-documentar-unsafe)
2. [Comentarios SAFETY en bloques unsafe](#comentarios-safety-en-bloques-unsafe)
3. [Sección # Safety en funciones unsafe](#sección--safety-en-funciones-unsafe)
4. [Documentar invariantes de tipo](#documentar-invariantes-de-tipo)
5. [Documentar invariantes de módulo](#documentar-invariantes-de-módulo)
6. [Niveles de justificación](#niveles-de-justificación)
7. [Patrones de documentación en la librería estándar](#patrones-de-documentación-en-la-librería-estándar)
8. [Clippy y lints para enforcement](#clippy-y-lints-para-enforcement)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Por qué documentar unsafe?

El compilador no puede verificar el código `unsafe`. La documentación es tu **única herramienta** para comunicar por qué el código es correcto — a tu yo futuro, a reviewers, y a cualquiera que mantenga el código.

```
┌──────────────────────────────────────────────────────┐
│       Código safe         │    Código unsafe          │
├───────────────────────────┼──────────────────────────┤
│ ¿Es correcto?             │ ¿Es correcto?            │
│ → Pregúntale al compilador │ → Pregúntale al         │
│                           │   COMENTARIO SAFETY      │
│                           │                          │
│ ¿Quién verifica?          │ ¿Quién verifica?         │
│ → rustc, automáticamente  │ → Humanos, manualmente   │
│                           │                          │
│ ¿Si no hay docs?          │ ¿Si no hay docs?         │
│ → No importa, compila     │ → Nadie sabe si es UB    │
│   correctamente           │   o no                   │
└───────────────────────────┴──────────────────────────┘
```

### El comentario SAFETY responde UNA pregunta

> **"¿Por qué este uso específico de unsafe no causa Undefined Behavior?"**

No es un comentario general. No es una descripción de qué hace el código. Es una **justificación** de por qué las precondiciones de la operación unsafe se cumplen en este punto exacto.

---

## Comentarios SAFETY en bloques unsafe

### Formato estándar

```rust
fn example(data: &[i32], index: usize) -> i32 {
    assert!(index < data.len());

    // SAFETY: `index` es menor que `data.len()` (verificado por el assert arriba),
    // por lo que `data.as_ptr().add(index)` apunta a un elemento válido del slice.
    unsafe { *data.as_ptr().add(index) }
}
```

La convención es:
- Empieza con `// SAFETY:` (mayúsculas, con dos puntos).
- Va **inmediatamente antes** del bloque `unsafe`.
- Explica **por qué** las precondiciones se cumplen, no qué hace el código.

### Múltiples operaciones unsafe en un bloque

Si un bloque tiene varias operaciones unsafe, documenta cada una:

```rust
fn swap_elements(data: &mut [i32], i: usize, j: usize) {
    assert!(i < data.len() && j < data.len());
    assert!(i != j);

    let ptr = data.as_mut_ptr();

    // SAFETY:
    // - `i` y `j` son ambos menores que `data.len()` (assert arriba),
    //   por lo que `ptr.add(i)` y `ptr.add(j)` apuntan a elementos válidos.
    // - `i != j` (assert arriba), por lo que los dos punteros no se solapan,
    //   cumpliendo la precondición de `ptr::swap_nonoverlapping`.
    unsafe {
        std::ptr::swap_nonoverlapping(ptr.add(i), ptr.add(j), 1);
    }
}
```

### Comentario inline cuando hay mezcla safe/unsafe

```rust
fn process_buffer(ptr: *mut u8, len: usize) {
    if ptr.is_null() || len == 0 {
        return;
    }

    // SAFETY: ptr no es nulo (verificado arriba) y apunta a `len` bytes
    // válidos (precondición de esta función, documentada en # Safety).
    let slice = unsafe { std::slice::from_raw_parts_mut(ptr, len) };

    // Operaciones safe sobre el slice
    for byte in slice.iter_mut() {
        *byte = byte.to_ascii_uppercase();
    }

    // SAFETY: acabamos de convertir todos los bytes a ASCII uppercase,
    // que es un subconjunto válido de UTF-8.
    let as_str = unsafe { std::str::from_utf8_unchecked(slice) };

    println!("result: {}", as_str);
}
```

---

## Sección # Safety en funciones unsafe

Las funciones `unsafe fn` deben tener una sección `# Safety` en su documentación que liste las precondiciones que el caller debe cumplir.

### Formato estándar

```rust
/// Copia `count` elementos desde `src` a `dst`.
///
/// # Safety
///
/// - `src` debe apuntar a al menos `count` elementos `T` consecutivos
///   válidos para lectura.
/// - `dst` debe apuntar a al menos `count` elementos `T` consecutivos
///   válidos para escritura.
/// - Las regiones `[src, src+count)` y `[dst, dst+count)` no deben solaparse.
///   Para copias con solapamiento, usar [`copy_overlapping`].
/// - Ambos punteros deben estar alineados a `align_of::<T>()`.
unsafe fn copy_nonoverlapping_custom<T>(src: *const T, dst: *mut T, count: usize) {
    // SAFETY: el caller garantiza todas las precondiciones
    unsafe {
        std::ptr::copy_nonoverlapping(src, dst, count);
    }
}
```

### Cada precondición es concreta y verificable

```rust
// INCORRECTO: vago, no verificable
/// # Safety
///
/// - The pointer must be valid.
/// - Don't mess this up.
unsafe fn bad_docs(ptr: *const i32) -> i32 {
    unsafe { *ptr }
}

// CORRECTO: específico y verificable
/// # Safety
///
/// - `ptr` must point to a properly initialized `i32` value.
/// - `ptr` must be aligned to `align_of::<i32>()` (4 bytes).
/// - The memory pointed to must remain valid for the duration of this call
///   (no concurrent deallocation).
/// - No mutable reference to the same `i32` may exist concurrently
///   (no aliasing violation).
unsafe fn good_docs(ptr: *const i32) -> i32 {
    unsafe { *ptr }
}
```

### Precondiciones comunes y cómo documentarlas

```
┌────────────────────────────────┬──────────────────────────────────┐
│ Precondición                   │ Cómo documentarla                │
├────────────────────────────────┼──────────────────────────────────┤
│ Puntero no nulo                │ "`ptr` must not be null"         │
│ Puntero alineado               │ "`ptr` must be aligned to        │
│                                │  align_of::<T>()"               │
│ Memoria inicializada           │ "`ptr` must point to a properly  │
│                                │  initialized T"                 │
│ Rango válido                   │ "`ptr` must be valid for reads   │
│                                │  of `len` consecutive T values"  │
│ Sin solapamiento               │ "The regions [src..src+n) and    │
│                                │  [dst..dst+n) must not overlap"  │
│ Sin aliasing                   │ "No other references (shared or  │
│                                │  mutable) to this memory may     │
│                                │  exist"                          │
│ Lifetime suficiente            │ "The pointed-to memory must      │
│                                │  remain valid for lifetime 'a"   │
│ Valor dentro de rango          │ "`index` must be less than       │
│                                │  self.len()"                     │
│ String UTF-8                   │ "`bytes` must be valid UTF-8"    │
│ Tipo válido                    │ "The bit pattern must represent   │
│                                │  a valid T"                      │
└────────────────────────────────┴──────────────────────────────────┘
```

---

## Documentar invariantes de tipo

Los invariantes de tipo son propiedades que siempre son verdad para cualquier instancia del tipo. Se documentan en la documentación del struct/enum.

### Formato

```rust
/// Un buffer circular de bytes con capacidad fija.
///
/// # Invariants
///
/// - `ptr` apunta a exactamente `cap` bytes de memoria asignada
///   con el allocator global, o es `NonNull::dangling()` si `cap == 0`.
/// - `len <= cap`.
/// - `head < cap` (o `head == 0` si `cap == 0`).
/// - `tail < cap` (o `tail == 0` si `cap == 0`).
/// - Los bytes en posiciones `tail..tail+len` (modular) están inicializados.
struct RingBuffer {
    ptr: *mut u8,
    cap: usize,
    head: usize,
    tail: usize,
    len: usize,
}
```

### Invariantes explícitos en el código con debug_assert

```rust
impl RingBuffer {
    /// Verifica los invariantes del tipo.
    /// Solo se ejecuta en modo debug.
    fn check_invariants(&self) {
        debug_assert!(self.len <= self.cap);
        if self.cap > 0 {
            debug_assert!(self.head < self.cap);
            debug_assert!(self.tail < self.cap);
            debug_assert!(!self.ptr.is_null());
        }
    }

    pub fn push(&mut self, byte: u8) -> bool {
        if self.len == self.cap {
            return false;
        }

        // SAFETY: len < cap (verificado arriba) y head < cap (invariante),
        // por lo que ptr.add(head) está dentro de la memoria asignada.
        unsafe { *self.ptr.add(self.head) = byte; }

        self.head = (self.head + 1) % self.cap;
        self.len += 1;

        self.check_invariants(); // verificar en debug builds
        true
    }

    pub fn pop(&mut self) -> Option<u8> {
        if self.len == 0 {
            return None;
        }

        // SAFETY: len > 0 (verificado arriba) y tail < cap (invariante),
        // por lo que ptr.add(tail) apunta a un byte inicializado.
        let byte = unsafe { *self.ptr.add(self.tail) };

        self.tail = (self.tail + 1) % self.cap;
        self.len -= 1;

        self.check_invariants();
        Some(byte)
    }
}
```

### Invariantes y constructores

El constructor es donde se **establecen** los invariantes. Los métodos los **mantienen**:

```rust
/// Un vector ordenado. Los elementos siempre están en orden ascendente.
///
/// # Invariants
///
/// - `data` está ordenado: `data[i] <= data[i+1]` para todo `i`.
/// - Esto se establece en `new()` (vec vacío = trivialmente ordenado)
///   y se mantiene en `insert()` (inserción en posición correcta).
struct SortedVec<T: Ord> {
    data: Vec<T>,
}

impl<T: Ord> SortedVec<T> {
    /// Crea un SortedVec vacío. Un vec vacío cumple el invariante trivialmente.
    fn new() -> Self {
        SortedVec { data: Vec::new() }
    }

    /// Inserta manteniendo el orden.
    fn insert(&mut self, value: T) {
        let pos = self.data.binary_search(&value).unwrap_or_else(|x| x);
        self.data.insert(pos, value);
        // INVARIANTE MANTENIDO: binary_search encontró la posición correcta
        debug_assert!(self.data.windows(2).all(|w| w[0] <= w[1]));
    }

    /// Busca eficientemente gracias al invariante de orden.
    fn contains(&self, value: &T) -> bool {
        // Podemos usar binary_search PORQUE el invariante garantiza orden
        self.data.binary_search(value).is_ok()
    }
}
```

---

## Documentar invariantes de módulo

Cuando un módulo entero tiene un invariante que varias funciones mantienen, documéntalo a nivel de módulo:

```rust
/// Módulo que gestiona un pool de memoria.
///
/// # Module Invariants
///
/// - `POOL` es inicializado exactamente una vez por `init()`.
/// - Después de `init()`, `POOL` apunta a `POOL_SIZE` bytes válidos.
/// - `OFFSET` siempre es `<= POOL_SIZE`.
/// - Toda memoria asignada por `alloc()` está dentro de `[POOL, POOL+POOL_SIZE)`.
/// - `alloc()` nunca retorna regiones solapadas (bump allocator).
/// - `reset()` invalida todas las asignaciones previas — los callers
///   no deben usar punteros retornados por `alloc()` después de `reset()`.
mod pool {
    use std::sync::atomic::{AtomicUsize, AtomicPtr, Ordering};

    const POOL_SIZE: usize = 64 * 1024; // 64 KB

    static POOL: AtomicPtr<u8> = AtomicPtr::new(std::ptr::null_mut());
    static OFFSET: AtomicUsize = AtomicUsize::new(0);

    pub fn init() {
        let layout = std::alloc::Layout::array::<u8>(POOL_SIZE).unwrap();
        // SAFETY: layout.size() > 0
        let ptr = unsafe { std::alloc::alloc_zeroed(layout) };
        if ptr.is_null() {
            std::alloc::handle_alloc_error(layout);
        }
        POOL.store(ptr, Ordering::Release);
        // INVARIANTE ESTABLECIDO: POOL apunta a POOL_SIZE bytes válidos
    }

    pub fn alloc(size: usize) -> Option<*mut u8> {
        let pool = POOL.load(Ordering::Acquire);
        if pool.is_null() {
            return None; // no inicializado
        }

        let offset = OFFSET.fetch_add(size, Ordering::Relaxed);
        if offset + size > POOL_SIZE {
            OFFSET.fetch_sub(size, Ordering::Relaxed); // revertir
            return None; // sin espacio
        }

        // SAFETY: pool apunta a POOL_SIZE bytes (invariante de init()),
        // offset + size <= POOL_SIZE (verificado arriba),
        // por lo que pool.add(offset) está dentro del rango válido.
        Some(unsafe { pool.add(offset) })
    }

    pub fn reset() {
        OFFSET.store(0, Ordering::Relaxed);
        // ADVERTENCIA: invalida todas las asignaciones previas
    }
}
```

---

## Niveles de justificación

No todo `unsafe` necesita la misma profundidad de documentación. Ajusta según la complejidad:

### Nivel 1: Trivial (una línea)

Cuando la justificación es obvia e inmediata:

```rust
fn abs_value(x: i32) -> u32 {
    // SAFETY: abs() de i32 siempre retorna un valor no negativo que cabe en u32
    unsafe { x.abs() as u32 }
    // Nota: esto en realidad no necesita unsafe, es solo un ejemplo del formato
}

fn main() {
    let x = 42;
    let ptr: *const i32 = &x;
    // SAFETY: ptr apunta a x que está vivo en este scope
    let val = unsafe { *ptr };
}
```

### Nivel 2: Estándar (2-4 líneas)

La mayoría de los casos. Referencia la verificación previa:

```rust
fn first_byte(data: &[u8]) -> Option<u8> {
    if data.is_empty() {
        return None;
    }

    // SAFETY: data no está vacío (verificado arriba), por lo que
    // data.as_ptr() apunta a al menos 1 byte válido. No usamos
    // .add() por lo que la alineación de u8 (1) siempre se cumple.
    Some(unsafe { *data.as_ptr() })
}
```

### Nivel 3: Detallado (párrafo completo)

Para invariantes complejos o razonamiento no obvio:

```rust
impl<T> Vec<T> {
    fn truncate_custom(&mut self, new_len: usize) {
        if new_len >= self.len() {
            return;
        }

        let old_len = self.len();

        // SAFETY: Debemos dropear los elementos en [new_len, old_len) y
        // luego actualizar len. El orden importa por panic safety:
        //
        // 1. Primero reducimos len a new_len. Si un Drop impl paniquea,
        //    los elementos ya dropeados no se volverán a dropear porque
        //    len ya fue reducido.
        //
        // 2. Luego dropeamos los elementos [new_len, old_len).
        //    ptr.add(new_len) es válido porque new_len < old_len <= capacity,
        //    y from_raw_parts_mut es correcto porque los (old_len - new_len)
        //    elementos a partir de new_len están inicializados (eran parte
        //    del Vec antes de truncar).
        //
        // 3. Si un Drop paniquea en el paso 2, los elementos restantes
        //    (después del que paniquea) se pierden (leak), pero no se
        //    hace double-free porque len ya fue actualizado en el paso 1.
        unsafe {
            let ptr = self.as_mut_ptr();
            self.set_len(new_len);  // paso 1

            let remaining = std::slice::from_raw_parts_mut(
                ptr.add(new_len),
                old_len - new_len,
            );
            std::ptr::drop_in_place(remaining);  // paso 2
        }
    }
}
```

### Cuándo usar cada nivel

```
┌───────────────────────────────────────────────────────┐
│  Nivel 1 (trivial)                                    │
│  - La precondición se verifica 1-2 líneas arriba     │
│  - La operación es un patrón conocido                │
│  - No hay riesgo de aliasing o lifetime               │
│                                                       │
│  Nivel 2 (estándar)                                   │
│  - Múltiples precondiciones verificadas en distintos  │
│    puntos                                             │
│  - La justificación requiere referir un invariante    │
│  - La operación involucra aritmética de punteros      │
│                                                       │
│  Nivel 3 (detallado)                                  │
│  - Panic safety está involucrado                      │
│  - El razonamiento depende del orden de operaciones   │
│  - Hay interacción entre múltiples invariantes        │
│  - Aliasing complejo o lifetimes no obvios            │
│  - El código será difícil de entender en 6 meses      │
│                                                       │
└───────────────────────────────────────────────────────┘
```

---

## Patrones de documentación en la librería estándar

### Vec::set_len

```rust
// De la documentación real de Vec::set_len:

/// # Safety
///
/// - `new_len` must be less than or equal to [`capacity()`].
/// - The elements at `old_len..new_len` must be initialized.
///
/// [`capacity()`]: Vec::capacity
pub unsafe fn set_len(&mut self, new_len: usize) { /* ... */ }
```

### slice::from_raw_parts

```rust
// De la documentación real:

/// # Safety
///
/// Behavior is undefined if any of the following conditions are violated:
///
/// * `data` must be [valid] for reads for `len * mem::size_of::<T>()` many bytes,
///   and it must be properly aligned.
///
/// * `data` must point to `len` consecutive properly initialized values of type `T`.
///
/// * The memory referenced by the returned slice must not be mutated for the duration
///   of lifetime `'a`, except inside an `UnsafeCell`.
///
/// * The total size `len * mem::size_of::<T>()` must be no larger than `isize::MAX`.
pub unsafe fn from_raw_parts<'a, T>(data: *const T, len: usize) -> &'a [T] { /* ... */ }
```

### String::from_utf8_unchecked

```rust
// De la documentación real:

/// # Safety
///
/// This function is unsafe because it does not check that the bytes passed
/// to it are valid UTF-8. If this constraint is violated, it may cause
/// memory unsafety issues with future users of the `String`, as the rest
/// of the standard library assumes that `String`s are valid UTF-8.
pub unsafe fn from_utf8_unchecked(bytes: Vec<u8>) -> String { /* ... */ }
```

### Patrón observable: estructura consistente

```
┌──────────────────────────────────────────────────────┐
│  Patrón de la std:                                   │
│                                                      │
│  /// # Safety                                        │
│  ///                                                 │
│  /// Behavior is undefined if any of the following   │
│  /// conditions are violated:                        │
│  ///                                                 │
│  /// * Condición 1                                   │
│  /// * Condición 2                                   │
│  /// * Condición 3                                   │
│                                                      │
│  Características:                                    │
│  - Cada condición es un bullet point                 │
│  - Usa "must" para obligaciones del caller           │
│  - Usa "must not" para prohibiciones                 │
│  - Referencia a otros items con [`links`]            │
│  - Explica la consecuencia si se viola               │
│                                                      │
└──────────────────────────────────────────────────────┘
```

---

## Clippy y lints para enforcement

### undocumented_unsafe_blocks

```rust
#![warn(clippy::undocumented_unsafe_blocks)]

fn example() {
    let x = 42;
    let ptr: *const i32 = &x;

    // WARNING: unsafe block missing a safety comment
    let _bad = unsafe { *ptr };

    // OK: tiene comentario SAFETY
    // SAFETY: ptr apunta a x que está vivo en este scope
    let _good = unsafe { *ptr };
}
```

### unsafe_op_in_unsafe_fn

```rust
#![warn(unsafe_op_in_unsafe_fn)]

// Sin el lint: el cuerpo de unsafe fn es implícitamente unsafe
// Con el lint: debes usar bloques unsafe explícitos

unsafe fn example(ptr: *const i32) -> i32 {
    // WARNING: unsafe operation in unsafe fn body
    // *ptr

    // OK: bloque unsafe explícito con justificación
    // SAFETY: el caller garantiza que ptr es válido (ver # Safety)
    unsafe { *ptr }
}
```

### missing_safety_doc

```rust
#![warn(clippy::missing_safety_doc)]

// WARNING: unsafe function missing # Safety section
// unsafe fn undocumented(ptr: *const i32) -> i32 {
//     unsafe { *ptr }
// }

/// Reads a value from the pointer.
///
/// # Safety
///
/// - `ptr` must point to a properly initialized `i32`.
/// - `ptr` must be properly aligned.
unsafe fn documented(ptr: *const i32) -> i32 {
    // SAFETY: el caller cumple las precondiciones documentadas
    unsafe { *ptr }
}
```

### Configurar todos los lints juntos

```rust
// En lib.rs o main.rs — configuración recomendada para proyectos con unsafe:
#![warn(unsafe_op_in_unsafe_fn)]
#![warn(clippy::undocumented_unsafe_blocks)]
#![warn(clippy::missing_safety_doc)]
#![warn(clippy::multiple_unsafe_ops_per_block)]
```

El lint `multiple_unsafe_ops_per_block` (clippy, nightly) exige que cada bloque unsafe contenga como máximo una operación unsafe, forzando documentación granular:

```rust
#![warn(clippy::multiple_unsafe_ops_per_block)]

fn example(a: *const i32, b: *const i32) -> i32 {
    // WARNING: multiple unsafe operations in this block
    // unsafe { *a + *b }

    // OK: un bloque por operación
    // SAFETY: a apunta a i32 válido (precondición del caller)
    let va = unsafe { *a };
    // SAFETY: b apunta a i32 válido (precondición del caller)
    let vb = unsafe { *b };
    va + vb
}
```

---

## Errores comunes

### 1. Comentario SAFETY que describe QUÉ en vez de POR QUÉ

```rust
fn example(ptr: *const i32) -> i32 {
    // INCORRECTO: describe qué hace, no por qué es safe
    // SAFETY: dereferencing a raw pointer
    unsafe { *ptr }

    // INCORRECTO: repite la operación en prosa
    // SAFETY: we read the value pointed to by ptr
    unsafe { *ptr }

    // CORRECTO: explica POR QUÉ es safe
    // SAFETY: ptr proviene de &value en el caller (línea 10),
    // value sigue vivo, y nadie más tiene &mut a ese dato
    unsafe { *ptr }
}
```

### 2. Comentario SAFETY genérico reutilizado sin pensar

```rust
// INCORRECTO: mismo comentario copipegado en 5 funciones diferentes
// SAFETY: pointer is valid
unsafe { *ptr_a }

// SAFETY: pointer is valid
unsafe { *ptr_b }

// ¿Cómo sabes que es válido? ¿Quién lo garantiza? ¿En qué condiciones?

// CORRECTO: cada uno explica su contexto específico
// SAFETY: ptr_a proviene de Vec::as_ptr() y index < vec.len()
unsafe { *ptr_a.add(index) }

// SAFETY: ptr_b fue retornado por libc::malloc (verificado no-nulo),
// y escribimos un i32 válido en esa posición
unsafe { *ptr_b }
```

### 3. No documentar # Safety en funciones unsafe públicas

```rust
// INCORRECTO: función unsafe pública sin documentación
pub unsafe fn transmute_slice<T, U>(slice: &[T]) -> &[U] {
    unsafe {
        std::slice::from_raw_parts(
            slice.as_ptr() as *const U,
            slice.len() * std::mem::size_of::<T>() / std::mem::size_of::<U>(),
        )
    }
}
// El caller no tiene idea de qué garantías debe cumplir

// CORRECTO:
/// Reinterpreta un slice de `T` como un slice de `U`.
///
/// # Safety
///
/// - `T` y `U` deben tener el mismo alineamiento, o `U` debe tener
///   un alineamiento menor o igual al de `T`.
/// - `slice.len() * size_of::<T>()` debe ser divisible por `size_of::<U>()`.
/// - Los bytes del slice deben representar valores `U` válidos.
/// - `T` y `U` no deben contener padding que pueda tener valores
///   indeterminados.
pub unsafe fn transmute_slice_documented<T, U>(slice: &[T]) -> &[U] {
    // ...
    todo!()
}
```

### 4. Invariantes de tipo no documentados

```rust
// INCORRECTO: ¿qué relación tienen estos campos?
struct Pool {
    ptr: *mut u8,
    size: usize,
    used: usize,
}
// ¿used <= size? ¿ptr siempre válido? ¿quién asignó ptr?

// CORRECTO:
/// Pool de memoria con asignación lineal (bump allocator).
///
/// # Invariants
///
/// - `ptr` apunta a `size` bytes asignados con `std::alloc::alloc`,
///   o es nulo si `size == 0`.
/// - `used <= size` siempre.
/// - Los bytes en `[ptr, ptr+used)` pueden estar parcialmente inicializados
///   (depende de lo que el caller escriba).
/// - Los bytes en `[ptr+used, ptr+size)` no están inicializados.
struct DocumentedPool {
    ptr: *mut u8,
    size: usize,
    used: usize,
}
```

### 5. No actualizar comentarios SAFETY cuando el código cambia

```rust
fn get_value(data: &[i32], index: usize) -> i32 {
    // Versión original:
    // assert!(index < data.len());
    // SAFETY: index < data.len() verificado por el assert

    // Alguien removió el assert pero dejó el comentario:
    // SAFETY: index < data.len() verificado por el assert  ← ¡MENTIRA!
    unsafe { *data.as_ptr().add(index) }

    // Ahora el comentario dice que hay un assert, pero NO lo hay.
    // El código tiene UB si index >= data.len().
    // El comentario SAFETY engaña al reviewer.
}
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║       DOCUMENTAR INVARIANTES — REFERENCIA RÁPIDA         ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  BLOQUES unsafe                                           ║
║  ──────────────                                           ║
║  // SAFETY: justificación de POR QUÉ es correcto         ║
║  unsafe { operación }                                     ║
║                                                           ║
║  FUNCIONES unsafe                                         ║
║  ────────────────                                         ║
║  /// # Safety                                             ║
║  ///                                                      ║
║  /// - `ptr` must not be null                             ║
║  /// - `ptr` must point to initialized T                   ║
║  /// - ...                                                ║
║  pub unsafe fn foo(ptr: *const T) { ... }                 ║
║                                                           ║
║  INVARIANTES DE TIPO                                      ║
║  ───────────────────                                      ║
║  /// # Invariants                                         ║
║  ///                                                      ║
║  /// - campo1 relación campo2                             ║
║  /// - campo3 siempre cumple X                            ║
║  struct MyType { ... }                                    ║
║                                                           ║
║  INVARIANTES DE MÓDULO                                    ║
║  ────────────────────                                     ║
║  /// # Module Invariants                                  ║
║  ///                                                      ║
║  /// - Propiedad global 1                                 ║
║  /// - Propiedad global 2                                 ║
║  mod my_module { ... }                                    ║
║                                                           ║
║  NIVELES                                                  ║
║  ───────                                                  ║
║  Trivial:   1 línea, precondición obvia                  ║
║  Estándar:  2-4 líneas, referencia verificaciones        ║
║  Detallado: párrafo, panic safety, orden de operaciones  ║
║                                                           ║
║  LINTS                                                    ║
║  ─────                                                    ║
║  clippy::undocumented_unsafe_blocks                       ║
║  clippy::missing_safety_doc                               ║
║  unsafe_op_in_unsafe_fn                                   ║
║  clippy::multiple_unsafe_ops_per_block                    ║
║                                                           ║
║  REGLAS                                                   ║
║  ──────                                                   ║
║  □ SAFETY explica POR QUÉ, no QUÉ                       ║
║  □ Cada bloque tiene su propia justificación             ║
║  □ Cada unsafe fn pública tiene # Safety                 ║
║  □ Los invariantes de tipo están documentados            ║
║  □ debug_assert! verifica invariantes en tests           ║
║  □ Los comentarios se actualizan cuando el código cambia ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Añadir documentación SAFETY a código existente

El siguiente código funciona correctamente pero no tiene ningún comentario SAFETY. Añade la documentación apropiada a cada bloque `unsafe`, a la sección `# Safety` de la función unsafe, y a los invariantes del tipo:

```rust
struct FixedString {
    buf: *mut u8,
    len: usize,
    cap: usize,
}

impl FixedString {
    fn new(capacity: usize) -> Self {
        let layout = std::alloc::Layout::array::<u8>(capacity).unwrap();
        let buf = unsafe { std::alloc::alloc(layout) };
        if buf.is_null() {
            std::alloc::handle_alloc_error(layout);
        }
        FixedString { buf, len: 0, cap: capacity }
    }

    unsafe fn push_bytes_unchecked(&mut self, bytes: &[u8]) {
        unsafe {
            std::ptr::copy_nonoverlapping(
                bytes.as_ptr(),
                self.buf.add(self.len),
                bytes.len(),
            );
        }
        self.len += bytes.len();
    }

    fn push_str(&mut self, s: &str) -> bool {
        if self.len + s.len() > self.cap {
            return false;
        }
        unsafe { self.push_bytes_unchecked(s.as_bytes()) };
        true
    }

    fn as_str(&self) -> &str {
        let bytes = unsafe { std::slice::from_raw_parts(self.buf, self.len) };
        unsafe { std::str::from_utf8_unchecked(bytes) }
    }

    fn clear(&mut self) {
        self.len = 0;
    }
}

impl Drop for FixedString {
    fn drop(&mut self) {
        let layout = std::alloc::Layout::array::<u8>(self.cap).unwrap();
        unsafe { std::alloc::dealloc(self.buf, layout); }
    }
}
```

Añade:
1. `/// # Invariants` al struct `FixedString`.
2. `/// # Safety` a `push_bytes_unchecked`.
3. `// SAFETY:` a cada bloque unsafe (hay 5 bloques).
4. `debug_assert!` donde sea útil para verificar invariantes.

**Pregunta de reflexión**: `as_str` usa `from_utf8_unchecked`. ¿Qué invariante del tipo garantiza que el contenido es UTF-8 válido? ¿Dónde se establece y mantiene ese invariante?

---

### Ejercicio 2: Documentar un módulo complejo

Dado este módulo de arena allocator, documenta completamente los invariantes y justifica cada uso de unsafe:

```rust
mod arena {
    const BLOCK_SIZE: usize = 4096;

    struct Block {
        data: *mut u8,
        used: usize,
    }

    pub struct Arena {
        blocks: Vec<Block>,
    }

    impl Arena {
        pub fn new() -> Self {
            Arena { blocks: Vec::new() }
        }

        fn alloc_block(&mut self) {
            let layout = std::alloc::Layout::array::<u8>(BLOCK_SIZE).unwrap();
            let data = unsafe { std::alloc::alloc(layout) };
            if data.is_null() {
                std::alloc::handle_alloc_error(layout);
            }
            self.blocks.push(Block { data, used: 0 });
        }

        pub fn alloc(&mut self, size: usize) -> &mut [u8] {
            assert!(size <= BLOCK_SIZE, "allocation too large");

            let needs_block = self.blocks.is_empty()
                || self.blocks.last().unwrap().used + size > BLOCK_SIZE;

            if needs_block {
                self.alloc_block();
            }

            let block = self.blocks.last_mut().unwrap();
            let ptr = unsafe { block.data.add(block.used) };
            block.used += size;

            unsafe { std::slice::from_raw_parts_mut(ptr, size) }
        }

        pub fn reset(&mut self) {
            for block in &mut self.blocks {
                block.used = 0;
            }
        }
    }

    impl Drop for Arena {
        fn drop(&mut self) {
            for block in &self.blocks {
                let layout = std::alloc::Layout::array::<u8>(BLOCK_SIZE).unwrap();
                unsafe { std::alloc::dealloc(block.data, layout); }
            }
        }
    }
}
```

Documenta:
1. Invariantes del módulo (`/// # Module Invariants`).
2. Invariantes de `Block` y `Arena`.
3. `// SAFETY:` para cada bloque unsafe.
4. Advierte sobre el lifetime de los slices retornados por `alloc` vs `reset`.

**Pregunta de reflexión**: `alloc` retorna `&mut [u8]` con lifetime ligado a `&mut self`. ¿Esto protege contra usar un slice después de `reset()`? ¿Qué pasa si llamas `alloc` dos veces — el segundo `&mut self` invalida el primer slice retornado?

---

### Ejercicio 3: Reescribir documentación incorrecta

Los siguientes comentarios SAFETY son todos incorrectos o insuficientes. Para cada uno, identifica el problema y reescribe el comentario correctamente:

```rust
// Caso 1:
fn read_at(ptr: *const u8, offset: usize) -> u8 {
    // SAFETY: safe because we use unsafe
    unsafe { *ptr.add(offset) }
}

// Caso 2:
fn make_string(bytes: Vec<u8>) -> String {
    // SAFETY: the bytes come from a trusted source
    unsafe { String::from_utf8_unchecked(bytes) }
}

// Caso 3:
fn get_element(data: &Vec<i32>, index: usize) -> i32 {
    // SAFETY: index is probably within bounds
    unsafe { *data.as_ptr().add(index) }
}

// Caso 4:
unsafe fn dangerous_function(ptr: *mut i32, val: i32) {
    *ptr = val;
}
// (sin documentación # Safety)

// Caso 5:
static mut COUNTER: u32 = 0;
fn increment() {
    // SAFETY: this is fine
    unsafe { COUNTER += 1; }
}
```

Para cada caso:
1. Explica qué está mal con el comentario actual.
2. Determina si el código es realmente safe o tiene un bug.
3. Reescribe el comentario correctamente (o reescribe el código si es necesario).

**Pregunta de reflexión**: ¿Hay algún caso donde un comentario SAFETY **no pueda** justificar la corrección porque el código es fundamentalmente incorrecto? ¿Cuáles de los 5 casos son imposibles de justificar como safe?
