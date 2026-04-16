# T03 — Decorator Funcional

---

## 1. De structs a funciones

T01 y T02 usaron structs con vtable/trait para decorar. Pero
un decorator es, en esencia, **una funcion que envuelve otra
funcion**. Si la "interfaz" es una firma de funcion, no
necesitas structs:

```
  T01/T02 (struct decorator):
  ───────────────────────────
  trait Handler { fn handle(&self, req: Request) -> Response; }
  struct LogDecorator { inner: Box<dyn Handler> }
  impl Handler for LogDecorator { ... }

  T03 (function decorator):
  ─────────────────────────
  type Handler = fn(Request) -> Response;
  fn with_logging(inner: Handler) -> Handler { ... }
```

```
  Struct decorator:              Functional decorator:
  ─────────────────              ─────────────────────
  Un struct por decorator        Una funcion/closure por decorator
  impl Trait para cada uno       Composicion de funciones
  Boilerplate moderado           Minimo boilerplate
  Estado facil (campos)          Estado via captures
  Bueno para APIs complejas      Bueno para pipelines simples
```

---

## 2. Decorator como higher-order function

Una funcion que recibe una funcion y retorna una funcion
modificada:

```rust
fn add_one(x: i32) -> i32 {
    x + 1
}

/// Decorator: loggea la entrada y salida de cualquier fn(i32) -> i32
fn with_logging(f: fn(i32) -> i32) -> impl Fn(i32) -> i32 {
    move |x| {
        println!("Input: {}", x);
        let result = f(x);
        println!("Output: {}", result);
        result
    }
}

/// Decorator: duplica el resultado
fn with_double(f: fn(i32) -> i32) -> impl Fn(i32) -> i32 {
    move |x| {
        f(x) * 2
    }
}

fn main() {
    let logged = with_logging(add_one);
    logged(5);
    // Input: 5
    // Output: 6

    let doubled = with_double(add_one);
    println!("{}", doubled(5));  // 12 (6 * 2)
}
```

```
  with_logging(add_one)
  │
  └─ retorna un closure que:
     1. Imprime el input
     2. Llama add_one(input)
     3. Imprime el output
     4. Retorna el output

  Es exactamente el patron decorator:
  misma "interfaz" (fn(i32)->i32), comportamiento extra.
```

---

## 3. Composicion de closures

Para encadenar decoradores funcionales, componemos closures:

```rust
/// Handler type: recibe request string, retorna response string
type Handler = Box<dyn Fn(&str) -> String>;

/// Middleware type: transforma un handler en otro handler
type Middleware = Box<dyn Fn(Handler) -> Handler>;

fn logging_middleware() -> Middleware {
    Box::new(|next: Handler| -> Handler {
        Box::new(move |req: &str| -> String {
            println!("[LOG] Request: {}", req);
            let resp = next(req);
            println!("[LOG] Response: {}", resp);
            resp
        })
    })
}

fn uppercase_middleware() -> Middleware {
    Box::new(|next: Handler| -> Handler {
        Box::new(move |req: &str| -> String {
            let resp = next(req);
            resp.to_uppercase()
        })
    })
}

fn prefix_middleware(prefix: String) -> Middleware {
    Box::new(move |next: Handler| -> Handler {
        let prefix = prefix.clone();
        Box::new(move |req: &str| -> String {
            let resp = next(req);
            format!("{}{}", prefix, resp)
        })
    })
}

/// Handler base: echo
fn echo_handler() -> Handler {
    Box::new(|req: &str| -> String {
        format!("Echo: {}", req)
    })
}

/// Aplicar una lista de middlewares a un handler
fn apply_middlewares(handler: Handler, middlewares: Vec<Middleware>) -> Handler {
    let mut h = handler;
    for mw in middlewares.into_iter().rev() {
        h = mw(h);
    }
    h
}

fn main() {
    let handler = echo_handler();
    let middlewares = vec![
        logging_middleware(),
        uppercase_middleware(),
        prefix_middleware(">>> ".to_string()),
    ];

    let decorated = apply_middlewares(handler, middlewares);
    let response = decorated("hello");
    println!("Final: {}", response);

    // [LOG] Request: hello
    // [LOG] Response: >>> ECHO: HELLO
    // Final: >>> ECHO: HELLO
}
```

```
  Flujo del request a traves de la cadena:
  ─────────────────────────────────────────

  decorated("hello")
  │
  ├─ logging_middleware (exterior)
  │   println!("[LOG] Request: hello")
  │   │
  │   ├─ uppercase_middleware
  │   │   │
  │   │   ├─ prefix_middleware
  │   │   │   │
  │   │   │   ├─ echo_handler
  │   │   │   │   return "Echo: hello"
  │   │   │   │
  │   │   │   return ">>> Echo: hello"
  │   │   │
  │   │   return ">>> ECHO: HELLO"
  │   │
  │   println!("[LOG] Response: >>> ECHO: HELLO")
  │   return ">>> ECHO: HELLO"
```

---

## 4. Middleware pattern (web server style)

El patron mas importante del decorator funcional: middleware
en web servers. Cada middleware procesa el request, opcionalmente
lo modifica, y lo pasa al siguiente:

```rust
use std::collections::HashMap;

#[derive(Debug, Clone)]
struct Request {
    path: String,
    method: String,
    headers: HashMap<String, String>,
    body: String,
}

#[derive(Debug, Clone)]
struct Response {
    status: u16,
    headers: HashMap<String, String>,
    body: String,
}

impl Response {
    fn ok(body: &str) -> Self {
        Self {
            status: 200,
            headers: HashMap::new(),
            body: body.to_string(),
        }
    }

    fn error(status: u16, msg: &str) -> Self {
        Self {
            status,
            headers: HashMap::new(),
            body: msg.to_string(),
        }
    }
}

/// Handler: funcion que procesa un request
type HandlerFn = Box<dyn Fn(Request) -> Response>;

/// Middleware: funcion que envuelve un handler
type MiddlewareFn = Box<dyn Fn(HandlerFn) -> HandlerFn>;

// ─── Middlewares ─────────────────────────────────────

fn logging() -> MiddlewareFn {
    Box::new(|next: HandlerFn| -> HandlerFn {
        Box::new(move |req: Request| -> Response {
            println!("--> {} {}", req.method, req.path);
            let resp = next(req);
            println!("<-- {}", resp.status);
            resp
        })
    })
}

fn auth_check(valid_token: String) -> MiddlewareFn {
    Box::new(move |next: HandlerFn| -> HandlerFn {
        let token = valid_token.clone();
        Box::new(move |req: Request| -> Response {
            match req.headers.get("Authorization") {
                Some(t) if t == &token => next(req),
                _ => Response::error(401, "Unauthorized"),
            }
        })
    })
}

fn cors(allowed_origin: String) -> MiddlewareFn {
    Box::new(move |next: HandlerFn| -> HandlerFn {
        let origin = allowed_origin.clone();
        Box::new(move |req: Request| -> Response {
            let mut resp = next(req);
            resp.headers.insert(
                "Access-Control-Allow-Origin".to_string(),
                origin.clone(),
            );
            resp
        })
    })
}

fn rate_limiter(max_per_sec: usize) -> MiddlewareFn {
    use std::sync::{Arc, Mutex};
    use std::time::Instant;

    let state = Arc::new(Mutex::new((Instant::now(), 0usize)));

    Box::new(move |next: HandlerFn| -> HandlerFn {
        let state = Arc::clone(&state);
        Box::new(move |req: Request| -> Response {
            let mut guard = state.lock().unwrap();
            let now = Instant::now();
            if now.duration_since(guard.0).as_secs() >= 1 {
                guard.0 = now;
                guard.1 = 0;
            }
            guard.1 += 1;
            if guard.1 > max_per_sec {
                return Response::error(429, "Too Many Requests");
            }
            drop(guard);  // Liberar lock antes de next()
            next(req)
        })
    })
}

// ─── Aplicar middlewares ─────────────────────────────

fn build_handler(
    base: HandlerFn,
    middlewares: Vec<MiddlewareFn>,
) -> HandlerFn {
    let mut handler = base;
    for mw in middlewares.into_iter().rev() {
        handler = mw(handler);
    }
    handler
}

fn main() {
    // Handler base
    let handler: HandlerFn = Box::new(|req: Request| {
        Response::ok(&format!("Hello from {}", req.path))
    });

    // Stack de middlewares
    let handler = build_handler(handler, vec![
        logging(),
        rate_limiter(100),
        auth_check("secret-token".to_string()),
        cors("*".to_string()),
    ]);

    // Simular requests
    let mut req = Request {
        path: "/api/users".into(),
        method: "GET".into(),
        headers: HashMap::new(),
        body: String::new(),
    };
    req.headers.insert("Authorization".into(), "secret-token".into());

    let resp = handler(req);
    println!("Response: {} - {}", resp.status, resp.body);
}
```

```
  Request flow:
  ─────────────

  handler(req)
  │
  ├─ logging()
  │   print "--> GET /api/users"
  │   │
  │   ├─ rate_limiter(100)
  │   │   check count < 100 → ok
  │   │   │
  │   │   ├─ auth_check("secret-token")
  │   │   │   check Authorization header → match
  │   │   │   │
  │   │   │   ├─ cors("*")
  │   │   │   │   │
  │   │   │   │   ├─ base handler
  │   │   │   │   │   return 200 "Hello from /api/users"
  │   │   │   │   │
  │   │   │   │   add CORS header
  │   │   │   │   return response
  │   │   │   │
  │   │   │   return response
  │   │   │
  │   │   return response
  │   │
  │   print "<-- 200"
  │   return response

  Cada middleware puede:
  1. Modificar el request antes de pasar (pre-processing)
  2. Decidir no pasar (short-circuit: auth, rate limit)
  3. Modificar el response despues (post-processing: CORS)
  4. Ambos (logging: antes y despues)
```

---

## 5. Decorator funcional en C: function pointers

C no tiene closures, pero el patron funciona con function
pointers y `void *context`:

```c
/* middleware.h */
typedef struct Request  Request;
typedef struct Response Response;

typedef Response (*HandlerFunc)(const Request *req, void *ctx);

typedef struct {
    HandlerFunc func;
    void       *ctx;
} Handler;

Response handler_call(Handler *h, const Request *req);

/* Middleware: toma un handler, retorna un handler "decorado" */
typedef Handler (*MiddlewareFunc)(Handler inner, void *mw_ctx);
```

```c
/* logging_mw.c */
typedef struct {
    Handler inner;
} LogCtx;

static Response logging_handler(const Request *req, void *ctx) {
    LogCtx *lc = ctx;
    printf("--> %s %s\n", req->method, req->path);
    Response resp = handler_call(&lc->inner, req);
    printf("<-- %d\n", resp.status);
    return resp;
}

Handler logging_middleware(Handler inner, void *mw_ctx) {
    (void)mw_ctx;
    LogCtx *lc = malloc(sizeof(*lc));
    lc->inner = inner;
    return (Handler){ .func = logging_handler, .ctx = lc };
}
```

```
  C vs Rust:

  C:     function pointer + void *ctx       = closure manual
  Rust:  Box<dyn Fn(Request) -> Response>   = closure automatico

  C: cada middleware necesita un struct XxxCtx + un static function
  Rust: un closure captura lo que necesita automaticamente

  El patron es IDENTICO conceptualmente.
  Rust solo tiene menos boilerplate.
```

---

## 6. Composicion de funciones puras

Para transformaciones de datos sin side effects:

```rust
/// Componer dos funciones: f(g(x))
fn compose<A, B, C>(
    f: impl Fn(B) -> C + 'static,
    g: impl Fn(A) -> B + 'static,
) -> impl Fn(A) -> C {
    move |x| f(g(x))
}

fn double(x: i32) -> i32 { x * 2 }
fn add_ten(x: i32) -> i32 { x + 10 }
fn to_string(x: i32) -> String { format!("result: {}", x) }

fn main() {
    // Componer: to_string(add_ten(double(x)))
    let pipeline = compose(to_string, compose(add_ten, double));
    println!("{}", pipeline(5));  // "result: 20"
    // 5 → double → 10 → add_ten → 20 → to_string → "result: 20"
}
```

### 6.1 Pipeline con metodo chain

Mas idiomatico en Rust — usar iterators y method chaining:

```rust
fn process(data: &[i32]) -> Vec<String> {
    data.iter()
        .filter(|&&x| x > 0)           // Decorator: filtrar
        .map(|&x| x * 2)               // Decorator: transformar
        .map(|x| x + 10)               // Decorator: transformar
        .map(|x| format!("val={}", x)) // Decorator: formatear
        .collect()
}

fn main() {
    let data = vec![-1, 3, -2, 5, 0, 7];
    let result = process(&data);
    println!("{:?}", result);
    // ["val=16", "val=20", "val=24"]
}
```

Cada `.filter()`, `.map()` es un decorator funcional sobre el
iterador anterior. La cadena es lazy — solo se ejecuta en
`.collect()`.

---

## 7. Patron pipe (builder de pipeline)

Construir un pipeline de transformaciones de forma fluida:

```rust
struct Pipeline<T> {
    transforms: Vec<Box<dyn Fn(T) -> T>>,
}

impl<T: 'static> Pipeline<T> {
    fn new() -> Self {
        Self { transforms: Vec::new() }
    }

    fn pipe(mut self, f: impl Fn(T) -> T + 'static) -> Self {
        self.transforms.push(Box::new(f));
        self
    }

    fn execute(&self, input: T) -> T {
        let mut val = input;
        for f in &self.transforms {
            val = f(val);
        }
        val
    }
}

fn main() {
    let pipeline = Pipeline::new()
        .pipe(|s: String| s.trim().to_string())
        .pipe(|s| s.to_lowercase())
        .pipe(|s| s.replace(' ', "_"))
        .pipe(|s| format!("processed_{}", s));

    let result = pipeline.execute("  Hello World  ".to_string());
    println!("{}", result);  // "processed_hello_world"

    // Reusar el mismo pipeline con otro input
    let result2 = pipeline.execute("  ANOTHER Test  ".to_string());
    println!("{}", result2);  // "processed_another_test"
}
```

```
  pipeline.execute("  Hello World  ")
  │
  ├─ trim()       → "Hello World"
  ├─ lowercase()  → "hello world"
  ├─ replace ' '  → "hello_world"
  └─ prefix       → "processed_hello_world"
```

### 7.1 Pipeline numerico

```rust
let math = Pipeline::new()
    .pipe(|x: f64| x.abs())
    .pipe(|x| x.sqrt())
    .pipe(|x| (x * 100.0).round() / 100.0);

println!("{}", math.execute(-16.0));  // 4.0
println!("{}", math.execute(-2.0));   // 1.41
```

---

## 8. Decorator funcional con antes/despues

Un patron comun: ejecutar logica antes y despues de la funcion
decorada:

```rust
/// Decorator generico: before + inner + after
fn around<A: Clone + std::fmt::Debug, R: std::fmt::Debug>(
    inner: impl Fn(A) -> R + 'static,
    name: &'static str,
) -> impl Fn(A) -> R {
    move |arg: A| {
        println!("[{}] Before: arg={:?}", name, arg);
        let start = std::time::Instant::now();
        let result = inner(arg);
        let elapsed = start.elapsed();
        println!("[{}] After: result={:?} ({:?})", name, result, elapsed);
        result
    }
}

fn expensive_computation(n: u64) -> u64 {
    (0..n).sum()
}

fn main() {
    let timed = around(expensive_computation, "sum");
    let result = timed(1_000_000);
    println!("Result: {}", result);
    // [sum] Before: arg=1000000
    // [sum] After: result=499999500000 (1.23ms)
    // Result: 499999500000
}
```

### 8.1 Retry como decorator funcional

```rust
fn with_retry<A: Clone, R>(
    inner: impl Fn(A) -> Result<R, String>,
    max_retries: usize,
) -> impl Fn(A) -> Result<R, String> {
    move |arg: A| {
        let mut last_err = String::new();
        for attempt in 0..=max_retries {
            match inner(arg.clone()) {
                Ok(val) => return Ok(val),
                Err(e) => {
                    last_err = e;
                    if attempt < max_retries {
                        eprintln!("Retry {}/{}: {}", attempt + 1, max_retries, last_err);
                    }
                }
            }
        }
        Err(format!("Failed after {} retries: {}", max_retries, last_err))
    }
}

fn flaky_operation(input: &str) -> Result<String, String> {
    use std::sync::atomic::{AtomicU32, Ordering};
    static COUNTER: AtomicU32 = AtomicU32::new(0);
    let count = COUNTER.fetch_add(1, Ordering::Relaxed);

    if count % 3 != 2 {
        Err("temporary failure".to_string())
    } else {
        Ok(format!("Success: {}", input))
    }
}

fn main() {
    let reliable = with_retry(flaky_operation, 5);
    match reliable("test") {
        Ok(v)  => println!("{}", v),
        Err(e) => println!("Error: {}", e),
    }
}
```

---

## 9. Comparacion: tres formas de decorator

```
  Forma          Cuando usar                    Ejemplo
  ─────          ───────────                    ───────
  Struct+Trait   API compleja, multiples        std::io::BufWriter
  (T01/T02)      metodos, estado rico           tower::Service

  Closure/Fn     Pipeline simple, una firma     middleware HTTP
  (T03)          de funcion, composicion        map/filter chains

  Method chain   Transformaciones de datos      Iterator::map().filter()
  (iterators)    lazy evaluation                Pipeline<T>

  Todos son decorator — diferente ergonomia, mismo principio.
```

### 9.1 Cuando elegir funcional sobre struct

```
  Funcional (closures):           Struct (trait impl):
  ─────────────────────           ────────────────────
  Una sola operacion (Fn(A)->B)   Multiples operaciones (trait)
  Estado simple (captures)        Estado complejo (campos)
  Composicion ad-hoc              Composicion reutilizable
  Prototipo rapido                API publica estable
  Facil de pasar como argumento   Facil de nombrar y almacenar
```

---

## 10. Errores comunes

| Error | Por que falla | Solucion |
|---|---|---|
| Capturar referencia en closure de middleware | Lifetime demasiado corto | `.clone()` antes de mover al closure |
| Cadena de Box<dyn Fn> sin 'static | Los closures no viven suficiente | Agregar `+ 'static` al trait bound |
| Orden de middlewares invertido | Pre-processing se ejecuta al reves | `.rev()` al iterar, o aplicar en orden correcto |
| Middleware que no llama `next` | La cadena se rompe | Siempre llamar next (excepto short-circuit intencional) |
| Closure que captura &mut | No puedes tener multiples &mut | Arc<Mutex<T>> para estado compartido |
| Pipeline que alloca en cada paso | Vec<String> intermedios | Reutilizar buffers o usar iterators lazy |
| Middleware stateful sin sincronizacion | Data race en multi-thread | Arc<Mutex> o atomics para estado |
| Componer closures con tipos incompatibles | `Fn(A)->B` y `Fn(C)->D` no componen | Los tipos intermedios deben coincidir |

---

## 11. Ejercicios

### Ejercicio 1 — Higher-order function basica

Escribe un decorator `with_caching` que cachea el resultado de
una funcion `fn(u32) -> u64` usando un HashMap.

**Prediccion**: ¿el HashMap vive dentro del closure? ¿Como persiste
entre llamadas?

<details>
<summary>Respuesta</summary>

```rust
use std::collections::HashMap;
use std::cell::RefCell;

fn with_caching(
    f: impl Fn(u32) -> u64 + 'static,
) -> impl FnMut(u32) -> u64 {
    let mut cache = HashMap::new();

    move |x: u32| -> u64 {
        if let Some(&cached) = cache.get(&x) {
            println!("Cache hit for {}", x);
            return cached;
        }
        println!("Cache miss for {}", x);
        let result = f(x);
        cache.insert(x, result);
        result
    }
}

fn expensive(n: u32) -> u64 {
    std::thread::sleep(std::time::Duration::from_millis(100));
    (n as u64) * (n as u64)
}

fn main() {
    let mut cached_expensive = with_caching(expensive);

    println!("{}", cached_expensive(5));  // Cache miss, 25
    println!("{}", cached_expensive(5));  // Cache hit, 25
    println!("{}", cached_expensive(3));  // Cache miss, 9
    println!("{}", cached_expensive(3));  // Cache hit, 9
}
```

El HashMap vive **dentro del closure** como variable capturada.
`move` mueve el HashMap al closure. Persiste entre llamadas
porque el closure es `FnMut` — puede mutar su estado interno.
Cada llamada lee/modifica el mismo HashMap.

Nota: retornamos `impl FnMut` (no `impl Fn`) porque el closure
muta `cache`. Si necesitaras `Fn`, usarias `RefCell<HashMap>`.

</details>

---

### Ejercicio 2 — Pipeline de strings

Construye un pipeline que: trim → lowercase → reemplaza espacios
por guiones → trunca a 20 chars.

**Prediccion**: ¿puedes usar el Pipeline<T> de la seccion 7
directamente?

<details>
<summary>Respuesta</summary>

```rust
struct Pipeline<T> {
    transforms: Vec<Box<dyn Fn(T) -> T>>,
}

impl<T: 'static> Pipeline<T> {
    fn new() -> Self { Self { transforms: Vec::new() } }
    fn pipe(mut self, f: impl Fn(T) -> T + 'static) -> Self {
        self.transforms.push(Box::new(f));
        self
    }
    fn execute(&self, input: T) -> T {
        let mut val = input;
        for f in &self.transforms { val = f(val); }
        val
    }
}

fn main() {
    let slugify = Pipeline::new()
        .pipe(|s: String| s.trim().to_string())
        .pipe(|s| s.to_lowercase())
        .pipe(|s| s.replace(' ', "-"))
        .pipe(|s| {
            if s.len() > 20 { s[..20].to_string() }
            else { s }
        });

    assert_eq!(slugify.execute("  Hello World  ".into()), "hello-world");
    assert_eq!(
        slugify.execute("  A Very Long Title That Should Be Truncated  ".into()),
        "a-very-long-title-th"
    );

    // Reutilizable con cualquier input
    let results: Vec<String> = vec![
        "  Rust Programming  ",
        "  THE QUICK BROWN FOX  ",
    ].into_iter()
     .map(|s| slugify.execute(s.to_string()))
     .collect();

    println!("{:?}", results);
    // ["rust-programming", "the-quick-brown-fox"]
}
```

Si, `Pipeline<String>` funciona directamente. Cada `.pipe()`
agrega un paso que toma `String` y retorna `String`. El
pipeline es reutilizable — `execute()` toma `&self`, no consume
el pipeline.

</details>

---

### Ejercicio 3 — Middleware HTTP

Implementa tres middlewares: `timer` (mide duracion), `not_found`
(retorna 404 si path no existe), y `json_content_type` (agrega
header).

**Prediccion**: ¿cual middleware debe estar mas al exterior?

<details>
<summary>Respuesta</summary>

```rust
use std::collections::HashMap;
use std::time::Instant;

#[derive(Debug, Clone)]
struct Request { path: String, method: String }

#[derive(Debug, Clone)]
struct Response {
    status: u16,
    headers: HashMap<String, String>,
    body: String,
}

type HandlerFn = Box<dyn Fn(Request) -> Response>;
type Middleware = Box<dyn Fn(HandlerFn) -> HandlerFn>;

fn timer() -> Middleware {
    Box::new(|next: HandlerFn| -> HandlerFn {
        Box::new(move |req: Request| {
            let start = Instant::now();
            let resp = next(req);
            let elapsed = start.elapsed();
            eprintln!("Request took: {:?}", elapsed);
            resp
        })
    })
}

fn not_found(known_paths: Vec<String>) -> Middleware {
    Box::new(move |next: HandlerFn| -> HandlerFn {
        let paths = known_paths.clone();
        Box::new(move |req: Request| {
            if !paths.contains(&req.path) {
                return Response {
                    status: 404,
                    headers: HashMap::new(),
                    body: format!("Not found: {}", req.path),
                };
            }
            next(req)
        })
    })
}

fn json_content_type() -> Middleware {
    Box::new(|next: HandlerFn| -> HandlerFn {
        Box::new(move |req: Request| {
            let mut resp = next(req);
            resp.headers.insert(
                "Content-Type".into(),
                "application/json".into(),
            );
            resp
        })
    })
}

fn apply(base: HandlerFn, mws: Vec<Middleware>) -> HandlerFn {
    let mut h = base;
    for mw in mws.into_iter().rev() { h = mw(h); }
    h
}

fn main() {
    let base: HandlerFn = Box::new(|req| Response {
        status: 200,
        headers: HashMap::new(),
        body: format!("{{\"path\": \"{}\"}}", req.path),
    });

    let handler = apply(base, vec![
        timer(),
        not_found(vec!["/api/users".into(), "/api/orders".into()]),
        json_content_type(),
    ]);

    let resp = handler(Request { path: "/api/users".into(), method: "GET".into() });
    println!("{:?}", resp);

    let resp = handler(Request { path: "/unknown".into(), method: "GET".into() });
    println!("{:?}", resp);
}
```

`timer` debe estar mas al exterior para medir el tiempo total
(incluyendo todos los middlewares internos). Si estuviera adentro,
solo mediria parte del procesamiento.

Orden (exterior a interior): timer → not_found → json_content_type → base.

</details>

---

### Ejercicio 4 — Compose generico

Implementa una funcion `compose` que compone dos funciones de
tipos arbitrarios: `f: B→C` y `g: A→B` → `h: A→C`.

**Prediccion**: ¿quien se ejecuta primero, f o g?

<details>
<summary>Respuesta</summary>

```rust
fn compose<A, B, C>(
    f: impl Fn(B) -> C + 'static,
    g: impl Fn(A) -> B + 'static,
) -> impl Fn(A) -> C {
    move |x| f(g(x))
}

fn main() {
    let double = |x: i32| x * 2;
    let to_str = |x: i32| format!("{}", x);
    let add_bang = |s: String| format!("{}!", s);

    // compose(f, g)(x) = f(g(x))
    // g se ejecuta primero, f despues

    let pipeline = compose(add_bang, compose(to_str, double));
    println!("{}", pipeline(5));  // "10!"
    // 5 → double → 10 → to_str → "10" → add_bang → "10!"
}
```

`g` se ejecuta primero (recibe el input), luego `f` (recibe
el output de g). Esto es composicion matematica: `(f ∘ g)(x) = f(g(x))`.

El orden de lectura es inverso al de ejecucion:
- Se lee: `compose(add_bang, compose(to_str, double))`
- Se ejecuta: double → to_str → add_bang

</details>

---

### Ejercicio 5 — Decorator con retry y backoff

Implementa un decorator `with_retry` que reintenta con delay
exponencial (1s, 2s, 4s, ...).

**Prediccion**: ¿el closure captura `max_retries` por valor
o por referencia?

<details>
<summary>Respuesta</summary>

```rust
use std::time::Duration;
use std::thread;

fn with_retry<R: Clone>(
    f: impl Fn() -> Result<R, String> + 'static,
    max_retries: u32,
) -> impl Fn() -> Result<R, String> {
    // max_retries se captura por VALOR (Copy)
    move || {
        for attempt in 0..=max_retries {
            match f() {
                Ok(val) => return Ok(val),
                Err(e) => {
                    if attempt == max_retries {
                        return Err(format!(
                            "Failed after {} retries: {}", max_retries, e
                        ));
                    }
                    let delay = Duration::from_millis(100 * 2u64.pow(attempt));
                    eprintln!(
                        "Attempt {}/{} failed: {}. Retrying in {:?}",
                        attempt + 1, max_retries, e, delay
                    );
                    thread::sleep(delay);
                }
            }
        }
        unreachable!()
    }
}

fn main() {
    use std::sync::atomic::{AtomicU32, Ordering};
    static CALLS: AtomicU32 = AtomicU32::new(0);

    let flaky = || {
        let n = CALLS.fetch_add(1, Ordering::Relaxed);
        if n < 2 {
            Err("connection refused".to_string())
        } else {
            Ok("connected!".to_string())
        }
    };

    let reliable = with_retry(flaky, 5);
    match reliable() {
        Ok(v) => println!("Success: {}", v),
        Err(e) => println!("Error: {}", e),
    }
    // Attempt 1/5 failed. Retrying in 100ms
    // Attempt 2/5 failed. Retrying in 200ms
    // Success: connected!
}
```

`max_retries` se captura por **valor** (u32 implementa Copy).
El closure `move` copia el valor dentro de si. No hay referencia
al parametro original — el closure es autocontenido y puede
vivir independientemente.

</details>

---

### Ejercicio 6 — Decorator funcional en C

Implementa el patron middleware en C para un sistema de
procesamiento de mensajes.

**Prediccion**: ¿como simulas el closure capture sin closures?

<details>
<summary>Respuesta</summary>

```c
typedef struct { char text[256]; int priority; } Message;
typedef struct { int code; char text[256]; } Result;

typedef Result (*HandlerFunc)(const Message *msg, void *ctx);

typedef struct {
    HandlerFunc func;
    void       *ctx;
} Handler;

Result handler_call(Handler *h, const Message *msg) {
    return h->func(msg, h->ctx);
}

/* Logging middleware */
typedef struct { Handler inner; } LogData;

static Result log_handler(const Message *msg, void *ctx) {
    LogData *ld = ctx;
    printf("[LOG] Processing: %s (prio=%d)\n", msg->text, msg->priority);
    Result r = handler_call(&ld->inner, msg);
    printf("[LOG] Result: %d\n", r.code);
    return r;
}

Handler with_logging(Handler inner) {
    LogData *ld = malloc(sizeof(*ld));
    ld->inner = inner;
    return (Handler){ .func = log_handler, .ctx = ld };
}

/* Priority filter middleware */
typedef struct { Handler inner; int min_priority; } FilterData;

static Result filter_handler(const Message *msg, void *ctx) {
    FilterData *fd = ctx;
    if (msg->priority < fd->min_priority) {
        return (Result){ .code = -1, .text = "Filtered" };
    }
    return handler_call(&fd->inner, msg);
}

Handler with_priority_filter(Handler inner, int min_priority) {
    FilterData *fd = malloc(sizeof(*fd));
    fd->inner = inner;
    fd->min_priority = min_priority;
    return (Handler){ .func = filter_handler, .ctx = fd };
}

/* Base handler */
static Result base_process(const Message *msg, void *ctx) {
    (void)ctx;
    Result r = { .code = 0 };
    snprintf(r.text, sizeof(r.text), "Processed: %s", msg->text);
    return r;
}

int main(void) {
    Handler base = { .func = base_process, .ctx = NULL };
    Handler filtered = with_priority_filter(base, 2);
    Handler logged = with_logging(filtered);

    Message m1 = { .text = "Important", .priority = 3 };
    Message m2 = { .text = "Low prio", .priority = 1 };

    handler_call(&logged, &m1);  /* Pasa el filtro */
    handler_call(&logged, &m2);  /* Filtrado */

    return 0;
}
```

El "closure capture" se simula con `void *ctx` que apunta a un
struct que contiene el handler inner + datos capturados (como
`min_priority`). Cada middleware tiene su propio `XxxData` struct
— equivalente funcional al capture del closure en Rust.

</details>

---

### Ejercicio 7 — Validacion como pipeline

Construye un pipeline de validaciones: cada paso retorna
`Result<T, String>`. Si uno falla, el pipeline para.

**Prediccion**: ¿como encadenas Result<T,E> en un pipeline?

<details>
<summary>Respuesta</summary>

```rust
struct ValidationPipeline<T> {
    validators: Vec<Box<dyn Fn(&T) -> Result<(), String>>>,
}

impl<T: 'static> ValidationPipeline<T> {
    fn new() -> Self {
        Self { validators: Vec::new() }
    }

    fn add(mut self, f: impl Fn(&T) -> Result<(), String> + 'static) -> Self {
        self.validators.push(Box::new(f));
        self
    }

    fn validate(&self, value: &T) -> Result<(), Vec<String>> {
        let errors: Vec<String> = self.validators.iter()
            .filter_map(|v| v(value).err())
            .collect();

        if errors.is_empty() { Ok(()) } else { Err(errors) }
    }

    /// Variante que para en el primer error
    fn validate_fast(&self, value: &T) -> Result<(), String> {
        for v in &self.validators {
            v(value)?;
        }
        Ok(())
    }
}

fn main() {
    let validate_email = ValidationPipeline::<String>::new()
        .add(|s| {
            if s.is_empty() { Err("empty".into()) } else { Ok(()) }
        })
        .add(|s| {
            if !s.contains('@') { Err("missing @".into()) } else { Ok(()) }
        })
        .add(|s| {
            if !s.contains('.') { Err("missing domain".into()) } else { Ok(()) }
        })
        .add(|s| {
            if s.len() > 254 { Err("too long".into()) } else { Ok(()) }
        });

    assert!(validate_email.validate(&"user@test.com".into()).is_ok());

    match validate_email.validate(&"invalid".into()) {
        Ok(()) => println!("Valid"),
        Err(errors) => println!("Errors: {:?}", errors),
        // Errors: ["missing @", "missing domain"]
    }

    // Fast: para en el primer error
    match validate_email.validate_fast(&"".into()) {
        Ok(()) => println!("Valid"),
        Err(e) => println!("First error: {}", e),
        // First error: empty
    }
}
```

`validate` recolecta TODOS los errores (util para forms).
`validate_fast` para en el primero (util para assertions).
Cada validador es un decorator funcional que verifica una
condicion. El pipeline los compone.

</details>

---

### Ejercicio 8 — Middleware con estado mutable

Implementa un middleware `counter` que cuenta cuantas veces
fue llamado.

**Prediccion**: ¿puedes usar un `&mut u64` dentro del closure,
o necesitas algo mas?

<details>
<summary>Respuesta</summary>

```rust
use std::sync::{Arc, atomic::{AtomicU64, Ordering}};

type HandlerFn = Box<dyn Fn(&str) -> String>;
type Middleware = Box<dyn Fn(HandlerFn) -> HandlerFn>;

fn counter_middleware(name: &str) -> (Middleware, Arc<AtomicU64>) {
    let count = Arc::new(AtomicU64::new(0));
    let count_clone = Arc::clone(&count);
    let name = name.to_string();

    let mw: Middleware = Box::new(move |next: HandlerFn| -> HandlerFn {
        let count = Arc::clone(&count_clone);
        let name = name.clone();
        Box::new(move |req: &str| {
            let n = count.fetch_add(1, Ordering::Relaxed) + 1;
            eprintln!("[{}] Call #{}", name, n);
            next(req)
        })
    });

    (mw, count)
}

fn main() {
    let (counter_mw, count) = counter_middleware("api");

    let base: HandlerFn = Box::new(|req: &str| format!("OK: {}", req));
    let handler = counter_mw(base);

    handler("request 1");
    handler("request 2");
    handler("request 3");

    println!("Total calls: {}", count.load(Ordering::Relaxed));
    // Total calls: 3
}
```

No puedes usar `&mut u64` porque:
- El closure es `Fn` (no `FnMut`) — se llama con `&self`
- Multiples llamadas concurrentes son posibles
- `&mut` no es `Send`/`Sync` entre threads

`Arc<AtomicU64>` resuelve todo:
- `Arc`: compartir entre el closure y el caller (para leer stats)
- `AtomicU64`: mutar sin `&mut` (interior mutability, thread-safe)

Para single-thread, `Rc<Cell<u64>>` bastaria.

</details>

---

### Ejercicio 9 — Combinar struct y funcional

Un handler struct que acepta middlewares funcionales:

```rust
trait Service {
    fn call(&self, req: &str) -> String;
}
```

Implementa `ServiceWithMiddleware` que envuelve un `Service`
con closures de middleware.

**Prediccion**: ¿el middleware envuelve el trait o la llamada?

<details>
<summary>Respuesta</summary>

```rust
trait Service {
    fn call(&self, req: &str) -> String;
}

struct EchoService;
impl Service for EchoService {
    fn call(&self, req: &str) -> String {
        format!("echo: {}", req)
    }
}

/// Struct que combina un Service con middlewares funcionales
struct ServiceWithMiddleware {
    handler: Box<dyn Fn(&str) -> String>,
}

impl ServiceWithMiddleware {
    /// Convierte un Service + middlewares en un handler funcional
    fn new<S: Service + 'static>(
        service: S,
        middlewares: Vec<Box<dyn Fn(Box<dyn Fn(&str) -> String>) -> Box<dyn Fn(&str) -> String>>>,
    ) -> Self {
        // Convertir el Service a closure
        let mut handler: Box<dyn Fn(&str) -> String> = Box::new(move |req: &str| {
            service.call(req)
        });

        // Aplicar middlewares
        for mw in middlewares.into_iter().rev() {
            handler = mw(handler);
        }

        Self { handler }
    }
}

impl Service for ServiceWithMiddleware {
    fn call(&self, req: &str) -> String {
        (self.handler)(req)
    }
}

fn main() {
    let logging = |next: Box<dyn Fn(&str) -> String>| -> Box<dyn Fn(&str) -> String> {
        Box::new(move |req: &str| {
            println!(">> {}", req);
            let resp = next(req);
            println!("<< {}", resp);
            resp
        })
    };

    let upper = |next: Box<dyn Fn(&str) -> String>| -> Box<dyn Fn(&str) -> String> {
        Box::new(move |req: &str| {
            next(req).to_uppercase()
        })
    };

    let service = ServiceWithMiddleware::new(
        EchoService,
        vec![Box::new(logging), Box::new(upper)],
    );

    let resp = service.call("hello");
    println!("Final: {}", resp);
    // >> hello
    // << ECHO: HELLO
    // Final: ECHO: HELLO
}
```

El middleware envuelve **la llamada** (convierte el Service a
closure, luego aplica middlewares a la closure). El resultado
final implementa Service de nuevo — transparente para el caller.

Este es el patron que usa tower (Rust web ecosystem): un
`Service` trait con layers que son funciones que transforman
services.

</details>

---

### Ejercicio 10 — Elegir la forma

Para cada caso, ¿struct decorator (T02) o funcional (T03)?

1. Buffered I/O sobre File
2. Middleware de autenticacion HTTP
3. Pipeline de transformacion CSV → JSON
4. Rate limiter con estado persistente
5. Composicion de filtros de imagen

**Prediccion**: justifica cada eleccion.

<details>
<summary>Respuesta</summary>

**1. Buffered I/O** → **Struct (T02)**
- Multiples metodos: write, flush, read
- Estado complejo: buffer interno, capacidad, posicion
- Necesita into_inner() para recuperar el File
- stdlib ya lo implementa asi: `BufWriter<W: Write>`

**2. Middleware HTTP auth** → **Funcional (T03)**
- Una sola operacion: `Request → Response`
- Estado simple: token string (capturado por closure)
- Se compone con otros middlewares facilmente
- Patron standard en web: actix, axum, tower

**3. Pipeline CSV → JSON** → **Funcional (T03)**
- Serie de transformaciones: parse → transform → format
- Cada paso es `fn(T) -> U`
- Natural como `.map().filter().map()`
- Pipeline::new().pipe(...).pipe(...)

**4. Rate limiter con estado** → **Struct (T02)**
- Estado complejo: contador, ventana de tiempo, config
- Necesita sincronizacion (Mutex/Atomic)
- Multiples metodos: check, reset, stats
- El estado justifica un struct dedicado

**5. Filtros de imagen** → **Hibrido**
- Struct para filtros complejos (blur: kernel matrix, state)
- Funcional para filtros simples (brightness: `|px| px * 1.2`)
- Pipeline para componer ambos

Regla practica:
- Una operacion + estado simple → funcional
- Multiples operaciones o estado complejo → struct
- Pipeline de transformaciones → funcional o iterator

</details>
