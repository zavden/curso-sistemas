# Channels Basicos — make(chan T), send (<-), receive, channels con y sin buffer

## 1. Introduccion

Los channels son la primitiva de comunicacion fundamental de Go. Mientras que goroutines son la unidad de ejecucion concurrente, los channels son el mecanismo por el cual las goroutines se comunican y sincronizan. Go implementa el modelo CSP (Communicating Sequential Processes) de Tony Hoare (1978): en lugar de compartir memoria y protegerla con locks, las goroutines **envian datos a traves de channels**.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CHANNELS EN GO                                         │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  "Don't communicate by sharing memory;                                  │
│   share memory by communicating."  — Go Proverbs (Rob Pike)             │
│                                                                          │
│  MODELO TRADICIONAL (C, Java, Python):                                  │
│                                                                          │
│  Goroutine A ──────► [ Memoria Compartida ] ◄────── Goroutine B        │
│                       protegida por Mutex                                │
│                                                                          │
│  MODELO GO (CSP):                                                        │
│                                                                          │
│  Goroutine A ──send──► [ Channel ] ──receive──► Goroutine B            │
│                        datos fluyen                                      │
│                        sin compartir                                     │
│                                                                          │
│  BENEFICIOS:                                                             │
│  ├─ Sin data races por diseno (datos tienen un solo dueno)             │
│  ├─ Sincronizacion implicita (send/receive bloquean)                   │
│  ├─ Composicion natural (pipelines, fan-out/fan-in)                    │
│  └─ Codigo mas facil de razonar que mutex/locks                        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

Para SysAdmin/DevOps, los channels son esenciales porque:
- Toda herramienta concurrente en Go (kubectl, terraform, prometheus) usa channels internamente
- Los patrones de pipeline (procesar logs, monitorear hosts, deploy rolling) se modelan naturalmente con channels
- Entender channels unbuffered vs buffered es critico para evitar deadlocks y goroutine leaks en produccion
- Los channels reemplazan la mayoria de usos de mutex, resultando en codigo mas simple y seguro

---

## 2. Que es un Channel

Un channel es un **conducto tipado** que permite a una goroutine enviar valores a otra goroutine. Es un tipo de primera clase en Go: puedes pasarlo como argumento, retornarlo de funciones, almacenarlo en structs, e incluso enviarlo a traves de otro channel.

### 2.1 Anatomia del tipo

```go
// Declaracion del tipo channel
var ch chan int          // channel bidireccional de int (valor zero: nil)
var sendOnly chan<- int  // channel de solo envio
var recvOnly <-chan int  // channel de solo recepcion

// La flecha <- SIEMPRE va a la izquierda del chan para receive-only
// y a la derecha del chan para send-only
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    TIPOS DE CHANNEL                                       │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  chan T        bidireccional — puede enviar Y recibir                   │
│  chan<- T      send-only    — solo puede enviar (ch <- val)             │
│  <-chan T      receive-only — solo puede recibir (val := <-ch)          │
│                                                                          │
│  La direccion se cubre en detalle en T02.                               │
│  Por ahora trabajamos con chan T (bidireccional).                       │
│                                                                          │
│  VALOR ZERO:                                                             │
│  ├─ var ch chan int      → ch == nil                                    │
│  ├─ Enviar a nil channel → BLOQUEA PARA SIEMPRE                       │
│  ├─ Recibir de nil channel → BLOQUEA PARA SIEMPRE                     │
│  └─ Cerrar nil channel → PANIC                                         │
│                                                                          │
│  COMPARABLE:                                                             │
│  ├─ ch1 == ch2  → true si apuntan al mismo channel                    │
│  ├─ ch == nil   → true si no inicializado                              │
│  └─ Util para select con channels opcionales                           │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.2 Creacion con make

Los channels se crean con `make`. A diferencia de slices y maps, **un channel no inicializado (nil) causa deadlock**, no panic:

```go
// Channel sin buffer (unbuffered) — capacidad 0
ch := make(chan int)

// Channel con buffer — capacidad N
ch := make(chan int, 10)  // buffer para 10 valores

// Channel de structs vacios — solo senalizacion, sin datos
done := make(chan struct{})

// Channel de channels — metaprogramacion concurrente
chOfCh := make(chan chan int)

// Channel de funciones
tasks := make(chan func())
```

### 2.3 Estructura interna (runtime/chan.go)

Internamente, un channel es un puntero a una estructura `hchan` en el runtime:

```go
// Simplificado de runtime/chan.go
type hchan struct {
    qcount   uint           // numero de elementos en el buffer
    dataqsiz uint           // tamano del buffer circular (0 = unbuffered)
    buf      unsafe.Pointer // puntero al buffer circular (array)
    elemsize uint16         // tamano de cada elemento
    closed   uint32         // 1 si el channel esta cerrado
    elemtype *_type         // tipo del elemento
    sendx    uint           // indice de envio en el buffer circular
    recvx    uint           // indice de recepcion en el buffer circular
    recvq    waitq          // lista de goroutines esperando recibir
    sendq    waitq          // lista de goroutines esperando enviar
    lock     mutex          // mutex interno (no sync.Mutex, es un spinlock)
}

type waitq struct {
    first *sudog            // lista enlazada de goroutines esperando
    last  *sudog
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ESTRUCTURA INTERNA DE UN CHANNEL                      │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  make(chan int, 4) crea:                                                │
│                                                                          │
│  hchan {                                                                 │
│    qcount: 0          dataqsiz: 4                                       │
│    closed: 0          elemsize: 8 (sizeof int)                          │
│                                                                          │
│    buf: ──► ┌───┬───┬───┬───┐   buffer circular                       │
│             │   │   │   │   │   (array de 4 ints)                      │
│             └───┴───┴───┴───┘                                           │
│             ▲               ▲                                            │
│           recvx=0        sendx=0                                        │
│                                                                          │
│    recvq: nil ──► (goroutines esperando recibir)                       │
│    sendq: nil ──► (goroutines esperando enviar)                        │
│                                                                          │
│    lock: mutex (spinlock del runtime)                                   │
│  }                                                                       │
│                                                                          │
│  Despues de ch <- 42; ch <- 99:                                         │
│                                                                          │
│    buf: ──► ┌────┬────┬───┬───┐                                        │
│             │ 42 │ 99 │   │   │                                        │
│             └────┴────┴───┴───┘                                         │
│             ▲          ▲                                                 │
│           recvx=0   sendx=2                                             │
│    qcount: 2                                                             │
│                                                                          │
│  Despues de v := <-ch  (recibe 42):                                     │
│                                                                          │
│    buf: ──► ┌────┬────┬───┬───┐                                        │
│             │    │ 99 │   │   │                                        │
│             └────┴────┴───┴───┘                                         │
│                  ▲     ▲                                                 │
│               recvx=1 sendx=2                                           │
│    qcount: 1                                                             │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 3. Operaciones Fundamentales

### 3.1 Send (enviar)

```go
ch <- value  // envia value al channel ch
```

El operador `<-` con el channel a la **izquierda** es un send. El comportamiento depende del tipo de channel:

```go
package main

import "fmt"

func main() {
    ch := make(chan string, 1) // buffered, capacidad 1
    
    ch <- "hello"  // send: no bloquea porque hay espacio en el buffer
    
    fmt.Println("Enviado!")
    
    // Si hicieramos otro ch <- "world" aqui, BLOQUEARIA
    // porque el buffer esta lleno (capacidad 1, ya tiene 1 elemento)
}
```

### 3.2 Receive (recibir)

```go
value := <-ch   // recibe un valor del channel y lo asigna
value = <-ch     // recibe y asigna a variable existente
<-ch             // recibe y descarta el valor (solo sincronizacion)
```

El operador `<-` con el channel a la **derecha** es un receive:

```go
package main

import "fmt"

func main() {
    ch := make(chan string, 1)
    ch <- "hello"
    
    // Tres formas de recibir:
    
    // 1. Recibir y asignar
    msg := <-ch
    fmt.Println(msg) // "hello"
    
    // 2. Recibir con "comma ok" — detectar channel cerrado
    ch2 := make(chan int, 1)
    ch2 <- 42
    close(ch2)
    
    v, ok := <-ch2
    fmt.Println(v, ok)  // 42 true  (habia un valor)
    
    v, ok = <-ch2
    fmt.Println(v, ok)  // 0 false  (channel cerrado y vacio)
    
    // 3. Recibir y descartar — solo sincronizacion
    done := make(chan struct{}, 1)
    done <- struct{}{}
    <-done  // esperamos la senal, no nos importa el valor
}
```

### 3.3 Close (cerrar)

```go
close(ch)  // cierra el channel — SOLO el sender debe cerrar
```

Cerrar un channel senala que no se enviaran mas valores:

```go
package main

import "fmt"

func producer(ch chan<- int) {
    for i := 0; i < 5; i++ {
        ch <- i
    }
    close(ch)  // el PRODUCTOR cierra el channel
}

func main() {
    ch := make(chan int)
    go producer(ch)
    
    // range sobre channel — itera hasta que se cierra
    for v := range ch {
        fmt.Println(v)  // 0, 1, 2, 3, 4
    }
    // El loop termina automaticamente cuando ch se cierra
    fmt.Println("Channel cerrado, loop terminado")
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    REGLAS DE CLOSE                                        │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. Solo el SENDER cierra el channel, nunca el receiver                │
│     ├─ No hay forma de saber si un sender aun va a enviar              │
│     └─ Enviar a channel cerrado → PANIC                                │
│                                                                          │
│  2. Cerrar channel cerrado → PANIC                                      │
│     └─ No hay forma de preguntar "esta cerrado?" sin recibir           │
│                                                                          │
│  3. Recibir de channel cerrado → retorna valor zero + false            │
│     └─ v, ok := <-ch   → ok == false si cerrado y vacio               │
│                                                                          │
│  4. Cerrar NO es obligatorio                                            │
│     ├─ Solo cierra si los receivers NECESITAN saber que ya no hay mas  │
│     └─ El GC recolecta channels no referenciados (cerrados o no)       │
│                                                                          │
│  5. No uses close como "cancelacion"                                    │
│     └─ Usa context.Context para senalizar cancelacion                  │
│                                                                          │
│  PANIC SCENARIOS:                                                        │
│  ├─ close(ch) + close(ch)     → panic: close of closed channel        │
│  ├─ ch <- val  (ch cerrado)   → panic: send on closed channel         │
│  └─ close(nil)                → panic: close of nil channel            │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 3.4 Tabla de comportamiento completa

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    COMPORTAMIENTO DE OPERACIONES                         │
├───────────────────┬──────────────┬──────────────┬────────────────────────┤
│ Operacion          │ Channel nil  │ Ch cerrado   │ Ch abierto            │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ ch <- val          │ BLOQUEA      │ PANIC        │ Envia o bloquea       │
│                   │ (para siempre)│              │ (si buffer lleno)     │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ <-ch              │ BLOQUEA      │ zero value   │ Recibe o bloquea      │
│                   │ (para siempre)│ (ok=false)   │ (si buffer vacio)     │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ close(ch)          │ PANIC        │ PANIC        │ Cierra (ok)           │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ len(ch)            │ 0            │ elementos    │ elementos en buffer   │
│                   │              │ restantes    │                        │
├───────────────────┼──────────────┼──────────────┼────────────────────────┤
│ cap(ch)            │ 0            │ capacidad    │ capacidad del buffer  │
│                   │              │ original     │                        │
└───────────────────┴──────────────┴──────────────┴────────────────────────┘
```

---

## 4. Channels Unbuffered (sin buffer)

Un channel unbuffered tiene **capacidad 0**. Cada send bloquea hasta que otra goroutine ejecuta un receive, y viceversa. Es una **sincronizacion punto a punto** — los datos se transfieren directamente entre goroutines sin almacenamiento intermedio.

### 4.1 Semantica de sincronizacion

```go
ch := make(chan int) // unbuffered — capacidad 0
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CHANNEL UNBUFFERED — RENDEZVOUS                       │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  make(chan int) — sin segundo argumento o con 0                         │
│                                                                          │
│  SEMANTICA: ambos participantes deben estar listos simultaneamente      │
│  Es un "rendezvous" (encuentro) — como un apreton de manos              │
│                                                                          │
│  Goroutine A              Channel (cap=0)        Goroutine B            │
│  ──────────              ─────────────          ──────────              │
│                                                                          │
│  ch <- 42   ──────────►  [BLOQUEADO]                                    │
│  (A se bloquea                                                           │
│   esperando                                                              │
│   receptor)              A sale del ◄─────────  v := <-ch               │
│                          bloqueo                 (B recibe 42)           │
│  A continua                                     B continua              │
│                                                                          │
│  GARANTIA: cuando ch <- 42 retorna, SABEMOS que                        │
│  el receptor ya tiene el valor. Es una "happens-before"                 │
│  relacion: todo lo que A hizo antes del send es visible                 │
│  para B despues del receive.                                             │
│                                                                          │
│  ┌──────────────────────────────────────┐                               │
│  │ Los datos NUNCA se copian al buffer  │                               │
│  │ (no hay buffer). Se copian DIRECTO   │                               │
│  │ del stack de A al stack de B.        │                               │
│  └──────────────────────────────────────┘                                │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 4.2 Ejemplo: sincronizacion basica

```go
package main

import (
    "fmt"
    "time"
)

func worker(id int, done chan<- bool) {
    fmt.Printf("Worker %d: iniciando trabajo...\n", id)
    time.Sleep(time.Second) // simula trabajo
    fmt.Printf("Worker %d: trabajo completado\n", id)
    done <- true // senaliza que termino
}

func main() {
    done := make(chan bool) // unbuffered
    
    go worker(1, done)
    
    fmt.Println("main: esperando worker...")
    <-done // bloquea hasta que worker envie
    fmt.Println("main: worker termino!")
}
// Output:
// main: esperando worker...
// Worker 1: iniciando trabajo...
// Worker 1: trabajo completado
// main: worker termino!
```

### 4.3 Ejemplo: intercambio de datos

```go
package main

import "fmt"

func sum(nums []int, result chan<- int) {
    total := 0
    for _, n := range nums {
        total += n
    }
    result <- total // envia resultado
}

func main() {
    nums := []int{1, 2, 3, 4, 5, 6, 7, 8, 9, 10}
    
    ch := make(chan int) // unbuffered
    
    // Dividir trabajo entre 2 goroutines
    mid := len(nums) / 2
    go sum(nums[:mid], ch) // suma primera mitad
    go sum(nums[mid:], ch) // suma segunda mitad
    
    // Recibir ambos resultados
    x := <-ch  // bloquea hasta primer resultado
    y := <-ch  // bloquea hasta segundo resultado
    
    fmt.Printf("Suma parcial 1: %d\n", x)
    fmt.Printf("Suma parcial 2: %d\n", y)
    fmt.Printf("Suma total: %d\n", x+y) // 55
}
```

### 4.4 Deadlock con unbuffered

El error mas comun con channels unbuffered es el deadlock — cuando nadie esta del otro lado:

```go
package main

func main() {
    ch := make(chan int) // unbuffered
    
    // DEADLOCK: main se bloquea en send, pero nadie lee
    ch <- 42  // fatal error: all goroutines are asleep - deadlock!
    
    // Go detecta deadlocks cuando TODAS las goroutines estan bloqueadas
    // Si hay goroutines que podrian desbloquearse, no es deadlock
}
```

```go
package main

func main() {
    ch := make(chan int) // unbuffered
    
    // DEADLOCK: send y receive en la MISMA goroutine
    // El send bloquea esperando receiver, pero el receive esta despues
    ch <- 42       // bloquea aqui — nunca llega a la siguiente linea
    v := <-ch      // nunca se ejecuta
    _ = v
}
```

```go
package main

import "fmt"

func main() {
    ch := make(chan int) // unbuffered
    
    // CORRECTO: send y receive en goroutines diferentes
    go func() {
        ch <- 42   // se bloquea hasta que main reciba
    }()
    
    v := <-ch      // se bloquea hasta que la goroutine envie
    fmt.Println(v) // 42
}
```

### 4.5 Cuando usar unbuffered

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CUANDO USAR CHANNEL UNBUFFERED                        │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  USA unbuffered cuando:                                                 │
│                                                                          │
│  ✓ Necesitas GARANTIA de entrega                                       │
│    "Cuando mi send retorna, se que el receiver ya tiene el valor"      │
│                                                                          │
│  ✓ Quieres sincronizacion natural                                      │
│    El sender espera al receiver — ritmo controlado                     │
│                                                                          │
│  ✓ Senalizacion simple (done/quit)                                     │
│    done := make(chan struct{})                                          │
│    close(done) para notificar a multiples receivers                    │
│                                                                          │
│  ✓ Handoff de responsabilidad                                          │
│    "Te paso este trabajo, no continuo hasta que lo aceptes"            │
│                                                                          │
│  ✓ Request/Response (como una llamada de funcion entre goroutines)     │
│    Envia request, bloquea esperando response                           │
│                                                                          │
│  NO uses unbuffered cuando:                                             │
│                                                                          │
│  ✗ El producer es mas rapido que el consumer (se bloqueara mucho)      │
│  ✗ Quieres "fire and forget"                                           │
│  ✗ Necesitas amortiguamiento de rafagas de trabajo                     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Channels Buffered (con buffer)

Un channel buffered tiene capacidad **N > 0**. Los sends no bloquean mientras haya espacio en el buffer, y los receives no bloquean mientras haya datos en el buffer. Es un **buzón con capacidad limitada**.

### 5.1 Semantica de buffer

```go
ch := make(chan int, 5) // buffer para 5 valores
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CHANNEL BUFFERED — COLA FIFO                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  make(chan int, 5) — buffer circular de capacidad 5                     │
│                                                                          │
│  Estado inicial:                                                         │
│  ┌───┬───┬───┬───┬───┐                                                 │
│  │   │   │   │   │   │  qcount=0  len(ch)=0  cap(ch)=5                │
│  └───┴───┴───┴───┴───┘                                                 │
│                                                                          │
│  Despues de ch <- 10; ch <- 20; ch <- 30:                               │
│  ┌────┬────┬────┬───┬───┐                                              │
│  │ 10 │ 20 │ 30 │   │   │  qcount=3  len(ch)=3  cap(ch)=5            │
│  └────┴────┴────┴───┴───┘                                              │
│  ▲ recvx          ▲ sendx                                               │
│                                                                          │
│  ch <- 40; ch <- 50  (buffer lleno):                                    │
│  ┌────┬────┬────┬────┬────┐                                            │
│  │ 10 │ 20 │ 30 │ 40 │ 50 │  qcount=5  len(ch)=5  cap(ch)=5          │
│  └────┴────┴────┴────┴────┘                                            │
│  ▲ recvx                    ▲ sendx (wraparound)                       │
│                                                                          │
│  ch <- 60  → BLOQUEA (buffer lleno, espera a que alguien reciba)       │
│                                                                          │
│  v := <-ch  (recibe 10, FIFO):                                          │
│  ┌────┬────┬────┬────┬────┐                                            │
│  │    │ 20 │ 30 │ 40 │ 50 │  qcount=4  len(ch)=4  cap(ch)=5          │
│  └────┴────┴────┴────┴────┘                                            │
│       ▲ recvx               ▲ sendx                                    │
│                                                                          │
│  Ahora ch <- 60 puede proceder (hay espacio):                          │
│  ┌────┬────┬────┬────┬────┐                                            │
│  │ 60 │ 20 │ 30 │ 40 │ 50 │  qcount=5  (buffer circular wraparound)  │
│  └────┴────┴────┴────┴────┘                                            │
│       ▲ sendx  ▲ recvx                                                  │
│                                                                          │
│  REGLA: los sends no bloquean HASTA que el buffer se llena             │
│  REGLA: los receives no bloquean MIENTRAS haya datos en el buffer      │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 5.2 Ejemplo: productor mas rapido que consumidor

```go
package main

import (
    "fmt"
    "time"
)

func producer(ch chan<- int) {
    for i := 1; i <= 10; i++ {
        fmt.Printf("Produciendo %d\n", i)
        ch <- i
        // El producer no se bloquea mientras haya espacio en el buffer
    }
    close(ch)
}

func consumer(ch <-chan int) {
    for v := range ch {
        fmt.Printf("  Consumiendo %d (procesando...)\n", v)
        time.Sleep(500 * time.Millisecond) // consumer lento
    }
}

func main() {
    ch := make(chan int, 3) // buffer de 3
    
    go producer(ch)
    consumer(ch)
    
    // Output tipico:
    // Produciendo 1
    // Produciendo 2
    // Produciendo 3
    // Produciendo 4    ← aqui el buffer se llena, producer se bloquea
    //   Consumiendo 1 (procesando...)
    // Produciendo 5    ← consumer libero espacio, producer continua
    //   Consumiendo 2 (procesando...)
    // ...
}
```

### 5.3 Channel como semaforo

Un channel buffered puede usarse como semáforo para limitar concurrencia:

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

func main() {
    const maxConcurrency = 3
    sem := make(chan struct{}, maxConcurrency) // semaforo
    
    var wg sync.WaitGroup
    
    for i := 1; i <= 10; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            
            sem <- struct{}{} // adquirir — bloquea si ya hay 3 en ejecucion
            defer func() { <-sem }() // liberar al terminar
            
            fmt.Printf("Worker %d: ejecutando (max %d concurrentes)\n", id, maxConcurrency)
            time.Sleep(time.Second)
            fmt.Printf("Worker %d: terminado\n", id)
        }(i)
    }
    
    wg.Wait()
    fmt.Println("Todos los workers terminaron")
}
```

### 5.4 Channel de capacidad 1 — mutex basico

```go
package main

import "fmt"

func main() {
    mu := make(chan struct{}, 1) // "mutex" con channel
    
    counter := 0
    done := make(chan struct{})
    
    for i := 0; i < 1000; i++ {
        go func() {
            mu <- struct{}{} // lock
            counter++
            <-mu // unlock
            done <- struct{}{}
        }()
    }
    
    for i := 0; i < 1000; i++ {
        <-done
    }
    
    fmt.Println("Counter:", counter) // siempre 1000
    // Funciona pero sync.Mutex es mas eficiente para esto
}
```

### 5.5 Cuando usar buffered

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CUANDO USAR CHANNEL BUFFERED                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  USA buffered cuando:                                                   │
│                                                                          │
│  ✓ Producer y consumer tienen velocidades diferentes                   │
│    Buffer absorbe las rafagas sin bloquear al producer                 │
│                                                                          │
│  ✓ Quieres desacoplar el timing del send y receive                     │
│    El sender no necesita esperar al receiver                           │
│                                                                          │
│  ✓ Conoces el numero de resultados de antemano                         │
│    results := make(chan int, numWorkers)                                │
│    — puedes recoger despues sin deadlock                               │
│                                                                          │
│  ✓ Semaforo para limitar concurrencia                                  │
│    sem := make(chan struct{}, maxConcurrency)                           │
│                                                                          │
│  ✓ Batch processing                                                    │
│    Buffer acumula un lote antes de procesarlo                          │
│                                                                          │
│  TAMAÑO DEL BUFFER:                                                     │
│                                                                          │
│  ├─ 1: mutex simple o "enviar sin bloquear si esta libre"              │
│  ├─ N conocido: "se que habra exactamente N resultados"                │
│  ├─ N estimado: absorber rafagas tipicas                               │
│  └─ ∞ (muy grande): ANTIPATTERN — esconde problemas de backpressure   │
│                                                                          │
│  REGLA: "Si no puedes justificar el tamano del buffer,                 │
│          usa unbuffered." — Go community wisdom                        │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 6. Unbuffered vs Buffered — Comparacion Profunda

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    UNBUFFERED vs BUFFERED                                 │
├──────────────────┬───────────────────────┬───────────────────────────────┤
│ Aspecto           │ Unbuffered (cap=0)    │ Buffered (cap=N)             │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Creacion          │ make(chan T)           │ make(chan T, N)              │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Send bloquea      │ Hasta que alguien     │ Hasta que haya espacio      │
│                   │ reciba                │ en el buffer                 │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Receive bloquea   │ Hasta que alguien     │ Hasta que haya datos        │
│                   │ envie                 │ en el buffer                 │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Garantia          │ Entrega sincrona      │ Entrega eventual             │
│                   │ (handshake)           │ (mailbox)                    │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Acoplamiento      │ Fuerte — sender y     │ Debil — sender puede        │
│                   │ receiver sincronizados│ avanzar sin esperar          │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Latencia send     │ Alta (espera receiver)│ Baja (si hay espacio)        │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Throughput        │ Menor (sincronizacion │ Mayor (producer/consumer     │
│                   │ en cada operacion)    │ desacoplados)                │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Backpressure      │ Inmediata — sender    │ Retardada — solo cuando     │
│                   │ se frena al ritmo     │ buffer se llena              │
│                   │ del receiver          │                              │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Deadlock risk     │ Alto si send/receive  │ Menor pero posible          │
│                   │ en misma goroutine    │ (buffer lleno + misma gor.) │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Analogia          │ Llamada telefonica    │ Buzon de correo              │
│                   │ (ambos presentes)     │ (dejas carta y te vas)       │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Uso tipico        │ Senalizacion, handoff │ Work queues, batching,       │
│                   │ request/response      │ semaforos, absorber rafagas  │
└──────────────────┴───────────────────────┴───────────────────────────────┘
```

### 6.1 Ejemplo comparativo — mismo programa

```go
package main

import (
    "fmt"
    "time"
)

func demo(name string, ch chan int) {
    start := time.Now()
    
    // Producer
    go func() {
        for i := 1; i <= 5; i++ {
            fmt.Printf("[%s] Enviando %d...\n", name, i)
            ch <- i
            fmt.Printf("[%s] Enviado %d (elapsed: %v)\n", name, i, time.Since(start).Round(time.Millisecond))
        }
        close(ch)
    }()
    
    // Consumer (lento — 200ms por item)
    for v := range ch {
        time.Sleep(200 * time.Millisecond)
        fmt.Printf("[%s]   Recibido %d (elapsed: %v)\n", name, v, time.Since(start).Round(time.Millisecond))
    }
}

func main() {
    fmt.Println("=== UNBUFFERED ===")
    demo("unbuf", make(chan int))
    
    fmt.Println("\n=== BUFFERED (cap=3) ===")
    demo("buf-3", make(chan int, 3))
}

// Con unbuffered: cada send espera ~200ms (ritmo del consumer)
// Total: ~1000ms (5 items × 200ms)
//
// Con buffered(3): primeros 4 sends no esperan (3 en buffer + 1 rendezvous)
// Solo el 5to send espera. Total: ~400ms
```

### 6.2 Transferencia directa (optimizacion del runtime)

Cuando hay un receiver esperando en un channel (unbuffered o buffered vacio), Go **copia los datos directamente** del stack del sender al stack del receiver, sin pasar por el buffer:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    OPTIMIZACION: TRANSFERENCIA DIRECTA                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  CASO NORMAL (buffer con espacio):                                      │
│                                                                          │
│  Sender ──copy──► Buffer ──copy──► Receiver                            │
│                   (2 copias)                                             │
│                                                                          │
│  CASO OPTIMIZADO (receiver ya esperando):                               │
│                                                                          │
│  Sender ──copy──────────────────► Receiver                              │
│                   (1 copia directa, bypass del buffer)                  │
│                                                                          │
│  El runtime encuentra al receiver en recvq (lista de espera),          │
│  lo despierta, y copia los datos directamente a su stack.              │
│                                                                          │
│  Esto aplica tanto a unbuffered como a buffered cuando                 │
│  el buffer esta vacio y hay un receiver bloqueado.                     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 7. Iteracion con range

`range` sobre un channel recibe valores hasta que el channel se cierra:

```go
package main

import "fmt"

// generador: produce numeros fibonacci hasta max
func fibonacci(max int, ch chan<- int) {
    a, b := 0, 1
    for a <= max {
        ch <- a
        a, b = b, a+b
    }
    close(ch) // IMPORTANTE: sin close, range se bloquea para siempre
}

func main() {
    ch := make(chan int, 10) // buffer para no bloquear el generador
    go fibonacci(100, ch)
    
    for v := range ch {
        fmt.Printf("%d ", v)
    }
    fmt.Println()
    // Output: 0 1 1 2 3 5 8 13 21 34 55 89
}
```

### 7.1 range vs comma-ok loop

```go
// FORMA 1: range (preferida cuando iteras todos los valores)
for v := range ch {
    process(v)
}

// FORMA 2: comma-ok loop (util cuando necesitas control fino)
for {
    v, ok := <-ch
    if !ok {
        break // channel cerrado
    }
    process(v)
}

// FORMA 3: receive con descarte (solo contar o sincronizar)
count := 0
for range ch {
    count++
}
```

### 7.2 Draining (vaciar un channel)

```go
// Vaciar un channel sin procesar los valores
// Util para limpieza o cancelacion
func drain(ch <-chan int) {
    for range ch {
        // descarta todos los valores hasta que se cierre
    }
}

// Vaciar con timeout
func drainWithTimeout(ch <-chan int, timeout time.Duration) {
    timer := time.NewTimer(timeout)
    defer timer.Stop()
    
    for {
        select {
        case _, ok := <-ch:
            if !ok {
                return // channel cerrado
            }
        case <-timer.C:
            return // timeout
        }
    }
}
```

---

## 8. Patrones Fundamentales con Channels

### 8.1 Done channel (senalizacion de completitud)

```go
package main

import (
    "fmt"
    "time"
)

func worker(id int, done chan<- struct{}) {
    fmt.Printf("Worker %d: trabajando...\n", id)
    time.Sleep(time.Duration(id) * 100 * time.Millisecond)
    fmt.Printf("Worker %d: listo\n", id)
    done <- struct{}{} // senal de completitud (zero-size, sin costo de memoria)
}

func main() {
    const numWorkers = 5
    done := make(chan struct{}, numWorkers) // buffered para no bloquear workers
    
    for i := 1; i <= numWorkers; i++ {
        go worker(i, done)
    }
    
    // Esperar a todos
    for i := 0; i < numWorkers; i++ {
        <-done
    }
    fmt.Println("Todos los workers terminaron")
}
```

### 8.2 Generator (funcion que retorna un channel)

```go
package main

import "fmt"

// counter retorna un channel que emite numeros secuenciales
func counter(start, end int) <-chan int {
    ch := make(chan int)
    go func() {
        for i := start; i <= end; i++ {
            ch <- i
        }
        close(ch)
    }()
    return ch // retorna receive-only channel
}

// squares toma un channel de ints y retorna sus cuadrados
func squares(in <-chan int) <-chan int {
    out := make(chan int)
    go func() {
        for v := range in {
            out <- v * v
        }
        close(out)
    }()
    return out
}

func main() {
    // Componer generadores: pipeline
    for v := range squares(counter(1, 10)) {
        fmt.Printf("%d ", v)
    }
    fmt.Println()
    // Output: 1 4 9 16 25 36 49 64 81 100
}
```

### 8.3 Request/Response

```go
package main

import "fmt"

// Request con channel de respuesta embebido
type Request struct {
    Args    []int
    ResultCh chan<- int // channel donde enviar la respuesta
}

func server(requests <-chan Request) {
    for req := range requests {
        // Procesar: sumar todos los args
        sum := 0
        for _, v := range req.Args {
            sum += v
        }
        req.ResultCh <- sum // enviar respuesta al solicitante
    }
}

func main() {
    requests := make(chan Request)
    go server(requests)
    
    // Cliente 1
    resp1 := make(chan int, 1) // buffered para no bloquear al server
    requests <- Request{Args: []int{1, 2, 3}, ResultCh: resp1}
    
    // Cliente 2
    resp2 := make(chan int, 1)
    requests <- Request{Args: []int{10, 20, 30}, ResultCh: resp2}
    
    fmt.Println("Resultado 1:", <-resp1) // 6
    fmt.Println("Resultado 2:", <-resp2) // 60
    
    close(requests)
}
```

### 8.4 Timeout con channel

```go
package main

import (
    "fmt"
    "time"
)

func slowOperation() <-chan string {
    ch := make(chan string, 1)
    go func() {
        time.Sleep(2 * time.Second)
        ch <- "resultado"
    }()
    return ch
}

func main() {
    // Esperar con timeout
    select {
    case result := <-slowOperation():
        fmt.Println("Resultado:", result)
    case <-time.After(1 * time.Second):
        fmt.Println("Timeout!")
    }
    // Output: Timeout! (porque la operacion tarda 2s y el timeout es 1s)
}
```

### 8.5 Channel de errores

```go
package main

import (
    "errors"
    "fmt"
)

type Result struct {
    Value int
    Err   error
}

func divide(a, b int, results chan<- Result) {
    if b == 0 {
        results <- Result{Err: errors.New("division by zero")}
        return
    }
    results <- Result{Value: a / b}
}

func main() {
    results := make(chan Result, 3)
    
    go divide(10, 2, results)
    go divide(20, 0, results)
    go divide(30, 5, results)
    
    for i := 0; i < 3; i++ {
        r := <-results
        if r.Err != nil {
            fmt.Printf("Error: %v\n", r.Err)
        } else {
            fmt.Printf("Resultado: %d\n", r.Value)
        }
    }
}
```

---

## 9. Antipatrones y Errores Comunes

### 9.1 Catalogo de errores

```go
// ============================================================
// ERROR 1: Send y receive en la misma goroutine (unbuffered)
// ============================================================
func bad1() {
    ch := make(chan int)
    ch <- 42    // DEADLOCK: bloquea aqui, nunca llega al receive
    fmt.Println(<-ch)
}

// FIX: usar goroutine separada o channel buffered
func good1a() {
    ch := make(chan int)
    go func() { ch <- 42 }()
    fmt.Println(<-ch)
}

func good1b() {
    ch := make(chan int, 1)
    ch <- 42 // no bloquea — cabe en el buffer
    fmt.Println(<-ch)
}

// ============================================================
// ERROR 2: Olvidar cerrar el channel (range bloquea para siempre)
// ============================================================
func bad2() {
    ch := make(chan int)
    go func() {
        for i := 0; i < 5; i++ {
            ch <- i
        }
        // Falta: close(ch) — el range de abajo nunca termina
    }()
    for v := range ch { // DEADLOCK cuando el producer termina
        fmt.Println(v)
    }
}

// FIX: cerrar el channel cuando terminas de enviar
func good2() {
    ch := make(chan int)
    go func() {
        for i := 0; i < 5; i++ {
            ch <- i
        }
        close(ch) // range terminara cuando vea el close
    }()
    for v := range ch {
        fmt.Println(v)
    }
}

// ============================================================
// ERROR 3: Enviar a channel cerrado
// ============================================================
func bad3() {
    ch := make(chan int, 1)
    close(ch)
    ch <- 42 // PANIC: send on closed channel
}

// FIX: solo el productor cierra, y solo cuando termina de enviar
func good3() {
    ch := make(chan int, 5)
    // El productor es el unico que cierra
    go func() {
        for i := 0; i < 5; i++ {
            ch <- i
        }
        close(ch) // cierra DESPUES de todos los sends
    }()
    for v := range ch {
        fmt.Println(v)
    }
}

// ============================================================
// ERROR 4: Cerrar channel dos veces
// ============================================================
func bad4() {
    ch := make(chan int)
    close(ch)
    close(ch) // PANIC: close of closed channel
}

// FIX: usar sync.Once si multiples paths podrian cerrar
func good4() {
    ch := make(chan int)
    var once sync.Once
    safeClose := func() {
        once.Do(func() { close(ch) })
    }
    safeClose()
    safeClose() // safe — no-op
}

// ============================================================
// ERROR 5: Goroutine leak — channel sin receiver
// ============================================================
func bad5() <-chan int {
    ch := make(chan int) // unbuffered
    go func() {
        result := expensiveComputation()
        ch <- result // Si nadie lee ch, esta goroutine NUNCA termina
        // Goroutine leak: permanece en memoria para siempre
    }()
    // Si el caller ignora el channel retornado, leak
    return ch
}

// FIX: usar context para cancelacion
func good5(ctx context.Context) <-chan int {
    ch := make(chan int, 1) // buffered 1 — si nadie lee, al menos no bloquea
    go func() {
        result := expensiveComputation()
        select {
        case ch <- result:
        case <-ctx.Done(): // si el caller cancela, la goroutine puede terminar
        }
    }()
    return ch
}

// ============================================================
// ERROR 6: Buffer demasiado grande como "solucion"
// ============================================================
func bad6() {
    // "Se me bloquea, le pongo buffer grande"
    ch := make(chan int, 1000000) // 1 millon de capacidad
    // Esto NO soluciona el problema — solo lo retrasa
    // y consume ~8MB de memoria
    // Eventualmente se llenara y se bloqueara igual
}

// FIX: entender POR QUE se bloquea y arreglar el consumer
func good6() {
    ch := make(chan int, 100) // buffer razonable para absorber rafagas
    // Con un consumer que procesa a ritmo adecuado
    go func() {
        for v := range ch {
            process(v) // si es lento, agregar mas workers
        }
    }()
}

// ============================================================
// ERROR 7: Leer de nil channel (bloqueo silencioso)
// ============================================================
func bad7() {
    var ch chan int // nil channel
    go func() {
        v := <-ch // BLOQUEA PARA SIEMPRE — no hay panic, solo deadlock
        fmt.Println(v)
    }()
    time.Sleep(time.Second)
    // La goroutine anterior nunca termina — goroutine leak
}

// FIX: siempre inicializar channels con make
func good7() {
    ch := make(chan int, 1)
    ch <- 42
    fmt.Println(<-ch)
}
```

### 9.2 Tabla de antipatrones

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ANTIPATRONES CON CHANNELS                             │
├───────────────────────────┬──────────────────────────────────────────────┤
│ Antipatron                 │ Consecuencia                                │
├───────────────────────────┼──────────────────────────────────────────────┤
│ Send/receive misma gor.   │ Deadlock (unbuffered)                       │
│ Olvidar close con range   │ Deadlock (range nunca termina)              │
│ Send a channel cerrado    │ Panic                                       │
│ Close channel 2 veces     │ Panic                                       │
│ Close desde el receiver   │ Race condition (sender puede enviar a       │
│                           │ channel cerrado → panic)                    │
│ Goroutine + send sin      │ Goroutine leak (permanece en memoria)       │
│ cancelacion               │                                             │
│ Buffer "infinito"         │ Consume memoria, esconde bugs               │
│ Nil channel sin intencion │ Bloqueo silencioso para siempre             │
│ len(ch) para decidir      │ Race condition (len puede cambiar entre     │
│ if/send                   │ la comprobacion y el uso)                   │
│ Channel como mutex        │ Funciona pero sync.Mutex es mas eficiente  │
└───────────────────────────┴──────────────────────────────────────────────┘
```

---

## 10. Nil Channel — Uso Intencionado

Un nil channel bloquea para siempre en send y receive. Esto **no siempre es un bug** — es una tecnica avanzada con `select`:

```go
package main

import "fmt"

func merge(ch1, ch2 <-chan int) <-chan int {
    out := make(chan int)
    go func() {
        defer close(out)
        for ch1 != nil || ch2 != nil {
            select {
            case v, ok := <-ch1:
                if !ok {
                    ch1 = nil // deshabilitar este case en el select
                    continue
                }
                out <- v
            case v, ok := <-ch2:
                if !ok {
                    ch2 = nil // deshabilitar este case en el select
                    continue
                }
                out <- v
            }
        }
    }()
    return out
}

func main() {
    ch1 := make(chan int, 3)
    ch2 := make(chan int, 3)
    
    ch1 <- 1; ch1 <- 3; ch1 <- 5; close(ch1)
    ch2 <- 2; ch2 <- 4; ch2 <- 6; close(ch2)
    
    for v := range merge(ch1, ch2) {
        fmt.Printf("%d ", v)
    }
    fmt.Println()
    // Output: valores intercalados de ambos channels
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    NIL CHANNEL EN SELECT                                  │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Un case con nil channel NUNCA se selecciona en un select.             │
│  Esto permite "desactivar" un case dinámicamente:                      │
│                                                                          │
│  select {                                                                │
│  case v := <-ch1:  // si ch1 == nil, este case se ignora              │
│  case v := <-ch2:  // si ch2 == nil, este case se ignora              │
│  }                                                                       │
│                                                                          │
│  PATRON: cuando un channel se cierra durante un merge,                 │
│  setearlo a nil para que el select deje de escucharlo                  │
│  sin necesidad de restructurar el loop.                                │
│                                                                          │
│  Es equivalente a "desconectar un cable" — el select                   │
│  simplemente deja de ver ese channel.                                  │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 11. Comparacion con C y Rust

### 11.1 C — pipes, sockets, o pthreads con queues

C no tiene channels nativos. Las alternativas son:

```c
// C: pipe() — comunicacion entre procesos (no threads)
#include <unistd.h>
#include <stdio.h>
#include <string.h>

int main() {
    int fd[2];
    pipe(fd);  // fd[0] = lectura, fd[1] = escritura
    
    pid_t pid = fork();
    if (pid == 0) {
        // Hijo: escribe
        close(fd[0]);
        const char *msg = "hello from child";
        write(fd[1], msg, strlen(msg) + 1);
        close(fd[1]);
    } else {
        // Padre: lee
        close(fd[1]);
        char buf[64];
        read(fd[0], buf, sizeof(buf));
        printf("Received: %s\n", buf);
        close(fd[0]);
    }
    return 0;
}

// LIMITACIONES vs Go channels:
// - pipe() es entre PROCESOS, no threads
// - Para threads necesitas implementar tu propia queue thread-safe:
//   mutex + condition variable + buffer circular
// - No hay tipado — todo es bytes
// - No hay "close" que notifique a todos los lectores
// - No hay select() multiplexando diferentes "channels"
//   (el select() de C es para file descriptors, no channels)
```

```c
// C: queue thread-safe manual (equivalente a buffered channel)
#include <pthread.h>
#include <stdlib.h>

typedef struct {
    int *buf;
    int cap, count, head, tail;
    pthread_mutex_t mu;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} chan_int;

chan_int *chan_new(int cap) {
    chan_int *ch = calloc(1, sizeof(chan_int));
    ch->buf = calloc(cap, sizeof(int));
    ch->cap = cap;
    pthread_mutex_init(&ch->mu, NULL);
    pthread_cond_init(&ch->not_full, NULL);
    pthread_cond_init(&ch->not_empty, NULL);
    return ch;
}

void chan_send(chan_int *ch, int val) {
    pthread_mutex_lock(&ch->mu);
    while (ch->count == ch->cap)
        pthread_cond_wait(&ch->not_full, &ch->mu);
    ch->buf[ch->tail] = val;
    ch->tail = (ch->tail + 1) % ch->cap;
    ch->count++;
    pthread_cond_signal(&ch->not_empty);
    pthread_mutex_unlock(&ch->mu);
}

int chan_recv(chan_int *ch) {
    pthread_mutex_lock(&ch->mu);
    while (ch->count == 0)
        pthread_cond_wait(&ch->not_empty, &ch->mu);
    int val = ch->buf[ch->head];
    ch->head = (ch->head + 1) % ch->cap;
    ch->count--;
    pthread_cond_signal(&ch->not_full);
    pthread_mutex_unlock(&ch->mu);
    return val;
}

// 40+ lineas para lo que Go hace en 1 linea:
// ch := make(chan int, cap)
// ch <- val
// val := <-ch
```

### 11.2 Rust — std::sync::mpsc y crossbeam/tokio channels

```rust
// Rust: std::sync::mpsc (multiple producer, single consumer)
use std::sync::mpsc;
use std::thread;

fn main() {
    // Channel unbuffered: mpsc::sync_channel(0)
    // Channel buffered:   mpsc::sync_channel(N) o mpsc::channel() (infinito)
    
    let (tx, rx) = mpsc::channel(); // unbounded (sin limite de buffer)
    
    thread::spawn(move || {
        tx.send(42).unwrap();
        // send() retorna Result — puede fallar si el receiver se dropeo
        // En Go, enviar cuando nadie escucha bloquea o panic (si cerrado)
    });
    
    let val = rx.recv().unwrap(); // bloquea hasta recibir
    println!("Received: {}", val);
    
    // DIFERENCIAS CON GO:
    // 1. mpsc::channel() es UNBOUNDED (buffer infinito) — Go no tiene esto
    // 2. mpsc::sync_channel(N) es bounded — equivalente a make(chan T, N)
    // 3. send() retorna Result<(), SendError> — compile-time safety
    // 4. Solo un receiver (mpsc = Multiple Producer, Single Consumer)
    // 5. Drop del receiver/sender cierra el channel automaticamente (RAII)
    // 6. No hay equivalente de close() — drop(tx) cierra implicitamente
}

// Rust con sync_channel (bounded, como Go buffered)
fn bounded_example() {
    let (tx, rx) = mpsc::sync_channel(3); // buffer de 3
    
    thread::spawn(move || {
        for i in 0..5 {
            tx.send(i).unwrap(); // bloquea en el 4to send (buffer lleno)
        }
        // tx se dropea aqui — cierra el channel automaticamente
    });
    
    // Iterar hasta que el channel se cierre
    for val in rx {  // rx implementa Iterator
        println!("{}", val);
    }
}

// Rust con crossbeam-channel (mas cercano a Go)
// crossbeam::channel::bounded/unbounded tienen:
// - Multiple producers Y multiple consumers (mpmc)
// - select! macro para multiplexar
// - try_send, try_recv
// - Timeout: recv_timeout, send_timeout

// Rust con tokio (async channels)
// tokio::sync::mpsc — async channel para el runtime async
// tokio::sync::broadcast — multiple consumers
// tokio::sync::oneshot — un solo valor (como request/response)
```

### 11.3 Tabla comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CHANNELS: GO vs C vs RUST                             │
├──────────────────┬─────────────────┬───────────────┬─────────────────────┤
│ Aspecto           │ C               │ Go            │ Rust               │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Primitiva         │ No hay          │ chan T         │ mpsc::channel()   │
│                   │ (implementar)   │ (built-in)    │ (stdlib)           │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Creacion          │ ~40 lineas      │ make(chan T)   │ mpsc::channel()   │
│                   │ (manual)        │ (1 linea)     │ (1 linea)          │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Tipado            │ bytes/void*     │ Tipado        │ Tipado + generics │
│                   │ (manual cast)   │ (generics)    │ (T: Send)          │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Buffer            │ Manual          │ make(ch T, N) │ sync_channel(N)   │
│                   │ (buf circular)  │               │ channel() unbounded│
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Cierre            │ Flag manual     │ close(ch)     │ Drop(tx)          │
│                   │ + cond_signal   │ (explicito)   │ (RAII, implicito)  │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Multiplexar       │ select() FDs    │ select {}     │ crossbeam select! │
│                   │ (no channels)   │ (built-in)    │ (crate externo)    │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Error en send     │ UB si no sync   │ Panic (closed)│ Result::Err       │
│                   │                 │ Block (nil)   │ (compile-time)     │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Multi-consumer    │ Manual          │ Si (MPMC)     │ No (MPSC)         │
│                   │                 │               │ crossbeam: MPMC    │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Garantia          │ Ninguna         │ Runtime       │ Compile-time      │
│                   │ (UB en races)   │ (race detect) │ (Send/Sync)        │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Range/Iterator    │ No              │ for v := range│ for val in rx     │
│                   │                 │ ch {}         │ (impl Iterator)    │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Syntax overhead   │ Alto (manual)   │ Bajo (← op)  │ Medio (.send()    │
│                   │                 │               │ .recv() .unwrap()) │
└──────────────────┴─────────────────┴───────────────┴─────────────────────┘
```

---

## 12. Programa Completo — Infrastructure Task Queue

Un sistema de cola de tareas para operaciones de infraestructura que demuestra channels unbuffered, buffered, senalizacion, generadores, y manejo de errores:

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

type Priority int

const (
    PriorityLow    Priority = 0
    PriorityNormal Priority = 1
    PriorityHigh   Priority = 2
)

func (p Priority) String() string {
    switch p {
    case PriorityLow:
        return "LOW"
    case PriorityNormal:
        return "NORMAL"
    case PriorityHigh:
        return "HIGH"
    default:
        return "UNKNOWN"
    }
}

type TaskType string

const (
    TaskDeploy     TaskType = "DEPLOY"
    TaskHealthCheck TaskType = "HEALTH_CHECK"
    TaskBackup     TaskType = "BACKUP"
    TaskRestart    TaskType = "RESTART"
    TaskScale      TaskType = "SCALE"
)

type Task struct {
    ID       int
    Type     TaskType
    Target   string
    Priority Priority
    Created  time.Time
}

type TaskResult struct {
    Task     Task
    Success  bool
    Message  string
    Duration time.Duration
    Worker   int
}

// ============================================================
// Task Generator (patron generator — retorna <-chan)
// ============================================================

func generateTasks(ctx context.Context, tasks []Task) <-chan Task {
    out := make(chan Task) // unbuffered — backpressure natural
    go func() {
        defer close(out)
        for _, t := range tasks {
            t.Created = time.Now()
            select {
            case out <- t:
            case <-ctx.Done():
                return
            }
        }
    }()
    return out
}

// ============================================================
// Priority Dispatcher (unbuffered → buffered por prioridad)
// ============================================================

type PriorityQueues struct {
    High   chan Task
    Normal chan Task
    Low    chan Task
}

func newPriorityQueues(bufSize int) *PriorityQueues {
    return &PriorityQueues{
        High:   make(chan Task, bufSize),
        Normal: make(chan Task, bufSize),
        Low:    make(chan Task, bufSize),
    }
}

func dispatch(ctx context.Context, in <-chan Task, queues *PriorityQueues) {
    defer close(queues.High)
    defer close(queues.Normal)
    defer close(queues.Low)

    for {
        select {
        case task, ok := <-in:
            if !ok {
                return
            }
            var target chan Task
            switch task.Priority {
            case PriorityHigh:
                target = queues.High
            case PriorityNormal:
                target = queues.Normal
            case PriorityLow:
                target = queues.Low
            }
            select {
            case target <- task:
            case <-ctx.Done():
                return
            }
        case <-ctx.Done():
            return
        }
    }
}

// ============================================================
// Worker Pool
// ============================================================

func worker(ctx context.Context, id int, tasks <-chan Task, results chan<- TaskResult) {
    for {
        select {
        case task, ok := <-tasks:
            if !ok {
                return
            }
            result := executeTask(id, task)
            select {
            case results <- result:
            case <-ctx.Done():
                return
            }
        case <-ctx.Done():
            return
        }
    }
}

func executeTask(workerID int, task Task) TaskResult {
    start := time.Now()

    // Simular ejecucion con duracion variable segun tipo
    var duration time.Duration
    switch task.Type {
    case TaskDeploy:
        duration = time.Duration(200+rand.Intn(300)) * time.Millisecond
    case TaskHealthCheck:
        duration = time.Duration(50+rand.Intn(100)) * time.Millisecond
    case TaskBackup:
        duration = time.Duration(300+rand.Intn(400)) * time.Millisecond
    case TaskRestart:
        duration = time.Duration(100+rand.Intn(200)) * time.Millisecond
    case TaskScale:
        duration = time.Duration(150+rand.Intn(250)) * time.Millisecond
    }
    time.Sleep(duration)

    // Simular fallo ocasional (10% chance)
    success := rand.Intn(10) > 0
    msg := fmt.Sprintf("%s on %s completed", task.Type, task.Target)
    if !success {
        msg = fmt.Sprintf("%s on %s FAILED: simulated error", task.Type, task.Target)
    }

    return TaskResult{
        Task:     task,
        Success:  success,
        Message:  msg,
        Duration: time.Since(start),
        Worker:   workerID,
    }
}

// ============================================================
// Merge channels (fan-in) — prioridad HIGH primero
// ============================================================

func mergeByPriority(ctx context.Context, queues *PriorityQueues) <-chan Task {
    out := make(chan Task)
    go func() {
        defer close(out)
        high := queues.High
        normal := queues.Normal
        low := queues.Low

        for high != nil || normal != nil || low != nil {
            // Intentar HIGH primero, luego NORMAL, luego LOW
            select {
            case t, ok := <-high:
                if !ok {
                    high = nil // nil channel — desactivar este case
                    continue
                }
                select {
                case out <- t:
                case <-ctx.Done():
                    return
                }
            case <-ctx.Done():
                return
            default:
                // No hay high, intentar normal
                select {
                case t, ok := <-normal:
                    if !ok {
                        normal = nil
                        continue
                    }
                    select {
                    case out <- t:
                    case <-ctx.Done():
                        return
                    }
                default:
                    // No hay normal, intentar low
                    select {
                    case t, ok := <-low:
                        if !ok {
                            low = nil
                            continue
                        }
                        select {
                        case out <- t:
                        case <-ctx.Done():
                            return
                        }
                    case <-ctx.Done():
                        return
                    default:
                        // Todas las colas vacias pero no cerradas,
                        // esperar a cualquiera
                        select {
                        case t, ok := <-high:
                            if !ok {
                                high = nil
                                continue
                            }
                            out <- t
                        case t, ok := <-normal:
                            if !ok {
                                normal = nil
                                continue
                            }
                            out <- t
                        case t, ok := <-low:
                            if !ok {
                                low = nil
                                continue
                            }
                            out <- t
                        case <-ctx.Done():
                            return
                        }
                    }
                }
            }
        }
    }()
    return out
}

// ============================================================
// Result Collector
// ============================================================

type Stats struct {
    Total     int
    Succeeded int
    Failed    int
    ByType    map[TaskType]int
    AvgTime   time.Duration
    mu        sync.Mutex
}

func collectResults(results <-chan TaskResult, done chan<- Stats) {
    stats := Stats{
        ByType: make(map[TaskType]int),
    }
    var totalDuration time.Duration

    for r := range results {
        stats.Total++
        if r.Success {
            stats.Succeeded++
        } else {
            stats.Failed++
        }
        stats.ByType[r.Task.Type]++
        totalDuration += r.Duration

        status := "OK"
        if !r.Success {
            status = "FAIL"
        }
        fmt.Printf("  [Worker-%d] [%s] [%s] Task #%d: %s → %s (%v)\n",
            r.Worker, r.Task.Priority, status, r.Task.ID,
            r.Task.Type, r.Task.Target, r.Duration.Round(time.Millisecond))
    }

    if stats.Total > 0 {
        stats.AvgTime = totalDuration / time.Duration(stats.Total)
    }
    done <- stats
}

// ============================================================
// Main — orquestar todo el pipeline
// ============================================================

func main() {
    ctx, cancel := context.WithTimeout(context.Background(), 30*time.Second)
    defer cancel()

    // Definir tareas
    tasks := []Task{
        {ID: 1, Type: TaskHealthCheck, Target: "api-server-1", Priority: PriorityHigh},
        {ID: 2, Type: TaskHealthCheck, Target: "api-server-2", Priority: PriorityHigh},
        {ID: 3, Type: TaskBackup, Target: "database-primary", Priority: PriorityNormal},
        {ID: 4, Type: TaskDeploy, Target: "web-frontend", Priority: PriorityNormal},
        {ID: 5, Type: TaskRestart, Target: "cache-redis", Priority: PriorityLow},
        {ID: 6, Type: TaskScale, Target: "worker-pool", Priority: PriorityHigh},
        {ID: 7, Type: TaskDeploy, Target: "api-gateway", Priority: PriorityNormal},
        {ID: 8, Type: TaskHealthCheck, Target: "db-replica-1", Priority: PriorityLow},
        {ID: 9, Type: TaskBackup, Target: "logs-volume", Priority: PriorityLow},
        {ID: 10, Type: TaskRestart, Target: "monitoring-agent", Priority: PriorityNormal},
        {ID: 11, Type: TaskScale, Target: "api-server", Priority: PriorityHigh},
        {ID: 12, Type: TaskDeploy, Target: "auth-service", Priority: PriorityHigh},
    }

    fmt.Println("╔══════════════════════════════════════════════════════════════╗")
    fmt.Println("║           INFRASTRUCTURE TASK QUEUE                          ║")
    fmt.Println("╠══════════════════════════════════════════════════════════════╣")
    fmt.Printf("║  Tasks: %d | Workers: 3 | Priority Queues: 3               ║\n", len(tasks))
    fmt.Println("╚══════════════════════════════════════════════════════════════╝")
    fmt.Println()

    // Pipeline:
    //
    //                          ┌─ [HIGH queue buf=5] ─┐
    // [Generator] → [Dispatch] ┤─ [NORM queue buf=5] ─├→ [Merge] → [Workers] → [Results]
    //                          └─ [LOW  queue buf=5] ─┘
    //

    // 1. Generar tareas (generator pattern)
    taskStream := generateTasks(ctx, tasks)

    // 2. Dispatcher: clasifica por prioridad (unbuffered → 3 buffered)
    queues := newPriorityQueues(5)
    go dispatch(ctx, taskStream, queues)

    // 3. Merge por prioridad (3 buffered → 1 unbuffered, HIGH primero)
    merged := mergeByPriority(ctx, queues)

    // 4. Worker pool (3 workers)
    const numWorkers = 3
    results := make(chan TaskResult, numWorkers) // buffered para no bloquear workers

    var wg sync.WaitGroup
    for i := 1; i <= numWorkers; i++ {
        wg.Add(1)
        go func(id int) {
            defer wg.Done()
            worker(ctx, id, merged, results)
        }(i)
    }

    // Cerrar results cuando todos los workers terminen
    go func() {
        wg.Wait()
        close(results)
    }()

    // 5. Recoger resultados
    statsCh := make(chan Stats, 1)
    go collectResults(results, statsCh)

    stats := <-statsCh

    // Imprimir resumen
    fmt.Println()
    fmt.Println("┌──────────────────────────────────────────┐")
    fmt.Println("│              RESULTS SUMMARY             │")
    fmt.Println("├──────────────────────────────────────────┤")
    fmt.Printf("│  Total tasks:    %d                      │\n", stats.Total)
    fmt.Printf("│  Succeeded:      %d                      │\n", stats.Succeeded)
    fmt.Printf("│  Failed:         %d                      │\n", stats.Failed)
    fmt.Printf("│  Avg duration:   %v                │\n", stats.AvgTime.Round(time.Millisecond))
    fmt.Println("├──────────────────────────────────────────┤")
    fmt.Println("│  By type:                                │")
    for taskType, count := range stats.ByType {
        padding := strings.Repeat(" ", 18-len(string(taskType)))
        fmt.Printf("│    %s:%s%d                        │\n", taskType, padding, count)
    }
    fmt.Println("└──────────────────────────────────────────┘")
}
```

### Ejecucion y explicacion

```bash
$ go run main.go

╔══════════════════════════════════════════════════════════════╗
║           INFRASTRUCTURE TASK QUEUE                          ║
╠══════════════════════════════════════════════════════════════╣
║  Tasks: 12 | Workers: 3 | Priority Queues: 3               ║
╚══════════════════════════════════════════════════════════════╝

  [Worker-1] [HIGH] [OK] Task #1: HEALTH_CHECK → api-server-1 (89ms)
  [Worker-2] [HIGH] [OK] Task #2: HEALTH_CHECK → api-server-2 (112ms)
  [Worker-3] [HIGH] [OK] Task #6: SCALE → worker-pool (284ms)
  [Worker-1] [HIGH] [OK] Task #11: SCALE → api-server (195ms)
  [Worker-2] [HIGH] [OK] Task #12: DEPLOY → auth-service (341ms)
  [Worker-3] [NORMAL] [OK] Task #3: BACKUP → database-primary (523ms)
  [Worker-1] [NORMAL] [OK] Task #4: DEPLOY → web-frontend (267ms)
  [Worker-2] [NORMAL] [FAIL] Task #7: DEPLOY → api-gateway (389ms)
  [Worker-1] [NORMAL] [OK] Task #10: RESTART → monitoring-agent (142ms)
  [Worker-3] [LOW] [OK] Task #5: RESTART → cache-redis (178ms)
  [Worker-2] [LOW] [OK] Task #8: HEALTH_CHECK → db-replica-1 (73ms)
  [Worker-1] [LOW] [OK] Task #9: BACKUP → logs-volume (456ms)

┌──────────────────────────────────────────┐
│              RESULTS SUMMARY             │
├──────────────────────────────────────────┤
│  Total tasks:    12                      │
│  Succeeded:      11                      │
│  Failed:         1                       │
│  Avg duration:   254ms                   │
├──────────────────────────────────────────┤
│  By type:                                │
│    HEALTH_CHECK:    3                    │
│    DEPLOY:          3                    │
│    BACKUP:          2                    │
│    RESTART:         2                    │
│    SCALE:           2                    │
└──────────────────────────────────────────┘
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    FLUJO DEL PIPELINE                                     │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Channels usados y su tipo:                                             │
│                                                                          │
│  1. taskStream (unbuffered)                                              │
│     ├─ Generator → Dispatcher                                           │
│     └─ Backpressure: generator se frena si dispatcher esta ocupado     │
│                                                                          │
│  2. queues.High/Normal/Low (buffered, cap=5)                            │
│     ├─ Dispatcher → Merger                                              │
│     └─ Buffer absorbe rafagas: dispatcher clasifica rapido,            │
│        merger consume segun prioridad                                   │
│                                                                          │
│  3. merged (unbuffered)                                                  │
│     ├─ Merger → Workers                                                 │
│     └─ Los workers "jalan" tareas a su ritmo                           │
│                                                                          │
│  4. results (buffered, cap=3)                                           │
│     ├─ Workers → Collector                                              │
│     └─ Buffer = numWorkers: ningun worker se bloquea esperando         │
│        al collector                                                     │
│                                                                          │
│  5. statsCh (buffered, cap=1)                                           │
│     ├─ Collector → Main                                                 │
│     └─ Un solo resultado final                                          │
│                                                                          │
│  Patrones demostrados:                                                   │
│  ├─ Generator (generateTasks retorna <-chan Task)                       │
│  ├─ Fan-out (dispatcher → 3 queues)                                    │
│  ├─ Fan-in (3 queues → merged)                                         │
│  ├─ Worker pool (3 workers comparten merged)                           │
│  ├─ Done channel (wg.Wait() + close(results))                         │
│  ├─ Nil channel (merge desactiva colas cerradas)                       │
│  ├─ Context cancelation (timeout de 30s)                               │
│  └─ Result collection (struct con Value + Error)                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 13. Ejercicios

### Ejercicio 1: Pipeline de procesamiento de logs

Implementa un pipeline con 3 etapas conectadas por channels:

```
[Reader] ──unbuffered──► [Parser] ──buffered(50)──► [Writer]
```

- **Reader**: genera lineas de log simuladas (timestamp + level + message)
- **Parser**: filtra solo lineas con level ERROR o WARN, extrae campos
- **Writer**: formatea y imprime las lineas filtradas con color

Requisitos:
- Usa channels unbuffered entre reader y parser (backpressure)
- Usa channel buffered(50) entre parser y writer (parser es rapido, writer lento)
- Cierra los channels correctamente para que range funcione
- Usa `context.WithTimeout` para limitar a 5 segundos

### Ejercicio 2: Rate limiter con token bucket

Implementa un rate limiter usando un channel buffered como token bucket:

```go
// El channel buffered ES el bucket
tokens := make(chan struct{}, maxTokens)
```

- Un goroutine "refiller" envia tokens al channel cada N milisegundos
- Los request handlers deben recibir un token antes de proceder
- Si no hay token disponible, el handler espera (con timeout)
- Imprime estadisticas de tokens usados y requests rechazados

### Ejercicio 3: Health checker con resultados por channel

Implementa un health checker que verifica N servicios en paralelo:

- Cada servicio es una goroutine que envia su resultado por un channel buffered(N)
- Los resultados incluyen: nombre del servicio, estado (healthy/unhealthy), latencia
- Main recibe todos los resultados y genera un reporte
- Usa un channel `chan struct{}` para senalizar que el reporte esta completo
- Agrega timeout: si un servicio no responde en 3 segundos, reportar como unhealthy

### Ejercicio 4: Broadcast con channels

Implementa un sistema de broadcast donde un sender puede enviar a multiples receivers:

```go
type Broadcaster struct {
    subscribers []chan string
}
```

- `Subscribe()` retorna un `<-chan string` nuevo
- `Publish(msg)` envia el mensaje a TODOS los subscribers
- `Close()` cierra todos los channels de subscribers
- Maneja el caso de subscriber lento (buffered + drop si lleno)
- Demuestra con 5 subscribers que reciben 10 mensajes

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    CHANNELS BASICOS — RESUMEN COMPLETO                   │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  CREACION:                                                               │
│  ├─ make(chan T)      → unbuffered (cap=0, rendezvous)                 │
│  ├─ make(chan T, N)   → buffered (cap=N, cola FIFO)                    │
│  └─ var ch chan T     → nil (BLOQUEA siempre, util en select)          │
│                                                                          │
│  OPERACIONES:                                                            │
│  ├─ ch <- val         → send (bloquea si buffer lleno o unbuffered)    │
│  ├─ val := <-ch       → receive (bloquea si buffer vacio)              │
│  ├─ val, ok := <-ch   → receive con deteccion de close                 │
│  ├─ close(ch)         → cierra (solo el sender, NUNCA el receiver)     │
│  ├─ for v := range ch → itera hasta close                              │
│  ├─ len(ch)           → elementos en buffer                            │
│  └─ cap(ch)           → capacidad del buffer                           │
│                                                                          │
│  UNBUFFERED (cap=0):                                                     │
│  ├─ Sincronizacion punto a punto (handshake)                           │
│  ├─ Garantia de entrega (cuando send retorna, receiver tiene el valor) │
│  ├─ Backpressure inmediata                                              │
│  └─ Usar para: senalizacion, handoff, request/response                 │
│                                                                          │
│  BUFFERED (cap=N):                                                       │
│  ├─ Cola FIFO con capacidad limitada                                   │
│  ├─ Send no bloquea si hay espacio                                     │
│  ├─ Desacopla timing de producer/consumer                              │
│  └─ Usar para: work queues, semaforos, absorber rafagas                │
│                                                                          │
│  REGLAS:                                                                 │
│  ├─ Solo el SENDER cierra                                               │
│  ├─ Send a closed channel → PANIC                                      │
│  ├─ Close de closed channel → PANIC                                    │
│  ├─ Receive de closed channel → zero value + ok=false                  │
│  ├─ Nil channel bloquea siempre (util para desactivar select cases)    │
│  └─ "Si no puedes justificar el tamano del buffer, usa unbuffered"     │
│                                                                          │
│  PATRONES:                                                               │
│  ├─ Done channel: make(chan struct{}), close(done) para broadcast      │
│  ├─ Generator: funcion retorna <-chan T                                 │
│  ├─ Request/Response: struct con ResultCh chan<- T                     │
│  ├─ Semaforo: make(chan struct{}, N) para limitar concurrencia          │
│  ├─ Result+Error: struct { Value T; Err error }                        │
│  └─ Nil channel trick: desactivar case en select                       │
│                                                                          │
│  vs C: no hay channels (40+ lineas para implementar uno manual)        │
│  vs Rust: mpsc stdlib (MPSC), crossbeam (MPMC), Drop cierra auto      │
│  Go: channels first-class, MPMC, select built-in, <- operador          │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S02 - Channels, T02 - Direccion de channels — chan<- (send-only), <-chan (receive-only), cuando anotar direccion
