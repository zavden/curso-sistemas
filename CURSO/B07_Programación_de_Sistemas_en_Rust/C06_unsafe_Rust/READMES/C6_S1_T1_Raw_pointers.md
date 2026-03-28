# Raw Pointers en Rust

## Índice

1. [¿Qué son los raw pointers?](#qué-son-los-raw-pointers)
2. [Crear raw pointers (es seguro)](#crear-raw-pointers-es-seguro)
3. [Dereferenciar raw pointers (es unsafe)](#dereferenciar-raw-pointers-es-unsafe)
4. [Aritmética de punteros](#aritmética-de-punteros)
5. [Conversiones entre punteros y referencias](#conversiones-entre-punteros-y-referencias)
6. [Punteros y colecciones: slice desde raw pointer](#punteros-y-colecciones-slice-desde-raw-pointer)
7. [Patrones comunes con raw pointers](#patrones-comunes-con-raw-pointers)
8. [Punteros nulos y comprobaciones](#punteros-nulos-y-comprobaciones)
9. [Raw pointers y el sistema de tipos](#raw-pointers-y-el-sistema-de-tipos)
10. [Errores comunes](#errores-comunes)
11. [Cheatsheet](#cheatsheet)
12. [Ejercicios](#ejercicios)

---

## ¿Qué son los raw pointers?

Rust tiene dos tipos de raw pointers:

| Tipo | Significado | Equivalente en C |
|------|-------------|------------------|
| `*const T` | Puntero inmutable a `T` | `const T*` |
| `*mut T` | Puntero mutable a `T` | `T*` |

A diferencia de las referencias (`&T`, `&mut T`), los raw pointers:

- **No** tienen garantías de lifetime.
- **No** se verifican por el borrow checker.
- **Pueden** ser nulos.
- **Pueden** apuntar a memoria inválida.
- **No** implementan auto-cleanup.

```
┌─────────────────────────────────────────────────────────┐
│              Sistema de punteros en Rust                │
├──────────────────────┬──────────────────────────────────┤
│   Safe (referencias) │    Unsafe (raw pointers)         │
├──────────────────────┼──────────────────────────────────┤
│ &T       (shared)    │ *const T  (lectura, sin check)   │
│ &mut T   (exclusive) │ *mut T    (lectura/escritura)     │
├──────────────────────┼──────────────────────────────────┤
│ Borrow checker: SÍ   │ Borrow checker: NO              │
│ Null: IMPOSIBLE       │ Null: POSIBLE                   │
│ Alineación: GARANTIZ. │ Alineación: NO GARANTIZADA      │
│ Lifetime: VERIFICADO  │ Lifetime: IGNORADO              │
└──────────────────────┴──────────────────────────────────┘
```

### ¿Cuándo necesitas raw pointers?

1. **FFI**: interactuar con código C que usa punteros.
2. **Estructuras autoreferenciadas**: cuando el borrow checker no puede expresar la relación.
3. **Optimización de bajo nivel**: acceso directo a memoria sin overhead de bounds checking.
4. **Implementar abstracciones unsafe**: como `Vec<T>`, `Box<T>`, smart pointers custom.

---

## Crear raw pointers (es seguro)

Crear un raw pointer es perfectamente seguro. Es **desreferenciar** lo que requiere `unsafe`.

### Desde referencias

```rust
fn main() {
    let value = 42;
    let r: &i32 = &value;

    // Crear raw pointer desde referencia — safe
    let ptr: *const i32 = r as *const i32;
    // Equivalente más corto:
    let ptr: *const i32 = &value;

    println!("ptr address: {:p}", ptr);
    // ptr address: 0x7ffd4a3b1c04 (ejemplo)
}
```

### Desde referencias mutables

```rust
fn main() {
    let mut value = 42;

    let ptr_mut: *mut i32 = &mut value as *mut i32;
    // Equivalente más corto:
    let ptr_mut: *mut i32 = &mut value;

    println!("ptr_mut address: {:p}", ptr_mut);
}
```

### Desde direcciones arbitrarias

```rust
fn main() {
    // Crear puntero a una dirección arbitraria — safe (no lo dereferencies)
    let arbitrary: *const i32 = 0xDEADBEEF as *const i32;

    println!("address: {:p}", arbitrary);
    // Esto compila y ejecuta sin problemas
    // Dereferenciar esto sería UB (undefined behavior)
}
```

### Con std::ptr

```rust
use std::ptr;

fn main() {
    // Puntero nulo
    let null_ptr: *const i32 = ptr::null();
    let null_mut_ptr: *mut i32 = ptr::null_mut();

    println!("is null: {}", null_ptr.is_null()); // true

    // Desde referencia (sin cast explícito)
    let x = 10;
    let ptr = ptr::addr_of!(x);  // *const i32
    println!("via addr_of!: {:p}", ptr);

    let mut y = 20;
    let ptr_mut = ptr::addr_of_mut!(y);  // *mut i32
    println!("via addr_of_mut!: {:p}", ptr_mut);
}
```

> **Predicción**: ¿Qué imprime `ptr::null::<i32>().is_null()`?
> Imprime `true`. `null()` devuelve un puntero cuya dirección es `0x0`.

### addr_of! vs cast de referencia

`addr_of!` es preferible cuando necesitas un puntero a un campo de un struct con campos no alineados o que no implementan `Unpin`:

```rust
#[repr(C, packed)]
struct Packed {
    a: u8,
    b: u32, // no alineado a 4 bytes
}

fn main() {
    let p = Packed { a: 1, b: 2 };

    // ERROR: &p.b crearía una referencia desalineada
    // let ptr = &p.b as *const u32;

    // CORRECTO: addr_of! no crea referencia intermedia
    let ptr = std::ptr::addr_of!(p.b);

    unsafe {
        // Lectura desalineada segura
        let val = ptr.read_unaligned();
        println!("b = {}", val); // b = 2
    }
}
```

---

## Dereferenciar raw pointers (es unsafe)

Aquí es donde entras en territorio `unsafe`. Dereferenciar un raw pointer le dice al compilador: "yo garantizo que este puntero es válido, alineado y que no viola aliasing".

### Lectura básica

```rust
fn main() {
    let value = 42;
    let ptr: *const i32 = &value;

    // UNSAFE: dereferenciar raw pointer
    let read_value = unsafe { *ptr };
    println!("value: {}", read_value); // value: 42
}
```

### Escritura

```rust
fn main() {
    let mut value = 42;
    let ptr: *mut i32 = &mut value;

    unsafe {
        *ptr = 100;
    }

    println!("value: {}", value); // value: 100
}
```

### Lo que debes garantizar al dereferenciar

```
┌──────────────────────────────────────────────────────┐
│   Invariantes que TÚ garantizas al usar unsafe       │
├──────────────────────────────────────────────────────┤
│ 1. El puntero NO es nulo                             │
│ 2. El puntero apunta a memoria válida (inicializada) │
│ 3. El puntero está correctamente alineado            │
│ 4. No hay aliasing inválido:                         │
│    - Si *mut T: nadie más lee/escribe ese dato       │
│    - Si *const T: nadie escribe ese dato             │
│ 5. El dato al que apunta es un T válido              │
└──────────────────────────────────────────────────────┘
```

### Ejemplo: violación de invariantes (¡NO hacer esto!)

```rust
fn main() {
    let ptr: *const i32 = std::ptr::null();

    // UB: dereferenciar puntero nulo
    // unsafe { println!("{}", *ptr); }

    let local = 42;
    let dangling: *const i32 = &local as *const i32;
    // Si local saliera de scope (por ejemplo, retornándolo de una función),
    // dereferenciar dangling sería use-after-free
}

// UB: puntero colgante (dangling pointer)
fn bad_pointer() -> *const i32 {
    let local = 42;
    &local as *const i32  // local se destruye al terminar la función
    // El puntero retornado apunta a memoria de stack ya liberada
}
```

---

## Aritmética de punteros

Como en C, puedes hacer aritmética con raw pointers. Rust usa el método `offset()` (y variantes) en lugar de `ptr + n`.

### offset y add/sub

```rust
fn main() {
    let numbers = [10, 20, 30, 40, 50];
    let ptr: *const i32 = numbers.as_ptr();

    unsafe {
        // offset: desplazar N elementos (no bytes)
        println!("{}", *ptr);              // 10
        println!("{}", *ptr.offset(1));    // 20  (avanza 4 bytes = sizeof(i32))
        println!("{}", *ptr.offset(4));    // 50

        // add/sub: equivalentes más legibles (Rust 1.26+)
        println!("{}", *ptr.add(2));       // 30
        println!("{}", *ptr.add(3));       // 40

        // sub: retroceder
        let end = ptr.add(4);
        println!("{}", *end.sub(1));       // 40
    }
}
```

### Diferencia entre punteros

```rust
fn main() {
    let arr = [1, 2, 3, 4, 5];
    let p1: *const i32 = &arr[1];
    let p2: *const i32 = &arr[4];

    unsafe {
        // offset_from: cuántos elementos hay entre dos punteros
        let diff = p2.offset_from(p1);
        println!("diff: {}", diff); // diff: 3
    }
}
```

> **Predicción**: si `ptr` apunta a un `u8` y llamas `ptr.add(3)`, ¿cuántos bytes avanza?
> Avanza 3 bytes, porque `sizeof(u8) = 1`. Con `i32` avanzaría `3 × 4 = 12` bytes.

### byte_add para avance en bytes

```rust
fn main() {
    let value: u32 = 0xDEADBEEF;
    let ptr: *const u32 = &value;

    // Avanzar bytes (no elementos) — Rust nightly / 1.75+
    let byte_ptr: *const u8 = ptr.cast::<u8>();
    unsafe {
        println!("byte 0: {:#04X}", *byte_ptr);           // 0xEF (little-endian)
        println!("byte 1: {:#04X}", *byte_ptr.add(1));     // 0xBE
        println!("byte 2: {:#04X}", *byte_ptr.add(2));     // 0xAD
        println!("byte 3: {:#04X}", *byte_ptr.add(3));     // 0xDE
    }
}
```

---

## Conversiones entre punteros y referencias

### Raw pointer → referencia

```rust
fn main() {
    let mut value = 42;
    let ptr: *mut i32 = &mut value;

    // Convertir raw pointer a referencia — unsafe
    let ref_from_ptr: &i32 = unsafe { &*ptr };
    println!("ref: {}", ref_from_ptr); // ref: 42

    // A referencia mutable
    let mut_ref: &mut i32 = unsafe { &mut *ptr };
    *mut_ref = 100;
    println!("modified: {}", value); // modified: 100
}
```

### Usando as_ref() y as_mut()

Estos métodos son más seguros porque retornan `Option`:

```rust
fn main() {
    let mut value = 42;
    let ptr: *mut i32 = &mut value;
    let null_ptr: *const i32 = std::ptr::null();

    unsafe {
        // as_ref(): *const T → Option<&T>
        // Retorna None si el puntero es nulo
        match ptr.as_ref() {
            Some(r) => println!("value: {}", r), // value: 42
            None => println!("null pointer"),
        }

        match null_ptr.as_ref() {
            Some(r) => println!("value: {}", r),
            None => println!("null pointer"),     // null pointer
        }

        // as_mut(): *mut T → Option<&mut T>
        if let Some(r) = ptr.as_mut() {
            *r = 99;
        }
    }
    println!("after as_mut: {}", value); // after as_mut: 99
}
```

### Referencia → raw pointer → referencia (roundtrip)

```rust
fn main() {
    let original = String::from("hello");

    // Paso 1: referencia → raw pointer (safe)
    let ptr: *const String = &original;

    // Paso 2: raw pointer → referencia (unsafe)
    let recovered: &String = unsafe { &*ptr };

    println!("{}", recovered); // hello
    // Esto es safe porque 'original' sigue vivo
}
```

```
┌────────────┐    as *const T    ┌─────────────┐    &*ptr (unsafe)   ┌────────────┐
│   &T       │ ───────────────→  │  *const T   │ ──────────────────→ │   &T       │
│ (referencia)│                  │ (raw pointer)│                    │(referencia) │
└────────────┘                   └─────────────┘                    └────────────┘
                                       │
                                  Safe crear
                                  Unsafe usar
```

---

## Punteros y colecciones: slice desde raw pointer

### Construir slice desde puntero y longitud

```rust
use std::slice;

fn main() {
    let data = vec![1, 2, 3, 4, 5];
    let ptr: *const i32 = data.as_ptr();
    let len = data.len();

    // Reconstruir un slice desde raw pointer
    let reconstructed: &[i32] = unsafe {
        slice::from_raw_parts(ptr, len)
    };

    println!("{:?}", reconstructed); // [1, 2, 3, 4, 5]
}
```

### Slice mutable

```rust
use std::slice;

fn main() {
    let mut data = vec![1, 2, 3, 4, 5];
    let ptr: *mut i32 = data.as_mut_ptr();
    let len = data.len();

    let slice_mut: &mut [i32] = unsafe {
        slice::from_raw_parts_mut(ptr, len)
    };

    slice_mut[0] = 100;
    slice_mut[4] = 500;

    println!("{:?}", data); // [100, 2, 3, 4, 500]
}
```

### Ejemplo real: split_at_mut manual

La librería estándar usa raw pointers internamente. Aquí una versión simplificada de `split_at_mut`:

```rust
fn my_split_at_mut(slice: &mut [i32], mid: usize) -> (&mut [i32], &mut [i32]) {
    let len = slice.len();
    assert!(mid <= len, "mid out of bounds");

    let ptr = slice.as_mut_ptr();

    unsafe {
        (
            std::slice::from_raw_parts_mut(ptr, mid),
            std::slice::from_raw_parts_mut(ptr.add(mid), len - mid),
        )
    }
}

fn main() {
    let mut data = vec![1, 2, 3, 4, 5];
    let (left, right) = my_split_at_mut(&mut data, 3);

    left[0] = 10;
    right[0] = 40;

    println!("{:?}", data); // [10, 2, 3, 40, 5]
}
```

El borrow checker no permite tener dos `&mut` al mismo slice. Los raw pointers permiten expresar que las dos mitades no se solapan — pero esa garantía queda bajo **tu** responsabilidad.

---

## Patrones comunes con raw pointers

### Patrón 1: Puntero opaco (void pointer)

```rust
use std::ffi::c_void;

fn store_anything(data: *const c_void) {
    // En un caso real, guardarías el puntero en algún lado
    println!("stored pointer: {:p}", data);
}

fn main() {
    let number: i32 = 42;
    let text = "hello";

    // Cast a void pointer
    store_anything(&number as *const i32 as *const c_void);
    store_anything(text.as_ptr() as *const c_void);
}
```

### Patrón 2: Swap manual con ptr::swap

```rust
use std::ptr;

fn main() {
    let mut a = 10;
    let mut b = 20;

    let pa: *mut i32 = &mut a;
    let pb: *mut i32 = &mut b;

    unsafe {
        ptr::swap(pa, pb);
    }

    println!("a={}, b={}", a, b); // a=20, b=10
}
```

### Patrón 3: Copiar memoria con ptr::copy / ptr::copy_nonoverlapping

```rust
use std::ptr;

fn main() {
    let src = [1, 2, 3, 4];
    let mut dst = [0i32; 4];

    unsafe {
        // copy_nonoverlapping: src y dst NO se solapan (como memcpy)
        ptr::copy_nonoverlapping(
            src.as_ptr(),       // origen
            dst.as_mut_ptr(),   // destino
            src.len(),          // cantidad de elementos (no bytes)
        );
    }

    println!("{:?}", dst); // [1, 2, 3, 4]
}
```

```
ptr::copy_nonoverlapping  ←→  memcpy   (no solapamiento)
ptr::copy                 ←→  memmove  (acepta solapamiento)
ptr::write                ←→  asignar sin leer el valor anterior
ptr::read                 ←→  leer sin mover ownership
ptr::swap                 ←→  intercambiar dos valores
```

### Patrón 4: Escribir sin Drop previo (ptr::write)

```rust
use std::ptr;

fn main() {
    // MaybeUninit es la forma correcta de tener memoria no inicializada
    let mut uninit: std::mem::MaybeUninit<String> = std::mem::MaybeUninit::uninit();

    unsafe {
        // write: escribe un valor sin intentar dropear el valor anterior
        // (que en este caso no existe porque no está inicializado)
        ptr::write(uninit.as_mut_ptr(), String::from("hello"));

        let initialized = uninit.assume_init();
        println!("{}", initialized); // hello
    }
}
```

---

## Punteros nulos y comprobaciones

### Verificar nulidad

```rust
use std::ptr;

fn process(data: *const i32) {
    if data.is_null() {
        println!("received null pointer");
        return;
    }

    let value = unsafe { *data };
    println!("value: {}", value);
}

fn main() {
    let x = 42;
    process(&x as *const i32);     // value: 42
    process(ptr::null());          // received null pointer
}
```

### NonNull: puntero que nunca es nulo

```rust
use std::ptr::NonNull;

fn main() {
    let mut value = 42;

    // NonNull garantiza que el puntero no es nulo
    let nn: NonNull<i32> = NonNull::new(&mut value as *mut i32)
        .expect("pointer was null");

    // Dereferenciar sigue siendo unsafe
    unsafe {
        println!("value: {}", *nn.as_ptr());  // value: 42
        *nn.as_ptr() = 100;
        println!("modified: {}", *nn.as_ptr()); // modified: 100
    }
}
```

`NonNull<T>` es el building block de muchas abstracciones internas de Rust (`Box`, `Vec`, `LinkedList`). Proporciona:

- Garantía de no-nulidad (habilita la **niche optimization**: `Option<NonNull<T>>` tiene el mismo tamaño que `*mut T`).
- Es covariant sobre `T` (como `*const T`).
- **No** implica ownership ni auto-drop.

```
┌─────────────────────────────────────────────────┐
│            Jerarquía de punteros                │
├─────────────────────────────────────────────────┤
│  *mut T / *const T                              │
│    └─ NonNull<T>     (nunca nulo)               │
│         └─ Unique<T> (ownership, nightly)       │
│              └─ Box<T> (ownership + Drop)       │
└─────────────────────────────────────────────────┘
```

---

## Raw pointers y el sistema de tipos

### Cast entre tipos de punteros

```rust
fn main() {
    let x: i32 = 42;
    let ptr: *const i32 = &x;

    // Cast de *const T a *const U (reinterpret)
    let byte_ptr: *const u8 = ptr as *const u8;

    // Cast de *const a *mut (no cambia la memoria, solo el tipo)
    let mut_ptr: *mut i32 = ptr as *mut i32;

    // Método cast<U>() — más legible
    let byte_ptr2: *const u8 = ptr.cast::<u8>();

    // const → mut con cast_mut() (Rust 1.65+)
    let mut_ptr2: *mut i32 = ptr.cast_mut();

    unsafe {
        println!("first byte: {:#04X}", *byte_ptr); // 0x2A (42 en hex)
    }
}
```

### Traits implementados por raw pointers

```rust
fn main() {
    let x = 42;
    let ptr: *const i32 = &x;

    // Send y Sync: raw pointers NO los implementan
    // Esto significa que *const T y *mut T no se pueden
    // compartir entre threads sin wrapper explícito

    // Clone y Copy: SÍ los implementan (copian la dirección)
    let ptr2 = ptr;  // copia del puntero, no del dato

    // PartialEq/Eq: comparan direcciones, no contenido
    let ptr3: *const i32 = &x;
    println!("same address: {}", std::ptr::eq(ptr, ptr3)); // true

    // Debug: muestra la dirección
    println!("{:?}", ptr);   // 0x7ffd... (dirección)
    println!("{:p}", ptr);   // 0x7ffd... (con formato :p)

    // Hash: usa la dirección
}
```

> **Nota importante**: `*const T` y `*mut T` no implementan `Send` ni `Sync`. Si necesitas enviar un raw pointer entre threads, debes envolverlo en un struct y **manualmente** implementar `unsafe impl Send` / `unsafe impl Sync` — asumiendo la responsabilidad de que sea seguro hacerlo.

---

## Errores comunes

### 1. Dereferenciar un puntero sin bloque unsafe

```rust
fn main() {
    let x = 42;
    let ptr: *const i32 = &x;

    // ERROR: dereference of raw pointer requires unsafe
    // let v = *ptr;

    // CORRECTO:
    let v = unsafe { *ptr };
    println!("{}", v);
}
```

### 2. Crear un puntero colgante (dangling)

```rust
fn dangling() -> *const String {
    let s = String::from("hello");
    &s as *const String
    // s se dropea aquí → el puntero queda colgando
}

// CORRECTO: asegurar que el dato vive suficiente
fn not_dangling(s: &String) -> *const String {
    s as *const String
    // El llamador mantiene vivo s
}
```

### 3. Olvidar que offset/add cuenta elementos, no bytes

```rust
fn main() {
    let arr = [1u32, 2, 3];
    let ptr = arr.as_ptr();

    unsafe {
        // INCORRECTO: esto avanza 4 * 4 = 16 bytes, saliendo del array
        // let val = *ptr.add(4); // UB: fuera de bounds

        // CORRECTO: add(1) avanza 4 bytes (sizeof u32)
        let val = *ptr.add(1); // 2
        println!("{}", val);
    }
}
```

### 4. Aliasing inválido: *mut T y &T simultáneos

```rust
fn main() {
    let mut x = 42;
    let ptr: *mut i32 = &mut x;

    // PELIGROSO: crear referencia compartida mientras existe *mut
    // y luego mutar a través del puntero
    let r: &i32 = &x;

    unsafe {
        // UB: mutar mientras r está vivo
        // *ptr = 100;
    }

    // Si necesitas leer, hazlo DESPUÉS de terminar con el puntero
    // o usa solo el puntero para todo
    println!("{}", r);
}
```

### 5. Usar ptr::read sin considerar ownership (doble Drop)

```rust
fn main() {
    let s = String::from("hello");
    let ptr: *const String = &s;

    unsafe {
        // ptr::read crea una COPIA bitwise — ahora hay dos owners
        let s2 = ptr::read(ptr);
        // Al finalizar main: tanto s como s2 llaman drop()
        // → double free!

        // CORRECTO: olvidar uno de los dos
        std::mem::forget(s2);
    }
}
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║               RAW POINTERS — REFERENCIA RÁPIDA           ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  CREAR (safe)                                             ║
║  ─────────────                                            ║
║  let p: *const T = &value;                                ║
║  let p: *mut T   = &mut value;                            ║
║  let p: *const T = ptr::null();                           ║
║  let p = ptr::addr_of!(field);     // campos packed       ║
║                                                           ║
║  DEREFERENCIAR (unsafe)                                   ║
║  ──────────────────────                                   ║
║  let v = unsafe { *ptr };          // leer                ║
║  unsafe { *ptr = val; }            // escribir            ║
║                                                           ║
║  ARITMÉTICA                                               ║
║  ──────────                                               ║
║  ptr.add(n)          avanza n elementos                   ║
║  ptr.sub(n)          retrocede n elementos                ║
║  ptr.offset(n)       avanza n (puede ser negativo)        ║
║  ptr.offset_from(q)  distancia en elementos               ║
║                                                           ║
║  CONVERSIÓN                                               ║
║  ──────────                                               ║
║  ptr as *const U        reinterpret cast                  ║
║  ptr.cast::<U>()        cast tipado                       ║
║  ptr.cast_mut()         *const → *mut                     ║
║  ptr.cast_const()       *mut → *const                     ║
║  unsafe { &*ptr }       → &T                              ║
║  unsafe { &mut *ptr }   → &mut T                          ║
║  ptr.as_ref()           → Option<&T>   (null → None)      ║
║  ptr.as_mut()           → Option<&mut T>                   ║
║                                                           ║
║  OPERACIONES std::ptr                                     ║
║  ────────────────────                                     ║
║  ptr::null()            puntero nulo *const T              ║
║  ptr::null_mut()        puntero nulo *mut T                ║
║  ptr::read(src)         leer sin mover                     ║
║  ptr::write(dst, val)   escribir sin dropear anterior      ║
║  ptr::copy(s, d, n)     memmove (n elementos)             ║
║  ptr::copy_nonoverlapping(s, d, n)   memcpy               ║
║  ptr::swap(a, b)        intercambiar valores               ║
║  ptr::eq(a, b)          comparar direcciones               ║
║  ptr.is_null()          ¿es nulo?                          ║
║                                                           ║
║  NonNull<T>                                               ║
║  ──────────                                               ║
║  NonNull::new(ptr)      → Option<NonNull<T>>              ║
║  nn.as_ptr()            → *mut T                          ║
║  nn.as_ref()            → &T (unsafe)                     ║
║  nn.as_mut()            → &mut T (unsafe)                 ║
║                                                           ║
║  REGLA DE ORO                                             ║
║  ─────────────                                            ║
║  Crear: safe  │  Dereferenciar: unsafe                    ║
║  Offset: unsafe si el resultado sale de la asignación     ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Buffer circular con raw pointers

Implementa un buffer circular (`RingBuffer<T>`) que use raw pointers internamente:

```rust
struct RingBuffer<T> {
    ptr: *mut T,
    capacity: usize,
    head: usize, // siguiente posición de escritura
    tail: usize, // siguiente posición de lectura
    len: usize,
}

impl<T> RingBuffer<T> {
    fn new(capacity: usize) -> Self { todo!() }
    fn push(&mut self, value: T) -> Result<(), T> { todo!() } // Err si lleno
    fn pop(&mut self) -> Option<T> { todo!() }
    fn len(&self) -> usize { todo!() }
    fn is_empty(&self) -> bool { todo!() }
    fn is_full(&self) -> bool { todo!() }
}

impl<T> Drop for RingBuffer<T> {
    fn drop(&mut self) { todo!() }
    // Debe dropear los elementos restantes y liberar la memoria
}
```

**Pistas**:
- Usa `std::alloc::alloc` para reservar memoria, o `Vec::with_capacity` + `into_raw_parts`.
- `push`: usa `ptr::write(self.ptr.add(self.head), value)`.
- `pop`: usa `ptr::read(self.ptr.add(self.tail))`.
- `drop`: itera los elementos restantes con `ptr::drop_in_place`, luego libera la memoria.

**Pregunta de reflexión**: ¿Por qué usamos `ptr::write` en `push` en lugar de `*ptr.add(head) = value`? ¿Qué pasaría si la posición contuviera memoria no inicializada?

---

### Ejercicio 2: Linked list intrusiva

Implementa una lista enlazada simple usando raw pointers:

```rust
struct Node<T> {
    value: T,
    next: *mut Node<T>, // null si es el último
}

struct LinkedList<T> {
    head: *mut Node<T>,
    len: usize,
}

impl<T> LinkedList<T> {
    fn new() -> Self { todo!() }
    fn push_front(&mut self, value: T) { todo!() }
    fn pop_front(&mut self) -> Option<T> { todo!() }
    fn peek(&self) -> Option<&T> { todo!() }
    fn len(&self) -> usize { todo!() }
    fn iter(&self) -> Iter<'_, T> { todo!() }
}

struct Iter<'a, T> {
    current: *const Node<T>,
    _marker: std::marker::PhantomData<&'a T>,
}

impl<'a, T> Iterator for Iter<'a, T> {
    type Item = &'a T;
    fn next(&mut self) -> Option<Self::Item> { todo!() }
}

impl<T> Drop for LinkedList<T> {
    fn drop(&mut self) { todo!() }
}
```

**Pistas**:
- `push_front`: usa `Box::into_raw(Box::new(Node { ... }))` para crear nodos en el heap.
- `pop_front`: usa `Box::from_raw(self.head)` para reclamar la memoria.
- `iter`: recorre con `(*current).next` en unsafe.
- `Drop`: pop todos los elementos.

**Pregunta de reflexión**: ¿Por qué necesitamos `PhantomData<&'a T>` en `Iter`? ¿Qué garantiza al compilador?

---

### Ejercicio 3: safe wrapper sobre una API de punteros

Dada esta API "insegura" que simula una biblioteca C:

```rust
// Simula una API C que trabaja con un buffer de floats
mod ffi_sim {
    static mut BUFFER: [f64; 1024] = [0.0; 1024];
    static mut LEN: usize = 0;

    pub unsafe fn buffer_push(value: f64) -> i32 {
        if LEN >= 1024 { return -1; }
        BUFFER[LEN] = value;
        LEN += 1;
        0
    }

    pub unsafe fn buffer_get(index: usize, out: *mut f64) -> i32 {
        if index >= LEN || out.is_null() { return -1; }
        *out = BUFFER[index];
        0
    }

    pub unsafe fn buffer_len() -> usize {
        LEN
    }

    pub unsafe fn buffer_clear() {
        LEN = 0;
    }
}
```

Crea un wrapper safe:

```rust
struct SafeBuffer; // no necesita estado propio, la API es global

#[derive(Debug)]
enum BufferError {
    Full,
    OutOfBounds,
}

impl SafeBuffer {
    fn new() -> Self { todo!() }        // llama buffer_clear
    fn push(&self, value: f64) -> Result<(), BufferError> { todo!() }
    fn get(&self, index: usize) -> Result<f64, BufferError> { todo!() }
    fn len(&self) -> usize { todo!() }
    fn is_empty(&self) -> bool { todo!() }
}
```

**Pistas**:
- `push`: convierte el retorno `-1` en `Err(BufferError::Full)`.
- `get`: usa una variable local `let mut out: f64 = 0.0;` y pasa `&mut out as *mut f64`.
- Documenta que `SafeBuffer` no es thread-safe (la API global usa `static mut`).

**Pregunta de reflexión**: ¿Por qué esta API global con `static mut` es inherentemente unsafe incluso con nuestro wrapper? ¿Qué pasaría si dos threads crearan `SafeBuffer` simultáneamente?
