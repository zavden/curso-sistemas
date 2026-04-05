# Goroutines vs Threads — Coste (~2KB vs ~1MB), Crecimiento Dinamico de Stack, Contexto de Scheduling

## 1. Introduccion

El topico anterior introdujo goroutines y el modelo M:N. Este topico profundiza en la pregunta fundamental: **¿por que goroutines y no threads?** La respuesta no es "goroutines son mejores" — es que son **diferentes herramientas para diferentes escalas**. Un OS thread es la unidad de concurrencia del kernel, pesada y poderosa. Una goroutine es una unidad de concurrencia del runtime de Go, ligera y abundante.

La diferencia no es solo cuantitativa (2KB vs 1MB) sino **cualitativa**: cambia como diseñas software. Con threads que cuestan ~1MB cada uno, limitas la concurrencia a cientos o pocos miles. Con goroutines que cuestan ~2KB, puedes tener una goroutine **por cada conexion, por cada request, por cada tarea** — millones si es necesario. Esto permite patrones de diseño que serian impracticos con threads.

Para SysAdmin/DevOps, entender esta diferencia es critico cuando:
- Evaluas por que Kubernetes (Go) maneja miles de pods concurrentemente sin explotar en memoria
- Diagnosticas por que un servicio Java consume 2GB con 1000 threads mientras el equivalente en Go usa 200MB
- Decides entre Go y otros lenguajes para herramientas de infraestructura
- Configuras `ulimit`, GOMAXPROCS, y connection pools en produccion

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    LA PREGUNTA CENTRAL                                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  "Si los OS threads ya existen y funcionan,                              │
│   ¿por que Go invento goroutines?"                                       │
│                                                                          │
│  RESPUESTA CORTA:                                                        │
│  Porque un servidor HTTP moderno necesita manejar 10,000-1,000,000      │
│  conexiones simultaneas (C10K/C1M problem), y los OS threads no         │
│  escalan a esos numeros.                                                │
│                                                                          │
│  10,000 OS threads = ~10 GB de stack (inaceptable)                      │
│  10,000 goroutines = ~20 MB de stack (trivial)                          │
│                                                                          │
│  Pero la diferencia va mas alla del stack...                            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 2. Anatomia de un OS Thread

### 2.1 Que es un Thread del Sistema Operativo

Un OS thread (tambien llamado kernel thread, native thread, o pthread en Linux) es la unidad basica de scheduling que el kernel del sistema operativo gestiona:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ANATOMIA DE UN OS THREAD (Linux)                       │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Kernel Space (por thread):                                              │
│  ├─ task_struct (~6KB)       ← descriptor del proceso/thread            │
│  │   ├─ estado (RUNNING, SLEEPING, STOPPED, ZOMBIE)                     │
│  │   ├─ prioridad / nice value                                          │
│  │   ├─ CPU affinity mask                                               │
│  │   ├─ senales pendientes                                              │
│  │   ├─ contadores de recursos (CPU time, page faults)                  │
│  │   ├─ file descriptor table                                           │
│  │   └─ puntero al address space (compartido entre threads del proceso) │
│  ├─ kernel stack (~8KB-16KB) ← stack para syscalls                      │
│  └─ thread_info              ← metadata de scheduling                   │
│                                                                          │
│  User Space (por thread):                                                │
│  ├─ stack (~1MB-8MB)         ← stack del usuario (fijo)                 │
│  │   ├─ variables locales                                               │
│  │   ├─ frames de funciones                                             │
│  │   ├─ return addresses                                                │
│  │   └─ guard page (PROT_NONE) ← deteccion de stack overflow           │
│  ├─ TLS (Thread Local Storage) ← variables per-thread                   │
│  └─ registros de CPU (guardados en context switch)                      │
│      ├─ general purpose: RAX, RBX, RCX, ..., R15 (16 regs x86_64)     │
│      ├─ stack pointer (RSP)                                             │
│      ├─ instruction pointer (RIP)                                       │
│      ├─ flags register (RFLAGS)                                         │
│      ├─ FPU/SSE/AVX state (~512 bytes - 2KB con AVX-512)               │
│      └─ segment registers                                               │
│                                                                          │
│  TOTAL POR THREAD: ~1MB (stack) + ~30KB (kernel) + ~2KB (registros)    │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Creacion de un Thread (Linux)

```c
// En Linux, un thread se crea con la syscall clone():
// (pthread_create es un wrapper de libc sobre clone)

// clone(fn, stack, flags, arg)
//
// El kernel debe:
// 1. Allocar task_struct (~6KB) en kernel memory
// 2. Allocar kernel stack (~8-16KB)
// 3. Copiar/inicializar campos del task_struct
// 4. Allocar stack en user space (~1MB, via mmap)
// 5. Configurar guard page (mprotect)
// 6. Agregar al CFS (Completely Fair Scheduler)
// 7. Signal/wake the new thread
//
// Tiempo tipico: 10-50 microsegundos
// Involucra: 1 syscall, varias allocations de kernel, page table updates

// Costo de la syscall clone en Linux (benchmark tipico):
// pthread_create: ~15-30 microsegundos
// Incluye: clone syscall + mmap del stack + inicializacion
```

### 2.3 Context Switch de un Thread

```
┌──────────────────────────────────────────────────────────────────────────┐
│                CONTEXT SWITCH DE UN OS THREAD                            │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  El kernel hace context switch cuando:                                   │
│  - El time slice expira (CFS quantum, ~1-10ms)                          │
│  - El thread hace una syscall bloqueante (read, write, futex)           │
│  - Una interrupcion de mayor prioridad llega                            │
│  - El thread llama sched_yield()                                        │
│                                                                          │
│  Pasos del context switch:                                               │
│  1. GUARDAR registros del thread actual:                                │
│     ├─ 16 registros de proposito general (128 bytes)                    │
│     ├─ FPU/SSE state (~512 bytes)                                       │
│     ├─ AVX state (si aplica, ~2KB)                                      │
│     ├─ Stack pointer, Instruction pointer                               │
│     └─ Flags                                                            │
│                                                                          │
│  2. ACTUALIZAR estado en task_struct                                    │
│                                                                          │
│  3. CAMBIAR page tables (si es otro proceso, no otro thread)            │
│     ├─ Flush TLB (Translation Lookaside Buffer)                        │
│     └─ Esto es MUY costoso — invalida caches de traduccion              │
│                                                                          │
│  4. RESTAURAR registros del nuevo thread                                │
│                                                                          │
│  5. INVALIDAR caches de CPU (L1i, L1d parcialmente)                     │
│     ├─ Los datos del thread anterior ya no son relevantes              │
│     └─ Cache misses = latencia extra por un rato                       │
│                                                                          │
│  Tiempo tipico: 1-10 microsegundos                                      │
│  Con TLB flush (entre procesos): 5-30 microsegundos                     │
│  Impacto en cache: latencia extra por microsegundos-milisegundos        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Anatomia de una Goroutine

### 3.1 Estructura en Memoria

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ANATOMIA DE UNA GOROUTINE                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Runtime (por goroutine):                                                │
│  ├─ g struct (~400 bytes)    ← descriptor de goroutine                  │
│  │   ├─ stack bounds (lo, hi)                                           │
│  │   ├─ stack guard (para stack growth check)                           │
│  │   ├─ status (runnable, running, waiting, dead)                       │
│  │   ├─ sched (gobuf: SP, PC, para context switch)                     │
│  │   ├─ puntero al M actual (nil si no esta running)                   │
│  │   ├─ defer stack                                                     │
│  │   ├─ panic stack                                                     │
│  │   └─ preemption flags                                                │
│  │                                                                       │
│  └─ stack (~2KB inicial)     ← stack del usuario                        │
│      ├─ variables locales                                               │
│      ├─ frames de funciones                                             │
│      └─ NO guard page (el compilador inserta stack checks)              │
│                                                                          │
│  NO TIENE:                                                               │
│  ├─ kernel task_struct (no es visible para el kernel)                   │
│  ├─ kernel stack (no hace syscalls directamente)                        │
│  ├─ TLS (Go usa context.Context en su lugar)                            │
│  ├─ CPU affinity (el scheduler de Go decide)                            │
│  ├─ Signal handlers per-goroutine                                       │
│  └─ FPU/SSE state separado (compartido con el M que la ejecuta)        │
│                                                                          │
│  TOTAL POR GOROUTINE: ~400 bytes (descriptor) + ~2KB (stack inicial)   │
│                      = ~2.4 KB                                           │
│                                                                          │
│  vs OS THREAD: ~1 MB (stack) + ~30 KB (kernel) = ~1030 KB              │
│                                                                          │
│  RATIO: 1 thread ≈ 430 goroutines en memoria                           │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.2 Creacion de una Goroutine

```go
// go f(x, y) se traduce a:
// runtime.newproc(fn, args...)

// El runtime hace:
// 1. Obtener una 'g' del pool de goroutines muertas (reciclaje)
//    O allocar una nueva g struct (~400 bytes)
// 2. Allocar stack (~2KB) del pool de stacks (reciclaje)
//    O allocar nuevo stack con mmap (raro, la mayoria se reciclan)
// 3. Copiar argumentos al stack de la nueva goroutine
// 4. Configurar PC (program counter) para apuntar a f
// 5. Poner la goroutine en la local run queue del P actual
// 6. Retornar (NO syscall, NO kernel involved)
//
// Tiempo tipico: 0.3-1 microsegundos
// NO involucra: syscalls, mmap, page tables, kernel scheduling
// El kernel NO SABE que se creo una goroutine
```

### 3.3 Context Switch de una Goroutine

```
┌──────────────────────────────────────────────────────────────────────────┐
│              CONTEXT SWITCH DE UNA GOROUTINE                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  El scheduler de Go hace context switch cuando:                          │
│  - La goroutine llama a una funcion (check point del compilador)        │
│  - La goroutine hace una operacion de canal (send/receive)              │
│  - La goroutine llama a un lock (sync.Mutex)                            │
│  - La goroutine hace I/O (netpoll o syscall)                            │
│  - runtime.Gosched() — yield explicito                                  │
│  - Preemcion asincrona (senal SIGURG, Go 1.14+)                        │
│                                                                          │
│  Pasos del context switch:                                               │
│  1. GUARDAR en gobuf:                                                   │
│     ├─ Stack Pointer (SP)       ← 1 registro                           │
│     ├─ Program Counter (PC)     ← 1 registro                           │
│     └─ Puntero a la g           ← 1 puntero                            │
│     Total: ~24 bytes (3 palabras de 64 bits)                            │
│                                                                          │
│  2. CAMBIAR el puntero g en el M (thread actual)                        │
│     ├─ M.curg = nueva_g                                                 │
│     └─ nueva_g.m = M                                                    │
│                                                                          │
│  3. RESTAURAR gobuf de la nueva goroutine                               │
│     ├─ Cargar SP → cambiar stack                                        │
│     └─ Cargar PC → saltar a la instruccion donde quedo                 │
│                                                                          │
│  NO HACE:                                                                │
│  ├─ Syscall (todo en user space)                                        │
│  ├─ Guardar/restaurar FPU/SSE (compartido con el M)                    │
│  ├─ Flush TLB (mismo address space)                                     │
│  ├─ Invalidar caches de CPU (mismo address space)                      │
│  └─ Notificar al kernel                                                 │
│                                                                          │
│  Tiempo tipico: 100-200 nanosegundos                                    │
│  (50-100x mas rapido que OS context switch)                             │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Comparacion Cuantitativa Detallada

### 4.1 Tabla de Costes

```
┌──────────────────────────────────────────────────────────────────────────────────┐
│                    GOROUTINE vs OS THREAD — NUMEROS REALES                        │
├──────────────────────┬──────────────────────┬────────────────────────────────────┤
│  METRICA             │  OS Thread (Linux)   │  Goroutine (Go)                    │
├──────────────────────┼──────────────────────┼────────────────────────────────────┤
│  Stack inicial       │  1-8 MB (fijo)       │  2 KB (dinamico, crece hasta 1GB) │
│  Stack growth        │  No crece (overflow  │  Automatico por copia             │
│  (cuando se llena)   │  = SIGSEGV = crash)  │  2KB → 4KB → 8KB → ...           │
│  Descriptor          │  ~6 KB (task_struct) │  ~400 bytes (g struct)            │
│  Kernel stack        │  8-16 KB             │  0 (no visible al kernel)         │
│  Creacion            │  10-50 µs            │  0.3-1 µs                         │
│  Context switch      │  1-10 µs             │  0.1-0.2 µs (100-200 ns)         │
│  TLB flush           │  Si (entre procesos) │  No (mismo address space)         │
│  Cache invalidation  │  Parcial             │  Minima (mismo address space)     │
│  Syscall requerida   │  Si (clone)          │  No (user space)                  │
│  Kernel scheduling   │  Si (CFS)            │  No (Go runtime scheduler)        │
│  Max practico/proceso│  ~4,000-32,000       │  ~1,000,000+                      │
│  Limite              │  ulimit -u           │  Solo memoria                     │
│  Reciclaje           │  No (crear/destruir) │  Si (pool de g's y stacks)        │
│  Memoria 10K units   │  ~10 GB              │  ~20 MB                           │
│  Memoria 1M units    │  IMPOSIBLE           │  ~2 GB                            │
├──────────────────────┼──────────────────────┼────────────────────────────────────┤
│  RESUMEN             │  Pesado, poderoso,   │  Ligero, abundante,               │
│                      │  kernel-managed      │  runtime-managed                  │
└──────────────────────┴──────────────────────┴────────────────────────────────────┘
```

### 4.2 Benchmark de Creacion

```go
package main

import (
    "fmt"
    "runtime"
    "sync"
    "time"
)

func benchmarkGoroutineCreation(n int) time.Duration {
    var wg sync.WaitGroup
    wg.Add(n)

    start := time.Now()
    for i := 0; i < n; i++ {
        go func() {
            wg.Done()
        }()
    }
    wg.Wait()
    return time.Since(start)
}

func main() {
    // Warmup
    benchmarkGoroutineCreation(1000)

    counts := []int{1_000, 10_000, 100_000, 1_000_000}

    fmt.Println("Goroutine Creation Benchmark")
    fmt.Println("────────────────────────────")

    for _, n := range counts {
        runtime.GC() // limpiar antes de cada test

        var totalDuration time.Duration
        iterations := 5
        for i := 0; i < iterations; i++ {
            totalDuration += benchmarkGoroutineCreation(n)
        }
        avg := totalDuration / time.Duration(iterations)
        perGoroutine := avg / time.Duration(n)

        fmt.Printf("  %8d goroutines: %10v total, %6v per goroutine\n",
            n, avg.Truncate(time.Microsecond), perGoroutine)
    }

    // Output tipico (hardware moderno):
    //     1,000 goroutines:     250µs total,   250ns per goroutine
    //    10,000 goroutines:   2,500µs total,   250ns per goroutine
    //   100,000 goroutines:  25,000µs total,   250ns per goroutine
    // 1,000,000 goroutines: 280,000µs total,   280ns per goroutine
}
```

### 4.3 Benchmark de Memoria

```go
func benchmarkMemory(n int) {
    var m1 runtime.MemStats
    runtime.GC()
    runtime.ReadMemStats(&m1)

    var wg sync.WaitGroup
    wg.Add(n)
    done := make(chan struct{})

    for i := 0; i < n; i++ {
        go func() {
            wg.Done()
            <-done // mantener la goroutine viva
        }()
    }
    wg.Wait() // esperar a que todas se creen

    var m2 runtime.MemStats
    runtime.ReadMemStats(&m2)

    totalMB := float64(m2.Sys-m1.Sys) / (1024 * 1024)
    perGoroutineKB := float64(m2.Sys-m1.Sys) / float64(n) / 1024

    fmt.Printf("  %8d goroutines: %.1f MB total, %.2f KB per goroutine\n",
        n, totalMB, perGoroutineKB)

    close(done) // liberar todas las goroutines
}

// Output tipico:
//     1,000 goroutines:   2.5 MB total, 2.56 KB per goroutine
//    10,000 goroutines:  25.0 MB total, 2.56 KB per goroutine
//   100,000 goroutines: 280.0 MB total, 2.87 KB per goroutine
// 1,000,000 goroutines: 2.8 GB total, 2.94 KB per goroutine

// vs OS threads (estimacion):
//     1,000 threads:  ~1,000 MB (1 GB)
//    10,000 threads: ~10,000 MB (10 GB) — probablemente OOM
//   100,000 threads: IMPOSIBLE en la mayoria de sistemas
```

### 4.4 Benchmark de Context Switch

```go
func benchmarkContextSwitch(n int) time.Duration {
    ch := make(chan struct{})

    go func() {
        for i := 0; i < n; i++ {
            ch <- struct{}{} // send → context switch
        }
    }()

    start := time.Now()
    for i := 0; i < n; i++ {
        <-ch // receive → context switch
    }
    return time.Since(start)
}

func main() {
    n := 1_000_000
    d := benchmarkContextSwitch(n)
    fmt.Printf("Goroutine context switch: %v per switch\n",
        d/time.Duration(n))

    // Output tipico:
    // Goroutine context switch: ~150-300ns per switch
    // vs OS thread context switch: ~1-10µs per switch
    // → goroutines son ~10-50x mas rapidas en context switch
}
```

---

## 5. Crecimiento Dinamico del Stack

### 5.1 El Problema del Stack Fijo

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    STACK FIJO vs STACK DINAMICO                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  OS THREAD (stack fijo):                                                 │
│                                                                          │
│  Thread A:           Thread B:           Thread C:                       │
│  ┌────────────┐     ┌────────────┐     ┌────────────┐                  │
│  │ guard page │     │ guard page │     │ guard page │                   │
│  ├────────────┤     ├────────────┤     ├────────────┤                   │
│  │            │     │            │     │            │                    │
│  │            │     │            │     │  STACK     │                    │
│  │  1 MB      │     │  1 MB      │     │  OVERFLOW! │ ← SIGSEGV        │
│  │  reservado │     │  reservado │     │  (crash)   │                    │
│  │            │     │            │     │            │                    │
│  │            │     │  usa 50KB  │     │  usa 1.1MB │                   │
│  │  usa 4KB   │     │            │     │            │                    │
│  └────────────┘     └────────────┘     └────────────┘                   │
│   desperdicio:       desperdicio:       necesitaba mas                   │
│   996 KB             950 KB             pero → crash                     │
│                                                                          │
│  PROBLEMA: reservar poco → crash. Reservar mucho → desperdicio.         │
│  No hay opcion correcta — es un compromiso con stacks fijos.            │
│                                                                          │
│                                                                          │
│  GOROUTINE (stack dinamico):                                             │
│                                                                          │
│  Goroutine A:   Goroutine B:     Goroutine C:                           │
│  ┌──────┐       ┌──────────┐     ┌──────────────────────┐              │
│  │ 2KB  │       │  8KB     │     │      64KB            │              │
│  │ usa  │       │  usa     │     │      usa             │              │
│  │ 1.5KB│       │  6KB     │     │      50KB            │              │
│  └──────┘       └──────────┘     └──────────────────────┘              │
│   overhead:      overhead:        overhead:                              │
│   0.5 KB         2 KB             14 KB                                  │
│                                                                          │
│  Cada goroutine usa SOLO lo que necesita.                               │
│  Si necesita mas → el runtime crece el stack automaticamente.           │
│  Si una goroutine profundamente recursiva necesita 10MB → lo obtiene.  │
│  Sin crash, sin desperdicio.                                            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Mecanismo de Crecimiento: Stack Copying

Go usa **stack copying** (tambien llamado "contiguous stacks", implementado en Go 1.4):

```go
// En cada llamada a funcion, el compilador inserta un PROLOGO:
//
// func f() {
//     // --- generado por el compilador ---
//     if SP < stackguard {     // ¿queda espacio?
//         runtime.morestack()  // NO → crecer el stack
//     }
//     // --- tu codigo ---
// }

// runtime.morestack() hace:
// 1. Allocar un nuevo stack del DOBLE de tamano
//    (2KB → 4KB → 8KB → 16KB → ...)
// 2. COPIAR todo el contenido del stack viejo al nuevo
// 3. ACTUALIZAR todos los punteros que apuntan al stack viejo
//    (esto es posible porque Go conoce el layout del stack)
// 4. Liberar el stack viejo
// 5. Continuar la ejecucion en el nuevo stack
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    STACK COPY EN ACCION                                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  ANTES (stack de 2KB, casi lleno):                                      │
│  ┌──────────────────┐ ← stack.hi (0x1000)                              │
│  │ frame: main()    │                                                   │
│  │ frame: handler() │                                                   │
│  │ frame: process() │                                                   │
│  │ frame: parse()   │                                                   │
│  │ frame: decode()  │ ← SP (casi en stack.lo)                          │
│  │ [guard zone]     │ ← stackguard0                                     │
│  └──────────────────┘ ← stack.lo (0x0800)                              │
│                                                                          │
│  decode() llama a validate() → necesita mas stack                       │
│  SP < stackguard0 → runtime.morestack() triggered                      │
│                                                                          │
│  DESPUES (nuevo stack de 4KB):                                          │
│  ┌──────────────────────────────────────┐ ← stack.hi (0x3000)          │
│  │ (espacio libre — 2KB disponibles)    │                               │
│  │                                      │                               │
│  │ frame: main()    [punteros ajustados]│                               │
│  │ frame: handler() [punteros ajustados]│                               │
│  │ frame: process() [punteros ajustados]│                               │
│  │ frame: parse()   [punteros ajustados]│                               │
│  │ frame: decode()  [punteros ajustados]│ ← SP (ahora hay espacio)     │
│  │ [guard zone]                         │ ← stackguard0 (nuevo)         │
│  └──────────────────────────────────────┘ ← stack.lo (0x2000)          │
│                                                                          │
│  El stack viejo (0x0800-0x1000) se libera.                              │
│  TODOS los punteros que apuntaban al rango viejo se ajustan            │
│  al rango nuevo. Esto incluye:                                          │
│  - Punteros en variables locales                                        │
│  - Punteros en parametros de funciones                                  │
│  - Punteros en el defer stack                                           │
│                                                                          │
│  RESTRICCION IMPORTANTE:                                                │
│  No puedes tener un puntero EXTERNO (desde el heap) que apunte         │
│  directamente al stack de una goroutine — porque el stack puede         │
│  moverse. Go garantiza esto: si una variable "escapa" al heap           │
│  (escape analysis), se alloca en el heap, no en el stack.              │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.3 Stack Shrinking

Go tambien **reduce** el tamano del stack cuando detecta que se uso mucho menos de la mitad durante un GC:

```go
// Durante garbage collection, si un stack esta usando menos de 1/4:
// stack actual: 64KB, usado: 10KB → shrink a 32KB
//
// Esto previene que goroutines que tuvieron un pico temporal
// de uso de stack mantengan un stack grande permanentemente.

// El shrinking sigue la misma mecanica: allocar nuevo stack mas pequeno,
// copiar, actualizar punteros, liberar el viejo.
```

### 5.4 Escape Analysis y Stack vs Heap

```go
// El compilador de Go decide si una variable va al stack o al heap
// usando "escape analysis":

func noEscape() int {
    x := 42        // x en el STACK (no escapa de la funcion)
    return x        // se copia el valor, no el puntero
}

func escapes() *int {
    x := 42        // x en el HEAP (escapa via puntero)
    return &x       // &x se retorna → x debe sobrevivir a la funcion
}

// Ver escape analysis:
// go build -gcflags="-m" main.go
//   ./main.go:5:2: x does not escape
//   ./main.go:10:2: moved to heap: x

// ¿Por que importa?
// Variables en el STACK son GRATIS (se limpian al retornar la funcion)
// Variables en el HEAP necesitan GC (mas lento, mas presion de GC)
// El stack de goroutines se copia → punteros al stack se actualizan
// Punteros al heap NO se ven afectados por stack copy
```

### 5.5 Por Que C y Rust No Pueden Copiar Stacks

```c
// C no puede copiar stacks porque:
// 1. Raw pointers al stack son comunes y el compilador no los trackea
int x = 42;
int *p = &x;           // p apunta al stack
some_library_call(p);  // ¿la libreria guardo p internamente?
// Si copiamos el stack, p apunta a MEMORIA INVALIDA
// No hay forma de encontrar y actualizar TODOS los punteros al stack

// 2. Stack addresses se pasan a syscalls y callbacks
// El kernel tiene punteros al stack del thread en estructuras internas
```

```rust
// Rust no puede copiar stacks porque:
// 1. References (&T, &mut T) con lifetimes apuntan al stack
fn example() {
    let x = 42;
    let r: &i32 = &x;  // r apunta al stack
    // Si movieramos el stack, r seria dangling
    // Rust GARANTIZA que r es valido — mover el stack romperia esa garantia
}

// 2. Pin<T> — ciertos tipos PROMETEN no moverse
//    (self-referential structs, async futures)
//    Copiar el stack moveria estos tipos → violacion de Pin

// 3. FFI — codigo C llamado desde Rust tiene punteros al stack
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│         POR QUE SOLO GO PUEDE COPIAR STACKS                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  GO:                                                                     │
│  ├─ GC conoce TODOS los punteros al stack (type info en runtime)        │
│  ├─ No hay raw pointers (unsafe.Pointer es raro y controlado)           │
│  ├─ Escape analysis mueve al heap lo que podria causar problemas        │
│  ├─ No hay FFI casual (cgo es explicito y tiene restricciones)          │
│  └─ → PUEDE copiar stacks y actualizar todos los punteros              │
│                                                                          │
│  C:                                                                      │
│  ├─ Raw pointers everywhere — imposible encontrarlos todos              │
│  └─ → Stack fijo, guard page para overflow detection                    │
│                                                                          │
│  RUST:                                                                   │
│  ├─ References con lifetime tracking — pero Pin y FFI lo impiden        │
│  ├─ tokio async: futures son state machines en el heap (no necesitan    │
│  │   stack copy — la "stack" del future ES el future en el heap)        │
│  └─ → std::thread: stack fijo. tokio: sin stack (stackless coroutines) │
│                                                                          │
│  NOTA: Rust async (tokio) resolvio el problema de forma DIFERENTE:      │
│  En lugar de goroutines con stack propio, usa "stackless coroutines"    │
│  donde el compilador transforma async fn en state machines que          │
│  viven en el heap. No necesitan stack copy porque no tienen stack.     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Scheduling: Kernel vs Go Runtime

### 6.1 Kernel Scheduler (CFS en Linux)

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CFS (Completely Fair Scheduler)                        │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Estructura: Red-Black Tree ordenado por "virtual runtime" (vruntime)   │
│                                                                          │
│  ┌─────────────┐                                                        │
│  │   RB-Tree   │                                                        │
│  │             │                                                         │
│  │    (15ms)   │ ← nodo con menor vruntime = siguiente a ejecutar      │
│  │   /    \    │                                                         │
│  │ (16ms) (20ms)│                                                        │
│  │  / \    / \  │                                                        │
│  │(17)(18)(21)(25)                                                       │
│  └─────────────┘                                                        │
│                                                                          │
│  Operaciones:                                                            │
│  - Seleccionar siguiente: O(1) — leftmost node                         │
│  - Insertar/remover: O(log N)                                           │
│  - Preemcion: cada ~1-10ms (sysctl_sched_latency)                      │
│                                                                          │
│  Caracteristicas:                                                        │
│  ├─ Time-sharing: cada thread recibe una fraccion justa de CPU          │
│  ├─ Prioridades: nice value (-20 a +19) afecta la fraccion             │
│  ├─ Grupos de scheduling (cgroups): aislamiento por container           │
│  ├─ PREEMPTIVO: el kernel interrumpe al thread cuando expira su slice  │
│  └─ Overhead: syscall entry/exit + context switch + TLB flush           │
│                                                                          │
│  El kernel NO SABE que existen goroutines — solo ve M's (OS threads)   │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 6.2 Go Scheduler

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    GO SCHEDULER (GMP)                                     │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Estructura: run queues (local por P + global)                          │
│                                                                          │
│  Global Queue: [G15, G16, G17]                                          │
│                                                                          │
│  P0 Local Queue:    P1 Local Queue:    P2 Local Queue:                  │
│  [G1, G2, G3]       [G4, G5]           [G6]                             │
│  Running: G0        Running: G10       Running: G11                     │
│                                                                          │
│  Operaciones:                                                            │
│  - Seleccionar siguiente: O(1) — dequeue de local queue                │
│  - Work stealing: O(1) — robar mitad de otro P                         │
│  - Insertar: O(1) — enqueue en local queue                             │
│  - No hay tree balancing, no hay prioridades complejas                 │
│                                                                          │
│  Caracteristicas:                                                        │
│  ├─ COOPERATIVO (con preemcion asistida):                               │
│  │   La goroutine cede control en function calls, chan ops, etc.        │
│  │   Go 1.14+: senal SIGURG para preemptar loops sin yields            │
│  ├─ Sin prioridades: todas las goroutines son iguales                  │
│  │   (no hay nice value ni real-time scheduling para goroutines)        │
│  ├─ Sin time slicing fijo: ejecuta hasta un schedule point             │
│  ├─ Work stealing: balanceo automatico entre P's                       │
│  ├─ Handoff: syscall bloqueante → P se mueve a otro M                  │
│  └─ Netpoll: I/O de red sin bloquear M's                              │
│                                                                          │
│  TODO en user space — SIN syscalls para scheduling                     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 6.3 Comparacion de Schedulers

```
┌──────────────────────────────────────────────────────────────────────────┐
│              KERNEL SCHEDULER vs GO SCHEDULER                            │
├────────────────────┬─────────────────────┬───────────────────────────────┤
│  CARACTERISTICA    │  Kernel (CFS)       │  Go Runtime (GMP)             │
├────────────────────┼─────────────────────┼───────────────────────────────┤
│  Unidad            │  OS thread          │  Goroutine                    │
│  Estructura        │  Red-Black Tree     │  FIFO queues (local + global)│
│  Complejidad select│  O(1) leftmost      │  O(1) dequeue                │
│  Preemcion         │  Timer interrupt    │  Cooperative + signal (1.14+)│
│  Time slice        │  ~1-10ms            │  Hasta proximo schedule point│
│  Prioridades       │  nice (-20 to +19)  │  Ninguna (todas iguales)     │
│  Context switch    │  1-10 µs            │  100-200 ns                  │
│  Registros salvados│  ~16 + FPU/SSE     │  3 (SP, PC, g pointer)       │
│  TLB impact        │  Flush (costoso)    │  Ninguno (mismo addr space)  │
│  Cache impact      │  Alto               │  Bajo                        │
│  Syscall needed    │  Si                 │  No                          │
│  Load balancing    │  Migration + CFS    │  Work stealing               │
│  I/O handling      │  Block thread       │  Netpoll / handoff           │
│  Escalabilidad     │  ~miles threads     │  ~millones goroutines        │
│  Overhead por unit │  ~30KB kernel       │  ~400 bytes runtime          │
│  Fairness          │  Garantizada (CFS)  │  Best-effort                 │
│  Real-time         │  SCHED_FIFO/RR      │  No soportado                │
│  Determinismo      │  Configurable       │  No deterministico           │
├────────────────────┼─────────────────────┼───────────────────────────────┤
│  IDEAL PARA        │  Tareas CPU-bound   │  Tareas I/O-bound con alta  │
│                    │  con prioridades    │  concurrencia                │
└────────────────────┴─────────────────────┴───────────────────────────────┘
```

---

## 7. Cuando los OS Threads Siguen Siendo Necesarios

### 7.1 Casos Donde Goroutines NO Bastan

```go
// 1. SYSCALLS BLOQUEANTES MASIVAS
// Si 1000 goroutines leen archivos de disco simultaneamente,
// Go crea hasta 1000 OS threads (uno por syscall bloqueante).
// El modelo M:N degenera a 1:1 en este caso.

// 2. CGO (llamadas a C)
// cgo bloquea el OS thread — no se puede hacer handoff
// porque el codigo C puede tener thread-local state
import "C"
func cgoExample() {
    C.expensive_c_function() // bloquea el M, P hace handoff
    // pero si MUCHAS goroutines llaman a C, creas muchos threads
}

// 3. CPU-BOUND PURO
// runtime.LockOSThread() fija la goroutine a un thread.
// Necesario para:
// - OpenGL (requiere thread-specific context)
// - Algunas APIs de C que no son thread-safe
// - Llamadas a runtime de lenguajes que usan TLS

func init() {
    // Fijar la goroutine main a un OS thread
    // Necesario para GUI frameworks (GTK, SDL, OpenGL)
    runtime.LockOSThread()
}

// 4. REAL-TIME SCHEDULING
// Go no soporta SCHED_FIFO/SCHED_RR para goroutines
// Si necesitas latencia garantizada < 1ms, necesitas threads
// con real-time priorities (sched_setscheduler)

// 5. CPU AFFINITY
// No puedes fijar una goroutine a un CPU core especifico
// Para workloads NUMA-aware, necesitas control manual de threads
```

### 7.2 runtime.LockOSThread

```go
import "runtime"

func renderLoop() {
    // Fijar esta goroutine a su OS thread actual
    runtime.LockOSThread()
    defer runtime.UnlockOSThread()

    // Ahora esta goroutine SIEMPRE se ejecuta en este thread
    // El scheduler de Go no la movera a otro M
    // Necesario para:
    // - OpenGL context (es per-thread)
    // - Windows COM (single-threaded apartment)
    // - Cualquier API que use TLS

    initOpenGL()
    for !shouldQuit {
        renderFrame()
    }
}

// CUIDADO: LockOSThread reduce la eficiencia del scheduler
// porque "desperdicia" un M para una sola goroutine.
// Usarlo solo cuando sea realmente necesario.
```

### 7.3 GOMAXPROCS vs CPU Cores

```go
// GOMAXPROCS controla cuantos P's (procesadores logicos) hay.
// Cada P ejecuta goroutines en un M (OS thread).
// GOMAXPROCS = numero de goroutines que REALMENTE ejecutan en paralelo.

// GOMAXPROCS = 1:
// Todas las goroutines se turnan en un solo thread.
// Hay concurrencia (interleaving) pero NO paralelismo.
// Util para debugging de race conditions.

// GOMAXPROCS = NumCPU() (default):
// Una goroutine activa por core.
// Maximo paralelismo real para CPU-bound.

// GOMAXPROCS > NumCPU():
// Mas P's que cores → context switches de OS entre M's.
// Para I/O-bound puede ayudar ligeramente.
// Para CPU-bound no ayuda (más overhead, no más throughput).

func demonstrateGOMAXPROCS() {
    // CPU-bound work con diferentes GOMAXPROCS
    work := func() {
        sum := 0
        for i := 0; i < 100_000_000; i++ {
            sum += i
        }
    }

    for _, procs := range []int{1, 2, 4, 8} {
        runtime.GOMAXPROCS(procs)
        start := time.Now()

        var wg sync.WaitGroup
        for i := 0; i < 8; i++ {
            wg.Add(1)
            go func() {
                defer wg.Done()
                work()
            }()
        }
        wg.Wait()

        fmt.Printf("  GOMAXPROCS=%d: %v\n", procs, time.Since(start))
    }
    // Output tipico (8-core machine):
    // GOMAXPROCS=1: 800ms  (secuencial)
    // GOMAXPROCS=2: 400ms  (2x speedup)
    // GOMAXPROCS=4: 200ms  (4x speedup)
    // GOMAXPROCS=8: 100ms  (8x speedup, lineal)
}
```

---

## 8. Comparacion con C y Rust

### 8.1 C — pthreads

```c
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/resource.h>

// Benchmarks comparativos en C:

// 1. Coste de creacion de thread
void *noop(void *arg) { return NULL; }

void bench_thread_creation(int n) {
    struct timespec start, end;
    pthread_t *threads = malloc(n * sizeof(pthread_t));

    clock_gettime(CLOCK_MONOTONIC, &start);
    for (int i = 0; i < n; i++) {
        pthread_create(&threads[i], NULL, noop, NULL);
    }
    for (int i = 0; i < n; i++) {
        pthread_join(threads[i], NULL);
    }
    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = (end.tv_sec - start.tv_sec) +
                    (end.tv_nsec - start.tv_nsec) / 1e9;
    printf("%d threads: %.3f seconds (%.0f µs per thread)\n",
           n, elapsed, elapsed / n * 1e6);

    free(threads);
}

// 2. Reducir stack size para tener mas threads
void create_many_threads(int n) {
    pthread_attr_t attr;
    pthread_attr_init(&attr);

    // Reducir stack a 64KB (minimo practico)
    pthread_attr_setstacksize(&attr, 64 * 1024);

    pthread_t *threads = malloc(n * sizeof(pthread_t));
    int created = 0;

    for (int i = 0; i < n; i++) {
        int ret = pthread_create(&threads[i], &attr, noop, NULL);
        if (ret != 0) {
            printf("Failed at thread %d: %s\n", i, strerror(ret));
            break;
        }
        created++;
    }
    printf("Created %d threads with 64KB stack\n", created);

    for (int i = 0; i < created; i++) {
        pthread_join(threads[i], NULL);
    }

    free(threads);
    pthread_attr_destroy(&attr);
}

// Output tipico:
//   1,000 threads: 0.025s (25 µs per thread)
//  10,000 threads: 0.350s (35 µs per thread)
// 100,000 threads: FAIL — EAGAIN (resource limit)
//
// Con 64KB stack:
//  50,000 threads creados (antes de ulimit)
//
// vs Go: 1,000,000 goroutines en ~0.3 segundos

/*
 * RESUMEN C vs Go para concurrencia:
 *
 * ┌──────────────┬──────────────────┬─────────────────────────┐
 * │              │ C (pthreads)     │ Go (goroutines)         │
 * ├──────────────┼──────────────────┼─────────────────────────┤
 * │ Create cost  │ 15-35 µs         │ 0.3-1 µs (30-100x)     │
 * │ Memory/unit  │ 64KB-1MB         │ 2KB (32-500x)           │
 * │ Max units    │ ~50K (tuned)     │ ~1M+ (default)          │
 * │ Stack growth │ No (crash)       │ Automatic               │
 * │ Scheduling   │ Kernel (CFS)     │ Runtime (GMP)           │
 * │ Switch cost  │ 1-10 µs          │ 100-200 ns (10-50x)     │
 * │ Network I/O  │ Block or epoll   │ Netpoll (transparent)   │
 * │ Complexity   │ High (manual)    │ Low (go keyword)        │
 * └──────────────┴──────────────────┴─────────────────────────┘
 */
```

### 8.2 Rust — std::thread vs tokio

```rust
use std::thread;
use std::time::{Duration, Instant};
use tokio::task;

// ─── std::thread (1:1, como pthreads) ───

fn bench_std_threads(n: usize) -> Duration {
    let start = Instant::now();
    let handles: Vec<_> = (0..n)
        .map(|_| thread::spawn(|| {}))
        .collect();
    for h in handles {
        h.join().unwrap();
    }
    start.elapsed()
}

// std::thread:
// - Stack default: 2MB (configurable con thread::Builder)
// - Overhead: igual que pthreads (~15-35µs create)
// - Max: ~miles (mismo limite que pthreads)
// - Scheduling: kernel (1:1)
// - Beneficio: compile-time Send/Sync safety

fn custom_stack_threads(n: usize) {
    let handles: Vec<_> = (0..n)
        .map(|_| {
            thread::Builder::new()
                .stack_size(64 * 1024)  // 64KB stack
                .spawn(|| {})
                .unwrap()
        })
        .collect();
    for h in handles {
        h.join().unwrap();
    }
}


// ─── tokio (M:N, como goroutines) ───

#[tokio::main]
async fn main() {
    // tokio::spawn es lo mas parecido a go func()
    let start = Instant::now();
    let mut handles = Vec::new();

    for _ in 0..1_000_000 {
        handles.push(task::spawn(async {}));
    }

    for h in handles {
        h.await.unwrap();
    }

    println!("1M tasks: {:?}", start.elapsed());
    // Output tipico: 1M tasks: ~200-500ms
    // Comparable a goroutines
}

// PERO: tokio tasks son FUNDAMENTALMENTE diferentes a goroutines:
//
// Goroutine: tiene su propio STACK (2KB, crece)
//            - Puede usar funciones bloqueantes normales
//            - El scheduler maneja todo transparentemente
//
// Tokio task: NO tiene stack (stackless coroutine)
//            - Es una STATE MACHINE generada por el compilador
//            - Solo puede llamar a funciones async (con .await)
//            - Funciones bloqueantes requieren task::spawn_blocking
//            - Menor overhead de memoria (no hay stack allocation)
//            - Pero restringe QUE puedes hacer dentro del task

/*
 * COMPARACION COMPLETA: Go goroutine vs Rust std::thread vs Rust tokio
 *
 * ┌──────────────────┬──────────────────┬──────────────────┬─────────────────┐
 * │                  │ Go goroutine     │ Rust std::thread │ Rust tokio task │
 * ├──────────────────┼──────────────────┼──────────────────┼─────────────────┤
 * │ Modelo           │ M:N (stackful)   │ 1:1 (native)     │ M:N (stackless) │
 * │ Stack            │ 2KB → 1GB        │ 2MB fijo         │ No tiene stack  │
 * │ Memory/unit      │ ~2.5 KB          │ ~2 MB            │ ~pocos bytes    │
 * │ Create cost      │ ~0.5 µs          │ ~20 µs           │ ~0.3 µs         │
 * │ Context switch   │ ~150 ns          │ ~3 µs            │ ~50 ns          │
 * │ Max units        │ ~1M              │ ~5K              │ ~10M            │
 * │ Stack growth     │ Automatic copy   │ No               │ N/A             │
 * │ Blocking I/O     │ Transparent      │ Blocks thread    │ spawn_blocking  │
 * │ Network I/O      │ Netpoll (auto)   │ Block/manual     │ Built-in async  │
 * │ Preemption       │ Cooperative+sig  │ Kernel preempt   │ Cooperative only│
 * │ Cancellation     │ context.Context  │ Drop + flag      │ Drop JoinHandle │
 * │ Data race detect │ Runtime (-race)  │ Compile (Send)   │ Compile (Send)  │
 * │ Learning curve   │ Low              │ Medium           │ High            │
 * │ Abstraction leak │ Minimal          │ Minimal          │ async coloring  │
 * └──────────────────┴──────────────────┴──────────────────┴─────────────────┘
 *
 * "async coloring" (Rust): si una funcion es async, TODOS los callers
 * deben ser async (o usar block_on). Go no tiene este problema —
 * TODA funcion puede ser llamada desde una goroutine sin cambios.
 * Esto se llama "function coloring problem" y es una de las ventajas
 * principales de goroutines sobre async/await.
 */
```

### 8.3 El Problema del "Function Coloring"

```
┌──────────────────────────────────────────────────────────────────────────┐
│              FUNCTION COLORING: Go vs Rust vs JavaScript                 │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  En Rust/JS/Python/C#, las funciones tienen "color":                    │
│  - Funciones normales (sync)                                            │
│  - Funciones async                                                      │
│  Una funcion async solo puede llamar a otra async con .await            │
│  Una funcion sync NO puede llamar a async directamente                 │
│                                                                          │
│  RUST:                                                                   │
│  fn sync_func() { ... }                                                 │
│  async fn async_func() { ... }                                          │
│                                                                          │
│  // ⛔ No puedes llamar async desde sync:                                │
│  fn sync_caller() {                                                     │
│      async_func().await;  // ERROR: .await solo en async fn             │
│  }                                                                       │
│                                                                          │
│  // ✅ Debes hacer al caller async tambien:                              │
│  async fn async_caller() {                                              │
│      async_func().await;  // OK                                         │
│  }                                                                       │
│  // → "infeccion async" sube por todo el call stack                    │
│                                                                          │
│                                                                          │
│  GO: NO HAY COLORING                                                    │
│  func anyFunc() { ... }                                                 │
│                                                                          │
│  // ✅ Cualquier funcion puede correr en una goroutine:                  │
│  go anyFunc()  // no importa si hace I/O, CPU, lo que sea              │
│                                                                          │
│  // ✅ Cualquier funcion puede llamar a cualquier otra:                  │
│  func caller() {                                                        │
│      anyFunc()     // funciona                                          │
│      otherFunc()   // funciona                                          │
│      ioFunc()      // funciona — el runtime maneja transparentemente    │
│  }                                                                       │
│                                                                          │
│  VENTAJA DE GO:                                                         │
│  No tienes que decidir si una funcion es "sync" o "async"              │
│  No hay "infeccion" de async subiendo por el call stack                │
│  Cualquier libreria funciona en cualquier contexto sin cambios          │
│                                                                          │
│  VENTAJA DE RUST:                                                       │
│  El compilador SABE que es async y optimiza mejor                       │
│  Zero-cost abstraction (no hay overhead de runtime scheduler)           │
│  State machines mas eficientes que goroutine stacks para muchos tasks   │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 9. Escenarios Reales: Goroutines en Produccion

### 9.1 HTTP Server: 10K Conexiones

```go
// Un servidor HTTP en Go crea UNA goroutine por conexion.
// net/http lo hace automaticamente:

func main() {
    http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
        // Esta funcion se ejecuta en su propia goroutine
        // 10,000 requests simultaneos = 10,000 goroutines
        // Memoria: ~25 MB de stacks (vs ~10 GB con threads)
        time.Sleep(100 * time.Millisecond) // simular trabajo
        fmt.Fprintf(w, "Hello from goroutine")
    })
    http.ListenAndServe(":8080", nil)
}

// Internamente, net/http hace:
// for {
//     conn, err := listener.Accept()
//     if err != nil { ... }
//     go srv.serve(conn)  // ← goroutine por conexion
// }
//
// Esto es posible PORQUE goroutines son baratas.
// En Java/C, necesitarias un thread pool con limite
// y aceptar conexiones solo cuando hay threads disponibles.
```

### 9.2 Kubernetes: Controller Pattern

```go
// Los controllers de Kubernetes usan goroutines extensivamente:
// - 1 goroutine por informer (watcher de recursos)
// - 1 goroutine por worker del workqueue
// - Goroutines para health checks, leader election, metrics

// Tipicamente un controller tiene:
// ~5 informers × 1 goroutine = 5 goroutines watching
// ~10 workers procesando events = 10 goroutines
// + health check, pprof, metrics = ~5 goroutines
// Total: ~20 goroutines por controller
// Un controller manager con 30 controllers: ~600 goroutines
// Memoria: trivial (~1.5 MB de stacks)

// Si fuera threads: 600 threads × 1MB = 600 MB solo de stacks
// Con goroutines: 600 goroutines × 2.5KB = 1.5 MB
```

### 9.3 Tabla de Escala

```
┌──────────────────────────────────────────────────────────────────────────┐
│                ESCALA EN PRODUCCION                                       │
├────────────────────────┬────────────────┬────────────────────────────────┤
│  APLICACION            │  GOROUTINES    │  EQUIVALENTE EN THREADS        │
├────────────────────────┼────────────────┼────────────────────────────────┤
│  CLI simple            │  1-5           │  Igual (no importa)            │
│  HTTP server basico    │  ~100          │  Similar (thread pool)         │
│  API con DB/cache      │  ~1,000        │  Posible pero costoso (~1GB)   │
│  Proxy/load balancer   │  ~10,000       │  Dificil (~10GB de stacks)     │
│  Chat server           │  ~100,000      │  Impractico                    │
│  Kubernetes controller │  ~500          │  Posible pero pesado           │
│  Kubernetes API server │  ~50,000       │  Imposible con threads nativos │
│  CDN edge server       │  ~1,000,000    │  Absolutamente imposible       │
├────────────────────────┼────────────────┼────────────────────────────────┤
│  El "sweet spot" de goroutines es 1,000 - 1,000,000                     │
│  Por debajo de 100: threads funcionan igual                             │
│  Por encima de 1M: necesitas disenar diferente (event loop, io_uring)  │
└────────────────────────┴────────────────┴────────────────────────────────┘
```

---

## 10. Programa Completo: Goroutine vs Thread Benchmark Suite

```go
package main

import (
    "fmt"
    "math"
    "runtime"
    "sync"
    "time"
)

// ─────────────────────────────────────────────────────────────────
// Benchmark helpers
// ─────────────────────────────────────────────────────────────────

type BenchResult struct {
    Name     string
    N        int
    Duration time.Duration
    PerUnit  time.Duration
    MemoryMB float64
}

func (r BenchResult) String() string {
    return fmt.Sprintf("  %-30s n=%-8d total=%-10v per_unit=%-8v mem=%.1f MB",
        r.Name, r.N, r.Duration.Truncate(time.Microsecond),
        r.PerUnit, r.MemoryMB)
}

func measureMemory() uint64 {
    runtime.GC()
    var m runtime.MemStats
    runtime.ReadMemStats(&m)
    return m.Sys
}

// ─────────────────────────────────────────────────────────────────
// Benchmark 1: Creation cost
// ─────────────────────────────────────────────────────────────────

func benchCreation(n int) BenchResult {
    var wg sync.WaitGroup
    wg.Add(n)

    memBefore := measureMemory()
    start := time.Now()

    for i := 0; i < n; i++ {
        go func() {
            wg.Done()
        }()
    }
    wg.Wait()

    elapsed := time.Since(start)
    memAfter := measureMemory()

    return BenchResult{
        Name:     "goroutine_creation",
        N:        n,
        Duration: elapsed,
        PerUnit:  elapsed / time.Duration(n),
        MemoryMB: float64(memAfter-memBefore) / (1024 * 1024),
    }
}

// ─────────────────────────────────────────────────────────────────
// Benchmark 2: Memory per goroutine (kept alive)
// ─────────────────────────────────────────────────────────────────

func benchMemory(n int) BenchResult {
    var wg sync.WaitGroup
    wg.Add(n)
    done := make(chan struct{})

    memBefore := measureMemory()
    start := time.Now()

    for i := 0; i < n; i++ {
        go func() {
            wg.Done()
            <-done // mantener viva
        }()
    }
    wg.Wait()

    elapsed := time.Since(start)
    memAfter := measureMemory()

    close(done)
    // Esperar un poco para que se liberen
    time.Sleep(50 * time.Millisecond)

    return BenchResult{
        Name:     "goroutine_memory",
        N:        n,
        Duration: elapsed,
        PerUnit:  elapsed / time.Duration(n),
        MemoryMB: float64(memAfter-memBefore) / (1024 * 1024),
    }
}

// ─────────────────────────────────────────────────────────────────
// Benchmark 3: Context switch via channel
// ─────────────────────────────────────────────────────────────────

func benchContextSwitch(n int) BenchResult {
    ch := make(chan struct{})

    go func() {
        for i := 0; i < n; i++ {
            ch <- struct{}{}
        }
    }()

    start := time.Now()
    for i := 0; i < n; i++ {
        <-ch
    }
    elapsed := time.Since(start)

    return BenchResult{
        Name:     "context_switch",
        N:        n,
        Duration: elapsed,
        PerUnit:  elapsed / time.Duration(n),
    }
}

// ─────────────────────────────────────────────────────────────────
// Benchmark 4: CPU-bound work with varying GOMAXPROCS
// ─────────────────────────────────────────────────────────────────

func cpuWork() float64 {
    sum := 0.0
    for i := 0; i < 10_000_000; i++ {
        sum += math.Sqrt(float64(i))
    }
    return sum
}

func benchCPUBound(numGoroutines int, maxProcs int) BenchResult {
    old := runtime.GOMAXPROCS(maxProcs)
    defer runtime.GOMAXPROCS(old)

    var wg sync.WaitGroup
    wg.Add(numGoroutines)

    start := time.Now()
    for i := 0; i < numGoroutines; i++ {
        go func() {
            defer wg.Done()
            cpuWork()
        }()
    }
    wg.Wait()
    elapsed := time.Since(start)

    return BenchResult{
        Name:     fmt.Sprintf("cpu_bound(procs=%d)", maxProcs),
        N:        numGoroutines,
        Duration: elapsed,
        PerUnit:  elapsed / time.Duration(numGoroutines),
    }
}

// ─────────────────────────────────────────────────────────────────
// Benchmark 5: I/O-bound simulation
// ─────────────────────────────────────────────────────────────────

func benchIOBound(n int) BenchResult {
    var wg sync.WaitGroup
    wg.Add(n)

    start := time.Now()
    for i := 0; i < n; i++ {
        go func() {
            defer wg.Done()
            time.Sleep(100 * time.Millisecond) // simular I/O
        }()
    }
    wg.Wait()
    elapsed := time.Since(start)

    return BenchResult{
        Name:     "io_bound_simulation",
        N:        n,
        Duration: elapsed,
        PerUnit:  elapsed / time.Duration(n),
    }
}

// ─────────────────────────────────────────────────────────────────
// Benchmark 6: Stack growth
// ─────────────────────────────────────────────────────────────────

func recursiveWork(depth int) int {
    if depth <= 0 {
        return 0
    }
    var buf [256]byte // 256 bytes por frame
    _ = buf
    return recursiveWork(depth-1) + 1
}

func benchStackGrowth(n int, depth int) BenchResult {
    var wg sync.WaitGroup
    wg.Add(n)

    memBefore := measureMemory()
    start := time.Now()

    for i := 0; i < n; i++ {
        go func() {
            defer wg.Done()
            recursiveWork(depth) // forza stack growth
        }()
    }
    wg.Wait()
    elapsed := time.Since(start)
    memAfter := measureMemory()

    return BenchResult{
        Name:     fmt.Sprintf("stack_growth(depth=%d)", depth),
        N:        n,
        Duration: elapsed,
        PerUnit:  elapsed / time.Duration(n),
        MemoryMB: float64(memAfter-memBefore) / (1024 * 1024),
    }
}

// ─────────────────────────────────────────────────────────────────
// Main
// ─────────────────────────────────────────────────────────────────

func main() {
    fmt.Println("╔═══════════════════════════════════════════════════╗")
    fmt.Println("║   Goroutine vs Thread Benchmark Suite             ║")
    fmt.Printf("║   CPUs: %-3d  GOMAXPROCS: %-3d  OS: Linux           ║\n",
        runtime.NumCPU(), runtime.GOMAXPROCS(0))
    fmt.Println("╚═══════════════════════════════════════════════════╝")

    // Warmup
    benchCreation(1000)
    runtime.GC()

    // 1. Creation cost
    fmt.Println("\n── Benchmark 1: Creation Cost ──")
    for _, n := range []int{1_000, 10_000, 100_000, 1_000_000} {
        fmt.Println(benchCreation(n))
    }

    // 2. Memory per goroutine
    fmt.Println("\n── Benchmark 2: Memory (goroutines kept alive) ──")
    for _, n := range []int{1_000, 10_000, 100_000} {
        fmt.Println(benchMemory(n))
        runtime.GC()
        time.Sleep(100 * time.Millisecond)
    }

    // 3. Context switch
    fmt.Println("\n── Benchmark 3: Context Switch (channel ping-pong) ──")
    for _, n := range []int{100_000, 1_000_000} {
        fmt.Println(benchContextSwitch(n))
    }

    // 4. CPU-bound with different GOMAXPROCS
    fmt.Println("\n── Benchmark 4: CPU-Bound (8 goroutines, varying GOMAXPROCS) ──")
    for _, procs := range []int{1, 2, 4, runtime.NumCPU()} {
        fmt.Println(benchCPUBound(8, procs))
    }

    // 5. I/O-bound
    fmt.Println("\n── Benchmark 5: I/O-Bound (100ms sleep per goroutine) ──")
    for _, n := range []int{100, 1_000, 10_000} {
        fmt.Println(benchIOBound(n))
    }

    // 6. Stack growth
    fmt.Println("\n── Benchmark 6: Stack Growth (recursive, 256 bytes/frame) ──")
    for _, depth := range []int{10, 100, 1000} {
        fmt.Println(benchStackGrowth(1000, depth))
    }

    // Summary comparison
    fmt.Println("\n── Summary: Goroutine vs OS Thread (estimated) ──")
    fmt.Println("  ┌──────────────────┬───────────────┬────────────────────┐")
    fmt.Println("  │ Metric           │ OS Thread     │ Goroutine          │")
    fmt.Println("  ├──────────────────┼───────────────┼────────────────────┤")
    fmt.Println("  │ Creation         │ ~20 µs        │ ~0.3 µs (67x)     │")
    fmt.Println("  │ Memory (idle)    │ ~1 MB         │ ~2.5 KB (400x)    │")
    fmt.Println("  │ Context switch   │ ~3 µs         │ ~0.15 µs (20x)    │")
    fmt.Println("  │ Max per process  │ ~5,000        │ ~1,000,000 (200x) │")
    fmt.Println("  │ Stack growth     │ No (crash)    │ Automatic          │")
    fmt.Println("  │ 10K instances    │ ~10 GB RAM    │ ~25 MB RAM (400x) │")
    fmt.Println("  └──────────────────┴───────────────┴────────────────────┘")

    fmt.Printf("\nFinal goroutine count: %d\n", runtime.NumGoroutine())
}
```

### Ejecutar:

```bash
go mod init goroutine-bench
go run main.go

# Con race detector (mas lento pero verifica correctness):
go run -race main.go

# Ver scheduler en accion:
GODEBUG=schedtrace=500 go run main.go
```

---

## 11. Ejercicios

### Ejercicio 1: Benchmark Comparativo Completo

```
Implementa un benchmark suite que mide y compara:

Requisitos:
- Creacion: medir tiempo de crear 1K, 10K, 100K, 1M goroutines
- Memoria: medir bytes por goroutine (idle vs con trabajo)
- Context switch: ping-pong entre dos goroutines via channel
  Medir: latencia de un round-trip, throughput (switches/segundo)
- CPU scaling: trabajo CPU-bound con GOMAXPROCS=1,2,4,8,NumCPU()
  Graficar speedup (¿es lineal? ¿donde satura?)
- Stack growth cost: comparar goroutines con recursion depth 10 vs 1000
  ¿Cuanto mas lentas son las goroutines que necesitan stack growth?
- Generar un reporte en formato tabla con los resultados
- Comparar tus numeros con los benchmarks de pthreads (ejecutar
  el equivalente C si tienes gcc disponible)
```

### Ejercicio 2: C10K Server

```
Implementa un echo server que maneja 10,000 conexiones simultaneas:

Requisitos:
- Servidor TCP que acepta conexiones y hace echo de lo recibido
- Crear un cliente que abre 10,000 conexiones concurrentes
- Medir: goroutines activas, memoria total, latencia por echo
- Ejecutar con GODEBUG=schedtrace=1000 y analizar:
  - ¿Cuantos threads (M) se crearon?
  - ¿Cuantos P's estan idle vs activos?
  - ¿Cuantas goroutines en la run queue?
- Comparar con un server equivalente en C usando threads
  (o buscar benchmarks de referencia)
- Bonus: ¿que pasa con 100,000 conexiones?
```

### Ejercicio 3: Stack Growth Visualizer

```
Implementa un programa que visualiza el crecimiento del stack:

Requisitos:
- Funcion recursiva que reporta el tamano actual del stack en cada nivel
  (usar unsafe.Pointer para obtener la direccion del stack)
- Mostrar: profundidad, direccion del stack, tamano estimado
- Detectar cuando ocurre un stack copy (la direccion del stack cambia)
- Ejecutar con GODEBUG=stackgrowth=1 si esta disponible
- Crear goroutines con diferente profundidad de recursion
- Medir: tiempo de stack growth, memoria antes/despues
- Responder: ¿el stack growth es O(n) donde n es el tamano del stack?
  ¿O es O(1) amortizado?
```

### Ejercicio 4: Goroutine Pool vs Direct Spawn

```
Compara dos estrategias para procesar 100,000 tareas:

Estrategia A: Spawn directo
- Crear una goroutine por tarea (100K goroutines)

Estrategia B: Worker pool
- Crear N workers (N = GOMAXPROCS)
- Los workers leen de un channel de tareas

Requisitos:
- Implementar ambas estrategias
- Benchmark con tareas CPU-bound (math.Sqrt loop)
- Benchmark con tareas I/O-bound (time.Sleep)
- Medir: tiempo total, memoria pico, goroutines pico
- Responder:
  - ¿Cual es mas rapida para CPU-bound? ¿Por que?
  - ¿Cual es mas rapida para I/O-bound? ¿Por que?
  - ¿Cuando vale la pena un worker pool en Go?
  - ¿Es el overhead de crear 100K goroutines significativo?
```

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│              GOROUTINES vs THREADS — RESUMEN COMPLETO                    │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  COSTE:                                                                  │
│  ├─ Creacion:  goroutine ~0.3µs vs thread ~20µs (67x)                  │
│  ├─ Memoria:   goroutine ~2.5KB vs thread ~1MB (400x)                  │
│  ├─ Switch:    goroutine ~150ns vs thread ~3µs (20x)                   │
│  └─ Max:       goroutine ~1M vs thread ~5K (200x)                      │
│                                                                          │
│  STACK:                                                                  │
│  ├─ Thread: fijo (1MB), no crece, overflow=crash                       │
│  ├─ Goroutine: dinamico (2KB→1GB), crece por copia                     │
│  └─ Solo Go puede copiar stacks (GC conoce todos los punteros)         │
│                                                                          │
│  SCHEDULING:                                                             │
│  ├─ Thread: kernel CFS, preemptivo, prioridades, TLB flush             │
│  ├─ Goroutine: runtime GMP, cooperativo+preempt, sin prioridades       │
│  └─ Goroutine switch: solo 3 registros, sin syscall, sin TLB flush     │
│                                                                          │
│  FUNCTION COLORING:                                                      │
│  ├─ Go: NO — cualquier funcion puede correr en goroutine               │
│  ├─ Rust: SI — async fn requiere .await, "infecta" el call stack       │
│  └─ Esta es una de las mayores ventajas de diseño de Go                │
│                                                                          │
│  CUANDO THREADS SON NECESARIOS:                                         │
│  ├─ CGO (thread-local state de C)                                      │
│  ├─ GPU/OpenGL (thread-specific context)                               │
│  ├─ Real-time scheduling (SCHED_FIFO/RR)                               │
│  ├─ CPU affinity (NUMA-aware workloads)                                │
│  └─ runtime.LockOSThread() para estos casos                            │
│                                                                          │
│  ESCALA EN PRODUCCION:                                                   │
│  ├─ <100 unidades: threads y goroutines son equivalentes               │
│  ├─ 1K-100K: goroutines son claramente superiores                      │
│  ├─ 100K-1M: solo goroutines (o event loops)                           │
│  └─ Kubernetes, Docker, CockroachDB: millones de goroutines            │
│                                                                          │
│  vs C: pthreads son pesados, manuales, limitados por kernel            │
│  vs Rust std: 1:1 como pthreads (mismos limites)                       │
│  vs Rust tokio: comparable a goroutines, pero con async coloring       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S01 - Goroutines, T03 - WaitGroup — sync.WaitGroup, Add, Done, Wait — coordinacion basica
