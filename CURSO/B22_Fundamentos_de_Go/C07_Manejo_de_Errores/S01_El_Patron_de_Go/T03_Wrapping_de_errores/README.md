# Wrapping de Errores — fmt.Errorf con %w, errors.Is, errors.As, Cadena de Errores

## 1. Introduccion

Antes de Go 1.13 (septiembre 2019), cuando una funcion recibia un error y queria agregar contexto, solo podia **construir un string nuevo**: `fmt.Errorf("open config: %v", err)`. Esto creaba un error completamente nuevo — el error original se perdia. El caller no podia saber si el error subyacente era un `os.ErrNotExist`, un timeout, o cualquier otro tipo. La unica opcion era comparar por texto (`err.Error() == "file not found"`) — fragil, no mantenible.

Go 1.13 introdujo **error wrapping**: un mecanismo para envolver errores preservando la cadena completa. Con `fmt.Errorf("context: %w", err)`, el nuevo error contiene al original. Luego, `errors.Is()` y `errors.As()` pueden recorrer la cadena buscando un error especifico. Go 1.20 extendio esto a **multiples errores simultaneos** con `errors.Join` y `Unwrap() []error` (error trees).

El resultado es un sistema de errores que funciona como una **linked list** (o arbol desde Go 1.20): cada nodo agrega contexto, y las funciones de inspeccion recorren la cadena completa. Esto permite que funciones de alto nivel (HTTP handlers, CLI commands) examinen errores de bajo nivel (syscalls, DNS failures) sin que las capas intermedias pierdan informacion.

Para SysAdmin/DevOps, el wrapping crea un **stack trace semantico**: no solo "donde fallo" (como un stack trace tecnico) sino "que estabamos intentando hacer en cada nivel". `"deploy app-v2: pull image: registry auth: TLS handshake: certificate expired"` te dice exactamente la cadena de causalidad, desde la accion de alto nivel hasta el fallo tecnico especifico.

```
┌──────────────────────────────────────────────────────────────────────────┐
│          WRAPPING DE ERRORES — La Cadena                                 │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Funcion de bajo nivel:                                                  │
│  ┌────────────────────────────────────────┐                              │
│  │  return os.ErrPermission              │  error original (leaf)        │
│  └───────────────┬────────────────────────┘                              │
│                  │ %w                                                     │
│  Capa intermedia 1:                                                      │
│  ┌────────────────────────────────────────┐                              │
│  │  fmt.Errorf("read config: %w", err)   │  wrap nivel 1                │
│  └───────────────┬────────────────────────┘                              │
│                  │ %w                                                     │
│  Capa intermedia 2:                                                      │
│  ┌────────────────────────────────────────┐                              │
│  │  fmt.Errorf("init database: %w", err) │  wrap nivel 2                │
│  └───────────────┬────────────────────────┘                              │
│                  │ %w                                                     │
│  Capa de alto nivel:                                                     │
│  ┌────────────────────────────────────────┐                              │
│  │  fmt.Errorf("deploy v2: %w", err)     │  wrap nivel 3                │
│  └────────────────────────────────────────┘                              │
│                                                                          │
│  err.Error() → "deploy v2: init database: read config: permission denied"│
│                                                                          │
│  errors.Is(err, os.ErrPermission) → true (recorre TODA la cadena)       │
│  errors.As(err, &pathErr)         → true (encuentra *PathError)         │
│                                                                          │
│  La cadena se recorre via Unwrap():                                      │
│  err → Unwrap() → Unwrap() → Unwrap() → os.ErrPermission               │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. %v vs %w — La Diferencia Fundamental

La diferencia entre `%v` y `%w` en `fmt.Errorf` es la diferencia entre **anotar** y **envolver**.

### %v — Anotacion (pierde el original)

```go
package main

import (
    "errors"
    "fmt"
    "os"
)

func readConfig(path string) error {
    _, err := os.ReadFile(path)
    if err != nil {
        // %v: copia el TEXTO del error, pero descarta el error original
        return fmt.Errorf("read config: %v", err)
    }
    return nil
}

func main() {
    err := readConfig("/nonexistent")

    fmt.Println(err)
    // read config: open /nonexistent: no such file or directory

    // PERO: el error original se perdio
    fmt.Println(errors.Is(err, os.ErrNotExist))
    // false — la cadena esta rota

    var pathErr *os.PathError
    fmt.Println(errors.As(err, &pathErr))
    // false — el tipo original no esta en la cadena
}
```

### %w — Wrapping (preserva el original)

```go
func readConfigWrapped(path string) error {
    _, err := os.ReadFile(path)
    if err != nil {
        // %w: ENVUELVE el error original — la cadena se preserva
        return fmt.Errorf("read config: %w", err)
    }
    return nil
}

func main() {
    err := readConfigWrapped("/nonexistent")

    fmt.Println(err)
    // read config: open /nonexistent: no such file or directory
    // (el texto es IDENTICO a %v)

    // PERO: el error original esta en la cadena
    fmt.Println(errors.Is(err, os.ErrNotExist))
    // true — errors.Is recorre Unwrap() y encuentra os.ErrNotExist

    var pathErr *os.PathError
    fmt.Println(errors.As(err, &pathErr))
    // true — errors.As encuentra *os.PathError en la cadena
    fmt.Println("Path:", pathErr.Path)
    // Path: /nonexistent
}
```

### Que hace %w internamente

```go
// Cuando escribes:
err := fmt.Errorf("context: %w", originalErr)

// Go crea internamente un tipo asi:
// type wrapError struct {
//     msg string  // "context: <original.Error()>"
//     err error   // originalErr — referencia directa
// }
//
// func (e *wrapError) Error() string { return e.msg }
// func (e *wrapError) Unwrap() error { return e.err }  ← ← ESTA ES LA CLAVE

// El metodo Unwrap() es lo que permite a errors.Is y errors.As
// recorrer la cadena. Sin Unwrap(), la cadena esta rota.
```

### Regla de decision

```
┌──────────────────────────────────────────────────────────────────────┐
│  ¿%v o %w?                                                          │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  Usa %w cuando:                                                      │
│  ├─ El caller podria necesitar inspeccionar el error original       │
│  ├─ El error tiene tipo (sentinel o custom) que el caller conoce    │
│  ├─ Quieres preservar la cadena para errors.Is/errors.As            │
│  └─ La funcion es una capa intermedia, no el consumidor final       │
│                                                                      │
│  Usa %v cuando:                                                      │
│  ├─ Quieres OCULTAR detalles de implementacion                      │
│  ├─ El error original es un detalle interno (ej: query SQL)         │
│  ├─ Cambiar la implementacion podria cambiar los errores            │
│  │  (no quieres que callers dependan de errores internos)           │
│  └─ El error viene de una libreria que podrias reemplazar           │
│                                                                      │
│  TIP: %w crea un CONTRATO — el caller puede depender del tipo      │
│  del error envuelto. Si cambias la implementacion, puedes romper    │
│  callers que usen errors.Is/As contra errores internos.             │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 3. La Interfaz Unwrap — Mecanismo del Wrapping

El wrapping funciona a traves de una **convencion de interfaz**, no una interfaz obligatoria. Un error soporta unwrap si implementa uno de estos metodos:

```go
// Unwrap simple (Go 1.13+): cadena lineal
type interface {
    Unwrap() error
}

// Unwrap multiple (Go 1.20+): arbol de errores
type interface {
    Unwrap() []error
}
```

### 3.1 Implementar Unwrap en tipos custom

```go
type DeployError struct {
    Service string
    Phase   string
    Cause   error // el error envuelto
}

func (e *DeployError) Error() string {
    return fmt.Sprintf("deploy %s [%s]: %v", e.Service, e.Phase, e.Cause)
}

// Unwrap devuelve el error envuelto — permite que errors.Is/As lo recorran
func (e *DeployError) Unwrap() error {
    return e.Cause
}

// Uso:
func deploy(name, image string) error {
    if err := pullImage(image); err != nil {
        return &DeployError{
            Service: name,
            Phase:   "pull",
            Cause:   err, // wrapping: preserva el error original
        }
    }
    return nil
}

// El caller puede inspeccionar a traves de DeployError:
err := deploy("api", "broken:v1")
errors.Is(err, ErrRegistryAuth)  // true — recorre DeployError.Unwrap()

var pullErr *PullError
errors.As(err, &pullErr)         // true — encuentra PullError dentro de DeployError
```

### 3.2 Unwrap multiple (Go 1.20+)

```go
// Un error puede envolver MULTIPLES errores simultaneamente

type MultiStepError struct {
    Steps  []string
    Errors []error
}

func (e *MultiStepError) Error() string {
    msgs := make([]string, len(e.Errors))
    for i, err := range e.Errors {
        msgs[i] = fmt.Sprintf("%s: %s", e.Steps[i], err.Error())
    }
    return strings.Join(msgs, "; ")
}

// Unwrap() []error — Go 1.20+ multi-unwrap
func (e *MultiStepError) Unwrap() []error {
    return e.Errors
}

// errors.Is busca en TODOS los sub-errores (BFS/DFS)
err := &MultiStepError{
    Steps:  []string{"db", "cache"},
    Errors: []error{
        fmt.Errorf("connect: %w", ErrTimeout),
        fmt.Errorf("ping: %w", ErrConnectionRefused),
    },
}

errors.Is(err, ErrTimeout)           // true — encontrado en el sub-error de db
errors.Is(err, ErrConnectionRefused) // true — encontrado en el sub-error de cache
```

### 3.3 errors.Join — la forma mas simple de multi-wrap

```go
// errors.Join (Go 1.20+) combina errores y devuelve uno que implementa
// Unwrap() []error

err := errors.Join(
    fmt.Errorf("db: %w", ErrTimeout),
    fmt.Errorf("cache: %w", ErrConnectionRefused),
    fmt.Errorf("dns: %w", os.ErrNotExist),
)

// El error resultante:
fmt.Println(err)
// db: operation timed out
// cache: connection refused
// dns: file does not exist

// errors.Is busca en TODOS:
errors.Is(err, ErrTimeout)           // true
errors.Is(err, ErrConnectionRefused) // true
errors.Is(err, os.ErrNotExist)       // true

// errors.Join devuelve nil si todos los argumentos son nil:
err = errors.Join(nil, nil, nil)
fmt.Println(err) // <nil>
```

---

## 4. errors.Is — Comparacion a Traves de la Cadena

`errors.Is` responde la pregunta: **"¿este error ES (o envuelve) este target?"**

### 4.1 Algoritmo de errors.Is

```go
// Pseudocodigo de errors.Is(err, target):
func Is(err, target error) bool {
    // 1. Caso base: comparacion directa
    if err == target {
        return true
    }

    // 2. Si target implementa Is(), usarlo (custom matching)
    if t, ok := target.(interface{ Is(error) bool }); ok {
        if t.Is(err) {
            return true
        }
    }

    // 3. Si err implementa Is(), usarlo
    if e, ok := err.(interface{ Is(error) bool }); ok {
        if e.Is(target) {
            return true
        }
    }

    // 4. Unwrap y recursion
    switch x := err.(type) {
    case interface{ Unwrap() error }:
        err = x.Unwrap()
        // continuar recursion con el error desenvuelto
    case interface{ Unwrap() []error }:
        for _, e := range x.Unwrap() {
            if Is(e, target) { // recursion en cada sub-error
                return true
            }
        }
        return false
    default:
        return false
    }
    // ... loop hasta que err sea nil
}
```

### 4.2 Ejemplos practicos

```go
package main

import (
    "errors"
    "fmt"
    "io"
    "os"
)

var (
    ErrDatabase  = errors.New("database error")
    ErrCache     = errors.New("cache error")
    ErrTransient = errors.New("transient error")
)

func main() {
    // Cadena lineal de wrapping
    inner := fmt.Errorf("connect timeout: %w", ErrTransient)
    mid := fmt.Errorf("query users: %w", inner)
    outer := fmt.Errorf("load dashboard: %w", mid)

    fmt.Println("outer:", outer)
    // load dashboard: query users: connect timeout: transient error

    // errors.Is recorre TODA la cadena:
    fmt.Println("Is ErrTransient?", errors.Is(outer, ErrTransient)) // true
    fmt.Println("Is ErrDatabase?", errors.Is(outer, ErrDatabase))   // false
    fmt.Println("Is inner?", errors.Is(outer, inner))               // true (por identidad)

    // Comparacion con errores de la stdlib:
    _, err := os.Open("/nonexistent")
    wrapped := fmt.Errorf("config: %w", err)
    doubleWrapped := fmt.Errorf("init: %w", wrapped)

    fmt.Println("\nIs os.ErrNotExist?", errors.Is(doubleWrapped, os.ErrNotExist)) // true
    fmt.Println("Is io.EOF?", errors.Is(doubleWrapped, io.EOF))                   // false

    // Con errors.Join (Go 1.20+ — arbol):
    multi := errors.Join(
        fmt.Errorf("db: %w", ErrDatabase),
        fmt.Errorf("cache: %w", ErrCache),
    )
    fmt.Println("\nMulti Is ErrDatabase?", errors.Is(multi, ErrDatabase)) // true
    fmt.Println("Multi Is ErrCache?", errors.Is(multi, ErrCache))         // true
    fmt.Println("Multi Is ErrTransient?", errors.Is(multi, ErrTransient)) // false
}
```

### 4.3 Custom Is — matching personalizado

```go
// Un error puede definir su propia logica de comparacion
// implementando el metodo Is(target error) bool

type StatusCodeError struct {
    Code    int
    Message string
}

func (e *StatusCodeError) Error() string {
    return fmt.Sprintf("HTTP %d: %s", e.Code, e.Message)
}

// Is personalizado: dos StatusCodeError son "iguales" si tienen el mismo Code
// (ignora el Message)
func (e *StatusCodeError) Is(target error) bool {
    var t *StatusCodeError
    if errors.As(target, &t) {
        return e.Code == t.Code
    }
    return false
}

func main() {
    err1 := &StatusCodeError{Code: 404, Message: "user not found"}
    err2 := &StatusCodeError{Code: 404, Message: "page not found"}
    err3 := &StatusCodeError{Code: 500, Message: "internal error"}

    fmt.Println(errors.Is(err1, err2)) // true — mismo Code (404)
    fmt.Println(errors.Is(err1, err3)) // false — diferente Code

    // Funciona a traves del wrapping:
    wrapped := fmt.Errorf("api call: %w", err1)
    fmt.Println(errors.Is(wrapped, err2)) // true — recorre, luego usa custom Is
}
```

---

## 5. errors.As — Extraccion de Tipo a Traves de la Cadena

`errors.As` responde la pregunta: **"¿este error contiene (en su cadena) un error de tipo T? Si si, extraelo."**

### 5.1 Algoritmo de errors.As

```go
// Pseudocodigo de errors.As(err, target):
// target es un **puntero a puntero** del tipo buscado: **T

func As(err error, target interface{}) bool {
    // target debe ser *(*T) donde *T implementa error

    // 1. Verificar si err es del tipo *T
    if err puede asignarse a *target {
        *target = err.(T)
        return true
    }

    // 2. Si err implementa As(), usarlo
    if e, ok := err.(interface{ As(interface{}) bool }); ok {
        if e.As(target) {
            return true
        }
    }

    // 3. Unwrap y recursion (identico a Is)
    switch x := err.(type) {
    case interface{ Unwrap() error }:
        return As(x.Unwrap(), target)
    case interface{ Unwrap() []error }:
        for _, e := range x.Unwrap() {
            if As(e, target) {
                return true
            }
        }
    }

    return false
}
```

### 5.2 Ejemplos practicos

```go
package main

import (
    "errors"
    "fmt"
    "net"
    "os"
)

func main() {
    // ─── Extraer *os.PathError de una cadena de wrapping ───
    _, err := os.Open("/nonexistent")
    wrapped := fmt.Errorf("load config: %w", err)
    deepWrapped := fmt.Errorf("init app: %w", wrapped)

    var pathErr *os.PathError
    if errors.As(deepWrapped, &pathErr) {
        fmt.Println("Found *os.PathError in chain!")
        fmt.Println("  Op:", pathErr.Op)     // open
        fmt.Println("  Path:", pathErr.Path) // /nonexistent
        fmt.Println("  Err:", pathErr.Err)   // no such file or directory
    }

    // ─── Extraer *net.OpError ───
    _, err = net.DialTimeout("tcp", "192.0.2.1:12345", 1) // IP sin respuesta
    if err != nil {
        netWrapped := fmt.Errorf("connect to database: %w", err)

        var opErr *net.OpError
        if errors.As(netWrapped, &opErr) {
            fmt.Println("\nFound *net.OpError!")
            fmt.Println("  Op:", opErr.Op)
            fmt.Println("  Net:", opErr.Net)
            fmt.Println("  Timeout:", opErr.Timeout())
        }
    }

    // ─── errors.As con multi-error (Go 1.20+) ───
    multi := errors.Join(
        fmt.Errorf("step1: %w", &os.PathError{Op: "read", Path: "/etc/hosts", Err: os.ErrPermission}),
        fmt.Errorf("step2: %w", &os.LinkError{Op: "symlink", Old: "/a", New: "/b", Err: os.ErrExist}),
    )

    var linkErr *os.LinkError
    if errors.As(multi, &linkErr) {
        fmt.Println("\nFound *os.LinkError in multi-error!")
        fmt.Println("  Op:", linkErr.Op)
        fmt.Println("  Old:", linkErr.Old)
    }
}
```

### 5.3 errors.As vs type assertion directa

```go
// Type assertion directa — solo funciona en el nivel superficial
err := getError()
if pathErr, ok := err.(*os.PathError); ok {
    // Solo funciona si err ES *os.PathError
    // NO funciona si err ENVUELVE *os.PathError
    fmt.Println(pathErr.Path)
}

// errors.As — recorre TODA la cadena
var pathErr *os.PathError
if errors.As(err, &pathErr) {
    // Funciona incluso si err es:
    // fmt.Errorf("ctx1: %w", fmt.Errorf("ctx2: %w", &os.PathError{...}))
    fmt.Println(pathErr.Path)
}

// REGLA: siempre prefiere errors.As sobre type assertion
// EXCEPCION: si sabes que el error NO esta wrappedo (raro)
```

### 5.4 Custom As — extraccion personalizada

```go
// Un error puede definir su propia logica de extraccion

type AppError struct {
    Code    int
    Message string
    Details map[string]string
}

func (e *AppError) Error() string {
    return fmt.Sprintf("[%d] %s", e.Code, e.Message)
}

// As personalizado: puede "convertirse" a otros tipos de error
func (e *AppError) As(target interface{}) bool {
    // Permitir que AppError se extraiga como *StatusCodeError
    if t, ok := target.(**StatusCodeError); ok {
        *t = &StatusCodeError{
            Code:    e.Code,
            Message: e.Message,
        }
        return true
    }
    return false
}
```

---

## 6. Visualizacion de la Cadena de Errores

Cuando debuggeas errores complejos, es util poder visualizar la cadena completa:

```go
package main

import (
    "errors"
    "fmt"
    "strings"
)

// PrintErrorChain imprime la cadena de errores con indentacion
func PrintErrorChain(err error) {
    fmt.Println("Error chain:")
    printChain(err, 0)
}

func printChain(err error, depth int) {
    if err == nil {
        return
    }

    indent := strings.Repeat("  ", depth)
    prefix := "└─"
    if depth == 0 {
        prefix = "→"
    }

    fmt.Printf("%s%s [%T] %s\n", indent, prefix, err, err.Error())

    switch x := err.(type) {
    case interface{ Unwrap() error }:
        printChain(x.Unwrap(), depth+1)
    case interface{ Unwrap() []error }:
        errs := x.Unwrap()
        for i, e := range errs {
            branch := "├─"
            if i == len(errs)-1 {
                branch = "└─"
            }
            subIndent := strings.Repeat("  ", depth+1)
            fmt.Printf("%s%s [%T] %s\n", subIndent, branch, e, e.Error())
            // Recursion en cada sub-error
            if unwrapper, ok := e.(interface{ Unwrap() error }); ok {
                printChain(unwrapper.Unwrap(), depth+2)
            }
        }
    }
}

// Errores para demo
var (
    ErrAuth     = errors.New("authentication failed")
    ErrTimeout  = errors.New("timeout")
)

type ServiceError struct {
    Service string
    Cause   error
}

func (e *ServiceError) Error() string {
    return fmt.Sprintf("service %s: %v", e.Service, e.Cause)
}

func (e *ServiceError) Unwrap() error {
    return e.Cause
}

func main() {
    // Cadena lineal:
    err := fmt.Errorf("deploy app: %w",
        &ServiceError{
            Service: "auth",
            Cause: fmt.Errorf("login: %w", ErrAuth),
        },
    )
    PrintErrorChain(err)

    fmt.Println()

    // Arbol (Go 1.20+):
    multiErr := fmt.Errorf("health check failed: %w",
        errors.Join(
            fmt.Errorf("db: %w", ErrTimeout),
            fmt.Errorf("cache: %w", &ServiceError{
                Service: "redis",
                Cause:   errors.New("connection refused"),
            }),
        ),
    )
    PrintErrorChain(multiErr)
}
```

**Salida:**
```
Error chain:
→ [*fmt.wrapError] deploy app: service auth: login: authentication failed
  └─ [*main.ServiceError] service auth: login: authentication failed
    └─ [*fmt.wrapError] login: authentication failed
      └─ [*errors.errorString] authentication failed

Error chain:
→ [*fmt.wrapError] health check failed: db: timeout
cache: service redis: connection refused
  └─ [*errors.joinError] db: timeout
cache: service redis: connection refused
    ├─ [*fmt.wrapError] db: timeout
    └─ [*fmt.wrapError] cache: service redis: connection refused
```

---

## 7. Patrones de Wrapping — Cuando y Como

### 7.1 Agregar contexto en cada capa

```go
// PATRON MAS COMUN: cada funcion agrega su nombre/operacion

func readConfig(path string) error {
    data, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("readConfig(%s): %w", path, err) // nivel 1
    }
    return parseConfig(data)
}

func initService(name string) error {
    if err := readConfig("/etc/" + name + ".conf"); err != nil {
        return fmt.Errorf("initService(%s): %w", name, err) // nivel 2
    }
    return nil
}

func main() {
    err := initService("myapp")
    // "initService(myapp): readConfig(/etc/myapp.conf): open /etc/myapp.conf: permission denied"
    // Cada capa agrego su contexto — el resultado es un mini stack trace semantico
}
```

### 7.2 Patron op-target (operacion + objetivo)

```go
// Formato consistente: "operacion objetivo: error"
// Mas legible que nombre-de-funcion

func startContainer(name string) error {
    if err := pullImage(name); err != nil {
        return fmt.Errorf("pull image %s: %w", name, err)
    }
    if err := createContainer(name); err != nil {
        return fmt.Errorf("create container %s: %w", name, err)
    }
    if err := runContainer(name); err != nil {
        return fmt.Errorf("run container %s: %w", name, err)
    }
    return nil
}
// Resultado: "run container myapp: exec: permission denied"
// Mas legible que: "startContainer: createContainer: exec: permission denied"
```

### 7.3 Cuando NO wrappear

```go
// ✗ NO wrappear si no agregas informacion nueva
func process(data []byte) error {
    return parse(data) // NO hagas: fmt.Errorf("process: %w", parse(data))
    // "process" no agrega contexto util — el caller ya sabe que llamo a process
}

// ✗ NO wrappear errores que son detalles de implementacion
func GetUser(id int) (*User, error) {
    row := db.QueryRow("SELECT ... WHERE id = $1", id)
    var u User
    err := row.Scan(&u.Name, &u.Email)
    if err != nil {
        // %v, no %w — no queremos que el caller dependa del tipo de error de SQL
        // Si cambias de Postgres a MySQL, los errores seran diferentes
        return nil, fmt.Errorf("get user %d: %v", id, err) // ← %v intencional
    }
    return &u, nil
}

// ✗ NO wrappear si creas un error nuevo con datos suficientes
func validate(port int) error {
    if port < 1 || port > 65535 {
        // Error nuevo con toda la info necesaria — no hay nada que wrappear
        return fmt.Errorf("invalid port: %d (must be 1-65535)", port)
    }
    return nil
}
```

### 7.4 Tabla de decision de wrapping

```
┌──────────────────────────────────────────────────────────────────────────┐
│  DECISION DE WRAPPING                                                    │
├─────────────────────────┬──────────┬─────────────────────────────────────┤
│  Escenario              │  Verbo   │  Razon                              │
├─────────────────────────┼──────────┼─────────────────────────────────────┤
│  Error del OS/stdlib    │  %w      │  Caller puede necesitar Is/As       │
│  Error de libreria      │  %w o %v │  %v si quieres aislar la dependencia│
│  Error de SQL query     │  %v      │  Detalle de implementacion          │
│  Error de capa inferior │  %w      │  Propagar contexto                  │
│  Error propio (nuevo)   │  N/A     │  No hay error que wrappear          │
│  Error no critico       │  log     │  Logear y continuar, no propagar    │
│  Ultimo return          │  directo │  return fn() — sin wrap extra       │
└─────────────────────────┴──────────┴─────────────────────────────────────┘
```

---

## 8. Multi-wrap con %w (Go 1.20+)

Go 1.20 permite usar `%w` multiples veces en un solo `fmt.Errorf`:

```go
package main

import (
    "errors"
    "fmt"
)

var (
    ErrValidation = errors.New("validation error")
    ErrDatabase   = errors.New("database error")
)

func saveUser(name string) error {
    // Simular que hay AMBOS errores simultaneamente
    validationErr := fmt.Errorf("name too short: %w", ErrValidation)
    dbErr := fmt.Errorf("connection pool exhausted: %w", ErrDatabase)

    // Multi-wrap: un error que envuelve DOS errores
    return fmt.Errorf("save user %q: %w and %w", name, validationErr, dbErr)
}

func main() {
    err := saveUser("ab")
    fmt.Println(err)
    // save user "ab": name too short: validation error and connection pool exhausted: database error

    // errors.Is encuentra AMBOS en la cadena:
    fmt.Println("Is ErrValidation?", errors.Is(err, ErrValidation)) // true
    fmt.Println("Is ErrDatabase?", errors.Is(err, ErrDatabase))     // true

    // Internamente, el error implementa Unwrap() []error
    // (no Unwrap() error — es multi-unwrap)
}
```

### Diferencia entre errors.Join y multi-%w

```go
// errors.Join: concatena mensajes con newline, sin contexto adicional
err1 := errors.Join(errA, errB)
// err1.Error() → "errA message\nerrB message"

// Multi-%w: permite agregar contexto Y wrappear multiples
err2 := fmt.Errorf("operation X: %w and %w", errA, errB)
// err2.Error() → "operation X: errA message and errB message"

// Ambos implementan Unwrap() []error
// Ambos son recorridos por errors.Is y errors.As
// La diferencia es el formato del mensaje
```

---

## 9. La Cadena como Linked List y como Arbol

### Cadena lineal (Go 1.13+)

```
err4 → Unwrap() → err3 → Unwrap() → err2 → Unwrap() → err1 → nil

errors.Is(err4, err1) recorre:
  err4 == err1?  no
  err4.Unwrap() = err3
  err3 == err1?  no
  err3.Unwrap() = err2
  err2 == err1?  no
  err2.Unwrap() = err1
  err1 == err1?  SI → return true

Complejidad: O(n) donde n = profundidad de la cadena
```

### Arbol de errores (Go 1.20+)

```
                errRoot
               /       \
          errA           errB
         /   \              \
    errA1   errA2          errB1

errRoot.Unwrap() = [errA, errB]
errA.Unwrap() = [errA1, errA2]
errB.Unwrap() = errB1

errors.Is(errRoot, errA2) recorre (DFS):
  errRoot == errA2?  no
  errRoot.Unwrap() = [errA, errB]
    errA == errA2?  no
    errA.Unwrap() = [errA1, errA2]
      errA1 == errA2?  no
      errA2 == errA2?  SI → return true

Complejidad: O(n) donde n = numero total de nodos en el arbol
```

### Cuidado con ciclos

```go
// Los errores NO deben formar ciclos en Unwrap()
// Si creas un ciclo, errors.Is entrara en loop infinito

// Esto es un BUG:
type badError struct {
    cause error
}
func (e *badError) Unwrap() error { return e.cause }

a := &badError{}
b := &badError{cause: a}
a.cause = b // ← CICLO: a → b → a → b → ...

// errors.Is(a, someErr) → loop infinito
// Go no tiene proteccion contra esto — responsabilidad del programador
```

---

## 10. Patrones Avanzados de Wrapping

### 10.1 Error enrichment (agregar metadatos sin nuevo tipo)

```go
// Cuando quieres agregar contexto sin crear un struct por cada caso

func connectDB(host string, port int) error {
    err := tcpConnect(host, port)
    if err != nil {
        // Agregar multiples datos al mensaje
        return fmt.Errorf("connect db (host=%s, port=%d, timeout=5s): %w",
            host, port, err)
    }
    return nil
}
// El contexto esta en el mensaje, el error original en la cadena
```

### 10.2 Error annotation pattern (anotar sin wrappear)

```go
// A veces quieres agregar una "nota" a un error sin exponerlo al caller

type annotatedError struct {
    msg   string
    cause error
    // Intencionalmente NO tiene Unwrap() — la cadena se corta aqui
}

func (e *annotatedError) Error() string {
    return fmt.Sprintf("%s: %v", e.msg, e.cause)
}

func internalOp() error {
    err := sqlQuery() // error de SQL interno
    if err != nil {
        // Anotar sin wrappear — el caller NO puede hacer errors.As con *sql.Error
        return &annotatedError{msg: "internal operation failed", cause: err}
    }
    return nil
}
```

### 10.3 Error upgrade pattern (envolver con tipo mas rico)

```go
// Convertir un error simple en uno con datos estructurados

func processRequest(userID int, action string) error {
    if err := authorize(userID, action); err != nil {
        // "Upgrade" el error generico a uno con datos inspeccionables
        return &APIError{
            Code:     403,
            Message:  "access denied",
            UserID:   userID,
            Action:   action,
            Internal: err, // wrapping via Unwrap()
        }
    }
    return nil
}

type APIError struct {
    Code     int
    Message  string
    UserID   int
    Action   string
    Internal error
}

func (e *APIError) Error() string {
    return fmt.Sprintf("API error %d: %s", e.Code, e.Message)
}

func (e *APIError) Unwrap() error {
    return e.Internal
}

// El caller puede:
// 1. errors.As(err, &apiErr) → obtener Code, UserID, Action
// 2. errors.Is(err, ErrUnauthorized) → buscar en el error original
// 3. err.Error() → mensaje limpio para el usuario
```

### 10.4 Error deduplication (evitar wrapping redundante)

```go
// Si el error ya tiene suficiente contexto, no agregues mas

func readConfig(path string) error {
    _, err := os.ReadFile(path)
    if err != nil {
        // os.ReadFile ya devuelve *os.PathError con Op, Path, Err
        // Su Error() ya dice: "open /etc/config: permission denied"

        // ✗ Redundante:
        // return fmt.Errorf("readConfig: read file %s: %w", path, err)
        // → "readConfig: read file /etc/config: open /etc/config: permission denied"
        //   ^^^^^^^^^^^  ^^^^^^^^^^^^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
        //   tu contexto    tu path (repetido)      PathError ya tiene el path

        // ✓ Solo agrega contexto de alto nivel:
        return fmt.Errorf("load config: %w", err)
        // → "load config: open /etc/config: permission denied"
    }
    return nil
}
```

---

## 11. Wrapping en la Stdlib — Patrones Reales

### 11.1 os package

```go
// os.Open devuelve *fs.PathError que implementa Unwrap()
// PathError.Err es el error del syscall (ej: syscall.ENOENT)
// os.ErrNotExist es un sentinel que PathError.Unwrap() puede igualar

err := os.Open("/nonexistent")
// cadena: *fs.PathError{Op:"open", Path:"/nonexistent", Err: syscall.Errno(2)}
//                                                             ↕ Is()
//                                                        os.ErrNotExist

errors.Is(err, os.ErrNotExist) // true — PathError.Err → syscall.ENOENT → Is(os.ErrNotExist)
```

### 11.2 net package

```go
// net.Dial devuelve *net.OpError que envuelve multiples niveles
err := net.Dial("tcp", "192.0.2.1:80")
// cadena: *net.OpError{Op:"dial", Net:"tcp", Addr:...,
//           Err: *os.SyscallError{Syscall:"connect",
//             Err: syscall.Errno(111)}} // ECONNREFUSED

// errors.As puede extraer cualquier nivel:
var opErr *net.OpError
errors.As(err, &opErr)     // nivel 1

var syscallErr *os.SyscallError
errors.As(err, &syscallErr) // nivel 2

// errors.Is verifica contra errores del sistema:
errors.Is(err, syscall.ECONNREFUSED) // true
```

### 11.3 encoding/json package

```go
// json.Unmarshal devuelve diferentes tipos segun el error:
// *json.SyntaxError — JSON malformado
// *json.UnmarshalTypeError — tipo no coincide

err := json.Unmarshal([]byte(`{bad json}`), &target)
var syntaxErr *json.SyntaxError
if errors.As(err, &syntaxErr) {
    fmt.Printf("Syntax error at byte offset %d\n", syntaxErr.Offset)
}
```

### 11.4 Patron comun de wrapping en stdlib

```
┌──────────────────────────────────────────────────────────────────────┐
│  PATRON DE WRAPPING EN LA STDLIB                                     │
│                                                                      │
│  Capa 1 (syscall):                                                   │
│    syscall.Errno(ENOENT)                                             │
│         ↑ Unwrap/Is                                                  │
│  Capa 2 (os):                                                        │
│    *os.SyscallError{Syscall:"open", Err: Errno}                     │
│         ↑ Unwrap                                                     │
│  Capa 3 (fs):                                                        │
│    *fs.PathError{Op:"open", Path:"/etc/config", Err: SyscallError}  │
│         ↑ Unwrap                                                     │
│  Tu codigo:                                                          │
│    fmt.Errorf("load config: %w", pathError)                          │
│                                                                      │
│  errors.Is(tuError, os.ErrNotExist) → recorre 4 niveles → true     │
│  errors.As(tuError, &pathErr) → recorre 1 nivel → encontrado       │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 12. Comparacion con C y Rust

### C — no tiene wrapping (manual con strings)

```c
// C: no hay cadena de errores — solo un int o un string

// Intento manual de "wrapping":
char error_msg[1024];

int read_config(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) {
        snprintf(error_msg, sizeof(error_msg),
                 "read_config(%s): %s", path, strerror(errno));
        return -1;
    }
    // ...
    return 0;
}

int init_service(const char* name) {
    char config_path[256];
    snprintf(config_path, sizeof(config_path), "/etc/%s.conf", name);
    if (read_config(config_path) != 0) {
        char prev_msg[1024];
        strncpy(prev_msg, error_msg, sizeof(prev_msg));
        snprintf(error_msg, sizeof(error_msg),
                 "init_service(%s): %s", name, prev_msg);
        return -1;
    }
    return 0;
}

// Problemas:
// 1. error_msg es global — race conditions
// 2. No hay tipo — solo un string, no puedes inspeccionar programaticamente
// 3. snprintf se puede truncar
// 4. No hay Unwrap/Is/As — solo comparar strings
// 5. El "wrapping" es manual y propenso a errores
```

### Rust — From trait, ? operator, y thiserror/anyhow

```rust
use std::io;
use std::num::ParseIntError;

// Rust: definir un enum que agrupa todos los errores posibles
#[derive(Debug)]
enum ConfigError {
    Io(io::Error),
    Parse(ParseIntError),
    Validation(String),
}

// Display = Error() de Go
impl std::fmt::Display for ConfigError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ConfigError::Io(e) => write!(f, "io error: {}", e),
            ConfigError::Parse(e) => write!(f, "parse error: {}", e),
            ConfigError::Validation(msg) => write!(f, "validation: {}", msg),
        }
    }
}

impl std::error::Error for ConfigError {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        // source() es el equivalente de Unwrap() de Go
        match self {
            ConfigError::Io(e) => Some(e),
            ConfigError::Parse(e) => Some(e),
            ConfigError::Validation(_) => None,
        }
    }
}

// From trait permite conversion automatica con ?
impl From<io::Error> for ConfigError {
    fn from(e: io::Error) -> Self {
        ConfigError::Io(e)
    }
}

impl From<ParseIntError> for ConfigError {
    fn from(e: ParseIntError) -> Self {
        ConfigError::Parse(e)
    }
}

fn read_config(path: &str) -> Result<Config, ConfigError> {
    let data = std::fs::read_to_string(path)?; // io::Error → ConfigError via From
    let port: u16 = data.trim().parse()?;       // ParseIntError → ConfigError via From
    Ok(Config { port })
}

// thiserror crate: genera todo el boilerplate automaticamente
// use thiserror::Error;
// #[derive(Error, Debug)]
// enum ConfigError {
//     #[error("io error: {0}")]
//     Io(#[from] io::Error),
//     #[error("parse error: {0}")]
//     Parse(#[from] ParseIntError),
//     #[error("validation: {0}")]
//     Validation(String),
// }

// anyhow crate: wrapping dinamico (como fmt.Errorf %w de Go)
// use anyhow::{Context, Result};
// fn read_config(path: &str) -> Result<Config> {
//     let data = std::fs::read_to_string(path)
//         .context("read config file")?;      // equivalente a fmt.Errorf("...: %w")
//     Ok(...)
// }
```

### Tabla comparativa de wrapping

```
┌──────────────────────────────────────────────────────────────────────────┐
│  WRAPPING — C vs Go vs Rust                                              │
├──────────────────┬───────────────┬────────────────┬──────────────────────┤
│  Aspecto         │  C            │  Go            │  Rust                │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Mecanismo       │  snprintf     │  fmt.Errorf %w │  From trait + ?      │
│                  │  (manual)     │  + Unwrap()    │  + source()          │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Cadena          │  No existe    │  Linked list   │  source() chain      │
│                  │               │  via Unwrap()  │  (similar)           │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Multi-error     │  No           │  Go 1.20+      │  Vec<Error> manual   │
│                  │               │  errors.Join   │                      │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Inspeccion      │  strcmp()      │  errors.Is     │  downcast_ref        │
│  por tipo        │  (fragil)     │  errors.As     │  (Any trait, raro)   │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Inspeccion      │  N/A          │  errors.Is     │  match enum variant  │
│  por valor       │               │  (sentinel)    │  (exhaustivo)        │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Ergonomia       │  Muy baja     │  Media (%w)    │  Alta (?, From)      │
│  de wrapping     │               │                │  + thiserror derive  │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Contexto        │  Manual       │  En el string  │  .context() (anyhow) │
│  adicional       │  (snprintf)   │  de Errorf     │  .with_context()     │
├──────────────────┼───────────────┼────────────────┼──────────────────────┤
│  Ocultar error   │  N/A          │  %v (no %w)    │  No usar From, solo  │
│  interno         │               │  intencional   │  map_err()           │
└──────────────────┴───────────────┴────────────────┴──────────────────────┘
```

---

## 13. Programa Completo: Infrastructure Deployment con Error Wrapping Chain

```go
// ============================================================================
// INFRASTRUCTURE DEPLOYMENT — Error Wrapping Chain
//
// Demuestra:
// - Cadenas de wrapping con %w a traves de multiples capas
// - errors.Is buscando sentinels a traves de la cadena
// - errors.As extrayendo tipos custom de cualquier nivel
// - Multi-wrap con errors.Join (Go 1.20+)
// - Custom Is() para matching flexible
// - Visualizacion de la cadena de errores
// - Patron de error upgrade y annotation
// - Cuando usar %w vs %v (ocultar vs exponer)
//
// Compilar y ejecutar:
//   go run main.go
// ============================================================================

package main

import (
    "errors"
    "fmt"
    "strings"
)

// ============================================================================
// PARTE 1: Sentinel errors
// ============================================================================

var (
    ErrNotFound       = errors.New("not found")
    ErrPermission     = errors.New("permission denied")
    ErrTimeout        = errors.New("timeout")
    ErrConnectionLost = errors.New("connection lost")
    ErrInvalidImage   = errors.New("invalid image")
    ErrQuotaExceeded  = errors.New("quota exceeded")
)

// ============================================================================
// PARTE 2: Tipos de error con wrapping
// ============================================================================

// RegistryError — error del registry de imagenes
type RegistryError struct {
    Registry string
    Image    string
    Tag      string
    Cause    error
}

func (e *RegistryError) Error() string {
    return fmt.Sprintf("registry %s: %s:%s: %v", e.Registry, e.Image, e.Tag, e.Cause)
}

func (e *RegistryError) Unwrap() error {
    return e.Cause
}

// KubeError — error de Kubernetes
type KubeError struct {
    Cluster   string
    Namespace string
    Resource  string
    Op        string
    Cause     error
}

func (e *KubeError) Error() string {
    return fmt.Sprintf("k8s %s/%s: %s %s: %v", e.Cluster, e.Namespace, e.Op, e.Resource, e.Cause)
}

func (e *KubeError) Unwrap() error {
    return e.Cause
}

// DeployError — error de deploy de alto nivel
type DeployError struct {
    App     string
    Version string
    Phase   string
    Cause   error
}

func (e *DeployError) Error() string {
    return fmt.Sprintf("deploy %s@%s [%s]: %v", e.App, e.Version, e.Phase, e.Cause)
}

func (e *DeployError) Unwrap() error {
    return e.Cause
}

// RetryableError — marca un error como retriable
type RetryableError struct {
    Cause      error
    MaxRetries int
}

func (e *RetryableError) Error() string {
    return fmt.Sprintf("%v (retriable, max %d)", e.Cause, e.MaxRetries)
}

func (e *RetryableError) Unwrap() error {
    return e.Cause
}

// Is personalizado: RetryableError "es" cualquier error que envuelva
// Y ademas matchea contra otro RetryableError (por identidad de Cause)
func (e *RetryableError) Is(target error) bool {
    var rt *RetryableError
    if errors.As(target, &rt) {
        return true // cualquier RetryableError matchea con otro
    }
    return false
}

// IsRetryable verifica si un error (en su cadena) es retriable
func IsRetryable(err error) bool {
    var rt *RetryableError
    return errors.As(err, &rt)
}

// ============================================================================
// PARTE 3: Funciones de bajo nivel (leaf errors)
// ============================================================================

func authenticateRegistry(registry, token string) error {
    if token == "" {
        return fmt.Errorf("authenticate %s: %w", registry, ErrPermission)
    }
    if token == "expired" {
        return fmt.Errorf("authenticate %s: token expired: %w", registry, ErrPermission)
    }
    return nil
}

func pullImage(registry, image, tag string) error {
    if err := authenticateRegistry(registry, "valid"); err != nil {
        return &RegistryError{
            Registry: registry,
            Image:    image,
            Tag:      tag,
            Cause:    err,
        }
    }

    if image == "broken-app" {
        return &RegistryError{
            Registry: registry,
            Image:    image,
            Tag:      tag,
            Cause:    ErrInvalidImage,
        }
    }

    if tag == "huge" {
        return &RegistryError{
            Registry: registry,
            Image:    image,
            Tag:      tag,
            Cause:    fmt.Errorf("layer download: %w", ErrQuotaExceeded),
        }
    }

    return nil
}

func applyManifest(cluster, namespace, resource string) error {
    if namespace == "restricted" {
        return &KubeError{
            Cluster:   cluster,
            Namespace: namespace,
            Resource:  resource,
            Op:        "apply",
            Cause:     ErrPermission,
        }
    }

    if cluster == "down-cluster" {
        return &KubeError{
            Cluster:   cluster,
            Namespace: namespace,
            Resource:  resource,
            Op:        "apply",
            Cause: &RetryableError{
                Cause:      ErrConnectionLost,
                MaxRetries: 3,
            },
        }
    }

    return nil
}

func healthCheck(app string) error {
    if app == "slow-app" {
        return &RetryableError{
            Cause:      fmt.Errorf("health %s: %w", app, ErrTimeout),
            MaxRetries: 5,
        }
    }
    return nil
}

// ============================================================================
// PARTE 4: Funcion de deploy (capas de wrapping)
// ============================================================================

func deploy(app, version, registry, cluster, namespace string) error {
    // Fase 1: Pull image (agrega wrapping de deploy)
    if err := pullImage(registry, app, version); err != nil {
        return &DeployError{
            App:     app,
            Version: version,
            Phase:   "pull",
            Cause:   err,
        }
    }

    // Fase 2: Apply manifest
    resource := fmt.Sprintf("deployment/%s", app)
    if err := applyManifest(cluster, namespace, resource); err != nil {
        return &DeployError{
            App:     app,
            Version: version,
            Phase:   "apply",
            Cause:   err,
        }
    }

    // Fase 3: Health check
    if err := healthCheck(app); err != nil {
        return &DeployError{
            App:     app,
            Version: version,
            Phase:   "health",
            Cause:   err,
        }
    }

    return nil
}

// ============================================================================
// PARTE 5: Utilidades de cadena de errores
// ============================================================================

func printChain(err error, depth int) {
    if err == nil {
        return
    }
    indent := strings.Repeat("  ", depth)
    fmt.Printf("%s[%T] %s\n", indent, err, err.Error())

    switch x := err.(type) {
    case interface{ Unwrap() error }:
        printChain(x.Unwrap(), depth+1)
    case interface{ Unwrap() []error }:
        for _, e := range x.Unwrap() {
            printChain(e, depth+1)
        }
    }
}

func analyzeError(label string, err error) {
    fmt.Printf("\n─── %s ───\n", label)
    fmt.Printf("Message: %s\n", err)
    fmt.Println()
    fmt.Println("Chain:")
    printChain(err, 1)
    fmt.Println()

    // Sentinel checks
    sentinels := []struct {
        name string
        err  error
    }{
        {"ErrNotFound", ErrNotFound},
        {"ErrPermission", ErrPermission},
        {"ErrTimeout", ErrTimeout},
        {"ErrConnectionLost", ErrConnectionLost},
        {"ErrInvalidImage", ErrInvalidImage},
        {"ErrQuotaExceeded", ErrQuotaExceeded},
    }

    fmt.Println("Sentinel checks (errors.Is):")
    for _, s := range sentinels {
        if errors.Is(err, s.err) {
            fmt.Printf("  ✓ Is %s\n", s.name)
        }
    }

    // Type checks
    fmt.Println()
    fmt.Println("Type extraction (errors.As):")

    var deployErr *DeployError
    if errors.As(err, &deployErr) {
        fmt.Printf("  ✓ DeployError: app=%s version=%s phase=%s\n",
            deployErr.App, deployErr.Version, deployErr.Phase)
    }

    var regErr *RegistryError
    if errors.As(err, &regErr) {
        fmt.Printf("  ✓ RegistryError: registry=%s image=%s:%s\n",
            regErr.Registry, regErr.Image, regErr.Tag)
    }

    var kubeErr *KubeError
    if errors.As(err, &kubeErr) {
        fmt.Printf("  ✓ KubeError: cluster=%s ns=%s op=%s resource=%s\n",
            kubeErr.Cluster, kubeErr.Namespace, kubeErr.Op, kubeErr.Resource)
    }

    // Retriable check
    if IsRetryable(err) {
        var rt *RetryableError
        errors.As(err, &rt)
        fmt.Printf("  ✓ RetryableError: maxRetries=%d\n", rt.MaxRetries)
    }
}

// ============================================================================
// PARTE 6: main
// ============================================================================

func main() {
    fmt.Println("╔══════════════════════════════════════════════════════════════╗")
    fmt.Println("║      Error Wrapping Chain — Infrastructure Deployment       ║")
    fmt.Println("╚══════════════════════════════════════════════════════════════╝")

    // ─── Caso 1: imagen invalida (cadena: DeployError → RegistryError → sentinel) ───
    err := deploy("broken-app", "v1.0", "gcr.io/myproject", "prod-cluster", "default")
    if err != nil {
        analyzeError("Caso 1: Imagen invalida", err)
    }

    // ─── Caso 2: quota excedida (cadena mas profunda) ───
    err = deploy("big-app", "huge", "gcr.io/myproject", "prod-cluster", "default")
    if err != nil {
        analyzeError("Caso 2: Quota excedida", err)
    }

    // ─── Caso 3: permiso denegado en namespace (KubeError) ───
    err = deploy("api", "v2.0", "gcr.io/myproject", "prod-cluster", "restricted")
    if err != nil {
        analyzeError("Caso 3: Permiso denegado", err)
    }

    // ─── Caso 4: cluster caido (RetryableError) ───
    err = deploy("api", "v2.0", "gcr.io/myproject", "down-cluster", "default")
    if err != nil {
        analyzeError("Caso 4: Cluster caido (retriable)", err)
    }

    // ─── Caso 5: health check timeout (RetryableError) ───
    err = deploy("slow-app", "v1.0", "gcr.io/myproject", "prod-cluster", "default")
    if err != nil {
        analyzeError("Caso 5: Health check timeout (retriable)", err)
    }

    // ─── Caso 6: multi-error con errors.Join ───
    fmt.Println()
    fmt.Println("─── Caso 6: Multi-deploy (errors.Join) ───")
    fmt.Println()

    apps := []struct {
        app, version string
    }{
        {"api", "v2.0"},
        {"broken-app", "v1.0"},
        {"big-app", "huge"},
    }

    var deployErrors []error
    for _, a := range apps {
        if err := deploy(a.app, a.version, "gcr.io/myproject", "prod-cluster", "default"); err != nil {
            deployErrors = append(deployErrors, err)
        }
    }

    if multiErr := errors.Join(deployErrors...); multiErr != nil {
        fmt.Printf("Multi-deploy failed (%d errors):\n\n", len(deployErrors))
        fmt.Println(multiErr)

        fmt.Println()
        fmt.Println("errors.Is checks across ALL errors:")
        fmt.Println("  Is ErrInvalidImage?", errors.Is(multiErr, ErrInvalidImage))
        fmt.Println("  Is ErrQuotaExceeded?", errors.Is(multiErr, ErrQuotaExceeded))
        fmt.Println("  Is ErrPermission?", errors.Is(multiErr, ErrPermission))
        fmt.Println("  Is ErrTimeout?", errors.Is(multiErr, ErrTimeout))
    }

    // ─── Caso 7: deploy exitoso ───
    fmt.Println()
    fmt.Println("─── Caso 7: Deploy exitoso ───")
    err = deploy("api", "v2.0", "gcr.io/myproject", "prod-cluster", "default")
    if err != nil {
        fmt.Println("ERROR:", err)
    } else {
        fmt.Println("  ✓ api@v2.0 deployed successfully")
    }

    fmt.Println()
    fmt.Println(strings.Repeat("─", 62))
    fmt.Println()
    fmt.Println("  RESUMEN:")
    fmt.Println("  - %w envuelve, %v anota (cadena rota)")
    fmt.Println("  - errors.Is recorre Unwrap() buscando sentinel")
    fmt.Println("  - errors.As recorre Unwrap() extrayendo tipo")
    fmt.Println("  - errors.Join combina errores (Go 1.20+)")
    fmt.Println("  - Custom Is() para matching flexible")
    fmt.Println("  - Unwrap() []error para arboles (Go 1.20+)")
    fmt.Println()
}
```

**Salida esperada (parcial):**
```
─── Caso 1: Imagen invalida ───
Message: deploy broken-app@v1.0 [pull]: registry gcr.io/myproject: broken-app:v1.0: invalid image

Chain:
  [*main.DeployError] deploy broken-app@v1.0 [pull]: ...
    [*main.RegistryError] registry gcr.io/myproject: broken-app:v1.0: invalid image
      [*errors.errorString] invalid image

Sentinel checks (errors.Is):
  ✓ Is ErrInvalidImage

Type extraction (errors.As):
  ✓ DeployError: app=broken-app version=v1.0 phase=pull
  ✓ RegistryError: registry=gcr.io/myproject image=broken-app:v1.0

─── Caso 4: Cluster caido (retriable) ───
Message: deploy api@v2.0 [apply]: k8s down-cluster/default: apply deployment/api: connection lost (retriable, max 3)

Sentinel checks (errors.Is):
  ✓ Is ErrConnectionLost

Type extraction (errors.As):
  ✓ DeployError: app=api version=v2.0 phase=apply
  ✓ KubeError: cluster=down-cluster ns=default op=apply resource=deployment/api
  ✓ RetryableError: maxRetries=3
```

---

## 14. Ejercicios

### Ejercicio 1: Construir y recorrer una cadena

```go
// Crea una cadena de 4 niveles de wrapping:
// Nivel 1: errors.New("disk full") — sentinel
// Nivel 2: *IOError{Device: "/dev/sda1", Cause: nivel1}
// Nivel 3: fmt.Errorf("write log: %w", nivel2)
// Nivel 4: fmt.Errorf("rotate logs: %w", nivel3)
//
// Luego verifica:
// 1. errors.Is(nivel4, ErrDiskFull) == true
// 2. errors.As(nivel4, &ioErr) == true, ioErr.Device == "/dev/sda1"
// 3. Imprime la cadena completa con printChain

var ErrDiskFull = errors.New("disk full")

type IOError struct {
    Device string
    Cause  error
}
// Implementa Error() y Unwrap() para IOError
```

<details>
<summary>Solucion</summary>

```go
func (e *IOError) Error() string {
    return fmt.Sprintf("io error on %s: %v", e.Device, e.Cause)
}

func (e *IOError) Unwrap() error {
    return e.Cause
}

func main() {
    level1 := ErrDiskFull
    level2 := &IOError{Device: "/dev/sda1", Cause: level1}
    level3 := fmt.Errorf("write log: %w", level2)
    level4 := fmt.Errorf("rotate logs: %w", level3)

    fmt.Println("Full message:", level4)
    // rotate logs: write log: io error on /dev/sda1: disk full

    fmt.Println("Is ErrDiskFull?", errors.Is(level4, ErrDiskFull)) // true

    var ioErr *IOError
    if errors.As(level4, &ioErr) {
        fmt.Println("IOError.Device:", ioErr.Device) // /dev/sda1
    }
}
```
</details>

### Ejercicio 2: %w vs %v decision

```go
// Para cada caso, decide si usar %w o %v y explica por que:

// Caso A: funcion publica de una libreria de base de datos
func (db *DB) GetUser(id int) (*User, error) {
    row := db.conn.QueryRow("SELECT * FROM users WHERE id = $1", id)
    err := row.Scan(&user.Name, &user.Email)
    if err != nil {
        return nil, fmt.Errorf("get user %d: ___", id, err) // %w o %v?
    }
    return &user, nil
}

// Caso B: capa intermedia que pasa errores de OS
func readCertificate(path string) ([]byte, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, fmt.Errorf("read certificate: ___", err) // %w o %v?
    }
    return data, nil
}

// Caso C: handler HTTP que transforma errores para la respuesta
func handleGetUser(w http.ResponseWriter, r *http.Request) {
    user, err := userService.GetByID(id)
    if err != nil {
        log.Printf("get user: ___", err) // %w o %v?
        http.Error(w, "user not found", 404)
    }
}
```

<details>
<summary>Solucion</summary>

```go
// Caso A: %v — detalle de implementacion
// El caller de GetUser no deberia depender de errores de sql.
// Si cambias de PostgreSQL a MySQL, los errores cambian.
// Usar %w haria que callers dependan de *pq.Error internamente.
return nil, fmt.Errorf("get user %d: %v", id, err) // %v

// Caso B: %w — el caller puede necesitar errors.Is(err, os.ErrNotExist)
// El error del OS es util para el caller (¿archivo no existe? ¿permiso?)
return nil, fmt.Errorf("read certificate: %w", err) // %w

// Caso C: %v — log interno, no se propaga
// Este es un log, no un return. El error no se devuelve al HTTP client.
// No hay cadena que recorrer — es consumo final del error.
log.Printf("get user: %v", err) // %v (en logs, %v es suficiente)
```
</details>

### Ejercicio 3: Implementar Custom Is para errores HTTP

```go
// Crea un tipo HTTPError con Code y Message.
// Implementa Is() de forma que dos HTTPError matcheen
// si tienen el mismo Code (ignoran Message).
// Verifica que funciona a traves de wrapping.

// Test:
// err404a := &HTTPError{Code: 404, Message: "user not found"}
// err404b := &HTTPError{Code: 404, Message: "page not found"}
// err500  := &HTTPError{Code: 500, Message: "internal error"}
// wrapped := fmt.Errorf("api: %w", err404a)
//
// errors.Is(wrapped, err404b) → true  (mismo code)
// errors.Is(wrapped, err500)  → false (diferente code)
```

<details>
<summary>Solucion</summary>

```go
type HTTPError struct {
    Code    int
    Message string
}

func (e *HTTPError) Error() string {
    return fmt.Sprintf("HTTP %d: %s", e.Code, e.Message)
}

func (e *HTTPError) Is(target error) bool {
    var t *HTTPError
    if errors.As(target, &t) {
        return e.Code == t.Code
    }
    return false
}

func main() {
    err404a := &HTTPError{Code: 404, Message: "user not found"}
    err404b := &HTTPError{Code: 404, Message: "page not found"}
    err500 := &HTTPError{Code: 500, Message: "internal error"}
    wrapped := fmt.Errorf("api: %w", err404a)

    fmt.Println(errors.Is(wrapped, err404b)) // true
    fmt.Println(errors.Is(wrapped, err500))  // false
}
```
</details>

### Ejercicio 4: Diagnosticar cadena de errores

```go
// Implementa una funcion ErrorChainInfo que dado un error devuelva
// un slice con informacion de cada nivel de la cadena:

type ChainLink struct {
    Type    string // reflect.TypeOf(err).String()
    Message string // err.Error()
    Depth   int
}

func ErrorChainInfo(err error) []ChainLink {
    // tu codigo aqui
    // Recorre la cadena via Unwrap() y Unwrap() []error
    // Para multi-unwrap, aplana los sub-errores (BFS)
}
```

<details>
<summary>Solucion</summary>

```go
import "reflect"

func ErrorChainInfo(err error) []ChainLink {
    var chain []ChainLink
    collectChain(err, 0, &chain)
    return chain
}

func collectChain(err error, depth int, chain *[]ChainLink) {
    if err == nil {
        return
    }

    *chain = append(*chain, ChainLink{
        Type:    reflect.TypeOf(err).String(),
        Message: err.Error(),
        Depth:   depth,
    })

    switch x := err.(type) {
    case interface{ Unwrap() error }:
        collectChain(x.Unwrap(), depth+1, chain)
    case interface{ Unwrap() []error }:
        for _, e := range x.Unwrap() {
            collectChain(e, depth+1, chain)
        }
    }
}

// Uso:
func main() {
    err := fmt.Errorf("deploy: %w",
        fmt.Errorf("pull: %w", errors.New("auth failed")))

    for _, link := range ErrorChainInfo(err) {
        indent := strings.Repeat("  ", link.Depth)
        fmt.Printf("%s[%s] %s\n", indent, link.Type, link.Message)
    }
    // [*fmt.wrapError] deploy: pull: auth failed
    //   [*fmt.wrapError] pull: auth failed
    //     [*errors.errorString] auth failed
}
```
</details>

---

## 15. Resumen Visual

```
┌──────────────────────────────────────────────────────────────────────────┐
│  WRAPPING DE ERRORES — Resumen                                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  %v vs %w:                                                               │
│  ├─ %v: copia texto, ROMPE la cadena (error original perdido)           │
│  ├─ %w: envuelve, PRESERVA la cadena (Unwrap() devuelve original)      │
│  ├─ %v para ocultar detalles internos (SQL, impl details)              │
│  └─ %w para exponer errores que el caller puede inspeccionar            │
│                                                                          │
│  Unwrap():                                                               │
│  ├─ Unwrap() error     → cadena lineal (Go 1.13+)                      │
│  ├─ Unwrap() []error   → arbol de errores (Go 1.20+)                   │
│  ├─ errors.Join(e1,e2) → crea Unwrap() []error automaticamente         │
│  ├─ fmt.Errorf("%w %w") → multi-wrap con contexto (Go 1.20+)          │
│  └─ Sin Unwrap() → error terminal (leaf), cadena termina aqui          │
│                                                                          │
│  errors.Is(err, target):                                                 │
│  ├─ Recorre la cadena Unwrap() completa                                 │
│  ├─ Compara con == en cada nivel                                        │
│  ├─ Respeta custom Is(error) bool si existe                             │
│  ├─ Recorre TODOS los sub-errores en arboles (Go 1.20+)                │
│  └─ Uso: comparar contra sentinel errors (ErrNotFound, os.ErrExist)    │
│                                                                          │
│  errors.As(err, &target):                                                │
│  ├─ Recorre la cadena buscando un tipo que matchee                      │
│  ├─ Asigna el valor encontrado a *target                                │
│  ├─ Respeta custom As(interface{}) bool si existe                       │
│  ├─ SIEMPRE prefiere errors.As sobre type assertion directa             │
│  └─ Uso: extraer *os.PathError, *net.OpError, *CustomError             │
│                                                                          │
│  PATRONES:                                                               │
│  ├─ op-target: fmt.Errorf("read config %s: %w", path, err)             │
│  ├─ Tipo custom con Unwrap(): struct{Cause error} + Unwrap()            │
│  ├─ Error upgrade: error simple → struct con datos + Unwrap()           │
│  ├─ Annotation: sin Unwrap() → cortar cadena intencionalmente           │
│  └─ Custom Is(): matching por campo (Code, no Message)                  │
│                                                                          │
│  CUANDO NO WRAPPEAR:                                                     │
│  ├─ Si no agregas contexto nuevo (return err directo)                   │
│  ├─ Si el error es detalle de implementacion (%v)                       │
│  └─ Si creas un error nuevo desde cero (no hay nada que wrappear)       │
│                                                                          │
│  vs C: no tiene wrapping — solo snprintf manual                         │
│  vs Rust: From trait + ? (compiletime), source() chain, thiserror/anyhow│
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: T04 - Sentinel errors y tipos de error — io.EOF, os.ErrNotExist, errores custom con campos
