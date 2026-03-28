# unsafe fn y bloques unsafe

## Índice

1. [El contrato de unsafe](#el-contrato-de-unsafe)
2. [Bloques unsafe](#bloques-unsafe)
3. [Funciones unsafe](#funciones-unsafe)
4. [unsafe fn vs bloque unsafe: diferencia clave](#unsafe-fn-vs-bloque-unsafe-diferencia-clave)
5. [Safety invariants vs validity invariants](#safety-invariants-vs-validity-invariants)
6. [Unsafe como límite de confianza](#unsafe-como-límite-de-confianza)
7. [Patrones de encapsulación](#patrones-de-encapsulación)
8. [unsafe y el compilador: lo que cambia y lo que no](#unsafe-y-el-compilador-lo-que-cambia-y-lo-que-no)
9. [Documentar safety con comentarios SAFETY](#documentar-safety-con-comentarios-safety)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## El contrato de unsafe

`unsafe` en Rust no significa "peligroso" ni "incorrecto". Significa: **"el compilador no puede verificar esta garantía — yo, el programador, la asumo"**.

```
┌──────────────────────────────────────────────────────────┐
│                  El modelo de Rust                        │
├──────────────────────────────────────────────────────────┤
│                                                          │
│   Código safe ──→ El compilador VERIFICA las garantías   │
│                                                          │
│   Código unsafe ─→ El programador PROMETE las garantías  │
│                    El compilador CONFÍA en esa promesa    │
│                                                          │
│   Si la promesa es falsa → Undefined Behavior (UB)       │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Los 5 superpoderes que unsafe desbloquea

| # | Superpoder | Ejemplo |
|---|-----------|---------|
| 1 | Dereferenciar raw pointers | `*ptr` |
| 2 | Llamar funciones `unsafe` | `unsafe_function()` |
| 3 | Acceder/modificar `static mut` | `COUNTER += 1` |
| 4 | Implementar traits `unsafe` | `unsafe impl Send for T` |
| 5 | Acceder a campos de `union` | `my_union.field` |

**Nada más**. `unsafe` no desactiva el borrow checker, no desactiva la verificación de tipos, no desactiva los lifetime checks. Todo lo demás sigue funcionando igual.

---

## Bloques unsafe

Un bloque `unsafe { ... }` habilita los superpoderes **dentro** de sus llaves.

### Sintaxis básica

```rust
fn main() {
    let x = 42;
    let ptr: *const i32 = &x;

    // Sin unsafe: error de compilación
    // let v = *ptr;

    // Con bloque unsafe: compila
    let v = unsafe { *ptr };
    println!("{}", v); // 42
}
```

### Múltiples operaciones en un bloque

```rust
fn main() {
    let mut data = [10, 20, 30, 40, 50];
    let ptr = data.as_mut_ptr();

    unsafe {
        *ptr = 100;
        *ptr.add(2) = 300;
        *ptr.add(4) = 500;
    }

    println!("{:?}", data); // [100, 20, 300, 40, 500]
}
```

### El bloque unsafe como expresión

```rust
fn main() {
    let data = [1, 2, 3];
    let ptr = data.as_ptr();

    // unsafe como expresión que retorna un valor
    let sum: i32 = unsafe {
        *ptr + *ptr.add(1) + *ptr.add(2)
    };

    println!("sum: {}", sum); // sum: 6
}
```

### Scope mínimo: buena práctica

```rust
fn process_buffer(ptr: *const u8, len: usize) -> Vec<u8> {
    // Validación safe ANTES del bloque unsafe
    assert!(!ptr.is_null(), "null pointer");
    assert!(len <= 10_000, "buffer too large");

    // Solo lo estrictamente necesario es unsafe
    let slice = unsafe {
        std::slice::from_raw_parts(ptr, len)
    };

    // Procesamiento safe DESPUÉS
    slice.iter()
        .filter(|&&b| b != 0)
        .copied()
        .collect()
}
```

> **Predicción**: ¿Compila `unsafe { let x = 5; }`? Sí. No hay nada unsafe dentro, pero compila — el compilador emitirá un warning `unused_unsafe` porque el bloque no contiene operaciones que requieran unsafe.

---

## Funciones unsafe

Una función marcada `unsafe fn` indica que **llamarla** requiere cumplir precondiciones que el compilador no puede verificar.

### Declarar y llamar

```rust
/// Suma dos punteros. El caller debe garantizar que ambos son válidos.
unsafe fn add_ptrs(a: *const i32, b: *const i32) -> i32 {
    *a + *b
}

fn main() {
    let x = 10;
    let y = 20;

    // Llamar unsafe fn requiere bloque unsafe
    let result = unsafe { add_ptrs(&x, &y) };
    println!("{}", result); // 30
}
```

### El cuerpo de unsafe fn

A partir de Rust 2024 edition, el cuerpo de `unsafe fn` **no** es automáticamente un contexto unsafe. Debes usar bloques `unsafe` explícitos dentro:

```rust
// Rust 2024+: unsafe fn NO implica contexto unsafe en el cuerpo
unsafe fn process(ptr: *const i32) -> i32 {
    // Necesitas bloque unsafe explícito para dereferenciar
    unsafe { *ptr }
}

// En editions anteriores (2015/2018/2021), el cuerpo entero era unsafe implícito:
// unsafe fn process_old(ptr: *const i32) -> i32 {
//     *ptr  // esto compilaba sin bloque unsafe explícito
// }
```

Para prepararte para 2024 edition, puedes usar el lint:

```rust
#![warn(unsafe_op_in_unsafe_fn)]

unsafe fn example(ptr: *const i32) -> i32 {
    // Warning: unsafe operation in unsafe fn body
    // *ptr

    // Correcto: bloque unsafe explícito
    unsafe { *ptr }
}
```

### Precondiciones: el contrato del caller

```rust
/// Devuelve el elemento en `index` sin verificar bounds.
///
/// # Safety
///
/// - `index` debe ser menor que `slice.len()`
unsafe fn get_unchecked(slice: &[i32], index: usize) -> i32 {
    // SAFETY: el caller garantiza que index < slice.len()
    unsafe { *slice.as_ptr().add(index) }
}

fn main() {
    let data = [10, 20, 30];

    // El caller cumple la precondición
    let val = unsafe { get_unchecked(&data, 1) };
    println!("{}", val); // 20

    // Violar la precondición → UB
    // let bad = unsafe { get_unchecked(&data, 100) }; // UB!
}
```

---

## unsafe fn vs bloque unsafe: diferencia clave

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  unsafe { ... }     "Yo, el autor de este bloque,            │
│  (bloque)            GARANTIZO que las operaciones            │
│                      dentro son correctas."                   │
│                                                              │
│  unsafe fn foo()    "Yo, el CALLER de esta función,          │
│  (función)           debo GARANTIZAR las precondiciones       │
│                      documentadas."                           │
│                                                              │
│  BLOQUE: la responsabilidad es del AUTOR del bloque          │
│  FUNCIÓN: la responsabilidad se transfiere al CALLER         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Ejemplo comparativo

```rust
// Opción A: función SAFE con bloque unsafe interno
// → La responsabilidad es MÍA (el autor)
fn safe_get(slice: &[i32], index: usize) -> Option<i32> {
    if index < slice.len() {
        // SAFETY: acabo de verificar que index < len
        Some(unsafe { *slice.as_ptr().add(index) })
    } else {
        None
    }
}

// Opción B: función UNSAFE
// → La responsabilidad es del CALLER
unsafe fn unsafe_get(slice: &[i32], index: usize) -> i32 {
    // SAFETY: el caller garantiza index < slice.len()
    unsafe { *slice.as_ptr().add(index) }
}

fn main() {
    let data = [10, 20, 30];

    // Opción A: sin unsafe, siempre safe
    let a = safe_get(&data, 1);   // Some(20)
    let b = safe_get(&data, 99);  // None (no UB)

    // Opción B: requiere unsafe, el caller asume responsabilidad
    let c = unsafe { unsafe_get(&data, 1) };   // 20
    // unsafe { unsafe_get(&data, 99) };        // UB si se llama así
}
```

### ¿Cuándo hacer una función unsafe?

```
┌─────────────────────────────────────────────────────┐
│  ¿Puedo verificar TODAS las precondiciones          │
│   dentro de la función?                             │
│                                                     │
│   SÍ  → función safe con unsafe block interno       │
│         (mejor: el usuario no puede equivocarse)    │
│                                                     │
│   NO  → función unsafe con documentación # Safety   │
│         (el caller debe leer y cumplir el contrato) │
└─────────────────────────────────────────────────────┘
```

Ejemplo: `Vec::set_len()` es `unsafe fn` porque no puede verificar que los primeros `new_len` elementos estén inicializados — solo el caller lo sabe. En cambio, `Vec::push()` es safe porque verifica internamente (crece el buffer si hace falta).

---

## Safety invariants vs validity invariants

Estos dos conceptos son fundamentales para razonar sobre `unsafe`:

### Validity invariant

Lo que el compilador asume que **siempre** es verdad sobre un tipo. Violarlo es UB instantáneo.

```rust
// Validity invariants de algunos tipos:
// bool: solo puede ser 0x00 o 0x01
// &T:   nunca null, siempre alineado, siempre apunta a T válido
// char: siempre un scalar value Unicode válido (≤ 0x10FFFF, no surrogate)
```

### Safety invariant

Lo que el **código safe** asume que es verdad. Violarlo no es UB inmediato, pero puede causar UB cuando código safe use el valor.

```rust
// Safety invariants de algunos tipos:
// Vec<T>: len ≤ capacity, los primeros len elementos están inicializados
// String: el contenido es UTF-8 válido
// HashMap: los hashes son consistentes con Eq

fn main() {
    // Violar safety invariant de String:
    let bad_string = unsafe {
        String::from_utf8_unchecked(vec![0xFF, 0xFE])
    };
    // No es UB crear bad_string
    // PERO: usarla en código safe que asuma UTF-8 → UB
    // println!("{}", bad_string); // Comportamiento impredecible
}
```

```
┌───────────────────────────────────────────────────────┐
│                                                       │
│  Validity Invariant    Safety Invariant               │
│  ──────────────────    ────────────────                │
│  Bit-level correctness Type-level contract            │
│  Violación = UB        Violación = UB eventual        │
│  inmediato              (cuando safe code confía)     │
│  Compilador asume       Código safe asume             │
│                                                       │
│  Ejemplo: &T no null   Ejemplo: Vec.len ≤ capacity   │
│  Ejemplo: bool es 0/1  Ejemplo: String es UTF-8      │
│                                                       │
│  unsafe code must       unsafe code must              │
│  NEVER violate          maintain for safe callers     │
│                                                       │
└───────────────────────────────────────────────────────┘
```

### Ejemplo: mantener el safety invariant de Vec

```rust
fn main() {
    let mut v: Vec<i32> = Vec::with_capacity(10);

    unsafe {
        // CORRECTO: escribir valores y luego ajustar len
        let ptr = v.as_mut_ptr();
        std::ptr::write(ptr, 1);
        std::ptr::write(ptr.add(1), 2);
        std::ptr::write(ptr.add(2), 3);
        v.set_len(3);  // ahora len=3, y los 3 elementos están inicializados
    }

    // Safe code confía en que v[0..3] están inicializados
    println!("{:?}", v); // [1, 2, 3]

    // INCORRECTO: set_len sin inicializar
    // unsafe { v.set_len(10); }
    // → v[3..10] serían basura → leerlos sería UB
}
```

---

## Unsafe como límite de confianza

### El módulo como boundary de seguridad

La unidad de seguridad en Rust no es la función ni el bloque `unsafe` — es el **módulo**. Todo el código dentro del módulo que contiene `unsafe` puede afectar la corrección del código unsafe.

```rust
mod safe_vec {
    pub struct MyVec {
        ptr: *mut i32,
        len: usize,    // INVARIANTE: len ≤ cap
        cap: usize,    // INVARIANTE: ptr apunta a cap elementos asignados
    }

    impl MyVec {
        pub fn new() -> Self {
            MyVec {
                ptr: std::ptr::null_mut(),
                len: 0,
                cap: 0,
            }
        }

        pub fn push(&mut self, value: i32) {
            if self.len == self.cap {
                self.grow(); // safe fn que mantiene invariantes
            }
            // SAFETY: len < cap después de grow(),
            // ptr apunta a cap elementos válidos
            unsafe {
                std::ptr::write(self.ptr.add(self.len), value);
            }
            self.len += 1;
        }

        pub fn get(&self, index: usize) -> Option<i32> {
            if index < self.len {
                // SAFETY: index < len, y los primeros len elementos
                // están inicializados (invariante del tipo)
                Some(unsafe { std::ptr::read(self.ptr.add(index)) })
            } else {
                None
            }
        }

        fn grow(&mut self) {
            // Implementación que mantiene los invariantes...
            let new_cap = if self.cap == 0 { 4 } else { self.cap * 2 };
            let layout = std::alloc::Layout::array::<i32>(new_cap).unwrap();

            let new_ptr = if self.ptr.is_null() {
                unsafe { std::alloc::alloc(layout) as *mut i32 }
            } else {
                let old_layout = std::alloc::Layout::array::<i32>(self.cap).unwrap();
                unsafe {
                    std::alloc::realloc(
                        self.ptr as *mut u8,
                        old_layout,
                        layout.size(),
                    ) as *mut i32
                }
            };

            if new_ptr.is_null() {
                std::alloc::handle_alloc_error(layout);
            }

            self.ptr = new_ptr;
            self.cap = new_cap;
        }
    }

    impl Drop for MyVec {
        fn drop(&mut self) {
            if !self.ptr.is_null() {
                // SAFETY: ptr fue asignado con alloc, layout coincide con cap
                unsafe {
                    let layout = std::alloc::Layout::array::<i32>(self.cap).unwrap();
                    std::alloc::dealloc(self.ptr as *mut u8, layout);
                }
            }
        }
    }
}
```

**Punto clave**: `len` y `cap` son privados. Si fueran `pub`, código externo podría poner `len = 1000` con `cap = 4`, rompiendo los invariantes. La privacidad de campos es parte de la estrategia de seguridad.

```
┌────────────────────────────────────────────────┐
│         Módulo safe_vec (trust boundary)        │
│                                                │
│  ┌──────────────────────────────────────────┐  │
│  │  campos privados: ptr, len, cap          │  │
│  │  invariantes mantenidos internamente     │  │
│  └──────────────────────────────────────────┘  │
│                                                │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐      │
│  │ push()   │ │ get()    │ │ grow()   │      │
│  │ (safe)   │ │ (safe)   │ │ (safe)   │      │
│  │ unsafe{} │ │ unsafe{} │ │ unsafe{} │      │
│  └──────────┘ └──────────┘ └──────────┘      │
│                                                │
│  Todo unsafe es CORRECTO porque las funciones  │
│  safe mantienen los invariantes de los campos  │
├────────────────────────────────────────────────┤
│         Código externo (no puede romper nada)  │
│  - No puede acceder a ptr/len/cap directamente │
│  - Solo usa la API pública safe                │
└────────────────────────────────────────────────┘
```

---

## Patrones de encapsulación

### Patrón 1: Función safe que envuelve unsafe

El patrón más común. Verificas todo lo necesario y luego usas unsafe internamente:

```rust
/// Convierte un byte a char ASCII.
/// Retorna None si no es ASCII válido.
fn byte_to_ascii(b: u8) -> Option<char> {
    if b.is_ascii() {
        // SAFETY: acabo de verificar que b es ASCII (0..=127),
        // que es un subset de Unicode scalar values
        Some(unsafe { char::from_u32_unchecked(b as u32) })
    } else {
        None
    }
}

fn main() {
    println!("{:?}", byte_to_ascii(65));  // Some('A')
    println!("{:?}", byte_to_ascii(200)); // None
}
```

### Patrón 2: Constructor unsafe, métodos safe

```rust
struct AlignedBuffer {
    ptr: *mut u8,
    len: usize,
    align: usize,
}

impl AlignedBuffer {
    /// # Safety
    ///
    /// - `align` debe ser una potencia de 2
    /// - `len` debe ser > 0
    /// - `len` debe ser múltiplo de `align`
    unsafe fn new(len: usize, align: usize) -> Self {
        let layout = std::alloc::Layout::from_size_align_unchecked(len, align);
        let ptr = std::alloc::alloc_zeroed(layout);
        if ptr.is_null() {
            std::alloc::handle_alloc_error(layout);
        }
        AlignedBuffer { ptr, len, align }
    }

    // Métodos safe: las invariantes del constructor garantizan corrección
    fn as_slice(&self) -> &[u8] {
        // SAFETY: ptr apunta a len bytes válidos (inicializados con ceros)
        unsafe { std::slice::from_raw_parts(self.ptr, self.len) }
    }

    fn as_mut_slice(&mut self) -> &mut [u8] {
        // SAFETY: igual que as_slice, y tenemos &mut self
        unsafe { std::slice::from_raw_parts_mut(self.ptr, self.len) }
    }
}

impl Drop for AlignedBuffer {
    fn drop(&mut self) {
        unsafe {
            let layout = std::alloc::Layout::from_size_align_unchecked(
                self.len, self.align
            );
            std::alloc::dealloc(self.ptr, layout);
        }
    }
}
```

### Patrón 3: Trait unsafe con implementación que promete invariantes

```rust
/// # Safety
///
/// Implementar este trait garantiza que `validate()` retorna true
/// solo para valores que son válidos según las reglas del tipo.
unsafe trait TrustedValidator {
    fn validate(&self) -> bool;
}

struct PositiveNumber(i32);

// SAFETY: validate() retorna true solo si el número es positivo
unsafe impl TrustedValidator for PositiveNumber {
    fn validate(&self) -> bool {
        self.0 > 0
    }
}

// Código que CONFÍA en la implementación
fn process<T: TrustedValidator>(item: &T) {
    if item.validate() {
        // Podemos confiar en que el valor cumple las reglas
        println!("item is valid");
    }
}
```

---

## unsafe y el compilador: lo que cambia y lo que no

### Lo que unsafe NO desactiva

```rust
fn example() {
    unsafe {
        // El borrow checker SIGUE activo para referencias
        let mut x = 5;
        let r1 = &x;
        // let r2 = &mut x; // ERROR: aún con unsafe
        println!("{}", r1);

        // La verificación de tipos SIGUE activa
        let a: i32 = 5;
        // let b: String = a; // ERROR: aún con unsafe

        // Los lifetime checks SIGUEN activos para referencias
        // let dangling: &i32;
        // {
        //     let temp = 42;
        //     dangling = &temp; // ERROR: aún con unsafe
        // }
    }
}
```

### Lo que unsafe SÍ habilita (solo los 5 superpoderes)

```rust
fn example() {
    let x = 42;
    let ptr: *const i32 = &x;

    unsafe {
        // 1. Dereferenciar raw pointers
        let _v = *ptr;

        // 2. Llamar funciones unsafe
        let _s = String::from_utf8_unchecked(vec![72, 105]);

        // 3. Acceder a static mut
        static mut COUNTER: i32 = 0;
        COUNTER += 1;

        // 4 y 5 requieren contexto diferente (impl, union)
    }
}
```

> **Predicción**: ¿Puedes tener dos `&mut` al mismo dato dentro de un bloque `unsafe`? No, no con referencias. El borrow checker sigue activo para `&mut`. Necesitarías usar `*mut` (raw pointers) para lograr acceso mutable múltiple.

---

## Documentar safety con comentarios SAFETY

La convención en el ecosistema Rust es documentar cada uso de `unsafe` con un comentario `SAFETY:` que explique **por qué** es correcto.

### En bloques unsafe

```rust
fn copy_elements(src: &[i32], dst: &mut [i32]) {
    let count = src.len().min(dst.len());

    // SAFETY:
    // - src.as_ptr() es válido para leer `count` elementos (count ≤ src.len())
    // - dst.as_mut_ptr() es válido para escribir `count` elementos (count ≤ dst.len())
    // - src y dst no se solapan (son slices de diferentes orígenes)
    unsafe {
        std::ptr::copy_nonoverlapping(
            src.as_ptr(),
            dst.as_mut_ptr(),
            count,
        );
    }
}
```

### En funciones unsafe (documentación # Safety)

```rust
/// Interpreta un slice de bytes como un slice de `T`.
///
/// # Safety
///
/// - `bytes.len()` debe ser múltiplo de `size_of::<T>()`
/// - `bytes.as_ptr()` debe estar alineado a `align_of::<T>()`
/// - Los bytes deben representar valores `T` válidos
unsafe fn reinterpret_slice<T>(bytes: &[u8]) -> &[T] {
    let count = bytes.len() / std::mem::size_of::<T>();
    // SAFETY: el caller garantiza alineación, tamaño y validez
    std::slice::from_raw_parts(bytes.as_ptr() as *const T, count)
}
```

### Lints de Clippy para unsafe

```rust
// Clippy puede detectar bloques unsafe sin comentario SAFETY
#![warn(clippy::undocumented_unsafe_blocks)]

fn main() {
    let x = 42;
    let ptr: *const i32 = &x;

    // Clippy warning: unsafe block missing a safety comment
    // unsafe { *ptr };

    // SAFETY: ptr apunta a x que está vivo en este scope
    let _ = unsafe { *ptr };
}
```

---

## Errores comunes

### 1. Usar unsafe fn cuando un bloque unsafe basta

```rust
// INCORRECTO: toda la función es unsafe innecesariamente
unsafe fn sum_slice(data: &[i32]) -> i32 {
    let mut sum = 0;
    for &x in data {
        sum += x;
    }
    sum
    // No hay nada unsafe aquí, no necesita ser unsafe fn
}

// CORRECTO: función safe normal
fn sum_slice_safe(data: &[i32]) -> i32 {
    data.iter().sum()
}
```

**Regla**: solo marca una función `unsafe fn` si tiene precondiciones que el caller debe garantizar y que tú no puedes verificar.

### 2. Bloque unsafe demasiado grande

```rust
fn process(data: &[u8]) -> String {
    // INCORRECTO: todo dentro de unsafe
    unsafe {
        let ptr = data.as_ptr();
        let len = data.len();
        let slice = std::slice::from_raw_parts(ptr, len);
        let filtered: Vec<u8> = slice.iter()
            .filter(|&&b| b.is_ascii_alphabetic())
            .copied()
            .collect();
        String::from_utf8_unchecked(filtered)
    }

    // CORRECTO: unsafe minimal
    // let filtered: Vec<u8> = data.iter()
    //     .filter(|&&b| b.is_ascii_alphabetic())
    //     .copied()
    //     .collect();
    // // SAFETY: is_ascii_alphabetic() garantiza que todos los bytes son ASCII,
    // // que es un subconjunto de UTF-8
    // unsafe { String::from_utf8_unchecked(filtered) }
}
```

### 3. Asumir que unsafe desactiva el borrow checker

```rust
fn main() {
    let mut v = vec![1, 2, 3];

    unsafe {
        // Esto NO compila — el borrow checker sigue activo para referencias
        let r1 = &v[0];
        // v.push(4);  // ERROR: cannot borrow v as mutable
        println!("{}", r1);
    }

    // Para mutación simultánea necesitas raw pointers, no unsafe blocks
    let ptr = v.as_mut_ptr();
    unsafe {
        *ptr = 100;          // Modificar primer elemento
        *ptr.add(2) = 300;   // Modificar tercer elemento
    }
    println!("{:?}", v); // [100, 2, 300]
}
```

### 4. No considerar panics dentro de unsafe

```rust
fn risky_operation(data: &mut Vec<i32>) {
    let ptr = data.as_mut_ptr();
    let original_len = data.len();

    unsafe {
        data.set_len(0); // "vaciar" temporalmente

        // Si esta operación paniquea, data está en estado inconsistente:
        // len=0 pero los elementos siguen en memoria → memory leak
        let might_panic = some_function();

        data.set_len(original_len); // restaurar
    }
}

fn some_function() -> i32 {
    // Podría paniquear...
    42
}

// CORRECTO: usar un guard que restaure en Drop
struct LenGuard<'a> {
    vec: &'a mut Vec<i32>,
    original_len: usize,
}

impl<'a> Drop for LenGuard<'a> {
    fn drop(&mut self) {
        // Se ejecuta incluso si hay panic
        unsafe { self.vec.set_len(self.original_len); }
    }
}
```

### 5. Confundir "compila" con "es correcto"

```rust
fn main() {
    let v = vec![1, 2, 3];
    let ptr = v.as_ptr();

    drop(v); // v se libera

    // Esto COMPILA sin problemas:
    unsafe {
        // Pero es UB: use-after-free
        // println!("{}", *ptr);
    }

    // unsafe no añade verificaciones en runtime.
    // Si compila pero viola invariantes → UB silencioso.
}
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║          unsafe fn Y BLOQUES unsafe — REF. RÁPIDA        ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  BLOQUE unsafe                                            ║
║  ──────────────                                           ║
║  unsafe { operación_unsafe }                              ║
║  - Habilita los 5 superpoderes dentro de { }              ║
║  - El AUTOR del bloque garantiza corrección               ║
║  - Mantener lo más pequeño posible                        ║
║                                                           ║
║  FUNCIÓN unsafe                                           ║
║  ──────────────                                           ║
║  unsafe fn foo(ptr: *const T) → ...                       ║
║  - El CALLER garantiza las precondiciones                 ║
║  - Documentar con /// # Safety                            ║
║  - Llamar requiere: unsafe { foo(ptr) }                   ║
║  - 2024 edition: cuerpo NO es unsafe implícito            ║
║                                                           ║
║  CUÁNDO USAR unsafe fn                                    ║
║  ─────────────────────                                    ║
║  ¿Precondiciones no verificables? → unsafe fn             ║
║  ¿Todo verificable internamente?  → fn safe + unsafe {}   ║
║                                                           ║
║  INVARIANTES                                              ║
║  ───────────                                              ║
║  Validity: reglas del tipo (bool=0/1, &T≠null)            ║
║  Safety:   contrato del tipo (Vec.len≤cap, String=UTF8)   ║
║  Violar validity → UB inmediato                           ║
║  Violar safety   → UB cuando safe code use el valor       ║
║                                                           ║
║  TRUST BOUNDARY                                           ║
║  ──────────────                                           ║
║  Unidad de seguridad = módulo (no bloque ni función)      ║
║  Campos privados protegen los invariantes                 ║
║  Código externo solo usa API safe pública                 ║
║                                                           ║
║  DOCUMENTACIÓN                                            ║
║  ─────────────                                            ║
║  // SAFETY: por qué este uso es correcto                  ║
║  #![warn(clippy::undocumented_unsafe_blocks)]             ║
║  #![warn(unsafe_op_in_unsafe_fn)]                         ║
║                                                           ║
║  5 SUPERPODERES (lo único que unsafe habilita)            ║
║  ─────────────────────────────────────────────            ║
║  1. Dereferenciar *const T / *mut T                       ║
║  2. Llamar unsafe fn                                      ║
║  3. Acceder/modificar static mut                          ║
║  4. Implementar unsafe trait                              ║
║  5. Acceder a campos de union                             ║
║                                                           ║
║  Lo que unsafe NO desactiva:                              ║
║  borrow checker, type checking, lifetime checking         ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Safe wrapper sobre unsafe API

Dada esta API "de sistema" simulada:

```rust
mod raw_api {
    use std::ptr;

    static mut REGISTRY: [(*const u8, usize); 64] = [(ptr::null(), 0); 64];
    static mut COUNT: usize = 0;

    /// Registra un nombre. Retorna el ID (índice) o -1 si está lleno.
    ///
    /// # Safety
    /// - `name` debe apuntar a `len` bytes válidos UTF-8
    /// - Los bytes deben permanecer válidos mientras el registro exista
    pub unsafe fn register(name: *const u8, len: usize) -> i32 {
        if COUNT >= 64 { return -1; }
        REGISTRY[COUNT] = (name, len);
        COUNT += 1;
        (COUNT - 1) as i32
    }

    /// Obtiene el nombre registrado en `id`.
    ///
    /// # Safety
    /// - `id` debe ser un valor retornado previamente por `register`
    /// - `out_len` no debe ser nulo
    pub unsafe fn lookup(id: i32, out_len: *mut usize) -> *const u8 {
        let idx = id as usize;
        if idx >= COUNT {
            return ptr::null();
        }
        *out_len = REGISTRY[idx].1;
        REGISTRY[idx].0
    }

    pub unsafe fn count() -> usize {
        COUNT
    }

    pub unsafe fn clear() {
        COUNT = 0;
    }
}
```

Crea un wrapper safe `Registry` con:
- `new()` que limpie el registro.
- `register(&self, name: &str) -> Result<u32, RegistryError>`.
- `lookup(&self, id: u32) -> Option<&str>`.
- `count(&self) -> usize`.

**Pistas**:
- `register`: el problema es que `name.as_ptr()` apunta a memoria del caller. Almacena los nombres en un `Vec<String>` propio para que los punteros sean estables (¡cuidado! `Vec` puede reubicar al crecer — usa `Pin` o `Box<str>` para estabilidad).
- `lookup`: usa `std::str::from_utf8_unchecked` y `std::slice::from_raw_parts`.
- Documenta con `// SAFETY:` cada bloque unsafe.

**Pregunta de reflexión**: ¿Por qué no podemos pasar `name.as_ptr()` directamente a `raw_api::register` y asumir que el caller mantiene vivo el `&str`? ¿Qué ownership problem surge?

---

### Ejercicio 2: Contenedor con invariante custom

Implementa `SortedVec<T: Ord>`: un vector que mantiene sus elementos siempre ordenados. Usa `unsafe` internamente para optimización (inserción binaria + memmove), pero expón solo API safe:

```rust
struct SortedVec<T: Ord> {
    data: Vec<T>,
}

impl<T: Ord> SortedVec<T> {
    fn new() -> Self { todo!() }

    fn insert(&mut self, value: T) {
        // Usa binary_search para encontrar posición
        // Usa ptr::copy para mover elementos (como memmove)
        // en lugar de Vec::insert (que hace lo mismo pero queremos practicar)
        todo!()
    }

    fn contains(&self, value: &T) -> bool { todo!() }
    fn get(&self, index: usize) -> Option<&T> { todo!() }
    fn len(&self) -> usize { todo!() }

    /// Retorna un slice — safe porque el invariant (ordenado) se mantiene
    fn as_slice(&self) -> &[T] { todo!() }
}
```

**Pistas**:
- `insert`: después de `binary_search`, haz espacio con `Vec::reserve(1)` + `set_len(len+1)`, luego `ptr::copy(src, dst, count)` para mover elementos a la derecha, y `ptr::write` para colocar el nuevo elemento.
- El safety invariant es "siempre ordenado". El unsafe solo optimiza el insert.
- Verifica el invariante con `debug_assert!(self.data.windows(2).all(|w| w[0] <= w[1]))`.

**Pregunta de reflexión**: ¿Qué pasaría si `insert` paniquease entre `set_len` y `ptr::write`? ¿Cómo protegerías contra ese escenario? (Pista: guard pattern con `Drop`).

---

### Ejercicio 3: Auditoría de unsafe

Dado el siguiente código, identifica todos los problemas de seguridad y corrígelos. Por cada bloque `unsafe`, escribe el comentario `// SAFETY:` apropiado o explica por qué el uso es incorrecto:

```rust
use std::ptr;

struct Pool {
    buffer: *mut u8,
    size: usize,
    used: usize,
}

impl Pool {
    fn new(size: usize) -> Self {
        let buffer = unsafe {
            std::alloc::alloc(
                std::alloc::Layout::from_size_align(size, 1).unwrap()
            )
        };
        Pool { buffer, size, used: 0 }
    }

    unsafe fn alloc(&mut self, bytes: usize) -> *mut u8 {
        let ptr = self.buffer.add(self.used);
        self.used += bytes;
        ptr
    }

    fn alloc_safe(&mut self, bytes: usize) -> Option<&mut [u8]> {
        if self.used + bytes > self.size {
            return None;
        }
        unsafe {
            let ptr = self.alloc(bytes);
            Some(std::slice::from_raw_parts_mut(ptr, bytes))
        }
    }

    fn reset(&mut self) {
        self.used = 0;
    }
}

impl Drop for Pool {
    fn drop(&mut self) {
        unsafe {
            std::alloc::dealloc(
                self.buffer,
                std::alloc::Layout::from_size_align(self.size, 1).unwrap()
            );
        }
    }
}

fn main() {
    let mut pool = Pool::new(1024);

    let a = pool.alloc_safe(100).unwrap();
    a[0] = 42;

    let b = pool.alloc_safe(200).unwrap();
    b[0] = 99;

    // ¿Es esto safe? ¿Qué pasa si usamos 'a' después de obtener 'b'?
    // a[1] = 10; // ← ¿compila? ¿es correcto?

    pool.reset();
}
```

Identifica al menos 4 problemas. Corrige el código y documenta cada bloque `unsafe`.

**Pregunta de reflexión**: `alloc_safe` retorna `&mut [u8]` con lifetime ligado a `&mut self`. ¿Esto previene obtener dos slices simultáneos? ¿Es suficiente para garantizar seguridad, o hay escenarios donde falla?
