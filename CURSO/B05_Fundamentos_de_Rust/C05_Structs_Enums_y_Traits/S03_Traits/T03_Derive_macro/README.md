# T03 — Derive macro

## Que es derive

`#[derive(...)]` es un atributo que genera automaticamente la
implementacion de uno o mas traits para un struct o enum.
Es una **macro procedural** que se ejecuta en tiempo de
compilacion: examina la definicion del tipo y genera codigo
Rust equivalente a escribir `impl Trait for Type` a mano.

```rust
// Sin derive — implementacion manual de Debug y Clone:
struct Point {
    x: f64,
    y: f64,
}

impl std::fmt::Debug for Point {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("Point")
            .field("x", &self.x)
            .field("y", &self.y)
            .finish()
    }
}

impl Clone for Point {
    fn clone(&self) -> Self {
        Point { x: self.x.clone(), y: self.y.clone() }
    }
}
```

```rust
// Con derive — el compilador genera lo mismo automaticamente:
#[derive(Debug, Clone)]
struct Point {
    x: f64,
    y: f64,
}

// Funciona tanto en structs como en enums:
#[derive(Debug, Clone, PartialEq)]
enum Color {
    Red,
    Green,
    Blue,
    Custom(u8, u8, u8),
}
// Para enums, el codigo generado hace match sobre cada variante.
```

## Traits derivables de la libreria estandar

No requieren dependencias externas. Cada uno genera una
implementacion campo por campo (o variante por variante):

```rust
// Debug — formato de depuracion con {:?} y {:#?}
#[derive(Debug)]
struct User { name: String, age: u32 }

let u = User { name: String::from("Ana"), age: 30 };
println!("{:?}", u);    // User { name: "Ana", age: 30 }
println!("{:#?}", u);   // version multi-linea con indentacion

// Clone — copia profunda con .clone()
#[derive(Clone)]
struct Config { path: String, retries: u32 }

let c1 = Config { path: String::from("/etc"), retries: 3 };
let c2 = c1.clone();   // c2 tiene su propio String en el heap

// Copy — copia implicita (requiere tambien Clone).
// Solo si todos los campos son Copy (viven en el stack).
#[derive(Debug, Clone, Copy)]
struct Vec2 { x: f32, y: f32 }

let a = Vec2 { x: 1.0, y: 2.0 };
let b = a;     // copia implicita, a sigue valido
```

```rust
// PartialEq — comparacion con == y !=, campo por campo.
#[derive(PartialEq)]
struct Dimensions { width: u32, height: u32 }

let a = Dimensions { width: 10, height: 20 };
let b = Dimensions { width: 10, height: 20 };
assert!(a == b);

// Eq — marcador sobre PartialEq. Afirma que x == x siempre es true.
// f32/f64 NO implementan Eq (NaN != NaN). Se requiere para HashMap keys.
#[derive(PartialEq, Eq)]
struct Id { value: u64 }

// PartialOrd — comparacion con <, >, <=, >=. Requiere PartialEq.
// Compara campo por campo en el orden de declaracion.
#[derive(PartialEq, PartialOrd)]
struct Version { major: u32, minor: u32, patch: u32 }

// Ord — orden total (para sort, BTreeMap). Requiere Eq + PartialOrd.
#[derive(PartialEq, Eq, PartialOrd, Ord)]
struct Priority { level: u8, name: String }
```

```rust
// Hash — calcula un hash del valor. Requiere Eq.
// Necesario para claves de HashMap/HashSet.
use std::collections::HashSet;

#[derive(PartialEq, Eq, Hash)]
struct Tag { name: String }

let mut tags = HashSet::new();
tags.insert(Tag { name: String::from("rust") });
tags.insert(Tag { name: String::from("rust") });   // duplicado, no se inserta
assert_eq!(tags.len(), 1);

// Default — valor por defecto para cada campo (bool→false, u32→0, String→"").
#[derive(Debug, Default)]
struct Settings { verbose: bool, retries: u32, name: String }

let s = Settings::default();
// Settings { verbose: false, retries: 0, name: "" }

// Util con struct update syntax:
let custom = Settings { verbose: true, ..Settings::default() };
```

## Requisitos para derive

Para que `#[derive(Trait)]` funcione, **todos los campos**
deben implementar ese trait. Si un solo campo no lo implementa,
el compilador rechaza el derive:

```rust
#[derive(Clone, Copy)]    // OK: todos los campos son Copy
struct Small { x: i32, y: i32 }

#[derive(Clone, Copy)]    // ERROR: String no es Copy
struct WithString {
    id: u32,
    name: String,
}
// error[E0204]: the trait `Copy` may not be implemented for this type
//   = note: this field does not implement `Copy`
//     name: String,
//     ------------ this field does not implement `Copy`

#[derive(Hash)]            // ERROR: f64 no implementa Hash
struct Measurement {
    value: f64,            // f64: Hash no existe (problemas con NaN)
    unit: String,
}
// error[E0277]: the trait bound `f64: Hash` is not satisfied
```

```rust
// Como leer el error y que hacer:
// 1. El compilador indica que trait fallo y cual campo es el problema.
// 2. Opciones:
//    a. No derivar ese trait (quedarse con menos derives)
//    b. Cambiar el tipo del campo (e.g. &str en vez de String para Copy)
//    c. Implementar el trait manualmente con logica personalizada
```

## Cuando derive funciona bien

Derive es la opcion correcta cuando el comportamiento campo
por campo es exactamente lo que necesitas:

```rust
// Tipos de datos simples — derive cubre todo:
#[derive(Debug, Clone, PartialEq)]
struct Rectangle { width: f64, height: f64 }

// Enums con datos:
#[derive(Debug, Clone, PartialEq)]
enum Shape {
    Circle(f64),
    Rectangle { width: f64, height: f64 },
}

assert_eq!(Shape::Circle(5.0), Shape::Circle(5.0));
assert_ne!(Shape::Circle(5.0), Shape::Circle(3.0));

// Newtype pattern — derive propaga el trait del tipo interno:
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
struct UserId(u64);       // Copy porque u64 es Copy

#[derive(Debug, Clone, PartialEq)]
struct Email(String);     // NO puede ser Copy (String no es Copy)
```

## Cuando implementar manualmente

El comportamiento campo por campo no siempre es lo correcto:

```rust
// 1. Ignorar campos en la comparacion.
struct User {
    id: u64,
    name: String,
    cache_key: String,   // campo interno, no parte de la identidad
}

impl PartialEq for User {
    fn eq(&self, other: &Self) -> bool {
        self.id == other.id && self.name == other.name
        // cache_key no se compara
    }
}
```

```rust
// 2. Orden personalizado (solo por un campo).
struct Task { priority: u8, name: String, created_at: u64 }

// derive(Ord) ordenaria por priority, luego name, luego created_at.
// Implementacion manual para ordenar SOLO por priority:
impl Ord for Task {
    fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        self.priority.cmp(&other.priority)
    }
}
impl PartialOrd for Task {
    fn partial_cmp(&self, other: &Self) -> Option<std::cmp::Ordering> {
        Some(self.cmp(other))
    }
}
impl PartialEq for Task {
    fn eq(&self, other: &Self) -> bool { self.priority == other.priority }
}
impl Eq for Task {}
```

```rust
// 3. Display — NO se puede derivar. Siempre es manual.
use std::fmt;

#[derive(Debug)]
struct Temperature { celsius: f64 }

impl fmt::Display for Temperature {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{:.1} C", self.celsius)
    }
}

let t = Temperature { celsius: 36.6 };
println!("{}", t);    // 36.6 C       (Display, manual)
println!("{:?}", t);  // Temperature { celsius: 36.6 }  (Debug, derivado)
```

```rust
// 4. Debug personalizado — ocultar datos sensibles.
use std::fmt;

struct Credentials { username: String, password: String }

impl fmt::Debug for Credentials {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("Credentials")
            .field("username", &self.username)
            .field("password", &"[REDACTED]")
            .finish()
    }
}
// Credentials { username: "admin", password: "[REDACTED]" }

// 5. Optimizacion — Clone que invalida caches en vez de copiarlos.
struct DataSet { raw: Vec<u8>, cached_sum: Option<u64> }

impl Clone for DataSet {
    fn clone(&self) -> Self {
        DataSet { raw: self.raw.clone(), cached_sum: None }
    }
}
```

## Derive macros de terceros

Crates externos pueden definir sus propias derive macros.
Para usarlas: agregar la dependencia, importar el trait, usar `#[derive(...)]`.

```toml
# Cargo.toml
[dependencies]
serde = { version = "1", features = ["derive"] }
serde_json = "1"
thiserror = "2"
```

```rust
// serde — serializacion y deserializacion a JSON, TOML, YAML, etc.
use serde::{Serialize, Deserialize};

#[derive(Debug, Serialize, Deserialize)]
struct Config { host: String, port: u16, debug: bool }

let config = Config { host: String::from("localhost"), port: 8080, debug: true };
let json = serde_json::to_string(&config).unwrap();
// {"host":"localhost","port":8080,"debug":true}
let parsed: Config = serde_json::from_str(&json).unwrap();

// Atributos para personalizar la serializacion:
// #[serde(rename = "statusCode")]           — otro nombre en JSON
// #[serde(skip_serializing_if = "Option::is_none")] — omitir si es None
// #[serde(default)]                         — usar Default si falta en el JSON
```

```rust
// thiserror — tipos de error con Display e Error automaticos.
use thiserror::Error;

#[derive(Debug, Error)]
enum AppError {
    #[error("file not found: {path}")]
    NotFound { path: String },

    #[error("permission denied")]
    PermissionDenied,

    #[error("IO error: {0}")]
    Io(#[from] std::io::Error),   // genera impl From<io::Error>
}
```

## cargo-expand para inspeccionar el codigo generado

`cargo expand` muestra el codigo que generan las macros.
Util para entender que hace derive internamente:

```bash
# Instalar (requiere nightly):
rustup toolchain install nightly
cargo install cargo-expand

# Ejecutar:
cargo +nightly expand          # expande todas las macros
cargo +nightly expand --lib    # solo la libreria
```

```rust
// Ejemplo: que genera #[derive(Debug)] para Point:
#[derive(Debug)]
struct Point { x: f64, y: f64 }

// cargo expand muestra algo como:
impl ::core::fmt::Debug for Point {
    fn fmt(&self, f: &mut ::core::fmt::Formatter) -> ::core::fmt::Result {
        ::core::fmt::Formatter::debug_struct(f, "Point")
            .field("x", &&self.x)
            .field("y", &&self.y)
            .finish()
    }
}

// #[derive(Clone)] para un struct con Vec genera:
// impl Clone for Buffer {
//     fn clone(&self) -> Buffer {
//         Buffer {
//             data: Clone::clone(&self.data),   // copia profunda
//             name: Clone::clone(&self.name),
//         }
//     }
// }
```

## Combinaciones comunes de derive

```rust
// La mas comun para tipos de datos generales:
#[derive(Debug, Clone, PartialEq)]
struct Record { id: u64, name: String, value: f64 }
// f64 impide derivar Eq, Ord y Hash.

// Tipos pequenos sin heap — se pueden hacer Copy:
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
struct Coord { x: i32, y: i32 }

// Tipos para configuracion — Default es clave:
#[derive(Debug, Clone, Default)]
struct AppConfig { host: String, port: u16, verbose: bool }
let config = AppConfig { port: 8080, ..AppConfig::default() };

// Enums simples sin datos (C-style):
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
enum Status { Active, Inactive, Pending }

// Con serializacion:
use serde::{Serialize, Deserialize};
#[derive(Debug, Clone, PartialEq, Serialize, Deserialize)]
struct ApiPayload { endpoint: String, method: String }
// El orden dentro de #[derive(...)] no importa.
```

## Derive y generics

Cuando el tipo tiene parametros genericos, derive agrega
automaticamente los bounds necesarios:

```rust
#[derive(Debug, Clone, PartialEq)]
struct Pair<T> { first: T, second: T }

// El compilador genera:
// impl<T: Debug> Debug for Pair<T> { ... }
// impl<T: Clone> Clone for Pair<T> { ... }
// impl<T: PartialEq> PartialEq for Pair<T> { ... }
//
// Pair<T> implementa Debug SOLO si T implementa Debug.
// Pair<i32>: ok (i32: Debug). Pair<Vec<u8>>: ok (Vec<u8>: Debug).

// Multiples parametros — cada uno recibe su bound:
#[derive(Debug, Clone)]
struct KeyValue<K, V> { key: K, value: V }
// impl<K: Debug, V: Debug> Debug for KeyValue<K, V> { ... }
```

```rust
// Caso especial con PhantomData: el bound se agrega aunque T no se use.
use std::marker::PhantomData;

#[derive(Debug)]
struct TypedId<T> { id: u64, _marker: PhantomData<T> }
// Genera: impl<T: Debug> Debug for TypedId<T> { ... }
// El bound T: Debug es innecesario (T no aparece en la salida).

// Implementacion manual para evitar bounds excesivos:
use std::fmt;

struct TypedId<T> { id: u64, _marker: PhantomData<T> }

impl<T> fmt::Debug for TypedId<T> {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("TypedId").field("id", &self.id).finish()
    }
}
// Ahora TypedId<T> implementa Debug para CUALQUIER T.
```

---

## Ejercicios

### Ejercicio 1 — Derive basico y limites

```rust
// Crear los siguientes tipos y determinar la combinacion
// maxima de traits derivables para cada uno.
// Intentar compilar con todos y corregir hasta que compile.
//
// 1. Point { x: f64, y: f64 }
//    Intentar: #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
//    Predecir cuales fallan y por que.
//
// 2. Label { id: u32, name: String }
//    Intentar: #[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
//    Predecir cual falla y por que.
//
// 3. Status enum con variantes Active, Inactive, Suspended (sin datos)
//    Intentar: #[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Default)]
//    Predecir si compila.
//
// Verificar las predicciones compilando y leer los errores.
```

### Ejercicio 2 — Derive vs implementacion manual

```rust
// Crear un struct:
// struct Employee { id: u64, name: String, department: String, internal_score: f64 }
//
// 1. Derivar Debug.
// 2. Implementar PartialEq manualmente para que compare solo id.
// 3. Implementar Display con formato: "Employee #42: Ana (Engineering)"
// 4. Escribir tests que verifiquen:
//    a. Dos Employee con mismo id pero distinto name son iguales (PartialEq).
//    b. El formato Display muestra lo esperado.
//    c. El formato Debug muestra TODOS los campos.
```

### Ejercicio 3 — Generics y derive

```rust
// 1. Crear: struct Wrapper<T> { value: T, label: String }
//    Derivar Debug, Clone, PartialEq.
//
// 2. Crear instancias con Wrapper<i32>, Wrapper<String>, Wrapper<Vec<u8>>.
//    Verificar que Debug, Clone y PartialEq funcionan.
//
// 3. Crear Wrapper<f64> y comparar con ==. Funciona?
//
// 4. Insertar Wrapper<f64> en un HashSet. Compila?
//    (Pista: HashSet requiere Hash y Eq.)
//
// 5. Derivar Copy en Wrapper<T>. Para que tipos de T funciona?
```
