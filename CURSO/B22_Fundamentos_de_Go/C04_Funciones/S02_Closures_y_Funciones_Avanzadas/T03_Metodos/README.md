# Metodos — Receptores y Method Sets

## 1. Introduccion

Un **metodo** en Go es una funcion con un **receptor** (receiver) — un parametro especial que vincula la funcion a un tipo. No hay clases en Go; en su lugar, defines tipos y les agregas metodos. Cualquier tipo definido por el usuario puede tener metodos, no solo structs.

La distincion entre **value receiver** y **pointer receiver** es una de las decisiones de diseno mas importantes en Go. Determina si el metodo puede modificar el receptor, como interactua con interfaces, y tiene implicaciones directas en concurrencia y performance.

En SysAdmin/DevOps, los metodos son la forma en que modelas servicios, conexiones, pools, configuraciones y monitores. Un `Service` tiene metodos `Start()`, `Stop()`, `Status()`. Un `ConnectionPool` tiene `Get()`, `Put()`, `Close()`. Entender receptores es entender como funciona el 90% del codigo Go en produccion.

```
┌──────────────────────────────────────────────────────────────┐
│              METODOS EN GO                                    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Sintaxis:                                                   │
│  func (receiver Type) MethodName(params) returns { body }    │
│       ↑                                                      │
│  Receptor: el "this" o "self" de otros lenguajes             │
│                                                              │
│  Dos tipos de receptor:                                      │
│                                                              │
│  Value receiver: func (s Server) Status() string             │
│  ├─ Recibe COPIA del valor                                   │
│  ├─ No puede modificar el original                           │
│  ├─ Seguro para concurrencia (cada call tiene su copia)      │
│  └─ Se puede llamar en valores Y punteros                    │
│                                                              │
│  Pointer receiver: func (s *Server) Start() error            │
│  ├─ Recibe PUNTERO al valor                                  │
│  ├─ Puede modificar el original                              │
│  ├─ Eficiente para structs grandes (no copia)                │
│  └─ Se puede llamar en punteros (Go auto-referencia valores) │
│                                                              │
│  Method set (regla clave para interfaces):                   │
│  ├─ T  tiene metodos con receiver T                          │
│  ├─ *T tiene metodos con receiver T y *T                     │
│  └─ Solo el method set determina que interfaces satisface    │
│                                                              │
│  Regla practica:                                             │
│  ├─ Si el metodo modifica el receptor → pointer receiver     │
│  ├─ Si el struct es grande → pointer receiver (eficiencia)   │
│  ├─ Si hay mutex/sync fields → pointer receiver (siempre)    │
│  ├─ Por consistencia: si uno es pointer, todos pointer       │
│  └─ Tipos pequenos inmutables (Point, Color) → value         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Sintaxis basica

### Declarar un metodo

```go
type Server struct {
    Name   string
    Host   string
    Port   int
    Active bool
}

// Metodo con value receiver
func (s Server) Address() string {
    return fmt.Sprintf("%s:%d", s.Host, s.Port)
}

// Metodo con pointer receiver
func (s *Server) Start() {
    s.Active = true // Modifica el original
    fmt.Printf("Starting %s on %s\n", s.Name, s.Address())
}

func (s *Server) Stop() {
    s.Active = false
    fmt.Printf("Stopping %s\n", s.Name)
}

// Uso:
srv := Server{Name: "api", Host: "0.0.0.0", Port: 8080}
fmt.Println(srv.Address()) // "0.0.0.0:8080"
srv.Start()                // "Starting api on 0.0.0.0:8080"
fmt.Println(srv.Active)    // true
srv.Stop()                 // "Stopping api"
fmt.Println(srv.Active)    // false
```

### El receptor es un parametro ordinario

```go
// Un metodo es azucar sintactico para una funcion con el receptor como primer parametro

// Esto:
func (s Server) Address() string {
    return fmt.Sprintf("%s:%d", s.Host, s.Port)
}

// Es equivalente a:
func ServerAddress(s Server) string {
    return fmt.Sprintf("%s:%d", s.Host, s.Port)
}

// Puedes llamar a la version "funcion" via method expression:
addr := Server.Address(srv) // Method expression — receptor como argumento
```

### Nombrar el receptor

```go
// Convencion Go: nombre corto (1-2 letras), NO "this" ni "self"
func (s Server) Status() string { ... }   // ✓ "s" para Server
func (db *Database) Query() { ... }        // ✓ "db" para Database
func (c *Config) Validate() error { ... }  // ✓ "c" para Config
func (lw *LogWriter) Write() { ... }       // ✓ "lw" para LogWriter

// ✗ Evitar:
func (this *Server) Status() string { ... } // No idiomatico
func (self *Server) Status() string { ... } // No idiomatico
func (server *Server) Status() string { ... } // Demasiado largo

// El nombre debe ser CONSISTENTE en todos los metodos del tipo:
// ✗ No cambiar de nombre entre metodos
func (s *Server) Start() { ... }
func (srv *Server) Stop() { ... }  // ✗ Inconsistente — usar "s" siempre
```

---

## 3. Value receiver en detalle

### Recibe una copia

```go
type Point struct {
    X, Y float64
}

func (p Point) Distance(other Point) float64 {
    dx := p.X - other.X
    dy := p.Y - other.Y
    return math.Sqrt(dx*dx + dy*dy)
}

// p es una COPIA — modificarlo no afecta al original
func (p Point) Translate(dx, dy float64) Point {
    p.X += dx // Modifica la copia
    p.Y += dy
    return p  // Retorna la copia modificada
}

origin := Point{0, 0}
moved := origin.Translate(3, 4)
fmt.Println(origin) // {0 0} — no cambio
fmt.Println(moved)  // {3 4} — nueva copia
```

### Cuando usar value receiver

```go
// 1. Tipos pequenos e inmutables
type Color struct {
    R, G, B uint8
}

func (c Color) Hex() string {
    return fmt.Sprintf("#%02x%02x%02x", c.R, c.G, c.B)
}

func (c Color) Brighter(factor float64) Color {
    return Color{
        R: uint8(math.Min(float64(c.R)*factor, 255)),
        G: uint8(math.Min(float64(c.G)*factor, 255)),
        B: uint8(math.Min(float64(c.B)*factor, 255)),
    }
}

// 2. Metodos que solo leen el estado
type Config struct {
    Host    string
    Port    int
    Debug   bool
}

func (c Config) IsProduction() bool {
    return !c.Debug && c.Port == 443
}

func (c Config) DSN() string {
    return fmt.Sprintf("host=%s port=%d", c.Host, c.Port)
}

// 3. Tipos basicos envueltos
type Celsius float64
type Fahrenheit float64

func (c Celsius) ToFahrenheit() Fahrenheit {
    return Fahrenheit(c*9/5 + 32)
}

func (f Fahrenheit) ToCelsius() Celsius {
    return Celsius((f - 32) * 5 / 9)
}
```

### Value receiver y concurrencia

```go
// Value receiver es thread-safe por naturaleza:
// cada goroutine recibe su propia copia

type Metrics struct {
    Requests int
    Errors   int
    Latency  float64
}

// Seguro: cada goroutine lee su propia copia
func (m Metrics) ErrorRate() float64 {
    if m.Requests == 0 {
        return 0
    }
    return float64(m.Errors) / float64(m.Requests) * 100
}

// Puedes pasar Metrics a multiples goroutines sin mutex
// porque cada una obtiene una copia independiente
```

---

## 4. Pointer receiver en detalle

### Puede modificar el receptor

```go
type Counter struct {
    n int
}

func (c *Counter) Increment() {
    c.n++ // Modifica el original
}

func (c *Counter) Reset() {
    c.n = 0
}

func (c *Counter) Value() int {
    return c.n // Tambien puede leer con pointer receiver
}

ctr := &Counter{}
ctr.Increment()
ctr.Increment()
ctr.Increment()
fmt.Println(ctr.Value()) // 3
ctr.Reset()
fmt.Println(ctr.Value()) // 0
```

### Cuando usar pointer receiver

```go
// 1. El metodo modifica el receptor
func (s *Server) SetPort(port int) {
    s.Port = port
}

// 2. El struct es grande (evitar copias)
type LargeConfig struct {
    // Imagina 50 campos...
    Data [1024]byte
    // ...
}

func (lc *LargeConfig) Process() { // Puntero: no copia 1KB
    // ...
}

// 3. El struct contiene sync primitives
type SafeMap struct {
    mu sync.RWMutex
    m  map[string]string
}

func (sm *SafeMap) Get(key string) (string, bool) {
    sm.mu.RLock()
    defer sm.mu.RUnlock()
    v, ok := sm.m[key]
    return v, ok
}

func (sm *SafeMap) Set(key, value string) {
    sm.mu.Lock()
    defer sm.mu.Unlock()
    sm.m[key] = value
}

// CRITICO: sync.Mutex/RWMutex NO se puede copiar
// Si usaras value receiver, copiarias el mutex → bug silencioso
// go vet detecta esto: "copies lock value"

// 4. Consistencia: si uno es pointer, todos lo son
type Service struct {
    Name   string
    Status string
    pid    int
}

func (s *Service) Start() error { ... }   // Modifica: pointer
func (s *Service) Stop() error { ... }    // Modifica: pointer
func (s *Service) IsRunning() bool { ... } // Solo lee, pero pointer por consistencia
func (s *Service) PID() int { ... }        // Solo lee, pero pointer por consistencia
```

### nil pointer receiver

```go
// Un metodo con pointer receiver puede ser llamado en nil
// Esto es valido y util para ciertos patrones

type List struct {
    Value int
    Next  *List
}

func (l *List) Len() int {
    if l == nil {
        return 0 // Manejar nil graciosamente
    }
    return 1 + l.Next.Len()
}

var empty *List
fmt.Println(empty.Len()) // 0 — no panic

// Patron en la stdlib: bytes.Buffer
var buf *bytes.Buffer
// buf.String() → panic (nil pointer dereference)
// La stdlib NO usa este patron con bytes.Buffer, pero algunos tipos si

// Patron util: logger con nil check
type Logger struct {
    prefix string
    output io.Writer
}

func (l *Logger) Printf(format string, args ...any) {
    if l == nil {
        return // noop si el logger es nil
    }
    fmt.Fprintf(l.output, l.prefix+format+"\n", args...)
}

// Permite:
var logger *Logger // nil
logger.Printf("this is silently ignored") // No panic
```

---

## 5. Auto-referencia y auto-derreferencia

### Go automatiza & y * al llamar metodos

```go
type Server struct {
    Name string
    Port int
}

func (s Server) Address() string {     // value receiver
    return fmt.Sprintf("%s:%d", s.Name, s.Port)
}

func (s *Server) SetPort(port int) {   // pointer receiver
    s.Port = port
}

// Puedes llamar AMBOS en un valor:
srv := Server{Name: "web", Port: 80}
srv.Address() // OK: value receiver en valor
srv.SetPort(443) // OK: Go automaticamente toma &srv
// Equivale a: (&srv).SetPort(443)

// Puedes llamar AMBOS en un puntero:
srvPtr := &Server{Name: "api", Port: 8080}
srvPtr.Address() // OK: Go automaticamente derreferencia *srvPtr
// Equivale a: (*srvPtr).Address()
srvPtr.SetPort(9090) // OK: pointer receiver en puntero
```

### Limitacion: valores no addressable

```go
// La auto-referencia NO funciona si el valor no es addressable

// ✗ ERROR: no se puede tomar la direccion de un map value
m := map[string]Server{
    "web": {Name: "web", Port: 80},
}
// m["web"].SetPort(443) // ERROR: cannot take address of m["web"]

// ✓ Solucion: extraer, modificar, reasignar
srv := m["web"]
srv.SetPort(443)
m["web"] = srv

// ✗ ERROR: no se puede tomar la direccion de un return value
func getServer() Server { return Server{} }
// getServer().SetPort(443) // ERROR: cannot take address of getServer()

// ✓ Solucion: asignar a variable primero
srv := getServer()
srv.SetPort(443)

// Los valores addressable son:
// ├─ Variables
// ├─ Elementos de slice (s[0])
// ├─ Campos de struct addressable
// ├─ Derreferencias de puntero (*p)
// └─ Operaciones de composite literal (&Server{})
//
// NO addressable:
// ├─ Valores de map (m["key"])
// ├─ Valores de retorno de funcion
// └─ Constantes
```

---

## 6. Method sets — la regla que gobierna interfaces

### Definicion

```
El method set de un tipo determina que interfaces puede satisfacer.

Method set de T  (valor):  solo metodos con receiver T
Method set de *T (puntero): metodos con receiver T y *T
```

```go
type Stringer interface {
    String() string
}

type Starter interface {
    Start() error
}

type Server struct {
    Name string
}

func (s Server) String() string {     // value receiver
    return s.Name
}

func (s *Server) Start() error {       // pointer receiver
    fmt.Printf("Starting %s\n", s.Name)
    return nil
}

// Method set de Server:  { String }
// Method set de *Server: { String, Start }

var _ Stringer = Server{}    // ✓ Server tiene String()
var _ Stringer = &Server{}   // ✓ *Server tambien tiene String()
var _ Starter = &Server{}    // ✓ *Server tiene Start()
// var _ Starter = Server{}  // ✗ ERROR: Server no tiene Start()
```

### ¿Por que esta asimetria?

```go
// Razon: si tienes un valor T, Go no siempre puede obtener un *T

// Caso 1: variable — Go puede tomar &
srv := Server{Name: "web"}
srv.Start() // Go hace (&srv).Start() — OK porque srv es addressable

// Caso 2: valor en interface — Go NO puede tomar &
var s Stringer = Server{Name: "web"} // Server valor dentro de interface
// Si Start() estuviera en el method set de Server,
// Go necesitaria &s.(Server) para llamarlo
// Pero el valor dentro de la interface no es addressable
// → Go no lo permite

// Por eso:
// - Value en interface → solo puede llamar value receiver methods
// - Pointer en interface → puede llamar ambos
```

### Tabla de method sets

```
┌──────────────────┬────────────────────────────┬────────────────────────────┐
│ Tipo             │ Method Set                 │ Puede satisfacer           │
├──────────────────┼────────────────────────────┼────────────────────────────┤
│ T (valor)        │ Metodos con receiver T     │ Interfaces con solo        │
│                  │                            │ metodos de receiver T      │
├──────────────────┼────────────────────────────┼────────────────────────────┤
│ *T (puntero)     │ Metodos con receiver T     │ Cualquier interface        │
│                  │ + metodos con receiver *T  │ (tiene todos los metodos)  │
└──────────────────┴────────────────────────────┴────────────────────────────┘
```

### Implicacion practica

```go
type Writer interface {
    Write([]byte) (int, error)
}

type Logger struct {
    out io.Writer
}

// Si usas pointer receiver:
func (l *Logger) Write(p []byte) (int, error) {
    return l.out.Write(p)
}

// Entonces:
var w Writer = &Logger{out: os.Stdout} // ✓ *Logger satisface Writer
// var w Writer = Logger{out: os.Stdout} // ✗ Logger NO satisface Writer

// Esto es importante cuando almacenas en slices o maps de interfaces:
loggers := []Writer{
    &Logger{out: os.Stdout}, // ✓ Necesitas &
    &Logger{out: os.Stderr}, // ✓ Necesitas &
}
```

---

## 7. Metodos en tipos no-struct

### Metodos en tipos basicos envueltos

```go
// Puedes definir metodos en CUALQUIER tipo definido por el usuario
// No solo structs

type Duration int64 // Milisegundos

func (d Duration) String() string {
    if d < 1000 {
        return fmt.Sprintf("%dms", d)
    }
    if d < 60000 {
        return fmt.Sprintf("%.1fs", float64(d)/1000)
    }
    return fmt.Sprintf("%.1fm", float64(d)/60000)
}

func (d Duration) Seconds() float64 {
    return float64(d) / 1000
}

latency := Duration(1500)
fmt.Println(latency)           // "1.5s" (Stringer interface)
fmt.Println(latency.Seconds()) // 1.5
```

### Metodos en tipos string

```go
type LogLevel string

const (
    DEBUG LogLevel = "DEBUG"
    INFO  LogLevel = "INFO"
    WARN  LogLevel = "WARN"
    ERROR LogLevel = "ERROR"
)

func (l LogLevel) Priority() int {
    switch l {
    case DEBUG: return 0
    case INFO:  return 1
    case WARN:  return 2
    case ERROR: return 3
    default:    return -1
    }
}

func (l LogLevel) IsAtLeast(other LogLevel) bool {
    return l.Priority() >= other.Priority()
}

func (l LogLevel) ColorCode() string {
    switch l {
    case DEBUG: return "\033[36m" // Cyan
    case INFO:  return "\033[32m" // Green
    case WARN:  return "\033[33m" // Yellow
    case ERROR: return "\033[31m" // Red
    default:    return "\033[0m"
    }
}

if level.IsAtLeast(WARN) {
    alert(msg)
}
```

### Metodos en tipos slice

```go
type Hosts []string

func (h Hosts) Contains(host string) bool {
    for _, item := range h {
        if item == host {
            return true
        }
    }
    return false
}

func (h Hosts) Filter(pred func(string) bool) Hosts {
    var result Hosts
    for _, item := range h {
        if pred(item) {
            result = append(result, item)
        }
    }
    return result
}

func (h Hosts) String() string {
    return strings.Join(h, ", ")
}

servers := Hosts{"web01", "web02", "db01", "db02", "cache01"}
webServers := servers.Filter(func(s string) bool {
    return strings.HasPrefix(s, "web")
})
fmt.Println(webServers) // "web01, web02"
```

### Metodos en tipos map

```go
type Headers map[string]string

func (h Headers) Get(key string) string {
    return h[strings.ToLower(key)]
}

func (h Headers) Set(key, value string) {
    h[strings.ToLower(key)] = value
}

func (h Headers) Has(key string) bool {
    _, ok := h[strings.ToLower(key)]
    return ok
}

func (h Headers) String() string {
    var parts []string
    for k, v := range h {
        parts = append(parts, fmt.Sprintf("%s: %s", k, v))
    }
    sort.Strings(parts)
    return strings.Join(parts, "\n")
}

hdrs := Headers{}
hdrs.Set("Content-Type", "application/json")
hdrs.Set("Authorization", "Bearer token123")
fmt.Println(hdrs.Get("content-type")) // "application/json"
```

### Restriccion: no puedes definir metodos en tipos de otro paquete

```go
// ✗ ERROR: no puedes agregar metodos a tipos de otro paquete
// func (s string) Reverse() string { ... }
// cannot define new methods on non-local type string

// ✗ ERROR: tampoco en alias de paquete estandar
// func (d time.Duration) Pretty() string { ... }
// cannot define new methods on non-local type time.Duration

// ✓ Solucion: crear tu propio tipo
type MyDuration time.Duration

func (d MyDuration) Pretty() string {
    dur := time.Duration(d)
    if dur < time.Second {
        return fmt.Sprintf("%dms", dur.Milliseconds())
    }
    return dur.Round(time.Millisecond).String()
}

// Nota: MyDuration NO hereda los metodos de time.Duration
// Necesitas convertir: time.Duration(d) para usar metodos de Duration
```

---

## 8. Consistency rule: value o pointer, no mezclar

### La regla

```go
// Si un tipo tiene ALGUN metodo con pointer receiver,
// TODOS los metodos deberian usar pointer receiver

// ✗ Inconsistente:
type Service struct {
    Name   string
    Status string
}

func (s *Service) Start() error { ... }    // pointer
func (s Service) IsRunning() bool { ... }  // value — inconsistente

// ✓ Consistente:
func (s *Service) Start() error { ... }      // pointer
func (s *Service) IsRunning() bool { ... }   // pointer (por consistencia)

// ¿Por que?
// 1. Evita confusion sobre si un metodo modifica o no
// 2. Simplifica el method set: *Service satisface cualquier interface
// 3. Previene copias accidentales del struct (especialmente si tiene mutex)
// 4. Es la convencion de la comunidad Go
```

### Excepciones validas

```go
// 1. Stringer/Formatter que no modifican — a veces se deja como value
//    Pero la mayoria de los proyectos usan pointer por consistencia

// 2. Tipos pequenos sin estado mutable
type Point struct {
    X, Y float64
}

// Todos value — Point es pequeno e inmutable
func (p Point) Distance(other Point) float64 { ... }
func (p Point) Add(other Point) Point { ... }
func (p Point) Scale(factor float64) Point { ... }
func (p Point) String() string { ... }
```

---

## 9. Metodos y interfaces — patrones fundamentales

### Satisfacer io.Writer

```go
type CountingWriter struct {
    writer io.Writer
    count  int64
}

func (cw *CountingWriter) Write(p []byte) (int, error) {
    n, err := cw.writer.Write(p)
    cw.count += int64(n) // Modifica: pointer receiver
    return n, err
}

func (cw *CountingWriter) BytesWritten() int64 {
    return cw.count
}

// Uso:
cw := &CountingWriter{writer: os.Stdout}
fmt.Fprintln(cw, "hello world")
fmt.Printf("Wrote %d bytes\n", cw.BytesWritten())
```

### Satisfacer fmt.Stringer

```go
// fmt.Stringer:
// type Stringer interface {
//     String() string
// }

type ServiceStatus struct {
    Name    string
    Active  bool
    PID     int
    Uptime  time.Duration
}

func (s ServiceStatus) String() string {
    status := "inactive"
    if s.Active {
        status = fmt.Sprintf("active (PID %d, up %s)", s.PID, s.Uptime.Round(time.Second))
    }
    return fmt.Sprintf("%-20s %s", s.Name, status)
}

// Ahora fmt.Println lo formatea automaticamente:
status := ServiceStatus{Name: "nginx", Active: true, PID: 1234, Uptime: 3*time.Hour}
fmt.Println(status) // "nginx                active (PID 1234, up 3h0m0s)"
```

### Satisfacer error interface

```go
// error:
// type error interface {
//     Error() string
// }

type ServiceError struct {
    Service string
    Op      string
    Err     error
}

func (e *ServiceError) Error() string {
    return fmt.Sprintf("service %s: %s: %v", e.Service, e.Op, e.Err)
}

func (e *ServiceError) Unwrap() error {
    return e.Err
}

// Uso:
func startService(name string) error {
    if err := exec.Command("systemctl", "start", name).Run(); err != nil {
        return &ServiceError{Service: name, Op: "start", Err: err}
    }
    return nil
}

if err := startService("nginx"); err != nil {
    var svcErr *ServiceError
    if errors.As(err, &svcErr) {
        fmt.Printf("Service: %s, Operation: %s\n", svcErr.Service, svcErr.Op)
    }
}
```

### Satisfacer sort.Interface

```go
// sort.Interface:
// type Interface interface {
//     Len() int
//     Less(i, j int) bool
//     Swap(i, j int)
// }

type ServiceList []Service

func (sl ServiceList) Len() int           { return len(sl) }
func (sl ServiceList) Less(i, j int) bool { return sl[i].Name < sl[j].Name }
func (sl ServiceList) Swap(i, j int)      { sl[i], sl[j] = sl[j], sl[i] }

// Metodos adicionales (no parte de la interface)
func (sl ServiceList) FindByName(name string) (Service, bool) {
    for _, s := range sl {
        if s.Name == name {
            return s, true
        }
    }
    return Service{}, false
}

services := ServiceList{
    {Name: "redis"}, {Name: "nginx"}, {Name: "postgres"},
}
sort.Sort(services) // Ordena in-place
// [{nginx} {postgres} {redis}]

// Nota: Swap modifica el slice (reslice, no el header)
// Value receiver funciona aqui porque slices son reference types
// El slice header es una copia, pero apunta al mismo backing array
```

---

## 10. Embedding y metodos — composicion

### Promoted methods

```go
type Logger struct {
    Prefix string
}

func (l *Logger) Log(msg string) {
    fmt.Printf("[%s] %s\n", l.Prefix, msg)
}

func (l *Logger) Logf(format string, args ...any) {
    l.Log(fmt.Sprintf(format, args...))
}

type Server struct {
    *Logger // Embedding — Server "hereda" Log y Logf
    Name    string
    Port    int
}

srv := &Server{
    Logger: &Logger{Prefix: "server"},
    Name:   "api",
    Port:   8080,
}

// Metodos de Logger son "promovidos" a Server
srv.Log("starting") // [server] starting
srv.Logf("listening on :%d", srv.Port) // [server] listening on :8080

// Internamente es: srv.Logger.Log("starting")
```

### Override de metodos promovidos

```go
type Base struct{}

func (b Base) String() string {
    return "Base"
}

type Derived struct {
    Base // Embedding
    Name string
}

// Override: Derived define su propio String()
func (d Derived) String() string {
    return fmt.Sprintf("Derived(%s)", d.Name)
}

d := Derived{Name: "test"}
fmt.Println(d)          // "Derived(test)" — usa String() de Derived
fmt.Println(d.Base)     // "Base" — acceso explicito al embebido
```

### Embedding de multiples tipos

```go
type ReadCloser struct {
    io.Reader // Promueve Read()
    io.Closer // Promueve Close()
}

// ReadCloser satisface io.ReadCloser automaticamente
// porque tiene tanto Read([]byte)(int, error) como Close() error

// Pero si ambos embebidos tienen un metodo con el mismo nombre:
type A struct{}
func (A) Method() string { return "A" }

type B struct{}
func (B) Method() string { return "B" }

type C struct {
    A
    B
}

// c.Method() → ERROR ambiguo: ambiguous selector c.Method
// Solucion: C debe definir su propio Method() o acceder explicitamente:
// c.A.Method() o c.B.Method()
```

### Embedding para SysAdmin: composicion real

```go
// Composicion de componentes de un servicio

type HealthChecker struct {
    endpoint string
    timeout  time.Duration
}

func (hc *HealthChecker) CheckHealth() error {
    client := &http.Client{Timeout: hc.timeout}
    resp, err := client.Get(hc.endpoint)
    if err != nil {
        return err
    }
    resp.Body.Close()
    if resp.StatusCode != 200 {
        return fmt.Errorf("unhealthy: status %d", resp.StatusCode)
    }
    return nil
}

type MetricsCollector struct {
    mu       sync.Mutex
    requests int64
    errors   int64
}

func (mc *MetricsCollector) RecordRequest() {
    mc.mu.Lock()
    mc.requests++
    mc.mu.Unlock()
}

func (mc *MetricsCollector) RecordError() {
    mc.mu.Lock()
    mc.errors++
    mc.mu.Unlock()
}

func (mc *MetricsCollector) Stats() (requests, errors int64) {
    mc.mu.Lock()
    defer mc.mu.Unlock()
    return mc.requests, mc.errors
}

type GracefulShutdown struct {
    done     chan struct{}
    timeout  time.Duration
}

func (gs *GracefulShutdown) Shutdown() {
    close(gs.done)
}

func (gs *GracefulShutdown) WaitForShutdown() <-chan struct{} {
    return gs.done
}

// Servicio completo: compone las tres capacidades
type APIServer struct {
    *HealthChecker     // Promueve CheckHealth()
    *MetricsCollector  // Promueve RecordRequest(), RecordError(), Stats()
    *GracefulShutdown  // Promueve Shutdown(), WaitForShutdown()
    
    name string
    addr string
}

func NewAPIServer(name, addr string) *APIServer {
    return &APIServer{
        HealthChecker: &HealthChecker{
            endpoint: fmt.Sprintf("http://%s/health", addr),
            timeout:  5 * time.Second,
        },
        MetricsCollector: &MetricsCollector{},
        GracefulShutdown: &GracefulShutdown{
            done:    make(chan struct{}),
            timeout: 30 * time.Second,
        },
        name: name,
        addr: addr,
    }
}

// srv tiene TODOS los metodos de los tres componentes:
srv := NewAPIServer("api", "localhost:8080")
srv.RecordRequest()     // de MetricsCollector
srv.CheckHealth()       // de HealthChecker
srv.Shutdown()          // de GracefulShutdown
```

---

## 11. Metodos y sync primitives

### CRITICO: nunca copiar un mutex

```go
type SafeCounter struct {
    mu sync.Mutex
    n  int
}

// ✓ Pointer receiver: no copia el mutex
func (sc *SafeCounter) Increment() {
    sc.mu.Lock()
    defer sc.mu.Unlock()
    sc.n++
}

// ✗ Value receiver: COPIA el mutex → bug catastrofico
// func (sc SafeCounter) Value() int {
//     sc.mu.Lock() // Lock en la COPIA del mutex, no en el original
//     defer sc.mu.Unlock()
//     return sc.n
// }

// ✓ Pointer receiver incluso para lectura
func (sc *SafeCounter) Value() int {
    sc.mu.Lock()
    defer sc.mu.Unlock()
    return sc.n
}

// go vet detecta esto:
// go vet ./...
// "copies lock value: SafeCounter contains sync.Mutex"
```

### Tipos con sync que SIEMPRE necesitan pointer receiver

```go
// Estos tipos NUNCA deben copiarse:
// sync.Mutex
// sync.RWMutex
// sync.WaitGroup
// sync.Cond
// sync.Once
// sync.Pool
// sync.Map
// atomic.Value
// bytes.Buffer (contiene bootstrap array)

// Si tu struct contiene alguno de estos → pointer receiver SIEMPRE
type Cache struct {
    mu    sync.RWMutex       // ← pointer receiver obligatorio
    items map[string][]byte
    once  sync.Once          // ← pointer receiver obligatorio
}
```

### Verificar con go vet

```bash
# go vet detecta copias de locks automaticamente:
$ go vet ./...
# ./main.go:15:9: call of fmt.Println copies lock value: SafeCounter contains sync.Mutex

# Tambien detecta copias en:
# - Asignacion: x = y (si y contiene lock)
# - Paso de argumento: f(y) (si y contiene lock)
# - Return: return y (si y contiene lock)
# - Range: for _, v := range slice (si v contiene lock)
```

---

## 12. Method values y method expressions (profundizado)

### Method value: receptor bound

```go
type Notifier struct {
    channel string
}

func (n *Notifier) Send(msg string) {
    fmt.Printf("[%s] %s\n", n.channel, msg)
}

slack := &Notifier{channel: "#alerts"}
email := &Notifier{channel: "ops@company.com"}

// Method values: funciones con receptor pre-vinculado
sendSlack := slack.Send   // func(string)
sendEmail := email.Send   // func(string)

sendSlack("disk full")    // [#alerts] disk full
sendEmail("disk full")    // [ops@company.com] disk full

// Util para pasar a APIs que esperan func(string):
var notifiers []func(string)
notifiers = append(notifiers, slack.Send, email.Send)

for _, notify := range notifiers {
    notify("service down")
}
```

### Method expression: receptor como primer argumento

```go
// Method expression: el tipo se convierte en el primer parametro

send := (*Notifier).Send // func(*Notifier, string)

send(slack, "via expression") // [#alerts] via expression
send(email, "via expression") // [ops@company.com] via expression

// Util para apply a multiples instancias:
notifiers := []*Notifier{slack, email}
for _, n := range notifiers {
    (*Notifier).Send(n, "broadcast message")
}
```

### Method value con goroutines y callbacks

```go
type Worker struct {
    id   int
    done chan struct{}
}

func (w *Worker) Process(job string) {
    fmt.Printf("Worker %d processing: %s\n", w.id, job)
}

func (w *Worker) Stop() {
    close(w.done)
    fmt.Printf("Worker %d stopped\n", w.id)
}

worker := &Worker{id: 1, done: make(chan struct{})}

// Pasar method value como callback:
go func() {
    <-worker.done
    fmt.Println("Worker done callback fired")
}()

// Pasar method value a otra funcion:
runJob("backup", worker.Process) // Process es func(string)

// Registrar cleanup con defer:
defer worker.Stop()
```

---

## 13. Patron constructor New y metodos

### Constructor idiomatico

```go
type ConnectionPool struct {
    host     string
    port     int
    maxConns int
    conns    chan net.Conn
    mu       sync.Mutex
    closed   bool
}

func NewConnectionPool(host string, port, maxConns int) (*ConnectionPool, error) {
    if maxConns <= 0 {
        return nil, fmt.Errorf("maxConns must be positive, got %d", maxConns)
    }
    
    pool := &ConnectionPool{
        host:     host,
        port:     port,
        maxConns: maxConns,
        conns:    make(chan net.Conn, maxConns),
    }
    
    // Pre-llenar con una conexion para validar
    conn, err := pool.dial()
    if err != nil {
        return nil, fmt.Errorf("cannot connect to %s:%d: %w", host, port, err)
    }
    pool.conns <- conn
    
    return pool, nil
}

func (p *ConnectionPool) dial() (net.Conn, error) {
    return net.DialTimeout("tcp", fmt.Sprintf("%s:%d", p.host, p.port), 5*time.Second)
}

func (p *ConnectionPool) Get() (net.Conn, error) {
    p.mu.Lock()
    if p.closed {
        p.mu.Unlock()
        return nil, fmt.Errorf("pool is closed")
    }
    p.mu.Unlock()
    
    select {
    case conn := <-p.conns:
        return conn, nil
    default:
        return p.dial()
    }
}

func (p *ConnectionPool) Put(conn net.Conn) {
    p.mu.Lock()
    if p.closed {
        p.mu.Unlock()
        conn.Close()
        return
    }
    p.mu.Unlock()
    
    select {
    case p.conns <- conn:
    default:
        conn.Close() // Pool lleno
    }
}

func (p *ConnectionPool) Close() error {
    p.mu.Lock()
    defer p.mu.Unlock()
    
    if p.closed {
        return nil
    }
    p.closed = true
    close(p.conns)
    
    var lastErr error
    for conn := range p.conns {
        if err := conn.Close(); err != nil {
            lastErr = err
        }
    }
    return lastErr
}

// Uso:
pool, err := NewConnectionPool("db-server", 5432, 10)
if err != nil {
    log.Fatal(err)
}
defer pool.Close()

conn, err := pool.Get()
if err != nil {
    log.Fatal(err)
}
defer pool.Put(conn)
```

### Constructor con functional options

```go
type Server struct {
    host         string
    port         int
    readTimeout  time.Duration
    writeTimeout time.Duration
    logger       *log.Logger
    tls          bool
}

type ServerOption func(*Server)

func WithPort(port int) ServerOption {
    return func(s *Server) { s.port = port }
}

func WithTimeouts(read, write time.Duration) ServerOption {
    return func(s *Server) {
        s.readTimeout = read
        s.writeTimeout = write
    }
}

func WithLogger(l *log.Logger) ServerOption {
    return func(s *Server) { s.logger = l }
}

func WithTLS() ServerOption {
    return func(s *Server) { s.tls = true }
}

func NewServer(host string, opts ...ServerOption) *Server {
    s := &Server{
        host:         host,
        port:         8080,
        readTimeout:  5 * time.Second,
        writeTimeout: 10 * time.Second,
        logger:       log.Default(),
    }
    for _, opt := range opts {
        opt(s)
    }
    return s
}

func (s *Server) Start() error {
    s.logger.Printf("Starting server on %s:%d (TLS: %v)", s.host, s.port, s.tls)
    // ...
    return nil
}

func (s *Server) ListenAddr() string {
    return fmt.Sprintf("%s:%d", s.host, s.port)
}
```

---

## 14. Comparacion: Go vs C vs Rust

### Go: metodos con receptores

```go
type Server struct {
    Name string
    Port int
}

// Method con receptor — azucar sintactico
func (s *Server) Start() error {
    fmt.Printf("Starting %s on :%d\n", s.Name, s.Port)
    return nil
}

// Llamada:
srv := &Server{Name: "web", Port: 80}
srv.Start()
```

### C: funciones con puntero a struct

```c
// C no tiene metodos. Simulas el patron pasando el struct:

typedef struct {
    char name[64];
    int port;
} Server;

// "Metodo" = funcion que recibe puntero al struct
int server_start(Server *s) {
    printf("Starting %s on :%d\n", s->name, s->port);
    return 0;
}

// Llamada:
Server srv = {.name = "web", .port = 80};
server_start(&srv);  // Explicitamente pasas el puntero

// Convencion: prefijo con nombre del tipo (server_)
// No hay dispatch automatico ni method sets
// No hay interfaces — simulas con function pointers:

typedef struct {
    int (*start)(void *self);
    int (*stop)(void *self);
    void *data;
} ServiceVTable;
// Esto es basicamente una vtable manual
```

### Rust: impl blocks y traits

```rust
struct Server {
    name: String,
    port: u16,
}

// Bloque impl: define metodos para Server
impl Server {
    // Constructor (convencion, no syntax especial)
    fn new(name: &str, port: u16) -> Self {
        Server {
            name: name.to_string(),
            port,
        }
    }
    
    // &self = referencia inmutable (como value receiver)
    fn address(&self) -> String {
        format!("{}:{}", self.name, self.port)
    }
    
    // &mut self = referencia mutable (como pointer receiver)
    fn set_port(&mut self, port: u16) {
        self.port = port;
    }
    
    // self = toma ownership (consume el valor)
    fn into_address(self) -> String {
        format!("{}:{}", self.name, self.port)
        // self ya no existe despues de esta llamada
    }
}

// Implementar trait (como satisfacer interface)
impl std::fmt::Display for Server {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        write!(f, "Server({}:{})", self.name, self.port)
    }
}

let mut srv = Server::new("web", 80);
println!("{}", srv.address());  // "web:80"
srv.set_port(443);
println!("{}", srv);            // "Server(web:443)"
```

### Tabla comparativa

```
┌──────────────────────┬────────────────────────┬─────────────────────┬──────────────────────────┐
│ Aspecto              │ Go                     │ C                   │ Rust                     │
├──────────────────────┼────────────────────────┼─────────────────────┼──────────────────────────┤
│ Metodos              │ func (r T) M()         │ No (funciones con   │ impl T { fn m(&self) }   │
│                      │                        │ struct*)            │                          │
│ Self/this            │ Receptor nombrado (s)  │ Parametro explicito │ &self, &mut self, self   │
│ Mutable vs inmutable │ *T vs T (receiver)     │ const T* vs T*      │ &mut self vs &self       │
│ Ownership en metodo  │ N/A (GC)              │ N/A (manual)        │ self (consume el valor)  │
│ Method sets          │ T: {T methods}         │ N/A                 │ Determinado por impl     │
│                      │ *T: {T + *T methods}   │                     │ + trait impl             │
│ Interfaces           │ Satisfaccion implicita │ vtable manual       │ impl Trait for T         │
│                      │                        │                     │ (explicita)              │
│ Constructor          │ NewXxx() (convencion)  │ xxx_new() (conv.)   │ T::new() (convencion)    │
│ Herencia             │ No (embedding)         │ No (composicion)    │ No (traits + composicion)│
│ Overloading          │ No                     │ No                  │ No (pero traits)         │
│ Method en types      │ Solo tipos del paquete │ Cualquier struct    │ Solo tipos del crate     │
│ Auto-deref           │ Si (. en punteros)     │ No (-> vs .)        │ Si (auto-deref chain)    │
│ Nil receiver         │ Valido (*T methods)    │ UB (NULL->field)    │ Imposible (no null)      │
│ Generic methods      │ No (Go 1.18: en tipo)  │ No (macros)         │ Si (full generics)       │
└──────────────────────┴────────────────────────┴─────────────────────┴──────────────────────────┘
```

**Diferencias clave**:
- **Go**: dos niveles de receptor (T y *T), method sets determinan interfaces, embedding para composicion
- **C**: simulacion manual, sin safety del compilador, vtables artesanales
- **Rust**: tres niveles (&self, &mut self, self), ownership system previene bugs en compile time, traits son explicitas

---

## 15. Patrones SysAdmin con metodos

### Service manager

```go
type ServiceState int

const (
    StateStopped ServiceState = iota
    StateStarting
    StateRunning
    StateStopping
    StateFailed
)

func (s ServiceState) String() string {
    return [...]string{"stopped", "starting", "running", "stopping", "failed"}[s]
}

type ManagedService struct {
    mu          sync.Mutex
    name        string
    state       ServiceState
    pid         int
    startCount  int
    lastStarted time.Time
    lastError   error
    onStateChange func(name string, from, to ServiceState)
}

func NewManagedService(name string) *ManagedService {
    return &ManagedService{
        name:  name,
        state: StateStopped,
    }
}

func (ms *ManagedService) Name() string {
    return ms.name
}

func (ms *ManagedService) State() ServiceState {
    ms.mu.Lock()
    defer ms.mu.Unlock()
    return ms.state
}

func (ms *ManagedService) setState(newState ServiceState) {
    oldState := ms.state
    ms.state = newState
    if ms.onStateChange != nil {
        ms.onStateChange(ms.name, oldState, newState)
    }
}

func (ms *ManagedService) Start() error {
    ms.mu.Lock()
    defer ms.mu.Unlock()
    
    if ms.state == StateRunning {
        return fmt.Errorf("service %s already running (PID %d)", ms.name, ms.pid)
    }
    
    ms.setState(StateStarting)
    
    cmd := exec.Command("systemctl", "start", ms.name)
    if err := cmd.Run(); err != nil {
        ms.lastError = err
        ms.setState(StateFailed)
        return fmt.Errorf("start %s: %w", ms.name, err)
    }
    
    // Obtener PID
    out, err := exec.Command("systemctl", "show", "-p", "MainPID", ms.name).Output()
    if err == nil {
        fmt.Sscanf(strings.TrimPrefix(string(out), "MainPID="), "%d", &ms.pid)
    }
    
    ms.startCount++
    ms.lastStarted = time.Now()
    ms.lastError = nil
    ms.setState(StateRunning)
    return nil
}

func (ms *ManagedService) Stop() error {
    ms.mu.Lock()
    defer ms.mu.Unlock()
    
    if ms.state == StateStopped {
        return nil
    }
    
    ms.setState(StateStopping)
    
    cmd := exec.Command("systemctl", "stop", ms.name)
    if err := cmd.Run(); err != nil {
        ms.lastError = err
        ms.setState(StateFailed)
        return fmt.Errorf("stop %s: %w", ms.name, err)
    }
    
    ms.pid = 0
    ms.setState(StateStopped)
    return nil
}

func (ms *ManagedService) Restart() error {
    if err := ms.Stop(); err != nil {
        return err
    }
    return ms.Start()
}

func (ms *ManagedService) Status() string {
    ms.mu.Lock()
    defer ms.mu.Unlock()
    
    info := fmt.Sprintf("%-20s %-10s", ms.name, ms.state)
    if ms.state == StateRunning {
        info += fmt.Sprintf(" PID=%-6d uptime=%s starts=%d",
            ms.pid, time.Since(ms.lastStarted).Round(time.Second), ms.startCount)
    }
    if ms.lastError != nil {
        info += fmt.Sprintf(" last_error=%v", ms.lastError)
    }
    return info
}

func (ms *ManagedService) OnStateChange(fn func(name string, from, to ServiceState)) {
    ms.mu.Lock()
    defer ms.mu.Unlock()
    ms.onStateChange = fn
}
```

### Metrics buffer con flush

```go
type MetricsBuffer struct {
    mu       sync.Mutex
    buffer   []Metric
    capacity int
    flushFn  func([]Metric) error
}

func NewMetricsBuffer(capacity int, flush func([]Metric) error) *MetricsBuffer {
    return &MetricsBuffer{
        buffer:   make([]Metric, 0, capacity),
        capacity: capacity,
        flushFn:  flush,
    }
}

func (mb *MetricsBuffer) Add(m Metric) error {
    mb.mu.Lock()
    defer mb.mu.Unlock()
    
    mb.buffer = append(mb.buffer, m)
    if len(mb.buffer) >= mb.capacity {
        return mb.flushLocked()
    }
    return nil
}

func (mb *MetricsBuffer) Flush() error {
    mb.mu.Lock()
    defer mb.mu.Unlock()
    return mb.flushLocked()
}

// Metodo privado (minuscula) — solo accesible dentro del paquete
func (mb *MetricsBuffer) flushLocked() error {
    if len(mb.buffer) == 0 {
        return nil
    }
    
    // Copiar buffer para liberar el lock rapido
    batch := make([]Metric, len(mb.buffer))
    copy(batch, mb.buffer)
    mb.buffer = mb.buffer[:0] // Reset sin realocar
    
    return mb.flushFn(batch)
}

func (mb *MetricsBuffer) Len() int {
    mb.mu.Lock()
    defer mb.mu.Unlock()
    return len(mb.buffer)
}
```

---

## 16. La regla de decision: ¿value o pointer?

### Flowchart

```
¿Que tipo de receptor usar?
│
├─ ¿El metodo modifica el receptor?
│  └─ Si → POINTER receiver (*T)
│
├─ ¿El struct contiene sync.Mutex, chan, o func fields?
│  └─ Si → POINTER receiver (*T) (no se pueden copiar)
│
├─ ¿El struct es grande (>= ~64 bytes)?
│  └─ Si → POINTER receiver (*T) (evitar copias costosas)
│
├─ ¿Otros metodos del tipo usan pointer receiver?
│  └─ Si → POINTER receiver (*T) (consistencia)
│
├─ ¿El tipo es un tipo basico pequeño (Point, Color, Time)?
│  └─ Si → VALUE receiver (T) (inmutable, seguro)
│
└─ ¿Ninguna de las anteriores aplica?
   └─ Probablemente POINTER receiver (*T) (default seguro)
```

### Resumen en una tabla

```
┌─────────────────────────────────┬──────────────┬───────────────┐
│ Situacion                       │ Value (T)    │ Pointer (*T)  │
├─────────────────────────────────┼──────────────┼───────────────┤
│ Modifica el receptor            │              │ ✓             │
│ Struct grande (>64 bytes)       │              │ ✓             │
│ Contiene sync primitives        │              │ ✓ (obligado)  │
│ Contiene map, slice, chan       │ Depende      │ ✓ (si modif.) │
│ Tipo basico inmutable           │ ✓            │               │
│ Consistencia con otros metodos  │              │ ✓             │
│ Necesita satisfacer interface   │ Revisa MS    │ ✓ (siempre)   │
│ Concurrencia sin mutex          │ ✓ (copia)    │               │
│ Default cuando no estas seguro  │              │ ✓             │
└─────────────────────────────────┴──────────────┴───────────────┘
```

---

## 17. Programa completo: Infrastructure Manager

Un sistema de gestion de infraestructura que demuestra metodos con value y pointer receivers, method sets, embedding, interfaces, y composicion.

```go
package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"sort"
	"strings"
	"sync"
	"time"
)

// ─── Tipos base con metodos ─────────────────────────────────────

type NodeRole string

const (
	RoleWeb   NodeRole = "web"
	RoleDB    NodeRole = "database"
	RoleCache NodeRole = "cache"
	RoleLB    NodeRole = "loadbalancer"
)

// Metodos en tipo string — value receiver (inmutable)
func (r NodeRole) Icon() string {
	switch r {
	case RoleWeb:   return "[W]"
	case RoleDB:    return "[D]"
	case RoleCache: return "[C]"
	case RoleLB:    return "[L]"
	default:        return "[?]"
	}
}

func (r NodeRole) DefaultPort() int {
	switch r {
	case RoleWeb:   return 80
	case RoleDB:    return 5432
	case RoleCache: return 6379
	case RoleLB:    return 443
	default:        return 0
	}
}

// ─── Node: metodos value y pointer ──────────────────────────────

type NodeStatus int

const (
	StatusUnknown NodeStatus = iota
	StatusHealthy
	StatusDegraded
	StatusDown
)

func (ns NodeStatus) String() string {
	return [...]string{"unknown", "healthy", "degraded", "down"}[ns]
}

type Node struct {
	mu          sync.RWMutex
	Name        string            `json:"name"`
	Host        string            `json:"host"`
	Port        int               `json:"port"`
	Role        NodeRole          `json:"role"`
	Tags        map[string]string `json:"tags"`
	status      NodeStatus
	lastCheck   time.Time
	checkCount  int
	failCount   int
}

func NewNode(name, host string, role NodeRole) *Node {
	return &Node{
		Name: name,
		Host: host,
		Port: role.DefaultPort(), // Usa value receiver de NodeRole
		Role: role,
		Tags: make(map[string]string),
	}
}

// Value receivers: solo leen, tipo es grande pero leemos con lock
// En este caso usamos pointer por consistencia y por el mutex
func (n *Node) Address() string {
	return fmt.Sprintf("%s:%d", n.Host, n.Port)
}

func (n *Node) FullName() string {
	return fmt.Sprintf("%s %s (%s)", n.Role.Icon(), n.Name, n.Host)
}

// Pointer receivers: modifican estado
func (n *Node) SetTag(key, value string) {
	n.mu.Lock()
	defer n.mu.Unlock()
	n.Tags[key] = value
}

func (n *Node) UpdateStatus(status NodeStatus) {
	n.mu.Lock()
	defer n.mu.Unlock()
	n.status = status
	n.lastCheck = time.Now()
	n.checkCount++
	if status == StatusDown || status == StatusDegraded {
		n.failCount++
	}
}

func (n *Node) Status() NodeStatus {
	n.mu.RLock()
	defer n.mu.RUnlock()
	return n.status
}

func (n *Node) FailRate() float64 {
	n.mu.RLock()
	defer n.mu.RUnlock()
	if n.checkCount == 0 {
		return 0
	}
	return float64(n.failCount) / float64(n.checkCount) * 100
}

// Satisface fmt.Stringer
func (n *Node) String() string {
	n.mu.RLock()
	defer n.mu.RUnlock()
	return fmt.Sprintf("%-25s %-10s %-10s checks=%d fails=%d",
		n.FullName(), n.Address(), n.status, n.checkCount, n.failCount)
}

// ─── NodeList: metodos en tipo slice ────────────────────────────

type NodeList []*Node

func (nl NodeList) Len() int           { return len(nl) }
func (nl NodeList) Less(i, j int) bool { return nl[i].Name < nl[j].Name }
func (nl NodeList) Swap(i, j int)      { nl[i], nl[j] = nl[j], nl[i] }

func (nl NodeList) ByRole(role NodeRole) NodeList {
	var result NodeList
	for _, n := range nl {
		if n.Role == role {
			result = append(result, n)
		}
	}
	return result
}

func (nl NodeList) ByStatus(status NodeStatus) NodeList {
	var result NodeList
	for _, n := range nl {
		if n.Status() == status {
			result = append(result, n)
		}
	}
	return result
}

func (nl NodeList) Names() []string {
	names := make([]string, len(nl))
	for i, n := range nl {
		names[i] = n.Name
	}
	return names
}

// ─── Interface: Checker ─────────────────────────────────────────

type Checker interface {
	Check(node *Node) NodeStatus
	Name() string
}

// TCPChecker: pointer receiver (tiene timeout mutable)
type TCPChecker struct {
	timeout time.Duration
}

func NewTCPChecker(timeout time.Duration) *TCPChecker {
	return &TCPChecker{timeout: timeout}
}

func (tc *TCPChecker) Name() string { return "tcp" }

func (tc *TCPChecker) Check(node *Node) NodeStatus {
	addr := node.Address()
	conn, err := net.DialTimeout("tcp", addr, tc.timeout)
	if err != nil {
		return StatusDown
	}
	conn.Close()
	return StatusHealthy
}

// ProcessChecker: pointer receiver (ejecuta comandos)
type ProcessChecker struct {
	timeout time.Duration
}

func NewProcessChecker(timeout time.Duration) *ProcessChecker {
	return &ProcessChecker{timeout: timeout}
}

func (pc *ProcessChecker) Name() string { return "process" }

func (pc *ProcessChecker) Check(node *Node) NodeStatus {
	cmd := exec.Command("systemctl", "is-active", node.Name)
	output, err := cmd.Output()
	if err != nil {
		return StatusDown
	}
	status := strings.TrimSpace(string(output))
	switch status {
	case "active":
		return StatusHealthy
	case "activating", "reloading":
		return StatusDegraded
	default:
		return StatusDown
	}
}

// ─── Cluster: composicion con embedding ─────────────────────────

type EventLog struct {
	mu     sync.Mutex
	events []string
}

func (el *EventLog) Record(format string, args ...any) {
	el.mu.Lock()
	defer el.mu.Unlock()
	entry := fmt.Sprintf("[%s] %s",
		time.Now().Format("15:04:05"),
		fmt.Sprintf(format, args...))
	el.events = append(el.events, entry)
	log.Println(entry)
}

func (el *EventLog) Recent(n int) []string {
	el.mu.Lock()
	defer el.mu.Unlock()
	if n > len(el.events) {
		n = len(el.events)
	}
	result := make([]string, n)
	copy(result, el.events[len(el.events)-n:])
	return result
}

type Cluster struct {
	*EventLog // Embedding: promueve Record(), Recent()

	mu       sync.RWMutex
	name     string
	nodes    NodeList
	checkers []Checker
}

func NewCluster(name string) *Cluster {
	return &Cluster{
		EventLog: &EventLog{},
		name:     name,
	}
}

func (c *Cluster) AddNode(nodes ...*Node) {
	c.mu.Lock()
	defer c.mu.Unlock()
	for _, n := range nodes {
		c.nodes = append(c.nodes, n)
		c.Record("node added: %s (%s)", n.Name, n.Role)
	}
}

func (c *Cluster) AddChecker(checkers ...Checker) {
	c.mu.Lock()
	defer c.mu.Unlock()
	c.checkers = append(c.checkers, checkers...)
	for _, ch := range checkers {
		c.Record("checker registered: %s", ch.Name())
	}
}

func (c *Cluster) Nodes() NodeList {
	c.mu.RLock()
	defer c.mu.RUnlock()
	result := make(NodeList, len(c.nodes))
	copy(result, c.nodes)
	return result
}

func (c *Cluster) RunChecks() {
	c.mu.RLock()
	nodes := make(NodeList, len(c.nodes))
	copy(nodes, c.nodes)
	checkers := make([]Checker, len(c.checkers))
	copy(checkers, c.checkers)
	c.mu.RUnlock()

	var wg sync.WaitGroup
	for _, node := range nodes {
		for _, checker := range checkers {
			wg.Add(1)
			go func(n *Node, ch Checker) {
				defer wg.Done()
				status := ch.Check(n)
				oldStatus := n.Status()
				n.UpdateStatus(status)
				if status != oldStatus {
					c.Record("status change: %s %s -> %s (checker: %s)",
						n.Name, oldStatus, status, ch.Name())
				}
			}(node, checker)
		}
	}
	wg.Wait()
}

// ─── Report: value receiver (inmutable, snapshot) ───────────────

type ClusterReport struct {
	Name      string        `json:"name"`
	Timestamp time.Time     `json:"timestamp"`
	Total     int           `json:"total_nodes"`
	Healthy   int           `json:"healthy"`
	Degraded  int           `json:"degraded"`
	Down      int           `json:"down"`
	Unknown   int           `json:"unknown"`
	ByRole    map[string]int `json:"by_role"`
}

// Value receiver: Report es un snapshot inmutable
func (r ClusterReport) HealthPercent() float64 {
	if r.Total == 0 {
		return 0
	}
	return float64(r.Healthy) / float64(r.Total) * 100
}

func (r ClusterReport) IsHealthy() bool {
	return r.Down == 0 && r.Degraded == 0
}

func (r ClusterReport) String() string {
	var b strings.Builder
	b.WriteString(fmt.Sprintf("=== Cluster: %s ===\n", r.Name))
	b.WriteString(fmt.Sprintf("Time: %s\n", r.Timestamp.Format("2006-01-02 15:04:05")))
	b.WriteString(fmt.Sprintf("Total: %d | Healthy: %d | Degraded: %d | Down: %d | Unknown: %d\n",
		r.Total, r.Healthy, r.Degraded, r.Down, r.Unknown))
	b.WriteString(fmt.Sprintf("Health: %.1f%%\n", r.HealthPercent()))
	if len(r.ByRole) > 0 {
		b.WriteString("By role: ")
		parts := make([]string, 0, len(r.ByRole))
		for role, count := range r.ByRole {
			parts = append(parts, fmt.Sprintf("%s=%d", role, count))
		}
		sort.Strings(parts)
		b.WriteString(strings.Join(parts, ", "))
		b.WriteString("\n")
	}
	return b.String()
}

func (c *Cluster) Report() ClusterReport {
	nodes := c.Nodes()
	report := ClusterReport{
		Name:      c.name,
		Timestamp: time.Now(),
		Total:     len(nodes),
		ByRole:    make(map[string]int),
	}
	for _, n := range nodes {
		switch n.Status() {
		case StatusHealthy:  report.Healthy++
		case StatusDegraded: report.Degraded++
		case StatusDown:     report.Down++
		default:             report.Unknown++
		}
		report.ByRole[string(n.Role)]++
	}
	return report
}

func (c *Cluster) ReportJSON() string {
	report := c.Report()
	data, _ := json.MarshalIndent(report, "", "  ")
	return string(data)
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	cluster := NewCluster("production")

	// Agregar nodos
	cluster.AddNode(
		NewNode("web01", "10.0.1.1", RoleWeb),
		NewNode("web02", "10.0.1.2", RoleWeb),
		NewNode("web03", "10.0.1.3", RoleWeb),
		NewNode("db01", "10.0.2.1", RoleDB),
		NewNode("db02", "10.0.2.2", RoleDB),
		NewNode("cache01", "10.0.3.1", RoleCache),
		NewNode("lb01", "10.0.0.1", RoleLB),
	)

	// Tags usando metodos de pointer receiver
	for _, n := range cluster.Nodes().ByRole(RoleWeb) {
		n.SetTag("tier", "frontend")
		n.SetTag("region", "us-east-1")
	}
	for _, n := range cluster.Nodes().ByRole(RoleDB) {
		n.SetTag("tier", "data")
		n.SetTag("backup", "daily")
	}

	// Simular algunos estados
	nodes := cluster.Nodes()
	nodes[0].UpdateStatus(StatusHealthy)
	nodes[1].UpdateStatus(StatusHealthy)
	nodes[2].UpdateStatus(StatusDegraded)
	nodes[3].UpdateStatus(StatusHealthy)
	nodes[4].UpdateStatus(StatusDown)
	nodes[5].UpdateStatus(StatusHealthy)
	nodes[6].UpdateStatus(StatusHealthy)

	// Listar nodos (usa String() de *Node)
	sort.Sort(cluster.Nodes())
	fmt.Println("=== All Nodes ===")
	for _, n := range cluster.Nodes() {
		fmt.Println(n) // Llama n.String() automaticamente
	}

	// Filtrar por role (metodo de NodeList)
	fmt.Println("\n=== Web Servers ===")
	for _, n := range cluster.Nodes().ByRole(RoleWeb) {
		fmt.Printf("  %s (fail rate: %.1f%%)\n", n.FullName(), n.FailRate())
	}

	// Nodos caidos
	down := cluster.Nodes().ByStatus(StatusDown)
	if len(down) > 0 {
		fmt.Printf("\n=== DOWN: %s ===\n", strings.Join(down.Names(), ", "))
	}

	// Report (value receiver — snapshot inmutable)
	report := cluster.Report()
	fmt.Printf("\n%s", report) // report.String()

	if !report.IsHealthy() {
		fmt.Printf("ALERT: cluster health at %.1f%%\n", report.HealthPercent())
	}

	// JSON report
	fmt.Printf("\n%s\n", cluster.ReportJSON())

	// Event log (promovido de EventLog via embedding)
	fmt.Println("\n=== Recent Events ===")
	for _, event := range cluster.Recent(5) { // Metodo promovido
		fmt.Println(event)
	}
}
```

### Mapa de receptores en el programa

```
┌──────────────────────────────────────────────────────────────┐
│  METODOS Y RECEPTORES EN EL PROGRAMA                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  VALUE RECEIVERS (inmutables, lectura pura)                   │
│  ├─ NodeRole.Icon() string                                   │
│  ├─ NodeRole.DefaultPort() int                               │
│  ├─ NodeStatus.String() string                               │
│  ├─ ClusterReport.HealthPercent() float64                    │
│  ├─ ClusterReport.IsHealthy() bool                           │
│  └─ ClusterReport.String() string                            │
│                                                              │
│  POINTER RECEIVERS (modifican o tienen mutex)                │
│  ├─ *Node: Address, FullName, SetTag, UpdateStatus,          │
│  │         Status, FailRate, String                          │
│  ├─ *TCPChecker: Name, Check                                │
│  ├─ *ProcessChecker: Name, Check                            │
│  ├─ *EventLog: Record, Recent                               │
│  └─ *Cluster: AddNode, AddChecker, Nodes, RunChecks,        │
│               Report, ReportJSON                             │
│                                                              │
│  METODOS EN TIPOS NO-STRUCT                                  │
│  ├─ NodeRole (string) → Icon, DefaultPort                    │
│  ├─ NodeStatus (int) → String                               │
│  └─ NodeList (slice) → Len, Less, Swap, ByRole, ByStatus,   │
│                         Names                                │
│                                                              │
│  INTERFACES SATISFECHAS                                      │
│  ├─ *Node satisface fmt.Stringer (String method)             │
│  ├─ ClusterReport satisface fmt.Stringer                     │
│  ├─ NodeList satisface sort.Interface (Len, Less, Swap)      │
│  ├─ *TCPChecker satisface Checker (Name, Check)              │
│  └─ *ProcessChecker satisface Checker (Name, Check)          │
│                                                              │
│  EMBEDDING                                                   │
│  Cluster embeds *EventLog                                    │
│  └─ Promueve Record() y Recent() a Cluster                   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 18. Tabla de errores comunes

```
┌────┬──────────────────────────────────────┬────────────────────────────────────────┬─────────┐
│ #  │ Error                                │ Solucion                               │ Nivel   │
├────┼──────────────────────────────────────┼────────────────────────────────────────┼─────────┤
│  1 │ Value receiver modifica el receptor  │ Usar pointer receiver (*T)             │ Logic   │
│  2 │ Copiar struct con sync.Mutex         │ Siempre pointer receiver               │ Race    │
│  3 │ T no satisface interface con *T      │ Usar &T o cambiar a value receiver     │ Compile │
│    │ methods                              │                                        │         │
│  4 │ Llamar pointer method en map value   │ Extraer, modificar, reasignar          │ Compile │
│  5 │ Receptor nombrado "this" o "self"    │ Usar 1-2 letras del tipo (s, c, db)    │ Style   │
│  6 │ Receptores inconsistentes en un tipo │ Si uno es pointer, todos pointer       │ Style   │
│  7 │ Nil pointer dereference en metodo    │ Verificar nil al inicio si aplica      │ Runtime │
│  8 │ Definir metodo en tipo de otro pkg   │ Crear tipo propio (type MyT OtherT)    │ Compile │
│  9 │ Ambiguedad por embedding multiple    │ Definir metodo propio o acceso explicito│ Compile │
│ 10 │ Value receiver en struct grande      │ Pointer receiver (evita copia costosa) │ Perf    │
│ 11 │ Ignorar method set al implementar    │ Verificar: var _ Interface = (*T)(nil) │ Compile │
│    │ interfaces                           │                                        │         │
│ 12 │ Mezclar embedding de valor y puntero │ Embedding de puntero si tiene mutex    │ Design  │
└────┴──────────────────────────────────────┴────────────────────────────────────────┴─────────┘
```

---

## 19. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
type Counter struct{ N int }

func (c Counter) Increment() { c.N++ }

ctr := Counter{N: 0}
ctr.Increment()
ctr.Increment()
fmt.Println(ctr.N)
```

<details>
<summary>Respuesta</summary>

```
0
```

Value receiver: `Increment` modifica una copia. El original `ctr` no cambia.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
type Counter struct{ N int }

func (c *Counter) Increment() { c.N++ }

ctr := Counter{N: 0}
ctr.Increment()
ctr.Increment()
fmt.Println(ctr.N)
```

<details>
<summary>Respuesta</summary>

```
2
```

Pointer receiver: Go automaticamente toma `&ctr`. `Increment` modifica el original.
</details>

**Ejercicio 3**: ¿Compila?

```go
type Printer interface {
    Print()
}

type Doc struct{ Text string }

func (d *Doc) Print() { fmt.Println(d.Text) }

var p Printer = Doc{Text: "hello"}
```

<details>
<summary>Respuesta</summary>

No compila. Error: `Doc does not implement Printer (method Print has pointer receiver)`. El method set de `Doc` (valor) no incluye `*Doc` methods. Solucion: `var p Printer = &Doc{Text: "hello"}`.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
type Num int

func (n Num) Double() Num { return n * 2 }
func (n *Num) Triple()    { *n *= 3 }

x := Num(5)
y := x.Double()
x.Triple()
fmt.Println(x, y)
```

<details>
<summary>Respuesta</summary>

```
15 10
```

`x.Double()` retorna 10 sin modificar x. `x.Triple()` modifica x in-place: 5*3=15.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
type Logger struct{ prefix string }

func (l *Logger) Log(msg string) {
    if l == nil {
        fmt.Println("<nil logger>", msg)
        return
    }
    fmt.Println(l.prefix, msg)
}

var l *Logger
l.Log("hello")
```

<details>
<summary>Respuesta</summary>

```
<nil logger> hello
```

Llamar un metodo con pointer receiver en nil es valido. El metodo verifica `l == nil` y maneja el caso.
</details>

**Ejercicio 6**: ¿Compila?

```go
m := map[string]Counter{"a": {N: 1}}
m["a"].Increment() // Increment tiene pointer receiver
```

<details>
<summary>Respuesta</summary>

No compila. Error: `cannot take address of m["a"]`. Los valores de map no son addressable, asi que Go no puede tomar `&m["a"]` para el pointer receiver. Solucion: extraer a variable, modificar, reasignar.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
type Base struct{}
func (Base) Who() string { return "Base" }

type Child struct{ Base }
func (Child) Who() string { return "Child" }

c := Child{}
fmt.Println(c.Who())
fmt.Println(c.Base.Who())
```

<details>
<summary>Respuesta</summary>

```
Child
Base
```

`c.Who()` llama al metodo de Child (override del promovido). `c.Base.Who()` accede explicitamente al metodo de Base.
</details>

**Ejercicio 8**: ¿Que imprime?

```go
type Hosts []string

func (h Hosts) First() string {
    if len(h) == 0 {
        return ""
    }
    return h[0]
}

servers := Hosts{"web01", "web02"}
fmt.Println(servers.First())
```

<details>
<summary>Respuesta</summary>

```
web01
```

Metodos en tipos slice funcionan igual que en structs. Value receiver porque `First()` solo lee.
</details>

**Ejercicio 9**: ¿Compila y que hace?

```go
type Duration int64

func (d Duration) String() string { return fmt.Sprintf("%dms", d) }

d := Duration(1500)
fmt.Println(d)
```

<details>
<summary>Respuesta</summary>

Compila. Imprime:

```
1500ms
```

`Duration` satisface `fmt.Stringer` con value receiver. `fmt.Println` llama automaticamente `d.String()`.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
type Server struct {
    Name string
}

func (s Server) SetName(name string) {
    s.Name = name
}

srv := Server{Name: "old"}
srv.SetName("new")
fmt.Println(srv.Name)
```

<details>
<summary>Respuesta</summary>

```
old
```

Value receiver: `SetName` modifica una copia. El original no cambia. Este es el bug mas comun con value receivers.
</details>

---

## 20. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              METODOS — RESUMEN                               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SINTAXIS                                                    │
│  func (r T) Method() R { }    ← value receiver              │
│  func (r *T) Method() R { }   ← pointer receiver            │
│  Receptor 1-2 letras: s, c, db (no this/self)                │
│                                                              │
│  VALUE RECEIVER (T)                                          │
│  ├─ Recibe COPIA del valor                                   │
│  ├─ No puede modificar el original                           │
│  ├─ Thread-safe por naturaleza                               │
│  └─ Usar para: tipos pequenos inmutables, solo lectura       │
│                                                              │
│  POINTER RECEIVER (*T)                                       │
│  ├─ Recibe PUNTERO al valor                                  │
│  ├─ Puede modificar el original                              │
│  ├─ Eficiente (no copia)                                     │
│  ├─ Puede ser llamado en nil                                 │
│  └─ Usar para: mutacion, struct grande, sync, consistencia   │
│                                                              │
│  METHOD SETS (interfaz clave)                                │
│  ├─ T  tiene: metodos con receiver T                         │
│  ├─ *T tiene: metodos con receiver T y *T                    │
│  ├─ var _ Interface = T{}   → solo si T methods bastan       │
│  └─ var _ Interface = &T{}  → siempre funciona               │
│                                                              │
│  AUTO-REFERENCIA                                             │
│  ├─ srv.PtrMethod() → Go hace (&srv).PtrMethod()            │
│  ├─ ptr.ValMethod() → Go hace (*ptr).ValMethod()             │
│  └─ NO funciona en: map values, return values, constantes    │
│                                                              │
│  TIPOS CON METODOS                                           │
│  ├─ Structs: el caso mas comun                               │
│  ├─ type Name string: metodos en tipo string                 │
│  ├─ type List []T: metodos en slices                         │
│  ├─ type Headers map[K]V: metodos en maps                    │
│  └─ Restriccion: solo en tipos del mismo paquete             │
│                                                              │
│  EMBEDDING                                                   │
│  ├─ Promueve metodos del tipo embebido                       │
│  ├─ Override: definir metodo con mismo nombre                │
│  ├─ Acceso al embebido: obj.EmbeddedType.Method()            │
│  └─ Ambiguedad si dos embebidos tienen mismo metodo          │
│                                                              │
│  REGLA DE DECISION                                           │
│  ├─ ¿Modifica? → pointer                                    │
│  ├─ ¿Tiene mutex/chan? → pointer (obligatorio)               │
│  ├─ ¿Struct grande? → pointer                               │
│  ├─ ¿Otro metodo es pointer? → pointer (consistencia)       │
│  ├─ ¿Tipo basico inmutable? → value                         │
│  └─ ¿No estas seguro? → pointer (default seguro)            │
│                                                              │
│  GO vs C vs RUST                                             │
│  ├─ Go: T vs *T, method sets, embedding, auto-deref         │
│  ├─ C: funciones + struct*, vtable manual, no methods        │
│  └─ Rust: &self vs &mut self vs self, impl blocks, traits    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: C05/S01/T01 - Arrays — tamaño fijo, parte del tipo ([3]int ≠ [4]int), poco usados directamente
