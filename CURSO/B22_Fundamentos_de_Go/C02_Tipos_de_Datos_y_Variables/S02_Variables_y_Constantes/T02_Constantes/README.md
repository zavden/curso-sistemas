# Constantes en Go

## 1. Introducción

Las constantes en Go son valores que se determinan en **tiempo de compilación** y no pueden cambiar durante la ejecución. A diferencia de las variables, las constantes nunca ocupan una dirección de memoria asignable — el compilador las sustituye directamente en el código generado (inline).

Go tiene un sistema de constantes único entre lenguajes modernos: las **constantes sin tipo (untyped constants)** que mantienen precisión arbitraria hasta que se asignan a una variable concreta. Este sistema elimina la necesidad de casting en la mayoría de expresiones numéricas.

```
┌──────────────────────────────────────────────────────────────┐
│             CONSTANTES EN GO                                 │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  const Port = 8080           ← sin tipo (untyped int)        │
│  const Port int = 8080       ← con tipo (typed int)          │
│                                                              │
│  const (                     ← bloque de constantes          │
│      A = 1                                                   │
│      B = 2                                                   │
│  )                                                           │
│                                                              │
│  const (                     ← enumeración con iota          │
│      Read  = iota            // 0                            │
│      Write                   // 1                            │
│      Exec                    // 2                            │
│  )                                                           │
│                                                              │
│  Tipos permitidos: bool, numéricos, string                   │
│  NO permitidos: slice, map, struct, función, puntero         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Declaración de constantes — `const`

### Constante individual

```go
// Sin tipo (untyped) — la forma más común y flexible
const Pi = 3.14159265358979323846
const MaxRetries = 5
const AppName = "sysmon"
const Debug = false

// Con tipo (typed) — fija el tipo explícitamente
const Port int = 8080
const Timeout time.Duration = 30 * time.Second  // ✗ ERROR
// time.Duration no es literal — las constantes solo aceptan tipos básicos
```

### Bloque de constantes

```go
// Agrupación semántica — patrón estándar en Go
const (
    AppName    = "sysmon"
    AppVersion = "2.1.0"
    AppAuthor  = "ops-team"
)

const (
    DefaultHost    = "0.0.0.0"
    DefaultPort    = 8080
    DefaultLogFile = "/var/log/sysmon.log"
)

const (
    KB = 1024
    MB = 1024 * KB
    GB = 1024 * MB
    TB = 1024 * GB
    PB = 1024 * TB
)
```

### Constantes con expresiones

Las constantes pueden usar expresiones, pero **solo con otras constantes y operaciones evaluables en compilación**:

```go
const (
    SecondsPerMinute = 60
    SecondsPerHour   = 60 * SecondsPerMinute    // 3600
    SecondsPerDay    = 24 * SecondsPerHour       // 86400
    SecondsPerWeek   = 7 * SecondsPerDay         // 604800
)

const (
    MaxPathLen  = 4096
    MaxNameLen  = 255
    MaxSymlinks = 40
)

// ✗ NO compila — las funciones no son evaluables en compilación
// const Now = time.Now()         // not a constant
// const Home = os.Getenv("HOME") // not a constant
// const Size = len(someSlice)    // someSlice es variable

// ✓ len() SÍ es constante con strings y arrays literales
const Greeting = "Hello"
const GreetLen = len(Greeting)   // 5 — len de string literal es constante
```

---

## 3. Tipos permitidos en constantes

Go restringe las constantes a **tipos básicos** que el compilador puede evaluar:

```
┌──────────────────────────────────────────────────────────────┐
│           TIPOS PERMITIDOS EN CONSTANTES                     │
├─────────────────────┬────────────────────────────────────────┤
│  ✓ Permitido        │  ✗ No permitido                        │
├─────────────────────┼────────────────────────────────────────┤
│  bool               │  slice ([]int, []string)               │
│  int, int8..int64   │  map                                   │
│  uint, uint8..64    │  struct                                │
│  float32, float64   │  array ([3]int)                        │
│  complex64, 128     │  pointer (*int)                        │
│  string             │  function                              │
│  rune (int32)       │  interface                             │
│  byte (uint8)       │  channel                               │
│  Tipos definidos    │  time.Duration (es int64, pero         │
│  basados en los     │    las expresiones con time.Second     │
│  anteriores         │    se resuelven como constantes)       │
└─────────────────────┴────────────────────────────────────────┘
```

```go
// Tipos definidos basados en tipos básicos SÍ funcionan
type Severity int

const (
    SevInfo    Severity = 0
    SevWarning Severity = 1
    SevError   Severity = 2
    SevFatal   Severity = 3
)

// time.Duration es int64 — funciona con literales numéricos
const (
    PollInterval = 5 * time.Second      // ✓ time.Second es constante
    MaxWait      = 30 * time.Minute     // ✓ time.Minute es constante
    Expiry       = 24 * time.Hour       // ✓
)

// ✗ Pero no con funciones de time
// const StartTime = time.Now()  // time.Now() no es constante
```

---

## 4. Constantes sin tipo (Untyped Constants) — Concepto clave

Esta es una de las características más poderosas y únicas de Go. Una constante sin tipo no tiene un tipo fijo — se comporta como un "ideal" que se adapta al contexto:

```go
// Constante sin tipo — se adapta al contexto
const Port = 8080

var a int = Port        // OK: Port se convierte a int
var b int64 = Port      // OK: Port se convierte a int64
var c uint16 = Port     // OK: Port se convierte a uint16
var d float64 = Port    // OK: Port se convierte a float64

// Constante CON tipo — fija el tipo
const TypedPort int = 8080

var e int = TypedPort       // OK: mismo tipo
// var f int64 = TypedPort  // ✗ cannot use TypedPort (constant 8080 of type int) as int64
var f int64 = int64(TypedPort) // ✓ conversión explícita necesaria
```

### Tipos por defecto (default types)

Cuando una constante sin tipo necesita un tipo concreto y no hay contexto que lo determine, Go usa estos defaults:

```
┌───────────────────────────────────────────┐
│   Literal           │  Tipo por defecto   │
├─────────────────────┼─────────────────────┤
│   42                │  int                │
│   3.14              │  float64            │
│   true/false        │  bool               │
│   "hello"           │  string             │
│   'A'               │  rune (int32)       │
│   1+2i              │  complex128         │
└─────────────────────┴─────────────────────┘
```

```go
const X = 42     // untyped int, default type: int
const Y = 3.14   // untyped float, default type: float64
const Z = "hi"   // untyped string, default type: string

// El default type se usa cuando Go necesita decidir:
fmt.Printf("%T\n", X)   // int
fmt.Printf("%T\n", Y)   // float64

// Pero en contexto tipado, se adapta:
var a float64 = X   // 42 se convierte a float64 sin cast
var b int32 = X     // 42 se convierte a int32 sin cast
```

### Precisión arbitraria

Las constantes numéricas sin tipo tienen **precisión ilimitada** en compilación:

```go
// Esto compila — el compilador maneja precisión arbitraria
const (
    Huge   = 1 << 100                          // mucho mayor que uint64 max
    Medium = Huge >> 98                         // 4
    Small  = Huge >> 99                         // 2
)

// Huge no cabe en ningún tipo de Go...
// var x = Huge  // ✗ constant 1267650600228229401496703205376 overflows int

// ...pero las operaciones con él sí producen valores usables
var x = Medium   // 4, cabe en int
var y = Small    // 2, cabe en int

// Precisión de punto flotante en constantes
const (
    Third = 1.0 / 3.0  // precisión completa en compilación
    // Como variable sería: 0.3333333333333333 (float64, 15 dígitos)
    // Como constante mantiene precisión arbitraria hasta asignación
)
```

### Por qué importa para SysAdmin

```go
// Sin constantes sin tipo, necesitarías casts por todas partes:
const MaxFileSize = 100 * 1024 * 1024  // 100 MB, untyped

// Se puede usar directamente con int64 (tamaños de archivo)
var fileSize int64 = MaxFileSize  // OK, sin cast

// Y con int (índices, contadores)
if len(data) > MaxFileSize {  // OK, len retorna int
    log.Fatal("file too large")
}

// Con typed constant:
const TypedMax int64 = 100 * 1024 * 1024
// if len(data) > TypedMax {  // ✗ mismatched types int and int64
// Necesitarías: if int64(len(data)) > TypedMax {
```

---

## 5. `iota` — El enumerador de Go

`iota` es un generador de constantes que **empieza en 0** y se incrementa en 1 por cada línea dentro de un bloque `const`:

```go
const (
    A = iota  // 0
    B = iota  // 1
    C = iota  // 2
)

// Forma abreviada — la expresión se repite implícitamente
const (
    A = iota  // 0
    B         // 1  (iota se repite implícitamente)
    C         // 2
    D         // 3
)
```

### Reglas de `iota`

```
┌──────────────────────────────────────────────────────────────┐
│                  REGLAS DE IOTA                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. iota = 0 al inicio de cada bloque const                  │
│  2. Se incrementa en 1 por cada LÍNEA (no por cada const)   │
│  3. Si omites la expresión, se repite la anterior            │
│  4. Funciona con cualquier expresión aritmética              │
│  5. Cada bloque const reinicia iota a 0                      │
│  6. El blank identifier _ consume un valor de iota           │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

```go
// iota se reinicia en cada bloque const
const (
    X = iota  // 0
    Y         // 1
)
const (
    A = iota  // 0  ← reiniciado
    B         // 1
)
```

---

## 6. Patrones con `iota` — Enumeraciones

### Enumeración básica — Severidad de log

```go
type LogLevel int

const (
    LevelDebug LogLevel = iota  // 0
    LevelInfo                   // 1
    LevelWarning                // 2
    LevelError                  // 3
    LevelFatal                  // 4
)

// String() para imprimir nombres legibles
func (l LogLevel) String() string {
    switch l {
    case LevelDebug:
        return "DEBUG"
    case LevelInfo:
        return "INFO"
    case LevelWarning:
        return "WARNING"
    case LevelError:
        return "ERROR"
    case LevelFatal:
        return "FATAL"
    default:
        return fmt.Sprintf("UNKNOWN(%d)", l)
    }
}

// Uso
var minLevel = LevelWarning
if currentLevel >= minLevel {
    log.Println(msg)
}
```

### Skipear el zero value con `_`

```go
// El zero value de un int es 0. Si una variable de tipo Status
// no se inicializa, sería StatusPending. ¿Es eso intencionado?

// Patrón: usar _ para que el 0 no sea un estado válido
type Status int

const (
    _             Status = iota  // 0 — descartado, ningún Status válido es 0
    StatusRunning                // 1
    StatusStopped                // 2
    StatusFailed                 // 3
)

// Ahora var s Status → s == 0 → no coincide con ningún Status válido
// Esto te permite detectar "no inicializado" vs "valor real"
```

### Empezar desde 1 (sin descartar)

```go
const (
    StatusRunning = iota + 1  // 1
    StatusStopped             // 2
    StatusFailed              // 3
)
```

---

## 7. Patrones avanzados con `iota` — Bit flags

### Permisos tipo Unix con bitmask

```go
type Permission uint32

const (
    PermExecute Permission = 1 << iota  // 1   (001)
    PermWrite                           // 2   (010)
    PermRead                            // 4   (100)
)

// Combinaciones predefinidas
const (
    PermNone Permission = 0                              // ---
    PermRW              = PermRead | PermWrite           // rw- (6)
    PermRX              = PermRead | PermExecute         // r-x (5)
    PermRWX             = PermRead | PermWrite | PermExecute // rwx (7)
)

// Operaciones con bit flags
func (p Permission) Has(flag Permission) bool {
    return p&flag != 0
}

func (p Permission) Set(flag Permission) Permission {
    return p | flag
}

func (p Permission) Clear(flag Permission) Permission {
    return p &^ flag  // AND NOT
}

func (p Permission) Toggle(flag Permission) Permission {
    return p ^ flag
}

func (p Permission) String() string {
    var buf [3]byte
    if p.Has(PermRead) {
        buf[0] = 'r'
    } else {
        buf[0] = '-'
    }
    if p.Has(PermWrite) {
        buf[1] = 'w'
    } else {
        buf[1] = '-'
    }
    if p.Has(PermExecute) {
        buf[2] = 'x'
    } else {
        buf[2] = '-'
    }
    return string(buf[:])
}

// Uso
perm := PermRead | PermWrite  // rw-
fmt.Println(perm)              // "rw-"
fmt.Println(perm.Has(PermExecute))  // false
perm = perm.Set(PermExecute)
fmt.Println(perm)              // "rwx"
```

### Feature flags para configuración de servicios

```go
type Feature uint64

const (
    FeatureMetrics    Feature = 1 << iota  // 1
    FeatureTracing                         // 2
    FeatureRateLimit                       // 4
    FeatureAuth                            // 8
    FeatureTLS                             // 16
    FeatureCompression                     // 32
    FeatureHTTP2                           // 64
    FeatureGraceful                        // 128
    FeatureHealthCheck                     // 256
    FeaturePprof                           // 512
    FeatureCORS                            // 1024
)

// Presets de configuración
const (
    FeaturesMinimal    = FeatureHealthCheck | FeatureGraceful
    FeaturesDev        = FeaturesMinimal | FeatureMetrics | FeaturePprof
    FeaturesStaging    = FeaturesDev | FeatureTracing | FeatureAuth | FeatureRateLimit
    FeaturesProduction = FeaturesStaging | FeatureTLS | FeatureCompression | FeatureHTTP2 | FeatureCORS
)

func (f Feature) Has(flag Feature) bool {
    return f&flag != 0
}

func (f Feature) String() string {
    names := []struct {
        flag Feature
        name string
    }{
        {FeatureMetrics, "metrics"},
        {FeatureTracing, "tracing"},
        {FeatureRateLimit, "rate-limit"},
        {FeatureAuth, "auth"},
        {FeatureTLS, "tls"},
        {FeatureCompression, "compression"},
        {FeatureHTTP2, "http2"},
        {FeatureGraceful, "graceful"},
        {FeatureHealthCheck, "health"},
        {FeaturePprof, "pprof"},
        {FeatureCORS, "cors"},
    }

    var enabled []string
    for _, n := range names {
        if f.Has(n.flag) {
            enabled = append(enabled, n.name)
        }
    }
    return fmt.Sprintf("[%s]", strings.Join(enabled, ", "))
}

// Uso
features := FeaturesProduction
if features.Has(FeatureTLS) {
    // configurar TLS
}
fmt.Println(features)
// [metrics, tracing, rate-limit, auth, tls, compression, http2, graceful, health, pprof, cors]
```

---

## 8. Patrones con `iota` — Unidades y potencias

### Tamaños de almacenamiento (patrón de la stdlib)

```go
// Bytes — usando shift (potencias de 2)
const (
    _  = iota             // ignora 0
    KB = 1 << (10 * iota) // 1 << 10 = 1024
    MB                    // 1 << 20 = 1048576
    GB                    // 1 << 30 = 1073741824
    TB                    // 1 << 40 = 1099511627776
    PB                    // 1 << 50
    EB                    // 1 << 60
)

// Uso en validaciones SysAdmin
const MaxUploadSize = 100 * MB  // 104857600 bytes

func checkDiskSpace(path string) error {
    // ... obtener espacio libre
    if freeBytes < 10*GB {
        return fmt.Errorf("low disk space on %s: %d bytes free", path, freeBytes)
    }
    return nil
}

// Formatear bytes legibles
func formatBytes(b int64) string {
    switch {
    case b >= EB:
        return fmt.Sprintf("%.2f EB", float64(b)/float64(EB))
    case b >= PB:
        return fmt.Sprintf("%.2f PB", float64(b)/float64(PB))
    case b >= TB:
        return fmt.Sprintf("%.2f TB", float64(b)/float64(TB))
    case b >= GB:
        return fmt.Sprintf("%.2f GB", float64(b)/float64(GB))
    case b >= MB:
        return fmt.Sprintf("%.2f MB", float64(b)/float64(MB))
    case b >= KB:
        return fmt.Sprintf("%.2f KB", float64(b)/float64(KB))
    default:
        return fmt.Sprintf("%d B", b)
    }
}
```

### Multiplicadores de tiempo

```go
const (
    Second = 1
    Minute = 60 * Second
    Hour   = 60 * Minute
    Day    = 24 * Hour
    Week   = 7 * Day
)

// Para timeouts en segundos (cuando no usas time.Duration)
const (
    APITimeout    = 30 * Second
    DBTimeout     = 5 * Second
    CacheExpiry   = 15 * Minute
    SessionExpiry = 24 * Hour
    TokenExpiry   = 7 * Day
)
```

### Códigos de estado HTTP como constantes

```go
// La stdlib ya define estos en net/http, pero como ejemplo:
const (
    StatusOK                  = 200
    StatusCreated             = 201
    StatusNoContent           = 204
    StatusBadRequest          = 400
    StatusUnauthorized        = 401
    StatusForbidden           = 403
    StatusNotFound            = 404
    StatusMethodNotAllowed    = 405
    StatusTooManyRequests     = 429
    StatusInternalServerError = 500
    StatusBadGateway          = 502
    StatusServiceUnavailable  = 503
    StatusGatewayTimeout      = 504
)
```

---

## 9. `iota` — Expresiones complejas

`iota` se puede usar en cualquier expresión aritmética. La expresión de la primera línea se repite implícitamente:

### Puertos con offset

```go
const BasePort = 9000

const (
    PortHTTP    = BasePort + iota  // 9000
    PortGRPC                       // 9001
    PortMetrics                    // 9002
    PortHealth                     // 9003
    PortPprof                      // 9004
    PortAdmin                      // 9005
)
```

### Múltiples iotas en la misma línea

```go
const (
    // iota se incrementa por LÍNEA, no por uso
    A, B = iota, iota * 10    // A=0, B=0
    C, D                       // C=1, D=10
    E, F                       // E=2, F=20
)
```

### Rangos de valores

```go
type Priority int

const (
    PriorityLow    Priority = iota * 10  // 0
    PriorityMedium                       // 10
    PriorityHigh                         // 20
    PriorityCritical                     // 30
)
```

### Valores con huecos intencionados

```go
const (
    LevelTrace = iota - 2  // -2
    LevelDebug             // -1
    LevelInfo              // 0  ← zero value = Info (buen default)
    LevelWarning           // 1
    LevelError             // 2
    LevelFatal             // 3
)
```

---

## 10. Constantes de la biblioteca estándar — Referencia SysAdmin

La stdlib de Go usa constantes extensivamente. Conocer las más importantes para SysAdmin:

### `os` — Flags de archivos y permisos

```go
import "os"

// Flags para os.OpenFile
const (
    os.O_RDONLY = 0         // abrir solo lectura
    os.O_WRONLY = 1         // abrir solo escritura
    os.O_RDWR   = 2         // abrir lectura-escritura
    os.O_APPEND = 1024      // append al escribir
    os.O_CREATE = 64        // crear si no existe
    os.O_EXCL   = 128       // error si ya existe (con O_CREATE)
    os.O_SYNC   = 1052672   // I/O síncrono
    os.O_TRUNC  = 512       // truncar al abrir
)

// Permisos (os.FileMode)
const (
    os.ModeDir        // directorio
    os.ModeAppend     // append-only
    os.ModeExclusive  // exclusive use
    os.ModeSymlink    // symbolic link
    os.ModeNamedPipe  // named pipe (FIFO)
    os.ModeSocket     // Unix socket
    os.ModeSetuid     // setuid
    os.ModeSetgid     // setgid
    os.ModeSticky     // sticky bit
)

// Ejemplo SysAdmin: crear archivo de log con permisos correctos
file, err := os.OpenFile(
    "/var/log/app.log",
    os.O_APPEND|os.O_CREATE|os.O_WRONLY,
    0644,  // rw-r--r--
)
```

### `time` — Duraciones y formatos

```go
import "time"

// Duraciones (type Duration int64, en nanosegundos)
const (
    time.Nanosecond  = 1
    time.Microsecond = 1000 * Nanosecond
    time.Millisecond = 1000 * Microsecond
    time.Second      = 1000 * Millisecond
    time.Minute      = 60 * Second
    time.Hour        = 60 * Minute
)

// Layouts de formato (la fecha de referencia de Go: Mon Jan 2 15:04:05 MST 2006)
const (
    time.RFC3339     = "2006-01-02T15:04:05Z07:00"  // ISO 8601
    time.RFC3339Nano = "2006-01-02T15:04:05.999999999Z07:00"
    time.RFC1123     = "Mon, 02 Jan 2006 15:04:05 MST"
    time.Kitchen     = "3:04PM"
    time.Stamp       = "Jan _2 15:04:05"             // syslog format
    time.DateTime    = "2006-01-02 15:04:05"         // Go 1.20+
    time.DateOnly    = "2006-01-02"                  // Go 1.20+
    time.TimeOnly    = "15:04:05"                    // Go 1.20+
)

// Uso en logs SysAdmin
timestamp := time.Now().Format(time.RFC3339)
// "2024-01-15T14:30:45-05:00"

syslogTime := time.Now().Format(time.Stamp)
// "Jan 15 14:30:45"
```

### `math` — Límites numéricos

```go
import "math"

const (
    math.MaxInt8   = 127
    math.MinInt8   = -128
    math.MaxInt16  = 32767
    math.MaxInt32  = 2147483647
    math.MaxInt64  = 9223372036854775807
    math.MaxUint8  = 255
    math.MaxUint16 = 65535
    math.MaxUint32 = 4294967295

    math.MaxFloat32 = 3.40282346638528859811704183484516925440e+38
    math.MaxFloat64 = 1.797693134862315708145274237317043567981e+308
)

// Uso: validar que un valor cabe en el tipo destino
func safePortConvert(port int) (uint16, error) {
    if port < 0 || port > math.MaxUint16 {
        return 0, fmt.Errorf("port %d out of range (0-%d)", port, math.MaxUint16)
    }
    return uint16(port), nil
}
```

### `net/http` — Códigos de estado y métodos

```go
import "net/http"

// Métodos
const (
    http.MethodGet     = "GET"
    http.MethodPost    = "POST"
    http.MethodPut     = "PUT"
    http.MethodPatch   = "PATCH"
    http.MethodDelete  = "DELETE"
    http.MethodHead    = "HEAD"
    http.MethodOptions = "OPTIONS"
)

// Status codes (los más usados en APIs)
const (
    http.StatusOK                  = 200
    http.StatusCreated             = 201
    http.StatusNoContent           = 204
    http.StatusBadRequest          = 400
    http.StatusUnauthorized        = 401
    http.StatusForbidden           = 403
    http.StatusNotFound            = 404
    http.StatusConflict            = 409
    http.StatusTooManyRequests     = 429
    http.StatusInternalServerError = 500
    http.StatusBadGateway          = 502
    http.StatusServiceUnavailable  = 503
    http.StatusGatewayTimeout      = 504
)
```

---

## 11. Constantes exportadas vs no exportadas

Las mismas reglas de visibilidad de Go aplican a constantes:

```go
package config

// Exportadas — accesibles desde otros paquetes (empieza con mayúscula)
const (
    DefaultPort    = 8080
    DefaultHost    = "0.0.0.0"
    MaxConnections = 10000
    Version        = "2.1.0"
)

// No exportadas — solo dentro de este paquete (empieza con minúscula)
const (
    internalBufferSize = 4096
    maxRetries         = 3
    defaultLogLevel    = "info"
)

// Desde otro paquete:
// config.DefaultPort   ✓
// config.maxRetries    ✗ unexported
```

### Convención de nombres

```go
// ✓ Go style — PascalCase o camelCase, NO SCREAMING_CASE
const MaxRetries = 3
const defaultTimeout = 30

// ✗ No es idiomático en Go (aunque compila)
const MAX_RETRIES = 3       // estilo C/Python, no Go
const DEFAULT_TIMEOUT = 30  // SCREAMING_SNAKE_CASE no es Go

// Excepción: la stdlib usa SCREAMING_CASE solo en muy pocos casos históricos
// como syscall.SIGTERM, que refleja nombres del sistema operativo
```

---

## 12. Constantes vs variables — Cuándo usar cada una

```
┌──────────────────────────────────────────────────────────────┐
│         CONSTANTE vs VARIABLE — DECISIÓN                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  ¿El valor puede cambiar en runtime?                         │
│    SÍ  → var (o parámetro de función)                        │
│    NO  → const                                               │
│                                                              │
│  ¿El valor viene de env/config/flag/archivo?                 │
│    SÍ  → var (leído en init() o main())                      │
│    NO  → const (si es literal conocido en compilación)       │
│                                                              │
│  ¿El tipo es slice/map/struct/función?                       │
│    SÍ  → var (const no soporta estos tipos)                  │
│    NO  → const (si es fijo)                                  │
│                                                              │
│  ¿Necesitas la dirección (&)?                                │
│    SÍ  → var (las constantes no tienen dirección)            │
│    NO  → const (si aplica)                                   │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

```go
// ✓ Constante — valores fijos, conocidos en compilación
const (
    MaxRetries     = 3
    DefaultPort    = 8080
    AppName        = "sysmon"
    SocketPath     = "/var/run/sysmon.sock"
    LogFormat      = "2006-01-02 15:04:05"
)

// ✓ Variable — valores que vienen del entorno
var (
    port      = getEnvInt("PORT", DefaultPort)
    logLevel  = getEnvStr("LOG_LEVEL", "info")
    dbURL     = os.Getenv("DATABASE_URL")
)

// ✓ Variable — tipos no soportados por const
var (
    allowedOrigins = []string{"https://app.example.com", "https://admin.example.com"}
    defaultHeaders = map[string]string{
        "X-Frame-Options":  "DENY",
        "X-Content-Type":   "nosniff",
    }
)
```

### Ventajas de `const` sobre `var` para valores fijos

```go
// 1. Seguridad: no se puede modificar accidentalmente
const MaxConns = 100
// MaxConns = 200  // ✗ cannot assign to MaxConns

var maxConns = 100
// maxConns = 200  // ✓ compila... ¿fue intencional o un bug?

// 2. Performance: el compilador sustituye directamente el valor (inline)
// No hay lectura de memoria, no hay indirección

// 3. Concurrencia: las constantes son inherentemente thread-safe
// No necesitan mutex ni atomic

// 4. No tienen dirección (no se pueden pasar por puntero accidentalmente)
// const X = 42
// &X  // ✗ cannot take address of X
```

---

## 13. Patrón: Constantes como sentinelas

Las constantes string son útiles como **valores sentinela** (sentinel values) — valores especiales que indican una condición:

```go
// Patrón sentinela para estados de servicio
const (
    StateStarting   = "starting"
    StateRunning    = "running"
    StateDegraded   = "degraded"
    StateStopping   = "stopping"
    StateStopped    = "stopped"
    StateFailed     = "failed"
    StateUnknown    = "unknown"
)

func checkServiceState(name string) string {
    // ... lógica de verificación
    if !isResponding {
        return StateFailed
    }
    if highLatency {
        return StateDegraded
    }
    return StateRunning
}

// Comparar con constante es más seguro que strings hardcodeados
state := checkServiceState("nginx")
if state == StateFailed || state == StateDegraded {
    alertOncall(name, state)
}
// vs
// if state == "faild" {  ← typo no detectado por el compilador
```

### Mejor aún: usar un tipo definido

```go
type ServiceState string

const (
    StateStarting ServiceState = "starting"
    StateRunning  ServiceState = "running"
    StateDegraded ServiceState = "degraded"
    StateStopping ServiceState = "stopping"
    StateStopped  ServiceState = "stopped"
    StateFailed   ServiceState = "failed"
)

// Ahora el compilador previene comparaciones con strings arbitrarios
func handleState(s ServiceState) {
    switch s {
    case StateRunning:
        // ...
    case StateFailed:
        // ...
    }
}

// handleState("typo")  // ✗ cannot use "typo" (untyped string) as ServiceState
// Necesitarías: handleState(ServiceState("typo")) — intención explícita
```

---

## 14. Patrón: Enumeración exhaustiva con método `String()`

Un patrón muy usado en herramientas de SysAdmin para que los valores sean legibles:

### Generación manual

```go
type ExitCode int

const (
    ExitOK          ExitCode = 0
    ExitWarning     ExitCode = 1
    ExitCritical    ExitCode = 2
    ExitUnknown     ExitCode = 3
)

func (e ExitCode) String() string {
    switch e {
    case ExitOK:
        return "OK"
    case ExitWarning:
        return "WARNING"
    case ExitCritical:
        return "CRITICAL"
    case ExitUnknown:
        return "UNKNOWN"
    default:
        return fmt.Sprintf("ExitCode(%d)", e)
    }
}

// Compatible con Nagios/Icinga monitoring
func main() {
    code := checkDisk("/")
    fmt.Printf("%s - Disk check %s\n", code, getMessage(code))
    os.Exit(int(code))
}
```

### Generación con `go generate` y `stringer`

```go
//go:generate stringer -type=LogLevel
type LogLevel int

const (
    LevelDebug LogLevel = iota
    LevelInfo
    LevelWarning
    LevelError
    LevelFatal
)
```

```bash
# Instalar stringer
go install golang.org/x/tools/cmd/stringer@latest

# Generar el método String()
go generate ./...

# Crea loglevel_string.go con:
# func (i LogLevel) String() string { ... }
# Usa un array lookup, no switch — más eficiente
```

Contenido generado (automático):

```go
// Code generated by "stringer -type=LogLevel"; DO NOT EDIT.
package main

import "strconv"

func _() {
    // Verify that enum values map to string indices
    var x [1]struct{}
    _ = x[LevelDebug-0]
    _ = x[LevelInfo-1]
    _ = x[LevelWarning-2]
    _ = x[LevelError-3]
    _ = x[LevelFatal-4]
}

const _LogLevel_name = "LevelDebugLevelInfoLevelWarningLevelErrorLevelFatal"

var _LogLevel_index = [...]uint8{0, 10, 19, 31, 41, 51}

func (i LogLevel) String() string {
    if i < 0 || i >= LogLevel(len(_LogLevel_index)-1) {
        return "LogLevel(" + strconv.FormatInt(int64(i), 10) + ")"
    }
    return _LogLevel_name[_LogLevel_index[i]:_LogLevel_index[i+1]]
}
```

---

## 15. Patrón: Constantes para configuración de compilación

Combinando constantes con build tags (T03 de C01):

```go
// config_dev.go
//go:build dev

package config

const (
    Environment = "development"
    LogLevel    = "debug"
    APIBaseURL  = "http://localhost:8080"
    EnablePprof = true
)

// config_staging.go
//go:build staging

package config

const (
    Environment = "staging"
    LogLevel    = "info"
    APIBaseURL  = "https://staging-api.example.com"
    EnablePprof = true
)

// config_production.go
//go:build production

package config

const (
    Environment = "production"
    LogLevel    = "warning"
    APIBaseURL  = "https://api.example.com"
    EnablePprof = false
)
```

```bash
# Compilar para cada entorno
go build -tags dev -o app-dev
go build -tags staging -o app-staging
go build -tags production -o app-prod

# El binario de producción tiene EnablePprof = false
# El compilador puede eliminar código muerto (dead code elimination)
```

```go
// main.go — el compilador elimina el bloque completo en producción
func setupServer(mux *http.ServeMux) {
    // ...

    if config.EnablePprof {  // constante false en producción → dead code
        mux.HandleFunc("/debug/pprof/", pprof.Index)
        mux.HandleFunc("/debug/pprof/cmdline", pprof.Cmdline)
        mux.HandleFunc("/debug/pprof/profile", pprof.Profile)
        // El compilador elimina todo este bloque en producción
    }
}
```

---

## 16. Ejemplo SysAdmin: Sistema de alertas con constantes

```go
package main

import (
    "fmt"
    "os"
    "strings"
    "time"
)

// ─── Constantes de la aplicación ─────────────────────────

const (
    AppName    = "alertmanager"
    AppVersion = "1.3.0"
)

// ─── Severidades con iota ────────────────────────────────

type Severity int

const (
    SevInfo     Severity = iota // 0
    SevWarning                  // 1
    SevCritical                 // 2
    SevFatal                    // 3
)

func (s Severity) String() string {
    names := [...]string{"INFO", "WARNING", "CRITICAL", "FATAL"}
    if int(s) < len(names) {
        return names[s]
    }
    return fmt.Sprintf("UNKNOWN(%d)", s)
}

func (s Severity) Color() string {
    colors := [...]string{"\033[32m", "\033[33m", "\033[31m", "\033[35m"}
    if int(s) < len(colors) {
        return colors[s]
    }
    return "\033[0m"
}

// ─── Canales de notificación con bit flags ───────────────

type Channel uint16

const (
    ChanLog     Channel = 1 << iota // 1 — siempre loguear
    ChanEmail                       // 2
    ChanSlack                       // 4
    ChanPager                       // 8
    ChanWebhook                     // 16
    ChanSMS                         // 32
)

func (c Channel) Has(flag Channel) bool {
    return c&flag != 0
}

func (c Channel) String() string {
    var parts []string
    flags := []struct {
        f Channel
        n string
    }{
        {ChanLog, "log"},
        {ChanEmail, "email"},
        {ChanSlack, "slack"},
        {ChanPager, "pager"},
        {ChanWebhook, "webhook"},
        {ChanSMS, "sms"},
    }
    for _, fl := range flags {
        if c.Has(fl.f) {
            parts = append(parts, fl.n)
        }
    }
    return strings.Join(parts, "+")
}

// ─── Routing: severidad → canales ────────────────────────

// Mapeo de severidad a canales (política de alerting)
var routingTable = map[Severity]Channel{
    SevInfo:     ChanLog,
    SevWarning:  ChanLog | ChanSlack,
    SevCritical: ChanLog | ChanSlack | ChanEmail | ChanWebhook,
    SevFatal:    ChanLog | ChanSlack | ChanEmail | ChanPager | ChanSMS,
}

// ─── Unidades de tamaño ──────────────────────────────────

const (
    _  = iota
    KB = 1 << (10 * iota)
    MB
    GB
    TB
)

// ─── Umbrales de monitoreo ───────────────────────────────

const (
    DiskWarningPercent  = 80
    DiskCriticalPercent = 95
    MemWarningPercent   = 85
    MemCriticalPercent  = 95
    CPUWarningPercent   = 80
    CPUCriticalPercent  = 95
    LoadWarningFactor   = 2.0  // load > 2x CPUs
    LoadCriticalFactor  = 4.0  // load > 4x CPUs
)

const (
    CheckInterval  = 30 * time.Second
    AlertCooldown  = 5 * time.Minute   // no repetir alerta en este periodo
    EscalateAfter  = 15 * time.Minute  // escalar si no se resuelve
)

// ─── Exit codes compatibles con Nagios ───────────────────

type ExitCode int

const (
    ExitOK       ExitCode = 0
    ExitWarning  ExitCode = 1
    ExitCritical ExitCode = 2
    ExitUnknown  ExitCode = 3
)

// ─── Tipos auxiliares ────────────────────────────────────

type Alert struct {
    Timestamp time.Time
    Severity  Severity
    Source    string
    Message   string
    Metric    string
    Value     float64
    Threshold float64
}

func (a Alert) Format() string {
    return fmt.Sprintf("[%s] %s %s: %s (value=%.1f, threshold=%.1f)",
        a.Timestamp.Format(time.DateTime),
        a.Severity,
        a.Source,
        a.Message,
        a.Value,
        a.Threshold,
    )
}

// ─── Lógica principal ────────────────────────────────────

func dispatch(alert Alert) {
    channels := routingTable[alert.Severity]

    fmt.Printf("%s%s\033[0m → routing to: %s\n",
        alert.Severity.Color(),
        alert.Format(),
        channels,
    )

    if channels.Has(ChanLog) {
        fmt.Fprintf(os.Stderr, "%s\n", alert.Format())
    }
    if channels.Has(ChanSlack) {
        fmt.Printf("  → Slack: #alerts — %s\n", alert.Message)
    }
    if channels.Has(ChanEmail) {
        fmt.Printf("  → Email: oncall@example.com — %s\n", alert.Message)
    }
    if channels.Has(ChanPager) {
        fmt.Printf("  → Pager: URGENT — %s\n", alert.Message)
    }
}

func evaluateDisk(usedPercent float64, mount string) *Alert {
    now := time.Now()

    switch {
    case usedPercent >= DiskCriticalPercent:
        return &Alert{
            Timestamp: now,
            Severity:  SevCritical,
            Source:    "disk",
            Message:   fmt.Sprintf("Disk usage critical on %s", mount),
            Metric:    "disk_used_percent",
            Value:     usedPercent,
            Threshold: DiskCriticalPercent,
        }
    case usedPercent >= DiskWarningPercent:
        return &Alert{
            Timestamp: now,
            Severity:  SevWarning,
            Source:    "disk",
            Message:   fmt.Sprintf("Disk usage high on %s", mount),
            Metric:    "disk_used_percent",
            Value:     usedPercent,
            Threshold: DiskWarningPercent,
        }
    default:
        return nil
    }
}

func main() {
    fmt.Printf("%s v%s — Alert Routing Demo\n\n", AppName, AppVersion)

    // Simular checks
    scenarios := []struct {
        mount   string
        percent float64
    }{
        {"/", 45.2},
        {"/var", 82.5},
        {"/data", 97.1},
    }

    for _, s := range scenarios {
        if alert := evaluateDisk(s.percent, s.mount); alert != nil {
            dispatch(*alert)
        } else {
            fmt.Printf("\033[32m[OK]\033[0m %s: %.1f%% used\n", s.mount, s.percent)
        }
        fmt.Println()
    }
}
```

---

## 17. Comparación: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Declaración | `const X = 42` | `#define X 42` / `const int x = 42` | `const X: i32 = 42` |
| Evaluación | Compilación | Preprocesador / compilación | Compilación |
| Tipos | Solo básicos (bool, num, string) | Cualquiera con `const` | Cualquiera con `const` |
| Enumeraciones | `iota` en bloque `const` | `enum { A, B, C }` | `enum E { A, B, C }` |
| Precisión | Arbitraria (untyped) | Fija al tipo | Fija al tipo |
| Sin tipo | Sí (untyped constants) | No | No |
| Bit flags | `1 << iota` manual | `1 << 0`, `1 << 1`... manual | `bitflags!` crate |
| String enum | `type T string; const` | No nativo (char*) | `enum` con `#[derive]` |
| Generación | `go generate stringer` | No estándar | `#[derive(Debug)]` automático |
| Scope | Paquete (exportable) | Archivo / translation unit | Módulo (pub/priv) |
| Expresiones | Aritmética constante | Aritmética + sizeof | Aritmética + funciones const |
| `const fn` | No existe | No | Sí (funciones evaluadas en compilación) |

---

## 18. Errores comunes

| # | Error | Código | Solución |
|---|---|---|---|
| 1 | Tipo no constante | `const t = time.Now()` | Usar `var t = time.Now()` |
| 2 | Slice/map como const | `const xs = []int{1,2}` | Usar `var xs = []int{1,2}` |
| 3 | Tomar dirección de const | `p := &MaxRetries` | `tmp := MaxRetries; p := &tmp` |
| 4 | iota fuera de bloque const | `iota` solo existe en `const()` | Ponerlo dentro de un bloque `const()` |
| 5 | Olvidar que iota se reinicia | Segundo `const()` empieza en 0 | Cada bloque `const()` reinicia iota |
| 6 | SCREAMING_CASE | `const MAX_PORT = 65535` | `const MaxPort = 65535` (Go style) |
| 7 | Typed const innecesario | `const Port int = 8080` | `const Port = 8080` (untyped más flexible) |
| 8 | Overflow silencioso | `const X uint8 = 256` | Error de compilación (typed overflow) |
| 9 | iota con huecos no deseados | Línea en blanco consume iota | Líneas en blanco NO consumen iota — solo `_` o constantes declaradas |
| 10 | Comparar tipos distintos | `SevWarning == 1` funciona untyped | Con typed: `SevWarning == Severity(1)` |
| 11 | Mutar "constante" | `const → var` cuando se necesita cambiar | Decidir desde el inicio si es mutable |
| 12 | Enum sin String() | Imprimir LogLevel da "2" | Implementar `String()` o usar `stringer` |

---

## 19. Ejercicios

### Ejercicio 1: Predicción de iota básico
```go
package main

import "fmt"

const (
    A = iota
    B
    C
    D
)

const (
    X = iota
    Y
)

func main() {
    fmt.Println(A, B, C, D)
    fmt.Println(X, Y)
}
```
**Predicción**: ¿Qué imprime? ¿Por qué X no es 4?

### Ejercicio 2: Predicción de iota con expresión
```go
package main

import "fmt"

const (
    _  = iota
    KB = 1 << (10 * iota)
    MB
    GB
    TB
)

func main() {
    fmt.Println("KB =", KB)
    fmt.Println("MB =", MB)
    fmt.Println("GB =", GB)
    fmt.Println("TB =", TB)
}
```
**Predicción**: ¿Qué valores tienen KB, MB, GB, TB? Calcula `1 << (10 * iota)` para cada línea.

### Ejercicio 3: Predicción de bit flags
```go
package main

import "fmt"

const (
    FlagRead    = 1 << iota  // ?
    FlagWrite                // ?
    FlagExecute              // ?
)

func main() {
    perm := FlagRead | FlagExecute
    fmt.Printf("perm = %d (%03b)\n", perm, perm)
    fmt.Println("has read:", perm&FlagRead != 0)
    fmt.Println("has write:", perm&FlagWrite != 0)
    fmt.Println("has exec:", perm&FlagExecute != 0)
}
```
**Predicción**: ¿Qué valor decimal y binario tiene `perm`? ¿Qué imprimen los tres `has`?

### Ejercicio 4: Typed vs untyped
```go
package main

import "fmt"

const UntypedPort = 8080
const TypedPort int = 8080

func main() {
    var a int = UntypedPort
    var b int64 = UntypedPort
    var c uint16 = UntypedPort

    var d int = TypedPort
    // var e int64 = TypedPort   // ¿Compila?
    // var f uint16 = TypedPort  // ¿Compila?

    fmt.Println(a, b, c, d)
}
```
**Predicción**: ¿Compilan las líneas comentadas? ¿Por qué?

### Ejercicio 5: iota con múltiples valores por línea
```go
package main

import "fmt"

const (
    A, B = iota, iota * 100   // línea 0
    C, D                       // línea 1
    E, F                       // línea 2
)

func main() {
    fmt.Println(A, B)
    fmt.Println(C, D)
    fmt.Println(E, F)
}
```
**Predicción**: ¿Qué valores tienen A-F?

### Ejercicio 6: Constante que no cabe en tipo
```go
package main

import "fmt"

const Big = 1 << 100

func main() {
    fmt.Println(Big)              // ¿Compila?
    fmt.Println(Big >> 98)        // ¿Y esto?
    fmt.Println(float64(Big))     // ¿Y esto?
    // var x int = Big            // ¿Compila?
    var y float64 = Big           // ¿Compila?
    fmt.Println(y)
}
```
**Predicción**: ¿Qué líneas compilan y cuáles no? ¿Por qué `float64` sí puede?

### Ejercicio 7: Constantes de string
```go
package main

import "fmt"

const (
    Hello = "Hello, "
    World = "World"
)

const Greeting = Hello + World  // ¿Compila?
const GreetLen = len(Greeting)  // ¿Compila?
// const Bytes = []byte(Greeting)  // ¿Compila?

func main() {
    fmt.Println(Greeting)
    fmt.Println(GreetLen)
}
```
**Predicción**: ¿Qué constantes compilan y cuáles no?

### Ejercicio 8: Log level system
Implementa un sistema completo de log levels con:
1. Tipo `LogLevel` con constantes `Debug` a `Fatal` usando `iota`
2. Método `String()` que retorne el nombre en mayúsculas
3. Método `ShouldLog(minLevel LogLevel) bool` que compare niveles
4. Función `ParseLevel(s string) (LogLevel, error)` que convierta string a LogLevel
5. Variable de paquete `currentLevel` inicializada desde `LOG_LEVEL` env var

### Ejercicio 9: Permission calculator
Crea una herramienta que:
1. Defina constantes de permiso Unix con `iota` y bit shift
2. Reciba un octal como argumento (ej: "755")
3. Imprima los permisos en formato `rwxr-xr-x` para owner/group/other
4. Use constantes para los nombres de cada permiso

### Ejercicio 10: Config con constantes y defaults
```go
const (
    DefaultHost       = "0.0.0.0"
    DefaultPort       = 8080
    DefaultLogLevel   = "info"
    DefaultMaxConns   = 100
    DefaultTimeoutSec = 30
    MaxTimeoutSec     = 300
    MinPort           = 1024
    MaxPort           = 65535
)
```
Escribe una función `LoadConfig()` que:
1. Lea de variables de entorno (`HOST`, `PORT`, `LOG_LEVEL`, `MAX_CONNS`, `TIMEOUT`)
2. Use las constantes como defaults si la variable no está definida
3. Valide que los valores están en rangos válidos usando las constantes `Min`/`Max`
4. Retorne la config y un error si alguna validación falla

---

## 20. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│              CONSTANTES EN GO — RESUMEN                      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  DECLARACIÓN                                                 │
│  ├─ const Name = value          → sin tipo (flexible)        │
│  ├─ const Name Type = value     → con tipo (restrictivo)     │
│  └─ const ( ... )               → bloque agrupado            │
│                                                              │
│  TIPOS PERMITIDOS                                            │
│  ├─ bool, int*, uint*, float*, complex*, string              │
│  └─ Tipos definidos basados en los anteriores                │
│                                                              │
│  UNTYPED CONSTANTS                                           │
│  ├─ Precisión arbitraria en compilación                      │
│  ├─ Se adaptan al contexto (no necesitan cast)               │
│  └─ Default types: int, float64, bool, string, rune          │
│                                                              │
│  IOTA                                                        │
│  ├─ Empieza en 0, se incrementa por línea                    │
│  ├─ Se reinicia en cada bloque const                         │
│  ├─ _ descarta un valor (skip zero value)                    │
│  ├─ Expresiones: iota+1, 1<<iota, iota*10                   │
│  └─ Uso: enumeraciones, bit flags, unidades                  │
│                                                              │
│  PATRONES SYSADMIN                                           │
│  ├─ KB/MB/GB/TB con 1 << (10 * iota)                        │
│  ├─ Feature flags con 1 << iota + Has()/Set()/Clear()       │
│  ├─ Permisos Unix con bitmask                                │
│  ├─ Severidades con routing a canales                        │
│  ├─ Exit codes Nagios/Icinga                                 │
│  ├─ Build tags para constantes por entorno                   │
│  └─ go generate stringer para String() automático            │
│                                                              │
│  const > var cuando el valor es fijo en compilación           │
│  var cuando: runtime, env vars, tipos complejos, &dirección  │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T03 - Conversiones de tipo — casting explícito obligatorio, no hay coerción implícita, strconv package
