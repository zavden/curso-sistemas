# panic y recover — Errores Irrecuperables y su Contencion

## 1. Introduccion

Go tiene dos mecanismos para errores:
- **`error`** — para errores esperados y recuperables (archivo no existe, red caida, input invalido). Es un valor que se retorna y se maneja con `if err != nil`.
- **`panic`** — para errores **del programador** o situaciones verdaderamente irrecuperables (index out of range, nil pointer, estado corrupto). Detiene la ejecucion normal.

La regla de oro en Go:

```
┌──────────────────────────────────────────────────────────────┐
│              panic vs error EN GO                             │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  "Don't panic" — Rob Pike                                    │
│                                                              │
│  Usa ERROR para:                                             │
│  ├─ Archivo no encontrado                                    │
│  ├─ Conexion rechazada / timeout                             │
│  ├─ Input invalido del usuario                               │
│  ├─ Permiso denegado                                         │
│  ├─ JSON malformado                                          │
│  ├─ Base de datos no disponible                              │
│  └─ Cualquier cosa que PUEDA pasar en produccion             │
│                                                              │
│  Usa PANIC para:                                             │
│  ├─ Bug del programador (deberia ser imposible)              │
│  ├─ Violacion de invariantes internas                        │
│  ├─ Estado corrupto que no se puede reparar                  │
│  ├─ Inicializacion que DEBE funcionar (regex.MustCompile)    │
│  └─ Nada mas                                                 │
│                                                              │
│  Si dudas → usa error                                        │
│                                                              │
│  recover:                                                    │
│  ├─ Contiene un panic dentro de un boundary                  │
│  ├─ Previene que un goroutine crashee todo el programa       │
│  ├─ NO es try/catch — no lo uses para control de flujo       │
│  └─ Solo funciona dentro de un defer                         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. panic — Que es y como funciona

`panic` es un builtin que detiene la ejecucion normal de la goroutine actual, ejecuta los defers pendientes (stack unwinding), y termina el programa con un stack trace:

```go
func main() {
    fmt.Println("before panic")
    panic("something went terribly wrong")
    fmt.Println("after panic")  // NUNCA se ejecuta
}
// Output:
// before panic
// goroutine 1 [running]:
// main.main()
//     /path/main.go:5 +0x68
// exit status 2
```

### panic acepta cualquier tipo

```go
// panic toma un argumento de tipo any (interface{})
panic("string message")                    // string
panic(42)                                  // int
panic(errors.New("wrapped error"))         // error
panic(fmt.Errorf("detail: %w", origErr))   // error con wrapping
panic(MyCustomError{Code: 500})            // struct custom

// Convencion: SIEMPRE usa string o error
// Otros tipos hacen el debugging mas dificil
```

### Que sucede durante un panic

```
┌──────────────────────────────────────────────────────────────┐
│  SECUENCIA DE UN PANIC                                       │
│                                                              │
│  1. panic("msg") se llama en funcC()                         │
│  2. Ejecucion normal de funcC() PARA                         │
│  3. Defers de funcC() se ejecutan (LIFO)                     │
│  4. funcC() "retorna" a funcB() (su caller)                  │
│  5. Ejecucion normal de funcB() PARA                         │
│  6. Defers de funcB() se ejecutan (LIFO)                     │
│  7. funcB() "retorna" a funcA()                              │
│  8. ... se repite hasta main() o el top de la goroutine      │
│  9. Defers de main() se ejecutan                             │
│  10. Se imprime el panic message y stack trace               │
│  11. El programa termina con exit status 2                   │
│                                                              │
│  Si ALGUNO de esos defers contiene recover():                │
│  → el unwinding se DETIENE ahi                               │
│  → la funcion que hizo recover() continua normalmente        │
│  → el panic queda "contenido"                                │
│                                                              │
│  funcA()                                                     │
│    └─ funcB()                 ← recover() en un defer aqui   │
│         └─ funcC()                                           │
│              └─ panic("msg")  ← el panic empieza aqui        │
│                                                              │
│  Unwinding: funcC defers → funcB defers → recover() atrapa   │
│  funcB continua despues del punto donde llamo a funcC         │
│  funcA NUNCA se entera del panic                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 3. Panics del runtime (implicitos)

Go genera panics automaticamente en ciertas situaciones. Estos son bugs del programador que el runtime detecta:

```go
// 1. Index out of range
s := []int{1, 2, 3}
fmt.Println(s[5])
// panic: runtime error: index out of range [5] with length 3

// 2. Nil pointer dereference
var p *int
fmt.Println(*p)
// panic: runtime error: invalid memory address or nil pointer dereference

// 3. Nil map write (leer de nil map retorna zero value, no panic)
var m map[string]int
m["key"] = 1
// panic: assignment to entry in nil map

// 4. Cerrar un channel ya cerrado
ch := make(chan int)
close(ch)
close(ch)
// panic: close of closed channel

// 5. Enviar a un channel cerrado
ch := make(chan int)
close(ch)
ch <- 42
// panic: send on closed channel

// 6. Type assertion fallida (sin comma-ok)
var x interface{} = "hello"
n := x.(int)
// panic: interface conversion: interface is string, not int

// 7. Stack overflow (recursion infinita)
func infinite() { infinite() }
infinite()
// fatal error: stack overflow
// NOTA: stack overflow es FATAL — no se puede recover()

// 8. Division por cero (solo enteros — flotantes dan +Inf)
x := 10 / 0
// panic: runtime error: integer divide by zero
// Pero: 10.0 / 0.0 = +Inf (no panic)

// 9. Slice fuera de rango
s := []int{1, 2, 3}
_ = s[1:5]
// panic: runtime error: slice bounds out of range [5] with length 3
```

### Fatal errors vs panics recuperables

```
┌──────────────────────────────────────────────────────────────┐
│  RECUPERABLE con recover():                                  │
│  ├─ Index out of range                                       │
│  ├─ Nil pointer dereference                                  │
│  ├─ Nil map write                                            │
│  ├─ Channel close/send errors                                │
│  ├─ Type assertion failure                                   │
│  ├─ Integer divide by zero                                   │
│  └─ Cualquier panic() explicito                              │
│                                                              │
│  NO RECUPERABLE (fatal errors):                              │
│  ├─ Stack overflow                                           │
│  ├─ Out of memory                                            │
│  ├─ Concurrent map read/write (data race)                    │
│  ├─ Deadlock (all goroutines asleep)                         │
│  └─ cgo segfault                                             │
│                                                              │
│  Los fatal errors terminan el programa INMEDIATAMENTE        │
│  sin ejecutar defers ni permitir recover                     │
└──────────────────────────────────────────────────────────────┘
```

---

## 4. Cuando usar panic — Los casos legitimos

### Caso 1: Must functions — Inicializacion que no puede fallar

```go
// El patron MustXxx es comun en la stdlib:
// Si la inicializacion falla, el programa no puede funcionar

// regexp.MustCompile — panic si el patron es invalido
var logPattern = regexp.MustCompile(`^(\d{4}-\d{2}-\d{2}) (\w+): (.+)$`)
// Si el regex esta mal, es un bug del programador
// Es mejor descubrirlo al iniciar que en produccion a las 3am

// template.Must — panic si el template es invalido
var indexTemplate = template.Must(template.ParseFiles("templates/index.html"))

// Tu propio Must:
func MustOpen(path string) *os.File {
    f, err := os.Open(path)
    if err != nil {
        panic(fmt.Sprintf("MustOpen(%s): %v", path, err))
    }
    return f
}
// SOLO usar en inicializacion (package-level vars, init())
// NUNCA en runtime con input del usuario
```

### Caso 2: Invariantes imposibles (unreachable code)

```go
type Direction int

const (
    North Direction = iota
    South
    East
    West
)

func (d Direction) String() string {
    switch d {
    case North:
        return "N"
    case South:
        return "S"
    case East:
        return "E"
    case West:
        return "W"
    default:
        // Si llegamos aqui, alguien creo un Direction con un valor invalido
        // Esto es un bug del programador, no un error de runtime
        panic(fmt.Sprintf("invalid Direction: %d", d))
    }
}
```

### Caso 3: Violacion de contrato interno (precondiciones)

```go
// Funciones internas donde el caller DEBE garantizar la precondicion
func (b *Buffer) mustHaveSpace(n int) {
    if b.Len()+n > b.Cap() {
        panic(fmt.Sprintf("buffer overflow: need %d bytes, have %d", n, b.Cap()-b.Len()))
    }
}

// En la stdlib: strings.Builder.Grow(-1) hace panic
var sb strings.Builder
sb.Grow(-1)
// panic: strings.Builder.Grow: negative count
```

### Caso 4: Estado corrupto irrecuperable

```go
func (s *Server) handleConnection(conn net.Conn) {
    // Si el estado interno esta corrupto, continuar es peligroso
    if s.state == nil {
        panic("server state is nil — initialization bug")
    }

    // Este panic sera capturado por el recover del server loop
    // El servidor sigue funcionando, pero esta conexion se cierra
}
```

### Caso 5: Funciones que no pueden fallar por contrato de la API

```go
// fmt.Fprintf retorna error, pero escribir a strings.Builder
// nunca puede fallar — panic si lo hace (indica bug en la stdlib)
func (b *MyBuilder) writeFormatted(format string, args ...interface{}) {
    _, err := fmt.Fprintf(&b.buf, format, args...)
    if err != nil {
        // Imposible: strings.Builder.Write nunca retorna error
        panic(fmt.Sprintf("impossible: Fprintf to Builder failed: %v", err))
    }
}
```

---

## 5. Cuando NO usar panic

```go
// ✗ NO uses panic para errores esperados
func readConfig(path string) Config {
    data, err := os.ReadFile(path)
    if err != nil {
        panic(err)  // ✗ El archivo podria no existir — es esperable
    }
    var cfg Config
    if err := json.Unmarshal(data, &cfg); err != nil {
        panic(err)  // ✗ El JSON podria estar mal — es esperable
    }
    return cfg
}

// ✓ Retorna error
func readConfig(path string) (Config, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return Config{}, fmt.Errorf("read config: %w", err)
    }
    var cfg Config
    if err := json.Unmarshal(data, &cfg); err != nil {
        return Config{}, fmt.Errorf("parse config: %w", err)
    }
    return cfg, nil
}
```

```go
// ✗ NO uses panic como try/catch
func divide(a, b int) int {
    defer func() {
        if r := recover(); r != nil {
            log.Println("division error:", r)
        }
    }()
    return a / b  // panic si b == 0
}
// ✗ Esto es abuso de panic/recover como control de flujo

// ✓ Verifica la condicion
func divide(a, b int) (int, error) {
    if b == 0 {
        return 0, errors.New("division by zero")
    }
    return a / b, nil
}
```

```go
// ✗ NO uses panic para validacion de input de usuario
func createUser(name string) {
    if name == "" {
        panic("name is required")  // ✗ esto es input del usuario
    }
}

// ✓ Retorna error
func createUser(name string) error {
    if name == "" {
        return errors.New("name is required")
    }
    // ...
    return nil
}
```

### Decision tree

```
┌──────────────────────────────────────────────────────────────┐
│  ¿Deberia usar panic?                                        │
│                                                              │
│  ¿Es un error que el USUARIO puede causar?                   │
│  └─ Si → return error                                        │
│                                                              │
│  ¿Es un error de red, disco, base de datos?                  │
│  └─ Si → return error                                        │
│                                                              │
│  ¿Es un error que depende de input externo?                  │
│  └─ Si → return error                                        │
│                                                              │
│  ¿Es un bug del PROGRAMADOR?                                 │
│  └─ Si → tal vez panic                                       │
│                                                              │
│  ¿Es una precondicion IMPOSIBLE de violar en uso normal?     │
│  └─ Si → panic                                               │
│                                                              │
│  ¿Es una inicializacion que DEBE funcionar para arrancar?    │
│  └─ Si → panic (MustXxx pattern)                             │
│                                                              │
│  ¿En todo otro caso?                                         │
│  └─ return error                                             │
└──────────────────────────────────────────────────────────────┘
```

---

## 6. recover — Capturar un panic

`recover` es un builtin que **detiene el unwinding** de un panic y retorna el valor que se paso a `panic()`. Si no hay panic en curso, `recover` retorna `nil`.

```go
func safeDiv(a, b int) (result int, err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("recovered: %v", r)
        }
    }()
    return a / b, nil
}

fmt.Println(safeDiv(10, 2))  // 5, <nil>
fmt.Println(safeDiv(10, 0))  // 0, recovered: runtime error: integer divide by zero
```

### La regla de recover: SOLO funciona en un defer

```go
// ✓ recover en defer — funciona
defer func() {
    if r := recover(); r != nil {
        log.Println("caught:", r)
    }
}()

// ✗ recover fuera de defer — SIEMPRE retorna nil
func tryRecover() {
    r := recover()  // siempre nil, incluso si hay panic
    fmt.Println(r)  // nil
    panic("boom")   // no se captura
}

// ✗ recover en una funcion llamada por defer — NO funciona
func handlePanic() {
    r := recover()  // nil — no es llamada directa desde defer
    log.Println(r)
}
func example() {
    defer handlePanic()  // handlePanic se ejecuta en defer,
                         // pero recover() dentro de handlePanic
                         // esta a 2 niveles del defer — no funciona
    panic("boom")        // NO se captura
}

// ✓ recover DIRECTAMENTE en la funcion del defer
func example() {
    defer func() {
        r := recover()  // ✓ directamente en la closure del defer
        log.Println(r)
    }()
    panic("boom")  // se captura
}
```

### Detalle tecnico: recover funciona a exactamente 1 nivel

```go
// recover() solo funciona cuando es llamado DIRECTAMENTE
// por la funcion diferida — no en funciones anidadas dentro del defer

defer func() {
    recover()                    // ✓ funciona (nivel 1)
    func() { recover() }()      // ✗ nil (nivel 2)
}()

// EXCEPCION: Go 1.21+ permite recover en funciones nombradas
// llamadas directamente desde defer
defer myRecoverFunc()  // ✓ si myRecoverFunc llama recover()
// Esto es un cambio reciente — en versiones anteriores no funcionaba
```

---

## 7. El patron idiomatico de recover

```go
// Patron completo y correcto:
func safeOperation() (err error) {
    defer func() {
        if r := recover(); r != nil {
            // Convertir panic a error
            switch v := r.(type) {
            case error:
                err = fmt.Errorf("panic recovered: %w", v)
            case string:
                err = fmt.Errorf("panic recovered: %s", v)
            default:
                err = fmt.Errorf("panic recovered: %v", v)
            }

            // Opcional: log del stack trace para debugging
            log.Printf("PANIC: %v\n%s", r, debug.Stack())
        }
    }()

    // ... operacion que podria hacer panic ...
    return nil
}
```

### Patron SysAdmin: HTTP handler con recovery

```go
func recoveryMiddleware(next http.Handler) http.Handler {
    return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if rec := recover(); rec != nil {
                // Log detallado para diagnostico
                log.Printf("PANIC: %s %s: %v\nStack:\n%s",
                    r.Method, r.URL.Path, rec, debug.Stack())

                // Metricas
                panicCounter.Inc()

                // Response al cliente
                w.Header().Set("Content-Type", "application/json")
                w.WriteHeader(http.StatusInternalServerError)
                json.NewEncoder(w).Encode(map[string]string{
                    "error":  "internal server error",
                    "status": "500",
                })
            }
        }()

        next.ServeHTTP(w, r)
    })
}
```

### Patron: Goroutine con recovery

```go
// Una goroutine que hace panic mata solo esa goroutine
// PERO: si nadie hace recover, mata todo el programa

// ✗ PELIGROSO — panic en goroutine mata el programa
func processJobs(jobs []Job) {
    for _, job := range jobs {
        go func(j Job) {
            j.Execute()  // si esto hace panic, todo el programa muere
        }(job)
    }
}

// ✓ SEGURO — recover en cada goroutine
func processJobs(jobs []Job) {
    var wg sync.WaitGroup
    errCh := make(chan error, len(jobs))

    for _, job := range jobs {
        wg.Add(1)
        go func(j Job) {
            defer wg.Done()
            defer func() {
                if r := recover(); r != nil {
                    errCh <- fmt.Errorf("job %s panicked: %v", j.Name, r)
                }
            }()
            j.Execute()
        }(job)
    }

    wg.Wait()
    close(errCh)

    for err := range errCh {
        log.Println("ERROR:", err)
    }
}
```

---

## 8. panic y recover en la stdlib

### encoding/json — panic interno con recover en boundary

```go
// La stdlib usa panic/recover INTERNAMENTE en algunos parsers
// para simplificar el manejo de errores en codigo recursivo profundo

// Patron simplificado de encoding/json:
type decodeState struct {
    data []byte
    off  int
}

// error interno que nunca escapa del paquete
type jsonError struct {
    msg string
}

func (ds *decodeState) error(msg string) {
    panic(jsonError{msg})  // panic INTERNO — nunca escapa
}

// La funcion publica atrapa el panic y lo convierte en error
func Unmarshal(data []byte, v interface{}) (err error) {
    defer func() {
        if r := recover(); r != nil {
            if je, ok := r.(jsonError); ok {
                err = errors.New(je.msg)  // panic → error normal
            } else {
                panic(r)  // re-panic: no es nuestro panic
            }
        }
    }()

    ds := &decodeState{data: data}
    ds.decode(v)
    return nil
}
```

**Punto clave**: el panic NUNCA escapa del paquete. Es un detalle de implementacion interno. El usuario del paquete solo ve `error`.

### net/http — Recovery del servidor

```go
// El servidor HTTP de Go tiene recovery built-in:
// Si un handler hace panic, el servidor:
// 1. Recupera el panic
// 2. Loggea el stack trace a stderr
// 3. Cierra la conexion
// 4. Sigue sirviendo otras requests

// Esto previene que un bug en un handler crashee todo el servidor
// Pero NO deberias depender de esto — agrega tu propio recovery middleware
```

### regexp.MustCompile

```go
// El ejemplo clasico de panic legitimo
func MustCompile(str string) *Regexp {
    regexp, err := Compile(str)
    if err != nil {
        panic(`regexp: Compile(` + quote(str) + `): ` + err.Error())
    }
    return regexp
}

// Uso en package-level var — panic en init es OK
var emailRegex = regexp.MustCompile(`^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$`)
// Si el regex esta mal, el programa no arranca — fail fast
```

---

## 9. Re-panic — Propagar panics que no son tuyos

Cuando usas recover, **solo debes capturar los panics que esperas**. Si capturas un panic inesperado (bug real), debes re-lanzarlo:

```go
type myInternalError struct {
    msg string
}

func riskyParse(data []byte) (result ParsedData, err error) {
    defer func() {
        if r := recover(); r != nil {
            // ¿Es nuestro error interno?
            if e, ok := r.(myInternalError); ok {
                err = errors.New(e.msg)  // convertir a error
                return
            }
            // NO es nuestro — re-panic
            // Puede ser nil pointer, index out of range, etc.
            // Estos son bugs que deben crashear, no silenciarse
            panic(r)
        }
    }()

    // ...
    return result, nil
}
```

### El peligro de tragar panics

```go
// ✗ TERRIBLE — traga TODOS los panics, incluyendo bugs reales
defer func() {
    recover()  // silencia todo, incluyendo nil pointer dereferences
}()

// ✗ MALO — loggea pero sigue ejecutando con estado potencialmente corrupto
defer func() {
    if r := recover(); r != nil {
        log.Println("panic:", r)
        // y luego... ¿que? El estado puede estar corrupto
    }
}()

// ✓ CORRECTO — recover selectivo con contexto
defer func() {
    if r := recover(); r != nil {
        // Siempre logear el stack trace
        log.Printf("recovered panic: %v\n%s", r, debug.Stack())

        // Convertir a error si es esperado
        if e, ok := r.(error); ok {
            err = fmt.Errorf("operation failed: %w", e)
        } else {
            err = fmt.Errorf("unexpected panic: %v", r)
        }

        // Reportar a metricas/alerting
        panicCounter.WithLabelValues(operationName).Inc()
    }
}()
```

---

## 10. recover y goroutines

Un `recover` en una goroutine **solo puede capturar panics de esa goroutine**. No puede capturar panics de goroutines hijas o de la goroutine padre:

```go
func main() {
    defer func() {
        if r := recover(); r != nil {
            fmt.Println("main recovered:", r)
        }
    }()

    go func() {
        panic("panic in goroutine")
        // Este panic NO es capturado por el recover de main
        // Mata todo el programa
    }()

    time.Sleep(time.Second)
}
// Output: 
// panic: panic in goroutine
// ...crash...
// El recover de main NUNCA se ejecuta para panics en goroutines hijas
```

### Regla: cada goroutine necesita su propio recover

```go
func main() {
    var wg sync.WaitGroup

    for i := 0; i < 5; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            defer func() {
                if r := recover(); r != nil {
                    log.Printf("goroutine %d panicked: %v", id, r)
                }
            }()

            // Si esto hace panic, solo esta goroutine se afecta
            riskyWork(id)
        }(i)
    }

    wg.Wait()
    fmt.Println("all goroutines done (survivors)")
}
```

### Patron SysAdmin: Supervisor de goroutines

```go
type Supervisor struct {
    name    string
    restart bool
    maxCrashes int
}

func (s *Supervisor) Run(ctx context.Context, fn func(ctx context.Context) error) error {
    crashes := 0

    for {
        select {
        case <-ctx.Done():
            return ctx.Err()
        default:
        }

        err := s.runProtected(ctx, fn)

        if err == nil {
            return nil  // termino normalmente
        }

        crashes++
        log.Printf("[%s] crash %d/%d: %v", s.name, crashes, s.maxCrashes, err)

        if !s.restart || crashes >= s.maxCrashes {
            return fmt.Errorf("%s: too many crashes (%d): %w", s.name, crashes, err)
        }

        log.Printf("[%s] restarting...", s.name)
        time.Sleep(time.Duration(crashes) * time.Second)  // backoff
    }
}

func (s *Supervisor) runProtected(ctx context.Context, fn func(ctx context.Context) error) (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic: %v\n%s", r, debug.Stack())
        }
    }()
    return fn(ctx)
}

// Uso:
sup := &Supervisor{name: "worker", restart: true, maxCrashes: 5}
err := sup.Run(ctx, func(ctx context.Context) error {
    return processQueue(ctx)
})
```

---

## 11. Patron: Boundaries de panic

El concepto de "panic boundary" es fundamental para servidores y servicios de larga vida. Un boundary es un punto donde capturas panics para que no se propaguen mas alla:

```go
// ── Boundary en servidor HTTP ──────────────────────────────────
// Cada request es un boundary: un panic en un handler no mata el servidor

func (s *Server) serveRequest(w http.ResponseWriter, r *http.Request) {
    defer func() {
        if rec := recover(); rec != nil {
            s.logPanic(r, rec)
            http.Error(w, "Internal Server Error", 500)
        }
    }()
    s.router.ServeHTTP(w, r)
}

// ── Boundary en worker pool ────────────────────────────────────
// Cada job es un boundary: un panic en un job no mata al worker

func (w *Worker) processJob(job Job) (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("job %s panicked: %v", job.ID, r)
        }
    }()
    return job.Execute()
}

// ── Boundary en plugin system ──────────────────────────────────
// Cada plugin es un boundary: un plugin buggy no mata al host

func (h *Host) callPlugin(plugin Plugin, event Event) (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("plugin %s panicked: %v", plugin.Name(), r)
            log.Printf("PLUGIN PANIC: %s\n%s", plugin.Name(), debug.Stack())
        }
    }()
    return plugin.Handle(event)
}
```

### Donde poner boundaries

```
┌──────────────────────────────────────────────────────────────┐
│  PANIC BOUNDARIES — donde colocarlos                         │
│                                                              │
│  HTTP Server                                                 │
│  └─ boundary: cada request handler                           │
│                                                              │
│  gRPC Server                                                 │
│  └─ boundary: cada RPC handler                               │
│                                                              │
│  Worker Pool                                                 │
│  └─ boundary: cada job/task                                  │
│                                                              │
│  Event Loop                                                  │
│  └─ boundary: cada evento                                    │
│                                                              │
│  Plugin System                                               │
│  └─ boundary: cada llamada a plugin                          │
│                                                              │
│  CLI Tool                                                    │
│  └─ boundary: normalmente NO — dejar que el panic crashee    │
│     para que el usuario vea el stack trace                    │
│                                                              │
│  Library                                                     │
│  └─ boundary: NUNCA exportar panics — convertir a error      │
│     en la API publica                                        │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 12. panic en init() y package-level vars

Los panics durante la inicializacion del programa son un caso especial:

```go
// package-level var — se ejecuta al importar el paquete
var (
    configPath = mustGetEnv("CONFIG_PATH")
    logLevel   = mustGetEnv("LOG_LEVEL")
)

func mustGetEnv(key string) string {
    val := os.Getenv(key)
    if val == "" {
        panic(fmt.Sprintf("required environment variable %s is not set", key))
    }
    return val
}

// init() — se ejecuta despues de las var declarations
func init() {
    if err := loadConfig(configPath); err != nil {
        panic(fmt.Sprintf("failed to load config: %v", err))
    }
}
// Si CONFIG_PATH no esta seteado, el programa no arranca:
// panic: required environment variable CONFIG_PATH is not set
```

### Cuando usar panic en init

```go
// ✓ Panic en init para dependencias OBLIGATORIAS
func init() {
    // La aplicacion no puede funcionar sin base de datos
    db, err := sql.Open("postgres", os.Getenv("DATABASE_URL"))
    if err != nil {
        panic(fmt.Sprintf("database connection failed: %v", err))
    }
    if err = db.Ping(); err != nil {
        panic(fmt.Sprintf("database ping failed: %v", err))
    }
    defaultDB = db
}

// ✗ No usar panic en init de libraries
// Si tu paquete hace panic en init, cualquiera que lo importe
// tiene un panic que no puede controlar
func init() {
    // ✗ Un paquete importado no deberia crashear el importador
    panic("this ruins everyone who imports this package")
}
```

---

## 13. Panic across API boundaries — La regla de las libraries

Una library **NUNCA debe dejar que un panic escape** a su caller. Si una library usa panic internamente (como encoding/json), debe capturarlo y convertirlo a error en la API publica:

```go
// ✗ MAL — panic escapa de la library
package mylib

func Parse(input string) Result {
    if len(input) == 0 {
        panic("empty input")  // el caller recibe un panic inesperado
    }
    // ...
}

// ✓ BIEN — error en la API publica
package mylib

func Parse(input string) (Result, error) {
    if len(input) == 0 {
        return Result{}, errors.New("empty input")
    }
    // ...
}
```

```go
// ✓ ACEPTABLE — panic interno con recover en el boundary
package mylib

type parseError string

func Parse(input string) (result Result, err error) {
    defer func() {
        if r := recover(); r != nil {
            if pe, ok := r.(parseError); ok {
                err = errors.New(string(pe))
            } else {
                panic(r)  // re-panic si no es nuestro
            }
        }
    }()

    result = parse(input)  // funciones internas usan panic(parseError(...))
    return result, nil
}

// Funciones internas — pueden usar panic para simplificar
func parse(input string) Result {
    // ...
    if unexpected {
        panic(parseError("unexpected token at position " + pos))
    }
    // ...
}
```

---

## 14. Testing con panic

### Verificar que una funcion hace panic

```go
func TestMustCompilePanicsOnInvalid(t *testing.T) {
    defer func() {
        if r := recover(); r == nil {
            t.Error("expected panic but none occurred")
        }
    }()

    MustCompile("[invalid regex")
    // Si no hace panic, el test falla
}
```

### Helper generico: assertPanics

```go
func assertPanics(t *testing.T, name string, fn func()) {
    t.Helper()
    defer func() {
        if r := recover(); r == nil {
            t.Errorf("%s: expected panic but none occurred", name)
        }
    }()
    fn()
}

func assertNotPanics(t *testing.T, name string, fn func()) {
    t.Helper()
    defer func() {
        if r := recover(); r != nil {
            t.Errorf("%s: unexpected panic: %v", name, r)
        }
    }()
    fn()
}

// Uso:
func TestDirection(t *testing.T) {
    assertNotPanics(t, "valid direction", func() {
        _ = North.String()
    })

    assertPanics(t, "invalid direction", func() {
        _ = Direction(99).String()
    })
}
```

### Verificar el mensaje del panic

```go
func TestPanicMessage(t *testing.T) {
    defer func() {
        r := recover()
        if r == nil {
            t.Fatal("expected panic")
        }
        msg := fmt.Sprint(r)
        if !strings.Contains(msg, "negative count") {
            t.Errorf("unexpected panic message: %v", r)
        }
    }()

    var sb strings.Builder
    sb.Grow(-1)
}
```

---

## 15. Ejemplo SysAdmin: Process manager con panic boundaries

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "log"
    "math/rand"
    "os"
    "os/signal"
    "runtime/debug"
    "sync"
    "syscall"
    "time"
)

// ── Tipos ──────────────────────────────────────────────────────

type ServiceStatus int

const (
    StatusStopped ServiceStatus = iota
    StatusRunning
    StatusCrashed
    StatusRestarting
)

func (s ServiceStatus) String() string {
    switch s {
    case StatusStopped:
        return "STOPPED"
    case StatusRunning:
        return "RUNNING"
    case StatusCrashed:
        return "CRASHED"
    case StatusRestarting:
        return "RESTARTING"
    default:
        panic(fmt.Sprintf("invalid ServiceStatus: %d", s))
    }
}

type Service struct {
    Name         string
    RunFunc      func(ctx context.Context) error
    RestartPolicy RestartPolicy
}

type RestartPolicy struct {
    MaxRestarts  int
    RestartDelay time.Duration
    MaxDelay     time.Duration
}

type ServiceState struct {
    Status       ServiceStatus
    Crashes      int
    LastError    string
    StartedAt    time.Time
    CrashedAt    time.Time
    PanicStack   string
}

// ── Process Manager ────────────────────────────────────────────

type ProcessManager struct {
    mu       sync.RWMutex
    services map[string]*managedService
    wg       sync.WaitGroup
}

type managedService struct {
    Service
    state    ServiceState
    cancel   context.CancelFunc
}

func NewProcessManager() *ProcessManager {
    return &ProcessManager{
        services: make(map[string]*managedService),
    }
}

func (pm *ProcessManager) Register(svc Service) {
    pm.mu.Lock()
    defer pm.mu.Unlock()

    if svc.RestartPolicy.MaxRestarts == 0 {
        svc.RestartPolicy.MaxRestarts = 3
    }
    if svc.RestartPolicy.RestartDelay == 0 {
        svc.RestartPolicy.RestartDelay = time.Second
    }
    if svc.RestartPolicy.MaxDelay == 0 {
        svc.RestartPolicy.MaxDelay = 30 * time.Second
    }

    pm.services[svc.Name] = &managedService{
        Service: svc,
        state:   ServiceState{Status: StatusStopped},
    }
}

func (pm *ProcessManager) StartAll(ctx context.Context) {
    pm.mu.RLock()
    defer pm.mu.RUnlock()

    for name := range pm.services {
        pm.startService(ctx, name)
    }
}

func (pm *ProcessManager) startService(ctx context.Context, name string) {
    ms := pm.services[name]
    if ms == nil {
        return
    }

    svcCtx, cancel := context.WithCancel(ctx)
    ms.cancel = cancel
    ms.state.Status = StatusRunning
    ms.state.StartedAt = time.Now()

    pm.wg.Add(1)
    go func() {
        defer pm.wg.Done()
        pm.runWithRecovery(svcCtx, ms)
    }()
}

// ── Panic boundary por servicio ────────────────────────────────

func (pm *ProcessManager) runWithRecovery(ctx context.Context, ms *managedService) {
    delay := ms.RestartPolicy.RestartDelay

    for {
        err := pm.runOnce(ctx, ms)

        // Verificar si fue cancelacion normal
        if ctx.Err() != nil {
            pm.mu.Lock()
            ms.state.Status = StatusStopped
            pm.mu.Unlock()
            log.Printf("[%s] stopped (context cancelled)", ms.Name)
            return
        }

        // Hubo error o panic
        pm.mu.Lock()
        ms.state.Crashes++
        ms.state.CrashedAt = time.Now()
        ms.state.Status = StatusCrashed
        if err != nil {
            ms.state.LastError = err.Error()
        }
        crashes := ms.state.Crashes
        pm.mu.Unlock()

        log.Printf("[%s] crashed (attempt %d/%d): %v",
            ms.Name, crashes, ms.RestartPolicy.MaxRestarts, err)

        // Verificar limite de restarts
        if crashes >= ms.RestartPolicy.MaxRestarts {
            log.Printf("[%s] FATAL: max restarts (%d) exceeded — giving up",
                ms.Name, ms.RestartPolicy.MaxRestarts)
            return
        }

        // Backoff antes de reiniciar
        pm.mu.Lock()
        ms.state.Status = StatusRestarting
        pm.mu.Unlock()

        log.Printf("[%s] restarting in %s...", ms.Name, delay)
        select {
        case <-ctx.Done():
            return
        case <-time.After(delay):
        }

        delay *= 2
        if delay > ms.RestartPolicy.MaxDelay {
            delay = ms.RestartPolicy.MaxDelay
        }
    }
}

func (pm *ProcessManager) runOnce(ctx context.Context, ms *managedService) (err error) {
    // ── PANIC BOUNDARY ──
    defer func() {
        if r := recover(); r != nil {
            stack := string(debug.Stack())

            pm.mu.Lock()
            ms.state.PanicStack = stack
            pm.mu.Unlock()

            // Convertir panic a error
            switch v := r.(type) {
            case error:
                err = fmt.Errorf("panic: %w", v)
            default:
                err = fmt.Errorf("panic: %v", v)
            }

            log.Printf("[%s] PANIC RECOVERED:\n%v\nStack:\n%s", ms.Name, r, stack)
        }
    }()

    return ms.RunFunc(ctx)
}

// ── Status report ──────────────────────────────────────────────

func (pm *ProcessManager) Status() map[string]ServiceState {
    pm.mu.RLock()
    defer pm.mu.RUnlock()

    states := make(map[string]ServiceState, len(pm.services))
    for name, ms := range pm.services {
        states[name] = ms.state
    }
    return states
}

func (pm *ProcessManager) PrintStatus() {
    states := pm.Status()

    fmt.Println("\n╔══════════════════════════════════════════════════════════╗")
    fmt.Println("║                   SERVICE STATUS                         ║")
    fmt.Println("╠══════════════════════════════════════════════════════════╣")

    for name, state := range states {
        var indicator string
        switch state.Status {
        case StatusRunning:
            indicator = "\033[32m●\033[0m"
        case StatusCrashed:
            indicator = "\033[31m●\033[0m"
        case StatusRestarting:
            indicator = "\033[33m●\033[0m"
        case StatusStopped:
            indicator = "\033[90m●\033[0m"
        }

        uptime := ""
        if state.Status == StatusRunning && !state.StartedAt.IsZero() {
            uptime = fmt.Sprintf(" (up %s)", time.Since(state.StartedAt).Round(time.Second))
        }

        fmt.Printf("║  %s %-20s %-12s crashes: %d%s\n",
            indicator, name, state.Status, state.Crashes, uptime)

        if state.LastError != "" {
            fmt.Printf("║    └─ last error: %s\n", state.LastError)
        }
    }

    fmt.Println("╚══════════════════════════════════════════════════════════╝")
}

func (pm *ProcessManager) StopAll() {
    pm.mu.RLock()
    for _, ms := range pm.services {
        if ms.cancel != nil {
            ms.cancel()
        }
    }
    pm.mu.RUnlock()

    pm.wg.Wait()
}

// ── Servicios de ejemplo ───────────────────────────────────────

func healthChecker(ctx context.Context) error {
    ticker := time.NewTicker(3 * time.Second)
    defer ticker.Stop()

    for {
        select {
        case <-ctx.Done():
            return nil
        case <-ticker.C:
            log.Println("[health] checking system health...")
            // Simular panic ocasional
            if rand.Float64() < 0.15 {
                panic("nil pointer in health check subsystem")
            }
        }
    }
}

func metricsCollector(ctx context.Context) error {
    ticker := time.NewTicker(5 * time.Second)
    defer ticker.Stop()

    for {
        select {
        case <-ctx.Done():
            return nil
        case <-ticker.C:
            log.Println("[metrics] collecting metrics...")
            // Simular error ocasional
            if rand.Float64() < 0.1 {
                return errors.New("metrics endpoint unreachable")
            }
        }
    }
}

func logRotator(ctx context.Context) error {
    ticker := time.NewTicker(10 * time.Second)
    defer ticker.Stop()

    for {
        select {
        case <-ctx.Done():
            return nil
        case <-ticker.C:
            log.Println("[logrotate] checking log sizes...")
            // Simular crash raro
            if rand.Float64() < 0.05 {
                // Simular index out of range
                s := []int{1, 2, 3}
                _ = s[10]
            }
        }
    }
}

func stableService(ctx context.Context) error {
    ticker := time.NewTicker(4 * time.Second)
    defer ticker.Stop()

    for {
        select {
        case <-ctx.Done():
            return nil
        case <-ticker.C:
            log.Println("[stable] doing stable work...")
        }
    }
}

// ── Main ───────────────────────────────────────────────────────

func main() {
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()

    pm := NewProcessManager()

    pm.Register(Service{
        Name:    "health-checker",
        RunFunc: healthChecker,
        RestartPolicy: RestartPolicy{
            MaxRestarts:  5,
            RestartDelay: time.Second,
            MaxDelay:     15 * time.Second,
        },
    })

    pm.Register(Service{
        Name:    "metrics-collector",
        RunFunc: metricsCollector,
        RestartPolicy: RestartPolicy{
            MaxRestarts:  3,
            RestartDelay: 2 * time.Second,
        },
    })

    pm.Register(Service{
        Name:    "log-rotator",
        RunFunc: logRotator,
        RestartPolicy: RestartPolicy{
            MaxRestarts:  3,
            RestartDelay: time.Second,
        },
    })

    pm.Register(Service{
        Name:    "stable-worker",
        RunFunc: stableService,
    })

    fmt.Println("Starting process manager (Ctrl+C to stop)...")
    pm.StartAll(ctx)

    // Status reporter
    statusTicker := time.NewTicker(8 * time.Second)
    defer statusTicker.Stop()

loop:
    for {
        select {
        case <-ctx.Done():
            break loop
        case <-statusTicker.C:
            pm.PrintStatus()
        }
    }

    fmt.Println("\nStopping all services...")
    pm.StopAll()
    pm.PrintStatus()
    fmt.Println("Process manager stopped")
}
```

---

## 16. Patron: Panic-safe wrappers para operaciones criticas

```go
// Wrapper generico que convierte panic en error
func SafeCall(fn func() error) (err error) {
    defer func() {
        if r := recover(); r != nil {
            switch v := r.(type) {
            case error:
                err = fmt.Errorf("panic: %w", v)
            default:
                err = fmt.Errorf("panic: %v", v)
            }
        }
    }()
    return fn()
}

// Wrapper con timeout y panic protection
func SafeCallWithTimeout(ctx context.Context, timeout time.Duration, fn func(ctx context.Context) error) error {
    ctx, cancel := context.WithTimeout(ctx, timeout)
    defer cancel()

    errCh := make(chan error, 1)
    go func() {
        errCh <- SafeCall(func() error {
            return fn(ctx)
        })
    }()

    select {
    case err := <-errCh:
        return err
    case <-ctx.Done():
        return ctx.Err()
    }
}

// Uso SysAdmin:
err := SafeCallWithTimeout(ctx, 30*time.Second, func(ctx context.Context) error {
    return deployToServer(ctx, server, artifact)
})
if err != nil {
    log.Printf("deploy failed (panic-safe): %v", err)
}
```

---

## 17. Errores comunes

| # | Error | Codigo | Solucion |
|---|---|---|---|
| 1 | Panic para errores esperados | `panic("file not found")` | `return fmt.Errorf(...)` |
| 2 | recover fuera de defer | `r := recover()` en codigo normal | `defer func() { recover() }()` |
| 3 | recover en funcion anidada | `defer func() { func() { recover() }() }()` | recover directo en la funcion del defer |
| 4 | Tragar todos los panics | `defer func() { recover() }()` sin accion | Logear, reportar, o re-panic |
| 5 | No re-panic para panics ajenos | Capturar panic de otro paquete | `if _, ok := r.(myType); !ok { panic(r) }` |
| 6 | Panic en goroutine sin recover | `go func() { panic(...) }()` | Agregar recover en cada goroutine |
| 7 | Depender del recover de net/http | No poner recovery en handlers | Agregar tu propio recovery middleware |
| 8 | panic(nil) | `panic(nil)` — recover retorna nil | `recover() == nil` no distingue de "no panic" |
| 9 | Panic en library publica | `panic()` en funcion exportada | Convertir a error en la API publica |
| 10 | Usar panic/recover como try/catch | Control de flujo con panic | Usar return error |
| 11 | log.Fatal pensando que ejecuta defers | `defer cleanup(); log.Fatal()` | `log.Print(); return` o `run()` wrapper |
| 12 | Ignorar stack trace en recover | `recover()` sin debug.Stack() | Siempre logear `debug.Stack()` |

### Error 8 en detalle — panic(nil)

```go
// Go 1.21 CAMBIO ESTE COMPORTAMIENTO
// Pre Go 1.21:
func example() {
    defer func() {
        r := recover()
        fmt.Println("recovered:", r)           // nil
        fmt.Println("was panic:", r != nil)     // false — ¡indistinguible!
    }()
    panic(nil)  // ← recover retorna nil, parece que no hubo panic
}

// Go 1.21+:
// panic(nil) ahora se convierte internamente en *runtime.PanicNilError
// recover() retorna un *runtime.PanicNilError en lugar de nil
func example() {
    defer func() {
        r := recover()
        fmt.Println("recovered:", r)           // runtime.PanicNilError{...}
        fmt.Println("was panic:", r != nil)     // true — correcto
    }()
    panic(nil)
}

// Moraleja: nunca uses panic(nil) — no tiene sentido semantico
```

---

## 18. Comparacion: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Crash por bug | `panic` | segfault / abort | `panic!` |
| Error recuperable | `return error` | return code / errno | `Result<T, E>` |
| Capturar crash | `recover()` en defer | `signal(SIGSEGV, ...)` (limitado) | `catch_unwind()` |
| Stack unwinding | Si (con defers) | No (abort) / Si (longjmp) | Si (con Drop) / No (abort mode) |
| Scope de captura | Goroutine actual | Proceso | Thread actual |
| Forzar crash | `panic("msg")` | `abort()` / `assert()` | `panic!("msg")` / `unreachable!()` |
| Must pattern | `regexp.MustCompile` | `assert()` | `.unwrap()` / `.expect("msg")` |
| Crash en init | Programa no arranca | `assert` en `main` | `panic!` en `fn main` |
| Stack trace | Automatico con panic | No (manual con backtrace libs) | `RUST_BACKTRACE=1` |
| Cross-goroutine/thread | No captura en otros goroutines | N/A | No captura en otros threads |

### Go panic vs Rust panic!

```go
// Go — panic es recuperable por defecto
func goExample() {
    defer func() {
        if r := recover(); r != nil {
            fmt.Println("recovered:", r)
        }
    }()
    panic("boom")  // siempre recuperable
}
```

```rust
// Rust — panic! puede ser irrecuperable
// Rust tiene dos modos de panic:
// 1. unwind (default) — similar a Go, ejecuta Drop, puede catch_unwind
// 2. abort — termina inmediatamente sin unwinding

// Cargo.toml:
// [profile.release]
// panic = "abort"   ← en release, panics terminan el proceso

use std::panic;

fn rust_example() {
    let result = panic::catch_unwind(|| {
        panic!("boom");
    });
    match result {
        Ok(_) => println!("no panic"),
        Err(e) => println!("caught: {:?}", e),
    }
}
// catch_unwind es el equivalente de recover
// Pero la documentacion dice: "This is NOT a general try/catch mechanism"
```

### C — No tiene panic/recover nativo

```c
// C usa setjmp/longjmp — mucho mas peligroso
#include <setjmp.h>

jmp_buf jump_buffer;

void risky_function() {
    // ... error ...
    longjmp(jump_buffer, 1);  // "panic" — salta a setjmp
    // PELIGRO: no ejecuta cleanup, no libera memoria
}

int main() {
    if (setjmp(jump_buffer) != 0) {
        // "recover" — pero el estado puede estar corrupto
        printf("recovered from error\n");
        return 1;
    }
    risky_function();
    return 0;
}
// longjmp/setjmp es considerado peligroso y raramente usado
// En C moderno, se prefiere return codes + goto cleanup
```

---

## 19. Ejercicios

### Ejercicio 1: Prediccion de panic basico
```go
package main

import "fmt"

func a() {
    defer fmt.Println("defer a")
    fmt.Println("entering a")
    b()
    fmt.Println("leaving a")
}

func b() {
    defer fmt.Println("defer b")
    fmt.Println("entering b")
    panic("panic in b")
    fmt.Println("leaving b")
}

func main() {
    defer fmt.Println("defer main")
    fmt.Println("calling a")
    a()
    fmt.Println("after a")
}
```
**Prediccion**: ¿Que lineas se imprimen antes del stack trace? ¿En que orden? ¿Se imprime "leaving a" o "after a"?

### Ejercicio 2: Prediccion de recover
```go
package main

import "fmt"

func safe() {
    defer func() {
        if r := recover(); r != nil {
            fmt.Println("recovered:", r)
        }
    }()
    fmt.Println("before panic")
    panic("boom")
    fmt.Println("after panic")
}

func main() {
    fmt.Println("before safe")
    safe()
    fmt.Println("after safe")
}
```
**Prediccion**: ¿Se imprime "after panic"? ¿Se imprime "after safe"? ¿Hay stack trace visible?

### Ejercicio 3: Prediccion de recover en goroutine
```go
package main

import (
    "fmt"
    "time"
)

func main() {
    defer func() {
        if r := recover(); r != nil {
            fmt.Println("main recovered:", r)
        }
    }()

    go func() {
        panic("goroutine panic")
    }()

    time.Sleep(time.Second)
    fmt.Println("main done")
}
```
**Prediccion**: ¿El recover de main captura el panic de la goroutine? ¿Se imprime "main done"?

### Ejercicio 4: Prediccion de recover anidado
```go
package main

import "fmt"

func innerRecover() {
    r := recover()
    fmt.Println("inner recover:", r)
}

func main() {
    defer func() {
        innerRecover()
        r := recover()
        fmt.Println("outer recover:", r)
    }()
    panic("test")
}
```
**Prediccion**: ¿`innerRecover()` captura el panic? ¿`recover()` en la funcion anonima lo captura? ¿Que imprime?

### Ejercicio 5: Prediccion de re-panic
```go
package main

import "fmt"

func level1() {
    defer func() {
        if r := recover(); r != nil {
            fmt.Println("level1 recovered:", r)
            panic("re-panic from level1")
        }
    }()
    level2()
}

func level2() {
    panic("original panic")
}

func main() {
    defer func() {
        if r := recover(); r != nil {
            fmt.Println("main recovered:", r)
        }
    }()
    level1()
    fmt.Println("after level1")
}
```
**Prediccion**: ¿Cuantos recovers se ejecutan? ¿Que mensaje captura main? ¿Se imprime "after level1"?

### Ejercicio 6: Prediccion de panic(nil) en Go 1.21+
```go
package main

import "fmt"

func main() {
    defer func() {
        r := recover()
        if r == nil {
            fmt.Println("no panic (or panic(nil))")
        } else {
            fmt.Printf("recovered: %T = %v\n", r, r)
        }
    }()

    panic(nil)
}
```
**Prediccion**: ¿Que imprime en Go 1.21+ vs Go 1.20?

### Ejercicio 7: Panic-safe map access
Implementa una funcion `SafeMapGet[K comparable, V any](m map[K]V, key K) (V, bool)` que:
1. Maneje el caso de map nil sin panic (nota: leer de nil map no hace panic, pero asegurate)
2. Crea una version `MustMapGet` que haga panic si la key no existe
3. Crea una funcion `SafeMapSet` que maneje nil map (esta si hace panic normalmente)
4. Escribe tests para todos los casos edge

### Ejercicio 8: Worker pool con panic boundaries
Implementa un worker pool que:
1. Lance N workers como goroutines
2. Cada worker tenga su propio recover
3. Si un job hace panic, el worker logee el error y continue con el siguiente job (no muere)
4. Reporte al final cuantos jobs tuvieron exito, cuantos fallaron con error, y cuantos hicieron panic
5. Un panic en un job NO debe afectar a otros workers

### Ejercicio 9: Panic-safe config loader
Implementa un sistema de carga de configuracion que:
1. Use `MustGetEnv` para variables REQUERIDAS (DATABASE_URL, etc.) — panic si faltan
2. Use `GetEnvDefault` para variables opcionales (LOG_LEVEL default "info")
3. Valide toda la config en una funcion que haga panic si hay inconsistencias internas
4. Envuelva todo en una funcion `LoadConfig() (Config, error)` con recover en el boundary
5. Los panics internos se convierten en errores descriptivos

### Ejercicio 10: Mini HTTP server con recovery
Implementa un mini servidor HTTP que:
1. Tenga recovery middleware que capture panics en handlers
2. Logee el stack trace completo del panic
3. Retorne JSON con `{"error": "internal server error"}` al cliente
4. Lleve un contador de panics por handler
5. Tenga un endpoint `/debug/panics` que muestre el historial de panics

---

## 20. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│              panic Y recover EN GO — RESUMEN                  │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  panic                                                       │
│  ├─ Detiene ejecucion normal                                 │
│  ├─ Ejecuta defers (LIFO) durante stack unwinding            │
│  ├─ Acepta cualquier tipo (usar string o error)              │
│  ├─ Si nadie hace recover → programa termina (exit 2)        │
│  ├─ Panics del runtime: index OOB, nil ptr, nil map write    │
│  └─ Fatal errors (no recuperables): stack overflow, OOM      │
│                                                              │
│  CUANDO USAR panic                                           │
│  ├─ Bug del programador (codigo imposible de alcanzar)       │
│  ├─ MustXxx pattern (init, regex, templates)                 │
│  ├─ Violacion de invariantes internas                        │
│  ├─ Estado corrupto irrecuperable                            │
│  └─ NUNCA para errores esperados → usar return error         │
│                                                              │
│  recover                                                     │
│  ├─ SOLO funciona dentro de un defer (directamente)          │
│  ├─ Retorna el valor pasado a panic()                        │
│  ├─ Retorna nil si no hay panic en curso                     │
│  ├─ Detiene el unwinding — la funcion que lo contiene        │
│  │   continua normalmente                                    │
│  ├─ Solo captura panics de la misma goroutine                │
│  └─ Go 1.21+: panic(nil) ya es distinguible                 │
│                                                              │
│  PATRONES                                                    │
│  ├─ Panic boundary: recover en el entry point de cada        │
│  │   unidad de trabajo (request, job, plugin, goroutine)     │
│  ├─ Re-panic: capturar solo TUS panics, re-lanzar los demas │
│  ├─ Library rule: NUNCA exportar panics — convertir a error  │
│  ├─ Supervisor: restart con backoff tras panic               │
│  ├─ SafeCall: wrapper generico panic → error                 │
│  └─ Testing: assertPanics / assertNotPanics                  │
│                                                              │
│  GOROUTINES                                                  │
│  ├─ Un panic en goroutine mata TODO el programa              │
│  ├─ recover de la goroutine padre NO lo captura              │
│  └─ Cada goroutine necesita su propio defer+recover          │
│                                                              │
│  STDLIB                                                      │
│  ├─ encoding/json: panic interno con recover en boundary     │
│  ├─ regexp.MustCompile: panic legitimo en init               │
│  ├─ net/http: recovery built-in para handlers                │
│  └─ strings.Builder.Grow(-1): panic por precondicion         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T03 - Comparacion con Rust — defer vs Drop, panic vs panic!, recover vs catch_unwind
