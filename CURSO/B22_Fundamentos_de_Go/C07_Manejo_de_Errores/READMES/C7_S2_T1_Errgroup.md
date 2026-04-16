# Errgroup — golang.org/x/sync/errgroup, Goroutines con Propagacion de Errores

## 1. Introduccion

Cuando tienes un proceso que necesita ejecutar **varias tareas en paralelo** (descargar archivos, hacer health checks a multiples servidores, consultar APIs), Go ofrece goroutines y `sync.WaitGroup`. Pero `WaitGroup` tiene un problema fundamental: **no maneja errores**. Si 3 de 5 goroutines fallan, `WaitGroup.Wait()` solo sabe que terminaron — no sabe que fallaron, no propaga los errores, y el codigo que llama a `Wait()` no tiene forma de reaccionar.

`errgroup` (paquete `golang.org/x/sync/errgroup`) resuelve exactamente esto: **coordina goroutines que pueden fallar** y propaga el primer error al caller. Es la respuesta idiomatica de Go a "necesito paralelismo con error handling".

Para SysAdmin/DevOps, `errgroup` es la herramienta correcta para:
- Health checks paralelos a N servidores
- Desplegar a multiples clusters simultaneamente
- Validar configuracion desde varias fuentes
- Descargar/subir archivos en paralelo con error handling
- Pipeline de datos con multiples etapas concurrentes

```
┌──────────────────────────────────────────────────────────────────────────┐
│                     EL PROBLEMA QUE RESUELVE ERRGROUP                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  CON sync.WaitGroup:                CON errgroup.Group:                  │
│  ─────────────────                  ──────────────────                   │
│                                                                          │
│  var wg sync.WaitGroup              g, ctx := errgroup.WithContext(ctx)  │
│  var mu sync.Mutex                                                       │
│  var firstErr error                 g.Go(func() error {                  │
│                                         return checkServer(ctx, s1)     │
│  wg.Add(3)                          })                                   │
│  go func() {                        g.Go(func() error {                  │
│      defer wg.Done()                    return checkServer(ctx, s2)     │
│      if err := check(s1); err != nil{})                                  │
│          mu.Lock()                  g.Go(func() error {                  │
│          if firstErr == nil {           return checkServer(ctx, s3)     │
│              firstErr = err         })                                   │
│          }                                                               │
│          mu.Unlock()                if err := g.Wait(); err != nil {     │
│      }                                  // primer error                  │
│  }()                                }                                    │
│  // ... repetir para s2, s3                                              │
│  wg.Wait()                                                               │
│  // ¿firstErr? ¿y los otros?        RESULTADO:                          │
│                                      - Codigo limpio                    │
│  PROBLEMAS:                          - Primer error propagado           │
│  - Boilerplate (mutex, var error)    - Contexto cancelable              │
│  - No cancela las otras goroutines   - Goroutines restantes canceladas  │
│  - El caller no sabe cuantas        - Limitacion de concurrencia       │
│    fallaron ni cuales                                                    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Anatomia de errgroup.Group

### 2.1 API Completa

`errgroup` tiene una API intencionalmente minimalista — solo 4 metodos:

```go
import "golang.org/x/sync/errgroup"

// Crear un Group SIN contexto
var g errgroup.Group

// Crear un Group CON contexto derivado
// El contexto se cancela cuando la primera goroutine devuelve error
// o cuando TODAS las goroutines completan exitosamente
g, ctx := errgroup.WithContext(parentCtx)

// Lanzar una goroutine gestionada por el grupo
g.Go(func() error {
    // ... trabajo que puede fallar
    return nil // o return err
})

// Esperar a que TODAS las goroutines completen
// Devuelve el PRIMER error no-nil (o nil si todas exitosas)
err := g.Wait()

// Limitar el numero de goroutines concurrentes (Go 1.20+)
g.SetLimit(10) // maximo 10 goroutines a la vez
```

### 2.2 Estructura Interna

```go
// Simplificacion de la implementacion real:
type Group struct {
    cancel  func(error)      // funcion para cancelar el contexto derivado
    wg      sync.WaitGroup   // coordina la espera de goroutines
    sem     chan token        // semaforo para limitar concurrencia (SetLimit)
    errOnce sync.Once        // garantiza que solo se almacena el PRIMER error
    err     error            // el primer error devuelto por una goroutine
}

type token struct{} // tipo vacio para el semaforo
```

Los puntos clave de la implementacion:
- `sync.Once` para capturar **solo el primer error** (thread-safe)
- `sync.WaitGroup` interno para esperar a todas las goroutines
- Semaforo via channel con buffer para `SetLimit`
- `cancel` invocado en el primer error para cancelar el contexto derivado

### 2.3 Flujo de Ejecucion Interno

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    FLUJO INTERNO DE errgroup.Go                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  g.Go(f) llamado                                                         │
│  │                                                                       │
│  ├─ ¿SetLimit activo?                                                    │
│  │   ├─ SI: sem <- token{}  ← BLOQUEA si hay N goroutines activas       │
│  │   └─ NO: continua                                                     │
│  │                                                                       │
│  ├─ g.wg.Add(1)                                                         │
│  │                                                                       │
│  └─ go func() {            ← nueva goroutine                            │
│       defer g.wg.Done()                                                  │
│       defer func() {                                                     │
│           if sem != nil { <-sem }  ← liberar slot del semaforo           │
│       }()                                                                │
│                                                                          │
│       err := f()           ← ejecutar la funcion del usuario             │
│       │                                                                  │
│       └─ if err != nil:                                                  │
│           g.errOnce.Do(func() {                                          │
│               g.err = err          ← almacenar PRIMER error              │
│               if g.cancel != nil {                                       │
│                   g.cancel(err)    ← cancelar el contexto                │
│               }                                                          │
│           })                                                             │
│     }()                                                                  │
│                                                                          │
│  g.Wait() llamado                                                        │
│  │                                                                       │
│  ├─ g.wg.Wait()           ← bloquea hasta que TODAS terminen            │
│  └─ return g.err           ← nil o el primer error                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

**Detalle critico**: `g.Wait()` espera a **todas** las goroutines, no solo hasta el primer error. Incluso si una goroutine falla inmediatamente, `Wait()` no retorna hasta que las demas terminen (o sean canceladas via contexto y reaccionen al cancel).

---

## 3. Patron Basico: Tareas Independientes en Paralelo

### 3.1 Ejemplo Fundamental

```go
package main

import (
    "fmt"
    "net/http"
    "time"

    "golang.org/x/sync/errgroup"
)

func checkEndpoint(url string) error {
    client := &http.Client{Timeout: 5 * time.Second}
    resp, err := client.Get(url)
    if err != nil {
        return fmt.Errorf("check %s: %w", url, err)
    }
    defer resp.Body.Close()

    if resp.StatusCode != http.StatusOK {
        return fmt.Errorf("check %s: status %d", url, resp.StatusCode)
    }
    return nil
}

func healthCheckAll(endpoints []string) error {
    var g errgroup.Group

    for _, endpoint := range endpoints {
        url := endpoint // captura para el closure (Go < 1.22)
        g.Go(func() error {
            return checkEndpoint(url)
        })
    }

    return g.Wait() // primer error o nil
}

func main() {
    endpoints := []string{
        "https://api.example.com/health",
        "https://db.example.com/health",
        "https://cache.example.com/health",
    }

    if err := healthCheckAll(endpoints); err != nil {
        fmt.Printf("Health check failed: %v\n", err)
        // En produccion: alertar, intentar failover, etc.
    } else {
        fmt.Println("All endpoints healthy")
    }
}
```

### 3.2 Captura de Variable en el Loop — El Bug Clasico

```go
// ⚠️ BUG en Go < 1.22: todas las goroutines ven el ULTIMO valor
for _, endpoint := range endpoints {
    g.Go(func() error {
        return checkEndpoint(endpoint) // ← captura la variable del loop
    })
}
// Si endpoints = ["a", "b", "c"], las 3 goroutines checkean "c"

// ✅ SOLUCION 1: variable local (idiomatico pre-1.22)
for _, endpoint := range endpoints {
    endpoint := endpoint // nueva variable en cada iteracion
    g.Go(func() error {
        return checkEndpoint(endpoint)
    })
}

// ✅ SOLUCION 2: argumento al closure (alternativa)
for _, endpoint := range endpoints {
    g.Go(func() error {
        return checkEndpoint(endpoint)
    })
    // Nota: en Go 1.22+ el loop ya crea nueva variable por iteracion
    // por lo que el bug original ya no existe
}

// ✅ SOLUCION 3: Go 1.22+ — no necesitas nada, el loop ya captura correctamente
// Pero el patron endpoint := endpoint sigue siendo inofensivo y explicito
```

### 3.3 Go 1.22+ Loop Variable Fix

A partir de Go 1.22, la semantica del loop `for range` cambio: cada iteracion crea una **nueva variable**, eliminando el bug clasico. Pero si tu codigo debe compilar con versiones anteriores, usa siempre la captura explicita:

```go
// go.mod
module myproject
go 1.22 // con 1.22+, el loop fix esta activo

// Si go.mod dice go 1.21 o anterior: DEBES capturar explicitamente
```

---

## 4. errgroup.WithContext — Cancelacion Cooperativa

### 4.1 El Patron Fundamental

`WithContext` es la variante mas poderosa: cuando una goroutine falla, el contexto derivado se **cancela automaticamente**, senalizando a las demas que deben detenerse:

```go
package main

import (
    "context"
    "fmt"
    "io"
    "net/http"
    "time"

    "golang.org/x/sync/errgroup"
)

func fetchData(ctx context.Context, url string) ([]byte, error) {
    req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
    if err != nil {
        return nil, fmt.Errorf("create request %s: %w", url, err)
    }

    client := &http.Client{Timeout: 30 * time.Second}
    resp, err := client.Do(req)
    if err != nil {
        return nil, fmt.Errorf("fetch %s: %w", url, err)
    }
    defer resp.Body.Close()

    body, err := io.ReadAll(resp.Body)
    if err != nil {
        return nil, fmt.Errorf("read %s: %w", url, err)
    }
    return body, nil
}

func fetchAllConfigs(ctx context.Context, urls []string) (map[string][]byte, error) {
    g, ctx := errgroup.WithContext(ctx) // ctx DERIVADO — se cancela en el primer error

    results := make(map[string][]byte)
    mu := sync.Mutex{} // proteger el mapa (acceso concurrente)

    for _, url := range urls {
        url := url
        g.Go(func() error {
            data, err := fetchData(ctx, url) // usa el contexto DERIVADO
            if err != nil {
                return err // este error cancela ctx → las demas goroutines reciben cancel
            }
            mu.Lock()
            results[url] = data
            mu.Unlock()
            return nil
        })
    }

    if err := g.Wait(); err != nil {
        return nil, fmt.Errorf("fetch configs: %w", err)
    }
    return results, nil
}
```

### 4.2 Flujo de Cancelacion

```
┌──────────────────────────────────────────────────────────────────────────┐
│                  CANCELACION CON WithContext                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  g, ctx := errgroup.WithContext(parentCtx)                               │
│                                                                          │
│  goroutine 1: fetchData(ctx, url1) ──── trabajando... ──── return nil   │
│                                                                          │
│  goroutine 2: fetchData(ctx, url2) ──── return ERROR ──┐                │
│                                                         │                │
│  goroutine 3: fetchData(ctx, url3) ──── trabaj...       │                │
│                                         │                │               │
│                          ctx.Done() ←───┘ cancel()  ←───┘               │
│                                         │                                │
│                          goroutine 3 ve ctx.Err() → aborta               │
│                                                                          │
│  g.Wait() retorna: error de goroutine 2 (primer error)                  │
│                                                                          │
│  IMPORTANTE: goroutine 3 debe VERIFICAR ctx en su logica:                │
│                                                                          │
│  func fetchData(ctx context.Context, url string) error {                │
│      select {                                                            │
│      case <-ctx.Done():                                                  │
│          return ctx.Err()  // ← reaccionar a cancelacion                │
│      default:                                                            │
│      }                                                                   │
│      // ... continuar trabajo                                            │
│  }                                                                       │
│                                                                          │
│  Si la goroutine NO verifica ctx, seguira ejecutando hasta terminar.     │
│  La cancelacion es COOPERATIVA — no mata goroutines.                     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.3 Error Comun: Reusar el Nombre ctx

```go
// ⚠️ BUG SUTIL: el ctx original se pierde
func process(ctx context.Context) error {
    g, ctx := errgroup.WithContext(ctx) // ctx ahora es el DERIVADO

    g.Go(func() error {
        // Este ctx se cancelara cuando el grupo falle
        return doWork(ctx)
    })

    if err := g.Wait(); err != nil {
        // ⚠️ AQUI ctx YA ESTA CANCELADO (el grupo termino con error)
        // Si intentas usar ctx para logging/cleanup, fallara
        log.Info(ctx, "group failed") // ctx cancelado → puede fallar
        return err
    }
    return nil
}

// ✅ SOLUCION: usar nombres diferentes
func process(ctx context.Context) error {
    g, gctx := errgroup.WithContext(ctx) // gctx para el grupo

    g.Go(func() error {
        return doWork(gctx) // goroutines usan gctx
    })

    if err := g.Wait(); err != nil {
        log.Info(ctx, "group failed") // ctx original sigue vivo
        return err
    }
    return nil
}
```

### 4.4 Cancelacion desde el Padre

El contexto derivado tambien se cancela si el **padre** se cancela:

```go
func processWithTimeout(servers []string) error {
    // Timeout global de 30 segundos
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()

    g, gctx := errgroup.WithContext(ctx)

    for _, server := range servers {
        server := server
        g.Go(func() error {
            return checkServer(gctx, server)
        })
    }

    err := g.Wait()
    if err != nil {
        if errors.Is(err, context.DeadlineExceeded) {
            return fmt.Errorf("health checks timed out after 30s: %w", err)
        }
        return fmt.Errorf("health check failed: %w", err)
    }
    return nil
}
```

---

## 5. SetLimit — Control de Concurrencia

### 5.1 El Problema de la Concurrencia Ilimitada

Sin limite, `errgroup.Go` lanza una goroutine **por cada llamada**. Con 10,000 URLs, lanzas 10,000 goroutines simultaneas — abres 10,000 conexiones, agotás descriptores de archivo, saturas el servidor destino, y el OOM killer de Linux te visita:

```go
// ⚠️ PELIGROSO con muchos elementos
for _, url := range tenThousandURLs {
    url := url
    g.Go(func() error {
        return fetch(ctx, url) // 10,000 goroutines simultaneas
    })
}
```

### 5.2 SetLimit en Accion

```go
func fetchAllWithLimit(ctx context.Context, urls []string) error {
    g, ctx := errgroup.WithContext(ctx)
    g.SetLimit(20) // maximo 20 goroutines concurrentes

    for _, url := range urls {
        url := url
        g.Go(func() error { // BLOQUEA si ya hay 20 activas
            return fetch(ctx, url)
        })
    }

    return g.Wait()
}
```

### 5.3 Como Funciona Internamente SetLimit

```go
// Pseudocodigo de la implementacion:
func (g *Group) SetLimit(n int) {
    if n < 0 {
        g.sem = nil // sin limite
        return
    }
    if len(g.sem) != 0 {
        panic("errgroup: modify limit while goroutines are active")
    }
    g.sem = make(chan token, n) // channel con buffer de tamano n
}

func (g *Group) Go(f func() error) {
    if g.sem != nil {
        g.sem <- token{} // BLOQUEA si el buffer esta lleno
    }
    g.wg.Add(1)
    go func() {
        defer g.wg.Done()
        defer func() {
            if g.sem != nil {
                <-g.sem // liberar un slot
            }
        }()
        if err := f(); err != nil {
            g.errOnce.Do(func() {
                g.err = err
                if g.cancel != nil {
                    g.cancel(g.err)
                }
            })
        }
    }()
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                     SetLimit(3) CON 6 TAREAS                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Tiempo →                                                                │
│                                                                          │
│  g.Go(t1)  → sem=[T] → goroutine 1 inicia                              │
│  g.Go(t2)  → sem=[T,T] → goroutine 2 inicia                            │
│  g.Go(t3)  → sem=[T,T,T] → goroutine 3 inicia ← BUFFER LLENO          │
│  g.Go(t4)  → BLOQUEA... espera slot libre                              │
│                                                                          │
│  goroutine 1 termina → sem=[T,T] → t4 se desbloquea → goroutine 4      │
│  goroutine 2 termina → sem=[T,T] → g.Go(t5) → goroutine 5              │
│  goroutine 3 termina → sem=[T,T] → g.Go(t6) → goroutine 6              │
│  ...                                                                     │
│  g.Wait() espera a que las 6 goroutines completen                       │
│                                                                          │
│  NOTA: g.Go BLOQUEA al goroutine que lo llama (usualmente main/caller)  │
│  No crea goroutines extra — reutiliza slots conforme se liberan          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.4 Elegir el Limite Correcto

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    GUIA PARA ELEGIR SetLimit(n)                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  TIPO DE TRABAJO             LIMITE SUGERIDO    POR QUE                  │
│  ──────────────              ──────────────     ────────                  │
│  HTTP requests externos      10-50              Rate limits, FDs         │
│  Database queries             pool size          Limitar al pool de DB   │
│  Lectura de archivos locales  runtime.NumCPU()   Bound por I/O          │
│  CPU-bound (criptografia)     runtime.NumCPU()   Mas no ayuda           │
│  Llamadas a APIs con rate     Rate limit / 2     Dejar margen           │
│  SSH a servidores             20-50              Limites de SSH          │
│                                                                          │
│  REGLA GENERAL:                                                          │
│  - I/O-bound: 10-100 (depende del destino)                              │
│  - CPU-bound: runtime.NumCPU()                                           │
│  - Red externa: respetar rate limits del servicio                        │
│  - Bases de datos: <= tamano del connection pool                         │
│  - Archivos: ulimit -n / 2 (dejar FDs para el sistema)                  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.5 SetLimit Restricciones

```go
// ⚠️ PANIC: no puedes cambiar el limite con goroutines activas
g.SetLimit(10)
g.Go(func() error { return work() })
g.SetLimit(20) // PANIC: "modify limit while goroutines are active"

// ✅ Solo puedes SetLimit ANTES de cualquier Go() o DESPUES de Wait()
g.SetLimit(10)
// ... todas las Go() ...
g.Wait()
// Ahora puedes reusar el grupo:
g.SetLimit(20) // OK — no hay goroutines activas
```

---

## 6. TryGo — Lanzamiento No-Bloqueante

### 6.1 API

`TryGo` es la version no-bloqueante de `Go` (disponible cuando se usa `SetLimit`):

```go
// TryGo intenta lanzar la goroutine.
// Retorna true si la goroutine fue lanzada, false si el limite esta alcanzado.
func (g *Group) TryGo(f func() error) bool
```

### 6.2 Caso de Uso: Work Stealing / Best Effort

```go
func processWithBackpressure(ctx context.Context, items []string) error {
    g, ctx := errgroup.WithContext(ctx)
    g.SetLimit(5)

    var skipped []string

    for _, item := range items {
        item := item
        if !g.TryGo(func() error {
            return processItem(ctx, item)
        }) {
            // No hay slots disponibles — registrar para retry posterior
            skipped = append(skipped, item)
        }
    }

    if err := g.Wait(); err != nil {
        return err
    }

    if len(skipped) > 0 {
        fmt.Printf("Skipped %d items (at capacity), will retry later\n", len(skipped))
    }
    return nil
}
```

### 6.3 TryGo vs Go — Decision

```
┌──────────────────────────────────────────────────────────────────────────┐
│                       TryGo vs Go                                        │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Go(f):                              TryGo(f):                           │
│  ──────                              ─────────                           │
│  Siempre ejecuta f                   Ejecuta f SOLO si hay slot libre   │
│  Bloquea si limite alcanzado         Retorna false si limite alcanzado  │
│  "Todas las tareas son obligatorias" "Las tareas son best-effort"       │
│  Garantiza que todo se procesa       Puede perder trabajo               │
│                                                                          │
│  USAR Go CUANDO:                     USAR TryGo CUANDO:                 │
│  - Todas las tareas son criticas     - Puedes tolerar items sin         │
│  - El caller puede bloquear            procesar                          │
│  - Necesitas completitud             - No quieres bloquear el caller    │
│  - Ejemplo: deploy a todos clusters  - Implementas backpressure        │
│                                      - Ejemplo: telemetria, pre-fetch  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Patrones de Recoleccion de Resultados

### 7.1 Mutex para Mapa Compartido

```go
func fetchAllPages(ctx context.Context, urls []string) (map[string][]byte, error) {
    g, ctx := errgroup.WithContext(ctx)
    g.SetLimit(10)

    var mu sync.Mutex
    results := make(map[string][]byte, len(urls))

    for _, url := range urls {
        url := url
        g.Go(func() error {
            data, err := fetch(ctx, url)
            if err != nil {
                return err
            }
            mu.Lock()
            results[url] = data
            mu.Unlock()
            return nil
        })
    }

    if err := g.Wait(); err != nil {
        return nil, err
    }
    return results, nil
}
```

### 7.2 Slice por Indice (Sin Mutex)

Cuando las goroutines escriben a **posiciones diferentes** de un slice, no hay data race — no necesitas mutex:

```go
func checkServers(ctx context.Context, servers []string) ([]ServerStatus, error) {
    g, ctx := errgroup.WithContext(ctx)

    statuses := make([]ServerStatus, len(servers))

    for i, server := range servers {
        i, server := i, server // captura
        g.Go(func() error {
            status, err := checkServer(ctx, server)
            if err != nil {
                return fmt.Errorf("check %s: %w", server, err)
            }
            statuses[i] = status // cada goroutine escribe a su propio indice
            return nil
        })
    }

    if err := g.Wait(); err != nil {
        return nil, err
    }
    return statuses, nil
}
```

**¿Por que es seguro?** Porque `statuses[0]`, `statuses[1]`, `statuses[2]` son **direcciones de memoria diferentes**. Cada goroutine escribe solo a su indice. No hay acceso compartido a la misma posicion. El race detector de Go confirma esto.

### 7.3 Channel para Streaming

```go
type Result struct {
    Server string
    Status ServerStatus
    Err    error
}

func checkServersStream(ctx context.Context, servers []string) <-chan Result {
    results := make(chan Result, len(servers))

    go func() {
        g, ctx := errgroup.WithContext(ctx)
        g.SetLimit(10)

        for _, server := range servers {
            server := server
            g.Go(func() error {
                status, err := checkServer(ctx, server)
                results <- Result{Server: server, Status: status, Err: err}
                // Nota: enviamos SIEMPRE al channel, incluso con error
                // No retornamos el error porque queremos TODOS los resultados
                return nil // ← siempre nil para no cancelar el grupo
            })
        }

        g.Wait()
        close(results) // cerrar cuando todas terminaron
    }()

    return results
}

// Uso:
for result := range checkServersStream(ctx, servers) {
    if result.Err != nil {
        log.Printf("WARN: %s failed: %v", result.Server, result.Err)
        continue
    }
    log.Printf("OK: %s - %s", result.Server, result.Status.State)
}
```

### 7.4 Patron Fan-Out/Fan-In con errgroup

```go
func pipeline(ctx context.Context, urls []string) ([]ProcessedData, error) {
    // Etapa 1: Fan-out — fetch en paralelo
    g, ctx := errgroup.WithContext(ctx)
    g.SetLimit(20)

    raw := make(chan []byte, len(urls))

    for _, url := range urls {
        url := url
        g.Go(func() error {
            data, err := fetch(ctx, url)
            if err != nil {
                return err
            }
            raw <- data
            return nil
        })
    }

    // Cerrar raw channel cuando todas las fetches terminen
    go func() {
        g.Wait()
        close(raw)
    }()

    // Etapa 2: Fan-in — procesar resultados
    g2, ctx2 := errgroup.WithContext(ctx)
    g2.SetLimit(runtime.NumCPU())

    var mu sync.Mutex
    var processed []ProcessedData

    for data := range raw {
        data := data
        g2.Go(func() error {
            result, err := process(ctx2, data)
            if err != nil {
                return err
            }
            mu.Lock()
            processed = append(processed, result)
            mu.Unlock()
            return nil
        })
    }

    if err := g.Wait(); err != nil {
        return nil, fmt.Errorf("fetch stage: %w", err)
    }
    if err := g2.Wait(); err != nil {
        return nil, fmt.Errorf("process stage: %w", err)
    }
    return processed, nil
}
```

---

## 8. Errgroup vs Alternativas

### 8.1 Tabla Comparativa

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                         COORDINACION DE GOROUTINES                               │
├────────────────┬───────────┬──────────┬───────────┬──────────┬──────────────────┤
│                │ WaitGroup │ errgroup │ channel   │ select   │ context          │
├────────────────┼───────────┼──────────┼───────────┼──────────┼──────────────────┤
│ Espera todos   │ ✅         │ ✅        │ manual    │ ✗        │ ✗                │
│ Primer error   │ ✗         │ ✅        │ manual    │ ✗        │ ✗                │
│ Todos errores  │ ✗         │ ✗ (1ro)  │ manual    │ ✗        │ ✗                │
│ Cancelacion    │ ✗         │ ✅ (ctx)  │ manual    │ ✅        │ ✅                │
│ Limite conc.   │ manual    │ ✅        │ ✅ (sem)  │ ✗        │ ✗                │
│ Backpressure   │ ✗         │ ✅ TryGo  │ ✅        │ ✗        │ ✗                │
│ Resultados     │ manual    │ manual   │ ✅        │ ✅        │ ✗                │
│ Boilerplate    │ medio     │ bajo     │ alto      │ alto     │ bajo             │
│ Stdlib         │ ✅         │ ✗ (ext)  │ ✅        │ ✅        │ ✅                │
├────────────────┼───────────┼──────────┼───────────┼──────────┼──────────────────┤
│ USAR CUANDO    │ solo      │ tareas   │ streaming │ multiples│ timeouts         │
│                │ necesitas │ paralelas│ de datos  │ channels │ cancelacion      │
│                │ esperar   │ con err  │ entre     │ o timers │ jerarquica       │
│                │           │ handling │ goroutines│          │                  │
└────────────────┴───────────┴──────────┴───────────┴──────────┴──────────────────┘
```

### 8.2 Errgroup NO Colecta Todos los Errores

Una limitacion importante: `errgroup` solo retorna el **primer** error. Si necesitas **todos** los errores:

```go
// Patron: recolectar todos los errores manualmente
func checkAllServers(ctx context.Context, servers []string) error {
    g, ctx := errgroup.WithContext(ctx)

    var mu sync.Mutex
    var errs []error

    for _, server := range servers {
        server := server
        g.Go(func() error {
            if err := checkServer(ctx, server); err != nil {
                mu.Lock()
                errs = append(errs, fmt.Errorf("%s: %w", server, err))
                mu.Unlock()
            }
            return nil // ← siempre nil para NO cancelar el grupo
        })
    }

    g.Wait() // siempre nil porque nunca retornamos error

    if len(errs) > 0 {
        return errors.Join(errs...) // Go 1.20+: multi-error
    }
    return nil
}
```

**Trade-off**: al retornar siempre `nil` desde `Go()`, pierdes la cancelacion automatica. Si quieres ambas cosas (recolectar todos + cancelar en el primero), necesitas logica custom.

### 8.3 Errgroup vs Worker Pool Manual

```go
// Worker pool manual — mas control, mas codigo:
func workerPool(ctx context.Context, jobs <-chan Job, numWorkers int) <-chan Result {
    results := make(chan Result, numWorkers)
    var wg sync.WaitGroup

    for i := 0; i < numWorkers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for job := range jobs {
                select {
                case <-ctx.Done():
                    return
                default:
                    results <- process(job)
                }
            }
        }()
    }

    go func() {
        wg.Wait()
        close(results)
    }()

    return results
}

// Con errgroup — menos codigo, mismo resultado para la mayoria de casos:
func errGroupPool(ctx context.Context, jobs []Job) ([]Result, error) {
    g, ctx := errgroup.WithContext(ctx)
    g.SetLimit(numWorkers) // ← reemplaza el worker pool manual

    results := make([]Result, len(jobs))
    for i, job := range jobs {
        i, job := i, job
        g.Go(func() error {
            result, err := process(ctx, job)
            if err != nil {
                return err
            }
            results[i] = result
            return nil
        })
    }

    if err := g.Wait(); err != nil {
        return nil, err
    }
    return results, nil
}

// ¿Cuando worker pool manual?
// - Necesitas workers de larga duracion que procesan un stream continuo
// - Necesitas prioridad entre jobs
// - Necesitas canceling individual de workers
// - Necesitas metricas por worker (latencia, throughput)
```

---

## 9. Errgroup en Servers HTTP

### 9.1 Arrancar Multiples Servers

Un patron comun en microservicios: arrancar varios listeners (HTTP, gRPC, metrics) y apagar todo si uno falla:

```go
func runServers(ctx context.Context) error {
    g, ctx := errgroup.WithContext(ctx)

    httpServer := &http.Server{Addr: ":8080", Handler: httpHandler()}
    grpcServer := grpc.NewServer()
    metricsServer := &http.Server{Addr: ":9090", Handler: metricsHandler()}

    // Goroutine 1: HTTP server
    g.Go(func() error {
        log.Println("HTTP server starting on :8080")
        if err := httpServer.ListenAndServe(); err != http.ErrServerClosed {
            return fmt.Errorf("http server: %w", err)
        }
        return nil
    })

    // Goroutine 2: gRPC server
    g.Go(func() error {
        lis, err := net.Listen("tcp", ":9000")
        if err != nil {
            return fmt.Errorf("grpc listen: %w", err)
        }
        log.Println("gRPC server starting on :9000")
        if err := grpcServer.Serve(lis); err != nil {
            return fmt.Errorf("grpc server: %w", err)
        }
        return nil
    })

    // Goroutine 3: Metrics server
    g.Go(func() error {
        log.Println("Metrics server starting on :9090")
        if err := metricsServer.ListenAndServe(); err != http.ErrServerClosed {
            return fmt.Errorf("metrics server: %w", err)
        }
        return nil
    })

    // Goroutine 4: Graceful shutdown
    g.Go(func() error {
        <-ctx.Done() // esperar a que el contexto se cancele (error o senal)

        log.Println("Shutting down servers...")
        shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
        defer cancel()

        // Apagar todos los servers en paralelo
        var shutdownGroup errgroup.Group
        shutdownGroup.Go(func() error { return httpServer.Shutdown(shutdownCtx) })
        shutdownGroup.Go(func() error { grpcServer.GracefulStop(); return nil })
        shutdownGroup.Go(func() error { return metricsServer.Shutdown(shutdownCtx) })

        return shutdownGroup.Wait()
    })

    return g.Wait()
}
```

### 9.2 Errgroup con Senales del SO

```go
func main() {
    ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
    defer stop()

    if err := runServers(ctx); err != nil {
        log.Fatalf("Server error: %v", err)
    }
    log.Println("Clean shutdown complete")
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                  LIFECYCLE CON ERRGROUP + SIGNALS                        │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  main()                                                                  │
│  │                                                                       │
│  ├─ signal.NotifyContext(SIGINT, SIGTERM)                                │
│  │                                                                       │
│  ├─ g, gctx := errgroup.WithContext(ctx)                                │
│  │                                                                       │
│  ├─ g.Go(httpServer)   ── ListenAndServe ──────────────┐                │
│  ├─ g.Go(grpcServer)   ── Serve ───────────────────────┤                │
│  ├─ g.Go(metricsServer)── ListenAndServe ──────────────┤                │
│  │                                                      │                │
│  ├─ g.Go(shutdown)     ── <-gctx.Done() ← ────────────┤                │
│  │                         │                            │                │
│  │                    [SIGINT recibido]                  │                │
│  │                         │                            │                │
│  │                    ctx cancelado                      │                │
│  │                         │                            │                │
│  │                    gctx cancelado                     │                │
│  │                         │                            │                │
│  │                    shutdown goroutine                  │                │
│  │                    se desbloquea                       │                │
│  │                         │                            │                │
│  │                    httpServer.Shutdown()              │                │
│  │                    grpcServer.GracefulStop()          │                │
│  │                    metricsServer.Shutdown()           │                │
│  │                         │                            │                │
│  │                    ListenAndServe retorna             │                │
│  │                    http.ErrServerClosed               │                │
│  │                    (filtrado → return nil)            │                │
│  │                                                      │                │
│  └─ g.Wait() retorna nil ← todas las goroutines        │                │
│                              terminaron sin error       │                │
│                                                                          │
│  log.Println("Clean shutdown complete")                                  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 10. Antipatrones y Errores Comunes

### 10.1 Panic Dentro de errgroup.Go

```go
// ⚠️ CRASH: un panic en una goroutine de errgroup mata el programa
g.Go(func() error {
    data := fetchData()
    return process(data[0]) // panic: index out of range
})

// errgroup NO recupera panics — el panic se propaga y crash

// ✅ SOLUCION: recover dentro de cada goroutine
g.Go(func() (err error) {
    defer func() {
        if r := recover(); r != nil {
            err = fmt.Errorf("panic recovered: %v\n%s", r, debug.Stack())
        }
    }()
    data := fetchData()
    return process(data[0])
})

// O mejor: funcion wrapper reutilizable
func safeGo(g *errgroup.Group, f func() error) {
    g.Go(func() (err error) {
        defer func() {
            if r := recover(); r != nil {
                err = fmt.Errorf("panic: %v\n%s", r, debug.Stack())
            }
        }()
        return f()
    })
}

safeGo(&g, func() error {
    return riskyOperation()
})
```

### 10.2 Goroutine Leak con Channels

```go
// ⚠️ LEAK: si errgroup cancela y nadie lee del channel
g.Go(func() error {
    result := compute()
    ch <- result // ← BLOQUEA PARA SIEMPRE si nadie lee
    return nil
})

// ✅ SOLUCION: select con contexto
g.Go(func() error {
    result := compute()
    select {
    case ch <- result:
        return nil
    case <-ctx.Done():
        return ctx.Err() // cancelado, no intentar enviar
    }
})
```

### 10.3 No Verificar el Contexto en Operaciones Largas

```go
// ⚠️ NO REACCIONA A CANCELACION
g.Go(func() error {
    for _, item := range millionItems {
        process(item) // ← no verifica ctx, sigue procesando aun cuando grupo cancelado
    }
    return nil
})

// ✅ VERIFICAR ctx periodicamente
g.Go(func() error {
    for _, item := range millionItems {
        select {
        case <-ctx.Done():
            return ctx.Err() // reaccionar rapido
        default:
        }
        if err := process(ctx, item); err != nil {
            return err
        }
    }
    return nil
})

// ✅ ALTERNATIVA: verificar cada N iteraciones (menos overhead)
g.Go(func() error {
    for i, item := range millionItems {
        if i%1000 == 0 { // verificar cada 1000 items
            select {
            case <-ctx.Done():
                return ctx.Err()
            default:
            }
        }
        if err := process(ctx, item); err != nil {
            return err
        }
    }
    return nil
})
```

### 10.4 Olvidar Wait

```go
// ⚠️ BUG: goroutines quedan en el aire
func badExample() {
    var g errgroup.Group
    g.Go(func() error {
        return doWork()
    })
    // ← nunca llamas g.Wait()
    // La goroutine puede seguir ejecutando despues de que badExample retorna
    // Resultado: race conditions, writes a memoria ya liberada
}

// ✅ SIEMPRE llamar Wait
func goodExample() error {
    var g errgroup.Group
    g.Go(func() error {
        return doWork()
    })
    return g.Wait() // esperar SIEMPRE
}
```

### 10.5 Reusar un Group Incorrectamente

```go
// ⚠️ POTENCIAL BUG: reusar un grupo despues de Wait
var g errgroup.Group
g.Go(func() error { return task1() })
g.Wait() // OK

g.Go(func() error { return task2() }) // ← Funciona pero...
g.Wait()
// ¿Y el error de task1? g.err ya tiene el error de la primera ronda
// g.errOnce ya se ejecuto — NO almacenara errores de la segunda ronda

// ✅ CREAR un nuevo Group para cada batch
func processBatch(batch []Item) error {
    var g errgroup.Group // nuevo grupo para cada batch
    for _, item := range batch {
        item := item
        g.Go(func() error { return process(item) })
    }
    return g.Wait()
}
```

### 10.6 Tabla de Antipatrones

```
┌─────────────────────────────────────┬─────────────────────────────────────┐
│  ANTIPATRON                         │  SOLUCION                           │
├─────────────────────────────────────┼─────────────────────────────────────┤
│ Panic dentro de g.Go (crash)        │ defer recover en cada goroutine    │
│ No llamar g.Wait() (leak)           │ Siempre llamar Wait                │
│ Reusar Group despues de Wait        │ Nuevo Group por batch              │
│ Channel sin select (leak)           │ select con ctx.Done()              │
│ No verificar ctx (no cancela)       │ select periodico en loops largos   │
│ Data race en mapa compartido        │ sync.Mutex o slice por indice      │
│ Shadowing ctx (ctx original perdido)│ Usar gctx para el grupo            │
│ SetLimit despues de Go (panic)      │ SetLimit antes de cualquier Go     │
│ return nil siempre (pierde error)   │ return err cuando error es critico │
│ Captura variable loop (Go <1.22)    │ v := v antes del closure           │
└─────────────────────────────────────┴─────────────────────────────────────┘
```

---

## 11. Comparacion con C y Rust

### 11.1 C — pthreads con Manejo de Errores Manual

C no tiene errgroup. La coordinacion de threads con errores requiere todo manualmente:

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const char *server;
    int result;     // 0 = OK, -1 = error
    char error[256];
} CheckResult;

void *check_server(void *arg) {
    CheckResult *r = (CheckResult *)arg;
    // Simular check
    if (strcmp(r->server, "bad-server") == 0) {
        r->result = -1;
        snprintf(r->error, sizeof(r->error), "%s: connection refused", r->server);
    } else {
        r->result = 0;
    }
    return NULL;
}

int check_all_servers(const char **servers, int count) {
    pthread_t *threads = malloc(count * sizeof(pthread_t));
    CheckResult *results = calloc(count, sizeof(CheckResult));
    int first_error = -1;

    // Crear threads (equivalente a g.Go)
    for (int i = 0; i < count; i++) {
        results[i].server = servers[i];
        int ret = pthread_create(&threads[i], NULL, check_server, &results[i]);
        if (ret != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(ret));
            // ¿Cancelar los anteriores? Necesitas logica manual...
            first_error = i;
            break;
        }
    }

    // Join todos los threads (equivalente a g.Wait)
    int created = (first_error >= 0) ? first_error : count;
    for (int i = 0; i < created; i++) {
        pthread_join(threads[i], NULL);
    }

    // Verificar errores (manual — errgroup hace esto automaticamente)
    int err = 0;
    for (int i = 0; i < created; i++) {
        if (results[i].result != 0) {
            if (err == 0) {
                fprintf(stderr, "First error: %s\n", results[i].error);
            }
            err = 1;
        }
    }

    free(threads);
    free(results);
    return err ? -1 : 0;
}

/*
 * PROBLEMAS vs errgroup:
 * - No hay cancelacion automatica (necesitas pthread_cancel o flag compartida)
 * - No hay limitacion de concurrencia (necesitas semaforo manual)
 * - Cada thread cuesta ~1MB de stack vs ~2KB goroutine
 * - pthread_cancel es peligroso (puede dejar mutex locked, memoria sin liberar)
 * - Manejo de errores completamente manual (struct de resultados)
 * - Memory management manual (malloc/free para threads y resultados)
 * - ulimit -u limita threads del sistema (tipicamente 4096-32768)
 *   vs goroutines que escalan a millones
 */
```

### 11.2 Rust — tokio::JoinSet y Equivalentes

Rust tiene varias opciones, la mas cercana a errgroup es `tokio::task::JoinSet`:

```rust
use tokio::task::JoinSet;
use anyhow::{Result, Context};
use std::time::Duration;

async fn check_server(server: &str) -> Result<()> {
    let client = reqwest::Client::builder()
        .timeout(Duration::from_secs(5))
        .build()?;

    let resp = client.get(format!("http://{}/health", server))
        .send()
        .await
        .context(format!("check {}", server))?;

    if !resp.status().is_success() {
        anyhow::bail!("check {}: status {}", server, resp.status());
    }
    Ok(())
}

// Opcion 1: JoinSet (mas parecido a errgroup)
async fn check_all_joinset(servers: Vec<String>) -> Result<()> {
    let mut set = JoinSet::new();

    for server in servers {
        set.spawn(async move {
            check_server(&server).await
        });
    }

    // JoinSet devuelve resultados uno a uno (no solo el primero)
    while let Some(result) = set.join_next().await {
        match result {
            Ok(Ok(())) => {} // tarea completo OK
            Ok(Err(e)) => return Err(e), // error de la tarea
            Err(e) => return Err(e.into()), // panic en la tarea (JoinError)
        }
    }
    Ok(())
}

// Opcion 2: futures::future::try_join_all (mas conciso)
use futures::future::try_join_all;

async fn check_all_try_join(servers: Vec<String>) -> Result<()> {
    let futures: Vec<_> = servers.iter()
        .map(|s| check_server(s))
        .collect();

    try_join_all(futures).await?;
    // Si cualquier future falla, retorna el primer error
    // Los demas futures se DROPEAN (cancelacion automatica)
    Ok(())
}

// Opcion 3: tokio con semaforo (equivalente a SetLimit)
use tokio::sync::Semaphore;
use std::sync::Arc;

async fn check_all_limited(servers: Vec<String>, max_concurrent: usize) -> Result<()> {
    let semaphore = Arc::new(Semaphore::new(max_concurrent));
    let mut set = JoinSet::new();

    for server in servers {
        let permit = semaphore.clone().acquire_owned().await?;
        set.spawn(async move {
            let _permit = permit; // se libera al salir del scope (RAII)
            check_server(&server).await
        });
    }

    while let Some(result) = set.join_next().await {
        result??; // desempaquetar JoinError y luego el error de la tarea
    }
    Ok(())
}

/*
 * COMPARACION errgroup vs Rust:
 *
 * ┌──────────────────────┬────────────────────┬───────────────────────┐
 * │                      │ Go errgroup        │ Rust JoinSet          │
 * ├──────────────────────┼────────────────────┼───────────────────────┤
 * │ Error retornado      │ Solo el primero    │ Todos (uno por uno)   │
 * │ Cancelacion          │ Via contexto       │ Drop del JoinSet      │
 * │ Panic handling       │ NO (crash)         │ JoinError captura     │
 * │ Limite concurrencia  │ SetLimit (built-in)│ Semaphore (separado)  │
 * │ API                  │ Go/Wait/SetLimit   │ spawn/join_next/abort │
 * │ Tipo de concurrencia │ Goroutines (OS)    │ Async tasks (runtime) │
 * │ Thread safety        │ Runtime check      │ Compile-time (Send)   │
 * └──────────────────────┴────────────────────┴───────────────────────┘
 *
 * Ventajas de errgroup:
 * - API mas simple (3 metodos vs spawn+join_next+abort+shutdown)
 * - Contexto integrado (WithContext)
 * - SetLimit built-in (Rust necesita semaforo separado)
 *
 * Ventajas de JoinSet:
 * - Devuelve TODOS los resultados (no solo el primero)
 * - Captura panics automaticamente (JoinError)
 * - Compile-time safety (Send + 'static)
 * - abort_all() para cancelacion inmediata
 * - JoinSet se dropea = todas las tareas canceladas (RAII)
 */
```

### 11.3 Tabla Comparativa C vs Go vs Rust

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│           GOROUTINES PARALELAS CON ERROR HANDLING: C vs Go vs Rust               │
├──────────────────┬──────────────────┬──────────────────┬──────────────────────────┤
│                  │ C (pthreads)     │ Go (errgroup)    │ Rust (JoinSet)           │
├──────────────────┼──────────────────┼──────────────────┼──────────────────────────┤
│ Lineas de codigo │ 60-80            │ 15-20            │ 20-30                    │
│ Error handling   │ Manual (struct)  │ return error     │ Result<T,E>              │
│ Primer error     │ Flag + mutex     │ errOnce (auto)   │ join_next() manual       │
│ Todos errores    │ Array manual     │ return nil + mu  │ join_next() loop         │
│ Cancelacion      │ pthread_cancel   │ context.Cancel   │ Drop / abort_all         │
│                  │ (peligroso)      │ (cooperativa)    │ (cooperativa)            │
│ Limite conc.     │ Semaforo POSIX   │ SetLimit         │ Semaphore (tokio)        │
│ Panic/crash      │ SIGSEGV (muerte) │ Crash (no recov) │ JoinError (capturado)    │
│ Coste por unidad │ ~1MB stack       │ ~2KB stack       │ ~pocos bytes (async)     │
│ Max practico     │ ~4,000 threads   │ ~1,000,000       │ ~1,000,000+ (async)      │
│ Data race detect │ Valgrind/TSan    │ -race flag       │ Compilador (Send/Sync)   │
│ Memory safety    │ Manual           │ GC               │ Borrow checker           │
└──────────────────┴──────────────────┴──────────────────┴──────────────────────────┘
```

---

## 12. Programa Completo: Infrastructure Health Monitor

Un sistema de monitoreo que verifica la salud de multiples servicios de infraestructura en paralelo, con limites de concurrencia, timeouts, reintentos, y recoleccion de todos los resultados:

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "math/rand"
    "os"
    "os/signal"
    "strings"
    "sync"
    "syscall"
    "time"

    "golang.org/x/sync/errgroup"
)

// ─────────────────────────────────────────────────────────────────
// Tipos de error
// ─────────────────────────────────────────────────────────────────

var (
    ErrTimeout     = errors.New("timeout")
    ErrUnhealthy   = errors.New("unhealthy")
    ErrUnreachable = errors.New("unreachable")
)

type ServiceError struct {
    Service  string
    Op       string
    Duration time.Duration
    Err      error
}

func (e *ServiceError) Error() string {
    return fmt.Sprintf("[%s] %s failed after %v: %v", e.Service, e.Op, e.Duration, e.Err)
}

func (e *ServiceError) Unwrap() error { return e.Err }

// ─────────────────────────────────────────────────────────────────
// Modelo de servicio
// ─────────────────────────────────────────────────────────────────

type ServiceType int

const (
    TypeHTTP ServiceType = iota
    TypeDatabase
    TypeCache
    TypeQueue
    TypeDNS
)

func (t ServiceType) String() string {
    switch t {
    case TypeHTTP:
        return "HTTP"
    case TypeDatabase:
        return "Database"
    case TypeCache:
        return "Cache"
    case TypeQueue:
        return "Queue"
    case TypeDNS:
        return "DNS"
    default:
        return "Unknown"
    }
}

type Service struct {
    Name     string
    Type     ServiceType
    Endpoint string
    Critical bool // si falla un servicio critico, abortar todo
}

type HealthResult struct {
    Service   Service
    Healthy   bool
    Latency   time.Duration
    Message   string
    CheckedAt time.Time
    Err       error
}

func (r HealthResult) String() string {
    status := "OK"
    if !r.Healthy {
        status = "FAIL"
    }
    return fmt.Sprintf("[%s] %-20s %-8s latency=%-10v %s",
        status, r.Service.Name, r.Service.Type, r.Latency, r.Message)
}

// ─────────────────────────────────────────────────────────────────
// Simulador de health checks
// ─────────────────────────────────────────────────────────────────

// checkService simula un health check con latencia y posibles fallos
func checkService(ctx context.Context, svc Service) (*HealthResult, error) {
    start := time.Now()

    // Simular latencia variable segun tipo de servicio
    var baseLatency time.Duration
    switch svc.Type {
    case TypeHTTP:
        baseLatency = 50 * time.Millisecond
    case TypeDatabase:
        baseLatency = 100 * time.Millisecond
    case TypeCache:
        baseLatency = 10 * time.Millisecond
    case TypeQueue:
        baseLatency = 30 * time.Millisecond
    case TypeDNS:
        baseLatency = 5 * time.Millisecond
    }

    // Anadir variabilidad
    jitter := time.Duration(rand.Intn(50)) * time.Millisecond
    checkDuration := baseLatency + jitter

    // Simular el check con respeto al contexto
    select {
    case <-ctx.Done():
        return nil, &ServiceError{
            Service:  svc.Name,
            Op:       "health_check",
            Duration: time.Since(start),
            Err:      fmt.Errorf("%w: %w", ErrTimeout, ctx.Err()),
        }
    case <-time.After(checkDuration):
        // Check completado
    }

    // Simular fallos basados en el nombre del servicio
    result := &HealthResult{
        Service:   svc,
        Latency:   time.Since(start),
        CheckedAt: time.Now(),
    }

    switch {
    case strings.Contains(svc.Name, "failing"):
        result.Healthy = false
        result.Message = "connection refused"
        result.Err = &ServiceError{
            Service:  svc.Name,
            Op:       "health_check",
            Duration: result.Latency,
            Err:      fmt.Errorf("%w: connection refused on %s", ErrUnreachable, svc.Endpoint),
        }

    case strings.Contains(svc.Name, "degraded"):
        result.Healthy = false
        result.Message = "high latency (>500ms)"
        result.Err = &ServiceError{
            Service:  svc.Name,
            Op:       "health_check",
            Duration: result.Latency,
            Err:      ErrUnhealthy,
        }

    case strings.Contains(svc.Name, "timeout"):
        // Simular un servicio que tarda demasiado
        select {
        case <-ctx.Done():
            return nil, &ServiceError{
                Service:  svc.Name,
                Op:       "health_check",
                Duration: time.Since(start),
                Err:      fmt.Errorf("%w: %w", ErrTimeout, ctx.Err()),
            }
        case <-time.After(2 * time.Second):
            result.Healthy = true
            result.Latency = time.Since(start)
            result.Message = "responded (slow)"
        }

    default:
        result.Healthy = true
        result.Message = "all checks passed"
    }

    return result, result.Err
}

// ─────────────────────────────────────────────────────────────────
// Monitor con errgroup — Estrategia "fail fast en criticos"
// ─────────────────────────────────────────────────────────────────

// MonitorFailFast: si un servicio CRITICO falla, cancela todos los demas checks
func MonitorFailFast(ctx context.Context, services []Service) ([]HealthResult, error) {
    g, gctx := errgroup.WithContext(ctx)
    g.SetLimit(5) // maximo 5 checks simultaneos

    results := make([]HealthResult, len(services))

    for i, svc := range services {
        i, svc := i, svc
        g.Go(func() error {
            result, err := checkService(gctx, svc)
            if result != nil {
                results[i] = *result
            }

            if err != nil && svc.Critical {
                // Servicio critico fallo → retornar error para cancelar el grupo
                return fmt.Errorf("critical service %s failed: %w", svc.Name, err)
            }

            // Servicios no criticos: registrar el resultado pero no cancelar
            return nil
        })
    }

    err := g.Wait()
    return results, err
}

// ─────────────────────────────────────────────────────────────────
// Monitor con errgroup — Estrategia "recolectar todos"
// ─────────────────────────────────────────────────────────────────

// MonitorCollectAll: verifica TODOS los servicios, recolecta todos los errores
func MonitorCollectAll(ctx context.Context, services []Service) ([]HealthResult, error) {
    g, gctx := errgroup.WithContext(ctx)
    g.SetLimit(5)

    var mu sync.Mutex
    var results []HealthResult
    var errs []error

    for _, svc := range services {
        svc := svc
        g.Go(func() error {
            result, err := checkService(gctx, svc)

            mu.Lock()
            defer mu.Unlock()

            if result != nil {
                results = append(results, *result)
            } else {
                results = append(results, HealthResult{
                    Service:   svc,
                    Healthy:   false,
                    CheckedAt: time.Now(),
                    Message:   "check failed before result",
                    Err:       err,
                })
            }

            if err != nil {
                errs = append(errs, err)
            }

            return nil // siempre nil — no cancelar el grupo
        })
    }

    g.Wait() // siempre nil

    if len(errs) > 0 {
        return results, fmt.Errorf("%d services unhealthy: %w", len(errs), errors.Join(errs...))
    }
    return results, nil
}

// ─────────────────────────────────────────────────────────────────
// Monitor con reintentos
// ─────────────────────────────────────────────────────────────────

func checkWithRetry(ctx context.Context, svc Service, maxRetries int) (*HealthResult, error) {
    var lastErr error

    for attempt := 0; attempt <= maxRetries; attempt++ {
        if attempt > 0 {
            // Backoff exponencial: 100ms, 200ms, 400ms...
            backoff := time.Duration(1<<uint(attempt-1)) * 100 * time.Millisecond
            select {
            case <-ctx.Done():
                return nil, fmt.Errorf("retry cancelled: %w", ctx.Err())
            case <-time.After(backoff):
            }
        }

        result, err := checkService(ctx, svc)
        if err == nil {
            if attempt > 0 {
                result.Message = fmt.Sprintf("%s (after %d retries)", result.Message, attempt)
            }
            return result, nil
        }
        lastErr = err
    }

    return nil, fmt.Errorf("all %d retries exhausted for %s: %w", maxRetries, svc.Name, lastErr)
}

func MonitorWithRetries(ctx context.Context, services []Service, maxRetries int) ([]HealthResult, error) {
    g, gctx := errgroup.WithContext(ctx)
    g.SetLimit(5)

    results := make([]HealthResult, len(services))

    for i, svc := range services {
        i, svc := i, svc
        g.Go(func() error {
            result, err := checkWithRetry(gctx, svc, maxRetries)
            if result != nil {
                results[i] = *result
            } else {
                results[i] = HealthResult{
                    Service:   svc,
                    Healthy:   false,
                    CheckedAt: time.Now(),
                    Err:       err,
                }
            }

            if err != nil && svc.Critical {
                return err // cancelar grupo solo si es critico
            }
            return nil
        })
    }

    err := g.Wait()
    return results, err
}

// ─────────────────────────────────────────────────────────────────
// Monitor periodico con graceful shutdown
// ─────────────────────────────────────────────────────────────────

func MonitorPeriodic(ctx context.Context, services []Service, interval time.Duration) error {
    ticker := time.NewTicker(interval)
    defer ticker.Stop()

    round := 0
    for {
        round++
        fmt.Printf("\n========== Health Check Round %d ==========\n", round)

        results, err := MonitorCollectAll(ctx, services)

        // Imprimir resultados
        healthy, unhealthy := 0, 0
        for _, r := range results {
            fmt.Println(r)
            if r.Healthy {
                healthy++
            } else {
                unhealthy++
            }
        }

        fmt.Printf("─── Summary: %d healthy, %d unhealthy", healthy, unhealthy)
        if err != nil {
            fmt.Printf(" — errors: %v", err)
        }
        fmt.Println()

        // Esperar al siguiente tick o cancelacion
        select {
        case <-ctx.Done():
            fmt.Println("\nMonitor stopped:", ctx.Err())
            return nil
        case <-ticker.C:
            // siguiente ronda
        }
    }
}

// ─────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────

func main() {
    services := []Service{
        {Name: "api-gateway", Type: TypeHTTP, Endpoint: "api.prod:8080", Critical: true},
        {Name: "user-service", Type: TypeHTTP, Endpoint: "users.prod:8081", Critical: true},
        {Name: "postgres-primary", Type: TypeDatabase, Endpoint: "db.prod:5432", Critical: true},
        {Name: "postgres-replica", Type: TypeDatabase, Endpoint: "db-ro.prod:5432", Critical: false},
        {Name: "redis-cache", Type: TypeCache, Endpoint: "redis.prod:6379", Critical: false},
        {Name: "rabbitmq", Type: TypeQueue, Endpoint: "mq.prod:5672", Critical: false},
        {Name: "dns-resolver", Type: TypeDNS, Endpoint: "dns.prod:53", Critical: false},
        {Name: "failing-service", Type: TypeHTTP, Endpoint: "fail.prod:8080", Critical: false},
        {Name: "degraded-cache", Type: TypeCache, Endpoint: "cache2.prod:6379", Critical: false},
    }

    ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
    defer stop()

    fmt.Println("=== Infrastructure Health Monitor ===")
    fmt.Printf("Monitoring %d services (Ctrl+C to stop)\n", len(services))

    // Estrategia 1: Single check, fail fast en criticos
    fmt.Println("\n--- Strategy: Fail Fast (critical services) ---")
    results, err := MonitorFailFast(ctx, services)
    for _, r := range results {
        if r.Service.Name != "" { // skip vacios (cancelados)
            fmt.Println(r)
        }
    }
    if err != nil {
        fmt.Printf("⛔ Critical failure: %v\n", err)
    } else {
        fmt.Println("✓ All critical services healthy")
    }

    // Estrategia 2: Recolectar todos
    fmt.Println("\n--- Strategy: Collect All ---")
    results, err = MonitorCollectAll(ctx, services)
    healthy, unhealthy := 0, 0
    for _, r := range results {
        fmt.Println(r)
        if r.Healthy {
            healthy++
        } else {
            unhealthy++
        }
    }
    fmt.Printf("Summary: %d/%d healthy\n", healthy, len(services))
    if err != nil {
        fmt.Printf("Errors: %v\n", err)
    }

    // Estrategia 3: Con reintentos
    fmt.Println("\n--- Strategy: With Retries (max 2) ---")
    results, err = MonitorWithRetries(ctx, services, 2)
    for _, r := range results {
        if r.Service.Name != "" {
            fmt.Println(r)
        }
    }
    if err != nil {
        fmt.Printf("After retries: %v\n", err)
    }

    // Estrategia 4: Monitoreo periodico (comentado para demo)
    // fmt.Println("\n--- Strategy: Periodic (every 10s) ---")
    // MonitorPeriodic(ctx, services, 10*time.Second)

    fmt.Println("\n=== Monitor Complete ===")
}
```

### Ejecutar:

```bash
# Instalar dependencia
go mod init infra-monitor
go get golang.org/x/sync/errgroup

# Ejecutar
go run main.go

# Ejecutar con race detector
go run -race main.go

# Output esperado (ejemplo):
# === Infrastructure Health Monitor ===
# Monitoring 9 services (Ctrl+C to stop)
#
# --- Strategy: Fail Fast (critical services) ---
# [OK]   api-gateway          HTTP     latency=72ms       all checks passed
# [OK]   user-service         HTTP     latency=68ms       all checks passed
# [OK]   postgres-primary     Database latency=134ms      all checks passed
# [OK]   postgres-replica     Database latency=112ms      all checks passed
# [OK]   redis-cache          Cache    latency=28ms       all checks passed
# [OK]   rabbitmq             Queue    latency=55ms       all checks passed
# [OK]   dns-resolver         DNS      latency=14ms       all checks passed
# [FAIL] failing-service      HTTP     latency=82ms       connection refused
# [FAIL] degraded-cache       Cache    latency=42ms       high latency (>500ms)
# ✓ All critical services healthy (failing ones are not critical)
```

### Analisis del programa:

```
┌──────────────────────────────────────────────────────────────────────────┐
│               PATRONES DE ERRGROUP USADOS EN EL PROGRAMA                 │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. WithContext (cancelacion cooperativa)                                │
│     → MonitorFailFast: cancela otros checks si un critico falla         │
│                                                                          │
│  2. SetLimit(5) (control de concurrencia)                               │
│     → Todos los monitors: maximo 5 checks simultaneos                   │
│                                                                          │
│  3. Slice por indice (sin mutex)                                        │
│     → MonitorFailFast, MonitorWithRetries: results[i] = ...             │
│                                                                          │
│  4. Mutex para resultados dinamicos                                     │
│     → MonitorCollectAll: append a slice + recolectar errores            │
│                                                                          │
│  5. Return nil para recolectar todos                                    │
│     → MonitorCollectAll: siempre nil, errores en slice separado         │
│                                                                          │
│  6. Return error para fail-fast                                         │
│     → MonitorFailFast: return error SOLO si svc.Critical                │
│                                                                          │
│  7. Contexto para cancelacion                                           │
│     → checkService: select con ctx.Done()                               │
│     → MonitorPeriodic: select entre ticker y ctx.Done()                 │
│                                                                          │
│  8. signal.NotifyContext (graceful shutdown)                             │
│     → main: Ctrl+C cancela el contexto raiz                             │
│                                                                          │
│  9. Errgroup anidado                                                    │
│     → MonitorPeriodic llama a MonitorCollectAll (grupo dentro de loop)  │
│                                                                          │
│  10. Reintentos con backoff                                              │
│      → checkWithRetry: backoff exponencial con respeto a ctx            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: File Validator Paralelo

Implementa un validador que verifica multiples archivos de configuracion en paralelo:

```
Requisitos:
- Recibir una lista de paths a archivos YAML/JSON/TOML
- Validar cada archivo en una goroutine separada (existencia, permisos, sintaxis)
- Usar SetLimit(runtime.NumCPU()) para no exceder los CPU cores
- Si un archivo critico (marcado con flag) falla validacion, cancelar todo
- Recolectar TODOS los errores de archivos no-criticos
- Usar errors.Join para el resultado final
- Imprimir un reporte con: path, status, latencia del check, error (si aplica)
```

### Ejercicio 2: Multi-Cluster Deploy

Implementa un deploy simulado a multiples clusters de Kubernetes:

```
Requisitos:
- Lista de clusters con nombre, region, y prioridad (primary/secondary)
- Fase 1: pre-check en paralelo a TODOS los clusters (errgroup)
- Fase 2: deploy primero a primary clusters (errgroup con cancelacion)
  - Si el deploy a primary falla, NO deployar a secondary
- Fase 3: deploy a secondary clusters (errgroup collect all)
- Cada deploy tiene: validate → pull_image → apply → verify (secuencial dentro de la goroutine)
- SetLimit(3) para no saturar el control plane de K8s
- Reintentos con backoff para fallos transitorios
- Reporte final: cuantos clusters exitosos, fallidos, cancelados
```

### Ejercicio 3: Log Aggregator con Pipeline

Implementa un pipeline de procesamiento de logs usando multiples errgroups:

```
Requisitos:
- Etapa 1 (Ingest): leer logs de N fuentes en paralelo (errgroup, SetLimit(10))
  - Enviar lineas a un channel compartido
- Etapa 2 (Transform): parsear lineas del channel (errgroup, SetLimit(CPU))
  - Filtrar por severidad, extraer campos, normalizar formato
  - Enviar resultados parseados a otro channel
- Etapa 3 (Output): escribir resultados a M destinos (errgroup, SetLimit(5))
  - Archivo, stdout, "remote" (simulado)
- Si Ingest falla en una fuente critica → cancelar todo
- Si Transform encuentra un error de parseo → logear y continuar
- Si Output falla en un destino → marcar como degradado, continuar con los otros
- Implementar graceful shutdown con signal.NotifyContext
```

### Ejercicio 4: DNS Health Checker con TryGo

Implementa un verificador de DNS que usa `TryGo` para backpressure:

```
Requisitos:
- Lista de 100+ dominios para verificar
- SetLimit(20) para no saturar el resolver
- Usar TryGo: si no hay slot libre, encolar para retry posterior
- Implementar cola de retry con backoff
- Timeout global de 30 segundos (context.WithTimeout)
- Recolectar: dominios exitosos, fallidos, no verificados (skipped por timeout)
- Reporte con porcentaje de exito y latencia promedio
```

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                       ERRGROUP — RESUMEN COMPLETO                        │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  CREACION:                                                               │
│  ├─ var g errgroup.Group              → sin contexto                    │
│  └─ g, ctx := errgroup.WithContext(c) → con cancelacion automatica      │
│                                                                          │
│  API:                                                                    │
│  ├─ g.Go(func() error)     → lanzar goroutine (bloquea si limite)       │
│  ├─ g.TryGo(func() error)  → lanzar si hay slot (non-blocking)         │
│  ├─ g.Wait() error          → esperar todas, retorna primer error       │
│  └─ g.SetLimit(n)           → maximo n goroutines concurrentes          │
│                                                                          │
│  PATRONES:                                                               │
│  ├─ Fail fast: return err en g.Go → cancela via contexto                │
│  ├─ Collect all: return nil + mu.Lock + append errs                     │
│  ├─ Slice por indice: results[i] = ... (sin mutex)                      │
│  ├─ Channel streaming: go func(){ g.Wait(); close(ch) }()              │
│  ├─ Fan-out/fan-in: dos errgroups en pipeline                           │
│  ├─ Multi-server: WithContext + signal.NotifyContext                    │
│  └─ Retry: loop con backoff + select ctx.Done()                         │
│                                                                          │
│  ANTIPATRONES:                                                           │
│  ├─ Panic sin recover (crash)                                           │
│  ├─ Channel sin select ctx (leak)                                       │
│  ├─ No verificar ctx en loops largos                                    │
│  ├─ Olvidar Wait (goroutine leak)                                       │
│  ├─ Reusar Group despues de Wait (errOnce consumido)                    │
│  └─ Shadowing ctx (perder el original)                                  │
│                                                                          │
│  CUANDO USAR:                                                            │
│  ├─ Tareas paralelas que pueden fallar                                  │
│  ├─ Health checks, fetches, deploys                                     │
│  ├─ Pipelines con etapas concurrentes                                   │
│  └─ Startup/shutdown de multiples servers                                │
│                                                                          │
│  CUANDO NO USAR:                                                        │
│  ├─ Necesitas TODOS los errores → collect all pattern                   │
│  ├─ Worker pool de larga duracion → channels + WaitGroup                │
│  ├─ Una sola goroutine → no necesitas errgroup                          │
│  └─ Sin error handling → sync.WaitGroup es suficiente                   │
│                                                                          │
│  vs C: manual threads+mutex+struct → 4x mas codigo                     │
│  vs Rust: JoinSet (todos resultados, panic safe, compile-time safety)   │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C07/S02 - Patrones Avanzados, T02 - Error handling en APIs — patrones para HTTP handlers, middleware de errores
