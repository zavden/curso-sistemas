# El Patron if err != nil — Idiomatico, No Repetitivo, Named Returns para Cleanup

## 1. Introduccion

`if err != nil` es la linea de codigo mas escrita en Go. Aparece en cada funcion que llama a otra funcion que puede fallar — y en Go, casi todas pueden. Este patron es la **columna vertebral** del error handling en Go: recibir el error, verificarlo, y decidir que hacer con el (devolver, logear, reintentar, transformar, ignorar explicitamente).

La critica mas comun de Go — especialmente de programadores que vienen de Python, Java o Rust — es que `if err != nil` es **repetitivo y verboso**. El equipo de Go no solo reconoce esta critica sino que la ha abordado directamente: argumentan que la repeticion es **intencional** y que fuerza al programador a considerar cada punto de fallo explicitamente. La verbosidad no es un defecto del lenguaje — es una decision de diseno que prioriza la **legibilidad** y la **claridad del flujo de errores** sobre la brevedad.

Sin embargo, "idiomatico" no significa "escribir el mismo boilerplate 200 veces". Go ofrece multiples patrones para **reducir la repeticion** sin sacrificar la explicitud: named returns para cleanup, helper functions, patron errWriter (T01), closures de error handling, y la estructura condicional con inicializacion (`if err := f(); err != nil`). La clave es saber cuando cada patron aplica.

Para SysAdmin/DevOps, el patron `if err != nil` en cada paso de un workflow (conectar → autenticar → consultar → procesar → responder) crea un **audit trail natural** de donde fallo una operacion. Cada `if err != nil` es una oportunidad para agregar contexto: "en que servidor", "que operacion", "con que parametros". Este contexto es invaluable cuando estas debuggeando un incidente a las 3am.

```
┌──────────────────────────────────────────────────────────────────────────┐
│         EL PATRON if err != nil — Anatomia                               │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  func doWork() error {                                                   │
│      ┌──────────────────────────────────────┐                            │
│      │ result, err := step1()               │ ← llamar operacion        │
│      │ if err != nil {                      │ ← verificar error         │
│      │     return fmt.Errorf("step1: %w",   │ ← agregar contexto        │
│      │                        err)          │ ← devolver al caller       │
│      │ }                                    │                            │
│      ├──────────────────────────────────────┤                            │
│      │ data, err := step2(result)           │ ← siguiente operacion     │
│      │ if err != nil {                      │                            │
│      │     return fmt.Errorf("step2: %w",   │                            │
│      │                        err)          │                            │
│      │ }                                    │                            │
│      ├──────────────────────────────────────┤                            │
│      │ return step3(data)                   │ ← ultimo paso —            │
│      │ // si step3 falla, su error sube     │   devolver directo         │
│      └──────────────────────────────────────┘                            │
│  }                                                                       │
│                                                                          │
│  Cada bloque: LLAMAR → VERIFICAR → CONTEXTUALIZAR → DEVOLVER            │
│  El flujo feliz es la columna izquierda (sin indentacion extra)          │
│  Los errores salen por la derecha (return temprano)                      │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. El Patron Basico y sus Variantes

### 2.1 Forma canonica

```go
// La forma mas comun y directa
result, err := someFunction()
if err != nil {
    return fmt.Errorf("context: %w", err)
}
// usar result...
```

### 2.2 Con inicializacion inline (short statement)

```go
// Go permite declarar variables en el if — el scope queda limitado al bloque
if err := os.Remove(path); err != nil {
    return fmt.Errorf("remove %s: %w", path, err)
}
// err no existe aqui — scope limitado al if

// Util cuando no necesitas el resultado de la funcion:
if err := os.MkdirAll(dir, 0755); err != nil {
    return fmt.Errorf("mkdir %s: %w", dir, err)
}

// O cuando el resultado solo se usa en el if:
if n, err := fmt.Fprintf(w, "hello"); err != nil {
    return fmt.Errorf("write failed after %d bytes: %w", n, err)
}
```

### 2.3 Verificar y asignar separadamente

```go
// Cuando necesitas reusar la variable err (comun en funciones largas)
var err error

data, err = readFile(path)
if err != nil {
    return err
}

processed, err = transform(data)
if err != nil {
    return err
}

err = save(processed)
if err != nil {
    return err
}
```

### 2.4 El ultimo return directo

```go
// El ultimo paso de una funcion no necesita if err != nil
// — simplemente devuelve lo que devuelve

func process(data []byte) error {
    validated, err := validate(data)
    if err != nil {
        return fmt.Errorf("validate: %w", err)
    }

    transformed, err := transform(validated)
    if err != nil {
        return fmt.Errorf("transform: %w", err)
    }

    // Ultimo paso — devolver directamente
    return save(transformed) // si save falla, su error sube; si no, devuelve nil
}
```

### 2.5 Comparacion visual de las variantes

```
┌─────────────────────────────────────────────────────────────────────┐
│  VARIANTES del if err != nil                                        │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  ① Canonica (resultado + error):                                    │
│     result, err := f()                                              │
│     if err != nil { return err }                                    │
│     // usar result                                                  │
│                                                                     │
│  ② Inline (solo error):                                             │
│     if err := f(); err != nil { return err }                        │
│     // err no existe aqui                                           │
│                                                                     │
│  ③ Reutilizando err:                                                │
│     var err error                                                   │
│     x, err = f1() ; if err != nil { return err }                   │
│     y, err = f2() ; if err != nil { return err }                   │
│                                                                     │
│  ④ Return directo (ultimo paso):                                    │
│     return f()  // sin if err != nil                                │
│                                                                     │
│  ⑤ Con contexto:                                                    │
│     if err := f(); err != nil {                                     │
│         return fmt.Errorf("doing X: %w", err)                      │
│     }                                                               │
└─────────────────────────────────────────────────────────────────────┘
```

---

## 3. Happy Path a la Izquierda

Un principio fundamental del estilo Go es: **el happy path debe ser el codigo sin indentar**. Los errores causan returns tempranos, y el flujo normal continua en la columna principal.

### Antipatron: happy path indentado

```go
// ✗ MALO: el happy path esta anidado dentro de multiples ifs
func processRequest(r *http.Request) (*Response, error) {
    body, err := io.ReadAll(r.Body)
    if err == nil {
        var req Request
        err = json.Unmarshal(body, &req)
        if err == nil {
            result, err := execute(req)
            if err == nil {
                return &Response{Data: result}, nil
            } else {
                return nil, fmt.Errorf("execute: %w", err)
            }
        } else {
            return nil, fmt.Errorf("unmarshal: %w", err)
        }
    } else {
        return nil, fmt.Errorf("read body: %w", err)
    }
}
```

### Patron correcto: early return

```go
// ✓ BUENO: errores causan return temprano, happy path a la izquierda
func processRequest(r *http.Request) (*Response, error) {
    body, err := io.ReadAll(r.Body)
    if err != nil {
        return nil, fmt.Errorf("read body: %w", err)
    }

    var req Request
    if err := json.Unmarshal(body, &req); err != nil {
        return nil, fmt.Errorf("unmarshal: %w", err)
    }

    result, err := execute(req)
    if err != nil {
        return nil, fmt.Errorf("execute: %w", err)
    }

    return &Response{Data: result}, nil
}
```

### La regla visual

```
┌──────────────────────────────────────────┐
│  ✗ Happy path indentado:                 │
│                                          │
│  if err == nil {                         │
│      if err == nil {                     │
│          if err == nil {                 │
│              // happy path aqui          │  ← dificil de leer
│          }                               │
│      }                                   │
│  }                                       │
│                                          │
│  ✓ Happy path a la izquierda:            │
│                                          │
│  if err != nil { return err }            │
│  if err != nil { return err }            │
│  if err != nil { return err }            │
│  // happy path aqui                      │  ← facil de leer
│                                          │
│  Los ojos siguen el margen izquierdo.    │
│  Cada "if err" es un "guardia" que       │
│  filtra errores antes de continuar.      │
└──────────────────────────────────────────┘
```

Esto es consistente con la filosofia de Go de "line of sight" — puedes leer una funcion de arriba abajo por el margen izquierdo y entender el happy path sin saltar mentalmente entre niveles de indentacion.

---

## 4. Named Returns para Cleanup con defer

Named returns (retornos con nombre) permiten que `defer` acceda y modifique los valores de retorno de una funcion. Este patron es especialmente util para **cleanup** (cerrar archivos, liberar locks, finalizar transacciones) donde necesitas que el cleanup pueda influir en el error devuelto.

### 4.1 Named returns basico

```go
// Sin named returns — no puedes modificar el error en defer
func readFile(path string) ([]byte, error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer f.Close() // ← si Close() falla, ¿como reportarlo?
    // El error de Close() se pierde silenciosamente

    return io.ReadAll(f)
}

// Con named returns — defer puede modificar err
func readFile(path string) (data []byte, err error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, fmt.Errorf("open %s: %w", path, err)
    }

    defer func() {
        closeErr := f.Close()
        if err == nil {
            // Solo reportar error de Close si la operacion principal fue exitosa
            err = closeErr
        }
        // Si ya habia un error, Close error se ignora
        // (el error original es mas informativo)
    }()

    data, err = io.ReadAll(f)
    if err != nil {
        return nil, fmt.Errorf("read %s: %w", path, err)
    }

    return data, nil
}
```

### 4.2 Como funcionan los named returns

```go
func divide(a, b float64) (result float64, err error) {
    // "result" y "err" son variables locales inicializadas a zero value
    // Cualquier "return" devuelve sus valores ACTUALES

    if b == 0 {
        err = errors.New("division by zero") // asignar al named return
        return                                // "naked return" — devuelve result=0, err=<el error>
    }

    result = a / b // asignar al named return
    return          // naked return — devuelve result=<el resultado>, err=nil
}

// NOTA: naked returns (return sin valores) solo son aceptables en funciones cortas.
// En funciones largas, usar return explicito para claridad:
func divide(a, b float64) (result float64, err error) {
    if b == 0 {
        return 0, errors.New("division by zero") // explicito — mas claro
    }
    return a / b, nil
}
```

### 4.3 Patron de transaccion con named returns

```go
// Patron clasico: transaccion que se commitea o rollbackea
func transferFunds(db *sql.DB, from, to int, amount float64) (err error) {
    tx, err := db.Begin()
    if err != nil {
        return fmt.Errorf("begin tx: %w", err)
    }

    // defer decide commit o rollback basado en el valor final de err
    defer func() {
        if err != nil {
            // Hubo error — rollback
            if rbErr := tx.Rollback(); rbErr != nil {
                err = fmt.Errorf("rollback failed: %v (original: %w)", rbErr, err)
            }
        } else {
            // Todo bien — commit
            if cmErr := tx.Commit(); cmErr != nil {
                err = fmt.Errorf("commit: %w", cmErr) // defer MODIFICA err
            }
        }
    }()

    // Operaciones de la transaccion
    _, err = tx.Exec("UPDATE accounts SET balance = balance - $1 WHERE id = $2", amount, from)
    if err != nil {
        return fmt.Errorf("debit account %d: %w", from, err)
    }

    _, err = tx.Exec("UPDATE accounts SET balance = balance + $1 WHERE id = $2", amount, to)
    if err != nil {
        return fmt.Errorf("credit account %d: %w", to, err)
    }

    return nil // el defer hara commit
}
```

### 4.4 Patron de lock con named returns

```go
type SafeCache struct {
    mu    sync.RWMutex
    items map[string]string
}

func (c *SafeCache) GetOrCompute(key string, compute func() (string, error)) (value string, err error) {
    // Intentar leer con read lock primero
    c.mu.RLock()
    if v, ok := c.items[key]; ok {
        c.mu.RUnlock()
        return v, nil
    }
    c.mu.RUnlock()

    // No encontrado — obtener write lock y computar
    c.mu.Lock()
    defer c.mu.Unlock() // defer garantiza unlock incluso si compute() hace panic

    // Double-check despues de obtener write lock
    if v, ok := c.items[key]; ok {
        return v, nil
    }

    value, err = compute()
    if err != nil {
        return "", fmt.Errorf("compute %s: %w", key, err)
    }

    c.items[key] = value
    return value, nil // named returns hacen el return mas claro
}
```

### 4.5 Reglas para named returns

```
┌──────────────────────────────────────────────────────────────────────┐
│  NAMED RETURNS — Cuando usar y cuando no                             │
├──────────────────────────────────────────────────────────────────────┤
│                                                                      │
│  ✓ USAR cuando:                                                      │
│  ├─ defer necesita leer/modificar el error de retorno               │
│  ├─ Transacciones (commit/rollback basado en error)                 │
│  ├─ Recursos con Close() que puede fallar                           │
│  └─ La funcion es corta y los nombres agregan documentacion         │
│                                                                      │
│  ✗ NO USAR cuando:                                                   │
│  ├─ La funcion es larga (naked returns se vuelven confusos)         │
│  ├─ No necesitas defer que modifique retornos                       │
│  ├─ Los nombres no agregan claridad ("result", "err" genericos)     │
│  └─ Hay riesgo de shadowing (err := ... crea variable nueva)       │
│                                                                      │
│  CUIDADO con shadowing:                                              │
│  func f() (err error) {                                              │
│      if true {                                                       │
│          err := doSomething() // ← NUEVA variable, shadowing!       │
│          // esta err NO es la named return                           │
│      }                                                               │
│      return // devuelve la err original (nil), no la del bloque     │
│  }                                                                   │
└──────────────────────────────────────────────────────────────────────┘
```

---

## 5. Reducir la Repeticion — Patrones Idiomaticos

El argumento de "if err != nil es demasiado repetitivo" es valido en codigo ingenuo. Pero Go ofrece multiples patrones para reducir la repeticion:

### 5.1 Patron errWriter (recordatorio de T01)

```go
// Cuando escribes multiples veces al mismo writer:
type errWriter struct {
    w   io.Writer
    err error
}

func (ew *errWriter) write(p []byte) {
    if ew.err != nil {
        return
    }
    _, ew.err = ew.w.Write(p)
}

func (ew *errWriter) printf(format string, args ...interface{}) {
    if ew.err != nil {
        return
    }
    _, ew.err = fmt.Fprintf(ew.w, format, args...)
}

// Uso: 1 check en lugar de 10
func writeReport(w io.Writer, data *Report) error {
    ew := &errWriter{w: w}
    ew.printf("Title: %s\n", data.Title)
    ew.printf("Date: %s\n", data.Date)
    ew.printf("Services: %d\n", len(data.Services))
    for _, svc := range data.Services {
        ew.printf("  - %s: %s\n", svc.Name, svc.Status)
    }
    return ew.err // UN solo check
}
```

### 5.2 Patron tipo-funcion (funciones como helpers)

```go
// Funcion helper que encapsula el patron comun
func must[T any](val T, err error) T {
    if err != nil {
        panic(err) // solo para inicializacion — nunca en runtime
    }
    return val
}

// Uso en inicializacion (var globales, init(), tests):
var (
    config = must(loadConfig("config.yaml"))
    tmpl   = must(template.ParseFiles("templates/*.html"))
    re     = must(regexp.Compile(`^\d{3}\.\d{3}\.\d{3}\.\d{3}$`))
)

// NUNCA usar must() en codigo que maneja requests o datos de usuario
// Solo para valores que DEBEN existir — si fallan, el programa no puede funcionar
```

### 5.3 Patron de pipeline con funciones

```go
// Encadenar operaciones donde cada paso puede fallar
type Pipeline struct {
    err error
}

func (p *Pipeline) Then(fn func() error) *Pipeline {
    if p.err != nil {
        return p // ya hubo error — saltar
    }
    p.err = fn()
    return p
}

func (p *Pipeline) ThenWith(fn func() error, context string) *Pipeline {
    if p.err != nil {
        return p
    }
    if err := fn(); err != nil {
        p.err = fmt.Errorf("%s: %w", context, err)
    }
    return p
}

func (p *Pipeline) Error() error {
    return p.err
}

// Uso:
func deployService(name string) error {
    p := &Pipeline{}
    p.ThenWith(func() error { return pullImage(name) }, "pull").
        ThenWith(func() error { return stopContainer(name) }, "stop").
        ThenWith(func() error { return startContainer(name) }, "start").
        ThenWith(func() error { return healthCheck(name) }, "health")
    return p.Error()
}
```

### 5.4 Patron de tabla de operaciones

```go
// Cuando tienes N operaciones del mismo tipo
func setupInfrastructure() error {
    steps := []struct {
        name string
        fn   func() error
    }{
        {"create network", createNetwork},
        {"start database", startDatabase},
        {"run migrations", runMigrations},
        {"start cache", startCache},
        {"start API", startAPI},
        {"register DNS", registerDNS},
    }

    for _, step := range steps {
        if err := step.fn(); err != nil {
            return fmt.Errorf("%s: %w", step.name, err)
        }
        fmt.Printf("  ✓ %s\n", step.name)
    }
    return nil
}
```

### 5.5 Patron validate-then-execute

```go
// Separar validacion (multiples checks) de ejecucion
func deployService(cfg DeployConfig) error {
    // Fase 1: validar todo primero (acumular errores)
    if err := cfg.Validate(); err != nil {
        return fmt.Errorf("invalid config: %w", err)
    }

    // Fase 2: ejecutar (cada paso puede fallar independientemente)
    // Solo llegamos aqui si la config es valida
    return cfg.Execute()
}

func (cfg DeployConfig) Validate() error {
    var errs []error

    if cfg.Image == "" {
        errs = append(errs, errors.New("image required"))
    }
    if cfg.Replicas < 1 {
        errs = append(errs, fmt.Errorf("replicas must be >= 1, got %d", cfg.Replicas))
    }
    if cfg.Port < 1 || cfg.Port > 65535 {
        errs = append(errs, fmt.Errorf("invalid port: %d", cfg.Port))
    }

    return errors.Join(errs...) // nil si no hay errores
}
```

---

## 6. Reasignacion de err y Shadowing

Uno de los puntos mas sutiles de `if err != nil` es la interaccion entre `:=` (declaracion corta) y `=` (asignacion). Go tiene una regla especial: `:=` puede **reusar** una variable si al menos una variable del lado izquierdo es nueva.

### 6.1 Reasignacion con :=

```go
func process() error {
    // Primera declaracion: x y err son NUEVAS
    x, err := step1()
    if err != nil {
        return err
    }

    // Segunda declaracion: y es NUEVA, err es REASIGNADA (no nueva)
    y, err := step2(x) // := reutiliza err porque y es nueva
    if err != nil {
        return err
    }

    // Tercera: z es NUEVA, err es REASIGNADA
    z, err := step3(y) // := reutiliza err porque z es nueva
    if err != nil {
        return err
    }

    _ = z
    return nil
}
```

### 6.2 El peligro del shadowing

```go
func dangerous() (err error) { // named return
    // Bloque interno — CUIDADO
    if condition {
        // := dentro de un bloque CREA una nueva variable err
        // que SOMBREA la named return
        result, err := riskyOp() // ← ESTA err es LOCAL al if block
        if err != nil {
            return err // devuelve la err local — OK en este caso
        }
        _ = result
    }

    // Pero si NO hay return dentro del if:
    if condition {
        _, err := riskyOp() // ← nueva err LOCAL — sombrea la named return
        // Si riskyOp() falla, la err del named return sigue siendo nil
        // porque esta err es una variable DIFERENTE
        _ = err // este warning de go vet te avisaria
    }

    return // devuelve err original (nil) — ¡el error de riskyOp se perdio!
}
```

### 6.3 La solucion al shadowing

```go
func safe() (err error) {
    if condition {
        // Usar = en lugar de := cuando quieres asignar a la variable existente
        var result Result
        result, err = riskyOp() // = asigna a la err del named return
        if err != nil {
            return
        }
        _ = result
    }

    // O declarar la variable auxiliar separadamente:
    if condition {
        result, localErr := riskyOp() // nombre diferente — sin ambiguedad
        if localErr != nil {
            err = localErr // asignar explicitamente a la named return
            return
        }
        _ = result
    }

    return
}
```

### 6.4 Herramientas que detectan shadowing

```bash
# go vet detecta algunos casos de shadowing
go vet ./...

# shadow analyzer (mas exhaustivo)
go install golang.org/x/tools/go/analysis/passes/shadow/cmd/shadow@latest
shadow ./...

# golangci-lint con govet shadow habilitado
golangci-lint run --enable govet
# govet tiene el check "shadow" que detecta variables sombreadas
```

---

## 7. El Debate: ¿if err != nil es Demasiado Verboso?

### 7.1 La critica

```go
// Un ejemplo tipico que los criticos senalan:
func processOrder(orderID string) (*Order, error) {
    user, err := getUser(orderID)
    if err != nil {
        return nil, fmt.Errorf("get user: %w", err)
    }

    inventory, err := checkInventory(user.CartItems)
    if err != nil {
        return nil, fmt.Errorf("check inventory: %w", err)
    }

    payment, err := processPayment(user, inventory.Total)
    if err != nil {
        return nil, fmt.Errorf("process payment: %w", err)
    }

    shipment, err := createShipment(user, inventory)
    if err != nil {
        return nil, fmt.Errorf("create shipment: %w", err)
    }

    order, err := finalizeOrder(user, payment, shipment)
    if err != nil {
        return nil, fmt.Errorf("finalize order: %w", err)
    }

    return order, nil
}

// 5 operaciones, 15 lineas de error handling, 5 lineas de logica
// Relacion: 3:1 error:logica — la critica es comprensible
```

### 7.2 La defensa del equipo de Go

El equipo de Go ha respondido a esta critica repetidamente:

**Rob Pike (2015, "Errors are values"):**
> "When I see Go programs that have `if err != nil { return err }` on every line, I think of it as a sign that the programmer hasn't thought about errors enough."

El punto: cada `if err != nil` es una **oportunidad para agregar contexto o tomar una decision**. Si todos tus bloques son `return err` sin contexto, estas desperdiciando la oportunidad.

**Russ Cox (2019, propuesta de `try`):**
Se propuso un builtin `try()` para Go 2 que eliminaria el boilerplate:
```go
// Propuesta (RECHAZADA):
// order := try(finalizeOrder(user, payment, shipment))
// Equivalente a:
// order, err := finalizeOrder(...)
// if err != nil { return ..., err }
```
La propuesta fue rechazada por la comunidad porque:
1. Ocultaba el manejo de errores (lo opuesto a la filosofia de Go)
2. Dificultaba agregar contexto
3. Incentivaba no pensar en errores

### 7.3 Comparacion de verbosidad real

```go
// Go: 20 lineas (5 operaciones + error handling)
func process() (*Result, error) {
    a, err := step1()
    if err != nil { return nil, fmt.Errorf("step1: %w", err) }
    b, err := step2(a)
    if err != nil { return nil, fmt.Errorf("step2: %w", err) }
    c, err := step3(b)
    if err != nil { return nil, fmt.Errorf("step3: %w", err) }
    d, err := step4(c)
    if err != nil { return nil, fmt.Errorf("step4: %w", err) }
    return step5(d)
}
```

```rust
// Rust: 7 lineas — pero MENOS contexto por defecto
fn process() -> Result<ResultType, Box<dyn Error>> {
    let a = step1()?;
    let b = step2(a)?;
    let c = step3(b)?;
    let d = step4(c)?;
    step5(d)
}

// Rust con contexto (anyhow crate): 12 lineas
fn process() -> anyhow::Result<ResultType> {
    let a = step1().context("step1")?;
    let b = step2(a).context("step2")?;
    let c = step3(b).context("step3")?;
    let d = step4(c).context("step4")?;
    step5(d).context("step5")
}
```

```python
# Python: 5 lineas — pero excepciones ocultas
def process():
    a = step1()      # puede lanzar excepcion — ¿cual?
    b = step2(a)     # puede lanzar excepcion — ¿cual?
    c = step3(b)     # puede lanzar excepcion — ¿cual?
    d = step4(c)     # puede lanzar excepcion — ¿cual?
    return step5(d)  # si algo falla, un try/catch lejano lo atrapa (o no)
```

```
┌──────────────────────────────────────────────────────────────────────┐
│  VERBOSIDAD vs CLARIDAD — El tradeoff                                │
├─────────────┬────────────┬────────────────┬──────────────────────────┤
│  Lenguaje   │  Lineas    │  Puntos fallo  │  Contexto por defecto    │
│             │            │  visibles      │                          │
├─────────────┼────────────┼────────────────┼──────────────────────────┤
│  Go         │  20        │  5/5 (100%)    │  Si (fmt.Errorf %w)      │
│  Rust (?)   │  7         │  5/5 (100%)    │  No (necesita .context)  │
│  Rust+ctx   │  12        │  5/5 (100%)    │  Si (anyhow .context)    │
│  Python     │  5         │  0/5 (0%)      │  No (stack trace remoto) │
│  Java       │  25+       │  variable      │  Si (catch blocks)       │
└─────────────┴────────────┴────────────────┴──────────────────────────┘
```

---

## 8. Decisiones en el if err != nil Block

El bloque `if err != nil` no siempre es `return err`. Dependiendo del contexto, puedes tomar diferentes decisiones:

### 8.1 Propagar con contexto (mas comun)

```go
if err != nil {
    return fmt.Errorf("operation X on %s: %w", target, err)
}
```

### 8.2 Log y continuar (errores no criticos)

```go
if err := sendMetrics(data); err != nil {
    // Metricas son best-effort — no detener el flujo principal
    log.Printf("warning: failed to send metrics: %v", err)
    // NO return — continuar
}
```

### 8.3 Reintentar

```go
var lastErr error
for attempt := 1; attempt <= maxRetries; attempt++ {
    lastErr = connectToService(host)
    if lastErr == nil {
        break // exito
    }
    log.Printf("attempt %d/%d failed: %v", attempt, maxRetries, lastErr)
    time.Sleep(time.Duration(attempt) * time.Second) // backoff lineal
}
if lastErr != nil {
    return fmt.Errorf("connect to %s after %d attempts: %w", host, maxRetries, lastErr)
}
```

### 8.4 Fallback a valor default

```go
config, err := loadConfig(path)
if err != nil {
    log.Printf("config not found, using defaults: %v", err)
    config = &Config{
        Port:    8080,
        Host:    "localhost",
        Timeout: 30 * time.Second,
    }
}
```

### 8.5 Transformar el error

```go
func handleAPIRequest(w http.ResponseWriter, r *http.Request) {
    user, err := getUser(r)
    if err != nil {
        // Transformar error interno a respuesta HTTP apropiada
        if errors.Is(err, ErrNotFound) {
            http.Error(w, "User not found", http.StatusNotFound)
        } else if errors.Is(err, ErrUnauthorized) {
            http.Error(w, "Unauthorized", http.StatusUnauthorized)
        } else {
            log.Printf("internal error: %v", err) // logear detalle
            http.Error(w, "Internal error", http.StatusInternalServerError) // no exponer
        }
        return
    }
    // ... usar user
}
```

### 8.6 Panic (solo para bugs)

```go
conn, err := db.Conn(ctx)
if err != nil {
    // La base de datos es REQUERIDA — si no funciona, el programa no puede operar
    // Pero esto es solo aceptable en init/startup, NO en handlers de request
    log.Fatalf("FATAL: cannot connect to database: %v", err)
    // log.Fatalf llama os.Exit(1) despues de logear
}
```

### Tabla de decisiones

```
┌──────────────────────────────────────────────────────────────────────────┐
│  ¿QUE HACER en if err != nil?                                            │
├─────────────────────┬────────────────────────────────────────────────────┤
│  Situacion          │  Accion                                            │
├─────────────────────┼────────────────────────────────────────────────────┤
│  Error esperado,    │  return fmt.Errorf("ctx: %w", err)                │
│  caller debe saber  │  — propagar con contexto                          │
├─────────────────────┼────────────────────────────────────────────────────┤
│  Error no critico,  │  log.Printf("warning: %v", err)                  │
│  operacion best-    │  — logear y continuar                             │
│  effort             │                                                    │
├─────────────────────┼────────────────────────────────────────────────────┤
│  Error transitorio  │  retry con backoff                                │
│  (red, timeout)     │  — reintentar N veces                             │
├─────────────────────┼────────────────────────────────────────────────────┤
│  Config ausente,    │  usar valor default                               │
│  defaults validos   │  — log.Printf + config = defaults                │
├─────────────────────┼────────────────────────────────────────────────────┤
│  Error interno →    │  mapear a HTTP status code                        │
│  respuesta externa  │  — transformar sin exponer detalles               │
├─────────────────────┼────────────────────────────────────────────────────┤
│  Recurso critico    │  log.Fatalf (solo en startup)                     │
│  en inicializacion  │  — el programa no puede funcionar sin esto        │
└─────────────────────┴────────────────────────────────────────────────────┘
```

---

## 9. defer, panic y recover — El Otro Lado del Error Handling

Go tiene un mecanismo separado para situaciones **irrecuperables**: `panic` y `recover`. Estos interactuan con `defer` y con el patron `if err != nil`.

### 9.1 defer — garantia de ejecucion

```go
func processFile(path string) error {
    f, err := os.Open(path)
    if err != nil {
        return err
    }
    defer f.Close() // SIEMPRE se ejecuta al salir de la funcion
    // incluso si hay return temprano, panic, o ejecucion normal

    // ... procesar f ...
    return nil
}

// Orden de ejecucion de multiples defers: LIFO (ultimo en entrar, primero en salir)
func multiDefer() {
    defer fmt.Println("1 — ultimo en ejecutar")
    defer fmt.Println("2")
    defer fmt.Println("3 — primero en ejecutar")
    fmt.Println("body")
}
// Output:
// body
// 3 — primero en ejecutar
// 2
// 1 — ultimo en ejecutar
```

### 9.2 defer con error handling — los 3 patrones

```go
// Patron A: defer simple (ignora error de Close)
func readSimple(path string) ([]byte, error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer f.Close() // si Close() falla, el error se pierde
    return io.ReadAll(f)
}

// Patron B: defer con named return (captura error de Close)
func readSafe(path string) (data []byte, err error) {
    f, err := os.Open(path)
    if err != nil {
        return nil, err
    }
    defer func() {
        if closeErr := f.Close(); closeErr != nil && err == nil {
            err = closeErr // solo si no habia error previo
        }
    }()
    return io.ReadAll(f)
}

// Patron C: defer con cleanup que necesita el error actual
func writeAtomic(path string, data []byte) (err error) {
    tmpPath := path + ".tmp"
    f, err := os.Create(tmpPath)
    if err != nil {
        return fmt.Errorf("create temp: %w", err)
    }

    defer func() {
        if err != nil {
            // Hubo error — limpiar archivo temporal
            f.Close()
            os.Remove(tmpPath)
        } else {
            // Exito — cerrar y renombrar atomicamente
            if closeErr := f.Close(); closeErr != nil {
                err = fmt.Errorf("close: %w", closeErr)
                os.Remove(tmpPath)
                return
            }
            if renameErr := os.Rename(tmpPath, path); renameErr != nil {
                err = fmt.Errorf("rename: %w", renameErr)
            }
        }
    }()

    _, err = f.Write(data)
    if err != nil {
        return fmt.Errorf("write: %w", err)
    }

    return nil // defer hace close + rename
}
```

### 9.3 panic y recover — cuando aplica

```go
// panic es para condiciones que NUNCA deberian ocurrir en codigo correcto
// NO es para errores operacionales

func getElement(slice []int, index int) int {
    // El runtime de Go hara panic automaticamente si index esta fuera de rango
    // Esto es un BUG — el caller paso un indice invalido
    return slice[index]
}

// recover captura un panic — util en servidores para no crashear
func safeHandler(handler http.HandlerFunc) http.HandlerFunc {
    return func(w http.ResponseWriter, r *http.Request) {
        defer func() {
            if r := recover(); r != nil {
                // Capturar el panic — no dejar que tumbe el servidor
                log.Printf("PANIC in handler: %v\n%s", r, debug.Stack())
                http.Error(w, "Internal Server Error", 500)
            }
        }()
        handler(w, r)
    }
}

// Patron: "must" functions usan panic en inicializacion
func mustConnect(dsn string) *sql.DB {
    db, err := sql.Open("postgres", dsn)
    if err != nil {
        panic(fmt.Sprintf("database connection failed: %v", err))
    }
    if err := db.Ping(); err != nil {
        panic(fmt.Sprintf("database ping failed: %v", err))
    }
    return db
}

// Solo usar must* en init() o main() — nunca en codigo que maneja requests
var db = mustConnect(os.Getenv("DATABASE_URL"))
```

---

## 10. Closures para Error Handling DRY

```go
// Cuando multiples operaciones tienen el mismo patron de error:

func setupServices(ctx context.Context) error {
    // Closure que encapsula el patron comun
    start := func(name string, fn func(context.Context) error) error {
        if err := fn(ctx); err != nil {
            return fmt.Errorf("start %s: %w", name, err)
        }
        fmt.Printf("  ✓ %s started\n", name)
        return nil
    }

    // Cada llamada es limpia — una linea
    if err := start("database", startDatabase); err != nil {
        return err
    }
    if err := start("cache", startCache); err != nil {
        return err
    }
    if err := start("queue", startQueue); err != nil {
        return err
    }
    if err := start("api", startAPI); err != nil {
        return err
    }

    return nil
}

// Variante: closure con retry integrado
func setupWithRetry(ctx context.Context) error {
    startWithRetry := func(name string, fn func(context.Context) error, retries int) error {
        var lastErr error
        for i := 0; i < retries; i++ {
            if err := fn(ctx); err != nil {
                lastErr = err
                log.Printf("  ⟳ %s attempt %d/%d failed: %v", name, i+1, retries, err)
                time.Sleep(time.Duration(i+1) * time.Second)
                continue
            }
            fmt.Printf("  ✓ %s started (attempt %d)\n", name, i+1)
            return nil
        }
        return fmt.Errorf("start %s: %w (after %d attempts)", name, lastErr, retries)
    }

    if err := startWithRetry("database", startDatabase, 5); err != nil {
        return err
    }
    if err := startWithRetry("cache", startCache, 3); err != nil {
        return err
    }

    return nil
}
```

---

## 11. Error Handling en Loops

Loops introducen la pregunta: ¿un error en un item debe abortar todo el loop, o solo saltar ese item?

### 11.1 Abortar en primer error

```go
func processAll(items []Item) error {
    for i, item := range items {
        if err := process(item); err != nil {
            return fmt.Errorf("item %d (%s): %w", i, item.Name, err)
        }
    }
    return nil
}
```

### 11.2 Acumular errores y continuar

```go
func processAll(items []Item) error {
    var errs []error
    for i, item := range items {
        if err := process(item); err != nil {
            errs = append(errs, fmt.Errorf("item %d (%s): %w", i, item.Name, err))
            continue // seguir con el siguiente item
        }
    }
    return errors.Join(errs...) // nil si no hubo errores
}
```

### 11.3 Limite de errores

```go
func processAll(items []Item, maxErrors int) error {
    var errs []error
    for i, item := range items {
        if err := process(item); err != nil {
            errs = append(errs, fmt.Errorf("item %d: %w", i, err))
            if len(errs) >= maxErrors {
                errs = append(errs, fmt.Errorf("too many errors (limit: %d), aborting", maxErrors))
                break
            }
            continue
        }
    }
    return errors.Join(errs...)
}
```

### 11.4 Partial success con reporte

```go
type BatchResult struct {
    Succeeded int
    Failed    int
    Errors    []error
}

func (br *BatchResult) Error() error {
    if br.Failed == 0 {
        return nil
    }
    return fmt.Errorf("%d/%d failed: %w",
        br.Failed, br.Succeeded+br.Failed, errors.Join(br.Errors...))
}

func processAllWithReport(items []Item) *BatchResult {
    result := &BatchResult{}
    for _, item := range items {
        if err := process(item); err != nil {
            result.Failed++
            result.Errors = append(result.Errors, err)
        } else {
            result.Succeeded++
        }
    }
    return result
}
```

---

## 12. Comparacion con C y Rust

### C — if de retorno manual

```c
// C: el patron es similar a Go pero sin tipo error y sin contexto automatico
int deploy_service(const char* name, const char* image) {
    int fd = open_config(name);
    if (fd < 0) {
        perror("open_config");  // imprime a stderr — no devuelve contexto
        return -1;
    }

    int ret = pull_image(image);
    if (ret != 0) {
        close(fd);  // cleanup manual — facil olvidarlo
        fprintf(stderr, "pull_image failed: %d\n", ret);
        return -1;
    }

    ret = start_container(name, image);
    if (ret != 0) {
        close(fd);  // hay que repetir el cleanup en cada error path
        fprintf(stderr, "start_container failed: %d\n", ret);
        return -1;
    }

    close(fd);
    return 0;
}

// Problemas de C:
// 1. Cleanup duplicado (close(fd) en cada error path) — Go tiene defer
// 2. No hay wrapping de errores — solo int return codes
// 3. errno global — race conditions en multithreading
// 4. goto cleanup es el "patron" comun en C:
int deploy_v2(const char* name) {
    int ret = -1;
    int fd = -1;
    char* buf = NULL;

    fd = open_config(name);
    if (fd < 0) goto cleanup;

    buf = malloc(4096);
    if (!buf) goto cleanup;

    if (read(fd, buf, 4096) < 0) goto cleanup;

    ret = 0;  // exito

cleanup:
    if (fd >= 0) close(fd);
    free(buf);   // free(NULL) es safe en C
    return ret;
}
// goto cleanup es el "defer" de C — feo pero funcional
```

### Rust — ? operator y Result chaining

```rust
// Rust: ? reemplaza el if err != nil completo
fn deploy_service(name: &str, image: &str) -> Result<(), DeployError> {
    let config = open_config(name)?;          // propaga si Err
    pull_image(image)?;                        // propaga si Err
    let container = start_container(name, image)?;  // propaga si Err
    health_check(&container)?;                 // propaga si Err
    Ok(())
}
// 4 operaciones, 4 lineas — ZERO boilerplate de error handling
// Pero: sin contexto (cual fallo?)

// Rust con contexto (anyhow crate):
use anyhow::Context;

fn deploy_service(name: &str, image: &str) -> anyhow::Result<()> {
    let config = open_config(name)
        .context("open config")?;              // contexto + propagacion
    pull_image(image)
        .with_context(|| format!("pull {}", image))?;
    let container = start_container(name, image)
        .context("start container")?;
    health_check(&container)
        .context("health check")?;
    Ok(())
}
// Mas contexto, todavia mas compacto que Go

// Rust: cleanup automatico via Drop (RAII)
// No hay defer — los destructores se llaman al salir del scope
fn process_file(path: &str) -> Result<Vec<u8>, io::Error> {
    let mut file = File::open(path)?;  // si Err, no hay file que cerrar
    let mut buf = Vec::new();
    file.read_to_end(&mut buf)?;       // si Err, file.drop() se llama auto
    Ok(buf)                             // file.drop() se llama al salir
    // NUNCA hay "olvidar cerrar" — el compilador lo garantiza
}
```

### Tabla comparativa del patron de error checking

```
┌──────────────────────────────────────────────────────────────────────────┐
│  if err != nil (Go) vs equivalentes                                      │
├──────────────┬──────────────────────┬────────────────────────────────────┤
│  Aspecto     │  Go                  │  Rust                              │
├──────────────┼──────────────────────┼────────────────────────────────────┤
│  Propagacion │  if err != nil {     │  ? (1 caracter)                    │
│              │    return err        │                                    │
│              │  }                   │                                    │
├──────────────┼──────────────────────┼────────────────────────────────────┤
│  Contexto    │  fmt.Errorf("%w")    │  .context() / .with_context()     │
│  al propagar │  (en el if block)    │  (method chaining)                 │
├──────────────┼──────────────────────┼────────────────────────────────────┤
│  Cleanup     │  defer f.Close()     │  Drop trait (automatico, RAII)     │
│              │  defer + named ret   │  No hay defer — innecesario        │
├──────────────┼──────────────────────┼────────────────────────────────────┤
│  Cleanup     │  defer func() {     │  N/A — Drop se llama siempre      │
│  con error   │    if err != nil     │  Drop no puede fallar (no         │
│              │  }()                 │  devuelve Result)                   │
├──────────────┼──────────────────────┼────────────────────────────────────┤
│  Happy path  │  Margen izquierdo    │  Margen izquierdo (? hace early   │
│              │  (early return)      │  return implicito)                  │
├──────────────┼──────────────────────┼────────────────────────────────────┤
│  Verbosidad  │  3 lineas por op     │  1 linea por op (con ?)           │
│  (por check) │                      │                                    │
├──────────────┼──────────────────────┼────────────────────────────────────┤
│              │  C                   │                                    │
├──────────────┼──────────────────────┤                                    │
│  Propagacion │  if (ret < 0)        │                                    │
│              │    return -1;        │                                    │
│  Cleanup     │  goto cleanup;       │                                    │
│  Contexto    │  perror() / manual   │                                    │
│  Verbosidad  │  2-4 lineas por op   │                                    │
│              │  + cleanup duplicado │                                    │
└──────────────┴──────────────────────┴────────────────────────────────────┘
```

---

## 13. Programa Completo: Service Deployment Pipeline

```go
// ============================================================================
// SERVICE DEPLOYMENT PIPELINE — if err != nil patterns
//
// Demuestra:
// - El patron if err != nil en todas sus variantes
// - Named returns para cleanup con defer
// - Happy path a la izquierda
// - Reduccion de repeticion con closures y tablas
// - Decisiones en el bloque if err != nil (retry, fallback, log, transform)
// - Error handling en loops (acumular, abortar, partial success)
// - Patron validate-then-execute
//
// Compilar y ejecutar:
//   go run main.go
// ============================================================================

package main

import (
    "errors"
    "fmt"
    "math/rand"
    "strings"
    "time"
)

// ============================================================================
// PARTE 1: Errores y tipos
// ============================================================================

var (
    ErrServiceDown    = errors.New("service is down")
    ErrTimeout        = errors.New("operation timed out")
    ErrInvalidConfig  = errors.New("invalid configuration")
    ErrRollbackFailed = errors.New("rollback failed")
)

type DeployError struct {
    Service string
    Phase   string
    Cause   error
}

func (e *DeployError) Error() string {
    return fmt.Sprintf("deploy %s [%s]: %v", e.Service, e.Phase, e.Cause)
}

func (e *DeployError) Unwrap() error {
    return e.Cause
}

type DeployConfig struct {
    Service  string
    Image    string
    Replicas int
    Port     int
    Env      string
    Timeout  time.Duration
}

type DeployResult struct {
    Service   string
    Status    string
    StartedAt time.Time
    Duration  time.Duration
}

func (r *DeployResult) String() string {
    return fmt.Sprintf("%-20s [%s] started=%s took=%v",
        r.Service, r.Status, r.StartedAt.Format("15:04:05"), r.Duration)
}

// ============================================================================
// PARTE 2: Simulaciones de operaciones
// ============================================================================

func pullImage(image string) error {
    if strings.Contains(image, "broken") {
        return fmt.Errorf("pull %s: %w", image, errors.New("image not found in registry"))
    }
    return nil
}

func stopContainer(name string) error {
    if strings.Contains(name, "stuck") {
        return fmt.Errorf("stop %s: %w", name, ErrTimeout)
    }
    return nil
}

func startContainer(name, image string, port int) error {
    if port == 0 {
        return fmt.Errorf("start %s: %w", name, ErrInvalidConfig)
    }
    return nil
}

func healthCheck(name string) error {
    // Simular check con probabilidad de fallo
    if strings.Contains(name, "flaky") && rand.Intn(3) == 0 {
        return &DeployError{Service: name, Phase: "health", Cause: ErrServiceDown}
    }
    return nil
}

func rollbackContainer(name string) error {
    if strings.Contains(name, "norb") {
        return fmt.Errorf("rollback %s: %w", name, ErrRollbackFailed)
    }
    return nil
}

func notifySlack(msg string) error {
    // Best-effort — nunca falla en esta demo
    return nil
}

// ============================================================================
// PARTE 3: Validate-then-execute con named returns
// ============================================================================

// Validate — acumular todos los errores antes de ejecutar
func (cfg *DeployConfig) Validate() error {
    var errs []error

    if cfg.Service == "" {
        errs = append(errs, fmt.Errorf("service: %w", ErrInvalidConfig))
    }
    if cfg.Image == "" {
        errs = append(errs, fmt.Errorf("image: %w", ErrInvalidConfig))
    }
    if cfg.Replicas < 1 || cfg.Replicas > 10 {
        errs = append(errs, fmt.Errorf("replicas must be 1-10, got %d", cfg.Replicas))
    }
    if cfg.Port < 1 || cfg.Port > 65535 {
        errs = append(errs, fmt.Errorf("port must be 1-65535, got %d", cfg.Port))
    }
    if cfg.Timeout <= 0 {
        cfg.Timeout = 30 * time.Second // default, no es error
    }

    return errors.Join(errs...) // nil si no hay errores
}

// Deploy — named return para cleanup con defer
func Deploy(cfg DeployConfig) (result *DeployResult, err error) {
    startTime := time.Now()

    // Fase 0: Validar
    if err = cfg.Validate(); err != nil {
        return nil, fmt.Errorf("validate: %w", err)
    }

    // defer para cleanup en caso de error — usa named return
    defer func() {
        if err != nil {
            // Hubo error — intentar rollback
            fmt.Printf("    ⟲ Rolling back %s...\n", cfg.Service)
            if rbErr := rollbackContainer(cfg.Service); rbErr != nil {
                // Rollback tambien fallo — combinar errores
                err = fmt.Errorf("%w (rollback also failed: %v)", err, rbErr)
            } else {
                fmt.Printf("    ⟲ Rollback OK\n")
            }

            // Notificar (best-effort)
            if notifyErr := notifySlack(fmt.Sprintf("Deploy of %s FAILED: %v", cfg.Service, err)); notifyErr != nil {
                // Log pero no cambiar err — la notificacion es secundaria
                fmt.Printf("    ⚠ Slack notification failed: %v\n", notifyErr)
            }
        }
    }()

    // Fase 1: Pull — if err != nil con contexto
    fmt.Printf("    ▸ Pulling %s...\n", cfg.Image)
    if err = pullImage(cfg.Image); err != nil {
        return nil, &DeployError{Service: cfg.Service, Phase: "pull", Cause: err}
    }

    // Fase 2: Stop — if err != nil con contexto
    fmt.Printf("    ▸ Stopping old container...\n")
    if err = stopContainer(cfg.Service); err != nil {
        return nil, &DeployError{Service: cfg.Service, Phase: "stop", Cause: err}
    }

    // Fase 3: Start — if err != nil con contexto
    fmt.Printf("    ▸ Starting new container (port %d)...\n", cfg.Port)
    if err = startContainer(cfg.Service, cfg.Image, cfg.Port); err != nil {
        return nil, &DeployError{Service: cfg.Service, Phase: "start", Cause: err}
    }

    // Fase 4: Health check — con RETRY
    fmt.Printf("    ▸ Health check...\n")
    var healthErr error
    for attempt := 1; attempt <= 3; attempt++ {
        healthErr = healthCheck(cfg.Service)
        if healthErr == nil {
            break
        }
        fmt.Printf("    ⟳ Health check attempt %d/3 failed: %v\n", attempt, healthErr)
        time.Sleep(100 * time.Millisecond) // simular backoff
    }
    if healthErr != nil {
        err = &DeployError{Service: cfg.Service, Phase: "health", Cause: healthErr}
        return nil, err // defer hara rollback
    }

    duration := time.Since(startTime)
    return &DeployResult{
        Service:   cfg.Service,
        Status:    "RUNNING",
        StartedAt: time.Now(),
        Duration:  duration,
    }, nil
}

// ============================================================================
// PARTE 4: Batch deploy con error handling en loops
// ============================================================================

type BatchDeployResult struct {
    Succeeded []*DeployResult
    Failed    []error
}

func (bdr *BatchDeployResult) Summary() string {
    var b strings.Builder
    total := len(bdr.Succeeded) + len(bdr.Failed)
    fmt.Fprintf(&b, "Deployed %d/%d services successfully\n", len(bdr.Succeeded), total)

    if len(bdr.Succeeded) > 0 {
        b.WriteString("\n  Succeeded:\n")
        for _, r := range bdr.Succeeded {
            fmt.Fprintf(&b, "    ✓ %s\n", r)
        }
    }

    if len(bdr.Failed) > 0 {
        b.WriteString("\n  Failed:\n")
        for _, err := range bdr.Failed {
            fmt.Fprintf(&b, "    ✗ %s\n", err)
        }
    }

    return b.String()
}

// DeployAll — loop con partial success
func DeployAll(configs []DeployConfig, maxFailures int) *BatchDeployResult {
    result := &BatchDeployResult{}

    for _, cfg := range configs {
        fmt.Printf("\n  ── Deploying %s ──\n", cfg.Service)

        deployResult, err := Deploy(cfg)
        if err != nil {
            result.Failed = append(result.Failed, err)
            fmt.Printf("    ✗ FAILED: %v\n", err)

            // ¿Abortar si demasiados fallos?
            if len(result.Failed) >= maxFailures {
                result.Failed = append(result.Failed,
                    fmt.Errorf("aborting: reached max failures (%d)", maxFailures))
                return result
            }
            continue // siguiente servicio
        }

        result.Succeeded = append(result.Succeeded, deployResult)
        fmt.Printf("    ✓ OK: %s\n", deployResult)
    }

    return result
}

// ============================================================================
// PARTE 5: Setup con tabla de operaciones (reducir repeticion)
// ============================================================================

func setupEnvironment(env string) error {
    // Closure que encapsula el patron comun
    step := func(name string, fn func() error) error {
        fmt.Printf("  ▸ %s... ", name)
        if err := fn(); err != nil {
            fmt.Println("FAIL")
            return fmt.Errorf("%s: %w", name, err)
        }
        fmt.Println("OK")
        return nil
    }

    // Tabla de operaciones — una linea por step
    steps := []struct {
        name string
        fn   func() error
    }{
        {"Create network", func() error { return nil }},
        {"Initialize DNS", func() error { return nil }},
        {"Setup monitoring", func() error { return nil }},
        {"Configure firewall", func() error {
            if env == "broken" {
                return errors.New("iptables command failed")
            }
            return nil
        }},
        {"Verify connectivity", func() error { return nil }},
    }

    for _, s := range steps {
        if err := step(s.name, s.fn); err != nil {
            return fmt.Errorf("setup %s environment: %w", env, err)
        }
    }

    return nil
}

// ============================================================================
// PARTE 6: main
// ============================================================================

func main() {
    fmt.Println("╔══════════════════════════════════════════════════════════╗")
    fmt.Println("║     Service Deployment Pipeline — if err != nil         ║")
    fmt.Println("╚══════════════════════════════════════════════════════════╝")

    // ─── Setup environment (tabla de operaciones) ───
    fmt.Println()
    fmt.Println("=== Phase 1: Environment Setup ===")
    fmt.Println()

    if err := setupEnvironment("production"); err != nil {
        fmt.Printf("Environment setup failed: %v\n", err)
        fmt.Println("(In production, we'd abort here)")
    } else {
        fmt.Println("  Environment ready")
    }

    // ─── Deploy batch (loop con partial success) ───
    fmt.Println()
    fmt.Println("=== Phase 2: Service Deployment ===")

    configs := []DeployConfig{
        {
            Service:  "api-gateway",
            Image:    "gateway:v2.1",
            Replicas: 3,
            Port:     8080,
            Timeout:  10 * time.Second,
        },
        {
            Service:  "auth-service",
            Image:    "broken-auth:v1.0", // ← imagen rota
            Replicas: 2,
            Port:     8081,
            Timeout:  10 * time.Second,
        },
        {
            Service:  "user-service",
            Image:    "users:v3.0",
            Replicas: 2,
            Port:     8082,
            Timeout:  10 * time.Second,
        },
        {
            Service:  "stuck-service", // ← no se puede detener
            Image:    "stuck:v1.0",
            Replicas: 1,
            Port:     8083,
            Timeout:  10 * time.Second,
        },
        {
            Service:  "cache-service",
            Image:    "redis:7",
            Replicas: 1,
            Port:     6379,
            Timeout:  5 * time.Second,
        },
    }

    batchResult := DeployAll(configs, 10) // max 10 failures before abort

    // ─── Resumen ───
    fmt.Println()
    fmt.Println("=== Phase 3: Summary ===")
    fmt.Println()
    fmt.Println(batchResult.Summary())

    // ─── Demo: errores inspeccionables ───
    fmt.Println("=== Phase 4: Error Inspection ===")
    fmt.Println()

    for _, err := range batchResult.Failed {
        fmt.Printf("  Error: %v\n", err)

        // errors.Is busca en la cadena
        if errors.Is(err, ErrTimeout) {
            fmt.Println("    → Type: TIMEOUT (retriable)")
        }
        if errors.Is(err, ErrInvalidConfig) {
            fmt.Println("    → Type: INVALID CONFIG (fix and retry)")
        }

        // errors.As extrae tipo concreto
        var deployErr *DeployError
        if errors.As(err, &deployErr) {
            fmt.Printf("    → Service: %s, Phase: %s\n", deployErr.Service, deployErr.Phase)
        }

        fmt.Println()
    }

    // ─── Demo: validacion (validate-then-execute) ───
    fmt.Println("=== Phase 5: Validation Demo ===")
    fmt.Println()

    badConfig := DeployConfig{
        Service:  "",
        Image:    "",
        Replicas: 0,
        Port:     99999,
    }

    if err := badConfig.Validate(); err != nil {
        fmt.Println("  Validation failed:")
        for _, line := range strings.Split(err.Error(), "\n") {
            fmt.Printf("    %s\n", line)
        }
    }

    fmt.Println()
    fmt.Println(strings.Repeat("─", 58))
    fmt.Println()
    fmt.Println("  PATRONES DEMOSTRADOS:")
    fmt.Println("  1. if err != nil — basico, inline, return directo")
    fmt.Println("  2. Named returns + defer — cleanup con rollback")
    fmt.Println("  3. Happy path a la izquierda — early return")
    fmt.Println("  4. Tabla de operaciones — reducir repeticion")
    fmt.Println("  5. Retry en health check — backoff en el if block")
    fmt.Println("  6. Loop con partial success — acumular errores")
    fmt.Println("  7. Validate-then-execute — errores.Join")
    fmt.Println("  8. errors.Is/As — inspeccion a traves de la cadena")
    fmt.Println()
}
```

**Salida esperada (parcial):**
```
╔══════════════════════════════════════════════════════════╗
║     Service Deployment Pipeline — if err != nil         ║
╚══════════════════════════════════════════════════════════╝

=== Phase 1: Environment Setup ===

  ▸ Create network... OK
  ▸ Initialize DNS... OK
  ▸ Setup monitoring... OK
  ▸ Configure firewall... OK
  ▸ Verify connectivity... OK
  Environment ready

=== Phase 2: Service Deployment ===

  ── Deploying api-gateway ──
    ▸ Pulling gateway:v2.1...
    ▸ Stopping old container...
    ▸ Starting new container (port 8080)...
    ▸ Health check...
    ✓ OK: api-gateway           [RUNNING] ...

  ── Deploying auth-service ──
    ▸ Pulling broken-auth:v1.0...
    ⟲ Rolling back auth-service...
    ⟲ Rollback OK
    ✗ FAILED: deploy auth-service [pull]: pull broken-auth:v1.0: image not found

  ── Deploying stuck-service ──
    ▸ Pulling stuck:v1.0...
    ▸ Stopping old container...
    ⟲ Rolling back stuck-service...
    ⟲ Rollback OK
    ✗ FAILED: deploy stuck-service [stop]: stop stuck-service: operation timed out

=== Phase 3: Summary ===

Deployed 3/5 services successfully

  Failed:
    ✗ deploy auth-service [pull]: ...
    ✗ deploy stuck-service [stop]: ...
```

---

## 14. Ejercicios

### Ejercicio 1: Refactorizar con happy path a la izquierda

```go
// Refactoriza esta funcion para que el happy path este a la izquierda.
// No cambies la logica — solo la estructura.

func getServerStatus(name string, port int) (string, error) {
    if name != "" {
        if port > 0 && port < 65536 {
            conn, err := connect(name, port)
            if err == nil {
                status, err := conn.Status()
                if err == nil {
                    return status, nil
                } else {
                    return "", fmt.Errorf("status: %w", err)
                }
            } else {
                return "", fmt.Errorf("connect: %w", err)
            }
        } else {
            return "", fmt.Errorf("invalid port: %d", port)
        }
    } else {
        return "", errors.New("empty name")
    }
}
```

<details>
<summary>Solucion</summary>

```go
func getServerStatus(name string, port int) (string, error) {
    if name == "" {
        return "", errors.New("empty name")
    }

    if port < 1 || port > 65535 {
        return "", fmt.Errorf("invalid port: %d", port)
    }

    conn, err := connect(name, port)
    if err != nil {
        return "", fmt.Errorf("connect: %w", err)
    }

    status, err := conn.Status()
    if err != nil {
        return "", fmt.Errorf("status: %w", err)
    }

    return status, nil
}
```
</details>

### Ejercicio 2: Named returns para transaccion

```go
// Implementa una funcion transferData que:
// 1. Abre un archivo fuente (puede fallar)
// 2. Abre un archivo destino (puede fallar — debe cerrar fuente)
// 3. Copia datos (puede fallar — debe cerrar ambos)
// 4. Si todo salio bien, cierra ambos archivos
// 5. Si algo fallo, cierra lo que se abrio y elimina el destino parcial
//
// Usa named returns y defer para el cleanup.
// Hint: el defer debe verificar err para decidir si eliminar el destino.

func transferData(srcPath, dstPath string) (written int64, err error) {
    // tu codigo aqui
}
```

<details>
<summary>Solucion</summary>

```go
func transferData(srcPath, dstPath string) (written int64, err error) {
    src, err := os.Open(srcPath)
    if err != nil {
        return 0, fmt.Errorf("open source %s: %w", srcPath, err)
    }
    defer src.Close()

    dst, err := os.Create(dstPath)
    if err != nil {
        return 0, fmt.Errorf("create dest %s: %w", dstPath, err)
    }

    defer func() {
        closeErr := dst.Close()
        if err != nil {
            // Hubo error en la copia — eliminar archivo parcial
            os.Remove(dstPath)
        } else if closeErr != nil {
            // La copia fue OK pero Close fallo
            err = fmt.Errorf("close dest: %w", closeErr)
            os.Remove(dstPath)
        }
    }()

    written, err = io.Copy(dst, src)
    if err != nil {
        return written, fmt.Errorf("copy: %w", err)
    }

    return written, nil
}
```
</details>

### Ejercicio 3: Pipeline con tabla de operaciones

```go
// Implementa una funcion que ejecute una secuencia de health checks
// usando el patron de tabla de operaciones.
// Cada check debe:
// - Imprimirse como "Checking <name>... OK/FAIL"
// - Si falla, agregar contexto con fmt.Errorf y el nombre
// - Si falla, NO abortar — acumular errores
// - Al final, devolver todos los errores con errors.Join
//
// Services a verificar: "database", "cache", "queue", "api", "dns"
// Simula fallo en "queue" y "dns"

func runHealthChecks() error {
    // tu codigo aqui
}
```

<details>
<summary>Solucion</summary>

```go
func runHealthChecks() error {
    checks := []struct {
        name string
        fn   func() error
    }{
        {"database", func() error { return nil }},
        {"cache", func() error { return nil }},
        {"queue", func() error { return errors.New("connection refused") }},
        {"api", func() error { return nil }},
        {"dns", func() error { return errors.New("NXDOMAIN") }},
    }

    var errs []error
    for _, check := range checks {
        fmt.Printf("  Checking %s... ", check.name)
        if err := check.fn(); err != nil {
            fmt.Println("FAIL")
            errs = append(errs, fmt.Errorf("check %s: %w", check.name, err))
        } else {
            fmt.Println("OK")
        }
    }

    return errors.Join(errs...)
}
```
</details>

### Ejercicio 4: Identificar shadowing

```go
// Este codigo tiene un bug de shadowing.
// Encuentra la linea exacta donde ocurre y explica por que
// la funcion puede devolver nil cuando no deberia.

func loadAndValidate(path string) (err error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("read: %w", err)
    }

    if len(data) > 0 {
        result, err := validate(data) // ← ¿hay shadowing aqui?
        if err != nil {
            return fmt.Errorf("validate: %w", err)
        }
        fmt.Println("Valid:", result)
    }

    // ¿que valor tiene err aqui si validate fallo y la linea 
    // de arriba NO entro al if err != nil?
    return err
}
```

<details>
<summary>Solucion</summary>

```go
// ANALISIS:
// Linea: result, err := validate(data)
//
// Esta linea USA := dentro de un bloque if.
// "result" es nueva → Go permite := 
// "err" parece que se reasigna, pero CUIDADO:
// Dentro del bloque if, := crea una NUEVA variable err
// que SOMBREA la err del named return.
//
// Si validate() tiene exito (err local == nil):
// - La err del named return sigue siendo nil (la de ReadFile ya fue nil)
// - CORRECTO en este caso
//
// Pero si validate() falla y err != nil:
// - El return dentro del if devuelve la err local — OK
// - Si por alguna razon el flujo continua (bug), 
//   la err del named return no tiene el error
//
// En ESTE codigo especifico, el return dentro del if previene el bug.
// Pero si cambiaras la logica (quitar el return), el shadowing
// causaria que la funcion devolviera nil ignorando el error de validate.

// Version SEGURA (sin shadowing):
func loadAndValidate(path string) (err error) {
    data, err := os.ReadFile(path)
    if err != nil {
        return fmt.Errorf("read: %w", err)
    }

    if len(data) > 0 {
        var result string
        result, err = validate(data) // = en vez de := — asigna a named return
        if err != nil {
            return fmt.Errorf("validate: %w", err)
        }
        fmt.Println("Valid:", result)
    }

    return err // ahora err refleja el resultado real de validate
}
```
</details>

---

## 15. Resumen Visual

```
┌──────────────────────────────────────────────────────────────────────────┐
│  EL PATRON if err != nil — Resumen                                       │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  EL PATRON BASICO:                                                       │
│  ├─ result, err := f()                                                  │
│  ├─ if err != nil { return fmt.Errorf("ctx: %w", err) }                │
│  └─ // usar result (happy path a la izquierda)                          │
│                                                                          │
│  VARIANTES:                                                              │
│  ├─ Inline: if err := f(); err != nil { ... }                           │
│  ├─ Reutilizar err: x, err = f1() ; y, err = f2()                      │
│  ├─ Return directo: return f() (ultimo paso)                            │
│  └─ Named returns: func f() (result T, err error) + defer              │
│                                                                          │
│  REDUCIR REPETICION:                                                     │
│  ├─ errWriter: acumular escrituras, verificar una vez                   │
│  ├─ Tabla de operaciones: []struct{name, fn} + loop                     │
│  ├─ Closures: step := func(name, fn) error { ... }                     │
│  ├─ Pipeline: p.Then(f1).Then(f2).Error()                               │
│  └─ Validate-then-execute: validar todo, luego ejecutar                 │
│                                                                          │
│  DECISIONES en if err != nil:                                            │
│  ├─ Propagar con contexto (fmt.Errorf %w) — mas comun                  │
│  ├─ Log y continuar — errores no criticos                               │
│  ├─ Reintentar — errores transitorios (red, timeout)                    │
│  ├─ Fallback a default — config ausente                                 │
│  ├─ Transformar — error interno → respuesta HTTP                        │
│  └─ Fatal — recurso critico en inicializacion                           │
│                                                                          │
│  NAMED RETURNS + defer:                                                  │
│  ├─ defer puede leer/modificar err                                      │
│  ├─ Transacciones: commit si err==nil, rollback si err!=nil             │
│  ├─ Close() que puede fallar: err = closeErr si err==nil                │
│  └─ CUIDADO: shadowing con := dentro de bloques                        │
│                                                                          │
│  LOOPS:                                                                  │
│  ├─ Abortar en primer error (return err)                                │
│  ├─ Acumular errores (errors.Join)                                      │
│  ├─ Limite de errores (break si len(errs) >= max)                       │
│  └─ Partial success (BatchResult con succeeded/failed)                  │
│                                                                          │
│  HAPPY PATH:                                                             │
│  ├─ Siempre a la izquierda (margen sin indentar)                        │
│  ├─ Errores causan return temprano (early return)                       │
│  ├─ No anidar if err == nil { if err == nil { ... } }                   │
│  └─ "Line of sight" — leer de arriba a abajo por el margen             │
│                                                                          │
│  vs C: goto cleanup (defer de Go es el reemplazo limpio)                │
│  vs Rust: ? operator (1 char vs 3 lineas, pero Go tiene mas control)   │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: T03 - Wrapping de errores — fmt.Errorf con %w, errors.Is, errors.As, cadena de errores
