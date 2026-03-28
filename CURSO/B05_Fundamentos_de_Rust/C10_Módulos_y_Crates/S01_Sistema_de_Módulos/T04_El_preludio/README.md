# El preludio: qué incluye y cómo crear uno propio

## Índice
1. [Qué es el preludio](#1-qué-es-el-preludio)
2. [Contenido del preludio estándar](#2-contenido-del-preludio-estándar)
3. [Evolución por ediciones](#3-evolución-por-ediciones)
4. [Preludes de la biblioteca estándar](#4-preludes-de-la-biblioteca-estándar)
5. [Preludes de crates externos](#5-preludes-de-crates-externos)
6. [Crear tu propio preludio](#6-crear-tu-propio-preludio)
7. [Cuándo crear un preludio y cuándo no](#7-cuándo-crear-un-preludio-y-cuándo-no)
8. [Errores comunes](#8-errores-comunes)
9. [Cheatsheet](#9-cheatsheet)
10. [Ejercicios](#10-ejercicios)

---

## 1. Qué es el preludio

El **preludio** (prelude) es un conjunto de items que Rust importa automáticamente en cada módulo de cada crate, sin necesidad de escribir `use`. Es la razón por la que puedes usar `Vec`, `String`, `Option`, `Result`, `println!`, `Some`, `None`, `Ok`, `Err` y muchos otros sin importar nada:

```rust
// No necesitas ningún use para esto:
fn main() {
    let numbers: Vec<i32> = vec![1, 2, 3];      // Vec, vec!
    let name: String = String::from("Alice");     // String
    let maybe: Option<i32> = Some(42);            // Option, Some
    let result: Result<i32, String> = Ok(10);     // Result, Ok
    let text: &str = "hello";                     // str primitivo
    let boxed: Box<i32> = Box::new(5);            // Box
    println!("{:?}", numbers);                    // println!, Debug
    let cloned = numbers.clone();                 // Clone
    let _default: Vec<i32> = Default::default();  // Default
    drop(name);                                   // drop
}
```

Todo esto funciona gracias a que el compilador inserta implícitamente al inicio de cada módulo:

```rust
// El compilador inserta esto automáticamente:
use std::prelude::v1::*;  // (edición 2015/2018)
// o
use std::prelude::rust_2021::*;  // (edición 2021)
```

---

## 2. Contenido del preludio estándar

El preludio de Rust es deliberadamente **pequeño y conservador**. Solo incluye items tan fundamentales que usarlos sin import sería tedioso. Aquí está el contenido completo:

### Tipos y traits del preludio (edición 2021)

```rust
// ====== Marcadores y conversiones ======
pub use std::marker::{Copy, Send, Sized, Sync, Unpin};
pub use std::ops::{Drop, Fn, FnMut, FnOnce};
pub use std::convert::{AsRef, AsMut, Into, From};
pub use std::convert::{TryFrom, TryInto};      // añadido en 2021

// ====== Memoria y ownership ======
pub use std::mem::drop;
pub use std::boxed::Box;
pub use std::borrow::ToOwned;
pub use std::clone::Clone;

// ====== Option y Result ======
pub use std::option::Option;
pub use std::option::Option::{Some, None};
pub use std::result::Result;
pub use std::result::Result::{Ok, Err};

// ====== Strings y colecciones ======
pub use std::string::{String, ToString};
pub use std::vec::Vec;

// ====== Iteradores ======
pub use std::iter::{
    Iterator,
    IntoIterator,
    DoubleEndedIterator,
    ExactSizeIterator,
    Extend,
};

// ====== Comparación y formato ======
pub use std::cmp::{PartialEq, Eq, PartialOrd, Ord};
pub use std::fmt::Debug;                        // conceptualmente
// (Debug no está técnicamente en el prelude, pero #[derive(Debug)] sí funciona)

// ====== Otros ======
pub use std::default::Default;

// ====== Macros (siempre disponibles, no via prelude sino por ser built-in) ======
// println!, eprintln!, format!, vec!, panic!, assert!, assert_eq!,
// todo!, unimplemented!, unreachable!, cfg!, include!, etc.
```

### Lo que NO está en el preludio

Muchos tipos comunes **no** están en el preludio:

```rust
// Todos estos requieren use explícito:
use std::collections::HashMap;     // no en prelude
use std::collections::HashSet;     // no en prelude
use std::io::{self, Read, Write};  // no en prelude
use std::fs::File;                 // no en prelude
use std::path::{Path, PathBuf};    // no en prelude
use std::sync::{Arc, Mutex};       // no en prelude
use std::cell::{Cell, RefCell};    // no en prelude
use std::rc::Rc;                   // no en prelude
use std::fmt::{self, Display};     // Display no está en prelude
```

Esto es intencional — el preludio solo incluye lo que *todo* programa Rust necesita. `HashMap` es común pero no universal; `Arc` solo se necesita en código concurrente.

---

## 3. Evolución por ediciones

Cada edición de Rust puede ampliar el preludio. Esto es posible sin romper código existente porque las ediciones son opt-in:

### Edición 2015 / 2018

```rust
use std::prelude::v1::*;
// Contenido base: Option, Result, Vec, String, Box, Clone,
// Copy, Send, Sync, Sized, Drop, Fn/FnMut/FnOnce,
// From, Into, AsRef, AsMut, Iterator, IntoIterator, etc.
```

### Edición 2021 (adiciones)

```rust
use std::prelude::rust_2021::*;
// Todo lo anterior MÁS:
// - TryFrom, TryInto (antes requerían use explícito)
// - FromIterator (antes requerían use explícito)
```

Antes de la edición 2021, necesitabas:

```rust
// Edición 2018:
use std::convert::TryFrom;
use std::convert::TryInto;

let x: i32 = 42u64.try_into().unwrap();  // necesitaba el use

// Edición 2021:
let x: i32 = 42u64.try_into().unwrap();  // funciona sin use
```

### Edición 2024 (adiciones)

```rust
use std::prelude::rust_2024::*;
// Todo lo anterior MÁS:
// - Future, IntoFuture (para hacer .await más ergonómico)
```

### Por qué las ediciones importan

Añadir items al preludio puede romper código existente por conflictos de nombres:

```rust
// Imagina que tu crate define su propio TryInto:
trait TryInto<T> {
    fn try_into(self) -> T;
}

// En edición 2018: sin conflicto (std::convert::TryInto no está en prelude)
// En edición 2021: CONFLICTO — ambos TryInto están en scope
```

Las ediciones permiten hacer este cambio de forma controlada. Al migrar de 2018 a 2021, `cargo fix --edition` resuelve estos conflictos automáticamente.

---

## 4. Preludes de la biblioteca estándar

Además del preludio principal, `std` tiene preludes especializados para submódulos específicos:

### std::io::prelude

```rust
use std::io::prelude::*;

// Importa:
// - Read
// - Write
// - BufRead
// - Seek

// Ahora puedes usar .read(), .write(), .lines(), .seek() sin imports adicionales
use std::fs::File;

let mut file = File::open("data.txt").unwrap();
let mut contents = String::new();
file.read_to_string(&mut contents).unwrap();  // Read está en scope
```

### std::net (uso implícito del prelude)

Los módulos de `std` no tienen preludes separados excepto `io`. Pero el patrón de crear un prelude es adoptado por muchos crates externos.

---

## 5. Preludes de crates externos

Muchos crates del ecosistema ofrecen un módulo `prelude` diseñado para glob import:

### Patrón estándar

```rust
// tokio
use tokio::prelude::*;

// serde
use serde::prelude::*;

// anyhow (no tiene prelude formal pero se usa así:)
use anyhow::{Result, Context, bail, ensure};

// diesel
use diesel::prelude::*;
// Importa: Queryable, Insertable, RunQueryDsl, ExpressionMethods, etc.
// Prácticamente necesario para que cualquier query compile

// bevy (game engine)
use bevy::prelude::*;
// Importa cientos de tipos — App, Commands, Query, Transform, etc.
```

### Cuándo usar el prelude de un crate

- **Sí**: cuando el crate está diseñado para usarse con prelude (bevy, diesel) y sus docs lo recomiendan.
- **Sí**: en archivos donde usas muchos tipos del crate.
- **No**: si solo usas uno o dos items — importa solo esos.
- **No**: si quieres claridad absoluta sobre de dónde viene cada tipo.

```rust
// Si solo usas HashMap de un crate grande:
use diesel::prelude::*;  // importa 50 items, solo necesitas 2 → excesivo

// Mejor:
use diesel::{Queryable, RunQueryDsl};
```

---

## 6. Crear tu propio preludio

Si tu crate tiene tipos que los usuarios necesitan en casi todos los archivos, un preludio puede simplificar su experiencia:

### Estructura básica

```rust
// src/lib.rs
pub mod models;
pub mod errors;
pub mod traits;

pub mod prelude {
    // Re-exportar los items más usados
    pub use crate::models::{User, Product, Order};
    pub use crate::errors::{AppError, Result};
    pub use crate::traits::{Validate, Serialize};
}
```

```rust
// Los usuarios de tu crate escriben:
use my_crate::prelude::*;

fn example() -> Result<User> {
    let user = User::new("Alice")?;
    user.validate()?;
    Ok(user)
}
```

### Ejemplo completo: framework web mini

```rust
// src/lib.rs
mod router;
mod request;
mod response;
mod middleware;
mod extractors;

pub use router::Router;
pub use request::Request;
pub use response::Response;

pub mod prelude {
    // Los tipos que todo handler necesita:
    pub use crate::request::Request;
    pub use crate::response::{Response, IntoResponse};
    pub use crate::router::{Router, Route};
    pub use crate::extractors::{Json, Query, Path};
    pub use crate::middleware::Middleware;

    // Re-exportar tipos comunes de dependencias:
    pub use http::StatusCode;
    pub use serde::{Serialize, Deserialize};

    // Type alias conveniente:
    pub type Result<T> = std::result::Result<T, crate::AppError>;
}
```

```rust
// Usuario del framework:
use my_framework::prelude::*;

#[derive(Deserialize)]
struct CreateUser {
    name: String,
    email: String,
}

async fn create_user(Json(body): Json<CreateUser>) -> Result<Response> {
    // Request, Response, Json, Result, Deserialize — todo del prelude
    Ok(Response::json(&body).status(StatusCode::CREATED))
}
```

### Preludio interno (para tu propio crate)

También puedes crear un preludio para uso interno:

```rust
// src/prelude.rs (no pub — solo para uso interno)
pub(crate) use crate::config::Config;
pub(crate) use crate::db::Connection;
pub(crate) use crate::errors::{AppError, Result};
pub(crate) use crate::models::{User, Product};
pub(crate) use tracing::{info, warn, error, debug};
```

```rust
// src/handlers/users.rs
use crate::prelude::*;  // todo lo necesario en una línea

pub async fn list_users(conn: &Connection, config: &Config) -> Result<Vec<User>> {
    info!("Listing users");
    // Config, Connection, Result, User, info! — todo del prelude interno
    todo!()
}
```

---

## 7. Cuándo crear un preludio y cuándo no

### Sí, crea un preludio cuando:

1. **Tu crate tiene 5+ items que se usan en casi todos los archivos del usuario.**
   - Frameworks (web, game, UI) son candidatos naturales.
   - Un crate con 2-3 tipos principales no necesita prelude.

2. **Los usuarios típicamente necesitan traits para que los métodos funcionen.**
   - Diesel sin su prelude es prácticamente inutilizable.
   - Si tus traits añaden métodos esenciales, ponlos en el prelude.

3. **Quieres re-exportar dependencias para conveniencia.**
   - `pub use serde::{Serialize, Deserialize}` en el prelude evita que el usuario añada serde a su Cargo.toml.

### No crees un preludio cuando:

1. **Tu crate tiene pocos items públicos.**
   - Si el usuario solo necesita `use my_crate::Client`, un prelude es overhead.

2. **Los items son independientes — el usuario rara vez necesita todos.**
   - Una colección de utilidades sueltas no se beneficia de un prelude.

3. **La claridad de origen importa.**
   - En código de producción crítico, saber exactamente de dónde viene cada tipo es valioso.

### Reglas para un buen preludio

```rust
pub mod prelude {
    // ✓ INCLUIR:
    // - Tipos que el usuario construye o recibe en casi toda interacción
    // - Traits necesarios para que los métodos del crate funcionen
    // - Type aliases convenientes (Result, Error)
    // - Re-exports de dependencias que el usuario siempre necesita

    // ✗ NO INCLUIR:
    // - Tipos internos o de implementación
    // - Items que solo se usan en configuración inicial
    // - Todo el crate (eso anula el propósito)
    // - Tipos que podrían conflictar con std (cuidado con Result, Error)
}
```

---

## 8. Errores comunes

### Error 1: Asumir que HashMap está en el preludio

```rust
fn main() {
    // ERROR: cannot find type `HashMap` in this scope
    let mut map = HashMap::new();
}
```

`HashMap` es uno de los tipos más usados que **no** está en el preludio. Siempre requiere import:

```rust
use std::collections::HashMap;
```

Otros tipos que la gente asume (incorrectamente) que están en el preludio: `HashSet`, `BTreeMap`, `Rc`, `Arc`, `Mutex`, `RefCell`, `Cell`, `File`, `Path`, `Display`.

### Error 2: Crear un preludio que conflicta con std

```rust
pub mod prelude {
    // PELIGROSO: sombrea std::result::Result
    pub type Result<T> = std::result::Result<T, crate::Error>;

    // PELIGROSO: sombrea std::option::Option variants
    // pub enum Option { ... }  // nunca hagas esto
}
```

Sombrear `Result` es un patrón **aceptado** (anyhow, thiserror, muchos frameworks lo hacen) porque el type alias sigue siendo un `Result` — solo fija el tipo de error. Pero sombrear `Option`, `Vec`, o `String` sería confuso y está desaconsejado.

```rust
// OK: type alias de Result (patrón estándar)
pub type Result<T> = std::result::Result<T, MyError>;

// OK en el preludio — el usuario puede escribir Result<T> en vez de Result<T, MyError>
// Si necesita el Result original de std: std::result::Result<T, E>
```

### Error 3: Preludio demasiado grande

```rust
pub mod prelude {
    pub use crate::*;  // re-exporta ABSOLUTAMENTE todo → derrota el propósito
}
```

Un preludio debe ser **curado** — solo los items esenciales. Si exportas todo, el usuario pierde la capacidad de saber de dónde viene cada cosa, y cualquier adición al crate puede romper su código por conflictos.

### Error 4: No documentar qué incluye el preludio

```rust
/// The prelude re-exports commonly used items.
///
/// # Contents
/// - [`User`] — the main user type
/// - [`AppError`] — the error type
/// - [`Result`] — type alias for `Result<T, AppError>`
/// - [`Validate`] — trait for validation
pub mod prelude {
    pub use crate::models::User;
    pub use crate::errors::{AppError, Result};
    pub use crate::traits::Validate;
}
```

Siempre documenta tu preludio — los usuarios necesitan saber qué entra en su scope cuando escriben `use my_crate::prelude::*`.

### Error 5: Confundir el preludio con lo que está "built-in"

```rust
// Las macros NO vienen del prelude — son built-in del compilador:
println!("hello");    // macro del compilador, no del prelude
vec![1, 2, 3];        // macro del compilador
format!("{}", x);     // macro del compilador
assert_eq!(a, b);     // macro del compilador

// Los tipos primitivos NO vienen del prelude — son parte del lenguaje:
let x: i32 = 42;      // primitivo del lenguaje
let s: &str = "hello"; // primitivo del lenguaje
let b: bool = true;    // primitivo del lenguaje
let a: [i32; 3] = [1, 2, 3]; // primitivo del lenguaje

// El preludio importa tipos de la BIBLIOTECA ESTÁNDAR:
// Vec, String, Box, Option, Result, Clone, Copy, etc.
// Estos son structs/enums/traits definidos en std, no primitivos.
```

---

## 9. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                      EL PRELUDIO CHEATSHEET                        ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  QUÉ ES                                                            ║
║  Items importados automáticamente en cada módulo de cada crate     ║
║  Equivale a: use std::prelude::rust_2021::*;                       ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  EN EL PRELUDIO (disponible sin use)                               ║
║  Tipos:   Option, Some, None, Result, Ok, Err                     ║
║           Vec, String, Box                                         ║
║  Traits:  Clone, Copy, Debug (derive), Default                     ║
║           PartialEq, Eq, PartialOrd, Ord                          ║
║           Iterator, IntoIterator, ExactSizeIterator                ║
║           From, Into, TryFrom, TryInto (2021+)                     ║
║           AsRef, AsMut, ToOwned, ToString                          ║
║           Drop, Fn, FnMut, FnOnce                                 ║
║           Send, Sync, Sized, Unpin, Copy                           ║
║  Funciones: drop()                                                 ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  NO EN EL PRELUDIO (requiere use)                                  ║
║  HashMap, HashSet, BTreeMap        std::collections::              ║
║  File, OpenOptions                 std::fs::                       ║
║  Path, PathBuf                     std::path::                     ║
║  Read, Write, BufRead              std::io::                       ║
║  Rc, Arc, Weak                     std::rc:: / std::sync::         ║
║  Cell, RefCell                     std::cell::                     ║
║  Mutex, RwLock                     std::sync::                     ║
║  Display, Formatter                std::fmt::                      ║
║  Pin, PhantomPinned                std::pin:: / std::marker::      ║
║  Cow                               std::borrow::                   ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  PRELUDES DE CRATES                                                ║
║  use mi_crate::prelude::*;        Patrón estándar                  ║
║  use std::io::prelude::*;         Read, Write, BufRead, Seek       ║
║  use diesel::prelude::*;          Prácticamente obligatorio        ║
║  use bevy::prelude::*;            Importa cientos de items         ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CREAR TU PROPIO PRELUDIO                                          ║
║  pub mod prelude {                                                 ║
║      pub use crate::TiposClave;                                    ║
║      pub use crate::TraitsEsenciales;                              ║
║      pub type Result<T> = std::result::Result<T, crate::Error>;    ║
║      // Solo items que el usuario necesita en CASI TODO archivo    ║
║  }                                                                 ║
║                                                                    ║
║  ✓ Curado (5-20 items)                                             ║
║  ✓ Documentado                                                     ║
║  ✗ No re-exportar todo el crate                                    ║
║  ✗ No sombrear Option, Vec, String                                 ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 10. Ejercicios

### Ejercicio 1: ¿Preludio o no?

Para cada tipo/trait, indica si está en el preludio estándar (disponible sin `use`) o no. Verifica compilando un programa vacío que use cada uno sin imports:

```rust
fn main() {
    // ¿Cuáles compilan sin ningún use?

    let a: Vec<i32> = Vec::new();                  // 1.
    let b: Option<String> = None;                  // 2.
    let c: HashMap<String, i32> = HashMap::new();  // 3.
    let d: Box<dyn std::fmt::Display> = Box::new(42); // 4.
    let e: Result<i32, String> = Ok(42);           // 5.
    let f = String::from("hello");                 // 6.
    let g: std::rc::Rc<i32> = std::rc::Rc::new(1); // 7. (path completo)
    let h = vec![1, 2, 3];                         // 8.
    let i: &dyn Iterator<Item = i32>;              // 9.
    let j: std::path::PathBuf;                     // 10.
    let k = 42_i32.clone();                        // 11. (.clone() método)
    let l: i32 = 42_u8.into();                     // 12. (.into() método)
    let m: i32 = i32::try_from(42_u64).unwrap();   // 13. (TryFrom, ed. 2021)
    drop(k);                                       // 14.
    let n = [1, 2, 3].iter().map(|x| x + 1);      // 15. (.iter().map())
    println!("{}", l);                             // 16. (println!)
}
```

**Preguntas:**
1. ¿Cuáles son built-in del lenguaje vs del preludio vs de std?
2. ¿El item 7 funciona sin `use`? ¿Por qué?
3. ¿El item 13 funcionaría en edición 2018?

### Ejercicio 2: Crear un preludio para una library

Tienes una library crate para gestión de tareas con esta estructura:

```rust
// src/lib.rs
pub mod models {
    pub struct Task {
        pub title: String,
        pub done: bool,
        pub(crate) id: u64,
    }

    pub struct Project {
        pub name: String,
        pub tasks: Vec<Task>,
    }

    pub enum Priority {
        Low,
        Medium,
        High,
        Critical,
    }
}

pub mod errors {
    #[derive(Debug)]
    pub enum TaskError {
        NotFound(u64),
        InvalidTitle(String),
        StorageError(String),
    }

    impl std::fmt::Display for TaskError {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            match self {
                Self::NotFound(id) => write!(f, "Task {} not found", id),
                Self::InvalidTitle(t) => write!(f, "Invalid title: {}", t),
                Self::StorageError(e) => write!(f, "Storage error: {}", e),
            }
        }
    }

    impl std::error::Error for TaskError {}
}

pub mod traits {
    use crate::errors::TaskError;

    pub trait Storable {
        fn save(&self) -> Result<(), TaskError>;
        fn load(id: u64) -> Result<Self, TaskError> where Self: Sized;
    }

    pub trait Filterable {
        fn matches(&self, query: &str) -> bool;
    }
}

pub mod storage {
    // Detalle de implementación
    pub(crate) struct FileBackend;
    pub struct MemoryBackend;

    impl MemoryBackend {
        pub fn new() -> Self { MemoryBackend }
    }
}
```

**Tareas:**
1. Crea un módulo `prelude` en `lib.rs` con los items apropiados.
2. Justifica cada inclusión y cada exclusión.
3. ¿Incluirías un `type Result<T> = std::result::Result<T, TaskError>`?
4. ¿Incluirías `MemoryBackend`? ¿`FileBackend`?
5. ¿Incluirías `Priority::*` (las variantes)?

### Ejercicio 3: Depurar conflictos de preludio

El siguiente código falla al migrar de edición 2018 a 2021. Identifica el problema y propón dos soluciones distintas:

```rust
// edition = "2021" en Cargo.toml

trait TryInto<T> {
    fn try_into(self) -> Result<T, String>;
}

impl TryInto<u8> for i32 {
    fn try_into(self) -> Result<u8, String> {
        if self >= 0 && self <= 255 {
            Ok(self as u8)
        } else {
            Err(format!("{} out of range for u8", self))
        }
    }
}

fn main() {
    let x: i32 = 42;
    // ERROR: multiple applicable items in scope
    let y: Result<u8, String> = x.try_into();
    println!("{:?}", y);
}
```

**Preguntas:**
1. ¿Por qué compila en edición 2018 pero no en 2021?
2. Solución A: ¿cómo desambiguas la llamada para usar *tu* `TryInto`?
3. Solución B: ¿cómo renombras tu trait para evitar el conflicto?
4. ¿`cargo fix --edition` resolvería esto automáticamente? ¿Cómo?

---

> **Nota**: el preludio es un mecanismo de ergonomía, no de magia. Saber exactamente qué incluye te evita sorpresas y te permite razonar sobre de dónde vienen los tipos y traits que usas. Cuando encuentres un método que "aparece de la nada" (como `.clone()`, `.into()`, `.iter()`), probablemente viene de un trait del preludio.
