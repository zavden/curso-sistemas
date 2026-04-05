# Comparacion con Rust — net/http vs hyper/axum, simplicidad vs rendimiento/seguridad de tipos

## 1. Introduccion

En los topicos anteriores construimos clientes HTTP, servidores, y APIs REST completas con la stdlib de Go. Ahora comparamos ese ecosistema con el de Rust: **hyper** (cliente/servidor HTTP de bajo nivel) y **axum** (framework web de alto nivel sobre hyper + tokio + tower). Esta comparacion no busca declarar un ganador — ambos lenguajes son excelentes para networking — sino mapear conceptos 1:1 para que entiendas las diferencias de diseno, las tradeoffs, y cuando elegir cada uno.

```
┌────────────────────────────────────────────────────────────────────────────────────┐
│              ECOSISTEMA HTTP: GO vs RUST                                          │
├────────────────────────────────────────────────────────────────────────────────────┤
│                                                                                    │
│  GO                                    RUST                                       │
│  ────────────────────────              ─────────────────────────                   │
│                                                                                    │
│  ┌──────────────────────┐              ┌──────────────────────┐                   │
│  │     net/http          │              │     hyper             │                   │
│  │  (stdlib, todo en 1)  │              │  (HTTP 1/2/3, bajo   │                   │
│  │  • Client             │              │   nivel, async)       │                   │
│  │  • Server             │              │  • conn::http1/http2  │                   │
│  │  • ServeMux           │              │  • client::conn       │                   │
│  │  • Transport          │              │  • server::conn       │                   │
│  │  • TLS               │              └──────────┬───────────┘                   │
│  │  • HTTP/2             │                          │                              │
│  └──────────────────────┘              ┌──────────▼───────────┐                   │
│                                         │     axum              │                   │
│  ┌──────────────────────┐              │  (framework web)      │                   │
│  │   Frameworks          │              │  • Router             │                   │
│  │   (opcionales)        │              │  • Handler (trait)    │                   │
│  │  • chi               │              │  • Extractors         │                   │
│  │  • gin               │              │  • Middleware (tower)  │                   │
│  │  • fiber             │              └──────────┬───────────┘                   │
│  │  • echo              │                          │                              │
│  └──────────────────────┘              ┌──────────▼───────────┐                   │
│                                         │     tower             │                   │
│                                         │  (middleware layer)   │                   │
│                                         │  • Service trait      │                   │
│                                         │  • Layer trait        │                   │
│                                         │  • timeout, retry,    │                   │
│                                         │    rate_limit, etc.   │                   │
│                                         └──────────┬───────────┘                   │
│                                                      │                              │
│  ┌──────────────────────┐              ┌──────────▼───────────┐                   │
│  │   Goroutines          │              │     tokio             │                   │
│  │  (runtime integrado)  │              │  (async runtime)      │                   │
│  │  • go func(){}()     │              │  • #[tokio::main]     │                   │
│  │  • M:N scheduling    │              │  • spawn              │                   │
│  │  • Automatico        │              │  • select!            │                   │
│  └──────────────────────┘              └──────────────────────┘                   │
│                                                                                    │
│  ┌──────────────────────┐              ┌──────────────────────┐                   │
│  │   Extras stdlib       │              │     Crates            │                   │
│  │  • encoding/json     │              │  • serde/serde_json   │                   │
│  │  • html/template     │              │  • askama/tera        │                   │
│  │  • crypto/tls        │              │  • rustls/native-tls  │                   │
│  │  • net/url           │              │  • url                │                   │
│  └──────────────────────┘              │  • reqwest (cliente)  │                   │
│                                         │  • tower-http         │                   │
│                                         └──────────────────────┘                   │
└────────────────────────────────────────────────────────────────────────────────────┘
```

**Diferencia fundamental**: Go empaqueta todo en la stdlib — un solo import y tienes cliente, servidor, router, TLS, HTTP/2. Rust distribuye la funcionalidad en crates especializados que se componen: tokio (runtime) + hyper (protocolo) + axum (framework) + tower (middleware) + serde (serialization). Esto da mas flexibilidad pero mas complejidad inicial.

---

## 2. Filosofia de diseno

### Go: simplicidad y productividad

```go
// Un servidor completo en Go — 15 lineas
package main

import (
    "encoding/json"
    "net/http"
)

func main() {
    http.HandleFunc("GET /hello/{name}", func(w http.ResponseWriter, r *http.Request) {
        name := r.PathValue("name")
        json.NewEncoder(w).Encode(map[string]string{"message": "Hello, " + name})
    })
    http.ListenAndServe(":8080", nil)
}
```

**Principios**:
- **Zero config**: funciona sin configuracion adicional
- **Sync-looking code**: las goroutines ocultan la complejidad async
- **Batteries included**: `net/http` + `encoding/json` son todo lo necesario
- **Implicit interfaces**: cualquier struct que implemente `ServeHTTP` es un handler
- **Errores como valores**: retornas `error`, no propagas con `?` ni lanzas excepciones

### Rust: seguridad y rendimiento

```rust
// Un servidor completo en Rust (axum) — 20 lineas
use axum::{extract::Path, routing::get, Json, Router};
use serde::Serialize;

#[derive(Serialize)]
struct Message {
    message: String,
}

async fn hello(Path(name): Path<String>) -> Json<Message> {
    Json(Message {
        message: format!("Hello, {name}"),
    })
}

#[tokio::main]
async fn main() {
    let app = Router::new().route("/hello/{name}", get(hello));
    let listener = tokio::net::TcpListener::bind("0.0.0.0:8080").await.unwrap();
    axum::serve(listener, app).await.unwrap();
}
```

**Principios**:
- **Type safety**: el sistema de tipos garantiza que los extractors son correctos en compilacion
- **Explicit async**: `async/await` hace visible donde ocurre I/O
- **Zero-cost abstractions**: tower middleware no tiene overhead en runtime
- **Ownership**: la gestion de memoria es determinista sin GC
- **Composicion**: capas independientes (tokio + hyper + tower + axum) se combinan
- **Error handling**: `Result<T, E>` con el operador `?` para propagacion ergonomica

### Comparacion de filosofias

| Aspecto | Go | Rust |
|---|---|---|
| Modelo mental | "Procesos secuenciales comunicandose" | "Transformaciones de tipos con ownership" |
| Concurrencia | Goroutines (implicito, runtime integrado) | async/await (explicito, runtime externo) |
| Errores | `if err != nil` (verboso, simple) | `Result<T,E>` + `?` (ergonomico, tipado) |
| Memoria | GC (pausas cortas, <1ms en Go 1.19+) | Ownership (zero pauses, determinista) |
| Interfaz HTTP | `http.Handler` (implicita) | `Handler` trait + extractors (explicita) |
| Serialization | `encoding/json` + struct tags | `serde` + derive macros |
| Aprendizaje | Horas para ser productivo | Semanas para ser productivo |
| Rendimiento HTTP | Excelente (top 10-15 en benchmarks) | Superior (top 3-5 en benchmarks) |

---

## 3. Cliente HTTP

### Go: http.Client

```go
package main

import (
    "context"
    "encoding/json"
    "fmt"
    "net/http"
    "time"
)

type User struct {
    ID    int    `json:"id"`
    Name  string `json:"name"`
    Email string `json:"email"`
}

func main() {
    // Cliente con timeout
    client := &http.Client{
        Timeout: 10 * time.Second,
    }

    // Request con context
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()

    req, err := http.NewRequestWithContext(ctx, "GET", "https://api.example.com/users/1", nil)
    if err != nil {
        panic(err)
    }
    req.Header.Set("Authorization", "Bearer token123")
    req.Header.Set("Accept", "application/json")

    // Ejecutar
    resp, err := client.Do(req)
    if err != nil {
        panic(err)
    }
    defer resp.Body.Close()

    // Decodificar
    if resp.StatusCode != http.StatusOK {
        fmt.Printf("Error: status %d\n", resp.StatusCode)
        return
    }

    var user User
    if err := json.NewDecoder(resp.Body).Decode(&user); err != nil {
        panic(err)
    }
    fmt.Printf("User: %+v\n", user)
}
```

**Caracteristicas clave del cliente Go**:
- `http.Client` es un struct, se configura por campos publicos
- Un solo `Timeout` cubre DNS + connect + TLS + headers + body
- Context permite cancelacion desde fuera
- `json.NewDecoder` lee el body como stream (no necesita `ioutil.ReadAll`)
- **Siempre** cerrar `resp.Body` con `defer`

### Rust: reqwest

```rust
use reqwest::Client;
use serde::Deserialize;
use std::time::Duration;

#[derive(Debug, Deserialize)]
struct User {
    id: u32,
    name: String,
    email: String,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    // Cliente con timeout
    let client = Client::builder()
        .timeout(Duration::from_secs(10))
        .connect_timeout(Duration::from_secs(5))
        .build()?;

    // Request con headers
    let user: User = client
        .get("https://api.example.com/users/1")
        .bearer_auth("token123")
        .header("Accept", "application/json")
        .send()
        .await?                    // enviar request (async)
        .error_for_status()?       // convertir 4xx/5xx en error
        .json::<User>()            // deserializar (async, consume body)
        .await?;

    println!("User: {user:?}");
    Ok(())
}
```

**Caracteristicas clave del cliente Rust**:
- `Client::builder()` — patron builder con validacion en compilacion
- Timeouts separados (connect vs total) sin configuracion de Transport
- `.bearer_auth()` — metodos tipados para headers comunes
- `.error_for_status()` — convierte status codes en errores (Go no tiene esto)
- `.json::<User>()` — deserializacion integrada con serde
- Body se consume automaticamente (no hay riesgo de leak)
- Cadena de `?` para propagacion de errores

### Comparacion lado a lado

```
┌────────────────────────────────────────────────────────────────────────┐
│                    FLUJO DE REQUEST                                    │
├────────────────────────────────────────────────────────────────────────┤
│                                                                        │
│  GO                                   RUST (reqwest)                  │
│  ─────────────────                    ─────────────────               │
│                                                                        │
│  client := &http.Client{             let client = Client::builder()   │
│      Timeout: 10*time.Second,            .timeout(10s)                │
│  }                                       .build()?;                   │
│                                                                        │
│  req, err := http.NewRequest(        let resp = client                │
│      "GET", url, nil)                    .get(url)                    │
│  // err check                            .bearer_auth(tok)            │
│  req.Header.Set("Auth", tok)             .send()                     │
│                                          .await?;                     │
│  resp, err := client.Do(req)                                          │
│  // err check                                                         │
│  defer resp.Body.Close()  ←── NO EQUIVALENTE (body auto-managed)     │
│                                                                        │
│  if resp.StatusCode != 200 {         resp.error_for_status()?;       │
│      // manual check                                                  │
│  }                                                                    │
│                                                                        │
│  var user User                       let user: User =                │
│  json.NewDecoder(resp.Body)              resp.json().await?;          │
│      .Decode(&user)                                                   │
│  // err check                                                         │
│                                                                        │
│  Errores: 4 puntos de if err !=nil   Errores: cadena de ?            │
│  Body: manual close + drain          Body: automatico                │
│  Status: manual check                Status: error_for_status()      │
│  JSON: streaming manual              JSON: integrado con serde       │
└────────────────────────────────────────────────────────────────────────┘
```

### Diferencias criticas en el cliente

| Aspecto | Go (net/http) | Rust (reqwest) |
|---|---|---|
| Creacion | Struct literal `&http.Client{}` | Builder pattern `Client::builder().build()` |
| Timeout global | `client.Timeout` | `.timeout()` |
| Timeout por fase | Requiere configurar `Transport` | `.connect_timeout()`, `.read_timeout()` |
| Body management | Manual (`defer resp.Body.Close()`) | Automatico (ownership consume el body) |
| Status check | Manual (`if resp.StatusCode != 200`) | `.error_for_status()` opcional |
| JSON decode | Manual (`json.NewDecoder + Decode`) | `.json::<T>().await` |
| Error handling | `if err != nil` en cada paso | `?` encadenado |
| Connection pool | `Transport.MaxIdleConnsPerHost` | Automatico en `Client` |
| Redirect | `CheckRedirect` callback | `Policy` enum + builder |
| Proxy | `Transport.Proxy` function | `.proxy()` en builder |
| Certificados | `Transport.TLSClientConfig` | `.add_root_certificate()` |
| HTTP/2 | Automatico con TLS | Feature flag `http2` |
| Streaming body | `io.Reader` en request | `Body::wrap_stream()` |
| Multipart | Manual con `mime/multipart` | `.multipart(Form)` integrado |

---

## 4. Servidor HTTP

### Go: net/http server

```go
package main

import (
    "encoding/json"
    "log"
    "net/http"
    "strconv"
    "sync"
)

type Item struct {
    ID   int    `json:"id"`
    Name string `json:"name"`
}

type Store struct {
    mu    sync.RWMutex
    items map[int]Item
    next  int
}

func NewStore() *Store {
    return &Store{items: make(map[int]Item), next: 1}
}

func (s *Store) Create(name string) Item {
    s.mu.Lock()
    defer s.mu.Unlock()
    item := Item{ID: s.next, Name: name}
    s.items[s.next] = item
    s.next++
    return item
}

func (s *Store) Get(id int) (Item, bool) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    item, ok := s.items[id]
    return item, ok
}

func (s *Store) List() []Item {
    s.mu.RLock()
    defer s.mu.RUnlock()
    items := make([]Item, 0, len(s.items))
    for _, item := range s.items {
        items = append(items, item)
    }
    return items
}

func main() {
    store := NewStore()
    mux := http.NewServeMux()

    // List items
    mux.HandleFunc("GET /items", func(w http.ResponseWriter, r *http.Request) {
        w.Header().Set("Content-Type", "application/json")
        json.NewEncoder(w).Encode(store.List())
    })

    // Get item
    mux.HandleFunc("GET /items/{id}", func(w http.ResponseWriter, r *http.Request) {
        id, err := strconv.Atoi(r.PathValue("id"))
        if err != nil {
            http.Error(w, "invalid id", http.StatusBadRequest)
            return
        }
        item, ok := store.Get(id)
        if !ok {
            http.Error(w, "not found", http.StatusNotFound)
            return
        }
        w.Header().Set("Content-Type", "application/json")
        json.NewEncoder(w).Encode(item)
    })

    // Create item
    mux.HandleFunc("POST /items", func(w http.ResponseWriter, r *http.Request) {
        var input struct {
            Name string `json:"name"`
        }
        if err := json.NewDecoder(r.Body).Decode(&input); err != nil {
            http.Error(w, "invalid json", http.StatusBadRequest)
            return
        }
        item := store.Create(input.Name)
        w.Header().Set("Content-Type", "application/json")
        w.WriteHeader(http.StatusCreated)
        json.NewEncoder(w).Encode(item)
    })

    log.Println("Listening on :8080")
    log.Fatal(http.ListenAndServe(":8080", mux))
}
```

### Rust: axum server

```rust
use axum::{
    extract::{Path, State},
    http::StatusCode,
    routing::{get, post},
    Json, Router,
};
use serde::{Deserialize, Serialize};
use std::sync::Arc;
use tokio::sync::RwLock;

#[derive(Clone, Serialize)]
struct Item {
    id: u32,
    name: String,
}

#[derive(Deserialize)]
struct CreateItem {
    name: String,
}

#[derive(Clone)]
struct AppState {
    items: Arc<RwLock<Vec<Item>>>,
    next_id: Arc<RwLock<u32>>,
}

// Handler: list items
async fn list_items(State(state): State<AppState>) -> Json<Vec<Item>> {
    let items = state.items.read().await;
    Json(items.clone())
}

// Handler: get item
async fn get_item(
    State(state): State<AppState>,
    Path(id): Path<u32>,
) -> Result<Json<Item>, StatusCode> {
    let items = state.items.read().await;
    items
        .iter()
        .find(|item| item.id == id)
        .cloned()
        .map(Json)
        .ok_or(StatusCode::NOT_FOUND)
}

// Handler: create item
async fn create_item(
    State(state): State<AppState>,
    Json(input): Json<CreateItem>,
) -> (StatusCode, Json<Item>) {
    let mut items = state.items.write().await;
    let mut next = state.next_id.write().await;
    let item = Item {
        id: *next,
        name: input.name,
    };
    *next += 1;
    items.push(item.clone());
    (StatusCode::CREATED, Json(item))
}

#[tokio::main]
async fn main() {
    let state = AppState {
        items: Arc::new(RwLock::new(Vec::new())),
        next_id: Arc::new(RwLock::new(1)),
    };

    let app = Router::new()
        .route("/items", get(list_items).post(create_item))
        .route("/items/{id}", get(get_item))
        .with_state(state);

    let listener = tokio::net::TcpListener::bind("0.0.0.0:8080").await.unwrap();
    axum::serve(listener, app).await.unwrap();
}
```

### Analisis detallado de las diferencias

#### 4.1 Routing

```go
// GO — ServeMux 1.22+
mux := http.NewServeMux()
mux.HandleFunc("GET /items", listHandler)
mux.HandleFunc("GET /items/{id}", getHandler)
mux.HandleFunc("POST /items", createHandler)
mux.HandleFunc("PUT /items/{id}", updateHandler)
mux.HandleFunc("DELETE /items/{id}", deleteHandler)

// Path param: r.PathValue("id") retorna string
// Wildcard: "GET /files/{path...}" captura el resto
// Host: "api.example.com/v1/" routing por host
```

```rust
// RUST — axum Router
let app = Router::new()
    .route("/items", get(list_handler).post(create_handler))
    .route("/items/{id}", get(get_handler).put(update_handler).delete(delete_handler));

// Path param: Path(id): Path<u32> — tipado en compilacion
// Wildcard: "/*path" captura el resto
// Host: requiere middleware o Router::nest
```

**Diferencia clave**: en Go, `r.PathValue("id")` siempre retorna `string` — tu conviertes manualmente con `strconv.Atoi`. En axum, `Path(id): Path<u32>` convierte y valida en compilacion; si llega `"abc"`, axum retorna 400 automaticamente sin que tu handler se ejecute.

```
┌──────────────────────────────────────────────────────────────────────┐
│                    EXTRACCION DE PATH PARAMS                        │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Request: GET /items/42                                              │
│                                                                      │
│  GO:                                                                 │
│  ┌─────────────────────────────────────────────────┐                │
│  │  r.PathValue("id")  → "42" (string)             │                │
│  │  id, err := strconv.Atoi("42")                  │                │
│  │  if err != nil {                                 │                │
│  │      http.Error(w, "bad id", 400)  ← runtime    │                │
│  │      return                                      │                │
│  │  }                                               │                │
│  │  // id = 42 (int)                                │                │
│  └─────────────────────────────────────────────────┘                │
│                                                                      │
│  RUST:                                                               │
│  ┌─────────────────────────────────────────────────┐                │
│  │  Path(id): Path<u32>  → 42 (u32)  ← compilacion │                │
│  │  // Si "abc": axum retorna 400 automaticamente  │                │
│  │  // Tu handler NUNCA recibe datos invalidos     │                │
│  └─────────────────────────────────────────────────┘                │
│                                                                      │
│  Request: GET /items/abc                                             │
│  GO: handler se ejecuta, TU manejas el error                       │
│  RUST: handler NO se ejecuta, axum responde 400                    │
└──────────────────────────────────────────────────────────────────────┘
```

#### 4.2 Handlers

```go
// GO — todos los handlers tienen la misma firma
// func(w http.ResponseWriter, r *http.Request)

// No hay informacion de tipos sobre que lee o escribe el handler
func createItem(w http.ResponseWriter, r *http.Request) {
    var input CreateInput
    if err := json.NewDecoder(r.Body).Decode(&input); err != nil {
        // Error en runtime
        http.Error(w, "bad json", 400)
        return
    }
    // ... usar input
    w.WriteHeader(201)
    json.NewEncoder(w).Encode(result)
}
```

```rust
// RUST — la firma del handler documenta que extrae y que retorna
// Los tipos son verificados en compilacion

async fn create_item(
    State(state): State<AppState>,   // extrae estado compartido
    Json(input): Json<CreateInput>,  // extrae y valida JSON body
) -> (StatusCode, Json<Item>) {     // retorna status + JSON
    // input ya esta parseado y validado
    // Si el body no es JSON valido, axum retorna 400 sin llegar aqui
    let item = /* ... */;
    (StatusCode::CREATED, Json(item))
}

// Otros extractors disponibles:
async fn complex_handler(
    State(db): State<Pool>,           // database pool
    Path(id): Path<u32>,              // path parameter
    Query(params): Query<Filters>,    // query string
    TypedHeader(auth): TypedHeader<Authorization<Bearer>>, // header tipado
    Json(body): Json<UpdateInput>,    // JSON body (debe ser ultimo)
) -> impl IntoResponse {
    // Todo extraido y validado antes de ejecutar el handler
}
```

**Diferencia fundamental**: en Go, el handler es una "caja negra" — la firma `func(w, r)` no dice nada sobre que espera. En axum, la firma ES la documentacion: cada extractor declara que necesita, y el compilador verifica que los tipos coincidan.

#### 4.3 Estado compartido

```go
// GO — closure o struct con metodos
type Server struct {
    store *Store  // tu decides la sincronizacion
}

// Opcion 1: metodos (recomendado)
func (s *Server) handleList(w http.ResponseWriter, r *http.Request) {
    items := s.store.List() // Store usa sync.RWMutex internamente
    json.NewEncoder(w).Encode(items)
}

// Opcion 2: closure
func main() {
    store := NewStore()
    mux.HandleFunc("GET /items", func(w http.ResponseWriter, r *http.Request) {
        json.NewEncoder(w).Encode(store.List())
    })
}
```

```rust
// RUST — State extractor con Arc
#[derive(Clone)]
struct AppState {
    db: Pool<Postgres>,         // connection pool (ya es thread-safe)
    cache: Arc<RwLock<Cache>>,  // estado mutable necesita Arc + lock
}

async fn handler(State(state): State<AppState>) -> impl IntoResponse {
    let data = state.cache.read().await; // async lock
    // ...
}

let app = Router::new()
    .route("/", get(handler))
    .with_state(state);  // inyeccion de estado
```

| Aspecto | Go | Rust (axum) |
|---|---|---|
| Inyeccion | Closure/struct methods | `State` extractor |
| Thread safety | `sync.Mutex`/`sync.RWMutex` (tu responsabilidad) | `Arc<RwLock<T>>` (compilador verifica Send+Sync) |
| Data races | Detectables con `-race` en runtime | Imposibles (compilador las previene) |
| Shared state | Cualquier campo en struct | Debe ser `Clone + Send + Sync + 'static` |

#### 4.4 Middleware

```go
// GO — func(http.Handler) http.Handler
func Logger(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        // Wrap writer para capturar status
        rec := &statusRecorder{ResponseWriter: w, status: 200}
        next.ServeHTTP(rec, r)
        log.Printf("%s %s %d %s", r.Method, r.URL.Path, rec.status, time.Since(start))
    })
}

type statusRecorder struct {
    http.ResponseWriter
    status int
}

func (r *statusRecorder) WriteHeader(code int) {
    r.status = code
    r.ResponseWriter.WriteHeader(code)
}

// Composicion manual
handler := Logger(Auth(RateLimit(mux)))
```

```rust
// RUST — tower middleware (Service + Layer traits)

// Opcion 1: from_fn (simple, como Go)
async fn logger(req: Request, next: Next) -> Response {
    let start = Instant::now();
    let method = req.method().clone();
    let uri = req.uri().clone();

    let response = next.run(req).await;

    tracing::info!("{method} {uri} {} {:?}", response.status(), start.elapsed());
    response
}

let app = Router::new()
    .route("/", get(handler))
    .layer(axum::middleware::from_fn(logger));

// Opcion 2: tower layers (composable, reusable)
use tower::ServiceBuilder;
use tower_http::{
    cors::CorsLayer,
    trace::TraceLayer,
    timeout::TimeoutLayer,
    compression::CompressionLayer,
    limit::RequestBodyLimitLayer,
};

let app = Router::new()
    .route("/", get(handler))
    .layer(
        ServiceBuilder::new()
            .layer(TraceLayer::new_for_http())
            .layer(TimeoutLayer::new(Duration::from_secs(30)))
            .layer(CompressionLayer::new())
            .layer(CorsLayer::permissive())
            .layer(RequestBodyLimitLayer::new(1024 * 1024)), // 1MB
    );
```

**Comparacion de middleware**:

| Aspecto | Go | Rust (tower) |
|---|---|---|
| Patron | `func(Handler) Handler` | `Service` trait + `Layer` trait |
| Status capture | Wrap manual de ResponseWriter | Acceso directo a Response |
| Composicion | Anidamiento manual `A(B(C(handler)))` | `ServiceBuilder` con `.layer()` |
| Middleware pre-built | Pocas (solo lo basico en stdlib) | tower-http: CORS, trace, timeout, compress, limit, etc. |
| Selectivo por ruta | Grupos manuales | `.route_layer()` por grupo de rutas |
| Tipado | Dinamico (interface) | Estatico (el compilador verifica la cadena completa) |
| Overhead | Minimo (dispatch por interfaz virtual) | Zero-cost (monomorphized en compilacion) |

#### 4.5 Respuestas de error

```go
// GO — errores son strings o structs manuales
func getItem(w http.ResponseWriter, r *http.Request) {
    id, err := strconv.Atoi(r.PathValue("id"))
    if err != nil {
        // Opcion 1: texto plano
        http.Error(w, "invalid id", http.StatusBadRequest)
        return
    }
    item, ok := store.Get(id)
    if !ok {
        // Opcion 2: JSON manual
        w.Header().Set("Content-Type", "application/json")
        w.WriteHeader(http.StatusNotFound)
        json.NewEncoder(w).Encode(map[string]string{
            "error": "item not found",
        })
        return
    }
    // exito...
}

// Para errores consistentes, creas tu propio tipo y helper
type APIError struct {
    Code    int    `json:"code"`
    Message string `json:"message"`
}

func writeError(w http.ResponseWriter, code int, msg string) {
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(code)
    json.NewEncoder(w).Encode(APIError{Code: code, Message: msg})
}
```

```rust
// RUST — errores tipados con IntoResponse

// Definir error
#[derive(Debug)]
enum AppError {
    NotFound(String),
    BadRequest(String),
    Internal(anyhow::Error),
}

// Implementar conversion automatica a Response
impl IntoResponse for AppError {
    fn into_response(self) -> Response {
        let (status, message) = match self {
            AppError::NotFound(msg) => (StatusCode::NOT_FOUND, msg),
            AppError::BadRequest(msg) => (StatusCode::BAD_REQUEST, msg),
            AppError::Internal(err) => {
                tracing::error!("Internal error: {err:?}");
                (StatusCode::INTERNAL_SERVER_ERROR, "internal error".into())
            }
        };
        (status, Json(serde_json::json!({"error": message}))).into_response()
    }
}

// Los handlers retornan Result<T, AppError>
async fn get_item(
    State(state): State<AppState>,
    Path(id): Path<u32>,
) -> Result<Json<Item>, AppError> {
    let items = state.items.read().await;
    items
        .iter()
        .find(|i| i.id == id)
        .cloned()
        .map(Json)
        .ok_or_else(|| AppError::NotFound(format!("item {id} not found")))
}

// Con From<> para conversion automatica
impl From<sqlx::Error> for AppError {
    fn from(err: sqlx::Error) -> Self {
        match err {
            sqlx::Error::RowNotFound => AppError::NotFound("not found".into()),
            _ => AppError::Internal(err.into()),
        }
    }
}

// Ahora puedes usar ? directamente
async fn get_user(
    State(db): State<Pool<Postgres>>,
    Path(id): Path<i32>,
) -> Result<Json<User>, AppError> {
    let user = sqlx::query_as!(User, "SELECT * FROM users WHERE id = $1", id)
        .fetch_one(&db)
        .await?;  // sqlx::Error -> AppError automaticamente
    Ok(Json(user))
}
```

**Diferencia clave**: en Go, manejas errores con `if/return` en cada handler y te aseguras manualmente de escribir JSON consistente. En Rust, defines un tipo de error con `IntoResponse` una vez, y todos los handlers retornan `Result<T, AppError>` — el framework convierte automaticamente.

---

## 5. Serialization: encoding/json vs serde

### Go: struct tags + reflection

```go
type User struct {
    ID        int       `json:"id"`
    Name      string    `json:"name"`
    Email     string    `json:"email"`
    Password  string    `json:"-"`                    // nunca serializar
    CreatedAt time.Time `json:"created_at"`
    DeletedAt *time.Time `json:"deleted_at,omitempty"` // omitir si nil
    Metadata  any       `json:"metadata,omitempty"`   // tipo dinamico
}

// Serializar
data, err := json.Marshal(user)

// Deserializar
var user User
err := json.Unmarshal(data, &user)

// Streaming (preferido para HTTP)
err := json.NewDecoder(r.Body).Decode(&user)
json.NewEncoder(w).Encode(user)

// Custom marshaling
type Status int

const (
    Active Status = iota
    Inactive
)

func (s Status) MarshalJSON() ([]byte, error) {
    switch s {
    case Active:
        return json.Marshal("active")
    case Inactive:
        return json.Marshal("inactive")
    default:
        return nil, fmt.Errorf("unknown status: %d", s)
    }
}

func (s *Status) UnmarshalJSON(data []byte) error {
    var str string
    if err := json.Unmarshal(data, &str); err != nil {
        return err
    }
    switch str {
    case "active":
        *s = Active
    case "inactive":
        *s = Inactive
    default:
        return fmt.Errorf("unknown status: %q", str)
    }
    return nil
}
```

### Rust: serde derive macros

```rust
use serde::{Deserialize, Serialize};
use chrono::{DateTime, Utc};

#[derive(Serialize, Deserialize)]
struct User {
    id: u32,
    name: String,
    email: String,
    #[serde(skip)]                          // nunca serializar
    password: String,
    created_at: DateTime<Utc>,
    #[serde(skip_serializing_if = "Option::is_none")]
    deleted_at: Option<DateTime<Utc>>,      // omitir si None
    #[serde(default)]                       // usar Default si falta
    metadata: serde_json::Value,            // tipo dinamico
}

// Serializar
let data = serde_json::to_string(&user)?;
let pretty = serde_json::to_string_pretty(&user)?;

// Deserializar
let user: User = serde_json::from_str(&data)?;
let user: User = serde_json::from_reader(reader)?; // streaming

// Custom serialization via enums (mucho mas ergonomico)
#[derive(Serialize, Deserialize)]
#[serde(rename_all = "lowercase")]
enum Status {
    Active,
    Inactive,
}
// Serializa como "active"/"inactive" automaticamente

// Serde attributes avanzados
#[derive(Serialize, Deserialize)]
#[serde(rename_all = "camelCase")]  // snake_case -> camelCase
struct ApiResponse {
    user_name: String,              // serializa como "userName"
    #[serde(rename = "type")]       // rename individual
    kind: String,
    #[serde(flatten)]               // aplanar struct anidado
    metadata: Metadata,
    #[serde(with = "hex")]          // modulo custom para ser/de
    data: Vec<u8>,
}

// Tagged enums — imposible en Go sin codigo manual
#[derive(Serialize, Deserialize)]
#[serde(tag = "type", content = "data")]
enum Event {
    UserCreated { id: u32, name: String },
    ItemPurchased { item_id: u32, quantity: u32 },
    SystemAlert { message: String, level: u8 },
}
// Serializa como:
// {"type": "UserCreated", "data": {"id": 1, "name": "Alice"}}
// {"type": "ItemPurchased", "data": {"item_id": 42, "quantity": 3}}
```

### Comparacion de serialization

| Aspecto | Go (encoding/json) | Rust (serde) |
|---|---|---|
| Mecanismo | Reflection en runtime | Macros en compilacion |
| Rendimiento | ~3-10x mas lento que serde | Referencia de velocidad |
| Struct tags | `` `json:"name,omitempty"` `` | `#[serde(rename, skip, default, ...)]` |
| Omit field | `omitempty` (solo zero values) | `skip_serializing_if` (predicado custom) |
| Skip | `json:"-"` | `#[serde(skip)]` |
| Rename all | No existe (o usas tags individuales) | `#[serde(rename_all = "camelCase")]` |
| Custom marshal | `MarshalJSON()`/`UnmarshalJSON()` (verbose) | `#[serde(with)]` o impl `Serialize`/`Deserialize` |
| Enums | No soportado nativamente | First-class: untagged, tagged, adjacently tagged |
| Flatten | No existe | `#[serde(flatten)]` |
| Unknown fields | Se ignoran silenciosamente | `#[serde(deny_unknown_fields)]` |
| Formatos | Solo JSON (stdlib) | JSON, TOML, YAML, MessagePack, CBOR, Bincode... |
| Error messages | "cannot unmarshal string into int" | Posicion exacta, tipo esperado vs recibido |
| Alternativas rapidas | sonic-json, jsoniter | simd-json (aun mas rapido) |

**El punto mas importante**: serde separa el modelo de datos del formato de wire. Un `#[derive(Serialize, Deserialize)]` funciona con JSON, TOML, YAML, MessagePack, y cualquier formato que implemente el trait. En Go, `encoding/json` es solo para JSON — para TOML necesitas otra libreria con otros tags.

---

## 6. Concurrencia en servidores web

### Go: goroutines (sync-looking async)

```go
func main() {
    http.HandleFunc("GET /slow", func(w http.ResponseWriter, r *http.Request) {
        // Cada request corre en su propia goroutine automaticamente
        // No necesitas async/await — el runtime maneja todo

        // I/O concurrente: lanzar goroutines explicitamente
        ctx := r.Context()
        ch := make(chan string, 2)

        go func() {
            result, _ := fetchFromDB(ctx)   // bloqueante, pero no bloquea OS thread
            ch <- result
        }()
        go func() {
            result, _ := fetchFromCache(ctx) // otra goroutine concurrente
            ch <- result
        }()

        // Esperar ambos resultados
        db := <-ch
        cache := <-ch

        json.NewEncoder(w).Encode(map[string]string{
            "db": db, "cache": cache,
        })
    })

    // El servidor crea una goroutine por request automaticamente
    // 10k goroutines concurrentes = ~20MB de memoria (2KB stack c/u)
    http.ListenAndServe(":8080", nil)
}
```

### Rust: async/await (explicit async)

```rust
async fn slow_handler(State(state): State<AppState>) -> Json<Response> {
    // Cada request es un future que se ejecuta en el tokio runtime
    // async/await hace explicito donde ocurre I/O

    // I/O concurrente: tokio::join! o tokio::spawn
    let (db_result, cache_result) = tokio::join!(
        fetch_from_db(&state.db),      // future 1
        fetch_from_cache(&state.cache), // future 2
    );

    // Ambos se ejecutan concurrentemente en el mismo thread
    // (a menos que se use tokio::spawn para paralelismo real)

    Json(Response {
        db: db_result.unwrap_or_default(),
        cache: cache_result.unwrap_or_default(),
    })
}

#[tokio::main]
async fn main() {
    // tokio runtime con thread pool (default: 1 thread por CPU core)
    // Cada thread ejecuta miles de tasks async cooperativamente
    let app = Router::new().route("/slow", get(slow_handler));
    let listener = tokio::net::TcpListener::bind("0.0.0.0:8080").await.unwrap();
    axum::serve(listener, app).await.unwrap();
}
```

### Modelo de concurrencia comparado

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    MODELOS DE CONCURRENCIA                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  GO: Goroutines (M:N scheduling)                                           │
│  ───────────────────────────────                                           │
│                                                                             │
│  OS Thread 1          OS Thread 2          OS Thread 3                     │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐                  │
│  │ goroutine A  │     │ goroutine D  │     │ goroutine G  │                  │
│  │ goroutine B  │     │ goroutine E  │     │ goroutine H  │                  │
│  │ goroutine C  │     │ goroutine F  │     │ goroutine I  │                  │
│  └─────────────┘     └─────────────┘     └─────────────┘                  │
│                                                                             │
│  • Stack crece dinamicamente (2KB → 1GB)                                   │
│  • Preemptive desde Go 1.14 (no se traban en loops)                       │
│  • El runtime migra goroutines entre threads                               │
│  • Sync code: net.Read() suspende goroutine, no el thread                 │
│  • 100k goroutines = ~200MB (aceptable)                                   │
│                                                                             │
│  RUST: async/await (cooperative scheduling via tokio)                      │
│  ────────────────────────────────────────────────────                      │
│                                                                             │
│  OS Thread 1          OS Thread 2          OS Thread 3                     │
│  ┌─────────────┐     ┌─────────────┐     ┌─────────────┐                  │
│  │ task A (fut) │     │ task D (fut) │     │ task G (fut) │                  │
│  │ task B (fut) │     │ task E (fut) │     │ task H (fut) │                  │
│  │ task C (fut) │     │ task F (fut) │     │ task I (fut) │                  │
│  └─────────────┘     └─────────────┘     └─────────────┘                  │
│                                                                             │
│  • Futures son state machines en el stack (no heap allocation por defecto) │
│  • Cooperative: tasks yield en cada .await point                           │
│  • CPU-heavy code puede bloquear thread → usar spawn_blocking             │
│  • 100k tasks = ~10-50MB (mucho menos que goroutines)                     │
│  • Pero: mas complejo (Pin, lifetimes, Send bounds)                       │
│                                                                             │
│  AMBOS:                                                                    │
│  • M:N threading (muchos tasks, pocos OS threads)                         │
│  • Work-stealing schedulers                                                │
│  • Excelente rendimiento con miles de conexiones concurrentes             │
└─────────────────────────────────────────────────────────────────────────────┘
```

| Aspecto | Go (goroutines) | Rust (tokio async) |
|---|---|---|
| Modelo | Preemptive M:N (desde Go 1.14) | Cooperative M:N |
| Syntax | `go func() {}()` | `tokio::spawn(async { })` |
| Scheduling | Runtime preemptive (~10ms timeslice) | Cooperative (yield en .await) |
| Stack | 2KB inicial, crece dinamicamente | State machine en stack, no extra stack |
| Memoria por task | ~2-8KB | ~64-256 bytes (solo el future) |
| CPU-bound work | Preemption evita starvation | Puede bloquear thread → `spawn_blocking` |
| Cancelacion | Context-based (propagacion manual) | Drop del future (automatico) |
| Paralelismo I/O | Goroutines + channels | `tokio::join!`, `FuturesUnordered` |
| Debug | Stack traces con `goroutine` names | Mas dificil (async stack traces aun limitados) |
| Data races | Posibles (detectables con `-race`) | Imposibles (compilador previene) |
| Deadlocks | Posibles (runtime no los detecta) | Posibles (async deadlocks son sutiles) |
| Color problem | No existe (todo es sync-looking) | Si: `async` contamina la API (`async fn` vs `fn`) |

### El "function coloring problem"

```go
// GO — no hay coloring
// Funciones sync y "async" tienen la misma firma
func fetchUser(ctx context.Context, id int) (*User, error) {
    // Internamente hace I/O async, pero la API es sync
    row := db.QueryRowContext(ctx, "SELECT * FROM users WHERE id = $1", id)
    // ...
}

// Puedes llamar cualquier funcion desde cualquier contexto
func handler(w http.ResponseWriter, r *http.Request) {
    user, err := fetchUser(r.Context(), 42) // sin await, sin async
    // ...
}
```

```rust
// RUST — async "contamina" la cadena de llamadas
async fn fetch_user(pool: &Pool<Postgres>, id: i32) -> Result<User, sqlx::Error> {
    sqlx::query_as!(User, "SELECT * FROM users WHERE id = $1", id)
        .fetch_one(pool)
        .await  // requiere .await
}

// Solo puedes llamar funciones async desde contextos async
async fn handler(State(pool): State<Pool<Postgres>>) -> Result<Json<User>, AppError> {
    let user = fetch_user(&pool, 42).await; // requiere .await
    // Cada capa intermedia tambien debe ser async
}

// Si necesitas llamar codigo sync desde async:
async fn handler() {
    // spawn_blocking para operaciones CPU-bound
    let result = tokio::task::spawn_blocking(|| {
        expensive_computation() // sync code en thread pool dedicado
    }).await.unwrap();
}
```

**El tradeoff**: Go evita el function coloring problem completamente — todo parece sync, el runtime maneja la magia. Esto es mas simple pero opaco: no sabes donde ocurre I/O mirando el codigo. Rust hace explicito cada punto de suspension con `.await`, lo que es mas verboso pero tambien mas transparente sobre el comportamiento del programa.

---

## 7. Timeouts y cancelacion

### Go: context.Context

```go
func main() {
    server := &http.Server{
        Addr:              ":8080",
        ReadTimeout:       5 * time.Second,   // tiempo para leer request completo
        ReadHeaderTimeout: 2 * time.Second,   // tiempo para leer headers
        WriteTimeout:      10 * time.Second,  // tiempo para escribir response
        IdleTimeout:       120 * time.Second, // keepalive idle
        Handler:           mux,
    }

    mux.HandleFunc("GET /data", func(w http.ResponseWriter, r *http.Request) {
        // Context se cancela cuando:
        // - El cliente desconecta
        // - WriteTimeout expira
        // - Server.Shutdown() es llamado
        ctx := r.Context()

        // Timeout por operacion
        ctx, cancel := context.WithTimeout(ctx, 3*time.Second)
        defer cancel()

        // Pasar context a toda la cadena
        data, err := fetchData(ctx)
        if err != nil {
            if ctx.Err() == context.DeadlineExceeded {
                http.Error(w, "timeout", http.StatusGatewayTimeout)
                return
            }
            http.Error(w, err.Error(), 500)
            return
        }
        json.NewEncoder(w).Encode(data)
    })
}

// Context se propaga manualmente por toda la cadena
func fetchData(ctx context.Context) (*Data, error) {
    req, _ := http.NewRequestWithContext(ctx, "GET", "http://api/data", nil)
    resp, err := http.DefaultClient.Do(req)
    // El request se cancela si ctx expira
    // ...
}
```

### Rust: tokio::time + CancellationToken

```rust
use axum::extract::Request;
use std::time::Duration;
use tokio::time::timeout;
use tokio_util::sync::CancellationToken;

// Server timeouts via tower
let app = Router::new()
    .route("/data", get(data_handler))
    .layer(
        ServiceBuilder::new()
            .layer(TimeoutLayer::new(Duration::from_secs(30))) // request timeout
    );

// Timeout por operacion
async fn data_handler() -> Result<Json<Data>, AppError> {
    // Opcion 1: tokio::time::timeout
    let data = timeout(Duration::from_secs(3), fetch_data())
        .await
        .map_err(|_| AppError::Timeout("fetch_data timed out"))?
        .map_err(AppError::Internal)?;

    Ok(Json(data))
}

// Opcion 2: CancellationToken (equivalente a context.Context)
async fn complex_handler(
    State(state): State<AppState>,
) -> Result<Json<Data>, AppError> {
    let token = CancellationToken::new();
    let child_token = token.child_token();

    let handle = tokio::spawn(async move {
        tokio::select! {
            result = fetch_data() => result,
            _ = child_token.cancelled() => Err(anyhow!("cancelled")),
        }
    });

    // Cancelar si tarda mas de 5 segundos
    tokio::select! {
        result = handle => result.unwrap(),
        _ = tokio::time::sleep(Duration::from_secs(5)) => {
            token.cancel(); // propagar cancelacion
            Err(AppError::Timeout("operation timed out"))
        }
    }
}

// Cancelacion automatica: si el cliente desconecta,
// axum droppea el future del handler, cancelando todo
// (los futures se cancelan cuando se droppean)
```

### Comparacion de cancelacion

| Aspecto | Go (context) | Rust (tokio) |
|---|---|---|
| Mecanismo | `context.Context` (valor, propagacion manual) | Drop de future (automatico) + `CancellationToken` |
| Cancelacion por cliente | Context se cancela | Future se droppea → cancel automatico |
| Timeout por request | `context.WithTimeout` | `tokio::time::timeout` o `TimeoutLayer` |
| Propagacion | Explicita (pasar ctx a cada funcion) | Implicita (drop propaga) + explicita (token) |
| Server timeouts | Campos en `http.Server` | `TimeoutLayer`, `hyper::server::conn::Builder` |
| Cleanup en cancelacion | `defer` + check `ctx.Err()` | `Drop` trait (deterministico) |
| Timeout granular | `ctx, cancel := context.WithTimeout(parent, d)` | `tokio::time::timeout(d, future)` |

**Ventaja de Go**: `context.Context` es un patron universal — toda la stdlib lo soporta (`database/sql`, `net/http`, `os/exec`). Un solo `ctx` fluye por toda la aplicacion.

**Ventaja de Rust**: la cancelacion es automatica — cuando droppeas un future, todo el arbol de computaciones se cancela sin leaks. En Go, si olvidas verificar `ctx.Err()`, el trabajo continua desperdiciandose.

---

## 8. Testing de APIs

### Go: httptest

```go
package api

import (
    "encoding/json"
    "net/http"
    "net/http/httptest"
    "strings"
    "testing"
)

// Unit test de un handler
func TestCreateItem(t *testing.T) {
    store := NewStore()
    handler := &Server{store: store}

    body := strings.NewReader(`{"name":"test item"}`)
    req := httptest.NewRequest("POST", "/items", body)
    req.Header.Set("Content-Type", "application/json")
    rec := httptest.NewRecorder()

    handler.handleCreateItem(rec, req)

    // Verificar response
    if rec.Code != http.StatusCreated {
        t.Errorf("status = %d, want %d", rec.Code, http.StatusCreated)
    }

    var item Item
    json.NewDecoder(rec.Body).Decode(&item)
    if item.Name != "test item" {
        t.Errorf("name = %q, want %q", item.Name, "test item")
    }
}

// Integration test con servidor real
func TestAPI(t *testing.T) {
    store := NewStore()
    mux := setupRouter(store)
    server := httptest.NewServer(mux)
    defer server.Close()

    // Usar server.URL para hacer requests reales
    resp, err := http.Post(server.URL+"/items",
        "application/json",
        strings.NewReader(`{"name":"test"}`))
    if err != nil {
        t.Fatal(err)
    }
    defer resp.Body.Close()

    if resp.StatusCode != http.StatusCreated {
        t.Errorf("status = %d, want %d", resp.StatusCode, http.StatusCreated)
    }
}

// Table-driven tests
func TestGetItem(t *testing.T) {
    tests := []struct {
        name       string
        id         string
        wantStatus int
    }{
        {"existing", "1", 200},
        {"not found", "999", 404},
        {"invalid id", "abc", 400},
    }

    store := NewStore()
    store.Create("item1")
    handler := &Server{store: store}

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            req := httptest.NewRequest("GET", "/items/"+tt.id, nil)
            req.SetPathValue("id", tt.id) // Go 1.22+
            rec := httptest.NewRecorder()

            handler.handleGetItem(rec, req)

            if rec.Code != tt.wantStatus {
                t.Errorf("status = %d, want %d", rec.Code, tt.wantStatus)
            }
        })
    }
}
```

### Rust: axum test utilities

```rust
use axum::{body::Body, http::{Request, StatusCode}};
use tower::ServiceExt; // for oneshot
use serde_json::json;

// Unit test de un handler (axum permite testear la app completa como Service)
#[tokio::test]
async fn test_create_item() {
    let app = create_app(); // retorna Router

    let response = app
        .oneshot(
            Request::builder()
                .method("POST")
                .uri("/items")
                .header("Content-Type", "application/json")
                .body(Body::from(r#"{"name":"test item"}"#))
                .unwrap(),
        )
        .await
        .unwrap();

    assert_eq!(response.status(), StatusCode::CREATED);

    let body = axum::body::to_bytes(response.into_body(), usize::MAX).await.unwrap();
    let item: Item = serde_json::from_slice(&body).unwrap();
    assert_eq!(item.name, "test item");
}

// Integration test con servidor real
#[tokio::test]
async fn test_api_integration() {
    let app = create_app();

    // Iniciar servidor en background
    let listener = tokio::net::TcpListener::bind("0.0.0.0:0").await.unwrap();
    let addr = listener.local_addr().unwrap();

    tokio::spawn(async move {
        axum::serve(listener, app).await.unwrap();
    });

    // Usar reqwest como cliente
    let client = reqwest::Client::new();
    let resp = client
        .post(format!("http://{addr}/items"))
        .json(&json!({"name": "test"}))
        .send()
        .await
        .unwrap();

    assert_eq!(resp.status(), StatusCode::CREATED);
}

// Table-driven tests con macro
#[tokio::test]
async fn test_get_item() {
    let app = create_app_with_seed(); // app con datos pre-cargados

    let cases = vec![
        ("existing",  "/items/1",   StatusCode::OK),
        ("not_found", "/items/999", StatusCode::NOT_FOUND),
        ("invalid",   "/items/abc", StatusCode::BAD_REQUEST),
    ];

    for (name, uri, expected_status) in cases {
        let response = app
            .clone() // Router es Clone
            .oneshot(
                Request::builder()
                    .uri(uri)
                    .body(Body::empty())
                    .unwrap(),
            )
            .await
            .unwrap();

        assert_eq!(
            response.status(), expected_status,
            "case '{name}': expected {expected_status}, got {}",
            response.status()
        );
    }
}
```

### Comparacion de testing

| Aspecto | Go (httptest) | Rust (axum/tower) |
|---|---|---|
| Unit test handler | `httptest.NewRequest` + `httptest.NewRecorder` | `app.oneshot(Request)` via tower `ServiceExt` |
| Integration test | `httptest.NewServer` (servidor real) | `TcpListener::bind("0.0.0.0:0")` + `tokio::spawn` |
| Table-driven | `for _, tt := range tests { t.Run(...) }` | `for (name, ...) in cases { }` |
| Paralelismo | `t.Parallel()` | `#[tokio::test]` es async por defecto |
| Client para tests | `http.Client` (stdlib) | `reqwest::Client` (crate externo) |
| Assertions | Manual (`if got != want { t.Errorf }`) | `assert_eq!` macro |
| Test helpers | Manual o testify | helpers custom o crates como `axum-test` |
| Mocking HTTP externo | `httptest.NewServer` como mock | `mockito`, `wiremock` crates |

**Ventaja de Go**: `httptest` esta en la stdlib, es simple, y no necesita async. `httptest.NewRecorder` es trivial.

**Ventaja de Rust**: `oneshot()` permite testear la app completa (router + middleware + extractors) sin levantar un servidor — es mas rapido y no necesita puertos TCP.

---

## 9. Rendimiento y benchmarks

### Benchmarks de referencia (TechEmpower Framework Benchmarks)

```
┌────────────────────────────────────────────────────────────────────────────┐
│            RENDIMIENTO HTTP (requests/segundo, mayor = mejor)             │
│            Fuente: TechEmpower Round 22 — plaintext                       │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  Framework         Lenguaje    RPS (aprox)      Notas                     │
│  ─────────────     ─────────   ───────────      ────────────────          │
│  may-minihttp      Rust        7,000,000+       bajo nivel, optimizado    │
│  axum              Rust        5,500,000+       framework completo        │
│  actix-web         Rust        5,000,000+       framework completo        │
│  hyper              Rust        4,500,000+       bajo nivel               │
│  ─────────────     ─────────   ───────────      ────────────────          │
│  fasthttp          Go          4,000,000+       no compatible net/http    │
│  fiber              Go          3,500,000+       basado en fasthttp       │
│  net/http (stdlib) Go          2,500,000+       stdlib, sin optimizar    │
│  gin               Go          2,300,000+       framework popular        │
│  echo              Go          2,200,000+       framework popular        │
│  chi               Go          2,000,000+       router minimalista       │
│                                                                            │
│  Nota: estos benchmarks miden throughput maximo en condiciones ideales.  │
│  En aplicaciones reales, la diferencia es mucho menor porque el          │
│  bottleneck suele ser I/O (database, network calls), no el framework.   │
│                                                                            │
│  Para JSON serialization:                                                 │
│  serde_json         Rust       ~2-3x mas rapido que encoding/json        │
│  simd-json          Rust       ~4-5x mas rapido que encoding/json        │
│  sonic-json         Go         ~2x mas rapido que encoding/json          │
│                                                                            │
│  Para database queries (el bottleneck real):                             │
│  sqlx (Rust) ≈ database/sql (Go) — la diferencia es <5%                 │
│  El driver de la DB y la query son lo que importa, no el ORM             │
└────────────────────────────────────────────────────────────────────────────┘
```

### Por que la diferencia de rendimiento?

```go
// GO — overhead que paga
// 1. Garbage collector (pausas <1ms, pero throughput ~5-10% menor)
// 2. Reflection para JSON (encoding/json usa reflect)
// 3. Interface dispatch (virtual calls en handlers, middleware)
// 4. Goroutine scheduling (preemption checks)
// 5. Memory allocations (strings, slices, interface boxing)
// 6. HTTP/2 en servidor (siempre habilitado con TLS)

// Ejemplo: cada JSON encode/decode usa reflection
json.Marshal(user)  // reflect.TypeOf, reflect.ValueOf en cada campo
```

```rust
// RUST — por que es mas rapido
// 1. Sin GC (ownership, zero-cost resource management)
// 2. Compile-time serialization (serde genera codigo especializado)
// 3. Monomorphization (generics = codigo especializado, no dispatch virtual)
// 4. Zero-cost abstractions (middleware layers resueltas en compilacion)
// 5. Stack allocation (futures son state machines, no heap objects)
// 6. SIMD optimizations (serde puede usar instrucciones SIMD)

// Ejemplo: serde genera codigo especializado en compilacion
#[derive(Serialize)]
struct User { id: u32, name: String }
// El compilador genera una funcion especializada para User,
// sin reflection, equivalente a escribir el JSON a mano
```

### Cuando importa (y cuando no)

```
┌────────────────────────────────────────────────────────────────────────────┐
│                    DONDE ESTA EL BOTTLENECK?                              │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  Operacion                     Latencia tipica     Framework importa?    │
│  ────────────────────          ────────────────     ──────────────────    │
│  Query PostgreSQL              1-10ms               No (DB domina)       │
│  Query Redis                   0.1-1ms              Casi no              │
│  HTTP call externo             10-500ms             No                   │
│  JSON encode (10 campos)       1-10μs               Solo en ultra-HPS   │
│  Routing + middleware          0.1-1μs               Solo en ultra-HPS   │
│  Business logic                0.01-1ms              Depende             │
│                                                                            │
│  "Ultra-HPS" = >100k requests/segundo sostenido                         │
│                                                                            │
│  CONCLUSION:                                                              │
│  Para la MAYORIA de APIs, Go y Rust rinden igual                        │
│  porque el bottleneck es I/O externo (DB, cache, APIs)                  │
│                                                                            │
│  Rust brilla cuando:                                                      │
│  • Processas payloads grandes (JSON, XML, binary)                       │
│  • Tienes logica CPU-heavy en cada request                              │
│  • Necesitas latencia P99 predecible (sin GC pauses)                    │
│  • Manejas >50k conexiones concurrentes con estado                      │
│  • El presupuesto de infraestructura es critico (menor CPU/memoria)     │
│                                                                            │
│  Go brilla cuando:                                                        │
│  • Quieres productividad maxima del equipo                              │
│  • El servicio es CRUD standard con DB backend                          │
│  • Necesitas iteracion rapida y deploys frecuentes                      │
│  • Tu equipo es grande y necesitas onboarding rapido                    │
│  • "Good enough" performance es suficiente (y casi siempre lo es)       │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 10. Seguridad de tipos: donde Rust previene bugs que Go no puede

### 10.1 Exhaustive pattern matching

```go
// GO — el compilador NO verifica que manejes todos los casos
type Role int
const (
    Admin Role = iota
    Editor
    Viewer
    // Si agregas Moderator aqui...
)

func permissions(role Role) []string {
    switch role {
    case Admin:
        return []string{"read", "write", "delete", "admin"}
    case Editor:
        return []string{"read", "write"}
    case Viewer:
        return []string{"read"}
    // ...Moderator no se maneja, compila OK, falla en runtime
    default:
        return nil // catch-all silencioso
    }
}
```

```rust
// RUST — el compilador FUERZA que manejes todos los casos
enum Role {
    Admin,
    Editor,
    Viewer,
}

fn permissions(role: &Role) -> Vec<&str> {
    match role {
        Role::Admin => vec!["read", "write", "delete", "admin"],
        Role::Editor => vec!["read", "write"],
        Role::Viewer => vec!["read"],
        // Si agregas Role::Moderator al enum,
        // TODOS los match expressions dan error de compilacion
        // hasta que lo manejes
    }
}
```

### 10.2 Null safety

```go
// GO — nil pointers causan panic en runtime
type User struct {
    Name    string
    Profile *Profile // puede ser nil
}

func getAvatar(user *User) string {
    // Esto compila, pero panic si user es nil o Profile es nil
    return user.Profile.AvatarURL
    // Necesitas checks manuales:
    if user == nil || user.Profile == nil {
        return ""
    }
    return user.Profile.AvatarURL
}
```

```rust
// RUST — Option<T> fuerza manejo explicito
struct User {
    name: String,
    profile: Option<Profile>,  // explicitamente opcional
}

fn get_avatar(user: &User) -> &str {
    // No puedes acceder sin manejar el None
    // Esto NO compila: user.profile.avatar_url

    // Debes ser explicito:
    match &user.profile {
        Some(profile) => &profile.avatar_url,
        None => "",
    }
    // O con chaining:
    user.profile.as_ref().map(|p| p.avatar_url.as_str()).unwrap_or("")
}
```

### 10.3 Thread safety

```go
// GO — data races son posibles y solo detectables con -race
type Counter struct {
    count int // SIN proteccion
}

func (c *Counter) Increment() {
    c.count++ // DATA RACE si multiples goroutines
}

// Esto compila sin warning, falla solo bajo concurrencia
func main() {
    c := &Counter{}
    for i := 0; i < 1000; i++ {
        go c.Increment() // data race!
    }
}
// Solo detectable con: go run -race main.go
```

```rust
// RUST — el compilador previene data races
struct Counter {
    count: u32,
}

impl Counter {
    fn increment(&mut self) {
        self.count += 1;
    }
}

fn main() {
    let counter = Counter { count: 0 };

    // Esto NO COMPILA:
    // std::thread::spawn(|| counter.increment());
    // Error: `Counter` cannot be shared between threads safely
    //        `Counter` doesn't implement `Send`... actually it does implement Send,
    //        but we have a move/borrow conflict

    // Solucion: Arc<Mutex<Counter>>
    let counter = Arc::new(Mutex::new(Counter { count: 0 }));
    for _ in 0..1000 {
        let counter = Arc::clone(&counter);
        std::thread::spawn(move || {
            counter.lock().unwrap().increment();
        });
    }
    // El compilador FUERZA la sincronizacion
}
```

### 10.4 Error handling exhaustivo

```go
// GO — facil olvidar manejar errores
func processRequest(r *http.Request) {
    data, _ := io.ReadAll(r.Body) // _ ignora el error silenciosamente!
    // Esto compila sin warning

    result, err := json.Marshal(data)
    // Si olvidas el if err != nil, usas result que puede ser nil
    fmt.Println(string(result))
}
```

```rust
// RUST — Result<T, E> debe consumirse
fn process_request(body: &[u8]) -> Result<String, Error> {
    let data: Value = serde_json::from_slice(body)?; // ? propaga error

    // Si no usas Result, el compilador da warning:
    // "unused `Result` that must be used"
    // #[must_use] previene ignorar errores silenciosamente

    Ok(serde_json::to_string(&data)?)
}
```

### 10.5 Request/Response type safety

```go
// GO — el handler no tiene informacion de tipos
func createUser(w http.ResponseWriter, r *http.Request) {
    // Nada en la firma dice que espera JSON o que retorna JSON
    // Puedes:
    // - Olvidar leer el body
    // - Escribir HTML en vez de JSON
    // - Olvidar el Content-Type
    // - Escribir status despues del body (no-op silencioso)
    // - Escribir multiples veces al ResponseWriter (se concatena)

    w.WriteHeader(201)
    w.Header().Set("Content-Type", "application/json") // BUG: header despues de WriteHeader
    // El header NO se aplica — pero compila y corre sin error
}
```

```rust
// RUST — la firma documenta y verifica la API
async fn create_user(
    Json(input): Json<CreateUserInput>,  // DEBE recibir JSON valido
) -> (StatusCode, Json<User>) {          // DEBE retornar status + JSON
    // Si input no es JSON valido: 400 automatico (nunca llega aqui)
    // El tipo de retorno FUERZA que retornes exactamente un status y un JSON
    // No puedes "olvidar" enviar la respuesta — el compilador te obliga

    let user = User { /* ... */ };
    (StatusCode::CREATED, Json(user))
    // Content-Type se establece automaticamente por Json<T>
}
```

### Resumen de seguridad de tipos

```
┌────────────────────────────────────────────────────────────────────────────┐
│                    BUGS QUE CADA LENGUAJE PREVIENE                        │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  Tipo de bug                      Go          Rust                        │
│  ────────────────────────         ─────────   ─────────                   │
│  Null pointer dereference         Runtime     Compilacion (Option<T>)     │
│  Data races                       -race flag  Compilacion (Send+Sync)     │
│  Missing enum case                Runtime     Compilacion (exhaustive)    │
│  Ignored error                    Runtime     Compilacion (#[must_use])   │
│  Wrong response type              Runtime     Compilacion (IntoResponse)  │
│  Invalid path param               Runtime     Compilacion (extractors)    │
│  Missing Content-Type             Runtime     Compilacion (Json<T>)       │
│  Header after body                Silencioso  Imposible (tipo consume)    │
│  Use after free                   Imposible*  Imposible (ownership)      │
│  Buffer overflow                  Imposible*  Imposible (bounds check)   │
│  Double free                      Imposible*  Imposible (ownership)      │
│                                                                            │
│  * Go previene memory-safety bugs via GC                                  │
│  Ambos son memory-safe, pero Rust ademas es type-safe para concurrencia  │
│                                                                            │
│  TRADEOFF:                                                                │
│  Go: menos bugs de memoria, pero errores de logica escapan al runtime   │
│  Rust: casi todos los bugs se atrapan en compilacion, pero mayor         │
│        tiempo de desarrollo y curva de aprendizaje                        │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 11. Ecosistema y dependencias

### Go: stdlib-first

```
┌────────────────────────────────────────────────────────────────────────────┐
│  APLICACION WEB TIPICA EN GO                                              │
│  Total de dependencias directas: 3-8                                      │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  Componente              Stdlib            Popular (si necesitas mas)     │
│  ────────────────        ─────────────     ────────────────────────       │
│  HTTP server             net/http          chi, gin, echo                 │
│  HTTP client             net/http          resty (solo conveniencia)      │
│  JSON                    encoding/json     sonic-json, jsoniter           │
│  Router                  ServeMux 1.22+    chi (si necesitas groups)     │
│  Middleware              manual            chi middleware, alice           │
│  Templates               html/template     —                             │
│  TLS                     crypto/tls        —                             │
│  Testing                 testing           testify (assertions), gomock  │
│  Logging                 log/slog (1.21+)  zap, zerolog                   │
│  Config                  os.Getenv         viper, envconfig              │
│  Database                database/sql      sqlx, pgx, gorm              │
│  Validation              manual            validator                      │
│  Auth                    manual            jwt-go, casbin                 │
│  Metrics                 expvar            prometheus/client_golang       │
│                                                                            │
│  go.sum tipico: 20-50 dependencias transitivas                           │
└────────────────────────────────────────────────────────────────────────────┘
```

### Rust: crate ecosystem

```
┌────────────────────────────────────────────────────────────────────────────┐
│  APLICACION WEB TIPICA EN RUST                                            │
│  Total de dependencias directas: 15-30                                    │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  Componente              Crate primario       Alternativas               │
│  ────────────────        ─────────────────    ────────────────            │
│  Async runtime           tokio                async-std, smol            │
│  HTTP server             axum                 actix-web, rocket, warp    │
│  HTTP client             reqwest              hyper (bajo nivel), ureq   │
│  JSON                    serde + serde_json   simd-json                  │
│  Router                  axum (integrado)     —                          │
│  Middleware              tower + tower-http   —                          │
│  Templates               askama, tera         minijinja                  │
│  TLS                     rustls               native-tls, openssl       │
│  Testing                 (stdlib)             —                          │
│  Logging/Tracing         tracing              log + env_logger           │
│  Config                  config               figment, dotenvy           │
│  Database                sqlx                 diesel, sea-orm            │
│  Validation              validator            garde                      │
│  Auth                    jsonwebtoken         —                          │
│  Metrics                 prometheus           metrics                    │
│  Error handling          anyhow + thiserror   eyre, color-eyre          │
│  CLI                     clap                 —                          │
│  Date/Time               chrono               time                       │
│                                                                            │
│  Cargo.lock tipico: 200-500 dependencias transitivas                     │
└────────────────────────────────────────────────────────────────────────────┘
```

### Comparacion de ecosistemas

| Aspecto | Go | Rust |
|---|---|---|
| Stdlib coverage | ~80% de necesidades web | ~20% (sin HTTP, JSON, async) |
| Dependencias directas tipicas | 3-8 | 15-30 |
| Dependencias transitivas | 20-50 | 200-500 |
| Package manager | `go mod` (integrado) | `cargo` (integrado) |
| Central registry | proxy.golang.org | crates.io |
| Audit | `govulncheck` | `cargo audit` |
| Compilacion | Rapida (5-30s proyecto tipico) | Lenta (2-15min primera vez, 10-60s incremental) |
| Binary size | 10-20MB (con debug info) | 2-10MB (stripped) |
| Docker image | ~10-20MB (scratch + binary) | ~5-15MB (scratch + binary) |
| Cross-compilation | Trivial (`GOOS=linux GOARCH=amd64`) | Requiere targets y toolchains |

**Tiempo de compilacion** — la diferencia mas visible en el dia a dia:

```
┌────────────────────────────────────────────────────────────────────────────┐
│  TIEMPO DE COMPILACION (proyecto web tipico)                              │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  Escenario                Go              Rust                            │
│  ────────────────         ──────────      ──────────                      │
│  Build limpio             5-15s           2-10min                         │
│  Build incremental        1-3s            10-30s                          │
│  Cambio en un .go/.rs     <1s             5-15s                           │
│  Tests                    2-10s           30s-3min                        │
│  CI/CD pipeline           30s-2min        5-20min                         │
│                                                                            │
│  Impacto: en Rust el ciclo edit-compile-test es significativamente       │
│  mas lento. Herramientas como cargo-watch y mold linker ayudan,          │
│  pero no eliminan la diferencia.                                          │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 12. Tabla comparativa comprehensiva

| Categoria | Aspecto | Go (net/http) | Rust (axum/hyper) |
|---|---|---|---|
| **Setup** | Dependencies | 0 (stdlib) | 5+ crates minimo |
| | Hello world | 8 lineas | 15 lineas |
| | Tiempo a productividad | Horas | Semanas |
| | Compilacion incremental | <1s | 5-15s |
| **HTTP Server** | Handler firma | `func(w, r)` uniforme | `async fn(extractors...) -> impl IntoResponse` |
| | Path params | `r.PathValue("id")` → string | `Path(id): Path<u32>` → tipado |
| | Query params | `r.URL.Query().Get("key")` → string | `Query(p): Query<Params>` → struct tipado |
| | JSON body | `json.NewDecoder(r.Body).Decode(&v)` manual | `Json(v): Json<T>` auto + validacion |
| | Response | `w.WriteHeader` + `json.Encode` manual | `(StatusCode, Json<T>)` tipado |
| | Router | ServeMux 1.22+ (basico pero suficiente) | axum Router (method chaining, nest, layers) |
| | Middleware | `func(Handler) Handler` manual | tower `Layer` + `ServiceBuilder` |
| **HTTP Client** | Creacion | `&http.Client{}` struct literal | `Client::builder().build()` |
| | Request | `NewRequestWithContext` + headers manuales | Builder chain fluent |
| | Response body | `defer resp.Body.Close()` manual | Ownership auto-consume |
| | JSON decode | `json.NewDecoder.Decode` manual | `.json::<T>().await` |
| | Error status | `if resp.StatusCode != 200` manual | `.error_for_status()` |
| **Concurrencia** | Modelo | Goroutines (sync-looking) | async/await (explicit) |
| | Creacion | `go func(){}()` | `tokio::spawn(async {})` |
| | Overhead por task | ~2-8KB | ~64-256 bytes |
| | Cancelacion | `context.Context` (manual) | Drop (automatico) |
| | Data races | Posibles (-race para detectar) | Imposibles (compilador) |
| | Coloring problem | No existe | Si (async contamina) |
| **Serialization** | Mecanismo | Reflection (runtime) | Macros (compile-time) |
| | Rendimiento | Base | 2-5x mas rapido |
| | Enums | No soportado | First-class |
| | Multiformato | No (solo JSON) | Si (serde es format-agnostic) |
| **Type Safety** | Null safety | Nil pointers (runtime panic) | Option<T> (compile-time) |
| | Error handling | `if err != nil` (facil ignorar) | `Result<T,E>` (#[must_use]) |
| | Exhaustive match | No | Si |
| | Thread safety | Race detector (runtime) | Compilador (Send+Sync) |
| **Rendimiento** | Throughput HTTP | ~2.5M req/s (stdlib) | ~5.5M req/s (axum) |
| | Latencia P99 | Buena (GC <1ms pausas) | Excelente (sin GC) |
| | Memoria | Mayor (GC overhead) | Menor (no GC) |
| | Startup | Rapido (~10-50ms) | Muy rapido (~1-10ms) |
| **Ecosistema** | Deps tipicas | 3-8 directas | 15-30 directas |
| | Stdlib HTTP | Completa | No existe (crates) |
| | Compilacion limpia | 5-15s | 2-10min |
| | Binary size | 10-20MB | 2-10MB (stripped) |
| | Cross-compile | Trivial | Requiere setup |
| **Operaciones** | Deploy | Binary estatico | Binary estatico |
| | Docker | ~10-20MB | ~5-15MB |
| | Observabilidad | pprof, expvar, trace | tokio-console, tracing |
| | Profiling | Integrado (pprof) | perf, flamegraph |
| **Equipo** | Onboarding | Rapido (lenguaje simple) | Lento (ownership, lifetimes) |
| | Code review | Facil (pocas formas de hacer algo) | Mas complejo (mas abstracciones) |
| | Hiring | Facil (pool grande) | Dificil (pool pequeno) |
| | Mantenimiento | Simple (idiomatico, uniforme) | Complejo (traits, generics, macros) |

---

## 13. Cuando elegir cada uno

```
┌────────────────────────────────────────────────────────────────────────────┐
│                    GUIA DE DECISION                                        │
├────────────────────────────────────────────────────────────────────────────┤
│                                                                            │
│  ELIGE GO CUANDO:                                                         │
│                                                                            │
│  ✓ CRUD APIs con database backend (el caso mas comun)                    │
│  ✓ Microservicios que necesitan iteracion rapida                         │
│  ✓ Equipos grandes o con rotacion frecuente                              │
│  ✓ Prototipado rapido con posibilidad de escalar                         │
│  ✓ Infraestructura y tooling (CLI, DevOps, orquestadores)               │
│  ✓ Proxies y load balancers (alta concurrencia, logica simple)           │
│  ✓ El rendimiento de la stdlib es "good enough" (y casi siempre lo es)  │
│  ✓ Tiempo de compilacion importa (CI/CD, developer experience)          │
│  ✓ Quieres dependencias minimas y stdlib coverage maxima                │
│                                                                            │
│  Ejemplos reales en Go:                                                   │
│  • Kubernetes, Docker, Terraform, CockroachDB, Prometheus               │
│  • APIs de Cloudflare, Dropbox, Uber, Twitch                            │
│  • La mayoria de microservicios en Google                                │
│                                                                            │
│  ─────────────────────────────────────────────────────────────────────    │
│                                                                            │
│  ELIGE RUST CUANDO:                                                       │
│                                                                            │
│  ✓ Latencia P99 predecible es critica (trading, gaming, real-time)      │
│  ✓ Procesamiento CPU-heavy en cada request (ML inference, crypto)       │
│  ✓ Alta densidad de conexiones con estado (WebSocket servers, chat)     │
│  ✓ Infraestructura de red (proxies, CDN edge, DNS servers)              │
│  ✓ Servicios con requisitos de memoria estrictos (edge, embedded)       │
│  ✓ Seguridad de tipos es critica (fintech, aerospace, medical)          │
│  ✓ El equipo ya conoce Rust y valora sus garantias                      │
│  ✓ El servicio es de larga vida y el costo de compilacion se amortiza  │
│  ✓ Necesitas interop con C sin FFI overhead                             │
│                                                                            │
│  Ejemplos reales en Rust:                                                 │
│  • Cloudflare (edge computing), Discord (messages), Figma (multiplayer) │
│  • AWS Firecracker (microVMs), Linkerd2-proxy, Deno                     │
│  • Datadog Agent, 1Password                                              │
│                                                                            │
│  ─────────────────────────────────────────────────────────────────────    │
│                                                                            │
│  NO ELIJAS BASADO EN:                                                     │
│                                                                            │
│  ✗ "Rust es mas rapido" — para la mayoria de APIs, la DB es el          │
│    bottleneck, no el framework                                            │
│  ✗ "Go es mas simple" — si necesitas las garantias de Rust, la          │
│    simplicidad de Go te costara bugs en produccion                       │
│  ✗ Benchmarks sinteticos — miden el framework, no tu aplicacion         │
│  ✗ Hype o popularidad — elige por las necesidades del proyecto          │
└────────────────────────────────────────────────────────────────────────────┘
```

---

## 14. Ejercicios

### Ejercicio 1: Implementar el mismo API en ambos lenguajes

Crea un servicio de "URL Shortener" con la misma funcionalidad en Go y Rust:

**Requisitos**:
- `POST /shorten` — recibir URL larga, retornar URL corta
- `GET /{code}` — redireccionar a la URL original
- `GET /stats/{code}` — retornar estadisticas (clicks, fecha de creacion)
- Store en memoria con mutex/RwLock
- Middleware de logging
- Graceful shutdown

**Comparar**:
1. Lineas de codigo totales
2. Numero de dependencias
3. Tiempo de compilacion (build limpio e incremental)
4. Rendimiento con `wrk` o `hey` (req/s, latencia P50/P99)
5. Uso de memoria bajo carga (1000 concurrent connections)

### Ejercicio 2: Middleware chain

Implementa la misma cadena de middleware en ambos lenguajes:
1. **Request ID** — agregar header `X-Request-ID` (UUID)
2. **Logger** — loggear method, path, status, duration
3. **Rate Limiter** — token bucket, 100 req/s por IP
4. **Auth** — verificar header `Authorization: Bearer <token>`
5. **CORS** — configurar origenes, metodos, headers permitidos

Comparar:
- Ergonomia del patron de middleware
- Composicion y orden de ejecucion
- Testing del middleware individual vs cadena completa

### Ejercicio 3: Cliente HTTP resiliente

Implementa un cliente HTTP con las mismas caracteristicas en ambos:
1. **Retries** — exponential backoff con jitter, max 3 intentos
2. **Circuit breaker** — abrir despues de 5 failures, half-open despues de 30s
3. **Timeout** — 5s por request, 30s total
4. **Connection pooling** — max 10 conexiones por host

Comparar:
- Cantidad de codigo necesario
- Manejo de errores (tipos de error, exhaustividad)
- Cancelacion y cleanup

### Ejercicio 4: Analisis critico

Escribe un documento (1-2 paginas) respondiendo:
1. Si tuvieras que elegir UN lenguaje para un startup que va a construir una plataforma SaaS, cual elegirias y por que?
2. En que punto del crecimiento de la empresa reconsiderarias? (usuarios, requests/s, equipo)
3. Es viable un enfoque hibrido? (Go para API gateway + Rust para servicios criticos) Que problemas tendrias?
4. Como cambia la decision si el equipo tiene 3 personas vs 30?

---

> **Fin del Capitulo 10: Networking y HTTP**
> **Siguiente**: C11 — Genericos y Testing, S01 - Genericos (Go 1.18+), T01 - Funciones genericas — type parameters, constraints, comparable, any
