# Interface Vacia (any) — interface{} / any (Go 1.18+), Type Assertions, Type Switch

## 1. Introduccion

La **interfaz vacia** `interface{}` (alias `any` desde Go 1.18) es una interfaz sin metodos. Como la satisfaccion de interfaces en Go es implicita y se basa en tener todos los metodos requeridos, y una interfaz vacia no requiere **ningun** metodo, **todo tipo la satisface automaticamente**. Es el tipo universal de Go — el equivalente a `void*` en C, `Object` en Java, `object` en Python, o `Box<dyn Any>` en Rust.

Esto la hace poderosa pero peligrosa: pierdes toda la seguridad de tipos del compilador. Cuando usas `any`, el compilador no puede verificar que el valor contenido tenga los metodos o campos que esperas — eso se mueve a **runtime** mediante **type assertions** y **type switches**. Cada type assertion que falla es un panic potencial.

En SysAdmin/DevOps, `any` aparece en: parseo de JSON/YAML arbitrario (configuraciones dinamicas), sistemas de plugins, event buses genericos, almacenamiento de metadatos heterogeneos, wrappers de APIs externas, y configuraciones que mezclan tipos (Terraform, Ansible, Kubernetes). Pero en Go moderno (1.18+), los **genericos** han reemplazado muchos usos de `any` — y eso es bueno.

```
┌─────────────────────────────────────────────────────────────────────┐
│              LA INTERFAZ VACIA — any / interface{}                   │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  type any = interface{}     // alias desde Go 1.18                  │
│                                                                     │
│  Interfaz con 0 metodos → TODO tipo la satisface:                   │
│                                                                     │
│  int           ✓   string        ✓   bool          ✓                │
│  float64       ✓   []byte        ✓   map[K]V       ✓                │
│  *os.File      ✓   chan int      ✓   func()        ✓                │
│  struct{...}   ✓   error         ✓   nil           ✓                │
│  otra interfaz ✓   *MyType       ✓   [3]int        ✓                │
│                                                                     │
│  Problema: si TODO entra, no sabes QUE hay dentro                   │
│  Solucion: type assertions y type switches para "abrir la caja"     │
│                                                                     │
│  ┌────────────────────────────────────────────────────┐              │
│  │  var x any = 42                                    │              │
│  │                                                    │              │
│  │  ┌─────────────────────────┐                       │              │
│  │  │ Interface value (any)   │                       │              │
│  │  ├─────────────────────────┤                       │              │
│  │  │ tab  → type: int        │ ← sabe que es int    │              │
│  │  │ data → value: 42        │ ← almacena el valor  │              │
│  │  └─────────────────────────┘                       │              │
│  │                                                    │              │
│  │  No puedes hacer x + 1 directamente                │              │
│  │  Necesitas: x.(int) + 1  (type assertion)          │              │
│  └────────────────────────────────────────────────────┘              │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 2. interface{} vs any — Historia y Equivalencia

### Antes de Go 1.18 (pre-genericos)

```go
// La unica forma de escribir la interfaz vacia:
func PrintAnything(v interface{}) {
    fmt.Println(v)
}

// Contenedores genericos usaban interface{}:
type Stack struct {
    items []interface{}
}

func (s *Stack) Push(v interface{}) {
    s.items = append(s.items, v)
}

func (s *Stack) Pop() interface{} {
    if len(s.items) == 0 {
        return nil
    }
    v := s.items[len(s.items)-1]
    s.items = s.items[:len(s.items)-1]
    return v
}
```

### Go 1.18+: any como alias

```go
// any es EXACTAMENTE interface{} — un alias de tipo, no un tipo nuevo
// Definido en builtin.go:
// type any = interface{}

// Ahora puedes escribir:
func PrintAnything(v any) {
    fmt.Println(v)
}

// Son 100% intercambiables — mismo tipo subyacente:
var a interface{} = 42
var b any = a        // OK — son el mismo tipo
var c interface{} = b // OK

// Convencion moderna: usar any en codigo nuevo
// Razon: mas legible, mas corto, consistente con constraints de genericos
```

### Cuando usar cual

```
┌──────────────────────────────────────────────────────────────────┐
│  interface{} vs any — convencion                                 │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  any          │ Codigo nuevo (Go 1.18+)                          │
│               │ Constraints de genericos: func F[T any](...)     │
│               │ Variables, parametros, retornos                   │
│                                                                  │
│  interface{}  │ Codigo legacy que no se ha migrado                │
│               │ Mantener consistencia con base existente          │
│               │ APIs publicas que deben soportar Go < 1.18        │
│                                                                  │
│  Preferencia: SIEMPRE any en codigo nuevo                        │
│  gofmt no reescribe automaticamente, pero golangci-lint tiene    │
│  un linter para sugerir el cambio                                │
└──────────────────────────────────────────────────────────────────┘
```

---

## 3. Almacenando Valores en any

Cualquier valor se puede asignar a una variable `any`. El valor se "envuelve" (box) en la interfaz, conservando su tipo dinamico:

```go
package main

import "fmt"

func main() {
    // Cualquier tipo entra en any
    var a any = 42
    var b any = "hello"
    var c any = true
    var d any = 3.14
    var e any = []int{1, 2, 3}
    var f any = map[string]int{"cpu": 85}
    var g any = struct{ Name string }{"web-01"}

    // fmt.Println sabe manejar any (usa reflection internamente)
    fmt.Println(a, b, c, d)
    fmt.Println(e, f, g)

    // Pero NO puedes operar directamente:
    // result := a + 1      // ERROR: mismatched types any and int
    // upper := b.ToUpper() // ERROR: b.ToUpper undefined (any has no methods)
    // first := e[0]        // ERROR: cannot index any

    // Necesitas TYPE ASSERTION para "extraer" el tipo concreto
    n := a.(int)         // extraer int
    s := b.(string)      // extraer string
    fmt.Println(n + 1)   // 43
    fmt.Println(s + "!")  // hello!
}
```

### any nil vs any con valor

```go
func main() {
    var a any           // nil interface — no tiene tipo ni valor
    var b any = nil     // igual — nil interface
    var c any = (*int)(nil) // interface con tipo *int pero valor nil

    fmt.Println(a == nil) // true  — nil interface
    fmt.Println(b == nil) // true  — nil interface
    fmt.Println(c == nil) // false — tiene tipo asignado (*int)

    fmt.Printf("a: type=%T value=%v\n", a, a) // <nil> <nil>
    fmt.Printf("b: type=%T value=%v\n", b, b) // <nil> <nil>
    fmt.Printf("c: type=%T value=%v\n", c, c) // *int  <nil>
}
```

### Slices y maps de any

```go
// Slice heterogeneo — mezcla de tipos
metadata := []any{
    "hostname",             // string
    42,                     // int
    true,                   // bool
    map[string]any{         // map anidado
        "cpu": 85.5,
        "mem": 4096,
    },
    []string{"prod", "us"}, // slice de strings dentro de any
}

for i, v := range metadata {
    fmt.Printf("  [%d] type=%-20T value=%v\n", i, v, v)
}
// [0] type=string               value=hostname
// [1] type=int                  value=42
// [2] type=bool                 value=true
// [3] type=map[string]interface {} value=map[cpu:85.5 mem:4096]
// [4] type=[]string             value=[prod us]
```

---

## 4. Type Assertions — Extraer el Tipo Concreto

Un **type assertion** extrae el valor concreto almacenado en una interfaz. La sintaxis es `x.(T)` donde `x` es una interfaz y `T` es el tipo concreto esperado.

### Forma simple (puede hacer panic)

```go
var x any = "hello"

s := x.(string) // OK — x contiene un string
fmt.Println(s)  // "hello"

n := x.(int)    // PANIC: interface conversion: interface {} is string, not int
```

**Regla: NUNCA uses la forma simple a menos que estes 100% seguro del tipo.** En codigo de produccion, usar la forma comma-ok.

### Forma comma-ok (segura)

```go
var x any = "hello"

// Forma segura — no hace panic
s, ok := x.(string)
fmt.Println(s, ok) // "hello" true

n, ok := x.(int)
fmt.Println(n, ok) // 0 false — n es el zero value de int

// Patron idiomatico:
if s, ok := x.(string); ok {
    fmt.Println("es un string:", s)
} else {
    fmt.Println("no es un string")
}
```

### Type assertion sobre interfaces nombradas

Type assertion no solo extrae tipos concretos — tambien puede verificar si un valor satisface **otra interfaz**:

```go
type Stringer interface {
    String() string
}

type HealthChecker interface {
    Check() error
}

type Server struct {
    Name string
}

func (s Server) String() string   { return s.Name }
func (s Server) Check() error     { return nil }

func Process(v any) {
    // ¿Satisface Stringer?
    if s, ok := v.(Stringer); ok {
        fmt.Println("String():", s.String())
    }

    // ¿Satisface HealthChecker?
    if hc, ok := v.(HealthChecker); ok {
        fmt.Println("Check():", hc.Check())
    }

    // ¿Es exactamente un Server?
    if srv, ok := v.(Server); ok {
        fmt.Println("Server name:", srv.Name)
    }
}

func main() {
    Process(Server{Name: "web-01"})
    // String(): web-01
    // Check(): <nil>
    // Server name: web-01

    Process("just a string") // Solo pasa por los ifs sin match de interfaz
}
```

### Type assertion sobre interfaz no-vacia

Type assertions tambien funcionan sobre cualquier interfaz, no solo `any`:

```go
type Reader interface {
    Read(p []byte) (int, error)
}

type ReadCloser interface {
    Read(p []byte) (int, error)
    Close() error
}

func ProcessReader(r Reader) {
    // ¿Este Reader tambien es un ReadCloser?
    if rc, ok := r.(ReadCloser); ok {
        defer rc.Close()
        fmt.Println("reader tambien soporta Close()")
        _ = rc
    } else {
        fmt.Println("reader basico, sin Close()")
    }
}
```

---

## 5. Type Switch — Manejar Multiples Tipos

Un **type switch** es la forma idiomatica de manejar un valor `any` que puede ser de multiples tipos. Es como un switch normal pero las "cases" son tipos:

### Sintaxis basica

```go
func Describe(v any) string {
    switch val := v.(type) {  // val es el valor YA convertido al tipo
    case int:
        return fmt.Sprintf("entero: %d (hex: 0x%x)", val, val)
    case string:
        return fmt.Sprintf("string: %q (len=%d)", val, len(val))
    case bool:
        if val {
            return "booleano: verdadero"
        }
        return "booleano: falso"
    case float64:
        return fmt.Sprintf("float: %.4f", val)
    case nil:
        return "nil — sin tipo ni valor"
    default:
        return fmt.Sprintf("tipo desconocido: %T = %v", val, val)
    }
}

func main() {
    fmt.Println(Describe(42))          // entero: 42 (hex: 0x2a)
    fmt.Println(Describe("hello"))     // string: "hello" (len=5)
    fmt.Println(Describe(true))        // booleano: verdadero
    fmt.Println(Describe(3.14))        // float: 3.1400
    fmt.Println(Describe(nil))         // nil — sin tipo ni valor
    fmt.Println(Describe([]int{1,2}))  // tipo desconocido: []int = [1 2]
}
```

### Multiples tipos en un case

```go
func IsNumeric(v any) bool {
    switch v.(type) {
    case int, int8, int16, int32, int64,
         uint, uint8, uint16, uint32, uint64,
         float32, float64:
        return true
    default:
        return false
    }
}

// NOTA: cuando un case tiene multiples tipos, val es de tipo any
// (no se puede deducir un tipo unico):
func Example(v any) {
    switch val := v.(type) {
    case int, float64:
        // val es 'any' aqui — NO int ni float64
        // No puedes hacer val + 1
        fmt.Printf("numerico: %v\n", val)
    case string:
        // val es 'string' aqui
        fmt.Println("largo:", len(val))
    }
}
```

### Type switch sobre interfaces

```go
type Shutdowner interface {
    Shutdown() error
}

type Restarter interface {
    Restart() error
}

type Describable interface {
    Describe() string
}

func ManageService(v any) {
    switch svc := v.(type) {
    case Shutdowner:
        fmt.Println("Puede apagarse")
        svc.Shutdown()
    case Restarter:
        fmt.Println("Puede reiniciarse")
        svc.Restart()
    case Describable:
        fmt.Println("Puede describirse:", svc.Describe())
    default:
        fmt.Printf("Tipo sin gestion: %T\n", svc)
    }
}
```

**Importante:** El orden de los cases importa — el primer match gana. Si un tipo satisface Shutdowner Y Restarter, solo entra en el case de Shutdowner.

### Type switch sin variable

```go
// Si solo necesitas el tipo, no el valor convertido:
func TypeName(v any) string {
    switch v.(type) {  // sin "val :="
    case int:
        return "int"
    case string:
        return "string"
    case bool:
        return "bool"
    default:
        return fmt.Sprintf("%T", v)
    }
}
```

---

## 6. Patron: JSON y Configuraciones Dinamicas

El uso mas comun de `any` en SysAdmin es parsear JSON/YAML cuya estructura no conoces de antemano:

### JSON a map[string]any

```go
package main

import (
    "encoding/json"
    "fmt"
)

func main() {
    // JSON de una API de monitoring (estructura variable)
    raw := `{
        "host": "web-01.prod",
        "cpu": 85.5,
        "memory_mb": 4096,
        "healthy": true,
        "tags": ["prod", "us-east"],
        "disks": {
            "/": {"used_pct": 72.3, "total_gb": 100},
            "/data": {"used_pct": 45.1, "total_gb": 500}
        }
    }`

    // Parsear sin struct predefinido
    var data map[string]any
    if err := json.Unmarshal([]byte(raw), &data); err != nil {
        fmt.Println("error:", err)
        return
    }

    // Acceder con type assertions
    // JSON numeros → siempre float64 en Go
    // JSON strings → string
    // JSON booleans → bool
    // JSON arrays → []any
    // JSON objects → map[string]any
    // JSON null → nil

    host := data["host"].(string)
    cpu := data["cpu"].(float64)     // JSON numeros = float64
    mem := data["memory_mb"].(float64) // incluso enteros!
    healthy := data["healthy"].(bool)
    tags := data["tags"].([]any)     // array de any, no []string!
    disks := data["disks"].(map[string]any)

    fmt.Printf("Host: %s\n", host)
    fmt.Printf("CPU: %.1f%%\n", cpu)
    fmt.Printf("Memory: %.0f MB\n", mem)
    fmt.Printf("Healthy: %v\n", healthy)

    // Tags necesita conversion individual
    fmt.Print("Tags: ")
    for _, t := range tags {
        fmt.Printf("%s ", t.(string))
    }
    fmt.Println()

    // Disks: map anidado
    for mount, info := range disks {
        disk := info.(map[string]any)
        fmt.Printf("Disk %s: used=%.1f%% total=%.0fGB\n",
            mount, disk["used_pct"].(float64), disk["total_gb"].(float64))
    }
}
```

### Helper seguro para extraer campos

```go
// Helpers para extraer valores de map[string]any de forma segura

func getString(m map[string]any, key string) (string, bool) {
    v, exists := m[key]
    if !exists {
        return "", false
    }
    s, ok := v.(string)
    return s, ok
}

func getFloat(m map[string]any, key string) (float64, bool) {
    v, exists := m[key]
    if !exists {
        return 0, false
    }
    f, ok := v.(float64)
    return f, ok
}

func getInt(m map[string]any, key string) (int, bool) {
    // JSON no tiene int — solo float64. Convertir:
    f, ok := getFloat(m, key)
    if !ok {
        return 0, false
    }
    return int(f), true
}

func getBool(m map[string]any, key string) (bool, bool) {
    v, exists := m[key]
    if !exists {
        return false, false
    }
    b, ok := v.(bool)
    return b, ok
}

func getMap(m map[string]any, key string) (map[string]any, bool) {
    v, exists := m[key]
    if !exists {
        return nil, false
    }
    sub, ok := v.(map[string]any)
    return sub, ok
}

func getSlice(m map[string]any, key string) ([]any, bool) {
    v, exists := m[key]
    if !exists {
        return nil, false
    }
    s, ok := v.([]any)
    return s, ok
}

// Uso seguro:
func ProcessAlert(data map[string]any) {
    host, ok := getString(data, "host")
    if !ok {
        fmt.Println("missing or invalid host")
        return
    }
    severity, _ := getString(data, "severity")
    cpu, _ := getFloat(data, "cpu")

    fmt.Printf("ALERT: %s severity=%s cpu=%.1f%%\n", host, severity, cpu)
}
```

### JSON numeros: la trampa de float64

```go
// TRAMPA: JSON numeros SIEMPRE son float64 en Go con any
raw := `{"port": 8080, "replicas": 3}`

var data map[string]any
json.Unmarshal([]byte(raw), &data)

// port := data["port"].(int)  // PANIC! es float64, no int

port := data["port"].(float64)   // OK: 8080.0
portInt := int(port)              // 8080

// Alternativa: json.Number para preservar precision
dec := json.NewDecoder(strings.NewReader(raw))
dec.UseNumber()  // numeros como json.Number (string)

var data2 map[string]any
dec.Decode(&data2)

num := data2["port"].(json.Number)
portInt2, _ := num.Int64()   // 8080 como int64
fmt.Println(portInt2)
```

---

## 7. Patron: Funciones que Aceptan any — Antes y Despues de Genericos

### Antes de genericos (pre-1.18): any era la unica opcion

```go
// Pre-genericos: contenedores usaban any
type OldSet struct {
    items map[any]struct{}
}

func NewOldSet() *OldSet {
    return &OldSet{items: make(map[any]struct{})}
}

func (s *OldSet) Add(v any) {
    s.items[v] = struct{}{}
}

func (s *OldSet) Contains(v any) bool {
    _, ok := s.items[v]
    return ok
}

// Problemas:
func main() {
    s := NewOldSet()
    s.Add("hello")
    s.Add(42)       // Mezcla de tipos — el compilador no se queja
    s.Add(true)     // Semanticamente incorrecto, pero compila

    // Al extraer, necesitas type assertion:
    // No sabes que tipo va a salir
    for item := range s.items {
        if str, ok := item.(string); ok {
            fmt.Println("string:", str)
        }
    }
}
```

### Despues de genericos (1.18+): preferir genericos

```go
// Post-genericos: type-safe en compilacion
type Set[T comparable] struct {
    items map[T]struct{}
}

func NewSet[T comparable]() *Set[T] {
    return &Set[T]{items: make(map[T]struct{})}
}

func (s *Set[T]) Add(v T) {
    s.items[v] = struct{}{}
}

func (s *Set[T]) Contains(v T) bool {
    _, ok := s.items[v]
    return ok
}

func main() {
    s := NewSet[string]()
    s.Add("hello")
    s.Add("world")
    // s.Add(42)     // ERROR de compilacion: int no es string
    // s.Add(true)   // ERROR de compilacion: bool no es string

    fmt.Println(s.Contains("hello")) // true — sin type assertion
}
```

### Cuando TODAVIA necesitas any (Go 1.18+)

```
┌──────────────────────────────────────────────────────────────────────┐
│  CUANDO USAR any vs GENERICOS (Go 1.18+)                            │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  USAR GENERICOS cuando:                                              │
│  ├─ Conoces el conjunto de tipos en compilacion                      │
│  ├─ Todos los elementos son del mismo tipo                           │
│  ├─ Quieres seguridad de tipos sin type assertions                   │
│  └─ Contenedores, algoritmos, utilidades                             │
│                                                                      │
│  USAR any cuando:                                                    │
│  ├─ JSON/YAML dinamico cuya estructura no conoces                    │
│  ├─ Datos verdaderamente heterogeneos (mezcla de tipos)              │
│  ├─ Reflection (reflect.ValueOf necesita any)                        │
│  ├─ Interoperabilidad con APIs legacy que usan interface{}           │
│  ├─ Plugin systems donde los tipos se descubren en runtime           │
│  └─ fmt.Println, log.Printf — funciones variadic universales        │
│                                                                      │
│  REGLA: si puedes usar genericos, usalos. any es ultimo recurso.     │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 8. Type Switch Avanzado: Patrones de Uso Real

### Patron 1: Event handler con type switch

```go
// Sistema de eventos de infraestructura
type Event struct {
    Timestamp time.Time
    Source    string
    Payload  any  // el tipo depende del evento
}

// Diferentes tipos de payload
type ServerCreated struct {
    Hostname string
    IP       string
    Spec     string
}

type ServerDeleted struct {
    Hostname string
    Reason   string
}

type AlertFired struct {
    Severity string
    Metric   string
    Value    float64
    Threshold float64
}

type DeployCompleted struct {
    Service  string
    Version  string
    Replicas int
    Duration time.Duration
}

func HandleEvent(e Event) {
    fmt.Printf("[%s] from %s: ", e.Timestamp.Format("15:04:05"), e.Source)

    switch p := e.Payload.(type) {
    case ServerCreated:
        fmt.Printf("SERVER CREATED %s (%s) spec=%s\n",
            p.Hostname, p.IP, p.Spec)

    case ServerDeleted:
        fmt.Printf("SERVER DELETED %s reason=%s\n",
            p.Hostname, p.Reason)

    case AlertFired:
        fmt.Printf("ALERT [%s] %s=%.2f (threshold=%.2f)\n",
            p.Severity, p.Metric, p.Value, p.Threshold)

    case DeployCompleted:
        fmt.Printf("DEPLOY %s v%s ×%d in %s\n",
            p.Service, p.Version, p.Replicas, p.Duration)

    case string:
        fmt.Printf("MESSAGE: %s\n", p)

    default:
        fmt.Printf("UNKNOWN EVENT TYPE: %T\n", p)
    }
}
```

### Patron 2: Recursive type walking (JSON-like)

```go
// Recorrer un arbol de map[string]any recursivamente
// Util para inspeccionar configuraciones JSON/YAML anidadas

func WalkConfig(prefix string, v any) {
    switch val := v.(type) {
    case map[string]any:
        for key, child := range val {
            path := key
            if prefix != "" {
                path = prefix + "." + key
            }
            WalkConfig(path, child)
        }
    case []any:
        for i, child := range val {
            path := fmt.Sprintf("%s[%d]", prefix, i)
            WalkConfig(path, child)
        }
    case string:
        fmt.Printf("  %-40s = %q\n", prefix, val)
    case float64:
        // Mostrar como int si no tiene decimales
        if val == float64(int64(val)) {
            fmt.Printf("  %-40s = %.0f\n", prefix, val)
        } else {
            fmt.Printf("  %-40s = %g\n", prefix, val)
        }
    case bool:
        fmt.Printf("  %-40s = %v\n", prefix, val)
    case nil:
        fmt.Printf("  %-40s = null\n", prefix)
    default:
        fmt.Printf("  %-40s = (%T) %v\n", prefix, val, val)
    }
}

func main() {
    configJSON := `{
        "server": {
            "host": "0.0.0.0",
            "port": 8080,
            "tls": {
                "enabled": true,
                "cert": "/etc/ssl/cert.pem"
            }
        },
        "database": {
            "host": "db.internal",
            "port": 5432,
            "pool_size": 25,
            "ssl_mode": "verify-full"
        },
        "features": ["auth", "metrics", "tracing"],
        "debug": false,
        "max_retries": 3
    }`

    var config map[string]any
    json.Unmarshal([]byte(configJSON), &config)

    fmt.Println("Configuration tree:")
    WalkConfig("", config)
}
// Output:
//   debug                                    = false
//   features[0]                              = "auth"
//   features[1]                              = "metrics"
//   features[2]                              = "tracing"
//   max_retries                              = 3
//   server.host                              = "0.0.0.0"
//   server.port                              = 8080
//   server.tls.cert                          = "/etc/ssl/cert.pem"
//   server.tls.enabled                       = true
//   database.host                            = "db.internal"
//   database.pool_size                       = 25
//   database.port                            = 5432
//   database.ssl_mode                        = "verify-full"
```

### Patron 3: Conversion flexible de tipos

```go
// Convertir any a string de forma robusta
// Util para templates, logging, CLI output

func ToString(v any) string {
    switch val := v.(type) {
    case string:
        return val
    case int:
        return strconv.Itoa(val)
    case int64:
        return strconv.FormatInt(val, 10)
    case float64:
        if val == float64(int64(val)) {
            return strconv.FormatInt(int64(val), 10)
        }
        return strconv.FormatFloat(val, 'f', -1, 64)
    case bool:
        if val {
            return "true"
        }
        return "false"
    case nil:
        return "<nil>"
    case fmt.Stringer:
        return val.String()
    case error:
        return val.Error()
    default:
        return fmt.Sprintf("%v", val)
    }
}
```

---

## 9. any en Funciones Variadic — fmt.Println y Similares

La razon por la que `fmt.Println` acepta cualquier cosa es que usa `...any`:

```go
// Firma real de fmt.Println:
// func Println(a ...any) (n int, err error)

// ...any = variadic de any = acepta 0 o mas valores de CUALQUIER tipo
fmt.Println("host:", hostname, "port:", 8080, "healthy:", true)
// Cada argumento se envuelve en any automaticamente

// Tu propia funcion variadic con any:
func LogFields(msg string, fields ...any) {
    if len(fields) % 2 != 0 {
        fmt.Println("[WARN] odd number of fields, last will be ignored")
    }
    fmt.Print(msg)
    for i := 0; i+1 < len(fields); i += 2 {
        fmt.Printf(" %v=%v", fields[i], fields[i+1])
    }
    fmt.Println()
}

func main() {
    LogFields("server started",
        "host", "web-01",
        "port", 8080,
        "pid", 12345,
        "debug", false,
    )
    // server started host=web-01 port=8080 pid=12345 debug=false
}
```

### El patron "structured logging" con any

```go
// Muchas librerias de logging usan este patron:
// slog (Go 1.21+) usa any para valores:

// import "log/slog"
// slog.Info("request handled",
//     "method", "GET",
//     "path", "/api/health",
//     "status", 200,
//     "duration_ms", 12.5,
// )

// Internamente slog usa any para los valores y type switches
// para serializar cada tipo correctamente
```

---

## 10. any como Campo de Struct — Metadatos Heterogeneos

```go
// Labels/annotations con valores de tipo mixto
type Resource struct {
    Name        string
    Kind        string
    Annotations map[string]any  // valores heterogeneos
}

func NewResource(name, kind string) *Resource {
    return &Resource{
        Name:        name,
        Kind:        kind,
        Annotations: make(map[string]any),
    }
}

func (r *Resource) SetAnnotation(key string, value any) {
    r.Annotations[key] = value
}

func (r *Resource) GetString(key string) (string, bool) {
    v, ok := r.Annotations[key]
    if !ok {
        return "", false
    }
    s, ok := v.(string)
    return s, ok
}

func (r *Resource) GetInt(key string) (int, bool) {
    v, ok := r.Annotations[key]
    if !ok {
        return 0, false
    }
    switch n := v.(type) {
    case int:
        return n, true
    case float64:
        return int(n), true
    default:
        return 0, false
    }
}

func main() {
    pod := NewResource("web-01", "Pod")
    pod.SetAnnotation("replicas", 3)
    pod.SetAnnotation("image", "nginx:1.25")
    pod.SetAnnotation("auto-scale", true)
    pod.SetAnnotation("limits", map[string]any{
        "cpu":    "500m",
        "memory": "256Mi",
    })

    if img, ok := pod.GetString("image"); ok {
        fmt.Println("Image:", img) // nginx:1.25
    }
    if reps, ok := pod.GetInt("replicas"); ok {
        fmt.Println("Replicas:", reps) // 3
    }
}
```

---

## 11. Comparacion de any con C y Rust

```
┌─────────────────────────────────────────────────────────────────────────┐
│  TIPO UNIVERSAL — Go vs C vs Rust                                       │
├──────────────┬──────────────────┬──────────────────┬────────────────────┤
│ Aspecto      │ Go (any)         │ C (void*)        │ Rust (Box<dyn Any>)│
├──────────────┼──────────────────┼──────────────────┼────────────────────┤
│ Sintaxis     │ any / interface{}│ void*            │ Box<dyn Any>       │
│              │                  │                  │ o enum + variants  │
├──────────────┼──────────────────┼──────────────────┼────────────────────┤
│ Info de tipo │ SI (runtime,     │ NO — se pierde   │ SI (TypeId, pero   │
│              │ reflection)      │ completamente    │ limitado a 'static)│
├──────────────┼──────────────────┼──────────────────┼────────────────────┤
│ Extraer tipo │ type assertion   │ cast manual      │ downcast()         │
│              │ x.(int)          │ *(int*)ptr       │ x.downcast::<i32>()│
│              │ safe con comma-ok│ completamente    │ retorna Option<>   │
│              │                  │ unsafe           │                    │
├──────────────┼──────────────────┼──────────────────┼────────────────────┤
│ Type switch  │ switch v.(type)  │ No existe        │ No existe nativo   │
│              │ (built-in)       │ (if-else con     │ (match con enum o  │
│              │                  │  tags manuales)  │  Any::type_id())   │
├──────────────┼──────────────────┼──────────────────┼────────────────────┤
│ Safety       │ Panic si type    │ UB (undefined    │ Retorna None si    │
│ en error     │ assertion falla  │ behavior)        │ tipo incorrecto    │
│              │ (con comma-ok:   │                  │ (sin panic)        │
│              │  zero + false)   │                  │                    │
├──────────────┼──────────────────┼──────────────────┼────────────────────┤
│ Alternativa  │ Genericos (1.18+)│ _Generic (C11),  │ Generics (default) │
│ preferida    │                  │ macros           │ + enum para ADTs   │
├──────────────┼──────────────────┼──────────────────┼────────────────────┤
│ JSON         │ map[string]any   │ Manual parsing   │ serde_json::Value  │
│ dinamico     │ (nativo)         │ con cJSON/jansson│ (similar a Go)     │
├──────────────┼──────────────────┼──────────────────┼────────────────────┤
│ Uso comun    │ JSON, logging,   │ Callbacks,       │ Raro — enum/trait  │
│              │ event systems    │ generic containers│ cubren la mayoria │
└──────────────┴──────────────────┴──────────────────┴────────────────────┘
```

### Ejemplo comparativo: contenedor heterogeneo

**C — void* (unsafe):**
```c
// C: contenedor con void* — sin seguridad de tipos
typedef enum { TYPE_INT, TYPE_STRING, TYPE_FLOAT } ValueType;

typedef struct {
    ValueType type;  // tag manual
    void*     data;
} AnyValue;

AnyValue make_int(int v) {
    int* p = malloc(sizeof(int));
    *p = v;
    return (AnyValue){TYPE_INT, p};
}

int get_int(AnyValue v) {
    if (v.type != TYPE_INT) {
        fprintf(stderr, "type mismatch!\n");
        abort();
    }
    return *(int*)v.data;  // cast manual, UB si el tipo es incorrecto
}
// Problemas: memory leaks, type tags manuales, casts unsafe
```

**Rust — enum (type-safe, preferido):**
```rust
// Rust: NO usa dyn Any normalmente — usa enum para tipos conocidos
enum ConfigValue {
    Int(i64),
    Str(String),
    Float(f64),
    Bool(bool),
    List(Vec<ConfigValue>),
}

// Match exhaustivo — el compilador verifica que cubres todos los casos
fn describe(v: &ConfigValue) -> String {
    match v {
        ConfigValue::Int(n) => format!("int: {}", n),
        ConfigValue::Str(s) => format!("str: {}", s),
        ConfigValue::Float(f) => format!("float: {:.2}", f),
        ConfigValue::Bool(b) => format!("bool: {}", b),
        ConfigValue::List(items) => format!("list of {} items", items.len()),
        // Si agregas un variant nuevo, el compilador te obliga a manejarlo
    }
}
```

**Go — any + type switch:**
```go
// Go: any con type switch — safe pero no exhaustivo
func Describe(v any) string {
    switch val := v.(type) {
    case int:
        return fmt.Sprintf("int: %d", val)
    case string:
        return fmt.Sprintf("str: %s", val)
    case float64:
        return fmt.Sprintf("float: %.2f", val)
    case bool:
        return fmt.Sprintf("bool: %v", val)
    case []any:
        return fmt.Sprintf("list of %d items", len(val))
    default:
        // El compilador NO te avisa si olvidas un tipo
        return fmt.Sprintf("unknown: %T", val)
    }
}
```

---

## 12. Anti-Patrones y Trampas

### Anti-patron 1: Usar any cuando conoces el tipo

```go
// ✗ MALO — ¿por que any si siempre es string?
func ProcessName(name any) {
    n := name.(string) // si alguien pasa un int → panic
    fmt.Println(n)
}

// ✓ BIEN — tipo explicito
func ProcessName(name string) {
    fmt.Println(name)
}
```

### Anti-patron 2: any en lugar de interfaz especifica

```go
// ✗ MALO — any pierde toda informacion de tipo
func LogMessage(logger any) {
    // Necesitas saber que tiene metodo Log
    if l, ok := logger.(interface{ Log(string) }); ok {
        l.Log("hello")
    }
}

// ✓ BIEN — interfaz con el metodo que necesitas
type Logger interface {
    Log(msg string)
}

func LogMessage(logger Logger) {
    logger.Log("hello")
}
```

### Anti-patron 3: map[string]any como "struct dinamico"

```go
// ✗ MALO — usar map como struct cuando la estructura es conocida
func CreateServer(config map[string]any) error {
    name := config["name"].(string)     // puede panic
    port := config["port"].(float64)    // ugh, JSON float64
    // typo en la key? Zero value silencioso
    debug := config["dubug"].(bool)     // panic! typo en "debug"
    _ = name; _ = port; _ = debug
    return nil
}

// ✓ BIEN — struct con tipos explicitos
type ServerConfig struct {
    Name  string `json:"name"`
    Port  int    `json:"port"`
    Debug bool   `json:"debug"`
}

func CreateServer(config ServerConfig) error {
    // Acceso directo, type-safe, autocompletado del IDE
    fmt.Println(config.Name, config.Port, config.Debug)
    return nil
}
```

### Trampa: Comparacion de any

```go
// any usa la comparacion del tipo dinamico
var a any = 42
var b any = 42
fmt.Println(a == b) // true — ambos son int(42)

var c any = 42
var d any = "42"
fmt.Println(c == d) // false — tipos diferentes (int vs string)

// PANIC si el tipo dinamico no es comparable:
var e any = []int{1, 2, 3}
var f any = []int{1, 2, 3}
// fmt.Println(e == f) // PANIC: runtime error: comparing uncomparable type []int
_ = e; _ = f
```

### Trampa: Nil en any

```go
// nil se comporta diferente segun como llega a any
var x any = nil           // nil interface
var y any = (*int)(nil)   // interface con tipo *int, valor nil

fmt.Println(x == nil) // true
fmt.Println(y == nil) // false — tiene tipo

fmt.Println(x == y)   // false — x es nil interface, y tiene tipo
```

---

## 13. any en la Stdlib — Usos Reales

### fmt: toda la familia Print

```go
// Firmas reales:
// func Print(a ...any) (n int, err error)
// func Println(a ...any) (n int, err error)
// func Printf(format string, a ...any) (n int, err error)
// func Sprintf(format string, a ...any) string
// func Fprintf(w io.Writer, format string, a ...any) (n int, err error)

// Internamente, fmt usa reflection + type switches para formatear
```

### encoding/json: Marshal y Unmarshal

```go
// func Marshal(v any) ([]byte, error)
// func Unmarshal(data []byte, v any) error

// v es any porque puede ser struct, map, slice, pointer...
data, _ := json.Marshal(Server{Name: "web-01"})
json.Unmarshal(data, &server)
```

### sync.Map: almacenamiento concurrente

```go
// sync.Map usa any para keys y values:
// func (m *Map) Store(key, value any)
// func (m *Map) Load(key any) (value any, ok bool)

var cache sync.Map
cache.Store("web-01", Server{Name: "web-01"})

if v, ok := cache.Load("web-01"); ok {
    srv := v.(Server)  // type assertion necesaria
    fmt.Println(srv.Name)
}
```

### context.Value: metadatos de contexto

```go
// func WithValue(parent Context, key, val any) Context
// func (ctx Context) Value(key any) any

type ctxKey string

const requestIDKey ctxKey = "request-id"

ctx := context.WithValue(context.Background(), requestIDKey, "req-abc123")

if id, ok := ctx.Value(requestIDKey).(string); ok {
    fmt.Println("Request ID:", id)
}
```

### reflect: toda la API de reflection

```go
// reflect.ValueOf acepta any:
// func ValueOf(i any) Value
// func TypeOf(i any) Type

v := reflect.ValueOf(42)
fmt.Println(v.Kind())  // int
fmt.Println(v.Int())   // 42
```

---

## 14. Ejercicios de Prediccion

### Ejercicio 1: ¿Que imprime?

```go
func identify(v any) {
    switch v.(type) {
    case int:
        fmt.Println("A")
    case float64:
        fmt.Println("B")
    case string:
        fmt.Println("C")
    case bool:
        fmt.Println("D")
    default:
        fmt.Println("E")
    }
}

func main() {
    identify(42)
    identify(42.0)
    identify("42")
    identify(true)
    identify(nil)
    identify([]int{42})
}
```

<details>
<summary>Respuesta</summary>

```
A
B
C
D
E
E
```

- `42` es un literal int (sin tipo explicito, Go infiere int) → case int → A
- `42.0` es un literal float64 → B
- `"42"` es string → C
- `true` es bool → D
- `nil` no matchea ningun tipo especifico → default → E
- `[]int{42}` es un slice, no esta en ningun case → default → E

</details>

### Ejercicio 2: ¿Panic o no?

```go
func main() {
    var x any = "hello"

    s, ok := x.(int)
    fmt.Println(s, ok)     // linea A

    n := x.(int)           // linea B
    fmt.Println(n)
}
```

<details>
<summary>Respuesta</summary>

La linea A imprime `0 false` — la forma comma-ok no hace panic, retorna el zero value de int y false.

La linea B **PANIC**: `interface conversion: interface {} is string, not int`. La forma simple sin comma-ok hace panic si el tipo no coincide.

El programa imprime una linea y luego panic.

</details>

### Ejercicio 3: ¿Que imprime?

```go
func main() {
    var a any = 42
    var b any = 42
    var c any = "42"

    fmt.Println(a == b)
    fmt.Println(a == c)
    fmt.Println(a == 42)
    fmt.Println(c == "42")
}
```

<details>
<summary>Respuesta</summary>

```
true
false
true
true
```

- `a == b`: ambos son `int(42)` → true
- `a == c`: tipo diferente (`int` vs `string`) → false (no panic, los tipos son comparables)
- `a == 42`: `42` se envuelve implicitamente en any como `int(42)` → true
- `c == "42"`: `"42"` se envuelve como `string("42")` → true

</details>

### Ejercicio 4: ¿Que tipo tiene val?

```go
func check(v any) {
    switch val := v.(type) {
    case int, float64:
        // ¿Que tipo es val aqui?
        fmt.Printf("type=%T value=%v\n", val, val)
    }
}

func main() {
    check(42)
    check(3.14)
}
```

<details>
<summary>Respuesta</summary>

```
type=int value=42
type=float64 value=3.14
```

Cuando un case tiene **multiples tipos** (`int, float64`), la variable `val` es de tipo `any` (no `int` ni `float64`). Sin embargo, `%T` muestra el tipo dinamico real. Lo que NO puedes hacer es `val + 1` porque el compilador ve `val` como `any`, no como un tipo numerico. Para operar necesitarias un type assertion adicional dentro del case.

</details>

### Ejercicio 5: ¿Compila?

```go
func main() {
    var x any = []int{1, 2, 3}
    var y any = []int{1, 2, 3}

    if x == y {
        fmt.Println("equal")
    }
}
```

<details>
<summary>Respuesta</summary>

**Compila** pero hace **PANIC en runtime**: `comparing uncomparable type []int`. El compilador permite `==` entre valores `any` (es una operacion valida sobre interfaces), pero si el tipo dinamico no es comparable (slices, maps, funcs), la comparacion hace panic en runtime. Esto es una de las pocas situaciones donde Go tiene errores de tipo en runtime.

</details>

### Ejercicio 6: ¿nil o no nil?

```go
func MaybeError(fail bool) any {
    if fail {
        var err *os.PathError
        return err  // nil *os.PathError
    }
    return nil
}

func main() {
    result := MaybeError(true)
    fmt.Println(result == nil)

    result2 := MaybeError(false)
    fmt.Println(result2 == nil)
}
```

<details>
<summary>Respuesta</summary>

```
false
true
```

- `MaybeError(true)` retorna `*os.PathError(nil)` envuelto en `any` → la interfaz tiene tipo `*os.PathError` pero valor nil → `== nil` es **false**
- `MaybeError(false)` retorna `nil` directamente → nil interface → `== nil` es **true**

Esta es la trampa clasica de Go. La forma correcta:
```go
func MaybeError(fail bool) any {
    if fail {
        return nil  // retornar nil DIRECTAMENTE, no un puntero nil
    }
    return nil
}
```

</details>

---

## 15. Programa Completo: Dynamic Configuration Engine con any

```go
// dynamic_config.go
// Motor de configuracion dinamica que demuestra el uso real de any.
// Parsea JSON/YAML-like configs, soporta valores heterogeneos,
// validacion con type switches, merge de configuraciones,
// y resolucion de variables de entorno.
package main

import (
    "encoding/json"
    "fmt"
    "os"
    "sort"
    "strconv"
    "strings"
)

// =====================================================================
// CONFIG VALUE — wrapper tipado sobre any
// =====================================================================

// ConfigValue envuelve any con operaciones seguras
type ConfigValue struct {
    raw any
}

func NewValue(v any) ConfigValue {
    return ConfigValue{raw: v}
}

func (cv ConfigValue) Raw() any { return cv.raw }

func (cv ConfigValue) IsNil() bool { return cv.raw == nil }

func (cv ConfigValue) String() (string, bool) {
    s, ok := cv.raw.(string)
    return s, ok
}

func (cv ConfigValue) Int() (int, bool) {
    switch v := cv.raw.(type) {
    case int:
        return v, true
    case float64:
        return int(v), true
    case string:
        n, err := strconv.Atoi(v)
        return n, err == nil
    default:
        return 0, false
    }
}

func (cv ConfigValue) Float() (float64, bool) {
    switch v := cv.raw.(type) {
    case float64:
        return v, true
    case int:
        return float64(v), true
    case string:
        f, err := strconv.ParseFloat(v, 64)
        return f, err == nil
    default:
        return 0, false
    }
}

func (cv ConfigValue) Bool() (bool, bool) {
    switch v := cv.raw.(type) {
    case bool:
        return v, true
    case string:
        b, err := strconv.ParseBool(v)
        return b, err == nil
    case int:
        return v != 0, true
    case float64:
        return v != 0, true
    default:
        return false, false
    }
}

func (cv ConfigValue) StringSlice() ([]string, bool) {
    arr, ok := cv.raw.([]any)
    if !ok {
        return nil, false
    }
    result := make([]string, 0, len(arr))
    for _, item := range arr {
        if s, ok := item.(string); ok {
            result = append(result, s)
        } else {
            return nil, false // no todos son strings
        }
    }
    return result, true
}

func (cv ConfigValue) Map() (map[string]any, bool) {
    m, ok := cv.raw.(map[string]any)
    return m, ok
}

// TypeName retorna el tipo Go del valor contenido
func (cv ConfigValue) TypeName() string {
    if cv.raw == nil {
        return "nil"
    }
    return fmt.Sprintf("%T", cv.raw)
}

// Display formatea el valor para mostrar
func (cv ConfigValue) Display() string {
    switch v := cv.raw.(type) {
    case string:
        return fmt.Sprintf("%q", v)
    case float64:
        if v == float64(int64(v)) {
            return fmt.Sprintf("%.0f", v)
        }
        return fmt.Sprintf("%g", v)
    case bool:
        return strconv.FormatBool(v)
    case nil:
        return "null"
    case []any:
        parts := make([]string, len(v))
        for i, item := range v {
            parts[i] = NewValue(item).Display()
        }
        return "[" + strings.Join(parts, ", ") + "]"
    case map[string]any:
        return fmt.Sprintf("{...%d keys}", len(v))
    default:
        return fmt.Sprintf("%v", v)
    }
}

// =====================================================================
// CONFIG STORE — almacenamiento jerárquico con paths
// =====================================================================

type ConfigStore struct {
    data map[string]any
    name string
}

func NewConfigStore(name string) *ConfigStore {
    return &ConfigStore{
        data: make(map[string]any),
        name: name,
    }
}

// LoadJSON carga configuracion desde JSON
func (cs *ConfigStore) LoadJSON(jsonData string) error {
    return json.Unmarshal([]byte(jsonData), &cs.data)
}

// Get obtiene un valor por path (e.g., "server.port")
func (cs *ConfigStore) Get(path string) ConfigValue {
    parts := strings.Split(path, ".")
    var current any = cs.data

    for _, part := range parts {
        m, ok := current.(map[string]any)
        if !ok {
            return NewValue(nil)
        }
        current, ok = m[part]
        if !ok {
            return NewValue(nil)
        }
    }
    return NewValue(current)
}

// Set establece un valor por path, creando maps intermedios si es necesario
func (cs *ConfigStore) Set(path string, value any) {
    parts := strings.Split(path, ".")
    current := cs.data

    // Navegar/crear maps intermedios
    for _, part := range parts[:len(parts)-1] {
        if next, ok := current[part]; ok {
            if nextMap, ok := next.(map[string]any); ok {
                current = nextMap
            } else {
                // Sobrescribir valor no-map con un map
                newMap := make(map[string]any)
                current[part] = newMap
                current = newMap
            }
        } else {
            newMap := make(map[string]any)
            current[part] = newMap
            current = newMap
        }
    }

    current[parts[len(parts)-1]] = value
}

// Keys retorna todas las keys en un nivel
func (cs *ConfigStore) Keys(path string) []string {
    var m map[string]any
    if path == "" {
        m = cs.data
    } else {
        v := cs.Get(path)
        var ok bool
        m, ok = v.Map()
        if !ok {
            return nil
        }
    }

    keys := make([]string, 0, len(m))
    for k := range m {
        keys = append(keys, k)
    }
    sort.Strings(keys)
    return keys
}

// Flatten retorna todos los paths con sus valores
func (cs *ConfigStore) Flatten() map[string]ConfigValue {
    result := make(map[string]ConfigValue)
    cs.flattenRecursive("", cs.data, result)
    return result
}

func (cs *ConfigStore) flattenRecursive(prefix string, data map[string]any, result map[string]ConfigValue) {
    for key, value := range data {
        path := key
        if prefix != "" {
            path = prefix + "." + key
        }

        if subMap, ok := value.(map[string]any); ok {
            cs.flattenRecursive(path, subMap, result)
        } else {
            result[path] = NewValue(value)
        }
    }
}

// =====================================================================
// CONFIG VALIDATOR — validacion con type switches
// =====================================================================

type ValidationRule struct {
    Path     string
    Required bool
    Type     string // "string", "int", "float", "bool", "array", "object"
    Min      *float64
    Max      *float64
    OneOf    []any
}

type ValidationError struct {
    Path    string
    Message string
}

func (e ValidationError) Error() string {
    return fmt.Sprintf("%s: %s", e.Path, e.Message)
}

func ValidateConfig(store *ConfigStore, rules []ValidationRule) []ValidationError {
    var errors []ValidationError

    for _, rule := range rules {
        cv := store.Get(rule.Path)

        // Check required
        if rule.Required && cv.IsNil() {
            errors = append(errors, ValidationError{
                Path:    rule.Path,
                Message: "required field is missing",
            })
            continue
        }

        if cv.IsNil() {
            continue // optional and missing — OK
        }

        // Check type
        if rule.Type != "" {
            valid := false
            switch rule.Type {
            case "string":
                _, valid = cv.String()
            case "int":
                _, valid = cv.Int()
            case "float":
                _, valid = cv.Float()
            case "bool":
                _, valid = cv.Bool()
            case "array":
                _, valid = cv.raw.([]any)
            case "object":
                _, valid = cv.Map()
            }
            if !valid {
                errors = append(errors, ValidationError{
                    Path:    rule.Path,
                    Message: fmt.Sprintf("expected type %s, got %s", rule.Type, cv.TypeName()),
                })
                continue
            }
        }

        // Check min/max (numeros)
        if rule.Min != nil || rule.Max != nil {
            f, ok := cv.Float()
            if ok {
                if rule.Min != nil && f < *rule.Min {
                    errors = append(errors, ValidationError{
                        Path:    rule.Path,
                        Message: fmt.Sprintf("value %.0f is below minimum %.0f", f, *rule.Min),
                    })
                }
                if rule.Max != nil && f > *rule.Max {
                    errors = append(errors, ValidationError{
                        Path:    rule.Path,
                        Message: fmt.Sprintf("value %.0f exceeds maximum %.0f", f, *rule.Max),
                    })
                }
            }
        }

        // Check oneOf
        if len(rule.OneOf) > 0 {
            found := false
            for _, allowed := range rule.OneOf {
                if cv.raw == allowed {
                    found = true
                    break
                }
            }
            if !found {
                errors = append(errors, ValidationError{
                    Path:    rule.Path,
                    Message: fmt.Sprintf("value %v not in allowed set %v", cv.raw, rule.OneOf),
                })
            }
        }
    }

    return errors
}

// =====================================================================
// CONFIG MERGER — merge de multiples configuraciones
// =====================================================================

// MergeConfigs combina multiples configs con prioridad (ultimo gana)
func MergeConfigs(stores ...*ConfigStore) *ConfigStore {
    result := NewConfigStore("merged")

    for _, store := range stores {
        mergeMap(result.data, store.data)
    }

    return result
}

func mergeMap(dst, src map[string]any) {
    for key, srcVal := range src {
        dstVal, exists := dst[key]

        // Si ambos son maps, merge recursivo
        if exists {
            dstMap, dstIsMap := dstVal.(map[string]any)
            srcMap, srcIsMap := srcVal.(map[string]any)
            if dstIsMap && srcIsMap {
                mergeMap(dstMap, srcMap)
                continue
            }
        }

        // Caso base: src sobrescribe dst
        // Deep copy de maps para evitar aliasing
        switch v := srcVal.(type) {
        case map[string]any:
            newMap := make(map[string]any)
            mergeMap(newMap, v)
            dst[key] = newMap
        default:
            dst[key] = srcVal
        }
    }
}

// =====================================================================
// ENV RESOLVER — sustituir ${ENV_VAR} en valores string
// =====================================================================

func ResolveEnvVars(store *ConfigStore) int {
    resolved := 0
    resolveEnvRecursive(store.data, &resolved)
    return resolved
}

func resolveEnvRecursive(data map[string]any, count *int) {
    for key, value := range data {
        switch v := value.(type) {
        case string:
            if strings.Contains(v, "${") {
                resolved := resolveEnvString(v)
                if resolved != v {
                    data[key] = resolved
                    *count++
                }
            }
        case map[string]any:
            resolveEnvRecursive(v, count)
        case []any:
            for i, item := range v {
                if s, ok := item.(string); ok && strings.Contains(s, "${") {
                    resolved := resolveEnvString(s)
                    if resolved != s {
                        v[i] = resolved
                        *count++
                    }
                }
            }
        }
    }
}

func resolveEnvString(s string) string {
    result := s
    for {
        start := strings.Index(result, "${")
        if start == -1 {
            break
        }
        end := strings.Index(result[start:], "}")
        if end == -1 {
            break
        }
        end += start

        varExpr := result[start+2 : end]
        var envName, defaultVal string

        // Soportar ${VAR:-default}
        if idx := strings.Index(varExpr, ":-"); idx != -1 {
            envName = varExpr[:idx]
            defaultVal = varExpr[idx+2:]
        } else {
            envName = varExpr
        }

        envVal := os.Getenv(envName)
        if envVal == "" {
            envVal = defaultVal
        }

        result = result[:start] + envVal + result[end+1:]
    }
    return result
}

// =====================================================================
// DIFF ENGINE — comparar configuraciones
// =====================================================================

type ConfigDiff struct {
    Path    string
    Type    string // "added", "removed", "changed"
    OldVal  ConfigValue
    NewVal  ConfigValue
}

func DiffConfigs(old, new *ConfigStore) []ConfigDiff {
    var diffs []ConfigDiff

    oldFlat := old.Flatten()
    newFlat := new.Flatten()

    // Check changed and removed
    for path, oldVal := range oldFlat {
        if newVal, exists := newFlat[path]; exists {
            if oldVal.Display() != newVal.Display() {
                diffs = append(diffs, ConfigDiff{
                    Path:   path,
                    Type:   "changed",
                    OldVal: oldVal,
                    NewVal: newVal,
                })
            }
        } else {
            diffs = append(diffs, ConfigDiff{
                Path:   path,
                Type:   "removed",
                OldVal: oldVal,
            })
        }
    }

    // Check added
    for path, newVal := range newFlat {
        if _, exists := oldFlat[path]; !exists {
            diffs = append(diffs, ConfigDiff{
                Path:   path,
                Type:   "added",
                NewVal: newVal,
            })
        }
    }

    // Sort by path for deterministic output
    sort.Slice(diffs, func(i, j int) bool {
        return diffs[i].Path < diffs[j].Path
    })

    return diffs
}

// =====================================================================
// MAIN
// =====================================================================

func main() {
    fmt.Println("╔══════════════════════════════════════════════════════╗")
    fmt.Println("║   Dynamic Configuration Engine — any/type switch    ║")
    fmt.Println("╚══════════════════════════════════════════════════════╝")

    // --- 1. Base config (defaults) ---

    fmt.Println("\n── 1. Loading Base Configuration ──")

    baseConfig := NewConfigStore("base")
    err := baseConfig.LoadJSON(`{
        "server": {
            "host": "0.0.0.0",
            "port": 8080,
            "read_timeout_ms": 30000,
            "write_timeout_ms": 30000,
            "tls": {
                "enabled": false,
                "cert_path": "",
                "key_path": ""
            }
        },
        "database": {
            "host": "localhost",
            "port": 5432,
            "name": "myapp",
            "pool_size": 10,
            "ssl_mode": "disable"
        },
        "logging": {
            "level": "info",
            "format": "json",
            "output": "stdout"
        },
        "features": ["auth", "metrics"],
        "debug": false,
        "max_retries": 3
    }`)
    if err != nil {
        fmt.Println("ERROR:", err)
        return
    }

    // Show flattened config
    flat := baseConfig.Flatten()
    paths := make([]string, 0, len(flat))
    for p := range flat {
        paths = append(paths, p)
    }
    sort.Strings(paths)

    for _, p := range paths {
        cv := flat[p]
        fmt.Printf("  %-40s %-12s = %s\n", p, "("+cv.TypeName()+")", cv.Display())
    }

    // --- 2. Type-safe access with ConfigValue ---

    fmt.Println("\n── 2. Type-Safe Access ──")

    if port, ok := baseConfig.Get("server.port").Int(); ok {
        fmt.Printf("  server.port = %d (as int)\n", port)
    }
    if host, ok := baseConfig.Get("server.host").String(); ok {
        fmt.Printf("  server.host = %s (as string)\n", host)
    }
    if debug, ok := baseConfig.Get("debug").Bool(); ok {
        fmt.Printf("  debug = %v (as bool)\n", debug)
    }
    if features, ok := baseConfig.Get("features").StringSlice(); ok {
        fmt.Printf("  features = %v (as []string)\n", features)
    }

    // Access missing key — safe
    if _, ok := baseConfig.Get("nonexistent.key").String(); !ok {
        fmt.Println("  nonexistent.key → not found (safe, no panic)")
    }

    // --- 3. Production override config ---

    fmt.Println("\n── 3. Production Override ──")

    prodConfig := NewConfigStore("production")
    prodConfig.LoadJSON(`{
        "server": {
            "port": 443,
            "tls": {
                "enabled": true,
                "cert_path": "${TLS_CERT_PATH:-/etc/ssl/cert.pem}",
                "key_path": "${TLS_KEY_PATH:-/etc/ssl/key.pem}"
            }
        },
        "database": {
            "host": "${DB_HOST:-db.prod.internal}",
            "port": 5432,
            "pool_size": 50,
            "ssl_mode": "verify-full"
        },
        "logging": {
            "level": "warn"
        },
        "features": ["auth", "metrics", "tracing", "rate-limit"],
        "debug": false
    }`)

    // --- 4. Merge configs ---

    fmt.Println("\n── 4. Merged Configuration (base + prod) ──")

    merged := MergeConfigs(baseConfig, prodConfig)

    flat = merged.Flatten()
    paths = make([]string, 0, len(flat))
    for p := range flat {
        paths = append(paths, p)
    }
    sort.Strings(paths)

    for _, p := range paths {
        cv := flat[p]
        fmt.Printf("  %-40s = %s\n", p, cv.Display())
    }

    // --- 5. Resolve environment variables ---

    fmt.Println("\n── 5. Resolving Environment Variables ──")

    // Simulate setting some env vars
    os.Setenv("DB_HOST", "db-primary.prod.internal")
    os.Setenv("TLS_CERT_PATH", "/etc/letsencrypt/live/myapp/fullchain.pem")
    defer os.Unsetenv("DB_HOST")
    defer os.Unsetenv("TLS_CERT_PATH")

    count := ResolveEnvVars(merged)
    fmt.Printf("  Resolved %d environment variable references\n", count)

    // Show resolved values
    dbHost, _ := merged.Get("database.host").String()
    certPath, _ := merged.Get("server.tls.cert_path").String()
    keyPath, _ := merged.Get("server.tls.key_path").String()

    fmt.Printf("  database.host    = %s (from $DB_HOST)\n", dbHost)
    fmt.Printf("  tls.cert_path    = %s (from $TLS_CERT_PATH)\n", certPath)
    fmt.Printf("  tls.key_path     = %s (default — $TLS_KEY_PATH not set)\n", keyPath)

    // --- 6. Validate configuration ---

    fmt.Println("\n── 6. Validating Configuration ──")

    minPort := 1.0
    maxPort := 65535.0
    minPool := 1.0
    maxPool := 200.0

    rules := []ValidationRule{
        {Path: "server.host", Required: true, Type: "string"},
        {Path: "server.port", Required: true, Type: "float", Min: &minPort, Max: &maxPort},
        {Path: "database.host", Required: true, Type: "string"},
        {Path: "database.port", Required: true, Type: "float", Min: &minPort, Max: &maxPort},
        {Path: "database.pool_size", Required: true, Type: "float", Min: &minPool, Max: &maxPool},
        {Path: "database.ssl_mode", Required: true, Type: "string",
            OneOf: []any{"disable", "require", "verify-ca", "verify-full"}},
        {Path: "logging.level", Required: true, Type: "string",
            OneOf: []any{"debug", "info", "warn", "error"}},
        {Path: "nonexistent.required", Required: true, Type: "string"},
    }

    errors := ValidateConfig(merged, rules)
    if len(errors) == 0 {
        fmt.Println("  All validation rules passed ✓")
    } else {
        for _, e := range errors {
            fmt.Printf("  FAIL: %s\n", e)
        }
    }

    // --- 7. Diff configs ---

    fmt.Println("\n── 7. Configuration Diff (base vs merged+resolved) ──")

    diffs := DiffConfigs(baseConfig, merged)
    for _, d := range diffs {
        switch d.Type {
        case "added":
            fmt.Printf("  + %-40s = %s\n", d.Path, d.NewVal.Display())
        case "removed":
            fmt.Printf("  - %-40s = %s\n", d.Path, d.OldVal.Display())
        case "changed":
            fmt.Printf("  ~ %-40s %s → %s\n", d.Path, d.OldVal.Display(), d.NewVal.Display())
        }
    }

    // --- 8. Dynamic set ---

    fmt.Println("\n── 8. Dynamic Set (runtime modification) ──")

    merged.Set("server.graceful_shutdown_ms", 15000.0)
    merged.Set("monitoring.enabled", true)
    merged.Set("monitoring.endpoint", "/metrics")
    merged.Set("monitoring.interval_sec", 30.0)

    for _, path := range []string{
        "server.graceful_shutdown_ms",
        "monitoring.enabled",
        "monitoring.endpoint",
        "monitoring.interval_sec",
    } {
        cv := merged.Get(path)
        fmt.Printf("  SET %-40s = %s (%s)\n", path, cv.Display(), cv.TypeName())
    }

    // --- 9. Type introspection with type switch ---

    fmt.Println("\n── 9. Type Introspection ──")

    flat = merged.Flatten()
    typeCounts := make(map[string]int)
    for _, cv := range flat {
        typeCounts[cv.TypeName()]++
    }

    fmt.Println("  Value types in final config:")
    typeNames := make([]string, 0, len(typeCounts))
    for t := range typeCounts {
        typeNames = append(typeNames, t)
    }
    sort.Strings(typeNames)
    for _, t := range typeNames {
        fmt.Printf("    %-20s %d values\n", t, typeCounts[t])
    }

    // --- 10. Summary ---

    fmt.Println("\n── Summary ──")
    fmt.Printf("  Base config:     %d keys\n", len(baseConfig.Flatten()))
    fmt.Printf("  Prod override:   %d keys\n", len(prodConfig.Flatten()))
    fmt.Printf("  Merged config:   %d keys\n", len(merged.Flatten()))
    fmt.Printf("  Env vars resolved: %d\n", count)
    fmt.Printf("  Validation errors: %d\n", len(errors))
    fmt.Printf("  Config diffs:      %d\n", len(diffs))

    fmt.Println("\n  any/interface{} permite manejar configuraciones dinamicas")
    fmt.Println("  donde los tipos de los valores no se conocen en compilacion.")
    fmt.Println("  Type assertions y type switches extraen los tipos de forma segura.")
}
```

**Output esperado:**
```
╔══════════════════════════════════════════════════════╗
║   Dynamic Configuration Engine — any/type switch    ║
╚══════════════════════════════════════════════════════╝

── 1. Loading Base Configuration ──
  database.host                            (string)     = "localhost"
  database.name                            (string)     = "myapp"
  database.pool_size                       (float64)    = 10
  database.port                            (float64)    = 5432
  database.ssl_mode                        (string)     = "disable"
  debug                                    (bool)       = false
  features                                 ([]interface {}) = ["auth", "metrics"]
  logging.format                           (string)     = "json"
  logging.level                            (string)     = "info"
  logging.output                           (string)     = "stdout"
  max_retries                              (float64)    = 3
  server.host                              (string)     = "0.0.0.0"
  server.port                              (float64)    = 8080
  server.read_timeout_ms                   (float64)    = 30000
  server.tls.cert_path                     (string)     = ""
  server.tls.enabled                       (bool)       = false
  server.tls.key_path                      (string)     = ""
  server.write_timeout_ms                  (float64)    = 30000

── 2. Type-Safe Access ──
  server.port = 8080 (as int)
  server.host = 0.0.0.0 (as string)
  debug = false (as bool)
  features = [auth metrics] (as []string)
  nonexistent.key → not found (safe, no panic)

── 4. Merged Configuration (base + prod) ──
  ...merged values with prod overrides...

── 5. Resolving Environment Variables ──
  Resolved 2 environment variable references
  database.host    = db-primary.prod.internal (from $DB_HOST)
  tls.cert_path    = /etc/letsencrypt/live/myapp/fullchain.pem (from $TLS_CERT_PATH)
  tls.key_path     = /etc/ssl/key.pem (default — $TLS_KEY_PATH not set)

── 6. Validating Configuration ──
  FAIL: nonexistent.required: required field is missing

── 7. Configuration Diff (base vs merged+resolved) ──
  ~ database.host               "localhost" → "db-primary.prod.internal"
  ~ database.pool_size          10 → 50
  ~ database.ssl_mode           "disable" → "verify-full"
  ~ features                    ["auth", "metrics"] → ["auth", "metrics", "tracing", "rate-limit"]
  ~ logging.level               "info" → "warn"
  ~ server.port                 8080 → 443
  ~ server.tls.cert_path        "" → "/etc/letsencrypt/live/myapp/fullchain.pem"
  ~ server.tls.enabled          false → true
  ~ server.tls.key_path         "" → "/etc/ssl/key.pem"
```

---

## 16. Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│  INTERFACE VACIA (any) — Resumen                                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. any = interface{} — alias desde Go 1.18, usar any en codigo nuevo    │
│  2. Todo tipo satisface any — es el tipo universal de Go                 │
│  3. Pierde seguridad de tipos — el compilador no verifica nada           │
│  4. Type assertion x.(T) — extraer tipo concreto                         │
│     ├─ Simple: panic si falla                                            │
│     └─ Comma-ok: v, ok := x.(T) — safe, zero value + false si falla     │
│  5. Type switch — switch v.(type) { case int: ... case string: ... }     │
│     ├─ Idiomatico para manejar multiples tipos                           │
│     ├─ Multiples tipos en un case: val es any, no un tipo especifico     │
│     └─ No es exhaustivo — siempre tener default                          │
│  6. JSON → map[string]any: numeros son SIEMPRE float64                   │
│  7. any nil vs any(*T)(nil) — la trampa de nil interface                 │
│  8. Comparar any: panic si el tipo dinamico no es comparable             │
│  9. Preferir genericos (1.18+) sobre any cuando sea posible              │
│ 10. Usar any solo para: JSON dinamico, heterogeneo real, reflection,     │
│     interop con APIs legacy, fmt/log variadic                            │
│                                                                          │
│  SysAdmin/DevOps:                                                        │
│  ├─ JSON/YAML configs dinamicas: map[string]any + type switches          │
│  ├─ Event systems: payload any con type switch en handler                │
│  ├─ Plugin metadata: annotations/labels heterogeneos                     │
│  └─ Validacion: type switch + helpers para extraer de forma segura       │
│                                                                          │
│  vs C: any es type-safe (tiene info de tipo); void* es UB                │
│  vs Rust: Rust prefiere enums (exhaustivos) sobre dyn Any                │
│           Go type switch no es exhaustivo — necesitas default            │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: T03 - Interfaces comunes — io.Reader, io.Writer, fmt.Stringer, error, sort.Interface
