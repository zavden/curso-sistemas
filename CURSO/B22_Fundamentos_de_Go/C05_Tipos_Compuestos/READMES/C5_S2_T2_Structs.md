# Structs — Declaracion, Inicializacion, Campos Exportados y Struct Tags

## 1. Introduccion

Los structs son el **unico mecanismo de composicion de datos** en Go. No hay clases, no hay herencia, no hay constructores — solo structs con campos (fields) y metodos asociados. Esta simplicidad es deliberada: los structs de Go se comportan como **tipos valor** (se copian al asignar o pasar a funciones), son comparables si todos sus campos lo son, y se combinan mediante **embedding** (composicion) en vez de herencia.

En SysAdmin/DevOps, los structs modelan todo: servidores, configuraciones, requests HTTP, entradas de log, metricas, alertas, deployments, y resultados de comandos. Con struct tags, Go conecta directamente los structs con formatos de serializacion (JSON, YAML, TOML, SQL, Protobuf) — la columna vertebral de las APIs y configuraciones de infraestructura.

```
┌──────────────────────────────────────────────────────────────┐
│              STRUCTS EN GO                                   │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  type Server struct {                                        │
│      Name     string        // campo exportado (mayuscula)   │
│      IP       string        // visible desde otros paquetes  │
│      port     int           // campo no exportado (minusc.)  │
│      Status   ServerStatus  // tipo custom                   │
│      Labels   map[string]string                              │
│      metrics  *Metrics      // puntero a otro struct         │
│  }                                                           │
│                                                              │
│  Propiedades:                                                │
│  ├─ Tipo valor: se copia al asignar y pasar a funciones      │
│  ├─ Zero value: cada campo tiene su zero value               │
│  │   (string="", int=0, bool=false, ptr=nil, map=nil)        │
│  ├─ Comparable si TODOS los campos son comparables           │
│  ├─ Campos con mayuscula → exportados (publicos)             │
│  ├─ Campos con minuscula → no exportados (privados al pkg)   │
│  ├─ Struct tags: metadatos para reflexion (`json:"name"`)    │
│  ├─ Anonymous fields → embedding (composicion)               │
│  └─ No herencia, no constructores, no destructores           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Declaracion de structs

### Declaracion con nombre (named struct)

```go
// Declaracion basica
type Server struct {
    Name   string
    IP     string
    Port   int
    Online bool
}

// Convencion: nombres de tipo en PascalCase
// Campos exportados en PascalCase, no exportados en camelCase

// Campos del mismo tipo pueden compartir linea (pero no es recomendado):
type Point struct {
    X, Y, Z float64 // Tres campos float64 en una linea
}

// Mejor legibilidad — un campo por linea:
type Point struct {
    X float64
    Y float64
    Z float64
}
```

### Struct con tipos variados

```go
type Deployment struct {
    // Tipos primitivos
    Name      string
    Replicas  int
    Ready     bool
    
    // Tipos compuestos
    Labels    map[string]string
    Ports     []int
    
    // Tipos custom
    Status    DeploymentStatus
    Strategy  RolloutStrategy
    
    // Punteros
    Config    *DeploymentConfig
    
    // Funciones (raro pero valido)
    OnReady   func(name string)
    
    // Interfaces
    Logger    io.Writer
    
    // Tiempo
    CreatedAt time.Time
    UpdatedAt time.Time
    
    // Duracion
    Timeout   time.Duration
}
```

### Struct vacio

```go
// struct{} ocupa 0 bytes de memoria
type Signal struct{}

// Usos principales del struct vacio:
// 1. Set eficiente: map[string]struct{} (0 bytes por value)
visited := map[string]struct{}{
    "web01": {},
    "db01":  {},
}

// 2. Channel de senal (no necesitas enviar datos):
done := make(chan struct{})
go func() {
    // ... trabajo ...
    close(done) // Senalar que termino
}()
<-done

// 3. Implementar interfaz sin estado:
type NullLogger struct{}
func (NullLogger) Write(p []byte) (int, error) {
    return len(p), nil
}
```

### Struct anonimo (sin nombre)

```go
// Struct sin nombre — util para one-offs
server := struct {
    Name string
    IP   string
    Port int
}{
    Name: "web01",
    IP:   "10.0.1.1",
    Port: 80,
}

fmt.Println(server.Name) // "web01"

// Comun en tests:
tests := []struct {
    name     string
    input    int
    expected int
}{
    {"positive", 5, 25},
    {"zero", 0, 0},
    {"negative", -3, 9},
}

for _, tt := range tests {
    t.Run(tt.name, func(t *testing.T) {
        result := square(tt.input)
        if result != tt.expected {
            t.Errorf("got %d, want %d", result, tt.expected)
        }
    })
}

// Struct anonimo inline para JSON rapido:
resp := struct {
    Status  string `json:"status"`
    Message string `json:"message"`
}{
    Status:  "ok",
    Message: "deployment complete",
}
json.NewEncoder(w).Encode(resp)
```

---

## 3. Inicializacion de structs

### Inicializacion con campos nombrados (recomendado)

```go
// ✓ Campos nombrados: claro, orden no importa, resistente a cambios
server := Server{
    Name:   "web01",
    IP:     "10.0.1.1",
    Port:   80,
    Online: true,
}

// Campos no mencionados reciben su zero value:
server2 := Server{
    Name: "db01",
    IP:   "10.0.2.1",
    // Port = 0, Online = false (zero values)
}

// Se puede inicializar en una linea:
s := Server{Name: "cache01", IP: "10.0.3.1", Port: 6379}
```

### Inicializacion posicional (evitar)

```go
// ✗ Posicional: fragil, rompe si se agregan/reordenan campos
server := Server{"web01", "10.0.1.1", 80, true}

// Si se agrega un campo Region entre IP y Port:
// type Server struct {
//     Name   string
//     IP     string
//     Region string  ← nuevo
//     Port   int
//     Online bool
// }
// server := Server{"web01", "10.0.1.1", 80, true}
// → Error de compilacion (80 no es string)
// Con campos nombrados no habria problema

// Unica excepcion aceptable: structs muy pequenos y estables:
type Point struct{ X, Y int }
p := Point{3, 4} // Aceptable por su simplicidad
```

### Zero value de structs

```go
// El zero value de un struct: cada campo con su zero value
var s Server
// s.Name = ""     (string zero)
// s.IP = ""       (string zero)
// s.Port = 0      (int zero)
// s.Online = false (bool zero)

// Esto es una caracteristica de diseño de Go:
// Los zero values deben ser UTILES

// ✓ Buen diseño: zero value usable
type Buffer struct {
    data []byte
    // len y cap son manejados por el slice — zero value funciona
}
var buf Buffer
buf.data = append(buf.data, 'h', 'e', 'l', 'l', 'o') // OK

// sync.Mutex tiene zero value usable:
var mu sync.Mutex
mu.Lock() // OK — no necesitas inicializacion

// ✗ Mal diseño: zero value inutilizable
type BadCache struct {
    items map[string]string // nil — panic en write
}
var c BadCache
// c.items["key"] = "val" // PANIC — nil map

// ✓ Solucion: constructor
func NewCache() *BadCache {
    return &BadCache{
        items: make(map[string]string),
    }
}
```

### new() vs literal con &

```go
// new(T) aloca un *T con zero value
s1 := new(Server)        // *Server con todos los campos en zero value
s1.Name = "web01"        // Asignar campo por campo

// Equivalente a:
s2 := &Server{}          // *Server con zero value — mas idiomatico

// Con campos nombrados:
s3 := &Server{
    Name: "web01",
    Port: 80,
}

// ¿Cuando usar new vs &T{}?
// - new(T): raro en Go idiomatico, solo para zero value puro
// - &T{}: preferido — permite inicializacion con campos
// - T{}: cuando no necesitas puntero (tipo valor)

// Lo que NUNCA hacer:
// s := Server{}
// sp := &s  // Innecesario — usa &Server{} directamente
```

### Patron constructor: New functions

```go
// Go no tiene constructores. Convencion: funcion New*
type Service struct {
    name    string
    port    int
    labels  map[string]string
    started bool
}

// Constructor basico
func NewService(name string, port int) *Service {
    return &Service{
        name:   name,
        port:   port,
        labels: make(map[string]string), // Inicializar map
    }
}

// Constructor con validacion
func NewServiceValidated(name string, port int) (*Service, error) {
    if name == "" {
        return nil, fmt.Errorf("service name cannot be empty")
    }
    if port < 1 || port > 65535 {
        return nil, fmt.Errorf("invalid port: %d", port)
    }
    return &Service{
        name:   name,
        port:   port,
        labels: make(map[string]string),
    }, nil
}

// Constructor con functional options (patron avanzado)
type ServiceOption func(*Service)

func WithLabels(labels map[string]string) ServiceOption {
    return func(s *Service) {
        for k, v := range labels {
            s.labels[k] = v
        }
    }
}

func WithStarted(started bool) ServiceOption {
    return func(s *Service) {
        s.started = started
    }
}

func NewServiceWithOptions(name string, port int, opts ...ServiceOption) *Service {
    s := &Service{
        name:   name,
        port:   port,
        labels: make(map[string]string),
    }
    for _, opt := range opts {
        opt(s)
    }
    return s
}

// Uso:
svc := NewServiceWithOptions("api", 8080,
    WithLabels(map[string]string{"env": "prod"}),
    WithStarted(true),
)
```

---

## 4. Campos exportados vs no exportados

### Visibilidad: mayuscula vs minuscula

```go
// En Go, la visibilidad se determina por la primera letra del nombre:
// Mayuscula → Exportado (visible desde otros paquetes)
// Minuscula → No exportado (privado al paquete)

// Esto aplica a: tipos, funciones, metodos, campos, constantes, variables

// package server
type Server struct {
    Name     string   // Exportado: accesible desde otros paquetes
    IP       string   // Exportado
    port     int      // NO exportado: solo visible en package server
    internal bool     // NO exportado
}

// Desde otro paquete:
// s := server.Server{
//     Name: "web01",    // ✓ OK
//     IP: "10.0.1.1",   // ✓ OK
//     port: 80,          // ✗ ERROR: cannot refer to unexported field
// }

// s.port = 80           // ✗ ERROR: cannot refer to unexported field
```

### Getter y Setter para campos no exportados

```go
// Go NO usa getX/setX como Java
// Convencion Go: el getter tiene el nombre del campo (sin Get)

type Server struct {
    name string
    port int
}

// ✓ Getter idiomatico: mismo nombre que el campo, sin "Get"
func (s *Server) Name() string {
    return s.name
}

func (s *Server) Port() int {
    return s.port
}

// ✗ NO idiomatico:
// func (s *Server) GetName() string { ... }  // No usar "Get"

// Setter: usa "Set" como prefijo (solo cuando es necesario)
func (s *Server) SetPort(port int) error {
    if port < 1 || port > 65535 {
        return fmt.Errorf("invalid port: %d", port)
    }
    s.port = port
    return nil
}

// ¿Cuando usar campos no exportados?
// - Cuando necesitas validacion en la escritura
// - Cuando el campo tiene invariantes que mantener
// - Cuando necesitas encapsulacion (ej: sync.Mutex no debe copiarse)
// - Para prevenir uso incorrecto desde otros paquetes

// ¿Cuando usar campos exportados?
// - DTOs (Data Transfer Objects): structs para JSON/YAML
// - Cuando no hay invariantes que proteger
// - Cuando el struct es simple y los campos son autoexplicativos
// - La mayoria de los structs de configuracion
```

### Package-level encapsulacion

```go
// Go encapsula a nivel de PAQUETE, no de struct
// Todos los archivos del mismo paquete pueden acceder a campos no exportados

// archivo: server.go
package infra

type Server struct {
    name   string
    port   int
    health bool
}

// archivo: utils.go (mismo paquete infra)
func resetServer(s *Server) {
    s.name = ""     // ✓ OK — mismo paquete
    s.port = 0      // ✓ OK
    s.health = false // ✓ OK
}

// Esto significa que la "privacidad" en Go es a nivel de paquete,
// no a nivel de struct como en Java/C++.
// Todos los archivos de package infra son "amigos" entre si.
```

---

## 5. Struct tags

### Que son los struct tags

```go
// Struct tags son strings de metadatos asociados a campos
// Se usan con reflexion (reflect package) para serialización,
// validacion, mapeo de DB, y mas.

type Server struct {
    Name     string `json:"name"`
    IP       string `json:"ip_address"`
    Port     int    `json:"port"`
    Online   bool   `json:"is_online"`
    internal string // Sin tag — no se serializa (no exportado)
}

// Los tags son strings raw entre backticks
// Formato: `key:"value" key2:"value2"`
// Cada key es un "namespace" (json, yaml, xml, db, validate, etc.)

// El tag es accesible via reflect:
t := reflect.TypeOf(Server{})
field, _ := t.FieldByName("IP")
fmt.Println(field.Tag.Get("json")) // "ip_address"
```

### JSON tags — el mas comun

```go
import "encoding/json"

type Deployment struct {
    // Basico: renombrar campo en JSON
    Name string `json:"name"`
    
    // Omitir si tiene zero value
    Replicas int `json:"replicas,omitempty"`
    
    // Omitir siempre (campo interno)
    InternalID string `json:"-"`
    
    // Campo cuyo nombre JSON es literalmente "-"
    Hyphen string `json:"-,"`
    
    // String representation de un numero
    Port int `json:"port,string"`
    
    // Sin tag: usa el nombre del campo tal cual
    Status string // Se serializa como "Status"
}

// omitempty rules:
// - int/float:   omite si es 0
// - string:      omite si es ""
// - bool:        omite si es false
// - pointer:     omite si es nil
// - slice/map:   omite si es nil (NO si esta vacio)
// - struct:      NUNCA omite (no es "empty" aunque tenga zero fields)
//   → usa *Struct para que omitempty funcione con structs
```

### Ejemplo completo de JSON

```go
type ServiceConfig struct {
    Name        string            `json:"name"`
    Port        int               `json:"port"`
    Host        string            `json:"host,omitempty"`
    Enabled     bool              `json:"enabled"`
    Labels      map[string]string `json:"labels,omitempty"`
    MaxRetries  int               `json:"max_retries,omitempty"`
    Timeout     string            `json:"timeout,omitempty"`
    HealthCheck *HealthCheck      `json:"health_check,omitempty"`
    secret      string            // No exportado → no serializado
}

type HealthCheck struct {
    Path     string `json:"path"`
    Interval string `json:"interval"`
}

// Marshal (Go → JSON)
cfg := ServiceConfig{
    Name:       "api",
    Port:       8080,
    Enabled:    true,
    Labels:     map[string]string{"env": "prod", "tier": "backend"},
    MaxRetries: 3,
    HealthCheck: &HealthCheck{
        Path:     "/healthz",
        Interval: "10s",
    },
}

data, err := json.MarshalIndent(cfg, "", "  ")
if err != nil {
    log.Fatal(err)
}
fmt.Println(string(data))
// {
//   "name": "api",
//   "port": 8080,
//   "enabled": true,
//   "labels": {
//     "env": "prod",
//     "tier": "backend"
//   },
//   "max_retries": 3,
//   "health_check": {
//     "path": "/healthz",
//     "interval": "10s"
//   }
// }
// Nota: "host" no aparece (omitempty, string vacio)

// Unmarshal (JSON → Go)
jsonStr := `{"name":"web","port":80,"enabled":false}`
var cfg2 ServiceConfig
err = json.Unmarshal([]byte(jsonStr), &cfg2)
fmt.Println(cfg2.Name, cfg2.Port, cfg2.Enabled) // web 80 false
```

### YAML tags

```go
// Requiere libreria externa: gopkg.in/yaml.v3
import "gopkg.in/yaml.v3"

type KubeDeployment struct {
    APIVersion string            `yaml:"apiVersion"`
    Kind       string            `yaml:"kind"`
    Metadata   KubeMetadata      `yaml:"metadata"`
    Spec       KubeDeploySpec    `yaml:"spec"`
}

type KubeMetadata struct {
    Name      string            `yaml:"name"`
    Namespace string            `yaml:"namespace,omitempty"`
    Labels    map[string]string `yaml:"labels,omitempty"`
}

type KubeDeploySpec struct {
    Replicas int              `yaml:"replicas"`
    Selector map[string]string `yaml:"selector"`
}

// Marshal → YAML
deploy := KubeDeployment{
    APIVersion: "apps/v1",
    Kind:       "Deployment",
    Metadata: KubeMetadata{
        Name:      "api-server",
        Namespace: "production",
        Labels: map[string]string{
            "app":  "api",
            "tier": "backend",
        },
    },
    Spec: KubeDeploySpec{
        Replicas: 3,
    },
}

data, _ := yaml.Marshal(deploy)
fmt.Println(string(data))
// apiVersion: apps/v1
// kind: Deployment
// metadata:
//   name: api-server
//   namespace: production
//   labels:
//     app: api
//     tier: backend
// spec:
//   replicas: 3
```

### Multiples tags en un campo

```go
type User struct {
    ID       int    `json:"id"        db:"user_id"    validate:"required"`
    Name     string `json:"name"      db:"user_name"  validate:"required,min=2"`
    Email    string `json:"email"     db:"email"      validate:"required,email"`
    Age      int    `json:"age"       db:"age"        validate:"gte=0,lte=150"`
    Role     string `json:"role"      db:"role"       validate:"oneof=admin user guest"`
    Password string `json:"-"         db:"password"   validate:"required,min=8"`
}

// Cada tag namespace es leido por una libreria diferente:
// - json:     encoding/json
// - db:       sqlx, gorm, etc.
// - validate: go-playground/validator
// - yaml:     gopkg.in/yaml.v3
// - toml:     BurntSushi/toml
// - xml:      encoding/xml
// - env:      caarlos0/env
// - mapstructure: mitchellh/mapstructure
```

### Alinear tags para legibilidad

```go
// Convencion: alinear tags con tabs para que sean legibles
// La mayoria de los editores/formatters lo hacen automaticamente

// ✓ Alineado (legible):
type Config struct {
    Host     string `json:"host"      yaml:"host"      env:"APP_HOST"`
    Port     int    `json:"port"      yaml:"port"      env:"APP_PORT"`
    LogLevel string `json:"log_level" yaml:"log_level"  env:"LOG_LEVEL"`
    Debug    bool   `json:"debug"     yaml:"debug"      env:"DEBUG"`
}

// ✗ Sin alinear (dificil de leer):
type Config struct {
    Host string `json:"host" yaml:"host" env:"APP_HOST"`
    Port int `json:"port" yaml:"port" env:"APP_PORT"`
    LogLevel string `json:"log_level" yaml:"log_level" env:"LOG_LEVEL"`
    Debug bool `json:"debug" yaml:"debug" env:"DEBUG"`
}
```

---

## 6. Structs como tipos valor

### Copia al asignar

```go
type Server struct {
    Name string
    Port int
}

s1 := Server{Name: "web01", Port: 80}
s2 := s1 // s2 es una COPIA completa de s1

s2.Name = "web02"
fmt.Println(s1.Name) // "web01" — no afectado
fmt.Println(s2.Name) // "web02"

// Cada asignacion y cada paso a funcion crea una copia
// Para structs grandes, esto puede ser costoso
```

### Copia al pasar a funcion

```go
func updateName(s Server, name string) {
    s.Name = name // Modifica la COPIA local
}

s := Server{Name: "web01", Port: 80}
updateName(s, "web02")
fmt.Println(s.Name) // "web01" — no modificado

// ✓ Para modificar: pasar puntero
func updateNamePtr(s *Server, name string) {
    s.Name = name // Modifica el original
}

updateNamePtr(&s, "web02")
fmt.Println(s.Name) // "web02" — modificado
```

### Cuando usar valor vs puntero

```go
// Valor (T):
// ✓ Struct pequeño (< 3-4 campos simples)
// ✓ Inmutable (no necesitas modificar)
// ✓ Thread-safe por naturaleza (cada goroutine tiene su copia)
// ✓ Localidad de cache (datos contiguos en stack)

type Point struct{ X, Y int }          // ✓ Valor
type Color struct{ R, G, B uint8 }     // ✓ Valor

// Puntero (*T):
// ✓ Struct grande (evitar copias)
// ✓ Necesitas modificar el struct
// ✓ Compartir entre multiples variables/goroutines (con sync)
// ✓ Puede ser nil (diferenciar "no inicializado" de "zero value")
// ✓ Contiene campos que no deben copiarse (sync.Mutex)

type Deployment struct{ /* 20 campos */ }  // *Deployment
type Database struct{ conn *sql.DB }       // *Database (estado mutable)
```

### Structs con campos referencia: copia shallow

```go
type Config struct {
    Name   string
    Labels map[string]string // Referencia
    Ports  []int             // Referencia
}

c1 := Config{
    Name:   "web",
    Labels: map[string]string{"env": "prod"},
    Ports:  []int{80, 443},
}

c2 := c1 // Copia shallow

// Name se copia (string es inmutable — seguro)
// Labels: se copia el PUNTERO al map — comparten el mismo map
// Ports: se copia el HEADER del slice — comparten el backing array

c2.Name = "api"          // OK — independiente
c2.Labels["env"] = "dev" // ¡Modifica c1 tambien!
c2.Ports[0] = 8080       // ¡Modifica c1 tambien!

fmt.Println(c1.Labels["env"]) // "dev" — corrompido
fmt.Println(c1.Ports[0])       // 8080 — corrompido

// ✓ Deep copy manual:
func (c Config) Clone() Config {
    clone := c
    // Copiar map
    clone.Labels = make(map[string]string, len(c.Labels))
    for k, v := range c.Labels {
        clone.Labels[k] = v
    }
    // Copiar slice
    clone.Ports = slices.Clone(c.Ports)
    return clone
}

c3 := c1.Clone()
c3.Labels["env"] = "staging" // No afecta c1
```

---

## 7. Comparacion de structs

### Structs comparables

```go
// Un struct es comparable si TODOS sus campos son comparables
type Point struct {
    X, Y int
}

p1 := Point{3, 4}
p2 := Point{3, 4}
p3 := Point{5, 6}

fmt.Println(p1 == p2) // true  — todos los campos iguales
fmt.Println(p1 == p3) // false
fmt.Println(p1 != p3) // true

// Se puede usar como key de map
distances := map[Point]float64{
    {0, 0}: 0,
    {3, 4}: 5,
}
```

### Structs no comparables

```go
// Si CUALQUIER campo no es comparable, el struct tampoco
type Server struct {
    Name  string
    Ports []int // slice no es comparable
}

// s1 := Server{"web01", []int{80}}
// s2 := Server{"web01", []int{80}}
// fmt.Println(s1 == s2) // ✗ ERROR: struct containing []int cannot be compared

// Solucion 1: reflect.DeepEqual (lento)
fmt.Println(reflect.DeepEqual(s1, s2)) // true

// Solucion 2: metodo Equal manual (rapido)
func (s Server) Equal(other Server) bool {
    return s.Name == other.Name && slices.Equal(s.Ports, other.Ports)
}
```

### No puedes comparar structs de diferentes tipos

```go
type ServerA struct {
    Name string
    Port int
}

type ServerB struct {
    Name string
    Port int
}

a := ServerA{"web01", 80}
b := ServerB{"web01", 80}

// fmt.Println(a == b) // ✗ ERROR: mismatched types ServerA and ServerB

// Aunque tengan los mismos campos, son tipos diferentes
// Puedes convertir si los campos son identicos:
fmt.Println(a == ServerA(b)) // ✓ true — conversion explicita
```

---

## 8. Struct y punteros: patrones de uso

### Puntero a struct: acceso a campos

```go
type Server struct {
    Name string
    Port int
}

s := &Server{Name: "web01", Port: 80}

// Go auto-dereferencia punteros a struct en acceso a campos:
fmt.Println(s.Name) // Equivalente a (*s).Name
s.Port = 8080       // Equivalente a (*s).Port = 8080

// No necesitas escribir (*s).Field — Go lo hace por ti
// Esta es una de las simplificaciones mas utiles del lenguaje
```

### nil struct pointer

```go
type Server struct {
    Name string
    Port int
}

var s *Server // nil

// Acceder a campos de nil pointer → panic
// fmt.Println(s.Name) // panic: runtime error: invalid memory address

// ✓ Verificar nil antes de acceder
if s != nil {
    fmt.Println(s.Name)
}

// Metodos con nil receiver son validos:
func (s *Server) String() string {
    if s == nil {
        return "<nil server>"
    }
    return fmt.Sprintf("%s:%d", s.Name, s.Port)
}

var sv *Server
fmt.Println(sv.String()) // "<nil server>" — no panic
```

### Patron: Optional fields con puntero

```go
// Punteros para distinguir "no proporcionado" de "zero value"
type UpdateRequest struct {
    Name     *string `json:"name,omitempty"`
    Port     *int    `json:"port,omitempty"`
    Enabled  *bool   `json:"enabled,omitempty"`
    Replicas *int    `json:"replicas,omitempty"`
}

// En JSON:
// {"port": 0}      → Port = &0  (explicitamente 0)
// {}                → Port = nil (no proporcionado)
// Sin puntero, ambos serian Port = 0 — ambiguedad

// Helper para crear punteros a literales:
func ptr[T any](v T) *T {
    return &v
}

req := UpdateRequest{
    Port:     ptr(8080),
    Enabled:  ptr(false), // Explicitamente false (no "no proporcionado")
    Replicas: ptr(0),     // Explicitamente 0 replicas
}

// Aplicar update parcial:
func (s *Server) Apply(req UpdateRequest) {
    if req.Name != nil {
        s.Name = *req.Name
    }
    if req.Port != nil {
        s.Port = *req.Port
    }
    // ... etc
}
```

---

## 9. Structs y JSON: patrones avanzados

### Custom Marshal/Unmarshal

```go
// Implementar json.Marshaler y json.Unmarshaler para control total

type Duration struct {
    time.Duration
}

// Serializar como string legible ("30s", "5m", "1h")
func (d Duration) MarshalJSON() ([]byte, error) {
    return json.Marshal(d.Duration.String())
}

func (d *Duration) UnmarshalJSON(b []byte) error {
    var s string
    if err := json.Unmarshal(b, &s); err != nil {
        return err
    }
    dur, err := time.ParseDuration(s)
    if err != nil {
        return err
    }
    d.Duration = dur
    return nil
}

type TimeoutConfig struct {
    Connect Duration `json:"connect_timeout"`
    Read    Duration `json:"read_timeout"`
    Write   Duration `json:"write_timeout"`
}

// JSON:
// {
//   "connect_timeout": "5s",
//   "read_timeout": "30s",
//   "write_timeout": "10s"
// }
```

### Enum-like types con JSON

```go
type Status string

const (
    StatusActive   Status = "active"
    StatusInactive Status = "inactive"
    StatusPending  Status = "pending"
)

// La serializacion funciona automaticamente porque Status es string
type Service struct {
    Name   string `json:"name"`
    Status Status `json:"status"`
}

// Para enums con iota (int), necesitas custom marshal:
type Priority int

const (
    PriorityLow Priority = iota
    PriorityMedium
    PriorityHigh
    PriorityCritical
)

var priorityNames = map[Priority]string{
    PriorityLow:      "low",
    PriorityMedium:   "medium",
    PriorityHigh:     "high",
    PriorityCritical: "critical",
}

var priorityValues = map[string]Priority{
    "low":      PriorityLow,
    "medium":   PriorityMedium,
    "high":     PriorityHigh,
    "critical": PriorityCritical,
}

func (p Priority) MarshalJSON() ([]byte, error) {
    name, ok := priorityNames[p]
    if !ok {
        return nil, fmt.Errorf("unknown priority: %d", p)
    }
    return json.Marshal(name)
}

func (p *Priority) UnmarshalJSON(b []byte) error {
    var s string
    if err := json.Unmarshal(b, &s); err != nil {
        return err
    }
    val, ok := priorityValues[s]
    if !ok {
        return fmt.Errorf("unknown priority: %q", s)
    }
    *p = val
    return nil
}
```

### json.RawMessage para campos polimorficos

```go
// Cuando un campo puede tener diferentes formas segun el contexto
type Event struct {
    Type    string          `json:"type"`
    Payload json.RawMessage `json:"payload"` // Parseo diferido
}

// Diferentes payloads segun type:
type DeployPayload struct {
    Service  string `json:"service"`
    Version  string `json:"version"`
    Replicas int    `json:"replicas"`
}

type AlertPayload struct {
    Host     string `json:"host"`
    Severity string `json:"severity"`
    Message  string `json:"message"`
}

func processEvent(data []byte) error {
    var event Event
    if err := json.Unmarshal(data, &event); err != nil {
        return err
    }

    switch event.Type {
    case "deploy":
        var payload DeployPayload
        if err := json.Unmarshal(event.Payload, &payload); err != nil {
            return err
        }
        fmt.Printf("Deploy %s v%s (%d replicas)\n",
            payload.Service, payload.Version, payload.Replicas)

    case "alert":
        var payload AlertPayload
        if err := json.Unmarshal(event.Payload, &payload); err != nil {
            return err
        }
        fmt.Printf("Alert on %s: [%s] %s\n",
            payload.Host, payload.Severity, payload.Message)

    default:
        return fmt.Errorf("unknown event type: %s", event.Type)
    }
    return nil
}
```

### Streaming JSON con Decoder/Encoder

```go
// Para archivos grandes o conexiones de red, usar Decoder/Encoder
// en lugar de Marshal/Unmarshal (no requiere tener todo en memoria)

// Leer JSON linea a linea (JSON Lines / NDJSON)
func processJSONLines(r io.Reader) error {
    decoder := json.NewDecoder(r)
    for decoder.More() {
        var entry LogEntry
        if err := decoder.Decode(&entry); err != nil {
            return err
        }
        processEntry(entry)
    }
    return nil
}

// Escribir JSON a un writer (HTTP response, archivo)
func writeJSON(w io.Writer, data any) error {
    encoder := json.NewEncoder(w)
    encoder.SetIndent("", "  ")
    return encoder.Encode(data)
}
```

---

## 10. Struct memory layout: alineacion y padding

### Como Go alinea los campos en memoria

```go
// Go alinea los campos a su tamano natural:
// - byte/bool:   1-byte alignment
// - int16:       2-byte alignment
// - int32/float32: 4-byte alignment
// - int64/float64: 8-byte alignment
// - pointer/string/slice: 8-byte alignment (64-bit)

// Orden de campos afecta el tamaño total del struct:

// ✗ Orden suboptimo — mucho padding:
type Bad struct {
    a bool    // 1 byte + 7 padding
    b int64   // 8 bytes
    c bool    // 1 byte + 3 padding
    d int32   // 4 bytes
    e bool    // 1 byte + 7 padding
    f int64   // 8 bytes
}
// Total: 40 bytes (15 bytes de padding desperdiciados)

// ✓ Orden optimo — campos ordenados por tamaño descendente:
type Good struct {
    b int64   // 8 bytes
    f int64   // 8 bytes
    d int32   // 4 bytes
    a bool    // 1 byte
    c bool    // 1 byte
    e bool    // 1 byte + 1 padding
}
// Total: 24 bytes (solo 1 byte de padding)

// Verificar con unsafe.Sizeof:
fmt.Println(unsafe.Sizeof(Bad{}))  // 40
fmt.Println(unsafe.Sizeof(Good{})) // 24
```

### Visualizar padding

```
Bad struct (40 bytes):
┌────┬───────────────┬────────────────┬────┬──────────┬──────────────┬────┬───────────────┬────────────────┐
│ a  │ padding(7)    │ b (8 bytes)    │ c  │ pad(3)   │ d (4 bytes)  │ e  │ padding(7)    │ f (8 bytes)    │
│ 1B │ 7B            │ 8B             │ 1B │ 3B       │ 4B           │ 1B │ 7B            │ 8B             │
└────┴───────────────┴────────────────┴────┴──────────┴──────────────┴────┴───────────────┴────────────────┘

Good struct (24 bytes):
┌────────────────┬────────────────┬──────────────┬────┬────┬────┬──────┐
│ b (8 bytes)    │ f (8 bytes)    │ d (4 bytes)  │ a  │ c  │ e  │ pad  │
│ 8B             │ 8B             │ 4B           │ 1B │ 1B │ 1B │ 1B   │
└────────────────┴────────────────┴──────────────┴────┴────┴────┴──────┘
```

### Herramientas para detectar padding

```bash
# fieldalignment: detecta structs con padding suboptimo
go install golang.org/x/tools/go/analysis/passes/fieldalignment/cmd/fieldalignment@latest
fieldalignment ./...

# go vet con fieldalignment:
# (no incluido en go vet por defecto, disponible como analyzer)

# Regla practica: solo optimizar structs que:
# 1. Se crean en cantidades masivas (>100K instancias)
# 2. Se almacenan en slices/maps grandes
# Para la mayoria de los structs, la legibilidad importa mas
```

### Cuando importa la alineacion

```go
// ✓ Importa: slice de 1 millon de metricas
type Metric struct {
    Timestamp int64   // 8
    Value     float64 // 8
    Flags     uint8   // 1
    // Total: 17 → 24 con padding
}
// 1,000,000 * 24 = 24 MB

type MetricOptimized struct {
    Timestamp int64   // 8
    Value     float64 // 8
    Flags     uint8   // 1 + 7 padding? No — ultimo campo, Go no agrega padding final innecesariamente
    // Bueno, Go SI agrega padding al final para que el stride sea multiplo de alignment
    // Total: 24 bytes de todas formas aqui
}

// ✗ No importa: struct de configuracion (se crea una vez)
type AppConfig struct {
    Debug     bool
    Port      int
    Host      string
    // No optimizar — legibilidad primero
}
```

---

## 11. Patrones SysAdmin con structs

### Patron: Builder para configuracion compleja

```go
type ServerConfig struct {
    Name        string
    Host        string
    Port        int
    TLS         bool
    CertFile    string
    KeyFile     string
    ReadTimeout time.Duration
    WriteTimeout time.Duration
    MaxConns     int
    Labels       map[string]string
}

type ServerConfigBuilder struct {
    config ServerConfig
}

func NewServerConfigBuilder(name string) *ServerConfigBuilder {
    return &ServerConfigBuilder{
        config: ServerConfig{
            Name:         name,
            Host:         "0.0.0.0",
            Port:         8080,
            ReadTimeout:  30 * time.Second,
            WriteTimeout: 30 * time.Second,
            MaxConns:     1000,
            Labels:       make(map[string]string),
        },
    }
}

func (b *ServerConfigBuilder) Host(h string) *ServerConfigBuilder {
    b.config.Host = h
    return b
}

func (b *ServerConfigBuilder) Port(p int) *ServerConfigBuilder {
    b.config.Port = p
    return b
}

func (b *ServerConfigBuilder) WithTLS(cert, key string) *ServerConfigBuilder {
    b.config.TLS = true
    b.config.CertFile = cert
    b.config.KeyFile = key
    return b
}

func (b *ServerConfigBuilder) Timeouts(read, write time.Duration) *ServerConfigBuilder {
    b.config.ReadTimeout = read
    b.config.WriteTimeout = write
    return b
}

func (b *ServerConfigBuilder) Label(key, value string) *ServerConfigBuilder {
    b.config.Labels[key] = value
    return b
}

func (b *ServerConfigBuilder) Build() (ServerConfig, error) {
    if b.config.Name == "" {
        return ServerConfig{}, fmt.Errorf("name is required")
    }
    if b.config.Port < 1 || b.config.Port > 65535 {
        return ServerConfig{}, fmt.Errorf("invalid port: %d", b.config.Port)
    }
    if b.config.TLS && (b.config.CertFile == "" || b.config.KeyFile == "") {
        return ServerConfig{}, fmt.Errorf("TLS requires cert and key files")
    }
    return b.config, nil
}

// Uso:
cfg, err := NewServerConfigBuilder("api-server").
    Host("10.0.1.1").
    Port(443).
    WithTLS("/etc/ssl/cert.pem", "/etc/ssl/key.pem").
    Timeouts(10*time.Second, 30*time.Second).
    Label("env", "prod").
    Label("tier", "backend").
    Build()
```

### Patron: Result type para operaciones

```go
// Encapsular resultado + error + metadata
type CommandResult struct {
    Host     string
    Command  string
    ExitCode int
    Stdout   string
    Stderr   string
    Duration time.Duration
    Err      error
}

func (r CommandResult) Success() bool {
    return r.Err == nil && r.ExitCode == 0
}

func (r CommandResult) String() string {
    status := "OK"
    if !r.Success() {
        status = "FAIL"
    }
    return fmt.Sprintf("[%s] %s: %q (exit=%d, %s)",
        status, r.Host, r.Command, r.ExitCode, r.Duration)
}

// Ejecutar en multiples hosts y recolectar resultados:
func runOnHosts(hosts []string, command string) []CommandResult {
    results := make([]CommandResult, len(hosts))
    var wg sync.WaitGroup

    for i, host := range hosts {
        wg.Add(1)
        go func(idx int, h string) {
            defer wg.Done()
            start := time.Now()
            stdout, stderr, exitCode, err := executeSSH(h, command)
            results[idx] = CommandResult{
                Host:     h,
                Command:  command,
                ExitCode: exitCode,
                Stdout:   stdout,
                Stderr:   stderr,
                Duration: time.Since(start),
                Err:      err,
            }
        }(i, host)
    }

    wg.Wait()
    return results
}
```

### Patron: Config from multiple sources

```go
// Cargar config con prioridad: defaults → file → env → flags
type AppConfig struct {
    Host     string `json:"host"     env:"APP_HOST"     flag:"host"`
    Port     int    `json:"port"     env:"APP_PORT"     flag:"port"`
    LogLevel string `json:"logLevel" env:"LOG_LEVEL"    flag:"log-level"`
    Debug    bool   `json:"debug"    env:"DEBUG"        flag:"debug"`
    DBUrl    string `json:"dbUrl"    env:"DATABASE_URL" flag:"db-url"`
}

func DefaultConfig() AppConfig {
    return AppConfig{
        Host:     "0.0.0.0",
        Port:     8080,
        LogLevel: "info",
        Debug:    false,
    }
}

func LoadConfig(configPath string) (AppConfig, error) {
    cfg := DefaultConfig()

    // Layer 2: archivo (si existe)
    if configPath != "" {
        data, err := os.ReadFile(configPath)
        if err != nil {
            return cfg, fmt.Errorf("read config: %w", err)
        }
        if err := json.Unmarshal(data, &cfg); err != nil {
            return cfg, fmt.Errorf("parse config: %w", err)
        }
    }

    // Layer 3: variables de entorno
    if v := os.Getenv("APP_HOST"); v != "" {
        cfg.Host = v
    }
    if v := os.Getenv("APP_PORT"); v != "" {
        port, err := strconv.Atoi(v)
        if err != nil {
            return cfg, fmt.Errorf("invalid APP_PORT: %w", err)
        }
        cfg.Port = port
    }
    if v := os.Getenv("LOG_LEVEL"); v != "" {
        cfg.LogLevel = v
    }
    if v := os.Getenv("DEBUG"); v != "" {
        cfg.Debug = v == "true" || v == "1"
    }
    if v := os.Getenv("DATABASE_URL"); v != "" {
        cfg.DBUrl = v
    }

    return cfg, nil
}
```

---

## 12. Comparacion: Go vs C vs Rust

### Go: structs como tipo valor, tags, no herencia

```go
type Server struct {
    Name string `json:"name"`
    Port int    `json:"port"`
}

s := Server{Name: "web01", Port: 80}
// Tipo valor: copia al asignar
// Tags para serializacion
// Metodos: func (s *Server) Method() {}
// Composicion via embedding (no herencia)
// Sin constructores/destructores
```

### C: structs como datos puros, sin metodos

```c
// C structs: pura data, sin metodos, sin tags, sin visibilidad
typedef struct {
    char name[64];
    int port;
    int online;
} Server;

// Inicializacion
Server s = {.name = "web01", .port = 80, .online = 1};

// Acceso
printf("%s:%d\n", s.name, s.port);

// Funciones asociadas (no metodos):
void server_print(const Server *s) {
    printf("%s:%d\n", s->name, s->port);
}

// Para punteros: s->field (no s.field)
// sizeof para tamaño (con padding)
// No hay tags/reflexion
// No hay visibilidad (todo es publico, usa _ prefix por convencion)
// Manual memory management para punteros
```

### Rust: structs con traits, ownership, derive macros

```rust
use serde::{Serialize, Deserialize};

// derive macros reemplazan los tags de Go
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
struct Server {
    #[serde(rename = "name")]
    name: String,        // Privado por defecto
    
    #[serde(rename = "port")]
    port: u16,
    
    #[serde(skip)]       // Equivalente a json:"-"
    internal: bool,
}

// Visibilidad: pub para exportar
pub struct PublicServer {
    pub name: String,    // Exportado
    pub port: u16,       // Exportado
    secret: String,      // Privado al modulo
}

// impl block para metodos (separado de la definicion)
impl Server {
    // Constructor (convencion: new o from_*)
    pub fn new(name: &str, port: u16) -> Self {
        Server {
            name: name.to_string(),
            port,
            internal: false,
        }
    }
    
    pub fn name(&self) -> &str {
        &self.name
    }
}

// Diferencias clave:
// - Rust: privado por defecto, pub para exportar
// - Go: exportado con Mayuscula, no exportado con minuscula
// - Rust: derive macros (compile-time, zero-cost)
// - Go: struct tags (runtime, reflexion)
// - Rust: ownership/borrowing para copias
// - Go: todo se copia libremente
```

### Tabla comparativa

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Go struct            │ C struct             │ Rust struct          │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Tipo                 │ Valor (copia)        │ Valor (copia)        │ Valor (move default) │
│ Metodos              │ Func con receiver    │ No (funciones libres)│ impl block           │
│ Constructores        │ NewX() convencion    │ No                   │ new() convencion     │
│ Destructores         │ No (GC)             │ No (manual free)     │ Drop trait (RAII)    │
│ Visibilidad          │ Mayusc/minusc       │ No (todo publico)    │ pub/privado          │
│ Herencia             │ No (embedding)      │ No                   │ No (traits + impl)   │
│ Tags/Annotations     │ Struct tags (runtime)│ No                   │ Derive macros (comp.)│
│ Serializacion        │ encoding/json + tags │ Manual               │ serde + derive       │
│ Comparacion          │ == (si comparable)   │ memcmp manual        │ PartialEq derive     │
│ Zero value           │ Cada campo su zero   │ No garantizado       │ Default trait        │
│ Generics             │ Go 1.18+            │ No (void*)           │ Si (monomorphization)│
│ Memory layout        │ Go controla padding  │ Controlable (#pragma)│ Controlable (#repr)  │
│ Null/nil             │ Puntero puede ser nil│ Puntero puede ser    │ Option<T>            │
│                      │                      │ NULL                 │                      │
│ Copy                 │ Implicit (siempre)   │ Implicit (memcpy)    │ Explicit (Clone)     │
│                      │                      │                      │ o implicit (Copy)    │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 13. Programa completo: Infrastructure Configuration Manager

Sistema de gestion de configuracion para infraestructura que demuestra todos los patrones de structs: declaracion, tags JSON/YAML, constructores, validacion, serializacion, campos opcionales, y tipos compuestos.

```go
package main

import (
	"encoding/json"
	"fmt"
	"os"
	"slices"
	"strings"
	"time"
)

// ─── Enums como tipos custom ────────────────────────────────────

type Environment string

const (
	EnvDev     Environment = "development"
	EnvStaging Environment = "staging"
	EnvProd    Environment = "production"
)

type Protocol string

const (
	ProtoHTTP  Protocol = "http"
	ProtoHTTPS Protocol = "https"
	ProtoTCP   Protocol = "tcp"
	ProtoGRPC  Protocol = "grpc"
)

// ─── Substructs ─────────────────────────────────────────────────

type TLSConfig struct {
	Enabled  bool   `json:"enabled"`
	CertFile string `json:"cert_file,omitempty"`
	KeyFile  string `json:"key_file,omitempty"`
	CAFile   string `json:"ca_file,omitempty"`
}

type HealthCheck struct {
	Enabled  bool   `json:"enabled"`
	Path     string `json:"path"`
	Interval string `json:"interval"`
	Timeout  string `json:"timeout"`
	Retries  int    `json:"retries"`
}

type ResourceLimits struct {
	CPUCores    float64 `json:"cpu_cores"`
	MemoryMB    int     `json:"memory_mb"`
	DiskGB      int     `json:"disk_gb,omitempty"`
	MaxOpenFiles int    `json:"max_open_files,omitempty"`
}

type LogConfig struct {
	Level  string `json:"level"`
	Format string `json:"format"` // "json" or "text"
	Output string `json:"output"` // "stdout", "stderr", or file path
}

// ─── Service — el struct principal ──────────────────────────────

type ServiceConfig struct {
	// Identidad
	Name        string      `json:"name"`
	Version     string      `json:"version"`
	Environment Environment `json:"environment"`
	Description string      `json:"description,omitempty"`

	// Red
	Host     string   `json:"host"`
	Port     int      `json:"port"`
	Protocol Protocol `json:"protocol"`

	// Opcionales (puntero → nil si no configurado)
	TLS         *TLSConfig      `json:"tls,omitempty"`
	HealthCheck *HealthCheck     `json:"health_check,omitempty"`
	Resources   *ResourceLimits  `json:"resources,omitempty"`
	Logging     *LogConfig       `json:"logging,omitempty"`

	// Metadata
	Labels      map[string]string `json:"labels,omitempty"`
	Annotations map[string]string `json:"annotations,omitempty"`
	DependsOn   []string          `json:"depends_on,omitempty"`

	// Interno (no serializado)
	configPath string
	loadedAt   time.Time
}

// ─── Constructor con defaults ───────────────────────────────────

func NewServiceConfig(name string, port int) *ServiceConfig {
	return &ServiceConfig{
		Name:        name,
		Version:     "0.0.0",
		Environment: EnvDev,
		Host:        "0.0.0.0",
		Port:        port,
		Protocol:    ProtoHTTP,
		Labels:      make(map[string]string),
		Annotations: make(map[string]string),
		Logging: &LogConfig{
			Level:  "info",
			Format: "json",
			Output: "stdout",
		},
		loadedAt: time.Now(),
	}
}

// ─── Functional Options ─────────────────────────────────────────

type ServiceOption func(*ServiceConfig)

func WithEnvironment(env Environment) ServiceOption {
	return func(s *ServiceConfig) {
		s.Environment = env
	}
}

func WithTLS(cert, key string) ServiceOption {
	return func(s *ServiceConfig) {
		s.TLS = &TLSConfig{
			Enabled:  true,
			CertFile: cert,
			KeyFile:  key,
		}
		s.Protocol = ProtoHTTPS
	}
}

func WithHealthCheck(path string, interval time.Duration) ServiceOption {
	return func(s *ServiceConfig) {
		s.HealthCheck = &HealthCheck{
			Enabled:  true,
			Path:     path,
			Interval: interval.String(),
			Timeout:  "5s",
			Retries:  3,
		}
	}
}

func WithResources(cpu float64, memMB int) ServiceOption {
	return func(s *ServiceConfig) {
		s.Resources = &ResourceLimits{
			CPUCores: cpu,
			MemoryMB: memMB,
		}
	}
}

func WithLabels(labels map[string]string) ServiceOption {
	return func(s *ServiceConfig) {
		for k, v := range labels {
			s.Labels[k] = v
		}
	}
}

func WithDependencies(deps ...string) ServiceOption {
	return func(s *ServiceConfig) {
		s.DependsOn = append(s.DependsOn, deps...)
	}
}

func NewService(name string, port int, opts ...ServiceOption) *ServiceConfig {
	s := NewServiceConfig(name, port)
	for _, opt := range opts {
		opt(s)
	}
	return s
}

// ─── Validation ─────────────────────────────────────────────────

type ValidationError struct {
	Field   string
	Message string
}

func (ve ValidationError) Error() string {
	return fmt.Sprintf("%s: %s", ve.Field, ve.Message)
}

func (s *ServiceConfig) Validate() []ValidationError {
	var errs []ValidationError

	if s.Name == "" {
		errs = append(errs, ValidationError{"name", "required"})
	}
	if s.Port < 1 || s.Port > 65535 {
		errs = append(errs, ValidationError{"port", fmt.Sprintf("invalid: %d (must be 1-65535)", s.Port)})
	}
	if s.TLS != nil && s.TLS.Enabled {
		if s.TLS.CertFile == "" {
			errs = append(errs, ValidationError{"tls.cert_file", "required when TLS enabled"})
		}
		if s.TLS.KeyFile == "" {
			errs = append(errs, ValidationError{"tls.key_file", "required when TLS enabled"})
		}
	}
	if s.HealthCheck != nil && s.HealthCheck.Enabled {
		if s.HealthCheck.Path == "" {
			errs = append(errs, ValidationError{"health_check.path", "required when health check enabled"})
		}
	}
	if s.Resources != nil {
		if s.Resources.CPUCores <= 0 {
			errs = append(errs, ValidationError{"resources.cpu_cores", "must be positive"})
		}
		if s.Resources.MemoryMB <= 0 {
			errs = append(errs, ValidationError{"resources.memory_mb", "must be positive"})
		}
	}

	validEnvs := []Environment{EnvDev, EnvStaging, EnvProd}
	if !slices.Contains(validEnvs, s.Environment) {
		errs = append(errs, ValidationError{"environment", fmt.Sprintf("invalid: %q", s.Environment)})
	}

	return errs
}

// ─── Serialization ──────────────────────────────────────────────

func (s *ServiceConfig) ToJSON() (string, error) {
	data, err := json.MarshalIndent(s, "", "  ")
	if err != nil {
		return "", err
	}
	return string(data), nil
}

func LoadServiceConfig(data []byte) (*ServiceConfig, error) {
	var cfg ServiceConfig
	if err := json.Unmarshal(data, &cfg); err != nil {
		return nil, fmt.Errorf("parse config: %w", err)
	}

	// Inicializar maps si nil (el JSON podria no tener labels)
	if cfg.Labels == nil {
		cfg.Labels = make(map[string]string)
	}
	if cfg.Annotations == nil {
		cfg.Annotations = make(map[string]string)
	}
	cfg.loadedAt = time.Now()

	return &cfg, nil
}

// ─── Display ────────────────────────────────────────────────────

func (s *ServiceConfig) Summary() string {
	var b strings.Builder

	fmt.Fprintf(&b, "Service: %s v%s [%s]\n", s.Name, s.Version, s.Environment)
	fmt.Fprintf(&b, "  Listen: %s://%s:%d\n", s.Protocol, s.Host, s.Port)

	if s.TLS != nil && s.TLS.Enabled {
		fmt.Fprintf(&b, "  TLS: enabled (cert=%s)\n", s.TLS.CertFile)
	}

	if s.HealthCheck != nil && s.HealthCheck.Enabled {
		fmt.Fprintf(&b, "  Health: %s (every %s)\n", s.HealthCheck.Path, s.HealthCheck.Interval)
	}

	if s.Resources != nil {
		fmt.Fprintf(&b, "  Resources: %.1f CPU, %d MB RAM\n",
			s.Resources.CPUCores, s.Resources.MemoryMB)
	}

	if s.Logging != nil {
		fmt.Fprintf(&b, "  Logging: level=%s format=%s output=%s\n",
			s.Logging.Level, s.Logging.Format, s.Logging.Output)
	}

	if len(s.Labels) > 0 {
		labels := make([]string, 0, len(s.Labels))
		for k, v := range s.Labels {
			labels = append(labels, k+"="+v)
		}
		slices.Sort(labels)
		fmt.Fprintf(&b, "  Labels: %s\n", strings.Join(labels, ", "))
	}

	if len(s.DependsOn) > 0 {
		fmt.Fprintf(&b, "  Depends on: %s\n", strings.Join(s.DependsOn, ", "))
	}

	return b.String()
}

// ─── Fleet Config ───────────────────────────────────────────────

type FleetConfig struct {
	Name     string           `json:"name"`
	Services []*ServiceConfig `json:"services"`
}

func (f *FleetConfig) ValidateAll() map[string][]ValidationError {
	errors := make(map[string][]ValidationError)
	for _, svc := range f.Services {
		if errs := svc.Validate(); len(errs) > 0 {
			errors[svc.Name] = errs
		}
	}

	// Verificar dependencias
	names := make(map[string]bool, len(f.Services))
	for _, svc := range f.Services {
		names[svc.Name] = true
	}
	for _, svc := range f.Services {
		for _, dep := range svc.DependsOn {
			if !names[dep] {
				errors[svc.Name] = append(errors[svc.Name],
					ValidationError{"depends_on", fmt.Sprintf("unknown dependency: %q", dep)})
			}
		}
	}

	return errors
}

// ─── Main ───────────────────────────────────────────────────────

func main() {
	fmt.Println(strings.Repeat("═", 70))
	fmt.Println("  INFRASTRUCTURE CONFIGURATION MANAGER")
	fmt.Println(strings.Repeat("═", 70))

	// ═══ Crear servicios con functional options ═══
	fmt.Println("\n── Creating Services ──\n")

	db := NewService("postgres", 5432,
		WithEnvironment(EnvProd),
		WithResources(4.0, 8192),
		WithLabels(map[string]string{
			"tier": "data", "region": "us-east", "role": "primary",
		}),
	)
	db.Version = "14.2"

	cache := NewService("redis", 6379,
		WithEnvironment(EnvProd),
		WithResources(2.0, 4096),
		WithLabels(map[string]string{
			"tier": "data", "region": "us-east",
		}),
	)
	cache.Version = "7.2.0"

	api := NewService("api-server", 8443,
		WithEnvironment(EnvProd),
		WithTLS("/etc/ssl/api-cert.pem", "/etc/ssl/api-key.pem"),
		WithHealthCheck("/healthz", 10*time.Second),
		WithResources(2.0, 2048),
		WithDependencies("postgres", "redis"),
		WithLabels(map[string]string{
			"tier": "backend", "region": "us-east", "team": "platform",
		}),
	)
	api.Version = "2.5.0"

	web := NewService("web-frontend", 443,
		WithEnvironment(EnvProd),
		WithTLS("/etc/ssl/web-cert.pem", "/etc/ssl/web-key.pem"),
		WithHealthCheck("/", 30*time.Second),
		WithResources(1.0, 512),
		WithDependencies("api-server"),
		WithLabels(map[string]string{
			"tier": "frontend", "region": "us-east",
		}),
	)
	web.Version = "3.1.0"

	monitoring := NewService("prometheus", 9090,
		WithEnvironment(EnvProd),
		WithHealthCheck("/-/healthy", 15*time.Second),
		WithResources(2.0, 4096),
		WithLabels(map[string]string{
			"tier": "infra", "region": "us-east",
		}),
	)
	monitoring.Version = "2.45.0"

	// ═══ Display summaries ═══
	for _, svc := range []*ServiceConfig{db, cache, api, web, monitoring} {
		fmt.Println(svc.Summary())
	}

	// ═══ Fleet validation ═══
	fleet := &FleetConfig{
		Name:     "production-us-east",
		Services: []*ServiceConfig{db, cache, api, web, monitoring},
	}

	fmt.Println("── Fleet Validation ──\n")
	errors := fleet.ValidateAll()
	if len(errors) == 0 {
		fmt.Println("  All services passed validation ✓")
	} else {
		for name, errs := range errors {
			fmt.Printf("  %s:\n", name)
			for _, e := range errs {
				fmt.Printf("    ✗ %s\n", e)
			}
		}
	}

	// ═══ Serialize to JSON ═══
	fmt.Println("\n── Fleet JSON Export ──\n")
	fleetJSON, err := json.MarshalIndent(fleet, "", "  ")
	if err != nil {
		fmt.Fprintf(os.Stderr, "marshal error: %v\n", err)
		return
	}
	// Print just the first service for brevity
	fmt.Println("  (showing first service)")
	oneService, _ := json.MarshalIndent(fleet.Services[2], "", "  ")
	fmt.Println(string(oneService))

	// ═══ Deserialize from JSON ═══
	fmt.Println("\n── Deserialize & Verify ──\n")
	var loaded FleetConfig
	if err := json.Unmarshal(fleetJSON, &loaded); err != nil {
		fmt.Fprintf(os.Stderr, "unmarshal error: %v\n", err)
		return
	}
	fmt.Printf("  Loaded fleet %q with %d services\n", loaded.Name, len(loaded.Services))
	for _, svc := range loaded.Services {
		protocol := svc.Protocol
		if svc.TLS != nil && svc.TLS.Enabled {
			protocol = ProtoHTTPS
		}
		fmt.Printf("    %-16s %s://%s:%d  v%s\n",
			svc.Name, protocol, svc.Host, svc.Port, svc.Version)
	}

	// ═══ Optional fields demo ═══
	fmt.Println("\n── Optional Fields (nil vs present) ──\n")
	for _, svc := range fleet.Services {
		parts := []string{svc.Name + ":"}
		if svc.TLS != nil {
			parts = append(parts, "TLS=✓")
		} else {
			parts = append(parts, "TLS=✗")
		}
		if svc.HealthCheck != nil {
			parts = append(parts, fmt.Sprintf("HC=%s", svc.HealthCheck.Path))
		} else {
			parts = append(parts, "HC=none")
		}
		if svc.Resources != nil {
			parts = append(parts, fmt.Sprintf("CPU=%.0f MEM=%dMB",
				svc.Resources.CPUCores, svc.Resources.MemoryMB))
		}
		fmt.Printf("  %s\n", strings.Join(parts, "  "))
	}

	// ═══ Struct patterns used ═══
	fmt.Println("\n── Struct Patterns Used ──")
	patterns := []string{
		"Named fields initialization     — ServiceConfig{Name: ..., Port: ...}",
		"Functional options constructor  — NewService(name, port, opts...)",
		"Pointer for optional substructs — *TLSConfig, *HealthCheck (nil = not set)",
		"Struct tags for JSON            — `json:\"name,omitempty\"`",
		"Custom enum types               — type Environment string",
		"Validation method               — Validate() []ValidationError",
		"Builder pattern                 — ServerConfigBuilder.Host().Port().Build()",
		"Unexported fields               — configPath, loadedAt (internal state)",
		"Deep struct composition         — FleetConfig → []ServiceConfig → substructs",
		"MarshalIndent for pretty output — json.MarshalIndent(cfg, \"\", \"  \")",
		"Nil checks for optional fields  — if svc.TLS != nil { ... }",
		"Map fields with make init       — Labels: make(map[string]string)",
	}
	for _, p := range patterns {
		fmt.Printf("  • %s\n", p)
	}
}
```

---

## 14. Tabla de errores comunes

```
┌────┬──────────────────────────────────────────┬──────────────────────────────────────────┬─────────┐
│ #  │ Error                                    │ Solucion                                 │ Nivel   │
├────┼──────────────────────────────────────────┼──────────────────────────────────────────┼─────────┤
│  1 │ Nil map en struct field (panic al write) │ Inicializar en constructor con make()    │ Panic   │
│  2 │ Inicializacion posicional (fragil)       │ Siempre usar campos nombrados            │ Maint.  │
│  3 │ Copiar struct con sync.Mutex             │ Nunca copiar — usar puntero. go vet      │ Race    │
│    │                                          │ detecta copias de sync types             │         │
│  4 │ Pasar struct grande por valor            │ Pasar puntero *T para evitar copia       │ Perf    │
│  5 │ Struct tag mal formado                   │ go vet detecta. Formato: `key:"val"`     │ Compile │
│  6 │ omitempty con struct (no funciona)       │ Usar *Struct para que nil = omit          │ Logic   │
│  7 │ JSON Unmarshal sin puntero               │ json.Unmarshal(data, &cfg) — necesita &   │ Compile │
│  8 │ Shallow copy corrompe map/slice fields   │ Implementar Clone() con deep copy         │ Logic   │
│  9 │ Exportar campo que deberia ser privado   │ Minuscula para campos con invariantes     │ Design  │
│ 10 │ GetX() como nombre de getter             │ Go idiomatico: X() no GetX()              │ Style   │
│ 11 │ No validar en constructor                │ NewX() o NewXValidated() con validacion   │ Logic   │
│ 12 │ Struct vacio como zero value inutilizable│ Disenar structs con zero value usable     │ Design  │
│ 13 │ Padding excesivo por orden de campos     │ Ordenar campos grandes primero (si perf.) │ Perf    │
│ 14 │ JSON field names en PascalCase           │ Usar tags: `json:"snake_case"`            │ API     │
└────┴──────────────────────────────────────────┴──────────────────────────────────────────┴─────────┘
```

---

## 15. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
type S struct {
    X int
    Y string
}
var s S
fmt.Println(s.X, s.Y == "")
```

<details>
<summary>Respuesta</summary>

```
0 true
```

Zero value de struct: cada campo recibe su zero value. `X = 0` (int), `Y = ""` (string). `"" == ""` es `true`.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
type Point struct{ X, Y int }
p1 := Point{3, 4}
p2 := p1
p2.X = 99
fmt.Println(p1.X)
```

<details>
<summary>Respuesta</summary>

```
3
```

Structs son tipos valor. `p2 := p1` crea una copia. Modificar `p2.X` no afecta `p1`.
</details>

**Ejercicio 3**: ¿Compila?

```go
type Config struct {
    Name   string
    Values []int
}
c1 := Config{Name: "a", Values: []int{1, 2}}
c2 := Config{Name: "a", Values: []int{1, 2}}
fmt.Println(c1 == c2)
```

<details>
<summary>Respuesta</summary>

No compila. Error: `struct containing []int cannot be compared using ==`. El campo `Values` es un slice, que no es comparable. Usar `reflect.DeepEqual(c1, c2)` o un metodo Equal manual.
</details>

**Ejercicio 4**: ¿Que imprime?

```go
type S struct {
    A int `json:"alpha"`
    B int `json:"-"`
    C int `json:"gamma,omitempty"`
}
s := S{A: 1, B: 2, C: 0}
data, _ := json.Marshal(s)
fmt.Println(string(data))
```

<details>
<summary>Respuesta</summary>

```
{"alpha":1}
```

`A` se serializa como "alpha" (tag). `B` tiene `json:"-"` → se omite siempre. `C` tiene `omitempty` y su valor es 0 (zero value de int) → se omite.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
type Server struct {
    Name string
    Port int
}
s := &Server{Name: "web01", Port: 80}
s.Port = 8080
fmt.Println(s.Port)
```

<details>
<summary>Respuesta</summary>

```
8080
```

Go auto-dereferencia punteros a struct. `s.Port` es equivalente a `(*s).Port`. Se modifica directamente.
</details>

**Ejercicio 6**: ¿Que imprime?

```go
type Config struct {
    Labels map[string]string
}
c1 := Config{Labels: map[string]string{"env": "prod"}}
c2 := c1
c2.Labels["env"] = "dev"
fmt.Println(c1.Labels["env"])
```

<details>
<summary>Respuesta</summary>

```
dev
```

Copia shallow. `c2 := c1` copia el struct, pero `Labels` es un map (referencia). Ambos apuntan al mismo map. Modificar `c2.Labels` afecta `c1.Labels`.
</details>

**Ejercicio 7**: ¿Que imprime?

```go
type S struct{}
fmt.Println(unsafe.Sizeof(S{}))
```

<details>
<summary>Respuesta</summary>

```
0
```

Un struct vacio (`struct{}`) ocupa exactamente 0 bytes. Por eso se usa como valor en `map[T]struct{}` para sets eficientes.
</details>

**Ejercicio 8**: ¿Que JSON produce?

```go
type Response struct {
    Items  []string `json:"items"`
    Error  *string  `json:"error,omitempty"`
}
r := Response{Items: []string{}}
data, _ := json.Marshal(r)
fmt.Println(string(data))
```

<details>
<summary>Respuesta</summary>

```
{"items":[]}
```

`Items` es un empty slice (no nil) → se serializa como `[]`. `Error` es `*string` nil → `omitempty` lo omite. Si `Items` fuera `nil`, seria `"items":null`.
</details>

**Ejercicio 9**: ¿Que imprime?

```go
type Bad struct {
    A bool
    B int64
    C bool
}
type Good struct {
    B int64
    A bool
    C bool
}
fmt.Println(unsafe.Sizeof(Bad{}), unsafe.Sizeof(Good{}))
```

<details>
<summary>Respuesta</summary>

```
24 16
```

`Bad`: A(1) + padding(7) + B(8) + C(1) + padding(7) = 24 bytes. `Good`: B(8) + A(1) + C(1) + padding(6) = 16 bytes. El orden de campos afecta el padding.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
var s *Server
fmt.Println(s == nil)
```

```go
type Server struct {
    Name string
}

func (s *Server) IsNil() bool {
    return s == nil
}
```

<details>
<summary>Respuesta</summary>

```
true
```

Un puntero a struct puede ser nil. `s == nil` retorna true. El metodo `IsNil()` tambien funciona en nil receiver — Go permite llamar metodos en nil pointers (mientras el metodo no acceda a campos sin verificar nil).
</details>

---

## 16. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              STRUCTS — RESUMEN                               │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DECLARACION                                                 │
│  type T struct { Field Type `tag:"value"` }                  │
│  struct{} → 0 bytes (sets, signals)                          │
│  struct anonimo: s := struct{X int}{42}                      │
│                                                              │
│  INICIALIZACION                                              │
│  ✓ T{Field: val}  — campos nombrados (recomendado)           │
│  ✗ T{val1, val2}  — posicional (fragil, evitar)              │
│  ✓ &T{Field: val} — puntero con inicializacion               │
│  ✓ new(T)          — puntero con zero value (raro)            │
│  ✓ NewT()          — constructor (convencion)                 │
│                                                              │
│  VISIBILIDAD                                                 │
│  Mayuscula → Exportado (public across packages)              │
│  minuscula → no exportado (private to package)               │
│  Getter: Name() no GetName()                                 │
│                                                              │
│  STRUCT TAGS                                                 │
│  `json:"name,omitempty"` — renombrar, omitir zero value     │
│  `json:"-"` — nunca serializar                               │
│  `yaml:"name"` `db:"column"` `validate:"required"`          │
│  Multiples: `json:"n" yaml:"n" env:"N"`                     │
│                                                              │
│  TIPO VALOR                                                  │
│  ├─ Se copia al asignar: s2 := s1 (copia completa)          │
│  ├─ Se copia al pasar a func: f(s) no modifica s            │
│  ├─ Shallow copy: map/slice fields siguen compartidos        │
│  └─ Deep copy: metodo Clone() manual                         │
│                                                              │
│  COMPARACION                                                 │
│  ├─ == funciona si TODOS los campos son comparables          │
│  ├─ Slices/maps en campos → struct no comparable             │
│  └─ reflect.DeepEqual o metodo Equal manual                  │
│                                                              │
│  PUNTEROS                                                    │
│  ├─ &T{} para pasar por referencia (evitar copias)           │
│  ├─ Auto-deref: s.Field equiv a (*s).Field                   │
│  ├─ nil pointer → panic al acceder campos                    │
│  ├─ *Struct para campos opcionales (nil = no configurado)    │
│  └─ Metodos en nil receiver son validos (con nil check)      │
│                                                              │
│  MEMORY LAYOUT                                               │
│  ├─ Padding por alignment (1/2/4/8 bytes)                    │
│  ├─ Ordenar campos por tamaño descendente reduce padding     │
│  └─ Solo optimizar si hay muchas instancias (>100K)          │
│                                                              │
│  JSON AVANZADO                                               │
│  ├─ MarshalJSON/UnmarshalJSON para control total             │
│  ├─ json.RawMessage para parseo diferido                     │
│  ├─ Decoder/Encoder para streaming                           │
│  └─ omitempty: funciona con *Struct, no con Struct           │
│                                                              │
│  PATRONES                                                    │
│  ├─ Constructor: NewX() / NewX(opts...)                      │
│  ├─ Functional options: WithTLS(), WithLabels()              │
│  ├─ Builder: Builder.Field(v).Build()                        │
│  ├─ Result type: {Data, Error, Metadata}                     │
│  ├─ Config from multiple sources: default→file→env→flags     │
│  └─ Table-driven tests: []struct{name, input, expected}      │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T03 - Embedding — composicion sobre herencia, promoted methods, embedding de interfaces
