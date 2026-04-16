# T03 — Typestate Builder en Rust

---

## 1. El problema que resuelve Typestate

T02 mostro un builder con `Option<T>` para campos obligatorios.
Si el caller olvida un campo obligatorio, `build()` retorna
`Err` en **runtime**:

```rust
// T02: error en RUNTIME
let db = DatabaseConfigBuilder::new()
    .host("localhost")
    // Olvide .user() y .password()
    .build();
// Err("user is required") — descubierto al ejecutar
```

El typestate builder mueve esta verificacion a **compile time**:

```rust
// T03: error en COMPILE TIME
let db = DatabaseConfigBuilder::new()
    .host("localhost")
    // Olvide .user() y .password()
    .build();
// ERROR DE COMPILACION: method `build` not found
// build() solo existe cuando user y password estan seteados
```

```
  T02 (Option):                     T03 (Typestate):
  ──────────────                    ────────────────
  Campos obligatorios = Option<T>   Campos obligatorios = tipo generico
  Verificacion en runtime           Verificacion en compile time
  Error: Err("missing field")       Error: "method not found"
  El programa compila y falla       El programa NO compila
```

---

## 2. La idea central: estado en el tipo

El typestate pattern codifica el **estado** del builder en sus
**parametros de tipo**. Cada vez que seteas un campo obligatorio,
el tipo del builder cambia:

```
  Builder<(), (), ()>        ← nada seteado
      │
      ├─ .host("localhost")
      ▼
  Builder<HasHost, (), ()>   ← host seteado
      │
      ├─ .user("admin")
      ▼
  Builder<HasHost, HasUser, ()>   ← host + user
      │
      ├─ .password("secret")
      ▼
  Builder<HasHost, HasUser, HasPassword>  ← TODO seteado
      │
      └─ .build()  ← SOLO DISPONIBLE en este estado
```

`build()` solo se implementa para
`Builder<HasHost, HasUser, HasPassword>`. Si falta un campo,
el tipo es diferente y `build()` no existe para ese tipo.

---

## 3. Implementacion paso a paso

### 3.1 Marker types (estados)

```rust
// Marker types: no tienen datos, solo marcan estado
struct Missing;    // campo NO seteado
struct Provided;   // campo seteado

// Tambien puedes usar structs con nombre:
// struct NoHost;
// struct HasHost;
// Pero Missing/Provided son mas genericos.
```

### 3.2 El builder con type parameters

```rust
struct DatabaseBuilder<Host, User, Pass> {
    host: Option<String>,
    port: u16,
    name: String,
    user: Option<String>,
    password: Option<String>,
    pool_size: usize,
    // PhantomData para usar los type parameters
    _host: std::marker::PhantomData<Host>,
    _user: std::marker::PhantomData<User>,
    _pass: std::marker::PhantomData<Pass>,
}
```

### 3.3 Constructor: estado inicial Missing

```rust
impl DatabaseBuilder<Missing, Missing, Missing> {
    fn new(name: &str) -> Self {
        DatabaseBuilder {
            host: None,
            port: 5432,
            name: name.to_string(),
            user: None,
            password: None,
            pool_size: 10,
            _host: std::marker::PhantomData,
            _user: std::marker::PhantomData,
            _pass: std::marker::PhantomData,
        }
    }
}
```

### 3.4 Setters obligatorios: cambian el estado

```rust
// host: Missing → Provided
impl<U, P> DatabaseBuilder<Missing, U, P> {
    fn host(self, host: &str) -> DatabaseBuilder<Provided, U, P> {
        DatabaseBuilder {
            host: Some(host.to_string()),
            port: self.port,
            name: self.name,
            user: self.user,
            password: self.password,
            pool_size: self.pool_size,
            _host: std::marker::PhantomData,
            _user: std::marker::PhantomData,
            _pass: std::marker::PhantomData,
        }
    }
}

// user: Missing → Provided
impl<H, P> DatabaseBuilder<H, Missing, P> {
    fn user(self, user: &str) -> DatabaseBuilder<H, Provided, P> {
        DatabaseBuilder {
            host: self.host,
            port: self.port,
            name: self.name,
            user: Some(user.to_string()),
            password: self.password,
            pool_size: self.pool_size,
            _host: std::marker::PhantomData,
            _user: std::marker::PhantomData,
            _pass: std::marker::PhantomData,
        }
    }
}

// password: Missing → Provided
impl<H, U> DatabaseBuilder<H, U, Missing> {
    fn password(self, pass: &str) -> DatabaseBuilder<H, U, Provided> {
        DatabaseBuilder {
            host: self.host,
            port: self.port,
            name: self.name,
            user: self.user,
            password: Some(pass.to_string()),
            pool_size: self.pool_size,
            _host: std::marker::PhantomData,
            _user: std::marker::PhantomData,
            _pass: std::marker::PhantomData,
        }
    }
}
```

### 3.5 Setters opcionales: disponibles en cualquier estado

```rust
impl<H, U, P> DatabaseBuilder<H, U, P> {
    fn port(mut self, port: u16) -> Self {
        self.port = port;
        self
    }

    fn pool_size(mut self, n: usize) -> Self {
        self.pool_size = n;
        self
    }
}
```

### 3.6 build(): solo cuando todo esta Provided

```rust
#[derive(Debug)]
struct DatabaseConfig {
    host: String,
    port: u16,
    name: String,
    user: String,
    password: String,
    pool_size: usize,
}

impl DatabaseBuilder<Provided, Provided, Provided> {
    fn build(self) -> DatabaseConfig {
        DatabaseConfig {
            host: self.host.unwrap(),      // safe: Provided garantiza Some
            port: self.port,
            name: self.name,
            user: self.user.unwrap(),      // safe
            password: self.password.unwrap(), // safe
            pool_size: self.pool_size,
        }
    }
}
```

### 3.7 Uso

```rust
fn main() {
    // OK: todos los campos obligatorios seteados
    let cfg = DatabaseBuilder::new("myapp")
        .host("localhost")
        .user("admin")
        .password("secret")
        .port(3306)
        .pool_size(20)
        .build();

    println!("{:?}", cfg);

    // ORDEN LIBRE: los obligatorios se pueden setear en cualquier orden
    let cfg2 = DatabaseBuilder::new("myapp")
        .password("secret")
        .port(5432)
        .user("admin")
        .host("db.example.com")
        .build();

    // ERROR DE COMPILACION: falta password
    // let bad = DatabaseBuilder::new("myapp")
    //     .host("localhost")
    //     .user("admin")
    //     .build();
    // error[E0599]: no method named `build` found for struct
    // `DatabaseBuilder<Provided, Provided, Missing>`
}
```

```
  El mensaje de error del compilador:

  error[E0599]: no method named `build` found for struct
  `DatabaseBuilder<Provided, Provided, Missing>`
                                        ^^^^^^^ ← falta password

  El tipo DICE que campos faltan:
  - <Provided, Provided, Missing> = host OK, user OK, password FALTA
  - <Missing, Provided, Provided> = host FALTA
  - <Missing, Missing, Missing>   = todos faltan
```

---

## 4. Diagrama de transiciones de estado

```
  DatabaseBuilder<Missing, Missing, Missing>
       │         │         │
       │         │         └─ .password("x")
       │         │              ▼
       │         │         DB<Missing, Missing, Provided>
       │         │
       │         └─ .user("x")
       │              ▼
       │         DB<Missing, Provided, Missing>
       │              │
       │              └─ .password("x")
       │                    ▼
       │              DB<Missing, Provided, Provided>
       │
       └─ .host("x")
            ▼
       DB<Provided, Missing, Missing>
            │         │
            │         └─ .user("x")
            │              ▼
            │         DB<Provided, Provided, Missing>
            │              │
            │              └─ .password("x")
            │                    ▼
            │              DB<Provided, Provided, Provided>
            │                    │
            │                    └─ .build() ← SOLO AQUI
            │
            └─ .password("x")
                 ▼
            DB<Provided, Missing, Provided>
                 │
                 └─ .user("x")
                      ▼
                 DB<Provided, Provided, Provided>
                      │
                      └─ .build() ← AQUI TAMBIEN

  Los setters opcionales (.port, .pool_size) no cambian el estado.
  Estan disponibles en CUALQUIER nodo del diagrama.
```

---

## 5. Reducir boilerplate: macro helper

El boilerplate de reconstruir el struct en cada transicion es
repetitivo. Un macro puede ayudar:

```rust
macro_rules! transition {
    ($self:ident, $field:ident = $value:expr) => {
        DatabaseBuilder {
            $field: Some($value),
            host: $self.host,
            port: $self.port,
            name: $self.name,
            user: $self.user,
            password: $self.password,
            pool_size: $self.pool_size,
            _host: std::marker::PhantomData,
            _user: std::marker::PhantomData,
            _pass: std::marker::PhantomData,
        }
    };
}

impl<U, P> DatabaseBuilder<Missing, U, P> {
    fn host(self, host: &str) -> DatabaseBuilder<Provided, U, P> {
        transition!(self, host = host.to_string())
    }
}

impl<H, P> DatabaseBuilder<H, Missing, P> {
    fn user(self, user: &str) -> DatabaseBuilder<H, Provided, P> {
        transition!(self, user = user.to_string())
    }
}

impl<H, U> DatabaseBuilder<H, U, Missing> {
    fn password(self, pass: &str) -> DatabaseBuilder<H, U, Provided> {
        transition!(self, password = pass.to_string())
    }
}
```

---

## 6. Variante: almacenar el valor en el marker type

En lugar de `Option<T>` + `PhantomData`, puedes almacenar el
valor directamente en el type parameter:

```rust
// Markers que CONTIENEN el valor
struct NoHost;
struct Host(String);

struct NoUser;
struct User(String);

struct NoPass;
struct Pass(String);

struct DatabaseBuilder<H, U, P> {
    host: H,
    user: U,
    pass: P,
    port: u16,
    name: String,
    pool_size: usize,
}

impl DatabaseBuilder<NoHost, NoUser, NoPass> {
    fn new(name: &str) -> Self {
        DatabaseBuilder {
            host: NoHost,
            user: NoUser,
            pass: NoPass,
            port: 5432,
            name: name.to_string(),
            pool_size: 10,
        }
    }
}

impl<U, P> DatabaseBuilder<NoHost, U, P> {
    fn host(self, host: &str) -> DatabaseBuilder<Host, U, P> {
        DatabaseBuilder {
            host: Host(host.to_string()),
            user: self.user,
            pass: self.pass,
            port: self.port,
            name: self.name,
            pool_size: self.pool_size,
        }
    }
}

impl<H, P> DatabaseBuilder<H, NoUser, P> {
    fn user(self, user: &str) -> DatabaseBuilder<H, User, P> {
        DatabaseBuilder {
            host: self.host,
            user: User(user.to_string()),
            pass: self.pass,
            port: self.port,
            name: self.name,
            pool_size: self.pool_size,
        }
    }
}

impl<H, U> DatabaseBuilder<H, U, NoPass> {
    fn password(self, pass: &str) -> DatabaseBuilder<H, U, Pass> {
        DatabaseBuilder {
            host: self.host,
            user: self.user,
            pass: Pass(pass.to_string()),
            port: self.port,
            name: self.name,
            pool_size: self.pool_size,
        }
    }
}

impl DatabaseBuilder<Host, User, Pass> {
    fn build(self) -> DatabaseConfig {
        DatabaseConfig {
            host: self.host.0,       // Host(String) → .0
            port: self.port,
            name: self.name,
            user: self.user.0,       // User(String) → .0
            password: self.pass.0,   // Pass(String) → .0
            pool_size: self.pool_size,
        }
    }
}
```

```
  Ventajas de esta variante:
  - Sin Option<T>: no hay unwrap() en build()
  - Sin PhantomData: el marker es el campo
  - build() no puede fallar: todos los datos estan en el tipo
  - Mas eficiente: sin overhead de Option

  Desventaja:
  - Los setters opcionales necesitan un impl generico mas complejo
    (H, U, P pueden ser cualquier marker)
```

### 6.1 Setters opcionales con esta variante

```rust
// port y pool_size son opcionales: disponibles en cualquier estado
impl<H, U, P> DatabaseBuilder<H, U, P> {
    fn port(mut self, port: u16) -> Self {
        self.port = port;
        self
    }

    fn pool_size(mut self, n: usize) -> Self {
        self.pool_size = n;
        self
    }
}
```

---

## 7. Typestate para secuencias obligatorias

A veces los campos deben setearse en un **orden** especifico
(no libre). Typestate puede forzar esto:

```rust
// Un builder de conexion HTTP que debe seguir:
// 1. method + url  (obligatorio primero)
// 2. headers       (opcional, pero solo despues de url)
// 3. body          (solo para POST/PUT, solo despues de headers)
// 4. send()        (solo al final)

struct ReqInit;          // estado inicial
struct ReqWithUrl;       // url seteada
struct ReqReady;         // listo para enviar

struct RequestBuilder<State> {
    method: String,
    url: String,
    headers: Vec<(String, String)>,
    body: Option<String>,
    _state: std::marker::PhantomData<State>,
}

impl RequestBuilder<ReqInit> {
    fn get(url: &str) -> RequestBuilder<ReqWithUrl> {
        RequestBuilder {
            method: "GET".into(),
            url: url.into(),
            headers: Vec::new(),
            body: None,
            _state: std::marker::PhantomData,
        }
    }

    fn post(url: &str) -> RequestBuilder<ReqWithUrl> {
        RequestBuilder {
            method: "POST".into(),
            url: url.into(),
            headers: Vec::new(),
            body: None,
            _state: std::marker::PhantomData,
        }
    }
}

impl RequestBuilder<ReqWithUrl> {
    fn header(mut self, key: &str, val: &str) -> Self {
        self.headers.push((key.into(), val.into()));
        self
    }

    fn body(self, body: &str) -> RequestBuilder<ReqReady> {
        RequestBuilder {
            method: self.method,
            url: self.url,
            headers: self.headers,
            body: Some(body.into()),
            _state: std::marker::PhantomData,
        }
    }

    // GET sin body: puede enviar directamente
    fn send(self) -> String {
        format!("{} {} (no body)", self.method, self.url)
    }
}

impl RequestBuilder<ReqReady> {
    fn send(self) -> String {
        format!("{} {} body={:?}", self.method, self.url, self.body)
    }
}
```

```rust
// OK: GET sin body
let r = RequestBuilder::get("https://example.com")
    .header("Accept", "text/html")
    .send();

// OK: POST con body
let r = RequestBuilder::post("https://example.com/api")
    .header("Content-Type", "application/json")
    .body(r#"{"name":"Alice"}"#)
    .send();

// ERROR: no puedes llamar body() despues de send()
// ERROR: no puedes llamar send() sin url
// ERROR: no puedes agregar headers despues de body()
```

---

## 8. Ejemplo real: conexion TLS

```rust
// Estados de una conexion TLS
struct Disconnected;
struct Connected;
struct TlsHandshaked;
struct Authenticated;

struct Connection<State> {
    host: String,
    port: u16,
    stream: Option<String>,  // simulado
    _state: std::marker::PhantomData<State>,
}

impl Connection<Disconnected> {
    fn new(host: &str, port: u16) -> Self {
        Connection {
            host: host.into(),
            port,
            stream: None,
            _state: std::marker::PhantomData,
        }
    }

    fn connect(self) -> Result<Connection<Connected>, String> {
        println!("TCP connect to {}:{}", self.host, self.port);
        Ok(Connection {
            host: self.host,
            port: self.port,
            stream: Some("tcp_stream".into()),
            _state: std::marker::PhantomData,
        })
    }
}

impl Connection<Connected> {
    fn start_tls(self) -> Result<Connection<TlsHandshaked>, String> {
        println!("TLS handshake...");
        Ok(Connection {
            host: self.host,
            port: self.port,
            stream: Some("tls_stream".into()),
            _state: std::marker::PhantomData,
        })
    }
}

impl Connection<TlsHandshaked> {
    fn authenticate(self, user: &str, pass: &str)
        -> Result<Connection<Authenticated>, String>
    {
        println!("Auth as {user}...");
        Ok(Connection {
            host: self.host,
            port: self.port,
            stream: self.stream,
            _state: std::marker::PhantomData,
        })
    }
}

impl Connection<Authenticated> {
    fn query(&self, sql: &str) -> Result<String, String> {
        println!("Query: {sql}");
        Ok("result".into())
    }

    fn close(self) {
        println!("Closing connection");
    }
}
```

```rust
fn main() -> Result<(), String> {
    let result = Connection::new("db.example.com", 5432)
        .connect()?
        .start_tls()?
        .authenticate("admin", "secret")?
        .query("SELECT * FROM users")?;

    // ERROR: no puedes query() sin authenticate()
    // let bad = Connection::new("db.example.com", 5432)
    //     .connect()?
    //     .query("SELECT 1");
    // error: method `query` not found for `Connection<Connected>`

    // ERROR: no puedes authenticate() sin start_tls()
    // let bad = Connection::new("db.example.com", 5432)
    //     .connect()?
    //     .authenticate("admin", "secret");
    // error: method `authenticate` not found for `Connection<Connected>`

    Ok(())
}
```

```
  El tipo sistema fuerza el protocolo:
  Disconnected → connect() → Connected
  Connected → start_tls() → TlsHandshaked
  TlsHandshaked → authenticate() → Authenticated
  Authenticated → query() / close()

  Saltarse un paso = error de compilacion.
  En C o Java, esto seria un check en runtime (o un bug).
```

---

## 9. Typestate vs runtime validation

### 9.1 Cuando usar typestate

```
  Usar typestate cuando:
  ✓ Campos obligatorios son conocidos en compile time
  ✓ El orden de operaciones es un protocolo fijo
  ✓ La API es publica y quieres prevenir mal uso
  ✓ Los errores de "campo faltante" son comunes y costosos
  ✓ Pocos campos obligatorios (2-4)

  NO usar typestate cuando:
  ✗ Los campos obligatorios dependen de runtime (config, user input)
  ✗ Muchos campos obligatorios (>5 — explosion de type params)
  ✗ El builder es interno y solo lo usas tu
  ✗ La validacion es compleja (cruzada entre campos)
  ✗ El boilerplate no justifica el beneficio
```

### 9.2 Comparacion de enfoques

```
  Enfoque          Verificacion    Boilerplate   Error
  ───────          ────────────    ───────────   ─────
  Option + runtime Runtime         Bajo          Err("missing field")
  Typestate        Compile time    Alto          "method not found"
  new() params     Compile time    Minimo        "expected 3 args"

  Complejidad vs seguridad:

  new(host, user, pass)  ← Simple pero no escala (12 params = ilegible)
       │
       ▼
  Builder con Option     ← Escala bien, error en runtime
       │
       ▼
  Typestate builder      ← Escala OK (2-4 obligatorios), compile time
       │
       ▼
  Derive typestate       ← Escala bien, compile time, macro genera todo
```

### 9.3 Tabla de decision

| Campos obligatorios | Campos opcionales | Recomendacion |
|--------------------|--------------------|---------------|
| 0 | pocos | Struct + Default |
| 1-3 | 0 | Constructor directo |
| 1-3 | muchos | Typestate builder |
| 4+ | pocos | Constructor + config struct |
| 4+ | muchos | Builder con Option (T02) |
| Depende de runtime | cualquier | Builder con Option (T02) |

---

## 10. Limitaciones y workarounds

### 10.1 Explosion de type parameters

Con N campos obligatorios, tienes N type parameters:

```rust
struct Builder<A, B, C, D, E, F> { ... }
// 6 campos obligatorios = 6 type params
// 2^6 = 64 posibles combinaciones de estado
// Pero solo escribes N impl blocks (uno por campo) + 1 para build
```

El numero de impl blocks crece linealmente (N+1), no
exponencialmente. Pero la firma del tipo se vuelve larga.

### 10.2 Workaround: agrupar estados

```rust
// En lugar de un param por campo:
struct Builder<Host, User, Pass, Db, Port, Timeout> { ... }

// Agrupar en "fases":
struct Builder<AuthState, ConnState> { ... }

struct NoAuth;
struct HasAuth { user: String, pass: String }

struct NoConn;
struct HasConn { host: String, port: u16 }
```

### 10.3 El typestate NO puede verificar

```
  Typestate puede:
  ✓ Campo presente o ausente
  ✓ Orden de operaciones
  ✓ Transiciones de estado validas

  Typestate NO puede:
  ✗ Validar VALORES (port > 0, password no vacia)
  ✗ Validar combinaciones complejas (si TLS, entonces cert obligatorio)
  ✗ Condiciones que dependen de runtime

  Para valores invalidos, sigue necesitando validacion en build():
  fn build(self) -> Result<Config, ValidationError>
```

---

## 11. Comparacion C — Rust — Typestate

```
  Aspecto           C config struct      Rust Option builder  Typestate
  ───────           ───────────────      ───────────────────  ─────────
  Campo faltante    Runtime (NULL)       Runtime (Err)        Compile time
  Valor invalido    Runtime              Runtime              Runtime
  Orden forzado     Imposible            Imposible            Si
  Boilerplate       Bajo                 Medio                Alto
  Error message     "NULL pointer"       "missing field"      "method not found"
  Reutilizacion     Si (struct vive)     Si (&mut) o No       No (consumed)
  IDE support       Autocomplete campo   Autocomplete metodo  Solo metodos validos
  Curva aprendizaje Baja                 Baja                 Media
```

---

## Errores comunes

### Error 1 — Olvidar que el tipo cambia

```rust
let b = DatabaseBuilder::new("myapp");
// b: DatabaseBuilder<Missing, Missing, Missing>

let b = b.host("localhost");
// b: DatabaseBuilder<Provided, Missing, Missing>  ← TIPO DIFERENTE

// Esto NO compila:
// let b: DatabaseBuilder<Missing, Missing, Missing> = b;
// Los tipos no coinciden
```

Cada setter obligatorio retorna un tipo **diferente**. No puedes
asignar a la misma variable con type annotation fija. Usa
inferencia de tipos (sin annotation) o shadowing.

### Error 2 — Intentar llamar setter obligatorio dos veces

```rust
let b = DatabaseBuilder::new("myapp")
    .host("localhost")
    .host("db.example.com");  // ERROR
// `host` solo existe para DatabaseBuilder<Missing, _, _>
// Despues de .host(), el tipo es <Provided, _, _>
// y .host() no esta implementado para Provided
```

Esto es una **feature**, no un bug. Previene setear un campo
obligatorio dos veces (que podria ser un error logico).

Si quieres permitir re-setear, implementa `host` tambien para
`Provided`:

```rust
impl<U, P> DatabaseBuilder<Provided, U, P> {
    fn host(mut self, host: &str) -> Self {
        self.host = Some(host.to_string());
        self
    }
}
```

### Error 3 — Typestate para campos condicionales

```rust
// "Si use_tls es true, cert es obligatorio"
// Esto NO es typestate — es logica condicional de runtime
// Typestate no puede expresar: "si el campo X tiene valor Y,
// entonces campo Z es obligatorio"

// Solucion: usar dos builders o un enum de configuracion
enum TlsConfig {
    Disabled,
    Enabled { cert: String, key: String },
}
```

### Error 4 — Demasiados type parameters

```rust
// Esto se vuelve inmanejable:
struct Builder<Host, Port, User, Pass, Db, Pool, Timeout, Ssl, Cert, Key>
{ ... }

// Mejor: agrupar o usar Option builder (T02) para >4 obligatorios
```

---

## Ejercicios

### Ejercicio 1 — Typestate basico

Implementa un typestate builder para `EmailMessage` con:

- `from` (obligatorio)
- `to` (obligatorio)
- `subject` (obligatorio)
- `body` (opcional, default = "")

El builder debe rechazar `build()` si falta from, to, o subject.

**Prediccion**: Cuantos type parameters necesitas?

<details><summary>Respuesta</summary>

Tres type parameters (uno por campo obligatorio).

```rust
use std::marker::PhantomData;

struct Missing;
struct Set;

struct EmailBuilder<From, To, Subject> {
    from: Option<String>,
    to: Option<String>,
    subject: Option<String>,
    body: String,
    _from: PhantomData<From>,
    _to: PhantomData<To>,
    _subject: PhantomData<Subject>,
}

impl EmailBuilder<Missing, Missing, Missing> {
    fn new() -> Self {
        EmailBuilder {
            from: None, to: None, subject: None,
            body: String::new(),
            _from: PhantomData, _to: PhantomData,
            _subject: PhantomData,
        }
    }
}

macro_rules! transition {
    ($self:ident, $field:ident = $val:expr) => {
        EmailBuilder {
            $field: Some($val),
            from: $self.from, to: $self.to,
            subject: $self.subject, body: $self.body,
            _from: PhantomData, _to: PhantomData,
            _subject: PhantomData,
        }
    };
}

impl<T, S> EmailBuilder<Missing, T, S> {
    fn from(self, addr: &str) -> EmailBuilder<Set, T, S> {
        transition!(self, from = addr.to_string())
    }
}

impl<F, S> EmailBuilder<F, Missing, S> {
    fn to(self, addr: &str) -> EmailBuilder<F, Set, S> {
        transition!(self, to = addr.to_string())
    }
}

impl<F, T> EmailBuilder<F, T, Missing> {
    fn subject(self, subj: &str) -> EmailBuilder<F, T, Set> {
        transition!(self, subject = subj.to_string())
    }
}

// body es opcional: disponible en cualquier estado
impl<F, T, S> EmailBuilder<F, T, S> {
    fn body(mut self, body: &str) -> Self {
        self.body = body.to_string();
        self
    }
}

// build solo cuando todo esta Set
struct Email {
    from: String,
    to: String,
    subject: String,
    body: String,
}

impl EmailBuilder<Set, Set, Set> {
    fn build(self) -> Email {
        Email {
            from: self.from.unwrap(),
            to: self.to.unwrap(),
            subject: self.subject.unwrap(),
            body: self.body,
        }
    }
}

fn main() {
    let email = EmailBuilder::new()
        .from("alice@example.com")
        .to("bob@example.com")
        .subject("Hello")
        .body("How are you?")
        .build();

    // No compila: falta subject
    // let bad = EmailBuilder::new()
    //     .from("alice@example.com")
    //     .to("bob@example.com")
    //     .build();
}
```

</details>

---

### Ejercicio 2 — Typestate con valores en markers

Reimplementa el ejercicio 1 pero almacenando los valores
directamente en los marker types (variante seccion 6), sin
`Option<T>` ni `PhantomData`.

**Prediccion**: Necesitas `unwrap()` en build()?

<details><summary>Respuesta</summary>

```rust
struct NoFrom;
struct From(String);
struct NoTo;
struct To(String);
struct NoSubject;
struct Subject(String);

struct EmailBuilder<F, T, S> {
    from: F,
    to: T,
    subject: S,
    body: String,
}

impl EmailBuilder<NoFrom, NoTo, NoSubject> {
    fn new() -> Self {
        EmailBuilder {
            from: NoFrom, to: NoTo, subject: NoSubject,
            body: String::new(),
        }
    }
}

impl<T, S> EmailBuilder<NoFrom, T, S> {
    fn from(self, addr: &str) -> EmailBuilder<From, T, S> {
        EmailBuilder {
            from: From(addr.into()),
            to: self.to,
            subject: self.subject,
            body: self.body,
        }
    }
}

impl<F, S> EmailBuilder<F, NoTo, S> {
    fn to(self, addr: &str) -> EmailBuilder<F, To, S> {
        EmailBuilder {
            from: self.from,
            to: To(addr.into()),
            subject: self.subject,
            body: self.body,
        }
    }
}

impl<F, T> EmailBuilder<F, T, NoSubject> {
    fn subject(self, s: &str) -> EmailBuilder<F, T, Subject> {
        EmailBuilder {
            from: self.from,
            to: self.to,
            subject: Subject(s.into()),
            body: self.body,
        }
    }
}

impl<F, T, S> EmailBuilder<F, T, S> {
    fn body(mut self, b: &str) -> Self {
        self.body = b.into();
        self
    }
}

struct Email {
    from: String,
    to: String,
    subject: String,
    body: String,
}

impl EmailBuilder<From, To, Subject> {
    fn build(self) -> Email {
        Email {
            from: self.from.0,      // From(String) → .0
            to: self.to.0,
            subject: self.subject.0,
            body: self.body,
        }
    }
}
```

No, no necesitas `unwrap()`. Los datos estan directamente en
los marker types (`From(String)`, `To(String)`). En el estado
`<From, To, Subject>`, los datos siempre estan presentes por
construccion. build() es infallible.

</details>

---

### Ejercicio 3 — Typestate con secuencia obligatoria

Implementa un builder de `HttpRequest` que fuerce el orden:

1. `method + url` (al crear)
2. `headers` (opcionales, acumulativos)
3. `body` (transiciona a estado final)
4. `send()` (solo despues de body o directamente despues de headers)

**Prediccion**: Cuantos estados necesitas?

<details><summary>Respuesta</summary>

Tres estados: `Building`, `HasBody`, y opcionalmente `Sent`.

```rust
struct Building;
struct HasBody;

struct Request<State> {
    method: String,
    url: String,
    headers: Vec<(String, String)>,
    body: Option<String>,
    _state: PhantomData<State>,
}

impl Request<Building> {
    fn get(url: &str) -> Self {
        Request {
            method: "GET".into(), url: url.into(),
            headers: Vec::new(), body: None,
            _state: PhantomData,
        }
    }

    fn post(url: &str) -> Self {
        Request {
            method: "POST".into(), url: url.into(),
            headers: Vec::new(), body: None,
            _state: PhantomData,
        }
    }

    fn header(mut self, key: &str, val: &str) -> Self {
        self.headers.push((key.into(), val.into()));
        self
    }

    fn body(self, body: &str) -> Request<HasBody> {
        Request {
            method: self.method, url: self.url,
            headers: self.headers,
            body: Some(body.into()),
            _state: PhantomData,
        }
    }

    // GET puede enviar sin body
    fn send(self) -> String {
        format!("{} {} headers={}", self.method, self.url,
                self.headers.len())
    }
}

impl Request<HasBody> {
    // Solo despues de body
    fn send(self) -> String {
        format!("{} {} body={:?}", self.method, self.url,
                self.body.unwrap())
    }
}

fn main() {
    // GET sin body
    let r = Request::get("https://example.com")
        .header("Accept", "text/html")
        .send();
    println!("{r}");

    // POST con body
    let r = Request::post("https://api.example.com")
        .header("Content-Type", "application/json")
        .body(r#"{"x":1}"#)
        .send();
    println!("{r}");

    // No puedes agregar headers despues de body:
    // Request::post("url").body("x").header("k", "v");
    // error: method `header` not found for `Request<HasBody>`
}
```

Dos estados son suficientes: `Building` (pre-body) y `HasBody`
(post-body). `send()` esta disponible en ambos (GET puede no
tener body). `header()` solo esta en `Building`.

</details>

---

### Ejercicio 4 — Protocolo de conexion

Implementa un typestate para un protocolo SMTP simplificado:

```
Disconnected → connect() → Connected
Connected → ehlo() → Ready
Ready → auth() → Authenticated
Authenticated → send_mail() → (sigue Authenticated)
Authenticated → quit() → Disconnected
```

**Prediccion**: `send_mail()` puede llamarse multiples veces?

<details><summary>Respuesta</summary>

```rust
struct Disconnected;
struct Connected;
struct Ready;
struct Authenticated;

struct SmtpClient<State> {
    host: String,
    _state: PhantomData<State>,
}

impl SmtpClient<Disconnected> {
    fn new(host: &str) -> Self {
        SmtpClient { host: host.into(), _state: PhantomData }
    }

    fn connect(self) -> Result<SmtpClient<Connected>, String> {
        println!("Connecting to {}...", self.host);
        Ok(SmtpClient { host: self.host, _state: PhantomData })
    }
}

impl SmtpClient<Connected> {
    fn ehlo(self, domain: &str) -> Result<SmtpClient<Ready>, String> {
        println!("EHLO {domain}");
        Ok(SmtpClient { host: self.host, _state: PhantomData })
    }
}

impl SmtpClient<Ready> {
    fn auth(self, user: &str, pass: &str)
        -> Result<SmtpClient<Authenticated>, String>
    {
        println!("AUTH LOGIN {user}");
        Ok(SmtpClient { host: self.host, _state: PhantomData })
    }
}

impl SmtpClient<Authenticated> {
    fn send_mail(&self, from: &str, to: &str, body: &str)
        -> Result<(), String>
    {
        println!("MAIL FROM:<{from}>");
        println!("RCPT TO:<{to}>");
        println!("DATA: {body}");
        Ok(())
    }

    fn quit(self) -> SmtpClient<Disconnected> {
        println!("QUIT");
        SmtpClient { host: self.host, _state: PhantomData }
    }
}

fn main() -> Result<(), String> {
    let client = SmtpClient::new("smtp.example.com")
        .connect()?
        .ehlo("mydomain.com")?
        .auth("user", "pass")?;

    // Puede enviar multiples mails
    client.send_mail("me@x.com", "you@y.com", "Hello")?;
    client.send_mail("me@x.com", "other@z.com", "Hi")?;

    let _disconnected = client.quit();

    Ok(())
}
```

Si, `send_mail()` toma `&self` (no consume), asi que puede
llamarse multiples veces. El estado sigue siendo `Authenticated`
despues de cada envio.

`quit()` toma `self` (consume), transicionando a `Disconnected`.
Despues de quit, no puedes llamar send_mail — el compilador lo
impide.

</details>

---

### Ejercicio 5 — Comparar los tres enfoques

Escribe la misma validacion "host + user + password obligatorios"
con tres enfoques:

1. Constructor con parametros
2. Builder con Option (T02)
3. Typestate builder (T03)

Para cada uno, muestra que pasa si olvidas password.

<details><summary>Respuesta</summary>

```rust
// ── 1. Constructor con parametros ──
struct Config1 {
    host: String, user: String, password: String, port: u16,
}
impl Config1 {
    fn new(host: &str, user: &str, password: &str) -> Self {
        Config1 {
            host: host.into(), user: user.into(),
            password: password.into(), port: 5432,
        }
    }
}

// Sin password: ERROR DE COMPILACION
// Config1::new("localhost", "admin");
// error: this function takes 3 arguments but 2 were supplied


// ── 2. Builder con Option (T02) ──
struct Config2Builder {
    host: Option<String>, user: Option<String>,
    password: Option<String>, port: u16,
}
impl Config2Builder {
    fn new() -> Self {
        Config2Builder {
            host: None, user: None, password: None, port: 5432,
        }
    }
    fn host(mut self, h: &str) -> Self { self.host = Some(h.into()); self }
    fn user(mut self, u: &str) -> Self { self.user = Some(u.into()); self }
    fn password(mut self, p: &str) -> Self { self.password = Some(p.into()); self }
    fn build(self) -> Result<Config1, String> {
        Ok(Config1 {
            host: self.host.ok_or("host required")?,
            user: self.user.ok_or("user required")?,
            password: self.password.ok_or("password required")?,
            port: self.port,
        })
    }
}

// Sin password: COMPILA, falla en RUNTIME
// Config2Builder::new().host("x").user("y").build()
// → Err("password required")


// ── 3. Typestate builder (T03) ──
// (usando la implementacion de seccion 3-6)

// Sin password: ERROR DE COMPILACION
// DatabaseBuilder::new("db").host("x").user("y").build()
// error: no method named `build` found for
//   `DatabaseBuilder<Provided, Provided, Missing>`
```

| Enfoque | Sin password | Cuando detecta |
|---------|-------------|---------------|
| Constructor | "takes 3 args, 2 supplied" | Compile time |
| Option builder | Err("password required") | Runtime |
| Typestate | "method build not found for ..Missing" | Compile time |

El constructor y typestate detectan en compile time. La
diferencia: el constructor no escala (12 params = ilegible),
el typestate si (obligatorios en setters, opcionales separados).

</details>

---

### Ejercicio 6 — Typestate con sealed traits

Usa sealed traits para prevenir que usuarios externos creen sus
propios estados:

```rust
mod sealed {
    pub trait FieldState {}
}
struct Missing;
struct Provided;
impl sealed::FieldState for Missing {}
impl sealed::FieldState for Provided {}
```

**Prediccion**: Por que querrías sellar los estados?

<details><summary>Respuesta</summary>

```rust
mod builder {
    // Sealed trait: trait publico pero no implementable fuera
    mod sealed {
        pub trait FieldState {}
    }

    pub struct Missing;
    pub struct Provided;

    impl sealed::FieldState for Missing {}
    impl sealed::FieldState for Provided {}

    pub struct Builder<H: sealed::FieldState, U: sealed::FieldState> {
        host: Option<String>,
        user: Option<String>,
        port: u16,
        _h: std::marker::PhantomData<H>,
        _u: std::marker::PhantomData<U>,
    }

    impl Builder<Missing, Missing> {
        pub fn new() -> Self {
            Builder {
                host: None, user: None, port: 5432,
                _h: std::marker::PhantomData,
                _u: std::marker::PhantomData,
            }
        }
    }

    impl<U: sealed::FieldState> Builder<Missing, U> {
        pub fn host(self, h: &str) -> Builder<Provided, U> {
            Builder {
                host: Some(h.into()), user: self.user, port: self.port,
                _h: std::marker::PhantomData, _u: std::marker::PhantomData,
            }
        }
    }

    impl<H: sealed::FieldState> Builder<H, Missing> {
        pub fn user(self, u: &str) -> Builder<H, Provided> {
            Builder {
                host: self.host, user: Some(u.into()), port: self.port,
                _h: std::marker::PhantomData, _u: std::marker::PhantomData,
            }
        }
    }

    impl Builder<Provided, Provided> {
        pub fn build(self) -> String {
            format!("{}@{}:{}", self.user.unwrap(),
                    self.host.unwrap(), self.port)
        }
    }
}

// Fuera del modulo:
fn main() {
    let cfg = builder::Builder::new()
        .host("localhost")
        .user("admin")
        .build();
    println!("{cfg}");

    // No puedes hacer:
    // struct Hacked;
    // impl builder::sealed::FieldState for Hacked {}
    // error: trait `FieldState` is private
}
```

Sellar los estados previene que un usuario externo cree un
estado falso y salte la validacion:

```rust
// Sin sealed, alguien podria hacer:
struct FakeProvided;
impl FieldState for FakeProvided {}

// Y crear un Builder<FakeProvided, FakeProvided> sin setear nada
// Luego llamar .build() — los campos serian None → panic
```

Con sealed traits, solo `Missing` y `Provided` (definidos en tu
modulo) pueden ser estados. Nadie externo puede inventar estados.

</details>

---

### Ejercicio 7 — Typestate para file operations

Implementa un typestate para un archivo que fuerza:
- `open()` antes de `read()/write()`
- No puedes `write()` si se abrio como readonly
- `close()` consume el handle

```
Closed → open_read() → OpenRead
Closed → open_write() → OpenWrite
OpenRead → read() → OpenRead
OpenWrite → write() → OpenWrite
OpenRead/OpenWrite → close() → Closed
```

**Prediccion**: Necesitas un trait compartido para read y write
operations?

<details><summary>Respuesta</summary>

```rust
struct Closed;
struct OpenRead;
struct OpenWrite;

struct File<State> {
    path: String,
    _state: PhantomData<State>,
}

impl File<Closed> {
    fn new(path: &str) -> Self {
        File { path: path.into(), _state: PhantomData }
    }

    fn open_read(self) -> Result<File<OpenRead>, String> {
        println!("Opening {} for reading", self.path);
        Ok(File { path: self.path, _state: PhantomData })
    }

    fn open_write(self) -> Result<File<OpenWrite>, String> {
        println!("Opening {} for writing", self.path);
        Ok(File { path: self.path, _state: PhantomData })
    }
}

impl File<OpenRead> {
    fn read(&self, buf: &mut [u8]) -> Result<usize, String> {
        println!("Reading from {}", self.path);
        Ok(0)
    }

    fn close(self) -> File<Closed> {
        println!("Closing {} (read)", self.path);
        File { path: self.path, _state: PhantomData }
    }
}

impl File<OpenWrite> {
    fn write(&self, data: &[u8]) -> Result<usize, String> {
        println!("Writing {} bytes to {}", data.len(), self.path);
        Ok(data.len())
    }

    fn close(self) -> File<Closed> {
        println!("Closing {} (write)", self.path);
        File { path: self.path, _state: PhantomData }
    }
}

fn main() -> Result<(), String> {
    // Leer
    let f = File::new("data.txt").open_read()?;
    let mut buf = [0u8; 1024];
    f.read(&mut buf)?;
    f.read(&mut buf)?;  // multiples reads OK
    let _closed = f.close();

    // Escribir
    let f = File::new("out.txt").open_write()?;
    f.write(b"hello")?;
    let _closed = f.close();

    // ERROR: no puedes write en OpenRead
    // let f = File::new("x").open_read()?;
    // f.write(b"data");
    // error: method `write` not found for `File<OpenRead>`

    // ERROR: no puedes read/write despues de close
    // let f = File::new("x").open_read()?;
    // let closed = f.close();
    // closed.read(&mut buf);
    // error: method `read` not found for `File<Closed>`

    Ok(())
}
```

No necesitas un trait compartido. `read` y `write` son metodos
separados en estados separados. `close()` existe en ambos estados
pero se implementa independientemente (o con un trait `Closeable`
si quieres compartir logica).

Si quisieras una funcion generica que trabaje con "cualquier
archivo abierto", necesitarias un trait:
```rust
trait OpenFile { fn path(&self) -> &str; }
impl OpenFile for File<OpenRead> { fn path(&self) -> &str { &self.path } }
impl OpenFile for File<OpenWrite> { fn path(&self) -> &str { &self.path } }
```

</details>

---

### Ejercicio 8 — Medir el costo del typestate

El typestate genera multiples tipos a nivel de compilacion.
Responde:

1. Cuantos tipos genera `DatabaseBuilder<H, U, P>` con 2
   posibles estados por parametro?
2. Cuantos tipos se instancian realmente en un uso tipico?
3. Hay overhead en runtime?

<details><summary>Respuesta</summary>

1. **Tipos posibles**: 2^3 = 8 combinaciones:
   ```
   <Missing, Missing, Missing>
   <Provided, Missing, Missing>
   <Missing, Provided, Missing>
   <Missing, Missing, Provided>
   <Provided, Provided, Missing>
   <Provided, Missing, Provided>
   <Missing, Provided, Provided>
   <Provided, Provided, Provided>
   ```

2. **Tipos instanciados** en un uso tipico: solo los que aparecen
   en el codigo. Si el uso es:
   ```rust
   Builder::new()          // <M, M, M>
       .host("x")          // <P, M, M>
       .user("y")          // <P, P, M>
       .password("z")      // <P, P, P>
       .build()
   ```
   Se instancian **4 tipos** (los 4 del camino). Los otros 4
   nunca se generan.

3. **Overhead en runtime: CERO.** PhantomData tiene tamano 0.
   Los marker types (Missing, Provided) tienen tamano 0.
   Despues de monomorphization, el codigo generado es identico
   al builder sin typestate. Las transiciones son moves (no
   copias) — el optimizer las elimina completamente.

   ```
   size_of::<DatabaseBuilder<Missing, Missing, Missing>>()
   == size_of::<DatabaseBuilder<Provided, Provided, Provided>>()
   == size_of los campos reales (sin overhead)
   ```

   El typestate es **purely compile-time** — zero-cost abstraction.

</details>

---

### Ejercicio 9 — Cuando NO usar typestate

Para cada escenario, indica si typestate es apropiado o excesivo:

1. Un builder con 2 campos obligatorios y 3 opcionales
2. Un builder con 8 campos obligatorios
3. Un builder donde los campos obligatorios dependen de una flag
4. Un protocolo de red con 5 pasos secuenciales
5. Un CRUD API interno usado en 3 lugares

<details><summary>Respuesta</summary>

| Escenario | Typestate? | Razon |
|-----------|-----------|-------|
| 1. 2 obligatorios + 3 opcionales | **Si** | Caso ideal: pocos type params, beneficio claro |
| 2. 8 campos obligatorios | **No** | 8 type params = ilegible. Usar constructor o Option builder |
| 3. Obligatorios condicionales | **No** | Typestate es estatico. Si "obligatorio" depende de runtime, usar Option + validacion |
| 4. Protocolo de 5 pasos | **Si** | Caso ideal: estados lineales, orden obligatorio, errores costosos si se viola |
| 5. CRUD interno en 3 lugares | **No** | Overkill. Solo 3 call sites — si hay un bug, lo encuentras rapido. El boilerplate no se justifica |

Regla general:
- **Si**: API publica + pocos campos obligatorios o protocolo con orden
- **No**: API interna + muchos campos o logica condicional
- **Depende**: considerar si el costo del boilerplate (escribir
  y mantener) se justifica por la seguridad que ganas

</details>

---

### Ejercicio 10 — Reflexion: typestate en el ecosistema

Responde sin ver la respuesta:

1. El typestate es una innovacion de Rust o existia antes?
   Que feature del lenguaje lo hace practico?

2. Si pudieras agregar UNA feature a C para permitir typestate,
   cual seria?

3. Nombra un trade-off que typestate introduce que los otros
   enfoques de builder no tienen.

4. Si tu equipo no conoce typestate y ven
   `Builder<Provided, Missing, Provided>` en un error, lo
   entenderan? Que harias para mejorar la experiencia?

<details><summary>Respuesta</summary>

**1. Historia:**

El concepto de typestate viene de los anos 80 (Strom & Yemini,
1986). No es nuevo. Pero antes de Rust, era impractico en la
mayoria de lenguajes:

- **C**: sin generics → imposible
- **Java/C#**: generics con erasure → overhead de runtime
- **C++**: templates permiten typestate pero el syntax es horrible
- **Haskell**: phantom types lo permiten pero sin move semantics
  no puedes prevenir reutilizacion

Lo que hace typestate practico en **Rust**:
1. **Generics** sin runtime overhead (monomorphization)
2. **Move semantics**: el builder se consume, no se puede reutilizar
3. **PhantomData**: zero-cost type-level marker
4. **Errores legibles**: "method not found for Type<State>"

**2. Feature de C para typestate:**

**Generics** (parametric polymorphism). Sin ellos, no puedes
parametrizar un struct por su estado. Con generics, podrias
escribir `ServerBuilder<MISSING, PROVIDED>` y restringir `build`
a `<PROVIDED, PROVIDED>`.

C11 `_Generic` no alcanza — es selection, no parametrizacion.
Necesitarias algo como templates de C++, pero con la semantica
de C.

**3. Trade-off unico del typestate:**

**Complejidad del tipo en mensajes de error.** Los otros builders
dan errores como `Err("missing field")` — claro y directo. El
typestate da errores como:

```
method `build` not found for struct
`DatabaseBuilder<Provided, Missing, Provided>`
```

Para un desarrollador que no conoce el patron, esto es crptico.
`Missing` y `Provided` no dicen que campo falta — necesitas contar
la posicion del type parameter y mapearlo al campo.

**4. Mejorar la experiencia:**

- **Documentar** el patron en el README del crate: "build() solo
  esta disponible cuando host, user, y password estan seteados."
- **Nombrar** los markers con nombres descriptivos:
  `Builder<NoHost, HasUser, NoPassword>` en lugar de
  `Builder<Missing, Provided, Missing>`.
- **Custom error message** (nightly): `#[diagnostic::on_unimplemented]`
  para dar un mensaje mas claro:
  ```
  "cannot build: host has not been set. Call .host() first."
  ```
- **Ejemplo** en la documentacion mostrando el error y como
  resolverlo.

</details>
