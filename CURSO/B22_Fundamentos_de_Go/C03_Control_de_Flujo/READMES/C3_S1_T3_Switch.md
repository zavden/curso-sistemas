# switch — Decisiones Limpias sin Fall-Through

## 1. Introduccion

El `switch` de Go es radicalmente diferente al de C, Java o JavaScript. La diferencia fundamental: **no hay fall-through por defecto**. Cada `case` termina automaticamente sin necesitar `break`. Si quieres fall-through, tienes que pedirlo explicitamente con la keyword `fallthrough`.

Esta decision de diseno elimina una de las fuentes de bugs mas comunes en C:

```c
// En C — bug clasico:
switch (signal) {
    case SIGTERM:
        log("terminating");
        // ← OLVIDO de break, cae al siguiente case
    case SIGKILL:
        kill_process();  // se ejecuta para SIGTERM tambien!
        break;
}
```

En Go esto es imposible. Cada `case` es un bloque aislado.

```
┌──────────────────────────────────────────────────────────────┐
│              switch EN GO                                     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  1. switch clasico (con expresion):                          │
│     switch expr {                                            │
│     case val1:        ← sin fall-through                     │
│     case val2, val3:  ← multiples valores en un case         │
│     default:          ← opcional, orden no importa           │
│     }                                                        │
│                                                              │
│  2. switch sin expresion (if-else chain):                    │
│     switch {                                                 │
│     case cond1:       ← actua como if/else if/else           │
│     case cond2:                                              │
│     default:                                                 │
│     }                                                        │
│                                                              │
│  3. switch con init statement:                               │
│     switch v := f(); v {                                     │
│     case ...:                                                │
│     }                                                        │
│                                                              │
│  4. type switch:                                             │
│     switch v := x.(type) {                                   │
│     case int:         ← v es int dentro de este case         │
│     case string:      ← v es string aqui                     │
│     }                                                        │
│                                                              │
│  Keywords:                                                   │
│  ├─ fallthrough  → fuerza caida al siguiente case            │
│  ├─ break        → sale del switch (raro, es implicito)      │
│  └─ default      → cuando ningun case coincide               │
│                                                              │
│  Sin parentesis, sin break obligatorio, sin fall-through     │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

## 2. switch con expresion — Forma basica

La forma mas comun: una expresion se evalua y se compara contra cada `case`:

```go
func describeSignal(sig os.Signal) string {
    switch sig {
    case syscall.SIGTERM:
        return "graceful termination requested"
    case syscall.SIGINT:
        return "interrupt (Ctrl+C)"
    case syscall.SIGHUP:
        return "hangup — reload configuration"
    case syscall.SIGUSR1:
        return "user-defined signal 1"
    case syscall.SIGUSR2:
        return "user-defined signal 2"
    case syscall.SIGKILL:
        return "force kill (cannot be caught)"
    default:
        return fmt.Sprintf("unknown signal: %v", sig)
    }
}
```

### Reglas fundamentales

```go
// 1. Sin parentesis alrededor de la expresion
switch status {   // ✓
switch (status) { // compila, pero gofmt quita los parentesis

// 2. Cada case termina automaticamente — NO necesita break
switch x {
case 1:
    fmt.Println("one")
    // ← break implicito aqui
case 2:
    fmt.Println("two")
    // ← break implicito aqui
}

// 3. La expresion y los cases deben ser del mismo tipo
var x int = 5
switch x {
case 5:      // ✓ int literal
case "five": // ✗ ERROR de compilacion: mismatched types
}

// 4. Los cases deben ser constantes O expresiones — Go acepta ambos
// (a diferencia de C que solo acepta constantes)
switch x {
case computeValue():  // ✓ expresiones validas
case y + 1:           // ✓
case 42:              // ✓ constante tambien
}
```

### Orden de evaluacion

```go
// Los cases se evaluan de ARRIBA a ABAJO, izquierda a derecha
// Se detiene en el PRIMER match
switch x {
case expensive1():   // se evalua primero
case expensive2():   // solo si expensive1() no coincidio
case expensive3():   // solo si los anteriores no coincidieron
default:             // solo si ningun case coincidio
}

// Consejo: pon los cases mas frecuentes primero para rendimiento
```

---

## 3. Multiples valores en un case

Un `case` puede listar multiples valores separados por comas — funciona como un OR logico:

```go
func classifyHTTPStatus(code int) string {
    switch code {
    case 200, 201, 202, 204:
        return "success"
    case 301, 302, 307, 308:
        return "redirect"
    case 400, 422:
        return "client error (bad request)"
    case 401, 403:
        return "client error (auth)"
    case 404:
        return "not found"
    case 429:
        return "rate limited"
    case 500, 502, 503, 504:
        return "server error"
    default:
        return fmt.Sprintf("unknown status: %d", code)
    }
}
```

### Patron SysAdmin: Clasificar puertos bien conocidos

```go
func classifyPort(port int) string {
    switch port {
    case 22:
        return "SSH"
    case 25, 465, 587:
        return "SMTP"
    case 53:
        return "DNS"
    case 80, 8080, 8000, 8888:
        return "HTTP"
    case 443, 8443:
        return "HTTPS"
    case 3306:
        return "MySQL"
    case 5432:
        return "PostgreSQL"
    case 6379:
        return "Redis"
    case 27017:
        return "MongoDB"
    case 9090:
        return "Prometheus"
    case 9200, 9300:
        return "Elasticsearch"
    case 2379, 2380:
        return "etcd"
    case 6443:
        return "Kubernetes API"
    case 10250:
        return "Kubelet"
    default:
        if port < 1024 {
            return "privileged (unknown)"
        }
        return "unprivileged (unknown)"
    }
}
```

---

## 4. switch con init statement

Igual que `if`, el `switch` acepta una **init statement** antes de la expresion. Las variables declaradas en ella quedan scopeadas al bloque `switch`:

```go
// switch init; expr { ... }

switch status := checkService("nginx"); status {
case "running":
    log.Println("nginx is healthy")
case "stopped":
    log.Println("nginx is down — attempting restart")
    restartService("nginx")
case "degraded":
    log.Printf("nginx is degraded: %s", status)
    alertTeam("nginx degraded")
default:
    log.Printf("unexpected nginx status: %s", status)
}
// status NO existe aqui — scope limitado al switch
```

### Patron: Error handling con init

```go
switch err := deploy(config); {
case err == nil:
    log.Println("deploy successful")
case errors.Is(err, ErrRollback):
    log.Println("deploy failed, rollback completed")
case errors.Is(err, ErrTimeout):
    log.Println("deploy timed out — manual intervention needed")
    alertOncall(err)
default:
    log.Printf("deploy failed: %v", err)
}
```

### Init con funcion que retorna multiples valores

```go
// No puedes hacer esto directamente:
// switch a, b := f(); a {  // ← solo si switch usa `a`

// Pero si necesitas ambos valores:
switch output, err := exec.Command("systemctl", "is-active", "nginx").Output(); {
case err != nil:
    log.Printf("failed to check nginx: %v", err)
case string(output) == "active\n":
    log.Println("nginx is active")
default:
    log.Printf("nginx status: %s", strings.TrimSpace(string(output)))
}
```

---

## 5. switch sin expresion — El if-else chain limpio

Cuando omites la expresion del `switch`, cada `case` es una condicion booleana independiente. Es equivalente a `switch true { ... }` y funciona como una cadena `if/else if/else` mas legible:

```go
func classifyLoad(load float64, numCPUs int) string {
    ratio := load / float64(numCPUs)

    switch {
    case ratio > 5.0:
        return "CRITICAL — system is thrashing"
    case ratio > 2.0:
        return "HIGH — significant queuing"
    case ratio > 1.0:
        return "ELEVATED — at capacity"
    case ratio > 0.7:
        return "NORMAL — healthy utilization"
    default:
        return "LOW — mostly idle"
    }
}
```

### Por que usar switch sin expresion en lugar de if-else

```go
// if-else chain — funciona pero es mas ruidoso
func categorize(temp float64) string {
    if temp > 90 {
        return "CRITICAL"
    } else if temp > 80 {
        return "WARNING"
    } else if temp > 70 {
        return "ELEVATED"
    } else if temp > 50 {
        return "NORMAL"
    } else {
        return "COOL"
    }
}

// switch sin expresion — mas limpio, alineado, escaneable
func categorize(temp float64) string {
    switch {
    case temp > 90:
        return "CRITICAL"
    case temp > 80:
        return "WARNING"
    case temp > 70:
        return "ELEVATED"
    case temp > 50:
        return "NORMAL"
    default:
        return "COOL"
    }
}
```

Ventajas del switch sin expresion:
- Los cases se alinean visualmente — facil de escanear
- No hay `else if` repetido
- El `default` es mas claro que un `else` final
- Facil de reordenar cases sin ajustar la logica

### Patron SysAdmin: Validacion de configuracion

```go
func validateConfig(cfg Config) error {
    switch {
    case cfg.Port < 1 || cfg.Port > 65535:
        return fmt.Errorf("invalid port: %d (must be 1-65535)", cfg.Port)
    case cfg.Workers < 1:
        return fmt.Errorf("workers must be >= 1, got %d", cfg.Workers)
    case cfg.Workers > runtime.NumCPU()*4:
        return fmt.Errorf("workers %d exceeds 4x CPU count (%d)", cfg.Workers, runtime.NumCPU())
    case cfg.Timeout < time.Second:
        return fmt.Errorf("timeout %s too short (min 1s)", cfg.Timeout)
    case cfg.Timeout > 5*time.Minute:
        return fmt.Errorf("timeout %s too long (max 5m)", cfg.Timeout)
    case cfg.LogLevel != "debug" && cfg.LogLevel != "info" && cfg.LogLevel != "warn" && cfg.LogLevel != "error":
        return fmt.Errorf("invalid log level: %q", cfg.LogLevel)
    case cfg.DataDir == "":
        return errors.New("data directory cannot be empty")
    default:
        return nil
    }
}
```

### Patron SysAdmin: Parsing de /proc/PID/status con switch

```go
func parseProcessStatus(pid int) (ProcessInfo, error) {
    data, err := os.ReadFile(fmt.Sprintf("/proc/%d/status", pid))
    if err != nil {
        return ProcessInfo{}, err
    }

    var info ProcessInfo
    info.PID = pid

    for _, line := range strings.Split(string(data), "\n") {
        parts := strings.SplitN(line, ":", 2)
        if len(parts) != 2 {
            continue
        }
        key := strings.TrimSpace(parts[0])
        val := strings.TrimSpace(parts[1])

        switch key {
        case "Name":
            info.Name = val
        case "State":
            info.State = parseState(val)
        case "PPid":
            info.PPid, _ = strconv.Atoi(val)
        case "Uid":
            fields := strings.Fields(val)
            if len(fields) > 0 {
                info.UID, _ = strconv.Atoi(fields[0])
            }
        case "Gid":
            fields := strings.Fields(val)
            if len(fields) > 0 {
                info.GID, _ = strconv.Atoi(fields[0])
            }
        case "VmRSS":
            // "12345 kB"
            fields := strings.Fields(val)
            if len(fields) > 0 {
                info.MemoryKB, _ = strconv.ParseInt(fields[0], 10, 64)
            }
        case "VmSize":
            fields := strings.Fields(val)
            if len(fields) > 0 {
                info.VirtualKB, _ = strconv.ParseInt(fields[0], 10, 64)
            }
        case "Threads":
            info.Threads, _ = strconv.Atoi(val)
        case "voluntary_ctxt_switches":
            info.VoluntaryCtxSwitches, _ = strconv.ParseInt(val, 10, 64)
        case "nonvoluntary_ctxt_switches":
            info.NonVoluntaryCtxSwitches, _ = strconv.ParseInt(val, 10, 64)
        }
        // Los demas campos se ignoran silenciosamente — no necesitan default
    }

    return info, nil
}

func parseState(s string) string {
    if len(s) == 0 {
        return "unknown"
    }
    // El formato es "S (sleeping)", "R (running)", etc.
    switch s[0] {
    case 'R':
        return "running"
    case 'S':
        return "sleeping"
    case 'D':
        return "disk-sleep (uninterruptible)"
    case 'Z':
        return "zombie"
    case 'T':
        return "stopped"
    case 't':
        return "tracing-stop"
    case 'X', 'x':
        return "dead"
    default:
        return fmt.Sprintf("unknown (%c)", s[0])
    }
}
```

---

## 6. El default

`default` se ejecuta cuando ningun `case` coincide. Es opcional y puede ir en cualquier posicion (pero por convencion va al final):

```go
switch runtime.GOOS {
case "linux":
    fmt.Println("running on Linux")
case "darwin":
    fmt.Println("running on macOS")
case "windows":
    fmt.Println("running on Windows")
case "freebsd", "openbsd", "netbsd":
    fmt.Println("running on BSD")
default:
    fmt.Printf("unsupported OS: %s\n", runtime.GOOS)
}
```

### default al inicio — Valido pero raro

```go
// Esto compila y funciona
switch x {
default:
    fmt.Println("default")
case 1:
    fmt.Println("one")
case 2:
    fmt.Println("two")
}
// Si x == 1, imprime "one" (default no se evalua primero)
// Si x == 99, imprime "default"

// El orden de evaluacion sigue siendo case-by-case de arriba a abajo,
// pero default solo se ejecuta si NINGUN case coincide,
// independientemente de su posicion
```

### Sin default — Perfectamente valido

```go
// A veces no necesitas default
switch day := time.Now().Weekday(); day {
case time.Saturday, time.Sunday:
    log.Println("weekend maintenance window open")
    // Solo actua en fin de semana
}
// Entre semana: no hace nada, y eso esta bien
```

---

## 7. fallthrough — Caer al siguiente case

`fallthrough` fuerza la ejecucion del **siguiente case** sin evaluar su condicion. Es la unica forma de obtener el comportamiento de C:

```go
func describePermLevel(level int) {
    fmt.Printf("Level %d grants: ", level)

    switch level {
    case 3:
        fmt.Print("admin access, ")
        fallthrough
    case 2:
        fmt.Print("write access, ")
        fallthrough
    case 1:
        fmt.Print("read access")
    case 0:
        fmt.Print("no access")
    }
    fmt.Println()
}

// describePermLevel(3) → "Level 3 grants: admin access, write access, read access"
// describePermLevel(2) → "Level 2 grants: write access, read access"
// describePermLevel(1) → "Level 1 grants: read access"
// describePermLevel(0) → "Level 0 grants: no access"
```

### Reglas de fallthrough

```go
// 1. fallthrough DEBE ser la ultima sentencia del case
switch x {
case 1:
    fmt.Println("one")
    fallthrough           // ✓ ultima sentencia
case 2:
    fmt.Println("two")
}

// 2. No puedes poner codigo despues de fallthrough
switch x {
case 1:
    fallthrough
    fmt.Println("one")    // ✗ ERROR: unreachable code
case 2:
    fmt.Println("two")
}

// 3. fallthrough NO evalua la condicion del siguiente case — entra directo
switch 1 {
case 1:
    fmt.Println("one")
    fallthrough
case 99:                  // NO se comprueba si 1 == 99
    fmt.Println("ninety-nine")  // SE EJECUTA por fallthrough
}
// Imprime: one
//          ninety-nine

// 4. fallthrough no puede usarse en el ultimo case ni en default
switch x {
case 1:
    fallthrough
default:
    fmt.Println("end")
    fallthrough  // ✗ ERROR: cannot fallthrough final case
}

// 5. fallthrough salta exactamente UN case — no es acumulativo
// Para caer por multiples, necesitas fallthrough en cada uno
switch x {
case 1:
    fmt.Println("one")
    fallthrough
case 2:
    fmt.Println("two")
    fallthrough     // necesitas otro fallthrough para seguir cayendo
case 3:
    fmt.Println("three")
    // sin fallthrough — se detiene aqui
case 4:
    fmt.Println("four")  // nunca se alcanza desde case 1
}
```

### Cuando usar fallthrough — Casi nunca

```
┌──────────────────────────────────────────────────────────────┐
│  ⚠ fallthrough es una CODE SMELL en Go                       │
│                                                              │
│  En la practica, casi nunca lo necesitas:                     │
│                                                              │
│  En lugar de fallthrough, usa:                               │
│  ├─ case val1, val2:  ← multiples valores en un case         │
│  ├─ Funciones helper  ← logica compartida entre cases        │
│  └─ switch { }        ← condiciones independientes           │
│                                                              │
│  La stdlib de Go usa fallthrough en < 20 lugares              │
│  en ~2 millones de lineas de codigo                           │
│                                                              │
│  Si lo usas, agrega un comentario explicando POR QUE          │
└──────────────────────────────────────────────────────────────┘
```

```go
// ✗ Uso innecesario de fallthrough
switch day {
case "Saturday":
    fallthrough
case "Sunday":
    isWeekend = true
}

// ✓ Usa multiples valores en su lugar
switch day {
case "Saturday", "Sunday":
    isWeekend = true
}
```

### El unico uso semi-legitimo: Acumulacion de capacidades

```go
// Cuando el orden importa y cada nivel AGREGA al siguiente
func setupPermissions(role string) Permissions {
    var perms Permissions

    switch role {
    case "superadmin":
        perms.CanDeleteUsers = true
        fallthrough  // superadmin tiene todo lo de admin +
    case "admin":
        perms.CanManageUsers = true
        perms.CanViewAuditLog = true
        fallthrough  // admin tiene todo lo de editor +
    case "editor":
        perms.CanEdit = true
        fallthrough  // editor tiene todo lo de viewer +
    case "viewer":
        perms.CanView = true
    }

    return perms
}
// setupPermissions("superadmin") → todo en true
// setupPermissions("admin")     → todo excepto CanDeleteUsers
// setupPermissions("viewer")    → solo CanView
```

Aun asi, muchos gophers preferirian:

```go
func setupPermissions(role string) Permissions {
    var perms Permissions

    perms.CanView = true // todos pueden ver

    switch role {
    case "superadmin":
        perms.CanDeleteUsers = true
        perms.CanManageUsers = true
        perms.CanViewAuditLog = true
        perms.CanEdit = true
    case "admin":
        perms.CanManageUsers = true
        perms.CanViewAuditLog = true
        perms.CanEdit = true
    case "editor":
        perms.CanEdit = true
    case "viewer":
        // solo CanView
    }

    return perms
}
// Mas explicito, mas facil de auditar — la repeticion es aceptable
```

---

## 8. break dentro de switch

En Go, `break` dentro de un `switch` sale del `switch` (no del loop que lo rodea). Esto es una fuente comun de confusion:

```go
// break sale del switch, NO del for
for {
    switch event := getEvent(); event {
    case "shutdown":
        break  // ← sale del SWITCH, el for sigue ejecutandose!
    case "process":
        handleEvent()
    }
}

// Para salir del for desde dentro del switch, usa un LABEL:
loop:
for {
    switch event := getEvent(); event {
    case "shutdown":
        break loop  // ← sale del FOR
    case "process":
        handleEvent()
    }
}
```

### Detalle importante: break en for dentro de switch

```go
switch action {
case "scan":
    for _, port := range ports {
        if port > 1024 {
            break  // ← sale del FOR (loop mas interno), no del switch
        }
        scan(port)
    }
    // La ejecucion continua aqui despues del break
    log.Println("scan of privileged ports complete")
}
```

La regla es simple: `break` sale de la estructura de control **mas interna** (`for`, `switch`, o `select`). Si necesitas salir de una estructura exterior, usa labels.

---

## 9. type switch — Polimorfismo en tiempo de ejecucion

El `type switch` permite despachar basado en el **tipo concreto** que contiene una interface. Es la forma idiomatica de hacer type assertions multiples:

```go
func describe(i interface{}) string {
    switch v := i.(type) {
    case int:
        return fmt.Sprintf("integer: %d", v)  // v es int
    case float64:
        return fmt.Sprintf("float: %.2f", v)  // v es float64
    case string:
        return fmt.Sprintf("string: %q (len=%d)", v, len(v))  // v es string
    case bool:
        if v {
            return "boolean: true"
        }
        return "boolean: false"
    case nil:
        return "nil"
    default:
        return fmt.Sprintf("unknown type: %T", v)  // v es interface{}
    }
}
```

### Sintaxis obligatoria

```go
// La variable v toma el tipo concreto en cada case
switch v := x.(type) {  // ← UNICA sintaxis valida para type switch
case int:
    // v es int aqui
case string:
    // v es string aqui
}

// Tambien puedes omitir la variable si no la necesitas
switch x.(type) {
case int:
    fmt.Println("es un int")
case string:
    fmt.Println("es un string")
}

// x DEBE ser un tipo interface — no puedes hacer type switch sobre tipos concretos
var x int = 5
switch x.(type) {  // ✗ ERROR: cannot type switch on non-interface value
```

### Multiples tipos en un case

```go
switch v := val.(type) {
case int, int64, int32:
    // ⚠ Cuando hay multiples tipos, v es interface{} (no int ni int64)
    // No puedes hacer operaciones aritmeticas con v aqui
    fmt.Printf("integer type: %T = %v\n", v, v)
case float32, float64:
    // v es interface{} aqui tambien
    fmt.Printf("float type: %T = %v\n", v, v)
case string:
    // Un solo tipo → v es string
    fmt.Printf("string (len %d): %s\n", len(v), v)
}
```

### Patron SysAdmin: Procesar valores de configuracion heterogeneos

```go
type ConfigValue interface{}  // o `any` en Go 1.18+

type Config struct {
    Values map[string]ConfigValue
}

func (c Config) GetString(key string) (string, error) {
    val, ok := c.Values[key]
    if !ok {
        return "", fmt.Errorf("key %q not found", key)
    }

    switch v := val.(type) {
    case string:
        return v, nil
    case int:
        return strconv.Itoa(v), nil
    case int64:
        return strconv.FormatInt(v, 10), nil
    case float64:
        return strconv.FormatFloat(v, 'f', -1, 64), nil
    case bool:
        return strconv.FormatBool(v), nil
    case nil:
        return "", fmt.Errorf("key %q is nil", key)
    default:
        return "", fmt.Errorf("key %q has unsupported type %T", key, v)
    }
}

func (c Config) GetDuration(key string) (time.Duration, error) {
    val, ok := c.Values[key]
    if !ok {
        return 0, fmt.Errorf("key %q not found", key)
    }

    switch v := val.(type) {
    case time.Duration:
        return v, nil
    case string:
        return time.ParseDuration(v)
    case int:
        return time.Duration(v) * time.Second, nil  // asume segundos
    case float64:
        return time.Duration(v * float64(time.Second)), nil
    default:
        return 0, fmt.Errorf("key %q: cannot convert %T to Duration", key, v)
    }
}
```

---

## 10. type switch con interfaces

El type switch no solo despacha por tipo concreto — tambien puede verificar si un valor satisface una interfaz:

```go
type Healthcheck interface {
    HealthCheck() error
}

type Restartable interface {
    Restart() error
}

type Stoppable interface {
    Stop() error
}

func handleFailure(svc interface{}) {
    // Primero verificar si es Restartable
    switch s := svc.(type) {
    case Restartable:
        log.Printf("restarting %T...", s)
        if err := s.Restart(); err != nil {
            log.Printf("restart failed: %v", err)
        }
    case Stoppable:
        log.Printf("stopping %T (no restart available)...", s)
        if err := s.Stop(); err != nil {
            log.Printf("stop failed: %v", err)
        }
    default:
        log.Printf("cannot handle failure for %T — no restart or stop method", s)
    }
}
```

### Patron: Error handling con type switch

```go
func handleError(err error) {
    if err == nil {
        return
    }

    switch e := err.(type) {
    case *os.PathError:
        log.Printf("path error: op=%s path=%s err=%v", e.Op, e.Path, e.Err)
        // Podria ser permiso denegado, archivo no existe, etc.
        switch {
        case errors.Is(e, os.ErrNotExist):
            log.Println("  → file does not exist")
        case errors.Is(e, os.ErrPermission):
            log.Println("  → permission denied")
        }

    case *net.OpError:
        log.Printf("network error: op=%s addr=%v err=%v", e.Op, e.Addr, e.Err)
        if e.Timeout() {
            log.Println("  → connection timeout")
        }

    case *json.SyntaxError:
        log.Printf("JSON syntax error at byte offset %d: %v", e.Offset, e)

    case *json.UnmarshalTypeError:
        log.Printf("JSON type mismatch: expected %s got %s for field %s",
            e.Type, e.Value, e.Field)

    case *exec.ExitError:
        log.Printf("command failed with exit code %d", e.ExitCode())
        if len(e.Stderr) > 0 {
            log.Printf("  stderr: %s", string(e.Stderr))
        }

    case *url.Error:
        log.Printf("URL error: op=%s url=%s err=%v", e.Op, e.URL, e.Err)
        if e.Timeout() {
            log.Println("  → request timeout")
        }

    default:
        log.Printf("unhandled error type %T: %v", err, err)
    }
}
```

---

## 11. type switch vs type assertions

Comparemos las dos formas de inspeccionar tipos en interfaces:

```go
// ─── Type assertions (una sola) ───
// Utiles cuando esperas UN tipo especifico

val, ok := x.(string)
if ok {
    fmt.Println("es string:", val)
}

// Sin ok — panic si no es string
val := x.(string)  // panic: interface conversion: interface is int, not string

// ─── Type switch (multiples) ───
// Util cuando necesitas despachar a multiples tipos

switch v := x.(type) {
case string:
    // ...
case int:
    // ...
}
```

### Cuando usar cada uno

```
┌─────────────────────────────────────────────────────────────┐
│                                                             │
│  Type assertion (x.(T))      │  Type switch (x.(type))     │
│  ─────────────────────────   │  ─────────────────────────   │
│  Esperas UN tipo             │  Multiples tipos posibles    │
│  if v, ok := x.(T); ok { }  │  switch v := x.(type) {     │
│  Rapido para un solo caso    │  case T1: case T2: ...      │
│  Comun en error handling     │  Comun en parseo generico    │
│  errors.As() es preferido    │  JSON/YAML/config parsing    │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## 12. switch sobre strings — Internals

Go no tiene una keyword `enum`. El switch sobre strings es la alternativa mas comun, y el compilador lo optimiza:

```go
// Para pocos cases: comparaciones secuenciales
// Para muchos cases: el compilador puede generar un jump table basado en hash
// No necesitas preocuparte por esto — es automatico

func parseLogLevel(s string) slog.Level {
    switch strings.ToLower(s) {
    case "debug":
        return slog.LevelDebug
    case "info":
        return slog.LevelInfo
    case "warn", "warning":
        return slog.LevelWarn
    case "error", "err":
        return slog.LevelError
    default:
        return slog.LevelInfo  // default seguro
    }
}
```

### Patron: Dispatcher de comandos CLI

```go
func dispatch(args []string) error {
    if len(args) < 1 {
        return errors.New("no command specified")
    }
    cmd := args[0]
    cmdArgs := args[1:]

    switch cmd {
    case "status":
        return cmdStatus(cmdArgs)
    case "start":
        return cmdStart(cmdArgs)
    case "stop":
        return cmdStop(cmdArgs)
    case "restart":
        return cmdRestart(cmdArgs)
    case "logs":
        return cmdLogs(cmdArgs)
    case "config":
        return cmdConfig(cmdArgs)
    case "deploy":
        return cmdDeploy(cmdArgs)
    case "rollback":
        return cmdRollback(cmdArgs)
    case "help", "-h", "--help":
        printUsage()
        return nil
    case "version", "-v", "--version":
        printVersion()
        return nil
    default:
        return fmt.Errorf("unknown command: %q\nRun 'tool help' for usage", cmd)
    }
}
```

### Alternativa: Map de funciones (cuando hay muchos comandos)

```go
// Cuando el switch se vuelve demasiado largo (>15 cases),
// considera un map de funciones:

type CommandFunc func(args []string) error

var commands = map[string]CommandFunc{
    "status":   cmdStatus,
    "start":    cmdStart,
    "stop":     cmdStop,
    "restart":  cmdRestart,
    "logs":     cmdLogs,
    "config":   cmdConfig,
    "deploy":   cmdDeploy,
    "rollback": cmdRollback,
}

func dispatch(args []string) error {
    if len(args) < 1 {
        return errors.New("no command specified")
    }

    fn, ok := commands[args[0]]
    if !ok {
        return fmt.Errorf("unknown command: %q", args[0])
    }
    return fn(args[1:])
}
// Ventaja: facil de extender sin tocar la logica de dispatch
// Desventaja: pierde el analisis estatico que el compilador hace en switch
```

---

## 13. switch y constantes con iota

El `switch` sobre constantes `iota` es la forma idiomatica de simular enums en Go:

```go
type ServiceState int

const (
    StateUnknown  ServiceState = iota  // 0
    StateStopped                        // 1
    StateStarting                       // 2
    StateRunning                        // 3
    StateStopping                       // 4
    StateFailed                         // 5
)

func (s ServiceState) String() string {
    switch s {
    case StateUnknown:
        return "UNKNOWN"
    case StateStopped:
        return "STOPPED"
    case StateStarting:
        return "STARTING"
    case StateRunning:
        return "RUNNING"
    case StateStopping:
        return "STOPPING"
    case StateFailed:
        return "FAILED"
    default:
        return fmt.Sprintf("ServiceState(%d)", int(s))
    }
}

func (s ServiceState) CanTransitionTo(next ServiceState) bool {
    switch s {
    case StateStopped:
        return next == StateStarting
    case StateStarting:
        return next == StateRunning || next == StateFailed
    case StateRunning:
        return next == StateStopping
    case StateStopping:
        return next == StateStopped || next == StateFailed
    case StateFailed:
        return next == StateStarting || next == StateStopped
    default:
        return false
    }
}
```

### El compilador NO verifica exhaustividad

```go
// A diferencia de Rust (match exhaustivo) o Java (sealed switch),
// Go NO te obliga a cubrir todos los valores de un "enum":

switch state {
case StateRunning:
    // ...
case StateStopped:
    // ...
// ← No hay error si olvidas StateStarting, StateFailed, etc.
}

// Solucion: usa golangci-lint con el linter "exhaustive"
// Este linter detecta switch statements que no cubren todos los valores
// de un tipo enum definido con iota

// .golangci.yml:
// linters:
//   enable:
//     - exhaustive
```

---

## 14. Patron: State machine con switch

Uno de los usos mas potentes del switch en SysAdmin — maquinas de estado para gestionar servicios, deploys, o protocolos:

```go
type DeployPhase int

const (
    PhaseInit DeployPhase = iota
    PhaseValidate
    PhaseBuild
    PhaseTest
    PhaseDeploy
    PhaseVerify
    PhaseDone
    PhaseFailed
)

type DeployPipeline struct {
    Phase    DeployPhase
    Config   DeployConfig
    Error    error
    Artifact string
    StartAt  time.Time
}

func (p *DeployPipeline) Run(ctx context.Context) error {
    p.StartAt = time.Now()
    p.Phase = PhaseInit

    for p.Phase != PhaseDone && p.Phase != PhaseFailed {
        select {
        case <-ctx.Done():
            p.Error = ctx.Err()
            p.Phase = PhaseFailed
            continue
        default:
        }

        switch p.Phase {
        case PhaseInit:
            log.Println("[1/6] Initializing deploy...")
            if err := p.loadConfig(); err != nil {
                p.fail(fmt.Errorf("init: %w", err))
                continue
            }
            p.Phase = PhaseValidate

        case PhaseValidate:
            log.Println("[2/6] Validating configuration...")
            if err := p.validate(); err != nil {
                p.fail(fmt.Errorf("validate: %w", err))
                continue
            }
            p.Phase = PhaseBuild

        case PhaseBuild:
            log.Println("[3/6] Building artifact...")
            artifact, err := p.build(ctx)
            if err != nil {
                p.fail(fmt.Errorf("build: %w", err))
                continue
            }
            p.Artifact = artifact
            p.Phase = PhaseTest

        case PhaseTest:
            log.Println("[4/6] Running tests...")
            if err := p.test(ctx); err != nil {
                p.fail(fmt.Errorf("test: %w", err))
                continue
            }
            p.Phase = PhaseDeploy

        case PhaseDeploy:
            log.Printf("[5/6] Deploying %s...", p.Artifact)
            if err := p.deploy(ctx); err != nil {
                log.Println("deploy failed — rolling back...")
                if rbErr := p.rollback(ctx); rbErr != nil {
                    p.fail(fmt.Errorf("deploy: %w; rollback also failed: %v", err, rbErr))
                    continue
                }
                p.fail(fmt.Errorf("deploy: %w (rollback successful)", err))
                continue
            }
            p.Phase = PhaseVerify

        case PhaseVerify:
            log.Println("[6/6] Verifying deployment...")
            if err := p.verify(ctx); err != nil {
                log.Println("verification failed — rolling back...")
                _ = p.rollback(ctx)
                p.fail(fmt.Errorf("verify: %w", err))
                continue
            }
            p.Phase = PhaseDone
            elapsed := time.Since(p.StartAt)
            log.Printf("Deploy completed successfully in %s", elapsed.Round(time.Second))
        }
    }

    return p.Error
}

func (p *DeployPipeline) fail(err error) {
    p.Error = err
    p.Phase = PhaseFailed
    log.Printf("FAILED: %v", err)
}
```

---

## 15. Patron: Clasificador de eventos del sistema

```go
type EventSeverity int

const (
    SevInfo EventSeverity = iota
    SevWarning
    SevError
    SevCritical
)

type SystemEvent struct {
    Source    string
    Message  string
    Severity EventSeverity
    Time     time.Time
}

func classifyEvent(source, message string) SystemEvent {
    event := SystemEvent{
        Source:  source,
        Message: message,
        Time:    time.Now(),
    }

    // switch sin expresion para logica compleja de clasificacion
    switch {
    // Eventos criticos
    case strings.Contains(message, "OOM"):
        event.Severity = SevCritical
    case strings.Contains(message, "kernel panic"):
        event.Severity = SevCritical
    case strings.Contains(message, "disk full"):
        event.Severity = SevCritical
    case strings.Contains(message, "segfault"):
        event.Severity = SevCritical

    // Errores
    case strings.Contains(message, "error"):
        event.Severity = SevError
    case strings.Contains(message, "failed"):
        event.Severity = SevError
    case strings.Contains(message, "refused"):
        event.Severity = SevError

    // Advertencias
    case strings.Contains(message, "warning"):
        event.Severity = SevWarning
    case strings.Contains(message, "deprecated"):
        event.Severity = SevWarning
    case strings.Contains(message, "slow"):
        event.Severity = SevWarning
    case strings.Contains(message, "retry"):
        event.Severity = SevWarning

    // Todo lo demas
    default:
        event.Severity = SevInfo
    }

    return event
}
```

### Router de alertas

```go
func routeAlert(event SystemEvent) {
    switch event.Severity {
    case SevCritical:
        // Alerta inmediata a oncall
        sendPagerDuty(event)
        sendSlack("#incidents", event)
        log.Printf("CRITICAL [%s]: %s", event.Source, event.Message)

    case SevError:
        // Slack + log
        sendSlack("#alerts", event)
        log.Printf("ERROR [%s]: %s", event.Source, event.Message)

    case SevWarning:
        // Solo log + metricas
        incrementMetric("warnings_total", event.Source)
        log.Printf("WARN [%s]: %s", event.Source, event.Message)

    case SevInfo:
        // Solo metricas
        incrementMetric("events_total", event.Source)
    }
}
```

---

## 16. Patron: Parser de formatos heterogeneos

Un uso comun en herramientas SysAdmin — parsear archivos con formatos mixtos:

```go
// Parser de /etc/fstab
type FstabEntry struct {
    Device     string
    MountPoint string
    FSType     string
    Options    []string
    Dump       int
    Pass       int
}

func parseFstab(path string) ([]FstabEntry, error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return nil, err
    }

    var entries []FstabEntry
    for lineNum, line := range strings.Split(string(data), "\n") {
        line = strings.TrimSpace(line)

        // Clasificar tipo de linea con switch
        switch {
        case line == "":
            continue  // linea vacia
        case strings.HasPrefix(line, "#"):
            continue  // comentario
        default:
            entry, err := parseFstabLine(line)
            if err != nil {
                return nil, fmt.Errorf("line %d: %w", lineNum+1, err)
            }
            entries = append(entries, entry)
        }
    }
    return entries, nil
}

func parseFstabLine(line string) (FstabEntry, error) {
    fields := strings.Fields(line)
    if len(fields) < 4 {
        return FstabEntry{}, fmt.Errorf("expected at least 4 fields, got %d", len(fields))
    }

    entry := FstabEntry{
        Device:     fields[0],
        MountPoint: fields[1],
        FSType:     fields[2],
        Options:    strings.Split(fields[3], ","),
    }

    if len(fields) > 4 {
        entry.Dump, _ = strconv.Atoi(fields[4])
    }
    if len(fields) > 5 {
        entry.Pass, _ = strconv.Atoi(fields[5])
    }

    // Validar tipo de filesystem
    switch entry.FSType {
    case "ext4", "xfs", "btrfs", "zfs":
        // Filesystems locales — OK
    case "nfs", "nfs4", "cifs", "glusterfs":
        // Filesystems de red
        if !strings.Contains(entry.Device, ":") && entry.FSType != "cifs" {
            return FstabEntry{}, fmt.Errorf("network fs %s device should contain ':'", entry.FSType)
        }
    case "tmpfs", "devtmpfs", "sysfs", "proc", "devpts", "cgroup2":
        // Filesystems virtuales — OK
    case "swap":
        if entry.MountPoint != "none" && entry.MountPoint != "swap" {
            return FstabEntry{}, fmt.Errorf("swap mount point should be 'none', got %q", entry.MountPoint)
        }
    case "vfat", "ntfs", "exfat":
        // Filesystems de intercambio con Windows
    default:
        // Desconocido pero no necesariamente invalido
        log.Printf("warning: unknown filesystem type: %s", entry.FSType)
    }

    return entry, nil
}
```

---

## 17. Patron: Procesar output de comandos con switch

```go
// Parser de output de systemctl
type UnitInfo struct {
    Name       string
    LoadState  string
    ActiveState string
    SubState   string
    Description string
}

func parseSystemctlOutput(output string) []UnitInfo {
    var units []UnitInfo

    for _, line := range strings.Split(output, "\n") {
        fields := strings.Fields(line)
        if len(fields) < 4 {
            continue
        }

        unit := UnitInfo{
            Name:        fields[0],
            LoadState:   fields[1],
            ActiveState: fields[2],
            SubState:    fields[3],
        }
        if len(fields) > 4 {
            unit.Description = strings.Join(fields[4:], " ")
        }

        units = append(units, unit)
    }

    return units
}

func reportServiceHealth(units []UnitInfo) {
    var failed, degraded, healthy int

    for _, unit := range units {
        switch unit.ActiveState {
        case "active":
            healthy++
            switch unit.SubState {
            case "running":
                // completamente saludable
            case "exited":
                // saludable pero es un oneshot que ya termino
                log.Printf("  [oneshot] %s (exited normally)", unit.Name)
            case "waiting":
                log.Printf("  [waiting] %s", unit.Name)
            }
        case "inactive":
            // No es un error — puede ser un servicio deshabilitado
            log.Printf("  [inactive] %s", unit.Name)
        case "failed":
            failed++
            log.Printf("  [FAILED] %s (sub: %s)", unit.Name, unit.SubState)
        case "activating":
            degraded++
            log.Printf("  [starting] %s", unit.Name)
        case "deactivating":
            degraded++
            log.Printf("  [stopping] %s", unit.Name)
        default:
            degraded++
            log.Printf("  [%s] %s", unit.ActiveState, unit.Name)
        }
    }

    fmt.Printf("\nService summary: %d healthy, %d degraded, %d failed (total: %d)\n",
        healthy, degraded, failed, len(units))

    switch {
    case failed > 0:
        fmt.Println("STATUS: CRITICAL — failed services detected")
    case degraded > 0:
        fmt.Println("STATUS: WARNING — services in transition")
    default:
        fmt.Println("STATUS: OK — all services healthy")
    }
}
```

---

## 18. switch vs map — Cuando usar cual

```
┌──────────────────────────────────────────────────────────────────┐
│  SWITCH vs MAP — Guia de decision                                │
├──────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Usa SWITCH cuando:                                              │
│  ├─ Los cases tienen logica diferente (no solo retornar valor)   │
│  ├─ Necesitas type switch (.(type))                              │
│  ├─ Quieres que el compilador verifique tipos                    │
│  ├─ Los cases son pocos (<15)                                    │
│  ├─ Necesitas fallthrough o logica condicional entre cases       │
│  └─ Es una state machine o dispatcher con side effects           │
│                                                                  │
│  Usa MAP cuando:                                                 │
│  ├─ Todos los cases hacen lo mismo: key → valor                  │
│  ├─ El mapeo es grande (>15 entries)                             │
│  ├─ Los entries se cargan desde config/DB                        │
│  ├─ Necesitas iterar sobre las keys                              │
│  └─ El mapeo puede cambiar en runtime                            │
│                                                                  │
│  Performance:                                                    │
│  ├─ switch: O(n) lineal (pero el compilador puede optimizar     │
│  │          a jump table para constantes)                        │
│  └─ map:    O(1) promedio (hash lookup)                          │
│  ├─ Para <20 cases: switch es mas rapido (no hay hash overhead)  │
│  └─ Para >20 cases: map es mas rapido                            │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘
```

```go
// switch — logica diferente por case
func handleSignal(sig os.Signal) {
    switch sig {
    case syscall.SIGTERM:
        gracefulShutdown()      // logica A
    case syscall.SIGHUP:
        reloadConfig()          // logica B
    case syscall.SIGUSR1:
        dumpGoroutines()        // logica C
    }
}

// map — mismo patron (key → valor)
var signalNames = map[syscall.Signal]string{
    syscall.SIGTERM: "SIGTERM",
    syscall.SIGINT:  "SIGINT",
    syscall.SIGHUP:  "SIGHUP",
    syscall.SIGUSR1: "SIGUSR1",
    syscall.SIGUSR2: "SIGUSR2",
    syscall.SIGKILL: "SIGKILL",
}
```

---

## 19. Comparacion: Go vs C vs Rust

| Aspecto | Go | C | Rust |
|---|---|---|---|
| Fall-through | No por defecto | Si por defecto | No existe (match) |
| Forzar fall-through | `fallthrough` keyword | Comportamiento default | No aplica |
| Prevenir fall-through | Automatico | `break` manual | No aplica |
| Switch sin expresion | `switch { }` | No | No (usar `match` con guardas) |
| Type switch | `switch v := x.(type)` | No | `match` con pattern matching |
| Multiples valores | `case a, b, c:` | Requiere fall-through | `pat1 \| pat2 \| pat3` |
| Exhaustividad | No verificada* | No verificada | **Verificada en compile time** |
| Patterns complejos | No | No | Destructuring, ranges, bindings |
| String switch | Si, directo | No (requiere if-else/strcmp) | Si, con `match` |
| Init statement | `switch v := f(); v {}` | No | No (usar `let` antes) |
| Scope de variables | Cada case es un bloque | Compartido entre cases | Cada arm es un bloque |
| break | Sale del switch | Sale del switch | No aplica (match retorna) |

*Con `exhaustive` linter de golangci-lint

### Equivalencias practicas

```go
// Go — switch con expresion
switch status {
case 200:
    fmt.Println("OK")
case 404:
    fmt.Println("Not Found")
default:
    fmt.Println("Other")
}
```

```c
// C — requiere break en cada case
switch (status) {
    case 200:
        printf("OK\n");
        break;           // ← obligatorio
    case 404:
        printf("Not Found\n");
        break;           // ← obligatorio
    default:
        printf("Other\n");
        break;
}
```

```rust
// Rust — match es una expresion que retorna valor
let msg = match status {
    200 => "OK",
    404 => "Not Found",
    _   => "Other",    // _ es obligatorio (exhaustivo)
};
println!("{}", msg);
```

### Switch sin expresion vs match con guardas

```go
// Go
switch {
case temp > 100:
    return "boiling"
case temp > 0:
    return "liquid"
default:
    return "frozen"
}
```

```rust
// Rust
match temp {
    t if t > 100 => "boiling",
    t if t > 0   => "liquid",
    _            => "frozen",
}
```

### Type switch vs enum + match

```go
// Go — type switch sobre interface
switch v := shape.(type) {
case Circle:
    return math.Pi * v.Radius * v.Radius
case Rectangle:
    return v.Width * v.Height
default:
    return 0  // no hay garantia de exhaustividad
}
```

```rust
// Rust — match sobre enum (exhaustivo)
match shape {
    Shape::Circle { radius } => std::f64::consts::PI * radius * radius,
    Shape::Rectangle { width, height } => width * height,
    // si agregas un variante al enum, esto NO compila sin cubrirlo
}
```

---

## 20. Ejemplo SysAdmin: Health checker con type switch y switch clasico

```go
package main

import (
    "context"
    "encoding/json"
    "errors"
    "fmt"
    "io"
    "log"
    "net"
    "net/http"
    "os"
    "os/exec"
    "strconv"
    "strings"
    "sync"
    "time"
)

// ── Tipos de health check ──────────────────────────────────────

type HealthResult struct {
    Name     string        `json:"name"`
    Status   string        `json:"status"`
    Message  string        `json:"message,omitempty"`
    Duration time.Duration `json:"duration_ms"`
}

type HealthChecker interface {
    Name() string
    Check(ctx context.Context) error
}

// ── TCP check ──────────────────────────────────────────────────

type TCPCheck struct {
    host    string
    port    int
    timeout time.Duration
}

func (c TCPCheck) Name() string {
    return fmt.Sprintf("tcp:%s:%d", c.host, c.port)
}

func (c TCPCheck) Check(ctx context.Context) error {
    addr := fmt.Sprintf("%s:%d", c.host, c.port)
    dialer := net.Dialer{Timeout: c.timeout}
    conn, err := dialer.DialContext(ctx, "tcp", addr)
    if err != nil {
        return err
    }
    return conn.Close()
}

// ── HTTP check ─────────────────────────────────────────────────

type HTTPCheck struct {
    url            string
    expectedStatus int
    timeout        time.Duration
}

func (c HTTPCheck) Name() string {
    return fmt.Sprintf("http:%s", c.url)
}

func (c HTTPCheck) Check(ctx context.Context) error {
    client := &http.Client{Timeout: c.timeout}
    req, err := http.NewRequestWithContext(ctx, "GET", c.url, nil)
    if err != nil {
        return err
    }

    resp, err := client.Do(req)
    if err != nil {
        return err
    }
    defer resp.Body.Close()
    io.Copy(io.Discard, resp.Body)

    if resp.StatusCode != c.expectedStatus {
        return fmt.Errorf("expected status %d, got %d", c.expectedStatus, resp.StatusCode)
    }
    return nil
}

// ── Command check ──────────────────────────────────────────────

type CommandCheck struct {
    name    string
    command string
    args    []string
}

func (c CommandCheck) Name() string {
    return fmt.Sprintf("cmd:%s", c.name)
}

func (c CommandCheck) Check(ctx context.Context) error {
    cmd := exec.CommandContext(ctx, c.command, c.args...)
    output, err := cmd.CombinedOutput()
    if err != nil {
        return fmt.Errorf("%v: %s", err, strings.TrimSpace(string(output)))
    }
    return nil
}

// ── Disk check ─────────────────────────────────────────────────

type DiskCheck struct {
    path         string
    minFreeBytes int64
}

func (c DiskCheck) Name() string {
    return fmt.Sprintf("disk:%s", c.path)
}

func (c DiskCheck) Check(ctx context.Context) error {
    // Leer output de df
    cmd := exec.CommandContext(ctx, "df", "-B1", c.path)
    output, err := cmd.Output()
    if err != nil {
        return fmt.Errorf("df failed: %w", err)
    }
    lines := strings.Split(strings.TrimSpace(string(output)), "\n")
    if len(lines) < 2 {
        return errors.New("unexpected df output")
    }
    fields := strings.Fields(lines[1])
    if len(fields) < 4 {
        return errors.New("unexpected df format")
    }
    available, err := strconv.ParseInt(fields[3], 10, 64)
    if err != nil {
        return fmt.Errorf("parse available space: %w", err)
    }
    if available < c.minFreeBytes {
        return fmt.Errorf("only %d MB free (need %d MB)",
            available/1024/1024, c.minFreeBytes/1024/1024)
    }
    return nil
}

// ── Health check runner ────────────────────────────────────────

func runHealthChecks(ctx context.Context, checks []HealthChecker) []HealthResult {
    results := make([]HealthResult, len(checks))
    var wg sync.WaitGroup

    for i, check := range checks {
        wg.Add(1)
        go func(idx int, hc HealthChecker) {
            defer wg.Done()

            start := time.Now()
            err := hc.Check(ctx)
            duration := time.Since(start)

            result := HealthResult{
                Name:     hc.Name(),
                Duration: duration,
            }

            if err == nil {
                result.Status = "healthy"
            } else {
                result.Status = "unhealthy"
                result.Message = err.Error()
            }

            results[idx] = result
        }(i, check)
    }

    wg.Wait()
    return results
}

// ── Diagnostico con type switch ────────────────────────────────

func diagnose(check HealthChecker, err error) string {
    // Primero: type switch sobre el check para contexto
    var context string
    switch c := check.(type) {
    case TCPCheck:
        context = fmt.Sprintf("TCP connection to %s:%d", c.host, c.port)
    case HTTPCheck:
        context = fmt.Sprintf("HTTP request to %s", c.url)
    case CommandCheck:
        context = fmt.Sprintf("Command '%s %s'", c.command, strings.Join(c.args, " "))
    case DiskCheck:
        context = fmt.Sprintf("Disk space on %s", c.path)
    default:
        context = fmt.Sprintf("Check %T", check)
    }

    // Segundo: type switch sobre el error para diagnostico
    var diagnostic string
    switch e := err.(type) {
    case *net.OpError:
        switch {
        case e.Timeout():
            diagnostic = "connection timed out — check firewall rules and service availability"
        case strings.Contains(e.Error(), "connection refused"):
            diagnostic = "connection refused — service is likely not running"
        case strings.Contains(e.Error(), "no route to host"):
            diagnostic = "no route to host — check network configuration and routing"
        default:
            diagnostic = fmt.Sprintf("network error: %v", e)
        }

    case *net.DNSError:
        switch {
        case e.IsNotFound:
            diagnostic = fmt.Sprintf("DNS lookup failed: %s not found — check /etc/hosts or DNS configuration", e.Name)
        case e.IsTimeout:
            diagnostic = "DNS lookup timed out — check /etc/resolv.conf and DNS server availability"
        default:
            diagnostic = fmt.Sprintf("DNS error: %v", e)
        }

    case *os.PathError:
        switch {
        case errors.Is(e, os.ErrNotExist):
            diagnostic = fmt.Sprintf("path %s does not exist", e.Path)
        case errors.Is(e, os.ErrPermission):
            diagnostic = fmt.Sprintf("permission denied for %s — check file ownership and permissions", e.Path)
        default:
            diagnostic = fmt.Sprintf("filesystem error: %v", e)
        }

    case *exec.ExitError:
        diagnostic = fmt.Sprintf("command exited with code %d", e.ExitCode())

    default:
        // switch sin expresion para clasificar por mensaje
        msg := err.Error()
        switch {
        case strings.Contains(msg, "timeout"):
            diagnostic = "operation timed out"
        case strings.Contains(msg, "refused"):
            diagnostic = "connection refused"
        case strings.Contains(msg, "no space"):
            diagnostic = "no disk space available"
        default:
            diagnostic = msg
        }
    }

    return fmt.Sprintf("[%s] %s", context, diagnostic)
}

// ── Config loading con switch ──────────────────────────────────

func loadChecksFromConfig(configs []map[string]interface{}) []HealthChecker {
    var checks []HealthChecker

    for _, cfg := range configs {
        checkType, ok := cfg["type"].(string)
        if !ok {
            log.Printf("warning: check missing 'type' field, skipping")
            continue
        }

        switch checkType {
        case "tcp":
            host, _ := cfg["host"].(string)
            port, _ := cfg["port"].(float64) // JSON numbers are float64
            timeout := 5 * time.Second
            if t, ok := cfg["timeout"].(string); ok {
                timeout, _ = time.ParseDuration(t)
            }
            checks = append(checks, TCPCheck{
                host:    host,
                port:    int(port),
                timeout: timeout,
            })

        case "http":
            url, _ := cfg["url"].(string)
            expectedStatus := 200
            if s, ok := cfg["expected_status"].(float64); ok {
                expectedStatus = int(s)
            }
            timeout := 10 * time.Second
            if t, ok := cfg["timeout"].(string); ok {
                timeout, _ = time.ParseDuration(t)
            }
            checks = append(checks, HTTPCheck{
                url:            url,
                expectedStatus: expectedStatus,
                timeout:        timeout,
            })

        case "command":
            name, _ := cfg["name"].(string)
            command, _ := cfg["command"].(string)
            var args []string
            if a, ok := cfg["args"].([]interface{}); ok {
                for _, arg := range a {
                    if s, ok := arg.(string); ok {
                        args = append(args, s)
                    }
                }
            }
            checks = append(checks, CommandCheck{
                name:    name,
                command: command,
                args:    args,
            })

        case "disk":
            path, _ := cfg["path"].(string)
            minFreeGB := 1.0
            if m, ok := cfg["min_free_gb"].(float64); ok {
                minFreeGB = m
            }
            checks = append(checks, DiskCheck{
                path:         path,
                minFreeBytes: int64(minFreeGB * 1024 * 1024 * 1024),
            })

        default:
            log.Printf("warning: unknown check type %q, skipping", checkType)
        }
    }

    return checks
}

// ── Main ───────────────────────────────────────────────────────

func main() {
    // Cargar configuracion
    configFile := "healthchecks.json"
    if len(os.Args) > 1 {
        configFile = os.Args[1]
    }

    data, err := os.ReadFile(configFile)
    if err != nil {
        // Usar checks por defecto si no hay config
        log.Printf("No config file (%v) — using defaults", err)
        data = []byte(`[
            {"type": "tcp", "host": "localhost", "port": 22},
            {"type": "tcp", "host": "localhost", "port": 80},
            {"type": "tcp", "host": "localhost", "port": 443},
            {"type": "tcp", "host": "localhost", "port": 5432},
            {"type": "disk", "path": "/", "min_free_gb": 5},
            {"type": "disk", "path": "/var", "min_free_gb": 2},
            {"type": "command", "name": "systemd", "command": "systemctl", "args": ["is-system-running"]}
        ]`)
    }

    var configs []map[string]interface{}
    if err := json.Unmarshal(data, &configs); err != nil {
        log.Fatalf("Invalid config: %v", err)
    }

    checks := loadChecksFromConfig(configs)
    if len(checks) == 0 {
        log.Fatal("No health checks configured")
    }

    // Ejecutar
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()

    fmt.Println("Running health checks...")
    fmt.Println(strings.Repeat("─", 70))

    results := runHealthChecks(ctx, checks)

    // Reportar resultados
    var unhealthy int
    for _, r := range results {
        var icon string
        switch r.Status {
        case "healthy":
            icon = "\033[32m✓\033[0m"
        case "unhealthy":
            icon = "\033[31m✗\033[0m"
            unhealthy++
        }

        fmt.Printf("  %s %-45s %6dms", icon, r.Name, r.Duration.Milliseconds())
        if r.Message != "" {
            fmt.Printf("  %s", r.Message)
        }
        fmt.Println()

        // Diagnostico para checks fallidos
        if r.Status == "unhealthy" {
            // Encontrar el check correspondiente y diagnosticar
            for _, check := range checks {
                if check.Name() == r.Name {
                    checkCtx, checkCancel := context.WithTimeout(context.Background(), 5*time.Second)
                    if checkErr := check.Check(checkCtx); checkErr != nil {
                        diag := diagnose(check, checkErr)
                        fmt.Printf("    └─ %s\n", diag)
                    }
                    checkCancel()
                    break
                }
            }
        }
    }

    fmt.Println(strings.Repeat("─", 70))

    // Resumen con switch
    switch {
    case unhealthy == 0:
        fmt.Printf("\033[32mAll %d checks passed\033[0m\n", len(results))
    case unhealthy == len(results):
        fmt.Printf("\033[31mAll %d checks FAILED\033[0m\n", len(results))
        os.Exit(2)
    default:
        fmt.Printf("\033[33m%d/%d checks failed\033[0m\n", unhealthy, len(results))
        os.Exit(1)
    }
}
```

### Archivo de configuracion de ejemplo: healthchecks.json

```json
[
    {"type": "tcp", "host": "localhost", "port": 22, "timeout": "3s"},
    {"type": "tcp", "host": "localhost", "port": 80},
    {"type": "tcp", "host": "localhost", "port": 443},
    {"type": "tcp", "host": "localhost", "port": 5432, "timeout": "2s"},
    {"type": "tcp", "host": "localhost", "port": 6379},
    {"type": "http", "url": "http://localhost:8080/health", "expected_status": 200, "timeout": "5s"},
    {"type": "http", "url": "http://localhost:9090/-/healthy", "expected_status": 200},
    {"type": "disk", "path": "/", "min_free_gb": 5},
    {"type": "disk", "path": "/var/log", "min_free_gb": 1},
    {"type": "command", "name": "systemd-health", "command": "systemctl", "args": ["is-system-running"]},
    {"type": "command", "name": "docker", "command": "docker", "args": ["info", "--format", "{{.ServerVersion}}"]}
]
```

---

## 21. Errores comunes

| # | Error | Codigo | Solucion |
|---|---|---|---|
| 1 | Poner `break` por costumbre de C | `case 1: ... break` | Innecesario — eliminar el `break` |
| 2 | Asumir fall-through | Esperar que case 1 caiga a case 2 | Usar `fallthrough` explicitamente |
| 3 | `fallthrough` condicional | `if cond { fallthrough }` | ✗ `fallthrough` debe ser ultimo statement |
| 4 | `break` para salir de `for` | `break` dentro de switch en for | Usar `break label` con label en el for |
| 5 | Duplicar cases | `case 1: ... case 1: ...` | Error de compilacion: duplicate case |
| 6 | Type switch sin interface | `switch x.(type)` con `x int` | Solo funciona con tipos interface |
| 7 | Type switch con `fallthrough` | Mezclar ambos | ✗ Prohibido por el compilador |
| 8 | Multiples tipos = interface | `case int, string:` → v es interface | Separar en cases individuales si necesitas el tipo |
| 9 | Olvidar `default` con input externo | Switch sobre user input sin default | Siempre poner default para input no controlado |
| 10 | Switch vacio | `switch { }` | Compila pero no hace nada — eliminar |
| 11 | Comparar interfaces con `==` en case | `case err == ErrNotFound:` en switch expr | Usar `switch { case errors.Is(...): }` |
| 12 | iota sin linter exhaustive | Olvidar cubrir un valor | Habilitar `exhaustive` en golangci-lint |

### Error 3 en detalle — fallthrough condicional

```go
// ✗ ESTO NO COMPILA
switch x {
case 1:
    fmt.Println("one")
    if someCondition {
        fallthrough  // ✗ ERROR: fallthrough statement out of place
    }
case 2:
    fmt.Println("two")
}

// ✓ Alternativa: extraer la logica compartida
switch x {
case 1:
    fmt.Println("one")
    if someCondition {
        handleTwo()  // llamar la funcion directamente
    }
case 2:
    handleTwo()
}
```

### Error 7 en detalle — type switch + fallthrough

```go
// ✗ PROHIBIDO
switch v := x.(type) {
case int:
    fmt.Println("int")
    fallthrough  // ✗ ERROR: cannot fallthrough in type switch
case string:
    fmt.Println("string")
}

// ¿Por que? Porque v cambia de tipo en cada case.
// fallthrough al siguiente case haria que v tenga un tipo indefinido.
```

---

## 22. Ejercicios

### Ejercicio 1: Prediccion de switch basico
```go
package main

import "fmt"

func main() {
    x := 3

    switch x {
    case 1, 2:
        fmt.Println("small")
    case 3:
        fmt.Println("three")
        // nota: sin fallthrough
    case 4:
        fmt.Println("four")
    default:
        fmt.Println("other")
    }

    switch {
    case x > 5:
        fmt.Println("big")
    case x > 2:
        fmt.Println("medium")
    case x > 0:
        fmt.Println("small")
    }
}
```
**Prediccion**: ¿Que imprime cada switch? ¿Se ejecuta "medium" Y "small" en el segundo switch?

### Ejercicio 2: Prediccion de fallthrough
```go
package main

import "fmt"

func main() {
    x := 2

    switch x {
    case 1:
        fmt.Println("one")
        fallthrough
    case 2:
        fmt.Println("two")
        fallthrough
    case 3:
        fmt.Println("three")
    case 4:
        fmt.Println("four")
    }
}
```
**Prediccion**: ¿Que lineas se imprimen? ¿Se imprime "four"?

### Ejercicio 3: Prediccion de type switch
```go
package main

import "fmt"

func check(val interface{}) {
    switch v := val.(type) {
    case int:
        fmt.Printf("int: %d\n", v*2)
    case string:
        fmt.Printf("string: %q (len=%d)\n", v, len(v))
    case []int:
        fmt.Printf("[]int: len=%d cap=%d\n", len(v), cap(v))
    case nil:
        fmt.Println("nil!")
    default:
        fmt.Printf("unknown: %T\n", v)
    }
}

func main() {
    check(42)
    check("hello")
    check([]int{1, 2, 3})
    check(nil)
    check(3.14)
    check(true)
}
```
**Prediccion**: ¿Que imprime para cada llamada? ¿Que tipo tiene `v` en el case `default`?

### Ejercicio 4: Prediccion de break en switch + for
```go
package main

import "fmt"

func main() {
    items := []string{"start", "process", "shutdown", "cleanup"}

    for _, item := range items {
        switch item {
        case "shutdown":
            fmt.Println("shutting down")
            break
        case "process":
            fmt.Println("processing")
        default:
            fmt.Printf("handling: %s\n", item)
        }
    }
    fmt.Println("loop finished")
}
```
**Prediccion**: ¿Se ejecuta "cleanup"? ¿El `break` detiene el `for` o solo el `switch`?

### Ejercicio 5: Prediccion de switch con init
```go
package main

import (
    "fmt"
    "os"
)

func main() {
    switch hostname, err := os.Hostname(); {
    case err != nil:
        fmt.Println("error:", err)
    case len(hostname) > 10:
        fmt.Println("long hostname:", hostname)
    default:
        fmt.Println("hostname:", hostname)
    }

    // fmt.Println(hostname)  // ← ¿Compilaria?
}
```
**Prediccion**: ¿Que case se ejecuta? ¿Que pasa si descomentas la ultima linea?

### Ejercicio 6: Clasificador de servicios
Implementa una funcion que reciba una lista de puertos abiertos y retorne un reporte clasificado por categoria:
1. Use switch para clasificar cada puerto (SSH, HTTP, database, monitoring, etc.)
2. Agrupa los resultados por categoria
3. Imprime un reporte con el conteo por categoria
4. Marca como "ALERT" cualquier puerto privilegiado (<1024) no reconocido

### Ejercicio 7: Parser de /etc/passwd
Implementa un parser que:
1. Lea `/etc/passwd` linea por linea
2. Use switch para clasificar usuarios por shell (`/bin/bash`, `/sbin/nologin`, `/usr/bin/zsh`, etc.)
3. Use switch para clasificar por rango de UID (0=root, 1-999=system, 1000+=regular)
4. Imprima un resumen con conteos por shell y por tipo de usuario

### Ejercicio 8: Mini state machine
Implementa un procesador de texto simple que:
1. Lea un archivo de configuracion tipo INI
2. Use una state machine con switch para alternar entre estados: `OutsideSection`, `InsideSection`, `InComment`
3. Parsee secciones `[section]`, claves `key=value`, y comentarios `; ...`
4. Retorne un `map[string]map[string]string` (seccion → key → value)

### Ejercicio 9: Health checker con type switch
Implementa una version simplificada del health checker:
1. Define 3 tipos de checks: `PingCheck` (ICMP), `PortCheck` (TCP), `FileCheck` (existe el archivo)
2. Todos satisfacen la interface `Checker` con metodo `Check() error`
3. Usa type switch en una funcion `suggest(check Checker, err error) string` que retorne una sugerencia de fix diferente segun el tipo de check

### Ejercicio 10: Log level router
Implementa un sistema que:
1. Parsee lineas de log con formato `LEVEL: message`
2. Use switch para despachar a diferentes handlers segun el nivel
3. Use switch sin expresion para detectar patterns en el mensaje (OOM, disk, network, auth)
4. Acumule estadisticas y las imprima como resumen final

---

## 23. Resumen

```
┌──────────────────────────────────────────────────────────────┐
│              switch EN GO — RESUMEN                           │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FORMAS                                                      │
│  ├─ switch expr { case v1: ... }         → clasico           │
│  ├─ switch { case cond: ... }            → if-else chain     │
│  ├─ switch init; expr { ... }            → con init          │
│  └─ switch v := x.(type) { case T: ... } → type switch       │
│                                                              │
│  FALL-THROUGH                                                │
│  ├─ NO por defecto (cada case es aislado)                    │
│  ├─ fallthrough fuerza caida al SIGUIENTE case               │
│  ├─ fallthrough NO evalua la condicion del siguiente case    │
│  ├─ Debe ser la ULTIMA sentencia del case                    │
│  ├─ No puede usarse en type switch                           │
│  └─ Casi nunca se necesita — usar case val1, val2 en su     │
│     lugar                                                    │
│                                                              │
│  BREAK                                                       │
│  ├─ break sale del switch, NO del for que lo rodea           │
│  ├─ Usa break label para salir del for                       │
│  └─ Es implicito — rara vez necesitas escribirlo             │
│                                                              │
│  TYPE SWITCH                                                 │
│  ├─ switch v := x.(type) — x debe ser interface              │
│  ├─ v toma el tipo concreto en cada case                     │
│  ├─ case int, string: → v es interface{} (multiples tipos)   │
│  ├─ Puede matchear interfaces, no solo tipos concretos       │
│  └─ No admite fallthrough                                    │
│                                                              │
│  SYSADMIN                                                    │
│  ├─ Clasificar puertos, signals, HTTP status, log levels     │
│  ├─ State machines para deploy pipelines                     │
│  ├─ Parsear /proc, /etc/fstab, systemctl output             │
│  ├─ Type switch para error handling (net, os, exec errors)   │
│  ├─ Dispatcher de comandos CLI                               │
│  └─ Config loading con type switch sobre JSON values         │
│                                                              │
│  EXHAUSTIVIDAD                                               │
│  ├─ Go NO verifica exhaustividad en compile time             │
│  ├─ Usar golangci-lint + exhaustive linter                   │
│  └─ Siempre poner default para input externo                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

---

> **Siguiente**: T04 - Labels y goto — break/continue con labels, goto (raro pero existe), cuando es legitimo
