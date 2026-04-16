# func — Declaracion y Multiples Retornos

## 1. Introduccion

Las funciones en Go son **ciudadanos de primera clase** — pueden asignarse a variables, pasarse como argumentos y retornarse desde otras funciones (esto se profundizara en T03). Pero antes de llegar ahi, necesitas dominar la mecanica fundamental: como se declaran, como manejan multiples valores de retorno, y por que `(value, error)` es el patron que define Go.

Has usado funciones a lo largo de todo el curso sin una explicacion formal. Este topico cubre la mecanica completa.

```
┌──────────────────────────────────────────────────────────────┐
│              FUNCIONES EN GO                                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  func name(params) returnType { body }                       │
│                                                              │
│  Caracteristicas:                                            │
│  ├─ Multiples valores de retorno (no es tupla, son valores)  │
│  ├─ Named returns: retornos con nombre (utiles con defer)    │
│  ├─ Sin overloading (no puedes tener dos func con mismo      │
│  │   nombre y diferentes parametros)                         │
│  ├─ Sin valores por defecto para parametros                  │
│  ├─ Sin generics en Go < 1.18 (ahora si con type params)    │
│  ├─ Funciones son valores (first-class citizens)             │
│  ├─ Closures soportados                                     │
│  └─ No hay keywords: public/private/static/virtual/override  │
│     La visibilidad se controla con mayuscula/minuscula        │
│                                                              │
│  Firma completa:                                             │
│  func Name(p1 T1, p2 T2) (r1 R1, r2 R2) { body }           │
│       ↑        ↑                 ↑                           │
│    nombre   parametros    retornos (opcionalmente nombrados) │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Sintaxis de declaracion

### Funcion sin retorno

```go
func greet(name string) {
    fmt.Printf("Hello, %s!\n", name)
}

// Sin parametros, sin retorno
func printSeparator() {
    fmt.Println(strings.Repeat("─", 60))
}
```

### Funcion con un retorno

```go
func add(a, b int) int {
    return a + b
}

// Parametros del mismo tipo se pueden agrupar
func fullName(first, last string) string {
    return first + " " + last
}
```

### Funcion con multiples retornos

```go
func divide(a, b float64) (float64, error) {
    if b == 0 {
        return 0, errors.New("division by zero")
    }
    return a / b, nil
}

result, err := divide(10, 3)
if err != nil {
    log.Fatal(err)
}
fmt.Println(result)  // 3.3333...
```

### Anatomia detallada

```go
func processConfig(path string, validate bool) (Config, error) {
//   ↑              ↑              ↑            ↑       ↑
//   keyword     nombre      parametros    ret1 tipo  ret2 tipo
//
// - func es obligatorio (no hay "def", "fn", "function")
// - nombre sigue convenciones: camelCase (unexported), PascalCase (exported)
// - parametros: nombre tipo (no tipo nombre como en C/Java)
// - retorno unico: tipo directo (sin parentesis)
// - retornos multiples: (tipo1, tipo2) con parentesis
// - cuerpo: siempre entre llaves { }, llave de apertura en la misma linea

    data, err := os.ReadFile(path)
    if err != nil {
        return Config{}, fmt.Errorf("read %s: %w", path, err)
    }

    var cfg Config
    if err := json.Unmarshal(data, &cfg); err != nil {
        return Config{}, fmt.Errorf("parse %s: %w", path, err)
    }

    if validate {
        if err := cfg.Validate(); err != nil {
            return Config{}, fmt.Errorf("validate: %w", err)
        }
    }

    return cfg, nil
}
```

---

## 3. Parametros — Pass by value

Go pasa **todo** por valor. Cuando pasas un argumento, se hace una copia:

```go
func increment(x int) {
    x++  // modifica la COPIA local
}

n := 5
increment(n)
fmt.Println(n)  // 5 — sin cambio, n no se modifico
```

### Para modificar el original, usa punteros

```go
func increment(x *int) {
    *x++  // modifica el valor al que apunta x
}

n := 5
increment(&n)
fmt.Println(n)  // 6 — modificado via puntero
```

### Tipos que "parecen" pasar por referencia

Algunos tipos contienen punteros internamente, asi que aunque se copian, la copia apunta a los mismos datos:

```go
// Slices: la copia comparte el array subyacente
func appendItem(s []string, item string) {
    s = append(s, item)  // puede o no afectar al caller
    // Si el append necesita realocar, el caller NO ve el cambio
    // Si cabe en la capacidad, el caller VE los datos pero no el len
}

// Maps: la copia apunta al mismo hash table
func addEntry(m map[string]int, key string, val int) {
    m[key] = val  // el caller VE este cambio
}

// Channels: la copia apunta al mismo channel
func send(ch chan int, val int) {
    ch <- val  // funciona, el caller recibe
}
```

```
┌──────────────────────────────────────────────────────────────┐
│  PASS BY VALUE — Que se copia:                               │
│                                                              │
│  Tipo               Se copia           Efecto                │
│  ──────────────────────────────────────────────────────────   │
│  int, float, bool   El valor entero    No afecta original    │
│  string             Header (ptr+len)   Inmutable, no importa │
│  array [N]T         TODO el array      No afecta original    │
│  struct             TODOS los campos   No afecta original    │
│  *T (puntero)       La direccion       Modificas el original │
│  slice []T          Header (ptr+len+cap) Comparte datos      │
│  map                Puntero interno    Comparte datos        │
│  chan               Puntero interno    Comparte canal         │
│  func               Puntero interno    Comparte funcion       │
│  interface          (type, value)      Comparte valor         │
│                                                              │
│  Regla: si necesitas modificar → usa puntero                 │
│  Regla: structs grandes (>64 bytes) → usa puntero por        │
│         eficiencia (evita copia costosa)                     │
└──────────────────────────────────────────────────────────────┘
```

### Patron SysAdmin: Config como puntero vs valor

```go
// Config pequeña — pasar por valor esta bien
type SmallConfig struct {
    Port    int
    Debug   bool
    LogLevel string
}

func startServer(cfg SmallConfig) { /* ... */ }

// Config grande — pasar por puntero es mas eficiente
type LargeConfig struct {
    // docenas de campos, nested structs, slices...
    Database    DatabaseConfig
    Cache       CacheConfig
    Auth        AuthConfig
    Monitoring  MonitoringConfig
    Features    map[string]bool
    AllowedIPs  []string
    // ...
}

func startServer(cfg *LargeConfig) { /* ... */ }
// La copia de un puntero son 8 bytes
// La copia de LargeConfig podrian ser cientos de bytes
```

---

## 4. Multiples valores de retorno — El patron que define Go

Go permite retornar multiples valores desde una funcion. Esto no es una tupla — son valores separados que se asignan individualmente:

```go
func stat(path string) (os.FileInfo, error) {
    return os.Stat(path)
}

info, err := stat("/etc/hosts")
// info y err son variables SEPARADAS, no miembros de una tupla
```

### El patron (value, error)

Este es el patron mas importante de Go. Practicamente toda funcion que puede fallar retorna `(result, error)`:

```go
// Patrones comunes en la stdlib:
file, err := os.Open(path)
data, err := io.ReadAll(reader)
conn, err := net.Dial("tcp", addr)
resp, err := http.Get(url)
n, err := fmt.Fprintf(w, "hello")
result, err := strconv.Atoi("42")
value, ok := myMap[key]          // variante: comma ok
value, ok := x.(string)         // variante: type assertion
```

### Multiples valores que no son errores

```go
// Retornar dos valores semanticos
func minMax(nums []int) (int, int) {
    if len(nums) == 0 {
        return 0, 0
    }
    min, max := nums[0], nums[0]
    for _, n := range nums[1:] {
        if n < min {
            min = n
        }
        if n > max {
            max = n
        }
    }
    return min, max
}

lo, hi := minMax([]int{3, 1, 4, 1, 5, 9})
fmt.Printf("min=%d max=%d\n", lo, hi)  // min=1 max=9
```

### Tres o mas retornos (raro pero valido)

```go
// La stdlib lo hace en algunos casos:
func net.SplitHostPort(hostport string) (host, port string, err error)

// Patron SysAdmin: leer metricas
func readLoadAvg() (load1, load5, load15 float64, err error) {
    data, err := os.ReadFile("/proc/loadavg")
    if err != nil {
        return 0, 0, 0, err
    }
    fields := strings.Fields(string(data))
    if len(fields) < 3 {
        return 0, 0, 0, fmt.Errorf("unexpected loadavg format: %q", data)
    }
    load1, _ = strconv.ParseFloat(fields[0], 64)
    load5, _ = strconv.ParseFloat(fields[1], 64)
    load15, _ = strconv.ParseFloat(fields[2], 64)
    return load1, load5, load15, nil
}

l1, l5, l15, err := readLoadAvg()
if err != nil {
    log.Fatal(err)
}
fmt.Printf("Load: %.2f / %.2f / %.2f\n", l1, l5, l15)
```

### Cuando usar mas de 2 retornos

```
┌──────────────────────────────────────────────────────────────┐
│  Numero de retornos — Guia:                                  │
│                                                              │
│  0 retornos: side-effects puros (log, print, write to chan)  │
│  1 retorno:  computacion pura (add, format, hash)            │
│  2 retornos: (value, error) o (value, ok) — LO MAS COMUN    │
│  3 retornos: raro, solo si son valores semanticamente        │
│              relacionados (load1, load5, load15)             │
│  4+ retornos: code smell — considera retornar un struct      │
│                                                              │
│  Si necesitas 4+ valores, usa un struct:                     │
│  ✗ func stats() (min, max, avg, median, p95, p99 float64)   │
│  ✓ func stats() Stats  // Stats tiene campos con nombre     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 5. Named return values — Retornos con nombre

Puedes dar nombre a los valores de retorno. Esto los declara como variables locales inicializadas a su zero value:

```go
func divide(a, b float64) (result float64, err error) {
    // 'result' ya existe como variable local, inicializada a 0.0
    // 'err' ya existe como variable local, inicializada a nil

    if b == 0 {
        err = errors.New("division by zero")
        return  // naked return — retorna result=0.0, err=error
    }

    result = a / b
    return  // naked return — retorna result=valor, err=nil
}
```

### Named returns son variables locales

```go
func example() (x int, y string) {
    // x y y ya existen como si hubieran sido declarados:
    // var x int    // = 0
    // var y string // = ""

    x = 42
    y = "hello"
    return  // retorna 42, "hello"
}
```

### Named returns con return explicito

```go
// PUEDES usar return con valores explicitos aunque tengas named returns
func divide(a, b float64) (result float64, err error) {
    if b == 0 {
        return 0, errors.New("division by zero")  // valores explicitos
    }
    return a / b, nil  // valores explicitos
}
// Esto es PERFECTAMENTE VALIDO y a menudo preferido sobre naked return
```

---

## 6. Naked return — Por que evitarlo

Un "naked return" es un `return` sin valores en una funcion con named returns. Retorna los valores actuales de las variables nombradas:

```go
func process(data []byte) (result string, err error) {
    if len(data) == 0 {
        err = errors.New("empty data")
        return  // ← naked return: retorna result="", err=error
    }

    result = string(data)
    return  // ← naked return: retorna result=string, err=nil
}
```

### Por que es problematico

```go
// En funciones cortas — aceptable
func isPositive(n int) (result bool) {
    result = n > 0
    return  // claro porque la funcion es de 2 lineas
}

// En funciones largas — PELIGROSO
func processConfig(path string) (cfg Config, err error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return  // ← ¿que retorna? necesitas ir a la firma para saber
    }

    // ... 50 lineas de procesamiento ...

    if validate {
        // ... 20 lineas mas ...
        if err = checkSchema(cfg); err != nil {
            return  // ← ¿que retorna cfg? ¿esta a medias? ¿esta completo?
        }
    }

    // ... 30 lineas mas ...

    return  // ← ¿que retorna? hay que leer TODA la funcion
}
// En una funcion larga, naked return hace imposible saber
// que se retorna sin leer TODO el codigo
```

### La recomendacion oficial de Go

```
┌──────────────────────────────────────────────────────────────┐
│  Naked returns:                                              │
│                                                              │
│  ✓ Aceptable en funciones CORTAS (< 10 lineas)              │
│  ✗ Evitar en funciones largas                                │
│  ✗ Evitar cuando hay multiples return paths                  │
│                                                              │
│  Named returns SIN naked return:                             │
│  ✓ Siempre aceptable — documentan el significado del retorno │
│  ✓ Necesarios para defer que modifica return values          │
│  ✓ Utiles como documentacion inline                         │
│                                                              │
│  golangci-lint: nakedret linter detecta naked returns        │
│  en funciones de mas de N lineas (configurable)              │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Named returns como documentacion

```go
// Sin named returns — ¿que significan los retornos?
func ParseAddr(s string) (string, int, error)
// ¿El string es host o IP? ¿El int es port o error code?

// Con named returns — auto-documentado
func ParseAddr(s string) (host string, port int, err error)
// Ahora es obvio que retorna
```

---

## 7. Cuando usar named returns

### Caso 1: defer que modifica el error (el caso principal)

```go
func readConfig(path string) (cfg Config, err error) {
    defer func() {
        if err != nil {
            err = fmt.Errorf("readConfig(%s): %w", path, err)
        }
    }()

    data, err := os.ReadFile(path)
    if err != nil {
        return Config{}, err  // defer enriquece el error
    }

    if err = json.Unmarshal(data, &cfg); err != nil {
        return Config{}, err  // defer enriquece el error
    }

    return cfg, nil
}
```

### Caso 2: Capturar error de Close en escritura

```go
func writeData(path string, data []byte) (err error) {
    f, err := os.Create(path)
    if err != nil {
        return err
    }

    defer func() {
        closeErr := f.Close()
        if err == nil {
            err = closeErr
        }
    }()

    _, err = f.Write(data)
    return err
}
```

### Caso 3: Documentacion de retornos

```go
// Los named returns explican que significan los valores
func SplitHostPort(hostport string) (host string, port string, err error)
func ReadMemInfo() (totalMB int64, freeMB int64, err error)
func GetDiskUsage(path string) (usedBytes int64, totalBytes int64, err error)
```

### Caso 4: Recover que convierte panic a error

```go
func safeProcess(input []byte) (result []byte, err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic: %v", r)
        }
    }()

    result = process(input)
    return result, nil
}
```

---

## 8. El blank identifier _ — Descartar valores

Cuando una funcion retorna multiples valores y no necesitas todos, usas `_`:

```go
// Descartar el error (NO RECOMENDADO en la mayoria de casos)
data, _ := os.ReadFile("config.yaml")

// Descartar el valor, solo queremos saber si la key existe
_, exists := myMap["key"]

// Descartar el indice en range
for _, value := range items {
    fmt.Println(value)
}

// Descartar el valor en range (solo queremos el indice)
for i := range items {
    fmt.Println(i)
}

// Descartar multiples valores
_, _, err := net.SplitHostPort("localhost:8080")
```

### Cuando es aceptable ignorar errores

```go
// ✓ Aceptable — errores que genuinamente no importan
fmt.Println("hello")           // el error de stdout raramente importa
_ = fmt.Fprintf(w, "log: %s", msg)  // logging best-effort
defer f.Close()                // error de Close al LEER (no escribir)
defer resp.Body.Close()        // limpieza best-effort

// ✗ NO aceptable — errores que pueden indicar problemas
data, _ := os.ReadFile(path)           // ¿y si el archivo no existe?
_, _ = conn.Write(data)                // ¿y si la conexion se cerro?
_ = json.Unmarshal(data, &config)      // ¿y si el JSON es invalido?
```

### _ como import (side-effect import)

```go
// Importar un paquete solo por sus side effects (init())
import _ "net/http/pprof"    // registra handlers de profiling
import _ "image/png"         // registra decoder PNG
import _ "github.com/lib/pq" // registra driver PostgreSQL

// El _ dice "se que no uso el nombre del paquete, pero lo necesito"
```

### _ como verificacion de interface

```go
// Verificar en compile time que un tipo satisface una interface
var _ http.Handler = (*MyServer)(nil)
var _ io.ReadCloser = (*MyReader)(nil)
var _ fmt.Stringer = ServiceState(0)

// Si MyServer no implementa http.Handler, error de compilacion
// El _ descarta el valor — solo queremos la verificacion
```

---

## 9. Visibilidad — Exported vs Unexported

Go no tiene keywords de visibilidad (`public`, `private`, `protected`). Usa la primera letra del nombre:

```go
// EXPORTADO — visible fuera del paquete (PascalCase)
func ParseConfig(data []byte) (Config, error) { ... }
func NewServer(addr string) *Server { ... }
func (s *Server) Start() error { ... }

// NO EXPORTADO — solo visible dentro del paquete (camelCase)
func parseConfigInternal(data []byte) (config, error) { ... }
func newConnection(addr string) *connection { ... }
func (s *Server) handleRequest(w http.ResponseWriter, r *http.Request) { ... }
```

### Reglas de visibilidad para funciones

```
┌──────────────────────────────────────────────────────────────┐
│  Visibilidad en Go:                                          │
│                                                              │
│  func DoWork()     → Exportada (visible fuera del paquete)   │
│  func doWork()     → No exportada (solo dentro del paquete)  │
│                                                              │
│  Aplica a TODOS los identificadores:                         │
│  ├─ Funciones: ParseConfig vs parseConfig                    │
│  ├─ Tipos: Server vs server                                  │
│  ├─ Variables: MaxRetries vs maxRetries                       │
│  ├─ Constantes: DefaultPort vs defaultPort                   │
│  ├─ Campos de struct: Name vs name                           │
│  └─ Metodos: Start() vs start()                              │
│                                                              │
│  No hay "protected" ni "friend" ni "internal"                │
│  Un paquete es la unidad de encapsulacion                    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Patron SysAdmin: API publica vs implementacion interna

```go
package monitor

// ── API Publica (Exportada) ──────────────────────────────────

// CheckHealth verifica la salud de un servicio.
func CheckHealth(host string, port int) (HealthResult, error) {
    return checkTCP(host, port)
}

// NewMonitor crea un nuevo monitor con la configuracion dada.
func NewMonitor(cfg Config) *Monitor {
    return &Monitor{
        cfg:    cfg,
        client: newHTTPClient(cfg.Timeout),
    }
}

// ── Implementacion Interna (No exportada) ────────────────────

func checkTCP(host string, port int) (HealthResult, error) {
    // ... implementacion ...
}

func newHTTPClient(timeout time.Duration) *http.Client {
    // ... implementacion ...
}

func parseHealthResponse(data []byte) (healthData, error) {
    // ... implementacion ...
}
```

---

## 10. Patron (value, error) — La columna vertebral de Go

### Por que Go usa error como valor de retorno

```
┌──────────────────────────────────────────────────────────────┐
│  Alternativas que Go RECHAZO:                                │
│                                                              │
│  1. Excepciones (Java, Python, C++):                         │
│     ✗ Control flow invisible                                 │
│     ✗ ¿Que excepciones puede lanzar esta funcion?            │
│     ✗ try/catch anidados son dificiles de leer               │
│     Go: "Los errores son valores, no excepciones"            │
│                                                              │
│  2. Result<T, E> (Rust):                                     │
│     ✓ Obliga a manejar el error                              │
│     ✗ Requiere generics y un type system mas complejo        │
│     Go: "Lo consideramos, pero (value, error) es mas simple" │
│                                                              │
│  3. errno (C):                                               │
│     ✗ Estado global, facil de ignorar                        │
│     ✗ No thread-safe en implementaciones viejas              │
│     Go: "Los errores deben ser locales y explicitos"         │
│                                                              │
│  Go eligio: multiples return values + interface error        │
│  Simple, explicito, sin magia                                │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Los patrones de error handling

```go
// Patron 1: Propagar con contexto (lo mas comun)
func loadUsers(path string) ([]User, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, fmt.Errorf("load users from %s: %w", path, err)
    }

    var users []User
    if err := json.Unmarshal(data, &users); err != nil {
        return nil, fmt.Errorf("parse users: %w", err)
    }

    return users, nil
}

// Patron 2: Manejar el error y continuar
func tryConnect(hosts []string) *Connection {
    for _, host := range hosts {
        conn, err := connect(host)
        if err != nil {
            log.Printf("warning: %s unreachable: %v", host, err)
            continue  // intentar el siguiente
        }
        return conn
    }
    return nil  // ningun host disponible
}

// Patron 3: Sentinel errors
var ErrNotFound = errors.New("not found")
var ErrUnauthorized = errors.New("unauthorized")

func getUser(id int) (User, error) {
    // ...
    if !exists {
        return User{}, ErrNotFound
    }
    return user, nil
}

// El caller puede comparar:
user, err := getUser(42)
if errors.Is(err, ErrNotFound) {
    http.Error(w, "user not found", 404)
    return
}

// Patron 4: Error types con informacion extra
type ValidationError struct {
    Field   string
    Message string
}

func (e *ValidationError) Error() string {
    return fmt.Sprintf("validation: %s: %s", e.Field, e.Message)
}

func validatePort(port int) error {
    if port < 1 || port > 65535 {
        return &ValidationError{
            Field:   "port",
            Message: fmt.Sprintf("%d is out of range (1-65535)", port),
        }
    }
    return nil
}
```

### Patron SysAdmin: Chain de errores con contexto

```go
func startService(name string) error {
    cfg, err := loadServiceConfig(name)
    if err != nil {
        return fmt.Errorf("start %s: %w", name, err)
    }

    if err := validateConfig(cfg); err != nil {
        return fmt.Errorf("start %s: %w", name, err)
    }

    if err := allocateResources(cfg); err != nil {
        return fmt.Errorf("start %s: %w", name, err)
    }

    if err := launchProcess(cfg); err != nil {
        return fmt.Errorf("start %s: %w", name, err)
    }

    return nil
}

// El error chain resultante:
// "start nginx: launch: exec: permission denied"
// Cada nivel agrega contexto
```

---

## 11. El comma-ok idiom

Un patron especial de dos retornos donde el segundo valor es `bool`:

```go
// Map access
value, ok := myMap[key]
if !ok {
    fmt.Println("key not found")
}

// Type assertion
str, ok := x.(string)
if !ok {
    fmt.Println("x is not a string")
}

// Channel receive
value, ok := <-ch
if !ok {
    fmt.Println("channel closed")
}

// Interface check
writer, ok := x.(io.Writer)
if !ok {
    fmt.Println("does not implement io.Writer")
}
```

### Diferencia entre (value, ok) y (value, error)

```go
// (value, ok) — binario: existe o no existe
value, ok := cache.Get("key")

// (value, error) — puede fallar de multiples formas
value, err := db.Query("SELECT ...")
// err puede ser: connection lost, timeout, syntax error, etc.

// Regla: usa bool cuando el unico "error" es "no encontrado"
// Regla: usa error cuando puede haber multiples tipos de fallo
```

### Patron: Funcion con comma-ok propia

```go
func findProcess(name string) (pid int, found bool) {
    entries, err := os.ReadDir("/proc")
    if err != nil {
        return 0, false
    }

    for _, entry := range entries {
        pid, err := strconv.Atoi(entry.Name())
        if err != nil {
            continue
        }

        cmdline, err := os.ReadFile(fmt.Sprintf("/proc/%d/comm", pid))
        if err != nil {
            continue
        }

        if strings.TrimSpace(string(cmdline)) == name {
            return pid, true
        }
    }

    return 0, false
}

if pid, found := findProcess("nginx"); found {
    fmt.Printf("nginx is running with PID %d\n", pid)
} else {
    fmt.Println("nginx is not running")
}
```

---

## 12. Funciones como tipos

Go tiene un sistema de tipos para funciones. Puedes declarar tipos de funciones y usarlos como parametros:

```go
// Declaracion de un tipo de funcion
type HandlerFunc func(http.ResponseWriter, *http.Request)
type Middleware func(http.Handler) http.Handler
type Validator func(string) error
type Transformer func([]byte) ([]byte, error)

// Usando el tipo en parametros
func applyMiddleware(h http.Handler, middlewares ...Middleware) http.Handler {
    for i := len(middlewares) - 1; i >= 0; i-- {
        h = middlewares[i](h)
    }
    return h
}
```

### Firma de funcion como contrato

```go
// Las firmas de funcion actuan como interfaces implicitas
// Cualquier funcion con la misma firma puede asignarse al tipo

type CheckFunc func(host string, port int) error

func checkTCP(host string, port int) error {
    conn, err := net.DialTimeout("tcp", fmt.Sprintf("%s:%d", host, port), 2*time.Second)
    if err != nil {
        return err
    }
    return conn.Close()
}

func checkHTTP(host string, port int) error {
    url := fmt.Sprintf("http://%s:%d/health", host, port)
    resp, err := http.Get(url)
    if err != nil {
        return err
    }
    defer resp.Body.Close()
    if resp.StatusCode != 200 {
        return fmt.Errorf("status %d", resp.StatusCode)
    }
    return nil
}

// Ambas funciones satisfacen CheckFunc
var checks = map[string]CheckFunc{
    "tcp":  checkTCP,
    "http": checkHTTP,
}
```

---

## 13. No hay overloading ni defaults

Go no tiene sobrecarga de funciones ni valores por defecto para parametros:

```go
// ✗ NO existe en Go — overloading
func connect(host string) { ... }
func connect(host string, port int) { ... }  // ERROR: connect redeclared

// ✗ NO existe en Go — default values
func connect(host string, port int = 8080) { ... }  // ERROR: syntax error
```

### Alternativa 1: Functional options (patron idiomatico)

```go
type ServerOption func(*Server)

func WithPort(port int) ServerOption {
    return func(s *Server) {
        s.port = port
    }
}

func WithTimeout(d time.Duration) ServerOption {
    return func(s *Server) {
        s.timeout = d
    }
}

func WithTLS(certFile, keyFile string) ServerOption {
    return func(s *Server) {
        s.certFile = certFile
        s.keyFile = keyFile
    }
}

func NewServer(addr string, opts ...ServerOption) *Server {
    s := &Server{
        addr:    addr,
        port:    8080,               // default
        timeout: 30 * time.Second,   // default
    }
    for _, opt := range opts {
        opt(s)
    }
    return s
}

// Uso — solo los parametros que necesitas:
s1 := NewServer("localhost")
s2 := NewServer("0.0.0.0", WithPort(9090))
s3 := NewServer("0.0.0.0", WithPort(443), WithTLS("cert.pem", "key.pem"))
```

### Alternativa 2: Config struct

```go
type ConnConfig struct {
    Host    string
    Port    int           // 0 = usar default
    Timeout time.Duration // 0 = usar default
    TLS     bool
}

func Connect(cfg ConnConfig) (*Conn, error) {
    if cfg.Port == 0 {
        cfg.Port = 5432  // default
    }
    if cfg.Timeout == 0 {
        cfg.Timeout = 10 * time.Second  // default
    }
    // ...
}

// Uso:
conn, err := Connect(ConnConfig{Host: "db.prod", Port: 5432})
conn, err := Connect(ConnConfig{Host: "db.staging"})  // port default
```

### Alternativa 3: Funciones separadas con nombres descriptivos

```go
// En lugar de overloading, nombra las funciones descriptivamente
func ConnectTCP(host string, port int) (*Conn, error) { ... }
func ConnectUnix(socketPath string) (*Conn, error) { ... }
func ConnectTLS(host string, port int, cfg *tls.Config) (*Conn, error) { ... }

// La stdlib hace esto:
http.Get(url)
http.Post(url, contentType, body)
http.NewRequest(method, url, body)
```

---

## 14. Funciones exportadas y documentacion

Go tiene una convencion estricta para documentar funciones exportadas:

```go
// ParseHostPort splits a network address of the form "host:port"
// into host and port. The host may be a literal IPv6 address,
// which must be enclosed in square brackets.
// An empty port is returned if there is no port.
func ParseHostPort(hostport string) (host string, port string, err error) {
    // ...
}

// La documentacion:
// 1. Empieza con el nombre de la funcion
// 2. Es una frase completa que describe QUE hace (no COMO)
// 3. Va justo antes de la funcion, sin linea en blanco
// 4. Se ve con: go doc paquete.Funcion
```

```go
// ✗ MAL — no empieza con el nombre
// This function checks health of a service
func CheckHealth(host string) error { ... }

// ✗ MAL — describe implementacion, no proposito
// CheckHealth uses TCP connection to port 80 to verify health
func CheckHealth(host string) error { ... }

// ✓ BIEN — empieza con nombre, describe proposito
// CheckHealth reports whether the service at host is healthy
// by making a TCP connection to port 80.
func CheckHealth(host string) error { ... }
```

---

## 15. Patron: Constructor con New

Go no tiene constructores. El patron idiomatico es una funcion `NewXxx` que retorna un puntero:

```go
type Monitor struct {
    hosts    []string
    interval time.Duration
    client   *http.Client
    results  map[string]HealthResult
    mu       sync.RWMutex
    logger   *log.Logger
}

// NewMonitor crea un Monitor con la configuracion dada.
// Si interval es 0, se usa 30 segundos por defecto.
func NewMonitor(hosts []string, interval time.Duration) *Monitor {
    if interval == 0 {
        interval = 30 * time.Second
    }
    return &Monitor{
        hosts:    hosts,
        interval: interval,
        client:   &http.Client{Timeout: 5 * time.Second},
        results:  make(map[string]HealthResult),
        logger:   log.Default(),
    }
}
```

### Variantes del patron New

```go
// NewXxx simple — un constructor
func NewServer(addr string) *Server

// NewXxx con error — cuando la construccion puede fallar
func NewTLSServer(addr, certFile, keyFile string) (*Server, error)

// NewXxx con options — cuando hay muchos parametros opcionales
func NewServer(addr string, opts ...Option) (*Server, error)

// MustNewXxx — panic si falla (solo para init)
func MustNewServer(addr string) *Server {
    s, err := NewServer(addr)
    if err != nil {
        panic(fmt.Sprintf("NewServer(%s): %v", addr, err))
    }
    return s
}

// Default — retorna una instancia con configuracion por defecto
func DefaultServer() *Server {
    return NewServer(":8080")
}
```

---

## 16. Comparacion: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Declaracion | `func name(p T) R` | `R name(T p)` | `fn name(p: T) -> R` |
| Multiples retornos | Nativo: `(R1, R2)` | No (struct o out params) | Nativo: `(R1, R2)` |
| Error handling | `(value, error)` | Return code / errno | `Result<T, E>` |
| Named returns | Si: `(x int, err error)` | No | No |
| Naked return | Si (evitarlo) | N/A | N/A |
| Default params | No | No | No (pero builder pattern) |
| Overloading | No | No (C11: _Generic) | No (pero traits + generics) |
| Visibilidad | Mayuscula/minuscula | Header files | `pub` keyword |
| Generics | Si (Go 1.18+) | No (macros) | Si (trait bounds) |
| Closures | Si | No (function pointers) | Si |
| Variadic | `...T` | `...` (stdarg.h) | No (macros) |
| First-class | Si | Punteros a funcion | Si |
| Constructors | `NewXxx()` convencion | `xxx_create()` convencion | `Xxx::new()` convencion |

### Equivalencias de firmas

```go
// Go
func add(a, b int) int { return a + b }
func divide(a, b float64) (float64, error) { ... }
func process(data []byte) (result string, err error) { ... }
```

```c
// C
int add(int a, int b) { return a + b; }
// No hay multiples retornos — usar struct o out-params:
typedef struct { double value; int error; } DivResult;
DivResult divide(double a, double b) { ... }
// O con out-params:
int divide(double a, double b, double *result) { ... }
```

```rust
// Rust
fn add(a: i32, b: i32) -> i32 { a + b }
fn divide(a: f64, b: f64) -> Result<f64, String> { ... }
fn process(data: &[u8]) -> Result<String, ProcessError> { ... }
```

---

## 17. Ejemplo SysAdmin: Service manager con funciones

```go
package main

import (
    "context"
    "encoding/json"
    "errors"
    "fmt"
    "log"
    "net"
    "os"
    "os/exec"
    "sort"
    "strconv"
    "strings"
    "sync"
    "time"
)

// ── Tipos ──────────────────────────────────────────────────────

type ServiceStatus int

const (
    StatusUnknown ServiceStatus = iota
    StatusRunning
    StatusStopped
    StatusFailed
)

func (s ServiceStatus) String() string {
    switch s {
    case StatusRunning:
        return "running"
    case StatusStopped:
        return "stopped"
    case StatusFailed:
        return "failed"
    default:
        return "unknown"
    }
}

type ServiceInfo struct {
    Name     string        `json:"name"`
    Status   string        `json:"status"`
    PID      int           `json:"pid,omitempty"`
    Port     int           `json:"port,omitempty"`
    Uptime   string        `json:"uptime,omitempty"`
    Memory   int64         `json:"memory_kb,omitempty"`
    CPU      float64       `json:"cpu_percent,omitempty"`
    Error    string        `json:"error,omitempty"`
}

// ── Funciones de deteccion de servicios ────────────────────────

// findPIDByName busca un proceso por nombre en /proc.
// Retorna el PID y true si se encuentra, 0 y false si no.
func findPIDByName(name string) (pid int, found bool) {
    entries, err := os.ReadDir("/proc")
    if err != nil {
        return 0, false
    }

    for _, entry := range entries {
        p, err := strconv.Atoi(entry.Name())
        if err != nil {
            continue
        }

        comm, err := os.ReadFile(fmt.Sprintf("/proc/%d/comm", p))
        if err != nil {
            continue
        }

        if strings.TrimSpace(string(comm)) == name {
            return p, true
        }
    }

    return 0, false
}

// getProcessMemory retorna el RSS en KB de un proceso.
func getProcessMemory(pid int) (rssKB int64, err error) {
    data, err := os.ReadFile(fmt.Sprintf("/proc/%d/status", pid))
    if err != nil {
        return 0, fmt.Errorf("read process status: %w", err)
    }

    for _, line := range strings.Split(string(data), "\n") {
        if strings.HasPrefix(line, "VmRSS:") {
            fields := strings.Fields(line)
            if len(fields) >= 2 {
                rssKB, err = strconv.ParseInt(fields[1], 10, 64)
                if err != nil {
                    return 0, fmt.Errorf("parse VmRSS: %w", err)
                }
                return rssKB, nil
            }
        }
    }

    return 0, nil  // proceso sin VmRSS (kernel thread)
}

// getProcessUptime calcula cuanto tiempo lleva corriendo un proceso.
func getProcessUptime(pid int) (uptime time.Duration, err error) {
    stat, err := os.Stat(fmt.Sprintf("/proc/%d", pid))
    if err != nil {
        return 0, fmt.Errorf("stat process: %w", err)
    }
    return time.Since(stat.ModTime()), nil
}

// checkPort verifica si un puerto TCP esta escuchando.
func checkPort(host string, port int) (open bool) {
    addr := fmt.Sprintf("%s:%d", host, port)
    conn, err := net.DialTimeout("tcp", addr, time.Second)
    if err != nil {
        return false
    }
    conn.Close()
    return true
}

// ── Funciones de recopilacion ──────────────────────────────────

// getServiceInfo recopila informacion completa de un servicio.
func getServiceInfo(name string, expectedPort int) ServiceInfo {
    info := ServiceInfo{Name: name}

    pid, found := findPIDByName(name)
    if !found {
        info.Status = StatusStopped.String()
        return info
    }

    info.PID = pid
    info.Status = StatusRunning.String()
    info.Port = expectedPort

    // Memoria
    if mem, err := getProcessMemory(pid); err == nil {
        info.Memory = mem
    }

    // Uptime
    if uptime, err := getProcessUptime(pid); err == nil {
        info.Uptime = uptime.Round(time.Second).String()
    }

    // Verificar puerto
    if expectedPort > 0 && !checkPort("localhost", expectedPort) {
        info.Status = StatusFailed.String()
        info.Error = fmt.Sprintf("port %d not responding", expectedPort)
    }

    return info
}

// checkAllServices verifica multiples servicios en paralelo.
func checkAllServices(services map[string]int) []ServiceInfo {
    var (
        results []ServiceInfo
        mu      sync.Mutex
        wg      sync.WaitGroup
    )

    for name, port := range services {
        wg.Add(1)
        go func(n string, p int) {
            defer wg.Done()
            info := getServiceInfo(n, p)

            mu.Lock()
            results = append(results, info)
            mu.Unlock()
        }(name, port)
    }

    wg.Wait()

    sort.Slice(results, func(i, j int) bool {
        return results[i].Name < results[j].Name
    })

    return results
}

// ── Funciones de reporte ───────────────────────────────────────

// printServiceTable imprime un reporte tabular de servicios.
func printServiceTable(services []ServiceInfo) {
    fmt.Printf("%-20s %-10s %8s %8s %12s %s\n",
        "SERVICE", "STATUS", "PID", "PORT", "MEMORY", "UPTIME")
    fmt.Println(strings.Repeat("─", 75))

    for _, svc := range services {
        status := svc.Status
        switch svc.Status {
        case "running":
            status = "\033[32m" + status + "\033[0m"
        case "stopped":
            status = "\033[90m" + status + "\033[0m"
        case "failed":
            status = "\033[31m" + status + "\033[0m"
        }

        pidStr := "-"
        if svc.PID > 0 {
            pidStr = strconv.Itoa(svc.PID)
        }

        portStr := "-"
        if svc.Port > 0 {
            portStr = strconv.Itoa(svc.Port)
        }

        memStr := "-"
        if svc.Memory > 0 {
            memStr = formatMemory(svc.Memory)
        }

        uptimeStr := "-"
        if svc.Uptime != "" {
            uptimeStr = svc.Uptime
        }

        fmt.Printf("%-20s %-10s %8s %8s %12s %s\n",
            svc.Name, status, pidStr, portStr, memStr, uptimeStr)

        if svc.Error != "" {
            fmt.Printf("  └─ \033[31m%s\033[0m\n", svc.Error)
        }
    }
}

// formatMemory formatea KB a una representacion legible.
func formatMemory(kb int64) string {
    switch {
    case kb >= 1024*1024:
        return fmt.Sprintf("%.1f GB", float64(kb)/1024/1024)
    case kb >= 1024:
        return fmt.Sprintf("%.1f MB", float64(kb)/1024)
    default:
        return fmt.Sprintf("%d KB", kb)
    }
}

// exportJSON exporta los resultados como JSON.
func exportJSON(services []ServiceInfo, path string) (err error) {
    f, err := os.Create(path)
    if err != nil {
        return fmt.Errorf("create %s: %w", path, err)
    }
    defer func() {
        closeErr := f.Close()
        if err == nil {
            err = closeErr
        }
    }()

    enc := json.NewEncoder(f)
    enc.SetIndent("", "  ")
    if err = enc.Encode(services); err != nil {
        return fmt.Errorf("encode JSON: %w", err)
    }

    return nil
}

// ── Funciones de accion ────────────────────────────────────────

// restartService intenta reiniciar un servicio via systemctl.
func restartService(ctx context.Context, name string) error {
    cmd := exec.CommandContext(ctx, "systemctl", "restart", name)
    output, err := cmd.CombinedOutput()
    if err != nil {
        return fmt.Errorf("restart %s: %v: %s", name, err, strings.TrimSpace(string(output)))
    }
    return nil
}

// autoRecover intenta reiniciar servicios caidos.
func autoRecover(ctx context.Context, services []ServiceInfo) (recovered int, errs []error) {
    for _, svc := range services {
        if svc.Status != "stopped" && svc.Status != "failed" {
            continue
        }

        log.Printf("Attempting to restart %s...", svc.Name)
        if err := restartService(ctx, svc.Name); err != nil {
            errs = append(errs, err)
            continue
        }

        // Verificar que arranco
        time.Sleep(2 * time.Second)
        if _, found := findPIDByName(svc.Name); found {
            log.Printf("✓ %s restarted successfully", svc.Name)
            recovered++
        } else {
            errs = append(errs, fmt.Errorf("%s: restart command succeeded but process not found", svc.Name))
        }
    }

    return recovered, errs
}

// ── Funcion principal ──────────────────────────────────────────

// run es la funcion principal — retorna exit code.
// Usar run() en lugar de logica en main() permite que los defers se ejecuten.
func run() int {
    // Servicios a monitorear: nombre → puerto esperado
    services := map[string]int{
        "sshd":       22,
        "nginx":      80,
        "postgres":   5432,
        "redis-server": 6379,
        "docker":     0,  // sin puerto especifico
        "kubelet":    10250,
        "prometheus": 9090,
    }

    // Verificar todos
    results := checkAllServices(services)

    // Imprimir tabla
    fmt.Println()
    printServiceTable(results)

    // Contar estados
    var running, stopped, failed int
    for _, svc := range results {
        switch svc.Status {
        case "running":
            running++
        case "stopped":
            stopped++
        case "failed":
            failed++
        }
    }

    fmt.Printf("\nSummary: %d running, %d stopped, %d failed (total: %d)\n",
        running, stopped, failed, len(results))

    // Exportar JSON si se pide
    if len(os.Args) > 1 && os.Args[1] == "--json" {
        outPath := "services.json"
        if len(os.Args) > 2 {
            outPath = os.Args[2]
        }
        if err := exportJSON(results, outPath); err != nil {
            log.Printf("Error exporting JSON: %v", err)
            return 1
        }
        fmt.Printf("Results exported to %s\n", outPath)
    }

    // Auto-recover si se pide
    if len(os.Args) > 1 && os.Args[1] == "--recover" {
        ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
        defer cancel()

        recovered, errs := autoRecover(ctx, results)
        fmt.Printf("\nRecovery: %d services restarted", recovered)
        if len(errs) > 0 {
            fmt.Printf(", %d errors:\n", len(errs))
            for _, e := range errs {
                fmt.Printf("  - %v\n", e)
            }
        } else {
            fmt.Println()
        }
    }

    if failed > 0 {
        return 1
    }
    return 0
}

func main() {
    os.Exit(run())
}
```

---

## 18. Errores comunes

| # | Error | Codigo | Solucion |
|---|---|---|---|
| 1 | Naked return en funcion larga | `return` sin valores en 50+ lineas | Usar return explicito siempre |
| 2 | Ignorar error critico | `data, _ := ReadFile(...)` | `if err != nil { return err }` |
| 3 | Error antes de defer | `f, err := Open(); defer f.Close(); if err...` | Primero `if err`, luego `defer` |
| 4 | No retornar error | `func process(x int) { if x < 0 { log.Fatal() } }` | `func process(x int) error` |
| 5 | Demasiados retornos | `func f() (a, b, c, d, e int)` | Retornar struct |
| 6 | Olvidar que todo es copy | Modificar struct pasado por valor | Usar `*Struct` como parametro |
| 7 | Shadowing de err | `err := ...; if err := ...; err != nil` | Verificar que `err` exterior se chequea |
| 8 | Overloading inexistente | Esperar `func(int)` y `func(string)` | Usar nombres diferentes |
| 9 | Named return + return explicito mezclados | Inconsistente dentro de la misma funcion | Elegir un estilo y mantenerlo |
| 10 | _ en import sin necesidad | `import _ "pkg"` cuando si se usa el paquete | Solo `_` para side-effect imports |
| 11 | No exportar constructor | `func newServer()` en lugar de `func NewServer()` | PascalCase para API publica |
| 12 | Error sin contexto | `return err` sin wrapping | `return fmt.Errorf("context: %w", err)` |

### Error 7 en detalle — Shadowing de err

```go
// ✗ BUG SUTIL — el err del if sombrea al err exterior
func process(path string) error {
    data, err := os.ReadFile(path)
    if err != nil {
        return err
    }

    if err := validate(data); err != nil {
        // Este err es una NUEVA variable local al if
        // Si validate retorna nil y queremos usar err despues,
        // el err exterior sigue siendo nil — no hay bug aqui

        // PERO:
        return err  // retorna el err del if, correcto
    }

    // ¿Y si el shadowing esta en el otro sentido?
    var config Config
    if config, err := parse(data); err != nil {
        //          ↑ := crea un NUEVO err, sombrea al de linea 2
        return err
    }
    // AQUI: err sigue siendo nil (del ReadFile), no el del parse
    // Si parse fallo, nunca lo detectamos fuera del if

    // FIX: usar = en lugar de :=
    if config, err = parse(data); err != nil {
        return err
    }
    // Ahora err es el mismo que el de linea 2
}
```

---

## 19. Ejercicios

### Ejercicio 1: Prediccion de pass by value
```go
package main

import "fmt"

type Server struct {
    Name string
    Port int
}

func updatePort(s Server, port int) {
    s.Port = port
    fmt.Println("inside:", s)
}

func updatePortPtr(s *Server, port int) {
    s.Port = port
    fmt.Println("inside ptr:", s)
}

func main() {
    s := Server{Name: "web", Port: 80}
    updatePort(s, 8080)
    fmt.Println("after value:", s)
    updatePortPtr(&s, 8080)
    fmt.Println("after ptr:", s)
}
```
**Prediccion**: ¿Cual es el valor de `s.Port` despues de cada llamada? ¿Por que?

### Ejercicio 2: Prediccion de named return
```go
package main

import "fmt"

func triple(n int) (result int) {
    result = n * 3
    return
}

func tripleModified(n int) (result int) {
    result = n * 3
    return result + 1
}

func main() {
    fmt.Println(triple(5))
    fmt.Println(tripleModified(5))
}
```
**Prediccion**: ¿Que imprime? ¿`tripleModified` retorna 15 o 16?

### Ejercicio 3: Prediccion de shadowing
```go
package main

import (
    "errors"
    "fmt"
)

func process() (err error) {
    err = errors.New("initial error")

    if val, err := compute(); err != nil {
        fmt.Println("compute failed:", err)
        return err
    } else {
        fmt.Println("computed:", val)
    }

    fmt.Println("outer err:", err)
    return err
}

func compute() (int, error) {
    return 42, nil
}

func main() {
    err := process()
    fmt.Println("result:", err)
}
```
**Prediccion**: ¿Que imprime? ¿El `err` retornado por `process()` es "initial error" o nil? ¿Hay shadowing?

### Ejercicio 4: Prediccion de comma-ok
```go
package main

import "fmt"

func main() {
    m := map[string]int{"a": 0, "b": 1}

    v1 := m["a"]
    v2 := m["c"]
    v3, ok3 := m["a"]
    v4, ok4 := m["c"]

    fmt.Println(v1, v2)
    fmt.Println(v3, ok3)
    fmt.Println(v4, ok4)
}
```
**Prediccion**: ¿Cual es la diferencia entre `v1` y `v2`? ¿`ok3` es true? ¿`ok4` es true? ¿Se puede distinguir "key existe con valor 0" de "key no existe" sin comma-ok?

### Ejercicio 5: Prediccion de multiples retornos
```go
package main

import (
    "fmt"
    "strconv"
)

func parsePort(s string) (int, error) {
    port, err := strconv.Atoi(s)
    if err != nil {
        return 0, fmt.Errorf("invalid port %q: %w", s, err)
    }
    if port < 1 || port > 65535 {
        return 0, fmt.Errorf("port %d out of range", port)
    }
    return port, nil
}

func main() {
    for _, s := range []string{"8080", "0", "99999", "abc", ""} {
        port, err := parsePort(s)
        if err != nil {
            fmt.Printf("%-8s → error: %v\n", s, err)
        } else {
            fmt.Printf("%-8s → %d\n", s, port)
        }
    }
}
```
**Prediccion**: ¿Que imprime para cada input? ¿Cuantos producen error?

### Ejercicio 6: System info collector
Implementa un conjunto de funciones que recopilen informacion del sistema:
1. `func GetHostname() (string, error)` — usa `os.Hostname()`
2. `func GetCPUCount() int` — usa `runtime.NumCPU()`
3. `func GetMemInfo() (totalMB, freeMB int64, err error)` — parsea `/proc/meminfo`
4. `func GetUptime() (time.Duration, error)` — parsea `/proc/uptime`
5. `func GetDiskUsage(path string) (usedGB, totalGB float64, err error)` — usa `syscall.Statfs`
6. Una funcion `CollectSystemInfo()` que llame a todas y retorne un struct `SystemInfo`

### Ejercicio 7: Config parser con functional options
Implementa un cliente HTTP configurable:
1. `type HTTPClient struct` con campos: baseURL, timeout, retries, headers, logger
2. `type Option func(*HTTPClient)`
3. Funciones: `WithTimeout()`, `WithRetries()`, `WithHeader()`, `WithLogger()`
4. `func NewHTTPClient(baseURL string, opts ...Option) *HTTPClient`
5. `func (c *HTTPClient) Get(path string) ([]byte, error)`

### Ejercicio 8: Error chain builder
Implementa un sistema de validacion:
1. `func ValidatePort(port int) error`
2. `func ValidateHost(host string) error`
3. `func ValidateTimeout(d time.Duration) error`
4. `func ValidateConfig(cfg Config) error` — llama a las anteriores, acumula errores
5. Usa `errors.Join` (Go 1.20+) para combinar multiples errores de validacion
6. El caller debe poder inspeccionar errores individuales con `errors.Is`

### Ejercicio 9: Prediccion avanzada — defer con named return
```go
func readConfig(path string) (data []byte, err error) {
    defer func() {
        if err != nil {
            err = fmt.Errorf("readConfig(%s): %w", path, err)
        }
    }()

    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer f.Close()

    return io.ReadAll(f)
}
```
**Prediccion**: Si el archivo no existe, ¿que error retorna? ¿El defer modifica el error? ¿Que pasa si `io.ReadAll` falla?

### Ejercicio 10: Service health checker con todos los patrones
Implementa un health checker que use:
1. `(value, error)` para las operaciones de red
2. `(value, ok)` para buscar en cache de resultados
3. Named returns con defer para enriquecer errores
4. Functional options para configurar timeouts, retries, etc.
5. Un `NewHealthChecker` constructor
6. Funciones no exportadas para la implementacion interna
7. Funciones exportadas para la API publica

---

## 20. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│              func EN GO — RESUMEN                             │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DECLARACION                                                 │
│  func name(params) returnType { body }                       │
│  func name(params) (r1 T1, r2 T2) { body }  ← multiples    │
│  func name(params) (result T, err error) { } ← named        │
│                                                              │
│  PARAMETROS                                                  │
│  ├─ Todo es pass by value (se copia)                         │
│  ├─ Para modificar: usar punteros (*T)                       │
│  ├─ Slices/maps/channels: copian header, comparten datos     │
│  ├─ Structs grandes: usar puntero por eficiencia             │
│  ├─ No hay default values ni overloading                     │
│  └─ Alternativas: functional options, config struct          │
│                                                              │
│  MULTIPLES RETORNOS                                          │
│  ├─ (value, error) — patron principal de Go                  │
│  ├─ (value, ok) — comma-ok idiom                             │
│  ├─ 3+ retornos: raro, considerar struct                     │
│  └─ No son tuplas — son valores separados                    │
│                                                              │
│  NAMED RETURNS                                               │
│  ├─ Documentan el significado de los retornos                │
│  ├─ Necesarios para defer que modifica returns               │
│  ├─ Inicializadas a zero value                               │
│  └─ Naked return: EVITAR en funciones largas                 │
│                                                              │
│  BLANK IDENTIFIER _                                          │
│  ├─ Descartar valores de retorno: _, err := f()              │
│  ├─ Side-effect import: import _ "pkg"                       │
│  ├─ Interface check: var _ Interface = (*Type)(nil)          │
│  └─ Descartar indice: for _, v := range s { }               │
│                                                              │
│  VISIBILIDAD                                                 │
│  ├─ PascalCase = exportado (publico)                         │
│  └─ camelCase = no exportado (privado al paquete)            │
│                                                              │
│  PATRONES                                                    │
│  ├─ NewXxx() — constructor idiomatico                        │
│  ├─ MustXxx() — panic si falla (solo init)                   │
│  ├─ Functional options — parametros opcionales               │
│  ├─ Config struct — muchos parametros con defaults           │
│  └─ run() int + os.Exit(run()) — defers se ejecutan         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T02 - Variadic functions — ...T, paso de slices con ..., fmt.Println como ejemplo
