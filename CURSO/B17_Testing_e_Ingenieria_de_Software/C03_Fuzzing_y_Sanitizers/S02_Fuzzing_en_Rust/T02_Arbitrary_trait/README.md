# T02 - Arbitrary trait: derivar Arbitrary para structs custom, generar inputs estructurados

## Índice

1. [El problema de los bytes crudos](#1-el-problema-de-los-bytes-crudos)
2. [Qué es el trait Arbitrary](#2-qué-es-el-trait-arbitrary)
3. [Arquitectura: de bytes a tipos](#3-arquitectura-de-bytes-a-tipos)
4. [El crate arbitrary](#4-el-crate-arbitrary)
5. [Unstructured: la fuente de bytes](#5-unstructured-la-fuente-de-bytes)
6. [Tipos que ya implementan Arbitrary](#6-tipos-que-ya-implementan-arbitrary)
7. [derive(Arbitrary): generación automática](#7-derivearbitrary-generación-automática)
8. [Cómo funciona derive internamente](#8-cómo-funciona-derive-internamente)
9. [Usar Arbitrary en fuzz_target!](#9-usar-arbitrary-en-fuzz_target)
10. [Structs simples con derive](#10-structs-simples-con-derive)
11. [Enums con derive](#11-enums-con-derive)
12. [Structs con campos complejos](#12-structs-con-campos-complejos)
13. [Implementación manual de Arbitrary](#13-implementación-manual-de-arbitrary)
14. [Cuándo implementar manualmente vs derivar](#14-cuándo-implementar-manualmente-vs-derivar)
15. [Restricciones de dominio: valores válidos](#15-restricciones-de-dominio-valores-válidos)
16. [Rangos numéricos con int_in_range](#16-rangos-numéricos-con-int_in_range)
17. [Strings con restricciones](#17-strings-con-restricciones)
18. [Colecciones con tamaño acotado](#18-colecciones-con-tamaño-acotado)
19. [Tipos recursivos y profundidad](#19-tipos-recursivos-y-profundidad)
20. [Combinando derive con campos manuales](#20-combinando-derive-con-campos-manuales)
21. [Arbitrary para secuencias de operaciones](#21-arbitrary-para-secuencias-de-operaciones)
22. [Patrón command: fuzzing stateful con Arbitrary](#22-patrón-command-fuzzing-stateful-con-arbitrary)
23. [size_hint: optimizar la generación](#23-size_hint-optimizar-la-generación)
24. [Shrinking: reducir inputs que causan crashes](#24-shrinking-reducir-inputs-que-causan-crashes)
25. [Arbitrary vs proptest::Arbitrary](#25-arbitrary-vs-proptestarbitrary)
26. [Arbitrary con cargo fuzz fmt](#26-arbitrary-con-cargo-fuzz-fmt)
27. [Debugging de inputs Arbitrary](#27-debugging-de-inputs-arbitrary)
28. [Patrones avanzados](#28-patrones-avanzados)
29. [Anti-patrones con Arbitrary](#29-anti-patrones-con-arbitrary)
30. [Comparación C vs Rust vs Go](#30-comparación-c-vs-rust-vs-go)
31. [Errores comunes](#31-errores-comunes)
32. [Programa de práctica: sistema de tickets](#32-programa-de-práctica-sistema-de-tickets)
33. [Ejercicios](#33-ejercicios)

---

## 1. El problema de los bytes crudos

En T01 vimos que `fuzz_target!` recibe bytes crudos:

```rust
fuzz_target!(|data: &[u8]| {
    // data = bytes aleatorios del fuzzer
    // Tú debes convertirlos en algo útil
});
```

Esto funciona bien para parsers que aceptan `&[u8]` directamente. Pero, ¿qué pasa cuando tu función espera un tipo estructurado?

```rust
// Tu API:
pub struct Config {
    pub name: String,
    pub port: u16,
    pub max_connections: u32,
    pub use_tls: bool,
    pub timeout_ms: Option<u64>,
}

pub fn apply_config(config: &Config) -> Result<(), ConfigError> { ... }
```

### Enfoque ingenuo: parsear bytes a mano

```rust
fuzz_target!(|data: &[u8]| {
    if data.len() < 4 + 2 + 4 + 1 {
        return;
    }

    let name_len = data[0] as usize;
    if data.len() < 1 + name_len + 2 + 4 + 1 {
        return;
    }

    let name = match std::str::from_utf8(&data[1..1+name_len]) {
        Ok(s) => s.to_string(),
        Err(_) => return,
    };

    let offset = 1 + name_len;
    let port = u16::from_le_bytes([data[offset], data[offset+1]]);
    let max_connections = u32::from_le_bytes([
        data[offset+2], data[offset+3], data[offset+4], data[offset+5]
    ]);
    let use_tls = data[offset+6] != 0;
    // ... y así sucesivamente para cada campo

    let config = Config {
        name,
        port,
        max_connections,
        use_tls,
        timeout_ms: None, // ¿cómo representar Option?
    };

    let _ = apply_config(&config);
});
```

### Problemas de este enfoque

```
┌────────────────────────────────────────────────────────────────┐
│          Problemas del parsing manual de bytes                  │
├────────────────────────────────────────────────────────────────┤
│                                                                │
│  1. TEDIOSO: 20+ líneas de boilerplate por cada struct         │
│     Cada campo nuevo = más código de parsing manual            │
│                                                                │
│  2. FRÁGIL: un error de offset y todo se desalinea             │
│     ¿Es offset+6 o offset+7? Fácil equivocarse                │
│                                                                │
│  3. INCOMPLETO: ¿Cómo representar Option<T>?                   │
│     ¿Cómo representar Vec<T> de tamaño variable?              │
│     ¿Cómo representar enums con variantes?                     │
│                                                                │
│  4. INEFICIENTE: muchos inputs se rechazan por longitud        │
│     El fuzzer desperdicia ciclos en inputs demasiado cortos    │
│                                                                │
│  5. NO MANTIENE INVARIANTES: cualquier combinación de bytes    │
│     es válida, pero no cualquier Config lo es                  │
│     port=0 podría ser inválido pero el fuzzer lo genera        │
│                                                                │
│  6. NO COMPOSABLE: si Config cambia, hay que reescribir       │
│     todo el parsing                                            │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### La solución: Arbitrary

```rust
// Con Arbitrary, TODO lo anterior se reduce a:
use arbitrary::Arbitrary;

#[derive(Arbitrary, Debug)]
pub struct Config {
    pub name: String,
    pub port: u16,
    pub max_connections: u32,
    pub use_tls: bool,
    pub timeout_ms: Option<u64>,
}

fuzz_target!(|config: Config| {
    let _ = apply_config(&config);
});
```

El trait `Arbitrary` convierte bytes crudos en tipos Rust estructurados, automáticamente.

---

## 2. Qué es el trait Arbitrary

`Arbitrary` es un trait de Rust definido en el crate `arbitrary` que describe cómo construir una instancia de un tipo a partir de bytes no estructurados.

### Definición del trait

```rust
pub trait Arbitrary<'a>: Sized {
    /// Construir una instancia a partir de datos no estructurados.
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error>;

    /// Dar una pista sobre cuántos bytes necesita este tipo.
    fn size_hint(depth: usize) -> (usize, Option<usize>) {
        (0, None)  // default: no sabemos
    }

    /// Intentar reducir este valor a algo más simple.
    fn arbitrary_take_rest(u: Unstructured<'a>) -> Result<Self, Error> {
        Self::arbitrary(&mut u)  // default: igual que arbitrary
    }
}
```

### Analogía con la S01

En S01/T04 (Escribir harnesses en C), implementamos un `FuzzedDataProvider` manual para C:

```c
// C (S01/T04): parsing manual
typedef struct {
    const uint8_t *data;
    size_t size;
    size_t pos;
} FuzzInput;

uint32_t fuzz_consume_uint32(FuzzInput *fi);
uint8_t  fuzz_consume_uint8(FuzzInput *fi);
bool     fuzz_consume_bool(FuzzInput *fi);
```

`Arbitrary` en Rust es exactamente lo mismo, pero **automatizado por el compilador** a través de `derive`:

```
C (manual):                          Rust (automático):
─────────────                        ──────────────────
FuzzInput fi;                        Unstructured<'a>
fuzz_consume_uint32(&fi) → u32       u32::arbitrary(&mut u) → u32
fuzz_consume_bool(&fi)   → bool      bool::arbitrary(&mut u) → bool
// struct → escribir a mano          #[derive(Arbitrary)] → automático
```

---

## 3. Arquitectura: de bytes a tipos

```
                      Fuzzer (libFuzzer)
                           │
                    bytes aleatorios
                    [0xA3, 0x7F, 0x00, 0x12, 0xFF, 0x01, 0x48, 0x65, ...]
                           │
                           ▼
                ┌─────────────────────┐
                │   Unstructured<'a>  │
                │                     │
                │  Cursor sobre bytes │
                │  pos: 0             │
                │  remaining: 47      │
                └──────────┬──────────┘
                           │
          ┌────────────────┼────────────────┐
          │                │                │
          ▼                ▼                ▼
    ┌───────────┐   ┌───────────┐   ┌───────────┐
    │ consume   │   │ consume   │   │ consume   │
    │ u16       │   │ bool      │   │ String    │
    │           │   │           │   │           │
    │ Lee 2     │   │ Lee 1     │   │ Lee len   │
    │ bytes     │   │ byte      │   │ + bytes   │
    │ pos: 0→2  │   │ pos: 2→3  │   │ pos: 3→N  │
    └─────┬─────┘   └─────┬─────┘   └─────┬─────┘
          │                │                │
          ▼                ▼                ▼
       port: 0x7FA3     tls: false      name: "He..."
          │                │                │
          └────────────────┼────────────────┘
                           │
                           ▼
                    ┌──────────────┐
                    │   Config {   │
                    │     name,    │
                    │     port,    │
                    │     tls,     │
                    │     ...      │
                    │   }          │
                    └──────────────┘
```

### El flujo paso a paso

1. **libFuzzer** genera un buffer de bytes aleatorios
2. **Unstructured** envuelve ese buffer como un cursor con posición
3. **Arbitrary::arbitrary()** consume bytes secuencialmente para construir cada campo
4. Si los bytes se agotan antes de terminar, retorna `Err(Error::NotEnoughData)`
5. Si la construcción tiene éxito, el tipo completo se pasa al closure de `fuzz_target!`

### Invariante clave

```
Mismos bytes → mismo tipo construido (determinista)

[0xA3, 0x7F, 0x00, 0x12, ...] → siempre produce Config {
    name: "...",   // siempre el mismo name
    port: 32675,   // siempre el mismo port
    ...
}
```

Esto es fundamental para reproducibilidad: si el fuzzer encuentra un crash, los mismos bytes siempre producen el mismo input estructurado.

---

## 4. El crate arbitrary

### Instalación

Para usar `Arbitrary` en fuzzing, necesitas dos cosas:

1. El crate `arbitrary` como dependencia del crate que define tus tipos (para `derive`)
2. La integración con `libfuzzer-sys` (ya incluida en `fuzz/Cargo.toml`)

### En tu Cargo.toml principal (si tus tipos usan derive)

```toml
# Cargo.toml del proyecto
[dependencies]
arbitrary = { version = "1", optional = true, features = ["derive"] }

[features]
default = []
fuzzing = ["arbitrary"]   # feature gate para no cargar arbitrary en producción
```

### En fuzz/Cargo.toml

```toml
# fuzz/Cargo.toml
[dependencies]
libfuzzer-sys = "0.11"
arbitrary = { version = "1", features = ["derive"] }
my_project = { path = "..", features = ["fuzzing"] }
```

### Feature gate: por qué es importante

```rust
// src/types.rs — en tu proyecto principal

// Solo derivar Arbitrary cuando se activa la feature "fuzzing"
#[derive(Debug, Clone, PartialEq)]
#[cfg_attr(feature = "fuzzing", derive(arbitrary::Arbitrary))]
pub struct Config {
    pub name: String,
    pub port: u16,
    pub max_connections: u32,
    pub use_tls: bool,
}

// Sin feature gate:
// - arbitrary es una dependencia de producción (innecesaria)
// - Aumenta tiempo de compilación para usuarios normales
// - Expone tipos internos que solo sirven para fuzzing

// Con feature gate:
// - cargo build             → NO compila arbitrary
// - cargo fuzz run target   → SÍ compila arbitrary (por features = ["fuzzing"])
```

### Alternativa: derive solo en fuzz/

Si no quieres modificar tu crate principal, puedes definir los tipos Arbitrary solo en el directorio de fuzzing:

```rust
// fuzz/fuzz_targets/fuzz_config.rs
#![no_main]
use libfuzzer_sys::fuzz_target;
use arbitrary::Arbitrary;
use my_project::Config;

// Wrapper que implementa Arbitrary
#[derive(Arbitrary, Debug)]
struct FuzzConfig {
    name: String,
    port: u16,
    max_connections: u32,
    use_tls: bool,
}

impl From<FuzzConfig> for Config {
    fn from(fc: FuzzConfig) -> Config {
        Config {
            name: fc.name,
            port: fc.port,
            max_connections: fc.max_connections,
            use_tls: fc.use_tls,
        }
    }
}

fuzz_target!(|fc: FuzzConfig| {
    let config: Config = fc.into();
    let _ = my_project::apply_config(&config);
});
```

---

## 5. Unstructured: la fuente de bytes

`Unstructured<'a>` es el tipo que envuelve el buffer de bytes del fuzzer y proporciona métodos para consumir datos de forma controlada.

### API principal

```rust
pub struct Unstructured<'a> {
    data: &'a [u8],
    // ... campos internos
}

impl<'a> Unstructured<'a> {
    /// Crear desde un slice de bytes
    pub fn new(data: &'a [u8]) -> Self;

    /// Cuántos bytes quedan sin consumir
    pub fn len(&self) -> usize;

    /// ¿Se agotaron los bytes?
    pub fn is_empty(&self) -> bool;

    /// Consumir N bytes crudos
    pub fn bytes(&mut self, count: usize) -> Result<&'a [u8], Error>;

    /// Consumir un entero en un rango
    pub fn int_in_range<T: Int>(&mut self, range: T..=T) -> Result<T, Error>;

    /// Consumir un ratio (0.0 a 1.0)
    pub fn ratio<T: Int>(&mut self, num: T, denom: T) -> Result<bool, Error>;

    /// Consumir un valor Arbitrary de cualquier tipo
    pub fn arbitrary<T: Arbitrary<'a>>(&mut self) -> Result<T, Error>;

    /// Elegir un elemento de un slice
    pub fn choose<T>(&mut self, choices: &'a [T]) -> Result<&'a T, Error>;
}
```

### Métodos clave en detalle

#### bytes: consumir bytes crudos

```rust
fn arbitrary(u: &mut Unstructured<'_>) -> Result<Self, Error> {
    let raw = u.bytes(16)?;  // Exactamente 16 bytes
    // raw: &[u8] de longitud 16
    // Si quedan menos de 16 bytes → Err(NotEnoughData)
    Ok(/* construir tipo */)
}
```

#### int_in_range: enteros acotados

```rust
fn arbitrary(u: &mut Unstructured<'_>) -> Result<Self, Error> {
    let port = u.int_in_range(1024..=65535)?;     // u16 entre 1024 y 65535
    let month = u.int_in_range(1..=12)?;           // u8 entre 1 y 12
    let count = u.int_in_range(0..=100)?;           // u32 entre 0 y 100
    let offset = u.int_in_range(-128..=127)?;       // i8 completo
    // ...
}
```

#### ratio: decisiones ponderadas

```rust
fn arbitrary(u: &mut Unstructured<'_>) -> Result<Self, Error> {
    // true con probabilidad 1/3, false con probabilidad 2/3
    let enable_feature = u.ratio(1, 3)?;

    // true con probabilidad 9/10 (90%)
    let is_valid = u.ratio(9, 10)?;

    // true con probabilidad 1/2 (50%) — equivalente a bool::arbitrary
    let coin = u.ratio(1, 2)?;
    // ...
}
```

#### choose: elegir de un conjunto

```rust
fn arbitrary(u: &mut Unstructured<'_>) -> Result<Self, Error> {
    let methods = &["GET", "POST", "PUT", "DELETE", "PATCH"];
    let method = u.choose(methods)?;
    // method: &str, uno de los 5 métodos

    let status_codes = &[200u16, 201, 204, 301, 400, 401, 403, 404, 500];
    let status = u.choose(status_codes)?;
    // ...
}
```

#### arbitrary: consumir un tipo Arbitrary completo

```rust
fn arbitrary(u: &mut Unstructured<'_>) -> Result<Self, Error> {
    // Consumir un String (longitud variable + bytes UTF-8)
    let name: String = u.arbitrary()?;

    // Consumir un Vec<u32> (longitud variable + elementos)
    let data: Vec<u32> = u.arbitrary()?;

    // Consumir un Option<bool> (1 byte de discriminante + posible bool)
    let flag: Option<bool> = u.arbitrary()?;
    // ...
}
```

### Cómo Unstructured consume bytes

```
Buffer inicial: [0xA3, 0x7F, 0x00, 0x12, 0xFF, 0x01, 0x48, 0x65, 0x6C, 0x6C, 0x6F]
                 ↑
                 pos = 0, remaining = 11 bytes

Operación 1: u16::arbitrary()
  → Lee 2 bytes: [0xA3, 0x7F] → 0x7FA3 = 32675
  → pos = 2, remaining = 9

Operación 2: bool::arbitrary()
  → Lee 1 byte: [0x00] → false (0 = false, cualquier otro = true)
  → pos = 3, remaining = 8

Operación 3: u32::arbitrary()
  → Lee 4 bytes: [0x12, 0xFF, 0x01, 0x48] → 0x4801FF12
  → pos = 7, remaining = 4

Operación 4: String::arbitrary()
  → Lee 1 byte para longitud: [0x65] → len = 101
  → remaining = 3, pero 101 > 3 → lee solo 3 bytes
  → [0x6C, 0x6C, 0x6F] → intenta UTF-8 → "llo"
  → pos = 11, remaining = 0

Operación 5: u8::arbitrary()
  → remaining = 0 → Err(NotEnoughData)
  → TODO el valor Arbitrary falla
```

---

## 6. Tipos que ya implementan Arbitrary

El crate `arbitrary` proporciona implementaciones de `Arbitrary` para todos los tipos primitivos y muchos tipos de la librería estándar:

### Tipos primitivos

| Tipo | Bytes consumidos | Comportamiento |
|---|---|---|
| `bool` | 1 | `0` → `false`, cualquier otro → `true` |
| `u8` | 1 | Valor directo |
| `u16` | 2 | Little-endian |
| `u32` | 4 | Little-endian |
| `u64` | 8 | Little-endian |
| `u128` | 16 | Little-endian |
| `usize` | 4 u 8 | Depende de la plataforma |
| `i8` | 1 | Valor directo (con signo) |
| `i16` | 2 | Little-endian |
| `i32` | 4 | Little-endian |
| `i64` | 8 | Little-endian |
| `i128` | 16 | Little-endian |
| `isize` | 4 u 8 | Depende de la plataforma |
| `f32` | 4 | Cualquier patrón de bits (incluyendo NaN, Inf) |
| `f64` | 8 | Cualquier patrón de bits (incluyendo NaN, Inf) |
| `char` | 4 | Genera codepoint unicode válido |

### Tipos compuestos de la stdlib

| Tipo | Comportamiento |
|---|---|
| `String` | Longitud variable + bytes UTF-8 válidos |
| `&str` | Similar a String (lifetime del buffer) |
| `Vec<T>` | Longitud variable + `T::arbitrary()` repetido |
| `Box<T>` | `T::arbitrary()` en heap |
| `Arc<T>` | `T::arbitrary()` envuelto en Arc |
| `Rc<T>` | `T::arbitrary()` envuelto en Rc |
| `Option<T>` | 1 byte discriminante: `None` o `Some(T::arbitrary())` |
| `Result<T, E>` | 1 byte discriminante: `Ok(T)` o `Err(E)` |
| `(A, B, C, ...)` | Tuplas hasta 26 elementos |
| `[T; N]` | Array fijo: `T::arbitrary()` N veces |
| `HashMap<K, V>` | Longitud variable + pares key-value |
| `HashSet<T>` | Longitud variable + elementos |
| `BTreeMap<K, V>` | Longitud variable + pares key-value |
| `BTreeSet<T>` | Longitud variable + elementos |
| `BinaryHeap<T>` | Longitud variable + elementos |
| `VecDeque<T>` | Longitud variable + elementos |
| `LinkedList<T>` | Longitud variable + elementos |
| `Cow<'a, T>` | Owned o Borrowed |
| `Cell<T>` | `T::arbitrary()` envuelto |
| `RefCell<T>` | `T::arbitrary()` envuelto |
| `Wrapping<T>` | `T::arbitrary()` envuelto |
| `NonZeroU8..U128` | Valor distinto de cero (reintenta si sale 0) |
| `Duration` | secs: u64 + nanos: u32 (acotado a < 1_000_000_000) |
| `Range<T>` | start + end (puede ser vacío) |
| `Ipv4Addr` | 4 bytes |
| `Ipv6Addr` | 16 bytes |
| `SocketAddr` | Ipv4/v6 + port |

### Ausencias notables

Los siguientes tipos **NO** implementan Arbitrary (por buenas razones):

| Tipo | Por qué no |
|---|---|
| `File` | Requiere I/O real |
| `TcpStream` | Requiere red real |
| `Thread` | Requiere runtime |
| `Mutex<T>` | Estado de sincronización no serializable |
| `Path` / `PathBuf` | Implementan, pero los paths generados son arbitrarios (no rutas reales del filesystem) |

---

## 7. derive(Arbitrary): generación automática

`#[derive(Arbitrary)]` genera automáticamente la implementación de `Arbitrary` para tus tipos. Es la forma más común de usar Arbitrary.

### Sintaxis

```rust
use arbitrary::Arbitrary;

#[derive(Arbitrary, Debug)]
struct MyStruct {
    field1: u32,
    field2: String,
    field3: bool,
}
```

### Requisito: todos los campos deben implementar Arbitrary

```rust
// ✓ COMPILA: todos los campos implementan Arbitrary
#[derive(Arbitrary, Debug)]
struct Good {
    name: String,        // String: ✓ implementa Arbitrary
    count: u32,          // u32: ✓
    items: Vec<u8>,      // Vec<u8>: ✓
    flag: Option<bool>,  // Option<bool>: ✓
}

// ✗ NO COMPILA: File no implementa Arbitrary
#[derive(Arbitrary, Debug)]
struct Bad {
    name: String,
    file: std::fs::File,  // ✗ File no implementa Arbitrary
}
// error[E0277]: the trait bound `File: Arbitrary<'_>` is not satisfied
```

### Debug es necesario (convención)

`Debug` no es técnicamente requerido por `Arbitrary`, pero sí por `fuzz_target!` para poder imprimir el valor cuando hay un crash:

```rust
// ✓ RECOMENDADO: siempre derivar Debug junto con Arbitrary
#[derive(Arbitrary, Debug)]
struct MyInput { ... }

// Sin Debug, fuzz_target! no puede mostrar el input que causó el crash:
// thread panicked at '...'
// Input: <cannot display>   ← inútil para debugging
```

---

## 8. Cómo funciona derive internamente

Cuando escribes `#[derive(Arbitrary)]`, el macro procedural genera una implementación como esta:

```rust
#[derive(Debug)]
struct Config {
    name: String,
    port: u16,
    use_tls: bool,
}

// El derive genera (simplificado):
impl<'a> Arbitrary<'a> for Config {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, arbitrary::Error> {
        Ok(Config {
            name: <String as Arbitrary>::arbitrary(u)?,
            port: <u16 as Arbitrary>::arbitrary(u)?,
            use_tls: <bool as Arbitrary>::arbitrary(u)?,
        })
    }

    fn size_hint(depth: usize) -> (usize, Option<usize>) {
        // Combinar los size_hints de todos los campos
        arbitrary::size_hint::and_all(&[
            <String as Arbitrary>::size_hint(depth),
            <u16 as Arbitrary>::size_hint(depth),
            <bool as Arbitrary>::size_hint(depth),
        ])
    }
}
```

### Para enums, el derive genera un match

```rust
#[derive(Debug)]
enum Shape {
    Circle(f64),
    Rectangle(f64, f64),
    Triangle { a: f64, b: f64, c: f64 },
    Point,
}

// El derive genera (simplificado):
impl<'a> Arbitrary<'a> for Shape {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, arbitrary::Error> {
        // Elegir variante basándose en un byte
        let variant = u.int_in_range(0..=3u8)?;
        match variant {
            0 => Ok(Shape::Circle(u.arbitrary()?)),
            1 => Ok(Shape::Rectangle(u.arbitrary()?, u.arbitrary()?)),
            2 => Ok(Shape::Triangle {
                a: u.arbitrary()?,
                b: u.arbitrary()?,
                c: u.arbitrary()?,
            }),
            3 => Ok(Shape::Point),
            _ => unreachable!(),
        }
    }
}
```

### Observaciones clave

```
1. Los campos se generan EN ORDEN de declaración
   → Cambiar el orden de los campos cambia qué bytes
     corresponden a qué campo

2. Los enums usan un byte extra para elegir la variante
   → 1 byte para hasta 256 variantes

3. El derive es RECURSIVO
   → Si un campo es un struct con Arbitrary, llama a su
     implementación

4. No hay validación de dominio
   → derive genera CUALQUIER valor válido del tipo Rust
   → port puede ser 0, name puede estar vacío, etc.
```

---

## 9. Usar Arbitrary en fuzz_target!

### Forma básica

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use arbitrary::Arbitrary;

#[derive(Arbitrary, Debug)]
struct MyInput {
    x: u32,
    y: u32,
    op: u8,
}

fuzz_target!(|input: MyInput| {
    // 'input' ya es un MyInput completamente construido
    // No necesitas parsear bytes manualmente
    match input.op % 4 {
        0 => { let _ = input.x.checked_add(input.y); }
        1 => { let _ = input.x.checked_sub(input.y); }
        2 => { let _ = input.x.checked_mul(input.y); }
        3 => {
            if input.y != 0 {
                let _ = input.x / input.y;
            }
        }
        _ => unreachable!(),
    }
});
```

### Cómo funciona internamente

Cuando pones un tipo que implementa `Arbitrary` como parámetro de `fuzz_target!`, la macro internamente:

```rust
// Lo que escribes:
fuzz_target!(|input: MyInput| { ... });

// Lo que genera (simplificado):
fn __fuzz_test_input(data: &[u8]) {
    // 1. Crear Unstructured desde los bytes del fuzzer
    let mut u = arbitrary::Unstructured::new(data);

    // 2. Intentar construir MyInput desde los bytes
    let input: MyInput = match <MyInput as Arbitrary>::arbitrary(&mut u) {
        Ok(v) => v,
        Err(_) => return,  // Bytes insuficientes → descartar silenciosamente
    };

    // 3. Ejecutar tu closure
    { /* tu código con 'input' */ }
}
```

### Implicación: no todos los bytes producen un input válido

Si el fuzzer genera un buffer demasiado corto para construir el tipo completo, la ejecución se descarta silenciosamente (no es un crash). El fuzzer aprende que necesita generar buffers más largos.

```
Buffer: [0xA3]  (1 byte)
→ MyInput necesita al menos 4+4+1 = 9 bytes
→ Err(NotEnoughData)
→ return (sin ejecutar el closure)

Buffer: [0xA3, 0x7F, 0x00, 0x12, 0xFF, 0x01, 0x48, 0x65, 0x6C]  (9 bytes)
→ x = 0x12007FA3, y = 0x480001FF, op = 0x65
→ Ok(MyInput { x, y, op })
→ Ejecutar closure
```

---

## 10. Structs simples con derive

### Ejemplo 1: punto 2D

```rust
use arbitrary::Arbitrary;

#[derive(Arbitrary, Debug, Clone, PartialEq)]
struct Point {
    x: f64,
    y: f64,
}

// En el harness:
fuzz_target!(|point: Point| {
    let distance = (point.x * point.x + point.y * point.y).sqrt();
    // Verificar que la distancia es no-negativa (podría fallar con NaN)
    if !distance.is_nan() {
        assert!(distance >= 0.0, "Distance is negative: {}", distance);
    }
});
```

### Ejemplo 2: configuración con múltiples tipos

```rust
#[derive(Arbitrary, Debug)]
struct HttpRequest {
    method: HttpMethod,
    path: String,
    headers: Vec<(String, String)>,
    body: Option<Vec<u8>>,
}

#[derive(Arbitrary, Debug)]
enum HttpMethod {
    Get,
    Post,
    Put,
    Delete,
    Patch,
    Head,
    Options,
}

fuzz_target!(|req: HttpRequest| {
    let _ = my_server::handle_request(&req);
});
```

### Ejemplo 3: struct con tipos de la stdlib

```rust
#[derive(Arbitrary, Debug)]
struct DatabaseQuery {
    table: String,
    columns: Vec<String>,
    filter: Option<FilterExpr>,
    limit: Option<u32>,
    offset: u32,
}

#[derive(Arbitrary, Debug)]
struct FilterExpr {
    column: String,
    operator: FilterOp,
    value: String,
}

#[derive(Arbitrary, Debug)]
enum FilterOp {
    Eq,
    Ne,
    Lt,
    Gt,
    Le,
    Ge,
    Like,
}

fuzz_target!(|query: DatabaseQuery| {
    let sql = my_lib::build_sql(&query);
    // Verificar que el SQL generado es válido (no tiene inyección)
    assert!(!sql.contains("--"), "SQL injection in generated query");
    assert!(!sql.contains(';'), "Multiple statements in query");
});
```

---

## 11. Enums con derive

### Enum simple (unit variants)

```rust
#[derive(Arbitrary, Debug)]
enum Color {
    Red,
    Green,
    Blue,
    Yellow,
    Custom(u8, u8, u8),  // RGB
}

// derive genera:
// 1 byte para elegir variante (0-4)
// Si Custom: 3 bytes más para (u8, u8, u8)
```

### Enum con datos (data-carrying variants)

```rust
#[derive(Arbitrary, Debug)]
enum JsonValue {
    Null,
    Bool(bool),
    Number(f64),
    Str(String),
    Array(Vec<JsonValue>),          // ¡Recursivo!
    Object(Vec<(String, JsonValue)>), // ¡Recursivo!
}

// Nota: tipos recursivos necesitan cuidado especial.
// Vec<JsonValue> puede crecer indefinidamente.
// Ver sección 19 para soluciones.
```

### Enum con campos nombrados

```rust
#[derive(Arbitrary, Debug)]
enum Shape {
    Circle { radius: f64 },
    Rectangle { width: f64, height: f64 },
    Triangle { base: f64, height: f64 },
    Polygon { sides: Vec<(f64, f64)> },
}

fuzz_target!(|shape: Shape| {
    let area = match &shape {
        Shape::Circle { radius } => std::f64::consts::PI * radius * radius,
        Shape::Rectangle { width, height } => width * height,
        Shape::Triangle { base, height } => 0.5 * base * height,
        Shape::Polygon { sides } => compute_polygon_area(sides),
    };
    // Verificar: área nunca debería ser negativa (excepto con inputs degenerados)
    if area.is_finite() {
        // Para shapes con dimensiones negativas, el cálculo puede dar negativo
        // ¿Es eso un bug? Depende de la especificación
    }
});
```

### Distribución de variantes

Con derive, cada variante tiene **igual probabilidad** de ser seleccionada:

```
enum Color:   5 variantes → cada una ~20% de probabilidad
enum Shape:   4 variantes → cada una ~25% de probabilidad
```

Si necesitas distribución desigual, debes implementar `Arbitrary` manualmente (sección 13).

---

## 12. Structs con campos complejos

### Structs anidados

```rust
#[derive(Arbitrary, Debug)]
struct Address {
    street: String,
    city: String,
    zip: String,
    country: CountryCode,
}

#[derive(Arbitrary, Debug)]
enum CountryCode {
    Us,
    Mx,
    Ca,
    Gb,
    De,
    Fr,
    Jp,
}

#[derive(Arbitrary, Debug)]
struct Person {
    name: String,
    age: u8,
    address: Address,          // struct anidado
    emails: Vec<String>,       // colección
    phone: Option<String>,     // opcional
}

#[derive(Arbitrary, Debug)]
struct Company {
    name: String,
    employees: Vec<Person>,    // Vec de struct complejo
    ceo: Person,               // struct como campo directo
    founded_year: u16,
    active: bool,
}

fuzz_target!(|company: Company| {
    let report = my_lib::generate_report(&company);
    assert!(report.len() > 0, "Empty report for company {:?}", company.name);
});
```

### Structs con genéricos

```rust
#[derive(Arbitrary, Debug)]
struct Pair<A: for<'a> Arbitrary<'a>, B: for<'a> Arbitrary<'a>> {
    first: A,
    second: B,
}

// Uso:
fuzz_target!(|pair: Pair<u32, String>| {
    // pair.first: u32
    // pair.second: String
});
```

### Structs con arrays fijos

```rust
#[derive(Arbitrary, Debug)]
struct Matrix3x3 {
    data: [[f64; 3]; 3],  // Array fijo: siempre 3x3
}

fuzz_target!(|m: Matrix3x3| {
    let det = my_math::determinant(&m.data);
    let inv = my_math::inverse(&m.data);

    if let Some(inv) = inv {
        // Verificar: M * M^(-1) ≈ I
        let product = my_math::multiply(&m.data, &inv);
        for i in 0..3 {
            for j in 0..3 {
                let expected = if i == j { 1.0 } else { 0.0 };
                let diff = (product[i][j] - expected).abs();
                if diff > 1e-6 && det.abs() > 1e-10 {
                    panic!(
                        "Inverse verification failed: M*M^-1[{}][{}] = {}, expected {}",
                        i, j, product[i][j], expected
                    );
                }
            }
        }
    }
});
```

---

## 13. Implementación manual de Arbitrary

A veces `derive` no es suficiente. Necesitas implementar `Arbitrary` manualmente cuando:

- Quieres restringir los valores generados a un dominio específico
- Quieres distribuciones no uniformes
- Tienes invariantes entre campos que derive no puede expresar
- Quieres controlar exactamente cuántos bytes se consumen

### Ejemplo: ángulo normalizado (0 a 360)

```rust
use arbitrary::{Arbitrary, Unstructured, Error};

#[derive(Debug, Clone)]
struct Angle(f64);  // Siempre entre 0.0 y 360.0

impl<'a> Arbitrary<'a> for Angle {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        // Generar un u16 y mapearlo al rango [0, 360)
        let raw: u16 = u.arbitrary()?;
        let angle = (raw as f64 / u16::MAX as f64) * 360.0;
        Ok(Angle(angle))
    }

    fn size_hint(_depth: usize) -> (usize, Option<usize>) {
        // Siempre consume exactamente 2 bytes
        (2, Some(2))
    }
}
```

### Ejemplo: string alfanumérico

```rust
#[derive(Debug, Clone)]
struct AlphanumericString(String);

impl<'a> Arbitrary<'a> for AlphanumericString {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let charset = b"abcdefghijklmnopqrstuvwxyz\
                        ABCDEFGHIJKLMNOPQRSTUVWXYZ\
                        0123456789";

        // Longitud entre 1 y 64
        let len = u.int_in_range(1..=64usize)?;
        let mut result = String::with_capacity(len);

        for _ in 0..len {
            let idx = u.int_in_range(0..=(charset.len() - 1))?;
            result.push(charset[idx] as char);
        }

        Ok(AlphanumericString(result))
    }

    fn size_hint(depth: usize) -> (usize, Option<usize>) {
        // Al menos 1 byte para longitud + 1 byte para un char
        // Máximo: variable
        (2, None)
    }
}
```

### Ejemplo: par ordenado

```rust
#[derive(Debug, Clone)]
struct OrderedPair {
    lo: u32,
    hi: u32,
}
// Invariante: lo <= hi

impl<'a> Arbitrary<'a> for OrderedPair {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let a: u32 = u.arbitrary()?;
        let b: u32 = u.arbitrary()?;
        let (lo, hi) = if a <= b { (a, b) } else { (b, a) };
        Ok(OrderedPair { lo, hi })
    }

    fn size_hint(_depth: usize) -> (usize, Option<usize>) {
        (8, Some(8))  // 4 bytes + 4 bytes
    }
}
```

### Ejemplo: enum con distribución no uniforme

```rust
#[derive(Debug)]
enum RequestType {
    Read,    // 70% de las veces
    Write,   // 20% de las veces
    Delete,  // 10% de las veces
}

impl<'a> Arbitrary<'a> for RequestType {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let n: u8 = u.arbitrary()?;
        Ok(match n % 10 {
            0..=6 => RequestType::Read,     // 7/10 = 70%
            7..=8 => RequestType::Write,    // 2/10 = 20%
            9     => RequestType::Delete,   // 1/10 = 10%
            _ => unreachable!(),
        })
    }

    fn size_hint(_depth: usize) -> (usize, Option<usize>) {
        (1, Some(1))
    }
}
```

### Template para implementación manual

```rust
use arbitrary::{Arbitrary, Unstructured, Error};

#[derive(Debug)]
struct MyType { /* campos */ }

impl<'a> Arbitrary<'a> for MyType {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        // 1. Consumir bytes de 'u' para construir cada campo
        // 2. Aplicar restricciones de dominio
        // 3. Mantener invariantes entre campos
        // 4. Retornar Ok(Self { ... })
        todo!()
    }

    fn size_hint(depth: usize) -> (usize, Option<usize>) {
        // (mínimo de bytes, máximo de bytes)
        // Retornar None como máximo si el tamaño es variable
        (0, None)
    }
}
```

---

## 14. Cuándo implementar manualmente vs derivar

```
┌──────────────────────────────────────────────────────────────────┐
│        Decisión: derive(Arbitrary) vs implementación manual      │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Usar derive cuando:                                             │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ ✓ Todos los campos son tipos que implementan Arbitrary     │  │
│  │ ✓ Cualquier combinación de valores es válida               │  │
│  │ ✓ No hay invariantes entre campos                          │  │
│  │ ✓ La distribución uniforme es aceptable                    │  │
│  │ ✓ Quieres rapidez de implementación                       │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  Implementar manualmente cuando:                                 │
│  ┌────────────────────────────────────────────────────────────┐  │
│  │ ✓ Los valores deben estar en un rango específico           │  │
│  │   (ej: port 1024-65535, age 0-150)                        │  │
│  │ ✓ Hay invariantes entre campos                             │  │
│  │   (ej: start <= end, width * height <= MAX_AREA)          │  │
│  │ ✓ Necesitas distribución no uniforme                       │  │
│  │   (ej: 90% valid inputs, 10% edge cases)                 │  │
│  │ ✓ Un campo depende del valor de otro                       │  │
│  │   (ej: array de exactamente 'count' elementos)            │  │
│  │ ✓ Hay campos que no implementan Arbitrary                  │  │
│  │ ✓ Quieres generar strings con formato específico           │  │
│  │   (ej: emails, URLs, IPs, fechas)                        │  │
│  └────────────────────────────────────────────────────────────┘  │
│                                                                  │
│  Tabla de decisión rápida:                                       │
│  ┌──────────────────────────────────┬───────────────────────┐   │
│  │ Escenario                        │ Recomendación          │   │
│  ├──────────────────────────────────┼───────────────────────┤   │
│  │ Struct con u32, String, bool     │ derive                 │   │
│  │ Enum con variantes simples       │ derive                 │   │
│  │ Port 1-65535                     │ manual (int_in_range)  │   │
│  │ Porcentaje 0.0-100.0            │ manual                 │   │
│  │ Vector de exactamente N items    │ manual                 │   │
│  │ String que es un email válido    │ manual                 │   │
│  │ Struct con lo <= hi              │ manual                 │   │
│  │ Config con flags interdepend.    │ manual o derive+valid  │   │
│  └──────────────────────────────────┴───────────────────────┘   │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

---

## 15. Restricciones de dominio: valores válidos

El principal motivo para implementar `Arbitrary` manualmente es generar **solo valores válidos** para tu dominio.

### Problema con derive: valores fuera de dominio

```rust
#[derive(Arbitrary, Debug)]
struct ServerConfig {
    port: u16,          // derive genera 0-65535, pero puertos < 1024 son privilegiados
    max_connections: u32, // derive genera 0-4B, pero 0 no tiene sentido
    timeout_secs: u32,  // derive genera 0-4B, pero > 86400 (1 día) es sospechoso
    name: String,       // derive genera strings enormes o vacíos
}
```

Con derive, el fuzzer pierde tiempo explorando inputs que son inmediatamente rechazados por validación:

```rust
fuzz_target!(|config: ServerConfig| {
    // 90% de los inputs son rechazados por validate()
    if let Err(_) = validate(&config) {
        return;  // Desperdicio de ciclos del fuzzer
    }
    let _ = apply_config(&config);
});
```

### Solución: Arbitrary manual con restricciones

```rust
#[derive(Debug)]
struct ServerConfig {
    port: u16,
    max_connections: u32,
    timeout_secs: u32,
    name: String,
}

impl<'a> Arbitrary<'a> for ServerConfig {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        Ok(ServerConfig {
            port: u.int_in_range(1024..=65535)?,       // Solo puertos no-privilegiados
            max_connections: u.int_in_range(1..=10000)?, // Rango razonable
            timeout_secs: u.int_in_range(1..=3600)?,   // 1 seg a 1 hora
            name: {
                // Nombre alfanumérico de 1-32 chars
                let len = u.int_in_range(1..=32usize)?;
                let charset = b"abcdefghijklmnopqrstuvwxyz0123456789_-";
                let mut s = String::with_capacity(len);
                for _ in 0..len {
                    let idx = u.int_in_range(0..=(charset.len() - 1))?;
                    s.push(charset[idx] as char);
                }
                s
            },
        })
    }

    fn size_hint(_depth: usize) -> (usize, Option<usize>) {
        // port: 2 + max_conn: 4 + timeout: 4 + name_len: 1 + min 1 char: 1 = 12
        (12, None)
    }
}
```

Ahora **cada input generado pasa la validación**, y el fuzzer dedica el 100% de sus ciclos a ejercitar la lógica real de `apply_config`.

---

## 16. Rangos numéricos con int_in_range

`int_in_range` es el método más usado en implementaciones manuales de Arbitrary.

### Sintaxis

```rust
u.int_in_range(start..=end)?
```

### Ejemplos por tipo

```rust
// u8: mes (1-12)
let month: u8 = u.int_in_range(1..=12)?;

// u16: puerto HTTP (80, 443, 8080, 8443, o range)
let port: u16 = u.int_in_range(1..=65535)?;

// u32: tamaño de buffer razonable
let buf_size: u32 = u.int_in_range(1..=8192)?;

// i32: offset que puede ser negativo
let offset: i32 = u.int_in_range(-1000..=1000)?;

// usize: índice en un array de hasta N elementos
let idx: usize = u.int_in_range(0..=99)?;

// i64: timestamp (año 2000 a 2100 en epoch)
let timestamp: i64 = u.int_in_range(946684800..=4102444800)?;
```

### Patrón: elegir de un conjunto discreto

```rust
// En vez de int_in_range, usa choose para conjuntos pequeños:
let status_code: u16 = *u.choose(&[200u16, 201, 204, 301, 400, 401, 403, 404, 500, 503])?;

// Para potencias de 2:
let buffer_size: usize = *u.choose(&[64, 128, 256, 512, 1024, 2048, 4096])?;
```

### Patrón: distribución logarítmica para tamaños

En fuzzing, a menudo queremos muchos inputs pequeños y pocos grandes:

```rust
fn arbitrary_size(u: &mut Unstructured<'_>) -> Result<usize, Error> {
    // Distribución pesada hacia valores pequeños
    let category: u8 = u.int_in_range(0..=9)?;
    match category {
        0..=4 => u.int_in_range(0..=16usize),     // 50%: 0-16 bytes
        5..=7 => u.int_in_range(17..=256usize),    // 30%: 17-256 bytes
        8     => u.int_in_range(257..=4096usize),   // 10%: 257-4096 bytes
        9     => u.int_in_range(4097..=65536usize),  // 10%: grande
        _ => unreachable!(),
    }
}
```

---

## 17. Strings con restricciones

### String ASCII alfanumérico

```rust
fn arbitrary_alphanum(u: &mut Unstructured<'_>, max_len: usize) -> Result<String, Error> {
    let charset = b"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    let len = u.int_in_range(0..=max_len)?;
    let mut s = String::with_capacity(len);
    for _ in 0..len {
        let idx = u.int_in_range(0..=(charset.len() - 1))?;
        s.push(charset[idx] as char);
    }
    Ok(s)
}
```

### String que es un email

```rust
fn arbitrary_email(u: &mut Unstructured<'_>) -> Result<String, Error> {
    let local = arbitrary_alphanum(u, 20)?;
    let domain = arbitrary_alphanum(u, 15)?;
    let tlds = &["com", "org", "net", "io", "dev"];
    let tld = u.choose(tlds)?;

    if local.is_empty() || domain.is_empty() {
        return Err(Error::IncorrectFormat);
    }

    Ok(format!("{}@{}.{}", local, domain, tld))
}
```

### String que es un path

```rust
fn arbitrary_path(u: &mut Unstructured<'_>) -> Result<String, Error> {
    let depth = u.int_in_range(1..=5usize)?;
    let mut parts = Vec::with_capacity(depth);

    for _ in 0..depth {
        let part = arbitrary_alphanum(u, 12)?;
        if part.is_empty() {
            parts.push("dir".to_string());
        } else {
            parts.push(part);
        }
    }

    Ok(format!("/{}", parts.join("/")))
}
```

### String que es un identificador válido de Rust

```rust
fn arbitrary_identifier(u: &mut Unstructured<'_>) -> Result<String, Error> {
    let first_charset = b"abcdefghijklmnopqrstuvwxyz_";
    let rest_charset = b"abcdefghijklmnopqrstuvwxyz0123456789_";

    let len = u.int_in_range(1..=32usize)?;
    let mut s = String::with_capacity(len);

    // Primer char: letra o underscore
    let idx = u.int_in_range(0..=(first_charset.len() - 1))?;
    s.push(first_charset[idx] as char);

    // Resto: alfanumérico o underscore
    for _ in 1..len {
        let idx = u.int_in_range(0..=(rest_charset.len() - 1))?;
        s.push(rest_charset[idx] as char);
    }

    Ok(s)
}
```

---

## 18. Colecciones con tamaño acotado

### Problema: derive genera colecciones enormes

```rust
#[derive(Arbitrary, Debug)]
struct Input {
    items: Vec<u32>,  // derive puede generar un Vec con miles de elementos
                      // → lento de procesar, OOM, timeout
}
```

### Solución: acotar el tamaño manualmente

```rust
use arbitrary::{Arbitrary, Unstructured, Error};

#[derive(Debug)]
struct Input {
    items: Vec<u32>,
}

impl<'a> Arbitrary<'a> for Input {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        // Máximo 100 elementos
        let len = u.int_in_range(0..=100usize)?;
        let mut items = Vec::with_capacity(len);
        for _ in 0..len {
            items.push(u.arbitrary()?);
        }
        Ok(Input { items })
    }
}
```

### Helper genérico: bounded_vec

```rust
fn arbitrary_bounded_vec<'a, T: Arbitrary<'a>>(
    u: &mut Unstructured<'a>,
    max_len: usize,
) -> Result<Vec<T>, Error> {
    let len = u.int_in_range(0..=max_len)?;
    let mut vec = Vec::with_capacity(len);
    for _ in 0..len {
        vec.push(u.arbitrary()?);
    }
    Ok(vec)
}

// Uso:
impl<'a> Arbitrary<'a> for MyInput {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        Ok(MyInput {
            small_list: arbitrary_bounded_vec(u, 10)?,
            medium_list: arbitrary_bounded_vec(u, 100)?,
            large_list: arbitrary_bounded_vec(u, 1000)?,
        })
    }
}
```

### HashMap acotado

```rust
fn arbitrary_bounded_map<'a, K, V>(
    u: &mut Unstructured<'a>,
    max_len: usize,
) -> Result<HashMap<K, V>, Error>
where
    K: Arbitrary<'a> + Eq + std::hash::Hash,
    V: Arbitrary<'a>,
{
    let len = u.int_in_range(0..=max_len)?;
    let mut map = HashMap::with_capacity(len);
    for _ in 0..len {
        let key: K = u.arbitrary()?;
        let val: V = u.arbitrary()?;
        map.insert(key, val);
    }
    Ok(map)
}
```

---

## 19. Tipos recursivos y profundidad

### El problema: recursión infinita

```rust
#[derive(Arbitrary, Debug)]
enum Expr {
    Literal(i32),
    Add(Box<Expr>, Box<Expr>),   // Recursivo
    Mul(Box<Expr>, Box<Expr>),   // Recursivo
    Neg(Box<Expr>),              // Recursivo
}

// derive genera:
// 1/4 probabilidad de Literal → termina
// 3/4 probabilidad de recursión → sigue generando
//
// Probabilidad de profundidad > N: (3/4)^N
// Profundidad > 10: (3/4)^10 ≈ 5.6% — frecuente
// Profundidad > 50: (3/4)^50 ≈ 0.00006% — raro pero posible
//
// Con bytes suficientes, el fuzzer PUEDE generar árboles enormes
// → Stack overflow o timeout
```

### Solución 1: implementación manual con profundidad

```rust
use arbitrary::{Arbitrary, Unstructured, Error};

#[derive(Debug)]
enum Expr {
    Literal(i32),
    Add(Box<Expr>, Box<Expr>),
    Mul(Box<Expr>, Box<Expr>),
    Neg(Box<Expr>),
}

impl Expr {
    fn arbitrary_with_depth(u: &mut Unstructured<'_>, depth: usize) -> Result<Self, Error> {
        if depth == 0 || u.len() < 4 {
            // Forzar un literal cuando la profundidad se agota
            // o cuando quedan pocos bytes
            return Ok(Expr::Literal(u.arbitrary()?));
        }

        let variant: u8 = u.int_in_range(0..=3)?;
        match variant {
            0 => Ok(Expr::Literal(u.arbitrary()?)),
            1 => Ok(Expr::Add(
                Box::new(Expr::arbitrary_with_depth(u, depth - 1)?),
                Box::new(Expr::arbitrary_with_depth(u, depth - 1)?),
            )),
            2 => Ok(Expr::Mul(
                Box::new(Expr::arbitrary_with_depth(u, depth - 1)?),
                Box::new(Expr::arbitrary_with_depth(u, depth - 1)?),
            )),
            3 => Ok(Expr::Neg(
                Box::new(Expr::arbitrary_with_depth(u, depth - 1)?),
            )),
            _ => unreachable!(),
        }
    }
}

impl<'a> Arbitrary<'a> for Expr {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        const MAX_DEPTH: usize = 8;
        Expr::arbitrary_with_depth(u, MAX_DEPTH)
    }
}
```

### Solución 2: ponderar las variantes terminales

```rust
impl<'a> Arbitrary<'a> for Expr {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        // 60% Literal (terminal), 40% recursivo
        // Esto hace que los árboles sean naturalmente pequeños
        let choice: u8 = u.int_in_range(0..=9)?;
        match choice {
            0..=5 => Ok(Expr::Literal(u.arbitrary()?)),  // 60%
            6..=7 => Ok(Expr::Add(
                Box::new(u.arbitrary()?),
                Box::new(u.arbitrary()?),
            )),
            8 => Ok(Expr::Mul(
                Box::new(u.arbitrary()?),
                Box::new(u.arbitrary()?),
            )),
            9 => Ok(Expr::Neg(Box::new(u.arbitrary()?))),
            _ => unreachable!(),
        }
    }
}
```

### Solución 3: combinar ambas (la más robusta)

```rust
impl Expr {
    fn arbitrary_impl(u: &mut Unstructured<'_>, depth: usize) -> Result<Self, Error> {
        // Probabilidad de recursión decrece con la profundidad
        let max_variant = if depth > 6 { 0 }       // Solo literal
                          else if depth > 3 { 3 }    // 1/4 recursivo
                          else { 9 };                 // 6/10 recursivo

        let choice: u8 = u.int_in_range(0..=max_variant)?;
        match choice {
            0..=3 => Ok(Expr::Literal(u.arbitrary()?)),
            4..=6 => Ok(Expr::Add(
                Box::new(Expr::arbitrary_impl(u, depth + 1)?),
                Box::new(Expr::arbitrary_impl(u, depth + 1)?),
            )),
            7..=8 => Ok(Expr::Mul(
                Box::new(Expr::arbitrary_impl(u, depth + 1)?),
                Box::new(Expr::arbitrary_impl(u, depth + 1)?),
            )),
            9 => Ok(Expr::Neg(
                Box::new(Expr::arbitrary_impl(u, depth + 1)?),
            )),
            _ => unreachable!(),
        }
    }
}

impl<'a> Arbitrary<'a> for Expr {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        Expr::arbitrary_impl(u, 0)
    }
}
```

---

## 20. Combinando derive con campos manuales

A veces quieres derive para la mayoría de campos pero control manual sobre uno o dos. Puedes hacerlo con un tipo wrapper:

### Patrón: newtype para restricciones

```rust
use arbitrary::{Arbitrary, Unstructured, Error};

// Tipo wrapper con restricción
#[derive(Debug, Clone)]
struct Port(u16);

impl<'a> Arbitrary<'a> for Port {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        Ok(Port(u.int_in_range(1..=65535)?))
    }
}

// Tipo wrapper: porcentaje 0-100
#[derive(Debug, Clone)]
struct Percentage(u8);

impl<'a> Arbitrary<'a> for Percentage {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        Ok(Percentage(u.int_in_range(0..=100)?))
    }
}

// Tipo wrapper: string no vacío
#[derive(Debug, Clone)]
struct NonEmptyString(String);

impl<'a> Arbitrary<'a> for NonEmptyString {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let s: String = u.arbitrary()?;
        if s.is_empty() {
            // Si la string generada es vacía, agregar algo
            Ok(NonEmptyString("x".to_string()))
        } else if s.len() > 100 {
            Ok(NonEmptyString(s[..100].to_string()))
        } else {
            Ok(NonEmptyString(s))
        }
    }
}

// Ahora puedes usar derive normalmente:
#[derive(Arbitrary, Debug)]
struct Config {
    name: NonEmptyString,        // Siempre no vacío
    port: Port,                   // Siempre 1-65535
    cpu_threshold: Percentage,    // Siempre 0-100
    use_tls: bool,                // derive normal
    timeout_ms: Option<u32>,      // derive normal
}
```

### Ventaja de este patrón

```
1. Los newtypes son REUTILIZABLES entre múltiples structs
2. derive sigue funcionando para el struct principal
3. Los invariantes están ENCAPSULADOS en el newtype
4. Fácil de agregar/quitar restricciones
```

---

## 21. Arbitrary para secuencias de operaciones

Uno de los usos más poderosos de Arbitrary es generar **secuencias de operaciones** para fuzzear APIs stateful.

### Concepto

```
En vez de generar UN input, generar una SECUENCIA de acciones:

  [Insert(5), Insert(3), Get(5), Remove(3), Get(3), Insert(7), ...]

Cada acción es un enum con derive(Arbitrary).
La secuencia es un Vec<Action>.
```

### Ejemplo: fuzzear un HashMap custom

```rust
use arbitrary::Arbitrary;
use std::collections::HashMap;

// Las operaciones que puede hacer el fuzzer
#[derive(Arbitrary, Debug)]
enum MapOp {
    Insert { key: u8, value: u32 },
    Remove { key: u8 },
    Get { key: u8 },
    Contains { key: u8 },
    Clear,
    Len,
}

// El input del fuzzer es una secuencia de operaciones
#[derive(Arbitrary, Debug)]
struct MapFuzzInput {
    initial_capacity: u8,
    operations: Vec<MapOp>,
}

fuzz_target!(|input: MapFuzzInput| {
    if input.operations.len() > 200 {
        return;  // Acotar para evitar timeouts
    }

    let mut my_map = my_lib::MyMap::with_capacity(input.initial_capacity as usize);
    let mut reference = HashMap::new();  // Oracle: HashMap de stdlib

    for op in &input.operations {
        match op {
            MapOp::Insert { key, value } => {
                let my_result = my_map.insert(*key, *value);
                let ref_result = reference.insert(*key, *value);
                assert_eq!(my_result, ref_result,
                    "Insert({}, {}) diverged", key, value);
            }
            MapOp::Remove { key } => {
                let my_result = my_map.remove(key);
                let ref_result = reference.remove(key);
                assert_eq!(my_result, ref_result,
                    "Remove({}) diverged", key);
            }
            MapOp::Get { key } => {
                let my_result = my_map.get(key);
                let ref_result = reference.get(key);
                assert_eq!(my_result, ref_result,
                    "Get({}) diverged", key);
            }
            MapOp::Contains { key } => {
                let my_result = my_map.contains_key(key);
                let ref_result = reference.contains_key(key);
                assert_eq!(my_result, ref_result,
                    "Contains({}) diverged", key);
            }
            MapOp::Clear => {
                my_map.clear();
                reference.clear();
            }
            MapOp::Len => {
                assert_eq!(my_map.len(), reference.len(),
                    "Len diverged");
            }
        }

        // Invariante: len siempre consistente
        assert_eq!(my_map.len(), reference.len());
        assert_eq!(my_map.is_empty(), reference.is_empty());
    }
});
```

### Por qué es tan efectivo

```
┌──────────────────────────────────────────────────────────────┐
│       Por qué el fuzzing de secuencias de operaciones        │
│       es más efectivo que fuzzear operaciones individuales    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. ENCUENTRA BUGS DE ESTADO                                 │
│     Insert → Remove → Insert → Get                          │
│     (el Get después del re-insert puede fallar si            │
│      Remove deja estado corrupto)                            │
│                                                              │
│  2. ENCUENTRA INTERACCIONES                                  │
│     Clear → Insert → Len                                     │
│     (Clear puede no resetear el counter de len)              │
│                                                              │
│  3. COBERTURA MÁS PROFUNDA                                   │
│     Una sola operación solo ejercita un código path           │
│     Una secuencia de 20 operaciones alcanza estados           │
│     que ningún test unitario individual alcanzaría            │
│                                                              │
│  4. ORACLE AUTOMÁTICO                                        │
│     Comparar con HashMap de stdlib → cualquier               │
│     divergencia es un bug                                    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 22. Patrón command: fuzzing stateful con Arbitrary

El patrón command es una generalización del patrón de secuencias de operaciones. Es el patrón más poderoso para fuzzear estructuras de datos y APIs con estado.

### Estructura general

```rust
use arbitrary::Arbitrary;

// 1. Definir las operaciones posibles
#[derive(Arbitrary, Debug)]
enum Command {
    // Cada variante es una operación con sus parámetros
    Op1 { param1: Type1, param2: Type2 },
    Op2 { param: Type3 },
    Op3,
    // ...
}

// 2. El input del fuzzer
#[derive(Arbitrary, Debug)]
struct FuzzInput {
    initial_state: InitState,     // Estado inicial configurable
    commands: Vec<Command>,        // Secuencia de operaciones
}

// 3. Ejecutar la secuencia
fuzz_target!(|input: FuzzInput| {
    if input.commands.len() > MAX_COMMANDS {
        return;
    }

    let mut sut = SystemUnderTest::new(input.initial_state);
    let mut oracle = ReferenceImplementation::new(input.initial_state);

    for cmd in &input.commands {
        execute_command(&mut sut, &mut oracle, cmd);
    }
});

fn execute_command(sut: &mut Sut, oracle: &mut Oracle, cmd: &Command) {
    match cmd {
        Command::Op1 { param1, param2 } => {
            let sut_result = sut.op1(*param1, *param2);
            let oracle_result = oracle.op1(*param1, *param2);
            assert_eq!(sut_result, oracle_result);
        }
        // ... etc
    }
}
```

### Ejemplo completo: priority queue

```rust
use arbitrary::Arbitrary;
use std::collections::BinaryHeap;

#[derive(Arbitrary, Debug)]
enum PQCommand {
    Push(i32),
    Pop,
    Peek,
    Len,
    Clear,
    PushMultiple(Vec<i32>),
}

#[derive(Arbitrary, Debug)]
struct PQInput {
    commands: Vec<PQCommand>,
}

fuzz_target!(|input: PQInput| {
    if input.commands.len() > 500 {
        return;
    }

    let mut my_pq = my_lib::PriorityQueue::new();
    let mut oracle = BinaryHeap::new();

    for cmd in &input.commands {
        match cmd {
            PQCommand::Push(val) => {
                my_pq.push(*val);
                oracle.push(*val);
            }
            PQCommand::Pop => {
                let mine = my_pq.pop();
                let theirs = oracle.pop();
                assert_eq!(mine, theirs, "Pop diverged");
            }
            PQCommand::Peek => {
                let mine = my_pq.peek();
                let theirs = oracle.peek().copied();
                assert_eq!(mine, theirs, "Peek diverged");
            }
            PQCommand::Len => {
                assert_eq!(my_pq.len(), oracle.len(), "Len diverged");
            }
            PQCommand::Clear => {
                my_pq.clear();
                oracle.clear();
            }
            PQCommand::PushMultiple(values) => {
                if values.len() > 50 { continue; }
                for v in values {
                    my_pq.push(*v);
                    oracle.push(*v);
                }
            }
        }
    }
});
```

---

## 23. size_hint: optimizar la generación

`size_hint` le dice al fuzzer cuántos bytes mínimos necesita tu tipo. Esto ayuda al fuzzer a generar buffers del tamaño correcto.

### Definición

```rust
fn size_hint(depth: usize) -> (usize, Option<usize>)
//                              │            │
//                              │            └── Máximo (None = desconocido)
//                              └── Mínimo de bytes necesarios
```

### El parámetro depth

`depth` es un contador de recursión. Sirve para prevenir recursión infinita en `size_hint` de tipos recursivos:

```rust
fn size_hint(depth: usize) -> (usize, Option<usize>) {
    // Si la profundidad es muy grande, retornar conservador
    if depth > 20 {
        return (0, None);
    }
    // ... calcular normalmente
}
```

### Ejemplos

```rust
// Tipo de tamaño fijo
impl<'a> Arbitrary<'a> for Angle {
    fn size_hint(_depth: usize) -> (usize, Option<usize>) {
        (2, Some(2))  // Siempre exactamente 2 bytes
    }
}

// Tipo de tamaño variable con mínimo conocido
impl<'a> Arbitrary<'a> for Name {
    fn size_hint(_depth: usize) -> (usize, Option<usize>) {
        (2, Some(66))  // Min: 1 byte len + 1 byte char = 2
                        // Max: 1 byte len + 65 bytes chars = 66
    }
}

// Tipo de tamaño totalmente variable
impl<'a> Arbitrary<'a> for Document {
    fn size_hint(_depth: usize) -> (usize, Option<usize>) {
        (1, None)  // Al menos 1 byte, sin máximo conocido
    }
}
```

### Helpers para combinar size_hints

El crate `arbitrary` proporciona helpers en `arbitrary::size_hint`:

```rust
use arbitrary::size_hint;

impl<'a> Arbitrary<'a> for MyStruct {
    fn size_hint(depth: usize) -> (usize, Option<usize>) {
        // Para structs: sumar los hints de todos los campos
        size_hint::and_all(&[
            <u32 as Arbitrary>::size_hint(depth),
            <String as Arbitrary>::size_hint(depth),
            <bool as Arbitrary>::size_hint(depth),
        ])
    }
}

// Para enums: el hint es el de la variante más grande
fn size_hint(depth: usize) -> (usize, Option<usize>) {
    // 1 byte para el discriminante + el más grande de los campos
    size_hint::and(
        (1, Some(1)),  // discriminante
        size_hint::or_all(&[
            <i32 as Arbitrary>::size_hint(depth),          // Literal
            size_hint::and_all(&[                          // Add
                <Box<Expr> as Arbitrary>::size_hint(depth),
                <Box<Expr> as Arbitrary>::size_hint(depth),
            ]),
        ]),
    )
}
```

### ¿Es importante implementar size_hint?

```
Prioridad BAJA para la mayoría de usos.

El default (0, None) funciona. El fuzzer eventualmente genera
buffers del tamaño correcto.

Implementar size_hint preciso PUEDE acelerar el fuzzing porque
el fuzzer descarta menos inputs por NotEnoughData.

En la práctica, la diferencia es marginal para la mayoría de tipos.

Prioridad ALTA solo si:
- Tu tipo necesita muchos bytes (>100)
- El fuzzer genera demasiados Err(NotEnoughData)
- Quieres optimizar al máximo la velocidad
```

---

## 24. Shrinking: reducir inputs que causan crashes

### Concepto

Cuando el fuzzer encuentra un crash, el input original puede ser grande y complejo. **Shrinking** es el proceso de reducir el input a algo más simple que sigue causando el crash.

### Shrinking en cargo-fuzz

`cargo-fuzz` usa `cargo fuzz tmin` para minimizar, que trabaja a nivel de **bytes**, no de tipos. Esto significa que minimiza los bytes subyacentes, no la representación del tipo Arbitrary.

```bash
# Minimizar un crash
cargo fuzz tmin fuzz_target fuzz/artifacts/fuzz_target/crash-abc123
```

### Limitación: minimización de bytes vs minimización estructural

```
Crash original:
  Bytes: [0x02, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x05, 0x48, ...]
  → MapFuzzInput {
      commands: [
          Insert { key: 255, value: 1 },
          Get { key: 0 },
          Insert { key: 5, value: ... },
          Remove { key: ... },
          ...  (20 operaciones)
      ]
    }

Después de tmin (minimización de bytes):
  Bytes: [0x02, 0xFF, 0x01, 0x00, 0x00, 0x00, 0x05]
  → MapFuzzInput {
      commands: [
          Insert { key: 255, value: 1 },
          Get { key: 5 },
      ]
    }

La minimización eliminó los bytes que corresponden a operaciones
innecesarias para reproducir el crash.
```

### cargo fuzz fmt para entender el input minimizado

Después de minimizar, usa `cargo fuzz fmt` para ver el tipo, no los bytes:

```bash
cargo fuzz fmt fuzz_map fuzz/artifacts/fuzz_map/minimized-from-abc123

# Output:
# MapFuzzInput {
#     commands: [
#         Insert { key: 255, value: 1 },
#         Get { key: 5 },
#     ],
# }
```

Esto es mucho más legible que un hexdump.

---

## 25. Arbitrary vs proptest::Arbitrary

Rust tiene dos ecosistemas de generación de inputs:
- `arbitrary` (crate): usado por cargo-fuzz/libFuzzer
- `proptest` (crate): usado para property-based testing

### Diferencias fundamentales

| Aspecto | arbitrary (fuzzing) | proptest (property testing) |
|---|---|---|
| **Motor** | libFuzzer (coverage-guided) | Motor propio (random) |
| **Guía** | Cobertura de código | Random puro |
| **Shrinking** | A nivel de bytes (tmin) | Estructural (automático) |
| **Velocidad** | Miles/seg | Cientos/seg |
| **Integración** | cargo fuzz | #[test] normal |
| **Toolchain** | Nightly obligatorio | Stable |
| **Duración** | Minutos a horas | Segundos |
| **Determinismo** | No (coverage-guided) | Sí (seeded random) |
| **Trait** | `Arbitrary<'a>` | `proptest::arbitrary::Arbitrary` |
| **Derive** | `#[derive(Arbitrary)]` | `proptest_derive::Arbitrary` |

### Cuándo usar cada uno

```
┌────────────────────────────┬────────────────────────────────┐
│ Usar arbitrary (fuzzing)   │ Usar proptest                  │
├────────────────────────────┼────────────────────────────────┤
│ Buscar crashes y panics    │ Verificar propiedades          │
│ Encontrar bugs de seguridad│ Regression tests               │
│ Explorar edge cases        │ Tests deterministas en CI      │
│ Código unsafe/FFI          │ Property-based testing formal  │
│ Ejecución larga (horas)    │ Ejecución corta (segundos)     │
│ Nightly disponible         │ Solo stable disponible         │
└────────────────────────────┴────────────────────────────────┘
```

### ¿Se pueden usar juntos?

Sí. Puedes implementar ambos traits en el mismo tipo:

```rust
#[derive(Debug, Clone)]
#[cfg_attr(feature = "fuzzing", derive(arbitrary::Arbitrary))]
struct MyInput {
    x: u32,
    y: u32,
}

// Para proptest (en tests/):
#[cfg(test)]
mod tests {
    use proptest::prelude::*;

    proptest! {
        #[test]
        fn test_property(x in 0u32..1000, y in 0u32..1000) {
            let input = MyInput { x, y };
            prop_assert!(my_lib::process(&input).is_ok());
        }
    }
}
```

---

## 26. Arbitrary con cargo fuzz fmt

`cargo fuzz fmt` es especialmente útil con tipos Arbitrary porque muestra la **representación del tipo** en vez de los bytes crudos.

### Sin Arbitrary (bytes crudos)

```bash
$ cargo fuzz fmt fuzz_parse fuzz/artifacts/fuzz_parse/crash-abc123

# Output:
# [123, 34, 107, 101, 121, 34, 58, 32, 0, 93, 125]
#
# Interpretación: un array de bytes. ¿Qué significan?
# Necesitas decodificar manualmente: "{\"key\": \0]}"
```

### Con Arbitrary (tipo estructurado)

```bash
$ cargo fuzz fmt fuzz_config fuzz/artifacts/fuzz_config/crash-def456

# Output:
# ServerConfig {
#     port: Port(8080),
#     max_connections: 0,
#     timeout_secs: 3600,
#     name: NonEmptyString("test_server"),
#     features: [
#         Feature::Logging,
#         Feature::Compression,
#     ],
# }
#
# Interpretación: INMEDIATAMENTE sabes que max_connections = 0
# probablemente es el problema.
```

### La ventaja para debugging

```
Con &[u8]:
  1. Leer hexdump
  2. Decodificar manualmente
  3. Entender qué campo corresponde a qué bytes
  4. Tiempo: minutos

Con Arbitrary:
  1. cargo fuzz fmt → ver el struct
  2. Identificar el campo problemático
  3. Tiempo: segundos
```

---

## 27. Debugging de inputs Arbitrary

### Imprimir el input en el harness

```rust
fuzz_target!(|input: MyInput| {
    // En desarrollo, puedes imprimir para debugging:
    // (Quitar en producción — imprime millones de líneas)
    #[cfg(debug_assertions)]
    if std::env::var("FUZZ_DEBUG").is_ok() {
        eprintln!("Input: {:?}", input);
    }

    let _ = my_lib::process(&input);
});
```

Ejecutar con debug:

```bash
FUZZ_DEBUG=1 cargo fuzz run fuzz_target -- -max_total_time=5
```

### Crear un test unitario desde un crash

```rust
// Después de cargo fuzz fmt, copiar el output:
#[test]
fn regression_crash_def456() {
    let input = ServerConfig {
        port: Port(8080),
        max_connections: 0,
        timeout_secs: 3600,
        name: NonEmptyString("test_server".to_string()),
        features: vec![Feature::Logging, Feature::Compression],
    };

    // Debe retornar error, no paniquear
    let result = apply_config(&input);
    assert!(result.is_err());
}
```

### Verificar que Arbitrary genera inputs válidos

Si implementaste Arbitrary manualmente, puedes verificar la generación con un test:

```rust
#[test]
fn arbitrary_generates_valid_inputs() {
    use arbitrary::{Arbitrary, Unstructured};

    // Probar con muchos buffers aleatorios
    for seed in 0..10000u64 {
        let bytes: Vec<u8> = (0..200).map(|i| ((seed + i) % 256) as u8).collect();
        let mut u = Unstructured::new(&bytes);

        if let Ok(config) = ServerConfig::arbitrary(&mut u) {
            // Verificar invariantes
            assert!(config.port.0 >= 1, "Port is 0");
            assert!(config.port.0 <= 65535, "Port out of range");
            assert!(config.max_connections >= 1, "Max connections is 0");
            assert!(config.timeout_secs >= 1, "Timeout is 0");
            assert!(!config.name.0.is_empty(), "Name is empty");
        }
        // Err(NotEnoughData) es aceptable
    }
}
```

---

## 28. Patrones avanzados

### Patrón 1: Arbitrary condicional (feature-gated)

```rust
// Solo en el harness, no en el crate principal
// fuzz/fuzz_targets/fuzz_api.rs

#[derive(Arbitrary, Debug)]
struct ApiRequest {
    // Usa un tipo local que envuelve el tipo del crate
    endpoint: Endpoint,
    auth: Option<AuthToken>,
    body: RequestBody,
}

#[derive(Debug)]
struct AuthToken(String);

impl<'a> Arbitrary<'a> for AuthToken {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        // Generar tokens con formato correcto: "Bearer <hex>"
        let hex_len = u.int_in_range(16..=64usize)?;
        let hex_chars = b"0123456789abcdef";
        let mut token = String::from("Bearer ");
        for _ in 0..hex_len {
            let idx = u.int_in_range(0..=15)?;
            token.push(hex_chars[idx] as char);
        }
        Ok(AuthToken(token))
    }
}
```

### Patrón 2: generación de grafos (DAG)

```rust
#[derive(Debug)]
struct Dag {
    nodes: Vec<String>,
    edges: Vec<(usize, usize)>,  // (from, to), from < to para ser DAG
}

impl<'a> Arbitrary<'a> for Dag {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let num_nodes = u.int_in_range(1..=20usize)?;
        let mut nodes = Vec::with_capacity(num_nodes);
        for i in 0..num_nodes {
            nodes.push(format!("node_{}", i));
        }

        // Generar edges donde from < to (garantiza DAG)
        let max_edges = num_nodes * (num_nodes - 1) / 2;
        let num_edges = u.int_in_range(0..=max_edges.min(50))?;
        let mut edges = Vec::with_capacity(num_edges);

        for _ in 0..num_edges {
            let from = u.int_in_range(0..=(num_nodes.saturating_sub(2)))?;
            let to = u.int_in_range((from + 1)..=(num_nodes - 1))?;
            edges.push((from, to));
        }

        Ok(Dag { nodes, edges })
    }
}
```

### Patrón 3: input con checksum válido

```rust
#[derive(Debug)]
struct Packet {
    header: [u8; 4],
    payload: Vec<u8>,
    checksum: u32,
}

impl<'a> Arbitrary<'a> for Packet {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let header: [u8; 4] = u.arbitrary()?;
        let payload_len = u.int_in_range(0..=1024usize)?;
        let mut payload = Vec::with_capacity(payload_len);
        for _ in 0..payload_len {
            payload.push(u.arbitrary()?);
        }

        // Calcular checksum VÁLIDO
        let mut sum: u32 = 0;
        for &b in &header {
            sum = sum.wrapping_add(b as u32);
        }
        for &b in &payload {
            sum = sum.wrapping_add(b as u32);
        }
        let checksum = sum;

        Ok(Packet { header, payload, checksum })
    }
}

// El fuzzer genera packets con checksum correcto
// → pasan la validación inicial
// → ejercitan la lógica de procesamiento real
```

### Patrón 4: múltiples tipos de input (dispatch)

```rust
#[derive(Arbitrary, Debug)]
enum FuzzInput {
    // El fuzzer elige automáticamente qué tipo de input generar
    RawBytes(Vec<u8>),
    Utf8String(String),
    Structured(MyStruct),
    Sequence(Vec<MyOp>),
}

fuzz_target!(|input: FuzzInput| {
    match input {
        FuzzInput::RawBytes(data) => {
            let _ = my_lib::parse_binary(&data);
        }
        FuzzInput::Utf8String(s) => {
            let _ = my_lib::parse_text(&s);
        }
        FuzzInput::Structured(s) => {
            let _ = my_lib::process(&s);
        }
        FuzzInput::Sequence(ops) => {
            let mut state = my_lib::State::new();
            for op in &ops {
                state.apply(op);
            }
        }
    }
});
```

---

## 29. Anti-patrones con Arbitrary

### Anti-patrón 1: ignorar el Error de Arbitrary

```rust
// ❌ MAL: unwrap en el closure
fuzz_target!(|data: &[u8]| {
    let mut u = Unstructured::new(data);
    let input: MyInput = MyInput::arbitrary(&mut u).unwrap();  // panic innecesario
    // ...
});

// ✓ BIEN: dejar que fuzz_target! maneje la conversión
fuzz_target!(|input: MyInput| {
    // ...
});

// ✓ TAMBIÉN BIEN: si necesitas Unstructured manualmente
fuzz_target!(|data: &[u8]| {
    let mut u = Unstructured::new(data);
    let input = match MyInput::arbitrary(&mut u) {
        Ok(v) => v,
        Err(_) => return,
    };
    // ...
});
```

### Anti-patrón 2: derive con tipos enormes

```rust
// ❌ MAL: derive genera Vecs de tamaño ilimitado
#[derive(Arbitrary, Debug)]
struct BadInput {
    data: Vec<Vec<Vec<u8>>>,  // Vec<Vec<Vec<>>> → potencialmente enorme
    matrix: Vec<Vec<f64>>,     // Puede ser 1000x1000
    text: String,              // Puede ser megabytes
}

// ✓ BIEN: acotar los tamaños
#[derive(Debug)]
struct GoodInput {
    data: Vec<Vec<u8>>,        // Acotado manualmente
    matrix: Vec<Vec<f64>>,
    text: String,
}

impl<'a> Arbitrary<'a> for GoodInput {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        // Acotar dimensiones
        let outer_len = u.int_in_range(0..=10usize)?;
        let mut data = Vec::new();
        for _ in 0..outer_len {
            let inner_len = u.int_in_range(0..=20usize)?;
            let inner: Vec<u8> = (0..inner_len).map(|_| u.arbitrary()).collect::<Result<_, _>>()?;
            data.push(inner);
        }

        let rows = u.int_in_range(1..=10usize)?;
        let cols = u.int_in_range(1..=10usize)?;
        let mut matrix = Vec::new();
        for _ in 0..rows {
            let row: Vec<f64> = (0..cols).map(|_| u.arbitrary()).collect::<Result<_, _>>()?;
            matrix.push(row);
        }

        let text_len = u.int_in_range(0..=256usize)?;
        let text_bytes = u.bytes(text_len)?;
        let text = String::from_utf8_lossy(text_bytes).to_string();

        Ok(GoodInput { data, matrix, text })
    }
}
```

### Anti-patrón 3: post-filtering excesivo

```rust
// ❌ MAL: generar todo y descartar el 99%
fuzz_target!(|config: Config| {
    if config.port == 0 { return; }           // descartar
    if config.name.is_empty() { return; }     // descartar
    if config.name.len() > 100 { return; }    // descartar
    if config.timeout == 0 { return; }        // descartar
    if !config.name.is_ascii() { return; }    // descartar
    // Solo el 1% de los inputs llega aquí

    let _ = apply_config(&config);
});

// ✓ BIEN: generar solo valores válidos con Arbitrary manual
// (ver sección 15)
```

### Anti-patrón 4: estado global en Arbitrary

```rust
// ❌ MAL: usar estado global en la generación
static COUNTER: AtomicU32 = AtomicU32::new(0);

impl<'a> Arbitrary<'a> for MyInput {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let id = COUNTER.fetch_add(1, Ordering::SeqCst);  // No determinista
        Ok(MyInput { id, data: u.arbitrary()? })
    }
}

// ✓ BIEN: todo debe depender solo de los bytes de Unstructured
impl<'a> Arbitrary<'a> for MyInput {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        Ok(MyInput {
            id: u.arbitrary()?,   // determinista
            data: u.arbitrary()?,
        })
    }
}
```

### Anti-patrón 5: Arbitrary que panicea

```rust
// ❌ MAL: panic en la implementación de Arbitrary
impl<'a> Arbitrary<'a> for MyType {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let n: u32 = u.arbitrary()?;
        let items: Vec<u8> = (0..n).map(|_| u.arbitrary().unwrap()).collect();
        //                                                  ^^^^^^
        //                        Si no hay bytes suficientes → panic
        //                        El fuzzer reporta crash en Arbitrary, no en tu código
        Ok(MyType { items })
    }
}

// ✓ BIEN: propagar errores
impl<'a> Arbitrary<'a> for MyType {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let n: u32 = u.int_in_range(0..=100)?;
        let items: Vec<u8> = (0..n).map(|_| u.arbitrary()).collect::<Result<_, _>>()?;
        Ok(MyType { items })
    }
}
```

---

## 30. Comparación C vs Rust vs Go

### Generación de inputs estructurados

| Aspecto | C (FuzzedDataProvider) | Rust (Arbitrary) | Go (go-fuzz-headers) |
|---|---|---|---|
| **Mecanismo** | Struct manual con funciones consume_* | Trait con derive automático | Funciones sobre ConsumeFuzzer |
| **Derivación automática** | No existe | `#[derive(Arbitrary)]` | No existe |
| **Tipos primitivos** | Manual: `consume_uint32()` | Automático: `u32::arbitrary()` | Manual: `cf.GetUint32()` |
| **Strings** | Manual: `consume_string()` | Automático: `String::arbitrary()` | Manual: `cf.GetString()` |
| **Structs** | Manual: campo por campo | Automático si todos los campos impl Arbitrary | Manual: `cf.GenerateStruct(&s)` (reflection) |
| **Enums** | `consume_uint8() % N` | Automático: 1 byte discriminante | `cf.GetInt() % N` |
| **Option/Maybe** | Manual: `if consume_bool()` | Automático: `Option<T>::arbitrary()` | Manual |
| **Colecciones** | Manual: len + loop | Automático: `Vec<T>::arbitrary()` | Manual: len + loop |
| **Restricciones** | Manual en el harness | Manual en impl Arbitrary | Manual en el harness |
| **Recursión** | Manual con depth counter | Manual con depth counter | Manual |
| **Seguridad de tipos** | Nula (todo es bytes) | Total (type system) | Parcial (reflection) |
| **Composabilidad** | Baja (funciones sueltas) | Alta (trait system) | Baja |
| **Shrinking** | tmin a nivel de bytes | tmin a nivel de bytes | — |
| **Visualización** | hexdump manual | `cargo fuzz fmt` → Debug | — |

### Ejemplo equivalente en los 3 lenguajes

**C (FuzzedDataProvider manual):**
```c
// Definir la función de consumo para cada campo
int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    FuzzInput fi;
    fuzz_input_init(&fi, data, size);

    uint16_t port = fuzz_consume_uint16(&fi);
    uint32_t max_conn = fuzz_consume_uint32(&fi);
    uint8_t use_tls = fuzz_consume_bool(&fi);

    size_t name_len;
    const uint8_t *name_data = fuzz_consume_bytes(&fi, 32, &name_len);
    // Null terminar, validar UTF-8, etc. — todo manual

    Config config = { .port = port, .max_conn = max_conn, ... };
    apply_config(&config);
    return 0;
}
```

**Rust (Arbitrary derive):**
```rust
#[derive(Arbitrary, Debug)]
struct Config {
    port: u16,
    max_conn: u32,
    use_tls: bool,
    name: String,
}

fuzz_target!(|config: Config| {
    let _ = apply_config(&config);
});
// ← 7 líneas totales (incluyendo use y derive)
```

**Go (go test -fuzz):**
```go
func FuzzApplyConfig(f *testing.F) {
    f.Fuzz(func(t *testing.T, port uint16, maxConn uint32, useTls bool, name string) {
        config := Config{
            Port:    port,
            MaxConn: maxConn,
            UseTLS:  useTls,
            Name:    name,
        }
        applyConfig(&config)
    })
}
// Go tiene soporte nativo para múltiples parámetros tipados
// Pero no soporta structs directamente como input
```

---

## 31. Errores comunes

### Error 1: olvidar features = ["derive"]

```toml
# ❌ MAL:
[dependencies]
arbitrary = "1"

# ✓ BIEN:
[dependencies]
arbitrary = { version = "1", features = ["derive"] }
```

Sin `features = ["derive"]`, el macro procedural `#[derive(Arbitrary)]` no está disponible:

```
error: cannot find derive macro `Arbitrary` in this scope
 --> src/lib.rs:3:10
  |
3 | #[derive(Arbitrary, Debug)]
  |          ^^^^^^^^^
```

### Error 2: no derivar Debug

```rust
// ❌ MAL: Arbitrary sin Debug
#[derive(Arbitrary)]
struct MyInput {
    x: u32,
}

// fuzz_target! necesita Debug para imprimir crashes:
fuzz_target!(|input: MyInput| { ... });
// error: `MyInput` doesn't implement `Debug`
```

### Error 3: campos que no implementan Arbitrary

```rust
// ❌ MAL:
#[derive(Arbitrary, Debug)]
struct MyInput {
    name: String,
    file: std::fs::File,  // File no implementa Arbitrary
}

// ✓ Solución 1: usar un tipo que sí implementa Arbitrary
#[derive(Arbitrary, Debug)]
struct MyInput {
    name: String,
    file_contents: Vec<u8>,  // Representar el contenido, no el File
}

// ✓ Solución 2: implementar Arbitrary manualmente
// (ver sección 13)
```

### Error 4: tipos recursivos con derive sin bounds

```rust
// ❌ PELIGROSO: derive en tipo recursivo sin protección de profundidad
#[derive(Arbitrary, Debug)]
enum Tree {
    Leaf(i32),
    Node(Box<Tree>, Box<Tree>),  // Recursivo — puede generar árboles enormes
}

// ✓ BIEN: implementar manualmente con control de profundidad
// (ver sección 19)
```

### Error 5: Arbitrary con side effects

```rust
// ❌ MAL: Arbitrary que hace I/O
impl<'a> Arbitrary<'a> for MyInput {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let path: String = u.arbitrary()?;
        let contents = std::fs::read_to_string(&path)?;  // ❌ I/O en Arbitrary
        Ok(MyInput { path, contents })
    }
}

// ✓ BIEN: Arbitrary solo lee de Unstructured
impl<'a> Arbitrary<'a> for MyInput {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, Error> {
        let path: String = u.arbitrary()?;
        let contents: String = u.arbitrary()?;
        Ok(MyInput { path, contents })
    }
}
```

### Error 6: confundir arbitrary::Error con otros errores

```rust
// ❌ MAL: usar ? con errores incompatibles
impl<'a> Arbitrary<'a> for ValidatedName {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, arbitrary::Error> {
        let name: String = u.arbitrary()?;
        let validated = validate_name(&name)?;  // ❌ validate retorna otro Error
        Ok(ValidatedName(validated))
    }
}

// ✓ BIEN: mapear errores explícitamente
impl<'a> Arbitrary<'a> for ValidatedName {
    fn arbitrary(u: &mut Unstructured<'a>) -> Result<Self, arbitrary::Error> {
        let name: String = u.arbitrary()?;
        match validate_name(&name) {
            Ok(v) => Ok(ValidatedName(v)),
            Err(_) => {
                // Si la validación falla, generar un nombre que sí pase
                Ok(ValidatedName("default".to_string()))
            }
        }
    }
}
```

### Error 7: desajuste de versiones entre arbitrary y libfuzzer-sys

```toml
# ❌ MAL: versiones incompatibles
[dependencies]
arbitrary = "0.4"        # Versión vieja
libfuzzer-sys = "0.11"   # Versión nueva que espera arbitrary 1.x

# ✓ BIEN: versiones compatibles
[dependencies]
arbitrary = { version = "1", features = ["derive"] }
libfuzzer-sys = "0.11"
```

---

## 32. Programa de práctica: sistema de tickets

### Objetivo

Implementar un sistema simple de tickets (issue tracker) y fuzzearlo usando Arbitrary para generar secuencias de operaciones.

### Especificación

```
ticket_system/
├── Cargo.toml
├── src/
│   └── lib.rs                 ← sistema de tickets
├── tests/
│   └── unit_tests.rs          ← tests unitarios
└── fuzz/
    ├── Cargo.toml
    ├── fuzz_targets/
    │   ├── fuzz_ops.rs        ← harness 1: operaciones sin verificación
    │   ├── fuzz_invariants.rs ← harness 2: operaciones + invariantes
    │   └── fuzz_oracle.rs     ← harness 3: comparación con Vec<Ticket>
    └── corpus/
        └── fuzz_ops/
```

### src/lib.rs

```rust
use std::collections::HashMap;

#[derive(Debug, Clone, PartialEq)]
pub enum Priority {
    Low,
    Medium,
    High,
    Critical,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Status {
    Open,
    InProgress,
    Resolved,
    Closed,
}

#[derive(Debug, Clone)]
pub struct Ticket {
    pub id: u32,
    pub title: String,
    pub priority: Priority,
    pub status: Status,
    pub assignee: Option<String>,
}

pub struct TicketSystem {
    tickets: HashMap<u32, Ticket>,
    next_id: u32,
    // BUG 1: counter de tickets abiertos que no siempre
    // se actualiza correctamente
    open_count: u32,
}

impl TicketSystem {
    pub fn new() -> Self {
        TicketSystem {
            tickets: HashMap::new(),
            next_id: 1,
            open_count: 0,
        }
    }

    /// Crear un ticket. Retorna el ID asignado.
    pub fn create(&mut self, title: String, priority: Priority) -> u32 {
        let id = self.next_id;
        self.next_id += 1;

        let ticket = Ticket {
            id,
            title,
            priority,
            status: Status::Open,
            assignee: None,
        };

        self.tickets.insert(id, ticket);
        self.open_count += 1;
        id
    }

    /// Asignar un ticket a alguien
    pub fn assign(&mut self, id: u32, assignee: String) -> Result<(), String> {
        let ticket = self.tickets.get_mut(&id)
            .ok_or_else(|| format!("Ticket {} not found", id))?;

        if ticket.status == Status::Closed {
            return Err("Cannot assign a closed ticket".to_string());
        }

        ticket.assignee = Some(assignee);

        // BUG 2: cambiar status a InProgress al asignar,
        // pero no verificar si ya era InProgress o Resolved
        // Esto permite la transición Resolved → InProgress
        // que podría violar la máquina de estados
        ticket.status = Status::InProgress;

        Ok(())
    }

    /// Resolver un ticket
    pub fn resolve(&mut self, id: u32) -> Result<(), String> {
        let ticket = self.tickets.get_mut(&id)
            .ok_or_else(|| format!("Ticket {} not found", id))?;

        match ticket.status {
            Status::Open | Status::InProgress => {
                ticket.status = Status::Resolved;
                // BUG 3: decrementa open_count al resolver,
                // pero si el ticket ya era InProgress (asignado),
                // open_count ya fue decrementado... ¡o no!
                // En realidad open_count solo se incrementa en create
                // y se decrementa aquí y en close, así que un ticket
                // que se resuelve Y luego se cierra decrementa 2 veces
                self.open_count = self.open_count.wrapping_sub(1);
                Ok(())
            }
            Status::Resolved => Err("Already resolved".to_string()),
            Status::Closed => Err("Cannot resolve a closed ticket".to_string()),
        }
    }

    /// Cerrar un ticket
    pub fn close(&mut self, id: u32) -> Result<(), String> {
        let ticket = self.tickets.get_mut(&id)
            .ok_or_else(|| format!("Ticket {} not found", id))?;

        // BUG 4: permite cerrar desde cualquier estado excepto Closed
        // Debería requerir Resolved primero
        if ticket.status == Status::Closed {
            return Err("Already closed".to_string());
        }

        ticket.status = Status::Closed;
        self.open_count = self.open_count.wrapping_sub(1);
        Ok(())
    }

    /// Reabrir un ticket cerrado
    pub fn reopen(&mut self, id: u32) -> Result<(), String> {
        let ticket = self.tickets.get_mut(&id)
            .ok_or_else(|| format!("Ticket {} not found", id))?;

        if ticket.status != Status::Closed {
            return Err("Can only reopen closed tickets".to_string());
        }

        ticket.status = Status::Open;
        ticket.assignee = None;
        self.open_count += 1;
        Ok(())
    }

    /// Obtener un ticket
    pub fn get(&self, id: u32) -> Option<&Ticket> {
        self.tickets.get(&id)
    }

    /// Contar tickets por status
    pub fn count_by_status(&self, status: &Status) -> usize {
        self.tickets.values()
            .filter(|t| t.status == *status)
            .count()
    }

    /// Contar tickets abiertos (usa el counter interno)
    pub fn open_count(&self) -> u32 {
        self.open_count
    }

    /// Total de tickets
    pub fn total(&self) -> usize {
        self.tickets.len()
    }

    /// Listar tickets por prioridad
    pub fn list_by_priority(&self, priority: &Priority) -> Vec<&Ticket> {
        self.tickets.values()
            .filter(|t| t.priority == *priority)
            .collect()
    }

    /// Buscar tickets por título (substring)
    pub fn search(&self, query: &str) -> Vec<&Ticket> {
        if query.is_empty() {
            return Vec::new();
        }
        self.tickets.values()
            .filter(|t| t.title.contains(query))
            .collect()
    }
}
```

### Bugs intencionales resumidos

```
Bug 1: open_count no refleja el conteo real de tickets Open
       porque cuenta Open + InProgress + Resolved mezclados

Bug 2: assign() siempre pone InProgress, incluso desde Resolved
       → permite transición Resolved → InProgress (estado inválido)

Bug 3: resolve() decrementa open_count, pero close() TAMBIÉN
       decrementa → un ticket que se resuelve y cierra resta 2
       → open_count puede ser negativo (wrapping_sub → underflow)

Bug 4: close() permite cerrar desde Open o InProgress sin
       resolver primero → violación de la máquina de estados
```

### Tipos Arbitrary para las operaciones

```rust
// fuzz/fuzz_targets/common.rs (o inline en cada harness)
use arbitrary::Arbitrary;

#[derive(Arbitrary, Debug, Clone)]
pub enum TicketPriority {
    Low,
    Medium,
    High,
    Critical,
}

impl From<TicketPriority> for ticket_system::Priority {
    fn from(p: TicketPriority) -> Self {
        match p {
            TicketPriority::Low => ticket_system::Priority::Low,
            TicketPriority::Medium => ticket_system::Priority::Medium,
            TicketPriority::High => ticket_system::Priority::High,
            TicketPriority::Critical => ticket_system::Priority::Critical,
        }
    }
}

#[derive(Debug)]
pub struct TicketTitle(pub String);

impl<'a> Arbitrary<'a> for TicketTitle {
    fn arbitrary(u: &mut arbitrary::Unstructured<'a>) -> Result<Self, arbitrary::Error> {
        let len = u.int_in_range(1..=50usize)?;
        let charset = b"abcdefghijklmnopqrstuvwxyz ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_:.";
        let mut s = String::with_capacity(len);
        for _ in 0..len {
            let idx = u.int_in_range(0..=(charset.len() - 1))?;
            s.push(charset[idx] as char);
        }
        Ok(TicketTitle(s))
    }
}

#[derive(Debug)]
pub struct AssigneeName(pub String);

impl<'a> Arbitrary<'a> for AssigneeName {
    fn arbitrary(u: &mut arbitrary::Unstructured<'a>) -> Result<Self, arbitrary::Error> {
        let names = &["alice", "bob", "carol", "dave", "eve"];
        let name = u.choose(names)?;
        Ok(AssigneeName(name.to_string()))
    }
}

#[derive(Arbitrary, Debug)]
pub enum TicketOp {
    Create {
        title: TicketTitle,
        priority: TicketPriority,
    },
    Assign {
        ticket_idx: u8,     // Índice en la lista de tickets creados
        assignee: AssigneeName,
    },
    Resolve {
        ticket_idx: u8,
    },
    Close {
        ticket_idx: u8,
    },
    Reopen {
        ticket_idx: u8,
    },
    Get {
        ticket_idx: u8,
    },
    Search {
        query: TicketTitle,
    },
    CountByPriority {
        priority: TicketPriority,
    },
    CheckOpenCount,
    CheckTotal,
}

#[derive(Arbitrary, Debug)]
pub struct FuzzInput {
    pub ops: Vec<TicketOp>,
}
```

### Harness 1: fuzz_ops.rs (solo no-crash)

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use arbitrary::Arbitrary;

// ... (tipos de arriba)

fuzz_target!(|input: FuzzInput| {
    if input.ops.len() > 200 {
        return;
    }

    let mut system = ticket_system::TicketSystem::new();
    let mut created_ids: Vec<u32> = Vec::new();

    for op in &input.ops {
        match op {
            TicketOp::Create { title, priority } => {
                let id = system.create(title.0.clone(), priority.clone().into());
                created_ids.push(id);
            }
            TicketOp::Assign { ticket_idx, assignee } => {
                if let Some(&id) = created_ids.get(*ticket_idx as usize % created_ids.len().max(1)) {
                    let _ = system.assign(id, assignee.0.clone());
                }
            }
            TicketOp::Resolve { ticket_idx } => {
                if let Some(&id) = created_ids.get(*ticket_idx as usize % created_ids.len().max(1)) {
                    let _ = system.resolve(id);
                }
            }
            TicketOp::Close { ticket_idx } => {
                if let Some(&id) = created_ids.get(*ticket_idx as usize % created_ids.len().max(1)) {
                    let _ = system.close(id);
                }
            }
            TicketOp::Reopen { ticket_idx } => {
                if let Some(&id) = created_ids.get(*ticket_idx as usize % created_ids.len().max(1)) {
                    let _ = system.reopen(id);
                }
            }
            TicketOp::Get { ticket_idx } => {
                if let Some(&id) = created_ids.get(*ticket_idx as usize % created_ids.len().max(1)) {
                    let _ = system.get(id);
                }
            }
            TicketOp::Search { query } => {
                let _ = system.search(&query.0);
            }
            TicketOp::CountByPriority { priority } => {
                let _ = system.count_by_status(&ticket_system::Status::Open);
            }
            TicketOp::CheckOpenCount => {
                let _ = system.open_count();
            }
            TicketOp::CheckTotal => {
                let _ = system.total();
            }
        }
    }
});
```

### Harness 2: fuzz_invariants.rs (con verificación de invariantes)

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use arbitrary::Arbitrary;

// ... (tipos de arriba)

fuzz_target!(|input: FuzzInput| {
    if input.ops.len() > 200 {
        return;
    }

    let mut system = ticket_system::TicketSystem::new();
    let mut created_ids: Vec<u32> = Vec::new();

    for op in &input.ops {
        // ... (ejecutar operación como en harness 1)
        execute_op(&mut system, &mut created_ids, op);

        // === INVARIANTES ===

        // Invariante 1: open_count() debe coincidir con el conteo real
        let real_open = system.count_by_status(&ticket_system::Status::Open);
        let real_in_progress = system.count_by_status(&ticket_system::Status::InProgress);
        // ¿open_count debería contar solo Open, o Open + InProgress?
        // Si solo Open:
        // assert_eq!(system.open_count() as usize, real_open,
        //     "open_count mismatch: counter={}, real={}",
        //     system.open_count(), real_open);

        // Invariante 2: total debe ser el número de tickets creados
        // (nunca se borran)
        assert_eq!(system.total(), created_ids.len(),
            "total mismatch");

        // Invariante 3: open_count no debe ser mayor que total
        assert!(
            (system.open_count() as usize) <= system.total(),
            "open_count ({}) > total ({})",
            system.open_count(), system.total()
        );

        // Invariante 4: todos los tickets tienen un status válido
        for &id in &created_ids {
            let ticket = system.get(id).expect("Created ticket should exist");
            match ticket.status {
                ticket_system::Status::Open |
                ticket_system::Status::InProgress |
                ticket_system::Status::Resolved |
                ticket_system::Status::Closed => {}
                // Si hubiera un estado no reconocido → panic
            }
        }

        // Invariante 5: un ticket sin asignar debería ser Open o Closed
        // (no InProgress)
        for &id in &created_ids {
            let ticket = system.get(id).unwrap();
            if ticket.assignee.is_none() {
                assert!(
                    ticket.status == ticket_system::Status::Open ||
                    ticket.status == ticket_system::Status::Closed,
                    "Ticket {} has no assignee but status is {:?}",
                    id, ticket.status
                );
            }
        }
    }
});

fn execute_op(
    system: &mut ticket_system::TicketSystem,
    created_ids: &mut Vec<u32>,
    op: &TicketOp,
) {
    match op {
        TicketOp::Create { title, priority } => {
            let id = system.create(title.0.clone(), priority.clone().into());
            created_ids.push(id);
        }
        TicketOp::Assign { ticket_idx, assignee } => {
            if !created_ids.is_empty() {
                let id = created_ids[*ticket_idx as usize % created_ids.len()];
                let _ = system.assign(id, assignee.0.clone());
            }
        }
        TicketOp::Resolve { ticket_idx } => {
            if !created_ids.is_empty() {
                let id = created_ids[*ticket_idx as usize % created_ids.len()];
                let _ = system.resolve(id);
            }
        }
        TicketOp::Close { ticket_idx } => {
            if !created_ids.is_empty() {
                let id = created_ids[*ticket_idx as usize % created_ids.len()];
                let _ = system.close(id);
            }
        }
        TicketOp::Reopen { ticket_idx } => {
            if !created_ids.is_empty() {
                let id = created_ids[*ticket_idx as usize % created_ids.len()];
                let _ = system.reopen(id);
            }
        }
        TicketOp::Get { ticket_idx } => {
            if !created_ids.is_empty() {
                let id = created_ids[*ticket_idx as usize % created_ids.len()];
                let _ = system.get(id);
            }
        }
        TicketOp::Search { query } => {
            let _ = system.search(&query.0);
        }
        TicketOp::CountByPriority { .. } | TicketOp::CheckOpenCount | TicketOp::CheckTotal => {}
    }
}
```

### Harness 3: fuzz_oracle.rs (comparación diferencial)

```rust
#![no_main]
use libfuzzer_sys::fuzz_target;
use arbitrary::Arbitrary;

// ... (tipos de arriba)

/// Oracle: implementación simple basada en Vec
struct OracleSystem {
    tickets: Vec<OracleTicket>,
}

struct OracleTicket {
    id: u32,
    title: String,
    status: String,
    assignee: Option<String>,
}

impl OracleSystem {
    fn new() -> Self { OracleSystem { tickets: Vec::new() } }

    fn create(&mut self, id: u32, title: String) {
        self.tickets.push(OracleTicket {
            id, title, status: "Open".to_string(), assignee: None,
        });
    }

    fn get_status(&self, id: u32) -> Option<&str> {
        self.tickets.iter()
            .find(|t| t.id == id)
            .map(|t| t.status.as_str())
    }

    fn total(&self) -> usize { self.tickets.len() }
}

fuzz_target!(|input: FuzzInput| {
    if input.ops.len() > 200 { return; }

    let mut system = ticket_system::TicketSystem::new();
    let mut oracle = OracleSystem::new();
    let mut created_ids: Vec<u32> = Vec::new();

    for op in &input.ops {
        match op {
            TicketOp::Create { title, priority } => {
                let id = system.create(title.0.clone(), priority.clone().into());
                oracle.create(id, title.0.clone());
                created_ids.push(id);
            }
            TicketOp::Get { ticket_idx } => {
                if !created_ids.is_empty() {
                    let id = created_ids[*ticket_idx as usize % created_ids.len()];
                    let sys_ticket = system.get(id);
                    let oracle_status = oracle.get_status(id);
                    // Ambos deben existir o no existir
                    assert_eq!(sys_ticket.is_some(), oracle_status.is_some());
                }
            }
            TicketOp::CheckTotal => {
                assert_eq!(system.total(), oracle.total(),
                    "Total mismatch: system={}, oracle={}",
                    system.total(), oracle.total());
            }
            _ => {
                // Ejecutar en system, no en oracle (oracle es parcial)
                execute_op(&mut system, &mut created_ids, op);
            }
        }
    }
});
```

### Tareas

1. **Crea el proyecto** `cargo new --lib ticket_system` e implementa `src/lib.rs`

2. **Agrega el feature gate** para Arbitrary en `Cargo.toml`:
   ```toml
   [dependencies]
   arbitrary = { version = "1", optional = true, features = ["derive"] }
   [features]
   fuzzing = ["arbitrary"]
   ```

3. **Inicializa fuzzing**: `cargo fuzz init -t fuzz_ops`

4. **Implementa los tipos Arbitrary** (TicketOp, TicketTitle, AssigneeName, etc.)

5. **Implementa los 3 harnesses**

6. **Fuzzea `fuzz_ops`** por 5 minutos. ¿Encuentra panics?

7. **Fuzzea `fuzz_invariants`** por 10 minutos.
   - ¿Qué invariante se rompe primero?
   - ¿Qué secuencia de operaciones lo causa?
   - Usa `cargo fuzz fmt` para ver la secuencia

8. **Minimiza** cada crash encontrado

9. **Corrige** cada bug y crea regression tests

10. **Compara**: ¿el harness `fuzz_ops` (sin invariantes) puede encontrar los bugs 1-4? ¿Por qué sí o por qué no?

---

## 33. Ejercicios

### Ejercicio 1: Arbitrary manual para un tipo con invariantes

**Objetivo**: Implementar Arbitrary para un tipo donde derive no es suficiente.

**Tareas**:

**a)** Implementa un tipo `DateRange`:
   ```rust
   pub struct DateRange {
       pub year: u16,    // 2000-2100
       pub month: u8,    // 1-12
       pub day: u8,      // 1-28/30/31 dependiendo del mes
       pub duration_days: u16,  // 1-365
   }
   ```

**b)** Implementa `Arbitrary` manualmente asegurando que:
   - `year` está entre 2000 y 2100
   - `month` está entre 1 y 12
   - `day` es válido para el mes (considera febrero = 28, sin bisiestos)
   - `duration_days` está entre 1 y 365

**c)** Escribe un test que verifique que `arbitrary()` siempre genera `DateRange` válidos (ejecutar 10000 veces con bytes aleatorios).

**d)** Escribe un harness que fuzzee una función `overlaps(a: &DateRange, b: &DateRange) -> bool` con un par de `DateRange`.

---

### Ejercicio 2: patrón command para una calculadora

**Objetivo**: Fuzzear una calculadora con stack usando secuencias de operaciones.

**Tareas**:

**a)** Implementa una calculadora RPN (Reverse Polish Notation):
   ```rust
   pub struct RpnCalc {
       stack: Vec<f64>,
   }

   impl RpnCalc {
       pub fn new() -> Self;
       pub fn push(&mut self, value: f64);
       pub fn add(&mut self) -> Result<f64, CalcError>;
       pub fn sub(&mut self) -> Result<f64, CalcError>;
       pub fn mul(&mut self) -> Result<f64, CalcError>;
       pub fn div(&mut self) -> Result<f64, CalcError>;
       pub fn dup(&mut self) -> Result<(), CalcError>;
       pub fn swap(&mut self) -> Result<(), CalcError>;
       pub fn peek(&self) -> Option<f64>;
       pub fn depth(&self) -> usize;
       pub fn clear(&mut self);
   }
   ```
   Introduce un bug: `swap` no funciona correctamente cuando hay exactamente 2 elementos en el stack y ambos son iguales (no cambia nada, lo cual es correcto, pero un invariante interno se corrompe).

**b)** Define las operaciones como un enum Arbitrary:
   ```rust
   #[derive(Arbitrary, Debug)]
   enum CalcOp {
       Push(f64),
       Add, Sub, Mul, Div,
       Dup, Swap, Clear,
   }
   ```

**c)** Escribe un harness con invariantes:
   - Después de `push`, `depth` debe incrementar en 1
   - Después de `add`/`sub`/`mul`/`div` exitosos, `depth` debe decrementar en 1
   - Después de `dup` exitoso, `depth` debe incrementar en 1
   - `swap` no cambia `depth`
   - `clear` hace `depth == 0`
   - `peek` nunca panicea

**d)** ¿El fuzzer encuentra el bug de `swap`? ¿Qué secuencia de operaciones lo activa?

---

### Ejercicio 3: Arbitrary con restricciones interdependientes

**Objetivo**: Generar inputs donde los campos dependen entre sí.

**Tareas**:

**a)** Define un tipo `Matrix`:
   ```rust
   pub struct Matrix {
       pub rows: usize,     // 1-10
       pub cols: usize,     // 1-10
       pub data: Vec<f64>,  // exactamente rows * cols elementos
   }
   ```

**b)** Implementa `Arbitrary` manualmente:
   - `rows` y `cols` acotados a 1-10
   - `data` tiene exactamente `rows * cols` elementos
   - Sin NaN ni Infinity en los datos (generar solo finitos)

**c)** Fuzzea una función `transpose(m: &Matrix) -> Matrix` con un harness roundtrip:
   ```rust
   // transpose(transpose(m)) == m
   ```

**d)** Fuzzea una función `multiply(a: &Matrix, b: &Matrix) -> Option<Matrix>` donde `a.cols == b.rows` debe ser true para producir `Some`. Implementa un `MatrixPair` Arbitrary que garantice dimensiones compatibles.

---

### Ejercicio 4: comparación con bytes crudos

**Objetivo**: Medir la diferencia de efectividad entre `&[u8]` y Arbitrary.

**Tareas**:

**a)** Implementa una función que recibe un struct:
   ```rust
   pub struct QueryParams {
       pub table: String,
       pub limit: u32,
       pub offset: u32,
       pub ascending: bool,
   }

   pub fn build_query(params: &QueryParams) -> Result<String, QueryError> {
       // Validar: table no vacío, limit > 0, limit <= 1000
       // Generar: "SELECT * FROM {table} LIMIT {limit} OFFSET {offset} ORDER BY id {ASC|DESC}"
       // Bug: si table contiene un espacio, genera SQL inválido
   }
   ```

**b)** Escribe DOS harnesses:
   - `fuzz_bytes`: con `&[u8]`, parsear manualmente a `QueryParams`
   - `fuzz_arbitrary`: con `derive(Arbitrary)` directo

**c)** Fuzzea cada uno 5 minutos con las mismas condiciones.

**d)** Compara:
   - exec/s de cada harness
   - Cobertura alcanzada
   - ¿Cuál encontró el bug? ¿Cuánto tardó cada uno?
   - ¿Cuántos inputs descartó el harness de bytes crudos?

---

## Navegación

- **Anterior**: [T01 - cargo-fuzz](../T01_Cargo_fuzz/README.md)
- **Siguiente**: [T03 - Fuzz + sanitizers](../T03_Fuzz_sanitizers/README.md)

---

> **Sección 2: Fuzzing en Rust** — Tópico 2 de 4 completado
