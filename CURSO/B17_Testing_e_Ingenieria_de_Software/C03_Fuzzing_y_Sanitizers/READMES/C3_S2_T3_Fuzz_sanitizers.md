# T03 - Fuzz + sanitizers: RUSTFLAGS para ASan/MSan, compilar en nightly, interpretar reportes

## Índice

1. [Por qué sanitizers en Rust](#1-por-qué-sanitizers-en-rust)
2. [Repaso: qué son los sanitizers](#2-repaso-qué-son-los-sanitizers)
3. [Sanitizers disponibles en Rust](#3-sanitizers-disponibles-en-rust)
4. [Requisito: Rust nightly](#4-requisito-rust-nightly)
5. [El flag -Zsanitizer](#5-el-flag--zsanitizer)
6. [AddressSanitizer (ASan) en Rust](#6-addresssanitizer-asan-en-rust)
7. [ASan: qué detecta en Rust](#7-asan-qué-detecta-en-rust)
8. [ASan: compilar y ejecutar manualmente](#8-asan-compilar-y-ejecutar-manualmente)
9. [ASan con cargo-fuzz](#9-asan-con-cargo-fuzz)
10. [ASan: interpretar reportes en Rust](#10-asan-interpretar-reportes-en-rust)
11. [ASan: ejemplo completo con unsafe](#11-asan-ejemplo-completo-con-unsafe)
12. [ASan: ejemplo con FFI (llamando C desde Rust)](#12-asan-ejemplo-con-ffi-llamando-c-desde-rust)
13. [MemorySanitizer (MSan) en Rust](#13-memorysanitizer-msan-en-rust)
14. [MSan: qué detecta](#14-msan-qué-detecta)
15. [MSan: compilar y ejecutar](#15-msan-compilar-y-ejecutar)
16. [MSan: la complicación de la stdlib](#16-msan-la-complicación-de-la-stdlib)
17. [MSan con cargo-fuzz](#17-msan-con-cargo-fuzz)
18. [MSan: interpretar reportes](#18-msan-interpretar-reportes)
19. [MSan: ejemplo completo](#19-msan-ejemplo-completo)
20. [ThreadSanitizer (TSan) en Rust](#20-threadsanitizer-tsan-en-rust)
21. [TSan: qué detecta](#21-tsan-qué-detecta)
22. [TSan: compilar y ejecutar](#22-tsan-compilar-y-ejecutar)
23. [TSan con cargo-fuzz](#23-tsan-con-cargo-fuzz)
24. [TSan: interpretar reportes](#24-tsan-interpretar-reportes)
25. [TSan: ejemplo completo](#25-tsan-ejemplo-completo)
26. [Leak Sanitizer (LSan) en Rust](#26-leak-sanitizer-lsan-en-rust)
27. [Estrategia multi-sanitizer con cargo-fuzz](#27-estrategia-multi-sanitizer-con-cargo-fuzz)
28. [RUSTFLAGS: la variable de entorno clave](#28-rustflags-la-variable-de-entorno-clave)
29. [ASAN_OPTIONS, MSAN_OPTIONS, TSAN_OPTIONS](#29-asan_options-msan_options-tsan_options)
30. [Sanitizers y cobertura de código](#30-sanitizers-y-cobertura-de-código)
31. [Impacto en rendimiento](#31-impacto-en-rendimiento)
32. [Compatibilidad entre sanitizers](#32-compatibilidad-entre-sanitizers)
33. [Falsos positivos y suppressions](#33-falsos-positivos-y-suppressions)
34. [Sanitizers en C vs Rust: comparación](#34-sanitizers-en-c-vs-rust-comparación)
35. [Errores comunes](#35-errores-comunes)
36. [Programa de práctica: cache con unsafe](#36-programa-de-práctica-cache-con-unsafe)
37. [Ejercicios](#37-ejercicios)

---

## 1. Por qué sanitizers en Rust

En S01 (Fuzzing en C) vimos que los sanitizers son **amplificadores** del fuzzer: convierten bugs silenciosos (como leer 1 byte más allá de un buffer) en crashes detectables. En C, los sanitizers son absolutamente esenciales porque casi todos los bugs de memoria son silenciosos sin ellos.

En Rust, la situación es diferente:

```
┌──────────────────────────────────────────────────────────────────┐
│              ¿Cuándo necesitas sanitizers en Rust?                │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Safe Rust:                                                      │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ Sanitizers son OPCIONALES (baja prioridad)                 │  │
│  │                                                            │  │
│  │ El compilador ya previene:                                 │  │
│  │   ✗ Buffer overflow → bounds checking                     │  │
│  │   ✗ Use-after-free  → ownership                           │  │
│  │   ✗ Double free     → Drop                                │  │
│  │   ✗ Data races      → Send/Sync                           │  │
│  │   ✗ Null deref      → Option<T>                           │  │
│  │                                                            │  │
│  │ El fuzzer sin sanitizers encuentra:                        │  │
│  │   ✓ Panics (unwrap, index OOB, overflow)                  │  │
│  │   ✓ Errores lógicos (roundtrip, invariantes)              │  │
│  │   ✓ Timeouts, OOM                                         │  │
│  │                                                            │  │
│  │ ASan solo aporta: detección de memory leaks (LSan)         │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  Unsafe Rust:                                                    │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ Sanitizers son ESENCIALES (alta prioridad)                 │  │
│  │                                                            │  │
│  │ unsafe {} desactiva las protecciones del compilador:       │  │
│  │   ✓ Buffer overflow → ptr::read/write sin bounds check    │  │
│  │   ✓ Use-after-free  → raw pointers pueden dangling        │  │
│  │   ✓ Double free     → ManuallyDrop, raw alloc             │  │
│  │   ✓ Data races      → raw pointers son Send               │  │
│  │   ✓ Uninitialized   → MaybeUninit mal usado               │  │
│  │                                                            │  │
│  │ Son los MISMOS bugs que en C → mismos sanitizers           │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  FFI (Foreign Function Interface):                               │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ Sanitizers son CRÍTICOS                                    │  │
│  │                                                            │  │
│  │ Cuando llamas código C desde Rust (o viceversa):           │  │
│  │   ✓ El código C no tiene ninguna protección               │  │
│  │   ✓ Los bugs del C se manifiestan en el proceso Rust      │  │
│  │   ✓ ASan/MSan instrumentan tanto el código C como Rust    │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Resumen práctico

```
Tu código es 100% safe Rust:
  → Fuzzea sin sanitizers (--sanitizer=none) para máxima velocidad
  → Opcionalmente con ASan para detectar leaks

Tu código tiene bloques unsafe:
  → Fuzzea CON ASan (default de cargo-fuzz)
  → Sesiones adicionales con MSan

Tu código usa FFI (bindgen, libc, etc.):
  → ASan es OBLIGATORIO
  → MSan altamente recomendado
  → TSan si hay concurrencia

Tu código tiene concurrencia unsafe (raw pointers entre threads):
  → TSan es OBLIGATORIO
```

---

## 2. Repaso: qué son los sanitizers

Los sanitizers son herramientas de instrumentación del compilador que insertan verificaciones adicionales en tu código durante la compilación. En runtime, estas verificaciones detectan bugs que de otra forma serían silenciosos.

```
Sin sanitizer:                        Con sanitizer:
─────────────                         ─────────────
ptr = malloc(10);                     ptr = malloc(10);
                                      // ASan: registrar allocación de 10 bytes
                                      // en shadow memory

ptr[15] = 'x';                        ptr[15] = 'x';
// Silencioso: escribe en             // ASan: VERIFICAR shadow memory
// memoria que no es tuya             // → 15 > 10 → HEAP-BUFFER-OVERFLOW
// Puede corromper datos              // → imprimir reporte detallado
// o crashear... o no                 // → abortar el programa
```

### Los 4 sanitizers principales

```
┌──────────────┬──────────────────────────────────────────────────┐
│ Sanitizer    │ Qué detecta                                      │
├──────────────┼──────────────────────────────────────────────────┤
│ ASan         │ Errores de DIRECCIONES de memoria:                │
│ (Address)    │ buffer overflow, use-after-free, double-free,     │
│              │ stack overflow, memory leaks                      │
├──────────────┼──────────────────────────────────────────────────┤
│ MSan         │ Uso de memoria NO INICIALIZADA:                   │
│ (Memory)     │ leer bytes que nunca fueron escritos              │
├──────────────┼──────────────────────────────────────────────────┤
│ TSan         │ DATA RACES entre threads:                         │
│ (Thread)     │ acceso concurrente sin sincronización             │
├──────────────┼──────────────────────────────────────────────────┤
│ LSan         │ MEMORY LEAKS:                                     │
│ (Leak)       │ memoria allocada que nunca se libera              │
│              │ (incluido en ASan por defecto)                    │
└──────────────┴──────────────────────────────────────────────────┘
```

Nota: UBSan (UndefinedBehaviorSanitizer) no tiene soporte directo en Rust a través de `-Zsanitizer`. En Rust, muchos UB que UBSan detecta en C ya son prevenidos por el compilador o por `debug-assertions = true` (integer overflow, etc.).

---

## 3. Sanitizers disponibles en Rust

### Tabla de soporte

| Sanitizer | Flag | Plataformas | Estado en Rust |
|---|---|---|---|
| AddressSanitizer | `-Zsanitizer=address` | Linux x86_64, aarch64, macOS | Experimental pero estable en la práctica |
| LeakSanitizer | `-Zsanitizer=leak` | Linux x86_64 | Experimental |
| MemorySanitizer | `-Zsanitizer=memory` | Linux x86_64 | Experimental, requiere stdlib instrumentada |
| ThreadSanitizer | `-Zsanitizer=thread` | Linux x86_64, aarch64, macOS | Experimental |
| HWAddressSanitizer | `-Zsanitizer=hwaddress` | Linux aarch64 | Experimental |
| CFI | `-Zsanitizer=cfi` | Linux x86_64 | Experimental |
| kcfi | `-Zsanitizer=kcfi` | Linux x86_64, aarch64 | Experimental (kernel) |
| ShadowCallStack | `-Zsanitizer=shadow-call-stack` | Linux aarch64 | Experimental |

### Los que usaremos con fuzzing

Para fuzzing, los sanitizers relevantes son:

```
1. ASan  → el más importante, el default de cargo-fuzz
2. MSan  → para código unsafe/FFI que maneja memoria no inicializada
3. TSan  → para código con concurrencia unsafe
4. LSan  → incluido en ASan, detecta memory leaks
```

---

## 4. Requisito: Rust nightly

Todos los sanitizers en Rust requieren el compilador nightly porque usan el flag `-Z`, que está reservado para opciones experimentales.

```bash
# Verificar que nightly está instalado
rustup toolchain list | grep nightly

# Instalar si no está
rustup toolchain install nightly

# Verificar que el flag funciona
rustup run nightly rustc -Zsanitizer=address --help
```

### ¿Por qué nightly?

```
Los sanitizers en Rust usan la infraestructura de LLVM
(los mismos sanitizers que usa Clang para C/C++).

La integración Rust↔LLVM para sanitizers no está estabilizada
porque:
1. La API interna puede cambiar entre versiones de LLVM
2. Algunas opciones de instrumentación son experimentales
3. El soporte de plataformas es limitado
4. Hay interacciones complejas con el runtime de Rust
   (allocator, unwinding, etc.)

En la práctica, ASan funciona muy bien en nightly y rara vez
se rompe entre versiones.
```

### cargo-fuzz ya maneja nightly

`cargo fuzz run` automáticamente usa nightly. No necesitas hacer `rustup default nightly` ni `cargo +nightly`. cargo-fuzz lo hace internamente.

---

## 5. El flag -Zsanitizer

### Sintaxis

```bash
# Directamente con rustc:
rustc -Zsanitizer=address mi_programa.rs

# Con cargo (a través de RUSTFLAGS):
RUSTFLAGS="-Zsanitizer=address" cargo +nightly build

# Con cargo-fuzz (integrado):
cargo fuzz run my_target --sanitizer=address
```

### Opciones de -Zsanitizer

| Valor | Sanitizer |
|---|---|
| `address` | AddressSanitizer |
| `memory` | MemorySanitizer |
| `thread` | ThreadSanitizer |
| `leak` | LeakSanitizer (standalone, sin ASan) |
| `hwaddress` | Hardware-assisted AddressSanitizer |
| `cfi` | Control Flow Integrity |

### Flags adicionales para sanitizers

```bash
# Instrumentar toda la memoria del stack (más cobertura de ASan)
RUSTFLAGS="-Zsanitizer=address -Zsanitizer-memory-track-origins" \
    cargo +nightly build

# Para MSan: rastrear origen de la memoria no inicializada
RUSTFLAGS="-Zsanitizer=memory -Zsanitizer-memory-track-origins" \
    cargo +nightly build

# Combinación típica para fuzzing con ASan:
RUSTFLAGS="-Zsanitizer=address" \
    cargo +nightly build --release
```

---

## 6. AddressSanitizer (ASan) en Rust

ASan es el sanitizer más usado. Es el **default** de cargo-fuzz (cuando no especificas `--sanitizer`, usa address).

### Cómo funciona

```
┌──────────────────────────────────────────────────────────────┐
│                 Cómo funciona ASan                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. SHADOW MEMORY                                            │
│     ASan reserva una región de memoria "shadow" que          │
│     mapea 1:8 con la memoria real.                           │
│     Cada byte de shadow = estado de 8 bytes reales.          │
│                                                              │
│     ┌──────────────────────┐                                 │
│     │ Memoria real (heap)  │                                 │
│     │ [allocated][poison]  │                                 │
│     │     ↕  (mapeo 8:1)   │                                 │
│     │ [shadow memory]      │                                 │
│     │ [00][FF]             │                                 │
│     └──────────────────────┘                                 │
│     00 = accesible, FF = envenenado (no accesible)           │
│                                                              │
│  2. INSTRUMENTACIÓN                                          │
│     Cada acceso a memoria (load/store) → verificar shadow    │
│     if (shadow[addr/8] != 0) → REPORT ERROR                 │
│                                                              │
│  3. REDZONES                                                 │
│     Alrededor de cada allocación, insertar "redzones"        │
│     (zonas envenenadas) para detectar out-of-bounds          │
│                                                              │
│     [redzone][tu buffer][redzone]                            │
│     [poison ][ok      ][poison ]                             │
│                                                              │
│  4. QUARANTINE                                               │
│     Después de free(), la memoria no se reutiliza            │
│     inmediatamente → detectar use-after-free                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 7. ASan: qué detecta en Rust

### En código unsafe

| Bug | Ejemplo en Rust | Detectado |
|---|---|---|
| Heap buffer overflow | `ptr::write(ptr.add(beyond_alloc), val)` | Sí |
| Stack buffer overflow | Array en stack accedido fuera de bounds | Sí |
| Global buffer overflow | Static array accedido fuera de bounds | Sí |
| Use-after-free | `let p = Box::into_raw(b); drop(unsafe{Box::from_raw(p)}); *p` | Sí |
| Double free | `Box::from_raw(ptr)` dos veces | Sí |
| Use-after-return | Referencia a variable local después de que retorna la función | Sí (con flag) |
| Stack overflow | Recursión infinita | Sí |
| Memory leak | `Box::into_raw(b)` sin corresponiente `Box::from_raw` | Sí (LSan) |

### En código safe

| Situación | Detectado por ASan |
|---|---|
| Panic por index out of bounds | No (Rust ya lo detecta como panic) |
| Unwrap en None | No (Rust ya lo detecta como panic) |
| Integer overflow | No (Rust lo detecta con debug-assertions) |
| Memory leak por Rc cycle | Sí (LSan) |
| Memory leak por std::mem::forget | Sí (LSan) |

### En FFI

| Bug en el código C llamado desde Rust | Detectado |
|---|---|
| Todos los bugs de ASan en C | Sí, si el código C también se compila con ASan |
| Buffer overflow en librería C | Sí, si la librería se compiló con ASan |
| Use-after-free en código C | Sí |

---

## 8. ASan: compilar y ejecutar manualmente

### Sin cargo-fuzz (para entender qué pasa internamente)

```bash
# Paso 1: compilar con ASan
RUSTFLAGS="-Zsanitizer=address" \
    cargo +nightly build --release

# Paso 2: ejecutar
./target/release/my_program

# Si hay un bug, ASan imprime un reporte y aborta
```

### Ejemplo mínimo

```rust
// src/main.rs
fn main() {
    unsafe {
        let layout = std::alloc::Layout::from_size_align(10, 1).unwrap();
        let ptr = std::alloc::alloc(layout);

        // Bug: escribir fuera del buffer
        *ptr.add(15) = 42;  // 15 > 10 → heap-buffer-overflow

        std::alloc::dealloc(ptr, layout);
    }
}
```

```bash
RUSTFLAGS="-Zsanitizer=address" cargo +nightly run --release
```

Salida:

```
=================================================================
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x60200000001f
WRITE of size 1 at 0x60200000001f thread T0
    #0 0x55a3... in my_program::main src/main.rs:7:9
    #1 0x55a3... in core::ops::function::FnOnce::call_once ...
    ...

0x60200000001f is located 5 bytes after 10-byte region [0x602000000010,0x60200000001a)
allocated by thread T0 here:
    #0 0x55a3... in __rust_alloc ...
    #1 0x55a3... in my_program::main src/main.rs:4:20

SUMMARY: AddressSanitizer: heap-buffer-overflow src/main.rs:7:9 in my_program::main
```

---

## 9. ASan con cargo-fuzz

### Uso básico (ASan es el default)

```bash
# Estos dos comandos son equivalentes:
cargo fuzz run my_target
cargo fuzz run my_target --sanitizer=address

# ASan ya está activado por defecto
```

### Qué hace cargo-fuzz internamente

```
cargo fuzz run my_target --sanitizer=address

  Internamente ejecuta:
  RUSTFLAGS="-Zsanitizer=address \
             -Cpasses=sancov-module \
             -Cllvm-args=-sanitizer-coverage-level=4 \
             -Cllvm-args=-sanitizer-coverage-inline-8bit-counters \
             -Cllvm-args=-sanitizer-coverage-pc-table \
             -Cllvm-args=-sanitizer-coverage-trace-compares" \
  cargo +nightly build --release \
      --manifest-path fuzz/Cargo.toml \
      --bin my_target

  Los RUSTFLAGS combinan:
  1. -Zsanitizer=address     → instrumentación de ASan
  2. -Cpasses=sancov-module  → instrumentación de cobertura (para libFuzzer)
  3. -Cllvm-args=...         → configuración de cobertura
```

### Desactivar ASan (más rápido)

```bash
# Sin sanitizer: ~2x más rápido
cargo fuzz run my_target --sanitizer=none
```

¿Cuándo desactivar ASan?

```
Desactivar ASan (--sanitizer=none) cuando:
  - Tu código es 100% safe Rust
  - Quieres máxima velocidad de fuzzing
  - Estás buscando panics/errores lógicos, no bugs de memoria
  - Ya fuzzeaste con ASan suficiente tiempo y ahora quieres explorar más

Mantener ASan (default) cuando:
  - Tu código tiene unsafe
  - Tu código usa FFI
  - Quieres la detección más completa posible
  - Es la primera sesión de fuzzing (no sabes qué hay)
```

---

## 10. ASan: interpretar reportes en Rust

### Anatomía de un reporte ASan

```
=================================================================
==PID==ERROR: AddressSanitizer: TIPO_DE_BUG on address DIRECCIÓN    ← 1. QUÉ
TIPO_ACCESO of size TAMAÑO at DIRECCIÓN thread THREAD
    #0 DIRECCIÓN in FUNCIÓN ARCHIVO:LÍNEA                           ← 2. DÓNDE
    #1 DIRECCIÓN in FUNCIÓN ARCHIVO:LÍNEA
    ...

DIRECCIÓN is located OFFSET bytes RELACIÓN TAMAÑO-byte region      ← 3. CONTEXTO
    [INICIO, FIN)
allocated by thread THREAD here:                                    ← 4. ALLOCACIÓN
    #0 DIRECCIÓN in FUNCIÓN ARCHIVO:LÍNEA
    ...

SUMMARY: AddressSanitizer: TIPO_DE_BUG ARCHIVO:LÍNEA in FUNCIÓN    ← 5. RESUMEN
```

### Los campos importantes

| Campo | Significado | Ejemplo |
|---|---|---|
| TIPO_DE_BUG | Clasificación del error | `heap-buffer-overflow`, `use-after-free` |
| TIPO_ACCESO | `READ` o `WRITE` | `WRITE of size 1` |
| stack trace superior | Dónde ocurrió el acceso | `src/parser.rs:42:15` |
| "is located" | Relación con la allocación | `5 bytes after 10-byte region` |
| stack trace inferior | Dónde se allocó la memoria | `src/parser.rs:30:20` |

### Tipos de error comunes

```
heap-buffer-overflow:
  Acceso fuera de los límites de un buffer en el heap.
  "5 bytes after 10-byte region" = accedió a offset 15 en un buffer de 10.

stack-buffer-overflow:
  Acceso fuera de un buffer en el stack (variable local, array).

use-after-free:
  Acceso a memoria que ya fue liberada.
  "freed by thread T0 here:" muestra dónde se liberó.

double-free:
  Intentar liberar memoria que ya fue liberada.
  "freed by thread T0 here:" muestra el primer free.

heap-use-after-free:
  Específicamente heap (Box, Vec, etc.) usado después de drop.

stack-use-after-return:
  Referencia a variable local usada después de que la función retornó.
  Requiere: ASAN_OPTIONS=detect_stack_use_after_return=1

alloc-dealloc-mismatch:
  Allocar con un allocator y deallocar con otro.

SEGV on unknown address:
  Acceso a dirección inválida (NULL deref, wild pointer).
```

### Ejemplo de lectura: heap-buffer-overflow

```
==54321==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000054
READ of size 1 at 0x602000000054 thread T0
                 ↑                              ↑
                 tipo de acceso                  thread principal

    #0 0x55a3b2c4 in my_lib::unsafe_parse::h1234
           src/unsafe_parse.rs:23:15         ← AQUÍ ESTÁ EL BUG
    #1 0x55a3b2c5 in fuzz_target::__fuzz_test_input
           fuzz/fuzz_targets/fuzz_parse.rs:6:5

0x602000000054 is located 0 bytes after 4-byte region
         [0x602000000050,0x602000000054)
         ↑                              ↑
         inicio del buffer              fin del buffer
                                        ← leyó JUSTO después del final

allocated by thread T0 here:
    #0 0x55a3b2c6 in __rust_alloc
    #1 0x55a3b2c7 in alloc::raw_vec::RawVec<T>::allocate_in
    #2 0x55a3b2c8 in my_lib::unsafe_parse::h5678
           src/unsafe_parse.rs:20:25      ← AQUÍ SE ALLOCÓ

SUMMARY: AddressSanitizer: heap-buffer-overflow
    src/unsafe_parse.rs:23:15 in my_lib::unsafe_parse
```

**Interpretación paso a paso**:

```
1. QUÉ: heap-buffer-overflow, lectura de 1 byte
2. DÓNDE: src/unsafe_parse.rs:23:15
3. CONTEXTO: leyó byte 0 después del final de un buffer de 4 bytes
   (el buffer ocupa bytes 0x50 a 0x53, y se leyó el byte 0x54)
4. ALLOCACIÓN: el buffer de 4 bytes se allocó en src/unsafe_parse.rs:20:25
5. ACCIÓN: ir a la línea 23, ver qué puntero se está leyendo
   y por qué el índice excede el tamaño del buffer
```

---

## 11. ASan: ejemplo completo con unsafe

### Código con bugs

```rust
// src/lib.rs
use std::alloc::{alloc, dealloc, Layout};
use std::ptr;

pub struct RawBuffer {
    ptr: *mut u8,
    len: usize,
    capacity: usize,
}

impl RawBuffer {
    pub fn new(capacity: usize) -> Self {
        let layout = Layout::array::<u8>(capacity).unwrap();
        let ptr = unsafe { alloc(layout) };
        if ptr.is_null() {
            std::alloc::handle_alloc_error(layout);
        }
        RawBuffer { ptr, len: 0, capacity }
    }

    pub fn push(&mut self, byte: u8) {
        // BUG 1: no verifica capacity antes de escribir
        unsafe {
            ptr::write(self.ptr.add(self.len), byte);
        }
        self.len += 1;
    }

    pub fn get(&self, index: usize) -> u8 {
        // BUG 2: no verifica que index < len
        unsafe {
            ptr::read(self.ptr.add(index))
        }
    }

    pub fn as_slice(&self) -> &[u8] {
        unsafe {
            std::slice::from_raw_parts(self.ptr, self.len)
        }
    }
}

impl Drop for RawBuffer {
    fn drop(&mut self) {
        let layout = Layout::array::<u8>(self.capacity).unwrap();
        unsafe {
            dealloc(self.ptr, layout);
        }
    }
}

// BUG 3: Clone copia el puntero, causando double-free
impl Clone for RawBuffer {
    fn clone(&self) -> Self {
        RawBuffer {
            ptr: self.ptr,  // ← copia el puntero, no los datos
            len: self.len,
            capacity: self.capacity,
        }
    }
}
```

### Harness

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_lib::RawBuffer;

fuzz_target!(|data: &[u8]| {
    if data.is_empty() {
        return;
    }

    let capacity = (data[0] as usize % 16) + 1;  // 1-16
    let mut buf = RawBuffer::new(capacity);

    for &byte in &data[1..] {
        buf.push(byte);  // BUG 1: overflow si data.len()-1 > capacity
    }

    // Leer todos los bytes escritos
    for i in 0..buf.as_slice().len() {
        let _ = buf.get(i);
    }

    // BUG 2: leer fuera de bounds
    if data.len() > 3 {
        let bad_index = data[1] as usize;
        let _ = buf.get(bad_index);  // puede ser > len
    }

    // BUG 3: clone + drop = double-free
    if data.len() > 5 && data[2] > 128 {
        let _clone = buf.clone();
        // Al final del scope: buf.drop() y _clone.drop()
        // → double-free del mismo puntero
    }
});
```

### Compilar y ejecutar

```bash
# ASan está activado por defecto
cargo fuzz run fuzz_raw_buffer -- -max_total_time=60

# El fuzzer encontrará rápidamente:
# 1. heap-buffer-overflow (BUG 1: push sin verificar capacity)
# 2. heap-buffer-overflow (BUG 2: get sin verificar bounds)
# 3. double-free (BUG 3: Clone que copia puntero)
```

### Reportes esperados

**Bug 1 (heap-buffer-overflow en push)**:
```
ERROR: AddressSanitizer: heap-buffer-overflow
WRITE of size 1
    #0 ... in my_lib::RawBuffer::push src/lib.rs:23:13
```

**Bug 3 (double-free)**:
```
ERROR: AddressSanitizer: attempting double-free on 0x602000000010
    #0 ... in __rust_dealloc
    #1 ... in <my_lib::RawBuffer as core::ops::drop::Drop>::drop src/lib.rs:43:13
```

---

## 12. ASan: ejemplo con FFI (llamando C desde Rust)

### Escenario: parsear datos con una librería C

```c
// c_parser.h
#ifndef C_PARSER_H
#define C_PARSER_H

#include <stddef.h>
#include <stdint.h>

// Parsea un buffer y retorna los bytes procesados.
// out_buf debe tener al menos in_len bytes.
int c_parse(const uint8_t *in_buf, size_t in_len,
            uint8_t *out_buf, size_t out_len);

#endif
```

```c
// c_parser.c
#include "c_parser.h"
#include <string.h>

int c_parse(const uint8_t *in_buf, size_t in_len,
            uint8_t *out_buf, size_t out_len) {
    // BUG: no verifica que out_len >= in_len
    // Si out_len < in_len → buffer overflow
    memcpy(out_buf, in_buf, in_len);

    // Procesar los datos copiados
    for (size_t i = 0; i < in_len; i++) {
        out_buf[i] ^= 0xFF;
    }

    return (int)in_len;
}
```

### Bindings en Rust

```rust
// src/lib.rs
extern "C" {
    fn c_parse(
        in_buf: *const u8,
        in_len: usize,
        out_buf: *mut u8,
        out_len: usize,
    ) -> i32;
}

pub fn safe_parse(input: &[u8]) -> Vec<u8> {
    let mut output = vec![0u8; input.len()];

    let result = unsafe {
        c_parse(
            input.as_ptr(),
            input.len(),
            output.as_mut_ptr(),
            output.len(),
        )
    };

    if result < 0 {
        return Vec::new();
    }

    output.truncate(result as usize);
    output
}

// Función con bug: buffer de salida demasiado pequeño
pub fn buggy_parse(input: &[u8]) -> Vec<u8> {
    // BUG: output buffer es la mitad del input
    let out_len = input.len() / 2;
    let mut output = vec![0u8; out_len];

    let _result = unsafe {
        c_parse(
            input.as_ptr(),
            input.len(),        // in_len = input.len()
            output.as_mut_ptr(),
            out_len,            // out_len = input.len() / 2
            // → C hace memcpy(out, in, in_len) → overflow!
        )
    };

    output
}
```

### Build script para compilar el C con ASan

```rust
// build.rs
fn main() {
    cc::Build::new()
        .file("c_parser.c")
        .flag("-fsanitize=address")  // ¡IMPORTANTE! El C también necesita ASan
        .flag("-fno-omit-frame-pointer")
        .compile("c_parser");
}
```

```toml
# Cargo.toml
[build-dependencies]
cc = "1"
```

### Harness

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_lib::buggy_parse;

fuzz_target!(|data: &[u8]| {
    if data.len() < 4 || data.len() > 1024 {
        return;
    }
    let _ = buggy_parse(data);
});
```

### ¿ASan detecta el bug en el código C?

Sí, porque:

1. El código Rust se compila con `-Zsanitizer=address`
2. El código C se compila con `-fsanitize=address` (en build.rs)
3. Ambos comparten el mismo runtime de ASan
4. El `memcpy` en C accede a la shadow memory de ASan
5. ASan detecta el overflow en el heap allocado por Rust

```
==12345==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602...
WRITE of size 1 at 0x602... thread T0
    #0 0x55a3... in memcpy ← en la librería C
    #1 0x55a3... in c_parse c_parser.c:8:5
    #2 0x55a3... in my_lib::buggy_parse src/lib.rs:30:9
    #3 0x55a3... in fuzz_target::__fuzz_test_input ...
```

---

## 13. MemorySanitizer (MSan) en Rust

MSan detecta uso de **memoria no inicializada**: leer bytes que fueron allocados pero nunca escritos.

### Por qué importa en Rust

En safe Rust, toda la memoria se inicializa antes de usarse. Pero en unsafe:

```rust
// Safe Rust: SIEMPRE inicializado
let x: u32 = 0;                    // Inicializado a 0
let v: Vec<u8> = Vec::new();       // Inicializado a Vec vacío
let b: Box<u8> = Box::new(42);     // Inicializado a 42

// Unsafe Rust: PUEDE ser no inicializado
unsafe {
    let mut x: u32 = std::mem::uninitialized();  // ← UB!
    // (deprecated, usar MaybeUninit)

    let mut buf: std::mem::MaybeUninit<[u8; 1024]> = std::mem::MaybeUninit::uninit();
    let ptr = buf.as_mut_ptr() as *mut u8;
    // ptr[0..1024] no está inicializado
    // Leer sin escribir primero → UB

    let val = *ptr;  // ← MSan detecta esto: lectura de memoria no inicializada
}
```

---

## 14. MSan: qué detecta

### Comportamiento de MSan

```
┌──────────────────────────────────────────────────────────────┐
│                 Cómo funciona MSan                            │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Shadow memory: cada byte tiene un bit "inicializado/no"     │
│                                                              │
│  1. Allocación: marcar como NO inicializado                  │
│  2. Escritura: marcar como INICIALIZADO                      │
│  3. Lectura:  verificar que está INICIALIZADO                │
│              si no → ERROR                                   │
│                                                              │
│  Ejemplo:                                                    │
│  let buf = alloc(10);                                        │
│  shadow:  [UUUUUUUUUU]   U = no inicializado                │
│                                                              │
│  buf[0] = 42;                                                │
│  shadow:  [IUUUUUUUUU]   I = inicializado                   │
│                                                              │
│  let x = buf[0];  → OK (I)                                  │
│  let y = buf[1];  → ERROR (U) ← MSan reporta aquí           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Bugs detectados por MSan

| Bug | Ejemplo | Peligro |
|---|---|---|
| Leer MaybeUninit sin escribir | `MaybeUninit::uninit().assume_init()` | UB, resultado impredecible |
| Buffer parcialmente inicializado | `alloc(100)`, escribir 50, leer 100 | Info leak, UB |
| Struct con padding no inicializado | Acceder al padding de un struct | UB |
| FFI: C retorna datos no inicializados | `calloc` vs `malloc` confusión | Info leak |

---

## 15. MSan: compilar y ejecutar

### Compilar con MSan

```bash
RUSTFLAGS="-Zsanitizer=memory" \
    cargo +nightly build --release --target x86_64-unknown-linux-gnu
```

### Nota importante sobre --target

MSan requiere especificar el target triple explícitamente porque necesita compilar las dependencias (incluyendo core/std) desde source con MSan:

```bash
# MSan necesita compilar std con instrumentación
# Esto puede requerir más configuración que ASan
RUSTFLAGS="-Zsanitizer=memory" \
    cargo +nightly build --release \
    --target x86_64-unknown-linux-gnu \
    -Zbuild-std
```

### El flag -Zbuild-std

`-Zbuild-std` recompila la librería estándar con las mismas flags de instrumentación. Es necesario para MSan porque la stdlib pre-compilada no tiene instrumentación de MSan:

```bash
# Sin -Zbuild-std: falsos positivos porque std no está instrumentada
# Con -Zbuild-std: preciso, sin falsos positivos por std

RUSTFLAGS="-Zsanitizer=memory -Zsanitizer-memory-track-origins" \
    cargo +nightly build --release \
    --target x86_64-unknown-linux-gnu \
    -Zbuild-std
```

### -Zsanitizer-memory-track-origins

Este flag adicional hace que MSan rastree el **origen** de la memoria no inicializada:

```
Sin track-origins:
  WARNING: MemorySanitizer: use-of-uninitialized-value
      #0 in my_func src/lib.rs:15:10
  → Sabes DÓNDE se leyó, pero no dónde se allocó

Con track-origins:
  WARNING: MemorySanitizer: use-of-uninitialized-value
      #0 in my_func src/lib.rs:15:10
  Uninitialized value was created by a heap allocation
      #0 in __rust_alloc
      #1 in my_func src/lib.rs:10:20
  → Sabes DÓNDE se leyó Y dónde se allocó sin inicializar
```

---

## 16. MSan: la complicación de la stdlib

### El problema

MSan necesita que **todo** el código en el proceso esté instrumentado. Si una función de la stdlib (que no tiene instrumentación) escribe en un buffer, MSan no ve la escritura y cree que el buffer sigue sin inicializar → **falso positivo**.

```
Ejemplo de falso positivo sin -Zbuild-std:

  let mut buf = vec![0u8; 10];     // Vec::new internamente llama a alloc
  std::io::stdin().read(&mut buf); // read() en std no está instrumentada
  let x = buf[0];                  // MSan: "¡no inicializado!" (falso positivo)
                                   // Porque read() escribió pero MSan no lo vio
```

### La solución: -Zbuild-std

```bash
# Recompilar std con MSan
RUSTFLAGS="-Zsanitizer=memory" \
    cargo +nightly build --release \
    --target x86_64-unknown-linux-gnu \
    -Zbuild-std
```

Esto recompila `core`, `alloc` y `std` con la instrumentación de MSan. El resultado es:
- Las allocaciones internas de std se rastrean correctamente
- No hay falsos positivos por std
- Compilación más lenta (porque recompila std)

### Implicación para FFI

Si tu código llama a una librería C, esa librería **también** debe compilarse con MSan:

```rust
// build.rs
fn main() {
    cc::Build::new()
        .file("my_c_code.c")
        .flag("-fsanitize=memory")          // MSan para el código C
        .flag("-fno-omit-frame-pointer")
        .compile("my_c_code");
}
```

Si la librería C no está instrumentada → falsos positivos en cualquier dato que pase por ella.

---

## 17. MSan con cargo-fuzz

### Uso

```bash
cargo fuzz run my_target --sanitizer=memory
```

### Lo que cargo-fuzz hace internamente

```bash
RUSTFLAGS="-Zsanitizer=memory \
           -Cpasses=sancov-module \
           -Cllvm-args=-sanitizer-coverage-level=4 \
           ..." \
    cargo +nightly build --release \
    --manifest-path fuzz/Cargo.toml \
    --bin my_target \
    --target x86_64-unknown-linux-gnu \
    -Zbuild-std
```

### Cuándo usar MSan con cargo-fuzz

```
Usar MSan cuando tu código tiene:
  ✓ unsafe con MaybeUninit
  ✓ unsafe con ptr::read en buffers que podrían no estar completamente escritos
  ✓ FFI con código C que usa malloc (no calloc)
  ✓ FFI con código C que retorna structs parcialmente inicializados
  ✓ Implementaciones de allocators custom

NO usar MSan cuando:
  ✗ Todo es safe Rust (imposible tener memoria no inicializada)
  ✗ Ya encontraste el bug con ASan (priorizar arreglar antes de buscar más)
```

---

## 18. MSan: interpretar reportes

### Formato del reporte

```
==12345==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x55a3... in my_lib::process_buffer src/lib.rs:25:15
    #1 0x55a3... in fuzz_target::__fuzz_test_input ...
    ...

  Uninitialized value was created by a heap allocation
    #0 0x55a3... in __rust_alloc
    #1 0x55a3... in alloc::raw_vec::RawVec<T>::allocate_in ...
    #2 0x55a3... in my_lib::process_buffer src/lib.rs:20:25

SUMMARY: MemorySanitizer: use-of-uninitialized-value
    src/lib.rs:25:15 in my_lib::process_buffer
```

### Cómo leer el reporte

```
Parte 1: "use-of-uninitialized-value"
  → Se leyó memoria que nunca fue escrita

Parte 2: stack trace superior
  → src/lib.rs:25:15 — DÓNDE se leyó el valor

Parte 3: "Uninitialized value was created by"
  → src/lib.rs:20:25 — DÓNDE se allocó sin inicializar
  (solo con -Zsanitizer-memory-track-origins)

Diagnóstico:
  1. Ir a la línea 20: ver la allocación
  2. Ir a la línea 25: ver la lectura
  3. Verificar que entre 20 y 25, todos los bytes leídos
     en la línea 25 fueron previamente escritos
  4. Si no → ese es el bug
```

---

## 19. MSan: ejemplo completo

### Código con bug

```rust
// src/lib.rs
use std::mem::MaybeUninit;

pub struct PacketHeader {
    pub version: u8,
    pub flags: u8,
    pub length: u16,
    pub checksum: u32,
}

pub fn parse_header(data: &[u8]) -> Option<PacketHeader> {
    if data.len() < 4 {
        return None;
    }

    unsafe {
        let mut header: MaybeUninit<PacketHeader> = MaybeUninit::uninit();
        let ptr = header.as_mut_ptr();

        // Escribir los primeros campos
        (*ptr).version = data[0];
        (*ptr).flags = data[1];
        (*ptr).length = u16::from_le_bytes([data[2], data[3]]);

        // BUG: no inicializar checksum si data.len() < 8
        if data.len() >= 8 {
            (*ptr).checksum = u32::from_le_bytes([
                data[4], data[5], data[6], data[7]
            ]);
        }
        // Si data.len() < 8, checksum queda sin inicializar

        let header = header.assume_init();  // ← UB si checksum no fue escrito
        Some(header)
    }
}

pub fn validate_header(header: &PacketHeader) -> bool {
    // Esta función LEE checksum → MSan detecta el valor no inicializado
    if header.version == 0 {
        return false;
    }
    // BUG: accede a checksum que podría no estar inicializado
    header.checksum != 0  // ← MSan: use-of-uninitialized-value
}
```

### Harness

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use my_lib::{parse_header, validate_header};

fuzz_target!(|data: &[u8]| {
    if let Some(header) = parse_header(data) {
        let _ = validate_header(&header);
    }
});
```

### Ejecutar con MSan

```bash
cargo fuzz run fuzz_header --sanitizer=memory -- -max_total_time=60
```

### Reporte esperado

```
==54321==WARNING: MemorySanitizer: use-of-uninitialized-value
    #0 0x55a3... in my_lib::validate_header src/lib.rs:35:5
    #1 0x55a3... in fuzz_header::__fuzz_test_input ...

  Uninitialized value was created by a heap allocation
    #0 0x55a3... in my_lib::parse_header src/lib.rs:16:54
```

### La corrección

```rust
pub fn parse_header(data: &[u8]) -> Option<PacketHeader> {
    if data.len() < 4 {
        return None;
    }

    // Corrección: inicializar completamente SIEMPRE
    let header = PacketHeader {
        version: data[0],
        flags: data[1],
        length: u16::from_le_bytes([data[2], data[3]]),
        checksum: if data.len() >= 8 {
            u32::from_le_bytes([data[4], data[5], data[6], data[7]])
        } else {
            0  // Valor por defecto explícito
        },
    };

    Some(header)
}
```

---

## 20. ThreadSanitizer (TSan) en Rust

TSan detecta **data races**: accesos concurrentes a la misma memoria sin sincronización, donde al menos uno es una escritura.

### Relevancia en Rust

```
Safe Rust:
  El tipo system (Send, Sync, borrow checker) previene data races
  en tiempo de compilación.

  Arc<Mutex<T>>  → no es posible data race
  &T compartido   → solo lectura, no data race
  &mut T          → exclusivo, no data race

Unsafe Rust:
  Raw pointers (*mut T) son Send por default.
  Si compartes un *mut T entre threads sin sincronización → data race.
  TSan lo detecta.

FFI:
  Código C con pthreads no tiene protecciones de data race.
  Si llamas funciones C concurrentes desde Rust → TSan es útil.
```

---

## 21. TSan: qué detecta

### Tipos de data races

```
Data race:
  Thread 1: escribe a X
  Thread 2: lee o escribe a X
  Sin sincronización entre los dos accesos
  → Comportamiento indefinido (UB)
```

### Ejemplos en Rust unsafe

```rust
// Data race con raw pointer
use std::thread;

static mut COUNTER: u64 = 0;

fn main() {
    let handles: Vec<_> = (0..4).map(|_| {
        thread::spawn(|| {
            for _ in 0..1000 {
                unsafe {
                    COUNTER += 1;  // ← data race: múltiples threads escriben
                }
            }
        })
    }).collect();

    for h in handles {
        h.join().unwrap();
    }

    unsafe {
        println!("Counter: {}", COUNTER);  // Valor indeterminado
    }
}
```

### Qué NO es un data race (TSan no lo reporta)

```rust
// Esto NO es data race: Mutex sincroniza
use std::sync::{Arc, Mutex};

let counter = Arc::new(Mutex::new(0u64));
let handles: Vec<_> = (0..4).map(|_| {
    let c = counter.clone();
    thread::spawn(move || {
        for _ in 0..1000 {
            *c.lock().unwrap() += 1;  // ← Mutex protege → no data race
        }
    })
}).collect();

// Esto NO es data race: AtomicU64 sincroniza
use std::sync::atomic::{AtomicU64, Ordering};
static COUNTER: AtomicU64 = AtomicU64::new(0);
// COUNTER.fetch_add(1, Ordering::SeqCst); ← atómico → no data race
```

---

## 22. TSan: compilar y ejecutar

### Compilar con TSan

```bash
RUSTFLAGS="-Zsanitizer=thread" \
    cargo +nightly build --release --target x86_64-unknown-linux-gnu
```

### Ejecutar

```bash
./target/x86_64-unknown-linux-gnu/release/my_program
```

### Nota sobre TSan vs ASan

```
TSan y ASan son INCOMPATIBLES — no puedes usar ambos al mismo tiempo.
Esto es una limitación de LLVM, no de Rust.

Estrategia:
  Sesión 1: cargo fuzz run --sanitizer=address   ← bugs de memoria
  Sesión 2: cargo fuzz run --sanitizer=thread     ← data races

No hay forma de combinarlos en una sola ejecución.
```

---

## 23. TSan con cargo-fuzz

### Uso

```bash
cargo fuzz run my_target --sanitizer=thread
```

### Cuándo usar TSan con fuzzing

TSan con fuzzing es relevante solo cuando tu harness o el código bajo test usa concurrencia:

```rust
// ✓ TSan tiene sentido aquí: el harness crea threads
fuzz_target!(|data: &[u8]| {
    let shared = Arc::new(my_lib::SharedState::new());

    let t1 = {
        let s = shared.clone();
        let d = data.to_vec();
        std::thread::spawn(move || {
            s.process(&d);
        })
    };

    let t2 = {
        let s = shared.clone();
        std::thread::spawn(move || {
            s.get_result();
        })
    };

    t1.join().unwrap();
    t2.join().unwrap();
});

// ✗ TSan NO tiene sentido aquí: no hay concurrencia
fuzz_target!(|data: &[u8]| {
    let _ = my_lib::parse(data);  // single-threaded
});
```

### Advertencia: TSan ralentiza significativamente

TSan es el sanitizer más lento. La instrumentación de cada acceso a memoria para detectar races tiene un costo alto:

```
Sin sanitizer:  ~10000 exec/s
Con ASan:       ~3000  exec/s  (3x más lento)
Con TSan:       ~500   exec/s  (20x más lento)
```

Por esto, las sesiones de fuzzing con TSan deben ser más cortas y focalizadas.

---

## 24. TSan: interpretar reportes

### Formato del reporte

```
==================
WARNING: ThreadSanitizer: data race (pid=12345)
  Write of size 8 at 0x55a3b2c4d5e6 by thread T2:
    #0 my_lib::SharedState::process src/lib.rs:42:15
    #1 fuzz_target::__fuzz_test_input::{{closure}} ...

  Previous read of size 8 at 0x55a3b2c4d5e6 by thread T1:
    #0 my_lib::SharedState::get_result src/lib.rs:50:20
    #1 fuzz_target::__fuzz_test_input::{{closure}} ...

  Location is global 'my_lib::SHARED_STATE' of size 8 at 0x55a3b2c4d5e6
    (my_program+0x123456)

  Thread T2 (tid=67890, running) created by main thread at:
    #0 std::thread::spawn ...
    #1 fuzz_target::__fuzz_test_input ...

  Thread T1 (tid=67891, running) created by main thread at:
    #0 std::thread::spawn ...
    #1 fuzz_target::__fuzz_test_input ...

SUMMARY: ThreadSanitizer: data race src/lib.rs:42:15 in my_lib::SharedState::process
==================
```

### Cómo leer el reporte

```
Parte 1: "Write of size 8 by thread T2"
  → Thread T2 ESCRIBIÓ en la dirección 0x55a3...
  → En src/lib.rs:42:15

Parte 2: "Previous read of size 8 by thread T1"
  → Thread T1 LEYÓ la misma dirección
  → En src/lib.rs:50:20
  → Sin sincronización entre T1 y T2

Parte 3: "Location is global 'SHARED_STATE'"
  → La variable accedida es un global (static mut)

Parte 4: Thread creation info
  → Dónde se crearon T1 y T2

Diagnóstico:
  1. Dos threads acceden a la misma memoria
  2. Al menos uno escribe
  3. No hay sincronización (Mutex, Atomic, etc.)
  4. → Data race → UB
  5. Solución: agregar sincronización
```

---

## 25. TSan: ejemplo completo

### Código con data race

```rust
// src/lib.rs
use std::ptr;
use std::sync::Arc;

pub struct UnsafeCounter {
    // Internamente usa raw pointer compartido entre threads
    ptr: *mut u64,
}

// UNSAFE: marcamos Send/Sync manualmente
unsafe impl Send for UnsafeCounter {}
unsafe impl Sync for UnsafeCounter {}

impl UnsafeCounter {
    pub fn new() -> Self {
        let val = Box::new(0u64);
        UnsafeCounter {
            ptr: Box::into_raw(val),
        }
    }

    pub fn increment(&self) {
        unsafe {
            // DATA RACE: múltiples threads pueden llamar esto
            // concurrentemente sin sincronización
            let current = ptr::read(self.ptr);
            ptr::write(self.ptr, current + 1);
        }
    }

    pub fn get(&self) -> u64 {
        unsafe {
            ptr::read(self.ptr)  // DATA RACE: lee mientras otro thread escribe
        }
    }
}

impl Drop for UnsafeCounter {
    fn drop(&mut self) {
        unsafe {
            let _ = Box::from_raw(self.ptr);
        }
    }
}
```

### Harness

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use std::sync::Arc;
use std::thread;

fuzz_target!(|data: &[u8]| {
    if data.len() < 2 {
        return;
    }

    let num_threads = (data[0] % 4) as usize + 2;  // 2-5 threads
    let iterations = (data[1] as usize % 100) + 1;   // 1-100 iteraciones

    let counter = Arc::new(my_lib::UnsafeCounter::new());

    let handles: Vec<_> = (0..num_threads)
        .map(|_| {
            let c = counter.clone();
            thread::spawn(move || {
                for _ in 0..iterations {
                    c.increment();
                    let _ = c.get();
                }
            })
        })
        .collect();

    for h in handles {
        h.join().unwrap();
    }
});
```

### Ejecutar

```bash
cargo fuzz run fuzz_counter --sanitizer=thread -- -max_total_time=30
```

TSan detectará rápidamente el data race en `increment` y `get`.

---

## 26. Leak Sanitizer (LSan) en Rust

### Qué es

LSan detecta **memory leaks**: memoria allocada que nunca se libera. En Rust, los leaks son más raros que en C, pero todavía son posibles:

### Cómo ocurren leaks en Rust

```rust
// 1. Box::into_raw sin corresponiente Box::from_raw
let b = Box::new(42u32);
let ptr = Box::into_raw(b);
// ptr nunca se convierte de vuelta a Box → leak de 4 bytes

// 2. std::mem::forget
let v = vec![1, 2, 3, 4, 5];
std::mem::forget(v);
// Vec nunca ejecuta drop → leak del buffer interno

// 3. Rc cycle (referencia circular)
use std::rc::Rc;
use std::cell::RefCell;

struct Node {
    next: Option<Rc<RefCell<Node>>>,
}

let a = Rc::new(RefCell::new(Node { next: None }));
let b = Rc::new(RefCell::new(Node { next: Some(a.clone()) }));
a.borrow_mut().next = Some(b.clone());
// a → b → a → b → ... → nunca se libera

// 4. FFI: código C que hace malloc sin free
extern "C" { fn c_function_that_leaks(data: *const u8, len: usize); }
```

### LSan con cargo-fuzz

LSan está **incluido en ASan** por defecto. No necesitas activarlo explícitamente:

```bash
# ASan incluye LSan
cargo fuzz run my_target --sanitizer=address

# Para desactivar LSan dentro de ASan:
ASAN_OPTIONS=detect_leaks=0 cargo fuzz run my_target --sanitizer=address

# LSan standalone (sin ASan):
cargo fuzz run my_target --sanitizer=leak
```

### Reporte de LSan

```
==12345==ERROR: LeakSanitizer: detected memory leaks

Direct leak of 4 byte(s) in 1 object(s) allocated from:
    #0 0x55a3... in __rust_alloc
    #1 0x55a3... in alloc::alloc::exchange_malloc
    #2 0x55a3... in alloc::boxed::Box<T>::new
    #3 0x55a3... in my_lib::create_and_leak src/lib.rs:10:15

SUMMARY: AddressSanitizer: 4 byte(s) leaked in 1 allocation(s).
```

### Precaución: leaks en fuzzing in-process

En fuzzing in-process (libFuzzer), los leaks se acumulan entre ejecuciones:

```
Ejecución 1: leak 100 bytes (total: 100 bytes)
Ejecución 2: leak 100 bytes (total: 200 bytes)
...
Ejecución 1000: leak 100 bytes (total: 100 KB)
...
Ejecución 100000: leak 100 bytes (total: 10 MB)
→ Eventualmente: OOM

Por eso LSan es importante en fuzzing:
un leak de 100 bytes por ejecución, multiplicado por
millones de ejecuciones = OOM.
```

---

## 27. Estrategia multi-sanitizer con cargo-fuzz

No puedes usar múltiples sanitizers simultáneamente (son incompatibles a nivel de LLVM). La estrategia es ejecutar sesiones secuenciales:

### Flujo recomendado

```
┌──────────────────────────────────────────────────────────────────┐
│             Estrategia multi-sanitizer para fuzzing               │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Paso 1: ASan (prioridad máxima)                                 │
│  ──────────────────────────────                                  │
│  cargo fuzz run my_target -- -max_total_time=3600                │
│                                                                  │
│  → Detecta: buffer overflow, use-after-free, double-free, leaks  │
│  → Incluye LSan por defecto                                      │
│  → Impacto en velocidad: ~3x más lento                          │
│                                                                  │
│  Paso 2: Sin sanitizer (máxima velocidad)                        │
│  ────────────────────────────────────────                        │
│  cargo fuzz run my_target --sanitizer=none -- -max_total_time=3600│
│                                                                  │
│  → Detecta: panics, errores lógicos, timeouts, OOM              │
│  → Máxima velocidad: más cobertura en menos tiempo               │
│  → Usar el corpus de ASan como punto de partida                  │
│                                                                  │
│  Paso 3: MSan (si hay unsafe/FFI)                                │
│  ────────────────────────────────                                │
│  cargo fuzz run my_target --sanitizer=memory -- -max_total_time=1800│
│                                                                  │
│  → Detecta: uso de memoria no inicializada                       │
│  → Requiere -Zbuild-std (compilación lenta)                      │
│  → Impacto en velocidad: ~3x más lento                          │
│                                                                  │
│  Paso 4: TSan (si hay concurrencia unsafe)                       │
│  ─────────────────────────────────────────                       │
│  cargo fuzz run my_target --sanitizer=thread -- -max_total_time=900│
│                                                                  │
│  → Detecta: data races                                           │
│  → Solo si el harness usa threads                                │
│  → Impacto en velocidad: ~20x más lento                         │
│                                                                  │
│  Paso 5: Merge de corpus                                         │
│  ───────────────────────                                         │
│  Cada sesión puede descubrir inputs nuevos.                      │
│  Merge todos los corpus al final:                                │
│  cargo fuzz cmin my_target                                       │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

### Script para sesiones multi-sanitizer

```bash
#!/bin/bash
# fuzz_multi_sanitizer.sh

TARGET=${1:-my_target}
DURATION=${2:-600}  # 10 minutos por default

echo "=== Paso 1: ASan ($DURATION seconds) ==="
cargo fuzz run "$TARGET" --sanitizer=address -- -max_total_time=$DURATION

echo "=== Paso 2: Sin sanitizer ($DURATION seconds) ==="
cargo fuzz run "$TARGET" --sanitizer=none -- -max_total_time=$DURATION

# Solo si hay unsafe/FFI:
if grep -rq "unsafe" src/; then
    echo "=== Paso 3: MSan ($DURATION seconds) ==="
    cargo fuzz run "$TARGET" --sanitizer=memory -- -max_total_time=$DURATION
fi

# Solo si hay threads en el harness:
if grep -rq "thread::spawn\|std::thread" fuzz/fuzz_targets/; then
    echo "=== Paso 4: TSan ($((DURATION / 2)) seconds) ==="
    cargo fuzz run "$TARGET" --sanitizer=thread -- -max_total_time=$((DURATION / 2))
fi

echo "=== Paso 5: Merge ==="
cargo fuzz cmin "$TARGET"

echo "=== Resultados ==="
crashes=$(ls fuzz/artifacts/"$TARGET"/crash-* 2>/dev/null | wc -l)
echo "Crashes encontrados: $crashes"
```

---

## 28. RUSTFLAGS: la variable de entorno clave

`RUSTFLAGS` es la variable de entorno que pasa flags adicionales al compilador de Rust. Es el mecanismo principal para activar sanitizers cuando no usas cargo-fuzz.

### Syntax y uso

```bash
# Un solo flag:
RUSTFLAGS="-Zsanitizer=address" cargo +nightly build

# Múltiples flags:
RUSTFLAGS="-Zsanitizer=address -Copt-level=3 -Cdebuginfo=2" \
    cargo +nightly build

# Con cargo test (para tests unitarios con sanitizer):
RUSTFLAGS="-Zsanitizer=address" cargo +nightly test
```

### Flags relevantes para fuzzing con sanitizers

| Flag | Propósito |
|---|---|
| `-Zsanitizer=address` | Activar ASan |
| `-Zsanitizer=memory` | Activar MSan |
| `-Zsanitizer=thread` | Activar TSan |
| `-Zsanitizer=leak` | Activar LSan standalone |
| `-Zsanitizer-memory-track-origins` | Rastrear origen de memoria no inicializada (MSan) |
| `-Copt-level=3` | Optimización máxima (velocidad) |
| `-Cdebuginfo=2` | Información de debug completa (stack traces legibles) |
| `-Cpasses=sancov-module` | Instrumentación de cobertura (para libFuzzer) |

### RUSTFLAGS vs cargo-fuzz --sanitizer

```
cargo fuzz run my_target --sanitizer=address
  ↕ equivalente a ↕
RUSTFLAGS="-Zsanitizer=address -Cpasses=sancov-module ..." \
    cargo +nightly build ... && ./target/.../my_target

cargo-fuzz es más conveniente porque:
1. Configura todos los flags automáticamente
2. Gestiona el directorio de corpus
3. Gestiona los artifacts
4. Maneja nightly automáticamente
```

### RUSTFLAGS con tests unitarios

Puedes ejecutar tests unitarios con sanitizers sin cargo-fuzz:

```bash
# Ejecutar todos los tests con ASan
RUSTFLAGS="-Zsanitizer=address" cargo +nightly test

# Ejecutar un test específico con ASan
RUSTFLAGS="-Zsanitizer=address" cargo +nightly test test_name

# Con MSan (necesita -Zbuild-std):
RUSTFLAGS="-Zsanitizer=memory" \
    cargo +nightly test \
    --target x86_64-unknown-linux-gnu \
    -Zbuild-std
```

Esto es útil para ejecutar **regression tests con sanitizers** sin necesitar cargo-fuzz.

---

## 29. ASAN_OPTIONS, MSAN_OPTIONS, TSAN_OPTIONS

Los sanitizers tienen variables de entorno para configurar su comportamiento en runtime.

### ASAN_OPTIONS

```bash
# Formato: VAR=val1:val2:val3
ASAN_OPTIONS="detect_leaks=1:abort_on_error=1:symbolize=1" \
    cargo fuzz run my_target

# Opciones útiles:
ASAN_OPTIONS="
  detect_leaks=1              # Activar leak detection (default: 1)
  abort_on_error=1            # Abortar en error (vs continuar)
  symbolize=1                 # Simbolizar stack traces
  print_stats=1               # Imprimir estadísticas al final
  halt_on_error=1             # Parar en el primer error
  detect_stack_use_after_return=1  # Detectar use-after-return (lento)
  quarantine_size_mb=256      # Tamaño de quarantine para UAF
  malloc_context_size=30      # Profundidad de stack traces en allocs
  check_initialization_order=1 # Verificar orden de init de globals
"
```

### MSAN_OPTIONS

```bash
MSAN_OPTIONS="
  abort_on_error=1
  symbolize=1
  print_stats=1
  halt_on_error=1
"
```

### TSAN_OPTIONS

```bash
TSAN_OPTIONS="
  abort_on_error=1
  symbolize=1
  halt_on_error=1
  history_size=7              # Tamaño del historial (0-7, más alto = más memoria)
  second_deadlock_stack=1     # Imprimir stack del segundo lock en deadlocks
  report_signal_unsafe=1     # Reportar llamadas unsafe en signal handlers
"
```

### LSAN_OPTIONS (para leak suppression)

```bash
LSAN_OPTIONS="
  suppressions=lsan_suppressions.txt  # Archivo de suppressions
  print_suppressions=1                # Imprimir qué se suprimió
  max_leaks=10                        # Máximo de leaks a reportar
"
```

---

## 30. Sanitizers y cobertura de código

### La sinergia fuzzer + sanitizer + cobertura

```
┌──────────────────────────────────────────────────────────────┐
│           La tríada: Fuzzer + Sanitizer + Cobertura          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FUZZER (libFuzzer):                                         │
│  → Genera inputs que maximizan la COBERTURA                  │
│  → Detecta: panics, timeouts, OOM                            │
│                                                              │
│  SANITIZER (ASan/MSan/TSan):                                 │
│  → Convierte bugs silenciosos en crashes DETECTABLES         │
│  → Amplifica lo que el fuzzer puede encontrar                │
│                                                              │
│  COBERTURA:                                                  │
│  → Guía al fuzzer hacia código nuevo                         │
│  → Mide qué tan bien explora el código                       │
│                                                              │
│  Juntos:                                                     │
│  Fuzzer genera input → Sanitizer verifica → Cobertura guía   │
│       ↓                      ↓                    ↓          │
│  Más inputs          Más bugs detectados    Más código        │
│  interesantes        por input              explorado         │
│                                                              │
│  Sin sanitizer: bug silencioso, el fuzzer no lo ve           │
│  Sin cobertura: fuzzer no sabe qué caminos explorar          │
│  Sin fuzzer: no hay inputs nuevos para probar                │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Medir cobertura con sanitizer activo

```bash
# Fuzzing con ASan
cargo fuzz run my_target -- -max_total_time=300

# Después: medir cobertura del corpus generado
cargo fuzz coverage my_target
```

La cobertura se mide con la instrumentación de `sancov-module`, que es independiente del sanitizer. Puedes tener ASan + cobertura sin conflicto.

---

## 31. Impacto en rendimiento

### Tabla de impacto

| Sanitizer | Slowdown CPU | Overhead RAM | Tamaño binario |
|---|---|---|---|
| Sin sanitizer | 1x (baseline) | 1x | 1x |
| ASan | 2-3x | 2-3x | 2-3x |
| MSan | 2-3x | 2-3x | 2-3x |
| TSan | 5-15x | 5-10x | 2-3x |
| LSan (en ASan) | ~0x extra | ~0x extra | Incluido en ASan |
| LSan standalone | ~0x | ~0x | 1.5x |

### Impacto en exec/s del fuzzer

```
Ejemplo: parser de JSON

Sin sanitizer:     12000 exec/s
Con ASan:           4000 exec/s  (3x más lento)
Con MSan:           3500 exec/s  (3.4x más lento)
Con TSan:            800 exec/s  (15x más lento)

¿Importa?

En 10 minutos:
  Sin sanitizer:  7,200,000 ejecuciones  → más cobertura
  Con ASan:       2,400,000 ejecuciones  → menos, pero encuentra más bugs
  Con TSan:         480,000 ejecuciones  → mucho menos, pero encuentra data races

La pregunta no es "¿cuántas ejecuciones?" sino
"¿encuentro el bug?"

Estrategia: alternar entre sin sanitizer (cobertura) y con sanitizer (detección).
```

### Optimizaciones para reducir el impacto

```bash
# 1. Siempre compilar en release (opt-level=3)
#    cargo-fuzz ya hace esto por defecto

# 2. Limitar el tamaño del input
cargo fuzz run my_target -- -max_len=1024

# 3. Usar -jobs para paralelizar
cargo fuzz run my_target -- -jobs=4 -workers=4

# 4. Para ASan: ajustar quarantine (reducir RAM)
ASAN_OPTIONS="quarantine_size_mb=64" cargo fuzz run my_target
```

---

## 32. Compatibilidad entre sanitizers

### Tabla de compatibilidad

```
              ASan   MSan   TSan   LSan
ASan           -     ✗✗     ✗✗     ✓ (incluido)
MSan          ✗✗      -     ✗✗     ✗✗
TSan          ✗✗     ✗✗      -     ✗✗
LSan           ✓     ✗✗     ✗✗      -

✓  = Compatible (se pueden usar juntos)
✗✗ = Incompatible (error de compilación o runtime)
```

### Por qué son incompatibles

```
Los sanitizers usan SHADOW MEMORY: una región de memoria
que mapea la memoria real para rastrear estado.

ASan: shadow = "esta dirección es accesible/envenenada"
MSan: shadow = "este byte está inicializado/no"
TSan: shadow = "qué thread accedió por última vez"

Cada sanitizer necesita su PROPIA shadow memory con
semántica diferente. No pueden compartirla.

Técnicamente, la shadow de ASan y MSan ocupa el MISMO
rango de direcciones virtuales → colisión → imposible.
```

### Consecuencia para fuzzing

Debes ejecutar **sesiones separadas** para cada sanitizer. No hay forma de combinarlos.

---

## 33. Falsos positivos y suppressions

### Falsos positivos comunes

| Sanitizer | Falso positivo | Causa |
|---|---|---|
| ASan | "alloc-dealloc-mismatch" | Diferentes allocators (jemalloc + system) |
| MSan | "use-of-uninitialized-value" en std | stdlib no instrumentada (falta -Zbuild-std) |
| TSan | Data race en runtime de Rust | Código interno de std/tokio con sincronización correcta pero que TSan no entiende |
| LSan | Leak en runtime de Rust | Allocaciones de inicialización que se liberan al final del proceso |

### Suppressions

Los suppressions son archivos de texto que le dicen al sanitizer que ignore ciertos reportes:

```
# asan_suppressions.txt
# Ignorar leak en la inicialización de std
leak:std::rt::init
leak:std::sys::init

# Ignorar leak en una librería de terceros
leak:openssl::*
```

```
# tsan_suppressions.txt
# Ignorar data race conocido en el runtime de tokio
race:tokio::runtime::*
```

### Usar suppressions con cargo-fuzz

```bash
# ASan suppressions (a través de LSAN_OPTIONS para leaks):
LSAN_OPTIONS="suppressions=lsan_suppressions.txt" \
    cargo fuzz run my_target

# TSan suppressions:
TSAN_OPTIONS="suppressions=tsan_suppressions.txt" \
    cargo fuzz run my_target --sanitizer=thread
```

### Cuándo suprimir vs cuándo corregir

```
SUPRIMIR cuando:
  - El reporte es un falso positivo confirmado
  - El bug está en una dependencia que no puedes modificar
  - El bug es conocido y ya tiene un issue/fix pendiente
  - Es un bug en el runtime de Rust, no en tu código

CORREGIR cuando:
  - El bug es real y está en tu código
  - El bug es un leak que puede acumularse en producción
  - SIEMPRE corregir antes de suprimir (investigar primero)
```

---

## 34. Sanitizers en C vs Rust: comparación

### Tabla comparativa

| Aspecto | C (Clang) | Rust (nightly) |
|---|---|---|
| **ASan flag** | `-fsanitize=address` | `-Zsanitizer=address` |
| **MSan flag** | `-fsanitize=memory` | `-Zsanitizer=memory` |
| **TSan flag** | `-fsanitize=thread` | `-Zsanitizer=thread` |
| **UBSan flag** | `-fsanitize=undefined` | No disponible directamente |
| **Compilador** | Clang (stable) | rustc nightly |
| **Estabilidad** | Estable, ampliamente usado | Experimental pero funcional |
| **Necesidad en safe code** | N/A (C no tiene "safe") | Baja (compilador previene bugs de memoria) |
| **Necesidad en unsafe** | Alta (todo C es "unsafe") | Alta (mismos bugs que C) |
| **stdlib instrumentada** | Sí (libc instrumentada) | Requiere `-Zbuild-std` para MSan |
| **FFI** | N/A | Código C debe compilarse con mismos flags |
| **UB detection** | UBSan cubre mucho UB | `debug-assertions` cubre overflow; para el resto, ASan |
| **Integración fuzzer** | `-fsanitize=fuzzer` | `cargo fuzz --sanitizer=X` |
| **Runtime options** | `ASAN_OPTIONS`, etc. | Mismas variables de entorno |
| **Suppressions** | `__asan_default_options()` | Variables de entorno |
| **Impacto rendimiento** | Igual (mismo backend LLVM) | Igual |

### Diferencias filosóficas

```
C:
  Los sanitizers son la PRIMERA LÍNEA de defensa.
  Sin sanitizers, la mayoría de bugs de memoria son silenciosos.
  Debes compilar con sanitizers para cualquier testing serio.

  Fuzzer sin sanitizer en C = encontrar solo crashes obvios (SEGV)
  Fuzzer CON sanitizer en C = encontrar TODOS los bugs de memoria

Rust safe:
  Los sanitizers son una SEGUNDA LÍNEA de defensa (redundante).
  El compilador ya previene bugs de memoria.
  Fuzzer sin sanitizer = encontrar panics y errores lógicos.
  Fuzzer con sanitizer = lo mismo + detectar leaks.

Rust unsafe:
  Los sanitizers son tan necesarios como en C.
  Los bloques unsafe desactivan las protecciones del compilador.
  = exactamente la misma situación que C.
```

---

## 35. Errores comunes

### Error 1: usar stable en vez de nightly

```
$ RUSTFLAGS="-Zsanitizer=address" cargo build
error: the option `Z` is only accepted on the nightly compiler

Solución: cargo +nightly build
O: cargo fuzz run (que usa nightly automáticamente)
```

### Error 2: combinar sanitizers incompatibles

```
$ RUSTFLAGS="-Zsanitizer=address -Zsanitizer=memory" cargo +nightly build
error: AddressSanitizer is incompatible with MemorySanitizer

Solución: ejecutar sesiones separadas
  Sesión 1: --sanitizer=address
  Sesión 2: --sanitizer=memory
```

### Error 3: MSan sin -Zbuild-std

```
# Sin -Zbuild-std: falsos positivos masivos
RUSTFLAGS="-Zsanitizer=memory" cargo +nightly build
# → Cualquier función de std que escribe memoria genera falso positivo

Solución: cargo fuzz run my_target --sanitizer=memory
(cargo-fuzz maneja -Zbuild-std automáticamente)
```

### Error 4: FFI sin instrumentar el código C

```rust
// build.rs
fn main() {
    cc::Build::new()
        .file("my_c_code.c")
        // ❌ Sin flags de sanitizer
        .compile("my_c_code");
}

// Resultado: ASan no detecta bugs en el código C
// porque el código C no está instrumentado

// ✓ Corrección:
fn main() {
    let mut build = cc::Build::new();
    build.file("my_c_code.c");

    // Detectar si estamos compilando con sanitizer
    if let Ok(flags) = std::env::var("RUSTFLAGS") {
        if flags.contains("sanitizer=address") {
            build.flag("-fsanitize=address");
            build.flag("-fno-omit-frame-pointer");
        }
        if flags.contains("sanitizer=memory") {
            build.flag("-fsanitize=memory");
            build.flag("-fno-omit-frame-pointer");
        }
        if flags.contains("sanitizer=thread") {
            build.flag("-fsanitize=thread");
        }
    }

    build.compile("my_c_code");
}
```

### Error 5: panic = "abort" con sanitizers

```toml
# ❌ fuzz/Cargo.toml:
[profile.release]
panic = "abort"

# Problema: ASan necesita catch_unwind para capturar panics.
# Con panic = "abort", los panics terminan el proceso inmediatamente
# y el fuzzer no puede guardar el crash como artifact.

# ✓ Corrección: no poner panic = "abort" (default es "unwind")
```

### Error 6: ignorar reportes de ASan por ser "solo leaks"

```
"Es solo un leak, no es un bug de seguridad"

EN FUZZING: un leak de 100 bytes por ejecución
× 1,000,000 ejecuciones = 100 MB leakeados = OOM

Los leaks en código fuzzeado DEBEN corregirse porque
acumulan durante la sesión de fuzzing y causan OOM.
```

### Error 7: esperar que ASan encuentre bugs en safe Rust

```
Expectativa: "Voy a fuzzear con ASan y encontrar buffer overflows"
Realidad:    Todo es safe Rust → ASan no encuentra nada

Diagnóstico:
  - Si todo es safe Rust, ASan solo aporta detección de leaks
  - Para buscar panics/lógica, usar --sanitizer=none (más rápido)
  - ASan solo aporta valor con unsafe o FFI
```

---

## 36. Programa de práctica: cache con unsafe

### Objetivo

Implementar un cache LRU usando unsafe (raw pointers para la linked list) y fuzzearlo con múltiples sanitizers.

### Especificación

```
unsafe_cache/
├── Cargo.toml
├── src/
│   └── lib.rs                ← LRU cache con unsafe
├── tests/
│   └── unit_tests.rs
└── fuzz/
    ├── Cargo.toml
    ├── fuzz_targets/
    │   ├── fuzz_ops_asan.rs  ← harness 1: operaciones con ASan
    │   ├── fuzz_ops_msan.rs  ← harness 2: operaciones con MSan
    │   └── fuzz_oracle.rs    ← harness 3: comparación diferencial
    └── corpus/
        └── fuzz_ops_asan/
```

### src/lib.rs

```rust
use std::collections::HashMap;
use std::ptr;

struct Node {
    key: u32,
    value: Vec<u8>,
    prev: *mut Node,
    next: *mut Node,
}

pub struct LruCache {
    map: HashMap<u32, *mut Node>,
    head: *mut Node,  // Most recently used
    tail: *mut Node,  // Least recently used
    capacity: usize,
    len: usize,
}

impl LruCache {
    pub fn new(capacity: usize) -> Self {
        assert!(capacity > 0);
        LruCache {
            map: HashMap::new(),
            head: ptr::null_mut(),
            tail: ptr::null_mut(),
            capacity,
            len: 0,
        }
    }

    pub fn get(&mut self, key: u32) -> Option<&[u8]> {
        let node_ptr = *self.map.get(&key)?;

        // Mover al frente (most recently used)
        self.detach(node_ptr);
        self.attach_front(node_ptr);

        unsafe {
            Some(&(*node_ptr).value)
        }
    }

    pub fn put(&mut self, key: u32, value: Vec<u8>) {
        if let Some(&node_ptr) = self.map.get(&key) {
            // Actualizar valor existente
            unsafe {
                (*node_ptr).value = value;
            }
            self.detach(node_ptr);
            self.attach_front(node_ptr);
            return;
        }

        // Insertar nuevo
        let node = Box::new(Node {
            key,
            value,
            prev: ptr::null_mut(),
            next: ptr::null_mut(),
        });
        let node_ptr = Box::into_raw(node);

        self.map.insert(key, node_ptr);
        self.attach_front(node_ptr);
        self.len += 1;

        // Evictar si excede capacidad
        if self.len > self.capacity {
            self.evict();
        }
    }

    pub fn remove(&mut self, key: u32) -> Option<Vec<u8>> {
        let node_ptr = self.map.remove(&key)?;
        self.detach(node_ptr);
        self.len -= 1;

        unsafe {
            let node = Box::from_raw(node_ptr);
            Some(node.value)
        }
    }

    pub fn len(&self) -> usize {
        self.len
    }

    pub fn is_empty(&self) -> bool {
        self.len == 0
    }

    pub fn capacity(&self) -> usize {
        self.capacity
    }

    pub fn contains(&self, key: u32) -> bool {
        self.map.contains_key(&key)
    }

    fn detach(&mut self, node: *mut Node) {
        unsafe {
            let prev = (*node).prev;
            let next = (*node).next;

            if !prev.is_null() {
                (*prev).next = next;
            } else {
                self.head = next;
            }

            if !next.is_null() {
                (*next).prev = prev;
            } else {
                self.tail = prev;
            }

            (*node).prev = ptr::null_mut();
            (*node).next = ptr::null_mut();
        }
    }

    fn attach_front(&mut self, node: *mut Node) {
        unsafe {
            (*node).next = self.head;
            (*node).prev = ptr::null_mut();

            if !self.head.is_null() {
                (*self.head).prev = node;
            }
            self.head = node;

            if self.tail.is_null() {
                self.tail = node;
            }
        }
    }

    fn evict(&mut self) {
        if self.tail.is_null() {
            return;
        }

        let tail = self.tail;
        self.detach(tail);

        unsafe {
            let node = Box::from_raw(tail);
            self.map.remove(&node.key);
            // BUG 1: no decrementa self.len al evictar
            // self.len -= 1;  ← FALTA esto
        }
    }
}

impl Drop for LruCache {
    fn drop(&mut self) {
        // BUG 2: si se llamó detach en un nodo pero no se liberó
        // (ej: get() detach + attach_front, pero si attach_front
        //  tiene un bug, el nodo puede quedar sin liberar)

        // Liberar recorriendo la lista
        let mut current = self.head;
        while !current.is_null() {
            unsafe {
                let next = (*current).next;
                let _ = Box::from_raw(current);  // Libera el nodo
                current = next;
            }
        }

        // BUG 3: los nodos en self.map que NO están en la lista
        // (si hay inconsistencia map↔lista) no se liberan → leak
        // Esto no debería pasar si el código es correcto, pero
        // si hay un bug en detach/attach, puede ocurrir
    }
}

// BUG 4: Send/Sync manuales (raw pointers)
// Si el cache se usa entre threads, hay data races potenciales
unsafe impl Send for LruCache {}
// unsafe impl Sync for LruCache {} ← no implementar Sync,
// pero enviar entre threads con Send + luego compartir → data race
```

### Bugs intencionales resumidos

```
Bug 1: evict() no decrementa self.len
       → self.len crece sin límite
       → len() retorna valor incorrecto
       → invariante len == map.len() se rompe

Bug 2: Drop itera la lista pero si hay inconsistencia
       lista↔map, algunos nodos se leakean

Bug 3: Si detach/attach corrompe la lista, Drop no
       libera todos los nodos → memory leak

Bug 4: Send sin Sync + compartir referencia entre
       threads → data race en los raw pointers
```

### Harness 1: fuzz_ops_asan.rs (ASan)

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use arbitrary::Arbitrary;

#[derive(Arbitrary, Debug)]
enum CacheOp {
    Put { key: u8, value_len: u8 },
    Get { key: u8 },
    Remove { key: u8 },
    Len,
    Contains { key: u8 },
}

#[derive(Arbitrary, Debug)]
struct FuzzInput {
    capacity: u8,
    ops: Vec<CacheOp>,
}

fuzz_target!(|input: FuzzInput| {
    let capacity = (input.capacity as usize % 10) + 1;  // 1-10
    if input.ops.len() > 500 { return; }

    let mut cache = unsafe_cache::LruCache::new(capacity);

    for op in &input.ops {
        match op {
            CacheOp::Put { key, value_len } => {
                let value = vec![*key; (*value_len as usize % 64) + 1];
                cache.put(*key as u32, value);
            }
            CacheOp::Get { key } => {
                let _ = cache.get(*key as u32);
            }
            CacheOp::Remove { key } => {
                let _ = cache.remove(*key as u32);
            }
            CacheOp::Len => {
                let _ = cache.len();
            }
            CacheOp::Contains { key } => {
                let _ = cache.contains(*key as u32);
            }
        }

        // Invariantes
        assert!(cache.len() <= cache.capacity(),
            "len ({}) > capacity ({})", cache.len(), cache.capacity());
    }
});
```

### Tareas

1. **Crea el proyecto** e implementa `src/lib.rs`

2. **Fuzzea con ASan** (default):
   ```bash
   cargo fuzz run fuzz_ops_asan -- -max_total_time=300
   ```
   - ¿ASan encuentra el Bug 1 (len incorrecto)?
   - ¿ASan encuentra memory leaks?

3. **Fuzzea con MSan**:
   ```bash
   cargo fuzz run fuzz_ops_asan --sanitizer=memory -- -max_total_time=300
   ```
   - ¿MSan encuentra algo que ASan no?

4. **Fuzzea sin sanitizer** (buscar panics):
   ```bash
   cargo fuzz run fuzz_ops_asan --sanitizer=none -- -max_total_time=300
   ```

5. **Implementa fuzz_oracle.rs**: comparación diferencial contra `std::collections::HashMap`

6. **Para cada crash**: minimizar, analizar, corregir, crear regression test

7. **Compara** los hallazgos de cada sanitizer:
   - ¿Qué bugs encontró ASan que los otros no?
   - ¿Qué bugs encontró el fuzzer sin sanitizer?
   - ¿Cuánto afectó cada sanitizer a exec/s?

---

## 37. Ejercicios

### Ejercicio 1: ASan con FFI

**Objetivo**: Detectar bugs de memoria en código C llamado desde Rust.

**Tareas**:

**a)** Crea un proyecto Rust que usa FFI para llamar a una función C:
   ```c
   // string_utils.c
   char *reverse_string(const char *input, size_t len) {
       char *result = malloc(len);  // BUG: no alloca +1 para '\0'
       for (size_t i = 0; i < len; i++) {
           result[i] = input[len - 1 - i];
       }
       result[len] = '\0';  // ← heap-buffer-overflow: escribe 1 byte más allá
       return result;
   }

   void free_string(char *s) {
       free(s);
   }
   ```

**b)** Escribe bindings seguros en Rust:
   ```rust
   pub fn reverse(input: &str) -> String {
       unsafe {
           let ptr = reverse_string(input.as_ptr(), input.len());
           let result = std::ffi::CStr::from_ptr(ptr).to_string_lossy().to_string();
           free_string(ptr);
           result
       }
   }
   ```

**c)** Escribe un harness y fuzzea con ASan. ¿Encuentra el heap-buffer-overflow?

**d)** Asegúrate de que `build.rs` pasa `-fsanitize=address` al código C.

**e)** Corrige el bug en el C y verifica con fuzzing.

---

### Ejercicio 2: MSan con MaybeUninit

**Objetivo**: Detectar uso de memoria no inicializada.

**Tareas**:

**a)** Implementa un buffer circular usando MaybeUninit:
   ```rust
   pub struct RingBuffer<T> {
       buf: Box<[MaybeUninit<T>]>,
       head: usize,
       tail: usize,
       len: usize,
       capacity: usize,
   }
   ```
   Con operaciones: `push`, `pop`, `peek`, `len`, `is_empty`, `is_full`.

**b)** Introduce un bug: `peek` lee del buffer sin verificar que la posición fue inicializada (si el buffer fue creado con capacidad 10, pero solo se hicieron 3 push, peek(5) lee MaybeUninit no inicializado).

**c)** Escribe un harness con secuencias de operaciones (Arbitrary).

**d)** Fuzzea con MSan:
   ```bash
   cargo fuzz run fuzz_ring --sanitizer=memory -- -max_total_time=300
   ```

**e)** ¿MSan detecta la lectura no inicializada? ¿Qué secuencia de operaciones la causa?

---

### Ejercicio 3: TSan con concurrencia unsafe

**Objetivo**: Detectar data races con ThreadSanitizer.

**Tareas**:

**a)** Implementa un "pool de objetos" que usa raw pointers compartidos:
   ```rust
   pub struct ObjectPool {
       objects: Vec<*mut Object>,
       // ...
   }
   unsafe impl Send for ObjectPool {}
   unsafe impl Sync for ObjectPool {}  // ← incorrecto: data race
   ```

**b)** Escribe un harness que cree threads que hagan operaciones concurrentes:
   - Thread 1: tomar un objeto del pool
   - Thread 2: devolver un objeto al pool
   - Thread 3: contar objetos disponibles

**c)** Fuzzea con TSan:
   ```bash
   cargo fuzz run fuzz_pool --sanitizer=thread -- -max_total_time=120
   ```

**d)** ¿TSan detecta el data race? ¿En qué función?

**e)** Corrige el pool usando `Mutex<Vec<Box<Object>>>` y verifica que TSan ya no reporta.

---

### Ejercicio 4: estrategia multi-sanitizer completa

**Objetivo**: Aplicar la estrategia completa de multi-sanitizer en un proyecto con unsafe y FFI.

**Tareas**:

**a)** Toma el LruCache del programa de práctica (sección 36) y:
   1. Fuzzea 10 minutos con ASan
   2. Fuzzea 10 minutos sin sanitizer
   3. Fuzzea 5 minutos con MSan (si aplica)
   4. Merge todos los corpus

**b)** Documenta para cada sesión:
   - exec/s promedio
   - Crashes encontrados (tipo y cantidad)
   - Cobertura final

**c)** Compara:
   - ¿Qué sanitizer encontró más bugs?
   - ¿Qué sanitizer encontró bugs que los otros no?
   - ¿Cuál fue el impacto de velocidad de cada sanitizer?

**d)** ¿El corpus generado por una sesión ayuda a las otras?
   (merge corpus de ASan → fuzzear sin sanitizer con ese corpus → ¿más cobertura?)

---

## Navegación

- **Anterior**: [T02 - Arbitrary trait](../T02_Arbitrary_trait/README.md)
- **Siguiente**: [T04 - Integrar fuzzing en workflow](../T04_Integrar_fuzzing_en_workflow/README.md)

---

> **Sección 2: Fuzzing en Rust** — Tópico 3 de 4 completado
