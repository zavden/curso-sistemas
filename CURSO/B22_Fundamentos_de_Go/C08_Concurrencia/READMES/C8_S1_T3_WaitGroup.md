# WaitGroup — sync.WaitGroup, Add, Done, Wait — Coordinacion Basica

## 1. Introduccion

Cuando lanzas goroutines con `go f()`, la goroutine que las lanza **no espera** a que terminen. Si `main` retorna antes de que las goroutines completen, el programa termina y las goroutines mueren sin aviso. Necesitas un mecanismo para decir: "espera a que estas N goroutines terminen".

`sync.WaitGroup` es la herramienta mas basica de coordinacion en Go. Hace exactamente una cosa: **contar goroutines pendientes y bloquear hasta que todas terminen**. Es un contador atomico con tres operaciones: `Add` (incrementar), `Done` (decrementar), y `Wait` (bloquear hasta que el contador llegue a cero).

```go
// El patron fundamental:
var wg sync.WaitGroup

wg.Add(3)       // "voy a lanzar 3 goroutines"

go func() {
    defer wg.Done()  // "esta goroutine termino" (decrementa en 1)
    doWork1()
}()

go func() {
    defer wg.Done()
    doWork2()
}()

go func() {
    defer wg.Done()
    doWork3()
}()

wg.Wait()  // bloquea hasta que el contador llegue a 0
fmt.Println("All done")
```

Para SysAdmin/DevOps, `WaitGroup` es la primera herramienta que usas cuando necesitas:
- Esperar a que N health checks paralelos completen antes de reportar estado
- Sincronizar la finalizacion de workers antes de un shutdown limpio
- Asegurar que todas las goroutines de cleanup terminaron antes de salir
- Coordinar fases de un deployment (primero todos los pre-checks, luego todos los deploys)

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    EL PROBLEMA QUE RESUELVE WAITGROUP                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SIN WaitGroup:                      CON WaitGroup:                     │
│  ──────────────                      ─────────────                      │
│                                                                          │
│  go task1()                          var wg sync.WaitGroup              │
│  go task2()                          wg.Add(2)                          │
│  go task3()                          go func() {                        │
│                                          defer wg.Done()                │
│  // ¿como sabemos cuando                task1()                        │
│  //  terminaron?                     }()                                │
│                                      go func() {                        │
│  time.Sleep(5*time.Second)               defer wg.Done()                │
│  // ← HACK: ¿5 segundos es             task2()                        │
│  //    suficiente? ¿demasiado?       }()                                │
│  //    NUNCA sabemos                 wg.Wait() // espera EXACTA        │
│                                                                          │
│  ALTERNATIVAS:                                                           │
│  ├─ time.Sleep → NUNCA correcto (puede ser mucho o poco)               │
│  ├─ Channel manual → funciona pero mas boilerplate                     │
│  ├─ WaitGroup → simple, correcto, idiomatico                           │
│  └─ errgroup → WaitGroup + error handling (visto en C07)               │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. API de sync.WaitGroup

### 2.1 Los Tres Metodos

```go
import "sync"

var wg sync.WaitGroup

// Add(delta int) — ajustar el contador
wg.Add(1)   // incrementar en 1 ("una goroutine mas por esperar")
wg.Add(5)   // incrementar en 5 ("cinco goroutines mas")
wg.Add(-1)  // decrementar en 1 (equivalente a Done, pero raro)

// Done() — decrementar el contador en 1
wg.Done()   // equivalente a wg.Add(-1)
// Tipicamente llamado con defer al inicio de la goroutine

// Wait() — bloquear hasta que el contador sea 0
wg.Wait()   // bloquea la goroutine actual
// Retorna inmediatamente si el contador ya es 0
```

### 2.2 Contrato de WaitGroup

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CONTRATO DE WAITGROUP                                 │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  REGLAS:                                                                │
│                                                                          │
│  1. Add DEBE llamarse ANTES de go (en la goroutine que lanza)          │
│     ✅ wg.Add(1); go func() { defer wg.Done(); ... }()                 │
│     ⚠️ go func() { wg.Add(1); defer wg.Done(); ... }()  ← RACE       │
│     (Wait podria ver 0 antes de que Add se ejecute)                    │
│                                                                          │
│  2. Done DEBE llamarse exactamente una vez por cada Add(1)             │
│     ✅ defer wg.Done() al inicio de cada goroutine                     │
│     ⚠️ Olvidar Done → Wait bloquea para siempre (deadlock)            │
│     ⚠️ Done de mas → panic: "negative WaitGroup counter"              │
│                                                                          │
│  3. Wait bloquea hasta que el contador llega a 0                       │
│     ├─ Multiples goroutines pueden llamar Wait simultaneamente         │
│     ├─ Todas se desbloquean cuando el contador llega a 0               │
│     └─ Wait retorna inmediatamente si el contador ya es 0              │
│                                                                          │
│  4. No reusar un WaitGroup mientras Wait esta activo                   │
│     ⚠️ Llamar Add despues de que Wait empezo a esperar es un bug      │
│     ✅ Crear un nuevo WaitGroup para cada batch de goroutines          │
│                                                                          │
│  5. No copiar un WaitGroup despues de primer uso                       │
│     ⚠️ wg2 := wg  ← COPIA EL VALOR, NO LA REFERENCIA                │
│     ⚠️ func f(wg sync.WaitGroup) ← COPIA (pasar por puntero)         │
│     ✅ func f(wg *sync.WaitGroup)                                      │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.3 Internos de WaitGroup

```go
// Pseudocodigo simplificado de la implementacion real:
// (runtime/sema.go + sync/waitgroup.go)

type WaitGroup struct {
    // state contiene el counter y el waiter count empaquetados
    // en un uint64 para operaciones atomicas
    state atomic.Uint64 // [counter: 32 bits | waiters: 32 bits]
    sema  uint32        // semaforo para bloquear/desbloquear waiters
}

func (wg *WaitGroup) Add(delta int) {
    // Sumar delta al counter atomicamente
    state := wg.state.Add(uint64(delta) << 32)
    counter := int32(state >> 32)
    waiters := uint32(state)

    if counter < 0 {
        panic("sync: negative WaitGroup counter")
    }

    // Si el counter llego a 0 y hay waiters, despertarlos
    if counter == 0 && waiters > 0 {
        wg.state.Store(0) // reset
        for ; waiters > 0; waiters-- {
            runtime_Semrelease(&wg.sema) // despertar cada waiter
        }
    }
}

func (wg *WaitGroup) Done() {
    wg.Add(-1) // Done es literalmente Add(-1)
}

func (wg *WaitGroup) Wait() {
    for {
        state := wg.state.Load()
        counter := int32(state >> 32)
        if counter == 0 {
            return // ya esta en 0, no esperar
        }
        // Incrementar waiters y dormir en el semaforo
        if wg.state.CompareAndSwap(state, state+1) {
            runtime_Semacquire(&wg.sema) // bloquear hasta que alguien haga Semrelease
            return
        }
    }
}
```

La implementacion real usa operaciones atomicas (`atomic.Uint64`) y un semaforo del runtime (`runtime_Semacquire`/`runtime_Semrelease`) para bloquear y despertar goroutines de forma eficiente, sin mutex.

---

## 3. Patrones Fundamentales

### 3.1 Patron Basico: Goroutines en Loop

```go
func checkServers(servers []string) {
    var wg sync.WaitGroup

    for _, server := range servers {
        server := server // captura (Go < 1.22)
        wg.Add(1)
        go func() {
            defer wg.Done()
            status := pingServer(server)
            fmt.Printf("%s: %s\n", server, status)
        }()
    }

    wg.Wait()
    fmt.Println("All servers checked")
}
```

### 3.2 Patron: Add en Batch vs Add Individual

```go
// OPCION A: Add en batch (antes del loop)
func batchAdd(items []string) {
    var wg sync.WaitGroup
    wg.Add(len(items)) // agregar todos de una vez

    for _, item := range items {
        item := item
        go func() {
            defer wg.Done()
            process(item)
        }()
    }

    wg.Wait()
}

// OPCION B: Add individual (dentro del loop)
func individualAdd(items []string) {
    var wg sync.WaitGroup

    for _, item := range items {
        item := item
        wg.Add(1) // agregar uno por uno
        go func() {
            defer wg.Done()
            process(item)
        }()
    }

    wg.Wait()
}

// Ambos son correctos. La diferencia:
// Batch Add: ligeramente mas eficiente (una sola operacion atomica)
// Individual Add: mas flexible (puedes condicionar si lanzar la goroutine)

// OPCION C: Add condicional
func conditionalAdd(items []string) {
    var wg sync.WaitGroup

    for _, item := range items {
        if shouldProcess(item) {
            item := item
            wg.Add(1)
            go func() {
                defer wg.Done()
                process(item)
            }()
        }
    }

    wg.Wait()
}
```

### 3.3 Patron: Recolectar Resultados

WaitGroup solo coordina **cuando** terminan las goroutines — no recolecta resultados. Para recolectar, combinas WaitGroup con un mecanismo de almacenamiento:

```go
// Metodo 1: Slice por indice (sin mutex — cada goroutine escribe a un indice diferente)
func checkAllByIndex(servers []string) []Result {
    results := make([]Result, len(servers))
    var wg sync.WaitGroup

    for i, server := range servers {
        i, server := i, server
        wg.Add(1)
        go func() {
            defer wg.Done()
            results[i] = checkServer(server) // cada goroutine escribe a SU indice
        }()
    }

    wg.Wait()
    return results // seguro: Wait garantiza que todas escribieron
}

// Metodo 2: Mutex para mapa compartido
func checkAllByMap(servers []string) map[string]Result {
    results := make(map[string]Result)
    var mu sync.Mutex
    var wg sync.WaitGroup

    for _, server := range servers {
        server := server
        wg.Add(1)
        go func() {
            defer wg.Done()
            result := checkServer(server)
            mu.Lock()
            results[server] = result
            mu.Unlock()
        }()
    }

    wg.Wait()
    return results
}

// Metodo 3: Channel para streaming
func checkAllStream(servers []string) <-chan Result {
    ch := make(chan Result, len(servers))
    var wg sync.WaitGroup

    for _, server := range servers {
        server := server
        wg.Add(1)
        go func() {
            defer wg.Done()
            ch <- checkServer(server)
        }()
    }

    // Cerrar el channel cuando todas terminen
    go func() {
        wg.Wait()
        close(ch)
    }()

    return ch
}

// Uso del streaming:
for result := range checkAllStream(servers) {
    fmt.Println(result)
}
```

### 3.4 Patron: Fases Secuenciales

```go
func deploy(ctx context.Context, services []Service) error {
    // Fase 1: Validar todos en paralelo
    var wg sync.WaitGroup
    var mu sync.Mutex
    var errs []error

    for _, svc := range services {
        svc := svc
        wg.Add(1)
        go func() {
            defer wg.Done()
            if err := validate(ctx, svc); err != nil {
                mu.Lock()
                errs = append(errs, err)
                mu.Unlock()
            }
        }()
    }
    wg.Wait()

    if len(errs) > 0 {
        return fmt.Errorf("validation failed: %w", errors.Join(errs...))
    }

    // Fase 2: Deploy todos en paralelo (solo si validacion paso)
    // Nuevo WaitGroup para la siguiente fase
    for _, svc := range services {
        svc := svc
        wg.Add(1)
        go func() {
            defer wg.Done()
            if err := deployService(ctx, svc); err != nil {
                mu.Lock()
                errs = append(errs, err)
                mu.Unlock()
            }
        }()
    }
    wg.Wait()

    if len(errs) > 0 {
        return fmt.Errorf("deploy failed: %w", errors.Join(errs...))
    }

    return nil
}
```

### 3.5 Patron: WaitGroup + Semaforo (Limitar Concurrencia)

```go
// WaitGroup no tiene SetLimit — combinar con un semaforo (channel):
func processWithLimit(items []string, maxConcurrent int) {
    var wg sync.WaitGroup
    sem := make(chan struct{}, maxConcurrent) // semaforo con buffer

    for _, item := range items {
        item := item
        wg.Add(1)
        sem <- struct{}{} // bloquea si hay maxConcurrent goroutines activas

        go func() {
            defer wg.Done()
            defer func() { <-sem }() // liberar slot al terminar

            process(item)
        }()
    }

    wg.Wait()
}

// ¿Por que no usar errgroup.SetLimit?
// - Si no necesitas error handling, WaitGroup + sem es mas ligero
// - Si necesitas error handling, usa errgroup (ya visto en C07)
// - Si necesitas TryGo (non-blocking), usa errgroup
```

---

## 4. Errores Comunes y Antipatrones

### 4.1 Add Dentro de la Goroutine (Race Condition)

```go
// ⚠️ BUG: Add dentro de la goroutine — race con Wait
func buggyAdd() {
    var wg sync.WaitGroup

    for i := 0; i < 5; i++ {
        go func() {
            wg.Add(1) // ← RACE: puede ejecutarse DESPUES de Wait
            defer wg.Done()
            doWork()
        }()
    }

    wg.Wait() // puede ver counter=0 antes de que todas las goroutines hagan Add
}
// RESULTADO: Wait retorna antes de que todas terminen

// ✅ SOLUCION: Add ANTES de go
func correctAdd() {
    var wg sync.WaitGroup

    for i := 0; i < 5; i++ {
        wg.Add(1) // ← CORRECTO: en la goroutine que lanza
        go func() {
            defer wg.Done()
            doWork()
        }()
    }

    wg.Wait()
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    POR QUE Add DENTRO DE go ES UN BUG                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Goroutine principal:        Goroutine lanzada:                         │
│  ─────────────────           ──────────────────                          │
│  go func() {                                                             │
│      wg.Add(1)  ←──── ¿cuando se ejecuta?                              │
│      ...                                                                 │
│  }()                                                                     │
│                                                                          │
│  wg.Wait()  ←──── puede llegar aqui ANTES de que Add se ejecute        │
│                                                                          │
│  TIMELINE POSIBLE:                                                       │
│  1. main: go func1()  → crea goroutine 1 (aun no ejecuta)             │
│  2. main: go func2()  → crea goroutine 2 (aun no ejecuta)             │
│  3. main: wg.Wait()   → counter es 0 → retorna inmediatamente!        │
│  4. goroutine 1: wg.Add(1)  ← demasiado tarde, Wait ya retorno        │
│  5. goroutine 2: wg.Add(1)                                             │
│  6. main: ya salio del scope, goroutines quedan huerfanas              │
│                                                                          │
│  SOLUCION: Add(1) ANTES de go → garantiza que Wait ve el counter > 0  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Olvidar Done (Deadlock)

```go
// ⚠️ BUG: si una goroutine no llama Done, Wait bloquea para siempre
func deadlock() {
    var wg sync.WaitGroup
    wg.Add(1)

    go func() {
        // olvidamos wg.Done()
        doWork()
    }()

    wg.Wait() // ← DEADLOCK: counter nunca llega a 0
    // El programa se congela. Go detecta deadlock global:
    // "fatal error: all goroutines are asleep - deadlock!"
    // (solo si TODAS las goroutines estan bloqueadas)
}

// ✅ SOLUCION: SIEMPRE usar defer wg.Done() como primera linea
func neverForget() {
    var wg sync.WaitGroup
    wg.Add(1)

    go func() {
        defer wg.Done() // PRIMERA linea — imposible olvidarlo
        doWork()         // incluso si doWork() hace panic, defer se ejecuta
    }()

    wg.Wait()
}
```

### 4.3 Done de Mas (Panic)

```go
// ⚠️ PANIC: counter negativo
func tooManyDones() {
    var wg sync.WaitGroup
    wg.Add(1)

    go func() {
        defer wg.Done()
        doWork()
        wg.Done() // ← OOPS: Done se llama DOS veces (defer + explicito)
    }()

    wg.Wait()
    // panic: sync: negative WaitGroup counter
}

// ✅ SOLUCION: solo defer wg.Done(), nunca Done explicito adicional
```

### 4.4 Copiar un WaitGroup

```go
// ⚠️ BUG: copiar WaitGroup copia el estado, no la referencia
func badCopy() {
    var wg sync.WaitGroup
    wg.Add(1)

    go func(wg sync.WaitGroup) { // ← COPIA del WaitGroup
        defer wg.Done() // Done en la COPIA, no en el original
        doWork()
    }(wg)

    wg.Wait() // ← espera en el ORIGINAL, que nunca recibe Done
    // DEADLOCK (o panic si go vet lo detecta)
}

// ✅ SOLUCION: pasar por puntero
func goodPointer() {
    var wg sync.WaitGroup
    wg.Add(1)

    go func(wg *sync.WaitGroup) { // ← puntero
        defer wg.Done() // Done en el ORIGINAL
        doWork()
    }(&wg)

    wg.Wait()
}

// ✅ MEJOR: closure captura por referencia (lo mas comun)
func bestClosure() {
    var wg sync.WaitGroup
    wg.Add(1)

    go func() {
        defer wg.Done() // captura wg del scope — es la misma variable
        doWork()
    }()

    wg.Wait()
}
```

`go vet` detecta copias de WaitGroup:

```bash
go vet ./...
# main.go:15:20: badCopy passes lock by value: sync.WaitGroup contains sync.noCopy
```

### 4.5 Reusar WaitGroup Durante Wait

```go
// ⚠️ BUG: Add despues de que Wait empezo
func reuseWhileWaiting() {
    var wg sync.WaitGroup

    wg.Add(1)
    go func() {
        defer wg.Done()
        doWork()

        // Lanzar MAS trabajo despues de que la goroutine principal ya esta en Wait
        wg.Add(1) // ← BUG: Add mientras Wait esta bloqueado
        go func() {
            defer wg.Done()
            moreWork()
        }()
    }()

    wg.Wait() // ← puede retornar antes de que moreWork termine
}

// ✅ SOLUCION: nuevo WaitGroup para cada batch
func separateBatches() {
    // Batch 1
    var wg1 sync.WaitGroup
    wg1.Add(1)
    go func() {
        defer wg1.Done()
        doWork()
    }()
    wg1.Wait()

    // Batch 2 (despues de que batch 1 termino)
    var wg2 sync.WaitGroup
    wg2.Add(1)
    go func() {
        defer wg2.Done()
        moreWork()
    }()
    wg2.Wait()
}
```

### 4.6 Tabla de Antipatrones

```
┌─────────────────────────────────────┬─────────────────────────────────────┐
│  ANTIPATRON                         │  CONSECUENCIA + SOLUCION            │
├─────────────────────────────────────┼─────────────────────────────────────┤
│ Add dentro de goroutine lanzada     │ Race: Wait retorna antes           │
│                                     │ → Add ANTES de go                  │
│                                     │                                     │
│ Olvidar Done                        │ Deadlock: Wait bloquea siempre     │
│                                     │ → SIEMPRE defer wg.Done()          │
│                                     │                                     │
│ Done de mas                         │ Panic: "negative counter"          │
│                                     │ → Solo defer, nunca Done extra     │
│                                     │                                     │
│ Copiar WaitGroup (valor)            │ Done en copia, Wait en original    │
│                                     │ → Puntero o closure                │
│                                     │                                     │
│ Add durante Wait activo             │ Race: Wait puede retornar antes    │
│                                     │ → Nuevo WaitGroup por batch        │
│                                     │                                     │
│ Add con delta incorrecto            │ Counter no coincide con goroutines │
│                                     │ → Add(1) por goroutine o           │
│                                     │   Add(len) antes del loop          │
│                                     │                                     │
│ Wait en multiples goroutines sin    │ Funciona (es safe) pero puede      │
│ entender la semantica               │ causar confusion                   │
│                                     │ → Documentar el patron             │
└─────────────────────────────────────┴─────────────────────────────────────┘
```

---

## 5. WaitGroup vs Alternativas

### 5.1 WaitGroup vs Channel

```go
// ─── WaitGroup: esperar N goroutines ───
func withWaitGroup(n int) {
    var wg sync.WaitGroup
    for i := 0; i < n; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            work(id)
        }(i)
    }
    wg.Wait()
}

// ─── Channel: esperar N goroutines ───
func withChannel(n int) {
    done := make(chan struct{}, n)
    for i := 0; i < n; i++ {
        go func(id int) {
            work(id)
            done <- struct{}{}
        }(i)
    }
    // Esperar N signals
    for i := 0; i < n; i++ {
        <-done
    }
}

// ─── Channel: esperar N goroutines con resultados ───
func withChannelResults(n int) []Result {
    ch := make(chan Result, n)
    for i := 0; i < n; i++ {
        go func(id int) {
            ch <- work(id)
        }(i)
    }
    results := make([]Result, n)
    for i := 0; i < n; i++ {
        results[i] = <-ch
    }
    return results
}
```

### 5.2 WaitGroup vs errgroup

```go
// ─── WaitGroup: sin error handling ───
func wgNoErrors(urls []string) {
    var wg sync.WaitGroup
    for _, url := range urls {
        url := url
        wg.Add(1)
        go func() {
            defer wg.Done()
            resp, err := http.Get(url)
            if err != nil {
                log.Printf("error: %v", err) // ← logear y continuar (no ideal)
                return
            }
            resp.Body.Close()
        }()
    }
    wg.Wait()
    // ¿Hubo errores? No lo sabemos...
}

// ─── errgroup: con error handling ───
func egWithErrors(ctx context.Context, urls []string) error {
    g, ctx := errgroup.WithContext(ctx)
    for _, url := range urls {
        url := url
        g.Go(func() error {
            req, err := http.NewRequestWithContext(ctx, "GET", url, nil)
            if err != nil {
                return err // ← propaga el error
            }
            resp, err := http.DefaultClient.Do(req)
            if err != nil {
                return err // ← cancela las demas goroutines
            }
            resp.Body.Close()
            return nil
        })
    }
    return g.Wait() // retorna el primer error
}
```

### 5.3 Tabla Comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│              COORDINACION DE GOROUTINES — CUANDO USAR QUE               │
├──────────────────┬──────────┬──────────────┬────────────────────────────┤
│                  │ WaitGroup│ Channel      │ errgroup                   │
├──────────────────┼──────────┼──────────────┼────────────────────────────┤
│ Esperar que      │          │              │                            │
│ terminen         │ ✅        │ ✅            │ ✅                          │
│                  │          │              │                            │
│ Recoger errores  │ ✗        │ Manual       │ ✅ (primer error)           │
│                  │          │              │                            │
│ Recoger          │ Manual   │ ✅            │ Manual                     │
│ resultados       │ (mutex)  │ (via chan)   │ (mutex/slice)              │
│                  │          │              │                            │
│ Cancelar si      │ ✗        │ Manual       │ ✅ (WithContext)            │
│ uno falla        │          │ (ctx+select) │                            │
│                  │          │              │                            │
│ Limitar          │ Manual   │ ✅            │ ✅ (SetLimit)               │
│ concurrencia     │ (sem)    │ (buffer)     │                            │
│                  │          │              │                            │
│ Boilerplate      │ Bajo     │ Medio        │ Bajo                      │
│                  │          │              │                            │
│ Stdlib           │ ✅        │ ✅            │ ✗ (x/sync)                │
│                  │          │              │                            │
│ Complejidad      │ Baja     │ Media        │ Baja                      │
├──────────────────┼──────────┼──────────────┼────────────────────────────┤
│ USAR CUANDO      │ Solo     │ Necesitas    │ Necesitas                  │
│                  │ necesitas│ streaming de │ error handling              │
│                  │ esperar  │ resultados o │ y/o cancelacion            │
│                  │          │ comunicacion │                            │
│                  │          │ entre gorout.│                            │
└──────────────────┴──────────┴──────────────┴────────────────────────────┘
```

---

## 6. WaitGroup en la Stdlib y Ecosistema

### 6.1 net/http — Server Shutdown

```go
// El server HTTP usa WaitGroup internamente para graceful shutdown:
// Cuando llamas srv.Shutdown(ctx), el server:
// 1. Deja de aceptar nuevas conexiones
// 2. Espera (via WaitGroup interno) a que las requests activas terminen
// 3. Retorna cuando todas terminaron o el contexto expiro

srv := &http.Server{Addr: ":8080", Handler: mux}

go func() {
    if err := srv.ListenAndServe(); err != http.ErrServerClosed {
        log.Fatal(err)
    }
}()

// Esperar senal de shutdown
<-ctx.Done()

// Graceful shutdown — internamente usa WaitGroup para esperar requests
shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
defer cancel()
srv.Shutdown(shutdownCtx) // espera a requests en vuelo
```

### 6.2 Patron: Shutdown Coordinator

```go
// Coordinar el shutdown de multiples subsistemas:
type App struct {
    wg      sync.WaitGroup
    cancel  context.CancelFunc
    servers []*http.Server
}

func (app *App) Start(ctx context.Context) {
    ctx, app.cancel = context.WithCancel(ctx)

    // Lanzar cada servidor en su goroutine
    for _, srv := range app.servers {
        srv := srv
        app.wg.Add(1)
        go func() {
            defer app.wg.Done()
            if err := srv.ListenAndServe(); err != http.ErrServerClosed {
                log.Printf("server error: %v", err)
                app.cancel() // cancelar todo si un server falla
            }
        }()
    }

    // Goroutine de shutdown
    app.wg.Add(1)
    go func() {
        defer app.wg.Done()
        <-ctx.Done()
        log.Println("Shutting down...")

        shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
        defer cancel()

        for _, srv := range app.servers {
            srv.Shutdown(shutdownCtx)
        }
    }()
}

func (app *App) Wait() {
    app.wg.Wait() // esperar a que todo se apague limpiamente
}
```

### 6.3 Patron: Worker Lifecycle con WaitGroup

```go
type WorkerPool struct {
    wg      sync.WaitGroup
    jobs    chan Job
    results chan Result
}

func NewWorkerPool(numWorkers int, bufferSize int) *WorkerPool {
    pool := &WorkerPool{
        jobs:    make(chan Job, bufferSize),
        results: make(chan Result, bufferSize),
    }

    // Lanzar workers
    for i := 0; i < numWorkers; i++ {
        pool.wg.Add(1)
        go pool.worker(i)
    }

    // Cerrar results cuando todos los workers terminen
    go func() {
        pool.wg.Wait()
        close(pool.results)
    }()

    return pool
}

func (p *WorkerPool) worker(id int) {
    defer p.wg.Done()
    for job := range p.jobs { // sale cuando jobs se cierra
        result := processJob(id, job)
        p.results <- result
    }
}

func (p *WorkerPool) Submit(job Job) {
    p.jobs <- job
}

func (p *WorkerPool) Close() {
    close(p.jobs) // senalar a los workers que no hay mas trabajo
}

func (p *WorkerPool) Results() <-chan Result {
    return p.results
}

// Uso:
pool := NewWorkerPool(10, 100)

// Enviar trabajo
for _, item := range items {
    pool.Submit(Job{Item: item})
}
pool.Close() // no mas trabajo

// Recoger resultados
for result := range pool.Results() {
    fmt.Println(result)
}
// pool.Results() se cierra cuando todos los workers terminan (via wg.Wait)
```

---

## 7. Patrones Avanzados

### 7.1 WaitGroup con Timeout

WaitGroup no tiene timeout nativo. Combinar con un channel:

```go
// waitWithTimeout espera al WaitGroup con un timeout
func waitWithTimeout(wg *sync.WaitGroup, timeout time.Duration) bool {
    done := make(chan struct{})
    go func() {
        wg.Wait()
        close(done)
    }()

    select {
    case <-done:
        return true // todas las goroutines terminaron
    case <-time.After(timeout):
        return false // timeout — algunas goroutines siguen corriendo
    }
}

// Uso:
var wg sync.WaitGroup
for _, svc := range services {
    svc := svc
    wg.Add(1)
    go func() {
        defer wg.Done()
        checkService(svc)
    }()
}

if !waitWithTimeout(&wg, 30*time.Second) {
    log.Println("WARNING: some health checks timed out")
    // Las goroutines siguen corriendo — no se pueden matar
    // Necesitas context.Context para cancelacion cooperativa
}
```

### 7.2 WaitGroup con Context (Cancelacion + Espera)

```go
func checkAllWithCancel(ctx context.Context, servers []string) error {
    ctx, cancel := context.WithTimeout(ctx, 30*time.Second)
    defer cancel()

    var wg sync.WaitGroup
    var mu sync.Mutex
    var firstErr error

    for _, server := range servers {
        server := server
        wg.Add(1)
        go func() {
            defer wg.Done()

            // Verificar cancelacion antes de empezar
            select {
            case <-ctx.Done():
                return
            default:
            }

            if err := checkServer(ctx, server); err != nil {
                mu.Lock()
                if firstErr == nil {
                    firstErr = err
                    cancel() // cancelar las demas goroutines
                }
                mu.Unlock()
            }
        }()
    }

    wg.Wait()
    return firstErr
}
```

### 7.3 Goroutine Tracker (WaitGroup + Conteo Descriptivo)

```go
// GoroutineTracker extiende WaitGroup con informacion de debugging
type GoroutineTracker struct {
    wg      sync.WaitGroup
    mu      sync.Mutex
    active  map[string]int
}

func NewTracker() *GoroutineTracker {
    return &GoroutineTracker{
        active: make(map[string]int),
    }
}

func (t *GoroutineTracker) Go(name string, f func()) {
    t.wg.Add(1)
    t.mu.Lock()
    t.active[name]++
    t.mu.Unlock()

    go func() {
        defer func() {
            t.mu.Lock()
            t.active[name]--
            if t.active[name] == 0 {
                delete(t.active, name)
            }
            t.mu.Unlock()
            t.wg.Done()
        }()
        f()
    }()
}

func (t *GoroutineTracker) Wait() {
    t.wg.Wait()
}

func (t *GoroutineTracker) Status() map[string]int {
    t.mu.Lock()
    defer t.mu.Unlock()
    result := make(map[string]int, len(t.active))
    for k, v := range t.active {
        result[k] = v
    }
    return result
}

// Uso:
tracker := NewTracker()

for _, svc := range services {
    svc := svc
    tracker.Go("health-check/"+svc.Name, func() {
        checkHealth(svc)
    })
}

// Ver que esta corriendo (para diagnostico):
fmt.Println("Active:", tracker.Status())
// Active: map[health-check/api:1 health-check/db:1 health-check/cache:1]

tracker.Wait()
```

### 7.4 Progressive Results con WaitGroup

```go
// Reportar progreso mientras las goroutines trabajan
func processWithProgress(items []string) {
    var wg sync.WaitGroup
    total := len(items)
    var completed int64

    for _, item := range items {
        item := item
        wg.Add(1)
        go func() {
            defer wg.Done()
            process(item)
            current := atomic.AddInt64(&completed, 1)
            fmt.Printf("\rProgress: %d/%d (%.0f%%)",
                current, total, float64(current)/float64(total)*100)
        }()
    }

    wg.Wait()
    fmt.Printf("\rProgress: %d/%d (100%%) — Done!\n", total, total)
}
```

---

## 8. Comparacion con C y Rust

### 8.1 C — pthread_join y Barriers

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

// ─── Opcion 1: pthread_join (equivalente basico de WaitGroup) ───
void *worker(void *arg) {
    int id = *(int *)arg;
    printf("Worker %d running\n", id);
    free(arg);
    return NULL;
}

void join_all() {
    int n = 5;
    pthread_t threads[5];

    for (int i = 0; i < n; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads[i], NULL, worker, id);
    }

    // Esperar a todos — DEBE ser en orden fijo
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
        // join es 1:1 — un join por thread
        // No puedes "join all" de una vez como wg.Wait()
    }
}

// ─── Opcion 2: pthread_barrier (mas parecido a WaitGroup) ───
pthread_barrier_t barrier;

void *barrier_worker(void *arg) {
    int id = *(int *)arg;
    printf("Worker %d running\n", id);
    free(arg);

    // Todos los threads esperan aqui hasta que N lleguen
    pthread_barrier_wait(&barrier);
    return NULL;
}

void use_barrier() {
    int n = 5;
    pthread_t threads[5];
    pthread_barrier_init(&barrier, NULL, n + 1); // +1 para main

    for (int i = 0; i < n; i++) {
        int *id = malloc(sizeof(int));
        *id = i;
        pthread_create(&threads[i], NULL, barrier_worker, id);
    }

    // Main tambien espera en la barrera
    pthread_barrier_wait(&barrier);
    printf("All done\n");

    pthread_barrier_destroy(&barrier);
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }
}

/*
 * DIFERENCIAS vs Go WaitGroup:
 *
 * pthread_join:
 * - Un join por thread (no puede esperar a "todos" de una vez)
 * - Necesitas guardar el pthread_t de cada thread
 * - Si un thread crashea, join puede bloquear indefinidamente
 * - Retorna el valor de retorno del thread (Go no tiene esto)
 *
 * pthread_barrier:
 * - Mas parecido a WaitGroup — todos esperan hasta que N lleguen
 * - PERO: el numero N es FIJO al crear la barrera
 * - WaitGroup permite Add dinamico (agregar goroutines sobre la marcha)
 * - Barrier debe ser destruido manualmente (pthread_barrier_destroy)
 *
 * Ambos:
 * - Requieren malloc/free para pasar datos a threads
 * - Error handling es via errno (no tipo error)
 * - No tienen reciclaje — cada thread es costoso
 */
```

### 8.2 Rust — JoinHandle, Barrier, y Scoped Threads

```rust
use std::thread;
use std::sync::{Arc, Barrier};

// ─── Opcion 1: JoinHandle (lo mas comun) ───
fn join_handles() {
    let handles: Vec<_> = (0..5)
        .map(|i| {
            thread::spawn(move || {
                println!("Worker {} running", i);
                i * 2 // retornar valor (Go no puede hacer esto)
            })
        })
        .collect();

    // join() retorna Result — captura panics
    for handle in handles {
        match handle.join() {
            Ok(result) => println!("Result: {}", result),
            Err(_) => println!("Thread panicked!"),
        }
    }
}

// ─── Opcion 2: Barrier (como WaitGroup con N fijo) ───
fn use_barrier() {
    let n = 5;
    let barrier = Arc::new(Barrier::new(n + 1)); // +1 para main

    let handles: Vec<_> = (0..n)
        .map(|i| {
            let b = barrier.clone();
            thread::spawn(move || {
                println!("Worker {} running", i);
                b.wait(); // esperar a que todos lleguen
            })
        })
        .collect();

    barrier.wait(); // main espera tambien
    println!("All done");

    for h in handles {
        h.join().unwrap();
    }
}

// ─── Opcion 3: Scoped threads (Go 1.22-like) ───
fn scoped_threads() {
    let mut results = vec![0; 5];

    thread::scope(|s| {
        for (i, result) in results.iter_mut().enumerate() {
            s.spawn(move || {
                *result = i * 2; // escribir directamente — borrow checker lo permite
            });
        }
        // Al salir del scope, TODOS los threads se joinean automaticamente
        // ← equivalente a wg.Wait() pero automatico y safe
    });

    println!("Results: {:?}", results);
}

// ─── tokio equivalente ───
async fn tokio_join() {
    let handles: Vec<_> = (0..5)
        .map(|i| {
            tokio::spawn(async move {
                println!("Worker {} running", i);
                i * 2
            })
        })
        .collect();

    // join_all equivalente a wg.Wait()
    let results = futures::future::join_all(handles).await;
    for result in results {
        println!("Result: {}", result.unwrap());
    }
}

/*
 * COMPARACION:
 *
 * ┌─────────────────────┬──────────────────────┬─────────────────────────┐
 * │                     │ Go WaitGroup         │ Rust                    │
 * ├─────────────────────┼──────────────────────┼─────────────────────────┤
 * │ Esperar todos       │ wg.Wait()            │ join_all / scope        │
 * │ Retorno de valores  │ No (usar chan/mutex)  │ Si (JoinHandle<T>)     │
 * │ Captura panics      │ No (crash)           │ Si (JoinError)         │
 * │ Add dinamico        │ Si (wg.Add)          │ No (Barrier es fijo)   │
 * │ Copy safety         │ Runtime (go vet)     │ Compile (no Clone)     │
 * │ Scoped lifetime     │ No (closure capture) │ Si (thread::scope)     │
 * │ Data race safety    │ Runtime (-race)      │ Compile (Send/Sync)    │
 * │ Overhead            │ Atomics + semaphore  │ Atomics + parking      │
 * └─────────────────────┴──────────────────────┴─────────────────────────┘
 *
 * VENTAJA DE GO: Add dinamico (agregar goroutines durante ejecucion)
 * VENTAJA DE RUST: thread::scope (auto-join, no leak possible)
 *                  JoinHandle retorna valores y captura panics
 */
```

---

## 9. Programa Completo: Infrastructure Deployment Coordinator

Un coordinador de deployments que usa WaitGroup para orquestar fases paralelas con diferentes patrones de sincronizacion:

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "os"
    "os/signal"
    "sync"
    "sync/atomic"
    "syscall"
    "time"
)

// ─────────────────────────────────────────────────────────────────
// Modelos
// ─────────────────────────────────────────────────────────────────

type Service struct {
    Name     string
    Image    string
    Region   string
    Critical bool
}

type PhaseResult struct {
    Service  string
    Phase    string
    Success  bool
    Duration time.Duration
    Message  string
}

func (r PhaseResult) String() string {
    status := "OK  "
    if !r.Success {
        status = "FAIL"
    }
    return fmt.Sprintf("  [%s] %-20s %-12s %8v  %s",
        status, r.Service, r.Phase, r.Duration.Truncate(time.Millisecond), r.Message)
}

// ─────────────────────────────────────────────────────────────────
// Simulador de operaciones
// ─────────────────────────────────────────────────────────────────

func simulateWork(name, phase string, failRate float64) PhaseResult {
    start := time.Now()
    duration := time.Duration(50+rand.Intn(150)) * time.Millisecond
    time.Sleep(duration)

    success := rand.Float64() > failRate
    msg := "completed successfully"
    if !success {
        msg = "operation failed"
    }

    return PhaseResult{
        Service:  name,
        Phase:    phase,
        Success:  success,
        Duration: time.Since(start),
        Message:  msg,
    }
}

// ─────────────────────────────────────────────────────────────────
// Fase 1: Pre-check (todos en paralelo, esperar a todos)
// ─────────────────────────────────────────────────────────────────

func preCheck(services []Service) ([]PhaseResult, bool) {
    fmt.Println("\n--- Phase 1: Pre-Check (parallel, wait all) ---")

    var wg sync.WaitGroup
    results := make([]PhaseResult, len(services))

    for i, svc := range services {
        i, svc := i, svc
        wg.Add(1)
        go func() {
            defer wg.Done()
            results[i] = simulateWork(svc.Name, "pre-check", 0.1)
        }()
    }

    wg.Wait()

    allPassed := true
    for _, r := range results {
        fmt.Println(r)
        if !r.Success {
            allPassed = false
        }
    }

    return results, allPassed
}

// ─────────────────────────────────────────────────────────────────
// Fase 2: Pull images (paralelo con limite de concurrencia)
// ─────────────────────────────────────────────────────────────────

func pullImages(services []Service, maxConcurrent int) ([]PhaseResult, bool) {
    fmt.Printf("\n--- Phase 2: Pull Images (parallel, max %d concurrent) ---\n", maxConcurrent)

    var wg sync.WaitGroup
    sem := make(chan struct{}, maxConcurrent)
    results := make([]PhaseResult, len(services))

    for i, svc := range services {
        i, svc := i, svc
        wg.Add(1)
        sem <- struct{}{} // esperar slot

        go func() {
            defer wg.Done()
            defer func() { <-sem }() // liberar slot
            results[i] = simulateWork(svc.Name, "pull-image", 0.05)
        }()
    }

    wg.Wait()

    allPassed := true
    for _, r := range results {
        fmt.Println(r)
        if !r.Success {
            allPassed = false
        }
    }

    return results, allPassed
}

// ─────────────────────────────────────────────────────────────────
// Fase 3: Deploy (paralelo con progress tracking)
// ─────────────────────────────────────────────────────────────────

func deployAll(services []Service) ([]PhaseResult, bool) {
    fmt.Println("\n--- Phase 3: Deploy (parallel with progress) ---")

    var wg sync.WaitGroup
    var completed int64
    total := int64(len(services))

    var mu sync.Mutex
    var results []PhaseResult

    for _, svc := range services {
        svc := svc
        wg.Add(1)
        go func() {
            defer wg.Done()
            result := simulateWork(svc.Name, "deploy", 0.15)

            mu.Lock()
            results = append(results, result)
            mu.Unlock()

            current := atomic.AddInt64(&completed, 1)
            fmt.Printf("  Progress: %d/%d deployed\n", current, total)
        }()
    }

    wg.Wait()

    allPassed := true
    fmt.Println("  --- Deploy Results ---")
    for _, r := range results {
        fmt.Println(r)
        if !r.Success {
            allPassed = false
        }
    }

    return results, allPassed
}

// ─────────────────────────────────────────────────────────────────
// Fase 4: Health check (con timeout usando WaitGroup + channel)
// ─────────────────────────────────────────────────────────────────

func healthCheck(services []Service, timeout time.Duration) ([]PhaseResult, bool) {
    fmt.Printf("\n--- Phase 4: Health Check (timeout %v) ---\n", timeout)

    var wg sync.WaitGroup
    results := make([]PhaseResult, len(services))

    ctx, cancel := context.WithTimeout(context.Background(), timeout)
    defer cancel()

    for i, svc := range services {
        i, svc := i, svc
        wg.Add(1)
        go func() {
            defer wg.Done()
            // Verificar ctx antes de empezar
            select {
            case <-ctx.Done():
                results[i] = PhaseResult{
                    Service: svc.Name,
                    Phase:   "health-check",
                    Success: false,
                    Message: "cancelled: " + ctx.Err().Error(),
                }
                return
            default:
            }
            results[i] = simulateWork(svc.Name, "health-check", 0.1)
        }()
    }

    // Wait con timeout
    done := make(chan struct{})
    go func() {
        wg.Wait()
        close(done)
    }()

    select {
    case <-done:
        // Todas completaron
    case <-ctx.Done():
        fmt.Println("  WARNING: health check timed out, some results may be incomplete")
    }

    allPassed := true
    for _, r := range results {
        if r.Service != "" { // skip vacios (cancelados que no empezaron)
            fmt.Println(r)
            if !r.Success {
                allPassed = false
            }
        }
    }

    return results, allPassed
}

// ─────────────────────────────────────────────────────────────────
// Fase 5: Rollback (si algo fallo, en paralelo)
// ─────────────────────────────────────────────────────────────────

func rollback(failedServices []string) {
    if len(failedServices) == 0 {
        return
    }

    fmt.Printf("\n--- Rollback (%d services) ---\n", len(failedServices))

    var wg sync.WaitGroup
    for _, name := range failedServices {
        name := name
        wg.Add(1)
        go func() {
            defer wg.Done()
            result := simulateWork(name, "rollback", 0.0)
            fmt.Println(result)
        }()
    }

    wg.Wait()
    fmt.Println("  Rollback complete")
}

// ─────────────────────────────────────────────────────────────────
// Orquestador principal
// ─────────────────────────────────────────────────────────────────

func orchestrate(ctx context.Context, services []Service) {
    start := time.Now()

    fmt.Println("========================================")
    fmt.Printf("  Deployment: %d services\n", len(services))
    fmt.Println("========================================")

    // Fase 1: Pre-check
    _, preCheckOK := preCheck(services)
    if !preCheckOK {
        fmt.Println("\n*** ABORT: Pre-check failed — not deploying ***")
        return
    }

    // Verificar cancelacion entre fases
    select {
    case <-ctx.Done():
        fmt.Println("\n*** ABORT: Deployment cancelled ***")
        return
    default:
    }

    // Fase 2: Pull images (max 3 concurrent)
    _, pullOK := pullImages(services, 3)
    if !pullOK {
        fmt.Println("\n*** ABORT: Image pull failed ***")
        return
    }

    select {
    case <-ctx.Done():
        fmt.Println("\n*** ABORT: Deployment cancelled ***")
        return
    default:
    }

    // Fase 3: Deploy
    deployResults, _ := deployAll(services)

    // Fase 4: Health check (5 second timeout)
    healthResults, healthOK := healthCheck(services, 5*time.Second)

    // Evaluar resultado final
    var failedServices []string
    for i, svc := range services {
        deployOK := i < len(deployResults) && deployResults[i].Success
        healthOK := i < len(healthResults) && healthResults[i].Success
        if !deployOK || !healthOK {
            failedServices = append(failedServices, svc.Name)
        }
    }

    if len(failedServices) > 0 {
        rollback(failedServices)
    }

    // Resumen
    elapsed := time.Since(start)
    successful := len(services) - len(failedServices)

    fmt.Println("\n========================================")
    fmt.Printf("  Deployment Summary\n")
    fmt.Printf("  Total:    %d services\n", len(services))
    fmt.Printf("  Success:  %d\n", successful)
    fmt.Printf("  Failed:   %d\n", len(failedServices))
    fmt.Printf("  Duration: %v\n", elapsed.Truncate(time.Millisecond))
    fmt.Printf("  Health:   %v\n", healthOK)
    fmt.Println("========================================")
}

// ─────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────

func main() {
    services := []Service{
        {Name: "api-gateway", Image: "api:v2.1.0", Region: "us-east-1", Critical: true},
        {Name: "user-service", Image: "users:v1.5.0", Region: "us-east-1", Critical: true},
        {Name: "auth-service", Image: "auth:v3.0.0", Region: "us-east-1", Critical: true},
        {Name: "notification", Image: "notify:v1.2.0", Region: "eu-west-1", Critical: false},
        {Name: "analytics", Image: "analytics:v2.0.0", Region: "eu-west-1", Critical: false},
        {Name: "cdn-worker", Image: "cdn:v1.0.0", Region: "ap-south-1", Critical: false},
        {Name: "search-index", Image: "search:v4.0.0", Region: "us-east-1", Critical: false},
        {Name: "cache-warmer", Image: "cache:v1.1.0", Region: "us-east-1", Critical: false},
    }

    ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
    defer stop()

    orchestrate(ctx, services)
}
```

### Ejecutar:

```bash
go mod init deploy-coordinator
go run main.go

# Con race detector:
go run -race main.go

# Output esperado (ejemplo):
# ========================================
#   Deployment: 8 services
# ========================================
#
# --- Phase 1: Pre-Check (parallel, wait all) ---
#   [OK  ] api-gateway          pre-check      120ms  completed successfully
#   [OK  ] user-service         pre-check       85ms  completed successfully
#   [OK  ] auth-service         pre-check      145ms  completed successfully
#   ...
#
# --- Phase 2: Pull Images (parallel, max 3 concurrent) ---
#   [OK  ] api-gateway          pull-image     102ms  completed successfully
#   ...
#
# --- Phase 3: Deploy (parallel with progress) ---
#   Progress: 1/8 deployed
#   Progress: 2/8 deployed
#   ...
#   Progress: 8/8 deployed
#   --- Deploy Results ---
#   [OK  ] notification         deploy          98ms  completed successfully
#   [FAIL] analytics            deploy         134ms  operation failed
#   ...
#
# --- Phase 4: Health Check (timeout 5s) ---
#   [OK  ] api-gateway          health-check    87ms  completed successfully
#   ...
#
# --- Rollback (1 services) ---
#   [OK  ] analytics            rollback       112ms  completed successfully
#   Rollback complete
#
# ========================================
#   Deployment Summary
#   Total:    8 services
#   Success:  7
#   Failed:   1
#   Duration: 892ms
# ========================================
```

### Patrones de WaitGroup usados:

```
┌──────────────────────────────────────────────────────────────────────────┐
│            PATRONES DE WAITGROUP EN EL PROGRAMA                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. Basico (pre-check): Add + go + defer Done + Wait                   │
│     → Esperar a que todos los checks paralelos terminen                │
│                                                                          │
│  2. Semaforo (pull images): WaitGroup + channel buffer                 │
│     → Limitar concurrencia a N goroutines simultaneas                  │
│                                                                          │
│  3. Progress tracking (deploy): WaitGroup + atomic counter             │
│     → Reportar progreso mientras las goroutines trabajan               │
│                                                                          │
│  4. Mutex para resultados (deploy): WaitGroup + sync.Mutex             │
│     → Recolectar resultados en un slice compartido                     │
│                                                                          │
│  5. Timeout (health check): WaitGroup + channel + select               │
│     → Esperar con deadline, abortar si tarda demasiado                 │
│                                                                          │
│  6. Fases secuenciales: un WaitGroup por fase                          │
│     → Fase 1 completa → Fase 2 empieza → ... → Rollback si fallo      │
│                                                                          │
│  7. Cancelacion entre fases: context.Context + select                  │
│     → Abortar el pipeline completo si llega SIGINT                     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 10. Ejercicios

### Ejercicio 1: Parallel File Hasher

```
Implementa un programa que calcula hashes SHA-256 de multiples archivos en paralelo:

Requisitos:
- Recibir una lista de paths como argumentos de linea de comando
- Calcular SHA-256 de cada archivo en una goroutine separada
- Usar WaitGroup para esperar a todos
- Recolectar resultados por indice (sin mutex)
- Si un archivo no existe, el resultado debe indicar el error (no crash)
- Imprimir resultados en formato: hash  filename
- Medir tiempo total vs estimacion secuencial
- Bonus: agregar semaforo para limitar a max 10 archivos abiertos
  simultaneamente (por limites de file descriptors)
```

### Ejercicio 2: Multi-Source Log Merger

```
Implementa un merger de logs de multiples fuentes:

Requisitos:
- N goroutines leen de N "fuentes" simuladas (cada una genera
  lineas con timestamp y contenido aleatorio)
- Un WaitGroup coordina cuando todas las fuentes terminan
- Las lineas se envian a un channel compartido
- Una goroutine "merger" lee del channel e imprime lineas
  en orden de llegada (no necesariamente cronologico)
- Al final, imprimir estadisticas:
  - Lineas por fuente
  - Total lineas procesadas
  - Duracion total
- Implementar graceful shutdown con context.Context:
  Si llega SIGINT, todas las fuentes deben parar y el merger
  debe procesar las lineas restantes en el channel
- Usar defer wg.Done() en TODAS las goroutines
- Verificar con -race que no hay data races
```

### Ejercicio 3: Dependency Graph Executor

```
Implementa un ejecutor de grafos de dependencias:

Requisitos:
- Definir un grafo de tareas con dependencias:
  A → (B, C)  (B y C dependen de A)
  B → D       (D depende de B)
  C → D       (D depende de C, tambien)
  D → E       (E depende de D)
- Cada tarea se ejecuta en su propia goroutine
- Una tarea NO empieza hasta que TODAS sus dependencias terminaron
- Usar WaitGroup para coordinar: cada tarea tiene su propio WaitGroup
  que sus dependientes esperan
- Detectar deadlocks (dependencias circulares) antes de ejecutar
- Imprimir el orden de ejecucion (demostrar que A→{B,C} son paralelas)
- Medir speedup vs ejecucion secuencial
```

### Ejercicio 4: Rate-Limited Batch Processor

```
Implementa un procesador de batches con rate limiting:

Requisitos:
- Procesar 10,000 items en batches de 100
- Cada batch se procesa en paralelo (WaitGroup)
- Entre batches, esperar 1 segundo (rate limiting)
- Dentro de cada batch, limitar a 20 goroutines concurrentes (semaforo)
- Recolectar errores de cada batch (mutex + slice)
- Si un batch tiene >50% de fallos, abortar los batches restantes
- Reportar progreso: batch N/M, items procesados, errores acumulados
- Implementar graceful shutdown: si llega SIGINT, completar el batch
  actual pero no empezar el siguiente
```

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    WAITGROUP — RESUMEN COMPLETO                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  API:                                                                    │
│  ├─ wg.Add(n)  → incrementar contador ("n goroutines mas")             │
│  ├─ wg.Done()  → decrementar en 1 (= Add(-1))                         │
│  └─ wg.Wait()  → bloquear hasta que contador sea 0                    │
│                                                                          │
│  CONTRATO:                                                               │
│  ├─ Add ANTES de go (no dentro de la goroutine lanzada)                │
│  ├─ SIEMPRE defer wg.Done() como primera linea                         │
│  ├─ No copiar WaitGroup (pasar por puntero o closure)                  │
│  ├─ No reusar durante Wait activo                                      │
│  └─ Done de mas = panic ("negative counter")                           │
│                                                                          │
│  PATRONES:                                                               │
│  ├─ Basico: Add + go + defer Done + Wait                               │
│  ├─ Resultados por indice: results[i] = ... (sin mutex)               │
│  ├─ Resultados por mutex: mu.Lock + append + mu.Unlock                 │
│  ├─ Resultados por channel: ch <- result + go { wg.Wait; close(ch) }  │
│  ├─ Semaforo: WaitGroup + chan struct{}{} para limitar concurrencia    │
│  ├─ Timeout: go { wg.Wait; close(done) } + select con time.After      │
│  ├─ Fases: un WaitGroup por fase secuencial                           │
│  └─ Progress: WaitGroup + atomic.AddInt64                              │
│                                                                          │
│  CUANDO USAR:                                                           │
│  ├─ Solo necesitas esperar → WaitGroup                                 │
│  ├─ Necesitas errores → errgroup                                       │
│  ├─ Necesitas resultados streaming → channel                           │
│  └─ Necesitas comunicacion bidireccional → channel                     │
│                                                                          │
│  INTERNOS:                                                               │
│  ├─ Counter atomico (uint64) + semaforo del runtime                    │
│  ├─ No usa mutex (operaciones atomicas puras)                          │
│  └─ Wait multiple: varias goroutines pueden Wait simultaneamente       │
│                                                                          │
│  vs C: pthread_join (1:1, sin conteo) / pthread_barrier (N fijo)       │
│  vs Rust: JoinHandle (retorna valor) / Barrier / thread::scope         │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S01 - Goroutines, T04 - Data races — race detector (-race flag), acceso concurrente a memoria compartida
