# net/http cliente — http.Get, http.Client, timeouts, headers, cookies

## 1. Introduccion

El paquete `net/http` de Go incluye un cliente HTTP completo y de produccion en la stdlib — sin dependencias externas. Soporta HTTP/1.1 y HTTP/2, connection pooling automatico, redirects, cookies, TLS, proxies, y timeouts granulares. A diferencia de C (que necesita libcurl) o Rust (que necesita reqwest/hyper), Go tiene todo integrado.

```
┌───────────────────────────────────────────────────────────────────────────────────┐
│                    net/http CLIENTE — ARQUITECTURA                               │
├───────────────────────────────────────────────────────────────────────────────────┤
│                                                                                   │
│  NIVELES DE API                                                                  │
│                                                                                   │
│  NIVEL 1: FUNCIONES GLOBALES (conveniencia — para scripts y prototipos)         │
│  ├─ http.Get(url)             → GET simple                                      │
│  ├─ http.Post(url, ct, body)  → POST con content-type y body                   │
│  ├─ http.PostForm(url, data)  → POST application/x-www-form-urlencoded          │
│  └─ http.Head(url)            → HEAD (solo headers)                             │
│  NOTA: Usan http.DefaultClient (sin timeouts!! peligroso en produccion)         │
│                                                                                   │
│  NIVEL 2: http.Client (recomendado para produccion)                             │
│  ├─ client := &http.Client{Timeout: 30*time.Second}                            │
│  ├─ client.Get(url)                                                              │
│  ├─ client.Do(req)            → control total con *http.Request                 │
│  ├─ client.Transport          → connection pooling, TLS, proxy                  │
│  ├─ client.CheckRedirect      → control de redirects                            │
│  └─ client.Jar                → cookie jar                                       │
│                                                                                   │
│  NIVEL 3: http.Request + http.Client.Do (maximo control)                        │
│  ├─ req, _ := http.NewRequestWithContext(ctx, method, url, body)                │
│  ├─ req.Header.Set("Authorization", "Bearer token")                             │
│  ├─ req.Header.Set("Content-Type", "application/json")                          │
│  └─ resp, err := client.Do(req)                                                 │
│                                                                                   │
│  FLUJO DE UNA REQUEST                                                            │
│  ┌────────┐   ┌────────────┐   ┌───────────┐   ┌──────────┐   ┌─────────┐     │
│  │ Request│──→│ Transport  │──→│ TCP/TLS   │──→│ Server   │──→│Response │     │
│  │ objeto │   │ pool,proxy │   │ conn pool │   │ remoto   │   │ objeto  │     │
│  └────────┘   └────────────┘   └───────────┘   └──────────┘   └─────────┘     │
│                     │                                                │            │
│                     │    ┌─────────────────────────┐                │            │
│                     └───→│ Connection Pool          │◄───────────────┘            │
│                          │ Mantiene conexiones TCP  │                             │
│                          │ abiertas para reusar     │                             │
│                          │ (HTTP keep-alive)        │                             │
│                          └─────────────────────────┘                             │
│                                                                                   │
│  COMPARACION                                                                     │
│  ├─ C:    libcurl (externo) o sockets raw + parseo manual de HTTP               │
│  ├─ Rust: reqwest (alto nivel, externo) o hyper (bajo nivel, externo)           │
│  └─ Go:   net/http (stdlib, completo, produccion-ready)                         │
└───────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Funciones globales — nivel basico

### 2.1 http.Get

```go
package main

import (
    "fmt"
    "io"
    "net/http"
    "os"
)

func main() {
    // http.Get es la forma mas simple de hacer un GET request
    resp, err := http.Get("https://httpbin.org/get")
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
    defer resp.Body.Close() // CRITICO: siempre cerrar el body
    
    // Verificar status code
    fmt.Printf("Status: %s (%d)\n", resp.Status, resp.StatusCode)
    fmt.Printf("Content-Type: %s\n", resp.Header.Get("Content-Type"))
    fmt.Printf("Content-Length: %d\n", resp.ContentLength)
    
    // Leer el body
    body, err := io.ReadAll(resp.Body)
    if err != nil {
        fmt.Fprintf(os.Stderr, "read body: %v\n", err)
        os.Exit(1)
    }
    
    fmt.Println(string(body))
}
```

### 2.2 http.Post y http.PostForm

```go
// POST con body JSON
import "strings"

func postJSON() {
    jsonBody := `{"name": "Alice", "age": 30}`
    resp, err := http.Post(
        "https://httpbin.org/post",
        "application/json",           // Content-Type
        strings.NewReader(jsonBody),  // body (io.Reader)
    )
    if err != nil {
        log.Fatal(err)
    }
    defer resp.Body.Close()
    
    io.Copy(os.Stdout, resp.Body)
}

// POST con form data (application/x-www-form-urlencoded)
func postForm() {
    resp, err := http.PostForm("https://httpbin.org/post", url.Values{
        "username": {"alice"},
        "password": {"secret123"},
        "remember": {"true"},
    })
    if err != nil {
        log.Fatal(err)
    }
    defer resp.Body.Close()
    
    io.Copy(os.Stdout, resp.Body)
}
```

### 2.3 Por que NO usar las funciones globales en produccion

```go
// http.Get(), http.Post(), etc. usan http.DefaultClient:
//
// var DefaultClient = &Client{}
//
// DefaultClient NO tiene timeout. Una request puede bloquearse FOREVER
// si el servidor no responde o responde muy lentamente.

// MAL — en produccion:
resp, err := http.Get("https://api.example.com/data")

// BIEN — crear tu propio cliente con timeouts:
client := &http.Client{
    Timeout: 30 * time.Second,
}
resp, err := client.Get("https://api.example.com/data")

// Las funciones globales solo se justifican para:
// - Scripts de una sola ejecucion
// - Prototipos rapidos
// - Ejemplos educativos
```

---

## 3. http.Client — el cliente de produccion

### 3.1 Crear un cliente con timeout

```go
// http.Client es un struct con varios campos configurables

client := &http.Client{
    Timeout: 30 * time.Second, // Timeout TOTAL de la request
    // Incluye: DNS lookup + TCP connect + TLS handshake +
    //          enviar request + esperar response + leer body
    // Si el timeout se excede en CUALQUIER punto, retorna error
}

resp, err := client.Get("https://api.example.com/data")
if err != nil {
    // Puede ser timeout
    if errors.Is(err, context.DeadlineExceeded) {
        fmt.Println("request timed out")
    }
    return
}
defer resp.Body.Close()
```

### 3.2 Anatomia de http.Client

```go
type Client struct {
    // Transport controla la capa de transporte: connection pool,
    // TLS, proxy, HTTP/2. Si nil, usa DefaultTransport.
    Transport RoundTripper
    
    // CheckRedirect controla como manejar redirects (3xx).
    // Si nil, sigue hasta 10 redirects y luego falla.
    CheckRedirect func(req *Request, via []*Request) error
    
    // Jar es el cookie jar para almacenar cookies entre requests.
    // Si nil, no se envian ni almacenan cookies automaticamente.
    Jar CookieJar
    
    // Timeout es el timeout TOTAL para la request completa.
    // Incluye connection, TLS, request, response headers, y body.
    // Zero = sin timeout (peligroso).
    Timeout time.Duration
}
```

### 3.3 Patron: cliente singleton reutilizable

```go
// http.Client es thread-safe y DEBE reutilizarse.
// Crear un cliente por request es un antipatron:
// - Desperdicia connection pool
// - Cada request hace nuevo TCP handshake + TLS handshake
// - Desperdicia recursos del SO (file descriptors, sockets)

// MAL:
func fetchData(url string) {
    client := &http.Client{Timeout: 10 * time.Second} // Nuevo cada vez
    resp, _ := client.Get(url)
    // ...
}

// BIEN: cliente compartido (package-level o dependency injection)
var httpClient = &http.Client{
    Timeout: 30 * time.Second,
}

func fetchData(url string) {
    resp, _ := httpClient.Get(url)
    // El connection pool se reutiliza entre llamadas
}

// MEJOR: inyectar como dependencia para testing
type APIClient struct {
    httpClient *http.Client
    baseURL    string
}

func NewAPIClient(baseURL string) *APIClient {
    return &APIClient{
        httpClient: &http.Client{
            Timeout: 30 * time.Second,
        },
        baseURL: baseURL,
    }
}
```

---

## 4. http.Request — control total

`http.NewRequestWithContext` te da control total sobre la request: metodo, URL, body, headers, contexto.

### 4.1 Crear requests manualmente

```go
// GET con headers custom
func getWithHeaders(url string) (*http.Response, error) {
    req, err := http.NewRequest("GET", url, nil)
    if err != nil {
        return nil, err
    }
    
    req.Header.Set("Authorization", "Bearer my-token-123")
    req.Header.Set("Accept", "application/json")
    req.Header.Set("User-Agent", "MyApp/1.0")
    
    client := &http.Client{Timeout: 10 * time.Second}
    return client.Do(req)
}

// POST JSON
func postJSON(url string, data interface{}) (*http.Response, error) {
    body, err := json.Marshal(data)
    if err != nil {
        return nil, fmt.Errorf("marshal: %w", err)
    }
    
    req, err := http.NewRequest("POST", url, bytes.NewReader(body))
    if err != nil {
        return nil, err
    }
    
    req.Header.Set("Content-Type", "application/json")
    req.Header.Set("Accept", "application/json")
    
    client := &http.Client{Timeout: 30 * time.Second}
    return client.Do(req)
}

// PUT con body desde io.Reader
func putFile(url string, file io.Reader) (*http.Response, error) {
    req, err := http.NewRequest("PUT", url, file)
    if err != nil {
        return nil, err
    }
    
    req.Header.Set("Content-Type", "application/octet-stream")
    
    client := &http.Client{Timeout: 5 * time.Minute}
    return client.Do(req)
}

// DELETE
func deleteResource(url string) (*http.Response, error) {
    req, err := http.NewRequest("DELETE", url, nil)
    if err != nil {
        return nil, err
    }
    
    req.Header.Set("Authorization", "Bearer my-token")
    
    client := &http.Client{Timeout: 10 * time.Second}
    return client.Do(req)
}

// PATCH con JSON parcial
func patchResource(url string, patch map[string]interface{}) (*http.Response, error) {
    body, _ := json.Marshal(patch)
    
    req, err := http.NewRequest("PATCH", url, bytes.NewReader(body))
    if err != nil {
        return nil, err
    }
    
    req.Header.Set("Content-Type", "application/json")
    
    client := &http.Client{Timeout: 10 * time.Second}
    return client.Do(req)
}
```

### 4.2 NewRequestWithContext — cancelacion y timeout per-request

```go
// SIEMPRE usar NewRequestWithContext en produccion.
// Permite cancelar requests individuales y setear timeouts per-request
// independientes del timeout del cliente.

func fetchWithContext(ctx context.Context, url string) ([]byte, error) {
    req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
    if err != nil {
        return nil, err
    }
    
    client := &http.Client{Timeout: 30 * time.Second}
    resp, err := client.Do(req)
    if err != nil {
        return nil, err
    }
    defer resp.Body.Close()
    
    return io.ReadAll(resp.Body)
}

// Uso 1: con timeout
func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()
    
    data, err := fetchWithContext(ctx, "https://api.example.com/slow-endpoint")
    // Si tarda mas de 5s → error: context deadline exceeded
}

// Uso 2: con cancelacion manual
func main() {
    ctx, cancel := context.WithCancel(context.Background())
    
    go func() {
        time.Sleep(2 * time.Second)
        cancel() // Cancelar la request despues de 2s
    }()
    
    data, err := fetchWithContext(ctx, "https://api.example.com/data")
}

// Uso 3: en un handler HTTP (request context del servidor)
func handler(w http.ResponseWriter, r *http.Request) {
    // r.Context() se cancela cuando el cliente cierra la conexion
    data, err := fetchWithContext(r.Context(), "https://api.example.com/data")
    if err != nil {
        http.Error(w, err.Error(), http.StatusBadGateway)
        return
    }
    w.Write(data)
}
```

### 4.3 Timeout del contexto vs timeout del cliente

```go
// Client.Timeout y context timeout interactuan:
// Se usa EL MAS CORTO de los dos.

client := &http.Client{Timeout: 30 * time.Second}

// Caso 1: context timeout < client timeout → context gana
ctx, _ := context.WithTimeout(context.Background(), 5*time.Second)
req, _ := http.NewRequestWithContext(ctx, "GET", url, nil)
client.Do(req) // Timeout efectivo: 5s

// Caso 2: client timeout < context timeout → client gana
ctx, _ := context.WithTimeout(context.Background(), 60*time.Second)
req, _ := http.NewRequestWithContext(ctx, "GET", url, nil)
client.Do(req) // Timeout efectivo: 30s

// Caso 3: sin context timeout → client timeout aplica
req, _ := http.NewRequest("GET", url, nil)
client.Do(req) // Timeout efectivo: 30s

// PATRON: client.Timeout como default "seguro", context para override per-request
```

---

## 5. http.Response — procesar la respuesta

### 5.1 Anatomia del Response

```go
type Response struct {
    Status     string // "200 OK"
    StatusCode int    // 200
    
    // Headers de la respuesta
    Header http.Header // map[string][]string
    
    // Body — io.ReadCloser (DEBE cerrarse)
    Body io.ReadCloser
    
    // ContentLength — longitud del body, o -1 si desconocida
    ContentLength int64
    
    // Proto — "HTTP/1.1" o "HTTP/2.0"
    Proto      string
    ProtoMajor int
    ProtoMinor int
    
    // Trailer — headers que llegan despues del body (raro)
    Trailer http.Header
    
    // Request — la request que genero esta response
    Request *http.Request
    
    // TLS — informacion de TLS (nil si no es HTTPS)
    TLS *tls.ConnectionState
    
    // Uncompressed — true si el body fue descomprimido automaticamente
    Uncompressed bool
}
```

### 5.2 Regla de oro: SIEMPRE cerrar resp.Body

```go
// REGLA: si err == nil, resp.Body SIEMPRE existe y DEBE cerrarse.
// Si no cierras el body, la conexion TCP no se devuelve al pool
// y eventualmente agotas los file descriptors del SO.

// Patron correcto:
resp, err := client.Get(url)
if err != nil {
    return err // No hay body que cerrar — resp puede ser nil
}
defer resp.Body.Close() // INMEDIATAMENTE despues del check de error

// INCLUSO si no te interesa el body:
resp, err := client.Head(url)
if err != nil {
    return err
}
defer resp.Body.Close() // Aun HEAD tiene un body vacio que cerrar

// INCLUSO con status codes de error:
resp, err := client.Get(url)
if err != nil {
    return err
}
defer resp.Body.Close() // Cerrar primero
if resp.StatusCode != 200 {
    // El body puede contener un mensaje de error util
    body, _ := io.ReadAll(resp.Body)
    return fmt.Errorf("HTTP %d: %s", resp.StatusCode, body)
}
```

### 5.3 Draining the body

```go
// A veces no quieres leer el body completo pero SI necesitas
// que la conexion se devuelva al pool.

// io.ReadAll lee todo y luego Close libera la conexion.
// Pero si el body es MUY grande y no lo necesitas:

resp, err := client.Get(url)
if err != nil {
    return err
}
defer resp.Body.Close()

if resp.StatusCode != 200 {
    // Descartar el body sin leerlo todo
    // Limitar a 1MB por si acaso (evitar OOM en body gigante)
    io.Copy(io.Discard, io.LimitReader(resp.Body, 1<<20))
    return fmt.Errorf("HTTP %d", resp.StatusCode)
}

// NOTA: si haces resp.Body.Close() sin drenar el body,
// la conexion NO se reutiliza (se cierra).
// El Transport detecta que quedan bytes sin leer y descarta la conexion.
// Para reutilizar la conexion, debes drenar o leer todo el body.
```

### 5.4 Leer el body de diferentes formas

```go
resp, err := client.Get(url)
if err != nil {
    return err
}
defer resp.Body.Close()

// Forma 1: Leer todo a []byte
body, err := io.ReadAll(resp.Body)

// Forma 2: Leer todo a string
body, _ := io.ReadAll(resp.Body)
text := string(body)

// Forma 3: Decodificar JSON directamente (mejor — no crea []byte intermedio)
var result struct {
    Name  string `json:"name"`
    Email string `json:"email"`
}
err = json.NewDecoder(resp.Body).Decode(&result)

// Forma 4: Copiar a un archivo
file, _ := os.Create("download.dat")
defer file.Close()
written, err := io.Copy(file, resp.Body)

// Forma 5: Copiar a stdout
io.Copy(os.Stdout, resp.Body)

// Forma 6: Leer con limite (prevenir OOM en bodies grandes)
limitedBody := io.LimitReader(resp.Body, 10*1024*1024) // Max 10 MB
body, err := io.ReadAll(limitedBody)

// Forma 7: Streaming — procesar linea por linea
scanner := bufio.NewScanner(resp.Body)
for scanner.Scan() {
    processLine(scanner.Text())
}
```

### 5.5 Verificar status codes

```go
resp, err := client.Get(url)
if err != nil {
    return err
}
defer resp.Body.Close()

// Metodo 1: Switch por rangos
switch {
case resp.StatusCode >= 200 && resp.StatusCode < 300:
    // 2xx — exito
    return processSuccess(resp)
    
case resp.StatusCode >= 300 && resp.StatusCode < 400:
    // 3xx — redirect (normalmente manejado por el cliente automaticamente)
    return fmt.Errorf("unexpected redirect: %s", resp.Header.Get("Location"))
    
case resp.StatusCode >= 400 && resp.StatusCode < 500:
    // 4xx — error del cliente
    body, _ := io.ReadAll(resp.Body)
    return fmt.Errorf("client error %d: %s", resp.StatusCode, body)
    
case resp.StatusCode >= 500:
    // 5xx — error del servidor (puede reintentar)
    return fmt.Errorf("server error %d: %s", resp.StatusCode, resp.Status)
}

// Metodo 2: Solo verificar exito
if resp.StatusCode != http.StatusOK {
    body, _ := io.ReadAll(resp.Body)
    return fmt.Errorf("HTTP %d: %s", resp.StatusCode, body)
}

// Metodo 3: Helper function
func checkResponse(resp *http.Response) error {
    if resp.StatusCode >= 200 && resp.StatusCode < 300 {
        return nil
    }
    body, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
    return fmt.Errorf("HTTP %d %s: %s", resp.StatusCode, resp.Status, body)
}

// Constantes utiles (hay muchas mas):
http.StatusOK                  // 200
http.StatusCreated             // 201
http.StatusNoContent           // 204
http.StatusMovedPermanently    // 301
http.StatusFound               // 302 (redirect temporal)
http.StatusNotModified         // 304
http.StatusBadRequest          // 400
http.StatusUnauthorized        // 401
http.StatusForbidden           // 403
http.StatusNotFound            // 404
http.StatusMethodNotAllowed    // 405
http.StatusConflict            // 409
http.StatusTooManyRequests     // 429
http.StatusInternalServerError // 500
http.StatusBadGateway          // 502
http.StatusServiceUnavailable  // 503
http.StatusGatewayTimeout      // 504
```

---

## 6. Headers

### 6.1 http.Header (request y response)

```go
// http.Header es un map[string][]string con metodos helper.
// Las claves se canonicalizan automaticamente: "content-type" → "Content-Type"

// --- Request headers ---

req, _ := http.NewRequest("GET", url, nil)

// Set — reemplaza el valor (si ya existia, lo borra)
req.Header.Set("Authorization", "Bearer token123")
req.Header.Set("Accept", "application/json")

// Add — agrega un valor (permite multiples valores para la misma clave)
req.Header.Add("Accept-Encoding", "gzip")
req.Header.Add("Accept-Encoding", "deflate")
// Resultado: Accept-Encoding: gzip, deflate

// Get — obtener el primer valor
ct := req.Header.Get("Content-Type")

// Values — obtener todos los valores
encodings := req.Header.Values("Accept-Encoding")
// ["gzip", "deflate"]

// Del — borrar un header
req.Header.Del("Accept-Encoding")

// Verificar existencia (Get retorna "" si no existe)
if req.Header.Get("X-Custom") == "" {
    fmt.Println("header no presente")
}

// Iterar sobre todos los headers
for key, values := range req.Header {
    for _, value := range values {
        fmt.Printf("%s: %s\n", key, value)
    }
}

// --- Response headers ---

resp, _ := client.Do(req)
contentType := resp.Header.Get("Content-Type")
cacheControl := resp.Header.Get("Cache-Control")
etag := resp.Header.Get("ETag")
```

### 6.2 Headers comunes

```
┌──────────────────────────────┬─────────────────────────────────────────────────┐
│ Header                       │ Descripcion y ejemplo                           │
├──────────────────────────────┼─────────────────────────────────────────────────┤
│ REQUEST HEADERS              │                                                 │
├──────────────────────────────┼─────────────────────────────────────────────────┤
│ Authorization                │ "Bearer <token>" o "Basic <base64>"            │
│ Content-Type                 │ "application/json", "multipart/form-data"      │
│ Accept                       │ "application/json", "text/html"                │
│ Accept-Encoding              │ "gzip, deflate, br" (Go descomprime gzip auto) │
│ User-Agent                   │ "MyApp/1.0" (Go: "Go-http-client/1.1")        │
│ If-None-Match                │ ETag para cache condicional                     │
│ If-Modified-Since            │ Fecha para cache condicional                    │
│ X-Request-ID                 │ ID unico para tracing distribuido              │
│ Cookie                       │ "session=abc123; theme=dark"                   │
├──────────────────────────────┼─────────────────────────────────────────────────┤
│ RESPONSE HEADERS             │                                                 │
├──────────────────────────────┼─────────────────────────────────────────────────┤
│ Content-Type                 │ "application/json; charset=utf-8"              │
│ Content-Length               │ "1234" (bytes del body)                        │
│ Content-Encoding             │ "gzip" (Go lo descomprime automaticamente)     │
│ Cache-Control                │ "max-age=3600, public"                         │
│ ETag                         │ "\"abc123\"" (para cache condicional)          │
│ Location                     │ URL de redirect (con 301/302/307/308)          │
│ Set-Cookie                   │ Cookie a almacenar en el jar                   │
│ X-RateLimit-Remaining        │ Requests restantes en la ventana               │
│ Retry-After                  │ Segundos a esperar (con 429/503)               │
└──────────────────────────────┴─────────────────────────────────────────────────┘
```

### 6.3 Autenticacion

```go
// Basic Auth
req, _ := http.NewRequest("GET", url, nil)
req.SetBasicAuth("username", "password")
// Equivale a: Authorization: Basic dXNlcm5hbWU6cGFzc3dvcmQ=

// Bearer Token (OAuth2, JWT)
req.Header.Set("Authorization", "Bearer eyJhbGciOiJIUzI1NiIs...")

// API Key en header custom
req.Header.Set("X-API-Key", "my-api-key-123")

// API Key en query string
u, _ := url.Parse("https://api.example.com/data")
q := u.Query()
q.Set("api_key", "my-api-key-123")
u.RawQuery = q.Encode()
req, _ = http.NewRequest("GET", u.String(), nil)

// Digest Auth (raro, no soportado nativamente — usar libreria)
```

---

## 7. Timeouts — detallados

### 7.1 Diagrama de timeouts del cliente

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    TIMEOUTS DEL CLIENTE HTTP                               │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  Client.Timeout (timeout TOTAL)                                            │
│  ◄────────────────────────────────────────────────────────────────────────► │
│                                                                             │
│  ┌────────┐  ┌────────┐  ┌──────────┐  ┌──────────┐  ┌────────────────┐  │
│  │  DNS   │  │  TCP   │  │   TLS    │  │  Send    │  │  Read response │  │
│  │ lookup │  │connect │  │handshake │  │ request  │  │  headers+body  │  │
│  └────────┘  └────────┘  └──────────┘  └──────────┘  └────────────────┘  │
│  ◄─────────────────────►                                                   │
│   Transport.DialContext                                                    │
│   (Dialer.Timeout)                                                         │
│                          ◄────────────►                                    │
│                          Transport.                                        │
│                          TLSHandshakeTimeout                               │
│                                          ◄──────────────────────────────►  │
│                                          Transport.                        │
│                                          ResponseHeaderTimeout             │
│                                                                             │
│  IMPORTANTE:                                                                │
│  - Client.Timeout cubre TODO (de DNS a leer el ultimo byte del body)     │
│  - Transport timeouts son mas granulares                                   │
│  - Si lees un body grande, Client.Timeout puede expirar DURANTE la lectura│
│  - Para streaming (bodies grandes), usa context timeout en vez de Client  │
│                                                                             │
│  Transport.ExpectContinueTimeout                                           │
│  └─ Tiempo esperando "100 Continue" antes de enviar body                  │
│     (solo para requests con Expect: 100-continue)                         │
│                                                                             │
│  Transport.IdleConnTimeout                                                 │
│  └─ Tiempo que una conexion idle se mantiene en el pool                   │
│     (no es un timeout de request — es de connection management)           │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 7.2 Configurar todos los timeouts

```go
// Cliente de produccion con todos los timeouts configurados

transport := &http.Transport{
    // Connection pool
    MaxIdleConns:        100,              // Max conexiones idle en total
    MaxIdleConnsPerHost: 10,               // Max conexiones idle por host
    MaxConnsPerHost:     20,               // Max conexiones totales por host (0=ilimitado)
    IdleConnTimeout:     90 * time.Second, // Cerrar idle conns despues de 90s
    
    // Timeouts de conexion
    DialContext: (&net.Dialer{
        Timeout:   10 * time.Second,  // Timeout de TCP connect
        KeepAlive: 30 * time.Second,  // TCP keepalive interval
    }).DialContext,
    
    // Timeouts de TLS
    TLSHandshakeTimeout: 10 * time.Second,
    
    // Timeout de response headers
    ResponseHeaderTimeout: 15 * time.Second, // Esperar max 15s por headers
    
    // Expect: 100-continue
    ExpectContinueTimeout: 1 * time.Second,
    
    // HTTP/2
    ForceAttemptHTTP2: true,
}

client := &http.Client{
    Transport: transport,
    Timeout:   30 * time.Second, // Timeout TOTAL
}
```

### 7.3 Timeout para bodies grandes (streaming)

```go
// PROBLEMA: Client.Timeout incluye la lectura del body.
// Para descargar un archivo de 1GB, 30s no es suficiente.

// SOLUCION 1: No usar Client.Timeout, usar context
client := &http.Client{
    // NO setear Timeout aqui
    Transport: &http.Transport{
        DialContext: (&net.Dialer{
            Timeout: 10 * time.Second,
        }).DialContext,
        TLSHandshakeTimeout:   10 * time.Second,
        ResponseHeaderTimeout: 15 * time.Second,
    },
}

// El timeout del context expira ANTES de leer el body
ctx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
defer cancel()

req, _ := http.NewRequestWithContext(ctx, "GET", url, nil)
resp, err := client.Do(req)
if err != nil {
    return err
}
defer resp.Body.Close()

// Cancelar el contexto despues de obtener los headers
// El body se lee sin timeout (o con su propio timeout)
cancel() // Liberar el context — los headers ya llegaron

// Leer el body sin timeout del context
// (pero si se corta la conexion, Read retornara error)
file, _ := os.Create("big-file.dat")
defer file.Close()
written, err := io.Copy(file, resp.Body)

// SOLUCION 2: Usar deadline en la conexion subyacente
// (requiere acceder al Transport, mas complejo)
```

### 7.4 Timeout con Retry-After

```go
// Cuando el servidor retorna 429 (Too Many Requests) o 503 (Unavailable),
// puede incluir un header Retry-After con el tiempo de espera.

func fetchWithRetry(client *http.Client, url string, maxRetries int) (*http.Response, error) {
    for attempt := 0; attempt <= maxRetries; attempt++ {
        resp, err := client.Get(url)
        if err != nil {
            return nil, err
        }
        
        if resp.StatusCode != http.StatusTooManyRequests &&
           resp.StatusCode != http.StatusServiceUnavailable {
            return resp, nil
        }
        
        resp.Body.Close()
        
        // Leer Retry-After
        retryAfter := resp.Header.Get("Retry-After")
        var waitTime time.Duration
        
        if retryAfter != "" {
            // Puede ser segundos o una fecha HTTP
            if seconds, err := strconv.Atoi(retryAfter); err == nil {
                waitTime = time.Duration(seconds) * time.Second
            } else if t, err := http.ParseTime(retryAfter); err == nil {
                waitTime = time.Until(t)
            }
        }
        
        if waitTime <= 0 {
            // Exponential backoff si no hay Retry-After
            waitTime = time.Duration(1<<uint(attempt)) * time.Second
        }
        
        if waitTime > 60*time.Second {
            waitTime = 60 * time.Second
        }
        
        log.Printf("Rate limited, retrying in %v (attempt %d/%d)", 
            waitTime, attempt+1, maxRetries)
        time.Sleep(waitTime)
    }
    
    return nil, fmt.Errorf("max retries exceeded for %s", url)
}
```

---

## 8. http.Transport — la capa de transporte

### 8.1 Connection pooling

```go
// http.Transport mantiene un pool de conexiones TCP abiertas.
// Las conexiones se reutilizan entre requests al mismo host.
// Esto evita el overhead de TCP handshake + TLS handshake por request.

// DefaultTransport (lo que se usa si no especificas Transport):
var DefaultTransport = &Transport{
    Proxy:                 ProxyFromEnvironment,
    DialContext:           defaultDialer.DialContext,
    ForceAttemptHTTP2:     true,
    MaxIdleConns:          100,
    IdleConnTimeout:       90 * time.Second,
    TLSHandshakeTimeout:  10 * time.Second,
    ExpectContinueTimeout: 1 * time.Second,
}

// NOTA: DefaultTransport es compartido por TODOS los clientes
// que no especifican Transport. Esto puede ser un problema si
// un paquete modifica DefaultTransport.
```

### 8.2 Configurar el pool de conexiones

```go
transport := &http.Transport{
    // MaxIdleConns: max conexiones idle totales (todos los hosts)
    // Default: 100. Reducir si tienes muchos hosts diferentes.
    MaxIdleConns: 100,
    
    // MaxIdleConnsPerHost: max idle por host
    // Default: 2. AUMENTAR si haces muchas requests al mismo host.
    // Si tienes 50 goroutines hablando con una API, pon al menos 50.
    MaxIdleConnsPerHost: 50,
    
    // MaxConnsPerHost: max conexiones TOTALES por host (idle + activas)
    // Default: 0 (ilimitado). Poner un limite previene flood accidental.
    MaxConnsPerHost: 100,
    
    // IdleConnTimeout: cerrar idle conns despues de N tiempo
    // Default: 90s. Poner mas alto si las requests son infrecuentes.
    IdleConnTimeout: 90 * time.Second,
}

// SINTOMA de MaxIdleConnsPerHost muy bajo:
// Si haces 50 requests concurrentes al mismo host con MaxIdleConnsPerHost=2,
// 48 de las 50 conexiones se CIERRAN despues de cada request (no caben en el pool).
// La siguiente request crea una nueva conexion TCP + TLS.
// Resultado: latencia alta y muchos TIME_WAIT sockets.
```

### 8.3 Proxy

```go
// Go respeta las variables de entorno HTTP_PROXY, HTTPS_PROXY, NO_PROXY

// Default: usar proxy de entorno
transport := &http.Transport{
    Proxy: http.ProxyFromEnvironment,
}

// Proxy fijo
proxyURL, _ := url.Parse("http://proxy.company.com:3128")
transport := &http.Transport{
    Proxy: http.ProxyURL(proxyURL),
}

// Proxy con autenticacion
proxyURL, _ := url.Parse("http://user:password@proxy.company.com:3128")
transport := &http.Transport{
    Proxy: http.ProxyURL(proxyURL),
}

// Sin proxy (ignorar variables de entorno)
transport := &http.Transport{
    Proxy: nil,
}

// Proxy custom (logica condicional)
transport := &http.Transport{
    Proxy: func(req *http.Request) (*url.URL, error) {
        if req.URL.Host == "internal.company.com" {
            return nil, nil // sin proxy para servicios internos
        }
        return url.Parse("http://proxy.company.com:3128")
    },
}

// SOCKS5 proxy (via golang.org/x/net/proxy)
import "golang.org/x/net/proxy"

dialer, _ := proxy.SOCKS5("tcp", "socks5.example.com:1080", nil, proxy.Direct)
transport := &http.Transport{
    DialContext: func(ctx context.Context, network, addr string) (net.Conn, error) {
        return dialer.Dial(network, addr)
    },
}
```

### 8.4 TLS personalizado

```go
import "crypto/tls"

// Configurar TLS
transport := &http.Transport{
    TLSClientConfig: &tls.Config{
        // Certificados custom (mTLS — mutual TLS)
        // El servidor requiere que el cliente presente un certificado
        // Certificates: []tls.Certificate{cert},
        
        // Root CAs custom (para CAs internas de empresa)
        // RootCAs: customCertPool,
        
        // Versiones de TLS minima/maxima
        MinVersion: tls.VersionTLS12,
        MaxVersion: tls.VersionTLS13,
        
        // InsecureSkipVerify — NUNCA en produccion
        // InsecureSkipVerify: true, // Solo para desarrollo/testing
    },
}

client := &http.Client{
    Transport: transport,
    Timeout:   30 * time.Second,
}

// mTLS — cargar certificado de cliente
cert, err := tls.LoadX509KeyPair("client-cert.pem", "client-key.pem")
if err != nil {
    log.Fatal(err)
}

transport := &http.Transport{
    TLSClientConfig: &tls.Config{
        Certificates: []tls.Certificate{cert},
    },
}
```

### 8.5 HTTP/2

```go
// Go soporta HTTP/2 automaticamente sobre TLS (HTTPS).
// No requiere ninguna configuracion especial.

client := &http.Client{
    Transport: &http.Transport{
        ForceAttemptHTTP2: true, // Default: true
    },
}

// Verificar que se uso HTTP/2
resp, _ := client.Get("https://www.google.com")
fmt.Println(resp.Proto) // "HTTP/2.0"

// Para HTTP/2 sin TLS (h2c — raro, solo desarrollo):
import "golang.org/x/net/http2"

transport := &http2.Transport{
    AllowHTTP: true,
    DialTLSContext: func(ctx context.Context, network, addr string, _ *tls.Config) (net.Conn, error) {
        return net.Dial(network, addr)
    },
}
client := &http.Client{Transport: transport}
```

---

## 9. Redirects

### 9.1 Comportamiento default

```go
// Por defecto, http.Client sigue hasta 10 redirects (301, 302, 307, 308).
// Despues de 10 redirects, retorna error.

resp, err := client.Get("http://example.com") // redirige a https://example.com
// resp.StatusCode = 200 (ya siguio el redirect)
// resp.Request.URL = "https://example.com" (la URL final)
```

### 9.2 Personalizar CheckRedirect

```go
// No seguir redirects (obtener el 301/302 directamente)
client := &http.Client{
    CheckRedirect: func(req *http.Request, via []*http.Request) error {
        return http.ErrUseLastResponse
    },
}

resp, _ := client.Get("http://example.com")
fmt.Println(resp.StatusCode) // 301
fmt.Println(resp.Header.Get("Location")) // https://example.com

// Limitar a 3 redirects
client := &http.Client{
    CheckRedirect: func(req *http.Request, via []*http.Request) error {
        if len(via) >= 3 {
            return fmt.Errorf("stopped after 3 redirects")
        }
        return nil
    },
}

// Preservar headers en redirects
// NOTA: por seguridad, Go elimina headers sensibles (Authorization)
// en redirects cross-origin. Puedes re-agregarlos:
client := &http.Client{
    CheckRedirect: func(req *http.Request, via []*http.Request) error {
        if len(via) >= 10 {
            return fmt.Errorf("too many redirects")
        }
        // Copiar Authorization del request original
        if auth := via[0].Header.Get("Authorization"); auth != "" {
            req.Header.Set("Authorization", auth)
        }
        return nil
    },
}

// Loggear redirects
client := &http.Client{
    CheckRedirect: func(req *http.Request, via []*http.Request) error {
        log.Printf("Redirect %d: %s → %s", len(via), via[len(via)-1].URL, req.URL)
        if len(via) >= 10 {
            return fmt.Errorf("too many redirects")
        }
        return nil
    },
}
```

---

## 10. Cookies

### 10.1 Cookie jar

```go
import "net/http/cookiejar"

// Crear un cookie jar (almacena cookies entre requests)
jar, err := cookiejar.New(nil)
if err != nil {
    log.Fatal(err)
}

client := &http.Client{
    Jar:     jar,
    Timeout: 30 * time.Second,
}

// Request 1: login — el servidor setea cookies (Set-Cookie header)
resp, _ := client.PostForm("https://example.com/login", url.Values{
    "username": {"alice"},
    "password": {"secret"},
})
resp.Body.Close()
// El jar almacena las cookies automaticamente

// Request 2: API call — las cookies se envian automaticamente
resp, _ = client.Get("https://example.com/api/profile")
// El jar agrego la cookie de sesion al request automaticamente
```

### 10.2 Cookies manuales

```go
// Setear cookies manualmente (sin jar)
req, _ := http.NewRequest("GET", "https://example.com/api/data", nil)
req.AddCookie(&http.Cookie{
    Name:  "session",
    Value: "abc123",
})
req.AddCookie(&http.Cookie{
    Name:  "theme",
    Value: "dark",
})

// Leer cookies de la respuesta
resp, _ := client.Do(req)
for _, cookie := range resp.Cookies() {
    fmt.Printf("Cookie: %s = %s (expires: %v)\n", 
        cookie.Name, cookie.Value, cookie.Expires)
}

// Verificar una cookie especifica
for _, cookie := range resp.Cookies() {
    if cookie.Name == "session" {
        fmt.Printf("Session ID: %s\n", cookie.Value)
    }
}
```

---

## 11. Multipart / file upload

### 11.1 Subir archivos con multipart/form-data

```go
import "mime/multipart"

func uploadFile(url, fieldName, filePath string, extraFields map[string]string) (*http.Response, error) {
    file, err := os.Open(filePath)
    if err != nil {
        return nil, err
    }
    defer file.Close()
    
    // Crear el body multipart
    var body bytes.Buffer
    writer := multipart.NewWriter(&body)
    
    // Agregar el archivo
    part, err := writer.CreateFormFile(fieldName, filepath.Base(filePath))
    if err != nil {
        return nil, err
    }
    if _, err := io.Copy(part, file); err != nil {
        return nil, err
    }
    
    // Agregar campos extra
    for key, val := range extraFields {
        writer.WriteField(key, val)
    }
    
    // Cerrar el writer (agrega el boundary final)
    contentType := writer.FormDataContentType()
    writer.Close()
    
    // Crear request
    req, err := http.NewRequest("POST", url, &body)
    if err != nil {
        return nil, err
    }
    req.Header.Set("Content-Type", contentType)
    
    client := &http.Client{Timeout: 5 * time.Minute}
    return client.Do(req)
}

// Uso:
resp, err := uploadFile(
    "https://api.example.com/upload",
    "document",
    "/path/to/file.pdf",
    map[string]string{
        "title":       "My Document",
        "description": "Important file",
    },
)
```

### 11.2 Upload con pipe (sin cargar todo en memoria)

```go
// Para archivos grandes, usar io.Pipe para no cargar el archivo
// completo en memoria antes de enviarlo.

func uploadLargeFile(url, filePath string) (*http.Response, error) {
    file, err := os.Open(filePath)
    if err != nil {
        return nil, err
    }
    defer file.Close()
    
    // Pipe: el writer escribe en el pipe,
    // el reader se pasa como body del request.
    pr, pw := io.Pipe()
    
    writer := multipart.NewWriter(pw)
    
    // Escribir en goroutine (el pipe es bloqueante)
    go func() {
        defer pw.Close()
        defer writer.Close()
        
        part, err := writer.CreateFormFile("file", filepath.Base(filePath))
        if err != nil {
            pw.CloseWithError(err)
            return
        }
        
        if _, err := io.Copy(part, file); err != nil {
            pw.CloseWithError(err)
            return
        }
    }()
    
    req, err := http.NewRequest("POST", url, pr)
    if err != nil {
        return nil, err
    }
    req.Header.Set("Content-Type", writer.FormDataContentType())
    
    // Sin Content-Length: usa chunked transfer encoding
    // El servidor debe soportar Transfer-Encoding: chunked
    
    client := &http.Client{Timeout: 30 * time.Minute}
    return client.Do(req)
}
```

---

## 12. Descargar archivos

### 12.1 Descarga basica con progreso

```go
func downloadFile(url, destPath string) error {
    client := &http.Client{
        // No usar Timeout para descargas grandes
        Transport: &http.Transport{
            DialContext: (&net.Dialer{
                Timeout: 10 * time.Second,
            }).DialContext,
            TLSHandshakeTimeout:   10 * time.Second,
            ResponseHeaderTimeout: 15 * time.Second,
        },
    }
    
    resp, err := client.Get(url)
    if err != nil {
        return err
    }
    defer resp.Body.Close()
    
    if resp.StatusCode != http.StatusOK {
        return fmt.Errorf("HTTP %d", resp.StatusCode)
    }
    
    // Crear archivo destino
    file, err := os.Create(destPath)
    if err != nil {
        return err
    }
    defer file.Close()
    
    // Copiar con progreso
    size := resp.ContentLength // -1 si desconocido
    
    var reader io.Reader = resp.Body
    if size > 0 {
        reader = &ProgressReader{
            Reader: resp.Body,
            Total:  size,
            OnProgress: func(downloaded, total int64) {
                pct := float64(downloaded) / float64(total) * 100
                fmt.Fprintf(os.Stderr, "\r%.1f%% (%s / %s)",
                    pct, formatBytes(downloaded), formatBytes(total))
            },
        }
    }
    
    written, err := io.Copy(file, reader)
    fmt.Fprintln(os.Stderr) // newline despues del progreso
    
    if err != nil {
        os.Remove(destPath) // Limpiar archivo parcial
        return err
    }
    
    fmt.Fprintf(os.Stderr, "Descargado: %s (%s)\n", destPath, formatBytes(written))
    return nil
}

// ProgressReader — wrapper que reporta progreso
type ProgressReader struct {
    Reader     io.Reader
    Total      int64
    downloaded int64
    OnProgress func(downloaded, total int64)
}

func (pr *ProgressReader) Read(p []byte) (int, error) {
    n, err := pr.Reader.Read(p)
    if n > 0 {
        pr.downloaded += int64(n)
        if pr.OnProgress != nil {
            pr.OnProgress(pr.downloaded, pr.Total)
        }
    }
    return n, err
}

func formatBytes(b int64) string {
    const (
        KB = 1024
        MB = KB * 1024
        GB = MB * 1024
    )
    switch {
    case b >= GB:
        return fmt.Sprintf("%.2f GB", float64(b)/float64(GB))
    case b >= MB:
        return fmt.Sprintf("%.2f MB", float64(b)/float64(MB))
    case b >= KB:
        return fmt.Sprintf("%.2f KB", float64(b)/float64(KB))
    default:
        return fmt.Sprintf("%d B", b)
    }
}
```

### 12.2 Descarga resumible con Range header

```go
// HTTP Range requests permiten descargar parcialmente un archivo.
// Util para resumir descargas interrumpidas.

func downloadResumable(url, destPath string) error {
    client := &http.Client{
        Transport: &http.Transport{
            ResponseHeaderTimeout: 15 * time.Second,
        },
    }
    
    var startByte int64
    
    // Verificar si ya existe un archivo parcial
    if info, err := os.Stat(destPath); err == nil {
        startByte = info.Size()
    }
    
    req, err := http.NewRequest("GET", url, nil)
    if err != nil {
        return err
    }
    
    if startByte > 0 {
        // Solicitar desde donde nos quedamos
        req.Header.Set("Range", fmt.Sprintf("bytes=%d-", startByte))
        fmt.Fprintf(os.Stderr, "Resumiendo desde %s\n", formatBytes(startByte))
    }
    
    resp, err := client.Do(req)
    if err != nil {
        return err
    }
    defer resp.Body.Close()
    
    var file *os.File
    
    switch resp.StatusCode {
    case http.StatusOK: // 200 — servidor no soporta Range, empieza de cero
        file, err = os.Create(destPath)
        startByte = 0
        
    case http.StatusPartialContent: // 206 — Range soportado, datos parciales
        file, err = os.OpenFile(destPath, os.O_APPEND|os.O_WRONLY, 0644)
        
    default:
        return fmt.Errorf("HTTP %d", resp.StatusCode)
    }
    
    if err != nil {
        return err
    }
    defer file.Close()
    
    written, err := io.Copy(file, resp.Body)
    if err != nil {
        return fmt.Errorf("download interrupted at %s: %w", 
            formatBytes(startByte+written), err)
    }
    
    fmt.Fprintf(os.Stderr, "Completado: %s total\n", formatBytes(startByte+written))
    return nil
}
```

---

## 13. Retries y resiliencia

### 13.1 Retry con exponential backoff

```go
// Patron de retry para requests HTTP con exponential backoff y jitter.

type RetryConfig struct {
    MaxRetries  int
    BaseDelay   time.Duration
    MaxDelay    time.Duration
    RetryOn     func(resp *http.Response, err error) bool
}

var defaultRetryConfig = RetryConfig{
    MaxRetries: 3,
    BaseDelay:  1 * time.Second,
    MaxDelay:   30 * time.Second,
    RetryOn: func(resp *http.Response, err error) bool {
        if err != nil {
            return true // Reintentar errores de red
        }
        // Reintentar errores de servidor (transitorios)
        return resp.StatusCode == 429 || // Too Many Requests
               resp.StatusCode == 502 || // Bad Gateway
               resp.StatusCode == 503 || // Service Unavailable
               resp.StatusCode == 504    // Gateway Timeout
    },
}

func doWithRetry(client *http.Client, req *http.Request, cfg RetryConfig) (*http.Response, error) {
    var lastErr error
    var lastResp *http.Response
    
    for attempt := 0; attempt <= cfg.MaxRetries; attempt++ {
        if attempt > 0 {
            delay := cfg.BaseDelay * time.Duration(1<<uint(attempt-1))
            if delay > cfg.MaxDelay {
                delay = cfg.MaxDelay
            }
            // Jitter: ±25%
            jitter := time.Duration(rand.Int63n(int64(delay) / 2))
            delay = delay/2 + jitter
            
            // Respetar Retry-After si existe
            if lastResp != nil {
                if ra := lastResp.Header.Get("Retry-After"); ra != "" {
                    if secs, err := strconv.Atoi(ra); err == nil {
                        raDelay := time.Duration(secs) * time.Second
                        if raDelay > delay {
                            delay = raDelay
                        }
                    }
                }
            }
            
            log.Printf("Retry %d/%d in %v", attempt, cfg.MaxRetries, delay)
            
            select {
            case <-time.After(delay):
            case <-req.Context().Done():
                return nil, req.Context().Err()
            }
        }
        
        // Para reintentar, necesitamos un body nuevo
        // (el body anterior ya fue leido/cerrado)
        if req.GetBody != nil {
            var err error
            req.Body, err = req.GetBody()
            if err != nil {
                return nil, fmt.Errorf("get body for retry: %w", err)
            }
        }
        
        resp, err := client.Do(req)
        
        if !cfg.RetryOn(resp, err) {
            return resp, err
        }
        
        // Guardar para Retry-After
        lastErr = err
        if resp != nil {
            lastResp = resp
            // Drenar y cerrar body para reutilizar la conexion
            io.Copy(io.Discard, io.LimitReader(resp.Body, 1<<20))
            resp.Body.Close()
        }
    }
    
    if lastResp != nil {
        return lastResp, lastErr
    }
    return nil, fmt.Errorf("max retries exceeded: %w", lastErr)
}

// Crear request retryable (con GetBody)
func newRetryableRequest(method, url string, body []byte) (*http.Request, error) {
    req, err := http.NewRequest(method, url, bytes.NewReader(body))
    if err != nil {
        return nil, err
    }
    
    // GetBody permite recrear el body para retries
    req.GetBody = func() (io.ReadCloser, error) {
        return io.NopCloser(bytes.NewReader(body)), nil
    }
    
    return req, nil
}
```

### 13.2 Circuit breaker simple

```go
// Circuit breaker: si un servicio falla consistentemente,
// dejar de intentar durante un periodo.

type CircuitBreaker struct {
    mu          sync.Mutex
    failures    int
    threshold   int           // fallos para abrir el circuito
    resetAfter  time.Duration // tiempo antes de probar de nuevo
    lastFailure time.Time
    state       string // "closed", "open", "half-open"
}

func NewCircuitBreaker(threshold int, resetAfter time.Duration) *CircuitBreaker {
    return &CircuitBreaker{
        threshold:  threshold,
        resetAfter: resetAfter,
        state:      "closed",
    }
}

func (cb *CircuitBreaker) Allow() bool {
    cb.mu.Lock()
    defer cb.mu.Unlock()
    
    switch cb.state {
    case "closed":
        return true
    case "open":
        // Verificar si paso suficiente tiempo para probar de nuevo
        if time.Since(cb.lastFailure) > cb.resetAfter {
            cb.state = "half-open"
            return true
        }
        return false
    case "half-open":
        return true // permitir un intento de prueba
    }
    return false
}

func (cb *CircuitBreaker) RecordSuccess() {
    cb.mu.Lock()
    defer cb.mu.Unlock()
    cb.failures = 0
    cb.state = "closed"
}

func (cb *CircuitBreaker) RecordFailure() {
    cb.mu.Lock()
    defer cb.mu.Unlock()
    cb.failures++
    cb.lastFailure = time.Now()
    if cb.failures >= cb.threshold {
        cb.state = "open"
    }
}

func (cb *CircuitBreaker) State() string {
    cb.mu.Lock()
    defer cb.mu.Unlock()
    return cb.state
}

// Uso:
breaker := NewCircuitBreaker(5, 30*time.Second)

func callAPI(url string) error {
    if !breaker.Allow() {
        return fmt.Errorf("circuit breaker open: service %s unavailable", url)
    }
    
    resp, err := httpClient.Get(url)
    if err != nil || resp.StatusCode >= 500 {
        breaker.RecordFailure()
        if resp != nil {
            resp.Body.Close()
        }
        return fmt.Errorf("service error")
    }
    defer resp.Body.Close()
    
    breaker.RecordSuccess()
    return nil
}
```

---

## 14. URL parsing y construccion

### 14.1 net/url

```go
import "net/url"

// Parsear una URL
u, err := url.Parse("https://user:pass@api.example.com:8443/v1/users?page=2&limit=10#section")
if err != nil {
    log.Fatal(err)
}

fmt.Println(u.Scheme)   // "https"
fmt.Println(u.User)     // "user:pass"
fmt.Println(u.Host)     // "api.example.com:8443"
fmt.Println(u.Hostname()) // "api.example.com" (sin puerto)
fmt.Println(u.Port())   // "8443"
fmt.Println(u.Path)     // "/v1/users"
fmt.Println(u.RawQuery) // "page=2&limit=10"
fmt.Println(u.Fragment) // "section"

// Parsear query string
q := u.Query() // url.Values (map[string][]string)
fmt.Println(q.Get("page"))  // "2"
fmt.Println(q.Get("limit")) // "10"

// Construir URL con query params
u = &url.URL{
    Scheme: "https",
    Host:   "api.example.com",
    Path:   "/v1/search",
}
q = u.Query()
q.Set("q", "hello world")      // se encodea como "hello+world"
q.Set("page", "1")
q.Add("tag", "go")
q.Add("tag", "programming")    // multiples valores
u.RawQuery = q.Encode()

fmt.Println(u.String())
// "https://api.example.com/v1/search?page=1&q=hello+world&tag=go&tag=programming"

// Resolver URL relativa
base, _ := url.Parse("https://api.example.com/v1/")
ref, _ := url.Parse("users/123")
resolved := base.ResolveReference(ref)
fmt.Println(resolved) // "https://api.example.com/v1/users/123"

// Path escaping
escaped := url.PathEscape("hello world/test")
fmt.Println(escaped) // "hello%20world%2Ftest"

// Query escaping
escaped = url.QueryEscape("hello world&test=true")
fmt.Println(escaped) // "hello+world%26test%3Dtrue"
```

---

## 15. Comparacion con C y Rust

```
┌────────────────────────────────┬───────────────────────────┬──────────────────────────┬──────────────────────────┐
│ Aspecto                        │ C                         │ Go                       │ Rust                     │
├────────────────────────────────┼───────────────────────────┼──────────────────────────┼──────────────────────────┤
│ Libreria HTTP                  │ libcurl (externo)         │ net/http (stdlib)        │ reqwest (externo)        │
│ Alternativa bajo nivel         │ sockets raw + parse HTTP  │ net.Conn (raro usarlo)   │ hyper (externo)          │
│ GET simple                     │ curl_easy_perform(curl)   │ http.Get(url)            │ reqwest::get(url).await  │
│ POST JSON                      │ curl + CURLOPT_POSTFIELDS │ http.Post(url, ct, body) │ client.post(url).json()  │
│ Crear request custom           │ curl_easy_setopt(...)     │ http.NewRequest(...)     │ client.request(m, url)   │
│ Setear header                  │ curl_slist_append(hdrs)   │ req.Header.Set(k, v)     │ req.header(k, v)         │
│ Timeout                        │ CURLOPT_TIMEOUT           │ Client{Timeout: d}       │ Client::timeout(d)       │
│ Connection timeout             │ CURLOPT_CONNECTTIMEOUT    │ Dialer.Timeout            │ Client::connect_timeout  │
│ Cookie jar                     │ CURLOPT_COOKIEJAR         │ Client{Jar: jar}         │ Client::cookie_store     │
│ Follow redirects               │ CURLOPT_FOLLOWLOCATION    │ automatico (default 10)  │ Client::redirect(Policy) │
│ Proxy                          │ CURLOPT_PROXY             │ Transport{Proxy: ...}    │ Client::proxy(proxy)     │
│ TLS custom                     │ CURLOPT_SSLCERT           │ TLSClientConfig          │ Client::use_rustls_tls   │
│ Connection pooling             │ CURLOPT_MAXCONNECTS       │ Transport (automatico)   │ Client (automatico)      │
│ HTTP/2                         │ CURLOPT_HTTP_VERSION      │ automatico (HTTPS)       │ Client::http2_prior()    │
│ Multipart upload               │ curl_mime_addpart         │ multipart.NewWriter      │ multipart::Form          │
│ Streaming response             │ CURLOPT_WRITEFUNCTION     │ resp.Body (io.Reader)    │ .bytes_stream()          │
│ Async requests                 │ curl_multi_perform        │ goroutines               │ async/await (nativo)     │
│ Error type                     │ CURLcode enum             │ error interface          │ Result<T, reqwest::Error>│
│ Close body                     │ (no aplica)               │ resp.Body.Close()        │ (automatico con Drop)    │
│ Basic auth                     │ CURLOPT_USERPWD           │ req.SetBasicAuth(u, p)   │ .basic_auth(u, p)        │
│ Respuesta como string          │ buffer + callback         │ io.ReadAll(resp.Body)    │ resp.text().await        │
│ Respuesta como JSON            │ jansson/cJSON (externo)   │ json.NewDecoder(body)    │ resp.json::<T>().await   │
│ Download a archivo             │ CURLOPT_WRITEDATA + fopen │ io.Copy(file, resp.Body) │ tokio::io::copy()        │
│ Retry logic                    │ manual loop               │ manual loop              │ reqwest-retry crate      │
│ URL parsing                    │ curl_url_set/get          │ url.Parse()              │ url::Url::parse()        │
│ Status code check              │ curl_easy_getinfo()       │ resp.StatusCode          │ resp.status()            │
│ Thread safety                  │ no (por handle)           │ si (Client es safe)      │ si (Client es Send+Sync) │
│ Dependencias                   │ libcurl + libssl          │ ninguna (stdlib)         │ reqwest + tokio + rustls │
│ Tamano binario (GET simple)    │ ~20KB + libs dinamicas    │ ~5MB                     │ ~2MB                     │
│ Lineas para GET+JSON parse     │ ~50 lineas               │ ~10 lineas               │ ~5 lineas                │
└────────────────────────────────┴───────────────────────────┴──────────────────────────┴──────────────────────────┘
```

### Insight: GET + JSON decode en los tres lenguajes

```
// C (libcurl + cJSON):
CURL *curl = curl_easy_init();
struct MemoryStruct chunk = {0};
curl_easy_setopt(curl, CURLOPT_URL, "https://api.example.com/user");
curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
CURLcode res = curl_easy_perform(curl);
if (res != CURLE_OK) { /* error */ }
cJSON *json = cJSON_Parse(chunk.memory);
const char *name = cJSON_GetObjectItem(json, "name")->valuestring;
cJSON_Delete(json);
free(chunk.memory);
curl_easy_cleanup(curl);
// ~20 lineas, 2 librerias externas

// Go (stdlib):
client := &http.Client{Timeout: 30 * time.Second}
resp, err := client.Get("https://api.example.com/user")
if err != nil { return err }
defer resp.Body.Close()
var user struct { Name string `json:"name"` }
json.NewDecoder(resp.Body).Decode(&user)
fmt.Println(user.Name)
// ~8 lineas, 0 dependencias

// Rust (reqwest):
let user: User = reqwest::Client::new()
    .get("https://api.example.com/user")
    .timeout(Duration::from_secs(30))
    .send().await?
    .json().await?;
println!("{}", user.name);
// ~5 lineas, 3+ dependencias (reqwest, tokio, serde)
```

---

## 16. Programa completo: API Client robusto

Un cliente HTTP que consume una API REST con retry, circuit breaker, timeouts, y logging.

```go
package main

import (
    "bytes"
    "context"
    "encoding/json"
    "fmt"
    "io"
    "log"
    "math/rand"
    "net"
    "net/http"
    "os"
    "strconv"
    "strings"
    "sync"
    "time"
)

// --- Tipos del API ---

type User struct {
    ID        int       `json:"id"`
    Name      string    `json:"name"`
    Email     string    `json:"email"`
    Role      string    `json:"role"`
    CreatedAt time.Time `json:"created_at"`
}

type APIError struct {
    StatusCode int
    Body       string
    Message    string
}

func (e *APIError) Error() string {
    return fmt.Sprintf("API error %d: %s", e.StatusCode, e.Message)
}

// --- Circuit Breaker ---

type circuitState int

const (
    circuitClosed circuitState = iota
    circuitOpen
    circuitHalfOpen
)

type CircuitBreaker struct {
    mu          sync.Mutex
    state       circuitState
    failures    int
    threshold   int
    resetAfter  time.Duration
    lastFailure time.Time
}

func (cb *CircuitBreaker) Allow() bool {
    cb.mu.Lock()
    defer cb.mu.Unlock()
    switch cb.state {
    case circuitClosed:
        return true
    case circuitOpen:
        if time.Since(cb.lastFailure) > cb.resetAfter {
            cb.state = circuitHalfOpen
            return true
        }
        return false
    case circuitHalfOpen:
        return true
    }
    return false
}

func (cb *CircuitBreaker) RecordSuccess() {
    cb.mu.Lock()
    defer cb.mu.Unlock()
    cb.failures = 0
    cb.state = circuitClosed
}

func (cb *CircuitBreaker) RecordFailure() {
    cb.mu.Lock()
    defer cb.mu.Unlock()
    cb.failures++
    cb.lastFailure = time.Now()
    if cb.failures >= cb.threshold {
        cb.state = circuitOpen
        log.Printf("[CIRCUIT] opened after %d failures", cb.failures)
    }
}

// --- API Client ---

type APIClient struct {
    baseURL    string
    httpClient *http.Client
    apiKey     string
    breaker    *CircuitBreaker
    
    // Config de retry
    maxRetries int
    baseDelay  time.Duration
    maxDelay   time.Duration
}

func NewAPIClient(baseURL, apiKey string) *APIClient {
    transport := &http.Transport{
        DialContext: (&net.Dialer{
            Timeout:   10 * time.Second,
            KeepAlive: 30 * time.Second,
        }).DialContext,
        MaxIdleConnsPerHost:   20,
        MaxConnsPerHost:       50,
        IdleConnTimeout:       90 * time.Second,
        TLSHandshakeTimeout:   10 * time.Second,
        ResponseHeaderTimeout: 15 * time.Second,
        ForceAttemptHTTP2:     true,
    }
    
    return &APIClient{
        baseURL: strings.TrimRight(baseURL, "/"),
        apiKey:  apiKey,
        httpClient: &http.Client{
            Transport: transport,
            Timeout:   30 * time.Second,
        },
        breaker: &CircuitBreaker{
            threshold:  5,
            resetAfter: 30 * time.Second,
        },
        maxRetries: 3,
        baseDelay:  time.Second,
        maxDelay:   15 * time.Second,
    }
}

func (c *APIClient) do(ctx context.Context, method, path string, body interface{}) (*http.Response, error) {
    // Circuit breaker
    if !c.breaker.Allow() {
        return nil, fmt.Errorf("circuit breaker open: service unavailable")
    }
    
    // Preparar body
    var bodyReader io.Reader
    var bodyBytes []byte
    if body != nil {
        var err error
        bodyBytes, err = json.Marshal(body)
        if err != nil {
            return nil, fmt.Errorf("marshal body: %w", err)
        }
        bodyReader = bytes.NewReader(bodyBytes)
    }
    
    url := c.baseURL + path
    
    // Retry loop
    var lastErr error
    for attempt := 0; attempt <= c.maxRetries; attempt++ {
        if attempt > 0 {
            delay := c.baseDelay * time.Duration(1<<uint(attempt-1))
            if delay > c.maxDelay {
                delay = c.maxDelay
            }
            jitter := time.Duration(rand.Int63n(int64(delay) / 2))
            delay = delay/2 + jitter
            
            log.Printf("[RETRY] %s %s attempt %d/%d in %v", method, path, attempt, c.maxRetries, delay)
            
            select {
            case <-time.After(delay):
            case <-ctx.Done():
                return nil, ctx.Err()
            }
            
            // Recrear body reader para retry
            if bodyBytes != nil {
                bodyReader = bytes.NewReader(bodyBytes)
            }
        }
        
        req, err := http.NewRequestWithContext(ctx, method, url, bodyReader)
        if err != nil {
            return nil, err
        }
        
        // Headers
        req.Header.Set("Authorization", "Bearer "+c.apiKey)
        req.Header.Set("Accept", "application/json")
        req.Header.Set("User-Agent", "GoAPIClient/1.0")
        if body != nil {
            req.Header.Set("Content-Type", "application/json")
        }
        
        resp, err := c.httpClient.Do(req)
        if err != nil {
            lastErr = err
            c.breaker.RecordFailure()
            continue
        }
        
        // Exito o error de cliente (no reintentar)
        if resp.StatusCode < 500 && resp.StatusCode != 429 {
            c.breaker.RecordSuccess()
            return resp, nil
        }
        
        // Error de servidor o rate limit — reintentar
        c.breaker.RecordFailure()
        io.Copy(io.Discard, io.LimitReader(resp.Body, 1<<20))
        resp.Body.Close()
        lastErr = fmt.Errorf("HTTP %d", resp.StatusCode)
    }
    
    return nil, fmt.Errorf("max retries exceeded: %w", lastErr)
}

func (c *APIClient) decodeResponse(resp *http.Response, target interface{}) error {
    defer resp.Body.Close()
    
    if resp.StatusCode < 200 || resp.StatusCode >= 300 {
        body, _ := io.ReadAll(io.LimitReader(resp.Body, 4096))
        return &APIError{
            StatusCode: resp.StatusCode,
            Body:       string(body),
            Message:    resp.Status,
        }
    }
    
    if target == nil {
        return nil
    }
    
    return json.NewDecoder(resp.Body).Decode(target)
}

// --- Metodos del API ---

func (c *APIClient) ListUsers(ctx context.Context, page, limit int) ([]User, error) {
    path := fmt.Sprintf("/api/v1/users?page=%d&limit=%d", page, limit)
    
    resp, err := c.do(ctx, "GET", path, nil)
    if err != nil {
        return nil, err
    }
    
    var users []User
    if err := c.decodeResponse(resp, &users); err != nil {
        return nil, err
    }
    
    return users, nil
}

func (c *APIClient) GetUser(ctx context.Context, id int) (*User, error) {
    path := fmt.Sprintf("/api/v1/users/%d", id)
    
    resp, err := c.do(ctx, "GET", path, nil)
    if err != nil {
        return nil, err
    }
    
    var user User
    if err := c.decodeResponse(resp, &user); err != nil {
        return nil, err
    }
    
    return &user, nil
}

func (c *APIClient) CreateUser(ctx context.Context, name, email, role string) (*User, error) {
    body := map[string]string{
        "name":  name,
        "email": email,
        "role":  role,
    }
    
    resp, err := c.do(ctx, "POST", "/api/v1/users", body)
    if err != nil {
        return nil, err
    }
    
    var user User
    if err := c.decodeResponse(resp, &user); err != nil {
        return nil, err
    }
    
    return &user, nil
}

func (c *APIClient) UpdateUser(ctx context.Context, id int, updates map[string]string) (*User, error) {
    path := fmt.Sprintf("/api/v1/users/%d", id)
    
    resp, err := c.do(ctx, "PATCH", path, updates)
    if err != nil {
        return nil, err
    }
    
    var user User
    if err := c.decodeResponse(resp, &user); err != nil {
        return nil, err
    }
    
    return &user, nil
}

func (c *APIClient) DeleteUser(ctx context.Context, id int) error {
    path := fmt.Sprintf("/api/v1/users/%d", id)
    
    resp, err := c.do(ctx, "DELETE", path, nil)
    if err != nil {
        return err
    }
    
    return c.decodeResponse(resp, nil)
}

// --- Requests concurrentes ---

func (c *APIClient) GetUsersParallel(ctx context.Context, ids []int) ([]*User, []error) {
    users := make([]*User, len(ids))
    errs := make([]error, len(ids))
    
    var wg sync.WaitGroup
    sem := make(chan struct{}, 10) // Max 10 requests simultaneas
    
    for i, id := range ids {
        wg.Add(1)
        sem <- struct{}{} // Adquirir slot
        
        go func(idx, userID int) {
            defer wg.Done()
            defer func() { <-sem }() // Liberar slot
            
            user, err := c.GetUser(ctx, userID)
            users[idx] = user
            errs[idx] = err
        }(i, id)
    }
    
    wg.Wait()
    return users, errs
}

// --- Main ---

func main() {
    apiKey := os.Getenv("API_KEY")
    if apiKey == "" {
        apiKey = "demo-key"
    }
    
    baseURL := os.Getenv("API_URL")
    if baseURL == "" {
        baseURL = "https://jsonplaceholder.typicode.com"
    }
    
    client := NewAPIClient(baseURL, apiKey)
    
    ctx, cancel := context.WithTimeout(context.Background(), 60*time.Second)
    defer cancel()
    
    // Listar usuarios
    fmt.Println("=== Listar usuarios ===")
    users, err := client.ListUsers(ctx, 1, 5)
    if err != nil {
        log.Printf("list users: %v", err)
    } else {
        for _, u := range users {
            fmt.Printf("  [%d] %s <%s>\n", u.ID, u.Name, u.Email)
        }
    }
    
    // Obtener un usuario
    fmt.Println("\n=== Obtener usuario 1 ===")
    user, err := client.GetUser(ctx, 1)
    if err != nil {
        log.Printf("get user: %v", err)
    } else {
        data, _ := json.MarshalIndent(user, "  ", "  ")
        fmt.Printf("  %s\n", data)
    }
    
    // Requests paralelas
    fmt.Println("\n=== Obtener usuarios 1-5 en paralelo ===")
    ids := []int{1, 2, 3, 4, 5}
    users2, errs := client.GetUsersParallel(ctx, ids)
    for i, u := range users2 {
        if errs[i] != nil {
            fmt.Printf("  [%d] ERROR: %v\n", ids[i], errs[i])
        } else {
            fmt.Printf("  [%d] %s\n", u.ID, u.Name)
        }
    }
    
    // Demostrar manejo de errores
    fmt.Println("\n=== Usuario inexistente ===")
    _, err = client.GetUser(ctx, 99999)
    if err != nil {
        var apiErr *APIError
        if ok := errors.As(err, &apiErr); ok {
            fmt.Printf("  API Error: status=%d body=%s\n", apiErr.StatusCode, apiErr.Body)
        } else {
            fmt.Printf("  Error: %v\n", err)
        }
    }
}
```

```
PROBAR EL PROGRAMA:

# Contra jsonplaceholder (API publica de prueba)
go run main.go

# Output:
# === Listar usuarios ===
#   [1] Leanne Graham <Sincere@april.biz>
#   [2] Ervin Howell <Shanna@melissa.tv>
#   [3] Clementine Bauch <Nathan@yesenia.net>
#   [4] Patricia Lebsack <Julianne.OConner@kory.org>
#   [5] Chelsey Dietrich <Lucio_Hettinger@annie.io>
#
# === Obtener usuario 1 ===
#   {
#     "id": 1,
#     "name": "Leanne Graham",
#     "email": "Sincere@april.biz",
#     ...
#   }
#
# === Obtener usuarios 1-5 en paralelo ===
#   [1] Leanne Graham
#   [2] Ervin Howell
#   [3] Clementine Bauch
#   [4] Patricia Lebsack
#   [5] Chelsey Dietrich

# Con API custom:
API_URL=https://my-api.example.com API_KEY=secret123 go run main.go

CONCEPTOS DEMOSTRADOS:
  http.Client:            reutilizable, con Transport configurado
  http.NewRequestWithContext: control total, cancelable
  http.Transport:         connection pool, timeouts granulares, HTTP/2
  Headers:                Authorization, Content-Type, Accept, User-Agent
  JSON decode:            json.NewDecoder(resp.Body) — streaming
  Status codes:           check + APIError tipado
  Retry:                  exponential backoff con jitter, Retry-After
  Circuit breaker:        closed/open/half-open states
  Concurrencia:           requests paralelas con WaitGroup + semaforo
  Context:                timeout global, cancelacion per-request
  Error handling:         errors.As para tipos especificos
  resp.Body.Close():      siempre, en todos los paths
```

---

## 17. Ejercicios

### Ejercicio 1: Descargador paralelo
Crea un programa que:
- Reciba una lista de URLs (como argumentos o desde un archivo, una por linea)
- Descargue todas en paralelo (flag `-workers N` para limitar concurrencia)
- Muestre progreso por archivo (porcentaje, velocidad, ETA)
- Soporte descarga resumible (`Range` header): si un archivo existe parcialmente, reanuda
- Guarde en el directorio especificado por `-output dir`
- Verifique el `Content-Length` al final (advertir si difiere)
- Reintente 3 veces con exponential backoff en caso de error
- Resumen final: exitosos, fallidos, total descargado, tiempo total

### Ejercicio 2: API health checker
Crea un monitor de APIs que:
- Lea endpoints desde un archivo JSON: `[{"url": "...", "method": "GET", "expect_status": 200, "timeout": "5s"}]`
- Haga requests periodicas (flag `-interval 30s`)
- Para cada endpoint reporte: status, latencia, status code, body size
- Calcule uptime porcentual por endpoint (ultimas N comprobaciones)
- Soporte headers custom por endpoint (Authorization, etc.)
- Escriba resultados a stdout como tabla o JSON (`-format table|json`)
- Soporte notificacion cuando un endpoint cambia de estado (up→down o down→up) — escribir a stderr
- Graceful shutdown con SIGINT

### Ejercicio 3: GitHub API client
Crea un cliente CLI para la API de GitHub usando solo net/http:
- Subcomandos: `repos <user>` (listar repos), `issues <owner/repo>` (listar issues), `star <owner/repo>` (dar star), `profile <user>` (ver perfil)
- Autenticacion con token (flag `-token` o env `GITHUB_TOKEN`)
- Paginacion automatica: GitHub limita a 30 items por pagina, seguir el header `Link` para obtener todas las paginas
- Rate limit: leer headers `X-RateLimit-Remaining` y `X-RateLimit-Reset`, pausar si queda poco
- Output: tabla y JSON (`-format table|json`)
- Cache condicional: usar `If-None-Match` con ETag para evitar requests innecesarias
- Retry con exponential backoff para errores 5xx

### Ejercicio 4: HTTP proxy logger
Crea un proxy HTTP forward que:
- Escuche en un puerto local: `./proxy -listen :8080`
- Reenvie todas las requests al destino original (como un proxy HTTP normal)
- Logee cada request: method, URL, status code, response time, body size
- Soporte filtrado: `-filter-host *.example.com` para solo logear ciertos dominios
- Soporte `-dump-body` para guardar request/response bodies en archivos
- Estadisticas en un endpoint especial: `GET http://proxy-local/stats` → JSON con requests totales, errores, latencia promedio por host
- Configure el proxy en tu navegador o con `HTTP_PROXY=http://localhost:8080 curl ...`

---

> **Siguiente**: C10/S02 - HTTP, T02 - net/http servidor — http.HandleFunc, http.ServeMux (Go 1.22+ con metodos y patrones), middleware
