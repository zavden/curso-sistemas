# Variadic Functions — ...T

## 1. Introduccion

Las funciones variadicas aceptan un **numero variable de argumentos** del mismo tipo. Ya las has usado: `fmt.Println`, `fmt.Sprintf`, `append` y `log.Printf` son todas variadicas. Este topico cubre la mecanica completa — como se declaran, como se reciben los argumentos, y por que `...` tiene dos significados distintos segun donde aparece.

En SysAdmin/DevOps, las funciones variadicas son criticas para construir herramientas flexibles: loggers que aceptan campos variables, checkers que prueban N hosts, builders de queries con filtros opcionales, y wrappers de comandos con argumentos dinamicos.

```
┌──────────────────────────────────────────────────────────────┐
│              VARIADIC FUNCTIONS EN GO                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Declaracion:                                                │
│  func name(fixed T1, variadic ...T2) R { }                   │
│                      ↑                                       │
│                  ultimo parametro siempre                     │
│                                                              │
│  Dos usos de "...":                                          │
│  ├─ En declaracion: ...T → "acepto 0 o mas T"               │
│  │   func f(nums ...int)    // nums es []int                 │
│  │                                                           │
│  └─ En llamada: slice... → "expande este slice"              │
│     s := []int{1,2,3}                                        │
│     f(s...)                 // pasa s como variadic           │
│                                                              │
│  Internamente:                                               │
│  ├─ Los argumentos se empaquetan en un []T (slice)           │
│  ├─ Si pasas 0 argumentos → slice nil (no vacio)             │
│  ├─ Si pasas slice... → NO se copia, se reutiliza            │
│  └─ Solo el ultimo parametro puede ser variadico             │
│                                                              │
│  Stdlib que usa variadics:                                    │
│  ├─ fmt.Println(a ...any)                                    │
│  ├─ fmt.Sprintf(format string, a ...any)                     │
│  ├─ append(slice []T, elems ...T) []T                        │
│  ├─ log.Printf(format string, v ...any)                      │
│  ├─ errors.Join(errs ...error) error                         │
│  └─ filepath.Join(elem ...string) string                     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Sintaxis basica

### Declaracion de una funcion variadica

```go
// El parametro variadico lleva "..." antes del tipo
func sum(nums ...int) int {
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
}

// Llamadas validas:
sum()           // nums es nil, total = 0
sum(1)          // nums es []int{1}
sum(1, 2, 3)    // nums es []int{1, 2, 3}
sum(1, 2, 3, 4) // nums es []int{1, 2, 3, 4}
```

### Parametros fijos + variadico

```go
// Los parametros fijos van ANTES del variadico
func logMessage(level string, parts ...string) {
    msg := strings.Join(parts, " ")
    fmt.Printf("[%s] %s\n", level, msg)
}

logMessage("INFO", "server", "started", "on", "port", "8080")
// [INFO] server started on port 8080

logMessage("ERROR")
// [ERROR] 
```

### Restriccion: solo el ultimo parametro

```go
// ✗ ERROR de compilacion — variadico debe ser el ultimo
func bad(nums ...int, name string) {}
// syntax error: can only use ... with final parameter in list

// ✗ ERROR — no puedes tener dos variadicos
func worse(a ...int, b ...string) {}

// ✓ Correcto
func good(name string, port int, tags ...string) {}
```

---

## 3. Mecanica interna: el slice subyacente

### Los argumentos se empaquetan en un slice

```go
func inspect(nums ...int) {
    fmt.Printf("Type: %T\n", nums)
    fmt.Printf("Value: %v\n", nums)
    fmt.Printf("Len: %d, Cap: %d\n", len(nums), cap(nums))
    fmt.Printf("nil? %t\n", nums == nil)
}

inspect()        // Type: []int, Value: [], Len: 0, Cap: 0, nil? true
inspect(1)       // Type: []int, Value: [1], Len: 1, Cap: 1, nil? false
inspect(1, 2, 3) // Type: []int, Value: [1 2 3], Len: 3, Cap: 3, nil? false
```

**Punto clave**: con 0 argumentos, el slice es `nil`, no un slice vacio (`[]int{}`). Esto importa cuando haces checks de nil.

### Diferencia: nil vs empty slice

```go
func process(items ...string) {
    if items == nil {
        fmt.Println("No items provided (nil)")
        return
    }
    if len(items) == 0 {
        fmt.Println("Empty slice provided")
        return
    }
    fmt.Printf("Processing %d items\n", len(items))
}

process()                   // "No items provided (nil)"
process([]string{}...)      // "Empty slice provided"
process("a", "b")           // "Processing 2 items"
```

---

## 4. Paso de slices con `...` (spread/unpack)

### Expandir un slice existente

```go
func sum(nums ...int) int {
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
}

numbers := []int{10, 20, 30}

// ✗ ERROR: no puedes pasar un slice directamente
// sum(numbers)  // cannot use numbers (variable of type []int) as int value

// ✓ Usa ... para expandir el slice
result := sum(numbers...)  // 60
```

### Cuando se usa `...` NO se copia el slice

```go
func modify(nums ...int) {
    if len(nums) > 0 {
        nums[0] = 999  // Modifica el slice original!
    }
}

original := []int{1, 2, 3}
modify(original...)
fmt.Println(original) // [999 2 3] — ¡modificado!

// Compara con paso sin ...:
modify(1, 2, 3)       // Crea un nuevo slice — original no se toca
```

**Esto es critico**: cuando usas `slice...`, la funcion recibe **el mismo backing array**. Si pasas argumentos individuales, Go crea un nuevo slice. Esto tiene implicaciones de seguridad y correctitud.

### Pasar slice con seguridad (copia defensiva)

```go
original := []int{1, 2, 3}

// Opcion 1: copia antes de pasar
cpy := make([]int, len(original))
copy(cpy, original)
modify(cpy...)

// Opcion 2: slicing con cap limitado (Go 1.2+)
// Esto fuerza un nuevo backing array si se hace append
modify(original[:len(original):len(original)]...)

// Opcion 3: append a nil (crea copia)
modify(append([]int(nil), original...)...)
```

---

## 5. `any` (interface{}) como tipo variadico

### fmt.Println — el ejemplo canonico

```go
// Firma real en la stdlib:
// func Println(a ...any) (n int, err error)

fmt.Println("host:", hostname, "port:", port, "status:", status)
// Acepta cualquier tipo, cualquier cantidad
```

### Construir una funcion con ...any

```go
func logFields(msg string, fields ...any) {
    if len(fields)%2 != 0 {
        fmt.Fprintf(os.Stderr, "logFields: odd number of fields\n")
        return
    }
    
    b := strings.Builder{}
    b.WriteString(msg)
    
    for i := 0; i < len(fields); i += 2 {
        key, ok := fields[i].(string)
        if !ok {
            fmt.Fprintf(os.Stderr, "logFields: key must be string, got %T\n", fields[i])
            return
        }
        b.WriteString(fmt.Sprintf(" %s=%v", key, fields[i+1]))
    }
    
    fmt.Println(b.String())
}

logFields("request completed",
    "method", "GET",
    "path", "/api/v1/nodes",
    "status", 200,
    "duration_ms", 42,
)
// request completed method=GET path=/api/v1/nodes status=200 duration_ms=42
```

### Tipado seguro vs ...any

```go
// ✗ Pierde type safety — errores en runtime
func addAnys(nums ...any) int {
    total := 0
    for _, n := range nums {
        total += n.(int) // panic si no es int
    }
    return total
}

// ✓ Mejor: tipo concreto
func addInts(nums ...int) int {
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
}

// ✓ Mejor aun (Go 1.18+): generics
func addNumbers[T int | int64 | float64](nums ...T) T {
    var total T
    for _, n := range nums {
        total += n
    }
    return total
}
```

---

## 6. Patrones SysAdmin con variadics

### Patron 1: Multi-host checker

```go
func checkHosts(port int, hosts ...string) map[string]error {
    results := make(map[string]error, len(hosts))
    for _, host := range hosts {
        addr := fmt.Sprintf("%s:%d", host, port)
        conn, err := net.DialTimeout("tcp", addr, 2*time.Second)
        if err != nil {
            results[host] = err
            continue
        }
        conn.Close()
        results[host] = nil
    }
    return results
}

// Uso:
results := checkHosts(22, "web01", "web02", "web03", "db01")
for host, err := range results {
    if err != nil {
        fmt.Printf("FAIL %s: %v\n", host, err)
    } else {
        fmt.Printf("OK   %s\n", host)
    }
}

// Tambien con slice:
servers := loadServerList("/etc/myapp/servers.conf")
results = checkHosts(443, servers...)
```

### Patron 2: Comando con argumentos variables

```go
func runCommand(name string, args ...string) (string, error) {
    cmd := exec.Command(name, args...)
    
    var stdout, stderr bytes.Buffer
    cmd.Stdout = &stdout
    cmd.Stderr = &stderr
    
    if err := cmd.Run(); err != nil {
        return "", fmt.Errorf("%s failed: %w\nstderr: %s", name, err, stderr.String())
    }
    return stdout.String(), nil
}

// Uso flexible:
output, err := runCommand("ls", "-la", "/var/log")
output, err = runCommand("systemctl", "status", "nginx")
output, err = runCommand("docker", "ps", "--format", "{{.Names}}\t{{.Status}}")

// Construir argumentos dinamicamente:
args := []string{"run", "--rm", "-d"}
if network != "" {
    args = append(args, "--network", network)
}
for k, v := range envVars {
    args = append(args, "-e", fmt.Sprintf("%s=%s", k, v))
}
args = append(args, image)
output, err = runCommand("docker", args...)
```

### Patron 3: Logger estructurado con campos variadicos

```go
type LogLevel int

const (
    DEBUG LogLevel = iota
    INFO
    WARN
    ERROR
)

func (l LogLevel) String() string {
    return [...]string{"DEBUG", "INFO", "WARN", "ERROR"}[l]
}

var minLevel = INFO

func log(level LogLevel, msg string, keyvals ...any) {
    if level < minLevel {
        return
    }
    if len(keyvals)%2 != 0 {
        keyvals = append(keyvals, "MISSING_VALUE")
    }
    
    ts := time.Now().Format("2006-01-02T15:04:05.000Z07:00")
    fmt.Printf("%s [%s] %s", ts, level, msg)
    
    for i := 0; i < len(keyvals); i += 2 {
        fmt.Printf(" %v=%v", keyvals[i], keyvals[i+1])
    }
    fmt.Println()
}

// Uso:
log(INFO, "service started", "port", 8080, "workers", 4)
log(ERROR, "connection failed", "host", "db01", "err", err, "retry", 3)
log(WARN, "disk usage high", "mount", "/var", "pct", 89.2)
```

### Patron 4: Merge de configuraciones

```go
type Config struct {
    Host     string
    Port     int
    Timeout  time.Duration
    Retries  int
    LogLevel string
}

// Merge N configs — el ultimo gana
func mergeConfigs(configs ...Config) Config {
    result := Config{
        Host:     "localhost",
        Port:     8080,
        Timeout:  30 * time.Second,
        Retries:  3,
        LogLevel: "info",
    }
    
    for _, c := range configs {
        if c.Host != "" {
            result.Host = c.Host
        }
        if c.Port != 0 {
            result.Port = c.Port
        }
        if c.Timeout != 0 {
            result.Timeout = c.Timeout
        }
        if c.Retries != 0 {
            result.Retries = c.Retries
        }
        if c.LogLevel != "" {
            result.LogLevel = c.LogLevel
        }
    }
    return result
}

// Capas: defaults → file config → env config → CLI flags
finalCfg := mergeConfigs(fileConfig, envConfig, cliConfig)
```

### Patron 5: Functional options (profundizado)

En T01 se introdujo el patron functional options. Aqui vemos como las variadics lo hacen posible:

```go
type Server struct {
    host         string
    port         int
    readTimeout  time.Duration
    writeTimeout time.Duration
    maxConns     int
    tlsCert      string
    tlsKey       string
}

type Option func(*Server)

func WithPort(port int) Option {
    return func(s *Server) { s.port = port }
}

func WithTimeouts(read, write time.Duration) Option {
    return func(s *Server) {
        s.readTimeout = read
        s.writeTimeout = write
    }
}

func WithMaxConns(n int) Option {
    return func(s *Server) { s.maxConns = n }
}

func WithTLS(cert, key string) Option {
    return func(s *Server) {
        s.tlsCert = cert
        s.tlsKey = key
    }
}

// El constructor acepta variadics de Option
func NewServer(host string, opts ...Option) *Server {
    s := &Server{
        host:         host,
        port:         8080,
        readTimeout:  5 * time.Second,
        writeTimeout: 10 * time.Second,
        maxConns:     100,
    }
    for _, opt := range opts {
        opt(s) // Aplica cada opcion
    }
    return s
}

// Uso limpio y extensible:
srv := NewServer("0.0.0.0",
    WithPort(443),
    WithTLS("/etc/ssl/cert.pem", "/etc/ssl/key.pem"),
    WithMaxConns(1000),
    WithTimeouts(10*time.Second, 30*time.Second),
)
```

---

## 7. Variadics en la stdlib: analisis detallado

### fmt.Printf / fmt.Sprintf

```go
// Firma:
// func Printf(format string, a ...any) (n int, err error)

// El primer parametro (format) es fijo, el resto es variadico
fmt.Printf("Host %s port %d status %s\n", host, port, status)

// Internamente, Printf itera sobre a y aplica los verbos del format string
// Si hay mas argumentos que verbos: los extra se ignoran (con warning de go vet)
// Si hay menos argumentos que verbos: imprime %!MISSING
```

### append

```go
// Firma:
// func append(slice []T, elems ...T) []T

// append es un builtin, no una funcion normal
// Pero sigue la mecanica variadica

s := []int{1, 2, 3}
s = append(s, 4)           // un elemento
s = append(s, 5, 6, 7)     // multiples elementos

other := []int{8, 9, 10}
s = append(s, other...)     // expandir otro slice

// Caso especial: append strings a []byte
b := []byte("hello ")
b = append(b, "world"...)   // string se trata como []byte
```

### errors.Join (Go 1.20+)

```go
// Firma:
// func Join(errs ...error) error

var errs []error
if err := checkDisk(); err != nil {
    errs = append(errs, fmt.Errorf("disk: %w", err))
}
if err := checkMemory(); err != nil {
    errs = append(errs, fmt.Errorf("memory: %w", err))
}
if err := checkNetwork(); err != nil {
    errs = append(errs, fmt.Errorf("network: %w", err))
}

if combined := errors.Join(errs...); combined != nil {
    return combined
}
// El error combinado implementa Unwrap() []error
// errors.Is y errors.As funcionan con cada sub-error
```

### filepath.Join

```go
// Firma:
// func Join(elem ...string) string

// Construye paths limpios
path := filepath.Join("/var", "log", "nginx", "access.log")
// "/var/log/nginx/access.log"

// Ignora elementos vacios
path = filepath.Join("/etc", "", "nginx", "nginx.conf")
// "/etc/nginx/nginx.conf"

// SysAdmin: construir paths dinamicamente
parts := []string{baseDir}
if subDir != "" {
    parts = append(parts, subDir)
}
parts = append(parts, filename)
fullPath := filepath.Join(parts...)
```

### strings.Join vs variadic

```go
// strings.Join NO es variadica — recibe un slice
// func Join(elems []string, sep string) string

tags := []string{"web", "production", "us-east-1"}
result := strings.Join(tags, ",")
// "web,production,us-east-1"

// Si quisieras una version variadica:
func joinTags(sep string, tags ...string) string {
    return strings.Join(tags, sep)
}
joinTags(",", "web", "production", "us-east-1")
```

---

## 8. Variadics con generics (Go 1.18+)

### Funcion generica variadica

```go
// Constraint para tipos comparables ordenables
func contains[T comparable](target T, items ...T) bool {
    for _, item := range items {
        if item == target {
            return true
        }
    }
    return false
}

// Uso: el tipo se infiere automaticamente
contains(80, 22, 80, 443, 8080)        // true (T = int)
contains("nginx", "apache", "caddy")   // false (T = string)

// Patron SysAdmin: verificar que un valor esta en un conjunto
if !contains(status, "running", "starting", "restarting") {
    fmt.Printf("Service in unexpected state: %s\n", status)
}
```

### Filter generico

```go
func filter[T any](pred func(T) bool, items ...T) []T {
    var result []T
    for _, item := range items {
        if pred(item) {
            result = append(result, item)
        }
    }
    return result
}

// Uso:
alive := filter(func(h string) bool {
    _, err := net.DialTimeout("tcp", h+":22", time.Second)
    return err == nil
}, "web01", "web02", "web03", "db01")
```

### Min/Max generico

```go
type Ordered interface {
    ~int | ~int64 | ~float64 | ~string
}

func min[T Ordered](first T, rest ...T) T {
    m := first
    for _, v := range rest {
        if v < m {
            m = v
        }
    }
    return m
}

func max[T Ordered](first T, rest ...T) T {
    m := first
    for _, v := range rest {
        if v > m {
            m = v
        }
    }
    return m
}

// Uso:
minLatency := min(latencyUS, latencyEU, latencyAPAC)
maxLoad := max(load1, load5, load15)

// Nota: desde Go 1.21, min y max son builtins
```

---

## 9. Propagacion de variadics

### Pasar variadic a otra funcion variadica

```go
// Funcion base
func logf(format string, args ...any) {
    fmt.Printf("[LOG] "+format+"\n", args...)
    //                                ^^^^
    //  args... expande el slice — NO pasa args como un unico argumento
}

// Wrapper que propaga
func errorf(format string, args ...any) {
    logf("ERROR: "+format, args...)
}

func warnf(format string, args ...any) {
    logf("WARN: "+format, args...)
}

errorf("disk %s at %d%%", "/dev/sda1", 95)
// [LOG] ERROR: disk /dev/sda1 at 95%
```

### Error comun: olvidar `...` al propagar

```go
func wrapper(args ...any) {
    // ✗ INCORRECTO: pasa args como UN SOLO argumento (un []any)
    fmt.Println(args)
    // Imprime: [[valor1 valor2 valor3]]
    
    // ✓ CORRECTO: expande args
    fmt.Println(args...)
    // Imprime: valor1 valor2 valor3
}
```

### Cadena de propagacion

```go
func debug(msg string, fields ...any) {
    log(DEBUG, msg, fields...)
}

func info(msg string, fields ...any) {
    log(INFO, msg, fields...)
}

func warn(msg string, fields ...any) {
    log(WARN, msg, fields...)
}

func errLog(msg string, fields ...any) {
    log(ERROR, msg, fields...)
}

// Todas propagan los fields al log central
// Los fields se pasan sin copia adicional
```

---

## 10. Patrones de la stdlib desglosados

### El patron de exec.Command

```go
// Firma real:
// func Command(name string, arg ...string) *Cmd

// Internamente:
func Command(name string, arg ...string) *Cmd {
    cmd := &Cmd{
        Path: name,
        Args: append([]string{name}, arg...),
        //    ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        //    Args[0] siempre es el nombre del programa
        //    El resto son los argumentos
    }
    // ...
    return cmd
}

// Por eso cuando accedes a cmd.Args:
cmd := exec.Command("ls", "-la", "/tmp")
// cmd.Args = ["ls", "-la", "/tmp"]
// cmd.Args[0] = "ls" (el programa)
// cmd.Args[1:] = ["-la", "/tmp"] (los argumentos)
```

### El patron de log.Printf con prefijo

```go
// En la stdlib, log.Logger.Printf:
func (l *Logger) Printf(format string, v ...any) {
    // Formatea con fmt.Sprintf y luego escribe con mutex
    l.output(2, fmt.Sprintf(format, v...))
}

// Patron para tu propio logger con prefijo:
type PrefixLogger struct {
    prefix string
    logger *log.Logger
}

func (p *PrefixLogger) Printf(format string, args ...any) {
    p.logger.Printf(p.prefix+format, args...)
}

func (p *PrefixLogger) Errorf(format string, args ...any) {
    p.logger.Printf("[ERROR] "+p.prefix+format, args...)
}
```

### El patron de slog (Go 1.21+)

```go
// slog usa variadics de any para key-value pairs:
// func Info(msg string, args ...any)

slog.Info("request handled",
    "method", r.Method,
    "path", r.URL.Path,
    "status", statusCode,
    "duration", time.Since(start),
    "remote", r.RemoteAddr,
)

// Internamente, slog convierte pairs a slog.Attr
// Si un argumento ya es slog.Attr, lo usa directamente:
slog.Info("request handled",
    slog.String("method", r.Method),
    slog.Int("status", statusCode),
    slog.Duration("duration", time.Since(start)),
)
```

---

## 11. Gotchas y errores comunes

### Gotcha 1: Modificacion del slice original via `...`

```go
func addDefaults(servers ...string) []string {
    // ¡Peligro! Si se paso un slice con ..., esto modifica el original
    return append(servers, "monitor01", "bastion01")
}

production := []string{"web01", "web02", "web03"}
// production tiene len=3, cap probablemente >= 3

all := addDefaults(production...)
// Si cap(production) > len(production), append modifica el backing array
// production PODRIA estar corrupto

// ✓ Solucion: copia defensiva dentro de la funcion
func addDefaultsSafe(servers ...string) []string {
    result := make([]string, len(servers), len(servers)+2)
    copy(result, servers)
    return append(result, "monitor01", "bastion01")
}
```

### Gotcha 2: nil vs empty en variadics

```go
func process(items ...string) {
    fmt.Println(items == nil)    // true si llamado sin argumentos
    fmt.Println(len(items) == 0) // true en ambos casos
}

process()                  // nil=true, len=0
process([]string{}...)     // nil=false, len=0  (slice vacio, no nil)
process([]string(nil)...)  // nil=true, len=0   (nil explicito)
```

### Gotcha 3: Variadic de interface no acepta slice de tipo concreto

```go
func printAll(items ...any) {
    for _, item := range items {
        fmt.Println(item)
    }
}

names := []string{"web01", "web02", "web03"}
// ✗ ERROR: cannot use names (variable of type []string) as []any
// printAll(names...)

// ✓ Necesitas convertir:
anySlice := make([]any, len(names))
for i, n := range names {
    anySlice[i] = n
}
printAll(anySlice...)

// ✓ O pasar uno a uno (funciona porque string satisface any):
printAll("web01", "web02", "web03")
```

**Razon**: `[]string` y `[]any` tienen layouts de memoria diferentes. `any` es una interfaz (16 bytes: type pointer + data pointer), mientras que `string` es (16 bytes: data pointer + length). Go no hace la conversion implicita.

### Gotcha 4: Performance con ...any y escape analysis

```go
// Los argumentos a ...any SIEMPRE escapan al heap
// porque el compilador no puede probar que la interfaz no se retiene

func logInfo(msg string, args ...any) {
    // args se aloca en el heap incluso si no se usa
    if currentLevel > INFO {
        return // Los args ya se alocaron
    }
    fmt.Printf(msg, args...)
}

// ✓ Solucion para hot paths: check primero
func logInfo(msg string, args ...any) {
    if currentLevel > INFO {
        return
    }
    fmt.Printf(msg, args...)
}

// ✓ Mejor solucion: no crear la llamada
if currentLevel <= INFO {
    logInfo("request %s took %v", path, duration)
}

// ✓ Solucion slog: usa LogValue para lazy evaluation
slog.Debug("details", "data", slog.GroupValue(
    slog.String("field1", computeExpensive1()),
))
```

### Gotcha 5: `append` con dos slices y variadics

```go
a := []int{1, 2, 3}
b := []int{4, 5, 6}

// ✗ ERROR: no puedes hacer append(a, b)
// ✓ append(a, b...)

c := append(a, b...)
// c = [1, 2, 3, 4, 5, 6]

// Pero cuidado: si cap(a) >= len(a)+len(b),
// a y c comparten backing array
// Solucion: full slice expression
c = append(a[:len(a):len(a)], b...)
```

---

## 12. Comparacion: Go vs C vs Rust

### Go: variadics tipadas

```go
func sum(nums ...int) int {
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
}

// Type-safe: solo acepta int
// Los argumentos se empaquetan en []int
sum(1, 2, 3)
```

### C: variadics sin tipo (va_list)

```c
#include <stdarg.h>

// C no sabe cuantos argumentos hay ni de que tipo
// Necesitas pasarlo explicitamente (count o sentinel)
int sum(int count, ...) {
    va_list args;
    va_start(args, count);
    
    int total = 0;
    for (int i = 0; i < count; i++) {
        total += va_arg(args, int);  // Tipo incorrecto = UB
    }
    
    va_end(args);
    return total;
}

// Peligros:
// - Tipo incorrecto en va_arg → undefined behavior
// - Numero incorrecto → lee basura del stack
// - No hay validacion en compile time (excepto format strings con __attribute__)
sum(3, 1, 2, 3);
sum(3, 1, 2);  // Compila, lee basura para el tercer arg
```

### Rust: sin variadics nativas (macros en su lugar)

```rust
// Rust NO tiene funciones variadicas (excepto FFI con extern "C")
// En su lugar usa macros:

macro_rules! sum {
    ($($x:expr),*) => {
        {
            let mut total = 0;
            $(total += $x;)*
            total
        }
    };
}

let result = sum!(1, 2, 3);  // Se expande en compile time

// Para funciones normales: usa slices o iteradores
fn sum(nums: &[i32]) -> i32 {
    nums.iter().sum()
}

sum(&[1, 2, 3]);

// O con Into<Vec<T>> para flexibilidad:
fn process(items: impl IntoIterator<Item = String>) {
    for item in items {
        println!("{}", item);
    }
}
```

### Tabla comparativa

```
┌─────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto         │ Go                   │ C                    │ Rust                 │
├─────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Sintaxis        │ ...T                 │ ... (va_list)        │ No nativo (macros)   │
│ Type safety     │ Si (tipo fijo o any) │ No (va_arg manual)   │ Si (macros tipadas)  │
│ Compile check   │ Parcial (any no)     │ Solo format attrs    │ Completo (macros)    │
│ Representacion  │ []T (slice)          │ Stack frame          │ Expansion en compile │
│ Num. conocido?  │ Si (len del slice)   │ No (manual)          │ Si (compile time)    │
│ Performance     │ Heap si es ...any    │ Stack (rapido)       │ Zero cost (inline)   │
│ Paso de arrays  │ slice...             │ No aplica            │ &[T] / IntoIterator  │
│ Multiples tipos │ ...any (runtime)     │ ... (sin tipo)       │ Traits + generics    │
│ Sentinel/count  │ No necesario         │ Necesario            │ No aplica            │
│ UB posible?     │ No                   │ Si (tipo/count mal)  │ No                   │
└─────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 13. Programa completo: Multi-Service Health Checker

Un health checker que usa variadics extensivamente para construir checks flexibles, configurar targets, y agregar resultados.

```go
package main

import (
	"context"
	"encoding/json"
	"fmt"
	"net"
	"net/http"
	"os"
	"strings"
	"sync"
	"time"
)

// ─── Tipos ──────────────────────────────────────────────────────

type CheckResult struct {
	Target   string        `json:"target"`
	Type     string        `json:"type"`
	Status   string        `json:"status"`
	Latency  time.Duration `json:"latency_ms"`
	Error    string        `json:"error,omitempty"`
}

type CheckFunc func(ctx context.Context, target string) CheckResult

type HealthChecker struct {
	timeout  time.Duration
	retries  int
	checks   []registeredCheck
	mu       sync.Mutex
	results  []CheckResult
}

type registeredCheck struct {
	targets  []string
	checkFn  CheckFunc
}

// ─── Functional Options (variadics) ─────────────────────────────

type HCOption func(*HealthChecker)

func WithTimeout(d time.Duration) HCOption {
	return func(hc *HealthChecker) { hc.timeout = d }
}

func WithRetries(n int) HCOption {
	return func(hc *HealthChecker) { hc.retries = n }
}

func NewHealthChecker(opts ...HCOption) *HealthChecker {
	hc := &HealthChecker{
		timeout: 5 * time.Second,
		retries: 1,
	}
	for _, opt := range opts {
		opt(hc)
	}
	return hc
}

// ─── Registro de checks (variadics para targets) ───────────────

func (hc *HealthChecker) AddTCPCheck(port int, hosts ...string) {
	targets := make([]string, len(hosts))
	for i, h := range hosts {
		targets[i] = fmt.Sprintf("%s:%d", h, port)
	}
	hc.checks = append(hc.checks, registeredCheck{
		targets: targets,
		checkFn: func(ctx context.Context, target string) CheckResult {
			start := time.Now()
			d := net.Dialer{Timeout: 2 * time.Second}
			conn, err := d.DialContext(ctx, "tcp", target)
			result := CheckResult{
				Target:  target,
				Type:    "tcp",
				Latency: time.Since(start),
			}
			if err != nil {
				result.Status = "FAIL"
				result.Error = err.Error()
			} else {
				conn.Close()
				result.Status = "OK"
			}
			return result
		},
	})
}

func (hc *HealthChecker) AddHTTPCheck(paths ...string) {
	targets := make([]string, len(paths))
	copy(targets, paths)
	hc.checks = append(hc.checks, registeredCheck{
		targets: targets,
		checkFn: func(ctx context.Context, target string) CheckResult {
			start := time.Now()
			req, err := http.NewRequestWithContext(ctx, "GET", target, nil)
			if err != nil {
				return CheckResult{
					Target: target, Type: "http", Status: "FAIL",
					Latency: time.Since(start), Error: err.Error(),
				}
			}
			resp, err := http.DefaultClient.Do(req)
			result := CheckResult{
				Target:  target,
				Type:    "http",
				Latency: time.Since(start),
			}
			if err != nil {
				result.Status = "FAIL"
				result.Error = err.Error()
			} else {
				resp.Body.Close()
				if resp.StatusCode >= 200 && resp.StatusCode < 400 {
					result.Status = "OK"
				} else {
					result.Status = "FAIL"
					result.Error = fmt.Sprintf("status %d", resp.StatusCode)
				}
			}
			return result
		},
	})
}

func (hc *HealthChecker) AddDNSCheck(names ...string) {
	targets := make([]string, len(names))
	copy(targets, names)
	hc.checks = append(hc.checks, registeredCheck{
		targets: targets,
		checkFn: func(ctx context.Context, target string) CheckResult {
			start := time.Now()
			resolver := net.Resolver{}
			addrs, err := resolver.LookupHost(ctx, target)
			result := CheckResult{
				Target:  target,
				Type:    "dns",
				Latency: time.Since(start),
			}
			if err != nil {
				result.Status = "FAIL"
				result.Error = err.Error()
			} else if len(addrs) == 0 {
				result.Status = "FAIL"
				result.Error = "no addresses returned"
			} else {
				result.Status = "OK"
			}
			return result
		},
	})
}

// ─── Ejecucion ──────────────────────────────────────────────────

func (hc *HealthChecker) RunAll() []CheckResult {
	ctx, cancel := context.WithTimeout(context.Background(), hc.timeout)
	defer cancel()

	var wg sync.WaitGroup

	for _, check := range hc.checks {
		for _, target := range check.targets {
			wg.Add(1)
			go func(fn CheckFunc, t string) {
				defer wg.Done()

				var result CheckResult
				for attempt := 0; attempt < hc.retries; attempt++ {
					result = fn(ctx, t)
					if result.Status == "OK" {
						break
					}
					if attempt < hc.retries-1 {
						time.Sleep(500 * time.Millisecond)
					}
				}

				hc.mu.Lock()
				hc.results = append(hc.results, result)
				hc.mu.Unlock()
			}(check.checkFn, target)
		}
	}

	wg.Wait()
	return hc.results
}

// ─── Formateo de resultados (variadics para filtros) ────────────

func filterResults(results []CheckResult, statuses ...string) []CheckResult {
	if len(statuses) == 0 {
		return results
	}
	statusSet := make(map[string]bool, len(statuses))
	for _, s := range statuses {
		statusSet[s] = true
	}
	var filtered []CheckResult
	for _, r := range results {
		if statusSet[r.Status] {
			filtered = append(filtered, r)
		}
	}
	return filtered
}

func formatReport(results []CheckResult, formats ...string) string {
	format := "text"
	if len(formats) > 0 {
		format = formats[0]
	}

	switch format {
	case "json":
		data, _ := json.MarshalIndent(results, "", "  ")
		return string(data)
	case "csv":
		var b strings.Builder
		b.WriteString("target,type,status,latency_ms,error\n")
		for _, r := range results {
			b.WriteString(fmt.Sprintf("%s,%s,%s,%d,%s\n",
				r.Target, r.Type, r.Status,
				r.Latency.Milliseconds(), r.Error))
		}
		return b.String()
	default: // text
		var b strings.Builder
		b.WriteString(fmt.Sprintf("%-35s %-6s %-6s %8s  %s\n",
			"TARGET", "TYPE", "STATUS", "LATENCY", "ERROR"))
		b.WriteString(strings.Repeat("─", 80) + "\n")
		for _, r := range results {
			errStr := r.Error
			if len(errStr) > 30 {
				errStr = errStr[:27] + "..."
			}
			b.WriteString(fmt.Sprintf("%-35s %-6s %-6s %6dms  %s\n",
				r.Target, r.Type, r.Status,
				r.Latency.Milliseconds(), errStr))
		}
		return b.String()
	}
}

// ─── Utilidades variadicas ──────────────────────────────────────

func logf(format string, args ...any) {
	ts := time.Now().Format("15:04:05")
	fmt.Printf("[%s] "+format+"\n", append([]any{ts}, args...)...)
}

func mustEnv(keys ...string) map[string]string {
	result := make(map[string]string, len(keys))
	var missing []string
	for _, key := range keys {
		val := os.Getenv(key)
		if val == "" {
			missing = append(missing, key)
		}
		result[key] = val
	}
	if len(missing) > 0 {
		logf("WARNING: missing env vars: %s", strings.Join(missing, ", "))
	}
	return result
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	logf("Starting health checker")

	// Crear checker con functional options (variadics)
	checker := NewHealthChecker(
		WithTimeout(10*time.Second),
		WithRetries(2),
	)

	// Registrar checks — cada metodo acepta targets variadicos
	checker.AddTCPCheck(22, "localhost")
	checker.AddTCPCheck(80, "localhost")
	checker.AddDNSCheck("google.com", "github.com", "cloudflare.com")
	checker.AddHTTPCheck(
		"https://httpbin.org/status/200",
		"https://httpbin.org/status/500",
	)

	// Ejecutar todos los checks en paralelo
	logf("Running %d check groups...", len(checker.checks))
	results := checker.RunAll()

	// Reporte completo
	fmt.Println("\n" + formatReport(results))

	// Solo fallos (variadic filter)
	failures := filterResults(results, "FAIL")
	if len(failures) > 0 {
		logf("Found %d failures:", len(failures))
		// JSON solo para los fallos (variadic format)
		fmt.Println(formatReport(failures, "json"))
	}

	// Verificar env vars criticas (variadic)
	envs := mustEnv("HOME", "USER", "SHELL", "NONEXISTENT_VAR")
	logf("Environment: HOME=%s USER=%s", envs["HOME"], envs["USER"])

	// Exit code basado en resultados
	if len(failures) > 0 {
		os.Exit(1)
	}
	logf("All checks passed")
}
```

### Desglose de variadics en el programa

```
┌──────────────────────────────────────────────────────────────┐
│  USO DE VARIADICS EN EL PROGRAMA                             │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. NewHealthChecker(opts ...HCOption)                       │
│     → Functional options pattern                             │
│                                                              │
│  2. AddTCPCheck(port int, hosts ...string)                   │
│     → Parametro fijo + variadico: "check este port en N      │
│       hosts"                                                 │
│                                                              │
│  3. AddHTTPCheck(paths ...string)                            │
│     → Lista flexible de URLs a verificar                     │
│                                                              │
│  4. AddDNSCheck(names ...string)                             │
│     → Lista flexible de dominios a resolver                  │
│                                                              │
│  5. filterResults(results, statuses ...string)               │
│     → Filtro con criterios variables: "FAIL", "OK", ambos   │
│                                                              │
│  6. formatReport(results, formats ...string)                 │
│     → Parametro opcional via variadico (0 o 1 formato)       │
│                                                              │
│  7. logf(format string, args ...any)                         │
│     → Wrapper de Printf con timestamp                        │
│                                                              │
│  8. mustEnv(keys ...string)                                  │
│     → Verificar N variables de entorno de una vez            │
│                                                              │
│  9. append([]any{ts}, args...)                               │
│     → Prepend de argumento + propagacion variadica           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 14. Patron: parametro opcional via variadico

Cuando una funcion necesita un parametro opcional pero Go no tiene defaults:

```go
// Patron: usar variadico para 0 o 1 argumento opcional
func readFile(path string, maxSize ...int64) ([]byte, error) {
    limit := int64(10 * 1024 * 1024) // default 10MB
    if len(maxSize) > 0 {
        limit = maxSize[0]
    }
    
    info, err := os.Stat(path)
    if err != nil {
        return nil, err
    }
    if info.Size() > limit {
        return nil, fmt.Errorf("file %s exceeds limit: %d > %d", path, info.Size(), limit)
    }
    return os.ReadFile(path)
}

// Uso:
data, err := readFile("/etc/hostname")          // usa default 10MB
data, err = readFile("/var/log/syslog", 1<<20)  // limite de 1MB
```

### Cuando usar este patron vs alternatives

```go
// ✓ Bueno: 1 parametro opcional simple
func connect(addr string, timeout ...time.Duration) (*Conn, error)

// ✗ Malo: multiples "opcionales" — usa functional options o config struct
func connect(addr string, timeout ...time.Duration, retries ...int) // NO COMPILA

// Alternativa: struct de config
type ConnConfig struct {
    Timeout time.Duration
    Retries int
    TLS     bool
}
func connect(addr string, cfg ConnConfig) (*Conn, error)

// Alternativa: functional options (mejor para APIs publicas)
func connect(addr string, opts ...ConnOption) (*Conn, error)
```

---

## 15. Variadics y concurrencia

### Lanzar goroutines para cada argumento variadico

```go
func pingAll(timeout time.Duration, hosts ...string) map[string]time.Duration {
    results := make(map[string]time.Duration, len(hosts))
    var mu sync.Mutex
    var wg sync.WaitGroup

    for _, host := range hosts {
        wg.Add(1)
        go func(h string) {
            defer wg.Done()
            start := time.Now()
            conn, err := net.DialTimeout("tcp", h+":80", timeout)
            latency := time.Since(start)
            
            mu.Lock()
            defer mu.Unlock()
            if err != nil {
                results[h] = -1
            } else {
                conn.Close()
                results[h] = latency
            }
        }(host)
    }

    wg.Wait()
    return results
}

// Uso: check todos los servidores en paralelo
latencies := pingAll(2*time.Second, "web01", "web02", "web03", "db01", "db02")
```

### Fan-out con errgroup

```go
func checkServices(ctx context.Context, services ...string) error {
    g, ctx := errgroup.WithContext(ctx)
    
    for _, svc := range services {
        svc := svc // capture para goroutine (pre-Go 1.22)
        g.Go(func() error {
            cmd := exec.CommandContext(ctx, "systemctl", "is-active", svc)
            output, err := cmd.Output()
            if err != nil {
                return fmt.Errorf("service %s: %w", svc, err)
            }
            status := strings.TrimSpace(string(output))
            if status != "active" {
                return fmt.Errorf("service %s: status=%s", svc, status)
            }
            return nil
        })
    }
    
    return g.Wait() // Retorna el primer error, cancela el resto
}

err := checkServices(ctx, "nginx", "postgresql", "redis", "prometheus")
if err != nil {
    log.Fatalf("Service check failed: %v", err)
}
```

---

## 16. Antipatrones

### Antipatron 1: Variadico cuando necesitas al menos un argumento

```go
// ✗ Puede llamarse sin argumentos — panic o resultado sin sentido
func minBad(nums ...int) int {
    m := nums[0] // panic si nums es nil/empty!
    for _, n := range nums[1:] {
        if n < m {
            m = n
        }
    }
    return m
}

// ✓ Forzar al menos uno con parametro fijo
func minGood(first int, rest ...int) int {
    m := first
    for _, n := range rest {
        if n < m {
            m = n
        }
    }
    return m
}

// minGood()     // ERROR de compilacion: not enough arguments
// minGood(42)   // OK: retorna 42
```

### Antipatron 2: Usar ...any cuando el tipo es conocido

```go
// ✗ Pierde type safety
func addHosts(hosts ...any) {
    for _, h := range hosts {
        host, ok := h.(string) // assertion en runtime
        if !ok {
            panic("expected string")
        }
        // ...
    }
}

// ✓ Usa el tipo concreto
func addHosts(hosts ...string) {
    for _, host := range hosts {
        // host ya es string, no necesita assertion
    }
}
```

### Antipatron 3: Variadico para simular multiples tipos de parametros

```go
// ✗ Horrible: posicion determina significado
func connect(params ...any) {
    host := params[0].(string)   // panic si mal tipo
    port := params[1].(int)      // panic si mal posicion
    tls := params[2].(bool)      // panic si olvidado
}

// ✓ Usa parametros con nombre
func connect(host string, port int, tls bool) {}

// ✓ O usa un struct si hay muchos parametros
type ConnParams struct {
    Host string
    Port int
    TLS  bool
}
```

### Antipatron 4: Variadics en interfaces publicas sin documentar

```go
// ✗ Que son estos "args"? pairs? positional? format args?
func Log(args ...any) {}

// ✓ Documenta el contrato
// LogFields accepts key-value pairs: LogFields("key1", val1, "key2", val2, ...)
// Keys must be strings. Panics if odd number of arguments.
func LogFields(keyvals ...any) {}

// ✓ Mejor: usa un tipo que fuerce la estructura
type Field struct {
    Key   string
    Value any
}
func LogStructured(fields ...Field) {}
```

---

## 17. Performance

### Benchmark: individual args vs slice spread

```go
func BenchmarkVariadicDirect(b *testing.B) {
    for i := 0; i < b.N; i++ {
        sum(1, 2, 3, 4, 5)
        // Go crea un []int{1,2,3,4,5} — puede stack-allocate
    }
}

func BenchmarkVariadicSpread(b *testing.B) {
    nums := []int{1, 2, 3, 4, 5}
    for i := 0; i < b.N; i++ {
        sum(nums...)
        // Reutiliza el slice existente — no alloc
    }
}

// Resultados tipicos:
// BenchmarkVariadicDirect-8    ~3ns/op   0 allocs/op
// BenchmarkVariadicSpread-8    ~2ns/op   0 allocs/op
```

### Benchmark: ...int vs ...any

```go
func sumInt(nums ...int) int {
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
}

func sumAny(nums ...any) int {
    total := 0
    for _, n := range nums {
        total += n.(int)
    }
    return total
}

// Resultados tipicos:
// BenchmarkSumInt-8    ~3ns/op     0 allocs/op
// BenchmarkSumAny-8    ~45ns/op    5 allocs/op
//                                  ↑ cada int se boxea en interfaz
```

**Conclusion**: `...any` es ~15x mas lento por las alocaciones de boxing. Usa tipos concretos siempre que sea posible. Reserve `...any` para APIs que genuinamente necesitan aceptar multiples tipos (como `fmt.Printf`).

### Escape analysis con variadics

```go
// El compilador puede stack-allocate el slice variadico si:
// 1. La funcion no escapa el slice (no lo retorna ni lo almacena)
// 2. Se pasan argumentos individuales (no slice...)
// 3. El compilador puede inlinear la funcion

// Verificar con:
// go build -gcflags="-m" ./...
// "does not escape" = stack allocated (bueno)
// "escapes to heap" = heap allocated (costoso)

func sum(nums ...int) int {  // nums does not escape
    total := 0
    for _, n := range nums {
        total += n
    }
    return total
}
```

---

## 18. Tabla de errores comunes

```
┌────┬──────────────────────────────────────┬────────────────────────────────────────┬─────────┐
│ #  │ Error                                │ Solucion                               │ Nivel   │
├────┼──────────────────────────────────────┼────────────────────────────────────────┼─────────┤
│  1 │ Variadico no es el ultimo parametro  │ Moverlo al final de la lista           │ Compile │
│  2 │ Dos parametros variadicos            │ Imposible — usa slice para uno         │ Compile │
│  3 │ Pasar []string a ...any              │ Convertir a []any manualmente          │ Compile │
│  4 │ Pasar slice sin ... a variadica      │ Agregar ... al final: slice...         │ Compile │
│  5 │ Olvidar ... al propagar variadics    │ fn(args...) no fn(args)                │ Logic   │
│  6 │ Modificar slice original via ...     │ Copia defensiva dentro de la funcion   │ Logic   │
│  7 │ Asumir len>0 sin parametro fijo      │ Usar first T, rest ...T               │ Runtime │
│  8 │ Usar ...any cuando tipo es conocido  │ Usar tipo concreto o generics          │ Design  │
│  9 │ Variadico para simular named params  │ Usar struct o functional options       │ Design  │
│ 10 │ Performance con ...any en hot path   │ Tipo concreto o check antes de llamar  │ Perf    │
│ 11 │ nil vs empty slice en variadico      │ Usar len()==0, no items==nil           │ Logic   │
│ 12 │ append(a, b) sin ...                 │ append(a, b...)                        │ Compile │
└────┴──────────────────────────────────────┴────────────────────────────────────────┴─────────┘
```

---

## 19. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
func f(nums ...int) {
    fmt.Println(nums == nil, len(nums))
}
f()
f([]int{}...)
```

<details>
<summary>Respuesta</summary>

```
true 0
false 0
```

Sin argumentos → `nil`. Con slice vacio expandido → not nil pero len 0.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
func modify(s ...int) {
    s[0] = 99
}

a := []int{1, 2, 3}
modify(a...)
fmt.Println(a)

modify(1, 2, 3)
fmt.Println("no panic")
```

<details>
<summary>Respuesta</summary>

```
[99 2 3]
no panic
```

`a...` comparte backing array → se modifica. `1, 2, 3` crea un nuevo slice → original no se toca.
</details>

**Ejercicio 3**: ¿Compila?

```go
func log(parts ...string, level int) {}
```

<details>
<summary>Respuesta</summary>

No. Error de compilacion: `can only use ... with final parameter in list`. El variadico debe ser el ultimo parametro.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
func wrap(args ...any) {
    fmt.Println(args...)
}
wrap("hello", 42, true)
```

<details>
<summary>Respuesta</summary>

```
hello 42 true
```

`args...` expande el `[]any` como argumentos individuales a `fmt.Println`, que los imprime separados por espacio.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
func wrap(args ...any) {
    fmt.Println(args)
}
wrap("hello", 42, true)
```

<details>
<summary>Respuesta</summary>

```
[hello 42 true]
```

Sin `...`, `args` se pasa como un unico argumento de tipo `[]any`. `Println` lo formatea como slice.
</details>

**Ejercicio 6**: ¿Compila?

```go
names := []string{"a", "b", "c"}
func printAll(items ...any) {}
printAll(names...)
```

<details>
<summary>Respuesta</summary>

No compila. `cannot use names (variable of type []string) as []any`. `[]string` no es convertible a `[]any` implicitamente.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
func sum(first int, rest ...int) int {
    total := first
    for _, n := range rest {
        total += n
    }
    return total
}
fmt.Println(sum(10))
fmt.Println(sum(10, 20, 30))
```

<details>
<summary>Respuesta</summary>

```
10
60
```

`sum(10)` → rest es nil, loop no ejecuta, retorna 10. `sum(10, 20, 30)` → 10+20+30 = 60.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
a := []int{1, 2, 3}
b := append(a[:2], 9)
fmt.Println(a)
fmt.Println(b)
```

<details>
<summary>Respuesta</summary>

```
[1 2 9]
[1 2 9]
```

`a[:2]` tiene cap 3, asi que `append` escribe en `a[2]` sin alocar nuevo array. Tanto `a` como `b` comparten backing array.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
func prepend(first any, args ...any) []any {
    return append([]any{first}, args...)
}

result := prepend("ts", "a", "b", "c")
fmt.Println(result)
```

<details>
<summary>Respuesta</summary>

```
[ts a b c]
```

Crea `[]any{"ts"}` y le appende los args expandidos.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
func count(items ...string) int {
    return len(items)
}

var s []string
fmt.Println(count())
fmt.Println(count(s...))
fmt.Println(count(""))
```

<details>
<summary>Respuesta</summary>

```
0
0
1
```

Sin args → nil, len=0. `s` es nil, `s...` → nil, len=0. `""` es un string vacio pero es un argumento, len=1.
</details>

---

## 20. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              VARIADIC FUNCTIONS — RESUMEN                    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DECLARACION                                                 │
│  func name(fixed T, variadic ...T) R { }                     │
│  ├─ Solo el ultimo parametro puede ser variadico             │
│  ├─ Internamente es un []T (slice)                           │
│  ├─ 0 args → nil slice (no empty slice)                      │
│  └─ Para forzar >=1: func f(first T, rest ...T)              │
│                                                              │
│  DOS SIGNIFICADOS DE "..."                                   │
│  ├─ En declaracion: ...T = "acepto 0+ argumentos de tipo T"  │
│  └─ En llamada: s... = "expande este slice como argumentos"  │
│                                                              │
│  PASO DE SLICES                                              │
│  ├─ f(1, 2, 3) — crea nuevo slice (seguro)                  │
│  ├─ f(s...) — reutiliza el backing array (¡cuidado!)         │
│  └─ []string no convierte a []any (conversion manual)        │
│                                                              │
│  PROPAGACION                                                 │
│  ├─ fn(args...) — expande y pasa los argumentos              │
│  ├─ fn(args)    — pasa el slice como un unico argumento      │
│  └─ Siempre usar ... al propagar a otra variadica            │
│                                                              │
│  PATRONES SYSADMIN                                           │
│  ├─ Multi-host checker: check(port, hosts...string)          │
│  ├─ Comando wrapper: run(name, args ...string)               │
│  ├─ Logger: logf(format, args ...any)                        │
│  ├─ Config merge: merge(configs ...Config)                   │
│  ├─ Functional options: New(opts ...Option)                  │
│  ├─ Parametro opcional: f(path, maxSize ...int64)            │
│  └─ Env check: mustEnv(keys ...string)                       │
│                                                              │
│  PERFORMANCE                                                 │
│  ├─ ...T concreto: ~3ns, 0 allocs (stack)                    │
│  ├─ ...any: ~45ns, N allocs (heap boxing)                    │
│  └─ Usar tipo concreto siempre que sea posible               │
│                                                              │
│  ERRORES FRECUENTES                                          │
│  ├─ Variadico no al final → error de compilacion             │
│  ├─ []string a ...any → error de compilacion                 │
│  ├─ Olvidar ... al expandir → logica incorrecta              │
│  ├─ Modificar slice original via ... → bug silencioso        │
│  └─ ...any cuando el tipo es conocido → design smell         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T03 - Funciones como valores — asignacion a variables, paso como argumento, retorno de funciones
