# net/http servidor — http.HandleFunc, http.ServeMux (Go 1.22+ con metodos y patrones), middleware

## 1. Introduccion

Go incluye un servidor HTTP de produccion en la stdlib. No es un servidor "de juguete" — es el mismo que usan servicios como Kubernetes, Docker Registry, y Cloudflare. A partir de Go 1.22, el `ServeMux` soporta routing con metodos HTTP y path parameters, eliminando la necesidad de routers externos para la mayoria de los casos.

```
┌───────────────────────────────────────────────────────────────────────────────────┐
│                    net/http SERVIDOR — ARQUITECTURA                              │
├───────────────────────────────────────────────────────────────────────────────────┤
│                                                                                   │
│  FLUJO DE UN REQUEST                                                             │
│                                                                                   │
│  Cliente HTTP                                                                     │
│       │                                                                           │
│       ▼                                                                           │
│  ┌──────────┐                                                                    │
│  │ net.Conn │  TCP connection (puede ser TLS)                                    │
│  └────┬─────┘                                                                    │
│       │                                                                           │
│       ▼                                                                           │
│  ┌──────────────┐                                                                │
│  │ http.Server  │  Acepta conexiones, parsea HTTP, maneja keepalive             │
│  │              │  Una goroutine por conexion (automatico)                       │
│  └────┬─────────┘                                                                │
│       │                                                                           │
│       ▼                                                                           │
│  ┌──────────────────────────────────────────────────────┐                        │
│  │ http.Handler (interface)                              │                        │
│  │                                                        │                        │
│  │  type Handler interface {                              │                        │
│  │      ServeHTTP(ResponseWriter, *Request)               │                        │
│  │  }                                                     │                        │
│  │                                                        │                        │
│  │  Implementaciones:                                     │                        │
│  │  ├─ http.ServeMux     → router/multiplexer            │                        │
│  │  ├─ http.HandlerFunc  → adaptor func → Handler        │                        │
│  │  ├─ http.FileServer   → servir archivos estaticos     │                        │
│  │  ├─ http.StripPrefix  → remover prefijo de path       │                        │
│  │  ├─ Middleware         → wrappers de Handler           │                        │
│  │  └─ Tu codigo custom  → cualquier struct con ServeHTTP│                        │
│  └──────────────────────────────────────────────────────┘                        │
│       │                                                                           │
│       ▼                                                                           │
│  ┌──────────────────────────────────────────────────────┐                        │
│  │ ServeMux (Go 1.22+) — router con patrones avanzados  │                        │
│  │                                                        │                        │
│  │  PATRONES:                                             │                        │
│  │  ├─ "GET /users"           → metodo + path exacto    │                        │
│  │  ├─ "POST /users"          → diferente handler por    │                        │
│  │  │                           metodo en el mismo path  │                        │
│  │  ├─ "GET /users/{id}"      → path parameter          │                        │
│  │  ├─ "GET /files/{path...}" → wildcard (rest of path) │                        │
│  │  ├─ "GET /api/"            → subtree (prefix match)  │                        │
│  │  ├─ "GET example.com/..."  → host-based routing      │                        │
│  │  └─ Precedencia: mas especifico gana                  │                        │
│  └──────────────────────────────────────────────────────┘                        │
│       │                                                                           │
│       ▼                                                                           │
│  ┌──────────────────────────────────────────────────────┐                        │
│  │ Handler function                                       │                        │
│  │                                                        │                        │
│  │  func(w http.ResponseWriter, r *http.Request) {       │                        │
│  │      // r = request entrante (method, URL, headers,   │                        │
│  │      //     body, context, path params)               │                        │
│  │      // w = donde escribir la respuesta               │                        │
│  │      //     (status code, headers, body)              │                        │
│  │      w.Header().Set("Content-Type", "application/json")│                       │
│  │      w.WriteHeader(http.StatusOK)                      │                        │
│  │      json.NewEncoder(w).Encode(data)                  │                        │
│  │  }                                                     │                        │
│  └──────────────────────────────────────────────────────┘                        │
│                                                                                   │
│  COMPARACION DE ROUTERS                                                          │
│  ├─ Go <1.22:  ServeMux solo path prefix, sin metodos, sin params              │
│  │             → todos usaban gorilla/mux o chi                                  │
│  ├─ Go 1.22+: ServeMux con metodos + path params + wildcards                   │
│  │             → suficiente para la mayoria de APIs                              │
│  ├─ chi:       compatible con stdlib, middleware chain, groups                   │
│  ├─ gin:       framework completo, rapido, mucho uso                            │
│  └─ echo:      framework completo, similar a gin                                │
│                                                                                   │
│  COMPARACION CON OTROS LENGUAJES                                                 │
│  ├─ C:      no tiene (nginx/apache son servidores separados)                    │
│  ├─ Rust:   hyper (bajo nivel) + axum/actix-web (frameworks, externos)          │
│  ├─ Python: Flask/Django (frameworks, externos) + uvicorn                       │
│  ├─ Node:   http.createServer (stdlib) + Express (externo)                      │
│  └─ Go:     net/http (stdlib, produccion-ready, incluyendo router)              │
└───────────────────────────────────────────────────────────────────────────────────┘
```

---

## 2. El minimo servidor HTTP

### 2.1 Tres lineas

```go
package main

import (
    "fmt"
    "net/http"
)

func main() {
    http.HandleFunc("GET /hello", func(w http.ResponseWriter, r *http.Request) {
        fmt.Fprintln(w, "Hello, World!")
    })
    
    http.ListenAndServe(":8080", nil) // nil = usa DefaultServeMux
}

// Probar: curl http://localhost:8080/hello
// Output: Hello, World!
```

### 2.2 Desglose de lo que pasa

```go
// http.HandleFunc registra un handler en http.DefaultServeMux (global)
// Equivalente a:
//   http.DefaultServeMux.HandleFunc("GET /hello", handler)

// http.ListenAndServe(addr, handler):
// 1. Crea un net.Listener en addr
// 2. Acepta conexiones TCP en un loop
// 3. Para cada conexion, lanza una goroutine que:
//    a. Parsea la request HTTP
//    b. Llama handler.ServeHTTP(w, r)
//    c. Si handler es nil, usa DefaultServeMux
//    d. Maneja HTTP keepalive (multiples requests por conexion)

// PROBLEMA: http.ListenAndServe NO retorna error hasta que falla.
// Si el puerto esta ocupado, retorna error inmediatamente.
// Si el servidor esta corriendo, solo retorna al morir.

err := http.ListenAndServe(":8080", nil)
log.Fatal(err) // "listen tcp :8080: bind: address already in use"
```

---

## 3. http.Handler — la interface central

### 3.1 La interface

```go
// http.Handler es LA interface central del servidor HTTP de Go.
// Todo lo que procesa requests HTTP implementa esta interface.

type Handler interface {
    ServeHTTP(ResponseWriter, *Request)
}

// Cualquier struct puede ser un handler si tiene ServeHTTP:
type myHandler struct {
    greeting string
}

func (h *myHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    fmt.Fprintf(w, "%s, visitor!\n", h.greeting)
}

func main() {
    handler := &myHandler{greeting: "Welcome"}
    http.ListenAndServe(":8080", handler)
}
```

### 3.2 http.HandlerFunc — adaptor de funciones

```go
// http.HandlerFunc convierte una funcion en un Handler.
// Es un type con metodo ServeHTTP que se llama a si mismo.

type HandlerFunc func(ResponseWriter, *Request)

func (f HandlerFunc) ServeHTTP(w ResponseWriter, r *Request) {
    f(w, r) // se llama a si misma
}

// Esto permite usar funciones directamente como handlers:
func helloHandler(w http.ResponseWriter, r *http.Request) {
    fmt.Fprintln(w, "Hello!")
}

func main() {
    // Estas tres formas son equivalentes:
    
    // Forma 1: HandleFunc (registra la funcion directamente)
    http.HandleFunc("GET /hello", helloHandler)
    
    // Forma 2: Handle con HandlerFunc (conversion explicita)
    http.Handle("GET /hello", http.HandlerFunc(helloHandler))
    
    // Forma 3: Handle con closure
    http.Handle("GET /hello", http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        fmt.Fprintln(w, "Hello!")
    }))
}
```

### 3.3 Struct como handler (con estado)

```go
// Los struct handlers son utiles cuando necesitas estado:
// - Conexion a base de datos
// - Logger configurado
// - Cache
// - Configuracion

type UserHandler struct {
    db     *sql.DB
    logger *slog.Logger
    cache  *Cache
}

func (h *UserHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    // Acceder al estado via h.db, h.logger, h.cache
    users, err := h.db.Query("SELECT * FROM users")
    if err != nil {
        h.logger.Error("query failed", "error", err)
        http.Error(w, "internal error", http.StatusInternalServerError)
        return
    }
    // ...
}

// O con metodos individuales (mas comun):
func (h *UserHandler) List(w http.ResponseWriter, r *http.Request) {
    // GET /users
}

func (h *UserHandler) Get(w http.ResponseWriter, r *http.Request) {
    // GET /users/{id}
}

func (h *UserHandler) Create(w http.ResponseWriter, r *http.Request) {
    // POST /users
}

// Registrar:
func main() {
    db, _ := sql.Open("postgres", "...")
    uh := &UserHandler{db: db, logger: slog.Default()}
    
    mux := http.NewServeMux()
    mux.HandleFunc("GET /users", uh.List)
    mux.HandleFunc("GET /users/{id}", uh.Get)
    mux.HandleFunc("POST /users", uh.Create)
    
    http.ListenAndServe(":8080", mux)
}
```

---

## 4. http.ServeMux — el router (Go 1.22+)

### 4.1 Crear un ServeMux propio (no usar DefaultServeMux)

```go
// DefaultServeMux es global y cualquier paquete puede registrar handlers.
// En produccion, SIEMPRE crear tu propio ServeMux.

// MAL: usar DefaultServeMux (global, no controlado)
func main() {
    http.HandleFunc("/api/users", usersHandler) // registra en DefaultServeMux
    http.ListenAndServe(":8080", nil)           // nil = DefaultServeMux
}

// BIEN: ServeMux propio
func main() {
    mux := http.NewServeMux()
    mux.HandleFunc("GET /api/users", usersHandler)
    mux.HandleFunc("POST /api/users", createUserHandler)
    
    http.ListenAndServe(":8080", mux)
}
```

### 4.2 Patrones de routing (Go 1.22+)

```go
mux := http.NewServeMux()

// --- METODOS HTTP ---

// Metodo explicito: solo acepta ese metodo
mux.HandleFunc("GET /users", listUsers)      // Solo GET
mux.HandleFunc("POST /users", createUser)    // Solo POST
mux.HandleFunc("DELETE /users", deleteAll)   // Solo DELETE

// Sin metodo: acepta CUALQUIER metodo (como antes de 1.22)
mux.HandleFunc("/health", healthCheck)       // GET, POST, PUT, etc.

// Si el cliente usa un metodo no registrado para esa ruta:
// → 405 Method Not Allowed (automatico)
// → Incluye header "Allow: GET, POST" con los metodos validos

// --- PATH PARAMETERS ---

// {nombre} captura un segmento del path
mux.HandleFunc("GET /users/{id}", getUser)
// /users/42     → id = "42"
// /users/alice  → id = "alice"
// /users/       → NO match (el parametro es requerido)
// /users/42/foo → NO match (demasiados segmentos)

// Leer el parametro en el handler:
func getUser(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    fmt.Fprintf(w, "User ID: %s\n", id)
}

// Multiples parametros
mux.HandleFunc("GET /repos/{owner}/{repo}/issues/{number}", getIssue)

func getIssue(w http.ResponseWriter, r *http.Request) {
    owner := r.PathValue("owner")
    repo := r.PathValue("repo")
    number := r.PathValue("number")
    fmt.Fprintf(w, "%s/%s#%s\n", owner, repo, number)
}

// --- WILDCARD (rest of path) ---

// {name...} captura el RESTO del path (incluyendo /)
mux.HandleFunc("GET /files/{path...}", serveFiles)
// /files/docs/readme.md → path = "docs/readme.md"
// /files/               → path = ""
// /files/a/b/c/d.txt    → path = "a/b/c/d.txt"

func serveFiles(w http.ResponseWriter, r *http.Request) {
    filePath := r.PathValue("path")
    fmt.Fprintf(w, "File: %s\n", filePath)
}

// --- SUBTREE (trailing slash) ---

// Path con / final: match como PREFIJO (subtree pattern)
mux.HandleFunc("GET /api/", apiCatchAll)
// /api/         → match
// /api/foo      → match
// /api/foo/bar  → match

// Path sin / final: match EXACTO
mux.HandleFunc("GET /api", apiRoot)
// /api  → match
// /api/ → NO match (redirect a /api si hay un handler para /api/)

// --- HOST-BASED ROUTING ---

// Prefijo con host: solo requests a ese host
mux.HandleFunc("GET api.example.com/users", apiUsers)
mux.HandleFunc("GET www.example.com/", webHandler)

// Sin host: cualquier host
mux.HandleFunc("GET /users", allHostsUsers)
```

### 4.3 Precedencia de patrones

```go
// Cuando multiples patrones pueden matchear, Go usa el MAS ESPECIFICO.

mux := http.NewServeMux()

mux.HandleFunc("GET /users", listUsers)           // Exacto
mux.HandleFunc("GET /users/{id}", getUser)         // Con param
mux.HandleFunc("GET /users/me", getMyProfile)      // Literal mas especifico
mux.HandleFunc("GET /", notFoundHandler)           // Catch-all
mux.HandleFunc("GET /api/", apiHandler)            // Subtree

// GET /users         → listUsers (exacto)
// GET /users/42      → getUser (param)
// GET /users/me      → getMyProfile (literal > param)
// GET /users/me/edit → 404 (no hay match)
// GET /api/v1/data   → apiHandler (subtree)
// GET /random        → notFoundHandler (catch-all)

// REGLA DE PRECEDENCIA:
// 1. Host-specific > no-host
// 2. Mas segmentos literales > menos
// 3. Literal > parameter ({id})
// 4. Parameter > wildcard ({path...})
// 5. Si hay conflicto EXACTO → panic en tiempo de registro
```

### 4.4 Path parameters: conversion y validacion

```go
// PathValue siempre retorna string. Debes convertir manualmente.

func getUser(w http.ResponseWriter, r *http.Request) {
    idStr := r.PathValue("id")
    
    id, err := strconv.Atoi(idStr)
    if err != nil {
        http.Error(w, "invalid user ID", http.StatusBadRequest)
        return
    }
    
    if id <= 0 {
        http.Error(w, "user ID must be positive", http.StatusBadRequest)
        return
    }
    
    // Usar id (int)
    fmt.Fprintf(w, "User %d\n", id)
}

// Helper para extraer y validar path params
func pathParamInt(r *http.Request, name string) (int, error) {
    s := r.PathValue(name)
    if s == "" {
        return 0, fmt.Errorf("missing path parameter: %s", name)
    }
    n, err := strconv.Atoi(s)
    if err != nil {
        return 0, fmt.Errorf("invalid %s: %q", name, s)
    }
    return n, nil
}

// Uso:
func getUser(w http.ResponseWriter, r *http.Request) {
    id, err := pathParamInt(r, "id")
    if err != nil {
        http.Error(w, err.Error(), http.StatusBadRequest)
        return
    }
    // ...
}
```

---

## 5. http.ResponseWriter — escribir la respuesta

### 5.1 La interface

```go
type ResponseWriter interface {
    // Header retorna el mapa de headers para modificar ANTES de WriteHeader/Write.
    // Modificar headers DESPUES de Write no tiene efecto.
    Header() http.Header
    
    // Write escribe datos al body de la respuesta.
    // Si WriteHeader no se llamo, llama WriteHeader(200) implicitamente.
    // Si Content-Type no esta seteado, Go lo detecta automaticamente.
    Write([]byte) (int, error)
    
    // WriteHeader envia el status code.
    // Solo se puede llamar UNA VEZ.
    // Debe llamarse ANTES de Write.
    WriteHeader(statusCode int)
}
```

### 5.2 Orden de operaciones (CRITICO)

```go
func handler(w http.ResponseWriter, r *http.Request) {
    // ORDEN CORRECTO:
    // 1. Setear headers
    // 2. WriteHeader (status code)
    // 3. Write (body)
    
    // 1. Headers PRIMERO
    w.Header().Set("Content-Type", "application/json")
    w.Header().Set("X-Request-ID", "abc123")
    
    // 2. Status code
    w.WriteHeader(http.StatusCreated) // 201
    
    // 3. Body
    json.NewEncoder(w).Encode(map[string]string{"status": "created"})
    
    // ---
    
    // ERRORES COMUNES:
    
    // ERROR 1: setear header DESPUES de Write
    w.Write([]byte("hello"))
    w.Header().Set("X-Custom", "value") // NO TIENE EFECTO — ya se enviaron headers
    
    // ERROR 2: llamar WriteHeader dos veces
    w.WriteHeader(200)
    w.WriteHeader(500) // superfluous response.WriteHeader call (log warning)
    
    // ERROR 3: Write sin Content-Type
    w.Write([]byte("<html>Hello</html>"))
    // Go detecta "text/html; charset=utf-8" automaticamente
    // Pero es mejor ser explicito
}
```

### 5.3 Respuestas comunes

```go
// --- Texto plano ---
func textHandler(w http.ResponseWriter, r *http.Request) {
    w.Header().Set("Content-Type", "text/plain; charset=utf-8")
    fmt.Fprintln(w, "Hello, World!")
    // WriteHeader(200) se llama implicitamente por fmt.Fprintln
}

// --- JSON ---
func jsonHandler(w http.ResponseWriter, r *http.Request) {
    data := map[string]interface{}{
        "message": "hello",
        "count":   42,
    }
    
    w.Header().Set("Content-Type", "application/json")
    json.NewEncoder(w).Encode(data)
}

// --- JSON con status code ---
func createHandler(w http.ResponseWriter, r *http.Request) {
    // ... crear recurso ...
    
    w.Header().Set("Content-Type", "application/json")
    w.Header().Set("Location", "/api/users/123")
    w.WriteHeader(http.StatusCreated) // 201
    json.NewEncoder(w).Encode(newUser)
}

// --- Sin body (204 No Content) ---
func deleteHandler(w http.ResponseWriter, r *http.Request) {
    // ... borrar recurso ...
    w.WriteHeader(http.StatusNoContent)
    // No llamar Write — no hay body
}

// --- Error con http.Error ---
func handler(w http.ResponseWriter, r *http.Request) {
    // http.Error es un helper que setea Content-Type text/plain,
    // llama WriteHeader, y escribe el mensaje.
    
    if r.URL.Query().Get("key") == "" {
        http.Error(w, "missing required parameter: key", http.StatusBadRequest)
        return // IMPORTANTE: return despues de Error
    }
}

// --- Redirect ---
func handler(w http.ResponseWriter, r *http.Request) {
    http.Redirect(w, r, "/new-location", http.StatusMovedPermanently) // 301
    // O 302 (Found), 307 (Temporary), 308 (Permanent)
}

// --- Archivo ---
func handler(w http.ResponseWriter, r *http.Request) {
    http.ServeFile(w, r, "/path/to/file.pdf")
    // Setea Content-Type automaticamente, soporta Range requests,
    // soporta If-Modified-Since, etc.
}

// --- HTML ---
func handler(w http.ResponseWriter, r *http.Request) {
    w.Header().Set("Content-Type", "text/html; charset=utf-8")
    tmpl := template.Must(template.ParseFiles("page.html"))
    tmpl.Execute(w, data)
}
```

### 5.4 Helper: writeJSON reutilizable

```go
// Helper que se usa en la mayoria de APIs

func writeJSON(w http.ResponseWriter, status int, data interface{}) {
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(status)
    
    if data != nil {
        if err := json.NewEncoder(w).Encode(data); err != nil {
            // Ya se enviaron headers — no podemos cambiar el status code.
            // Solo podemos loggear.
            log.Printf("writeJSON encode error: %v", err)
        }
    }
}

func writeError(w http.ResponseWriter, status int, message string) {
    writeJSON(w, status, map[string]string{"error": message})
}

// Uso:
func listUsers(w http.ResponseWriter, r *http.Request) {
    users, err := db.GetUsers()
    if err != nil {
        writeError(w, http.StatusInternalServerError, "failed to fetch users")
        return
    }
    writeJSON(w, http.StatusOK, users)
}
```

---

## 6. http.Request — leer la request entrante

### 6.1 Anatomia del Request del servidor

```go
func handler(w http.ResponseWriter, r *http.Request) {
    // --- Metodo y URL ---
    r.Method  // "GET", "POST", "PUT", "DELETE", "PATCH", "HEAD", "OPTIONS"
    r.URL     // *url.URL parseado
    r.URL.Path      // "/api/users/42"
    r.URL.RawQuery  // "page=2&limit=10"
    r.RequestURI    // "/api/users/42?page=2&limit=10" (raw, no parseado)
    r.Proto         // "HTTP/1.1" o "HTTP/2.0"
    
    // --- Path parameters (Go 1.22+) ---
    r.PathValue("id") // valor del {id} en el patron
    
    // --- Query parameters ---
    r.URL.Query()                // url.Values (map[string][]string)
    r.URL.Query().Get("page")   // "2" (primer valor)
    r.URL.Query().Has("page")   // true
    r.URL.Query()["tag"]        // ["go", "programming"] (todos los valores)
    
    // --- Headers ---
    r.Header.Get("Authorization")   // "Bearer token123"
    r.Header.Get("Content-Type")    // "application/json"
    r.Header.Get("Accept")          // "application/json"
    r.Header.Get("User-Agent")      // "curl/7.68.0"
    r.Header.Get("X-Forwarded-For") // IP real detras de proxy/LB
    r.Host                          // "api.example.com:8080"
    
    // --- Body ---
    r.Body          // io.ReadCloser (debe cerrarse, pero el servidor lo hace automaticamente)
    r.ContentLength // -1 si desconocida
    
    // --- Context ---
    r.Context()     // context.Context (se cancela cuando el cliente cierra la conexion)
    
    // --- Direccion del cliente ---
    r.RemoteAddr    // "192.168.1.100:45678" (puede ser IP del proxy)
    
    // --- Form data (debe parsearse primero) ---
    r.ParseForm()
    r.Form.Get("username")     // query params + body form data
    r.PostForm.Get("username") // solo body form data
    
    // --- Multipart ---
    r.ParseMultipartForm(10 << 20) // max 10 MB en memoria
    file, header, err := r.FormFile("upload")
    
    // --- Cookies ---
    cookie, err := r.Cookie("session")
    cookies := r.Cookies()
    
    // --- TLS ---
    if r.TLS != nil {
        // Request fue sobre HTTPS
        r.TLS.PeerCertificates // certificados del cliente (mTLS)
    }
    
    // --- Basic Auth ---
    username, password, ok := r.BasicAuth()
}
```

### 6.2 Leer JSON body

```go
func createUser(w http.ResponseWriter, r *http.Request) {
    // Verificar Content-Type
    ct := r.Header.Get("Content-Type")
    if ct != "" && !strings.HasPrefix(ct, "application/json") {
        writeError(w, http.StatusUnsupportedMediaType, "Content-Type must be application/json")
        return
    }
    
    // Limitar tamano del body (prevenir OOM)
    r.Body = http.MaxBytesReader(w, r.Body, 1<<20) // Max 1 MB
    
    var input struct {
        Name  string `json:"name"`
        Email string `json:"email"`
    }
    
    decoder := json.NewDecoder(r.Body)
    decoder.DisallowUnknownFields() // Rechazar campos extra
    
    if err := decoder.Decode(&input); err != nil {
        // Clasificar el error
        var maxBytesErr *http.MaxBytesError
        switch {
        case errors.As(err, &maxBytesErr):
            writeError(w, http.StatusRequestEntityTooLarge, "body too large")
        case errors.Is(err, io.EOF):
            writeError(w, http.StatusBadRequest, "body is empty")
        case strings.HasPrefix(err.Error(), "json: unknown field"):
            writeError(w, http.StatusBadRequest, err.Error())
        default:
            writeError(w, http.StatusBadRequest, "invalid JSON: "+err.Error())
        }
        return
    }
    
    // Verificar que no hay mas JSON en el body (single object)
    if decoder.More() {
        writeError(w, http.StatusBadRequest, "body must contain a single JSON object")
        return
    }
    
    // Validar campos
    if input.Name == "" {
        writeError(w, http.StatusBadRequest, "name is required")
        return
    }
    if !strings.Contains(input.Email, "@") {
        writeError(w, http.StatusBadRequest, "invalid email")
        return
    }
    
    // Procesar...
    writeJSON(w, http.StatusCreated, map[string]string{
        "message": "user created",
        "name":    input.Name,
    })
}
```

### 6.3 Leer query parameters con defaults

```go
func listItems(w http.ResponseWriter, r *http.Request) {
    q := r.URL.Query()
    
    // Pagination con defaults
    page := queryInt(q, "page", 1)
    limit := queryInt(q, "limit", 20)
    
    // Validar limites
    if page < 1 {
        page = 1
    }
    if limit < 1 || limit > 100 {
        limit = 20
    }
    
    // Filtros opcionales
    sortBy := queryString(q, "sort", "created_at")
    order := queryString(q, "order", "desc")
    search := q.Get("search") // "" si no existe
    
    // Tags multiples: ?tag=go&tag=web
    tags := q["tag"] // []string o nil
    
    fmt.Fprintf(w, "page=%d limit=%d sort=%s order=%s search=%q tags=%v\n",
        page, limit, sortBy, order, search, tags)
}

func queryInt(q url.Values, key string, defaultVal int) int {
    s := q.Get(key)
    if s == "" {
        return defaultVal
    }
    n, err := strconv.Atoi(s)
    if err != nil {
        return defaultVal
    }
    return n
}

func queryString(q url.Values, key, defaultVal string) string {
    s := q.Get(key)
    if s == "" {
        return defaultVal
    }
    return s
}
```

### 6.4 Form data y file upload

```go
// POST application/x-www-form-urlencoded
func loginHandler(w http.ResponseWriter, r *http.Request) {
    if err := r.ParseForm(); err != nil {
        http.Error(w, "invalid form data", http.StatusBadRequest)
        return
    }
    
    username := r.PostFormValue("username")
    password := r.PostFormValue("password")
    remember := r.PostFormValue("remember") == "on"
    
    // Procesar login...
}

// POST multipart/form-data (file upload)
func uploadHandler(w http.ResponseWriter, r *http.Request) {
    // Limitar tamano total del request
    r.Body = http.MaxBytesReader(w, r.Body, 50<<20) // Max 50 MB
    
    // Parsear multipart form
    // El argumento es el max de memoria para archivos (el resto va a disco temp)
    if err := r.ParseMultipartForm(10 << 20); err != nil { // 10 MB en memoria
        http.Error(w, "file too large", http.StatusRequestEntityTooLarge)
        return
    }
    
    // Obtener el archivo
    file, header, err := r.FormFile("document")
    if err != nil {
        http.Error(w, "missing file: document", http.StatusBadRequest)
        return
    }
    defer file.Close()
    
    fmt.Printf("Archivo: %s (%d bytes, type: %s)\n",
        header.Filename, header.Size, header.Header.Get("Content-Type"))
    
    // Guardar a disco
    dst, err := os.Create(filepath.Join("/uploads", filepath.Base(header.Filename)))
    if err != nil {
        http.Error(w, "save error", http.StatusInternalServerError)
        return
    }
    defer dst.Close()
    
    written, err := io.Copy(dst, file)
    if err != nil {
        http.Error(w, "save error", http.StatusInternalServerError)
        return
    }
    
    // Campos del form ademas del archivo
    title := r.FormValue("title")
    
    writeJSON(w, http.StatusOK, map[string]interface{}{
        "filename": header.Filename,
        "size":     written,
        "title":    title,
    })
}
```

---

## 7. Middleware

Middleware es un patron donde "envuelves" un handler con logica adicional que se ejecuta antes y/o despues del handler original. En Go, un middleware es simplemente una funcion que recibe un `http.Handler` y retorna otro `http.Handler`.

### 7.1 Concepto

```
┌─────────────────────────────────────────────────────────────────┐
│                    MIDDLEWARE CHAIN                              │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Request → Logging → Auth → RateLimit → Handler → Response     │
│                                                                  │
│  Cada middleware:                                                │
│  1. Recibe el handler original (next)                           │
│  2. Retorna un NUEVO handler que:                               │
│     a. Hace algo ANTES (logging, auth check, timer start)      │
│     b. Llama next.ServeHTTP(w, r)                               │
│     c. Hace algo DESPUES (log duration, add headers)           │
│     d. O NO llama next (reject: 401, 429, etc.)               │
│                                                                  │
│  func middleware(next http.Handler) http.Handler {              │
│      return http.HandlerFunc(func(w, r) {                      │
│          // ANTES                                                │
│          next.ServeHTTP(w, r) // delegar                        │
│          // DESPUES                                              │
│      })                                                         │
│  }                                                               │
│                                                                  │
│  COMPOSICION:                                                    │
│  handler := logging(auth(rateLimit(myHandler)))                 │
│  // Ejecuta: logging → auth → rateLimit → myHandler            │
│  // El primero en la cadena es el primero en ejecutar           │
└─────────────────────────────────────────────────────────────────┘
```

### 7.2 Middleware: Logging

```go
func loggingMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        
        // Wrapper para capturar el status code
        wrapped := &statusRecorder{ResponseWriter: w, statusCode: 200}
        
        // Llamar al handler
        next.ServeHTTP(wrapped, r)
        
        // Loggear DESPUES
        duration := time.Since(start)
        log.Printf("%s %s %d %v %s",
            r.Method,
            r.URL.Path,
            wrapped.statusCode,
            duration,
            r.RemoteAddr,
        )
    })
}

// statusRecorder captura el status code que el handler escribe
type statusRecorder struct {
    http.ResponseWriter
    statusCode int
    written    bool
}

func (r *statusRecorder) WriteHeader(code int) {
    if !r.written {
        r.statusCode = code
        r.written = true
    }
    r.ResponseWriter.WriteHeader(code)
}

func (r *statusRecorder) Write(b []byte) (int, error) {
    if !r.written {
        r.statusCode = 200
        r.written = true
    }
    return r.ResponseWriter.Write(b)
}
```

### 7.3 Middleware: Recovery (panic handler)

```go
func recoveryMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                // Loggear el stack trace
                log.Printf("PANIC: %v\n%s", err, debug.Stack())
                
                // Responder 500 al cliente
                http.Error(w, "Internal Server Error", http.StatusInternalServerError)
            }
        }()
        
        next.ServeHTTP(w, r)
    })
}
```

### 7.4 Middleware: CORS

```go
func corsMiddleware(allowedOrigins []string) func(http.Handler) http.Handler {
    originSet := make(map[string]bool)
    for _, o := range allowedOrigins {
        originSet[o] = true
    }
    
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            origin := r.Header.Get("Origin")
            
            if originSet["*"] || originSet[origin] {
                w.Header().Set("Access-Control-Allow-Origin", origin)
                w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS, PATCH")
                w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Request-ID")
                w.Header().Set("Access-Control-Max-Age", "86400")
            }
            
            // Preflight request
            if r.Method == "OPTIONS" {
                w.WriteHeader(http.StatusNoContent)
                return
            }
            
            next.ServeHTTP(w, r)
        })
    }
}
```

### 7.5 Middleware: Rate limiting

```go
func rateLimitMiddleware(requestsPerSecond float64, burst int) func(http.Handler) http.Handler {
    // Un limiter por IP
    type visitor struct {
        limiter  *rate.Limiter
        lastSeen time.Time
    }
    
    var (
        mu       sync.Mutex
        visitors = make(map[string]*visitor)
    )
    
    // Cleanup goroutine
    go func() {
        for {
            time.Sleep(time.Minute)
            mu.Lock()
            for ip, v := range visitors {
                if time.Since(v.lastSeen) > 3*time.Minute {
                    delete(visitors, ip)
                }
            }
            mu.Unlock()
        }
    }()
    
    getVisitor := func(ip string) *rate.Limiter {
        mu.Lock()
        defer mu.Unlock()
        
        v, exists := visitors[ip]
        if !exists {
            limiter := rate.NewLimiter(rate.Limit(requestsPerSecond), burst)
            visitors[ip] = &visitor{limiter: limiter, lastSeen: time.Now()}
            return limiter
        }
        v.lastSeen = time.Now()
        return v.limiter
    }
    
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            ip, _, _ := net.SplitHostPort(r.RemoteAddr)
            limiter := getVisitor(ip)
            
            if !limiter.Allow() {
                w.Header().Set("Retry-After", "1")
                http.Error(w, "rate limit exceeded", http.StatusTooManyRequests)
                return
            }
            
            next.ServeHTTP(w, r)
        })
    }
}

// Requiere: go get golang.org/x/time/rate
```

### 7.6 Middleware: Request ID

```go
func requestIDMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        // Usar el ID del cliente si existe, sino generar uno
        requestID := r.Header.Get("X-Request-ID")
        if requestID == "" {
            requestID = generateID()
        }
        
        // Agregar al response header
        w.Header().Set("X-Request-ID", requestID)
        
        // Agregar al context para uso en handlers
        ctx := context.WithValue(r.Context(), requestIDKey, requestID)
        
        next.ServeHTTP(w, r.WithContext(ctx))
    })
}

type contextKey string

const requestIDKey contextKey = "requestID"

func getRequestID(r *http.Request) string {
    id, _ := r.Context().Value(requestIDKey).(string)
    return id
}

func generateID() string {
    b := make([]byte, 8)
    rand.Read(b)
    return fmt.Sprintf("%x", b)
}
```

### 7.7 Middleware: Auth

```go
func authMiddleware(validTokens map[string]string) func(http.Handler) http.Handler {
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            auth := r.Header.Get("Authorization")
            if auth == "" {
                w.Header().Set("WWW-Authenticate", "Bearer")
                writeError(w, http.StatusUnauthorized, "missing authorization header")
                return
            }
            
            // Extraer token
            parts := strings.SplitN(auth, " ", 2)
            if len(parts) != 2 || parts[0] != "Bearer" {
                writeError(w, http.StatusUnauthorized, "invalid authorization format")
                return
            }
            token := parts[1]
            
            // Validar token
            userID, valid := validTokens[token]
            if !valid {
                writeError(w, http.StatusUnauthorized, "invalid token")
                return
            }
            
            // Agregar user info al context
            ctx := context.WithValue(r.Context(), userIDKey, userID)
            next.ServeHTTP(w, r.WithContext(ctx))
        })
    }
}

const userIDKey contextKey = "userID"

func getUserID(r *http.Request) string {
    id, _ := r.Context().Value(userIDKey).(string)
    return id
}
```

### 7.8 Componer middleware

```go
// Funcion que aplica middleware en orden

func chain(handler http.Handler, middlewares ...func(http.Handler) http.Handler) http.Handler {
    // Aplicar en orden inverso para que el primero se ejecute primero
    for i := len(middlewares) - 1; i >= 0; i-- {
        handler = middlewares[i](handler)
    }
    return handler
}

// Uso:
func main() {
    mux := http.NewServeMux()
    mux.HandleFunc("GET /api/users", listUsers)
    mux.HandleFunc("POST /api/users", createUser)
    mux.HandleFunc("GET /health", healthCheck)
    
    // Aplicar middleware a todas las rutas
    handler := chain(mux,
        recoveryMiddleware,                            // 1. Recover panics
        requestIDMiddleware,                           // 2. Request ID
        loggingMiddleware,                             // 3. Logging
        corsMiddleware([]string{"http://localhost:3000"}), // 4. CORS
        rateLimitMiddleware(10, 20),                   // 5. Rate limiting
    )
    
    http.ListenAndServe(":8080", handler)
}
```

### 7.9 Middleware selectivo (solo para algunas rutas)

```go
// A veces no quieres que un middleware aplique a TODAS las rutas.
// Por ejemplo, /health no necesita auth.

func main() {
    mux := http.NewServeMux()
    
    // Rutas publicas (sin auth)
    mux.HandleFunc("GET /health", healthCheck)
    mux.HandleFunc("POST /auth/login", loginHandler)
    
    // Rutas protegidas (con auth)
    tokens := map[string]string{"secret-token": "user-1"}
    auth := authMiddleware(tokens)
    
    mux.Handle("GET /api/users", auth(http.HandlerFunc(listUsers)))
    mux.Handle("POST /api/users", auth(http.HandlerFunc(createUser)))
    mux.Handle("GET /api/users/{id}", auth(http.HandlerFunc(getUser)))
    
    // Middleware global (aplica a todo)
    handler := chain(mux,
        recoveryMiddleware,
        loggingMiddleware,
    )
    
    http.ListenAndServe(":8080", handler)
}
```

---

## 8. http.Server — configuracion de produccion

### 8.1 http.Server vs http.ListenAndServe

```go
// http.ListenAndServe es un shortcut que no permite configurar timeouts.
// En produccion, SIEMPRE usar http.Server directamente.

// MAL (sin timeouts, sin graceful shutdown):
http.ListenAndServe(":8080", handler)

// BIEN:
server := &http.Server{
    Addr:    ":8080",
    Handler: handler,
    
    // Timeouts
    ReadTimeout:       5 * time.Second,   // Tiempo max para leer request completo
    ReadHeaderTimeout: 2 * time.Second,   // Tiempo max para leer headers
    WriteTimeout:      10 * time.Second,  // Tiempo max para escribir response
    IdleTimeout:       120 * time.Second, // Timeout para keepalive idle connections
    MaxHeaderBytes:    1 << 20,           // Max tamano de headers (1 MB)
    
    // Logging de errores internos
    ErrorLog: log.New(os.Stderr, "[HTTP] ", log.LstdFlags),
}

if err := server.ListenAndServe(); err != http.ErrServerClosed {
    log.Fatalf("server: %v", err)
}
```

### 8.2 Diagrama de timeouts del servidor

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                    TIMEOUTS DEL SERVIDOR HTTP                              │
├─────────────────────────────────────────────────────────────────────────────┤
│                                                                             │
│  ReadHeaderTimeout         ReadTimeout                                      │
│  ◄────────────────►  ◄──────────────────────────────────►                  │
│                                                                             │
│  ┌────────────────┐  ┌──────────────────┐  ┌──────────────────────────┐   │
│  │  Read headers  │  │   Read body      │  │    Write response        │   │
│  │  (+ request    │  │   (POST data)    │  │    (headers + body)      │   │
│  │   line)        │  │                  │  │                          │   │
│  └────────────────┘  └──────────────────┘  └──────────────────────────┘   │
│                                                                             │
│                                              ◄──────────────────────────►  │
│                                                    WriteTimeout            │
│                                                                             │
│  ◄──────────────────────────────────────────────────────────────────────►  │
│  ReadTimeout + WriteTimeout = duracion maxima total de un request          │
│                                                                             │
│                       (despues de la response)                              │
│                                              ◄──────────────────────────►  │
│                                                    IdleTimeout             │
│                                              (keepalive entre requests)    │
│                                                                             │
│  ReadTimeout:       protege contra slowloris (enviar headers lentamente)  │
│  ReadHeaderTimeout: subconjunto de ReadTimeout, solo headers              │
│  WriteTimeout:      protege contra clientes lentos que no leen            │
│  IdleTimeout:       cierra conexiones keepalive inactivas                 │
│                                                                             │
│  NOTA: WriteTimeout empieza al FINAL de leer headers (no del body).      │
│  Si ReadTimeout=5s y WriteTimeout=10s, el total es 5+10=15s.             │
│  Pero si el handler tarda mucho procesando, WriteTimeout expira           │
│  y la conexion se cierra abruptamente.                                    │
│                                                                             │
│  Para timeouts per-request (mas flexible), usar context en el handler.    │
└─────────────────────────────────────────────────────────────────────────────┘
```

### 8.3 Graceful shutdown del servidor HTTP

```go
func main() {
    mux := http.NewServeMux()
    mux.HandleFunc("GET /", func(w http.ResponseWriter, r *http.Request) {
        // Simular request lento
        time.Sleep(2 * time.Second)
        fmt.Fprintln(w, "done")
    })
    mux.HandleFunc("GET /health", func(w http.ResponseWriter, r *http.Request) {
        w.WriteHeader(200)
    })
    
    server := &http.Server{
        Addr:              ":8080",
        Handler:           mux,
        ReadTimeout:       5 * time.Second,
        ReadHeaderTimeout: 2 * time.Second,
        WriteTimeout:      10 * time.Second,
        IdleTimeout:       120 * time.Second,
    }
    
    // Iniciar en goroutine
    go func() {
        log.Printf("Server starting on %s", server.Addr)
        if err := server.ListenAndServe(); err != http.ErrServerClosed {
            log.Fatalf("server: %v", err)
        }
    }()
    
    // Esperar senal
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    <-ctx.Done()
    
    // Graceful shutdown
    log.Println("Shutting down...")
    
    shutdownCtx, cancel := context.WithTimeout(context.Background(), 15*time.Second)
    defer cancel()
    
    // server.Shutdown:
    // 1. Cierra el listener (no mas conexiones nuevas)
    // 2. Espera que los requests en curso terminen
    // 3. Cierra conexiones idle
    // 4. Espera hasta el deadline del contexto
    if err := server.Shutdown(shutdownCtx); err != nil {
        log.Printf("Forced shutdown: %v", err)
        server.Close() // Forzar cierre inmediato
    }
    
    log.Println("Server stopped")
}
```

### 8.4 HTTPS (TLS)

```go
// Opcion 1: Archivos de certificado
server := &http.Server{
    Addr:    ":443",
    Handler: mux,
}
err := server.ListenAndServeTLS("cert.pem", "key.pem")

// Opcion 2: TLS config manual
server := &http.Server{
    Addr:    ":443",
    Handler: mux,
    TLSConfig: &tls.Config{
        MinVersion: tls.VersionTLS12,
        CurvePreferences: []tls.CurveID{
            tls.X25519,
            tls.CurveP256,
        },
        CipherSuites: []uint16{
            tls.TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
            tls.TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
            tls.TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305,
            tls.TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305,
        },
    },
}

// Opcion 3: Let's Encrypt automatico con autocert
import "golang.org/x/crypto/acme/autocert"

manager := &autocert.Manager{
    Prompt:     autocert.AcceptTOS,
    HostPolicy: autocert.HostWhitelist("example.com", "www.example.com"),
    Cache:      autocert.DirCache("/var/cache/certs"),
}

server := &http.Server{
    Addr:      ":443",
    Handler:   mux,
    TLSConfig: manager.TLSConfig(),
}

// Redirect HTTP → HTTPS
go http.ListenAndServe(":80", manager.HTTPHandler(nil))
server.ListenAndServeTLS("", "")
```

---

## 9. Servir archivos estaticos

### 9.1 http.FileServer

```go
// Servir un directorio completo
mux.Handle("GET /static/", http.StripPrefix("/static/",
    http.FileServer(http.Dir("./public"))))
// /static/css/style.css → ./public/css/style.css
// /static/js/app.js     → ./public/js/app.js
// /static/              → lista de archivos (directory listing)

// Sin directory listing (seguridad):
type noDirFileSystem struct {
    fs http.FileSystem
}

func (nfs noDirFileSystem) Open(path string) (http.File, error) {
    f, err := nfs.fs.Open(path)
    if err != nil {
        return nil, err
    }
    
    stat, err := f.Stat()
    if err != nil {
        f.Close()
        return nil, err
    }
    
    if stat.IsDir() {
        // Verificar si existe index.html
        index := filepath.Join(path, "index.html")
        if _, err := nfs.fs.Open(index); err != nil {
            f.Close()
            return nil, os.ErrNotExist
        }
    }
    
    return f, nil
}

mux.Handle("GET /static/", http.StripPrefix("/static/",
    http.FileServer(noDirFileSystem{http.Dir("./public")})))
```

### 9.2 Embed files (Go 1.16+)

```go
import "embed"

//go:embed static/*
var staticFiles embed.FS

func main() {
    mux := http.NewServeMux()
    
    // Servir archivos embebidos en el binario
    mux.Handle("GET /static/", http.FileServer(http.FS(staticFiles)))
    // o con StripPrefix:
    sub, _ := fs.Sub(staticFiles, "static")
    mux.Handle("GET /static/", http.StripPrefix("/static/",
        http.FileServer(http.FS(sub))))
    
    http.ListenAndServe(":8080", mux)
}

// Ventaja: el binario contiene todos los archivos.
// No necesitas copiar la carpeta static al servidor.
// go build produce un unico binario con todo incluido.
```

### 9.3 Servir un SPA (Single Page Application)

```go
// Las SPAs (React, Vue, Angular) necesitan:
// 1. Servir archivos estaticos normalmente
// 2. Para cualquier ruta no-archivo, servir index.html
//    (el router del frontend maneja las rutas)

//go:embed frontend/dist/*
var frontendFiles embed.FS

func spaHandler() http.Handler {
    dist, _ := fs.Sub(frontendFiles, "frontend/dist")
    fileServer := http.FileServer(http.FS(dist))
    
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        // Intentar servir el archivo
        path := r.URL.Path
        
        // Verificar si el archivo existe
        f, err := dist.Open(strings.TrimPrefix(path, "/"))
        if err == nil {
            f.Close()
            fileServer.ServeHTTP(w, r)
            return
        }
        
        // Archivo no existe → servir index.html (SPA fallback)
        r.URL.Path = "/"
        fileServer.ServeHTTP(w, r)
    })
}

func main() {
    mux := http.NewServeMux()
    
    // API routes
    mux.HandleFunc("GET /api/users", listUsers)
    mux.HandleFunc("POST /api/users", createUser)
    
    // SPA catch-all (debe ir al final)
    mux.Handle("GET /", spaHandler())
    
    http.ListenAndServe(":8080", mux)
}
```

---

## 10. Context en handlers

### 10.1 Request context

```go
func handler(w http.ResponseWriter, r *http.Request) {
    ctx := r.Context()
    
    // ctx se cancela cuando:
    // 1. El cliente cierra la conexion (disconnect)
    // 2. El server timeout expira
    // 3. El request se completa
    
    // Pasar a funciones que hacen I/O:
    result, err := queryDatabase(ctx, "SELECT ...")
    if err != nil {
        if ctx.Err() == context.Canceled {
            // El cliente se fue — no vale la pena responder
            return
        }
        writeError(w, 500, err.Error())
        return
    }
    
    writeJSON(w, 200, result)
}

func queryDatabase(ctx context.Context, query string) (interface{}, error) {
    // ctx.Done() se cierra cuando el cliente se va
    // Esto permite abortar queries lentas
    rows, err := db.QueryContext(ctx, query)
    if err != nil {
        return nil, err
    }
    // ...
}
```

### 10.2 Timeout per-request en handler

```go
func slowHandler(w http.ResponseWriter, r *http.Request) {
    // Timeout especifico para este handler
    ctx, cancel := context.WithTimeout(r.Context(), 5*time.Second)
    defer cancel()
    
    resultCh := make(chan string, 1)
    errCh := make(chan error, 1)
    
    go func() {
        result, err := expensiveOperation(ctx)
        if err != nil {
            errCh <- err
        } else {
            resultCh <- result
        }
    }()
    
    select {
    case result := <-resultCh:
        writeJSON(w, 200, map[string]string{"result": result})
    case err := <-errCh:
        writeError(w, 500, err.Error())
    case <-ctx.Done():
        writeError(w, http.StatusGatewayTimeout, "operation timed out")
    }
}
```

### 10.3 Pasar valores por context

```go
// Middleware que agrega valores al context:
func withUser(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        // Autenticar y obtener el usuario
        user, err := authenticate(r)
        if err != nil {
            writeError(w, 401, "unauthorized")
            return
        }
        
        // Agregar al context
        ctx := context.WithValue(r.Context(), userCtxKey, user)
        next.ServeHTTP(w, r.WithContext(ctx))
    })
}

// Extraer del context en el handler:
func profileHandler(w http.ResponseWriter, r *http.Request) {
    user := r.Context().Value(userCtxKey).(*User)
    writeJSON(w, 200, user)
}

// BUENAS PRACTICAS para context values:
// 1. Usar tipos unexported como key (no strings)
// 2. Proveer funciones getter tipadas
// 3. Solo para request-scoped data (no para pasar dependencias)

type ctxKey int

const (
    userCtxKey ctxKey = iota
    reqIDCtxKey
)

func UserFromContext(ctx context.Context) (*User, bool) {
    user, ok := ctx.Value(userCtxKey).(*User)
    return user, ok
}
```

---

## 11. Comparacion con C y Rust

```
┌──────────────────────────────────┬──────────────────────┬─────────────────────────┬──────────────────────────┐
│ Aspecto                          │ C                    │ Go                      │ Rust                     │
├──────────────────────────────────┼──────────────────────┼─────────────────────────┼──────────────────────────┤
│ Servidor HTTP stdlib             │ No (nginx, apache)   │ net/http (completo)     │ No (hyper = externo)     │
│ Framework popular                │ (no hay)             │ stdlib (o chi/gin)      │ axum, actix-web          │
│ Router con metodos               │ manual               │ ServeMux (Go 1.22+)    │ axum Router              │
│ Path parameters                  │ manual               │ {id} nativo             │ :id (axum) / {id} (actix)│
│ Wildcard                         │ manual               │ {path...}               │ /*path                   │
│ Middleware                       │ (no hay patron)      │ func(Handler)Handler    │ tower Layer/Service      │
│ Handler interface                │ function pointer     │ http.Handler interface  │ impl Service<Request>    │
│ Request body                     │ read(fd, buf, len)   │ r.Body (io.Reader)      │ body.collect().await      │
│ Response                         │ write(fd, buf, len)  │ w.Write(data)           │ (StatusCode, body)       │
│ JSON request                     │ cJSON + manual       │ json.Decoder(r.Body)    │ Json<T> extractor        │
│ JSON response                    │ cJSON + manual       │ json.Encoder(w)         │ Json(data)               │
│ Query params                     │ parse manual         │ r.URL.Query()           │ Query<T> extractor       │
│ Path params                      │ parse manual         │ r.PathValue("id")       │ Path<T> extractor        │
│ Form data                        │ parse manual         │ r.ParseForm()           │ Form<T> extractor        │
│ File upload                      │ parse manual         │ r.FormFile("f")         │ Multipart extractor      │
│ Status code                      │ manual en header     │ w.WriteHeader(code)     │ StatusCode::OK           │
│ Headers                          │ manual               │ w.Header().Set(k,v)     │ headers.insert(k,v)      │
│ Cookies                          │ manual               │ http.SetCookie(w, c)    │ Cookie/CookieJar         │
│ Static files                     │ open+read+write      │ http.FileServer         │ tower-http ServeDir      │
│ Embedded files                   │ (no hay)             │ embed.FS + http.FS      │ rust-embed/include_dir   │
│ TLS/HTTPS                        │ OpenSSL manual       │ ListenAndServeTLS       │ axum-server + rustls     │
│ Let's Encrypt                    │ certbot (externo)    │ autocert (stdlib-adj)   │ (no hay stdlib)          │
│ Graceful shutdown                │ signal + manual      │ server.Shutdown(ctx)    │ axum::serve().with_gshut │
│ Timeouts                         │ setsockopt           │ Server.ReadTimeout etc. │ tower::timeout           │
│ Goroutine per request            │ (fork/thread)        │ automatico              │ tokio task per request    │
│ Context/cancelacion              │ (no hay)             │ r.Context()             │ (no hay request context) │
│ Max body size                    │ manual               │ http.MaxBytesReader     │ ContentLengthLimit       │
│ Recovery de panics               │ (crash)              │ recover() middleware    │ (crash) / catch_unwind   │
│ Request ID                       │ manual               │ middleware + context    │ middleware + extensions  │
│ Rate limiting                    │ manual               │ x/time/rate + mw       │ tower::limit             │
│ Concurrencia                     │ fork/thread/epoll    │ goroutine (automatico)  │ tokio async (automatico) │
│ Compilacion                      │ gcc + libs           │ go build (todo incluido)│ cargo build              │
│ Lineas para server basico        │ ~200                 │ ~10                     │ ~15                      │
│ Lineas para API REST completa    │ ~2000+               │ ~200                    │ ~150 (con macros)        │
└──────────────────────────────────┴──────────────────────┴─────────────────────────┴──────────────────────────┘
```

### Insight: handler en tres lenguajes

```
// C: handler HTTP (pseudo-codigo con framework custom)
void handle_users(int client_fd, http_request *req) {
    if (strcmp(req->method, "GET") != 0) {
        send_response(client_fd, 405, "Method Not Allowed");
        return;
    }
    // Query database manually...
    // Build JSON string manually...
    char *json = build_json(users, count);
    send_response(client_fd, 200, json);
    free(json);
}

// Go: handler HTTP (stdlib)
func listUsers(w http.ResponseWriter, r *http.Request) {
    users, err := db.QueryContext(r.Context(), "SELECT * FROM users")
    if err != nil {
        writeError(w, 500, err.Error())
        return
    }
    writeJSON(w, 200, users)
}

// Rust (axum): handler HTTP
async fn list_users(
    State(db): State<Pool>,
) -> Result<Json<Vec<User>>, AppError> {
    let users = sqlx::query_as("SELECT * FROM users")
        .fetch_all(&db).await?;
    Ok(Json(users))
}
```

---

## 12. Programa completo: servidor HTTP con todas las piezas

```go
package main

import (
    "context"
    "crypto/rand"
    "encoding/json"
    "fmt"
    "log"
    "log/slog"
    "net"
    "net/http"
    "os"
    "os/signal"
    "runtime/debug"
    "strconv"
    "strings"
    "sync"
    "syscall"
    "time"
)

// --- Helpers ---

func writeJSON(w http.ResponseWriter, status int, data interface{}) {
    w.Header().Set("Content-Type", "application/json")
    w.WriteHeader(status)
    if data != nil {
        json.NewEncoder(w).Encode(data)
    }
}

func writeError(w http.ResponseWriter, status int, message string) {
    writeJSON(w, status, map[string]string{"error": message})
}

func generateID() string {
    b := make([]byte, 8)
    rand.Read(b)
    return fmt.Sprintf("%x", b)
}

// --- Domain ---

type Note struct {
    ID        string    `json:"id"`
    Title     string    `json:"title"`
    Content   string    `json:"content"`
    Tags      []string  `json:"tags,omitempty"`
    CreatedAt time.Time `json:"created_at"`
    UpdatedAt time.Time `json:"updated_at"`
}

type NoteStore struct {
    mu    sync.RWMutex
    notes map[string]*Note
}

func NewNoteStore() *NoteStore {
    return &NoteStore{notes: make(map[string]*Note)}
}

func (s *NoteStore) List(search string, tag string) []*Note {
    s.mu.RLock()
    defer s.mu.RUnlock()
    
    var result []*Note
    for _, n := range s.notes {
        if search != "" && !strings.Contains(strings.ToLower(n.Title), strings.ToLower(search)) &&
            !strings.Contains(strings.ToLower(n.Content), strings.ToLower(search)) {
            continue
        }
        if tag != "" {
            found := false
            for _, t := range n.Tags {
                if t == tag {
                    found = true
                    break
                }
            }
            if !found {
                continue
            }
        }
        result = append(result, n)
    }
    return result
}

func (s *NoteStore) Get(id string) (*Note, bool) {
    s.mu.RLock()
    defer s.mu.RUnlock()
    n, ok := s.notes[id]
    return n, ok
}

func (s *NoteStore) Create(title, content string, tags []string) *Note {
    s.mu.Lock()
    defer s.mu.Unlock()
    
    now := time.Now()
    note := &Note{
        ID:        generateID(),
        Title:     title,
        Content:   content,
        Tags:      tags,
        CreatedAt: now,
        UpdatedAt: now,
    }
    s.notes[note.ID] = note
    return note
}

func (s *NoteStore) Update(id string, title, content *string, tags *[]string) (*Note, bool) {
    s.mu.Lock()
    defer s.mu.Unlock()
    
    note, ok := s.notes[id]
    if !ok {
        return nil, false
    }
    
    if title != nil {
        note.Title = *title
    }
    if content != nil {
        note.Content = *content
    }
    if tags != nil {
        note.Tags = *tags
    }
    note.UpdatedAt = time.Now()
    
    return note, true
}

func (s *NoteStore) Delete(id string) bool {
    s.mu.Lock()
    defer s.mu.Unlock()
    _, ok := s.notes[id]
    delete(s.notes, id)
    return ok
}

func (s *NoteStore) Count() int {
    s.mu.RLock()
    defer s.mu.RUnlock()
    return len(s.notes)
}

// --- Middleware ---

type contextKey int

const requestIDKey contextKey = iota

type statusRecorder struct {
    http.ResponseWriter
    statusCode int
    written    bool
    bytes      int
}

func (r *statusRecorder) WriteHeader(code int) {
    if !r.written {
        r.statusCode = code
        r.written = true
    }
    r.ResponseWriter.WriteHeader(code)
}

func (r *statusRecorder) Write(b []byte) (int, error) {
    if !r.written {
        r.statusCode = 200
        r.written = true
    }
    n, err := r.ResponseWriter.Write(b)
    r.bytes += n
    return n, err
}

func recoveryMW(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if err := recover(); err != nil {
                slog.Error("panic recovered",
                    "error", err,
                    "stack", string(debug.Stack()),
                    "path", r.URL.Path,
                )
                writeError(w, 500, "internal server error")
            }
        }()
        next.ServeHTTP(w, r)
    })
}

func requestIDMW(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        id := r.Header.Get("X-Request-ID")
        if id == "" {
            id = generateID()
        }
        w.Header().Set("X-Request-ID", id)
        ctx := context.WithValue(r.Context(), requestIDKey, id)
        next.ServeHTTP(w, r.WithContext(ctx))
    })
}

func loggingMW(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        wrapped := &statusRecorder{ResponseWriter: w, statusCode: 200}
        
        next.ServeHTTP(wrapped, r)
        
        reqID, _ := r.Context().Value(requestIDKey).(string)
        slog.Info("request",
            "method", r.Method,
            "path", r.URL.Path,
            "status", wrapped.statusCode,
            "bytes", wrapped.bytes,
            "duration", time.Since(start).String(),
            "remote", r.RemoteAddr,
            "request_id", reqID,
        )
    })
}

func corsMW(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        w.Header().Set("Access-Control-Allow-Origin", "*")
        w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS")
        w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Request-ID")
        
        if r.Method == "OPTIONS" {
            w.WriteHeader(http.StatusNoContent)
            return
        }
        
        next.ServeHTTP(w, r)
    })
}

func chain(h http.Handler, mws ...func(http.Handler) http.Handler) http.Handler {
    for i := len(mws) - 1; i >= 0; i-- {
        h = mws[i](h)
    }
    return h
}

// --- Handlers ---

type NoteHandler struct {
    store *NoteStore
}

func (h *NoteHandler) List(w http.ResponseWriter, r *http.Request) {
    search := r.URL.Query().Get("search")
    tag := r.URL.Query().Get("tag")
    
    notes := h.store.List(search, tag)
    if notes == nil {
        notes = []*Note{} // retornar [] en vez de null
    }
    writeJSON(w, http.StatusOK, notes)
}

func (h *NoteHandler) Get(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    
    note, ok := h.store.Get(id)
    if !ok {
        writeError(w, http.StatusNotFound, "note not found")
        return
    }
    
    writeJSON(w, http.StatusOK, note)
}

func (h *NoteHandler) Create(w http.ResponseWriter, r *http.Request) {
    var input struct {
        Title   string   `json:"title"`
        Content string   `json:"content"`
        Tags    []string `json:"tags"`
    }
    
    r.Body = http.MaxBytesReader(w, r.Body, 1<<20)
    if err := json.NewDecoder(r.Body).Decode(&input); err != nil {
        writeError(w, http.StatusBadRequest, "invalid JSON: "+err.Error())
        return
    }
    
    if input.Title == "" {
        writeError(w, http.StatusBadRequest, "title is required")
        return
    }
    
    note := h.store.Create(input.Title, input.Content, input.Tags)
    
    w.Header().Set("Location", "/api/notes/"+note.ID)
    writeJSON(w, http.StatusCreated, note)
}

func (h *NoteHandler) Update(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    
    var input struct {
        Title   *string   `json:"title"`
        Content *string   `json:"content"`
        Tags    *[]string `json:"tags"`
    }
    
    r.Body = http.MaxBytesReader(w, r.Body, 1<<20)
    if err := json.NewDecoder(r.Body).Decode(&input); err != nil {
        writeError(w, http.StatusBadRequest, "invalid JSON: "+err.Error())
        return
    }
    
    note, ok := h.store.Update(id, input.Title, input.Content, input.Tags)
    if !ok {
        writeError(w, http.StatusNotFound, "note not found")
        return
    }
    
    writeJSON(w, http.StatusOK, note)
}

func (h *NoteHandler) Delete(w http.ResponseWriter, r *http.Request) {
    id := r.PathValue("id")
    
    if !h.store.Delete(id) {
        writeError(w, http.StatusNotFound, "note not found")
        return
    }
    
    w.WriteHeader(http.StatusNoContent)
}

func healthHandler(w http.ResponseWriter, r *http.Request) {
    writeJSON(w, http.StatusOK, map[string]string{"status": "ok"})
}

// --- Main ---

func main() {
    // Logger estructurado
    logger := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{
        Level: slog.LevelInfo,
    }))
    slog.SetDefault(logger)
    
    // Store
    store := NewNoteStore()
    
    // Datos de ejemplo
    store.Create("Aprender Go", "Estudiar net/http, ServeMux, middleware", []string{"go", "study"})
    store.Create("Comprar leche", "2 litros de leche descremada", []string{"shopping"})
    store.Create("Proyecto API", "Implementar CRUD de notas con Go", []string{"go", "project"})
    
    // Handlers
    nh := &NoteHandler{store: store}
    
    // Router
    mux := http.NewServeMux()
    
    // Health
    mux.HandleFunc("GET /health", healthHandler)
    
    // API
    mux.HandleFunc("GET /api/notes", nh.List)
    mux.HandleFunc("POST /api/notes", nh.Create)
    mux.HandleFunc("GET /api/notes/{id}", nh.Get)
    mux.HandleFunc("PATCH /api/notes/{id}", nh.Update)
    mux.HandleFunc("DELETE /api/notes/{id}", nh.Delete)
    
    // Stats
    mux.HandleFunc("GET /api/stats", func(w http.ResponseWriter, r *http.Request) {
        writeJSON(w, 200, map[string]interface{}{
            "notes_count": store.Count(),
            "uptime":      time.Since(startTime).String(),
        })
    })
    
    // Middleware chain
    handler := chain(mux,
        recoveryMW,
        requestIDMW,
        loggingMW,
        corsMW,
    )
    
    // Server
    addr := ":8080"
    if port := os.Getenv("PORT"); port != "" {
        addr = ":" + port
    }
    
    server := &http.Server{
        Addr:              addr,
        Handler:           handler,
        ReadTimeout:       5 * time.Second,
        ReadHeaderTimeout: 2 * time.Second,
        WriteTimeout:      10 * time.Second,
        IdleTimeout:       120 * time.Second,
        MaxHeaderBytes:    1 << 20,
    }
    
    // Start
    go func() {
        slog.Info("server starting", "addr", addr)
        if err := server.ListenAndServe(); err != http.ErrServerClosed {
            log.Fatalf("server: %v", err)
        }
    }()
    
    // Graceful shutdown
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    <-ctx.Done()
    
    slog.Info("shutting down...")
    shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()
    
    if err := server.Shutdown(shutdownCtx); err != nil {
        slog.Error("forced shutdown", "error", err)
    }
    slog.Info("server stopped")
}

var startTime = time.Now()
```

```
PROBAR EL PROGRAMA:

go run main.go

# Health check
curl localhost:8080/health
# {"status":"ok"}

# Listar notas
curl localhost:8080/api/notes | jq
# [{"id":"...","title":"Aprender Go",...}, ...]

# Crear nota
curl -X POST localhost:8080/api/notes \
  -H "Content-Type: application/json" \
  -d '{"title":"Nueva nota","content":"Contenido","tags":["test"]}' | jq
# {"id":"abc123","title":"Nueva nota",...}

# Obtener nota
curl localhost:8080/api/notes/abc123 | jq

# Actualizar nota (PATCH — solo campos a cambiar)
curl -X PATCH localhost:8080/api/notes/abc123 \
  -H "Content-Type: application/json" \
  -d '{"title":"Titulo actualizado"}' | jq

# Eliminar nota
curl -X DELETE localhost:8080/api/notes/abc123 -v
# HTTP/1.1 204 No Content

# Buscar
curl "localhost:8080/api/notes?search=go" | jq
curl "localhost:8080/api/notes?tag=study" | jq

# Stats
curl localhost:8080/api/stats | jq

# Request con ID custom
curl -H "X-Request-ID: my-trace-123" localhost:8080/api/notes -v
# X-Request-ID: my-trace-123

# Metodo no permitido
curl -X PUT localhost:8080/api/notes -v
# HTTP/1.1 405 Method Not Allowed

# Not found
curl localhost:8080/api/notes/inexistente -v
# {"error":"note not found"}

# CORS preflight
curl -X OPTIONS localhost:8080/api/notes -v
# HTTP/1.1 204 No Content
# Access-Control-Allow-Origin: *

# Logs del servidor (JSON estructurado):
# {"time":"...","level":"INFO","msg":"request","method":"GET",
#  "path":"/api/notes","status":200,"bytes":512,"duration":"1.2ms",
#  "remote":"127.0.0.1:45678","request_id":"a1b2c3d4"}

# Graceful shutdown:
# Ctrl+C
# {"level":"INFO","msg":"shutting down..."}
# {"level":"INFO","msg":"server stopped"}

CONCEPTOS DEMOSTRADOS:
  ServeMux (Go 1.22+):  metodos + path params (GET /api/notes/{id})
  http.Handler:         interface, HandlerFunc, struct handlers
  ResponseWriter:       writeJSON helper, status codes, headers
  Request:              PathValue, URL.Query, JSON body, MaxBytesReader
  Middleware chain:     recovery, request ID, logging, CORS
  statusRecorder:       capturar status code para logging
  http.Server:          timeouts de produccion, MaxHeaderBytes
  Graceful shutdown:    server.Shutdown(ctx) con deadline
  slog:                 logging estructurado JSON
  Context:              request ID en context, cancelacion
  CRUD completo:        List, Create, Get, Update (PATCH), Delete
  Validacion:           campos requeridos, body size limit
  Store thread-safe:    RWMutex para lectura/escritura concurrente
```

---

## 13. Ejercicios

### Ejercicio 1: URL shortener
Crea un acortador de URLs con la stdlib:
- `POST /api/shorten` con body `{"url": "https://example.com/long/path"}` → retorna `{"short_url": "http://localhost:8080/abc123", "code": "abc123"}`
- `GET /{code}` → redirect 301 al URL original
- `GET /api/stats/{code}` → retorna visitas totales, ultimo acceso, URL original, fecha de creacion
- `DELETE /api/urls/{code}` → eliminar URL (requiere header `X-Admin-Key`)
- Persistencia en memoria (mapa con mutex)
- Middleware de logging y recovery
- Graceful shutdown
- Generar codigos aleatorios de 6 caracteres (base62)
- Validar que el URL es valido (url.Parse, verificar scheme)

### Ejercicio 2: Servidor con auth y roles
Crea una API con autenticacion y autorizacion:
- `POST /auth/register` → crear usuario con username, password, role (user/admin)
- `POST /auth/login` → retorna token (string aleatorio almacenado en mapa)
- Middleware de auth que verifica el token Bearer y agrega usuario al context
- Middleware de roles: `requireRole("admin")` que verifica el rol del usuario en el context
- Rutas publicas: `/health`, `/auth/*`
- Rutas de user: `GET /api/profile` (solo con token)
- Rutas de admin: `GET /api/users` (listar todos), `DELETE /api/users/{id}` (solo admin)
- Passwords hasheados con `crypto/bcrypt` (o `golang.org/x/crypto/bcrypt`)
- Tokens con expiracion (configurable, default 24h)

### Ejercicio 3: File sharing server
Crea un servidor para compartir archivos:
- `POST /upload` con multipart/form-data → sube archivo, retorna `{"id": "abc123", "url": "/files/abc123/nombre.txt", "size": 12345, "expires_at": "..."}`
- `GET /files/{id}/{filename}` → descarga el archivo con Content-Disposition
- `GET /api/files` → listar archivos subidos (nombre, tamano, fecha, descargas)
- `DELETE /api/files/{id}` → eliminar archivo
- Limite de tamano: 50 MB por archivo (http.MaxBytesReader)
- Expiracion automatica: archivos se borran despues de 24h (goroutine de limpieza)
- Middleware de rate limiting: max 10 uploads por minuto por IP
- Servir con Content-Type correcto (detectar con http.DetectContentType)
- Soporte para Range requests (http.ServeContent)

### Ejercicio 4: Reverse proxy con middleware
Crea un reverse proxy HTTP con middleware configurable:
- Uso: `./proxy -target https://api.example.com -listen :8080`
- Reenviar todas las requests al target, retornar la response al cliente
- Middleware de logging: loggear cada request con method, path, status, duracion
- Middleware de cache: cachear GET responses por 60s (configurable), key = method+path+query
- Middleware de rate limit: max N requests/segundo por IP
- Middleware de rewrite: `-rewrite /api/v1=/v1` (reescribir paths)
- Headers: agregar `X-Forwarded-For`, `X-Forwarded-Host`, `X-Proxy-By`
- Health check: si el target no responde, retornar 502
- Graceful shutdown con server.Shutdown

---

> **Siguiente**: C10/S02 - HTTP, T03 - APIs REST — JSON request/response, routing, status codes, patrones de handler
