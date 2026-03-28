# HTTP Client

## Índice

1. [¿Por qué reqwest?](#1-por-qué-reqwest)
2. [Setup y primer request](#2-setup-y-primer-request)
3. [Métodos HTTP: GET, POST, PUT, DELETE](#3-métodos-http-get-post-put-delete)
4. [Trabajar con JSON](#4-trabajar-con-json)
5. [Headers y autenticación](#5-headers-y-autenticación)
6. [Manejo de errores de red](#6-manejo-de-errores-de-red)
7. [Configuración del Client](#7-configuración-del-client)
8. [Respuestas grandes y streaming](#8-respuestas-grandes-y-streaming)
9. [Patrones prácticos](#9-patrones-prácticos)
10. [Errores comunes](#10-errores-comunes)
11. [Cheatsheet](#11-cheatsheet)
12. [Ejercicios](#12-ejercicios)

---

## 1. ¿Por qué reqwest?

HTTP es un protocolo sobre TCP con reglas complejas: framing de headers, chunked transfer
encoding, TLS, redirecciones, cookies, compresión, keep-alive, y más. Implementar todo esto
sobre `tokio::net::TcpStream` sería impracticable. `reqwest` es el cliente HTTP estándar del
ecosistema Rust.

```
┌─── Pila de un HTTP client en Rust ────────────────┐
│                                                    │
│  Tu código                                         │
│  ┌──────────────────────────────────────┐          │
│  │         reqwest                       │          │
│  │  API de alto nivel: get, post, json   │          │
│  ├──────────────────────────────────────┤          │
│  │         hyper                         │          │
│  │  Implementación HTTP/1.1 y HTTP/2     │          │
│  ├──────────────────────────────────────┤          │
│  │     rustls / native-tls               │          │
│  │  TLS (HTTPS)                          │          │
│  ├──────────────────────────────────────┤          │
│  │         tokio                         │          │
│  │  I/O async, TCP, DNS                  │          │
│  └──────────────────────────────────────┘          │
│                                                    │
└────────────────────────────────────────────────────┘
```

### Cargo.toml

```toml
[dependencies]
reqwest = { version = "0.12", features = ["json"] }
tokio = { version = "1", features = ["full"] }
serde = { version = "1", features = ["derive"] }
serde_json = "1"
```

La feature `json` habilita `.json()` en requests y responses. Sin ella, tendrías que
serializar/deserializar manualmente.

---

## 2. Setup y primer request

### GET simple

```rust
#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let body = reqwest::get("https://httpbin.org/ip")
        .await?      // Enviar request, esperar respuesta
        .text()      // Leer body como String
        .await?;     // Esperar lectura completa del body

    println!("Response: {}", body);

    Ok(())
}
```

Nota los **dos `.await`**: el primero espera la conexión TCP + headers de respuesta, el
segundo espera la lectura completa del body. Son operaciones separadas porque podrías
querer inspeccionar los headers antes de leer el body.

### El flujo de un request

```
┌─── Ciclo de vida de un HTTP request ──────────────────────┐
│                                                            │
│  reqwest::get(url)                                         │
│       │                                                    │
│       ▼                                                    │
│  DNS lookup (async)                                        │
│       │                                                    │
│       ▼                                                    │
│  TCP connect (async)                                       │
│       │                                                    │
│       ▼                                                    │
│  TLS handshake si HTTPS (async)                            │
│       │                                                    │
│       ▼                                                    │
│  Enviar headers HTTP (async)       ← primer .await         │
│       │                                                    │
│       ▼                                                    │
│  Recibir status + headers          ← Response              │
│       │                                                    │
│       ▼                                                    │
│  Leer body: .text() / .json()      ← segundo .await       │
│       │                                                    │
│       ▼                                                    │
│  Resultado final                                           │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Inspeccionar la Response antes del body

```rust
#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let response = reqwest::get("https://httpbin.org/get").await?;

    // Inspeccionar metadata ANTES de leer el body
    println!("Status: {}", response.status());
    println!("Headers: {:#?}", response.headers());
    println!("Content-Length: {:?}",
        response.content_length());

    // Ahora sí leer el body (consume response)
    let body = response.text().await?;
    println!("Body: {}", body);

    Ok(())
}
```

> **Importante**: métodos como `.text()`, `.json()`, `.bytes()` **consumen** la `Response`.
> Solo puedes llamar uno de ellos, y solo una vez. Si necesitas los headers después de leer
> el body, guárdalos antes.

### Client reutilizable

`reqwest::get()` crea un `Client` interno cada vez. Para mejor rendimiento, crea un `Client`
y reutilízalo — mantiene un pool de conexiones TCP:

```rust
use reqwest::Client;

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    // Crear UNA vez, reutilizar para todos los requests
    let client = Client::new();

    // El client mantiene un connection pool interno
    let resp1 = client.get("https://httpbin.org/ip").send().await?;
    let resp2 = client.get("https://httpbin.org/headers").send().await?;
    // La segunda request puede reutilizar la conexión TCP de la primera

    println!("IP: {}", resp1.text().await?);
    println!("Headers: {}", resp2.text().await?);

    Ok(())
}
```

```
┌─── reqwest::get() vs Client ──────────────────────┐
│                                                    │
│  reqwest::get(url):                                │
│  • Crea Client temporal cada vez                   │
│  • Nueva conexión TCP por request                  │
│  • Conveniente para scripts/one-off                │
│                                                    │
│  Client::new() + client.get(url):                  │
│  • Un Client con connection pool                   │
│  • Reutiliza conexiones TCP (keep-alive)           │
│  • Comparte configuración (timeouts, headers)      │
│  • Recomendado para aplicaciones                   │
│                                                    │
└────────────────────────────────────────────────────┘
```

---

## 3. Métodos HTTP: GET, POST, PUT, DELETE

### GET con query parameters

```rust
use reqwest::Client;

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();

    // Opción 1: query params en la URL
    let resp = client
        .get("https://httpbin.org/get?name=alice&age=30")
        .send()
        .await?;
    println!("{}", resp.text().await?);

    // Opción 2: .query() — más limpio, escapa caracteres especiales
    let resp = client
        .get("https://httpbin.org/get")
        .query(&[("name", "alice"), ("age", "30")])
        .send()
        .await?;
    println!("{}", resp.text().await?);

    // Opción 3: .query() con struct
    #[derive(serde::Serialize)]
    struct Params {
        name: String,
        age: u32,
        active: bool,
    }

    let params = Params {
        name: "alice".to_string(),
        age: 30,
        active: true,
    };

    let resp = client
        .get("https://httpbin.org/get")
        .query(&params)  // Serializa como ?name=alice&age=30&active=true
        .send()
        .await?;
    println!("{}", resp.text().await?);

    Ok(())
}
```

### POST con body

```rust
use reqwest::Client;

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();

    // POST con body de texto plano
    let resp = client
        .post("https://httpbin.org/post")
        .body("raw text body")
        .send()
        .await?;
    println!("Text: {}", resp.status());

    // POST con form data (application/x-www-form-urlencoded)
    let resp = client
        .post("https://httpbin.org/post")
        .form(&[("username", "alice"), ("password", "secret")])
        .send()
        .await?;
    println!("Form: {}", resp.status());

    // POST con JSON (requiere feature "json")
    let resp = client
        .post("https://httpbin.org/post")
        .json(&serde_json::json!({
            "name": "alice",
            "items": [1, 2, 3]
        }))
        .send()
        .await?;
    println!("JSON: {}", resp.status());

    Ok(())
}
```

### PUT y DELETE

```rust
use reqwest::Client;
use serde::{Serialize, Deserialize};

#[derive(Serialize)]
struct UpdateUser {
    name: String,
    email: String,
}

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();

    // PUT: actualizar recurso
    let resp = client
        .put("https://httpbin.org/put")
        .json(&UpdateUser {
            name: "alice".to_string(),
            email: "alice@example.com".to_string(),
        })
        .send()
        .await?;
    println!("PUT: {}", resp.status());

    // DELETE: eliminar recurso
    let resp = client
        .delete("https://httpbin.org/delete")
        .send()
        .await?;
    println!("DELETE: {}", resp.status());

    // PATCH: actualización parcial
    let resp = client
        .patch("https://httpbin.org/patch")
        .json(&serde_json::json!({"email": "new@example.com"}))
        .send()
        .await?;
    println!("PATCH: {}", resp.status());

    Ok(())
}
```

### Tabla resumen de métodos

| Método   | Propósito                    | Body típico | Builder              |
|----------|------------------------------|:-----------:|----------------------|
| GET      | Obtener recurso              | No          | `.get(url)`          |
| POST     | Crear recurso / enviar datos | Sí          | `.post(url)`         |
| PUT      | Reemplazar recurso completo  | Sí          | `.put(url)`          |
| PATCH    | Actualización parcial        | Sí          | `.patch(url)`        |
| DELETE   | Eliminar recurso             | Rara vez    | `.delete(url)`       |
| HEAD     | Solo headers (sin body)      | No          | `.head(url)`         |

---

## 4. Trabajar con JSON

### Deserializar respuestas JSON

```rust
use serde::Deserialize;
use reqwest::Client;

#[derive(Debug, Deserialize)]
struct IpResponse {
    origin: String,
}

#[derive(Debug, Deserialize)]
struct HttpBinResponse {
    args: std::collections::HashMap<String, String>,
    headers: std::collections::HashMap<String, String>,
    origin: String,
    url: String,
}

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();

    // .json() deserializa directamente al tipo
    let ip: IpResponse = client
        .get("https://httpbin.org/ip")
        .send()
        .await?
        .json()     // Deserializa JSON → IpResponse
        .await?;

    println!("My IP: {}", ip.origin);

    // Con tipo más complejo
    let data: HttpBinResponse = client
        .get("https://httpbin.org/get")
        .query(&[("foo", "bar")])
        .send()
        .await?
        .json()
        .await?;

    println!("URL: {}", data.url);
    println!("Args: {:?}", data.args);

    Ok(())
}
```

### Serializar structs en requests

```rust
use serde::{Serialize, Deserialize};
use reqwest::Client;

#[derive(Serialize)]
struct CreateUser {
    name: String,
    email: String,
    role: String,
}

#[derive(Debug, Deserialize)]
struct ApiResponse {
    json: serde_json::Value,  // Para respuestas con estructura desconocida
}

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();

    let new_user = CreateUser {
        name: "alice".to_string(),
        email: "alice@example.com".to_string(),
        role: "admin".to_string(),
    };

    let resp: ApiResponse = client
        .post("https://httpbin.org/post")
        .json(&new_user)    // Serializa CreateUser → JSON body
        .send()
        .await?
        .json()             // Deserializa respuesta → ApiResponse
        .await?;

    println!("Server received: {}", resp.json);

    Ok(())
}
```

### serde_json::Value para JSON dinámico

Cuando no conoces la estructura exacta de la respuesta:

```rust
use reqwest::Client;
use serde_json::Value;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let client = Client::new();

    // Deserializar a Value (JSON genérico)
    let data: Value = client
        .get("https://httpbin.org/get")
        .send()
        .await?
        .json()
        .await?;

    // Acceder a campos con indexación
    println!("Origin: {}", data["origin"]);
    println!("Host header: {}", data["headers"]["Host"]);

    // Acceso seguro con .get()
    if let Some(url) = data.get("url").and_then(|v| v.as_str()) {
        println!("URL: {}", url);
    }

    // Iterar sobre un array
    if let Some(items) = data.get("items").and_then(|v| v.as_array()) {
        for item in items {
            println!("Item: {}", item);
        }
    }

    Ok(())
}
```

### Campos opcionales y defaults

```rust
use serde::Deserialize;

#[derive(Debug, Deserialize)]
struct User {
    name: String,
    email: String,

    #[serde(default)]               // Si no está presente: 0
    age: u32,

    #[serde(default)]               // Si no está presente: None
    bio: Option<String>,

    #[serde(default = "default_role")]  // Si no está presente: "user"
    role: String,

    #[serde(rename = "created_at")]    // El campo JSON se llama "created_at"
    created: Option<String>,

    #[serde(alias = "user_name")]      // Acepta "user_name" o "name"
    username: Option<String>,
}

fn default_role() -> String {
    "user".to_string()
}
```

Esto es crítico al consumir APIs reales, donde los campos pueden estar ausentes, tener
nombres distintos a las convenciones de Rust, o cambiar entre versiones.

---

## 5. Headers y autenticación

### Headers custom

```rust
use reqwest::{Client, header};

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();

    let resp = client
        .get("https://httpbin.org/headers")
        .header("X-Custom-Header", "my-value")
        .header(header::ACCEPT, "application/json")
        .header(header::USER_AGENT, "my-rust-app/1.0")
        .send()
        .await?;

    println!("{}", resp.text().await?);

    Ok(())
}
```

### Headers por defecto en el Client

```rust
use reqwest::{Client, header};

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let mut default_headers = header::HeaderMap::new();
    default_headers.insert(
        header::ACCEPT,
        header::HeaderValue::from_static("application/json"),
    );
    default_headers.insert(
        "X-Api-Version",
        header::HeaderValue::from_static("2"),
    );

    let client = Client::builder()
        .default_headers(default_headers)
        .build()?;

    // Todos los requests incluyen estos headers automáticamente
    let resp = client.get("https://httpbin.org/headers")
        .send()
        .await?;
    println!("{}", resp.text().await?);

    Ok(())
}
```

### Bearer token (OAuth/JWT)

```rust
use reqwest::Client;

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();
    let token = "my-secret-token";  // Normalmente de variable de entorno

    let resp = client
        .get("https://httpbin.org/bearer")
        .bearer_auth(token)  // Agrega "Authorization: Bearer my-secret-token"
        .send()
        .await?;

    println!("Status: {}", resp.status());

    Ok(())
}
```

### Basic auth

```rust
use reqwest::Client;

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();

    let resp = client
        .get("https://httpbin.org/basic-auth/alice/password123")
        .basic_auth("alice", Some("password123"))
        .send()
        .await?;

    println!("Status: {}", resp.status());  // 200 si credenciales correctas

    Ok(())
}
```

### Leer headers de respuesta

```rust
use reqwest::{Client, header};

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();

    let resp = client
        .get("https://httpbin.org/response-headers?X-Custom=hello")
        .send()
        .await?;

    // Header específico
    if let Some(ct) = resp.headers().get(header::CONTENT_TYPE) {
        println!("Content-Type: {:?}", ct);
    }

    // Iterar todos los headers
    for (name, value) in resp.headers() {
        println!("{}: {:?}", name, value);
    }

    // Header custom
    if let Some(custom) = resp.headers().get("x-custom") {
        println!("X-Custom: {}", custom.to_str().unwrap_or("invalid utf8"));
    }

    Ok(())
}
```

---

## 6. Manejo de errores de red

### Anatomía de reqwest::Error

`reqwest::Error` encapsula varios tipos de fallo. Puedes inspeccionarlo para decidir cómo
reaccionar:

```rust
use reqwest::Client;

#[tokio::main]
async fn main() {
    let client = Client::builder()
        .timeout(std::time::Duration::from_secs(5))
        .build()
        .unwrap();

    match client.get("https://httpbin.org/delay/10").send().await {
        Ok(resp) => println!("Status: {}", resp.status()),
        Err(e) => {
            if e.is_timeout() {
                println!("Request timed out");
            } else if e.is_connect() {
                println!("Connection failed (DNS, refused, etc.)");
            } else if e.is_request() {
                println!("Error building request");
            } else if e.is_body() {
                println!("Error reading body");
            } else if e.is_decode() {
                println!("Error deserializing response");
            } else if e.is_redirect() {
                println!("Too many redirects");
            } else {
                println!("Other error: {}", e);
            }

            // La URL que causó el error
            if let Some(url) = e.url() {
                println!("URL: {}", url);
            }

            // El status code (si llegó una respuesta)
            if let Some(status) = e.status() {
                println!("Status: {}", status);
            }
        }
    }
}
```

### Status codes: éxito vs error

Un status HTTP 4xx o 5xx **no** es un error de `reqwest` por defecto — recibes una
`Response` con ese status. Usa `.error_for_status()` para convertirlo en error:

```rust
use reqwest::Client;

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::new();

    // Sin error_for_status: 404 es una Response válida
    let resp = client.get("https://httpbin.org/status/404").send().await?;
    println!("Status: {}", resp.status());       // 404
    println!("Success? {}", resp.status().is_success());  // false

    // Con error_for_status: 404 se convierte en Err
    let result = client
        .get("https://httpbin.org/status/404")
        .send()
        .await?
        .error_for_status();  // Convierte 4xx/5xx en Err

    match result {
        Ok(resp) => println!("Success: {}", resp.status()),
        Err(e) => {
            println!("HTTP error: {}", e);
            println!("Status: {:?}", e.status());  // Some(404)
        }
    }

    Ok(())
}
```

```
┌─── Flujo de errores ──────────────────────────────────────┐
│                                                            │
│  client.get(url).send().await                              │
│       │                                                    │
│       ├─ Err: DNS, conexión, timeout, TLS                  │
│       │       → e.is_connect(), e.is_timeout()             │
│       │                                                    │
│       └─ Ok(Response)                                      │
│            │                                               │
│            ├─ status 2xx → éxito                           │
│            ├─ status 3xx → seguido automáticamente         │
│            ├─ status 4xx → Ok(Response) con status 4xx     │
│            └─ status 5xx → Ok(Response) con status 5xx     │
│                 │                                          │
│                 └─ .error_for_status()                     │
│                      ├─ 2xx → Ok(Response)                 │
│                      └─ 4xx/5xx → Err(reqwest::Error)      │
│                                                            │
└────────────────────────────────────────────────────────────┘
```

### Categorizar status codes

```rust
use reqwest::{Client, StatusCode};

async fn handle_response(client: &Client, url: &str) -> Result<String, String> {
    let resp = client.get(url).send().await
        .map_err(|e| format!("Network error: {}", e))?;

    match resp.status() {
        StatusCode::OK => {
            resp.text().await.map_err(|e| format!("Body error: {}", e))
        }
        StatusCode::NOT_FOUND => {
            Err("Resource not found".to_string())
        }
        StatusCode::UNAUTHORIZED | StatusCode::FORBIDDEN => {
            Err("Authentication required".to_string())
        }
        StatusCode::TOO_MANY_REQUESTS => {
            // Rate limited — podríamos retry
            let retry_after = resp.headers()
                .get("retry-after")
                .and_then(|v| v.to_str().ok())
                .and_then(|v| v.parse::<u64>().ok());
            Err(format!("Rate limited. Retry after: {:?}s", retry_after))
        }
        status if status.is_server_error() => {
            Err(format!("Server error: {}", status))
        }
        status => {
            Err(format!("Unexpected status: {}", status))
        }
    }
}
```

---

## 7. Configuración del Client

### Builder pattern

```rust
use reqwest::Client;
use std::time::Duration;

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::builder()
        // Timeouts
        .timeout(Duration::from_secs(30))           // Timeout total del request
        .connect_timeout(Duration::from_secs(5))    // Solo para conexión TCP

        // Redirecciones
        .redirect(reqwest::redirect::Policy::limited(5))  // Máx 5 redirects
        // .redirect(reqwest::redirect::Policy::none())    // No seguir redirects

        // User-Agent
        .user_agent("my-app/1.0")

        // Connection pool
        .pool_max_idle_per_host(10)      // Max conexiones idle por host
        .pool_idle_timeout(Duration::from_secs(90))

        // TLS
        // .danger_accept_invalid_certs(true)  // Solo para desarrollo/testing
        // .min_tls_version(reqwest::tls::Version::TLS_1_2)

        // Proxy
        // .proxy(reqwest::Proxy::all("http://proxy:8080")?)

        // Compresión (descomprime gzip/brotli automáticamente)
        .gzip(true)
        .brotli(true)

        .build()?;

    let resp = client.get("https://httpbin.org/get").send().await?;
    println!("{}", resp.text().await?);

    Ok(())
}
```

### Timeouts: qué cubre cada uno

```
┌─── Timeouts de reqwest ───────────────────────────────────────┐
│                                                                │
│  connect_timeout                                               │
│  ├──────────┤                                                  │
│  DNS  TCP   TLS                                                │
│  ──── ────  ────  ──────── Headers ──── Body ────              │
│  ├──────────────────────────────────────────────┤              │
│                     timeout (total)                             │
│                                                                │
│  connect_timeout: solo cubre establecer la conexión            │
│  timeout: cubre TODO, desde el inicio hasta el body completo   │
│                                                                │
│  Si connect_timeout < timeout: útil para fallar rápido         │
│  si el host no responde, pero dar más tiempo al body           │
│                                                                │
└────────────────────────────────────────────────────────────────┘
```

### Per-request timeout

```rust
use reqwest::Client;
use std::time::Duration;

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::builder()
        .timeout(Duration::from_secs(30))  // Default: 30s
        .build()?;

    // Este request específico tiene timeout de 5s
    let resp = client
        .get("https://httpbin.org/delay/3")
        .timeout(Duration::from_secs(5))  // Override para este request
        .send()
        .await?;

    println!("{}", resp.status());

    Ok(())
}
```

---

## 8. Respuestas grandes y streaming

### Leer body en chunks

Para respuestas grandes (archivos, streams), no quieres cargar todo en memoria:

```rust
use reqwest::Client;
use tokio::io::AsyncWriteExt;

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let client = Client::new();

    let resp = client
        .get("https://httpbin.org/bytes/102400")  // 100 KB de datos
        .send()
        .await?;

    let total_size = resp.content_length().unwrap_or(0);
    println!("Downloading {} bytes...", total_size);

    // Abrir archivo para escribir
    let mut file = tokio::fs::File::create("/tmp/download.bin").await?;

    // Leer en chunks
    let mut stream = resp.bytes_stream();
    let mut downloaded: u64 = 0;

    use futures::StreamExt;
    while let Some(chunk) = stream.next().await {
        let chunk = chunk?;
        file.write_all(&chunk).await?;
        downloaded += chunk.len() as u64;

        if total_size > 0 {
            let pct = (downloaded as f64 / total_size as f64) * 100.0;
            print!("\r{:.1}% ({}/{})", pct, downloaded, total_size);
        }
    }

    println!("\nDownload complete");

    Ok(())
}
```

```toml
# Necesitas la feature "stream" y el crate futures
[dependencies]
reqwest = { version = "0.12", features = ["json", "stream"] }
futures = "0.3"
```

### bytes() vs text() vs json() vs bytes_stream()

```
┌─── Formas de leer el body ─────────────────────────────────────┐
│                                                                 │
│  .text().await?          → String (todo en memoria)             │
│  .bytes().await?         → Bytes (todo en memoria, binario)     │
│  .json::<T>().await?     → T deserializado (todo en memoria)    │
│  .bytes_stream()         → Stream<Bytes> (chunked, streaming)   │
│                                                                 │
│  .text() / .bytes() / .json():                                  │
│     Simples, cargan todo el body en RAM                         │
│     Bien para respuestas < ~10 MB                               │
│                                                                 │
│  .bytes_stream():                                               │
│     Procesa chunk por chunk                                     │
│     Necesario para descargas grandes                            │
│     Requiere feature "stream" y crate futures                   │
│                                                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## 9. Patrones prácticos

### Retry con backoff exponencial

```rust
use reqwest::Client;
use std::time::Duration;

async fn get_with_retry(
    client: &Client,
    url: &str,
    max_retries: u32,
) -> Result<reqwest::Response, reqwest::Error> {
    let mut last_error = None;

    for attempt in 0..=max_retries {
        if attempt > 0 {
            let delay = Duration::from_millis(100 * 2u64.pow(attempt - 1));
            println!("Retry {} after {:?}...", attempt, delay);
            tokio::time::sleep(delay).await;
        }

        match client.get(url).send().await {
            Ok(resp) if resp.status().is_server_error() => {
                println!("Server error: {}, retrying...", resp.status());
                last_error = Some(resp.error_for_status().unwrap_err());
            }
            Ok(resp) => return Ok(resp),
            Err(e) if e.is_timeout() || e.is_connect() => {
                println!("Transient error: {}, retrying...", e);
                last_error = Some(e);
            }
            Err(e) => return Err(e),  // Error no retriable
        }
    }

    Err(last_error.unwrap())
}

#[tokio::main]
async fn main() -> Result<(), reqwest::Error> {
    let client = Client::builder()
        .timeout(Duration::from_secs(5))
        .build()?;

    let resp = get_with_retry(&client, "https://httpbin.org/get", 3).await?;
    println!("Success: {}", resp.status());

    Ok(())
}
```

### Requests en paralelo

```rust
use reqwest::Client;
use serde::Deserialize;

#[derive(Debug, Deserialize)]
struct IpResponse {
    origin: String,
}

#[tokio::main]
async fn main() -> Result<(), Box<dyn std::error::Error>> {
    let client = Client::new();

    let urls = vec![
        "https://httpbin.org/ip",
        "https://httpbin.org/user-agent",
        "https://httpbin.org/headers",
    ];

    // Lanzar todos los requests en paralelo
    let mut handles = Vec::new();

    for url in &urls {
        let client = client.clone();  // Client es Clone (Arc interno)
        let url = url.to_string();

        handles.push(tokio::spawn(async move {
            let resp = client.get(&url).send().await?;
            let body = resp.text().await?;
            Ok::<_, reqwest::Error>((url, body))
        }));
    }

    // Esperar todos los resultados
    for handle in handles {
        match handle.await? {
            Ok((url, body)) => {
                println!("=== {} ===", url);
                println!("{}\n", &body[..body.len().min(100)]);
            }
            Err(e) => eprintln!("Error: {}", e),
        }
    }

    Ok(())
}
```

### API client estructurado

```rust
use reqwest::{Client, StatusCode};
use serde::{Serialize, Deserialize};
use std::time::Duration;

#[derive(Debug)]
pub enum ApiError {
    Network(reqwest::Error),
    NotFound,
    Unauthorized,
    RateLimited { retry_after_secs: Option<u64> },
    Server(StatusCode, String),
}

impl std::fmt::Display for ApiError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ApiError::Network(e) => write!(f, "network error: {}", e),
            ApiError::NotFound => write!(f, "not found"),
            ApiError::Unauthorized => write!(f, "unauthorized"),
            ApiError::RateLimited { retry_after_secs } => {
                write!(f, "rate limited (retry after: {:?}s)", retry_after_secs)
            }
            ApiError::Server(status, body) => {
                write!(f, "server error {}: {}", status, body)
            }
        }
    }
}

impl From<reqwest::Error> for ApiError {
    fn from(e: reqwest::Error) -> Self {
        ApiError::Network(e)
    }
}

pub struct ApiClient {
    client: Client,
    base_url: String,
    token: String,
}

#[derive(Debug, Deserialize)]
struct User {
    id: u64,
    name: String,
    email: String,
}

#[derive(Serialize)]
struct CreateUserRequest {
    name: String,
    email: String,
}

impl ApiClient {
    pub fn new(base_url: &str, token: &str) -> Result<Self, reqwest::Error> {
        let client = Client::builder()
            .timeout(Duration::from_secs(30))
            .connect_timeout(Duration::from_secs(5))
            .user_agent("my-api-client/1.0")
            .build()?;

        Ok(Self {
            client,
            base_url: base_url.trim_end_matches('/').to_string(),
            token: token.to_string(),
        })
    }

    async fn handle_response<T: serde::de::DeserializeOwned>(
        &self,
        resp: reqwest::Response,
    ) -> Result<T, ApiError> {
        match resp.status() {
            StatusCode::OK | StatusCode::CREATED => {
                Ok(resp.json().await?)
            }
            StatusCode::NOT_FOUND => Err(ApiError::NotFound),
            StatusCode::UNAUTHORIZED | StatusCode::FORBIDDEN => {
                Err(ApiError::Unauthorized)
            }
            StatusCode::TOO_MANY_REQUESTS => {
                let retry = resp.headers()
                    .get("retry-after")
                    .and_then(|v| v.to_str().ok())
                    .and_then(|v| v.parse().ok());
                Err(ApiError::RateLimited { retry_after_secs: retry })
            }
            status => {
                let body = resp.text().await.unwrap_or_default();
                Err(ApiError::Server(status, body))
            }
        }
    }

    pub async fn get_user(&self, id: u64) -> Result<User, ApiError> {
        let resp = self.client
            .get(format!("{}/users/{}", self.base_url, id))
            .bearer_auth(&self.token)
            .send()
            .await?;

        self.handle_response(resp).await
    }

    pub async fn create_user(
        &self,
        name: &str,
        email: &str,
    ) -> Result<User, ApiError> {
        let resp = self.client
            .post(format!("{}/users", self.base_url))
            .bearer_auth(&self.token)
            .json(&CreateUserRequest {
                name: name.to_string(),
                email: email.to_string(),
            })
            .send()
            .await?;

        self.handle_response(resp).await
    }
}
```

Este patrón encapsula la configuración del client, autenticación, manejo de errores HTTP,
y deserialización en un módulo limpio y reutilizable.

### Paginación

```rust
use reqwest::Client;
use serde::Deserialize;

#[derive(Debug, Deserialize)]
struct Page<T> {
    data: Vec<T>,
    total: usize,
    page: usize,
    per_page: usize,
}

#[derive(Debug, Deserialize)]
struct Item {
    id: u64,
    name: String,
}

async fn fetch_all_items(
    client: &Client,
    base_url: &str,
) -> Result<Vec<Item>, reqwest::Error> {
    let mut all_items = Vec::new();
    let mut page = 1;
    let per_page = 50;

    loop {
        let resp: Page<Item> = client
            .get(format!("{}/items", base_url))
            .query(&[("page", page), ("per_page", per_page)])
            .send()
            .await?
            .json()
            .await?;

        let fetched = resp.data.len();
        all_items.extend(resp.data);

        println!("Page {}: {} items (total: {})", page, fetched, resp.total);

        // ¿Hay más páginas?
        if all_items.len() >= resp.total || fetched == 0 {
            break;
        }

        page += 1;
    }

    Ok(all_items)
}
```

---

## 10. Errores comunes

### Error 1: crear un Client por request

```rust
// MAL: nuevo Client (y nueva conexión TCP) por cada request
async fn fetch_data(url: &str) -> Result<String, reqwest::Error> {
    let resp = reqwest::get(url).await?;  // Crea Client temporal
    resp.text().await
}

async fn process_many(urls: Vec<String>) {
    for url in &urls {
        let data = fetch_data(url).await.unwrap();
        // Cada iteración: DNS + TCP connect + TLS handshake
    }
}

// BIEN: un Client compartido con connection pool
async fn process_many_fast(urls: Vec<String>) {
    let client = reqwest::Client::new();

    for url in &urls {
        let resp = client.get(url).send().await.unwrap();
        let data = resp.text().await.unwrap();
        // Reutiliza conexiones existentes (keep-alive)
    }
}
```

**Por qué falla**: cada `Client` nuevo abre conexiones TCP frescas. Con un `Client`
reutilizado, las conexiones se mantienen vivas y se reutilizan, ahorrando DNS + TCP + TLS
por request.

### Error 2: no verificar status antes de deserializar

```rust
use serde::Deserialize;

#[derive(Deserialize)]
struct User {
    name: String,
    email: String,
}

// MAL: si el servidor retorna 500, .json() falla con error críptico
async fn get_user_bad(client: &reqwest::Client) -> Result<User, reqwest::Error> {
    client
        .get("https://api.example.com/user/1")
        .send()
        .await?
        .json()  // Si status=500, body es HTML de error → deserialización falla
        .await
}

// BIEN: verificar status primero
async fn get_user_good(
    client: &reqwest::Client,
) -> Result<User, Box<dyn std::error::Error>> {
    let resp = client
        .get("https://api.example.com/user/1")
        .send()
        .await?;

    if !resp.status().is_success() {
        let status = resp.status();
        let body = resp.text().await.unwrap_or_default();
        return Err(format!("HTTP {}: {}", status, body).into());
    }

    Ok(resp.json().await?)
}
```

**Por qué falla**: un status 500 normalmente retorna HTML o texto de error, no JSON.
Intentar deserializar ese body como JSON produce un error de serde confuso como
"expected value at line 1 column 1".

### Error 3: usar reqwest blocking en un runtime async

```rust
// MAL: feature "blocking" dentro de un tokio::spawn
#[tokio::main]
async fn main() {
    tokio::spawn(async {
        // Esto BLOQUEA el worker thread de Tokio
        let resp = reqwest::blocking::get("https://httpbin.org/get").unwrap();
        // panic: "Cannot start a runtime from within a runtime"
    });
}

// BIEN: usar la API async
#[tokio::main]
async fn main() {
    tokio::spawn(async {
        let resp = reqwest::get("https://httpbin.org/get").await.unwrap();
        println!("{}", resp.text().await.unwrap());
    });
}
```

**Por qué falla**: `reqwest::blocking` crea internamente su propio runtime de Tokio. No
puedes crear un runtime dentro de otro — Tokio lo detecta y hace panic.

### Error 4: ignorar timeouts

```rust
// MAL: sin timeout, un servidor lento puede bloquear tu app indefinidamente
async fn fetch_no_timeout() -> Result<String, reqwest::Error> {
    reqwest::get("https://slow-server.example.com/data")
        .await?
        .text()
        .await
}

// BIEN: siempre configurar timeouts
async fn fetch_with_timeout() -> Result<String, reqwest::Error> {
    let client = reqwest::Client::builder()
        .timeout(std::time::Duration::from_secs(10))
        .connect_timeout(std::time::Duration::from_secs(3))
        .build()?;

    client
        .get("https://slow-server.example.com/data")
        .send()
        .await?
        .text()
        .await
}
```

**Por qué falla**: sin timeout, un servidor que acepta la conexión pero nunca responde
mantiene tu task suspendida para siempre. En producción, siempre define al menos un
`timeout()` en el `Client`.

### Error 5: leaks de Response sin leer el body

```rust
// MAL: leer solo el status y descartar la Response
async fn check_status(client: &reqwest::Client, url: &str) -> bool {
    match client.get(url).send().await {
        Ok(resp) => resp.status().is_success(),
        // resp se dropea aquí SIN leer el body
        // La conexión TCP no puede reutilizarse (body pendiente)
        Err(_) => false,
    }
}

// BIEN: si solo necesitas headers, usa HEAD
async fn check_status_good(client: &reqwest::Client, url: &str) -> bool {
    match client.head(url).send().await {
        Ok(resp) => resp.status().is_success(),
        // HEAD no tiene body, la conexión se reutiliza correctamente
        Err(_) => false,
    }
}

// ALTERNATIVA: si necesitas GET, consumir o descartar el body explícitamente
async fn check_status_alt(client: &reqwest::Client, url: &str) -> bool {
    match client.get(url).send().await {
        Ok(resp) => {
            let success = resp.status().is_success();
            let _ = resp.bytes().await;  // Consumir body para liberar conexión
            success
        }
        Err(_) => false,
    }
}
```

**Por qué falla**: si descartas una `Response` sin leer su body, la conexión TCP subyacente
no puede volver al pool (el body aún está pendiente). Con muchos requests así, el pool se
agota y cada request requiere una conexión nueva.

---

## 11. Cheatsheet

```text
╔══════════════════════════════════════════════════════════════════════════╗
║                         HTTP CLIENT (REQWEST)                          ║
╠══════════════════════════════════════════════════════════════════════════╣
║                                                                        ║
║  SETUP                                                                 ║
║  ─────                                                                 ║
║  reqwest = { version = "0.12", features = ["json"] }                   ║
║  let client = Client::new();            // reutilizar siempre          ║
║  let client = Client::builder()         // configurar timeouts etc.    ║
║      .timeout(Duration::from_secs(30))                                 ║
║      .build()?;                                                        ║
║                                                                        ║
║  REQUESTS                                                              ║
║  ────────                                                              ║
║  client.get(url).send().await?          // GET                         ║
║  client.post(url).json(&body).send()    // POST JSON                   ║
║  client.put(url).json(&body).send()     // PUT JSON                    ║
║  client.delete(url).send()              // DELETE                      ║
║  .query(&[("k","v")])                   // ?k=v en URL                 ║
║  .form(&[("k","v")])                    // form-urlencoded body        ║
║  .body("raw")                           // body crudo                  ║
║  .header("X-Key", "val")               // header custom               ║
║  .bearer_auth(token)                    // Authorization: Bearer ...   ║
║  .basic_auth(user, Some(pass))          // Authorization: Basic ...    ║
║  .timeout(dur)                          // override per-request        ║
║                                                                        ║
║  RESPONSES                                                             ║
║  ──────────                                                            ║
║  resp.status()                          // StatusCode                  ║
║  resp.status().is_success()             // 2xx?                        ║
║  resp.headers()                         // &HeaderMap                  ║
║  resp.content_length()                  // Option<u64>                 ║
║  resp.text().await?                     // body → String               ║
║  resp.bytes().await?                    // body → Bytes                ║
║  resp.json::<T>().await?                // body → T (deserialize)      ║
║  resp.bytes_stream()                    // body → Stream<Bytes>        ║
║  resp.error_for_status()?               // 4xx/5xx → Err              ║
║                                                                        ║
║  ERRORES                                                               ║
║  ───────                                                               ║
║  e.is_timeout()                         // timeout expirado            ║
║  e.is_connect()                         // conexión fallida            ║
║  e.is_decode()                          // deserialización fallida     ║
║  e.is_redirect()                        // demasiados redirects        ║
║  e.url()                                // URL que falló               ║
║  e.status()                             // status code (si hubo)       ║
║                                                                        ║
║  CLIENT CONFIG                                                         ║
║  ─────────────                                                         ║
║  .timeout(dur)                          // timeout total               ║
║  .connect_timeout(dur)                  // solo conexión TCP           ║
║  .user_agent("name/ver")               // User-Agent default          ║
║  .default_headers(map)                  // headers en todos los req    ║
║  .redirect(Policy::limited(N))          // máx N redirects             ║
║  .gzip(true) / .brotli(true)           // descompresión automática    ║
║  .pool_max_idle_per_host(N)             // connection pool             ║
║  .danger_accept_invalid_certs(true)     // SOLO para dev/testing       ║
║                                                                        ║
║  PATRONES                                                              ║
║  ────────                                                              ║
║  Retry: loop con backoff exponencial, solo en errores transitorios     ║
║  Paralelo: client.clone() + tokio::spawn por request                   ║
║  Streaming: .bytes_stream() + StreamExt::next() chunk por chunk        ║
║  API client: struct con Client + base_url + handle_response()          ║
║  Paginación: loop incrementando page hasta total alcanzado             ║
║                                                                        ║
╚══════════════════════════════════════════════════════════════════════════╝
```

---

## 12. Ejercicios

### Ejercicio 1: consulta de IP pública y geolocalización

Construye un programa que:
1. Consulte tu IP pública usando `https://httpbin.org/ip` (GET, JSON response).
2. Define un struct `IpResponse` con `#[derive(Deserialize)]` y usa `.json()`.
3. Muestra la IP obtenida.
4. Maneja errores: si el request falla por timeout (configura un timeout de 5 segundos),
   muestra un mensaje amigable. Si el status no es 200, muestra el código.

Usa un `Client` reutilizable (no `reqwest::get()`).

**Pista**: `Client::builder().timeout(Duration::from_secs(5)).build()?` para el timeout.
Verifica `resp.status().is_success()` antes de deserializar.

> **Pregunta de reflexión**: ¿Qué diferencia hay entre `reqwest::get(url)` y
> `client.get(url).send()`? ¿En qué escenario la diferencia se vuelve significativa
> en rendimiento?

---

### Ejercicio 2: POST con JSON y manejo de status codes

Implementa un programa que envíe datos a `https://httpbin.org/post`:
1. Define un struct `Payload` con campos `name: String`, `items: Vec<String>`,
   `priority: u32`.
2. Serializa y envía con `.json(&payload)`.
3. Deserializa la respuesta en un struct que capture el campo `json` de httpbin
   (la respuesta de httpbin incluye un campo `json` con lo que enviaste).
4. Verifica que los datos recibidos coincidan con los enviados.
5. Prueba también enviando a `https://httpbin.org/status/500` y maneja el error
   con un match sobre `StatusCode`.

**Pista**: la respuesta de httpbin tiene la forma `{ "json": { ... }, "headers": { ... }, ... }`.
Puedes deserializar solo lo que necesitas con `#[serde(rename)]` o usar `serde_json::Value`.

> **Pregunta de reflexión**: si cambias `.json(&payload)` por `.body(serde_json::to_string(&payload)?)`
> sin agregar el header `Content-Type: application/json`, ¿qué diferencia verías en la
> respuesta del servidor? ¿Qué hace `.json()` además de serializar?

---

### Ejercicio 3: descarga con progreso y retry

Crea un programa que descargue un archivo (usa `https://httpbin.org/bytes/1048576` para
1 MB de datos aleatorios):
1. Usa `.bytes_stream()` para leer chunk por chunk.
2. Muestra progreso porcentual usando `content_length()`.
3. Guarda los bytes en un archivo con `tokio::fs::File`.
4. Implementa retry: si la descarga falla (timeout, error de conexión), reintenta hasta
   3 veces con backoff exponencial (100ms, 200ms, 400ms).
5. Reporta: bytes descargados, tiempo total, y velocidad promedio (KB/s).

**Pista**: necesitas `features = ["json", "stream"]` en reqwest y `futures = "0.3"` para
`StreamExt::next()`. Usa `std::time::Instant` para medir el tiempo.

> **Pregunta de reflexión**: ¿Por qué es importante usar `.bytes_stream()` en vez de
> `.bytes().await?` para descargas grandes? ¿Qué pasaría con la memoria de tu programa
> si descargaras un archivo de 2 GB con `.bytes()`?
