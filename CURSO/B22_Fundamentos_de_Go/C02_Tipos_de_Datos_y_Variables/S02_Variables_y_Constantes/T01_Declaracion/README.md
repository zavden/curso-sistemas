# Declaración de Variables en Go

## 1. Introducción

Go tiene un sistema de declaración de variables diseñado para ser **explícito, seguro y conciso**. A diferencia de lenguajes dinámicos donde las variables aparecen sin declaración, Go requiere que toda variable sea declarada antes de usarse — pero ofrece múltiples formas de hacerlo según el contexto.

Las reglas fundamentales son:
- Toda variable tiene un **tipo fijo** determinado en compilación
- Toda variable se inicializa a su **zero value** si no se asigna explícitamente
- Toda variable declarada **debe usarse** (en funciones) o el compilador rechaza el programa
- No existe `undefined`, `null` para tipos valor, ni variables sin inicializar

```
┌─────────────────────────────────────────────────────┐
│          FORMAS DE DECLARACIÓN EN GO                │
├─────────────────────────────────────────────────────┤
│                                                     │
│  var name string          ← tipo explícito          │
│  var name string = "go"   ← tipo + valor            │
│  var name = "go"          ← tipo inferido           │
│  name := "go"             ← short declaration       │
│                                                     │
│  var (                    ← bloque de               │
│      a int                  declaraciones           │
│      b string                                       │
│  )                                                  │
│                                                     │
│  Nivel paquete: solo var/const/type/func            │
│  Nivel función: var + :=                            │
│                                                     │
└─────────────────────────────────────────────────────┘
```

---

## 2. Declaración con `var` — Forma larga

La forma canónica de declarar variables. Válida en **cualquier ámbito** (paquete o función):

```go
// Tipo explícito, zero value automático
var port int          // port = 0
var host string       // host = ""
var debug bool        // debug = false
var timeout float64   // timeout = 0.0

// Tipo explícito con valor inicial
var port int = 8080
var host string = "localhost"
var debug bool = true
var timeout float64 = 30.5

// Tipo inferido del valor (el compilador deduce el tipo)
var port = 8080          // int
var host = "localhost"   // string
var debug = true         // bool
var timeout = 30.5       // float64
```

### Cuándo usar `var` con tipo explícito

```go
// 1. Cuando quieres el zero value (la intención es clara)
var retryCount int
var lastError string

// 2. Cuando el tipo inferido no es el que quieres
var port uint16 = 8080       // sin tipo: sería int
var ratio float32 = 0.75     // sin tipo: sería float64
var mask uint32 = 0xFF       // sin tipo: sería int
var offset int64 = 100       // sin tipo: sería int

// 3. Para interfaces (el tipo concreto no revela la intención)
var logger io.Writer = os.Stdout
var handler http.Handler = myMux
var reader io.Reader = file

// 4. Variables de paquete (no se puede usar :=)
var Version = "1.0.0"
var DefaultTimeout = 30 * time.Second
```

---

## 3. Short Declaration `:=` — Forma corta

Disponible **solo dentro de funciones**. Declara e inicializa en una sola expresión:

```go
func main() {
    port := 8080                        // int
    host := "localhost"                  // string
    debug := os.Getenv("DEBUG") == "1"  // bool

    // Con llamadas a funciones
    conn, err := net.Dial("tcp", "localhost:8080")
    data, err := os.ReadFile("/etc/hostname")

    // Con type assertion
    val, ok := cache["key"]
    iface, ok := obj.(Stringer)
}
```

### Reglas de `:=`

```go
// ✗ NO se puede usar a nivel de paquete
port := 8080  // syntax error: non-declaration statement outside function body

// ✓ Solo dentro de funciones
func main() {
    port := 8080  // OK
}
```

```go
// Con múltiples valores, al menos UNO debe ser nuevo
conn, err := net.Dial("tcp", addr)   // ambos nuevos: OK
data, err := os.ReadFile(path)       // err ya existe, data es nuevo: OK
// data, err := os.Open(path)        // ✗ ambos ya existen: no new variables on left side
```

### `:=` reasigna variables existentes (en el mismo scope)

```go
func process() error {
    result, err := step1()   // ambos nuevos
    fmt.Println(result)

    value, err := step2()    // value nuevo, err REASIGNADO (mismo err de arriba)
    fmt.Println(value)

    // err aquí es el mismo puntero — contiene el error de step2
    return err
}
```

Esta regla es fundamental para el patrón idiomático de manejo de errores en Go, donde `err` se reutiliza continuamente.

---

## 4. Declaraciones múltiples — Bloques `var`

Para agrupar declaraciones relacionadas, Go permite bloques con paréntesis:

```go
// Bloque var — ideal para variables de paquete
var (
    // Configuración del servidor
    ListenAddr  = ":8080"
    ReadTimeout = 15 * time.Second
    IdleTimeout = 60 * time.Second

    // Estado interno
    startTime time.Time
    reqCount  int64
)
```

### Comparación de estilos

```go
// ✗ Repetitivo
var listenAddr string
var readTimeout time.Duration
var idleTimeout time.Duration
var maxConns int

// ✓ Agrupado por contexto
var (
    listenAddr  = ":8080"
    readTimeout = 15 * time.Second
    idleTimeout = 60 * time.Second
    maxConns    = 1000
)
```

### Variables múltiples en línea (dentro de funciones)

```go
func handleRequest() {
    // Múltiples variables del mismo tipo
    var x, y, z int

    // Múltiples con valores
    var host, port = "localhost", 8080

    // Short declaration múltiple
    name, age, active := "server1", 5, true

    // Swap sin variable temporal
    x, y = y, x
}
```

---

## 5. Inferencia de tipos

Cuando se omite el tipo, Go lo infiere del valor asignado. Las reglas son predecibles:

```go
// Literales numéricos sin punto → int
count := 42          // int (no int32, no int64)

// Literales con punto → float64
ratio := 3.14        // float64 (no float32)

// Literales string → string
name := "server"     // string

// Literales bool → bool
active := true       // bool

// Literales rune → int32
ch := 'A'            // int32 (rune)

// Literales complejos → complex128
z := 1 + 2i          // complex128
```

### Cuidado con la inferencia en contextos numéricos

```go
// El tipo se "fija" en la primera asignación
var ratio = 0.5       // float64
// ratio = 1          // OK: 1 se convierte a float64 implícitamente (constante sin tipo)

// Pero con variables tipadas:
var n int = 42
// var f float64 = n   // ✗ cannot use n (variable of type int) as float64
var f float64 = float64(n)  // ✓ conversión explícita
```

### Inferencia con funciones

```go
// El tipo se infiere del retorno de la función
data, err := os.ReadFile("/etc/hosts")  // []byte, error
file, err := os.Open("/etc/hosts")      // *os.File, error
resp, err := http.Get(url)              // *http.Response, error
port, err := strconv.Atoi("8080")       // int, error

// net.Dial retorna net.Conn (interfaz), no *net.TCPConn
conn, err := net.Dial("tcp", addr)      // net.Conn, error
```

---

## 6. Scope (ámbito) de variables

Go tiene **lexical scoping** — una variable es visible desde su declaración hasta el final del bloque `{}` que la contiene:

```
┌──────────────────────────────────────────────────┐
│ Package scope (var, const, type, func)           │
│  ┌───────────────────────────────────────────┐   │
│  │ Function scope                            │   │
│  │  ┌────────────────────────────────────┐   │   │
│  │  │ Block scope (if, for, switch, {})  │   │   │
│  │  │  ┌─────────────────────────────┐   │   │   │
│  │  │  │ Inner block scope           │   │   │   │
│  │  │  └─────────────────────────────┘   │   │   │
│  │  └────────────────────────────────────┘   │   │
│  └───────────────────────────────────────────┘   │
│                                                  │
│  Resolución: inner → outer (el más cercano gana) │
└──────────────────────────────────────────────────┘
```

### Scope a nivel de paquete

```go
package main

// Variables de paquete — visibles en todo el paquete
var (
    version   = "1.0.0"
    buildTime string
)

// Visibles desde otros paquetes si empiezan con mayúscula
var DefaultPort = 8080    // exportada (pública)
var defaultHost = "0.0.0.0"  // no exportada (privada al paquete)

func main() {
    fmt.Println(version)      // OK: acceso a variable de paquete
    fmt.Println(DefaultPort)  // OK
}
```

### Scope a nivel de función

```go
func processConfig(path string) error {
    // path es parámetro — vive en el scope de la función

    data, err := os.ReadFile(path)  // data, err: scope de función
    if err != nil {
        return err
    }

    // data es accesible aquí
    fmt.Println(len(data))
    return nil
}
```

### Scope de bloque — `if`, `for`, `switch`

```go
func connect() {
    // Variable declarada en el init del if
    if conn, err := net.Dial("tcp", "localhost:8080"); err != nil {
        log.Printf("Error: %v", err)
        // conn y err solo existen dentro de este if/else
    } else {
        // conn disponible aquí también (mismo bloque if/else)
        defer conn.Close()
        // ... usar conn
    }

    // ✗ conn no existe aquí
    // conn.Write(data)  // undefined: conn

    // Si necesitas conn fuera del if:
    conn, err := net.Dial("tcp", "localhost:8080")
    if err != nil {
        log.Fatal(err)
    }
    defer conn.Close()
    // conn disponible aquí
}
```

### Scope en `for`

```go
func scanPorts() {
    // i solo existe dentro del for
    for i := 1; i <= 1024; i++ {
        addr := fmt.Sprintf("localhost:%d", i)
        // addr se re-crea en cada iteración
        conn, err := net.DialTimeout("tcp", addr, 100*time.Millisecond)
        if err == nil {
            fmt.Printf("Port %d open\n", i)
            conn.Close()
        }
    }
    // i no existe aquí
    // addr no existe aquí
}

// for range — la variable de iteración tiene scope del for
for key, value := range configMap {
    // key y value solo existen dentro del for
    fmt.Printf("%s = %s\n", key, value)
}
```

---

## 7. Shadowing — Ocultamiento de variables

Cuando una variable en un scope interno tiene el **mismo nombre** que una en un scope externo, la interna "oculta" (shadows) a la externa:

```go
var port = 8080  // paquete

func main() {
    fmt.Println(port)  // 8080 — variable de paquete

    port := 9090       // ← SHADOW: nueva variable local
    fmt.Println(port)  // 9090 — variable local

    if true {
        port := 3000   // ← SHADOW: otra variable nueva
        fmt.Println(port)  // 3000
    }

    fmt.Println(port)  // 9090 — vuelve al scope de función
}
// La variable de paquete sigue siendo 8080
```

### Shadowing accidental — Bug muy común

```go
// ✗ BUG: err se shadowea, el error de OpenFile se pierde
func configureLogging() error {
    var err error

    if logToFile {
        // := crea NUEVO err dentro del if (shadow)
        file, err := os.OpenFile("/var/log/app.log", os.O_CREATE|os.O_WRONLY, 0644)
        if err != nil {
            return err
        }
        log.SetOutput(file)
    }

    // err aquí sigue siendo nil — NUNCA recibió el error de OpenFile
    return err
}

// ✓ CORRECTO: usar = en lugar de :=, declarar file antes
func configureLogging() error {
    var err error

    if logToFile {
        var file *os.File
        file, err = os.OpenFile("/var/log/app.log", os.O_CREATE|os.O_WRONLY, 0644)
        if err != nil {
            return err
        }
        log.SetOutput(file)
    }

    return err
}
```

### Shadowing de imports y builtins

```go
// ✗ Shadowear builtins es legal pero peligroso
func bad() {
    len := 42             // shadows builtin len()
    // fmt.Println(len("hello"))  // ✗ cannot call non-function len

    true := false         // shadows builtin true
    // if true { ... }    // siempre false ahora

    fmt := "hello"        // shadows import fmt
    // fmt.Println(...)   // ✗ fmt.Println undefined
}

// Detección con go vet / golangci-lint:
// govet tiene el check "shadow" (deshabilitado por defecto)
// golangci-lint: govet.settings.shadow.strict = true
```

### Configurar detección de shadow en golangci-lint

```yaml
# .golangci.yml
linters-settings:
  govet:
    enable:
      - shadow
    settings:
      shadow:
        strict: true
```

---

## 8. Variables de paquete vs variables locales

```
┌─────────────────────────────────────────────────────────────┐
│          VARIABLES DE PAQUETE vs LOCALES                    │
├──────────────────────┬──────────────────────────────────────┤
│  Variable paquete    │  Variable local                      │
├──────────────────────┼──────────────────────────────────────┤
│  Declarada fuera de  │  Declarada dentro de funciones       │
│  funciones           │                                      │
│  Solo var (no :=)    │  var o :=                            │
│  Vive toda la vida   │  Vive hasta fin del bloque {}        │
│  del programa        │                                      │
│  Accesible desde     │  Solo accesible en su scope          │
│  todo el paquete     │                                      │
│  Exportable (A-Z)    │  Nunca exportable                    │
│  Zero-initialized    │  Zero-initialized o asignada         │
│  antes de main()     │                                      │
│  Puede ser estado    │  Efímera, predecible                 │
│  global mutable ⚠    │                                      │
└──────────────────────┴──────────────────────────────────────┘
```

### Variables de paquete — uso responsable

```go
package server

import "time"

// ✓ Constantes de configuración (inmutables en la práctica)
var (
    Version   = "1.2.3"
    BuildTime = "2024-01-15T10:30:00Z"
)

// ✓ Valores por defecto (se leen, rara vez se escriben)
var (
    DefaultPort    = 8080
    DefaultTimeout = 30 * time.Second
)

// ⚠ Estado mutable compartido — requiere sincronización
var (
    activeConns int64  // accedida desde múltiples goroutines
    // Usar sync/atomic o sync.Mutex para proteger
)

// ✗ EVITAR: estado global que dificulta testing
var db *sql.DB  // ¿cómo testeas funciones que usan esto?
// Mejor: pasar db como parámetro o en un struct
```

### Orden de inicialización de variables de paquete

```go
package main

// Se inicializan en orden de declaración (dentro del archivo)
// y en orden de dependencia (entre archivos del paquete)
var (
    a = compute()  // se ejecuta primero si no depende de nada
    b = a + 1      // depende de a, se ejecuta después
    c = b * 2      // depende de b
)

// init() se ejecuta después de todas las variables de paquete
func init() {
    fmt.Println(a, b, c)  // todas ya inicializadas
}
```

---

## 9. La función `init()`

Función especial para inicialización de paquetes. Se ejecuta automáticamente antes de `main()`:

```go
package main

import (
    "fmt"
    "log"
    "os"
)

// Puede haber múltiples init() en un archivo (y en varios archivos del paquete)
func init() {
    // Configurar logging
    log.SetFlags(log.Ldate | log.Ltime | log.Lshortfile)
}

func init() {
    // Validar entorno
    if os.Getenv("APP_ENV") == "" {
        log.Println("WARNING: APP_ENV not set, defaulting to development")
    }
}

func main() {
    fmt.Println("main starts after all init() functions")
}
```

### Orden de ejecución

```
┌──────────────────────────────────────────────────────┐
│           ORDEN DE INICIALIZACIÓN                    │
├──────────────────────────────────────────────────────┤
│                                                      │
│  1. Imports se resuelven recursivamente              │
│     (dependencias primero, sin ciclos)               │
│                                                      │
│  2. Variables de paquete de cada paquete importado   │
│                                                      │
│  3. init() de cada paquete importado                 │
│                                                      │
│  4. Variables de paquete del paquete main             │
│                                                      │
│  5. init() del paquete main                          │
│                                                      │
│  6. main()                                           │
│                                                      │
│  Si el paquete A importa B y C, y B importa C:      │
│  C.vars → C.init() → B.vars → B.init() → A.vars    │
│  → A.init()                                          │
│                                                      │
└──────────────────────────────────────────────────────┘
```

### Usos legítimos de `init()` en SysAdmin

```go
// 1. Registrar drivers/plugins (patrón estándar de la stdlib)
import _ "net/http/pprof"       // registra endpoints de profiling
import _ "github.com/lib/pq"    // registra driver PostgreSQL

// 2. Validar entorno crítico al arrancar
func init() {
    required := []string{"DATABASE_URL", "API_KEY", "LOG_LEVEL"}
    for _, env := range required {
        if os.Getenv(env) == "" {
            log.Fatalf("FATAL: required environment variable %s is not set", env)
        }
    }
}

// 3. Precompilar regexps (una vez, no por request)
var logPattern *regexp.Regexp

func init() {
    logPattern = regexp.MustCompile(
        `^(\d{4}-\d{2}-\d{2}T[\d:]+Z)\s+(\w+)\s+(.*)$`,
    )
}
```

### Cuándo **no** usar `init()`

```go
// ✗ No para configuración compleja que puede fallar de formas que quieras manejar
func init() {
    db, err := sql.Open("postgres", os.Getenv("DATABASE_URL"))
    // Si falla aquí, el programa muere sin poder hacer cleanup
    // Mejor: inicializar en main() donde puedes manejar el error
}

// ✗ No para lógica que depende de flags/argumentos de línea de comando
func init() {
    // flag.Parse() aún no se ha ejecutado aquí
    // os.Args está disponible, pero flag.Parse() se llama normalmente en main()
}

// ✓ Mejor patrón: constructor explícito
func NewServer(config Config) (*Server, error) {
    // Inicialización explícita, testeable, errores manejables
}
```

---

## 10. Blank Identifier `_`

El underscore descarta valores que no necesitas. **Go exige que toda variable declarada se use**, así que `_` es el escape:

```go
// Descartar valor de retorno que no necesitas
_, err := fmt.Fprintf(w, "data")       // ignoro bytes escritos
_, err = io.Copy(dst, src)             // ignoro bytes copiados
hash, _ := hashFile("/etc/hosts")      // ignoro error (⚠ solo si estás seguro)

// Descartar índice o valor en range
for _, line := range lines {           // ignoro índice
    process(line)
}
for i := range items {                 // ignoro valor (más eficiente)
    items[i].Process()
}

// Import solo por side effects (init())
import _ "net/http/pprof"
import _ "image/png"

// Verificar que un tipo implementa una interfaz (compile-time check)
var _ http.Handler = (*MyHandler)(nil)
var _ io.ReadWriteCloser = (*Connection)(nil)
```

### Cuándo es aceptable ignorar errores

```go
// ✓ Operaciones de "best effort" en cleanup/shutdown
defer file.Close()              // error de Close() rara vez importa al leer
defer resp.Body.Close()         // idem

// ✓ Escritura a un Writer que no puede fallar
fmt.Fprintf(os.Stderr, "debug: %s\n", msg)  // stderr no falla en la práctica

// ✗ NUNCA ignorar errores de:
// - Escritura a archivos de datos
// - Operaciones de red
// - Operaciones de base de datos
// - Cualquier I/O que el usuario espera que se complete
```

---

## 11. Variables no usadas — Error de compilación

Go es estricto: una variable local declarada pero no usada es un **error de compilación**, no un warning:

```go
func main() {
    x := 42
    // Si no usas x en ningún lado:
    // ./main.go:4:2: x declared and not used
}
```

### Estrategias durante desarrollo

```go
// 1. Usar _ para descartar intencionalmente
result, _ := compute()

// 2. Usar el blank identifier temporalmente
x := 42
_ = x  // "uso" temporal durante debugging — quitar antes de commit

// 3. Variables de paquete NO tienen esta restricción
var unused int  // compila sin error (pero golangci-lint lo reportará)
```

### Imports no usados — Mismo tratamiento

```go
import "fmt"
// Si no usas fmt: imported and not used: "fmt"

// Solución temporal durante desarrollo:
import "fmt"
var _ = fmt.Println  // "uso" temporal

// Mejor: usar goimports que auto-gestiona imports
```

---

## 12. Patrones idiomáticos de declaración

### Patrón: configuración con valores por defecto

```go
type ServerConfig struct {
    Host         string
    Port         int
    ReadTimeout  time.Duration
    WriteTimeout time.Duration
    MaxConns     int
    TLSCert      string
    TLSKey       string
}

func NewServerConfig() ServerConfig {
    return ServerConfig{
        Host:         "0.0.0.0",
        Port:         8080,
        ReadTimeout:  15 * time.Second,
        WriteTimeout: 15 * time.Second,
        MaxConns:     10000,
    }
}

// Override con opciones del entorno
func ServerConfigFromEnv() ServerConfig {
    cfg := NewServerConfig()

    if host := os.Getenv("HOST"); host != "" {
        cfg.Host = host
    }
    if portStr := os.Getenv("PORT"); portStr != "" {
        if port, err := strconv.Atoi(portStr); err == nil {
            cfg.Port = port
        }
    }
    if timeout := os.Getenv("READ_TIMEOUT"); timeout != "" {
        if d, err := time.ParseDuration(timeout); err == nil {
            cfg.ReadTimeout = d
        }
    }

    return cfg
}
```

### Patrón: manejo de errores con `:=` encadenado

```go
func deployService(name string) error {
    // Cada := reusa err pero crea la variable específica nueva
    config, err := loadConfig(name)
    if err != nil {
        return fmt.Errorf("loading config: %w", err)
    }

    image, err := buildImage(config)
    if err != nil {
        return fmt.Errorf("building image: %w", err)
    }

    container, err := startContainer(image)
    if err != nil {
        return fmt.Errorf("starting container: %w", err)
    }

    status, err := healthCheck(container)
    if err != nil {
        return fmt.Errorf("health check: %w", err)
    }

    log.Printf("Service %s deployed: %s", name, status)
    return nil
}
```

### Patrón: variables de grupo semántico

```go
func collectSystemInfo() (*SystemInfo, error) {
    // Agrupar por contexto
    var (
        hostname string
        ips      []string
        err      error
    )

    hostname, err = os.Hostname()
    if err != nil {
        return nil, fmt.Errorf("getting hostname: %w", err)
    }

    addrs, err := net.InterfaceAddrs()
    if err != nil {
        return nil, fmt.Errorf("getting interfaces: %w", err)
    }

    for _, addr := range addrs {
        if ipNet, ok := addr.(*net.IPNet); ok && !ipNet.IP.IsLoopback() {
            ips = append(ips, ipNet.IP.String())
        }
    }

    return &SystemInfo{
        Hostname: hostname,
        IPs:      ips,
    }, nil
}
```

---

## 13. `new()` y `make()` — Funciones de asignación

Go tiene dos funciones builtin para crear valores. A menudo confunden a principiantes:

```
┌──────────────────────────────────────────────────────────┐
│              new() vs make()                             │
├──────────────┬───────────────────────────────────────────┤
│  new(T)      │  make(T, args...)                         │
├──────────────┼───────────────────────────────────────────┤
│  Cualquier   │  Solo slice, map, channel                 │
│  tipo        │                                           │
│  Retorna *T  │  Retorna T (no puntero)                   │
│  Zero value  │  Inicializado (listo para usar)           │
│  Rara vez    │  Muy usado                                │
│  necesario   │                                           │
└──────────────┴───────────────────────────────────────────┘
```

### `new()` — Raramente necesario

```go
// new(T) equivale a: tmp := T{}; &tmp
// Asigna memoria, la pone a zero, retorna puntero

p := new(int)       // *int, apunta a 0
s := new(string)    // *string, apunta a ""
b := new(bool)      // *bool, apunta a false

// Equivalentes más idiomáticos (preferidos):
p := new(int)
// vs
n := 0
p := &n
// vs (más común)
var p *int = new(int)
```

```go
// Caso útil: cuando necesitas un puntero a zero value en una línea
type Config struct {
    MaxRetries *int  // nil = no configurado, 0 = sin retries
}

cfg := Config{
    MaxRetries: new(int),  // &0, no hay otra forma en un literal
}

// Alternativa con helper (más legible)
func intPtr(n int) *int { return &n }
cfg := Config{
    MaxRetries: intPtr(0),
}
```

### `make()` — Esencial para slice, map, channel

```go
// make inicializa la estructura interna, no solo la memoria

// Slices
s := make([]byte, 0, 4096)        // len=0, cap=4096 (buffer de lectura)
lines := make([]string, 0, 100)   // pre-alocar para ~100 líneas
matrix := make([][]int, rows)     // slice de slices

// Maps
m := make(map[string]string)      // map vacío, listo para escribir
m := make(map[string]int, 1000)   // pre-alocar para ~1000 entradas

// Channels
ch := make(chan error)             // sin buffer (síncrono)
ch := make(chan string, 100)       // con buffer de 100
```

```go
// ¿Por qué no basta con var?
var s []string    // nil slice — append funciona, pero len/cap = 0
var m map[string]string  // nil map — ¡PANIC si escribes! m["key"] = "val"

// make() crea un map inicializado:
m := make(map[string]string)  // map vacío, puedes escribir
m["key"] = "val"              // OK
```

---

## 14. Punteros a variables — Basics

Los punteros serán tratados en profundidad más adelante, pero son fundamentales para entender declaraciones:

```go
// & toma la dirección de una variable
x := 42
p := &x        // p es *int, apunta a x
fmt.Println(*p)  // 42 — dereferenciar

// * declara un tipo puntero
var ptr *int          // nil (no apunta a nada)
var ptr *int = &x     // apunta a x

// Modificar a través del puntero
*p = 100
fmt.Println(x)  // 100 — x cambió porque p apuntaba a x
```

### No puedes tomar la dirección de todo

```go
// ✓ Variables
x := 42
p := &x

// ✗ Constantes
const c = 42
// p := &c  // cannot take address of c

// ✗ Literales directos
// p := &42  // cannot take address of 42

// ✓ Literales de struct/array (excepción útil)
p := &Config{Port: 8080}   // crea Config en heap, retorna puntero
```

---

## 15. Variables y concurrencia — Vista previa

Las variables compartidas entre goroutines necesitan sincronización:

```go
package main

import (
    "fmt"
    "sync"
    "sync/atomic"
)

// ✗ Data race — resultado impredecible
func badCounter() {
    var count int
    var wg sync.WaitGroup

    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            count++  // ← múltiples goroutines leen y escriben sin protección
        }()
    }
    wg.Wait()
    fmt.Println(count)  // Número impredecible, probablemente < 1000
}

// ✓ Correcto — atomic
func atomicCounter() {
    var count atomic.Int64
    var wg sync.WaitGroup

    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            count.Add(1)  // operación atómica, segura
        }()
    }
    wg.Wait()
    fmt.Println(count.Load())  // Siempre 1000
}

// ✓ Correcto — mutex
func mutexCounter() {
    var (
        mu    sync.Mutex
        count int
    )
    var wg sync.WaitGroup

    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            mu.Lock()
            count++
            mu.Unlock()
        }()
    }
    wg.Wait()
    fmt.Println(count)  // Siempre 1000
}
```

### Detectar data races

```bash
# Compilar y ejecutar con detector de races
go run -race main.go
go test -race ./...

# Salida cuando detecta un race:
# WARNING: DATA RACE
# goroutine 7 at 0x...
#   main.badCounter.func1()
# Previous write at 0x...
#   main.badCounter.func1()
```

---

## 16. Ejemplo SysAdmin: Monitor de servicios

```go
package main

import (
    "context"
    "fmt"
    "log"
    "net"
    "os"
    "strconv"
    "strings"
    "sync"
    "time"
)

// Variables de paquete — configuración inmutable después de init
var (
    checkInterval = 10 * time.Second
    checkTimeout  = 3 * time.Second
    logPrefix     = "[monitor]"
)

// ServiceCheck define un servicio a monitorear
type ServiceCheck struct {
    Name    string
    Host    string
    Port    int
    Proto   string // "tcp" o "udp"
}

// ServiceStatus almacena el resultado de un check
type ServiceStatus struct {
    Service  string
    Up       bool
    Latency  time.Duration
    Error    string
    CheckedAt time.Time
}

func init() {
    log.SetFlags(log.Ldate | log.Ltime | log.Lmicroseconds)
    log.SetPrefix(logPrefix + " ")

    // Override intervalo desde env
    if val := os.Getenv("CHECK_INTERVAL"); val != "" {
        if d, err := time.ParseDuration(val); err == nil {
            checkInterval = d
        }
    }
}

func main() {
    // Declaración con := — servicios a monitorear
    services := []ServiceCheck{
        {Name: "SSH", Host: "localhost", Port: 22, Proto: "tcp"},
        {Name: "HTTP", Host: "localhost", Port: 80, Proto: "tcp"},
        {Name: "HTTPS", Host: "localhost", Port: 443, Proto: "tcp"},
        {Name: "DNS", Host: "8.8.8.8", Port: 53, Proto: "tcp"},
        {Name: "PostgreSQL", Host: "localhost", Port: 5432, Proto: "tcp"},
    }

    // Variables locales para el loop de monitoreo
    var (
        results []ServiceStatus
        mu      sync.Mutex
        wg      sync.WaitGroup
    )

    // Un ciclo de checks
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()

    for _, svc := range services {
        wg.Add(1)
        go func(s ServiceCheck) {
            defer wg.Done()

            status := checkService(ctx, s)

            mu.Lock()
            results = append(results, status)
            mu.Unlock()
        }(svc)
    }

    wg.Wait()

    // Reportar resultados
    printReport(results)

    // Exit code basado en resultados
    exitCode := 0
    for _, r := range results {
        if !r.Up {
            exitCode = 1
        }
    }
    os.Exit(exitCode)
}

func checkService(ctx context.Context, svc ServiceCheck) ServiceStatus {
    // Variables locales con tipos claros
    addr := net.JoinHostPort(svc.Host, strconv.Itoa(svc.Port))
    start := time.Now()

    // Dialer con contexto para respetar timeout global
    dialer := net.Dialer{Timeout: checkTimeout}
    conn, err := dialer.DialContext(ctx, svc.Proto, addr)

    // Calcular latencia independientemente del resultado
    latency := time.Since(start)

    if err != nil {
        return ServiceStatus{
            Service:   svc.Name,
            Up:        false,
            Latency:   latency,
            Error:     err.Error(),
            CheckedAt: time.Now(),
        }
    }
    defer conn.Close()

    return ServiceStatus{
        Service:   svc.Name,
        Up:        true,
        Latency:   latency,
        CheckedAt: time.Now(),
    }
}

func printReport(results []ServiceStatus) {
    // Variables para el formato de la tabla
    var (
        upCount   int
        downCount int
        maxName   int
    )

    // Calcular ancho máximo para alineación
    for _, r := range results {
        if len(r.Service) > maxName {
            maxName = len(r.Service)
        }
    }

    // Header
    fmt.Println(strings.Repeat("=", 60))
    fmt.Printf("Service Health Check — %s\n", time.Now().Format("2006-01-02 15:04:05"))
    fmt.Println(strings.Repeat("=", 60))

    // Resultados
    for _, r := range results {
        // Variable local para el ícono de estado
        status := "UP  "
        if !r.Up {
            status = "DOWN"
            downCount++
        } else {
            upCount++
        }

        // Formato con padding dinámico
        fmt.Printf("  %-*s  [%s]  %8s", maxName, r.Service, status, r.Latency.Round(time.Microsecond))
        if r.Error != "" {
            fmt.Printf("  — %s", r.Error)
        }
        fmt.Println()
    }

    fmt.Println(strings.Repeat("-", 60))
    fmt.Printf("  Total: %d | Up: %d | Down: %d\n", upCount+downCount, upCount, downCount)
    fmt.Println(strings.Repeat("=", 60))
}
```

---

## 17. Comparación: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Declaración | `var x int` / `x := 0` | `int x;` / `int x = 0;` | `let x: i32;` / `let x = 0;` |
| Inicialización | Zero value garantizado | Basura si no inicializas | Error si usas sin inicializar |
| Mutabilidad | Mutable por defecto | Mutable por defecto | Inmutable por defecto (`let mut`) |
| Type inference | `x := 42` | No (antes de C23) | `let x = 42;` |
| Scope | Lexical, bloques `{}` | Lexical, bloques `{}` | Lexical, bloques `{}` |
| Variables no usadas | Error de compilación | Warning (`-Wunused`) | Warning (`#[warn(unused)]`) |
| Shadowing | Permitido (peligroso) | Permitido | Permitido (idiomático) |
| Constantes | `const` (compile-time) | `#define` / `const` | `const` (compile-time) |
| Punteros | Sí, sin aritmética | Sí, con aritmética | Sí, via referencias `&` |
| Short declaration | `:=` | No | No |
| Múltiple retorno | `a, b := f()` | No (struct) | Tuplas: `let (a, b) = f()` |
| Global mutable | `var` paquete (⚠ races) | Global/`static` | `static mut` (unsafe) |

---

## 18. Errores comunes

| # | Error | Código | Solución |
|---|---|---|---|
| 1 | `:=` fuera de función | `port := 8080` a nivel paquete | Usar `var port = 8080` |
| 2 | Variable no usada | `x := 42` sin usar `x` | Usar `_` o eliminar |
| 3 | Import no usado | `import "fmt"` sin usar | Usar `goimports` |
| 4 | Shadow accidental de err | `:=` dentro de `if` crea nuevo `err` | Usar `=` o declarar variable antes |
| 5 | Shadow de builtin | `len := 42` | Nunca usar nombres de builtins |
| 6 | `:=` sin variable nueva | `x, y := 1, 2; x, y := 3, 4` | Al menos una variable debe ser nueva |
| 7 | `new` vs `make` | `new(map[string]int)` → nil map | Usar `make(map[string]int)` |
| 8 | var para todo | `var x int = 42` dentro de función | Usar `x := 42` (más idiomático) |
| 9 | Escribir en nil map | `var m map[string]int; m["k"] = 1` | Usar `make` primero |
| 10 | Ignorar error con `_` | `result, _ := riskyOp()` | Manejar el error |
| 11 | Init complejo en `init()` | DB connect en `init()` | Mover a `main()` o constructor |
| 12 | Asignar tipos distintos | `var x int = int64(42)` | Conversión explícita: `int(val)` |

---

## 19. Ejercicios

### Ejercicio 1: Predicción de declaraciones
```go
package main

import "fmt"

var x = 10

func main() {
    fmt.Println(x)
    x := 20
    fmt.Println(x)
    {
        x := 30
        fmt.Println(x)
    }
    fmt.Println(x)
}
```
**Predicción**: ¿Qué imprime? ¿Cuántas variables `x` distintas existen?

### Ejercicio 2: Shadowing de error
```go
package main

import (
    "fmt"
    "os"
)

func readConfig() (string, error) {
    var result string
    var err error

    if data, err := os.ReadFile("/etc/hostname"); err == nil {
        result = string(data)
    }

    return result, err
}

func main() {
    val, err := readConfig()
    fmt.Printf("val=%q err=%v\n", val, err)
}
```
**Predicción**: Si `/etc/hostname` existe y contiene "server1\n", ¿qué imprime? ¿Y si el archivo no existe?

### Ejercicio 3: Múltiples `:=`
```go
package main

import "fmt"

func main() {
    x, y := 1, 2
    fmt.Println(x, y)

    y, z := 3, 4
    fmt.Println(x, y, z)

    // ¿Compilará esta línea? ¿Por qué?
    // x, y = 5, 6
    // x, y := 5, 6
}
```
**Predicción**: ¿Qué imprime? ¿Cuál de las dos líneas comentadas compila y cuál no?

### Ejercicio 4: Scope en for
```go
package main

import "fmt"

func main() {
    total := 0
    for i := 0; i < 5; i++ {
        x := i * 2
        total += x
    }
    fmt.Println(total)
    // fmt.Println(i)  // ¿Qué pasa si descomentamos?
    // fmt.Println(x)  // ¿Y esto?
}
```
**Predicción**: ¿Qué imprime? ¿Qué errores dan las líneas comentadas?

### Ejercicio 5: `new` vs `make`
```go
package main

import "fmt"

func main() {
    a := new([]int)
    b := make([]int, 0)
    c := make([]int, 5)
    d := make([]int, 0, 10)

    fmt.Printf("a: %T, val=%v, nil=%t\n", a, *a, *a == nil)
    fmt.Printf("b: %T, val=%v, nil=%t, len=%d, cap=%d\n", b, b, b == nil, len(b), cap(b))
    fmt.Printf("c: %T, val=%v, len=%d, cap=%d\n", c, c, len(c), cap(c))
    fmt.Printf("d: %T, val=%v, len=%d, cap=%d\n", d, d, len(d), cap(d))
}
```
**Predicción**: ¿Qué tipo tiene `a` versus `b`? ¿Cuál es nil?

### Ejercicio 6: Init order
```go
package main

import "fmt"

var a = func() int { fmt.Println("init a"); return 1 }()
var b = func() int { fmt.Println("init b"); return a + 1 }()

func init() {
    fmt.Println("init() called, a =", a, "b =", b)
}

func main() {
    fmt.Println("main() called")
}
```
**Predicción**: ¿En qué orden aparecen los mensajes?

### Ejercicio 7: Blank identifier
```go
package main

import (
    "fmt"
    "strconv"
)

func main() {
    vals := []string{"42", "hello", "100", "", "7"}
    sum := 0

    for _, v := range vals {
        n, err := strconv.Atoi(v)
        if err != nil {
            continue
        }
        sum += n
    }

    fmt.Println("Sum:", sum)
}
```
**Predicción**: ¿Qué imprime? ¿Qué pasa con "hello" y ""?

### Ejercicio 8: Config con variables de paquete
Escribe un programa que:
1. Defina variables de paquete para `defaultHost`, `defaultPort`, `defaultLogLevel`
2. Tenga una función `init()` que las override desde variables de entorno
3. En `main()`, imprima la configuración resultante
4. Usa `var` a nivel paquete y `:=` dentro de funciones

### Ejercicio 9: Service checker refactored
Partiendo del ejemplo de la sección 16, modifica `checkService` para que:
1. Acepte un `context.Context` con timeout individual por servicio
2. Declare variables con el scope más estrecho posible
3. Use una goroutine por check con `sync.WaitGroup` y `sync.Mutex` para recopilar resultados

### Ejercicio 10: Variable detective
```go
package main

import (
    "fmt"
    "os"
)

var debug = false

func init() {
    if os.Getenv("DEBUG") != "" {
        debug = true
    }
}

func process(data []byte) (result string, err error) {
    if debug {
        fmt.Printf("processing %d bytes\n", len(data))
    }

    if len(data) == 0 {
        return "", fmt.Errorf("empty data")
    }

    result = string(data)

    if debug {
        fmt.Printf("result: %q\n", result)
    }

    return result, nil
}

func main() {
    result, err := process([]byte("hello"))
    if err != nil {
        fmt.Fprintf(os.Stderr, "error: %v\n", err)
        os.Exit(1)
    }
    fmt.Println(result)
}
```
**Predicción**: Enumera TODAS las variables del programa. Para cada una indica: nombre, tipo, scope (paquete/función/bloque), y cómo fue declarada (var/`:=`/parámetro/retorno nombrado).

---

## 20. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│          DECLARACIÓN DE VARIABLES — RESUMEN                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FORMAS DE DECLARAR                                          │
│  ├─ var name type          → zero value, tipo explícito      │
│  ├─ var name type = value  → tipo explícito + valor          │
│  ├─ var name = value       → tipo inferido                   │
│  ├─ name := value          → short (solo en funciones)       │
│  └─ var ( ... )            → bloque de declaraciones         │
│                                                              │
│  REGLAS CLAVE                                                │
│  ├─ := requiere al menos una variable nueva                  │
│  ├─ Variables locales no usadas = error de compilación       │
│  ├─ Shadowing es legal pero peligroso                        │
│  ├─ Scope es lexical (del { al } más cercano)                │
│  ├─ new(T) → *T con zero value (raro)                        │
│  └─ make(T) → T inicializado (slice, map, chan)              │
│                                                              │
│  IDIOMÁTICO                                                  │
│  ├─ := para variables locales (90% del tiempo)               │
│  ├─ var para zero values intencionales                       │
│  ├─ var para tipos de interfaz explícitos                    │
│  ├─ var() bloque para variables de paquete                   │
│  └─ _ para descartar valores no necesarios                   │
│                                                              │
│  SCOPE: paquete → función → bloque → inner block             │
│  INIT:  imports → vars de paquete → init() → main()          │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T02 - Constantes — `const`, `iota`, constantes sin tipo, enumeraciones
