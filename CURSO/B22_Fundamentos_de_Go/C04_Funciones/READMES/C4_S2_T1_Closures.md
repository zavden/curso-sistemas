# Closures — Captura de Variables del Entorno

## 1. Introduccion

Un **closure** es una funcion literal que **referencia variables definidas fuera de su cuerpo**. La funcion "cierra" sobre esas variables — de ahi el nombre. En T03 vimos que las funciones son valores; ahora entendemos *como* capturan el estado de su entorno.

En Go, los closures capturan variables **por referencia**, no por copia. Esto significa que el closure y el codigo que lo creo comparten la misma variable — si uno la modifica, el otro ve el cambio. Esta mecanica es poderosa pero es tambien la fuente del bug mas clasico de Go: el closure dentro de un loop.

Los closures son la base de patrones fundamentales en SysAdmin/DevOps: generadores de configuracion, factories de middleware, acumuladores de metricas, pools de workers con estado, y goroutines que necesitan acceso a variables compartidas.

```
┌──────────────────────────────────────────────────────────────┐
│              CLOSURES EN GO                                   │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Funcion literal:                                            │
│  fn := func(x int) int { return x * 2 }                      │
│  ↑ No captura nada — es una funcion literal, no un closure    │
│                                                              │
│  Closure:                                                    │
│  factor := 3                                                 │
│  fn := func(x int) int { return x * factor }                  │
│  ↑ Captura 'factor' — es un closure                           │
│                                                              │
│  Mecanica de captura:                                        │
│  ├─ Por REFERENCIA (no por copia)                            │
│  ├─ La variable capturada vive mientras el closure exista     │
│  ├─ Closure y creador comparten la MISMA variable            │
│  ├─ Modificar la variable afecta a ambos                     │
│  └─ El compilador mueve la variable al heap si es necesario   │
│     (escape analysis)                                        │
│                                                              │
│  El bug clasico:                                             │
│  for i := 0; i < 3; i++ {                                    │
│      go func() { fmt.Println(i) }()                          │
│  }                                                           │
│  Pre-Go 1.22: imprime "3 3 3" (captura misma variable i)     │
│  Go 1.22+:    imprime "0 1 2" (i tiene scope por iteracion)  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Captura por referencia — la regla fundamental

### Demostrar que es por referencia

```go
func main() {
    x := 10
    
    // El closure captura &x, no el valor 10
    increment := func() {
        x++ // Modifica la MISMA x del scope exterior
    }
    
    fmt.Println(x) // 10
    increment()
    fmt.Println(x) // 11
    increment()
    fmt.Println(x) // 12
    
    // Modificar x fuera del closure tambien afecta al closure
    x = 100
    increment()
    fmt.Println(x) // 101
}
```

### Multiples closures sobre la misma variable

```go
func main() {
    count := 0
    
    inc := func() { count++ }
    dec := func() { count-- }
    get := func() int { return count }
    
    inc()
    inc()
    inc()
    dec()
    
    fmt.Println(get()) // 2
    // Los tres closures comparten la misma variable count
}
```

### Captura vs argumento — diferencia critica

```go
func main() {
    x := 10
    
    // Closure: captura x por referencia
    closureFn := func() {
        fmt.Println("closure:", x) // lee la variable compartida
    }
    
    // Funcion con argumento: recibe copia de x
    argFn := func(val int) {
        fmt.Println("arg:", val) // lee su propia copia
    }
    
    x = 20
    
    closureFn()  // "closure: 20" — ve el cambio
    argFn(10)    // "arg: 10"     — tiene su copia original
}
```

---

## 3. Escape analysis: como Go maneja la captura

### La variable se mueve al heap

```go
func makeCounter() func() int {
    n := 0 // Normalmente n viviria en el stack de makeCounter
    return func() int {
        n++ // Pero este closure sobrevive a makeCounter
        return n
    }
    // Cuando makeCounter retorna, su stack frame se destruye
    // Pero n sigue vivo porque el closure lo referencia
    // → El compilador mueve n al heap (escape analysis)
}

c := makeCounter()
fmt.Println(c()) // 1
fmt.Println(c()) // 2
fmt.Println(c()) // 3
// n vive en el heap, accesible solo via el closure
```

### Verificar con el compilador

```bash
# go build muestra que escapa al heap
$ go build -gcflags="-m" main.go
# ./main.go:3:2: moved to heap: n
# ./main.go:4:9: func literal escapes to heap

# Si la variable NO escapa (closure no sobrevive al scope):
func process() {
    x := 42
    fn := func() { fmt.Println(x) }
    fn() // fn no escapa — x puede quedarse en el stack
}
# ./main.go:2:2: x does not escape (posible si fn se inlinea)
```

### Lo que el compilador hace internamente

```go
// Lo que tu escribes:
func makeAdder(base int) func(int) int {
    return func(n int) int {
        return base + n
    }
}

// Lo que el compilador genera conceptualmente:
// (esto es una simplificacion ilustrativa)
type closureEnv struct {
    base *int // puntero a la variable capturada
}

func makeAdder(base int) func(int) int {
    heapBase := new(int) // mueve base al heap
    *heapBase = base
    
    env := &closureEnv{base: heapBase}
    // Retorna funcion + puntero al entorno
    return func(n int) int {
        return *env.base + n
    }
}
```

---

## 4. El bug del closure en el loop — la trampa clasica

### Pre-Go 1.22: el problema

```go
// ESTE ES EL BUG MAS FAMOSO DE GO
func main() {
    fns := make([]func(), 5)
    
    for i := 0; i < 5; i++ {
        fns[i] = func() {
            fmt.Println(i) // Captura i por referencia
        }
    }
    
    for _, fn := range fns {
        fn()
    }
}

// Pre-Go 1.22 imprime:
// 5
// 5
// 5
// 5
// 5
//
// ¿Por que? Todos los closures capturan la MISMA variable i
// Cuando el loop termina, i vale 5
// Todos los closures leen i → todos imprimen 5
```

### Pre-Go 1.22: las soluciones clasicas

```go
// Solucion 1: variable local en cada iteracion
for i := 0; i < 5; i++ {
    i := i // Shadow: nueva variable i en cada iteracion
    fns[i] = func() {
        fmt.Println(i) // Captura la i local de esta iteracion
    }
}

// Solucion 2: pasar como argumento
for i := 0; i < 5; i++ {
    fns[i] = func(n int) func() {
        return func() {
            fmt.Println(n) // n es una copia, no la variable del loop
        }
    }(i)
}

// Solucion 3: pasar como parametro de la funcion literal
for i := 0; i < 5; i++ {
    func(n int) {
        fns[n] = func() {
            fmt.Println(n)
        }
    }(i)
}
```

### Go 1.22+: el fix en el lenguaje

```go
// A partir de Go 1.22, la variable del loop tiene scope POR ITERACION
// Cada iteracion crea una nueva variable

// go.mod debe tener: go 1.22 o superior

for i := 0; i < 5; i++ {
    fns[i] = func() {
        fmt.Println(i) // Go 1.22+: cada iteracion tiene su propia i
    }
}
// Imprime: 0 1 2 3 4 ✓

// Tambien aplica a for-range:
items := []string{"web01", "web02", "web03"}
for _, item := range items {
    go func() {
        fmt.Println(item) // Go 1.22+: cada iteracion captura su propio item
    }()
}
```

### ¿Como saber que version usa tu proyecto?

```go
// El comportamiento depende de la directiva "go" en go.mod:
// go 1.21 → comportamiento viejo (variable compartida)
// go 1.22 → comportamiento nuevo (variable por iteracion)

// Verificar:
// cat go.mod | grep "^go "
// go 1.22

// IMPORTANTE: si mantienes codigo legacy con go 1.21 o menor,
// el i := i sigue siendo necesario
```

---

## 5. for-range y closures

### Pre-Go 1.22: el mismo problema con range

```go
// Pre-Go 1.22:
servers := []string{"web01", "web02", "web03"}

for _, server := range servers {
    go func() {
        fmt.Println(server) // Todas imprimen "web03"
    }()
}
time.Sleep(100 * time.Millisecond)
// web03
// web03
// web03

// La variable "server" es UNA sola variable que se reasigna
// en cada iteracion. Todas las goroutines la comparten.
```

### Pre-Go 1.22: la solucion idiomatica

```go
// Pasar como argumento a la goroutine
for _, server := range servers {
    go func(s string) {
        fmt.Println(s) // s es una copia
    }(server) // ← pasar el valor actual
}
// web01 web02 web03 (en cualquier orden)
```

### Go 1.22+: funciona como esperas

```go
// Go 1.22+: cada iteracion tiene su propia variable
for _, server := range servers {
    go func() {
        fmt.Println(server) // OK: cada iteracion tiene su propio server
    }()
}
```

### Verificar con go vet (pre-1.22)

```bash
# go vet detecta este patron:
$ go vet ./...
# loop variable server captured by func literal

# golangci-lint tambien lo detecta:
$ golangci-lint run
# exportloopref: loop variable server captured by func literal
```

---

## 6. Closures con goroutines — patrones seguros

### Patron 1: pasar valores como argumentos

```go
func checkAllHosts(hosts []string) {
    var wg sync.WaitGroup
    
    for _, host := range hosts {
        wg.Add(1)
        // Pasar host como argumento — copia segura
        go func(h string) {
            defer wg.Done()
            if err := pingHost(h); err != nil {
                log.Printf("FAIL %s: %v", h, err)
            }
        }(host)
    }
    
    wg.Wait()
}
```

### Patron 2: variable local en el loop

```go
func deployAll(services []string, version string) {
    var wg sync.WaitGroup
    
    for _, svc := range services {
        svc := svc // Variable local (necesario pre-Go 1.22)
        wg.Add(1)
        go func() {
            defer wg.Done()
            // svc es la variable local, no la del loop
            if err := deploy(svc, version); err != nil {
                log.Printf("deploy %s failed: %v", svc, err)
            }
        }()
    }
    
    wg.Wait()
}
```

### Patron 3: closure que comparte estado con mutex

```go
func collectMetrics(hosts []string) map[string]float64 {
    results := make(map[string]float64)
    var mu sync.Mutex
    var wg sync.WaitGroup
    
    for _, host := range hosts {
        wg.Add(1)
        go func(h string) {
            defer wg.Done()
            
            latency := measureLatency(h)
            
            mu.Lock()
            results[h] = latency // Closure captura results y mu
            mu.Unlock()
        }(host)
    }
    
    wg.Wait()
    return results
}
```

### Patron 4: closure con channel para resultados

```go
type HealthResult struct {
    Host    string
    Healthy bool
    Latency time.Duration
}

func checkHealthParallel(hosts []string) []HealthResult {
    results := make(chan HealthResult, len(hosts))
    
    for _, host := range hosts {
        go func(h string) {
            start := time.Now()
            healthy := ping(h)
            results <- HealthResult{
                Host:    h,
                Healthy: healthy,
                Latency: time.Since(start),
            }
        }(host)
    }
    
    var all []HealthResult
    for range hosts {
        all = append(all, <-results)
    }
    return all
}
```

---

## 7. Closures como generadores

### Generador de IDs

```go
func idGenerator(prefix string) func() string {
    var counter uint64 // Capturada por referencia
    var mu sync.Mutex  // Para seguridad en goroutines
    
    return func() string {
        mu.Lock()
        defer mu.Unlock()
        counter++
        return fmt.Sprintf("%s-%06d", prefix, counter)
    }
}

nextRequestID := idGenerator("req")
nextJobID := idGenerator("job")

fmt.Println(nextRequestID()) // req-000001
fmt.Println(nextRequestID()) // req-000002
fmt.Println(nextJobID())     // job-000001
fmt.Println(nextRequestID()) // req-000003
// Cada generador tiene su propio counter independiente
```

### Generador de Fibonacci

```go
func fibonacci() func() int {
    a, b := 0, 1
    return func() int {
        val := a
        a, b = b, a+b
        return val
    }
}

fib := fibonacci()
for i := 0; i < 10; i++ {
    fmt.Printf("%d ", fib())
}
// 0 1 1 2 3 5 8 13 21 34
```

### Generador de reintentos con backoff

```go
func backoffGenerator(initial, max time.Duration, factor float64) func() time.Duration {
    current := initial
    return func() time.Duration {
        delay := current
        current = time.Duration(float64(current) * factor)
        if current > max {
            current = max
        }
        return delay
    }
}

nextDelay := backoffGenerator(100*time.Millisecond, 30*time.Second, 2.0)

for attempt := 1; attempt <= 8; attempt++ {
    delay := nextDelay()
    fmt.Printf("Attempt %d: wait %v\n", attempt, delay)
    // Attempt 1: wait 100ms
    // Attempt 2: wait 200ms
    // Attempt 3: wait 400ms
    // Attempt 4: wait 800ms
    // Attempt 5: wait 1.6s
    // Attempt 6: wait 3.2s
    // Attempt 7: wait 6.4s
    // Attempt 8: wait 12.8s
}
```

### Iterador perezoso (lazy iterator)

```go
// Genera lineas de un archivo bajo demanda
func lineIterator(path string) (func() (string, bool), func() error) {
    f, err := os.Open(path)
    if err != nil {
        return func() (string, bool) { return "", false },
               func() error { return err }
    }
    
    scanner := bufio.NewScanner(f)
    closed := false
    
    next := func() (string, bool) {
        if closed || !scanner.Scan() {
            return "", false
        }
        return scanner.Text(), true
    }
    
    cleanup := func() error {
        closed = true
        return f.Close()
    }
    
    return next, cleanup
}

// Uso:
next, cleanup := lineIterator("/var/log/syslog")
defer cleanup()

for line, ok := next(); ok; line, ok = next() {
    if strings.Contains(line, "error") {
        fmt.Println(line)
    }
}
```

---

## 8. Closures en patrones SysAdmin

### Patron: Config watcher con hot reload

```go
func watchConfig(path string, interval time.Duration) func() map[string]string {
    var (
        config    map[string]string
        lastMod   time.Time
        mu        sync.RWMutex
    )
    
    // Carga inicial
    loadConfig := func() {
        data, err := os.ReadFile(path)
        if err != nil {
            log.Printf("config read error: %v", err)
            return
        }
        var newConfig map[string]string
        if err := json.Unmarshal(data, &newConfig); err != nil {
            log.Printf("config parse error: %v", err)
            return
        }
        mu.Lock()
        config = newConfig
        mu.Unlock()
        log.Printf("config reloaded: %d keys", len(newConfig))
    }
    
    loadConfig()
    
    // Goroutine de monitoreo — closure captura todo el estado
    go func() {
        ticker := time.NewTicker(interval)
        defer ticker.Stop()
        for range ticker.C {
            info, err := os.Stat(path)
            if err != nil {
                continue
            }
            if info.ModTime().After(lastMod) {
                lastMod = info.ModTime()
                loadConfig()
            }
        }
    }()
    
    // Retorna un getter thread-safe
    return func() map[string]string {
        mu.RLock()
        defer mu.RUnlock()
        // Retornar copia para evitar race conditions
        cpy := make(map[string]string, len(config))
        for k, v := range config {
            cpy[k] = v
        }
        return cpy
    }
}

// Uso:
getConfig := watchConfig("/etc/myapp/config.json", 5*time.Second)

// En cualquier parte del programa:
cfg := getConfig() // Siempre tiene la version mas reciente
fmt.Println(cfg["database_host"])
```

### Patron: Connection pool con closure

```go
func makePool(factory func() (net.Conn, error), maxSize int) (get func() (net.Conn, error), put func(net.Conn)) {
    pool := make(chan net.Conn, maxSize)
    
    get = func() (net.Conn, error) {
        select {
        case conn := <-pool:
            // Verificar que la conexion sigue viva
            conn.SetReadDeadline(time.Now().Add(1 * time.Millisecond))
            buf := make([]byte, 1)
            if _, err := conn.Read(buf); err != nil {
                conn.Close() // Conexion muerta, crear nueva
                return factory()
            }
            conn.SetReadDeadline(time.Time{}) // Reset deadline
            return conn, nil
        default:
            return factory()
        }
    }
    
    put = func(conn net.Conn) {
        select {
        case pool <- conn:
            // Devuelta al pool
        default:
            conn.Close() // Pool lleno, cerrar
        }
    }
    
    return get, put
}

// Uso:
getConn, putConn := makePool(func() (net.Conn, error) {
    return net.DialTimeout("tcp", "db-server:5432", 5*time.Second)
}, 10)

conn, err := getConn()
if err != nil {
    log.Fatal(err)
}
defer putConn(conn)
// Usar conn...
```

### Patron: Rate limiter con closure

```go
func rateLimiter(maxPerSecond int) func() bool {
    var (
        mu        sync.Mutex
        tokens    = maxPerSecond
        lastReset = time.Now()
    )
    
    return func() bool {
        mu.Lock()
        defer mu.Unlock()
        
        now := time.Now()
        elapsed := now.Sub(lastReset)
        if elapsed >= time.Second {
            tokens = maxPerSecond
            lastReset = now
        }
        
        if tokens <= 0 {
            return false
        }
        tokens--
        return true
    }
}

allow := rateLimiter(100) // 100 requests/sec

if allow() {
    processRequest(r)
} else {
    http.Error(w, "rate limited", 429)
}
```

### Patron: Circuit breaker con closure

```go
func circuitBreaker(threshold int, resetTimeout time.Duration) func(func() error) error {
    var (
        mu           sync.Mutex
        failures     int
        lastFailure  time.Time
        state        = "closed" // closed, open, half-open
    )
    
    return func(operation func() error) error {
        mu.Lock()
        
        // Si el circuito esta abierto, verificar si debemos intentar
        if state == "open" {
            if time.Since(lastFailure) > resetTimeout {
                state = "half-open"
                log.Println("circuit breaker: half-open, trying...")
            } else {
                mu.Unlock()
                return fmt.Errorf("circuit breaker open: %d consecutive failures", failures)
            }
        }
        mu.Unlock()
        
        // Ejecutar la operacion
        err := operation()
        
        mu.Lock()
        defer mu.Unlock()
        
        if err != nil {
            failures++
            lastFailure = time.Now()
            if failures >= threshold {
                state = "open"
                log.Printf("circuit breaker: OPEN after %d failures", failures)
            }
            return err
        }
        
        // Exito: resetear
        failures = 0
        state = "closed"
        return nil
    }
}

// Uso:
callDB := circuitBreaker(5, 30*time.Second)

err := callDB(func() error {
    return db.Ping()
})
if err != nil {
    // Usar cache o fallback
    log.Printf("DB unavailable: %v", err)
}
```

---

## 9. Closures y defer

### defer con closure — leer valor al final

```go
func processFile(path string) error {
    start := time.Now()
    var bytesProcessed int
    
    // defer con closure: lee bytesProcessed al momento de ejecutarse,
    // no al momento de registrarse
    defer func() {
        log.Printf("processed %s: %d bytes in %v", path, bytesProcessed, time.Since(start))
    }()
    
    data, err := os.ReadFile(path)
    if err != nil {
        return err // defer imprime: "0 bytes"
    }
    
    bytesProcessed = len(data)
    // ... procesar ...
    return nil // defer imprime: "N bytes"
}
```

### defer con closure para modificar error de retorno

```go
func writeConfig(path string, cfg Config) (err error) {
    // Closure sobre named return: puede modificar err
    defer func() {
        if err != nil {
            err = fmt.Errorf("writeConfig(%s): %w", path, err)
            // Limpieza: si escribimos parcialmente, borrar
            os.Remove(path + ".tmp")
        }
    }()
    
    data, err := json.MarshalIndent(cfg, "", "  ")
    if err != nil {
        return err // defer envuelve el error
    }
    
    tmpPath := path + ".tmp"
    if err = os.WriteFile(tmpPath, data, 0644); err != nil {
        return err // defer envuelve y limpia
    }
    
    return os.Rename(tmpPath, path) // defer envuelve si falla
}
```

### Contraste: defer con argumento vs defer con closure

```go
func example() {
    x := 0
    
    // defer con argumento: evalua x AHORA (x=0)
    defer fmt.Println("arg:", x)
    
    // defer con closure: lee x al EJECUTARSE
    defer func() {
        fmt.Println("closure:", x)
    }()
    
    x = 42
}
// Output (LIFO):
// closure: 42   ← leyo x al ejecutarse
// arg: 0        ← x fue evaluado al registrar el defer
```

---

## 10. Closures con errgroup

### Ejecucion paralela con captura segura

```go
func validateInfrastructure(hosts []string) error {
    g, ctx := errgroup.WithContext(context.Background())
    
    // Resultados protegidos por mutex (closure captura ambos)
    var (
        mu      sync.Mutex
        results = make(map[string]string)
    )
    
    for _, host := range hosts {
        host := host // Pre-Go 1.22
        g.Go(func() error {
            // Closure captura: ctx, mu, results, host
            status, err := checkHost(ctx, host)
            if err != nil {
                return fmt.Errorf("check %s: %w", host, err)
            }
            
            mu.Lock()
            results[host] = status
            mu.Unlock()
            return nil
        })
    }
    
    if err := g.Wait(); err != nil {
        return err
    }
    
    // Usar results...
    for host, status := range results {
        fmt.Printf("%-20s %s\n", host, status)
    }
    return nil
}
```

### errgroup con semaforo (limitar concurrencia)

```go
func scanPorts(host string, ports []int) map[int]bool {
    g, ctx := errgroup.WithContext(context.Background())
    g.SetLimit(20) // Maximo 20 goroutines simultaneas
    
    var mu sync.Mutex
    results := make(map[int]bool)
    
    for _, port := range ports {
        port := port
        g.Go(func() error {
            open := isPortOpen(ctx, host, port)
            
            mu.Lock()
            results[port] = open
            mu.Unlock()
            return nil
        })
    }
    
    g.Wait()
    return results
}
```

---

## 11. Closures y method values

### Method value como closure implicito

```go
type Logger struct {
    prefix string
    level  int
}

func (l *Logger) Info(msg string) {
    if l.level <= 1 {
        fmt.Printf("[INFO] %s: %s\n", l.prefix, msg)
    }
}

logger := &Logger{prefix: "deploy", level: 0}

// Method value — es un closure que captura logger
infoFn := logger.Info

// Cambiar logger.prefix afecta al method value (pointer receiver)
logger.prefix = "rollback"
infoFn("starting") // [INFO] rollback: starting

// El method value capturo el puntero, no la copia
// Es equivalente a:
// infoFn := func(msg string) { logger.Info(msg) }
```

### Cuando se copia: value receiver

```go
type Counter struct {
    N int
}

func (c Counter) Value() int {
    return c.N // Value receiver: opera sobre copia
}

ctr := Counter{N: 5}
getFn := ctr.Value // Captura COPIA del receptor

ctr.N = 100
fmt.Println(getFn()) // 5 — la copia no cambio
```

---

## 12. Closures con http.Handler

### Handler con estado via closure

```go
func makeStatsHandler() http.Handler {
    var (
        mu       sync.Mutex
        requests int64
        errors   int64
        started  = time.Now()
    )
    
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        mu.Lock()
        requests++
        current := requests
        mu.Unlock()
        
        response := map[string]any{
            "requests": current,
            "errors":   errors,
            "uptime":   time.Since(started).String(),
            "rps":      float64(current) / time.Since(started).Seconds(),
        }
        
        w.Header().Set("Content-Type", "application/json")
        json.NewEncoder(w).Encode(response)
    })
}

// Cada request incrementa el contador compartido via closure
http.Handle("/stats", makeStatsHandler())
```

### Middleware factory con closure

```go
func corsMiddleware(allowedOrigins ...string) func(http.Handler) http.Handler {
    // Pre-computar el set una sola vez
    originSet := make(map[string]bool, len(allowedOrigins))
    for _, o := range allowedOrigins {
        originSet[o] = true
    }
    
    return func(next http.Handler) http.Handler {
        return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
            origin := r.Header.Get("Origin")
            
            // Closure captura originSet (pre-computado)
            if originSet[origin] || originSet["*"] {
                w.Header().Set("Access-Control-Allow-Origin", origin)
                w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE")
                w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")
            }
            
            if r.Method == "OPTIONS" {
                w.WriteHeader(http.StatusNoContent)
                return
            }
            
            next.ServeHTTP(w, r)
        })
    }
}

// El originSet se computa UNA sola vez, no en cada request
cors := corsMiddleware("https://app.example.com", "https://admin.example.com")
http.Handle("/api/", cors(apiHandler))
```

---

## 13. Closures y testing

### Test doubles con closures

```go
type EmailSender struct {
    Send func(to, subject, body string) error
}

// Produccion:
sender := &EmailSender{
    Send: func(to, subject, body string) error {
        return smtp.SendMail("mail:587", auth, "admin@example.com",
            []string{to}, []byte("Subject: "+subject+"\n\n"+body))
    },
}

// Test:
func TestAlertNotification(t *testing.T) {
    var sent []struct{ to, subject, body string }
    
    sender := &EmailSender{
        Send: func(to, subject, body string) error {
            // Closure captura sent — registra cada llamada
            sent = append(sent, struct{ to, subject, body string }{to, subject, body})
            return nil
        },
    }
    
    triggerAlert(sender, "disk full on web01")
    
    if len(sent) != 1 {
        t.Fatalf("expected 1 email, got %d", len(sent))
    }
    if !strings.Contains(sent[0].subject, "disk full") {
        t.Errorf("unexpected subject: %s", sent[0].subject)
    }
}
```

### Closures para simular fallos

```go
func TestRetryOnFailure(t *testing.T) {
    attempts := 0
    
    // Closure como operacion que falla las primeras 2 veces
    operation := func() error {
        attempts++
        if attempts < 3 {
            return fmt.Errorf("simulated failure #%d", attempts)
        }
        return nil
    }
    
    err := retry(3, 0, operation)
    if err != nil {
        t.Fatalf("expected success after retries, got: %v", err)
    }
    if attempts != 3 {
        t.Errorf("expected 3 attempts, got %d", attempts)
    }
}
```

### Closure como spy

```go
func TestMiddleware(t *testing.T) {
    var callLog []string
    
    handler := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        callLog = append(callLog, fmt.Sprintf("%s %s", r.Method, r.URL.Path))
        w.WriteHeader(200)
    })
    
    wrapped := loggingMiddleware(handler)
    
    req := httptest.NewRequest("GET", "/api/health", nil)
    rec := httptest.NewRecorder()
    wrapped.ServeHTTP(rec, req)
    
    if len(callLog) != 1 {
        t.Fatalf("handler called %d times, expected 1", len(callLog))
    }
    if callLog[0] != "GET /api/health" {
        t.Errorf("unexpected call: %s", callLog[0])
    }
}
```

---

## 14. Closures sobre channels

### Productor con closure

```go
func lineProducer(path string) (<-chan string, func()) {
    ch := make(chan string, 100)
    done := make(chan struct{})
    
    go func() {
        defer close(ch)
        f, err := os.Open(path)
        if err != nil {
            log.Printf("cannot open %s: %v", path, err)
            return
        }
        defer f.Close()
        
        scanner := bufio.NewScanner(f)
        for scanner.Scan() {
            select {
            case ch <- scanner.Text():
            case <-done:
                return
            }
        }
    }()
    
    // Closure de cancelacion: captura done
    cancel := func() {
        close(done)
    }
    
    return ch, cancel
}

// Uso:
lines, cancel := lineProducer("/var/log/syslog")
defer cancel()

for line := range lines {
    if strings.Contains(line, "CRITICAL") {
        fmt.Println(line)
        cancel() // Dejar de leer
        break
    }
}
```

### Fan-out con closures

```go
func fanOut(input <-chan string, workers int, process func(string) error) <-chan error {
    errs := make(chan error, workers)
    var wg sync.WaitGroup
    
    for i := 0; i < workers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for item := range input {
                if err := process(item); err != nil {
                    // Closure captura errs e id
                    errs <- fmt.Errorf("worker %d: %w", id, err)
                }
            }
        }(i)
    }
    
    // Closure que cierra el channel cuando todos terminan
    go func() {
        wg.Wait()
        close(errs)
    }()
    
    return errs
}
```

---

## 15. Errores comunes con closures

### Error 1: Capturar puntero a struct temporal

```go
type Task struct {
    Name string
}

// ✗ Bug sutil:
tasks := []Task{{"backup"}, {"deploy"}, {"monitor"}}
var fns []func()
for _, task := range tasks {
    fns = append(fns, func() {
        fmt.Println(task.Name)
    })
}
// Pre-Go 1.22: todas imprimen "monitor"
// Go 1.22+: funciona correctamente

// ✓ Seguro en todas las versiones:
for _, task := range tasks {
    t := task // Copia local
    fns = append(fns, func() {
        fmt.Println(t.Name)
    })
}
```

### Error 2: Closure que captura variable de scope externo sin darse cuenta

```go
func setupHandlers(db *sql.DB) {
    var lastError error // Variable compartida por todos los handlers
    
    http.HandleFunc("/api/users", func(w http.ResponseWriter, r *http.Request) {
        rows, err := db.Query("SELECT * FROM users")
        if err != nil {
            lastError = err // ✗ Race condition!
            // Multiples goroutines escriben la misma variable
            http.Error(w, err.Error(), 500)
            return
        }
        defer rows.Close()
        // ...
    })
    
    http.HandleFunc("/api/status", func(w http.ResponseWriter, r *http.Request) {
        if lastError != nil { // ✗ Race condition!
            fmt.Fprintf(w, "last error: %v", lastError)
        }
    })
}

// ✓ Solucion: proteger con mutex o usar otro mecanismo
```

### Error 3: Closure en defer dentro de loop

```go
func processFiles(paths []string) error {
    // ✗ Peligro: los defers se acumulan hasta que la funcion retorna
    for _, path := range paths {
        f, err := os.Open(path)
        if err != nil {
            return err
        }
        defer f.Close() // ¡No se ejecuta hasta el final de la funcion!
        // Si paths tiene 10000 archivos, 10000 file descriptors abiertos
    }
    
    // ✓ Solucion: extraer a funcion
    for _, path := range paths {
        if err := processOneFile(path); err != nil {
            return err
        }
    }
    return nil
}

func processOneFile(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close() // Se ejecuta al salir de ESTA funcion
    // ...
    return nil
}

// ✓ Alternativa: IIFE
for _, path := range paths {
    err := func() error {
        f, err := os.Open(path)
        if err != nil {
            return err
        }
        defer f.Close()
        // ...
        return nil
    }()
    if err != nil {
        return err
    }
}
```

### Error 4: Closure que modifica variable despues de retornar

```go
func getHandlers() []http.HandlerFunc {
    prefix := "/api/v1"
    
    handlers := []http.HandlerFunc{
        func(w http.ResponseWriter, r *http.Request) {
            // Closure sobre prefix
            fmt.Fprintf(w, "prefix: %s", prefix)
        },
    }
    
    prefix = "/api/v2" // Modificar despues de crear el closure
    // ✗ Todos los closures ahora ven "/api/v2"
    
    return handlers
}

// ✓ Solucion: capturar el valor, no la variable
func getHandlers() []http.HandlerFunc {
    prefix := "/api/v1"
    capturedPrefix := prefix // Copia
    
    handlers := []http.HandlerFunc{
        func(w http.ResponseWriter, r *http.Request) {
            fmt.Fprintf(w, "prefix: %s", capturedPrefix) // Usa la copia
        },
    }
    
    prefix = "/api/v2" // No afecta al closure
    return handlers
}
```

---

## 16. Comparacion: Go vs C vs Rust

### Go: closures por referencia

```go
func makeAccumulator() func(int) int {
    sum := 0
    return func(n int) int {
        sum += n  // Captura sum por referencia
        return sum
    }
}

acc := makeAccumulator()
acc(5)  // 5
acc(3)  // 8
acc(10) // 18
```

### C: sin closures (workarounds manuales)

```c
// C no tiene closures. Workarounds:

// 1. Puntero a funcion + void* userdata
typedef int (*AccFunc)(void *data, int n);

struct AccData {
    int sum;
};

int accumulate(void *data, int n) {
    struct AccData *d = (struct AccData *)data;
    d->sum += n;
    return d->sum;
}

// Uso:
struct AccData data = {0};
accumulate(&data, 5);  // 5
accumulate(&data, 3);  // 8

// 2. Variables estaticas (pero no son reentrantes)
int accumulate_static(int n) {
    static int sum = 0;  // Solo una instancia
    sum += n;
    return sum;
}

// 3. Apple Blocks (extension no estandar, solo Clang)
// int (^acc)(int) = ^(int n) { sum += n; return sum; };

// Conclusion: C requiere manejo manual del estado
// El programador es responsable del lifetime y thread safety
```

### Rust: closures con ownership explicito

```rust
fn make_accumulator() -> impl FnMut(i32) -> i32 {
    let mut sum = 0;
    move |n| {  // move: toma ownership de sum
        sum += n;  // El closure es el unico dueno
        sum
    }
}

let mut acc = make_accumulator();
acc(5);  // 5
acc(3);  // 8
acc(10); // 18

// Rust tiene 3 traits de closure:
// Fn:     captura por referencia inmutable (&T)
// FnMut:  captura por referencia mutable (&mut T)  
// FnOnce: captura por valor (consume — solo se llama una vez)

// El compilador elige el trait mas restrictivo posible:
let name = String::from("hello");

// Fn: solo lee
let greet = || println!("{}", name);
greet();
greet(); // OK — puede llamarse multiples veces

// FnMut: modifica
let mut count = 0;
let mut inc = || { count += 1; count };
inc();
inc(); // OK — requiere mut

// FnOnce: consume
let data = vec![1, 2, 3];
let consume = move || {
    drop(data); // Toma ownership y destruye
};
consume();
// consume(); // ERROR: ya fue consumido
```

### Tabla comparativa

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────────┐
│ Aspecto              │ Go                   │ C                    │ Rust                     │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────────┤
│ Closures nativos     │ Si                   │ No                   │ Si                       │
│ Captura              │ Por referencia       │ N/A (manual)         │ Ref, mut ref, o move     │
│ Mutabilidad          │ Siempre mutable      │ N/A                  │ Fn vs FnMut vs FnOnce    │
│ Ownership            │ GC maneja lifetime   │ Manual               │ Ownership system         │
│ Thread safety        │ Responsabilidad del  │ Manual               │ Send + Sync en compile   │
│                      │ programador (mutex)  │                      │ time                     │
│ Escape al heap       │ Si (escape analysis) │ N/A (stack manual)   │ Raro (Box<dyn Fn>)       │
│ Performance          │ Heap alloc posible   │ Zero cost (manual)   │ Zero cost (monomorphize) │
│ Loop variable bug    │ Si (pre-1.22)        │ N/A                  │ No (ownership previene)  │
│ Closures recursivos  │ Via variable         │ N/A                  │ Box<dyn Fn> o workaround │
│ Inferencia de tipo   │ Si (parcial)         │ N/A                  │ Si (completa)            │
│ Generic closures     │ Si (Go 1.18+)        │ N/A                  │ Si (trait bounds)        │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────────┘
```

**Key insight**: Go elige simplicidad — todas las capturas son por referencia, y el GC se encarga del lifetime. Rust elige seguridad — el sistema de tipos te obliga a ser explicito sobre como capturas y quien es dueno. C no tiene closures, asi que simulas el patron con punteros a funcion y datos separados.

---

## 17. Closures recursivos

### El problema

```go
// ✗ ERROR: no puedes referenciar una variable en su propia declaracion
// fib := func(n int) int {
//     if n <= 1 { return n }
//     return fib(n-1) + fib(n-2) // fib no esta definida aun
// }

// ✓ Solucion: declarar la variable primero
var fib func(n int) int
fib = func(n int) int {
    if n <= 1 {
        return n
    }
    return fib(n-1) + fib(n-2) // Ahora fib ya existe
}

fmt.Println(fib(10)) // 55
```

### Uso practico: recorrer un arbol de directorios

```go
var walk func(path string, depth int)
walk = func(path string, depth int) {
    entries, err := os.ReadDir(path)
    if err != nil {
        log.Printf("cannot read %s: %v", path, err)
        return
    }
    
    indent := strings.Repeat("  ", depth)
    for _, entry := range entries {
        if strings.HasPrefix(entry.Name(), ".") {
            continue // Saltar dotfiles
        }
        
        fullPath := filepath.Join(path, entry.Name())
        if entry.IsDir() {
            fmt.Printf("%s%s/\n", indent, entry.Name())
            walk(fullPath, depth+1) // Recursion via closure
        } else {
            info, _ := entry.Info()
            fmt.Printf("%s%s (%d bytes)\n", indent, entry.Name(), info.Size())
        }
    }
}

walk("/etc/nginx", 0)
```

---

## 18. Programa completo: Task Scheduler con Closures

Un scheduler de tareas cron-like que usa closures extensivamente para definir tareas, condiciones de ejecucion, manejo de errores, y notificaciones.

```go
package main

import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"runtime"
	"strings"
	"sync"
	"time"
)

// ─── Tipos core ─────────────────────────────────────────────────

type TaskFunc func(ctx context.Context) error

type Task struct {
	Name      string
	Run       TaskFunc
	Schedule  func(now time.Time) bool  // Closure: ¿ejecutar ahora?
	OnSuccess func(name string, dur time.Duration)
	OnFailure func(name string, err error, dur time.Duration)
	Timeout   time.Duration
}

type TaskResult struct {
	Name     string        `json:"name"`
	Status   string        `json:"status"`
	Duration time.Duration `json:"duration_ms"`
	Error    string        `json:"error,omitempty"`
	RunAt    time.Time     `json:"run_at"`
}

// ─── Schedule factories (closures que definen cuando ejecutar) ──

func Every(d time.Duration) func(time.Time) bool {
	lastRun := time.Time{}
	var mu sync.Mutex
	return func(now time.Time) bool {
		mu.Lock()
		defer mu.Unlock()
		if now.Sub(lastRun) >= d {
			lastRun = now
			return true
		}
		return false
	}
}

func EveryMinuteAt(second int) func(time.Time) bool {
	var lastMin int = -1
	return func(now time.Time) bool {
		if now.Second() == second && now.Minute() != lastMin {
			lastMin = now.Minute()
			return true
		}
		return false
	}
}

func EveryHourAt(minute, second int) func(time.Time) bool {
	var lastHour int = -1
	return func(now time.Time) bool {
		if now.Minute() == minute && now.Second() == second && now.Hour() != lastHour {
			lastHour = now.Hour()
			return true
		}
		return false
	}
}

func OnWeekdays(days ...time.Weekday) func(time.Time) bool {
	daySet := make(map[time.Weekday]bool)
	for _, d := range days {
		daySet[d] = true
	}
	return func(now time.Time) bool {
		return daySet[now.Weekday()]
	}
}

func And(conditions ...func(time.Time) bool) func(time.Time) bool {
	return func(now time.Time) bool {
		for _, cond := range conditions {
			if !cond(now) {
				return false
			}
		}
		return true
	}
}

func Unless(condition func(time.Time) bool, except func(time.Time) bool) func(time.Time) bool {
	return func(now time.Time) bool {
		return condition(now) && !except(now)
	}
}

// ─── Task factories (closures que definen que ejecutar) ─────────

func ShellTask(command string, args ...string) TaskFunc {
	return func(ctx context.Context) error {
		cmd := exec.CommandContext(ctx, command, args...)
		output, err := cmd.CombinedOutput()
		if err != nil {
			return fmt.Errorf("%s: %w\noutput: %s", command, err, string(output))
		}
		if len(output) > 0 {
			log.Printf("[task] %s output: %s", command, strings.TrimSpace(string(output)))
		}
		return nil
	}
}

func HealthCheckTask(name, addr string) TaskFunc {
	consecutiveFailures := 0 // Estado persistente via closure
	return func(ctx context.Context) error {
		conn, err := (&net.Dialer{Timeout: 5 * time.Second}).DialContext(ctx, "tcp", addr)
		if err != nil {
			consecutiveFailures++
			return fmt.Errorf("%s unreachable (%d consecutive failures): %w",
				name, consecutiveFailures, err)
		}
		conn.Close()
		if consecutiveFailures > 0 {
			log.Printf("[task] %s recovered after %d failures", name, consecutiveFailures)
			consecutiveFailures = 0
		}
		return nil
	}
}

func DiskCheckTask(mountpoint string, thresholdPct float64) TaskFunc {
	return func(ctx context.Context) error {
		cmd := exec.CommandContext(ctx, "df", "--output=pcent", mountpoint)
		output, err := cmd.Output()
		if err != nil {
			return fmt.Errorf("df %s: %w", mountpoint, err)
		}
		lines := strings.Split(strings.TrimSpace(string(output)), "\n")
		if len(lines) < 2 {
			return fmt.Errorf("unexpected df output for %s", mountpoint)
		}
		pctStr := strings.TrimSpace(strings.TrimSuffix(lines[1], "%"))
		var pct float64
		fmt.Sscanf(pctStr, "%f", &pct)
		if pct > thresholdPct {
			return fmt.Errorf("disk %s at %.0f%% (threshold: %.0f%%)",
				mountpoint, pct, thresholdPct)
		}
		return nil
	}
}

func MemoryCheckTask(thresholdPct float64) TaskFunc {
	return func(ctx context.Context) error {
		data, err := os.ReadFile("/proc/meminfo")
		if err != nil {
			return err
		}
		info := make(map[string]float64)
		for _, line := range strings.Split(string(data), "\n") {
			fields := strings.Fields(line)
			if len(fields) >= 2 {
				key := strings.TrimSuffix(fields[0], ":")
				var val float64
				fmt.Sscanf(fields[1], "%f", &val)
				info[key] = val
			}
		}
		total := info["MemTotal"]
		available := info["MemAvailable"]
		if total == 0 {
			return fmt.Errorf("cannot read MemTotal")
		}
		usedPct := ((total - available) / total) * 100
		if usedPct > thresholdPct {
			return fmt.Errorf("memory at %.1f%% (threshold: %.0f%%)", usedPct, thresholdPct)
		}
		return nil
	}
}

func CleanupTask(dir string, maxAge time.Duration, pattern string) TaskFunc {
	return func(ctx context.Context) error {
		entries, err := os.ReadDir(dir)
		if err != nil {
			return err
		}
		cutoff := time.Now().Add(-maxAge)
		var removed int
		for _, entry := range entries {
			if entry.IsDir() {
				continue
			}
			matched, _ := filepath.Match(pattern, entry.Name())
			if !matched {
				continue
			}
			info, err := entry.Info()
			if err != nil {
				continue
			}
			if info.ModTime().Before(cutoff) {
				path := filepath.Join(dir, entry.Name())
				if err := os.Remove(path); err != nil {
					log.Printf("[cleanup] cannot remove %s: %v", path, err)
				} else {
					removed++
				}
			}
		}
		if removed > 0 {
			log.Printf("[cleanup] removed %d files from %s", removed, dir)
		}
		return nil
	}
}

// ─── Callback factories (closures para notificaciones) ──────────

func LogOnSuccess() func(string, time.Duration) {
	return func(name string, dur time.Duration) {
		log.Printf("[OK] %s completed in %v", name, dur)
	}
}

func LogOnFailure() func(string, error, time.Duration) {
	return func(name string, err error, dur time.Duration) {
		log.Printf("[FAIL] %s failed after %v: %v", name, dur, err)
	}
}

func ThrottledNotifier(cooldown time.Duration, notify func(string, error)) func(string, error, time.Duration) {
	lastNotified := make(map[string]time.Time) // Closure captura este map
	var mu sync.Mutex
	return func(name string, err error, dur time.Duration) {
		mu.Lock()
		defer mu.Unlock()
		if time.Since(lastNotified[name]) < cooldown {
			return // Demasiado pronto, suprimir
		}
		lastNotified[name] = time.Now()
		notify(name, err)
	}
}

func CountingNotifier() (func(string, error, time.Duration), func() map[string]int) {
	counts := make(map[string]int) // Estado compartido via closure
	var mu sync.Mutex

	notifier := func(name string, err error, dur time.Duration) {
		mu.Lock()
		counts[name]++
		mu.Unlock()
		log.Printf("[FAIL #%d] %s: %v", counts[name], name, err)
	}

	getter := func() map[string]int {
		mu.Lock()
		defer mu.Unlock()
		cpy := make(map[string]int, len(counts))
		for k, v := range counts {
			cpy[k] = v
		}
		return cpy
	}

	return notifier, getter
}

// ─── Scheduler ──────────────────────────────────────────────────

type Scheduler struct {
	tasks   []Task
	results []TaskResult
	mu      sync.Mutex
}

func NewScheduler() *Scheduler {
	return &Scheduler{}
}

func (s *Scheduler) Register(tasks ...Task) {
	s.tasks = append(s.tasks, tasks...)
}

func (s *Scheduler) tick(now time.Time) {
	for _, task := range s.tasks {
		if task.Schedule != nil && !task.Schedule(now) {
			continue
		}

		go func(t Task) {
			timeout := t.Timeout
			if timeout == 0 {
				timeout = 30 * time.Second
			}
			ctx, cancel := context.WithTimeout(context.Background(), timeout)
			defer cancel()

			start := time.Now()
			err := t.Run(ctx)
			dur := time.Since(start)

			result := TaskResult{
				Name:     t.Name,
				Duration: dur,
				RunAt:    now,
			}

			if err != nil {
				result.Status = "FAIL"
				result.Error = err.Error()
				if t.OnFailure != nil {
					t.OnFailure(t.Name, err, dur)
				}
			} else {
				result.Status = "OK"
				if t.OnSuccess != nil {
					t.OnSuccess(t.Name, dur)
				}
			}

			s.mu.Lock()
			s.results = append(s.results, result)
			// Mantener solo ultimos 1000 resultados
			if len(s.results) > 1000 {
				s.results = s.results[len(s.results)-1000:]
			}
			s.mu.Unlock()
		}(task)
	}
}

func (s *Scheduler) Run(ctx context.Context) {
	ticker := time.NewTicker(1 * time.Second)
	defer ticker.Stop()

	log.Printf("[scheduler] started with %d tasks", len(s.tasks))

	for {
		select {
		case <-ctx.Done():
			log.Println("[scheduler] shutting down")
			return
		case now := <-ticker.C:
			s.tick(now)
		}
	}
}

func (s *Scheduler) Results() []TaskResult {
	s.mu.Lock()
	defer s.mu.Unlock()
	cpy := make([]TaskResult, len(s.results))
	copy(cpy, s.results)
	return cpy
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	log.SetFlags(log.Ltime | log.Lmicroseconds)
	sched := NewScheduler()

	// Notificadores con estado (closures)
	failNotifier, getFailCounts := CountingNotifier()
	throttled := ThrottledNotifier(5*time.Minute, func(name string, err error) {
		log.Printf("[THROTTLED ALERT] %s: %v", name, err)
	})

	// Registrar tareas — cada una definida con closures
	sched.Register(
		Task{
			Name:      "memory-check",
			Run:       MemoryCheckTask(90.0),
			Schedule:  Every(30 * time.Second),
			OnSuccess: LogOnSuccess(),
			OnFailure: failNotifier,
			Timeout:   10 * time.Second,
		},
		Task{
			Name:      "disk-check-root",
			Run:       DiskCheckTask("/", 85.0),
			Schedule:  Every(1 * time.Minute),
			OnSuccess: LogOnSuccess(),
			OnFailure: throttled,
			Timeout:   10 * time.Second,
		},
		Task{
			Name: "cleanup-tmp",
			Run:  CleanupTask(os.TempDir(), 24*time.Hour, "*.tmp"),
			Schedule: And(
				Every(1*time.Hour),
				OnWeekdays(time.Monday, time.Wednesday, time.Friday),
			),
			OnSuccess: LogOnSuccess(),
			OnFailure: failNotifier,
			Timeout:   2 * time.Minute,
		},
		Task{
			Name: "system-stats",
			Run: func(ctx context.Context) error {
				var mem runtime.MemStats
				runtime.ReadMemStats(&mem)
				log.Printf("[stats] goroutines=%d heap=%dMB sys=%dMB",
					runtime.NumGoroutine(),
					mem.HeapAlloc/1024/1024,
					mem.Sys/1024/1024,
				)
				return nil
			},
			Schedule:  Every(15 * time.Second),
			OnSuccess: nil, // Silencioso en exito
			OnFailure: LogOnFailure(),
			Timeout:   5 * time.Second,
		},
	)

	// Contexto con cancelacion
	ctx, cancel := context.WithCancel(context.Background())
	defer cancel()

	// Goroutine para mostrar failure counts cada minuto
	go func() {
		ticker := time.NewTicker(1 * time.Minute)
		defer ticker.Stop()
		for {
			select {
			case <-ctx.Done():
				return
			case <-ticker.C:
				counts := getFailCounts() // Closure retorna el estado
				if len(counts) > 0 {
					data, _ := json.Marshal(counts)
					log.Printf("[scheduler] failure counts: %s", data)
				}
			}
		}
	}()

	// Ejecutar scheduler (bloqueante)
	log.Println("Task scheduler starting... (Ctrl+C to stop)")

	// En produccion usarias signal handling; aqui ejecutamos 2 minutos como demo
	demoCtx, demoCancel := context.WithTimeout(ctx, 2*time.Minute)
	defer demoCancel()

	sched.Run(demoCtx)

	// Mostrar resultados finales
	results := sched.Results()
	log.Printf("[scheduler] completed with %d results", len(results))
	for _, r := range results {
		status := "OK"
		if r.Error != "" {
			status = "FAIL: " + r.Error
		}
		log.Printf("  %s: %s (%v)", r.Name, status, r.Duration)
	}
}
```

### Nota: imports faltantes

El programa usa `net`, `path/filepath` y algunos otros. Los imports completos serian:

```go
import (
	"context"
	"encoding/json"
	"fmt"
	"log"
	"net"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"strings"
	"sync"
	"time"
)
```

### Mapa de closures en el programa

```
┌──────────────────────────────────────────────────────────────┐
│  CLOSURES EN EL TASK SCHEDULER                               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Schedule closures (¿cuando ejecutar?):                      │
│  ├─ Every(d)         → captura lastRun, mu                   │
│  ├─ EveryMinuteAt(s) → captura lastMin                       │
│  ├─ EveryHourAt(m,s) → captura lastHour                      │
│  ├─ OnWeekdays(...)  → captura daySet (pre-computado)        │
│  ├─ And(conds...)    → captura slice de closures             │
│  └─ Unless(a, b)     → captura dos closures                  │
│                                                              │
│  Task closures (¿que ejecutar?):                             │
│  ├─ ShellTask(cmd)       → captura command, args             │
│  ├─ HealthCheckTask(addr)→ captura consecutiveFailures       │
│  ├─ DiskCheckTask(mp)    → captura mountpoint, threshold     │
│  ├─ MemoryCheckTask(t)   → captura thresholdPct              │
│  └─ CleanupTask(dir)     → captura dir, maxAge, pattern      │
│                                                              │
│  Notification closures (¿como notificar?):                   │
│  ├─ LogOnSuccess()       → stateless (solo log)              │
│  ├─ LogOnFailure()       → stateless (solo log)              │
│  ├─ ThrottledNotifier()  → captura lastNotified map, mu      │
│  └─ CountingNotifier()   → captura counts map, mu            │
│     └─ Retorna 2 closures que comparten el MISMO map         │
│                                                              │
│  Composicion:                                                │
│  And(Every(1h), OnWeekdays(Mon,Wed,Fri))                     │
│   → Un closure que contiene otros closures                   │
│   → Cada uno mantiene su propio estado independiente         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 19. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
x := 10
fn := func() { fmt.Println(x) }
x = 20
fn()
```

<details>
<summary>Respuesta</summary>

```
20
```

El closure captura `x` por referencia. Cuando se ejecuta `fn()`, lee el valor actual de `x`, que es 20.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
fn := func(x int) func() int {
    return func() int {
        x++
        return x
    }
}

a := fn(0)
b := fn(0)
fmt.Println(a(), a(), b(), a())
```

<details>
<summary>Respuesta</summary>

```
1 2 1 3
```

Cada llamada a `fn(0)` crea una nueva variable `x` (parametro). `a` y `b` tienen closures independientes sobre variables `x` distintas. `a()` → 1, `a()` → 2, `b()` → 1 (su propia x), `a()` → 3.
</details>

**Ejercicio 3**: ¿Que imprime (Go 1.22+)?

```go
fns := make([]func(), 3)
for i := range 3 {
    fns[i] = func() { fmt.Print(i, " ") }
}
for _, fn := range fns {
    fn()
}
```

<details>
<summary>Respuesta</summary>

```
0 1 2 
```

Go 1.22+: cada iteracion tiene su propia variable `i`. Cada closure captura una `i` diferente.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
func make() (func(), func() int) {
    n := 0
    return func() { n++ }, func() int { return n }
}

inc, get := make()
inc()
inc()
inc()
fmt.Println(get())
```

<details>
<summary>Respuesta</summary>

```
3
```

Ambos closures comparten la misma variable `n`. `inc` la incrementa 3 veces, `get` la lee.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
x := 0
defer func() { fmt.Println("defer:", x) }()
x = 42
fmt.Println("main:", x)
```

<details>
<summary>Respuesta</summary>

```
main: 42
defer: 42
```

El defer con closure lee `x` al momento de ejecutarse (al salir de la funcion), cuando `x` vale 42.
</details>

**Ejercicio 6**: ¿Que imprime?

```go
x := 0
defer fmt.Println("defer:", x)
x = 42
fmt.Println("main:", x)
```

<details>
<summary>Respuesta</summary>

```
main: 42
defer: 0
```

Sin closure: `fmt.Println` recibe `x` como argumento, que se evalua al registrar el defer (x=0). Contraste con el ejercicio 5.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
adders := make([]func(int) int, 3)
for i := range 3 {
    base := i * 10
    adders[i] = func(n int) int { return base + n }
}
fmt.Println(adders[0](5), adders[1](5), adders[2](5))
```

<details>
<summary>Respuesta</summary>

```
5 15 25
```

Cada iteracion crea una nueva variable `base` (0, 10, 20). Cada closure captura su propio `base`. `0+5=5`, `10+5=15`, `20+5=25`.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
type S struct{ N int }

s := S{N: 1}
fn := func() { fmt.Println(s.N) }
s.N = 2
fn()
s = S{N: 3}
fn()
```

<details>
<summary>Respuesta</summary>

```
2
3
```

El closure captura la variable `s` por referencia (no una copia). Cualquier modificacion a `s` (tanto campo como reasignacion completa) es visible para el closure.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
func wrap(f func()) func() {
    return func() {
        fmt.Print("before ")
        f()
        fmt.Print(" after")
    }
}

f := func() { fmt.Print("hello") }
g := wrap(f)
f = func() { fmt.Print("world") }
g()
```

<details>
<summary>Respuesta</summary>

```
before hello after
```

`wrap` captura el valor de `f` al momento de la llamada (es un parametro, no una variable capturada del scope externo). Reasignar `f` despues no afecta al closure de `wrap`, que ya recibio la funcion original como argumento.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
var fs []func()
for i := 0; i < 3; i++ {
    i := i
    fs = append(fs, func() { fmt.Print(i, " ") })
    i += 10
}
for _, f := range fs {
    f()
}
```

<details>
<summary>Respuesta</summary>

```
10 11 12 
```

`i := i` crea una variable local (shadow). Luego `i += 10` modifica esa variable local. El closure la captura por referencia y ve el valor modificado. Iteracion 0: i=0, i+=10 → 10. Iteracion 1: i=1, i+=10 → 11. Iteracion 2: i=2, i+=10 → 12.
</details>

---

## 20. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              CLOSURES — RESUMEN                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  MECANICA                                                    │
│  ├─ Captura por REFERENCIA (no copia)                        │
│  ├─ Closure y creador comparten la MISMA variable            │
│  ├─ Si la variable escapa, se mueve al heap                  │
│  └─ go build -gcflags="-m" para verificar escape             │
│                                                              │
│  LOOP VARIABLE (EL BUG CLASICO)                              │
│  ├─ Pre-Go 1.22: una variable compartida por todas las       │
│  │   iteraciones → todos los closures ven el ultimo valor    │
│  ├─ Go 1.22+: variable por iteracion (fix en el lenguaje)    │
│  ├─ go.mod "go 1.22" activa el nuevo comportamiento          │
│  └─ i := i o pasar como argumento: seguro en todas versiones │
│                                                              │
│  GOROUTINES + CLOSURES                                       │
│  ├─ Pasar como argumento: go func(h string) { ... }(host)   │
│  ├─ Variable local: svc := svc antes del go func()          │
│  ├─ Proteger estado compartido: sync.Mutex                   │
│  └─ Channels para resultados                                 │
│                                                              │
│  DEFER + CLOSURE                                             │
│  ├─ defer func() { ... }() → lee variables al EJECUTARSE     │
│  ├─ defer fn(x) → evalua x al REGISTRARSE                   │
│  └─ Closure sobre named return: puede modificar el error     │
│                                                              │
│  PATRONES                                                    │
│  ├─ Generador: makeCounter() → func() int                   │
│  ├─ Iterator: lineIterator() → func() (string, bool)        │
│  ├─ Config watcher: watchConfig() → func() Config            │
│  ├─ Rate limiter: captura tokens + lastReset                 │
│  ├─ Circuit breaker: captura failures + state                │
│  ├─ Connection pool: captura chan + factory                   │
│  ├─ Backoff: captura current delay                           │
│  ├─ Throttled notifier: captura lastNotified map             │
│  └─ Counting notifier: 2 closures compartiendo 1 map        │
│                                                              │
│  TESTING                                                     │
│  ├─ Mock con closure: captura llamadas en slice              │
│  ├─ Fallo simulado: closure con counter de intentos          │
│  └─ Spy: closure que registra interacciones                  │
│                                                              │
│  RECURSION                                                   │
│  ├─ var fn func(...)                                         │
│  ├─ fn = func(...) { ... fn(...) ... }                       │
│  └─ No se puede usar := (variable no existe aun)             │
│                                                              │
│  ERRORES COMUNES                                             │
│  ├─ Loop var capturada (pre-1.22)                            │
│  ├─ Race condition: closure accede a variable sin mutex      │
│  ├─ defer en loop: closures acumulan resources               │
│  └─ Modificar variable despues de crear closure              │
│                                                              │
│  GO vs C vs RUST                                             │
│  ├─ Go: captura por ref, GC maneja lifetime, simple          │
│  ├─ C: sin closures (void* userdata manual)                  │
│  └─ Rust: Fn/FnMut/FnOnce, ownership explicito, zero-cost   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T02 - init() — funcion especial, orden de ejecucion, multiples init() por archivo, cuando usarla (raro)
