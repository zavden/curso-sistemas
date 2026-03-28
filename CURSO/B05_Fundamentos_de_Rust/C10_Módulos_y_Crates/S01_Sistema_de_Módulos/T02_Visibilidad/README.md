# Visibilidad: pub, pub(crate), pub(super), pub(in path)

## Índice
1. [Privacidad por defecto](#1-privacidad-por-defecto)
2. [pub: visibilidad pública](#2-pub-visibilidad-pública)
3. [pub(crate): visible en todo el crate](#3-pubcrate-visible-en-todo-el-crate)
4. [pub(super): visible en el módulo padre](#4-pubsuper-visible-en-el-módulo-padre)
5. [pub(in path): visibilidad a medida](#5-pubin-path-visibilidad-a-medida)
6. [Visibilidad de structs y sus campos](#6-visibilidad-de-structs-y-sus-campos)
7. [Visibilidad de enums](#7-visibilidad-de-enums)
8. [Visibilidad de traits e impls](#8-visibilidad-de-traits-e-impls)
9. [Reglas de privacidad del compilador](#9-reglas-de-privacidad-del-compilador)
10. [Patrones de diseño de API](#10-patrones-de-diseño-de-api)
11. [Errores comunes](#11-errores-comunes)
12. [Cheatsheet](#12-cheatsheet)
13. [Ejercicios](#13-ejercicios)

---

## 1. Privacidad por defecto

En Rust, **todo es privado por defecto**. Funciones, structs, enums, constantes, módulos, campos — todo. Solo el código dentro del mismo módulo (y sus submódulos) puede acceder a items privados:

```rust
mod database {
    fn connect() -> String {
        // privada: solo accesible dentro de mod database
        String::from("connected")
    }

    fn internal_helper() {
        // OK: mismo módulo
        let c = connect();
    }

    pub fn public_api() -> String {
        // OK: mismo módulo puede acceder a connect()
        connect()
    }
}

fn main() {
    // ERROR: connect es privada
    // database::connect();

    // OK: public_api es pub
    let result = database::public_api();
}
```

### Quién puede ver qué

La visibilidad en Rust se basa en la **posición en el árbol de módulos**, no en la posición en el archivo. La regla fundamental:

> Un item privado es accesible desde el módulo donde se define y desde todos los submódulos descendientes de ese módulo.

```rust
mod parent {
    fn secret() -> i32 { 42 }

    mod child {
        fn test() {
            // ERROR: child NO puede acceder a privados del padre
            // Los hijos no ven hacia arriba automáticamente
            // super::secret();  // ERROR
        }
    }
}
```

**Atención**: los hijos **no** heredan acceso a los items privados del padre. Esto difiere de lenguajes como Java donde las clases internas ven los privados de la clase contenedora. En Rust, privado significa *solo en este módulo*.

---

## 2. pub: visibilidad pública

`pub` hace que un item sea visible desde **cualquier lugar** — dentro del crate o desde crates externos que dependan del tuyo:

```rust
// src/lib.rs
pub mod api {
    pub fn endpoint() -> String {
        String::from("response")
    }

    pub struct Response {
        pub status: u16,
        pub body: String,
    }
}
```

```rust
// En otro crate que dependa del anterior:
use mi_crate::api;

fn main() {
    let r = api::endpoint();
    let resp = api::Response { status: 200, body: String::new() };
}
```

### pub en el contexto de un binary crate

En un binary crate (solo `main.rs`, sin `lib.rs`), `pub` técnicamente no expone nada al exterior — no hay consumidores externos. Pero sigue siendo necesario para visibilidad entre módulos dentro del crate:

```rust
// src/main.rs
mod utils;

fn main() {
    utils::helper();  // necesita que helper sea pub en utils
}
```

```rust
// src/utils.rs
pub fn helper() {
    // pub para que main.rs pueda llamarla
}
```

---

## 3. pub(crate): visible en todo el crate

`pub(crate)` hace que un item sea visible desde **cualquier módulo dentro del mismo crate**, pero no desde crates externos:

```rust
// src/lib.rs
pub mod api {
    pub fn public_endpoint() -> String {
        // visible para todos (incluso crates externos)
        internal_setup();
        String::from("response")
    }

    pub(crate) fn internal_setup() {
        // visible dentro de este crate, pero NO para crates externos
        println!("Setting up...");
    }

    fn private_helper() {
        // solo visible dentro de mod api
    }
}

pub mod cli {
    pub fn run() {
        // OK: pub(crate) es visible desde cualquier módulo del crate
        crate::api::internal_setup();

        // ERROR: private_helper es privada de api
        // crate::api::private_helper();
    }
}
```

### Cuándo usar pub(crate)

`pub(crate)` es el equivalente de `internal` en C# o `package-private` en Java. Es ideal para:

```rust
pub(crate) struct DatabasePool {
    // Los módulos del crate pueden usar el pool,
    // pero usuarios externos no — es un detalle de implementación
    pub(crate) connections: Vec<Connection>,
}

pub(crate) const MAX_RETRIES: u32 = 3;
// Configuración interna compartida entre módulos

pub(crate) fn validate_input(input: &str) -> bool {
    // Validación compartida entre api y cli, pero no expuesta externamente
    !input.is_empty() && input.len() < 1000
}
```

---

## 4. pub(super): visible en el módulo padre

`pub(super)` hace que un item sea visible en el módulo padre inmediato (un nivel arriba):

```rust
mod services {
    mod internal {
        pub(super) fn helper() -> String {
            // Visible en `services` (el padre), pero no más arriba
            String::from("internal help")
        }

        pub(super) struct Config {
            pub(super) debug: bool,
        }
    }

    pub fn process() -> String {
        // OK: services es el padre de internal
        let cfg = internal::Config { debug: true };
        let h = internal::helper();
        format!("{h} (debug={})", cfg.debug)
    }
}

fn main() {
    println!("{}", services::process());  // OK

    // ERROR: helper es pub(super), solo visible en services
    // services::internal::helper();
}
```

### pub(super) en jerarquías

```rust
mod level0 {
    pub mod level1 {
        pub(super) fn visible_in_level0() {
            // visible en level0
        }

        pub mod level2 {
            pub(super) fn visible_in_level1() {
                // visible en level1, NO en level0
            }

            pub fn public_fn() {
                // Puede llamar a su propio pub(super):
                visible_in_level1();

                // Y al pub(super) del padre:
                super::visible_in_level0();
            }
        }
    }

    fn test() {
        level1::visible_in_level0();      // OK: somos el super de level1

        // ERROR: visible_in_level1 es pub(super) de level2,
        // solo visible en level1
        // level1::level2::visible_in_level1();
    }
}
```

`pub(super)` es útil para exponer detalles que un submódulo necesita compartir con su padre sin hacerlos visibles a todo el crate.

---

## 5. pub(in path): visibilidad a medida

`pub(in path)` permite especificar **exactamente** hasta qué ancestro del árbol de módulos es visible un item:

```rust
mod app {
    pub mod services {
        pub mod auth {
            mod tokens {
                // Visible hasta app::services (dos niveles arriba)
                pub(in crate::app::services) fn generate_token() -> String {
                    String::from("token_abc123")
                }

                // Visible hasta app (tres niveles arriba)
                pub(in crate::app) fn revoke_all() {
                    println!("All tokens revoked");
                }
            }

            pub fn login() -> String {
                // OK: auth es descendiente de services
                let token = tokens::generate_token();
                format!("Logged in with {}", token)
            }
        }

        pub fn admin_panel() {
            // OK: services es el path especificado
            let token = auth::tokens::generate_token();
            println!("Admin token: {}", token);
        }
    }

    fn app_level() {
        // OK: app es el path de revoke_all
        services::auth::tokens::revoke_all();

        // ERROR: generate_token solo es visible hasta services
        // services::auth::tokens::generate_token();
    }
}
```

### Restricciones de pub(in path)

El path debe ser un **ancestro** del módulo actual — no puedes hacer visible algo hacia un hermano o un módulo no relacionado:

```rust
mod a {
    mod b {
        // OK: crate::a es un ancestro de b
        pub(in crate::a) fn ok() {}

        // ERROR: crate::other no es un ancestro de b
        // pub(in crate::other) fn bad() {}
    }
}

mod other {}
```

### Cuándo usar pub(in path)

En la práctica, `pub(in path)` se usa poco. Es útil en crates grandes con jerarquías profundas donde necesitas un control granular:

```rust
mod engine {
    mod rendering {
        mod shaders {
            // Solo el subsistema de rendering debe ver esto
            pub(in crate::engine::rendering) fn compile_shader() {}
        }

        mod pipeline {
            fn setup() {
                // OK: estamos dentro de rendering
                super::shaders::compile_shader();
            }
        }
    }

    mod physics {
        fn test() {
            // ERROR: compile_shader es pub(in engine::rendering)
            // super::rendering::shaders::compile_shader();
        }
    }
}
```

---

## 6. Visibilidad de structs y sus campos

Un struct `pub` con campos privados es un patrón fundamental en Rust — los campos tienen su propia visibilidad **independiente** del struct:

```rust
pub struct User {
    pub name: String,       // público: cualquiera puede leer/escribir
    pub email: String,      // público
    password_hash: String,  // privado: solo este módulo
    login_count: u32,       // privado
}
```

### Consecuencias de campos privados

Si un struct tiene **al menos un campo privado**, no se puede construir directamente desde fuera del módulo:

```rust
mod auth {
    pub struct User {
        pub name: String,
        password_hash: String,  // privado
    }

    impl User {
        // Constructor necesario — única forma de crear User desde fuera
        pub fn new(name: &str, password: &str) -> Self {
            User {
                name: name.to_string(),
                password_hash: hash(password),
            }
        }

        pub fn verify(&self, password: &str) -> bool {
            self.password_hash == hash(password)
        }

        fn hash(password: &str) -> String {
            format!("hashed_{}", password)
        }
    }
}

fn main() {
    // OK: usar el constructor público
    let user = auth::User::new("Alice", "secret123");

    // OK: campo público
    println!("{}", user.name);

    // ERROR: campo privado
    // println!("{}", user.password_hash);

    // ERROR: no se puede construir directamente (tiene campos privados)
    // let user = auth::User {
    //     name: String::from("Bob"),
    //     password_hash: String::from("???"),
    // };
}
```

### Campos pub(crate) y pub(super)

Los campos también aceptan los modificadores de visibilidad restringida:

```rust
pub struct Config {
    pub name: String,                    // visible para todos
    pub(crate) database_url: String,     // visible en el crate, no externamente
    pub(super) debug_mode: bool,         // visible en el módulo padre
    secret_key: String,                  // solo en este módulo
}
```

### Tuple structs y visibilidad

```rust
// Todos los campos públicos:
pub struct Point(pub f64, pub f64);

// Campo privado → necesita constructor:
pub struct Meters(f64);  // el f64 es privado

impl Meters {
    pub fn new(value: f64) -> Self {
        assert!(value >= 0.0, "Meters must be non-negative");
        Meters(value)
    }

    pub fn value(&self) -> f64 {
        self.0
    }
}
```

---

## 7. Visibilidad de enums

A diferencia de los structs, **las variantes de un enum público son automáticamente públicas**. No se puede hacer una variante privada:

```rust
pub enum Color {
    Red,           // público automáticamente
    Green,         // público automáticamente
    Blue,          // público automáticamente
    Custom(u8, u8, u8),  // también público, incluyendo los campos
}

// Desde cualquier lugar:
let c = Color::Custom(255, 128, 0);

match c {
    Color::Red => {},
    Color::Green => {},
    Color::Blue => {},
    Color::Custom(r, g, b) => println!("{r},{g},{b}"),
}
```

### ¿Por qué los enums son diferentes?

Un enum sin acceso a todas sus variantes no se puede usar en un `match` exhaustivo. Hacer variantes privadas rompería una propiedad fundamental del tipo. Si necesitas ocultar variantes, el patrón es usar `#[non_exhaustive]`:

```rust
#[non_exhaustive]
pub enum DatabaseError {
    ConnectionFailed,
    QueryTimeout,
    InvalidQuery(String),
}
// Los usuarios externos DEBEN incluir `_ =>` en su match
// Esto permite añadir variantes sin romper compatibilidad
```

### Campos de variantes con nombre

Aunque las variantes son públicas, los campos **con nombre** dentro de variantes siguen la misma regla que siempre — pero en la práctica todos son públicos porque no hay sintaxis para hacerlos privados en variantes:

```rust
pub enum Shape {
    Circle { radius: f64 },         // radius es público
    Rectangle { width: f64, height: f64 },  // ambos públicos
}

let s = Shape::Circle { radius: 5.0 };  // construcción directa OK
```

---

## 8. Visibilidad de traits e impls

### Traits

Un trait sigue las mismas reglas de visibilidad:

```rust
mod shapes {
    pub trait Area {
        fn area(&self) -> f64;  // los métodos del trait no necesitan pub
    }

    trait InternalValidation {
        fn validate(&self) -> bool;
    }
}
```

**Importante**: los métodos de un trait **no llevan `pub`** — la visibilidad del método la determina la visibilidad del trait. Si puedes ver el trait, puedes ver todos sus métodos:

```rust
pub trait Drawable {
    fn draw(&self);       // NO pub — la visibilidad la da el trait
    fn bounds(&self) -> Rect;
    // pub fn bad(&self);  // ERROR: pub no es válido aquí
}
```

### impl blocks

Los bloques `impl` no tienen visibilidad propia — la visibilidad la determinan los items individuales dentro:

```rust
pub struct Counter {
    count: u32,  // privado
}

impl Counter {
    pub fn new() -> Self {
        Counter { count: 0 }
    }

    pub fn increment(&mut self) {
        self.count += 1;
        self.log_change();  // OK: mismo módulo
    }

    pub fn value(&self) -> u32 {
        self.count
    }

    fn log_change(&self) {
        // privada — solo usable dentro de este módulo
        println!("Counter changed to {}", self.count);
    }
}
```

### Trait implementations

Las implementaciones de traits tienen una regla especial: la visibilidad depende de la visibilidad tanto del trait como del tipo:

```rust
mod inner {
    pub struct Foo;

    // Esta impl es visible donde tanto Display como Foo sean visibles
    impl std::fmt::Display for Foo {
        fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
            write!(f, "Foo")
        }
    }

    trait Secret {
        fn secret(&self);
    }

    // Esta impl solo es visible donde Secret sea visible (solo inner)
    impl Secret for Foo {
        fn secret(&self) {
            println!("shh");
        }
    }
}

fn main() {
    let foo = inner::Foo;
    println!("{}", foo);  // OK: Display es público, Foo es pub

    // foo.secret();  // ERROR: trait Secret no es visible aquí
}
```

Para llamar un método de trait, el trait debe estar **en scope** — no basta con que el tipo lo implemente:

```rust
use std::io::Read;  // necesario para usar .read() en tipos que implementan Read

let mut file = std::fs::File::open("test.txt").unwrap();
let mut buf = [0u8; 100];
file.read(&mut buf).unwrap();  // solo funciona si `use std::io::Read` está presente
```

---

## 9. Reglas de privacidad del compilador

### Regla 1: los items públicos no pueden exponer tipos privados

```rust
mod inner {
    struct PrivateType;  // privada

    // ERROR: tipo público que retorna tipo privado
    // pub fn leak() -> PrivateType {
    //     PrivateType
    // }

    // ERROR: campo público de tipo privado
    // pub struct Wrapper {
    //     pub data: PrivateType,
    // }
}
```

El compilador emite `E0446: private type in public interface`. Esto evita que los consumidores de tu API dependan de tipos que no pueden nombrar.

### Regla 2: tipo público en firma pública requiere visibilidad coherente

```rust
pub(crate) struct Internal;

pub mod api {
    // ERROR: pub fn expone tipo pub(crate) — los externos no pueden ver Internal
    // pub fn get() -> super::Internal {
    //     super::Internal
    // }

    // OK: pub(crate) fn con tipo pub(crate) — coherente
    pub(crate) fn get() -> super::Internal {
        super::Internal
    }
}
```

La regla es: **la visibilidad de la firma no puede ser mayor que la visibilidad de los tipos que usa**.

### Regla 3: un módulo hijo no ve los privados de su padre

```rust
mod parent {
    fn secret() -> i32 { 42 }

    mod child {
        fn try_access() {
            // ERROR: secret es privada de parent, no de child
            // let x = super::secret();
        }
    }
}
```

Para que `child` acceda, `secret` debe ser al menos `pub(super)`:

```rust
mod parent {
    pub(super) fn accessible_to_child() -> i32 { 42 }
    // Espera... pub(super) haría visible a la raíz, no al child.
    // El child es un DESCENDIENTE, no el padre.

    // Para que child acceda, necesita ser pub(crate), pub, o pub(super)
    // Pero pub(super) sube al padre de parent, no baja.

    // Realmente, para que child acceda basta con que NO sea privado:
    fn private_to_parent() -> i32 { 42 }

    mod child {
        fn try_access() {
            // En realidad... ¡ESTO SÍ COMPILA!
            // Los submódulos SÍ pueden acceder a items privados del padre.
            let x = super::private_to_parent();
        }
    }
}
```

**Corrección importante**: la regla real de Rust es:

> Un item privado es visible para el módulo que lo define **y todos sus descendientes**.

Esto significa que `child` **sí** puede acceder a `parent::secret()` con `super::secret()`. Verifiquémoslo:

```rust
mod parent {
    fn secret() -> i32 { 42 }

    mod child {
        fn access() {
            let x = super::secret();  // COMPILA: child es descendiente de parent
        }

        mod grandchild {
            fn deep_access() {
                let x = super::super::secret();  // COMPILA: grandchild también es descendiente
            }
        }
    }
}

// Lo que NO funciona es acceder desde un módulo HERMANO:
mod sibling {
    fn try_access() {
        // ERROR: sibling no es descendiente de parent
        // super::parent::secret();
    }
}
```

---

## 10. Patrones de diseño de API

### Patrón 1: campos privados con constructores y accessors

```rust
pub struct Email {
    address: String,  // privado: validado en el constructor
}

impl Email {
    pub fn new(address: &str) -> Result<Self, String> {
        if !address.contains('@') {
            return Err(format!("Invalid email: {}", address));
        }
        Ok(Email { address: address.to_string() })
    }

    pub fn as_str(&self) -> &str {
        &self.address
    }
}
```

Esto garantiza que **no puede existir un `Email` inválido** — el invariante se mantiene porque nadie puede construir `Email` directamente.

### Patrón 2: módulo sellado (sealed module)

Exponer solo lo necesario, ocultando la estructura interna:

```rust
// src/lib.rs
mod engine;      // privado: detalle de implementación

pub use engine::Database;     // re-exportar solo el tipo público
pub use engine::Query;
pub use engine::connect;      // re-exportar solo la función pública

// Los usuarios ven:
//   mi_crate::Database
//   mi_crate::Query
//   mi_crate::connect
// Pero NO:
//   mi_crate::engine (módulo invisible)
//   mi_crate::engine::internal_fn (inaccesible)
```

### Patrón 3: pub(crate) para helpers compartidos

```rust
// src/utils.rs
pub(crate) fn sanitize(input: &str) -> String {
    input.trim().to_lowercase()
}

pub(crate) fn hash(data: &str) -> String {
    format!("{:x}", md5::compute(data))
}
```

```rust
// src/api.rs
pub fn create_user(name: &str) {
    let clean_name = crate::utils::sanitize(name);  // OK dentro del crate
    // ...
}
```

```rust
// Desde un crate externo:
// mi_crate::utils::sanitize("test");  // ERROR: utils no es pub
```

### Patrón 4: builder con campos privados

```rust
pub struct ServerConfig {
    host: String,
    port: u16,
    max_connections: usize,
    timeout_secs: u64,
}

pub struct ServerConfigBuilder {
    host: String,
    port: u16,
    max_connections: usize,
    timeout_secs: u64,
}

impl ServerConfigBuilder {
    pub fn new(host: &str, port: u16) -> Self {
        ServerConfigBuilder {
            host: host.to_string(),
            port,
            max_connections: 100,
            timeout_secs: 30,
        }
    }

    pub fn max_connections(mut self, n: usize) -> Self {
        self.max_connections = n;
        self
    }

    pub fn timeout(mut self, secs: u64) -> Self {
        self.timeout_secs = secs;
        self
    }

    pub fn build(self) -> ServerConfig {
        ServerConfig {
            host: self.host,
            port: self.port,
            max_connections: self.max_connections,
            timeout_secs: self.timeout_secs,
        }
    }
}

// Uso:
let config = ServerConfigBuilder::new("0.0.0.0", 8080)
    .max_connections(500)
    .timeout(60)
    .build();
```

### Patrón 5: tipo opaco (newtype privado)

```rust
pub mod tokens {
    // El tipo interno es privado — nadie puede fabricar un Token
    pub struct Token(String);

    impl Token {
        pub(crate) fn generate(user_id: u32) -> Self {
            Token(format!("tok_{}_{}", user_id, rand::random::<u64>()))
        }

        pub fn as_str(&self) -> &str {
            &self.0
        }
    }

    // Desde fuera del crate:
    // - Puedes recibir un Token y leerlo con .as_str()
    // - NO puedes crear uno (Token(String) es privado)
    // - NO puedes modificar su contenido
}
```

---

## 11. Errores comunes

### Error 1: Olvidar pub en items anidados

```rust
pub mod api {
    struct User {  // ← olvidó pub
        pub name: String,
    }

    pub fn get_user() -> User {
        // ERROR E0446: private type `User` in public interface
        User { name: String::from("Alice") }
    }
}
```

**Solución**: hacer `pub struct User` o cambiar la función a `pub(crate)`.

### Error 2: Struct pub con campos privados sin constructor

```rust
pub mod models {
    pub struct Config {
        host: String,  // privado
        port: u16,     // privado
    }

    // Sin constructor público → nadie fuera de models puede crear un Config
}

fn main() {
    // ERROR: cannot construct `Config` — fields are private
    // let c = models::Config { host: "localhost".into(), port: 8080 };

    // Tampoco hay Config::new() ni Config::default()
    // El tipo es público pero inutilizable fuera del módulo
}
```

**Solución**: añadir un constructor público o `#[derive(Default)]` con `impl Default`.

### Error 3: Hacer todo pub "por comodidad"

```rust
// MALO: todo público sin razón
pub struct Database {
    pub connection_string: String,  // ¿debe poder modificarse externamente?
    pub pool_size: usize,           // ¿y si alguien pone 0?
    pub internal_cache: HashMap<String, Vec<u8>>,  // ¿detalle de implementación?
}
```

**Problema**: no puedes cambiar la representación interna sin romper la API pública. Cada campo `pub` es una promesa de compatibilidad.

```rust
// MEJOR: exponer solo lo necesario
pub struct Database {
    connection_string: String,
    pool_size: usize,
    internal_cache: HashMap<String, Vec<u8>>,
}

impl Database {
    pub fn new(conn: &str, pool_size: usize) -> Result<Self, String> {
        if pool_size == 0 {
            return Err("Pool size must be > 0".into());
        }
        Ok(Database {
            connection_string: conn.to_string(),
            pool_size,
            internal_cache: HashMap::new(),
        })
    }

    pub fn pool_size(&self) -> usize {
        self.pool_size
    }
}
```

### Error 4: pub(crate) vs pub en binary crates

```rust
// En un binary crate (solo main.rs), pub(crate) y pub son equivalentes
// porque no hay consumidores externos.
// Pero por claridad de intención:
pub(crate) fn internal_helper() { }  // "esto es interno"
pub fn also_works() { }              // "esto podría ser externo" (confuso)
```

En binary crates, usa `pub(crate)` para comunicar intención — si el crate nunca será una library, `pub` y `pub(crate)` tienen el mismo efecto, pero `pub(crate)` documenta que es un detalle interno.

### Error 5: Olvidar que `use` no cambia la visibilidad

```rust
mod inner {
    pub struct Foo;
}

// use trae Foo al scope actual, pero no lo hace más público:
use inner::Foo;  // privado: solo disponible en este módulo

// Para re-exportar públicamente:
pub use inner::Foo;  // ahora Foo es accesible como crate::Foo
```

---

## 12. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════╗
║                     VISIBILIDAD CHEATSHEET                         ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  MODIFICADORES                                                     ║
║  (sin modifier)       Privado: módulo actual + descendientes       ║
║  pub                  Público: visible para todos                  ║
║  pub(crate)           Visible en todo el crate, no externamente    ║
║  pub(super)           Visible en el módulo padre                   ║
║  pub(in crate::a::b)  Visible hasta el módulo especificado         ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  APLICA A                                                          ║
║  ✓ Funciones, constantes, statics, type aliases                    ║
║  ✓ Structs (el tipo) y sus campos (independientemente)             ║
║  ✓ Enums (el tipo) — variantes siempre públicas si enum es pub     ║
║  ✓ Traits                                                          ║
║  ✓ Módulos                                                         ║
║  ✓ Métodos en impl blocks                                          ║
║  ✗ Métodos de trait (visibilidad viene del trait)                   ║
║  ✗ Variantes de enum (siempre públicas si enum es pub)             ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  STRUCTS vs ENUMS                                                  ║
║  struct pub + campo sin pub → campo privado, necesita constructor   ║
║  enum pub → todas las variantes públicas automáticamente           ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  REGLAS DEL COMPILADOR                                             ║
║  1. Items privados visibles en módulo actual + descendientes       ║
║  2. pub fn no puede retornar/aceptar tipos más privados (E0446)    ║
║  3. pub use re-exporta; use sin pub no cambia visibilidad          ║
║  4. Para llamar métodos de trait: el trait debe estar en scope      ║
║                                                                    ║
╠══════════════════════════════════════════════════════════════════════╣
║                                                                    ║
║  NIVEL RECOMENDADO POR CONTEXTO                                    ║
║  Library crate:                                                    ║
║    pub         → API pública estable                               ║
║    pub(crate)  → helpers compartidos entre módulos                  ║
║    (privado)   → detalles de implementación del módulo              ║
║                                                                    ║
║  Binary crate:                                                     ║
║    pub(crate)  → compartido entre módulos                           ║
║    (privado)   → interno al módulo                                  ║
║    pub         → solo si también tienes lib.rs                      ║
║                                                                    ║
╚══════════════════════════════════════════════════════════════════════╝
```

---

## 13. Ejercicios

### Ejercicio 1: Corregir visibilidad

El siguiente código no compila. Sin cambiar la estructura de módulos ni los nombres, añade los modificadores de visibilidad mínimos necesarios para que compile. Usa el nivel más restrictivo posible (privado > `pub(super)` > `pub(crate)` > `pub`):

```rust
mod models {
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

        fn name(&self) -> &str {
            &self.name
        }
    }
}

mod services {
    fn create_user(name: &str, email: &str) -> crate::models::User {
        crate::models::User::new(name, email)
    }

    fn greet(user: &crate::models::User) -> String {
        format!("Hello, {}!", user.name())
    }
}

fn main() {
    let user = services::create_user("Alice", "alice@example.com");
    let greeting = services::greet(&user);
    println!("{greeting}");
}
```

**Preguntas:**
1. ¿`User::email` necesita ser público?
2. ¿Hay algún item donde `pub(super)` sea suficiente en vez de `pub(crate)`?
3. ¿Podría algún item usar `pub(crate)` en vez de `pub`? ¿Depende de si es library o binary crate?

### Ejercicio 2: Diseñar visibilidad para una API

Diseña los modificadores de visibilidad para esta library crate. El objetivo es exponer la API mínima necesaria para que un usuario externo pueda:
- Crear un `BankAccount` con un titular y saldo inicial
- Depositar y retirar dinero
- Consultar el saldo
- **NO** poder modificar el saldo directamente ni ver el historial interno

```rust
// src/lib.rs
mod ledger;    // ¿pub?

_____ use ledger::Ledger;  // ¿pub use?

_____ struct BankAccount {
    _____ holder: String,
    _____ balance: f64,
    _____ ledger: Ledger,
}

impl BankAccount {
    _____ fn new(holder: &str, initial_balance: f64) -> Self {
        let mut ledger = Ledger::new();
        ledger.record("OPEN", initial_balance);
        BankAccount {
            holder: holder.to_string(),
            balance: initial_balance,
            ledger,
        }
    }

    _____ fn deposit(&mut self, amount: f64) {
        self.balance += amount;
        self.ledger.record("DEPOSIT", amount);
    }

    _____ fn withdraw(&mut self, amount: f64) -> Result<(), String> {
        if amount > self.balance {
            return Err("Insufficient funds".into());
        }
        self.balance -= amount;
        self.ledger.record("WITHDRAW", amount);
        Ok(())
    }

    _____ fn balance(&self) -> f64 {
        self.balance
    }

    _____ fn audit_log(&self) -> &[LedgerEntry] {
        // Solo para uso interno del crate (auditoría interna)
        self.ledger.entries()
    }
}
```

```rust
// src/ledger.rs
_____ struct Ledger {
    _____ entries: Vec<LedgerEntry>,
}

_____ struct LedgerEntry {
    _____ action: String,
    _____ amount: f64,
}

impl Ledger {
    _____ fn new() -> Self { ... }
    _____ fn record(&mut self, action: &str, amount: f64) { ... }
    _____ fn entries(&self) -> &[LedgerEntry] { ... }
}
```

Rellena cada `_____` con el modificador correcto. Justifica cada decisión.

### Ejercicio 3: Refactorizar pub excesivo

Este código funciona pero todo es innecesariamente público. Reduce la visibilidad al mínimo sin romper `main()`:

```rust
pub mod database {
    pub struct Connection {
        pub url: String,
        pub is_connected: bool,
        pub query_count: u32,
    }

    impl Connection {
        pub fn new(url: &str) -> Self {
            Connection {
                url: url.to_string(),
                is_connected: false,
                query_count: 0,
            }
        }

        pub fn connect(&mut self) {
            self.is_connected = true;
        }

        pub fn execute(&mut self, query: &str) -> String {
            self.query_count += 1;
            format!("Result of: {}", query)
        }

        pub fn disconnect(&mut self) {
            self.is_connected = false;
        }

        pub fn reset_counter(&mut self) {
            self.query_count = 0;
        }
    }
}

fn main() {
    let mut conn = database::Connection::new("postgres://localhost/db");
    conn.connect();
    let result = conn.execute("SELECT 1");
    println!("{}", result);
    conn.disconnect();
}
```

**Preguntas:**
1. ¿`main()` usa `url`, `is_connected`, o `query_count` directamente?
2. ¿`main()` usa `reset_counter()`?
3. ¿Qué nivel usarías para `reset_counter` si otro módulo del crate lo necesitara pero no los usuarios externos?
4. Después de hacer los campos privados, ¿necesitas añadir algún accessor?

---

> **Nota**: la visibilidad es una de las herramientas más potentes de Rust para diseñar APIs robustas. Campos privados con constructores validados garantizan invariantes en compilación — si el constructor verifica que un email contiene `@`, **todo `Email` existente es válido**. Este patrón, conocido como *parse, don't validate*, es idiomático en Rust y se apoya directamente en el sistema de visibilidad.
