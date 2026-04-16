# Context — context.Background, WithCancel, WithTimeout, WithValue, propagacion de cancelacion

## 1. Introduccion

El paquete `context` es la columna vertebral de la cancelacion, timeouts, y propagacion de metadata en Go. Cada operacion que cruza boundaries (HTTP requests, database queries, RPC calls, pipelines) deberia aceptar un `context.Context` como **primer parametro**. Sin context, no hay forma estandar de decirle a una operacion "para, ya no te necesito" — y eso lleva a goroutine leaks, timeouts silenciosos, y recursos desperdiciados.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CONTEXT EN GO                                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Context resuelve 3 problemas:                                          │
│                                                                          │
│  1. CANCELACION: "deja de trabajar, el resultado ya no importa"        │
│     ├─ El usuario cerro la conexion                                    │
│     ├─ Un request padre fue cancelado                                  │
│     └─ El servidor esta haciendo shutdown                              │
│                                                                          │
│  2. DEADLINES/TIMEOUTS: "si no terminas en X tiempo, para"             │
│     ├─ SLA: "este endpoint debe responder en < 500ms"                  │
│     ├─ Circuit breaker: "si la DB no responde en 2s, fail fast"       │
│     └─ Cascading timeouts: cada etapa reduce el deadline               │
│                                                                          │
│  3. REQUEST-SCOPED VALUES: metadata que viaja con la request           │
│     ├─ Request ID, trace ID, user ID                                   │
│     ├─ Auth tokens, locale                                              │
│     └─ (usar con cuidado — no para dependencias, solo metadata)       │
│                                                                          │
│  DISEÑO:                                                                 │
│  ├─ Inmutable: crear derivados con With* (arbol de contexts)           │
│  ├─ Thread-safe: cualquier goroutine puede leerlo                      │
│  ├─ Composable: los With* forman un arbol de padre→hijo               │
│  ├─ Cancelacion se propaga: cancelar padre cancela TODOS los hijos    │
│  └─ Convencion: SIEMPRE primer parametro, nombre ctx                  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

Para SysAdmin/DevOps:
- Todo HTTP handler en Go recibe `r.Context()` — la base para timeouts y cancelacion
- kubectl, terraform, y herramientas CLI usan context para manejar Ctrl+C (signal propagation)
- Database drivers (`database/sql`), HTTP clients, y gRPC usan context para deadlines
- En microservicios, el context lleva trace IDs (OpenTelemetry) para distributed tracing
- Sin context, un request que timeout en el load balancer puede seguir consumiendo CPU en el backend

---

## 2. La Interfaz context.Context

### 2.1 Definicion

```go
// context.Context es una INTERFAZ con 4 metodos
type Context interface {
    // Deadline retorna el tiempo limite, si existe
    // ok es false si no hay deadline
    Deadline() (deadline time.Time, ok bool)
    
    // Done retorna un channel que se cierra cuando el context se cancela
    // Recibir de Done() bloquea hasta la cancelacion
    // Multiples goroutines pueden leer de Done() simultaneamente
    Done() <-chan struct{}
    
    // Err retorna nil si Done no esta cerrado
    // Retorna Canceled si fue cancelado explicitamente
    // Retorna DeadlineExceeded si el deadline expiro
    Err() error
    
    // Value retorna el valor asociado a una key, o nil
    // Las keys deben ser comparable y tipicamente son tipos no exportados
    Value(key any) any
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CONTEXT — LOS 4 METODOS                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Deadline() (time.Time, bool)                                           │
│  ├─ "¿Hay un tiempo limite? ¿Cual es?"                                │
│  ├─ ok=false si no hay deadline                                        │
│  └─ Util para decidir si vale la pena empezar una operacion larga     │
│                                                                          │
│  Done() <-chan struct{}                                                  │
│  ├─ "Dame un channel para saber cuando cancelarse"                    │
│  ├─ Se usa en select: case <-ctx.Done():                              │
│  ├─ Se cierra cuando: cancel() se llama O deadline expira             │
│  └─ Background/TODO retornan nil (nunca se cancelan)                  │
│                                                                          │
│  Err() error                                                             │
│  ├─ nil → context sigue activo                                         │
│  ├─ context.Canceled → alguien llamo cancel()                          │
│  ├─ context.DeadlineExceeded → el deadline expiro                     │
│  └─ Llamar DESPUES de <-ctx.Done() para saber la razon                │
│                                                                          │
│  Value(key) any                                                          │
│  ├─ Busca key en este context y luego sube por los padres             │
│  ├─ Retorna nil si no se encuentra                                     │
│  └─ NO usar para pasar dependencias — solo metadata                   │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Context Raiz: Background y TODO

Toda cadena de context empieza con una raiz. Go provee dos:

```go
// context.Background — el context raiz estandar
// Nunca se cancela, no tiene deadline, no tiene valores
// Usar en: main(), init(), tests, y como raiz de toda la cadena
ctx := context.Background()

// context.TODO — marcador temporal
// Semanticamente identico a Background, pero indica:
// "No se que context usar todavia, debo revisitar esto"
// Usar cuando: aun no has implementado context propagation
ctx := context.TODO()
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    BACKGROUND vs TODO                                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Background():                                                           │
│  ├─ "Este es el inicio de la cadena, conscientemente"                  │
│  ├─ Usar en main(), init(), test setup                                │
│  └─ Nunca se cancela (Done() retorna nil)                              │
│                                                                          │
│  TODO():                                                                 │
│  ├─ "Deberia propagar context pero aun no lo hago"                    │
│  ├─ Usar temporalmente en codigo legacy                               │
│  └─ Herramientas de linting pueden detectar TODO y advertir           │
│                                                                          │
│  INTERNAMENTE son lo mismo (emptyCtx), la diferencia es semantica.     │
│                                                                          │
│  REGLA: Si ves context.TODO() en un code review, pregunta:            │
│  "¿De donde deberia venir este context?"                               │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 4. WithCancel — Cancelacion Explicita

`WithCancel` crea un context hijo que se puede cancelar manualmente llamando a la funcion `cancel()`:

```go
func WithCancel(parent Context) (ctx Context, cancel CancelFunc)
// CancelFunc = func()
```

### 4.1 Uso basico

```go
package main

import (
    "context"
    "fmt"
    "time"
)

func worker(ctx context.Context, id int) {
    for {
        select {
        case <-ctx.Done():
            fmt.Printf("  Worker %d: cancelado (%v)\n", id, ctx.Err())
            return
        default:
            fmt.Printf("  Worker %d: trabajando...\n", id)
            time.Sleep(300 * time.Millisecond)
        }
    }
}

func main() {
    // Crear context cancelable
    ctx, cancel := context.WithCancel(context.Background())
    
    // Iniciar workers
    for i := 1; i <= 3; i++ {
        go worker(ctx, i)
    }
    
    // Dejar trabajar 1 segundo
    time.Sleep(1 * time.Second)
    
    // Cancelar — TODOS los workers reciben la senal
    fmt.Println("\n--- Cancelando ---")
    cancel()
    
    time.Sleep(100 * time.Millisecond) // dar tiempo al cleanup
    fmt.Println("Todos los workers terminaron")
    
    // cancel() es idempotente — llamar multiples veces es safe
    cancel() // no-op
    cancel() // no-op
}
```

### 4.2 defer cancel() — SIEMPRE

```go
// REGLA: SIEMPRE hacer defer cancel() despues de WithCancel/WithTimeout/WithDeadline
// Incluso si el context expira naturalmente, cancel() libera recursos internos

func good(ctx context.Context) error {
    ctx, cancel := context.WithCancel(ctx)
    defer cancel() // SIEMPRE — libera recursos aunque el context no se cancele manualmente
    
    // ... usar ctx ...
    return nil
}

// Sin defer cancel(), el goroutine interno del timer (en WithTimeout)
// queda vivo hasta que el parent se cancele — goroutine leak
func bad(ctx context.Context) error {
    ctx, _ = context.WithTimeout(ctx, 5*time.Second)
    // ← goroutine leak: el timer corre por 5 segundos incluso si terminamos antes
    
    // ... usar ctx ...
    return nil
}
```

### 4.3 WithCancelCause (Go 1.20+)

```go
package main

import (
    "context"
    "errors"
    "fmt"
)

func main() {
    ctx, cancel := context.WithCancelCause(context.Background())
    
    // Cancelar con una RAZON especifica
    cancel(errors.New("user disconnected"))
    
    fmt.Println("Err:", ctx.Err())                    // context canceled
    fmt.Println("Cause:", context.Cause(ctx))          // user disconnected
    
    // Sin WithCancelCause, solo sabes "fue cancelado" pero no POR QUE
    // Con Cause, puedes distinguir entre diferentes razones de cancelacion
}
```

---

## 5. WithTimeout y WithDeadline — Limites de Tiempo

### 5.1 WithTimeout

```go
func WithTimeout(parent Context, timeout time.Duration) (Context, CancelFunc)
```

Crea un context que se cancela automaticamente despues de `timeout`:

```go
package main

import (
    "context"
    "fmt"
    "time"
)

func slowOperation(ctx context.Context) (string, error) {
    // Simular operacion que tarda 3 segundos
    select {
    case <-time.After(3 * time.Second):
        return "resultado", nil
    case <-ctx.Done():
        return "", ctx.Err()
    }
}

func main() {
    // Timeout de 1 segundo
    ctx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
    defer cancel()
    
    fmt.Println("Iniciando operacion con timeout de 1s...")
    result, err := slowOperation(ctx)
    
    if err != nil {
        fmt.Printf("Error: %v\n", err) // context deadline exceeded
    } else {
        fmt.Printf("Resultado: %s\n", result)
    }
    
    // Verificar deadline
    if deadline, ok := ctx.Deadline(); ok {
        fmt.Printf("Deadline era: %v\n", deadline.Format("15:04:05.000"))
        fmt.Printf("Tiempo restante: %v\n", time.Until(deadline))
    }
}
```

### 5.2 WithDeadline

```go
func WithDeadline(parent Context, d time.Time) (Context, CancelFunc)
```

Igual que WithTimeout pero con un **punto en el tiempo** absoluto:

```go
package main

import (
    "context"
    "fmt"
    "time"
)

func main() {
    // Deadline: dentro de 2 segundos desde ahora
    deadline := time.Now().Add(2 * time.Second)
    ctx, cancel := context.WithDeadline(context.Background(), deadline)
    defer cancel()
    
    fmt.Printf("Deadline: %v\n", deadline.Format("15:04:05.000"))
    fmt.Printf("Ahora:    %v\n", time.Now().Format("15:04:05.000"))
    
    // Verificar cuanto tiempo queda
    if dl, ok := ctx.Deadline(); ok {
        fmt.Printf("Queda: %v\n", time.Until(dl).Round(time.Millisecond))
    }
    
    // WithTimeout(parent, d) es equivalente a:
    // WithDeadline(parent, time.Now().Add(d))
}
```

### 5.3 Deadline inheritance — el mas estricto gana

```go
package main

import (
    "context"
    "fmt"
    "time"
)

func main() {
    // Parent con timeout de 5 segundos
    parentCtx, parentCancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer parentCancel()
    
    // Hijo con timeout de 2 segundos (MAS estricto que el padre)
    childCtx, childCancel := context.WithTimeout(parentCtx, 2*time.Second)
    defer childCancel()
    
    // El hijo se cancelara en 2s, no en 5s
    dl, _ := childCtx.Deadline()
    fmt.Printf("Child deadline: %v from now\n", time.Until(dl).Round(time.Millisecond))
    // ~2s
    
    // Hijo con timeout de 10 segundos (MENOS estricto que el padre)
    childCtx2, childCancel2 := context.WithTimeout(parentCtx, 10*time.Second)
    defer childCancel2()
    
    // El hijo hereda el deadline del padre (5s), no 10s
    dl2, _ := childCtx2.Deadline()
    fmt.Printf("Child2 deadline: %v from now\n", time.Until(dl2).Round(time.Millisecond))
    // ~5s (hereda del padre, que es mas estricto)
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    DEADLINE INHERITANCE                                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  REGLA: el deadline mas estricto (mas cercano) SIEMPRE gana.           │
│                                                                          │
│  Background (sin deadline)                                               │
│  └─ WithTimeout(5s) → deadline = now+5s                                │
│     ├─ WithTimeout(2s) → deadline = now+2s (mas estricto, gana)       │
│     ├─ WithTimeout(10s) → deadline = now+5s (hereda padre, +estricto) │
│     └─ WithCancel() → deadline = now+5s (hereda del padre)            │
│                                                                          │
│  PROPAGACION DE CANCELACION:                                            │
│  ├─ Cancelar el padre → cancela TODOS los hijos                       │
│  ├─ Cancelar un hijo → NO cancela al padre ni a hermanos              │
│  └─ Deadline del padre expira → cancela a todos los hijos             │
│                                                                          │
│  CASCADING TIMEOUTS EN MICROSERVICIOS:                                  │
│                                                                          │
│  API Gateway (timeout: 10s)                                             │
│  └─ User Service (timeout: 5s, hereda 10s del gateway)                │
│     └─ Database Query (timeout: 2s, hereda 5s del service)             │
│        └─ Connection Pool Get (timeout: 500ms)                         │
│                                                                          │
│  Cada capa puede poner su propio timeout, pero NUNCA excede           │
│  el del padre. Si el gateway cancela, TODO el arbol se cancela.       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.4 Verificar deadline antes de empezar

```go
package main

import (
    "context"
    "fmt"
    "time"
)

func expensiveOperation(ctx context.Context) error {
    // Verificar si tiene sentido empezar
    if deadline, ok := ctx.Deadline(); ok {
        remaining := time.Until(deadline)
        if remaining < 100*time.Millisecond {
            return fmt.Errorf("not enough time: %v remaining, need at least 100ms", remaining)
        }
        fmt.Printf("  Starting with %v remaining\n", remaining.Round(time.Millisecond))
    }
    
    // Operacion larga con checkpoints de cancelacion
    steps := []string{"connecting", "querying", "processing", "formatting"}
    for _, step := range steps {
        select {
        case <-ctx.Done():
            return fmt.Errorf("cancelled during %s: %w", step, ctx.Err())
        case <-time.After(200 * time.Millisecond):
            fmt.Printf("  Step: %s done\n", step)
        }
    }
    
    return nil
}

func main() {
    // Con suficiente tiempo
    ctx1, cancel1 := context.WithTimeout(context.Background(), 2*time.Second)
    defer cancel1()
    
    fmt.Println("=== Con 2s de timeout ===")
    if err := expensiveOperation(ctx1); err != nil {
        fmt.Printf("  Error: %v\n", err)
    } else {
        fmt.Println("  Completado!")
    }
    
    // Sin suficiente tiempo
    ctx2, cancel2 := context.WithTimeout(context.Background(), 500*time.Millisecond)
    defer cancel2()
    
    fmt.Println("\n=== Con 500ms de timeout ===")
    if err := expensiveOperation(ctx2); err != nil {
        fmt.Printf("  Error: %v\n", err)
    }
}
```

---

## 6. WithValue — Metadata en el Context

### 6.1 API

```go
func WithValue(parent Context, key, val any) Context
```

`WithValue` agrega un par key-value al context. El valor se busca subiendo por la cadena de padres.

### 6.2 Uso correcto

```go
package main

import (
    "context"
    "fmt"
)

// REGLA: usar tipos no exportados como keys para evitar colisiones
type contextKey string

const (
    requestIDKey contextKey = "requestID"
    userIDKey    contextKey = "userID"
)

// Helpers tipados — encapsulan el type assertion
func WithRequestID(ctx context.Context, id string) context.Context {
    return context.WithValue(ctx, requestIDKey, id)
}

func RequestID(ctx context.Context) string {
    if id, ok := ctx.Value(requestIDKey).(string); ok {
        return id
    }
    return "unknown"
}

func WithUserID(ctx context.Context, id int) context.Context {
    return context.WithValue(ctx, userIDKey, id)
}

func UserID(ctx context.Context) int {
    if id, ok := ctx.Value(userIDKey).(int); ok {
        return id
    }
    return 0
}

// Los handlers usan los helpers — nunca acceden a las keys directamente
func handleRequest(ctx context.Context) {
    fmt.Printf("  Request %s from user %d\n", RequestID(ctx), UserID(ctx))
}

func main() {
    ctx := context.Background()
    ctx = WithRequestID(ctx, "req-abc-123")
    ctx = WithUserID(ctx, 42)
    
    handleRequest(ctx)
    // Output: Request req-abc-123 from user 42
}
```

### 6.3 Que poner y que NO poner en context values

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CONTEXT VALUES — QUE SI Y QUE NO                     │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SI poner en context values:                                            │
│  ├─ Request ID / Trace ID / Span ID (observabilidad)                  │
│  ├─ User ID / Auth claims (autenticacion, post-middleware)            │
│  ├─ Locale / Language (internacionalizacion)                          │
│  ├─ Logger con campos pre-poblados                                     │
│  └─ Informacion de la request que cruza boundaries                    │
│                                                                          │
│  NO poner en context values:                                            │
│  ├─ Database connections (pasar como parametro explicito)              │
│  ├─ Loggers (pasar como parametro o inyeccion de dependencias)        │
│  ├─ Config structs (pasar como parametro o global)                    │
│  ├─ Funciones/closures                                                 │
│  ├─ Mutexes o channels                                                 │
│  ├─ Datos que la funcion NECESITA para operar                         │
│  └─ Cualquier cosa que deberia ser un parametro explicito             │
│                                                                          │
│  REGLA DE ORO:                                                           │
│  "Context values son para datos que CRUZAN API boundaries, no para    │
│  evitar agregar parametros a las funciones."                           │
│                                                                          │
│  Si quitar el value del context hace que la funcion NO COMPILE,       │
│  entonces deberia ser un parametro, no un context value.               │
│  Si quitar el value hace que la funcion funcione igual pero sin       │
│  metadata/tracing, entonces SI pertenece al context.                  │
│                                                                          │
│  TIPO DE KEY:                                                            │
│  ├─ SIEMPRE tipo no exportado (type ctxKey string)                    │
│  ├─ NUNCA string literal (ctx.Value("requestID")) — colisiones       │
│  ├─ NUNCA built-in types (ctx.Value(1)) — colisiones                  │
│  └─ Preferir tipo struct vacio para keys unicas:                      │
│     type requestIDKeyType struct{}                                     │
│     var requestIDKey requestIDKeyType                                  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 6.4 Internals — cadena de busqueda

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    VALUE LOOKUP — CADENA DE PADRES                      │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ctx := context.Background()                                            │
│  ctx = context.WithValue(ctx, "a", 1)   // nodo 1: a=1                │
│  ctx = context.WithCancel(ctx)          // nodo 2: cancel              │
│  ctx = context.WithValue(ctx, "b", 2)   // nodo 3: b=2                │
│  ctx = context.WithTimeout(ctx, 5s)     // nodo 4: timeout            │
│  ctx = context.WithValue(ctx, "a", 99)  // nodo 5: a=99 (shadow!)    │
│                                                                          │
│  ctx.Value("a") busca de abajo hacia arriba:                           │
│  nodo 5: a=99 ← ENCONTRADO (retorna 99, no 1)                        │
│                                                                          │
│  ctx.Value("b") busca:                                                  │
│  nodo 5: no tiene "b"                                                   │
│  nodo 4: no tiene "b" (es timerCtx)                                    │
│  nodo 3: b=2 ← ENCONTRADO                                              │
│                                                                          │
│  ctx.Value("c") busca:                                                  │
│  nodo 5 → 4 → 3 → 2 → 1 → Background → nil (no encontrado)         │
│                                                                          │
│  COMPLEJIDAD: O(N) donde N = profundidad del arbol                    │
│  En la practica N < 10, asi que es irrelevante.                       │
│  Pero NO uses context como un key-value store general.                │
│                                                                          │
│  SHADOWING: un valor con la misma key en un hijo                      │
│  "oculta" el valor del padre. No lo modifica.                          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Arbol de Contexts — Propagacion de Cancelacion

### 7.1 Visualizacion del arbol

```go
package main

import (
    "context"
    "fmt"
    "sync"
    "time"
)

func service(ctx context.Context, name string, wg *sync.WaitGroup) {
    defer wg.Done()
    select {
    case <-ctx.Done():
        fmt.Printf("  %-15s cancelado (%v)\n", name, ctx.Err())
    case <-time.After(10 * time.Second):
        fmt.Printf("  %-15s completado\n", name)
    }
}

func main() {
    // Construir arbol de contexts
    //
    //  Background
    //  └─ root (WithCancel)
    //     ├─ apiCtx (WithTimeout 3s)
    //     │  ├─ dbCtx (WithTimeout 1s)
    //     │  └─ cacheCtx (WithCancel)
    //     └─ workerCtx (WithCancel)
    
    root, rootCancel := context.WithCancel(context.Background())
    defer rootCancel()
    
    apiCtx, apiCancel := context.WithTimeout(root, 3*time.Second)
    defer apiCancel()
    
    dbCtx, dbCancel := context.WithTimeout(apiCtx, 1*time.Second)
    defer dbCancel()
    
    cacheCtx, cacheCancel := context.WithCancel(apiCtx)
    defer cacheCancel()
    
    workerCtx, workerCancel := context.WithCancel(root)
    defer workerCancel()
    
    var wg sync.WaitGroup
    wg.Add(4)
    
    go service(apiCtx, "api-handler", &wg)
    go service(dbCtx, "db-query", &wg)
    go service(cacheCtx, "cache-lookup", &wg)
    go service(workerCtx, "background-job", &wg)
    
    // Esperar a que dbCtx expire (1s)
    time.Sleep(1200 * time.Millisecond)
    fmt.Println("\n--- db timeout (1s) expiro ---")
    
    // Cancelar root — cancela TODO
    time.Sleep(500 * time.Millisecond)
    fmt.Println("\n--- Cancelando root ---")
    rootCancel()
    
    wg.Wait()
    
    // Output:
    //   db-query        cancelado (context deadline exceeded)  ← 1s timeout
    //   --- Cancelando root ---
    //   api-handler     cancelado (context canceled)           ← root cancel
    //   cache-lookup    cancelado (context canceled)           ← root cancel
    //   background-job  cancelado (context canceled)           ← root cancel
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    PROPAGACION DE CANCELACION                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Background                                                              │
│  └─ root (cancel) ────────────────── cancel() → cancela TODO          │
│     ├─ apiCtx (timeout 3s)                                             │
│     │  ├─ dbCtx (timeout 1s) ───── expira a 1s                       │
│     │  └─ cacheCtx (cancel)                                            │
│     └─ workerCtx (cancel)                                               │
│                                                                          │
│  REGLAS:                                                                 │
│  ├─ Cancelar un padre cancela TODOS sus hijos (recursivo)             │
│  ├─ Cancelar un hijo NO cancela al padre                              │
│  ├─ El deadline mas estricto gana (no se puede extender)              │
│  ├─ Un context cancelado permanece cancelado (irreversible)           │
│  └─ Err() cambia de nil a Canceled/DeadlineExceeded una sola vez     │
│                                                                          │
│  IMPLEMENTACION:                                                         │
│  ├─ Cada cancelCtx mantiene un map de hijos                           │
│  ├─ cancel() cierra Done() y llama cancel() en cada hijo             │
│  ├─ Los hijos se registran con el padre al crearse                    │
│  └─ Todo protegido por un mutex interno                               │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 8. Context en la Practica

### 8.1 HTTP server — r.Context()

```go
package main

import (
    "context"
    "fmt"
    "net/http"
    "time"
)

func handler(w http.ResponseWriter, r *http.Request) {
    // r.Context() es el context del request
    // Se cancela automaticamente cuando:
    // - El cliente cierra la conexion
    // - El server's ReadTimeout/WriteTimeout expira
    // - El handler retorna
    ctx := r.Context()
    
    // Agregar nuestro propio timeout
    ctx, cancel := context.WithTimeout(ctx, 2*time.Second)
    defer cancel()
    
    // Simular operacion lenta
    select {
    case <-time.After(3 * time.Second):
        fmt.Fprintln(w, "Completado")
    case <-ctx.Done():
        // El cliente se fue o nuestro timeout expiro
        http.Error(w, "Request cancelled", http.StatusServiceUnavailable)
        fmt.Printf("Request cancelada: %v\n", ctx.Err())
    }
}

func main() {
    http.HandleFunc("/slow", handler)
    fmt.Println("Server en :8080")
    http.ListenAndServe(":8080", nil)
}

// curl http://localhost:8080/slow
// → "Request cancelled" (timeout 2s < operacion 3s)
//
// curl --max-time 1 http://localhost:8080/slow
// → client cierra a 1s → ctx.Done() se activa
```

### 8.2 Database queries con context

```go
package main

import (
    "context"
    "database/sql"
    "fmt"
    "time"
)

func getUserByID(ctx context.Context, db *sql.DB, userID int) (string, error) {
    // Verificar si el context ya expiro antes de hacer la query
    if err := ctx.Err(); err != nil {
        return "", fmt.Errorf("context already done before query: %w", err)
    }
    
    // QueryRowContext propaga el context al driver de la DB
    // Si ctx se cancela, la query se cancela en la DB tambien
    var name string
    err := db.QueryRowContext(ctx,
        "SELECT name FROM users WHERE id = $1", userID,
    ).Scan(&name)
    
    if err != nil {
        return "", fmt.Errorf("query user %d: %w", userID, err)
    }
    return name, nil
}

func handleGetUser(ctx context.Context, db *sql.DB, userID int) {
    // Timeout de 1s para la query
    ctx, cancel := context.WithTimeout(ctx, 1*time.Second)
    defer cancel()
    
    name, err := getUserByID(ctx, db, userID)
    if err != nil {
        fmt.Printf("Error: %v\n", err)
        return
    }
    fmt.Printf("User: %s\n", name)
}
```

### 8.3 HTTP client con context

```go
package main

import (
    "context"
    "fmt"
    "io"
    "net/http"
    "time"
)

func fetchURL(ctx context.Context, url string) (string, error) {
    // Crear request con context
    req, err := http.NewRequestWithContext(ctx, http.MethodGet, url, nil)
    if err != nil {
        return "", err
    }
    
    resp, err := http.DefaultClient.Do(req)
    if err != nil {
        return "", err // incluye context.Canceled o DeadlineExceeded
    }
    defer resp.Body.Close()
    
    body, err := io.ReadAll(resp.Body)
    if err != nil {
        return "", err
    }
    return string(body), nil
}

func main() {
    // Timeout de 2 segundos para el fetch
    ctx, cancel := context.WithTimeout(context.Background(), 2*time.Second)
    defer cancel()
    
    body, err := fetchURL(ctx, "https://httpbin.org/delay/5")
    if err != nil {
        fmt.Printf("Error: %v\n", err) // context deadline exceeded
        return
    }
    fmt.Printf("Body: %.100s...\n", body)
}
```

### 8.4 Signal handling con context (shutdown graceful)

```go
package main

import (
    "context"
    "fmt"
    "os/signal"
    "sync"
    "syscall"
    "time"
)

func worker(ctx context.Context, id int, wg *sync.WaitGroup) {
    defer wg.Done()
    
    ticker := time.NewTicker(500 * time.Millisecond)
    defer ticker.Stop()
    
    for {
        select {
        case <-ticker.C:
            fmt.Printf("  Worker %d: processing\n", id)
        case <-ctx.Done():
            fmt.Printf("  Worker %d: shutting down (%v)\n", id, ctx.Err())
            return
        }
    }
}

func main() {
    // signal.NotifyContext crea un context que se cancela
    // automaticamente cuando llega SIGINT (Ctrl+C) o SIGTERM
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    
    fmt.Println("Server started (Ctrl+C to stop)")
    
    var wg sync.WaitGroup
    for i := 1; i <= 3; i++ {
        wg.Add(1)
        go worker(ctx, i, &wg)
    }
    
    // Esperar a que el context se cancele (signal recibida)
    <-ctx.Done()
    fmt.Println("\nShutdown signal received, waiting for workers...")
    
    wg.Wait()
    fmt.Println("All workers stopped. Goodbye!")
}
```

### 8.5 Middleware chain con context

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "time"
)

type contextKey string

const (
    requestIDKey contextKey = "requestID"
    startTimeKey contextKey = "startTime"
    userKey      contextKey = "user"
)

// Middleware 1: agregar request ID
func withRequestID(ctx context.Context) context.Context {
    id := fmt.Sprintf("req-%06d", rand.Intn(1000000))
    return context.WithValue(ctx, requestIDKey, id)
}

// Middleware 2: agregar timestamp
func withStartTime(ctx context.Context) context.Context {
    return context.WithValue(ctx, startTimeKey, time.Now())
}

// Middleware 3: agregar timeout
func withTimeout(ctx context.Context, d time.Duration) (context.Context, context.CancelFunc) {
    return context.WithTimeout(ctx, d)
}

// Middleware 4: autenticacion (simula)
func withAuth(ctx context.Context, token string) context.Context {
    // Simular validacion de token
    if token == "valid-token" {
        return context.WithValue(ctx, userKey, "admin")
    }
    return ctx // sin user
}

// Handler final
func handleRequest(ctx context.Context) {
    reqID := ctx.Value(requestIDKey).(string)
    startTime := ctx.Value(startTimeKey).(time.Time)
    user, _ := ctx.Value(userKey).(string)
    if user == "" {
        user = "anonymous"
    }
    
    // Simular trabajo
    select {
    case <-time.After(100 * time.Millisecond):
        elapsed := time.Since(startTime)
        fmt.Printf("  [%s] user=%s completed in %v\n", reqID, user, elapsed.Round(time.Millisecond))
    case <-ctx.Done():
        elapsed := time.Since(startTime)
        fmt.Printf("  [%s] user=%s cancelled after %v: %v\n",
            reqID, user, elapsed.Round(time.Millisecond), ctx.Err())
    }
}

func main() {
    fmt.Println("=== Middleware Chain ===")
    
    // Simular 5 requests
    for i := 0; i < 5; i++ {
        ctx := context.Background()
        ctx = withRequestID(ctx)
        ctx = withStartTime(ctx)
        ctx, cancel := withTimeout(ctx, 500*time.Millisecond)
        
        token := "valid-token"
        if i == 3 {
            token = "invalid"
        }
        ctx = withAuth(ctx, token)
        
        handleRequest(ctx)
        cancel()
    }
}
```

---

## 9. AfterFunc (Go 1.21+)

```go
func AfterFunc(ctx Context, f func()) (stop func() bool)
```

`AfterFunc` registra una funcion que se ejecuta en su propia goroutine cuando el context se cancela:

```go
package main

import (
    "context"
    "fmt"
    "sync"
    "time"
)

func main() {
    ctx, cancel := context.WithCancel(context.Background())
    
    var wg sync.WaitGroup
    wg.Add(1)
    
    // Registrar cleanup que se ejecuta cuando ctx se cancela
    stop := context.AfterFunc(ctx, func() {
        defer wg.Done()
        fmt.Println("  [AfterFunc] Cleanup ejecutado!")
        fmt.Println("  [AfterFunc] Cerrando conexiones...")
        time.Sleep(100 * time.Millisecond)
        fmt.Println("  [AfterFunc] Cleanup completo")
    })
    
    fmt.Println("Trabajando...")
    time.Sleep(500 * time.Millisecond)
    
    fmt.Println("Cancelando...")
    cancel()
    
    wg.Wait()
    
    // stop() retorna true si la funcion fue prevenida de ejecutarse
    // Retorna false si ya se ejecuto o ya fue stopped
    _ = stop
    
    // Usar AfterFunc para: cleanup de recursos, logging, metricas,
    // notificacion de cancelacion sin select loop
}
```

---

## 10. Errores y Antipatrones

### 10.1 Catalogo de errores

```go
// ============================================================
// ERROR 1: No pasar context como primer parametro
// ============================================================
func bad1(data string, ctx context.Context) error { // ← ctx deberia ser primero
    return nil
}

func good1(ctx context.Context, data string) error { // ← correcto
    return nil
}
// golint/staticcheck advierten sobre esto

// ============================================================
// ERROR 2: Almacenar context en un struct
// ============================================================
type Bad2 struct {
    ctx context.Context // ← NUNCA almacenar context en struct
    db  *sql.DB
}

// FIX: pasar context como parametro del metodo
type Good2 struct {
    db *sql.DB
}

func (g *Good2) Query(ctx context.Context, query string) error {
    return g.db.QueryRowContext(ctx, query).Err()
}

// EXCEPCION: request-scoped objects como http.Request tienen ctx
// porque representan una sola request y su ciclo de vida

// ============================================================
// ERROR 3: Olvidar defer cancel()
// ============================================================
func bad3() {
    ctx, _ := context.WithTimeout(context.Background(), 5*time.Second)
    // ← _ descarta cancel — goroutine leak del timer interno
    doWork(ctx)
}

func good3() {
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel() // SIEMPRE
    doWork(ctx)
}

// ============================================================
// ERROR 4: Usar context.Background() en lugar de propagar
// ============================================================
func bad4Handler(w http.ResponseWriter, r *http.Request) {
    // IGNORA el context del request — si el cliente cancela,
    // la DB query sigue ejecutandose
    result, err := db.QueryContext(context.Background(), "SELECT ...")
    // ...
}

func good4Handler(w http.ResponseWriter, r *http.Request) {
    // PROPAGA el context del request — si el cliente cancela,
    // la DB query se cancela tambien
    result, err := db.QueryContext(r.Context(), "SELECT ...")
    // ...
}

// ============================================================
// ERROR 5: Usar string keys en WithValue
// ============================================================
func bad5(ctx context.Context) context.Context {
    // String literal como key — cualquier paquete puede colisionar
    return context.WithValue(ctx, "requestID", "abc-123")
}

func good5(ctx context.Context) context.Context {
    // Tipo no exportado — imposible colision
    type requestIDKey struct{}
    return context.WithValue(ctx, requestIDKey{}, "abc-123")
}

// ============================================================
// ERROR 6: Dependencias en context values
// ============================================================
func bad6(ctx context.Context) {
    // La funcion NO FUNCIONA sin el logger en el context
    logger := ctx.Value("logger").(*Logger)
    logger.Info("processing...") // panic si no esta en el context
}

func good6(ctx context.Context, logger *Logger) {
    // El logger es un parametro explicito — claro y compilador-enforced
    logger.Info("processing...")
}

// ============================================================
// ERROR 7: Ignorar ctx.Done() en operaciones largas
// ============================================================
func bad7(ctx context.Context, items []string) {
    for _, item := range items {
        processItem(item) // si ctx se cancela, sigue procesando todo
    }
}

func good7(ctx context.Context, items []string) {
    for _, item := range items {
        select {
        case <-ctx.Done():
            return // respetar cancelacion
        default:
        }
        processItem(item)
    }
}

// ============================================================
// ERROR 8: Cancelar y luego seguir usando el context
// ============================================================
func bad8() {
    ctx, cancel := context.WithCancel(context.Background())
    cancel()
    
    // El context YA esta cancelado — toda operacion con ctx falla
    doWork(ctx) // recibe context canceled inmediatamente
}
```

### 10.2 Tabla de antipatrones

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ANTIPATRONES CON CONTEXT                             │
├──────────────────────────────────────────────────────────────────────────┤
│ Antipatron                      │ Solucion                              │
├─────────────────────────────────┼───────────────────────────────────────┤
│ ctx no es primer parametro      │ func f(ctx context.Context, ...)     │
│ ctx almacenado en struct        │ Pasar como parametro del metodo      │
│ Olvidar defer cancel()          │ SIEMPRE defer cancel()               │
│ Background() en lugar de       │ Propagar ctx del caller               │
│ propagar                        │                                       │
│ String keys en WithValue        │ Tipos no exportados como keys        │
│ Dependencias en values          │ Parametros explicitos                 │
│ Ignorar ctx.Done()             │ select { case <-ctx.Done(): }         │
│ Usar ctx despues de cancel()    │ No cancelar prematuramente           │
│ nil context                     │ Usar context.TODO() si no tienes ctx │
│ Context value como config       │ Pasar config como parametro          │
└─────────────────────────────────┴───────────────────────────────────────┘
```

---

## 11. Comparacion con C y Rust

### 11.1 C — no hay equivalente

```c
// C no tiene context. Los patrones equivalentes son:

// 1. CANCELACION: variable atomica compartida
#include <stdatomic.h>
#include <pthread.h>

atomic_int cancelled = 0;

void *worker(void *arg) {
    while (!atomic_load(&cancelled)) {
        do_work();
    }
    return NULL;
}

void cancel_all() {
    atomic_store(&cancelled, 1);
}

// LIMITACIONES:
// - No hay arbol de cancelacion (padre→hijos)
// - No hay timeout integrado (necesitas timer thread + pthread_cancel)
// - No hay propagacion de metadata
// - No hay forma estandar — cada libreria inventa su propio mecanismo

// 2. TIMEOUT: alarm() o timer_create()
#include <signal.h>

volatile sig_atomic_t timed_out = 0;

void alarm_handler(int sig) {
    timed_out = 1;
}

void with_timeout(int seconds) {
    signal(SIGALRM, alarm_handler);
    alarm(seconds);
    
    while (!timed_out) {
        do_work();
    }
    
    alarm(0); // cancelar timer
}

// LIMITACIONES:
// - alarm() es global (un solo timer por proceso)
// - Señales son per-process, no per-thread
// - pthread_cancel es peligroso (puede dejar estado inconsistente)
// - No hay forma de "heredar" timeout en sub-operaciones
```

### 11.2 Rust — no hay equivalente directo en std

```rust
// Rust no tiene context en std, pero tiene alternativas:

// 1. tokio::CancellationToken (async, mas cercano a context)
use tokio_util::sync::CancellationToken;

async fn worker(token: CancellationToken) {
    loop {
        tokio::select! {
            _ = token.cancelled() => {
                println!("cancelled!");
                return;
            }
            _ = do_work() => {}
        }
    }
}

async fn main_example() {
    let token = CancellationToken::new();
    let child_token = token.child_token(); // ← arbol como Go context
    
    tokio::spawn(worker(child_token));
    
    tokio::time::sleep(Duration::from_secs(1)).await;
    token.cancel(); // cancela parent + child
}

// 2. tokio::time::timeout (equivalente a WithTimeout)
use tokio::time::timeout;

async fn with_timeout_example() {
    match timeout(Duration::from_secs(2), slow_operation()).await {
        Ok(result) => println!("Result: {:?}", result),
        Err(_) => println!("Timed out!"),
    }
}

// DIFERENCIAS CON GO CONTEXT:
// - Rust separa cancelacion (CancellationToken) y timeout (timeout())
//   Go lo unifica todo en Context
// - Rust no tiene WithValue equivalente (usa parametros explicitos)
//   Esto es intencional: Rust prefiere tipos explicitos a any
// - CancellationToken es una crate (tokio-util), no stdlib
// - En sync Rust, no hay equivalente estandar
//   (usarias Arc<AtomicBool> manualmente, como C)
// - Rust no tiene la convencion "ctx como primer parametro"
//   En su lugar, el token se pasa o se almacena en un struct

// 3. Para metadata/tracing, Rust usa tracing crate con Span
use tracing::{info, instrument, span, Level};

#[instrument(fields(request_id = %request_id))]
async fn handle_request(request_id: String) {
    info!("processing request");
    // El span carry metadata automaticamente, similar a context values
    // pero con type safety y compile-time guarantees
}
```

### 11.3 Tabla comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CONTEXT: GO vs C vs RUST                             │
├──────────────────┬─────────────────┬───────────────┬─────────────────────┤
│ Aspecto           │ C               │ Go            │ Rust               │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Cancelacion       │ atomic flag     │ ctx.Done()    │ CancellationToken  │
│                   │ (manual)        │ (built-in)    │ (tokio-util crate) │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Timeout           │ alarm()/timer   │ WithTimeout   │ tokio::timeout     │
│                   │ (global/manual) │ (composable)  │ (composable)       │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Arbol             │ No (manual)     │ Si (built-in) │ child_token()     │
│                   │                 │               │ (tokio-util)       │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Values/Metadata   │ Thread-local    │ WithValue     │ No (parametros    │
│                   │ storage (TLS)   │ (any typed)   │ explicitos)        │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Convencion        │ Ninguna         │ 1er parametro │ Almacenar en      │
│                   │ estandar        │ (ctx)         │ struct o parametro │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Stdlib            │ No              │ Si            │ No (crates)        │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Sync + Async      │ Solo sync       │ Solo sync     │ Separados          │
│                   │                 │ (pero funciona│ (async: tokio,     │
│                   │                 │ con goroutines│ sync: Arc<AtomicBool│
│                   │                 │ que son async)│ manual)            │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Type safety       │ Ninguna (void*) │ Runtime (any) │ Compile-time      │
│                   │                 │               │ (generics/traits)  │
└──────────────────┴─────────────────┴───────────────┴─────────────────────┘
```

---

## 12. Programa Completo — Infrastructure Deployment Coordinator

Un coordinador de deployment que demuestra todos los aspectos de context: cancelacion, timeouts, deadline inheritance, values, signal handling, y propagacion en un arbol de operaciones:

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "os/signal"
    "strings"
    "sync"
    "syscall"
    "time"
)

// ============================================================
// Context value keys y helpers
// ============================================================

type ctxKey int

const (
    deployIDKey ctxKey = iota
    environmentKey
    operatorKey
)

func WithDeployID(ctx context.Context, id string) context.Context {
    return context.WithValue(ctx, deployIDKey, id)
}

func DeployID(ctx context.Context) string {
    if id, ok := ctx.Value(deployIDKey).(string); ok {
        return id
    }
    return "unknown"
}

func WithEnvironment(ctx context.Context, env string) context.Context {
    return context.WithValue(ctx, environmentKey, env)
}

func Environment(ctx context.Context) string {
    if env, ok := ctx.Value(environmentKey).(string); ok {
        return env
    }
    return "unknown"
}

func WithOperator(ctx context.Context, op string) context.Context {
    return context.WithValue(ctx, operatorKey, op)
}

func Operator(ctx context.Context) string {
    if op, ok := ctx.Value(operatorKey).(string); ok {
        return op
    }
    return "system"
}

// ============================================================
// Logging con context
// ============================================================

func logCtx(ctx context.Context, format string, args ...any) {
    prefix := fmt.Sprintf("[%s][%s]", DeployID(ctx), Environment(ctx))
    msg := fmt.Sprintf(format, args...)
    fmt.Printf("  %s %s\n", prefix, msg)
}

// ============================================================
// Deployment phases (cada una respeta context)
// ============================================================

type PhaseResult struct {
    Phase    string
    Service  string
    Success  bool
    Duration time.Duration
    Error    string
}

// Phase 1: Validate
func validateService(ctx context.Context, service string) PhaseResult {
    start := time.Now()
    
    // Verificar deadline
    if deadline, ok := ctx.Deadline(); ok {
        remaining := time.Until(deadline)
        logCtx(ctx, "Validate %s (%.0fs remaining)", service, remaining.Seconds())
    }
    
    select {
    case <-time.After(time.Duration(50+rand.Intn(100)) * time.Millisecond):
        logCtx(ctx, "Validate %s: OK", service)
        return PhaseResult{Phase: "validate", Service: service, Success: true, Duration: time.Since(start)}
    case <-ctx.Done():
        return PhaseResult{Phase: "validate", Service: service, Success: false,
            Duration: time.Since(start), Error: ctx.Err().Error()}
    }
}

// Phase 2: Pull image (con sub-timeout)
func pullImage(ctx context.Context, service string) PhaseResult {
    start := time.Now()
    
    // Sub-timeout: pull no debe tardar mas de 3s
    pullCtx, cancel := context.WithTimeout(ctx, 3*time.Second)
    defer cancel()
    
    logCtx(ctx, "Pull image for %s...", service)
    
    pullTime := time.Duration(200+rand.Intn(800)) * time.Millisecond
    // 10% chance de ser MUY lento (simular timeout)
    if rand.Intn(10) == 0 {
        pullTime = 5 * time.Second
    }
    
    select {
    case <-time.After(pullTime):
        logCtx(ctx, "Pull %s: complete", service)
        return PhaseResult{Phase: "pull", Service: service, Success: true, Duration: time.Since(start)}
    case <-pullCtx.Done():
        errMsg := pullCtx.Err().Error()
        if pullCtx.Err() == context.DeadlineExceeded && ctx.Err() == nil {
            errMsg = "pull timeout (3s limit)"
        }
        logCtx(ctx, "Pull %s: FAILED (%s)", service, errMsg)
        return PhaseResult{Phase: "pull", Service: service, Success: false,
            Duration: time.Since(start), Error: errMsg}
    }
}

// Phase 3: Deploy (con health check sub-operation)
func deployService(ctx context.Context, service string) PhaseResult {
    start := time.Now()
    logCtx(ctx, "Deploy %s...", service)
    
    // Simular deploy
    deployTime := time.Duration(300+rand.Intn(500)) * time.Millisecond
    select {
    case <-time.After(deployTime):
    case <-ctx.Done():
        return PhaseResult{Phase: "deploy", Service: service, Success: false,
            Duration: time.Since(start), Error: ctx.Err().Error()}
    }
    
    // Health check sub-operation con su propio timeout
    healthCtx, healthCancel := context.WithTimeout(ctx, 2*time.Second)
    defer healthCancel()
    
    logCtx(ctx, "Health check %s...", service)
    
    healthTime := time.Duration(100+rand.Intn(400)) * time.Millisecond
    select {
    case <-time.After(healthTime):
        // 5% chance de fallo en health check
        if rand.Intn(20) == 0 {
            logCtx(ctx, "Health check %s: UNHEALTHY", service)
            return PhaseResult{Phase: "deploy", Service: service, Success: false,
                Duration: time.Since(start), Error: "health check failed"}
        }
        logCtx(ctx, "Deploy %s: HEALTHY", service)
        return PhaseResult{Phase: "deploy", Service: service, Success: true, Duration: time.Since(start)}
    case <-healthCtx.Done():
        return PhaseResult{Phase: "deploy", Service: service, Success: false,
            Duration: time.Since(start), Error: "health check timeout"}
    }
}

// ============================================================
// Coordinator — orquesta el deployment completo
// ============================================================

func coordinateDeploy(ctx context.Context, services []string) []PhaseResult {
    var allResults []PhaseResult
    var mu sync.Mutex
    
    addResult := func(r PhaseResult) {
        mu.Lock()
        allResults = append(allResults, r)
        mu.Unlock()
    }
    
    // Phase 1: Validate all (paralelo)
    logCtx(ctx, "=== Phase 1: Validation ===")
    var wg sync.WaitGroup
    validated := make([]string, 0, len(services))
    var validMu sync.Mutex
    
    for _, svc := range services {
        wg.Add(1)
        go func(s string) {
            defer wg.Done()
            result := validateService(ctx, s)
            addResult(result)
            if result.Success {
                validMu.Lock()
                validated = append(validated, s)
                validMu.Unlock()
            }
        }(svc)
    }
    wg.Wait()
    
    if ctx.Err() != nil {
        return allResults
    }
    
    logCtx(ctx, "%d/%d services validated\n", len(validated), len(services))
    
    // Phase 2: Pull images (paralelo, max 3 concurrent)
    logCtx(ctx, "=== Phase 2: Pull Images ===")
    sem := make(chan struct{}, 3) // semaforo
    pulled := make([]string, 0, len(validated))
    var pullMu sync.Mutex
    
    for _, svc := range validated {
        wg.Add(1)
        go func(s string) {
            defer wg.Done()
            select {
            case sem <- struct{}{}:
                defer func() { <-sem }()
            case <-ctx.Done():
                return
            }
            result := pullImage(ctx, s)
            addResult(result)
            if result.Success {
                pullMu.Lock()
                pulled = append(pulled, s)
                pullMu.Unlock()
            }
        }(svc)
    }
    wg.Wait()
    
    if ctx.Err() != nil {
        return allResults
    }
    
    logCtx(ctx, "%d/%d images pulled\n", len(pulled), len(validated))
    
    // Phase 3: Deploy (secuencial — rolling update)
    logCtx(ctx, "=== Phase 3: Deploy (rolling) ===")
    for _, svc := range pulled {
        if ctx.Err() != nil {
            break
        }
        result := deployService(ctx, svc)
        addResult(result)
        
        if !result.Success {
            logCtx(ctx, "STOPPING: %s failed, not deploying remaining services", svc)
            break
        }
    }
    
    return allResults
}

// ============================================================
// Main
// ============================================================

func main() {
    // Context raiz: signal handling para Ctrl+C
    signalCtx, signalStop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer signalStop()
    
    // Deployment timeout: 15 segundos total
    ctx, cancel := context.WithTimeout(signalCtx, 15*time.Second)
    defer cancel()
    
    // Agregar metadata al context
    ctx = WithDeployID(ctx, fmt.Sprintf("deploy-%04d", rand.Intn(10000)))
    ctx = WithEnvironment(ctx, "production")
    ctx = WithOperator(ctx, "devops-bot")

    services := []string{
        "api-gateway",
        "auth-service",
        "user-service",
        "order-service",
        "payment-service",
        "notification-service",
    }

    fmt.Println("╔══════════════════════════════════════════════════════════════╗")
    fmt.Println("║           INFRASTRUCTURE DEPLOYMENT COORDINATOR             ║")
    fmt.Println("╠══════════════════════════════════════════════════════════════╣")
    fmt.Printf("║  Deploy ID: %-20s Environment: %-12s ║\n",
        DeployID(ctx), Environment(ctx))
    fmt.Printf("║  Operator:  %-20s Services:    %-12d ║\n",
        Operator(ctx), len(services))
    fmt.Printf("║  Timeout:   15s (Ctrl+C to abort)                           ║\n")
    fmt.Println("╚══════════════════════════════════════════════════════════════╝")
    fmt.Println()

    //  Context tree:
    //
    //  Background
    //  └─ signalCtx (Ctrl+C cancels)
    //     └─ ctx (timeout 15s + values: deployID, env, operator)
    //        ├─ validate workers (inherit ctx)
    //        ├─ pull workers (inherit ctx)
    //        │  └─ pullCtx (timeout 3s per pull)
    //        └─ deploy (inherit ctx)
    //           └─ healthCtx (timeout 2s per health check)

    start := time.Now()
    results := coordinateDeploy(ctx, services)
    elapsed := time.Since(start)

    // Summary
    fmt.Println()
    fmt.Println("┌──────────────────────────────────────────────────────────┐")
    fmt.Println("│                  DEPLOYMENT SUMMARY                      │")
    fmt.Println("├──────────────────────────────────────────────────────────┤")
    
    succeeded := 0
    failed := 0
    for _, r := range results {
        status := "OK"
        detail := ""
        if !r.Success {
            status = "FAIL"
            detail = fmt.Sprintf(" (%s)", r.Error)
            failed++
        } else {
            succeeded++
        }
        fmt.Printf("│  [%-4s] %-8s %-20s %6v%s\n",
            status, r.Phase, r.Service,
            r.Duration.Round(time.Millisecond),
            detail)
    }
    
    fmt.Println("├──────────────────────────────────────────────────────────┤")
    fmt.Printf("│  Total: %d | OK: %d | Failed: %d | Time: %v\n",
        len(results), succeeded, failed, elapsed.Round(time.Millisecond))
    
    if ctx.Err() != nil {
        fmt.Printf("│  Context: %v\n", ctx.Err())
    }
    
    fmt.Println("├──────────────────────────────────────────────────────────┤")
    fmt.Println("│  Context tree used:                                      │")
    fmt.Println("│    Background → signal → timeout(15s) + values          │")
    fmt.Println("│      ├─ validate workers (inherit parent deadline)      │")
    fmt.Println("│      ├─ pull workers + sub-timeout(3s per pull)         │")
    fmt.Println("│      └─ deploy + sub-timeout(2s per health check)      │")
    fmt.Println("└──────────────────────────────────────────────────────────┘")
    
    _ = strings.Join // silenciar import
}
```

---

## 13. Ejercicios

### Ejercicio 1: Cascading timeout API

Implementa un API gateway que llama a 3 microservicios con cascading timeouts:

```
Gateway (timeout: 5s)
├─ AuthService (timeout: 1s, hereda 5s del gateway)
├─ UserService (timeout: 2s, hereda del gateway)
│  └─ DBQuery (timeout: 500ms, hereda del service)
└─ OrderService (timeout: 3s, hereda del gateway)
```

- Cada servicio simula latencia variable
- Si AuthService falla o timeout, cancelar todo (auth es critico)
- Si UserService o OrderService fallan, continuar con los demas
- Propagar request ID y user ID via context values
- Imprimir cuanto deadline restante tenia cada operacion al iniciar

### Ejercicio 2: Graceful shutdown orchestrator

Implementa un orchestrator que maneja shutdown graceful de multiples componentes:

```go
type Component interface {
    Start(ctx context.Context) error
    Shutdown(ctx context.Context) error  // ctx con timeout para shutdown
}
```

- signal.NotifyContext para capturar Ctrl+C
- Fase 1: stop accepting new work (context cancel)
- Fase 2: drain in-progress work (timeout 10s)
- Fase 3: force shutdown (context deadline)
- Implementar 4 componentes: HTTPServer, Worker, Cache, MetricsExporter
- Cada componente tiene su propio sub-timeout para shutdown
- AfterFunc para logging de cleanup

### Ejercicio 3: Context-aware retry with backoff

Implementa un retry helper que respeta context:

```go
func Retry(ctx context.Context, maxAttempts int, fn func(ctx context.Context) error) error
```

- Cada retry verifica ctx.Done() antes de intentar
- Backoff exponencial: 100ms, 200ms, 400ms, 800ms...
- Si el deadline del context es < proximo backoff, no reintentar (fail fast)
- Wrappear cada intento con sub-timeout (mitad del tiempo restante)
- Loggear cada intento con context values (request ID)

### Ejercicio 4: Distributed tracing con context

Implementa un mini-tracing system usando context values:

```go
type Span struct {
    TraceID  string
    SpanID   string
    ParentID string
    Name     string
    Start    time.Time
    End      time.Time
}

func StartSpan(ctx context.Context, name string) (context.Context, func())
func SpanFromContext(ctx context.Context) *Span
```

- Cada StartSpan crea un nuevo span hijo del span actual en el context
- El TraceID se hereda del padre (o se genera si es root)
- La funcion retornada termina el span y lo registra
- Implementar un SpanCollector que recoge todos los spans
- Demostrar con un arbol de 3 niveles (handler → service → query)

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CONTEXT — RESUMEN COMPLETO                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  INTERFACE:                                                              │
│  ├─ Deadline() (time.Time, bool)  — ¿hay deadline?                    │
│  ├─ Done() <-chan struct{}        — channel de cancelacion             │
│  ├─ Err() error                   — nil / Canceled / DeadlineExceeded  │
│  └─ Value(key) any               — metadata request-scoped            │
│                                                                          │
│  CONSTRUCTORES:                                                          │
│  ├─ context.Background()         — raiz de toda cadena                │
│  ├─ context.TODO()               — marcador temporal                  │
│  ├─ WithCancel(parent)           — cancelacion manual                  │
│  ├─ WithCancelCause(parent)      — cancelacion con razon (Go 1.20+)   │
│  ├─ WithTimeout(parent, d)       — auto-cancel despues de d           │
│  ├─ WithDeadline(parent, t)      — auto-cancel en tiempo t            │
│  ├─ WithValue(parent, k, v)      — agregar metadata                   │
│  └─ AfterFunc(ctx, f)            — ejecutar f al cancelarse (Go 1.21+)│
│                                                                          │
│  REGLAS:                                                                 │
│  ├─ SIEMPRE primer parametro: func f(ctx context.Context, ...)        │
│  ├─ SIEMPRE defer cancel()                                             │
│  ├─ NUNCA almacenar en struct (excepto request objects)               │
│  ├─ NUNCA nil context — usar TODO() si no tienes uno                  │
│  ├─ PROPAGAR, no crear Background() en medio de la cadena            │
│  ├─ Keys de Value: tipos no exportados                                 │
│  └─ Values: solo metadata, NUNCA dependencias                         │
│                                                                          │
│  PROPAGACION:                                                            │
│  ├─ Cancelar padre → cancela TODOS los hijos (recursivo)              │
│  ├─ Cancelar hijo → NO cancela padre ni hermanos                      │
│  ├─ Deadline mas estricto gana (no se puede extender)                 │
│  └─ Values se buscan de hijo a padre (O(N), N = profundidad)          │
│                                                                          │
│  USOS PRINCIPALES:                                                       │
│  ├─ HTTP: r.Context() para todo handler                                │
│  ├─ Database: db.QueryContext(ctx, ...)                               │
│  ├─ HTTP client: http.NewRequestWithContext(ctx, ...)                  │
│  ├─ Signals: signal.NotifyContext para Ctrl+C                          │
│  ├─ gRPC: context propagado automaticamente entre servicios           │
│  └─ Goroutines: select { case <-ctx.Done(): }                         │
│                                                                          │
│  vs C: no hay equivalente (atomic flags, alarm(), pthread_cancel)     │
│  vs Rust: CancellationToken (tokio-util), timeout() func, no Values  │
│  Go: stdlib, primer parametro por convencion, arbol de cancelacion    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S03 - Sincronizacion, T04 - Comparacion con Rust — goroutines vs tokio tasks, channels vs mpsc, mutex vs Mutex<T>, share-memory vs message-passing
