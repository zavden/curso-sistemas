# T04 — Zero values

## 1. ¿Qué son los zero values?

En Go, toda variable declarada sin un valor explícito se inicializa
automáticamente a su **zero value**. No existe el concepto de variable
"sin inicializar" ni de memoria basura. Esto es una garantía del lenguaje.

```
    ┌──────────────────────────────────────────────────────────────┐
    │                Zero values en Go                             │
    │                                                              │
    │  PRINCIPIO: toda variable tiene un valor conocido desde      │
    │  el momento de su declaración. No hay basura, no hay         │
    │  undefined, no hay null para tipos valor.                    │
    │                                                              │
    │  ┌────────────────────┬──────────────────────────────────┐   │
    │  │ Tipo               │ Zero value                       │   │
    │  ├────────────────────┼──────────────────────────────────┤   │
    │  │ bool               │ false                            │   │
    │  │ int, uint, float   │ 0                                │   │
    │  │ complex            │ 0+0i                             │   │
    │  │ string             │ "" (string vacío)                │   │
    │  │ pointer            │ nil                               │   │
    │  │ slice              │ nil                               │   │
    │  │ map                │ nil                               │   │
    │  │ channel            │ nil                               │   │
    │  │ function           │ nil                               │   │
    │  │ interface          │ nil                               │   │
    │  │ struct             │ cada campo a su zero value        │   │
    │  │ array              │ cada elemento a su zero value     │   │
    │  └────────────────────┴──────────────────────────────────┘   │
    │                                                              │
    │  Comparación:                                                │
    │  C      → variables locales contienen BASURA (undefined)    │
    │  Rust   → el compilador PROHÍBE usar variables sin init     │
    │  Python → NameError si usas una variable no definida        │
    │  Go     → todo tiene un valor conocido, SIEMPRE             │
    └──────────────────────────────────────────────────────────────┘
```

### 1.1 ¿Por qué son importantes?

```bash
# 1. SEGURIDAD: no hay memory bugs por variables sin inicializar.
#    En C, leer una variable local sin inicializar es undefined behavior.
#    En Go, leer cualquier variable siempre devuelve un valor predecible.

# 2. DISEÑO DE APIs: los zero values DEBEN ser útiles.
#    En Go se dice: "make the zero value useful".
#    Un buen tipo en Go funciona correctamente sin inicialización explícita.

# 3. SIMPLICIDAD: no necesitas constructores obligatorios.
#    var buf bytes.Buffer  ← listo para usar, sin New()
#    var wg sync.WaitGroup ← listo para usar, sin New()
#    var mu sync.Mutex     ← listo para usar, sin New()

# 4. IDIOMA GO: muchas APIs dependen del zero value.
#    Si un campo de un struct es false/0/"", significa "usar el default".
#    Esto elimina la necesidad de Option types en muchos casos.
```

---

## 2. Zero values de tipos primitivos

```go
package main

import "fmt"

func main() {
    // Booleanos:
    var b bool
    fmt.Printf("bool:       %v\n", b)     // false

    // Enteros:
    var i int
    var i8 int8
    var i16 int16
    var i32 int32
    var i64 int64
    var u uint
    var u8 uint8
    var u64 uint64
    var uptr uintptr
    fmt.Printf("int:        %v\n", i)     // 0
    fmt.Printf("int8:       %v\n", i8)    // 0
    fmt.Printf("int16:      %v\n", i16)   // 0
    fmt.Printf("int32:      %v\n", i32)   // 0
    fmt.Printf("int64:      %v\n", i64)   // 0
    fmt.Printf("uint:       %v\n", u)     // 0
    fmt.Printf("uint8:      %v\n", u8)    // 0
    fmt.Printf("uint64:     %v\n", u64)   // 0
    fmt.Printf("uintptr:    %v\n", uptr)  // 0

    // Flotantes:
    var f32 float32
    var f64 float64
    fmt.Printf("float32:    %v\n", f32)   // 0
    fmt.Printf("float64:    %v\n", f64)   // 0

    // Complejos:
    var c64 complex64
    var c128 complex128
    fmt.Printf("complex64:  %v\n", c64)   // (0+0i)
    fmt.Printf("complex128: %v\n", c128)  // (0+0i)

    // String:
    var s string
    fmt.Printf("string:     %q\n", s)     // "" (string vacío)
    fmt.Printf("len:        %d\n", len(s)) // 0

    // Byte y rune (aliases):
    var by byte
    var r rune
    fmt.Printf("byte:       %v\n", by)    // 0
    fmt.Printf("rune:       %v (%c)\n", r, r) // 0 (null character)
}
```

---

## 3. Zero values de tipos compuestos

### 3.1 Punteros

```go
package main

import "fmt"

func main() {
    // Zero value de un puntero es nil:
    var p *int
    fmt.Println(p)       // <nil>
    fmt.Println(p == nil) // true

    // ⚠️ Derreferenciar un puntero nil causa PANIC:
    // fmt.Println(*p)   // panic: runtime error: invalid memory address
    //                    // or nil pointer dereference

    // Patrón seguro:
    if p != nil {
        fmt.Println(*p)
    }

    // Asignar un valor:
    n := 42
    p = &n
    fmt.Println(*p)  // 42
}
```

### 3.2 Slices

```go
package main

import "fmt"

func main() {
    // Zero value de un slice es nil:
    var s []int
    fmt.Println(s)         // []
    fmt.Println(s == nil)  // true
    fmt.Println(len(s))    // 0
    fmt.Println(cap(s))    // 0

    // ¡Un slice nil es USABLE!
    // Puedes hacer append sin inicializar:
    s = append(s, 1, 2, 3)
    fmt.Println(s)         // [1 2 3]
    fmt.Println(s == nil)  // false (ya no es nil)

    // range sobre slice nil no causa panic:
    var empty []string
    for _, v := range empty {
        fmt.Println(v)  // nunca se ejecuta
    }

    // len() y cap() sobre slice nil devuelven 0:
    fmt.Println(len(empty))  // 0
    fmt.Println(cap(empty))  // 0

    // ⚠️ Pero indexar un slice nil SÍ causa panic:
    // fmt.Println(empty[0])  // panic: index out of range

    // Diferencia: nil slice vs empty slice
    var nilSlice []int          // nil: s == nil → true
    emptySlice := []int{}       // empty: s == nil → false
    madeSlice := make([]int, 0) // empty: s == nil → false

    // Los tres se comportan IGUAL para len, cap, range, append.
    // La diferencia solo importa para == nil check
    // y para serialización JSON:
    // nil slice → JSON: null
    // empty slice → JSON: []

    _ = nilSlice; _ = emptySlice; _ = madeSlice
}
```

### 3.3 Maps

```go
package main

import "fmt"

func main() {
    // Zero value de un map es nil:
    var m map[string]int
    fmt.Println(m)         // map[]
    fmt.Println(m == nil)  // true
    fmt.Println(len(m))    // 0

    // LEER de un map nil funciona (devuelve zero value del tipo valor):
    v := m["key"]
    fmt.Println(v)  // 0

    // Comma-ok idiom también funciona:
    v, ok := m["key"]
    fmt.Println(v, ok)  // 0 false

    // ⚠️ ESCRIBIR en un map nil causa PANIC:
    // m["key"] = 42  // panic: assignment to entry in nil map

    // Hay que inicializar antes de escribir:
    m = make(map[string]int)
    m["key"] = 42  // ✓ funciona

    // O inicializar con literal:
    m = map[string]int{"key": 42}

    // range sobre map nil no causa panic:
    var nilMap map[string]string
    for k, v := range nilMap {
        fmt.Println(k, v)  // nunca se ejecuta
    }
}
```

### 3.4 Channels

```go
package main

import "fmt"

func main() {
    // Zero value de un channel es nil:
    var ch chan int
    fmt.Println(ch)        // <nil>
    fmt.Println(ch == nil) // true

    // ⚠️ Enviar a un channel nil BLOQUEA para siempre:
    // ch <- 42  // deadlock — bloquea forever

    // ⚠️ Recibir de un channel nil BLOQUEA para siempre:
    // v := <-ch  // deadlock — bloquea forever

    // ⚠️ Cerrar un channel nil causa PANIC:
    // close(ch)  // panic: close of nil channel

    // Un channel nil es útil en select para desactivar un case:
    // (se cubre en el capítulo de concurrencia)

    // Hay que inicializar con make:
    ch = make(chan int, 1)
    ch <- 42
    fmt.Println(<-ch)  // 42
}
```

### 3.5 Functions

```go
package main

import "fmt"

func main() {
    // Zero value de una variable function es nil:
    var fn func(int) int
    fmt.Println(fn)        // <nil>
    fmt.Println(fn == nil) // true

    // ⚠️ Llamar a una función nil causa PANIC:
    // fn(42)  // panic: runtime error: invalid memory address

    // Patrón seguro — verificar antes de llamar:
    if fn != nil {
        result := fn(42)
        fmt.Println(result)
    }

    // Asignar una función:
    fn = func(x int) int { return x * 2 }
    fmt.Println(fn(21))  // 42

    // Uso práctico: callbacks opcionales
    type Config struct {
        OnStart func()
        OnStop  func()
    }

    cfg := Config{
        OnStart: func() { fmt.Println("Starting...") },
        // OnStop no se asigna → nil → se puede verificar
    }

    if cfg.OnStart != nil {
        cfg.OnStart()
    }
    if cfg.OnStop != nil {
        cfg.OnStop()
    }
}
```

### 3.6 Interfaces

```go
package main

import "fmt"

func main() {
    // Zero value de una interface es nil:
    var err error
    fmt.Println(err)        // <nil>
    fmt.Println(err == nil) // true

    // Una interface nil no tiene tipo ni valor:
    var i interface{}
    fmt.Println(i)        // <nil>
    fmt.Println(i == nil) // true

    // ⚠️ GOTCHA: interface con tipo pero valor nil NO es nil:
    var p *int = nil
    var iface interface{} = p
    fmt.Println(p == nil)     // true
    fmt.Println(iface == nil) // false ← ¡SORPRESA!

    // Esto pasa porque una interface almacena (tipo, valor):
    // var err error = nil       → (nil, nil)     → err == nil: true
    // var err error = (*int)(nil) → (*int, nil)  → err == nil: false
    //
    // La interface tiene un tipo asignado (*int), aunque el valor sea nil.
    // Para que interface == nil, TANTO el tipo como el valor deben ser nil.
}
```

---

## 4. Zero values de structs

```go
package main

import (
    "fmt"
    "time"
)

type ServerConfig struct {
    Host      string        // → ""
    Port      int           // → 0
    EnableTLS bool          // → false
    Timeout   time.Duration // → 0 (time.Duration es int64)
    MaxConns  int           // → 0
    Tags      []string      // → nil
    Metadata  map[string]string // → nil
    Logger    func(string)  // → nil
}

func main() {
    // Declarar sin inicializar — todos los campos a zero value:
    var cfg ServerConfig

    fmt.Printf("Host:      %q\n", cfg.Host)       // ""
    fmt.Printf("Port:      %d\n", cfg.Port)        // 0
    fmt.Printf("EnableTLS: %t\n", cfg.EnableTLS)   // false
    fmt.Printf("Timeout:   %v\n", cfg.Timeout)     // 0s
    fmt.Printf("MaxConns:  %d\n", cfg.MaxConns)    // 0
    fmt.Printf("Tags:      %v (nil=%t)\n", cfg.Tags, cfg.Tags == nil)
    //          Tags:      [] (nil=true)
    fmt.Printf("Metadata:  %v (nil=%t)\n", cfg.Metadata, cfg.Metadata == nil)
    //          Metadata:  map[] (nil=true)
    fmt.Printf("Logger:    %v\n", cfg.Logger == nil) // true
}
```

### 4.1 Structs anidados

```go
type Address struct {
    Street string
    City   string
    State  string
    ZIP    string
}

type Employee struct {
    Name    string
    Age     int
    Address Address   // struct embebido → zero value recursivo
    Manager *Employee // puntero → nil
}

func main() {
    var emp Employee

    fmt.Println(emp.Name)           // "" (zero value de string)
    fmt.Println(emp.Age)            // 0
    fmt.Println(emp.Address.Street) // "" (zero value recursivo)
    fmt.Println(emp.Address.City)   // ""
    fmt.Println(emp.Manager)       // <nil> (puntero zero value)

    // El struct entero es una composición de zero values.
    // NO hay null — cada campo tiene un valor definido.
}
```

---

## 5. Zero values de arrays

```go
package main

import "fmt"

func main() {
    // Arrays se inicializan con zero value de cada elemento:
    var ints [5]int
    fmt.Println(ints)  // [0 0 0 0 0]

    var strs [3]string
    fmt.Println(strs)  // [   ] (tres strings vacíos)

    var bools [4]bool
    fmt.Println(bools) // [false false false false]

    // Arrays de structs:
    type Point struct {
        X, Y float64
    }
    var points [3]Point
    fmt.Println(points) // [{0 0} {0 0} {0 0}]

    // A diferencia de slices, un array NUNCA es nil.
    // Un array siempre tiene un tamaño fijo y todos sus elementos.
}
```

---

## 6. "Make the zero value useful" — principio de diseño de Go

Este es uno de los principios más importantes del diseño de Go.
Un tipo bien diseñado debe ser útil con su zero value,
sin necesidad de inicialización explícita.

### 6.1 Ejemplos de la stdlib

```go
package main

import (
    "bytes"
    "fmt"
    "strings"
    "sync"
)

func main() {
    // bytes.Buffer — listo para usar sin make/New:
    var buf bytes.Buffer
    buf.WriteString("Hello, ")
    buf.WriteString("World!")
    fmt.Println(buf.String())  // Hello, World!

    // strings.Builder — listo para usar sin make/New:
    var sb strings.Builder
    sb.WriteString("one ")
    sb.WriteString("two ")
    sb.WriteString("three")
    fmt.Println(sb.String())  // one two three

    // sync.Mutex — listo para usar sin make/New:
    var mu sync.Mutex
    mu.Lock()
    // ... sección crítica ...
    mu.Unlock()

    // sync.WaitGroup — listo para usar sin make/New:
    var wg sync.WaitGroup
    wg.Add(1)
    go func() {
        defer wg.Done()
        fmt.Println("worker done")
    }()
    wg.Wait()

    // sync.Once — listo para usar sin make/New:
    var once sync.Once
    once.Do(func() {
        fmt.Println("this runs only once")
    })

    // sync.Map — listo para usar sin make/New:
    var sm sync.Map
    sm.Store("key", "value")
    v, _ := sm.Load("key")
    fmt.Println(v)  // value
}
```

### 6.2 Diseñar tus propios tipos con zero values útiles

```go
// BIEN — zero value útil:

// Logger con zero value que escribe a os.Stderr:
type Logger struct {
    Prefix string
    Level  int
    // output no exportado, nil = usar os.Stderr
    output io.Writer
}

func (l *Logger) writer() io.Writer {
    if l.output != nil {
        return l.output
    }
    return os.Stderr  // default útil
}

func (l *Logger) Info(msg string) {
    fmt.Fprintf(l.writer(), "%s[INFO] %s\n", l.Prefix, msg)
}

// Uso:
var log Logger           // funciona sin inicializar
log.Info("hello")        // escribe a stderr
log.Prefix = "[myapp] "
log.Info("hello")        // [myapp] [INFO] hello
```

```go
// BIEN — zero value = configuración por defecto:

type Server struct {
    Host         string        // "" → escucha en todas las interfaces
    Port         int           // 0 → puerto aleatorio
    ReadTimeout  time.Duration // 0 → sin timeout
    WriteTimeout time.Duration // 0 → sin timeout
    MaxBodySize  int64         // 0 → tratarlo como "usar default"
}

func (s *Server) listenAddr() string {
    host := s.Host  // "" es válido para net.Listen (all interfaces)
    port := s.Port
    if port == 0 {
        port = 8080  // default
    }
    return fmt.Sprintf("%s:%d", host, port)
}

func (s *Server) maxBody() int64 {
    if s.MaxBodySize == 0 {
        return 10 << 20  // default: 10 MB
    }
    return s.MaxBodySize
}

// Uso — zero value funciona:
var srv Server
srv.Start()  // escucha en :8080 con defaults
```

```go
// MAL — zero value inútil:

type BadClient struct {
    BaseURL string
    Token   string
    Client  *http.Client  // nil → panic al usar
}

// Con zero value: var c BadClient → c.Client es nil → panic
// Necesitas un constructor: NewBadClient(url, token)

// MEJOR — hacer el zero value seguro:

type GoodClient struct {
    BaseURL string
    Token   string
    Client  *http.Client
}

func (c *GoodClient) httpClient() *http.Client {
    if c.Client != nil {
        return c.Client
    }
    return http.DefaultClient  // fallback a default
}
```

---

## 7. Zero value vs nil — distinción importante

```
    ┌──────────────────────────────────────────────────────────────┐
    │              Zero value vs nil en Go                         │
    │                                                              │
    │  TIPOS VALOR (nunca nil):                                    │
    │  ┌────────────────────────────────────────────────────────┐  │
    │  │ bool, int, float, complex, string, array, struct      │  │
    │  │                                                        │  │
    │  │ Siempre tienen un valor. No pueden ser nil.           │  │
    │  │ var x int → 0 (no nil)                                │  │
    │  │ var s string → "" (no nil)                            │  │
    │  │ var cfg Config → todos campos a zero value (no nil)   │  │
    │  └────────────────────────────────────────────────────────┘  │
    │                                                              │
    │  TIPOS REFERENCIA (pueden ser nil):                          │
    │  ┌────────────────────────────────────────────────────────┐  │
    │  │ pointer, slice, map, channel, function, interface     │  │
    │  │                                                        │  │
    │  │ Su zero value ES nil.                                 │  │
    │  │ nil = "no apunta a nada" / "no existe"                │  │
    │  │ var p *int → nil                                      │  │
    │  │ var s []int → nil                                     │  │
    │  │ var m map[string]int → nil                            │  │
    │  └────────────────────────────────────────────────────────┘  │
    │                                                              │
    │  ⚠️ nil NO es null:                                         │
    │  - nil en Go no es un tipo universal (como null en Java)    │
    │  - nil tiene el tipo del contexto donde se usa              │
    │  - No puedes hacer: var x = nil (sin tipo, no compila)      │
    │  - nil solo se puede asignar a los tipos de referencia      │
    └──────────────────────────────────────────────────────────────┘
```

```go
package main

import "fmt"

func main() {
    // nil solo para tipos referencia:
    var p *int = nil             // ✓ puntero
    var s []int = nil            // ✓ slice
    var m map[string]int = nil   // ✓ map
    var ch chan int = nil         // ✓ channel
    var fn func() = nil          // ✓ function
    var i interface{} = nil      // ✓ interface

    // NO se puede asignar nil a tipos valor:
    // var n int = nil            // ✗ cannot use nil as int
    // var str string = nil       // ✗ cannot use nil as string
    // var b bool = nil           // ✗ cannot use nil as bool

    // nil sin contexto no compila:
    // x := nil                   // ✗ use of untyped nil

    _ = p; _ = s; _ = m; _ = ch; _ = fn; _ = i
}
```

---

## 8. Detectar zero value — patrones comunes

### 8.1 Verificar si un valor es el zero value

```go
package main

import (
    "fmt"
    "reflect"
)

func main() {
    // Para tipos conocidos, comparar directamente:
    var n int
    if n == 0 { fmt.Println("int is zero") }

    var s string
    if s == "" { fmt.Println("string is zero") }

    var b bool
    if !b { fmt.Println("bool is zero (false)") }

    var f float64
    if f == 0 { fmt.Println("float is zero") }

    // Para punteros, slices, maps, channels, interfaces:
    var p *int
    if p == nil { fmt.Println("pointer is nil") }

    var sl []int
    if sl == nil { fmt.Println("slice is nil") }

    var m map[string]int
    if m == nil { fmt.Println("map is nil") }

    // Para structs, comparar con el zero value del tipo:
    type Config struct {
        Host string
        Port int
    }
    var cfg Config
    if cfg == (Config{}) {
        fmt.Println("config is zero value")
    }

    // Genérico con reflect (lento, usar solo si es necesario):
    fmt.Println(reflect.ValueOf(n).IsZero())   // true
    fmt.Println(reflect.ValueOf(s).IsZero())   // true
    fmt.Println(reflect.ValueOf(cfg).IsZero()) // true
}
```

### 8.2 Patrón: usar zero value como "no configurado"

```go
type DatabaseConfig struct {
    Host     string        // "" → usar localhost
    Port     int           // 0 → usar 5432
    Database string        // "" → usar "myapp"
    SSLMode  string        // "" → usar "disable"
    Timeout  time.Duration // 0 → usar 30s
    MaxConns int           // 0 → usar 10
}

func (c *DatabaseConfig) Effective() DatabaseConfig {
    result := *c  // copiar

    if result.Host == "" {
        result.Host = "localhost"
    }
    if result.Port == 0 {
        result.Port = 5432
    }
    if result.Database == "" {
        result.Database = "myapp"
    }
    if result.SSLMode == "" {
        result.SSLMode = "disable"
    }
    if result.Timeout == 0 {
        result.Timeout = 30 * time.Second
    }
    if result.MaxConns == 0 {
        result.MaxConns = 10
    }

    return result
}

func (c *DatabaseConfig) DSN() string {
    eff := c.Effective()
    return fmt.Sprintf("host=%s port=%d dbname=%s sslmode=%s connect_timeout=%d",
        eff.Host, eff.Port, eff.Database, eff.SSLMode,
        int(eff.Timeout.Seconds()))
}

// Uso:
var cfg DatabaseConfig  // todo zero value
dsn := cfg.DSN()
// "host=localhost port=5432 dbname=myapp sslmode=disable connect_timeout=30"

// Con overrides parciales:
cfg.Host = "db.prod.internal"
cfg.Port = 5433
dsn = cfg.DSN()
// "host=db.prod.internal port=5433 dbname=myapp sslmode=disable connect_timeout=30"
```

### 8.3 El problema del cero legítimo

```go
// ¿Qué pasa si 0 es un valor LEGÍTIMO, no "no configurado"?
// Ejemplo: puerto 0 significa "puerto aleatorio" en net.Listen.
// Pero en nuestro struct, 0 significaba "usar default 5432".

// Solución 1: usar punteros para campos opcionales:
type Config struct {
    Port *int  // nil = usar default, *0 = puerto aleatorio
}

// Verificar:
if cfg.Port == nil {
    // usar default
} else if *cfg.Port == 0 {
    // puerto aleatorio explícito
} else {
    // usar *cfg.Port
}

// Solución 2: campo separado para "fue configurado":
type Config struct {
    Port    int
    PortSet bool  // true si se configuró explícitamente
}

// Solución 3: usar un valor sentinel que no es válido:
type Config struct {
    Port int  // -1 = usar default, 0 = aleatorio, >0 = específico
}

// Cada solución tiene trade-offs:
// Punteros: nil safety, requiere derreferenciar
// Bool separado: más campos, más claro
// Sentinel: simple, pero el sentinel debe documentarse
```

---

## 9. Zero values y JSON

La interacción entre zero values y serialización JSON es una de las
áreas más prácticas donde este concepto importa.

### 9.1 omitempty — omitir zero values en JSON

```go
package main

import (
    "encoding/json"
    "fmt"
)

type ServerInfo struct {
    Name      string  `json:"name"`
    IP        string  `json:"ip"`
    Port      int     `json:"port"`
    Healthy   bool    `json:"healthy"`
    CPUUsage  float64 `json:"cpu_usage"`
    Tags      []string `json:"tags"`
}

type ServerInfoOmit struct {
    Name      string   `json:"name,omitempty"`
    IP        string   `json:"ip,omitempty"`
    Port      int      `json:"port,omitempty"`
    Healthy   bool     `json:"healthy,omitempty"`
    CPUUsage  float64  `json:"cpu_usage,omitempty"`
    Tags      []string `json:"tags,omitempty"`
}

func main() {
    // Sin omitempty — incluye TODOS los campos:
    s1 := ServerInfo{Name: "web-01", IP: "10.0.0.1"}
    data, _ := json.MarshalIndent(s1, "", "  ")
    fmt.Println(string(data))
    // {
    //   "name": "web-01",
    //   "ip": "10.0.0.1",
    //   "port": 0,          ← incluido aunque es zero
    //   "healthy": false,   ← incluido aunque es zero
    //   "cpu_usage": 0,     ← incluido aunque es zero
    //   "tags": null         ← nil slice → null
    // }

    // Con omitempty — omite campos con zero value:
    s2 := ServerInfoOmit{Name: "web-01", IP: "10.0.0.1"}
    data, _ = json.MarshalIndent(s2, "", "  ")
    fmt.Println(string(data))
    // {
    //   "name": "web-01",
    //   "ip": "10.0.0.1"
    // }
    // Port, Healthy, CPUUsage, Tags omitidos porque son zero values.
}
```

### 9.2 El problema de omitempty con bool y 0

```go
// ⚠️ omitempty omite false y 0, que pueden ser valores intencionales:

type Alert struct {
    Name     string `json:"name,omitempty"`
    Severity int    `json:"severity,omitempty"`  // 0 = info
    Silenced bool   `json:"silenced,omitempty"`  // false = no silenciado
}

a := Alert{Name: "high-cpu", Severity: 0, Silenced: false}
data, _ := json.Marshal(a)
fmt.Println(string(data))
// {"name":"high-cpu"}
// ¡Severity y Silenced desaparecieron! Pero 0 y false ERAN los valores.

// Solución: usar punteros para campos donde 0/false son válidos:

type AlertFixed struct {
    Name     string `json:"name,omitempty"`
    Severity *int   `json:"severity,omitempty"`  // nil = omitir, *0 = info
    Silenced *bool  `json:"silenced,omitempty"`  // nil = omitir, *false = no silenciado
}

severity := 0
silenced := false
a2 := AlertFixed{
    Name:     "high-cpu",
    Severity: &severity,
    Silenced: &silenced,
}
data, _ = json.Marshal(a2)
fmt.Println(string(data))
// {"name":"high-cpu","severity":0,"silenced":false}
// Ahora 0 y false se incluyen porque el puntero no es nil.
```

### 9.3 nil slice vs empty slice en JSON

```go
type Response struct {
    Items []string `json:"items"`
}

// nil slice → JSON null:
r1 := Response{Items: nil}
data, _ := json.Marshal(r1)
fmt.Println(string(data))  // {"items":null}

// empty slice → JSON []:
r2 := Response{Items: []string{}}
data, _ = json.Marshal(r2)
fmt.Println(string(data))  // {"items":[]}

// Esto importa para APIs:
// null puede significar "campo no presente"
// [] significa "lista vacía"
// Algunos clientes los tratan diferente.

// Recomendación para APIs:
// - Si quieres null cuando no hay datos → nil slice
// - Si quieres [] cuando no hay datos → inicializar con []string{}
// - Si usas omitempty → nil se omite, [] se incluye como []
```

### 9.4 Deserializar JSON con campos faltantes

```go
type Config struct {
    Host    string `json:"host"`
    Port    int    `json:"port"`
    Debug   bool   `json:"debug"`
    Workers int    `json:"workers"`
}

// JSON con campos faltantes:
jsonData := `{"host": "localhost", "port": 8080}`

var cfg Config
json.Unmarshal([]byte(jsonData), &cfg)

fmt.Println(cfg.Host)    // "localhost"
fmt.Println(cfg.Port)    // 8080
fmt.Println(cfg.Debug)   // false  ← zero value (no estaba en JSON)
fmt.Println(cfg.Workers) // 0      ← zero value (no estaba en JSON)

// No hay error — los campos faltantes quedan con zero value.
// Esto es consistente con el principio de zero values de Go.

// Para distinguir "faltante" de "presente con valor cero":
type ConfigDetect struct {
    Host    *string `json:"host"`
    Port    *int    `json:"port"`
    Debug   *bool   `json:"debug"`
    Workers *int    `json:"workers"`
}

var cfg2 ConfigDetect
json.Unmarshal([]byte(jsonData), &cfg2)

fmt.Println(cfg2.Debug)   // <nil> ← campo faltante
fmt.Println(cfg2.Workers) // <nil> ← campo faltante
// Ahora puedes distinguir nil (faltante) de *false/*0 (presente).
```

---

## 10. Zero values y YAML/TOML

```go
// El comportamiento con YAML es similar:

import "gopkg.in/yaml.v3"

type AppConfig struct {
    Server struct {
        Host string `yaml:"host"`
        Port int    `yaml:"port"`
    } `yaml:"server"`
    Database struct {
        DSN      string `yaml:"dsn"`
        MaxConns int    `yaml:"max_conns"`
    } `yaml:"database"`
    Features struct {
        EnableCache bool `yaml:"enable_cache"`
        EnableAuth  bool `yaml:"enable_auth"`
    } `yaml:"features"`
}

// Si el YAML solo tiene:
// server:
//   host: localhost
//
// Todos los demás campos quedan en zero value:
// server.Port = 0, database.DSN = "", features.EnableCache = false

// Patrón recomendado: aplicar defaults después de deserializar:
func (c *AppConfig) ApplyDefaults() {
    if c.Server.Port == 0 {
        c.Server.Port = 8080
    }
    if c.Database.MaxConns == 0 {
        c.Database.MaxConns = 10
    }
}
```

---

## 11. Zero values en tests

```go
import "testing"

// Los zero values simplifican los tests:

func TestDefaultConfig(t *testing.T) {
    // Zero value debe funcionar:
    var cfg ServerConfig
    if cfg.listenAddr() != ":8080" {
        t.Errorf("expected :8080, got %s", cfg.listenAddr())
    }
}

func TestServerWithDefaults(t *testing.T) {
    // Solo especificar lo que el test necesita:
    srv := Server{
        Port: 9090,  // solo esto es relevante para el test
        // Host, Timeout, MaxConns → zero values (defaults)
    }
    if srv.listenAddr() != ":9090" {
        t.Errorf("expected :9090, got %s", srv.listenAddr())
    }
}

// Table-driven tests — zero values como defaults:
func TestParsePort(t *testing.T) {
    tests := []struct {
        name    string
        input   string
        want    int
        wantErr bool  // zero value = false = "no espero error"
    }{
        {name: "valid", input: "8080", want: 8080},
        {name: "zero", input: "0", want: 0},
        {name: "invalid", input: "abc", wantErr: true},
        {name: "empty", input: "", wantErr: true},
    }

    for _, tt := range tests {
        t.Run(tt.name, func(t *testing.T) {
            got, err := ParsePort(tt.input)
            if (err != nil) != tt.wantErr {
                t.Errorf("error = %v, wantErr %v", err, tt.wantErr)
                return
            }
            if got != tt.want {
                t.Errorf("got %d, want %d", got, tt.want)
            }
        })
    }
}
// En el test "valid": wantErr no se especifica → false (zero value)
// → "no espero error". Elegante y conciso.
```

---

## 12. Zero values en concurrencia

```go
import "sync"

// sync.Mutex — zero value es un mutex desbloqueado:
type SafeCounter struct {
    mu    sync.Mutex  // no necesita init
    count int
}

func (c *SafeCounter) Inc() {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.count++
}

// sync.WaitGroup — zero value es un grupo con counter 0:
func processAll(items []string) {
    var wg sync.WaitGroup  // no necesita init
    for _, item := range items {
        wg.Add(1)
        go func(s string) {
            defer wg.Done()
            process(s)
        }(item)
    }
    wg.Wait()
}

// sync.Once — zero value es "nunca ejecutado":
type Singleton struct {
    once  sync.Once  // no necesita init
    value *Resource
}

func (s *Singleton) Get() *Resource {
    s.once.Do(func() {
        s.value = loadResource()
    })
    return s.value
}

// ⚠️ NUNCA copies un Mutex, WaitGroup, Once después de usarlos.
// Copiar estos tipos después del primer uso es un bug.
// El vet checker lo detecta: go vet ./...
```

---

## 13. Nil safety — patrones defensivos

```go
// Go no tiene null safety del compilador (como Rust Option o Kotlin ?.)
// La seguridad frente a nil es responsabilidad del programador.

// Patrón 1: nil check explícito
func ProcessUser(u *User) error {
    if u == nil {
        return errors.New("user is nil")
    }
    // ... usar u de forma segura ...
    return nil
}

// Patrón 2: method receiver nil-safe
type Logger struct {
    prefix string
}

func (l *Logger) Info(msg string) {
    if l == nil {
        return  // no-op si el logger es nil
    }
    fmt.Printf("[%s] %s\n", l.prefix, msg)
}

// Uso:
var log *Logger  // nil
log.Info("hello")  // no panic — es un no-op

// Patrón 3: devolver zero value en lugar de nil
func FindUser(id int) User {
    // Devolver User{} (zero value) en lugar de nil
    // El llamador puede verificar: if user.ID == 0 { not found }
    return User{}
}

// vs. devolver puntero (puede ser nil):
func FindUser(id int) *User {
    // Devolver nil si no se encuentra
    // El llamador DEBE verificar nil
    return nil
}

// Patrón 4: initializar maps en constructores
type Registry struct {
    services map[string]string
}

func NewRegistry() *Registry {
    return &Registry{
        services: make(map[string]string),  // ← no nil
    }
}

// Si Registry se usa sin New:
// var r Registry → r.services es nil → panic al escribir
// Alternativa nil-safe:
func (r *Registry) Register(name, addr string) {
    if r.services == nil {
        r.services = make(map[string]string)
    }
    r.services[name] = addr
}
```

---

## 14. Zero values en la práctica SysAdmin

### 14.1 Config con defaults progresivos

```go
package main

import (
    "fmt"
    "time"
)

type MonitorConfig struct {
    // Conexión — zero values son "usar defaults":
    Endpoint    string        // "" → http://localhost:9090
    BearerToken string        // "" → sin autenticación

    // Intervalos — 0 = usar defaults:
    ScrapeInterval time.Duration // 0 → 15s
    Timeout        time.Duration // 0 → 10s
    RetryInterval  time.Duration // 0 → 5s
    MaxRetries     int           // 0 → 3

    // Features — false = desactivado:
    EnableAlerts   bool
    EnableSlack    bool
    SlackWebhook   string

    // Thresholds — 0 = usar defaults:
    CPUThreshold    float64 // 0 → 90.0
    MemoryThreshold float64 // 0 → 85.0
    DiskThreshold   float64 // 0 → 80.0
}

func (c MonitorConfig) WithDefaults() MonitorConfig {
    if c.Endpoint == "" {
        c.Endpoint = "http://localhost:9090"
    }
    if c.ScrapeInterval == 0 {
        c.ScrapeInterval = 15 * time.Second
    }
    if c.Timeout == 0 {
        c.Timeout = 10 * time.Second
    }
    if c.RetryInterval == 0 {
        c.RetryInterval = 5 * time.Second
    }
    if c.MaxRetries == 0 {
        c.MaxRetries = 3
    }
    if c.CPUThreshold == 0 {
        c.CPUThreshold = 90.0
    }
    if c.MemoryThreshold == 0 {
        c.MemoryThreshold = 85.0
    }
    if c.DiskThreshold == 0 {
        c.DiskThreshold = 80.0
    }
    return c
}

func main() {
    // Caso 1: todo defaults
    cfg := MonitorConfig{}.WithDefaults()
    fmt.Printf("Endpoint: %s, Interval: %v, CPU threshold: %.0f%%\n",
        cfg.Endpoint, cfg.ScrapeInterval, cfg.CPUThreshold)
    // Endpoint: http://localhost:9090, Interval: 15s, CPU threshold: 90%

    // Caso 2: overrides parciales
    cfg = MonitorConfig{
        Endpoint:       "http://prometheus:9090",
        ScrapeInterval: 30 * time.Second,
        EnableAlerts:   true,
        CPUThreshold:   95.0,
    }.WithDefaults()
    fmt.Printf("Endpoint: %s, Interval: %v, CPU threshold: %.0f%%\n",
        cfg.Endpoint, cfg.ScrapeInterval, cfg.CPUThreshold)
    // Endpoint: http://prometheus:9090, Interval: 30s, CPU threshold: 95%
}
```

### 14.2 Health check result

```go
// Zero value de HealthResult indica "sin datos todavía":

type HealthResult struct {
    Healthy   bool          // false = no healthy (o no chequeado aún)
    Message   string        // "" = sin mensaje
    Latency   time.Duration // 0 = no medido
    CheckedAt time.Time     // zero time = nunca chequeado
}

func (h HealthResult) Status() string {
    if h.CheckedAt.IsZero() {
        return "UNKNOWN"  // nunca chequeado
    }
    if h.Healthy {
        return "HEALTHY"
    }
    return "UNHEALTHY"
}

// time.Time zero value:
var t time.Time
fmt.Println(t)         // 0001-01-01 00:00:00 +0000 UTC
fmt.Println(t.IsZero()) // true
// IsZero() es la forma idiomática de verificar si un time.Time
// fue inicializado o no.
```

---

## 15. Comparación con otros lenguajes

```
    ┌──────────────────┬──────────────┬──────────────┬──────────────┐
    │ Concepto         │ Go           │ C            │ Rust         │
    ├──────────────────┼──────────────┼──────────────┼──────────────┤
    │ Variable sin     │ Zero value   │ Undefined    │ Error de     │
    │ inicializar      │ (garantizado)│ behavior     │ compilación  │
    ├──────────────────┼──────────────┼──────────────┼──────────────┤
    │ Null/nil         │ nil (solo    │ NULL         │ Option<T>    │
    │                  │ ref types)   │ (cualquier   │ (explícito)  │
    │                  │              │ puntero)     │              │
    ├──────────────────┼──────────────┼──────────────┼──────────────┤
    │ String vacío     │ ""           │ "" o NULL    │ ""           │
    │ vs null          │ (solo "")    │ (ambiguo)    │ (solo "")    │
    ├──────────────────┼──────────────┼──────────────┼──────────────┤
    │ Array sin init   │ [0,0,0...]   │ [basura]     │ Error de     │
    │                  │              │              │ compilación  │
    ├──────────────────┼──────────────┼──────────────┼──────────────┤
    │ Struct sin init  │ Campos a     │ Campos con   │ Debe init    │
    │                  │ zero value   │ basura       │ todos campos │
    ├──────────────────┼──────────────┼──────────────┼──────────────┤
    │ Filosofía        │ "Make the    │ "Performance │ "Make invalid│
    │                  │ zero value   │ over safety" │ states       │
    │                  │ useful"      │              │ unrepresent- │
    │                  │              │              │ able"        │
    └──────────────────┴──────────────┴──────────────┴──────────────┘
```

---

## 16. Tabla de errores comunes

| Error / Síntoma | Causa | Solución |
|---|---|---|
| `panic: assignment to entry in nil map` | Escribir en map sin inicializar | `make(map[K]V)` antes de escribir, o nil-check en método |
| `panic: nil pointer dereference` | Derreferenciar puntero nil | Verificar `p != nil` antes de `*p` |
| `panic: close of nil channel` | Cerrar channel sin inicializar | `make(chan T)` antes de usar |
| Deadlock con channel nil | Enviar/recibir de channel nil bloquea forever | Inicializar con `make(chan T)` |
| JSON tiene `"port": 0` no deseado | Campo int con zero value se serializa | Usar `omitempty` o puntero `*int` |
| JSON tiene `"items": null` en vez de `[]` | nil slice se serializa como null | Inicializar como `[]T{}` en vez de nil |
| `omitempty` omite `false` y `0` legítimos | `omitempty` considera zero value como "vacío" | Usar punteros `*bool`, `*int` con `omitempty` |
| Interface no es nil aunque el valor sí | Interface almacena (tipo, valor); tipo != nil | Comparar el valor concreto, no la interface; devolver `nil` directamente |
| `go vet`: copying mutex value | Copiar sync.Mutex después del primer uso | Usar puntero a struct con mutex, o no copiar |
| `time.Time` muestra "0001-01-01 00:00:00" | time.Time con zero value | Verificar con `t.IsZero()` |
| Config con defaults incorrectos | Zero value 0 confundido con "usar default" | Documentar que 0 = default, o usar punteros para opcional |
| Slice nil vs empty slice | Comportamiento diferente en JSON y == nil | Elegir conscientemente nil vs `[]T{}` según la API |

---

## 17. Ejercicios

### Ejercicio 1 — Explorar zero values

```
Crea un programa que declare variables de TODOS los tipos
sin inicializarlas y las imprima con %v y %#v:
- bool, int, int8, int64, uint, float32, float64
- string, byte, rune
- *int, []int, map[string]int, chan int, func(), error
- Un struct con campos mixtos
- Un array [3]int

Predicción: ¿cuál es el zero value de error? ¿Y de func()?
¿%#v de un string vacío muestra "" o nada?
```

### Ejercicio 2 — Zero value útil

```
Diseña un tipo HTTPClient que sea útil con su zero value:
- Si BaseURL es "", usar "http://localhost"
- Si Timeout es 0, usar 30 segundos
- Si Client es nil, usar http.DefaultClient
- Si Headers es nil, no enviar headers extra

Crea un método Get(path string) que funcione con:
  var c HTTPClient
  c.Get("/health")  // → GET http://localhost/health con timeout 30s

Predicción: ¿qué pasa si intentas añadir a c.Headers sin
inicializarlo? ¿Cómo lo manejas de forma nil-safe?
```

### Ejercicio 3 — nil slice vs empty slice

```
Crea un programa que demuestre la diferencia entre:
- var s []int (nil slice)
- s := []int{} (empty slice)
- s := make([]int, 0) (empty slice)

Para cada uno, imprime:
- len, cap
- s == nil
- JSON marshaled output
- Resultado de append(s, 1)
- Resultado de range

Predicción: ¿los tres se comportan igual con range y append?
¿En qué se diferencia la serialización JSON?
```

### Ejercicio 4 — Interface nil gotcha

```
Crea un programa que demuestre el problema de interface nil:

1. Crea una función que devuelve error:
   func mayFail(fail bool) error {
       var err *MyError = nil  // puntero nil
       if fail {
           err = &MyError{msg: "failed"}
       }
       return err  // ← ¿esto es nil?
   }

2. Llama con fail=false
3. Verifica: err == nil → ¿true o false?
4. Arregla la función para que devuelva nil correctamente

Predicción: ¿por qué return err NO devuelve nil aunque
*MyError sea nil? ¿Cómo lo arreglas?
```

### Ejercicio 5 — Config con defaults

```
Crea un MonitorConfig con estos campos y defaults:
- Interval (0 → 15s)
- Timeout (0 → 10s)
- Retries (0 → 3)
- AlertEmail ("" → ninguno)
- Verbose (false → no verbose)
- Thresholds map (nil → usar hardcoded)

1. Implementa WithDefaults()
2. Prueba con zero value completo
3. Prueba con overrides parciales
4. Serializa a JSON con omitempty — ¿se pierden valores?

Predicción: si Retries es 0 y quieres "0 reintentos" (no default),
¿cómo lo expresas con el patrón de zero value = default?
```

### Ejercicio 6 — JSON omitempty

```
Crea un struct Alert con campos:
- Name string
- Severity int (0=info, 1=warning, 2=critical)
- Silenced bool
- Tags []string

1. Serializa con omitempty: ¿se pierden Severity=0 y Silenced=false?
2. Cambia Severity y Silenced a punteros
3. Serializa de nuevo: ¿ahora se incluyen?
4. Deserializa JSON sin "silenced" → ¿nil o *false?

Predicción: ¿json.Unmarshal de {} en AlertFixed deja
Severity como nil o como *0?
```

### Ejercicio 7 — Nil-safe methods

```
Crea un tipo Logger con:
- receiver *Logger (puntero)
- métodos Info, Warn, Error que son no-op si Logger es nil
- método SetOutput que inicializa el logger

Prueba:
  var log *Logger
  log.Info("hello")     // no-op, no panic
  log.SetOutput(os.Stdout)
  log.Info("hello")     // ahora sí imprime

Predicción: ¿un método con pointer receiver se puede
llamar en un puntero nil? ¿Por qué funciona?
```

### Ejercicio 8 — Lazy initialization

```
Crea un struct ConnectionPool que:
- Tenga un map[string]*Connection internamente
- Se inicialice el map en la primera llamada a Get()
- Use sync.Once para thread-safety
- Funcione con zero value (var pool ConnectionPool)

Predicción: ¿sync.Once funciona con zero value?
¿Qué pasa si dos goroutines llaman Get() simultáneamente?
```

### Ejercicio 9 — time.Time zero value

```
Crea un struct CacheEntry con:
- Key string
- Value string
- CreatedAt time.Time
- ExpiresAt time.Time

Implementa:
- IsExpired() bool — si ExpiresAt.IsZero(), nunca expira
- Age() time.Duration — si CreatedAt.IsZero(), retorna 0
- String() — muestra "never" para tiempos zero

Predicción: ¿time.Time{} == time.Time{} es true?
¿Puedes comparar time.Time con == o necesitas .Equal()?
```

### Ejercicio 10 — Zero value audit

```
Lee el código fuente de un proyecto Go real
(por ejemplo, la CLI de tu elección en GitHub) y encuentra:
1. Un tipo que tiene zero value útil (funciona sin New)
2. Un tipo que requiere constructor (no funciona sin New)
3. Un lugar donde nil slice vs empty slice importa
4. Un uso de omitempty en structs JSON
5. Un nil check defensivo

Documenta cada hallazgo con el archivo y línea.

Predicción: ¿la mayoría de tipos en la stdlib tienen
zero values útiles? ¿Puedes encontrar uno que no?
```

---

## 18. Resumen

```
    ┌───────────────────────────────────────────────────────────┐
    │              Zero values en Go — Resumen                  │
    │                                                           │
    │  PRINCIPIO:                                               │
    │  Toda variable tiene un valor conocido desde su           │
    │  declaración. No hay basura, no hay undefined.            │
    │                                                           │
    │  ZERO VALUES:                                             │
    │  ┌─ bool    → false                                      │
    │  ├─ números → 0                                          │
    │  ├─ string  → ""                                         │
    │  ├─ pointer → nil                                        │
    │  ├─ slice   → nil (pero append, len, range funcionan)    │
    │  ├─ map     → nil (leer OK, ⚠️ escribir = panic)        │
    │  ├─ channel → nil (⚠️ send/recv = deadlock)              │
    │  ├─ func    → nil (⚠️ llamar = panic)                    │
    │  ├─ interface → nil                                      │
    │  ├─ struct  → cada campo a su zero value (recursivo)     │
    │  └─ array   → cada elemento a su zero value              │
    │                                                           │
    │  "MAKE THE ZERO VALUE USEFUL":                            │
    │  ┌─ bytes.Buffer → listo sin New                         │
    │  ├─ sync.Mutex   → listo sin New                         │
    │  ├─ strings.Builder → listo sin New                      │
    │  └─ Tus tipos: usar defaults cuando campo == zero        │
    │                                                           │
    │  GOTCHAS:                                                 │
    │  ┌─ map nil: leer OK, escribir PANIC                     │
    │  ├─ interface nil: (tipo, nil) ≠ (nil, nil)              │
    │  ├─ omitempty: omite false y 0 (usar *bool/*int)         │
    │  ├─ nil slice → JSON null; []T{} → JSON []               │
    │  └─ 0 como "no configurado" vs "valor legítimo cero"     │
    │                                                           │
    │  PARA SYSADMIN:                                           │
    │  ┌─ Config con defaults: if field == 0 { usar default }  │
    │  ├─ time.Time.IsZero() para "nunca inicializado"         │
    │  ├─ nil-safe methods con pointer receiver                │
    │  └─ JSON: elegir nil vs empty según semántica de la API  │
    └───────────────────────────────────────────────────────────┘
```
