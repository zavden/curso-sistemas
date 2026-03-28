# mod y archivos: el sistema de módulos de Rust

## Índice
1. [Qué es un módulo](#1-qué-es-un-módulo)
2. [Módulos inline con mod](#2-módulos-inline-con-mod)
3. [Módulos en archivos separados](#3-módulos-en-archivos-separados)
4. [mod.rs vs nombre.rs (edición 2018+)](#4-modrs-vs-nombrors-edición-2018)
5. [Rutas: absolute y relative](#5-rutas-absolute-y-relative)
6. [self, super y crate](#6-self-super-y-crate)
7. [Jerarquías profundas](#7-jerarquías-profundas)
8. [El crate root: lib.rs y main.rs](#8-el-crate-root-librs-y-mainrs)
9. [Múltiples binarios](#9-múltiples-binarios)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. Qué es un módulo

Un módulo en Rust es una **unidad de organización** que agrupa funciones, structs, enums, traits, constantes, y otros módulos bajo un nombre. Los módulos forman un **árbol jerárquico** dentro de cada crate:

```text
mi_crate (raíz)
├── main.rs o lib.rs        ← crate root
├── mod network
│   ├── mod http
│   │   ├── fn get()
│   │   └── fn post()
│   └── mod tcp
│       └── fn connect()
└── mod utils
    └── fn format_date()
```

Los módulos cumplen tres funciones:

1. **Organización**: agrupan código relacionado.
2. **Privacidad**: todo es privado por defecto; expones solo lo necesario con `pub`.
3. **Namespacing**: evitan colisiones de nombres (`network::http::get` vs `db::http::get`).

### Terminología clave

| Término | Significado |
|---------|-------------|
| **Crate** | Unidad de compilación. Puede ser library o binary |
| **Crate root** | El archivo raíz: `lib.rs` (library) o `main.rs` (binary) |
| **Module tree** | Jerarquía de módulos dentro de un crate |
| **Path** | Ruta para referirse a un item: `crate::network::http::get` |
| **Item** | Función, struct, enum, trait, const, mod, impl, type alias |

---

## 2. Módulos inline con mod

La forma más simple de crear un módulo es **inline** — el contenido está directamente en el mismo archivo:

```rust
// main.rs

mod math {
    pub fn add(a: i32, b: i32) -> i32 {
        a + b
    }

    pub fn multiply(a: i32, b: i32) -> i32 {
        a * b
    }

    fn helper() -> i32 {
        // privada — solo accesible dentro de mod math
        42
    }
}

fn main() {
    let sum = math::add(2, 3);
    let product = math::multiply(4, 5);
    println!("{sum}, {product}");

    // math::helper(); // ERROR: función privada
}
```

### Módulos anidados inline

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

    pub mod dns {
        pub fn resolve(host: &str) -> String {
            format!("127.0.0.1 (resolved {})", host)
        }
    }
}

fn main() {
    let response = network::http::get("https://example.com");
    let ip = network::dns::resolve("example.com");
    println!("{response}");
    println!("{ip}");
}
```

Los módulos inline son útiles para agrupar funcionalidad pequeña o para tests (veremos `mod tests` en C11). Para código más extenso, los módulos se separan en archivos.

---

## 3. Módulos en archivos separados

Cuando un módulo crece, lo movemos a su propio archivo. La declaración `mod nombre;` (con punto y coma, sin llaves) le dice a Rust que busque el contenido en un archivo:

```rust
// main.rs
mod math;  // ← busca el contenido en math.rs o math/mod.rs

fn main() {
    let result = math::add(2, 3);
    println!("{result}");
}
```

```rust
// math.rs
pub fn add(a: i32, b: i32) -> i32 {
    a + b
}

pub fn multiply(a: i32, b: i32) -> i32 {
    a * b
}
```

### Estructura en disco

```text
src/
├── main.rs        ← declara `mod math;`
└── math.rs        ← contenido del módulo math
```

### Reglas de búsqueda de archivos

Cuando escribes `mod nombre;` en un archivo, Rust busca el contenido en exactamente **dos posibles ubicaciones** (no ambas):

```text
Desde src/main.rs o src/lib.rs:
  mod math;  →  busca src/math.rs  O  src/math/mod.rs

Desde src/network.rs o src/network/mod.rs:
  mod http;  →  busca src/network/http.rs  O  src/network/http/mod.rs
```

La regla general es: **el archivo del submódulo está junto al archivo que lo declara, o en un subdirectorio con el nombre del módulo padre**.

---

## 4. mod.rs vs nombre.rs (edición 2018+)

Históricamente, un módulo con submódulos **requería** un directorio con `mod.rs`:

### Estilo antiguo (pre-2018, aún válido)

```text
src/
├── main.rs
└── network/
    ├── mod.rs        ← contenido de mod network
    ├── http.rs       ← submódulo http
    └── dns.rs        ← submódulo dns
```

```rust
// src/network/mod.rs
pub mod http;
pub mod dns;

pub fn version() -> &'static str {
    "1.0"
}
```

### Estilo moderno (edición 2018+, recomendado)

Desde la edición 2018, puedes usar `nombre.rs` en lugar de `nombre/mod.rs`:

```text
src/
├── main.rs
├── network.rs        ← contenido de mod network (antes era network/mod.rs)
└── network/
    ├── http.rs        ← submódulo http
    └── dns.rs         ← submódulo dns
```

```rust
// src/network.rs
pub mod http;
pub mod dns;

pub fn version() -> &'static str {
    "1.0"
}
```

```rust
// src/main.rs
mod network;

fn main() {
    println!("{}", network::version());
    println!("{}", network::http::get("/api"));
    println!("{}", network::dns::resolve("example.com"));
}
```

### Comparación

```text
ESTILO ANTIGUO (mod.rs)          ESTILO MODERNO (2018+)
src/                             src/
├── main.rs                      ├── main.rs
└── network/                     ├── network.rs         ← más fácil de encontrar
    ├── mod.rs   ← ¿cuál mod.rs? └── network/
    ├── http.rs                      ├── http.rs
    └── dns.rs                       └── dns.rs
```

**Ventajas del estilo moderno:**
- No tienes 10 pestañas abiertas que digan "mod.rs" — cada archivo tiene nombre único.
- El archivo del módulo (`network.rs`) está al mismo nivel que su directorio (`network/`).
- Más fácil de navegar en editores y herramientas de búsqueda.

**No puedes mezclar ambos estilos para el mismo módulo:**

```text
src/
├── network.rs       ← ERROR si ambos existen
└── network/
    └── mod.rs       ← conflicto: ¿cuál es network?
```

El compilador emitirá un error si encuentra ambos archivos para el mismo módulo.

### Recomendación

Usa el estilo moderno (2018+) para proyectos nuevos. El estilo `mod.rs` sigue siendo válido y lo encontrarás en crates más antiguos — no es incorrecto, solo menos ergonómico.

---

## 5. Rutas: absolute y relative

Para referirte a un item en el árbol de módulos, usas **paths** (rutas):

### Rutas absolutas — empiezan con `crate::`

```rust
// Desde cualquier lugar del crate, siempre funciona:
crate::network::http::get("/api");
crate::utils::format_date();
```

`crate` se refiere a la raíz del crate actual — es como `/` en un filesystem.

### Rutas relativas — empiezan desde el módulo actual

```rust
// Desde dentro de mod network:
mod network {
    pub mod http {
        pub fn get(url: &str) -> String {
            format!("GET {}", url)
        }
    }

    pub fn fetch(url: &str) -> String {
        // Ruta relativa: http está en el mismo módulo
        http::get(url)
    }
}
```

### Cuándo usar cada una

```rust
mod database {
    pub mod connection {
        pub fn connect() -> String {
            String::from("connected")
        }
    }

    pub mod queries {
        pub fn execute() {
            // ABSOLUTA: explícita, no se rompe si reorganizas
            let conn = crate::database::connection::connect();

            // RELATIVA: más corta, pero depende de la posición
            // Desde queries, connection es un hermano:
            let conn = super::connection::connect();

            println!("Using {}", conn);
        }
    }
}
```

**Convención del ecosistema**: preferir rutas relativas para items cercanos (hermanos, hijos), absolutas para items lejanos o cuando la ruta relativa sería confusa.

---

## 6. self, super y crate

Tres palabras clave especiales para construir rutas:

### `crate` — la raíz del crate

```rust
// Siempre apunta al módulo raíz (lib.rs o main.rs)
crate::config::load();
```

### `self` — el módulo actual

```rust
mod utils {
    pub fn helper() -> i32 { 42 }

    pub fn work() {
        // Estas dos líneas son equivalentes:
        let a = helper();
        let b = self::helper();

        // self es útil para desambiguar con use:
        use self::helper;
    }
}
```

`self` es más útil en declaraciones `use` para importar items del módulo actual o resolver ambigüedades (lo veremos en T03).

### `super` — el módulo padre

```rust
mod parent {
    pub fn parent_fn() -> &'static str {
        "from parent"
    }

    pub mod child {
        pub fn child_fn() {
            // super sube un nivel — accede a parent
            let msg = super::parent_fn();
            println!("{msg}");
        }

        pub mod grandchild {
            pub fn deep_fn() {
                // super::super sube dos niveles
                let msg = super::super::parent_fn();
                println!("{msg}");
            }
        }
    }
}
```

### Ejemplo completo con los tres

```rust
mod app {
    pub fn version() -> &'static str { "1.0" }

    pub mod services {
        pub mod auth {
            pub fn login(user: &str) -> String {
                // super → services
                // super::super → app
                let ver = super::super::version();

                // O con ruta absoluta:
                let ver = crate::app::version();

                format!("Login {} (v{})", user, ver)
            }
        }

        pub mod logging {
            pub fn log(msg: &str) {
                // self:: explícito (aquí innecesario, pero legal)
                println!("[LOG] {}", msg);
            }

            pub fn log_auth(user: &str) {
                // super → services, luego baja a auth
                let result = super::auth::login(user);
                self::log(&result);
            }
        }
    }
}
```

---

## 7. Jerarquías profundas

### Proyecto real: estructura típica

```text
src/
├── main.rs
├── config.rs
├── config/
│   ├── file.rs
│   └── env.rs
├── handlers.rs
├── handlers/
│   ├── users.rs
│   └── products.rs
├── models.rs
├── models/
│   ├── user.rs
│   └── product.rs
├── db.rs
└── db/
    ├── pool.rs
    └── migrations.rs
```

```rust
// src/main.rs
mod config;
mod handlers;
mod models;
mod db;

fn main() {
    let cfg = config::load();
    let pool = db::pool::create(&cfg);
    // ...
}
```

```rust
// src/config.rs
mod file;
mod env;

pub struct Config {
    pub database_url: String,
    pub port: u16,
}

pub fn load() -> Config {
    let from_file = file::read_config();
    let from_env = env::read_overrides();
    merge(from_file, from_env)
}

fn merge(file: Config, env: Config) -> Config {
    // ...
    # file
}
```

```rust
// src/config/file.rs
use super::Config;  // Config está en el módulo padre (config.rs)

pub fn read_config() -> Config {
    Config {
        database_url: String::from("postgres://localhost/mydb"),
        port: 8080,
    }
}
```

### Submódulos privados como detalle de implementación

No es necesario que todos los submódulos sean `pub`. Los submódulos privados son detalles de implementación:

```rust
// src/db.rs
mod pool;         // privado: detalle de implementación
mod migrations;   // privado

// Solo exponemos lo que el usuario del módulo necesita:
pub use pool::Pool;
pub use pool::create;

pub fn migrate() {
    migrations::run_all();
}
```

Desde `main.rs`:

```rust
mod db;

fn main() {
    let pool = db::create("postgres://...");  // OK: create es pub
    db::migrate();                            // OK: migrate es pub
    // db::pool::internal_fn();               // ERROR: pool no es pub
    // db::migrations::run_all();             // ERROR: migrations no es pub
}
```

---

## 8. El crate root: lib.rs y main.rs

### Binary crate — `main.rs`

```text
src/
└── main.rs    ← punto de entrada, contiene fn main()
```

```rust
// src/main.rs
mod utils;

fn main() {
    utils::greet("world");
}
```

### Library crate — `lib.rs`

```text
src/
└── lib.rs    ← define la API pública del crate
```

```rust
// src/lib.rs
pub mod utils;
pub mod models;

pub fn init() {
    // ...
}
```

Los usuarios del crate acceden con el nombre del crate:

```rust
// En otro crate:
use mi_crate::utils;
use mi_crate::models::User;
```

### Ambos en el mismo proyecto

Un proyecto puede tener **ambos** `lib.rs` y `main.rs`:

```text
src/
├── lib.rs     ← library crate
├── main.rs    ← binary crate (usa la library)
├── models.rs
└── utils.rs
```

```rust
// src/lib.rs
pub mod models;
pub mod utils;

pub fn init() -> String {
    String::from("initialized")
}
```

```rust
// src/main.rs
// main.rs accede a la library como un crate externo,
// usando el nombre del paquete definido en Cargo.toml
use mi_proyecto::models;
use mi_proyecto::utils;

fn main() {
    let msg = mi_proyecto::init();
    println!("{msg}");
}
```

**Importante**: `main.rs` y `lib.rs` son dos crates separados dentro del mismo paquete. `main.rs` **no** puede hacer `mod models;` si `models.rs` ya está declarado en `lib.rs` — accede a través del nombre del crate como cualquier dependencia externa.

```rust
// src/main.rs

// INCORRECTO (si models.rs ya está en lib.rs):
// mod models;  // conflicto: ¿quién es dueño de models?

// CORRECTO:
use mi_proyecto::models;
```

---

## 9. Múltiples binarios

Un paquete puede tener varios ejecutables. Los binarios adicionales se colocan en `src/bin/`:

```text
src/
├── lib.rs
├── main.rs           ← binary: cargo run
└── bin/
    ├── server.rs     ← binary: cargo run --bin server
    └── cli.rs        ← binary: cargo run --bin cli
```

```rust
// src/bin/server.rs
use mi_proyecto::models;

fn main() {
    println!("Starting server...");
    // Usa la library del mismo crate
}
```

```rust
// src/bin/cli.rs
use mi_proyecto::utils;

fn main() {
    println!("CLI tool v1.0");
}
```

Cada archivo en `src/bin/` es un binary crate independiente. Todos pueden usar la library crate (`lib.rs`).

### Binarios con múltiples archivos

Si un binario necesita sus propios módulos, usa un directorio:

```text
src/
├── lib.rs
└── bin/
    ├── server/
    │   ├── main.rs       ← cargo run --bin server
    │   ├── routes.rs
    │   └── middleware.rs
    └── cli.rs            ← cargo run --bin cli
```

```rust
// src/bin/server/main.rs
mod routes;
mod middleware;

use mi_proyecto::models;

fn main() {
    middleware::setup();
    routes::register();
}
```

---

## 10. Errores comunes

### Error 1: Olvidar declarar el módulo

```text
src/
├── main.rs
└── utils.rs
```

```rust
// src/main.rs
fn main() {
    // ERROR: no existe `utils`
    utils::helper();
}
```

Crear el archivo `utils.rs` **no es suficiente**. Debes declarar el módulo:

```rust
// src/main.rs
mod utils;  // ← necesario: le dice a Rust que utils.rs es parte del crate

fn main() {
    utils::helper();
}
```

Rust no escanea el directorio `src/` buscando archivos. Cada módulo debe ser **declarado explícitamente** en su módulo padre.

### Error 2: Declarar un módulo en el lugar incorrecto

```text
src/
├── main.rs
├── network.rs
└── network/
    └── http.rs
```

```rust
// INCORRECTO: declarar http en main.rs
// src/main.rs
mod network;
mod http;  // ERROR: busca src/http.rs, no src/network/http.rs

// CORRECTO: declarar http dentro de network
// src/network.rs
pub mod http;  // busca src/network/http.rs
```

La regla: **un módulo se declara en su padre directo**, no en un ancestro arbitrario.

### Error 3: Tener mod.rs y nombre.rs simultáneamente

```text
src/
├── main.rs
├── network.rs        ← conflicto
└── network/
    └── mod.rs         ← conflicto
```

```
error[E0761]: file for module `network` found at both
  "src/network.rs" and "src/network/mod.rs"
```

**Solución**: elige uno u otro, no ambos.

### Error 4: Confundir mod con use

```rust
// mod DECLARA un módulo (define su contenido)
mod math;  // "math.rs es parte de mi crate"

// use IMPORTA un path para usarlo sin calificación completa
use math::add;  // "puedo escribir add() en vez de math::add()"

// mod NO es un import — es una declaración
// Declarar el mismo módulo dos veces es un error:
mod math;  // primera declaración
mod math;  // ERROR: duplicate module
```

### Error 5: Intentar acceder a items de un submódulo sin pub

```rust
// src/network.rs
mod http {  // módulo privado
    pub fn get() -> String {
        internal_helper()
    }

    fn internal_helper() -> String {
        String::from("data")
    }
}

pub fn fetch() -> String {
    http::get()  // OK: network puede acceder a su submódulo http
}
```

```rust
// src/main.rs
mod network;

fn main() {
    network::fetch();       // OK: fetch es pub
    // network::http::get(); // ERROR: http no es pub
}
```

Para exponer `http` al exterior, agrégale `pub`:

```rust
// src/network.rs
pub mod http {  // ahora es público
    pub fn get() -> String { ... }
}
```

---

## 11. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                  MÓDULOS Y ARCHIVOS CHEATSHEET                     ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  DECLARAR MÓDULOS                                                  ║
║  mod nombre { ... }         Inline (contenido en el mismo archivo) ║
║  mod nombre;                Archivo separado (nombre.rs)           ║
║                             o directorio (nombre/mod.rs)           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  ARCHIVOS                                                          ║
║  src/main.rs                Crate root (binary)                    ║
║  src/lib.rs                 Crate root (library)                   ║
║  src/foo.rs                 Módulo foo (estilo 2018+)              ║
║  src/foo/mod.rs             Módulo foo (estilo antiguo)            ║
║  src/foo/bar.rs             Submódulo foo::bar                     ║
║  src/bin/name.rs            Binary adicional                       ║
║                                                                    ║
║  NO mezclar foo.rs + foo/mod.rs para el mismo módulo               ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  RUTAS                                                             ║
║  crate::a::b::c             Absoluta (desde la raíz del crate)    ║
║  self::a::b                 Relativa (desde el módulo actual)      ║
║  super::a                   Relativa (desde el módulo padre)       ║
║  a::b                       Relativa (hijo del módulo actual)      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  CRATE ROOT                                                       ║
║  lib.rs  → define API pública del crate                            ║
║  main.rs → punto de entrada, usa lib.rs como crate externo         ║
║  Ambos pueden coexistir en un paquete                              ║
║  main.rs accede via: use nombre_paquete::modulo;                   ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  REGLAS CLAVE                                                      ║
║  1. Crear archivo NO basta — hay que declarar mod nombre;          ║
║  2. mod se declara en el módulo padre, no en un ancestro           ║
║  3. Todo es privado por defecto — usa pub para exponer             ║
║  4. Un módulo padre puede acceder a items privados de sus hijos    ║
║     FALSO → también necesita pub (salvo tests con #[cfg(test)])    ║
║  5. mod ≠ use: mod declara, use importa                           ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: Organizar en módulos

Tienes todo el código en un solo `main.rs`. Reorganízalo en módulos separados en archivos (estilo 2018+), sin cambiar la funcionalidad:

```rust
// src/main.rs — TODO EN UN SOLO ARCHIVO (refactorizar)

struct User {
    name: String,
    email: String,
}

impl User {
    fn new(name: &str, email: &str) -> Self {
        User {
            name: name.to_string(),
            email: email.to_string(),
        }
    }

    fn display(&self) -> String {
        format!("{} <{}>", self.name, self.email)
    }
}

fn validate_email(email: &str) -> bool {
    email.contains('@') && email.contains('.')
}

fn format_greeting(user: &User) -> String {
    format!("Hello, {}!", user.name)
}

fn log_action(action: &str) {
    println!("[LOG] {}", action);
}

fn main() {
    let email = "alice@example.com";
    if validate_email(email) {
        let user = User::new("Alice", email);
        log_action(&format!("Created user: {}", user.display()));
        println!("{}", format_greeting(&user));
    }
}
```

**Estructura objetivo:**

```text
src/
├── main.rs           ← mod declarations + fn main
├── models.rs         ← User struct
├── validation.rs     ← validate_email
├── formatting.rs     ← format_greeting
└── logging.rs        ← log_action
```

**Preguntas:**
1. ¿Qué items necesitan `pub` y cuáles no?
2. ¿Cómo accede `formatting.rs` al tipo `User` definido en `models.rs`?
3. ¿Necesitas `use` en algún módulo? ¿Dónde?

### Ejercicio 2: Jerarquía con submódulos

Crea la siguiente estructura de módulos. Cada función simplemente retorna un `String` describiendo lo que haría. Todo debe compilar y ser accesible desde `main`:

```text
src/
├── main.rs
├── db.rs
├── db/
│   ├── postgres.rs     ← pub fn connect() -> String
│   └── queries.rs      ← pub fn find_user(id: u32) -> String
│                          (usa super::postgres::connect internamente)
├── api.rs
└── api/
    ├── routes.rs        ← pub fn register() -> String
    └── middleware.rs     ← pub fn auth_check() -> String
```

```rust
// src/main.rs esperado:
mod db;
mod api;

fn main() {
    println!("{}", db::postgres::connect());
    println!("{}", db::queries::find_user(1));
    println!("{}", api::routes::register());
    println!("{}", api::middleware::auth_check());
}
```

**Preguntas:**
1. ¿Qué contiene `db.rs`? ¿Y `api.rs`?
2. ¿Cómo usa `queries.rs` la función `connect` de `postgres.rs`?
3. Si quisieras que `postgres` sea un detalle de implementación (no accesible desde `main`), ¿qué cambiarías?

### Ejercicio 3: Library + binary

Crea un proyecto con `lib.rs` y `main.rs` que coexistan. El proyecto se llama `minigrep`:

```text
src/
├── lib.rs            ← pub fn search(query: &str, content: &str) -> Vec<String>
├── main.rs           ← lee args, llama a minigrep::search, imprime resultados
└── config.rs         ← pub struct Config { query, filename }, declarado desde lib.rs
```

```rust
// src/lib.rs
mod config;  // declara config como parte de la library

pub use config::Config;  // re-exporta Config en la raíz

pub fn search(query: &str, content: &str) -> Vec<String> {
    content
        .lines()
        .filter(|line| line.contains(query))
        .map(|line| line.to_string())
        .collect()
}
```

**Implementa:**
1. `config.rs` con `Config` y un constructor `Config::new(args: &[String]) -> Config`
2. `main.rs` que use `minigrep::Config` y `minigrep::search`
3. Verifica: ¿`main.rs` puede hacer `mod config;`? ¿Por qué sí o por qué no?
4. ¿Qué pasa si `Config` no tiene `pub` en `lib.rs` — puede `main.rs` usarlo?

---

> **Nota**: el sistema de módulos es una de las primeras fuentes de confusión para quienes vienen de otros lenguajes. En Python o JavaScript, crear un archivo ya lo hace importable. En Rust, **debes declarar cada módulo explícitamente**. Esto es por diseño — el compilador no escanea directorios, lo que hace que la estructura del módulo sea explícita, predecible y verificable en compilación.
