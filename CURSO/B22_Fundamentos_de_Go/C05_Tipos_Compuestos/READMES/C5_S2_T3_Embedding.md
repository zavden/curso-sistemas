# Embedding — Composicion sobre Herencia, Promoted Methods y Embedding de Interfaces

## 1. Introduccion

Go no tiene herencia. En su lugar tiene **embedding** — un mecanismo de composicion donde un struct o interface incluye otro tipo como campo anonimo, y los metodos y campos del tipo embebido se **promueven** al tipo contenedor, como si fueran propios. Esto logra reutilizacion de codigo sin las jerarquias fragiles de la herencia clasica.

La filosofia es explicita: **composicion sobre herencia**. Un `HTTPServer` no "es un" `TCPServer` — un `HTTPServer` **tiene un** `TCPServer` embebido. Los metodos del `TCPServer` estan disponibles directamente en `HTTPServer`, pero `HTTPServer` puede sobreescribirlos y no existe relacion de subtipo. No hay polimorfismo de herencia — ese rol lo cumplen las interfaces.

En SysAdmin/DevOps, embedding aparece constantemente: un `MonitoredService` embebe un `Service` base y agrega metricas, un `AuthenticatedClient` embebe un `http.Client` y agrega headers de autenticacion, un `LoggingDB` embebe un `sql.DB` y agrega logging de queries.

```
┌──────────────────────────────────────────────────────────────┐
│              EMBEDDING EN GO                                 │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Herencia clasica (Java/C++):     Embedding Go:              │
│                                                              │
│  class Dog extends Animal {       type Dog struct {           │
│    // hereda de Animal              Animal  // embebido       │
│    // ES-UN Animal                  // TIENE-UN Animal       │
│    // subtipo polimorfico           // NO es subtipo          │
│  }                                  Breed string              │
│                                   }                           │
│                                                              │
│  Que hace embedding:                                         │
│  ├─ Promueve campos: dog.Name (en vez de dog.Animal.Name)    │
│  ├─ Promueve metodos: dog.Speak() (en vez de dog.Animal...)  │
│  ├─ Satisface interfaces del embebido automaticamente        │
│  ├─ Permite "override": metodo del outer type tiene priorid. │
│  └─ NO es herencia: no hay relacion de subtipo               │
│                                                              │
│  Tipos de embedding:                                         │
│  ├─ Struct en struct:     type S struct { Base }             │
│  ├─ *Struct en struct:    type S struct { *Base }            │
│  ├─ Interface en struct:  type S struct { io.Writer }        │
│  └─ Interface en interf.: type RW interface { Reader; ... }  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Embedding basico: struct en struct

### Sintaxis: campo anonimo

```go
// Un campo sin nombre explicito es un "anonymous field" o "embedded field"
type Base struct {
    ID   int
    Name string
}

type Server struct {
    Base           // Embedded — sin nombre de campo
    IP     string
    Port   int
}

// Equivalente semantico (pero con comportamiento diferente):
type ServerNamed struct {
    base   Base   // Campo nombrado — NO es embedding
    IP     string
    Port   int
}
```

### Promocion de campos

```go
type Base struct {
    ID   int
    Name string
}

type Server struct {
    Base
    IP   string
    Port int
}

s := Server{
    Base: Base{ID: 1, Name: "web01"},
    IP:   "10.0.1.1",
    Port: 80,
}

// Los campos de Base se promueven a Server:
fmt.Println(s.Name)      // "web01" — acceso directo (promoted)
fmt.Println(s.ID)        // 1       — acceso directo
fmt.Println(s.IP)        // "10.0.1.1"
fmt.Println(s.Port)      // 80

// El acceso largo tambien funciona:
fmt.Println(s.Base.Name) // "web01" — acceso explicito via el tipo
fmt.Println(s.Base.ID)   // 1

// Ambas formas son equivalentes — el compilador las traduce igual
```

### Promocion de metodos

```go
type Base struct {
    ID   int
    Name string
}

func (b Base) String() string {
    return fmt.Sprintf("[%d] %s", b.ID, b.Name)
}

func (b *Base) SetName(name string) {
    b.Name = name
}

type Server struct {
    Base
    IP   string
    Port int
}

s := Server{
    Base: Base{ID: 1, Name: "web01"},
    IP:   "10.0.1.1",
    Port: 80,
}

// Los metodos de Base se promueven a Server:
fmt.Println(s.String())   // "[1] web01" — promoted method
s.SetName("web02")        // promoted pointer receiver method
fmt.Println(s.Name)       // "web02"

// Acceso explicito tambien funciona:
fmt.Println(s.Base.String()) // "[1] web02"
```

### Inicializacion: no se puede mezclar con campos del embebido

```go
type Base struct {
    ID   int
    Name string
}

type Server struct {
    Base
    Port int
}

// ✓ Inicializar el embebido como un campo:
s1 := Server{
    Base: Base{ID: 1, Name: "web01"},
    Port: 80,
}

// ✗ NO puedes poner campos del embebido en el nivel superior:
// s2 := Server{
//     ID:   1,       // Error: unknown field ID
//     Name: "web01", // Error: unknown field Name
//     Port: 80,
// }
// La promocion funciona para ACCESO, pero no para INICIALIZACION

// ✓ Zero value y despues asignar campos:
var s3 Server
s3.ID = 1       // OK — acceso promovido
s3.Name = "web01" // OK
s3.Port = 80
```

---

## 3. Embedding de puntero: *T en struct

### Diferencia entre T y *T embebido

```go
type Logger struct {
    Prefix string
    Level  string
}

func (l *Logger) Log(msg string) {
    fmt.Printf("[%s] %s: %s\n", l.Level, l.Prefix, msg)
}

// Embedding por valor:
type ServiceV struct {
    Logger       // Copia del Logger — cada ServiceV tiene su propio Logger
    Name string
}

// Embedding por puntero:
type ServiceP struct {
    *Logger      // Puntero a Logger — puede ser nil, multiples pueden compartir
    Name string
}
```

### Embedding por valor: independencia

```go
type Logger struct {
    Prefix string
}

func (l *Logger) Log(msg string) {
    fmt.Printf("[%s] %s\n", l.Prefix, msg)
}

type Service struct {
    Logger
    Name string
}

s1 := Service{Logger: Logger{Prefix: "SVC1"}, Name: "api"}
s2 := s1 // Copia COMPLETA — Logger tambien se copia

s2.Prefix = "SVC2"
s1.Log("hello") // [SVC1] hello — independiente
s2.Log("hello") // [SVC2] hello
```

### Embedding por puntero: comparticion y nil

```go
type Logger struct {
    Prefix string
}

func (l *Logger) Log(msg string) {
    if l == nil {
        return // Nil-safe
    }
    fmt.Printf("[%s] %s\n", l.Prefix, msg)
}

type Service struct {
    *Logger // Puede ser nil
    Name    string
}

// Compartir un logger entre multiples servicios:
sharedLogger := &Logger{Prefix: "SHARED"}
s1 := Service{Logger: sharedLogger, Name: "api"}
s2 := Service{Logger: sharedLogger, Name: "web"}

sharedLogger.Prefix = "UPDATED"
s1.Log("test") // [UPDATED] test — ambos ven el cambio
s2.Log("test") // [UPDATED] test

// Peligro: nil pointer
s3 := Service{Name: "orphan"} // Logger es nil
// s3.Log("test") // Funciona solo si Log verifica nil
// s3.Prefix       // panic: nil pointer dereference
// → Acceder a CAMPOS del embebido nil causa panic
// → Llamar METODOS puede funcionar si el metodo verifica nil
```

### Cuando usar *T vs T

```
┌──────────────────────────────────┬──────────────────────────────────┐
│ Embedding por valor (T)          │ Embedding por puntero (*T)       │
├──────────────────────────────────┼──────────────────────────────────┤
│ Cada instancia tiene su copia    │ Multiples instancias comparten   │
│ Siempre inicializado (zero val.) │ Puede ser nil → panic en campos  │
│ Seguro para copiar               │ Copia comparte el puntero        │
│ No puede ser nil                 │ Util para lazy initialization    │
│ Mas memoria (copia el struct)    │ Menos memoria (solo 8 bytes ptr) │
│ Independencia entre instancias   │ Cambios en uno afectan a todos   │
│                                  │                                  │
│ Usar cuando:                     │ Usar cuando:                     │
│ - El embebido es pequeño         │ - El embebido es grande          │
│ - Cada instancia es independiente│ - Necesitas compartir estado     │
│ - Siempre necesitas el embebido  │ - El embebido es opcional        │
│ - Tipo contiene sync.Mutex       │ - Lazy init / late binding       │
│   → NUNCA copiar, usar puntero   │                                  │
└──────────────────────────────────┴──────────────────────────────────┘
```

---

## 4. Override: el outer type tiene prioridad

### Sobreescribir metodos promovidos

```go
type Base struct {
    Name string
}

func (b Base) Greet() string {
    return fmt.Sprintf("Hello from Base: %s", b.Name)
}

func (b Base) Type() string {
    return "base"
}

type Extended struct {
    Base
    Extra string
}

// "Override": Extended define su propio Greet
func (e Extended) Greet() string {
    return fmt.Sprintf("Hello from Extended: %s (%s)", e.Name, e.Extra)
}

e := Extended{
    Base:  Base{Name: "test"},
    Extra: "extended data",
}

fmt.Println(e.Greet())      // "Hello from Extended: test (extended data)"
fmt.Println(e.Type())       // "base" — promovido (no sobreescrito)
fmt.Println(e.Base.Greet()) // "Hello from Base: test" — acceso directo al "padre"
```

### Reglas de resolucion de nombres

```go
// Go busca nombres en este orden:
// 1. Campos/metodos directos del tipo
// 2. Campos/metodos promovidos de embebidos (un nivel)
// 3. Campos/metodos promovidos de embebidos de embebidos (dos niveles)
// ... y asi sucesivamente

// Si hay ambiguedad al MISMO nivel → error de compilacion

type A struct{ Val int }
type B struct{ Val int }

type C struct {
    A
    B
}

c := C{A: A{Val: 1}, B: B{Val: 2}}

// fmt.Println(c.Val) // ✗ ERROR: ambiguous selector c.Val
// Ambos A y B tienen Val al mismo nivel de profundidad

// ✓ Desambiguar explicitamente:
fmt.Println(c.A.Val) // 1
fmt.Println(c.B.Val) // 2

// ✓ O agregar un campo propio (tiene prioridad):
type D struct {
    A
    B
    Val int // Campo directo — resuelve la ambiguedad
}

d := D{A: A{Val: 1}, B: B{Val: 2}, Val: 99}
fmt.Println(d.Val)   // 99 — campo directo tiene prioridad
fmt.Println(d.A.Val) // 1  — acceso explicito
fmt.Println(d.B.Val) // 2  — acceso explicito
```

### La profundidad resuelve ambiguedad

```go
type Inner struct {
    X int
}

type Middle struct {
    Inner
    Y int
}

type Outer struct {
    Middle
    X int // Campo directo X
}

o := Outer{
    Middle: Middle{Inner: Inner{X: 1}, Y: 2},
    X:      3,
}

fmt.Println(o.X)              // 3 — campo directo (profundidad 0)
fmt.Println(o.Middle.X)       // 1 — promovido de Inner via Middle (profundidad 2)
fmt.Println(o.Middle.Inner.X) // 1 — acceso explicito completo
fmt.Println(o.Y)              // 2 — promovido de Middle (profundidad 1)

// No hay ambiguedad porque o.X (profundidad 0) y Middle.Inner.X (profundidad 2)
// estan a diferentes niveles — el mas cercano gana
```

---

## 5. Embedding e interfaces

### Promoted methods satisfacen interfaces

```go
type Stringer interface {
    String() string
}

type Base struct {
    Name string
}

func (b Base) String() string {
    return b.Name
}

type Server struct {
    Base
    Port int
}

// Server satisface Stringer automaticamente
// porque Base tiene String() y esa se promueve a Server
var s Stringer = Server{Base: Base{Name: "web01"}, Port: 80}
fmt.Println(s.String()) // "web01"

// Esto es extremadamente poderoso:
// Si embedes un tipo que implementa una interfaz,
// tu tipo tambien la implementa automaticamente
```

### Ejemplo con io.Writer

```go
type CountingWriter struct {
    io.Writer           // Embebe la interfaz io.Writer
    BytesWritten int64
}

func (cw *CountingWriter) Write(p []byte) (int, error) {
    n, err := cw.Writer.Write(p) // Delegar al Writer embebido
    cw.BytesWritten += int64(n)
    return n, err
}

// Uso:
file, _ := os.Create("output.log")
cw := &CountingWriter{Writer: file}

fmt.Fprintf(cw, "Hello, World!\n")
fmt.Printf("Bytes written: %d\n", cw.BytesWritten) // 14

// CountingWriter satisface io.Writer gracias a su metodo Write
// (que sobreescribe el promovido del embebido)
```

### El method set y la satisfaccion de interfaces

```go
// Recordatorio del method set (de T03 Metodos):
// T  tiene metodos con receiver T
// *T tiene metodos con receiver T y *T

type Base struct{ Name string }
func (b Base) Get() string      { return b.Name }
func (b *Base) Set(name string) { b.Name = name }

type Server struct {
    Base
    Port int
}

type Getter interface { Get() string }
type Setter interface { Set(string) }
type GetSetter interface { Get() string; Set(string) }

var _ Getter    = Server{}   // ✓ Get() tiene value receiver → promovido a Server
var _ Setter    = &Server{}  // ✓ Set() tiene pointer receiver → promovido a *Server
var _ GetSetter = &Server{}  // ✓ *Server tiene ambos
// var _ Setter = Server{}   // ✗ Server (no puntero) NO tiene Set()

// Regla de embedding y method sets:
// Si embedes T:  el outer type obtiene los metodos de T (value receiver)
// Si embedes *T: el outer type obtiene los metodos de T y *T (ambos receivers)
```

---

## 6. Embedding de interfaces en structs

### El patron del "wrapper" o "decorator"

```go
// Cuando embedes una INTERFAZ (no un struct), el campo anonimo
// es de tipo interfaz — debe asignarse con un valor concreto

type Logger interface {
    Log(level, msg string)
}

type Service struct {
    Logger  // Embebe la interfaz, no un struct concreto
    Name string
}

// Service ahora satisface Logger (tiene Log promovido)
// Pero Logger es nil hasta que lo asignes:

type ConsoleLogger struct{}
func (ConsoleLogger) Log(level, msg string) {
    fmt.Printf("[%s] %s\n", level, msg)
}

svc := Service{
    Logger: ConsoleLogger{}, // Asignar implementacion concreta
    Name:   "api",
}
svc.Log("INFO", "started") // [INFO] started

// Si Logger es nil:
svc2 := Service{Name: "orphan"}
// svc2.Log("INFO", "test") // panic: nil pointer dereference
// El metodo promovido llama a un nil interface
```

### Partial interface implementation

```go
// Embedding de interfaz permite implementar solo algunos metodos
// y delegar el resto al embebido

type ReadWriteCloser interface {
    Read(p []byte) (int, error)
    Write(p []byte) (int, error)
    Close() error
}

type LoggingReadWriter struct {
    ReadWriteCloser // Embebe toda la interfaz
    label string
}

// Solo "override" Write — Read y Close se delegan al embebido
func (lrw *LoggingReadWriter) Write(p []byte) (int, error) {
    fmt.Printf("[%s] writing %d bytes\n", lrw.label, len(p))
    return lrw.ReadWriteCloser.Write(p) // Delegar
}

// Uso:
file, _ := os.Create("test.log")
lrw := &LoggingReadWriter{
    ReadWriteCloser: file, // os.File implementa ReadWriteCloser
    label:           "disk",
}
lrw.Write([]byte("hello")) // [disk] writing 5 bytes + escribe al archivo
lrw.Close()                 // Delegado a file.Close()
```

### Embedding de interfaz en interfaz

```go
// Las interfaces pueden embeber otras interfaces
// Esto es COMPOSICION de interfaces (no herencia)

type Reader interface {
    Read(p []byte) (int, error)
}

type Writer interface {
    Write(p []byte) (int, error)
}

type Closer interface {
    Close() error
}

// Componer interfaces:
type ReadWriter interface {
    Reader
    Writer
}

type ReadWriteCloser interface {
    Reader
    Writer
    Closer
}

// Equivalente a escribir todos los metodos:
type ReadWriteCloser interface {
    Read(p []byte) (int, error)
    Write(p []byte) (int, error)
    Close() error
}

// La stdlib de Go usa esto extensivamente:
// io.Reader, io.Writer, io.Closer
// io.ReadWriter = io.Reader + io.Writer
// io.ReadCloser = io.Reader + io.Closer
// io.WriteCloser = io.Writer + io.Closer
// io.ReadWriteCloser = io.Reader + io.Writer + io.Closer
```

---

## 7. Embedding multiple: composicion real

### Combinar capacidades

```go
type Identified struct {
    ID        string
    CreatedAt time.Time
}

func (i Identified) Age() time.Duration {
    return time.Since(i.CreatedAt)
}

type Named struct {
    Name        string
    DisplayName string
}

func (n Named) Label() string {
    if n.DisplayName != "" {
        return n.DisplayName
    }
    return n.Name
}

type Tagged struct {
    Tags map[string]string
}

func (t Tagged) HasTag(key string) bool {
    _, ok := t.Tags[key]
    return ok
}

func (t Tagged) GetTag(key string) string {
    return t.Tags[key]
}

// Componer multiples capacidades:
type Server struct {
    Identified
    Named
    Tagged
    IP   string
    Port int
}

s := Server{
    Identified: Identified{ID: "srv-001", CreatedAt: time.Now().Add(-24 * time.Hour)},
    Named:      Named{Name: "web01", DisplayName: "Web Server #1"},
    Tagged:     Tagged{Tags: map[string]string{"env": "prod", "region": "us-east"}},
    IP:         "10.0.1.1",
    Port:       80,
}

// Todos los metodos promovidos:
fmt.Println(s.ID)              // "srv-001"     — de Identified
fmt.Println(s.Age())           // ~24h          — de Identified
fmt.Println(s.Label())         // "Web Server #1" — de Named
fmt.Println(s.HasTag("env"))   // true          — de Tagged
fmt.Println(s.GetTag("region"))// "us-east"     — de Tagged
fmt.Println(s.IP)              // "10.0.1.1"    — propio
```

### Composicion para SysAdmin: monitored + authenticated + logged

```go
// Patron real: un HTTP client con metricas, auth, y logging
type BaseClient struct {
    httpClient *http.Client
    baseURL    string
}

func (c *BaseClient) Do(req *http.Request) (*http.Response, error) {
    req.URL.Host = c.baseURL
    return c.httpClient.Do(req)
}

type AuthMixin struct {
    token string
}

func (a *AuthMixin) AddAuth(req *http.Request) {
    req.Header.Set("Authorization", "Bearer "+a.token)
}

type MetricsMixin struct {
    requestCount int
    totalLatency time.Duration
}

func (m *MetricsMixin) RecordRequest(duration time.Duration) {
    m.requestCount++
    m.totalLatency += duration
}

func (m *MetricsMixin) AvgLatency() time.Duration {
    if m.requestCount == 0 {
        return 0
    }
    return m.totalLatency / time.Duration(m.requestCount)
}

type LogMixin struct {
    logger *log.Logger
}

func (l *LogMixin) LogRequest(method, url string, status int, dur time.Duration) {
    l.logger.Printf("%s %s → %d (%s)", method, url, status, dur)
}

// Componer todo:
type InfraClient struct {
    BaseClient
    AuthMixin
    MetricsMixin
    LogMixin
}

func NewInfraClient(baseURL, token string) *InfraClient {
    return &InfraClient{
        BaseClient:   BaseClient{httpClient: &http.Client{}, baseURL: baseURL},
        AuthMixin:    AuthMixin{token: token},
        MetricsMixin: MetricsMixin{},
        LogMixin:     LogMixin{logger: log.Default()},
    }
}

// InfraClient tiene TODOS los metodos:
// client.Do(req)                  — de BaseClient
// client.AddAuth(req)             — de AuthMixin
// client.RecordRequest(dur)       — de MetricsMixin
// client.AvgLatency()             — de MetricsMixin
// client.LogRequest(...)          — de LogMixin
```

---

## 8. Embedding y el anti-pattern de "herencia"

### Embedding NO es herencia

```go
type Animal struct {
    Name string
}

func (a Animal) Speak() string {
    return a.Name + " makes a sound"
}

type Dog struct {
    Animal
    Breed string
}

// Dog NO es un subtipo de Animal
// No puedes usar Dog donde se espera Animal:
func feedAnimal(a Animal) {
    fmt.Println("Feeding", a.Name)
}

d := Dog{Animal: Animal{Name: "Rex"}, Breed: "Labrador"}

// feedAnimal(d) // ✗ ERROR: cannot use d (type Dog) as type Animal
feedAnimal(d.Animal) // ✓ Pasar el embebido explicitamente

// Para polimorfismo, usa INTERFACES:
type Speaker interface {
    Speak() string
}

func makeNoise(s Speaker) {
    fmt.Println(s.Speak())
}

makeNoise(d) // ✓ Dog satisface Speaker via el metodo promovido
```

### Anti-pattern: cadena de herencia simulada

```go
// ✗ NO hagas esto — simular jerarquia de herencia:
type Entity struct { ID int }
type LivingEntity struct { Entity; Health int }
type Character struct { LivingEntity; Name string }
type Player struct { Character; Score int }
type AdminPlayer struct { Player; Permissions []string }

// 5 niveles de "herencia" — fragil, confuso, dificil de mantener
// Los campos se promueven a traves de muchos niveles:
admin := AdminPlayer{...}
admin.ID      // Funciona pero ¿de donde viene? Hay que rastrear 4 niveles
admin.Health  // ¿Es de AdminPlayer o de algún embebido?

// ✓ Mejor: composicion plana
type AdminPlayer struct {
    ID          int
    Name        string
    Health      int
    Score       int
    Permissions []string
}
// Claro, directo, sin sorpresas

// ✓ O composicion con interfaces si necesitas polimorfismo:
type Identified interface { GetID() int }
type Named interface { GetName() string }
type Scored interface { GetScore() int }
```

### Cuando SI usar embedding

```
┌──────────────────────────────────────────────────────────────┐
│  ¿CUANDO USAR EMBEDDING?                                    │
│                                                              │
│  ✓ Usar cuando:                                              │
│  ├─ Quieres promover metodos para satisfacer una interfaz    │
│  ├─ El wrapper agrega funcionalidad (decorator pattern)      │
│  ├─ Composicion de capacidades ortogonales (mixins)          │
│  ├─ El tipo embebido es parte integral del outer type        │
│  └─ Maximo 2-3 niveles de profundidad                        │
│                                                              │
│  ✗ NO usar cuando:                                           │
│  ├─ Solo quieres reusar campos (usa campo nombrado)          │
│  ├─ Simulas herencia profunda (> 2 niveles)                  │
│  ├─ El outer type no deberia exponer TODOS los metodos       │
│  │   del embebido (viola encapsulacion)                      │
│  ├─ Los metodos promovidos confunden la API                  │
│  └─ Solo necesitas un subset del tipo embebido               │
│                                                              │
│  Regla: si al embeber T, todos los metodos de T tienen       │
│  sentido en el outer type → embedding OK.                    │
│  Si solo algunos → usa campo nombrado + delegacion manual.   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Ejemplo del problema de exponer demasiado

```go
// ✗ Embedding expone metodos que no deberian ser publicos:
type UserStore struct {
    sync.Mutex               // Promueve Lock() y Unlock() al API publica
    users map[string]string
}

// Ahora cualquiera puede hacer:
store := &UserStore{users: make(map[string]string)}
store.Lock()   // ¿Es esto parte del API de UserStore? No deberia
store.Unlock() // El lock es un detalle de implementacion

// ✓ Solucion: campo nombrado (no embedding)
type UserStore struct {
    mu    sync.Mutex         // Campo privado — Lock/Unlock no se promueven
    users map[string]string
}

func (s *UserStore) Set(name, email string) {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.users[name] = email
}

func (s *UserStore) Get(name string) (string, bool) {
    s.mu.Lock()
    defer s.mu.Unlock()
    email, ok := s.users[name]
    return email, ok
}
// Lock/Unlock son detalles internos — no expuestos
```

---

## 9. Embedding y JSON

### Tags en campos embebidos

```go
// Los campos promovidos se serializan como si fueran del outer type
type Metadata struct {
    ID        string    `json:"id"`
    CreatedAt time.Time `json:"created_at"`
}

type Server struct {
    Metadata           // Embebido — campos promovidos en JSON
    Name     string    `json:"name"`
    Port     int       `json:"port"`
}

s := Server{
    Metadata: Metadata{ID: "srv-001", CreatedAt: time.Now()},
    Name:     "web01",
    Port:     80,
}

data, _ := json.MarshalIndent(s, "", "  ")
fmt.Println(string(data))
// {
//   "id": "srv-001",
//   "created_at": "2026-04-03T...",
//   "name": "web01",
//   "port": 80
// }
// Los campos de Metadata aparecen al mismo nivel — "aplanados"
```

### Colision de campos JSON

```go
type Base struct {
    Name string `json:"name"`
}

type Extended struct {
    Base
    Name string `json:"name"` // Campo directo con mismo JSON name
}

e := Extended{
    Base: Base{Name: "base-name"},
    Name: "extended-name",
}

data, _ := json.Marshal(e)
fmt.Println(string(data))
// {"name":"extended-name"}
// El campo directo tiene prioridad sobre el promovido
// (misma regla que para acceso a campos)
```

### Embedding y omitempty

```go
// Un struct embebido NO se omite con omitempty
// (porque los structs nunca son "empty" para encoding/json)

type Optional struct {
    Extra string `json:"extra,omitempty"`
}

type Main struct {
    Optional         // Los campos de Optional se aplanan
    Name     string `json:"name"`
}

m := Main{Name: "test"} // Extra es ""
data, _ := json.Marshal(m)
fmt.Println(string(data))
// {"name":"test"}
// Extra se omite porque tiene omitempty y es ""
// Funciona porque los campos se aplanan, no el struct
```

---

## 10. Patrones de embedding en la stdlib de Go

### http.Handler wrapping

```go
// La stdlib usa embedding extensivamente para wrappers HTTP

// Patron: middleware como struct con embedding
type LoggingHandler struct {
    handler http.Handler // Campo nombrado (no embedding) — correcto aqui
    logger  *log.Logger
}

func (lh *LoggingHandler) ServeHTTP(w http.ResponseWriter, r *http.Request) {
    start := time.Now()
    lh.handler.ServeHTTP(w, r)
    lh.logger.Printf("%s %s %s", r.Method, r.URL.Path, time.Since(start))
}

// Patron con embedding de ResponseWriter para capturar status:
type StatusRecorder struct {
    http.ResponseWriter // Embedding — promueve Header(), Write(), etc.
    StatusCode int
}

func (sr *StatusRecorder) WriteHeader(code int) {
    sr.StatusCode = code
    sr.ResponseWriter.WriteHeader(code) // Delegar al original
}

// Uso en middleware:
func loggingMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        rec := &StatusRecorder{ResponseWriter: w, StatusCode: 200}
        next.ServeHTTP(rec, r)
        log.Printf("%s %s → %d", r.Method, r.URL.Path, rec.StatusCode)
    })
}
```

### io composition en stdlib

```go
// La stdlib compone interfaces con embedding:

// io.ReadWriter = io.Reader + io.Writer
type ReadWriter interface {
    Reader
    Writer
}

// bufio.ReadWriter embebe *bufio.Reader y *bufio.Writer
type ReadWriter struct {
    *Reader // *bufio.Reader
    *Writer // *bufio.Writer
}

// Un solo objeto que lee y escribe buffered
// Los metodos de Reader y Writer se promueven automaticamente

// Uso:
r := bufio.NewReader(os.Stdin)
w := bufio.NewWriter(os.Stdout)
rw := bufio.NewReadWriter(r, w)
// rw.ReadString('\n') — de *bufio.Reader
// rw.WriteString("hello") — de *bufio.Writer
// rw.Flush() — de *bufio.Writer
```

### testing.TB embedding

```go
// testing.T embebe testing.TB (interfaz comun con testing.B)

// Patron en tests: helper struct que embebe *testing.T
type testHelper struct {
    *testing.T
    tmpDir string
}

func newTestHelper(t *testing.T) *testHelper {
    t.Helper()
    dir, err := os.MkdirTemp("", "test-*")
    if err != nil {
        t.Fatal(err)
    }
    th := &testHelper{T: t, tmpDir: dir}
    t.Cleanup(func() { os.RemoveAll(dir) })
    return th
}

func (h *testHelper) WriteFile(name, content string) string {
    h.Helper()
    path := filepath.Join(h.tmpDir, name)
    if err := os.WriteFile(path, []byte(content), 0644); err != nil {
        h.Fatal(err) // Promovido de *testing.T
    }
    return path
}

// Uso en test:
func TestSomething(t *testing.T) {
    h := newTestHelper(t)
    path := h.WriteFile("config.json", `{"port": 8080}`)
    // h.Fatal, h.Error, h.Log, etc. — todos promovidos de *testing.T
}
```

---

## 11. Comparacion: Go vs C vs Rust

### Go: embedding como composicion implicita

```go
// Go: campos anonimos + promocion automatica
type Base struct{ Name string }
func (b Base) Hello() string { return "Hello " + b.Name }

type Derived struct {
    Base        // Embedding
    Extra string
}

d := Derived{Base: Base{Name: "Go"}, Extra: "!"}
d.Hello()     // Promovido
d.Base.Hello() // Explicito
// No es subtipo — no hay polimorfismo de herencia
```

### C: composicion manual, sin promocion

```c
// C: composicion explicita, acceso manual
typedef struct {
    char name[64];
} Base;

const char* base_hello(const Base *b) {
    static char buf[128];
    snprintf(buf, sizeof(buf), "Hello %s", b->name);
    return buf;
}

typedef struct {
    Base base;       // Composicion — siempre campo nombrado
    char extra[64];
} Derived;

// Acceso: SIEMPRE via el campo
Derived d = {.base = {.name = "C"}, .extra = "!"};
base_hello(&d.base);  // No hay promocion — siempre d.base.xxx
// d.name no funciona — C no promueve campos

// Algunos usan cast trick (primer campo = base):
// Base *b = (Base *)&d;  // Funciona si base es el primer campo
// Fragil, no type-safe, pero comun en C (ejemplo: GObject, Linux kernel)
```

### Rust: traits + composition, sin herencia

```rust
// Rust: sin herencia, sin embedding automatico
// Composicion explicita + traits para polimorfismo

struct Base {
    name: String,
}

impl Base {
    fn hello(&self) -> String {
        format!("Hello {}", self.name)
    }
}

struct Derived {
    base: Base,     // Composicion — siempre campo nombrado
    extra: String,
}

// No hay promocion automatica
// Para acceder a metodos de Base: d.base.hello()

// Para polimorfismo, usa traits:
trait Greeter {
    fn hello(&self) -> String;
}

impl Greeter for Base {
    fn hello(&self) -> String {
        format!("Hello {}", self.name)
    }
}

impl Greeter for Derived {
    fn hello(&self) -> String {
        // Delegar explicitamente:
        self.base.hello()
    }
}

// Rust Deref trait: similar a embedding pero mas controlado
use std::ops::Deref;

impl Deref for Derived {
    type Target = Base;
    fn deref(&self) -> &Base {
        &self.base
    }
}
// Ahora d.hello() funciona via Deref coercion
// Pero esto es controversional — "Deref polymorphism" anti-pattern
// Solo recomendado para smart pointers (Box, Arc, Rc)
```

### Tabla comparativa

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Go                   │ C                    │ Rust                 │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Herencia             │ No                   │ No                   │ No                   │
│ Composicion          │ Embedding (auto)     │ Campo nombrado       │ Campo nombrado       │
│ Promocion de campos  │ Si (automatica)      │ No                   │ No (Deref = hack)    │
│ Promocion de metodos │ Si (automatica)      │ No (funciones libres)│ No (traits explicit) │
│ Override             │ Outer type gana      │ N/A                  │ impl por tipo        │
│ Polimorfismo         │ Interfaces (implicito│ void*/cast (manual)  │ Traits (explicito)   │
│                      │ duck typing)         │                      │                      │
│ Multiple embedding   │ Si (con ambiguedad)  │ Si (campos nombrados)│ Si (campos nombrados)│
│ Interface embedding  │ Si (composicion)     │ N/A                  │ Trait bounds         │
│ Subtipo              │ No                   │ No                   │ No                   │
│ Acceso a "padre"     │ outer.Embedded.Meth()│ d.base.func()        │ self.base.method()   │
│ JSON flatten         │ Si (automatico)      │ N/A                  │ #[serde(flatten)]    │
│ Peligro              │ Exponer demasiado    │ Cast incorrecto      │ Deref abuse          │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 12. Programa completo: Composable Infrastructure Agent

Un agente de infraestructura construido mediante composicion con embedding. Cada "mixin" agrega una capacidad (identificacion, networking, health checking, metricas, logging) y el tipo final las compone todas.

```go
package main

import (
	"encoding/json"
	"fmt"
	"math/rand"
	"slices"
	"strings"
	"sync"
	"time"
)

// ─── Capability 1: Identification ───────────────────────────────

type Identity struct {
	ID        string    `json:"id"`
	Name      string    `json:"name"`
	Role      string    `json:"role"`
	CreatedAt time.Time `json:"created_at"`
}

func NewIdentity(name, role string) Identity {
	return Identity{
		ID:        fmt.Sprintf("%s-%04d", role, rand.Intn(10000)),
		Name:      name,
		Role:      role,
		CreatedAt: time.Now(),
	}
}

func (i Identity) Uptime() time.Duration {
	return time.Since(i.CreatedAt)
}

func (i Identity) String() string {
	return fmt.Sprintf("%s (%s)", i.Name, i.ID)
}

// ─── Capability 2: Networking ───────────────────────────────────

type Network struct {
	Host     string `json:"host"`
	Port     int    `json:"port"`
	Protocol string `json:"protocol"`
}

func NewNetwork(host string, port int) Network {
	proto := "http"
	if port == 443 || port == 8443 {
		proto = "https"
	}
	return Network{Host: host, Port: port, Protocol: proto}
}

func (n Network) Addr() string {
	return fmt.Sprintf("%s:%d", n.Host, n.Port)
}

func (n Network) URL() string {
	return fmt.Sprintf("%s://%s:%d", n.Protocol, n.Host, n.Port)
}

// ─── Capability 3: Health Checking ──────────────────────────────

type HealthStatus string

const (
	HealthOK       HealthStatus = "healthy"
	HealthDegraded HealthStatus = "degraded"
	HealthDown     HealthStatus = "down"
	HealthUnknown  HealthStatus = "unknown"
)

type HealthChecker struct {
	mu          sync.RWMutex
	status      HealthStatus
	lastCheck   time.Time
	checkCount  int
	failCount   int
	consecutiveOK   int
	consecutiveFail int
}

func NewHealthChecker() HealthChecker {
	return HealthChecker{status: HealthUnknown}
}

func (h *HealthChecker) RecordCheck(ok bool) {
	h.mu.Lock()
	defer h.mu.Unlock()

	h.checkCount++
	h.lastCheck = time.Now()

	if ok {
		h.consecutiveOK++
		h.consecutiveFail = 0
		if h.consecutiveOK >= 3 {
			h.status = HealthOK
		} else if h.status == HealthDown {
			h.status = HealthDegraded
		}
	} else {
		h.failCount++
		h.consecutiveFail++
		h.consecutiveOK = 0
		if h.consecutiveFail >= 3 {
			h.status = HealthDown
		} else {
			h.status = HealthDegraded
		}
	}
}

func (h *HealthChecker) Status() HealthStatus {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return h.status
}

func (h *HealthChecker) HealthSummary() string {
	h.mu.RLock()
	defer h.mu.RUnlock()
	return fmt.Sprintf("status=%s checks=%d fails=%d last=%s",
		h.status, h.checkCount, h.failCount,
		h.lastCheck.Format("15:04:05"))
}

// ─── Capability 4: Metrics ──────────────────────────────────────

type Metrics struct {
	mu      sync.RWMutex
	gauges  map[string]float64
	counters map[string]int64
}

func NewMetrics() Metrics {
	return Metrics{
		gauges:   make(map[string]float64),
		counters: make(map[string]int64),
	}
}

func (m *Metrics) SetGauge(name string, value float64) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.gauges[name] = value
}

func (m *Metrics) GetGauge(name string) float64 {
	m.mu.RLock()
	defer m.mu.RUnlock()
	return m.gauges[name]
}

func (m *Metrics) IncrCounter(name string) {
	m.mu.Lock()
	defer m.mu.Unlock()
	m.counters[name]++
}

func (m *Metrics) Snapshot() map[string]any {
	m.mu.RLock()
	defer m.mu.RUnlock()

	snap := make(map[string]any)
	for k, v := range m.gauges {
		snap["gauge."+k] = v
	}
	for k, v := range m.counters {
		snap["counter."+k] = v
	}
	return snap
}

// ─── Capability 5: Labels ──────────────────────────────────────

type Labeled struct {
	Labels map[string]string `json:"labels,omitempty"`
}

func NewLabeled(labels map[string]string) Labeled {
	l := Labeled{Labels: make(map[string]string)}
	for k, v := range labels {
		l.Labels[k] = v
	}
	return l
}

func (l Labeled) HasLabel(key string) bool {
	_, ok := l.Labels[key]
	return ok
}

func (l Labeled) GetLabel(key string) string {
	return l.Labels[key]
}

func (l Labeled) MatchLabels(selector map[string]string) bool {
	for k, v := range selector {
		if l.Labels[k] != v {
			return false
		}
	}
	return true
}

// ─── Agent: composicion de todas las capacidades ────────────────

type Agent struct {
	Identity                  // ¿Quien soy?
	Network                   // ¿Donde estoy?
	HealthChecker             // ¿Como estoy?
	Metrics                   // ¿Que mido?
	Labeled                   // ¿Como me clasifican?
}

func NewAgent(name, role, host string, port int, labels map[string]string) *Agent {
	return &Agent{
		Identity:      NewIdentity(name, role),
		Network:       NewNetwork(host, port),
		HealthChecker: NewHealthChecker(),
		Metrics:       NewMetrics(),
		Labeled:       NewLabeled(labels),
	}
}

// Override: Agent tiene su propio String() que combina todo
func (a *Agent) String() string {
	return fmt.Sprintf("%s @ %s [%s]",
		a.Identity.String(), // Llamar explicitamente al embebido
		a.URL(),
		a.Status())
}

// Metodo propio: reporte completo
func (a *Agent) Report() string {
	var b strings.Builder
	fmt.Fprintf(&b, "Agent Report: %s\n", a.String())
	fmt.Fprintf(&b, "  ID:       %s\n", a.ID)
	fmt.Fprintf(&b, "  Role:     %s\n", a.Role)
	fmt.Fprintf(&b, "  Address:  %s\n", a.URL())
	fmt.Fprintf(&b, "  Health:   %s\n", a.HealthSummary())
	fmt.Fprintf(&b, "  Uptime:   %s\n", a.Uptime().Round(time.Second))

	// Labels
	if len(a.Labels) > 0 {
		labelParts := make([]string, 0, len(a.Labels))
		for k, v := range a.Labels {
			labelParts = append(labelParts, k+"="+v)
		}
		slices.Sort(labelParts)
		fmt.Fprintf(&b, "  Labels:   %s\n", strings.Join(labelParts, ", "))
	}

	// Metrics
	snap := a.Snapshot()
	if len(snap) > 0 {
		fmt.Fprintf(&b, "  Metrics:\n")
		metricKeys := make([]string, 0, len(snap))
		for k := range snap {
			metricKeys = append(metricKeys, k)
		}
		slices.Sort(metricKeys)
		for _, k := range metricKeys {
			fmt.Fprintf(&b, "    %-30s %v\n", k, snap[k])
		}
	}

	return b.String()
}

// ─── Fleet: coleccion de agents ─────────────────────────────────

type Fleet struct {
	agents []*Agent
}

func NewFleet(agents ...*Agent) *Fleet {
	return &Fleet{agents: agents}
}

func (f *Fleet) ByRole(role string) []*Agent {
	var result []*Agent
	for _, a := range f.agents {
		if a.Role == role {
			result = append(result, a)
		}
	}
	return result
}

func (f *Fleet) ByLabels(selector map[string]string) []*Agent {
	var result []*Agent
	for _, a := range f.agents {
		if a.MatchLabels(selector) {
			result = append(result, a)
		}
	}
	return result
}

func (f *Fleet) Healthy() []*Agent {
	var result []*Agent
	for _, a := range f.agents {
		if a.Status() == HealthOK {
			result = append(result, a)
		}
	}
	return result
}

func (f *Fleet) StatusSummary() map[HealthStatus]int {
	summary := make(map[HealthStatus]int)
	for _, a := range f.agents {
		summary[a.Status()]++
	}
	return summary
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	fmt.Println(strings.Repeat("═", 70))
	fmt.Println("  COMPOSABLE INFRASTRUCTURE AGENT")
	fmt.Println(strings.Repeat("═", 70))

	// Crear agentes con composicion
	agents := []*Agent{
		NewAgent("web-frontend", "web", "10.0.1.1", 443,
			map[string]string{"env": "prod", "region": "us-east", "tier": "frontend"}),
		NewAgent("api-server", "api", "10.0.2.1", 8443,
			map[string]string{"env": "prod", "region": "us-east", "tier": "backend"}),
		NewAgent("api-server-2", "api", "10.0.2.2", 8443,
			map[string]string{"env": "prod", "region": "us-west", "tier": "backend"}),
		NewAgent("postgres-primary", "database", "10.0.3.1", 5432,
			map[string]string{"env": "prod", "region": "us-east", "role": "primary"}),
		NewAgent("redis-cache", "cache", "10.0.4.1", 6379,
			map[string]string{"env": "prod", "region": "us-east"}),
	}

	fleet := NewFleet(agents...)

	// Simular health checks
	fmt.Println("\n── Simulating Health Checks ──\n")
	for _, agent := range agents {
		// Simular 5 checks exitosos
		for i := 0; i < 5; i++ {
			success := rand.Float64() > 0.1 // 90% success rate
			agent.RecordCheck(success)
		}
		// Simular metricas
		agent.SetGauge("cpu_percent", rand.Float64()*100)
		agent.SetGauge("memory_percent", rand.Float64()*100)
		agent.IncrCounter("requests_total")
		agent.IncrCounter("requests_total")
	}

	// Hacer que uno falle
	for i := 0; i < 5; i++ {
		agents[4].RecordCheck(false) // redis falla
	}

	// Reportes individuales
	fmt.Println("── Agent Reports ──")
	for _, agent := range agents {
		fmt.Println(agent.Report())
	}

	// Metodos promovidos en accion
	fmt.Println("── Promoted Methods Demo ──\n")
	a := agents[0]
	fmt.Printf("  Identity.String():     %s\n", a.Identity.String())
	fmt.Printf("  Agent.String():        %s\n", a.String()) // Override
	fmt.Printf("  Network.URL():         %s\n", a.URL())    // Promovido
	fmt.Printf("  Network.Addr():        %s\n", a.Addr())   // Promovido
	fmt.Printf("  HealthChecker.Status(): %s\n", a.Status()) // Promovido
	fmt.Printf("  Labeled.HasLabel(env):  %v\n", a.HasLabel("env")) // Promovido
	fmt.Printf("  Metrics.GetGauge(cpu):  %.1f\n", a.GetGauge("cpu_percent")) // Promovido

	// Fleet operations
	fmt.Println("\n── Fleet Operations ──\n")

	// Por role
	apiAgents := fleet.ByRole("api")
	fmt.Printf("  API agents: %d\n", len(apiAgents))
	for _, a := range apiAgents {
		fmt.Printf("    %s\n", a)
	}

	// Por labels
	usEast := fleet.ByLabels(map[string]string{"region": "us-east"})
	fmt.Printf("\n  US-East agents: %d\n", len(usEast))
	for _, a := range usEast {
		fmt.Printf("    %s\n", a)
	}

	// Healthy
	healthy := fleet.Healthy()
	fmt.Printf("\n  Healthy agents: %d / %d\n", len(healthy), len(agents))

	// Status summary
	fmt.Printf("\n  Status summary:\n")
	for status, count := range fleet.StatusSummary() {
		fmt.Printf("    %-12s %d\n", status, count)
	}

	// Embedding patterns used
	fmt.Println("\n── Embedding Patterns Used ──")
	patterns := []string{
		"Agent embeds Identity      → promotes ID, Name, Role, Uptime()",
		"Agent embeds Network       → promotes Host, Port, Addr(), URL()",
		"Agent embeds HealthChecker → promotes Status(), RecordCheck()",
		"Agent embeds Metrics       → promotes SetGauge(), GetGauge(), Snapshot()",
		"Agent embeds Labeled       → promotes HasLabel(), MatchLabels()",
		"Agent.String() overrides Identity.String() (outer type wins)",
		"Agent.Report() calls a.Identity.String() explicitly (access base)",
		"Each capability is independently testable and reusable",
		"Fleet queries use promoted methods transparently",
	}
	for _, p := range patterns {
		fmt.Printf("  • %s\n", p)
	}
}
```

---

## 13. Tabla de errores comunes

```
┌────┬──────────────────────────────────────────┬──────────────────────────────────────────┬─────────┐
│ #  │ Error                                    │ Solucion                                 │ Nivel   │
├────┼──────────────────────────────────────────┼──────────────────────────────────────────┼─────────┤
│  1 │ Embedding de sync.Mutex expone Lock()    │ Usar campo nombrado: mu sync.Mutex       │ Design  │
│  2 │ Acceso a campo de *T embebido nil        │ Verificar nil antes, o no embeber *T      │ Panic   │
│  3 │ Ambiguedad por mismo campo en 2 embebidos│ Desambiguar: outer.TypeA.Field            │ Compile │
│  4 │ Confundir embedding con herencia         │ Embedding = HAS-A, no IS-A. Sin subtipo  │ Design  │
│  5 │ Cadena de embedding > 2 niveles          │ Aplanar struct o usar campos nombrados    │ Maint.  │
│  6 │ Inicializar campos del embebido al nivel │ Usar Base: Base{} en literal, no fields   │ Compile │
│    │ superior en literal                      │ promovidos directamente                   │         │
│  7 │ Exponer TODO el API del embebido         │ Solo embeber si todos los metodos tienen   │ Design  │
│    │                                          │ sentido en el outer type                  │         │
│  8 │ Embeber interface sin asignar impl.      │ El campo interfaz es nil → panic al llamar│ Panic   │
│  9 │ Copiar struct con *T embebido            │ Copia comparte el puntero — deep copy si  │ Logic   │
│    │                                          │ necesitas independencia                   │         │
│ 10 │ JSON tag collision entre outer y embedded │ El campo directo gana — verificar nombres │ Logic   │
│ 11 │ No llamar al "super" (metodo base)       │ outer.Embedded.Method() para delegar      │ Logic   │
│ 12 │ Usar Deref-like embedding para herencia  │ Usar interfaces para polimorfismo         │ Design  │
└────┴──────────────────────────────────────────┴──────────────────────────────────────────┴─────────┘
```

---

## 14. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
type Base struct{ Name string }
func (b Base) Hello() string { return "Hi from " + b.Name }

type Child struct{ Base }

c := Child{Base: Base{Name: "Go"}}
fmt.Println(c.Hello())
fmt.Println(c.Name)
```

<details>
<summary>Respuesta</summary>

```
Hi from Go
Go
```

`Hello()` y `Name` se promueven de Base a Child. `c.Hello()` equivale a `c.Base.Hello()`. `c.Name` equivale a `c.Base.Name`.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
type A struct{ Val int }
type B struct{ Val int }
type C struct {
    A
    B
    Val int
}

c := C{A: A{Val: 1}, B: B{Val: 2}, Val: 3}
fmt.Println(c.Val)
```

<details>
<summary>Respuesta</summary>

```
3
```

`C` tiene su propio campo `Val` (profundidad 0) que tiene prioridad sobre `A.Val` y `B.Val` (profundidad 1). Sin el campo directo, habria error de ambiguedad.
</details>

**Ejercicio 3**: ¿Compila?

```go
type A struct{ X int }
type B struct{ X int }
type C struct {
    A
    B
}

c := C{A: A{X: 1}, B: B{X: 2}}
fmt.Println(c.X)
```

<details>
<summary>Respuesta</summary>

No compila. Error: `ambiguous selector c.X`. Ambos `A` y `B` tienen `X` al mismo nivel de profundidad. Solucion: `c.A.X` o `c.B.X`.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
type Base struct{ Name string }
func (b Base) Who() string { return b.Name }

type Override struct{ Base }
func (o Override) Who() string { return "Override: " + o.Name }

o := Override{Base: Base{Name: "test"}}
fmt.Println(o.Who())
fmt.Println(o.Base.Who())
```

<details>
<summary>Respuesta</summary>

```
Override: test
test
```

`Override` define su propio `Who()` que tiene prioridad sobre el promovido de `Base`. `o.Base.Who()` accede al metodo del embebido directamente.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
type Writer interface {
    Write([]byte) (int, error)
}

type MyWriter struct {
    Writer // Embedding de interfaz
}

w := MyWriter{} // Writer es nil
fmt.Println(w.Writer == nil)
```

<details>
<summary>Respuesta</summary>

```
true
```

`Writer` es una interfaz embebida sin asignar. Es nil. Acceder al campo `w.Writer` retorna nil. Si se llamara `w.Write(...)`, seria un panic por nil pointer.
</details>

**Ejercicio 6**: ¿Que JSON produce?

```go
type Meta struct {
    ID string `json:"id"`
}

type Item struct {
    Meta
    Name string `json:"name"`
}

item := Item{Meta: Meta{ID: "abc"}, Name: "test"}
data, _ := json.Marshal(item)
fmt.Println(string(data))
```

<details>
<summary>Respuesta</summary>

```
{"id":"abc","name":"test"}
```

Los campos del struct embebido se aplanan en la serializacion JSON. `ID` aparece al mismo nivel que `Name`, no anidado dentro de un campo "meta".
</details>

**Ejercicio 7**: ¿Que imprime?

```go
type Base struct{ Name string }
type Child struct{ *Base }

c := Child{} // Base es nil
fmt.Println(c.Base == nil)
```

<details>
<summary>Respuesta</summary>

```
true
```

`*Base` embebido es nil. `c.Base` retorna nil. Acceder a `c.Name` en este estado causaria panic (nil pointer dereference).
</details>

**Ejercicio 8**: ¿Que imprime?

```go
type Logger struct{ prefix string }
func (l Logger) Log(msg string) {
    fmt.Printf("[%s] %s\n", l.prefix, msg)
}

type Service struct {
    Logger
    Name string
}

s1 := Service{Logger: Logger{prefix: "SVC"}, Name: "api"}
s2 := s1
s2.prefix = "NEW"
s1.Log("hello")
s2.Log("hello")
```

<details>
<summary>Respuesta</summary>

```
[SVC] hello
[NEW] hello
```

Embedding por valor. `s2 := s1` copia todo, incluyendo el `Logger`. Modificar `s2.prefix` no afecta `s1`. Son independientes.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
type Logger struct{ prefix string }
func (l Logger) Log(msg string) {
    fmt.Printf("[%s] %s\n", l.prefix, msg)
}

type Service struct {
    *Logger
    Name string
}

shared := &Logger{prefix: "SHARED"}
s1 := Service{Logger: shared, Name: "api"}
s2 := Service{Logger: shared, Name: "web"}
shared.prefix = "UPDATED"
s1.Log("hello")
s2.Log("hello")
```

<details>
<summary>Respuesta</summary>

```
[UPDATED] hello
[UPDATED] hello
```

Embedding por puntero. `s1` y `s2` comparten el mismo `*Logger`. Modificar `shared.prefix` afecta a ambos.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
type Stringer interface {
    String() string
}

type Named struct{ Name string }
func (n Named) String() string { return n.Name }

type Server struct {
    Named
    Port int
}

var s Stringer = Server{Named: Named{Name: "web01"}, Port: 80}
fmt.Println(s.String())
```

<details>
<summary>Respuesta</summary>

```
web01
```

`Server` satisface `Stringer` automaticamente porque `Named` tiene `String()` y ese metodo se promueve a `Server`. Se puede asignar un `Server` a una variable `Stringer`.
</details>

---

## 15. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              EMBEDDING — RESUMEN                             │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SINTAXIS                                                    │
│  type Outer struct {                                         │
│      Inner       // Embed by value (independiente, no nil)   │
│      *Other      // Embed by pointer (compartido, puede nil) │
│      io.Writer   // Embed interface (nil hasta asignar)      │
│  }                                                           │
│                                                              │
│  PROMOCION                                                   │
│  ├─ Campos: outer.Field == outer.Inner.Field                 │
│  ├─ Metodos: outer.Method() == outer.Inner.Method()          │
│  ├─ Interfaces: Inner satisface I → Outer satisface I        │
│  └─ Inicializacion: Outer{Inner: Inner{...}} (no promovida) │
│                                                              │
│  RESOLUCION DE NOMBRES                                       │
│  1. Campo/metodo directo del outer type (profundidad 0)      │
│  2. Promovido de embebido (profundidad 1)                    │
│  3. Promovido de embebido de embebido (profundidad 2)...     │
│  Misma profundidad + mismo nombre → ERROR de ambiguedad      │
│                                                              │
│  OVERRIDE                                                    │
│  ├─ Outer type define mismo metodo → outer gana              │
│  ├─ Acceso a "base": outer.Embedded.Method()                 │
│  └─ No es polimorfismo — no hay vtable ni dispatch dinamico  │
│                                                              │
│  TIPOS DE EMBEDDING                                          │
│  ┌─────────────────────┬─────────────────────────────────┐   │
│  │ Struct en struct (T) │ Copia, independiente, no nil    │   │
│  │ *Struct en struct    │ Puntero, compartido, puede nil  │   │
│  │ Interface en struct  │ Nil hasta asignar, wrapper/decor│   │
│  │ Interface en interf. │ Composicion de interfaces       │   │
│  └─────────────────────┴─────────────────────────────────┘   │
│                                                              │
│  NO ES HERENCIA                                              │
│  ├─ No hay subtipo (Dog no ES Animal)                        │
│  ├─ No hay polimorfismo de herencia                          │
│  ├─ Para polimorfismo → usar interfaces                      │
│  └─ Embedding = HAS-A, interfaces = BEHAVES-LIKE            │
│                                                              │
│  CUANDO EMBEBER                                              │
│  ✓ Todos los metodos del embebido tienen sentido en outer    │
│  ✓ Decorator/wrapper pattern                                 │
│  ✓ Mixins de capacidades ortogonales                         │
│  ✓ Satisfacer interfaz via metodos promovidos                │
│  ✗ NO para simular herencia profunda                         │
│  ✗ NO si expone metodos que no deberian ser publicos         │
│  ✗ NO para sync.Mutex (expone Lock/Unlock)                   │
│                                                              │
│  JSON                                                        │
│  Los campos del embebido se aplanan al mismo nivel           │
│  Campo directo con mismo tag → outer gana                    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T04 - Comparacion — == en structs (solo si todos los campos son comparables), reflect.DeepEqual, maps package
