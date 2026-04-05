# Goroutines — go Keyword, Modelo M:N, Scheduler de Go

## 1. Introduccion

La concurrencia es una de las razones por las que Go existe. Rob Pike, Ken Thompson y Robert Griesemer disenaron Go en Google en 2007 porque los lenguajes existentes (C++, Java) hacian la concurrencia **dificil, peligrosa o ambas**. Go resolvio esto con tres mecanismos: **goroutines** (unidades de ejecucion), **channels** (comunicacion entre goroutines), y un **scheduler** integrado en el runtime que gestiona todo automaticamente.

Una **goroutine** es una funcion que se ejecuta concurrentemente con otras funciones. Es la unidad fundamental de concurrencia en Go — tan ligera que un programa Go tipico puede tener miles o incluso millones ejecutandose simultaneamente. A diferencia de un thread del sistema operativo (~1MB de stack, gestionado por el kernel), una goroutine cuesta **~2KB de stack**, es gestionada por el **runtime de Go** (no por el kernel), y su creacion es ordenes de magnitud mas rapida.

Para SysAdmin/DevOps, las goroutines son directamente aplicables a:
- Manejar miles de conexiones HTTP/gRPC simultaneas
- Health checks paralelos a multiples servidores
- Pipeline de procesamiento de logs en tiempo real
- Polling concurrente a APIs y bases de datos
- Workers que procesan colas de mensajes

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    POR QUE GO TIENE GOROUTINES                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  EL PROBLEMA (2007):                                                     │
│  ├─ C/C++: threads POSIX → costosos, dificiles, propensos a bugs       │
│  ├─ Java: threads 1:1 → pesados, ThreadPoolExecutor complejo            │
│  ├─ Python: GIL → no hay paralelismo real en threads                    │
│  ├─ Erlang: procesos ligeros → pero nicho, no mainstream                │
│  └─ Servidores de Google manejaban millones de requests/segundo         │
│     con codigo C++ que era DIFICIL de mantener y debuggear              │
│                                                                          │
│  LA SOLUCION DE GO:                                                      │
│  ├─ Goroutines: ligeras (~2KB), baratas de crear, miles/millones        │
│  ├─ Channels: comunicacion segura sin shared memory                     │
│  ├─ Scheduler M:N: runtime mapea goroutines a OS threads                │
│  └─ go keyword: una sola palabra para concurrencia                      │
│                                                                          │
│  INFLUENCIAS:                                                            │
│  ├─ CSP (Communicating Sequential Processes) — Tony Hoare, 1978         │
│  ├─ Newsqueak/Alef/Limbo — lenguajes previos de Rob Pike                │
│  └─ Erlang — procesos ligeros con message passing                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. La Keyword go — Lanzar una Goroutine

### 2.1 Sintaxis Basica

```go
// go <llamada_a_funcion> — lanza la funcion en una nueva goroutine
go doWork()

// Con argumentos:
go processItem(item)

// Con funcion anonima (closure):
go func() {
    fmt.Println("executing in a goroutine")
}() // ← los parentesis () invocan la funcion inmediatamente

// Con closure que captura variables:
name := "server-01"
go func() {
    fmt.Println("checking", name)
}()

// Con closure y argumento (para evitar captura incorrecta):
for _, server := range servers {
    server := server // captura en Go < 1.22
    go func() {
        checkHealth(server)
    }()
}
```

### 2.2 Que Sucede Cuando Escribes go f()

```go
go f(x, y, z)

// El runtime de Go hace lo siguiente:
// 1. Evalua f, x, y, z en la goroutine ACTUAL (no en la nueva)
// 2. Crea una nueva estructura 'g' (goroutine descriptor)
// 3. Asigna un stack inicial de ~2KB para la nueva goroutine
// 4. Coloca la goroutine en la run queue del P (procesador logico) actual
// 5. Retorna INMEDIATAMENTE — no espera a que f empiece
// 6. El scheduler eventualmente ejecuta f(x, y, z) en la nueva goroutine
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                   go f(x, y) — QUE SUCEDE                                │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  goroutine actual (G1):                                                  │
│  │                                                                       │
│  ├─ evaluar f, x, y         ← en G1, ANTES de crear G2                 │
│  │                                                                       │
│  ├─ runtime.newproc()       ← crear goroutine descriptor G2            │
│  │   ├─ allocar stack (~2KB)                                            │
│  │   ├─ copiar f, x, y al stack de G2                                  │
│  │   └─ poner G2 en la run queue del P actual                          │
│  │                                                                       │
│  ├─ continuar ejecucion     ← G1 NO espera a G2                        │
│  │   (la siguiente linea se ejecuta inmediatamente)                     │
│  │                                                                       │
│  └─ ... G2 se ejecutara "eventualmente" ...                             │
│                                                                          │
│  IMPORTANTE:                                                             │
│  - "go f(x)" NO es una promesa, ni un future, ni un async              │
│  - NO retorna un handle — no puedes esperar su resultado directamente  │
│  - Para esperar: WaitGroup, channels, errgroup, o context              │
│  - Los argumentos se evaluan ANTES de crear la goroutine:              │
│                                                                          │
│    i := 10                                                               │
│    go fmt.Println(i)  // imprime 10, no el valor futuro de i           │
│    i = 20             // no afecta a la goroutine — ya copio 10         │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.3 Ejemplo Fundamental

```go
package main

import (
    "fmt"
    "time"
)

func greet(name string) {
    for i := 0; i < 3; i++ {
        fmt.Printf("[%s] hello #%d\n", name, i+1)
        time.Sleep(100 * time.Millisecond)
    }
}

func main() {
    // Lanzar dos goroutines
    go greet("alpha")
    go greet("beta")

    // main es TAMBIEN una goroutine (la goroutine principal)
    greet("main")

    // CUANDO MAIN RETORNA, TODAS LAS GOROUTINES MUEREN
    // No importa si alpha/beta terminaron — el programa sale
}

// Output posible (el orden es NO DETERMINISTICO):
// [main] hello #1
// [alpha] hello #1
// [beta] hello #1
// [main] hello #2
// [beta] hello #2
// [alpha] hello #2
// [main] hello #3
// [alpha] hello #3
// [beta] hello #3
```

### 2.4 La Goroutine Main

```go
func main() {
    // main() se ejecuta en la goroutine PRINCIPAL (goroutine 1)
    // Es especial: cuando main retorna, el programa TERMINA
    // Todas las goroutines mueren inmediatamente (no hay cleanup)

    go longRunningTask()

    // Si main retorna aqui, longRunningTask se aborta
    // SIN ejecutar defers de longRunningTask
    // SIN esperar a que termine
}

// CONSECUENCIA: main debe esperar a las goroutines que lanza
// Opciones para esperar:
// 1. sync.WaitGroup (siguiente topico)
// 2. Channels
// 3. select + signal handling
// 4. time.Sleep (solo para demos — NO en produccion)
// 5. errgroup (ya visto en C07)
```

---

## 3. El Modelo M:N — Goroutines sobre OS Threads

### 3.1 Modelos de Threading

Existen tres modelos de mapeo entre unidades de concurrencia del lenguaje (green threads / goroutines) y threads del sistema operativo:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    MODELOS DE THREADING                                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1:1 (Thread por OS thread)          N:1 (Todas en un OS thread)        │
│  ─────────────────────               ──────────────────────             │
│  Java, C++, Rust (std)              Node.js (event loop)               │
│                                                                          │
│  User     OS                         User     OS                        │
│  T1 ────→ T1                         T1 ─┐                             │
│  T2 ────→ T2                         T2 ─┼──→ T1                       │
│  T3 ────→ T3                         T3 ─┘                             │
│                                                                          │
│  Pro: paralelismo real               Pro: ligero, sin locks             │
│  Con: costoso (~1MB/thread)          Con: NO paralelismo real           │
│       limite OS (~4000-32000)              un bloqueo bloquea todo      │
│                                                                          │
│  M:N (M goroutines en N OS threads)  ← GO USA ESTE                     │
│  ──────────────────────────────────                                      │
│                                                                          │
│  User      Scheduler     OS                                              │
│  G1 ─┐                                                                   │
│  G2 ─┤                                                                   │
│  G3 ─┼──→ P1 ──→ M1 ──→ T1                                             │
│  G4 ─┤    P2 ──→ M2 ──→ T2                                             │
│  G5 ─┤    P3 ──→ M3 ──→ T3                                             │
│  G6 ─┘                                                                   │
│  ...                                                                     │
│  G1000                                                                   │
│                                                                          │
│  M goroutines multiplexadas en N OS threads                              │
│  Pro: paralelismo real + millones de goroutines                         │
│  Con: scheduler runtime tiene overhead                                  │
│                                                                          │
│  G = goroutine (unidad de ejecucion)                                    │
│  M = machine (OS thread)                                                │
│  P = processor (contexto de scheduling, run queue)                      │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.2 El Modelo GMP de Go

El scheduler de Go se llama internamente **GMP** (Goroutine, Machine, Processor):

```
┌──────────────────────────────────────────────────────────────────────────┐
│                       MODELO GMP                                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  G (Goroutine):                                                          │
│  ├─ Estructura: ~400 bytes + stack (empieza en ~2KB, crece hasta 1GB)   │
│  ├─ Contiene: stack pointer, program counter, estado, goroutine ID      │
│  ├─ Estados: _Grunnable, _Grunning, _Gwaiting, _Gsyscall, _Gdead     │
│  └─ Creacion: go f() → runtime.newproc()                               │
│                                                                          │
│  M (Machine / OS Thread):                                                │
│  ├─ Un thread del kernel (pthread en Linux)                             │
│  ├─ Ejecuta goroutines asignadas por el scheduler                      │
│  ├─ Un M debe tener un P asignado para ejecutar goroutines             │
│  ├─ Maximo: GOMAXPROCS activos + threads bloqueados en syscalls         │
│  └─ No tiene limite fijo — se crean segun necesidad                     │
│                                                                          │
│  P (Processor / Contexto logico):                                       │
│  ├─ Contexto de scheduling con run queue local                         │
│  ├─ Cantidad: GOMAXPROCS (default = numero de CPU cores)               │
│  ├─ Cada P tiene una cola local de goroutines runnable                  │
│  ├─ Un P esta vinculado a un M en cada momento                         │
│  └─ Si el M se bloquea (syscall), el P se desvincula y busca otro M   │
│                                                                          │
│                                                                          │
│  RELACIONES:                                                             │
│                                                                          │
│  ┌─────────────────────────────────────────────────┐                    │
│  │  Global Run Queue                                │                    │
│  │  [G7] [G8] [G9] ...                             │                    │
│  └─────────────────────────────────────────────────┘                    │
│           │ (steal cuando local queue vacia)                            │
│           ▼                                                              │
│  ┌──────────────┐   ┌──────────────┐   ┌──────────────┐                │
│  │  P0           │   │  P1           │   │  P2           │               │
│  │  Local Queue: │   │  Local Queue: │   │  Local Queue: │               │
│  │  [G1][G2][G3] │   │  [G4][G5]     │   │  [G6]         │              │
│  │       │       │   │       │       │   │       │       │               │
│  │  Running: G0  │   │  Running: G10 │   │  Running: G11 │              │
│  └──────┬───────┘   └──────┬───────┘   └──────┬───────┘               │
│         │                   │                   │                        │
│  ┌──────▼───────┐   ┌──────▼───────┐   ┌──────▼───────┐                │
│  │  M0 (thread) │   │  M1 (thread) │   │  M2 (thread) │                │
│  └──────────────┘   └──────────────┘   └──────────────┘                │
│                                                                          │
│  GOMAXPROCS=3 → 3 P's → maximo 3 goroutines ejecutando en paralelo    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.3 GOMAXPROCS

```go
import "runtime"

// Ver el valor actual:
fmt.Println(runtime.GOMAXPROCS(0)) // 0 = consultar sin cambiar

// Cambiar en runtime:
old := runtime.GOMAXPROCS(4) // establecer a 4, retorna el anterior

// Cambiar via variable de entorno (antes de que el programa inicie):
// GOMAXPROCS=4 go run main.go

// Default (desde Go 1.5):
// GOMAXPROCS = runtime.NumCPU()
// (numero de CPU cores logicos disponibles)

// ¿Cuando cambiar GOMAXPROCS?
// - Containers: Go detecta CPU del HOST, no del container
//   Solucion: uber-go/automaxprocs
//   import _ "go.uber.org/automaxprocs" // auto-detecta CPU limit del container
// - CPU-bound: GOMAXPROCS = NumCPU() (default, correcto)
// - I/O-bound: GOMAXPROCS puede ser mayor que NumCPU
//   (goroutines bloqueadas en I/O no consumen CPU)

fmt.Printf("CPUs: %d, GOMAXPROCS: %d, Goroutines: %d\n",
    runtime.NumCPU(),
    runtime.GOMAXPROCS(0),
    runtime.NumGoroutine())
```

---

## 4. El Scheduler de Go — En Profundidad

### 4.1 Ciclo de Vida de una Goroutine

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ESTADOS DE UNA GOROUTINE                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│                    go f()                                                │
│                      │                                                   │
│                      ▼                                                   │
│                 ┌──────────┐                                             │
│                 │ Runnable │ ← en la run queue, esperando su turno       │
│                 │ (_Grunnable)                                            │
│                 └────┬─────┘                                             │
│                      │ scheduler asigna a un P/M                        │
│                      ▼                                                   │
│                 ┌──────────┐                                             │
│          ┌──────│ Running  │──────┐                                     │
│          │      │ (_Grunning)     │                                      │
│          │      └──────────┘      │                                      │
│          │           │            │                                       │
│          │    [preemption o       │                                       │
│          │     yield voluntario]   │                                      │
│          │           │            │                                       │
│          │           ▼            │                                       │
│          │    ┌────────────┐      │                                      │
│          │    │ Runnable   │      │ [bloqueado en chan,                   │
│          │    │ (de nuevo) │      │  mutex, I/O, syscall,                │
│          │    └────────────┘      │  time.Sleep, select]                 │
│          │                        │                                       │
│          │                        ▼                                       │
│          │                 ┌───────────┐                                 │
│          │                 │ Waiting   │ ← park: fuera de run queue      │
│          │                 │ (_Gwaiting)│                                 │
│          │                 └─────┬─────┘                                 │
│          │                       │ [evento ocurre:                       │
│          │                       │  chan recibe, mutex libre,            │
│          │                       │  I/O completa, timer expira]         │
│          │                       ▼                                       │
│          │                 ┌──────────┐                                  │
│          │                 │ Runnable │ ← de vuelta en la queue          │
│          │                 └──────────┘                                  │
│          │                                                               │
│          │ [funcion retorna]                                             │
│          ▼                                                               │
│    ┌──────────┐                                                          │
│    │  Dead    │ ← goroutine terminada, stack liberado                   │
│    │ (_Gdead) │   (puede reciclarse para nueva goroutine)               │
│    └──────────┘                                                          │
│                                                                          │
│  Estado especial:                                                        │
│  ┌───────────┐                                                           │
│  │ Syscall   │ ← bloqueado en syscall del kernel                        │
│  │ (_Gsyscall)│  El P se desvincula y busca otro M                      │
│  └───────────┘   (handoff para no bloquear otras goroutines)            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Scheduling Cooperativo y Preemptivo

Go usa un modelo **hibrido** — cooperativo con preemcion asistida:

```go
// COOPERATIVO: la goroutine cede control voluntariamente en ciertos puntos:
// - Llamada a funcion (el compilador puede insertar un check point)
// - Operaciones de canal (send, receive)
// - Llamadas a syscalls (I/O, network)
// - runtime.Gosched() — yield explicito
// - Locks (sync.Mutex)
// - time.Sleep()
// - Allocation (GC check points)

// PREEMPTIVO (desde Go 1.14):
// - El runtime puede interrumpir goroutines en loops largos
//   que no tienen puntos de scheduling naturales
// - Usa senales del SO (SIGURG en Linux) para forzar preemcion
// - Esto resolvio el problema clasico de "goroutine que never yields":

// ANTES de Go 1.14, esto bloqueaba un P permanentemente:
func cpuHog() {
    for { // loop infinito sin function calls ni allocations
        // pre-1.14: NUNCA cede control → un P muerto
        // post-1.14: el runtime lo interrumpe con senal
    }
}
```

### 4.3 Work Stealing

Cuando un P agota su run queue local, **roba** goroutines de otros P's:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                        WORK STEALING                                     │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ANTES:                                                                  │
│  P0: [G1, G2, G3, G4, G5, G6]     P1: []      P2: []                  │
│  P0 tiene todo el trabajo, P1 y P2 estan idle                           │
│                                                                          │
│  WORK STEALING:                                                          │
│  1. P1 intenta obtener de la global queue → vacia                       │
│  2. P1 elige un P aleatorio (P0) y roba la MITAD de su queue           │
│  3. P2 hace lo mismo                                                    │
│                                                                          │
│  DESPUES:                                                                │
│  P0: [G1, G2]     P1: [G3, G4]     P2: [G5, G6]                       │
│  Trabajo distribuido equitativamente                                    │
│                                                                          │
│  ALGORITMO:                                                              │
│  1. Revisar la local run queue del P actual                             │
│  2. Si vacia: revisar la global run queue                               │
│  3. Si vacia: revisar netpoll (I/O ready)                              │
│  4. Si vacia: steal de otro P aleatorio (mitad de su queue)            │
│  5. Si todo vacio: el M se duerme (idle)                                │
│                                                                          │
│  Este algoritmo es O(1) amortizado y escala bien a muchos cores        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.4 Handoff de P en Syscalls

Cuando una goroutine hace una syscall bloqueante (I/O de disco, DNS), el M se bloquea en el kernel. El scheduler **desvincula** el P del M bloqueado y lo asigna a un M libre:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    HANDOFF EN SYSCALLS                                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ANTES de syscall:                                                       │
│  P0 ─── M0                                                              │
│  │       │                                                               │
│  │  Running: G1 (a punto de hacer read() syscall)                       │
│  │  Queue: [G2, G3]                                                     │
│                                                                          │
│  DURANTE syscall:                                                        │
│  G1 llama read() → M0 se bloquea en el kernel                          │
│  │                                                                       │
│  ├─ P0 se desvincula de M0                                              │
│  ├─ P0 busca un M libre (o crea uno nuevo: M3)                         │
│  ├─ P0 ─── M3                                                          │
│  │   Running: G2                                                        │
│  │   Queue: [G3]                                                        │
│  │                                                                       │
│  └─ M0 sigue bloqueado con G1 (sin P)                                  │
│                                                                          │
│  DESPUES de syscall:                                                     │
│  read() retorna → M0 se desbloquea                                     │
│  │                                                                       │
│  ├─ M0 intenta obtener un P libre                                       │
│  ├─ Si hay P libre: M0 toma ese P y sigue ejecutando G1               │
│  ├─ Si NO hay P libre: G1 va a la global queue, M0 se duerme (idle)   │
│                                                                          │
│  RESULTADO:                                                              │
│  Las goroutines G2, G3 NO se bloquearon por la syscall de G1           │
│  El paralelismo real se mantuvo durante toda la operacion              │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.5 Network Poller

Para I/O de red, Go usa un mecanismo aun mas eficiente — el **netpoll** (basado en epoll/kqueue/IOCP):

```go
// Cuando una goroutine hace I/O de red:
conn.Read(buf)

// El runtime NO bloquea el M. En su lugar:
// 1. Registra el file descriptor en epoll (Linux) / kqueue (macOS)
// 2. Pone la goroutine en estado _Gwaiting (park)
// 3. El M sigue ejecutando OTRAS goroutines
// 4. Cuando epoll notifica que el FD esta ready:
//    - La goroutine se pone en _Grunnable (unpark)
//    - Se coloca en una run queue para reanudar

// Esto significa que 10,000 goroutines esperando por red
// NO necesitan 10,000 threads — necesitan 1 goroutine de netpoll
// + GOMAXPROCS threads para ejecutar las que estan ready
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    NETPOLL vs SYSCALL BLOCKING                           │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  I/O DE RED (netpoll):               I/O DE DISCO (syscall blocking):   │
│  ─────────────────────               ────────────────────────────       │
│  conn.Read(buf)                      os.ReadFile("data.txt")            │
│  │                                   │                                   │
│  ├─ NO bloquea el M                 ├─ BLOQUEA el M                    │
│  ├─ Registra FD en epoll            ├─ P hace handoff a otro M         │
│  ├─ Goroutine parked                ├─ Goroutine ejecuta en M bloqueado│
│  ├─ M ejecuta otras goroutines     ├─ Cuando read retorna:            │
│  ├─ epoll notifica FD ready         │   M intenta obtener un P         │
│  └─ Goroutine unparked             └─ Si no hay P: global queue       │
│                                                                          │
│  EFICIENCIA:                         EFICIENCIA:                         │
│  10,000 conexiones =                 10,000 lecturas de disco =         │
│  0 threads extra                     potencialmente 10,000 threads      │
│  (epoll es O(ready_fds))            (el kernel bloquea cada uno)        │
│                                                                          │
│  POR ESO: Go es excelente para servidores de red (HTTP, gRPC, DB)      │
│  PERO: I/O de disco intensivo puede crear muchos threads               │
│  SOLUCION: limitar concurrencia de I/O de disco (semaforo/SetLimit)    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Stack Dinamico de Goroutines

### 5.1 Stack Inicial y Crecimiento

```go
// Cada goroutine empieza con un stack de ~2KB (Go 1.4+, antes era 8KB)
// El stack CRECE automaticamente segun necesidad hasta 1GB (default max)

// ¿Como crece?
// En cada llamada a funcion, el compilador inserta un "stack check":
// - ¿Queda espacio en el stack para esta funcion?
// - SI: continuar normalmente
// - NO: runtime.morestack() → allocar un stack mas grande, copiar todo

// El crecimiento es TRANSPARENTE para el programador:
func deepRecursion(n int) int {
    if n <= 0 {
        return 0
    }
    var buf [256]byte // 256 bytes en el stack en cada frame
    _ = buf
    return deepRecursion(n - 1)
}

// Con goroutines: 1000 goroutines con recursion profunda
// Cada una empieza con 2KB, crece individualmente segun necesidad
// Total memoria: proporcional al uso REAL, no al maximo posible
```

### 5.2 Comparacion de Stacks

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    STACK: Goroutine vs OS Thread                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  OS Thread:                          Goroutine:                          │
│  ─────────                           ──────────                          │
│  ┌────────────────┐ ← 1MB (fijo)    ┌────┐ ← 2KB (inicial)             │
│  │                │                  │    │                              │
│  │                │                  └────┘                              │
│  │   mayormente   │                                                      │
│  │    sin usar    │                  Cuando necesita mas:                │
│  │                │                  ┌────────┐ ← 4KB                   │
│  │                │                  │        │                           │
│  │                │                  └────────┘                          │
│  │                │                                                      │
│  ├────────────────┤ ← stack usado   Cuando necesita mas:                │
│  │   stack real   │                  ┌────────────────┐ ← 8KB           │
│  │   utilizado    │                  │                │                   │
│  └────────────────┘                  └────────────────┘                  │
│                                                                          │
│  1000 threads = 1GB de stack          1000 goroutines = 2MB de stack    │
│  (casi todo sin usar)                (crece segun necesidad real)       │
│                                                                          │
│  CRECIMIENTO:                                                            │
│  Thread: fijo, no crece              Goroutine: 2KB → 4KB → 8KB → ...  │
│  (stack overflow = crash)            → hasta 1GB (runtime.SetMaxStack)  │
│                                                                          │
│  COPIA:                                                                  │
│  Cuando el stack crece, Go copia TODO el contenido al nuevo stack       │
│  y actualiza todos los punteros internos. Esto es posible porque       │
│  Go controla el layout del stack (C/Rust no pueden hacer esto).        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.3 Impacto Practico

```go
package main

import (
    "fmt"
    "runtime"
    "sync"
)

func main() {
    var wg sync.WaitGroup
    n := 100_000

    // Memoria antes
    var m1 runtime.MemStats
    runtime.ReadMemStats(&m1)

    for i := 0; i < n; i++ {
        wg.Add(1)
        go func() {
            defer wg.Done()
            // Goroutine minima — no hace nada, solo existe
            runtime.Gosched() // yield para que el scheduler la registre
        }()
    }

    // Memoria despues de crear 100K goroutines
    var m2 runtime.MemStats
    runtime.ReadMemStats(&m2)

    fmt.Printf("Created %d goroutines\n", n)
    fmt.Printf("Memory used: ~%.1f MB\n", float64(m2.Sys-m1.Sys)/(1024*1024))
    fmt.Printf("Per goroutine: ~%.0f bytes\n", float64(m2.Sys-m1.Sys)/float64(n))
    // Output tipico: ~400-500 bytes por goroutine (descriptor + stack minimo)

    wg.Wait()
    fmt.Printf("Active goroutines after Wait: %d\n", runtime.NumGoroutine())
}
```

---

## 6. Goroutine Internals — La Estructura g

### 6.1 Estructura Interna Simplificada

```go
// Pseudocodigo de la estructura interna (runtime/runtime2.go):
type g struct {
    // Stack
    stack       stack   // limites del stack: [stack.lo, stack.hi)
    stackguard0 uintptr // check para stack overflow

    // Estado
    atomicstatus atomic.Uint32 // _Grunnable, _Grunning, _Gwaiting, etc.
    goid         uint64        // goroutine ID (NO expuesto publicamente en Go)

    // Scheduling
    sched     gobuf // registros guardados (SP, PC, etc.) para context switch
    m         *m    // M que esta ejecutando esta goroutine (nil si no esta running)
    waitreason waitReason // por que esta esperando (si _Gwaiting)

    // Panic/defer
    _panic *_panic // stack de panics activos
    _defer *_defer // stack de defers pendientes

    // Preemption
    preempt     bool // solicitud de preemcion pendiente
    preemptStop bool // detener en el proximo scheduling point

    // GC
    gcAssistBytes int64 // creditos de GC assist
}

type gobuf struct {
    sp   uintptr // stack pointer
    pc   uintptr // program counter
    g    guintptr // puntero a la g
    ret  uintptr // valor de retorno
    // ... mas registros para context switch
}

// NOTA: Go NO expone el goroutine ID publicamente
// runtime.Goid() no existe en la API publica
// Esto es INTENCIONAL — previene patrones como thread-local storage
// que Go considera anti-patrones
```

### 6.2 Goroutine ID — Por Que No Existe Publicamente

```go
// En muchos lenguajes puedes obtener el thread ID:
// Java: Thread.currentThread().getId()
// Python: threading.get_ident()
// C: pthread_self()
// Rust: std::thread::current().id()

// En Go, el goroutine ID existe internamente pero NO esta expuesto:
// ¿Por que?

// 1. Prevenir thread-local storage (TLS) patterns
//    TLS es un anti-patron en Go porque:
//    - Rompe la composabilidad (una funcion se comporta diferente segun quien la llama)
//    - Dificulta el testing
//    - Crea dependencias implicitas

// 2. La alternativa Go: pasar contexto explicitamente
//    context.Context es el reemplazo de TLS en Go

// Si REALMENTE necesitas el goroutine ID (debugging):
import "runtime"

func goroutineID() uint64 {
    var buf [64]byte
    n := runtime.Stack(buf[:], false)
    // buf contiene: "goroutine 42 [running]:\n..."
    // parsear el numero — esto es un HACK, no para produccion
    var id uint64
    for i := len("goroutine "); buf[i] != ' '; i++ {
        id = id*10 + uint64(buf[i]-'0')
    }
    return id
}
// NUNCA usar esto en produccion — es lento y fragil
```

---

## 7. Patrones Basicos con Goroutines

### 7.1 Fire and Forget

```go
// Lanzar trabajo que no nos importa si falla
// ⚠️ PELIGROSO: si la goroutine hace panic, el programa muere
go sendMetrics(data)
go logAuditEvent(event)

// Mejor: con recovery
go func() {
    defer func() {
        if r := recover(); r != nil {
            log.Printf("metrics panic: %v", r)
        }
    }()
    sendMetrics(data)
}()
```

### 7.2 Resultado via Channel

```go
// Obtener resultado de una goroutine via channel
func fetchAsync(url string) <-chan Result {
    ch := make(chan Result, 1) // buffer 1 para no bloquear la goroutine
    go func() {
        data, err := fetch(url)
        ch <- Result{Data: data, Err: err}
    }()
    return ch
}

// Uso:
resultCh := fetchAsync("https://api.example.com/data")
// ... hacer otro trabajo mientras fetch esta en progreso ...
result := <-resultCh // bloquear solo cuando necesitamos el resultado
if result.Err != nil {
    log.Fatal(result.Err)
}
```

### 7.3 Worker Pool Simple

```go
func workerPool(jobs <-chan Job, results chan<- Result, numWorkers int) {
    var wg sync.WaitGroup

    for i := 0; i < numWorkers; i++ {
        wg.Add(1)
        go func(workerID int) {
            defer wg.Done()
            for job := range jobs {
                result := process(job)
                results <- result
            }
        }(i)
    }

    // Cerrar results cuando todos los workers terminen
    go func() {
        wg.Wait()
        close(results)
    }()
}

// Uso:
jobs := make(chan Job, 100)
results := make(chan Result, 100)

workerPool(jobs, results, 10) // 10 workers

// Enviar trabajo
for _, item := range items {
    jobs <- Job{Item: item}
}
close(jobs) // senalar que no hay mas trabajo

// Recoger resultados
for result := range results {
    fmt.Println(result)
}
```

### 7.4 Timeout con Context

```go
func fetchWithTimeout(url string, timeout time.Duration) ([]byte, error) {
    ctx, cancel := context.WithTimeout(context.Background(), timeout)
    defer cancel()

    type result struct {
        data []byte
        err  error
    }
    ch := make(chan result, 1)

    go func() {
        data, err := fetchData(ctx, url)
        ch <- result{data, err}
    }()

    select {
    case res := <-ch:
        return res.data, res.err
    case <-ctx.Done():
        return nil, fmt.Errorf("fetch %s: %w", url, ctx.Err())
    }
}
```

### 7.5 Periodic Worker

```go
func periodicCheck(ctx context.Context, interval time.Duration, check func(context.Context)) {
    ticker := time.NewTicker(interval)
    defer ticker.Stop()

    // Ejecutar inmediatamente la primera vez
    check(ctx)

    for {
        select {
        case <-ctx.Done():
            return // contexto cancelado, salir
        case <-ticker.C:
            check(ctx)
        }
    }
}

// Uso:
ctx, cancel := context.WithCancel(context.Background())
defer cancel()

go periodicCheck(ctx, 30*time.Second, func(ctx context.Context) {
    if err := healthCheck(ctx); err != nil {
        log.Printf("health check failed: %v", err)
    }
})
```

---

## 8. Goroutine Leaks

### 8.1 Que es un Leak

Una goroutine leak ocurre cuando una goroutine se bloquea **para siempre** y nunca retorna. El stack y los recursos que referencia **nunca se liberan**. A diferencia de un memory leak en C (que puedes detectar con Valgrind), un goroutine leak es mas sutil porque la goroutine sigue "viva" pero inutil.

```go
// LEAK 1: channel sin receptor
func leakyChannel() {
    ch := make(chan int) // unbuffered
    go func() {
        result := expensiveComputation()
        ch <- result // ← BLOQUEA PARA SIEMPRE si nadie lee de ch
    }()
    // Si la funcion retorna sin leer de ch, la goroutine queda bloqueada
}

// LEAK 2: channel sin emisor (el receptor espera eternamente)
func leakyReceiver() {
    ch := make(chan int)
    go func() {
        val := <-ch // ← BLOQUEA PARA SIEMPRE si nadie escribe a ch
        process(val)
    }()
    // Si nunca enviamos a ch, la goroutine queda bloqueada
}

// LEAK 3: goroutine que nunca ve cancelacion
func leakyWorker(ctx context.Context) {
    go func() {
        for {
            doWork() // ← si doWork no verifica ctx, sigue para siempre
            // Deberia tener: select { case <-ctx.Done(): return; default: }
        }
    }()
}
```

### 8.2 Como Detectar Leaks

```go
// 1. runtime.NumGoroutine() — verificar que el numero no crece
func TestNoLeak(t *testing.T) {
    before := runtime.NumGoroutine()

    doSomething() // la funcion bajo test

    time.Sleep(100 * time.Millisecond) // dar tiempo a goroutines para terminar

    after := runtime.NumGoroutine()
    if after > before {
        t.Errorf("goroutine leak: before=%d after=%d", before, after)
    }
}

// 2. uber-go/goleak — libreria especializada para detectar leaks en tests
import "go.uber.org/goleak"

func TestMain(m *testing.M) {
    goleak.VerifyTestMain(m) // detecta goroutines que sobreviven los tests
}

func TestNoLeak(t *testing.T) {
    defer goleak.VerifyNone(t) // verifica al final del test
    doSomething()
}

// 3. pprof — goroutine profiling
import _ "net/http/pprof"

go func() {
    http.ListenAndServe("localhost:6060", nil)
}()
// Luego: go tool pprof http://localhost:6060/debug/pprof/goroutine
//        (top) → muestra donde estan bloqueadas las goroutines
```

### 8.3 Como Prevenir Leaks

```go
// REGLA: toda goroutine debe tener una forma de terminar

// Pattern 1: context para cancelacion
func worker(ctx context.Context) {
    for {
        select {
        case <-ctx.Done():
            return // SIEMPRE tener una salida
        default:
            doWork()
        }
    }
}

// Pattern 2: done channel
func worker(done <-chan struct{}) {
    for {
        select {
        case <-done:
            return
        default:
            doWork()
        }
    }
}

// Pattern 3: buffered channel para fire-and-forget
func noLeak() {
    ch := make(chan int, 1) // buffer 1 → el send nunca bloquea
    go func() {
        ch <- expensiveComputation()
    }()
    // Incluso si no leemos de ch, la goroutine termina
    // (el valor queda en el buffer hasta que ch sea garbage collected)
}
```

---

## 9. Comparacion con C y Rust

### 9.1 C — pthreads

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void *worker(void *arg) {
    int id = *(int *)arg;
    printf("Worker %d starting\n", id);
    sleep(1); // simular trabajo
    printf("Worker %d done\n", id);
    free(arg); // liberar memoria del argumento
    return NULL;
}

int main() {
    int num_workers = 5;
    pthread_t threads[5];

    for (int i = 0; i < num_workers; i++) {
        int *id = malloc(sizeof(int)); // necesario para pasar al thread
        *id = i;

        int ret = pthread_create(&threads[i], NULL, worker, id);
        if (ret != 0) {
            fprintf(stderr, "pthread_create failed: %d\n", ret);
            return 1;
        }
    }

    // Esperar a todos los threads (equivalente a WaitGroup.Wait)
    for (int i = 0; i < num_workers; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("All workers done\n");
    return 0;
}

/*
 * DIFERENCIAS vs Go:
 *
 * Creacion:
 *   Go: go worker(i)                    — 1 palabra
 *   C:  pthread_create(&t, NULL, f, arg) — 4 args + error check + malloc
 *
 * Stack:
 *   Go: 2KB inicial, crece dinamicamente
 *   C:  1MB fijo (configurable con pthread_attr_setstacksize)
 *
 * Coste:
 *   Go: ~2-5 microsegundos por goroutine
 *   C:  ~10-50 microsegundos por thread (syscall clone)
 *
 * Limite:
 *   Go: millones (solo memoria)
 *   C:  miles (ulimit -u, tipicamente 4096-32768)
 *
 * Scheduling:
 *   Go: runtime scheduler (M:N, work stealing)
 *   C:  kernel scheduler (1:1, CFS en Linux)
 *
 * Limpieza:
 *   Go: GC recoge goroutines muertas
 *   C:  pthread_join obligatorio (o detach) para liberar recursos
 *
 * Comunicacion:
 *   Go: channels (built-in)
 *   C:  pipes, shared memory + mutex, condition variables
 */
```

### 9.2 Rust — std::thread y tokio::spawn

```rust
use std::thread;
use std::time::Duration;

// ─── std::thread: threads del OS (1:1) ───

fn main() {
    let mut handles = vec![];

    for i in 0..5 {
        // thread::spawn retorna un JoinHandle (Go no retorna nada)
        let handle = thread::spawn(move || {
            println!("Worker {} starting", i);
            thread::sleep(Duration::from_secs(1));
            println!("Worker {} done", i);
            i * 2 // retornar valor (Go no puede hacer esto directamente)
        });
        handles.push(handle);
    }

    // Join todos los threads — obtener resultados
    for handle in handles {
        match handle.join() {
            Ok(result) => println!("Result: {}", result),
            Err(e) => println!("Thread panicked: {:?}", e),
        }
    }
}

// ─── tokio: async tasks (M:N como goroutines) ───

#[tokio::main]
async fn main() {
    let mut handles = vec![];

    for i in 0..5 {
        // tokio::spawn es lo mas parecido a go func()
        let handle = tokio::spawn(async move {
            println!("Worker {} starting", i);
            tokio::time::sleep(Duration::from_secs(1)).await;
            println!("Worker {} done", i);
            i * 2 // retornar valor
        });
        handles.push(handle);
    }

    for handle in handles {
        match handle.await {
            Ok(result) => println!("Result: {}", result),
            Err(e) => println!("Task panicked: {}", e),
        }
    }
}

/*
 * COMPARACION Go vs Rust:
 *
 * ┌─────────────────┬──────────────────┬──────────────────────────────┐
 * │                 │ Go goroutine     │ Rust                         │
 * ├─────────────────┼──────────────────┼──────────────────────────────┤
 * │ Crear           │ go f()           │ thread::spawn / tokio::spawn │
 * │ Retorno         │ ninguno          │ JoinHandle<T> con valor      │
 * │ Esperar         │ WaitGroup/chan   │ .join() / .await             │
 * │ Stack           │ 2KB dinamico     │ thread: 2MB / tokio: tiny    │
 * │ Modelo          │ M:N (built-in)   │ 1:1 (std) o M:N (tokio)     │
 * │ Scheduler       │ Runtime (siempre)│ OS (std) o tokio (async)     │
 * │ Cancelacion     │ context.Context  │ CancellationToken / drop     │
 * │ Panic           │ crash (recover)  │ JoinError (capturado)        │
 * │ Send safety     │ Runtime (-race)  │ Compile-time (Send trait)    │
 * │ Data sharing    │ mutex/channel    │ Arc<Mutex<T>> (compile-time) │
 * └─────────────────┴──────────────────┴──────────────────────────────┘
 *
 * DIFERENCIA CLAVE:
 * Go: go f() — no necesitas pensar en ownership, el GC se encarga
 * Rust: spawn(move || ...) — move OBLIGA a transferir ownership
 *       El compilador RECHAZA si intentas compartir datos sin Arc/Mutex
 *       → Imposible crear data races (detectado en compile-time)
 *       Go detecta data races en runtime con -race flag
 */
```

---

## 10. Herramientas de Inspeccion

### 10.1 runtime.NumGoroutine

```go
fmt.Println("Active goroutines:", runtime.NumGoroutine())
// Incluye: main, goroutines de runtime, y las que tu creaste
```

### 10.2 GODEBUG=gctrace y schedtrace

```bash
# Ver actividad del scheduler (cada 1000ms):
GODEBUG=schedtrace=1000 go run main.go

# Output:
# SCHED 0ms: gomaxprocs=8 idleprocs=7 threads=5 spinningthreads=0
#   idlethreads=2 runqueue=0 [0 0 0 0 0 0 0 0]
#
# gomaxprocs: numero de P's
# idleprocs: P's sin trabajo
# threads: total de M's creados
# runqueue: goroutines en la global queue
# [0 0 0 0 ...]: goroutines en cada local queue de P

# Ver detalles del scheduling (scheddetail):
GODEBUG=schedtrace=1000,scheddetail=1 go run main.go
```

### 10.3 pprof para Goroutines

```go
import (
    "net/http"
    _ "net/http/pprof"
    "runtime"
)

func main() {
    // Exponer pprof endpoint
    go func() {
        http.ListenAndServe("localhost:6060", nil)
    }()

    // ... tu programa ...
}

// Desde la terminal:
// Ver goroutines activas:
//   go tool pprof http://localhost:6060/debug/pprof/goroutine
//   (top) → muestra donde estan bloqueadas
//   (traces) → full stack traces

// Ver en el navegador:
//   http://localhost:6060/debug/pprof/goroutine?debug=1
//   → Lista todas las goroutines con su stack trace

// Dump de goroutines por senal (como Java thread dump):
// kill -SIGQUIT <pid>
// → El runtime imprime el stack trace de TODAS las goroutines a stderr
```

### 10.4 Stack Trace Manual

```go
import "runtime/debug"

// Imprimir stack trace de la goroutine actual:
debug.PrintStack()

// Obtener stack trace de TODAS las goroutines:
buf := make([]byte, 1<<20) // 1MB buffer
n := runtime.Stack(buf, true) // true = todas las goroutines
fmt.Printf("%s\n", buf[:n])
```

---

## 11. Programa Completo: Infrastructure Scanner

Un scanner de infraestructura que usa goroutines para verificar multiples servicios concurrentemente, demostrando los patrones basicos:

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "os"
    "os/signal"
    "runtime"
    "sync"
    "syscall"
    "time"
)

// ─────────────────────────────────────────────────────────────────
// Modelos
// ─────────────────────────────────────────────────────────────────

type ServiceType string

const (
    TypeHTTP     ServiceType = "HTTP"
    TypeDatabase ServiceType = "Database"
    TypeCache    ServiceType = "Cache"
    TypeDNS      ServiceType = "DNS"
    TypeQueue    ServiceType = "Queue"
)

type Service struct {
    Name     string
    Type     ServiceType
    Endpoint string
}

type ScanResult struct {
    Service     Service
    Healthy     bool
    Latency     time.Duration
    Message     string
    ScannedAt   time.Time
    GoroutineID string // para demostrar concurrencia
}

func (r ScanResult) String() string {
    status := "OK  "
    if !r.Healthy {
        status = "FAIL"
    }
    return fmt.Sprintf("[%s] %-22s %-10s latency=%-8v goroutine=%-6s %s",
        status, r.Service.Name, r.Service.Type,
        r.Latency.Truncate(time.Millisecond),
        r.GoroutineID, r.Message)
}

// ─────────────────────────────────────────────────────────────────
// Scanner simulado
// ─────────────────────────────────────────────────────────────────

func scanService(ctx context.Context, svc Service) ScanResult {
    start := time.Now()

    // Obtener goroutine ID (solo para demo — no hacer en produccion)
    var buf [32]byte
    n := runtime.Stack(buf[:], false)
    gid := ""
    s := string(buf[:n])
    for i := len("goroutine "); i < len(s) && s[i] != ' '; i++ {
        gid += string(s[i])
    }

    // Simular latencia variable segun tipo
    var baseLatency time.Duration
    switch svc.Type {
    case TypeHTTP:
        baseLatency = 50 * time.Millisecond
    case TypeDatabase:
        baseLatency = 80 * time.Millisecond
    case TypeCache:
        baseLatency = 10 * time.Millisecond
    case TypeDNS:
        baseLatency = 5 * time.Millisecond
    case TypeQueue:
        baseLatency = 30 * time.Millisecond
    }

    jitter := time.Duration(rand.Intn(50)) * time.Millisecond
    scanDuration := baseLatency + jitter

    // Respetar cancelacion
    select {
    case <-ctx.Done():
        return ScanResult{
            Service:     svc,
            Healthy:     false,
            Latency:     time.Since(start),
            Message:     "cancelled: " + ctx.Err().Error(),
            ScannedAt:   time.Now(),
            GoroutineID: gid,
        }
    case <-time.After(scanDuration):
        // scan completado
    }

    // Simular resultados
    healthy := true
    message := "all checks passed"

    switch {
    case svc.Name == "failing-api":
        healthy = false
        message = "connection refused"
    case svc.Name == "slow-db":
        healthy = false
        message = "query timeout after 5s"
    case svc.Name == "degraded-cache":
        healthy = true
        message = "responding but high latency (degraded)"
    }

    return ScanResult{
        Service:     svc,
        Healthy:     healthy,
        Latency:     time.Since(start),
        Message:     message,
        ScannedAt:   time.Now(),
        GoroutineID: gid,
    }
}

// ─────────────────────────────────────────────────────────────────
// Scanner con goroutines basicas + WaitGroup
// ─────────────────────────────────────────────────────────────────

func scanAll(ctx context.Context, services []Service) []ScanResult {
    var wg sync.WaitGroup
    results := make([]ScanResult, len(services))

    fmt.Printf("\nStarting scan of %d services using %d CPU cores (GOMAXPROCS=%d)\n",
        len(services), runtime.NumCPU(), runtime.GOMAXPROCS(0))
    fmt.Printf("Active goroutines before scan: %d\n", runtime.NumGoroutine())

    start := time.Now()

    for i, svc := range services {
        i, svc := i, svc // captura
        wg.Add(1)
        go func() {
            defer wg.Done()
            results[i] = scanService(ctx, svc)
        }()
    }

    fmt.Printf("Active goroutines during scan: %d\n", runtime.NumGoroutine())

    wg.Wait()

    elapsed := time.Since(start)
    fmt.Printf("Scan completed in %v (sequential would be ~%v)\n",
        elapsed.Truncate(time.Millisecond),
        estimateSequential(services))

    return results
}

func estimateSequential(services []Service) time.Duration {
    // Estimacion: ~100ms por servicio
    return time.Duration(len(services)) * 100 * time.Millisecond
}

// ─────────────────────────────────────────────────────────────────
// Scanner con channel para resultados en streaming
// ─────────────────────────────────────────────────────────────────

func scanStream(ctx context.Context, services []Service) <-chan ScanResult {
    results := make(chan ScanResult, len(services))

    go func() {
        var wg sync.WaitGroup
        for _, svc := range services {
            svc := svc
            wg.Add(1)
            go func() {
                defer wg.Done()
                results <- scanService(ctx, svc)
            }()
        }
        wg.Wait()
        close(results) // cerrar cuando todas terminen
    }()

    return results
}

// ─────────────────────────────────────────────────────────────────
// Scanner periodico con graceful shutdown
// ─────────────────────────────────────────────────────────────────

func scanPeriodic(ctx context.Context, services []Service, interval time.Duration) {
    ticker := time.NewTicker(interval)
    defer ticker.Stop()

    round := 0
    for {
        round++
        fmt.Printf("\n══════════ Scan Round %d ══════════\n", round)

        results := scanAll(ctx, services)

        healthy, unhealthy := 0, 0
        for _, r := range results {
            fmt.Println(r)
            if r.Healthy {
                healthy++
            } else {
                unhealthy++
            }
        }
        fmt.Printf("── Summary: %d/%d healthy\n", healthy, len(services))

        select {
        case <-ctx.Done():
            fmt.Printf("\nScanner stopped: %v\n", ctx.Err())
            return
        case <-ticker.C:
            // siguiente ronda
        }
    }
}

// ─────────────────────────────────────────────────────────────────
// Demostrar la diferencia goroutines vs secuencial
// ─────────────────────────────────────────────────────────────────

func benchmark(ctx context.Context, services []Service) {
    fmt.Println("\n── Benchmark: Sequential vs Concurrent ──")

    // Secuencial
    start := time.Now()
    seqResults := make([]ScanResult, len(services))
    for i, svc := range services {
        seqResults[i] = scanService(ctx, svc)
    }
    seqTime := time.Since(start)

    // Concurrente (goroutines)
    start = time.Now()
    _ = scanAll(ctx, services)
    concTime := time.Since(start)

    fmt.Printf("\nSequential:  %v\n", seqTime.Truncate(time.Millisecond))
    fmt.Printf("Concurrent:  %v\n", concTime.Truncate(time.Millisecond))
    fmt.Printf("Speedup:     %.1fx faster\n", float64(seqTime)/float64(concTime))
}

// ─────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────

func main() {
    services := []Service{
        {Name: "api-gateway", Type: TypeHTTP, Endpoint: "api.prod:8080"},
        {Name: "user-service", Type: TypeHTTP, Endpoint: "users.prod:8081"},
        {Name: "auth-service", Type: TypeHTTP, Endpoint: "auth.prod:8082"},
        {Name: "postgres-primary", Type: TypeDatabase, Endpoint: "db.prod:5432"},
        {Name: "postgres-replica", Type: TypeDatabase, Endpoint: "db-ro.prod:5432"},
        {Name: "redis-cache", Type: TypeCache, Endpoint: "redis.prod:6379"},
        {Name: "memcached", Type: TypeCache, Endpoint: "mc.prod:11211"},
        {Name: "rabbitmq", Type: TypeQueue, Endpoint: "mq.prod:5672"},
        {Name: "dns-resolver", Type: TypeDNS, Endpoint: "dns.prod:53"},
        {Name: "cdn-endpoint", Type: TypeHTTP, Endpoint: "cdn.prod:443"},
        {Name: "failing-api", Type: TypeHTTP, Endpoint: "fail.prod:8080"},
        {Name: "slow-db", Type: TypeDatabase, Endpoint: "slow.prod:5432"},
        {Name: "degraded-cache", Type: TypeCache, Endpoint: "cache2.prod:6379"},
    }

    ctx, stop := signal.NotifyContext(context.Background(), os.Interrupt, syscall.SIGTERM)
    defer stop()

    fmt.Println("╔══════════════════════════════════════╗")
    fmt.Println("║   Infrastructure Scanner             ║")
    fmt.Printf("║   Services: %-3d  CPUs: %-3d           ║\n",
        len(services), runtime.NumCPU())
    fmt.Printf("║   GOMAXPROCS: %-3d                     ║\n",
        runtime.GOMAXPROCS(0))
    fmt.Println("╚══════════════════════════════════════╝")

    // Modo 1: Scan unico con resultados en batch
    fmt.Println("\n── Mode 1: Batch Scan ──")
    results := scanAll(ctx, services)
    for _, r := range results {
        fmt.Println(r)
    }

    // Modo 2: Scan con resultados en streaming (channel)
    fmt.Println("\n── Mode 2: Streaming Scan ──")
    for r := range scanStream(ctx, services) {
        fmt.Println(r) // resultados llegan conforme se completan
    }

    // Modo 3: Benchmark
    benchmark(ctx, services)

    // Modo 4: Scan periodico (descomentar para demo interactiva)
    // fmt.Println("\n── Mode 4: Periodic Scan (Ctrl+C to stop) ──")
    // scanPeriodic(ctx, services, 5*time.Second)

    fmt.Printf("\nFinal goroutine count: %d\n", runtime.NumGoroutine())
    fmt.Println("\n=== Scanner Complete ===")
}
```

### Ejecutar:

```bash
go mod init infra-scanner
go run main.go

# Con race detector:
go run -race main.go

# Con schedule tracing:
GODEBUG=schedtrace=100 go run main.go

# Output esperado (ejemplo):
# ╔══════════════════════════════════════╗
# ║   Infrastructure Scanner             ║
# ║   Services: 13   CPUs: 8            ║
# ║   GOMAXPROCS: 8                      ║
# ╚══════════════════════════════════════╝
#
# ── Mode 1: Batch Scan ──
# Starting scan of 13 services using 8 CPU cores (GOMAXPROCS=8)
# Active goroutines before scan: 1
# Active goroutines during scan: 14
# Scan completed in 97ms (sequential would be ~1.3s)
# [OK  ] api-gateway            HTTP       latency=72ms    goroutine=18    all checks passed
# [OK  ] user-service           HTTP       latency=85ms    goroutine=19    all checks passed
# [OK  ] postgres-primary       Database   latency=91ms    goroutine=22    all checks passed
# [FAIL] failing-api            HTTP       latency=63ms    goroutine=28    connection refused
# [FAIL] slow-db                Database   latency=97ms    goroutine=29    query timeout after 5s
# ...
#
# ── Benchmark: Sequential vs Concurrent ──
# Sequential:  1247ms
# Concurrent:  98ms
# Speedup:     12.7x faster
```

---

## 12. Ejercicios

### Ejercicio 1: Port Scanner Concurrente

```
Implementa un scanner de puertos que verifica multiples puertos en paralelo:

Requisitos:
- Recibir: host, rango de puertos (ej: 1-1024), timeout por puerto
- Lanzar una goroutine por cada puerto (con WaitGroup)
- Usar net.DialTimeout para verificar si el puerto esta abierto
- Recolectar resultados en un slice (por indice, sin mutex)
- Imprimir puertos abiertos ordenados
- Medir tiempo total vs estimacion secuencial
- Comparar el tiempo con diferentes GOMAXPROCS (1, 4, 8, runtime.NumCPU())
- Bonus: agregar SetLimit equivalente con un semaforo (chan struct{})
  para no abrir mas de 100 conexiones simultaneas
```

### Ejercicio 2: Log Processor con Goroutines

```
Implementa un procesador de logs que lee de multiples fuentes concurrentemente:

Requisitos:
- Simular 5 fuentes de logs (cada una genera lineas con Sleep aleatorio)
- Cada fuente corre en su propia goroutine
- Las lineas se envian a un channel compartido
- Una goroutine "aggregator" lee del channel e imprime con timestamp
- Implementar graceful shutdown con context.Context
- Usar runtime.NumGoroutine() para mostrar goroutines activas
- Verificar con -race que no hay data races
- Bonus: agregar una goroutine que imprime estadisticas cada 5 segundos
  (lineas/segundo por fuente, total)
```

### Ejercicio 3: Goroutine Profiling

```
Implementa un programa que demuestra los internals del scheduler:

Requisitos:
- Crear goroutines CPU-bound (loop matematico) y I/O-bound (time.Sleep)
- Ejecutar con GODEBUG=schedtrace=100 y explicar el output
- Medir el impacto de GOMAXPROCS en goroutines CPU-bound:
  - GOMAXPROCS=1: ¿cuanto tarda?
  - GOMAXPROCS=4: ¿cuanto tarda?
  - GOMAXPROCS=NumCPU(): ¿cuanto tarda?
- Crear un goroutine leak intencional y detectarlo con:
  - runtime.NumGoroutine()
  - runtime.Stack() dump
- Corregir el leak y verificar que el conteo vuelve a la normalidad
- Documentar: ¿por que las goroutines I/O-bound no se benefician
  de mas GOMAXPROCS pero las CPU-bound si?
```

### Ejercicio 4: Concurrent DNS Resolver

```
Implementa un resolver DNS concurrente:

Requisitos:
- Leer una lista de dominios de un archivo (uno por linea)
- Resolver cada dominio en una goroutine separada (net.LookupHost)
- Limitar concurrencia a 50 goroutines simultaneas (semaforo con channel)
- Timeout global de 30 segundos (context.WithTimeout)
- Timeout per-domain de 2 segundos
- Imprimir resultados conforme llegan (channel streaming)
- Reporte final: resueltos, fallidos, timeout, total time
- Bonus: comparar rendimiento con 1, 10, 50, 100 goroutines
  ¿Donde esta el "sweet spot"? ¿Por que?
```

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    GOROUTINES — RESUMEN COMPLETO                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  CREAR:                                                                  │
│  ├─ go f()              → lanzar funcion en nueva goroutine             │
│  ├─ go func() { ... }() → lanzar closure                               │
│  └─ Argumentos evaluados ANTES de crear la goroutine                   │
│                                                                          │
│  MODELO M:N (GMP):                                                       │
│  ├─ G: goroutine (~2KB stack, crece hasta 1GB)                          │
│  ├─ M: OS thread (ejecuta goroutines)                                   │
│  ├─ P: procesador logico (run queue, GOMAXPROCS)                        │
│  └─ N goroutines multiplexadas en GOMAXPROCS threads                    │
│                                                                          │
│  SCHEDULER:                                                              │
│  ├─ Cooperativo + preemptivo (Go 1.14+)                                │
│  ├─ Work stealing (P roba de otro P cuando esta idle)                  │
│  ├─ Handoff (P se desvincula de M bloqueado en syscall)                │
│  ├─ Netpoll (epoll/kqueue para I/O de red no-bloqueante)               │
│  └─ Schedule points: function calls, chan ops, syscalls, locks          │
│                                                                          │
│  STACK:                                                                  │
│  ├─ Inicial: ~2KB (vs ~1MB de OS thread)                               │
│  ├─ Crece: automaticamente por copia (2KB → 4KB → 8KB → ...)          │
│  ├─ Maximo: 1GB (configurable)                                         │
│  └─ 1000 goroutines = ~2MB vs 1000 threads = ~1GB                     │
│                                                                          │
│  PATRONES BASICOS:                                                       │
│  ├─ Fire and forget (con recover)                                       │
│  ├─ Resultado via channel                                               │
│  ├─ Worker pool                                                         │
│  ├─ Timeout con context                                                 │
│  └─ Periodic worker                                                     │
│                                                                          │
│  LEAKS:                                                                  │
│  ├─ Causa: goroutine bloqueada para siempre (chan, mutex)               │
│  ├─ Detectar: NumGoroutine(), goleak, pprof                           │
│  ├─ Prevenir: context.Context, done channel, buffered channel          │
│  └─ Regla: toda goroutine debe tener forma de terminar                 │
│                                                                          │
│  INSPECCION:                                                             │
│  ├─ runtime.NumGoroutine() — contar goroutines activas                 │
│  ├─ GODEBUG=schedtrace — ver actividad del scheduler                   │
│  ├─ pprof /debug/pprof/goroutine — stack traces                        │
│  └─ kill -SIGQUIT — dump de todas las goroutines                       │
│                                                                          │
│  vs C: pthreads (costosos, 1:1, kernel managed, ulimit)                │
│  vs Rust: std::thread (1:1) o tokio::spawn (M:N async)                 │
│           Rust: compile-time Send safety vs Go: runtime -race           │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S01 - Goroutines, T02 - Goroutines vs threads — coste (~2KB stack vs ~1MB), crecimiento dinamico de stack, contexto de scheduling
