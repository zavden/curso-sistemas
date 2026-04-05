# Direccion de Channels — chan<- (send-only), <-chan (receive-only), cuando anotar direccion

## 1. Introduccion

Go permite restringir un channel a **solo envio** (`chan<- T`) o **solo recepcion** (`<-chan T`). Estas anotaciones de direccion son una forma de **documentacion ejecutable**: el compilador garantiza que un channel send-only nunca se use para recibir, y viceversa. Es uno de los mecanismos mas elegantes de Go para expresar contratos entre goroutines — sin interfaces adicionales, sin wrappers, sin costo en runtime.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    DIRECCION DE CHANNELS                                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  TRES TIPOS DE CHANNEL:                                                  │
│                                                                          │
│  chan T        bidireccional — puede enviar Y recibir                   │
│  chan<- T      send-only    — solo puede enviar                         │
│  <-chan T      receive-only — solo puede recibir                        │
│                                                                          │
│  CONVERSION IMPLICITA:                                                   │
│                                                                          │
│  chan T ──► chan<- T    (OK — restringir a solo envio)                  │
│  chan T ──► <-chan T    (OK — restringir a solo recepcion)              │
│  chan<- T ──► chan T    (ERROR — no puedes ampliar permisos)             │
│  <-chan T ──► chan T    (ERROR — no puedes ampliar permisos)             │
│  chan<- T ──► <-chan T  (ERROR — no puedes cambiar direccion)            │
│                                                                          │
│  Es como permisos de archivo:                                           │
│  ├─ chan T   = rw (lectura + escritura)                                 │
│  ├─ chan<- T = w  (solo escritura)                                      │
│  └─ <-chan T = r  (solo lectura)                                        │
│  Puedes quitar permisos, nunca agregar.                                 │
│                                                                          │
│  PRINCIPIO: "Otorga el minimo privilegio necesario."                    │
│  Si una funcion solo envia, dale chan<- T.                              │
│  Si una funcion solo recibe, dale <-chan T.                             │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

Para SysAdmin/DevOps, la direccion de channels es importante porque:
- En pipelines complejos (deploy, monitoring, log processing), cada etapa tiene un rol claro: produce o consume
- La direccion previene bugs donde una etapa accidentalmente envia por un channel de entrada o recibe por uno de salida
- En equipos grandes, la firma de la funcion documenta si es productor o consumidor sin leer la implementacion
- Herramientas como `golangci-lint` advierten sobre channels bidireccionales que solo se usan en una direccion

---

## 2. Sintaxis y Lectura del Tipo

### 2.1 Como leer la flecha

La posicion de `<-` respecto a `chan` determina la direccion:

```go
// REGLA: la flecha <- siempre apunta en la direccion del FLUJO de datos

chan<- T    // la flecha "entra" al channel → send-only
            // "yo puedo METER datos en este channel"
            // Operaciones permitidas: ch <- val
            // Operaciones prohibidas: <-ch, close(ch)... ← ESPERA

<-chan T    // la flecha "sale" del channel → receive-only
            // "yo puedo SACAR datos de este channel"
            // Operaciones permitidas: <-ch, for range ch
            // Operaciones prohibidas: ch <- val, close(ch)
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    MNEMOTECNICO PARA RECORDAR                            │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  chan<- T     "datos van HACIA el channel"     → SEND-ONLY             │
│     ──►                                                                  │
│                                                                          │
│  <-chan T     "datos SALEN del channel"        → RECEIVE-ONLY          │
│  ◄──                                                                     │
│                                                                          │
│  Otra forma de verlo:                                                    │
│                                                                          │
│  chan<- T     <- esta a la DERECHA de chan → flecha HACIA chan → SEND  │
│  <-chan T     <- esta a la IZQUIERDA de chan → flecha DESDE chan → RECV│
│                                                                          │
│  Y la mas simple:                                                        │
│  Si puedes leer "chan recibe flecha" → send-only                       │
│  Si puedes leer "flecha sale de chan" → receive-only                   │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Tipos compuestos con channels

La lectura se complica con tipos mas complejos. Las reglas de precedencia:

```go
// Channel de channels — la direccion exterior es la del channel principal
chan<- chan int      // send-only de (chan int bidireccional)
chan<- <-chan int    // send-only de (receive-only chan int)
<-chan chan<- int    // receive-only de (send-only chan int)

// Con funciones
chan<- func() int   // send-only de funciones que retornan int

// Con punteros
chan *int            // channel bidireccional de punteros a int
chan<- *int          // send-only de punteros a int

// Con slices
<-chan []string      // receive-only de slices de strings

// Con maps
chan<- map[string]int // send-only de maps
```

```go
// Caso ambiguo — NECESITA parentesis para claridad
// (aunque Go tiene reglas de precedencia definidas)

// Sin parentesis:
<-chan<- int         // ERROR de compilacion — ambiguo
                     // ¿Es (<-chan) (<-int) o <-(chan<- int)?

// Con parentesis (siempre claro):
(<-chan) (chan<- int) // receive-only de (send-only de int)
// Pero esto no es sintaxis valida — el tipo ya se resuelve por precedencia

// En la practica, Go resuelve de izquierda a derecha:
<-chan (<-chan int)   // receive-only de (receive-only de int)
// Esto SI compila y es un channel de channels
```

### 2.3 Tabla de operaciones permitidas

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    OPERACIONES POR TIPO DE CHANNEL                       │
├───────────────────┬──────────────┬──────────────┬────────────────────────┤
│ Operacion          │ chan T        │ chan<- T      │ <-chan T              │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ ch <- val          │ ✓            │ ✓            │ ✗ (compile error)    │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ <-ch              │ ✓            │ ✗ (compile   │ ✓                    │
│                   │              │    error)    │                        │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ close(ch)          │ ✓            │ ✓ (*)        │ ✗ (compile error)    │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ for v := range ch │ ✓            │ ✗ (compile   │ ✓                    │
│                   │              │    error)    │                        │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ len(ch)            │ ✓            │ ✓            │ ✓                    │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ cap(ch)            │ ✓            │ ✓            │ ✓                    │
└───────────────────┴──────────────┴──────────────┴────────────────────────┘

(*) close() en chan<- T esta PERMITIDO por el compilador.
    Esto es intencional: el sender es quien cierra el channel.
    "Solo el productor cierra" → el productor tiene chan<- T → necesita close().
    
    NOTA: close() en <-chan T esta PROHIBIDO.
    El receiver NO puede cerrar — es responsabilidad del sender.
```

---

## 3. Conversion Implicita

Go convierte automaticamente un channel bidireccional a una direccion restringida cuando se pasa como argumento. Esta conversion es **implicita**, **gratuita** (sin costo en runtime), y **solo en una direccion** (restringir, nunca ampliar).

### 3.1 Conversion en la practica

```go
package main

import "fmt"

// producer recibe solo un send-only channel
func producer(ch chan<- int) {
    for i := 1; i <= 5; i++ {
        ch <- i
    }
    close(ch) // permitido — el sender cierra
}

// consumer recibe solo un receive-only channel
func consumer(ch <-chan int) {
    for v := range ch {
        fmt.Printf("Recibido: %d\n", v)
    }
}

func main() {
    ch := make(chan int) // bidireccional
    
    // Go convierte implicitamente:
    go producer(ch) // chan int → chan<- int (OK — restriccion)
    consumer(ch)    // chan int → <-chan int (OK — restriccion)
    
    // Errores de compilacion que la direccion previene:
    // En producer: <-ch → "invalid operation: receive from send-only channel"
    // En consumer: ch <- 1 → "invalid operation: send to receive-only channel"
}
```

### 3.2 Que conversiones son validas

```go
package main

func main() {
    ch := make(chan int)
    
    // Restringir — SIEMPRE valido
    var sendOnly chan<- int = ch   // OK
    var recvOnly <-chan int = ch   // OK
    
    // Ampliar — NUNCA valido
    // var bidir chan int = sendOnly  // ERROR: cannot use sendOnly (chan<- int) as chan int
    // var bidir chan int = recvOnly  // ERROR: cannot use recvOnly (<-chan int) as chan int
    
    // Cambiar direccion — NUNCA valido
    // var recv <-chan int = sendOnly  // ERROR: cannot use sendOnly (chan<- int) as <-chan int
    // var send chan<- int = recvOnly  // ERROR: cannot use recvOnly (<-chan int) as chan<- int
    
    _ = sendOnly
    _ = recvOnly
}
```

### 3.3 Diagrama de conversiones

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    JERARQUIA DE CONVERSIONES                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│                        chan T                                             │
│                      (bidireccional)                                     │
│                       /         \                                        │
│                      ▼           ▼                                       │
│              chan<- T             <-chan T                                │
│             (send-only)         (receive-only)                           │
│                                                                          │
│  ────────── = conversion implicita permitida (siempre hacia abajo)     │
│                                                                          │
│  NO hay conversiones:                                                    │
│  - De abajo hacia arriba (ampliar permisos)                            │
│  - Horizontalmente (cambiar direccion)                                  │
│                                                                          │
│  ANALOGIA con interfaces:                                                │
│  ├─ chan T      es como io.ReadWriter                                  │
│  ├─ chan<- T    es como io.Writer                                      │
│  └─ <-chan T    es como io.Reader                                      │
│  Un ReadWriter puede usarse donde se pide un Reader o Writer,          │
│  pero no al reves.                                                      │
│                                                                          │
│  ANALOGIA con permisos Unix:                                             │
│  ├─ chan T      es como rw (chmod 6xx)                                 │
│  ├─ chan<- T    es como w  (chmod 2xx)                                 │
│  └─ <-chan T    es como r  (chmod 4xx)                                 │
│  chmod puede quitar permisos, pero un proceso no puede darselos solo.  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.4 Internamente no hay diferencia

La conversion no crea un nuevo channel ni copia datos. Es puramente un **constraint del tipo system** — en runtime, `chan T`, `chan<- T`, y `<-chan T` son exactamente el mismo puntero a `hchan`:

```go
package main

import (
    "fmt"
    "unsafe"
)

func main() {
    ch := make(chan int)
    var sendOnly chan<- int = ch
    var recvOnly <-chan int = ch
    
    // Los tres apuntan a la misma estructura hchan
    fmt.Printf("chan T:    %p\n", ch)        // 0xc000060060
    fmt.Printf("chan<- T:  %p\n", sendOnly)  // 0xc000060060
    fmt.Printf("<-chan T:  %p\n", recvOnly)  // 0xc000060060
    
    // Tamano del tipo — todos son un puntero (8 bytes en 64-bit)
    fmt.Printf("sizeof chan T:    %d\n", unsafe.Sizeof(ch))        // 8
    fmt.Printf("sizeof chan<- T:  %d\n", unsafe.Sizeof(sendOnly))  // 8
    fmt.Printf("sizeof <-chan T:  %d\n", unsafe.Sizeof(recvOnly))  // 8
    
    // NO hay overhead en runtime — la restriccion es 100% compile-time
}
```

---

## 4. Cuando y Por Que Anotar Direccion

### 4.1 Regla general

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CUANDO ANOTAR DIRECCION                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  REGLA: Anotar SIEMPRE la direccion en:                                │
│                                                                          │
│  1. PARAMETROS de funciones                                             │
│     func producer(out chan<- int)                                       │
│     func consumer(in <-chan int)                                        │
│     → Documenta el contrato y el compilador lo enforce                 │
│                                                                          │
│  2. RETORNOS de funciones (generators)                                  │
│     func counter() <-chan int                                           │
│     → El caller solo puede LEER, no enviar accidentalmente            │
│                                                                          │
│  3. CAMPOS de structs                                                   │
│     type Worker struct { tasks <-chan Task }                            │
│     → Cada componente tiene acceso minimo                              │
│                                                                          │
│  NO anotar en:                                                          │
│                                                                          │
│  1. Variables locales (cuando necesitas ambas operaciones)              │
│     ch := make(chan int) // bidireccional localmente                    │
│                                                                          │
│  2. Cuando pasas el channel a funciones que ya anotan                   │
│     (la conversion es implicita)                                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Patron: funcion generator

El patron mas comun. La funcion crea el channel internamente y retorna solo la vista de lectura:

```go
package main

import (
    "fmt"
    "time"
)

// tick retorna un receive-only channel que emite cada intervalo
// El caller NO puede enviar ni cerrar — solo el generator controla el lifecycle
func tick(interval time.Duration) <-chan time.Time {
    ch := make(chan time.Time)   // bidireccional internamente
    go func() {
        ticker := time.NewTicker(interval)
        defer ticker.Stop()
        for t := range ticker.C {
            ch <- t
        }
    }()
    return ch  // retorna como <-chan time.Time (conversion implicita)
}

// counter retorna un receive-only channel que emite numeros secuenciales
func counter(start, end int) <-chan int {
    ch := make(chan int)
    go func() {
        for i := start; i <= end; i++ {
            ch <- i
        }
        close(ch) // el generator cierra — el caller no puede
    }()
    return ch
}

func main() {
    // El caller solo puede RECIBIR — no puede interferir con el generator
    for v := range counter(1, 5) {
        fmt.Printf("%d ", v)
    }
    fmt.Println()
    // Output: 1 2 3 4 5
    
    // Estos serian errores de compilacion:
    // c := counter(1, 5)
    // c <- 99       // ERROR: send to receive-only channel
    // close(c)      // ERROR: close of receive-only channel
}
```

### 4.3 Patron: producer/consumer

```go
package main

import (
    "fmt"
    "strings"
    "sync"
)

// produce SOLO envia — no puede leer del channel
func produce(out chan<- string, items []string) {
    for _, item := range items {
        out <- strings.ToUpper(item)
    }
    close(out) // sender cierra — permitido con chan<-
}

// consume SOLO recibe — no puede enviar al channel
func consume(in <-chan string, wg *sync.WaitGroup) {
    defer wg.Done()
    for msg := range in {
        fmt.Printf("  Procesado: %s\n", msg)
    }
}

func main() {
    ch := make(chan string, 5) // bidireccional local
    
    var wg sync.WaitGroup
    wg.Add(1)
    
    go produce(ch, []string{"alpha", "bravo", "charlie", "delta"})
    go consume(ch, &wg)
    
    wg.Wait()
    
    // Si produce intentara <-out → compile error
    // Si consume intentara in <- "x" → compile error
}
```

### 4.4 Patron: pipeline con direccion en cada etapa

```go
package main

import "fmt"

// Cada etapa del pipeline tiene su contrato claro:
//   in: <-chan T (solo lee de la etapa anterior)
//   out: retorna <-chan U (solo escribe internamente, expone solo lectura)

func generate(nums ...int) <-chan int {
    out := make(chan int)
    go func() {
        for _, n := range nums {
            out <- n
        }
        close(out)
    }()
    return out
}

func square(in <-chan int) <-chan int {
    out := make(chan int)
    go func() {
        for n := range in {
            out <- n * n
        }
        close(out)
    }()
    return out
}

func filter(in <-chan int, predicate func(int) bool) <-chan int {
    out := make(chan int)
    go func() {
        for n := range in {
            if predicate(n) {
                out <- n
            }
        }
        close(out)
    }()
    return out
}

func printer(in <-chan int) {
    for v := range in {
        fmt.Printf("%d ", v)
    }
    fmt.Println()
}

func main() {
    // Pipeline: generate → square → filter(>10) → print
    //
    // Cada → es una conversion chan int → <-chan int
    // El compilador GARANTIZA que ninguna etapa puede
    // enviar hacia atras ni cerrar el input de otra etapa
    
    nums := generate(1, 2, 3, 4, 5, 6, 7, 8, 9, 10)
    squared := square(nums)
    big := filter(squared, func(n int) bool { return n > 10 })
    printer(big)
    // Output: 16 25 36 49 64 81 100
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    PIPELINE CON DIRECCION                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  generate()     square()      filter()      printer()                   │
│  ┌─────────┐   ┌─────────┐   ┌─────────┐   ┌─────────┐               │
│  │ produce │   │ consume │   │ consume │   │ consume │               │
│  │         │──►│ produce │──►│ produce │──►│         │               │
│  └─────────┘   └─────────┘   └─────────┘   └─────────┘               │
│  ret: <-chan   in: <-chan     in: <-chan     in: <-chan                  │
│                ret: <-chan    ret: <-chan                                │
│                                                                          │
│  Cada etapa:                                                             │
│  ├─ Internamente crea chan T (bidireccional)                           │
│  ├─ Escribe en el (tiene acceso completo)                              │
│  ├─ Lo cierra cuando termina de producir                               │
│  └─ Lo retorna como <-chan T (solo lectura para la siguiente etapa)    │
│                                                                          │
│  BENEFICIO:                                                              │
│  square() NO PUEDE enviar datos a generate() por accidente            │
│  filter() NO PUEDE cerrar el channel de square()                       │
│  Cada etapa es una caja negra con interfaces claras                    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Direccion en Structs

Usar channels direccionales en campos de structs refuerza la separacion de responsabilidades a nivel de componente:

### 5.1 Componentes con roles claros

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

// ============================================================
// Cada struct tiene SOLO el acceso que necesita
// ============================================================

type Event struct {
    Type    string
    Message string
    Time    time.Time
}

// EventEmitter SOLO puede enviar eventos
type EventEmitter struct {
    name   string
    events chan<- Event  // send-only — no puede leer sus propios eventos
}

func (e *EventEmitter) Emit(eventType, message string) {
    e.events <- Event{
        Type:    eventType,
        Message: fmt.Sprintf("[%s] %s", e.name, message),
        Time:    time.Now(),
    }
}

// EventHandler SOLO puede recibir eventos
type EventHandler struct {
    name   string
    events <-chan Event  // receive-only — no puede inyectar eventos
}

func (h *EventHandler) Handle(wg *sync.WaitGroup) {
    defer wg.Done()
    for event := range h.events {
        fmt.Printf("Handler(%s): [%s] %s\n", h.name, event.Type, event.Message)
    }
}

func main() {
    // El channel bidireccional existe SOLO aqui — control centralizado
    eventBus := make(chan Event, 10)
    
    // Los componentes reciben vistas restringidas
    emitter1 := &EventEmitter{name: "api-server", events: eventBus}
    emitter2 := &EventEmitter{name: "database", events: eventBus}
    handler := &EventHandler{name: "logger", events: eventBus}
    
    var wg sync.WaitGroup
    wg.Add(1)
    go handler.Handle(&wg)
    
    // Emitters envian eventos
    emitter1.Emit("REQUEST", "GET /api/users")
    emitter2.Emit("QUERY", "SELECT * FROM users")
    emitter1.Emit("RESPONSE", "200 OK, 3 users")
    emitter2.Emit("SLOW_QUERY", "took 2.3s")
    
    close(eventBus) // solo main tiene acceso bidireccional para cerrar
    wg.Wait()
    
    // emitter1 NO puede leer eventos: <-emitter1.events → compile error
    // handler NO puede inyectar eventos: handler.events <- e → compile error
    // handler NO puede cerrar: close(handler.events) → compile error
}
```

### 5.2 Patron: componente con input y output

```go
// Worker tiene un input (recibe tareas) y un output (envia resultados)
type Worker struct {
    id      int
    tasks   <-chan Task     // receive-only — solo puede LEER tareas
    results chan<- Result   // send-only — solo puede ESCRIBIR resultados
}

func NewWorker(id int, tasks <-chan Task, results chan<- Result) *Worker {
    return &Worker{id: id, tasks: tasks, results: results}
}

func (w *Worker) Run() {
    for task := range w.tasks {
        // Procesar task...
        w.results <- Result{TaskID: task.ID, WorkerID: w.id, Data: "done"}
    }
}

// El main o el coordinator tiene los channels bidireccionales
// y distribuye las vistas restringidas:
//
// tasks := make(chan Task, 100)
// results := make(chan Result, 100)
// worker := NewWorker(1, tasks, results)
// go worker.Run()
// 
// tasks <- Task{...}   // main envia tareas
// r := <-results       // main recibe resultados
// close(tasks)          // main cierra cuando no hay mas tareas
```

---

## 6. Direccion en Interfaces

Las interfaces de Go pueden incluir channels direccionales, reforzando contratos a nivel de abstraccion:

```go
package main

import (
    "fmt"
    "time"
)

// ============================================================
// Interfaces con channels direccionales
// ============================================================

// Producer solo necesita un channel de salida
type Producer interface {
    Produce(out chan<- string)
}

// Consumer solo necesita un channel de entrada
type Consumer interface {
    Consume(in <-chan string)
}

// Transformer necesita ambos (input y output), pero separados
type Transformer interface {
    Transform(in <-chan string, out chan<- string)
}

// ============================================================
// Implementaciones
// ============================================================

type FileReader struct {
    lines []string
}

func (f *FileReader) Produce(out chan<- string) {
    for _, line := range f.lines {
        out <- line
    }
    close(out)
}

type UpperCaser struct{}

func (u *UpperCaser) Transform(in <-chan string, out chan<- string) {
    for line := range in {
        out <- fmt.Sprintf("[%s] %s", time.Now().Format("15:04:05"), line)
    }
    close(out)
}

type Printer struct{}

func (p *Printer) Consume(in <-chan string) {
    for line := range in {
        fmt.Println(line)
    }
}

// ============================================================
// Pipeline builder que usa las interfaces
// ============================================================

func buildPipeline(p Producer, t Transformer, c Consumer) {
    ch1 := make(chan string)   // producer → transformer
    ch2 := make(chan string)   // transformer → consumer
    
    go p.Produce(ch1)          // chan string → chan<- string (implicito)
    go t.Transform(ch1, ch2)   // chan string → <-chan string, chan<- string
    c.Consume(ch2)             // chan string → <-chan string
}

func main() {
    reader := &FileReader{lines: []string{
        "server started",
        "connection accepted",
        "query executed",
        "response sent",
    }}
    transformer := &UpperCaser{}
    printer := &Printer{}
    
    buildPipeline(reader, transformer, printer)
}
```

---

## 7. Patrones Avanzados con Direccion

### 7.1 Channel factory — retornar channel configurado

```go
package main

import (
    "fmt"
    "time"
)

// monitor retorna un receive-only channel de alertas
// El caller no puede enviar alertas falsas ni cerrar el monitor
func monitor(target string, interval time.Duration, threshold float64) <-chan string {
    alerts := make(chan string, 5) // buffered para no bloquear el monitor
    
    go func() {
        ticker := time.NewTicker(interval)
        defer ticker.Stop()
        defer close(alerts)
        
        for i := 0; i < 10; i++ { // 10 checks para demo
            <-ticker.C
            // Simular check
            usage := float64(40 + i*7) // incrementa gradualmente
            if usage > threshold {
                alerts <- fmt.Sprintf("[ALERT] %s: usage %.1f%% > %.1f%%", target, usage, threshold)
            }
        }
    }()
    
    return alerts // conversion implicita chan string → <-chan string
}

func main() {
    // Crear monitores — cada uno retorna solo la vista de lectura
    cpuAlerts := monitor("CPU", 100*time.Millisecond, 70.0)
    memAlerts := monitor("Memory", 150*time.Millisecond, 80.0)
    
    // Merge alerts de multiples monitores
    for {
        select {
        case alert, ok := <-cpuAlerts:
            if !ok {
                cpuAlerts = nil // desactivar en select
                if memAlerts == nil {
                    return
                }
                continue
            }
            fmt.Println(alert)
        case alert, ok := <-memAlerts:
            if !ok {
                memAlerts = nil
                if cpuAlerts == nil {
                    return
                }
                continue
            }
            fmt.Println(alert)
        }
    }
}
```

### 7.2 Middleware de channels

```go
package main

import (
    "fmt"
    "time"
)

// rateLimit toma un receive-only channel y retorna otro receive-only channel
// que emite los mismos valores pero con rate limiting
func rateLimit[T any](in <-chan T, interval time.Duration) <-chan T {
    out := make(chan T)
    go func() {
        defer close(out)
        limiter := time.NewTicker(interval)
        defer limiter.Stop()
        for v := range in {
            <-limiter.C // esperar al siguiente tick
            out <- v
        }
    }()
    return out
}

// batch toma un receive-only channel y retorna otro que emite slices
// agrupando N elementos
func batch[T any](in <-chan T, size int) <-chan []T {
    out := make(chan []T)
    go func() {
        defer close(out)
        buf := make([]T, 0, size)
        for v := range in {
            buf = append(buf, v)
            if len(buf) == size {
                // Copiar el slice para evitar data races
                batch := make([]T, len(buf))
                copy(batch, buf)
                out <- batch
                buf = buf[:0]
            }
        }
        // Enviar el ultimo batch parcial
        if len(buf) > 0 {
            out <- buf
        }
    }()
    return out
}

// tee duplica un channel en dos (como el comando tee de Unix)
func tee[T any](in <-chan T) (<-chan T, <-chan T) {
    out1 := make(chan T)
    out2 := make(chan T)
    go func() {
        defer close(out1)
        defer close(out2)
        for v := range in {
            // Enviar a AMBOS — usa variables locales para select
            o1, o2 := out1, out2
            for o1 != nil || o2 != nil {
                select {
                case o1 <- v:
                    o1 = nil
                case o2 <- v:
                    o2 = nil
                }
            }
        }
    }()
    return out1, out2
}

func generate(n int) <-chan int {
    out := make(chan int)
    go func() {
        for i := 1; i <= n; i++ {
            out <- i
        }
        close(out)
    }()
    return out
}

func main() {
    // Pipeline: generate → rateLimit → batch
    nums := generate(12)
    limited := rateLimit(nums, 50*time.Millisecond)
    batched := batch(limited, 4)
    
    for b := range batched {
        fmt.Printf("Batch: %v\n", b)
    }
    // Output:
    // Batch: [1 2 3 4]    (despues de ~200ms)
    // Batch: [5 6 7 8]    (despues de ~200ms)
    // Batch: [9 10 11 12] (despues de ~200ms)
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    MIDDLEWARE DE CHANNELS                                 │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  El patron es:                                                           │
│                                                                          │
│  func middleware(in <-chan T) <-chan U {                                 │
│      out := make(chan U)                                                │
│      go func() {                                                         │
│          defer close(out)                                                │
│          for v := range in {                                             │
│              // transformar, filtrar, agrupar, limitar...               │
│              out <- transform(v)                                        │
│          }                                                               │
│      }()                                                                 │
│      return out                                                          │
│  }                                                                       │
│                                                                          │
│  TODOS los middlewares siguen este contrato:                            │
│  ├─ Input:  <-chan T (receive-only, no puede modificar la fuente)      │
│  ├─ Output: <-chan U (receive-only, el caller no puede enviar)         │
│  ├─ Goroutine interna que lee de in y escribe a out                    │
│  ├─ close(out) cuando in se cierra (propaga el cierre)                │
│  └─ Son composables: f(g(h(source)))                                   │
│                                                                          │
│  Middlewares comunes:                                                    │
│  ├─ rateLimit: controla velocidad                                      │
│  ├─ batch: agrupa N elementos                                          │
│  ├─ filter: descarta segun predicado                                   │
│  ├─ map/transform: transforma cada elemento                           │
│  ├─ tee: duplica el stream                                             │
│  ├─ take: solo los primeros N                                          │
│  ├─ skip: ignora los primeros N                                        │
│  ├─ debounce: emite solo si no hay otro en intervalo                   │
│  └─ buffer: agrupa por tiempo (flush cada N ms)                        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 7.3 Done pattern con close (broadcast)

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

// Cerrar un channel es un broadcast a TODOS los receivers.
// La direccion ayuda a separar quien puede enviar la senal de quien la recibe.

// Coordinator tiene el power de cerrar (send-side control)
type Coordinator struct {
    done chan struct{} // bidireccional — puede cerrar
}

func NewCoordinator() *Coordinator {
    return &Coordinator{done: make(chan struct{})}
}

func (c *Coordinator) Shutdown() {
    close(c.done) // broadcast a todos los workers
}

// DoneSignal retorna un receive-only channel
// Los workers NO pueden cerrar este channel — solo esperar
func (c *Coordinator) DoneSignal() <-chan struct{} {
    return c.done // conversion implicita
}

// Worker recibe el done signal como receive-only
func worker(id int, done <-chan struct{}, wg *sync.WaitGroup) {
    defer wg.Done()
    for {
        select {
        case <-done: // esperar senal de shutdown
            fmt.Printf("Worker %d: shutting down\n", id)
            return
        default:
            // Simular trabajo
            time.Sleep(100 * time.Millisecond)
            fmt.Printf("Worker %d: working...\n", id)
        }
    }
}

func main() {
    coord := NewCoordinator()
    
    var wg sync.WaitGroup
    for i := 1; i <= 3; i++ {
        wg.Add(1)
        go worker(i, coord.DoneSignal(), &wg) // recibe <-chan struct{}
    }
    
    time.Sleep(500 * time.Millisecond)
    fmt.Println("\n--- Sending shutdown signal ---")
    coord.Shutdown() // close(done) — broadcast
    
    wg.Wait()
    fmt.Println("All workers stopped")
    
    // Ningun worker puede llamar close(done) — compile error
    // Esto previene double-close panics
}
```

---

## 8. Errores Comunes con Direccion

### 8.1 Catalogo de errores de compilacion

```go
package main

// ERROR 1: Recibir de send-only channel
func bad1(ch chan<- int) {
    v := <-ch   // ERROR: invalid operation: receive from send-only type chan<- int
    _ = v
}

// ERROR 2: Enviar a receive-only channel
func bad2(ch <-chan int) {
    ch <- 42    // ERROR: invalid operation: send to receive-only type <-chan int
}

// ERROR 3: Cerrar receive-only channel
func bad3(ch <-chan int) {
    close(ch)   // ERROR: invalid operation: close of receive-only channel
}

// ERROR 4: Ampliar permisos
func bad4() {
    var sendOnly chan<- int = make(chan int)
    var ch chan int = sendOnly  // ERROR: cannot use sendOnly (variable of type chan<- int)
                                //        as chan int value in variable declaration
    _ = ch
}

// ERROR 5: range sobre send-only
func bad5(ch chan<- int) {
    for v := range ch {  // ERROR: invalid operation: range over send-only channel
        _ = v
    }
}
```

### 8.2 Errores logicos (compilan pero son bugs)

```go
// ERROR LOGICO 1: No anotar direccion — pierdes proteccion
func processNoDirection(ch chan int) {
    // Compila, pero el caller podria pasar un channel
    // que usa para enviar Y recibir, creando confusión
    <-ch      // ¿Es esta funcion un consumer?
    ch <- 42  // ¿O un producer? ¿O ambos?
    // Sin direccion, no hay contrato claro
}

// MEJOR:
func processConsumer(ch <-chan int) {
    // Claramente un consumer — solo lee
    for v := range ch {
        fmt.Println(v)
    }
}

// ERROR LOGICO 2: Retornar channel bidireccional de una factory
func badFactory() chan int {
    ch := make(chan int)
    go func() {
        ch <- 42
        close(ch)
    }()
    return ch // El caller puede enviar a este channel → BUG
    // Si envia despues del close → PANIC
}

// MEJOR:
func goodFactory() <-chan int {
    ch := make(chan int)
    go func() {
        ch <- 42
        close(ch)
    }()
    return ch // El caller solo puede leer — safe
}

// ERROR LOGICO 3: close() en chan<- aunque es valido, puede ser confuso
func sender(ch chan<- int) {
    ch <- 1
    ch <- 2
    close(ch)  // Valido y correcto — el sender DEBE cerrar
    // Pero si hay MULTIPLES senders, ¿quien cierra?
    // Solo UNO puede cerrar. Coordinar con sync.Once o un closer dedicado.
}
```

---

## 9. Comparacion con C y Rust

### 9.1 C — no hay concepto equivalente

C no tiene channels, por lo que no tiene direccionalidad. Los pipes de Unix tienen un concepto similar con file descriptors separados:

```c
#include <unistd.h>
#include <stdio.h>

int main() {
    int fd[2];
    pipe(fd);
    
    // fd[0] = lectura (como <-chan)
    // fd[1] = escritura (como chan<-)
    
    // Pero NO hay enforcement:
    write(fd[0], "bug", 3);  // Escribir al fd de LECTURA
    // ¡Compila y ejecuta! Resultado: comportamiento indefinido
    
    // En Go:
    // ch := make(chan int)
    // var readOnly <-chan int = ch
    // readOnly <- 42  // ERROR DE COMPILACION — imposible
    
    // C no puede prevenir este tipo de bugs en compile-time.
    // Solo puedes confiar en convencion y documentacion.
    
    close(fd[0]);
    close(fd[1]);
    return 0;
}

// Para "simular" direccion en C, usarias wrappers:
typedef struct {
    int fd;
} read_end;

typedef struct {
    int fd;
} write_end;

// Pero el compilador no enforce nada — puedes hacer cast
// y usar write() en un read_end. Es solo documentacion, no seguridad.
```

### 9.2 Rust — ownership y tipos afines

Rust no tiene exactamente la misma distincion, pero su sistema de ownership logra un resultado similar y mas fuerte:

```rust
use std::sync::mpsc;
use std::thread;

fn main() {
    let (tx, rx) = mpsc::channel::<i32>();
    // tx: Sender<i32>  — SOLO puede enviar (como chan<- int)
    // rx: Receiver<i32> — SOLO puede recibir (como <-chan int)
    
    // tx y rx son TIPOS DIFERENTES — no hay "channel bidireccional"
    // La separacion es estructural, no una anotacion opcional.
    
    // tx.recv()  → ERROR: method not found (no existe recv en Sender)
    // rx.send(1) → ERROR: method not found (no existe send en Receiver)
    
    // Ademas, Receiver NO implementa Clone:
    // let rx2 = rx.clone();  → ERROR: Clone not implemented for Receiver
    // Solo puede haber UN receiver (MPSC = Multiple Producer, Single Consumer)
    
    // Sender SI implementa Clone:
    let tx2 = tx.clone();  // OK — multiples producers
    
    thread::spawn(move || {
        tx.send(1).unwrap();   // send() retorna Result — puede fallar
    });
    
    thread::spawn(move || {
        tx2.send(2).unwrap();
    });
    
    println!("{}", rx.recv().unwrap()); // 1 o 2
    println!("{}", rx.recv().unwrap()); // el otro
    
    // Cuando todos los Senders se dropean, rx.recv() retorna Err
    // No hay close() explicito — RAII cierra automaticamente
}

// crossbeam-channel: MPMC (como Go)
// use crossbeam_channel::{bounded, Sender, Receiver};
// let (s, r) = bounded::<i32>(10);
// s y r son tipos separados, como en mpsc
// Pero r SI implementa Clone — multiples consumers (como Go)
```

### 9.3 Tabla comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    DIRECCION DE CHANNELS: GO vs C vs RUST                │
├──────────────────┬─────────────────┬───────────────┬─────────────────────┤
│ Aspecto           │ C               │ Go            │ Rust               │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Direccionalidad   │ No hay          │ Anotacion     │ Tipos separados   │
│                   │ (convencion)    │ (chan<-, <-ch) │ (Sender/Receiver)  │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Enforcement       │ Ninguno         │ Compile-time  │ Compile-time      │
│                   │                 │ (type system) │ (type system +     │
│                   │                 │               │  ownership)        │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Granularidad      │ N/A             │ send/recv/    │ send/recv son      │
│                   │                 │ close         │ metodos separados  │
│                   │                 │ (3 permisos)  │ en tipos separados │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Bidireccional     │ N/A             │ chan T         │ No existe          │
│                   │                 │ (por defecto) │ (siempre separado) │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Conversion        │ N/A             │ Implicita     │ No hay (son tipos │
│                   │                 │ (restringir)  │ diferentes desde   │
│                   │                 │               │ el inicio)         │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Multi-consumer    │ Manual          │ Si (MPMC)     │ mpsc: No          │
│                   │                 │               │ crossbeam: Si      │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Cierre            │ Manual flag     │ close() en    │ Drop(Sender)      │
│                   │                 │ chan/chan<-    │ (implicito, RAII)  │
│                   │                 │ (no en <-chan)│                     │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Error model       │ Undefined       │ Panic/block   │ Result (send/recv │
│                   │ behavior        │ (runtime)     │ retornan Result)   │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Filosofia         │ "confio en el   │ "anota tu     │ "el tipo system   │
│                   │  programador"   │  intencion"   │  lo decide por ti"│
└──────────────────┴─────────────────┴───────────────┴─────────────────────┘
```

---

## 10. Programa Completo — Infrastructure Log Pipeline

Un pipeline de procesamiento de logs con direccion estricta en cada componente. Demuestra channels direccionales en funciones, structs, interfaces, y composicion de middlewares:

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

type LogLevel int

const (
    DEBUG LogLevel = iota
    INFO
    WARN
    ERROR
    FATAL
)

func (l LogLevel) String() string {
    return [...]string{"DEBUG", "INFO", "WARN", "ERROR", "FATAL"}[l]
}

type LogEntry struct {
    Timestamp time.Time
    Level     LogLevel
    Service   string
    Message   string
    Fields    map[string]string
}

func (e LogEntry) String() string {
    fields := ""
    for k, v := range e.Fields {
        fields += fmt.Sprintf(" %s=%s", k, v)
    }
    return fmt.Sprintf("[%s] [%-5s] [%s] %s%s",
        e.Timestamp.Format("15:04:05.000"), e.Level, e.Service, e.Message, fields)
}

// ============================================================
// Source — genera logs (retorna receive-only)
// ============================================================

func logSource(ctx context.Context, service string, count int) <-chan LogEntry {
    out := make(chan LogEntry)
    
    levels := []LogLevel{DEBUG, DEBUG, INFO, INFO, INFO, WARN, ERROR, FATAL}
    messages := map[LogLevel][]string{
        DEBUG: {"cache hit", "query plan generated", "connection pool stats"},
        INFO:  {"request processed", "health check passed", "config reloaded"},
        WARN:  {"high memory usage", "slow query detected", "rate limit approaching"},
        ERROR: {"connection refused", "timeout exceeded", "disk space critical"},
        FATAL: {"out of memory", "unrecoverable state"},
    }
    
    go func() {
        defer close(out)
        for i := 0; i < count; i++ {
            level := levels[rand.Intn(len(levels))]
            msgs := messages[level]
            entry := LogEntry{
                Timestamp: time.Now(),
                Level:     level,
                Service:   service,
                Message:   msgs[rand.Intn(len(msgs))],
                Fields: map[string]string{
                    "request_id": fmt.Sprintf("req-%04d", rand.Intn(10000)),
                },
            }
            select {
            case out <- entry:
            case <-ctx.Done():
                return
            }
            time.Sleep(time.Duration(10+rand.Intn(40)) * time.Millisecond)
        }
    }()
    return out // conversion implicita: chan LogEntry → <-chan LogEntry
}

// ============================================================
// Middlewares — cada uno: <-chan LogEntry → <-chan LogEntry
// Misma firma, composables
// ============================================================

// filterByLevel: solo pasa logs >= minLevel
func filterByLevel(in <-chan LogEntry, minLevel LogLevel) <-chan LogEntry {
    out := make(chan LogEntry)
    go func() {
        defer close(out)
        for entry := range in {
            if entry.Level >= minLevel {
                out <- entry
            }
        }
    }()
    return out
}

// enrichWithHost: agrega campo "host" a cada log
func enrichWithHost(in <-chan LogEntry, hostname string) <-chan LogEntry {
    out := make(chan LogEntry)
    go func() {
        defer close(out)
        for entry := range in {
            if entry.Fields == nil {
                entry.Fields = make(map[string]string)
            }
            entry.Fields["host"] = hostname
            out <- entry
        }
    }()
    return out
}

// deduplicate: filtra mensajes duplicados dentro de una ventana de tiempo
func deduplicate(in <-chan LogEntry, window time.Duration) <-chan LogEntry {
    out := make(chan LogEntry)
    go func() {
        defer close(out)
        seen := make(map[string]time.Time)
        for entry := range in {
            key := fmt.Sprintf("%s:%s:%s", entry.Service, entry.Level, entry.Message)
            if lastSeen, exists := seen[key]; exists {
                if time.Since(lastSeen) < window {
                    continue // duplicado dentro de la ventana — skip
                }
            }
            seen[key] = time.Now()
            out <- entry
        }
    }()
    return out
}

// ============================================================
// Fan-in — merge multiples sources (receive-only → receive-only)
// ============================================================

func merge(ctx context.Context, channels ...<-chan LogEntry) <-chan LogEntry {
    out := make(chan LogEntry)
    var wg sync.WaitGroup
    
    // Un goroutine por channel de entrada
    for _, ch := range channels {
        wg.Add(1)
        go func(c <-chan LogEntry) {
            defer wg.Done()
            for entry := range c {
                select {
                case out <- entry:
                case <-ctx.Done():
                    return
                }
            }
        }(ch)
    }
    
    // Cerrar out cuando todos los inputs se cierren
    go func() {
        wg.Wait()
        close(out)
    }()
    
    return out
}

// ============================================================
// Tee — duplicar stream (receive-only → 2 receive-only)
// ============================================================

func tee(in <-chan LogEntry) (<-chan LogEntry, <-chan LogEntry) {
    out1 := make(chan LogEntry)
    out2 := make(chan LogEntry)
    go func() {
        defer close(out1)
        defer close(out2)
        for entry := range in {
            // Enviar a ambos — necesita select para no bloquear si uno es lento
            o1, o2 := out1, out2
            for o1 != nil || o2 != nil {
                select {
                case o1 <- entry:
                    o1 = nil
                case o2 <- entry:
                    o2 = nil
                }
            }
        }
    }()
    return out1, out2
}

// ============================================================
// Sinks — consumen logs (reciben receive-only)
// ============================================================

// Sink interface — cualquier cosa que consuma logs
type Sink interface {
    Write(in <-chan LogEntry)
}

// ConsoleSink — imprime a stdout
type ConsoleSink struct {
    name string
}

func (s *ConsoleSink) Write(in <-chan LogEntry) {
    for entry := range in {
        fmt.Printf("  [%s] %s\n", s.name, entry)
    }
}

// CounterSink — cuenta logs por nivel
type CounterSink struct {
    name   string
    counts map[LogLevel]int
    mu     sync.Mutex
    done   chan struct{}
}

func NewCounterSink(name string) *CounterSink {
    return &CounterSink{
        name:   name,
        counts: make(map[LogLevel]int),
        done:   make(chan struct{}),
    }
}

func (s *CounterSink) Write(in <-chan LogEntry) {
    for entry := range in {
        s.mu.Lock()
        s.counts[entry.Level]++
        s.mu.Unlock()
    }
    close(s.done)
}

func (s *CounterSink) Report() {
    <-s.done // esperar a que termine de procesar
    fmt.Printf("\n  [%s] Log counts:\n", s.name)
    for level := DEBUG; level <= FATAL; level++ {
        s.mu.Lock()
        count := s.counts[level]
        s.mu.Unlock()
        if count > 0 {
            bar := strings.Repeat("█", count)
            fmt.Printf("    %-5s: %s (%d)\n", level, bar, count)
        }
    }
}

// ============================================================
// Main — construir el pipeline completo
// ============================================================

func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
    defer cancel()

    fmt.Println("╔══════════════════════════════════════════════════════════════╗")
    fmt.Println("║           INFRASTRUCTURE LOG PIPELINE                        ║")
    fmt.Println("╠══════════════════════════════════════════════════════════════╣")
    fmt.Println("║  Direction annotations on every channel parameter           ║")
    fmt.Println("╚══════════════════════════════════════════════════════════════╝")

    //  Pipeline architecture:
    //
    //  ┌─────────────┐   ┌─────────────┐   ┌─────────────┐
    //  │ api-server   │   │ database     │   │ cache        │
    //  │ (source)     │   │ (source)     │   │ (source)     │
    //  │ ret: <-chan   │   │ ret: <-chan   │   │ ret: <-chan   │
    //  └──────┬───────┘   └──────┬───────┘   └──────┬───────┘
    //         │                  │                   │
    //         └────────┬─────────┴─────────┬─────────┘
    //                  │                   │
    //           ┌──────▼───────┐           │
    //           │ merge()      │ ◄─────────┘
    //           │ in: ...<-chan │
    //           │ ret: <-chan   │
    //           └──────┬───────┘
    //                  │
    //           ┌──────▼───────┐
    //           │ filter(WARN) │
    //           │ in: <-chan    │
    //           │ ret: <-chan   │
    //           └──────┬───────┘
    //                  │
    //           ┌──────▼───────┐
    //           │ enrich(host) │
    //           │ in: <-chan    │
    //           │ ret: <-chan   │
    //           └──────┬───────┘
    //                  │
    //           ┌──────▼───────┐
    //           │ deduplicate  │
    //           │ in: <-chan    │
    //           │ ret: <-chan   │
    //           └──────┬───────┘
    //                  │
    //           ┌──────▼───────┐
    //           │ tee()        │
    //           │ in: <-chan    │
    //           │ ret: <-chan ×2│
    //           └──┬───────┬───┘
    //              │       │
    //        ┌─────▼──┐ ┌──▼──────┐
    //        │Console │ │Counter  │
    //        │ Sink   │ │ Sink    │
    //        │in:<-ch │ │in:<-ch  │
    //        └────────┘ └─────────┘

    fmt.Println("\n--- Starting pipeline ---\n")

    // 1. Sources (cada una retorna <-chan LogEntry)
    apiLogs := logSource(ctx, "api-server", 15)
    dbLogs := logSource(ctx, "database", 10)
    cacheLogs := logSource(ctx, "cache", 8)

    // 2. Fan-in: merge 3 sources → 1 stream
    merged := merge(ctx, apiLogs, dbLogs, cacheLogs)

    // 3. Middleware pipeline (cada uno: <-chan → <-chan)
    filtered := filterByLevel(merged, WARN)            // solo WARN+
    enriched := enrichWithHost(filtered, "prod-web-01") // agregar host
    deduped := deduplicate(enriched, 500*time.Millisecond)  // dedup 500ms

    // 4. Tee: duplicar para 2 sinks
    toConsole, toCounter := tee(deduped)

    // 5. Sinks (cada uno recibe <-chan LogEntry)
    consoleSink := &ConsoleSink{name: "STDOUT"}
    counterSink := NewCounterSink("STATS")

    var wg sync.WaitGroup
    wg.Add(2)
    go func() {
        defer wg.Done()
        consoleSink.Write(toConsole)
    }()
    go func() {
        defer wg.Done()
        counterSink.Write(toCounter)
    }()

    wg.Wait()

    // 6. Report
    counterSink.Report()

    fmt.Println("\n--- Pipeline complete ---")
    fmt.Println()
    fmt.Println("Direction annotations used:")
    fmt.Println("  logSource()       → ret: <-chan LogEntry")
    fmt.Println("  filterByLevel()   → in: <-chan, ret: <-chan")
    fmt.Println("  enrichWithHost()  → in: <-chan, ret: <-chan")
    fmt.Println("  deduplicate()     → in: <-chan, ret: <-chan")
    fmt.Println("  merge()           → in: ...<-chan, ret: <-chan")
    fmt.Println("  tee()             → in: <-chan, ret: (<-chan, <-chan)")
    fmt.Println("  Sink.Write()      → in: <-chan")
    fmt.Println("  NINGUNA funcion tiene chan T bidireccional en su firma")
}
```

### Ejecucion y explicacion

```bash
$ go run main.go

╔══════════════════════════════════════════════════════════════╗
║           INFRASTRUCTURE LOG PIPELINE                        ║
╠══════════════════════════════════════════════════════════════╣
║  Direction annotations on every channel parameter           ║
╚══════════════════════════════════════════════════════════════╝

--- Starting pipeline ---

  [STDOUT] [14:32:01.234] [WARN ] [api-server] high memory usage request_id=req-0042 host=prod-web-01
  [STDOUT] [14:32:01.289] [ERROR] [database] connection refused request_id=req-7721 host=prod-web-01
  [STDOUT] [14:32:01.334] [WARN ] [cache] rate limit approaching request_id=req-1155 host=prod-web-01
  [STDOUT] [14:32:01.401] [ERROR] [api-server] timeout exceeded request_id=req-8834 host=prod-web-01
  [STDOUT] [14:32:01.456] [FATAL] [database] out of memory request_id=req-0091 host=prod-web-01
  [STDOUT] [14:32:01.512] [WARN ] [api-server] slow query detected request_id=req-3302 host=prod-web-01
  [STDOUT] [14:32:01.567] [ERROR] [cache] connection refused request_id=req-5518 host=prod-web-01

  [STATS] Log counts:
    WARN : ███ (3)
    ERROR: ███ (3)
    FATAL: █ (1)

--- Pipeline complete ---

Direction annotations used:
  logSource()       → ret: <-chan LogEntry
  filterByLevel()   → in: <-chan, ret: <-chan
  enrichWithHost()  → in: <-chan, ret: <-chan
  deduplicate()     → in: <-chan, ret: <-chan
  merge()           → in: ...<-chan, ret: <-chan
  tee()             → in: <-chan, ret: (<-chan, <-chan)
  Sink.Write()      → in: <-chan
  NINGUNA funcion tiene chan T bidireccional en su firma
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ANALISIS DE DIRECCION EN EL PROGRAMA                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  El programa tiene 7 funciones/metodos con channels.                   │
│  NINGUNO usa chan T bidireccional en su firma publica.                  │
│                                                                          │
│  Internamente, cada funcion crea make(chan T) bidireccional,           │
│  pero SOLO lo expone como <-chan T o chan<- T.                          │
│                                                                          │
│  CONSECUENCIAS:                                                          │
│                                                                          │
│  1. Un source NO puede leer lo que otro source envia                   │
│     (retorna <-chan, no tiene acceso bidireccional)                     │
│                                                                          │
│  2. Un middleware NO puede enviar datos "hacia atras"                   │
│     (su input es <-chan, su output es <-chan)                           │
│                                                                          │
│  3. Un sink NO puede inyectar datos en el pipeline                     │
│     (recibe <-chan, no puede enviar)                                    │
│                                                                          │
│  4. Solo el CREADOR del channel puede cerrarlo                         │
│     (la goroutine interna que tiene el chan T bidireccional)            │
│                                                                          │
│  5. Si alguien intenta violar estas reglas, el COMPILADOR lo impide    │
│     No necesitas tests, code review, ni documentacion para esto        │
│                                                                          │
│  TOTAL de bugs de channel prevenidos en compile-time:                  │
│  ├─ Send accidental a un input channel                                 │
│  ├─ Receive accidental de un output channel                            │
│  ├─ Close desde un consumer/middleware                                  │
│  ├─ Flujo de datos en direccion equivocada                             │
│  └─ Double close (solo un punto de cierre por channel)                 │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 11. Ejercicios

### Ejercicio 1: Pipeline con direccion estricta

Crea un pipeline de procesamiento de metricas con 4 etapas, cada una con channels direccionales:

```
[Collector] ──<-chan──► [Normalizer] ──<-chan──► [Aggregator] ──<-chan──► [Exporter]
```

- **Collector**: retorna `<-chan Metric` — genera metricas simuladas (CPU, memory, disk)
- **Normalizer**: recibe `<-chan Metric`, retorna `<-chan Metric` — normaliza valores a porcentaje (0-100)
- **Aggregator**: recibe `<-chan Metric`, retorna `<-chan AggregatedMetric` — calcula min/max/avg por ventana de 5 metricas
- **Exporter**: recibe `<-chan AggregatedMetric` — imprime en formato de tabla

Requisito: ninguna funcion debe tener `chan T` bidireccional en su firma.

### Ejercicio 2: Multi-sink con tee chain

Implementa una funcion `teeN` que duplica un channel en N copias:

```go
func teeN[T any](in <-chan T, n int) []<-chan T
```

- Cada copia es un `<-chan T` independiente
- Si un consumer es lento, no debe bloquear a los demas (usa buffered channels internos)
- Demuestra con 4 sinks que reciben el mismo stream de datos
- Cada sink procesa a diferente velocidad

### Ejercicio 3: Channel adapter

Crea adaptadores que convierten entre tipos de datos a traves de channels:

```go
// Convierte un <-chan string a <-chan LogEntry (parsing)
func parseAdapter(in <-chan string) <-chan LogEntry

// Convierte un <-chan LogEntry a <-chan string (serialization)
func serializeAdapter(in <-chan LogEntry) <-chan string

// Convierte un <-chan LogEntry a chan<- LogEntry (tee + discard original)
// — este es el unico que "invierte" la direccion internamente
func sinkAdapter(in <-chan LogEntry) chan<- LogEntry
```

Demuestra composicion: `source → parse → process → serialize → sink`

### Ejercicio 4: Registry de channels tipados

Implementa un registry que gestiona channels con direccion:

```go
type ChannelRegistry struct {
    // ...
}

func (r *ChannelRegistry) RegisterProducer(name string) chan<- Event
func (r *ChannelRegistry) RegisterConsumer(name string) <-chan Event
func (r *ChannelRegistry) Shutdown()
```

- Multiples producers pueden registrarse (cada uno recibe `chan<- Event`)
- Multiples consumers pueden registrarse (cada uno recibe `<-chan Event`)
- Internamente, el registry hace fan-in de producers y broadcast a consumers
- Shutdown cierra todos los channels ordenadamente
- Demuestra con 3 producers y 2 consumers

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    DIRECCION DE CHANNELS — RESUMEN COMPLETO              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  TIPOS:                                                                  │
│  ├─ chan T       bidireccional (send + receive + close)                 │
│  ├─ chan<- T     send-only (send + close)                               │
│  └─ <-chan T     receive-only (receive + range)                         │
│                                                                          │
│  CONVERSION:                                                             │
│  ├─ chan T → chan<- T     OK (implicita, restringir)                    │
│  ├─ chan T → <-chan T     OK (implicita, restringir)                    │
│  ├─ chan<- T → chan T     ERROR (ampliar permisos)                      │
│  ├─ <-chan T → chan T     ERROR (ampliar permisos)                      │
│  └─ chan<- T ↔ <-chan T  ERROR (cambiar direccion)                      │
│                                                                          │
│  CUANDO ANOTAR:                                                          │
│  ├─ SIEMPRE en parametros de funciones                                  │
│  ├─ SIEMPRE en retornos de generators/factories                        │
│  ├─ SIEMPRE en campos de structs                                        │
│  └─ NO en variables locales (cuando necesitas bidireccional)           │
│                                                                          │
│  PATRONES:                                                               │
│  ├─ Generator: func f() <-chan T                                       │
│  ├─ Producer:  func p(out chan<- T)                                    │
│  ├─ Consumer:  func c(in <-chan T)                                     │
│  ├─ Middleware: func m(in <-chan T) <-chan U                            │
│  ├─ Fan-in:   func merge(...<-chan T) <-chan T                         │
│  ├─ Tee:      func tee(in <-chan T) (<-chan T, <-chan T)               │
│  └─ Done:     func DoneSignal() <-chan struct{}                        │
│                                                                          │
│  REGLA DE ORO:                                                           │
│  "Cada componente debe tener el minimo acceso necesario al channel."   │
│                                                                          │
│  CLOSE:                                                                  │
│  ├─ Permitido en chan T y chan<- T (el SENDER cierra)                   │
│  ├─ Prohibido en <-chan T (el RECEIVER no cierra)                      │
│  └─ Solo UN punto de cierre por channel (evita double-close panic)     │
│                                                                          │
│  vs C: no hay enforcement (convencion, fd[0]/fd[1])                    │
│  vs Rust: Sender<T>/Receiver<T> son tipos separados (no hay bidir)     │
│  Go: anotacion opcional pero altamente recomendada, compile-time       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S02 - Channels, T03 - select — multiplexacion de channels, default case, timeout con time.After
