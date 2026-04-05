# Once, Map, Pool — sync.Once (singleton), sync.Map (map concurrente), sync.Pool (reciclaje de objetos)

## 1. Introduccion

El paquete `sync` provee tres tipos especializados que resuelven problemas de concurrencia extremadamente comunes: **sync.Once** para inicializacion que solo ocurre una vez, **sync.Map** para maps concurrentes sin lock explicito, y **sync.Pool** para reciclar objetos y reducir presion sobre el garbage collector. Estos tres tipos complementan a Mutex y WaitGroup, y cada uno esta optimizado para su caso de uso especifico.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    SYNC: TIPOS ESPECIALIZADOS                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  sync.Once:                                                              │
│  ├─ Ejecuta una funcion EXACTAMENTE una vez                            │
│  ├─ Thread-safe: N goroutines llaman, solo 1 ejecuta                   │
│  ├─ Las demas ESPERAN a que la primera termine                         │
│  └─ Caso tipico: lazy init, singleton, setup unico                    │
│                                                                          │
│  sync.Map:                                                               │
│  ├─ Map concurrente sin lock explicito                                 │
│  ├─ Optimizado para 2 patrones: read-heavy y disjoint-keys            │
│  ├─ NO es un reemplazo general de map + Mutex                         │
│  └─ Caso tipico: cache, registry, write-once config                   │
│                                                                          │
│  sync.Pool:                                                              │
│  ├─ Pool de objetos reutilizables (reduce allocations)                 │
│  ├─ El GC puede vaciar el pool en cualquier momento                   │
│  ├─ NO es un pool de conexiones/workers                                │
│  └─ Caso tipico: buffers, encoders, temporary objects                  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

Para SysAdmin/DevOps:
- `sync.Once` se usa para cargar configuracion, inicializar loggers, y conectar a bases de datos la primera vez que se necesitan
- `sync.Map` se usa en caches internos de DNS resolvers, service discovery registries, y stores de metricas
- `sync.Pool` se usa en servidores HTTP de alto rendimiento para reciclar buffers, encoders JSON, y objetos temporales — reduciendo allocations y pausas de GC

---

## 2. sync.Once — Ejecutar Exactamente Una Vez

### 2.1 API

```go
type Once struct {
    // campos no exportados
}

func (o *Once) Do(f func())   // ejecuta f exactamente una vez
```

`Do` garantiza que:
1. `f` se ejecuta **exactamente una vez**, sin importar cuantas goroutines llamen `Do`
2. Todas las goroutines que llaman `Do` **esperan** hasta que `f` termine
3. Despues de que `f` retorna, las llamadas subsiguientes a `Do` retornan inmediatamente (no ejecutan `f`)

### 2.2 Uso basico — Lazy initialization

```go
package main

import (
    "fmt"
    "sync"
)

type Database struct {
    host string
}

var (
    dbInstance *Database
    dbOnce     sync.Once
)

func GetDB() *Database {
    dbOnce.Do(func() {
        fmt.Println("  Conectando a la base de datos... (solo una vez)")
        dbInstance = &Database{host: "postgres://localhost:5432"}
    })
    return dbInstance
}

func main() {
    var wg sync.WaitGroup
    
    // 10 goroutines piden la DB simultaneamente
    for i := 1; i <= 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            db := GetDB()
            fmt.Printf("  Goroutine %d: usando DB en %s\n", id, db.host)
        }(i)
    }
    
    wg.Wait()
    // Output:
    //   Conectando a la base de datos... (solo una vez)  ← aparece UNA sola vez
    //   Goroutine 3: usando DB en postgres://localhost:5432
    //   Goroutine 1: usando DB en postgres://localhost:5432
    //   ... (todas usan la misma instancia)
}
```

### 2.3 Internals

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    sync.Once — INTERNALS                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  type Once struct {                                                     │
│      done atomic.Uint32   // 0 = no ejecutado, 1 = completado          │
│      m    Mutex           // protege la ejecucion de f                  │
│  }                                                                       │
│                                                                          │
│  func (o *Once) Do(f func()) {                                         │
│      // Fast path: si ya se ejecuto, retorna inmediatamente            │
│      if o.done.Load() != 0 {  // atomic read — sin lock               │
│          return                                                          │
│      }                                                                   │
│      // Slow path: primera vez (o competencia)                         │
│      o.doSlow(f)                                                        │
│  }                                                                       │
│                                                                          │
│  func (o *Once) doSlow(f func()) {                                     │
│      o.m.Lock()                                                         │
│      defer o.m.Unlock()                                                 │
│      if o.done.Load() == 0 {   // double-check bajo lock              │
│          defer o.done.Store(1)  // marcar como completado              │
│          f()                    // ejecutar                             │
│      }                                                                   │
│  }                                                                       │
│                                                                          │
│  SECUENCIA con 3 goroutines:                                            │
│                                                                          │
│  G1: Do() → done=0 → doSlow() → Lock() → done=0 → f() → done=1     │
│  G2: Do() → done=0 → doSlow() → Lock() [BLOQUEADA esperando G1]      │
│  G3: Do() → done=0 → doSlow() → Lock() [BLOQUEADA esperando G1]      │
│                                                                          │
│  G1 termina f(), Unlock():                                              │
│  G2: Lock() → done=1 → skip f() → Unlock()                           │
│  G3: Lock() → done=1 → skip f() → Unlock()                           │
│                                                                          │
│  Llamadas posteriores:                                                   │
│  G4: Do() → done=1 → return (fast path, sin lock)                     │
│                                                                          │
│  NOTA CRITICA: done se marca DESPUES de que f() retorna.               │
│  Si f() hace panic, done NO se marca → la siguiente llamada           │
│  intentara ejecutar f() de nuevo. Esto cambio en Go 1.21:             │
│  ahora done se marca incluso si f() hace panic.                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.4 Patrones comunes con sync.Once

```go
package main

import (
    "fmt"
    "log"
    "os"
    "sync"
)

// ============================================================
// Patron 1: Singleton (instancia global unica)
// ============================================================

type Logger struct {
    file *os.File
}

var (
    loggerInstance *Logger
    loggerOnce     sync.Once
)

func GetLogger() *Logger {
    loggerOnce.Do(func() {
        // Inicializacion costosa — solo se ejecuta una vez
        loggerInstance = &Logger{file: os.Stdout}
        log.Println("Logger inicializado")
    })
    return loggerInstance
}

// ============================================================
// Patron 2: Lazy init con error handling
// ============================================================

type Config struct {
    Host string
    Port int
}

var (
    configInstance *Config
    configErr      error
    configOnce     sync.Once
)

func GetConfig() (*Config, error) {
    configOnce.Do(func() {
        // Si esto falla, el error se captura
        configInstance, configErr = loadConfig()
    })
    return configInstance, configErr
    // NOTA: si loadConfig fallo, TODAS las llamadas posteriores
    // retornan el mismo error. Once no reintenta.
}

func loadConfig() (*Config, error) {
    // Simular carga de config
    return &Config{Host: "localhost", Port: 8080}, nil
}

// ============================================================
// Patron 3: sync.OnceFunc / sync.OnceValue (Go 1.21+)
// ============================================================

// Go 1.21 agrego helpers que simplifican los patrones anteriores:

// sync.OnceFunc retorna una funcion que solo ejecuta f una vez
// var initDB = sync.OnceFunc(func() {
//     db = connectToDB()
// })

// sync.OnceValue retorna una funcion que solo ejecuta f una vez y cachea el resultado
// var getConfig = sync.OnceValue(func() *Config {
//     return loadConfigFromFile()
// })
// cfg := getConfig()  // primera vez: ejecuta, despues: retorna cached

// sync.OnceValues es como OnceValue pero para (T, error)
// var getDB = sync.OnceValues(func() (*Database, error) {
//     return connectToDB()
// })
// db, err := getDB()  // primera vez: conecta, despues: retorna cached

// ============================================================
// Patron 4: Close-once (prevenir double-close panic)
// ============================================================

type SafeCloser struct {
    once sync.Once
    ch   chan struct{}
}

func NewSafeCloser() *SafeCloser {
    return &SafeCloser{ch: make(chan struct{})}
}

func (sc *SafeCloser) Close() {
    sc.once.Do(func() {
        close(sc.ch) // safe — solo se ejecuta una vez
    })
}

func (sc *SafeCloser) Done() <-chan struct{} {
    return sc.ch
}

// ============================================================
// Patron 5: Once por instancia (no global)
// ============================================================

type Service struct {
    initOnce sync.Once
    ready    bool
    name     string
}

func (s *Service) Init() {
    s.initOnce.Do(func() {
        fmt.Printf("  Inicializando servicio %s...\n", s.name)
        // Setup costoso...
        s.ready = true
    })
}

func main() {
    // Multiples servicios, cada uno con su propio Once
    services := []*Service{
        {name: "auth"},
        {name: "users"},
        {name: "orders"},
    }
    
    var wg sync.WaitGroup
    for _, svc := range services {
        for i := 0; i < 5; i++ {
            wg.Add(1)
            go func(s *Service) {
                defer wg.Done()
                s.Init() // cada servicio se inicializa una sola vez
            }(svc)
        }
    }
    
    wg.Wait()
    for _, svc := range services {
        fmt.Printf("  %s: ready=%v\n", svc.name, svc.ready)
    }
}
```

### 2.5 Errores comunes con sync.Once

```go
// ERROR 1: Usar Once.Do con diferentes funciones
var once sync.Once

func bad1() {
    once.Do(func() { fmt.Println("A") }) // ejecuta "A"
    once.Do(func() { fmt.Println("B") }) // NO ejecuta "B" — Once ya se uso
    // Solo la PRIMERA llamada a Do ejecuta su funcion
    // Las demas se ignoran, incluso si son funciones diferentes
}

// ERROR 2: Deadlock — llamar Do desde dentro de Do
var once2 sync.Once

func bad2() {
    once2.Do(func() {
        once2.Do(func() {}) // DEADLOCK — intenta lockear un mutex ya lockeado
    })
}

// ERROR 3: Esperar que Once "reintente" en error
var (
    dbOnce sync.Once
    db     *Database
)

func bad3() *Database {
    dbOnce.Do(func() {
        // Si esto falla, Once NO reintentara
        db, _ = connectToDB()
    })
    return db // puede ser nil si conecto fallo, y SIEMPRE sera nil despues
    // FIX: manejar el error separadamente o usar sync.OnceValues (Go 1.21+)
}

// ERROR 4: Copiar Once (mismo problema que Mutex)
func bad4() {
    var once sync.Once
    onceCopy := once     // COPIA — go vet advierte
    onceCopy.Do(func() { // lockea la COPIA, no el original
        fmt.Println("esto no protege nada")
    })
}
```

---

## 3. sync.Map — Map Concurrente Especializado

### 3.1 Por que existe sync.Map

Un `map[K]V` de Go **no es thread-safe**. Acceder a un map desde multiples goroutines sin sincronizacion causa un crash fatal (no solo data race — crash):

```go
// CRASH: concurrent map writes
m := make(map[string]int)
go func() { m["a"] = 1 }()
go func() { m["b"] = 2 }()
// fatal error: concurrent map writes
```

La solucion comun es `map + sync.Mutex` o `map + sync.RWMutex`. Pero para ciertos patrones de acceso, `sync.Map` es significativamente mas eficiente.

### 3.2 API

```go
type Map struct {
    // campos no exportados
}

func (m *Map) Store(key, value any)                      // escribir
func (m *Map) Load(key any) (value any, ok bool)         // leer
func (m *Map) LoadOrStore(key, value any) (actual any, loaded bool)  // leer o escribir
func (m *Map) LoadAndDelete(key any) (value any, loaded bool)        // leer y borrar
func (m *Map) Delete(key any)                             // borrar
func (m *Map) Range(f func(key, value any) bool)          // iterar (f retorna false para parar)
func (m *Map) Swap(key, value any) (previous any, loaded bool)  // Go 1.20+
func (m *Map) CompareAndSwap(key, old, new any) bool      // Go 1.20+ — CAS atomico
func (m *Map) CompareAndDelete(key, value any) bool       // Go 1.20+
```

### 3.3 Cuando usar sync.Map vs map+Mutex

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    sync.Map vs map + RWMutex                            │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  sync.Map esta OPTIMIZADO para exactamente 2 patrones:                 │
│                                                                          │
│  1. WRITE-ONCE, READ-MANY:                                              │
│     Los entries se escriben una vez y luego solo se leen.              │
│     Ejemplo: cache de configuracion que se puebla al inicio            │
│     y luego solo se consulta.                                           │
│                                                                          │
│  2. DISJOINT KEYS:                                                       │
│     Multiples goroutines leen/escriben, pero cada una trabaja          │
│     con un subconjunto diferente de keys (sin overlap).                │
│     Ejemplo: per-goroutine caches, per-connection state.               │
│                                                                          │
│  PARA TODO LO DEMAS → usa map + RWMutex:                              │
│  ├─ Keys compartidas con writes frecuentes                             │
│  ├─ Necesitas Len() (sync.Map NO tiene Len)                           │
│  ├─ Necesitas iterar frecuentemente (Range lockea internamente)       │
│  ├─ Tipos conocidos (sync.Map usa any — perdida de type safety)       │
│  └─ Benchmark muestra que map+Mutex es mas rapido para tu caso       │
│                                                                          │
│  PERFORMANCE:                                                            │
│  ├─ Read-only workload: sync.Map ~2-5x mas rapido que map+RWMutex    │
│  ├─ Disjoint writes: sync.Map ~2-3x mas rapido                       │
│  ├─ Shared writes: map+RWMutex ~1.5-3x mas rapido que sync.Map       │
│  └─ Mixed (shared keys): depende del ratio — benchmark tu caso       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.4 Internals — como funciona

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    sync.Map — INTERNALS                                 │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  sync.Map usa dos maps internos:                                       │
│                                                                          │
│  type Map struct {                                                      │
│      mu     Mutex                                                       │
│      read   atomic.Pointer[readOnly] // read-only map (lock-free)      │
│      dirty  map[any]*entry           // read-write map (bajo mu)       │
│      misses int                      // contador de cache misses       │
│  }                                                                       │
│                                                                          │
│  type readOnly struct {                                                 │
│      m       map[any]*entry                                            │
│      amended bool   // true si dirty tiene keys que read no tiene     │
│  }                                                                       │
│                                                                          │
│  FLUJO DE Load():                                                       │
│  1. Buscar en read map (atomic load, SIN lock)                        │
│  2. Si lo encuentra: retornar (fast path, ~30ns)                      │
│  3. Si no y amended=true: Lock(), buscar en dirty, misses++           │
│  4. Si misses >= len(dirty): promover dirty→read, dirty=nil           │
│                                                                          │
│  FLUJO DE Store():                                                      │
│  1. Si key existe en read: atomic update del entry (sin lock)         │
│  2. Si no: Lock(), agregar a dirty                                     │
│                                                                          │
│  FLUJO DE Delete():                                                     │
│  1. Si key existe en read: marcar entry como "expunged"               │
│  2. Si no: Lock(), delete de dirty                                     │
│                                                                          │
│  PROMOCION (dirty → read):                                              │
│  Cuando misses alcanza len(dirty), dirty se promueve a read           │
│  atomicamente, y dirty se resetea a nil.                               │
│  Las nuevas writes van a un nuevo dirty (copia de read + nuevas).     │
│                                                                          │
│  POR QUE ES RAPIDO PARA READ-HEAVY:                                    │
│  Los reads del read map son completamente lock-free (atomic pointer). │
│  Solo caen al slow path cuando la key no esta en read (miss).         │
│  Si la mayoria de keys estan en read, la mayoria de operaciones       │
│  son lock-free.                                                         │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.5 Ejemplos practicos

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

// ============================================================
// Ejemplo 1: Cache write-once (patron ideal para sync.Map)
// ============================================================

func example1() {
    fmt.Println("=== Write-Once Cache ===")
    
    var cache sync.Map
    
    // Fase 1: poblar cache (writes)
    items := map[string]string{
        "config.host":     "localhost",
        "config.port":     "8080",
        "config.debug":    "false",
        "config.log_level": "info",
    }
    for k, v := range items {
        cache.Store(k, v)
    }
    
    // Fase 2: leer concurrentemente (reads — lock-free, rapido)
    var wg sync.WaitGroup
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            if val, ok := cache.Load("config.host"); ok {
                _ = val.(string) // type assertion necesaria (any)
            }
        }(i)
    }
    wg.Wait()
    fmt.Println("  100 reads concurrentes completados")
}

// ============================================================
// Ejemplo 2: LoadOrStore (compute-if-absent)
// ============================================================

func example2() {
    fmt.Println("\n=== LoadOrStore (Compute-If-Absent) ===")
    
    var cache sync.Map
    
    var wg sync.WaitGroup
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            key := fmt.Sprintf("key-%d", id%3) // solo 3 keys distintas
            
            // LoadOrStore: si existe, retorna el valor actual
            // Si no existe, almacena el nuevo valor
            actual, loaded := cache.LoadOrStore(key, fmt.Sprintf("from-goroutine-%d", id))
            
            if loaded {
                fmt.Printf("  G%d: key=%s ya existia con valor=%s\n", id, key, actual)
            } else {
                fmt.Printf("  G%d: key=%s almacenado=%s\n", id, key, actual)
            }
        }(i)
    }
    wg.Wait()
}

// ============================================================
// Ejemplo 3: Range (iterar)
// ============================================================

func example3() {
    fmt.Println("\n=== Range ===")
    
    var m sync.Map
    m.Store("service-a", "healthy")
    m.Store("service-b", "unhealthy")
    m.Store("service-c", "healthy")
    m.Store("service-d", "draining")
    
    // Range itera sobre todos los entries
    // f retorna false para parar
    fmt.Println("  Servicios unhealthy:")
    m.Range(func(key, value any) bool {
        if value.(string) != "healthy" {
            fmt.Printf("    %s: %s\n", key, value)
        }
        return true // continuar iterando
    })
}

// ============================================================
// Ejemplo 4: CompareAndSwap (Go 1.20+)
// ============================================================

func example4() {
    fmt.Println("\n=== CompareAndSwap ===")
    
    var m sync.Map
    m.Store("status", "starting")
    
    // Solo actualizar si el valor actual es el esperado
    swapped := m.CompareAndSwap("status", "starting", "running")
    fmt.Printf("  starting → running: swapped=%v\n", swapped) // true
    
    swapped = m.CompareAndSwap("status", "starting", "failed")
    fmt.Printf("  starting → failed: swapped=%v\n", swapped) // false (ya es "running")
    
    val, _ := m.Load("status")
    fmt.Printf("  status actual: %s\n", val)
}

func main() {
    example1()
    example2()
    example3()
    example4()
    _ = time.Now()
}
```

### 3.6 sync.Map tipado con generics (wrapper)

`sync.Map` usa `any` para keys y values, perdiendo type safety. Un wrapper con generics lo soluciona:

```go
package main

import (
    "fmt"
    "sync"
)

// TypedMap envuelve sync.Map con type safety
type TypedMap[K comparable, V any] struct {
    m sync.Map
}

func (tm *TypedMap[K, V]) Store(key K, value V) {
    tm.m.Store(key, value)
}

func (tm *TypedMap[K, V]) Load(key K) (V, bool) {
    val, ok := tm.m.Load(key)
    if !ok {
        var zero V
        return zero, false
    }
    return val.(V), true
}

func (tm *TypedMap[K, V]) Delete(key K) {
    tm.m.Delete(key)
}

func (tm *TypedMap[K, V]) LoadOrStore(key K, value V) (V, bool) {
    actual, loaded := tm.m.LoadOrStore(key, value)
    return actual.(V), loaded
}

func (tm *TypedMap[K, V]) Range(f func(key K, value V) bool) {
    tm.m.Range(func(k, v any) bool {
        return f(k.(K), v.(V))
    })
}

func main() {
    var cache TypedMap[string, int]
    
    cache.Store("requests", 42)
    cache.Store("errors", 3)
    
    if v, ok := cache.Load("requests"); ok {
        fmt.Printf("requests: %d\n", v) // type-safe, no need for assertion
    }
    
    cache.Range(func(key string, value int) bool {
        fmt.Printf("  %s = %d\n", key, value)
        return true
    })
}
```

### 3.7 Benchmark: sync.Map vs map+RWMutex

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

const iterations = 100000

func benchSyncMap(readers, writers int) time.Duration {
    var m sync.Map
    // Pre-populate
    for i := 0; i < 100; i++ {
        m.Store(i, i)
    }
    
    var wg sync.WaitGroup
    start := time.Now()
    
    for i := 0; i < readers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for j := 0; j < iterations; j++ {
                m.Load(j % 100)
            }
        }()
    }
    for i := 0; i < writers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for j := 0; j < iterations; j++ {
                m.Store(j%100, id)
            }
        }(i)
    }
    wg.Wait()
    return time.Since(start)
}

func benchRWMutexMap(readers, writers int) time.Duration {
    var mu sync.RWMutex
    m := make(map[int]int)
    for i := 0; i < 100; i++ {
        m[i] = i
    }
    
    var wg sync.WaitGroup
    start := time.Now()
    
    for i := 0; i < readers; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for j := 0; j < iterations; j++ {
                mu.RLock()
                _ = m[j%100]
                mu.RUnlock()
            }
        }()
    }
    for i := 0; i < writers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for j := 0; j < iterations; j++ {
                mu.Lock()
                m[j%100] = id
                mu.Unlock()
            }
        }(i)
    }
    wg.Wait()
    return time.Since(start)
}

func main() {
    fmt.Println("sync.Map vs map+RWMutex Benchmark")
    fmt.Println("==================================")
    
    scenarios := []struct{ readers, writers int }{
        {100, 0},  // read-only
        {100, 1},  // read-heavy
        {10, 10},  // balanced
        {1, 100},  // write-heavy
    }
    
    for _, s := range scenarios {
        sm := benchSyncMap(s.readers, s.writers)
        rw := benchRWMutexMap(s.readers, s.writers)
        
        winner := "sync.Map"
        ratio := float64(rw) / float64(sm)
        if ratio < 1 {
            winner = "RWMutex"
            ratio = 1 / ratio
        }
        
        fmt.Printf("  R=%3d W=%3d | sync.Map: %8v | RWMutex: %8v | %s %.1fx\n",
            s.readers, s.writers,
            sm.Round(time.Millisecond), rw.Round(time.Millisecond),
            winner, ratio)
    }
}
```

---

## 4. sync.Pool — Reciclaje de Objetos

### 4.1 Por que existe sync.Pool

Crear y destruir objetos genera presion sobre el garbage collector. En servidores de alto rendimiento, el patron de "crear buffer → usar → descartar" puede generar millones de allocations por segundo. `sync.Pool` permite **reutilizar objetos** en lugar de crearlos y descartarlos.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    sync.Pool — CONCEPTO                                 │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SIN Pool:                                                               │
│  Goroutine 1: alloc(buf) → use → GC                                   │
│  Goroutine 2: alloc(buf) → use → GC                                   │
│  Goroutine 3: alloc(buf) → use → GC                                   │
│  → 3 allocations, 3 GC cleanups                                        │
│                                                                          │
│  CON Pool:                                                               │
│  Goroutine 1: Get(pool) → use → Put(pool)                             │
│  Goroutine 2: Get(pool) → use → Put(pool)  // reutiliza buf de G1    │
│  Goroutine 3: Get(pool) → use → Put(pool)  // reutiliza buf de G2    │
│  → 1 allocation, 0 GC cleanups extra                                   │
│                                                                          │
│  sync.Pool NO es:                                                       │
│  ├─ Un pool de conexiones (usa un pool dedicado para eso)             │
│  ├─ Un cache (el GC puede vaciar el pool en cualquier momento)        │
│  ├─ Un pool de workers/goroutines (usa channels para eso)             │
│  └─ Un free-list persistente (los objetos pueden desaparecer)         │
│                                                                          │
│  sync.Pool ES:                                                          │
│  ├─ Un cache temporal de objetos reutilizables                        │
│  ├─ Thread-safe y lock-free (per-P pool con stealing)                 │
│  ├─ Vaciado automaticamente en cada GC cycle                          │
│  └─ Ideal para buffers, encoders, objetos temporales costosos         │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.2 API

```go
type Pool struct {
    New func() any  // funcion que crea un nuevo objeto cuando el pool esta vacio
    // campos no exportados
}

func (p *Pool) Get() any      // obtener un objeto (del pool o New())
func (p *Pool) Put(x any)     // devolver un objeto al pool
```

### 4.3 Uso basico — Buffer pool

```go
package main

import (
    "bytes"
    "fmt"
    "sync"
)

var bufPool = sync.Pool{
    New: func() any {
        // Se llama cuando el pool esta vacio
        fmt.Println("  [Pool] Creando nuevo buffer")
        return new(bytes.Buffer)
    },
}

func processRequest(id int) string {
    // Obtener buffer del pool (o crear uno nuevo)
    buf := bufPool.Get().(*bytes.Buffer)
    
    // IMPORTANTE: reset antes de usar (puede tener datos del uso anterior)
    buf.Reset()
    
    // Usar el buffer
    fmt.Fprintf(buf, "Request %d processed at some time", id)
    result := buf.String()
    
    // Devolver al pool para reutilizacion
    bufPool.Put(buf)
    
    return result
}

func main() {
    var wg sync.WaitGroup
    
    for i := 1; i <= 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            result := processRequest(id)
            fmt.Printf("  %s\n", result)
        }(i)
    }
    
    wg.Wait()
    
    // Output: "Creando nuevo buffer" aparece MUCHAS MENOS veces que 10
    // porque los buffers se reutilizan entre goroutines
}
```

### 4.4 Internals

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    sync.Pool — INTERNALS                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  sync.Pool usa un diseno per-P (por procesador) para minimizar        │
│  contention:                                                             │
│                                                                          │
│  type Pool struct {                                                     │
│      local     unsafe.Pointer // [GOMAXPROCS]poolLocal                 │
│      localSize uintptr                                                  │
│      victim    unsafe.Pointer // pool de la generacion anterior         │
│      New       func() any                                               │
│  }                                                                       │
│                                                                          │
│  type poolLocal struct {                                                │
│      private any       // solo accesible por el P actual (sin lock)    │
│      shared  poolChain // lista lock-free compartida entre Ps          │
│  }                                                                       │
│                                                                          │
│  FLUJO DE Get():                                                        │
│  1. Fijar goroutine al P actual (pin)                                  │
│  2. Intentar private del P actual (sin lock, O(1))                    │
│  3. Si vacio: intentar shared del P actual (lock-free pop)            │
│  4. Si vacio: robar de shared de otro P (work stealing)               │
│  5. Si vacio: intentar victim pool (generacion anterior)               │
│  6. Si vacio: llamar New() para crear uno nuevo                       │
│  7. Desfijar (unpin)                                                    │
│                                                                          │
│  FLUJO DE Put():                                                        │
│  1. Fijar goroutine al P actual                                        │
│  2. Si private esta vacio: almacenar en private (sin lock)            │
│  3. Si no: almacenar en shared (lock-free push)                       │
│  4. Desfijar                                                            │
│                                                                          │
│  GC Y VACIADO:                                                          │
│  ├─ En cada ciclo de GC, victim = local, local = vaciar              │
│  ├─ Los objetos en victim sobreviven UN ciclo de GC                  │
│  ├─ Despues de 2 GC cycles sin uso, el objeto se recolecta          │
│  └─ No puedes asumir que un Put(x) hara que el mismo x vuelva      │
│                                                                          │
│  POR QUE ES RAPIDO:                                                     │
│  ├─ private es acceso sin NINGUNA sincronizacion                      │
│  ├─ shared usa lock-free queue (atomic CAS)                           │
│  ├─ Per-P: cada procesador tiene su propio pool local                │
│  └─ Work stealing solo cuando el P local esta vacio                   │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.5 Patrones comunes

```go
package main

import (
    "bytes"
    "encoding/json"
    "fmt"
    "sync"
)

// ============================================================
// Patron 1: Encoder/Decoder pool
// ============================================================

var encoderPool = sync.Pool{
    New: func() any {
        return &bytes.Buffer{}
    },
}

func marshalJSON(v any) ([]byte, error) {
    buf := encoderPool.Get().(*bytes.Buffer)
    defer func() {
        buf.Reset()
        encoderPool.Put(buf)
    }()
    
    enc := json.NewEncoder(buf)
    if err := enc.Encode(v); err != nil {
        return nil, err
    }
    
    // Copiar el resultado antes de devolver el buffer
    result := make([]byte, buf.Len())
    copy(result, buf.Bytes())
    return result, nil
}

// ============================================================
// Patron 2: Slice pool con capacidad predefinida
// ============================================================

func newSlicePool(cap int) *sync.Pool {
    return &sync.Pool{
        New: func() any {
            s := make([]byte, 0, cap)
            return &s
        },
    }
}

var smallBufPool = newSlicePool(1024)    // 1KB
var largeBufPool = newSlicePool(64*1024) // 64KB

func getBuffer(size int) (*[]byte, *sync.Pool) {
    if size <= 1024 {
        buf := smallBufPool.Get().(*[]byte)
        *buf = (*buf)[:0] // reset length, keep capacity
        return buf, smallBufPool
    }
    buf := largeBufPool.Get().(*[]byte)
    *buf = (*buf)[:0]
    return buf, largeBufPool
}

// ============================================================
// Patron 3: Struct pool con reset
// ============================================================

type RequestContext struct {
    Method  string
    Path    string
    Headers map[string]string
    Body    []byte
}

var reqCtxPool = sync.Pool{
    New: func() any {
        return &RequestContext{
            Headers: make(map[string]string, 8),
            Body:    make([]byte, 0, 4096),
        }
    },
}

func getRequestContext() *RequestContext {
    ctx := reqCtxPool.Get().(*RequestContext)
    // Reset — CRITICO: siempre limpiar antes de usar
    ctx.Method = ""
    ctx.Path = ""
    for k := range ctx.Headers {
        delete(ctx.Headers, k)
    }
    ctx.Body = ctx.Body[:0]
    return ctx
}

func putRequestContext(ctx *RequestContext) {
    // Opcional: limitar tamano maximo del body para evitar retener
    // buffers enormes en el pool
    if cap(ctx.Body) > 1<<20 { // > 1MB
        ctx.Body = nil // dejar que el GC lo recoja
    }
    reqCtxPool.Put(ctx)
}

func main() {
    // Ejemplo 1: JSON encoder pool
    data := map[string]string{"hello": "world"}
    result, _ := marshalJSON(data)
    fmt.Printf("JSON: %s", result)
    
    // Ejemplo 3: Request context pool
    ctx := getRequestContext()
    ctx.Method = "GET"
    ctx.Path = "/api/users"
    ctx.Headers["Content-Type"] = "application/json"
    fmt.Printf("Request: %s %s\n", ctx.Method, ctx.Path)
    putRequestContext(ctx) // devolver al pool
    
    // El siguiente Get probablemente reutiliza el mismo objeto
    ctx2 := getRequestContext()
    fmt.Printf("Reused? method is empty: %v\n", ctx2.Method == "")
    putRequestContext(ctx2)
}
```

### 4.6 Errores comunes con sync.Pool

```go
// ERROR 1: No resetear el objeto antes de usar
func bad1() {
    var pool = sync.Pool{New: func() any { return new(bytes.Buffer) }}
    
    buf := pool.Get().(*bytes.Buffer)
    buf.WriteString("datos sensibles del request anterior")
    pool.Put(buf)
    
    buf2 := pool.Get().(*bytes.Buffer)
    // buf2 todavia contiene "datos sensibles del request anterior"
    // ← DATA LEAK entre requests
    _ = buf2
}

// FIX: SIEMPRE Reset() despues de Get()
func good1() {
    var pool = sync.Pool{New: func() any { return new(bytes.Buffer) }}
    
    buf := pool.Get().(*bytes.Buffer)
    buf.Reset() // SIEMPRE PRIMERO
    buf.WriteString("datos nuevos")
    pool.Put(buf)
}

// ERROR 2: Usar el objeto despues de Put
func bad2() {
    var pool = sync.Pool{New: func() any { return make([]byte, 1024) }}
    
    buf := pool.Get().([]byte)
    pool.Put(buf)
    
    // DESPUES de Put, otra goroutine puede obtener el mismo buf
    buf[0] = 42 // DATA RACE — alguien mas podria estar usandolo
}

// ERROR 3: Retener referencia a objeto del pool
func bad3() {
    var pool = sync.Pool{New: func() any { return new(bytes.Buffer) }}
    
    buf := pool.Get().(*bytes.Buffer)
    buf.WriteString("hello")
    result := buf.Bytes() // result es un slice del buffer interno
    pool.Put(buf)
    
    // result apunta a memoria del buffer que ahora esta en el pool
    // Si otra goroutine obtiene ese buffer y escribe, result se corrompe
    fmt.Println(string(result)) // ← puede mostrar basura
}

// FIX: copiar antes de devolver al pool
func good3() {
    var pool = sync.Pool{New: func() any { return new(bytes.Buffer) }}
    
    buf := pool.Get().(*bytes.Buffer)
    buf.Reset()
    buf.WriteString("hello")
    result := make([]byte, buf.Len())
    copy(result, buf.Bytes()) // copiar ANTES de Put
    pool.Put(buf)
    
    fmt.Println(string(result)) // safe
}

// ERROR 4: Pool con New nil
func bad4() {
    var pool sync.Pool // New es nil
    
    obj := pool.Get() // retorna nil porque New es nil y el pool esta vacio
    // obj.(type) → panic: interface conversion: interface is nil
    _ = obj
}

// ERROR 5: Almacenar objetos de tamano variable sin limitar
func bad5() {
    var pool = sync.Pool{
        New: func() any { return make([]byte, 0, 1024) },
    }
    
    // Un request atipico crea un buffer de 100MB
    buf := pool.Get().([]byte)
    buf = append(buf, make([]byte, 100*1024*1024)...) // 100MB
    pool.Put(buf) // ← ahora el pool retiene un buffer de 100MB
    // El proximo Get() obtiene 100MB cuando solo necesitaba 1KB
}

// FIX: verificar tamano antes de Put
func good5() {
    var pool = sync.Pool{
        New: func() any { return make([]byte, 0, 1024) },
    }
    
    buf := pool.Get().([]byte)
    buf = append(buf, make([]byte, 100*1024*1024)...)
    
    // Solo devolver si el tamano es razonable
    if cap(buf) <= 64*1024 { // <= 64KB
        pool.Put(buf[:0])
    }
    // Si es muy grande, dejarlo para el GC
}
```

---

## 5. Los Tres Tipos Juntos — Patrones Combinados

### 5.1 Lazy cache con Once + Map

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

type ServiceDiscovery struct {
    initOnce sync.Once
    cache    sync.Map // service name → endpoints
}

func (sd *ServiceDiscovery) init() {
    sd.initOnce.Do(func() {
        fmt.Println("  [Init] Loading service catalog...")
        // Simular carga de catalogo
        defaults := map[string]string{
            "auth":    "auth.internal:8080",
            "users":   "users.internal:8081",
            "orders":  "orders.internal:8082",
        }
        for k, v := range defaults {
            sd.cache.Store(k, v)
        }
        fmt.Println("  [Init] Service catalog loaded")
    })
}

func (sd *ServiceDiscovery) Resolve(service string) (string, bool) {
    sd.init() // lazy init — solo la primera vez
    val, ok := sd.cache.Load(service)
    if !ok {
        return "", false
    }
    return val.(string), true
}

func (sd *ServiceDiscovery) Register(service, endpoint string) {
    sd.init()
    sd.cache.Store(service, endpoint)
}

func main() {
    var sd ServiceDiscovery
    
    var wg sync.WaitGroup
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            if ep, ok := sd.Resolve("auth"); ok {
                fmt.Printf("  G%d: auth → %s\n", id, ep)
            }
        }(i)
    }
    wg.Wait()
    _ = time.Now()
}
```

### 5.2 High-performance logger con Pool + Once

```go
package main

import (
    "bytes"
    "fmt"
    "sync"
    "time"
)

type Logger struct {
    initOnce sync.Once
    bufPool  sync.Pool
    output   func(string)
}

func (l *Logger) init() {
    l.initOnce.Do(func() {
        l.bufPool = sync.Pool{
            New: func() any {
                return bytes.NewBuffer(make([]byte, 0, 256))
            },
        }
        if l.output == nil {
            l.output = func(s string) { fmt.Print(s) }
        }
    })
}

func (l *Logger) Log(level, msg string, fields map[string]any) {
    l.init()
    
    buf := l.bufPool.Get().(*bytes.Buffer)
    buf.Reset()
    
    // Formatear log entry usando el buffer del pool
    fmt.Fprintf(buf, "[%s] [%-5s] %s", time.Now().Format("15:04:05.000"), level, msg)
    for k, v := range fields {
        fmt.Fprintf(buf, " %s=%v", k, v)
    }
    buf.WriteByte('\n')
    
    // Copiar el resultado antes de devolver el buffer
    l.output(buf.String())
    
    l.bufPool.Put(buf)
}

func main() {
    logger := &Logger{}
    
    var wg sync.WaitGroup
    for i := 0; i < 5; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            logger.Log("INFO", "request processed", map[string]any{
                "worker": id,
                "status": 200,
            })
        }(i)
    }
    wg.Wait()
}
```

---

## 6. Comparacion con C y Rust

### 6.1 C

```c
// C: no tiene equivalentes directos

// === pthread_once (equivalente a sync.Once) ===
#include <pthread.h>

pthread_once_t init_once = PTHREAD_ONCE_INIT;
Database *db = NULL;

void init_db(void) {
    db = connect_to_database();
}

Database *get_db(void) {
    pthread_once(&init_once, init_db);
    return db;
}

// Limitaciones vs Go sync.Once:
// - No hay equivalente de sync.OnceValue/OnceFunc
// - El callback no puede capturar variables (no closures en C)
// - Error handling es manual (variable global para el error)

// === No hay sync.Map equivalente en C ===
// Usarias: concurrent hash table con RWLock por bucket (como Java ConcurrentHashMap)
// Librerias: ck (concurrency kit), libcuckoo

// === No hay sync.Pool equivalente en C ===
// Usarias: free-list manual con mutex
// O memory pools como apr_pool_t (Apache Portable Runtime)

typedef struct {
    void *objects[MAX_POOL];
    int count;
    pthread_mutex_t mu;
} ObjectPool;

void *pool_get(ObjectPool *p) {
    pthread_mutex_lock(&p->mu);
    if (p->count > 0) {
        void *obj = p->objects[--p->count];
        pthread_mutex_unlock(&p->mu);
        return obj;
    }
    pthread_mutex_unlock(&p->mu);
    return malloc(OBJECT_SIZE); // crear nuevo
}

void pool_put(ObjectPool *p, void *obj) {
    pthread_mutex_lock(&p->mu);
    if (p->count < MAX_POOL) {
        p->objects[p->count++] = obj;
    } else {
        free(obj); // pool lleno
    }
    pthread_mutex_unlock(&p->mu);
}

// DIFERENCIAS:
// - Manual memory management (no GC)
// - El pool NO se vacia automaticamente (leak si no se gestiona)
// - Sin per-CPU optimization (single mutex = contention)
// - Sin work stealing
```

### 6.2 Rust

```rust
// === std::sync::Once (equivalente a sync.Once) ===
use std::sync::Once;

static INIT: Once = Once::new();
static mut DB: Option<Database> = None;

fn get_db() -> &'static Database {
    unsafe {
        INIT.call_once(|| {
            DB = Some(Database::connect());
        });
        DB.as_ref().unwrap()
    }
}

// Rust prefiere OnceLock (Go 1.21+ equivale):
use std::sync::OnceLock;

static CONFIG: OnceLock<Config> = OnceLock::new();

fn get_config() -> &'static Config {
    CONFIG.get_or_init(|| {
        Config::load_from_file()
    })
    // Retorna &Config — no necesita unsafe
    // Equivale exactamente a sync.OnceValue
}

// === No hay sync.Map equivalente en std ===
// Crates: dashmap (mas popular), flurry, evmap
use dashmap::DashMap;

fn dashmap_example() {
    let map: DashMap<String, i32> = DashMap::new();
    
    map.insert("key".to_string(), 42);
    
    if let Some(val) = map.get("key") {
        println!("{}", *val);
    }
    // DashMap usa sharding (similar a striped locks)
    // Es type-safe (generics, no any)
    // Tiene .len(), .iter(), .retain() — mas completo que sync.Map
}

// === No hay sync.Pool equivalente en std ===
// Crates: object-pool, crossbeam's ShardedLock
// Rust no necesita Pool tanto como Go porque:
// 1. No tiene GC — no hay "GC pressure" que reducir
// 2. Stack allocation es la norma (no heap)
// 3. Ownership previene el reuse accidental
// 4. Los objetos se destruyen deterministicamente (Drop)
//
// Cuando Rust necesita pooling (ej: buffers de red),
// usa typed pools que no se vacian automaticamente:
use std::sync::Mutex;
use std::collections::VecDeque;

struct Pool<T> {
    items: Mutex<VecDeque<T>>,
    create: fn() -> T,
}

impl<T> Pool<T> {
    fn get(&self) -> T {
        self.items.lock().unwrap().pop_front()
            .unwrap_or_else(|| (self.create)())
    }
    
    fn put(&self, item: T) {
        self.items.lock().unwrap().push_back(item);
    }
}
```

### 6.3 Tabla comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    Once/Map/Pool: GO vs C vs RUST                       │
├──────────────────┬─────────────────┬───────────────┬─────────────────────┤
│ Tipo              │ C               │ Go            │ Rust               │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Once              │ pthread_once    │ sync.Once     │ std::sync::Once    │
│                   │ (no closures)   │ OnceFunc/     │ OnceLock           │
│                   │                 │ OnceValue 1.21│ (type-safe)        │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Concurrent Map    │ Manual (RW +    │ sync.Map      │ dashmap (crate)    │
│                   │ hash table)     │ (any typed)   │ (generic, sharded) │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Object Pool       │ Manual free-list│ sync.Pool     │ Manual o crate     │
│                   │ + mutex         │ (per-P, GC    │ (no GC → menos    │
│                   │                 │ aware)        │ necesario)          │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ GC interaction    │ N/A (no GC)     │ Pool vaciado  │ N/A (no GC)       │
│                   │                 │ por GC        │                     │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Type safety       │ void* (manual)  │ any (runtime) │ Generics (compile) │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Per-CPU optim.    │ Manual (TLS)    │ Si (per-P)    │ Manual (crossbeam) │
└──────────────────┴─────────────────┴───────────────┴─────────────────────┘
```

---

## 7. Programa Completo — Infrastructure Cache System

Un sistema de cache multi-capa que usa sync.Once para inicializacion, sync.Map para almacenamiento concurrente, y sync.Pool para reciclar objetos de respuesta:

```go
package main

import (
    "bytes"
    "encoding/json"
    "fmt"
    "math/rand"
    "sync"
    "sync/atomic"
    "time"
)

// ============================================================
// Tipos
// ============================================================

type CacheEntry struct {
    Key       string
    Value     any
    CreatedAt time.Time
    TTL       time.Duration
    Hits      atomic.Int64
}

func (e *CacheEntry) IsExpired() bool {
    return time.Since(e.CreatedAt) > e.TTL
}

// ResponseBuffer — objeto reciclable via Pool
type ResponseBuffer struct {
    buf     bytes.Buffer
    headers map[string]string
}

func (rb *ResponseBuffer) Reset() {
    rb.buf.Reset()
    for k := range rb.headers {
        delete(rb.headers, k)
    }
}

// ============================================================
// CacheSystem — usa los 3 tipos sync
// ============================================================

type CacheSystem struct {
    // sync.Once: inicializacion lazy
    initOnce    sync.Once
    cleanupOnce sync.Once
    
    // sync.Map: almacenamiento concurrente (read-heavy, ideal para cache)
    store sync.Map
    
    // sync.Pool: reciclar ResponseBuffers
    bufPool sync.Pool
    
    // Metricas (atomics para contadores simples)
    hits   atomic.Int64
    misses atomic.Int64
    evictions atomic.Int64
    
    // Config
    defaultTTL time.Duration
    maxEntries int
}

func NewCacheSystem(defaultTTL time.Duration, maxEntries int) *CacheSystem {
    return &CacheSystem{
        defaultTTL: defaultTTL,
        maxEntries: maxEntries,
    }
}

func (cs *CacheSystem) init() {
    cs.initOnce.Do(func() {
        // Inicializar pool de buffers
        cs.bufPool = sync.Pool{
            New: func() any {
                return &ResponseBuffer{
                    headers: make(map[string]string, 4),
                }
            },
        }
        
        // Iniciar cleanup loop
        go cs.cleanupLoop()
        
        fmt.Println("  [INIT] Cache system initialized")
    })
}

// Get — buscar en cache (read-heavy, lock-free via sync.Map)
func (cs *CacheSystem) Get(key string) (any, bool) {
    cs.init()
    
    val, ok := cs.store.Load(key)
    if !ok {
        cs.misses.Add(1)
        return nil, false
    }
    
    entry := val.(*CacheEntry)
    if entry.IsExpired() {
        cs.store.Delete(key)
        cs.evictions.Add(1)
        cs.misses.Add(1)
        return nil, false
    }
    
    entry.Hits.Add(1)
    cs.hits.Add(1)
    return entry.Value, true
}

// Set — escribir en cache
func (cs *CacheSystem) Set(key string, value any) {
    cs.init()
    cs.SetWithTTL(key, value, cs.defaultTTL)
}

func (cs *CacheSystem) SetWithTTL(key string, value any, ttl time.Duration) {
    cs.init()
    
    entry := &CacheEntry{
        Key:       key,
        Value:     value,
        CreatedAt: time.Now(),
        TTL:       ttl,
    }
    cs.store.Store(key, entry)
}

// GetOrCompute — cache-aside pattern
func (cs *CacheSystem) GetOrCompute(key string, compute func() (any, error)) (any, error) {
    cs.init()
    
    // Intentar leer del cache
    if val, ok := cs.Get(key); ok {
        return val, nil
    }
    
    // Cache miss — computar el valor
    val, err := compute()
    if err != nil {
        return nil, err
    }
    
    // Almacenar en cache (LoadOrStore para evitar race)
    entry := &CacheEntry{
        Key:       key,
        Value:     val,
        CreatedAt: time.Now(),
        TTL:       cs.defaultTTL,
    }
    actual, loaded := cs.store.LoadOrStore(key, entry)
    if loaded {
        // Otra goroutine ya lo computo — usar el valor existente
        return actual.(*CacheEntry).Value, nil
    }
    return val, nil
}

// GetResponseBuffer — obtener buffer del pool
func (cs *CacheSystem) GetResponseBuffer() *ResponseBuffer {
    cs.init()
    rb := cs.bufPool.Get().(*ResponseBuffer)
    rb.Reset() // SIEMPRE reset
    return rb
}

// PutResponseBuffer — devolver buffer al pool
func (cs *CacheSystem) PutResponseBuffer(rb *ResponseBuffer) {
    // Limitar tamano del buffer para no retener buffers enormes
    if rb.buf.Cap() > 1<<16 { // > 64KB
        return // dejar que el GC lo recoja
    }
    cs.bufPool.Put(rb)
}

// cleanupLoop — periodicamente limpiar entries expirados
func (cs *CacheSystem) cleanupLoop() {
    ticker := time.NewTicker(1 * time.Second)
    defer ticker.Stop()
    
    for range ticker.C {
        expired := 0
        cs.store.Range(func(key, value any) bool {
            entry := value.(*CacheEntry)
            if entry.IsExpired() {
                cs.store.Delete(key)
                cs.evictions.Add(1)
                expired++
            }
            return true
        })
        if expired > 0 {
            fmt.Printf("  [CLEANUP] Evicted %d expired entries\n", expired)
        }
    }
}

// Stats — retornar metricas
func (cs *CacheSystem) Stats() map[string]int64 {
    hits := cs.hits.Load()
    misses := cs.misses.Load()
    total := hits + misses
    var hitRate int64
    if total > 0 {
        hitRate = hits * 100 / total
    }
    
    return map[string]int64{
        "hits":      hits,
        "misses":    misses,
        "evictions": cs.evictions.Load(),
        "hit_rate":  hitRate,
    }
}

// ============================================================
// Simulacion
// ============================================================

type ServiceInfo struct {
    Name   string `json:"name"`
    Host   string `json:"host"`
    Port   int    `json:"port"`
    Health string `json:"health"`
}

func main() {
    cache := NewCacheSystem(3*time.Second, 1000)

    fmt.Println("╔══════════════════════════════════════════════════════════════╗")
    fmt.Println("║           INFRASTRUCTURE CACHE SYSTEM                       ║")
    fmt.Println("╠══════════════════════════════════════════════════════════════╣")
    fmt.Println("║  sync.Once: lazy init | sync.Map: store | sync.Pool: bufs  ║")
    fmt.Println("╚══════════════════════════════════════════════════════════════╝")
    fmt.Println()

    // Fase 1: Poblar cache
    fmt.Println("--- Phase 1: Populate cache ---")
    services := []ServiceInfo{
        {Name: "api-gateway", Host: "10.0.1.1", Port: 8080, Health: "healthy"},
        {Name: "auth-service", Host: "10.0.1.2", Port: 8081, Health: "healthy"},
        {Name: "user-service", Host: "10.0.1.3", Port: 8082, Health: "healthy"},
        {Name: "order-service", Host: "10.0.1.4", Port: 8083, Health: "healthy"},
        {Name: "payment-service", Host: "10.0.1.5", Port: 8084, Health: "degraded"},
    }
    
    for _, svc := range services {
        cache.Set(svc.Name, svc)
        fmt.Printf("  Cached: %s\n", svc.Name)
    }
    
    // Fase 2: Lecturas concurrentes + serializar respuestas con Pool
    fmt.Println("\n--- Phase 2: Concurrent reads (50 goroutines, 100 reads each) ---")
    
    var wg sync.WaitGroup
    for i := 0; i < 50; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                svcName := services[rand.Intn(len(services))].Name
                
                val, ok := cache.Get(svcName)
                if ok {
                    // Usar buffer del pool para serializar
                    buf := cache.GetResponseBuffer()
                    svc := val.(ServiceInfo)
                    json.NewEncoder(&buf.buf).Encode(svc)
                    buf.headers["Content-Type"] = "application/json"
                    _ = buf.buf.Len() // simular enviar response
                    cache.PutResponseBuffer(buf) // devolver al pool
                }
            }
        }(i)
    }
    wg.Wait()
    
    // Fase 3: GetOrCompute (cache-aside)
    fmt.Println("\n--- Phase 3: GetOrCompute (cache-aside) ---")
    
    for i := 0; i < 5; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            val, _ := cache.GetOrCompute("computed-metric", func() (any, error) {
                fmt.Printf("  [COMPUTE] Goroutine %d computing metric...\n", id)
                time.Sleep(100 * time.Millisecond)
                return 42.5, nil
            })
            fmt.Printf("  [RESULT] Goroutine %d got metric: %v\n", id, val)
        }(i)
    }
    wg.Wait()
    
    // Fase 4: Esperar eviction (TTL = 3s)
    fmt.Println("\n--- Phase 4: Waiting for TTL expiry (3s) ---")
    time.Sleep(4 * time.Second)
    
    // Verificar que los entries se evictaron
    _, found := cache.Get("api-gateway")
    fmt.Printf("  api-gateway still cached: %v\n", found) // false
    
    // Stats finales
    stats := cache.Stats()
    fmt.Println()
    fmt.Println("┌──────────────────────────────────────────┐")
    fmt.Println("│              CACHE STATS                  │")
    fmt.Println("├──────────────────────────────────────────┤")
    fmt.Printf("│  Hits:       %-8d                     │\n", stats["hits"])
    fmt.Printf("│  Misses:     %-8d                     │\n", stats["misses"])
    fmt.Printf("│  Evictions:  %-8d                     │\n", stats["evictions"])
    fmt.Printf("│  Hit Rate:   %d%%                         │\n", stats["hit_rate"])
    fmt.Println("├──────────────────────────────────────────┤")
    fmt.Println("│  sync.Once:  lazy init + cleanup start   │")
    fmt.Println("│  sync.Map:   lock-free reads (5K reads)  │")
    fmt.Println("│  sync.Pool:  buffer recycling (5K bufs)  │")
    fmt.Println("└──────────────────────────────────────────┘")
}
```

---

## 8. Ejercicios

### Ejercicio 1: Feature flag system

Implementa un sistema de feature flags usando los 3 tipos:

```go
type FeatureFlags struct {
    // sync.Once: cargar flags iniciales desde "archivo"
    // sync.Map: almacenar flags (read-heavy — se consultan en cada request)
    // sync.Pool: reciclar objetos de evaluacion
}

func (ff *FeatureFlags) IsEnabled(flag string, userID string) bool
func (ff *FeatureFlags) SetFlag(flag string, enabled bool, percentage int)
func (ff *FeatureFlags) Reload()  // forzar recarga (necesita segundo Once?)
```

- `IsEnabled` consulta el flag y aplica percentage rollout (user hash % 100 < percentage)
- Los flags se cargan una vez con `Once`, se almacenan en `Map`
- Los objetos de evaluacion (con contexto del user) se reciclan con `Pool`
- Demostrar con 100 goroutines evaluando flags concurrentemente

### Ejercicio 2: Connection pool manager

Implementa un pool manager que gestiona pools por host:

```go
type PoolManager struct {
    // sync.Map: pool por host (disjoint keys — ideal para sync.Map)
    // sync.Once per host: inicializar pool lazy
    // sync.Pool per host: reciclar conexiones
}

func (pm *PoolManager) GetConn(host string) (*Connection, error)
func (pm *PoolManager) PutConn(host string, conn *Connection)
func (pm *PoolManager) Stats() map[string]PoolStats
```

- Cada host tiene su propio `sync.Pool` (almacenado en `sync.Map`)
- La inicializacion del pool de cada host usa `sync.Once` (via `LoadOrStore`)
- Limitar conexiones por pool (descartar si el pool esta "lleno")
- Benchmark: comparar con un solo pool global con mutex

### Ejercicio 3: Metrics aggregator

Implementa un agregador de metricas:

```go
type MetricsAggregator struct {
    // sync.Once: init
    // sync.Map: metric name → *MetricBucket
    // sync.Pool: reciclar sample slices
}

func (ma *MetricsAggregator) Record(name string, value float64)
func (ma *MetricsAggregator) Snapshot() map[string]MetricSnapshot  // min, max, avg, p95, count
```

- Cada metrica tiene su propio bucket (almacenado en `sync.Map`)
- Los samples se almacenan en slices reciclados via `sync.Pool`
- `Snapshot` calcula estadisticas y resetea los buckets
- Demostrar con 20 goroutines enviando 1000 metricas cada una

### Ejercicio 4: Template engine con cache

Implementa un template engine que cachea templates compilados:

```go
type TemplateEngine struct {
    // sync.Once: init
    // sync.Map: template name → compiled template (write-once, read-many)
    // sync.Pool: reciclar buffers de renderizado
}

func (te *TemplateEngine) Render(name string, data map[string]any) (string, error)
```

- Los templates se compilan una vez y se cachean en `sync.Map`
- Los buffers de renderizado se reciclan con `sync.Pool`
- Simular 100 requests concurrentes renderizando 5 templates diferentes
- Medir la diferencia en allocations con y sin Pool

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ONCE, MAP, POOL — RESUMEN COMPLETO                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  sync.Once:                                                              │
│  ├─ Do(f): ejecuta f exactamente una vez, goroutines esperan          │
│  ├─ OnceFunc/OnceValue/OnceValues (Go 1.21+): helpers tipados         │
│  ├─ Zero value listo para usar                                          │
│  ├─ Internals: atomic check (fast) + mutex (slow path)                │
│  ├─ Patrones: singleton, lazy init, safe-close, per-instance init     │
│  ├─ Errores: Do dentro de Do (deadlock), copiar, no-retry en error    │
│  └─ ~2ns despues de primera ejecucion (atomic load only)              │
│                                                                          │
│  sync.Map:                                                               │
│  ├─ Store/Load/Delete/Range/LoadOrStore/CompareAndSwap               │
│  ├─ Optimizado para: write-once read-many + disjoint keys            │
│  ├─ NO usar para: shared writes frecuentes (map+RWMutex es mejor)    │
│  ├─ Internals: dual map (read lock-free + dirty con mutex)            │
│  ├─ No tiene Len(), no es type-safe (any), Range lockea              │
│  ├─ 2-5x mas rapido que RWMutex para read-only workloads             │
│  └─ Wrapper generico para type safety                                  │
│                                                                          │
│  sync.Pool:                                                              │
│  ├─ Get/Put: obtener y devolver objetos reutilizables                 │
│  ├─ New func: crear objeto cuando pool vacio                           │
│  ├─ El GC puede vaciar el pool en cualquier momento                   │
│  ├─ Internals: per-P pool + work stealing (lock-free)                 │
│  ├─ SIEMPRE Reset() despues de Get() (data leak prevention)          │
│  ├─ NUNCA usar despues de Put (data race)                              │
│  ├─ NUNCA retener referencia a internals (copiar antes de Put)        │
│  ├─ Limitar tamano max antes de Put (evitar retener buffers enormes)  │
│  └─ NO es un connection pool ni un cache                               │
│                                                                          │
│  vs C: pthread_once (no closures), manual hash tables, manual pools   │
│  vs Rust: OnceLock (type-safe), dashmap (crate), no Pool (no GC)      │
│  Go: integrados en stdlib, optimizados para el runtime y GC           │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S03 - Sincronizacion, T03 - Context — context.Background, WithCancel, WithTimeout, WithValue, propagacion de cancelacion
