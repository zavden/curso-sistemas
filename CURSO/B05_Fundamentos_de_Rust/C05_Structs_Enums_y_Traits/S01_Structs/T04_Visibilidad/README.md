# T04 — Visibilidad

## Todo es privado por defecto

En Rust, cada elemento (struct, funcion, constante, modulo) es
**privado** por defecto. Privado significa que solo es accesible
dentro del modulo donde se define. No hay keyword `private` --
la privacidad es el estado natural.

```rust
mod database {
    struct Connection { host: String, port: u16 }

    fn connect() -> Connection {
        Connection { host: String::from("localhost"), port: 5432 }
    }

    fn status(conn: &Connection) {
        // Dentro del mismo modulo, todo es accesible:
        println!("Connected to {}:{}", conn.host, conn.port);
    }
}

fn main() {
    // ERROR: Connection es privado al modulo database.
    // let conn = database::Connection { ... };

    // ERROR: connect es privada al modulo database.
    // let conn = database::connect();
}
```

## pub en structs

`pub struct` hace que el tipo sea visible fuera del modulo, pero
los **campos siguen siendo privados**. Diferencia fundamental con C,
donde un struct siempre expone todos sus campos.

```rust
mod config {
    pub struct AppConfig {
        name: String,       // privado — no se puede acceder desde fuera
        version: String,    // privado
    }
}

fn main() {
    // Podemos nombrar el tipo, pero NO construirlo:
    // let cfg = config::AppConfig {
    //     name: String::from("MyApp"),
    //     version: String::from("1.0"),
    // };
    // ERROR: field `name` of struct `AppConfig` is private
    //
    // Si TODOS los campos son privados, es imposible crear
    // el struct desde fuera con sintaxis literal. Necesitas un constructor.
}
```

## pub en campos

La visibilidad se controla **campo por campo**:

```rust
mod server {
    pub struct ServerConfig {
        pub host: String,       // publico — parte de la interfaz
        pub port: u16,          // publico
        connection_pool: u32,   // privado — detalle de implementacion
        secret_key: String,     // privado — dato sensible
    }

    impl ServerConfig {
        pub fn new(host: String, port: u16) -> Self {
            ServerConfig {
                host,
                port,
                connection_pool: 10,
                secret_key: String::from("default"),
            }
        }
    }
}

fn main() {
    let cfg = server::ServerConfig::new(String::from("0.0.0.0"), 8080);

    println!("{}:{}", cfg.host, cfg.port);  // OK — campos publicos

    // println!("{}", cfg.connection_pool);  // ERROR: field is private
    // println!("{}", cfg.secret_key);       // ERROR: field is private

    // Tampoco se puede construir con sintaxis literal porque
    // hay campos privados — incluso si conoces los valores.
}
```

## Constructores para structs con campos privados

Cuando un struct tiene al menos un campo privado, la unica forma de
crearlo desde fuera del modulo es una funcion constructora (`new`):

```rust
mod auth {
    pub struct User {
        pub username: String,
        email: String,          // privado — se valida en el constructor
        password_hash: String,  // privado — nunca expuesto directamente
    }

    impl User {
        pub fn new(username: String, email: String, password: String) -> Self {
            let password_hash = format!("hashed_{}", password);
            User { username, email, password_hash }
        }

        // Getter: acceso de solo lectura al campo privado.
        pub fn email(&self) -> &str {
            &self.email
        }

        // Metodo que usa el campo privado internamente.
        pub fn verify_password(&self, password: &str) -> bool {
            self.password_hash == format!("hashed_{}", password)
        }
    }
}

fn main() {
    let user = auth::User::new(
        String::from("admin"),
        String::from("admin@example.com"),
        String::from("secret123"),
    );

    println!("User: {}", user.username);                     // OK — pub
    println!("Email: {}", user.email());                      // OK — getter
    println!("Valid: {}", user.verify_password("secret123")); // true

    // println!("{}", user.email);           // ERROR: field is private
    // println!("{}", user.password_hash);   // ERROR: field is private
}
```

## El modulo como boundary de encapsulacion

En Rust, la unidad de encapsulacion es el **modulo** (`mod`), no el
struct. Dentro de un mismo modulo, todo el codigo puede acceder a los
campos privados de cualquier struct definido ahi.

```rust
mod geometry {
    pub struct Point { x: f64, y: f64 }
    pub struct Circle { center: Point, radius: f64 }

    impl Point {
        pub fn new(x: f64, y: f64) -> Self { Point { x, y } }
    }

    impl Circle {
        pub fn new(center: Point, radius: f64) -> Self { Circle { center, radius } }
    }

    // Funcion libre en el mismo modulo: accede a campos privados
    // de Point Y de Circle sin problema.
    pub fn distance_to_edge(circle: &Circle, point: &Point) -> f64 {
        let dx = circle.center.x - point.x;
        let dy = circle.center.y - point.y;
        let dist = (dx * dx + dy * dy).sqrt();
        (dist - circle.radius).abs()
    }
}

fn main() {
    let p = geometry::Point::new(3.0, 4.0);
    let c = geometry::Circle::new(geometry::Point::new(0.0, 0.0), 5.0);
    println!("Distance: {}", geometry::distance_to_edge(&c, &p));

    // Fuera del modulo, no se puede:
    // println!("{}", p.x);       // ERROR: field `x` is private
    // println!("{}", c.radius);  // ERROR: field `radius` is private
}
```

```rust
// Comparacion: en Java/C++, private es por CLASE.
// En Rust, la privacidad es por MODULO. La frontera es el mod, no el tipo.
```

## pub(crate)

`pub(crate)` hace que un elemento sea visible en todo el crate
pero invisible para crates externos. Equivalente a "internal" en C#.

```rust
mod internal {
    pub(crate) struct CacheEntry {
        pub(crate) key: String,
        pub(crate) value: Vec<u8>,
    }

    pub(crate) fn create_entry(key: String, val: Vec<u8>) -> CacheEntry {
        CacheEntry { key, value: val }
    }
}

mod api {
    pub fn store(key: String, data: Vec<u8>) {
        // OK: pub(crate) es visible desde otro modulo del mismo crate.
        let entry = crate::internal::create_entry(key, data);
        println!("Stored: {} ({} bytes)", entry.key, entry.value.len());
    }
}

// Desde un crate externo: use mi_crate::internal::CacheEntry;
// ERROR: pub(crate) no es visible externamente.
```

## pub(super)

`pub(super)` hace que un elemento sea visible solo en el modulo padre.
Util para submodulos que exponen funcionalidad a su contenedor sin
hacerla publica globalmente.

```rust
mod network {
    mod protocol {
        pub(super) fn handshake() -> bool { true }
        pub(super) fn send(data: &[u8]) {
            println!("Sending {} bytes", data.len());
        }
    }

    pub fn connect() {
        // OK: handshake y send son pub(super), y network es el padre.
        if protocol::handshake() {
            protocol::send(&[1, 2, 3]);
        }
    }
}

fn main() {
    network::connect();  // OK
    // network::protocol::handshake();  // ERROR: pub(super), solo visible en network
}
```

## pub(in path)

`pub(in path)` limita la visibilidad a un modulo ancestro concreto:

```rust
mod app {
    mod services {
        mod database {
            pub(in crate::app) fn raw_query(sql: &str) -> Vec<String> {
                println!("Executing: {}", sql);
                vec![String::from("row1"), String::from("row2")]
            }
        }

        pub fn get_users() -> Vec<String> {
            database::raw_query("SELECT * FROM users")  // OK: dentro de app
        }
    }

    pub fn admin_query() {
        let results = services::database::raw_query("SELECT * FROM logs");
        println!("Results: {:?}", results);  // OK: estamos en app
    }
}

fn main() {
    app::admin_query();  // OK
    // app::services::database::raw_query("...");  // ERROR: fuera de app
}
```

```rust
// Resumen de niveles de visibilidad (de mas a menos restrictivo):
//
// (nada)          → privado al modulo actual
// pub(super)      → visible en el modulo padre
// pub(in path)    → visible hasta el modulo indicado por path
// pub(crate)      → visible en todo el crate
// pub             → publico sin restricciones
```

## Visibilidad en enums

Los enums se comportan diferente: cuando un enum es `pub`, **todas
sus variantes son publicas automaticamente**. No existe visibilidad
por variante. Los datos internos de cada variante tambien son publicos.

```rust
mod events {
    pub enum Event {
        Click { x: i32, y: i32 },   // x, y accesibles desde fuera
        KeyPress(char),              // el char accesible desde fuera
        Quit,
    }
}

fn handle(event: events::Event) {
    match event {
        events::Event::Click { x, y } => println!("({}, {})", x, y),
        events::Event::KeyPress(c) => println!("Key: {}", c),
        events::Event::Quit => println!("Quit"),
    }
}

// Razon: un enum representa alternativas. Si tienes acceso al tipo,
// necesitas poder hacer match sobre todas las variantes.
```

## Patrones practicos

### Getters para acceso de solo lectura

```rust
mod account {
    pub struct BankAccount {
        owner: String,
        balance: f64,  // privado — modificacion solo via metodos controlados
    }

    impl BankAccount {
        pub fn new(owner: String, initial: f64) -> Self {
            BankAccount { owner, balance: initial }
        }

        pub fn owner(&self) -> &str { &self.owner }   // getter: referencia
        pub fn balance(&self) -> f64 { self.balance }  // getter: copia (Copy)

        pub fn deposit(&mut self, amount: f64) {
            if amount > 0.0 { self.balance += amount; }
        }

        pub fn withdraw(&mut self, amount: f64) -> Result<(), String> {
            if amount > self.balance {
                return Err(String::from("Insufficient funds"));
            }
            self.balance -= amount;
            Ok(())
        }
    }
}

fn main() {
    let mut acc = account::BankAccount::new(String::from("Alice"), 1000.0);
    acc.deposit(500.0);
    println!("{}: ${}", acc.owner(), acc.balance()); // Alice: $1500

    // acc.balance = -999.0;  // ERROR: field is private
    // Imposible romper la invariante balance >= 0.
}
```

### Invariantes a traves de campos privados

```rust
mod validation {
    // El campo privado garantiza que solo existen Email validos.
    pub struct Email { address: String }

    impl Email {
        pub fn new(address: String) -> Result<Self, String> {
            if !address.contains('@') || address.len() < 3 {
                return Err(format!("Invalid email: {}", address));
            }
            Ok(Email { address })
        }
        pub fn as_str(&self) -> &str { &self.address }
    }
}

fn main() {
    let ok = validation::Email::new(String::from("user@example.com"));   // Ok
    let bad = validation::Email::new(String::from("invalid"));           // Err

    // No se puede saltear la validacion:
    // let hack = validation::Email { address: String::from("bad") };
    // ERROR: field `address` is private
}
```

---

## Ejercicios

### Ejercicio 1 -- Encapsulacion basica

```rust
// Crear un modulo `temperature` con un struct `Celsius` que:
//   - Almacene la temperatura en un campo privado f64
//   - Tenga un constructor new() que rechace valores bajo -273.15
//     (cero absoluto), devolviendo Result<Self, String>
//   - Tenga un getter value() -> f64
//   - Tenga un metodo to_fahrenheit() -> f64
//
// Desde main, verificar que:
//   1. Celsius::new(100.0) funciona
//   2. Celsius::new(-300.0) devuelve error
//   3. No se puede hacer Celsius { degrees: 50.0 } desde fuera
```

### Ejercicio 2 -- Visibilidad mixta

```rust
// Crear un modulo `user` con un struct `Profile` que tenga:
//   - pub name: String          (publico)
//   - email: String             (privado — solo via getter)
//   - password_hash: String     (privado — sin getter, solo verificacion)
//
// Implementar:
//   - Profile::new(name, email, password) -> Self
//   - Profile::email(&self) -> &str
//   - Profile::check_password(&self, candidate: &str) -> bool
//
// Verificar que:
//   1. Se puede leer profile.name directamente
//   2. Se puede leer el email solo via profile.email()
//   3. No se puede acceder a password_hash de ninguna forma
//   4. check_password funciona correctamente
```

### Ejercicio 3 -- pub(crate) y pub(super)

```rust
// Crear la siguiente estructura de modulos:
//
// mod app {
//     mod db {
//         pub(super) fn connect() -> String
//         pub(crate) fn raw_sql(query: &str) -> Vec<String>
//     }
//     mod api {
//         pub fn get_items() -> Vec<String>
//         // Debe usar db::raw_sql (accesible via pub(crate))
//     }
//     pub fn init()
//     // Debe usar db::connect (accesible via pub(super))
// }
//
// Verificar que:
//   1. app::init() funciona
//   2. app::api::get_items() funciona
//   3. app::db::connect() NO compila (pub(super), solo visible en app)
//   4. app::db::raw_sql() NO compila desde fuera del modulo app
```
