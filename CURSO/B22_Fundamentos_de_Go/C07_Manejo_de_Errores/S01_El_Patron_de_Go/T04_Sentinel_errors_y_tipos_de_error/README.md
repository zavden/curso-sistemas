# Sentinel Errors y Tipos de Error — io.EOF, os.ErrNotExist, Errores Custom con Campos

## 1. Introduccion

Go tiene **dos mecanismos** para que el caller identifique un error programaticamente: **sentinel errors** (valores predefinidos comparables con `errors.Is`) y **tipos de error** (structs inspeccionables con `errors.As`). Estos mecanismos son complementarios, no excluyentes, y saber cuando usar cada uno es una de las decisiones de diseno mas importantes en Go.

Un **sentinel error** es una variable de paquete de tipo `error` que actua como **valor centinela** — un valor conocido que significa una condicion especifica. `io.EOF` significa "fin de datos", `os.ErrNotExist` significa "recurso no existe", `sql.ErrNoRows` significa "la query no devolvio filas". Son como codigos de estado HTTP pero para errores de Go: el caller compara con `errors.Is(err, io.EOF)` para decidir como reaccionar.

Un **tipo de error** es un struct que implementa la interfaz `error` y lleva **datos adicionales** que el caller puede inspeccionar. `*os.PathError` tiene la operacion (`"open"`), el path (`"/etc/config"`), y el error subyacente. `*net.OpError` tiene la operacion de red, el protocolo, la direccion. El caller extrae el tipo con `errors.As(err, &pathErr)` y luego accede a los campos.

La decision entre sentinel y tipo depende de la pregunta que el caller necesita responder:
- **"¿Que paso?"** → sentinel error (`errors.Is(err, io.EOF)`)
- **"¿Que paso y cuales son los detalles?"** → tipo de error (`errors.As(err, &pathErr)`)

Para SysAdmin/DevOps, esta distincion es directamente practica: si una operacion falla porque un archivo no existe, necesitas saber "no existe" (sentinel) para decidir si crearlo. Pero si necesitas saber **que archivo** no existe y **que operacion** se intento, necesitas el tipo de error. Un sistema de monitoring necesita ambos: el sentinel para categorizar, el tipo para generar alertas con detalles.

```
┌──────────────────────────────────────────────────────────────────────────┐
│       SENTINEL ERRORS vs TIPOS DE ERROR                                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SENTINEL ERROR:                     TIPO DE ERROR:                      │
│  ─────────────                       ──────────────                      │
│  var ErrNotFound = errors.New(...)    type PathError struct {             │
│                                          Op   string                     │
│  Responde: ¿QUE paso?                   Path string                     │
│  Mecanismo: errors.Is(err, sentinel)     Err  error                     │
│  Datos: ninguno (solo identidad)     }                                   │
│  Uso: decisiones de control          Responde: ¿QUE paso Y detalles?    │
│       (if/switch)                    Mecanismo: errors.As(err, &pe)      │
│                                      Datos: campos del struct            │
│  Ejemplos:                           Uso: logging, diagnostico,          │
│  ├─ io.EOF                               reparacion                     │
│  ├─ os.ErrNotExist                                                       │
│  ├─ os.ErrPermission                Ejemplos:                            │
│  ├─ sql.ErrNoRows                   ├─ *os.PathError                    │
│  ├─ context.Canceled                ├─ *net.OpError                     │
│  └─ http.ErrServerClosed            ├─ *json.SyntaxError                │
│                                      ├─ *strconv.NumError               │
│                                      └─ *url.Error                      │
│                                                                          │
│  COMBINADOS: tipo de error que envuelve un sentinel                      │
│  *PathError{Op:"open", Path:"/etc/x", Err: os.ErrPermission}           │
│  errors.Is(err, os.ErrPermission) → true (via Unwrap)                   │
│  errors.As(err, &pathErr) → true (extrae PathError con datos)           │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Sentinel Errors — Valores Centinela

### 2.1 Definicion y convencion

```go
// Un sentinel error es una variable de paquete exportada de tipo error.
// Convencion de nombrado: Err + Condicion (CamelCase)

// Paquete errors:
// (no tiene sentinels propios — solo provee New)

// Paquete io:
var EOF = errors.New("EOF")                           // fin de stream
var ErrClosedPipe = errors.New("io: read/write on closed pipe")
var ErrNoProgress = errors.New("multiple Read calls return no data or error")
var ErrShortBuffer = errors.New("short buffer")
var ErrShortWrite = errors.New("short write")
var ErrUnexpectedEOF = errors.New("unexpected EOF")

// Paquete os:
var ErrNotExist = errors.New("file does not exist")
var ErrExist = errors.New("file already exists")
var ErrPermission = errors.New("permission denied")
var ErrClosed = errors.New("file already closed")
var ErrInvalid = errors.New("invalid argument")

// Paquete context:
var Canceled = errors.New("context canceled")
var DeadlineExceeded = errors.New("context deadline exceeded") // ← nota: tambien implementa Timeout()

// Paquete sql:
var ErrNoRows = errors.New("sql: no rows in result set")
var ErrConnDone = errors.New("sql: connection is already closed")
var ErrTxDone = errors.New("sql: transaction has already been committed or rolled back")

// Paquete net/http:
var ErrServerClosed = errors.New("http: Server closed")
var ErrHandlerTimeout = errors.New("http: Handler timeout")
```

### 2.2 El contrato de un sentinel error

Un sentinel error establece un **contrato publico**: los callers pueden depender de su existencia y comparar contra el. Esto significa:

```go
// ✓ El caller puede escribir:
if errors.Is(err, io.EOF) {
    // fin normal del stream
}

// El contrato implica:
// 1. io.EOF SIEMPRE significa "fin de datos"
// 2. io.EOF NUNCA cambiara de valor o significado
// 3. Funciones que devuelven io.EOF lo hacen consistentemente
// 4. Agregar un nuevo sentinel no rompe el contrato existente

// ROMPER EL CONTRATO (nunca hacerlo):
// var EOF = errors.New("end of file") // ← cambiar el mensaje rompe %v pero no Is
// var EOF = fmt.Errorf("EOF: %w", someOtherErr) // ← cambiar la estructura rompe Is
```

### 2.3 Por que errors.New y no una constante string

```go
// PREGUNTA: ¿por que no usar una constante string?
// const ErrNotFound = "not found" // tipo string, no error

// RESPUESTA: porque la comparacion seria por TEXTO, no por IDENTIDAD

// errors.New devuelve un puntero unico cada vez:
var ErrA = errors.New("not found")
var ErrB = errors.New("not found")
fmt.Println(ErrA == ErrB) // false — son punteros diferentes

// Esto es CRITICO: dos paquetes pueden tener errores con el mismo texto
// pero son errores DIFERENTES. La comparacion por identidad (puntero)
// evita falsos positivos.

// Si usaras strings:
// errMsg := "not found"
// if err.Error() == errMsg { } // ← fragil, coincidencia accidental
```

### 2.4 Patron: const error (tipo string como error)

Algunas librerias usan un patron alternativo para sentinels "const":

```go
// Tipo string que implementa error — permite definir errores como constantes
type Error string

func (e Error) Error() string { return string(e) }

// Ahora puedes usar const (no var):
const (
    ErrNotFound   Error = "not found"
    ErrTimeout    Error = "timeout"
    ErrForbidden  Error = "forbidden"
)

// Ventaja: son constantes — no se pueden reasignar accidentalmente
// ErrNotFound = Error("something else") // ← ERROR de compilacion con const

// Desventaja: la comparacion es por VALOR (texto), no por identidad
// Dos paquetes con const ErrNotFound Error = "not found" seran iguales
// Esto puede ser un problema o una feature, dependiendo del caso

// La stdlib de Go usa var con errors.New — este es el patron canonico
// El patron const es mas comun en librerias de terceros
```

---

## 3. Catalogo Completo de Sentinel Errors de la Stdlib

### 3.1 io — Errores de I/O

```go
package io

var EOF = errors.New("EOF")
// EOF indica que no hay mas datos para leer.
// CONTRATO: cuando Read devuelve (0, io.EOF), el stream termino normalmente.
// NOTA: Read PUEDE devolver (n > 0, io.EOF) — lee datos parciales Y señala fin.

var ErrUnexpectedEOF = errors.New("unexpected EOF")
// ErrUnexpectedEOF indica que el stream termino antes de lo esperado.
// Ejemplo: leer un archivo JSON truncado, o un header HTTP incompleto.

var ErrClosedPipe = errors.New("io: read/write on closed pipe")
// Intentar leer/escribir en un pipe ya cerrado.

var ErrShortBuffer = errors.New("short buffer")
// El buffer proporcionado es demasiado pequeño para la operacion.

var ErrShortWrite = errors.New("short write")
// Se escribieron menos bytes de los solicitados sin reportar error.

var ErrNoProgress = errors.New("multiple Read calls return no data or error")
// El Reader devolvio (0, nil) multiples veces — probablemente buggy.
```

#### io.EOF — el sentinel mas importante

```go
// io.EOF es UNICO entre los sentinels: representa una condicion NORMAL, no un error.
// "Fin de datos" no es un fallo — es el final esperado de un stream.

// Patron idiomatico con bufio.Scanner (maneja EOF internamente):
scanner := bufio.NewScanner(reader)
for scanner.Scan() {
    line := scanner.Text()
    // procesar linea
}
if err := scanner.Err(); err != nil {
    // error REAL (no EOF — Scanner maneja EOF internamente)
    log.Fatal(err)
}

// Patron con io.Reader directo:
buf := make([]byte, 1024)
for {
    n, err := reader.Read(buf)
    if n > 0 {
        process(buf[:n]) // SIEMPRE procesar datos antes de verificar err
    }
    if err != nil {
        if errors.Is(err, io.EOF) {
            break // fin normal — no es error
        }
        return fmt.Errorf("read: %w", err) // error real
    }
}

// ANTIPATRON: verificar err antes de procesar datos
// ✗ if err != nil { break } // puedes perder los ultimos bytes
// ✓ if n > 0 { process(buf[:n]) } // siempre primero
```

### 3.2 os — Errores del sistema operativo

```go
package os

var ErrNotExist = errors.New("file does not exist")
// Recurso no existe — mapeado desde syscall ENOENT

var ErrExist = errors.New("file already exists")
// Recurso ya existe — mapeado desde syscall EEXIST

var ErrPermission = errors.New("permission denied")
// Sin permisos — mapeado desde syscall EACCES y EPERM

var ErrClosed = errors.New("file already closed")
// Operacion en un descriptor ya cerrado

var ErrInvalid = errors.New("invalid argument")
// Argumento invalido para una operacion del OS

// Estos sentinels son MAPEADOS desde errores de syscall.
// os.Open devuelve *PathError cuyo Err campo contiene un syscall.Errno.
// PathError.Unwrap() y la logica de Is() mapean:
//   syscall.ENOENT → os.ErrNotExist
//   syscall.EEXIST → os.ErrExist
//   syscall.EACCES, syscall.EPERM → os.ErrPermission
```

#### Uso practico de os sentinels

```go
// Verificar si un archivo existe:
func fileExists(path string) bool {
    _, err := os.Stat(path)
    return !errors.Is(err, os.ErrNotExist)
}

// Crear archivo solo si no existe:
func createIfNotExists(path string, data []byte) error {
    f, err := os.OpenFile(path, os.O_CREATE|os.O_EXCL|os.O_WRONLY, 0644)
    if err != nil {
        if errors.Is(err, os.ErrExist) {
            return nil // ya existe — no es error
        }
        return fmt.Errorf("create %s: %w", path, err)
    }
    defer f.Close()
    _, err = f.Write(data)
    return err
}

// Manejar permisos:
func readConfig(path string) ([]byte, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        if errors.Is(err, os.ErrPermission) {
            return nil, fmt.Errorf("insufficient permissions for %s (try sudo?): %w", path, err)
        }
        if errors.Is(err, os.ErrNotExist) {
            return nil, fmt.Errorf("config file %s not found (run init first?): %w", path, err)
        }
        return nil, fmt.Errorf("read config: %w", err)
    }
    return data, nil
}
```

### 3.3 context — Errores de cancelacion

```go
package context

var Canceled = errors.New("context canceled")
// El contexto fue cancelado explicitamente (alguien llamo cancel())

// DeadlineExceeded es especial — es un tipo, no solo un sentinel
var DeadlineExceeded error = deadlineExceededError{}

type deadlineExceededError struct{}
func (deadlineExceededError) Error() string   { return "context deadline exceeded" }
func (deadlineExceededError) Timeout() bool   { return true }   // ← implementa net.Error
func (deadlineExceededError) Temporary() bool { return true }   // deprecated en Go 1.18+

// Uso practico:
func callWithTimeout(ctx context.Context) error {
    ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
    defer cancel()

    result, err := longOperation(ctx)
    if err != nil {
        if errors.Is(err, context.DeadlineExceeded) {
            return fmt.Errorf("operation timed out after 5s: %w", err)
        }
        if errors.Is(err, context.Canceled) {
            return fmt.Errorf("operation canceled by user: %w", err)
        }
        return err
    }
    _ = result
    return nil
}
```

### 3.4 Otros paquetes

```go
// database/sql
var ErrNoRows = errors.New("sql: no rows in result set")
// ← MUY comun: SELECT que no encuentra filas

// net/http
var ErrServerClosed = errors.New("http: Server closed")
// ← Returned by Serve() cuando el server se cierra con Shutdown()

// encoding/json — NO tiene sentinels, usa tipos de error
// (SyntaxError, UnmarshalTypeError, etc.)

// crypto/tls
var ErrCertificateRequired = errors.New("tls: certificate required")

// regexp
var ErrInternalError = errors.New("regexp: internal error")
```

### 3.5 Tabla exhaustiva de sentinels en la stdlib

| Paquete | Sentinel | Significado | Frecuencia de uso |
|---------|----------|-------------|-------------------|
| `io` | `EOF` | Fin de stream (normal) | Muy alta |
| `io` | `ErrUnexpectedEOF` | Stream truncado | Alta |
| `io` | `ErrClosedPipe` | Pipe cerrado | Media |
| `io` | `ErrShortWrite` | Write parcial | Baja |
| `os` | `ErrNotExist` | Archivo/dir no existe | Muy alta |
| `os` | `ErrExist` | Ya existe | Alta |
| `os` | `ErrPermission` | Sin permisos | Alta |
| `os` | `ErrClosed` | Descriptor cerrado | Media |
| `context` | `Canceled` | Contexto cancelado | Muy alta |
| `context` | `DeadlineExceeded` | Timeout del contexto | Muy alta |
| `sql` | `ErrNoRows` | Query sin resultados | Muy alta |
| `sql` | `ErrTxDone` | Transaccion terminada | Media |
| `sql` | `ErrConnDone` | Conexion cerrada | Media |
| `http` | `ErrServerClosed` | Server cerrado limpiamente | Alta |
| `http` | `ErrHandlerTimeout` | Handler excedio timeout | Media |
| `http` | `ErrBodyNotAllowed` | Body en respuesta sin body | Baja |

---

## 4. Tipos de Error — Structs con Datos

### 4.1 os.PathError / fs.PathError

```go
// Definicion (simplificada):
type PathError struct {
    Op   string // operacion: "open", "read", "write", "stat", "remove"
    Path string // ruta del archivo/directorio
    Err  error  // error subyacente (syscall error)
}

func (e *PathError) Error() string {
    return e.Op + " " + e.Path + ": " + e.Err.Error()
}

func (e *PathError) Unwrap() error {
    return e.Err
}

// Uso con errors.As:
_, err := os.Open("/etc/shadow")
if err != nil {
    var pathErr *os.PathError
    if errors.As(err, &pathErr) {
        fmt.Println("Operacion:", pathErr.Op)   // "open"
        fmt.Println("Ruta:", pathErr.Path)       // "/etc/shadow"
        fmt.Println("Error OS:", pathErr.Err)    // "permission denied"

        // Ademas, podemos verificar el sentinel:
        if errors.Is(err, os.ErrPermission) {
            fmt.Println("→ Necesitas permisos de root")
        }
    }
}
```

### 4.2 net.OpError

```go
// Definicion (simplificada):
type OpError struct {
    Op     string     // "dial", "read", "write", "close"
    Net    string     // "tcp", "udp", "unix"
    Source net.Addr   // direccion local
    Addr   net.Addr   // direccion remota
    Err    error      // error subyacente
}

func (e *OpError) Error() string { /* ... */ }
func (e *OpError) Unwrap() error { return e.Err }
func (e *OpError) Timeout() bool { /* delega a Err */ }
func (e *OpError) Temporary() bool { /* deprecated */ }

// Ejemplo practico:
conn, err := net.DialTimeout("tcp", "db.example.com:5432", 5*time.Second)
if err != nil {
    var opErr *net.OpError
    if errors.As(err, &opErr) {
        fmt.Printf("Operacion: %s\n", opErr.Op)    // "dial"
        fmt.Printf("Protocolo: %s\n", opErr.Net)    // "tcp"
        fmt.Printf("Destino: %s\n", opErr.Addr)     // "db.example.com:5432"
        fmt.Printf("Es timeout: %v\n", opErr.Timeout())

        // Drill down al error del syscall:
        var syscallErr *os.SyscallError
        if errors.As(err, &syscallErr) {
            fmt.Printf("Syscall: %s\n", syscallErr.Syscall) // "connect"
        }
    }
}
```

### 4.3 net.DNSError

```go
// DNSError tiene campos especializados para errores de DNS
type DNSError struct {
    Err         string // descripcion
    Name        string // hostname buscado
    Server      string // servidor DNS consultado
    IsTimeout   bool   // fue timeout
    IsTemporary bool   // es temporal (retriable)
    IsNotFound  bool   // NXDOMAIN (Go 1.18+)
}

// Uso:
_, err := net.LookupHost("nonexistent.example.invalid")
if err != nil {
    var dnsErr *net.DNSError
    if errors.As(err, &dnsErr) {
        fmt.Println("Host:", dnsErr.Name)
        fmt.Println("DNS Server:", dnsErr.Server)
        fmt.Println("Is NotFound:", dnsErr.IsNotFound)
        fmt.Println("Is Timeout:", dnsErr.IsTimeout)
        fmt.Println("Is Temporary:", dnsErr.IsTemporary)

        if dnsErr.IsNotFound {
            fmt.Println("→ Dominio no existe — verificar DNS o nombre")
        } else if dnsErr.IsTimeout {
            fmt.Println("→ DNS no responde — verificar red o resolver")
        }
    }
}
```

### 4.4 json.SyntaxError y json.UnmarshalTypeError

```go
// SyntaxError: JSON malformado
type SyntaxError struct {
    msg    string // no exportado
    Offset int64  // byte offset donde se encontro el error
}

// UnmarshalTypeError: tipo JSON no coincide con tipo Go
type UnmarshalTypeError struct {
    Value  string       // descripcion del valor JSON ("number", "string")
    Type   reflect.Type // tipo Go esperado
    Offset int64        // byte offset
    Struct string       // nombre del struct destino
    Field  string       // nombre del campo JSON
}

// Uso:
data := []byte(`{"port": "not-a-number"}`)
var cfg struct {
    Port int `json:"port"`
}

err := json.Unmarshal(data, &cfg)
if err != nil {
    var syntaxErr *json.SyntaxError
    if errors.As(err, &syntaxErr) {
        fmt.Printf("JSON syntax error at byte %d\n", syntaxErr.Offset)
    }

    var typeErr *json.UnmarshalTypeError
    if errors.As(err, &typeErr) {
        fmt.Printf("Type mismatch: JSON %s cannot be %s (field: %s.%s, byte %d)\n",
            typeErr.Value, typeErr.Type, typeErr.Struct, typeErr.Field, typeErr.Offset)
    }
}
```

### 4.5 strconv.NumError

```go
// NumError: error de conversion numerica
type NumError struct {
    Func string // funcion que fallo: "Atoi", "ParseInt", "ParseFloat"
    Num  string // input que causo el error
    Err  error  // error subyacente (ErrSyntax o ErrRange)
}

// Sentinels de strconv:
var ErrSyntax = errors.New("invalid syntax")
var ErrRange = errors.New("value out of range")

// Uso:
n, err := strconv.Atoi("99999999999999999999")
if err != nil {
    var numErr *strconv.NumError
    if errors.As(err, &numErr) {
        fmt.Println("Funcion:", numErr.Func) // "Atoi"
        fmt.Println("Input:", numErr.Num)     // "99999999999999999999"

        if errors.Is(err, strconv.ErrRange) {
            fmt.Println("→ Numero fuera de rango para int")
        }
        if errors.Is(err, strconv.ErrSyntax) {
            fmt.Println("→ No es un numero valido")
        }
    }
}
```

### 4.6 Patron comun: tipo con sentinel embebido

```
┌──────────────────────────────────────────────────────────────────────┐
│  PATRON DE LA STDLIB: Tipo con Sentinel                              │
│                                                                      │
│  Sentinel error (identidad):                                         │
│    var ErrNotExist = errors.New("file does not exist")               │
│                                                                      │
│  Tipo de error (datos):                                              │
│    type PathError struct {                                           │
│        Op   string                                                   │
│        Path string                                                   │
│        Err  error  ← contiene el sentinel (o syscall error)         │
│    }                                                                 │
│    func (e *PathError) Unwrap() error { return e.Err }              │
│                                                                      │
│  Relacion:                                                           │
│    os.Open("/nope")                                                  │
│    → &PathError{Op:"open", Path:"/nope", Err: syscall.ENOENT}      │
│    errors.Is(err, os.ErrNotExist) → true (via Unwrap + mapping)     │
│    errors.As(err, &pathErr)       → true (extrae el PathError)      │
│                                                                      │
│  El caller elige QUE nivel de detalle necesita:                      │
│    - errors.Is: ¿que paso? (not exist, permission, timeout)         │
│    - errors.As: ¿que paso con detalles? (que archivo, que operacion)│
└──────────────────────────────────────────────────────────────────────┘
```

---

## 5. Disenar Errores Custom para Tu Dominio

### 5.1 Cuando crear un sentinel

```go
// Crear sentinels cuando:
// 1. El error representa una condicion BIEN DEFINIDA del dominio
// 2. Multiples funciones pueden devolver la misma condicion
// 3. El caller necesita distinguir esta condicion de otras

// Paquete de infraestructura: errores de operaciones
var (
    ErrServiceDown      = errors.New("service is down")
    ErrServiceDegraded  = errors.New("service is degraded")
    ErrQuotaExceeded    = errors.New("quota exceeded")
    ErrMaintenanceMode  = errors.New("service in maintenance mode")
    ErrCircuitOpen      = errors.New("circuit breaker is open")
    ErrRateLimited      = errors.New("rate limit exceeded")
    ErrConfigInvalid    = errors.New("invalid configuration")
    ErrDependencyFailed = errors.New("dependency check failed")
)

// Cada sentinel es un CONTRATO: si exportas ErrServiceDown,
// los callers pueden depender de el. No lo renombres ni cambies su semantica.
```

### 5.2 Cuando crear un tipo de error

```go
// Crear tipos cuando:
// 1. El caller necesita DATOS ademas de la identidad
// 2. El error tiene contexto variable (host, port, servicio)
// 3. Quieres metodos utilitarios (Timeout(), Retriable(), etc.)
// 4. El error envuelve otro error con Unwrap()

// Tipo de error para operaciones de infraestructura
type InfraError struct {
    Service   string        // servicio afectado
    Operation string        // operacion que fallo
    Host      string        // host donde ocurrio
    Duration  time.Duration // cuanto tardo antes de fallar
    Cause     error         // error subyacente
    Retriable bool          // ¿se puede reintentar?
}

func (e *InfraError) Error() string {
    msg := fmt.Sprintf("%s %s on %s", e.Operation, e.Service, e.Host)
    if e.Duration > 0 {
        msg += fmt.Sprintf(" (after %v)", e.Duration)
    }
    if e.Cause != nil {
        msg += ": " + e.Cause.Error()
    }
    return msg
}

func (e *InfraError) Unwrap() error {
    return e.Cause
}

// Metodos utilitarios para el caller
func (e *InfraError) IsRetriable() bool {
    return e.Retriable
}

func (e *InfraError) Timeout() bool {
    return errors.Is(e.Cause, context.DeadlineExceeded) ||
        errors.Is(e.Cause, ErrTimeout)
}
```

### 5.3 Sentinel + tipo juntos

```go
// Patron completo: sentinels para condiciones + tipo para datos

// Sentinels
var (
    ErrNotFound   = errors.New("resource not found")
    ErrConflict   = errors.New("resource conflict")
    ErrBadRequest = errors.New("bad request")
)

// Tipo que envuelve sentinels
type APIError struct {
    Code     int
    Resource string
    ID       string
    Detail   string
    Cause    error // puede ser un sentinel
}

func (e *APIError) Error() string {
    return fmt.Sprintf("API %d: %s %s/%s: %s", e.Code, e.Detail, e.Resource, e.ID, e.Cause)
}

func (e *APIError) Unwrap() error {
    return e.Cause
}

// Constructores que combinan sentinel + tipo:
func NewNotFound(resource, id string) error {
    return &APIError{
        Code:     404,
        Resource: resource,
        ID:       id,
        Detail:   "not found",
        Cause:    ErrNotFound, // sentinel embebido
    }
}

func NewConflict(resource, id, detail string) error {
    return &APIError{
        Code:     409,
        Resource: resource,
        ID:       id,
        Detail:   detail,
        Cause:    ErrConflict,
    }
}

// El caller puede usar AMBOS mecanismos:
err := NewNotFound("deployment", "app-v2")

// errors.Is para decision rapida:
if errors.Is(err, ErrNotFound) {
    // crear el recurso
}

// errors.As para detalles:
var apiErr *APIError
if errors.As(err, &apiErr) {
    fmt.Printf("HTTP %d: %s/%s\n", apiErr.Code, apiErr.Resource, apiErr.ID)
}
```

---

## 6. El Patron Sentinel en sql.ErrNoRows — Caso de Estudio

`sql.ErrNoRows` es uno de los sentinels mas usados y mas propenso a errores:

```go
// El problema: ¿"no rows" es un error o un resultado valido?
func getUserByEmail(db *sql.DB, email string) (*User, error) {
    var u User
    err := db.QueryRow("SELECT id, name FROM users WHERE email = $1", email).
        Scan(&u.ID, &u.Name)

    if err != nil {
        if errors.Is(err, sql.ErrNoRows) {
            // ¿Que devolver? Esto depende del CONTEXTO:
            // Opcion A: devolver nil, nil (no es error, solo no hay usuario)
            // Opcion B: devolver nil, ErrNotFound (es un error del dominio)
            // Opcion C: devolver nil, err (propagar sql.ErrNoRows)
        }
        return nil, fmt.Errorf("query user %s: %w", email, err)
    }
    return &u, nil
}
```

### Las tres opciones y cuando usar cada una

```go
// OPCION A: nil, nil — "no es un error, es ausencia"
// Usar cuando: la ausencia es una respuesta valida (ej: buscar cache)
func findUser(db *sql.DB, email string) (*User, error) {
    var u User
    err := db.QueryRow("...").Scan(&u.ID, &u.Name)
    if errors.Is(err, sql.ErrNoRows) {
        return nil, nil // no encontrado — no es error
    }
    if err != nil {
        return nil, fmt.Errorf("find user: %w", err)
    }
    return &u, nil
}
// Caller: user, err := findUser(db, email)
//         if err != nil { handle } // error real de DB
//         if user == nil { /* no existe */ } // ausencia


// OPCION B: nil, ErrNotFound — "es un error del dominio"
// Usar cuando: el caller necesita distinguir "no existe" de otros errores
func getUser(db *sql.DB, email string) (*User, error) {
    var u User
    err := db.QueryRow("...").Scan(&u.ID, &u.Name)
    if errors.Is(err, sql.ErrNoRows) {
        return nil, fmt.Errorf("user %s: %w", email, ErrNotFound) // WRAP con tu sentinel
    }
    if err != nil {
        return nil, fmt.Errorf("get user: %w", err)
    }
    return &u, nil
}
// Caller: if errors.Is(err, ErrNotFound) { /* crear usuario */ }


// OPCION C: propagar sql.ErrNoRows — casi nunca recomendado
// Problema: acopla al caller con el paquete database/sql
func getUserRaw(db *sql.DB, email string) (*User, error) {
    var u User
    err := db.QueryRow("...").Scan(&u.ID, &u.Name)
    if err != nil {
        return nil, fmt.Errorf("get user: %w", err) // propaga sql.ErrNoRows
    }
    return &u, nil
}
// Caller tendria que: errors.Is(err, sql.ErrNoRows)
// ← el caller ahora depende de sql internamente — MAL
```

---

## 7. Disenar Jerarquias de Errores

### 7.1 Jerarquia plana (solo sentinels)

```go
// Para dominios simples — todos los errores al mismo nivel
var (
    ErrNotFound    = errors.New("not found")
    ErrBadInput    = errors.New("bad input")
    ErrUnauthorized = errors.New("unauthorized")
    ErrInternal    = errors.New("internal error")
)

// El caller hace switch basado en errors.Is:
switch {
case errors.Is(err, ErrNotFound):
    w.WriteHeader(404)
case errors.Is(err, ErrBadInput):
    w.WriteHeader(400)
case errors.Is(err, ErrUnauthorized):
    w.WriteHeader(401)
default:
    w.WriteHeader(500)
}
```

### 7.2 Jerarquia con tipo + sentinels (recomendado para dominios complejos)

```go
// Tipo base con categoria (sentinel) + datos
type DomainError struct {
    Kind    error  // sentinel: ErrNotFound, ErrConflict, etc.
    Entity  string // "user", "deployment", "service"
    ID      string // identificador
    Message string // detalle legible
    Cause   error  // error subyacente (opcional)
}

func (e *DomainError) Error() string {
    msg := fmt.Sprintf("%s %s", e.Entity, e.Kind)
    if e.ID != "" {
        msg += fmt.Sprintf(" (id=%s)", e.ID)
    }
    if e.Message != "" {
        msg += ": " + e.Message
    }
    if e.Cause != nil {
        msg += ": " + e.Cause.Error()
    }
    return msg
}

func (e *DomainError) Unwrap() error {
    return e.Kind // ← Unwrap devuelve el sentinel!
    // Esto permite: errors.Is(err, ErrNotFound) a traves del DomainError
}

// Constructores por caso:
func NotFound(entity, id string) error {
    return &DomainError{Kind: ErrNotFound, Entity: entity, ID: id}
}

func Conflict(entity, id, msg string) error {
    return &DomainError{Kind: ErrConflict, Entity: entity, ID: id, Message: msg}
}

func Unauthorized(entity, msg string) error {
    return &DomainError{Kind: ErrUnauthorized, Entity: entity, Message: msg}
}

// Uso:
err := NotFound("deployment", "app-v2")

// errors.Is funciona a traves del tipo:
errors.Is(err, ErrNotFound) // true — Unwrap devuelve ErrNotFound

// errors.As extrae el tipo con datos:
var de *DomainError
errors.As(err, &de) // de.Entity == "deployment", de.ID == "app-v2"
```

### 7.3 Cuando Unwrap devuelve el sentinel vs el Cause

```go
// Dilema: DomainError tiene Kind (sentinel) y Cause (error subyacente).
// ¿Cual devuelve Unwrap()?

// Opcion A: Unwrap() devuelve Kind (el sentinel)
func (e *DomainError) Unwrap() error { return e.Kind }
// Pro: errors.Is(err, ErrNotFound) funciona directamente
// Con: el Cause (error subyacente) no esta en la cadena

// Opcion B: Unwrap() devuelve Cause
func (e *DomainError) Unwrap() error { return e.Cause }
// Pro: la cadena sigue al error real subyacente
// Con: errors.Is(err, ErrNotFound) NO funciona (Kind no esta en cadena)

// Opcion C (Go 1.20+): Unwrap devuelve AMBOS
func (e *DomainError) Unwrap() []error {
    errs := []error{e.Kind}
    if e.Cause != nil {
        errs = append(errs, e.Cause)
    }
    return errs
}
// Pro: errors.Is funciona para AMBOS (sentinel Y causa)
// Con: solo Go 1.20+

// Opcion D: Custom Is
func (e *DomainError) Is(target error) bool {
    return errors.Is(e.Kind, target) // matchear por Kind sin Unwrap
}
func (e *DomainError) Unwrap() error { return e.Cause }
// Pro: Is matchea Kind, Unwrap sigue Cause — ambos funcionan
// Con: mas codigo
```

---

## 8. Interfaces de Error Opcionales

Ademas de la interfaz basica `error`, Go usa interfaces opcionales que los errores pueden implementar para indicar propiedades adicionales:

### 8.1 net.Error — Timeout y Temporary

```go
// net.Error extiende error con dos propiedades:
type Error interface {
    error
    Timeout() bool   // ¿fue un timeout?
    Temporary() bool // ¿es temporal/retriable? (deprecated Go 1.18+)
}

// Uso — verificar si un error de red fue timeout:
func connectWithRetry(addr string, maxRetries int) (net.Conn, error) {
    var lastErr error
    for i := 0; i < maxRetries; i++ {
        conn, err := net.DialTimeout("tcp", addr, 5*time.Second)
        if err == nil {
            return conn, nil
        }
        lastErr = err

        // Verificar si es timeout (retriable)
        var netErr net.Error
        if errors.As(err, &netErr) && netErr.Timeout() {
            log.Printf("timeout connecting to %s, retrying (%d/%d)", addr, i+1, maxRetries)
            continue
        }

        // No es timeout — probablemente no vale la pena reintentar
        return nil, fmt.Errorf("connect %s: %w", addr, err)
    }
    return nil, fmt.Errorf("connect %s after %d attempts: %w", addr, maxRetries, lastErr)
}
```

### 8.2 Crear tus propias interfaces de error opcionales

```go
// Puedes definir interfaces que tus errores opcionalmente implementen

// Retriable indica si un error se puede reintentar
type Retriable interface {
    error
    IsRetriable() bool
    RetryAfter() time.Duration
}

// Categorizable indica la categoria HTTP del error
type Categorizable interface {
    error
    HTTPStatus() int
}

// Tus tipos de error implementan las interfaces relevantes:
type ServiceError struct {
    Service   string
    Message   string
    Status    int
    Retry     bool
    RetryWait time.Duration
    Cause     error
}

func (e *ServiceError) Error() string {
    return fmt.Sprintf("[%d] %s: %s", e.Status, e.Service, e.Message)
}

func (e *ServiceError) Unwrap() error        { return e.Cause }
func (e *ServiceError) HTTPStatus() int       { return e.Status }
func (e *ServiceError) IsRetriable() bool     { return e.Retry }
func (e *ServiceError) RetryAfter() time.Duration { return e.RetryWait }

// Funciones helper que verifican las interfaces opcionales:
func ShouldRetry(err error) bool {
    var r Retriable
    return errors.As(err, &r) && r.IsRetriable()
}

func HTTPStatusFromError(err error) int {
    var c Categorizable
    if errors.As(err, &c) {
        return c.HTTPStatus()
    }
    return 500 // default
}

func RetryDelay(err error) time.Duration {
    var r Retriable
    if errors.As(err, &r) && r.IsRetriable() {
        return r.RetryAfter()
    }
    return 0
}
```

---

## 9. Antipatrones con Sentinel Errors y Tipos

### 9.1 Exportar errores internos

```go
// ✗ MALO: exportar errores que son detalles de implementacion
package mydb

var ErrPostgresDown = errors.New("postgres connection lost")
// ← el caller ahora depende de que uses Postgres

// ✓ BUENO: exportar errores del dominio
var ErrDatabaseUnavailable = errors.New("database unavailable")
// ← abstracto: si cambias a MySQL, el sentinel sigue valido
```

### 9.2 Comparar errores por string

```go
// ✗ NUNCA: comparar por texto
if err.Error() == "file does not exist" { // ← fragil
    // ...
}

// ✓ SIEMPRE: usar errors.Is
if errors.Is(err, os.ErrNotExist) { // ← robusto
    // ...
}
```

### 9.3 Crear sentinels en funciones (no en variables de paquete)

```go
// ✗ MALO: crear errores dentro de funciones
func check() error {
    return errors.New("not found") // error diferente en cada llamada
}
// errors.Is(check(), check()) → false — son instancias diferentes!

// ✓ BUENO: definir como variable de paquete
var ErrNotFound = errors.New("not found")
func check() error {
    return ErrNotFound // siempre la misma instancia
}
// errors.Is(check(), ErrNotFound) → true
```

### 9.4 Type assertion en lugar de errors.As

```go
// ✗ FRAGIL: type assertion directa (no recorre la cadena)
if pathErr, ok := err.(*os.PathError); ok {
    // Solo funciona si err es EXACTAMENTE *os.PathError
    // Falla si err esta wrappedo
}

// ✓ ROBUSTO: errors.As (recorre la cadena)
var pathErr *os.PathError
if errors.As(err, &pathErr) {
    // Funciona incluso si err esta wrappedo 10 niveles
}
```

### 9.5 Sentinel error mutable

```go
// ✗ PELIGROSO: el sentinel puede ser reasignado
var ErrBadRequest = errors.New("bad request")
// En otro lugar del codigo:
ErrBadRequest = errors.New("something else") // ← rompe TODOS los callers

// ✓ MEJOR: usar const con tipo string (si quieres inmutabilidad)
type Error string
func (e Error) Error() string { return string(e) }
const ErrBadRequest Error = "bad request"
// ErrBadRequest = "other" // ← ERROR de compilacion
```

---

## 10. Comparacion con C y Rust

### C — errno y defines

```c
// C: errores son enteros definidos en errno.h
#include <errno.h>

// "Sentinels" de C — macros numericas
#define ENOENT  2   // No such file or directory
#define EACCES  13  // Permission denied
#define EEXIST  17  // File exists
#define EINVAL  22  // Invalid argument
#define ENOSPC  28  // No space left on device
#define ETIMEDOUT 110 // Connection timed out

// Uso:
FILE* f = fopen("/etc/config", "r");
if (f == NULL) {
    switch (errno) {
    case ENOENT:
        printf("File not found\n");
        break;
    case EACCES:
        printf("Permission denied\n");
        break;
    default:
        printf("Error: %s\n", strerror(errno));
    }
}

// Problemas de C:
// 1. errno es global — race conditions en multithreading
// 2. errno se sobreescribe con la siguiente llamada
// 3. No hay "tipos de error" — solo un int
// 4. No hay wrapping — no puedes encadenar contexto
// 5. strerror() devuelve string estatico, no un valor programable
```

### Rust — enum errors y pattern matching

```rust
// Rust: errores son enums — equivalente a sentinel + tipo combinados
use std::io;
use std::num::ParseIntError;

// io::ErrorKind es el "sentinel" de Rust para IO
// Es un enum con ~40 variantes:
match io::Error::last_os_error().kind() {
    io::ErrorKind::NotFound => println!("Not found"),
    io::ErrorKind::PermissionDenied => println!("Permission denied"),
    io::ErrorKind::ConnectionRefused => println!("Connection refused"),
    io::ErrorKind::TimedOut => println!("Timed out"),
    io::ErrorKind::AlreadyExists => println!("Already exists"),
    _ => println!("Other error"),
}

// Rust: errores custom son enums — EXHAUSTIVOS (el compilador verifica)
#[derive(Debug)]
enum AppError {
    NotFound { entity: String, id: String },
    Conflict { entity: String, detail: String },
    Unauthorized { reason: String },
    Internal(Box<dyn std::error::Error>), // wrapping dinamico
}

impl std::fmt::Display for AppError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            AppError::NotFound { entity, id } =>
                write!(f, "{} {} not found", entity, id),
            AppError::Conflict { entity, detail } =>
                write!(f, "{} conflict: {}", entity, detail),
            AppError::Unauthorized { reason } =>
                write!(f, "unauthorized: {}", reason),
            AppError::Internal(e) =>
                write!(f, "internal error: {}", e),
        }
    }
}

// Pattern matching es EXHAUSTIVO — el compilador asegura que
// manejas TODOS los casos:
fn handle_error(e: &AppError) {
    match e {
        AppError::NotFound { .. } => { /* 404 */ }
        AppError::Conflict { .. } => { /* 409 */ }
        AppError::Unauthorized { .. } => { /* 401 */ }
        AppError::Internal(_) => { /* 500 */ }
        // Si agregas una nueva variante, este match NO compila
        // hasta que la manejes — IMPOSIBLE olvidar un caso
    }
}
```

### Tabla comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│  SENTINEL / ERROR TYPES — C vs Go vs Rust                                │
├──────────────────┬───────────────┬────────────────┬──────────────────────┤
│  Aspecto         │  C            │  Go            │  Rust                │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Sentinels       │  #define ERRNO│  var Err* =    │  enum ErrorKind      │
│                  │  (enteros)    │  errors.New()  │  (variantes)         │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Tipos           │  No existen   │  structs con   │  enum con data       │
│  de error        │  (solo int)   │  Error()+      │  (variantes con      │
│                  │               │  Unwrap()      │  campos)             │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Comparacion     │  switch errno │  errors.Is()   │  match enum variant  │
│                  │               │  (recorre      │  (exhaustivo!)       │
│                  │               │  cadena)       │                      │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Extraccion      │  N/A          │  errors.As()   │  if let / match      │
│  de datos        │               │                │  con destructuring   │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Exhaustividad   │  No           │  No            │  SI — match obliga   │
│                  │               │  (switch sin   │  a manejar TODOS     │
│                  │               │  default OK)   │  los casos           │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Extensibilidad  │  Agregar      │  Agregar       │  Agregar variante    │
│                  │  #define      │  var Err* =    │  ROMPE compilacion   │
│                  │  (open)       │  (open)        │  de match existentes │
│                  │               │                │  (#[non_exhaustive]  │
│                  │               │                │  para evitarlo)      │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Thread safety   │  errno NO     │  Si (valores   │  Si (owned values)   │
│                  │  es safe      │  inmutables)   │                      │
└──────────────────┴───────────────┴────────────────┴──────────────────────┘
```

---

## 11. Programa Completo: Infrastructure Error Hierarchy

```go
// ============================================================================
// INFRASTRUCTURE ERROR HIERARCHY — Sentinel Errors + Error Types
//
// Demuestra:
// - Sentinel errors para condiciones del dominio
// - Tipos de error con datos estructurados
// - Jerarquia: tipo con sentinel embebido (Unwrap devuelve sentinel)
// - Multi-unwrap (Go 1.20+) para sentinel + cause
// - Interfaces opcionales (Retriable, Categorizable)
// - Conversion de error a HTTP status
// - Catalogo de errores de la stdlib (io.EOF, os.ErrNotExist, etc.)
// - sql.ErrNoRows handling
//
// Compilar y ejecutar:
//   go run main.go
// ============================================================================

package main

import (
    "errors"
    "fmt"
    "strings"
    "time"
)

// ============================================================================
// PARTE 1: Sentinel errors del dominio
// ============================================================================

var (
    // Errores de recursos
    ErrNotFound    = errors.New("not found")
    ErrConflict    = errors.New("already exists")
    ErrGone        = errors.New("resource deleted")

    // Errores de autenticacion/autorizacion
    ErrUnauthorized = errors.New("unauthorized")
    ErrForbidden    = errors.New("forbidden")

    // Errores operacionales
    ErrTimeout      = errors.New("timeout")
    ErrUnavailable  = errors.New("service unavailable")
    ErrRateLimited  = errors.New("rate limited")
    ErrCircuitOpen  = errors.New("circuit breaker open")

    // Errores de datos
    ErrValidation   = errors.New("validation failed")
    ErrCorrupted    = errors.New("data corrupted")
)

// ============================================================================
// PARTE 2: Interfaces opcionales
// ============================================================================

// Retriable indica si un error se puede reintentar
type Retriable interface {
    IsRetriable() bool
    RetryAfter() time.Duration
}

// HTTPStatusCode mapea errores a codigos HTTP
type HTTPStatusCode interface {
    HTTPStatus() int
}

// Helper functions
func ShouldRetry(err error) bool {
    var r Retriable
    return errors.As(err, &r) && r.IsRetriable()
}

func GetHTTPStatus(err error) int {
    var h HTTPStatusCode
    if errors.As(err, &h) {
        return h.HTTPStatus()
    }

    // Fallback: mapear sentinels a HTTP status
    switch {
    case errors.Is(err, ErrNotFound):
        return 404
    case errors.Is(err, ErrConflict):
        return 409
    case errors.Is(err, ErrUnauthorized):
        return 401
    case errors.Is(err, ErrForbidden):
        return 403
    case errors.Is(err, ErrTimeout):
        return 504
    case errors.Is(err, ErrUnavailable):
        return 503
    case errors.Is(err, ErrRateLimited):
        return 429
    case errors.Is(err, ErrValidation):
        return 400
    default:
        return 500
    }
}

// ============================================================================
// PARTE 3: Tipo de error del dominio
// ============================================================================

// InfraError combina sentinel + datos + interfaces opcionales
type InfraError struct {
    Kind      error         // sentinel (ErrNotFound, ErrTimeout, etc.)
    Service   string        // servicio afectado
    Operation string        // operacion que fallo
    Detail    string        // descripcion legible
    Cause     error         // error subyacente
    retry     bool          // retriable?
    retryWait time.Duration // esperar antes de reintentar
    status    int           // HTTP status override
}

func (e *InfraError) Error() string {
    var parts []string
    if e.Service != "" {
        parts = append(parts, e.Service)
    }
    if e.Operation != "" {
        parts = append(parts, e.Operation)
    }
    prefix := strings.Join(parts, "/")

    msg := prefix
    if e.Detail != "" {
        msg += ": " + e.Detail
    }
    if e.Cause != nil {
        msg += ": " + e.Cause.Error()
    }
    return msg
}

// Unwrap devuelve AMBOS: sentinel y causa (Go 1.20+)
func (e *InfraError) Unwrap() []error {
    errs := []error{}
    if e.Kind != nil {
        errs = append(errs, e.Kind)
    }
    if e.Cause != nil {
        errs = append(errs, e.Cause)
    }
    return errs
}

// Interfaces opcionales
func (e *InfraError) IsRetriable() bool          { return e.retry }
func (e *InfraError) RetryAfter() time.Duration  { return e.retryWait }
func (e *InfraError) HTTPStatus() int {
    if e.status != 0 {
        return e.status
    }
    return GetHTTPStatus(e.Kind)
}

// ============================================================================
// PARTE 4: Builder pattern para construir errores
// ============================================================================

type InfraErrorBuilder struct {
    err InfraError
}

func NewInfraError(kind error) *InfraErrorBuilder {
    return &InfraErrorBuilder{err: InfraError{Kind: kind}}
}

func (b *InfraErrorBuilder) Service(s string) *InfraErrorBuilder {
    b.err.Service = s
    return b
}

func (b *InfraErrorBuilder) Op(op string) *InfraErrorBuilder {
    b.err.Operation = op
    return b
}

func (b *InfraErrorBuilder) Detail(d string) *InfraErrorBuilder {
    b.err.Detail = d
    return b
}

func (b *InfraErrorBuilder) Cause(c error) *InfraErrorBuilder {
    b.err.Cause = c
    return b
}

func (b *InfraErrorBuilder) Retriable(wait time.Duration) *InfraErrorBuilder {
    b.err.retry = true
    b.err.retryWait = wait
    return b
}

func (b *InfraErrorBuilder) Status(code int) *InfraErrorBuilder {
    b.err.status = code
    return b
}

func (b *InfraErrorBuilder) Build() error {
    return &b.err
}

// ============================================================================
// PARTE 5: Simulaciones de operaciones
// ============================================================================

func findDeployment(name string) error {
    if name == "ghost-app" {
        return NewInfraError(ErrNotFound).
            Service("kubernetes").
            Op("get").
            Detail(fmt.Sprintf("deployment/%s not found in namespace default", name)).
            Build()
    }
    return nil
}

func scaleDeployment(name string, replicas int) error {
    if replicas > 100 {
        return NewInfraError(ErrRateLimited).
            Service("kubernetes").
            Op("scale").
            Detail(fmt.Sprintf("max 100 replicas, requested %d", replicas)).
            Retriable(30 * time.Second).
            Build()
    }
    if name == "broken-app" {
        return NewInfraError(ErrUnavailable).
            Service("kubernetes").
            Op("scale").
            Detail("API server not responding").
            Cause(fmt.Errorf("dial tcp 10.0.0.1:6443: %w", ErrTimeout)).
            Retriable(5 * time.Second).
            Build()
    }
    return nil
}

func readSecrets(path string) error {
    if strings.Contains(path, "restricted") {
        return NewInfraError(ErrForbidden).
            Service("vault").
            Op("read").
            Detail(fmt.Sprintf("policy denies access to %s", path)).
            Status(403).
            Build()
    }
    return nil
}

func validateConfig(cfg map[string]string) error {
    var errs []error
    if cfg["port"] == "" {
        errs = append(errs, fmt.Errorf("field port: %w", ErrValidation))
    }
    if cfg["host"] == "" {
        errs = append(errs, fmt.Errorf("field host: %w", ErrValidation))
    }
    if len(errs) > 0 {
        return NewInfraError(ErrValidation).
            Service("config").
            Op("validate").
            Cause(errors.Join(errs...)).
            Build()
    }
    return nil
}

// ============================================================================
// PARTE 6: main
// ============================================================================

func main() {
    fmt.Println("╔══════════════════════════════════════════════════════════════╗")
    fmt.Println("║     Sentinel Errors + Error Types — Infrastructure Demo     ║")
    fmt.Println("╚══════════════════════════════════════════════════════════════╝")
    fmt.Println()

    // Lista de operaciones a ejecutar
    operations := []struct {
        name string
        fn   func() error
    }{
        {"Find ghost deployment", func() error { return findDeployment("ghost-app") }},
        {"Scale over limit", func() error { return scaleDeployment("api", 200) }},
        {"Scale broken app", func() error { return scaleDeployment("broken-app", 3) }},
        {"Read restricted secret", func() error { return readSecrets("secret/restricted/db-password") }},
        {"Validate bad config", func() error { return validateConfig(map[string]string{}) }},
        {"Validate good config", func() error {
            return validateConfig(map[string]string{"port": "8080", "host": "localhost"})
        }},
        {"Find existing deployment", func() error { return findDeployment("real-app") }},
    }

    for i, op := range operations {
        fmt.Printf("─── Operation %d: %s ───\n", i+1, op.name)

        err := op.fn()
        if err == nil {
            fmt.Println("  Result: SUCCESS")
            fmt.Println()
            continue
        }

        fmt.Printf("  Error: %v\n", err)
        fmt.Printf("  HTTP:  %d\n", GetHTTPStatus(err))

        // Sentinel checks
        sentinels := []struct{ name string; err error }{
            {"NotFound", ErrNotFound}, {"Conflict", ErrConflict},
            {"Unauthorized", ErrUnauthorized}, {"Forbidden", ErrForbidden},
            {"Timeout", ErrTimeout}, {"Unavailable", ErrUnavailable},
            {"RateLimited", ErrRateLimited}, {"Validation", ErrValidation},
        }
        for _, s := range sentinels {
            if errors.Is(err, s.err) {
                fmt.Printf("  Is:    %s ✓\n", s.name)
            }
        }

        // Type extraction
        var infraErr *InfraError
        if errors.As(err, &infraErr) {
            fmt.Printf("  Type:  InfraError{Service:%s, Op:%s}\n",
                infraErr.Service, infraErr.Operation)
        }

        // Retriable check
        if ShouldRetry(err) {
            var r Retriable
            errors.As(err, &r)
            fmt.Printf("  Retry: yes (after %v)\n", r.RetryAfter())
        }

        fmt.Println()
    }

    // ─── Demo: mapeo de sentinels a HTTP ───
    fmt.Println("=== Sentinel → HTTP Status Mapping ===")
    fmt.Println()
    allSentinels := []struct{ name string; err error }{
        {"ErrNotFound", ErrNotFound},
        {"ErrConflict", ErrConflict},
        {"ErrUnauthorized", ErrUnauthorized},
        {"ErrForbidden", ErrForbidden},
        {"ErrTimeout", ErrTimeout},
        {"ErrUnavailable", ErrUnavailable},
        {"ErrRateLimited", ErrRateLimited},
        {"ErrValidation", ErrValidation},
        {"ErrCircuitOpen", ErrCircuitOpen},
        {"ErrCorrupted", ErrCorrupted},
    }
    for _, s := range allSentinels {
        fmt.Printf("  %-20s → HTTP %d\n", s.name, GetHTTPStatus(s.err))
    }

    fmt.Println()
    fmt.Println(strings.Repeat("─", 62))
    fmt.Println()
    fmt.Println("  REGLAS:")
    fmt.Println("  1. Sentinel = condicion conocida (comparar con errors.Is)")
    fmt.Println("  2. Tipo = datos del error (extraer con errors.As)")
    fmt.Println("  3. Combinar: tipo con sentinel en Unwrap()")
    fmt.Println("  4. Interfaces opcionales: Retriable, HTTPStatusCode")
    fmt.Println("  5. Builder pattern para construir errores ricos")
    fmt.Println("  6. No exportar errores internos (detalles de implementacion)")
    fmt.Println()
}
```

**Salida esperada (parcial):**
```
─── Operation 1: Find ghost deployment ───
  Error: kubernetes/get: deployment/ghost-app not found in namespace default
  HTTP:  404
  Is:    NotFound ✓
  Type:  InfraError{Service:kubernetes, Op:get}

─── Operation 2: Scale over limit ───
  Error: kubernetes/scale: max 100 replicas, requested 200
  HTTP:  429
  Is:    RateLimited ✓
  Type:  InfraError{Service:kubernetes, Op:scale}
  Retry: yes (after 30s)

─── Operation 3: Scale broken app ───
  Error: kubernetes/scale: API server not responding: dial tcp 10.0.0.1:6443: timeout
  HTTP:  503
  Is:    Timeout ✓
  Is:    Unavailable ✓
  Type:  InfraError{Service:kubernetes, Op:scale}
  Retry: yes (after 5s)
```

---

## 12. Ejercicios

### Ejercicio 1: Disenar errores para un servicio de cache

```go
// Diseña la jerarquia de errores para un servicio de cache distribuida.
// Necesitas:
// 1. Sentinels: ErrCacheMiss, ErrCacheExpired, ErrCacheFull, ErrNodeDown
// 2. Un tipo CacheError con: Key, Node, TTL (time.Duration), Cause
// 3. CacheError debe satisfacer Retriable si el error es ErrNodeDown
// 4. errors.Is(err, ErrCacheMiss) debe funcionar a traves del tipo
//
// Implementa y demuestra con 3 casos de test.
```

<details>
<summary>Solucion</summary>

```go
var (
    ErrCacheMiss    = errors.New("cache miss")
    ErrCacheExpired = errors.New("cache entry expired")
    ErrCacheFull    = errors.New("cache is full")
    ErrNodeDown     = errors.New("cache node is down")
)

type CacheError struct {
    Kind  error
    Key   string
    Node  string
    TTL   time.Duration
    Cause error
}

func (e *CacheError) Error() string {
    msg := fmt.Sprintf("cache[%s] key=%s", e.Node, e.Key)
    if e.TTL > 0 {
        msg += fmt.Sprintf(" ttl=%v", e.TTL)
    }
    msg += ": " + e.Kind.Error()
    if e.Cause != nil {
        msg += ": " + e.Cause.Error()
    }
    return msg
}

func (e *CacheError) Unwrap() []error {
    errs := []error{e.Kind}
    if e.Cause != nil {
        errs = append(errs, e.Cause)
    }
    return errs
}

func (e *CacheError) IsRetriable() bool {
    return errors.Is(e.Kind, ErrNodeDown)
}

func (e *CacheError) RetryAfter() time.Duration {
    if e.IsRetriable() {
        return 2 * time.Second
    }
    return 0
}

func main() {
    // Caso 1: cache miss
    err1 := &CacheError{Kind: ErrCacheMiss, Key: "user:123", Node: "cache-01"}
    fmt.Println(err1)
    fmt.Println("Is CacheMiss?", errors.Is(err1, ErrCacheMiss))   // true
    fmt.Println("Retriable?", ShouldRetry(err1))                    // false

    // Caso 2: nodo caido
    err2 := &CacheError{Kind: ErrNodeDown, Key: "session:abc", Node: "cache-03",
        Cause: errors.New("connection refused")}
    fmt.Println("\n", err2)
    fmt.Println("Is NodeDown?", errors.Is(err2, ErrNodeDown))       // true
    fmt.Println("Retriable?", ShouldRetry(err2))                     // true

    // Caso 3: expirado
    err3 := &CacheError{Kind: ErrCacheExpired, Key: "token:xyz", Node: "cache-01",
        TTL: 5 * time.Minute}
    fmt.Println("\n", err3)
    fmt.Println("Is CacheExpired?", errors.Is(err3, ErrCacheExpired)) // true
}
```
</details>

### Ejercicio 2: Manejar sql.ErrNoRows correctamente

```go
// Implementa un UserRepository con estos metodos:
// - GetByID(id int) (*User, error) — ErrNotFound si no existe
// - FindByEmail(email string) (*User, error) — nil, nil si no existe
// - MustGetByID(id int) *User — panic si no existe
//
// En cada caso, maneja sql.ErrNoRows de forma diferente y explica por que.
// (Simula la DB con un map en lugar de SQL real)

type User struct {
    ID    int
    Name  string
    Email string
}

type UserRepository struct {
    users map[int]*User
}
```

<details>
<summary>Solucion</summary>

```go
import "database/sql"

var ErrNotFound = errors.New("not found")

type UserRepository struct {
    users map[int]*User
}

func NewUserRepository() *UserRepository {
    return &UserRepository{
        users: map[int]*User{
            1: {ID: 1, Name: "Alice", Email: "alice@example.com"},
            2: {ID: 2, Name: "Bob", Email: "bob@example.com"},
        },
    }
}

// simulateQuery simula db.QueryRow().Scan()
func (r *UserRepository) simulateQuery(id int) (*User, error) {
    u, ok := r.users[id]
    if !ok {
        return nil, sql.ErrNoRows // simular lo que devolveria la DB
    }
    return u, nil
}

// GetByID — ErrNotFound si no existe (error del dominio)
// Porque: el caller necesita saber "no existe" vs "error de DB"
func (r *UserRepository) GetByID(id int) (*User, error) {
    u, err := r.simulateQuery(id)
    if errors.Is(err, sql.ErrNoRows) {
        return nil, fmt.Errorf("user %d: %w", id, ErrNotFound)
    }
    if err != nil {
        return nil, fmt.Errorf("get user %d: %w", id, err)
    }
    return u, nil
}

// FindByEmail — nil, nil si no existe (ausencia no es error)
// Porque: buscar un email que no existe es un resultado valido, no un fallo
func (r *UserRepository) FindByEmail(email string) (*User, error) {
    for _, u := range r.users {
        if u.Email == email {
            return u, nil
        }
    }
    return nil, nil // no encontrado, pero no es error
}

// MustGetByID — panic si no existe (solo para inicializacion)
// Porque: se usa en contextos donde el usuario DEBE existir
func (r *UserRepository) MustGetByID(id int) *User {
    u, err := r.GetByID(id)
    if err != nil {
        panic(fmt.Sprintf("MustGetByID(%d): %v", id, err))
    }
    return u
}
```
</details>

### Ejercicio 3: Builder de errores con interfaces opcionales

```go
// Extiende el InfraErrorBuilder del programa principal para que:
// 1. Soporte un metodo Transient() que marca el error como temporal
// 2. Soporte un metodo WithMetadata(key, value string) para agregar
//    pares clave-valor arbitrarios
// 3. InfraError implemente fmt.Formatter para mostrar metadata
//    cuando se usa %+v (verbose)
```

<details>
<summary>Solucion</summary>

```go
type InfraError struct {
    Kind      error
    Service   string
    Operation string
    Detail    string
    Cause     error
    retry     bool
    retryWait time.Duration
    status    int
    transient bool
    metadata  map[string]string
}

func (e *InfraError) Format(f fmt.State, verb rune) {
    switch verb {
    case 'v':
        if f.Flag('+') {
            // Verbose: incluir metadata
            fmt.Fprintf(f, "%s", e.Error())
            if len(e.metadata) > 0 {
                fmt.Fprintf(f, "\n  metadata:")
                for k, v := range e.metadata {
                    fmt.Fprintf(f, "\n    %s=%s", k, v)
                }
            }
            if e.transient {
                fmt.Fprintf(f, "\n  (transient)")
            }
        } else {
            fmt.Fprint(f, e.Error())
        }
    case 's':
        fmt.Fprint(f, e.Error())
    }
}

func (b *InfraErrorBuilder) Transient() *InfraErrorBuilder {
    b.err.transient = true
    return b
}

func (b *InfraErrorBuilder) WithMetadata(key, value string) *InfraErrorBuilder {
    if b.err.metadata == nil {
        b.err.metadata = make(map[string]string)
    }
    b.err.metadata[key] = value
    return b
}

// Uso:
err := NewInfraError(ErrTimeout).
    Service("redis").Op("GET").
    Detail("connection pool exhausted").
    Transient().
    WithMetadata("pool_size", "10").
    WithMetadata("active_conns", "10").
    Retriable(5 * time.Second).
    Build()

fmt.Printf("%v\n", err)   // mensaje simple
fmt.Printf("%+v\n", err)  // con metadata y flags
```
</details>

---

## 13. Resumen Visual

```
┌──────────────────────────────────────────────────────────────────────────┐
│  SENTINEL ERRORS y TIPOS DE ERROR — Resumen                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SENTINEL ERRORS (var Err* = errors.New(...)):                           │
│  ├─ Valores predefinidos con identidad unica                            │
│  ├─ Comparar con errors.Is(err, sentinel) — recorre Unwrap()           │
│  ├─ Responden: ¿QUE paso? (not found, timeout, permission)             │
│  ├─ Convencion: Err + Condicion (ErrNotFound, ErrTimeout)              │
│  ├─ Contrato publico: callers dependen de ellos — no cambiarlos        │
│  └─ Alternativa: const Error string (inmutable, pero matchea por valor)│
│                                                                          │
│  STDLIB SENTINELS MAS IMPORTANTES:                                       │
│  ├─ io.EOF — fin de stream (condicion normal, no error)                │
│  ├─ os.ErrNotExist — recurso no existe (ENOENT)                        │
│  ├─ os.ErrPermission — sin permisos (EACCES)                           │
│  ├─ context.Canceled — contexto cancelado                               │
│  ├─ context.DeadlineExceeded — timeout del contexto                     │
│  └─ sql.ErrNoRows — query sin resultados                               │
│                                                                          │
│  TIPOS DE ERROR (structs con Error() + Unwrap()):                        │
│  ├─ Llevan DATOS que el caller puede inspeccionar                       │
│  ├─ Extraer con errors.As(err, &typedErr)                               │
│  ├─ Responden: ¿QUE paso Y con que detalles?                           │
│  └─ stdlib: PathError, OpError, DNSError, SyntaxError, NumError        │
│                                                                          │
│  COMBINACION (patron recomendado):                                       │
│  ├─ Tipo con sentinel en Unwrap() → ambos mecanismos                   │
│  ├─ errors.Is(err, sentinel) funciona a traves del tipo                │
│  ├─ errors.As(err, &tipo) extrae los datos                             │
│  ├─ Multi-unwrap (Go 1.20+): Unwrap() []error para sentinel + cause   │
│  └─ Builder pattern para construir errores ricos                        │
│                                                                          │
│  INTERFACES OPCIONALES:                                                  │
│  ├─ net.Error: Timeout() bool                                           │
│  ├─ Custom: Retriable, HTTPStatusCode, Categorizable                   │
│  └─ Verificar con errors.As(err, &interfaz)                            │
│                                                                          │
│  ANTIPATRONES:                                                           │
│  ├─ Comparar por string (err.Error() == "...")                          │
│  ├─ Crear sentinels dentro de funciones (identidad diferente)           │
│  ├─ Exportar errores internos (detalles de implementacion)              │
│  ├─ Type assertion directa en lugar de errors.As                        │
│  └─ Sentinel mutable (var que se puede reasignar)                       │
│                                                                          │
│  vs C: errno (int global, no type-safe, race conditions)                │
│  vs Rust: enum exhaustivo + match (compilador verifica todos los casos) │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C07/S02 - Patrones Avanzados, T01 - Errgroup — golang.org/x/sync/errgroup, goroutines con propagacion de errores
