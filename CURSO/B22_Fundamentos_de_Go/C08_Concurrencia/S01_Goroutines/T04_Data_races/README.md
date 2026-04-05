# Data Races — Race Detector (-race flag), Acceso Concurrente a Memoria Compartida

## 1. Introduccion

Un **data race** ocurre cuando dos o mas goroutines acceden a la misma variable simultaneamente y **al menos una de ellas escribe**. Es el bug de concurrencia mas comun, mas peligroso, y mas dificil de diagnosticar. Un data race puede causar desde resultados incorrectos silenciosos hasta crashes aleatorios, corrupcion de datos, y vulnerabilidades de seguridad — y lo peor: puede funcionar "correctamente" durante meses y fallar solo bajo carga en produccion.

Go tiene una herramienta excepcional para detectar data races: el **race detector**, integrado en el toolchain con el flag `-race`. Es una de las razones por las que Go es mas seguro para concurrencia que C, donde herramientas equivalentes (ThreadSanitizer, Helgrind) son externas y mas dificiles de usar.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    QUE ES UN DATA RACE                                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Condiciones para un data race (TODAS deben cumplirse):                 │
│                                                                          │
│  1. Dos o mas goroutines acceden a la MISMA ubicacion de memoria       │
│  2. Al menos una de las operaciones es una ESCRITURA                   │
│  3. Los accesos NO estan sincronizados (sin mutex, channel, atomic)    │
│                                                                          │
│  EJEMPLO MINIMO:                                                         │
│                                                                          │
│  var counter int          // variable compartida                        │
│                                                                          │
│  go func() {                                                             │
│      counter++            // goroutine A: LEE counter, SUMA 1, ESCRIBE │
│  }()                                                                     │
│                                                                          │
│  go func() {                                                             │
│      counter++            // goroutine B: LEE counter, SUMA 1, ESCRIBE │
│  }()                                                                     │
│                                                                          │
│  // ¿Resultado? counter podria ser 1 o 2 (o cualquier cosa)            │
│  // Depende del timing del scheduler — NO DETERMINISTICO               │
│                                                                          │
│  IMPORTANTE:                                                             │
│  ├─ Un data race NO es lo mismo que una race condition                  │
│  ├─ Data race: acceso no sincronizado a memoria (siempre un bug)       │
│  └─ Race condition: resultado depende del orden de ejecucion            │
│     (puede ser un bug de logica, pero no necesariamente de memoria)    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

Para SysAdmin/DevOps, los data races son criticos porque:
- Un counter de metricas con data race puede reportar numeros incorrectos — tomas decisiones de scaling basadas en datos corruptos
- Un mapa de configuracion compartido puede corromperse y causar un crash a las 3am
- Un flag de "shutdown" sin sincronizacion puede dejar goroutines huerfanas que consumen recursos
- La mayoria de bugs de produccion en servicios Go concurrentes son data races no detectados en testing

---

## 2. Anatomia de un Data Race

### 2.1 Por Que counter++ No Es Atomico

```go
var counter int

// counter++ parece una operacion atomica, pero NO lo es.
// El compilador lo traduce a TRES operaciones de CPU:
//
// 1. LOAD: leer el valor actual de counter desde memoria al registro
// 2. ADD:  sumar 1 al registro
// 3. STORE: escribir el nuevo valor del registro a memoria
//
// En pseudoassembly x86_64:
//   MOV RAX, [counter]   ; LOAD: RAX = counter
//   INC RAX              ; ADD:  RAX = RAX + 1
//   MOV [counter], RAX   ; STORE: counter = RAX
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│              INTERLEAVING QUE CAUSA DATA RACE EN counter++               │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  counter = 0 inicialmente. Dos goroutines hacen counter++.              │
│  Resultado esperado: counter = 2                                        │
│                                                                          │
│  EJECUCION CORRECTA (sin interleaving):                                 │
│  ─────────────────────────────────────                                   │
│  Goroutine A:                Goroutine B:                               │
│  1. LOAD  RAX = 0                                                       │
│  2. ADD   RAX = 1                                                       │
│  3. STORE counter = 1                                                   │
│                              4. LOAD  RBX = 1                           │
│                              5. ADD   RBX = 2                           │
│                              6. STORE counter = 2  ← CORRECTO          │
│                                                                          │
│  EJECUCION CON DATA RACE (interleaving):                                │
│  ──────────────────────────────────────                                   │
│  Goroutine A:                Goroutine B:                               │
│  1. LOAD  RAX = 0                                                       │
│                              2. LOAD  RBX = 0    ← AMBOS LEEN 0        │
│  3. ADD   RAX = 1                                                       │
│                              4. ADD   RBX = 1    ← AMBOS CALCULAN 1    │
│  5. STORE counter = 1                                                   │
│                              6. STORE counter = 1 ← SOBREESCRIBE!      │
│                                                                          │
│  Resultado: counter = 1 (INCORRECTO, deberia ser 2)                    │
│  Se "perdio" un incremento — el write de A fue sobreescrito por B      │
│                                                                          │
│  PEOR: con optimizaciones de CPU (store buffers, cache coherency),     │
│  el resultado puede ser 0, 1, o 2. Es COMPLETAMENTE NO DETERMINISTICO.│
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Ejemplo Ejecutable con Data Race

```go
package main

import (
    "fmt"
    "sync"
)

func main() {
    var counter int
    var wg sync.WaitGroup

    // 1000 goroutines incrementan el mismo counter
    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            counter++ // DATA RACE: read-modify-write sin sincronizacion
        }()
    }

    wg.Wait()
    fmt.Println("Counter:", counter) // ¿1000? Probablemente no.

    // Ejecutar varias veces produce resultados diferentes:
    // $ go run main.go → Counter: 947
    // $ go run main.go → Counter: 962
    // $ go run main.go → Counter: 1000 (a veces "funciona" por suerte)
    // $ go run main.go → Counter: 889
    //
    // El resultado "correcto" (1000) puede aparecer a veces —
    // esto es lo que hace los data races tan peligrosos:
    // pueden "funcionar" en tests y fallar en produccion.
}
```

### 2.3 Tipos de Data Races

```go
// ─── 1. Read-Write race (mas comun) ───
var config *Config
go func() { config = loadConfig() }()  // escritura
go func() { useConfig(config) }()      // lectura — puede leer nil o parcial

// ─── 2. Write-Write race ───
var result string
go func() { result = "success" }()  // escritura A
go func() { result = "failure" }()  // escritura B — ¿cual gana?

// ─── 3. Map concurrent access (MUY comun) ───
m := make(map[string]int)
go func() { m["a"] = 1 }()  // escritura
go func() { m["b"] = 2 }()  // escritura — CRASH: concurrent map writes
// Go detecta esto en runtime y hace PANIC (no es silencioso)
// "fatal error: concurrent map read and map write"
// o "fatal error: concurrent map writes"

// ─── 4. Slice concurrent access ───
var results []int
go func() { results = append(results, 1) }()  // append modifica slice header
go func() { results = append(results, 2) }()  // append modifica slice header
// Puede corromper el slice — length/capacity inconsistentes

// ─── 5. Interface assignment race ───
var err error
go func() { err = errors.New("fail") }()  // interface = (type, pointer)
go func() { fmt.Println(err) }()          // puede leer type de uno y pointer de otro
// Resultado: posible panic por interface inconsistente

// ─── 6. Struct field race ───
type Server struct {
    Status  string
    Healthy bool
}
var srv Server
go func() { srv.Status = "running"; srv.Healthy = true }()
go func() { fmt.Println(srv.Status, srv.Healthy) }()
// Puede imprimir Status="" Healthy=true (estado inconsistente)
```

### 2.4 Data Race vs Race Condition

```go
// ─── DATA RACE: acceso no sincronizado a memoria ───
// Siempre un BUG. Go puede detectarlo con -race.
var x int
go func() { x = 1 }()  // write
go func() { _ = x }()   // read sin sincronizar → DATA RACE

// ─── RACE CONDITION: logica depende del orden ───
// Puede ser un bug de diseño, pero NO necesariamente un data race.
var mu sync.Mutex
var balance int = 100

// Ambos con mutex (NO hay data race) pero hay race condition:
go func() {
    mu.Lock()
    if balance >= 50 {
        balance -= 50 // withdraw
    }
    mu.Unlock()
}()

go func() {
    mu.Lock()
    if balance >= 80 {
        balance -= 80 // withdraw
    }
    mu.Unlock()
}()
// Sin data race (mutex protege). Pero:
// Si ambos ven balance=100, ambos retiran → balance = -30
// Eso es una race condition de LOGICA, no de memoria.
// El race detector de Go NO detecta race conditions — solo data races.
```

---

## 3. El Race Detector de Go (-race)

### 3.1 Como Usar

```bash
# Compilar y ejecutar con race detector:
go run -race main.go

# Compilar con race detector:
go build -race -o myapp .
./myapp

# Tests con race detector:
go test -race ./...

# Benchmark con race detector:
go test -race -bench=. ./...

# Instalar binario con race detector (para staging):
go install -race ./cmd/myserver
```

### 3.2 Output del Race Detector

```go
// Programa con data race:
package main

import (
    "fmt"
    "sync"
)

func main() {
    var counter int
    var wg sync.WaitGroup

    for i := 0; i < 2; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            counter++
        }()
    }

    wg.Wait()
    fmt.Println(counter)
}
```

```bash
$ go run -race main.go

==================
WARNING: DATA RACE
Read at 0x00c00001c0b0 by goroutine 7:
  main.main.func1()
      /home/user/main.go:15 +0x6e

Previous write at 0x00c00001c0b0 by goroutine 6:
  main.main.func1()
      /home/user/main.go:15 +0x84

Goroutine 7 (running) created at:
  main.main()
      /home/user/main.go:13 +0xbc

Goroutine 6 (finished) created at:
  main.main()
      /home/user/main.go:13 +0xbc
==================
2
Found 1 data race(s)
exit status 66
```

### 3.3 Interpretando el Output

```
┌──────────────────────────────────────────────────────────────────────────┐
│              ANATOMIA DEL REPORTE DE DATA RACE                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  WARNING: DATA RACE                                                      │
│                                                                          │
│  ┌─ ACCESO 1 ────────────────────────────────────────────────────┐      │
│  │ Read at 0x00c00001c0b0 by goroutine 7:                        │      │
│  │   main.main.func1()                                           │      │
│  │       /home/user/main.go:15 +0x6e                             │      │
│  │                                                                │      │
│  │ → Goroutine 7 LEYO la direccion 0x00c00001c0b0                │      │
│  │ → En main.go linea 15 (counter++)                             │      │
│  └────────────────────────────────────────────────────────────────┘      │
│                                                                          │
│  ┌─ ACCESO 2 (CONFLICTIVO) ──────────────────────────────────────┐      │
│  │ Previous write at 0x00c00001c0b0 by goroutine 6:              │      │
│  │   main.main.func1()                                           │      │
│  │       /home/user/main.go:15 +0x84                             │      │
│  │                                                                │      │
│  │ → Goroutine 6 ESCRIBIO la MISMA direccion previamente         │      │
│  │ → En la misma linea 15 (counter++)                            │      │
│  └────────────────────────────────────────────────────────────────┘      │
│                                                                          │
│  ┌─ STACK TRACES DE CREACION ────────────────────────────────────┐      │
│  │ Goroutine 7 created at:  main.go:13                           │      │
│  │ Goroutine 6 created at:  main.go:13                           │      │
│  │                                                                │      │
│  │ → Ambas goroutines fueron creadas en el loop de main.go:13    │      │
│  └────────────────────────────────────────────────────────────────┘      │
│                                                                          │
│  INFORMACION CLAVE:                                                      │
│  - MISMA direccion de memoria (0x00c00001c0b0)                          │
│  - Al menos una es WRITE                                                │
│  - Sin sincronizacion entre ambos accesos                              │
│  - Stack trace muestra DONDE se creo cada goroutine                    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.4 Como Funciona Internamente el Race Detector

```
┌──────────────────────────────────────────────────────────────────────────┐
│              INTERNOS DEL RACE DETECTOR                                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  El race detector de Go usa ThreadSanitizer (TSan) v2 de Google,        │
│  la misma tecnologia que se usa en C/C++ con clang/gcc.                 │
│                                                                          │
│  ALGORITMO: "happens-before" based on vector clocks                     │
│                                                                          │
│  1. El compilador INSTRUMENTA cada acceso a memoria:                    │
│     x = 42        → runtime.racewrite(&x); x = 42                      │
│     _ = x         → runtime.raceread(&x); _ = x                        │
│                                                                          │
│  2. Cada goroutine tiene un VECTOR CLOCK (timestamp logico)             │
│     que se actualiza en cada operacion de sincronizacion:               │
│     - mu.Lock() / mu.Unlock()                                          │
│     - ch <- v / v = <-ch                                                │
│     - wg.Add() / wg.Wait()                                             │
│     - atomic operations                                                 │
│                                                                          │
│  3. Para cada acceso a memoria, el detector verifica:                   │
│     ¿El ultimo acceso a esta direccion desde OTRA goroutine             │
│      tiene un "happens-before" relationship con este acceso?            │
│     SI → no hay race                                                    │
│     NO → DATA RACE detectado, reportar                                  │
│                                                                          │
│  OVERHEAD:                                                               │
│  ├─ CPU:    5-10x mas lento                                             │
│  ├─ Memory: 5-10x mas memoria (shadow memory para cada 8 bytes)        │
│  └─ Binary: ~2x mas grande                                             │
│                                                                          │
│  LIMITACIONES:                                                           │
│  ├─ Solo detecta races que OCURREN durante la ejecucion                │
│  │   (si el code path con race no se ejecuta, no lo detecta)           │
│  ├─ NO detecta race conditions (logica, solo memoria)                  │
│  ├─ NO detecta deadlocks                                               │
│  └─ El overhead hace que no sea practico en produccion continua         │
│                                                                          │
│  RECOMENDACION:                                                          │
│  ├─ SIEMPRE ejecutar tests con -race: go test -race ./...              │
│  ├─ CI/CD: incluir -race en el pipeline de tests                       │
│  ├─ Staging: ejecutar con -race bajo carga de stress                    │
│  └─ Produccion: no -race continuo, pero hacer canary runs              │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.5 GORACE — Variables de Entorno

```bash
# Configurar el comportamiento del race detector:

# Generar log en archivo (en lugar de stderr):
GORACE="log_path=/tmp/race.log" go run -race main.go

# Limitar el numero de reportes (default: unlimited)
GORACE="halt_on_error=1" go run -race main.go
# halt_on_error=1: el programa TERMINA en el primer race detectado
# Util para CI/CD — fallo rapido

# Historial de goroutines en el reporte:
GORACE="history_size=5" go run -race main.go
# Mas historial = mas memoria, mas detalle en el reporte

# Combinar opciones:
GORACE="halt_on_error=1 history_size=5 log_path=/tmp/race.log" go test -race ./...
```

---

## 4. Catalogo Completo de Data Races

### 4.1 Race en Variables Simples

```go
// ⚠️ DATA RACE: incremento no atomico
var counter int
var wg sync.WaitGroup
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func() {
        defer wg.Done()
        counter++ // RACE: read + write sin sincronizar
    }()
}
wg.Wait()

// ✅ FIX 1: sync.Mutex
var mu sync.Mutex
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func() {
        defer wg.Done()
        mu.Lock()
        counter++
        mu.Unlock()
    }()
}

// ✅ FIX 2: sync/atomic (mejor para contadores simples)
var atomicCounter int64
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func() {
        defer wg.Done()
        atomic.AddInt64(&atomicCounter, 1)
    }()
}

// ✅ FIX 3: channel (fan-in pattern)
ch := make(chan int, 100)
for i := 0; i < 100; i++ {
    go func() { ch <- 1 }()
}
total := 0
for i := 0; i < 100; i++ {
    total += <-ch
}
```

### 4.2 Race en Maps (Crash Garantizado)

```go
// ⚠️ FATAL ERROR: concurrent map writes
// Go detecta esto en runtime y CRASHEA el programa
// (no es silencioso como otros data races)
m := make(map[string]int)
var wg sync.WaitGroup
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func(n int) {
        defer wg.Done()
        key := fmt.Sprintf("key-%d", n%10)
        m[key] = n // FATAL: concurrent map writes
    }(i)
}
wg.Wait()
// fatal error: concurrent map writes
// goroutine X [running]:
// runtime.throw({0x..., 0x...})

// ✅ FIX 1: sync.Mutex
var mu sync.Mutex
m := make(map[string]int)
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func(n int) {
        defer wg.Done()
        key := fmt.Sprintf("key-%d", n%10)
        mu.Lock()
        m[key] = n
        mu.Unlock()
    }(i)
}

// ✅ FIX 2: sync.Map (optimizado para ciertos patrones)
var sm sync.Map
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func(n int) {
        defer wg.Done()
        key := fmt.Sprintf("key-%d", n%10)
        sm.Store(key, n)
    }(i)
}

// ✅ FIX 3: sync.RWMutex (multiples lectores, un escritor)
var rwmu sync.RWMutex
m := make(map[string]int)

// Lectores concurrentes:
rwmu.RLock()
val := m["key"] // multiples goroutines pueden leer simultaneamente
rwmu.RUnlock()

// Escritor exclusivo:
rwmu.Lock()
m["key"] = val + 1 // solo una goroutine puede escribir
rwmu.Unlock()
```

### 4.3 Race en Slices

```go
// ⚠️ DATA RACE: append no es thread-safe
var results []int
var wg sync.WaitGroup
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func(n int) {
        defer wg.Done()
        results = append(results, n) // RACE: modifica header (len, cap, ptr)
    }(i)
}
wg.Wait()
// results puede tener menos de 100 elementos,
// o contener duplicados, o causar un panic por corrupcion

// ✅ FIX 1: Mutex
var mu sync.Mutex
for i := 0; i < 100; i++ {
    wg.Add(1)
    go func(n int) {
        defer wg.Done()
        mu.Lock()
        results = append(results, n)
        mu.Unlock()
    }(i)
}

// ✅ FIX 2: Slice por indice (sin mutex — SAFE)
results := make([]int, 100)
for i := 0; i < 100; i++ {
    i := i
    wg.Add(1)
    go func() {
        defer wg.Done()
        results[i] = compute(i) // cada goroutine escribe a su propio indice
    }()
}
// ¿Por que es safe? Porque results[0], results[1], ... son
// direcciones de memoria DIFERENTES. No hay shared access.

// ✅ FIX 3: Channel
ch := make(chan int, 100)
for i := 0; i < 100; i++ {
    go func(n int) { ch <- compute(n) }(i)
}
var results []int
for i := 0; i < 100; i++ {
    results = append(results, <-ch) // solo main goroutine hace append
}
```

### 4.4 Race en Structs

```go
// ⚠️ DATA RACE: campos de struct compartido
type Config struct {
    Host     string
    Port     int
    MaxConns int
}

var cfg Config

// Goroutine 1: escribe campos
go func() {
    cfg = Config{Host: "prod.db", Port: 5432, MaxConns: 100}
}()

// Goroutine 2: lee campos
go func() {
    fmt.Println(cfg.Host, cfg.Port) // puede leer estado parcial:
    // Host = "prod.db" pero Port = 0 (no se escribio aun)
    // O Host = "" y Port = 5432
}()

// ✅ FIX: atomic.Value para configuracion inmutable
var atomicCfg atomic.Value

// Escritor:
go func() {
    atomicCfg.Store(Config{Host: "prod.db", Port: 5432, MaxConns: 100})
}()

// Lector:
go func() {
    cfg := atomicCfg.Load().(Config) // lectura atomica del struct completo
    fmt.Println(cfg.Host, cfg.Port)   // siempre consistente
}()
```

### 4.5 Race en Interfaces

```go
// ⚠️ DATA RACE MUY SUTIL: interface = (type, pointer) — dos words
// Asignar a una interface NO es atomico en Go.

var err error // interface = (type_ptr, data_ptr) — 2 palabras

go func() {
    err = errors.New("connection refused")
    // Esto escribe DOS palabras: el type pointer y el data pointer
}()

go func() {
    if err != nil {
        fmt.Println(err.Error())
        // Puede leer: type de un error, data de otro → CRASH
        // Ejemplo: type apunta a *os.PathError, data apunta a *errorString
        // → err.Error() llama al metodo equivocado → panic o corrupcion
    }
}()

// ✅ FIX: proteger con mutex o usar atomic.Value
var mu sync.Mutex

go func() {
    mu.Lock()
    err = errors.New("connection refused")
    mu.Unlock()
}()

go func() {
    mu.Lock()
    localErr := err
    mu.Unlock()
    if localErr != nil {
        fmt.Println(localErr.Error()) // safe: copia local
    }
}()
```

### 4.6 Race en Closures de Loops

```go
// ⚠️ DATA RACE + BUG DE LOGICA: captura de variable del loop
for i := 0; i < 5; i++ {
    go func() {
        fmt.Println(i) // RACE: i es compartida entre main y goroutine
        // Ademas, probablemente imprime "5" cinco veces (no 0,1,2,3,4)
    }()
}

// ✅ FIX 1: captura explicita (Go < 1.22)
for i := 0; i < 5; i++ {
    i := i // nueva variable en cada iteracion
    go func() {
        fmt.Println(i) // safe: cada goroutine tiene su propia copia
    }()
}

// ✅ FIX 2: argumento del closure
for i := 0; i < 5; i++ {
    go func(n int) {
        fmt.Println(n) // safe: n es un parametro (copia)
    }(i)
}

// ✅ FIX 3: Go 1.22+ — el loop crea nueva variable por iteracion
// for i := 0; i < 5; i++ {
//     go func() { fmt.Println(i) }() // safe en Go 1.22+
// }
```

### 4.7 Race en Flags de Control

```go
// ⚠️ DATA RACE: flag de shutdown
var shutdown bool

go func() {
    // Worker loop
    for !shutdown { // RACE: lee shutdown sin sincronizar
        doWork()
    }
}()

time.Sleep(5 * time.Second)
shutdown = true // RACE: escribe shutdown sin sincronizar

// ✅ FIX 1: atomic.Bool (Go 1.19+)
var shutdown atomic.Bool

go func() {
    for !shutdown.Load() {
        doWork()
    }
}()

time.Sleep(5 * time.Second)
shutdown.Store(true)

// ✅ FIX 2: context.Context (idiomatico en Go)
ctx, cancel := context.WithCancel(context.Background())

go func() {
    for {
        select {
        case <-ctx.Done():
            return
        default:
            doWork()
        }
    }
}()

time.Sleep(5 * time.Second)
cancel() // safe: channel-based signaling
```

---

## 5. Soluciones: Mecanismos de Sincronizacion

### 5.1 Mapa de Decision

```
┌──────────────────────────────────────────────────────────────────────────┐
│              ¿QUE SINCRONIZACION USAR?                                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ¿Que tipo de acceso?                                                   │
│  │                                                                       │
│  ├─ Contador simple (int, bool) → sync/atomic                          │
│  │   atomic.AddInt64, atomic.Bool, atomic.Value                        │
│  │                                                                       │
│  ├─ Proteger seccion critica → sync.Mutex                              │
│  │   mu.Lock(); critical_section(); mu.Unlock()                        │
│  │                                                                       │
│  ├─ Muchos lectores, pocos escritores → sync.RWMutex                   │
│  │   rwmu.RLock() para lectores, rwmu.Lock() para escritores           │
│  │                                                                       │
│  ├─ Map compartido → sync.Map o sync.RWMutex + map                    │
│  │   sync.Map: mejor si keys no cambian mucho (read-heavy)             │
│  │   RWMutex + map: mejor si hay muchas escrituras                     │
│  │                                                                       │
│  ├─ Config/estado inmutable → atomic.Value                              │
│  │   atomicCfg.Store(newConfig); cfg := atomicCfg.Load()               │
│  │                                                                       │
│  ├─ Comunicar datos entre goroutines → channels                        │
│  │   ch <- data; data := <-ch                                          │
│  │                                                                       │
│  ├─ Inicializacion una sola vez → sync.Once                            │
│  │   once.Do(func() { initializeExpensiveResource() })                 │
│  │                                                                       │
│  └─ Senalizacion (shutdown, ready) → context.Context o channel         │
│      ctx.Done(), close(done)                                            │
│                                                                          │
│  REGLA DE ORO:                                                           │
│  "Don't communicate by sharing memory;                                  │
│   share memory by communicating." — Go Proverb                          │
│                                                                          │
│  Prefiere channels cuando puedas. Usa mutex/atomic cuando channels     │
│  sean innecesariamente complicados (contadores, caches).               │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 sync/atomic — Operaciones Atomicas

```go
import "sync/atomic"

// ─── Contadores ───
var counter int64
atomic.AddInt64(&counter, 1)           // increment
atomic.AddInt64(&counter, -1)          // decrement
val := atomic.LoadInt64(&counter)      // read
atomic.StoreInt64(&counter, 0)         // reset

// ─── Bool atomico (Go 1.19+) ───
var ready atomic.Bool
ready.Store(true)
if ready.Load() { /* ... */ }

// ─── Int atomico (Go 1.19+) ───
var count atomic.Int64
count.Add(1)
count.Add(-1)
val := count.Load()
count.Store(0)

// ─── Pointer atomico (Go 1.19+) ───
var ptr atomic.Pointer[Config]
ptr.Store(&Config{Host: "db.prod"})
cfg := ptr.Load() // *Config, safe

// ─── Value atomico (pre-1.19, para cualquier tipo) ───
var cfgStore atomic.Value
cfgStore.Store(Config{Host: "db.prod"})
cfg := cfgStore.Load().(Config)

// ─── Compare-And-Swap (CAS) ───
var state int64
// Solo cambia si el valor actual es el esperado:
swapped := atomic.CompareAndSwapInt64(&state, 0, 1)
// swapped=true: state era 0, ahora es 1
// swapped=false: state NO era 0, no se modifico

// Patron CAS para lock-free updates:
for {
    old := atomic.LoadInt64(&state)
    new := old + 1
    if atomic.CompareAndSwapInt64(&state, old, new) {
        break // exito
    }
    // otro goroutine modifico state entre Load y CAS — reintentar
}
```

### 5.3 sync.Mutex

```go
// Mutex basico:
var mu sync.Mutex
var balance int

func withdraw(amount int) bool {
    mu.Lock()
    defer mu.Unlock() // SIEMPRE defer para garantizar unlock

    if balance < amount {
        return false
    }
    balance -= amount
    return true
}

// ANTIPATRONES con Mutex:

// ⚠️ Olvidar Unlock (deadlock):
func bad() {
    mu.Lock()
    if condition {
        return // ← mu nunca se desbloquea!
    }
    mu.Unlock()
}

// ✅ SIEMPRE defer:
func good() {
    mu.Lock()
    defer mu.Unlock()
    if condition {
        return // defer Unlock se ejecuta
    }
}

// ⚠️ Lock recursivo (deadlock inmediato):
func recursive() {
    mu.Lock()
    defer mu.Unlock()
    recursive() // ← intenta Lock de nuevo → DEADLOCK
    // Go mutex NO es reentrante (a diferencia de Java/Python)
}

// ⚠️ Copiar Mutex (race):
type Cache struct {
    mu   sync.Mutex // ← se copia si copias Cache
    data map[string]string
}
c1 := Cache{data: make(map[string]string)}
c2 := c1 // ← COPIA el Mutex — ambos con estado independiente
// go vet detecta esto: "copies lock value"
```

### 5.4 sync.RWMutex

```go
// RWMutex: multiples lectores O un escritor (no ambos)
type Cache struct {
    mu   sync.RWMutex
    data map[string]string
}

func (c *Cache) Get(key string) (string, bool) {
    c.mu.RLock()         // multiple goroutines pueden leer simultaneamente
    defer c.mu.RUnlock()
    val, ok := c.data[key]
    return val, ok
}

func (c *Cache) Set(key, value string) {
    c.mu.Lock()          // solo una goroutine puede escribir
    defer c.mu.Unlock()  // y bloquea a todos los lectores
    c.data[key] = value
}

// ¿Cuando usar RWMutex vs Mutex?
// RWMutex: ratio lectura:escritura > 10:1
// Mutex: escrituras frecuentes o seccion critica muy corta
// (RWMutex tiene MAS overhead que Mutex en la parte de locking)
```

### 5.5 sync.Once

```go
// Once garantiza que una funcion se ejecuta EXACTAMENTE una vez,
// incluso si multiples goroutines la llaman simultaneamente.

var (
    instance *Database
    once     sync.Once
)

func GetDB() *Database {
    once.Do(func() {
        // Se ejecuta UNA SOLA VEZ, sin importar cuantas
        // goroutines llamen a GetDB simultaneamente.
        // Las demas goroutines esperan hasta que esta termine.
        instance = connectToDatabase()
    })
    return instance
}

// Patron: lazy initialization thread-safe
type Service struct {
    clientOnce sync.Once
    client     *http.Client
}

func (s *Service) getClient() *http.Client {
    s.clientOnce.Do(func() {
        s.client = &http.Client{Timeout: 10 * time.Second}
    })
    return s.client
}
```

### 5.6 sync.Map

```go
// sync.Map esta optimizado para dos patrones especificos:
// 1. Keys que se escriben una vez y se leen muchas veces (cache)
// 2. Goroutines que acceden a subsets disjuntos de keys

var cache sync.Map

// Store (escribir):
cache.Store("config:db_host", "db.prod.internal")

// Load (leer):
val, ok := cache.Load("config:db_host")
if ok {
    host := val.(string)
    fmt.Println("DB Host:", host)
}

// LoadOrStore (leer o insertar si no existe):
actual, loaded := cache.LoadOrStore("config:port", 5432)
// loaded=true: ya existia, actual es el valor existente
// loaded=false: no existia, se inserto 5432

// Delete:
cache.Delete("config:old_key")

// Range (iterar — NO es consistente si hay escrituras concurrentes):
cache.Range(func(key, value any) bool {
    fmt.Printf("%v = %v\n", key, value)
    return true // continuar iterando
})

// ¿Cuando NO usar sync.Map?
// - Cuando necesitas operaciones compuestas (check-then-act)
// - Cuando el set de keys cambia frecuentemente
// - Cuando necesitas iterar consistentemente
// En esos casos, RWMutex + map regular es mejor.
```

---

## 6. Patterns: Diseñar Sin Data Races

### 6.1 Confinement: No Compartir

```go
// El patron mas seguro: cada goroutine tiene su PROPIA copia
func processItems(items []string) []Result {
    results := make([]Result, len(items))

    var wg sync.WaitGroup
    for i, item := range items {
        i, item := i, item // variables LOCALES — no compartidas
        wg.Add(1)
        go func() {
            defer wg.Done()
            // item es local, results[i] es acceso exclusivo a un indice
            results[i] = process(item) // NO HAY datos compartidos
        }()
    }

    wg.Wait()
    return results
}
// Zero sincronizacion necesaria — el mejor patron para concurrencia
```

### 6.2 Producer-Consumer via Channel

```go
// En lugar de compartir memoria, pasar datos entre goroutines:
func processWithChannel(items []string) []Result {
    jobs := make(chan string, len(items))
    results := make(chan Result, len(items))

    // Producer: enviar trabajo
    go func() {
        for _, item := range items {
            jobs <- item
        }
        close(jobs)
    }()

    // Workers: procesar
    var wg sync.WaitGroup
    for w := 0; w < 10; w++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            for item := range jobs {
                results <- process(item) // resultado via channel, no shared memory
            }
        }()
    }

    // Cerrar results cuando workers terminen
    go func() {
        wg.Wait()
        close(results)
    }()

    // Consumer: recolectar resultados
    var output []Result
    for r := range results {
        output = append(output, r)
    }
    return output
}
```

### 6.3 Immutable Data + Atomic Swap

```go
// En lugar de modificar datos compartidos, crear nuevas versiones:
type Config struct {
    Hosts    []string
    MaxConns int
    Timeout  time.Duration
}

type ConfigManager struct {
    current atomic.Pointer[Config]
}

func NewConfigManager(initial *Config) *ConfigManager {
    cm := &ConfigManager{}
    cm.current.Store(initial)
    return cm
}

// Leer: sin lock, sin copia
func (cm *ConfigManager) Get() *Config {
    return cm.current.Load() // atomico, O(1)
}

// Actualizar: crear NUEVA config, swap atomico
func (cm *ConfigManager) Update(fn func(old Config) Config) {
    for {
        old := cm.current.Load()
        newCfg := fn(*old) // crear nueva basada en la vieja
        if cm.current.CompareAndSwap(old, &newCfg) {
            return // swap exitoso
        }
        // otro goroutine actualizo primero — reintentar con el nuevo valor
    }
}

// Uso:
cm := NewConfigManager(&Config{
    Hosts:    []string{"db1.prod", "db2.prod"},
    MaxConns: 100,
    Timeout:  5 * time.Second,
})

// Lectores: sin lock
go func() {
    cfg := cm.Get()
    fmt.Println(cfg.Hosts) // safe: cfg es inmutable
}()

// Escritor: crea nueva version
go func() {
    cm.Update(func(old Config) Config {
        old.MaxConns = 200 // no modifica la vieja — crea copia
        return old
    })
}()
```

---

## 7. Comparacion con C y Rust

### 7.1 C — Data Races Son Undefined Behavior

```c
#include <pthread.h>
#include <stdio.h>

int counter = 0; // variable global compartida

void *increment(void *arg) {
    for (int i = 0; i < 1000000; i++) {
        counter++; // DATA RACE — en C es UNDEFINED BEHAVIOR
        // El compilador puede:
        // - Reordenar la operacion
        // - Optimizar eliminandola
        // - Generar codigo que corrompe memoria
        // - Literalmente CUALQUIER COSA (por el estandar C)
    }
    return NULL;
}

int main() {
    pthread_t t1, t2;
    pthread_create(&t1, NULL, increment, NULL);
    pthread_create(&t2, NULL, increment, NULL);
    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    printf("Counter: %d\n", counter); // undefined behavior
    return 0;
}

// DETECCION en C:
// ThreadSanitizer (TSan): clang -fsanitize=thread main.c
// Helgrind (Valgrind):    valgrind --tool=helgrind ./a.out
// Ambos son EXTERNOS al compilador (Go lo tiene built-in)

// FIX en C:
// 1. pthread_mutex_lock/unlock (equivalente a sync.Mutex)
// 2. __atomic_add_fetch (equivalente a atomic.AddInt64)
// 3. _Atomic type qualifier (C11)

/*
 * C vs Go data races:
 * - C: undefined behavior (literalmente puede hacer cualquier cosa)
 * - Go: "defined but undesirable" behavior
 *   Go garantiza que un data race no causa memory corruption
 *   fuera de la variable afectada (no hay buffer overflows,
 *   no hay arbitrary code execution). Pero el VALOR es indefinido.
 * - C: deteccion requiere herramientas externas (TSan, Helgrind)
 * - Go: deteccion built-in (-race flag)
 */
```

### 7.2 Rust — Data Races Son Imposibles (en Safe Rust)

```rust
use std::thread;
use std::sync::{Arc, Mutex, RwLock};
use std::sync::atomic::{AtomicI64, Ordering};

// ─── Rust: el compilador PROHIBE data races ───

// ⛔ COMPILE ERROR: no puedes compartir datos mutables entre threads
fn compile_error() {
    let mut counter = 0;
    thread::spawn(|| {
        counter += 1;
    // ERROR: closure may outlive the current function,
    //        but it borrows `counter` which is owned by the current function
    });
}

// ⛔ COMPILE ERROR: no puedes mover datos a dos threads
fn also_error() {
    let mut counter = 0;
    let handle1 = thread::spawn(move || { counter += 1; });
    let handle2 = thread::spawn(move || { counter += 1; });
    // ERROR: use of moved value: `counter`
    // counter ya se movio al primer thread
}

// ✅ FIX 1: Arc<Mutex<T>> (equivalente a Go mutex)
fn with_mutex() {
    let counter = Arc::new(Mutex::new(0));
    let mut handles = vec![];

    for _ in 0..100 {
        let counter = Arc::clone(&counter);
        handles.push(thread::spawn(move || {
            let mut num = counter.lock().unwrap();
            *num += 1;
            // MutexGuard se dropea aqui (RAII) → unlock automatico
        }));
    }

    for h in handles { h.join().unwrap(); }
    println!("Counter: {}", *counter.lock().unwrap());
}

// ✅ FIX 2: AtomicI64 (equivalente a Go atomic)
fn with_atomic() {
    let counter = Arc::new(AtomicI64::new(0));
    let mut handles = vec![];

    for _ in 0..100 {
        let counter = Arc::clone(&counter);
        handles.push(thread::spawn(move || {
            counter.fetch_add(1, Ordering::Relaxed);
        }));
    }

    for h in handles { h.join().unwrap(); }
    println!("Counter: {}", counter.load(Ordering::Relaxed));
}

// ✅ FIX 3: RwLock (equivalente a Go RWMutex)
fn with_rwlock() {
    let data = Arc::new(RwLock::new(vec![1, 2, 3]));

    // Multiples lectores:
    let d = data.read().unwrap();
    println!("{:?}", *d);

    // Un escritor:
    let mut d = data.write().unwrap();
    d.push(4);
}

/*
 * COMPARACION DE SEGURIDAD:
 *
 * ┌───────────────┬──────────────────┬──────────────────┬──────────────┐
 * │               │ C                │ Go               │ Rust         │
 * ├───────────────┼──────────────────┼──────────────────┼──────────────┤
 * │ Data race     │ Undefined        │ Defined but      │ IMPOSIBLE    │
 * │ behavior      │ Behavior (UB)    │ undesirable      │ (en safe)    │
 * │               │                  │                  │              │
 * │ Deteccion     │ TSan/Helgrind    │ -race flag       │ Compilador   │
 * │               │ (externo)        │ (built-in)       │ (built-in)   │
 * │               │                  │                  │              │
 * │ Cuando        │ Runtime          │ Runtime          │ Compile-time │
 * │ se detecta    │ (si se ejecuta)  │ (si se ejecuta)  │ (siempre)    │
 * │               │                  │                  │              │
 * │ Costo de      │ Ninguno (UB)     │ 5-10x slowdown   │ 0 (zero-cost)│
 * │ deteccion     │ (con TSan: 5x)   │ (con -race)      │              │
 * │               │                  │                  │              │
 * │ Falso         │ Posible (TSan)   │ NO (Go race      │ NO (tipos)   │
 * │ negativo      │                  │ detector is      │              │
 * │               │                  │ precise)         │              │
 * │               │                  │                  │              │
 * │ Puede matar   │ Si (corruption,  │ No (GC prevents  │ No (borrow   │
 * │ el proceso    │ SIGSEGV)         │ corruption)      │ checker)     │
 * │ por race      │                  │ Maps: si (panic) │              │
 * └───────────────┴──────────────────┴──────────────────┴──────────────┘
 *
 * C: "Here's a gun, try not to shoot yourself."
 * Go: "Here's a gun with a safety, and we'll tell you if you used it wrong."
 * Rust: "You can't even hold the gun unless you prove you know what you're doing."
 */
```

---

## 8. Herramientas de Analisis

### 8.1 go vet

```bash
# go vet detecta ALGUNOS patrones de concurrencia problematicos:
go vet ./...

# Detecta:
# - Copiar sync.Mutex, sync.WaitGroup (valor en lugar de puntero)
# - Llamadas incorrectas a sync/atomic (alignment issues)
# - Algunos usos incorrectos de context

# NO detecta:
# - Data races en general (para eso es -race)
# - Race conditions logicas
# - Deadlocks
```

### 8.2 golangci-lint

```bash
# golangci-lint incluye linters de concurrencia:
golangci-lint run ./...

# Linters relevantes:
# - govet: copias de sync types
# - staticcheck: usos incorrectos de sync/atomic
# - gosec: patrones de seguridad incluyendo concurrencia
# - bodyclose: HTTP response bodies no cerrados (puede causar leaks)
```

### 8.3 go test -race en CI/CD

```yaml
# GitHub Actions — SIEMPRE correr tests con -race:
name: Tests
on: [push, pull_request]
jobs:
  test:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-go@v5
        with:
          go-version: '1.22'
      - name: Test with race detector
        run: go test -race -timeout 5m ./...
        env:
          GORACE: "halt_on_error=1"
          # halt_on_error=1: fallo rapido en el primer race
```

### 8.4 pprof para Mutex Contention

```go
import (
    "net/http"
    _ "net/http/pprof"
    "runtime"
)

func main() {
    // Habilitar mutex profiling
    runtime.SetMutexProfileFraction(1)

    go func() {
        http.ListenAndServe("localhost:6060", nil)
    }()

    // ... tu programa ...
}

// Analizar mutex contention:
// go tool pprof http://localhost:6060/debug/pprof/mutex
// (top) → muestra donde los goroutines esperan por locks
// (web) → visualizacion grafica de contention
```

---

## 9. Programa Completo: Data Race Detector Lab

Un programa pedagogico que demuestra data races comunes, su deteccion con `-race`, y las correcciones correspondientes:

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "sync"
    "sync/atomic"
    "time"
)

// ═════════════════════════════════════════════════════════════════
// ESCENARIO: Sistema de metricas de infraestructura
// Multiples goroutines reportan metricas concurrentemente.
// Versiones BUGGY (con data race) y FIXED (sin data race).
// ═════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────
// DEMO 1: Counter race → atomic fix
// ─────────────────────────────────────────────────────────────────

func demo1_counter_race() {
    fmt.Println("\n=== Demo 1: Counter Race ===")

    // BUGGY version
    var requestCount int
    var wg sync.WaitGroup
    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            requestCount++ // DATA RACE
        }()
    }
    wg.Wait()
    fmt.Printf("  BUGGY:  requestCount = %d (expected 1000)\n", requestCount)

    // FIXED version
    var safeCount atomic.Int64
    for i := 0; i < 1000; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            safeCount.Add(1) // atomic — no race
        }()
    }
    wg.Wait()
    fmt.Printf("  FIXED:  requestCount = %d (expected 1000)\n", safeCount.Load())
}

// ─────────────────────────────────────────────────────────────────
// DEMO 2: Map race → sync.RWMutex fix
// ─────────────────────────────────────────────────────────────────

func demo2_map_race() {
    fmt.Println("\n=== Demo 2: Map Race ===")

    // FIXED version (no podemos correr la buggy — crashea)
    type MetricStore struct {
        mu      sync.RWMutex
        metrics map[string]float64
    }

    store := &MetricStore{
        metrics: make(map[string]float64),
    }

    var wg sync.WaitGroup

    // Writers: 10 goroutines escriben metricas
    services := []string{"api", "db", "cache", "queue", "dns"}
    for i := 0; i < 10; i++ {
        wg.Add(1)
        go func(workerID int) {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                svc := services[rand.Intn(len(services))]
                key := fmt.Sprintf("%s.latency_ms", svc)
                value := rand.Float64() * 100

                store.mu.Lock()
                store.metrics[key] = value
                store.mu.Unlock()
            }
        }(i)
    }

    // Readers: 5 goroutines leen metricas
    for i := 0; i < 5; i++ {
        wg.Add(1)
        go func(readerID int) {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                svc := services[rand.Intn(len(services))]
                key := fmt.Sprintf("%s.latency_ms", svc)

                store.mu.RLock()
                val, exists := store.metrics[key]
                store.mu.RUnlock()

                if exists && readerID == 0 && j == 0 {
                    fmt.Printf("  Read: %s = %.2f ms\n", key, val)
                }
            }
        }(i)
    }

    wg.Wait()

    store.mu.RLock()
    fmt.Printf("  FIXED:  %d unique metrics stored (no crash)\n", len(store.metrics))
    store.mu.RUnlock()
}

// ─────────────────────────────────────────────────────────────────
// DEMO 3: Config race → atomic.Pointer fix
// ─────────────────────────────────────────────────────────────────

type ServerConfig struct {
    Endpoint string
    MaxConns int
    Timeout  time.Duration
}

func demo3_config_race() {
    fmt.Println("\n=== Demo 3: Config Race ===")

    // FIXED: atomic.Pointer para configuracion compartida
    var currentConfig atomic.Pointer[ServerConfig]
    currentConfig.Store(&ServerConfig{
        Endpoint: "api.prod:8080",
        MaxConns: 100,
        Timeout:  5 * time.Second,
    })

    var wg sync.WaitGroup

    // Readers: muchas goroutines leen la config continuamente
    for i := 0; i < 50; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            for j := 0; j < 100; j++ {
                cfg := currentConfig.Load() // atomic read — siempre consistente
                _ = cfg.Endpoint            // safe: cfg no cambia despues de Load
                _ = cfg.MaxConns
            }
        }(i)
    }

    // Writer: actualiza la config periodicamente
    wg.Add(1)
    go func() {
        defer wg.Done()
        configs := []*ServerConfig{
            {Endpoint: "api.prod:8080", MaxConns: 100, Timeout: 5 * time.Second},
            {Endpoint: "api.prod:8081", MaxConns: 200, Timeout: 10 * time.Second},
            {Endpoint: "api.staging:8080", MaxConns: 50, Timeout: 3 * time.Second},
        }
        for i := 0; i < 10; i++ {
            newCfg := configs[i%len(configs)]
            currentConfig.Store(newCfg) // atomic write
            time.Sleep(time.Millisecond)
        }
    }()

    wg.Wait()
    finalCfg := currentConfig.Load()
    fmt.Printf("  FIXED:  final config = %s (max_conns=%d)\n",
        finalCfg.Endpoint, finalCfg.MaxConns)
}

// ─────────────────────────────────────────────────────────────────
// DEMO 4: Shutdown flag race → context.Context fix
// ─────────────────────────────────────────────────────────────────

func demo4_shutdown_race() {
    fmt.Println("\n=== Demo 4: Shutdown Flag Race ===")

    ctx, cancel := context.WithCancel(context.Background())
    var wg sync.WaitGroup

    var processed atomic.Int64

    // Workers que respetan cancelacion
    for i := 0; i < 5; i++ {
        wg.Add(1)
        go func(workerID int) {
            defer wg.Done()
            for {
                select {
                case <-ctx.Done():
                    return // shutdown — safe via channel
                default:
                    processed.Add(1)
                    time.Sleep(time.Millisecond)
                }
            }
        }(i)
    }

    // Dejar que trabajen un poco
    time.Sleep(50 * time.Millisecond)

    // Shutdown limpio
    cancel() // safe: channel-based, no shared variable
    wg.Wait()

    fmt.Printf("  FIXED:  processed %d items before shutdown (no race)\n",
        processed.Load())
}

// ─────────────────────────────────────────────────────────────────
// DEMO 5: Slice append race → confinement fix
// ─────────────────────────────────────────────────────────────────

func demo5_slice_race() {
    fmt.Println("\n=== Demo 5: Slice Append Race ===")

    // FIXED: slice por indice (confinement — sin datos compartidos)
    n := 1000
    results := make([]int, n)
    var wg sync.WaitGroup

    for i := 0; i < n; i++ {
        i := i
        wg.Add(1)
        go func() {
            defer wg.Done()
            results[i] = i * i // cada goroutine escribe a su propio indice
        }()
    }

    wg.Wait()

    // Verificar
    correct := true
    for i := 0; i < n; i++ {
        if results[i] != i*i {
            correct = false
            break
        }
    }
    fmt.Printf("  FIXED:  %d results computed, all correct: %v\n", n, correct)
}

// ─────────────────────────────────────────────────────────────────
// DEMO 6: Once para inicializacion thread-safe
// ─────────────────────────────────────────────────────────────────

type ExpensiveResource struct {
    Name      string
    CreatedAt time.Time
}

var (
    resourceOnce     sync.Once
    sharedResource   *ExpensiveResource
    initCount        atomic.Int64
)

func getResource() *ExpensiveResource {
    resourceOnce.Do(func() {
        initCount.Add(1)
        // Simular inicializacion costosa
        time.Sleep(10 * time.Millisecond)
        sharedResource = &ExpensiveResource{
            Name:      "database-pool",
            CreatedAt: time.Now(),
        }
    })
    return sharedResource
}

func demo6_once() {
    fmt.Println("\n=== Demo 6: sync.Once for Initialization ===")

    var wg sync.WaitGroup
    for i := 0; i < 100; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            r := getResource()
            _ = r.Name // safe: resource is immutable after init
        }()
    }

    wg.Wait()
    fmt.Printf("  FIXED:  initialization called %d time(s) (expected 1)\n",
        initCount.Load())
    fmt.Printf("  Resource: %s (created at %v)\n",
        sharedResource.Name, sharedResource.CreatedAt.Format("15:04:05.000"))
}

// ─────────────────────────────────────────────────────────────────
// DEMO 7: Resumen de patrones
// ─────────────────────────────────────────────────────────────────

func printSummary() {
    fmt.Println("\n════════════════════════════════════════════")
    fmt.Println("  DATA RACE FIXES SUMMARY")
    fmt.Println("════════════════════════════════════════════")
    fmt.Println("  Counter increment   → sync/atomic (Add, Store, Load)")
    fmt.Println("  Map access          → sync.RWMutex + regular map")
    fmt.Println("  Config swap         → atomic.Pointer[T]")
    fmt.Println("  Shutdown flag       → context.Context (channel-based)")
    fmt.Println("  Slice append        → Confinement (index per goroutine)")
    fmt.Println("  Lazy initialization → sync.Once")
    fmt.Println("════════════════════════════════════════════")
    fmt.Println("  DETECCION: go test -race ./...")
    fmt.Println("  CI/CD:     GORACE=halt_on_error=1 go test -race ./...")
    fmt.Println("════════════════════════════════════════════")
}

// ─────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────

func main() {
    fmt.Println("╔═══════════════════════════════════════════╗")
    fmt.Println("║   Data Race Detector Lab                  ║")
    fmt.Println("║   Run with: go run -race main.go          ║")
    fmt.Println("╚═══════════════════════════════════════════╝")

    demo1_counter_race()
    demo2_map_race()
    demo3_config_race()
    demo4_shutdown_race()
    demo5_slice_race()
    demo6_once()
    printSummary()
}
```

### Ejecutar:

```bash
go mod init race-lab
go run -race main.go

# La version BUGGY de demo1 mostrara un warning de data race.
# Las versiones FIXED no deben mostrar ningun warning.

# Para verificar que las versiones fixed son realmente safe:
GORACE="halt_on_error=1" go run -race main.go
# Si hay ALGUN race, el programa termina inmediatamente
```

---

## 10. Ejercicios

### Ejercicio 1: Race Finder

```
Se te da el siguiente programa con 5 data races ocultos.
Encuentra y corrige TODOS sin cambiar el comportamiento:

var (
    activeConns int
    serverStatus string
    metrics     map[string]int64
    lastError   error
    configPath  string
)

func handleConnection(conn net.Conn) {
    activeConns++
    defer func() { activeConns-- }()
    
    data := make([]byte, 1024)
    n, err := conn.Read(data)
    if err != nil {
        lastError = err
        return
    }
    
    metrics["bytes_received"] += int64(n)
    
    if activeConns > 100 {
        serverStatus = "overloaded"
    } else {
        serverStatus = "healthy"
    }
}

Requisitos:
- Ejecutar con -race para confirmar los 5 races
- Corregir usando la herramienta ADECUADA para cada caso
  (no uses Mutex para todo — elige la herramienta correcta)
- Verificar con -race que no hay races despues de los fixes
- Documentar: para cada race, explica POR QUE es un race
  y POR QUE elegiste esa solucion especifica
```

### Ejercicio 2: Thread-Safe Cache

```
Implementa un cache thread-safe con expiracion:

Requisitos:
- Get(key) → (value, bool): lectura concurrente (RLock)
- Set(key, value, ttl): escritura con TTL
- Delete(key): eliminacion
- Cleanup(): goroutine que borra entradas expiradas cada N segundos
- Count(): numero de entradas (atomico, sin lock)
- Keys(): lista de keys (RLock)
- Benchmark: comparar rendimiento con diferentes ratios read:write
  (100:1, 10:1, 1:1) y diferentes mecanismos:
  - sync.RWMutex + map
  - sync.Map
  - Sharded map (N maps con N locks para reducir contention)
- Verificar con -race que no hay data races
- Bonus: implementar eviction por LRU cuando el cache excede N entradas
```

### Ejercicio 3: Concurrent Metrics Aggregator

```
Implementa un sistema de metricas concurrente:

Requisitos:
- Multiples goroutines (simulando servicios) reportan metricas:
  counter (increment), gauge (set value), histogram (observe value)
- Un aggregator recibe metricas via channel (no shared memory)
- Cada 5 segundos, el aggregator calcula y imprime:
  - Counters: total
  - Gauges: ultimo valor
  - Histograms: min, max, avg, p50, p95, p99
- Implementar con ZERO data races (verificar con -race)
- Usar el patron correcto para cada tipo de metrica:
  - Counters: atomic (fast path, no lock)
  - Gauges: atomic.Value o atomic.Pointer
  - Histograms: channel + aggregation en goroutine dedicada
- Comparar el throughput (metricas/segundo) con vs sin -race flag
```

### Ejercicio 4: Data Race Quiz

```
Para cada fragmento de codigo, responde:
¿Hay data race? ¿SI o NO? Si SI, ¿donde exactamente?

// Fragmento 1:
var x int
go func() { x = 1 }()
go func() { x = 2 }()

// Fragmento 2:
ch := make(chan int, 1)
x := 0
go func() { x = 1; ch <- 1 }()
<-ch
fmt.Println(x)

// Fragmento 3:
var mu sync.Mutex
var x int
mu.Lock()
go func() {
    mu.Lock()
    x++
    mu.Unlock()
}()
x++
mu.Unlock()

// Fragmento 4:
results := make([]int, 10)
var wg sync.WaitGroup
for i := 0; i < 10; i++ {
    wg.Add(1)
    go func(idx int) {
        defer wg.Done()
        results[idx] = idx * 2
    }(i)
}
wg.Wait()

// Fragmento 5:
var once sync.Once
var config *Config
go func() { once.Do(func() { config = loadConfig() }) }()
go func() { once.Do(func() { config = loadConfig() }) }()
time.Sleep(time.Second)
fmt.Println(config)

Para cada uno: explicar por que SI o NO hay race,
citando las reglas de "happens-before" de Go.
```

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    DATA RACES — RESUMEN COMPLETO                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  DEFINICION:                                                             │
│  ├─ 2+ goroutines acceden a la misma memoria                           │
│  ├─ Al menos una escribe                                                │
│  └─ Sin sincronizacion                                                  │
│                                                                          │
│  DETECCION:                                                              │
│  ├─ go test -race ./...     (SIEMPRE en CI/CD)                         │
│  ├─ go run -race main.go   (durante desarrollo)                        │
│  ├─ go build -race          (binario instrumentado)                     │
│  └─ GORACE="halt_on_error=1" (fallar rapido)                           │
│                                                                          │
│  SOLUCIONES:                                                             │
│  ├─ Confinement: cada goroutine con sus datos (el mejor)               │
│  ├─ sync/atomic: contadores, flags, punteros                           │
│  ├─ sync.Mutex: seccion critica general                                │
│  ├─ sync.RWMutex: muchos lectores, pocos escritores                    │
│  ├─ sync.Map: map read-heavy                                           │
│  ├─ sync.Once: inicializacion lazy thread-safe                         │
│  ├─ atomic.Pointer: config swap sin lock                               │
│  ├─ Channels: comunicar datos sin compartir memoria                    │
│  └─ context.Context: shutdown signaling                                │
│                                                                          │
│  RACES COMUNES:                                                          │
│  ├─ counter++ (read-modify-write)                                      │
│  ├─ Map concurrent access (CRASH en Go)                                │
│  ├─ Slice append (corrupcion silenciosa)                               │
│  ├─ Interface assignment (2 words, no atomico)                         │
│  ├─ Loop variable capture (closure bug)                                │
│  ├─ Config struct fields (estado parcial)                              │
│  └─ Shutdown flag bool (read-write sin sync)                           │
│                                                                          │
│  REGLA DE ORO:                                                           │
│  "Don't communicate by sharing memory;                                  │
│   share memory by communicating."                                       │
│                                                                          │
│  vs C: undefined behavior (cualquier cosa puede pasar)                 │
│  vs Rust: IMPOSIBLE en safe Rust (compilador lo prohibe)               │
│  Go: detecta en runtime con -race (5-10x overhead)                     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S02 - Channels, T01 - Channels basicos — make(chan T), send (<-), receive, channels con y sin buffer
