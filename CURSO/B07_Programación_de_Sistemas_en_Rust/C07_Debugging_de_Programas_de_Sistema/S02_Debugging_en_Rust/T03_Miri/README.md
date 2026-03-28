# Miri como herramienta de debugging

## Índice

1. [Miri en el flujo de debugging](#miri-en-el-flujo-de-debugging)
2. [Leer e interpretar errores de Miri](#leer-e-interpretar-errores-de-miri)
3. [Debugging interactivo con Miri](#debugging-interactivo-con-miri)
4. [Estrategias de aislamiento de bugs](#estrategias-de-aislamiento-de-bugs)
5. [Miri vs otras herramientas de debugging](#miri-vs-otras-herramientas-de-debugging)
6. [Patrones de debugging con Miri](#patrones-de-debugging-con-miri)
7. [Debugging de problemas de aliasing](#debugging-de-problemas-de-aliasing)
8. [Workflow completo: del crash al fix](#workflow-completo-del-crash-al-fix)
9. [Errores comunes](#errores-comunes)
10. [Cheatsheet](#cheatsheet)
11. [Ejercicios](#ejercicios)

---

## Miri en el flujo de debugging

> **Nota**: Los fundamentos de Miri (qué es, instalación, qué detecta, Stacked/Tree Borrows)
> se cubrieron en C06/S02/T03. Aquí nos centramos en **cómo usar Miri para diagnosticar
> y resolver bugs** en la práctica diaria.

Cuando un programa con `unsafe` se comporta de forma extraña — segfaults intermitentes,
datos corruptos, resultados que cambian entre compilaciones — el primer instinto es
alcanzar GDB o `println!`. Pero estos bugs a menudo son consecuencia de **Undefined
Behavior**, y las herramientas tradicionales no pueden detectar UB directamente.

```
┌─────────────────────────────────────────────────────────────┐
│        ¿Cuándo elegir Miri para debugging?                  │
├─────────────────────────────────────────────────────────────┤
│                                                             │
│  Síntoma                          → Herramienta             │
│  ─────────────────────────────    ─────────────────         │
│  Crash con backtrace claro        → RUST_BACKTRACE + GDB   │
│  Panic en código safe             → RUST_BACKTRACE          │
│  "Funciona en debug, falla en     → Miri (probable UB)     │
│   release"                                                  │
│  Datos corruptos sin crash        → Miri                    │
│  Segfault intermitente            → Miri + Valgrind         │
│  "Funciona en mi máquina"         → Miri (portabilidad UB) │
│  Test pasa/falla aleatoriamente   → Miri o ThreadSanitizer  │
│  Macro genera código incorrecto   → cargo-expand            │
│                                                             │
│  Regla: si hay unsafe cerca del bug → prueba Miri primero   │
└─────────────────────────────────────────────────────────────┘
```

### Posición de Miri en el flujo de debugging

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│  1. Reproducir│───▶│ 2. ¿Hay      │───▶│ 3. Aislar    │
│  el bug       │    │   unsafe?    │    │  caso mínimo │
└──────────────┘    └──────┬───────┘    └──────┬───────┘
                      Sí │    No               │
                         ▼     │               ▼
                   ┌──────────┐│    ┌────────────────────┐
                   │cargo miri││    │ 4. Interpretar     │
                   │  test    ││    │    output de Miri   │
                   └──────────┘│    └────────┬───────────┘
                               │             │
                               ▼             ▼
                        Herramientas    ┌──────────────┐
                        tradicionales   │ 5. Corregir  │
                        (GDB, log...)   │    y verificar│
                                        └──────────────┘
```

---

## Leer e interpretar errores de Miri

La habilidad más importante al debuggear con Miri es **leer sus mensajes de error**.
Miri no dice simplemente "error"; te dice exactamente **qué regla se violó**, **dónde**,
y a menudo **por qué**.

### Anatomía de un error de Miri

```rust
// src/lib.rs
fn bad_alias() {
    let mut x: i32 = 42;
    let r = &x;
    let p = &mut x;  // Alias mutable mientras existe referencia inmutable
    *p = 99;
    println!("{}", r);
}

#[test]
fn test_bad_alias() {
    bad_alias();
}
```

```
$ cargo +nightly miri test test_bad_alias

error: Undefined Behavior: not granting access to tag <2158>   ← ①
         because that would remove [SharedReadOnly for <2156>]  ← ②
         which is strongly protected because it is an argument
         of call 768                                            ← ③
  --> src/lib.rs:4:13
   |
4  |     let p = &mut x;
   |             ^^^^^^ not granting access to tag <2158>
   |
   = help: this indicates a potential bug in the program:       ← ④
           it performed an invalid operation, but the
           Stacked Borrows rules it violated are still
           experimental
   = help: see https://github.com/rust-lang/miri/...
           for further information
   = note: BACKTRACE:                                           ← ⑤
   = note: inside `bad_alias` at src/lib.rs:4:13
   = note: inside `test_bad_alias` at src/lib.rs:10:5
```

Desglose línea por línea:

```
┌────────────────────────────────────────────────────────────┐
│  Elemento          │ Significado                           │
├────────────────────┼───────────────────────────────────────┤
│ ① Tag <2158>       │ Identificador interno del borrow      │
│                    │ que Miri asigna a cada referencia      │
│ ② SharedReadOnly   │ El tipo de permiso que se violaría:   │
│    for <2156>      │ una lectura compartida existente       │
│ ③ "strongly        │ La referencia está protegida por ser  │
│    protected"      │ argumento activo de una función       │
│ ④ "experimental"   │ Stacked Borrows es el modelo; no es   │
│                    │ 100% definitivo pero detecta bugs     │
│                    │ reales                                 │
│ ⑤ BACKTRACE        │ La pila de llamadas hasta el punto    │
│                    │ exacto de la violación                │
└────────────────────────────────────────────────────────────┘
```

### Tipos de error y qué buscar

**1. Out-of-bounds access**

```
error: Undefined Behavior: pointer to alloc1234 was dereferenced
       after this allocation got freed
```

Diagnóstico: use-after-free. Busca dónde se liberó la memoria y por qué el puntero
sigue vivo.

**2. Invalid value constructed**

```
error: Undefined Behavior: type validation failed at .value:
       encountered 0x02, but expected a boolean
```

Diagnóstico: se escribió un valor inválido para el tipo. Común en transmutes incorrectos
o lecturas de unions sin el variante correcto.

**3. Data race detected**

```
error: Undefined Behavior: Data race detected between (1) Write
       on thread `<unnamed>` and (2) Read on thread `main`
```

Diagnóstico: acceso concurrente sin sincronización. Miri identifica ambos threads
y las operaciones conflictivas.

**4. Stacked Borrows violation**

```
error: Undefined Behavior: attempting a write access using <1234>
       at alloc5678[0x0], but that tag does not exist in the
       borrow stack for this location
```

Diagnóstico: un puntero fue invalidado por una operación intermedia. Miri te dice
qué tag (referencia) intentó acceder y que ya no existe en la pila de borrows.

---

## Debugging interactivo con Miri

### Variables de entorno para diagnóstico

Miri ofrece variables de entorno que controlan cuánta información reporta:

```bash
# Ver TODAS las operaciones de la pila de borrows (muy verboso)
MIRIFLAGS="-Zmiri-tag-gc=0" cargo +nightly miri test test_name

# Desactivar garbage collection de tags para rastrear todo el historial
# Útil cuando el error dice "tag does not exist" y necesitas
# saber cuándo se eliminó
MIRIFLAGS="-Zmiri-tag-gc=0" cargo +nightly miri test

# Backtrace completo en errores
MIRIFLAGS="-Zmiri-backtrace=full" cargo +nightly miri test

# Combinar flags
MIRIFLAGS="-Zmiri-backtrace=full -Zmiri-tag-gc=0" \
    cargo +nightly miri test
```

### Usar `-Zmiri-track-pointer-tag` para rastrear un tag

Cuando Miri reporta un error con un tag específico (e.g., `<2158>`), puedes
rastrear toda la vida de ese tag:

```bash
MIRIFLAGS="-Zmiri-track-pointer-tag=2158" cargo +nightly miri test
```

Esto produce output adicional cada vez que el tag 2158 se crea, se usa, o se
invalida:

```
note: tracking was triggered
  --> src/lib.rs:3:13
   |
3  |     let r = &x;
   |             ^^ created tag <2158> for SharedReadOnly

note: tracking was triggered
  --> src/lib.rs:4:13
   |
4  |     let p = &mut x;
   |             ^^^^^^ popped tracked tag for item [SharedReadOnly
   |                    for <2158>]
```

Ahora ves exactamente:
- **Línea 3**: se creó el tag `<2158>` como `SharedReadOnly`
- **Línea 4**: el `&mut x` eliminó (`popped`) ese tag de la pila

### Usar `-Zmiri-track-alloc-id` para rastrear una alocación

Para errores de use-after-free o out-of-bounds:

```bash
# Si el error menciona "alloc1234"
MIRIFLAGS="-Zmiri-track-alloc-id=1234" cargo +nightly miri test
```

Muestra cuándo se creó la alocación, cuándo se liberó, y cualquier acceso posterior.

### Seed para reproducibilidad

Miri puede detectar problemas de concurrencia ejecutando threads en órdenes
diferentes. Usa `-Zmiri-seed` para reproducir un fallo:

```bash
# El error ocurrió con seed 42? Reproducirlo:
MIRIFLAGS="-Zmiri-seed=42" cargo +nightly miri test

# Buscar bugs de concurrencia probando múltiples seeds:
for seed in $(seq 0 99); do
    echo "=== Seed $seed ==="
    MIRIFLAGS="-Zmiri-seed=$seed" cargo +nightly miri test 2>&1 \
        | grep -q "Undefined Behavior" && echo "FOUND BUG with seed $seed"
done
```

---

## Estrategias de aislamiento de bugs

### Reducir el caso de prueba

Miri es lento (10-100x más que ejecución normal). Un test grande puede tardar
minutos. La estrategia es **aislar el bug en el test más pequeño posible**:

```rust
// ❌ Test grande: tarda 30 segundos en Miri
#[test]
fn test_full_pipeline() {
    let data = load_test_data();          // 10,000 registros
    let result = process_pipeline(data);   // Muchas operaciones
    assert_eq!(result.len(), 9500);
}

// ✅ Test mínimo: tarda 0.1 segundos en Miri
#[test]
fn test_pipeline_single_record() {
    let data = vec![TestRecord::new("minimal")];
    let result = process_pipeline(data);
    assert_eq!(result.len(), 1);
}
```

### Estrategia de bisección con Miri

Cuando un test grande falla en Miri, bisecciona manualmente:

```rust
// Paso 1: ¿El bug está en la primera o segunda mitad?
#[test]
fn test_isolate_phase() {
    let mut buf = UnsafeBuffer::new(64);

    // Fase 1: escritura
    for i in 0..32 {
        buf.write(i, i as u8);
    }
    // ¿Falla aquí? → El bug está en write()

    // Fase 2: lectura
    for i in 0..32 {
        assert_eq!(buf.read(i), i as u8);
    }
    // ¿Falla aquí? → El bug está en read() o en la interacción
}

// Paso 2: reducir al caso mínimo
#[test]
fn test_minimal_reproduction() {
    let mut buf = UnsafeBuffer::new(4);
    buf.write(0, 42);
    buf.write(1, 43);  // ← Miri señala el error aquí
    let val = buf.read(0);
    assert_eq!(val, 42);
}
```

### Aislar unsafe de la lógica de negocio

```rust
// Separar la operación unsafe para testearla independientemente
mod raw_ops {
    /// # Safety
    /// `ptr` must be valid for writes of `len` bytes
    pub unsafe fn copy_bytes(src: *const u8, dst: *mut u8, len: usize) {
        unsafe { std::ptr::copy_nonoverlapping(src, dst, len) }
    }
}

// Test dirigido solo a la operación unsafe
#[test]
fn test_copy_bytes_non_overlapping() {
    let src = [1u8, 2, 3, 4];
    let mut dst = [0u8; 4];
    unsafe {
        raw_ops::copy_bytes(src.as_ptr(), dst.as_mut_ptr(), 4);
    }
    assert_eq!(dst, [1, 2, 3, 4]);
}

// Test que verifica el caso de overlap (debería fallar en Miri)
#[test]
fn test_copy_bytes_overlap_detection() {
    let mut data = [1u8, 2, 3, 4];
    let ptr = data.as_mut_ptr();
    unsafe {
        // copy_nonoverlapping con overlap → UB, Miri lo detecta
        raw_ops::copy_bytes(ptr, ptr.add(1), 3);
    }
}
```

---

## Miri vs otras herramientas de debugging

```
┌──────────────┬─────────────┬────────────┬───────────┬──────────┐
│ Herramienta  │ Detecta UB  │ Data races │ Leaks     │ Velocidad│
├──────────────┼─────────────┼────────────┼───────────┼──────────┤
│ Miri         │ ✅ Sí       │ ✅ Sí      │ ✅ Sí     │ 10-100x  │
│              │ (exhaustivo)│            │           │ más lento│
├──────────────┼─────────────┼────────────┼───────────┼──────────┤
│ Valgrind     │ Parcial     │ ❌ No*     │ ✅ Sí     │ 5-20x    │
│              │ (memoria)   │            │           │ más lento│
├──────────────┼─────────────┼────────────┼───────────┼──────────┤
│ ASan         │ Parcial     │ ❌ No      │ ✅ LeakSan│ 2x       │
│              │ (memoria)   │            │           │ más lento│
├──────────────┼─────────────┼────────────┼───────────┼──────────┤
│ TSan         │ ❌ No       │ ✅ Sí      │ ❌ No     │ 5-15x    │
│              │             │            │           │ más lento│
├──────────────┼─────────────┼────────────┼───────────┼──────────┤
│ GDB          │ ❌ No       │ ❌ No      │ ❌ No     │ Normal   │
│              │ (post-mortem│            │           │          │
│              │  solo)      │            │           │          │
└──────────────┴─────────────┴────────────┴───────────┴──────────┘

* Valgrind con Helgrind/DRD detecta races, pero no en código Rust típico
```

### Cuándo Miri NO es suficiente

```
┌─────────────────────────────────────────────────────────────┐
│  Limitación de Miri          │ Alternativa                  │
├──────────────────────────────┼──────────────────────────────┤
│ No ejecuta FFI (C/C++)      │ Valgrind, ASan               │
│ No hace syscalls reales     │ strace + ejecución real      │
│ No detecta bugs lógicos     │ Tests + fuzzing              │
│ Muy lento para tests largos │ ASan para tests de           │
│                              │ integración                  │
│ No prueba hardware real     │ Tests en CI con target real   │
│ No ejecuta inline asm       │ Tests de integración         │
└──────────────────────────────┴──────────────────────────────┘
```

### Combinar herramientas: estrategia en capas

```
┌─────────────────────────────────────────────────────┐
│              Pirámide de debugging                   │
│                                                      │
│                    ┌───────┐                          │
│                    │ Miri  │  ← Tests unitarios      │
│                    │       │    con unsafe            │
│                 ┌──┴───────┴──┐                      │
│                 │   ASan/TSan  │  ← Tests de         │
│                 │              │    integración       │
│              ┌──┴──────────────┴──┐                  │
│              │   Valgrind/strace   │  ← Con FFI y    │
│              │                     │    syscalls      │
│           ┌──┴─────────────────────┴──┐              │
│           │   Fuzzing (cargo-fuzz)     │  ← Inputs   │
│           │                            │    inesperados│
│        ┌──┴────────────────────────────┴──┐          │
│        │    GDB / RUST_BACKTRACE           │  ← Post │
│        │                                    │  mortem │
│        └────────────────────────────────────┘        │
└─────────────────────────────────────────────────────┘
```

---

## Patrones de debugging con Miri

### Patrón 1: Reproducir un segfault

```rust
// El programa produce segfault intermitente en release
use std::alloc::{alloc, dealloc, Layout};

struct Pool {
    blocks: Vec<*mut u8>,
    layout: Layout,
}

impl Pool {
    fn new(block_size: usize) -> Self {
        Pool {
            blocks: Vec::new(),
            layout: Layout::from_size_align(block_size, 8).unwrap(),
        }
    }

    fn allocate(&mut self) -> *mut u8 {
        let ptr = unsafe { alloc(self.layout) };
        self.blocks.push(ptr);
        ptr
    }

    fn deallocate_all(&mut self) {
        for &ptr in &self.blocks {
            unsafe { dealloc(ptr, self.layout) };
        }
        // BUG: no limpiamos el vector → los punteros siguen ahí
        // self.blocks.clear();  ← falta esta línea
    }
}

impl Drop for Pool {
    fn drop(&mut self) {
        self.deallocate_all();
        // Si deallocate_all() ya se llamó manualmente,
        // esto hace double-free
    }
}

#[test]
fn test_pool_lifecycle() {
    let mut pool = Pool::new(1024);
    let _p1 = pool.allocate();
    let _p2 = pool.allocate();
    pool.deallocate_all();  // Primera liberación
    // Drop hace segunda liberación → double free → Miri lo detecta
}
```

```
$ cargo +nightly miri test test_pool_lifecycle

error: Undefined Behavior: memory access to alloc1478,
       which has been freed
```

**Proceso de fix:**

```rust
fn deallocate_all(&mut self) {
    for &ptr in &self.blocks {
        unsafe { dealloc(ptr, self.layout) };
    }
    self.blocks.clear();  // ← Fix: limpiar después de liberar
}
```

### Patrón 2: Encontrar la causa de datos corruptos

```rust
struct RingBuffer {
    data: *mut u8,
    capacity: usize,
    head: usize,
    len: usize,
}

impl RingBuffer {
    fn new(capacity: usize) -> Self {
        let layout = Layout::from_size_align(capacity, 1).unwrap();
        let data = unsafe { alloc(layout) };
        RingBuffer { data, capacity, head: 0, len: 0 }
    }

    fn push(&mut self, byte: u8) {
        if self.len >= self.capacity {
            return; // Buffer lleno, ignorar
        }
        let write_pos = (self.head + self.len) % self.capacity;
        unsafe {
            // BUG: write_pos puede apuntar fuera si capacity = 0
            self.data.add(write_pos).write(byte);
        }
        self.len += 1;
    }

    fn pop(&mut self) -> Option<u8> {
        if self.len == 0 {
            return None;
        }
        let val = unsafe { self.data.add(self.head).read() };
        self.head = (self.head + 1) % self.capacity;
        self.len -= 1;
        Some(val)
    }
}

impl Drop for RingBuffer {
    fn drop(&mut self) {
        if self.capacity > 0 {
            let layout = Layout::from_size_align(self.capacity, 1).unwrap();
            unsafe { dealloc(self.data, layout) };
        }
    }
}

// Test que Miri puede verificar
#[test]
fn test_ring_buffer_wrap_around() {
    let mut rb = RingBuffer::new(4);
    rb.push(1);
    rb.push(2);
    rb.push(3);
    rb.push(4);
    assert_eq!(rb.pop(), Some(1));
    rb.push(5);  // Debe escribir en posición 0 (wrap around)
    assert_eq!(rb.pop(), Some(2));
    assert_eq!(rb.pop(), Some(3));
    assert_eq!(rb.pop(), Some(4));
    assert_eq!(rb.pop(), Some(5));
}
```

Si Miri pasa sin errores, sabes que tu aritmética de punteros es correcta para
este caso. Si reporta un error, te señala la línea exacta.

### Patrón 3: Validar transmutes

```rust
#[repr(u8)]
enum Color {
    Red = 0,
    Green = 1,
    Blue = 2,
}

fn color_from_byte(b: u8) -> Color {
    // ❌ UB si b > 2
    unsafe { std::mem::transmute(b) }
}

#[test]
fn test_color_valid() {
    let c = color_from_byte(1);
    assert!(matches!(c, Color::Green));  // OK
}

#[test]
fn test_color_invalid() {
    let c = color_from_byte(5);  // Miri: Undefined Behavior!
    // "encountered 0x05, but expected a valid enum tag"
}
```

**Fix:**

```rust
fn color_from_byte(b: u8) -> Option<Color> {
    match b {
        0 => Some(Color::Red),
        1 => Some(Color::Green),
        2 => Some(Color::Blue),
        _ => None,
    }
}
```

---

## Debugging de problemas de aliasing

Los errores de aliasing (Stacked Borrows) son los más difíciles de entender.
Aquí hay un proceso sistemático.

### Paso 1: Ejecutar con Tree Borrows para comparar

```bash
# Primero con Stacked Borrows (default)
cargo +nightly miri test

# Luego con Tree Borrows (más permisivo)
MIRIFLAGS="-Zmiri-tree-borrows" cargo +nightly miri test
```

Si el error **desaparece** con Tree Borrows, probablemente tu código es correcto
pero usa un patrón que Stacked Borrows rechaza. Esto es útil para priorizar:
Tree Borrows es el modelo más nuevo y probable futuro estándar.

Si el error **persiste** en ambos modelos, definitivamente tienes un bug real.

### Paso 2: Rastrear el tag problemático

```bash
# El error dice "tag <1234> does not exist in the borrow stack"
MIRIFLAGS="-Zmiri-track-pointer-tag=1234 -Zmiri-tag-gc=0" \
    cargo +nightly miri test
```

Esto imprime tres eventos clave:
1. **Creación**: dónde se creó el tag (qué referencia)
2. **Invalidación**: qué operación eliminó el tag de la pila
3. **Uso inválido**: dónde se intentó usar el tag ya eliminado

### Paso 3: Entender el patrón de invalidación

```rust
// Patrón común: crear un puntero raw, luego usar la referencia original
fn problematic() {
    let mut x = 42;
    let raw = &mut x as *mut i32;  // raw tiene tag <A>

    // Esta línea crea un nuevo borrow que invalida <A>
    let r = &x;                     // r tiene tag <B>, invalida <A>
    println!("{}", r);

    unsafe { *raw = 99; }          // Usar <A> después de invalidación → UB
}

// Fix: reordenar para que raw se use antes de crear r
fn fixed() {
    let mut x = 42;
    let raw = &mut x as *mut i32;
    unsafe { *raw = 99; }          // Usar raw primero
    let r = &x;                     // Ahora r es válido
    println!("{}", r);
}
```

### Paso 4: Patrón de self-referential structs

```rust
// Estructura que se auto-referencia: un caso clásico de aliasing
struct SelfRef {
    data: String,
    ptr: *const String,  // Apunta a self.data
}

impl SelfRef {
    fn new(s: String) -> Self {
        let mut sr = SelfRef {
            data: s,
            ptr: std::ptr::null(),
        };
        sr.ptr = &sr.data;  // ptr apunta a data dentro de sr
        sr
        // ⚠️ BUG: al retornar, sr se mueve → ptr queda dangling
    }
}

#[test]
fn test_self_ref() {
    let sr = SelfRef::new("hello".to_string());
    // Miri detecta que ptr ya no apunta a una ubicación válida
    unsafe {
        assert_eq!(&*sr.ptr, &sr.data);  // UB: dangling pointer
    }
}
```

Miri detecta esto inmediatamente. La solución es usar `Pin` o crates como
`ouroboros` para estructuras auto-referenciales.

---

## Workflow completo: del crash al fix

Ejemplo práctico: un allocator custom que produce corrupción de datos.

### Paso 1: Reproducir

```rust
use std::alloc::{alloc, dealloc, Layout};

struct BumpAllocator {
    region: *mut u8,
    layout: Layout,
    offset: usize,
}

impl BumpAllocator {
    fn new(size: usize) -> Self {
        let layout = Layout::from_size_align(size, 8).unwrap();
        let region = unsafe { alloc(layout) };
        BumpAllocator { region, layout, offset: 0 }
    }

    fn alloc<T>(&mut self) -> Option<*mut T> {
        let align = std::mem::align_of::<T>();
        let size = std::mem::size_of::<T>();

        // Alinear offset
        let aligned = (self.offset + align - 1) & !(align - 1);

        if aligned + size > self.layout.size() {
            return None;
        }

        let ptr = unsafe { self.region.add(aligned) as *mut T };
        self.offset = aligned + size;
        Some(ptr)
    }
}

impl Drop for BumpAllocator {
    fn drop(&mut self) {
        unsafe { dealloc(self.region, self.layout) };
    }
}
```

### Paso 2: Escribir test para Miri

```rust
#[test]
fn test_bump_allocator_types() {
    let mut bump = BumpAllocator::new(256);

    // Alocar diferentes tipos
    let p_i32 = bump.alloc::<i32>().unwrap();
    let p_f64 = bump.alloc::<f64>().unwrap();
    let p_u8  = bump.alloc::<u8>().unwrap();

    unsafe {
        p_i32.write(42);
        p_f64.write(3.14);
        p_u8.write(255);

        assert_eq!(p_i32.read(), 42);
        assert_eq!(p_f64.read(), 3.14);
        assert_eq!(p_u8.read(), 255);
    }
}
```

### Paso 3: Ejecutar en Miri

```bash
cargo +nightly miri test test_bump_allocator_types
```

### Paso 4: Interpretar y corregir

Si Miri reporta un error de alineación:
```
error: Undefined Behavior: accessing memory with alignment 4,
       but alignment 8 is required
```

Esto indica que la lógica de alineación tiene un bug. Verificar la aritmética
de alineamiento y corregir.

### Paso 5: Verificar el fix

```bash
# Re-ejecutar el test original
cargo +nightly miri test test_bump_allocator_types

# Ejecutar TODOS los tests con Miri para asegurar que no se rompió nada
cargo +nightly miri test
```

---

## Errores comunes

### 1. Ignorar errores de Stacked Borrows porque "el código funciona"

```bash
# ❌ "Funciona en cargo test, así que Miri está equivocado"
cargo test          # Pasa
cargo miri test     # Falla con Stacked Borrows violation
# El código tiene UB real — que funcione hoy es coincidencia

# ✅ Corregir el código o verificar con Tree Borrows
MIRIFLAGS="-Zmiri-tree-borrows" cargo +nightly miri test
# Si también falla → bug definitivo. Si pasa → evaluar caso por caso
```

### 2. No reducir el caso de prueba antes de investigar

```bash
# ❌ Ejecutar toda la suite con Miri (puede tardar horas)
cargo +nightly miri test

# ✅ Ejecutar solo el test relevante
cargo +nightly miri test test_specific_function

# ✅ O filtrar por módulo
cargo +nightly miri test --lib specific_module::
```

### 3. No usar tracking flags para errores crípticos

```bash
# ❌ Mirar el error e intentar adivinar qué tag es qué
# "tag <2158> does not exist in borrow stack"... ¿cuál es <2158>?

# ✅ Rastrear el tag específico
MIRIFLAGS="-Zmiri-track-pointer-tag=2158 -Zmiri-tag-gc=0" \
    cargo +nightly miri test
```

### 4. Olvidar que Miri no cubre FFI

```rust
// ❌ Pensar que si Miri pasa, todo el programa es seguro
extern "C" {
    fn external_function(ptr: *mut u8);
}

#[test]
fn test_ffi_call() {
    let mut buf = vec![0u8; 64];
    unsafe { external_function(buf.as_mut_ptr()); }
    // Miri: "unsupported operation: cannot call foreign function"
    // ← No se ejecuta, no se verifica
}

// ✅ Separar tests: Miri para lógica Rust, Valgrind para FFI
```

### 5. No limpiar el caché de Miri cuando cambia el toolchain

```bash
# ❌ Errores extraños después de actualizar nightly
cargo +nightly miri test
# → errores de compilación incomprensibles

# ✅ Limpiar y reinstalar
cargo +nightly miri setup
cargo clean
cargo +nightly miri test
```

---

## Cheatsheet

```
┌──────────────────────────────────────────────────────────────┐
│                 MIRI DEBUGGING CHEATSHEET                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  EJECUTAR                                                    │
│  cargo +nightly miri test              Todo                  │
│  cargo +nightly miri test NOMBRE       Un test específico    │
│  cargo +nightly miri run               Ejecutar el programa  │
│                                                              │
│  FLAGS DE DEBUGGING (vía MIRIFLAGS)                          │
│  -Zmiri-backtrace=full                 Backtrace completo    │
│  -Zmiri-track-pointer-tag=N           Rastrear un tag        │
│  -Zmiri-track-alloc-id=N             Rastrear una alocación │
│  -Zmiri-tag-gc=0                      No limpiar tags viejos│
│  -Zmiri-tree-borrows                  Usar Tree Borrows     │
│  -Zmiri-seed=N                        Seed para concurrencia│
│                                                              │
│  FLAGS DE DETECCIÓN                                          │
│  -Zmiri-strict-provenance             Provenance estricta    │
│  -Zmiri-leak-check                    Detectar memory leaks │
│  -Zmiri-disable-stacked-borrows       Desactivar aliasing   │
│                                                              │
│  WORKFLOW DE DIAGNÓSTICO                                     │
│  1. cargo miri test NOMBRE            Reproducir             │
│  2. Leer tag del error                Identificar borrow     │
│  3. -Zmiri-track-pointer-tag=TAG      Rastrear la vida del  │
│                                        borrow                │
│  4. Comparar Stacked vs Tree Borrows  Evaluar severidad     │
│  5. Corregir y re-ejecutar            Verificar fix          │
│                                                              │
│  MANTENIMIENTO                                               │
│  cargo +nightly miri setup            Instalar/actualizar    │
│  cargo clean                          Limpiar caché          │
│  rustup update nightly                Actualizar toolchain   │
│                                                              │
│  COMBINACIONES COMUNES                                       │
│  MIRIFLAGS="-Zmiri-backtrace=full \                          │
│    -Zmiri-track-pointer-tag=N \                              │
│    -Zmiri-tag-gc=0"                                          │
│                                                              │
│  MIRIFLAGS="-Zmiri-tree-borrows \                            │
│    -Zmiri-strict-provenance \                                │
│    -Zmiri-leak-check"                                        │
└──────────────────────────────────────────────────────────────┘
```

---

## Ejercicios

### Ejercicio 1: Diagnosticar un double-free

Dado este código, ejecútalo con Miri e identifica el bug:

```rust
use std::alloc::{alloc, dealloc, Layout};

struct TwoBuffers {
    a: *mut u8,
    b: *mut u8,
    layout: Layout,
}

impl TwoBuffers {
    fn new(size: usize) -> Self {
        let layout = Layout::from_size_align(size, 1).unwrap();
        let a = unsafe { alloc(layout) };
        let b = unsafe { alloc(layout) };
        TwoBuffers { a, b, layout }
    }

    fn swap_buffers(&mut self) {
        std::mem::swap(&mut self.a, &mut self.b);
    }

    fn free_a(&mut self) {
        unsafe { dealloc(self.a, self.layout) };
        self.a = std::ptr::null_mut();
    }
}

impl Drop for TwoBuffers {
    fn drop(&mut self) {
        unsafe {
            dealloc(self.a, self.layout);
            dealloc(self.b, self.layout);
        }
    }
}

#[test]
fn test_two_buffers() {
    let mut tb = TwoBuffers::new(64);
    tb.swap_buffers();
    tb.free_a();
    // ¿Qué pasa en el Drop?
}
```

**Tareas:**
1. Predice qué reportará Miri
2. Ejecuta `cargo +nightly miri test test_two_buffers`
3. Identifica las dos causas del error (hint: `null_mut` y `swap`)
4. Corrige el `Drop` para que maneje punteros nulos

**Pregunta de reflexión:** ¿Por qué este bug no causa crash en todas las ejecuciones
sin Miri, pero Miri siempre lo detecta?

---

### Ejercicio 2: Rastrear un tag de Stacked Borrows

Dado este código que Miri rechaza:

```rust
fn aliased_mutation() -> i32 {
    let mut val = 10;
    let ptr = &mut val as *mut i32;
    let reference = &val;

    unsafe { *ptr = 20; }

    *reference  // Leer a través de referencia inmutable
}

#[test]
fn test_aliased() {
    assert_eq!(aliased_mutation(), 20);
}
```

**Tareas:**
1. Ejecuta el test con Miri y anota el número de tag del error
2. Usa `-Zmiri-track-pointer-tag=TAG` para ver la vida completa del tag
3. Identifica en qué línea se invalida el tag y por qué operación
4. Reescribe la función para que sea correcta (mismo resultado, sin UB)

**Pregunta de reflexión:** El test pasa con `cargo test` normal y produce el resultado
esperado (20). ¿Por qué Miri lo marca como UB si el resultado es "correcto"?

---

### Ejercicio 3: Workflow completo de debugging

Tienes una estructura `StackVec` que usa un array en el stack como almacenamiento:

```rust
struct StackVec<T, const N: usize> {
    data: [std::mem::MaybeUninit<T>; N],
    len: usize,
}

impl<T, const N: usize> StackVec<T, N> {
    fn new() -> Self {
        StackVec {
            data: unsafe { std::mem::MaybeUninit::uninit().assume_init() },
            len: 0,
        }
    }

    fn push(&mut self, val: T) -> Result<(), T> {
        if self.len >= N {
            return Err(val);
        }
        self.data[self.len] = std::mem::MaybeUninit::new(val);
        self.len += 1;
        Ok(())
    }

    fn pop(&mut self) -> Option<T> {
        if self.len == 0 {
            return None;
        }
        self.len -= 1;
        Some(unsafe { self.data[self.len].assume_init_read() })
    }

    fn as_slice(&self) -> &[T] {
        unsafe {
            std::slice::from_raw_parts(
                self.data.as_ptr() as *const T,
                self.len,
            )
        }
    }
}

impl<T, const N: usize> Drop for StackVec<T, N> {
    fn drop(&mut self) {
        for i in 0..self.len {
            unsafe { self.data[i].assume_init_drop(); }
        }
    }
}
```

**Tareas:**
1. Escribe 3 tests para cubrir: push/pop básico, overflow, y uso con `String`
2. Ejecuta con `cargo +nightly miri test`
3. Hay un bug sutil en `new()` — ¿Miri lo detecta con tipos `Copy`? ¿Y con `String`?
4. Corrige `new()` usando el método correcto de `MaybeUninit`
5. Verifica que todos los tests pasan en Miri después del fix

**Pregunta de reflexión:** ¿Por qué `MaybeUninit::uninit().assume_init()` para crear
un array de `MaybeUninit<T>` es UB en teoría pero `[MaybeUninit::uninit(); N]` no lo es?
¿Qué diferencia semántica hay entre ambas expresiones?