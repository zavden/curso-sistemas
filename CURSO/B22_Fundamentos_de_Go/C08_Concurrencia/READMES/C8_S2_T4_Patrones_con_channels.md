# Patrones con Channels — Fan-out/Fan-in, Pipeline, Done Channel, Semaforos

## 1. Introduccion

Los channels de Go no son solo un mecanismo de comunicacion — son **bloques de construccion composables** que, combinados con goroutines y select, forman patrones de concurrencia bien definidos. Estos patrones fueron documentados por primera vez por Rob Pike en "Go Concurrency Patterns" (2012) y expandidos por Sameer Ajmani en "Advanced Go Concurrency Patterns" (2013). Son la base sobre la que se construye toda la concurrencia idiomatica en Go.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    PATRONES DE CONCURRENCIA CON CHANNELS                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Los patrones se dividen en categorias:                                 │
│                                                                          │
│  TOPOLOGIA DE DATOS:                                                     │
│  ├─ Pipeline:       A → B → C → D  (cadena secuencial)                │
│  ├─ Fan-out:        A → { B, C, D }  (1 a N)                          │
│  ├─ Fan-in:         { B, C, D } → E  (N a 1)                          │
│  └─ Fan-out/Fan-in: A → { B, C, D } → E  (distribuir y recoger)      │
│                                                                          │
│  CONTROL DE FLUJO:                                                       │
│  ├─ Done channel:   close(done) como broadcast de cancelacion          │
│  ├─ Or-done:        envolver channel con cancelacion                   │
│  ├─ Tee:            duplicar stream para multiples consumers           │
│  └─ Bridge:         aplanar channel de channels                        │
│                                                                          │
│  LIMITACION DE RECURSOS:                                                 │
│  ├─ Semaforo:       buffered channel como contador de permisos         │
│  ├─ Rate limiter:   ticker como fuente de tokens                       │
│  └─ Worker pool:    N goroutines leyendo del mismo channel             │
│                                                                          │
│  CICLO DE VIDA:                                                          │
│  ├─ Generator:      funcion que retorna <-chan T                       │
│  ├─ Confinement:    cada goroutine con sus datos propios               │
│  └─ Error propagation: Result{Value, Err} a traves del pipeline       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

Para SysAdmin/DevOps, estos patrones aparecen constantemente:
- **Pipeline**: procesar logs (leer → parsear → filtrar → escribir)
- **Fan-out/Fan-in**: health check de 100 servidores en paralelo, recoger resultados
- **Worker pool**: pool de workers procesando jobs de una cola
- **Semaforo**: limitar conexiones concurrentes a una API o base de datos
- **Done channel**: shutdown graceful de microservicios
- Herramientas como kubectl, terraform, y prometheus usan estos patrones internamente

---

## 2. Pipeline

Un pipeline es una cadena de **etapas** conectadas por channels. Cada etapa es una funcion que:
1. Recibe datos de un channel de entrada (`<-chan T`)
2. Los transforma
3. Los envia a un channel de salida (`<-chan U`)

### 2.1 Anatomia de un pipeline

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    PIPELINE                                               │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────┐    ┌──────────┐    ┌──────────┐    ┌──────────┐         │
│  │ Source    │    │ Stage 1  │    │ Stage 2  │    │  Sink    │         │
│  │ (genera) │──►│(transforma│──►│(transforma│──►│(consume) │         │
│  │          │    │)          │    │)          │    │          │         │
│  └──────────┘    └──────────┘    └──────────┘    └──────────┘         │
│  ret: <-chan T   in: <-chan T     in: <-chan U     in: <-chan V         │
│                  ret: <-chan U    ret: <-chan V                          │
│                                                                          │
│  Cada etapa:                                                             │
│  ├─ Corre en su propia goroutine                                       │
│  ├─ Lee de su input channel (range)                                    │
│  ├─ Escribe a su output channel                                        │
│  ├─ Cierra su output channel cuando el input se cierra                │
│  └─ Toda la cadena se limpia automaticamente por propagacion de close │
│                                                                          │
│  BENEFICIOS:                                                             │
│  ├─ Cada etapa es independiente y testeable                            │
│  ├─ Composable: reordena, agrega, o quita etapas sin cambiar las otras│
│  ├─ Concurrencia gratis: todas las etapas ejecutan en paralelo        │
│  └─ Backpressure natural: si una etapa es lenta, la anterior espera   │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Implementacion basica

```go
package main

import (
    "fmt"
    "strings"
)

// Source: genera datos
func generate(items ...string) <-chan string {
    out := make(chan string)
    go func() {
        defer close(out)
        for _, item := range items {
            out <- item
        }
    }()
    return out
}

// Stage: transforma a mayusculas
func toUpper(in <-chan string) <-chan string {
    out := make(chan string)
    go func() {
        defer close(out)
        for s := range in {
            out <- strings.ToUpper(s)
        }
    }()
    return out
}

// Stage: agrega prefijo
func addPrefix(in <-chan string, prefix string) <-chan string {
    out := make(chan string)
    go func() {
        defer close(out)
        for s := range in {
            out <- prefix + s
        }
    }()
    return out
}

// Stage: filtra por longitud
func filterByLength(in <-chan string, minLen int) <-chan string {
    out := make(chan string)
    go func() {
        defer close(out)
        for s := range in {
            if len(s) >= minLen {
                out <- s
            }
        }
    }()
    return out
}

func main() {
    // Componer el pipeline
    // generate → toUpper → addPrefix → filterByLength → print
    
    source := generate("hello", "go", "concurrency", "is", "powerful", "hi")
    upper := toUpper(source)
    prefixed := addPrefix(upper, ">> ")
    long := filterByLength(prefixed, 8) // solo items con >= 8 chars
    
    for result := range long {
        fmt.Println(result)
    }
    // Output:
    // >> HELLO
    // >> CONCURRENCY
    // >> POWERFUL
}
```

### 2.3 Pipeline con cancelacion (done channel)

Sin cancelacion, si el consumer deja de leer, los producers se bloquean para siempre (goroutine leak). La solucion es un **done channel**:

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "time"
)

// Todas las etapas aceptan context para cancelacion
func generateCtx(ctx context.Context, items ...int) <-chan int {
    out := make(chan int)
    go func() {
        defer close(out)
        for _, item := range items {
            select {
            case out <- item:
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

func squareCtx(ctx context.Context, in <-chan int) <-chan int {
    out := make(chan int)
    go func() {
        defer close(out)
        for v := range in {
            select {
            case out <- v * v:
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

func filterEvenCtx(ctx context.Context, in <-chan int) <-chan int {
    out := make(chan int)
    go func() {
        defer close(out)
        for v := range in {
            if v%2 == 0 {
                select {
                case out <- v:
                case <-ctx.Done():
                    return
                }
            }
        }
    }()
    return out
}

func main() {
    ctx, cancel := context.WithCancel(context.Background())
    defer cancel() // cancelar todo el pipeline al salir
    
    // Pipeline: generate → square → filterEven → take 3
    source := generateCtx(ctx, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
    squared := squareCtx(ctx, source)
    even := filterEvenCtx(ctx, squared)
    
    // Solo queremos 3 resultados — luego cancelamos
    count := 0
    for v := range even {
        fmt.Println(v)
        count++
        if count >= 3 {
            cancel() // cancela todas las goroutines del pipeline
            break
        }
    }
    
    time.Sleep(50 * time.Millisecond) // dar tiempo al cleanup
    fmt.Printf("Obtenidos %d resultados, pipeline cancelado limpiamente\n", count)
    _ = rand.Int() // usar rand para que el import no se queje
}
```

### 2.4 Pipeline generico (Go 1.18+)

```go
package main

import (
    "context"
    "fmt"
)

// Stage generico: aplica una funcion a cada elemento
func stage[In, Out any](ctx context.Context, in <-chan In, fn func(In) Out) <-chan Out {
    out := make(chan Out)
    go func() {
        defer close(out)
        for v := range in {
            select {
            case out <- fn(v):
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

// Filter generico: solo pasa elementos que cumplen el predicado
func filter[T any](ctx context.Context, in <-chan T, predicate func(T) bool) <-chan T {
    out := make(chan T)
    go func() {
        defer close(out)
        for v := range in {
            if predicate(v) {
                select {
                case out <- v:
                case <-ctx.Done():
                    return
                }
            }
        }
    }()
    return out
}

// Take: solo los primeros N elementos
func take[T any](ctx context.Context, in <-chan T, n int) <-chan T {
    out := make(chan T)
    go func() {
        defer close(out)
        count := 0
        for v := range in {
            if count >= n {
                return
            }
            select {
            case out <- v:
                count++
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

// Repeat: genera un valor infinitamente
func repeat[T any](ctx context.Context, values ...T) <-chan T {
    out := make(chan T)
    go func() {
        defer close(out)
        for {
            for _, v := range values {
                select {
                case out <- v:
                case <-ctx.Done():
                    return
                }
            }
        }
    }()
    return out
}

func main() {
    ctx, cancel := context.WithCancel(context.Background())
    defer cancel()
    
    // Pipeline generico:
    // repeat(1,2,3...) → take(10) → stage(x*x) → filter(>5) → print
    
    nums := repeat(ctx, 1, 2, 3, 4, 5)
    first10 := take(ctx, nums, 10)
    squared := stage(ctx, first10, func(n int) int { return n * n })
    big := filter(ctx, squared, func(n int) bool { return n > 5 })
    
    for v := range big {
        fmt.Printf("%d ", v)
    }
    fmt.Println()
    // Output: 9 16 25 9 16 25
}
```

---

## 3. Fan-out

Fan-out distribuye trabajo de **un channel a multiples goroutines**. Cada goroutine lee del mismo channel — Go garantiza que cada valor se entrega a exactamente un receiver.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    FAN-OUT                                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│                      ┌──────────┐                                       │
│                 ┌───►│ Worker 1 │                                       │
│                 │    └──────────┘                                       │
│  ┌──────────┐  │    ┌──────────┐                                       │
│  │  Source   │──┼───►│ Worker 2 │                                       │
│  └──────────┘  │    └──────────┘                                       │
│                 │    ┌──────────┐                                       │
│                 └───►│ Worker 3 │                                       │
│                      └──────────┘                                       │
│                                                                          │
│  Todos los workers leen del MISMO channel.                              │
│  Go distribuye: cada valor va a UN solo worker.                        │
│                                                                          │
│  CUANDO USAR:                                                            │
│  ├─ CPU-bound: distribuir computo pesado entre N workers               │
│  ├─ I/O-bound: distribuir requests HTTP, queries DB                    │
│  └─ Rate-limited: N workers = N max operaciones concurrentes           │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.1 Fan-out simple (worker pool)

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

type Job struct {
    ID   int
    Data string
}

type Result struct {
    JobID    int
    Worker   int
    Output   string
    Duration time.Duration
}

func worker(id int, jobs <-chan Job, results chan<- Result) {
    for job := range jobs {
        start := time.Now()
        // Simular procesamiento
        time.Sleep(time.Duration(50+len(job.Data)*10) * time.Millisecond)
        results <- Result{
            JobID:    job.ID,
            Worker:   id,
            Output:   fmt.Sprintf("processed(%s)", job.Data),
            Duration: time.Since(start),
        }
    }
}

func main() {
    const numWorkers = 3
    
    jobs := make(chan Job, 10)
    results := make(chan Result, 10)
    
    // Fan-out: N workers leyendo del mismo channel
    var wg sync.WaitGroup
    for i := 1; i <= numWorkers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            worker(id, jobs, results)
        }(i)
    }
    
    // Cerrar results cuando todos los workers terminen
    go func() {
        wg.Wait()
        close(results)
    }()
    
    // Enviar jobs
    go func() {
        items := []string{"alpha", "bravo", "charlie", "delta", "echo",
            "foxtrot", "golf", "hotel", "india"}
        for i, item := range items {
            jobs <- Job{ID: i + 1, Data: item}
        }
        close(jobs) // cerrar jobs — workers terminaran con range
    }()
    
    // Recoger resultados
    for r := range results {
        fmt.Printf("  Job #%d → Worker %d: %s (%v)\n",
            r.JobID, r.Worker, r.Output, r.Duration.Round(time.Millisecond))
    }
}
```

### 3.2 Fan-out con cancelacion

```go
package main

import (
    "context"
    "fmt"
    "sync"
    "time"
)

func workerCtx(ctx context.Context, id int, jobs <-chan int, results chan<- string) {
    for {
        select {
        case job, ok := <-jobs:
            if !ok {
                return // channel cerrado
            }
            // Simular trabajo con respeto a cancelacion
            select {
            case <-time.After(time.Duration(job*50) * time.Millisecond):
                select {
                case results <- fmt.Sprintf("Worker %d: job %d done", id, job):
                case <-ctx.Done():
                    return
                }
            case <-ctx.Done():
                fmt.Printf("  Worker %d: cancelado en job %d\n", id, job)
                return
            }
        case <-ctx.Done():
            return
        }
    }
}

func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 500*time.Millisecond)
    defer cancel()
    
    jobs := make(chan int, 20)
    results := make(chan string, 20)
    
    // Fan-out: 3 workers
    var wg sync.WaitGroup
    for i := 1; i <= 3; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            workerCtx(ctx, id, jobs, results)
        }(i)
    }
    
    go func() {
        wg.Wait()
        close(results)
    }()
    
    // Enviar muchos jobs (mas de los que podran completarse en 500ms)
    go func() {
        for i := 1; i <= 20; i++ {
            select {
            case jobs <- i:
            case <-ctx.Done():
                close(jobs)
                return
            }
        }
        close(jobs)
    }()
    
    for r := range results {
        fmt.Printf("  %s\n", r)
    }
    fmt.Println("Pipeline terminado (timeout o completado)")
}
```

---

## 4. Fan-in

Fan-in combina **multiples channels de entrada en un solo channel de salida**. Es el complemento natural de fan-out.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    FAN-IN                                                 │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ┌──────────┐                                                           │
│  │ Source 1  │───┐                                                      │
│  └──────────┘   │    ┌──────────┐    ┌──────────┐                      │
│  ┌──────────┐   ├───►│  Fan-in  │───►│  Sink    │                      │
│  │ Source 2  │───┤    │ (merge)  │    │          │                      │
│  └──────────┘   │    └──────────┘    └──────────┘                      │
│  ┌──────────┐   │                                                       │
│  │ Source 3  │───┘                                                      │
│  └──────────┘                                                           │
│                                                                          │
│  Multiples producers → un solo consumer.                                │
│  Cada valor de cualquier source se pasa al output.                     │
│  El output se cierra cuando TODAS las sources se cierran.              │
│                                                                          │
│  IMPLEMENTACIONES:                                                       │
│  ├─ goroutine-per-channel: un goroutine por cada input (simple)        │
│  ├─ reflect.Select: select dinamico sin goroutines extra (eficiente)   │
│  └─ recursive: dividir en pares y merge recursivamente                 │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.1 Fan-in con goroutine por channel

```go
package main

import (
    "context"
    "fmt"
    "sync"
    "time"
)

func fanIn[T any](ctx context.Context, channels ...<-chan T) <-chan T {
    out := make(chan T)
    var wg sync.WaitGroup
    
    // Un goroutine por channel de entrada
    for _, ch := range channels {
        wg.Add(1)
        go func(c <-chan T) {
            defer wg.Done()
            for v := range c {
                select {
                case out <- v:
                case <-ctx.Done():
                    return
                }
            }
        }(ch)
    }
    
    // Cerrar output cuando todos los inputs se cierren
    go func() {
        wg.Wait()
        close(out)
    }()
    
    return out
}

func source(ctx context.Context, name string, interval time.Duration, count int) <-chan string {
    out := make(chan string)
    go func() {
        defer close(out)
        for i := 1; i <= count; i++ {
            select {
            case <-time.After(interval):
                select {
                case out <- fmt.Sprintf("[%s] item-%d", name, i):
                case <-ctx.Done():
                    return
                }
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

func main() {
    ctx, cancel := context.WithCancel(context.Background())
    defer cancel()
    
    // 3 sources con diferentes velocidades
    s1 := source(ctx, "fast", 50*time.Millisecond, 5)
    s2 := source(ctx, "medium", 100*time.Millisecond, 5)
    s3 := source(ctx, "slow", 200*time.Millisecond, 5)
    
    // Fan-in: 3 → 1
    merged := fanIn(ctx, s1, s2, s3)
    
    for msg := range merged {
        fmt.Printf("  %s\n", msg)
    }
    fmt.Println("Todos los sources terminaron")
}
```

### 4.2 Fan-in con reflect.Select (dinamico)

Cuando tienes muchos channels, un goroutine por cada uno puede ser ineficiente. `reflect.Select` permite hacer select sobre un slice dinamico de channels:

```go
package main

import (
    "fmt"
    "reflect"
    "time"
)

// fanInReflect merge N channels sin crear N goroutines extra
func fanInReflect[T any](channels ...<-chan T) <-chan T {
    out := make(chan T)
    
    go func() {
        defer close(out)
        
        // Construir cases para reflect.Select
        cases := make([]reflect.SelectCase, len(channels))
        for i, ch := range channels {
            cases[i] = reflect.SelectCase{
                Dir:  reflect.SelectRecv,
                Chan: reflect.ValueOf(ch),
            }
        }
        
        // Loop hasta que todos los channels se cierren
        for len(cases) > 0 {
            // reflect.Select elige un case aleatorio (como select nativo)
            chosen, value, ok := reflect.Select(cases)
            if !ok {
                // Channel cerrado — remover del slice
                cases = append(cases[:chosen], cases[chosen+1:]...)
                continue
            }
            out <- value.Interface().(T)
        }
    }()
    
    return out
}

func gen(name string, count int, delay time.Duration) <-chan string {
    ch := make(chan string)
    go func() {
        defer close(ch)
        for i := 1; i <= count; i++ {
            time.Sleep(delay)
            ch <- fmt.Sprintf("[%s] %d", name, i)
        }
    }()
    return ch
}

func main() {
    // 5 sources — solo 1 goroutine extra para merge (en lugar de 5)
    merged := fanInReflect(
        gen("A", 3, 80*time.Millisecond),
        gen("B", 3, 100*time.Millisecond),
        gen("C", 3, 120*time.Millisecond),
        gen("D", 3, 140*time.Millisecond),
        gen("E", 3, 160*time.Millisecond),
    )
    
    for msg := range merged {
        fmt.Printf("  %s\n", msg)
    }
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    FAN-IN: GOROUTINE vs REFLECT                         │
├──────────────────┬───────────────────────┬───────────────────────────────┤
│ Aspecto           │ Goroutine per channel │ reflect.Select              │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Goroutines extra  │ N (una por channel)   │ 1 (una sola)               │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Performance       │ Buena (N pequeno)     │ Mejor (N grande)            │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Tipado            │ Type-safe             │ Usa reflection (cast)       │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Complejidad       │ Simple                │ Mas complejo                 │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Uso recomendado   │ < 100 channels        │ > 100 channels o dinamico  │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Cancelacion       │ Natural (ctx.Done)    │ Agregar case extra          │
└──────────────────┴───────────────────────┴───────────────────────────────┘
```

---

## 5. Fan-out/Fan-in Combinado

El patron mas util: distribuir trabajo entre N workers y recoger todos los resultados.

### 5.1 Implementacion completa

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "sync"
    "time"
)

type Task struct {
    ID   int
    Host string
}

type HealthResult struct {
    Task    Task
    Healthy bool
    Latency time.Duration
    Worker  int
}

// Source: genera tareas
func generateTasks(ctx context.Context, hosts []string) <-chan Task {
    out := make(chan Task)
    go func() {
        defer close(out)
        for i, host := range hosts {
            select {
            case out <- Task{ID: i + 1, Host: host}:
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

// Fan-out/Fan-in: distribuir tareas entre N workers
func fanOutFanIn(ctx context.Context, tasks <-chan Task, numWorkers int) <-chan HealthResult {
    results := make(chan HealthResult)
    var wg sync.WaitGroup
    
    // Fan-out: crear N workers que leen del mismo channel
    for i := 1; i <= numWorkers; i++ {
        wg.Add(1)
        go func(workerID int) {
            defer wg.Done()
            for task := range tasks {
                // Verificar cancelacion antes de trabajo pesado
                select {
                case <-ctx.Done():
                    return
                default:
                }
                
                // Simular health check
                start := time.Now()
                latency := time.Duration(20+rand.Intn(180)) * time.Millisecond
                time.Sleep(latency)
                healthy := rand.Intn(10) > 1 // 90% healthy
                
                select {
                case results <- HealthResult{
                    Task:    task,
                    Healthy: healthy,
                    Latency: time.Since(start),
                    Worker:  workerID,
                }:
                case <-ctx.Done():
                    return
                }
            }
        }(i)
    }
    
    // Fan-in: cerrar results cuando todos los workers terminen
    go func() {
        wg.Wait()
        close(results)
    }()
    
    return results
}

func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 5*time.Second)
    defer cancel()
    
    hosts := []string{
        "web-01", "web-02", "web-03",
        "api-01", "api-02",
        "db-primary", "db-replica-01", "db-replica-02",
        "cache-01", "cache-02",
        "queue-01",
        "monitor-01",
    }
    
    fmt.Printf("Checking %d hosts with 4 workers...\n\n", len(hosts))
    
    tasks := generateTasks(ctx, hosts)
    results := fanOutFanIn(ctx, tasks, 4)
    
    healthy, unhealthy := 0, 0
    for r := range results {
        status := "HEALTHY"
        if !r.Healthy {
            status = "UNHEALTHY"
            unhealthy++
        } else {
            healthy++
        }
        fmt.Printf("  [Worker %d] %-15s → %-9s (%v)\n",
            r.Worker, r.Task.Host, status, r.Latency.Round(time.Millisecond))
    }
    
    fmt.Printf("\nResults: %d healthy, %d unhealthy, %d total\n",
        healthy, unhealthy, healthy+unhealthy)
}
```

### 5.2 Fan-out/Fan-in con orden preservado

A veces necesitas que los resultados salgan en el mismo orden que los inputs. Esto requiere un paso extra de reordenamiento:

```go
package main

import (
    "context"
    "fmt"
    "sort"
    "sync"
    "time"
)

type OrderedTask struct {
    Index int // posicion original
    Value string
}

type OrderedResult struct {
    Index  int
    Value  string
    Output string
}

func orderedFanOutFanIn(ctx context.Context, items []string, numWorkers int) []string {
    tasks := make(chan OrderedTask, len(items))
    results := make(chan OrderedResult, len(items))
    
    // Enviar todas las tareas con su indice
    for i, item := range items {
        tasks <- OrderedTask{Index: i, Value: item}
    }
    close(tasks)
    
    // Fan-out: N workers
    var wg sync.WaitGroup
    for i := 0; i < numWorkers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for task := range tasks {
                // Simular procesamiento con duracion variable
                time.Sleep(time.Duration(10+len(task.Value)*5) * time.Millisecond)
                select {
                case results <- OrderedResult{
                    Index:  task.Index,
                    Value:  task.Value,
                    Output: fmt.Sprintf("processed(%s)", task.Value),
                }:
                case <-ctx.Done():
                    return
                }
            }
        }()
    }
    
    // Cerrar results cuando todos los workers terminen
    go func() {
        wg.Wait()
        close(results)
    }()
    
    // Recoger todos los resultados
    collected := make([]OrderedResult, 0, len(items))
    for r := range results {
        collected = append(collected, r)
    }
    
    // Reordenar por indice original
    sort.Slice(collected, func(i, j int) bool {
        return collected[i].Index < collected[j].Index
    })
    
    // Extraer outputs en orden
    output := make([]string, len(collected))
    for i, r := range collected {
        output[i] = r.Output
    }
    return output
}

func main() {
    ctx := context.Background()
    items := []string{"alpha", "bravo", "charlie", "delta", "echo", "foxtrot"}
    
    results := orderedFanOutFanIn(ctx, items, 3)
    
    for i, r := range results {
        fmt.Printf("  [%d] %s\n", i, r)
    }
    // Output siempre en orden:
    // [0] processed(alpha)
    // [1] processed(bravo)
    // [2] processed(charlie)
    // [3] processed(delta)
    // [4] processed(echo)
    // [5] processed(foxtrot)
}
```

---

## 6. Done Channel

El done channel es un patron de **cancelacion broadcast**: cerrar un channel notifica a todas las goroutines que lo escuchan simultaneamente.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    DONE CHANNEL                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  close(done) es un BROADCAST:                                           │
│                                                                          │
│  ┌──────────┐                                                           │
│  │  Main    │── close(done) ──►  Goroutine 1: <-done  (desbloquea)    │
│  │          │                    Goroutine 2: <-done  (desbloquea)     │
│  │          │                    Goroutine 3: <-done  (desbloquea)     │
│  └──────────┘                    ...                                    │
│                                  Goroutine N: <-done  (desbloquea)     │
│                                                                          │
│  Un channel cerrado:                                                    │
│  ├─ SIEMPRE retorna inmediatamente en receive (<-done)                 │
│  ├─ Retorna el zero value del tipo (struct{}{} para chan struct{})     │
│  ├─ Funciona en select como un case siempre listo                     │
│  └─ Notifica a TODAS las goroutines simultaneamente                    │
│                                                                          │
│  POR QUE chan struct{} y no chan bool:                                  │
│  ├─ struct{} tiene tamano 0 — no ocupa memoria                        │
│  ├─ Expresa intencion: "esta es una senal, no un dato"                │
│  └─ No hay ambiguedad: close() es la unica operacion con sentido     │
│                                                                          │
│  NOTA: En Go moderno, context.Context reemplaza done channel          │
│  en la mayoria de los casos. Pero done channel sigue siendo util      │
│  cuando no necesitas la funcionalidad extra de context (valores,       │
│  deadline, causa de cancelacion).                                      │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 6.1 Done channel basico

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

func worker(id int, done <-chan struct{}, wg *sync.WaitGroup) {
    defer wg.Done()
    for {
        select {
        case <-done:
            fmt.Printf("  Worker %d: shutdown signal recibida\n", id)
            return
        default:
            fmt.Printf("  Worker %d: trabajando...\n", id)
            time.Sleep(200 * time.Millisecond)
        }
    }
}

func main() {
    done := make(chan struct{})
    var wg sync.WaitGroup
    
    for i := 1; i <= 3; i++ {
        wg.Add(1)
        go worker(i, done, &wg)
    }
    
    time.Sleep(600 * time.Millisecond)
    
    fmt.Println("\n--- Cerrando done channel ---")
    close(done) // broadcast a TODOS los workers
    
    wg.Wait()
    fmt.Println("Todos los workers terminaron")
}
```

### 6.2 Or-done channel

Un wrapper que permite leer de un channel con soporte de cancelacion. Sin or-done, cada receive necesita su propio select:

```go
package main

import (
    "context"
    "fmt"
    "time"
)

// orDone envuelve un channel con cancelacion
// Sin orDone, cada receive necesita:
//   select {
//   case v, ok := <-ch:
//   case <-ctx.Done():
//   }
// Con orDone, puedes hacer: for v := range orDone(ctx, ch) { }
func orDone[T any](ctx context.Context, in <-chan T) <-chan T {
    out := make(chan T)
    go func() {
        defer close(out)
        for {
            select {
            case <-ctx.Done():
                return
            case v, ok := <-in:
                if !ok {
                    return
                }
                select {
                case out <- v:
                case <-ctx.Done():
                    return
                }
            }
        }
    }()
    return out
}

func slowSource() <-chan int {
    ch := make(chan int)
    go func() {
        defer close(ch)
        for i := 1; i <= 100; i++ {
            time.Sleep(100 * time.Millisecond)
            ch <- i
        }
    }()
    return ch
}

func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 500*time.Millisecond)
    defer cancel()
    
    source := slowSource()
    
    // Sin orDone, necesitarias select en cada iteracion
    // Con orDone, puedes usar range limpiamente
    for v := range orDone(ctx, source) {
        fmt.Printf("  %d\n", v)
    }
    fmt.Println("Terminado (timeout o source cerrada)")
}
```

---

## 7. Semaforo con Channels

Un channel buffered puede actuar como **semaforo**: el buffer limita cuantas goroutines pueden estar "activas" simultaneamente.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    SEMAFORO CON BUFFERED CHANNEL                        │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  sem := make(chan struct{}, N)  // semaforo con N permisos              │
│                                                                          │
│  // Adquirir permiso (bloquea si N ya estan activos)                   │
│  sem <- struct{}{}                                                      │
│                                                                          │
│  // Liberar permiso                                                     │
│  <-sem                                                                   │
│                                                                          │
│  MECANISMO:                                                              │
│  ├─ Buffer de N → hasta N goroutines pueden "enviar" sin bloquear     │
│  ├─ La goroutine N+1 se bloquea en send (buffer lleno)                │
│  ├─ Cuando una goroutine libera (<-sem), se abre un espacio           │
│  └─ La goroutine bloqueada puede continuar                             │
│                                                                          │
│  Ejemplo con N=3:                                                        │
│                                                                          │
│  [G1 activa] [G2 activa] [G3 activa]                                  │
│  sem: ┌───┬───┬───┐                                                   │
│       │ ■ │ ■ │ ■ │  (lleno — G4 se bloquea en sem <- struct{}{})    │
│       └───┴───┴───┘                                                    │
│                                                                          │
│  G1 termina y hace <-sem:                                               │
│  sem: ┌───┬───┬───┐                                                   │
│       │   │ ■ │ ■ │  (espacio — G4 puede continuar)                   │
│       └───┴───┴───┘                                                    │
│                                                                          │
│  COMPARACION:                                                            │
│  ├─ Channel semaphore: simple, idiomatico Go                           │
│  ├─ golang.org/x/sync/semaphore: Weighted (fracciones de permiso),    │
│  │  TryAcquire (non-blocking), context integration                     │
│  └─ errgroup.SetLimit: semaforo integrado con error propagation       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 7.1 Semaforo basico

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

func main() {
    const maxConcurrency = 3
    sem := make(chan struct{}, maxConcurrency)
    
    var wg sync.WaitGroup
    
    for i := 1; i <= 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            
            // Adquirir permiso
            sem <- struct{}{}
            
            // Seccion "concurrency-limited"
            fmt.Printf("  [START] Worker %d (active: %d/%d)\n", id, len(sem), maxConcurrency)
            time.Sleep(300 * time.Millisecond) // simular trabajo
            fmt.Printf("  [DONE]  Worker %d\n", id)
            
            // Liberar permiso
            <-sem
        }(i)
    }
    
    wg.Wait()
    fmt.Println("Todos los workers terminaron")
    // En cualquier momento, maximo 3 workers imprimen START antes de su DONE
}
```

### 7.2 Semaforo con timeout

```go
package main

import (
    "context"
    "fmt"
    "time"
)

type Semaphore chan struct{}

func NewSemaphore(n int) Semaphore {
    return make(Semaphore, n)
}

func (s Semaphore) Acquire(ctx context.Context) error {
    select {
    case s <- struct{}{}:
        return nil
    case <-ctx.Done():
        return ctx.Err()
    }
}

func (s Semaphore) TryAcquire() bool {
    select {
    case s <- struct{}{}:
        return true
    default:
        return false
    }
}

func (s Semaphore) Release() {
    <-s
}

func main() {
    sem := NewSemaphore(2) // solo 2 concurrentes
    
    for i := 1; i <= 5; i++ {
        ctx, cancel := context.WithTimeout(context.Background(), 200*time.Millisecond)
        
        go func(id int) {
            defer cancel()
            
            if err := sem.Acquire(ctx); err != nil {
                fmt.Printf("  Worker %d: no pudo adquirir semaforo (%v)\n", id, err)
                return
            }
            defer sem.Release()
            
            fmt.Printf("  Worker %d: trabajando...\n", id)
            time.Sleep(500 * time.Millisecond)
            fmt.Printf("  Worker %d: terminado\n", id)
        }(i)
    }
    
    time.Sleep(2 * time.Second) // esperar a que todos terminen o fallen
}
```

### 7.3 Weighted semaphore (limitando recursos, no goroutines)

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

// WeightedSemaphore limita basado en "peso" (ej: bytes, conexiones)
type WeightedSemaphore struct {
    slots chan struct{}
}

func NewWeightedSemaphore(maxWeight int) *WeightedSemaphore {
    return &WeightedSemaphore{slots: make(chan struct{}, maxWeight)}
}

func (ws *WeightedSemaphore) Acquire(weight int) {
    for i := 0; i < weight; i++ {
        ws.slots <- struct{}{} // adquirir N slots
    }
}

func (ws *WeightedSemaphore) Release(weight int) {
    for i := 0; i < weight; i++ {
        <-ws.slots // liberar N slots
    }
}

func main() {
    // Simular un pool de conexiones con peso variable
    // Total: 10 unidades. Query simple=1, query compleja=3, backup=5
    sem := NewWeightedSemaphore(10)
    
    type Job struct {
        Name   string
        Weight int
    }
    
    jobs := []Job{
        {"simple-query-1", 1},
        {"simple-query-2", 1},
        {"complex-query-1", 3},
        {"backup-job", 5},
        {"simple-query-3", 1},
        {"complex-query-2", 3},
        {"simple-query-4", 1},
    }
    
    var wg sync.WaitGroup
    for _, job := range jobs {
        wg.Add(1)
        go func(j Job) {
            defer wg.Done()
            
            fmt.Printf("  [WAIT] %s (weight=%d)\n", j.Name, j.Weight)
            sem.Acquire(j.Weight)
            
            fmt.Printf("  [RUN]  %s (weight=%d)\n", j.Name, j.Weight)
            time.Sleep(time.Duration(j.Weight*100) * time.Millisecond)
            
            sem.Release(j.Weight)
            fmt.Printf("  [DONE] %s\n", j.Name)
        }(job)
    }
    
    wg.Wait()
}
```

---

## 8. Tee Channel

Como el comando `tee` de Unix, duplica un stream para que multiples consumers lo procesen independientemente.

```go
package main

import (
    "context"
    "fmt"
    "sync"
    "time"
)

// tee duplica un channel en dos
func tee[T any](ctx context.Context, in <-chan T) (<-chan T, <-chan T) {
    out1 := make(chan T)
    out2 := make(chan T)
    
    go func() {
        defer close(out1)
        defer close(out2)
        
        for v := range in {
            // Necesitamos enviar a ambos. Usamos variables locales
            // para manejar el caso donde uno es mas lento que el otro
            o1, o2 := out1, out2
            for o1 != nil || o2 != nil {
                select {
                case o1 <- v:
                    o1 = nil // ya enviado a out1
                case o2 <- v:
                    o2 = nil // ya enviado a out2
                case <-ctx.Done():
                    return
                }
            }
        }
    }()
    
    return out1, out2
}

func main() {
    ctx, cancel := context.WithCancel(context.Background())
    defer cancel()
    
    // Source
    source := make(chan string)
    go func() {
        defer close(source)
        items := []string{"event-1", "event-2", "event-3", "event-4", "event-5"}
        for _, item := range items {
            source <- item
        }
    }()
    
    // Tee: duplicar stream
    toLogger, toMetrics := tee(ctx, source)
    
    var wg sync.WaitGroup
    
    // Consumer 1: logger
    wg.Add(1)
    go func() {
        defer wg.Done()
        for msg := range toLogger {
            fmt.Printf("  [LOG]    %s\n", msg)
        }
    }()
    
    // Consumer 2: metrics
    wg.Add(1)
    go func() {
        defer wg.Done()
        count := 0
        for range toMetrics {
            count++
        }
        fmt.Printf("  [METRIC] Total events: %d\n", count)
    }()
    
    wg.Wait()
    _ = time.Now() // silenciar import
}
```

---

## 9. Bridge Channel

Bridge aplana un `<-chan <-chan T` (channel de channels) en un solo `<-chan T`. Util cuando un generator produce channels en secuencia.

```go
package main

import (
    "context"
    "fmt"
)

// bridge aplana un channel de channels en un solo channel
func bridge[T any](ctx context.Context, chanStream <-chan <-chan T) <-chan T {
    out := make(chan T)
    
    go func() {
        defer close(out)
        
        for {
            // Obtener el siguiente channel del stream
            var stream <-chan T
            select {
            case maybeStream, ok := <-chanStream:
                if !ok {
                    return // no hay mas channels
                }
                stream = maybeStream
            case <-ctx.Done():
                return
            }
            
            // Drenar el channel actual
            for v := range orDone(ctx, stream) {
                select {
                case out <- v:
                case <-ctx.Done():
                    return
                }
            }
        }
    }()
    
    return out
}

// orDone helper (del patron anterior)
func orDone[T any](ctx context.Context, in <-chan T) <-chan T {
    out := make(chan T)
    go func() {
        defer close(out)
        for {
            select {
            case v, ok := <-in:
                if !ok {
                    return
                }
                select {
                case out <- v:
                case <-ctx.Done():
                    return
                }
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

// genPages simula un API paginada que retorna channels secuenciales
func genPages(ctx context.Context) <-chan <-chan string {
    chanStream := make(chan (<-chan string))
    go func() {
        defer close(chanStream)
        
        pages := [][]string{
            {"page1-item1", "page1-item2", "page1-item3"},
            {"page2-item1", "page2-item2"},
            {"page3-item1", "page3-item2", "page3-item3", "page3-item4"},
        }
        
        for _, page := range pages {
            ch := make(chan string)
            select {
            case chanStream <- ch:
            case <-ctx.Done():
                return
            }
            
            go func(items []string) {
                defer close(ch)
                for _, item := range items {
                    select {
                    case ch <- item:
                    case <-ctx.Done():
                        return
                    }
                }
            }(page)
        }
    }()
    return chanStream
}

func main() {
    ctx, cancel := context.WithCancel(context.Background())
    defer cancel()
    
    // Sin bridge: necesitarias un loop anidado
    // Con bridge: un solo range
    for item := range bridge(ctx, genPages(ctx)) {
        fmt.Printf("  %s\n", item)
    }
    // Output:
    // page1-item1
    // page1-item2
    // page1-item3
    // page2-item1
    // page2-item2
    // page3-item1
    // page3-item2
    // page3-item3
    // page3-item4
}
```

---

## 10. Confinement (confinamiento)

Confinement asegura que **los datos solo son accesibles por una goroutine a la vez**, eliminando la necesidad de sincronizacion. Es la solucion mas segura y eficiente: si no compartes memoria, no hay data races.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    TIPOS DE CONFINEMENT                                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  LEXICAL CONFINEMENT:                                                    │
│  Los datos se crean y usan dentro del scope de una sola goroutine.     │
│  El compilador puede verificar esto en muchos casos.                   │
│                                                                          │
│  func process() <-chan int {                                            │
│      results := make([]int, 0) // solo esta goroutine lo ve           │
│      // ... poblar results ...                                          │
│      ch := make(chan int)                                               │
│      go func() {                                                         │
│          for _, r := range results {                                    │
│              ch <- r                                                    │
│          }                                                               │
│          close(ch)                                                       │
│      }()                                                                 │
│      return ch  // los datos "salen" solo por el channel               │
│  }                                                                       │
│                                                                          │
│  AD-HOC CONFINEMENT:                                                     │
│  Por convencion/diseno, solo una goroutine accede a los datos.         │
│  No hay enforcement del compilador — requiere disciplina.              │
│                                                                          │
│  CHANNEL CONFINEMENT:                                                    │
│  Los datos se pasan entre goroutines por channels.                     │
│  El receptor "posee" los datos hasta que los envie a otro.             │
│                                                                          │
│  G1 ──send──► ch ──recv──► G2 ──send──► ch2 ──recv──► G3             │
│  (G1 ya no     (G2 posee     (G2 ya no     (G3 posee                  │
│   posee)        ahora)        posee)        ahora)                     │
│                                                                          │
│  REGLA: "Un dato tiene un solo dueno en cada momento."                 │
│  Transfiere ownership a traves de channels.                            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 10.1 Confinement por indice (sin sharing)

```go
package main

import (
    "fmt"
    "sync"
)

func main() {
    data := make([]int, 10)
    
    var wg sync.WaitGroup
    
    // Cada goroutine trabaja en SU indice — sin sharing, sin locks
    for i := 0; i < len(data); i++ {
        wg.Add(1)
        go func(idx int) {
            defer wg.Done()
            data[idx] = idx * idx // cada goroutine escribe a su propio indice
        }(i)
    }
    
    wg.Wait()
    fmt.Println(data) // [0 1 4 9 16 25 36 49 64 81]
    
    // Esto es safe porque:
    // - Cada goroutine solo escribe a data[idx]
    // - Ningun otra goroutine lee o escribe en esa posicion
    // - Las goroutines no leen posiciones de otras goroutines
    // - El slice header no se modifica (no append, no reslice)
}
```

### 10.2 Confinement por ownership (transferencia via channel)

```go
package main

import (
    "fmt"
    "sync"
)

type Document struct {
    ID      int
    Content string
    Version int
}

func main() {
    // Pipeline donde cada etapa "posee" el documento
    // Nunca hay dos goroutines modificando el mismo documento
    
    source := make(chan *Document)
    enriched := make(chan *Document)
    validated := make(chan *Document)
    
    var wg sync.WaitGroup
    
    // Stage 1: Enricher — posee el documento mientras lo enriquece
    wg.Add(1)
    go func() {
        defer wg.Done()
        defer close(enriched)
        for doc := range source {
            // Solo esta goroutine modifica doc ahora
            doc.Content = fmt.Sprintf("[enriched] %s", doc.Content)
            doc.Version++
            enriched <- doc // transfiere ownership
            // A partir de aqui, NO debemos tocar doc
        }
    }()
    
    // Stage 2: Validator — posee el documento mientras lo valida
    wg.Add(1)
    go func() {
        defer wg.Done()
        defer close(validated)
        for doc := range enriched {
            // Solo esta goroutine modifica doc ahora
            if len(doc.Content) > 10 {
                doc.Content = fmt.Sprintf("[valid] %s", doc.Content)
                doc.Version++
                validated <- doc
            }
        }
    }()
    
    // Producer: genera documentos
    go func() {
        docs := []string{"report", "invoice", "summary"}
        for i, d := range docs {
            source <- &Document{ID: i + 1, Content: d, Version: 1}
        }
        close(source)
    }()
    
    // Consumer: imprime resultados
    for doc := range validated {
        fmt.Printf("  Doc %d: %s (v%d)\n", doc.ID, doc.Content, doc.Version)
    }
    
    wg.Wait()
}
```

---

## 11. Error Propagation en Pipelines

### 11.1 Result type

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "math/rand"
    "time"
)

// Result encapsula valor o error — como Result<T,E> de Rust
type Result[T any] struct {
    Value T
    Err   error
}

// mapStage aplica una funcion que puede fallar
func mapStage[In, Out any](
    ctx context.Context,
    in <-chan Result[In],
    fn func(In) (Out, error),
) <-chan Result[Out] {
    out := make(chan Result[Out])
    go func() {
        defer close(out)
        for r := range in {
            // Propagar errores sin procesarlos
            if r.Err != nil {
                select {
                case out <- Result[Out]{Err: r.Err}:
                case <-ctx.Done():
                    return
                }
                continue
            }
            
            // Procesar valores
            val, err := fn(r.Value)
            select {
            case out <- Result[Out]{Value: val, Err: err}:
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

// sourceStage genera Results a partir de un slice
func sourceStage[T any](ctx context.Context, items ...T) <-chan Result[T] {
    out := make(chan Result[T])
    go func() {
        defer close(out)
        for _, item := range items {
            select {
            case out <- Result[T]{Value: item}:
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

func main() {
    ctx := context.Background()
    
    // Pipeline con error propagation:
    // source → validate → transform → collect
    
    source := sourceStage(ctx, "web-01", "web-02", "", "db-01", "invalid!", "cache-01")
    
    validated := mapStage(ctx, source, func(host string) (string, error) {
        if host == "" {
            return "", errors.New("empty hostname")
        }
        if len(host) > 10 {
            return "", fmt.Errorf("hostname too long: %s", host)
        }
        return host, nil
    })
    
    checked := mapStage(ctx, validated, func(host string) (string, error) {
        // Simular health check
        time.Sleep(50 * time.Millisecond)
        if rand.Intn(5) == 0 {
            return "", fmt.Errorf("health check failed: %s", host)
        }
        return fmt.Sprintf("%s:healthy", host), nil
    })
    
    // Collect results
    for r := range checked {
        if r.Err != nil {
            fmt.Printf("  ERROR: %v\n", r.Err)
        } else {
            fmt.Printf("  OK:    %s\n", r.Value)
        }
    }
}
```

### 11.2 Error collection con errgroup

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "time"

    "golang.org/x/sync/errgroup"
)

func main() {
    ctx := context.Background()
    g, ctx := errgroup.WithContext(ctx)
    g.SetLimit(5) // max 5 concurrentes
    
    hosts := []string{
        "web-01", "web-02", "web-03",
        "api-01", "api-02", "api-03",
        "db-01", "db-02",
        "cache-01", "cache-02",
    }
    
    results := make(chan string, len(hosts))
    
    for _, host := range hosts {
        g.Go(func() error {
            // Simular health check
            time.Sleep(time.Duration(50+rand.Intn(200)) * time.Millisecond)
            
            if rand.Intn(10) == 0 {
                return fmt.Errorf("host %s: connection refused", host)
            }
            
            select {
            case results <- fmt.Sprintf("%s: healthy", host):
                return nil
            case <-ctx.Done():
                return ctx.Err()
            }
        })
    }
    
    // Esperar y recoger el primer error (si hay)
    go func() {
        g.Wait()
        close(results)
    }()
    
    for r := range results {
        fmt.Printf("  %s\n", r)
    }
    
    if err := g.Wait(); err != nil {
        fmt.Printf("\nFirst error: %v\n", err)
    } else {
        fmt.Println("\nAll hosts healthy")
    }
}
```

---

## 12. Comparacion con C y Rust

### 12.1 C — todo manual

```c
// C: implementar fan-out/fan-in requiere:
// - Thread pool con pthread_create
// - Cola thread-safe (mutex + cond var + buffer circular)
// - Logica de distribucion manual
// - Shutdown signaling con flags atomicos
// - Barrera o join para sincronizar al final

// Pseudocodigo de un worker pool en C:
#include <pthread.h>

#define NUM_WORKERS 4
#define QUEUE_SIZE 100

typedef struct {
    int data[QUEUE_SIZE];
    int head, tail, count;
    pthread_mutex_t mu;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
    int shutdown;
} Queue;

void *worker_func(void *arg) {
    Queue *q = (Queue *)arg;
    while (1) {
        pthread_mutex_lock(&q->mu);
        while (q->count == 0 && !q->shutdown)
            pthread_cond_wait(&q->not_empty, &q->mu);
        if (q->shutdown && q->count == 0) {
            pthread_mutex_unlock(&q->mu);
            return NULL;
        }
        int item = q->data[q->head];
        q->head = (q->head + 1) % QUEUE_SIZE;
        q->count--;
        pthread_cond_signal(&q->not_full);
        pthread_mutex_unlock(&q->mu);
        
        process(item); // procesar
    }
}

// ~60 lineas para lo que Go hace en ~15 lineas con channels

// PIPELINE en C: encadenar multiples queues thread-safe
// Cada etapa: thread pool + input queue + output queue
// Total para 3 etapas: ~200 lineas de boilerplate
// Go equivalente: ~30 lineas
```

### 12.2 Rust — rayon, crossbeam, tokio

```rust
// === Pipeline con iteradores (sync, rayon) ===
use rayon::prelude::*;

fn pipeline_rayon() {
    let results: Vec<i32> = (1..=100)
        .into_par_iter()           // fan-out automatico
        .filter(|&x| x % 2 == 0)  // filtrar pares
        .map(|x| x * x)           // cuadrados
        .filter(|&x| x > 100)     // solo > 100
        .collect();                // fan-in
    
    // rayon maneja el thread pool internamente
    // Mas simple que Go para este caso, pero menos flexible
    // No tiene "streaming" (channels) — es batch processing
}

// === Fan-out/Fan-in con crossbeam channels ===
use crossbeam_channel::{bounded, Sender, Receiver};
use std::thread;

fn fan_out_fan_in_crossbeam() {
    let (task_tx, task_rx) = bounded::<i32>(100);
    let (result_tx, result_rx) = bounded::<String>(100);
    
    // Fan-out: 4 workers leen del mismo channel
    let mut handles = vec![];
    for id in 0..4 {
        let rx = task_rx.clone(); // crossbeam Receiver es Clone (MPMC)
        let tx = result_tx.clone();
        handles.push(thread::spawn(move || {
            for task in rx {
                tx.send(format!("Worker {}: processed {}", id, task)).unwrap();
            }
        }));
    }
    drop(result_tx); // cerrar sender original
    
    // Enviar tareas
    thread::spawn(move || {
        for i in 1..=20 {
            task_tx.send(i).unwrap();
        }
        // task_tx se dropea aqui — cierra el channel
    });
    
    // Fan-in: leer resultados
    for result in result_rx {
        println!("{}", result);
    }
    
    for h in handles {
        h.join().unwrap();
    }
}

// === Pipeline con tokio channels (async) ===
use tokio::sync::mpsc;

async fn pipeline_tokio() {
    let (tx1, mut rx1) = mpsc::channel::<i32>(100);
    let (tx2, mut rx2) = mpsc::channel::<i32>(100);
    
    // Stage 1: filter
    tokio::spawn(async move {
        while let Some(v) = rx1.recv().await {
            if v % 2 == 0 {
                tx2.send(v).await.unwrap();
            }
        }
    });
    
    // Stage 2: transform
    let (tx3, mut rx3) = mpsc::channel::<String>(100);
    tokio::spawn(async move {
        while let Some(v) = rx2.recv().await {
            tx3.send(format!("result: {}", v * v)).await.unwrap();
        }
    });
    
    // Source
    tokio::spawn(async move {
        for i in 1..=10 {
            tx1.send(i).await.unwrap();
        }
    });
    
    // Sink
    while let Some(result) = rx3.recv().await {
        println!("{}", result);
    }
}

// COMPARACION:
// - rayon: mas simple para batch, pero no streaming
// - crossbeam: mas cercano a Go (MPMC, select!, sync)
// - tokio: async channels (function coloring, necesita runtime)
// - Go: built-in, sync channels, select keyword, goroutines
```

### 12.3 Tabla comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    PATRONES DE CONCURRENCIA: GO vs C vs RUST            │
├──────────────────┬─────────────────┬───────────────┬─────────────────────┤
│ Patron            │ C               │ Go            │ Rust               │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Pipeline          │ Thread pools +  │ Funciones que │ rayon iter chain   │
│                   │ colas manuales  │ retornan      │ o tokio channels   │
│                   │ (~200 lineas)   │ <-chan (<30)   │ (~40 lineas)       │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Fan-out           │ pthread_create  │ N goroutines  │ rayon par_iter     │
│                   │ + queue shared  │ leyendo same  │ o crossbeam clone  │
│                   │                 │ channel       │ receiver           │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Fan-in            │ pthread_join +  │ WaitGroup +   │ thread::join +     │
│                   │ result queue    │ close(results)│ channel collect    │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Done/cancel       │ Atomic flag +   │ close(done)   │ Drop(sender) or   │
│                   │ cond_signal     │ or ctx.Done() │ CancellationToken  │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Semaforo          │ sem_t (POSIX)   │ Buffered chan │ tokio::Semaphore   │
│                   │ sem_wait/post   │ struct{}      │ or Arc<Semaphore>  │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Confinement       │ Convention only │ Convention +  │ Ownership system   │
│                   │ (no enforcement)│ channel pass  │ (compiler enforced)│
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Error propagation │ errno + manual  │ Result struct │ Result<T,E> +      │
│                   │ checking        │ in channel    │ ? operator         │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Composabilidad    │ Baja (mucho     │ Alta (funciones│ Alta (iteradores, │
│                   │ boilerplate)    │ composables)  │ combinadores)      │
└──────────────────┴─────────────────┴───────────────┴─────────────────────┘
```

---

## 13. Programa Completo — Infrastructure Deployment Pipeline

Un sistema completo de deployment que combina todos los patrones: pipeline, fan-out/fan-in, done channel, semaforo, tee, confinement, y error propagation:

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "strings"
    "sync"
    "time"
)

// ============================================================
// Tipos
// ============================================================

type Service struct {
    Name     string
    Image    string
    Replicas int
    Port     int
}

type DeployResult struct {
    Service  Service
    Phase    string
    Success  bool
    Message  string
    Duration time.Duration
}

type PipelineStats struct {
    Total     int
    Succeeded int
    Failed    int
    Phases    map[string]int
    mu        sync.Mutex
}

func (s *PipelineStats) Record(result DeployResult) {
    s.mu.Lock()
    defer s.mu.Unlock()
    s.Total++
    if result.Success {
        s.Succeeded++
    } else {
        s.Failed++
    }
    s.Phases[result.Phase]++
}

// ============================================================
// Pipeline Stages
// ============================================================

// Stage 1: Validate — verifica que el servicio es desplegable
func validate(ctx context.Context, in <-chan Service) (<-chan Service, <-chan DeployResult) {
    valid := make(chan Service)
    results := make(chan DeployResult)
    
    go func() {
        defer close(valid)
        defer close(results)
        
        for svc := range in {
            start := time.Now()
            
            var err string
            if svc.Name == "" {
                err = "empty service name"
            } else if svc.Replicas <= 0 {
                err = "invalid replica count"
            } else if svc.Port < 1024 || svc.Port > 65535 {
                err = fmt.Sprintf("invalid port: %d", svc.Port)
            }
            
            if err != "" {
                select {
                case results <- DeployResult{
                    Service: svc, Phase: "validate", Success: false,
                    Message: err, Duration: time.Since(start),
                }:
                case <-ctx.Done():
                    return
                }
                continue
            }
            
            select {
            case valid <- svc:
            case <-ctx.Done():
                return
            }
        }
    }()
    
    return valid, results
}

// Stage 2: Pull Image — fan-out con semaforo (max 3 pulls concurrentes)
func pullImages(ctx context.Context, in <-chan Service, maxConcurrent int) (<-chan Service, <-chan DeployResult) {
    pulled := make(chan Service)
    results := make(chan DeployResult)
    sem := make(chan struct{}, maxConcurrent) // semaforo
    
    go func() {
        defer close(pulled)
        defer close(results)
        
        var wg sync.WaitGroup
        
        for svc := range in {
            wg.Add(1)
            
            // Adquirir semaforo
            select {
            case sem <- struct{}{}:
            case <-ctx.Done():
                wg.Done()
                continue
            }
            
            go func(s Service) {
                defer wg.Done()
                defer func() { <-sem }() // liberar semaforo
                
                start := time.Now()
                
                // Simular pull de imagen
                pullTime := time.Duration(100+rand.Intn(400)) * time.Millisecond
                select {
                case <-time.After(pullTime):
                case <-ctx.Done():
                    return
                }
                
                // 5% de fallo en pull
                if rand.Intn(20) == 0 {
                    select {
                    case results <- DeployResult{
                        Service: s, Phase: "pull", Success: false,
                        Message: fmt.Sprintf("failed to pull %s: timeout", s.Image),
                        Duration: time.Since(start),
                    }:
                    case <-ctx.Done():
                    }
                    return
                }
                
                select {
                case pulled <- s:
                case <-ctx.Done():
                }
            }(svc)
        }
        
        wg.Wait()
    }()
    
    return pulled, results
}

// Stage 3: Deploy — fan-out workers para deployment real
func deploy(ctx context.Context, in <-chan Service, numWorkers int) <-chan DeployResult {
    results := make(chan DeployResult)
    
    var wg sync.WaitGroup
    
    for i := 0; i < numWorkers; i++ {
        wg.Add(1)
        go func(workerID int) {
            defer wg.Done()
            
            for svc := range in {
                start := time.Now()
                
                // Simular deploy de cada replica (confinement por iteracion)
                var replicaResults []string
                allOk := true
                
                for r := 1; r <= svc.Replicas; r++ {
                    deployTime := time.Duration(50+rand.Intn(150)) * time.Millisecond
                    select {
                    case <-time.After(deployTime):
                        if rand.Intn(15) == 0 {
                            replicaResults = append(replicaResults,
                                fmt.Sprintf("replica %d: FAILED", r))
                            allOk = false
                        } else {
                            replicaResults = append(replicaResults,
                                fmt.Sprintf("replica %d: OK", r))
                        }
                    case <-ctx.Done():
                        return
                    }
                }
                
                msg := fmt.Sprintf("deployed %d replicas [%s]",
                    svc.Replicas, strings.Join(replicaResults, ", "))
                
                select {
                case results <- DeployResult{
                    Service: svc, Phase: "deploy", Success: allOk,
                    Message: msg, Duration: time.Since(start),
                }:
                case <-ctx.Done():
                    return
                }
            }
        }(i)
    }
    
    go func() {
        wg.Wait()
        close(results)
    }()
    
    return results
}

// ============================================================
// Fan-in para resultados de multiples stages
// ============================================================

func mergeResults(ctx context.Context, channels ...<-chan DeployResult) <-chan DeployResult {
    out := make(chan DeployResult)
    var wg sync.WaitGroup
    
    for _, ch := range channels {
        wg.Add(1)
        go func(c <-chan DeployResult) {
            defer wg.Done()
            for r := range c {
                select {
                case out <- r:
                case <-ctx.Done():
                    return
                }
            }
        }(ch)
    }
    
    go func() {
        wg.Wait()
        close(out)
    }()
    
    return out
}

// ============================================================
// Tee para logging + stats simultaneamente
// ============================================================

func teeDeploy(ctx context.Context, in <-chan DeployResult) (<-chan DeployResult, <-chan DeployResult) {
    out1 := make(chan DeployResult)
    out2 := make(chan DeployResult)
    
    go func() {
        defer close(out1)
        defer close(out2)
        for r := range in {
            o1, o2 := out1, out2
            for o1 != nil || o2 != nil {
                select {
                case o1 <- r:
                    o1 = nil
                case o2 <- r:
                    o2 = nil
                case <-ctx.Done():
                    return
                }
            }
        }
    }()
    
    return out1, out2
}

// ============================================================
// Main
// ============================================================

func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()

    // Servicios a desplegar
    services := []Service{
        {Name: "api-gateway", Image: "api:v2.1.0", Replicas: 3, Port: 8080},
        {Name: "auth-service", Image: "auth:v1.5.2", Replicas: 2, Port: 8081},
        {Name: "user-service", Image: "user:v3.0.1", Replicas: 3, Port: 8082},
        {Name: "order-service", Image: "order:v2.4.0", Replicas: 2, Port: 8083},
        {Name: "payment-service", Image: "payment:v1.2.3", Replicas: 2, Port: 8084},
        {Name: "notification-svc", Image: "notif:v1.0.0", Replicas: 1, Port: 8085},
        {Name: "search-service", Image: "search:v2.2.1", Replicas: 3, Port: 8086},
        {Name: "cache-warmer", Image: "cache:v1.1.0", Replicas: 1, Port: 8087},
    }

    fmt.Println("╔══════════════════════════════════════════════════════════════╗")
    fmt.Println("║           INFRASTRUCTURE DEPLOYMENT PIPELINE                ║")
    fmt.Println("╠══════════════════════════════════════════════════════════════╣")
    fmt.Printf("║  Services: %d | Pull workers: 3 | Deploy workers: 4        ║\n", len(services))
    fmt.Println("║  Patterns: pipeline, fan-out/in, semaphore, tee, confine   ║")
    fmt.Println("╚══════════════════════════════════════════════════════════════╝")

    //  Pipeline architecture:
    //
    //  [Source] ──► [Validate] ──► [Pull Images] ──► [Deploy] ──► [Tee]
    //                  │              (sem=3)        (workers=4)    │  │
    //                  │ errors       │ errors                      │  │
    //                  ▼              ▼                             ▼  ▼
    //              [Merge Results] ◄──────────────────── [Log] [Stats]

    fmt.Println()

    // Source (generator)
    source := make(chan Service)
    go func() {
        defer close(source)
        for _, svc := range services {
            select {
            case source <- svc:
            case <-ctx.Done():
                return
            }
        }
    }()

    // Pipeline stages
    validated, validateErrors := validate(ctx, source)
    pulled, pullErrors := pullImages(ctx, validated, 3) // semaforo de 3
    deployResults := deploy(ctx, pulled, 4)              // 4 workers

    // Fan-in: merge all result streams
    allResults := mergeResults(ctx, validateErrors, pullErrors, deployResults)

    // Tee: log + stats
    toLog, toStats := teeDeploy(ctx, allResults)

    // Stats collector
    stats := &PipelineStats{Phases: make(map[string]int)}
    
    var wg sync.WaitGroup
    
    // Logger sink
    wg.Add(1)
    go func() {
        defer wg.Done()
        for r := range toLog {
            icon := "OK"
            if !r.Success {
                icon = "FAIL"
            }
            fmt.Printf("  [%-4s] [%-8s] %-20s %s (%v)\n",
                icon, r.Phase, r.Service.Name, r.Message,
                r.Duration.Round(time.Millisecond))
        }
    }()
    
    // Stats sink
    wg.Add(1)
    go func() {
        defer wg.Done()
        for r := range toStats {
            stats.Record(r)
        }
    }()

    wg.Wait()

    // Print summary
    fmt.Println()
    fmt.Println("┌──────────────────────────────────────────────────┐")
    fmt.Println("│              DEPLOYMENT SUMMARY                   │")
    fmt.Println("├──────────────────────────────────────────────────┤")
    fmt.Printf("│  Total operations:   %-5d                       │\n", stats.Total)
    fmt.Printf("│  Succeeded:          %-5d                       │\n", stats.Succeeded)
    fmt.Printf("│  Failed:             %-5d                       │\n", stats.Failed)
    fmt.Println("├──────────────────────────────────────────────────┤")
    fmt.Println("│  By phase:                                       │")
    for phase, count := range stats.Phases {
        padding := strings.Repeat(" ", 20-len(phase))
        fmt.Printf("│    %s:%s%-5d                       │\n", phase, padding, count)
    }
    fmt.Println("├──────────────────────────────────────────────────┤")
    fmt.Println("│  Patterns used:                                  │")
    fmt.Println("│    Pipeline:     validate → pull → deploy       │")
    fmt.Println("│    Fan-out:      4 deploy workers               │")
    fmt.Println("│    Fan-in:       merge 3 result streams         │")
    fmt.Println("│    Semaphore:    max 3 concurrent pulls         │")
    fmt.Println("│    Tee:          log + stats simultaneously     │")
    fmt.Println("│    Confinement:  replica results per goroutine  │")
    fmt.Println("│    Done:         ctx.Done() in all stages       │")
    fmt.Println("│    Error prop:   errors flow through pipeline   │")
    fmt.Println("└──────────────────────────────────────────────────┘")
}
```

### Ejecucion esperada

```bash
$ go run main.go

╔══════════════════════════════════════════════════════════════╗
║           INFRASTRUCTURE DEPLOYMENT PIPELINE                ║
╠══════════════════════════════════════════════════════════════╣
║  Services: 8 | Pull workers: 3 | Deploy workers: 4        ║
║  Patterns: pipeline, fan-out/in, semaphore, tee, confine   ║
╚══════════════════════════════════════════════════════════════╝

  [OK  ] [pull    ] api-gateway          pulled api:v2.1.0 (234ms)
  [OK  ] [pull    ] auth-service         pulled auth:v1.5.2 (189ms)
  [OK  ] [pull    ] user-service         pulled user:v3.0.1 (312ms)
  [OK  ] [deploy  ] api-gateway          deployed 3 replicas [r1: OK, r2: OK, r3: OK] (345ms)
  [OK  ] [deploy  ] auth-service         deployed 2 replicas [r1: OK, r2: OK] (198ms)
  [FAIL] [deploy  ] user-service         deployed 3 replicas [r1: OK, r2: FAILED, r3: OK] (267ms)
  [OK  ] [pull    ] order-service        pulled order:v2.4.0 (156ms)
  [OK  ] [pull    ] payment-service      pulled payment:v1.2.3 (278ms)
  [OK  ] [deploy  ] order-service        deployed 2 replicas [r1: OK, r2: OK] (156ms)
  [OK  ] [pull    ] notification-svc     pulled notif:v1.0.0 (123ms)
  [OK  ] [deploy  ] payment-service      deployed 2 replicas [r1: OK, r2: OK] (201ms)
  [OK  ] [pull    ] search-service       pulled search:v2.2.1 (345ms)
  [OK  ] [pull    ] cache-warmer         pulled cache:v1.1.0 (167ms)
  [OK  ] [deploy  ] notification-svc     deployed 1 replicas [r1: OK] (89ms)
  [OK  ] [deploy  ] search-service       deployed 3 replicas [r1: OK, r2: OK, r3: OK] (312ms)
  [OK  ] [deploy  ] cache-warmer         deployed 1 replicas [r1: OK] (78ms)

┌──────────────────────────────────────────────────────────────┐
│              DEPLOYMENT SUMMARY                               │
├──────────────────────────────────────────────────────────────┤
│  Total operations:   8                                        │
│  Succeeded:          7                                        │
│  Failed:             1                                        │
├──────────────────────────────────────────────────────────────┤
│  By phase:                                                    │
│    deploy:              8                                     │
├──────────────────────────────────────────────────────────────┤
│  Patterns used:                                               │
│    Pipeline:     validate → pull → deploy                    │
│    Fan-out:      4 deploy workers                            │
│    Fan-in:       merge 3 result streams                      │
│    Semaphore:    max 3 concurrent pulls                      │
│    Tee:          log + stats simultaneously                  │
│    Confinement:  replica results per goroutine               │
│    Done:         ctx.Done() in all stages                    │
│    Error prop:   errors flow through pipeline                │
└──────────────────────────────────────────────────────────────┘
```

---

## 14. Ejercicios

### Ejercicio 1: Log processing pipeline

Implementa un pipeline completo de procesamiento de logs:

```
[FileReader] → [Parser] → [Enricher] → [Fan-out: 3 workers] → [Fan-in] → [Tee] → [File + Console]
```

- **FileReader**: genera lineas simuladas de log (100 lineas)
- **Parser**: extrae timestamp, level, message (descarta lineas invalidas)
- **Enricher**: agrega hostname y request_id
- **Fan-out**: 3 workers clasifican logs por severity (usando semaforo de 3)
- **Fan-in**: merge resultados de los 3 workers
- **Tee**: duplica para escritura a "archivo" (buffered) y consola
- Todos los stages deben soportar cancelacion con context
- Usar Result[T] para propagar errores a traves del pipeline

### Ejercicio 2: HTTP scraper con rate limiting

Implementa un scraper que procesa una lista de URLs:

```
[URLSource] → [Fan-out: N workers + semaforo] → [Fan-in] → [OrderedCollect]
```

- Semaforo: maximo 5 requests concurrentes
- Rate limiter: maximo 10 requests por segundo (token bucket)
- Timeout: 3 segundos por request
- Preservar orden original de las URLs en el resultado
- Reintentar URLs fallidas hasta 3 veces con backoff
- Done channel para cancelar todo si mas del 50% falla

### Ejercicio 3: Event sourcing pipeline

Implementa un pipeline de event sourcing:

```
[EventSource] → [Validator] → [Tee] → [EventStore] + [Projector]
                                                       ↓
                                              [Bridge] → [Notifier]
```

- **EventSource**: genera eventos (UserCreated, OrderPlaced, PaymentProcessed)
- **Validator**: rechaza eventos invalidos (propaga error via Result)
- **Tee**: duplica para almacenamiento y proyeccion
- **EventStore**: acumula eventos en orden (confinement)
- **Projector**: actualiza vista materializada (genera sub-eventos via channel de channels)
- **Bridge**: aplana los sub-eventos
- **Notifier**: envia notificaciones

### Ejercicio 4: Deployment orchestrator

Implementa un orquestador de deployment con rollback:

- Pipeline de 5 fases: validate → pull → deploy → health-check → promote
- Si health-check falla, trigger rollback pipeline (reverse order)
- Fan-out/fan-in para cada fase (N servicios en paralelo)
- Semaforo global: maximo 3 servicios desplegandose a la vez
- Done channel: si un servicio critico falla, cancelar todo
- Tee: enviar progreso a consola + audit log
- Confinement: cada servicio mantiene su estado de rollback localmente

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    PATRONES CON CHANNELS — RESUMEN COMPLETO             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  PIPELINE:                                                               │
│  ├─ Cadena de etapas: Source → Stage → Stage → Sink                   │
│  ├─ Cada etapa: in <-chan → goroutine → out <-chan                     │
│  ├─ Propagacion de close a traves de la cadena                        │
│  ├─ Context para cancelacion de todo el pipeline                       │
│  └─ Generics (Go 1.18+) para stages reutilizables                     │
│                                                                          │
│  FAN-OUT:                                                                │
│  ├─ N goroutines leyendo del mismo channel                             │
│  ├─ Go distribuye: cada valor va a exactamente 1 worker               │
│  └─ Worker pool = fan-out mas comun                                    │
│                                                                          │
│  FAN-IN:                                                                 │
│  ├─ N channels de entrada → 1 channel de salida                       │
│  ├─ Implementacion: goroutine-per-channel o reflect.Select            │
│  └─ Output se cierra cuando TODOS los inputs se cierran               │
│                                                                          │
│  DONE CHANNEL:                                                           │
│  ├─ close(done) como broadcast a todas las goroutines                  │
│  ├─ chan struct{} para zero-cost signaling                             │
│  ├─ Or-done: envolver channel con cancelacion                          │
│  └─ En Go moderno, context.Context es preferido                        │
│                                                                          │
│  SEMAFORO:                                                               │
│  ├─ make(chan struct{}, N) como semaforo de N permisos                 │
│  ├─ Acquire: sem <- struct{}{}  (bloquea si lleno)                    │
│  ├─ Release: <-sem  (libera espacio)                                   │
│  └─ Con timeout: select { case sem <- ...: case <-ctx.Done(): }       │
│                                                                          │
│  TEE:                                                                    │
│  ├─ Duplicar un channel en 2+ copias                                   │
│  ├─ Cada copia recibe todos los valores                                │
│  └─ Necesita select para manejar consumers a diferente velocidad      │
│                                                                          │
│  BRIDGE:                                                                 │
│  ├─ Aplanar <-chan <-chan T en <-chan T                                 │
│  └─ Util para APIs paginadas o generadores de secuencias              │
│                                                                          │
│  CONFINEMENT:                                                            │
│  ├─ Cada goroutine trabaja con sus propios datos                       │
│  ├─ Ownership se transfiere via channels                               │
│  └─ Elimina necesidad de locks (la mejor solucion a data races)       │
│                                                                          │
│  ERROR PROPAGATION:                                                      │
│  ├─ Result[T]{Value, Err} a traves del pipeline                       │
│  ├─ Errores bypass stages (se pasan sin procesar)                     │
│  └─ errgroup para fan-out con error collection                         │
│                                                                          │
│  vs C: todo manual (~200 lineas para un pipeline de 3 stages)          │
│  vs Rust: rayon (batch), crossbeam (channels), tokio (async)           │
│  Go: channels built-in + goroutines = patrones composables en <30 LOC │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S03 - Sincronizacion, T01 - Mutex — sync.Mutex, sync.RWMutex, Lock/Unlock, defer unlock, cuando preferir sobre channels
