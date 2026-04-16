# Error como Valor — El Tipo error (Interfaz), errors.New, fmt.Errorf, Por Que No Excepciones

## 1. Introduccion

Go toma una decision radical respecto al manejo de errores: **no tiene excepciones**. No hay `try/catch/finally`, no hay `throw`, no hay stack unwinding. En su lugar, los errores son **valores ordinarios** que las funciones devuelven como cualquier otro resultado. Un error en Go es simplemente un valor que implementa la interfaz `error` — una interfaz con un solo metodo: `Error() string`.

Esta decision no fue accidental ni por limitacion — fue una eleccion filosofica deliberada del equipo de Go (Rob Pike, Ken Thompson, Robert Griesemer). La justificacion es que las excepciones crean **flujo de control invisible**: cuando una funcion puede lanzar una excepcion, el caller no sabe mirando la firma si la funcion puede fallar, de que formas puede fallar, ni que excepciones puede propagar. El error se convierte en un camino oculto que rompe el flujo normal del programa.

En Go, si una funcion puede fallar, su firma lo declara: `func Open(name string) (*File, error)`. El caller ve inmediatamente que esta funcion puede devolver un error. El error es un valor de retorno que debes manejar (o explicitamente ignorar). No hay magia, no hay caminos ocultos, no hay stack unwinding que destruya rendimiento.

Para SysAdmin/DevOps, esta filosofia es particularmente valiosa: en sistemas de infraestructura, los errores son la norma (red caida, disco lleno, servicio no responde, timeout, permiso denegado). Tratar errores como valores que fluyen por el programa te permite inspeccionarlos, enriquecerlos con contexto, decidir reintentar, logearlos, agregarlos — exactamente como haces con cualquier otro dato.

```
┌──────────────────────────────────────────────────────────────────────────┐
│           ERROR COMO VALOR — La Filosofia de Go                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Java/Python/C++:                Go:                                     │
│  ─────────────────               ───                                     │
│                                                                          │
│  try {                           result, err := operation()              │
│      result = operation()        if err != nil {                         │
│  } catch (IOException e) {           // manejar error                    │
│      // manejar IO               }                                      │
│  } catch (TimeoutEx e) {         // usar result                          │
│      // manejar timeout                                                  │
│  } catch (Exception e) {                                                 │
│      // manejar generico         Ventajas Go:                            │
│  } finally {                     ├─ El error es visible en la firma      │
│      // cleanup                  ├─ No hay flujo de control oculto       │
│  }                               ├─ No hay stack unwinding               │
│                                  ├─ El error es un valor — lo puedes     │
│  Problemas:                      │  inspeccionar, enriquecer, agregar    │
│  ├─ Flujo oculto (throw)        ├─ Rendimiento predecible               │
│  ├─ ¿Que excepciones puede      └─ Fuerza al programador a pensar       │
│  │  lanzar f()? No sabes.          en cada punto de fallo                │
│  ├─ Stack unwinding costoso                                              │
│  ├─ catch generico oculta bugs   Desventajas Go:                         │
│  └─ Excepciones checked          ├─ Verbose (if err != nil repetido)     │
│     (Java) = boilerplate         ├─ Facil de ignorar con _ = err         │
│     masivo                       └─ No hay ? operator (como Rust)        │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. La Interfaz error — La Definicion Mas Simple

La interfaz `error` es una de las mas simples de toda la stdlib de Go. Esta definida en el paquete builtin:

```go
// Definicion real en el runtime de Go (builtin/builtin.go):
type error interface {
    Error() string
}
```

Eso es todo. **Un solo metodo**. Cualquier tipo que tenga un metodo `Error() string` es un error en Go. Esta simplicidad es intencional y poderosa:

```go
package main

import "fmt"

// Cualquier tipo puede ser un error — solo necesita Error() string

// Un string puede ser un error
type SimpleError string

func (e SimpleError) Error() string {
    return string(e)
}

// Un struct puede ser un error
type DetailedError struct {
    Code    int
    Message string
    Op      string
}

func (e *DetailedError) Error() string {
    return fmt.Sprintf("[%d] %s: %s", e.Code, e.Op, e.Message)
}

// Un int puede ser un error (poco comun, pero valido)
type ErrCode int

func (e ErrCode) Error() string {
    return fmt.Sprintf("error code: %d", int(e))
}

func main() {
    // Todos estos son "error" — satisfacen la interfaz implicitamente
    var err1 error = SimpleError("connection refused")
    var err2 error = &DetailedError{Code: 404, Message: "not found", Op: "GET /api/users"}
    var err3 error = ErrCode(503)

    fmt.Println(err1) // connection refused
    fmt.Println(err2) // [404] GET /api/users: not found
    fmt.Println(err3) // error code: 503
}
```

### Por que un solo metodo

La decision de tener solo `Error() string` viene directamente del principio de interfaces pequeñas (C06/S02/T01):

| Alternativa considerada | Por que fue rechazada |
|------------------------|----------------------|
| `Error() string` + `Code() int` | No todos los errores tienen codigo numerico |
| `Error() string` + `Stack() string` | Stack traces son caros y no siempre necesarios |
| `Error() string` + `Unwrap() error` | Se agrego como convencion en Go 1.13, no como interfaz obligatoria |
| `Error() string` + `Is(target error) bool` | Idem — convencion opcional, no obligatoria |

La filosofia: **la interfaz base es minima; las capacidades adicionales se agregan via interfaces opcionales**. Esto permite que `errors.Is()` y `errors.As()` funcionen por convencion sin romper la interfaz base.

---

## 3. errors.New — Crear Errores Simples

El paquete `errors` proporciona la forma mas basica de crear un error:

```go
package main

import (
    "errors"
    "fmt"
)

func main() {
    // errors.New crea un error con un mensaje de texto fijo
    err := errors.New("connection refused")
    fmt.Println(err)        // connection refused
    fmt.Println(err.Error()) // connection refused (mismo resultado)

    // Cada llamada a errors.New crea un error DISTINTO
    // (incluso con el mismo mensaje)
    err1 := errors.New("timeout")
    err2 := errors.New("timeout")
    fmt.Println(err1 == err2) // false — son instancias diferentes
}
```

### Implementacion interna de errors.New

```go
// Esto es lo que errors.New hace internamente (simplificado):
// Fuente: src/errors/errors.go

package errors

// New devuelve un error que formatea como el texto dado.
// Cada llamada a New devuelve un valor de error distinto
// incluso si el texto es identico.
func New(text string) error {
    return &errorString{text}
}

// errorString es una implementacion trivial de error.
type errorString struct {
    s string
}

func (e *errorString) Error() string {
    return e.s
}
```

Notas clave:
- `New` devuelve un **puntero** (`&errorString`), no un valor
- Esto asegura que cada llamada crea un error unico (comparacion por identidad, no por texto)
- `errorString` no se exporta — es un detalle de implementacion

### Cuando usar errors.New

```go
// ✓ Bueno: errores simples con mensaje fijo
var ErrNotFound = errors.New("not found")
var ErrTimeout = errors.New("operation timed out")
var ErrPermission = errors.New("permission denied")

// ✗ Malo: errores con datos variables — usa fmt.Errorf o tipos custom
err := errors.New("user " + name + " not found") // ← no hagas esto
err = fmt.Errorf("user %s not found", name)       // ← usa esto
```

---

## 4. fmt.Errorf — Errores con Formato

`fmt.Errorf` es como `fmt.Sprintf` pero devuelve un `error` en lugar de un `string`:

```go
package main

import (
    "fmt"
    "os"
)

func readConfig(path string) ([]byte, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        // fmt.Errorf agrega contexto al error
        return nil, fmt.Errorf("readConfig(%s): %v", path, err)
    }
    return data, nil
}

func loadApp() error {
    data, err := readConfig("/etc/myapp/config.yaml")
    if err != nil {
        // Otro nivel de contexto
        return fmt.Errorf("loadApp: %v", err)
    }
    _ = data
    return nil
}

func main() {
    err := loadApp()
    if err != nil {
        fmt.Println(err)
        // loadApp: readConfig(/etc/myapp/config.yaml): open /etc/myapp/config.yaml: no such file or directory
    }
}
```

### Verbos de formato para errores

| Verbo | Comportamiento | Uso |
|-------|---------------|-----|
| `%v` | Llama `err.Error()` | Formato por defecto |
| `%s` | Llama `err.Error()` | Identico a `%v` para errores |
| `%q` | `err.Error()` con comillas | Debugging — muestra el string entrecomillado |
| `%+v` | Depende del tipo | Algunos tipos custom muestran stack traces |
| **`%w`** | **Wrap — preserva el error original** | **Go 1.13+** — permite `errors.Is/As` |

### %w — Wrapping de errores (Go 1.13+)

La diferencia entre `%v` y `%w` es critica:

```go
package main

import (
    "errors"
    "fmt"
    "os"
)

var ErrConfig = errors.New("configuration error")

func loadConfig(path string) error {
    _, err := os.ReadFile(path)
    if err != nil {
        // %v — PIERDE el error original (solo copia el texto)
        return fmt.Errorf("config: %v", err)
    }
    return nil
}

func loadConfigWrapped(path string) error {
    _, err := os.ReadFile(path)
    if err != nil {
        // %w — PRESERVA el error original (envuelve, no copia)
        return fmt.Errorf("config: %w", err)
    }
    return nil
}

func main() {
    // Con %v — el error original se pierde
    err := loadConfig("/nonexistent")
    fmt.Println(errors.Is(err, os.ErrNotExist)) // false — ¡el original se perdio!

    // Con %w — el error original se preserva
    err = loadConfigWrapped("/nonexistent")
    fmt.Println(errors.Is(err, os.ErrNotExist)) // true — errors.Is desenvuelve la cadena
}
```

### %w en detalle

```go
// fmt.Errorf con %w devuelve un tipo que implementa Unwrap():
//
// type wrapError struct {
//     msg string
//     err error  // el error original
// }
// func (e *wrapError) Error() string { return e.msg }
// func (e *wrapError) Unwrap() error { return e.err }

// Puedes usar %w multiples veces (Go 1.20+):
err := fmt.Errorf("op failed: %w and %w", err1, err2)
// El error resultante implementa Unwrap() []error (multi-unwrap)
```

---

## 5. Por Que Go No Tiene Excepciones

El equipo de Go documento explicitamente su decision. Estas son las razones:

### 5.1 Flujo de control invisible

```go
// En lenguajes con excepciones, CUALQUIER funcion puede fallar silenciosamente:

// Java:
// String readFile(String path) throws IOException {
//     return new String(Files.readAllBytes(Paths.get(path)));
// }
//
// void process() {
//     String data = readFile("/etc/config");  // ← puede lanzar IOException
//     String parsed = parse(data);             // ← puede lanzar ParseException
//     save(parsed);                            // ← puede lanzar SQLException
//     // ¿Que pasa si parse() lanza? save() no se ejecuta.
//     // ¿Que pasa si save() lanza? El estado queda inconsistente.
//     // El flujo de control tiene caminos OCULTOS en cada linea.
// }

// Go: el flujo es EXPLICITO
func process() error {
    data, err := readFile("/etc/config")
    if err != nil {
        return fmt.Errorf("read: %w", err)
    }
    parsed, err := parse(data)
    if err != nil {
        return fmt.Errorf("parse: %w", err)
    }
    if err := save(parsed); err != nil {
        return fmt.Errorf("save: %w", err)
    }
    return nil
    // Cada punto de fallo es VISIBLE. No hay caminos ocultos.
}
```

### 5.2 Stack unwinding es costoso

```
Excepciones (Java/C++/Python):
┌────────────────────────────────┐
│  main()                        │
│    └─ process()                │
│        └─ save()               │
│            └─ dbQuery()        │
│                └─ THROW! ──────┤──→ Stack unwinding:
│                                │    1. Destruir frame dbQuery
│                                │    2. Destruir frame save
│                                │    3. Destruir frame process
│                                │    4. Buscar catch en main
│                                │    5. Capturar stack trace
│                                │    Costo: microsegundos a milisegundos
└────────────────────────────────┘

Go (error como valor):
┌────────────────────────────────┐
│  main()                        │
│    └─ process()                │
│        └─ save()               │
│            └─ dbQuery()        │
│                └─ return err ──┤──→ Return normal:
│            if err != nil ──────┤    1. Devolver (nil, err) — 1 instruccion
│                return err      │    2. Caller verifica — 1 comparacion
│        if err != nil ──────────┤    3. Repeat up
│            return err          │    Costo: nanosegundos
└────────────────────────────────┘

Go es ordenes de magnitud mas rapido en el "error path"
porque no hay stack unwinding — es un return normal.
```

### 5.3 Excepciones checked (Java) vs errores Go

```
Java checked exceptions:
  void process() throws IOException, ParseException, SQLException {
      // El caller DEBE hacer try/catch o throws
      // Problema: la lista de excepciones crece con cada capa
      // Solucion comun: throws Exception (mata el proposito)
  }

Go:
  func process() error {
      // Un solo tipo: error
      // El caller verifica con if err != nil
      // Si quiere saber el tipo: errors.Is, errors.As, type switch
      // No hay "exception list" que crece
  }
```

### 5.4 Cita de Rob Pike

> "Errors are values. Values can be programmed, and since errors are values, errors can be programmed. [...] Use the language to simplify your error handling."
> — Rob Pike, [Errors are values](https://go.dev/blog/errors-are-values) (2015)

### 5.5 Go SI tiene panic/recover — pero no son excepciones

```go
// panic es para situaciones IRRECUPERABLES — bugs del programador,
// no errores esperados del runtime

func mustParseInt(s string) int {
    n, err := strconv.Atoi(s)
    if err != nil {
        panic(fmt.Sprintf("mustParseInt(%q): %v", s, err))
        // panic detiene la ejecucion normal
        // Ejecuta funciones defer en orden inverso
        // Si no hay recover, termina el programa
    }
    return n
}

// recover captura un panic — pero NO es try/catch
func safeCall(fn func()) (panicked bool) {
    defer func() {
        if r := recover(); r != nil {
            fmt.Println("Recovered from panic:", r)
            panicked = true
        }
    }()
    fn()
    return false
}

// REGLA: panic para bugs del programador, error para fallos operacionales
//
// ✓ panic: index out of range, nil dereference, impossible state
// ✓ error: file not found, network timeout, invalid input
//
// Nunca uses panic como control de flujo normal.
// Nunca uses panic para errores que el caller deberia manejar.
```

---

## 6. Patrones de Creacion de Errores

### 6.1 errors.New — errores constantes (sentinel)

```go
// Para errores que se comparan por identidad (sentinel errors)
var (
    ErrNotFound     = errors.New("resource not found")
    ErrUnauthorized = errors.New("unauthorized")
    ErrTimeout      = errors.New("operation timed out")
    ErrConflict     = errors.New("resource conflict")
)

func getUser(id int) (*User, error) {
    if id <= 0 {
        return nil, ErrNotFound
    }
    // ...
    return &User{}, nil
}

// El caller compara con el sentinel
user, err := getUser(-1)
if errors.Is(err, ErrNotFound) {
    // manejar 404
}
```

### 6.2 fmt.Errorf — errores con contexto dinamico

```go
// Para errores que necesitan datos variables
func connectDB(host string, port int) error {
    // ... intento de conexion ...
    return fmt.Errorf("connect to %s:%d: connection refused", host, port)
}

// Con wrapping (preserva el error original):
func initDatabase(host string, port int) error {
    if err := connectDB(host, port); err != nil {
        return fmt.Errorf("initDatabase: %w", err)
    }
    return nil
}
```

### 6.3 Tipos de error custom — errores con datos estructurados

```go
// Para errores que el caller necesita inspeccionar (no solo leer el mensaje)
type NetworkError struct {
    Op      string        // operacion que fallo
    Host    string        // host destino
    Port    int           // puerto
    Cause   error         // error subyacente
    Timeout bool          // true si fue timeout
    Retries int           // intentos realizados
}

func (e *NetworkError) Error() string {
    msg := fmt.Sprintf("%s %s:%d", e.Op, e.Host, e.Port)
    if e.Timeout {
        msg += " (timeout)"
    }
    if e.Retries > 0 {
        msg += fmt.Sprintf(" after %d retries", e.Retries)
    }
    if e.Cause != nil {
        msg += ": " + e.Cause.Error()
    }
    return msg
}

// Unwrap permite que errors.Is/As busquen en la cadena
func (e *NetworkError) Unwrap() error {
    return e.Cause
}

// Uso
func callAPI(host string) error {
    return &NetworkError{
        Op:      "POST",
        Host:    host,
        Port:    443,
        Cause:   errors.New("TLS handshake failed"),
        Timeout: false,
        Retries: 3,
    }
}
```

### 6.4 Funcion constructora para errores (patron idiomatico)

```go
// Cuando un tipo de error se crea frecuentemente,
// usa una funcion constructora para simplificar

type ValidationError struct {
    Field   string
    Value   interface{}
    Message string
}

func (e *ValidationError) Error() string {
    return fmt.Sprintf("validation error: field %q (value: %v): %s",
        e.Field, e.Value, e.Message)
}

// Constructor que simplifica la creacion
func NewValidationError(field string, value interface{}, format string, args ...interface{}) *ValidationError {
    return &ValidationError{
        Field:   field,
        Value:   value,
        Message: fmt.Sprintf(format, args...),
    }
}

// Uso limpio
func validatePort(port int) error {
    if port < 1 || port > 65535 {
        return NewValidationError("port", port, "must be between 1 and 65535")
    }
    return nil
}
```

---

## 7. El Contrato de (result, error)

Go tiene una **convencion universal** para devolver errores: el error es siempre el **ultimo valor de retorno**.

```go
// Patron estandar: (resultado, error)
func Open(name string) (*File, error)
func Read(p []byte) (n int, error)
func ParseInt(s string, base int, bitSize int) (int64, error)
func Get(url string) (*Response, error)

// Solo error (cuando no hay resultado):
func Close() error
func Remove(name string) error
func MkdirAll(path string, perm FileMode) error

// NUNCA: el error primero (rompe la convencion)
// func Open(name string) (error, *File)  // ✗ NO

// NUNCA: error en medio de otros retornos
// func Parse(s string) (int, error, bool)  // ✗ NO
```

### Contrato implicito del retorno

```go
// CONTRATO:
// Si err == nil → el resultado es valido y se puede usar
// Si err != nil → el resultado NO debe usarse (puede ser zero value o parcial)

func parseConfig(data []byte) (*Config, error) {
    if len(data) == 0 {
        return nil, errors.New("empty data") // nil result + error
    }

    cfg := &Config{}
    // ... parse ...

    if cfg.Port == 0 {
        return nil, fmt.Errorf("missing port") // nil result + error
    }

    return cfg, nil // valid result + nil error
}

// El caller DEBE respetar el contrato:
cfg, err := parseConfig(data)
if err != nil {
    // NO usar cfg aqui — puede ser nil o invalido
    return err
}
// Solo aqui es seguro usar cfg
fmt.Println(cfg.Port)
```

### Excepciones al contrato

```go
// Algunas funciones de la stdlib devuelven resultados PARCIALES con error:
// io.Reader.Read puede devolver n > 0 Y err != nil

n, err := reader.Read(buf)
// n puede ser > 0 incluso si err != nil
// Debes procesar los n bytes ANTES de verificar err
if n > 0 {
    process(buf[:n]) // procesar datos parciales
}
if err != nil {
    if err == io.EOF {
        break // fin normal
    }
    return err // error real
}

// bufio.Scanner.Scan es otro ejemplo:
scanner := bufio.NewScanner(reader)
for scanner.Scan() {
    line := scanner.Text() // datos parciales disponibles
}
if err := scanner.Err(); err != nil {
    // error despues de que el scan termino
}
```

---

## 8. Multiples Errores y Patrones de Agregacion

### 8.1 Errores multiples con errors.Join (Go 1.20+)

```go
package main

import (
    "errors"
    "fmt"
)

func validateUser(name, email string, age int) error {
    var errs []error

    if name == "" {
        errs = append(errs, errors.New("name is required"))
    }
    if email == "" {
        errs = append(errs, errors.New("email is required"))
    }
    if age < 0 || age > 150 {
        errs = append(errs, fmt.Errorf("invalid age: %d", age))
    }

    // errors.Join combina multiples errores en uno
    // Si todos son nil, devuelve nil
    return errors.Join(errs...)
}

func main() {
    err := validateUser("", "", -5)
    if err != nil {
        fmt.Println("Validation errors:")
        fmt.Println(err)
        // name is required
        // email is required
        // invalid age: -5
    }

    // errors.Is funciona a traves de Join:
    nameErr := errors.New("name is required")
    combined := errors.Join(nameErr, errors.New("email is required"))
    fmt.Println(errors.Is(combined, nameErr)) // true
}
```

### 8.2 Patron pre-Go 1.20: error collector manual

```go
// Para versiones anteriores a Go 1.20
type MultiError struct {
    Errors []error
}

func (me *MultiError) Error() string {
    if len(me.Errors) == 0 {
        return ""
    }
    msgs := make([]string, len(me.Errors))
    for i, err := range me.Errors {
        msgs[i] = err.Error()
    }
    return strings.Join(msgs, "; ")
}

func (me *MultiError) Add(err error) {
    if err != nil {
        me.Errors = append(me.Errors, err)
    }
}

// ErrorOrNil devuelve nil si no hay errores (evita el nil-interface trap)
func (me *MultiError) ErrorOrNil() error {
    if len(me.Errors) == 0 {
        return nil // nil puro — no (*MultiError)(nil)
    }
    return me
}

func validateAll(services []string) error {
    errs := &MultiError{}
    for _, svc := range services {
        if err := validateService(svc); err != nil {
            errs.Add(fmt.Errorf("service %s: %w", svc, err))
        }
    }
    return errs.ErrorOrNil() // ← CLAVE: devolver nil puro si no hay errores
}
```

---

## 9. Error Values en la Stdlib — Patrones Reales

### 9.1 os package

```go
package main

import (
    "errors"
    "fmt"
    "os"
)

func main() {
    // os.Open devuelve *os.PathError (que implementa error)
    _, err := os.Open("/nonexistent/file")
    if err != nil {
        fmt.Printf("Type: %T\n", err) // *fs.PathError
        fmt.Println("Message:", err)    // open /nonexistent/file: no such file or directory

        // PathError es un tipo de error con datos estructurados:
        // type PathError struct {
        //     Op   string  // "open", "read", "write"
        //     Path string  // ruta del archivo
        //     Err  error   // error subyacente del OS
        // }

        // Inspeccionar usando errors.As:
        var pathErr *os.PathError
        if errors.As(err, &pathErr) {
            fmt.Println("Op:", pathErr.Op)     // open
            fmt.Println("Path:", pathErr.Path) // /nonexistent/file
            fmt.Println("Err:", pathErr.Err)   // no such file or directory
        }

        // Verificar el tipo de error subyacente:
        if errors.Is(err, os.ErrNotExist) {
            fmt.Println("File does not exist") // ← esta es la forma idiomatica
        }
    }

    // os.ErrNotExist, os.ErrExist, os.ErrPermission — sentinel errors
    // Definidos como: var ErrNotExist = errors.New("file does not exist")
    // PathError.Unwrap() devuelve el syscall error que coincide con estos sentinels
}
```

### 9.2 net package

```go
// net.Dial devuelve *net.OpError
conn, err := net.DialTimeout("tcp", "192.168.1.1:8080", 5*time.Second)
if err != nil {
    // net.OpError tiene: Op, Net, Source, Addr, Err
    var opErr *net.OpError
    if errors.As(err, &opErr) {
        fmt.Println("Op:", opErr.Op)       // dial
        fmt.Println("Net:", opErr.Net)     // tcp
        fmt.Println("Addr:", opErr.Addr)   // 192.168.1.1:8080

        // Verificar si fue timeout
        if opErr.Timeout() {
            fmt.Println("Connection timed out — retrying...")
        }

        // Verificar si la conexion fue rechazada
        var syscallErr *os.SyscallError
        if errors.As(err, &syscallErr) {
            fmt.Println("Syscall:", syscallErr.Syscall) // connect
        }
    }
}
```

### 9.3 strconv package

```go
// strconv.Atoi devuelve *strconv.NumError
n, err := strconv.Atoi("not-a-number")
if err != nil {
    // NumError tiene: Func, Num, Err
    var numErr *strconv.NumError
    if errors.As(err, &numErr) {
        fmt.Println("Func:", numErr.Func) // Atoi
        fmt.Println("Num:", numErr.Num)   // not-a-number
        fmt.Println("Err:", numErr.Err)   // invalid syntax
    }

    // Sentinel errors de strconv:
    if errors.Is(err, strconv.ErrSyntax) {
        fmt.Println("Invalid syntax")
    }
    if errors.Is(err, strconv.ErrRange) {
        fmt.Println("Out of range")
    }
}
```

### 9.4 Patron comun en la stdlib

```
┌──────────────────────────────────────────────────────────────────────┐
│  PATRON DE ERRORES EN LA STDLIB                                      │
│                                                                      │
│  Paquete devuelve *PkgError (tipo custom con datos)                  │
│  PkgError implementa:                                                │
│    - Error() string     → mensaje legible                            │
│    - Unwrap() error     → error subyacente (cadena)                  │
│    - Timeout() bool     → es timeout? (si aplica)                    │
│    - Temporary() bool   → es temporal? (deprecated en Go 1.18+)      │
│                                                                      │
│  Paquete exporta sentinel errors (var Err* = errors.New(...))        │
│  El caller usa:                                                      │
│    - errors.Is(err, os.ErrNotExist) → comparar con sentinel         │
│    - errors.As(err, &pathErr)       → extraer tipo concreto         │
│    - err.Error()                     → mensaje (logging)             │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 10. Error como Valor — "Errors are Values" (Rob Pike)

La frase "errors are values" significa que los errores se pueden programar como cualquier otro valor: almacenar en variables, pasar a funciones, agrupar en slices, usar en maps, transformar con funciones de orden superior.

### 10.1 Patron errWriter (del blog de Rob Pike)

```go
// Problema: multiples writes con verificacion de error repetitiva
func writeResponse(w io.Writer, headers map[string]string, body []byte) error {
    // Verbose y repetitivo:
    _, err := fmt.Fprintf(w, "HTTP/1.1 200 OK\r\n")
    if err != nil {
        return err
    }
    for k, v := range headers {
        _, err = fmt.Fprintf(w, "%s: %s\r\n", k, v)
        if err != nil {
            return err
        }
    }
    _, err = fmt.Fprintf(w, "\r\n")
    if err != nil {
        return err
    }
    _, err = w.Write(body)
    if err != nil {
        return err
    }
    return nil
}

// Solucion: errWriter — captura el primer error y ignora writes subsiguientes
type errWriter struct {
    w   io.Writer
    err error
}

func (ew *errWriter) write(format string, args ...interface{}) {
    if ew.err != nil {
        return // ya hubo error — no hacer nada
    }
    _, ew.err = fmt.Fprintf(ew.w, format, args...)
}

func (ew *errWriter) writeBytes(p []byte) {
    if ew.err != nil {
        return
    }
    _, ew.err = ew.w.Write(p)
}

// Ahora el codigo es limpio:
func writeResponseClean(w io.Writer, headers map[string]string, body []byte) error {
    ew := &errWriter{w: w}

    ew.write("HTTP/1.1 200 OK\r\n")
    for k, v := range headers {
        ew.write("%s: %s\r\n", k, v)
    }
    ew.write("\r\n")
    ew.writeBytes(body)

    return ew.err // verificar UNA vez al final
}
```

### 10.2 Error como maquina de estado

```go
// Los errores pueden acumular estado y cambiar de comportamiento
type RetryableError struct {
    Err       error
    Attempt   int
    MaxRetries int
    LastAt    time.Time
}

func (e *RetryableError) Error() string {
    return fmt.Sprintf("attempt %d/%d: %v", e.Attempt, e.MaxRetries, e.Err)
}

func (e *RetryableError) ShouldRetry() bool {
    return e.Attempt < e.MaxRetries
}

func (e *RetryableError) NextAttempt() *RetryableError {
    return &RetryableError{
        Err:        e.Err,
        Attempt:    e.Attempt + 1,
        MaxRetries: e.MaxRetries,
        LastAt:     time.Now(),
    }
}

func (e *RetryableError) Unwrap() error {
    return e.Err
}

// Uso: el error como valor programable
func callWithRetry(fn func() error, maxRetries int) error {
    retErr := &RetryableError{MaxRetries: maxRetries}

    for {
        err := fn()
        if err == nil {
            return nil
        }

        retErr = &RetryableError{
            Err:        err,
            Attempt:    retErr.Attempt + 1,
            MaxRetries: maxRetries,
            LastAt:     time.Now(),
        }

        if !retErr.ShouldRetry() {
            return retErr // devolver como valor — con toda la historia de reintentos
        }

        backoff := time.Duration(retErr.Attempt) * time.Second
        time.Sleep(backoff)
    }
}
```

### 10.3 Error en colecciones y maps

```go
// Errores como valores en un map — registro de fallos por servicio
type HealthReport struct {
    Results map[string]error    // servicio → error (nil si healthy)
    At      time.Time
}

func (hr *HealthReport) FailedServices() []string {
    var failed []string
    for svc, err := range hr.Results {
        if err != nil {
            failed = append(failed, svc)
        }
    }
    return failed
}

func (hr *HealthReport) AllHealthy() bool {
    for _, err := range hr.Results {
        if err != nil {
            return false
        }
    }
    return true
}

func (hr *HealthReport) Error() error {
    failed := hr.FailedServices()
    if len(failed) == 0 {
        return nil
    }
    return fmt.Errorf("%d services unhealthy: %s",
        len(failed), strings.Join(failed, ", "))
}
```

---

## 11. Antipatrones — Que NO Hacer con Errores

### 11.1 Ignorar errores silenciosamente

```go
// ✗ NUNCA: ignorar errores sin razon
result, _ := riskyOperation() // ← el _ ignora un error potencialmente critico

// ✓ Si realmente quieres ignorar, documenta por que
result, _ := riskyOperation() // error imposible: input is always valid (compile-time constant)

// ✓ O logea y continua
result, err := riskyOperation()
if err != nil {
    log.Printf("warning: riskyOperation failed (non-critical): %v", err)
    // continuar con resultado parcial o default
}
```

### 11.2 Comparar por string

```go
// ✗ NUNCA: comparar errores por mensaje de texto
if err.Error() == "connection refused" { // ← fragil, se rompe con cualquier cambio
    // ...
}

// ✓ Comparar con sentinel errors o errors.Is
if errors.Is(err, ErrConnectionRefused) {
    // ...
}

// ✓ O extraer tipo concreto con errors.As
var netErr *net.OpError
if errors.As(err, &netErr) {
    // acceder a campos del error
}
```

### 11.3 Log + return (duplicar el error)

```go
// ✗ MALO: logear Y devolver el error
func process() error {
    err := doSomething()
    if err != nil {
        log.Println("error:", err) // logea aqui
        return err                  // Y lo devuelve para que el caller TAMBIEN lo logee
        // Resultado: el mismo error aparece 5 veces en los logs
    }
    return nil
}

// ✓ CORRECTO: o logeas (si tu eres responsable) o devuelves (si el caller lo es)
func process() error {
    err := doSomething()
    if err != nil {
        return fmt.Errorf("process: %w", err) // agrega contexto y devuelve
    }
    return nil
}
// El caller (o main) decide cuando logear
```

### 11.4 Panic como control de flujo

```go
// ✗ NUNCA: usar panic para errores esperados
func getUser(id int) *User {
    user, err := db.FindUser(id)
    if err != nil {
        panic(err) // ← NO — esto no es un bug del programa, es un error operacional
    }
    return user
}

// ✓ CORRECTO: devolver error
func getUser(id int) (*User, error) {
    user, err := db.FindUser(id)
    if err != nil {
        return nil, fmt.Errorf("getUser(%d): %w", id, err)
    }
    return user, nil
}
```

### 11.5 Wrapping excesivo

```go
// ✗ MALO: agregar contexto sin informacion nueva
func a() error {
    return fmt.Errorf("a: %w", b())
}
func b() error {
    return fmt.Errorf("b: %w", c())
}
func c() error {
    return fmt.Errorf("c: %w", errors.New("disk full"))
}
// Resultado: "a: b: c: disk full" — mucho ruido, poco valor

// ✓ MEJOR: agregar contexto que ayuda al debugging
func saveConfig(path string, cfg *Config) error {
    data, err := json.Marshal(cfg)
    if err != nil {
        return fmt.Errorf("marshal config: %w", err)
    }
    if err := os.WriteFile(path, data, 0644); err != nil {
        return fmt.Errorf("write %s: %w", path, err) // el PATH es contexto util
    }
    return nil
}
```

---

## 12. Comparacion con C y Rust

### C — error handling manual con int returns

```c
#include <stdio.h>
#include <errno.h>
#include <string.h>

// C: funciones devuelven int (0 = exito, -1 = error)
// El error real esta en errno (variable global)

int read_config(const char* path, char* buf, size_t bufsize) {
    FILE* f = fopen(path, "r");
    if (f == NULL) {
        // errno esta seteado por fopen
        // pero errno puede ser sobreescrito por la siguiente llamada!
        return -1;
    }

    size_t n = fread(buf, 1, bufsize, f);
    if (ferror(f)) {
        fclose(f);    // ¿y si fclose tambien falla?
        return -1;
    }

    fclose(f);
    return (int)n;  // retorno positivo = exito
}

int main() {
    char buf[4096];
    int result = read_config("/etc/config", buf, sizeof(buf));
    if (result < 0) {
        // errno puede haber sido sobreescrito entre fopen y aqui
        fprintf(stderr, "Error: %s\n", strerror(errno));
        return 1;
    }
    printf("Read %d bytes\n", result);
    return 0;
}

// Problemas de C:
// 1. errno es global — race conditions en multithreading
// 2. errno se sobreescribe — debes capturarlo inmediatamente
// 3. No hay tipo "error" — es un int magico
// 4. Facil olvidar verificar retorno (mas que en Go)
// 5. No hay wrapping — debes formatear mensajes manualmente
// 6. El mismo canal (return value) mezcla resultado y error
```

### Rust — Result<T, E> y el operador ?

```rust
use std::fs;
use std::io;

// Rust: Result<T, E> separa exito y error en el sistema de tipos
// El compilador OBLIGA a manejar el Result

fn read_config(path: &str) -> Result<String, io::Error> {
    // ? propaga el error automaticamente
    // Equivale a: match fs::read_to_string(path) {
    //     Ok(s) => s,
    //     Err(e) => return Err(e),
    // }
    let content = fs::read_to_string(path)?;
    Ok(content)
}

// Multiples errores con Box<dyn Error> o custom enum
#[derive(Debug)]
enum AppError {
    Io(io::Error),
    Parse(serde_json::Error),
    Validation(String),
}

impl std::fmt::Display for AppError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            AppError::Io(e) => write!(f, "IO error: {}", e),
            AppError::Parse(e) => write!(f, "Parse error: {}", e),
            AppError::Validation(msg) => write!(f, "Validation: {}", msg),
        }
    }
}

impl std::error::Error for AppError {}

// From trait permite conversion automatica con ?
impl From<io::Error> for AppError {
    fn from(e: io::Error) -> Self {
        AppError::Io(e)
    }
}

fn load_config(path: &str) -> Result<Config, AppError> {
    let content = fs::read_to_string(path)?; // io::Error → AppError via From
    let cfg: Config = serde_json::from_str(&content)
        .map_err(AppError::Parse)?; // conversion manual
    if cfg.port == 0 {
        return Err(AppError::Validation("port required".into()));
    }
    Ok(cfg)
}
```

### Tabla comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ERROR HANDLING — C vs Go vs Rust                                        │
├──────────────────┬──────────────┬────────────────────┬───────────────────┤
│  Aspecto         │  C           │  Go                │  Rust              │
├──────────────────┼──────────────┼────────────────────┼───────────────────┤
│  Tipo de error   │  int/errno   │  error (interfaz)  │  Result<T,E>       │
│  Verificacion    │  Opcional    │  Convencional      │  Obligatoria       │
│  Propagacion     │  Manual      │  if err != nil     │  ? operator        │
│  Contexto        │  strerror()  │  fmt.Errorf("%w")  │  .map_err() /      │
│                  │              │                    │  From trait         │
│  Errores         │  errno.h     │  sentinel vars     │  enum variants     │
│  conocidos       │  macros      │  (errors.Is)       │  (match)           │
│  Thread safety   │  errno no    │  error es valor    │  Result es owned   │
│                  │  es safe     │  (safe)            │  (safe)            │
│  Multiples       │  No          │  errors.Join       │  Vec<Error>        │
│  errores         │              │  (Go 1.20+)        │                    │
│  Excepciones     │  longjmp     │  panic/recover     │  panic!/catch      │
│                  │  (raro, UB)  │  (raro, solo bugs) │  _unwind (raro)    │
│  Stack trace     │  No (sin     │  runtime.Stack()   │  RUST_BACKTRACE=1  │
│                  │  libunwind)  │  (explicito)       │  (automatico)      │
│  Overhead        │  Zero        │  ~Zero (return)    │  Zero              │
│  Filosofia       │  "check the  │  "errors are       │  "make illegal     │
│                  │   return"    │   values"          │   states            │
│                  │              │                    │   unrepresentable"  │
└──────────────────┴──────────────┴────────────────────┴───────────────────┘
```

---

## 13. Programa Completo: Infrastructure Config Loader con Error Handling

```go
// ============================================================================
// INFRASTRUCTURE CONFIG LOADER — Error como Valor
//
// Demuestra:
// - Tipos de error custom con datos estructurados
// - errors.New para sentinel errors
// - fmt.Errorf con %w para wrapping
// - Patron errWriter (errors are values)
// - errors.Join para validacion multiple (Go 1.20+)
// - Cadena de errores con Unwrap
// - errors.Is y errors.As para inspeccion
// - Antipatrones vs patrones correctos
//
// Compilar y ejecutar:
//   go run main.go
// ============================================================================

package main

import (
    "errors"
    "fmt"
    "strconv"
    "strings"
    "time"
)

// ============================================================================
// PARTE 1: Sentinel Errors — errores conocidos y comparables
// ============================================================================

var (
    ErrNotFound     = errors.New("not found")
    ErrInvalidPort  = errors.New("invalid port number")
    ErrEmptyHost    = errors.New("host cannot be empty")
    ErrMissingField = errors.New("required field missing")
    ErrParseConfig  = errors.New("configuration parse error")
)

// ============================================================================
// PARTE 2: Tipos de Error Custom
// ============================================================================

// ConfigError — error con contexto de configuracion
type ConfigError struct {
    File    string // archivo de configuracion
    Line    int    // linea donde ocurrio el error
    Field   string // campo que fallo
    Value   string // valor que se intento asignar
    Cause   error  // error subyacente
}

func (e *ConfigError) Error() string {
    var b strings.Builder
    if e.File != "" {
        fmt.Fprintf(&b, "%s", e.File)
        if e.Line > 0 {
            fmt.Fprintf(&b, ":%d", e.Line)
        }
        b.WriteString(": ")
    }
    if e.Field != "" {
        fmt.Fprintf(&b, "field %q", e.Field)
        if e.Value != "" {
            fmt.Fprintf(&b, " (value: %q)", e.Value)
        }
        b.WriteString(": ")
    }
    if e.Cause != nil {
        b.WriteString(e.Cause.Error())
    } else {
        b.WriteString("unknown error")
    }
    return b.String()
}

func (e *ConfigError) Unwrap() error {
    return e.Cause
}

// ValidationErrors — multiples errores de validacion
type ValidationErrors struct {
    Errors []error
}

func (ve *ValidationErrors) Error() string {
    msgs := make([]string, len(ve.Errors))
    for i, err := range ve.Errors {
        msgs[i] = fmt.Sprintf("  - %s", err.Error())
    }
    return fmt.Sprintf("validation failed (%d errors):\n%s",
        len(ve.Errors), strings.Join(msgs, "\n"))
}

func (ve *ValidationErrors) Add(err error) {
    if err != nil {
        ve.Errors = append(ve.Errors, err)
    }
}

// ErrorOrNil evita el nil-interface trap (T03 del capitulo anterior)
func (ve *ValidationErrors) ErrorOrNil() error {
    if len(ve.Errors) == 0 {
        return nil // nil puro
    }
    return ve
}

// Unwrap devuelve todos los errores (Go 1.20+ compatible)
func (ve *ValidationErrors) Unwrap() []error {
    return ve.Errors
}

// ============================================================================
// PARTE 3: Modelo de datos
// ============================================================================

// ServiceConfig representa la configuracion de un servicio
type ServiceConfig struct {
    Name     string
    Host     string
    Port     int
    Protocol string
    Timeout  time.Duration
    Retries  int
    Tags     []string
}

func (sc *ServiceConfig) String() string {
    return fmt.Sprintf("%s://%s:%d (timeout=%v, retries=%d, tags=%v)",
        sc.Protocol, sc.Host, sc.Port, sc.Timeout, sc.Retries, sc.Tags)
}

// InfraConfig contiene toda la configuracion de infraestructura
type InfraConfig struct {
    Environment string
    Services    []ServiceConfig
    LoadedAt    time.Time
    Source      string
}

// ============================================================================
// PARTE 4: Parser con errores detallados
// ============================================================================

// parseLine parsea una linea "key=value"
func parseLine(line string, lineNum int, file string) (string, string, error) {
    line = strings.TrimSpace(line)
    if line == "" || strings.HasPrefix(line, "#") {
        return "", "", nil // lineas vacias y comentarios — no es error
    }

    parts := strings.SplitN(line, "=", 2)
    if len(parts) != 2 {
        return "", "", &ConfigError{
            File:  file,
            Line:  lineNum,
            Value: line,
            Cause: fmt.Errorf("expected format 'key=value', got %q", line),
        }
    }

    key := strings.TrimSpace(parts[0])
    value := strings.TrimSpace(parts[1])

    if key == "" {
        return "", "", &ConfigError{
            File:  file,
            Line:  lineNum,
            Value: line,
            Cause: ErrMissingField,
        }
    }

    return key, value, nil
}

// parseServiceBlock parsea un bloque de configuracion de servicio
func parseServiceBlock(lines map[string]string, file string) (*ServiceConfig, error) {
    svc := &ServiceConfig{}
    ve := &ValidationErrors{}

    // Name (requerido)
    if name, ok := lines["name"]; ok && name != "" {
        svc.Name = name
    } else {
        ve.Add(&ConfigError{
            File:  file,
            Field: "name",
            Cause: ErrMissingField,
        })
    }

    // Host (requerido)
    if host, ok := lines["host"]; ok && host != "" {
        svc.Host = host
    } else {
        ve.Add(&ConfigError{
            File:  file,
            Field: "host",
            Cause: ErrEmptyHost,
        })
    }

    // Port (requerido, numerico, 1-65535)
    if portStr, ok := lines["port"]; ok {
        port, err := strconv.Atoi(portStr)
        if err != nil {
            ve.Add(&ConfigError{
                File:  file,
                Field: "port",
                Value: portStr,
                Cause: fmt.Errorf("%w: %v", ErrInvalidPort, err),
            })
        } else if port < 1 || port > 65535 {
            ve.Add(&ConfigError{
                File:  file,
                Field: "port",
                Value: portStr,
                Cause: fmt.Errorf("%w: must be 1-65535, got %d", ErrInvalidPort, port),
            })
        } else {
            svc.Port = port
        }
    } else {
        ve.Add(&ConfigError{
            File:  file,
            Field: "port",
            Cause: ErrMissingField,
        })
    }

    // Protocol (default: tcp)
    if proto, ok := lines["protocol"]; ok {
        switch proto {
        case "tcp", "udp", "http", "https", "grpc":
            svc.Protocol = proto
        default:
            ve.Add(&ConfigError{
                File:  file,
                Field: "protocol",
                Value: proto,
                Cause: fmt.Errorf("unsupported protocol: %s (valid: tcp, udp, http, https, grpc)", proto),
            })
        }
    } else {
        svc.Protocol = "tcp" // default
    }

    // Timeout (default: 30s)
    if timeoutStr, ok := lines["timeout"]; ok {
        dur, err := time.ParseDuration(timeoutStr)
        if err != nil {
            ve.Add(&ConfigError{
                File:  file,
                Field: "timeout",
                Value: timeoutStr,
                Cause: fmt.Errorf("invalid duration: %w", err),
            })
        } else {
            svc.Timeout = dur
        }
    } else {
        svc.Timeout = 30 * time.Second
    }

    // Retries (default: 3)
    if retriesStr, ok := lines["retries"]; ok {
        retries, err := strconv.Atoi(retriesStr)
        if err != nil {
            ve.Add(&ConfigError{
                File:  file,
                Field: "retries",
                Value: retriesStr,
                Cause: err,
            })
        } else if retries < 0 || retries > 10 {
            ve.Add(&ConfigError{
                File:  file,
                Field: "retries",
                Value: retriesStr,
                Cause: fmt.Errorf("must be 0-10, got %d", retries),
            })
        } else {
            svc.Retries = retries
        }
    } else {
        svc.Retries = 3
    }

    // Tags (opcional)
    if tagsStr, ok := lines["tags"]; ok && tagsStr != "" {
        svc.Tags = strings.Split(tagsStr, ",")
        for i := range svc.Tags {
            svc.Tags[i] = strings.TrimSpace(svc.Tags[i])
        }
    }

    return svc, ve.ErrorOrNil() // ← nil puro si no hay errores
}

// ============================================================================
// PARTE 5: Loader principal
// ============================================================================

// LoadConfig parsea configuracion de un string (simula lectura de archivo)
func LoadConfig(data string, filename string) (*InfraConfig, error) {
    if data == "" {
        return nil, &ConfigError{
            File:  filename,
            Cause: fmt.Errorf("empty configuration data"),
        }
    }

    config := &InfraConfig{
        Source:   filename,
        LoadedAt: time.Now(),
    }

    lines := strings.Split(data, "\n")
    var currentBlock map[string]string
    var services []ServiceConfig
    allErrors := &ValidationErrors{}

    for i, line := range lines {
        lineNum := i + 1
        line = strings.TrimSpace(line)

        // Lineas vacias y comentarios
        if line == "" || strings.HasPrefix(line, "#") {
            continue
        }

        // Inicio de bloque [service]
        if line == "[service]" {
            // Procesar bloque anterior si existe
            if currentBlock != nil {
                svc, err := parseServiceBlock(currentBlock, filename)
                if err != nil {
                    allErrors.Add(err)
                } else {
                    services = append(services, *svc)
                }
            }
            currentBlock = make(map[string]string)
            continue
        }

        // Directiva de entorno
        if strings.HasPrefix(line, "environment=") {
            config.Environment = strings.TrimPrefix(line, "environment=")
            continue
        }

        // Parsear key=value
        if currentBlock != nil {
            key, value, err := parseLine(line, lineNum, filename)
            if err != nil {
                allErrors.Add(err)
                continue
            }
            if key != "" {
                currentBlock[key] = value
            }
        }
    }

    // Procesar ultimo bloque
    if currentBlock != nil {
        svc, err := parseServiceBlock(currentBlock, filename)
        if err != nil {
            allErrors.Add(err)
        } else {
            services = append(services, *svc)
        }
    }

    config.Services = services

    // Validacion global
    if config.Environment == "" {
        allErrors.Add(&ConfigError{
            File:  filename,
            Field: "environment",
            Cause: ErrMissingField,
        })
    }

    if len(config.Services) == 0 && len(allErrors.Errors) == 0 {
        allErrors.Add(&ConfigError{
            File:  filename,
            Cause: errors.New("no services defined"),
        })
    }

    return config, allErrors.ErrorOrNil()
}

// ============================================================================
// PARTE 6: main — demostracion
// ============================================================================

func main() {
    fmt.Println("╔══════════════════════════════════════════════════════════╗")
    fmt.Println("║     Error como Valor — Infrastructure Config Loader     ║")
    fmt.Println("╚══════════════════════════════════════════════════════════╝")
    fmt.Println()

    // ─── Caso 1: Configuracion valida ───
    fmt.Println("=== Caso 1: Configuracion valida ===")
    fmt.Println()

    validConfig := `
environment=production

[service]
name=api-gateway
host=api.example.com
port=8080
protocol=https
timeout=10s
retries=5
tags=production, critical, public

[service]
name=database
host=db-master.internal
port=5432
protocol=tcp
timeout=5s
retries=3
tags=production, storage
`

    config, err := LoadConfig(validConfig, "infra.conf")
    if err != nil {
        fmt.Println("ERROR:", err)
    } else {
        fmt.Printf("  Environment: %s\n", config.Environment)
        fmt.Printf("  Loaded from: %s at %s\n", config.Source, config.LoadedAt.Format("15:04:05"))
        fmt.Printf("  Services: %d\n", len(config.Services))
        for _, svc := range config.Services {
            fmt.Printf("    - %s\n", &svc)
        }
    }

    // ─── Caso 2: Configuracion con errores ───
    fmt.Println()
    fmt.Println("=== Caso 2: Configuracion con errores multiples ===")
    fmt.Println()

    badConfig := `
[service]
name=broken-service
host=
port=99999
protocol=websocket
timeout=invalid
retries=50

[service]
port=abc
`

    config, err = LoadConfig(badConfig, "bad-infra.conf")
    if err != nil {
        fmt.Println("Errors found:")
        fmt.Println(err)
        fmt.Println()

        // Demostrar errors.Is a traves de la cadena
        fmt.Println("  Contiene ErrEmptyHost?", errors.Is(err, ErrEmptyHost))
        fmt.Println("  Contiene ErrInvalidPort?", errors.Is(err, ErrInvalidPort))
        fmt.Println("  Contiene ErrMissingField?", errors.Is(err, ErrMissingField))

        // Demostrar errors.As
        var cfgErr *ConfigError
        if errors.As(err, &cfgErr) {
            fmt.Printf("\n  Primer ConfigError encontrado:\n")
            fmt.Printf("    File:  %s\n", cfgErr.File)
            fmt.Printf("    Field: %s\n", cfgErr.Field)
            fmt.Printf("    Cause: %v\n", cfgErr.Cause)
        }
    }

    if config != nil && len(config.Services) > 0 {
        fmt.Printf("\n  Servicios parseados correctamente: %d\n", len(config.Services))
    }

    // ─── Caso 3: Sentinel errors ───
    fmt.Println()
    fmt.Println("=== Caso 3: Sentinel errors y comparacion ===")
    fmt.Println()

    err1 := ErrNotFound
    err2 := fmt.Errorf("user lookup: %w", ErrNotFound)
    err3 := fmt.Errorf("API call: %w", fmt.Errorf("database: %w", ErrNotFound))

    fmt.Println("  err1 (directo):", err1)
    fmt.Println("  err2 (1 wrap):", err2)
    fmt.Println("  err3 (2 wraps):", err3)
    fmt.Println()
    fmt.Println("  errors.Is(err1, ErrNotFound):", errors.Is(err1, ErrNotFound))
    fmt.Println("  errors.Is(err2, ErrNotFound):", errors.Is(err2, ErrNotFound))
    fmt.Println("  errors.Is(err3, ErrNotFound):", errors.Is(err3, ErrNotFound))

    // ─── Caso 4: Error como valor — errWriter pattern ───
    fmt.Println()
    fmt.Println("=== Caso 4: Error como valor (errWriter pattern) ===")
    fmt.Println()

    var b strings.Builder
    ew := &errWriter{w: &b}

    ew.write("Service Report\n")
    ew.write("==============\n")
    ew.write("Environment: %s\n", "production")
    ew.write("Timestamp: %s\n", time.Now().Format(time.RFC3339))
    ew.write("Services: %d\n", 2)

    if ew.err != nil {
        fmt.Println("  Write error:", ew.err)
    } else {
        fmt.Print("  ", strings.ReplaceAll(b.String(), "\n", "\n  "))
    }

    // ─── Caso 5: Configuracion vacia ───
    fmt.Println()
    fmt.Println("=== Caso 5: Error en configuracion vacia ===")
    fmt.Println()

    _, err = LoadConfig("", "empty.conf")
    if err != nil {
        fmt.Println("  Error:", err)

        var cfgErr *ConfigError
        if errors.As(err, &cfgErr) {
            fmt.Printf("  File: %s\n", cfgErr.File)
        }
    }

    fmt.Println()
    fmt.Println(strings.Repeat("─", 58))
    fmt.Println()
    fmt.Println("  FILOSOFIA: \"Errors are values\"")
    fmt.Println("  - error es una interfaz: Error() string")
    fmt.Println("  - errors.New para sentinels, fmt.Errorf para contexto")
    fmt.Println("  - %w preserva la cadena (Unwrap), %v la rompe")
    fmt.Println("  - errors.Is busca en la cadena, errors.As extrae tipo")
    fmt.Println("  - Errores se programan como cualquier valor")
    fmt.Println("  - No hay excepciones — flujo explicito y predecible")
    fmt.Println()
}

// ─── errWriter: patron "errors are values" ───

type errWriter struct {
    w   *strings.Builder
    err error
}

func (ew *errWriter) write(format string, args ...interface{}) {
    if ew.err != nil {
        return
    }
    _, ew.err = fmt.Fprintf(ew.w, format, args...)
}
```

**Salida esperada:**
```
╔══════════════════════════════════════════════════════════╗
║     Error como Valor — Infrastructure Config Loader     ║
╚══════════════════════════════════════════════════════════╝

=== Caso 1: Configuracion valida ===

  Environment: production
  Loaded from: infra.conf at 12:30:45
  Services: 2
    - https://api.example.com:8080 (timeout=10s, retries=5, tags=[production critical public])
    - tcp://db-master.internal:5432 (timeout=5s, retries=3, tags=[production storage])

=== Caso 2: Configuracion con errores multiples ===

Errors found:
validation failed (4 errors):
  - bad-infra.conf: field "host": host cannot be empty
  - bad-infra.conf: field "port" (value: "99999"): invalid port number: must be 1-65535, got 99999
  - bad-infra.conf: field "protocol" (value: "websocket"): unsupported protocol
  - bad-infra.conf: field "environment": required field missing

  Contiene ErrEmptyHost? true
  Contiene ErrInvalidPort? true
  Contiene ErrMissingField? true

=== Caso 3: Sentinel errors y comparacion ===

  errors.Is(err1, ErrNotFound): true
  errors.Is(err2, ErrNotFound): true
  errors.Is(err3, ErrNotFound): true

=== Caso 5: Error en configuracion vacia ===

  Error: empty.conf: empty configuration data
  File: empty.conf
```

---

## 14. Ejercicios

### Ejercicio 1: Crear un tipo de error para operaciones de red

```go
// Crea un tipo NetworkError que:
// 1. Tenga campos: Op (string), Host (string), Port (int), Cause (error), IsTimeout (bool)
// 2. Implemente error con un mensaje descriptivo
// 3. Implemente Unwrap() para la cadena de errores
// 4. Tenga un metodo Timeout() bool
// 5. Crea una funcion connect(host string, port int) error que devuelva
//    NetworkError cuando el host contiene "fail" o el puerto es 0
// 6. En main, usa errors.Is y errors.As para inspeccionar el error

func main() {
    err := connect("db-fail.internal", 5432)
    // debe poder hacer:
    // errors.Is(err, ErrConnectionRefused)
    // errors.As(err, &netErr) → acceder a netErr.Host, netErr.Port
}
```

<details>
<summary>Solucion</summary>

```go
package main

import (
    "errors"
    "fmt"
    "strings"
)

var ErrConnectionRefused = errors.New("connection refused")
var ErrInvalidAddress = errors.New("invalid address")

type NetworkError struct {
    Op        string
    Host      string
    Port      int
    Cause     error
    IsTimeout bool
}

func (e *NetworkError) Error() string {
    msg := fmt.Sprintf("%s %s:%d", e.Op, e.Host, e.Port)
    if e.IsTimeout {
        msg += " (timeout)"
    }
    if e.Cause != nil {
        msg += ": " + e.Cause.Error()
    }
    return msg
}

func (e *NetworkError) Unwrap() error {
    return e.Cause
}

func (e *NetworkError) Timeout() bool {
    return e.IsTimeout
}

func connect(host string, port int) error {
    if port == 0 {
        return &NetworkError{
            Op:    "connect",
            Host:  host,
            Port:  port,
            Cause: ErrInvalidAddress,
        }
    }
    if strings.Contains(host, "fail") {
        return &NetworkError{
            Op:    "connect",
            Host:  host,
            Port:  port,
            Cause: ErrConnectionRefused,
        }
    }
    return nil
}

func main() {
    err := connect("db-fail.internal", 5432)
    if err != nil {
        fmt.Println("Error:", err)
        fmt.Println("Is connection refused?", errors.Is(err, ErrConnectionRefused))

        var netErr *NetworkError
        if errors.As(err, &netErr) {
            fmt.Printf("Host: %s, Port: %d, Timeout: %v\n",
                netErr.Host, netErr.Port, netErr.Timeout())
        }
    }
}
```
</details>

### Ejercicio 2: Implementar el patron errWriter para un reporte

```go
// Implementa un ReportWriter que:
// 1. Acumule escrituras en un strings.Builder
// 2. Capture el primer error y ignore escrituras subsiguientes
// 3. Tenga metodos: Header(title), Line(key, value), Separator(), Result() (string, error)
// 4. Genera un reporte de health check usando el patron

// Ejemplo de uso:
// rw := NewReportWriter()
// rw.Header("Infrastructure Status")
// rw.Line("database", "healthy")
// rw.Line("cache", "degraded")
// rw.Separator()
// rw.Line("total", "2 services")
// report, err := rw.Result()
```

<details>
<summary>Solucion</summary>

```go
package main

import (
    "fmt"
    "strings"
)

type ReportWriter struct {
    b   strings.Builder
    err error
}

func NewReportWriter() *ReportWriter {
    return &ReportWriter{}
}

func (rw *ReportWriter) write(format string, args ...interface{}) {
    if rw.err != nil {
        return
    }
    _, rw.err = fmt.Fprintf(&rw.b, format, args...)
}

func (rw *ReportWriter) Header(title string) {
    rw.write("%s\n", title)
    rw.write("%s\n", strings.Repeat("=", len(title)))
}

func (rw *ReportWriter) Line(key, value string) {
    rw.write("  %-20s %s\n", key+":", value)
}

func (rw *ReportWriter) Separator() {
    rw.write("%s\n", strings.Repeat("-", 40))
}

func (rw *ReportWriter) Result() (string, error) {
    if rw.err != nil {
        return "", rw.err
    }
    return rw.b.String(), nil
}

func main() {
    rw := NewReportWriter()
    rw.Header("Infrastructure Status")
    rw.Line("database", "healthy")
    rw.Line("cache", "degraded")
    rw.Line("dns", "healthy")
    rw.Separator()
    rw.Line("total", "3 services")
    rw.Line("unhealthy", "0")

    report, err := rw.Result()
    if err != nil {
        fmt.Println("Error generating report:", err)
    } else {
        fmt.Println(report)
    }
}
```
</details>

### Ejercicio 3: Refactorizar codigo con antipatrones

```go
// Este codigo tiene MULTIPLES antipatrones de error handling.
// Identifica cada antipatron y corrigelo.

func processServers(servers []string) {
    for _, s := range servers {
        result, _ := checkServer(s)  // antipatron 1
        fmt.Println(result)

        if err := deploy(s); err != nil {
            log.Println("deploy failed:", err)     // antipatron 2
            return fmt.Errorf("deploy: " + err.Error()) // antipatron 3 y 4
        }

        if err := verify(s); err != nil {
            if err.Error() == "timeout" {  // antipatron 5
                retry(s)
            }
        }
    }
}
```

<details>
<summary>Solucion</summary>

```go
// Antipatrones identificados:
// 1. Ignorar error sin razon (result, _ := checkServer(s))
// 2. Log + return (duplica el error en los logs)
// 3. String concatenation en lugar de fmt.Errorf con %w
// 4. No usa %w — pierde el error original
// 5. Compara error por string en lugar de errors.Is

var ErrTimeout = errors.New("timeout")

func processServers(servers []string) error {
    for _, s := range servers {
        // Fix 1: manejar el error
        result, err := checkServer(s)
        if err != nil {
            return fmt.Errorf("check %s: %w", s, err)
        }
        fmt.Println(result)

        // Fix 2, 3, 4: solo devolver (no log+return), usar %w
        if err := deploy(s); err != nil {
            return fmt.Errorf("deploy %s: %w", s, err)
        }

        // Fix 5: usar errors.Is en lugar de comparar strings
        if err := verify(s); err != nil {
            if errors.Is(err, ErrTimeout) {
                retry(s)
            } else {
                return fmt.Errorf("verify %s: %w", s, err)
            }
        }
    }
    return nil
}
```
</details>

---

## 15. Resumen Visual

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ERROR COMO VALOR — Resumen                                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  La interfaz error:                                                      │
│  ┌────────────────────────────────────┐                                  │
│  │  type error interface {            │                                  │
│  │      Error() string               │                                  │
│  │  }                                 │                                  │
│  └────────────────────────────────────┘                                  │
│  Un metodo. Cualquier tipo la satisface. Punto.                          │
│                                                                          │
│  CREACION:                                                               │
│  ├─ errors.New("msg")         → sentinel error (comparar con Is)        │
│  ├─ fmt.Errorf("ctx: %v", e) → contexto sin preservar cadena           │
│  ├─ fmt.Errorf("ctx: %w", e) → contexto CON cadena (Go 1.13+)         │
│  ├─ &CustomError{...}        → error con datos estructurados            │
│  └─ errors.Join(e1, e2, ...) → multiples errores (Go 1.20+)            │
│                                                                          │
│  CONTRATO (result, error):                                               │
│  ├─ error es SIEMPRE el ultimo valor de retorno                         │
│  ├─ err == nil → resultado valido                                       │
│  ├─ err != nil → resultado NO debe usarse                               │
│  └─ Excepcion: io.Reader puede devolver n>0 con err!=nil                │
│                                                                          │
│  INSPECCION:                                                             │
│  ├─ errors.Is(err, target)  → busca en la cadena Unwrap()              │
│  ├─ errors.As(err, &target) → extrae tipo concreto de la cadena        │
│  └─ switch err.(type) { }   → type switch (T02 vacia del C06)          │
│                                                                          │
│  POR QUE NO EXCEPCIONES:                                                │
│  ├─ Flujo de control invisible (throw rompe el flujo normal)            │
│  ├─ Stack unwinding costoso (microsegundos vs nanosegundos)             │
│  ├─ Excepciones checked = boilerplate (Java throws list)                │
│  ├─ catch generico oculta bugs                                          │
│  └─ Go tiene panic/recover — pero solo para bugs, no errores           │
│                                                                          │
│  PATRONES:                                                               │
│  ├─ errWriter: acumula writes, verifica error una vez al final          │
│  ├─ ErrorOrNil(): evita el nil-interface trap al devolver               │
│  ├─ Sentinel errors: var Err* = errors.New("...") exportados           │
│  └─ Constructor: NewXxxError(params) para tipos custom                  │
│                                                                          │
│  ANTIPATRONES:                                                           │
│  ├─ Ignorar errores con _                                               │
│  ├─ Comparar por string (err.Error() == "msg")                          │
│  ├─ Log + return (duplica en logs)                                      │
│  ├─ panic como control de flujo                                         │
│  └─ Wrapping excesivo sin contexto nuevo                                │
│                                                                          │
│  vs C: errno global, no type-safe, race conditions                      │
│  vs Rust: Result<T,E> obligatorio, ? operator, compile-time checked    │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: T02 - El patron if err != nil — idiomatico, no repetitivo, named returns para cleanup
