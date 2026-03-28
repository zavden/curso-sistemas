# use y re-exports: importar y re-exportar items

## Índice
1. [Qué hace use](#1-qué-hace-use)
2. [Rutas en use](#2-rutas-en-use)
3. [use con as: renombrar imports](#3-use-con-as-renombrar-imports)
4. [Nested paths con llaves](#4-nested-paths-con-llaves)
5. [Glob imports con *](#5-glob-imports-con-)
6. [pub use: re-exportar](#6-pub-use-re-exportar)
7. [Convenciones idiomáticas](#7-convenciones-idiomáticas)
8. [use en distintos contextos](#8-use-en-distintos-contextos)
9. [Imports de crates externos](#9-imports-de-crates-externos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué hace use

`use` trae un **path** al scope actual para poder usarlo sin calificación completa. No copia ni mueve nada — solo crea un atajo de nombre:

```rust
mod network {
    pub mod http {
        pub fn get(url: &str) -> String {
            format!("GET {}", url)
        }

        pub fn post(url: &str, body: &str) -> String {
            format!("POST {} body={}", url, body)
        }
    }
}

// Sin use:
fn without_use() {
    let r = network::http::get("https://example.com");
    let p = network::http::post("https://example.com", "data");
}

// Con use:
use network::http;

fn with_use() {
    let r = http::get("https://example.com");
    let p = http::post("https://example.com", "data");
}

// O importando las funciones directamente:
use network::http::{get, post};

fn with_direct_use() {
    let r = get("https://example.com");
    let p = post("https://example.com", "data");
}
```

### use no cambia visibilidad

`use` solo crea un alias local. No hace público algo que es privado:

```rust
mod inner {
    pub fn public_fn() {}
    fn private_fn() {}
}

use inner::public_fn;   // OK
// use inner::private_fn;  // ERROR: private_fn no es visible aquí
```

---

## 2. Rutas en use

Las declaraciones `use` aceptan las mismas rutas que el resto del código:

### Rutas absolutas con crate::

```rust
mod services {
    pub mod auth {
        pub fn login() -> String {
            String::from("logged in")
        }
    }
}

// Desde cualquier lugar del crate:
use crate::services::auth::login;

fn main() {
    println!("{}", login());
}
```

### Rutas con self::

```rust
mod parent {
    pub fn helper() -> i32 { 42 }

    pub mod child {
        // self:: se refiere al módulo actual (child)
        use self::inner::secret_value;
        // Equivalente a: use crate::parent::child::inner::secret_value;

        mod inner {
            pub(super) fn secret_value() -> i32 { 99 }
        }

        pub fn work() -> i32 {
            secret_value()
        }
    }
}
```

### Rutas con super::

```rust
mod utils {
    pub fn format_name(name: &str) -> String {
        format!("Mr./Ms. {}", name)
    }
}

mod services {
    // super:: sube al módulo padre (la raíz en este caso)
    use super::utils::format_name;

    pub fn greet(name: &str) -> String {
        format!("Hello, {}!", format_name(name))
    }
}
```

### Rutas a crates externos

Para crates externos (declarados en `Cargo.toml`), la ruta empieza con el nombre del crate:

```rust
// Cargo.toml:
// [dependencies]
// serde = { version = "1", features = ["derive"] }
// rand = "0.8"

use serde::{Serialize, Deserialize};
use rand::Rng;

#[derive(Serialize, Deserialize)]
struct Config {
    name: String,
}

fn random_number() -> u32 {
    let mut rng = rand::thread_rng();
    rng.gen_range(1..=100)
}
```

### std también es un crate

La biblioteca estándar se importa igual:

```rust
use std::collections::HashMap;
use std::io::{self, Read, Write};
use std::fs::File;
use std::path::PathBuf;
```

---

## 3. use con as: renombrar imports

`as` permite renombrar un import para evitar conflictos o mejorar claridad:

### Resolver conflictos de nombres

```rust
use std::fmt::Result as FmtResult;
use std::io::Result as IoResult;

fn format_something() -> FmtResult {
    Ok(())
}

fn read_something() -> IoResult<String> {
    Ok(String::new())
}
```

### Nombres más claros en contexto

```rust
use std::collections::HashMap as Map;
use std::collections::BTreeMap as OrderedMap;

fn example() {
    let fast: Map<String, i32> = Map::new();
    let sorted: OrderedMap<String, i32> = OrderedMap::new();
}
```

### Renombrar crates externos

```rust
// En Cargo.toml también puedes renombrar:
// [dependencies]
// serde_json_v2 = { package = "serde_json", version = "1" }

// O con use:
use serde_json as json;

fn parse(data: &str) -> json::Value {
    json::from_str(data).unwrap()
}
```

### Convención: cuándo usar as

- **Sí**: cuando dos tipos del mismo nombre colisionan (`fmt::Result` vs `io::Result`).
- **Sí**: cuando el nombre original es muy largo o poco claro en contexto.
- **No**: para renombrar tipos estándar bien conocidos sin razón (`use String as Str` confunde).
- **Alternativa**: en lugar de `as`, importar el módulo padre y calificar:

```rust
// En vez de:
use std::fmt::Result as FmtResult;
use std::io::Result as IoResult;

// Puedes hacer:
use std::fmt;
use std::io;

fn format_something() -> fmt::Result { Ok(()) }
fn read_something() -> io::Result<String> { Ok(String::new()) }
```

Esta alternativa (importar el módulo, no el tipo) es generalmente preferida en la comunidad Rust.

---

## 4. Nested paths con llaves

Las llaves `{}` permiten agrupar múltiples imports del mismo path en una sola declaración:

### Sintaxis básica

```rust
// Sin agrupar:
use std::collections::HashMap;
use std::collections::BTreeMap;
use std::collections::HashSet;

// Agrupado:
use std::collections::{HashMap, BTreeMap, HashSet};
```

### Agrupar con self

Cuando necesitas importar tanto el módulo como items dentro de él:

```rust
// Sin agrupar:
use std::io;
use std::io::Read;
use std::io::Write;

// Agrupado con self:
use std::io::{self, Read, Write};
// self se refiere a std::io mismo
```

### Nested paths profundos

```rust
// Múltiples niveles:
use std::{
    collections::{HashMap, HashSet},
    io::{self, Read, Write, BufReader},
    fs::File,
    path::{Path, PathBuf},
};
```

### Formato multilínea

Para imports largos, la convención es un item por línea:

```rust
use std::collections::{
    BTreeMap,
    BTreeSet,
    HashMap,
    HashSet,
    VecDeque,
};
```

`rustfmt` (el formateador estándar) ordena alfabéticamente y aplica formato consistente a los `use` agrupados.

---

## 5. Glob imports con *

El operador `*` importa **todos los items públicos** de un módulo:

```rust
use std::collections::*;

fn example() {
    let mut map = HashMap::new();
    let mut set = HashSet::new();
    let mut deque = VecDeque::new();
    // Todos disponibles sin calificar
}
```

### Cuándo es apropiado usar *

En general, los glob imports se **desaconsejan** en código de producción por varias razones:

```rust
// PROBLEMA 1: ¿de dónde viene cada nombre?
use crate::models::*;
use crate::utils::*;

fn example() {
    let user = User::new("Alice");  // ¿de models o de utils?
    format_name("Bob");             // ¿de dónde viene?
}

// PROBLEMA 2: imports futuros pueden romper tu código
// Si models añade un format_name() y utils ya tiene uno → conflicto

// PROBLEMA 3: dificulta las herramientas de análisis
// Los IDEs y rust-analyzer trabajan mejor con imports explícitos
```

### Excepciones aceptadas

Hay contextos donde `*` es idiomático:

```rust
// 1. En tests: importar todo del módulo bajo test
#[cfg(test)]
mod tests {
    use super::*;  // idiomático: trae todo el módulo padre

    #[test]
    fn test_something() {
        // Acceso directo a todas las funciones del módulo
    }
}

// 2. Preludes diseñados para glob import
use my_framework::prelude::*;
// Los prelude están diseñados expresamente para esto

// 3. Enum variants en un match local
use Color::*;
match pixel {
    Red => {},
    Green => {},
    Blue => {},
    Custom(r, g, b) => {},
}
```

---

## 6. pub use: re-exportar

`pub use` es una de las herramientas más poderosas del sistema de módulos. **Re-exporta** un item haciendo que sea accesible como si perteneciera al módulo actual:

### Concepto básico

```rust
// src/lib.rs
mod internal {
    pub struct Config {
        pub name: String,
    }

    pub fn load() -> Config {
        Config { name: String::from("default") }
    }
}

// Sin re-export, los usuarios acceden así:
//   mi_crate::internal::Config  ← expone la estructura interna

// Con re-export:
pub use internal::Config;
pub use internal::load;

// Ahora los usuarios acceden así:
//   mi_crate::Config  ← limpio, sin conocer "internal"
//   mi_crate::load
```

### Reestructurar sin romper la API

`pub use` permite cambiar la organización interna sin afectar a los usuarios:

```rust
// Versión 1: todo en un módulo
// src/lib.rs
pub mod models {
    pub struct User { ... }
    pub struct Product { ... }
}
// Usuarios: mi_crate::models::User

// Versión 2: separamos internamente, pero mantenemos la API
// src/lib.rs
mod user;     // src/user.rs
mod product;  // src/product.rs

pub mod models {
    pub use crate::user::User;
    pub use crate::product::Product;
}
// Usuarios: mi_crate::models::User  ← SIN CAMBIOS
```

### Re-exportar crates externos

Un patrón muy común en libraries es re-exportar tipos de dependencias:

```rust
// src/lib.rs
pub use serde;           // re-exporta todo el crate serde
pub use serde_json::Value;  // re-exporta solo Value

// Los usuarios pueden hacer:
//   use mi_crate::serde::Serialize;
//   use mi_crate::Value;
// Sin necesidad de añadir serde/serde_json a su Cargo.toml
```

### Facade pattern con pub use

Crear una API pública limpia que oculte la complejidad interna:

```rust
// Estructura interna compleja:
// src/lib.rs
mod parsing;
mod evaluation;
mod types;
mod errors;

// API pública simple (facade):
pub use types::{Expression, Value};
pub use errors::CalcError;
pub use parsing::parse;
pub use evaluation::evaluate;

// Los usuarios ven:
//   mi_crate::Expression
//   mi_crate::Value
//   mi_crate::CalcError
//   mi_crate::parse
//   mi_crate::evaluate
// No ven los módulos internos ni su organización
```

### pub use con renombrado

```rust
mod v2 {
    pub struct Client {
        // nueva implementación
    }
}

// Re-exportar con nombre distinto:
pub use v2::Client as HttpClient;

// Los usuarios usan: mi_crate::HttpClient
// Sin saber que internamente es v2::Client
```

---

## 7. Convenciones idiomáticas

### Importar funciones: calificar con el módulo padre

```rust
// IDIOMÁTICO: importar el módulo, calificar la función
use std::io;

fn read_file() -> io::Result<String> {
    io::read_to_string(io::stdin())
}

// MENOS IDIOMÁTICO: importar la función directamente
use std::io::read_to_string;
use std::io::stdin;

fn read_file2() -> std::io::Result<String> {
    read_to_string(stdin())
}
```

**Por qué**: al ver `io::read_to_string`, queda claro que es de `std::io`. Si solo ves `read_to_string`, no sabes de dónde viene.

**Excepción**: funciones que son el punto central del módulo y se usan repetidamente:

```rust
// OK — thread::spawn es tan ubicuo que importar spawn directamente es aceptable
use std::thread::spawn;
```

### Importar structs, enums, traits: directamente

```rust
// IDIOMÁTICO: importar tipos directamente
use std::collections::HashMap;
use std::io::Read;

let map: HashMap<String, i32> = HashMap::new();

// MENOS IDIOMÁTICO para tipos: calificar con módulo
let map: collections::HashMap<String, i32> = collections::HashMap::new();
```

**Por qué**: los tipos se usan como anotaciones de tipo y en patterns — calificarlos es verboso.

### Ordenar use declarations

`rustfmt` aplica este orden automáticamente:

```rust
// 1. std / core / alloc
use std::collections::HashMap;
use std::io;

// 2. Crates externos (en orden alfabético)
use serde::{Serialize, Deserialize};
use tokio::runtime::Runtime;

// 3. Crate actual
use crate::models::User;
use crate::utils::format;

// 4. super / self (modules del padre o actual)
use super::config::Config;
```

Cada grupo separado por una línea en blanco.

### use dentro de funciones

`use` puede aparecer dentro de funciones para limitar el scope:

```rust
fn process_data() {
    use std::collections::HashMap;
    // HashMap solo existe dentro de esta función

    let mut map = HashMap::new();
    map.insert("key", "value");
}

// HashMap no está disponible aquí
```

Esto es útil para imports que solo se necesitan en un lugar específico o para variants de enum:

```rust
fn describe_color(c: Color) -> &'static str {
    use Color::*;  // solo dentro de esta función
    match c {
        Red => "red",
        Green => "green",
        Blue => "blue",
    }
}
```

---

## 8. use en distintos contextos

### use en módulos inline

```rust
mod processing {
    use std::collections::HashMap;  // disponible dentro de processing

    pub fn analyze(data: &[&str]) -> HashMap<&str, usize> {
        let mut counts = HashMap::new();
        for item in data {
            *counts.entry(*item).or_insert(0) += 1;
        }
        counts
    }
}

// HashMap NO está disponible aquí (salvo que lo importemos aparte)
```

### use en archivos de módulo

```rust
// src/services.rs
use crate::models::User;         // import del propio crate
use crate::database::Connection;
use std::collections::HashMap;    // import de std
use serde::Serialize;             // import de crate externo

pub fn list_users(conn: &Connection) -> Vec<User> {
    // User, Connection, HashMap, Serialize disponibles
    todo!()
}
```

### use en impl blocks

`use` no puede ir directamente dentro de un `impl`, pero sí dentro de los métodos:

```rust
struct Processor;

impl Processor {
    // use aquí es ERROR
    // use std::collections::HashMap;

    fn work(&self) {
        // use aquí es OK
        use std::collections::HashMap;
        let map = HashMap::<String, i32>::new();
    }
}
```

---

## 9. Imports de crates externos

### Edición 2015 vs 2018+

En la edición 2015, necesitabas `extern crate` para usar dependencias:

```rust
// Edición 2015 (obsoleto):
extern crate serde;
extern crate rand;

use serde::Serialize;
use rand::Rng;
```

Desde la edición 2018, `extern crate` es innecesario — Cargo lo maneja automáticamente:

```rust
// Edición 2018+ (actual):
use serde::Serialize;
use rand::Rng;
// Simplemente funciona si está en Cargo.toml
```

**Excepción**: `extern crate` sigue siendo necesario para macros de ciertas versiones antiguas y para crates que exportan macros con `#[macro_export]` en edición 2015:

```rust
// Raro pero existe: algunos crates legacy requieren esto
#[macro_use]
extern crate lazy_static;
// En edición 2018+, preferir: use lazy_static::lazy_static;
```

### std vs core vs alloc

```rust
// std: biblioteca estándar completa (incluye I/O, threads, networking)
use std::collections::HashMap;

// core: subconjunto sin heap ni OS (para embedded / no_std)
use core::fmt;
use core::cell::Cell;

// alloc: heap allocation sin OS completo (Vec, String, Box sin std)
use alloc::vec::Vec;
use alloc::string::String;
```

Para la mayoría de programas, `std` es suficiente. `core` y `alloc` son para desarrollo `#![no_std]` (embedded, kernels, WebAssembly minimalista).

---

## 10. Errores comunes

### Error 1: Importar un trait sin usarlo (y no poder llamar sus métodos)

```rust
use std::fs::File;
// Olvidamos: use std::io::Read;

fn main() {
    let mut file = File::open("test.txt").unwrap();
    let mut buf = String::new();

    // ERROR: method `read_to_string` not found
    // El método existe (File implementa Read), pero Read no está en scope
    file.read_to_string(&mut buf).unwrap();
}
```

**Solución**: importar el trait:

```rust
use std::fs::File;
use std::io::Read;  // ahora .read_to_string() funciona

// O importar todo el prelude de io:
use std::io::prelude::*;
```

El error del compilador generalmente sugiere: `help: items from traits can only be used if the trait is in scope; use std::io::Read`.

### Error 2: Conflicto de nombres sin resolver

```rust
use std::fmt::Result;
use std::io::Result;  // ERROR: `Result` already defined

// Solución A: renombrar con as
use std::fmt::Result as FmtResult;
use std::io::Result as IoResult;

// Solución B: importar módulos y calificar (preferida)
use std::fmt;
use std::io;

fn format() -> fmt::Result { Ok(()) }
fn read() -> io::Result<()> { Ok(()) }
```

### Error 3: use de un item privado

```rust
mod inner {
    fn secret() {}
    pub fn public() {}
}

use inner::secret;  // ERROR: `secret` is private
use inner::public;  // OK
```

**Trampas sutiles**:

```rust
mod inner {
    pub mod deep {
        pub fn func() {}
    }
}

// ERROR si inner no es pub y estamos fuera:
// El módulo intermedio debe ser visible
use inner::deep::func;  // OK aquí porque estamos en el mismo archivo

// Pero desde otro módulo:
mod other {
    // use crate::inner::deep::func;  // ERROR si inner no es pub
    // Aunque deep y func son pub, inner bloquea el acceso
}
```

### Error 4: use duplicado o redundante

```rust
use std::collections::HashMap;
use std::collections::HashMap;  // WARNING: unused import (el segundo)

// rustfmt y clippy detectan esto
// clippy: warning: the item `HashMap` is imported redundantly
```

### Error 5: Confundir use con mod

```rust
// Esto DECLARA un módulo (busca el archivo):
mod utils;

// Esto IMPORTA un item ya existente:
use crate::utils::helper;

// ERROR COMÚN: intentar "importar" un archivo con use:
// use utils;  // ERROR si no hiciste `mod utils;` antes
// use necesita que el módulo ya esté declarado
```

El flujo correcto es:
1. `mod nombre;` — declara el módulo (vincula el archivo).
2. `use nombre::item;` — importa items del módulo declarado.

---

## 11. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                    use Y RE-EXPORTS CHEATSHEET                     ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  SINTAXIS DE use                                                   ║
║  use path::item;                 Importar un item                  ║
║  use path::{A, B, C};           Importar varios del mismo path     ║
║  use path::{self, A, B};        Importar módulo + items            ║
║  use path::*;                   Glob: importar todo lo público     ║
║  use path::item as alias;       Renombrar al importar              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  TIPOS DE RUTA                                                     ║
║  use crate::a::b;               Absoluta (raíz del crate)         ║
║  use self::a::b;                Relativa (módulo actual)           ║
║  use super::a::b;               Relativa (módulo padre)            ║
║  use std::collections::HashMap; Crate externo (std)                ║
║  use serde::Serialize;          Crate externo (dependencia)        ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  RE-EXPORTS                                                        ║
║  pub use path::Item;             Re-exporta: visible externamente  ║
║  pub use path::Item as Alias;    Re-exporta con otro nombre        ║
║  pub use path::*;                Re-exporta todo (raro, preludes)  ║
║                                                                    ║
║  use (sin pub) → alias privado, solo en este módulo                ║
║  pub use       → alias público, accesible desde fuera              ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CONVENCIONES                                                      ║
║  Funciones   → importar módulo padre: use std::io; → io::read()   ║
║  Tipos       → importar directamente: use std::..::HashMap;       ║
║  Traits      → importar directamente (necesario para métodos)      ║
║  Conflictos  → usar as o calificar con módulo                      ║
║  Tests       → use super::*; es idiomático                         ║
║  Glob (*)    → evitar excepto tests y preludes                     ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ORDEN (rustfmt)                                                   ║
║  1. std / core / alloc                                             ║
║  2. Crates externos (alfabético)                                   ║
║  3. crate:: (módulos propios)                                      ║
║  4. super:: / self::                                               ║
║  Línea en blanco entre grupos                                      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  mod vs use                                                        ║
║  mod nombre;     DECLARA módulo (vincula archivo)                  ║
║  use nombre::x;  IMPORTA item de módulo ya declarado               ║
║  Primero mod, luego use — sin mod, use no funciona                 ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Simplificar imports

Reescribe estos imports usando nested paths, `self`, y `as` donde sea apropiado. El resultado debe ser equivalente pero más conciso:

```rust
use std::collections::HashMap;
use std::collections::HashSet;
use std::collections::BTreeMap;
use std::io;
use std::io::Read;
use std::io::Write;
use std::io::BufRead;
use std::fmt::Display;
use std::fmt::Debug;
use std::fmt::Formatter;
use std::fmt::Result as FmtResult;
use std::fs::File;
use std::fs::OpenOptions;
use std::path::Path;
use std::path::PathBuf;
```

**Preguntas:**
1. ¿Puedes reducirlo a 4 declaraciones `use` o menos?
2. ¿Cómo manejas `std::fmt::Result` sin conflicto con `std::io::Result`?
3. ¿Tiene sentido usar `use std::*` aquí? ¿Por qué no?

### Ejercicio 2: Diseñar una API con pub use

Tienes esta estructura interna de una library crate:

```text
src/
├── lib.rs
├── parsing/
│   ├── mod.rs
│   ├── lexer.rs      → pub struct Token, pub fn tokenize(&str) -> Vec<Token>
│   └── parser.rs     → pub struct Ast, pub fn parse(Vec<Token>) -> Ast
├── evaluation/
│   ├── mod.rs
│   └── evaluator.rs  → pub fn evaluate(&Ast) -> Result<Value, EvalError>
├── types/
│   ├── mod.rs
│   ├── value.rs      → pub enum Value { Int(i64), Float(f64), Str(String) }
│   └── error.rs      → pub struct EvalError { pub message: String }
```

Los usuarios de tu crate deberían poder escribir:

```rust
use my_calc::{tokenize, parse, evaluate, Value, EvalError};

fn main() {
    let tokens = tokenize("2 + 3 * 4");
    let ast = parse(tokens);
    match evaluate(&ast) {
        Ok(Value::Int(n)) => println!("= {}", n),
        Err(e) => println!("Error: {}", e.message),
        _ => {}
    }
}
```

Escribe el contenido de `lib.rs` con los `pub use` necesarios. Los usuarios **no** deben necesitar conocer los módulos `parsing`, `evaluation`, ni `types`.

**Pregunta adicional**: ¿deberían los usuarios poder acceder a `Token` y `Ast`? Justifica tu decisión.

### Ejercicio 3: Tracing de imports

Para cada `use` en el siguiente código, indica:
- La ruta completa del item importado
- Si compila o no, y por qué
- Si es idiomático o si lo cambiarías

```rust
// src/lib.rs
mod models {
    pub struct User {
        pub name: String,
        pub(crate) email: String,
    }

    impl User {
        pub fn new(name: &str, email: &str) -> Self {
            User {
                name: name.to_string(),
                email: email.to_string(),
            }
        }
    }

    pub(crate) fn validate(email: &str) -> bool {
        email.contains('@')
    }
}

mod services {
    use super::models::User;           // (A)
    use super::models::validate;       // (B)
    use crate::models::User as MyUser; // (C)
    use std::collections::*;           // (D)

    pub fn create(name: &str, email: &str) -> User {  // (E) ¿qué User?
        if validate(email) {
            User::new(name, email)
        } else {
            panic!("Invalid email");
        }
    }
}

pub use models::User;                  // (F)
use models::validate;                  // (G)

pub fn public_api() {
    use crate::services::create;       // (H)
    let u = create("Alice", "a@b.com");
}
```

**Analiza cada import (A-H):**
1. ¿Compila? ¿Por qué sí o por qué no?
2. ¿`validate` es accesible en (B) y (G)?
3. ¿El import (C) conflicta con (A)?
4. ¿El import (D) es idiomático aquí?
5. ¿En (E), `User` se refiere al import (A) o (C)?
6. ¿(F) hace que usuarios externos vean `User` como `mi_crate::User`?

---

> **Nota**: el sistema de `use` / `pub use` es lo que permite a los crates de Rust tener APIs limpias independientemente de su estructura interna. Crates como `serde`, `tokio`, `anyhow` hacen un uso extensivo de `pub use` para que importar sus tipos sea simple — `use anyhow::Result` en vez de `use anyhow::error::chain::Result`. Cuando diseñes tu API pública, piensa en cómo será la experiencia del `use` para tus usuarios.
