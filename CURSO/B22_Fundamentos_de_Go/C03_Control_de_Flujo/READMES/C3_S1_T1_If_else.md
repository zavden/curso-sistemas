# if/else en Go

## 1. Introducción

El `if/else` de Go es minimalista pero con una característica poderosa que otros lenguajes no tienen: la **init statement** — una declaración opcional que se ejecuta antes de la condición y cuyas variables quedan scopeadas al bloque `if/else`.

Las reglas son pocas y estrictas:
- **Sin paréntesis** alrededor de la condición
- **Llaves obligatorias** siempre (incluso para una sola línea)
- **No hay operador ternario** (`? :`) — Go lo elimina intencionalmente
- La condición debe ser **exactamente `bool`** — no hay truthiness/falsiness

```
┌──────────────────────────────────────────────────────────────┐
│              if/else EN GO                                    │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  if condition {             ← sin paréntesis, llaves oblig.  │
│      // ...                                                  │
│  }                                                           │
│                                                              │
│  if condition {                                              │
│      // ...                                                  │
│  } else {                   ← else en la misma línea que }   │
│      // ...                                                  │
│  }                                                           │
│                                                              │
│  if init; condition {       ← init statement (scope local)   │
│      // init vars aquí                                       │
│  } else {                                                    │
│      // init vars aquí también                               │
│  }                                                           │
│  // init vars NO disponibles aquí                            │
│                                                              │
│  No hay ternario: x = cond ? a : b  ✗                       │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. Sintaxis básica

### if simple

```go
port := 8080

if port < 1024 {
    fmt.Println("privileged port — requires root")
}
```

### if/else

```go
if port < 1024 {
    fmt.Println("privileged port")
} else {
    fmt.Println("unprivileged port")
}
```

### if/else if/else

```go
status := resp.StatusCode

if status >= 200 && status < 300 {
    fmt.Println("success")
} else if status >= 300 && status < 400 {
    fmt.Println("redirect")
} else if status >= 400 && status < 500 {
    fmt.Println("client error")
} else if status >= 500 {
    fmt.Println("server error")
} else {
    fmt.Println("unknown status")
}
```

### Reglas de formato — Enforced por `gofmt`

```go
// ✗ ERROR DE COMPILACIÓN — paréntesis no necesarios (compila, pero gofmt los quita)
if (x > 0) {  // gofmt: remueve paréntesis innecesarios
}

// ✗ ERROR DE COMPILACIÓN — sin llaves
// if x > 0
//     fmt.Println("yes")

// ✗ ERROR DE COMPILACIÓN — else en línea separada
// if x > 0 {
//     fmt.Println("yes")
// }
// else {              // ← syntax error: unexpected else
//     fmt.Println("no")
// }

// ✓ CORRECTO — else en la misma línea que }
if x > 0 {
    fmt.Println("yes")
} else {
    fmt.Println("no")
}
```

---

## 3. Init statement — La característica más importante

Go permite ejecutar una **declaración corta** antes de la condición, separada por punto y coma. Las variables declaradas en el init viven **solo dentro del if/else**:

```go
// Patrón fundamental: declarar err en el scope del if
if err := doSomething(); err != nil {
    log.Printf("error: %v", err)
    return err
}
// err no existe aquí — scope limitado al if/else
```

### Anatomía del init statement

```
if init_statement; condition {
    ↑                ↑
    │                └─ debe ser bool
    └─ declaración corta (:=) o asignación
       Las variables declaradas aquí solo viven en el if/else
}
```

### Ejemplos prácticos

```go
// Abrir archivo — err scopeada al if
if file, err := os.Open("/etc/hosts"); err != nil {
    log.Fatalf("cannot open hosts: %v", err)
} else {
    defer file.Close()
    // file y err disponibles aquí
    data, _ := io.ReadAll(file)
    fmt.Println(string(data))
}
// file y err NO disponibles aquí

// Lookup en map — ok scopeado al if
if val, ok := config["port"]; ok {
    fmt.Printf("port = %s\n", val)
} else {
    fmt.Println("port not configured, using default")
}

// Conversión de tipo — ok scopeado
if n, err := strconv.Atoi(portStr); err == nil {
    server.Port = n
} else {
    log.Printf("invalid port %q: %v, using default", portStr, err)
    server.Port = 8080
}

// Type assertion — ok scopeado
if conn, ok := reader.(net.Conn); ok {
    fmt.Println("remote:", conn.RemoteAddr())
}
```

### Cuándo usar init statement

```go
// ✓ USAR cuando la variable solo importa para la condición
if err := os.MkdirAll(logDir, 0755); err != nil {
    return err
}

// ✓ USAR con map lookups
if handler, exists := routes[path]; exists {
    handler.ServeHTTP(w, r)
}

// ✗ NO USAR cuando necesitas la variable después del if
// ✗ Esto no compila:
if data, err := os.ReadFile(path); err != nil {
    return err
}
// fmt.Println(data)  // ✗ undefined: data

// ✓ Declarar antes cuando necesitas las variables después
data, err := os.ReadFile(path)
if err != nil {
    return err
}
fmt.Println(string(data))  // ✓ data disponible
```

---

## 4. La condición debe ser `bool` — Sin truthiness

Go no tiene el concepto de "truthy" o "falsy". La condición debe ser **exactamente** de tipo `bool`:

```go
// ✗ NINGUNO DE ESTOS COMPILA — Go no tiene truthiness
x := 42
// if x { }              // non-boolean condition
// if x > 0 && x { }    // non-boolean condition

s := "hello"
// if s { }              // non-boolean condition
// if len(s) { }         // non-boolean condition

var p *int
// if p { }              // non-boolean condition

var slice []int
// if slice { }          // non-boolean condition

var err error
// if err { }            // non-boolean condition
```

```go
// ✓ CORRECTO — comparaciones explícitas
if x > 0 { }
if x != 0 { }
if s != "" { }
if len(s) > 0 { }
if p != nil { }
if slice != nil { }
if len(slice) > 0 { }
if err != nil { }
```

### Comparación con otros lenguajes

```
┌──────────────────────────────────────────────────────────────┐
│              TRUTHINESS EN DISTINTOS LENGUAJES               │
├─────────────┬───────────┬───────────┬───────────┬────────────┤
│  Valor      │  Python   │  JS       │  C        │  Go        │
├─────────────┼───────────┼───────────┼───────────┼────────────┤
│  0          │  falsy    │  falsy    │  falsy    │  ✗ no bool │
│  ""         │  falsy    │  falsy    │  N/A      │  ✗ no bool │
│  nil/null   │  falsy    │  falsy    │  falsy    │  ✗ no bool │
│  []         │  falsy    │  truthy!  │  N/A      │  ✗ no bool │
│  42         │  truthy   │  truthy   │  truthy   │  ✗ no bool │
│  "hello"    │  truthy   │  truthy   │  N/A      │  ✗ no bool │
├─────────────┴───────────┴───────────┴───────────┴────────────┤
│  Go: SOLO true y false son condiciones válidas               │
│  Cualquier otra cosa requiere comparación explícita          │
└──────────────────────────────────────────────────────────────┘
```

---

## 5. No hay operador ternario

Go intencionalmente omite el operador ternario `? :`. La razón oficial es legibilidad:

```go
// ✗ NO EXISTE en Go
// result := condition ? valueA : valueB

// ✓ Usar if/else
var result string
if condition {
    result = valueA
} else {
    result = valueB
}

// ✓ Con función helper si lo necesitas mucho (raro)
func ternary[T any](cond bool, a, b T) T {
    if cond {
        return a
    }
    return b
}
// Nota: ambos argumentos se evalúan siempre (no hay short-circuit)

// ✓ Patrón idiomático — inicializar con default y sobreescribir
level := "info"
if debug {
    level = "debug"
}
```

### Patrones que reemplazan al ternario

```go
// Patrón 1: Valor por defecto
host := os.Getenv("HOST")
if host == "" {
    host = "0.0.0.0"
}

// Patrón 2: Return temprano (más idiomático en Go)
func getStatusText(code int) string {
    if code >= 200 && code < 300 {
        return "success"
    }
    if code >= 400 && code < 500 {
        return "client error"
    }
    if code >= 500 {
        return "server error"
    }
    return "unknown"
}

// Patrón 3: Map lookup (para constantes finitas)
var statusText = map[int]string{
    200: "OK",
    404: "Not Found",
    500: "Internal Server Error",
}
```

---

## 6. Early return — El patrón idiomático de Go

Go favorece fuertemente el **early return** sobre el nesting profundo. El "happy path" va alineado a la izquierda:

```go
// ✗ ANTI-PATRÓN — nesting profundo (arrow code)
func processRequest(r *http.Request) error {
    if r.Method == "POST" {
        body, err := io.ReadAll(r.Body)
        if err == nil {
            if len(body) > 0 {
                var data RequestData
                if err := json.Unmarshal(body, &data); err == nil {
                    if data.Valid() {
                        // finalmente, la lógica real
                        return process(data)
                    } else {
                        return fmt.Errorf("invalid data")
                    }
                } else {
                    return fmt.Errorf("bad json: %w", err)
                }
            } else {
                return fmt.Errorf("empty body")
            }
        } else {
            return fmt.Errorf("read error: %w", err)
        }
    } else {
        return fmt.Errorf("method not allowed: %s", r.Method)
    }
}

// ✓ IDIOMÁTICO — early return, happy path a la izquierda
func processRequest(r *http.Request) error {
    if r.Method != "POST" {
        return fmt.Errorf("method not allowed: %s", r.Method)
    }

    body, err := io.ReadAll(r.Body)
    if err != nil {
        return fmt.Errorf("read error: %w", err)
    }

    if len(body) == 0 {
        return fmt.Errorf("empty body")
    }

    var data RequestData
    if err := json.Unmarshal(body, &data); err != nil {
        return fmt.Errorf("bad json: %w", err)
    }

    if !data.Valid() {
        return fmt.Errorf("invalid data")
    }

    return process(data)
}
```

```
┌──────────────────────────────────────────────────────────────┐
│           EARLY RETURN — PRINCIPIOS                          │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. Verificar precondiciones al inicio                       │
│  2. Retornar error inmediatamente si falla                   │
│  3. El happy path fluye hacia abajo sin indentación extra    │
│  4. Evitar else cuando el if retorna                         │
│                                                              │
│  ✗ if err != nil {        ✓ if err != nil {                  │
│        return err              return err                    │
│    } else {                }                                 │
│        // happy path       // happy path (sin else)          │
│    }                                                         │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

### Eliminar `else` innecesario

```go
// ✗ else innecesario después de return
func classify(code int) string {
    if code >= 500 {
        return "server_error"
    } else if code >= 400 {
        return "client_error"
    } else if code >= 200 {
        return "success"
    } else {
        return "unknown"
    }
}

// ✓ Sin else — mismo resultado, más limpio
func classify(code int) string {
    if code >= 500 {
        return "server_error"
    }
    if code >= 400 {
        return "client_error"
    }
    if code >= 200 {
        return "success"
    }
    return "unknown"
}
```

---

## 7. Manejo de errores con `if err != nil`

Este es el patrón más frecuente en todo Go. Entenderlo profundamente es esencial:

```go
// Patrón básico
result, err := someFunction()
if err != nil {
    return fmt.Errorf("context: %w", err)
}
// usar result

// Con init statement (cuando no necesitas result fuera)
if _, err := fmt.Fprintf(w, "data"); err != nil {
    return fmt.Errorf("writing response: %w", err)
}

// Cadena de operaciones
func deploy(ctx context.Context, name string) error {
    config, err := loadConfig(name)
    if err != nil {
        return fmt.Errorf("loading config for %s: %w", name, err)
    }

    image, err := buildImage(config)
    if err != nil {
        return fmt.Errorf("building image for %s: %w", name, err)
    }

    containerID, err := startContainer(ctx, image)
    if err != nil {
        return fmt.Errorf("starting container for %s: %w", name, err)
    }

    if err := healthCheck(ctx, containerID); err != nil {
        return fmt.Errorf("health check for %s: %w", name, err)
    }

    log.Printf("deployed %s as container %s", name, containerID[:12])
    return nil
}
```

### Variantes del manejo de error

```go
// 1. Retornar el error envuelto (lo más común)
if err != nil {
    return fmt.Errorf("opening %s: %w", path, err)
}

// 2. Loguear y retornar
if err != nil {
    log.Printf("WARNING: cannot read config %s: %v", path, err)
    return err
}

// 3. Loguear y continuar (operaciones best-effort)
if err := notifySlack(msg); err != nil {
    log.Printf("slack notification failed: %v", err)
    // no retornamos — la notificación es opcional
}

// 4. Fatal — terminar el programa (solo en main o init)
if err != nil {
    log.Fatalf("FATAL: cannot connect to database: %v", err)
}

// 5. Panic — para errores que no deberían ocurrir
if err != nil {
    panic(fmt.Sprintf("impossible error: %v", err))
}

// 6. Ignorar intencionalmente (rarísimo, documentar por qué)
_ = conn.Close()  // best-effort cleanup, error irrelevante
```

### Errores con contexto progresivo

```go
// Cada nivel agrega contexto — el error final tiene el trace completo
func LoadConfig(path string) (*Config, error) {
    data, err := readFile(path)
    if err != nil {
        return nil, fmt.Errorf("LoadConfig: %w", err)
    }
    // ...
}

func readFile(path string) ([]byte, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, fmt.Errorf("readFile %s: %w", path, err)
    }
    return data, nil
}

// Error resultante:
// "LoadConfig: readFile /etc/app.conf: open /etc/app.conf: permission denied"
// Cada nivel añade contexto sin perder la causa raíz
```

---

## 8. Operadores de comparación y lógicos

### Operadores de comparación

```go
// == != < > <= >=
x := 42
fmt.Println(x == 42)   // true
fmt.Println(x != 0)    // true
fmt.Println(x < 100)   // true
fmt.Println(x >= 42)   // true

// Strings se comparan lexicográficamente (byte a byte)
fmt.Println("abc" < "abd")    // true
fmt.Println("abc" < "abcd")   // true (prefijo < extensión)
fmt.Println("abc" == "abc")   // true

// Comparar con nil (solo tipos nilables)
var p *int
var s []int
var m map[string]int
var err error
fmt.Println(p == nil)    // true
fmt.Println(s == nil)    // true
fmt.Println(m == nil)    // true
fmt.Println(err == nil)  // true

// Structs se comparan campo a campo (si todos los campos son comparables)
type Point struct{ X, Y int }
a := Point{1, 2}
b := Point{1, 2}
fmt.Println(a == b)  // true

// Slices, maps, funciones NO son comparables con ==
// var s1, s2 []int
// fmt.Println(s1 == s2)  // ✗ invalid: slice can only be compared to nil
```

### Operadores lógicos

```go
// && (AND), || (OR), ! (NOT)

// Short-circuit evaluation — Go evalúa de izquierda a derecha
// y se detiene al determinar el resultado

// && se detiene en el primer false
if file != nil && file.IsValid() {
    // file.IsValid() NO se ejecuta si file es nil
}

// || se detiene en el primer true
if isAdmin || hasPermission(user, resource) {
    // hasPermission() NO se ejecuta si isAdmin es true
}

// Esto es seguro gracias al short-circuit:
if p != nil && *p > 0 {
    // *p solo se evalúa si p no es nil
}

if len(s) > 0 && s[0] == '/' {
    // s[0] solo se accede si len > 0
}

if i < len(slice) && slice[i] > threshold {
    // slice[i] solo se accede si i está en rango
}
```

### Precedencia de operadores

```go
// De mayor a menor precedencia:
// !       (NOT)
// && 	   (AND)
// ||      (OR)

// Esto significa:
if a || b && c { }
// equivale a:
if a || (b && c) { }
// NO a:
// if (a || b) && c { }

// Recomendación: usar paréntesis para claridad cuando se mezcla && y ||
if (isRoot || isAdmin) && !isReadOnly {
    // ...
}
```

---

## 9. Comparación de errores — `errors.Is` y `errors.As`

Dentro de bloques `if`, es común comparar errores:

```go
import "errors"

// errors.Is — ¿es este error (o envuelve) el error target?
if errors.Is(err, os.ErrNotExist) {
    fmt.Println("file does not exist")
}

if errors.Is(err, os.ErrPermission) {
    fmt.Println("permission denied")
}

// Funciona con errores envueltos por fmt.Errorf %w
wrapped := fmt.Errorf("loading config: %w", os.ErrNotExist)
fmt.Println(errors.Is(wrapped, os.ErrNotExist))  // true

// ✗ NO usar == para comparar errores envueltos
// if err == os.ErrNotExist { }  // falla con errores envueltos
```

```go
// errors.As — extraer un tipo de error específico
var pathErr *os.PathError
if errors.As(err, &pathErr) {
    fmt.Printf("operation: %s, path: %s, error: %v\n",
        pathErr.Op, pathErr.Path, pathErr.Err)
}

var netErr net.Error
if errors.As(err, &netErr) {
    if netErr.Timeout() {
        fmt.Println("network timeout — retrying...")
    }
}

// Patrón SysAdmin: clasificar errores para decidir acción
func handleError(err error) {
    if errors.Is(err, context.DeadlineExceeded) {
        log.Println("operation timed out")
    } else if errors.Is(err, context.Canceled) {
        log.Println("operation was canceled")
    } else if errors.Is(err, os.ErrNotExist) {
        log.Println("resource not found")
    } else if errors.Is(err, os.ErrPermission) {
        log.Println("insufficient permissions")
    } else {
        log.Printf("unexpected error: %v", err)
    }
}
```

---

## 10. Scope del init statement — Detalle importante

Las variables del init statement viven en todo el bloque `if/else if/else`:

```go
// file disponible en TODO el if/else
if file, err := os.Open(path); err != nil {
    // file y err disponibles aquí
    log.Printf("cannot open %s: %v", path, err)
} else if info, err := file.Stat(); err != nil {
    // file (del primer init), info, y nuevo err disponibles
    file.Close()
    log.Printf("cannot stat %s: %v", path, err)
} else {
    // file, info, y err todos disponibles
    fmt.Printf("%s: %d bytes\n", info.Name(), info.Size())
    file.Close()
}
// Nada de lo anterior existe aquí
```

### Shadowing en init statements

```go
var err error  // err exterior

if data, err := fetchData(); err != nil {
    // ⚠ Este err es NUEVO (shadowed por :=)
    // No modifica el err exterior
    fmt.Println("inner err:", err)
}

fmt.Println("outer err:", err)  // sigue siendo nil
```

```go
// Para modificar el err exterior, no uses := en el init
var err error
var data []byte

if data, err = fetchData(); err != nil {
    // Este err ES el exterior (usamos =, no :=)
    return err
}
// data y err (exterior) disponibles
```

---

## 11. Patrones con `if` en SysAdmin

### Verificar existencia de archivo

```go
func fileExists(path string) bool {
    _, err := os.Stat(path)
    return !errors.Is(err, os.ErrNotExist)
}

// Uso
if fileExists("/etc/nginx/nginx.conf") {
    fmt.Println("nginx is configured")
}

// Patrón más completo — distinguir "no existe" de otros errores
func checkFile(path string) (exists bool, err error) {
    info, err := os.Stat(path)
    if err != nil {
        if errors.Is(err, os.ErrNotExist) {
            return false, nil  // no existe, no es error
        }
        return false, err  // otro error (permisos, etc.)
    }
    _ = info
    return true, nil
}
```

### Verificar permisos

```go
func checkPermissions(path string) error {
    info, err := os.Stat(path)
    if err != nil {
        return fmt.Errorf("stat %s: %w", path, err)
    }

    mode := info.Mode().Perm()

    // Verificar que no sea world-writable
    if mode&0002 != 0 {
        return fmt.Errorf("%s is world-writable (%04o) — security risk", path, mode)
    }

    // Verificar que el owner pueda leer
    if mode&0400 == 0 {
        return fmt.Errorf("%s is not owner-readable (%04o)", path, mode)
    }

    return nil
}
```

### Verificar conectividad

```go
func checkPort(host string, port int, timeout time.Duration) error {
    addr := net.JoinHostPort(host, strconv.Itoa(port))

    if conn, err := net.DialTimeout("tcp", addr, timeout); err != nil {
        return fmt.Errorf("port %d unreachable: %w", port, err)
    } else {
        conn.Close()
        return nil
    }
}

// Verificar múltiples servicios
func checkServices(services map[string]string) map[string]error {
    results := make(map[string]error)
    for name, addr := range services {
        if conn, err := net.DialTimeout("tcp", addr, 3*time.Second); err != nil {
            results[name] = err
        } else {
            conn.Close()
            results[name] = nil
        }
    }
    return results
}
```

### Parse de /proc con validación

```go
func getProcessCount() (int, error) {
    entries, err := os.ReadDir("/proc")
    if err != nil {
        return 0, fmt.Errorf("reading /proc: %w", err)
    }

    count := 0
    for _, entry := range entries {
        // Los directorios numéricos en /proc son PIDs
        if _, err := strconv.Atoi(entry.Name()); err == nil {
            count++
        }
    }
    return count, nil
}

func isProcessRunning(pid int) bool {
    if _, err := os.Stat(fmt.Sprintf("/proc/%d", pid)); err != nil {
        return false
    }
    return true
}
```

---

## 12. Guardas de tipo con `if` — Interfaces

```go
// Verificar que un tipo implementa una interfaz en runtime
func tryClose(v interface{}) error {
    if closer, ok := v.(io.Closer); ok {
        return closer.Close()
    }
    return nil  // no implementa Close, nada que hacer
}

// Verificar múltiples interfaces
func describe(v interface{}) string {
    var parts []string

    if _, ok := v.(io.Reader); ok {
        parts = append(parts, "Reader")
    }
    if _, ok := v.(io.Writer); ok {
        parts = append(parts, "Writer")
    }
    if _, ok := v.(io.Closer); ok {
        parts = append(parts, "Closer")
    }
    if _, ok := v.(fmt.Stringer); ok {
        parts = append(parts, "Stringer")
    }

    if len(parts) == 0 {
        return fmt.Sprintf("%T (no known interfaces)", v)
    }
    return fmt.Sprintf("%T implements: %s", v, strings.Join(parts, ", "))
}
```

---

## 13. Condicionales y concurrencia — `select` + `if`

Cuando trabajas con channels, `select` actúa como un switch para comunicación concurrente. Dentro de los cases se usa `if` normalmente:

```go
func monitor(ctx context.Context, checks <-chan CheckResult, alerts chan<- Alert) {
    for {
        select {
        case <-ctx.Done():
            log.Println("monitor shutting down")
            return

        case result := <-checks:
            if result.Error != nil {
                // Clasificar severidad
                if result.Consecutive > 3 {
                    alerts <- Alert{Severity: Critical, Message: result.Error.Error()}
                } else if result.Consecutive > 1 {
                    alerts <- Alert{Severity: Warning, Message: result.Error.Error()}
                }
            }

        case <-time.After(30 * time.Second):
            log.Println("no checks received in 30s — stale?")
        }
    }
}
```

---

## 14. Patrones avanzados con `if`

### Retry con backoff

```go
func withRetry(maxAttempts int, fn func() error) error {
    var lastErr error

    for attempt := 1; attempt <= maxAttempts; attempt++ {
        if err := fn(); err != nil {
            lastErr = err
            backoff := time.Duration(attempt*attempt) * 100 * time.Millisecond
            log.Printf("attempt %d/%d failed: %v (retrying in %s)",
                attempt, maxAttempts, err, backoff)

            if attempt < maxAttempts {
                time.Sleep(backoff)
            }
        } else {
            if attempt > 1 {
                log.Printf("succeeded on attempt %d", attempt)
            }
            return nil
        }
    }

    return fmt.Errorf("all %d attempts failed, last error: %w", maxAttempts, lastErr)
}

// Uso
err := withRetry(3, func() error {
    return pingService("https://api.example.com/health")
})
```

### Validación de configuración encadenada

```go
func validateConfig(cfg *Config) error {
    if cfg.Host == "" {
        return fmt.Errorf("host is required")
    }

    if cfg.Port < 1 || cfg.Port > 65535 {
        return fmt.Errorf("port must be 1-65535, got %d", cfg.Port)
    }

    if cfg.Port < 1024 {
        // Verificar si somos root
        if os.Getuid() != 0 {
            return fmt.Errorf("port %d requires root privileges", cfg.Port)
        }
    }

    if cfg.TLSEnabled {
        if cfg.CertFile == "" {
            return fmt.Errorf("tls_cert is required when TLS is enabled")
        }
        if cfg.KeyFile == "" {
            return fmt.Errorf("tls_key is required when TLS is enabled")
        }
        if _, err := os.Stat(cfg.CertFile); err != nil {
            return fmt.Errorf("cert file %s: %w", cfg.CertFile, err)
        }
        if _, err := os.Stat(cfg.KeyFile); err != nil {
            return fmt.Errorf("key file %s: %w", cfg.KeyFile, err)
        }
    }

    if cfg.LogLevel != "" {
        validLevels := map[string]bool{
            "debug": true, "info": true, "warning": true, "error": true, "fatal": true,
        }
        if !validLevels[strings.ToLower(cfg.LogLevel)] {
            return fmt.Errorf("invalid log_level %q (valid: debug, info, warning, error, fatal)",
                cfg.LogLevel)
        }
    }

    return nil
}
```

### Feature toggles con variables de entorno

```go
func setupFeatures() {
    if os.Getenv("ENABLE_METRICS") != "" {
        log.Println("Metrics endpoint enabled on /metrics")
        http.Handle("/metrics", metricsHandler())
    }

    if os.Getenv("ENABLE_PPROF") != "" {
        log.Println("Pprof endpoints enabled on /debug/pprof/")
        // net/http/pprof registra handlers en init()
    }

    if certFile := os.Getenv("TLS_CERT"); certFile != "" {
        if keyFile := os.Getenv("TLS_KEY"); keyFile != "" {
            log.Println("TLS enabled")
            enableTLS(certFile, keyFile)
        } else {
            log.Println("WARNING: TLS_CERT set but TLS_KEY missing — TLS disabled")
        }
    }
}
```

---

## 15. Anti-patrones comunes

### Booleano redundante

```go
// ✗ Comparar bool con true/false
if isReady == true {
}
if isReady == false {
}

// ✓ Directo
if isReady {
}
if !isReady {
}
```

### Error checking duplicado

```go
// ✗ Doble negación confusa
if err := doWork(); !(err == nil) {
    return err
}

// ✓ Idiomático
if err := doWork(); err != nil {
    return err
}
```

### Else después de return/break/continue

```go
// ✗ else innecesario
func process(s string) (int, error) {
    if s == "" {
        return 0, fmt.Errorf("empty string")
    } else {  // ← else innecesario porque el if retorna
        return strconv.Atoi(s)
    }
}

// ✓ Sin else
func process(s string) (int, error) {
    if s == "" {
        return 0, fmt.Errorf("empty string")
    }
    return strconv.Atoi(s)
}
```

### Nesting excesivo

```go
// ✗ if/else profundo para múltiples condiciones
if a {
    if b {
        if c {
            doWork()
        }
    }
}

// ✓ Flatten con &&
if a && b && c {
    doWork()
}

// ✓ O early return / continue
if !a {
    return
}
if !b {
    return
}
if !c {
    return
}
doWork()
```

---

## 16. Ejemplo SysAdmin: Pre-flight check de deploy

```go
package main

import (
    "errors"
    "fmt"
    "net"
    "os"
    "os/exec"
    "runtime"
    "strconv"
    "strings"
    "time"
)

type CheckResult struct {
    Name    string
    Passed  bool
    Message string
}

func (r CheckResult) String() string {
    status := "PASS"
    if !r.Passed {
        status = "FAIL"
    }
    return fmt.Sprintf("[%s] %-30s %s", status, r.Name, r.Message)
}

func checkRoot() CheckResult {
    if os.Getuid() == 0 {
        return CheckResult{"Root privileges", true, "running as root"}
    }
    return CheckResult{"Root privileges", false, "not running as root (uid=" + strconv.Itoa(os.Getuid()) + ")"}
}

func checkDiskSpace(path string, minGB float64) CheckResult {
    name := fmt.Sprintf("Disk space (%s)", path)

    // Usar df para obtener espacio disponible
    out, err := exec.Command("df", "-BG", "--output=avail", path).Output()
    if err != nil {
        return CheckResult{name, false, fmt.Sprintf("cannot check: %v", err)}
    }

    lines := strings.Split(strings.TrimSpace(string(out)), "\n")
    if len(lines) < 2 {
        return CheckResult{name, false, "unexpected df output"}
    }

    availStr := strings.TrimSpace(strings.TrimSuffix(lines[1], "G"))
    avail, err := strconv.ParseFloat(availStr, 64)
    if err != nil {
        return CheckResult{name, false, fmt.Sprintf("parse error: %v", err)}
    }

    if avail < minGB {
        return CheckResult{name, false,
            fmt.Sprintf("%.0f GB available, need %.0f GB", avail, minGB)}
    }

    return CheckResult{name, true, fmt.Sprintf("%.0f GB available", avail)}
}

func checkPort(port int) CheckResult {
    name := fmt.Sprintf("Port %d available", port)

    listener, err := net.Listen("tcp", fmt.Sprintf(":%d", port))
    if err != nil {
        // El puerto está ocupado o no tenemos permiso
        if strings.Contains(err.Error(), "permission denied") {
            return CheckResult{name, false, "permission denied (need root for ports < 1024)"}
        }
        return CheckResult{name, false, fmt.Sprintf("port in use: %v", err)}
    }
    listener.Close()

    return CheckResult{name, true, "port is available"}
}

func checkService(name, addr string) CheckResult {
    checkName := fmt.Sprintf("Service %s (%s)", name, addr)

    if conn, err := net.DialTimeout("tcp", addr, 3*time.Second); err != nil {
        return CheckResult{checkName, false, fmt.Sprintf("unreachable: %v", err)}
    } else {
        conn.Close()
        return CheckResult{checkName, true, "reachable"}
    }
}

func checkEnvVar(name string, required bool) CheckResult {
    checkName := fmt.Sprintf("Env var %s", name)

    if val := os.Getenv(name); val != "" {
        // Ocultar valores sensibles
        display := val
        if strings.Contains(strings.ToLower(name), "key") ||
            strings.Contains(strings.ToLower(name), "secret") ||
            strings.Contains(strings.ToLower(name), "password") {
            display = val[:3] + "***"
        }
        return CheckResult{checkName, true, fmt.Sprintf("set (%s)", display)}
    }

    if required {
        return CheckResult{checkName, false, "not set (required)"}
    }
    return CheckResult{checkName, true, "not set (optional)"}
}

func checkCommand(cmd string) CheckResult {
    name := fmt.Sprintf("Command '%s'", cmd)

    if path, err := exec.LookPath(cmd); err != nil {
        return CheckResult{name, false, "not found in PATH"}
    } else {
        return CheckResult{name, true, fmt.Sprintf("found at %s", path)}
    }
}

func checkMemory(minMB int) CheckResult {
    name := "Available memory"

    if runtime.GOOS != "linux" {
        return CheckResult{name, true, "skipped (non-Linux)"}
    }

    data, err := os.ReadFile("/proc/meminfo")
    if err != nil {
        return CheckResult{name, false, fmt.Sprintf("cannot read meminfo: %v", err)}
    }

    for _, line := range strings.Split(string(data), "\n") {
        if strings.HasPrefix(line, "MemAvailable:") {
            fields := strings.Fields(line)
            if len(fields) >= 2 {
                if kb, err := strconv.ParseInt(fields[1], 10, 64); err == nil {
                    mb := kb / 1024
                    if mb < int64(minMB) {
                        return CheckResult{name, false,
                            fmt.Sprintf("%d MB available, need %d MB", mb, minMB)}
                    }
                    return CheckResult{name, true,
                        fmt.Sprintf("%d MB available", mb)}
                }
            }
        }
    }

    return CheckResult{name, false, "cannot parse MemAvailable"}
}

func checkFilePerms(path string, maxMode os.FileMode) CheckResult {
    name := fmt.Sprintf("File perms %s", path)

    info, err := os.Stat(path)
    if err != nil {
        if errors.Is(err, os.ErrNotExist) {
            return CheckResult{name, false, "file not found"}
        }
        return CheckResult{name, false, fmt.Sprintf("stat error: %v", err)}
    }

    actualMode := info.Mode().Perm()
    if actualMode&^maxMode != 0 {
        return CheckResult{name, false,
            fmt.Sprintf("mode %04o is too permissive (max %04o)", actualMode, maxMode)}
    }

    return CheckResult{name, true, fmt.Sprintf("mode %04o (max %04o)", actualMode, maxMode)}
}

func main() {
    fmt.Println("═══════════════════════════════════════════════════")
    fmt.Println("  Pre-flight Deploy Checks")
    fmt.Println("═══════════════════════════════════════════════════")

    checks := []CheckResult{
        // Privilegios
        checkRoot(),

        // Recursos del sistema
        checkDiskSpace("/", 5),
        checkDiskSpace("/var", 2),
        checkMemory(512),

        // Puertos
        checkPort(8080),
        checkPort(443),

        // Dependencias externas
        checkService("PostgreSQL", "localhost:5432"),
        checkService("Redis", "localhost:6379"),
        checkService("DNS", "8.8.8.8:53"),

        // Variables de entorno
        checkEnvVar("APP_ENV", true),
        checkEnvVar("DATABASE_URL", true),
        checkEnvVar("API_KEY", true),
        checkEnvVar("LOG_LEVEL", false),

        // Herramientas necesarias
        checkCommand("docker"),
        checkCommand("git"),
        checkCommand("curl"),

        // Permisos de archivos sensibles
        checkFilePerms("/etc/hosts", 0644),
    }

    // Resultados
    passed, failed := 0, 0
    fmt.Println()
    for _, c := range checks {
        if c.Passed {
            fmt.Printf("  \033[32m%s\033[0m\n", c)
            passed++
        } else {
            fmt.Printf("  \033[31m%s\033[0m\n", c)
            failed++
        }
    }

    fmt.Println()
    fmt.Println("───────────────────────────────────────────────────")
    fmt.Printf("  Total: %d | Passed: %d | Failed: %d\n", passed+failed, passed, failed)
    fmt.Println("═══════════════════════════════════════════════════")

    if failed > 0 {
        fmt.Printf("\n  \033[31m✗ %d check(s) failed — deploy blocked\033[0m\n\n", failed)
        os.Exit(1)
    }
    fmt.Println("\n  \033[32m✓ All checks passed — ready to deploy\033[0m\n")
}
```

---

## 17. Comparación: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Paréntesis | No requeridos | Requeridos | No requeridos |
| Llaves | Obligatorias siempre | Opcionales para 1 línea | Obligatorias siempre |
| Init statement | `if x := f(); x > 0` | No | `if let Some(x) = f()` |
| Condición | Solo `bool` | Cualquier entero (0 = false) | Solo `bool` |
| Ternario | No existe | `cond ? a : b` | No existe (if es expresión) |
| if como expresión | No | No | Sí: `let x = if c { a } else { b };` |
| Pattern matching | No (solo type switch) | No | `if let Pattern = expr` |
| Posición del else | Misma línea que `}` | Libre | Libre |
| Truthiness | No (solo bool) | Sí (0=false, !=0 true) | No (solo bool) |
| Manejo de error | `if err != nil` | Códigos de retorno | `?` operator, `Result` |

---

## 18. Errores comunes

| # | Error | Código | Solución |
|---|---|---|---|
| 1 | Paréntesis innecesarios | `if (x > 0) { }` | `if x > 0 { }` |
| 2 | else en línea separada | `}\nelse {` | `} else {` en misma línea |
| 3 | Condición no bool | `if len(s) { }` | `if len(s) > 0 { }` |
| 4 | Var de init fuera de scope | Usar `file` después del if | Declarar antes del if |
| 5 | Shadow de err en init | `if x, err := f(); err != nil` | Cuidado: `:=` crea nuevo err |
| 6 | else después de return | `if err { return } else {…}` | Eliminar else, código sigue |
| 7 | Nesting excesivo | 4+ niveles de if anidados | Early return / flatten |
| 8 | `==` con errores envueltos | `if err == os.ErrNotExist` | `if errors.Is(err, os.ErrNotExist)` |
| 9 | Bool redundante | `if ready == true` | `if ready` |
| 10 | Ignorar segundo retorno | `val := m["key"]` sin ok | `if val, ok := m["key"]; ok` |
| 11 | `=` en vez de `==` | `if x = 5 { }` | `if x == 5 { }` (Go lo detecta) |
| 12 | Ternario inventado | Func `cond(b,a,b)` evalúa ambos | Usar if/else normal |

---

## 19. Ejercicios

### Ejercicio 1: Predicción de scope
```go
package main

import "fmt"

func main() {
    x := 10

    if x := 20; x > 15 {
        fmt.Println("A:", x)
    } else {
        fmt.Println("B:", x)
    }

    fmt.Println("C:", x)

    if y := x * 2; y > 15 {
        x = y
    }

    fmt.Println("D:", x)
}
```
**Predicción**: ¿Qué imprime A, C y D? ¿Por qué C no es 20?

### Ejercicio 2: Predicción de init statement
```go
package main

import (
    "fmt"
    "strconv"
)

func main() {
    s := "42"

    if n, err := strconv.Atoi(s); err != nil {
        fmt.Println("error:", err)
    } else if n > 100 {
        fmt.Println("big:", n)
    } else {
        fmt.Println("small:", n)
    }

    // fmt.Println(n)   // ¿Compila?
    // fmt.Println(err)  // ¿Compila?
}
```
**Predicción**: ¿Qué imprime? ¿Compilan las líneas comentadas?

### Ejercicio 3: Predicción de short-circuit
```go
package main

import "fmt"

func check(name string) bool {
    fmt.Printf("checking %s\n", name)
    return name != "bad"
}

func main() {
    if check("first") || check("second") {
        fmt.Println("at least one passed")
    }

    fmt.Println("---")

    if check("first") && check("bad") && check("third") {
        fmt.Println("all passed")
    } else {
        fmt.Println("something failed")
    }
}
```
**Predicción**: ¿Cuántas veces se llama `check` en cada bloque? ¿Se ejecuta `check("second")`? ¿Se ejecuta `check("third")`?

### Ejercicio 4: Error handling chain
```go
package main

import (
    "fmt"
    "os"
)

func main() {
    var err error

    if _, err = os.Stat("/etc/hosts"); err != nil {
        fmt.Println("1:", err)
    }

    if _, err = os.Stat("/nonexistent"); err != nil {
        fmt.Println("2:", err)
    }

    if _, err = os.Stat("/etc/passwd"); err != nil {
        fmt.Println("3:", err)
    }

    fmt.Println("final err:", err)
}
```
**Predicción**: ¿Cuáles de los Println 1, 2, 3 se ejecutan? ¿Cuál es el valor final de `err`?

### Ejercicio 5: Truthiness
```go
package main

import "fmt"

func main() {
    var s string
    var n int
    var b bool
    var p *int
    var sl []int

    // ¿Cuáles compilan?
    if s != "" { fmt.Println("s not empty") }
    if n != 0 { fmt.Println("n not zero") }
    if b { fmt.Println("b is true") }
    if p != nil { fmt.Println("p not nil") }
    if len(sl) > 0 { fmt.Println("sl not empty") }

    // ¿Alguno imprime algo?
}
```
**Predicción**: ¿Todos compilan? ¿Alguno imprime algo con los zero values?

### Ejercicio 6: Shadowing accidental
```go
package main

import (
    "fmt"
    "os"
)

func getConfig() (string, error) {
    var config string
    var err error

    if data, err := os.ReadFile("/etc/hostname"); err == nil {
        config = string(data)
    }

    return config, err
}

func main() {
    config, err := getConfig()
    fmt.Printf("config=%q err=%v\n", config, err)
}
```
**Predicción**: Si `/etc/hostname` contiene "myhost\n", ¿qué imprime? ¿Es `err` nil o no? ¿Por qué?

### Ejercicio 7: Pre-flight checker
Implementa un sistema de pre-flight checks que:
1. Verifique que ciertos comandos existen en PATH (`docker`, `git`, `kubectl`)
2. Verifique que ciertas variables de entorno están definidas
3. Verifique que ciertos puertos están disponibles
4. Imprima resultados con `[PASS]`/`[FAIL]` y salga con exit code 1 si algo falla

### Ejercicio 8: Config validator
Implementa `validateConfig(cfg map[string]string) []error` que:
1. Verifique que "host" existe y no está vacío
2. Verifique que "port" existe y es un número entre 1-65535
3. Si "tls" es "true", verifique que "cert" y "key" existen como archivos
4. Si "log_level" existe, verifique que sea uno de: debug, info, warning, error
5. Retorne un slice con todos los errores encontrados (no solo el primero)

### Ejercicio 9: Retry con clasificación de error
Implementa `retryWithClassification(fn func() error) error` que:
1. Reintente hasta 3 veces para errores transitorios (timeout, connection refused)
2. NO reintente para errores permanentes (permission denied, not found)
3. Use backoff exponencial (1s, 2s, 4s) entre reintentos
4. Use `errors.Is` o inspección de mensaje para clasificar

### Ejercicio 10: Health check endpoint
Implementa un health check HTTP handler que:
1. Verifique conectividad a base de datos (simular con net.Dial)
2. Verifique espacio en disco (leer /proc o usar `df`)
3. Verifique carga del sistema (leer /proc/loadavg)
4. Retorne 200 si todo OK, 503 si algo falla
5. El body JSON incluya el estado de cada check

---

## 20. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│              if/else EN GO — RESUMEN                         │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  SINTAXIS                                                    │
│  ├─ if condition { }                                         │
│  ├─ if init; condition { } else { }                          │
│  ├─ Sin paréntesis, llaves obligatorias                      │
│  ├─ else en la misma línea que }                             │
│  └─ No existe operador ternario                              │
│                                                              │
│  INIT STATEMENT                                              │
│  ├─ if err := f(); err != nil { }                            │
│  ├─ if val, ok := m[key]; ok { }                             │
│  ├─ Variables scopeadas al bloque if/else completo           │
│  └─ Cuidado con shadowing de variables externas              │
│                                                              │
│  CONDICIONES                                                 │
│  ├─ Solo bool (sin truthiness)                               │
│  ├─ Short-circuit: && se detiene en false, || en true        │
│  ├─ Precedencia: ! > && > || (usar paréntesis si mezclas)   │
│  └─ errors.Is() / errors.As() para comparar errores         │
│                                                              │
│  ESTILO IDIOMÁTICO                                           │
│  ├─ Early return — happy path a la izquierda                 │
│  ├─ No else después de return/break/continue                 │
│  ├─ if err != nil { return } sin else                        │
│  ├─ Flatten en vez de nesting profundo                       │
│  └─ if ready (no if ready == true)                           │
│                                                              │
│  SYSADMIN: validación de config, checks de conectividad,     │
│  verificación de permisos, pre-flight checks, retry logic    │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T02 - for — el único loop en Go: for clásico, for-range, for condicional, for infinito
