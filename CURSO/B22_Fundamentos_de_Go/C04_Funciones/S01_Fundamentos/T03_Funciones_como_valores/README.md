# Funciones como Valores — First-Class Functions

## 1. Introduccion

En Go, las funciones son **ciudadanos de primera clase** (first-class citizens). Esto significa que una funcion es un valor como cualquier otro — puedes asignarla a una variable, pasarla como argumento, retornarla desde otra funcion, almacenarla en un slice o map, y compararla con `nil`. Este mecanismo es la base de patrones fundamentales en SysAdmin/DevOps: middleware HTTP, plugins de validacion, dispatch tables, pipelines de transformacion, y strategy pattern para seleccionar comportamiento en runtime.

Go no tiene clases, herencia ni decoradores como Python — en su lugar usa composicion de funciones. Donde en OOP usarias el patron Strategy con una jerarquia de clases, en Go simplemente pasas una funcion.

```
┌──────────────────────────────────────────────────────────────┐
│           FUNCIONES COMO VALORES EN GO                        │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Una funcion es un valor con un tipo:                         │
│                                                              │
│  func(int, int) int          ← tipo de funcion               │
│  var op func(int, int) int   ← variable de tipo funcion      │
│  op = add                    ← asignar funcion a variable    │
│  result := op(2, 3)          ← llamar via variable           │
│                                                              │
│  Que puedes hacer con una funcion:                            │
│  ├─ Asignar a variable:      var f func()                    │
│  ├─ Pasar como argumento:    process(data, transformFn)      │
│  ├─ Retornar desde funcion:  return func() { ... }           │
│  ├─ Almacenar en slice:      []func(){fn1, fn2, fn3}         │
│  ├─ Almacenar en map:        map[string]func(){...}          │
│  ├─ Comparar con nil:        if f != nil { f() }             │
│  └─ Definir inline (literal):func(x int) { ... }            │
│                                                              │
│  Que NO puedes hacer:                                        │
│  ├─ Comparar dos funciones: f1 == f2 (solo nil)              │
│  └─ Usar como key de map: map[func()]... (no comparable)     │
│                                                              │
│  Relacion con closures (T01 siguiente):                      │
│  Una funcion literal que captura variables de su entorno      │
│  es un closure — este topico cubre la mecanica, el siguiente  │
│  cubre la captura de variables                                │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Tipos de funcion

### Toda funcion tiene un tipo

```go
func add(a, b int) int { return a + b }
func mul(a, b int) int { return a * b }

// El tipo de ambas es: func(int, int) int
// Puedes asignar cualquiera a una variable de ese tipo

var op func(int, int) int

op = add
fmt.Println(op(3, 4)) // 7

op = mul
fmt.Println(op(3, 4)) // 12
```

### Definir un tipo con nombre (type alias)

```go
// Definir un tipo nombrado para funciones hace el codigo mas legible
type BinaryOp func(int, int) int

func apply(a, b int, op BinaryOp) int {
    return op(a, b)
}

// Mucho mas claro que:
// func apply(a, b int, op func(int, int) int) int

result := apply(10, 3, add) // 13
result = apply(10, 3, mul)  // 30
```

### Tipos de funcion comunes en SysAdmin

```go
// Handler: recibe un request, no retorna nada (side effect)
type Handler func(event Event)

// Predicate: evalua una condicion
type Predicate func(host string) bool

// Transformer: transforma un valor
type Transformer func(input string) string

// Validator: valida y retorna error
type Validator func(config Config) error

// Middleware: envuelve un handler
type Middleware func(next http.Handler) http.Handler

// CheckFunc: health check con resultado
type CheckFunc func(ctx context.Context, target string) (bool, error)

// RetryFunc: operacion que puede reintentarse
type RetryFunc func() error
```

---

## 3. Asignacion a variables

### Variable de tipo funcion

```go
// Declaracion con var (zero value es nil)
var greet func(string) string
fmt.Println(greet == nil) // true

// Asignar una funcion existente
func hello(name string) string {
    return "Hello, " + name
}
greet = hello
fmt.Println(greet("world")) // "Hello, world"

// Asignar una funcion literal (anonima)
greet = func(name string) string {
    return "Hi, " + name + "!"
}
fmt.Println(greet("admin")) // "Hi, admin!"
```

### Short declaration con funcion literal

```go
// := con funcion literal — el patron mas comun
double := func(n int) int { return n * 2 }
fmt.Println(double(21)) // 42

// Multi-linea
sanitize := func(input string) string {
    s := strings.TrimSpace(input)
    s = strings.ToLower(s)
    s = strings.ReplaceAll(s, " ", "-")
    return s
}
fmt.Println(sanitize("  Hello World  ")) // "hello-world"
```

### Zero value: nil

```go
var callback func()

// Llamar a nil panic
// callback() // panic: runtime error: invalid memory address or nil pointer dereference

// Siempre verificar antes de llamar
if callback != nil {
    callback()
}

// Patron comun: callback opcional con default
func process(data []byte, onComplete func()) {
    // ... procesar data ...
    if onComplete != nil {
        onComplete()
    }
}

// Llamar sin callback:
process(data, nil) // ok, no panic
```

### Comparacion: solo con nil

```go
var f1 func()
var f2 func()

fmt.Println(f1 == nil) // true — valido
fmt.Println(f2 == nil) // true — valido

// ✗ ERROR de compilacion:
// fmt.Println(f1 == f2) // invalid operation: cannot compare f1 == f2

// Razon: las funciones no son comparable en Go
// No puedes usarlas como key de map tampoco:
// m := map[func()]string{} // invalid map key type func()
```

---

## 4. Funciones como argumentos

### Patron basico: pasar comportamiento

```go
func applyToAll(numbers []int, transform func(int) int) []int {
    result := make([]int, len(numbers))
    for i, n := range numbers {
        result[i] = transform(n)
    }
    return result
}

nums := []int{1, 2, 3, 4, 5}

doubled := applyToAll(nums, func(n int) int { return n * 2 })
// [2, 4, 6, 8, 10]

squared := applyToAll(nums, func(n int) int { return n * n })
// [1, 4, 9, 16, 25]
```

### Patron SysAdmin: filtrado con predicado

```go
type Service struct {
    Name   string
    Status string
    CPU    float64
    Memory int64 // MB
}

func filterServices(services []Service, pred func(Service) bool) []Service {
    var result []Service
    for _, svc := range services {
        if pred(svc) {
            result = append(result, svc)
        }
    }
    return result
}

services := []Service{
    {"nginx", "running", 2.3, 128},
    {"postgres", "running", 45.2, 2048},
    {"redis", "stopped", 0, 0},
    {"prometheus", "running", 12.1, 512},
    {"old-app", "running", 89.5, 4096},
}

// Filtrar servicios detenidos
stopped := filterServices(services, func(s Service) bool {
    return s.Status == "stopped"
})

// Filtrar servicios con alto uso de CPU
highCPU := filterServices(services, func(s Service) bool {
    return s.CPU > 50.0
})

// Filtrar servicios con mucha memoria
highMem := filterServices(services, func(s Service) bool {
    return s.Memory > 1024
})

// Combinar predicados (ver seccion 8)
```

### La stdlib usa este patron extensivamente

```go
// sort.Slice: pasas la funcion de comparacion
sort.Slice(services, func(i, j int) bool {
    return services[i].CPU > services[j].CPU // orden descendente por CPU
})

// strings.Map: transforma cada rune
clean := strings.Map(func(r rune) rune {
    if r < 32 || r > 126 {
        return -1 // eliminar non-printable
    }
    return r
}, rawInput)

// filepath.WalkDir: callback por cada archivo
filepath.WalkDir("/var/log", func(path string, d fs.DirEntry, err error) error {
    if err != nil {
        return err
    }
    if !d.IsDir() && strings.HasSuffix(path, ".log") {
        fmt.Println(path)
    }
    return nil
})

// http.HandleFunc: registrar handler
http.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
    w.WriteHeader(200)
    fmt.Fprintf(w, `{"status":"ok"}`)
})
```

---

## 5. Retornar funciones

### Patron basico: factory de funciones

```go
// Retorna una funcion configurada
func makeGreeter(greeting string) func(string) string {
    return func(name string) string {
        return greeting + ", " + name + "!"
    }
}

helloFn := makeGreeter("Hello")
holaFn := makeGreeter("Hola")

fmt.Println(helloFn("admin")) // "Hello, admin!"
fmt.Println(holaFn("admin"))  // "Hola, admin!"
```

### Factory de validadores

```go
// Genera validadores parametrizados
func minLength(n int) func(string) error {
    return func(s string) error {
        if len(s) < n {
            return fmt.Errorf("must be at least %d characters, got %d", n, len(s))
        }
        return nil
    }
}

func maxLength(n int) func(string) error {
    return func(s string) error {
        if len(s) > n {
            return fmt.Errorf("must be at most %d characters, got %d", n, len(s))
        }
        return nil
    }
}

func matchesPattern(pattern string) func(string) error {
    re := regexp.MustCompile(pattern) // Compila una sola vez
    return func(s string) error {
        if !re.MatchString(s) {
            return fmt.Errorf("must match pattern %s", pattern)
        }
        return nil
    }
}

// Uso:
validateUsername := minLength(3)
validatePassword := minLength(12)
validateHostname := matchesPattern(`^[a-z][a-z0-9-]*$`)

if err := validateHostname("web-01"); err != nil {
    fmt.Println(err)
}
```

### Factory de retry logic

```go
func withRetry(maxAttempts int, delay time.Duration) func(func() error) error {
    return func(operation func() error) error {
        var lastErr error
        for attempt := 1; attempt <= maxAttempts; attempt++ {
            lastErr = operation()
            if lastErr == nil {
                return nil
            }
            if attempt < maxAttempts {
                log.Printf("attempt %d/%d failed: %v, retrying in %v",
                    attempt, maxAttempts, lastErr, delay)
                time.Sleep(delay)
                delay *= 2 // exponential backoff
            }
        }
        return fmt.Errorf("all %d attempts failed, last: %w", maxAttempts, lastErr)
    }
}

// Crear el retry handler una vez
retry := withRetry(3, time.Second)

// Usarlo multiples veces
err := retry(func() error {
    return connectToDatabase(dsn)
})

err = retry(func() error {
    return pingService("https://api.example.com/health")
})
```

---

## 6. Funciones literales (anonimas)

### Sintaxis

```go
// Funcion literal = funcion sin nombre, definida inline
// Se pueden ejecutar inmediatamente (IIFE) o asignar a variable

// Asignar a variable
double := func(n int) int { return n * 2 }

// Ejecutar inmediatamente (Immediately Invoked Function Expression)
result := func(a, b int) int {
    return a + b
}(10, 20) // ← se llama aqui mismo
fmt.Println(result) // 30

// Pasar como argumento inline
sort.Slice(items, func(i, j int) bool {
    return items[i].Priority > items[j].Priority
})
```

### IIFE para scope limitado

```go
func main() {
    // Encapsular inicializacion compleja
    config := func() Config {
        data, err := os.ReadFile("/etc/myapp/config.json")
        if err != nil {
            log.Fatalf("Cannot read config: %v", err)
        }
        var cfg Config
        if err := json.Unmarshal(data, &cfg); err != nil {
            log.Fatalf("Cannot parse config: %v", err)
        }
        if cfg.Port == 0 {
            cfg.Port = 8080
        }
        return cfg
    }() // ← ejecutada inmediatamente

    // config ya esta lista, data y err no contaminan el scope
    fmt.Printf("Starting on port %d\n", config.Port)
}
```

### IIFE con goroutines

```go
// Muy comun: lanzar goroutine con funcion literal
for _, host := range hosts {
    go func(h string) {
        if err := healthCheck(h); err != nil {
            log.Printf("FAIL %s: %v", h, err)
        }
    }(host) // ← pasar host como argumento
}

// A partir de Go 1.22, la variable del loop tiene scope por iteracion
// Pero el patron de pasar como argumento sigue siendo valido y explicito
```

---

## 7. Almacenar funciones en colecciones

### Slice de funciones

```go
// Pipeline de transformaciones
type Transform func(string) string

func buildPipeline(transforms ...Transform) Transform {
    return func(input string) string {
        result := input
        for _, t := range transforms {
            result = t(result)
        }
        return result
    }
}

normalize := buildPipeline(
    strings.TrimSpace,
    strings.ToLower,
    func(s string) string { return strings.ReplaceAll(s, " ", "-") },
    func(s string) string { return strings.ReplaceAll(s, "_", "-") },
)

fmt.Println(normalize("  Hello_World  ")) // "hello-world"
```

### Map de funciones (dispatch table)

```go
// Patron clasico SysAdmin: dispatch table por comando
type CommandFunc func(args []string) error

var commands = map[string]CommandFunc{
    "status": func(args []string) error {
        return showStatus(args)
    },
    "start": func(args []string) error {
        if len(args) == 0 {
            return fmt.Errorf("start requires service name")
        }
        return startService(args[0])
    },
    "stop": func(args []string) error {
        if len(args) == 0 {
            return fmt.Errorf("stop requires service name")
        }
        return stopService(args[0])
    },
    "restart": func(args []string) error {
        if len(args) == 0 {
            return fmt.Errorf("restart requires service name")
        }
        if err := stopService(args[0]); err != nil {
            return err
        }
        return startService(args[0])
    },
    "logs": func(args []string) error {
        return tailLogs(args)
    },
}

func dispatch(cmd string, args []string) error {
    fn, ok := commands[cmd]
    if !ok {
        return fmt.Errorf("unknown command: %s\nAvailable: %s",
            cmd, strings.Join(sortedKeys(commands), ", "))
    }
    return fn(args)
}
```

### Map de funciones con funciones nombradas

```go
// Alternativa: registrar funciones nombradas (mas testeable)
func cmdStatus(args []string) error {
    // ...
    return nil
}

func cmdStart(args []string) error {
    // ...
    return nil
}

var commands = map[string]CommandFunc{
    "status":  cmdStatus,
    "start":   cmdStart,
    // No necesitas wrapping — la firma ya coincide
}
```

---

## 8. Composicion de funciones

### Componer dos funciones

```go
// compose(f, g) retorna una funcion que aplica g y luego f
// (matematicamente: f ∘ g, "f after g")
func compose(f, g func(string) string) func(string) string {
    return func(s string) string {
        return f(g(s))
    }
}

trim := strings.TrimSpace
lower := strings.ToLower

trimAndLower := compose(lower, trim) // lower(trim(s))
fmt.Println(trimAndLower("  HELLO  ")) // "hello"
```

### Pipeline (composicion en orden natural)

```go
// pipe ejecuta funciones en orden de izquierda a derecha
// (mas intuitivo que compose para la mayoria de programadores)
func pipe(fns ...func(string) string) func(string) string {
    return func(s string) string {
        result := s
        for _, fn := range fns {
            result = fn(result)
        }
        return result
    }
}

sanitizeHostname := pipe(
    strings.TrimSpace,
    strings.ToLower,
    func(s string) string { return strings.ReplaceAll(s, " ", "-") },
    func(s string) string {
        re := regexp.MustCompile(`[^a-z0-9-]`)
        return re.ReplaceAllString(s, "")
    },
)

fmt.Println(sanitizeHostname("  My Server #1  ")) // "my-server-1"
```

### Combinar predicados

```go
type Predicate[T any] func(T) bool

func and[T any](predicates ...Predicate[T]) Predicate[T] {
    return func(item T) bool {
        for _, p := range predicates {
            if !p(item) {
                return false
            }
        }
        return true
    }
}

func or[T any](predicates ...Predicate[T]) Predicate[T] {
    return func(item T) bool {
        for _, p := range predicates {
            if p(item) {
                return true
            }
        }
        return false
    }
}

func not[T any](p Predicate[T]) Predicate[T] {
    return func(item T) bool {
        return !p(item)
    }
}

// Uso:
isRunning := func(s Service) bool { return s.Status == "running" }
highCPU := func(s Service) bool { return s.CPU > 80 }
highMem := func(s Service) bool { return s.Memory > 2048 }

// Servicios running con CPU o memoria alta
critical := and(isRunning, or(Predicate[Service](highCPU), Predicate[Service](highMem)))

for _, svc := range services {
    if critical(svc) {
        fmt.Printf("ALERT: %s (cpu=%.1f%%, mem=%dMB)\n", svc.Name, svc.CPU, svc.Memory)
    }
}
```

---

## 9. Funciones como campos de struct

### Struct con callbacks

```go
type Watcher struct {
    Path     string
    Interval time.Duration
    OnChange func(path string, info os.FileInfo)
    OnError  func(path string, err error)
}

func (w *Watcher) Start(ctx context.Context) {
    var lastMod time.Time

    ticker := time.NewTicker(w.Interval)
    defer ticker.Stop()

    for {
        select {
        case <-ctx.Done():
            return
        case <-ticker.C:
            info, err := os.Stat(w.Path)
            if err != nil {
                if w.OnError != nil {
                    w.OnError(w.Path, err)
                }
                continue
            }
            if info.ModTime().After(lastMod) {
                lastMod = info.ModTime()
                if w.OnChange != nil {
                    w.OnChange(w.Path, info)
                }
            }
        }
    }
}

// Uso: cada instancia con diferentes callbacks
watcher := &Watcher{
    Path:     "/etc/nginx/nginx.conf",
    Interval: 5 * time.Second,
    OnChange: func(path string, info os.FileInfo) {
        log.Printf("Config changed: %s (size=%d)", path, info.Size())
        exec.Command("nginx", "-s", "reload").Run()
    },
    OnError: func(path string, err error) {
        log.Printf("Cannot stat %s: %v", path, err)
    },
}
```

### Struct con estrategias intercambiables

```go
type Deployer struct {
    Name     string
    Build    func(ctx context.Context) error
    Test     func(ctx context.Context) error
    Deploy   func(ctx context.Context, target string) error
    Rollback func(ctx context.Context, target string) error
    Notify   func(msg string)
}

func (d *Deployer) Execute(ctx context.Context, target string) error {
    notify := d.Notify
    if notify == nil {
        notify = func(string) {} // noop default
    }

    notify(fmt.Sprintf("Starting deploy of %s to %s", d.Name, target))

    if d.Build != nil {
        if err := d.Build(ctx); err != nil {
            return fmt.Errorf("build failed: %w", err)
        }
        notify("Build completed")
    }

    if d.Test != nil {
        if err := d.Test(ctx); err != nil {
            return fmt.Errorf("tests failed: %w", err)
        }
        notify("Tests passed")
    }

    if err := d.Deploy(ctx, target); err != nil {
        notify(fmt.Sprintf("Deploy failed, rolling back: %v", err))
        if d.Rollback != nil {
            d.Rollback(ctx, target)
        }
        return fmt.Errorf("deploy failed: %w", err)
    }

    notify("Deploy completed successfully")
    return nil
}
```

### Funciones como campos vs interfaces

```go
// Con funcion como campo:
type Logger struct {
    Output func(msg string)
}

// Con interfaz:
type Writer interface {
    Write(msg string)
}

// ¿Cuando usar cada uno?
//
// Funcion como campo:
// ├─ Cuando solo necesitas UNA operacion
// ├─ Cuando quieres inline la implementacion
// ├─ Cuando la implementacion es un one-liner
// └─ Cuando no necesitas estado adicional
//
// Interfaz:
// ├─ Cuando agrupas MULTIPLES operaciones relacionadas
// ├─ Cuando necesitas mock/test con multiples metodos
// ├─ Cuando la implementacion tiene estado propio
// └─ Cuando quieres polimorfismo con type assertion
```

---

## 10. Patron Middleware (HTTP)

### Anatomia de un middleware en Go

```go
// Un middleware es una funcion que recibe un handler y retorna un handler
// func(http.Handler) http.Handler

// http.Handler es una interfaz con un metodo:
// type Handler interface {
//     ServeHTTP(ResponseWriter, *Request)
// }

// http.HandlerFunc es un adaptador:
// type HandlerFunc func(ResponseWriter, *Request)
// func (f HandlerFunc) ServeHTTP(w ResponseWriter, r *Request) { f(w, r) }
```

### Middleware de logging

```go
func loggingMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        start := time.Now()
        
        // Wrapper para capturar status code
        wrapped := &statusRecorder{ResponseWriter: w, status: 200}
        
        next.ServeHTTP(wrapped, r)
        
        log.Printf("%s %s %d %v %s",
            r.Method, r.URL.Path, wrapped.status,
            time.Since(start), r.RemoteAddr)
    })
}

type statusRecorder struct {
    http.ResponseWriter
    status int
}

func (sr *statusRecorder) WriteHeader(code int) {
    sr.status = code
    sr.ResponseWriter.WriteHeader(code)
}
```

### Middleware de autenticacion

```go
func authMiddleware(validTokens map[string]bool) func(http.Handler) http.Handler {
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            token := r.Header.Get("Authorization")
            if token == "" {
                http.Error(w, "missing auth token", http.StatusUnauthorized)
                return
            }
            
            token = strings.TrimPrefix(token, "Bearer ")
            if !validTokens[token] {
                http.Error(w, "invalid token", http.StatusForbidden)
                return
            }
            
            next.ServeHTTP(w, r) // Continua la cadena
        })
    }
}
```

### Middleware de rate limiting

```go
func rateLimitMiddleware(rps int) func(http.Handler) http.Handler {
    limiter := rate.NewLimiter(rate.Limit(rps), rps*2)
    
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            if !limiter.Allow() {
                http.Error(w, "rate limit exceeded", http.StatusTooManyRequests)
                return
            }
            next.ServeHTTP(w, r)
        })
    }
}
```

### Encadenar middlewares

```go
// Patron: chain aplica middlewares en orden
func chain(handler http.Handler, middlewares ...func(http.Handler) http.Handler) http.Handler {
    // Aplicar en orden inverso para que el primero en la lista
    // sea el mas externo (ejecuta primero)
    for i := len(middlewares) - 1; i >= 0; i-- {
        handler = middlewares[i](handler)
    }
    return handler
}

// Uso:
tokens := map[string]bool{"secret-token-123": true}

mux := http.NewServeMux()
mux.HandleFunc("/api/data", handleData)

// La request pasa por: logging → rateLimit → auth → handler
server := chain(mux,
    loggingMiddleware,
    rateLimitMiddleware(100),
    authMiddleware(tokens),
)

http.ListenAndServe(":8080", server)
```

```
┌──────────────────────────────────────────────────────────────┐
│  CADENA DE MIDDLEWARE                                        │
│                                                              │
│  Request → [Logging] → [RateLimit] → [Auth] → [Handler]     │
│                                                              │
│  Cada middleware:                                            │
│  1. Puede modificar la request                               │
│  2. Puede decidir NO llamar a next (cortar la cadena)        │
│  3. Puede modificar la response despues de next              │
│  4. Llama a next.ServeHTTP(w, r) para continuar              │
│                                                              │
│  Si Auth falla → retorna 401/403, NO llama a next            │
│  Si RateLimit excede → retorna 429, NO llama a next          │
│  Logging siempre llama a next y registra despues             │
└──────────────────────────────────────────────────────────────┘
```

---

## 11. Patron Strategy

### Seleccionar algoritmo en runtime

```go
type CompressionStrategy func(data []byte) ([]byte, error)

func compressGzip(data []byte) ([]byte, error) {
    var buf bytes.Buffer
    w := gzip.NewWriter(&buf)
    _, err := w.Write(data)
    if err != nil {
        return nil, err
    }
    if err := w.Close(); err != nil {
        return nil, err
    }
    return buf.Bytes(), nil
}

func compressZlib(data []byte) ([]byte, error) {
    var buf bytes.Buffer
    w := zlib.NewWriter(&buf)
    _, err := w.Write(data)
    if err != nil {
        return nil, err
    }
    if err := w.Close(); err != nil {
        return nil, err
    }
    return buf.Bytes(), nil
}

func noCompress(data []byte) ([]byte, error) {
    return data, nil
}

// Seleccion en runtime
func getCompressor(name string) CompressionStrategy {
    switch name {
    case "gzip":
        return compressGzip
    case "zlib":
        return compressZlib
    case "none":
        return noCompress
    default:
        return compressGzip // default
    }
}

// Uso:
compress := getCompressor(config.Compression)
data, err := compress(payload)
```

### Strategy para deployment targets

```go
type DeployStrategy func(artifact string, target string) error

func deploySSH(artifact, target string) error {
    return exec.Command("scp", artifact, target+":~/deploy/").Run()
}

func deployDocker(artifact, target string) error {
    cmd := exec.Command("docker", "push", target+"/"+artifact)
    return cmd.Run()
}

func deployK8s(artifact, target string) error {
    cmd := exec.Command("kubectl", "set", "image",
        "deployment/app", "app="+artifact,
        "--namespace="+target)
    return cmd.Run()
}

var strategies = map[string]DeployStrategy{
    "ssh":    deploySSH,
    "docker": deployDocker,
    "k8s":    deployK8s,
}

func deploy(method, artifact, target string) error {
    strategy, ok := strategies[method]
    if !ok {
        return fmt.Errorf("unknown deploy method: %s", method)
    }
    return strategy(artifact, target)
}
```

---

## 12. Patron Decorator / Wrapper

### Decorar funciones existentes

```go
// Tipo base: cualquier operacion que puede fallar
type Operation func() error

// Decorator: agregar logging
func withLogging(name string, op Operation) Operation {
    return func() error {
        log.Printf("Starting: %s", name)
        start := time.Now()
        err := op()
        if err != nil {
            log.Printf("Failed: %s (%v) after %v", name, err, time.Since(start))
        } else {
            log.Printf("Completed: %s in %v", name, time.Since(start))
        }
        return err
    }
}

// Decorator: agregar timeout
func withTimeout(d time.Duration, op Operation) Operation {
    return func() error {
        done := make(chan error, 1)
        go func() {
            done <- op()
        }()
        select {
        case err := <-done:
            return err
        case <-time.After(d):
            return fmt.Errorf("operation timed out after %v", d)
        }
    }
}

// Decorator: agregar retry
func withRetryOp(attempts int, delay time.Duration, op Operation) Operation {
    return func() error {
        var lastErr error
        for i := 0; i < attempts; i++ {
            lastErr = op()
            if lastErr == nil {
                return nil
            }
            if i < attempts-1 {
                time.Sleep(delay)
                delay *= 2
            }
        }
        return fmt.Errorf("failed after %d attempts: %w", attempts, lastErr)
    }
}

// Componer decorators:
backupDB := func() error {
    return exec.Command("pg_dump", "-Fc", "mydb", "-f", "/backup/mydb.dump").Run()
}

robustBackup := withLogging("db-backup",
    withTimeout(5*time.Minute,
        withRetryOp(3, 10*time.Second,
            backupDB,
        ),
    ),
)

if err := robustBackup(); err != nil {
    alert(err)
}
```

```
┌──────────────────────────────────────────────────────────────┐
│  DECORATORS APILADOS                                         │
│                                                              │
│  robustBackup()                                              │
│    └─ withLogging                                            │
│         ├─ log "Starting: db-backup"                         │
│         └─ withTimeout (5 min)                               │
│              └─ withRetry (3 intentos)                       │
│                   └─ backupDB (pg_dump)                      │
│                        ├─ intento 1: falla → sleep 10s       │
│                        ├─ intento 2: falla → sleep 20s       │
│                        └─ intento 3: exito!                  │
│              ← timeout no alcanzado                          │
│         ← log "Completed: db-backup in 35.2s"               │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 13. Funciones como valores en testing

### Table-driven tests con funciones

```go
func TestSanitize(t *testing.T) {
    tests := []struct {
        name      string
        input     string
        transform func(string) string
        expected  string
    }{
        {
            name:      "trim and lower",
            input:     "  HELLO  ",
            transform: func(s string) string { return strings.ToLower(strings.TrimSpace(s)) },
            expected:  "hello",
        },
        {
            name:      "replace spaces",
            input:     "hello world",
            transform: func(s string) string { return strings.ReplaceAll(s, " ", "-") },
            expected:  "hello-world",
        },
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got := tt.transform(tt.input)
            if got != tt.expected {
                t.Errorf("got %q, want %q", got, tt.expected)
            }
        })
    }
}
```

### Dependency injection via funciones

```go
// Produccion: llama realmente al sistema
type ServiceChecker struct {
    RunCommand func(name string, args ...string) (string, error)
}

func NewServiceChecker() *ServiceChecker {
    return &ServiceChecker{
        RunCommand: func(name string, args ...string) (string, error) {
            out, err := exec.Command(name, args...).Output()
            return string(out), err
        },
    }
}

func (sc *ServiceChecker) IsActive(service string) (bool, error) {
    output, err := sc.RunCommand("systemctl", "is-active", service)
    if err != nil {
        return false, err
    }
    return strings.TrimSpace(output) == "active", nil
}

// Test: inyectar funcion mock
func TestIsActive(t *testing.T) {
    checker := &ServiceChecker{
        RunCommand: func(name string, args ...string) (string, error) {
            if args[0] == "is-active" && args[1] == "nginx" {
                return "active\n", nil
            }
            return "inactive\n", nil
        },
    }

    active, err := checker.IsActive("nginx")
    if err != nil {
        t.Fatal(err)
    }
    if !active {
        t.Error("expected nginx to be active")
    }
}
```

---

## 14. Funciones de orden superior genericas (Go 1.18+)

### Map, Filter, Reduce

```go
// Map: transforma cada elemento
func Map[T, U any](items []T, fn func(T) U) []U {
    result := make([]U, len(items))
    for i, item := range items {
        result[i] = fn(item)
    }
    return result
}

// Filter: selecciona elementos
func Filter[T any](items []T, fn func(T) bool) []T {
    var result []T
    for _, item := range items {
        if fn(item) {
            result = append(result, item)
        }
    }
    return result
}

// Reduce: acumula a un solo valor
func Reduce[T, U any](items []T, initial U, fn func(U, T) U) U {
    acc := initial
    for _, item := range items {
        acc = fn(acc, item)
    }
    return acc
}
```

### Uso en contexto SysAdmin

```go
type Host struct {
    Name   string
    IP     string
    Region string
    Load   float64
}

hosts := []Host{
    {"web01", "10.0.1.1", "us-east", 0.82},
    {"web02", "10.0.1.2", "us-east", 0.45},
    {"web03", "10.0.2.1", "eu-west", 0.91},
    {"db01", "10.0.1.10", "us-east", 0.33},
    {"db02", "10.0.2.10", "eu-west", 0.67},
}

// Extraer solo los nombres
names := Map(hosts, func(h Host) string { return h.Name })
// ["web01", "web02", "web03", "db01", "db02"]

// Filtrar hosts sobrecargados
overloaded := Filter(hosts, func(h Host) bool { return h.Load > 0.80 })
// [web01, web03]

// Calcular carga promedio
totalLoad := Reduce(hosts, 0.0, func(acc float64, h Host) float64 {
    return acc + h.Load
})
avgLoad := totalLoad / float64(len(hosts))
// 0.636

// Agrupar por region
byRegion := Reduce(hosts, map[string][]Host{}, func(acc map[string][]Host, h Host) map[string][]Host {
    acc[h.Region] = append(acc[h.Region], h)
    return acc
})
```

### ForEach y Apply

```go
// ForEach: ejecutar side-effect por cada elemento
func ForEach[T any](items []T, fn func(T)) {
    for _, item := range items {
        fn(item)
    }
}

// Notificar a todos los hosts sobrecargados
ForEach(overloaded, func(h Host) {
    log.Printf("ALERT: %s load=%.2f", h.Name, h.Load)
    sendAlert(h.Name, h.Load)
})
```

---

## 15. Dispatch tables avanzadas

### CLI tool con subcomandos

```go
type SubCommand struct {
    Name        string
    Description string
    Usage       string
    Run         func(args []string) error // La funcion es un campo
}

var subcommands = []SubCommand{
    {
        Name:        "check",
        Description: "Run health checks on targets",
        Usage:       "check <host1> [host2] ...",
        Run: func(args []string) error {
            if len(args) == 0 {
                return fmt.Errorf("no targets specified")
            }
            return runHealthChecks(args)
        },
    },
    {
        Name:        "deploy",
        Description: "Deploy artifact to target",
        Usage:       "deploy <artifact> <target>",
        Run: func(args []string) error {
            if len(args) < 2 {
                return fmt.Errorf("usage: deploy <artifact> <target>")
            }
            return deployArtifact(args[0], args[1])
        },
    },
    {
        Name:        "config",
        Description: "Show current configuration",
        Usage:       "config [key]",
        Run: func(args []string) error {
            return showConfig(args)
        },
    },
}

func main() {
    if len(os.Args) < 2 {
        printUsage(subcommands)
        os.Exit(1)
    }

    cmdName := os.Args[1]
    for _, cmd := range subcommands {
        if cmd.Name == cmdName {
            if err := cmd.Run(os.Args[2:]); err != nil {
                fmt.Fprintf(os.Stderr, "Error: %v\n", err)
                os.Exit(1)
            }
            return
        }
    }

    fmt.Fprintf(os.Stderr, "Unknown command: %s\n", cmdName)
    printUsage(subcommands)
    os.Exit(1)
}

func printUsage(cmds []SubCommand) {
    fmt.Println("Available commands:")
    for _, cmd := range cmds {
        fmt.Printf("  %-12s %s\n", cmd.Name, cmd.Description)
    }
}
```

### Signal handler con dispatch

```go
func setupSignalHandlers(handlers map[os.Signal]func()) {
    sigChan := make(chan os.Signal, 1)
    
    // Registrar todas las signals
    signals := make([]os.Signal, 0, len(handlers))
    for sig := range handlers {
        signals = append(signals, sig)
    }
    signal.Notify(sigChan, signals...)

    go func() {
        for sig := range sigChan {
            if handler, ok := handlers[sig]; ok {
                handler()
            }
        }
    }()
}

// Uso:
setupSignalHandlers(map[os.Signal]func(){
    syscall.SIGTERM: func() {
        log.Println("SIGTERM received, shutting down gracefully...")
        shutdown()
        os.Exit(0)
    },
    syscall.SIGHUP: func() {
        log.Println("SIGHUP received, reloading config...")
        reloadConfig()
    },
    syscall.SIGUSR1: func() {
        log.Println("SIGUSR1 received, dumping state...")
        dumpState()
    },
})
```

---

## 16. Comparacion: Go vs C vs Rust

### Go: funciones como valores nativos

```go
// Asignar a variable
var op func(int, int) int = add

// Pasar como argumento
sort.Slice(items, func(i, j int) bool {
    return items[i] < items[j]
})

// Retornar funcion
func multiplier(factor int) func(int) int {
    return func(n int) int { return n * factor }
}

// Almacenar en colecciones
handlers := map[string]func(){}
```

### C: punteros a funcion

```c
// Declaracion de puntero a funcion (sintaxis compleja)
int (*op)(int, int) = add;

// Typedef hace mas legible
typedef int (*BinaryOp)(int, int);
BinaryOp op = add;

// Pasar como argumento
void apply(int *arr, int n, int (*fn)(int)) {
    for (int i = 0; i < n; i++) {
        arr[i] = fn(arr[i]);
    }
}

// qsort de la stdlib
qsort(arr, n, sizeof(int), compare_ints);

// No hay funciones anonimas en C estandar
// (GCC tiene extension "nested functions" pero no es portatil)
// No hay closures (sin captura de variables)
```

### Rust: closures con traits (Fn, FnMut, FnOnce)

```rust
// Funciones como valores
let op: fn(i32, i32) -> i32 = add;

// Closures (capturan entorno)
let factor = 3;
let multiplier = |n: i32| n * factor;
// El closure captura `factor` por referencia

// Closures como argumentos con traits
fn apply<F: Fn(i32) -> i32>(items: &[i32], f: F) -> Vec<i32> {
    items.iter().map(|&x| f(x)).collect()
}

// Retornar closure (requiere Box o impl)
fn multiplier(factor: i32) -> impl Fn(i32) -> i32 {
    move |n| n * factor  // move: toma ownership de factor
}

// Closures en iteradores (zero-cost)
let result: Vec<_> = items.iter()
    .filter(|x| x.is_active())
    .map(|x| x.name.clone())
    .collect();
```

### Tabla comparativa detallada

```
┌──────────────────────┬─────────────────────┬────────────────────────┬─────────────────────────┐
│ Aspecto              │ Go                  │ C                      │ Rust                    │
├──────────────────────┼─────────────────────┼────────────────────────┼─────────────────────────┤
│ Sintaxis             │ func(T) R           │ R (*name)(T)           │ fn(T) -> R / Fn traits  │
│ Funciones anonimas   │ Si (func literals)  │ No (standard C)        │ Si (closures |x| ...)   │
│ Closures             │ Si (captura por ref) │ No                    │ Si (Fn/FnMut/FnOnce)    │
│ Captura de vars      │ Por referencia       │ N/A                   │ Por ref, mut ref, o move│
│ Type safety          │ Si                  │ Parcial (void* cast)   │ Si (compile-time)       │
│ Almacenar en struct  │ Si (campo func)     │ Si (puntero a func)    │ Si (generics/Box<dyn>)  │
│ Almacenar en map     │ Si (como valor)     │ Si (como puntero)      │ Si (Box<dyn Fn()>)      │
│ Comparacion          │ Solo nil            │ Igualdad de punteros   │ No (closures)           │
│ Performance          │ Indirection (heap)  │ Direct call (stack)    │ Monomorphized (inline)  │
│ Generics + funciones │ Si (Go 1.18+)       │ Macros (no type safe)  │ Si (full generics)      │
│ Higher-order         │ Si (manual)         │ Si (verboso)           │ Si (iterators, map, etc)│
│ Overhead closures    │ Heap alloc posible  │ N/A                    │ Zero-cost (stack)       │
│ Method values        │ Si (obj.Method)     │ No                     │ No (pero |x| obj.m(x))  │
│ Curry/partial app    │ Manual (closures)   │ No (sin closures)      │ Manual (closures/move)  │
└──────────────────────┴─────────────────────┴────────────────────────┴─────────────────────────┘
```

**Resumen de diferencias clave**:

- **Go**: simple y pragmatico — funciones son valores, closures capturan por referencia, no hay distincion entre Fn/FnMut/FnOnce. El compilador decide si el closure escapa al heap.
- **C**: punteros a funcion sin closures. Si necesitas "capturar estado", pasas un `void *userdata` extra. Flexible pero sin type safety.
- **Rust**: sistema mas rico con tres traits de closure (`Fn`, `FnMut`, `FnOnce`) que expresan como se usa el entorno capturado. Zero-cost abstractions gracias a monomorphization.

---

## 17. Method values y method expressions

### Method value: funcion bound a una instancia

```go
type Server struct {
    Name string
    Port int
}

func (s *Server) Start() error {
    fmt.Printf("Starting %s on :%d\n", s.Name, s.Port)
    return nil
}

func (s *Server) Stop() error {
    fmt.Printf("Stopping %s\n", s.Name)
    return nil
}

// Method value: captura el receptor
srv := &Server{Name: "api", Port: 8080}

startFn := srv.Start // tipo: func() error
stopFn := srv.Stop   // tipo: func() error

// Ahora puedes pasar estos como funciones normales
startFn() // "Starting api on :8080"
stopFn()  // "Stopping api"

// Util para callbacks y goroutines:
go srv.Start()

// O en dispatch tables:
actions := map[string]func() error{
    "start": srv.Start,
    "stop":  srv.Stop,
}
```

### Method expression: funcion unbound

```go
// Method expression: el receptor se convierte en primer parametro
startFn := (*Server).Start // tipo: func(*Server) error

srv1 := &Server{Name: "web", Port: 80}
srv2 := &Server{Name: "api", Port: 8080}

startFn(srv1) // "Starting web on :80"
startFn(srv2) // "Starting api on :8080"

// Util cuando quieres aplicar el mismo metodo a multiples instancias
servers := []*Server{srv1, srv2}
for _, s := range servers {
    (*Server).Start(s)
}
```

### Diferencia: method value vs method expression

```go
// Method value: receptor ya fijado
// srv.Start → func() error
// Equivale a: func() error { return srv.Start() }

// Method expression: receptor como parametro
// (*Server).Start → func(*Server) error
// Equivale a: func(s *Server) error { return s.Start() }

// Method value captura el receptor en el momento de la asignacion
srv := &Server{Name: "v1"}
fn := srv.Start
srv.Name = "v2"
fn() // Imprime "Starting v1" — capturo &Server del momento de asignacion
     // NOTA: como srv es un puntero, fn captura el puntero, no una copia
     //       Asi que en realidad imprime "Starting v2"
     //       El puntero se capturo, no el valor

// Con value receiver es diferente:
type Counter struct{ N int }
func (c Counter) Value() int { return c.N }

c := Counter{N: 42}
fn := c.Value  // Copia el valor de c
c.N = 100
fn()           // Retorna 42 — capturo la copia
```

---

## 18. Programa completo: Plugin-Based System Monitor

Un monitor de sistema que usa funciones como valores para crear un sistema de plugins extensible sin interfaces complejas.

```go
package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"sort"
	"strings"
	"sync"
	"time"
)

// ─── Tipos core ─────────────────────────────────────────────────

type Metric struct {
	Name      string    `json:"name"`
	Value     float64   `json:"value"`
	Unit      string    `json:"unit"`
	Timestamp time.Time `json:"timestamp"`
	Tags      map[string]string `json:"tags,omitempty"`
}

type Alert struct {
	Metric   string `json:"metric"`
	Message  string `json:"message"`
	Severity string `json:"severity"`
}

// Tipos de funcion — cada uno es un "plugin"
type Collector    func(ctx context.Context) ([]Metric, error)
type AlertRule    func(metric Metric) *Alert
type Formatter   func(metrics []Metric, alerts []Alert) string
type Notifier    func(alert Alert) error
type Transformer func(metrics []Metric) []Metric

// ─── Monitor ────────────────────────────────────────────────────

type Monitor struct {
	collectors   map[string]Collector
	rules        []AlertRule
	transformers []Transformer
	formatters   map[string]Formatter
	notifiers    []Notifier
	interval     time.Duration
	mu           sync.RWMutex
	lastMetrics  []Metric
}

type MonitorOption func(*Monitor)

func WithInterval(d time.Duration) MonitorOption {
	return func(m *Monitor) { m.interval = d }
}

func NewMonitor(opts ...MonitorOption) *Monitor {
	m := &Monitor{
		collectors: make(map[string]Collector),
		formatters: make(map[string]Formatter),
		interval:   10 * time.Second,
	}
	for _, opt := range opts {
		opt(m)
	}
	return m
}

// ─── Registro de plugins (funciones como valores) ───────────────

func (m *Monitor) RegisterCollector(name string, fn Collector) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.collectors[name] = fn
	log.Printf("[monitor] registered collector: %s", name)
}

func (m *Monitor) RegisterRule(fn AlertRule) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.rules = append(m.rules, fn)
}

func (m *Monitor) RegisterTransformer(fn Transformer) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.transformers = append(m.transformers, fn)
}

func (m *Monitor) RegisterFormatter(name string, fn Formatter) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.formatters[name] = fn
}

func (m *Monitor) RegisterNotifier(fn Notifier) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.notifiers = append(m.notifiers, fn)
}

// ─── Collectors (funciones que se registran como plugins) ───────

func CPUCollector() Collector {
	return func(ctx context.Context) ([]Metric, error) {
		// Simplificado: usa /proc/loadavg en Linux
		data, err := os.ReadFile("/proc/loadavg")
		if err != nil {
			return nil, fmt.Errorf("cpu collector: %w", err)
		}
		fields := strings.Fields(string(data))
		if len(fields) < 3 {
			return nil, fmt.Errorf("unexpected /proc/loadavg format")
		}

		var load1, load5, load15 float64
		fmt.Sscanf(fields[0], "%f", &load1)
		fmt.Sscanf(fields[1], "%f", &load5)
		fmt.Sscanf(fields[2], "%f", &load15)

		now := time.Now()
		cores := float64(runtime.NumCPU())

		return []Metric{
			{Name: "cpu.load1", Value: load1, Unit: "load", Timestamp: now,
				Tags: map[string]string{"cores": fmt.Sprintf("%d", runtime.NumCPU())}},
			{Name: "cpu.load5", Value: load5, Unit: "load", Timestamp: now},
			{Name: "cpu.load15", Value: load15, Unit: "load", Timestamp: now},
			{Name: "cpu.load1_pct", Value: (load1 / cores) * 100, Unit: "%", Timestamp: now},
		}, nil
	}
}

func MemoryCollector() Collector {
	return func(ctx context.Context) ([]Metric, error) {
		data, err := os.ReadFile("/proc/meminfo")
		if err != nil {
			return nil, fmt.Errorf("memory collector: %w", err)
		}

		info := make(map[string]float64)
		for _, line := range strings.Split(string(data), "\n") {
			fields := strings.Fields(line)
			if len(fields) < 2 {
				continue
			}
			key := strings.TrimSuffix(fields[0], ":")
			var val float64
			fmt.Sscanf(fields[1], "%f", &val)
			info[key] = val // kB
		}

		total := info["MemTotal"]
		available := info["MemAvailable"]
		used := total - available
		usedPct := 0.0
		if total > 0 {
			usedPct = (used / total) * 100
		}

		now := time.Now()
		return []Metric{
			{Name: "mem.total_mb", Value: total / 1024, Unit: "MB", Timestamp: now},
			{Name: "mem.used_mb", Value: used / 1024, Unit: "MB", Timestamp: now},
			{Name: "mem.available_mb", Value: available / 1024, Unit: "MB", Timestamp: now},
			{Name: "mem.used_pct", Value: usedPct, Unit: "%", Timestamp: now},
		}, nil
	}
}

func DiskCollector(mountpoints ...string) Collector {
	if len(mountpoints) == 0 {
		mountpoints = []string{"/"}
	}
	return func(ctx context.Context) ([]Metric, error) {
		var metrics []Metric
		now := time.Now()

		for _, mp := range mountpoints {
			output, err := exec.CommandContext(ctx, "df", "-B1", mp).Output()
			if err != nil {
				continue
			}
			lines := strings.Split(string(output), "\n")
			if len(lines) < 2 {
				continue
			}
			fields := strings.Fields(lines[1])
			if len(fields) < 5 {
				continue
			}

			var total, used, avail float64
			fmt.Sscanf(fields[1], "%f", &total)
			fmt.Sscanf(fields[2], "%f", &used)
			fmt.Sscanf(fields[3], "%f", &avail)

			tags := map[string]string{"mountpoint": mp}
			usedPct := 0.0
			if total > 0 {
				usedPct = (used / total) * 100
			}

			metrics = append(metrics,
				Metric{Name: "disk.total_gb", Value: total / (1024 * 1024 * 1024), Unit: "GB", Timestamp: now, Tags: tags},
				Metric{Name: "disk.used_pct", Value: usedPct, Unit: "%", Timestamp: now, Tags: tags},
				Metric{Name: "disk.avail_gb", Value: avail / (1024 * 1024 * 1024), Unit: "GB", Timestamp: now, Tags: tags},
			)
		}
		return metrics, nil
	}
}

func UptimeCollector() Collector {
	return func(ctx context.Context) ([]Metric, error) {
		data, err := os.ReadFile("/proc/uptime")
		if err != nil {
			return nil, fmt.Errorf("uptime collector: %w", err)
		}
		var uptimeSec float64
		fmt.Sscanf(string(data), "%f", &uptimeSec)

		return []Metric{
			{Name: "system.uptime_hours", Value: uptimeSec / 3600, Unit: "hours", Timestamp: time.Now()},
		}, nil
	}
}

// Factory: crear collector para cualquier archivo de /proc
func ProcFileCollector(name, path, unit string, parser func(string) (float64, error)) Collector {
	return func(ctx context.Context) ([]Metric, error) {
		data, err := os.ReadFile(path)
		if err != nil {
			return nil, err
		}
		value, err := parser(strings.TrimSpace(string(data)))
		if err != nil {
			return nil, err
		}
		return []Metric{
			{Name: name, Value: value, Unit: unit, Timestamp: time.Now()},
		}, nil
	}
}

// ─── Alert Rules (funciones que evaluan metricas) ───────────────

func ThresholdRule(metricName string, threshold float64, severity string) AlertRule {
	return func(m Metric) *Alert {
		if m.Name == metricName && m.Value > threshold {
			return &Alert{
				Metric:   m.Name,
				Message:  fmt.Sprintf("%s = %.2f%s exceeds threshold %.2f", m.Name, m.Value, m.Unit, threshold),
				Severity: severity,
			}
		}
		return nil
	}
}

func RangeRule(metricName string, min, max float64, severity string) AlertRule {
	return func(m Metric) *Alert {
		if m.Name == metricName && (m.Value < min || m.Value > max) {
			return &Alert{
				Metric:   m.Name,
				Message:  fmt.Sprintf("%s = %.2f%s outside range [%.2f, %.2f]", m.Name, m.Value, m.Unit, min, max),
				Severity: severity,
			}
		}
		return nil
	}
}

func CustomRule(predicate func(Metric) bool, severity, msgTemplate string) AlertRule {
	return func(m Metric) *Alert {
		if predicate(m) {
			return &Alert{
				Metric:   m.Name,
				Message:  fmt.Sprintf(msgTemplate, m.Name, m.Value),
				Severity: severity,
			}
		}
		return nil
	}
}

// ─── Transformers (funciones que modifican metricas) ────────────

func AddHostTag(hostname string) Transformer {
	return func(metrics []Metric) []Metric {
		for i := range metrics {
			if metrics[i].Tags == nil {
				metrics[i].Tags = make(map[string]string)
			}
			metrics[i].Tags["host"] = hostname
		}
		return metrics
	}
}

func RoundValues(decimals int) Transformer {
	factor := 1.0
	for i := 0; i < decimals; i++ {
		factor *= 10
	}
	return func(metrics []Metric) []Metric {
		for i := range metrics {
			metrics[i].Value = float64(int(metrics[i].Value*factor)) / factor
		}
		return metrics
	}
}

func FilterByPrefix(prefix string) Transformer {
	return func(metrics []Metric) []Metric {
		var filtered []Metric
		for _, m := range metrics {
			if strings.HasPrefix(m.Name, prefix) {
				filtered = append(filtered, m)
			}
		}
		return filtered
	}
}

// ─── Formatters (funciones que producen output) ─────────────────

func TextFormatter() Formatter {
	return func(metrics []Metric, alerts []Alert) string {
		var b strings.Builder
		b.WriteString(fmt.Sprintf("=== System Report %s ===\n\n",
			time.Now().Format("2006-01-02 15:04:05")))

		// Agrupar metricas por prefijo
		groups := make(map[string][]Metric)
		for _, m := range metrics {
			prefix := strings.Split(m.Name, ".")[0]
			groups[prefix] = append(groups[prefix], m)
		}

		// Ordenar grupos
		keys := make([]string, 0, len(groups))
		for k := range groups {
			keys = append(keys, k)
		}
		sort.Strings(keys)

		for _, key := range keys {
			b.WriteString(fmt.Sprintf("[%s]\n", strings.ToUpper(key)))
			for _, m := range groups[key] {
				b.WriteString(fmt.Sprintf("  %-25s %10.2f %s\n", m.Name, m.Value, m.Unit))
			}
			b.WriteString("\n")
		}

		if len(alerts) > 0 {
			b.WriteString("[ALERTS]\n")
			for _, a := range alerts {
				b.WriteString(fmt.Sprintf("  [%s] %s\n", a.Severity, a.Message))
			}
		}

		return b.String()
	}
}

func JSONFormatter() Formatter {
	return func(metrics []Metric, alerts []Alert) string {
		report := struct {
			Timestamp string   `json:"timestamp"`
			Metrics   []Metric `json:"metrics"`
			Alerts    []Alert  `json:"alerts,omitempty"`
		}{
			Timestamp: time.Now().Format(time.RFC3339),
			Metrics:   metrics,
			Alerts:    alerts,
		}
		data, _ := json.MarshalIndent(report, "", "  ")
		return string(data)
	}
}

func CSVFormatter() Formatter {
	return func(metrics []Metric, alerts []Alert) string {
		var b strings.Builder
		b.WriteString("timestamp,name,value,unit\n")
		for _, m := range metrics {
			b.WriteString(fmt.Sprintf("%s,%s,%.4f,%s\n",
				m.Timestamp.Format(time.RFC3339), m.Name, m.Value, m.Unit))
		}
		return b.String()
	}
}

func PrometheusFormatter() Formatter {
	return func(metrics []Metric, alerts []Alert) string {
		var b strings.Builder
		for _, m := range metrics {
			name := strings.ReplaceAll(m.Name, ".", "_")
			labels := ""
			if len(m.Tags) > 0 {
				parts := make([]string, 0, len(m.Tags))
				for k, v := range m.Tags {
					parts = append(parts, fmt.Sprintf(`%s="%s"`, k, v))
				}
				sort.Strings(parts)
				labels = "{" + strings.Join(parts, ",") + "}"
			}
			b.WriteString(fmt.Sprintf("%s%s %.4f %d\n",
				name, labels, m.Value, m.Timestamp.UnixMilli()))
		}
		return b.String()
	}
}

// ─── Notifiers (funciones que envian alertas) ───────────────────

func StdoutNotifier() Notifier {
	return func(a Alert) error {
		fmt.Printf("ALERT [%s] %s: %s\n", a.Severity, a.Metric, a.Message)
		return nil
	}
}

func FileNotifier(path string) Notifier {
	return func(a Alert) error {
		f, err := os.OpenFile(path, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
		if err != nil {
			return err
		}
		defer f.Close()
		_, err = fmt.Fprintf(f, "%s [%s] %s: %s\n",
			time.Now().Format(time.RFC3339), a.Severity, a.Metric, a.Message)
		return err
	}
}

func FilteredNotifier(minSeverity string, next Notifier) Notifier {
	severityOrder := map[string]int{"info": 0, "warning": 1, "critical": 2}
	minLevel := severityOrder[minSeverity]

	return func(a Alert) error {
		if severityOrder[a.Severity] >= minLevel {
			return next(a)
		}
		return nil
	}
}

// ─── Ejecucion del monitor ──────────────────────────────────────

func (m *Monitor) Collect(ctx context.Context) ([]Metric, []Alert) {
	m.mu.RLock()
	defer m.mu.RUnlock()

	// Recolectar metricas en paralelo
	type collectorResult struct {
		name    string
		metrics []Metric
		err     error
	}

	results := make(chan collectorResult, len(m.collectors))
	for name, fn := range m.collectors {
		go func(n string, f Collector) {
			metrics, err := f(ctx)
			results <- collectorResult{n, metrics, err}
		}(name, fn)
	}

	var allMetrics []Metric
	for i := 0; i < len(m.collectors); i++ {
		r := <-results
		if r.err != nil {
			log.Printf("[monitor] collector %s failed: %v", r.name, r.err)
			continue
		}
		allMetrics = append(allMetrics, r.metrics...)
	}

	// Aplicar transformers (pipeline de funciones)
	for _, transform := range m.transformers {
		allMetrics = transform(allMetrics)
	}

	// Evaluar reglas de alerta (cada regla es una funcion)
	var alerts []Alert
	for _, metric := range allMetrics {
		for _, rule := range m.rules {
			if alert := rule(metric); alert != nil {
				alerts = append(alerts, *alert)
			}
		}
	}

	// Enviar notificaciones
	for _, alert := range alerts {
		for _, notify := range m.notifiers {
			if err := notify(alert); err != nil {
				log.Printf("[monitor] notifier failed: %v", err)
			}
		}
	}

	m.mu.RUnlock()
	m.mu.Lock()
	m.lastMetrics = allMetrics
	m.mu.Unlock()
	m.mu.RLock()

	return allMetrics, alerts
}

func (m *Monitor) Format(name string, metrics []Metric, alerts []Alert) string {
	m.mu.RLock()
	defer m.mu.RUnlock()

	fn, ok := m.formatters[name]
	if !ok {
		fn = m.formatters["text"]
	}
	if fn == nil {
		return fmt.Sprintf("%d metrics, %d alerts\n", len(metrics), len(alerts))
	}
	return fn(metrics, alerts)
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	hostname, _ := os.Hostname()

	// Crear monitor con functional options
	mon := NewMonitor(WithInterval(5 * time.Second))

	// Registrar collectors (cada uno es una funcion)
	mon.RegisterCollector("cpu", CPUCollector())
	mon.RegisterCollector("memory", MemoryCollector())
	mon.RegisterCollector("disk", DiskCollector("/", "/home"))
	mon.RegisterCollector("uptime", UptimeCollector())

	// Registrar transformers (pipeline de funciones)
	mon.RegisterTransformer(AddHostTag(hostname))
	mon.RegisterTransformer(RoundValues(2))

	// Registrar reglas de alerta (cada regla es una funcion)
	mon.RegisterRule(ThresholdRule("cpu.load1_pct", 90, "critical"))
	mon.RegisterRule(ThresholdRule("cpu.load1_pct", 70, "warning"))
	mon.RegisterRule(ThresholdRule("mem.used_pct", 90, "critical"))
	mon.RegisterRule(ThresholdRule("mem.used_pct", 80, "warning"))
	mon.RegisterRule(ThresholdRule("disk.used_pct", 90, "critical"))
	mon.RegisterRule(RangeRule("system.uptime_hours", 0.1, 8760, "info"))

	// Registrar formatters (cada uno es una funcion)
	mon.RegisterFormatter("text", TextFormatter())
	mon.RegisterFormatter("json", JSONFormatter())
	mon.RegisterFormatter("csv", CSVFormatter())
	mon.RegisterFormatter("prometheus", PrometheusFormatter())

	// Registrar notifiers (cada uno es una funcion)
	mon.RegisterNotifier(StdoutNotifier())
	mon.RegisterNotifier(FilteredNotifier("warning",
		FileNotifier(filepath.Join(os.TempDir(), "monitor-alerts.log"))))

	// Seleccionar formato
	format := "text"
	if len(os.Args) > 1 {
		format = os.Args[1]
	}

	// Ejecutar una vez
	ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	metrics, alerts := mon.Collect(ctx)
	fmt.Println(mon.Format(format, metrics, alerts))

	log.Printf("[monitor] collected %d metrics, triggered %d alerts", len(metrics), len(alerts))
}
```

### Desglose de funciones como valores en el programa

```
┌──────────────────────────────────────────────────────────────┐
│  FUNCIONES COMO VALORES — PATRON PLUGIN                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Tipo de funcion      Rol en el sistema      Ejemplo         │
│  ──────────────────   ────────────────────   ──────────────  │
│  Collector            Recolectar metricas    CPUCollector()   │
│  AlertRule            Evaluar condiciones    ThresholdRule()  │
│  Transformer          Pipeline de datos      AddHostTag()     │
│  Formatter            Generar output         JSONFormatter()  │
│  Notifier             Enviar alertas         FileNotifier()   │
│  MonitorOption        Configurar monitor     WithInterval()   │
│                                                              │
│  Cada "plugin" es simplemente una funcion:                    │
│  ├─ No necesitas interfaces con multiples metodos            │
│  ├─ No necesitas registros de plugins complejos              │
│  ├─ No necesitas reflection ni generacion de codigo          │
│  ├─ Factories (CPUCollector()) retornan funciones            │
│  │   configuradas — closure sobre los parametros             │
│  └─ Composicion: FilteredNotifier envuelve otro Notifier     │
│                                                              │
│  El monitor ejecuta funciones almacenadas:                    │
│  1. collectors (map[string]Collector) → en paralelo          │
│  2. transformers ([]Transformer) → en secuencia (pipeline)   │
│  3. rules ([]AlertRule) → por cada metrica                   │
│  4. notifiers ([]Notifier) → por cada alerta                 │
│  5. formatters (map[string]Formatter) → seleccion por nombre │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 19. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
var f func()
fmt.Println(f == nil)
```

<details>
<summary>Respuesta</summary>

```
true
```

El zero value de una variable de tipo funcion es `nil`.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
func apply(n int, fn func(int) int) int {
    return fn(n)
}
result := apply(5, func(n int) int { return n * n })
fmt.Println(result)
```

<details>
<summary>Respuesta</summary>

```
25
```

La funcion literal `func(n int) int { return n * n }` se pasa como argumento y se aplica a 5.
</details>

**Ejercicio 3**: ¿Compila?

```go
f1 := func() {}
f2 := func() {}
fmt.Println(f1 == f2)
```

<details>
<summary>Respuesta</summary>

No compila. Error: `invalid operation: cannot compare f1 == f2 (func can only be compared to nil)`. Las funciones solo pueden compararse con `nil`.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
func makeCounter() func() int {
    n := 0
    return func() int {
        n++
        return n
    }
}

c1 := makeCounter()
c2 := makeCounter()
fmt.Println(c1(), c1(), c1())
fmt.Println(c2(), c2())
```

<details>
<summary>Respuesta</summary>

```
1 2 3
1 2
```

Cada llamada a `makeCounter()` crea un nuevo `n`. `c1` y `c2` tienen closures independientes sobre variables `n` distintas. (Este es un preview de closures — T01 siguiente.)
</details>

**Ejercicio 5**: ¿Que imprime?

```go
fns := make([]func(), 3)
for i := 0; i < 3; i++ {
    fns[i] = func() { fmt.Print(i, " ") }
}
for _, fn := range fns {
    fn()
}
```

<details>
<summary>Respuesta</summary>

**Pre-Go 1.22**: `3 3 3` — todas las closures capturan la misma variable `i`, que al final del loop vale 3.

**Go 1.22+**: `0 1 2` — cada iteracion tiene su propia variable `i`.

Esto se cubrira en detalle en T01 Closures.
</details>

**Ejercicio 6**: ¿Que imprime?

```go
type Op func(int, int) int

func do(a, b int, operations ...Op) []int {
    results := make([]int, len(operations))
    for i, op := range operations {
        results[i] = op(a, b)
    }
    return results
}

add := func(a, b int) int { return a + b }
sub := func(a, b int) int { return a - b }
mul := func(a, b int) int { return a * b }

fmt.Println(do(10, 3, add, sub, mul))
```

<details>
<summary>Respuesta</summary>

```
[13 7 30]
```

`do` aplica cada operacion a (10, 3): 10+3=13, 10-3=7, 10*3=30.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
func decorate(fn func(string) string) func(string) string {
    return func(s string) string {
        return "[" + fn(s) + "]"
    }
}

upper := strings.ToUpper
decorated := decorate(upper)
doubleDecorated := decorate(decorated)

fmt.Println(doubleDecorated("hello"))
```

<details>
<summary>Respuesta</summary>

```
[[HELLO]]
```

`decorated("hello")` = `"[HELLO]"`. `doubleDecorated("hello")` = `"[" + "[HELLO]" + "]"` = `"[[HELLO]]"`.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
commands := map[string]func(int) int{
    "double": func(n int) int { return n * 2 },
    "square": func(n int) int { return n * n },
}

fn, ok := commands["triple"]
fmt.Println(ok, fn == nil)
```

<details>
<summary>Respuesta</summary>

```
false true
```

`"triple"` no existe en el map. `ok` es false y `fn` es el zero value de `func(int) int`, que es `nil`.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
result := func(a, b int) int {
    return a + b
}(100, 200)
fmt.Println(result)
```

<details>
<summary>Respuesta</summary>

```
300
```

Funcion literal invocada inmediatamente (IIFE). Se define y se llama con `(100, 200)`.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
type Server struct{ Name string }

func (s Server) GetName() string { return s.Name }

srv := Server{Name: "web01"}
fn := srv.GetName  // method value
srv.Name = "web02"
fmt.Println(fn())
```

<details>
<summary>Respuesta</summary>

```
web01
```

`srv.GetName` es un method value con value receiver. Al asignar, se copia el receptor (`Server{Name: "web01"}`). Cambiar `srv.Name` despues no afecta la copia capturada.
</details>

---

## 20. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│          FUNCIONES COMO VALORES — RESUMEN                    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  TIPO DE FUNCION                                             │
│  type Op func(int, int) int ← nombrado (recomendado)         │
│  var f func(int, int) int   ← anonimo                       │
│  Zero value = nil                                            │
│  Solo comparar con nil (f == nil), no entre funciones        │
│                                                              │
│  ASIGNAR                                                     │
│  f = existingFunc           ← funcion nombrada               │
│  f = func(x int) { ... }   ← funcion literal (anonima)      │
│  result := func(){}()      ← IIFE                           │
│                                                              │
│  PASAR COMO ARGUMENTO                                        │
│  sort.Slice(s, func(i,j int) bool { ... })                   │
│  filepath.WalkDir(root, func(path, d, err) error { ... })    │
│  http.HandleFunc(path, func(w, r) { ... })                   │
│                                                              │
│  RETORNAR FUNCION                                            │
│  Factory: func makeValidator(min int) func(string) error     │
│  Decorator: func withRetry(n) func(func()error) error        │
│  Middleware: func logging(next Handler) Handler               │
│                                                              │
│  ALMACENAR EN COLECCIONES                                    │
│  []func()                 ← pipeline de operaciones          │
│  map[string]func()        ← dispatch table / CLI commands    │
│  struct { OnEvent func()} ← callbacks configurables          │
│                                                              │
│  COMPOSICION                                                 │
│  pipe(fn1, fn2, fn3)      ← pipeline en orden natural        │
│  compose(f, g)            ← f(g(x))                         │
│  chain(handler, mw1, mw2) ← middleware HTTP                  │
│  and(pred1, pred2)        ← combinar predicados              │
│                                                              │
│  PATRONES CLAVE                                              │
│  ├─ Middleware:   func(Handler) Handler                      │
│  ├─ Strategy:     map[string]func() — seleccion en runtime   │
│  ├─ Decorator:    withLog(withRetry(withTimeout(op)))         │
│  ├─ Plugin:       registrar funciones en struct              │
│  ├─ DI en tests:  struct { RunCmd func(...) } → mock facil   │
│  └─ Method value: srv.Start como func() error                │
│                                                              │
│  METHOD VALUE vs METHOD EXPRESSION                           │
│  srv.Start        → func() error        (receptor bound)     │
│  (*Server).Start  → func(*Server) error (receptor como param)│
│                                                              │
│  FUNCIONES vs INTERFACES                                     │
│  ├─ 1 operacion → func como campo                           │
│  ├─ N operaciones relacionadas → interface                   │
│  └─ La stdlib usa ambos: http.HandlerFunc vs http.Handler    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: S02/T01 - Closures — captura de variables, closure sobre loop variable (pre-Go 1.22 vs post), goroutine closures
