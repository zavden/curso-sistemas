# T02 — Adapter en Rust

---

## 1. De C a Rust: traits como interfaz

T01 mostro tres formas de adapter en C:
- Wrapper functions (traduccion directa)
- Opaque struct intermedio (encapsula backend)
- Vtable con function pointers (intercambio en runtime)

En Rust, el sistema de traits reemplaza las tres:

```
  C                              Rust
  ──                             ────
  Wrapper function               fn adapter(x: A) -> B
  Opaque struct + vtable         impl Trait for ConcreteType
  Function pointer table         dyn Trait (dynamic dispatch)
  Diferentes .c, mismo .h        Diferentes impl, mismo trait
```

---

## 2. Adapter basico: impl Trait para un tipo propio

```rust
/// Interfaz que nuestro codigo espera
trait Logger {
    fn log(&self, level: LogLevel, msg: &str);
}

#[derive(Debug, Clone, Copy, PartialEq, PartialOrd)]
enum LogLevel { Debug, Info, Warn, Error }

/// "Libreria" existente con API diferente
struct SyslogClient {
    facility: String,
}

impl SyslogClient {
    fn new(facility: &str) -> Self {
        Self { facility: facility.to_string() }
    }

    fn send(&self, priority: i32, message: &str) {
        eprintln!("[syslog:{}] prio={} {}", self.facility, priority, message);
    }
}

/// Adapter: hacer que SyslogClient satisfaga Logger
impl Logger for SyslogClient {
    fn log(&self, level: LogLevel, msg: &str) {
        let priority = match level {
            LogLevel::Debug => 7,
            LogLevel::Info  => 6,
            LogLevel::Warn  => 4,
            LogLevel::Error => 3,
        };
        self.send(priority, msg);
    }
}

/// Codigo generico que usa la interfaz
fn process_request(logger: &dyn Logger) {
    logger.log(LogLevel::Info, "processing request");
    // ... trabajo ...
    logger.log(LogLevel::Debug, "request done");
}

fn main() {
    let syslog = SyslogClient::new("myapp");
    process_request(&syslog);
}
```

```
  En C (T01): necesitas wrapper function + opaque struct.
  En Rust:    impl Logger for SyslogClient — UNA declaracion.

  El compilador genera el dispatch automaticamente:
  - &dyn Logger → vtable (como T01 seccion 4)
  - &impl Logger → monomorphizacion (sin vtable, zero-cost)
```

---

## 3. La orphan rule: cuando NO puedes hacer impl directamente

Rust tiene una regla fundamental: solo puedes implementar un
trait para un tipo si **al menos uno de los dos** (trait o tipo)
esta definido en tu crate:

```rust
// Tu crate define Logger → puedes implementar para cualquier tipo
trait Logger { fn log(&self, msg: &str); }
impl Logger for SyslogClient { ... }  // ✓ Logger es tuyo

// Pero si AMBOS son de otros crates:
// impl Display for Vec<i32> { ... }
// ERROR: ni Display ni Vec son tuyos
// Esto es la "orphan rule"
```

```
  ¿Puedes hacer impl Trait for Type?

  Trait tuyo + Type tuyo    → SI
  Trait tuyo + Type ajeno   → SI
  Trait ajeno + Type tuyo   → SI
  Trait ajeno + Type ajeno  → NO (orphan rule)
```

La orphan rule evita conflictos: si dos crates pudieran hacer
`impl Display for Vec<i32>`, el compilador no sabria cual usar.

---

## 4. Newtype: el adapter universal

Cuando la orphan rule te bloquea, envuelves el tipo ajeno en
un struct tuple de un campo — el **newtype**:

```rust
use std::fmt;

// Queremos que Vec<u8> se muestre como hex.
// No podemos: impl Display for Vec<u8> (ambos ajenos)

// Solucion: newtype
struct HexBytes(Vec<u8>);

impl fmt::Display for HexBytes {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for byte in &self.0 {
            write!(f, "{:02x}", byte)?;
        }
        Ok(())
    }
}

fn main() {
    let data = HexBytes(vec![0xDE, 0xAD, 0xBE, 0xEF]);
    println!("{}", data);  // "deadbeef"
}
```

```
  Sin newtype:
  impl Display for Vec<u8> { ... }
  → ERROR: orphan rule

  Con newtype:
  struct HexBytes(Vec<u8>);         ← Tu tipo
  impl Display for HexBytes { ... } ← Tu tipo + trait ajeno = OK
```

### 4.1 Newtype como adapter de API

```rust
/// Libreria de terceros: espera trait Serialize (no serde)
trait ThirdPartySerialize {
    fn serialize_to_bytes(&self) -> Vec<u8>;
}

/// Tu tipo existente con serde
#[derive(serde::Serialize)]
struct User {
    name: String,
    age: u32,
}

/// Newtype adapter
struct SerializableUser(User);

impl ThirdPartySerialize for SerializableUser {
    fn serialize_to_bytes(&self) -> Vec<u8> {
        // Traducir: serde JSON → bytes
        serde_json::to_vec(&self.0).unwrap_or_default()
    }
}

fn send_to_network(data: &dyn ThirdPartySerialize) {
    let bytes = data.serialize_to_bytes();
    println!("Sending {} bytes", bytes.len());
}

fn main() {
    let user = User { name: "Alice".into(), age: 30 };
    let adapted = SerializableUser(user);
    send_to_network(&adapted);
}
```

### 4.2 Costo del newtype: zero

```rust
use std::mem::size_of;

struct Meters(f64);
struct Kilograms(f64);

fn main() {
    assert_eq!(size_of::<f64>(), size_of::<Meters>());
    assert_eq!(size_of::<f64>(), size_of::<Kilograms>());
    // El newtype ocupa EXACTAMENTE lo mismo que el tipo interno.
    // Zero overhead — el wrapper no existe en el binario.
}
```

El compilador elimina el newtype completamente. Es una
abstraccion puramente en tiempo de compilacion.

---

## 5. Adapter con trait generico

Para adaptar multiples tipos a una interfaz comun:

```rust
/// Interfaz de almacenamiento (como T01 seccion 4)
trait Storage {
    fn read(&self, key: &str) -> Option<String>;
    fn write(&mut self, key: &str, value: &str);
    fn delete(&mut self, key: &str) -> bool;
}

/// Backend: HashMap en memoria
use std::collections::HashMap;

struct MemoryStorage {
    data: HashMap<String, String>,
}

impl MemoryStorage {
    fn new() -> Self {
        Self { data: HashMap::new() }
    }
}

impl Storage for MemoryStorage {
    fn read(&self, key: &str) -> Option<String> {
        self.data.get(key).cloned()
    }

    fn write(&mut self, key: &str, value: &str) {
        self.data.insert(key.to_string(), value.to_string());
    }

    fn delete(&mut self, key: &str) -> bool {
        self.data.remove(key).is_some()
    }
}

/// Backend: filesystem
use std::fs;
use std::path::PathBuf;

struct FileStorage {
    base_path: PathBuf,
}

impl FileStorage {
    fn new(path: &str) -> Self {
        let base = PathBuf::from(path);
        fs::create_dir_all(&base).ok();
        Self { base_path: base }
    }

    fn key_path(&self, key: &str) -> PathBuf {
        self.base_path.join(key)
    }
}

impl Storage for FileStorage {
    fn read(&self, key: &str) -> Option<String> {
        fs::read_to_string(self.key_path(key)).ok()
    }

    fn write(&mut self, key: &str, value: &str) {
        fs::write(self.key_path(key), value).ok();
    }

    fn delete(&mut self, key: &str) -> bool {
        fs::remove_file(self.key_path(key)).is_ok()
    }
}

/// Codigo generico: funciona con cualquier backend
fn backup_data(src: &dyn Storage, dst: &mut dyn Storage, keys: &[&str]) {
    for key in keys {
        if let Some(value) = src.read(key) {
            dst.write(key, &value);
        }
    }
}

fn main() {
    let mut mem = MemoryStorage::new();
    mem.write("config", "debug=true");
    mem.write("secret", "password123");

    let mut fs = FileStorage::new("/tmp/backup");
    backup_data(&mem, &mut fs, &["config", "secret"]);

    println!("{:?}", fs.read("config"));  // Some("debug=true")
}
```

```
  Comparacion con C (T01):

  C:    StorageOps vtable + void *backend_data
        Cada backend: static const StorageOps + factory function
        Caller: storage_read(s, key, buf, buf_size)

  Rust: trait Storage { fn read(&self, key) -> Option<String> }
        Cada backend: impl Storage for ConcreteType
        Caller: storage.read(key) — sin vtable manual

  Lo que Rust agrega:
  - Sin void* casting (generics o dyn Trait)
  - Sin malloc/free manual (String/Vec manejan memoria)
  - Errores de tipo en compile time, no en runtime
  - Drop automatico al salir de scope
```

---

## 6. Adapter con From/Into

`From` y `Into` son traits standard para conversion entre tipos.
Son adapters idiomaticos de Rust:

```rust
/// Formato interno de coordenadas
#[derive(Debug)]
struct Point { x: f64, y: f64 }

/// Formato de una libreria de terceros
struct LatLng { lat: f64, lng: f64 }

/// Formato de otra libreria
struct GeoCoord { latitude: f32, longitude: f32 }

/// Adapter: LatLng → Point
impl From<LatLng> for Point {
    fn from(ll: LatLng) -> Self {
        Self { x: ll.lng, y: ll.lat }  // Nota: lng→x, lat→y
    }
}

/// Adapter: GeoCoord → Point
impl From<GeoCoord> for Point {
    fn from(gc: GeoCoord) -> Self {
        Self { x: gc.longitude as f64, y: gc.latitude as f64 }
    }
}

/// Adapter bidireccional: Point → LatLng
impl From<Point> for LatLng {
    fn from(p: Point) -> Self {
        Self { lat: p.y, lng: p.x }
    }
}

/// Funcion generica que acepta cualquier cosa convertible a Point
fn distance_to_origin(p: impl Into<Point>) -> f64 {
    let p = p.into();
    (p.x * p.x + p.y * p.y).sqrt()
}

fn main() {
    let ll = LatLng { lat: 3.0, lng: 4.0 };
    let gc = GeoCoord { latitude: 3.0, longitude: 4.0 };

    // From/Into como adapter automatico
    println!("{}", distance_to_origin(ll));    // 5.0
    println!("{}", distance_to_origin(gc));    // 5.0

    // Conversion explicita
    let p = Point { x: 10.0, y: 20.0 };
    let back: LatLng = p.into();
    println!("{}, {}", back.lat, back.lng);    // 20, 10
}
```

```
  From/Into como adapter pattern:
  ───────────────────────────────

  impl From<ExternalType> for MyType
  → "Puedo construir mi tipo a partir del externo"
  → Conversion automatica con .into() o ::from()

  fn process(input: impl Into<MyType>)
  → "Acepto cualquier cosa que se convierta a mi tipo"
  → El caller no necesita convertir explicitamente
```

---

## 7. Adapter con closures

Para adaptaciones simples y puntuales, un closure basta:

```rust
/// Interfaz que espera el procesador
fn process_lines(input: &mut dyn Iterator<Item = String>) {
    for line in input {
        println!("Processing: {}", line);
    }
}

fn main() {
    // Fuente 1: Vec de &str
    let data = vec!["hello", "world"];
    let mut iter1 = data.into_iter().map(|s| s.to_string());
    process_lines(&mut iter1);

    // Fuente 2: rango de numeros
    let mut iter2 = (1..=5).map(|n| format!("line_{}", n));
    process_lines(&mut iter2);

    // Fuente 3: CSV con adapter inline
    let csv = "name,age\nAlice,30\nBob,25";
    let mut iter3 = csv.lines().skip(1).map(|line| {
        // Adapter: CSV line → formatted string
        let parts: Vec<&str> = line.split(',').collect();
        format!("{} (age {})", parts[0], parts[1])
    });
    process_lines(&mut iter3);
}
```

El `.map()` actua como adapter: transforma el formato de cada
elemento para que coincida con lo que `process_lines` espera.

---

## 8. Adapter para tipos de error

Uno de los usos mas comunes de adapter en Rust:

```rust
use std::fmt;
use std::io;
use std::num::ParseIntError;

/// Error de nuestra aplicacion
#[derive(Debug)]
enum AppError {
    Io(io::Error),
    Parse(ParseIntError),
    Config(String),
}

impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AppError::Io(e)     => write!(f, "IO error: {}", e),
            AppError::Parse(e)  => write!(f, "Parse error: {}", e),
            AppError::Config(s) => write!(f, "Config error: {}", s),
        }
    }
}

/// Adapter: io::Error → AppError
impl From<io::Error> for AppError {
    fn from(e: io::Error) -> Self {
        AppError::Io(e)
    }
}

/// Adapter: ParseIntError → AppError
impl From<ParseIntError> for AppError {
    fn from(e: ParseIntError) -> Self {
        AppError::Parse(e)
    }
}

/// Ahora el ? operator adapta errores automaticamente
fn read_config(path: &str) -> Result<u32, AppError> {
    let content = std::fs::read_to_string(path)?;  // io::Error → AppError
    let port: u32 = content.trim().parse()?;        // ParseIntError → AppError
    if port == 0 {
        return Err(AppError::Config("port cannot be 0".into()));
    }
    Ok(port)
}
```

```
  El operador ? usa From para adaptar errores:

  std::fs::read_to_string(path)
  → Result<String, io::Error>
  → ? convierte io::Error via From<io::Error> for AppError
  → Result<String, AppError>

  content.trim().parse::<u32>()
  → Result<u32, ParseIntError>
  → ? convierte via From<ParseIntError> for AppError
  → Result<u32, AppError>

  Cada impl From<E> for AppError es un adapter de errores.
```

### 8.1 thiserror: genera los adapters automaticamente

```rust
use thiserror::Error;

#[derive(Debug, Error)]
enum AppError {
    #[error("IO error: {0}")]
    Io(#[from] io::Error),           // Genera impl From<io::Error>

    #[error("Parse error: {0}")]
    Parse(#[from] ParseIntError),    // Genera impl From<ParseIntError>

    #[error("Config error: {0}")]
    Config(String),
}

// Misma funcionalidad, sin escribir los impl From manualmente.
```

---

## 9. Adapter: static dispatch vs dynamic dispatch

```rust
trait Encoder {
    fn encode(&self, data: &[u8]) -> Vec<u8>;
}

struct Base64Encoder;
struct HexEncoder;

impl Encoder for Base64Encoder {
    fn encode(&self, data: &[u8]) -> Vec<u8> {
        // Simplificado
        data.iter().map(|b| b.wrapping_add(1)).collect()
    }
}

impl Encoder for HexEncoder {
    fn encode(&self, data: &[u8]) -> Vec<u8> {
        let mut out = Vec::with_capacity(data.len() * 2);
        for b in data {
            out.extend_from_slice(format!("{:02x}", b).as_bytes());
        }
        out
    }
}

// Static dispatch: el compilador genera codigo para cada tipo
// Zero overhead, pero no puede cambiar en runtime
fn process_static(encoder: &impl Encoder, data: &[u8]) -> Vec<u8> {
    encoder.encode(data)
}

// Dynamic dispatch: vtable como en C
// Un poquito de overhead, pero puede cambiar en runtime
fn process_dynamic(encoder: &dyn Encoder, data: &[u8]) -> Vec<u8> {
    encoder.encode(data)
}

fn main() {
    let data = b"hello";

    // Static: el compilador sabe el tipo exacto
    let result = process_static(&Base64Encoder, data);

    // Dynamic: elegir en runtime
    let encoders: Vec<Box<dyn Encoder>> = vec![
        Box::new(Base64Encoder),
        Box::new(HexEncoder),
    ];
    for enc in &encoders {
        let result = process_dynamic(enc.as_ref(), data);
        println!("{:?}", result);
    }
}
```

```
  Comparacion:

  C (T01 vtable):            Rust dyn Trait:         Rust impl Trait:
  ──────────────             ──────────────          ───────────────
  StorageOps *ops            &dyn Storage            &impl Storage
  ops->read(s, ...)          storage.read(...)       storage.read(...)
  Siempre dynamic dispatch   Dynamic dispatch        Static dispatch
  Runtime overhead           Runtime overhead        Zero overhead
  Un solo binario            Un solo binario         Code duplicado

  C no tiene equivalente de impl Trait (static dispatch).
  El vtable es la unica opcion en C.
```

---

## 10. Comparacion completa: C vs Rust

```
  Patron C (T01)                Equivalente Rust
  ──────────────                ────────────────
  Wrapper function              Standalone fn o From/Into impl
  Opaque struct adapter         Newtype: struct Adapter(Inner)
  Vtable (StorageOps)           trait Storage + impl per backend
  void *backend_data            Campos tipados en cada impl
  factory function              Constructor o From::from
  Link-time backend selection   Feature flags / cfg
  Runtime backend selection     Box<dyn Trait> o enum dispatch
  Callback con void *user_data  Closure (captura entorno)
  #ifdef MOCK                   cfg(test) + mock impl
  Traduccion de errores         impl From<E1> for E2 + ? operator

  Lo que Rust agrega:
  ────────────────────
  • Orphan rule previene conflictos de impl
  • Newtype con zero overhead (compilador elimina el wrapper)
  • From/Into como adapter idiomatico (? operator lo usa)
  • Static dispatch (impl Trait) sin vtable
  • Drop automatico al salir de scope
  • thiserror para generar adapters de error automaticamente
```

---

## 11. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| `impl ForeignTrait for ForeignType` | Orphan rule | Newtype wrapper |
| Newtype sin delegar metodos | Pierde toda la API interna | Implementar Deref o delegar manualmente |
| Deref en newtype semantico | Confunde "es-un" con "se-convierte-a" | Solo metodos especificos, no Deref |
| Adapter que clona todo | Overhead de copias innecesarias | Usar referencias o Cow |
| From impl que puede fallar | `From` no retorna Result | Usar `TryFrom` en vez de `From` |
| Demasiados niveles de newtype | `Wrapper(Wrapper2(Wrapper3(T)))` | Maximo 1-2 niveles |
| Olvidar impl Display para error adapter | `?` compila pero los errores no se muestran bien | thiserror o impl Display manual |
| Static dispatch donde necesitas runtime | `impl Trait` no puede ir en Vec | `Box<dyn Trait>` para colecciones |

---

## 12. Ejercicios

### Ejercicio 1 — impl Trait como adapter

Tienes un trait `Formatter` y un tipo `JsonPrinter` con API
diferente. Implementa el adapter.

```rust
trait Formatter {
    fn format(&self, key: &str, value: &str) -> String;
}

struct JsonPrinter;
impl JsonPrinter {
    fn print_pair(&self, k: &str, v: &str) -> String {
        format!("{{\"{}\":\"{}\"}}", k, v)
    }
}
```

**Prediccion**: ¿cuantas lineas necesita el adapter?

<details>
<summary>Respuesta</summary>

```rust
impl Formatter for JsonPrinter {
    fn format(&self, key: &str, value: &str) -> String {
        self.print_pair(key, value)
    }
}

fn display(f: &dyn Formatter) {
    println!("{}", f.format("name", "Alice"));
}

fn main() {
    let jp = JsonPrinter;
    display(&jp);  // {"name":"Alice"}
}
```

Tres lineas en el impl (sin contar llaves). El adapter solo
delega `format` a `print_pair` — el caso mas simple.

</details>

---

### Ejercicio 2 — Newtype por orphan rule

Quieres implementar `std::fmt::UpperHex` para `Vec<u8>`.
No puedes por la orphan rule.

**Prediccion**: ¿como lo resuelves? ¿Cual es el costo en runtime?

<details>
<summary>Respuesta</summary>

```rust
use std::fmt;

struct HexVec<'a>(&'a [u8]);

impl fmt::UpperHex for HexVec<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        for byte in self.0 {
            write!(f, "{:02X}", byte)?;
        }
        Ok(())
    }
}

impl fmt::Display for HexVec<'_> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "0x")?;
        fmt::UpperHex::fmt(self, f)
    }
}

fn main() {
    let data = vec![0xCA, 0xFE, 0xBA, 0xBE];
    println!("{:X}", HexVec(&data));   // CAFEBABE
    println!("{}", HexVec(&data));     // 0xCAFEBABE
}
```

Costo en runtime: **zero**. `HexVec(&[u8])` es una referencia
de 16 bytes (puntero + largo), identico a `&[u8]`. El struct
wrapper no existe en el binario — el compilador lo elimina.

Nota: `HexVec` toma `&[u8]` en vez de `Vec<u8>` para no tomar
ownership. Asi puedes usar `HexVec(&data)` sin mover `data`.

</details>

---

### Ejercicio 3 — From/Into como adapter

Implementa conversiones entre tres formatos de temperatura:

```rust
struct Celsius(f64);
struct Fahrenheit(f64);
struct Kelvin(f64);
```

**Prediccion**: ¿cuantos `impl From` necesitas para conversion
completa entre los tres?

<details>
<summary>Respuesta</summary>

6 conversiones (cada par en ambas direcciones):

```rust
#[derive(Debug, Clone, Copy)]
struct Celsius(f64);

#[derive(Debug, Clone, Copy)]
struct Fahrenheit(f64);

#[derive(Debug, Clone, Copy)]
struct Kelvin(f64);

// C ↔ F
impl From<Celsius> for Fahrenheit {
    fn from(c: Celsius) -> Self {
        Fahrenheit(c.0 * 9.0 / 5.0 + 32.0)
    }
}

impl From<Fahrenheit> for Celsius {
    fn from(f: Fahrenheit) -> Self {
        Celsius((f.0 - 32.0) * 5.0 / 9.0)
    }
}

// C ↔ K
impl From<Celsius> for Kelvin {
    fn from(c: Celsius) -> Self {
        Kelvin(c.0 + 273.15)
    }
}

impl From<Kelvin> for Celsius {
    fn from(k: Kelvin) -> Self {
        Celsius(k.0 - 273.15)
    }
}

// F ↔ K (via Celsius para evitar duplicar formulas)
impl From<Fahrenheit> for Kelvin {
    fn from(f: Fahrenheit) -> Self {
        let c: Celsius = f.into();
        c.into()
    }
}

impl From<Kelvin> for Fahrenheit {
    fn from(k: Kelvin) -> Self {
        let c: Celsius = k.into();
        c.into()
    }
}

fn boiling_point(temp: impl Into<Celsius>) -> bool {
    temp.into().0 >= 100.0
}

fn main() {
    let water = Fahrenheit(212.0);
    println!("Boiling? {}", boiling_point(water));  // true

    let absolute_zero = Kelvin(0.0);
    let c: Celsius = absolute_zero.into();
    println!("{:?} = {:?}", absolute_zero, c);  // -273.15
}
```

6 impls: C→F, F→C, C→K, K→C, F→K, K→F. Las dos ultimas
delegan via Celsius para no duplicar la logica de conversion.
Con N tipos, necesitas N*(N-1) impls para conversion completa.

</details>

---

### Ejercicio 4 — Adapter de Storage (como T01)

Reimplementa el ejemplo de Storage de T01 (file + memory backends)
usando traits en Rust.

**Prediccion**: ¿cuantas lineas menos que la version C?

<details>
<summary>Respuesta</summary>

```rust
use std::collections::HashMap;
use std::fs;
use std::path::PathBuf;

trait Storage {
    fn read(&self, key: &str) -> Option<String>;
    fn write(&mut self, key: &str, value: &str);
    fn delete(&mut self, key: &str) -> bool;
}

// Backend: memoria
struct MemoryStorage(HashMap<String, String>);

impl MemoryStorage {
    fn new() -> Self { Self(HashMap::new()) }
}

impl Storage for MemoryStorage {
    fn read(&self, key: &str) -> Option<String> {
        self.0.get(key).cloned()
    }
    fn write(&mut self, key: &str, value: &str) {
        self.0.insert(key.into(), value.into());
    }
    fn delete(&mut self, key: &str) -> bool {
        self.0.remove(key).is_some()
    }
}

// Backend: filesystem
struct FileStorage(PathBuf);

impl FileStorage {
    fn new(base: &str) -> Self {
        let p = PathBuf::from(base);
        fs::create_dir_all(&p).ok();
        Self(p)
    }
}

impl Storage for FileStorage {
    fn read(&self, key: &str) -> Option<String> {
        fs::read_to_string(self.0.join(key)).ok()
    }
    fn write(&mut self, key: &str, value: &str) {
        fs::write(self.0.join(key), value).ok();
    }
    fn delete(&mut self, key: &str) -> bool {
        fs::remove_file(self.0.join(key)).is_ok()
    }
}

// Codigo generico
fn backup(src: &dyn Storage, dst: &mut dyn Storage, keys: &[&str]) {
    for key in keys {
        if let Some(val) = src.read(key) {
            dst.write(key, &val);
        }
    }
}

fn main() {
    let mut mem = MemoryStorage::new();
    mem.write("config", "debug=true");

    let mut fs_store = FileStorage::new("/tmp/backup");
    backup(&mem, &mut fs_store, &["config"]);

    println!("{:?}", fs_store.read("config"));
}
```

~50 lineas vs ~150 en C. Ahorro principal:
- Sin vtable manual (el trait lo genera)
- Sin `void *backend_data` (el struct ES el backend)
- Sin malloc/free (String y HashMap manejan memoria)
- Sin buffer + size params (String retorna owned data)

</details>

---

### Ejercicio 5 — Adapter de errores con From

Tu aplicacion tiene tres fuentes de error: IO, JSON parsing,
y validacion. Implementa el adapter con `From`.

**Prediccion**: ¿que permite el `?` operator despues de
implementar `From`?

<details>
<summary>Respuesta</summary>

```rust
use std::io;
use std::fmt;

#[derive(Debug)]
enum AppError {
    Io(io::Error),
    Json(serde_json::Error),
    Validation(String),
}

impl fmt::Display for AppError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            AppError::Io(e)         => write!(f, "IO: {}", e),
            AppError::Json(e)       => write!(f, "JSON: {}", e),
            AppError::Validation(s) => write!(f, "Validation: {}", s),
        }
    }
}

impl From<io::Error> for AppError {
    fn from(e: io::Error) -> Self { AppError::Io(e) }
}

impl From<serde_json::Error> for AppError {
    fn from(e: serde_json::Error) -> Self { AppError::Json(e) }
}

#[derive(serde::Deserialize)]
struct Config {
    port: u16,
    host: String,
}

fn load_config(path: &str) -> Result<Config, AppError> {
    let data = std::fs::read_to_string(path)?;   // io::Error → AppError
    let cfg: Config = serde_json::from_str(&data)?; // serde Error → AppError

    if cfg.port == 0 {
        return Err(AppError::Validation("port cannot be 0".into()));
    }

    Ok(cfg)
}
```

El `?` operator llama `From::from(error)` automaticamente.
Sin los `impl From`, tendrias que escribir:

```rust
let data = std::fs::read_to_string(path)
    .map_err(|e| AppError::Io(e))?;
let cfg: Config = serde_json::from_str(&data)
    .map_err(|e| AppError::Json(e))?;
```

Con `From`, el `?` hace la conversion implicitamente.

</details>

---

### Ejercicio 6 — Newtype con delegacion

Crea un newtype `SortedVec<T>` que envuelve `Vec<T>` y garantiza
que siempre esta ordenado. Delega `len`, `is_empty`, `iter`
pero NO `push` (usa `insert` que mantiene el orden).

**Prediccion**: ¿deberias implementar `Deref<Target = Vec<T>>`?

<details>
<summary>Respuesta</summary>

```rust
#[derive(Debug)]
struct SortedVec<T: Ord>(Vec<T>);

impl<T: Ord> SortedVec<T> {
    fn new() -> Self {
        Self(Vec::new())
    }

    fn insert(&mut self, value: T) {
        let pos = self.0.binary_search(&value).unwrap_or_else(|p| p);
        self.0.insert(pos, value);
    }

    // Delegar metodos read-only
    fn len(&self) -> usize { self.0.len() }
    fn is_empty(&self) -> bool { self.0.is_empty() }
    fn iter(&self) -> std::slice::Iter<'_, T> { self.0.iter() }
    fn get(&self, index: usize) -> Option<&T> { self.0.get(index) }
    fn contains(&self, value: &T) -> bool { self.0.binary_search(value).is_ok() }
    fn first(&self) -> Option<&T> { self.0.first() }
    fn last(&self) -> Option<&T> { self.0.last() }

    // Acceso al slice interno (read-only)
    fn as_slice(&self) -> &[T] { &self.0 }
}

fn main() {
    let mut sv = SortedVec::new();
    sv.insert(5);
    sv.insert(1);
    sv.insert(3);

    println!("{:?}", sv);  // SortedVec([1, 3, 5])
    println!("Contains 3? {}", sv.contains(&3));  // true
}
```

**NO implementar `Deref<Target = Vec<T>>`** porque:
- `Deref` expondria `.push()` que rompe la invariante de orden
- `Deref` expondria `.swap()`, `.sort_unstable()`, etc.
- El invariante ("siempre ordenado") se romperia facilmente

`Deref` hacia `[T]` seria aceptable (slice es read-only):

```rust
impl<T: Ord> std::ops::Deref for SortedVec<T> {
    type Target = [T];
    fn deref(&self) -> &[T] { &self.0 }
}
// Ahora sv.len(), sv.iter(), sv[i] funcionan via slice
// Pero no sv.push() (slice no tiene push)
```

Regla: Deref solo hacia tipos que no pueden romper invariantes
del newtype. T03 explora esto en detalle.

</details>

---

### Ejercicio 7 — Adapter con closure

Adapta un iterador de `(i32, i32)` a un iterador de `Point`:

```rust
struct Point { x: f64, y: f64 }
```

**Prediccion**: ¿necesitas un struct adapter o basta un `.map()`?

<details>
<summary>Respuesta</summary>

`.map()` basta:

```rust
#[derive(Debug)]
struct Point { x: f64, y: f64 }

fn process_points(points: &mut dyn Iterator<Item = Point>) {
    for p in points {
        println!("({}, {})", p.x, p.y);
    }
}

fn main() {
    let raw_data = vec![(1, 2), (3, 4), (5, 6)];

    // Adapter: closure en .map()
    let mut adapted = raw_data
        .into_iter()
        .map(|(x, y)| Point { x: x as f64, y: y as f64 });

    process_points(&mut adapted);
}
```

No necesitas struct adapter. `.map()` es el adapter — convierte
cada `(i32, i32)` a `Point` inline. Esto es idiomatico en Rust
y equivale a un wrapper function en C, pero mas composable
(puedes encadenar `.filter().map().take()` etc.).

Solo necesitas un struct adapter cuando:
- La conversion necesita estado (counter, cache)
- La conversion es compleja (multiples pasos)
- Quieres reutilizar la conversion en multiples lugares

</details>

---

### Ejercicio 8 — Adapter para testing (mock)

Tienes un trait `Clock` que obtiene el tiempo. Implementa un
adapter real y un mock.

**Prediccion**: ¿como haces que el mock devuelva tiempos
controlados en tests?

<details>
<summary>Respuesta</summary>

```rust
use std::time::{SystemTime, UNIX_EPOCH};
use std::cell::Cell;

trait Clock {
    fn now_secs(&self) -> u64;
}

// Adapter real: usa SystemTime
struct SystemClock;

impl Clock for SystemClock {
    fn now_secs(&self) -> u64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_secs()
    }
}

// Adapter mock: tiempo controlable
struct MockClock {
    time: Cell<u64>,
}

impl MockClock {
    fn new(initial: u64) -> Self {
        Self { time: Cell::new(initial) }
    }

    fn advance(&self, seconds: u64) {
        self.time.set(self.time.get() + seconds);
    }

    fn set(&self, time: u64) {
        self.time.set(time);
    }
}

impl Clock for MockClock {
    fn now_secs(&self) -> u64 {
        self.time.get()
    }
}

// Logica de negocio: usa trait, no implementacion concreta
struct Cache<C: Clock> {
    clock: C,
    ttl: u64,
    data: Vec<(String, String, u64)>,  // key, value, stored_at
}

impl<C: Clock> Cache<C> {
    fn new(clock: C, ttl: u64) -> Self {
        Self { clock, ttl, data: Vec::new() }
    }

    fn put(&mut self, key: &str, value: &str) {
        let now = self.clock.now_secs();
        self.data.push((key.into(), value.into(), now));
    }

    fn get(&self, key: &str) -> Option<&str> {
        let now = self.clock.now_secs();
        self.data.iter().rev()
            .find(|(k, _, stored)| k == key && now - stored < self.ttl)
            .map(|(_, v, _)| v.as_str())
    }
}

// Produccion
fn main() {
    let mut cache = Cache::new(SystemClock, 60);
    cache.put("key", "value");
    println!("{:?}", cache.get("key"));
}

// Tests
#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cache_not_expired() {
        let clock = MockClock::new(1000);
        let mut cache = Cache::new(clock, 60);

        cache.put("key", "value");
        assert_eq!(cache.get("key"), Some("value"));
    }

    #[test]
    fn test_cache_expired() {
        let clock = MockClock::new(1000);
        let mut cache = Cache::new(clock, 60);

        cache.put("key", "value");
        cache.clock.advance(61);  // Avanzar 61 segundos
        assert_eq!(cache.get("key"), None);  // Expirado
    }
}
```

El mock usa `Cell<u64>` para poder mutar el tiempo con `&self`
(sin `&mut self`), lo que permite llamar `cache.clock.advance()`
sin necesitar `&mut cache`. En C (T01 ejercicio 4) usamos un
static global — aqui el mock es un valor local por test.

</details>

---

### Ejercicio 9 — Adapter bidireccional con TryFrom

Adapta entre tu tipo `Color` y un `u32` (packed RGBA):

```rust
struct Color { r: u8, g: u8, b: u8, a: u8 }
```

**Prediccion**: ¿`From` o `TryFrom`? ¿En que direccion puede fallar?

<details>
<summary>Respuesta</summary>

```rust
#[derive(Debug, PartialEq)]
struct Color { r: u8, g: u8, b: u8, a: u8 }

// Color → u32: siempre exitoso (From)
impl From<Color> for u32 {
    fn from(c: Color) -> Self {
        (c.r as u32) << 24
        | (c.g as u32) << 16
        | (c.b as u32) << 8
        | c.a as u32
    }
}

// u32 → Color: siempre exitoso (From)
// Cada u32 mapea a un Color valido
impl From<u32> for Color {
    fn from(packed: u32) -> Self {
        Self {
            r: (packed >> 24) as u8,
            g: (packed >> 16) as u8,
            b: (packed >> 8) as u8,
            a: packed as u8,
        }
    }
}

fn main() {
    let red = Color { r: 255, g: 0, b: 0, a: 255 };
    let packed: u32 = red.into();
    println!("0x{:08X}", packed);  // 0xFF0000FF

    let back: Color = packed.into();
    assert_eq!(back, Color { r: 255, g: 0, b: 0, a: 255 });
}
```

Ambas direcciones usan `From` (no `TryFrom`) porque:
- Color → u32: 4 bytes caben en 32 bits, siempre
- u32 → Color: cada valor de 32 bits produce un Color valido

`TryFrom` seria necesario si la conversion pudiera fallar:

```rust
// Ejemplo donde TryFrom es necesario:
impl TryFrom<&str> for Color {
    type Error = ColorParseError;
    fn try_from(s: &str) -> Result<Self, Self::Error> {
        // "#FF0000FF" → Color
        // Puede fallar: formato invalido, hex invalido
        todo!()
    }
}
```

Regla: `From` si siempre exitoso, `TryFrom` si puede fallar.

</details>

---

### Ejercicio 10 — Elegir la forma de adapter

Para cada situacion, ¿que forma de adapter usarias?

1. Implementar `Display` para `chrono::DateTime`
2. Convertir `Config` propio a `serde_json::Value`
3. Hacer que `Vec<Sensor>` se pueda usar como `Iterator<Item=Reading>`
4. Unificar `std::io::Error` y `serde_json::Error` en tu error type

**Prediccion**: ¿newtype, From, impl Trait, o closure?

<details>
<summary>Respuesta</summary>

**1. Display para chrono::DateTime** → **Newtype**
```rust
struct FormattedDate(chrono::DateTime<chrono::Utc>);
impl fmt::Display for FormattedDate {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0.format("%Y-%m-%d"))
    }
}
```
Orphan rule: Display y DateTime son ambos de otros crates.

**2. Config → serde_json::Value** → **From**
```rust
impl From<Config> for serde_json::Value {
    fn from(cfg: Config) -> Self {
        serde_json::json!({
            "port": cfg.port,
            "host": cfg.host,
        })
    }
}
```
Config es tuyo, puedes implementar From directamente.

**3. Vec<Sensor> como Iterator<Item=Reading>** → **Closure (.map)**
```rust
let readings = sensors.iter().map(|s| s.read());
```
No necesita struct. `.map()` adapta `&Sensor` → `Reading`.

**4. Unificar errores** → **From** (o thiserror)
```rust
impl From<io::Error> for AppError {
    fn from(e: io::Error) -> Self { AppError::Io(e) }
}
impl From<serde_json::Error> for AppError {
    fn from(e: serde_json::Error) -> Self { AppError::Json(e) }
}
```
Permite usar `?` para conversion automatica.

Resumen:
- Orphan rule bloqueante → Newtype
- Conversion entre tipos → From/TryFrom
- Transformacion de iterador → .map() con closure
- Unificar errores → From (o thiserror #[from])

</details>
