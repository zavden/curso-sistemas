# Miri: detección de Undefined Behavior en Rust

## Índice

1. [¿Qué es Miri?](#qué-es-miri)
2. [Instalar y ejecutar Miri](#instalar-y-ejecutar-miri)
3. [Qué detecta Miri](#qué-detecta-miri)
4. [Ejemplos de UB detectado por Miri](#ejemplos-de-ub-detectado-por-miri)
5. [Integrar Miri en tu flujo de trabajo](#integrar-miri-en-tu-flujo-de-trabajo)
6. [Flags y configuración](#flags-y-configuración)
7. [Stacked Borrows y Tree Borrows](#stacked-borrows-y-tree-borrows)
8. [Limitaciones de Miri](#limitaciones-de-miri)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## ¿Qué es Miri?

Miri es un **intérprete** del MIR (Mid-level Intermediate Representation) de Rust. Ejecuta tu programa paso a paso, verificando en cada operación que no se produzca Undefined Behavior.

```
┌──────────────────────────────────────────────────────┐
│          Compilación normal vs Miri                  │
├──────────────────────────────────────────────────────┤
│                                                      │
│  cargo test (normal):                                │
│  Código → Compilar → Ejecutar nativamente            │
│  - Rápido                                            │
│  - UB puede pasar desapercibido (funciona "por       │
│    casualidad")                                      │
│                                                      │
│  cargo miri test:                                    │
│  Código → Compilar a MIR → Interpretar paso a paso   │
│  - Lento (10-100x más lento)                         │
│  - Detecta UB que el código nativo oculta            │
│  - Verifica aliasing, alineación, inicialización,    │
│    bounds, lifetime de punteros                      │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### ¿Por qué no basta con "funciona en mi máquina"?

```rust
fn main() {
    let mut v = vec![1, 2, 3];
    let ptr = v.as_ptr();

    v.push(4); // puede reubicar el buffer interno

    // Esto PUEDE funcionar en tu máquina (si el buffer no se reubicó)
    // Pero es UB: ptr puede apuntar a memoria liberada
    // Miri lo detecta. Tu CPU no.
    unsafe {
        println!("{}", *ptr); // use-after-free potencial
    }
}
```

El UB puede "funcionar" con un compilador, en una plataforma, con un nivel de optimización. Miri detecta el problema **independientemente** de si se manifiesta o no.

---

## Instalar y ejecutar Miri

### Instalación

```bash
# Miri requiere el toolchain nightly
rustup toolchain install nightly

# Instalar el componente miri
rustup component add miri --toolchain nightly
```

### Ejecución básica

```bash
# Ejecutar tests bajo Miri
cargo +nightly miri test

# Ejecutar el binario principal bajo Miri
cargo +nightly miri run

# Ejecutar un test específico
cargo +nightly miri test test_name

# Ejecutar con más información
cargo +nightly miri test -- --nocapture
```

### Primera ejecución: setup automático

```bash
$ cargo +nightly miri test
# La primera vez, Miri descarga y compila la librería estándar
# instrumentada. Esto puede tardar unos minutos.

# Después, ejecuta los tests interpretándolos
```

### Ejemplo rápido

Crea un proyecto de prueba:

```bash
cargo new miri_demo && cd miri_demo
```

```rust
// src/lib.rs
pub fn safe_get(data: &[i32], index: usize) -> Option<i32> {
    if index < data.len() {
        // SAFETY: index < data.len() verificado arriba
        Some(unsafe { *data.as_ptr().add(index) })
    } else {
        None
    }
}

pub unsafe fn unsafe_get(data: &[i32], index: usize) -> i32 {
    // SAFETY: el caller garantiza index < data.len()
    unsafe { *data.as_ptr().add(index) }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_safe_get_valid() {
        let data = [10, 20, 30];
        assert_eq!(safe_get(&data, 1), Some(20));
    }

    #[test]
    fn test_safe_get_out_of_bounds() {
        let data = [10, 20, 30];
        assert_eq!(safe_get(&data, 5), None);
    }

    #[test]
    fn test_unsafe_get_valid() {
        let data = [10, 20, 30];
        // Uso correcto: index < len
        let val = unsafe { unsafe_get(&data, 2) };
        assert_eq!(val, 30);
    }

    // Este test detectaría UB si se descomenta:
    // #[test]
    // fn test_unsafe_get_ub() {
    //     let data = [10, 20, 30];
    //     let val = unsafe { unsafe_get(&data, 100) };
    //     // Miri: error: out-of-bounds pointer arithmetic
    // }
}
```

```bash
# Tests pasan normalmente
$ cargo test
# running 3 tests ... ok

# Tests pasan también bajo Miri (no hay UB)
$ cargo +nightly miri test
# running 3 tests ... ok
```

---

## Qué detecta Miri

### Lista de UB que Miri detecta

```
┌──────────────────────────────────────────────────────────┐
│              UB detectado por Miri                        │
├──────────────────────────────────────────────────────────┤
│                                                          │
│  MEMORIA                                                 │
│  ├─ Out-of-bounds access (lectura/escritura)            │
│  ├─ Use-after-free (acceder memoria ya liberada)        │
│  ├─ Double free                                          │
│  ├─ Puntero desalineado (dereferenciar)                 │
│  ├─ Acceso a memoria no inicializada                    │
│  └─ Memory leaks (con flag -Zmiri-leak-check)           │
│                                                          │
│  ALIASING (Stacked/Tree Borrows)                        │
│  ├─ Usar &mut mientras existe &                         │
│  ├─ Crear &mut desde un shared reference                │
│  ├─ Invalidar una referencia por retag                  │
│  └─ Violar las reglas de préstamo con raw pointers      │
│                                                          │
│  TIPOS                                                   │
│  ├─ Valor inválido (bool que no es 0/1, char inválido)  │
│  ├─ Enum con discriminante inválido                     │
│  ├─ Null reference (&T que es 0x0)                      │
│  └─ Tipo transmutido inválido                           │
│                                                          │
│  CONCURRENCIA                                            │
│  ├─ Data races (acceso no sincronizado entre threads)   │
│  └─ Uso incorrecto de atomics                           │
│                                                          │
│  OTROS                                                   │
│  ├─ Aritmética de punteros fuera de una asignación      │
│  ├─ Unwinding a través de extern "C"                    │
│  └─ Violación de precondiciones de funciones intrínsecas │
│                                                          │
└──────────────────────────────────────────────────────────┘
```

### Lo que Miri NO detecta

- **Bugs lógicos**: si tu código produce el resultado incorrecto pero no hace UB, Miri no lo reporta.
- **Problemas de rendimiento**: Miri no mide performance.
- **Todos los paths**: Miri solo ejecuta los paths que tus tests cubren. Si un test no ejecuta el código con UB, Miri no lo detecta.
- **UB en código C**: Miri solo interpreta Rust. Si llamas a C via FFI, Miri no puede verificar el código C.

---

## Ejemplos de UB detectado por Miri

### 1. Out-of-bounds access

```rust
#[test]
fn test_oob() {
    let data = [1, 2, 3];
    let ptr = data.as_ptr();

    unsafe {
        let _val = *ptr.add(10); // OOB
    }
}
```

```
$ cargo +nightly miri test test_oob

error: Undefined Behavior: dereferencing pointer failed: alloc1234
       has size 12, so pointer to 4 bytes starting at offset 40
       is out-of-bounds
  --> src/lib.rs:7:20
   |
7  |         let _val = *ptr.add(10);
   |                    ^^^^^^^^^^^^ out-of-bounds
```

### 2. Use-after-free

```rust
#[test]
fn test_use_after_free() {
    let ptr;
    {
        let data = vec![1, 2, 3];
        ptr = data.as_ptr();
        // data se dropea aquí
    }

    unsafe {
        let _val = *ptr; // use-after-free
    }
}
```

```
$ cargo +nightly miri test test_use_after_free

error: Undefined Behavior: dereferencing pointer failed: alloc1234
       has been freed, so this pointer is dangling
  --> src/lib.rs:10:20
   |
10 |         let _val = *ptr;
   |                    ^^^^ dangling pointer
```

### 3. Valor inválido

```rust
#[test]
fn test_invalid_bool() {
    let val: u8 = 2;
    let _b: bool = unsafe { std::mem::transmute(val) };
    // bool solo puede ser 0 o 1
}
```

```
$ cargo +nightly miri test test_invalid_bool

error: Undefined Behavior: constructing invalid value:
       encountered 0x02, but expected a boolean
```

### 4. Puntero desalineado

```rust
#[test]
fn test_unaligned() {
    let data = [0u8; 8];
    let ptr = &data[1] as *const u8;

    // Intentar leer un u32 desde una dirección no alineada a 4 bytes
    let u32_ptr = ptr as *const u32;
    unsafe {
        let _val = *u32_ptr; // desalineado
    }
}
```

```
$ cargo +nightly miri test test_unaligned

error: Undefined Behavior: accessing memory with alignment 1,
       but alignment 4 is required
```

### 5. Violación de aliasing (Stacked Borrows)

```rust
#[test]
fn test_aliasing_violation() {
    let mut x = 42;
    let r = &x;              // shared reference
    let ptr = r as *const i32 as *mut i32;

    unsafe {
        *ptr = 100;           // mutar a través de shared reference → UB
    }

    println!("{}", x);
}
```

```
$ cargo +nightly miri test test_aliasing_violation

error: Undefined Behavior: attempting a write access using
       <tag> which is a SharedReadOnly tag, but this location
       is protected
```

### 6. Data race

```rust
#[test]
fn test_data_race() {
    use std::thread;

    static mut COUNTER: u32 = 0;

    let t1 = thread::spawn(|| unsafe { COUNTER += 1; });
    let t2 = thread::spawn(|| unsafe { COUNTER += 1; });

    t1.join().unwrap();
    t2.join().unwrap();
}
```

```
$ cargo +nightly miri test test_data_race

error: Undefined Behavior: Data race detected between (1) Write
       on thread `<unnamed>` and (2) Write on thread `<unnamed>`
```

---

## Integrar Miri en tu flujo de trabajo

### En CI (GitHub Actions)

```yaml
# .github/workflows/miri.yml
name: Miri
on: [push, pull_request]

jobs:
  miri:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: dtolnay/rust-toolchain@nightly
        with:
          components: miri
      - name: Run Miri
        run: cargo miri test
        env:
          MIRIFLAGS: "-Zmiri-strict-provenance"
```

### Separar tests para Miri

Algunos tests no son compatibles con Miri (FFI, I/O de red, timing). Sepáralos:

```rust
#[cfg(test)]
mod tests {
    use super::*;

    // Tests que corren bajo Miri Y cargo test normal
    #[test]
    fn test_basic_operations() {
        let mut buf = MyBuffer::new(10);
        buf.push(42);
        assert_eq!(buf.get(0), Some(&42));
    }

    #[test]
    fn test_boundary_conditions() {
        let mut buf = MyBuffer::new(2);
        buf.push(1);
        buf.push(2);
        assert!(buf.push(3).is_err()); // lleno
    }

    // Tests que NO son compatibles con Miri
    #[test]
    #[cfg_attr(miri, ignore)]
    fn test_with_file_io() {
        // Miri no soporta la mayoría de syscalls
        let content = std::fs::read_to_string("/etc/hostname").unwrap();
        assert!(!content.is_empty());
    }

    #[test]
    #[cfg_attr(miri, ignore)]
    fn test_with_networking() {
        // Miri no soporta sockets
    }

    #[test]
    #[cfg_attr(miri, ignore)]
    fn test_performance() {
        // Miri es 10-100x más lento, no tiene sentido medir performance
    }
}
```

### Detectar si estamos bajo Miri

```rust
fn main() {
    // cfg(miri) es true cuando se ejecuta bajo Miri
    if cfg!(miri) {
        println!("Running under Miri");
        // Usar datasets más pequeños para que termine en tiempo razonable
    } else {
        println!("Running natively");
    }
}

// También en atributos:
#[cfg(not(miri))]
fn only_native() {
    // Código que solo se compila en ejecución nativa
}
```

### Reducir datasets bajo Miri

```rust
#[test]
fn test_large_operations() {
    // Miri es muy lento — usar datasets pequeños
    let iterations = if cfg!(miri) { 100 } else { 1_000_000 };

    let mut buf = MyBuffer::new(iterations);
    for i in 0..iterations {
        buf.push(i as u8);
    }
    assert_eq!(buf.len(), iterations);
}
```

---

## Flags y configuración

### Flags más útiles

```bash
# Strict provenance: detecta casts entero↔puntero sospechosos
cargo +nightly miri test -Zmiri-strict-provenance

# Verificar memory leaks (por defecto Miri no los reporta)
cargo +nightly miri test -Zmiri-leak-check

# Tree Borrows en lugar de Stacked Borrows (modelo más permisivo)
cargo +nightly miri test -Zmiri-tree-borrows

# Seed para el PRNG de Miri (reproducir ejecución no determinista)
cargo +nightly miri test -Zmiri-seed=42

# Deshabilitar aislamiento (permitir acceso al sistema de archivos)
cargo +nightly miri test -Zmiri-disable-isolation

# Backtrace más detallado
MIRIFLAGS="-Zmiri-backtrace=full" cargo +nightly miri test

# Combinar flags
MIRIFLAGS="-Zmiri-strict-provenance -Zmiri-leak-check" cargo +nightly miri test
```

### Variable de entorno MIRIFLAGS

```bash
# Configurar flags globalmente para la sesión
export MIRIFLAGS="-Zmiri-strict-provenance -Zmiri-leak-check"
cargo +nightly miri test

# O por comando:
MIRIFLAGS="-Zmiri-strict-provenance" cargo +nightly miri test
```

### Flags en detalle

```
┌──────────────────────────────┬─────────────────────────────────┐
│ Flag                         │ Efecto                          │
├──────────────────────────────┼─────────────────────────────────┤
│ -Zmiri-strict-provenance     │ Rechaza int-to-ptr casts        │
│                              │ (más estricto, recomendado)     │
├──────────────────────────────┼─────────────────────────────────┤
│ -Zmiri-leak-check            │ Detecta memory leaks al final   │
├──────────────────────────────┼─────────────────────────────────┤
│ -Zmiri-tree-borrows          │ Usa Tree Borrows (más permisivo │
│                              │ que Stacked Borrows por defecto)│
├──────────────────────────────┼─────────────────────────────────┤
│ -Zmiri-seed=N                │ Fija el PRNG para reproducir    │
│                              │ ejecuciones con concurrencia    │
├──────────────────────────────┼─────────────────────────────────┤
│ -Zmiri-disable-isolation     │ Permite acceso al FS real       │
│                              │ (peligroso en CI)               │
├──────────────────────────────┼─────────────────────────────────┤
│ -Zmiri-backtrace=full        │ Backtrace completo en errores   │
├──────────────────────────────┼─────────────────────────────────┤
│ -Zmiri-tag-gc=N              │ Frecuencia de garbage collection│
│                              │ de tags (performance tuning)    │
└──────────────────────────────┴─────────────────────────────────┘
```

---

## Stacked Borrows y Tree Borrows

Miri implementa modelos formales de aliasing para Rust. Estos modelos definen las reglas de quién puede acceder a qué memoria y cuándo.

### Stacked Borrows (default)

Cada ubicación de memoria tiene una **pila de permisos** (stack of borrows). Cada referencia o puntero recibe un "tag" que se apila:

```
┌──────────────────────────────────────────────────────┐
│  Stacked Borrows: pila de permisos para &x           │
│                                                      │
│  let mut x = 42;                                     │
│  Stack: [Unique(x)]                                  │
│                                                      │
│  let r1 = &x;          // push SharedRO              │
│  Stack: [Unique(x), SharedRO(r1)]                    │
│                                                      │
│  let r2 = &x;          // push SharedRO              │
│  Stack: [Unique(x), SharedRO(r1), SharedRO(r2)]      │
│                                                      │
│  // Leer a través de r2: OK (está en el stack)       │
│  // Leer a través de r1: OK (está en el stack)       │
│                                                      │
│  let m = &mut x;       // pop todo, push Unique      │
│  Stack: [Unique(x), Unique(m)]                       │
│                                                      │
│  // Leer a través de r1: ERROR! r1 fue popped        │
│  // Leer a través de r2: ERROR! r2 fue popped        │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### Ejemplo que SB detecta

```rust
#[test]
fn test_stacked_borrows() {
    let mut val = 10;
    let ref1 = &val;
    let ptr = ref1 as *const i32 as *mut i32;

    // Escribir a través de ptr (creado desde shared ref)
    // Esto viola Stacked Borrows: ptr tiene permiso SharedRO,
    // no puede escribir
    unsafe {
        *ptr = 20; // UB bajo Stacked Borrows
    }

    println!("{}", val);
}
```

### Tree Borrows (alternativo, más permisivo)

Tree Borrows es un modelo más reciente y relajado. Organiza los permisos en un árbol en vez de una pila:

```bash
# Usar Tree Borrows
cargo +nightly miri test -Zmiri-tree-borrows
```

Algunos patrones que Stacked Borrows rechaza pero Tree Borrows acepta:

```rust
#[test]
fn test_tree_borrows_accepts() {
    let mut data = [1, 2, 3, 4];
    let ptr1 = data.as_mut_ptr();
    let ptr2 = data.as_mut_ptr();

    // Stacked Borrows: crear ptr2 invalida ptr1 (popped del stack)
    // Tree Borrows: ambos son hijos del mismo nodo, pueden coexistir
    //               si acceden a posiciones diferentes

    unsafe {
        *ptr1.add(0) = 10;    // escribe en posición 0
        *ptr2.add(2) = 30;    // escribe en posición 2 (no se solapa)
    }

    assert_eq!(data, [10, 2, 30, 4]);
}
```

```
┌──────────────────────────────────────────────────┐
│  Stacked Borrows vs Tree Borrows                 │
├──────────────────────────────────────────────────┤
│                                                  │
│  Stacked Borrows (default):                      │
│  + Modelo más simple                             │
│  + Más conservador (rechaza más código)          │
│  - Puede rechazar código que es safe en práctica │
│                                                  │
│  Tree Borrows (-Zmiri-tree-borrows):             │
│  + Más permisivo (acepta más patrones reales)    │
│  + Modela mejor el acceso por bytes              │
│  - Modelo más complejo                           │
│  - Más reciente, aún en evolución                │
│                                                  │
│  Recomendación:                                  │
│  1. Empieza con el default (Stacked Borrows)     │
│  2. Si falla en código que crees correcto,       │
│     prueba con -Zmiri-tree-borrows               │
│  3. Si ambos fallan, tu código probablemente      │
│     tiene UB real                                │
│                                                  │
└──────────────────────────────────────────────────┘
```

---

## Limitaciones de Miri

### No soporta FFI

```rust
#[test]
#[cfg_attr(miri, ignore)]
fn test_ffi() {
    // Miri no puede interpretar código C
    unsafe {
        let pid = libc::getpid();
        assert!(pid > 0);
    }
}
```

Miri intercepta algunas funciones de libc comunes (`malloc`, `free`, `memcpy`, etc.) pero no puede ejecutar código C arbitrario.

### No soporta la mayoría de syscalls

```rust
#[test]
#[cfg_attr(miri, ignore)]
fn test_file_io() {
    // La mayoría de operaciones de I/O fallan bajo Miri
    // (a menos que uses -Zmiri-disable-isolation)
    std::fs::write("/tmp/test.txt", "hello").unwrap();
}
```

### Rendimiento

```rust
#[test]
fn test_with_miri_aware_size() {
    // Un test que tarda 1 segundo nativamente puede tardar
    // 1-2 minutos bajo Miri

    let n = if cfg!(miri) { 1_000 } else { 10_000_000 };

    let mut v = Vec::new();
    for i in 0..n {
        v.push(i);
    }
    assert_eq!(v.len(), n);
}
```

### Cobertura de paths

```rust
fn maybe_ub(flag: bool) -> i32 {
    let x = 42;
    let ptr: *const i32 = &x;

    if flag {
        // UB: leer memoria ya liberada
        drop(x); // (esto no compila realmente, pero ilustra el punto)
        unsafe { *ptr }
    } else {
        // Safe path
        42
    }
}

#[test]
fn test_safe_path() {
    // Miri solo ejecuta el path safe — NO detecta el UB en el path flag=true
    assert_eq!(maybe_ub(false), 42);
}

// Necesitas un test que cubra el path con UB:
// #[test]
// fn test_ub_path() {
//     maybe_ub(true); // Miri detectaría el UB aquí
// }
```

### Tabla de soporte

```
┌────────────────────────────────┬─────────┐
│ Característica                 │ Soporte │
├────────────────────────────────┼─────────┤
│ Código Rust puro               │ ✓       │
│ std::collections               │ ✓       │
│ Threads (std::thread)          │ ✓       │
│ Atomics                        │ ✓       │
│ Mutex, RwLock, Condvar         │ ✓       │
│ Allocator (alloc/dealloc)      │ ✓       │
│ SIMD intrínsics                │ Parcial │
│ Inline assembly                │ ✗       │
│ FFI (llamar C)                 │ ✗       │
│ File I/O                       │ Parcial │
│ Network I/O                    │ ✗       │
│ Timing (Instant, Duration)     │ Parcial │
│ Process/Command                │ ✗       │
│ Señales                        │ ✗       │
└────────────────────────────────┴─────────┘
```

---

## Errores comunes

### 1. No ejecutar Miri porque "mis tests pasan"

```bash
# Tests pasan — ¿significa que no hay UB?
$ cargo test
# running 15 tests ... ok

# Sorpresa: hay UB que los tests no detectan
$ cargo +nightly miri test
# error: Undefined Behavior: ...

# Miri y cargo test son COMPLEMENTARIOS, no sustitutos
# cargo test: ¿hace lo correcto?
# cargo miri test: ¿lo hace sin UB?
```

### 2. Ignorar TODOS los tests bajo Miri en vez de adaptarlos

```rust
// INCORRECTO: ignorar TODO bajo Miri anula su propósito
#[cfg(test)]
#[cfg(not(miri))]  // ← TODO el módulo ignorado bajo Miri
mod tests {
    // Miri no ejecuta nada...
}

// CORRECTO: ignorar solo tests incompatibles con Miri
#[cfg(test)]
mod tests {
    #[test]
    fn test_logic() {
        // Este SÍ debe correr bajo Miri
    }

    #[test]
    #[cfg_attr(miri, ignore)]
    fn test_file_io() {
        // Este no puede correr bajo Miri (I/O)
    }
}
```

### 3. No reducir el tamaño de datos bajo Miri

```rust
#[test]
fn test_large_allocation() {
    // Esto tarda MINUTOS bajo Miri
    let mut v = Vec::with_capacity(10_000_000);
    for i in 0..10_000_000 {
        v.push(i);
    }

    // CORRECTO: reducir bajo Miri
    // let n = if cfg!(miri) { 1_000 } else { 10_000_000 };
    // let mut v = Vec::with_capacity(n);
    // for i in 0..n { v.push(i); }
}
```

### 4. Asumir que si Miri no reporta error, no hay UB

```rust
// Miri solo verifica los paths EJECUTADOS
// Si tu test no cubre un branch con UB, Miri no lo detecta

fn process(data: &[u8], mode: u8) -> u8 {
    match mode {
        0 => data[0],                              // safe
        1 => unsafe { *data.as_ptr().add(9999) },  // UB si len < 10000
        _ => 0,
    }
}

#[test]
fn test_mode_zero() {
    let data = [42];
    assert_eq!(process(&data, 0), 42);
    // Miri: OK — pero mode=1 tiene UB que no se detectó
}

// Necesitas:
// #[test]
// fn test_mode_one() {
//     let data = [42];
//     process(&data, 1); // Miri detectaría el OOB aquí
// }
```

### 5. No usar -Zmiri-strict-provenance

```rust
#[test]
fn test_provenance() {
    let x = 42;
    let addr = &x as *const i32 as usize;  // puntero → entero
    let ptr = addr as *const i32;            // entero → puntero

    // Sin strict-provenance: Miri puede aceptar esto
    // Con strict-provenance: Miri lo rechaza
    // (el puntero perdió su "provenance" al pasar por usize)
    let _val = unsafe { *ptr };
}

// Usar: cargo +nightly miri test -Zmiri-strict-provenance
// Esto detecta patrones de conversión puntero↔entero que pueden
// causar problemas con optimizaciones del compilador
```

---

## Cheatsheet

```
╔═══════════════════════════════════════════════════════════╗
║                MIRI — REFERENCIA RÁPIDA                  ║
╠═══════════════════════════════════════════════════════════╣
║                                                           ║
║  INSTALAR                                                 ║
║  ────────                                                 ║
║  rustup toolchain install nightly                         ║
║  rustup component add miri --toolchain nightly            ║
║                                                           ║
║  EJECUTAR                                                 ║
║  ────────                                                 ║
║  cargo +nightly miri test          todos los tests        ║
║  cargo +nightly miri test nombre   test específico        ║
║  cargo +nightly miri run           el binario             ║
║                                                           ║
║  FLAGS RECOMENDADOS                                       ║
║  ─────────────────                                        ║
║  -Zmiri-strict-provenance   rechazar int↔ptr casts       ║
║  -Zmiri-leak-check          detectar memory leaks         ║
║  -Zmiri-tree-borrows        modelo de aliasing relajado   ║
║  -Zmiri-seed=N              reproducir concurrencia       ║
║  -Zmiri-backtrace=full      traza completa en errores     ║
║                                                           ║
║  DETECTA                                                  ║
║  ───────                                                  ║
║  Out-of-bounds, use-after-free, double free               ║
║  Desalineación, memoria no inicializada                   ║
║  Violaciones de aliasing (Stacked/Tree Borrows)           ║
║  Valores inválidos (bool, char, enum)                     ║
║  Data races, transmutes inválidos                         ║
║                                                           ║
║  NO DETECTA                                               ║
║  ──────────                                               ║
║  Bugs lógicos, problemas de performance                   ║
║  UB en paths no ejecutados por los tests                  ║
║  UB en código C (FFI)                                     ║
║                                                           ║
║  NO SOPORTA                                               ║
║  ──────────                                               ║
║  FFI, inline assembly, network I/O                        ║
║  Señales, processes, la mayoría de syscalls                ║
║                                                           ║
║  ADAPTAR TESTS                                            ║
║  ─────────────                                            ║
║  #[cfg_attr(miri, ignore)]   ignorar test bajo Miri      ║
║  if cfg!(miri) { ... }       reducir datos bajo Miri     ║
║  #[cfg(not(miri))]           compilar solo sin Miri       ║
║                                                           ║
║  CI                                                       ║
║  ──                                                       ║
║  MIRIFLAGS="-Zmiri-strict-provenance -Zmiri-leak-check"  ║
║  cargo +nightly miri test                                 ║
║                                                           ║
╚═══════════════════════════════════════════════════════════╝
```

---

## Ejercicios

### Ejercicio 1: Encontrar UB con Miri

El siguiente código tiene varios bugs de UB. Crea tests que los expongan bajo Miri. Luego corrige cada bug:

```rust
pub struct Stack<T> {
    data: *mut T,
    len: usize,
    cap: usize,
}

impl<T> Stack<T> {
    pub fn new() -> Self {
        Stack {
            data: std::ptr::null_mut(),
            len: 0,
            cap: 0,
        }
    }

    pub fn push(&mut self, value: T) {
        if self.len == self.cap {
            self.grow();
        }
        unsafe {
            // Bug 1: ¿qué pasa cuando cap == 0 y data es null?
            self.data.add(self.len).write(value);
        }
        self.len += 1;
    }

    pub fn pop(&mut self) -> Option<T> {
        if self.len == 0 {
            return None;
        }
        self.len -= 1;
        // Bug 2: ¿read crea una copia? ¿qué pasa con el Drop original?
        Some(unsafe { self.data.add(self.len).read() })
    }

    pub fn peek(&self) -> Option<&T> {
        if self.len == 0 {
            return None;
        }
        // Bug 3: ¿es correcto crear una referencia aquí?
        Some(unsafe { &*self.data.add(self.len - 1) })
    }

    fn grow(&mut self) {
        let new_cap = if self.cap == 0 { 4 } else { self.cap * 2 };
        let new_layout = std::alloc::Layout::array::<T>(new_cap).unwrap();

        let new_data = if self.data.is_null() {
            unsafe { std::alloc::alloc(new_layout) as *mut T }
        } else {
            let old_layout = std::alloc::Layout::array::<T>(self.cap).unwrap();
            unsafe {
                std::alloc::realloc(
                    self.data as *mut u8,
                    old_layout,
                    new_layout.size(),
                ) as *mut T
            }
        };

        // Bug 4: ¿qué pasa si new_data es null?
        self.data = new_data;
        self.cap = new_cap;
    }
}

impl<T> Drop for Stack<T> {
    fn drop(&mut self) {
        // Bug 5: ¿se dropean los elementos restantes?
        if !self.data.is_null() {
            let layout = std::alloc::Layout::array::<T>(self.cap).unwrap();
            unsafe { std::alloc::dealloc(self.data as *mut u8, layout); }
        }
    }
}
```

Escribe tests que fallen bajo `cargo +nightly miri test` y luego corrige los 5 bugs.

**Pregunta de reflexión**: ¿Cuáles de los 5 bugs habrían pasado desapercibidos con `cargo test` normal? ¿Cuáles habría detectado incluso sin Miri?

---

### Ejercicio 2: Validar tu propio unsafe con Miri

Toma cualquiera de los ejercicios de los temas anteriores de este capítulo (RingBuffer, LinkedList, FixedVec, etc.) y:

1. Implementa la solución.
2. Escribe al menos 5 tests que cubran:
   - Operaciones básicas (push, pop, get).
   - Condiciones de frontera (vacío, lleno, un elemento).
   - Tipos con Drop (usa `String` o `Vec` como tipo genérico).
   - Múltiples operaciones intercaladas.
3. Ejecuta `cargo +nightly miri test`.
4. Si Miri reporta errores, corrígelos y documenta qué aprendiste.

**Pistas**:
- Los bugs más comunes que Miri detecta en estas estructuras: no dropear elementos en `Drop`, double-free al hacer `pop` + `drop`, desalineación con tipos genéricos.
- Usa `String` como tipo de test — tiene Drop no trivial, lo que expone bugs de ownership.

**Pregunta de reflexión**: ¿Encontró Miri algún bug que tus tests con `cargo test` no detectaban? ¿Por qué el test pasaba nativamente pero fallaba bajo Miri?

---

### Ejercicio 3: Explorar Stacked Borrows

Experimenta con estos escenarios para entender Stacked Borrows. Para cada uno, predice si Miri lo aceptará o rechazará, luego verifica:

```rust
// Escenario A: crear *mut desde & y escribir
#[test]
fn scenario_a() {
    let x = 42;
    let ptr = &x as *const i32 as *mut i32;
    unsafe { *ptr = 100; }
    println!("{}", x);
}

// Escenario B: dos *mut del mismo &mut, acceso intercalado
#[test]
fn scenario_b() {
    let mut x = 42;
    let ptr1 = &mut x as *mut i32;
    let ptr2 = &mut x as *mut i32;
    unsafe {
        *ptr1 = 10;
        *ptr2 = 20;
        *ptr1 = 30;
    }
}

// Escenario C: *const leído después de *mut write
#[test]
fn scenario_c() {
    let mut x = 42;
    let ptr_r: *const i32 = &x;
    let ptr_w: *mut i32 = &mut x;
    unsafe {
        *ptr_w = 100;
        let _val = *ptr_r;  // ¿sigue válido ptr_r?
    }
}

// Escenario D: raw pointer derivado de &mut, luego &mut usado
#[test]
fn scenario_d() {
    let mut x = 42;
    let ptr = &mut x as *mut i32;
    x = 100;           // uso directo de x
    unsafe { *ptr = 200; }  // ¿sigue válido ptr?
}

// Escenario E: &mut reborrow pattern
#[test]
fn scenario_e() {
    let mut x = 42;
    let r = &mut x;
    let ptr = r as *mut i32;
    unsafe { *ptr = 100; }
    *r = 200;  // ¿invalida ptr? ¿o r "contiene" a ptr?
    println!("{}", x);
}
```

Para cada escenario:
1. Predice: ¿UB o no?
2. Ejecuta con `cargo +nightly miri test`.
3. Si falla, ejecuta con `-Zmiri-tree-borrows` y compara.
4. Explica por qué Stacked Borrows acepta o rechaza cada caso.

**Pregunta de reflexión**: ¿Hay algún escenario que Stacked Borrows rechaza pero Tree Borrows acepta? ¿Qué te dice eso sobre la diferencia entre los dos modelos? ¿Cuál usarías por defecto en CI y por qué?
