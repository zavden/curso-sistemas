# init() — La Funcion Especial de Inicializacion

## 1. Introduccion

`init()` es una funcion especial en Go que se ejecuta **automaticamente** antes de `main()`. No la llamas tu — el runtime de Go la invoca durante la inicializacion del programa. Cada paquete puede tener multiples funciones `init()`, incluso en el mismo archivo, y todas se ejecutan en un orden determinista antes de que el programa empiece.

`init()` existe para configurar estado que **debe** existir antes de que cualquier codigo del paquete se use: registrar drivers de base de datos, compilar expresiones regulares, validar configuracion critica, inicializar tablas de lookup, o verificar dependencias del sistema.

**Sin embargo**, `init()` es una de las funciones mas **sobreusadas** y **mal entendidas** de Go. La comunidad Go recomienda usarla con mucha precaucion porque introduce estado global implicito, dificulta el testing, y hace que el orden de inicializacion sea dificil de razonar. En la mayoria de los casos, una funcion explicita como `Setup()` o un constructor `New()` es preferible.

```
┌──────────────────────────────────────────────────────────────┐
│              init() EN GO                                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  Reglas:                                                     │
│  ├─ Firma unica: func init() { }                            │
│  │   Sin parametros, sin retorno                             │
│  ├─ Se ejecuta automaticamente (no la llamas tu)             │
│  ├─ Se ejecuta ANTES de main()                               │
│  ├─ Puede haber MULTIPLES init() en el mismo archivo         │
│  ├─ No se puede referenciar: &init o init() manual → error   │
│  └─ No se puede llamar desde otro codigo                     │
│                                                              │
│  Orden de ejecucion:                                         │
│  1. Variables de paquete (const → var)                        │
│  2. init() de paquetes importados (recursivamente)           │
│  3. init() del paquete actual (en orden de archivo)          │
│  4. main() del paquete main                                  │
│                                                              │
│  Diagrama de orden:                                          │
│                                                              │
│  import "db"         import "log"                            │
│       ↓                   ↓                                  │
│  db/vars → db/init   log/vars → log/init                    │
│       ↓                   ↓                                  │
│       └─────────┬─────────┘                                  │
│                 ↓                                            │
│  main/vars → main/init → main()                             │
│                                                              │
│  Recomendacion de la comunidad Go:                           │
│  ├─ EVITAR init() siempre que sea posible                    │
│  ├─ Preferir constructores explicitos (New, Setup, MustX)    │
│  ├─ Usos legitimos: registrar drivers, compilar regex        │
│  └─ En la stdlib: database/sql, image, encoding, crypto     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Sintaxis y reglas basicas

### Declaracion

```go
// Firma unica — no acepta parametros ni retorna nada
func init() {
    // codigo de inicializacion
}

// ✗ Ninguna de estas variantes compila:
// func init(x int) {}           // cannot define init with parameters
// func init() error { return nil } // cannot define init with return values
// func init[T any]() {}         // init cannot have type parameters
```

### No se puede llamar ni referenciar

```go
func main() {
    // ✗ ERROR: cannot refer to init function
    // init()
    
    // ✗ ERROR: cannot take address of init
    // f := init
    
    // ✗ ERROR: cannot use init as value
    // fmt.Println(init)
}
```

### Multiples init() en el mismo archivo

```go
package main

import "fmt"

func init() {
    fmt.Println("init 1")
}

func init() {
    fmt.Println("init 2")
}

func init() {
    fmt.Println("init 3")
}

func main() {
    fmt.Println("main")
}

// Output:
// init 1
// init 2
// init 3
// main
```

**Nota**: `init` es la **unica** funcion en Go que puede declararse multiples veces con el mismo nombre en el mismo paquete (y en el mismo archivo). Cualquier otra funcion duplicada causa error de compilacion.

---

## 3. Orden de ejecucion — las reglas completas

### Orden dentro de un paquete

```
1. Evaluar constantes (const)
2. Evaluar variables de paquete (var) — en orden de declaracion
3. Ejecutar todas las funciones init() — en orden de aparicion en el archivo
```

```go
package main

import "fmt"

// Paso 1: constantes
const greeting = "hello"

// Paso 2: variables de paquete (se evaluan en orden)
var a = func() int {
    fmt.Println("var a initialized")
    return 1
}()

var b = func() int {
    fmt.Println("var b initialized")
    return 2
}()

// Paso 3: init()
func init() {
    fmt.Println("init called, a =", a, "b =", b)
}

// Paso 4: main
func main() {
    fmt.Println("main called")
}

// Output:
// var a initialized
// var b initialized
// init called, a = 1 b = 2
// main called
```

### Orden entre archivos del mismo paquete

```
Cuando un paquete tiene multiples archivos:
1. Los archivos se procesan en orden ALFABETICO por nombre de archivo
2. Dentro de cada archivo: vars → inits (en orden de aparicion)
```

```
mypackage/
├── a_setup.go      ← se procesa primero (a < b < z)
│   ├── var x = ...
│   └── func init() { ... }  ← init #1
├── b_config.go     ← se procesa segundo
│   ├── var y = ...
│   └── func init() { ... }  ← init #2
└── z_cleanup.go    ← se procesa ultimo
    └── func init() { ... }  ← init #3

Orden: x → init#1 → y → init#2 → init#3
```

**IMPORTANTE**: El orden entre archivos depende del nombre de archivo. Esto significa que renombrar un archivo puede cambiar el orden de inicializacion. Por eso dependencias entre `init()` de diferentes archivos son peligrosas.

### Orden entre paquetes (imports)

```go
// main.go
package main

import (
    "myapp/config"   // 1. Se inicializa primero (dependencia)
    "myapp/database"  // 2. Se inicializa segundo
    "myapp/server"    // 3. Se inicializa tercero
)

func main() { ... } // 4. main() al final
```

```
┌──────────────────────────────────────────────────────────────┐
│  ORDEN DE INICIALIZACION DE PAQUETES                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  El compilador construye un DAG (grafo aciclico dirigido)    │
│  de dependencias y los inicializa en orden topologico:       │
│                                                              │
│  main imports: config, database, server                      │
│  database imports: config                                    │
│  server imports: config, database                            │
│                                                              │
│  Orden resultante:                                           │
│  1. config    (sin dependencias propias)                     │
│  2. database  (depende de config, ya inicializado)           │
│  3. server    (depende de config y database, ya listos)      │
│  4. main                                                     │
│                                                              │
│  Regla: cada paquete se inicializa UNA SOLA VEZ,             │
│  aunque sea importado por multiples paquetes                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Garantia de ejecucion unica

```go
// Aunque config sea importado por main, database y server:
// config.init() se ejecuta UNA SOLA VEZ

// Esto es diferente de sync.Once (que es para runtime)
// init() es para compile-time/link-time initialization order
```

---

## 4. Usos legitimos de init()

### Uso 1: Registrar drivers (patron de la stdlib)

```go
// database/sql usa este patron extensivamente:
// El driver se registra al importar el paquete

// En el paquete del driver (ej: github.com/lib/pq):
package pq

import "database/sql"

func init() {
    sql.Register("postgres", &Driver{})
}

// En tu codigo:
package main

import (
    "database/sql"
    _ "github.com/lib/pq" // Side-effect import: solo ejecuta init()
)

func main() {
    db, err := sql.Open("postgres", connStr) // "postgres" ya esta registrado
    // ...
}
```

Otros paquetes de la stdlib que usan este patron:

```go
// image: registrar decodificadores
import _ "image/png"  // png.init() registra el decodificador PNG
import _ "image/jpeg" // jpeg.init() registra el decodificador JPEG

// crypto: registrar hash functions
import _ "crypto/sha256" // registra SHA-256

// encoding: registrar codecs
import _ "encoding/json"
```

### Uso 2: Compilar expresiones regulares

```go
package validator

import "regexp"

// Compilar una sola vez, reusar en cada llamada
var (
    emailRegex    *regexp.Regexp
    hostnameRegex *regexp.Regexp
    ipv4Regex     *regexp.Regexp
)

func init() {
    emailRegex = regexp.MustCompile(`^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$`)
    hostnameRegex = regexp.MustCompile(`^[a-z][a-z0-9-]{0,62}$`)
    ipv4Regex = regexp.MustCompile(`^(\d{1,3}\.){3}\d{1,3}$`)
}

func IsValidEmail(s string) bool { return emailRegex.MatchString(s) }
func IsValidHostname(s string) bool { return hostnameRegex.MatchString(s) }
```

**Alternativa sin init() (preferida)**:

```go
// regexp.MustCompile en var declaration — el mismo efecto, sin init()
var (
    emailRegex    = regexp.MustCompile(`^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$`)
    hostnameRegex = regexp.MustCompile(`^[a-z][a-z0-9-]{0,62}$`)
    ipv4Regex     = regexp.MustCompile(`^(\d{1,3}\.){3}\d{1,3}$`)
)
// ✓ Mas explicito, mismo resultado
// La inicializacion de var ocurre ANTES de init() de todas formas
```

### Uso 3: Verificar precondiciones del entorno

```go
package main

import (
    "log"
    "os"
    "os/exec"
    "runtime"
)

func init() {
    if runtime.GOOS != "linux" {
        log.Fatal("this tool requires Linux")
    }
    
    // Verificar que herramientas necesarias estan en PATH
    required := []string{"docker", "kubectl", "helm"}
    for _, tool := range required {
        if _, err := exec.LookPath(tool); err != nil {
            log.Fatalf("required tool not found: %s", tool)
        }
    }
}
```

### Uso 4: Inicializar tablas de lookup

```go
package httpstatus

var statusText map[int]string

func init() {
    statusText = map[int]string{
        200: "OK",
        201: "Created",
        204: "No Content",
        301: "Moved Permanently",
        302: "Found",
        400: "Bad Request",
        401: "Unauthorized",
        403: "Forbidden",
        404: "Not Found",
        500: "Internal Server Error",
        502: "Bad Gateway",
        503: "Service Unavailable",
    }
}

// Alternativa sin init() (preferida):
var statusText = map[int]string{
    200: "OK",
    201: "Created",
    // ... mismo contenido
}
```

---

## 5. ¿Por que evitar init()? — Los problemas

### Problema 1: Estado global implicito

```go
// ✗ Quien leea main() no sabe que init() ya configuro cosas
package main

var config Config // ¿Cuando se inicializo? ¿Con que valores?

func init() {
    data, err := os.ReadFile("/etc/myapp/config.json")
    if err != nil {
        log.Fatal(err) // ¡Fatal en init mata el programa sin cleanup!
    }
    json.Unmarshal(data, &config)
}

func main() {
    // config ya esta lista... pero ¿como lo sabes mirando solo main()?
    startServer(config)
}

// ✓ Version explicita:
func main() {
    config, err := loadConfig("/etc/myapp/config.json")
    if err != nil {
        log.Fatalf("cannot load config: %v", err)
    }
    startServer(config)
}
```

### Problema 2: Dificulta el testing

```go
// ✗ init() se ejecuta cuando el paquete se importa en tests
package database

var db *sql.DB

func init() {
    var err error
    db, err = sql.Open("postgres", os.Getenv("DATABASE_URL"))
    if err != nil {
        log.Fatal(err)
    }
    // ¡Cada test que importe este paquete intenta conectar a una DB real!
}

// ✓ Version testeable:
package database

func Connect(dsn string) (*sql.DB, error) {
    return sql.Open("postgres", dsn)
}

// En tests: pasar un DSN de test
// En produccion: llamar Connect(os.Getenv("DATABASE_URL"))
```

### Problema 3: Orden de inicializacion fragil

```go
// Archivo a_config.go:
package main

func init() {
    loadConfig() // ← Asume que el logger ya esta listo
}

// Archivo b_logger.go:
package main

func init() {
    setupLogger() // ← Asume que config ya esta lista
}

// ¿Cual se ejecuta primero? Depende del nombre del archivo:
// a_config.go → init de config (a < b)
// b_logger.go → init de logger
//
// ¡Si renombras b_logger.go a a_logger.go, el orden cambia!
// Este tipo de dependencia circular implicita es un bug esperando a pasar
```

### Problema 4: Panic en init() mata todo

```go
func init() {
    // Si esto falla, el programa entero muere sin que main() pueda reaccionar
    data, err := os.ReadFile("/etc/required-config.json")
    if err != nil {
        panic(fmt.Sprintf("critical config missing: %v", err))
        // No hay recover posible — el programa no ha empezado
        // No hay cleanup, no hay graceful degradation
    }
}

// ✓ En main(), puedes decidir que hacer:
func main() {
    data, err := os.ReadFile("/etc/required-config.json")
    if err != nil {
        // Intentar config por defecto, o modo degradado
        log.Printf("config missing, using defaults: %v", err)
        data = defaultConfig
    }
}
```

### Problema 5: Side-effect imports oscurecen dependencias

```go
import (
    _ "myapp/plugins/auth"    // ¿Que registra? ¿Donde?
    _ "myapp/plugins/cache"   // ¿Efectos secundarios?
    _ "myapp/plugins/metrics" // ¿Dependencias entre ellos?
)

// El lector no sabe que hacen estos imports sin leer cada init()
// Es como tener constructores estaticos ocultos
```

---

## 6. Alternativas a init()

### Alternativa 1: Inicializacion en var declaration

```go
// ✗ Con init():
var templates *template.Template

func init() {
    var err error
    templates, err = template.ParseGlob("templates/*.html")
    if err != nil {
        log.Fatal(err)
    }
}

// ✓ Sin init() — MustCompile/MustParse en var:
var templates = template.Must(template.ParseGlob("templates/*.html"))

// template.Must panic si hay error — mismo efecto que log.Fatal en init()
// pero mas explicito y visible en la declaracion
```

### Alternativa 2: Constructor explicito

```go
// ✗ Con init():
package cache

var store *redis.Client

func init() {
    store = redis.NewClient(&redis.Options{
        Addr: os.Getenv("REDIS_URL"),
    })
    if err := store.Ping(context.Background()).Err(); err != nil {
        log.Fatal(err)
    }
}

func Get(key string) (string, error) {
    return store.Get(context.Background(), key).Result()
}

// ✓ Sin init() — constructor explicito:
package cache

type Store struct {
    client *redis.Client
}

func NewStore(addr string) (*Store, error) {
    client := redis.NewClient(&redis.Options{Addr: addr})
    if err := client.Ping(context.Background()).Err(); err != nil {
        return nil, fmt.Errorf("redis connection failed: %w", err)
    }
    return &Store{client: client}, nil
}

func (s *Store) Get(key string) (string, error) {
    return s.client.Get(context.Background(), key).Result()
}

// En main():
store, err := cache.NewStore(os.Getenv("REDIS_URL"))
// Ahora puedes manejar el error, inyectar en tests, etc.
```

### Alternativa 3: sync.Once para lazy initialization

```go
// ✗ init() ejecuta la inicializacion aunque nadie use el paquete
// ✓ sync.Once: inicializa la primera vez que se necesita

package config

var (
    instance *Config
    once     sync.Once
    initErr  error
)

func Get() (*Config, error) {
    once.Do(func() {
        data, err := os.ReadFile("/etc/myapp/config.json")
        if err != nil {
            initErr = err
            return
        }
        instance = &Config{}
        initErr = json.Unmarshal(data, instance)
    })
    return instance, initErr
}

// La config se carga la primera vez que alguien llama Get()
// No antes, y exactamente una vez (thread-safe)
```

### Alternativa 4: Registro explicito (en lugar de side-effect imports)

```go
// ✗ Patron init() (como database/sql):
import _ "github.com/lib/pq" // magic side-effect

// ✓ Registro explicito:
package main

import (
    "myapp/database"
    "myapp/drivers/postgres"
    "myapp/drivers/mysql"
)

func main() {
    database.Register("postgres", postgres.New())
    database.Register("mysql", mysql.New())
    
    db, err := database.Open("postgres", connStr)
    // ...
}
```

---

## 7. init() en la stdlib — casos reales

### database/sql

```go
// La stdlib usa init() para el patron de registro de drivers.
// Este es probablemente el uso mas conocido.

// En github.com/lib/pq/conn.go:
func init() {
    sql.Register("postgres", &Driver{})
}

// sql.Register almacena el driver en un map global:
var (
    driversMu sync.RWMutex
    drivers   = make(map[string]driver.Driver)
)

func Register(name string, driver driver.Driver) {
    driversMu.Lock()
    defer driversMu.Unlock()
    if driver == nil {
        panic("sql: Register driver is nil")
    }
    if _, dup := drivers[name]; dup {
        panic("sql: Register called twice for driver " + name)
    }
    drivers[name] = driver
}
```

### net/http

```go
// net/http no usa init() para registrar handlers
// Pero el DefaultServeMux es una variable global inicializada sin init():
var DefaultServeMux = &defaultServeMux
var defaultServeMux ServeMux

// http.HandleFunc opera sobre el DefaultServeMux global
// Esto es similar a init() en espiritu: estado global implicito
```

### crypto/\*

```go
// crypto/sha256/sha256.go
func init() {
    crypto.RegisterHash(crypto.SHA256, New)
}

// Patron identico al de database/sql:
// importas _ "crypto/sha256" y el hash queda registrado
```

### image/\*

```go
// image/png/reader.go
func init() {
    image.RegisterFormat("png", pngHeader, Decode, DecodeConfig)
}

// image/jpeg/reader.go
func init() {
    image.RegisterFormat("jpeg", "\xff\xd8", Decode, DecodeConfig)
}

// Uso:
import (
    "image"
    _ "image/png"
    _ "image/jpeg"
)

img, format, err := image.Decode(file) // Detecta formato automaticamente
```

---

## 8. Multiples init() — cuando y como

### Multiples init() en el mismo archivo

```go
package main

import "fmt"

// Util para separar inicializaciones logicamente diferentes
// Cada init() es independiente y se ejecuta en orden de aparicion

func init() {
    fmt.Println("Phase 1: setting up logging")
    // setupLogging()
}

func init() {
    fmt.Println("Phase 2: loading config")
    // loadConfig()
}

func init() {
    fmt.Println("Phase 3: connecting to database")
    // connectDB()
}

func main() {
    fmt.Println("Ready")
}

// Output:
// Phase 1: setting up logging
// Phase 2: loading config
// Phase 3: connecting to database
// Ready
```

### ¿Por que permitir multiples init()?

```
Razones:
1. Generacion de codigo: herramientas pueden generar archivos con init()
   sin preocuparse por conflictos de nombres
2. Separacion logica: diferentes aspectos de inicializacion aislados
3. Build tags: diferentes init() activos segun OS/arch

Pero en la practica:
- Multiples init() en el mismo archivo es raro y confuso
- Si necesitas multiples fases, usa una sola init() con llamadas a funciones
- O mejor: no uses init() en absoluto
```

### Build tags y init()

```go
// config_linux.go
//go:build linux

package config

func init() {
    configDir = "/etc/myapp"
    logDir = "/var/log/myapp"
}

// config_darwin.go
//go:build darwin

package config

func init() {
    home, _ := os.UserHomeDir()
    configDir = filepath.Join(home, ".config", "myapp")
    logDir = filepath.Join(home, "Library", "Logs", "myapp")
}

// Solo uno se compila segun la plataforma
// Esto es un uso razonable de init()
```

---

## 9. init() y dependencias circulares

### El problema

```go
// package a
package a

import "myapp/b"

func init() {
    b.SetCallback(myCallback) // a depende de b
}

// package b
package b

import "myapp/a"

func init() {
    a.SetHandler(myHandler) // b depende de a
}

// ✗ ERROR de compilacion:
// import cycle not allowed
// package myapp/a
//     imports myapp/b
//     imports myapp/a

// Go prohibe imports circulares — esto es una feature, no un bug
// Te obliga a diseñar bien las dependencias
```

### Solucion: paquete intermediario

```go
// package types (sin dependencias)
package types

type Callback func(event string)
type Handler func(data []byte) error

// package a (importa types, no b)
package a

import "myapp/types"

var callback types.Callback

func SetCallback(cb types.Callback) { callback = cb }

// package b (importa types, no a)
package b

import "myapp/types"

var handler types.Handler

func SetHandler(h types.Handler) { handler = h }

// package main (importa ambos, los conecta)
package main

import (
    "myapp/a"
    "myapp/b"
)

func main() {
    a.SetCallback(b.HandleEvent)
    b.SetHandler(a.ProcessData)
}
```

---

## 10. init() y testing

### El problema con tests

```go
// mypackage/db.go
package mypackage

var dbConn *sql.DB

func init() {
    var err error
    dbConn, err = sql.Open("postgres", os.Getenv("DATABASE_URL"))
    if err != nil {
        log.Fatal(err) // ¡Mata el test runner!
    }
}

// mypackage/db_test.go
package mypackage

func TestSomething(t *testing.T) {
    // init() ya se ejecuto — intento conectar a DB real
    // Si DATABASE_URL no esta seteado → log.Fatal → test muere
    
    // No puedes evitar que init() se ejecute
    // No puedes inyectar una conexion de test
    // No puedes testear sin una DB real corriendo
}
```

### Patron testeable (sin init)

```go
// mypackage/db.go
package mypackage

type DB struct {
    conn *sql.DB
}

func NewDB(dsn string) (*DB, error) {
    conn, err := sql.Open("postgres", dsn)
    if err != nil {
        return nil, err
    }
    return &DB{conn: conn}, nil
}

// mypackage/db_test.go
package mypackage

func TestSomething(t *testing.T) {
    // Opcion 1: usar DB de test
    db, err := NewDB("postgres://localhost:5432/testdb?sslmode=disable")
    if err != nil {
        t.Skip("test database not available")
    }
    
    // Opcion 2: usar interface mock
    // Opcion 3: usar testcontainers
    
    // El test controla la inicializacion completamente
}
```

### TestMain: cuando necesitas setup global para tests

```go
// Si REALMENTE necesitas inicializacion global en tests,
// usa TestMain en lugar de init():

package mypackage

var testDB *sql.DB

func TestMain(m *testing.M) {
    // Setup
    var err error
    testDB, err = sql.Open("postgres", os.Getenv("TEST_DATABASE_URL"))
    if err != nil {
        log.Fatalf("test setup failed: %v", err)
    }
    
    // Ejecutar tests
    code := m.Run()
    
    // Teardown
    testDB.Close()
    
    os.Exit(code)
}

func TestQuery(t *testing.T) {
    // testDB ya esta listo
}
```

---

## 11. Side-effect imports (import _)

### Mecanica

```go
// import con _ descarta el nombre del paquete
// El paquete se importa SOLO por sus side effects (init functions)

import _ "net/http/pprof" // Registra handlers de profiling en DefaultServeMux

// Equivale a:
// 1. Se ejecutan las variables de paquete de net/http/pprof
// 2. Se ejecutan las init() de net/http/pprof
// 3. No puedes usar ninguna funcion exportada de pprof
```

### Ejemplo: pprof para diagnostico

```go
package main

import (
    "log"
    "net/http"
    _ "net/http/pprof" // init() registra /debug/pprof/* handlers
)

func main() {
    // Los endpoints de pprof ya estan registrados:
    // /debug/pprof/
    // /debug/pprof/goroutine
    // /debug/pprof/heap
    // /debug/pprof/profile
    
    log.Fatal(http.ListenAndServe(":6060", nil))
}

// En produccion SysAdmin:
// curl http://localhost:6060/debug/pprof/goroutine?debug=1
// go tool pprof http://localhost:6060/debug/pprof/heap
```

### Ejemplo: embed con init()

```go
package main

import (
    "embed"
    "net/http"
)

//go:embed static/*
var staticFiles embed.FS

// No es init(), pero es un patron similar:
// embed procesa la directiva en compile time
// El contenido esta disponible como variable de paquete
```

### Cuando es aceptable import _

```
Aceptable:
├─ Drivers de base de datos: _ "github.com/lib/pq"
├─ Decodificadores de imagen: _ "image/png"
├─ Profiling: _ "net/http/pprof"
├─ Hash algorithms: _ "crypto/sha256"
└─ Instrumentacion: _ "go.uber.org/automaxprocs"

Cuestionable:
├─ Tus propios paquetes con side effects
├─ Mas de 3-4 side-effect imports
└─ Imports cuyo efecto no es obvio sin leer el codigo
```

---

## 12. init() vs sync.Once

### Comparacion detallada

```go
// init(): se ejecuta una vez, al inicio, incondicionalmente
var config Config

func init() {
    // Se ejecuta SIEMPRE, incluso si nadie usa config
    data, _ := os.ReadFile("/etc/myapp/config.json")
    json.Unmarshal(data, &config)
}

// sync.Once: se ejecuta una vez, cuando se necesita (lazy)
var (
    config   Config
    configOnce sync.Once
)

func getConfig() Config {
    configOnce.Do(func() {
        // Se ejecuta SOLO la primera vez que se llama getConfig()
        data, _ := os.ReadFile("/etc/myapp/config.json")
        json.Unmarshal(data, &config)
    })
    return config
}
```

### Tabla comparativa

```
┌──────────────────┬──────────────────────────┬──────────────────────────┐
│ Aspecto          │ init()                   │ sync.Once                │
├──────────────────┼──────────────────────────┼──────────────────────────┤
│ Cuando ejecuta   │ Al inicio del programa   │ Primera llamada          │
│ Control          │ Automatico (no lo llamas) │ Manual (tu decides)      │
│ Lazy             │ No (eagerly)             │ Si (on-demand)           │
│ Parametros       │ No                       │ Via closure              │
│ Retorno          │ No                       │ Via variable capturada   │
│ Error handling   │ Fatal/panic (no retorno) │ Puedes manejar errores   │
│ Testing          │ No se puede skip/mock    │ Se puede controlar       │
│ Thread-safe      │ Si (ejecuta antes main)  │ Si (lock interno)        │
│ Multiples veces  │ Exactamente 1            │ Exactamente 1            │
│ Visibilidad      │ Implicita                │ Explicita                │
│ Uso recomendado  │ Registrar drivers        │ Lazy init, singletons    │
└──────────────────┴──────────────────────────┴──────────────────────────┘
```

### sync.Once con error handling

```go
type DBPool struct {
    db     *sql.DB
    once   sync.Once
    initErr error
}

func (p *DBPool) Get() (*sql.DB, error) {
    p.once.Do(func() {
        p.db, p.initErr = sql.Open("postgres", os.Getenv("DATABASE_URL"))
        if p.initErr != nil {
            return
        }
        p.initErr = p.db.Ping()
    })
    return p.db, p.initErr
}

// ✓ El error se retorna, no se oculta con log.Fatal
// ✓ El caller decide que hacer: retry, fallback, abort
// ✓ En tests: puedes crear un DBPool con un DSN de test
```

---

## 13. Patron SysAdmin: init() para CLI tools

### Caso aceptable: verificar prerrequisitos

```go
package main

import (
    "fmt"
    "log"
    "os"
    "os/exec"
    "runtime"
)

// Para CLI tools de un solo binario, init() es mas aceptable
// porque no hay tests complejos y el programa hace una cosa

func init() {
    // Verificar que corremos en Linux
    if runtime.GOOS != "linux" {
        fmt.Fprintf(os.Stderr, "Error: this tool requires Linux (got %s)\n", runtime.GOOS)
        os.Exit(1)
    }
    
    // Verificar root (si necesario)
    if os.Geteuid() != 0 {
        fmt.Fprintf(os.Stderr, "Error: this tool requires root privileges\n")
        os.Exit(1)
    }
}

func main() {
    // Si llegamos aqui, estamos en Linux como root
    // ...
}
```

### Caso aceptable: configurar log format

```go
func init() {
    log.SetFlags(log.Ldate | log.Ltime | log.Lshortfile)
    log.SetPrefix("[myapp] ")
    
    // O redirigir a archivo:
    if logPath := os.Getenv("MYAPP_LOG"); logPath != "" {
        f, err := os.OpenFile(logPath, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
        if err != nil {
            log.Fatal(err) // OK en CLI tool simple
        }
        log.SetOutput(f)
    }
}
```

### Caso cuestionable: init() para dependencias complejas

```go
// ✗ Evitar en bibliotecas y servicios complejos:
func init() {
    // Conectar a Consul, cargar feature flags, inicializar tracing...
    // Todo esto deberia ser explicito en main()
}

// ✓ En main() explicito:
func run() error {
    consul, err := connectConsul()
    if err != nil {
        return fmt.Errorf("consul: %w", err)
    }
    
    flags, err := loadFeatureFlags(consul)
    if err != nil {
        return fmt.Errorf("feature flags: %w", err)
    }
    
    tracer, err := initTracing()
    if err != nil {
        return fmt.Errorf("tracing: %w", err)
    }
    defer tracer.Shutdown()
    
    return startServer(flags, tracer)
}

func main() {
    if err := run(); err != nil {
        log.Fatal(err)
    }
}
```

---

## 14. Comparacion: Go vs C vs Rust

### Go: init()

```go
// init() se ejecuta antes de main, automaticamente
package main

var config = loadDefaults()

func init() {
    overrideFromEnv(&config)
}

func main() {
    // config ya esta lista
}
```

### C: sin init() (workarounds)

```c
// C no tiene funciones de inicializacion automatica (standard)

// Workaround 1: inicializacion en main()
int main(int argc, char *argv[]) {
    setup_logging();
    load_config();
    init_database();
    
    // programa real empieza aqui
    return run(argc, argv);
}

// Workaround 2: GCC constructor attribute (extension, no standard)
__attribute__((constructor))
void my_init(void) {
    // Se ejecuta antes de main()
    printf("initialized\n");
}

__attribute__((destructor))
void my_cleanup(void) {
    // Se ejecuta despues de main()
    printf("cleaned up\n");
}

// Workaround 3: C++ usa constructores de objetos globales
// (pero eso es C++, no C, y tiene el "static initialization order fiasco")

// Workaround 4: Dynamic linker (DT_INIT en ELF)
// Compartido con .init section en ELF binaries
```

### Rust: sin init() (constructores explicitos)

```rust
// Rust NO tiene init() ni constructores estaticos
// La filosofia de Rust: toda inicializacion es explicita

// Patron 1: lazy_static (crate externo, historico)
use lazy_static::lazy_static;

lazy_static! {
    static ref CONFIG: Config = load_config();
    static ref REGEX: Regex = Regex::new(r"^[a-z]+$").unwrap();
}

// Patron 2: std::sync::OnceLock (Rust 1.70+, stdlib)
use std::sync::OnceLock;

static CONFIG: OnceLock<Config> = OnceLock::new();

fn get_config() -> &'static Config {
    CONFIG.get_or_init(|| load_config())
}

// Patron 3: Inicializacion explicita en main
fn main() {
    let config = Config::load().expect("failed to load config");
    let db = Database::connect(&config.db_url).expect("db connection failed");
    
    run(config, db);
}

// Rust no tiene side-effect imports
// Los traits se registran explicitamente en el type system
// No hay "magic" de inicializacion
```

### Tabla comparativa

```
┌──────────────────────┬──────────────────────┬──────────────────────┬──────────────────────┐
│ Aspecto              │ Go                   │ C                    │ Rust                 │
├──────────────────────┼──────────────────────┼──────────────────────┼──────────────────────┤
│ Init automatico      │ init()               │ No (extensiones GCC) │ No                   │
│ Multiples por file   │ Si                   │ N/A                  │ N/A                  │
│ Orden determinista   │ Si (DAG de imports)  │ No garantizado       │ N/A                  │
│ Side-effect imports  │ import _ "pkg"       │ Linker scripts       │ No existe            │
│ Lazy init            │ sync.Once            │ pthread_once         │ OnceLock/LazyLock    │
│ Driver registration  │ init() + sql.Register│ Manual               │ Traits explicitos    │
│ Constructor global   │ var x = expr         │ GCC constructor      │ lazy_static/OnceLock │
│ Filosofia            │ "Magic" controlado   │ Nada automatico      │ Todo explicito       │
│ Testing impact       │ Alto (no evitable)   │ Bajo (no existe)     │ Bajo (no existe)     │
│ Comunidad recomienda │ Evitar en general    │ N/A                  │ Explicito siempre    │
└──────────────────────┴──────────────────────┴──────────────────────┴──────────────────────┘
```

---

## 15. La decision: ¿usar init() o no?

### Flowchart de decision

```
¿Necesitas inicializar algo antes de que el paquete se use?
│
├─ No → No uses init()
│
└─ Si → ¿Puedes hacerlo en una declaracion var?
         │
         ├─ Si → var x = expr (sin init)
         │       ej: var re = regexp.MustCompile(...)
         │
         └─ No → ¿Estas registrando un driver/codec/handler?
                  │
                  ├─ Si → init() es aceptable
                  │       (patron de la stdlib)
                  │
                  └─ No → ¿El paquete es una CLI tool simple?
                           │
                           ├─ Si → init() para prerrequisitos es OK
                           │       (verificar OS, root, tools en PATH)
                           │
                           └─ No → ¿Es una biblioteca reutilizable?
                                    │
                                    ├─ Si → NO uses init()
                                    │       Usa New()/Setup()/MustX()
                                    │
                                    └─ No → Probablemente no uses init()
                                            Usa sync.Once o constructores
```

### Resumen de la regla

```
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│   REGLA GENERAL: no uses init() a menos que:                 │
│                                                              │
│   1. Estes escribiendo un driver que se registra             │
│      (database/sql, image, crypto)                           │
│                                                              │
│   2. Necesites compilar un regex/template una sola vez       │
│      (pero var x = regexp.MustCompile() es mejor)            │
│                                                              │
│   3. Necesites verificar prerrequisitos del sistema          │
│      en una CLI tool simple                                  │
│                                                              │
│   En todos los demas casos: constructor explicito            │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 16. Programa completo: Comparacion init() vs explicito

Dos versiones del mismo programa — un service manager — para ilustrar la diferencia entre usar init() y no usarlo.

### Version con init() (antipatron)

```go
package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"strings"
	"time"
)

// ─── Estado global inicializado en init() ───────────────────────

var (
	hostname    string
	config      AppConfig
	logger      *log.Logger
	startupTime time.Time
)

type AppConfig struct {
	Services   []string `json:"services"`
	CheckInterval int   `json:"check_interval_sec"`
	LogFile    string   `json:"log_file"`
	AlertEmail string   `json:"alert_email"`
}

func init() {
	startupTime = time.Now()

	var err error
	hostname, err = os.Hostname()
	if err != nil {
		log.Fatal("cannot get hostname: ", err)
	}
}

func init() {
	data, err := os.ReadFile("/etc/svcmon/config.json")
	if err != nil {
		log.Fatalf("cannot read config: %v", err)
	}
	if err := json.Unmarshal(data, &config); err != nil {
		log.Fatalf("cannot parse config: %v", err)
	}
	if len(config.Services) == 0 {
		log.Fatal("no services configured")
	}
	if config.CheckInterval == 0 {
		config.CheckInterval = 30
	}
}

func init() {
	logFile := config.LogFile
	if logFile == "" {
		logFile = "/var/log/svcmon.log"
	}
	f, err := os.OpenFile(logFile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		log.Fatalf("cannot open log file: %v", err)
	}
	logger = log.New(f, fmt.Sprintf("[svcmon@%s] ", hostname), log.Ldate|log.Ltime)
}

// ─── Logica del programa ────────────────────────────────────────

func checkService(name string) (string, error) {
	output, err := exec.Command("systemctl", "is-active", name).Output()
	if err != nil {
		return "unknown", err
	}
	return strings.TrimSpace(string(output)), nil
}

func main() {
	// ¿De donde viene config? ¿hostname? ¿logger?
	// Hay que leer 3 init() para entender el estado
	
	logger.Printf("Starting (services: %s)", strings.Join(config.Services, ", "))
	
	for _, svc := range config.Services {
		status, err := checkService(svc)
		if err != nil {
			logger.Printf("CHECK FAIL %s: %v", svc, err)
			continue
		}
		logger.Printf("%-20s %s", svc, status)
	}
	
	logger.Printf("Completed in %v", time.Since(startupTime))
}
```

### Version sin init() (recomendada)

```go
package main

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"os/exec"
	"strings"
	"time"
)

// ─── Tipos ──────────────────────────────────────────────────────

type AppConfig struct {
	Services      []string `json:"services"`
	CheckInterval int      `json:"check_interval_sec"`
	LogFile       string   `json:"log_file"`
	AlertEmail    string   `json:"alert_email"`
}

type App struct {
	hostname string
	config   AppConfig
	logger   *log.Logger
	started  time.Time
}

// ─── Constructor explicito ──────────────────────────────────────

func NewApp(configPath string) (*App, error) {
	app := &App{
		started: time.Now(),
	}

	// Hostname
	var err error
	app.hostname, err = os.Hostname()
	if err != nil {
		return nil, fmt.Errorf("cannot get hostname: %w", err)
	}

	// Config
	data, err := os.ReadFile(configPath)
	if err != nil {
		return nil, fmt.Errorf("cannot read config %s: %w", configPath, err)
	}
	if err := json.Unmarshal(data, &app.config); err != nil {
		return nil, fmt.Errorf("cannot parse config: %w", err)
	}
	if len(app.config.Services) == 0 {
		return nil, fmt.Errorf("no services configured")
	}
	if app.config.CheckInterval == 0 {
		app.config.CheckInterval = 30
	}

	// Logger
	logFile := app.config.LogFile
	if logFile == "" {
		logFile = "/var/log/svcmon.log"
	}
	f, err := os.OpenFile(logFile, os.O_APPEND|os.O_CREATE|os.O_WRONLY, 0644)
	if err != nil {
		return nil, fmt.Errorf("cannot open log file %s: %w", logFile, err)
	}
	app.logger = log.New(f, fmt.Sprintf("[svcmon@%s] ", app.hostname), log.Ldate|log.Ltime)

	return app, nil
}

// ─── Metodos ────────────────────────────────────────────────────

func (a *App) CheckService(name string) (string, error) {
	output, err := exec.Command("systemctl", "is-active", name).Output()
	if err != nil {
		return "unknown", err
	}
	return strings.TrimSpace(string(output)), nil
}

func (a *App) Run() error {
	a.logger.Printf("Starting (services: %s)", strings.Join(a.config.Services, ", "))

	var failures int
	for _, svc := range a.config.Services {
		status, err := a.CheckService(svc)
		if err != nil {
			a.logger.Printf("CHECK FAIL %s: %v", svc, err)
			failures++
			continue
		}
		a.logger.Printf("%-20s %s", svc, status)
	}

	a.logger.Printf("Completed in %v (%d failures)", time.Since(a.started), failures)

	if failures > 0 {
		return fmt.Errorf("%d service checks failed", failures)
	}
	return nil
}

// ─── Main explicito ─────────────────────────────────────────────

func main() {
	configPath := "/etc/svcmon/config.json"
	if len(os.Args) > 1 {
		configPath = os.Args[1]
	}

	// Todo es explicito: de donde viene el config, como se maneja el error
	app, err := NewApp(configPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}

	if err := app.Run(); err != nil {
		fmt.Fprintf(os.Stderr, "Error: %v\n", err)
		os.Exit(1)
	}
}
```

### Diferencias clave

```
┌──────────────────────────────────────────────────────────────┐
│  VERSION init()              │  VERSION EXPLICITA            │
├──────────────────────────────┼───────────────────────────────┤
│ Estado global mutable        │ Estado encapsulado en struct  │
│ 3 init() en cadena           │ 1 constructor NewApp()       │
│ log.Fatal mata el programa   │ Retorna error al caller      │
│ Config path hardcoded        │ Config path parametrizable   │
│ No se puede testear          │ Facil de testear (inyectar)  │
│ Quien lee main() no sabe     │ Quien lee main() entiende    │
│   de donde viene el estado   │   todo el flujo              │
│ logger no se puede cerrar    │ Se puede cerrar en defer     │
│ Sin dependency injection     │ DI natural via constructor   │
└──────────────────────────────┴───────────────────────────────┘
```

---

## 17. Herramientas y linters

### go vet y init()

```bash
# go vet no advierte sobre init() por defecto
# Pero si detecta problemas dentro de init():
go vet ./...
```

### golangci-lint: gochecknoinits

```yaml
# .golangci.yml
linters:
  enable:
    - gochecknoinits  # Advierte sobre cualquier uso de init()

# Output:
# mypackage/setup.go:5:1: don't use `init` function (gochecknoinits)
```

### golangci-lint: gochecknoglobals

```yaml
# .golangci.yml
linters:
  enable:
    - gochecknoglobals  # Advierte sobre variables globales mutables

# Combinado con gochecknoinits:
# Te fuerza a usar constructores explicitos y dependency injection
```

### Excepciones en el linter

```go
// Si decides usar init() con justificacion, puedes silenciar el linter:

//nolint:gochecknoinits // required for driver registration
func init() {
    sql.Register("mydriver", &Driver{})
}
```

---

## 18. Tabla de errores comunes

```
┌────┬──────────────────────────────────────┬────────────────────────────────────────┬─────────┐
│ #  │ Error                                │ Solucion                               │ Nivel   │
├────┼──────────────────────────────────────┼────────────────────────────────────────┼─────────┤
│  1 │ init() con parametros o retorno      │ Firma unica: func init() {}           │ Compile │
│  2 │ Llamar init() manualmente            │ Imposible — no se puede referenciar   │ Compile │
│  3 │ Referenciar &init o asignar a var    │ Imposible — init no es un valor       │ Compile │
│  4 │ Import circular via init()           │ Refactorizar con paquete intermediario │ Compile │
│  5 │ log.Fatal en init() sin cleanup      │ Mover a main(), retornar error        │ Design  │
│  6 │ Estado global mutable sin mutex      │ Encapsular en struct con constructor   │ Race    │
│  7 │ Dependencia de orden entre inits     │ Orden depende de nombre de archivo    │ Logic   │
│  8 │ init() que conecta a servicios       │ Hace tests imposibles sin el servicio │ Test    │
│  9 │ Exceso de side-effect imports        │ Registro explicito cuando sea posible │ Design  │
│ 10 │ init() para config que podria fallar │ Constructor con retorno de error      │ Design  │
│ 11 │ init() en biblioteca reutilizable    │ SIEMPRE usar constructores explicitos │ Design  │
│ 12 │ Asumir que init() es lazy            │ init() es eager, usar sync.Once       │ Logic   │
└────┴──────────────────────────────────────┴────────────────────────────────────────┴─────────┘
```

---

## 19. Ejercicios de prediccion

**Ejercicio 1**: ¿Que imprime?

```go
package main

import "fmt"

var x = 1

func init() { x = 2 }

func main() {
    fmt.Println(x)
}
```

<details>
<summary>Respuesta</summary>

```
2
```

Orden: `var x = 1` → `init()` cambia x a 2 → `main()` imprime 2.
</details>

**Ejercicio 2**: ¿Que imprime?

```go
package main

import "fmt"

func init() { fmt.Println("init 1") }
func init() { fmt.Println("init 2") }
func main() { fmt.Println("main") }
func init() { fmt.Println("init 3") }
```

<details>
<summary>Respuesta</summary>

```
init 1
init 2
init 3
main
```

Las funciones `init()` se ejecutan en orden de aparicion en el archivo, antes de `main()`. No importa que `init 3` este declarado despues de `main` — lo que importa es el orden textual de las funciones init entre si.
</details>

**Ejercicio 3**: ¿Compila?

```go
package main

func init() int { return 0 }
func main() {}
```

<details>
<summary>Respuesta</summary>

No compila. Error: `func init must have no arguments and no return values`.
</details>

**Ejercicio 4**: ¿Compila?

```go
package main

func main() {
    init()
}

func init() {}
```

<details>
<summary>Respuesta</summary>

No compila. Error: `undefined: init`. No se puede llamar a `init()` explicitamente.
</details>

**Ejercicio 5**: ¿Que imprime?

```go
package main

import "fmt"

var a = func() int {
    fmt.Println("var a")
    return 1
}()

func init() {
    fmt.Println("init, a =", a)
}

var b = func() int {
    fmt.Println("var b")
    return 2
}()

func main() {
    fmt.Println("main, a =", a, "b =", b)
}
```

<details>
<summary>Respuesta</summary>

```
var a
var b
init, a = 1
main, a = 1 b = 2
```

Orden: todas las variables de paquete se inicializan primero (en orden de declaracion), luego todas las `init()`, luego `main()`.
</details>

**Ejercicio 6**: ¿Que imprime?

```go
package main

import "fmt"

var x = f()

func f() int {
    fmt.Println("f called")
    return 42
}

func init() {
    fmt.Println("init, x =", x)
    x = 100
}

func main() {
    fmt.Println("main, x =", x)
}
```

<details>
<summary>Respuesta</summary>

```
f called
init, x = 42
main, x = 100
```

`var x = f()` evalua f() → x=42. init() lee x=42 y lo cambia a 100. main() ve x=100.
</details>

**Ejercicio 7**: ¿Compila y que imprime?

```go
package main

import "fmt"

func init() {
    fmt.Println("init A")
}

func init() {
    fmt.Println("init B")
}

func init() {
    fmt.Println("init C")
}

func main() {
    fmt.Println("main")
}
```

<details>
<summary>Respuesta</summary>

Si compila — multiples `init()` es valido (unica funcion en Go con esta propiedad).

```
init A
init B
init C
main
```
</details>

**Ejercicio 8**: Si tengo dos archivos en el mismo paquete:

```
// a.go
func init() { fmt.Println("a init") }

// b.go
func init() { fmt.Println("b init") }
```

¿Cual se ejecuta primero?

<details>
<summary>Respuesta</summary>

`a init` se ejecuta primero. Los archivos se procesan en orden alfabetico: `a.go` antes de `b.go`.
</details>

**Ejercicio 9**: ¿Compila?

```go
package main

func main() {
    f := init
    f()
}

func init() {}
```

<details>
<summary>Respuesta</summary>

No compila. Error: `undefined: init`. `init` no es un valor y no puede asignarse a una variable.
</details>

**Ejercicio 10**: ¿Que imprime?

```go
package main

import "fmt"

func init() {
    defer fmt.Println("deferred in init")
    fmt.Println("init body")
}

func main() {
    fmt.Println("main")
}
```

<details>
<summary>Respuesta</summary>

```
init body
deferred in init
main
```

`defer` funciona normalmente dentro de `init()`. El defer se ejecuta al salir de `init()`, antes de que empiece `main()`.
</details>

---

## 20. Resumen visual

```
┌──────────────────────────────────────────────────────────────┐
│              init() — RESUMEN                                │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  REGLAS                                                      │
│  ├─ func init() {} — sin params, sin retorno                 │
│  ├─ Se ejecuta automaticamente antes de main()               │
│  ├─ Puede haber multiples en el mismo archivo                │
│  ├─ No se puede llamar ni referenciar                        │
│  └─ Unica funcion con nombre duplicado permitido             │
│                                                              │
│  ORDEN DE EJECUCION                                          │
│  ├─ 1. const evaluadas                                       │
│  ├─ 2. var de paquete (en orden de declaracion)              │
│  ├─ 3. init() de paquetes importados (DAG topologico)        │
│  ├─ 4. init() del paquete actual (orden archivo → aparicion) │
│  ├─ 5. main()                                                │
│  └─ Archivos: orden ALFABETICO por nombre                    │
│                                                              │
│  USOS ACEPTABLES                                             │
│  ├─ Registrar drivers: sql.Register(), image.RegisterFormat()│
│  ├─ Verificar prerrequisitos en CLI tools simples            │
│  ├─ Build tags: init diferente por OS/arch                   │
│  └─ Compilar regex (pero var x = MustCompile es mejor)       │
│                                                              │
│  USOS A EVITAR                                               │
│  ├─ Conectar a servicios externos (DB, API, cache)           │
│  ├─ Cargar config que puede fallar                           │
│  ├─ Cualquier cosa en bibliotecas reutilizables              │
│  ├─ Dependencias entre init() de diferentes archivos         │
│  └─ Estado global mutable sin proteccion                     │
│                                                              │
│  ALTERNATIVAS PREFERIDAS                                     │
│  ├─ var x = expr (para init simple sin error)                │
│  ├─ NewXxx() constructor (retorna error)                     │
│  ├─ sync.Once (lazy, thread-safe, con error)                 │
│  ├─ MustXxx() (panic controlado, solo en startup)            │
│  └─ TestMain() (para setup de tests, no init())              │
│                                                              │
│  LINTERS                                                     │
│  ├─ gochecknoinits: advierte sobre cualquier init()          │
│  └─ gochecknoglobals: advierte sobre globals mutables        │
│                                                              │
│  GO vs C vs RUST                                             │
│  ├─ Go: init() automatico, determinista, pero implicito      │
│  ├─ C: no tiene init (GCC constructor = extension)           │
│  └─ Rust: no tiene init, todo es explicito (OnceLock)        │
│                                                              │
│  REGLA DE ORO                                                │
│  Si tu primer instinto es escribir init():                    │
│  preguntate si NewXxx() o sync.Once no seria mejor.          │
│  Casi siempre lo es.                                         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T03 - Metodos — receptor (value receiver vs pointer receiver), method sets, cuando usar cada uno
