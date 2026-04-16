# Mutex — sync.Mutex, sync.RWMutex, Lock/Unlock, defer unlock, cuando preferir sobre channels

## 1. Introduccion

Un **mutex** (mutual exclusion) es un mecanismo de sincronizacion que garantiza que **solo una goroutine a la vez** puede acceder a una seccion critica de codigo. Mientras que channels implementan el modelo CSP ("share memory by communicating"), los mutex implementan el modelo tradicional de **shared memory** — y Go los ofrece como ciudadanos de primera clase en el paquete `sync` porque hay casos donde son la herramienta correcta.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    MUTEX EN GO                                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  "Share memory by communicating" ← channels (CSP)                      │
│  "Communicate by sharing memory" ← mutex (traditional)                 │
│                                                                          │
│  Go provee AMBOS — no son opuestos, son complementarios.               │
│                                                                          │
│  CHANNEL es mejor para:                                                 │
│  ├─ Transferir ownership de datos entre goroutines                     │
│  ├─ Distribuir trabajo (fan-out/fan-in)                                │
│  ├─ Senalizacion (done, cancelacion)                                   │
│  └─ Pipeline de procesamiento                                          │
│                                                                          │
│  MUTEX es mejor para:                                                   │
│  ├─ Proteger estado interno de una estructura                          │
│  ├─ Cache (map compartido, read-heavy)                                 │
│  ├─ Contadores, metricas, estadisticas                                 │
│  ├─ Acceso a recursos que no se "transfieren" (config, registry)      │
│  └─ Cuando el patron es "leer/modificar/escribir" en-place            │
│                                                                          │
│  REGLA PRACTICA:                                                         │
│  ├─ ¿Los datos se MUEVEN entre goroutines? → Channel                  │
│  ├─ ¿Los datos se COMPARTEN en-place? → Mutex                         │
│  └─ ¿No estas seguro? → Empieza con channels, refactoriza si es mas  │
│     natural con mutex                                                   │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

Para SysAdmin/DevOps:
- Todo in-memory cache en Go (config cargada de archivo, DNS cache, connection pool metadata) usa mutex
- Las metricas de Prometheus (`Counter`, `Gauge`, `Histogram`) usan atomics y mutex internamente
- `sync.RWMutex` es critico para read-heavy workloads (99% reads, 1% writes — como configuracion que rara vez cambia)
- Los deadlocks por mutex mal usado son una de las causas mas comunes de "el servicio se congelo" en produccion

---

## 2. sync.Mutex — Exclusion Mutua Basica

### 2.1 API

```go
type Mutex struct {
    // campos no exportados
}

func (m *Mutex) Lock()      // adquirir — bloquea si ya esta lockeado
func (m *Mutex) Unlock()    // liberar — panic si no esta lockeado
func (m *Mutex) TryLock() bool  // Go 1.18+ — intentar sin bloquear
```

### 2.2 Uso basico

```go
package main

import (
    "fmt"
    "sync"
)

func main() {
    var mu sync.Mutex
    counter := 0
    
    var wg sync.WaitGroup
    
    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            
            mu.Lock()         // ENTRAR a la seccion critica
            counter++          // solo una goroutine a la vez ejecuta esto
            mu.Unlock()        // SALIR de la seccion critica
        }()
    }
    
    wg.Wait()
    fmt.Println("Counter:", counter) // siempre 1000
}
```

### 2.3 defer Unlock — patron idiomatico

```go
package main

import (
    "fmt"
    "sync"
)

type SafeCounter struct {
    mu sync.Mutex
    v  map[string]int
}

func (c *SafeCounter) Inc(key string) {
    c.mu.Lock()
    defer c.mu.Unlock() // SIEMPRE usar defer para garantizar unlock
    
    c.v[key]++
    // Si hay un panic aqui, defer ejecuta Unlock de todas formas
    // Sin defer, un panic dejaria el mutex lockeado → deadlock
}

func (c *SafeCounter) Value(key string) int {
    c.mu.Lock()
    defer c.mu.Unlock()
    return c.v[key]
}

func main() {
    c := SafeCounter{v: make(map[string]int)}
    
    var wg sync.WaitGroup
    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            c.Inc("somekey")
        }()
    }
    
    wg.Wait()
    fmt.Println("Value:", c.Value("somekey")) // 1000
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    POR QUE defer Unlock()                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SIN defer:                                                              │
│                                                                          │
│  func (c *Cache) Get(key string) (string, error) {                     │
│      c.mu.Lock()                                                        │
│      val, ok := c.data[key]                                            │
│      if !ok {                                                            │
│          c.mu.Unlock()  // ← debes recordar unlock en CADA return      │
│          return "", ErrNotFound                                         │
│      }                                                                   │
│      c.mu.Unlock()      // ← y aqui tambien                            │
│      return val, nil                                                    │
│  }                                                                       │
│  // Problema: olvidar un Unlock = deadlock                              │
│  // Problema: panic antes del Unlock = deadlock                         │
│                                                                          │
│  CON defer:                                                              │
│                                                                          │
│  func (c *Cache) Get(key string) (string, error) {                     │
│      c.mu.Lock()                                                        │
│      defer c.mu.Unlock()  // GARANTIZADO al salir de la funcion        │
│      val, ok := c.data[key]                                            │
│      if !ok {                                                            │
│          return "", ErrNotFound  // Unlock automatico                   │
│      }                                                                   │
│      return val, nil             // Unlock automatico                   │
│  }                                                                       │
│                                                                          │
│  REGLA: Lock() y defer Unlock() siempre en lineas consecutivas.       │
│  Si ves Lock() sin defer Unlock() inmediatamente debajo, es un        │
│  code smell (excepto en optimizaciones muy especificas).               │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.4 Mutex en struct — composicion idiomatica

```go
// PATRON: mutex justo arriba de los campos que protege
type Config struct {
    mu       sync.Mutex // protege los campos debajo
    host     string
    port     int
    features map[string]bool
}

// PATRON: metodos que encapsulan Lock/Unlock
func (c *Config) SetHost(h string) {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.host = h
}

func (c *Config) Host() string {
    c.mu.Lock()
    defer c.mu.Unlock()
    return c.host
}

// PATRON: mutex no exportado = API thread-safe por diseno
// El usuario de Config no sabe que hay un mutex — solo llama metodos
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    POSICION DEL MUTEX EN STRUCT                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  CONVENCION: el mutex se coloca justo ARRIBA de los campos que protege │
│  (con un comentario si no es obvio)                                     │
│                                                                          │
│  type Server struct {                                                    │
│      addr string  // inmutable, no necesita mutex                      │
│      port int     // inmutable                                          │
│                                                                          │
│      mu       sync.Mutex    // protege connections y stats             │
│      connections map[string]*Conn                                      │
│      stats      ServerStats                                             │
│                                                                          │
│      configMu sync.RWMutex  // protege config (read-heavy)             │
│      config   *Config                                                   │
│  }                                                                       │
│                                                                          │
│  Si un struct tiene multiples grupos de campos que cambian              │
│  independientemente, usa multiples mutex. Esto reduce la               │
│  contention (cada lock protege menos datos).                           │
│                                                                          │
│  REGLA: nunca exportes un mutex. Los metodos del struct deben          │
│  encapsular toda la logica de locking.                                  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.5 Internals — como funciona sync.Mutex

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    INTERNALS DE sync.Mutex                               │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  type Mutex struct {                                                    │
│      state int32   // flags: locked | woken | starving | waiterCount   │
│      sema  uint32  // semaforo del runtime para park/unpark goroutines │
│  }                                                                       │
│                                                                          │
│  MODOS DE OPERACION (Go 1.9+):                                         │
│                                                                          │
│  MODO NORMAL:                                                            │
│  ├─ Lock() intenta atomic CAS (Compare-And-Swap) en state             │
│  ├─ Si tiene exito: mutex adquirido (fast path, ~20ns)                │
│  ├─ Si falla: goroutine hace spin (unos ciclos de CPU)                │
│  ├─ Si spin no funciona: goroutine se "parkea" (suspende)             │
│  └─ Las goroutines "parkeadas" se despiertan en orden FIFO            │
│     PERO: goroutines nuevas (spinning) compiten con las despertadas   │
│     Esto favorece a las nuevas (ya estan ejecutando → cache hot)      │
│                                                                          │
│  MODO STARVATION (Go 1.9+):                                            │
│  ├─ Se activa si una goroutine espera > 1ms                            │
│  ├─ En este modo: el mutex se entrega DIRECTAMENTE a la goroutine     │
│  │  mas antigua en la cola FIFO (sin competencia)                      │
│  ├─ Goroutines nuevas NO intentan spin — van directamente a la cola   │
│  └─ Garantiza que ninguna goroutine espera indefinidamente             │
│     (previene starvation)                                               │
│                                                                          │
│  RENDIMIENTO:                                                            │
│  ├─ Uncontended Lock/Unlock: ~20-25ns (atomic CAS)                    │
│  ├─ Contended (pocas goroutines): ~100-500ns (spin + CAS)             │
│  ├─ Contended (muchas goroutines): ~1-10us (park/unpark)              │
│  └─ Starvation mode: mayor latencia individual pero fairness          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 3. sync.RWMutex — Lectores Multiples, Escritor Unico

`sync.RWMutex` permite **multiples lectores simultaneos** o **un unico escritor**. Es ideal para datos que se leen mucho y se escriben poco (read-heavy workloads).

### 3.1 API

```go
type RWMutex struct {
    // campos no exportados
}

// Exclusivo (escritura) — como Mutex.Lock/Unlock
func (rw *RWMutex) Lock()       // adquirir write lock (espera a que todos los readers terminen)
func (rw *RWMutex) Unlock()     // liberar write lock
func (rw *RWMutex) TryLock() bool  // Go 1.18+

// Compartido (lectura) — multiples goroutines pueden tenerlo simultaneamente
func (rw *RWMutex) RLock()      // adquirir read lock
func (rw *RWMutex) RUnlock()    // liberar read lock
func (rw *RWMutex) TryRLock() bool // Go 1.18+
```

### 3.2 Semantica

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    sync.RWMutex — SEMANTICA                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Estado actual      │ RLock() (leer) │ Lock() (escribir)               │
│  ──────────────────┼────────────────┼─────────────────                  │
│  Sin lock           │ OK (adquiere)  │ OK (adquiere)                   │
│  Read-locked (1+)   │ OK (comparte)  │ BLOQUEA (espera readers)        │
│  Write-locked       │ BLOQUEA        │ BLOQUEA                          │
│                                                                          │
│  MULTIPLES READERS simultanamente: SI                                   │
│  READER + WRITER simultaneamente: NO                                    │
│  MULTIPLES WRITERS simultaneamente: NO                                  │
│                                                                          │
│  ┌─────────────────────────────────────────────────────────┐           │
│  │ Readers:  R1 ████████     R3 ████████                   │           │
│  │           R2 ████████████████████████                    │           │
│  │                                                         │           │
│  │ Writer:            W1 [bloqueado...] ████                │           │
│  │                                                         │           │
│  │ Timeline:  ──────────────────────────────────────►      │           │
│  │                                                         │           │
│  │ R1 y R2 leen simultaneamente.                           │           │
│  │ W1 espera hasta que R1 y R2 terminan.                   │           │
│  │ R3 espera hasta que W1 termina.                         │           │
│  └─────────────────────────────────────────────────────────┘           │
│                                                                          │
│  WRITER PRIORITY:                                                       │
│  Cuando un writer esta esperando, nuevos readers tambien               │
│  se bloquean. Esto previene write starvation — sin esto,              │
│  un flujo constante de readers podria bloquear al writer              │
│  indefinidamente.                                                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.3 Ejemplo: cache thread-safe

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

type Cache struct {
    mu   sync.RWMutex
    data map[string]string
}

func NewCache() *Cache {
    return &Cache{data: make(map[string]string)}
}

// Get usa RLock — multiples goroutines pueden leer simultaneamente
func (c *Cache) Get(key string) (string, bool) {
    c.mu.RLock()
    defer c.mu.RUnlock()
    val, ok := c.data[key]
    return val, ok
}

// Set usa Lock — exclusion total mientras escribe
func (c *Cache) Set(key, value string) {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.data[key] = value
}

// Delete usa Lock
func (c *Cache) Delete(key string) {
    c.mu.Lock()
    defer c.mu.Unlock()
    delete(c.data, key)
}

// Keys usa RLock — lectura
func (c *Cache) Keys() []string {
    c.mu.RLock()
    defer c.mu.RUnlock()
    keys := make([]string, 0, len(c.data))
    for k := range c.data {
        keys = append(keys, k)
    }
    return keys
}

func main() {
    cache := NewCache()
    
    var wg sync.WaitGroup
    
    // 10 writers
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            key := fmt.Sprintf("key-%d", id)
            cache.Set(key, fmt.Sprintf("value-%d", id))
        }(i)
    }
    
    // 100 readers (10x mas que writers — tipico en produccion)
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            key := fmt.Sprintf("key-%d", id%10)
            if val, ok := cache.Get(key); ok {
                _ = val // usar el valor
            }
        }(i)
    }
    
    wg.Wait()
    fmt.Printf("Cache tiene %d keys\n", len(cache.Keys()))
    
    _ = time.Now() // silenciar import
}
```

### 3.4 Benchmark: Mutex vs RWMutex

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

func benchmarkMutex(readers, writers int, iterations int) time.Duration {
    var mu sync.Mutex
    data := make(map[string]int)
    data["key"] = 0
    
    var wg sync.WaitGroup
    start := time.Now()
    
    for i := 0; i < writers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for j := 0; j < iterations; j++ {
                mu.Lock()
                data["key"]++
                mu.Unlock()
            }
        }()
    }
    
    for i := 0; i < readers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for j := 0; j < iterations; j++ {
                mu.Lock()
                _ = data["key"]
                mu.Unlock()
            }
        }()
    }
    
    wg.Wait()
    return time.Since(start)
}

func benchmarkRWMutex(readers, writers int, iterations int) time.Duration {
    var mu sync.RWMutex
    data := make(map[string]int)
    data["key"] = 0
    
    var wg sync.WaitGroup
    start := time.Now()
    
    for i := 0; i < writers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for j := 0; j < iterations; j++ {
                mu.Lock()
                data["key"]++
                mu.Unlock()
            }
        }()
    }
    
    for i := 0; i < readers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for j := 0; j < iterations; j++ {
                mu.RLock()
                _ = data["key"]
                mu.RUnlock()
            }
        }()
    }
    
    wg.Wait()
    return time.Since(start)
}

func main() {
    iterations := 10000
    
    fmt.Println("Benchmark: Mutex vs RWMutex")
    fmt.Println("===========================")
    
    scenarios := []struct {
        readers int
        writers int
    }{
        {1, 1},     // balanced
        {10, 1},    // read-heavy
        {100, 1},   // very read-heavy
        {1, 10},    // write-heavy
        {10, 10},   // balanced high concurrency
    }
    
    for _, s := range scenarios {
        muDuration := benchmarkMutex(s.readers, s.writers, iterations)
        rwDuration := benchmarkRWMutex(s.readers, s.writers, iterations)
        
        speedup := float64(muDuration) / float64(rwDuration)
        better := "RWMutex"
        if speedup < 1 {
            better = "Mutex"
            speedup = 1 / speedup
        }
        
        fmt.Printf("  R=%3d W=%3d | Mutex: %8v | RWMutex: %8v | %s %.1fx faster\n",
            s.readers, s.writers,
            muDuration.Round(time.Millisecond),
            rwDuration.Round(time.Millisecond),
            better, speedup)
    }
    
    // Resultados tipicos:
    // R=  1 W=  1 | Mutex similar a RWMutex (overhead de RW insignificante)
    // R= 10 W=  1 | RWMutex 2-5x faster (readers no se bloquean entre si)
    // R=100 W=  1 | RWMutex 5-10x faster (gran beneficio para read-heavy)
    // R=  1 W= 10 | Mutex slightly faster (RWMutex tiene overhead extra)
    // R= 10 W= 10 | Similar o Mutex slightly faster
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CUANDO USAR Mutex vs RWMutex                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Usa sync.Mutex cuando:                                                │
│  ├─ Las operaciones son mayoritariamente escrituras                    │
│  ├─ Lectura y escritura tienen duracion similar                        │
│  ├─ Simplicidad es mas importante que performance                      │
│  └─ La seccion critica es muy corta (el overhead de RWMutex no vale)  │
│                                                                          │
│  Usa sync.RWMutex cuando:                                              │
│  ├─ Las lecturas superan a las escrituras (>= 10:1 ratio)             │
│  ├─ La seccion critica de lectura es significativa (no trivial)       │
│  ├─ Hay muchos readers concurrentes (>= 10)                           │
│  └─ Puedes tolerar ligeramente mas complejidad en el codigo           │
│                                                                          │
│  BENCHMARKS TIPICOS:                                                     │
│  ├─ Uncontended: Mutex ~20ns, RWMutex RLock ~25ns                    │
│  ├─ 10 readers / 1 writer: RWMutex 3-5x mejor                        │
│  ├─ 100 readers / 1 writer: RWMutex 5-10x mejor                      │
│  ├─ 1 reader / 10 writers: Mutex 1.1-1.3x mejor                     │
│  └─ High contention: ambos degradan — considera redisenar             │
│                                                                          │
│  EN LA PRACTICA:                                                         │
│  ├─ Config/settings → RWMutex (se leen constantemente, se escriben    │
│  │  solo cuando cambian)                                                │
│  ├─ Cache → RWMutex (muchas lecturas, writes en miss/eviction)        │
│  ├─ Counter → Mutex (o mejor: sync/atomic)                            │
│  ├─ Connection pool → Mutex (get/put son ambas writes)                │
│  └─ Rate limiter state → Mutex (check-and-update es write)            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 4. TryLock (Go 1.18+)

`TryLock` intenta adquirir el mutex sin bloquear. Retorna `true` si lo logro, `false` si ya esta lockeado:

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

type ResourcePool struct {
    mu       sync.Mutex
    resource string
}

func (p *ResourcePool) TryUse(id int) {
    if p.mu.TryLock() {
        defer p.mu.Unlock()
        fmt.Printf("  Worker %d: usando recurso (%s)\n", id, p.resource)
        time.Sleep(100 * time.Millisecond)
        fmt.Printf("  Worker %d: recurso liberado\n", id)
    } else {
        fmt.Printf("  Worker %d: recurso ocupado, haciendo otra cosa\n", id)
        // Hacer trabajo alternativo en lugar de esperar
    }
}

func main() {
    pool := &ResourcePool{resource: "database-connection"}
    
    var wg sync.WaitGroup
    for i := 1; i <= 5; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            pool.TryUse(id)
        }(i)
    }
    
    wg.Wait()
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    TryLock — CUIDADO                                     │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  TryLock es RARA VEZ la herramienta correcta. La documentacion oficial │
│  dice: "Note that while correct uses of TryLock do exist, they are     │
│  rare, and use of TryLock is often a sign of a deeper problem in a     │
│  particular use of mutexes."                                            │
│                                                                          │
│  USOS LEGITIMOS:                                                        │
│  ├─ Lock ordering: evitar deadlock al adquirir multiples locks         │
│  ├─ Optimistic locking: intentar sin bloquear, fallar rapido           │
│  ├─ Backoff: si no puedo adquirir, intento otro recurso               │
│  └─ Diagnostico: "¿esta lockeado?" para logging/metrics               │
│                                                                          │
│  NO USAR PARA:                                                          │
│  ├─ Polling loop: for !mu.TryLock() {} ← busy-wait, PEOR que Lock()  │
│  ├─ Evitar deadlocks: arregla el deadlock, no lo escondas             │
│  └─ Performance: TryLock no es mas rapido que Lock (mismo CAS)        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Errores y Antipatrones

### 5.1 Catalogo de errores

```go
// ============================================================
// ERROR 1: Copiar un Mutex
// ============================================================
type Bad1 struct {
    mu sync.Mutex
    v  int
}

func (b Bad1) Inc() { // VALUE RECEIVER — copia el struct incluyendo el mutex
    b.mu.Lock()        // lockea una COPIA del mutex — no protege nada
    defer b.mu.Unlock()
    b.v++              // modifica la copia, no el original
}

// FIX: SIEMPRE usar pointer receiver con mutex
func (b *Bad1) IncFixed() {
    b.mu.Lock()
    defer b.mu.Unlock()
    b.v++
}

// go vet detecta esto: "Inc passes lock by value: Bad1 contains sync.Mutex"

// ============================================================
// ERROR 2: Lock sin Unlock (deadlock)
// ============================================================
func bad2(mu *sync.Mutex) {
    mu.Lock()
    // ... codigo que puede retornar temprano ...
    if someCondition {
        return // ← Unlock NUNCA se ejecuta → deadlock
    }
    mu.Unlock()
}

// FIX: defer
func good2(mu *sync.Mutex) {
    mu.Lock()
    defer mu.Unlock()
    if someCondition {
        return // defer ejecuta Unlock automaticamente
    }
}

// ============================================================
// ERROR 3: Unlock sin Lock (panic)
// ============================================================
func bad3(mu *sync.Mutex) {
    mu.Unlock() // panic: sync: unlock of unlocked mutex
}

// ============================================================
// ERROR 4: Re-entrant lock (deadlock)
// ============================================================
type Bad4 struct {
    mu sync.Mutex
}

func (b *Bad4) outer() {
    b.mu.Lock()
    defer b.mu.Unlock()
    b.inner() // DEADLOCK — inner intenta lockear el mismo mutex
}

func (b *Bad4) inner() {
    b.mu.Lock() // ← DEADLOCK: ya esta lockeado por outer
    defer b.mu.Unlock()
    // ...
}

// FIX: separar la logica interna (sin lock) de la API publica (con lock)
type Good4 struct {
    mu sync.Mutex
}

func (g *Good4) outer() {
    g.mu.Lock()
    defer g.mu.Unlock()
    g.innerLocked() // la version que ASUME que ya tiene el lock
}

func (g *Good4) inner() {
    g.mu.Lock()
    defer g.mu.Unlock()
    g.innerLocked()
}

func (g *Good4) innerLocked() { // nombre indica "caller must hold lock"
    // logica sin Lock/Unlock
}

// NOTA: Go NO tiene mutex re-entrante (a diferencia de Java).
// Esto es intencional — re-entrant mutex esconden bugs de diseño.
// Si necesitas re-entrancia, tu API necesita refactorizarse.

// ============================================================
// ERROR 5: Lock ordering inconsistente (deadlock)
// ============================================================
func bad5_goroutine1(mu1, mu2 *sync.Mutex) {
    mu1.Lock()
    defer mu1.Unlock()
    mu2.Lock()        // espera mu2
    defer mu2.Unlock()
}

func bad5_goroutine2(mu1, mu2 *sync.Mutex) {
    mu2.Lock()        // espera mu1 — DEADLOCK con goroutine1
    defer mu2.Unlock()
    mu1.Lock()
    defer mu1.Unlock()
}

// Goroutine 1: tiene mu1, espera mu2
// Goroutine 2: tiene mu2, espera mu1
// → Deadlock circular

// FIX: SIEMPRE adquirir locks en el mismo orden
func good5_goroutine1(mu1, mu2 *sync.Mutex) {
    mu1.Lock() // primero mu1
    defer mu1.Unlock()
    mu2.Lock() // despues mu2
    defer mu2.Unlock()
}

func good5_goroutine2(mu1, mu2 *sync.Mutex) {
    mu1.Lock() // primero mu1 (MISMO ORDEN)
    defer mu1.Unlock()
    mu2.Lock() // despues mu2
    defer mu2.Unlock()
}

// ============================================================
// ERROR 6: Seccion critica demasiado grande
// ============================================================
func bad6(mu *sync.Mutex, data map[string]int) {
    mu.Lock()
    defer mu.Unlock()
    
    // TODA esta logica esta dentro del lock — contention alta
    for k, v := range data {
        result := expensiveComputation(v)       // ← NO necesita el lock
        data[k] = result                         // ← SI necesita el lock
        sendToExternalService(k, result)          // ← NO necesita el lock (y es LENTA)
    }
}

// FIX: minimizar la seccion critica
func good6(mu *sync.Mutex, data map[string]int) {
    // 1. Copiar los datos que necesitamos (bajo lock)
    mu.Lock()
    snapshot := make(map[string]int, len(data))
    for k, v := range data {
        snapshot[k] = v
    }
    mu.Unlock()
    
    // 2. Procesar sin lock
    results := make(map[string]int)
    for k, v := range snapshot {
        results[k] = expensiveComputation(v)
    }
    
    // 3. Escribir resultados (bajo lock)
    mu.Lock()
    for k, v := range results {
        data[k] = v
    }
    mu.Unlock()
    
    // 4. Notificar sin lock
    for k, v := range results {
        sendToExternalService(k, v)
    }
}

// ============================================================
// ERROR 7: RLock cuando necesitas Lock
// ============================================================
func bad7(rw *sync.RWMutex, data map[string]int) {
    rw.RLock()         // ← READ lock
    data["key"]++      // ← WRITE operation — DATA RACE
    rw.RUnlock()
    // Multiples goroutines con RLock pueden hacer data["key"]++ simultaneamente
}

// FIX: usar Lock para operaciones de escritura
func good7(rw *sync.RWMutex, data map[string]int) {
    rw.Lock()          // ← WRITE lock
    data["key"]++
    rw.Unlock()
}

// ============================================================
// ERROR 8: Escapar datos protegidos por mutex
// ============================================================
type Bad8 struct {
    mu   sync.Mutex
    data []int
}

func (b *Bad8) GetData() []int {
    b.mu.Lock()
    defer b.mu.Unlock()
    return b.data // ← PELIGRO: retorna referencia al slice interno
    // El caller puede modificar b.data SIN el lock → data race
}

// FIX: retornar una copia
func (b *Bad8) GetDataFixed() []int {
    b.mu.Lock()
    defer b.mu.Unlock()
    result := make([]int, len(b.data))
    copy(result, b.data)
    return result // copia independiente — safe
}
```

### 5.2 Tabla de antipatrones

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ANTIPATRONES CON MUTEX                                │
├───────────────────────────────┬──────────────────────────────────────────┤
│ Antipatron                     │ Consecuencia                            │
├───────────────────────────────┼──────────────────────────────────────────┤
│ Copiar mutex (value receiver) │ Lock no protege nada (go vet detecta)  │
│ Lock sin Unlock               │ Deadlock                                │
│ Unlock sin Lock               │ Panic                                   │
│ Re-entrant lock               │ Deadlock (Go no soporta re-entrancy)   │
│ Lock ordering inconsistente   │ Deadlock circular                       │
│ Seccion critica grande        │ Alta contention, baja performance      │
│ RLock para escritura          │ Data race                               │
│ Escapar referencia protegida  │ Data race (caller sin lock)            │
│ Lock en hot path              │ Cuello de botella (usar atomic)         │
│ Mutex global para todo        │ Contention innecesaria                  │
└───────────────────────────────┴──────────────────────────────────────────┘
```

---

## 6. Mutex vs Channel — Decision Guide

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    MUTEX vs CHANNEL — GUIA DE DECISION                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ¿Los datos se MUEVEN entre goroutines?                                │
│  ├─ SI → Channel (transferencia de ownership)                          │
│  └─ NO → Mutex (acceso compartido en-place)                            │
│                                                                          │
│  ¿La operacion es "pasar trabajo a otro"?                              │
│  ├─ SI → Channel (fan-out, pipeline)                                   │
│  └─ NO → Mutex (leer/modificar estado compartido)                      │
│                                                                          │
│  ¿Necesitas notificar/senalizar?                                       │
│  ├─ SI → Channel (done, cancel) o Context                              │
│  └─ NO → Mutex                                                         │
│                                                                          │
│  ¿El acceso es read-heavy (>> 10:1)?                                   │
│  ├─ SI → RWMutex                                                       │
│  └─ NO → Mutex o Channel segun el caso                                 │
│                                                                          │
│  ¿Es un counter/flag simple?                                           │
│  ├─ SI → sync/atomic (mejor que Mutex para esto)                      │
│  └─ NO → Mutex o Channel                                               │
│                                                                          │
│  EJEMPLOS CONCRETOS:                                                     │
│                                                                          │
│  Channel:                           Mutex:                               │
│  ├─ Worker pool (fan-out/fan-in)   ├─ Cache (map[string]T)            │
│  ├─ Pipeline de procesamiento      ├─ Config compartida               │
│  ├─ Request/response              ├─ Metricas/contadores              │
│  ├─ Pub/sub, event bus             ├─ Connection pool metadata         │
│  ├─ Shutdown signaling            ├─ Registry de servicios            │
│  └─ Rate limiting (token bucket)  └─ State machine compartida        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 6.1 Ejemplo: la misma funcionalidad con channel vs mutex

```go
package main

import (
    "fmt"
    "sync"
)

// ============================================================
// VERSION CHANNEL: counter como goroutine con channel
// ============================================================

type ChannelCounter struct {
    incCh chan struct{}
    getCh chan int
}

func NewChannelCounter() *ChannelCounter {
    c := &ChannelCounter{
        incCh: make(chan struct{}),
        getCh: make(chan int),
    }
    go c.loop()
    return c
}

func (c *ChannelCounter) loop() {
    var count int
    for {
        select {
        case <-c.incCh:
            count++
        case c.getCh <- count:
        }
    }
}

func (c *ChannelCounter) Inc() {
    c.incCh <- struct{}{}
}

func (c *ChannelCounter) Get() int {
    return <-c.getCh
}

// ============================================================
// VERSION MUTEX: counter con mutex
// ============================================================

type MutexCounter struct {
    mu    sync.Mutex
    count int
}

func (c *MutexCounter) Inc() {
    c.mu.Lock()
    defer c.mu.Unlock()
    c.count++
}

func (c *MutexCounter) Get() int {
    c.mu.Lock()
    defer c.mu.Unlock()
    return c.count
}

func main() {
    // Channel version
    cc := NewChannelCounter()
    var wg sync.WaitGroup
    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            cc.Inc()
        }()
    }
    wg.Wait()
    fmt.Println("Channel counter:", cc.Get()) // 1000

    // Mutex version
    mc := &MutexCounter{}
    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            mc.Inc()
        }()
    }
    wg.Wait()
    fmt.Println("Mutex counter:", mc.Get()) // 1000

    // Para un counter simple, Mutex es claramente mejor:
    // - Menos codigo (no necesita goroutine de background)
    // - Mejor performance (~20ns vs ~200ns por operacion)
    // - No hay goroutine leak (channel version: loop() nunca termina)
    // - Mas facil de razonar
    //
    // Channel seria mejor si el counter fuera parte de un pipeline
    // o si necesitara notificar cuando cambia
}
```

---

## 7. Patrones Avanzados

### 7.1 Mutex con timeout (usando channel)

Go's `sync.Mutex` no tiene timeout nativo. Si necesitas timeout, combina con un channel:

```go
package main

import (
    "context"
    "fmt"
    "sync"
    "time"
)

type TimedMutex struct {
    ch chan struct{}
}

func NewTimedMutex() *TimedMutex {
    ch := make(chan struct{}, 1)
    ch <- struct{}{} // inicializar con un "token"
    return &TimedMutex{ch: ch}
}

func (m *TimedMutex) Lock(ctx context.Context) error {
    select {
    case <-m.ch:
        return nil // adquirido
    case <-ctx.Done():
        return ctx.Err() // timeout o cancelacion
    }
}

func (m *TimedMutex) Unlock() {
    m.ch <- struct{}{}
}

func main() {
    mu := NewTimedMutex()
    
    // Goroutine 1: adquiere y mantiene por 2 segundos
    go func() {
        _ = mu.Lock(context.Background())
        fmt.Println("G1: lock adquirido")
        time.Sleep(2 * time.Second)
        mu.Unlock()
        fmt.Println("G1: lock liberado")
    }()
    
    time.Sleep(100 * time.Millisecond) // asegurar que G1 tiene el lock
    
    // Goroutine 2: intenta con timeout de 500ms
    ctx, cancel := context.WithTimeout(context.Background(), 500*time.Millisecond)
    defer cancel()
    
    if err := mu.Lock(ctx); err != nil {
        fmt.Printf("G2: no pudo adquirir lock: %v\n", err)
    }
    // Output:
    // G1: lock adquirido
    // G2: no pudo adquirir lock: context deadline exceeded
}
```

### 7.2 Striped locks (reducir contention)

En lugar de un solo mutex para toda una estructura, usar N mutexes (stripes) basados en un hash del key:

```go
package main

import (
    "fmt"
    "hash/fnv"
    "sync"
)

const numStripes = 16

type StripedMap struct {
    stripes [numStripes]struct {
        mu   sync.RWMutex
        data map[string]string
    }
}

func NewStripedMap() *StripedMap {
    sm := &StripedMap{}
    for i := range sm.stripes {
        sm.stripes[i].data = make(map[string]string)
    }
    return sm
}

func (sm *StripedMap) stripe(key string) int {
    h := fnv.New32a()
    h.Write([]byte(key))
    return int(h.Sum32()) % numStripes
}

func (sm *StripedMap) Get(key string) (string, bool) {
    idx := sm.stripe(key)
    sm.stripes[idx].mu.RLock()
    defer sm.stripes[idx].mu.RUnlock()
    val, ok := sm.stripes[idx].data[key]
    return val, ok
}

func (sm *StripedMap) Set(key, value string) {
    idx := sm.stripe(key)
    sm.stripes[idx].mu.Lock()
    defer sm.stripes[idx].mu.Unlock()
    sm.stripes[idx].data[key] = value
}

func main() {
    sm := NewStripedMap()
    
    var wg sync.WaitGroup
    
    // 100 writers concurrentes — solo compiten si caen en la misma stripe
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            key := fmt.Sprintf("key-%d", id)
            sm.Set(key, fmt.Sprintf("value-%d", id))
        }(i)
    }
    
    wg.Wait()
    
    // Verificar
    val, _ := sm.Get("key-42")
    fmt.Println("key-42:", val)
    
    // Con 16 stripes y 100 keys, la contention promedio es
    // 100/16 ≈ 6 goroutines por stripe en lugar de 100 en un solo mutex
}
```

### 7.3 Copy-on-write con RWMutex

```go
package main

import (
    "fmt"
    "sync"
    "sync/atomic"
)

// Config usa copy-on-write: las lecturas no necesitan lock
type Config struct {
    ptr atomic.Pointer[configData]
    mu  sync.Mutex // solo para serializar writes
}

type configData struct {
    Host     string
    Port     int
    Features map[string]bool
}

func NewConfig(host string, port int) *Config {
    c := &Config{}
    c.ptr.Store(&configData{
        Host:     host,
        Port:     port,
        Features: make(map[string]bool),
    })
    return c
}

// Get es lock-free — solo atomic load
func (c *Config) Get() configData {
    return *c.ptr.Load() // copia del valor (no del puntero)
}

// Update serializa writes y hace copy-on-write
func (c *Config) Update(fn func(*configData)) {
    c.mu.Lock()
    defer c.mu.Unlock()
    
    // Copiar datos actuales
    current := c.ptr.Load()
    newData := &configData{
        Host:     current.Host,
        Port:     current.Port,
        Features: make(map[string]bool, len(current.Features)),
    }
    for k, v := range current.Features {
        newData.Features[k] = v
    }
    
    // Aplicar la modificacion a la copia
    fn(newData)
    
    // Atomic swap — los readers ven la version vieja O la nueva, nunca una parcial
    c.ptr.Store(newData)
}

func main() {
    config := NewConfig("localhost", 8080)
    
    var wg sync.WaitGroup
    
    // 10 writers
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            config.Update(func(c *configData) {
                c.Features[fmt.Sprintf("feature-%d", id)] = true
            })
        }(i)
    }
    
    // 100 readers — NO necesitan lock
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            cfg := config.Get() // lock-free atomic load
            _ = cfg.Host        // leer sin preocupaciones
        }()
    }
    
    wg.Wait()
    
    final := config.Get()
    fmt.Printf("Host: %s, Port: %d, Features: %d\n",
        final.Host, final.Port, len(final.Features))
}
```

---

## 8. Comparacion con C y Rust

### 8.1 C — pthread_mutex

```c
#include <pthread.h>
#include <stdio.h>

// C: pthread_mutex_t
pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
int counter = 0;

void *worker(void *arg) {
    for (int i = 0; i < 1000; i++) {
        pthread_mutex_lock(&mu);
        counter++;
        pthread_mutex_unlock(&mu);
    }
    return NULL;
}

int main() {
    pthread_t threads[10];
    for (int i = 0; i < 10; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }
    for (int i = 0; i < 10; i++) {
        pthread_join(threads[i], NULL);
    }
    printf("Counter: %d\n", counter); // 10000
    
    pthread_mutex_destroy(&mu);
    return 0;
}

// DIFERENCIAS con Go:
// - pthread_mutex_t requiere inicializacion y destruccion explicita
// - No hay defer — facil olvidar unlock (especialmente con returns)
// - pthread_rwlock_t existe pero la API es mas verbose
// - pthread_mutex_trylock() es equivalente a TryLock()
// - Copiar un pthread_mutex_t es undefined behavior
//   (Go: go vet detecta copias de sync.Mutex)
// - No hay starvation mode — depende de la implementacion del OS
// - pthread tiene RECURSIVE mutex (PTHREAD_MUTEX_RECURSIVE)
//   Go NO tiene recursive mutex (intencionalmente)
```

### 8.2 Rust — std::sync::Mutex<T>

```rust
use std::sync::{Mutex, RwLock, Arc};
use std::thread;

fn main() {
    // Rust: Mutex<T> ENVUELVE los datos
    // No puedes acceder a los datos sin adquirir el lock
    let counter = Arc::new(Mutex::new(0)); // Mutex protege un i32
    
    let mut handles = vec![];
    
    for _ in 0..10 {
        let counter = Arc::clone(&counter);
        handles.push(thread::spawn(move || {
            for _ in 0..1000 {
                let mut num = counter.lock().unwrap();
                // num es un MutexGuard<i32> — RAII lock
                *num += 1;
                // lock se libera automaticamente cuando num sale del scope
                // ¡No hay Unlock() explicito!
            }
        }));
    }
    
    for h in handles {
        h.join().unwrap();
    }
    
    println!("Counter: {}", *counter.lock().unwrap()); // 10000
}

// DIFERENCIAS FUNDAMENTALES con Go:
//
// 1. Mutex<T> CONTIENE los datos — imposible acceder sin lock
//    Go: Mutex y datos son campos separados — puedes olvidar lockear
//
// 2. MutexGuard<T> libera automaticamente (RAII, como defer pero mejor)
//    Go: necesitas defer mu.Unlock() (puedes olvidarlo)
//
// 3. El compilador PROHIBE data races (Send/Sync traits)
//    Go: data races son runtime errors (race detector)
//
// 4. lock() retorna Result — puede fallar (poisoned mutex)
//    Go: Lock() nunca falla (pero puede deadlock)
//
// 5. No hay TryLock en std (hay try_lock)
//    Go: TryLock desde Go 1.18
//
// 6. RwLock<T> equivale a RWMutex — misma semantica
//    let data = RwLock::new(vec![1, 2, 3]);
//    let reader = data.read().unwrap();  // RLock
//    let writer = data.write().unwrap(); // Lock

// RwLock ejemplo:
fn rwlock_example() {
    let config = Arc::new(RwLock::new(String::from("initial")));
    
    // 10 readers simultaneos
    for i in 0..10 {
        let cfg = Arc::clone(&config);
        thread::spawn(move || {
            let val = cfg.read().unwrap(); // RLock — multiples simultaneos OK
            println!("Reader {}: {}", i, *val);
            // RwLockReadGuard se dropea aqui — RUnlock automatico
        });
    }
    
    // 1 writer
    {
        let mut val = config.write().unwrap(); // Lock exclusivo
        *val = String::from("updated");
        // RwLockWriteGuard se dropea aqui — Unlock automatico
    }
}
```

### 8.3 Tabla comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    MUTEX: GO vs C vs RUST                                │
├──────────────────┬─────────────────┬───────────────┬─────────────────────┤
│ Aspecto           │ C               │ Go            │ Rust               │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Tipo              │ pthread_mutex_t │ sync.Mutex    │ Mutex<T>           │
│                   │ (separado)      │ (separado)    │ (envuelve datos)   │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Init/Destroy      │ init()/destroy()│ Zero value    │ Mutex::new(val)    │
│                   │ (manual)        │ (automatico)  │ (automatico)       │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Unlock            │ Manual          │ Manual        │ Automatico (RAII)  │
│                   │ (olvidable)     │ (defer ayuda) │ (Drop trait)       │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Datos protegidos  │ Convencion      │ Convencion    │ Compilador enforce │
│                   │ (documentacion) │ (mu + campos) │ (Mutex<T>)         │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Data race safety  │ Ninguna (UB)    │ Runtime       │ Compile-time       │
│                   │                 │ (-race flag)  │ (Send/Sync)        │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ RWMutex           │ pthread_rwlock  │ sync.RWMutex  │ RwLock<T>         │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Recursive         │ Si (config)     │ No (intenc.)  │ No (intencional)  │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Poisoning         │ No              │ No            │ Si (panic in lock) │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Copiar            │ UB              │ go vet detect │ !Send (no compila)│
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Starvation prev.  │ OS-dependent    │ Si (Go 1.9+)  │ OS-dependent      │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ TryLock           │ Si              │ Si (Go 1.18+) │ Si (try_lock)     │
└──────────────────┴─────────────────┴───────────────┴─────────────────────┘
```

---

## 9. Programa Completo — Infrastructure Service Registry

Un registry de servicios thread-safe que demuestra Mutex, RWMutex, defer, mutex en structs, striped locks, y la decision channel vs mutex:

```go
package main

import (
    "fmt"
    "math/rand"
    "strings"
    "sync"
    "sync/atomic"
    "time"
)

// ============================================================
// Tipos
// ============================================================

type ServiceStatus string

const (
    StatusHealthy   ServiceStatus = "HEALTHY"
    StatusUnhealthy ServiceStatus = "UNHEALTHY"
    StatusDraining  ServiceStatus = "DRAINING"
    StatusUnknown   ServiceStatus = "UNKNOWN"
)

type ServiceInstance struct {
    ID       string
    Host     string
    Port     int
    Status   ServiceStatus
    LastSeen time.Time
    Metadata map[string]string
}

// ============================================================
// ServiceRegistry — RWMutex para registro read-heavy
// ============================================================

type ServiceRegistry struct {
    mu       sync.RWMutex
    services map[string][]ServiceInstance // service name → instances
    
    // Metricas separadas con su propio mutex (reducir contention)
    statsMu sync.Mutex
    stats   RegistryStats
}

type RegistryStats struct {
    Registers   int64
    Deregisters int64
    Lookups     int64
    HealthChecks int64
}

func NewServiceRegistry() *ServiceRegistry {
    return &ServiceRegistry{
        services: make(map[string][]ServiceInstance),
    }
}

// Register — write lock (modificacion)
func (r *ServiceRegistry) Register(name string, instance ServiceInstance) {
    r.mu.Lock()
    defer r.mu.Unlock()
    
    instance.LastSeen = time.Now()
    instance.Status = StatusHealthy
    
    // Verificar duplicados
    for i, existing := range r.services[name] {
        if existing.ID == instance.ID {
            r.services[name][i] = instance // actualizar
            r.recordStat(func(s *RegistryStats) { s.Registers++ })
            return
        }
    }
    
    r.services[name] = append(r.services[name], instance)
    r.recordStat(func(s *RegistryStats) { s.Registers++ })
}

// Deregister — write lock
func (r *ServiceRegistry) Deregister(name, instanceID string) bool {
    r.mu.Lock()
    defer r.mu.Unlock()
    
    instances, ok := r.services[name]
    if !ok {
        return false
    }
    
    for i, inst := range instances {
        if inst.ID == instanceID {
            r.services[name] = append(instances[:i], instances[i+1:]...)
            if len(r.services[name]) == 0 {
                delete(r.services, name)
            }
            r.recordStat(func(s *RegistryStats) { s.Deregisters++ })
            return true
        }
    }
    return false
}

// Lookup — read lock (lectura concurrente)
func (r *ServiceRegistry) Lookup(name string) []ServiceInstance {
    r.mu.RLock()
    defer r.mu.RUnlock()
    
    r.recordStat(func(s *RegistryStats) { s.Lookups++ })
    
    instances := r.services[name]
    if instances == nil {
        return nil
    }
    
    // Retornar solo instancias healthy (copia para evitar data races)
    var healthy []ServiceInstance
    for _, inst := range instances {
        if inst.Status == StatusHealthy {
            // Copiar para no exponer datos internos
            copy := inst
            copy.Metadata = make(map[string]string, len(inst.Metadata))
            for k, v := range inst.Metadata {
                copy.Metadata[k] = v
            }
            healthy = append(healthy, copy)
        }
    }
    return healthy
}

// LookupRoundRobin — read lock + atomic counter para round-robin
var rrCounter atomic.Int64

func (r *ServiceRegistry) LookupRoundRobin(name string) (ServiceInstance, bool) {
    instances := r.Lookup(name)
    if len(instances) == 0 {
        return ServiceInstance{}, false
    }
    idx := rrCounter.Add(1) % int64(len(instances))
    return instances[idx], true
}

// AllServices — read lock
func (r *ServiceRegistry) AllServices() map[string]int {
    r.mu.RLock()
    defer r.mu.RUnlock()
    
    result := make(map[string]int, len(r.services))
    for name, instances := range r.services {
        result[name] = len(instances)
    }
    return result
}

// HealthCheck — write lock (modifica status)
func (r *ServiceRegistry) HealthCheck() map[string]ServiceStatus {
    r.mu.Lock()
    defer r.mu.Unlock()
    
    r.recordStat(func(s *RegistryStats) { s.HealthChecks++ })
    
    results := make(map[string]ServiceStatus)
    
    for name, instances := range r.services {
        for i := range instances {
            // Marcar como unhealthy si no se ve hace > 30s
            if time.Since(instances[i].LastSeen) > 30*time.Second {
                instances[i].Status = StatusUnhealthy
            }
            results[fmt.Sprintf("%s/%s", name, instances[i].ID)] = instances[i].Status
        }
    }
    
    return results
}

// UpdateHeartbeat — write lock (modifica LastSeen)
func (r *ServiceRegistry) UpdateHeartbeat(name, instanceID string) bool {
    r.mu.Lock()
    defer r.mu.Unlock()
    
    instances, ok := r.services[name]
    if !ok {
        return false
    }
    
    for i := range instances {
        if instances[i].ID == instanceID {
            instances[i].LastSeen = time.Now()
            instances[i].Status = StatusHealthy
            return true
        }
    }
    return false
}

// recordStat usa su propio mutex para no mezclar con el RWMutex de datos
func (r *ServiceRegistry) recordStat(fn func(*RegistryStats)) {
    r.statsMu.Lock()
    defer r.statsMu.Unlock()
    fn(&r.stats)
}

func (r *ServiceRegistry) Stats() RegistryStats {
    r.statsMu.Lock()
    defer r.statsMu.Unlock()
    return r.stats
}

// ============================================================
// Simulacion
// ============================================================

func simulateRegistrations(registry *ServiceRegistry, wg *sync.WaitGroup) {
    defer wg.Done()
    
    services := []struct {
        name      string
        instances int
        basePort  int
    }{
        {"api-gateway", 3, 8080},
        {"user-service", 2, 8081},
        {"order-service", 3, 8082},
        {"payment-service", 2, 8083},
        {"notification-service", 1, 8084},
    }
    
    for _, svc := range services {
        for i := 1; i <= svc.instances; i++ {
            registry.Register(svc.name, ServiceInstance{
                ID:   fmt.Sprintf("%s-%d", svc.name, i),
                Host: fmt.Sprintf("10.0.%d.%d", rand.Intn(255), rand.Intn(255)),
                Port: svc.basePort,
                Metadata: map[string]string{
                    "version": fmt.Sprintf("v1.%d.0", rand.Intn(10)),
                    "region":  []string{"us-east-1", "us-west-2", "eu-west-1"}[rand.Intn(3)],
                },
            })
        }
    }
}

func simulateReaders(registry *ServiceRegistry, wg *sync.WaitGroup, duration time.Duration) {
    defer wg.Done()
    
    end := time.After(duration)
    services := []string{"api-gateway", "user-service", "order-service",
        "payment-service", "notification-service"}
    
    for {
        select {
        case <-end:
            return
        default:
            name := services[rand.Intn(len(services))]
            instances := registry.Lookup(name)
            _ = instances
            time.Sleep(time.Duration(1+rand.Intn(5)) * time.Millisecond)
        }
    }
}

func simulateHeartbeats(registry *ServiceRegistry, wg *sync.WaitGroup, duration time.Duration) {
    defer wg.Done()
    
    ticker := time.NewTicker(100 * time.Millisecond)
    defer ticker.Stop()
    end := time.After(duration)
    
    for {
        select {
        case <-ticker.C:
            all := registry.AllServices()
            for name := range all {
                // Simular heartbeat de algunas instancias
                for i := 1; i <= 3; i++ {
                    registry.UpdateHeartbeat(name, fmt.Sprintf("%s-%d", name, i))
                }
            }
        case <-end:
            return
        }
    }
}

func main() {
    registry := NewServiceRegistry()
    
    fmt.Println("╔══════════════════════════════════════════════════════════════╗")
    fmt.Println("║           INFRASTRUCTURE SERVICE REGISTRY                    ║")
    fmt.Println("╠══════════════════════════════════════════════════════════════╣")
    fmt.Println("║  RWMutex for data | Mutex for stats | Atomic for round-robin║")
    fmt.Println("╚══════════════════════════════════════════════════════════════╝")
    fmt.Println()
    
    var wg sync.WaitGroup
    duration := 2 * time.Second
    
    // Fase 1: registrar servicios
    fmt.Println("--- Phase 1: Registering services ---")
    wg.Add(1)
    simulateRegistrations(registry, &wg)
    wg.Wait()
    
    all := registry.AllServices()
    totalInstances := 0
    for name, count := range all {
        fmt.Printf("  %-25s %d instances\n", name, count)
        totalInstances += count
    }
    fmt.Printf("  Total: %d instances\n\n", totalInstances)
    
    // Fase 2: lecturas concurrentes + heartbeats
    fmt.Println("--- Phase 2: Concurrent reads + heartbeats ---")
    fmt.Printf("  Running for %v with 50 readers + 5 heartbeaters...\n", duration)
    
    for i := 0; i < 50; i++ {
        wg.Add(1)
        go simulateReaders(registry, &wg, duration)
    }
    
    for i := 0; i < 5; i++ {
        wg.Add(1)
        go simulateHeartbeats(registry, &wg, duration)
    }
    
    wg.Wait()
    
    // Fase 3: round-robin lookup
    fmt.Println("\n--- Phase 3: Round-robin lookups ---")
    for i := 0; i < 6; i++ {
        if inst, ok := registry.LookupRoundRobin("api-gateway"); ok {
            fmt.Printf("  Request %d → %s (%s:%d)\n", i+1, inst.ID, inst.Host, inst.Port)
        }
    }
    
    // Stats
    stats := registry.Stats()
    fmt.Println()
    fmt.Println("┌──────────────────────────────────────────┐")
    fmt.Println("│              REGISTRY STATS               │")
    fmt.Println("├──────────────────────────────────────────┤")
    fmt.Printf("│  Registrations:   %-8d                │\n", stats.Registers)
    fmt.Printf("│  Deregistrations: %-8d                │\n", stats.Deregisters)
    fmt.Printf("│  Lookups:         %-8d                │\n", stats.Lookups)
    fmt.Printf("│  Health checks:   %-8d                │\n", stats.HealthChecks)
    fmt.Println("├──────────────────────────────────────────┤")
    fmt.Println("│  Synchronization used:                   │")
    fmt.Println("│    RWMutex: service data (read-heavy)   │")
    fmt.Println("│    Mutex:   stats (write-heavy)         │")
    fmt.Println("│    Atomic:  round-robin counter         │")
    fmt.Println("└──────────────────────────────────────────┘")
    
    fmt.Println()
    fmt.Println("Design decisions:")
    fmt.Println("  - RWMutex for services: 50 readers vs 5 writers → read-heavy")
    fmt.Println("  - Separate Mutex for stats: avoid mixing write contention")
    fmt.Println("  - Atomic for round-robin: single counter, no lock needed")
    fmt.Println("  - Copy on return: Lookup returns copies to prevent data races")
    fmt.Println("  - NOT channels: data is shared in-place, not transferred")
    
    _ = strings.Join // silenciar import
}
```

---

## 10. Ejercicios

### Ejercicio 1: Rate limiter thread-safe

Implementa un rate limiter usando `sync.Mutex`:

```go
type RateLimiter struct {
    // tokens, capacity, refillRate, lastRefill
    // mu sync.Mutex
}

func (rl *RateLimiter) Allow() bool           // consume 1 token (Mutex)
func (rl *RateLimiter) AllowN(n int) bool      // consume N tokens (Mutex)
func (rl *RateLimiter) Tokens() float64        // tokens actuales (RLock)
```

- Token bucket con refill basado en tiempo transcurrido
- Thread-safe con `sync.Mutex` para `Allow` (read-modify-write)
- `Tokens()` puede usar `RLock` si separas el read
- Demostrar con 20 goroutines haciendo 1000 requests cada una
- Comparar performance con version usando channel como token bucket

### Ejercicio 2: Concurrent LRU cache

Implementa un LRU cache thread-safe:

```go
type LRUCache struct {
    mu       sync.RWMutex
    capacity int
    items    map[string]*entry
    order    *list.List
}

func (c *LRUCache) Get(key string) (string, bool)  // RLock → promote (Lock)
func (c *LRUCache) Set(key, value string)            // Lock
func (c *LRUCache) Delete(key string) bool            // Lock
func (c *LRUCache) Len() int                          // RLock
```

- Get necesita Lock (no RLock) porque modifica el orden LRU
- Considerar la alternativa: RLock para check + Lock para promote
- Benchmark con diferentes ratios de read/write
- Comparar con `sync.Map` para el mismo caso de uso

### Ejercicio 3: Mutex hierarchy (multi-lock sin deadlock)

Implementa un sistema bancario con transferencias entre cuentas:

```go
type Account struct {
    id      int
    mu      sync.Mutex
    balance int
}

func Transfer(from, to *Account, amount int) error
```

- Cada cuenta tiene su propio mutex
- `Transfer` necesita lockear AMBAS cuentas simultaneamente
- Resolver el deadlock usando **lock ordering** (siempre lockear por ID ascendente)
- Demostrar que funciona con 100 goroutines haciendo transferencias aleatorias
- Verificar que el balance total se conserva

### Ejercicio 4: Read-copy-update (RCU) pattern

Implementa un config manager usando copy-on-write + atomic.Pointer:

```go
type ConfigManager struct {
    current atomic.Pointer[AppConfig]
    mu      sync.Mutex // solo para serializar writes
}
```

- Reads son completamente lock-free (atomic.Pointer.Load)
- Writes hacen copy-on-write bajo mutex
- Benchmark: comparar con RWMutex para 1000 readers + 1 writer
- Demostrar que reads nunca bloquean, incluso durante writes lentos

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    MUTEX — RESUMEN COMPLETO                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  sync.Mutex:                                                             │
│  ├─ Lock(): adquirir (bloquea si ya esta lockeado)                    │
│  ├─ Unlock(): liberar (panic si no esta lockeado)                     │
│  ├─ TryLock(): intentar sin bloquear (Go 1.18+, rara vez necesario)   │
│  ├─ Zero value: listo para usar (no necesita inicializacion)          │
│  ├─ Normal + Starvation mode (Go 1.9+, previene starvation)          │
│  └─ ~20ns uncontended, ~100-500ns contended                           │
│                                                                          │
│  sync.RWMutex:                                                           │
│  ├─ RLock/RUnlock: multiples readers simultaneos                      │
│  ├─ Lock/Unlock: escritor exclusivo                                   │
│  ├─ Writer priority: nuevos readers esperan si hay writer pendiente   │
│  ├─ Usar cuando reads >> writes (>= 10:1)                             │
│  └─ 3-10x mas rapido que Mutex para read-heavy workloads              │
│                                                                          │
│  PATRON IDIOMATICO:                                                      │
│  ├─ mu.Lock()                                                          │
│  ├─ defer mu.Unlock()  (SIEMPRE en la linea siguiente)                │
│  ├─ Mutex en struct: justo arriba de los campos que protege           │
│  ├─ Metodos encapsulan Lock/Unlock (API thread-safe)                  │
│  └─ NUNCA exportar un mutex                                            │
│                                                                          │
│  ANTIPATRONES:                                                           │
│  ├─ Copiar mutex (value receiver) → go vet detecta                    │
│  ├─ Lock sin Unlock → deadlock                                         │
│  ├─ Re-entrant lock → deadlock (Go NO tiene recursive mutex)          │
│  ├─ Lock ordering inconsistente → deadlock circular                    │
│  ├─ Seccion critica grande → alta contention                          │
│  ├─ RLock para escritura → data race                                  │
│  └─ Escapar referencia protegida → data race del caller               │
│                                                                          │
│  MUTEX vs CHANNEL:                                                       │
│  ├─ Datos se MUEVEN → Channel                                         │
│  ├─ Datos se COMPARTEN en-place → Mutex                                │
│  ├─ Senalizacion/cancelacion → Channel o Context                      │
│  ├─ Counter/flag simple → sync/atomic                                  │
│  └─ Cache, config, registry → Mutex (RWMutex si read-heavy)          │
│                                                                          │
│  PATRONES AVANZADOS:                                                     │
│  ├─ Mutex con timeout (channel + select)                               │
│  ├─ Striped locks (N mutex por hash, reduce contention)               │
│  ├─ Copy-on-write (atomic.Pointer + mutex para writes)                │
│  └─ Lock ordering (siempre adquirir en orden de ID)                   │
│                                                                          │
│  vs C: pthread_mutex_t (manual init/destroy, UB en copia, recursive)  │
│  vs Rust: Mutex<T> envuelve datos, RAII unlock, compile-time safety   │
│  Go: zero value, defer unlock, go vet detect, runtime race detection  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S03 - Sincronizacion, T02 - Once, Map, Pool — sync.Once (singleton), sync.Map (map concurrente), sync.Pool (reciclaje de objetos)
