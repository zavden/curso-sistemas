# select — Multiplexacion de Channels, default case, timeout con time.After

## 1. Introduccion

`select` es la sentencia de control mas importante para concurrencia en Go. Permite a una goroutine **esperar simultaneamente en multiples operaciones de channels** — sends y receives — y ejecutar la primera que este lista. Es el equivalente concurrente de `switch`, pero en lugar de evaluar expresiones, evalúa la **disponibilidad de comunicacion** en channels.

Sin `select`, una goroutine solo puede esperar en un channel a la vez. Con `select`, puede escuchar decenas de channels simultaneamente, implementar timeouts, polling no-bloqueante, cancelacion, y multiplexacion — todo con una sola sentencia.

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    SELECT EN GO                                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  select es como un switch, pero para channels:                         │
│                                                                          │
│  switch evalua:        EXPRESIONES (valores)                           │
│  select evalua:        CHANNELS (disponibilidad de comunicacion)       │
│                                                                          │
│  switch {                        select {                               │
│  case x > 0:                     case v := <-ch1:                      │
│      // x es positivo                // ch1 tenia un valor             │
│  case x < 0:                     case ch2 <- val:                      │
│      // x es negativo                // ch2 acepto el envio            │
│  default:                        default:                               │
│      // x es 0                       // ningun channel listo           │
│  }                               }                                      │
│                                                                          │
│  DIFERENCIAS CLAVE:                                                     │
│  ├─ switch evalua secuencialmente (primer case true gana)              │
│  ├─ select evalua TODOS simultaneamente                                │
│  ├─ Si multiples cases estan listos, elige uno AL AZAR                │
│  ├─ Sin default, select BLOQUEA hasta que algun case este listo       │
│  └─ Con default, select NUNCA bloquea                                  │
│                                                                          │
│  USOS FUNDAMENTALES:                                                     │
│  ├─ Multiplexar multiples channels                                     │
│  ├─ Timeout (time.After, time.NewTimer)                                │
│  ├─ Cancelacion (context.Done())                                       │
│  ├─ Non-blocking send/receive (default)                                │
│  ├─ Polling periodico (for + select + time.Ticker)                    │
│  └─ Heartbeat / keepalive                                              │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

Para SysAdmin/DevOps, `select` es omnipresente:
- Todo servidor Go que maneja shutdown graceful usa `select` con context/signal
- Los health checkers, watchers, y controllers de Kubernetes usan `for { select {} }` como loop principal
- Load balancers y proxies multiplexan conexiones con `select`
- Timeouts en operaciones de red, DNS, y llamadas a APIs se implementan con `select` + `time.After`
- Monitoring y alerting usan `select` con tickers para polling periodico

---

## 2. Sintaxis Completa

### 2.1 Forma basica

```go
select {
case v := <-ch1:
    // Se recibio v de ch1
case v, ok := <-ch2:
    // Se recibio v de ch2, ok indica si ch2 esta abierto
case ch3 <- value:
    // Se envio value a ch3
default:
    // Ningun channel estaba listo (opcional)
}
```

### 2.2 Reglas de evaluacion

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    REGLAS DE EVALUACION DE SELECT                        │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. TODOS los case se evaluan simultaneamente                          │
│     No hay prioridad — no importa el orden en el codigo               │
│                                                                          │
│  2. Si NINGUN case esta listo:                                         │
│     ├─ Sin default: BLOQUEA hasta que alguno este listo               │
│     └─ Con default: ejecuta default inmediatamente                     │
│                                                                          │
│  3. Si UN case esta listo:                                              │
│     └─ Ejecuta ese case                                                │
│                                                                          │
│  4. Si MULTIPLES cases estan listos:                                   │
│     └─ Elige uno AL AZAR (uniform random)                              │
│        Esto es intencional — previene starvation                       │
│        NO hay prioridad por orden de aparicion                         │
│                                                                          │
│  5. Evaluacion de expresiones en los case:                             │
│     ├─ Las expresiones del lado DERECHO se evaluan ANTES del select   │
│     ├─ ch3 <- f()  → f() se evalua UNA VEZ antes del select          │
│     └─ Los receives del lado izquierdo solo se asignan si se eligen   │
│                                                                          │
│  6. select {} (sin cases):                                              │
│     └─ BLOQUEA PARA SIEMPRE — util para mantener main vivo            │
│                                                                          │
│  7. select con un solo case (sin default):                             │
│     └─ Es equivalente a la operacion directa del channel              │
│        select { case v := <-ch: } ≡ v := <-ch                         │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 2.3 Evaluacion de expresiones en cases

```go
package main

import "fmt"

func sideEffect(name string) int {
    fmt.Printf("  evaluando %s\n", name)
    return 42
}

func getChannel(name string) chan int {
    fmt.Printf("  evaluando channel %s\n", name)
    ch := make(chan int, 1)
    ch <- 1
    return ch
}

func main() {
    ch := make(chan int, 1)
    ch <- 0
    
    fmt.Println("Antes del select:")
    
    // TODAS las expresiones se evaluan ANTES de que select elija un case
    select {
    case <-getChannel("A"):     // getChannel("A") se evalua siempre
        fmt.Println("Case A")
    case ch <- sideEffect("B"): // sideEffect("B") se evalua siempre
        fmt.Println("Case B")
    }
    
    // Output:
    // Antes del select:
    //   evaluando channel A
    //   evaluando B
    // Case A  (o Case B — aleatorio)
    
    // IMPORTANTE: ambas expresiones se evaluaron aunque solo un case se ejecuto
}
```

---

## 3. Select sin default (bloqueante)

Sin `default`, `select` bloquea hasta que al menos un case este listo. Este es el uso mas comun.

### 3.1 Multiplexar dos channels

```go
package main

import (
    "fmt"
    "time"
)

func producer(name string, interval time.Duration) <-chan string {
    ch := make(chan string)
    go func() {
        defer close(ch)
        for i := 1; i <= 5; i++ {
            time.Sleep(interval)
            ch <- fmt.Sprintf("[%s] mensaje %d", name, i)
        }
    }()
    return ch
}

func main() {
    fast := producer("rapido", 100*time.Millisecond)
    slow := producer("lento", 300*time.Millisecond)
    
    // Multiplexar: recibir de ambos, procesando el que este listo primero
    for fast != nil || slow != nil {
        select {
        case msg, ok := <-fast:
            if !ok {
                fast = nil // desactivar este case (nil channel nunca esta listo)
                continue
            }
            fmt.Println(msg)
        case msg, ok := <-slow:
            if !ok {
                slow = nil
                continue
            }
            fmt.Println(msg)
        }
    }
    fmt.Println("Ambos producers terminaron")
}
```

### 3.2 Send y receive mezclados

```go
package main

import "fmt"

func main() {
    ch1 := make(chan int, 1)
    ch2 := make(chan int, 1)
    
    ch1 <- 42
    
    // select puede mezclar sends y receives
    select {
    case v := <-ch1:
        fmt.Println("Recibido de ch1:", v)  // probablemente este
    case ch2 <- 99:
        fmt.Println("Enviado a ch2")        // o este — ambos listos, aleatorio
    }
}
```

### 3.3 Seleccion aleatoria (fairness)

```go
package main

import "fmt"

func main() {
    ch1 := make(chan string, 1)
    ch2 := make(chan string, 1)
    
    counts := map[string]int{"ch1": 0, "ch2": 0}
    
    for i := 0; i < 10000; i++ {
        ch1 <- "ch1"
        ch2 <- "ch2"
        
        select {
        case v := <-ch1:
            counts[v]++
        case v := <-ch2:
            counts[v]++
        }
        
        // Vaciar el channel no elegido
        select {
        case <-ch1:
        case <-ch2:
        }
    }
    
    fmt.Printf("ch1: %d veces (%.1f%%)\n", counts["ch1"], float64(counts["ch1"])/100.0)
    fmt.Printf("ch2: %d veces (%.1f%%)\n", counts["ch2"], float64(counts["ch2"])/100.0)
    
    // Output tipico:
    // ch1: 5023 veces (50.2%)
    // ch2: 4977 veces (49.8%)
    // 
    // Aproximadamente 50/50 — seleccion uniforme, sin prioridad
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    POR QUE SELECCION ALEATORIA                          │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  Si select eligiera siempre el PRIMER case listo:                      │
│                                                                          │
│  select {                                                                │
│  case <-highTraffic:   // siempre tendria datos                        │
│      // SIEMPRE se eligiria este                                        │
│  case <-lowTraffic:    // rara vez se eligiria                         │
│      // STARVATION — nunca se procesa                                  │
│  }                                                                       │
│                                                                          │
│  Con seleccion aleatoria, ambos channels tienen chance justa.          │
│                                                                          │
│  NOTA: si necesitas prioridad, debes implementarla manualmente:        │
│                                                                          │
│  // Primero intentar high priority (sin bloquear)                      │
│  select {                                                                │
│  case v := <-highPriority:                                              │
│      process(v)                                                          │
│  default:                                                                │
│      // No hay high priority, aceptar cualquiera                       │
│      select {                                                            │
│      case v := <-highPriority:                                          │
│          process(v)                                                      │
│      case v := <-lowPriority:                                           │
│          process(v)                                                      │
│      }                                                                   │
│  }                                                                       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 4. Select con default (non-blocking)

`default` hace que `select` nunca bloquee. Si ningun case esta listo, ejecuta `default` inmediatamente.

### 4.1 Non-blocking receive

```go
package main

import "fmt"

func main() {
    ch := make(chan int, 1)
    
    // Intentar recibir sin bloquear
    select {
    case v := <-ch:
        fmt.Println("Recibido:", v)
    default:
        fmt.Println("No hay datos disponibles")
    }
    // Output: No hay datos disponibles
    
    ch <- 42
    
    // Ahora si hay datos
    select {
    case v := <-ch:
        fmt.Println("Recibido:", v)
    default:
        fmt.Println("No hay datos disponibles")
    }
    // Output: Recibido: 42
}
```

### 4.2 Non-blocking send

```go
package main

import "fmt"

func main() {
    ch := make(chan int, 1)
    ch <- 1 // buffer lleno
    
    // Intentar enviar sin bloquear
    select {
    case ch <- 2:
        fmt.Println("Enviado")
    default:
        fmt.Println("Channel lleno, descartando")
    }
    // Output: Channel lleno, descartando
    
    <-ch // vaciar el buffer
    
    select {
    case ch <- 2:
        fmt.Println("Enviado")
    default:
        fmt.Println("Channel lleno, descartando")
    }
    // Output: Enviado
}
```

### 4.3 Try-send pattern (descarte cuando lleno)

```go
package main

import (
    "fmt"
    "time"
)

// trySend intenta enviar sin bloquear — descarta si el receiver es lento
func trySend[T any](ch chan<- T, val T) bool {
    select {
    case ch <- val:
        return true // enviado
    default:
        return false // descartado
    }
}

func main() {
    // Channel con buffer pequeno — los eventos viejos se descartan
    events := make(chan string, 3)
    
    sent := 0
    dropped := 0
    
    for i := 1; i <= 10; i++ {
        msg := fmt.Sprintf("event-%d", i)
        if trySend(events, msg) {
            sent++
        } else {
            dropped++
            fmt.Printf("  DROPPED: %s\n", msg)
        }
    }
    
    fmt.Printf("\nEnviados: %d, Descartados: %d\n", sent, dropped)
    fmt.Println("Eventos en buffer:")
    close(events)
    for e := range events {
        fmt.Printf("  %s\n", e)
    }
}
```

### 4.4 Polling loop (busy-wait controlado)

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    results := make(chan string, 1)
    
    // Simular operacion asincrona
    go func() {
        time.Sleep(500 * time.Millisecond)
        results <- "completado"
    }()
    
    // Polling: hacer trabajo util mientras esperas
    ticker := time.NewTicker(100 * time.Millisecond)
    defer ticker.Stop()
    
    for {
        select {
        case result := <-results:
            fmt.Printf("\nResultado: %s\n", result)
            return
        default:
            // No esta listo — hacer otra cosa
            fmt.Print(".")
            <-ticker.C // esperar antes de re-check
        }
    }
    // Output: .....
    // Resultado: completado
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    DEFAULT — CUIDADO CON BUSY-WAIT                      │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  for {                                                                   │
│      select {                                                            │
│      case v := <-ch:                                                    │
│          process(v)                                                      │
│      default:                                                            │
│          // ¡PELIGRO! Esto es un BUSY-WAIT                              │
│          // El CPU gira al 100% haciendo nada util                     │
│      }                                                                   │
│  }                                                                       │
│                                                                          │
│  SOLUCION 1: Agregar sleep o ticker en default                         │
│  default:                                                                │
│      time.Sleep(10 * time.Millisecond) // cede el CPU                  │
│                                                                          │
│  SOLUCION 2: Quitar default (dejar que select bloquee)                 │
│  for {                                                                   │
│      select {                                                            │
│      case v := <-ch:                                                    │
│          process(v)                                                      │
│      case <-done:                                                        │
│          return                                                          │
│      }                                                                   │
│  }                                                                       │
│                                                                          │
│  REGLA: solo usa default cuando realmente quieres non-blocking.        │
│  En un loop, casi siempre necesitas un ticker o timer en su lugar.     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 5. Timeout con time.After y time.NewTimer

### 5.1 time.After — timeout simple

`time.After(d)` retorna un `<-chan time.Time` que recibe un valor despues de la duracion `d`:

```go
package main

import (
    "fmt"
    "time"
)

func slowOperation() <-chan string {
    ch := make(chan string, 1)
    go func() {
        time.Sleep(2 * time.Second) // operacion lenta
        ch <- "resultado de operacion lenta"
    }()
    return ch
}

func main() {
    fmt.Println("Esperando resultado con timeout de 1s...")
    
    select {
    case result := <-slowOperation():
        fmt.Println("Resultado:", result)
    case <-time.After(1 * time.Second):
        fmt.Println("TIMEOUT: la operacion tardo demasiado")
    }
    // Output: TIMEOUT: la operacion tardo demasiado
    
    fmt.Println("\nEsperando resultado con timeout de 3s...")
    
    select {
    case result := <-slowOperation():
        fmt.Println("Resultado:", result)
    case <-time.After(3 * time.Second):
        fmt.Println("TIMEOUT: la operacion tardo demasiado")
    }
    // Output: Resultado: resultado de operacion lenta
}
```

### 5.2 Timeout por item vs timeout total

```go
package main

import (
    "fmt"
    "math/rand"
    "time"
)

func source() <-chan int {
    ch := make(chan int)
    go func() {
        defer close(ch)
        for i := 1; i <= 10; i++ {
            delay := time.Duration(rand.Intn(800)) * time.Millisecond
            time.Sleep(delay)
            ch <- i
        }
    }()
    return ch
}

func main() {
    ch := source()
    
    // ============================================
    // TIMEOUT POR ITEM: cada receive tiene su propio timeout
    // Si un item tarda mas de 500ms, timeout
    // ============================================
    fmt.Println("=== Timeout por item (500ms) ===")
    for {
        select {
        case v, ok := <-ch:
            if !ok {
                fmt.Println("Source cerrada")
                goto totalTimeout
            }
            fmt.Printf("Recibido: %d\n", v)
        case <-time.After(500 * time.Millisecond):
            // NOTA: time.After crea un NUEVO timer en CADA iteracion
            // Esto puede causar memory leak si el loop es muy rapido
            fmt.Println("Timeout esperando item")
            goto totalTimeout
        }
    }

totalTimeout:
    // ============================================
    // TIMEOUT TOTAL: un solo timeout para toda la operacion
    // ============================================
    fmt.Println("\n=== Timeout total (2s) ===")
    ch2 := source()
    deadline := time.After(2 * time.Second) // UN solo timer
    
    for {
        select {
        case v, ok := <-ch2:
            if !ok {
                fmt.Println("Source cerrada (completo)")
                return
            }
            fmt.Printf("Recibido: %d\n", v)
        case <-deadline:
            fmt.Println("TIMEOUT TOTAL: 2 segundos alcanzados")
            return
        }
    }
}
```

### 5.3 time.NewTimer — reutilizable y cancelable

`time.After` crea un timer que no se puede cancelar ni reutilizar. Para performance en loops, usar `time.NewTimer`:

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    ch := make(chan string, 1)
    
    // Simular mensajes esporadicos
    go func() {
        delays := []time.Duration{100, 200, 800, 50, 100}
        for i, d := range delays {
            time.Sleep(d * time.Millisecond)
            ch <- fmt.Sprintf("msg-%d", i+1)
        }
        close(ch)
    }()
    
    // time.NewTimer es mas eficiente que time.After en loops
    // porque puedes Reset() en lugar de crear un nuevo timer
    timer := time.NewTimer(500 * time.Millisecond)
    defer timer.Stop() // SIEMPRE hacer Stop() para liberar recursos
    
    for {
        // Reset timer para cada iteracion
        if !timer.Stop() {
            // Drenar el timer si ya disparo
            select {
            case <-timer.C:
            default:
            }
        }
        timer.Reset(500 * time.Millisecond)
        
        select {
        case msg, ok := <-ch:
            if !ok {
                fmt.Println("Source cerrada")
                return
            }
            fmt.Printf("Recibido: %s\n", msg)
        case <-timer.C:
            fmt.Println("Timeout: 500ms sin actividad")
            return
        }
    }
}
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    time.After vs time.NewTimer                           │
├──────────────────┬───────────────────────┬───────────────────────────────┤
│ Aspecto           │ time.After(d)         │ time.NewTimer(d)             │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Retorna           │ <-chan time.Time      │ *time.Timer                  │
│                   │ (solo el channel)     │ (struct con .C channel)      │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Cancelable        │ No                    │ Si (.Stop())                 │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Reutilizable      │ No                    │ Si (.Reset(d))               │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Memory leak       │ Si, en loops rapidos  │ No (si haces Stop)           │
│                   │ (cada llamada crea un │                              │
│                   │  timer que no se GC   │                              │
│                   │  hasta que dispara)   │                              │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Uso ideal         │ Select unico          │ Loops con timeout repetido   │
│                   │ (timeout simple)      │ (reset en cada iteracion)    │
├──────────────────┼───────────────────────┼───────────────────────────────┤
│ Idiomático        │ select {              │ timer := time.NewTimer(d)    │
│                   │   case <-time.After(d)│ defer timer.Stop()           │
│                   │ }                     │ select { case <-timer.C: }   │
└──────────────────┴───────────────────────┴───────────────────────────────┘
```

---

## 6. Select con time.Ticker (polling periodico)

`time.Ticker` emite en un channel a intervalos regulares. Combinado con `select`, crea loops de polling o heartbeat:

### 6.1 Polling basico

```go
package main

import (
    "fmt"
    "math/rand"
    "time"
)

type ServiceStatus struct {
    Name    string
    Healthy bool
    Latency time.Duration
}

func checkService(name string) ServiceStatus {
    latency := time.Duration(10+rand.Intn(90)) * time.Millisecond
    healthy := rand.Intn(10) > 0 // 90% healthy
    return ServiceStatus{Name: name, Healthy: healthy, Latency: latency}
}

func main() {
    ticker := time.NewTicker(1 * time.Second)
    defer ticker.Stop()
    
    done := make(chan struct{})
    go func() {
        time.Sleep(5 * time.Second)
        close(done)
    }()
    
    fmt.Println("Health checker iniciado (cada 1s, durante 5s)")
    
    // Check inmediato + periodico
    services := []string{"api", "database", "cache"}
    check := func() {
        for _, svc := range services {
            status := checkService(svc)
            icon := "OK"
            if !status.Healthy {
                icon = "FAIL"
            }
            fmt.Printf("  [%s] %-10s %v\n", icon, status.Name, status.Latency)
        }
        fmt.Println()
    }
    
    check() // check inmediato
    
    for {
        select {
        case <-ticker.C:
            check()
        case <-done:
            fmt.Println("Health checker detenido")
            return
        }
    }
}
```

### 6.2 Heartbeat (mantener conexion viva)

```go
package main

import (
    "fmt"
    "time"
)

// workerWithHeartbeat envia heartbeats mientras trabaja
func workerWithHeartbeat(done <-chan struct{}) (<-chan string, <-chan struct{}) {
    results := make(chan string)
    heartbeat := make(chan struct{}, 1) // buffered — no bloquear al worker
    
    go func() {
        defer close(results)
        defer close(heartbeat)
        
        ticker := time.NewTicker(500 * time.Millisecond)
        defer ticker.Stop()
        
        tasks := []string{"compilar", "testear", "desplegar", "verificar"}
        
        for _, task := range tasks {
            // Simular trabajo
            workTime := 800 * time.Millisecond
            workDone := time.After(workTime)
            
        workLoop:
            for {
                select {
                case <-done:
                    return
                case <-ticker.C:
                    // Enviar heartbeat (non-blocking)
                    select {
                    case heartbeat <- struct{}{}:
                    default:
                    }
                case <-workDone:
                    results <- fmt.Sprintf("completado: %s", task)
                    break workLoop
                }
            }
        }
    }()
    
    return results, heartbeat
}

func main() {
    done := make(chan struct{})
    results, heartbeat := workerWithHeartbeat(done)
    
    // Timeout si no recibimos heartbeat en 2 segundos
    timeout := 2 * time.Second
    
    for {
        select {
        case result, ok := <-results:
            if !ok {
                fmt.Println("\nWorker termino todos los tasks")
                return
            }
            fmt.Printf("  RESULTADO: %s\n", result)
        case <-heartbeat:
            fmt.Print("♥ ") // worker sigue vivo
        case <-time.After(timeout):
            fmt.Println("\nWorker no responde — cerrando")
            close(done)
            return
        }
    }
}
```

---

## 7. Select con Context (cancelacion)

El patron mas importante en Go production code: `select` con `ctx.Done()` para cancelacion cooperativa.

### 7.1 Cancelacion basica

```go
package main

import (
    "context"
    "fmt"
    "time"
)

func worker(ctx context.Context, id int, results chan<- string) {
    for i := 1; ; i++ {
        // Simular trabajo
        workTime := time.Duration(100+id*50) * time.Millisecond
        
        select {
        case <-ctx.Done():
            fmt.Printf("Worker %d: cancelado (%v)\n", id, ctx.Err())
            return
        case <-time.After(workTime):
            select {
            case results <- fmt.Sprintf("Worker %d: item %d", id, i):
            case <-ctx.Done():
                return
            }
        }
    }
}

func main() {
    // Cancelar despues de 1 segundo
    ctx, cancel := context.WithTimeout(context.Background(), 1*time.Second)
    defer cancel()
    
    results := make(chan string, 10)
    
    for i := 1; i <= 3; i++ {
        go worker(ctx, i, results)
    }
    
    for {
        select {
        case result := <-results:
            fmt.Printf("  %s\n", result)
        case <-ctx.Done():
            fmt.Printf("\nContexto cancelado: %v\n", ctx.Err())
            // Dar tiempo a workers para cleanup
            time.Sleep(100 * time.Millisecond)
            return
        }
    }
}
```

### 7.2 Select con signal de OS (shutdown graceful)

```go
package main

import (
    "context"
    "fmt"
    "os"
    "os/signal"
    "syscall"
    "time"
)

func server(ctx context.Context) {
    ticker := time.NewTicker(1 * time.Second)
    defer ticker.Stop()
    
    count := 0
    for {
        select {
        case <-ticker.C:
            count++
            fmt.Printf("  Procesando request #%d\n", count)
        case <-ctx.Done():
            fmt.Println("  Server: shutdown graceful iniciado...")
            // Simular cleanup
            time.Sleep(500 * time.Millisecond)
            fmt.Println("  Server: conexiones cerradas, datos persistidos")
            return
        }
    }
}

func main() {
    // Contexto que se cancela con SIGINT o SIGTERM
    ctx, stop := signal.NotifyContext(context.Background(), 
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    
    fmt.Println("Server iniciado (Ctrl+C para detener)")
    server(ctx)
    fmt.Println("Server detenido limpiamente")
}
```

### 7.3 Patron: operacion con timeout + cancelacion

```go
package main

import (
    "context"
    "errors"
    "fmt"
    "time"
)

var ErrTimeout = errors.New("operation timed out")

func fetchWithTimeout(ctx context.Context, url string, timeout time.Duration) (string, error) {
    // Crear sub-contexto con timeout
    ctx, cancel := context.WithTimeout(ctx, timeout)
    defer cancel()
    
    resultCh := make(chan string, 1)
    errCh := make(chan error, 1)
    
    go func() {
        // Simular fetch HTTP
        time.Sleep(time.Duration(100+len(url)*10) * time.Millisecond)
        resultCh <- fmt.Sprintf("Response from %s: 200 OK", url)
    }()
    
    select {
    case result := <-resultCh:
        return result, nil
    case err := <-errCh:
        return "", err
    case <-ctx.Done():
        // ctx.Err() distingue entre timeout y cancelacion del padre
        if errors.Is(ctx.Err(), context.DeadlineExceeded) {
            return "", fmt.Errorf("%s: %w", url, ErrTimeout)
        }
        return "", ctx.Err() // cancelacion del padre
    }
}

func main() {
    ctx := context.Background()
    
    urls := []string{
        "api.example.com/fast",       // rapido
        "api.example.com/medium-path", // medio
        "api.example.com/very/long/path/that/takes/forever", // lento
    }
    
    for _, url := range urls {
        result, err := fetchWithTimeout(ctx, url, 300*time.Millisecond)
        if err != nil {
            fmt.Printf("  ERROR: %v\n", err)
        } else {
            fmt.Printf("  OK: %s\n", result)
        }
    }
}
```

---

## 8. Select Vacio y Select con un Solo Case

### 8.1 select {} — bloqueo infinito

```go
// select {} bloquea la goroutine para siempre
// Util para mantener main vivo cuando todo corre en goroutines

package main

import (
    "fmt"
    "net/http"
)

func main() {
    http.HandleFunc("/", func(w http.ResponseWriter, r *http.Request) {
        fmt.Fprintln(w, "Hello, World!")
    })
    
    go func() {
        fmt.Println("Server en :8080")
        http.ListenAndServe(":8080", nil)
    }()
    
    // Mantener main vivo
    select {} // bloquea para siempre
    
    // Alternativas:
    // - runtime.Goexit()       — termina main pero no las goroutines
    // - <-make(chan struct{})   — equivalente pero menos idiomatico
    // - for {}                  — busy-wait, consume CPU al 100%
}
```

### 8.2 Select con un case — redundante pero util con nil channels

```go
package main

import "fmt"

func main() {
    var ch <-chan int = nil
    
    // Con un solo case sobre nil channel + default
    // Es equivalente a: "si hay datos disponibles, procesalos"
    select {
    case v := <-ch: // nil channel — NUNCA esta listo
        fmt.Println(v)
    default:
        fmt.Println("No hay datos (ch es nil)")
    }
    
    // Sin default + nil channel → bloquea para siempre
    // select {
    //     case v := <-ch: // DEADLOCK — nil nunca tiene datos
    //         fmt.Println(v)
    // }
}
```

---

## 9. Patrones Avanzados con Select

### 9.1 Priority select (prioridad manual)

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    high := make(chan string, 10)
    low := make(chan string, 10)
    done := make(chan struct{})
    
    // Generar mensajes
    go func() {
        for i := 1; i <= 5; i++ {
            high <- fmt.Sprintf("HIGH-%d", i)
            low <- fmt.Sprintf("LOW-%d", i)
            low <- fmt.Sprintf("LOW-%d", i+10)
        }
        time.Sleep(100 * time.Millisecond)
        close(done)
    }()
    
    time.Sleep(50 * time.Millisecond) // dejar que se llenen
    
    // Priority select: SIEMPRE procesa HIGH primero
    for {
        // Paso 1: drenar todo high priority
        select {
        case msg := <-high:
            fmt.Printf("  >>> %s\n", msg)
            continue // volver a intentar high
        default:
            // No hay high priority
        }
        
        // Paso 2: si no hay high, aceptar cualquiera
        select {
        case msg := <-high:
            fmt.Printf("  >>> %s\n", msg)
        case msg := <-low:
            fmt.Printf("      %s\n", msg)
        case <-done:
            // Drenar restantes antes de salir
            for {
                select {
                case msg := <-high:
                    fmt.Printf("  >>> %s\n", msg)
                case msg := <-low:
                    fmt.Printf("      %s\n", msg)
                default:
                    return
                }
            }
        }
    }
}
```

### 9.2 Fan-in con select (merge N channels)

```go
package main

import (
    "fmt"
    "sync"
    "time"
)

// fanIn merge multiples channels en uno usando select y goroutines
func fanIn[T any](channels ...<-chan T) <-chan T {
    out := make(chan T)
    var wg sync.WaitGroup
    
    for _, ch := range channels {
        wg.Add(1)
        go func(c <-chan T) {
            defer wg.Done()
            for v := range c {
                out <- v
            }
        }(ch)
    }
    
    go func() {
        wg.Wait()
        close(out)
    }()
    
    return out
}

// fanInReflect merge sin goroutines extra usando reflect.Select
// (mas eficiente para muchos channels, pero mas complejo)
// Se cubrira en patrones avanzados

func generate(name string, count int, delay time.Duration) <-chan string {
    ch := make(chan string)
    go func() {
        defer close(ch)
        for i := 1; i <= count; i++ {
            time.Sleep(delay)
            ch <- fmt.Sprintf("[%s] item-%d", name, i)
        }
    }()
    return ch
}

func main() {
    // 4 sources con diferentes velocidades
    a := generate("alpha", 3, 100*time.Millisecond)
    b := generate("bravo", 3, 150*time.Millisecond)
    c := generate("charlie", 3, 200*time.Millisecond)
    d := generate("delta", 3, 250*time.Millisecond)
    
    // Fan-in: merge 4 → 1
    merged := fanIn(a, b, c, d)
    
    for msg := range merged {
        fmt.Printf("  %s\n", msg)
    }
    fmt.Println("Todos los sources terminaron")
}
```

### 9.3 Debounce con select

```go
package main

import (
    "fmt"
    "time"
)

// debounce emite el ultimo valor recibido despues de un periodo
// de inactividad. Si llegan mas valores durante el periodo,
// reinicia el timer
func debounce[T any](in <-chan T, interval time.Duration) <-chan T {
    out := make(chan T)
    go func() {
        defer close(out)
        var latest T
        var hasValue bool
        timer := time.NewTimer(interval)
        timer.Stop() // empezar parado
        defer timer.Stop()
        
        for {
            select {
            case v, ok := <-in:
                if !ok {
                    // Input cerrado — emitir ultimo valor si hay
                    if hasValue {
                        out <- latest
                    }
                    return
                }
                latest = v
                hasValue = true
                // Reset timer — reiniciar el periodo de espera
                if !timer.Stop() {
                    select {
                    case <-timer.C:
                    default:
                    }
                }
                timer.Reset(interval)
                
            case <-timer.C:
                if hasValue {
                    out <- latest
                    hasValue = false
                }
            }
        }
    }()
    return out
}

func main() {
    events := make(chan string)
    
    // Simular rafaga de eventos seguida de pausa
    go func() {
        // Rafaga rapida — solo el ultimo deberia emitirse
        events <- "keystroke-1"
        time.Sleep(50 * time.Millisecond)
        events <- "keystroke-2"
        time.Sleep(50 * time.Millisecond)
        events <- "keystroke-3" // este se emite despues de 200ms de silencio
        
        time.Sleep(300 * time.Millisecond) // pausa
        
        // Otra rafaga
        events <- "keystroke-4"
        time.Sleep(50 * time.Millisecond)
        events <- "keystroke-5" // este se emite
        
        time.Sleep(300 * time.Millisecond)
        close(events)
    }()
    
    debounced := debounce(events, 200*time.Millisecond)
    
    for v := range debounced {
        fmt.Printf("  Emitido: %s\n", v)
    }
    // Output:
    //   Emitido: keystroke-3   (despues de 200ms sin actividad)
    //   Emitido: keystroke-5   (despues de 200ms sin actividad)
}
```

### 9.4 Rate limiter con select

```go
package main

import (
    "fmt"
    "time"
)

func main() {
    requests := make(chan int, 5)
    for i := 1; i <= 5; i++ {
        requests <- i
    }
    close(requests)
    
    // Limiter: permite 1 request cada 200ms
    limiter := time.Tick(200 * time.Millisecond)
    
    for req := range requests {
        <-limiter // esperar al siguiente tick
        fmt.Printf("  Request %d procesado a %s\n", req, time.Now().Format("15:04:05.000"))
    }
    
    fmt.Println()
    
    // Burst limiter: permite rafagas de hasta 3 seguidas
    burstyLimiter := make(chan time.Time, 3)
    
    // Pre-llenar con 3 tokens (burst inicial)
    for i := 0; i < 3; i++ {
        burstyLimiter <- time.Now()
    }
    
    // Refill: 1 token cada 200ms
    go func() {
        for t := range time.Tick(200 * time.Millisecond) {
            select {
            case burstyLimiter <- t:
            default: // bucket lleno — descartar
            }
        }
    }()
    
    burstyRequests := make(chan int, 5)
    for i := 1; i <= 5; i++ {
        burstyRequests <- i
    }
    close(burstyRequests)
    
    for req := range burstyRequests {
        <-burstyLimiter // consume un token
        fmt.Printf("  Bursty request %d a %s\n", req, time.Now().Format("15:04:05.000"))
    }
    // Los primeros 3 se procesan inmediatamente (burst)
    // Los siguientes esperan 200ms cada uno
}
```

### 9.5 Or-channel (primer channel que cierre)

```go
package main

import (
    "fmt"
    "time"
)

// or retorna un channel que se cierra cuando CUALQUIERA de los inputs se cierra
// Es el equivalente de "race" — el primero que termine gana
func or(channels ...<-chan struct{}) <-chan struct{} {
    switch len(channels) {
    case 0:
        return nil
    case 1:
        return channels[0]
    }
    
    orDone := make(chan struct{})
    go func() {
        defer close(orDone)
        switch len(channels) {
        case 2:
            select {
            case <-channels[0]:
            case <-channels[1]:
            }
        default:
            // Dividir en dos mitades y recurrir
            // Esto permite manejar cualquier numero de channels
            mid := len(channels) / 2
            select {
            case <-or(channels[:mid]...):
            case <-or(channels[mid:]...):
            }
        }
    }()
    return orDone
}

func after(d time.Duration) <-chan struct{} {
    ch := make(chan struct{})
    go func() {
        time.Sleep(d)
        close(ch)
    }()
    return ch
}

func main() {
    start := time.Now()
    
    // Esperar al primero que termine
    <-or(
        after(2*time.Second),
        after(5*time.Second),
        after(1*time.Second),  // este gana
        after(3*time.Second),
    )
    
    fmt.Printf("Terminado despues de %v\n", time.Since(start).Round(time.Millisecond))
    // Output: Terminado despues de 1s
}
```

---

## 10. Antipatrones con Select

### 10.1 Catalogo de errores

```go
// ============================================================
// ERROR 1: time.After en loop tight (memory leak)
// ============================================================
func bad1(ch <-chan int) {
    for {
        select {
        case v := <-ch:
            fmt.Println(v)
        case <-time.After(5 * time.Second):
            // Cada iteracion crea un NUEVO timer
            // Si ch envia rapido, los timers se acumulan
            // y no se GC hasta que disparan (5s despues)
            // Con 1M iter/seg → 5M timers en memoria
            return
        }
    }
}

// FIX: usar time.NewTimer y Reset
func good1(ch <-chan int) {
    timer := time.NewTimer(5 * time.Second)
    defer timer.Stop()
    
    for {
        select {
        case v := <-ch:
            fmt.Println(v)
            if !timer.Stop() {
                <-timer.C
            }
            timer.Reset(5 * time.Second)
        case <-timer.C:
            return
        }
    }
}

// ============================================================
// ERROR 2: Busy-wait con default en loop
// ============================================================
func bad2(ch <-chan int) {
    for {
        select {
        case v := <-ch:
            fmt.Println(v)
        default:
            // CPU al 100% — spin loop
            // Esto NO es polling, es busy-wait
        }
    }
}

// FIX 1: quitar default (dejar que select bloquee)
func good2a(ch <-chan int, done <-chan struct{}) {
    for {
        select {
        case v := <-ch:
            fmt.Println(v)
        case <-done:
            return
        }
    }
}

// FIX 2: si necesitas polling, usar ticker
func good2b(ch <-chan int) {
    ticker := time.NewTicker(100 * time.Millisecond)
    defer ticker.Stop()
    for {
        select {
        case v := <-ch:
            fmt.Println(v)
            return
        case <-ticker.C:
            fmt.Print(".") // polling controlado
        }
    }
}

// ============================================================
// ERROR 3: Asumir orden/prioridad en select
// ============================================================
func bad3(high, low <-chan int) {
    for {
        select {
        case v := <-high:
            fmt.Println("High:", v) // NO tiene prioridad sobre low
        case v := <-low:
            fmt.Println("Low:", v)  // puede elegirse antes que high
        }
    }
    // Si ambos tienen datos, 50/50 — no hay prioridad
}

// FIX: priority select manual (ver seccion 9.1)
func good3(high, low <-chan int) {
    for {
        select {
        case v := <-high:
            fmt.Println("High:", v)
            continue
        default:
        }
        select {
        case v := <-high:
            fmt.Println("High:", v)
        case v := <-low:
            fmt.Println("Low:", v)
        }
    }
}

// ============================================================
// ERROR 4: Olvidar ctx.Done() en select
// ============================================================
func bad4(ctx context.Context, ch <-chan int) {
    for v := range ch {
        // Este loop IGNORA la cancelacion del contexto
        // Si ctx se cancela, el loop sigue hasta que ch se cierre
        fmt.Println(v)
    }
}

// FIX: siempre incluir ctx.Done() en select
func good4(ctx context.Context, ch <-chan int) {
    for {
        select {
        case v, ok := <-ch:
            if !ok {
                return
            }
            fmt.Println(v)
        case <-ctx.Done():
            return // respetar cancelacion
        }
    }
}

// ============================================================
// ERROR 5: Goroutine leak por select sin salida
// ============================================================
func bad5() <-chan int {
    ch := make(chan int)
    go func() {
        // Si nadie lee de ch, esta goroutine queda bloqueada para siempre
        select {
        case ch <- 42:
        // No hay case para salir — goroutine leak
        }
    }()
    return ch
}

// FIX: siempre tener una via de escape
func good5(ctx context.Context) <-chan int {
    ch := make(chan int, 1)
    go func() {
        select {
        case ch <- 42:
        case <-ctx.Done(): // via de escape
        }
    }()
    return ch
}

// ============================================================
// ERROR 6: Leer de channel cerrado repetidamente en select
// ============================================================
func bad6(ch <-chan int, done <-chan struct{}) {
    for {
        select {
        case v := <-ch: // Si ch se cerro, esto retorna 0, false
            // Pero NO usamos comma-ok, asi que procesamos zeros
            fmt.Println(v) // Imprime 0 infinitamente
        case <-done:
            return
        }
    }
}

// FIX: usar comma-ok y nil-ificar el channel
func good6(ch <-chan int, done <-chan struct{}) {
    for {
        select {
        case v, ok := <-ch:
            if !ok {
                ch = nil // desactivar este case
                continue
            }
            fmt.Println(v)
        case <-done:
            return
        }
    }
}
```

### 10.2 Tabla resumen de antipatrones

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    ANTIPATRONES CON SELECT                               │
├──────────────────────────────┬───────────────────────────────────────────┤
│ Antipatron                    │ Solucion                                 │
├──────────────────────────────┼───────────────────────────────────────────┤
│ time.After en loop rapido    │ time.NewTimer + Reset                    │
├──────────────────────────────┼───────────────────────────────────────────┤
│ default en loop (busy-wait)  │ Quitar default o usar ticker            │
├──────────────────────────────┼───────────────────────────────────────────┤
│ Asumir prioridad de cases    │ Priority select manual (nested)         │
├──────────────────────────────┼───────────────────────────────────────────┤
│ Olvidar ctx.Done()           │ Siempre incluir ctx.Done() en select    │
├──────────────────────────────┼───────────────────────────────────────────┤
│ Select sin via de escape     │ Agregar ctx.Done() o done channel       │
├──────────────────────────────┼───────────────────────────────────────────┤
│ Leer ch cerrado sin comma-ok │ v, ok := <-ch + nil-ificar si !ok      │
├──────────────────────────────┼───────────────────────────────────────────┤
│ for { select { default: } }  │ for { select { sin default } }          │
│ (spin loop al 100% CPU)      │ o ticker en lugar de default            │
└──────────────────────────────┴───────────────────────────────────────────┘
```

---

## 11. Internals — Como Funciona Select en el Runtime

### 11.1 Compilacion de select

El compilador transforma `select` en llamadas al runtime:

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    SELECT — COMPILACION Y RUNTIME                       │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  CODIGO GO:                                                              │
│                                                                          │
│  select {                                                                │
│  case v := <-ch1:    // case 0                                         │
│  case ch2 <- val:     // case 1                                         │
│  default:             // case 2                                         │
│  }                                                                       │
│                                                                          │
│  COMPILADOR GENERA:                                                      │
│                                                                          │
│  1. Construye array de scase (select case):                            │
│     cases = [{ch: ch1, kind: recv},                                    │
│              {ch: ch2, kind: send, elem: &val},                        │
│              {kind: default}]                                           │
│                                                                          │
│  2. Llama a runtime.selectgo(cases):                                   │
│     ├─ Mezcla el orden de los cases (randomize)                        │
│     ├─ Ordena por direccion de hchan (lock ordering, evita deadlock)   │
│     ├─ Lockea TODOS los channels involucrados                          │
│     ├─ Busca algun case listo                                          │
│     │  ├─ Si hay uno: ejecuta, unlock, retorna index                  │
│     │  ├─ Si hay multiples: elige uno al azar, ejecuta, retorna       │
│     │  ├─ Si hay default y ninguno listo: retorna default index       │
│     │  └─ Si no hay default y ninguno listo:                           │
│     │     ├─ Encola la goroutine en sendq/recvq de CADA channel      │
│     │     ├─ Unlock todos los channels                                 │
│     │     ├─ Park (suspender) la goroutine                            │
│     │     ├─ Cuando un channel la despierta:                          │
│     │     │  ├─ Lockea todos los channels otra vez                    │
│     │     │  ├─ Desencola de todos los otros channels                 │
│     │     │  └─ Ejecuta el case que la desperto                       │
│     │     └─ Retorna el index del case ejecutado                      │
│     └─ Unlockea todos los channels                                     │
│                                                                          │
│  COMPLEJIDAD:                                                            │
│  ├─ O(N) por cada select, donde N = numero de cases                   │
│  ├─ Lock ordering por direccion evita deadlocks                       │
│  ├─ La randomizacion es con fastrand (PRNG del runtime, no crypto)    │
│  └─ Un select con 2 cases es optimizado (selectgo2, sin arrays)       │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

### 11.2 Estructura scase

```go
// runtime/select.go (simplificado)
type scase struct {
    c    *hchan    // channel (nil para default)
    elem unsafe.Pointer // puntero al valor a enviar/recibir
    kind uint16   // caseRecv, caseSend, caseDefault
}

// selectgo ejecuta un select
// cas0: array de cases
// order0: array para randomizar orden
// nsends: numero de send cases
// nrecvs: numero de recv cases
// block: true si no hay default (puede bloquear)
func selectgo(cas0 *scase, order0 *uint16, pc0 *uintptr,
    nsends, nrecvs int, block bool) (int, bool) {
    // ...
}
```

### 11.3 Optimizaciones del compilador

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    OPTIMIZACIONES DE SELECT                              │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  El compilador reemplaza select por operaciones mas simples cuando      │
│  puede:                                                                  │
│                                                                          │
│  1. select {} (vacio)                                                   │
│     → runtime.block() (bloqueo infinito, sin overhead)                 │
│                                                                          │
│  2. select { case <-ch: }  (un case, sin default)                      │
│     → chanrecv(ch)  (receive directo, sin selectgo)                    │
│                                                                          │
│  3. select { case ch <- v: }  (un case, sin default)                   │
│     → chansend(ch, v)  (send directo, sin selectgo)                    │
│                                                                          │
│  4. select { case <-ch: ... default: }  (un recv + default)            │
│     → selectnbrecv(ch)  (non-blocking receive, sin selectgo)           │
│                                                                          │
│  5. select { case ch <- v: ... default: }  (un send + default)         │
│     → selectnbsend(ch, v)  (non-blocking send, sin selectgo)           │
│                                                                          │
│  6. select con 2 cases (sin default)                                   │
│     → selectgo2()  (optimizado, sin arrays)                             │
│                                                                          │
│  Solo para 3+ cases se usa el selectgo general con arrays.             │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 12. Comparacion con C y Rust

### 12.1 C — select(), poll(), epoll()

C tiene `select()` pero para **file descriptors**, no para channels. El concepto es similar (multiplexar I/O) pero la API es completamente diferente:

```c
// C: select() para file descriptors
#include <sys/select.h>
#include <stdio.h>
#include <unistd.h>

int main() {
    fd_set readfds;
    struct timeval timeout;
    
    FD_ZERO(&readfds);
    FD_SET(STDIN_FILENO, &readfds);
    
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;
    
    // Esperar datos en stdin con timeout de 5s
    int ret = select(STDIN_FILENO + 1, &readfds, NULL, NULL, &timeout);
    
    if (ret > 0 && FD_ISSET(STDIN_FILENO, &readfds)) {
        char buf[256];
        read(STDIN_FILENO, buf, sizeof(buf));
        printf("Leido: %s\n", buf);
    } else if (ret == 0) {
        printf("Timeout\n");
    } else {
        perror("select");
    }
    
    // DIFERENCIAS con Go select:
    // - Solo para file descriptors (no channels/estructuras arbitrarias)
    // - FD_SETSIZE limita a ~1024 file descriptors
    // - No tiene "default" — siempre puede bloquear (salvo timeout 0)
    // - No randomiza — revisa en orden
    // - Modifica los fd_sets — debes reconstruirlos en cada iteracion
    // - No es thread-safe sin locks adicionales
    // - Para miles de FDs, necesitas epoll/kqueue (O(1) vs O(N))
    
    return 0;
}

// C: epoll (Linux) — mas eficiente para muchos FDs
#include <sys/epoll.h>

// int epfd = epoll_create1(0);
// struct epoll_event ev = {.events = EPOLLIN, .data.fd = sockfd};
// epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev);
//
// struct epoll_event events[MAX_EVENTS];
// int n = epoll_wait(epfd, events, MAX_EVENTS, timeout_ms);
// for (int i = 0; i < n; i++) {
//     process(events[i].data.fd);
// }

// Para multiplexar "channels" (queues thread-safe), C no tiene nada.
// Tendrias que convertir la queue en un eventfd y usar epoll sobre eso.
```

### 12.2 Rust — select! macro (crossbeam y tokio)

```rust
// Rust: std::sync::mpsc NO tiene select
// Rust elimino mpsc::Select por razones de seguridad y API
// Para multiplexar, usas crossbeam-channel o tokio

// === crossbeam-channel (sync) ===
use crossbeam_channel::{select, bounded, after, tick, never};
use std::time::Duration;

fn crossbeam_example() {
    let (s1, r1) = bounded::<String>(10);
    let (s2, r2) = bounded::<i32>(10);
    
    // select! macro — similar a Go select
    select! {
        recv(r1) -> msg => {
            match msg {
                Ok(m) => println!("r1: {}", m),
                Err(_) => println!("r1 cerrado"),
            }
        },
        recv(r2) -> num => {
            println!("r2: {:?}", num);
        },
        // timeout (equivalente a time.After)
        recv(after(Duration::from_secs(1))) -> _ => {
            println!("timeout");
        },
        // default (non-blocking, como Go default)
        default => {
            println!("nada listo");
        },
    }
    
    // crossbeam select! es lo mas cercano a Go select:
    // - Randomiza cuando multiples estan listos ✓
    // - Soporta send Y recv ✓  
    // - default case ✓
    // - Timeout con after() ✓
    // - Ticker con tick() ✓
    // - Desactivar con never() (como nil channel en Go) ✓
}

// === tokio (async) ===
use tokio::select;

async fn tokio_example() {
    let (tx1, mut rx1) = tokio::sync::mpsc::channel::<String>(10);
    let (tx2, mut rx2) = tokio::sync::mpsc::channel::<i32>(10);
    
    tokio::select! {
        Some(msg) = rx1.recv() => {
            println!("rx1: {}", msg);
        },
        Some(num) = rx2.recv() => {
            println!("rx2: {}", num);
        },
        _ = tokio::time::sleep(Duration::from_secs(1)) => {
            println!("timeout");
        },
        // tokio select! por defecto es BIASED (no random)
        // Para random: tokio::select! { biased; ... } lo hace deterministic
        // Sin biased: ¡ES RANDOM como Go! (desde tokio 1.x)
    }
}

// Diferencia clave con Go:
// - crossbeam select! es una MACRO (codigo generado en compile-time)
// - tokio select! es async (necesita runtime, function coloring)
// - Go select es una KEYWORD (built-in, el runtime lo maneja)
// - Rust no puede olvidar manejar errores (Result en recv)
// - Go puede ignorar errores en receive (v := <-ch sin comma-ok)
```

### 12.3 Tabla comparativa

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    SELECT/MULTIPLEXING: GO vs C vs RUST                 │
├──────────────────┬─────────────────┬───────────────┬─────────────────────┤
│ Aspecto           │ C               │ Go            │ Rust               │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Primitiva         │ select()/poll() │ select {}     │ crossbeam select!  │
│                   │ epoll/kqueue    │ (keyword)     │ tokio select!      │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Opera sobre       │ File descriptors│ Channels      │ Channels           │
│                   │ (sockets, pipes)│ (cualquier T) │ (Receiver<T>)      │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Randomizacion     │ No (secuencial) │ Si (uniform)  │ crossbeam: Si      │
│                   │                 │               │ tokio: configurable │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Default case      │ timeout=0       │ default {}    │ default => {}      │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Timeout           │ struct timeval  │ time.After()  │ after(Duration)    │
│                   │                 │ time.NewTimer │ sleep(Duration)    │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Send + recv       │ N/A (solo FDs)  │ Si (mezclados)│ crossbeam: Si     │
│                   │                 │               │ tokio: recv only   │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Desactivar case   │ FD_CLR()        │ ch = nil      │ never()            │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Max operandos     │ FD_SETSIZE      │ Sin limite    │ Sin limite         │
│                   │ (~1024)         │ (O(N))        │ (compile-time)     │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Implementacion    │ Syscall kernel  │ Runtime       │ Macro              │
│                   │                 │ (user space)  │ (compile-time)     │
├──────────────────┼─────────────────┼───────────────┼─────────────────────┤
│ Error handling    │ errno check     │ v, ok := <-ch │ Result<T, Err>    │
│                   │ (manual)        │ (optional)    │ (obligatorio)      │
└──────────────────┴─────────────────┴───────────────┴─────────────────────┘
```

---

## 13. Programa Completo — Infrastructure Event Router

Un sistema de enrutamiento de eventos con multiples fuentes, routing por tipo, timeouts, rate limiting, heartbeat, y shutdown graceful — todo orquestado con `select`:

```go
package main

import (
    "context"
    "fmt"
    "math/rand"
    "os"
    "os/signal"
    "strings"
    "sync"
    "syscall"
    "time"
)

// ============================================================
// Tipos
// ============================================================

type EventType string

const (
    EventAlert   EventType = "ALERT"
    EventMetric  EventType = "METRIC"
    EventLog     EventType = "LOG"
    EventAudit   EventType = "AUDIT"
)

type Severity int

const (
    SevInfo Severity = iota
    SevWarning
    SevCritical
)

func (s Severity) String() string {
    return [...]string{"INFO", "WARNING", "CRITICAL"}[s]
}

type Event struct {
    ID        int
    Type      EventType
    Severity  Severity
    Source    string
    Message  string
    Timestamp time.Time
}

// ============================================================
// Event Sources — cada una retorna <-chan Event
// ============================================================

func alertSource(ctx context.Context, source string) <-chan Event {
    out := make(chan Event)
    go func() {
        defer close(out)
        id := 0
        for {
            delay := time.Duration(500+rand.Intn(1500)) * time.Millisecond
            select {
            case <-time.After(delay):
                id++
                sev := []Severity{SevInfo, SevWarning, SevWarning, SevCritical}[rand.Intn(4)]
                msgs := []string{"CPU spike detected", "Memory threshold exceeded",
                    "Disk usage high", "Network latency spike", "Connection pool exhausted"}
                select {
                case out <- Event{
                    ID: id, Type: EventAlert, Severity: sev,
                    Source: source, Message: msgs[rand.Intn(len(msgs))],
                    Timestamp: time.Now(),
                }:
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

func metricSource(ctx context.Context, source string) <-chan Event {
    out := make(chan Event)
    go func() {
        defer close(out)
        id := 0
        ticker := time.NewTicker(300 * time.Millisecond)
        defer ticker.Stop()
        for {
            select {
            case <-ticker.C:
                id++
                select {
                case out <- Event{
                    ID: id, Type: EventMetric, Severity: SevInfo,
                    Source: source,
                    Message: fmt.Sprintf("cpu=%.1f%% mem=%.1f%% disk=%.1f%%",
                        rand.Float64()*100, rand.Float64()*100, rand.Float64()*100),
                    Timestamp: time.Now(),
                }:
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

func auditSource(ctx context.Context, source string) <-chan Event {
    out := make(chan Event)
    go func() {
        defer close(out)
        id := 0
        actions := []string{"user_login", "config_changed", "deployment_started",
            "permission_granted", "secret_accessed", "firewall_rule_added"}
        for {
            delay := time.Duration(800+rand.Intn(2000)) * time.Millisecond
            select {
            case <-time.After(delay):
                id++
                select {
                case out <- Event{
                    ID: id, Type: EventAudit, Severity: SevInfo,
                    Source: source, Message: actions[rand.Intn(len(actions))],
                    Timestamp: time.Now(),
                }:
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

// ============================================================
// Fan-in: merge multiples sources
// ============================================================

func merge(ctx context.Context, sources ...<-chan Event) <-chan Event {
    out := make(chan Event, 20)
    var wg sync.WaitGroup
    for _, src := range sources {
        wg.Add(1)
        go func(ch <-chan Event) {
            defer wg.Done()
            for {
                select {
                case e, ok := <-ch:
                    if !ok {
                        return
                    }
                    select {
                    case out <- e:
                    case <-ctx.Done():
                        return
                    }
                case <-ctx.Done():
                    return
                }
            }
        }(src)
    }
    go func() {
        wg.Wait()
        close(out)
    }()
    return out
}

// ============================================================
// Router — enruta eventos por tipo usando select
// ============================================================

type Router struct {
    alertCh  chan Event
    metricCh chan Event
    auditCh  chan Event
    dropCh   chan Event
}

func NewRouter(bufSize int) *Router {
    return &Router{
        alertCh:  make(chan Event, bufSize),
        metricCh: make(chan Event, bufSize),
        auditCh:  make(chan Event, bufSize),
        dropCh:   make(chan Event, bufSize),
    }
}

func (r *Router) Route(ctx context.Context, in <-chan Event) {
    defer close(r.alertCh)
    defer close(r.metricCh)
    defer close(r.auditCh)
    defer close(r.dropCh)

    for {
        select {
        case event, ok := <-in:
            if !ok {
                return
            }
            var target chan Event
            switch event.Type {
            case EventAlert:
                target = r.alertCh
            case EventMetric:
                target = r.metricCh
            case EventAudit:
                target = r.auditCh
            default:
                target = r.dropCh
            }
            // Non-blocking send — drop si el handler es lento
            select {
            case target <- event:
            default:
                fmt.Printf("  [DROP] %s event from %s (handler full)\n",
                    event.Type, event.Source)
            }
        case <-ctx.Done():
            return
        }
    }
}

// ============================================================
// Handlers — consumen eventos enrutados
// ============================================================

type HandlerStats struct {
    Processed int
    Critical  int
    mu        sync.Mutex
}

func (s *HandlerStats) Inc(critical bool) {
    s.mu.Lock()
    s.Processed++
    if critical {
        s.Critical++
    }
    s.mu.Unlock()
}

func alertHandler(ctx context.Context, events <-chan Event, stats *HandlerStats) {
    // Priority select: CRITICAL primero
    for {
        select {
        case event, ok := <-events:
            if !ok {
                return
            }
            icon := "⚡"
            if event.Severity == SevCritical {
                icon = "🔴"
            }
            fmt.Printf("  %s [ALERT] [%s] %s: %s\n",
                icon, event.Severity, event.Source, event.Message)
            stats.Inc(event.Severity == SevCritical)
        case <-ctx.Done():
            // Drain remaining alerts before exit
            for event := range events {
                fmt.Printf("  [DRAIN-ALERT] %s: %s\n", event.Source, event.Message)
                stats.Inc(event.Severity == SevCritical)
            }
            return
        }
    }
}

func metricHandler(ctx context.Context, events <-chan Event, stats *HandlerStats) {
    // Batch metrics: acumular y reportar cada 5 metricas
    batch := make([]Event, 0, 5)
    
    // Timer para flush si no se llena el batch
    timer := time.NewTimer(2 * time.Second)
    defer timer.Stop()
    
    flush := func() {
        if len(batch) == 0 {
            return
        }
        fmt.Printf("  [METRIC-BATCH] %d metrics collected:\n", len(batch))
        for _, e := range batch {
            fmt.Printf("    %s → %s\n", e.Source, e.Message)
        }
        stats.mu.Lock()
        stats.Processed += len(batch)
        stats.mu.Unlock()
        batch = batch[:0]
    }
    
    for {
        select {
        case event, ok := <-events:
            if !ok {
                flush()
                return
            }
            batch = append(batch, event)
            if len(batch) >= 5 {
                flush()
                if !timer.Stop() {
                    select {
                    case <-timer.C:
                    default:
                    }
                }
                timer.Reset(2 * time.Second)
            }
        case <-timer.C:
            flush()
            timer.Reset(2 * time.Second)
        case <-ctx.Done():
            flush()
            return
        }
    }
}

func auditHandler(ctx context.Context, events <-chan Event, stats *HandlerStats) {
    for {
        select {
        case event, ok := <-events:
            if !ok {
                return
            }
            fmt.Printf("  [AUDIT] %s | %s | %s\n",
                event.Timestamp.Format("15:04:05"), event.Source, event.Message)
            stats.Inc(false)
        case <-ctx.Done():
            for event := range events {
                fmt.Printf("  [DRAIN-AUDIT] %s\n", event.Message)
                stats.Inc(false)
            }
            return
        }
    }
}

// ============================================================
// Heartbeat monitor — verifica que los handlers estan vivos
// ============================================================

func heartbeatMonitor(ctx context.Context, interval time.Duration,
    alertStats, metricStats, auditStats *HandlerStats) {
    
    ticker := time.NewTicker(interval)
    defer ticker.Stop()
    
    var lastAlert, lastMetric, lastAudit int
    
    for {
        select {
        case <-ticker.C:
            alertStats.mu.Lock()
            currentAlert := alertStats.Processed
            alertStats.mu.Unlock()
            
            metricStats.mu.Lock()
            currentMetric := metricStats.Processed
            metricStats.mu.Unlock()
            
            auditStats.mu.Lock()
            currentAudit := auditStats.Processed
            auditStats.mu.Unlock()
            
            stale := []string{}
            if currentAlert == lastAlert {
                stale = append(stale, "alert")
            }
            if currentMetric == lastMetric {
                stale = append(stale, "metric")
            }
            if currentAudit == lastAudit {
                stale = append(stale, "audit")
            }
            
            if len(stale) > 0 {
                fmt.Printf("  [HEARTBEAT] WARNING: stale handlers: %s\n",
                    strings.Join(stale, ", "))
            } else {
                fmt.Printf("  [HEARTBEAT] All handlers active (a:%d m:%d au:%d)\n",
                    currentAlert, currentMetric, currentAudit)
            }
            
            lastAlert = currentAlert
            lastMetric = currentMetric
            lastAudit = currentAudit
            
        case <-ctx.Done():
            return
        }
    }
}

// ============================================================
// Main
// ============================================================

func main() {
    // Contexto con cancelacion por signal
    ctx, stop := signal.NotifyContext(context.Background(),
        syscall.SIGINT, syscall.SIGTERM)
    defer stop()
    
    // Timeout automatico para demo (5 segundos)
    ctx, cancel := context.WithTimeout(ctx, 5*time.Second)
    defer cancel()

    fmt.Println("╔══════════════════════════════════════════════════════════════╗")
    fmt.Println("║           INFRASTRUCTURE EVENT ROUTER                        ║")
    fmt.Println("╠══════════════════════════════════════════════════════════════╣")
    fmt.Println("║  Select patterns: multiplex, timeout, heartbeat, priority   ║")
    fmt.Println("║  Duration: 5s (or Ctrl+C to stop)                           ║")
    fmt.Println("╚══════════════════════════════════════════════════════════════╝")
    fmt.Println()

    // 1. Create event sources
    alerts1 := alertSource(ctx, "prod-web-01")
    alerts2 := alertSource(ctx, "prod-web-02")
    metrics := metricSource(ctx, "monitoring")
    audit := auditSource(ctx, "auth-service")

    // 2. Fan-in: merge all sources
    allEvents := merge(ctx, alerts1, alerts2, metrics, audit)

    // 3. Router: route by event type
    router := NewRouter(10)
    go router.Route(ctx, allEvents)

    // 4. Handler stats
    alertStats := &HandlerStats{}
    metricStats := &HandlerStats{}
    auditStats := &HandlerStats{}

    // 5. Start handlers (each uses select internally)
    var wg sync.WaitGroup
    
    wg.Add(1)
    go func() {
        defer wg.Done()
        alertHandler(ctx, router.alertCh, alertStats)
    }()
    
    wg.Add(1)
    go func() {
        defer wg.Done()
        metricHandler(ctx, router.metricCh, metricStats)
    }()
    
    wg.Add(1)
    go func() {
        defer wg.Done()
        auditHandler(ctx, router.auditCh, auditStats)
    }()

    // 6. Heartbeat monitor
    wg.Add(1)
    go func() {
        defer wg.Done()
        heartbeatMonitor(ctx, 2*time.Second, alertStats, metricStats, auditStats)
    }()

    // Wait for shutdown
    wg.Wait()

    // Final stats
    fmt.Println()
    fmt.Println("┌──────────────────────────────────────────┐")
    fmt.Println("│              FINAL STATS                 │")
    fmt.Println("├──────────────────────────────────────────┤")
    fmt.Printf("│  Alerts processed:  %-5d                │\n", alertStats.Processed)
    fmt.Printf("│  Critical alerts:   %-5d                │\n", alertStats.Critical)
    fmt.Printf("│  Metrics batched:   %-5d                │\n", metricStats.Processed)
    fmt.Printf("│  Audit events:      %-5d                │\n", auditStats.Processed)
    fmt.Printf("│  Total:             %-5d                │\n",
        alertStats.Processed+metricStats.Processed+auditStats.Processed)
    fmt.Println("└──────────────────────────────────────────┘")
    
    fmt.Fprintln(os.Stderr) // flush
}
```

### Ejecucion esperada

```bash
$ go run main.go

╔══════════════════════════════════════════════════════════════╗
║           INFRASTRUCTURE EVENT ROUTER                        ║
╠══════════════════════════════════════════════════════════════╣
║  Select patterns: multiplex, timeout, heartbeat, priority   ║
║  Duration: 5s (or Ctrl+C to stop)                           ║
╚══════════════════════════════════════════════════════════════╝

  [AUDIT] 14:32:01 | auth-service | user_login
  [METRIC-BATCH] 5 metrics collected:
    monitoring → cpu=45.2% mem=67.1% disk=23.4%
    monitoring → cpu=78.3% mem=42.5% disk=89.1%
    monitoring → cpu=12.7% mem=91.0% disk=45.6%
    monitoring → cpu=56.8% mem=33.2% disk=67.8%
    monitoring → cpu=89.1% mem=55.4% disk=12.3%
  ⚡ [ALERT] [WARNING] prod-web-01: CPU spike detected
  🔴 [ALERT] [CRITICAL] prod-web-02: Memory threshold exceeded
  [AUDIT] 14:32:02 | auth-service | config_changed
  [HEARTBEAT] All handlers active (a:2 m:5 au:2)
  ⚡ [ALERT] [INFO] prod-web-01: Network latency spike
  [METRIC-BATCH] 5 metrics collected:
    ...
  [AUDIT] 14:32:03 | auth-service | secret_accessed
  🔴 [ALERT] [CRITICAL] prod-web-02: Connection pool exhausted
  [HEARTBEAT] All handlers active (a:5 m:10 au:3)
  ...

┌──────────────────────────────────────────────�
│              FINAL STATS                 │
├──────────────────────────────────────────┤
│  Alerts processed:  8                    │
│  Critical alerts:   3                    │
│  Metrics batched:   15                   │
│  Audit events:      4                    │
│  Total:             27                   │
└──────────────────────────────────────────┘
```

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    SELECT PATTERNS EN EL PROGRAMA                       │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  1. MULTIPLEXING (merge)                                                │
│     select { case <-ch1: / case <-ch2: / case <-ch3: }                │
│     → Combina 4 sources en 1 stream                                    │
│                                                                          │
│  2. CANCELATION (todos los handlers)                                    │
│     select { case <-events: / case <-ctx.Done(): }                     │
│     → Shutdown cooperativo cuando el contexto se cancela               │
│                                                                          │
│  3. TIMEOUT (fuentes de eventos)                                        │
│     select { case <-time.After(d): / case <-ctx.Done(): }             │
│     → Delays aleatorios con cancelacion                                │
│                                                                          │
│  4. NON-BLOCKING SEND (router)                                          │
│     select { case target <- event: / default: DROP }                   │
│     → Descarta eventos si el handler es lento                          │
│                                                                          │
│  5. TIMER + DATA (metricHandler)                                        │
│     select { case <-events: / case <-timer.C: / case <-ctx.Done(): } │
│     → Batch con flush por tiempo o por cantidad                        │
│                                                                          │
│  6. HEARTBEAT (heartbeatMonitor)                                        │
│     select { case <-ticker.C: / case <-ctx.Done(): }                  │
│     → Polling periodico para detectar handlers muertos                 │
│                                                                          │
│  7. SIGNAL SHUTDOWN (main)                                               │
│     signal.NotifyContext → ctx.Done() propaga a todo el sistema        │
│     → Ctrl+C cierra todo limpiamente                                   │
│                                                                          │
│  8. DRAIN ON SHUTDOWN (alertHandler, auditHandler)                      │
│     case <-ctx.Done(): for event := range events { ... }               │
│     → Procesa eventos pendientes antes de terminar                     │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## 14. Ejercicios

### Ejercicio 1: Connection pool con select

Implementa un connection pool que usa `select` para:
- Obtener conexion del pool (con timeout de 2s)
- Devolver conexion al pool (non-blocking — si el pool esta lleno, descarta)
- Health check periodico (cada 5s, remover conexiones muertas)
- Shutdown graceful (cerrar todas las conexiones pendientes)

```go
type Pool struct {
    conns chan *Connection
}

func (p *Pool) Get(ctx context.Context) (*Connection, error)  // select con ctx.Done + timeout
func (p *Pool) Put(conn *Connection)                           // select con default (non-blocking)
func (p *Pool) healthCheck(ctx context.Context)                // select con ticker
```

### Ejercicio 2: Rate-limited API caller

Implementa un caller que hace requests a una API con:
- Rate limiting: maximo 10 requests por segundo (token bucket con select + ticker)
- Timeout por request: 3 segundos (select + time.After)
- Timeout total: 30 segundos (select + deadline)
- Retry con backoff: si falla, reintentar con delay exponencial (select + timer)
- Cancelacion: ctx.Done() en cada select
- Reporte de estadisticas cada 5 segundos (select + ticker)

### Ejercicio 3: Multi-service health dashboard

Implementa un dashboard que monitorea 5 servicios:
- Cada servicio tiene su propio source `<-chan HealthStatus`
- Un goroutine principal usa `select` para multiplexar los 5 sources
- Si un servicio no reporta en 10 segundos, marcarlo como DOWN (select + timer per service)
- Refrescar el dashboard cada 2 segundos (select + ticker)
- Alertar si mas de 2 servicios estan DOWN (priority select: alertas primero)
- Shutdown con Ctrl+C (signal.NotifyContext)

### Ejercicio 4: Event deduplicator con ventana temporal

Implementa un deduplicator que:
- Recibe eventos de un `<-chan Event`
- Usa `select` con un timer para definir ventanas de tiempo (1 segundo)
- Dentro de cada ventana, descarta eventos duplicados (mismo source+type)
- Al final de cada ventana, emite los eventos unicos
- Maneja shutdown graceful con ctx.Done()
- Usa non-blocking send para el output (default: contar drops)

---

## Resumen

```
┌──────────────────────────────────────────────────────────────────────────┐
│                    SELECT — RESUMEN COMPLETO                             │
├──────────────────────────────────────────────────────────────────────────┤
│                                                                          │
│  SINTAXIS:                                                               │
│  ├─ select { case <-ch: ... }          multiplexar receives            │
│  ├─ select { case ch <- v: ... }       multiplexar sends               │
│  ├─ select { case ... default: }       non-blocking                    │
│  └─ select {}                           bloqueo infinito                │
│                                                                          │
│  EVALUACION:                                                             │
│  ├─ Todos los cases se evaluan simultaneamente                         │
│  ├─ Si multiples listos → seleccion aleatoria (uniform)                │
│  ├─ Sin default → bloquea hasta que algun case este listo              │
│  ├─ Con default → nunca bloquea                                        │
│  └─ Expresiones se evaluan ANTES del select                            │
│                                                                          │
│  TIMEOUT:                                                                │
│  ├─ time.After(d): simple, un solo uso (OK fuera de loops)            │
│  ├─ time.NewTimer(d): reutilizable, cancelable (para loops)           │
│  └─ time.Ticker: polling periodico (heartbeat, health check)          │
│                                                                          │
│  CANCELACION:                                                            │
│  ├─ case <-ctx.Done(): siempre incluir en select dentro de loops      │
│  ├─ signal.NotifyContext: shutdown con SIGINT/SIGTERM                  │
│  └─ close(done): broadcast a multiples goroutines                     │
│                                                                          │
│  PATRONES:                                                               │
│  ├─ Priority select: nested select con default                         │
│  ├─ Fan-in: merge N channels con goroutines + select                  │
│  ├─ Debounce: timer.Reset en cada valor nuevo                         │
│  ├─ Rate limit: token bucket con ticker + buffered channel            │
│  ├─ Or-channel: primer channel que cierre gana                        │
│  ├─ Heartbeat: ticker + non-blocking send                             │
│  ├─ Batch flush: timer + counter en select                            │
│  └─ Drain: for range despues de ctx.Done()                            │
│                                                                          │
│  ANTIPATRONES:                                                           │
│  ├─ time.After en loop rapido → memory leak (usar NewTimer+Reset)     │
│  ├─ default en loop → busy-wait 100% CPU (quitar o agregar ticker)    │
│  ├─ Asumir prioridad → no hay (usar priority select manual)           │
│  ├─ Olvidar ctx.Done() → goroutine no responde a cancelacion          │
│  └─ Leer channel cerrado sin comma-ok → zeros infinitos              │
│                                                                          │
│  vs C: select() es para file descriptors, no channels                  │
│  vs Rust: crossbeam select! (macro, sync), tokio select! (async)       │
│  Go: select es keyword built-in, runtime maneja la multiplexacion      │
│                                                                          │
└──────────────────────────────────────────────────────────────────────────┘
```

> **Siguiente**: C08/S02 - Channels, T04 - Patrones con channels — fan-out/fan-in, pipeline, done channel, semaforos
